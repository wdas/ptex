#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include "Ptexture.h"
#include "PtexHalf.h"

int main(int /*argc*/, char** /*argv*/)
{
    static Ptex::Res res[] = { Ptex::Res(8,7),
			       Ptex::Res(1,2),
			       Ptex::Res(3,1),
			       Ptex::Res(5,4),
			       Ptex::Res(9,8),
			       Ptex::Res(2,4),
			       Ptex::Res(6,2),
			       Ptex::Res(7,4),
			       Ptex::Res(2,1)};
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
    Ptex::DataType dt = Ptex::dt_half;//float;
//#define DTYPE float
#define DTYPE PtexHalf
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
	DTYPE* fbuff = (DTYPE*)buff;
	int ures = res[i].u(), vres = res[i].v();
	for (int v = 0; v < vres; v++) {
	    for (int u = 0; u < ures; u++) {
		float c = (u ^ v) & 1;
		fbuff[(v*ures+u)*nchan] = u/float(ures-1);
		fbuff[(v*ures+u)*nchan+1] = v/float(vres-1);
		fbuff[(v*ures+u)*nchan+2] = c;
	    }
	}

	w->writeFace(i, Ptex::FaceInfo(res[i], adjfaces[i], adjedges[i]), buff);
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

    // add some incremental edits
    w = PtexWriter::edit("test.ptx", true, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    w->writeMeta("hello", "ciao");
    w->writeMeta("flarf", "boodle");
    vals[2] = 0;
    w->writeMeta("yahoo", vals, 3);
    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
    }
    w->release();

    // add some non-incremental edits
    w = PtexWriter::edit("test.ptx", false, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    w->writeMeta("hello", "aloha");
    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
    }
    w->release();

    return 0;
}
