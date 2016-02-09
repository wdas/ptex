#include <string>
#include <map>
#include <iostream>
#include "Ptexture.h"
#include <cstdlib>
#include <cstdio> // printf()
using namespace Ptex;

void DumpData(Ptex::Res res, Ptex::DataType dt, int nchan, void* data, std::string prefix)
{
    float* pixel = (float*) malloc(sizeof(float)*nchan);
    uint8_t* cpixel = (uint8_t*) malloc(sizeof(uint8_t)*nchan);
    printf("%sdata (%d x %d):\n", prefix.c_str(), res.u(), res.v());
    int ures = res.u(), vres = res.v();
    int pixelSize = Ptex::DataSize(dt) * nchan;
        
    for (int vi = 0; vi < vres; vi++) {
        if (vi == 8 && vres > 16) { vi = vres-8; std::cout << prefix << "  ..." << std::endl; }
        std::cout << prefix << "  ";
        for (int ui = 0; ui < ures; ui++) {
            if (ui == 8 && ures > 16) { ui = ures-8; std::cout << "... "; }
            uint8_t* dpixel = (uint8_t*)data + (vi * ures + ui) * pixelSize;
            Ptex::ConvertToFloat(pixel, dpixel, dt, nchan);
            Ptex::ConvertFromFloat(cpixel, pixel, Ptex::dt_uint8, nchan);
            for (int c=0; c < nchan; c++) {
                printf("%02x", cpixel[c]);
            }
            printf(" ");
        }
        printf("\n");
    }

    free(cpixel);
    free(pixel);
}

template <typename T>
class DumpMetaArrayVal
{
 public:
    void operator()(PtexMetaData* meta, const char* key)
    {
        const T* val=0;
        int count=0;
        meta->getValue(key, val, count);
        for (int i = 0; i < count; i++) {
            if (i%10==0 && (i || count > 10)) std::cout << "\n  ";
            std::cout <<  "  " << val[i];
        }
    }
};


void DumpMetaData(PtexMetaData* meta)
{
    // Sort the meta keys before printing them out because the
    // internal key order may change depending on whether large meta
    // data support was included.  Normally the key order shouldn't
    // matter, but we don't want it to cause the regression test to fail.

    std::map<std::string,int> sortedkeys;
    for (int i = 0; i < meta->numKeys(); i++)
    {
        const char* key;
        Ptex::MetaDataType type;
        meta->getKey(i, key, type);
        sortedkeys[key] = i;
    }

    std::cout << "meta:" << std::endl;
    std::map<std::string,int>::iterator iter;
    for (iter = sortedkeys.begin(); iter != sortedkeys.end(); iter++) {
        const char* key;
        Ptex::MetaDataType type;
        meta->getKey(iter->second, key, type);
        std::cout << "  " << key << " type=" << Ptex::MetaDataTypeName(type);
        switch (type) {
        case Ptex::mdt_string:
            {
                const char* val=0;
                meta->getValue(key, val);
                std::cout <<  "  " << val;
            }
            break;
        case Ptex::mdt_int8:   DumpMetaArrayVal<int8_t>()(meta, key); break;
        case Ptex::mdt_int16:  DumpMetaArrayVal<int16_t>()(meta, key); break;
        case Ptex::mdt_int32:  DumpMetaArrayVal<int32_t>()(meta, key); break;
        case Ptex::mdt_float:  DumpMetaArrayVal<float>()(meta, key); break;
        case Ptex::mdt_double: DumpMetaArrayVal<double>()(meta, key); break;
        }
        std::cout << std::endl;
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    Ptex::String error;
    PtexPtr<PtexCache> c(PtexCache::create(0,0));
    c->setSearchPath("foo/bar:.");
    PtexPtr<PtexTexture> r(c->get("test.ptx", error));

    if (!r) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }
    std::cout << "meshType: " << Ptex::MeshTypeName(r->meshType()) << std::endl;
    std::cout << "dataType: " << Ptex::DataTypeName(r->dataType()) << std::endl;
    std::cout << "numChannels: " << r->numChannels() << std::endl;
    std::cout << "alphaChannel: ";
    if (r->alphaChannel() == -1) std::cout << "(none)" << std::endl;
    else std::cout << r->alphaChannel() << std::endl;
    std::cout << "numFaces: " << r->numFaces() << std::endl;

    PtexMetaData* meta = r->getMetaData();
    std::cout << "numMetaKeys: " << meta->numKeys() << std::endl;
    if (meta->numKeys()) DumpMetaData(meta);
    meta->release();

    int nfaces = r->numFaces();
    for (int i = 0; i < nfaces; i++) {
        const Ptex::FaceInfo& f = r->getFaceInfo(i);
        std::cout << "face " << i << ":\n"
                  << "  res: " << int(f.res.ulog2) << ' ' << int(f.res.vlog2) << "\n"
                  << "  adjface: " 
                  << f.adjfaces[0] << ' '
                  << f.adjfaces[1] << ' '
                  << f.adjfaces[2] << ' '
                  << f.adjfaces[3] << "\n"
                  << "  adjedge: " 
                  << f.adjedge(0) << ' '
                  << f.adjedge(1) << ' '
                  << f.adjedge(2) << ' '
                  << f.adjedge(3) << "\n"
                  << "  flags: " << int(f.flags) << "\n";

        Ptex::Res res = f.res;
        void* data = malloc(Ptex::DataSize(r->dataType()) * r->numChannels() * res.size());
        while (res.ulog2 > 0 || res.vlog2 > 0) {
            r->getData(i, data, 0, res);
            DumpData(res, r->dataType(), r->numChannels(), data, "  ");
            if (res.ulog2) res.ulog2--;
            if (res.vlog2) res.vlog2--;
        }
        r->getData(i, data, 0, Ptex::Res(0,0));
        DumpData(res, r->dataType(), r->numChannels(), data, "  ");
        free(data);

        // Read more channels than are available.
        // This should wipe pixel with zeroes and nothing more.
        float pixel[] = {-1.0f, -1.0f, -1.0f};
        r->getPixel(i, 1, 1, pixel, 3, 3);
        if (pixel[0] != 0.0f || pixel[1] != 0.0f || pixel[2] != 0.0f) {
                std::cerr << "pixel should be zero" << std::endl;
                return 1;
        }
    }

    return 0;
}
