#ifndef PtexMitchellFilter_h
#define PtexMitchellFilter_h
#include "Ptexture.h"
#include "PtexFilterContext.h"
#include "PtexFilterKernel.h"
#include <vector>

class PtexMitchellFilter : public PtexFilter, public Ptex
{
 public:
    PtexMitchellFilter(float sharpness)
	: _extrapolate(true)
    {
	setSharpness(sharpness);
    }
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      PtexTexture* tx, int faceid,
		      float u, float v, float uw, float vw);

 protected:
    struct Face {
	bool valid;		// true if face is valid
	bool blend;		// face needs blending due to insufficient res.
	int id;			// faceid
	int rotate;		// nsteps ccw face must be rotated to align w/ main
	Res res;		// resolution to filter face at
	operator bool() { return valid; }
	Face() : valid(0), blend(0) {}
	void set(int faceid, Res resval, int rotateval=0)
	{
	    blend = 0;
	    valid = 1;
	    id = faceid;
	    res = resval;
	    rotate = rotateval&3;
	    // odd rotations require u/v swap
	    if (rotateval&1) res.swapuv();
	}
	void clampres(Res resval)
	{
	    // clamp res to resval
	    res.clamp(resval);
	    // blend if res is not at least as high as resval
	    if (!(res >= resval)) blend = 1;
	}
	void clear() { valid=0; blend=0; }
    };


    void setSharpness(float sharpness);
    void getNeighborhood(const FaceInfo& f);
		 
    void computeWeights(double* kernel, double x1, double step, int size)
    {
	double m, x;
	const double* c = _filter;
	for (int i = 0; i < size; i++) {
	    x = fabs(x1 + i*step);
	    if (x < 1)      m = (c[0]*x + c[1])*x*x + c[2];
	    else if (x < 2) m = ((c[3]*x + c[4])*x + c[5])*x + c[6];
	    else m = 0;
	    kernel[i] = m;
	}
    }

    void evalFaces(Res res, double weight, float uw, float vw);
    void evalFaces(Res res, double weight) 
    {
	evalFaces(res, weight, 1.0/res.u(), 1.0/res.v()); 
    }

    PtexFilterContext _ctx;	// filter context

    bool _isConstant;		// true if neighborhood is constant
    float _ublend, _vblend;	// u,v edge blending weights (0=none, 1=full blend)
    Face _face, _uface, _vface;	// main face and neighboring faces
    Face _cface;		// corner face (only valid if just one face)
    std::vector<Face> _cfaces;	// all faces around corner

    float _sharpness;		// current filter sharpness
    double _filter[7];		// filter coefficients for current sharpness
    bool _extrapolate;		// extrapolate beyond filter edges
};

#endif
