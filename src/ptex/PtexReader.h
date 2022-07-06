#ifndef PtexReader_h
#define PtexReader_h

/*
PTEX SOFTWARE
Copyright 2014 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/
#include <stdio.h>
#include <zlib.h>
#include <vector>
#include <string>
#include <map>
#include <errno.h>
#include "Ptexture.h"
#include "PtexIO.h"
#include "PtexUtils.h"

#include "PtexHashMap.h"

PTEX_NAMESPACE_BEGIN

class PtexReader : public PtexTexture {
public:
    PtexReader(bool premultiply, PtexInputHandler* inputHandler, PtexErrorHandler* errorHandler);
    virtual ~PtexReader();
    virtual void release() { delete this; }
    bool needToOpen() const { return _needToOpen; }
    bool open(const char* path, Ptex::String& error);
    void prune();
    void purge();
    void setPendingPurge() { _pendingPurge = true; }
    bool pendingPurge() const { return _pendingPurge; }
    bool tryClose();
    bool ok() const { return _ok; }
    bool isOpen() { return _fp; }
    void invalidate() {
        _ok = false;
        _needToOpen = false;
    }

    void increaseMemUsed(size_t amount) { if (amount) AtomicAdd(&_memUsed, amount); }
    void logOpen() { AtomicIncrement(&_opens); }
    void logBlockRead() { AtomicIncrement(&_blockReads); }

    virtual const char* path() { return _path.c_str(); }

    virtual Info getInfo() {
        Info info;
        info.meshType = MeshType(_header.meshtype);
        info.dataType = DataType(_header.datatype);
        info.uBorderMode = BorderMode(_extheader.ubordermode);
        info.vBorderMode = BorderMode(_extheader.vbordermode);
        info.edgeFilterMode = EdgeFilterMode(_extheader.edgefiltermode);
        info.alphaChannel = _header.alphachan;
        info.numChannels = _header.nchannels;
        info.numFaces = _header.nfaces;
        return info;
    }

    virtual Ptex::MeshType meshType() { return MeshType(_header.meshtype); }
    virtual Ptex::DataType dataType() { return DataType(_header.datatype); }
    virtual Ptex::BorderMode uBorderMode() { return BorderMode(_extheader.ubordermode); }
    virtual Ptex::BorderMode vBorderMode() { return BorderMode(_extheader.vbordermode); }
    virtual Ptex::EdgeFilterMode edgeFilterMode() { return EdgeFilterMode(_extheader.edgefiltermode); }
    virtual int alphaChannel() { return _header.alphachan; }
    virtual int numChannels() { return _header.nchannels; }
    virtual int numFaces() { return _header.nfaces; }
    virtual bool hasEdits() { return _hasEdits; }
    virtual bool hasMipMaps() { return _header.nlevels > 1; }

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

    DataType datatype() const { return DataType(_header.datatype); }
    int nchannels() const { return _header.nchannels; }
    int pixelsize() const { return _pixelsize; }
    const Header& header() const { return _header; }
    const ExtHeader& extheader() const { return _extheader; }
    const LevelInfo& levelinfo(int level) const { return _levelinfo[level]; }

    class MetaData : public PtexMetaData {
    public:
	MetaData(PtexReader* reader)
	    : _reader(reader) {}
        ~MetaData() {}
	virtual void release() {}

	virtual int numKeys() { return int(_entries.size()); }
	virtual void getKey(int index, const char*& key, MetaDataType& type)
	{
	    if (index < 0 || index >= int(_entries.size())) {
		return;
	    }
	    Entry* e = _entries[index];
	    key = e->key;
	    type = e->type;
	}

	virtual bool findKey(const char* key, int& index, MetaDataType& type)
	{
	    MetaMap::iterator iter = _map.find(key);
	    if (iter==_map.end()) {
		index = -1;
		return false;
	    }
	    index = iter->second.index;
	    type = iter->second.type;
	    return true;
	}

	virtual void getValue(const char* key, const char*& value)
	{
	    int index = -1;
	    MetaDataType type;
	    if (!findKey(key, index, type)) {
                value = 0;
                return;
            }
            Entry* e = getEntry(index);
	    if (e && e->type == mdt_string) value = (const char*) e->data;
	    else value = 0;
	}

	virtual void getValue(int index, const char*& value)
	{
	    if (index < 0 || index >= int(_entries.size())) { value = 0; return; }
	    Entry* e = getEntry(index);
	    if (e && e->type == mdt_string) value = (const char*) e->data;
	    else value = 0;
	}

        template<typename T>
        void getValue(int index, MetaDataType requestedType, const T*& value, int& count)
        {
	    if (index < 0 || index >= int(_entries.size())) {
                value = 0;
                count = 0;
                return;
            }
	    Entry* e = getEntry(index);
	    if (e && e->type == requestedType) {
	        value = (const T*) e->data;
	        count = int(e->datasize/sizeof(T));
	    }
	    else { value = 0; count = 0; }
        }

        template<typename T>
        void getValue(const char* key, MetaDataType requestedType, const T*& value, int& count)
        {
	    int index = -1;
	    MetaDataType type;
            findKey(key, index, type);
            getValue<T>(index, requestedType, value, count);
        }

	virtual void getValue(const char* key, const int8_t*& value, int& count)
	{
            getValue<int8_t>(key, mdt_int8, value, count);
	}

	virtual void getValue(int index, const int8_t*& value, int& count)
	{
            getValue<int8_t>(index, mdt_int8, value, count);
	}

	virtual void getValue(const char* key, const int16_t*& value, int& count)
	{
            getValue<int16_t>(key, mdt_int16, value, count);
	}

	virtual void getValue(int index, const int16_t*& value, int& count)
	{
            getValue<int16_t>(index, mdt_int16, value, count);
	}

	virtual void getValue(const char* key, const int32_t*& value, int& count)
	{
            getValue<int32_t>(key, mdt_int32, value, count);
	}

	virtual void getValue(int index, const int32_t*& value, int& count)
	{
            getValue<int32_t>(index, mdt_int32, value, count);
	}

	virtual void getValue(const char* key, const float*& value, int& count)
	{
            getValue<float>(key, mdt_float, value, count);
	}

	virtual void getValue(int index, const float*& value, int& count)
	{
            getValue<float>(index, mdt_float, value, count);
	}

	virtual void getValue(const char* key, const double*& value, int& count)
	{
            getValue<double>(key, mdt_double, value, count);
	}

	virtual void getValue(int index, const double*& value, int& count)
	{
            getValue<double>(index, mdt_double, value, count);
	}

	void addEntry(uint8_t keysize, const char* key, uint8_t datatype,
		      uint32_t datasize, const void* data, size_t& metaDataMemUsed)
	{
	    Entry* e = newEntry(keysize, key, datatype, datasize, metaDataMemUsed);
	    e->data = new char[datasize];
	    memcpy(e->data, data, datasize);
            metaDataMemUsed += datasize;
	}

	void addLmdEntry(uint8_t keysize, const char* key, uint8_t datatype,
			 uint32_t datasize, FilePos filepos, uint32_t zipsize,
                         size_t& metaDataMemUsed)
	{
	    Entry* e = newEntry(keysize, key, datatype, datasize, metaDataMemUsed);
	    e->isLmd = true;
	    e->lmdData = 0;
	    e->lmdPos = filepos;
	    e->lmdZipSize = zipsize;
	}

        size_t selfDataSize()
        {
            return sizeof(*this) + sizeof(Entry*) * _entries.capacity();
        }

    protected:
	class LargeMetaData
	{
	 public:
	    LargeMetaData(int size)
		: _data(new char [size]) {}
	    virtual ~LargeMetaData() { delete [] _data; }
	    void* data() { return _data; }
        private:
            LargeMetaData(const LargeMetaData&);
	    char* _data;
	};

	struct Entry {
	    const char* key;	      // ptr to map key string
	    MetaDataType type;	      // meta data type
	    uint32_t datasize;	      // size of data in bytes
	    char* data;		      // if lmd, data only valid when lmd is loaded and ref'ed
	    bool isLmd;		      // true if data is a large meta data block
	    LargeMetaData* lmdData;   // large meta data (lazy-loaded)
	    FilePos lmdPos;	      // large meta data file position
	    uint32_t lmdZipSize;      // large meta data size on disk
	    uint32_t index;           // index in vector

	    Entry() :
		key(0), type(MetaDataType(0)), datasize(0), data(0),
		isLmd(0), lmdData(0), lmdPos(0), lmdZipSize(0) {}
	    ~Entry() { clear(); }
	    void clear() {
		if (isLmd) {
		    isLmd = 0;
		    if (lmdData) { delete lmdData; lmdData = 0; }
		    lmdPos = 0;
		    lmdZipSize = 0;
		}
		else {
		    if (data) { delete [] data; }
		}
                data = 0;
	    }
	};

	Entry* newEntry(uint8_t keysize, const char* key, uint8_t datatype, uint32_t datasize, size_t& metaDataMemUsed)
	{
	    std::pair<MetaMap::iterator,bool> result =
		_map.insert(std::make_pair(std::string(key, keysize), Entry()));
	    Entry* e = &result.first->second;
	    bool newentry = result.second;
	    uint32_t index = 0;
	    if (newentry) {
		index = uint32_t(_entries.size());
		_entries.push_back(e);
	    }
	    else {
		index = e->index;
		e->clear();
	    }
	    e->key = result.first->first.c_str();
	    e->type = MetaDataType(datatype);
	    e->datasize = datasize;
	    e->index = index;
            metaDataMemUsed += sizeof(std::string) + keysize + 1 + sizeof(Entry);
            return e;
        }

        PTEXAPI Entry* getEntry(int index);

        PtexReader* _reader;
        typedef std::map<std::string, Entry> MetaMap;
        MetaMap _map;
        std::vector<Entry*> _entries;
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


    class FaceData : public PtexFaceData {
    public:
        FaceData(Res resArg)
            : _res(resArg) {}
        virtual ~FaceData() {}
        virtual void release() { }
        virtual Ptex::Res res() { return _res; }
        virtual FaceData* reduce(PtexReader*, Res newres, PtexUtils::ReduceFn, size_t& newMemUsed) = 0;
    protected:
        Res _res;
    };

    class PackedFace : public FaceData {
    public:
        PackedFace(Res resArg, int pixelsize, int size)
            : FaceData(resArg),
              _pixelsize(pixelsize), _data(new char [size]) {}
        void* data() { return _data; }
        virtual bool isConstant() { return false; }
        virtual void getPixel(int u, int v, void* result)
        {
            memcpy(result, _data + (v*_res.u() + u) * _pixelsize, _pixelsize);
        }
        virtual void* getData() { return _data; }
        virtual bool isTiled() { return false; }
        virtual Ptex::Res tileRes() { return _res; }
        virtual PtexFaceData* getTile(int) { return 0; }
        virtual FaceData* reduce(PtexReader*, Res newres, PtexUtils::ReduceFn, size_t& newMemUsed);

    protected:
        virtual ~PackedFace() { delete [] _data; }

        int _pixelsize;
        char* _data;
    };

    class ConstantFace : public PackedFace {
    public:
        ConstantFace(int pixelsize)
            : PackedFace(0, pixelsize, pixelsize) {}
        virtual bool isConstant() { return true; }
        virtual void getPixel(int, int, void* result) { memcpy(result, _data, _pixelsize); }
        virtual FaceData* reduce(PtexReader*, Res newres, PtexUtils::ReduceFn, size_t& newMemUsed);
    };

    class ErrorFace : public ConstantFace {
        bool _deleteOnRelease;
    public:
        ErrorFace(void* errorPixel, int pixelsize, bool deleteOnRelease)
            : ConstantFace(pixelsize), _deleteOnRelease(deleteOnRelease)
        {
            memcpy(_data, errorPixel, pixelsize);
        }
        virtual void release() { if (_deleteOnRelease) delete this; }
    };

    class TiledFaceBase : public FaceData {
    public:
        TiledFaceBase(PtexReader* reader, Res resArg, Res tileresArg)
            : FaceData(resArg),
              _reader(reader),
              _tileres(tileresArg)
        {
            _dt = reader->datatype();
            _nchan = reader->nchannels();
            _pixelsize = DataSize(_dt)*_nchan;
            _ntilesu = _res.ntilesu(tileresArg);
            _ntilesv = _res.ntilesv(tileresArg);
            _ntiles = _ntilesu*_ntilesv;
            _tiles.resize(_ntiles);
        }

        virtual void release() { }
        virtual bool isConstant() { return false; }
        virtual void getPixel(int u, int v, void* result);
        virtual void* getData() { return 0; }
        virtual bool isTiled() { return true; }
        virtual Ptex::Res tileRes() { return _tileres; }
        virtual FaceData* reduce(PtexReader*, Res newres, PtexUtils::ReduceFn, size_t& newMemUsed);
        Res tileres() const { return _tileres; }
        int ntilesu() const { return _ntilesu; }
        int ntilesv() const { return _ntilesv; }
        int ntiles() const { return _ntiles; }

    protected:
        size_t baseExtraMemUsed() { return _tiles.size() * sizeof(_tiles[0]); }

        virtual ~TiledFaceBase() {
            for (std::vector<FaceData*>::iterator i = _tiles.begin(); i != _tiles.end(); ++i) {
                if (*i) delete *i;
            }
        }

        PtexReader* _reader;
        Res _tileres;
        DataType _dt;
        int _nchan;
        int _ntilesu;
        int _ntilesv;
        int _ntiles;
        int _pixelsize;
        std::vector<FaceData*> _tiles;
    };


    class TiledFace : public TiledFaceBase {
    public:
        TiledFace(PtexReader* reader, Res resArg, Res tileresArg, int levelid)
            : TiledFaceBase(reader, resArg, tileresArg),
              _levelid(levelid)
        {
            _fdh.resize(_ntiles),
            _offsets.resize(_ntiles);
        }
        virtual PtexFaceData* getTile(int tile)
        {
            FaceData*& f = _tiles[tile];
            if (!f) readTile(tile, f);
            return f;
        }
        void readTile(int tile, FaceData*& data);
        size_t memUsed() {
            return sizeof(*this) + baseExtraMemUsed() + _fdh.size() * (sizeof(_fdh[0]) + sizeof(_offsets[0]));
        }

    protected:
        friend class PtexReader;
        int _levelid;
        std::vector<FaceDataHeader> _fdh;
        std::vector<FilePos> _offsets;
    };


    class TiledReducedFace : public TiledFaceBase {
    public:
        TiledReducedFace(PtexReader* reader, Res resArg, Res tileresArg,
                         TiledFaceBase* parentface, PtexUtils::ReduceFn reducefn)
            : TiledFaceBase(reader, resArg, tileresArg),
              _parentface(parentface),
              _reducefn(reducefn)
        {
        }
        ~TiledReducedFace()
        {
        }
        virtual PtexFaceData* getTile(int tile);

        size_t memUsed() { return sizeof(*this) + baseExtraMemUsed(); }

    protected:
        TiledFaceBase* _parentface;
        PtexUtils::ReduceFn* _reducefn;
    };


    class Level {
    public:
        std::vector<FaceDataHeader> fdh;
        std::vector<FilePos> offsets;
        std::vector<FaceData*> faces;

        Level(int nfaces)
            : fdh(nfaces),
              offsets(nfaces),
              faces(nfaces) {}

        ~Level() {
            for (std::vector<FaceData*>::iterator i = faces.begin(); i != faces.end(); ++i) {
                if (*i) delete *i;
            }
        }

        size_t memUsed() {
            return sizeof(*this) + fdh.size() * (sizeof(fdh[0]) +
                                                 sizeof(offsets[0]) +
                                                 sizeof(faces[0]));
        }
    };


protected:
    void setError(const char* error)
    {
        std::string msg = error;
        msg += " PtexFile: ";
        msg += _path;
        msg += "\n";
        if (_err) _err->reportError(msg.c_str());
        else std::cerr << msg;
        _ok = 0;
    }

    FilePos tell() { return _pos; }
    void seek(FilePos pos)
    {
        if (!_fp && !reopenFP()) return;
        logBlockRead();
        if (pos != _pos) {
            _io->seek(_fp, pos);
            _pos = pos;
        }
    }

    void closeFP();
    bool reopenFP();
    bool readBlock(void* data, int size, bool reportError=true);
    bool readZipBlock(void* data, int zipsize, int unzipsize);
    Level* getLevel(int levelid)
    {
        Level*& level = _levels[levelid];
        if (!level) readLevel(levelid, level);
        return level;
    }

    uint8_t* getConstData() { return _constdata; }
    FaceData* getFace(int levelid, Level* level, int faceid, Res res)
    {
        FaceData*& face = level->faces[faceid];
        if (!face) readFace(levelid, level, faceid, res);
        return face;
    }

    void readFaceInfo();
    void readLevelInfo();
    void readConstData();
    void readLevel(int levelid, Level*& level);
    void readFace(int levelid, Level* level, int faceid, Res res);
    void readFaceData(FilePos pos, FaceDataHeader fdh, Res res, int levelid, FaceData*& face);
    void readMetaData();
    void readMetaDataBlock(MetaData* metadata, FilePos pos, int zipsize, int memsize, size_t& metaDataMemUsed);
    void readLargeMetaDataHeaders(MetaData* metadata, FilePos pos, int zipsize, int memsize, size_t& metaDataMemUsed);
    void readEditData();
    void readEditFaceData();
    void readEditMetaData();

    FaceData* errorData(bool deleteOnRelease=false)
    {
        return new ErrorFace(&_errorPixel[0], _pixelsize, deleteOnRelease);
    }

    void computeOffsets(FilePos pos, int noffsets, const FaceDataHeader* fdh, FilePos* offsets)
    {
        FilePos* end = offsets + noffsets;
        while (offsets != end) { *offsets++ = pos; pos += fdh->blocksize(); fdh++; }
    }

    class DefaultInputHandler : public PtexInputHandler
    {
        char* buffer;
     public:
        DefaultInputHandler() : buffer(0) {}
        virtual Handle open(const char* path) {
            FILE* fp = fopen(path, "rb");
            if (fp) {
                buffer = new char [IBuffSize];
                setvbuf(fp, buffer, _IOFBF, IBuffSize);
            }
            else buffer = 0;
            return (Handle) fp;
        }
        virtual void seek(Handle handle, int64_t pos) { fseeko((FILE*)handle, pos, SEEK_SET); }
        virtual size_t read(void* bufferArg, size_t size, Handle handle) {
            return fread(bufferArg, size, 1, (FILE*)handle) == 1 ? size : 0;
        }
        virtual bool close(Handle handle) {
            bool ok = handle && (fclose((FILE*)handle) == 0);
            if (buffer) { delete [] buffer; buffer = 0; }
            return ok;
        }
        virtual const char* lastError() { return strerror(errno); }
    };

    Mutex readlock;
    DefaultInputHandler _defaultIo;   // Default IO handler
    PtexInputHandler* _io;            // IO handler
    PtexErrorHandler* _err;           // Error handler
    bool _premultiply;                // true if reader should premultiply the alpha chan
    bool _ok;                         // flag set to false if open or read error occurred
    bool _needToOpen;                 // true if file needs to be opened (or reopened after a purge)
    bool _pendingPurge;               // true if a purge attempt was made but file was busy
    PtexInputHandler::Handle _fp;     // file pointer
    FilePos _pos;                     // current seek position
    std::string _path;                // current file path
    Header _header;                   // the header
    ExtHeader _extheader;             // extended header
    FilePos _faceinfopos;             // file positions of data sections
    FilePos _constdatapos;            // ...
    FilePos _levelinfopos;
    FilePos _leveldatapos;
    FilePos _metadatapos;
    FilePos _lmdheaderpos;
    FilePos _lmddatapos;
    FilePos _editdatapos;
    int _pixelsize;                   // size of a pixel in bytes
    uint8_t* _constdata;              // constant pixel value per face
    MetaData* _metadata;              // meta data (read on demand)
    bool _hasEdits;                   // has edit blocks

    std::vector<FaceInfo> _faceinfo;   // per-face header info
    std::vector<uint32_t> _rfaceids;   // faceids sorted in reduction order
    std::vector<LevelInfo> _levelinfo; // per-level header info
    std::vector<FilePos> _levelpos;    // file position of each level's data
    std::vector<Level*> _levels;              // level data (read on demand)

    struct MetaEdit
    {
        FilePos pos;
        int zipsize;
        int memsize;
    };
    std::vector<MetaEdit> _metaedits;

    struct FaceEdit
    {
        FilePos pos;
        int faceid;
        FaceDataHeader fdh;
    };
    std::vector<FaceEdit> _faceedits;

    class ReductionKey {
        int64_t _val;
    public:
        ReductionKey() : _val(-1) {}
        ReductionKey(uint32_t faceid, Res res)
            : _val( int64_t(faceid)<<32 | uint32_t(16777619*((res.val()<<16) ^ faceid)) ) {}

        void copy(volatile ReductionKey& key) volatile
        {
            _val = key._val;
        }

        void move(volatile ReductionKey& key) volatile
        {
            _val = key._val;
        }

        bool matches(const ReductionKey& key) volatile
        {
            return _val == key._val;
        }
        bool isEmpty() volatile { return _val==-1; }
        uint32_t hash() volatile
        {
            return uint32_t(_val);
        }
    };
    typedef PtexHashMap<ReductionKey, FaceData*> ReductionMap;
    ReductionMap _reductions;
    std::vector<char> _errorPixel; // referenced by errorData()

    z_stream_s _zstream;
    size_t _baseMemUsed;
    volatile size_t _memUsed;
    volatile size_t _opens;
    volatile size_t _blockReads;
};

PTEX_NAMESPACE_END

#endif
