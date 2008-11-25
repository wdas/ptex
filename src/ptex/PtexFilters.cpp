#include "Ptexture.h"
#include "PtexMitchellFilter.h"
#include "PtexSeparableFilter.h"
#include "PtexSeparableKernel.h"

class PtexSeparableMitchellFilter : public PtexSeparableFilter
{
 public:
    PtexSeparableMitchellFilter(float sharpness)
    {
	// compute Mitchell filter coefficients:
	// abs(x) < 1:
	//     1/6 * ((12 - 9*B - 6*C)*x^3 +
	// 	          (-18 + 12*B + 6*C)*x^2 +
	// 		  (6 - 2*B))
	//     == c[0]*x^3 + c[1]*x^2 + c[2]
	// abs(x) < 2:
	// 		 ((-B - 6*C)*x3 +
	// 		  (6*B + 30*C)*x2 +
	// 		  (-12*B - 48*C)*x +
	// 		  (8*B + 24*C)) :
	//     == c[3]*x^3 + c[4]*x^2 + c[5]*x + c[6]
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
			     Res faceRes)
    {
	// clamp filter width to no larger than 0.25
	uw = PtexUtils::min(uw, 0.25f);
	vw = PtexUtils::min(vw, 0.25f);

	// clamp filter width to no smaller than a texel
	uw = PtexUtils::max(uw, 1.0f/(faceRes.u()));
	vw = PtexUtils::max(vw, 1.0f/(faceRes.v()));

	// compute desired texture res based on filter width
	int ureslog2 = int(ceil(log2(1.0/uw))),
	    vreslog2 = int(ceil(log2(1.0/vw)));
	Res res(ureslog2, vreslog2);
	k.res = res;
	
	// convert from normalized coords to pixel coords
	u = u * k.res.u() - 0.5;
	v = v * k.res.v() - 0.5;
	uw *= k.res.u();
	vw *= k.res.v();

	// find integer pixel extent: [u,v] +/- [2*uw,2*vw]
	// (mitchell is 4 units wide for a 1 unit filter period)
	int u1 = int(ceil(u - 2*uw)), u2 = int(ceil(u + 2*uw));
	int v1 = int(ceil(v - 2*vw)), v2 = int(ceil(v + 2*vw));
	k.u = u1;
	k.v = v1;
	k.uw = u2-u1;
	k.vw = v2-v1;

	// compute kernel weights along u and v directions
	computeWeights(k.ku, (u1-u)/uw, 1.0/uw, k.uw);
	computeWeights(k.kv, (v1-v)/vw, 1.0/vw, k.vw);
    }

 private:
    double k(double x, const double* c)
    {
	x = fabs(x);
	if (x < 1)      return (c[0]*x + c[1])*x*x + c[2];
	else if (x < 2) return ((c[3]*x + c[4])*x + c[5])*x + c[6];
	else            return 0;
    }

    void computeWeights(double* kernel, double x1, double step, int size)
    {
	const double* c = _filter;
	for (int i = 0; i < size; i++) kernel[i] = k(x1 + i*step, c);
    }

    double _filter[7]; // filter coefficients for current sharpness
};



class PtexBoxFilter : public PtexSeparableFilter
{
 public:
    PtexBoxFilter() {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes)
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
	float u1 = u - 0.5*uw, u2 = u + 0.5*uw;
	float v1 = v - 0.5*vw, v2 = v + 0.5*vw;
	float u1floor = floor(u1), u2ceil = ceil(u2);
	float v1floor = floor(v1), v2ceil = ceil(v2);
	k.u = int(u1floor);
	k.v = int(v1floor);
	k.uw = int(u2ceil)-k.u;
	k.vw = int(v2ceil)-k.v;

	// compute kernel weights along u and v directions
	computeWeights(k.ku, k.uw, 1-(u1-u1floor), 1-(u2ceil-u2));
	computeWeights(k.kv, k.vw, 1-(v1-v1floor), 1-(v2ceil-v2));
    }

 private:
    void computeWeights(double* kernel, int size, float f1, float f2)
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


PtexFilter* PtexFilter::mitchell(float sharpness)
{
    //return new PtexSeparableMitchellFilter(sharpness);
        return new PtexBoxFilter();
        return new PtexMitchellFilter(sharpness);
}



PtexFilter* PtexFilter::box() { return new PtexBoxFilter(); }
PtexFilter* PtexFilter::gaussian() { return 0; }
PtexFilter* PtexFilter::trilinear() { return 0; }
PtexFilter* PtexFilter::radialBSpline() { return 0; }
