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
#include <math.h>
#include <assert.h>

#include "PtexSeparableFilter.h"
#include "PtexSeparableKernel.h"
#include "PtexUtils.h"


//#define NOEDGEBLEND // uncomment to disable filtering across edges (for debugging)

void PtexSeparableFilter::eval(float* result, int firstChan, int nChannels,
			       int faceid, float u, float v, float uw, float vw,
			       float width, float blur)
{
    // init
    if (!_tx || nChannels <= 0) return;
    if (faceid < 0 || faceid >= _tx->numFaces()) return;
    _ntxchan = _tx->numChannels();
    _dt = _tx->dataType();
    _firstChanOffset = firstChan*DataSize(_dt);
    _nchan = PtexUtils::min(nChannels, _ntxchan-firstChan);

    // clamp u and v
    u = PtexUtils::clamp(u, 0.0f, 1.0f);
    v = PtexUtils::clamp(v, 0.0f, 1.0f);

    // get face info
    const FaceInfo& f = _tx->getFaceInfo(faceid);

    // if neighborhood is constant, just return constant value of face
    if (f.isNeighborhoodConstant()) {
	PtexPtr<PtexFaceData> data ( _tx->getData(faceid, 0) );
	if (data) {
	    char* d = (char*) data->getData() + _firstChanOffset;
	    Ptex::ConvertToFloat(result, d, _dt, _nchan);
	}
	return;
    }

    // build kernel
    PtexSeparableKernel k;
    if (f.isSubface()) {
	// for a subface, build the kernel as if it were on a main face and then downres
	uw = uw * width + blur * 2;
	vw = vw * width + blur * 2;
	buildKernel(k, u*.5, v*.5, uw*.5, vw*.5, f.res);
	if (k.res.ulog2 == 0) k.upresU();
	if (k.res.vlog2 == 0) k.upresV();
	k.res.ulog2--; k.res.vlog2--;
    }
    else {
	uw = uw * width + blur;
	vw = vw * width + blur;
	buildKernel(k, u, v, uw, vw, f.res);
    }
    k.stripZeros();

    // check kernel (debug only)
    assert(k.uw > 0 && k.vw > 0);
    assert(k.uw <= PtexSeparableKernel::kmax && k.vw <= PtexSeparableKernel::kmax);
    assert(k.u + k.uw - 1 >= 0 && k.u < k.res.u());
    assert(k.v + k.vw - 1 >= 0 && k.v < k.res.v());
    _weight = k.weight();

    // allocate temporary double-precision result
    _result = (double*) alloca(sizeof(double)*_nchan);
    memset(_result, 0, sizeof(double)*_nchan);

    // apply to faces
    splitAndApply(k, faceid, f);

    // normalize (both for data type and cumulative kernel weight applied)
    // and output result
    double scale = 1.0 / (_weight * OneValue(_dt));
    for (int i = 0; i < _nchan; i++) result[i] = float(_result[i] * scale);

    // clear temp result
    _result = 0;
}


void PtexSeparableFilter::splitAndApply(PtexSeparableKernel& k, int faceid, const Ptex::FaceInfo& f)
{
    // do we need to split? (i.e. does kernel span an edge?)
    bool splitR = (k.u+k.uw > k.res.u()), splitL = (k.u < 0);
    bool splitT = (k.v+k.vw > k.res.v()), splitB = (k.v < 0);

#ifdef NOEDGEBLEND
    // for debugging only
    if (splitR) k.mergeR();
    if (splitL) k.mergeL();
    if (splitT) k.mergeT();
    if (splitB) k.mergeB();
#else
    if (splitR || splitL || splitT || splitB) { 
	PtexSeparableKernel ka, kc;
	if (splitR) {
	    if (f.adjface(e_right) >= 0) {
		k.splitR(ka);
		if (splitT) {
		    if (f.adjface(e_top) >= 0) {
			ka.splitT(kc);
			applyToCorner(kc, faceid, f, e_top);
		    }
		    else ka.mergeT();
		}
		if (splitB) {
		    if (f.adjface(e_bottom) >= 0) {
			ka.splitB(kc);
			applyToCorner(kc, faceid, f, e_right);
		    }
		    else ka.mergeB();
		}
		applyAcrossEdge(ka, faceid, f, e_right);
	    }
	    else k.mergeR();
	}
	if (splitL) {
	    if (f.adjface(e_left) >= 0) {
		k.splitL(ka);
		if (splitT) {
		    if (f.adjface(e_top) >= 0) {
			ka.splitT(kc);
			applyToCorner(kc, faceid, f, e_left);
		    }
		    else ka.mergeT();
		}
		if (splitB) {
		    if (f.adjface(e_bottom) >= 0) {
			ka.splitB(kc);
			applyToCorner(kc, faceid, f, e_bottom);
		    }
		    else ka.mergeB();
		}
		applyAcrossEdge(ka, faceid, f, e_left);
	    }
	    else k.mergeL();
	}
	if (splitT) {
	    if (f.adjface(e_top) >= 0) {
		k.splitT(ka);
		applyAcrossEdge(ka, faceid, f, e_top);
	    }
	    else k.mergeT();
	}
	if (splitB) {
	    if (f.adjface(e_bottom) >= 0) {
		k.splitB(ka);
		applyAcrossEdge(ka, faceid, f, e_bottom);
	    }
	    else k.mergeB();
	}
    }
#endif

    // do local face
    apply(k, faceid, f); 
}


void PtexSeparableFilter::applyAcrossEdge(PtexSeparableKernel& k, 
					  int faceid, const Ptex::FaceInfo& f, int eid)
{
    int afid = f.adjface(eid), aeid = f.adjedge(eid);
    const Ptex::FaceInfo& af = _tx->getFaceInfo(afid);

    // adjust uv coord and res for face/subface boundary
    bool fIsSubface = f.isSubface(), afIsSubface = af.isSubface();
    if (fIsSubface != afIsSubface) { // main face to subface boundary
	if (afIsSubface) {
	    k.adjustMainToSubface(eid);
	}
	else {  // subface to main face transition
	    // Note: the transform depends on which subface the kernel is
	    // coming from.  The "primary" subface is the one the main
	    // face is pointing at.  The secondary subface adjustment
	    // happens to be the same as for the primary subface for the
	    // next edge, so the cases can be combined.
	    bool primary = (af.adjface(aeid) == faceid);
	    k.adjustSubfaceToMain(eid - primary);
	}
    }

    // rotate and apply (resplit if going to a subface)
    k.rotate(eid - aeid + 2);
    if (afIsSubface) splitAndApply(k, afid, af);
    else apply(k, afid, af);
}


void PtexSeparableFilter::applyToCorner(PtexSeparableKernel& k, int faceid, 
					const Ptex::FaceInfo& f, int eid)
{
    // traverse clockwise around corner vertex and gather corner faces
    int afid = faceid, aeid = eid;
    const FaceInfo* af = &f;
    bool prevIsSubface = af->isSubface();

    const int MaxValence = 10;
    int cfaceId[MaxValence];
    int cedgeId[MaxValence];
    const FaceInfo* cface[MaxValence];

    int numCorners = 0;
    for (int i = 0; i < MaxValence; i++) {
	// advance to next face
	int prevFace = afid;
	afid = af->adjface(aeid);
	aeid = (af->adjedge(aeid) + 1) % 4;

	// we hit a boundary or reached starting face
	// note: we need need to edge id too because we might
	// have a torus texture where all 4 corners are from the same face
	if (afid < 0 || (afid == faceid && aeid == eid)) {
	    numCorners = i - 2;
	    break;
	}

	// record face info
	af = &_tx->getFaceInfo(afid);
	cfaceId[i] = afid;
	cedgeId[i] = aeid;
	cface[i] = af;

	// check to see if corner is a subface "tee"
	bool isSubface = af->isSubface();
	if (prevIsSubface && !isSubface && af->adjface((aeid+3)%4) == prevFace) 
	{
	    // adjust the eid depending on whether we started from
	    // the primary or secondary subface.
	    bool primary = (i==1);
	    k.adjustSubfaceToMain(eid + primary * 2);
	    k.rotate(eid - aeid + 3 - primary);
	    splitAndApply(k, afid, *af);
	    return;
	}
	prevIsSubface = isSubface;
    }

    if (numCorners == 1) {
	// regular case (valence 4)
	applyToCornerFace(k, f, eid, cfaceId[1], *cface[1], cedgeId[1]);
    }
    else if (numCorners > 1) {
	// valence 5+, make kernel symmetric and apply equally to each face
	// first, rotate to standard orientation, u=v=0
	k.rotate(eid + 2);
	k.makeSymmetric();
	for (int i = 1; i <= numCorners; i++) {
	    PtexSeparableKernel kc = k;
	    applyToCornerFace(kc, f, 2, cfaceId[i], *cface[i], cedgeId[i]);
	}
	// adjust weight for additional corners (one was already counted)
	_weight += k.weight() * (numCorners-1);
    }
    else {
	// valence 2 or 3, ignore corner face (just adjust weight)
	_weight -= k.weight();
    }
}


void PtexSeparableFilter::applyToCornerFace(PtexSeparableKernel& k, const Ptex::FaceInfo& f, int eid,
					    int cfid, const Ptex::FaceInfo& cf, int ceid)
{
    // adjust uv coord and res for face/subface boundary
    bool fIsSubface = f.isSubface(), cfIsSubface = cf.isSubface();
    if (fIsSubface != cfIsSubface) {
	if (cfIsSubface) k.adjustMainToSubface(eid + 3);
	else k.adjustSubfaceToMain(eid + 3);
    }
    
    // rotate and apply (resplit if going to a subface)
    k.rotate(eid - ceid + 2);
    if (cfIsSubface) splitAndApply(k, cfid, cf);
    else apply(k, cfid, cf);
}


void PtexSeparableFilter::apply(PtexSeparableKernel& k, int faceid, const Ptex::FaceInfo& f)
{
    assert(k.u >= 0 && k.u + k.uw <= k.res.u());
    assert(k.v >= 0 && k.v + k.vw <= k.res.v());

    if (k.uw == 0 || k.vw == 0) return;

    // downres kernel if needed
    while (k.res.u() > f.res.u()) k.downresU();
    while (k.res.v() > f.res.v()) k.downresV();

    // get face data, and apply
    PtexPtr<PtexFaceData> dh ( _tx->getData(faceid, k.res) );
    if (!dh) return;

    if (dh->isConstant()) {
	k.applyConst(_result, (char*)dh->getData()+_firstChanOffset, _dt, _nchan);
    }
    else if (dh->isTiled()) {
	Ptex::Res tileres = dh->tileRes();
	PtexSeparableKernel kt;
	kt.res = tileres;
	int tileresu = tileres.u();
	int tileresv = tileres.v();
	int ntilesu = k.res.u() / tileresu;
	for (int v = k.v, vw = k.vw; vw > 0; vw -= kt.vw, v += kt.vw) {
	    int tilev = v / tileresv;
	    kt.v = v % tileresv;
	    kt.vw = PtexUtils::min(vw, tileresv - kt.v);
	    kt.kv = k.kv + v - k.v;
	    for (int u = k.u, uw = k.uw; uw > 0; uw -= kt.uw, u += kt.uw) {
		int tileu = u / tileresu;
		kt.u = u % tileresu;
		kt.uw = PtexUtils::min(uw, tileresu - kt.u);
		kt.ku = k.ku + u - k.u;
		PtexPtr<PtexFaceData> th ( dh->getTile(tilev * ntilesu + tileu) );
		if (th) {
		    if (th->isConstant())
			kt.applyConst(_result, (char*)th->getData()+_firstChanOffset, _dt, _nchan);
		    else
			kt.apply(_result, (char*)th->getData()+_firstChanOffset, _dt, _nchan, _ntxchan);
		}
	    }
	}
    }
    else {
	k.apply(_result, (char*)dh->getData()+_firstChanOffset, _dt, _nchan, _ntxchan);
    }
}
