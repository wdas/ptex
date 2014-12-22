/*
PTEX SOFTWARE
Copyright 2014 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

/**
   @file PtexCache.cpp
   @brief Ptex Cache Implementation

   <b> Ownership.</b>
   The cache owns all files and data.  If a texture is in use, the
   cache will not touch it.  When it is no longer in use, it may be
   kept or deleted to keep resource usage under the set limits.
   Deletions are done in lru order.

   <b> Resource Tracking.</b>
   All textures are created as part of the cache and have a ptr back to
   the cache.  Each texture updates the cache's resource total whenever
   it is released to the cache.

   <b> Reference Counting.</b>
   Every texture has a ref count to track whether it is being used.
   Objects belonging to the texture (such as texture tiles or meta data)
   are not ref-counted and are kept at least as long as the
   texture is in use.

   <b> Cache Lifetime.</b>
   When a cache is released from its owner, it will delete itself
   and all contained textures immediately.

   <b> Threading.</b> The cache is fully thread-safe and completely
   lock/wait/atomic-free for accessing data that is present in the
   cache.  Acquiring a texture from PtexCache::get requires an atomic
   increment of the refcount.  Releasing the texture requires an
   atomic decrement and potentially a brief spinlock to add the
   texture to a list of recent textures.
 */

#include "PtexPlatform.h"
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <iostream>
#include <ctype.h>
#include "Ptexture.h"
#include "PtexReader.h"
#include "PtexCache.h"


PTEX_NAMESPACE_BEGIN

void PtexCachedReader::release()
{
    if (0 == unref()) {
        _cache->logRecentlyUsed(this);
    }
}


bool PtexReaderCache::findFile(const char*& filename, std::string& buffer, Ptex::String& error)
{
    bool isAbsolute = (filename[0] == '/'
#ifdef WINDOWS
                       || filename[0] == '\\'
                       || (isalpha(filename[0]) && filename[1] == ':')
#endif
    );
    if (isAbsolute || _searchdirs.empty()) return true; // no need to search

    // file is relative, search in searchpath
    buffer.reserve(256); // minimize reallocs (will grow automatically)
    struct stat statbuf;
    for (size_t i = 0, size = _searchdirs.size(); i < size; i++) {
        buffer = _searchdirs[i];
        buffer += "/";
        buffer += filename;
        if (stat(buffer.c_str(), &statbuf) == 0) {
            filename = buffer.c_str();
            return true;
        }
    }
    // not found
    std::string errstr = "Can't find ptex file: ";
    errstr += filename;
    error = errstr.c_str();
    return false;
}


PtexTexture* PtexReaderCache::get(const char* filename, Ptex::String& error)
{
    // lookup reader in map
    StringKey key(filename);
    PtexCachedReader* reader = _files.get(key);
    bool isNew = false;

    if (reader) {
        if (!reader->ok()) return 0;
        reader->ref();
    } else {
        reader = new PtexCachedReader(_premultiply, _io, this);
        isNew = true;
    }

    bool ok = true;
    bool needOpen = reader->needToOpen();
    if (needOpen) {
	std::string buffer;
	const char* pathToOpen = filename;
	if (!_io) ok = findFile(pathToOpen, buffer, error);
        if (ok) ok = reader->open(pathToOpen, error);
    }

    if (ok && isNew) {
        size_t newMemUsed = 0;
        PtexCachedReader* newreader = reader;
        reader = _files.tryInsert(key, reader, newMemUsed);
        adjustMemUsed(newMemUsed);
        if (reader != newreader) {
            // another thread got here first
            reader->ref();
            delete newreader;
            isNew = false;
        }
    }

    if (!ok) {
        reader->unref();
        return 0;
    }

    if (needOpen) {
        logOpen(reader);
    }

    return reader;
}

PtexCache* PtexCache::create(int maxFiles, size_t maxMem, bool premultiply,
			     PtexInputHandler* handler)
{
    // set default files to 100
    if (maxFiles <= 0) maxFiles = 100;

    return new PtexReaderCache(maxFiles, maxMem, premultiply, handler);
}


void PtexReaderCache::logOpen(PtexCachedReader* reader)
{
    bool shouldPrune;
    {
        AutoSpin locker(_logOpenLock);
        _newOpenFiles.push_back(reader);
        shouldPrune = _newOpenFiles.size() >= 16;
    }
    if (shouldPrune) {
        pruneFiles();
    }
}

void PtexReaderCache::logBlockRead(PtexCachedReader* reader)
{
    reader->setIoTimestamp(AtomicIncrement(&_ioTimestamp));
    AtomicIncrement(&_blockReads);
}



void PtexReaderCache::pruneFiles()
{
    if (!_pruneFileLock.trylock()) return;

    {
        AutoSpin locker(_logOpenLock);
        std::swap(_tmpOpenFiles, _newOpenFiles);
    }

    for (std::vector<PtexCachedReader*>::iterator iter = _tmpOpenFiles.begin(); iter != _tmpOpenFiles.end(); ++iter) {
        _openFiles.push_back(ReaderAge(*iter));
    }
    _fileOpens += _tmpOpenFiles.size();
    _tmpOpenFiles.clear();

    uint32_t now = _ioTimestamp;
    for (std::vector<ReaderAge>::iterator iter = _openFiles.begin(); iter != _openFiles.end(); ++iter) {
        iter->age = now - iter->reader->ioTimestamp();
    }

    _peakFilesOpen = std::max(_peakFilesOpen, _openFiles.size());
    int numToClose = int(_openFiles.size()) - _maxFiles;
    if (numToClose > 0) {
        std::nth_element(_openFiles.begin(), _openFiles.end() - numToClose, _openFiles.end(), compareReaderAge);
        std::vector<ReaderAge> keep;

        while (numToClose && !_openFiles.empty()) {
            ReaderAge file = _openFiles.back();
            _openFiles.pop_back();
            if (file.reader->tryClose()) {
                numToClose--;
            } else {
                keep.push_back(file);
            }
        }
    }

    _pruneFileLock.unlock();
}


void PtexReaderCache::logRecentlyUsed(PtexCachedReader* reader)
{
    if (!_maxMem) return;
    bool shouldPrune;
    {
        AutoSpin locker(_logRecentLock);
        _newRecentFiles.push_back(reader);
        shouldPrune = _newRecentFiles.size() >= 16;
    }
    if (shouldPrune) {
        pruneData();
    }
}


void PtexReaderCache::pruneData()
{
    if (!_pruneDataLock.trylock()) return;

    {
        AutoSpin locker(_logRecentLock);
        std::swap(_tmpRecentFiles, _newRecentFiles);
    }

    // migrate recent additions to active file list, updating time stamp and accounting for new memory use
    size_t memUsedChange = 0;
    for (std::vector<PtexCachedReader*>::iterator iter = _tmpRecentFiles.begin(); iter != _tmpRecentFiles.end(); ++iter) {
        PtexCachedReader* reader = *iter;
        uint32_t timestamp = ++_dataTimestamp;
        reader->setDataTimestamp(timestamp);
        _activeFiles.push_back(ReaderAge(reader, timestamp));
        memUsedChange += reader->memUsedChange();
    }
    adjustMemUsed(memUsedChange);
    _tmpRecentFiles.clear();

    // compute age of active entries (skip stale entries - old readers that have been accessed again more recently)
    uint32_t now = _dataTimestamp;
    std::vector<ReaderAge>::iterator keep = _activeFiles.begin();
    for (std::vector<ReaderAge>::iterator iter = _activeFiles.begin(); iter != _activeFiles.end(); ++iter) {
        if (iter->reader->dataTimestamp() == iter->timestamp) {
            *keep = *iter;
            keep->age = now - iter->timestamp;
            ++keep;
        }
    }

    // remove skipped entries
    _activeFiles.erase(keep, _activeFiles.end());


    // pop and prune least recent files
    std::sort(_activeFiles.begin(), _activeFiles.end(), compareReaderAge); // TODO: (maybe) use nth_element on avg reader size?
    memUsedChange = 0;
    size_t memUsed = _memUsed;
    _peakMemUsed = std::max(memUsed, _peakMemUsed);
    while (memUsed + memUsedChange > _maxMem && !_activeFiles.empty()) {
        PtexCachedReader* reader = _activeFiles.back().reader;
        _activeFiles.pop_back();
        if (reader->tryPrune()) {
            // Note: after clearing, memUsedChange is negative
            memUsedChange += reader->memUsedChange();
        }
    }
    adjustMemUsed(memUsedChange);

    _pruneDataLock.unlock();
}


void PtexReaderCache::purge(PtexTexture* texture)
{
    PtexCachedReader* reader = static_cast<PtexCachedReader*>(texture);
    reader->unref();
    purge(reader);
    reader->ref();
}


void PtexReaderCache::purge(const char* filename)
{
    StringKey key(filename);
    PtexCachedReader* reader = _files.get(key);
    if (reader) purge(reader);
}

void PtexReaderCache::purge(PtexCachedReader* reader)
{
    if (reader->tryPurge()) {
        adjustMemUsed(reader->memUsedChange());
    }
}

void PtexReaderCache::Purger::operator()(PtexCachedReader* reader)
{
    if (reader->tryPurge()) {
        memUsedChange += reader->memUsedChange();
    }
}

void PtexReaderCache::purgeAll()
{
    Purger purger;
    _files.foreach(purger);
    adjustMemUsed(purger.memUsedChange);
}

void PtexReaderCache::MemUsedSummer::operator()(PtexCachedReader* reader)
{
    memUsedChange += reader->memUsedChange();
}

void PtexReaderCache::getStats(Stats& stats)
{
    MemUsedSummer summer;
    _files.foreach(summer);
    adjustMemUsed(summer.memUsedChange);

    stats.memUsed = _memUsed;
    stats.peakMemUsed = std::max(stats.memUsed, _peakMemUsed);
    { AutoSpin locker(_logOpenLock); stats.filesOpen = _openFiles.size(); }
    stats.peakFilesOpen = _peakFilesOpen;
    stats.filesAccessed = _files.size();
    stats.fileReopens = _fileOpens < stats.filesAccessed ? 0 : _fileOpens - stats.filesAccessed;
    stats.blockReads = _blockReads;
}

PTEX_NAMESPACE_END
