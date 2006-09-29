/*
   PtexCache - LRU Cache class for file handles and data (in memory).

   * Ownership:
   The cache owns all files and data.  If an object is in use, the
   cache will not delete it.  When it is no longer in use, it may be
   kept or may be deleted to keep resource usage under the set limits.
   Deletions are done in lru order.

   * Resource Tracking:
   All objects are created as part of the cache and have a ptr back to
   the cache.  Each object updates the cache's resource total when it is
   created or deleted.  Unused objects are kept in an lru list in the
   cache.  Only objects in the lru list can be deleted.
   
   * Reference Counting:
   Every object has a ref count to track whether it is being used.
   But objects don't generally ref their parent or children (otherwise
   nothing would get freed).

   A data handle must hold onto (and ref) all the objects it is using
   or may need in the future.  E.g. For a non-tiled face, this is just
   the single face data block.  For a tiled face, the file, the tiled
   face, and the current tile must all be ref'd.

   * Touching an Object:
   Objects that are used, even for an instant, should still be ref'ed
   and unref'ed in order to keep the objects at the end of the lru
   list.  A touch method is provided to do this efficiently in one
   step.
   
   * Parents, Children, and Orphans:
   Every object must be reachable by some other object, generally the
   object that created it, i.e. it's parent.  Even though the parent
   doesn't own it's children (the cache does), it must still track
   them.  Parentless objects (i.e. orphans) are not reachable and are
   not retained in the cache.
   
   When any object is deleted (file, tiled face, etc.), it must orphan
   its children.  If an orphaned child is not in use, then it is
   immediately deleted.  Otherwise, the child's parent ptr is set to
   null and the child is deleted when it is no longer in use.  A
   parent may also orphan a child that it no longer needs; the
   behavior is the same.

   Each object stores a ptr to its own entry within its parent. When
   the object is deleted by the cache, it clears this pointer so that
   the parent no longer sees it.

   * Cache LifeTime:
   When a cache is released from it's owner, it will delete itself but
   only after all objects it owns are no longer in use.  To do this, a
   ref count on the cache is used.  The owner holds 1 ref (only one
   owner allowed), and each object holds a ref (maintained internally).
 */

#include "Ptexture.h"
#include "PtexReader.h"
#include "PtexCache.h"

PtexCache* PtexCache::create(int maxFiles, int maxMem)
{
    if (maxFiles <= 0) maxFiles = 100;
    if (maxMem <= 0) maxMem = 1024*1024*100;
    return new PtexCacheImpl(maxFiles, maxMem);
}


PtexTexture* PtexCacheImpl::get(const char* filename, std::string& error)
{
    PtexReader*& p = _files[filename];
    if (p) p->ref();
    else {
	p = new PtexReader((void**)&p, this);
	if (!p->open(filename, error)) {
	    p->release();
	    return 0;
	}
    }
    return p;
}


void PtexCacheImpl::setFileUnused(PtexLruItem* file)
{
    _unusedFiles.push(file);
    purgeFiles();
    unref();
}


void PtexCacheImpl::setDataUnused(PtexLruItem* data)
{
    _unusedData.push(data);
    purgeData();
    unref();
}


void PtexCacheImpl::purge(PtexTexture* tx)
{
    PtexReader* reader = dynamic_cast<PtexReader*>(tx);
    if (!reader) return;
    purge(reader->path());
}


void PtexCacheImpl::purge(const char* path)
{
    FileMap::iterator iter = _files.find(path);
    if (iter != _files.end()) {
	iter->second->orphan();
	_files.erase(iter);
    }
}


void PtexCacheImpl::purgeAll()
{
    FileMap::iterator iter = _files.begin();
    while (iter != _files.end()) {
	iter->second->orphan();
	iter = _files.erase(iter);
    }
}
