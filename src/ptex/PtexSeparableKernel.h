#ifndef PtexSeparableKernel_h
#define PtexSeparableKernel_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include <algorithm>
#include <numeric>
#include "Ptexture.h"
#include "PtexUtils.h"

// Separable convolution kernel
class PtexSeparableKernel : public Ptex {
public:
    int ures, vres;		// resolution that kernel was built for
    int u, v;			// uv offset within face data
    int uw, vw;			// kernel width
    double* ku;			// kernel weights in u
    double* kv;			// kernel weights in v

    void set(int uresVal, int vresVal,
	     int uVal, int vVal,
	     int uwVal, int vwVal,
	     double* kuVal, double* kvVal)
    {
	ures = uresVal;
	vres = vresVal;
	u = uVal;
	v = vVal;
	uw = uwVal;
	vw = vwVal;
	ku =  kuVal;
	kv =  kvVal;
    }

    void reallocU(double*& buffer)
    {
	memcpy(buffer, ku, uw*sizeof(double));
	ku = buffer; buffer += uw;
    }

    void reallocV(double*& buffer)
    {
	memcpy(buffer, kv, vw*sizeof(double));
	kv = buffer; buffer += vw;
    }

    void realloc(double*& buffer)
    {
	reallocU(buffer);
	reallocV(buffer);
    }

    double weight() const
    {
	return accumulate(ku, uw) * accumulate(kv, vw);
    }

    void mergeL()
    {
	int w = -u;
	ku[w] += accumulate(ku, w);
	ku += w;
	uw -= w;
    }

    void mergeR()
    {
	int w = uw + u - ures;
	double* kp = ku + uw - w;
	kp[-1] += accumulate(kp, w);
	uw -= w;
    }

    void mergeB()
    {
	int w = -v;
	kv[w] += accumulate(kv, w);
	vw -= w;
    }

    void mergeT()
    {
	int w = vw + v - vres;
	double* kp = kv + vw - w;
	kp[-1] += accumulate(kp, w);
	vw -= w;
    }

    void splitL(PtexSeparableKernel& k)
    {
	int w = -u; // width of split portion
	
	// split off left piece
	k = *this;
	k.u = ures - w;
	k.uw = w;

	// update local
	u = 0;
	uw -= w;
	ku += w;
    }

    void splitR(PtexSeparableKernel& k)
    {
	int w = u + uw - ures; // width of split portion
	
	// split off right piece
	k = *this;
	k.u = 0;
	k.uw = w;
	k.ku += w;

	// update local
	uw -= w;
    }

    void splitB(PtexSeparableKernel& k)
    {
	int w = -v; // width of split portion
	
	// split off left piece
	k = *this;
	k.v = vres - w;
	k.vw = w;

	// update local
	v = 0;
	vw -= w;
	kv += w;
    }

    void splitT(PtexSeparableKernel& k)
    {
	int w = v + vw - vres; // width of split portion
	
	// split off right piece
	k = *this;
	k.v = 0;
	k.vw = w;
	k.kv += w;

	// update local
	vw -= w;
    }

    void flipU()
    {
	u = ures - u - uw;
	std::reverse(ku, ku+uw);
    }

    void flipV()
    {
	v = vres - v - vw;
	std::reverse(kv, kv+vw);
    }

    void swapUV()
    {
	std::swap(ures, vres);
	std::swap(u, v);
	std::swap(uw, vw);
	std::swap(ku, kv);
    }

    void rotate(int rot)
    {
	// rotate kernel 'rot' steps ccw
	switch (rot & 3) {
	default: return;
	case 1: flipU(); swapUV(); break;
	case 2: flipU(); flipV(); break;
	case 3: flipV(); swapUV(); break;
	}
    }

    void downresU()
    {
	double* src = ku;
	double* dst = ku;

	// copy odd leading sample (if any)
	if (u & 1) {
	    *dst++ = *src++;
	    uw--;
	}

	// average even pairs
	for (int i = uw/2; i > 0; i--) {
	    *dst++ = (src[0] + src[1]) * 0.5;
	    src += 2;
	}

	// copy odd trailing sample (if any)
	if (uw & 1) {
	    *dst++ = *src++;
	}
	
	// update state
	u /= 2;
	uw = dst - ku;
	ures /= 2;
    }

    void downresV()
    {
	double* src = kv;
	double* dst = kv;

	// copy odd leading sample (if any)
	if (v & 1) {
	    *dst++ = *src++;
	    vw--;
	}

	// average even pairs
	for (int i = vw/2; i > 0; i--) {
	    *dst++ = (src[0] + src[1]) * 0.5;
	    src += 2;
	}

	// copy odd trailing sample (if any)
	if (vw & 1) {
	    *dst++ = *src++;
	}
	
	// update state
	v /= 2;
	vw = dst - kv;
	vres /= 2;
    }

    void apply(double* dst, void* data, DataType dt, int nChan, int nTxChan)
    {
	// dispatch specialized apply function
	ApplyFn fn = applyFunctions[(nChan!=nTxChan)*20 + ((unsigned)nChan<=4)*nChan*4 + dt];
	fn(*this, dst, data, nChan, nTxChan);
    }

    void applyConst(double* dst, void* data, DataType dt, int nChan)
    {
	// dispatch specialized apply function
	ApplyConstFn fn = applyConstFunctions[((unsigned)nChan<=4)*nChan*4 + dt];
	fn(weight(), dst, data, nChan);
    }

private:
    typedef void (*ApplyFn)(PtexSeparableKernel& k, double* dst, void* data, int nChan, int nTxChan);
    typedef void (*ApplyConstFn)(double weight, double* dst, void* data, int nChan);
    static ApplyFn applyFunctions[40];
    static ApplyConstFn applyConstFunctions[20];
    static inline double accumulate(const double* p, int n)
    {
	double result = 0;
	for (const double* e = p + n; p != e; p++) result += *p;
	return result;
    }
};

#endif
