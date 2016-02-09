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
#include <cstddef>

#include "PtexMutex.h"
#include "PtexHashMap.h"
#include "PtexReader.h"

PTEX_NAMESPACE_BEGIN

// Intrusive LRU list item (to be used only by PtexLruList)
class PtexLruItem
{
    PtexLruItem* _prev;
    PtexLruItem* _next;

    void extract() {
        _next->_prev = _prev;
        _prev->_next = _next;
        _next = _prev = this;
    }

public:
    PtexLruItem() : _prev(this), _next(this) {}

    // add item to end of list (pointed to by _prev)
    void push(PtexLruItem* item) {
        item->extract();
        _prev->_next = item;
        item->_next = this;
        item->_prev = _prev;
        _prev = item;
    }

    // remove item from front of list (pointed to by _next)
    PtexLruItem* pop() {
        if (_next == this) return 0;
        PtexLruItem* item = _next;
        _next->extract();
        return item;
    }
};

// Intrusive LRU list (with LRU item stored as member of T)
template<class T, PtexLruItem T::*item>
class PtexLruList
{
    PtexLruItem _end;

public:
    void push(T* node)
    {
        // the item is added to the intrusive pointer specified by the template
        // templatization allows more than one intrusive list to be in the object
        _end.push(&(node->*item));
    }

    T* pop()
    {
        PtexLruItem* it = _end.pop();
        // "it" points to the intrusive item, a member within T
        // subtract off the pointer-to-member offset to get a pointer to the containing T object
        static const T* dummy = 0;
        static const std::ptrdiff_t itemOffset = (const char*)&(dummy->*item) - (const char*)dummy;
        return it ? (T*) ((char*)it - itemOffset) : 0;
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
    PtexCachedReader(bool premultiply, PtexInputHandler* inputHandler, PtexErrorHandler* errorHandler, PtexReaderCache* cache)
        : PtexReader(premultiply, inputHandler, errorHandler), _cache(cache), _refCount(1),
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

    bool tryPrune(size_t& memUsedChange) {
        if (trylock()) {
            prune();
            memUsedChange = getMemUsedChange();
            unlock();
            return true;
        }
        return false;
    }

    bool tryPurge(size_t& memUsedChange) {
        if (trylock()) {
            purge();
            memUsedChange = getMemUsedChange();
            unlock();
            return true;
        }
        setPendingPurge();
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
    PtexReaderCache(int maxFiles, size_t maxMem, bool premultiply, PtexInputHandler* inputHandler, PtexErrorHandler* errorHandler)
        : _maxFiles(maxFiles), _maxMem(maxMem), _io(inputHandler), _err(errorHandler), _premultiply(premultiply),
          _memUsed(sizeof(*this)), _filesOpen(0), _mruList(&_mruLists[0]), _prevMruList(&_mruLists[1]),
          _peakMemUsed(0), _peakFilesOpen(0), _fileOpens(0), _blockReads(0)
    {
        memset((void*)&_mruLists[0], 0, sizeof(_mruLists));
        CACHE_LINE_PAD_INIT(_memUsed); // keep cppcheck happy
        CACHE_LINE_PAD_INIT(_filesOpen);
        CACHE_LINE_PAD_INIT(_mruLock);
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
        size_t memUsedChangeTotal;
        Purger() : memUsedChangeTotal(0) {}
        void operator() (PtexCachedReader* reader);
    };

    bool findFile(const char*& filename, std::string& buffer, Ptex::String& error);
    void processMru();
    void pruneFiles();
    void pruneData();
    size_t _maxFiles;
    size_t _maxMem;
    PtexInputHandler* _io;
    PtexErrorHandler* _err;
    std::string _searchpath;
    std::vector<std::string> _searchdirs;
    typedef PtexHashMap<StringKey,PtexCachedReader*> FileMap;
    FileMap _files;
    bool _premultiply;
    volatile size_t _memUsed; CACHE_LINE_PAD(_memUsed,size_t);
    volatile size_t _filesOpen; CACHE_LINE_PAD(_filesOpen,size_t);
    Mutex _mruLock; CACHE_LINE_PAD(_mruLock,Mutex);

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
