#include <alloca.h>
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include "Ptexture.h"

int main()
{
    static Ptex::Res res[] = { Ptex::Res(2,2),
			       Ptex::Res(2,3),
			       Ptex::Res(2,4),
			       Ptex::Res(8,8), };
    static int adjedges[][4] = {{ 0, 2, 0, 8 },
				{ 0, 0, 0, 1 },
				{ 1, 0, 0, 1 },
				{ 2, 3, 0, 0 }};
    static int adjfaces[][4] ={{ -1, 1, 3, -1 },
			       { -1, -1, 2, 0 },
			       { 1, -1, -1, 3 },
			       { 0, 2, -1, -1 }};

    int nfaces = 4;
    Ptex::DataType dt = Ptex::dt_float;
    int alpha = -1;
    int nchan = 3;

    std::string error;
    PtexWriter* w = 
	PtexWriter::open("test.ptx", Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    if (!w) {
	std::cerr << error << std::endl;
	return 1;
    }
    int size = 0;
    for (int i = 0; i < nfaces; i++)
	size = std::max(size, res[i].size());
    size *= Ptex::DataSize(dt) * nchan;

    void* buff = malloc(size);
    memset(buff, 0, size);
    for (int i = 0; i < nfaces; i++)
    {
	float* fbuff = (float*)buff;
	//uint8_t* fbuff = (uint8_t*)buff;
	for (int v = 0; v < 256; v++) {
	    for (int u = 0; u < 256; u++) {
		if (1) { // u >= 16 || v > 16) {
		    fbuff[(v*256+u)*3 + 1] = (v) /255.0;
		    fbuff[(v*256+u)*3 + 2] = (u) /255.0;
		}
	    }
	}
	
#if 0
	((char*)buff)[0] = i>>16;
	((char*)buff)[1] = i>>8;
	((char*)buff)[2] = i;
	if (i == 2) {
	    for (int j = 0; j < size; j++)
		((char*)buff)[j] = j % 5;
	}

	if (i == 3) {
	    memset(buff, 0, size);
	    // 	    for (int v = 0; v < 257; v++) {
	    // 		for (int u = 0; u < 128; u++) {
	    // 		    ((char*)buff)[3*(v*4096+u)] = u;
	    // 		    ((char*)buff)[3*(v*4096+u)+1] = v;
	    // 		}
	    // 	    }
	    // 		((char*)buff)[j] = j % 1000;
	    //  	    ((char*)buff)[12] = 12;
	    // 	    ((char*)buff)[128] = 0x34;
	    for (int i = 0; i < 1000; i++){
		int n = i*3;//random() % (4096*4096*3);
		((char*)buff)[n] = random();
		// 		((char*)buff)[n+1] = random();
		// 		((char*)buff)[n+2] = random();
	    }
	}
#endif

	w->writeFace(i, Ptex::FaceInfo(res[i], adjfaces[i], adjedges[i]), buff);
	//w->writeConstantFace(buff);
    }
    free(buff);

#if 1
    w->writeMeta("hello", "goodbye");
    double vals[3] = { 1.1,2.2,3.3 };
    w->writeMeta("flarf", vals, 3);
    int16_t ivals[4] = { 2, 4, 6, 8 };
    w->writeMeta("flarfi", ivals, 4);
#endif

    if (!w->close(error)) {
	std::cerr << error << std::endl;
    }

    w->release();

#if 1
    // add some edits
    w = PtexWriter::edit("test.ptx", true, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    w->writeMeta("hello", "ciao");
    w->writeMeta("flarf", "boodle");
    vals[2] = 0;
    w->writeMeta("yahoo", vals, 3);
    if (!w->close(error)) {
	std::cerr << error << std::endl;
    }
    w->release();
#endif

#if 1
    // add some more edits
    w = PtexWriter::edit("test.ptx", false, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    w->writeMeta("hello", "aloha");
    float fvals[] = {0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1, 0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1, 0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1, 0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1};
    w->writeConstantFace(1, Ptex::Res(7,8), fvals);
    w->writeFace(2, Ptex::Res(2, 2), fvals+3);
    if (!w->close(error)) {
	std::cerr << error << std::endl;
    }
    w->release();
#endif

    return 0;
}
