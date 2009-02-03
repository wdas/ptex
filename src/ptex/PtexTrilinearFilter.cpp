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
#include "PtexTrilinearFilter.h"
#include "PtexUtils.h"

void PtexTrilinearFilter::eval(float* result, int firstchan, int nchannels,
			       PtexTexture* tx, int faceid,
			       float u, float v, float uw, float vw)
{
    // init result
    _result = result;
    memset(_result, 0, sizeof(float)*nchannels);

    // clamp/store param vals
    _firstchan = PtexUtils::clamp(firstchan, 0, tx->numChannels() - 1);
    _nchannels = PtexUtils::clamp(nchannels, 1, tx->numChannels() - _firstchan);
    _tx = tx;
    _faceid = PtexUtils::clamp(faceid, 0, tx->numFaces() - 1);
    _u = PtexUtils::clamp(u, 0.0f, 1.0f);
    _v = PtexUtils::clamp(v, 0.0f, 1.0f);
    _uw = PtexUtils::clamp(uw, 1e-10f, 1.0f);
    _vw = PtexUtils::clamp(vw, 1e-10f, 1.0f);

    // choose lower mipmap res, and compute amount to lerp towards higher res
    Res res = 0;
    double lerp = 0;
    chooseMipmapRes(res, lerp);

    // eval at lower res (scale weight to normalize data type)
    double scale = Ptex::OneValueInv(tx->dataType());
    evalBilinear(res, scale * (1-lerp));

    // eval at next higher res (if needed)
    if (lerp) evalBilinear(Res(res.ulog2+1, res.vlog2+1), scale * lerp);
}


void PtexTrilinearFilter::chooseMipmapRes(Res& res, double& lerp)
{
    // choose mipmap res based on lower-res axis (as compared to base aspect)
    const FaceInfo& f = _tx->getFaceInfo(_faceid);
    int aspect = f.res.ulog2 - f.res.vlog2; // log2(ures/vres);
    double resuf = PtexUtils::min(double(f.res.u()), 1.0/_uw);
    double resvf = PtexUtils::min(double(f.res.v()), 1.0/_vw);
    double log2resu = log2(resuf);
    double log2resv = log2(resvf);
    if (log2resu-log2resv < aspect) {
	// u is lower, choose based on target u res
	res.ulog2 = int(ceil(log2resu));
	res.vlog2 = res.ulog2 - aspect;
	if (res.vlog2 < 0)
	lerp = resuf / res.u() - 1;
    }
    else {
	// v is lower, choose based on target v res
	res.vlog2 = int(ceil(log2resv));
	res.ulog2 = res.vlog2 + aspect;
	lerp = resvf / res.v() - 1;
    }

    // only lerp if needed
    if (lerp < 1e-6) { lerp = 0; }
    else if (lerp > 1 - 1e-6) { lerp = 0; res.ulog2++; res.vlog2++; }
}


void PtexTrilinearFilter::evalBilinear(Res res, float weight)
{
    PtexPtr<PtexFaceData> data(_tx->getData(_faceid, res));
    if (data->isConstant()) {
    }
    else if (data->isTiled()) {
    }
    else {
    }
}
