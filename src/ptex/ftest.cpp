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
    float result[1];
    int faceid = 0;
    float u=0, v=0, uw=1, vw=1;
    f->eval(result, 0, 1, r, faceid, u, v, uw, vw);

    f->release();
    r->release();
    return 0;
}
