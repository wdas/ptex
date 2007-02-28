#ifndef PtexFilterKernel_h
#define PtexFilterKernel_h

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
    EdgeId eidval() const { return eid; }
    void split(PtexFilterKernel& ku, PtexFilterKernel& kv, PtexFilterKernel& kc);
    void merge(PtexFilterKernel& k, EdgeId eid, bool extrapolate);
    void apply(int faceid, int rotateval, const PtexFilterContext& c) const;
    static void applyConst(void* data, const PtexFilterContext& c, double weight);

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
    double totalWeight() const
    {
	double* pos = start;
	double w = 0;
	int rowskip = stride-uw;
	for (int i = 0; i < vw; i++, pos += rowskip)
	    for (int j = 0; j < uw; j++) w += *pos++;
	return w;
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
