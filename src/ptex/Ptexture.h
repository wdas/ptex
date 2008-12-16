#ifndef Ptexture_h
#define Ptexture_h
/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

#include <stdint.h>
#include <string>

struct Ptex {
    enum MeshType     { mt_triangle, mt_quad };
    enum DataType     { dt_uint8, dt_uint16, dt_half, dt_float };
    enum EdgeId       { e_bottom, e_right, e_top, e_left };
    enum MetaDataType { mdt_string, mdt_int8, mdt_int16, mdt_int32, mdt_float, mdt_double };
    static const char* MeshTypeName(MeshType mt);
    static const char* DataTypeName(DataType dt);
    static const char* EdgeIdName(EdgeId eid);
    static const char* MetaDataTypeName(MetaDataType mdt);

    static int DataSize(DataType dt) {
	static const int sizes[] = { 1,2,2,4 };
	return sizes[dt]; 
    }
    static double OneValue(DataType dt) {
	static const double one[] = { 255.0, 65535.0, 1.0, 1.0 };
	return one[dt]; 
    }
    static double OneValueInv(DataType dt) {
	static const double one[] = { 1.0/255.0, 1.0/65535.0, 1.0, 1.0 };
	return one[dt]; 
    }
    static void ConvertToFloat(float* dst, const void* src,
			       Ptex::DataType dt, int numChannels);
    static void ConvertFromFloat(void* dst, const float* src,
				 Ptex::DataType dt, int numChannels);

    struct Res {
	// Pixel resolution stored in log form (log2(ures), log2(vres))
	// Note: res values can be negative but faces will
	// always be stored with a res >= 0 (i.e. at least 1 pixel).  The
	// negative res values are only used for accessing
	// face-blended reductions.
	int8_t ulog2, vlog2;
	Res() : ulog2(0), vlog2(0) {}
	Res(int8_t ulog2, int8_t vlog2) : ulog2(ulog2), vlog2(vlog2) {}
	Res(uint16_t value) { val() = value; }
	
	int u() const { return 1<<(unsigned)ulog2; }
	int v() const { return 1<<(unsigned)vlog2; }
	uint16_t& val() { return *(uint16_t*)this; }
	const uint16_t& val() const { return *(uint16_t*)this; }
	int size() const { return u() * v(); }
	bool operator==(const Res& r) const { return val() == r.val(); }
	bool operator!=(const Res& r) const { return val() != r.val(); }
	bool operator>=(const Res& r) const { return ulog2 >= r.ulog2 && vlog2 >= r.vlog2; }
	Res swappeduv() const { return Res(vlog2, ulog2); }
	void swapuv() { *this = swappeduv(); }
	void clamp(const Res& r) { 
	    if (ulog2 > r.ulog2) ulog2 = r.ulog2;
	    if (vlog2 > r.vlog2) vlog2 = r.vlog2;
	}
	int ntilesu(Res tileres) const { return 1<<(ulog2-tileres.ulog2); }
	int ntilesv(Res tileres) const { return 1<<(vlog2-tileres.vlog2); }
	int ntiles(Res tileres) const { return ntilesu(tileres) * ntilesv(tileres); }
    };

    struct FaceInfo {
	Res res;		// resolution of face
	uint8_t adjedges;       // adjacent edges, 2 bits per edge
	uint8_t flags;		// flags (internal use)
	int32_t adjfaces[4];	// adjacent faces (-1 == no adjacent face)

	FaceInfo() : res(), adjedges(0), flags(0) 
	{ 
	    adjfaces[0] = adjfaces[1] = adjfaces[2] = adjfaces[3] = -1; 
	}

	FaceInfo(Res res) : res(res), adjedges(0), flags(0) 
	{ 
	    adjfaces[0] = adjfaces[1] = adjfaces[2] = adjfaces[3] = -1; 
	}

	FaceInfo(Res res, int adjfaces[4], int adjedges[4], bool isSubface=false)
	    : res(res), flags(isSubface ? flag_subface : 0)
	{
	    setadjfaces(adjfaces[0], adjfaces[1], adjfaces[2], adjfaces[3]);
	    setadjedges(adjedges[0], adjedges[1], adjedges[2], adjedges[3]);
	}

	EdgeId adjedge(int eid) const { return EdgeId((adjedges >> (2*eid)) & 3); }
	int adjface(int eid) const { return adjfaces[eid]; }
	bool isConstant() const { return (flags & flag_constant) != 0; }
	bool isNeighborhoodConstant() const { return (flags & flag_nbconstant) != 0; }
	bool hasEdits() const { return (flags & flag_hasedits) != 0; }
	bool isSubface() const { return (flags & flag_subface) != 0; }

	void setadjfaces(int f0, int f1, int f2, int f3)
	{ adjfaces[0] = f0, adjfaces[1] = f1, adjfaces[2] = f2; adjfaces[3] = f3; }
	void setadjedges(int e0, int e1, int e2, int e3)
	{ adjedges = (e0&3) | ((e1&3)<<2) | ((e2&3)<<4) | ((e3&3)<<6); }
	enum { flag_constant = 1, flag_hasedits = 2, flag_nbconstant = 4, flag_subface = 8 };
    };
};


class PtexMetaData {
 public:
    virtual void release() = 0;
    virtual int numKeys() = 0;
    virtual void getKey(int n, const char*& key, Ptex::MetaDataType& type) = 0;
    virtual void getValue(const char* key, const char*& value) = 0;
    virtual void getValue(const char* key, const int8_t*& value, int& count) = 0;
    virtual void getValue(const char* key, const int16_t*& value, int& count) = 0;
    virtual void getValue(const char* key, const int32_t*& value, int& count) = 0;
    virtual void getValue(const char* key, const float*& value, int& count) = 0;
    virtual void getValue(const char* key, const double*& value, int& count) = 0;

 protected:
    virtual ~PtexMetaData() {}
};


class PtexFaceData {
 public:
    virtual void release() = 0;
    virtual bool isConstant() = 0;
    virtual Ptex::Res res() = 0;
    virtual void getPixel(int u, int v, void* result) = 0;
    virtual void* getData() = 0;

    virtual bool isTiled() = 0;
    virtual Ptex::Res tileRes() = 0;
    virtual PtexFaceData* getTile(int tile) = 0;

 protected:
    virtual ~PtexFaceData() {}
};


class PtexTexture {
 public:
    static PtexTexture* open(const char* path, std::string& error, bool premultiply=0);
    virtual void release() = 0;
    virtual const char* path() = 0;
    virtual Ptex::MeshType meshType() = 0;
    virtual Ptex::DataType dataType() = 0;
    virtual int alphaChannel() = 0;
    virtual int numChannels() = 0;
    virtual int numFaces() = 0;
    virtual bool hasEdits() = 0;

    virtual PtexMetaData* getMetaData() = 0;
    virtual const Ptex::FaceInfo& getFaceInfo(int faceid) = 0;
    virtual void getData(int faceid, void* buffer, int stride) = 0;
    virtual void getData(int faceid, void* buffer, int stride, Ptex::Res res) = 0;
    virtual PtexFaceData* getData(int faceid) = 0;
    virtual PtexFaceData* getData(int faceid, Ptex::Res res) = 0;
    virtual void getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels) = 0;
    virtual void getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels,
			  Ptex::Res res) = 0;
    
 protected:
    virtual ~PtexTexture() {}
};


class PtexCache {
 public:
    static PtexCache* create(int maxFiles=0, int maxMem=0, bool premultiply=0);
    virtual void release() = 0;
    virtual void setSearchPath(const char* path) = 0;
    virtual const char* getSearchPath() = 0;
    virtual PtexTexture* get(const char* path, std::string& error) = 0;
    virtual void purge(PtexTexture* texture) = 0;
    virtual void purge(const char* path) = 0;
    virtual void purgeAll() = 0;

 protected:
    virtual ~PtexCache() {}
};


class PtexWriter {
 public:
    static PtexWriter* open(const char* path, 
			    Ptex::MeshType mt, Ptex::DataType dt,
			    int nchannels, int alphachan, int nfaces,
			    std::string& error, bool genmipmaps=true);
    static PtexWriter* edit(const char* path, bool incremental, 
			    Ptex::MeshType mt, Ptex::DataType dt,
			    int nchannels, int alphachan, int nfaces,
			    std::string& error, bool genmipmaps=true);

    virtual void release() = 0;
    
    virtual void writeMeta(const char* key, const char* string) = 0;
    virtual void writeMeta(const char* key, const int8_t* value, int count) = 0;
    virtual void writeMeta(const char* key, const int16_t* value, int count) = 0;
    virtual void writeMeta(const char* key, const int32_t* value, int count) = 0;
    virtual void writeMeta(const char* key, const float* value, int count) = 0;
    virtual void writeMeta(const char* key, const double* value, int count) = 0;
    virtual void writeMeta(PtexMetaData* data) = 0;
    virtual bool writeFace(int faceid, const Ptex::FaceInfo& info, const void* data, int stride=0) = 0;
    virtual bool writeConstantFace(int faceid, const Ptex::FaceInfo& info, const void* data) = 0;
    virtual bool close(std::string& error) = 0;

 protected:
    virtual ~PtexWriter() {}
};


class PtexFilter {
 public:
    static PtexFilter* mitchell(float sharpness);
    static PtexFilter* box();
    static PtexFilter* gaussian();
    static PtexFilter* trilinear();
    static PtexFilter* radialBSpline();

    virtual void release() = 0;
    virtual void eval(float* result, int firstchan, int nchannels,
		      PtexTexture* tx, int faceid,
		      float u, float v, float uw, float vw) = 0;
 protected:
    virtual ~PtexFilter() {};
};


template <class T> class PtexPtr {
    T* _ptr;
 public:
    PtexPtr(T* ptr) : _ptr(ptr) {}
    ~PtexPtr() { if (_ptr) _ptr->release(); }

    operator T* () { return _ptr; }
    T* operator-> () { return _ptr; }
    T* ptr() { return _ptr; }

 private:
    void operator= (PtexPtr& p);
};


#endif
