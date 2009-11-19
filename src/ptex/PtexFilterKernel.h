#ifndef PtexFilterKernel_h
#define PtexFilterKernel_h

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

#include "Ptexture.h"

class PtexFilterContext;

// Filter convolution kernel
class PtexFilterKernel : public Ptex {
public:
    PtexFilterKernel() : valid(false) {}
    void set(Res resval, int uval, int vval, int uwval, int vwval,
	     double* startval, int strideval)
    {
	valid = true;
	eid = e_bottom;
	res = resval;
	u = uval; v = vval;
	uw = uwval; vw = vwval;
	start = startval;
	stride = strideval;
    }

    operator bool() const { return valid; }
    void clear() { valid = false; }
    EdgeId eidval() const { return eid; }
    void split(PtexFilterKernel& ku, PtexFilterKernel& kv, PtexFilterKernel& kc);
    void apply(int faceid, int rotateval, const PtexFilterContext& c) const;
    static void applyConst(void* data, const PtexFilterContext& c, double weight);

    void merge(PtexFilterKernel& k, EdgeId eid, float weight=1.0)
    {
	k.valid = 0;

	// merge weights of given kernel into this kernel along edge k.eid
	double* dst; // start of destination edge
	int du, dv;  // offset to next pixel in u/v along edge
	switch (eid) {
	default:
	case e_bottom: dst = start; du = 1; dv = -uw; break;
	case e_right:  dst = start + uw-1; du = 0; dv = stride; break;
	case e_top:    dst = start + (vw-1)*stride; du = 1; dv = -uw; break;
	case e_left:   dst = start; du = 0; dv = stride; break;
	}
    
	double* kp = k.start;
	int rowskip = k.stride - k.uw;
	for (int i = 0; i < k.vw; i++, dst += dv, kp += rowskip)
	    for (int j = 0; j < k.uw; j++, dst += du) *dst += weight * *kp++;
    }

    double totalWeight() const
    {
	double* pos = start;
	double w = 0;
	int rowskip = stride-uw;
	for (int i = 0; i < vw; i++, pos += rowskip)
	    for (int j = 0; j < uw; j++) w += *pos++;
	return w;
    }

    class Iter; friend class Iter;
    class TileIter; friend class TileIter;

private:
    void splitL(PtexFilterKernel& ku)
    {
	// u < 0
	int w = -u;
	ku.set(res, res.u()-w, v, w,vw, start, stride);
	ku.eid = e_left;
	start += w; u = 0; uw -= w;
    }
    void splitR(PtexFilterKernel& ku)
    {
	// u + uw > res.u()
	int w = res.u()-u;
	ku.set(res, 0, v, uw-w, vw, start+w, stride);
	ku.eid = e_right;
	uw = w;
    }
    void splitB(PtexFilterKernel& kv)
    {
	// v < 0
	int w = -v;
	kv.set(res, u, res.v()-w, uw, w, start, stride);
	kv.eid = e_bottom;
	start += w*stride; v = 0; vw -= w;
    }
    void splitT(PtexFilterKernel& kv)
    {
	// v + vw > res.v()
	int w = res.v()-v;
	kv.set(res, u, 0, uw, vw-w, start+w*stride, stride);
	kv.eid = e_top;
	vw = w;
    }

    bool valid;			// true if kernel is valid (non-empty)
    EdgeId eid;			// edge leading towards kernel face from parent
    Res res;			// resolution that kernel was built for
    int u, v;			// uv location of first pixel in kernel
    int uw, vw;			// kernel width in pixels
    double* start;		// kernel start (may point into parent kernel)
    int stride;  		// distance between rows in kernel
};


#endif
