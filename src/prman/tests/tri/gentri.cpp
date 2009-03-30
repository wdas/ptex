#include <iostream>
#include <stdlib.h>
#include "Ptexture.h"

int main()
{
    int nchan = 3;
    std::string error;
    Ptex::DataType dt = Ptex::dt_uint8;
    PtexPtr<PtexWriter> tx(PtexWriter::open("tri.ptx", Ptex::mt_triangle, dt, nchan, -1, 2, error));
    if (!tx) {
	std::cerr << error << std::endl;
	return 1;
    }
    Ptex::Res res(3, 3);
    int w = res.u();
    int ndata = res.size() * nchan;
    uint8_t data[ndata], *pixel = data;
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
	    pixel[0] = int(u * 255);
	    pixel[1] = int(v * 255);
	    pixel[2] = int((1-u-v) * 255);
	    pixel += 3;
	}
    }

    tx->writeFace(0, res, data);
    //    tx->writeFace(1, res, data);
    if (!tx->close(error)) {
	std::cerr << error << std::endl;
	return 1;
    }
    return 0;
}
