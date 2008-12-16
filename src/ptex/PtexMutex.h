#ifndef PtexMutex_h
#define PtexMutex_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

// #define DEBUG_THREADING

namespace PtexInternal {
    template <class T>
    class DebugLock : public T {
     public:
	DebugLock() : _locked(0) {}
	void lock()   { T::lock(); _locked = 1; }
	void unlock() { assert(_locked); _locked = 0; T::unlock(); }
	bool locked() { return _locked != 0; }
     private:
	int _locked;
    };

    template <class T>
    class AutoLock {
    public:
	AutoLock(T& m) : _m(m) { _m.lock(); }
	~AutoLock()            { _m.unlock(); }
    private:
	T& _m;
    };

#ifndef NDEBUG
    // add debug wrappers to mutex and spinlock
    typedef DebugLock<_Mutex> Mutex;
    typedef DebugLock<_SpinLock> SpinLock;
#else
    typedef _Mutex Mutex;
    typedef _SpinLock SpinLock;
#endif

    typedef AutoLock<Mutex> AutoMutex;
    typedef AutoLock<SpinLock> AutoSpin;
}

#endif
