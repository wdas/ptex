#ifndef PtexFilterContext_h
#define PtexFilterContext_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include "Ptexture.h"
#include "PtexUtils.h"

class PtexFilterContext : public Ptex
{
 public:
    float* result;		// result being evaluated
    int firstchan;		// first channel to eval
    int nchannels;		// number of channels to eval
    PtexTexture* tx;		// texture being evaluated
    int ntxchannels;		// number of channels in texture
    DataType dt;		// data type of texture
    int faceid;			// face being evaluated
    float u, v;			// u,v being evaluated
    float uw, vw;		// filter width

    bool prepare(float* result_val, int firstchan_val, int nchannels_val,
		 PtexTexture* tx_val, int faceid_val,
		 float u_val, float v_val, float uw_val, float vw_val)
    {
	memset(result_val, 0, sizeof(float)*nchannels_val);

	// get tx info
	tx = tx_val;
	if (!tx) return 0;
	ntxchannels = tx->numChannels();
	dt = tx->dataType();

	// record context
	result = result_val;
	firstchan = firstchan_val;
	nchannels = PtexUtils::min(nchannels_val, 
				   ntxchannels-firstchan_val);
	faceid = faceid_val;
	u = PtexUtils::clamp(u_val, 0.0f, 1.0f);
	v = PtexUtils::clamp(v_val, 0.0f, 1.0f);
	uw = uw_val; vw = vw_val;

	// validate params
	if ((nchannels <= 0) ||
	    (faceid < 0 || faceid >= tx->numFaces()))
	    return 0;

	return 1;
    }
};

#endif
