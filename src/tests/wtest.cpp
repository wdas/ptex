#include <alloca.h>
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include "Ptexture.h"

int main()
{
    static Ptex::Res res[] = { Ptex::Res(8,7),
			       Ptex::Res(1,2),
			       Ptex::Res(3,1),
			       Ptex::Res(5,4),
			       Ptex::Res(9,8),
			       Ptex::Res(2,4),
			       Ptex::Res(6,2),
			       Ptex::Res(7,4),
			       Ptex::Res(2,1),
 };
    static int adjedges[][4] = {{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 },
				{ 2, 3, 0, 1 }};
    static int adjfaces[][4] ={{ 3, 1, -1, -1 },
			       { 4, 2, -1, 0 },
			       { 5, -1, -1, 1 },
			       { 6, 4, 0, -1 },
			       { 7, 5, 1, 3 },
			       { 8, -1, 2, 4 },
			       { -1, 7, 3, -1 },
			       { -1, 8, 4, 6 },
			       { -1, -1, 5, 7 }};

    int nfaces = sizeof(res)/sizeof(res[0]);
    Ptex::DataType dt = Ptex::dt_float;
    int alpha = -1;
    int nchan = 3;

    Ptex::String error;
    PtexWriter* w = 
	PtexWriter::open("test.ptx", Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    if (!w) {
	std::cerr << error.c_str() << std::endl;
	return 1;
    }
    int size = 0;
    for (int i = 0; i < nfaces; i++)
	size = std::max(size, res[i].size());
    size *= Ptex::DataSize(dt) * nchan;

    void* buff = malloc(size);
    for (int i = 0; i < nfaces; i++)
    {
	memset(buff, 0, size);
	float* fbuff = (float*)buff;
#if 0
	//uint8_t* fbuff = (uint8_t*)buff;
	for (int v = 0; v < 256; v++) {
	    for (int u = 0; u < 256; u++) {
		if (1) { // u >= 16 || v > 16) {
		    fbuff[(v*256+u)*3 + 1] = (v) /255.0;
		    fbuff[(v*256+u)*3 + 2] = (u) /255.0;
		}
	    }
	}
#endif
	
	if (1) { // i!=7) { //i == 0) {
	    int ures = res[i].u(), vres = res[i].v();
	    for (int v = 0; v < vres; v++) {
		for (int u = 0; u < ures; u++) {
		    float c = (u ^ v) & 1;
		    //c = ((i/3)^(i%3))&1 ? .8 : .2;
		    fbuff[(v*ures+u)*nchan] = u/float(ures-1);
		    fbuff[(v*ures+u)*nchan+1] = v/float(vres-1);
		    fbuff[(v*ures+u)*nchan+2] = c;
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

    w->writeMeta("hello", "goodbye");
    double vals[3] = { 1.1,2.2,3.3 };
    w->writeMeta("flarf", vals, 3);
    int16_t ivals[4] = { 2, 4, 6, 8 };
    w->writeMeta("flarfi", ivals, 4);

    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
    }

    w->release();

    // add some edits
    w = PtexWriter::edit("test.ptx", true, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    w->writeMeta("hello", "ciao");
    w->writeMeta("flarf", "boodle");
    vals[2] = 0;
    w->writeMeta("yahoo", vals, 3);
    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
    }
    w->release();

    // add some more edits
    w = PtexWriter::edit("test.ptx", false, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    w->writeMeta("hello", "aloha");
#if 0
    float fvals[] = {0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1, 0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1, 0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1, 0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1};
    w->writeConstantFace(1, Ptex::Res(7,8), fvals);
    w->writeFace(2, Ptex::Res(2, 2), fvals+3);
#endif
    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
    }
    w->release();

    return 0;
}
