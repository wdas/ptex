#ifndef LockFile_h
#define LockFile_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.

   Author: Brent Burley, May 2007
*/

#include <string>

class PtexLockFile
{
public:
    PtexLockFile() : _fd(-1) {}
    PtexLockFile(const char* path, const char* suffix = ".lock")
	: _fd(-1) { lock(path, suffix); }
    ~PtexLockFile() { if (islocked()) unlock(); }
    PtexLockFile(PtexLockFile& lockfile) { *this = lockfile; }

    // assignment transfers ownership!
    void operator=(PtexLockFile& lockfile) {
	_fd = lockfile._fd;
	_path = lockfile._path;
	lockfile._fd = -1;
    }

    bool lock(const char* path, const char* suffix = ".lock");
    bool islocked() { return _fd != -1; }
    bool unlock();
    int fd() { return _fd; }
    const char* path() { return _path.c_str(); }

private:
    int _fd;
    std::string _path;
};

#endif
