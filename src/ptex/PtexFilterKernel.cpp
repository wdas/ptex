#include "PtexFilterContext.h"
#include "PtexFilterKernel.h"
#include "PtexHalf.h"

struct PtexFilterKernel::Iter : public Ptex {
    const PtexFilterContext& ctx;

    int ustride;		// distance between u samples
    int vstride;		// distance between v samples
    int rowskip;		// distance to skip at end of row
    double* pos;		// current iterator pos
    double* rowend;		// end of current iterator row
    double* end;		// end of current iterator

    int dstart;			// offset to start of data
    int drowskip;		// amount of data to skip at end of row

    
    Iter(const PtexFilterKernel& k, int rotate, const PtexFilterContext& c)
	: ctx(c)
    {
	int resu = k.res.u(), resv = k.res.v();
	switch (rotate) {
	case 0:
	    ustride = 1;
	    vstride = k.stride;
	    rowskip = k.stride - k.uw;
	    pos = k.start;
	    rowend = pos + k.uw;
	    end = pos + vstride * k.vw;
	    dstart = k.v * resu + k.u;
	    drowskip = resu - k.uw;
	    break;

	case 1: // rotate kernel 90 deg cw relative to data
	    ustride = -k.stride;
	    vstride = 1;
	    rowskip = k.stride * k.vw + 1;
	    pos = k.start + k.stride * (k.vw-1);
	    rowend = k.start - k.stride;
	    end = pos + k.uw;
	    dstart = (resu - k.uw - k.u) * resv + k.v;
	    drowskip = resv - k.vw;
	    break;
		
	case 2: // rotate kernel 180 deg cw relative to data
	    ustride = -1;
	    vstride = -k.stride;
	    rowskip = k.uw - k.stride;
	    pos = k.start + k.stride * (k.vw-1) + k.uw-1;
	    rowend = pos - k.uw;
	    end = pos + vstride * k.vw;
	    dstart = (resv - k.vw - k.v) * resu + (resu - k.uw - k.u);
	    drowskip = resu = k.uw;
	    break;
		
	case 3: // rotate kernel 90 deg ccw relative to data
	    ustride = k.stride;
	    vstride = -1;
	    rowskip = -k.stride * k.vw - 1;
	    pos = k.start + k.stride * (k.vw-1);
	    rowend = k.start - k.stride;
	    end = pos + k.uw;
	    dstart = (resv - k.vw - k.v) * resu + k.u;
	    drowskip = resv - k.vw;
	    break;
	}
	dstart = dstart * c.ntxchannels + c.firstchan;
	drowskip *= c.ntxchannels;
    }

    double nextval()
    {
	double result = *pos; 
	pos += ustride;
	return result; 
    }

    bool rowdone()
    {
	if (pos != rowend) return 0; 
	rowend += vstride;
	pos += rowskip;
	return 1; 
    }

    bool done()
    {
	return pos == end;
    }
};


struct PtexFilterKernel::TileIter : public Ptex
{
    int index;			// current tile index
    PtexFilterKernel tile;	// kernel current tile

    //    TileIter(const PtexFilterKernel& k, int rotate, Res tileres)
    TileIter(const PtexFilterKernel&, int, Res)
    {
	// TODO
	index = 0;
	tile.set(0,0,0,0,0,0,0);
    }

    bool next()
    {
	return 0;
    }
};


void PtexFilterKernel::split(PtexFilterKernel& ku, PtexFilterKernel& kv,
			     PtexFilterKernel& kc) 
{
    // split off adjacent u, v, and corner pieces from kernel
    if (u < 0)                { splitL(ku); }
    else if (u+uw > res.u()) { splitR(ku); }
    if (v < 0)                { splitB(kv); if (ku) ku.splitB(kc); }
    else if (v+vw > res.v()) { splitT(kv); if (ku) ku.splitT(kc); }

    // for corner, set eid to edge leading in to corner (counterclockwise)
    if (kc) kc.eid = ((ku.eid+1)%4 == kv.eid) ? ku.eid : kv.eid;
}
	
	
void PtexFilterKernel::merge(PtexFilterKernel& k, EdgeId eid, bool extrapolate)
{
    k.valid = 0;

    if (extrapolate) {
	// merge weights of given kernel into this kernel along edge k.eid
	// scale weights according to extrapolation formula
	double s1, s2, *dst, *dst2;
	double* kp = k.start;
	int rowskip = k.stride - k.uw;
	switch (eid) {
	case e_bottom:
	    s1 = k.vw+1; s2 = -k.vw;
	    for (int i = 0; i < k.vw; i++, s1--, s2++, kp += rowskip) {
		dst = start;
		dst2 = dst + stride;
		for (int j = 0; j < k.uw; j++) {
		    *dst++ += *kp * s1;
		    *dst2++ += *kp++ * s2;
		}
	    }
	    break;
	case e_right:
	    dst = start + uw-2;
	    for (int i = 0; i < k.vw; i++, kp += rowskip, dst += stride) {
		s1 = 2; s2 = -1;
		for (int j = 0; j < k.uw; j++, s1++, s2--) {
		    dst[0] += *kp * s2;
		    dst[1] += *kp++ * s1;
		}
	    }
	    break;
	case e_top:
	    s1 = 2; s2 = -1;
	    for (int i = 0; i < k.vw; i++, s1++, s2--, kp += rowskip) {
		dst = start + (vw-1)*stride;
		dst2 = dst - stride;
		for (int j = 0; j < k.uw; j++) {
		    *dst++ += *kp * s1;
		    *dst2++ += *kp++ * s2;
		}
	    }
	    break;
	case e_left:
	    dst = start;
	    for (int i = 0; i < k.vw; i++, kp += rowskip, dst += stride) {
		s1 = k.uw+1; s2 = -k.uw;
		for (int j = 0; j < k.uw; j++, s1--, s2++) {
		    dst[0] += *kp * s1;
		    dst[1] += *kp++ * s2;
		}
	    }
	    break;
	}
    }
    else {
	// clamp edge values 
	// merge weights of given kernel into this kernel along edge k.eid
	double* dst; // start of destination edge
	int du, dv;  // offset to next pixel in u/v along edge
	switch (eid) {
	default:
	case e_bottom: dst = start; du = 1; dv = -uw; break;
	case e_right:  dst = start + uw-1; du = 0; dv = stride; break;
	case e_top:    dst = start + (vw-1)*stride; du = 1; dv = -uw; break;
	case e_left:   dst = start; du = 0; dv = stride; break;
	}

	double* kp = k.start;
	int rowskip = k.stride - k.uw;
	for (int i = 0; i < k.vw; i++, dst += dv, kp += rowskip)
	    for (int j = 0; j < k.uw; j++, dst += du) *dst += *kp++;
    }
}


namespace {
    // fixed length accumulator: result[i] += val[i] * weight
    template<typename T, int n>
    struct VecAccum {
	VecAccum() {}
	void operator()(float* result, const T* val, double weight) 
	{
	    *result += *val * weight;
	    // use template to unroll loop
	    VecAccum<T,n-1>()(result+1, val+1, weight);
	}
    };

    // loop terminator
    template<typename T>
    struct VecAccum<T,0> { void operator()(float*, const T*, double) {} };

    // variable length accumulator: result[i] += val[i] * weight
    template<typename T>
    struct VecAccumN {
	void operator()(float* result, const T* val, int nchan, double weight) 
	{
	    for (int i = 0; i < nchan; i++) result[i] += val[i] * weight;
	}
    };
    
    template<typename T>
    inline void ApplyConstT(void* data, const PtexFilterContext& c, double weight)
    {
	VecAccumN<T>()(c.result, ((T*)data)+c.firstchan, c.nchannels, weight);
    }

    inline void ApplyConst(void* data, const PtexFilterContext& c, double weight)
    {
	switch (c.dt) {
	case Ptex::dt_uint8:   ApplyConstT<uint8_t> (data, c, weight); break;
	case Ptex::dt_uint16:  ApplyConstT<uint16_t>(data, c, weight); break;
	case Ptex::dt_half:    ApplyConstT<PtexHalf>(data, c, weight); break;
	case Ptex::dt_float:   ApplyConstT<float>   (data, c, weight); break;
	}
    }

    template<typename T, int n>
    inline void ApplyT(void* data, PtexFilterKernel::Iter& i)
    {
	T* ptr = ((T*)data) + i.dstart;
	float* result = i.ctx.result;
	int ntxchan = i.ctx.ntxchannels;
	VecAccum<T,n> accum;
	do {
	    do { 
		accum(result, ptr, i.nextval()); 
		ptr += ntxchan;
	    } while (!i.rowdone());
	    ptr += i.drowskip;
	} while(!i.done());
    }

    template<typename T>
    inline void ApplyT(void* data, PtexFilterKernel::Iter& i)
    {
	T* ptr = ((T*)data) + i.dstart;
	float* result = i.ctx.result;
	int nchan = i.ctx.nchannels;
	int ntxchan = i.ctx.ntxchannels;
	VecAccumN<T> accum;
	do {
	    do { 
		accum(result, ptr, nchan, i.nextval()); 
		ptr += ntxchan;
	    } while (!i.rowdone());
	    ptr += i.drowskip;
	} while(!i.done());
    }
	

    inline void Apply(void* data, PtexFilterKernel::Iter& i)
    {
	switch((i.ctx.nchannels-1)<<2|i.ctx.dt) {
	case (0<<2|Ptex::dt_uint8):  ApplyT<uint8_t, 1>(data, i); break;
	case (0<<2|Ptex::dt_uint16): ApplyT<uint16_t,1>(data, i); break;
	case (0<<2|Ptex::dt_half):   ApplyT<PtexHalf,1>(data, i); break;
	case (0<<2|Ptex::dt_float):  ApplyT<float,   1>(data, i); break;
	case (1<<2|Ptex::dt_uint8):  ApplyT<uint8_t, 2>(data, i); break;
	case (1<<2|Ptex::dt_uint16): ApplyT<uint16_t,2>(data, i); break;
	case (1<<2|Ptex::dt_half):   ApplyT<PtexHalf,2>(data, i); break;
	case (1<<2|Ptex::dt_float):  ApplyT<float,   2>(data, i); break;
	case (2<<2|Ptex::dt_uint8):  ApplyT<uint8_t, 3>(data, i); break;
	case (2<<2|Ptex::dt_uint16): ApplyT<uint16_t,3>(data, i); break;
	case (2<<2|Ptex::dt_half):   ApplyT<PtexHalf,3>(data, i); break;
	case (2<<2|Ptex::dt_float):  ApplyT<float,   3>(data, i); break;
	case (3<<2|Ptex::dt_uint8):  ApplyT<uint8_t, 4>(data, i); break;
	case (3<<2|Ptex::dt_uint16): ApplyT<uint16_t,4>(data, i); break;
	case (3<<2|Ptex::dt_half):   ApplyT<PtexHalf,4>(data, i); break;
	case (3<<2|Ptex::dt_float):  ApplyT<float,   4>(data, i); break;
	default:
	    switch (i.ctx.dt) {
	    case Ptex::dt_uint8:  ApplyT<uint8_t> (data, i); break;
	    case Ptex::dt_uint16: ApplyT<uint16_t>(data, i); break;
	    case Ptex::dt_half:   ApplyT<PtexHalf>(data, i); break;
	    case Ptex::dt_float:  ApplyT<float>   (data, i); break;
	    }
	}
    }
}


void PtexFilterKernel::apply(int faceid, int rotate, const PtexFilterContext& c) const
{
    PtexFaceData* dh = c.tx->getData(faceid, rotate & 1 ? res : res.swappeduv());
    if (!dh) return;

    if (dh->isConstant()) {
	ApplyConst(dh->getData(), c, totalWeight());
    }
    else if (dh->isTiled()) {
	TileIter tileiter(*this, rotate, dh->tileRes());
	do {
	    PtexFaceData* th = dh->getTile(tileiter.index);
	    if (th->isConstant()) {
		ApplyConst(th->getData(), c, totalWeight());
	    }
	    else {
		Iter iter(tileiter.tile, rotate, c);
		Apply(th->getData(), iter);
	    }
	} while (tileiter.next());
    }
    else {
	Iter iter(*this, rotate, c);
	Apply(dh->getData(), iter);
    }
    dh->release();
}
