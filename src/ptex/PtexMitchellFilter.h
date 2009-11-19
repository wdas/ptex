#ifndef PtexMitchellFilter_h
#define PtexMitchellFilter_h

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

#include <vector>
#include "math.h"
#include "Ptexture.h"
#include "PtexFilterContext.h"
#include "PtexFilterKernel.h"

class PtexMitchellFilter : public PtexFilter, public Ptex
{
 public:
    PtexMitchellFilter(PtexTexture* tx, float sharpness)
	: _tx(tx)
    {
	setSharpness(sharpness);
    }
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      int faceid, float u, float v,
		      float uw1, float vw1, float uw2, float vw2,
		      float width, float blur);

 protected:
    struct Face {
	bool valid;		// true if face is valid
	bool blend;		// face needs blending due to insufficient res.
	int id;			// faceid
	int rotate;		// nsteps ccw face must be rotated to align w/ main
	Res res;		// resolution to filter face at (unrotated)
	operator bool() { return valid; }
	Face() : valid(0), blend(0) {}
	void set(int faceid, Res resval, int rotateval=0)
	{
	    blend = 0;
	    valid = 1;
	    id = faceid;
	    res = resval;
	    rotate = rotateval&3;
	    // swap u and v res if rotation is odd
	    if (rotateval&1) res.swapuv();
	}
	void clampres(Res resval)
	{
	    // clamp res to resval and set blend flag if < resval
	    if (res.ulog2 > resval.ulog2) res.ulog2 = resval.ulog2;
	    else if (res.ulog2 < resval.ulog2) blend = 1;
	    if (res.vlog2 > resval.vlog2) res.vlog2 = resval.vlog2;
	    else if (res.vlog2 < resval.vlog2) blend = 1;
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
	evalFaces(res, weight, 1.0f/res.u(), 1.0f/res.v()); 
    }
    void evalLargeDu(float du, float weight);
    void evalLargeDuFace(int faceid, int level, float weight);

    PtexTexture* _tx;		// the texture
    PtexFilterContext _ctx;	// filter context

    bool _isConstant;		// true if neighborhood is constant
    bool _interior;		// true if corner point is an interior point
    double _ublend, _vblend;	// u,v edge blending weights (0=none, 1=full blend)
    Face _face, _uface, _vface;	// main face and neighboring faces
    Face _cface;		// corner face (only valid if just one face)
    std::vector<Face> _cfaces;	// all faces around corner

    float _sharpness;		// current filter sharpness
    double _filter[7];		// filter coefficients for current sharpness
};

#endif
