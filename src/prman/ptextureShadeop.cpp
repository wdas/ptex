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
	    filter->eval(*result, chan, 3, tx, int(*faceid), *u, *v, *uw, *vw);

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
    {0, 0, 0, 0}
};


RSLEXPORT RslFunctionTable RslPublicFunctions(ptexFunctions, initPtexCache, termPtexCache);
