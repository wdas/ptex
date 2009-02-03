#ifndef PtexTrilinearFilter_h
#define PtexTrilinearFilter_h

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
#include <vector>

class PtexTrilinearFilter : public PtexFilter, public Ptex
{
 public:
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      PtexTexture* tx, int faceid,
		      float u, float v, float uw, float vw);

 protected:
    void chooseMipmapRes(Res& res, double& lerp);
    void evalBilinear(Res res, float weight);

    PtexTrilinearFilter() {}
    virtual ~PtexTrilinearFilter() {}

    float* _result;
    int _firstchan;
    int _nchannels;
    PtexTexture* _tx; 
    int _faceid;
    float _u;
    float _v;
    float _uw;
    float _vw;
};

#endif
