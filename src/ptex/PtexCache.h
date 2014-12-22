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

class PtexLruItem
{
    PtexLruItem* _prev;
    PtexLruItem* _next;

public:
    PtexLruItem() : _prev(this), _next(this) {}

    void extract() {
        _next->_prev = _prev;
        _prev->_next = _next;
        _next = _prev = this;
    }
    void push(PtexLruItem* item) {
        item->extract();
        _prev->_next = item;
        item->_next = this;
        item->_prev = _prev;
        _prev = item;
    }
    PtexLruItem* pop() {
        if (_next == this) return 0;
        PtexLruItem* item = _next;
        _next->extract();
        return item;
    }
};

template<class T, PtexLruItem T::*item>
class PtexLruList
{
    PtexLruItem _end;

public:
    void push(T* node)
    {
        _end.push(&(node->*item));
    }

    T* pop()
    {
        PtexLruItem* it = _end.pop();
        return it ? (T*) ((char*)it - (char*)&(((T*)0)->*item)) : 0;
    }
};

class PtexReaderCache;

class PtexCachedReader : public PtexReader
{
    PtexReaderCache* _cache;
    volatile int32_t _refCount;
    size_t _memUsedAccountedFor;
    size_t _opensAccountedFor;
    size_t _blockReadsAccountedFor;
    PtexLruItem _openFilesItem;
    PtexLruItem _activeFilesItem;
    friend class PtexReaderCache;

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
        : PtexReader(premultiply, handler), _cache(cache), _refCount(1),
          _memUsedAccountedFor(0), _opensAccountedFor(0), _blockReadsAccountedFor(0)
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

    size_t getMemUsedChange() {
        size_t memUsedTmp = _memUsed;
        size_t result = memUsedTmp - _memUsedAccountedFor;
        _memUsedAccountedFor = memUsedTmp;
        return result;
    }

    size_t getOpensChange() {
        size_t opensTmp = _opens;
        size_t result = opensTmp - _opensAccountedFor;
        _opensAccountedFor = opensTmp;
        return result;
    }

    size_t getBlockReadsChange() {
        size_t blockReadsTmp = _blockReads;
        size_t result = blockReadsTmp - _blockReadsAccountedFor;
        _blockReadsAccountedFor = blockReadsTmp;
        return result;
    }
};


/** Cache for reading Ptex texture files */
class PtexReaderCache : public PtexCache
{
public:
    PtexReaderCache(int maxFiles, size_t maxMem, bool premultiply, PtexInputHandler* handler)
	: _maxFiles(maxFiles), _maxMem(maxMem), _io(handler), _premultiply(premultiply),
          _memUsed(sizeof(*this)), _mruList(&_mruLists[0]), _prevMruList(&_mruLists[1]),
          _peakMemUsed(0), _peakFilesOpen(0), _fileOpens(0), _blockReads(0)
    {
        memset((void*)&_mruLists[0], 0, sizeof(_mruLists));
    }

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

    void adjustMemUsed(size_t amount) {
        if (amount) {
            size_t memUsed = AtomicAdd(&_memUsed, amount);
            _peakMemUsed = std::max(_peakMemUsed, memUsed);
        }
    }
    void adjustFilesOpen(size_t amount) {
        if (amount) {
            size_t filesOpen = AtomicAdd(&_filesOpen, amount);
            _peakFilesOpen = std::max(_peakFilesOpen, filesOpen);
        }
    }
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
    void processMru();
    void pruneFiles();
    void pruneData();
    size_t _maxFiles;
    size_t _maxMem;
    PtexInputHandler* _io;
    std::string _searchpath;
    std::vector<std::string> _searchdirs;
    typedef PtexHashMap<StringKey,PtexCachedReader*> FileMap;
    FileMap _files;
    bool _premultiply;
    volatile size_t _memUsed; PAD(_memUsed);
    volatile size_t _filesOpen; PAD(_filesOpen);
    Mutex _mruLock; PAD(_mruLock);

    static const int numMruFiles = 50;
    struct MruList {
        volatile int next;
        PtexCachedReader* volatile files[numMruFiles];
    };
    MruList _mruLists[2];
    MruList* volatile _mruList;
    MruList* volatile _prevMruList;

    PtexLruList<PtexCachedReader, &PtexCachedReader::_openFilesItem> _openFiles;
    PtexLruList<PtexCachedReader, &PtexCachedReader::_activeFilesItem> _activeFiles;

    size_t _peakMemUsed;
    size_t _peakFilesOpen;
    size_t _fileOpens;
    size_t _blockReads;
};

PTEX_NAMESPACE_END

#endif
