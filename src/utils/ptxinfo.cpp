/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
#include <string>
#include <iostream>
#include "Ptexture.h"
#include "PtexReader.h"

void DumpFaceInfo(const Ptex::FaceInfo& f)
{
    Ptex::Res res = f.res;
    std::cout << "  res: " << int(res.ulog2) << ' ' << int(res.vlog2)
	      << " (" << res.u() << " x " << res.v() << ")"
	      << "  adjface: " 
	      << f.adjfaces[0] << ' '
	      << f.adjfaces[1] << ' '
	      << f.adjfaces[2] << ' '
	      << f.adjfaces[3]
	      << "  adjedge: " 
	      << f.adjedge(0) << ' '
	      << f.adjedge(1) << ' '
	      << f.adjedge(2) << ' '
	      << f.adjedge(3)
	      << "  flags:";
    // output flag names
    if (f.flags == 0) std::cout << " (none)";
    else {
	if (f.isSubface()) std::cout << " subface";
	if (f.isConstant()) std::cout << " constant";
	if (f.isNeighborhoodConstant()) std::cout << " nbconstant";
	if (f.hasEdits()) std::cout << " hasedits";
    }
    std::cout << std::endl;
}


void DumpTiling(PtexFaceData* dh)
{
    std::cout << "  tiling: ";
    if (dh->isTiled()) {
	Ptex::Res res = dh->tileRes();
	std::cout << "ntiles = " << dh->res().ntiles(res)
		  << ", res = "
		  << int(res.ulog2) << ' ' << int(res.vlog2)
		  << " (" << res.u() << " x " << res.v() << ")\n";
    }
    else if (dh->isConstant()) {
	std::cout << "  (constant)" << std::endl;
    }
    else {
	std::cout << "  (untiled)" << std::endl;
    }
}
			

void DumpData(PtexTexture* r, int faceid, bool dumpall)
{
    int levels = 1;
    if (dumpall) {
	PtexReader* R = dynamic_cast<PtexReader*> (r);
	if (R) levels = R->header().nlevels;
    }

    const Ptex::FaceInfo& f = r->getFaceInfo(faceid);
    int nchan = r->numChannels();
    float* pixel = (float*) malloc(sizeof(float)*nchan);
    Ptex::Res res = f.res;
    while (levels && res.ulog2 >= 1 && res.vlog2 >= 1) {
	int ures = res.u(), vres = res.v();
	std::cout << "  data (" << ures << " x " << vres << ")";
	if (f.isConstant()) { ures = vres = 1; }
	bool isconst = (ures == 1 && vres == 1);
	if (isconst) std::cout << ", const: ";
	else std::cout << ":";
	for (int vi = 0; vi < vres; vi++) {
	    for (int ui = 0; ui < ures; ui++) {
		if (!isconst) std::cout << "\n    (" << ui << ", " << vi << "): ";
		r->getPixel(faceid, ui, vi, pixel, 0, nchan, res);
		for (int c=0; c < nchan; c++) {
		    printf(" %.3f", pixel[c]);
		}
	    }
	}
	std::cout << std::endl;
	res.ulog2--;
	res.vlog2--;
	levels--;
    }
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
    std::cout << "meta:" << std::endl;
    for (int i = 0; i < meta->numKeys(); i++) {
	const char* key;
	Ptex::MetaDataType type;
	meta->getKey(i, key, type);
	std::cout << "  " << key << " type=" << Ptex::MetaDataTypeName(type);
	switch (type) {
	case Ptex::mdt_string:
	    {
		const char* val=0;
		meta->getValue(key, val);
		std::cout <<  "  \"" << val << "\"";
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


void DumpInternal(PtexTexture* tx)
{
    PtexReader* r = dynamic_cast<PtexReader*> (tx);
    if (!r) {
	std::cout << "Internal error - PtexReader cast failed\n";
	return;
    }

    const PtexIO::Header& h = r->header();
    const PtexIO::ExtHeader& eh = r->extheader();
    std::cout << "Header:\n"
	      << "  magic: ";

    if (h.magic == PtexIO::Magic)
	std::cout << "'Ptex'" << std::endl;
    else
	std::cout << h.magic << std::endl;

    std::cout << "  version: " << h.version << '.' << h.minorversion << std::endl
	      << "  meshtype: " << h.meshtype << std::endl
	      << "  datatype: " << h.datatype << std::endl
	      << "  alphachan: " << int(h.alphachan) << std::endl
	      << "  nchannels: " << h.nchannels << std::endl
	      << "  nlevels: " << h.nlevels << std::endl
	      << "  nfaces: " << h.nfaces << std::endl
	      << "  extheadersize: " << h.extheadersize << std::endl
	      << "  faceinfosize: " << h.faceinfosize << std::endl
	      << "  constdatasize: " << h.constdatasize << std::endl
	      << "  levelinfosize: " << h.levelinfosize << std::endl
	      << "  leveldatasize: " << h.leveldatasize << std::endl
	      << "  metadatazipsize: " << h.metadatazipsize << std::endl
	      << "  metadatamemsize: " << h.metadatamemsize << std::endl
	      << "  ubordermode: " << eh.ubordermode << std::endl
	      << "  vbordermode: " << eh.vbordermode << std::endl
	      << "  lmdheaderzipsize: " << eh.lmdheaderzipsize << std::endl
	      << "  lmdheadermemsize: " << eh.lmdheadermemsize << std::endl
	      << "  lmddatasize: " << eh.lmddatasize << std::endl
	      << "  editdatasize: " << eh.editdatasize << std::endl
	      << "  editdatapos: " << eh.editdatapos << std::endl;

    std::cout << "Level info:\n";
    for (int i = 0; i < h.nlevels; i++) {
	const PtexIO::LevelInfo& l = r->levelinfo(i);
	std::cout << "  Level " << i << std::endl
		  << "    leveldatasize: " << l.leveldatasize << std::endl
		  << "    levelheadersize: " << l.levelheadersize << std::endl
		  << "    nfaces: " << l.nfaces << std::endl;
    }
}

void usage()
{
    std::cerr << "Usage: ptxinfo [options] file\n"
	      << "  -m Dump meta data\n"
	      << "  -f Dump face info\n"
	      << "  -d Dump data\n"
	      << "  -D Dump data for all mipmap levels\n"
	      << "  -t Dump tiling info\n"
	      << "  -i Dump internal info\n";
    exit(1);
}


int main(int argc, char** argv)
{
    bool dumpmeta = 0;
    bool dumpfaceinfo = 0;
    bool dumpdata = 0;
    bool dumpalldata = 0;
    bool dumpinternal = 0;
    bool dumptiling = 0;
    const char* fname = 0;

    while (--argc) {
	if (**++argv == '-') {
	    char* cp = *argv + 1;
	    if (!*cp) usage(); // handle bare '-'
	    while (*cp) {
		switch (*cp++) {
		case 'm': dumpmeta = 1; break;
		case 'd': dumpdata = 1; break;
		case 'D': dumpdata = 1; dumpalldata = 1; break;
		case 'f': dumpfaceinfo = 1; break;
		case 't': dumptiling = 1; break;
		case 'i': dumpinternal = 1; break;
		default: usage();
		}
	    }
	}
	else if (fname) usage();
	else fname = *argv;
    }
    if (!fname) usage();

    Ptex::String error;
    PtexPtr<PtexTexture> r ( PtexTexture::open(fname, error) );
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
    std::cout << "uBorderMode: " << Ptex::BorderModeName(r->uBorderMode()) << std::endl;
    std::cout << "vBorderMode: " << Ptex::BorderModeName(r->vBorderMode()) << std::endl;
    std::cout << "numFaces: " << r->numFaces() << std::endl;
    std::cout << "hasEdits: " << (r->hasEdits() ? "yes" : "no") << std::endl;
    std::cout << "hasMipMaps: " << (r->hasMipMaps() ? "yes" : "no") << std::endl;

    PtexPtr<PtexMetaData> meta ( r->getMetaData() );
    if (meta) {
	std::cout << "numMetaKeys: " << meta->numKeys() << std::endl;
	if (dumpmeta && meta->numKeys()) DumpMetaData(meta);
    }

    if (dumpfaceinfo || dumpdata || dumptiling) {
	uint64_t texels = 0;
	for (int i = 0; i < r->numFaces(); i++) {
	    std::cout << "face " << i << ":";
	    const Ptex::FaceInfo& f = r->getFaceInfo(i);
	    DumpFaceInfo(f);
	    texels += f.res.size();

	    if (dumptiling) {
		PtexPtr<PtexFaceData> dh ( r->getData(i, f.res) );
		DumpTiling(dh);
	    }
	    if (dumpdata) DumpData(r, i, dumpalldata);
	}
	std::cout << "texels: " << texels << std::endl;
    }

    if (dumpinternal) DumpInternal(r);
    return 0;
}
