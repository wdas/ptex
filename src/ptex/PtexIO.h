#ifndef PtexIO_h
#define PtexIO_h

#include "Ptexture.h"

struct PtexIO : public Ptex {
    struct Header {
	uint32_t magic;
	uint32_t version;
	MeshType meshtype:32;
	DataType datatype:32;
	uint32_t alphachan;
	uint16_t nchannels;
	uint16_t nlevels;
	uint32_t nfaces;
	uint32_t extheadersize;
	uint32_t faceinfosize;
	uint32_t constdatasize;
	uint32_t levelinfosize;
	uint64_t leveldatasize;
	uint32_t metadatazipsize;
	uint32_t metadatamemsize;
	int pixelSize() { return DataSize(datatype) * nchannels; }
    };
    struct LevelInfo {
	uint64_t leveldatasize;
	uint32_t levelheadersize;
	uint32_t nfaces;
	LevelInfo() : leveldatasize(0), levelheadersize(0), nfaces(0) {}
    };
    enum Encoding { enc_constant, enc_zipped, enc_diffzipped, enc_tiled };
    struct FaceDataHeader {
	uint32_t blocksize:30;
	Encoding encoding:2;
	uint32_t& val() { return *(uint32_t*) this; }
	const uint32_t& val() const { return *(uint32_t*) this; }
	FaceDataHeader() : blocksize(0), encoding(Encoding(0)) {}
    };
    enum EditType { et_editfacedata, et_editmetadata };
    struct EditDataHeader {
	EditType edittype;
	uint32_t editsize;
    };
    struct EditFaceDataHeader {
	uint32_t faceid;
	FaceInfo faceinfo;
	FaceDataHeader fdh;
    };
    struct EditMetaDataHeader {
	uint32_t metadatazipsize;
	uint32_t metadatamemsize;
    };

    static const uint32_t Magic = 'P' | ('t'<<8) | ('e'<<16) | ('x'<<24);
    static const int HeaderSize = sizeof(Header);
    static const int LevelInfoSize = sizeof(LevelInfo);
    static const int FaceDataHeaderSize = sizeof(FaceDataHeader);
    static const int EditDataHeaderSize = sizeof(EditDataHeader);
    static const int EditFaceDataHeaderSize = sizeof(EditFaceDataHeader);
    static const int EditMetaDataHeaderSize = sizeof(EditMetaDataHeader);
    static const int BlockSize = 16384; // target block size for file I/O
    static const int TileSize  = 65536; // target tile size (uncompressed)
    static const int AllocaMax = 16384;	// max size for using alloca
    static bool LittleEndian() { return static_cast<const char&>(1) == 1; }
};

#endif
