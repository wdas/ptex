/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include <math.h>
#include <assert.h>
#include "PtexSeparableFilter.h"
#include "PtexSeparableKernel.h"
#include "PtexUtils.h"


void PtexSeparableFilter::eval(float* result, int firstChan, int nChannels,
			       PtexTexture* tx, int faceid,
			       float u, float v, float uw, float vw)
{
    // init
    if (!tx || nChannels <= 0) return;
    if (faceid < 0 || faceid >= tx->numFaces()) return;
    _ntxchan = tx->numChannels();
    _dt = tx->dataType();
    _firstchan = firstChan;
    _nchan = PtexUtils::min(nChannels, _ntxchan-_firstchan);

    // clamp u and v
    u = PtexUtils::clamp(u, 0.0f, 1.0f);
    v = PtexUtils::clamp(v, 0.0f, 1.0f);

    // get face info
    const FaceInfo& f = tx->getFaceInfo(faceid);

    // if neighborhood is constant, just return constant value of face
    if (f.isNeighborhoodConstant()) {
	PtexFaceData* data = tx->getData(faceid, 0);
	if (data) {
	    char* d = (char*) data->getData() + _firstchan*DataSize(_dt);
	    Ptex::ConvertToFloat(result, d, _dt, _nchan);
	    data->release();
	}
	return;
    }

#if 0
    // if du and dv are > .25, then eval using the constant per-face data
    float minw = std::min(uw, vw);
    if (minw > .25) {
	/* for minw between .25 and 1, lerp w/ regular eval */
	if (minw <= 1) {
	    float blend = (minw-.25)/.75; // lerp amount
	    evalLargeDu(1.0, weight * blend);

	    // continue w/ regular eval, but weighted by lerp val
	    weight *= (1-blend);
	}
	else {
	    // minw > 1, just do large du eval
	    evalLargeDu(minw, weight);
	    return;
	}
    }
#endif

    // clamp filter width to no larger than 0.25 (todo: make value filter-specific)
    uw = std::min(uw, 0.25f);
    vw = std::min(vw, 0.25f);

    // clamp filter width to no smaller than a texel
    uw = std::max(uw, 1.0f/(f.res.u()));
    vw = std::max(vw, 1.0f/(f.res.v()));

    // compute desired texture res based on filter width
    int ureslog2 = int(ceil(log2(1.0/uw))),
	vreslog2 = int(ceil(log2(1.0/vw)));
    Res res(ureslog2, vreslog2);

    // compute weights, trim zeros, find extent

    // build kernel
    PtexSeparableKernel k;
    //    k.set(res.u(), res.v(), ...);

    // allocate temporary double-precision result
    _result = (double*) alloca(sizeof(double)*_nchan);
    memset(_result, 0, sizeof(double)*_nchan);

    // apply to faces
    //splitAndApply(k, faceid, f);

    // normalize (both for data type and cumulative kernel weight applied)
    // and output result
    double scale = 1.0 / (_weight * OneValue(_dt));
    for (int i = 0; i < _nchan; i++) result[i] = _result[i] * scale;

    // clear temp result
    _result = 0;
}



#if 0
foo() {
    float u = _ctx.u * ures - 0.5, v = _ctx.v * vres - 0.5;
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
}
#endif


#if 0
void PtexSeparableKernel::splitAndApply()
{
    // which quadrant are we in?
    bool uHigh = (u > .5), vHigh = (v > .5);

    // do we need to split?
    bool uSplit = uHigh ? (u+uw >= res.u()) : (u < 0);
    bool vSplit = vHigh ? (v+vw >= res.v()) : (v < 0);

    // no splitting - just apply to local face
    if (!uSplit && !vSplit) { apply(); return; }

    // do we have neighbors for the splits?
    // (if not, merge weights back into main kernel)
    int ufid, vfid;
    EdgeId ueid, veid;
    const FaceInfo *uf, *vf;
    if (uSplit) {
	ueid = uHigh ? e_right : e_left;
	ufid = f->adjface(ueid);
	uf = (ufid >= 0) ? &tx->getFaceInfo(ufid) : 0;
	if (!uf) {
	    if (uHigh) mergeR(); else mergeL();
	    uSplit = 0;
	}
    }
    if (vSplit) {
	veid = vHigh ? e_top : e_bottom;
	vfid = f->adjface(veid);
	vf = (vfid >= 0) ? &tx->getFaceInfo(vfid) : 0;
	if (!vf) {
	    if (vHigh) mergeT(); else mergeB();
	    vSplit = 0;
	}
    }

    // are we near a corner?
    if (uSplit && vSplit) {
	// split into 3 pieces: k, ku, kv where both ku and kv
	// include the corner and apply each piece.  First kernel
	// to hit the corner sets corner faceid, second kernel
	// applies iff faceid matches.

	// alternate approach - split kernel into 4 pieces
	// find corner from both neighbors and apply corner
	// only if regular.
    }

    // u split only?
    else if (uSplit) {
	// split kernel into two pieces
	PtexSeparableKernel k;
	if (uHigh) splitR(k); else splitL(k);

	// adjust uv coord and res for face/subface boundary
	bool ms = f->isSubface(), ns = uf->isSubface();
	if (ms != ns) {
	    // todo
	}
	    
	// rotate kernel to account for orientation difference
	k.rotate(ueid - f->adjedge(ueid) + 2);

	// apply (resplit if going from face to subface)
	if (!f->isSubface() && uf->isSubface()) k.splitAndApply();
	else k.apply();
    }

    // v split only?
    else if (vSplit) {
    }

    // do local face
    apply(); 
}
#endif

void PtexSeparableFilter::apply(PtexSeparableKernel& k, int faceid, Ptex::FaceInfo& f)
{
    // downres kernel if needed
    int fresu = f.res.u(), fresv = f.res.v();
    while (k.ures < fresu) k.downresU();
    while (k.vres < fresv) k.downresV();

    // get face data, and apply
    PtexFaceData* dh = _tx->getData(faceid, f.res);
    if (!dh) return;

    if (dh->isConstant()) {
	k.applyConst(_result, dh->getData(), _dt, _nchan);
    }
    else if (dh->isTiled()) {
	Ptex::Res tileres = dh->tileRes();
	int tileresu = tileres.u(), tileresv = tileres.v();
	int ntilesu = k.ures / tileresu;
	PtexSeparableKernel kt;
	kt.ures = tileresu; kt.vres = tileresv;
	int v = k.v, vw = k.vw;
	while (vw > 0) {
	    int tilev = v / tileresv;
	    kt.v = v % tileresv;
	    kt.vw = PtexUtils::min(vw, tileresv - kt.v);
	    kt.kv = k.kv + v - k.v;
	    int u = k.u, uw = k.uw;
	    while (uw > 0) {
		int tileu = u / tileresu;
		kt.u = u % tileresu;
		kt.uw = PtexUtils::min(uw, tileresu - kt.u);
		kt.ku = k.ku + u - k.u;
		PtexFaceData* th = dh->getTile(tilev * ntilesu + tileu);
		if (th) {
		    if (th->isConstant()) kt.applyConst(_result, th->getData(), _dt, _nchan);
		    else kt.apply(_result, th->getData(), _dt, _nchan, _ntxchan);
		}
		th->release();
		uw -= kt.ures;
		u += kt.ures;
	    }
	    vw -= kt.vres;
	    v += kt.vres;
	}
    }
    else {
	k.apply(_result, dh->getData(), _dt, _nchan, _ntxchan);
    }
    dh->release();
}


#if 0
void PtexSeparableFilter::evalLargeDu(float w, float weight)
{
    // eval using the constant per-face values
    // use "blended" values based on filter size
    int level = int(ceil(log2(1.0/w)));
    if (level > 0) level = 0;
    static int minlev = 5;
    if (level < minlev) { minlev = level; printf("%d\n", minlev); }
    switch (level) {
    case 0:   level = -1; break;
    case -1:  level = -2; break;
    case -2:  level = -7; break;
    default:
    case -3:  level = -28; break;
    }
    // get face
    const FaceInfo& f = _ctx.tx->getFaceInfo(_ctx.faceid);

    // get u and v neighbors
    EdgeId ueid, veid;
    float ublend, vblend; // lerp amounts: 0 at texel center, 0.5 at boundary
    if (_ctx.u < .5) { ueid = e_left;   ublend = .5 - _ctx.u; } 
    else             { ueid = e_right;  ublend = _ctx.u - .5; }
    if (_ctx.v < .5) { veid = e_bottom; vblend = .5 - _ctx.v; }
    else             { veid = e_top;    vblend = _ctx.v - .5; }
    int ufid = f.adjfaces[ueid], vfid = f.adjfaces[veid], cfid = -1;

    if (ufid >= 0 && vfid >= 0) {
	// get corner face from u face (just get first face for an e.p.)
	EdgeId ceid = f.adjedge(ueid);
	int dir = (ueid+1)%4==veid? 3 : 1; // ccw or cw
	int eid = EdgeId((ceid + dir) % 4);
	const FaceInfo& uf = _ctx.tx->getFaceInfo(ufid);
	cfid = uf.adjfaces[eid];
    }

    // apply lerp weights to each of the faces
    double mweight = weight * (1 - ublend) * (1 - vblend); // main face
    double uweight = weight * ublend * (1 - vblend);       // u blend
    double vweight = weight * (1 - ublend) * vblend;       // v blend
    double cweight = weight * ublend * vblend;	           // corner blend

    // for missing faces, push weight across boundary
    if (cfid >= 0) {
	evalLargeDuFace(cfid, level, cweight);
    } else {
	if (ufid >= 0) uweight += cweight;
	else vweight += cweight;
    }
    if (vfid >= 0) evalLargeDuFace(vfid, level, vweight); else mweight += vweight;
    if (ufid >= 0) evalLargeDuFace(ufid, level, uweight); else mweight += uweight;
    evalLargeDuFace(_ctx.faceid, level, mweight);
}


void PtexSeparableFilter::evalLargeDuFace(int faceid, int level, float weight)
{
    PtexFaceData* dh = _ctx.tx->getData(faceid, Res(level,level));
    if (dh) {
	PtexFilterKernel::applyConst(dh->getData(), _ctx, weight);
	dh->release();
    }
}

#endif
