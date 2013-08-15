#ifndef PtexCache_h
#define PtexCache_h

/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

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
#include "Ptexture.h"
#include "PtexDict.h"

#include "threads.h"
#include "uni.h"

#define USE_SPIN // use spinlocks instead of mutex for main cache lock

namespace PtexInternal {

#ifdef USE_SPIN
	typedef SpinLock CacheLock;
	typedef VUtils::ReadWriteSpinLock RWSpinLock;

#else
	typedef Mutex CacheLock;
#endif

typedef AutoLock<CacheLock> AutoLockCache;

#ifndef NDEBUG
#define GATHER_STATS
#endif

#ifdef GATHER_STATS
	struct CacheStats{
		int nfilesOpened;
		int nfilesClosed;
		int ndataAllocated;
		int ndataFreed;
		int nblocksRead;
		long int nbytesRead;
		int nseeks;

		CacheStats()
			: nfilesOpened(0),
			nfilesClosed(0),
			ndataAllocated(0),
			ndataFreed(0),
			nblocksRead(0),
			nbytesRead(0),
			nseeks(0)	{}

		~CacheStats();
		void print();
		static void inc(int& val) {
			static SpinLock spinlock;
			AutoSpin lock(spinlock);
			val++;
		}
		static void add(long int& val, int inc) {
			static SpinLock spinlock;
			AutoSpin lock(spinlock);
			val+=inc;
		}
	};
	extern CacheStats stats;
#define STATS_INC(x) stats.inc(stats.x);
#define STATS_ADD(x, y) stats.add(stats.x, y);
#else
#define STATS_INC(x)
#define STATS_ADD(x, y)
#endif
}
using namespace PtexInternal;

class PtexLruList;

/** One item in a cache, typically an open file or a block of memory */
// The item is only deleted when its usage count becomes zero; if an item is not
// used by any texture, it may still be used by a cache.
class PtexLruItem {
	VUtils::InterlockedCounter useCount; // Total use count (Ptex structures plus cache)

	PtexLruItem(const PtexLruItem&) {}
	PtexLruItem& operator=(const PtexLruItem&) {}
public:
	PtexLruItem(void) {
		useCount.set(1);
		inLruCache.set(0);
	}

	void incUseCount(void) {
		useCount.increment();
	}

	void decUseCount(void) {
		if (0==useCount.decrement()) {
			delete this;
		}
	}

	PtexLruItem *getParentPtr(void) { return _parentPtr; }

	void orphan(void);
	template <typename T> static void orphanList(T& list, PtexLruItem *parent)
	{
		parent->rwLock.lockWrite();
		for (typename T::iterator i=list.begin(); i != list.end(); i++) {
			PtexLruItem* obj = *i;
			void **p=(void**)&*i;
			*p=NULL;
			if (obj) {
				parent->rwLock.unlockWrite();
				obj->orphan();
				parent->rwLock.lockWrite();
			}
		}
		parent->rwLock.unlockWrite();
	}

	RWSpinLock rwLock;
protected:
	PtexLruItem(void** parent, PtexLruItem *parentPtr)
		: _parent(parent), _parentPtr(parentPtr), _prev(0), _next(0), _cacheLock(0) {
			if (parentPtr) _cacheLock=parentPtr->_cacheLock;
			useCount.set(1);
			inLruCache.set(0);
	}
	virtual ~PtexLruItem()
	{
		// detach from parent (if any)
		unparent();

		// we can only get here if the item is not used at all by anyone, including the lru list, so there is nothing else to do.
	}

	virtual CacheLock* getCacheLock(void) { return NULL; }
	virtual PtexLruList *getLruList(void) { return NULL; }

protected:
	CacheLock *_cacheLock;
	VUtils::InterlockedCounter inLruCache; // How many times the item is added to the LRU list
private:
	friend class PtexLruList; // maintains prev/next, deletes
	void** _parent; // pointer to this item within parent
	PtexLruItem* _prev; // prev in lru list (0 if in-use)
	PtexLruItem* _next; // next in lru list (0 if in-use)
	PtexLruItem* _parentPtr; // the parent itself; used to lock it when deleting due to cache overflow


	void unparent(void) {
		PtexLruItem *parentPtr=_parentPtr;
		void **parent=_parent;

		_parentPtr=NULL;
		_parent=NULL;

		if (parentPtr) parentPtr->rwLock.lockWrite();
		if (parent) *parent=NULL;
		if (parentPtr) parentPtr->rwLock.unlockWrite();
	}
};



/** A list of items kept in least-recently-used (LRU) order.
Only items not in use are kept in the list. */
class PtexLruList {
public:
	PtexLruList():_end(NULL, NULL) { _end._prev = _end._next = &_end; }
	~PtexLruList() { while (pop()) continue; }

	// Remove an item from the list; typically called when an item is ref()'d
	int extract(PtexLruItem* node)
	{
		int res=node->inLruCache.decrement();
		if (res>0) {
			return false; // Still in list, leave item for a while
		}
		else if (res<0) {
			node->inLruCache.increment();
			return false; // Item was not in the list
		}

		_lock.lock();

		res=node->inLruCache.get();
		if (res>0) {
			_lock.unlock();
			return false; // Still in cache
		}

		if (!node->_prev) { // Item was not on the list
			node->inLruCache.set(0);
			_lock.unlock();
			return false;
		}

		if (node->_prev) node->_prev->_next = node->_next;
		if (node->_next) node->_next->_prev = node->_prev;
		node->_next=node->_prev=NULL;

		node->_cacheLock=&_lock;

		_lock.unlock();

		node->decUseCount();

		return true;
	}

	// Add an item to the LRU list; typically called when an item is unref()'d
	int push(PtexLruItem* node)
	{
		int cnt=node->inLruCache.get();
		if (cnt!=0)
			return false; // Item is already in LRU list

		// delete node if orphaned
		int res=false;
		_lock.lock();

		node->_cacheLock=&_lock;

		if (!node->_parent) {
			// Do not delete the node - it will be automatically deleted when its usage count gets down to zero
			// delete node;
			node->inLruCache.set(0);
			_lock.unlock();
		}
		else {
			if (!node->_prev) {
				// add to end of list
				node->_next = &_end;
				node->_prev = _end._prev;
				if (_end._prev) _end._prev->_next = node;
				_end._prev = node;
				res=true;
				node->incUseCount(); // To prevent auto deletion
				node->inLruCache.set(10);
			}
			_lock.unlock();
		}

		return res;
	}

	bool pop()
	{
		_lock.lock();
		if (_end._next == &_end) {
			_lock.unlock();
			return 0;
		}

		PtexLruItem *last=_end._next;
		if (last->_prev) last->_prev->_next=last->_next;
		if (last->_next) last->_next->_prev=last->_prev;
		last->_prev=last->_next=NULL;

		last->inLruCache.set(0);

		_lock.unlock();

		last->decUseCount(); // delete last;

		return 1;
	}

	void init(PtexLruItem* node) {
		node->_cacheLock=&_lock;
	}

	CacheLock _lock;
private:
	PtexLruItem _end;
};

inline void PtexLruItem::orphan(void) 
{
	// parent no longer wants me
	// Remove the item from its parent
	if (_parent)
		unparent();

	// Remove the item from the LRU list, if it is there
	if (_prev) {
		PtexLruList *lruList=getLruList();
		if (lruList)
			lruList->extract(this);
	}
}


/** Ptex cache implementation.  Maintains a file and memory cache
within set limits */
class PtexCacheImpl : public PtexCache {
public:
	PtexCacheImpl(int maxFiles, uint64_t maxMem)
		: _pendingDelete(false),
		_maxFiles(maxFiles), _unusedFileCount(0),
		_maxDataSize(maxMem),
		_unusedDataSize(0), _unusedDataCount(0),
		purgeFlag(0)
	{
		/* Allow for a minimum number of data blocks so cache doesn't
		thrash too much if there are any really big items in the
		cache pushing over the limit. It's better to go over the
		limit in this case and make sure there's room for at least
		a modest number of objects in the cache.
		*/

		// try to allow for at least 10 objects per file (up to 100 files)
		_minDataCount = 10 * maxFiles;
		// but no more than 1000
		if (_minDataCount > 1000) _minDataCount = 1000;
	}

	virtual void release() { delete this; }

	Mutex openlock;
	// CacheLock cachelock;

	// internal use - only call from reader classes for deferred deletion
	void setPendingDelete() { _pendingDelete = true; }
	void handlePendingDelete() { if (_pendingDelete) delete this; }

	// internal use - only call from PtexCachedFile, PtexCachedData
	static void addFile() { STATS_INC(nfilesOpened); }
	void setFileInUse(PtexLruItem* file);
	void setFileUnused(PtexLruItem* file);
	void removeFile();
	static void addData() { STATS_INC(ndataAllocated); }
	void setDataInUse(PtexLruItem* data, int size);
	int setDataUnused(PtexLruItem* data, int size);
	void removeData(int size);

	void purgeFiles() {
		AutoLockCache locker(_unusedFiles._lock);
		while (_unusedFileCount > _maxFiles) 
		{
			if (!_unusedFiles.pop()) break;
			// note: pop will destroy item and item destructor will
			// call removeFile which will decrement _unusedFileCount
		}
	}

	int needsPurge(void) {
		return ((_unusedDataSize > _maxDataSize) &&
			(_unusedDataCount > _minDataCount));
	}

	void purgeData() {
		_unusedData._lock.lock();

		while (needsPurge())
		{
			_unusedData._lock.unlock();
			int res=_unusedData.pop();
			_unusedData._lock.lock();
			if (!res) break;
			// note: pop will destroy item and item destructor will
			// call removeData which will decrement _unusedDataSize
			// and _unusedDataCount
		}

		_unusedData._lock.unlock();
	}

	CacheLock& getFilesLock(void) { return _unusedFiles._lock; }
	CacheLock& getDataLock(void) { return _unusedData._lock; }

	PtexLruList& getFilesLruList(void) { return _unusedFiles; }
	PtexLruList& getDataLruList(void) { return _unusedData; }

	RWSpinLock& getPurgeLock(void) { return purgeLock; }
	void setPurgeFlag(void) { purgeFlag=true; }
	int getPurgeFlag(void) { return purgeFlag; }
	void clearPurgeFlag(void) { purgeFlag=false; }

protected:
	~PtexCacheImpl();

//private:
	bool _pendingDelete;	             // flag set if delete is pending

	int _maxFiles, _unusedFileCount;	     // file limit, current unused file count
	uint64_t _maxDataSize, _unusedDataSize;  // data limit (bytes), current size
	int _minDataCount, _unusedDataCount;     // min, current # of unused data blocks
	PtexLruList _unusedFiles, _unusedData;   // lists of unused items

	volatile int purgeFlag;
	RWSpinLock purgeLock;
};


/** Cache entry for open file handle */
class PtexCachedFile : public PtexLruItem
{
public:
	PtexCachedFile(void** parent, PtexCacheImpl* cache, PtexLruItem *parentPtr)
		: PtexLruItem(parent, parentPtr), _cache(cache), _refcount(1)
	{ _cache->addFile(); }
	void ref(void)
	{
		incUseCount();
		_cache->setFileInUse(this);
	}
	void unref() {
		_cache->setFileUnused(this);
		decUseCount();
	}

	CacheLock* getCacheLock(void) { return &_cache->getFilesLock(); }
	PtexLruList* getLruList(void) { return &_cache->getFilesLruList(); }
protected:
	virtual ~PtexCachedFile() {
		_cache->removeFile();
	}
	PtexCacheImpl* _cache;
private:
	int _refcount; // Usage count by Ptex structures
};


/** Cache entry for allocated memory block */
class PtexCachedData : public PtexLruItem
{
public:
	PtexCachedData(void** parent, PtexCacheImpl* cache, int size, PtexLruItem *parentPtr)
		: PtexLruItem(parent, parentPtr), _cache(cache)
	{
		inLruCache.set(0);
		_size.set(size);
		_cache->addData();
	}
	void ref()
	{
		incUseCount();
		_cache->setDataInUse(this, _size.get());
	}

	void unref()
	{
		_cache->setDataUnused(this, _size.get());
		decUseCount();
	}

	CacheLock* getCacheLock(void) { return &_cache->getDataLock(); }
	void setCacheLock(void) { _cacheLock=getCacheLock(); }
	PtexLruList* getLruList(void) { return &_cache->getDataLruList(); }

protected:
	void incSize(int size) { _size += size; }
	virtual ~PtexCachedData() {
		_cache->removeData(_size.get());
	}
	PtexCacheImpl* _cache;
private:
	VUtils::InterlockedCounter _size;
};


#endif
