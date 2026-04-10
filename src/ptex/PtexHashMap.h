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
#include <string>
#include "PtexPlatform.h"
#include "PtexMutex.h"

PTEX_NAMESPACE_BEGIN

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
    uint16_t volatile _len;
    uint16_t volatile _ownsVal;
    uint32_t volatile _hash;

    void operator=(const StringKey& key); // disallow
    StringKey(const StringKey& key); // disallow

public:
    StringKey() : _val(0), _len(0), _ownsVal(false), _hash(0) {}
    StringKey(const char* val)
    {
        _val = val;
        _len = uint16_t(strlen(val));
        _hash = uint32_t(std::hash<std::string_view>{}(std::string_view(_val, _len)));
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

    bool matches(const StringKey& key) const volatile
    {
        return key._hash == _hash && key._len == _len && _val && 0 == memCompare(key._val, _val, _len);
    }

    bool isEmpty() const volatile { return _val==0; }

    uint32_t hash() const volatile
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
    bool matches(const IntKey& key) const volatile { return _val == key._val; }
    bool isEmpty() volatile const { return _val==0; }
    uint32_t hash() volatile const { return (_val*7919) & ~0xf;  }
};

template <typename Key, typename Value>
class PtexHashMap
{
    // a table is a TableHeader followed in memory by an array of Entry records

    struct TableHeader {
        uint32_t numEntries;
        uint32_t size;
    };

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
        size_t memUsed;
        _table = allocTable(16, memUsed);
    }

    void deleteContents()
    {
        TableHeader* header;
        Entry* entries;
        getTable(_table, header, entries);

        for (uint32_t i = 0; i < header->numEntries; ++i) {
            entries[i].key.~Key();
            if (entries[i].value) delete entries[i].value;
        }
        free(_table);
        for (size_t i = 0; i < _oldTables.size(); ++i) {
            free(_oldTables[i]);
        }
        std::vector<void*>().swap(_oldTables);
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

    uint32_t size() const {
        return ((TableHeader*)_table)->size;
    }

    Value get(Key& key) const
    {
        const TableHeader* header;
        const Entry* entries;
        getTable(_table, header, entries);
        uint32_t mask = header->numEntries-1;
        uint32_t hash = key.hash();

        Value result = 0;
        for (uint32_t i = hash;; ++i) {
            const Entry& e = entries[i & mask];
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
        void* table = lockTableAndGrowIfNeeded(newMemUsed);
        TableHeader* header;
        Entry* entries;
        getTable(table, header, entries);
        uint32_t mask = header->numEntries-1;
        uint32_t hash = key.hash();

        Value result = 0;
        for (uint32_t i = hash;; ++i) {
            Entry& e = entries[i & mask];
            if (e.value == 0) {
                e.value = value;
                ++header->size;
                PtexMemoryFence(); // must write key after value
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
        unlockTable(table);
        return result;
    }

    template <typename Fn>
    void foreach(Fn& fn) const
    {
        const TableHeader* header;
        const Entry* entries;
        getTable(_table, header, entries);
        for (uint32_t i = 0; i < header->numEntries; ++i) {
            Value v = entries[i].value;
            if (v) fn(v);
        }
    }

private:
    void* allocTable(int32_t numEntries, size_t& memsize)
    {
        memsize = sizeof(TableHeader) + sizeof(Entry) * numEntries;
        void* table = malloc(memsize);
        memset(table, 0, memsize);
        TableHeader* header = new (table) TableHeader;
        header->numEntries = numEntries;
        header->size = 0;
        Entry* entries = (Entry*)((char*)table + sizeof(TableHeader));
        for (int32_t i = 0; i < numEntries; ++i)
        {
            new (&entries[i]) Entry();
        }
        return table;
    }

    static void getTable(const void* table, const TableHeader*& header, const Entry*& entries)
    {
        header = (const TableHeader*) table;
        entries = (const Entry*)((const char*)table + sizeof(TableHeader));
    }

    static void getTable(void* table, TableHeader*& header, Entry*& entries)
    {
        header = (TableHeader*) table;
        entries = (Entry*)((char*)table + sizeof(TableHeader));
    }

    void unlockTable(void* table)
    {
        _table = table;
        _lock.unlock();
    }

    void* lockTableAndGrowIfNeeded(size_t& newMemUsed)
    {
        _lock.lock();
        void* table = _table;
        TableHeader* header;
        Entry* entries;
        getTable(table, header, entries);

        if (header->size*2 >= header->numEntries) {
            table = grow(table, newMemUsed);
        }
        return table;
    }

    void* grow(void* oldTable, size_t& newMemUsed)
    {
        TableHeader* oldHeader;
        Entry* oldEntries;
        getTable(oldTable, oldHeader, oldEntries);

        _oldTables.push_back(oldTable);
        void* newTable = allocTable(oldHeader->numEntries*2, newMemUsed);
        TableHeader* newHeader;
        Entry* newEntries;
        getTable(newTable, newHeader, newEntries);
        uint32_t mask = newHeader->numEntries-1;
        for (uint32_t oldIndex = 0; oldIndex < oldHeader->numEntries; ++oldIndex) {
            Entry& oldEntry = oldEntries[oldIndex];
            if (oldEntry.value) {
                for (int newIndex = oldEntry.key.hash();; ++newIndex) {
                    Entry& newEntry = newEntries[newIndex&mask];
                    if (!newEntry.value) {
                        newEntry.key.move(oldEntry.key);
                        newEntry.value = oldEntry.value;
                        break;
                    }
                }
            }
        }
        newHeader->size = oldHeader->size;
        return newTable;
    }

    void* _table;
    Mutex _lock;
    std::vector<void*> _oldTables;
};

PTEX_NAMESPACE_END

#endif
