#include <assert.h> // todo - remove asserts
#include <math.h>
#include "PtexMitchellFilter.h"
#include "PtexUtils.h"


PtexFilter* PtexFilter::mitchell(float sharpness)
{
    return new PtexMitchellFilter(sharpness);
}


void PtexMitchellFilter::setSharpness(float sharpness)
{
    _sharpness = sharpness;
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


void PtexMitchellFilter::eval(float* result, int firstchan, int nchannels,
			      PtexTexture* tx, int faceid,
			      float u, float v, float uw, float vw)
{
    /*
      Filter Scenarios:

      Regular - Local res available for all faces within kernel (and no e.p.)
      eval at local res only (local res = desired res clamped against local max)

      Blended - Desired res not available for all faces, or near e.p.
      blend between different resolution surfaces:
      a) local res: filter res clamped to res of local face
      b) uface res: local res clamped to res of u neighbor face
      c) vface res: local res clamped to res of v neighbor face
      d) cface res: local res clamped to res of u, v and corner faces
      Notes:
      - for all evals, adjacent faces are included if they have sufficent
      resolution; otherwise missing pixel values are extrapolated from local face
      - reuse evals that have the same res
      
      u,v blend factors are 0..1 (smoothstepped) over blend region
      u blend region = 2 pixels given [vface ures at edge .. local ures mid-face]
      v blend region = 2 pixels given [uface vres at edge .. local vres mid-face]
      - smoothstep between edge/mid-face resolutions (mid-face = u or v == 0.5)
    */

    if (!_ctx.prepare(result, firstchan, nchannels, tx, faceid, u, v, uw, vw))
	return;

    double weight = OneValueInv(_ctx.dt);

    // get face
    const FaceInfo& f = tx->getFaceInfo(faceid);
    _isConstant = f.isConstant();

    // clamp filter width to no larger than 0.25 (todo - handle larger filter widths)
    _ctx.uw = std::min(_ctx.uw, 0.25f);
    _ctx.vw = std::min(_ctx.vw, 0.25f);

    // clamp filter width to no smaller than a pixel
    _ctx.uw = std::max(_ctx.uw, 1.0f/(f.res.u()));
    _ctx.vw = std::max(_ctx.vw, 1.0f/(f.res.v()));

    // compute desired texture res based on filter width
    int ureslog2 = int(ceil(log2(1.0/_ctx.uw))),
	vreslog2 = int(ceil(log2(1.0/_ctx.vw)));
    _face.set(faceid, Res(ureslog2, vreslog2));

    // find neighboring faces and determine if neighborhood is constant
#if 1
    getNeighborhood(f);
#endif

#if 1
    _ctx.uw = 1.0f/(f.res.u());
    _ctx.vw = 1.0f/(f.res.v());
#endif

    // if neighborhood is constant, just return constant value of face
    if (_isConstant) {
	PtexFaceData* data = tx->getData(faceid, 0);
	if (data) {
	    int dtsize = DataSize(_ctx.dt);
	    char* d = (char*) data->getData();
	    memcpy(_ctx.result, d + _ctx.firstchan*dtsize, _ctx.nchannels*dtsize);
	}
	data->release();
	return;
    }

    // if we don't have any neighbors, just eval face by itself
    if (!_uface && !_vface) {
	evalFaces(_face.res, weight, _ctx.uw, _ctx.vw);
     	return;
    }

    // compute weights for the 4 possible surfaces
    double mweight = weight * (1 - _ublend) * (1 - _vblend); // main face
    double uweight = weight * _ublend * (1 - _vblend);       // u blend
    double vweight = weight * (1 - _ublend) * _vblend;       // v blend
    double cweight = weight * _ublend * _vblend;	     // corner blend

    // eval surfaces using given weights
    // but first combine weights where surfaces are missing or no blending is required
    if (cweight) {
	if (_cface) {
	    // combine w/ main if no blend needed
	    if (!_cface.blend) mweight += cweight;
	    // combine with an edge if res matches
	    else if (_cface.res == _uface.res) uweight += cweight;
	    else if (_cface.res == _vface.res) vweight += cweight;
	    else evalFaces(_cface.res, cweight);
	}
	// else if (_cfaces.size() > 1) {
	//     // todo - near an e.p., blend using linear rbf
	// }
	else mweight += cweight;
    }
    if (uweight) {
	if (!_uface.blend) mweight += uweight;
	else if (_uface.res == _vface.res) vweight += uweight;
	else evalFaces(_uface.res, uweight);
    }
    if (vweight) {
	if (!_vface.blend) mweight += vweight;
	else evalFaces(_vface.res, vweight);
    }
    if (mweight) {
	evalFaces(_face.res, mweight, _ctx.uw, _ctx.vw);
    }
}


void PtexMitchellFilter::getNeighborhood(const FaceInfo& f)
{
    _uface.clear(); _vface.clear(), _cface.clear(), _cfaces.clear();

    EdgeId ueid, veid;
    float udist, vdist;
    if (_ctx.u < .5) { ueid = e_left;   udist = _ctx.u; } 
    else             { ueid = e_right;  udist = 1 - _ctx.u; }
    if (_ctx.v < .5) { veid = e_bottom; vdist = _ctx.v; }
    else             { veid = e_top;    vdist = 1 - _ctx.v; }

    bool nearu = udist < 1.5 * _ctx.uw;
    bool nearv = vdist < 1.5 * _ctx.vw;

    if (!nearu && !nearv) 
	// not near an edge, no neighbors
	return;

    // get u and v neighbors and compute blend weights
    float ublendwidth=0, vblendwidth=0;
    int ufid = f.adjfaces[ueid], vfid = f.adjfaces[veid];
    const FaceInfo* uf = 0, * vf = 0;
    if (nearu && ufid != -1) {
	ublendwidth = 2.0/f.res.u();
	// get u neighbor face
	uf = &_ctx.tx->getFaceInfo(ufid);
	if (!uf->isConstant()) _isConstant = 0;
	_uface.set(ufid, uf->res, f.adjedge(ueid) - ueid + 2);
	// clamp res against desired res and check for blending
	_uface.clampres(_face.res);
    }
    if (nearv && vfid != -1)  {
	vblendwidth = 2.0/f.res.v();
	// get v neighbor face
	vf = &_ctx.tx->getFaceInfo(vfid);
	if (!vf->isConstant()) _isConstant = 0;
	_vface.set(vfid, vf->res, f.adjedge(veid) - veid + 2);
	// clamp res against desired res and check for blending
	_vface.clampres(_face.res);
    }

    // compute blend weights based on distances to edges
    // blend width is 2 pixels / res in u or v direction
    if (_uface && _vface) {
	// smoothstep ublendwidth towards adj ures
	if (_vface.res.ulog2 != f.res.ulog2) {
	    float adjwidth = 2.0/_vface.res.u();
	    float wblend = PtexUtils::smoothstep(vdist, 0, 0.5);
	    ublendwidth = ublendwidth * wblend + adjwidth * (1-wblend);
	}
	// smoothstep vblendwidth towards adj vres
	if (_uface && _uface.res.vlog2 != f.res.vlog2) {
	    float adjwidth = 2.0/_uface.res.v();
	    float wblend = PtexUtils::smoothstep(udist, 0, 0.5);
	    vblendwidth = vblendwidth * wblend + adjwidth * (1-wblend);
	}
    }
    _ublend = 1 - PtexUtils::smoothstep(udist, 0, ublendwidth);
    _vblend = 1 - PtexUtils::smoothstep(vdist, 0, vblendwidth);

    // if we're not blending in both directions, then we don't care about the corner
    if (!_ublend || !_vblend) return;

    // gather faces around corner starting at uface
    _cfaces.reserve(8); // (could be any number, but typically just 1 or 2)

    int cfid = ufid;		       // current face id (start at uface)
    const FaceInfo* cf = uf;	       // current face info
    EdgeId ceid = f.adjedge(ueid);     // current edge id
    int rotate = _uface.rotate;	       // cumulative rotation
    int dir = (ueid+1)%4==veid? 3 : 1; // ccw or cw
    int count = 0;		       // runaway loop count

    while (count++ < 10) {
	// advance to next face
	int eid = EdgeId((ceid + dir) % 4);
	cfid = cf->adjfaces[eid];
	if (cfid == _vface.id || cfid == -1) 
	    // reached a boundary or a boundary, stop
	    break;

	cf = &_ctx.tx->getFaceInfo(cfid);
	ceid = cf->adjedge(eid);
	rotate += ceid - eid + 2;

	// record face (note: first face (uface) is skipped)
	_cfaces.push_back(Face());
	Face& face = _cfaces.back();
	face.set(cfid, cf->res, rotate);
    }
    // if we reached the vface, corner is an interior point
    if (cfid == _vface.id) {
	// see if we're reguler - i.e. we have a single corner face
	if (_cfaces.size() == 1) {
	    _cface = _cfaces.front();
	    _cface.res.clamp(_uface.res);
	    _cface.res.clamp(_vface.res);
	}
    }
    // otherwise corner is an e.p. on a mesh boundary
    else {
	// don't blend w/ corner faces for this case
	_cfaces.clear();
    }
}


void PtexMitchellFilter::evalFaces(Res res, double weight, float uw, float vw)
{
    // initialize kernel
    // convert u,v and filter width to (fractional) pixels
    int ures = res.u(), vres = res.v();
    float u = _ctx.u * ures - 0.5, v = _ctx.v * vres - 0.5;
    uw *= ures; vw *= vres;
    
    // find integer pixel extent: [u,v] +/- [2*uw,2*vw]
    // (mitchell is 4 units wide for a 1 unit filter period)
    int u1 = int(ceil(u - 2*uw)), u2 = int(ceil(u + 2*uw));
    int v1 = int(ceil(v - 2*vw)), v2 = int(ceil(v + 2*vw));
    int kuw = u2-u1, kvw = v2-v1;
    assert(kuw >= 4 && kuw <= 8 && kvw >= 4 && kvw <= 8);

    // compute kernel weights along u and v directions
    double* ukernel = (double*)alloca(kuw * sizeof(double));
    double* vkernel = (double*)alloca(kvw * sizeof(double));
    computeWeights(ukernel, (u1-u)/uw, 1.0/uw, kuw);
    computeWeights(vkernel, (v1-v)/vw, 1.0/vw, kvw);
    double scale = weight * (16/(kuw*kvw));

    // skip zero entries (will save a lot of work later)
#if 1
    while (!ukernel[0])     { ukernel++; u1++; kuw--; }
    while (!ukernel[kuw-1]) { kuw--; }
    while (!vkernel[0])     { vkernel++; v1++; kvw--; }
    while (!vkernel[kvw-1]) { kvw--; }
#endif

    // compute tensor product to form rectangular kernel
    double* kbuffer = (double*) alloca(kuw*kvw*sizeof(double));
    double* kp = kbuffer;
    for (int i = 0; i < kvw; i++, kp += kuw) {
	double vk = vkernel[i] * scale;
	for (int j = 0; j < kuw; j++) kp[j] = ukernel[j]*vk;
    }
    PtexFilterKernel k; k.set(res, u1, v1, kuw, kvw, kbuffer, kuw);

    // split kernel across edges into face, u,v, and corner parts
    PtexFilterKernel ku, kv, kc;
    k.split(ku, kv, kc);

    if (ku || kv) {
	// merge kernel parts back in for missing/insufficient-res faces
	if (kc && (!_cface || !(_cface.res >= res))) {
	    // merge corner into an edge, preferably an existing one
	    // todo - if both edges exist - then merge half into each edge?
	    if (kv && _uface) 
		// merge corner into ku across v edge
		ku.merge(kc, kv.eidval(), _extrapolate);
	    else
		// merge corner into kv across u edge
		kv.merge(kc, ku.eidval(), _extrapolate);
	}
	// merge boundary edges into main kernel
	if (ku && (!_uface || !(_uface.res >= res)))
	    k.merge(ku, ku.eidval(), _extrapolate);
	if (kv && (!_vface || !(_vface.res >= res)))
	    k.merge(kv, kv.eidval(), _extrapolate);
    }

    // eval faces
    k.apply(_face.id, 0, _ctx);
    if (ku) ku.apply(_uface.id, _uface.rotate, _ctx);
    if (kv) kv.apply(_vface.id, _vface.rotate, _ctx);
    if (kc) kc.apply(_cface.id, _cface.rotate, _ctx);
}


