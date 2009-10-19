#ifndef PtexWriter_h
#define PtexWriter_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include "PtexPlatform.h"
#include <zlib.h>
#include <map>
#include <vector>
#include <stdio.h>
#include "Ptexture.h"
#include "PtexIO.h"
#include "PtexReader.h"


class PtexWriterBase : public PtexWriter, public PtexIO {
public:
    virtual void setBorderModes(Ptex::BorderMode uBorderMode, Ptex::BorderMode vBorderMode)
    {
	_extheader.ubordermode = uBorderMode;
	_extheader.vbordermode = vBorderMode;
    }
    virtual void writeMeta(const char* key, const char* value);
    virtual void writeMeta(const char* key, const int8_t* value, int count);
    virtual void writeMeta(const char* key, const int16_t* value, int count);
    virtual void writeMeta(const char* key, const int32_t* value, int count);
    virtual void writeMeta(const char* key, const float* value, int count);
    virtual void writeMeta(const char* key, const double* value, int count);
    virtual void writeMeta(PtexMetaData* data);
    virtual bool close(Ptex::String& error);
    virtual void release();

    bool ok(Ptex::String& error) {
	if (!_ok) getError(error);
	return _ok;
    }
    void getError(Ptex::String& error) {
	error = (_error + "\nPtex file: " + _path).c_str(); 
    }

protected:
    virtual void finish() = 0;
    PtexWriterBase(const char* path,
		   Ptex::MeshType mt, Ptex::DataType dt,
		   int nchannels, int alphachan, int nfaces,
		   bool compress);
    virtual ~PtexWriterBase();

    int writeBlank(FILE* fp, int size);
    int writeBlock(FILE* fp, const void* data, int size);
    int writeZipBlock(FILE* fp, const void* data, int size, bool finish=true);
    int readBlock(FILE* fp, void* data, int size);
    int copyBlock(FILE* dst, FILE* src, FilePos pos, int size);
    Res calcTileRes(Res faceres);
    virtual void addMetaData(const char* key, MetaDataType t, const void* value, int size);
    void writeConstFaceBlock(FILE* fp, const void* data, FaceDataHeader& fdh);
    void writeFaceBlock(FILE* fp, const void* data, int stride, Res res,
		       FaceDataHeader& fdh);
    void writeFaceData(FILE* fp, const void* data, int stride, Res res,
		       FaceDataHeader& fdh);
    void writeReduction(FILE* fp, const void* data, int stride, Res res);
    void writeMetaData(FILE* fp, uint32_t& memsize, uint32_t& zipsize);
    void setError(const std::string& error) { _error = error; _ok = false; }
    bool storeFaceInfo(int faceid, FaceInfo& dest, const FaceInfo& src, int flags=0);

    bool _ok;			// true if no error has occurred
    std::string _error;		// the error text (if any)
    std::string _path;		// file path
    std::string _tilepath;	// temp tile file path ("<path>.tiles.tmp")
    FILE* _tilefp;		// temp tile file handle
    Header _header;		// the file header
    ExtHeader _extheader;	// extended header
    int _pixelSize;		// size of a pixel in bytes

    struct MetaEntry {
	MetaDataType datatype;
	std::vector<uint8_t> data;
	MetaEntry() : datatype(MetaDataType(0)), data() {}
    };
    typedef std::map<std::string, MetaEntry> MetaData;
    MetaData _metadata;
    z_stream_s _zstream;	// libzip compression stream

    PtexUtils::ReduceFn* _reduceFn;
};


class PtexMainWriter : public PtexWriterBase {
public:
    PtexMainWriter(const char* path, bool newfile,
		   Ptex::MeshType mt, Ptex::DataType dt,
		   int nchannels, int alphachan, int nfaces, bool genmipmaps);

    virtual bool close(Ptex::String& error);
    virtual bool writeFace(int faceid, const FaceInfo& f, const void* data, int stride);
    virtual bool writeConstantFace(int faceid, const FaceInfo& f, const void* data);

protected:
    virtual ~PtexMainWriter();
    virtual void addMetaData(const char* key, MetaDataType t, const void* value, int size)
    {
	PtexWriterBase::addMetaData(key, t, value, size);
	_hasNewData = true;
    }

private:
    virtual void finish();
    void generateReductions();
    void flagConstantNeighorhoods();
    void storeConstValue(int faceid, const void* data, int stride, Res res);

    std::string _newpath;		  // path to ".new" file
    std::string _tmppath;		  // temp file path ("<path>.tmp")
    FILE* _tmpfp;			  // temp file handle
    bool _hasNewData;			  // true if data has been written
    bool _genmipmaps;			  // true if mipmaps should be generated
    std::vector<FaceInfo> _faceinfo;	  // info about each face
    std::vector<uint8_t> _constdata;	  // constant data for each face
    std::vector<uint32_t> _rfaceids;	  // faceid reordering for reduction levels
    std::vector<uint32_t> _faceids_r;     // faceid indexed by rfaceid

    static const int MinReductionLog2 =2; // log2(minimum reduction size) - can tune
    struct LevelRec {
	// note: level 0 is ordered by faceid
	//       levels 1+ are reduction levels (half res in both u and v) and
	//       are ordered by rfaceid[faceid].   Also, faces with a minimum
	//       dimension (the smaller of u or v) smaller than MinReductionLog2
	//       are omitted from subsequent levels.
	std::vector<FilePos> pos;	  // position of data blocks within _tmp file
	std::vector<FaceDataHeader> fdh;  // face data headers
    };
    std::vector<LevelRec> _levels;	  // info about each level
    std::vector<FilePos> _rpos;		  // reduction file positions

    PtexReader* _reader;	          // reader for accessing existing data in file
};


class PtexIncrWriter : public PtexWriterBase {
 public:
    PtexIncrWriter(const char* path, FILE* fp,
		   Ptex::MeshType mt, Ptex::DataType dt,
		   int nchannels, int alphachan, int nfaces);

    virtual bool close(Ptex::String& error);
    virtual bool writeFace(int faceid, const FaceInfo& f, const void* data, int stride);
    virtual bool writeConstantFace(int faceid, const FaceInfo& f, const void* data);

 protected:
    virtual void finish();
    virtual ~PtexIncrWriter();

 private:
    FILE* _fp;		// the file being edited
};

#endif
