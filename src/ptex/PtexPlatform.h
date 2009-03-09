#ifndef PtexPlatform_h
#define PtexPlatform_h
#define PtexPlatform_h
/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

/** @file PtexPlatform.h
    @brief Platform-specific classes, functions, and includes.
*/

// platform-specific includes
#if defined(_WIN32) || defined(_WINDOWS) || defined(_MSC_VER)
#ifndef WINDOWS
#define WINDOWS
#endif
#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_NO_DEPRECATE 1
#define NOMINMAX 1

// windows - defined for both Win32 and Win64
#include <Windows.h>
#include <malloc.h>
#include <io.h>

#else

// linux/unix/posix
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
// OS for spinlock
#ifdef __APPLE__
#include <libkern/OSAtomic.h>
#include <sys/types.h>
#endif
#endif

// general includes
#include <stdio.h>
#include <math.h>
#include <assert.h>

// missing functions on Windows
#ifdef WINDOWS
#define snprintf sprintf_s
#define strtok_r strtok_s
typedef __int64 FilePos;
#define fseeko _fseeki64
#define ftello _ftelli64
#define PATH_MAX MAX_PATH

inline double log2(double x) {
    return log(x) * 1.4426950408889634; 
}

#else
typedef off_t FilePos;
#endif
    

namespace PtexInternal {

    /*
     * Mutex/SpinLock classes
     */

#ifdef WINDOWS

    class _Mutex {
    public:
	_Mutex()       { _mutex = CreateMutex(NULL, FALSE, NULL); }
	~_Mutex()      { CloseHandle(_mutex); }
	void lock()   { WaitForSingleObject(_mutex, INFINITE); }
	void unlock() { ReleaseMutex(_mutex); }
    private:
	HANDLE _mutex;
    };

    class _SpinLock {
    public:
	_SpinLock()    { InitializeCriticalSection(&_spinlock); }
	~_SpinLock()   { DeleteCriticalSection(&_spinlock); }
	void lock()   { EnterCriticalSection(&_spinlock); }
	void unlock() { LeaveCriticalSection(&_spinlock); }
    private:
	CRITICAL_SECTION _spinlock;
    };

#else
    // assume linux/unix/posix

    class _Mutex {
     public:
	_Mutex()      { pthread_mutex_init(&_mutex, 0); }
	~_Mutex()     { pthread_mutex_destroy(&_mutex); }
	void lock()   { pthread_mutex_lock(&_mutex); }
	void unlock() { pthread_mutex_unlock(&_mutex); }
    private:
	pthread_mutex_t _mutex;
    };

#ifdef __APPLE__
    class _SpinLock {
    public:
	_SpinLock()   { _spinlock = 0; }
	~_SpinLock()  { }
	void lock()   { OSSpinLockLock(&_spinlock); }
	void unlock() { OSSpinLockUnlock(&_spinlock); }
    private:
	OSSpinLock _spinlock;
    };
#else
    class _SpinLock {
    public:
	_SpinLock()   { pthread_spin_init(&_spinlock, PTHREAD_PROCESS_PRIVATE); }
	~_SpinLock()  { pthread_spin_destroy(&_spinlock); }
	void lock()   { pthread_spin_lock(&_spinlock); }
	void unlock() { pthread_spin_unlock(&_spinlock); }
    private:
	pthread_spinlock_t _spinlock;
    };
#endif // __APPLE__
#endif
}

#endif // PtexPlatform_h
