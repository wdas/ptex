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


/* Lock file w/ waiting and auto-crash recovery

   Requirements:
   * Create lock file and wait if file is already locked.
   * Support possibly hundreds of processes on hundreds of different
     systems waiting for a single lock file.
   * Automatically recover from stuck locks when locking process crashes.

   Goals:
   * Minimize latency for transferring lock to waiting process.
   * Automatically clean up lock files.

   Locking a file in a portable, reliable way is a difficult task.
   Functions such as flock(), lockf(), open(O_CREAT+O_EXCL) are not
   reportedly either non-portable or unreliable on NFS, or both.
   [refs: opengroup/linux man pages; Linux NFS FAQ, section D10]

   The method suggested by the open() man page is to create a unique
   temp file on the same device and then attempt to create a hard link
   to the lock file path using the link() function (which is atomic).
   If the lock file already exists, link() will fail indicating the
   file is already locked by another process.

   There are two key problems with the link() approach.  First, if the
   process holding the lock fails to delete the lock file
   (e.g. because it crashed), then this will create a stuck lock.
   Second, if the process holding the lock is still running, there's
   no way to wait on the lock and be notified when the lock is
   release.  It's possible to sleep and retry periodically, but that
   incurs unnecessary overhead and delays.

   Another option is to use POSIX file locks; i.e. create a lock file
   (in a non-exclusive way) and then lock it with fcntl(F_SETLK).  The
   fcntl lock is automatically released when the file is closed, even
   if the process crashes.  Also, fcntl(F_SETLKW) can be used to wait
   for the lock, and notification is immediate, and no polling overhead
   is required.  So the two key problems seem to be solved.

   One issue remains.  One of our goals is to automatically clean up
   the lock files.  The simple thing to do would be to delete the lock
   file when releasing the lock and let the next process create a new
   lock file.  But this creates a race condition where one process
   could open the lock file but before it requests the lock, another
   process could delete the file (having just unlocked the file for
   instance).  The new lock request would succeed, but because the file
   is now deleted, the lock is worthless as other processes would not
   see the lock.  In addition to the race condition, deleting and
   recreating the lock file repeatedly also incurs more overhead
   because all waiting processes must reset themselves to the new
   lock file each time it is recreated.

   To avoid deleting the lock file when other processes are waiting,
   we can unlock the file and attempt to immediately relock it.  This
   relock attempt *should* fail if other processes are waiting.  The
   race condition is still possible (though less likely), and an
   implementation may not work this way (though ours seems to), so we
   still need to handle the case where the file just locked has been
   deleted.  Ideally, the system should also handle the case where the
   file has been explicitly deleted by the user (in an attempt to remove
   a stuck lock for instance).

   To handle the race condition, we need to check whether the lock
   file is still there after we have acquired a lock on it.  Note that
   it's not sufficient to just test whether the lock file exists
   because the lock file may have already been recreated by another
   process.  To detect whether the file has been deleted and
   recreated, we can compare the inode of the file handle with the
   lock file path.  If the inode values are the same, then the file is
   still there and we *should* be able to just assume ownership of the
   lock.  One more gotcha - the file server is allowed to recycle
   inodes, and this seems to occur frequently in the case when a file
   is deleted and recreated immediately with identical attributes.  An
   additional safeguard can be to compare the creation time.


   Example sequence of events:

   Process A		Process B
   ------------------------------------
   open lock file
   			open lock file
   lock (F_SETLKW)
   			lock (F_SETLKW)
   <lock acquired>
   stat file
   <file matches>
   ...
   unlock file
   <lock released>
			<lock acquired>
   attempt relock			
   <failed>
   done (don't delete)
   			stat file
   			<file matches>
   			...
   			attempt relock			
   			<lock acquired>
   			delete file
   			<lock released>
*/

#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <iostream>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "PtexLockFile.h"


namespace {
    /* we use an alarm signal to interrupt the lock wait after a timeout
       but we don't want the signal to actually do anything */
    void ignoreSignal(int) {} // dummy signal handler
}


bool PtexLockFile::lock(const char* lockpath, const char* suffix)
{
    _path = lockpath;
    _path += suffix;

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    bool locked = 0;

    while (1) {
	// open/creat lock file and wait for lock
	if (_fd != -1) close(_fd);
	_fd = open(_path.c_str(), O_RDWR | O_CREAT, 0666);
	if (_fd == -1) {
	    // open failed
	    break;
	}

	// query file info (for later comparison)
	struct stat fdstat;
	if (fstat(_fd, &fdstat) != 0) {
	    // stat failed
	    break;
	}

	// use alarm signal as timeout
	struct sigaction act, oldact;
	memset(&act, 0, sizeof act);
	memset(&oldact, 0, sizeof act);
	act.sa_handler = ignoreSignal;
	sigaction(SIGALRM, &act, &oldact);
	alarm(60);

	// wait for lock
	int lkw_status = fcntl(_fd, F_SETLKW, &lock);
	int lkw_errno = errno;

	// reset timer
	alarm(0);
	sigaction(SIGALRM, &oldact, 0);

	if (lkw_status != -1) {
	    // lock successful, file should soon be deleted by lock owner
	    // poll for file deletion
	    struct stat s;
	    if (stat(_path.c_str(), &s) == 0 &&
		s.st_ino == fdstat.st_ino && s.st_ctime == fdstat.st_ctime)
	    {
		// original lock file is still there
		// success!
		locked = true;
		break;
	    }
	}
	else {
	    if (lkw_errno == EINTR) {
		// lock failed due to time out - print message and try again
		std::cerr << "Waiting for lock: " << _path << std::endl;
	    }
	    else {
		// lock failed in unexpected way
		break;
	    }
	}
    }

    if (!locked && _fd != -1) {
	// failed - close file
	close(_fd);
    }
    return locked;
}


bool PtexLockFile::unlock()
{
    if (!_fd) return 0;

    // stat file
    struct stat s1, s2;
    bool stat_ok = (fstat(_fd, &s1) == 0);

    // close (and unlock) file
    close(_fd);
	

    // try to reopen and re-lock to see if others are waiting
    if (stat_ok) {
	_fd = open(_path.c_str(), O_RDWR, 0666);
	if (_fd != -1) {
	    // stat again to make sure file hasn't changed
	    if (fstat(_fd, &s2) == 0 &&
		s1.st_ino == s2.st_ino &&
		s1.st_ctime == s2.st_ctime)
	    {
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = 0;
		lock.l_len = 0;
		if (fcntl(_fd, F_SETLK, &lock) != -1) {
		    // re-lock was successful - ok to delete file
		    unlink(_path.c_str());
		}
	    }
	    close(_fd);
	}
    }
    return 1;
}
