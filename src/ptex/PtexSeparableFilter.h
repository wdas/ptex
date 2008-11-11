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
	_result(0), _tx(0), _weight(0), 
	_firstchan(0), _nchan(0), _ntxchan(0),
	_dt((DataType)0) {}
    virtual ~PtexSeparableFilter() {}

    virtual int kernelWidth(float filterWidthInPixels) = 0;
    virtual void buildKernel(double* weights, int kw) = 0;
    
    void apply(PtexSeparableKernel& k, int faceid, Ptex::FaceInfo& f);
    void evalLargeDu(float du, float weight);
    void evalLargeDuFace(int faceid, int level, float weight);

    PtexTexture* _tx;		// texture being evaluated
    double* _result;		// temp result
    float _weight;		// accumulated weight of data in _result
    int _firstchan;		// first channel to eval
    int _nchan;			// number of channels to eval
    int _ntxchan;		// number of channels in texture
    DataType _dt;		// data type of texture
};

#endif
