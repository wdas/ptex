#ifndef PtexReader_h
#define PtexReader_h

#include <stdio.h>
#include <zlib.h>
#include <vector>
#include <map>
#include "Ptexture.h"
#include "PtexIO.h"
#include "PtexCache.h"
#include "PtexUtils.h"

namespace PtexInternal {
#include "DGHashMap.h"
}
using namespace PtexInternal;

#ifndef NDEBUG
template<typename T> class safevector : public std::vector<T>
{
public:
    safevector() : std::vector<T>() {}
    safevector(size_t n, const T& val = T()) : std::vector<T>(n, val) {}
    const T& operator[] (size_t n) const { return std::vector<T>::at(n); }
    T& operator[] (size_t n) { return std::vector<T>::at(n); }
};
#else
#define safevector std::vector
#endif

class PtexReader : public PtexCachedFile, public PtexTexture, public PtexIO {
public:
    PtexReader(void** parent, PtexCacheImpl* cache);
    bool open(const char* path, std::string& error);

    virtual void release() { unref(); }
    virtual Ptex::MeshType meshType() { return MeshType(_header.meshtype); }
    virtual Ptex::DataType dataType() { return DataType(_header.datatype); }
    virtual int alphaChannel() { return _header.alphachan; }
    virtual int numChannels() { return _header.nchannels; }
    virtual int numFaces() { return _header.nfaces; }

    virtual PtexMetaData* getMetaData();
    virtual const Ptex::FaceInfo& getFaceInfo(int faceid);
    virtual void getData(int faceid, void* buffer, int stride);
    virtual PtexFaceData* getData(int faceid)
    { return PtexReader::getData(faceid, _faceinfo[faceid].res); }
    virtual PtexFaceData* getData(int faceid, Res res);

    const char* path() const { return _path.c_str(); }

    DataType datatype() const { return _header.datatype; }
    int nchannels() const { return _header.nchannels; }
    int pixelsize() const { return _pixelsize; }
    const Header& header() const { return _header; }
    const LevelInfo& levelinfo(int level) const { return _levelinfo[level]; }

    class MetaData : public PtexCachedData, public PtexMetaData {
    public:
	MetaData(void** parent, PtexCacheImpl* cache, int size)
	    : PtexCachedData(parent, cache, sizeof(*this) + size) {}
	virtual void release() { unref(); }

	virtual int numKeys() { return _entries.size(); }
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
	    if (e) { value = (const int8_t*) &e->value[0]; count = e->value.size(); }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const int16_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const int16_t*) &e->value[0]; 
		count = e->value.size()/sizeof(int16_t); 
	    }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const int32_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const int32_t*) &e->value[0]; 
		count = e->value.size()/sizeof(int32_t); 
	    }
	    else { value = 0; count = 0; }
	}
	
	virtual void getValue(const char* key, const float*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const float*) &e->value[0]; 
		count = e->value.size()/sizeof(float); 
	    }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const double*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const double*) &e->value[0]; 
		count = e->value.size()/sizeof(double); 
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
	ConstDataPtr(void* data) : _data(data) {}
	virtual void release() { delete this; }
	virtual Ptex::Res res() { return 0; }
	virtual bool isConstant() { return true; }
	virtual void* getPixel(int, int) { return _data; }
	virtual void* getPixel(float, float) { return _data; }
	virtual void* getData() { return _data; }
	virtual bool isTiled() { return false; }
	virtual Ptex::Res tileRes() { return 0; }
	virtual PtexFaceData* getTile(int) { return 0; }

    protected:
	void* _data;
    };


    class FaceData : public PtexCachedData, public PtexFaceData {
    public:
	FaceData(void** parent, PtexCacheImpl* cache, Res res, int size)
	    : PtexCachedData(parent, cache, size), _res(res) {}
	virtual void release() { unref(); }
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
	virtual void* getPixel(int uindex, int vindex)
	{
	    return (char*)_data + (vindex*_res.u() + uindex) * _pixelsize;
	}
	virtual void* getPixel(float u, float v)
	{
	    return PackedFace::getPixel(index(u, _res.u()),index(v, _res.v()));
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
	virtual void* getPixel(int, int) { return _data; }
	virtual void* getPixel(float, float) { return _data; }
	virtual void reduce(FaceData*&, PtexReader*,
			    Res newres, PtexUtils::ReduceFn);
    };


    class TiledFaceBase : public FaceData {
    public:
	TiledFaceBase(void** parent, PtexCacheImpl* cache, Res res,
		      Res tileres, DataType dt, int nchan, int size)
	    : FaceData(parent, cache, res, size),
	      _tileres(tileres),
	      _dt(dt),
	      _nchan(nchan),
	      _ntilesu(_res.ntilesu(tileres)),
	      _ntilesv(_res.ntilesv(tileres)),
	      _ntiles(_ntilesu*_ntilesv),
	      _pixelsize(DataSize(dt)*nchan),
	      _tiles(_ntiles) {}

	virtual void release() { unref(); }
	virtual bool isConstant() { return false; }
	virtual void* getPixel(int uindex, int vindex);

	virtual void* getPixel(float u, float v)
	{
	    return TiledFaceBase::getPixel(index(u, _res.u()),index(v, _res.v()));
	}
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
	TiledFace(void** parent, PtexCacheImpl* cache, Res res,
		  Res tileres, PtexReader* reader)
	    : TiledFaceBase(parent, cache, res, tileres, 
			    reader->datatype(), reader->nchannels(),
			    sizeof(*this) + _ntiles*(sizeof(FaceDataHeader)+
						     sizeof(off_t)+
						     sizeof(FaceData*))),
	      _reader(reader),
	      _fdh(_ntiles),
	      _offsets(_ntiles)
	{
	    _reader->ref();
	}
	virtual PtexFaceData* getTile(int tile)
	{
	    FaceData*& f = _tiles[tile];
	    if (!f) _reader->readFace(_offsets[tile], _fdh[tile], _tileres, f);
	    else f->ref();
	    return f;
	}

    protected:
	friend class PtexReader;
	virtual ~TiledFace() { _reader->unref(); }
	PtexReader* _reader;
	safevector<FaceDataHeader> _fdh;
	safevector<off_t> _offsets;
    };	    


    class TiledReducedFace : public TiledFaceBase {
    public:
	TiledReducedFace(void** parent, PtexCacheImpl* cache, Res res, 
			 Res tileres, DataType dt, int nchan,
			 TiledFaceBase* parentface, PtexUtils::ReduceFn reducefn)
	    : TiledFaceBase(parent, cache, res, tileres, dt, nchan,
			    sizeof(*this) + _ntiles*sizeof(FaceData*)),
	      _parentface(parentface),
	      _reducefn(reducefn)
	{
	    _parentface->ref(); 
	}
	virtual PtexFaceData* getTile(int tile);

    protected:
	virtual ~TiledReducedFace() { _parentface->unref(); }
	TiledFaceBase* _parentface;
	PtexUtils::ReduceFn* _reducefn;
    };


    class Level : public PtexCachedData {
    public:
	safevector<FaceDataHeader> fdh;
	safevector<off_t> offsets;
	safevector<FaceData*> faces;
	
	Level(void** parent, PtexCacheImpl* cache, int nfaces) 
	    : PtexCachedData(parent, cache, 
			     sizeof(*this) + nfaces * (sizeof(FaceDataHeader) +
						       sizeof(off_t) + 
						       sizeof(FaceData*))),
	      fdh(nfaces),
	      offsets(nfaces),
	      faces(nfaces) {}
    protected:
	virtual ~Level() { orphanList(faces); }
    };

protected:
    virtual ~PtexReader();
    void setError(const char* error)
    {
	_error = error; _error += " PtexFile: "; _error += _path;
	printf("%s\n", _error.c_str());
	_ok = 0;
    }

    off_t tell() { return (_pos = ftello(_fp)); }
    void seek(off_t pos) 
    {
	if (pos != _pos) {
	    fseeko(_fp, pos, SEEK_SET); 
	    _pos = pos;
	}
    }
    off_t seekToEnd() 
    {
	fseeko(_fp, 0, SEEK_END);
	_pos = ftello(_fp);	
	return _pos;
    }
    void skip(off_t bytes)
    {
	seek(_pos + bytes);
    }

    bool readBlock(void* data, int size);
    bool readZipBlock(void* data, int zipsize, int unzipsize);
    Level* getLevel(int levelid)
    {
	Level*& level = _levels[levelid];
	if (!level) readLevel(levelid, level);
	else level->ref();
	return level;
    }

    uint8_t* getConstData() { if (!_constdata) readConstData(); return _constdata; }
    FaceData* getFace(Level* level, int faceid, Res res)
    {
	FaceData*& face = level->faces[faceid];
	if (!face) readFace(level->offsets[faceid], level->fdh[faceid], res, face);
	else face->ref();
	return face;
    }

    void readFaceInfo();
    void readLevelInfo();
    void readConstData();
    void readLevel(int levelid, Level*& level);
    void readFace(off_t pos, const FaceDataHeader& fdh, Res res, FaceData*& face);
    void readMetaData();
    void readMetaDataBlock(off_t pos, int zipsize, int memsize);
    void readEditData();
    void readEditFaceData();
    void readEditMetaData();

    void computeOffsets(off_t pos, int noffsets, const FaceDataHeader* fdh, off_t* offsets)
    {
	off_t* end = offsets + noffsets;
	while (offsets != end) { *offsets++ = pos; pos += fdh->blocksize; fdh++; }
    }

    static int index(float u, int uw)
    {
	int index = int(u*uw); if (index >= uw) index = uw-1;
	return index;
    }

    bool _ok;
    std::string _error;
    FILE* _fp;
    off_t _pos;
    std::string _path;
    Header _header;
    off_t _extheaderpos;
    off_t _faceinfopos;
    off_t _constdatapos;
    off_t _levelinfopos;
    off_t _leveldatapos;
    off_t _metadatapos;
    off_t _editdatapos;
    int _pixelsize;
    uint8_t* _constdata;
    MetaData* _metadata;

    safevector<FaceInfo> _faceinfo;
    safevector<uint32_t> _rfaceids;
    safevector<LevelInfo> _levelinfo;
    safevector<off_t> _levelpos;
    safevector<Level*> _levels;

    struct MetaEdit
    {
	off_t pos;
	int zipsize;
	int memsize;
    };
    safevector<MetaEdit> _metaedits;

    struct FaceEdit
    {
	off_t pos;
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
    typedef DGHashMap<ReductionKey, FaceData*, ReductionKey::Hasher> ReductionMap;
    ReductionMap _reductions;

    z_stream_s _zstream;
};

#endif
