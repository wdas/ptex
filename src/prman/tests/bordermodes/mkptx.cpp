#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include "Ptexture.h"
#include "PtexHalf.h"
#include <limits.h>

int main(int /*argc*/, char** /*argv*/)
{
    static Ptex::Res res = Ptex::Res(2,2);
    static int adjedges[4] = { 0, 0, 0, 0 };
    static int adjfaces[4] = { -1, -1, -1, -1 };
    static int adjedgesPeriodic[4] = { 2, 3, 0, 1 };
    static int adjfacesPeriodic[4] = { 0, 0, 0, 0 };

    int nfaces = 1;
    Ptex::DataType dt = Ptex::dt_half;
    int alpha = -1;
    int nchan = 3;

    int size = res.size() * Ptex::DataSize(dt) * nchan;
    void* buff = malloc(size);
    memset(buff, 0, size);
    PtexHalf* fbuff = (PtexHalf*)buff;
    int ures = res.u(), vres = res.v();
    for (int v = 0; v < vres; v++) {
	for (int u = 0; u < ures; u++) {
	    fbuff[(v*ures+u)*nchan] = random() * (1.0/RAND_MAX);
	    fbuff[(v*ures+u)*nchan+1] = random() * (1.0/RAND_MAX);
	    fbuff[(v*ures+u)*nchan+2] = random() * (1.0/RAND_MAX);
	}
    }

    Ptex::String error;
    PtexWriter* w = PtexWriter::open("clamp.ptx", Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    if (!w) {
	std::cerr << error.c_str() << std::endl;
	return 1;
    }
    w->writeFace(0, Ptex::FaceInfo(res, adjfaces, adjedges), buff);
    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
	return 1;
    }

    w = PtexWriter::open("black.ptx", Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    if (!w) {
	std::cerr << error.c_str() << std::endl;
	return 1;
    }
    w->setBorderModes(Ptex::m_black, Ptex::m_black);
    w->writeFace(0, Ptex::FaceInfo(res, adjfaces, adjedges), buff);
    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
	return 1;
    }

    w = PtexWriter::open("periodic.ptx", Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    if (!w) {
	std::cerr << error.c_str() << std::endl;
	return 1;
    }
    w->setBorderModes(Ptex::m_periodic, Ptex::m_periodic);
    w->writeFace(0, Ptex::FaceInfo(res, adjfacesPeriodic, adjedgesPeriodic), buff);
    if (!w->close(error)) {
	std::cerr << error.c_str() << std::endl;
	return 1;
    }

    free(buff);
    return 0;
}
