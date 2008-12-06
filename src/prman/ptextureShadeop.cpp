/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
/* ptexture prman shadeop - Brent Burley, Oct 2006
 */


#include <math.h>
#include <iostream>
#include <RslPlugin.h>
#include <rx.h>
#include <Ptexture.h>

static PtexCache* cache = 0;

static void initPtexCache(RixContext *)
{ 
    if (!cache) {
	int maxfiles = 1000; // open file handles
	int maxmem = 100;    // memory (MB)
	char* maxfilesenv = getenv("PTEX_MAXFILES");
	if (maxfilesenv) {
	    int val = atoi(maxfilesenv);
	    if (val) {
		std::cout << "Ptex cache size overridden by PTEX_MAXFILES, "
			  << "file limit changed from " << maxfiles
			  << " to " << val << std::endl;
		maxfiles = val;
	    }
	}
	char* maxmemenv = getenv("PTEX_MAXMEM");
	if (maxmemenv) {
	    int val = atoi(maxmemenv);
	    if (val) {
		std::cout << "Ptex cache size overridden by PTEX_MAXMEM, "
			  << "mem limit changed from " << maxmem
			  << " to " << val << " (MB)" << std::endl;
		maxmem = val;
	    }
	}
	cache = PtexCache::create(maxfiles, maxmem*1024*1024);

	// init the search path
	static char* path = 0;
	RxInfoType_t type;
	int count;
	// look for a texture path
	int status = RxOption("searchpath:texture", &path, sizeof(char*),
			      &type, &count);
	if (status != 0 || type != RxInfoStringV)
	    // not found - look for general resourcepath instead
	    status = RxOption("searchpath:resourcepath", &path, sizeof(char*),
			      &type, &count);
	if (status != 0 || type != RxInfoStringV) path = 0;

	if (path) {
	    cache->setSearchPath(path);
	}
    }
}

static void termPtexCache(RixContext *)
{
    cache->release();
    cache = 0;
}

/* 
 * This has been replaced by ptextureDSO which is a more general call 
 */
static int ptextureColor(RslContext*, int argc, const RslArg* argv[] )
{
    RslPointIter result(argv[0]);
    RslStringIter mapname(argv[1]);
    RslFloatIter channel(argv[2]);
    RslFloatIter faceid(argv[3]);
    RslFloatIter u(argv[4]);
    RslFloatIter v(argv[5]);
    RslFloatIter uw(argv[6]);
    RslFloatIter vw(argv[7]);
    RslFloatIter sharpness(argv[8]);

    float sharp = *sharpness;

    std::string error;
    PtexTexture* tx = cache->get(*mapname, error);
    if (tx) {
	int chan = int(*channel);
	PtexFilter* filter = PtexFilter::mitchell(sharp);
	int numVals = RslArg::NumValues(argc, argv);
	for (int i = 0; i < numVals; ++i) {
	    float* resultval = *result;
	    filter->eval(resultval, chan, 3, tx, int(*faceid), *u, *v, *uw, *vw);

	    // copy first channel into missing channels (e.g. promote 1-chan to gray)
	    for (int i = chan + tx->numChannels(); i < 3; i++)
		resultval[i] = resultval[0];

	    ++result; ++faceid; ++u; ++v; ++uw; ++vw;
	}
	filter->release();
	tx->release();
    }
    else {
	if (!error.empty()) std::cerr << error << std::endl;
	int numVals = RslArg::NumValues(argc, argv);
	for (int i = 0; i < numVals; ++i) {
	    float* c = *result;
	    c[0] = c[1] = c[2] = 0;
	    ++result;
	}
    }

    return 0;
}


static int ptextureFloat(RslContext*, int argc, const RslArg* argv[] )
{
    RslPointIter result(argv[0]);
    RslStringIter mapname(argv[1]);
    RslFloatIter channel(argv[2]);
    RslFloatIter faceid(argv[3]);
    RslFloatIter u(argv[4]);
    RslFloatIter v(argv[5]);
    RslFloatIter uw(argv[6]);
    RslFloatIter vw(argv[7]);
    RslFloatIter sharpness(argv[8]);

    std::string error;
    PtexTexture* tx = cache->get(*mapname, error);
    if (tx) {
	int chan = int(*channel);
	PtexFilter* filter = PtexFilter::mitchell(*sharpness);
	int numVals = RslArg::NumValues(argc, argv);
	for (int i = 0; i < numVals; ++i) {
	    filter->eval(*result, chan, 1, tx, int(*faceid), *u, *v, *uw, *vw);
	    ++result; ++faceid; ++u; ++v; ++uw; ++vw;
	}
	filter->release();
	tx->release();
    }
    else {
	if (!error.empty()) std::cerr << error << std::endl;
	int numVals = RslArg::NumValues(argc, argv);
	for (int i = 0; i < numVals; ++i) {
	    float* c = *result;
	    c[0] = 0;
	    ++result;
	}
    }

    return 0;
}


namespace {
    inline float min(float a, float b) { return a < b ? a : b; }
    inline float max(float a, float b) { return a > b ? a : b; }
    inline float max4(float a, float b, float c, float d) { return max(max(a,b),max(c,d)); }
    inline float min4(float a, float b, float c, float d) { return min(min(a,b),min(c,d)); }
    inline float range4(float a, float b, float c, float d) { return max4(a,b,c,d)-min4(a,b,c,d); }
}

static int ptexenvColor(RslContext*, int argc, const RslArg* argv[] )
{
    if (argc != 8) return 1;

    RslColorIter result(argv[0]);
    RslStringIter mapname(argv[1]);
    RslFloatIter channel(argv[2]);
    RslVectorIter R0(argv[3]);
    RslVectorIter R1(argv[4]);
    RslVectorIter R2(argv[5]);
    RslVectorIter R3(argv[6]);
    RslFloatIter blur(argv[7]);

    float sharp = 0;

    std::string error;
    PtexTexture* tx = cache->get(*mapname, error);
    if (tx) {
	int chan = int(*channel);
	PtexFilter* filter = PtexFilter::mitchell(sharp);
	int numVals = RslArg::NumValues(argc, argv);
	for (int i = 0; i < numVals; ++i) {
	    float* resultval = *result;
	    float *r0 = *R0, *r1 = *R1, *r2 = *R2, *r3 = *R3;
	    float x0 = r0[0], y0 = r0[1], z0 = r0[2];
	    float x1 = r1[0], y1 = r1[1], z1 = r1[2];
	    float x2 = r2[0], y2 = r2[1], z2 = r2[2];
	    float x3 = r3[0], y3 = r3[1], z3 = r3[2];
	    float x = (x0+x1+x2+x3), y = (y0+y1+y2+y3), z = (z0+z1+z2+z3);
	    float ax=fabs(x), ay=fabs(y), az=fabs(z);
	    int faceid;
	    float u, v, du, dv;
	    if (ax > ay && ax > az) {
		if (x > 0) { faceid=0; u = -z/x; v =  y/x; } // px
		else       { faceid=1; u = -z/x; v = -y/x; } // nx
		du = range4(z0/x0, z1/x1, z2/x2, z3/x3);
		dv = range4(y0/x0, y1/x1, y2/x2, y3/x3);
	    }
	    else if (ay > ax && ay > az) {
		if (y > 0) { faceid=2; u =  x/y; v = -z/y; } // py
		else       { faceid=3; u = -x/y; v = -z/y; } // ny
		du = range4(x0/y0, x1/y1, x2/y2, x3/y3);
		dv = range4(z0/y0, z1/y1, z2/y2, z3/y3);
	    }
	    else if (az > 0) {
		if (z > 0) { faceid=4; u =  x/z; v =  y/z; } // pz
		else       { faceid=5; u =  x/z; v = -y/z; } // nz
		du = range4(x0/z0, x1/z1, x2/z2, x3/z3);
		dv = range4(y0/z0, y1/z1, y2/z2, y3/z3);
	    }
	    else {
		u = v = du = dv = 0;    
	    }
	    filter->eval(resultval, chan, 3, tx, faceid, (1+u)/2, (1+v)/2, 
			 du/2 + *blur, dv/2 + *blur);

	    // copy first channel into missing channels (e.g. promote 1-chan to gray)
	    for (int i = chan + tx->numChannels(); i < 3; i++)
		resultval[i] = resultval[0];

	    ++result; ++R0; ++R1; ++R2; ++R3; ++blur;
	}
	filter->release();
	tx->release();
    }
    else {
	if (!error.empty()) std::cerr << error << std::endl;
	int numVals = RslArg::NumValues(argc, argv);
	for (int i = 0; i < numVals; ++i) {
	    float* c = *result;
	    c[0] = c[1] = c[2] = 0;
	    ++result;
	}
    }

    return 0;
}

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
static RslFunction ptexFunctions[] =
{
    // color = ptexture(mapname, chan, faceid, u, v, uw, vw, sharpness)
    { "color ptexture(string, float, float, float, float, float, float, float)",
      ptextureColor, 0, 0 },

    // float = ptexture(mapname, chan, faceid, u, v, uw, vw, sharpness)
    { "float ptexture(string, float, float, float, float, float, float, float)",
      ptextureFloat, 0, 0 },

    // color = ptexenv(mapname, R0, R1, R2, R3, blur)
    { "color ptexenv(string, uniform float, vector, vector, vector, vector, float)",
      ptexenvColor, 0, 0 },
    {0, 0, 0, 0}
};


RSLEXPORT RslFunctionTable RslPublicFunctions(ptexFunctions, initPtexCache, termPtexCache);