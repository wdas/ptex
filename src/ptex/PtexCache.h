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
#include "PtexHashMap.h"
#include "PtexReader.h"

using namespace PtexInternal;

class PtexCachedReader;

/** Cache for reading Ptex texture files */
class PtexReaderCache : public PtexCache
{
public:
    PtexReaderCache(int maxFiles, int maxMem, bool premultiply, PtexInputHandler* handler)
	: _maxFiles(maxFiles), _maxMem(maxMem), _io(handler), _cleanupCount(0), _premultiply(premultiply)
    {}

    ~PtexReaderCache()
    {}

    virtual void release() { delete this; }

    virtual void setSearchPath(const char* path)
    {
        // record path
        _searchpath = path ? path : "";

        // split into dirs
        _searchdirs.clear();

        if (path) {
            const char* cp = path;
            while (1) {
                const char* delim = strchr(cp, ':');
                if (!delim) {
                    if (*cp) _searchdirs.push_back(cp);
                    break;
                }
                int len = int(delim-cp);
                if (len) _searchdirs.push_back(std::string(cp, len));
                cp = delim+1;
            }
        }
    }

    virtual const char* getSearchPath()
    {
	return _searchpath.c_str();
    }

    virtual PtexTexture* get(const char* path, Ptex::String& error);

    // TODO
    virtual void purge(PtexTexture* /*texture*/) {}
    virtual void purge(const char* /*filename*/) {}
    virtual void purgeAll() {}

private:
    int _maxFiles;
    int _maxMem;
    PtexInputHandler* _io;
    std::string _searchpath;
    std::vector<std::string> _searchdirs;
    typedef PtexHashMap<StringKey,PtexCachedReader*> FileMap;
    FileMap _files;
    int _cleanupCount;
    bool _premultiply;
};

class PtexCachedReader : public PtexReader
{
public:
    PtexCachedReader(bool premultiply, PtexInputHandler* handler)
        : PtexReader(premultiply, handler)
    {}

    virtual void release() {} // TODO
};


#endif
