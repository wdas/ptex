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
#include "PtexUtils.h"
#include "PtexHalf.h"
#include "PtexSeparableKernel.h"

namespace {

    // Vector Accumulate with a rotation function for the first 2 channels
    // (to rotate based on rotation of face)
    template<typename T, int n>
    struct VecAccumTV {
	void operator()(float* dst, const T* val, float weight,
	    void (*rotFn)(float*, const T*, float))
	{
	    rotFn(dst, val, weight);
	    PtexUtils::VecAccum<T,n-2>()(dst+2, val+2, weight);
	}
    };

    // for nchan=1,0, don't rotate
    template<typename T>
    struct VecAccumTV<T,1> {
	void operator()(float* dst, const T* val, float weight,
	    void (*)(float*, const T*, float))
	{
	    PtexUtils::VecAccum<T,1>()(dst, val, weight);
	}
    };
    template<typename T>
    struct VecAccumTV<T,0> {
	void operator()(float* dst, const T* val, float weight,
	    void (*)(float*, const T*, float))
	{
	    PtexUtils::VecAccum<T,0>()(dst, val, weight);
	}
    };

    // generic rotated vec accum.
    template<typename T>
    struct VecAccumNTV {
	void operator()(float* dst, const T* val, int nchan, float weight,
	    void (*rotFn)(float*, const T*, float))
	{
	    if (nchan >= 2)
	    {
		rotFn(dst, val, weight);
		PtexUtils::VecAccumN<T>()(dst+2, val+2, nchan-2, weight);
	    }
	    else {
		PtexUtils::VecAccumN<T>()(dst, val, nchan, weight);
	    }
	}
    };

    // Rotation functions
    // Similar to the rotations for UV sampling coordinates
    // R0/1/2/3 are the number of rotations CCW (called from rotate()
    template<typename T>
    void VecAccumTV_R0(float* dst, const T* val, float weight)
    {
	dst[0] += val[0] * weight;
	dst[1] += val[1] * weight;
    }
    template<typename T>
    void VecAccumTV_R1(float* dst, const T* val, float weight)
    {
	dst[0] -= val[1] * weight;
	dst[1] += val[0] * weight;
    }
    template<typename T>
    void VecAccumTV_R2(float* dst, const T* val, float weight)
    {
	dst[0] -= val[0] * weight;
	dst[1] -= val[1] * weight;
    }
    template<typename T>
    void VecAccumTV_R3(float* dst, const T* val, float weight)
    {
	dst[0] += val[1] * weight;
	dst[1] -= val[0] * weight;
    }
    typedef void (*AccumRotFn)(float* dst, const float* src, float weight);
    static AccumRotFn accumRotFunctions[4] = {
	VecAccumTV_R0<float>, VecAccumTV_R1<float>,
	VecAccumTV_R2<float>, VecAccumTV_R3<float>,
    };

    // apply to 1..4 channels (unrolled channel loop) of packed data (nTxChan==nChan)
    template<class T, int nChan>
    void Apply(PtexSeparableKernel& k, float* result, void* data, int /*nChan*/, int /*nTxChan*/)
    {
	float* rowResult = (float*) alloca(nChan*sizeof(float));
	int rowlen = k.res.u() * nChan;
	int datalen = k.uw * nChan;
	int rowskip = rowlen - datalen;
	float* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nChan;
	T* pEnd = p + k.vw * rowlen;
	AccumRotFn rot = accumRotFunctions[k.rot&3];
	while (p != pEnd)
	{
	    float* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    PtexUtils::VecMult<T,nChan>()(rowResult, p, *kup++);
	    p += nChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		PtexUtils::VecAccum<T,nChan>()(rowResult, p, *kup++);
		p += nChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    VecAccumTV<float,nChan>()(result, rowResult, *kvp++, rot);
	    p += rowskip;
	}
    }

    // apply to 1..4 channels (unrolled channel loop) w/ pixel stride
    template<class T, int nChan>
    void ApplyS(PtexSeparableKernel& k, float* result, void* data, int /*nChan*/, int nTxChan)
    {
	float* rowResult = (float*) alloca(nChan*sizeof(float));
	int rowlen = k.res.u() * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	float* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nTxChan;
	T* pEnd = p + k.vw * rowlen;
	AccumRotFn rot = accumRotFunctions[k.rot&3];
	while (p != pEnd)
	{
	    float* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    PtexUtils::VecMult<T,nChan>()(rowResult, p, *kup++);
	    p += nTxChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		PtexUtils::VecAccum<T,nChan>()(rowResult, p, *kup++);
		p += nTxChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    VecAccumTV<float,nChan>()(result, rowResult, *kvp++, rot);
	    p += rowskip;
	}
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyN(PtexSeparableKernel& k, float* result, void* data, int nChan, int nTxChan)
    {
	float* rowResult = (float*) alloca(nChan*sizeof(float));
	int rowlen = k.res.u() * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	float* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nTxChan;
	T* pEnd = p + k.vw * rowlen;
	AccumRotFn rot = accumRotFunctions[k.rot&3];
	while (p != pEnd)
	{
	    float* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    PtexUtils::VecMultN<T>()(rowResult, p, nChan, *kup++);
	    p += nTxChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		PtexUtils::VecAccumN<T>()(rowResult, p, nChan, *kup++);
		p += nTxChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    VecAccumNTV<float>()(result, rowResult, nChan, *kvp++, rot);
	    p += rowskip;
	}
    }

    // apply const with consideration to rot
    template<class T>
    void ApplyConstNTV(float weight, float* dst, void* data, int nChan, int rot)
    {
	typedef void (*RotConstFn)(float* dst, const T* val, float weight);
	RotConstFn rotFn;
	switch (rot&3)
	{
	    default: rotFn = VecAccumTV_R0<T>; break;
	    case 1: rotFn = VecAccumTV_R1<T>; break;
	    case 2: rotFn = VecAccumTV_R2<T>; break;
	    case 3: rotFn = VecAccumTV_R3<T>; break;
	}
	VecAccumNTV<T>()(dst, (T*) data, nChan, weight, rotFn);
    }

    template<class T, int nChan>
    void ApplyConstTV(float weight, float* dst, void* data, int /*nChan*/, int rot)
    {
	typedef void (*RotConstFn)(float* dst, const T* val, float weight);
	RotConstFn rotFn;
	switch (rot&3)
	{
	    default: rotFn = VecAccumTV_R0<T>; break;
	    case 1: rotFn = VecAccumTV_R1<T>; break;
	    case 2: rotFn = VecAccumTV_R2<T>; break;
	    case 3: rotFn = VecAccumTV_R3<T>; break;
	}
	VecAccumTV<T, nChan>()(dst, (T*) data, weight, rotFn);
    }

}



PtexSeparableKernel::ApplyFn
PtexSeparableKernel::applyFunctions[] = {
    // nChan == nTxChan
    ApplyN<uint8_t>,  ApplyN<uint16_t>,  ApplyN<PtexHalf>,  ApplyN<float>,
    Apply<uint8_t,1>, Apply<uint16_t,1>, Apply<PtexHalf,1>, Apply<float,1>,
    Apply<uint8_t,2>, Apply<uint16_t,2>, Apply<PtexHalf,2>, Apply<float,2>,
    Apply<uint8_t,3>, Apply<uint16_t,3>, Apply<PtexHalf,3>, Apply<float,3>,
    Apply<uint8_t,4>, Apply<uint16_t,4>, Apply<PtexHalf,4>, Apply<float,4>,

    // nChan != nTxChan (need pixel stride)
    ApplyN<uint8_t>,   ApplyN<uint16_t>,   ApplyN<PtexHalf>,   ApplyN<float>,
    ApplyS<uint8_t,1>, ApplyS<uint16_t,1>, ApplyS<PtexHalf,1>, ApplyS<float,1>,
    ApplyS<uint8_t,2>, ApplyS<uint16_t,2>, ApplyS<PtexHalf,2>, ApplyS<float,2>,
    ApplyS<uint8_t,3>, ApplyS<uint16_t,3>, ApplyS<PtexHalf,3>, ApplyS<float,3>,
    ApplyS<uint8_t,4>, ApplyS<uint16_t,4>, ApplyS<PtexHalf,4>, ApplyS<float,4>,

};

PtexSeparableKernel::ApplyConstFn
PtexSeparableKernel::applyConstFunctions[] = {
    ApplyConstNTV<uint8_t>,  ApplyConstNTV<uint16_t>,  ApplyConstNTV<PtexHalf>,  ApplyConstNTV<float>,
    ApplyConstTV<uint8_t,1>, ApplyConstTV<uint16_t,1>, ApplyConstTV<PtexHalf,1>, ApplyConstTV<float,1>,
    ApplyConstTV<uint8_t,2>, ApplyConstTV<uint16_t,2>, ApplyConstTV<PtexHalf,2>, ApplyConstTV<float,2>,
    ApplyConstTV<uint8_t,3>, ApplyConstTV<uint16_t,3>, ApplyConstTV<PtexHalf,3>, ApplyConstTV<float,3>,
    ApplyConstTV<uint8_t,4>, ApplyConstTV<uint16_t,4>, ApplyConstTV<PtexHalf,4>, ApplyConstTV<float,4>,
};
