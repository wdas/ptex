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
	uw = std::min(uw, 0.25f);
	vw = std::min(vw, 0.25f);

	// clamp filter width to no smaller than a texel
	uw = std::max(uw, 1.0f/(faceRes.u()));
	vw = std::max(vw, 1.0f/(faceRes.v()));

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
	k.uw = PtexUtils::min(u2-u1, PtexSeparableKernel::kmax);
	k.vw = PtexUtils::min(v2-v1, PtexSeparableKernel::kmax);

	// compute kernel weights along u and v directions
	computeWeights(k.ku, (u1-u)/uw, 1.0/uw, k.uw);
	computeWeights(k.kv, (v1-v)/vw, 1.0/vw, k.vw);

	// trim zero weights
	while (k.ku[0] == 0) { k.ku++; k.u++; k.uw--; }
	while (k.kv[0] == 0) { k.kv++; k.v++; k.vw--; }
	while (k.ku[k.uw-1] == 0) { k.uw--; }
	while (k.kv[k.vw-1] == 0) { k.vw--; }
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

    double _filter[7];		// filter coefficients for current sharpness
};


PtexFilter* PtexFilter::mitchell(float sharpness)
{
    return new PtexSeparableMitchellFilter(sharpness);
    //    return new PtexMitchellFilter(sharpness);
}



PtexFilter* PtexFilter::box() { return 0; }
PtexFilter* PtexFilter::gaussian() { return 0; }
PtexFilter* PtexFilter::trilinear() { return 0; }
PtexFilter* PtexFilter::radialBSpline() { return 0; }
