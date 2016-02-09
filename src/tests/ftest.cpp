#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "Ptexture.h"
using namespace Ptex;

int main(int argc, char** argv)
{
    int maxmem = argc >= 2 ? atoi(argv[1]) : 1024*1024;
    PtexPtr<PtexCache> c(PtexCache::create(0, maxmem));

    Ptex::String error;
    PtexPtr<PtexTexture> r ( c->get("test.ptx", error) );

    if (!r) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }

    PtexFilter::Options opts(PtexFilter::f_bicubic, 0, 1.0);
    PtexPtr<PtexFilter> f ( PtexFilter::getFilter(r, opts) );
    float result[4];
    int faceid = 0;
    float u=0, v=0, uw=.125, vw=.125;
    for (v = 0; v <= 1; v += .125) {
        for (u = 0; u <= 1; u += .125) {
            f->eval(result, 0, 1, faceid, u, v, uw, 0, 0, vw);
            printf("%8f %8f -> %8f\n", u, v, result[0]);
        }
    }

    return 0;
}
