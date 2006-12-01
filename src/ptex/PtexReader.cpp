#include <iostream>
#include <sstream>
#include <errno.h>
#include <stdio.h>
#include "Ptexture.h"
#include "PtexUtils.h"
#include "PtexReader.h"



PtexTexture* PtexTexture::open(const char* path, std::string& error)
{
    // create a private cache and use it to open the file
    PtexCache* cache = PtexCache::create(1, 1024*1024);
    PtexTexture* file = cache->get(path, error);

    // make reader own the cache (so it will delete it later)
    PtexReader* reader = dynamic_cast<PtexReader*> (file);
    if (reader) reader->setOwnsCache();

    // and purge cache so cache doesn't try to hold reader open
    cache->purgeAll();
    return file;
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
      _ownsCache(false),
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
    for (ReductionMap::iterator i = _reductions.begin(); i != _reductions.end(); i++) {
	FaceData* f = i->second;
	if (f) f->orphan();
    }
    if (_metadata) {
	_metadata->orphan();
	_metadata = 0;
    }
    
    inflateEnd(&_zstream);

    if (_ownsCache) _cache->setPendingDelete();
}


void PtexReader::release()
{
    PtexCacheImpl* cache = _cache;
    {
	// create local scope for cache lock
	AutoLock lock(cache->cachelock);
	unref();
    }
    // If this reader owns the cache, then releasing it may cause deletion of the
    // reader and thus flag the cache for pending deletion.  Call the cache
    // to handle the pending deletion.
    cache->handlePendingDelete();
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
	int nfaces = _header.nfaces;
	_faceinfo.resize(nfaces);
	readZipBlock(&_faceinfo[0], _header.faceinfosize, 
		     sizeof(FaceInfo)*nfaces);

	// generate rfaceids
	_rfaceids.resize(nfaces);
	std::vector<uint32_t> faceids_r(nfaces);
	PtexUtils::genRfaceids(&_faceinfo[0], nfaces,
			       &_rfaceids[0], &faceids_r[0]);

	// store face res values indexed by rfaceid
	_res_r.resize(nfaces);
	for (int i = 0; i < nfaces; i++)
	    _res_r[i] = _faceinfo[faceids_r[i]].res;
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
    AutoLock locker(_cache->cachelock);
    if (_metadata) _metadata->ref();
    else readMetaData();
    return _metadata;
}


void PtexReader::readMetaData()
{
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    AutoLock locker(readlock);
    if (_metadata) {
	_cache->cachelock.lock();
	// another thread must have read it while we were waiting
	_metadata->ref();
	return;
    }


    // compute total size (including edit blocks)
    int totalsize = _header.metadatamemsize;
    for (int i = 0, size = _metaedits.size(); i < size; i++)
	totalsize += _metaedits[i].memsize;

    // allocate new meta data (keep local until fully initialized)
    MetaData* volatile newmeta = new MetaData((void**)&_metadata, _cache, totalsize);
    if (totalsize != 0) {
	if (_header.metadatamemsize)
	    readMetaDataBlock(newmeta, _metadatapos,
			      _header.metadatazipsize, _header.metadatamemsize);
	for (int i = 0, size = _metaedits.size(); i < size; i++)
	    readMetaDataBlock(newmeta, _metaedits[i].pos,
			      _metaedits[i].zipsize, _metaedits[i].memsize);
    }

    // store meta data
    _cache->cachelock.lock();
    _metadata = newmeta;
}


void PtexReader::readMetaDataBlock(MetaData* metadata, off_t pos, int zipsize, int memsize)
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
	    metadata->addEntry(keysize-1, key, datatype, datasize, data);
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
	STATS_INC(nblocksRead);
	STATS_ADD(nbytesRead, size);
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
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    AutoLock locker(readlock);
    if (level) {
	// another thread must have read it while we were waiting
	_cache->cachelock.lock();
	level->ref();
	return;
    }

    // go ahead and read the level
    LevelInfo& l = _levelinfo[levelid];

    // keep new level local until finished
    Level* volatile newlevel = new Level((void**)&level, _cache, l.nfaces);
    seek(_levelpos[levelid]);
    readZipBlock(&newlevel->fdh[0], l.levelheadersize, FaceDataHeaderSize * l.nfaces);
    computeOffsets(tell(), l.nfaces, &newlevel->fdh[0], &newlevel->offsets[0]);

    // apply edits (if any) to level 0
    if (levelid == 0) {
	for (int i = 0, size = _faceedits.size(); i < size; i++) {
	    FaceEdit& e = _faceedits[i];
	    newlevel->fdh[e.faceid] = e.fdh;
	    newlevel->offsets[e.faceid] = e.pos;
	}
    }

    // don't assign to result until level data is fully initialized
    _cache->cachelock.lock();
    level = newlevel;
}


void PtexReader::readFace(int levelid, Level* level, int faceid)
{
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    FaceData*& face = level->faces[faceid];
    AutoLock locker(readlock);

    if (face) {
	// another thread must have read it while we were waiting
	_cache->cachelock.lock();
	face->ref();
	return; 
    }

    // Go ahead and read the face, and read nearby faces if
    // possible. The goal is to coalesce small faces into single
    // runs of consecutive reads to minimize seeking and take
    // advantage of read-ahead buffering.

    // Try to read as many faces as will fit in BlockSize.  Use the
    // in-memory size rather than the on-disk size to prevent flooding
    // the memory cache.  And don't coalesce w/ tiled faces as these
    // are meant to be read individually.

    // scan both backwards and forwards looking for unread faces
    int first = faceid, last = faceid;
    int totalsize = 0;

    FaceDataHeader fdh = level->fdh[faceid];
    if (fdh.encoding != enc_tiled) {
	totalsize += unpackedSize(fdh, levelid, faceid);

	int nfaces = level->fdh.size();
	while (1) {
	    int f = first-1;
	    if (f < 0 || level->faces[f]) break;
	    fdh = level->fdh[f];
	    if (fdh.encoding == enc_tiled) break;
	    int size = totalsize + unpackedSize(fdh, levelid, f);
	    if (size > BlockSize) break;
	    first = f;
	    totalsize = size;
	}
	while (1) {
	    int f = last+1;
	    if (f >= nfaces || level->faces[f]) break;
	    fdh = level->fdh[f];
	    if (fdh.encoding == enc_tiled) break;
	    int size = totalsize + unpackedSize(fdh, levelid, f);
	    if (size > BlockSize) break;
	    last = f;
	    totalsize = size;
	}
    }

    // read all faces in range
    // keep track of extra faces we read so we can add them to the cache later
    std::vector<FaceData*> extraFaces; 
    extraFaces.reserve(last-first);

    for (int i = first; i <= last; i++) {
	fdh = level->fdh[i];
	// skip faces with zero size (which is true for level-0 constant faces)
	if (fdh.blocksize) {
	    FaceData*& face = level->faces[i];
	    readFaceData(level->offsets[i], fdh, getRes(levelid, i), face);
	    if (i != faceid) extraFaces.push_back(face);
	}
    }

    // reacquire cache lock, then unref extra faces to add them to the cache
    _cache->cachelock.lock();
    for (int i = 0, size = extraFaces.size(); i < size; i++)
	extraFaces[i]->unref();
}


void PtexReader::TiledFace::readTile(int tile, FaceData*& data)
{
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    AutoLock locker(_reader->readlock);
    if (data) {
	// another thread must have read it while we were waiting
	_cache->cachelock.lock();
	data->ref();
	return; 
    }
    
    // TODO - bundle tile reads (see readFace)

    // go ahead and read the face data
    _reader->readFaceData(_offsets[tile], _fdh[tile], _tileres, data);
    _cache->cachelock.lock();
}


void PtexReader::readFaceData(off_t pos, FaceDataHeader fdh, Res res,
			      FaceData*& face)
{
    // keep new face local until fully initialized
    FaceData* volatile newface = 0;

    seek(pos);
    switch (fdh.encoding) {
    case enc_constant: 
	{
	    ConstantFace* pf = new ConstantFace((void**)&face, _cache, _pixelsize);
	    readBlock(pf->data(), _pixelsize);
	    newface = pf;
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
	    newface = tf;
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
	    if (fdh.encoding == enc_diffzipped)
		PtexUtils::decodeDifference(tmp, unpackedSize, _header.datatype);
	    PtexUtils::interleave(tmp, uw * DataSize(_header.datatype), uw, vw,
				  pf->data(), uw * _pixelsize,
				  _header.datatype, _header.nchannels);
	    newface = pf;
	}
	break;
    }

    face = newface;
}


void PtexReader::getData(int faceid, void* buffer, int stride)
{
    // note - all locking is handled in called getData methods
    FaceInfo& f = _faceinfo[faceid];
    int resu = f.res.u(), resv = f.res.v();
    int rowlen = _pixelsize * resu;
    if (stride == 0) stride = rowlen;
    
    PtexFaceData* d = getData(faceid);
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


PtexFaceData* PtexReader::getData(int faceid)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;

   FaceInfo& fi = _faceinfo[faceid];
    if (fi.isConstant() || fi.res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize, _pixelsize);
    }

    // get level zero (full) res face
    AutoLock locker(_cache->cachelock);
    Level* level = getLevel(0);
    FaceData* face = getFace(0, level, faceid);
    level->unref();
    return face;
}


PtexFaceData* PtexReader::getData(int faceid, Res res)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;

    FaceInfo& fi = _faceinfo[faceid];
    if (fi.isConstant() || res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize, _pixelsize);
    }

    // lock cache (can't autolock since we might need to unlock early)
    _cache->cachelock.lock();

    // determine how many reduction levels are needed
    int redu = fi.res.ulog2 - res.ulog2, redv = fi.res.vlog2 - res.vlog2;
    if (redu < 0 || redv < 0) {
	std::cerr << "PtexReader::getData - enlargements not supported" << std::endl;
	_cache->cachelock.unlock();
	return 0;
    }
    
    if (redu == 0 && redv == 0) {
	// no reduction - get level zero (full) res face
	Level* level = getLevel(0);
	FaceData* face = getFace(0, level, faceid);
	level->unref();
	_cache->cachelock.unlock();
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
		face = getFace(levelid, level, rfaceid);
	    level->unref();
	    if (face) {
		_cache->cachelock.unlock();
		return face;
	    }
	}
    }

    // dynamic reduction required - look in dynamic reduction cache
    FaceData*& face = _reductions[ReductionKey(faceid, res)];
    if (face) { 
	face->ref();
	_cache->cachelock.unlock();
	return face; 
    }

    // not found,  generate new reduction
    // unlock cache - getData and reduce will handle their own locking
    _cache->cachelock.unlock();
    
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


void PtexReader::getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels)
{
    memset(result, 0, nchannels);

    // clip nchannels against actual number available
    nchannels = PtexUtils::min(nchannels, 
			       _header.nchannels-firstchan);
    if (nchannels <= 0) return;

    // get raw pixel data
    PtexFaceData* data = getData(faceid);
    if (!data) return;
    void* pixel = alloca(_pixelsize);
    data->getPixel(u, v, pixel);
    data->release();

    // adjust for firstchan offset
    int datasize = DataSize(_header.datatype);
    if (firstchan)
	pixel = (char*) pixel + datasize * firstchan;
    
    // convert/copy to result as needed
    if (_header.datatype == dt_float)
	memcpy(result, pixel, datasize * nchannels);
    else
	ConvertToFloat(result, pixel, _header.datatype, nchannels);
}


void PtexReader::PackedFace::reduce(FaceData*& face, PtexReader* r,
				    Res newres, PtexUtils::ReduceFn reducefn)
{
    // get reduce lock and make sure we still need to reduce
    AutoLock rlocker(r->reducelock);
    if (face) {
	// another thread must have generated it while we were waiting
	AutoLock locker(_cache->cachelock);
	face->ref();
	return; 
    }

    // allocate a new face and reduce image
    DataType dt = r->datatype();
    int nchan = r->nchannels();
    PackedFace* pf = new PackedFace((void**)&face, _cache, newres,
				    _pixelsize, _pixelsize * newres.size());
    // reduce and copy into new face
    reducefn(_data, _pixelsize * _res.u(), _res.u(), _res.v(),
	     pf->_data, _pixelsize * newres.u(), dt, nchan);
    AutoLock clocker(_cache->cachelock);
    face = pf;
}



void PtexReader::ConstantFace::reduce(FaceData*& face, PtexReader*,
				      Res, PtexUtils::ReduceFn)
{
    // get cache lock (just to protect the ref count)
    AutoLock locker(_cache->cachelock);

    ref();
    face = this;
}


void PtexReader::TiledFaceBase::reduce(FaceData*& face, PtexReader* r,
				       Res newres, PtexUtils::ReduceFn reducefn)
{
    // get reduce lock and make sure we still need to reduce
    AutoLock rlocker(r->reducelock);
    if (face) {
	// another thread must have generated it while we were waiting
	AutoLock locker(_cache->cachelock);
	face->ref();
	return; 
    }

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

    // keep new face local until fully initialized
    FaceData* volatile newface = 0;

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
	    newface = new ConstantFace((void**)&face, _cache, _pixelsize);
	    memcpy(newface->getData(), tiles[0]->getData(), _pixelsize);
	}
	else {
	    // allocate a new packed face
	    newface = new PackedFace((void**)&face, _cache, newres,
				     _pixelsize, _pixelsize*newres.size());

	    int tileures = _tileres.u();
	    int tilevres = _tileres.v();
	    int sstride = _pixelsize * tileures;
	    int dstride = _pixelsize * newres.u();
	    int dstepu = dstride/_ntilesu;
	    int dstepv = dstride*newres.v()/_ntilesv - dstepu*(_ntilesu-1);
	    
	    char* dst = (char*) newface->getData();
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
	newface = new TiledReducedFace((void**)&face, _cache, newres, newtileres,
				       _dt, _nchan, this, reducefn);
    }
    AutoLock clocker(_cache->cachelock);
    face = newface;
}


void PtexReader::TiledFaceBase::getPixel(int ui, int vi, void* result)
{
    int tileu = ui >> _tileres.ulog2;
    int tilev = vi >> _tileres.vlog2;
    PtexFaceData* tile = getTile(tilev * _ntilesu + tileu);
    tile->getPixel(ui - (tileu<<_tileres.ulog2),
		   vi - (tilev<<_tileres.vlog2), result);
    tile->release();
}



PtexFaceData* PtexReader::TiledReducedFace::getTile(int tile)
{
    _cache->cachelock.lock();
    FaceData*& face = _tiles[tile];
    if (face) {
	face->ref();
	_cache->cachelock.unlock();
	return face;
    }

    // first, get all parent tiles for this tile
    // and check if they are constant (with the same value)
    int pntilesu = _parentface->ntilesu();
    int pntilesv = _parentface->ntilesv();
    int nu = pntilesu / _ntilesu; // num parent tiles for this tile in u dir
    int nv = pntilesv / _ntilesv; // num parent tiles for this tile in v dir

    int ntiles = nu*nv; // num parent tiles for this tile
    PtexFaceData** tiles = (PtexFaceData**) alloca(ntiles * sizeof(PtexFaceData*));
    bool allConstant = true;
    int ptile = (tile/_ntilesu) * nv * pntilesu + (tile%_ntilesu) * nu;
    for (int i = 0; i < ntiles;) {
	// temporarily release cache lock while we get parent tile
	_cache->cachelock.unlock();
	PtexFaceData* tile = tiles[i] = _parentface->getTile(ptile);
	_cache->cachelock.lock();
	allConstant = (allConstant && tile->isConstant() && 
		       (i==0 || (0 == memcmp(tiles[0]->getData(), tile->getData(), 
					     _pixelsize))));
	i++;
	ptile += i%nu? 1 : pntilesu - nu + 1;
    }
    
    // make sure another thread didn't make the tile while we were checking
    if (face) {
	face->ref();
	_cache->cachelock.unlock();
	return face;
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
	int ptilevres = _parentface->tileres().v();
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

    _cache->cachelock.unlock();

    // release the tiles
    for (int i = 0; i < ntiles; i++) tiles[i]->release();
    return face;
}
