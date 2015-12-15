#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include "Ptexture.h"
#include "PtexHalf.h"
#include <string.h>
using namespace Ptex;

void writeMeta(PtexWriter* w,
               const char* sval, double* dvals, int ndvals, int16_t* ivals, int nivals,
               const char* xval)
{
    if (sval) w->writeMeta("sval", sval);
    if (dvals) w->writeMeta("dvals", dvals, ndvals);
    if (ivals) w->writeMeta("ivals", ivals, nivals);
    if (xval) w->writeMeta("xval", xval);
}


bool checkMeta(const char* path,
               const char* sval, double* dvals, int ndvals, int16_t* ivals, int nivals,
               const char* xval)
{
    Ptex::String error;
    PtexPtr<PtexTexture> tx(PtexTexture::open(path, error));
    if (!tx) {
        std::cerr << error.c_str() << std::endl;
        return 0;
    }
    PtexPtr<PtexMetaData> meta(tx->getMetaData());

    const char* f_sval;
    meta->getValue("sval", f_sval);

    const double* f_dvals;
    int f_ndvals;
    meta->getValue("dvals", f_dvals, f_ndvals);

    const int16_t* f_ivals;
    int f_nivals;
    meta->getValue("ivals", f_ivals, f_nivals);

    const char* f_xval;
    meta->getValue("xval", f_xval);

    bool ok = ((!sval || 0==strcmp(sval, f_sval)) &&
               (!ndvals || (ndvals == f_ndvals &&
                            0==memcmp(dvals, f_dvals,
                                      ndvals * sizeof(dvals[0])))) &&
               (!nivals || (nivals == f_nivals &&
                            0==memcmp(ivals, f_ivals,
                                      nivals*sizeof(ivals[0])))) &&
               (!xval || 0==strcmp(xval, f_xval)));
    if (!ok) {
        std::cerr << "Meta data readback failed" << std::endl;
        return 0;
    }
    return 1;
}


int main(int /*argc*/, char** /*argv*/)
{
    static Ptex::Res res[] = { Ptex::Res(8,7),
                               Ptex::Res(0x0201),
                               Ptex::Res(3,1),
                               Ptex::Res(0x0405),
                               Ptex::Res(9,8),
                               Ptex::Res(0x0402),
                               Ptex::Res(6,2),
                               Ptex::Res(0x0407),
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
    Ptex::DataType dt = Ptex::dt_uint16;
    float ptexOne = Ptex::OneValue(dt);
    typedef uint16_t Dtype;
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
        Dtype* fbuff = (Dtype*)buff;
        int ures = res[i].u(), vres = res[i].v();
        for (int v = 0; v < vres; v++) {
            for (int u = 0; u < ures; u++) {
                float c = (u ^ v) & 1;
                fbuff[(v*ures+u)*nchan] = u/float(ures-1) * ptexOne;
                fbuff[(v*ures+u)*nchan+1] = v/float(vres-1) * ptexOne;
                fbuff[(v*ures+u)*nchan+2] = c * ptexOne;
            }
        }

        w->writeFace(i, Ptex::FaceInfo(res[i], adjfaces[i], adjedges[i]), buff);
    }
    free(buff);

    const char* sval = "a str val";
    int ndvals = 3;
    double dvals_buff[3] = { 1.1,2.2,3.3 };
    double* dvals = dvals_buff;
    int nivals = 4;
    int16_t ivals[4] = { 2, 4, 6, 8 };
    const char* xval = 0;

    writeMeta(w, sval, dvals, ndvals, ivals, nivals, xval);
    if (!w->close(error)) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }
    w->release();
    if (!checkMeta("test.ptx", sval, dvals, ndvals, ivals, nivals, xval))
        return 1;

    // add some incremental edits
    w = PtexWriter::edit("test.ptx", true, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    sval = "a string value";
    dvals[2] = 0;
    writeMeta(w, sval, dvals, ndvals, 0, 0, 0);

    if (!w->close(error)) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }
    w->release();
    if (!checkMeta("test.ptx", sval, dvals, ndvals, ivals, nivals, xval))
        return 1;

    // add some non-incremental edits, including some large meta data
    ndvals = 500;
    dvals = (double*)malloc(ndvals * sizeof(dvals[0]));
    for (int i = 0; i < ndvals; i++) dvals[i] = i;

    w = PtexWriter::edit("test.ptx", false, Ptex::mt_quad, dt, nchan, alpha, nfaces, error);
    xval = "another string value";
    writeMeta(w, 0, dvals, ndvals, 0, 0, xval);
    if (!w->close(error)) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }
    w->release();
    if (!checkMeta("test.ptx", sval, dvals, ndvals, ivals, nivals, xval))
        return 1;
    free(dvals);

    return 0;
}
