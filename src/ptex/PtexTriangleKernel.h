#ifndef PtexTriangleKernel_h
#define PtexTriangleKernel_h

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

#include <assert.h>
#include <algorithm>
#include <numeric>
#include "Ptexture.h"
#include "PtexUtils.h"

PTEX_NAMESPACE_BEGIN

// kernel width as a multiple of filter width (should be between 3 and 4)
// for values below 3, the gaussian is not close to zero and a contour will be formed
// larger values are more expensive (proportional to width-squared)
const float PtexTriangleKernelWidth = 3.5f;


/// Triangle filter kernel iterator (in texel coords)
class PtexTriangleKernelIter {
 public:
    int rowlen;                 // row length (in u)
    float u, v;         // uv center in texels
    int u1, v1, w1;             // uvw lower bounds
    int u2, v2, w2;             // uvw upper bounds
    float A,B,C;                // ellipse coefficients (F = 1)
    bool valid;                 // footprint is valid (non-empty)
    float wscale;               // amount to scale weights by (proportional to texel area)
    float weight;               // accumulated weight

    void apply(float* dst, void* data, DataType dt, int nChan, int nTxChan)
    {
        // dispatch specialized apply function
        ApplyFn fn = applyFunctions[(nChan!=nTxChan)*20 + ((unsigned)nChan<=4)*nChan*4 + dt];
        fn(*this, dst, data, nChan, nTxChan);
    }

    void applyConst(float* dst, void* data, DataType dt, int nChan);

 private:

    typedef void (*ApplyFn)(PtexTriangleKernelIter& k, float* dst, void* data, int nChan, int nTxChan);
    static ApplyFn applyFunctions[40];
};


/// Triangle filter kernel (in normalized triangle coords)
class PtexTriangleKernel {
 public:
    Res res;                    // desired resolution
    float u, v;         // uv filter center
    float u1, v1, w1;           // uvw lower bounds
    float u2, v2, w2;           // uvw upper bounds
    float A,B,C;                // ellipse coefficients (F = A*C-B*B/4)

    void set(Res resVal, float uVal, float vVal,
             float u1Val, float v1Val, float w1Val,
             float u2Val, float v2Val, float w2Val,
             float AVal, float BVal, float CVal)
    {
        res = resVal;
        u = uVal; v = vVal;
        u1 = u1Val; v1 = v1Val; w1 = w1Val;
        u2 = u2Val; v2 = v2Val; w2 = w2Val;
        A = AVal; B = BVal; C = CVal;
    }

    void set(float uVal, float vVal,
             float u1Val, float v1Val, float w1Val,
             float u2Val, float v2Val, float w2Val)
    {
        u = uVal; v = vVal;
        u1 = u1Val; v1 = v1Val; w1 = w1Val;
        u2 = u2Val; v2 = v2Val; w2 = w2Val;
    }

    void setABC(float AVal, float BVal, float CVal)
    {
        A = AVal; B = BVal; C = CVal;
    }

    void splitU(PtexTriangleKernel& ka)
    {
        ka = *this;
        u1 = 0;
        ka.u2 = 0;
    }

    void splitV(PtexTriangleKernel& ka)
    {
        ka = *this;
        v1 = 0;
        ka.v2 = 0;
    }

    void splitW(PtexTriangleKernel& ka)
    {
        ka = *this;
        w1 = 0;
        ka.w2 = 0;
    }

    void rotate1()
    {
        // rotate ellipse where u'=w, v'=u, w'=v
        // (derived by converting to Barycentric form, rotating, and converting back)
        setABC(C, 2.0f*C-B, A+C-B);
    }

    void rotate2()
    {
        // rotate ellipse where u'=v, v'=w, w'=u
        // (derived by converting to Barycentric form, rotating, and converting back)
        setABC(A+C-B, 2.0f*A-B, A);
    }

    void reorient(int eid, int aeid)
    {
        float w = 1.0f-u-v;

#define C(eid, aeid) (eid*3 + aeid)
        switch (C(eid, aeid)) {
        case C(0, 0): set(1.0f-u, -v, 1.0f-u2, -v2, 1.0f-w2, 1.0f-u1, -v1, 1.0f-w1); break;
        case C(0, 1): set(1.0f-w, 1.0f-u, 1.0f-w2, 1.0f-u2, -v2, 1.0f-w1, 1.0f-u1, -v1); rotate1(); break;
        case C(0, 2): set( -v, 1.0f-w, -v2, 1.0f-w2, 1.0f-u2, -v1, 1.0f-w1, 1.0f-u1); rotate2(); break;

        case C(1, 0): set(1.0f-v, -w, 1.0f-v2, -w2, 1.0f-u2, 1.0f-v1, -w1, 1.0f-u1); rotate2(); break;
        case C(1, 1): set(1.0f-u, 1.0f-v, 1.0f-u2, 1.0f-v2, -w2, 1.0f-u1, 1.0f-v1, -w1); break;
        case C(1, 2): set( -w, 1.0f-u, -w2, 1.0f-u2, 1.0f-v2, -w1, 1.0f-u1, 1.0f-v1); rotate1(); break;

        case C(2, 0): set(1.0f-w, -u, 1.0f-w2, -u2, 1.0f-v2, 1.0f-w1, -u1, 1.0f-v1); rotate1(); break;
        case C(2, 1): set(1.0f-v, 1.0f-w, 1.0f-v2, 1.0f-w2, -u2, 1.0f-v1, 1.0f-w1, -u1); rotate2(); break;
        case C(2, 2): set( -u, 1.0f-v, -u2, 1.0f-v2, 1.0f-w2, -u1, 1.0f-v1, 1.0f-w1); break;
#undef C
        }
    }

    void clampRes(Res fres)
    {
        res.ulog2 = PtexUtils::min(res.ulog2, fres.ulog2);
        res.vlog2 = res.ulog2;
    }

    void clampExtent()
    {
        u1 = PtexUtils::max(u1, 0.0f);
        v1 = PtexUtils::max(v1, 0.0f);
        w1 = PtexUtils::max(w1, 0.0f);
        u2 = PtexUtils::min(u2, 1.0f-(v1+w1));
        v2 = PtexUtils::min(v2, 1.0f-(w1+u1));
        w2 = PtexUtils::min(w2, 1.0f-(u1+v1));
    }

    void getIterators(PtexTriangleKernelIter& ke, PtexTriangleKernelIter& ko)
    {
        int resu = res.u();

        // normalize coefficients for texel units
        float Finv = 1.0f/((float)resu*(float)resu*(A*C - 0.25f * B * B));
        float Ak = A*Finv, Bk = B*Finv, Ck = C*Finv;

        // build even iterator
        ke.rowlen = resu;
        ke.wscale = 1.0f/((float)resu*(float)resu);
        float scale = (float)ke.rowlen;
        ke.u = u * scale - float(1/3.0);
        ke.v = v * scale - float(1/3.0);
        ke.u1 = int(PtexUtils::ceil(u1 * scale - float(1/3.0)));
        ke.v1 = int(PtexUtils::ceil(v1 * scale - float(1/3.0)));
        ke.w1 = int(PtexUtils::ceil(w1 * scale - float(1/3.0)));
        ke.u2 = int(PtexUtils::ceil(u2 * scale - float(1/3.0)));
        ke.v2 = int(PtexUtils::ceil(v2 * scale - float(1/3.0)));
        ke.w2 = int(PtexUtils::ceil(w2 * scale - float(1/3.0)));
        ke.A = Ak; ke.B = Bk; ke.C = Ck;
        ke.valid = (ke.u2 > ke.u1 && ke.v2 > ke.v1 && ke.w2 > ke.w1);
        ke.weight = 0;

        // build odd iterator: flip kernel across diagonal (u = 1-v, v = 1-u, w = -w)
        ko.rowlen = ke.rowlen;
        ko.wscale = ke.wscale;
        ko.u = (1.0f-v) * scale - float(1/3.0);
        ko.v = (1.0f-u) * scale - float(1/3.0);
        ko.u1 = int(PtexUtils::ceil((1.0f-v2) * scale - float(1/3.0)));
        ko.v1 = int(PtexUtils::ceil((1.0f-u2) * scale - float(1/3.0)));
        ko.w1 = int(PtexUtils::ceil((-w2) * scale - float(1/3.0)));
        ko.u2 = int(PtexUtils::ceil((1.0f-v1) * scale - float(1/3.0)));
        ko.v2 = int(PtexUtils::ceil((1.0f-u1) * scale - float(1/3.0)));
        ko.w2 = int(PtexUtils::ceil((-w1) * scale - float(1/3.0)));
        ko.A = Ck; ko.B = Bk; ko.C = Ak;
        ko.valid = (ko.u2 > ko.u1 && ko.v2 > ko.v1 && ko.w2 > ko.w1);
        ko.weight = 0;
    }
};

PTEX_NAMESPACE_END

#endif
