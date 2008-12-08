#ifndef PtexCache_h
#define PtexCache_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include <assert.h>
#include <pthread.h>
#include <assert.h>
#include "Ptexture.h"

namespace PtexInternal {
#include "DGDict.h"

#ifndef NDEBUG
    // debug version of mutex
    class Mutex {
	void check(int errcode) { assert(errcode == 0); }
    public:
	Mutex() {
	    pthread_mutexattr_t attr;
	    pthread_mutexattr_init(&attr);
	    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	    pthread_mutex_init(&_mutex, &attr);
	    pthread_mutexattr_destroy(&attr);
	}
	~Mutex()             { check(pthread_mutex_destroy(&_mutex)); }
	void lock()          { check(pthread_mutex_lock(&_mutex)); _locked = 1; }
	void unlock()        { assert(_locked); _locked = 0; check(pthread_mutex_unlock(&_mutex)); }
	bool locked()        { return _locked; }
    private:
	pthread_mutex_t _mutex;
	int _locked;
    };
#else
    class Mutex {
    public:
	Mutex()       { pthread_mutex_init(&_mutex, 0); }
	~Mutex()      { pthread_mutex_destroy(&_mutex); }
	void lock()   { pthread_mutex_lock(&_mutex); }
	void unlock() { pthread_mutex_unlock(&_mutex); }
    private:
	pthread_mutex_t _mutex;
    };
#endif

    class SpinLock {
    public:
	SpinLock()    { pthread_spin_init(&_spinlock, PTHREAD_PROCESS_PRIVATE); }
	~SpinLock()   { pthread_spin_destroy(&_spinlock); }
	void lock()   { pthread_spin_lock(&_spinlock); }
	void unlock() { pthread_spin_unlock(&_spinlock); }
    private:
	pthread_spinlock_t _spinlock;
    };

#ifndef NDEBUG
    class DebugSpinLock : public SpinLock {
     public:
	DebugSpinLock() : _locked(0) {}
	void lock()   { SpinLock::lock(); _locked = 1; }
	void unlock() { assert(_locked); _locked = 0; SpinLock::unlock(); }
	bool locked() { return _locked; }
     private:
	int _locked;
    };
#endif

    template <class T>
    class AutoLock {
    public:
	AutoLock(T& m) : _m(m) { _m.lock(); }
	~AutoLock()            { _m.unlock(); }
    private:
	T& _m;
    };

    typedef AutoLock<Mutex> AutoMutex;
    typedef AutoLock<SpinLock> AutoSpin;

#define USE_SPIN
#ifdef USE_SPIN
#  ifndef NDEBUG
    typedef DebugSpinLock CacheLock;
#  else
    typedef SpinLock CacheLock;
#  endif
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
	    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
	    pthread_mutex_lock(&m);
	    val++;
	    pthread_mutex_unlock(&m);
	}
	static void add(long int& val, int inc) {
	    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
	    pthread_mutex_lock(&m);
	    val+=inc;
	    pthread_mutex_unlock(&m);
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

class PtexLruItem {
public:
    bool inuse() { return _prev == 0; }
    void orphan() 
    {
	// parent no longer wants me
	void** p = _parent;
	_parent = 0;
	assert(p && *p == this);
	if (!inuse()) delete this;
	*p = 0;
    }
    template <typename T> static void orphanList(T& list)
    {
	for (typename T::iterator i=list.begin(); i != list.end(); i++) {
	    PtexLruItem* obj = *i;
	    if (obj) {
		assert(obj->_parent == (void**)&*i);
		obj->orphan();
	    }
	}
    }

protected:
    PtexLruItem(void** parent=0)
	: _parent(parent), _prev(0), _next(0) {}
    virtual ~PtexLruItem()
    {
	// detach from parent (if any)
	if (_parent) { assert(*_parent == this); *_parent = 0; }
	// unlink from lru list (if in list)
	if (_prev) {
	    _prev->_next = _next; 
	    _next->_prev = _prev;
	}
    }

private:
    friend class PtexLruList;	// maintains prev/next, deletes
    void** _parent;		// pointer to this item within parent
    PtexLruItem* _prev;		// prev in lru list (0 if in-use)
    PtexLruItem* _next;		// next in lru list (0 if in-use)
};



class PtexLruList {
public:
    PtexLruList() { _end._prev = _end._next = &_end; }
    ~PtexLruList() { while (pop()); }

    void extract(PtexLruItem* node)
    {
	// remove from list
	node->_prev->_next = node->_next;
	node->_next->_prev = node->_prev;
	node->_next = node->_prev = 0;
    }

    void push(PtexLruItem* node)
    {
	// delete node if orphaned
	if (!node->_parent) delete node;
	else {
	    // add to end of list
	    node->_next = &_end;
	    node->_prev = _end._prev;
	    _end._prev->_next = node;
	    _end._prev = node;
	}
    }

    bool pop()
    {
	if (_end._next == &_end) return 0;
	delete _end._next; // item will unlink itself
	return 1;
    }

private:
    PtexLruItem _end;
};


class PtexCacheImpl : public PtexCache {
public:
    PtexCacheImpl(int maxFiles, int maxMem)
	: _pendingDelete(false),
	  _maxFiles(maxFiles), _unusedFileCount(0),
	  _maxDataSize(maxMem),
	  _unusedDataSize(0), _unusedDataCount(0)
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
    CacheLock cachelock;

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
    void setDataUnused(PtexLruItem* data, int size);
    void removeData(int size);

    void purgeFiles() {
	while (_unusedFileCount > _maxFiles) 
	{
	    if (!_unusedFiles.pop()) break;
	    // note: pop will destroy item and item destructor will
	    // call removeFile which will decrement _unusedFileCount
	}
    }
    void purgeData() {
	while ((_unusedDataSize > _maxDataSize) &&
	       (_unusedDataCount > _minDataCount))
	{
	    if (!_unusedData.pop()) break;
	    // note: pop will destroy item and item destructor will
	    // call removeData which will decrement _unusedDataSize
	    // and _unusedDataCount
	}
    }

protected:
    ~PtexCacheImpl();

private:
    bool _pendingDelete;	             // flag set if delete is pending

    int _maxFiles, _unusedFileCount;	     // file limit, current unused file count
    long int _maxDataSize, _unusedDataSize;  // data limit (bytes), current size
    int _minDataCount, _unusedDataCount;     // min, current # of unused data blocks
    PtexLruList _unusedFiles, _unusedData;   // lists of unused items
};


class PtexCachedFile : public PtexLruItem
{
public:
    PtexCachedFile(void** parent, PtexCacheImpl* cache)
	: PtexLruItem(parent), _cache(cache), _refcount(1)
    { _cache->addFile(); }
    void ref() { assert(_cache->cachelock.locked()); if (!_refcount++) _cache->setFileInUse(this); }
    void unref() { assert(_cache->cachelock.locked()); if (!--_refcount) _cache->setFileUnused(this); }
protected:
    virtual ~PtexCachedFile() {	_cache->removeFile(); }
    PtexCacheImpl* _cache;
private:
    int _refcount;
};


class PtexCachedData : public PtexLruItem
{
public:
    PtexCachedData(void** parent, PtexCacheImpl* cache, int size)
	: PtexLruItem(parent), _cache(cache), _refcount(1), _size(size)
    { _cache->addData(); }
    void ref() { assert(_cache->cachelock.locked()); if (!_refcount++) _cache->setDataInUse(this, _size); }
    void unref() { assert(_cache->cachelock.locked()); if (!--_refcount) _cache->setDataUnused(this, _size); }
protected:
    void incSize(int size) { _size += size; }
    virtual ~PtexCachedData() { _cache->removeData(_size); }
    PtexCacheImpl* _cache;
private:
    int _refcount;
    int _size;
};


#endif
