/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
#include "PtexPlatform.h"
#include "PtexUtils.h"
#include "PtexHalf.h"
#include "PtexSeparableKernel.h"

namespace {
    // fixed length vector accumulator: result[i] += val[i] * weight
    template<typename T, int n>
    struct VecAccum {
	VecAccum() {}
	void operator()(double* result, const T* val, double weight) 
	{
	    *result += *val * weight;
	    // use template to unroll loop
	    VecAccum<T,n-1>()(result+1, val+1, weight);
	}
    };
    template<typename T>
    struct VecAccum<T,0> { void operator()(double*, const T*, double) {} };

    // variable length vector accumulator: result[i] += val[i] * weight
    template<typename T>
    struct VecAccumN {
	void operator()(double* result, const T* val, int nchan, double weight) 
	{
	    for (int i = 0; i < nchan; i++) result[i] += val[i] * weight;
	}
    };

    // fixed length vector multiplier: result[i] += val[i] * weight
    template<typename T, int n>
    struct VecMult {
	VecMult() {}
	void operator()(double* result, const T* val, double weight) 
	{
	    *result = *val * weight;
	    // use template to unroll loop
	    VecMult<T,n-1>()(result+1, val+1, weight);
	}
    };
    template<typename T>
    struct VecMult<T,0> { void operator()(double*, const T*, double) {} };

    // variable length vector multiplier: result[i] = val[i] * weight
    template<typename T>
    struct VecMultN {
	void operator()(double* result, const T* val, int nchan, double weight) 
	{
	    for (int i = 0; i < nchan; i++) result[i] = val[i] * weight;
	}
    };


    // apply to 1..4 channels (unrolled channel loop) of packed data (nTxChan==nChan)
    template<class T, int nChan>
    void Apply(PtexSeparableKernel& k, double* result, void* data, int /*nChan*/, int /*nTxChan*/)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.res.u() * nChan;
	int datalen = k.uw * nChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nChan;
	T* pEnd = p + k.vw * rowlen;
	while (p != pEnd)
	{
	    double* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    VecMult<T,nChan>()(rowResult, p, *kup++);
	    p += nChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		VecAccum<T,nChan>()(rowResult, p, *kup++);
		p += nChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    VecAccum<double,nChan>()(result, rowResult, *kvp++);
	    p += rowskip;
	}
    }

    // apply to 1..4 channels (unrolled channel loop) w/ pixel stride
    template<class T, int nChan>
    void ApplyS(PtexSeparableKernel& k, double* result, void* data, int /*nChan*/, int nTxChan)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.res.u() * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nTxChan;
	T* pEnd = p + k.vw * rowlen;
	while (p != pEnd)
	{
	    double* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    VecMult<T,nChan>()(rowResult, p, *kup++);
	    p += nTxChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		VecAccum<T,nChan>()(rowResult, p, *kup++);
		p += nTxChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    VecAccum<double,nChan>()(result, rowResult, *kvp++);
	    p += rowskip;
	}
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyN(PtexSeparableKernel& k, double* result, void* data, int nChan, int nTxChan)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.res.u() * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.res.u() + k.u) * nTxChan;
	T* pEnd = p + k.vw * rowlen;
	while (p != pEnd)
	{
	    double* kup = k.ku;
	    T* pRowEnd = p + datalen;
	    // just mult and copy first element
	    VecMultN<T>()(rowResult, p, nChan, *kup++);
	    p += nTxChan;
	    // accumulate remaining elements
	    while (p != pRowEnd) {
		// rowResult[i] = p[i] * ku[u] for i in {0..n-1}
		VecAccumN<T>()(rowResult, p, nChan, *kup++);
		p += nTxChan;
	    }
	    // result[i] += rowResult[i] * kv[v] for i in {0..n-1}
	    VecAccumN<double>()(result, rowResult, nChan, *kvp++);
	    p += rowskip;
	}
    }

    // apply to 1..4 channels, unrolled
    template<class T, int nChan>
    void ApplyConst(double weight, double* result, void* data, int /*nChan*/)
    {
	// result[i] += data[i] * weight for i in {0..n-1}
	VecAccum<T,nChan>()(result, (T*) data, weight);
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyConstN(double weight, double* result, void* data, int nChan)
    {
	// result[i] += data[i] * weight for i in {0..n-1}
	VecAccumN<T>()(result, (T*) data, nChan, weight);
    }
}



PtexSeparableKernel::ApplyFn
PtexSeparableKernel::applyFunctions[] = {
    // nChan == nTxChan
    ApplyN<uint8_t>, ApplyN<uint16_t>, ApplyN<PtexHalf>, ApplyN<float>,
    Apply<uint8_t, 1>, Apply<uint16_t,1>, Apply<PtexHalf,1>, Apply<float,   1>,
    Apply<uint8_t, 2>, Apply<uint16_t,2>, Apply<PtexHalf,2>, Apply<float,   2>,
    Apply<uint8_t, 3>, Apply<uint16_t,3>, Apply<PtexHalf,3>, Apply<float,   3>,
    Apply<uint8_t, 4>, Apply<uint16_t,4>, Apply<PtexHalf,4>, Apply<float,   4>,

    // nChan != nTxChan (need pixel stride)
    ApplyN<uint8_t>, ApplyN<uint16_t>, ApplyN<PtexHalf>, ApplyN<float>,
    ApplyS<uint8_t, 1>, ApplyS<uint16_t,1>, ApplyS<PtexHalf,1>, ApplyS<float,   1>,
    ApplyS<uint8_t, 2>, ApplyS<uint16_t,2>, ApplyS<PtexHalf,2>, ApplyS<float,   2>,
    ApplyS<uint8_t, 3>, ApplyS<uint16_t,3>, ApplyS<PtexHalf,3>, ApplyS<float,   3>,
    ApplyS<uint8_t, 4>, ApplyS<uint16_t,4>, ApplyS<PtexHalf,4>, ApplyS<float,   4>,
};


PtexSeparableKernel::ApplyConstFn
PtexSeparableKernel::applyConstFunctions[] = {
    ApplyConstN<uint8_t>, ApplyConstN<uint16_t>, ApplyConstN<PtexHalf>, ApplyConstN<float>,
    ApplyConst<uint8_t, 1>, ApplyConst<uint16_t,1>, ApplyConst<PtexHalf,1>, ApplyConst<float,   1>,
    ApplyConst<uint8_t, 2>, ApplyConst<uint16_t,2>, ApplyConst<PtexHalf,2>, ApplyConst<float,   2>,
    ApplyConst<uint8_t, 3>, ApplyConst<uint16_t,3>, ApplyConst<PtexHalf,3>, ApplyConst<float,   3>,
    ApplyConst<uint8_t, 4>, ApplyConst<uint16_t,4>, ApplyConst<PtexHalf,4>, ApplyConst<float,   4>,
};
