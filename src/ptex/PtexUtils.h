#ifndef PtexUtils_h
#define PtexUtils_h

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

#include <cmath>
#include "PtexExports.h"
#include "Ptexture.h"
#include "PtexHalf.h"

#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

#include "PtexVersion.h"

PTEX_NAMESPACE_BEGIN
namespace PtexUtils {

inline bool isPowerOfTwo(int x)
{
    return !(x&(x-1));
}

inline uint32_t ones(uint32_t x)
{
    // count number of ones
    x = (x & 0x55555555) + ((x >> 1) & 0x55555555); // add pairs of bits
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333); // add bit pairs
    x = (x & 0x0f0f0f0f) + ((x >> 4) & 0x0f0f0f0f); // add nybbles
    x += (x >> 8);                                      // add bytes
    x += (x >> 16);                                     // add words
    return(x & 0xff);
}

inline uint32_t floor_log2(uint32_t x)
{
    // floor(log2(n))
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    return ones(x>>1);
}

inline uint32_t ceil_log2(uint32_t x)
{
    // ceil(log2(n))
    bool isPow2 = isPowerOfTwo(x);
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    return ones(x>>1) + !isPow2;
}

inline float reciprocalPow2(int power)
{
    // 1.0/pow(2,power)
    union {
        float f;
        int32_t i;
    };
    i = (127-power)<<23;
    return f;
}

inline int calcResFromWidth(float w)
{
    // read exponent directly from float32 representation
    // equiv to ceil(log2(1.0/w)) but much faster and no error
    union {
        float wf;
        int32_t wi;
    };
    wf = w;
    int result = 127 - ((wi >> 23) & 0xff);
    return result;
}

inline float smoothstep(float x, float a, float b)
{
    if ( x < a ) return 0;
    if ( x >= b ) return 1;
    x = (x - a)/(b - a);
    return x*x * (3 - 2*x);
}

inline float qsmoothstep(float x, float a, float b)
{
    // quintic smoothstep (cubic is only C1)
    if ( x < a ) return 0;
    if ( x >= b ) return 1;
    x = (x - a)/(b - a);
    return x*x*x * (10 + x * (-15 + x*6));
}

template<typename T>
inline T abs(T x) { return x > 0 ? x : -x; }

inline float abs(float x)
{
    union {
        float f;
        int32_t i;
    };
    f = x;
    i &= 0x7fffffff;
    return f;
}

template<typename T>
inline T min(T a, T b) { return a < b ? a : b; }

template<typename T>
inline T max(T a, T b) { return a > b ? a : b; }

template<typename T>
inline T clamp(T x, T lo, T hi) { return min(max(x,lo),hi); }

template<typename T>
inline T halve(T val) { return T(val>>1); }

inline float halve(float val) { return 0.5f * val; }
inline PtexHalf halve(PtexHalf val) { return 0.5f * val; }

template<typename T>
inline T quarter(T val) { return T(val>>2); }

inline float quarter(float val) { return 0.25f * val; }
inline PtexHalf quarter(PtexHalf val) { return 0.25f * val; }

PTEXAPI
bool isConstant(const void* data, int stride, int ures, int vres, int pixelSize);

PTEXAPI
void interleave(const void* src, int sstride, int ures, int vres,
                void* dst, int dstride, DataType dt, int nchannels);

PTEXAPI
void deinterleave(const void* src, int sstride, int ures, int vres,
                  void* dst, int dstride, DataType dt, int nchannels);

PTEXAPI
void encodeDifference(void* data, int size, DataType dt);

PTEXAPI
void decodeDifference(void* data, int size, DataType dt);

typedef void ReduceFn(const void* src, int sstride, int ures, int vres,
                      void* dst, int dstride, DataType dt, int nchannels);

PTEXAPI
void reduce(const void* src, int sstride, int ures, int vres,
            void* dst, int dstride, DataType dt, int nchannels);

PTEXAPI
void reduceu(const void* src, int sstride, int ures, int vres,
             void* dst, int dstride, DataType dt, int nchannels);

PTEXAPI
void reducev(const void* src, int sstride, int ures, int vres,
             void* dst, int dstride, DataType dt, int nchannels);

PTEXAPI
void reduceTri(const void* src, int sstride, int ures, int vres,
               void* dst, int dstride, DataType dt, int nchannels);

PTEXAPI
void average(const void* src, int sstride, int ures, int vres,
             void* dst, DataType dt, int nchannels);

PTEXAPI
void fill(const void* src, void* dst, int dstride,
          int ures, int vres, int pixelsize);

PTEXAPI
void copy(const void* src, int sstride, void* dst, int dstride,
          int nrows, int rowlen);

PTEXAPI
void blend(const void* src, float weight, void* dst, bool flip,
           int rowlen, DataType dt, int nchannels);

PTEXAPI
void multalpha(void* data, int npixels, DataType dt, int nchannels, int alphachan);

PTEXAPI
void divalpha(void* data, int npixels, DataType dt, int nchannels, int alphachan);


PTEXAPI
void genRfaceids(const FaceInfo* faces, int nfaces,
                 uint32_t* rfaceids, uint32_t* faceids);

// fixed length vector accumulator: dst[i] += val[i] * weight
template<typename T, int n>
struct VecAccum {
    VecAccum() {}
    void operator()(float* dst, const T* val, float weight)
    {
        *dst += (float)*val * weight;
        // use template to unroll loop
        VecAccum<T,n-1>()(dst+1, val+1, weight);
    }
};
template<typename T>
struct VecAccum<T,0> { void operator()(float*, const T*, float) {} };

// variable length vector accumulator: dst[i] += val[i] * weight
template<typename T>
struct VecAccumN {
    void operator()(float* dst, const T* val, int nchan, float weight)
    {
        for (int i = 0; i < nchan; i++) dst[i] += (float)val[i] * weight;
    }
};

// fixed length vector multiplier: dst[i] += val[i] * weight
template<typename T, int n>
struct VecMult {
    VecMult() {}
    void operator()(float* dst, const T* val, float weight)
    {
        *dst = (float)*val * weight;
        // use template to unroll loop
        VecMult<T,n-1>()(dst+1, val+1, weight);
    }
};
template<typename T>
struct VecMult<T,0> { void operator()(float*, const T*, float) {} };

// variable length vector multiplier: dst[i] = val[i] * weight
template<typename T>
struct VecMultN {
    void operator()(float* dst, const T* val, int nchan, float weight)
    {
        for (int i = 0; i < nchan; i++) dst[i] = (float)val[i] * weight;
    }
};

typedef void (*ApplyConstFn)(float weight, float* dst, void* data, int nChan);
extern ApplyConstFn applyConstFunctions[20];
inline void applyConst(float weight, float* dst, void* data, Ptex::DataType dt, int nChan)
{
    // dispatch specialized apply function
    ApplyConstFn fn = applyConstFunctions[((unsigned)nChan<=4)*nChan*4 + dt];
    fn(weight, dst, data, nChan);
}

#ifdef __SSE4_1__
inline float floor(float f) {
    float result;
    _mm_store_ss(&result, _mm_round_ps(_mm_set1_ps(f), (_MM_FROUND_NO_EXC | _MM_FROUND_TO_NEG_INF)));
    return result;
}
inline float ceil(float f) {
    float result;
    _mm_store_ss(&result, _mm_round_ps(_mm_set1_ps(f), (_MM_FROUND_NO_EXC | _MM_FROUND_TO_POS_INF)));
    return result;
}
#else
using std::floor;
using std::ceil;
#endif

} // end namespace Utils

PTEX_NAMESPACE_END

#endif
