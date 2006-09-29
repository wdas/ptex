#include <iostream>
#include <sstream>
#include <errno.h>
#include <stdio.h>
#include "DGDict.h"
#include "Ptexture.h"
#include "PtexUtils.h"
#include "PtexReader.h"



PtexTexture* PtexTexture::open(const char* path, std::string& error)
{
    PtexCache* cache = PtexCache::create(1, 1024*1024);
    return cache->get(path, error);
}


bool PtexReader::open(const char* path, std::string& error)
{
    if (!LittleEndian()) {
	error = "Ptex library doesn't currently support big-endian cpu's";
	return 0;
    }
    _path = path;
    _fp = fopen(path, "rb");
    if (!_fp) { 
	std::stringstream str;
	str << "Can't open ptex file: " << path << "\n" << strerror(errno);
	error = str.str();
	return 0;
    }
    readBlock(&_header, HeaderSize);
    if (_header.magic != Magic) {
	std::stringstream str;
	str << "Not a ptex file: " << path;
	error = str.str();
	return 0;
    }
    if (_header.version != 1) {
	std::stringstream str;
	str << "Unsupported ptex file version (" << _header.version << "): "
	    << path;
	error = str.str();
	return 0;
    }
    _pixelsize = _header.pixelSize();

    // compute offsets of various blocks
    off_t pos = tell();
    _extheaderpos = pos;  pos += _header.extheadersize;
    _faceinfopos = pos;   pos += _header.faceinfosize;
    _constdatapos = pos;  pos += _header.constdatasize;
    _levelinfopos = pos;  pos += _header.levelinfosize;
    _leveldatapos = pos;  pos += _header.leveldatasize;
    _metadatapos = pos;   pos += _header.metadatazipsize;
    _editdatapos = pos;
    
    // read basic file info
    readFaceInfo();
    readConstData();
    readLevelInfo();
    readEditData();

    return 1;
}


PtexReader::PtexReader(void** parent, PtexCacheImpl* cache)
    : PtexCachedFile(parent, cache),
      _ok(true),
      _fp(0),
      _pos(0),
      _pixelsize(0),
      _constdata(0),
      _metadata(0)
{
    memset(&_header, 0, sizeof(_header));
    memset(&_zstream, 0, sizeof(_zstream));
    inflateInit(&_zstream);
}


PtexReader::~PtexReader()
{
    if (_fp) fclose(_fp);
    if (_constdata) free(_constdata);
    orphanList(_levels);
    inflateEnd(&_zstream);
}


const Ptex::FaceInfo& PtexReader::getFaceInfo(int faceid)
{
    if (faceid >= 0 && uint32_t(faceid) < _faceinfo.size())
	return _faceinfo[faceid];

    static Ptex::FaceInfo dummy;
    return dummy;
}


void PtexReader::readFaceInfo()
{
    if (_faceinfo.empty()) {
	// read compressed face info block
	seek(_faceinfopos);
	_faceinfo.resize(_header.nfaces);
	readZipBlock(&_faceinfo[0], _header.faceinfosize, 
		     sizeof(FaceInfo)*_header.nfaces);
    }
}



void PtexReader::readLevelInfo()
{
    if (_levelinfo.empty()) {
	// read level info block
	seek(_levelinfopos);
	_levelinfo.resize(_header.nlevels);
	readBlock(&_levelinfo[0], LevelInfoSize*_header.nlevels);

	// initialize related data
	_levels.resize(_header.nlevels);
	_levelpos.resize(_header.nlevels);
	off_t pos = _leveldatapos;
	for (int i = 0; i < _header.nlevels; i++) {
	    _levelpos[i] = pos;
	    pos += _levelinfo[i].leveldatasize;
	}
    }
}


void PtexReader::readConstData()
{
    if (!_constdata) {
	// read compressed constant data block
	seek(_constdatapos);
	_constdata = (uint8_t*) malloc(_pixelsize * _header.nfaces);
	readZipBlock(_constdata, _header.constdatasize, _pixelsize * _header.nfaces);
    }
}


PtexMetaData* PtexReader::getMetaData()
{
    if (_metadata) _metadata->ref();
    else readMetaData();
    return _metadata;
}


void PtexReader::readMetaData()
{
    int totalsize = _header.metadatamemsize;
    for (int i = 0, size = _metaedits.size(); i < size; i++)
	totalsize += _metaedits[i].memsize;
    _metadata = new MetaData((void**)&_metadata, _cache, totalsize);
    if (totalsize == 0) return;

    if (_header.metadatamemsize)
	readMetaDataBlock(_metadatapos, _header.metadatazipsize, _header.metadatamemsize);
    for (int i = 0, size = _metaedits.size(); i < size; i++)
	readMetaDataBlock(_metaedits[i].pos, _metaedits[i].zipsize, _metaedits[i].memsize);
}


void PtexReader::readMetaDataBlock(off_t pos, int zipsize, int memsize)
{
    seek(pos);
    // read from file
    bool useMalloc = memsize > AllocaMax;
    char* buff = useMalloc ? (char*) malloc(memsize) : (char*)alloca(memsize);

    if (readZipBlock(buff, zipsize, memsize)) {
	// unpack data entries
	char* ptr = buff;
	char* end = ptr + memsize;
	while (ptr < end) {
	    uint8_t keysize = *ptr++;
	    char* key = (char*)ptr; ptr += keysize;
	    uint8_t datatype = *ptr++;
	    uint32_t datasize; memcpy(&datasize, ptr, sizeof(datasize));
	    ptr += sizeof(datasize);
	    char* data = ptr; ptr += datasize;
	    _metadata->addEntry(keysize-1, key, datatype, datasize, data);
	}
    }
    if (useMalloc) free(buff);
}


void PtexReader::readEditData()
{
    off_t end = seekToEnd();
    seek(_editdatapos);
    while (tell() != end) {
	// read the edit data header
	EditDataHeader edh;
	if (!readBlock(&edh, EditDataHeaderSize)) return;
	off_t pos = tell();
	switch (edh.edittype) {
	case et_editfacedata:   readEditFaceData(); break;
	case et_editmetadata:   readEditMetaData(); break;
	}
	// advance to next edit
	seek(pos + edh.editsize);
    }    
}


void PtexReader::readEditFaceData()
{
    // read header
    EditFaceDataHeader efdh;
    if (!readBlock(&efdh, EditFaceDataHeaderSize)) return;

    // update face info
    int faceid = efdh.faceid;
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return;
    FaceInfo& f = _faceinfo[faceid];
    f = efdh.faceinfo;
    f.flags |= FaceInfo::flag_hasedits;

    if (f.isConstant()) {
	// read const edits now
	readBlock(_constdata + _pixelsize * faceid, _pixelsize);
    }
    else {
	// otherwise record header info for later
	_faceedits.push_back(FaceEdit());
	FaceEdit& e = _faceedits.back();
	e.pos = tell();
	e.faceid = faceid;
	e.fdh = efdh.fdh;
    }
}


void PtexReader::readEditMetaData()
{
    // read header
    EditMetaDataHeader emdh;
    if (!readBlock(&emdh, EditMetaDataHeaderSize)) return;

    // record header info for later
    _metaedits.push_back(MetaEdit());
    MetaEdit& e = _metaedits.back();
    e.pos = tell();
    e.zipsize = emdh.metadatazipsize;
    e.memsize = emdh.metadatamemsize;
}


bool PtexReader::readBlock(void* data, int size)
{
    int result = fread(data, size, 1, _fp);
    if (result == 1) {
	_pos += size;
	return 1;
    }
    setError("PtexReader error: read failed (EOF)");
    return 0;
}


bool PtexReader::readZipBlock(void* data, int zipsize, int unzipsize)
{
    void* buff = alloca(BlockSize);
    _zstream.next_out = (Bytef*) data;
    _zstream.avail_out = unzipsize;
    
    while (1) {
	int size = (zipsize < BlockSize) ? zipsize : BlockSize;
	zipsize -= size;
	if (!readBlock(buff, size)) break;
	_zstream.next_in = (Bytef*) buff;
	_zstream.avail_in = size;
	int zresult = inflate(&_zstream, zipsize ? Z_NO_FLUSH : Z_FINISH);
	if (zresult == Z_STREAM_END) break;
	if (zresult != Z_OK) {
	    setError("PtexReader error: unzip failed, file corrupt");
	    inflateReset(&_zstream);
	    return 0;
	}
    }
    
    int total = _zstream.total_out;
    inflateReset(&_zstream);
    return total == unzipsize;
}


void PtexReader::readLevel(int levelid, Level*& level)
{
    LevelInfo& l = _levelinfo[levelid];
    level = new Level((void**)&level, _cache, l.nfaces);
    seek(_levelpos[levelid]);
    readZipBlock(&level->fdh[0], l.levelheadersize, FaceDataHeaderSize * l.nfaces);
    computeOffsets(tell(), l.nfaces, &level->fdh[0], &level->offsets[0]);

    // apply edits (if any) to level 0
    if (levelid == 0) {
	for (int i = 0, size = _faceedits.size(); i < size; i++) {
	    FaceEdit& e = _faceedits[i];
	    level->fdh[e.faceid] = e.fdh;
	    level->offsets[e.faceid] = e.pos;
	}
    }
}


void PtexReader::readFace(off_t pos, const FaceDataHeader& fdh, Res res,
			  FaceData*& face)
{
    seek(pos);
    switch (fdh.encoding) {
    case enc_constant: 
	{
	    ConstantFace* pf = new ConstantFace((void**)&face, _cache, _pixelsize);
	    readBlock(pf->data(), _pixelsize);
	    face = pf;
	}
	break;
    case enc_tiled:
	{
	    Res tileres;
	    readBlock(&tileres, sizeof(tileres));
	    uint32_t tileheadersize;
	    readBlock(&tileheadersize, sizeof(tileheadersize));
	    int ntiles = res.ntiles(tileres);
	    TiledFace* tf = new TiledFace((void**)&face, _cache, res,
					  this, tileres, ntiles);
	    readZipBlock(&tf->_fdh[0], tileheadersize, FaceDataHeaderSize * ntiles);
	    computeOffsets(tell(), ntiles, &tf->_fdh[0], &tf->_offsets[0]);
	    face = tf;
	}
	break;
    case enc_zipped:
    case enc_diffzipped:
	{
	    int uw = res.u(), vw = res.v();
	    int npixels = uw * vw;
	    int unpackedSize = _pixelsize * npixels;
	    PackedFace* pf = new PackedFace((void**)&face, _cache, 
					    res, _pixelsize, unpackedSize);
	    void* tmp = alloca(unpackedSize);
	    readZipBlock(tmp, fdh.blocksize, unpackedSize);
	    face = pf;
	    if (fdh.encoding == enc_diffzipped)
		PtexUtils::decodeDifference(tmp, unpackedSize, _header.datatype);
	    PtexUtils::interleave(tmp, uw * DataSize(_header.datatype), uw, vw,
				  pf->data(), uw * _pixelsize,
				  _header.datatype, _header.nchannels);
	}
	break;
    }
}


void PtexReader::getData(int faceid, void* buffer, int stride)
{
    FaceInfo& f = _faceinfo[faceid];
    int resu = f.res.u(), resv = f.res.v();
    int rowlen = _pixelsize * resu;
    if (stride == 0) stride = rowlen;
    
    PtexFaceData* d = getData(faceid, f.res);
    if (d->isConstant()) {
	// fill dest buffer with pixel value
	char* pixel = (char*) d->getData();
	char* firstrow = (char*) buffer;
	char* p = firstrow;
	// fill first row
	for (int i = 0; i < resu; i++) {
	    memcpy(p, pixel, _pixelsize);
	    p += _pixelsize;
	}
	// copy to remaining rows
	p = firstrow + stride;
	for (int i = 1; i < resv; i++) {
	    memcpy(p, firstrow, rowlen);
	    p += stride;
	}
    }
    else if (d->isTiled()) {
	// loop over tiles and copy line by line
	Res tileres = d->tileRes();
	int ntilesu = f.res.ntilesu(tileres);
	int ntilesv = f.res.ntilesv(tileres);
	int tileures = tileres.u();
	int tilevres = tileres.v();
	int tilerowlen = _pixelsize * tileures;
	int tile = 0;
	char* dsttilerow = (char*) buffer;
	for (int i = 0; i < ntilesv; i++) {
	    char* dsttile = dsttilerow;
	    for (int j = 0; j < ntilesu; j++) {
		PtexFaceData* t = d->getTile(tile++);
		const char* src = (const char*) t->getData();
		if (t->isConstant()) {
		    // fill dest tile with pixel value
		    char* p = dsttile;
		    // fill first row
		    for (int i = 0; i < tileures; i++) {
			memcpy(p, src, _pixelsize);
			p += _pixelsize;
		    }
		    // copy to remaining rows
		    p = dsttile + stride;
		    for (int i = 1; i < tilevres; i++) {
			memcpy(p, dsttile, tilerowlen);
			p += stride;
		    }
		} else {
		    // copy tile row by row
		    const char* end = src + tilerowlen * tilevres;
		    char* dst = dsttile;
		    for (; src < end; src += tilerowlen, dst += stride)
			memcpy(dst, src, tilerowlen);
		}
		t->release();
		dsttile += tilerowlen;
	    }
	    dsttilerow += stride * tilevres;
	}
    }
    else {
	// regular non-tiled case
	if (stride == rowlen) {
	    // packed case - copy in single block
	    memcpy(buffer, d->getData(), rowlen*resv);
	} else {
	    // copy a row at a time
	    char* src = (char*) d->getData();
	    char* dst = (char*) buffer;
	    for (int i = 0; i < resv; i++) {
		memcpy(dst, src, rowlen);
		src += rowlen;
		dst += stride;
	    }
	}
    }
    d->release();
}


PtexFaceData* PtexReader::getData(int faceid, Res res)
{
    FaceInfo& fi = _faceinfo[faceid];
    if (fi.isConstant() || res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize);
    }

    if (res != fi.res) {
	// TODO
	std::cerr << "Reductions not yet supported" << std::endl;
	return 0;
    }
    Level* level = getLevel(0);
    FaceData* face = getFace(level, faceid);
    level->unref();
    return face;
}


void* PtexReader::TiledFace::getPixel(int ui, int vi)
{
    int tileu = ui >> _tileres.ulog2;
    int tilev = vi >> _tileres.vlog2;
    PtexFaceData* tile = getTile(tilev * _ntilesu + tileu);
    return tile->getPixel(ui - (tileu<<_tileres.ulog2),
			  vi - (tilev<<_tileres.vlog2));
}
