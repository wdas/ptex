#ifndef PtexFilterContext_h
#define PtexFilterContext_h

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

#include <string.h>
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
