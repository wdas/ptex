#ifndef PtexCache_h
#define PtexCache_h

#include "Ptexture.h"
namespace PtexInternal {
#include "DGDict.h"
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

    void touch(PtexLruItem* node)
    {
	// extract
	node->_prev->_next = node->_next;
	node->_next->_prev = node->_prev;
	// add to end of list
	node->_next = &_end;
	node->_prev = _end._prev;
	_end._prev->_next = node;
	_end._prev = node;
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
	  _maxFiles(maxFiles), _fileCount(0),
	  _maxData(maxMem), _dataSize(0)
    {}

    virtual void release() { unref(); purgeAll(); }

protected:
    ~PtexCacheImpl() {}

    friend class PtexCachedFile;
    void addFile() { ref(); _fileCount++; purgeFiles(); }
    void setFileInUse(PtexLruItem* file) { ref(); _unusedFiles.extract(file); }
    void setFileUnused(PtexLruItem* file);
    void touchFile(PtexLruItem* file) { _unusedFiles.touch(file); }
    void removeFile() { _fileCount--; }

    friend class PtexCachedData;
    void addData(int size) { ref(); _dataSize += size; purgeData(); }
    void setDataInUse(PtexLruItem* data) { ref(); _unusedData.extract(data); }
    void setDataUnused(PtexLruItem* data);
    void touchData(PtexLruItem* data) { _unusedData.touch(data); }
    void removeData(int size) { _dataSize -= size; }

protected:
    void ref() { _refcount++; }
    void unref() { if (!--_refcount) delete this; }
    void purgeFiles() { while (_fileCount > _maxFiles && _unusedFiles.pop()); }
    void purgeData() { while (_dataSize > _maxData && _unusedData.pop()); }

    int _refcount;
    int _maxFiles, _fileCount;
    long int _maxData, _dataSize;
    PtexLruList _unusedFiles, _unusedData;
};


class PtexCachedFile : public PtexLruItem
{
public:
    PtexCachedFile(void** parent, PtexCacheImpl* cache)
	: PtexLruItem(parent), _cache(cache), _refcount(1)
    { _cache->addFile(); }
    void ref() { if (!_refcount++) _cache->setFileInUse(this); }
    void unref() { if (!--_refcount) _cache->setFileUnused(this); }
    void touch() { if (!inuse()) _cache->touchFile(this); }
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
    void ref() { if (!_refcount++) _cache->setDataInUse(this); }
    void unref() { if (!--_refcount) _cache->setDataUnused(this); }
    void touch() { if (!inuse()) _cache->touchData(this); }
protected:
    virtual ~PtexCachedData() { _cache->removeData(_size); }
    PtexCacheImpl* _cache;
private:
    int _refcount;
    int _size;
};


#endif
