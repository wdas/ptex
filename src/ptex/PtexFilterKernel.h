#ifndef PtexFilterKernel_h
#define PtexFilterKernel_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
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
