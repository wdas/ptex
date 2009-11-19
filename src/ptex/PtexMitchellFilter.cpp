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

#include "PtexPlatform.h"
#include "PtexMitchellFilter.h"
#include "PtexUtils.h"


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

    double B = 1 - sharpness; // choose C = (1-B)/2
    _filter[0] = 1.5 - B;
    _filter[1] = 1.5 * B - 2.5;
    _filter[2] = 1 - (1./3) * B;
    _filter[3] = (1./3) * B - 0.5;
    _filter[4] = 2.5 - 1.5 * B;
    _filter[5] = 2 * B - 4;
    _filter[6] = 2 - (2./3) * B;
}


void PtexMitchellFilter::eval(float* result, int firstchan, int nchannels,
			      int faceid, float u, float v,
			      float uw1, float vw1, float uw2, float vw2,
			      float width, float blur)
{
#if 0
    // for debugging only!
    {
	// point sample highest res
	const FaceInfo& f = tx->getFaceInfo(faceid);
	int resu = f.res.u(), resv = f.res.v();
	int ui = PtexUtils::clamp(int(u * resu), 0, resu-1);
	int vi = PtexUtils::clamp(int(v * resv), 0, resv-1);
	tx->getPixel(faceid, ui, vi, result, firstchan, nchannels);
	return;
    }
#endif

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
      resolution; otherwise missing pixel values are taken from local face
      - reuse evals that have the same res
      
      u,v blend factors are 0..1 (smoothstepped) over blend region
      u blend region = 2 pixels given [vface ures at edge .. local ures mid-face]
      v blend region = 2 pixels given [uface vres at edge .. local vres mid-face]
      - smoothstep between edge/mid-face resolutions (mid-face = u or v == 0.5)
    */

    // find filter width as bounding box of vectors w1 and w2
    float uw = fabs(uw1) + fabs(uw2), vw = fabs(vw1) + fabs(vw2);

    if (!_ctx.prepare(result, firstchan, nchannels, _tx, faceid, u, v, uw, vw))
	return;

    double weight = OneValueInv(_ctx.dt);

    // get face
    const FaceInfo& f = _tx->getFaceInfo(faceid);
    _isConstant = f.isConstant();

    // clamp filter width to no larger than 0.25 (todo - handle larger filter widths)
    _ctx.uw = _ctx.uw * width + blur;
    _ctx.vw = _ctx.vw * width + blur;
    _ctx.uw = PtexUtils::min(_ctx.uw, 0.25f);
    _ctx.vw = PtexUtils::min(_ctx.vw, 0.25f);

    // clamp filter width to no smaller than a pixel
    _ctx.uw = PtexUtils::max(_ctx.uw, 1.0f/(f.res.u()));
    _ctx.vw = PtexUtils::max(_ctx.vw, 1.0f/(f.res.v()));

    // compute desired texture res based on filter width
    int ureslog2 = int(ceil(log2(1.0/_ctx.uw))),
	vreslog2 = int(ceil(log2(1.0/_ctx.vw)));
    _face.set(faceid, Res(ureslog2, vreslog2));

    // find neighboring faces and determine if neighborhood is constant
#if 1
    getNeighborhood(f);
#endif

    // if neighborhood is constant, just return constant value of face
    if (_isConstant) {
	PtexFaceData* data = _tx->getData(faceid, 0);
	if (data) {
	    char* d = (char*) data->getData() + _ctx.firstchan*DataSize(_ctx.dt);
	    Ptex::ConvertToFloat(_ctx.result, d, _ctx.dt, _ctx.nchannels);
	    data->release();
	}
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
	else if (_vface && (_uface.res == _vface.res)) vweight += uweight;
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
    double udist, vdist;
    if (_ctx.u < .5) { ueid = e_left;   udist = _ctx.u; } 
    else             { ueid = e_right;  udist = 1 - _ctx.u; }
    if (_ctx.v < .5) { veid = e_bottom; vdist = _ctx.v; }
    else             { veid = e_top;    vdist = 1 - _ctx.v; }

    // blend zone starts at 1.5 texels in (at the current res)
    // to keep the kernel from accessing samples from the low-res face
    static const double blendstart = 1.5; // dist from edge (in texels) to start blending
    static const double blendend = 2.5;   // dist from edge (in texels) to stop blending

    // get u and v neighbors and compute blend weights
    double ublendstart=0, ublendend=0, vblendstart=0, vblendend=0;
    int ufid = f.adjfaces[ueid], vfid = f.adjfaces[veid];
    const FaceInfo* uf = 0, * vf = 0;
    if (ufid != -1) {
	// compute blend distances in u dir
	double texel = 1.0/_face.res.u();
	ublendstart = PtexUtils::min(blendstart*texel, .375);
	ublendend = PtexUtils::min(blendend*texel, 0.5);
	// get u neighbor face
	uf = &_ctx.tx->getFaceInfo(ufid);
	_uface.set(ufid, uf->res, f.adjedge(ueid) - ueid + 2);
	// clamp res against desired res and check for blending
	_uface.clampres(_face.res);
    }
    if (vfid != -1)  {
	// compute blend distances in v dir
	double texel = 1.0/_face.res.v();
	vblendstart = PtexUtils::min(blendstart*texel, .375);
	vblendend = PtexUtils::min(blendend*texel, 0.5);
	// get v neighbor face
	vf = &_ctx.tx->getFaceInfo(vfid);
	_vface.set(vfid, vf->res, f.adjedge(veid) - veid + 2);
	// clamp res against desired res and check for blending
	_vface.clampres(_face.res);
    }

    // smoothstep blendwidths towards corner
    if (_uface && _vface) {
	// smoothstep ublendwidth towards adj ures
	if (_vface.res.ulog2 != _face.res.ulog2) {
	    double texel = 1.0/_vface.res.u();
	    double adjstart = PtexUtils::min(blendstart*texel, .375);
	    double adjend = PtexUtils::min(blendend*texel, .5);
	    double wblend = PtexUtils::smoothstep(vdist, vblendstart, vblendend);
	    ublendstart = ublendstart * wblend + adjstart * (1-wblend);
	    ublendend = ublendend * wblend + adjend * (1-wblend);
	}
	// smoothstep vblendwidth towards adj vres
	if (_uface.res.vlog2 != _face.res.vlog2) {
	    double texel = 1.0/_uface.res.v();
	    double adjstart = PtexUtils::min(blendstart*texel, .375);
	    double adjend = PtexUtils::min(blendend*texel, .5);
	    double wblend = PtexUtils::smoothstep(udist, ublendstart, ublendend);
	    vblendstart = vblendstart * wblend + adjstart * (1-wblend);
	    vblendend = vblendend * wblend + adjend * (1-wblend);
	}
    }

    // compute blend weights based on distances to edges
    bool nearu = _uface && (udist < ublendend);
    bool nearv = _vface && (vdist < vblendend);

    if (!nearu) {
	_ublend = 0;
	_uface.clear();
    }
    else {
	// in blend zone w/ u
	_ublend = 1 - PtexUtils::qsmoothstep(udist, ublendstart, ublendend);
	if (_isConstant && !uf->isConstant()) _isConstant = 0;
    }

    if (!nearv) {
	_vblend = 0;
	_vface.clear();
    }
    else {
	// in blend zone w/ v
	_vblend = 1 - PtexUtils::qsmoothstep(vdist, vblendstart, vblendend);
	if (_isConstant && !vf->isConstant()) _isConstant = 0;
    }

    // gather corner faces if needed
    _interior = false;
    if (nearu && nearv) {
	// gather faces around corner starting at uface
	_cfaces.reserve(8); // (could be any number, but typically just 1 or 2)

	int cfid = ufid;		   // current face id (start at uface)
	const FaceInfo* cf = uf;	   // current face info
	EdgeId ceid = f.adjedge(ueid);	   // current edge id
	int rotate = _uface.rotate;	   // cumulative rotation
	int dir = (ueid+1)%4==veid? 3 : 1; // ccw or cw
	int count = 0;			   // runaway loop count

	while (count++ < 10) {
	    // advance to next face
	    int eid = EdgeId((ceid + dir) % 4);
	    cfid = cf->adjfaces[eid];
	    if (cfid == _vface.id || cfid == -1) 
		// reached "vface" or boundary, stop
		break;
	    ceid = cf->adjedge(eid);
	    cf = &_ctx.tx->getFaceInfo(cfid);
	    rotate += ceid - eid + 2;

	    // record face (note: first face (uface) is skipped)
	    _cfaces.push_back(Face());
	    Face& face = _cfaces.back();
	    face.set(cfid, cf->res, rotate);

	    if (_isConstant && !cf->isConstant()) _isConstant = 0;
	}
	// if we reached the vface, corner is an interior point
	if (cfid == _vface.id) {
	    _interior = true;
	    // see if we're regular - i.e. we have a single corner face
	    if (_cfaces.size() == 1) {
		_cface = _cfaces.front();

		// clamp res against u and v neighbors and check for blending
		_cface.clampres(_uface.res);
		_cface.clampres(_vface.res);

		// if either u or v needs blending, corner needs blending too
		if (_uface.blend || _vface.blend)
		    _cface.blend = true;
	    }
	}
	// otherwise corner is an e.p. on a mesh boundary
	else {
	    // don't blend w/ corner faces for this case
	    _cfaces.clear();
	}
    }

    // if all faces are constant, see if all have the same value
    if (_isConstant) {
	int pixelsize = DataSize(_ctx.dt) * _ctx.ntxchannels;
	PtexFaceData* data = _ctx.tx->getData(_face.id, 0);
	if (data) {
	    void* constval = data->getData();
	    if (_uface) {
		PtexFaceData* udata = _ctx.tx->getData(_uface.id, 0);
		if (udata) {
		    if (0 != memcmp(constval, udata->getData(), pixelsize))
			_isConstant = 0;
		    udata->release();
		}
	    }
	    if (_isConstant && _vface) {
		PtexFaceData* vdata = _ctx.tx->getData(_vface.id, 0);
		if (vdata) {
		    if (0 != memcmp(constval, vdata->getData(), pixelsize))
			_isConstant = 0;
		    vdata->release();
		}
	    }
	    if (_isConstant) {
		for (size_t i = 0, size = _cfaces.size(); i < size; i++) {
		    PtexFaceData* cdata = _ctx.tx->getData(_cfaces[i].id, 0);
		    if (cdata) {
			if (0 != memcmp(constval, cdata->getData(), pixelsize)) {
			    _isConstant = 0;
			    break;
			}
			cdata->release();
		    }
		}
	    }
	    data->release();
	}
    }
}


void PtexMitchellFilter::evalFaces(Res res, double weight, float uw, float vw)
{
    // initialize kernel
    // convert u,v and filter width to (fractional) pixels
    int ures = res.u(), vres = res.v();

    if (ures < 4 || vres < 4) {
	// can't use 4x4 mitchell, just do bilinear interp.
	// this should only happen for very small faces so smooth filtering shouldn't
	// be needed (fingers crossed)

	// todo - build 2x2 bilinear kernel
	PtexFilterKernel k;
	k.set(0, 0, 0, 1, 1, &weight, 0);
	k.apply(_face.id, 0, _ctx);
	return;
    }


    double u = _ctx.u * ures - 0.5, v = _ctx.v * vres - 0.5;
    uw *= ures; vw *= vres;

    // find integer pixel extent: [u,v] +/- [2*uw,2*vw]
    // (mitchell is 4 units wide for a 1 unit filter period)

    int u1 = int(ceil(u - 2*uw)), u2 = int(ceil(u + 2*uw));
    int v1 = int(ceil(v - 2*vw)), v2 = int(ceil(v + 2*vw));

    int kuw = u2-u1, kvw = v2-v1;
    if (kuw > 8 || kvw > 8) {
	// shouldn't happen - but just in case...
	assert(kuw <= 8 && kvw <= 8);
	return;
    }

    // compute kernel weights along u and v directions
    double* ukernel = (double*)alloca(kuw * sizeof(double));
    double* vkernel = (double*)alloca(kvw * sizeof(double));
    computeWeights(ukernel, (u1-u)/uw, 1.0/uw, kuw);
    computeWeights(vkernel, (v1-v)/vw, 1.0/vw, kvw);
    double scale = weight;

    // skip zero entries (will save a lot of work later)
    while (!ukernel[0])     { ukernel++; u1++; kuw--; }
    while (!ukernel[kuw-1]) { kuw--; }
    while (!vkernel[0])     { vkernel++; v1++; kvw--; }
    while (!vkernel[kvw-1]) { kvw--; }

    double sumu = 0; for (int i = 0; i < kuw; i++) sumu += ukernel[i];
    double sumv = 0; for (int i = 0; i < kvw; i++) sumv += vkernel[i];
    scale /= sumu * sumv;

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
	if (kc) {
	    if (!_cface && _interior) {
		// valence-3 interior case
		// clear corner and renormalize kernel
		double amt = 1.0/(1 - kc.totalWeight()/weight);
		kc.clear();
		for (double *kp = kbuffer, *end = kbuffer + kuw*kvw; kp != end; kp++)
		    *kp *= amt;
	    }
	    else if (!_cface || !(_cface.res >= res)) {
		// merge corner into u and/or v faces
		if (kv && _uface) {
		    if (_vface) {
			// merge corner 50% into ku and kv faces
			ku.merge(kc, kv.eidval(), 0.5);
			kv.merge(kc, ku.eidval(), 0.5);
		    }
		    else {
			// merge corner into ku across v edge
			ku.merge(kc, kv.eidval());
		    }
		} else {
		    // merge corner into kv across u edge
		    kv.merge(kc, ku.eidval());
		}
	    }
	}
	// merge boundary edges into main kernel
	if (ku && (!_uface || !(_uface.res >= res)))
	    k.merge(ku, ku.eidval());
	if (kv && (!_vface || !(_vface.res >= res)))
	    k.merge(kv, kv.eidval());

	if (ku) ku.apply(_uface.id, _uface.rotate, _ctx);
	if (kv) kv.apply(_vface.id, _vface.rotate, _ctx);
	if (kc) kc.apply(_cface.id, _cface.rotate, _ctx);
    }

    // eval faces
    k.apply(_face.id, 0, _ctx);
}
