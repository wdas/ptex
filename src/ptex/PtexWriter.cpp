/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/

/* Ptex writer classes:

   PtexIncrWriter implements "incremental" mode and simply appends
   "edit" blocks to the end of the file.

   PtexMainWriter implements both writing from scratch and updating
   an existing file, either to add data or to "roll up" previous
   incremental edits.

   Because the various headers (faceinfo, levelinfo, etc.) are
   variable-length and precede the data, and because the data size
   is not known until it is compressed and written, all data
   are written to a temp file and then copied at the end to the
   final location.  This happens during the "finish" phase.

   Each time a texture is written to the file, a reduction of the
   texture is also generated and stored.  These reductions are stored
   in a temporary form and recalled later as the resolution levels are
   generated.

   The final reduction for each face is averaged and stored in the
   const data block.
*/

#include "PtexPlatform.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <sstream>

#include "Ptexture.h"
#include "PtexUtils.h"
#include "PtexWriter.h"


namespace {
    std::string fileError(const char* message, const char* path)
    {
	std::stringstream str;
	str << message << path << "\n" << strerror(errno);
	return str.str();
    }

    bool checkFormat(Ptex::MeshType mt, Ptex::DataType dt, int nchannels, int alphachan,
		     std::string& error)
    {
	// check to see if given file attributes are valid
	if (!PtexIO::LittleEndian()) {
	    error = "PtexWriter doesn't currently support big-endian cpu's";
	    return 0;
	}

	if (mt < Ptex::mt_triangle || mt > Ptex::mt_quad) {
	    error = "PtexWriter error: Invalid mesh type";
	    return 0;
	}

	if (mt == Ptex::mt_triangle) {
	    error = "PtexWriter error: Triangle mesh type not yet supported";
	    return 0;
	}

	if (dt < Ptex::dt_uint8 || dt > Ptex::dt_float) {
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

	return 1;
    }

    FILE* createTempFile(std::string& error)
    {
	// choose temp dir
	static const char* tempdir = 0;
	if (!tempdir) {
	    tempdir = getenv("TEMP");
	    if (!tempdir) tempdir = getenv("TMP");
	    if (!tempdir) tempdir = "/tmp";
	    tempdir = strdup(tempdir);
	}

	// build filename template
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s/ptexXXXXXX", tempdir);

	// create unique temp file
	int fd = mkstemp(path);
	FILE* fp = (fd != -1) ? fdopen(fd, "rb+") : 0;
	if (!fp) {
	    error = fileError("Can't create temp file: ", path);
	    return 0;
	}

	// unlink file (it will get deleted on close)
	unlink(path);
	return fp;
    }
}


PtexWriter* PtexWriter::open(const char* path, 
			     Ptex::MeshType mt, Ptex::DataType dt,
			     int nchannels, int alphachan, int nfaces,
			     std::string& error, bool genmipmaps)
{
    if (!checkFormat(mt, dt, nchannels, alphachan, error))
	return 0;

    // acquire lock file
    PtexLockFile lock(path);
    if (!lock.islocked()) {
	error = fileError("Can't acquire lock file: ", lock.path());
	return 0;
    }

    bool newfile = true;
    PtexMainWriter* w = new PtexMainWriter(path, lock, newfile,
					   mt, dt, nchannels, alphachan, nfaces, 
					   genmipmaps);
    if (!w->ok(error)) {
	w->release();
	return 0;
    }
    return w;
}


PtexWriter* PtexWriter::edit(const char* path, bool incremental,
			     Ptex::MeshType mt, Ptex::DataType dt,
			     int nchannels, int alphachan, int nfaces, 
			     std::string& error, bool genmipmaps)
{
    if (!checkFormat(mt, dt, nchannels, alphachan, error))
	return 0;

    // acquire lock file
    PtexLockFile lock(path);
    if (!lock.islocked()) {
	error = fileError("Can't acquire lock file: ", lock.path());
	return 0;
    }

    // try to open existing file (it might not exist)
    FILE* fp = fopen(path, "rb+");
    if (!fp && errno != ENOENT) {
	error = fileError("Can't open ptex file for update: ", path);
    }

    PtexWriterBase* w = 0;
    // use incremental writer iff incremental mode requested and file exists
    if (incremental && fp) {
	w = new PtexIncrWriter(path, lock, fp, mt, dt, nchannels, alphachan,
			       nfaces);
    }
    // otherwise use main writer
    else {
	bool newfile = true;
	if (fp) {
	    // main writer will use PtexReader to read file
	    newfile = false;
	    fclose(fp);
	    fp = 0;
	}
	w = new PtexMainWriter(path, lock, newfile, mt, dt, nchannels, alphachan,
			       nfaces, genmipmaps);
    }

    if (!w->ok(error)) {
	w->release();
	return 0;
    }
    return w;
}



PtexWriterBase::PtexWriterBase(const char* path, PtexLockFile lock, FILE* fp,
			       Ptex::MeshType mt, Ptex::DataType dt,
			       int nchannels, int alphachan, int nfaces,
			       bool compress)
    : _lock(lock),
      _ok(true),
      _path(path),
      _fp(fp),
      _tilefp(0)
{
    memset(&_header, 0, sizeof(_header));
    _header.magic = Magic;
    _header.version = 1;
    _header.meshtype = mt;
    _header.datatype = dt;
    _header.alphachan = alphachan;
    _header.nchannels = nchannels;
    _header.nfaces = nfaces;
    _header.nlevels = 0;
    _pixelSize = _header.pixelSize();

    memset(&_zstream, 0, sizeof(_zstream));
    deflateInit(&_zstream, compress ? Z_DEFAULT_COMPRESSION : 0);

    // create temp file for writing tiles
    // (must compress each tile before assembling a tiled face)
    std::string error;
    _tilefp = createTempFile(error);
    if (!_tilefp) { setError(error); return; }
}


void PtexWriterBase::release()
{
    std::string error;
    // close file if app didn't, and report error if any
    if (_fp && !close(error))
	std::cerr << error << std::endl;
    delete this;
}

PtexWriterBase::~PtexWriterBase()
{
    deflateEnd(&_zstream);
}


bool PtexWriterBase::close(std::string& error)
{
    if (_ok) finish();
    if (!_ok) error = getError();
    if (_fp) {
	fclose(_fp);
	_fp = 0;
    }
    if (_tilefp) {
	fclose(_tilefp);
    }
    return _ok;
}


void PtexWriterBase::writeMeta(const char* key, const char* value)
{
    addMetaData(key, mdt_string, value, int(strlen(value)+1));
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
    static char zeros[BlockSize] = {0};
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


int PtexWriterBase::copyBlock(FILE* dst, FILE* src, FilePos pos, int size)
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
    tileres.ulog2 = PtexUtils::min((n+1)/2, int(faceres.ulog2));
    tileres.vlog2 = PtexUtils::min(n - tileres.ulog2, int(faceres.vlog2));
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
	// write tiles to tilefp temp file
	rewind(_tilefp);

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
	for (; rowp != rowpend; rowp += tilevstride) {
	    const char* p = rowp;
	    const char* pend = p + ntilesu * tileustride;
	    for (; p != pend; tdh++, p += tileustride) {
		// determine if tile is constant
		if (PtexUtils::isConstant(p, stride, tileures, tilevres, _pixelSize))
		    writeConstFaceBlock(_tilefp, p, *tdh);
		else
		    writeFaceBlock(_tilefp, p, stride, tileres, *tdh);
		datasize += tdh->blocksize;
	    }
	}

	// output compressed tile header
	uint32_t tileheadersize = writeZipBlock(_tilefp, &tileHeader[0], 
						int(sizeof(FaceDataHeader)*tileHeader.size()));


	// output tile data pre-header
	int totalsize = 0;
	totalsize += writeBlock(fp, &tileres, sizeof(Res));
	totalsize += writeBlock(fp, &tileheadersize, sizeof(tileheadersize));

	// copy compressed tile header from temp file
	totalsize += copyBlock(fp, _tilefp, datasize, tileheadersize);

	// copy tile data from temp file
	totalsize += copyBlock(fp, _tilefp, 0, datasize);

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
	uint8_t keysize = uint8_t(key.size()+1);
	uint8_t datatype = val.datatype;
	uint32_t datasize = uint32_t(val.data.size());
	writeZipBlock(fp, &keysize, sizeof(keysize), false);
	writeZipBlock(fp, key.c_str(), keysize, false);
	writeZipBlock(fp, &datatype, sizeof(datatype), false);
	writeZipBlock(fp, &datasize, sizeof(datasize), false);
	writeZipBlock(fp, &val.data[0], datasize, false);
	memsize += (sizeof(keysize) + keysize + sizeof(datatype)
		    + sizeof(datasize) + datasize);
    }
    if (memsize) {
	// finish zip block
	zipsize = writeZipBlock(fp, 0, 0, /*finish*/ true);
    }
}



PtexMainWriter::PtexMainWriter(const char* path, PtexLockFile lock, bool newfile,
			       Ptex::MeshType mt, Ptex::DataType dt,
			       int nchannels, int alphachan, int nfaces, bool genmipmaps)
    : PtexWriterBase(path, lock, 0, mt, dt, nchannels, alphachan, nfaces,
		     /* compress */ true),
      _hasNewData(false),
      _genmipmaps(genmipmaps),
      _reader(0)
{
    _fp = createTempFile(_error);
    if (!_fp) { _ok = 0; return; }

    // data will be written to a ".new" path and then renamed to final location
    _newpath = path; _newpath += ".new";

    _levels.reserve(20);
    _levels.resize(1);

    // init faceinfo and set flags to -1 to mark as uninitialized
    _faceinfo.resize(nfaces);
    for (int i = 0; i < nfaces; i++) _faceinfo[i].flags = uint8_t(-1);

    _levels.front().pos.resize(nfaces);
    _levels.front().fdh.resize(nfaces);
    _rpos.resize(nfaces);
    _constdata.resize(nfaces*_pixelSize);

    if (!newfile) {
	// open reader for existing file
	PtexTexture* tex = PtexTexture::open(path, _error);
	if (!tex) { 
	    _ok = 0;
	    return;
	}
	_reader = dynamic_cast<PtexReader*>(tex);
	if (!_reader) {
	    setError("Internal error: dynamic_cast<PtexReader*> failed");
	    tex->release();
	    return;
	}

	// make sure header matches
	bool headerMatch = (mt == _reader->meshType() &&
			    dt == _reader->dataType() &&
			    nchannels == _reader->numChannels() &&
			    alphachan == _reader->alphaChannel() &&
			    nfaces == _reader->numFaces());
	if (!headerMatch) {
	    std::stringstream str;
	    str << "PtexWriter::edit error: header doesn't match existing file, "
		<< "conversions not currently supported";
	    setError(str.str());
	    return;
	}

	// copy meta data from existing file
	PtexPtr<PtexMetaData> meta = _reader->getMetaData();
	writeMeta(meta);

	// see if we have any edits
	_hasNewData = _reader->hasEdits();
    }
}


PtexMainWriter::~PtexMainWriter()
{
    if (_reader) _reader->release();
}


bool PtexMainWriter::close(std::string& error)
{
    // closing base writer will write all pending data via finish() method
    // and will close _fp (which in this case is on the temp disk)
    bool result = PtexWriterBase::close(error);
    if (result && _hasNewData) {
	// rename temppath into final location
	if (rename(_newpath.c_str(), _path.c_str()) == -1) {
	    error = fileError("Can't write to ptex file: ", _path.c_str());
	    unlink(_newpath.c_str());
	    result = false;
	}
    }
    return result;
}

bool PtexMainWriter::writeFace(int faceid, const FaceInfo& f, const void* data, int stride)
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
    _faceinfo[faceid] = f;

    // clear non-user-settable flags
    _faceinfo[faceid].flags &= FaceInfo::flag_subface;

    // record position of current face
    _levels.front().pos[faceid] = ftello(_fp);

    // write face data
    writeFaceData(_fp, data, stride, f.res, _levels.front().fdh[faceid]);
    if (!_ok) return 0;

    // premultiply (if needed) before making reductions; use temp copy of data
    uint8_t* temp = 0;
    if (_header.hasAlpha()) {
	// first copy to temp buffer
	int rowlen = f.res.u() * _pixelSize, nrows = f.res.v();
	temp = (uint8_t*) malloc(rowlen * nrows);
	PtexUtils::copy(data, stride, temp, rowlen, nrows, rowlen);

	// multiply alpha
	PtexUtils::multalpha(temp, f.res.size(), _header.datatype, _header.nchannels,
			     _header.alphachan);

	// override source buffer
	data = temp;
	stride = rowlen;
    }

    // generate first reduction (if needed)
    if (_genmipmaps &&
	(f.res.ulog2 > MinReductionLog2 && f.res.vlog2 > MinReductionLog2))
    {
	_rpos[faceid] = ftello(_fp);
	writeReduction(_fp, data, stride, f.res);
    }
    else {
	storeConstValue(faceid, data, stride, f.res);
    }

    if (temp) free(temp);
    _hasNewData = true;
    return 1;
}


bool PtexMainWriter::writeConstantFace(int faceid, const FaceInfo& f, const void* data)
{
    if (!_ok) return 0;
    if (faceid < 0 || size_t(faceid) >= _faceinfo.size()) {
	setError("PtexWriter error: faceid out of range");
	return 0;
    }

    // store face value in constant block
    if (size_t(faceid) >= _faceinfo.size()) return 0;
    _faceinfo[faceid] = f;

    // clear non-user-settable flags
    _faceinfo[faceid].flags &= FaceInfo::flag_subface;

    // set constant flag
    _faceinfo[faceid].flags |= FaceInfo::flag_constant;
    memcpy(&_constdata[faceid*_pixelSize], data, _pixelSize);
    _hasNewData = true;
    return 1;
}



void PtexMainWriter::storeConstValue(int faceid, const void* data, int stride, Res res)
{
    // compute average value and store in _constdata block
    uint8_t* constdata = &_constdata[faceid*_pixelSize];
    PtexUtils::average(data, stride, res.u(), res.v(), constdata,
		       _header.datatype, _header.nchannels);
    if (_header.hasAlpha())
	PtexUtils::divalpha(constdata, 1, _header.datatype, _header.nchannels, _header.alphachan);
}



void PtexMainWriter::finish()
{
    // do nothing if there's no new data to write
    if (!_hasNewData) return;

    // copy missing faces from _reader
    if (_reader) {
	for (int i = 0, nfaces = _header.nfaces; i < nfaces; i++) {
	    if (_faceinfo[i].flags == uint8_t(-1)) {
		// copy face data
                const Ptex::FaceInfo& info = _reader->getFaceInfo(i);
                int size = _pixelSize * info.res.size();
                if (info.isConstant()) {
                    PtexPtr<PtexFaceData> data = _reader->getData(i);
                    if (data) {
                        writeConstantFace(i, info, data->getData());
                    }
                } else {
                    void* data = malloc(size);
                    _reader->getData(i, data, 0);
                    writeFace(i, info, data, 0);
                    free(data);
                }
	    }
	}
    }
    else {
	// just flag missing faces as constant (black)
	for (int i = 0, nfaces = _header.nfaces; i < nfaces; i++) {
	    if (_faceinfo[i].flags == uint8_t(-1))
		_faceinfo[i].flags = FaceInfo::flag_constant;
	}
    }

    // write reductions to tmp file
    if (_genmipmaps)
	generateReductions();

    // flag faces w/ constant neighborhoods
    flagConstantNeighorhoods();

    // update header
    _header.nlevels = uint16_t(_levels.size());
    _header.nfaces = uint32_t(_faceinfo.size());

    // create new file
    FILE* newfp = fopen(_newpath.c_str(), "w+b");
    if (!newfp) {
	setError(fileError("Can't write to ptex file: ", _newpath.c_str()));
	return;
    }

    // write blank header (to fill in later)
    writeBlank(newfp, HeaderSize);

    // write compressed face info block
    _header.faceinfosize = writeZipBlock(newfp, &_faceinfo[0], 
					 sizeof(FaceInfo)*_header.nfaces);

    // write compressed const data block
    _header.constdatasize = writeZipBlock(newfp, &_constdata[0], int(_constdata.size()));

    // write blank level info block (to fill in later)
    FilePos levelInfoPos = ftello(newfp);
    writeBlank(newfp, LevelInfoSize * _header.nlevels);

    // write level data blocks (and record level info)
    std::vector<LevelInfo> levelinfo(_header.nlevels);
    for (int li = 0; li < _header.nlevels; li++)
    {
	LevelInfo& info = levelinfo[li];
	LevelRec& level = _levels[li];
	int nfaces = int(level.fdh.size());
	info.nfaces = nfaces;
	// output compressed level data header
	info.levelheadersize = writeZipBlock(newfp, &level.fdh[0],
					     sizeof(FaceDataHeader)*nfaces);
	info.leveldatasize = info.levelheadersize;
	// copy level data from tmp file
	for (int fi = 0; fi < nfaces; fi++)
	    info.leveldatasize += copyBlock(newfp, _fp, level.pos[fi],
					    level.fdh[fi].blocksize);
	_header.leveldatasize += info.leveldatasize;
    }

    // write meta data
    writeMetaData(newfp, _header.metadatamemsize, _header.metadatazipsize);

    // rewrite level info block
    fseeko(newfp, levelInfoPos, SEEK_SET);
    _header.levelinfosize = writeBlock(newfp, &levelinfo[0], LevelInfoSize*_header.nlevels);

    // rewrite header
    fseeko(newfp, 0, SEEK_SET);
    writeBlock(newfp, &_header, HeaderSize);
    fclose(newfp);
}


void PtexMainWriter::flagConstantNeighorhoods()
{
    // for each constant face
    for (int faceid = 0, n = int(_faceinfo.size()); faceid < n; faceid++) {
	FaceInfo& f = _faceinfo[faceid];
	if (!f.isConstant()) continue;
	uint8_t* constdata = &_constdata[faceid*_pixelSize];

	// check to see if neighborhood is constant
	bool isConst = true;
	for (int eid = 0; eid < 4; eid++) {
	    bool prevWasSubface = f.isSubface();
	    int prevFid = faceid;
	    // traverse across edge
	    int afid = f.adjface(eid);
	    int aeid = f.adjedge(eid);
	    int count = 0;
	    const int maxcount = 10; // max valence (as safety valve)
	    while (afid != faceid) {
		// if we hit a boundary, assume non-const (not worth
		// the trouble to redo traversal from CCW direction;
		// also, boundary might want to be "black")
		// assume const if we hit max valence too
		if (afid < 0 || ++count == maxcount)
		{ isConst = false; break; }

		// check if neighor is constant, and has the same value as face
		FaceInfo& af = _faceinfo[afid];
		if (!af.isConstant() || 
		    0 != memcmp(constdata, &_constdata[afid*_pixelSize], _pixelSize))
		{ isConst = false; break; }

		// traverse around vertex in CW direction
		// handle T junction between subfaces and main face
		bool isSubface = af.isSubface();
		bool isT = prevWasSubface && !isSubface && af.adjface(aeid) == prevFid;
		std::swap(prevFid, afid);
		prevWasSubface = isSubface;

		if (isT) {
		    // traverse to secondary subface across T junction
		    FaceInfo& pf = _faceinfo[afid];
		    int peid = af.adjedge(aeid);
		    peid = (peid + 3) % 4;
		    afid = pf.adjface(peid);
		    aeid = pf.adjedge(peid);
		    aeid = (aeid + 3) % 4;
		}
		else {
		    // traverse around vertex
		    aeid = (aeid + 1) % 4;
		    afid = af.adjface(aeid);
		    aeid = af.adjedge(aeid);
		}
	    }
	    if (!isConst) break;
	}
	if (isConst) f.flags |= FaceInfo::flag_nbconstant;
    }
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
    
    // generate and cache reductions (including const data)
    // first, find largest face and allocate tmp buffer
    int buffsize = 0;
    for (int i = 0; i < nfaces; i++)
	buffsize = PtexUtils::max(buffsize, _faceinfo[i].res.size());
    buffsize *= _pixelSize;
    char* buff = (char*) malloc(buffsize);

    int nlevels = int(_levels.size());
    for (int i = 1; i < nlevels; i++) {
	LevelRec& level = _levels[i];
	int nextsize = (i+1 < nlevels)? int(_levels[i+1].fdh.size()) : 0;
	for (int rfaceid = 0, size = int(level.fdh.size()); rfaceid < size; rfaceid++) {
	    // output current reduction for face (previously generated)
	    int faceid = _faceids_r[rfaceid];
	    Res res = _faceinfo[faceid].res;
	    res.ulog2 -= i; res.vlog2 -= i;
	    int stride = res.u() * _pixelSize;
	    int blocksize = res.size() * _pixelSize;
	    fseeko(_fp, _rpos[faceid], SEEK_SET);
	    readBlock(_fp, buff, blocksize);
	    fseeko(_fp, 0, SEEK_END);
	    level.pos[rfaceid] = ftello(_fp);
	    writeFaceData(_fp, buff, stride, res, level.fdh[rfaceid]);
	    if (!_ok) return;

	    // write a new reduction if needed for next level
	    if (rfaceid < nextsize) {
		fseeko(_fp, _rpos[faceid], SEEK_SET);
		writeReduction(_fp, buff, stride, res);
	    }
	    else {
		// the last reduction for each face is its constant value
		storeConstValue(faceid, buff, stride, res);
	    }
	}
    }
    fseeko(_fp, 0, SEEK_END);
    free(buff);
}


PtexIncrWriter::PtexIncrWriter(const char* path, PtexLockFile lock, FILE* fp, 
			       Ptex::MeshType mt, Ptex::DataType dt,
			       int nchannels, int alphachan, int nfaces)
    : PtexWriterBase(path, lock, fp, mt, dt, nchannels, alphachan, nfaces,
		     /* compress */ false)
{
    // note: incremental saves are not compressed (see compress flag above)
    // to improve save time in the case where in incremental save is followed by
    // a full save (which ultimately it always should be).  With a compressed
    // incremental save, the data would be compressed twice and decompressed once
    // on every save vs. just compressing once.

    // make sure existing header matches
    PtexIO::Header header;
    if (!fread(&header, PtexIO::HeaderSize, 1, fp) || header.magic != Magic) {
	std::stringstream str;
	str << "Not a ptex file: " << path;
	setError(str.str());
	return;
    }
    
    bool headerMatch = (mt == header.meshtype &&
			dt == header.datatype &&
			nchannels == header.nchannels &&
			alphachan == int(header.alphachan) &&
			nfaces == int(header.nfaces));
    if (!headerMatch) {
	std::stringstream str;
	str << "PtexWriter::edit error: header doesn't match existing file, "
	    << "conversions not currently supported";
	setError(str.str());
	return;
    }

    // seek to end of file to append
    fseeko(_fp, 0, SEEK_END);
}


PtexIncrWriter::~PtexIncrWriter()
{
}


bool PtexIncrWriter::writeFace(int faceid, const FaceInfo& f, const void* data, int stride)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;
    if (stride == 0) stride = f.res.u()*_pixelSize;

    // handle constant case
    if (PtexUtils::isConstant(data, stride, f.res.u(), f.res.v(), _pixelSize))
	return writeConstantFace(faceid, f, data);

    // init headers
    uint8_t edittype = et_editfacedata;
    uint32_t editsize;
    EditFaceDataHeader efdh;
    efdh.faceid = faceid;
    efdh.faceinfo = f;
    efdh.faceinfo.flags = 0;

    // record position and skip headers
    FilePos pos = ftello(_fp);
    writeBlank(_fp, sizeof(edittype) + sizeof(editsize) + sizeof(efdh));
    
    // must compute constant (average) val first
    uint8_t* constval = (uint8_t*) malloc(_pixelSize);

    if (_header.hasAlpha()) {
	// must premult alpha before averaging
	// first copy to temp buffer
	int rowlen = f.res.u() * _pixelSize, nrows = f.res.v();
	uint8_t* temp = (uint8_t*) malloc(rowlen * nrows);
	PtexUtils::copy(data, stride, temp, rowlen, nrows, rowlen);

	// multiply alpha
	PtexUtils::multalpha(temp, f.res.size(), _header.datatype, _header.nchannels,
			     _header.alphachan);
	// average
	PtexUtils::average(temp, rowlen, f.res.u(), f.res.v(), constval,
			   _header.datatype, _header.nchannels);
	// unmult alpha
	PtexUtils::divalpha(constval, 1, _header.datatype, _header.nchannels,
			    _header.alphachan);
	free(temp);
    }
    else {
	// average
	PtexUtils::average(data, stride, f.res.u(), f.res.v(), constval,
			   _header.datatype, _header.nchannels);
    }
    // write const val
    writeBlock(_fp, constval, _pixelSize);
    free(constval);

    // write face data
    writeFaceData(_fp, data, stride, f.res, efdh.fdh);

    // update editsize in header
    editsize = sizeof(efdh) + _pixelSize + efdh.fdh.blocksize;

    // rewind and write headers
    fseeko(_fp, pos, SEEK_SET);
    writeBlock(_fp, &edittype, sizeof(edittype));
    writeBlock(_fp, &editsize, sizeof(editsize));
    writeBlock(_fp, &efdh, sizeof(efdh));
    fseeko(_fp, 0, SEEK_END);
    return 1;
}


bool PtexIncrWriter::writeConstantFace(int faceid, const FaceInfo& f, const void* data)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;

    // init headers
    uint8_t edittype = et_editfacedata;
    uint32_t editsize;
    EditFaceDataHeader efdh;
    efdh.faceid = faceid;
    efdh.faceinfo = f;
    efdh.faceinfo.flags = FaceInfo::flag_constant;
    efdh.fdh.blocksize = 0; // const value already stored
    efdh.fdh.encoding = enc_constant;
    editsize = sizeof(efdh) + _pixelSize;

    // write headers
    writeBlock(_fp, &edittype, sizeof(edittype));
    writeBlock(_fp, &editsize, sizeof(editsize));
    writeBlock(_fp, &efdh, sizeof(efdh));
    // write data
    writeBlock(_fp, data, _pixelSize);
    return 1;
}


void PtexIncrWriter::finish()
{
    // write meta data
    if (!_metadata.empty()) {
	// init headers
	uint8_t edittype = et_editmetadata;
	uint32_t editsize;
	EditMetaDataHeader emdh;

	// record position and skip headers
	FilePos pos = ftello(_fp);
	writeBlank(_fp, sizeof(edittype) + sizeof(editsize) + sizeof(emdh));
    
	// write meta data
	writeMetaData(_fp, emdh.metadatamemsize, emdh.metadatazipsize);

	// update headers
	editsize = sizeof(emdh) + emdh.metadatazipsize;

	// rewind and write headers
	fseeko(_fp, pos, SEEK_SET);
	writeBlock(_fp, &edittype, sizeof(edittype));
	writeBlock(_fp, &editsize, sizeof(editsize));
	writeBlock(_fp, &emdh, sizeof(emdh));
	fseeko(_fp, 0, SEEK_END);
    }
}
