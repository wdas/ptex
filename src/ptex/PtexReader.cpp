#include <iostream>
#include <sstream>
#include <errno.h>
#include <stdio.h>
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
    
    // we can free the const data directly since we own it (it doesn't go through the cache)
    if (_constdata) free(_constdata);

    // the rest must be orphaned since there may still be references outstanding
    orphanList(_levels);
    for (ReductionMap::iterator i = _reductions.begin(); i != _reductions.end(); i++)
	i->second->orphan();
    if (_metadata) _metadata->orphan();
    
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

	// generate rfaceids
	_rfaceids.resize(_header.nfaces);
	std::vector<uint32_t> faceids_r(_header.nfaces);
	PtexUtils::genRfaceids(&_faceinfo[0], _header.nfaces,
			       &_rfaceids[0], &faceids_r[0]);
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
	int size = _pixelsize * _header.nfaces;
	_constdata = (uint8_t*) malloc(size);
	readZipBlock(_constdata, _header.constdatasize, size);
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

#ifdef GATHER_STATS
	stats.nblocksRead++;
	stats.nbytesRead += size;
#endif
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
	    TiledFace* tf = new TiledFace((void**)&face, _cache, res, tileres, this);
	    readZipBlock(&tf->_fdh[0], tileheadersize, FaceDataHeaderSize * tf->_ntiles);
	    computeOffsets(tell(), tf->_ntiles, &tf->_fdh[0], &tf->_offsets[0]);
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
	PtexUtils::fill(d->getData(), buffer, stride,
			resu, resv, _pixelsize);
    }
    else if (d->isTiled()) {
	// loop over tiles
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
		if (t->isConstant())
		    PtexUtils::fill(t->getData(), dsttile, stride,
				    tileures, tilevres, _pixelsize);
		else
		    PtexUtils::copy(t->getData(), tilerowlen, dsttile, stride, 
				    tilevres, tilerowlen);
		t->release();
		dsttile += tilerowlen;
	    }
	    dsttilerow += stride * tilevres;
	}
    }
    else {
	PtexUtils::copy(d->getData(), rowlen, buffer, stride, resv, rowlen);
    }
    d->release();
}


PtexFaceData* PtexReader::getData(int faceid, Res res)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;

    FaceInfo& fi = _faceinfo[faceid];
    if (fi.isConstant() || res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize);
    }

    // get precomputed reduction level (if possible)
    // determine how many reduction levels are needed
    int redu = fi.res.ulog2 - res.ulog2, redv = fi.res.vlog2 - res.vlog2;
    if (redu < 0 || redv < 0) {
	std::cerr << "PtexReader::getData - enlargements not supported" << std::endl;
	return 0;
    }
    
    if (redu == 0 && redv == 0) {
	// no reduction - get level zero (full) res face
	Level* level = getLevel(0);
	FaceData* face = getFace(level, faceid, res);
	level->unref();
	return face;
    }
    else if (redu == redv && !fi.hasEdits()) {
	// reduction is symmetric and face has no edits, access reduction level
	int levelid = redu;
	if (size_t(levelid) < _levels.size()) {
	    Level* level = getLevel(levelid);

	    // get reduction face id
	    int rfaceid = _rfaceids[faceid];

	    // get the face data (if present)
	    FaceData* face = 0;
	    if (size_t(rfaceid) < level->faces.size())
		face = getFace(level, rfaceid, res);
	    level->unref();
	    if (face) return face;
	}
    }
    // dynamic reduction required - find in dynamic reduction cache
    FaceData*& face = _reductions[ReductionKey(faceid, res)];
    if (face) { face->ref(); return face; }
	
    // not found, must generate new reduction
    if (redu > redv) {
	// get next-higher u-res and reduce in u
	PtexFaceData* psrc = getData(faceid, Res(res.ulog2+1, res.vlog2));
	FaceData* src = dynamic_cast<FaceData*>(psrc);
	if (src) src->reduce(face, this, res, PtexUtils::reduceu);
	if (psrc) psrc->release();
    }
    else if (redu < redv) {
	// get next-higher v-res and reduce in v
	PtexFaceData* psrc = getData(faceid, Res(res.ulog2, res.vlog2+1));
	FaceData* src = dynamic_cast<FaceData*>(psrc);
	if (src) src->reduce(face, this, res, PtexUtils::reducev);
	if (psrc) psrc->release();
    }
    else { // redu == redv
	// get next-higher res and reduce (in both u and v
	PtexFaceData* psrc = getData(faceid, Res(res.ulog2+1, res.vlog2+1));
	FaceData* src = dynamic_cast<FaceData*>(psrc);
	if (src) src->reduce(face, this, res, PtexUtils::reduce);
	if (psrc) psrc->release();
    }
    return face;
}


void PtexReader::PackedFace::reduce(FaceData*& face, PtexReader* r,
				    Res newres, PtexUtils::ReduceFn reducefn)
{
    // allocate a new face and reduce image
    DataType dt = r->datatype();
    int nchan = r->nchannels();
    PackedFace* pf = new PackedFace((void**)&face, _cache, newres,
				    _pixelsize, _pixelsize * newres.size());
    face = pf;
    // reduce and copy into new face
    reducefn(_data, _pixelsize * _res.u(), _res.u(), _res.v(),
	     pf->_data, _pixelsize * newres.u(), dt, nchan);
}



void PtexReader::ConstantFace::reduce(FaceData*& face, PtexReader*,
				      Res, PtexUtils::ReduceFn)
{
    face = this; ref();
}


void PtexReader::TiledFaceBase::reduce(FaceData*& face, PtexReader* r,
				       Res newres, PtexUtils::ReduceFn reducefn)
{
    /* Tiled reductions should generally only be anisotropic (just u
       or v, not both) since isotropic reductions are precomputed and
       stored on disk.  (This function should still work for isotropic
       reductions though.)

       In the anisotropic case, the number of tiles should be kept the
       same along the direction not being reduced in order to preserve
       the laziness of the file access.  In contrast, if reductions
       were not tiled, then any reduction would read all the tiles and
       defeat the purpose of tiling.
    */

    // propagate the tile res to the reduction
    // but make sure tile isn't larger than the new face!
    Res newtileres = _tileres;
    if (newtileres.ulog2 > newres.ulog2) newtileres.ulog2 = newres.ulog2;
    if (newtileres.vlog2 > newres.vlog2) newtileres.vlog2 = newres.vlog2;

    // determine how many tiles we will have on the reduction
    int newntiles = newres.ntiles(newtileres);

    if (newntiles == 1) {
	// no need to keep tiling, reduce tiles into a single face
	// first, get all tiles and check if they are constant (with the same value)
	PtexFaceData** tiles = (PtexFaceData**) alloca(_ntiles * sizeof(PtexFaceData*));
	bool allConstant = true;
	for (int i = 0; i < _ntiles; i++) {
	    PtexFaceData* tile = tiles[i] = getTile(i);
	    allConstant = (allConstant && tile->isConstant() && 
			   (i == 0 || (0 == memcmp(tiles[0]->getData(), tile->getData(), 
						   _pixelsize))));
	}
	if (allConstant) {
	    // allocate a new constant face
	    face = new ConstantFace((void**)&face, _cache, _pixelsize);
	    memcpy(face->getData(), tiles[0]->getData(), _pixelsize);
	}
	else {
	    // allocate a new packed face
	    face = new PackedFace((void**)&face, _cache, newres,
				  _pixelsize, _pixelsize*newres.size());

	    int tileures = _tileres.u();
	    int tilevres = _tileres.v();
	    int sstride = _pixelsize * tileures;
	    int dstride = _pixelsize * newres.u();
	    int dstepu = dstride/_ntilesu;
	    int dstepv = dstride*newres.v()/_ntilesv - dstepu*(_ntilesu-1);
	    
	    char* dst = (char*) face->getData();
	    for (int i = 0; i < _ntiles;) {
		PtexFaceData* tile = tiles[i];
		if (tile->isConstant())
		    PtexUtils::fill(tile->getData(), dst, dstride,
				    newres.u()/_ntilesu, newres.v()/_ntilesv,
				    _pixelsize);
		else
		    reducefn(tile->getData(), sstride, tileures, tilevres,
			     dst, dstride, _dt, _nchan);
		i++;
		dst += i%_ntilesu ? dstepu : dstepv;
	    }
	}
	// release the tiles
	for (int i = 0; i < _ntiles; i++) tiles[i]->release();
    }
    else {
	// otherwise, tile the reduced face
	face = new TiledReducedFace((void**)&face, _cache, newres, newtileres,
				    _dt, _nchan, this, reducefn);
    }
}


void* PtexReader::TiledFaceBase::getPixel(int ui, int vi)
{
    int tileu = ui >> _tileres.ulog2;
    int tilev = vi >> _tileres.vlog2;
    PtexFaceData* tile = getTile(tilev * _ntilesu + tileu);
    return tile->getPixel(ui - (tileu<<_tileres.ulog2),
			  vi - (tilev<<_tileres.vlog2));
}



PtexFaceData* PtexReader::TiledReducedFace::getTile(int tile)
{
    FaceData*& face = _tiles[tile];
    if (face) { face->ref(); return face; }

    // first, get all tiles and check if they are constant (with the same value)
    int pntilesu = _parentface->ntilesu();
    int pntilesv = _parentface->ntilesv();
    int nu = pntilesu / _ntilesu;
    int nv = pntilesv / _ntilesv;

    int ntiles = nu*nv;
    PtexFaceData** tiles = (PtexFaceData**) alloca(ntiles * sizeof(PtexFaceData*));
    bool allConstant = true;
    int ptile = (tile/_ntilesu) * nv * pntilesu + (tile%_ntilesu) * nu;
    for (int i = 0; i < ntiles;) {
	PtexFaceData* tile = tiles[i] = _parentface->getTile(ptile);
	allConstant = (allConstant && tile->isConstant() && 
		       (i==0 || (0 == memcmp(tiles[0]->getData(), tile->getData(), 
					     _pixelsize))));
	i++;
	ptile += i%nu? 1 : pntilesu - nu + 1;
    }
    if (allConstant) {
	// allocate a new constant face
	face = new ConstantFace((void**)&face, _cache, _pixelsize);
	memcpy(face->getData(), tiles[0]->getData(), _pixelsize);
    }
    else {
	// allocate a new packed face for the tile
	face = new PackedFace((void**)&face, _cache, _tileres,
			      _pixelsize, _pixelsize*_tileres.size());

	// generate reduction from parent tiles
	int ptileures = _parentface->tileres().u();
	int ptilevres = _tileres.v();
	int sstride = ptileures * _pixelsize;
	int dstride = _tileres.u() * _pixelsize;
	int dstepu = dstride/nu;
	int dstepv = dstride*_tileres.v()/nv - dstepu*(nu-1);
    
	char* dst = (char*) face->getData();
	for (int i = 0; i < ntiles;) {
	    PtexFaceData* tile = tiles[i];
	    if (tile->isConstant())
		PtexUtils::fill(tile->getData(), dst, dstride,
				_tileres.u()/nu, _tileres.v()/nv,
				_pixelsize);
	    else
		_reducefn(tile->getData(), sstride, ptileures, ptilevres,
			  dst, dstride, _dt, _nchan);
	    i++;
	    dst += i%nu ? dstepu : dstepv;
	}
    }

    // release the tiles
    for (int i = 0; i < ntiles; i++) tiles[i]->release();
    return face;
}
