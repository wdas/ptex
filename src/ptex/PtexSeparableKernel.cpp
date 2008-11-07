#include <alloca.h>
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


    template<class T, int nChan>
    void Apply(PtexSeparableKernel& k, double* result, void* data, int /*nChan*/, int nTxChan)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.ures * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.ures + k.u) * nTxChan;
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

    template<class T>
    void ApplyN(PtexSeparableKernel& k, double* result, void* data, int nChan, int nTxChan)
    {
	double* rowResult = (double*) alloca(nChan*sizeof(double));
	int rowlen = k.ures * nTxChan;
	int datalen = k.uw * nTxChan;
	int rowskip = rowlen - datalen;
	double* kvp = k.kv;
	T* p = (T*)data + (k.v * k.ures + k.u) * nTxChan;
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

}



PtexSeparableKernel::ApplyFn
PtexSeparableKernel::getApplyFn(Ptex::DataType dt, int nChan)
{
    if (nChan > 4) nChan = 0;
    switch(nChan<<2|dt) {
    case (0<<2|Ptex::dt_uint8):  return ApplyN<uint8_t>;
    case (0<<2|Ptex::dt_uint16): return ApplyN<uint16_t>;
    case (0<<2|Ptex::dt_half):   return ApplyN<PtexHalf>;
    case (0<<2|Ptex::dt_float):  return ApplyN<float>;
    case (1<<2|Ptex::dt_uint8):  return Apply<uint8_t, 1>;
    case (1<<2|Ptex::dt_uint16): return Apply<uint16_t,1>;
    case (1<<2|Ptex::dt_half):   return Apply<PtexHalf,1>;
    case (1<<2|Ptex::dt_float):  return Apply<float,   1>;
    case (2<<2|Ptex::dt_uint8):  return Apply<uint8_t, 2>;
    case (2<<2|Ptex::dt_uint16): return Apply<uint16_t,2>;
    case (2<<2|Ptex::dt_half):   return Apply<PtexHalf,2>;
    case (2<<2|Ptex::dt_float):  return Apply<float,   2>;
    case (3<<2|Ptex::dt_uint8):  return Apply<uint8_t, 3>;
    case (3<<2|Ptex::dt_uint16): return Apply<uint16_t,3>;
    case (3<<2|Ptex::dt_half):   return Apply<PtexHalf,3>;
    case (3<<2|Ptex::dt_float):  return Apply<float,   3>;
    case (4<<2|Ptex::dt_uint8):  return Apply<uint8_t, 4>;
    case (4<<2|Ptex::dt_uint16): return Apply<uint16_t,4>;
    case (4<<2|Ptex::dt_half):   return Apply<PtexHalf,4>;
    case (4<<2|Ptex::dt_float):  return Apply<float,   4>;
    }
    return 0;
}

