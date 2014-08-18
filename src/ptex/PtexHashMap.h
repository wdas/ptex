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

#include <inttypes.h>
#include <vector>
#include "PtexPlatform.h"

namespace PtexInternal {

template <typename Key, typename Value>
class PtexHashMap
{
    class Entry {
        Entry(const Entry&); // disallow
        void operator=(const Entry&); // disallow
    public:
        Entry() : key(), value(0) {}
        Key volatile key;
        Value* volatile value;
    };

    PtexHashMap(const PtexHashMap&); // disallow
    void operator=(const PtexHashMap&); // disallow

public:
    PtexHashMap()
        : _entries(0), _numEntries(16), _size(0), _inuse(0)
    {
        _entries = new Entry[_numEntries];
    }

    ~PtexHashMap()
    {
        delete [] _entries;
    }

    int32_t size() const { return _size; }

    Value* get(Key& key)
    {
        Entry* entries = getEntries();
        uint32_t mask = _numEntries-1;
        uint32_t hash = key.hash();

        Value* result = 0;
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

        releaseEntries(entries);
        return result;
    }

    Value* tryInsert(Key& key, Value* value)
    {
        Entry* entries = getEntriesAndGrowIfNeeded();
        uint32_t mask = _numEntries-1;
        uint32_t hash = key.hash();

        // open addressing w/ linear probing
        Value* result = 0;
        for (uint32_t i = hash;; ++i) {
            Entry& e = entries[i & mask];
            if (e.key.matches(key)) {
                // entry already exists
                result = e.value;
                delete value;
                break;
            }
            if (e.value == 0) {
                // blank reached, try to set entry
                if (AtomicCompareAndSwapPtr(&e.value, (Value*)0, value)) {
                    AtomicIncrement(&_size);
                    e.key.copy(key);
                    result = e.value;
                    break;
                }
                else {
                    // another thread got there first, recheck entry
                    while (e.key.isEmpty()) ;
                    --i;
                }
            }
        }
        releaseEntries(entries);
        return result;
    }

private:
    Entry* getEntries()
    {
        while (1) {
            while (AtomicLoad(&_entries) == 0) ;  // check *before* ref counting so we don't livelock
            AtomicIncrement(&_inuse);
            Entry* entries = _entries;
            if (entries) return entries;
            AtomicDecrement(&_inuse);
        }
    }

    Entry* getEntriesAndGrowIfNeeded()
    {
        while (_size*2 >= _numEntries) {
            Entry* entries = _entries;
            if (entries && AtomicCompareAndSwapPtr(&_entries, entries, (Entry*)0)) {
                while (AtomicLoad(&_inuse)) ;
                AtomicIncrement(&_inuse);
                if (_size*2 >= _numEntries) {
                    return grow(entries);
                }
                AtomicStore(&_entries, entries);
                return entries;
            }
        }
        return getEntries();
    }

    Entry* grow(Entry* oldEntries)
    {
        uint32_t numNewEntries = _numEntries*2;
        Entry* entries = new Entry[numNewEntries];
        uint32_t mask = numNewEntries-1;
        for (uint32_t oldIndex = 0; oldIndex < _numEntries; ++oldIndex) {
            Entry& oldEntry = oldEntries[oldIndex];
            for (int newIndex = oldEntry.key.hash();; ++newIndex) {
                Entry& newEntry = entries[newIndex&mask];
                if (!newEntry.value) {
                    newEntry.key.copy(oldEntry.key);
                    newEntry.value = oldEntry.value;
                    break;
                }
            }
        }
        std::cout << numNewEntries << '\n';
        AtomicStore(&_numEntries, numNewEntries);
        AtomicStore(&_entries, entries);
        return entries;
    }

    void releaseEntries(Entry* /*entries*/)
    {
        AtomicDecrement(&_inuse);
    }

    Entry* volatile _entries;
    uint32_t volatile _numEntries;
    uint32_t volatile _size;
    uint32_t volatile _inuse;
};

}

#endif
