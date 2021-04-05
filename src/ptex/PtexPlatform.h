#ifndef PtexPlatform_h
#define PtexPlatform_h
/*
PTEX SOFTWARE
Copyright 2014 Disney Enterprises, Inc.  All rights reserved

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

/** @file PtexPlatform.h
    @brief Platform-specific classes, functions, and includes.
*/

#include "PtexInt.h"

// compiler-specific defines: PTEX_COMPILER_{CLANG,GCC,ICC,MSVC}
#if defined(__clang__)
#   define PTEX_COMPILER_CLANG
#elif defined(__GNUC__)
#   define PTEX_COMPILER_GCC
#elif defined(__ICC)
#   define PTEX_COMPILER_ICC
#elif defined(_MSC_VER)
#   define PTEX_COMPILER_MSVC
#endif

// platform-specific includes
#if defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS) || defined(_MSC_VER)
#define PTEX_PLATFORM_WINDOWS
#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_NO_DEPRECATE 1
#define NOMINMAX 1

// windows - defined for both Win32 and Win64
#include <Windows.h>
#include <malloc.h>
#include <io.h>
#include <tchar.h>
#include <process.h>

#else

// linux/unix/posix
#include <stdlib.h>
#include <alloca.h>
#include <string.h>
#include <pthread.h>

#ifdef __APPLE__
#include <os/lock.h>
#include <sys/types.h>
#include <unistd.h>
#define PTEX_PLATFORM_MACOS
#else
#define PTEX_PLATFORM_UNIX
#endif
#endif

// general includes
#include <stdio.h>
#include <math.h>
#include <assert.h>

// missing functions on Windows
#ifdef PTEX_PLATFORM_WINDOWS
typedef __int64 FilePos;
#define fseeko _fseeki64
#define ftello _ftelli64

#else
typedef off_t FilePos;
#endif

#include "PtexVersion.h"

PTEX_NAMESPACE_BEGIN

/*
 * Mutex
 */

#ifdef PTEX_PLATFORM_WINDOWS

class Mutex {
public:
    Mutex()       { _mutex = CreateMutex(NULL, FALSE, NULL); }
    ~Mutex()      { CloseHandle(_mutex); }
    void lock()   { WaitForSingleObject(_mutex, INFINITE); }
    bool trylock() { return WAIT_TIMEOUT != WaitForSingleObject(_mutex,0);}
    void unlock() { ReleaseMutex(_mutex); }
private:
    HANDLE _mutex;
};

class SpinLock {
public:
    SpinLock()    { InitializeCriticalSection(&_spinlock); }
    ~SpinLock()   { DeleteCriticalSection(&_spinlock); }
    void lock()   { EnterCriticalSection(&_spinlock); }
    bool trylock() { return TryEnterCriticalSection(&_spinlock); }
    void unlock() { LeaveCriticalSection(&_spinlock); }
private:
    CRITICAL_SECTION _spinlock;
};

#else
// assume linux/unix/posix

class Mutex {
public:
    Mutex()      { pthread_mutex_init(&_mutex, 0); }
    ~Mutex()     { pthread_mutex_destroy(&_mutex); }
    void lock()   { pthread_mutex_lock(&_mutex); }
    bool trylock() { return 0 == pthread_mutex_trylock(&_mutex); }
    void unlock() { pthread_mutex_unlock(&_mutex); }
private:
    pthread_mutex_t _mutex;
};

#ifdef __APPLE__
class SpinLock {
public:
    SpinLock()   { _spinlock = OS_UNFAIR_LOCK_INIT; }
    ~SpinLock()  { }
    void lock()   { os_unfair_lock_lock(&_spinlock); }
    bool trylock() { return os_unfair_lock_trylock(&_spinlock); }
    void unlock() { os_unfair_lock_unlock(&_spinlock); }
private:
    os_unfair_lock _spinlock;
};
#else
class SpinLock {
public:
    SpinLock()   { pthread_spin_init(&_spinlock, PTHREAD_PROCESS_PRIVATE); }
    ~SpinLock()  { pthread_spin_destroy(&_spinlock); }
    void lock()   { pthread_spin_lock(&_spinlock); }
    bool trylock() { return 0 == pthread_spin_trylock(&_spinlock); }
    void unlock() { pthread_spin_unlock(&_spinlock); }
private:
    pthread_spinlock_t _spinlock;
};
#endif // __APPLE__
#endif

/*
 * Atomics
 */

#ifdef PTEX_PLATFORM_WINDOWS
    #define ATOMIC_ALIGNED __declspec(align(8))
    #define ATOMIC_ADD32(x,y) (InterlockedExchangeAdd((volatile long*)(x),(long)(y)) + (y))
    #define ATOMIC_ADD64(x,y) (InterlockedExchangeAdd64((volatile long long*)(x),(long long)(y)) + (y))
    #define ATOMIC_SUB32(x,y) (InterlockedExchangeAdd((volatile long*)(x),-((long)(y))) - (y))
    #define ATOMIC_SUB64(x,y) (InterlockedExchangeAdd64((volatile long long*)(x),-((long long)(y))) - (y))
    #define MEM_FENCE()       MemoryBarrier()
    #define BOOL_CMPXCH32(x,y,z) (InterlockedCompareExchange((volatile long*)(x),(long)(z),(long)(y))   == (y))
    #define BOOL_CMPXCH64(x,y,z) (InterlockedCompareExchange64((volatile long long*)(x),(long long)(z),(long long)(y)) == (y))
    #ifdef NDEBUG
        #define PTEX_INLINE __forceinline
    #else
        #define PTEX_INLINE inline
    #endif
#else
    #define ATOMIC_ALIGNED __attribute__((aligned(8)))
    #define ATOMIC_ADD32(x,y)  __sync_add_and_fetch(x,y)
    #define ATOMIC_ADD64(x,y)  __sync_add_and_fetch(x,y)
    #define ATOMIC_SUB32(x,y)  __sync_sub_and_fetch(x,y)
    #define ATOMIC_SUB64(x,y)  __sync_sub_and_fetch(x,y)
    #define MEM_FENCE()        __sync_synchronize()
    #define BOOL_CMPXCH32(x,y,z) __sync_bool_compare_and_swap((x),(y),(z))
    #define BOOL_CMPXCH64(x,y,z) __sync_bool_compare_and_swap((x),(y),(z))

    #ifdef NDEBUG
        #define PTEX_INLINE inline __attribute__((always_inline))
    #else
        #define PTEX_INLINE inline
    #endif
#endif

template <typename T>
PTEX_INLINE T AtomicAdd(volatile T* target, T value)
{
    switch(sizeof(T)){
    case 4:
        return (T)ATOMIC_ADD32(target, value);
        break;
    case 8:
        return (T)ATOMIC_ADD64(target, value);
        break;
    default:
        assert(0=="Can only use 32 or 64 bit atomics");
        return *(T*)NULL;
    }
}

template <typename T>
PTEX_INLINE T AtomicIncrement(volatile T* target)
{
    return AtomicAdd(target, (T)1);
}

template <typename T>
PTEX_INLINE T AtomicSubtract(volatile T* target, T value)
{
    switch(sizeof(T)){
    case 4:
        return (T)ATOMIC_SUB32(target, value);
        break;
    case 8:
        return (T)ATOMIC_SUB64(target, value);
        break;
    default:
        assert(0=="Can only use 32 or 64 bit atomics");
        return *(T*)NULL;
    }
}

template <typename T>
PTEX_INLINE T AtomicDecrement(volatile T* target)
{
    return AtomicSubtract(target, (T)1);
}

// GCC is pretty forgiving, but ICC only allows int, long and long long
// so use partial specialization over structs (C(98)) to get certain compilers
// to do the specialization to sizeof(T) before doing typechecking and
// throwing errors for no good reason.
template <typename T, size_t n>
struct AtomicCompareAndSwapImpl;

template <typename T>
struct AtomicCompareAndSwapImpl<T, sizeof(uint32_t)> {
    PTEX_INLINE bool operator()(T volatile* target, T oldvalue, T newvalue){
        return  BOOL_CMPXCH32((volatile uint32_t*)target,
                              (uint32_t)oldvalue,
                              (uint32_t)newvalue);
    }
};

template <typename T>
struct AtomicCompareAndSwapImpl<T, sizeof(uint64_t)> {
    PTEX_INLINE bool operator()(T volatile* target, T oldvalue, T newvalue){
        return  BOOL_CMPXCH64((volatile uint64_t*)target,
                              (uint64_t)oldvalue,
                              (uint64_t)newvalue);
    }
};

template <typename T>
PTEX_INLINE bool AtomicCompareAndSwap(T volatile* target, T oldvalue, T newvalue)
{
    return AtomicCompareAndSwapImpl<T, sizeof(T)>()(target, oldvalue, newvalue);
}

template <typename T>
PTEX_INLINE void AtomicStore(T volatile* target, T value)
{
    MEM_FENCE();
    *target = value;
}

PTEX_INLINE void PtexMemoryFence()
{
    MEM_FENCE();
}


#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#define CACHE_LINE_PAD(var,type) char var##_pad[CACHE_LINE_SIZE - sizeof(type)]
#define CACHE_LINE_PAD_INIT(var) memset(&var##_pad[0], 0, sizeof(var##_pad))

PTEX_NAMESPACE_END

#endif // PtexPlatform_h
