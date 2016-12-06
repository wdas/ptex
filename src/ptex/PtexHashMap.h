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
  @file PtexHashMap.h
  @brief Contains PtexHashMap, a lightweight multi-threaded hash table.
*/

#ifndef PtexHashMap_h
#define PtexHashMap_h

#include <vector>
#include "PtexPlatform.h"
#include "PtexMutex.h"

PTEX_NAMESPACE_BEGIN

inline uint32_t memHash(const char* val, int len)
{
    int len64 = len & ~7;
    uint64_t val64[4]; val64[0] = 0;
    memcpy(&val64[0], &val[len64], len & 7);
    uint64_t hashval[4] = {0,0,0,0};
    hashval[0] = val64[0]*16777619;

    for (int i = 0; i+32 <= len64; i+=32) {
        for (int j = 0; j < 4; ++j) {
            memcpy(&val64[j], &val[i+j*8], 8);
            hashval[j] = (hashval[j]*16777619) ^ val64[j];
        }
    }
    hashval[0] = (hashval[0]*16777619) ^ hashval[1];
    hashval[2] = (hashval[2]*16777619) ^ hashval[3];
    hashval[0] = (hashval[0]*16777619) ^ hashval[2];
    return uint32_t(hashval[0]);
}

inline bool memCompare(const char* a, const char* b, int len)
{
    int len64 = len & ~7;
    uint64_t val64[2];
    for (int i = 0; i < len64; i+=8) {
        memcpy(&val64[0], &a[i], 8);
        memcpy(&val64[1], &b[i], 8);
        if (val64[0] != val64[1]) return 1;
    }
    return memcmp(&a[len64], &b[len64], len & 7);
}


class StringKey
{
    const char* volatile _val;
    uint32_t volatile _len;
    uint32_t volatile _hash;
    bool volatile _ownsVal;

    void operator=(const StringKey& key); // disallow
    StringKey(const StringKey& key); // disallow

public:
    StringKey() : _val(0), _len(0), _hash(0), _ownsVal(false) {}
    StringKey(const char* val)
    {
        _val = val;
        _len = uint32_t(strlen(val));
        _hash = memHash(_val, _len);
        _ownsVal = false;
    }

    ~StringKey() { if (_ownsVal) delete [] _val; }

    void copy(volatile StringKey& key) volatile
    {
        char* newval = new char[key._len+1];
        memcpy(newval, key._val, key._len+1);
        _val = newval;
        _len = key._len;
        _hash = key._hash;
        _ownsVal = true;
    }

    void move(volatile StringKey& key) volatile
    {
        _val = key._val;
        _len = key._len;
        _hash = key._hash;
        _ownsVal = key._ownsVal;
        key._ownsVal = false;
    }

    bool matches(const StringKey& key) volatile
    {
        return key._hash == _hash && key._len == _len && _val && 0 == memCompare(key._val, _val, _len);
    }

    bool isEmpty() volatile { return _val==0; }

    uint32_t hash() volatile
    {
        return _hash;
    }
};

class IntKey
{
    int _val;

public:
    IntKey() : _val(0) {}
    IntKey(int val) : _val(val) {}
    void copy(volatile IntKey& key) volatile { _val = key._val; }
    void move(volatile IntKey& key) volatile { _val = key._val; }
    bool matches(const IntKey& key) volatile { return _val == key._val; }
    bool isEmpty() volatile { return _val==0; }
    uint32_t hash() volatile { return (_val*7919) & ~0xf;  }
};

template <typename Key, typename Value>
class PtexHashMap
{
    class Entry {
        Entry(const Entry&); // disallow
        void operator=(const Entry&); // disallow
    public:
        Entry() : key(), value(0) {}
        Key volatile key;
        Value volatile value;
    };

    PtexHashMap(const PtexHashMap&); // disallow
    void operator=(const PtexHashMap&); // disallow

    void initContents()
    {
        _numEntries = 16;
        _size = 0;
        _entries = new Entry[_numEntries];
    }

    void deleteContents()
    {
        for (uint32_t i = 0; i < _numEntries; ++i) {
            if (_entries[i].value) delete _entries[i].value;
        }
        delete [] _entries;
        for (size_t i = 0; i < _oldEntries.size(); ++i) {
            delete [] _oldEntries[i];
        }
        std::vector<Entry*>().swap(_oldEntries);
    }

public:
    PtexHashMap()
    {
        initContents();
    }

    ~PtexHashMap()
    {
        deleteContents();
    }

    void clear()
    {
        deleteContents();
        initContents();
    }

    uint32_t size() const { return _size; }

    Value get(Key& key)
    {
        uint32_t mask = _numEntries-1;
        Entry* entries = getEntries();
        uint32_t hash = key.hash();

        Value result = 0;
        for (uint32_t i = hash;; ++i) {
            Entry& e = entries[i & mask];
            if (e.key.matches(key)) {
                result = e.value;
                break;
            }
            if (e.value == 0) {
                break;
            }
        }

        return result;
    }

    Value tryInsert(Key& key, Value value, size_t& newMemUsed)
    {
        Entry* entries = lockEntriesAndGrowIfNeeded(newMemUsed);
        uint32_t mask = _numEntries-1;
        uint32_t hash = key.hash();

        Value result = 0;
        for (uint32_t i = hash;; ++i) {
            Entry& e = entries[i & mask];
            if (e.value == 0) {
                e.value = value;
                ++_size;
                PtexMemoryFence();
                e.key.copy(key);
                result = e.value;
                break;
            }
            while (e.key.isEmpty()) ;
            if (e.key.matches(key)) {
                result = e.value;
                break;
            }
        }
        unlockEntries(entries);
        return result;
    }

    template <typename Fn>
    void foreach(Fn& fn)
    {
        Entry* entries = getEntries();
        for (uint32_t i = 0; i < _numEntries; ++i) {
            Value v = entries[i].value;
            if (v) fn(v);
        }
    }

private:
    Entry* getEntries()
    {
        while (1) {
            Entry* entries = _entries;
            if (entries) return entries;
        }
    }

    Entry* lockEntries()
    {
        while (1) {
            Entry* entries = _entries;
            if (entries && AtomicCompareAndSwap(&_entries, entries, (Entry*)0)) {
                return entries;
            }
        }
    }

    void unlockEntries(Entry* entries)
    {
        AtomicStore(&_entries, entries);
    }

    Entry* lockEntriesAndGrowIfNeeded(size_t& newMemUsed)
    {
        Entry* entries = lockEntries();
        if (_size*2 >= _numEntries) {
            entries = grow(entries, newMemUsed);
        }
        return entries;
    }

    Entry* grow(Entry* oldEntries, size_t& newMemUsed)
    {
        _oldEntries.push_back(oldEntries);
        uint32_t numNewEntries = _numEntries*2;
        Entry* entries = new Entry[numNewEntries];
        newMemUsed = numNewEntries * sizeof(Entry);
        uint32_t mask = numNewEntries-1;
        for (uint32_t oldIndex = 0; oldIndex < _numEntries; ++oldIndex) {
            Entry& oldEntry = oldEntries[oldIndex];
            if (oldEntry.value) {
                for (int newIndex = oldEntry.key.hash();; ++newIndex) {
                    Entry& newEntry = entries[newIndex&mask];
                    if (!newEntry.value) {
                        newEntry.key.move(oldEntry.key);
                        newEntry.value = oldEntry.value;
                        break;
                    }
                }
            }
        }
        _numEntries = numNewEntries;
        return entries;
    }

    Entry* volatile _entries;
    uint32_t volatile _numEntries;
    uint32_t volatile _size;
    std::vector<Entry*> _oldEntries;
};

PTEX_NAMESPACE_END

#endif
