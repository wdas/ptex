#ifndef PtexCache_h
#define PtexCache_h

#include "Ptexture.h"
namespace PtexInternal {
#include "DGDict.h"

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
    };
    extern CacheStats stats;
#endif
}
using namespace PtexInternal;

class PtexLruItem {
public:
    bool inuse() { return _prev == 0; }
    void orphan() 
    {
	// parent no longer wants me
	_parent = 0; 
	if (!inuse()) delete this;
    }
    template <typename T> static void orphanList(T& list)
    {
	for (typename T::iterator i=list.begin(); i != list.end(); i++) {
	    PtexLruItem* obj = *i;
	    if (obj) obj->orphan();
	}
    }

protected:
    PtexLruItem(void** parent=0)
	: _parent(parent), _prev(0), _next(0) {}
    virtual ~PtexLruItem()
    {
	// detach from parent (if any)
	if (_parent) *_parent = 0;
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
	: _refcount(1), 
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
	_minDataCount = 10 * (maxFiles < 100 ? maxFiles : 100);
    }

    virtual void release() { purgeAll(); unref(); }

protected:
    ~PtexCacheImpl() {}

    friend class PtexCachedFile;
    void addFile() {
	ref(); purgeFiles();
#ifdef GATHER_STATS
	stats.nfilesOpened++;
#endif
    }
    void setFileInUse(PtexLruItem* file) { 
	ref(); 
	_unusedFiles.extract(file); 
	_unusedFileCount--;
    }
    void setFileUnused(PtexLruItem* file);
    void removeFile() { 
	_unusedFileCount--;
#ifdef GATHER_STATS
	stats.nfilesClosed++;
#endif
    }

    friend class PtexCachedData;
    void addData(int size) {
	ref(); purgeData(); 
#ifdef GATHER_STATS
	stats.ndataAllocated++;
#endif
    }
    void setDataInUse(PtexLruItem* data, int size) {
	ref();
	_unusedData.extract(data); 
	_unusedDataCount--;
	_unusedDataSize -= size;
    }
    void setDataUnused(PtexLruItem* data, int size);
    void removeData(int size) {
	_unusedDataCount --;
	_unusedDataSize -= size;
#ifdef GATHER_STATS
	stats.ndataFreed++;
#endif
    }

protected:
    void ref() { _refcount++; }
    void unref() { if (!--_refcount) delete this; }
    void purgeFiles() {
	while (_unusedFileCount > _maxFiles && _unusedFiles.pop()) {
	    // destructor will call removeFile which will decrement count
	}
    }
    void purgeData() {
	while ((_unusedDataSize > _maxDataSize) &&
	       (_unusedDataCount > _minDataCount) &&
	       _unusedData.pop()) 
	{
	    // destructor will call removeData which will decrement size and count
	}
    }

    int _refcount;		             // refs: owner (1) + in-use items
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
    void ref() { if (!_refcount++) _cache->setFileInUse(this); }
    void unref() { if (!--_refcount) _cache->setFileUnused(this); }
protected:
    virtual ~PtexCachedFile() { _cache->removeFile(); }
    PtexCacheImpl* _cache;
private:
    int _refcount;
};


class PtexCachedData : public PtexLruItem
{
public:
    PtexCachedData(void** parent, PtexCacheImpl* cache, int size)
	: PtexLruItem(parent), _cache(cache), _refcount(1), _size(size)
    { _cache->addData(size); }
    void ref() { if (!_refcount++) setInUse(); }
    void unref() { if (!--_refcount) setUnused(); }
protected:
    virtual void setInUse() { _cache->setDataInUse(this, _size); }
    virtual void setUnused() { _cache->setDataUnused(this, _size); }
    virtual ~PtexCachedData() { _cache->removeData(_size); }
    PtexCacheImpl* _cache;
private:
    int _refcount;
    int _size;
};


#endif
