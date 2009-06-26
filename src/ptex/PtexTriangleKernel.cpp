#include "PtexPlatform.h"
#include "PtexUtils.h"
#include "PtexHalf.h"
#include "PtexTriangleKernel.h"


namespace {
    inline double gaussian(double x_squared)
    {
	return exp(-2*x_squared);
    }
}


double PtexTriangleKernel::weight()
{
    if (_weight > 0) return _weight;
    
    // loop over the texels and calculate the kernel weights as if we
    // were actually applying the filter

    int resu = res.u();
    double DDQ = 2*A;
    double U = u1 - u;
    for (int vi = v1; vi != v2; vi++) {
	double V = vi - v;
	double DQ = A*(2*U+1)+B*V;
	double Q = A*U*U + (B*U + C*V)*V;
	int rowlen = resu - vi;
	int x1 = PtexUtils::max(u1, rowlen-w2);
	int x2 = PtexUtils::min(u2, rowlen-w1);
	assert(x2 >= x1);
	for (int i = x1; i != x2; i++) {
	    if (Q < 1) {
		_weight += gaussian(Q);
	    }
	    Q += DQ;
	    DQ += DDQ;
	}
    }

    return _weight;
}


namespace {

#define DO_APPLY(VecAccum)					\
    {								\
	int resu = k.res.u();					\
	double DDQ = 2*k.A;					\
	for (int vi = k.v1; vi != k.v2; vi++) {			\
	    int rowlen = resu - vi;				\
	    int x1 = PtexUtils::max(k.u1, rowlen-k.w2);		\
	    int x2 = PtexUtils::min(k.u2, rowlen-k.w1);		\
	    assert(x2 >= x1);					\
	    double U = x1 - k.u;				\
	    double V = vi - k.v;				\
	    double DQ = k.A*(2*U+1)+k.B*V;			\
	    double Q = k.A*U*U + (k.B*U + k.C*V)*V;		\
	    T* p = (T*)data + (vi * resu + x1) * nTxChan;	\
	    T* pEnd = p + (x2-x1)*nTxChan;			\
	    for (; p != pEnd; p += nTxChan) {			\
		if (Q < 1) {					\
		    double weight = gaussian(Q);		\
		    k._weight += weight;			\
		    VecAccum;					\
		}						\
		Q += DQ;					\
		DQ += DDQ;					\
	    }							\
	}							\
    }

    // apply to 1..4 channels (unrolled channel loop) of packed data (nTxChan==nChan)
    // the ellipse equation, Q, is calculated via finite differences (Heckbert '89 pg 57)
    template<class T, int nChan>
    void Apply(PtexTriangleKernel& k, double* result, void* data, int /*nChan*/, int /*nTxChan*/)
    {
	const int nTxChan = nChan;
	DO_APPLY((PtexUtils::VecAccum<T,nChan>()(result, p, weight)));
    }

    // apply to 1..4 channels (unrolled channel loop) w/ pixel stride
    template<class T, int nChan>
    void ApplyS(PtexTriangleKernel& k, double* result, void* data, int /*nChan*/, int nTxChan)
    {
	DO_APPLY((PtexUtils::VecAccum<T,nChan>()(result, p, weight)));
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyN(PtexTriangleKernel& k, double* result, void* data, int nChan, int nTxChan)
    {
	DO_APPLY((PtexUtils::VecAccumN<T>()(result, p, nChan, weight)));
    }
}


PtexTriangleKernel::ApplyFn
PtexTriangleKernel::applyFunctions[] = {
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
