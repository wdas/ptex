#include <string>
#include <alloca.h>
#include <iostream>
#include "Ptexture.h"

int main(int argc, char** argv)
{
    int maxmem = argc >= 2 ? atoi(argv[1]) : 1024*1024;
    PtexCache* c = PtexCache::create(0, maxmem);

    std::string error;
    PtexTexture* r = c->get("test.ptx", error);

    if (!r) {
	std::cerr << error << std::endl;
	return 1;
    }

    PtexFilter* f = PtexFilter::mitchell(1.0);
    float result[4];
    int faceid = 0;
    float u=0, v=0, uw=1, vw=1;
    for (v = 0; v <= 1; v += .125) {
	for (u = 0; u <= 1; u += .125) {
	    f->eval(result, 0, 1, r, faceid, u, v, uw, vw);
	    printf("%8f %8f -> %8f\n", u, v, result[0]);
	}
    }

    f->release();
    r->release();
    return 0;
}
