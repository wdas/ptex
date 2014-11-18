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

/**
   @file PtexCache.cpp
   @brief LRU Cache Implementation

   <b> Ownership.</b>
   The cache owns all files and data.  If an object is in use, the
   cache will not delete it.  When it is no longer in use, it may be
   kept or may be deleted to keep resource usage under the set limits.
   Deletions are done in lru order.

   <b> Resource Tracking.</b>
   All objects are created as part of the cache and have a ptr back to
   the cache.  Each object updates the cache's resource total when it is
   created or deleted.  Unused objects are kept in an lru list in the
   cache.  Only objects in the lru list can be deleted.
   
   <b> Reference Counting.</b>
   Every object has a ref count to track whether it is being used.
   But objects don't generally ref their parent or children (otherwise
   nothing would get freed).

   A data handle must hold onto (and ref) all the objects it is using
   or may need in the future.  E.g. For a non-tiled face, this is just
   the single face data block.  For a tiled face, the file, the tiled
   face, and the current tile must all be ref'd.

   <b> Parents, Children, and Orphans.</b>
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

   <b> Cache LifeTime.</b>
   When a cache is released from its owner, it will delete itself but
   only after all objects it owns are no longer in use.  To do this, a
   ref count on the cache is used.  The owner holds 1 ref (only one
   owner allowed), and each object holds a ref (maintained internally).

   <b> Threading.</b>
   To fully support multi-threading, the following data structures
   must be protected with a mutex: the cache lru lists, ref counts,
   and parent/child ptrs.  This is done with a single mutex per cache.
   To avoid the need for recursive locks and to minimize the number of
   lock points, this mutex is locked and unlocked primarily at the
   external api boundary for methods that affect the cache state:
   (e.g. getMetaData, getData, getTile, release, purge, and purgeAll).
   Care must be taken to release the cache lock when calling any external
   api from within the library.

   Also, in order to prevent thread starvation, the cache lock is
   released during file reads and significant computation such as
   generating an image data reduction.  Additional mutexes are used to
   prevent contention in these cases:
   - 1 mutex per cache to prevent concurrent file opens
   - 1 mutex per file to prevent concurrent file reads
   - 1 mutex per file to prevent concurrent (and possibly redundant)
   reductions.
 */

#include "PtexPlatform.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <iostream>
#include <ctype.h>
#include "Ptexture.h"
#include "PtexReader.h"
#include "PtexCache.h"

PtexTexture* PtexReaderCache::get(const char* filename, Ptex::String& error)
{
    // lookup reader in map
    StringKey key(filename);
    PtexReader* reader = _files.get(key);
    if (reader) {
	// -1 means previous open attempt failed
	if (intptr_t(reader) == -1) return 0;
	// TODO reader->ref();
	return reader;
    }
    else {
	bool ok = true;

	// make a new reader
	PtexReader* newreader = new PtexCachedReader(_premultiply, _io);

	std::string tmppath;
	const char* pathToOpen = filename;
	if (!_io) {
            bool isAbsolute = (filename[0] == '/'
#ifdef WINDOWS
                               || filename[0] == '\\'
                               || (isalpha(filename[0]) && filename[1] == ':')
#endif
                               );
	    if (!isAbsolute && !_searchdirs.empty()) {
		// file is relative, search in searchpath
		tmppath.reserve(256); // minimize reallocs (will grow automatically)
		bool found = false;
		struct stat statbuf;
		for (size_t i = 0, size = _searchdirs.size(); i < size; i++) {
		    tmppath = _searchdirs[i];
		    tmppath += "/";
		    tmppath += filename;
		    if (stat(tmppath.c_str(), &statbuf) == 0) {
			found = true;
			pathToOpen = tmppath.c_str();
			break;
		    }
		}
		if (!found) {
		    std::string errstr = "Can't find ptex file: ";
		    errstr += filename;
		    error = errstr.c_str();
		    ok = false;
		}
	    }
	}
	newreader->open(pathToOpen, error);

	// record in _files map entry (even if open failed)
        reader = _files.tryInsert(key, newreader);
        if (reader != newreader) {
            // another thread got here first
            delete newreader;
        }

        return reader;
    }
}

PtexCache* PtexCache::create(int maxFiles, int maxMem, bool premultiply,
			     PtexInputHandler* handler)
{
    // set default files to 100
    if (maxFiles <= 0) maxFiles = 100;

    // set default memory to 100 MB
    const int MB = 1024*1024;
    if (maxMem <= 0) maxMem = 100 * MB;

    // if memory is < 1 MB, warn
    if (maxMem < 1 * MB) {
	std::cerr << "Warning, PtexCache created with < 1 MB" << std::endl;
    }

    return new PtexReaderCache(maxFiles, maxMem, premultiply, handler);
}


