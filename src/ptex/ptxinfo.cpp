#include <string>
#include <alloca.h>
#include <iostream>
#include "Ptexture.h"

void DumpFaceInfo(const Ptex::FaceInfo& f)
{
    Ptex::Res res = f.res;
    std::cout << "  res: " << int(res.ulog2) << ' ' << int(res.vlog2)
	      << " (" << res.u() << " x " << res.v() << ")\n"
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
}


void DumpData(Ptex::DataType dt, int nchan, PtexFaceData* dh)
{
    float* pixel = (float*) alloca(sizeof(float)*nchan);
    Ptex::Res res = dh->res();
    int ures = res.u(), vres = res.v();
    std::cout << "  data (" << ures << " x " << vres << ")";
    if (dh->isConstant()) { ures = vres = 1; }
    bool isconst = (ures == 1 && vres == 1);
    if (isconst) std::cout << ", const: ";
    else std::cout << ":";
    for (int vi = 0; vi < vres; vi++) {
	for (int ui = 0; ui < ures; ui++) {
	    if (!isconst) std::cout << "\n    (" << ui << ", " << vi << "): ";
	    void* src = dh->getPixel(ui, vi);
	    Ptex::ConvertToFloat(pixel, src, dt, nchan);
	    for (int c=0; c < nchan; c++) {
		printf(" %.3f", pixel[c]);
	    }
	}
    }
    std::cout << std::endl;
}

void DumpMetaData(PtexMetaData* meta)
{
    std::cout << "meta:" << std::endl;
    for (int i = 0; i < meta->numKeys(); i++) {
	const char* key;
	Ptex::MetaDataType type;
	meta->getKey(i, key, type);
	std::cout << "  " << key << " type=" << Ptex::MetaDataTypeName(type);
	int count;
	switch (type) {
	case Ptex::mdt_string:
	    {
		const char* val=0;
		meta->getValue(key, val);
		std::cout <<  "  " << val;
	    }
	    break;
	case Ptex::mdt_int8:
	    {
		const int8_t* val=0;
		meta->getValue(key, val, count);
		for (int j = 0; j < count; j++)
		    std::cout <<  "  " << val[j];
	    }
	    break;
	case Ptex::mdt_int16:
	    {
		const int16_t* val=0;
		meta->getValue(key, val, count);
		for (int j = 0; j < count; j++)
		    std::cout <<  "  " << val[j];
	    }
	    break;
	case Ptex::mdt_int32:
	    {
		const int32_t* val=0;
		meta->getValue(key, val, count);
		for (int j = 0; j < count; j++)
		    std::cout <<  "  " << val[j];
	    } 
	    break;
	case Ptex::mdt_float:
	    {
		const float* val=0;
		meta->getValue(key, val, count);
		for (int j = 0; j < count; j++)
		    std::cout <<  "  " << val[j];
	    }
	    break;
	case Ptex::mdt_double:
	    {
		const double* val=0;
		meta->getValue(key, val, count);
		for (int j = 0; j < count; j++)
		    std::cout <<  "  " << val[j];
	    }
	    break;
	}
	std::cout << std::endl;
    }
}

void usage()
{
    std::cerr << "Usage: ptxinfo [options] file\n"
	      << "  -m Dump meta data\n"
	      << "  -f Dump face info\n"
	      << "  -d Dump data\n";
    exit(1);
}


int main(int argc, char** argv)
{
    bool dumpmeta = 0;
    bool dumpfaceinfo = 0;
    bool dumpdata = 0;
    const char* fname = 0;

    while (--argc) {
	if (**++argv == '-') {
	    for (char* cp = *argv + 1; *cp; cp++) {
		switch (*cp) {
		case 'm': dumpmeta = 1; break;
		case 'd': dumpdata = 1; break;
		case 'f': dumpfaceinfo = 1; break;
		default: usage();
		}
	    }
	}
	else if (fname) usage();
	else fname = *argv;
    }
    if (!fname) usage();

    std::string error;
    PtexTexture* r = PtexTexture::open(fname, error);
    if (!r) {
	std::cerr << error << std::endl;
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
    if (dumpmeta && meta->numKeys()) DumpMetaData(meta);
    meta->release();

    if (dumpfaceinfo || dumpdata) {
	for (int i = 0; i < r->numFaces(); i++) {
	    std::cout << "face " << i << ":\n";
	    const Ptex::FaceInfo& f = r->getFaceInfo(i);
	    if (dumpfaceinfo) DumpFaceInfo(f);

	    if (dumpdata) {
		PtexFaceData* dh = r->getData(i, f.res);
		if (dh) DumpData(r->dataType(), r->numChannels(), dh);
		dh->release();
	    }
	}
    }
    return 0;
}
