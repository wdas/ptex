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
    PtexHashMap(const PtexHashMap&); // disallow
    bool operator=(const PtexHashMap&); // disallow

public:
    PtexHashMap()
        : _table(0), _numEntries(0), _inuse(0)
    {
        _table = new Table(16384);
    }

    ~PtexHashMap()
    {
        delete _table;
    }

    int32_t numEntries() const { return _numEntries; }

    Value* operator[] (const Key& key) const
    {
        if (!_table) {
            helpBuild();
        }

        AtomicIncrement(&_inuse);

        // access table locally (_table ptr may change)
        Table* table;
        if (!table) {
            
        }

        Entry* entries = &table->entries[0];
        uint32_t mask = table->mask, hash = key.hash();

        // open addressing w/ linear probing
        Value* result = 0;
        for (uint32_t i = hash;; ++i) {
            const Entry& e = entries[i & mask];
            if (e.key == key) {
                result = e.value;
                break;
            }
            if (e.value == 0) {
                break;
            }
        }

        AtomicDecrement(&_inuse);
        return result;
    }


    Value* tryInsert (const Key& key, Value* value)
    {
        resize();

        AtomicIncrement(&_inuse);

        // access table locally (_table ptr may change)
        Table* table = _table;
        Entry* entries = &table->entries[0];
        uint32_t mask = table->mask, hash = key.hash();

        // open addressing w/ linear probing
        Value* result = value;
        for (uint32_t i = hash;; ++i) {
            Entry& e = entries[i & mask];
            if (e.key == key) {
                result = e.value;
                break;
            }
            if (e.value == 0) {
                // blank reached, entry not found
                if (AtomicCompareAndSwapPtr(&e.value, (Value*)0, value)) {
                    e.key = key;
                    AtomicIncrement(&_numEntries);
                    break;
                }
                else {
                    // another thread got there first, check again
                    while (!e.key);
                    if (e.key == key) {
                        result = e.value;
                        break;
                    }
                }
            }
        }

        if (result != value) delete value;
        AtomicDecrement(&_inuse);
        return result;
    }

private:
    void resize()
    {
        // check size
        // if too big, grow:
        //   set rebuild state flag
        //   wait for any inserts to finish (while _inuse > 0)
        //   make new table (with ref to prev table)
        //   move entries from old to new (concurrent)
        // if cas into place
        //   clear state
    }

    struct Entry {
        Entry() : key(0), value(0) {}
        Key key;
        Value* volatile value;
    };

    struct Table {
        uint32_t mask;
        std::vector<Entry> entries;

        Table(int size) : mask(size-1), entries(size)
        {
        }
    };

    Table* _table;
    uint32_t _numEntries;
    uint32_t volatile _inuse;
};

}

#endif
