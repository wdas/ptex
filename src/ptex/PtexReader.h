#ifndef PtexReader_h
#define PtexReader_h

/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
#include <stdio.h>
#include <zlib.h>
#include <vector>
#include <string>
#include <map>
#include "Ptexture.h"
#include "PtexIO.h"
#include "PtexCache.h"
#include "PtexUtils.h"

#include "PtexHashMap.h"
using namespace PtexInternal;

#ifndef NDEBUG
#include <assert.h>
template<typename T> class safevector : public std::vector<T>
{
public:
    safevector() : std::vector<T>() {}
    safevector(size_t n, const T& val = T()) : std::vector<T>(n, val) {}
    const T& operator[] (size_t n) const {
	assert(n < std::vector<T>::size());
	return std::vector<T>::operator[](n); 
    }
    T& operator[] (size_t n) {
	assert(n < std::vector<T>::size());
	return std::vector<T>::operator[](n); 
    }
};
#else
#define safevector std::vector
#endif

class PtexReader : public PtexCachedFile, public PtexTexture, public PtexIO {
public:
    PtexReader(void** parent, PtexCacheImpl* cache, bool premultiply);
    bool open(const char* path, Ptex::String& error);

    void setOwnsCache() { _ownsCache = true; }
    virtual void release();
    virtual const char* path() { return _path.c_str(); }
    virtual Ptex::MeshType meshType() { return MeshType(_header.meshtype); }
    virtual Ptex::DataType dataType() { return DataType(_header.datatype); }
    virtual int alphaChannel() { return _header.alphachan; }
    virtual int numChannels() { return _header.nchannels; }
    virtual int numFaces() { return _header.nfaces; }
    virtual bool hasEdits() { return _hasEdits; }

    virtual PtexMetaData* getMetaData();
    virtual const Ptex::FaceInfo& getFaceInfo(int faceid);
    virtual void getData(int faceid, void* buffer, int stride);
    virtual void getData(int faceid, void* buffer, int stride, Res res);
    virtual PtexFaceData* getData(int faceid);
    virtual PtexFaceData* getData(int faceid, Res res);
    virtual void getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels);
    virtual void getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels, 
			  Ptex::Res res);

    DataType datatype() const { return _header.datatype; }
    int nchannels() const { return _header.nchannels; }
    int pixelsize() const { return _pixelsize; }
    const Header& header() const { return _header; }
    const LevelInfo& levelinfo(int level) const { return _levelinfo[level]; }

    class MetaData : public PtexCachedData, public PtexMetaData {
    public:
	MetaData(MetaData** parent, PtexCacheImpl* cache, int size)
	    : PtexCachedData((void**)parent, cache, sizeof(*this) + size) {}
	virtual void release() { AutoLockCache lock(_cache->cachelock); unref(); }

	virtual int numKeys() { return int(_entries.size()); }
	virtual void getKey(int n, const char*& key, MetaDataType& type)
	{
	    Entry* e = getEntry(n);
	    key = e->key;
	    type = e->type;
	}

	virtual void getValue(const char* key, const char*& value)
	{
	    Entry* e = getEntry(key);
	    if (e) value = (const char*) &e->value[0];
	    else value = 0;
	}

	virtual void getValue(const char* key, const int8_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) { value = (const int8_t*) &e->value[0]; count = int(e->value.size()); }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const int16_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const int16_t*) &e->value[0]; 
		count = int(e->value.size()/sizeof(int16_t));
	    }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const int32_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const int32_t*) &e->value[0]; 
		count = int(e->value.size()/sizeof(int32_t));
	    }
	    else { value = 0; count = 0; }
	}
	
	virtual void getValue(const char* key, const float*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const float*) &e->value[0]; 
		count = int(e->value.size()/sizeof(float));
	    }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const double*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const double*) &e->value[0]; 
		count = int(e->value.size()/sizeof(double));
	    }
	    else { value = 0; count = 0; }
	}

	void addEntry(uint8_t keysize, const char* key, uint8_t datatype,
		      uint32_t datasize, void* data)
	{
	    std::pair<MetaMap::iterator,bool> result = 
		_map.insert(std::make_pair(std::string(key, keysize), Entry()));
	    Entry* e = &result.first->second;
	    e->key = result.first->first.c_str();
	    e->type = MetaDataType(datatype);
	    e->value.resize(datasize);
	    memcpy(&e->value[0], data, datasize);
	    if (result.second) {
		// inserted new entry
		_entries.push_back(e);
	    }
	}

    protected:
	struct Entry {
	    const char* key; // ptr to map key string
	    MetaDataType type;
	    safevector<uint8_t> value;
	    Entry() : key(0), type(MetaDataType(0)), value() {}
	};
	Entry* getEntry(int n) { return _entries[n]; }
	Entry* getEntry(const char* key)
	{
	    MetaMap::iterator iter = _map.find(key);
	    if (iter != _map.end()) return &iter->second;
	    return 0;
	}

	typedef std::map<std::string, Entry> MetaMap;
	MetaMap _map;
	safevector<Entry*> _entries;
    };


    class ConstDataPtr : public PtexFaceData {
    public:
	ConstDataPtr(void* data, int pixelsize) 
	    : _data(data), _pixelsize(pixelsize) {}
	virtual void release() { delete this; }
	virtual Ptex::Res res() { return 0; }
	virtual bool isConstant() { return true; }
	virtual void getPixel(int, int, void* result)
	{ memcpy(result, _data, _pixelsize); }
	virtual void* getData() { return _data; }
	virtual bool isTiled() { return false; }
	virtual Ptex::Res tileRes() { return 0; }
	virtual PtexFaceData* getTile(int) { return 0; }

    protected:
	void* _data;
	int _pixelsize;
    };


    class FaceData : public PtexCachedData, public PtexFaceData {
    public:
	FaceData(void** parent, PtexCacheImpl* cache, Res res, int size)
	    : PtexCachedData(parent, cache, size), _res(res) {}
	virtual void release() { AutoLockCache lock(_cache->cachelock); unref(); }
	virtual Ptex::Res res() { return _res; }
	virtual void reduce(FaceData*&, PtexReader*,
			    Res newres, PtexUtils::ReduceFn) = 0;
    protected:
	Res _res;
    };

    class PackedFace : public FaceData {
    public:
	PackedFace(void** parent, PtexCacheImpl* cache, Res res, int pixelsize, int size)
	    : FaceData(parent, cache, res, sizeof(*this)+size),
	      _pixelsize(pixelsize), _data(malloc(size)) {}
	void* data() { return _data; }
	virtual bool isConstant() { return false; }
	virtual void getPixel(int u, int v, void* result)
	{
	    memcpy(result, (char*)_data + (v*_res.u() + u) * _pixelsize, _pixelsize);
	}
	virtual void* getData() { return _data; };
	virtual bool isTiled() { return false; }
	virtual Ptex::Res tileRes() { return _res; };
	virtual PtexFaceData* getTile(int) { return 0; };
	virtual void reduce(FaceData*&, PtexReader*,
			    Res newres, PtexUtils::ReduceFn);

    protected:
	virtual ~PackedFace() { free(_data); }

	int _pixelsize;
	void* _data;
    };

    class ConstantFace : public PackedFace {
    public:
	ConstantFace(void** parent, PtexCacheImpl* cache, int pixelsize)
	    : PackedFace(parent, cache, 0, pixelsize, pixelsize) {}
	virtual bool isConstant() { return true; }
	virtual void getPixel(int, int, void* result) { memcpy(result, _data, _pixelsize); }
	virtual void reduce(FaceData*&, PtexReader*,
			    Res newres, PtexUtils::ReduceFn);
    };


    class TiledFaceBase : public FaceData {
    public:
	TiledFaceBase(void** parent, PtexCacheImpl* cache, Res res,
		      Res tileres, DataType dt, int nchan)
	    : FaceData(parent, cache, res, sizeof(*this)),
	      _tileres(tileres),
	      _dt(dt),
	      _nchan(nchan),
	      _pixelsize(DataSize(dt)*nchan)
	{
	    _ntilesu = _res.ntilesu(tileres);
	    _ntilesv = _res.ntilesv(tileres);
	    _ntiles = _ntilesu*_ntilesv;
	    _tiles.resize(_ntiles);
	    incSize(sizeof(FaceData*)*_ntiles);
	}

	virtual void release() {
	    // Tiled faces ref the reader (directly or indirectly) and
	    // thus may trigger cache deletion on release.  Call cache
	    // to check for pending delete.
	    // Note: release() may delete "this", so save _cache in
	    // local var.
	    PtexCacheImpl* cache = _cache;
	    FaceData::release();
	    cache->handlePendingDelete();
	}
	virtual bool isConstant() { return false; }
	virtual void getPixel(int u, int v, void* result);
	virtual void* getData() { return 0; }
	virtual bool isTiled() { return true; }
	virtual Ptex::Res tileRes() { return _tileres; };
	virtual void reduce(FaceData*&, PtexReader*,
			    Res newres, PtexUtils::ReduceFn);
	Res tileres() const { return _tileres; }
	int ntilesu() const { return _ntilesu; }
	int ntilesv() const { return _ntilesv; }
	int ntiles() const { return _ntiles; }

    protected:
	virtual ~TiledFaceBase() { orphanList(_tiles); }

	Res _tileres;
	DataType _dt;
	int _nchan;
	int _ntilesu;
	int _ntilesv;
	int _ntiles;
	int _pixelsize;
	safevector<FaceData*> _tiles;
    };


    class TiledFace : public TiledFaceBase {
    public:
	TiledFace(void** parent, PtexCacheImpl* cache, Res res, Res tileres,
		  int levelid, PtexReader* reader)
	    : TiledFaceBase(parent, cache, res, tileres, 
			    reader->datatype(), reader->nchannels()),
	      _reader(reader),
	      _levelid(levelid)
	{
	    _fdh.resize(_ntiles),
	    _offsets.resize(_ntiles);
	    incSize((sizeof(FaceDataHeader)+sizeof(FilePos))*_ntiles);
	}
	virtual PtexFaceData* getTile(int tile)
	{
	    AutoLockCache locker(_cache->cachelock);
	    FaceData*& f = _tiles[tile];
	    if (!f) readTile(tile, f);
	    else f->ref();
	    return f;
	}
	void readTile(int tile, FaceData*& data);

    protected:
	friend class PtexReader;
	PtexReader* _reader;
	int _levelid;
	safevector<FaceDataHeader> _fdh;
	safevector<FilePos> _offsets;
    };	    


    class TiledReducedFace : public TiledFaceBase {
    public:
	TiledReducedFace(void** parent, PtexCacheImpl* cache, Res res, 
			 Res tileres, DataType dt, int nchan,
			 TiledFaceBase* parentface, PtexUtils::ReduceFn reducefn)
	    : TiledFaceBase(parent, cache, res, tileres, dt, nchan),
	      _parentface(parentface),
	      _reducefn(reducefn)
	{
	    AutoLockCache locker(_cache->cachelock);
	    _parentface->ref(); 
	}
	~TiledReducedFace()
	{
	    _parentface->unref();
	}
	virtual PtexFaceData* getTile(int tile);

    protected:
	TiledFaceBase* _parentface;
	PtexUtils::ReduceFn* _reducefn;
    };


    class Level : public PtexCachedData {
    public:
	safevector<FaceDataHeader> fdh;
	safevector<FilePos> offsets;
	safevector<FaceData*> faces;
	
	Level(void** parent, PtexCacheImpl* cache, int nfaces) 
	    : PtexCachedData(parent, cache, 
			     sizeof(*this) + nfaces * (sizeof(FaceDataHeader) +
						       sizeof(FilePos) + 
						       sizeof(FaceData*))),
	      fdh(nfaces),
	      offsets(nfaces),
	      faces(nfaces) {}
    protected:
	virtual ~Level() { orphanList(faces); }
    };

    Mutex readlock;
    Mutex reducelock;

protected:
    virtual ~PtexReader();
    void setError(const char* error)
    {
	_error = error; _error += " PtexFile: "; _error += _path;
	printf("%s\n", _error.c_str());
	_ok = 0;
    }

    FilePos tell() { return _pos; }
    void seek(FilePos pos) 
    {
	if (pos != _pos) {
	    fseeko(_fp, pos, SEEK_SET); 
	    _pos = pos;
#ifdef GATHER_STATS
	    stats.nseeks++;
#endif
	}
    }

    bool readBlock(void* data, int size, bool reportError=true);
    bool readZipBlock(void* data, int zipsize, int unzipsize);
    Level* getLevel(int levelid)
    {
	Level*& level = _levels[levelid];
	if (!level) readLevel(levelid, level);
	else level->ref();
	return level;
    }

    uint8_t* getConstData() { if (!_constdata) readConstData(); return _constdata; }
    FaceData* getFace(int levelid, Level* level, int faceid)
    {
	FaceData*& face = level->faces[faceid];
	if (!face) readFace(levelid, level, faceid);
	else face->ref();
	return face;
    }

    Res getRes(int levelid, int faceid)
    {
	if (levelid == 0) return _faceinfo[faceid].res;
	else {
	    // for reduction level, look up res via rfaceid
	    Res res = _res_r[faceid];
	    // and adjust for number of reductions
	    return Res(res.ulog2 - levelid, res.vlog2 - levelid);
	}
    }

    int unpackedSize(FaceDataHeader fdh, int levelid, int faceid)
    {
	if (fdh.encoding() == enc_constant) 
	    // level 0 constant faces are not stored
	    return levelid == 0 ? 0 : _pixelsize;
	else 
	    return getRes(levelid, faceid).size() * _pixelsize;
    }

    void readFaceInfo();
    void readLevelInfo();
    void readConstData();
    void readLevel(int levelid, Level*& level);
    void readFace(int levelid, Level* level, int faceid);
    void readFaceData(FilePos pos, FaceDataHeader fdh, Res res, int levelid, FaceData*& face);
    void readMetaData();
    void readMetaDataBlock(MetaData* metadata, FilePos pos, int zipsize, int memsize);
    void readEditData();
    void readEditFaceData();
    void readEditMetaData();

    void computeOffsets(FilePos pos, int noffsets, const FaceDataHeader* fdh, FilePos* offsets)
    {
	FilePos* end = offsets + noffsets;
	while (offsets != end) { *offsets++ = pos; pos += fdh->blocksize(); fdh++; }
    }
    void blendFaces(FaceData*& face, int faceid, Res res, bool blendu);

    bool _premultiply;		      // true if reader should premultiply the alpha chan
    bool _ownsCache;		      // true if reader owns the cache
    bool _ok;			      // flag set if read error occurred)
    std::string _error;		      // error string (if !_ok)
    FILE* _fp;			      // file pointer
    FilePos _pos;		      // current seek position
    std::string _path;		      // current file path
    Header _header;		      // the header
    FilePos _extheaderpos;	      // file positions of data sections
    FilePos _faceinfopos;	      // ...
    FilePos _constdatapos;
    FilePos _levelinfopos;
    FilePos _leveldatapos;
    FilePos _metadatapos;
    FilePos _editdatapos;
    int _pixelsize;		      // size of a pixel in bytes
    uint8_t* _constdata;	      // constant pixel value per face
    MetaData* _metadata;	      // meta data (read on demand)
    bool _hasEdits;		      // has edit blocks

    safevector<FaceInfo> _faceinfo;   // per-face header info
    safevector<uint32_t> _rfaceids;   // faceids sorted in reduction order
    safevector<Res> _res_r;	      // face res indexed by rfaceid
    safevector<LevelInfo> _levelinfo; // per-level header info
    safevector<FilePos> _levelpos;    // file position of each level's data
    safevector<Level*> _levels;	      // level data (read on demand)

    struct MetaEdit
    {
	FilePos pos;
	int zipsize;
	int memsize;
    };
    safevector<MetaEdit> _metaedits;

    struct FaceEdit
    {
	FilePos pos;
	int faceid;
	FaceDataHeader fdh;
    };
    safevector<FaceEdit> _faceedits;

    struct ReductionKey {
	int faceid;
	Res res;
	ReductionKey() : faceid(0), res(0,0) {}
	ReductionKey(uint32_t faceid, Res res) : faceid(faceid), res(res) {}
	bool operator==(const ReductionKey& k) const 
	{ return k.faceid == faceid && k.res == res; }
	struct Hasher {
	    uint32_t operator() (const ReductionKey& key) const
	    {
		// constants from Knuth
		static uint32_t M = 1664525, C = 1013904223;
 		uint32_t val = (key.res.ulog2 * M + key.res.vlog2 + C) * M + key.faceid;
		return val;
	    }
	};
    };
    typedef PtexHashMap<ReductionKey, FaceData*, ReductionKey::Hasher> ReductionMap;
    ReductionMap _reductions;

    z_stream_s _zstream;
};

#endif
