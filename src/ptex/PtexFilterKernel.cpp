/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include "PtexFilterContext.h"
#include "PtexFilterKernel.h"
#include "PtexHalf.h"

/* Kernel iterator

   Allows moving through the kernel while moving through the data
   following the in-memory order of the data.  Since the kernel may be
   rotated with respect to the data, the iterator allows moving
   through the kernel in arbitrary order which is accomplished by
   having both a ustride and a vstride, either of which may be
   positive or negative.
*/

class PtexFilterKernel::Iter : public Ptex {
 public:
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
	case 0: // no rotation
	    ustride = 1;
	    vstride = k.stride;
	    pos = k.start;
	    rowend = pos + k.uw;
	    end = pos + vstride * k.vw;
	    dstart = k.v * resu + k.u;
	    drowskip = resu - k.uw;
	    break;

	case 1: // rotate kernel 90 deg cw relative to data
	    ustride = -k.stride;
	    vstride = 1;
	    pos = k.start + k.stride * (k.vw-1);
	    rowend = k.start - k.stride;
	    end = pos + k.uw;
	    dstart = k.u * resv + (resv - k.vw - k.v);
	    drowskip = resv - k.vw;
	    break;
		
	case 2: // rotate kernel 180 deg cw relative to data
	    ustride = -1;
	    vstride = -k.stride;
	    pos = k.start + k.stride * (k.vw-1) + k.uw-1;
	    rowend = pos - k.uw;
	    end = pos + vstride * k.vw;
	    dstart = (resv - k.vw - k.v) * resu + (resu - k.uw - k.u);
	    drowskip = resu - k.uw;
	    break;
		
	case 3: // rotate kernel 90 deg ccw relative to data
	    ustride = k.stride;
	    vstride = -1;
	    pos = k.start + k.uw-1;
	    rowend = pos + k.vw * k.stride;
	    end = pos - k.uw;
	    dstart = (resu - k.uw - k.u) * resv + k.v;
	    drowskip = resv - k.vw;
	    break;
	}
	rowskip = int(pos - rowend) + vstride;
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


class PtexFilterKernel::TileIter : public Ptex
{
 public:
    PtexFilterKernel kernels[4];  // kernel split over tiles
    int tiles[4];		  // tiles covered by kernel
    int ntiles;			  // number of tiles
    int index;			  // current index

    TileIter(const PtexFilterKernel& k, int rotate, Res tileres)
	: index(0)
    {
	// find first tile (ignoring rotation for now)
	int tileu = k.u >> tileres.ulog2; 
	int tilev = k.v >> tileres.vlog2;
	int ntilesu = k.res.ntilesu(tileres);
	int ntilesv = k.res.ntilesv(tileres);

	// find updated u, v within first tile
	int u = k.u - tileu * tileres.u();
	int v = k.v - tilev * tileres.v();

	// set primary kernel entry and tile offsets
	kernels[0].set(tileres, u, v, k.uw, k.vw, k.start, k.stride);
	int tilesu[4], tilesv[4];
	tilesu[0] = tileu; tilesv[0] = tilev;
	ntiles = 1;

	// split kernel across tile boundaries into (up to) 4 pieces
	PtexFilterKernel ku, kv, kc;
	kernels[0].split(ku, kv, kc);
	if (ku) {
	    kernels[ntiles] = ku;
	    tilesu[ntiles] = tileu + 1;
	    tilesv[ntiles] = tilev;
	    ntiles++;
	}
	if (kv) {
	    kernels[ntiles] = kv;
	    tilesu[ntiles] = tileu;
	    tilesv[ntiles] = tilev + 1;
	    ntiles++;
	}
	if (kc) {
	    kernels[ntiles] = kc;
	    tilesu[ntiles] = tileu + 1;
	    tilesv[ntiles] = tilev + 1;
	    ntiles++;
	}

	// calculate (rotated) tile indices
	switch (rotate) {
	default:
	case 0: // no rotation
	    for (int i = 0; i < ntiles; i++)
		tiles[i] = tilesv[i] * ntilesu + tilesu[i];
	    break;
	case 1: // rotate kernel 90 deg cw relative to data
	    for (int i = 0; i < ntiles; i++)
		tiles[i] = tilesu[i] * ntilesv + (ntilesv - 1 - tilesv[i]);
	    break;
	case 2: // rotate kernel 180 deg cw relative to data
	    for (int i = 0; i < ntiles; i++)
		tiles[i] = (ntilesv - 1 - tilesv[i]) * ntilesu
		    + (ntilesu - 1 - tilesu[i]);
	    break;
	case 3: // rotate kernel 90 deg ccw relative to data
	    for (int i = 0; i < ntiles; i++)
		tiles[i] = (ntilesu - 1 - tilesu[i]) * ntilesv + tilesv[i];
	    break;
	}
    }

    int tile() { return tiles[index]; }
    const PtexFilterKernel& kernel() { return kernels[index]; }
    bool next() { return ++index < ntiles; }
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
	
	
namespace {
    // fixed length accumulator: result[i] += val[i] * weight
    template<typename T, int n>
    struct VecAccum {
	VecAccum() {}
	void operator()(float* result, const T* val, double weight) 
	{
	    *result = float(*result + *val * weight);
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
	    for (int i = 0; i < nchan; i++) 
		result[i] = float(result[i] + val[i] * weight);
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
    PtexPtr<PtexFaceData> dh ( c.tx->getData(faceid, (rotate & 1) ? res.swappeduv() : res) );
    if (!dh) return;

    if (dh->isConstant()) {
	ApplyConst(dh->getData(), c, totalWeight());
    }
    else if (dh->isTiled()) {
	Res tres = dh->tileRes();
	TileIter tileiter(*this, rotate, (rotate & 1) ? tres.swappeduv() : tres);
	do {
	    PtexPtr<PtexFaceData> th ( dh->getTile(tileiter.tile()) );
	    if (th->isConstant()) {
		ApplyConst(th->getData(), c, tileiter.kernel().totalWeight());
	    }
	    else {
		Iter iter(tileiter.kernel(), rotate, c);
		Apply(th->getData(), iter);
	    }
	} while (tileiter.next());
    }
    else {
	Iter iter(*this, rotate, c);
	Apply(dh->getData(), iter);
    }
}


void PtexFilterKernel::applyConst(void* data, const PtexFilterContext& c, double weight)
{
    ApplyConst(data, c, weight);
}

