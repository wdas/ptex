#ifndef PtexTriangleKernel_h
#define PtexTriangleKernel_h

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

// kernel width as a multiple of filter width (should be between 3 and 4)
// for values below 3, the gaussian is not close to zero and a contour will be formed
// larger values are more expensive (proportional to width-squared)
static const float PtexTriangleKernelWidth = 3.5;


/// Triangle filter kernel iterator (in texel coords)
class PtexTriangleKernelIter : public Ptex {
 public:
    int rowlen;			// row length (in u)
    double u, v;		// uv center in texels
    int u1, v1, w1;		// uvw lower bounds
    int u2, v2, w2;		// uvw upper bounds
    double A,B,C;		// ellipse coefficients (F = 1)
    bool valid;			// footprint is valid (non-empty)
    double weight;		// accumulated weight

    void apply(double* dst, void* data, DataType dt, int nChan, int nTxChan)
    {
	// dispatch specialized apply function
	ApplyFn fn = applyFunctions[(nChan!=nTxChan)*20 + ((unsigned)nChan<=4)*nChan*4 + dt];
	fn(*this, dst, data, nChan, nTxChan);
    }

    void applyConst(double* dst, void* data, DataType dt, int nChan);

 private:

    typedef void (*ApplyFn)(PtexTriangleKernelIter& k, double* dst, void* data, int nChan, int nTxChan);
    static ApplyFn applyFunctions[40];
};


/// Triangle filter kernel (in normalized triangle coords)
class PtexTriangleKernel : public Ptex {
 public:
    Res res;			// desired resolution
    double u, v;		// uv filter center
    double u1, v1, w1;		// uvw lower bounds
    double u2, v2, w2;		// uvw upper bounds
    double A,B,C;		// ellipse coefficients (F = A*C-B*B/4)

    void set(Res resVal, double uVal, double vVal,
	     double u1Val, double v1Val, double w1Val,
	     double u2Val, double v2Val, double w2Val,
	     double AVal, double BVal, double CVal)
    {
	res = resVal;
	u = uVal; v = vVal;
	u1 = u1Val; v1 = v1Val; w1 = w1Val;
	u2 = u2Val; v2 = v2Val; w2 = w2Val;
	A = AVal; B = BVal; C = CVal;
    }

    void splitU(PtexTriangleKernel& ka)
    {
	ka = *this;
	u1 = 0;
	ka.u2 = 1;
    }

    void splitV(PtexTriangleKernel& ka)
    {
	ka = *this;
	v1 = 0;
	ka.v2 = 1;
    }

    void splitW(PtexTriangleKernel& ka)
    {
	ka = *this;
	w1 = 0;
	ka.w2 = 1;
    }

    void reorient(int /*eid*/, int /*aeid*/, Res /*ares*/)
    {
	// TODO
    }
    
    void clampRes(Res fres)
    {
	res.ulog2 = PtexUtils::min(res.ulog2, fres.ulog2);
	res.vlog2 = res.ulog2;
    }

    void clampExtent()
    {
	u1 = PtexUtils::max(u1, 0.0);
	v1 = PtexUtils::max(v1, 0.0);
	w1 = PtexUtils::max(w1, 0.0);
	u2 = PtexUtils::min(u2, 1-(v1+w1));
	v2 = PtexUtils::min(v2, 1-(w1+u1));
	w2 = PtexUtils::min(w2, 1-(u1+v1));
    }

    void getIterators(PtexTriangleKernelIter& ke, PtexTriangleKernelIter& ko)
    {
	int resu = res.u();

	// normalize coefficients for texel units
	double Finv = 1.0/(resu*resu*(A*C - 0.25 * B * B));
	double Ak = A*Finv, Bk = B*Finv, Ck = C*Finv;

	// build even iterator
	ke.rowlen = resu;
	double scale = ke.rowlen;
	ke.u = u * scale - 1/3.0;
	ke.v = v * scale - 1/3.0;
	ke.u1 = int(ceil(u1 * scale - 1/3.0));
	ke.v1 = int(ceil(v1 * scale - 1/3.0));
	ke.w1 = int(ceil(w1 * scale - 1/3.0));
	ke.u2 = int(ceil(u2 * scale - 1/3.0));
	ke.v2 = int(ceil(v2 * scale - 1/3.0));
	ke.w2 = int(ceil(w2 * scale - 1/3.0));
	ke.A = Ak; ke.B = Bk; ke.C = Ck;
	ke.valid = (ke.u2 > ke.u1 && ke.v2 > ke.v1 && ke.w2 > ke.w1);
	ke.weight = 0;

	// build odd iterator: flip kernel across diagonal (u = 1-v, v = 1-u, w = -w)
	ko.rowlen = resu;
	ko.u = (1-v) * scale - 1/3.0;
	ko.v = (1-u) * scale - 1/3.0;
	ko.u1 = int(ceil((1-v2) * scale - 1/3.0));
	ko.v1 = int(ceil((1-u2) * scale - 1/3.0));
	ko.w1 = int(ceil(( -w2) * scale - 1/3.0));
	ko.u2 = int(ceil((1-v1) * scale - 1/3.0));
	ko.v2 = int(ceil((1-u1) * scale - 1/3.0));
	ko.w2 = int(ceil(( -w1) * scale - 1/3.0));
	ko.A = Ck; ko.B = Bk; ko.C = Ak;
	ko.valid = (ko.u2 > ko.u1 && ko.v2 > ko.v1 && ko.w2 > ko.w1);
	ko.weight = 0;
    }
};

#endif
