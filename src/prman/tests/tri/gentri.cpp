#include <iostream>
#include <stdlib.h>
#include "Ptexture.h"
#include <stdio.h>

float randf()
{
    return random() * (1.0/(unsigned)(2<<31-1));
}

int main()
{
    int nchan = 3;
    std::string error;
    Ptex::DataType dt = Ptex::dt_float;
    int nfaces = 9;
    int alpha = -1;
    PtexPtr<PtexWriter> tx(PtexWriter::open("tri.ptx", Ptex::mt_triangle, dt, nchan,
					    alpha, nfaces, error));
    if (!tx) {
	std::cerr << error << std::endl;
	return 1;
    }

    int adjfaces [][4] = {
	-1, 1, -1, -1,
	0, 2, 5, -1,
	-1, 3, 1, -1,
	2, 4, 7, -1,
	-1, -1, 3, -1,
	1, 6, -1, -1,
	5, 7, 8, -1,
	3, -1, 6, -1,
	6, -1, -1, -1,
    };

    int adjedges [][4] = {
	0, 0, 0, 0,
	1, 2, 0, 0,
	0, 0, 1, 0,
	1, 2, 0, 0,
	0, 0, 1, 0,
	2, 0, 0, 0,
	1, 2, 0, 0,
	2, 0, 1, 0,
	2, 0, 0, 0,
    };

    for (int faceid = 0; faceid < nfaces; faceid++) {
	int level = 5;
	Ptex::Res res(level, level);
	int w = res.u();
	int ndata = res.size() * nchan;
	float data[ndata];
	double scale = 1.0/w;
	float* pixel = data;

	for (int vi = 0; vi < w; vi++) {
	    for (int ui = 0; ui < w; ui++) {
		float u = (ui + 1/3.0) * scale;
		float v = (vi + 1/3.0) * scale;
		if (ui + vi >= w) {
		    float utmp = u;
		    u = 1 - v;
		    v = 1 - utmp;
		}
		float r = randf(), g = randf(), b = randf();
		pixel[0] = r;//(ui + vi) & 1;
		pixel[1] = g;
		pixel[2] = b;
		if (faceid == 6) pixel[0] = pixel[1] = .5;
		pixel += 3;
	    }
	}
	tx->writeFace(faceid, Ptex::FaceInfo(res, adjfaces[faceid], adjedges[faceid]), data);
    }
    if (!tx->close(error)) {
	std::cerr << error << std::endl;
	return 1;
    }
    return 0;
}
