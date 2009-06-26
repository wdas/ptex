#include <iostream>
#include <stdlib.h>
#include "Ptexture.h"

int main()
{
    int nchan = 3;
    std::string error;
    Ptex::DataType dt = Ptex::dt_float;
    int nfaces = 60;
    int alpha = -1;
    PtexPtr<PtexWriter> tx(PtexWriter::open("tri.ptx", Ptex::mt_triangle, dt, nchan,
					    alpha, nfaces, error));
    if (!tx) {
	std::cerr << error << std::endl;
	return 1;
    }
    int r = 3;
    Ptex::Res res(r, r);
    int w = res.u();
    int ndata = res.size() * nchan;
    float data[ndata], *pixel = data;
    double scale = 1.0/w;
    for (int vi = 0; vi < w; vi++) {
	for (int ui = 0; ui < w; ui++) {
	    float u = (ui + 1/3.0) * scale;
	    float v = (vi + 1/3.0) * scale;
	    if (ui + vi >= w) {
		float utmp = u;
		u = 1 - v;
		v = 1 - utmp;
	    }
	    pixel[0] = u;
	    pixel[1] = v;
	    pixel[2] = 1-u-v;
	    pixel += 3;
	}
    }

    for (int i = 0; i < nfaces; i++)
	tx->writeFace(i, res, data);
    if (!tx->close(error)) {
	std::cerr << error << std::endl;
	return 1;
    }
    return 0;
}
