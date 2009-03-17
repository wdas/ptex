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


//#define NOEDGEBLEND // enable for debugging

void PtexSeparableFilter::eval(float* result, int firstChan, int nChannels,
			       int faceid, float u, float v, float uw, float vw)
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
    buildKernel(k, u, v, uw, vw, f.res);
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
    // which quadrant are we in? (kernel can only overlap one side in each dir)
    bool uHigh = (k.u >= 0), vHigh = (k.v >= 0);

    // do we need to split?
    bool uSplit = uHigh ? (k.u+k.uw > k.res.u()) : (k.u < 0);
    bool vSplit = vHigh ? (k.v+k.vw > k.res.v()) : (k.v < 0);

    // no splitting - just apply to local face
    if (!uSplit && !vSplit) { apply(k, faceid, f); return; }

    // do we have neighbors for the splits?
    // (if not, merge weights back into main kernel)
    int ufid=0, vfid=0;
    EdgeId ueid=e_left, veid=e_bottom;
    const FaceInfo *uf=0, *vf=0;
    if (uSplit) {
	ueid = uHigh ? e_right : e_left;
	ufid = f.adjface(ueid);
	uf = (ufid >= 0) ? &_tx->getFaceInfo(ufid) : 0;
#ifdef NOEDGEBLEND
	uf = 0;
#endif
	if (!uf) {
	    if (uHigh) k.mergeR(); else k.mergeL();
	    uSplit = 0;
	}
    }
    if (vSplit) {
	veid = vHigh ? e_top : e_bottom;
	vfid = f.adjface(veid);
	vf = (vfid >= 0) ? &_tx->getFaceInfo(vfid) : 0;
#ifdef NOEDGEBLEND
	vf = 0;
#endif
	if (!vf) {
	    if (vHigh) k.mergeT(); else k.mergeB();
	    vSplit = 0;
	}
    }

    if (uSplit) {
	// split kernel into two pieces
	PtexSeparableKernel ku;
	if (uHigh) k.splitR(ku); else k.splitL(ku);

	bool regularCorner = 0;
	if (vSplit) {
	    regularCorner = isCornerRegular(faceid, uHigh, vHigh);
	    if (!regularCorner) {
		// irregular corner, split corner off of u and ignore
		PtexSeparableKernel kc;
		if (vHigh) ku.splitT(kc); else ku.splitB(kc);
		// subtract corner weight from total
		_weight -= kc.weight();
	    }
	}

	applyAcrossEdge(ku, faceid, f, ueid, ufid, *uf, regularCorner);
    }

    if (vSplit) {
	// split kernel into two pieces and apply across edge
	PtexSeparableKernel kv;
	if (vHigh) k.splitT(kv); else k.splitB(kv);
	applyAcrossEdge(kv, faceid, f, veid, vfid, *vf, false);
    }

    // do local face
    apply(k, faceid, f); 
}


void PtexSeparableFilter::applyAcrossEdge(PtexSeparableKernel& k, 
					  int faceid, const Ptex::FaceInfo& f, int eid,
					  int afid, const Ptex::FaceInfo& af,
					  bool regularCorner)
{
    // adjust uv coord and res for face/subface boundary
    int aeid = f.adjedge(eid);
    bool ms = f.isSubface(), ns = af.isSubface();
    bool resplit = regularCorner;

    if (ms != ns) {
	if (!ms && ns) {
	    // adjust kernel from main face to subface
	    assert(k.res.ulog2 > 0 && k.res.vlog2 > 0);
	    k.res.ulog2--; k.res.vlog2--;
	    switch (eid) {
	    case e_bottom: k.v -= k.res.v(); break;
	    case e_right:  break;
	    case e_top:    k.u -= k.res.u(); break;
	    case e_left:   k.u -= k.res.u(); k.v -= k.res.v(); break;
	    }
	    resplit = true;
	    // TODO - switch to other subface if necessary
	}
	else { // (ms && !ns)
	    // adjust kernel from subface to main face

	    // Note: the transform depends on which subface the kernel is
	    // coming from.  The "primary" subface is the one the main
	    // face is pointing at.  The secondary subface adjustment
	    // happens to be the same as for the primary subface for the
	    // next edge, so the cases can be combined.
	    bool primary = (af.adjface(aeid) == faceid);
	    switch ((eid - primary) & 3) {
	    case e_bottom: k.v += k.res.v(); break;
	    case e_right:  break;
	    case e_top:    k.u += k.res.u(); break;
	    case e_left:   k.u += k.res.u(); k.v += k.res.v(); break;
	    }
	    k.res.ulog2++; k.res.vlog2++;
	}
    }
	    
    // rotate kernel to account for orientation difference
    k.rotate(eid - aeid + 2);

    // resplit if near a regular corner or going from face to subface
    if (resplit) splitAndApply(k, afid, af);
    else apply(k, afid, af);
}


void PtexSeparableFilter::apply(PtexSeparableKernel& k, int faceid, const Ptex::FaceInfo& f)
{
    assert(k.u >= 0 && k.u < k.res.u());
    assert(k.v >= 0 && k.v < k.res.v());

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


bool PtexSeparableFilter::isCornerRegular(int faceid, bool uHigh, bool vHigh)
{
    // traverse around vertex clockwise to see if it's regular (valence 4)
    // note: clockwise traversal means we can ignore the difference between
    // faces and subfaces

    int fid = faceid;
    int eid = (vHigh<<1) | (uHigh ^ vHigh); // LL=0, LR=1, UR=2, UL=3
    bool prevWasSubface = 0;
    int prevFid = 0;

    for (int i = 0; i < 4; i++) {
	// advance to next face
	const Ptex::FaceInfo& f = _tx->getFaceInfo(fid);
	bool isSubface = f.isSubface();
	if (prevWasSubface && !isSubface) {
	    // we're going from a subface to a main face
	    if (f.adjface((eid+3)%4) == prevFid) {
		// subface was primary subface - must be interior T corner
		return true;
	    }
	}
	prevWasSubface = isSubface;
	prevFid = fid;

	fid = f.adjface(eid);
	if (fid < 0) return false; // hit a boundary
	eid = (f.adjedge(eid) + 1) % 4;
    }
    
    // we're regular iff we're back where we started
    return fid == faceid;
}
