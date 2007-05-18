#ifndef PtexWriter_h
#define PtexWriter_h

#include <zlib.h>
#include <map>
#include <vector>
#include <stdio.h>
#include "Ptexture.h"
#include "PtexIO.h"


class PtexWriterBase : public PtexWriter, public PtexIO {
public:
    virtual void writeMeta(const char* key, const char* value);
    virtual void writeMeta(const char* key, const int8_t* value, int count);
    virtual void writeMeta(const char* key, const int16_t* value, int count);
    virtual void writeMeta(const char* key, const int32_t* value, int count);
    virtual void writeMeta(const char* key, const float* value, int count);
    virtual void writeMeta(const char* key, const double* value, int count);
    virtual void writeMeta(PtexMetaData* data);
    virtual bool close(std::string& error);
    virtual void release();

protected:
    virtual void finish() = 0;
    PtexWriterBase();
    virtual ~PtexWriterBase();

    bool create(const char* path, MeshType mt, DataType dt, int nchannels, int alphachan,
		int nfaces, std::string& error, bool genmipmaps);
    bool append(const char* path, MeshType mt, DataType dt, int nchannels, int alphachan,
		int nfaces, std::string& error);
    int writeBlank(FILE* fp, int size);
    int writeBlock(FILE* fp, const void* data, int size);
    int writeZipBlock(FILE* fp, const void* data, int size, bool finish=true);
    int readBlock(FILE* fp, void* data, int size);
    int copyBlock(FILE* dst, FILE* src, off_t pos, int size);
    Res calcTileRes(Res faceres);
    void addMetaData(const char* key, MetaDataType t, const void* value, int size);
    void writeConstFaceBlock(FILE* fp, const void* data, FaceDataHeader& fdh);
    void writeFaceBlock(FILE* fp, const void* data, int stride, Res res,
		       FaceDataHeader& fdh);
    void writeFaceData(FILE* fp, const void* data, int stride, Res res,
		       FaceDataHeader& fdh);
    void writeReduction(FILE* fp, const void* data, int stride, Res res);
    void writeMetaData(FILE* fp, uint32_t& memsize, uint32_t& zipsize);
    void setError(const std::string& error) { _error = error; _ok = false; }

    bool _ok;			// true if no error has occurred
    std::string _error;		// the error text (if any)
    std::string _path;		// file path
    FILE* _fp;			// main file pointer
    Header _header;		// the file header
    int _pixelSize;		// size of a pixel in bytes
    bool _genmipmaps;		// true if mipmaps should be generated

    struct MetaEntry {
	MetaDataType datatype;
	std::vector<uint8_t> data;
	MetaEntry() : datatype(MetaDataType(0)), data() {}
    };
    typedef std::map<std::string, MetaEntry> MetaData;
    MetaData _metadata;
    z_stream_s _zstream;	// libzip compression stream
};


class PtexMainWriter : public PtexWriterBase {
public:
    PtexMainWriter();
    bool open(const char* path, Ptex::MeshType mt, Ptex::DataType dt,
	      int nchannels, int alphachan, int nfaces, std::string& error, 
	      bool genmipmaps);

    virtual bool close(std::string& error);
    virtual bool writeFace(int faceid, const FaceInfo& f, const void* data, int stride);
    virtual bool writeConstantFace(int faceid, const FaceInfo& f, const void* data);

protected:
    virtual ~PtexMainWriter();

private:
    virtual void finish();
    void generateReductions();
    void storeConstValue(int faceid, const void* data, int stride, Res res);

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
	std::vector<off_t> pos;		  // position of data blocks within _tmp file
	std::vector<FaceDataHeader> fdh;  // face data headers
    };
    std::vector<LevelRec> _levels;	  // info about each level
    std::vector<off_t> _rpos;		  // reduction file positions

    std::string _finalpath;	          // final path (base _path has ".tmp" appended)
    FILE* _tmpfp;			  // temp file pointer (on /tmp, not _tmppath!)
};


class PtexIncrWriter : public PtexWriterBase {
 public:
    PtexIncrWriter();
    bool open(const char* path, Ptex::MeshType mt, Ptex::DataType dt,
	      int nchannels, int alphachan, int nfaces, std::string& error);

    virtual bool writeFace(int faceid, const FaceInfo& f, const void* data, int stride);
    virtual bool writeConstantFace(int faceid, const FaceInfo& f, const void* data);

 protected:
    bool writeFace(int faceid, const FaceInfo &f, const void* data, int stride, bool isConst);
    virtual void finish();
    virtual ~PtexIncrWriter();
};

#endif
