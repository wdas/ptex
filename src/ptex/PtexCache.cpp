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
#ifdef PTEX_PLATFORM_WINDOWS
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
        if (reader->pendingPurge()) {
            // a previous purge attempt was made and file was busy.  Try again now.
            purge(reader);
        }
        reader->ref();
    } else {
        reader = new PtexCachedReader(_premultiply, _io, _err, this);
        isNew = true;
    }

    bool needOpen = reader->needToOpen();
    if (needOpen) {
        std::string buffer;
        const char* pathToOpen = filename;
        // search for the file (unless we have an I/O handler)
        if (_io || findFile(pathToOpen, buffer, error)) {
            reader->open(pathToOpen, error);
        } else {
            // flag reader as invalid so we don't try to open it again on next lookup
            reader->invalidate();
        }
    }

    if (isNew) {
        size_t newMemUsed = 0;
        PtexCachedReader* newreader = reader;
        reader = _files.tryInsert(key, reader, newMemUsed);
        adjustMemUsed(newMemUsed);
        if (reader != newreader) {
            // another thread got here first
            reader->ref();
            delete newreader;
        }
    }

    if (!reader->ok()) {
        reader->unref();
        return 0;
    }

    if (needOpen) {
        reader->logOpen();
    }

    return reader;
}

PtexCache* PtexCache::create(int maxFiles, size_t maxMem, bool premultiply,
                             PtexInputHandler* inputHandler,
                             PtexErrorHandler* errorHandler)
{
    // set default files to 100
    if (maxFiles <= 0) maxFiles = 100;

    return new PtexReaderCache(maxFiles, maxMem, premultiply, inputHandler, errorHandler);
}


void PtexReaderCache::logRecentlyUsed(PtexCachedReader* reader)
{
    while (1) {
        MruList* mruList = _mruList;
        int slot = AtomicIncrement(&mruList->next)-1;
        if (slot < numMruFiles) {
            mruList->files[slot] = reader;
            return;
        }
        // no mru slot available, process mru list and try again
        do processMru();
        while (_mruList->next >= numMruFiles);
    }
}

void PtexReaderCache::processMru()
{
    // use a non-blocking lock so we can proceed as soon as space has been freed in the mru list
    // (which happens almost immediately in the processMru thread that has the lock)
    if (!_mruLock.trylock()) return;
    if (_mruList->next < numMruFiles) {
        _mruLock.unlock();
        return;
    }

    // switch mru buffers and reset slot counter so other threads can proceed immediately
    MruList* mruList = _mruList;
    AtomicStore(&_mruList, _prevMruList);
    _prevMruList = mruList;

    // extract relevant stats and add to open/active list
    size_t memUsedChange = 0, filesOpenChange = 0;
    for (int i = 0; i < numMruFiles; ++i) {
        PtexCachedReader* reader;
        do { reader = mruList->files[i]; } while (!reader); // loop on (unlikely) race condition
        mruList->files[i] = 0;
        memUsedChange += reader->getMemUsedChange();
        size_t opens = reader->getOpensChange();
        size_t blockReads = reader->getBlockReadsChange();
        filesOpenChange += opens;
        if (opens || blockReads) {
            _fileOpens += opens;
            _blockReads += blockReads;
            _openFiles.push(reader);
        }
        if (_maxMem) {
            _activeFiles.push(reader);
        }
    }
    AtomicStore(&mruList->next, 0);
    adjustMemUsed(memUsedChange);
    adjustFilesOpen(filesOpenChange);

    bool shouldPruneFiles = _filesOpen > _maxFiles;
    bool shouldPruneData = _maxMem && _memUsed > _maxMem;

    if (shouldPruneFiles) {
        pruneFiles();
    }
    if (shouldPruneData) {
        pruneData();
    }
    _mruLock.unlock();
}


void PtexReaderCache::pruneFiles()
{
    size_t numToClose = _filesOpen - _maxFiles;
    if (numToClose > 0) {
        while (numToClose) {
            PtexCachedReader* reader = _openFiles.pop();
            if (!reader) { _filesOpen = 0; break; }
            if (reader->tryClose()) {
                --numToClose;
                --_filesOpen;
            }
        }
    }
}


void PtexReaderCache::pruneData()
{
    size_t memUsedChangeTotal = 0;
    size_t memUsed = _memUsed;
    while (memUsed + memUsedChangeTotal > _maxMem) {
        PtexCachedReader* reader = _activeFiles.pop();
        if (!reader) break;
        size_t memUsedChange;
        if (reader->tryPrune(memUsedChange)) {
            // Note: after clearing, memUsedChange is negative
            memUsedChangeTotal += memUsedChange;
        }
    }
    adjustMemUsed(memUsedChangeTotal);
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
    size_t memUsedChange;
    if (reader->tryPurge(memUsedChange)) {
        adjustMemUsed(memUsedChange);
    }
}

void PtexReaderCache::Purger::operator()(PtexCachedReader* reader)
{
    size_t memUsedChange;
    if (reader->tryPurge(memUsedChange)) {
        memUsedChangeTotal += memUsedChange;
    }
}

void PtexReaderCache::purgeAll()
{
    Purger purger;
    _files.foreach(purger);
    adjustMemUsed(purger.memUsedChangeTotal);
}

void PtexReaderCache::getStats(Stats& stats)
{
    stats.memUsed = _memUsed;
    stats.peakMemUsed = _peakMemUsed;
    stats.filesOpen = _filesOpen;
    stats.peakFilesOpen = _peakFilesOpen;
    stats.filesAccessed = _files.size();
    stats.fileReopens = _fileOpens < stats.filesAccessed ? 0 : _fileOpens - stats.filesAccessed;
    stats.blockReads = _blockReads;
}

PTEX_NAMESPACE_END
