#ifndef PtexTriangleFilter_h
#define PtexTriangleFilter_h

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
class PtexTriangleKernel;

class PtexTriangleFilter : public PtexFilter, public Ptex
{
 public:
    PtexTriangleFilter(PtexTexture* tx, const PtexFilter::Options& opts ) :
	_tx(tx), _options(opts), _result(0), _weight(0), 
	_firstChanOffset(0), _nchan(0), _ntxchan(0),
	_dt((DataType)0) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
		      int faceid, float u, float v,
		      float uw1, float vw1, float uw2, float vw2, 
		      float width, float blur);

 protected:
    void buildKernel(PtexTriangleKernel& k, float u, float v, 
		     float uw1, float vw1, float uw2, float vw2,
		     float width, float blur, Res faceRes);

    void splitAndApply(PtexTriangleKernel& k, int faceid, const FaceInfo& f);
    void applyAcrossEdge(PtexTriangleKernel& k, const Ptex::FaceInfo& f, int eid);
    void apply(PtexTriangleKernel& k, int faceid, const Ptex::FaceInfo& f);

    virtual ~PtexTriangleFilter() {}

    PtexTexture* _tx;		// texture being evaluated
    Options _options;		// options
    double* _result;		// temp result
    double _weight;		// accumulated weight of data in _result
    int _firstChanOffset;	// byte offset of first channel to eval
    int _nchan;			// number of channels to eval
    int _ntxchan;		// number of channels in texture
    DataType _dt;		// data type of texture
};

#endif
