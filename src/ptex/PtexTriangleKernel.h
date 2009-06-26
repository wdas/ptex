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

// Triangle convolution kernel
class PtexTriangleKernel : public Ptex {
 public:
    Res res;			// resolution that kernel was built for
    double u, v;		// u, v, w filter center in texels
    int u1, v1, w1;		// uvw lower bounds
    int u2, v2, w2;		// uvw upper bounds
    double A,B,C;		// ellipse coefficients
    double _weight;		// accumulated weight
    
    void set(Res resVal, double uVal, double vVal,
	     int u1Val, int v1Val, int w1Val,
	     int u2Val, int v2Val, int w2Val,
	     double AVal, double BVal, double CVal)
    {
	res = resVal;
	u = uVal; v = vVal;
	u1 = u1Val; v1 = v1Val; w1 = w1Val;
	u2 = u2Val; v2 = v2Val; w2 = w2Val;
	A = AVal; B = BVal; C = CVal;
	assert(u2 > u1 && v2 > v1 && w2 > w1);
	_weight = 0;
    }

    void splitU(PtexTriangleKernel& k)
    {
	k = *this;
	
    }
    void splitV(PtexTriangleKernel& /*k*/) {}
    void splitW(PtexTriangleKernel& /*k*/) {}
    void reorient(int /*eid*/, int /*aeid*/, Res /*ares*/) {}
    double weight();
    void downres(Res /*fres*/) {}

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

    typedef void (*ApplyFn)(PtexTriangleKernel& k, double* dst, void* data, int nChan, int nTxChan);
    static ApplyFn applyFunctions[40];
};

#endif
