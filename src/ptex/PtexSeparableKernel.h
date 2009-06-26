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

#include <assert.h>
#include <algorithm>
#include <numeric>
#include "Ptexture.h"
#include "PtexUtils.h"

// Separable convolution kernel
class PtexSeparableKernel : public Ptex {
 public:
    Res res;			// resolution that kernel was built for
    int u, v;			// uv offset within face data
    int uw, vw;			// kernel width
    double* ku;			// kernel weights in u
    double* kv;			// kernel weights in v
    static const int kmax = 10;	// max kernel width
    double kubuff[kmax];
    double kvbuff[kmax];
    
    PtexSeparableKernel()
	: res(0), u(0), v(0), uw(0), vw(0), ku(kubuff), kv(kvbuff) {}

    PtexSeparableKernel(const PtexSeparableKernel& k)
    {
	set(k.res, k.u, k.v, k.uw, k.vw, k.ku, k.kv);
    }
    
    PtexSeparableKernel& operator= (const PtexSeparableKernel& k)
    {
	set(k.res, k.u, k.v, k.uw, k.vw, k.ku, k.kv);
	return *this;
    }

    void set(Res resVal,
	     int uVal, int vVal,
	     int uwVal, int vwVal,
	     const double* kuVal, const double* kvVal)
    {
	assert(uwVal <= kmax && vwVal <= kmax);
	res = resVal;
	u = uVal;
	v = vVal;
	uw = uwVal;
	vw = vwVal;
	memcpy(kubuff, kuVal, sizeof(*ku)*uw);
	memcpy(kvbuff, kvVal, sizeof(*kv)*vw);
	ku = kubuff;
	kv = kvbuff;
    }

    void stripZeros()
    {
	while (ku[0] == 0) { ku++; u++; uw--; }
	while (ku[uw-1] == 0) { uw--; }
	while (kv[0] == 0) { kv++; v++; vw--; }
	while (kv[vw-1] == 0) { vw--; }
	assert(uw > 0 && vw > 0);
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
	u = 0;
    }

    void mergeR()
    {
	int w = uw + u - res.u();
	double* kp = ku + uw - w;
	kp[-1] += accumulate(kp, w);
	uw -= w;
    }

    void mergeB()
    {
	int w = -v;
	kv[w] += accumulate(kv, w);
	kv += w;
	vw -= w;
	v = 0;
    }

    void mergeT()
    {
	int w = vw + v - res.v();
	double* kp = kv + vw - w;
	kp[-1] += accumulate(kp, w);
	vw -= w;
    }

    void splitL(PtexSeparableKernel& k)
    {
	// split off left piece of width w into k
	int w = -u;

	if (w < uw) {
	    // normal case - split off a portion
	    //    res  u          v  uw vw  ku  kv
	    k.set(res, res.u()-w, v, w, vw, ku, kv);
	    
	    // update local
	    u = 0;
	    uw -= w;
	    ku += w;
	}
	else {
	    // entire kernel is split off
	    k = *this;
	    k.u += res.u();
	    u = 0; uw = 0;
	}
    }

    void splitR(PtexSeparableKernel& k)
    {
	// split off right piece of width w into k
	int w = u + uw - res.u();

	if (w < uw) {
	    // normal case - split off a portion
	    //    res  u  v  uw vw  ku           kv
	    k.set(res, 0, v, w, vw, ku + uw - w, kv);

	    // update local
	    uw -= w;
	}
	else {
	    // entire kernel is split off
	    k = *this;
	    k.u -= res.u();
	    u = 0; uw = 0;
	}
    }

    void splitB(PtexSeparableKernel& k)
    {
	// split off bottom piece of width w into k
	int w = -v;
	if (w < vw) {
	    // normal case - split off a portion
	    //    res  u  v          uw vw  ku  kv
	    k.set(res, u, res.v()-w, uw, w, ku, kv);

	    // update local
	    v = 0;
	    vw -= w;
	    kv += w;
	}
	else {
	    // entire kernel is split off
	    k = *this;
	    k.v += res.v();
	    v = 0; vw = 0;
	}
    }

    void splitT(PtexSeparableKernel& k)
    {
	// split off top piece of width w into k
	int w = v + vw - res.v();
	if (w < vw) {
	    // normal case - split off a portion
	    //    res  u  v  uw vw  ku  kv
	    k.set(res, u, 0, uw, w, ku, kv + vw - w);

	    // update local
	    vw -= w;
	}
	else {
	    // entire kernel is split off
	    k = *this;
	    k.v -= res.v();
	    v = 0; vw = 0;
	}
    }

    void flipU()
    {
	u = res.u() - u - uw;
	std::reverse(ku, ku+uw);
    }

    void flipV()
    {
	v = res.v() - v - vw;
	std::reverse(kv, kv+vw);
    }

    void swapUV()
    {
	res.swapuv();
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

    void adjustMainToSubface(int eid)
    {
	// to adjust the kernel for the subface, we must adjust the res down and offset the uv coords
	// however, if the res is already zero, we must upres the kernel first
	if (res.ulog2 == 0) upresU();
	if (res.vlog2 == 0) upresV();

	if (res.ulog2 > 0) res.ulog2--;
	if (res.vlog2 > 0) res.vlog2--;
	switch (eid&3) {
	case e_bottom: v -= res.v(); break;
	case e_right:  break;
	case e_top:    u -= res.u(); break;
	case e_left:   u -= res.u(); v -= res.v(); break;
	}
    }

    void adjustSubfaceToMain(int eid)
    {
	switch (eid&3) {
	case e_bottom: v += res.v(); break;
	case e_right:  break;
	case e_top:    u += res.u(); break;
	case e_left:   u += res.u(); v += res.v(); break;
	}
	res.ulog2++; res.vlog2++;
    }

    void downresU()
    {
	double* src = ku;
	double* dst = ku;

	// skip odd leading sample (if any)
	if (u & 1) {
	    dst++;
	    src++;
	    uw--;
	}

	// combine even pairs
	for (int i = uw/2; i > 0; i--) {
	    *dst++ = src[0] + src[1];
	    src += 2;
	}

	// copy odd trailing sample (if any)
	if (uw & 1) {
	    *dst++ = *src++;
	}
	
	// update state
	u /= 2;
	uw = int(dst - ku);
	res.ulog2--;
    }

    void downresV()
    {
	double* src = kv;
	double* dst = kv;

	// skip odd leading sample (if any)
	if (v & 1) {
	    dst++;
	    src++;
	    vw--;
	}

	// combine even pairs
	for (int i = vw/2; i > 0; i--) {
	    *dst++ = src[0] + src[1];
	    src += 2;
	}

	// copy odd trailing sample (if any)
	if (vw & 1) {
	    *dst++ = *src++;
	}
	
	// update state
	v /= 2;
	vw = int(dst - kv);
	res.vlog2--;
    }

    void upresU()
    {
	double* src = ku + uw-1;
	double* dst = ku + uw*2-2;
	for (int i = uw; i > 0; i--) {
	    dst[0] = dst[1] = *src-- / 2;
	    dst -=2;
	}
	uw *= 2;
	u *= 2;
	res.ulog2++;
    }

    void upresV()
    {
	double* src = kv + vw-1;
	double* dst = kv + vw*2-2;
	for (int i = vw; i > 0; i--) {
	    dst[0] = dst[1] = *src-- / 2;
	    dst -=2;
	}
	vw *= 2;
	v *= 2;
	res.vlog2++;
    }

    void makeSymmetric()
    {
	assert(u == 0 && v == 0);

	// downres higher-res dimension until equal
	if (res.ulog2 > res.vlog2) {
	    do { downresU(); } while(res.ulog2 > res.vlog2);
	}
	else if (res.vlog2 > res.ulog2) {
	    do { downresV(); } while (res.vlog2 > res.ulog2);
	}

	// check initial weight so we can preserve overall weight
	double initialWeight = weight();

	// truncate excess samples in longer dimension
	uw = vw = PtexUtils::min(uw, vw);

	// combine corresponding u and v samples
	double newWeight = 0;
	for (int i = 0; i < uw; i++) {
	    ku[i] += kv[i];
	    newWeight += ku[i];
	}

	// compensate for weight change by scaling v weights
	double scale = newWeight == 0 ? 1.0 : initialWeight / (newWeight * newWeight);
	for (int i = 0; i < uw; i++) kv[i] = ku[i] * scale;
    }

    void apply(double* dst, void* data, DataType dt, int nChan, int nTxChan)
    {
	// dispatch specialized apply function
	ApplyFn fn = applyFunctions[(nChan!=nTxChan)*20 + ((unsigned)nChan<=4)*nChan*4 + dt];
	fn(*this, dst, data, nChan, nTxChan);
    }

    void applyConst(double* dst, void* data, DataType dt, int nChan)
    {
	PtexUtils::applyConst(weight(), dst, data, dt, nChan);
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
