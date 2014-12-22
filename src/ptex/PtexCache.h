#ifndef PtexCache_h
#define PtexCache_h

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

#include "PtexPlatform.h"
#include <assert.h>

#include "PtexMutex.h"
#include "PtexHashMap.h"
#include "PtexReader.h"

PTEX_NAMESPACE_BEGIN

class PtexReaderCache;

class PtexCachedReader : public PtexReader
{
    PtexReaderCache* _cache;
    volatile int32_t _refCount;
    uint32_t _ioTimestamp;
    uint32_t _dataTimestamp;
    size_t _memUsedAccountedFor;

    bool trylock()
    {
        return AtomicCompareAndSwap(&_refCount, 0, -1);
    }

    void unlock()
    {
        AtomicStore(&_refCount, 0);
    }

public:
    PtexCachedReader(bool premultiply, PtexInputHandler* handler, PtexReaderCache* cache)
        : PtexReader(premultiply, handler), _cache(cache), _refCount(1), _ioTimestamp(0), _dataTimestamp(0), _memUsedAccountedFor(0)
    {
    }

    ~PtexCachedReader() {}

    void ref() {
        while (1) {
            int32_t oldCount = _refCount;
            if (oldCount >= 0 && AtomicCompareAndSwap(&_refCount, oldCount, oldCount+1))
                return;
        }
    }

    int32_t unref() {
        return AtomicDecrement(&_refCount);
    }

    virtual void release();

    bool tryPrune() {
        if (trylock()) {
            prune();
            unlock();
            return true;
        }
        return false;
    }

    bool tryPurge() {
        if (trylock()) {
            purge();
            unlock();
            return true;
        }
        return false;
    }

    uint32_t ioTimestamp() const { return _ioTimestamp; }
    uint32_t dataTimestamp() const { return _dataTimestamp; }
    void setIoTimestamp(uint32_t dataTimestamp) { _dataTimestamp = dataTimestamp; }
    void setDataTimestamp(uint32_t dataTimestamp) { _dataTimestamp = dataTimestamp; }
    size_t memUsedChange() {
        size_t memUsed = _memUsed;
        size_t result = memUsed - _memUsedAccountedFor;
        _memUsedAccountedFor = memUsed;
        return result;
    }
};

/** Cache for reading Ptex texture files */
class PtexReaderCache : public PtexCache
{
public:
    PtexReaderCache(int maxFiles, size_t maxMem, bool premultiply, PtexInputHandler* handler)
	: _maxFiles(maxFiles), _maxMem(maxMem), _io(handler), _premultiply(premultiply),
          _ioTimestamp(0), _dataTimestamp(0), _memUsed(sizeof(*this)), _peakMemUsed(0),
          _peakFilesOpen(0), _fileOpens(0), _blockReads(0)
    {}

    ~PtexReaderCache()
    {}

    virtual void release() { delete this; }

    virtual void setSearchPath(const char* path)
    {
        // record path
        _searchpath = path ? path : "";

        // split into dirs
        _searchdirs.clear();

        if (path) {
            const char* cp = path;
            while (1) {
                const char* delim = strchr(cp, ':');
                if (!delim) {
                    if (*cp) _searchdirs.push_back(cp);
                    break;
                }
                int len = int(delim-cp);
                if (len) _searchdirs.push_back(std::string(cp, len));
                cp = delim+1;
            }
        }
    }

    virtual const char* getSearchPath()
    {
	return _searchpath.c_str();
    }

    virtual PtexTexture* get(const char* path, Ptex::String& error);

    virtual void purge(PtexTexture* /*texture*/);
    virtual void purge(const char* /*filename*/);
    virtual void purgeAll();
    virtual void getStats(Stats& stats);

    void purge(PtexCachedReader* reader);
    void logOpen(PtexCachedReader* reader);
    void logBlockRead(PtexCachedReader* reader);

    void adjustMemUsed(size_t amount) { if (amount) AtomicAdd(&_memUsed, amount); }
    void logRecentlyUsed(PtexCachedReader* reader);

private:
    struct Purger {
        size_t memUsedChange;
        Purger() : memUsedChange(0) {}
        void operator() (PtexCachedReader* reader);
    };
    struct MemUsedSummer {
        size_t memUsedChange;
        MemUsedSummer() : memUsedChange(0) {}
        void operator() (PtexCachedReader* reader);
    };

    bool findFile(const char*& filename, std::string& buffer, Ptex::String& error);
    void pruneFiles();
    void pruneData();
    int _maxFiles;
    size_t _maxMem;
    PtexInputHandler* _io;
    std::string _searchpath;
    std::vector<std::string> _searchdirs;
    typedef PtexHashMap<StringKey,PtexCachedReader*> FileMap;
    FileMap _files;
    bool _premultiply;
    struct ReaderAge {
        PtexCachedReader* reader;
        uint32_t age, timestamp;
        ReaderAge(PtexCachedReader* reader, uint32_t timestamp=0) : reader(reader), age(0), timestamp(timestamp) {}
    };
    static bool compareReaderAge(const ReaderAge& a, const ReaderAge& b) { return a.age < b.age; }
    std::vector<PtexCachedReader*> _newOpenFiles; PAD(_newOpenFiles);
    std::vector<PtexCachedReader*> _tmpOpenFiles; PAD(_tmpOpenFiles);
    std::vector<ReaderAge> _openFiles; PAD(_openFiles);
    std::vector<PtexCachedReader*> _newRecentFiles; PAD(_newRecentFiles);
    std::vector<PtexCachedReader*> _tmpRecentFiles; PAD(_tmpRecentFiles);
    std::vector<ReaderAge> _activeFiles; PAD(_activeFiles);
    SpinLock _logOpenLock; PAD(_logOpenLock);
    SpinLock _logRecentLock; PAD(_logRecentLock);
    SpinLock _pruneFileLock; PAD(_pruneFileLock);
    SpinLock _pruneDataLock; PAD(_pruneDataLock);
    volatile uint32_t _ioTimestamp; PAD(_ioTimestamp);
    uint32_t _dataTimestamp; PAD(_dataTimestamp);
    volatile size_t _memUsed; PAD(_memUsed);
    size_t _peakMemUsed; PAD(_peakMemUsed);
    size_t _peakFilesOpen; PAD(_peakFilesOpen);
    size_t _fileOpens; PAD(_fileOpens);
    size_t _blockReads; PAD(_blockReads);
};

PTEX_NAMESPACE_END

#endif
