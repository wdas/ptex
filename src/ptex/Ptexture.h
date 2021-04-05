#ifndef Ptexture_h
#define Ptexture_h

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

/**
  @file Ptexture.h
  @brief Public API classes for reading, writing, caching, and filtering Ptex files.
*/

#include "PtexExports.h"
#include "PtexInt.h"
#include "PtexVersion.h"

#include <ostream>

#if !defined(PTEX_PLATFORM_WINDOWS)
#  ifndef DOXYGEN
#    define PTEX_USE_STDSTRING
#  endif
#endif

#ifdef DOXYGEN
/** Common data structures and enums used throughout the API */
namespace Ptex {
#else
PTEX_NAMESPACE_BEGIN
#endif

 /** Type of base mesh for which the textures are defined.  A mesh
     can be triangle-based (with triangular textures) or quad-based
     (with rectangular textures). */
enum MeshType {
     mt_triangle,		///< Mesh is triangle-based.
     mt_quad			///< Mesh is quad-based.
 };

/** Type of data stored in texture file. */
enum DataType {
    dt_uint8,		///< Unsigned, 8-bit integer.
    dt_uint16,		///< Unsigned, 16-bit integer.
    dt_half,		///< Half-precision (16-bit) floating point.
    dt_float		///< Single-precision (32-bit) floating point.
};

/** How to handle transformation across edges when filtering */
enum EdgeFilterMode {
    efm_none,		///< Don't do anything with the values.
    efm_tanvec		///< Values are vectors in tangent space; rotate values.
};

/** How to handle mesh border when filtering. */
enum BorderMode {
    m_clamp,		///< texel access is clamped to border
    m_black,		///< texel beyond border are assumed to be black
    m_periodic		///< texel access wraps to other side of face
};

/** Edge IDs used in adjacency data in the Ptex::FaceInfo struct.
    Edge ID usage for triangle meshes is TBD. */
enum EdgeId {
    e_bottom,		///< Bottom edge, from UV (0,0) to (1,0)
    e_right,		///< Right edge, from UV (1,0) to (1,1)
    e_top,			///< Top edge, from UV (1,1) to (0,1)
    e_left			///< Left edge, from UV (0,1) to (0,0)
};

/** Type of meta data entry. */
enum MetaDataType {
    mdt_string,		///< Null-terminated string.
    mdt_int8,		///< Signed 8-bit integer.
    mdt_int16,		///< Signed 16-bit integer.
    mdt_int32,		///< Signed 32-bit integer.
    mdt_float,		///< Single-precision (32-bit) floating point.
    mdt_double		///< Double-precision (32-bit) floating point.
};

/** Look up name of given mesh type. */
PTEXAPI const char* MeshTypeName(MeshType mt);

/** Look up name of given data type. */
PTEXAPI const char* DataTypeName(DataType dt);

/** Look up name of given border mode. */
PTEXAPI const char* BorderModeName(BorderMode m);

/** Look up name of given edge filter mode. */
PTEXAPI const char* EdgeFilterModeName(EdgeFilterMode m);

/** Look up name of given edge ID. */
PTEXAPI const char* EdgeIdName(EdgeId eid);

/** Look up name of given meta data type. */
PTEXAPI const char* MetaDataTypeName(MetaDataType mdt);

/** Look up size of given data type (in bytes). */
inline int DataSize(DataType dt) {
    static const int sizes[] = { 1,2,2,4 };
    return sizes[dt];
}

/** Look up value of given data type that corresponds to the normalized value of 1.0. */
inline float OneValue(DataType dt) {
    static const float one[] = { 255.f, 65535.f, 1.f, 1.f };
    return one[dt];
}

/** Lookup up inverse value of given data type that corresponds to the normalized value of 1.0. */
inline float OneValueInv(DataType dt) {
    static const float one[] = { 1.f/255.f, 1.f/65535.f, 1.f, 1.f };
    return one[dt];
}

/** Convert a number of data values from the given data type to float. */
PTEXAPI void ConvertToFloat(float* dst, const void* src,
                            Ptex::DataType dt, int numChannels);

/** Convert a number of data values from float to the given data type. */
PTEXAPI void ConvertFromFloat(void* dst, const float* src,
                              Ptex::DataType dt, int numChannels);

/** Pixel resolution of a given texture.
    The resolution is stored in log form: ulog2 = log2(ures), vlog2 = log2(vres)).
    Note: negative ulog2 or vlog2 values are reserved for internal use.
*/
struct Res {
    int8_t ulog2;		///< log base 2 of u resolution, in texels
    int8_t vlog2;		///< log base 2 of v resolution, in texels

    /// Default constructor, sets res to 0 (1x1 texel).
    Res() : ulog2(0), vlog2(0) {}

    /// Constructor.
    Res(int8_t ulog2_, int8_t vlog2_) : ulog2(ulog2_), vlog2(vlog2_) {}

    /// Constructor.
    Res(uint16_t value) : ulog2(int8_t(value&0xff)), vlog2(int8_t((value>>8)&0xff)) {}

    /// U resolution in texels.
    int u() const { return 1<<(unsigned)ulog2; }

    /// V resolution in texels.
    int v() const { return 1<<(unsigned)vlog2; }

    /// Resolution as a single 16-bit integer value.
    uint16_t val() const { return uint16_t(ulog2 | (vlog2<<8)); }

    /// Total size of specified texture in texels (u * v).
    int size() const { return u() * v(); }

    /// Comparison operator.
    bool operator==(const Res& r) const { return r.ulog2 == ulog2 && r.vlog2 == vlog2; }

    /// Comparison operator.
    bool operator!=(const Res& r) const { return !(r==*this); }

    /// True if res is >= given res in both u and v directions.
    bool operator>=(const Res& r) const { return ulog2 >= r.ulog2 && vlog2 >= r.vlog2; }

    /// Get value of resolution with u and v swapped.
    Res swappeduv() const { return Res(vlog2, ulog2); }

    /// Swap the u and v resolution values in place.
    void swapuv() { *this = swappeduv(); }

    /// Clamp the resolution value against the given value.
    void clamp(const Res& r) {
        if (ulog2 > r.ulog2) ulog2 = r.ulog2;
        if (vlog2 > r.vlog2) vlog2 = r.vlog2;
    }

    /// Determine the number of tiles in the u direction for the given tile res.
    int ntilesu(Res tileres) const { return 1<<(ulog2-tileres.ulog2); }

    /// Determine the number of tiles in the v direction for the given tile res.
    int ntilesv(Res tileres) const { return 1<<(vlog2-tileres.vlog2); }

    /// Determine the total number of tiles for the given tile res.
    int ntiles(Res tileres) const { return ntilesu(tileres) * ntilesv(tileres); }
};

/** Information about a face, as stored in the Ptex file header.
    The FaceInfo data contains the face resolution and neighboring face
    adjacency information as well as a set of flags describing the face.

    The adjfaces data member contains the face ids of the four neighboring faces.
    The neighbors are accessed in EdgeId order, CCW, starting with the bottom edge.
    The adjedges data member contains the corresponding edge id for each neighboring face.

    If a face has no neighbor for a given edge, the adjface id should be -1, and the
    adjedge id doesn't matter (but is typically zero).

    If an adjacent face is a pair of subfaces, the id of the first subface as encountered
    in a CCW traversal should be stored as the adjface id.
*/
struct FaceInfo {
    Res res;		///< Resolution of face.
    uint8_t adjedges;       ///< Adjacent edges, 2 bits per edge.
    uint8_t flags;		///< Flags.
    int32_t adjfaces[4];	///< Adjacent faces (-1 == no adjacent face).

    /// Default constructor
    FaceInfo() : res(), adjedges(0), flags(0)
    {
        adjfaces[0] = adjfaces[1] = adjfaces[2] = adjfaces[3] = -1;
    }

    /// Constructor.
    FaceInfo(Res res_) : res(res_), adjedges(0), flags(0)
    {
        adjfaces[0] = adjfaces[1] = adjfaces[2] = adjfaces[3] = -1;
    }

    /// Constructor.
    FaceInfo(Res res_, int adjfaces_[4], int adjedges_[4], bool isSubface_=false)
        : res(res_), flags(isSubface_ ? flag_subface : 0)
    {
        setadjfaces(adjfaces_[0], adjfaces_[1], adjfaces_[2], adjfaces_[3]);
        setadjedges(adjedges_[0], adjedges_[1], adjedges_[2], adjedges_[3]);
    }

    /// Access an adjacent edge id.  The eid value must be 0..3.
    EdgeId adjedge(int eid) const { return EdgeId((adjedges >> (2*eid)) & 3); }

    /// Access an adjacent face id.  The eid value must be 0..3.
    int adjface(int eid) const { return adjfaces[eid]; }

    /// Determine if face is constant (by checking a flag).
    bool isConstant() const { return (flags & flag_constant) != 0; }

    /// Determine if neighborhood of face is constant (by checking a flag).
    bool isNeighborhoodConstant() const { return (flags & flag_nbconstant) != 0; }

    /// Determine if face has edits in the file (by checking a flag).
    bool hasEdits() const { return (flags & flag_hasedits) != 0; }

    /// Determine if face is a subface (by checking a flag).
    bool isSubface() const { return (flags & flag_subface) != 0; }

    /// Set the adjfaces data.
    void setadjfaces(int f0, int f1, int f2, int f3)
    { adjfaces[0] = f0, adjfaces[1] = f1, adjfaces[2] = f2; adjfaces[3] = f3; }

    /// Set the adjedges data.
    void setadjedges(int e0, int e1, int e2, int e3)
    { adjedges = (uint8_t)((e0&3) | ((e1&3)<<2) | ((e2&3)<<4) | ((e3&3)<<6)); }

    /// Flag bit values (for internal use).
    enum { flag_constant = 1, flag_hasedits = 2, flag_nbconstant = 4, flag_subface = 8 };
};


/** Memory-managed string. Used for returning error messages from
    API functions.  On most platforms, this is a typedef to
    std::string.  For Windows, this is a custom class that
    implements a subset of std::string.  (Note: std::string cannot
    be passed through a Windows DLL interface).
*/
#ifdef PTEX_USE_STDSTRING
typedef std::string String;
#else
class String
{
public:
    String() : _str(0) {}
    String(const String& str) : _str(0) { *this = str; }
    PTEXAPI ~String();
    PTEXAPI String& operator=(const char* str);
    String& operator=(const String& str) { *this = str._str; return *this; }
    String& operator=(const std::string& str) { *this = str.c_str(); return *this; }
    const char* c_str() const { return _str ? _str : ""; }
    bool empty() const { return _str == 0 || _str[0] == '\0'; }

private:
    char* _str;
};
#endif

/// std::stream output operator.  \relates Ptex::String
#ifndef PTEX_USE_STDSTRING
PTEXAPI std::ostream& operator << (std::ostream& stream, const Ptex::String& str);
#endif


#ifdef DOXYGEN
} // end namespace Ptex
#endif

/**
   @class PtexMetaData
   @brief Meta data accessor

   Meta data is acquired from PtexTexture and accessed through this interface.
 */
class PtexMetaData {
 protected:
    /// Destructor not for public use.  Use release() instead.
    virtual ~PtexMetaData() {}

 public:
    /// Release resources held by this pointer (pointer becomes invalid).
    virtual void release() = 0;

    /// Query number of meta data entries stored in file.
    virtual int numKeys() = 0;

    /// Query the name and type of a meta data entry.
    virtual void getKey(int index, const char*& key, Ptex::MetaDataType& type) = 0;

    /// Query the index and type of a meta data entry by name.
    virtual bool findKey(const char* key, int& index, Ptex::MetaDataType& type) = 0;

    /** Query the value of a given meta data entry.
	If the key doesn't exist or the type doesn't match, value is set to null */
    virtual void getValue(const char* key, const char*& value) = 0;

    /** Query the value of a given meta data entry by index.
	If the index is out of range or the type doesn't match, value is set to null */
    virtual void getValue(int index, const char*& value) = 0;

    /** Query the value of a given meta data entry.
	If the key doesn't exist or the type doesn't match, value is set to null */
    virtual void getValue(const char* key, const int8_t*& value, int& count) = 0;

    /** Query the value of a given meta data entry by index.
	If the index is out of range or the type doesn't match, value is set to null */
    virtual void getValue(int index, const int8_t*& value, int& count) = 0;

    /** Query the value of a given meta data entry.
	If the key doesn't exist or the type doesn't match, value is set to null */
    virtual void getValue(const char* key, const int16_t*& value, int& count) = 0;

    /** Query the value of a given meta data entry by index.
	If the index is out of range or the type doesn't match, value is set to null */
    virtual void getValue(int index, const int16_t*& value, int& count) = 0;

    /** Query the value of a given meta data entry.
	If the key doesn't exist or the type doesn't match, value is set to null */
    virtual void getValue(const char* key, const int32_t*& value, int& count) = 0;

    /** Query the value of a given meta data entry by index.
	If the index is out of range or the type doesn't match, value is set to null */
    virtual void getValue(int index, const int32_t*& value, int& count) = 0;

    /** Query the value of a given meta data entry.
	If the key doesn't exist or the type doesn't match, value is set to null */
    virtual void getValue(const char* key, const float*& value, int& count) = 0;

    /** Query the value of a given meta data entry by index.
	If the index is out of range or the type doesn't match, value is set to null */
    virtual void getValue(int index, const float*& value, int& count) = 0;

    /** Query the value of a given meta data entry.
	If the key doesn't exist or the type doesn't match, value is set to null */
    virtual void getValue(const char* key, const double*& value, int& count) = 0;

    /** Query the value of a given meta data entry by index.
	If the index is out of range or the type doesn't match, value is set to null */
    virtual void getValue(int index, const double*& value, int& count) = 0;
};


/**
    @class PtexFaceData
    @brief Per-face texture data accessor

    Per-face texture data is acquired from PtexTexture and accessed
    through this interface.  This interface provides low-level access
    to the data as stored on disk for maximum efficiency.  If this
    isn't needed, face data can be more conveniently read directly
    from PtexTexture.
 */
class PtexFaceData {
 protected:
    /// Destructor not for public use.  Use release() instead.
    virtual ~PtexFaceData() {}

 public:
    /// Release resources held by this pointer (pointer becomes invalid).
    virtual void release() = 0;

    /** True if this data block is constant. */
    virtual bool isConstant() = 0;

    /** Resolution of the texture held by this data block.  Note: the
        indicated texture res may be larger than 1x1 even if the
        texture data is constant. */
    virtual Ptex::Res res() = 0;

    /** Read a single texel from the data block.  The texel coordinates, u and v, have
        a range of [0..ures-1, 0..vres-1].  Note: this method will work correctly even if
        the face is constant or tiled. */
    virtual void getPixel(int u, int v, void* result) = 0;

    /** Access the data from this data block.

        If the data block is constant, getData will return a pointer to a single texel's data value.

        If the data block is tiled, then getData will return null and
        the data must be accessed per-tile via the getTile() function. */
    virtual void* getData() = 0;

    /** True if this data block is tiled.
        If tiled, the data must be access per-tile via getTile(). */
    virtual bool isTiled() = 0;

    /** Resolution of each tile in this data block. */
    virtual Ptex::Res tileRes() = 0;

    /** Access a tile from the data block.  Tiles are accessed in v-major order. */
    virtual PtexFaceData* getTile(int tile) = 0;
};


/**
   @class PtexTexture
   @brief Interface for reading data from a ptex file

   PtexTexture instances can be acquired via the static open() method, or via
   the PtexCache interface.

   Data access through this interface is returned in v-major order with all data channels interleaved per texel.
 */
class PtexTexture {
 protected:
    /// Destructor not for public use.  Use release() instead.
    virtual ~PtexTexture() {}

 public:
    /** Open a ptex file for reading.

        If an error occurs, an error message will be stored in the
        error string param and a null pointer will be returned.

        If the premultiply param is set to true and the texture file has a specified alpha channel,
        then all data stored in the file will be multiplied by alpha when read from disk.  If premultiply
        is false, then the full-resolution textures will be returned as stored on disk which is assumed
        to be unmultiplied.  Reductions (both stored mip-maps and dynamically generated reductions) are
        always premultiplied with alpha.  See PtexWriter for more information about alpha channels.
    */
    PTEXAPI static PtexTexture* open(const char* path, Ptex::String& error, bool premultiply=0);


    /// Release resources held by this pointer (pointer becomes invalid).
    virtual void release() = 0;

    /** Path that file was opened with.  If the file was opened using a search path (via PtexCache),
        the the path will be the path as found in the search path.  Otherwise, the path will be
        the path as supplied to open. */
    virtual const char* path() = 0;

    /** Get most commonly used info in a single call for convenience / efficiency */
    struct Info {
        MeshType meshType;
        DataType dataType;
        BorderMode uBorderMode;
        BorderMode vBorderMode;
        EdgeFilterMode edgeFilterMode;
        int alphaChannel;
        int numChannels;
        int numFaces;
    };
    virtual Info getInfo() = 0;

    /** Type of mesh for which texture data is defined. */
    virtual Ptex::MeshType meshType() = 0;

    /** Type of data stored in file. */
    virtual Ptex::DataType dataType() = 0;

    /** Mode for filtering texture access beyond mesh border. */
    virtual Ptex::BorderMode uBorderMode() = 0;

    /** Mode for filtering texture access beyond mesh border. */
    virtual Ptex::BorderMode vBorderMode() = 0;

    /** Mode for filtering textures across edges. */
    virtual Ptex::EdgeFilterMode edgeFilterMode() = 0;

    /** Index of alpha channel (if any).  One channel in the file can be flagged to be the alpha channel.
        If no channel is acting as the alpha channel, -1 is returned.
        See PtexWriter for more details.  */
    virtual int alphaChannel() = 0;

    /** Number of channels stored in file. */
    virtual int numChannels() = 0;

    /** Number of faces stored in file. */
    virtual int numFaces() = 0;

    /** True if the file has edit blocks.  See PtexWriter for more details. */
    virtual bool hasEdits() = 0;

    /** True if the file has mipmaps.  See PtexWriter for more details. */
    virtual bool hasMipMaps() = 0;

    /** Access meta data. */
    virtual PtexMetaData* getMetaData() = 0;

    /** Access resolution and adjacency information about a face. */
    virtual const Ptex::FaceInfo& getFaceInfo(int faceid) = 0;

    /** Access texture data for a face at highest-resolution.

        The texture data is copied into the user-supplied buffer.
        The buffer must be at least this size (in bytes):
        DataSize(dataType()) * numChannels() * getFaceInfo(faceid).res.size().

        If a stride is given, then (stride-row_length) bytes will be
        skipped after each row.  If stride is zero, then no bytes will
        be skipped.  Note: the image can be flipped vertically by using
        an appropriate negative stride value.

        @param faceid Face index [0..numFaces-1]
        @param buffer User-supplied buffer
        @param stride Size of each row in user buffer (in bytes)
    */
    virtual void getData(int faceid, void* buffer, int stride) = 0;

    /** Access texture data for a face at a specific resolution.

        The specified resolution may be lower than the full resolution
        for the face.  If it is lower, then the texture data is
        accessed from the stored mip-maps.  If the requested
        resolution doesn't match a stored resolution, the desired
        resolution will be generated from the nearest available
        resolution.

        See previous getData() method for interface details.
     */
    virtual void getData(int faceid, void* buffer, int stride, Ptex::Res res) = 0;

    /** Access texture data for a face at highest-resolution as stored on disk. */
    virtual PtexFaceData* getData(int faceid) = 0;

    /** Access texture data for a face at a specific resolution as stored on disk.

        The specified resolution may be lower (but not higher) than
        the full resolution for the face.  If it is lower, then the
        texture data is accessed from the stored mip-maps.  If the
        requested resolution doesn't match a stored resolution, the
        desired resolution will be generated from the nearest
        available resolution.
      */
    virtual PtexFaceData* getData(int faceid, Ptex::Res res) = 0;

    /** Access a single texel from the highest resolution texture .
        The texel data is converted to floating point (integer types
        are normalized 0.0 to 1.0).  A subset of the available
        channels may be accessed.

        @param faceid Face index [0..numFaces-1]
        @param u U coordinate [0..ures-1]
        @param v V coordinate [0..vres-1]
        @param result Result data
        @param firstchan First channel to access [0..numChannels-1]
        @param nchannels Number of channels to access.
     */
    virtual void getPixel(int faceid, int u, int v,
                          float* result, int firstchan, int nchannels) = 0;

    /** Access a single texel for a face at a particular resolution.

        The specified resolution may be lower (but not higher) than
        the full resolution for the face.  If it is lower, then the
        texture data is accessed from the stored mip-maps.  If the
        requested resolution doesn't match a stored resolution, the
        desired resolution will be generated from the nearest
        available resolution.

        See previous getPixel() method for details.
    */
    virtual void getPixel(int faceid, int u, int v,
                          float* result, int firstchan, int nchannels,
                          Ptex::Res res) = 0;
};


/** @class PtexInputHandler
    @brief Custom handler interface for intercepting and redirecting Ptex input stream calls

    A custom instance of this class can be defined and supplied to the PtexCache class.
    Files accessed through the cache will have their input streams redirected through this
    interface.
 */
class PtexInputHandler {
 protected:
    virtual ~PtexInputHandler() {}

 public:
    typedef void* Handle;

    /** Open a file in read mode.
        Returns null if there was an error.
        If an error occurs, the error string is available via lastError().
    */
    virtual Handle open(const char* path) = 0;

    /** Seek to an absolute byte position in the input stream. */
    virtual void seek(Handle handle, int64_t pos) = 0;

    /** Read a number of bytes from the file.
        Returns the number of bytes successfully read.
        If less than the requested number of bytes is read, the error string
        is available via lastError().
    */
    virtual size_t read(void* buffer, size_t size, Handle handle) = 0;

    /** Close a file.  Returns false if an error occurs, and the error
        string is available via lastError().  */
    virtual bool close(Handle handle) = 0;

    /** Return the last error message encountered. */
    virtual const char* lastError() = 0;
};


/** @class PtexErrorHandler
    @brief Custom handler interface redirecting Ptex error messages

    A custom instance of this class can be defined and supplied to the PtexCache class.
    Files accessed through the cache will have their error streams redirected through this
    interface.
 */
class PtexErrorHandler {
 protected:
    virtual ~PtexErrorHandler() {}

 public:
    virtual void reportError(const char* error) = 0;
};


/**
   @class PtexCache
   @brief File-handle and memory cache for reading ptex files

   The PtexCache class allows cached read access to multiple ptex
   files while constraining the open file count and memory usage to
   specified limits.  File and data objects accessed via the cache
   are added back to the cache when their release method is called.
   Released objects are maintained in an LRU list and only destroyed
   when the specified resource limits are exceeded.

   The cache is fully multi-threaded.  Cached data will be shared among
   all threads that have access to the cache, and the data are protected
   with internal locks.  See PtexCache.cpp for details about the caching
   and locking implementation.
 */

class PtexCache {
 protected:
    /// Destructor not for public use.  Use release() instead.
    virtual ~PtexCache() {}

 public:
    /** Create a cache with the specified limits.

        @param maxFiles Maximum open file handles.  If zero,
        limit is set to 100 open files.

        @param maxMem Maximum allocated memory, in bytes.  If zero
        the cache is unlimited.

        @param premultiply If true, textures will be premultiplied by
        the alpha channel (if any) when read from disk.  For authoring
        purposes, this should generally be set to false, and for
        rendering purposes, this should generally be set to true.  See
        PtexTexture and PtexWriter for more details.

        @param inputHandler If specified, all input calls made through this cache will
        be directed through the handler.

        @param errorHandler If specified, errors encounted with files access through
        this cache will be directed to the handler.  By default, errors will be
        reported to stderr.
     */
    PTEXAPI static PtexCache* create(int maxFiles,
                                     size_t maxMem,
                                     bool premultiply=false,
                                     PtexInputHandler* inputHandler=0,
                                     PtexErrorHandler* errorHandler=0);

    /// Release PtexCache.  Cache will be immediately destroyed and all resources will be released.
    virtual void release() = 0;

    /** Set a search path for finding textures.
        Note: if an input handler is installed the search path will be ignored.

        @param path colon-delimited search path.
     */
    virtual void setSearchPath(const char* path) = 0;

    /** Query the search path.  Returns string set via setSearchPath.  */
    virtual const char* getSearchPath() = 0;

    /** Access a texture.  If the specified path was previously accessed
        from the cache, then a pointer to the cached texture will be
        returned.

        If the specified path hasn't been opened yet or was purged
        from the cache (via the purge or purgeAll methods) then the
        file will be opened.  If the path is relative (i.e. doesn't
        begin with a '/') then the search path will be used to locate
        the file.

        The texture will be accessible until the PtexTexture::release
        method is called, at which point the texture will be returned
        to the cache.  Once released, the texture may have it's data
        pruned (immediately or some time later) to stay within the
        maximum cache size.

        If the texture could not be opened, null will be returned and
        an error string will be set.  If an error were previously
        encountered with the file (include the file not being found),
        null will be returned and no error string will be set.

        @param path File path.  If path is relative, search path will
        be used to find the file.

        @param error Error string set if texture could not be
        opened.
     */
    virtual PtexTexture* get(const char* path, Ptex::String& error) = 0;

    /** Remove a texture file from the cache.  If the texture is in use
        by another thread, that reference will remain valid and the file
        will be purged once it is no longer in use.  This texture
        should be released immediately after purging.
     */
    virtual void purge(PtexTexture* texture) = 0;

    /** Remove a texture file from the cache by pathname.  The path must
        match the full path as opened.  This function will not search
        for the file, but if a search path was used, the path must
        match the path as found by the search path.
     */
    virtual void purge(const char* path) = 0;

    /** Remove all texture files from the cache. Textures with
        active PtexTexture* handles will remain valid and will be purged
        upon release.
     */
    virtual void purgeAll() = 0;

    struct Stats {
        uint64_t memUsed;
        uint64_t peakMemUsed;
        uint64_t filesOpen;
        uint64_t peakFilesOpen;
        uint64_t filesAccessed;
        uint64_t fileReopens;
        uint64_t blockReads;
    };

    /** Get stats. */
    virtual void getStats(Stats& stats) = 0;
};


/**
   @class PtexWriter
   @brief Interface for writing data to a ptex file.

   Note: if an alpha channel is specified, then the textures being
   written to the file are expected to have unmultiplied-alpha data.
   Generated mipmaps will be premultiplied by the Ptex library.  On
   read, PtexTexture will (if requested) premultiply all textures by
   alpha when getData is called; by default only reductions are
   premultiplied.  If the source textures are already premultiplied,
   then alphachan can be set to -1 and the library will just leave all
   the data as-is.  The only reason to store unmultiplied-alpha
   textures in the file is to preserve the original texture data for
   later editing.
*/

class PtexWriter {
 protected:
    /// Destructor not for public use.  Use release() instead.
    virtual ~PtexWriter() {}

 public:
    /** Open a new texture file for writing.
        @param path Path to file.
        @param mt Type of mesh for which the textures are defined.
        @param dt Type of data stored within file.
        @param nchannels Number of data channels.
        @param alphachan Index of alpha channel, [0..nchannels-1] or -1 if no alpha channel is present.
        @param nfaces Number of faces in mesh.
        @param error String containing error message if open failed.
        @param genmipmaps Specify true if mipmaps should be generated.
     */
    PTEXAPI
    static PtexWriter* open(const char* path,
                            Ptex::MeshType mt, Ptex::DataType dt,
                            int nchannels, int alphachan, int nfaces,
                            Ptex::String& error, bool genmipmaps=true);

    /** Open an existing texture file for writing.

        If the incremental param is specified as true, then data
        values written to the file are appended to the file as "edit
        blocks".  This is the fastest way to write data to the file, but
        edit blocks are slower to read back, and they have no mipmaps so
        filtering can be inefficient.

        If incremental is false, then the edits are applied to the
        file and the entire file is regenerated on close as if it were
        written all at once with open().

        If the file doesn't exist it will be created and written as if
        open() were used.  If the file exists, the mesh type, data
        type, number of channels, alpha channel, and number of faces
        must agree with those stored in the file.
     */
    PTEXAPI
    static PtexWriter* edit(const char* path, bool incremental,
                            Ptex::MeshType mt, Ptex::DataType dt,
                            int nchannels, int alphachan, int nfaces,
                            Ptex::String& error, bool genmipmaps=true);

    /** Apply edits to a file.

        If a file has pending edits, the edits will be applied and the
        file will be regenerated with no edits.  This is equivalent to
        calling edit() with incremental set to false.  The advantage
        is that the file attributes such as mesh type, data type,
        etc., don't need to be known in advance.
     */
    PTEXAPI
    static bool applyEdits(const char* path, Ptex::String& error);

    /** Release resources held by this pointer (pointer becomes invalid). */
    virtual void release() = 0;

    /** Set border modes */
    virtual void setBorderModes(Ptex::BorderMode uBorderMode, Ptex::BorderMode vBorderMode) = 0;

    /** Set edge filter mode */
    virtual void setEdgeFilterMode(Ptex::EdgeFilterMode edgeFilterMode) = 0;

    /** Write a string as meta data.  Both the key and string params must be null-terminated strings. */
    virtual void writeMeta(const char* key, const char* string) = 0;

    /** Write an array of signed 8-bit integers as meta data.  The key must be a null-terminated string. */
    virtual void writeMeta(const char* key, const int8_t* value, int count) = 0;

    /** Write an array of signed 16-bit integers as meta data.  The key must be a null-terminated string. */
    virtual void writeMeta(const char* key, const int16_t* value, int count) = 0;

    /** Write an array of signed 32-bit integers as meta data.  The key must be a null-terminated string. */
    virtual void writeMeta(const char* key, const int32_t* value, int count) = 0;

    /** Write an array of signed 32-bit floats as meta data.  The key must be a null-terminated string. */
    virtual void writeMeta(const char* key, const float* value, int count) = 0;

    /** Write an array of signed 32-bit doubles as meta data.  The key must be a null-terminated string. */
    virtual void writeMeta(const char* key, const double* value, int count) = 0;

    /** Copy meta data from an existing meta data block. */
    virtual void writeMeta(PtexMetaData* data) = 0;

    /** Write texture data for a face.
        The data is assumed to be channel-interleaved per texel and stored in v-major order.

        @param faceid Face index [0..nfaces-1].
        @param info Face resolution and adjacency information.
        @param data Texel data.
        @param stride Distance between rows, in bytes (if zero, data is assumed packed).

        If an error is encountered while writing, false is returned and an error message can be
        retrieved when close is called.
     */
    virtual bool writeFace(int faceid, const Ptex::FaceInfo& info, const void* data, int stride=0) = 0;

    /** Write constant texture data for a face.
        The data is written as a single constant texel value.  Note: the resolution specified in the
        info param may indicate a resolution greater than 1x1 and the value will be preserved when
        reading.  This is useful to indicate a texture's logical resolution even when the data is
        constant. */
    virtual bool writeConstantFace(int faceid, const Ptex::FaceInfo& info, const void* data) = 0;

    /** Close the file.  This operation can take some time if mipmaps are being generated or if there
        are many edit blocks.  If an error occurs while writing, false is returned and an error string
        is written into the error parameter. */
    virtual bool close(Ptex::String& error) = 0;

#if NEW_API
    virtual bool writeFaceReduction(int faceid, const Ptex::Res& res, const void* data, int stride=0) = 0;
    virtual bool writeConstantFaceReduction(int faceid, const Ptex::Res& res, const void* data) = 0;
#endif
};


/**
   @class PtexFilter
   @brief Interface for filtered sampling of ptex data files.

   PtexFilter instances are obtained by calling one of the particular static methods.  When finished using
   the filter, it must be returned to the library using release().

   To apply the filter to a ptex data file, use the eval() method.
 */
class PtexFilter {
 protected:
    /// Destructor not for public use.  Use release() instead.
    virtual ~PtexFilter() {}

 public:
    /// Filter types
    enum FilterType {
        f_point,                ///< Point-sampled (no filtering)
        f_bilinear,             ///< Bi-linear interpolation
        f_box,                  ///< Box filter
        f_gaussian,             ///< Gaussian filter
        f_bicubic,              ///< General bi-cubic filter (uses sharpness option)
        f_bspline,              ///< BSpline (equivalent to bi-cubic w/ sharpness=0)
        f_catmullrom,           ///< Catmull-Rom (equivalent to bi-cubic w/ sharpness=1)
        f_mitchell              ///< Mitchell (equivalent to bi-cubic w/ sharpness=2/3)
    };

    /// Choose filter options
    struct Options {
        int __structSize;       ///< (for internal use only)
        FilterType filter;      ///< Filter type.
        bool lerp;              ///< Interpolate between mipmap levels.
        float sharpness;        ///< Filter sharpness, 0..1 (for general bi-cubic filter only).
        bool noedgeblend;       ///< Disable cross-face filtering.  Useful for debugging or rendering on polys.

        /// Constructor - sets defaults
        Options(FilterType filter_=f_box, bool lerp_=0, float sharpness_=0, bool noedgeblend_=0) :
            __structSize(sizeof(Options)),
            filter(filter_), lerp(lerp_), sharpness(sharpness_), noedgeblend(noedgeblend_) {}
    };

    /* Construct a filter for the given texture.
    */
    PTEXAPI static PtexFilter* getFilter(PtexTexture* tx, const Options& opts);

    /** Release resources held by this pointer (pointer becomes invalid). */
    virtual void release() = 0;

    /** Apply filter to a ptex data file.

        The filter region is a parallelogram centered at the given
        (u,v) coordinate with sides defined by two vectors [uw1, vw1]
        and [uw2, vw2].  For an axis-aligned rectangle, the vectors
        are [uw, 0] and [0, vw].  See \link filterfootprint Filter
        Footprint \endlink for details.

        @param result Buffer to hold filter result.  Must be large enough to hold nchannels worth of data.
        @param firstchan First channel to evaluate [0..tx->numChannels()-1]
        @param nchannels Number of channels to evaluate
        @param faceid Face index [0..tx->numFaces()-1]
        @param u U coordinate, normalized [0..1]
        @param v V coordinate, normalized [0..1]
        @param uw1 U filter width 1, normalized [0..1]
        @param vw1 V filter width 1, normalized [0..1]
        @param uw2 U filter width 2, normalized [0..1]
        @param vw2 V filter width 2, normalized [0..1]
        @param width scale factor for filter width
        @param blur amount to add to filter width [0..1]
    */
    virtual void eval(float* result, int firstchan, int nchannels,
                      int faceid, float u, float v, float uw1, float vw1, float uw2, float vw2,
                      float width=1, float blur=0) = 0;
};


/**
   @class PtexPtr
   @brief Smart-pointer for acquiring and releasing API objects

   All public API objects must be released back to the Ptex library
   via the release() method.  This smart-pointer class can wrap any of
   the Ptex API objects and will automatically release the object when
   the pointer goes out of scope.  Usage of PtexPtr is optional, but
   recommended.

   Note: for efficiency and safety, PtexPtr is noncopyable.  However,
   ownership can be transferred between PtexPtr instances via the
   PtexPtr::swap member function.

   Example:
   \code
      {
          Ptex::String error;
          PtexPtr<PtexTexture> inptx(PtexTexture::open(inptxname, error));
          if (!inptx) {
              std::cerr << error << std::endl;
          }
          else {
              // read some data
              inptx->getData(faceid, buffer, stride);
          }
      }
   \endcode
 */
template <class T> class PtexPtr {
    T* _ptr;
 public:
    /// Constructor.
    PtexPtr(T* ptr=0) : _ptr(ptr) {}

    /// Destructor, calls ptr->release().
    ~PtexPtr() { if (_ptr) _ptr->release(); }

    /// Use as pointer value.
    operator T* () const { return _ptr; }

    /// Access members of pointer.
    T* operator-> () const { return _ptr; }

    /// Get pointer value.
    T* get() const { return _ptr; }

    /// Swap pointer values.
    void swap(PtexPtr& p)
    {
        T* tmp = p._ptr;
        p._ptr = _ptr;
        _ptr = tmp;
    }

    /// Deallocate object pointed to, and optionally set to new value.
    void reset(T* ptr=0) {
        if (_ptr) _ptr->release();
        _ptr = ptr;
    }

 private:
    /// Copying prohibited
    PtexPtr(const PtexPtr& p);

    /// Assignment prohibited
    void operator= (PtexPtr& p);
};

#ifndef DOXYGEN
namespace PtexUtils {}

PTEX_NAMESPACE_END

using Ptex::PtexMetaData;
using Ptex::PtexFaceData;
using Ptex::PtexTexture;
using Ptex::PtexInputHandler;
using Ptex::PtexErrorHandler;
using Ptex::PtexCache;
using Ptex::PtexWriter;
using Ptex::PtexFilter;
using Ptex::PtexPtr;
namespace PtexUtils = Ptex::PtexUtils;

#endif
#endif
