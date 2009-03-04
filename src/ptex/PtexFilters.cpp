/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include "PtexPlatform.h"
#include "Ptexture.h"
#include "PtexMitchellFilter.h"
#include "PtexSeparableFilter.h"
#include "PtexSeparableKernel.h"

class PtexPointFilter : public PtexFilter, public Ptex
{
 public:
    PtexPointFilter(PtexTexture* tx) : _tx(tx) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      int faceid, float u, float v, float /*uw*/, float /*vw*/)
    {
	if (!_tx || nchannels <= 0) return;
	if (faceid < 0 || faceid >= _tx->numFaces()) return;
	const FaceInfo& f = _tx->getFaceInfo(faceid);
	int resu = f.res.u(), resv = f.res.v();
	int ui = PtexUtils::clamp(int(u*resu), 0, resu-1);
	int vi = PtexUtils::clamp(int(v*resv), 0, resv-1);
	_tx->getPixel(faceid, ui, vi, result, firstchan, nchannels);
    }
    
 private:
    PtexTexture* _tx;
};


class PtexPointFilterTri : public PtexFilter, public Ptex
{
 public:
    PtexPointFilterTri(PtexTexture* tx) : _tx(tx) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      int faceid, float u, float v, float /*uw*/, float /*vw*/)
    {
	if (!_tx || nchannels <= 0) return;
	if (faceid < 0 || faceid >= _tx->numFaces()) return;
	const FaceInfo& f = _tx->getFaceInfo(faceid);
	int res = f.res.u();
	int resm1 = res - 1;
	float ut = u * res, vt = v * res;
	int ui = PtexUtils::clamp(int(ut), 0, resm1);
	int vi = PtexUtils::clamp(int(vt), 0, resm1);
	float uf = ut - floor(ui), vf = vt - floor(vi);
	
	if (uf + vf <= 1.0) {
	    // "even" triangles are stored in lower-left half-texture
	    _tx->getPixel(faceid, ui, vi, result, firstchan, nchannels);
	}
	else {
	    // "odd" triangles are stored in upper-right half-texture
	    _tx->getPixel(faceid, resm1-vi, resm1-ui, result, firstchan, nchannels);
	}
    }
    
 private:
    PtexTexture* _tx;
};


class PtexBicubicFilter : public PtexSeparableFilter
{
 public:
    PtexBicubicFilter(PtexTexture* tx, float sharpness)
	: PtexSeparableFilter(tx)
    {
	// compute Cubic filter coefficients:
	// abs(x) < 1:
	//   1/6 * ((12 - 9*B - 6*C)*x^3 + (-18 + 12*B + 6*C)*x^2 + (6 - 2*B))
	//   == c[0]*x^3 + c[1]*x^2 + c[2]
	// abs(x) < 2:
	//   1/6 * ((-B - 6*C)*x^3 + (6*B + 30*C)*x^2 + (-12*B - 48*C)*x + (8*B + 24*C))
	//   == c[3]*x^3 + c[4]*x^2 + c[5]*x + c[6]
	// else: 0

	float B = 1 - sharpness; // choose C = (1-B)/2
	_filter[0] = 1.5 - B;
	_filter[1] = 1.5 * B - 2.5;
	_filter[2] = 1 - (1./3) * B;
	_filter[3] = (1./3) * B - 0.5;
	_filter[4] = 2.5 - 1.5 * B;
	_filter[5] = 2 * B - 4;
	_filter[6] = 2 - (2./3) * B;
    }

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes, bool isSubface)
    {
	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(faceRes.u()));
	vw = PtexUtils::max(vw, 1.0f/(faceRes.v()));

	// clamp filter width to no larger than 0.25
	float minw = isSubface ? .25f : .125f;
	uw = PtexUtils::min(uw, minw);
	vw = PtexUtils::min(vw, minw);

	// compute desired texture res based on filter width
	int ureslog2 = int(ceil(log2(1.0/uw)));
	int vreslog2 = int(ceil(log2(1.0/vw)));

	Res res(ureslog2, vreslog2);
	k.res = res;
	
	// convert from normalized coords to pixel coords
	double upix = u * k.res.u() - 0.5;
	double vpix = v * k.res.v() - 0.5;
	double uwpix = uw * k.res.u();
	double vwpix = vw * k.res.v();

	// find integer pixel extent: [u,v] +/- [2*uw,2*vw]
	// (mitchell is 4 units wide for a 1 unit filter period)
	int u1 = int(ceil(upix - 2*uwpix)), u2 = int(ceil(upix + 2*uwpix));
	int v1 = int(ceil(vpix - 2*vwpix)), v2 = int(ceil(vpix + 2*vwpix));
	k.u = u1;
	k.v = v1;
	k.uw = u2-u1;
	k.vw = v2-v1;

	// compute kernel weights along u and v directions
	computeWeights(k.ku, (u1-upix)/uwpix, 1.0/uwpix, k.uw);
	computeWeights(k.kv, (v1-vpix)/vwpix, 1.0/vwpix, k.vw);
    }

 private:
    double k(double x)
    {
	const double* c = _filter;
	x = fabs(x);
	if (x < 1)      return (c[0]*x + c[1])*x*x + c[2];
	else if (x < 2) return ((c[3]*x + c[4])*x + c[5])*x + c[6];
	else            return 0;
    }

    void computeWeights(double* kernel, double x1, double step, int size)
    {
	for (int i = 0; i < size; i++) kernel[i] = k(x1 + i*step);
    }

    double _filter[7]; // filter coefficients for current sharpness
};



class PtexGaussianFilter : public PtexSeparableFilter
{
 public:
    PtexGaussianFilter(PtexTexture* tx)
	: PtexSeparableFilter(tx) {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes, bool isSubface)
    {
	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(faceRes.u()));
	vw = PtexUtils::max(vw, 1.0f/(faceRes.v()));

	// clamp filter width to no larger than 0.25
	float minw = isSubface ? .25f : .125f;
	uw = PtexUtils::min(uw, minw);
	vw = PtexUtils::min(vw, minw);

	// compute desired texture res based on filter width
	int ureslog2 = int(ceil(log2(1.0/uw)));
	int vreslog2 = int(ceil(log2(1.0/vw)));
	Res res(ureslog2, vreslog2);
	k.res = res;
	
	// convert from normalized coords to pixel coords
	double upix = u * k.res.u() - 0.5;
	double vpix = v * k.res.v() - 0.5;
	double uwpix = uw * k.res.u();
	double vwpix = vw * k.res.v();

	// find integer pixel extent: [u,v] +/- [2*uw,2*vw]
	// (gaussian is 4 units wide for a 1 unit filter period)
	int u1 = int(ceil(upix - 2*uwpix)), u2 = int(ceil(upix + 2*uwpix));
	int v1 = int(ceil(vpix - 2*vwpix)), v2 = int(ceil(vpix + 2*vwpix));
	k.u = u1;
	k.v = v1;
	k.uw = u2-u1;
	k.vw = v2-v1;

	// compute kernel weights along u and v directions
	computeWeights(k.ku, (u1-upix)/uwpix, 1.0/uwpix, k.uw);
	computeWeights(k.kv, (v1-vpix)/vwpix, 1.0/vwpix, k.vw);
    }

 private:
    double k(double x)
    {
	return exp(-2*x*x);
    }

    void computeWeights(double* kernel, double x1, double step, int size)
    {
	for (int i = 0; i < size; i++) kernel[i] = k(x1 + i*step);
    }
};



class PtexBoxFilter : public PtexSeparableFilter
{
 public:
    PtexBoxFilter(PtexTexture* tx)
	: PtexSeparableFilter(tx) {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes, bool /*isSubface*/)
    {
	// clamp filter width to no larger than 1.0
	uw = PtexUtils::min(uw, 1.0f);
	vw = PtexUtils::min(vw, 1.0f);

	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(faceRes.u()));
	vw = PtexUtils::max(vw, 1.0f/(faceRes.v()));

	// compute desired texture res based on filter width
	int ureslog2 = int(ceil(log2(1.0/uw))),
	    vreslog2 = int(ceil(log2(1.0/vw)));
	Res res(ureslog2, vreslog2);
	k.res = res;
	
	// convert from normalized coords to pixel coords
	u = u * k.res.u();
	v = v * k.res.v();
	uw *= k.res.u();
	vw *= k.res.v();

	// find integer pixel extent: [u,v] +/- [uw/2,vw/2]
	// (box is 1 unit wide for a 1 unit filter period)
	double u1 = u - 0.5*uw, u2 = u + 0.5*uw;
	double v1 = v - 0.5*vw, v2 = v + 0.5*vw;
	double u1floor = floor(u1), u2ceil = ceil(u2);
	double v1floor = floor(v1), v2ceil = ceil(v2);
	k.u = int(u1floor);
	k.v = int(v1floor);
	k.uw = int(u2ceil)-k.u;
	k.vw = int(v2ceil)-k.v;

	// compute kernel weights along u and v directions
	computeWeights(k.ku, k.uw, 1-(u1-u1floor), 1-(u2ceil-u2));
	computeWeights(k.kv, k.vw, 1-(v1-v1floor), 1-(v2ceil-v2));
    }

 private:
    void computeWeights(double* kernel, int size, double f1, double f2)
    {
	assert(size >= 1 && size <= 3);

	if (size == 1) {
	    kernel[0] = f1 + f2 - 1;
	}
	else {
	    kernel[0] = f1;
	    for (int i = 1; i < size-1; i++) kernel[i] = 1.0;
	    kernel[size-1] = f2;
	}
    }
};


class PtexBilinearFilter : public PtexSeparableFilter
{
 public:
    PtexBilinearFilter(PtexTexture* tx)
	: PtexSeparableFilter(tx) {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes, bool /*isSubface*/)
    {
	// clamp filter width to no larger than 1.0
	uw = PtexUtils::min(uw, 1.0f);
	vw = PtexUtils::min(vw, 1.0f);

	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(faceRes.u()));
	vw = PtexUtils::max(vw, 1.0f/(faceRes.v()));

	// choose resolution closest to filter res
	// there are three choices of "closest" that come to mind:
	// 1) closest in terms of filter width, i.e. period of signal
	// 2) closest in terms of texel resolution, (1 / filter width), i.e. freq of signal
	// 3) closest in terms of resolution level (log2(1/filter width))
	// Choice (1) probably makes the most sense.  In log2 terms, that means you should
	// use the next higher level when the fractional part of the log2 res is > log2(1/.75),
	// and you should add 1-log2(4/3) to round up.
	const double roundWidth = 0.5849625007211563; // 1-log2(1/.75)
	int ureslog2 = int(log2(1.0/uw) + roundWidth);
	int vreslog2 = int(log2(1.0/vw) + roundWidth);
	Res res(ureslog2, vreslog2);
	k.res = res;
	
	// convert from normalized coords to pixel coords
	double upix = u * k.res.u() - 0.5;
	double vpix = v * k.res.v() - 0.5;

	float ufloor = floor(upix);
	float vfloor = floor(vpix);
	k.u = int(ufloor);
	k.v = int(vfloor);
	k.uw = 2;
	k.vw = 2;

	// compute kernel weights
	float ufrac = upix-ufloor, vfrac = vpix-vfloor;
	k.ku[0] = 1 - ufrac;
	k.ku[1] = ufrac;
	k.kv[0] = 1 - vfrac;
	k.kv[1] = vfrac;
    }
};


PtexFilter* PtexFilter::getFilter(PtexTexture* tex, const PtexFilter::Options& opts)
{

    switch (tex->meshType()) {
    case Ptex::mt_quad:
	switch (opts.filter) {
	case -1:	    return new PtexMitchellFilter(tex, opts.sharpness);
	case f_point:       return new PtexPointFilter(tex);
	case f_bilinear:    return new PtexBilinearFilter(tex);
	default:
	case f_box:         return new PtexBoxFilter(tex);
	case f_gaussian:    return new PtexGaussianFilter(tex);
	case f_bicubic:     return new PtexBicubicFilter(tex, opts.sharpness);
	case f_bspline:     return new PtexBicubicFilter(tex, 0.0);
	case f_catmullrom:  return new PtexBicubicFilter(tex, 1.0);
	case f_mitchell:    return new PtexBicubicFilter(tex, 2.0/3.0);
	}
	break;

    case Ptex::mt_triangle:
// 	switch (opts.filter) {
// 	case f_point:       return new PtexPointFilterTri(tex);
// 	}
	return new PtexPointFilterTri(tex);
    }
    return 0;
}
