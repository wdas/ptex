#ifndef PtexIO_h
#define PtexIO_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include "Ptexture.h"

struct PtexIO : public Ptex {
    struct Header {
	uint32_t magic;
	uint32_t version;
	MeshType meshtype:32;
	DataType datatype:32;
	int32_t  alphachan;
	uint16_t nchannels;
	uint16_t nlevels;
	uint32_t nfaces;
	uint32_t extheadersize;
	uint32_t faceinfosize;
	uint32_t constdatasize;
	uint32_t levelinfosize;
	uint32_t reserved;
	uint64_t leveldatasize;
	uint32_t metadatazipsize;
	uint32_t metadatamemsize;
	int pixelSize() const { return DataSize(datatype) * nchannels; }
	bool hasAlpha() const { return alphachan >= 0 && alphachan < nchannels; }
    };
    struct ExtHeader {
	BorderMode ubordermode:32;
	BorderMode vbordermode:32;
	uint32_t lmdheaderzipsize;
	uint32_t lmdheadermemsize;
	uint64_t largemetadatasize;
    };
    struct LevelInfo {
	uint64_t leveldatasize;
	uint32_t levelheadersize;
	uint32_t nfaces;
	LevelInfo() : leveldatasize(0), levelheadersize(0), nfaces(0) {}
    };
    enum Encoding { enc_constant, enc_zipped, enc_diffzipped, enc_tiled };
    struct FaceDataHeader {
	uint32_t data; // bits 0..29 = blocksize, bits 30..31 = encoding
	uint32_t blocksize() const { return data & 0x3fffffff; }
	Encoding encoding() const { return Encoding((data >> 30) & 0x3); }
	uint32_t& val() { return *(uint32_t*) this; }
	const uint32_t& val() const { return *(uint32_t*) this; }
	void set(uint32_t blocksize, Encoding encoding)
	{ data = (blocksize & 0x3fffffff) | ((encoding & 0x3) << 30); }
	FaceDataHeader() : data(0) {}
    };
    enum EditType { et_editfacedata, et_editmetadata };
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
    static const int ExtHeaderSize = sizeof(ExtHeader);
    static const int LevelInfoSize = sizeof(LevelInfo);
    static const int FaceDataHeaderSize = sizeof(FaceDataHeader);
    static const int EditFaceDataHeaderSize = sizeof(EditFaceDataHeader);
    static const int EditMetaDataHeaderSize = sizeof(EditMetaDataHeader);
    static const int BlockSize = 16384; // target block size for file I/O
    static const int TileSize  = 65536; // target tile size (uncompressed)
    static const int AllocaMax = 16384;	// max size for using alloca
    static bool LittleEndian() {
	short word = 0x0201;
	return *(char*)&word == 1; 
    }
};

#endif
