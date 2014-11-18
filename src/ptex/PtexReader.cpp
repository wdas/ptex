/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

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

#include "PtexPlatform.h"
#include <iostream>
#include <sstream>
#include <stdio.h>

#include "Ptexture.h"
#include "PtexUtils.h"
#include "PtexReader.h"


PtexTexture* PtexTexture::open(const char* path, Ptex::String& error, bool premultiply)
{
    PtexReader* reader = new PtexReader(premultiply, 0);
    bool ok = reader->open(path, error);
    if (!ok) {
        reader->release();
        return 0;
    }
    return reader;
}


PtexReader::PtexReader(bool premultiply, PtexInputHandler* io)
    : _io(io ? io : &_defaultIo),
      _premultiply(premultiply),
      _ok(true),
      _fp(0),
      _pos(0),
      _pixelsize(0),
      _constdata(0),
      _metadata(0),
      _hasEdits(false)
{
    memset(&_header, 0, sizeof(_header));
    memset(&_zstream, 0, sizeof(_zstream));
    inflateInit(&_zstream);
    _inflateInitialized = true;
}


void PtexReader::clear()
{
    if (_fp) {
        _io->close(_fp);
        _fp = 0;
    }

    if (_constdata) {
        free(_constdata);
        _constdata = 0;
    }

    for (std::vector<Level*>::iterator i = _levels.begin(); i != _levels.end(); ++i) {
        if (*i) delete *i;
    }
    _levels.clear();

    if (_metadata) {
	delete _metadata;
	_metadata = 0;
    }

    if (_inflateInitialized) {
        inflateEnd(&_zstream);
        _inflateInitialized = false;
    }

    _ok = false;
}


bool PtexReader::open(const char* path, Ptex::String& error)
{
    if (!LittleEndian()) {
	error = "Ptex library doesn't currently support big-endian cpu's";
	return 0;
    }
    _path = path;
    _fp = _io->open(path);
    if (!_fp) {
	std::string errstr = "Can't open ptex file: ";
	errstr += path; errstr += "\n"; errstr += _io->lastError();
	error = errstr.c_str();
	return 0;
    }
    readBlock(&_header, HeaderSize);
    if (_header.magic != Magic) {
	std::string errstr = "Not a ptex file: "; errstr += path;
	error = errstr.c_str();
	return 0;
    }
    if (_header.version != 1) {
        std::stringstream s;
        s << "Unsupported ptex file version ("<< _header.version << "): " << path;
        error = s.str();
        return 0;
    }
    _pixelsize = _header.pixelSize();

    // read extended header
    memset(&_extheader, 0, sizeof(_extheader));
    readBlock(&_extheader, PtexUtils::min(uint32_t(ExtHeaderSize), _header.extheadersize));

    // compute offsets of various blocks
    FilePos pos = HeaderSize + _header.extheadersize;
    _faceinfopos = pos;   pos += _header.faceinfosize;
    _constdatapos = pos;  pos += _header.constdatasize;
    _levelinfopos = pos;  pos += _header.levelinfosize;
    _leveldatapos = pos;  pos += _header.leveldatasize;
    _metadatapos = pos;   pos += _header.metadatazipsize;
                          pos += sizeof(uint64_t); // compatibility barrier
    _lmdheaderpos = pos;  pos += _extheader.lmdheaderzipsize;
    _lmddatapos = pos;    pos += _extheader.lmddatasize;

    // edit data may not start immediately if additional sections have been added
    // use value from extheader if present (and > pos)
    _editdatapos = PtexUtils::max(FilePos(_extheader.editdatapos), pos);

    // read basic file info
    readFaceInfo();
    readConstData();
    readLevelInfo();
    readEditData();

    // an error occurred while reading the file
    if (!_ok) {
	error = _error.c_str();
        clear();
	return 0;
    }

    return 1;
}


void PtexReader::close()
{
    if (_fp) {
        AutoMutex locker(readlock);
        if (_fp) {
            _io->close(_fp);
            _fp = 0;
        }
    }
}


bool PtexReader::reopen()
{
    if (_fp) return true;

    // we assume this is called lazily in a scope where readlock is already held
    _fp = _io->open(_path.c_str());
    if (!_fp) {
        setError("Can't reopen");
	return false;
    }
    _pos = 0;
    Header header;
    ExtHeader extheader;
    readBlock(&header, HeaderSize);
    memset(&extheader, 0, sizeof(extheader));
    readBlock(&extheader, PtexUtils::min(uint32_t(ExtHeaderSize), header.extheadersize));
    if (0 != memcmp(&header, &_header, sizeof(header)) ||
        0 != memcmp(&extheader, &_extheader, sizeof(extheader)))
    {
        setError("Header mismatch on reopen of");
	return false;
    }
    return true;
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
		     (int)(sizeof(FaceInfo)*nfaces));

	// generate rfaceids
	_rfaceids.resize(nfaces);
	std::vector<uint32_t> faceids_r(nfaces);
	PtexUtils::genRfaceids(&_faceinfo[0], nfaces,
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
	FilePos pos = _leveldatapos;
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
	if (_premultiply && _header.hasAlpha())
	    PtexUtils::multalpha(_constdata, _header.nfaces, _header.datatype,
				 _header.nchannels, _header.alphachan);
    }
}


PtexMetaData* PtexReader::getMetaData()
{
    if (!_metadata) readMetaData();
    return _metadata;
}


PtexReader::MetaData::Entry*
PtexReader::MetaData::getEntry(const char* key)
{
    MetaMap::iterator iter = _map.find(key);
    if (iter == _map.end()) {
	// not found
	return 0;
    }

    Entry* e = &iter->second;
    if (!e->isLmd) {
	// normal (small) meta data - just return directly
	return e;
    }

    // large meta data - may not be read in yet
    if (e->lmdData) {
	// already in memory, add a ref
	_lmdRefs.push_back(e->lmdData);
	return e;
    }
    else {
	// not present, must read from file

	// get read lock and make sure we still need to read
	AutoMutex locker(_reader->readlock);
	if (e->lmdData) {
            e->data = e->lmdData->data();
            _lmdRefs.push_back(e->lmdData);
            return e;
	}
	// go ahead and read, keep local until finished
	LargeMetaData* lmdData = new LargeMetaData(e->datasize);
	e->data = lmdData->data();
	_reader->seek(e->lmdPos);
	_reader->readZipBlock(e->data, e->lmdZipSize, e->datasize);
	// update entry
	e->lmdData = lmdData;
	return e;
    }
}


void PtexReader::readMetaData()
{
    // get read lock and make sure we still need to read
    AutoMutex locker(readlock);
    if (_metadata) {
        return;
    }

#if 0
    // TODO

    // compute total size (including edit blocks) for cache tracking
    int totalsize = _header.metadatamemsize;
    for (size_t i = 0, size = _metaedits.size(); i < size; i++)
	totalsize += _metaedits[i].memsize;
#endif

    // allocate new meta data (keep local until fully initialized)
    MetaData* newmeta = new MetaData(this);

    // read primary meta data blocks
    if (_header.metadatamemsize)
	readMetaDataBlock(newmeta, _metadatapos,
			  _header.metadatazipsize, _header.metadatamemsize);

    // read large meta data headers
    if (_extheader.lmdheadermemsize)
	readLargeMetaDataHeaders(newmeta, _lmdheaderpos,
				 _extheader.lmdheaderzipsize, _extheader.lmdheadermemsize);

    // read meta data edits
    for (size_t i = 0, size = _metaedits.size(); i < size; i++)
	readMetaDataBlock(newmeta, _metaedits[i].pos,
			  _metaedits[i].zipsize, _metaedits[i].memsize);

    // store meta data
    _metadata = newmeta;

}


void PtexReader::readMetaDataBlock(MetaData* metadata, FilePos pos, int zipsize, int memsize)
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
	    key[keysize-1] = '\0';
	    uint8_t datatype = *ptr++;
	    uint32_t datasize; memcpy(&datasize, ptr, sizeof(datasize));
	    ptr += sizeof(datasize);
	    char* data = ptr; ptr += datasize;
	    metadata->addEntry((uint8_t)(keysize-1), key, datatype, datasize, data);
	}
    }
    if (useMalloc) free(buff);
}


void PtexReader::readLargeMetaDataHeaders(MetaData* metadata, FilePos pos, int zipsize, int memsize)
{
    seek(pos);
    // read from file
    bool useMalloc = memsize > AllocaMax;
    char* buff = useMalloc ? (char*) malloc(memsize) : (char*)alloca(memsize);

    if (readZipBlock(buff, zipsize, memsize)) {
	pos += zipsize;

	// unpack data entries
	char* ptr = buff;
	char* end = ptr + memsize;
	while (ptr < end) {
	    uint8_t keysize = *ptr++;
	    char* key = (char*)ptr; ptr += keysize;
	    uint8_t datatype = *ptr++;
	    uint32_t datasize; memcpy(&datasize, ptr, sizeof(datasize));
	    ptr += sizeof(datasize);
	    uint32_t zipsize; memcpy(&zipsize, ptr, sizeof(zipsize));
	    ptr += sizeof(zipsize);
	    metadata->addLmdEntry((uint8_t)(keysize-1), key, datatype, datasize, pos, zipsize);
	    pos += zipsize;
	}
    }
    if (useMalloc) free(buff);
}

void PtexReader::readEditData()
{
    // determine file range to scan for edits
    FilePos pos = FilePos(_editdatapos), endpos;
    if (_extheader.editdatapos > 0) {
	// newer files record the edit data position and size in the extheader
	// note: position will be non-zero even if size is zero
	endpos = FilePos(pos + _extheader.editdatasize);
    }
    else {
	// must have an older file, just read until EOF
	endpos = FilePos((uint64_t)-1);
    }

    while (pos < endpos) {
	seek(pos);
	// read the edit data header
	uint8_t edittype = et_editmetadata;
	uint32_t editsize;
	if (!readBlock(&edittype, sizeof(edittype), /*reporterror*/ false)) break;
	if (!readBlock(&editsize, sizeof(editsize), /*reporterror*/ false)) break;
	if (!editsize) break;
	_hasEdits = true;
	pos = tell() + editsize;
	switch (edittype) {
	case et_editfacedata:   readEditFaceData(); break;
	case et_editmetadata:   readEditMetaData(); break;
	}
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

    // read const value now
    uint8_t* constdata = _constdata + _pixelsize * faceid;
    if (!readBlock(constdata, _pixelsize)) return;
    if (_premultiply && _header.hasAlpha())
	PtexUtils::multalpha(constdata, 1, _header.datatype,
			     _header.nchannels, _header.alphachan);

    // update header info for remaining data
    if (!f.isConstant()) {
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


bool PtexReader::readBlock(void* data, int size, bool reporterror)
{
    if (!_fp) return false;
    int result = (int)_io->read(data, size, _fp);
    if (result == size) {
	_pos += size;
	return true;
    }
    if (reporterror)
	setError("PtexReader error: read failed (EOF)");
    return false;
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

    int total = (int)_zstream.total_out;
    inflateReset(&_zstream);
    return total == unzipsize;
}


void PtexReader::readLevel(int levelid, Level*& level)
{
    // get read lock and make sure we still need to read
    AutoMutex locker(readlock);
    if (level) {
	// make sure it's still there now that we have the lock
	if (level) {
	    return;
	}
    }

    // go ahead and read the level
    LevelInfo& l = _levelinfo[levelid];

    // keep new level local until finished
    Level* newlevel = new Level(l.nfaces);
    seek(_levelpos[levelid]);
    readZipBlock(&newlevel->fdh[0], l.levelheadersize, FaceDataHeaderSize * l.nfaces);
    computeOffsets(tell(), l.nfaces, &newlevel->fdh[0], &newlevel->offsets[0]);

    // apply edits (if any) to level 0
    if (levelid == 0) {
	for (size_t i = 0, size = _faceedits.size(); i < size; i++) {
	    FaceEdit& e = _faceedits[i];
	    newlevel->fdh[e.faceid] = e.fdh;
	    newlevel->offsets[e.faceid] = e.pos;
	}
    }

    // don't assign to result until level data is fully initialized
    MemoryFence();
    level = newlevel;
}


void PtexReader::readFace(int levelid, Level* level, int faceid, Ptex::Res res)
{
    FaceData*& face = level->faces[faceid];
    FaceDataHeader fdh = level->fdh[faceid];
    readFaceData(level->offsets[faceid], fdh, res, levelid, face);
}


void PtexReader::TiledFace::readTile(int tile, FaceData*& data)
{
    _reader->readFaceData(_offsets[tile], _fdh[tile], _tileres, _levelid, data);
}


void PtexReader::readFaceData(FilePos pos, FaceDataHeader fdh, Res res, int levelid,
			      FaceData*& face)
{
    AutoMutex locker(readlock);
    if (face) {
        return;
    }

    // keep new face local until fully initialized
    FaceData* newface = 0;

    seek(pos);
    switch (fdh.encoding()) {
    case enc_constant:
	{
	    ConstantFace* pf = new ConstantFace(_pixelsize);
	    readBlock(pf->data(), _pixelsize);
	    if (levelid==0 && _premultiply && _header.hasAlpha())
		PtexUtils::multalpha(pf->data(), 1, _header.datatype,
				     _header.nchannels, _header.alphachan);
	    newface = pf;
	}
	break;
    case enc_tiled:
	{
	    Res tileres;
	    readBlock(&tileres, sizeof(tileres));
	    uint32_t tileheadersize;
	    readBlock(&tileheadersize, sizeof(tileheadersize));
	    TiledFace* tf = new TiledFace(res, tileres, levelid, this);
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
	    PackedFace* pf = new PackedFace(res, _pixelsize, unpackedSize);
            bool useMalloc = unpackedSize > AllocaMax;
            void* tmp = useMalloc ? malloc(unpackedSize) : alloca(unpackedSize);
	    readZipBlock(tmp, fdh.blocksize(), unpackedSize);
	    if (fdh.encoding() == enc_diffzipped)
		PtexUtils::decodeDifference(tmp, unpackedSize, _header.datatype);
	    PtexUtils::interleave(tmp, uw * DataSize(_header.datatype), uw, vw,
				  pf->data(), uw * _pixelsize,
				  _header.datatype, _header.nchannels);
	    if (levelid==0 && _premultiply && _header.hasAlpha())
		PtexUtils::multalpha(pf->data(), npixels, _header.datatype,
				     _header.nchannels, _header.alphachan);
	    newface = pf;
            if (useMalloc) free(tmp);
	}
	break;
    }

    MemoryFence();
    face = newface;
}


void PtexReader::getData(int faceid, void* buffer, int stride)
{
    if (!_ok) return;
    FaceInfo& f = _faceinfo[faceid];
    getData(faceid, buffer, stride, f.res);
}


void PtexReader::getData(int faceid, void* buffer, int stride, Res res)
{
    if (!_ok) return;

    // note - all locking is handled in called getData methods
    int resu = res.u(), resv = res.v();
    int rowlen = _pixelsize * resu;
    if (stride == 0) stride = rowlen;

    PtexPtr<PtexFaceData> d ( getData(faceid, res) );
    if (!d) return;
    if (d->isConstant()) {
	// fill dest buffer with pixel value
	PtexUtils::fill(d->getData(), buffer, stride,
			resu, resv, _pixelsize);
    }
    else if (d->isTiled()) {
	// loop over tiles
	Res tileres = d->tileRes();
	int ntilesu = res.ntilesu(tileres);
	int ntilesv = res.ntilesv(tileres);
	int tileures = tileres.u();
	int tilevres = tileres.v();
	int tilerowlen = _pixelsize * tileures;
	int tile = 0;
	char* dsttilerow = (char*) buffer;
	for (int i = 0; i < ntilesv; i++) {
	    char* dsttile = dsttilerow;
	    for (int j = 0; j < ntilesu; j++) {
		PtexPtr<PtexFaceData> t ( d->getTile(tile++) );
		if (!t) { i = ntilesv; break; }
		if (t->isConstant())
		    PtexUtils::fill(t->getData(), dsttile, stride,
				    tileures, tilevres, _pixelsize);
		else
		    PtexUtils::copy(t->getData(), tilerowlen, dsttile, stride,
				    tilevres, tilerowlen);
		dsttile += tilerowlen;
	    }
	    dsttilerow += stride * tilevres;
	}
    }
    else {
	PtexUtils::copy(d->getData(), rowlen, buffer, stride, resv, rowlen);
    }
}


PtexFaceData* PtexReader::getData(int faceid)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;
    if (!_ok) return 0;

    FaceInfo& fi = _faceinfo[faceid];
    if (fi.isConstant() || fi.res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize, _pixelsize);
    }

    // get level zero (full) res face
    Level* level = getLevel(0);
    FaceData* face = getFace(0, level, faceid, fi.res);
    return face;
}


PtexFaceData* PtexReader::getData(int faceid, Res res)
{
    if (!_ok) return 0;
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;

    FaceInfo& fi = _faceinfo[faceid];
    if ((fi.isConstant() && res >= 0) || res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize, _pixelsize);
    }

    // determine how many reduction levels are needed
    int redu = fi.res.ulog2 - res.ulog2, redv = fi.res.vlog2 - res.vlog2;

    if (redu == 0 && redv == 0) {
	// no reduction - get level zero (full) res face
	Level* level = getLevel(0);
	FaceData* face = getFace(0, level, faceid, res);
	return face;
    }

    if (redu == redv && !fi.hasEdits() && res >= 0) {
	// reduction is symmetric and non-negative
	// and face has no edits => access data from reduction level (if present)
	int levelid = redu;
	if (size_t(levelid) < _levels.size()) {
	    Level* level = getLevel(levelid);

	    // get reduction face id
	    int rfaceid = _rfaceids[faceid];

	    // get the face data (if present)
	    FaceData* face = 0;
	    if (size_t(rfaceid) < level->faces.size())
		face = getFace(levelid, level, rfaceid, res);
	    if (face) {
		return face;
	    }
	}
    }

    // dynamic reduction required - look in dynamic reduction cache
    ReductionKey key(faceid, res);
    FaceData* face = _reductions.get(key);
    if (face) {
	return face;
    }

    // not found,  generate new reduction
    FaceData *newface = 0;

    if (res.ulog2 < 0 || res.vlog2 < 0) {
	std::cerr << "PtexReader::getData - reductions below 1 pixel not supported" << std::endl;
	return 0;
    }
    if (redu < 0 || redv < 0) {
	std::cerr << "PtexReader::getData - enlargements not supported" << std::endl;
	return 0;
    }
    if (_header.meshtype == mt_triangle)
    {
	if (redu != redv) {
	    std::cerr << "PtexReader::getData - anisotropic reductions not supported for triangle mesh" << std::endl;
	    return 0;
	}
	PtexPtr<PtexFaceData> psrc ( getData(faceid, Res((int8_t)(res.ulog2+1), (int8_t)(res.vlog2+1))) );
	FaceData* src = static_cast<FaceData*>(psrc.get());
	assert(src);
        newface = src->reduce(this, res, PtexUtils::reduceTri);
    }
    else {
        // determine which direction to blend
        bool blendu;
        if (redu == redv) {
            // for symmetric face blends, alternate u and v blending
            blendu = (res.ulog2 & 1);
        }
        else blendu = redu > redv;

        if (blendu) {
            // get next-higher u-res and reduce in u
            PtexPtr<PtexFaceData> psrc ( getData(faceid, Res((int8_t)(res.ulog2+1), (int8_t)res.vlog2)) );
            FaceData* src = static_cast<FaceData*>(psrc.get());
            assert(src);
            newface = src->reduce(this, res, PtexUtils::reduceu);
        }
        else {
            // get next-higher v-res and reduce in v
            PtexPtr<PtexFaceData> psrc ( getData(faceid, Res((int8_t)res.ulog2, (int8_t)(res.vlog2+1))) );
            FaceData* src = static_cast<FaceData*>(psrc.get());
            assert(src);
            newface = src->reduce(this, res, PtexUtils::reducev);
        }
    }

    face = _reductions.tryInsert(key, newface);
    if (face != newface) delete newface;
    return face;
}


void PtexReader::getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels)
{
    memset(result, 0, sizeof(*result)*nchannels);

    // clip nchannels against actual number available
    nchannels = PtexUtils::min(nchannels,
			       _header.nchannels-firstchan);
    if (nchannels <= 0) return;

    // get raw pixel data
    PtexPtr<PtexFaceData> data ( getData(faceid) );
    if (!data) return;
    void* pixel = alloca(_pixelsize);
    data->getPixel(u, v, pixel);

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


void PtexReader::getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels,
			  Ptex::Res res)
{
    memset(result, 0, nchannels);

    // clip nchannels against actual number available
    nchannels = PtexUtils::min(nchannels,
			       _header.nchannels-firstchan);
    if (nchannels <= 0) return;

    // get raw pixel data
    PtexPtr<PtexFaceData> data ( getData(faceid, res) );
    if (!data) return;
    void* pixel = alloca(_pixelsize);
    data->getPixel(u, v, pixel);

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


PtexReader::FaceData* PtexReader::PackedFace::reduce(PtexReader* r,
				    Res newres, PtexUtils::ReduceFn reducefn)
{
    // allocate a new face and reduce image
    DataType dt = r->datatype();
    int nchan = r->nchannels();
    PackedFace* pf = new PackedFace(newres, _pixelsize, _pixelsize * newres.size());
    // reduce and copy into new face
    reducefn(_data, _pixelsize * _res.u(), _res.u(), _res.v(),
	     pf->_data, _pixelsize * newres.u(), dt, nchan);
    return pf;
}



PtexReader::FaceData* PtexReader::ConstantFace::reduce(PtexReader*,
				      Res, PtexUtils::ReduceFn)
{
    // must make a new constant face (even though it's identical to this one)
    // because it will be owned by a different reduction level
    // and will therefore have a different parent
    ConstantFace* pf = new ConstantFace(_pixelsize);
    memcpy(pf->_data, _data, _pixelsize);
    return pf;
}


PtexReader::FaceData* PtexReader::TiledFaceBase::reduce(PtexReader* r,
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

    // keep new face local until fully initialized
    FaceData* newface = 0;

    // don't tile triangle reductions (too complicated)
    Res newtileres;
    bool isTriangle = r->_header.meshtype == mt_triangle;
    if (isTriangle) {
	newtileres = newres;
    }
    else {
	// propagate the tile res to the reduction
	newtileres = _tileres;
	// but make sure tile isn't larger than the new face!
	if (newtileres.ulog2 > newres.ulog2) newtileres.ulog2 = newres.ulog2;
	if (newtileres.vlog2 > newres.vlog2) newtileres.vlog2 = newres.vlog2;
    }


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
	    newface = new ConstantFace(_pixelsize);
	    memcpy(newface->getData(), tiles[0]->getData(), _pixelsize);
	}
        else if (isTriangle) {
            // reassemble all tiles into temporary contiguous image
            // (triangle reduction doesn't work on tiles)
            int tileures = _tileres.u();
            int tilevres = _tileres.v();
            int sstride = _pixelsize * tileures;
            int dstride = sstride * _ntilesu;
            int dstepv = dstride * tilevres - sstride*(_ntilesu-1);

            char* tmp = (char*) malloc(_ntiles * _tileres.size() * _pixelsize);
            char* tmpptr = tmp;
            for (int i = 0; i < _ntiles;) {
                PtexFaceData* tile = tiles[i];
                if (tile->isConstant())
                    PtexUtils::fill(tile->getData(), tmpptr, dstride,
                                    tileures, tilevres, _pixelsize);
                else
                    PtexUtils::copy(tile->getData(), sstride, tmpptr, dstride, tilevres, sstride);
                i++;
                tmpptr += i%_ntilesu ? sstride : dstepv;
            }

            // allocate a new packed face
            newface = new PackedFace(newres, _pixelsize, _pixelsize * newres.size());
            // reduce and copy into new face
            reducefn(tmp, _pixelsize * _res.u(), _res.u(), _res.v(),
                     newface->getData(), _pixelsize * newres.u(), _dt, _nchan);

            free(tmp);
        }
	else {
	    // allocate a new packed face
	    newface = new PackedFace(newres, _pixelsize, _pixelsize*newres.size());

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
	newface = new TiledReducedFace(newres, newtileres, _dt, _nchan, this, reducefn);
    }
    return newface;
}


void PtexReader::TiledFaceBase::getPixel(int ui, int vi, void* result)
{
    int tileu = ui >> _tileres.ulog2;
    int tilev = vi >> _tileres.vlog2;
    PtexPtr<PtexFaceData> tile ( getTile(tilev * _ntilesu + tileu) );
    tile->getPixel(ui - (tileu<<_tileres.ulog2),
		   vi - (tilev<<_tileres.vlog2), result);
}



PtexFaceData* PtexReader::TiledReducedFace::getTile(int tile)
{
    FaceData*& face = _tiles[tile];
    if (face) {
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
	PtexFaceData* tile = tiles[i] = _parentface->getTile(ptile);
	allConstant = (allConstant && tile->isConstant() &&
		       (i==0 || (0 == memcmp(tiles[0]->getData(), tile->getData(),
					     _pixelsize))));
	i++;
	ptile += i%nu? 1 : pntilesu - nu + 1;
    }

    FaceData* newface = 0;
    if (allConstant) {
	// allocate a new constant face
	newface = new ConstantFace(_pixelsize);
	memcpy(face->getData(), tiles[0]->getData(), _pixelsize);
    }
    else {
	// allocate a new packed face for the tile
	newface = new PackedFace(_tileres, _pixelsize, _pixelsize*_tileres.size());

	// generate reduction from parent tiles
	int ptileures = _parentface->tileres().u();
	int ptilevres = _parentface->tileres().v();
	int sstride = ptileures * _pixelsize;
	int dstride = _tileres.u() * _pixelsize;
	int dstepu = dstride/nu;
	int dstepv = dstride*_tileres.v()/nv - dstepu*(nu-1);

	char* dst = (char*) newface->getData();
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

    if (!AtomicCompareAndSwapPtr(&face, (FaceData*)0, newface)) {
        delete newface;
    }

    return face;
}
