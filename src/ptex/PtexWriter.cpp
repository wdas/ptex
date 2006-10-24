#include <iostream>
#include <sstream>
#include <errno.h>
#include <alloca.h>
#include <algorithm>
#include "Ptexture.h"
#include "PtexUtils.h"
#include "PtexWriter.h"


PtexWriter* PtexWriter::open(const char* path, 
			     Ptex::MeshType mt, Ptex::DataType dt,
			     int nchannels, int alphachan, int nfaces,
			     std::string& error)
{
    if (!PtexIO::LittleEndian()) {
	error = "PtexWriter doesn't currently support big-endian cpu's";
	return 0;
    }
    PtexMainWriter* w = new PtexMainWriter;
    if (!w->open(path, mt, dt, nchannels, alphachan, nfaces, error)) {
	w->release();
	return 0;
    }
    return w;
}


PtexWriter* PtexWriter::edit(const char* path, bool incremental,
			     Ptex::MeshType mt, Ptex::DataType dt,
			     int nchannels, int alphachan, int nfaces, 
			     std::string& error)
{
    if (!PtexIO::LittleEndian()) {
	error = "PtexWriter doesn't currently support big-endian cpu's";
	return 0;
    }

    // open existing file
    PtexTexture* r = PtexTexture::open(path, error);
    if (!r) return 0;

    // make sure header matches
    bool headerMatch = (mt == r->meshType() && dt == r->dataType() &&
			nchannels == r->numChannels() &&
			alphachan == r->alphaChannel() &&
			nfaces == r->numFaces());
    if (!headerMatch) {
	std::stringstream str;
	str << "PtexWriter::edit error: header doesn't match existing file: " << path;
	str << "  Conversions not currently supported." << path;
	error = str.str();
	r->release();
	return 0;
      }

    if (incremental) {
	// incremental writer: close reader and append to existing file
	r->release();
	PtexIncrWriter* w = new PtexIncrWriter;
	if (!w->open(path, mt, dt, nchannels, alphachan, nfaces, error)) {
	    w->release();
	    return 0;
	}
	return w;
    }

    // non-incremental
    // make new regular writer and copy existing file as starting point
    PtexMainWriter* w = new PtexMainWriter;
    if (!w->open(path, mt, dt, nchannels, alphachan, nfaces, error)) {
	w->release();
	r->release();
	return 0;
    }

    // copy meta data
    PtexMetaData* meta = r->getMetaData();
    w->writeMeta(meta);
    meta->release();

    // copy face data
    int pixelsize = nchannels * Ptex::DataSize(dt);
    for (int i = 0; i < nfaces; i++) {
	const Ptex::FaceInfo& info = r->getFaceInfo(i);
	int size = pixelsize * info.res.size();
	if (info.isConstant()) {
	    PtexFaceData* data = r->getData(i);
	    w->writeConstantFace(i, info, data->getData());
	    data->release();
	} else {
	    void* data = malloc(size);
	    r->getData(i, data, 0);
	    w->writeFace(i, info, data, 0);
	    free(data);
	}
    }

    r->release();
    return w;
}


PtexWriterBase::PtexWriterBase()
    : _ok(true),
      _fp(0),
      _pixelSize(0)
{
    memset(&_header, 0, sizeof(_header));
    memset(&_zstream, 0, sizeof(_zstream));
    deflateInit(&_zstream, Z_DEFAULT_COMPRESSION);
}


PtexWriterBase::~PtexWriterBase()
{
    if (_ok) {
	std::string error;
	if (_fp && !close(error)) {
	    std::cerr << error << std::endl;
	}
    }
    if (_fp) fclose(_fp);
    deflateEnd(&_zstream);
}


bool PtexWriterBase::open(const char* path, const char* mode, MeshType mt, DataType dt, 
			  int nchannels, int alphachan, 
			  int nfaces, std::string& error)
{
    if (mt < mt_triangle || mt > mt_quad) {
	error = "PtexWriter error: Invalid mesh type";
	return 0;
    }

    if (dt < dt_uint8 || dt > dt_float) {
	error = "PtexWriter error: Invalid data type";
	return 0;
    }

    if (nchannels <= 0) {
	error = "PtexWriter error: Invalid number of channels";
	return 0;
    }

    if (alphachan != -1 && (alphachan < 0 || alphachan >= nchannels)) {
	error = "PtexWriter error: Invalid alpha channel";
	return 0;
    }

    _fp = fopen(path, mode);
    if (!_fp) {
	std::stringstream str;
	str << "Can't open ptex file: " << path << "\n" << strerror(errno);
	error = str.str();
	return 0;
    }

    // record header attributes
    _ok = true;
    _path = path;
    _header.magic = Magic;
    _header.version = 1;
    _header.meshtype = mt;
    _header.datatype = dt;
    _header.alphachan = alphachan;
    _header.nchannels = nchannels;
    _header.nfaces = nfaces;
    _header.nlevels = 0;

    // init state
    _pixelSize = _header.pixelSize();
    
    return 1;
}


bool PtexWriterBase::close(std::string& error)
{
    finish();
    if (!_ok) {
	error = _error;
	error += "\nPtex file: ";
	error += _path;
    }
    if (_fp) {
	fclose(_fp);
	_fp = 0;
    }
    return _ok;
}


void PtexWriterBase::writeMeta(const char* key, const char* value)
{
    addMetaData(key, mdt_string, value, strlen(value)+1);
}


void PtexWriterBase::writeMeta(const char* key, const int8_t* value, int count)
{
    addMetaData(key, mdt_int8, value, count);
}


void PtexWriterBase::writeMeta(const char* key, const int16_t* value, int count)
{
    addMetaData(key, mdt_int16, value, count*sizeof(int16_t));
}


void PtexWriterBase::writeMeta(const char* key, const int32_t* value, int count)
{
    addMetaData(key, mdt_int32, value, count*sizeof(int32_t));
}


void PtexWriterBase::writeMeta(const char* key, const float* value, int count)
{
    addMetaData(key, mdt_float, value, count*sizeof(float));
}


void PtexWriterBase::writeMeta(const char* key, const double* value, int count)
{
    addMetaData(key, mdt_double, value, count*sizeof(double));
}


void PtexWriterBase::writeMeta(PtexMetaData* data)
{
    int nkeys = data->numKeys();
    for (int i = 0; i < nkeys; i++) {
	const char* key = 0;
	MetaDataType type;
	data->getKey(i, key, type);
	int count;
	switch (type) {
	case mdt_string:
	    {
		const char* val=0;
		data->getValue(key, val);
		writeMeta(key, val);
	    }
	    break;
	case mdt_int8:
	    {
		const int8_t* val=0;
		data->getValue(key, val, count);
		writeMeta(key, val, count);
	    }
	    break;
	case mdt_int16:
	    {
		const int16_t* val=0;
		data->getValue(key, val, count);
		writeMeta(key, val, count);
	    }
	    break;
	case mdt_int32:
	    {
		const int32_t* val=0;
		data->getValue(key, val, count);
		writeMeta(key, val, count);
	    } 
	    break;
	case mdt_float:
	    {
		const float* val=0;
		data->getValue(key, val, count);
		writeMeta(key, val, count);
	    }
	    break;
	case mdt_double:
	    {
		const double* val=0;
		data->getValue(key, val, count);
		writeMeta(key, val, count);
	    }
	    break;
	}
    }
}


void PtexWriterBase::addMetaData(const char* key, MetaDataType t, 
				 const void* value, int size)
{
    MetaEntry& m = _metadata[key];
    m.datatype = t;
    m.data.resize(size);
    memcpy(&m.data[0], value, size);
}


int PtexWriterBase::writeBlank(FILE* fp, int size)
{
    if (!_ok) return 0;
    static char zeros[BlockSize];
    int remain = size;
    while (remain > 0) {
	remain -= writeBlock(fp, zeros, remain < BlockSize ? remain : BlockSize);
    }
    return size;
}


int PtexWriterBase::writeBlock(FILE* fp, const void* data, int size)
{
    if (!_ok) return 0;
    if (!fwrite(data, size, 1, fp)) {
	setError("PtexWriter error: file write failed");
	return 0;
    }
    return size;
}


int PtexWriterBase::writeZipBlock(FILE* fp, const void* data, int size, bool finish)
{
    if (!_ok) return 0;
    void* buff = alloca(BlockSize);
    _zstream.next_in = (Bytef*)data;
    _zstream.avail_in = size;
    
    while (1) {
	_zstream.next_out = (Bytef*)buff;
	_zstream.avail_out = BlockSize;
	int zresult = deflate(&_zstream, finish ? Z_FINISH : Z_NO_FLUSH);
	int size = BlockSize - _zstream.avail_out;
	if (size > 0) writeBlock(fp, buff, size);
	if (zresult == Z_STREAM_END) break;
	if (zresult != Z_OK) {
	    setError("PtexWriter error: data compression internal error");
	    break;
	}
	if (!finish && _zstream.avail_out != 0) 
	    // waiting for more input
	    break;
    }
    
    if (!finish) return 0;

    int total = _zstream.total_out;
    deflateReset(&_zstream);
    return total;
}


int PtexWriterBase::readBlock(FILE* fp, void* data, int size)
{
    if (!fread(data, size, 1, fp)) {
	setError("PtexWriter error: temp file read failed");
	return 0;
    }
    return size;
}


int PtexWriterBase::copyBlock(FILE* dst, FILE* src, off_t pos, int size)
{
    fseeko(src, pos, SEEK_SET);
    int remain = size;
    void* buff = alloca(BlockSize);
    while (remain) {
	int nbytes = remain < BlockSize ? remain : BlockSize;
	if (!fread(buff, nbytes, 1, src)) {
	    setError("PtexWriter error: temp file read failed");
	    return 0;
	}
	if (!writeBlock(dst, buff, nbytes)) break;
	remain -= nbytes;
    }
    return size;
}


Ptex::Res PtexWriterBase::calcTileRes(Res faceres)
{
    // desired number of tiles = floor(log2(facesize / tilesize))
    int facesize = faceres.size() * _pixelSize;
    int ntileslog2 = PtexUtils::floor_log2(facesize/TileSize);
    if (ntileslog2 == 0) return faceres;

    // number of tiles is defined as:
    //   ntileslog2 = ureslog2 + vreslog2 - (tile_ureslog2 + tile_vreslog2)
    // rearranging to solve for the tile res:
    //   tile_ureslog2 + tile_vreslog2 = ureslog2 + vreslog2 - ntileslog2
    int n = faceres.ulog2 + faceres.vlog2 - ntileslog2;

    // choose u and v sizes for roughly square result (u ~= v ~= n/2)
    // and make sure tile isn't larger than face
    Res tileres;
    tileres.ulog2 = std::min((n+1)/2, int(faceres.ulog2));
    tileres.vlog2 = std::min(n - tileres.ulog2, int(faceres.vlog2));
    return tileres;
}


void PtexWriterBase::writeConstFaceBlock(FILE* fp, const void* data,
					 FaceDataHeader& fdh)
{
    // write a single const face data block
    // record level data for face and output the one pixel value
    fdh.blocksize = _pixelSize;
    fdh.encoding = enc_constant;
    writeBlock(fp, data, _pixelSize);
}


void PtexWriterBase::writeFaceBlock(FILE* fp, const void* data, int stride, 
				    Res res, FaceDataHeader& fdh)
{
    // write a single face data block
    // copy to temp buffer, and deinterleave
    int ures = res.u(), vres = res.v();
    int blockSize = ures*vres*_pixelSize;
    bool useMalloc = blockSize > AllocaMax;
    char* buff = useMalloc ? (char*) malloc(blockSize) : (char*)alloca(blockSize);
    PtexUtils::deinterleave(data, stride, ures, vres, buff, 
			    ures*DataSize(_header.datatype),
			    _header.datatype, _header.nchannels);
    
    // difference if needed
    bool diff = (_header.datatype == dt_uint8 || 
		 _header.datatype == dt_uint16);
    if (diff) PtexUtils::encodeDifference(buff, blockSize, _header.datatype);

    // compress and stream data to file, and record size in header
    int zippedsize = writeZipBlock(fp, buff, blockSize);

    // record compressed size and encoding in data header
    fdh.blocksize = zippedsize;
    fdh.encoding = diff ? enc_diffzipped : enc_zipped;
    if (useMalloc) free(buff);
}


void PtexWriterBase::writeFaceData(FILE* fp, const void* data, int stride, 
				   Res res, FaceDataHeader& fdh)
{
    // determine whether to break into tiles
    Res tileres = calcTileRes(res);
    int ntilesu = res.ntilesu(tileres);
    int ntilesv = res.ntilesv(tileres);
    int ntiles = ntilesu * ntilesv;
    if (ntiles == 1) {
	// write single block
	writeFaceBlock(fp, data, stride, res, fdh);
    } else {
	// write tiles to new tmpfile
	FILE* tilefp = tmpfile();

	// alloc tile header
	std::vector<FaceDataHeader> tileHeader(ntiles);
	int tileures = tileres.u();
	int tilevres = tileres.v();
	int tileustride = tileures*_pixelSize;
	int tilevstride = tilevres*stride;
    
	// output tiles
	FaceDataHeader* tdh = &tileHeader[0];
	int datasize = 0;
	const char* rowp = (const char*) data;
	const char* rowpend = rowp + ntilesv * tilevstride;
	for (; rowp < rowpend; rowp += tilevstride) {
	    const char* p = rowp;
	    const char* pend = p + ntilesu * tileustride;
	    for (; p < pend; tdh++, p += tileustride) {
		// determine if tile is constant
		if (PtexUtils::isConstant(p, stride, tileures, tilevres, _pixelSize))
		    writeConstFaceBlock(tilefp, p, *tdh);
		else
		    writeFaceBlock(tilefp, p, stride, tileres, *tdh);
		datasize += tdh->blocksize;
	    }
	}

	// output compressed tile header
	uint32_t tileheadersize = writeZipBlock(tilefp, &tileHeader[0], 
						sizeof(FaceDataHeader)*tileHeader.size());


	// output tile data pre-header
	int totalsize = 0;
	totalsize += writeBlock(fp, &tileres, sizeof(Res));
	totalsize += writeBlock(fp, &tileheadersize, sizeof(tileheadersize));

	// copy compressed tile header from temp file
	totalsize += copyBlock(fp, tilefp, datasize, tileheadersize);

	// copy tile data from temp file
	totalsize += copyBlock(fp, tilefp, 0, datasize);
	fclose(tilefp);

	fdh.blocksize = totalsize;
	fdh.encoding = enc_tiled;
    }
}


void PtexWriterBase::writeReduction(FILE* fp, const void* data, int stride, Res res)
{
    // reduce and write to file
    Ptex::Res newres(res.ulog2-1, res.vlog2-1);
    int buffsize = newres.size() * _pixelSize;
    bool useMalloc = buffsize > AllocaMax;
    char* buff = useMalloc ? (char*) malloc(buffsize) : (char*)alloca(buffsize);

    int dstride = newres.u() * _pixelSize;
    PtexUtils::reduce(data, stride, res.u(), res.v(), buff, dstride,
		      _header.datatype, _header.nchannels);
    writeBlock(fp, buff, buffsize);

    if (useMalloc) free(buff);
}



void PtexWriterBase::writeMetaData(FILE* fp, uint32_t& memsize, uint32_t& zipsize)
{
    memsize = 0;
    zipsize = 0;
    for (MetaData::iterator iter = _metadata.begin(); iter != _metadata.end(); iter++)
    {
	const std::string& key = iter->first;
	MetaEntry& val = iter->second;
	uint8_t keysize = key.size()+1, datatype = val.datatype;
	uint32_t datasize = val.data.size();
	writeZipBlock(_fp, &keysize, sizeof(keysize), 0);
	writeZipBlock(_fp, key.c_str(), keysize, 0);
	writeZipBlock(_fp, &datatype, sizeof(datatype), 0);
	writeZipBlock(_fp, &datasize, sizeof(datasize), 0);
	writeZipBlock(_fp, &val.data[0], datasize, 0);
	memsize += (sizeof(keysize) + keysize + sizeof(datatype)
		    + sizeof(datasize) + datasize);
    }
    zipsize = writeZipBlock(fp, 0, 0, 1);
}



PtexMainWriter::PtexMainWriter()
    : _tmpfp(0)
{
}


PtexMainWriter::~PtexMainWriter()
{
    if (_tmpfp) fclose(_tmpfp);
}


bool PtexMainWriter::open(const char* path, 
			  MeshType mt, DataType dt, 
			  int nchannels, int alphachan, 
			  int nfaces, std::string& error)
{
    // write to temporary file, rename into place when finished
    _finalpath = path;
    std::string tmppath = path; tmppath += ".tmp";
    if (!PtexWriterBase::open(tmppath.c_str(), "wb", mt, dt,
			      nchannels, alphachan, nfaces, error))
	return 0;

    _tmpfp = tmpfile();
    if (!_tmpfp) {
	error = "PtexWriter error: Can't create tmpfile";
	return 0;
    }

    _levels.reserve(20);
    _levels.resize(1);

    _faceinfo.resize(nfaces);
    memset(&_faceinfo[0], 0, sizeof(FaceInfo)*nfaces);
    _levels.front().pos.resize(nfaces);
    _levels.front().fdh.resize(nfaces);
    _rpos.resize(nfaces);
    _constdata.resize(nfaces*_pixelSize);

    return true;
}


bool PtexMainWriter::close(std::string& error)
{
    if (!PtexWriterBase::close(error)) return 0;

    // rename from tmp path to final path
    if (0 != rename(_path.c_str(), _finalpath.c_str())) {
	std::stringstream str;
	str << "Can't create ptex file: " << _finalpath << "\n"
	    << strerror(errno) << "\n"
	    << "Data written to: " << _path;
	error = str.str();
	return 0;
    }
    return 1;
}

bool PtexMainWriter::writeFace(int faceid, const FaceInfo& f, void* data, int stride)
{
    if (!_ok) return 0;
    if (faceid < 0 || size_t(faceid) >= _faceinfo.size()) {
	setError("PtexWriter error: faceid out of range");
	return 0;
    }
    if (stride == 0) stride = f.res.u()*_pixelSize;

    // handle constant case
    if (PtexUtils::isConstant(data, stride, f.res.u(), f.res.v(), _pixelSize))
	return writeConstantFace(faceid, f, data);

    // non-constant case
    if (size_t(faceid) >= _faceinfo.size()) return 0;
    _faceinfo[faceid] = f;
    _faceinfo[faceid].flags = 0;

    // record position of current face
    _levels.front().pos[faceid] = ftello(_tmpfp);

    // write face data
    writeFaceData(_tmpfp, data, stride, f.res, _levels.front().fdh[faceid]);

    // generate first reduction (if needed)
    if (f.res.ulog2 > MinReductionLog2 && f.res.vlog2 > MinReductionLog2) {
	_rpos[faceid] = ftello(_tmpfp);
	writeReduction(_tmpfp, data, stride, f.res);
    }
    else {
	storeConstValue(faceid, data, stride, f.res);
    }
    return 1;
}


bool PtexMainWriter::writeConstantFace(int faceid, const FaceInfo& f, void* data)
{
    if (!_ok) return 0;
    if (faceid < 0 || size_t(faceid) >= _faceinfo.size()) {
	setError("PtexWriter error: faceid out of range");
	return 0;
    }

    // store face value in constant block
    if (size_t(faceid) >= _faceinfo.size()) return 0;
    _faceinfo[faceid] = f;
    _faceinfo[faceid].flags = FaceInfo::flag_constant;
    memcpy(&_constdata[faceid*_pixelSize], data, _pixelSize);
    return 1;
}



void PtexMainWriter::storeConstValue(int faceid, const void* data, int stride, Res res)
{
    // compute average value and store in _constdata block
    PtexUtils::average(data, stride, res.u(), res.v(), &_constdata[faceid*_pixelSize],
		       _header.datatype, _header.nchannels);
}



void PtexMainWriter::finish()
{
    // write reductions to tmp file
    generateReductions();

    // update header
    _header.nlevels = _levels.size();
    _header.nfaces = _faceinfo.size();

    // write blank header (to fill in later)
    writeBlank(_fp, HeaderSize);
    
    // write compressed face info block
    _header.faceinfosize = writeZipBlock(_fp, &_faceinfo[0], 
					 sizeof(FaceInfo)*_header.nfaces);

    // write compressed const data block
    _header.constdatasize = writeZipBlock(_fp, &_constdata[0], _constdata.size());

    // write blank level info block (to fill in later)
    off_t levelInfoPos = ftello(_fp);
    writeBlank(_fp, LevelInfoSize * _header.nlevels);

    // write level data blocks (and record level info)
    std::vector<LevelInfo> levelinfo(_header.nlevels);
    for (int li = 0; li < _header.nlevels; li++)
    {
	LevelInfo& info = levelinfo[li];
	LevelRec& level = _levels[li];
	int nfaces = level.fdh.size();
	info.nfaces = nfaces;
	// output compressed level data header
	info.levelheadersize = writeZipBlock(_fp, &level.fdh[0],
					     sizeof(FaceDataHeader)*nfaces);
	info.leveldatasize = info.levelheadersize;
	// copy level data from tmp file
	for (int fi = 0; fi < nfaces; fi++)
	    info.leveldatasize += copyBlock(_fp, _tmpfp, level.pos[fi],
					    level.fdh[fi].blocksize);
	_header.leveldatasize += info.leveldatasize;
    }
    fclose(_tmpfp);
    _tmpfp = 0;

    // write meta data
    writeMetaData(_fp, _header.metadatamemsize, _header.metadatazipsize);

    // rewrite level info block
    fseeko(_fp, levelInfoPos, SEEK_SET);
    _header.levelinfosize = writeBlock(_fp, &levelinfo[0], LevelInfoSize*_header.nlevels);

    // rewrite header
    fseeko(_fp, 0, SEEK_SET);
    writeBlock(_fp, &_header, HeaderSize);
}


void PtexMainWriter::generateReductions()
{
    // first generate "rfaceids", reduction faceids,
    // which are faceids reordered by decreasing smaller dimension
    int nfaces = _header.nfaces;
    _rfaceids.resize(nfaces);
    _faceids_r.resize(nfaces);
    PtexUtils::genRfaceids(&_faceinfo[0], nfaces, &_rfaceids[0], &_faceids_r[0]);

    // determine how many faces in each level, and resize _levels
    // traverse in reverse rfaceid order to find number of faces
    // larger than cutoff size of each level
    for (int rfaceid = nfaces-1, cutoffres = MinReductionLog2; rfaceid >= 0; rfaceid--) {
	int faceid = _faceids_r[rfaceid];
	FaceInfo& face = _faceinfo[faceid];
	Res res = face.res;
	int min = face.isConstant() ? 1 : PtexUtils::min(res.ulog2, res.vlog2);
	while (min > cutoffres) {
	    // i == last face for current level
	    int size = rfaceid+1;
	    _levels.push_back(LevelRec());
	    LevelRec& level = _levels.back();
	    level.pos.resize(size);
	    level.fdh.resize(size);
	    cutoffres++;
	}
    }
    
#if 0
    // Debug printouts
    for (int i = 0; i < nfaces; i++) {
	int faceid = _faceids_r[i];
	FaceInfo& face = _faceinfo[faceid];
	printf("rfaceid %d (check %d): size=%d res=%dx%d faceid=%d const=%d\n",
	       i, _rfaceids[faceid], 
	       PtexUtils::min(face.res.ulog2, face.res.vlog2),
	       face.res.u(), face.res.v(), faceid,
	       face.isConstant());
    }

    for (int i = 0; i < _levels.size(); i++) {
	LevelRec& level = _levels[i];
	printf("level %d: %d faces\n", i, int(level.pos.size()));
    }
#endif

    // generate and cache reductions (including const data)
    // first, find largest face and allocate tmp buffer
    int buffsize = 0;
    for (int i = 0; i < nfaces; i++)
	buffsize = PtexUtils::max(buffsize, _faceinfo[i].res.size());
    buffsize *= _pixelSize;
    char* buff = (char*) malloc(buffsize);

    int nlevels = _levels.size();
    for (int i = 1; i < nlevels; i++) {
	LevelRec& level = _levels[i];
	int nextsize = (i+1 < nlevels)? _levels[i+1].fdh.size() : 0;
	for (int rfaceid = 0, size = level.fdh.size(); rfaceid < size; rfaceid++) {
	    // output current reduction for face (previously generated)
	    int faceid = _faceids_r[rfaceid];
	    Res res = _faceinfo[faceid].res;
	    res.ulog2 -= i; res.vlog2 -= i;
	    int stride = res.u() * _pixelSize;
	    int size = res.size() * _pixelSize;
	    fseeko(_tmpfp, _rpos[faceid], SEEK_SET);
	    readBlock(_tmpfp, buff, size);
	    fseeko(_tmpfp, 0, SEEK_END);
	    level.pos[rfaceid] = ftello(_tmpfp);
	    writeFaceData(_tmpfp, buff, stride, res, level.fdh[rfaceid]);

	    // write a new reduction if needed
	    if (rfaceid < nextsize) {
		fseeko(_tmpfp, _rpos[faceid], SEEK_SET);
		writeReduction(_tmpfp, buff, stride, res);
	    }
	    else {
		storeConstValue(faceid, buff, stride, res);
	    }
	}
    }
    fseeko(_tmpfp, 0, SEEK_END);
    free(buff);
}


PtexIncrWriter::PtexIncrWriter()
{
}


PtexIncrWriter::~PtexIncrWriter()
{
}


bool PtexIncrWriter::open(const char* path, Ptex::MeshType mt, Ptex::DataType dt,
			  int nchannels, int alphachan, int nfaces, std::string& error)
{
    if (!PtexWriterBase::open(path, "r+b", mt, dt, nchannels, alphachan, nfaces, error))
	return 0;
    return 1;
}


bool PtexIncrWriter::writeFace(int faceid, const FaceInfo& f, void* data, int stride)
{
    // handle constant case
    bool isconst = PtexUtils::isConstant(data, stride, f.res.u(), f.res.v(), _pixelSize);
    return writeFace(faceid, f, data, stride, isconst);
}


bool PtexIncrWriter::writeConstantFace(int faceid, const FaceInfo& f, void* data)
{
    return writeFace(faceid, f, data, 0, true);
}


bool PtexIncrWriter::writeFace(int faceid, const FaceInfo& f, void* data, int stride,
			       bool isConst)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;

    // init headers
    EditDataHeader edh;
    EditFaceDataHeader efdh;
    edh.edittype = et_editfacedata;
    efdh.faceid = faceid;
    efdh.faceinfo = f;
    efdh.faceinfo.flags = 0;

    // record position and skip headers
    fseeko(_fp, 0, SEEK_END);
    off_t pos = ftello(_fp);
    writeBlank(_fp, sizeof(edh) + sizeof(efdh));
    
    // write face data
    if (isConst)
	writeConstFaceBlock(_fp, data, efdh.fdh);
    else
	writeFaceData(_fp, data, stride, f.res, efdh.fdh);

    // update editsize in header
    edh.editsize = sizeof(efdh) + efdh.fdh.blocksize;

    // rewind and write headers
    fseeko(_fp, pos, SEEK_SET);
    writeBlock(_fp, &edh, sizeof(edh));
    writeBlock(_fp, &efdh, sizeof(efdh));
    return 1;
}


void PtexIncrWriter::finish()
{
    if (!_metadata.empty()) {
	// write meta data

	// init headers
	EditDataHeader edh;
	EditMetaDataHeader emdh;
	edh.edittype = et_editmetadata;

	// record position and skip headers
	fseeko(_fp, 0, SEEK_END);
	off_t pos = ftello(_fp);
	writeBlank(_fp, sizeof(edh) + sizeof(emdh));
    
	// write meta data
	writeMetaData(_fp, emdh.metadatamemsize, emdh.metadatazipsize);

	// update headers
	edh.editsize = sizeof(emdh) + emdh.metadatazipsize;

	// rewind and write headers
	fseeko(_fp, pos, SEEK_SET);
	writeBlock(_fp, &edh, sizeof(edh));
	writeBlock(_fp, &emdh, sizeof(emdh));
    }
}
