#ifndef PtexSeparableFilter_h
#define PtexSeparableFilter_h

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

class PtexSeparableKernel;

class PtexSeparableFilter : public PtexFilter, public Ptex
{
 public:
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      PtexTexture* tx, int faceid,
		      float u, float v, float uw, float vw);

 protected:
    PtexSeparableFilter() :
	_tx(0), _result(0), _weight(0), 
	_firstChanOffset(0), _nchan(0), _ntxchan(0),
	_dt((DataType)0) {}
    virtual ~PtexSeparableFilter() {}

    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
			     Res faceRes) = 0;
    
    void splitAndApply(PtexSeparableKernel& k, int faceid, const Ptex::FaceInfo& f);
    void applyAcrossEdge(PtexSeparableKernel& k, 
			 int faceid, const Ptex::FaceInfo& f, int eid,
			 int afid, const Ptex::FaceInfo& af,
			 bool regularCorner);
    void apply(PtexSeparableKernel& k, int faceid, const Ptex::FaceInfo& f);
    bool isCornerRegular(int faceid, bool uHigh, bool vHigh);

    void evalLargeDu(float du, float weight);
    void evalLargeDuFace(int faceid, int level, float weight);

    PtexTexture* _tx;		// texture being evaluated
    double* _result;		// temp result
    float _weight;		// accumulated weight of data in _result
    int _firstChanOffset;	// byte offset of first channel to eval
    int _nchan;			// number of channels to eval
    int _ntxchan;		// number of channels in texture
    DataType _dt;		// data type of texture
};

#endif
