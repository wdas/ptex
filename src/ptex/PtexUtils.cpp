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

#include "PtexPlatform.h"
#include <algorithm>
#include <vector>
#include <stdlib.h>
#include <string.h>

#include "PtexHalf.h"
#include "PtexUtils.h"


PTEX_NAMESPACE_BEGIN

const char* MeshTypeName(MeshType mt)
{
    static const char* names[] = { "triangle", "quad" };
    const int mtype = static_cast<int>(mt);
    if (mtype < 0 || mtype >= int(sizeof(names)/sizeof(const char*)))
        return "(invalid mesh type)";
    return names[mtype];
}


const char* DataTypeName(DataType dt)
{
    static const char* names[] = { "uint8", "uint16", "float16", "float32" };
    const int dtype = static_cast<int>(dt);
    if (dtype < 0 || dtype >= int(sizeof(names)/sizeof(const char*)))
        return "(invalid data type)";
    return names[dtype];
}


const char* BorderModeName(BorderMode m)
{
    static const char* names[] = { "clamp", "black", "periodic" };
    const int mode = static_cast<int>(m);
    if (mode < 0 || mode >= int(sizeof(names)/sizeof(const char*)))
        return "(invalid border mode)";
    return names[mode];
}

const char* EdgeFilterModeName(EdgeFilterMode m)
{
    static const char* names[] = { "none", "tanvec" };
    const int mode = static_cast<int>(m);
    if (mode < 0 || mode >= int(sizeof(names)/sizeof(const char*)))
        return "(invalid edge filter mode)";
    return names[mode];
}


const char* EdgeIdName(EdgeId eid)
{
    static const char* names[] = { "bottom", "right", "top", "left" };
    const int edgeid = static_cast<int>(eid);
    if (edgeid < 0 || edgeid >= int(sizeof(names)/sizeof(const char*)))
        return "(invalid edge id)";
    return names[edgeid];
}


const char* MetaDataTypeName(MetaDataType mdt)
{
    static const char* names[] = { "string", "int8", "int16", "int32", "float", "double" };
    const int mdtype = static_cast<int>(mdt);
    if (mdtype < 0 || mdtype >= int(sizeof(names)/sizeof(const char*)))
        return "(invalid meta data type)";
    return names[mdtype];
}


namespace {
    template<typename DST, typename SRC>
    void ConvertArrayClamped(DST* dst, SRC* src, int numChannels, float scale, float round=0)
    {
        for (int i = 0; i < numChannels; i++)
            dst[i] = DST(PtexUtils::clamp(src[i], 0.0f, 1.0f) * scale + round);
    }

    template<typename DST, typename SRC>
    void ConvertArray(DST* dst, SRC* src, int numChannels, float scale, float round=0)
    {
        for (int i = 0; i < numChannels; i++)
                dst[i] = DST((float)src[i] * scale + round);
    }
}

void ConvertToFloat(float* dst, const void* src, DataType dt, int numChannels)
{
    switch (dt) {
    case dt_uint8:  ConvertArray(dst, static_cast<const uint8_t*>(src),  numChannels, 1.f/255.f); break;
    case dt_uint16: ConvertArray(dst, static_cast<const uint16_t*>(src), numChannels, 1.f/65535.f); break;
    case dt_half:   ConvertArray(dst, static_cast<const PtexHalf*>(src), numChannels, 1.f); break;
    case dt_float:  memcpy(dst, src, sizeof(float)*numChannels); break;
    }
}


void ConvertFromFloat(void* dst, const float* src, DataType dt, int numChannels)
{
    switch (dt) {
    case dt_uint8:  ConvertArrayClamped(static_cast<uint8_t*>(dst),  src, numChannels, 255.0, 0.5); break;
    case dt_uint16: ConvertArrayClamped(static_cast<uint16_t*>(dst), src, numChannels, 65535.0, 0.5); break;
    case dt_half:   ConvertArray(static_cast<PtexHalf*>(dst), src, numChannels, 1.0); break;
    case dt_float:  memcpy(dst, src, sizeof(float)*numChannels); break;
    }
}


namespace PtexUtils {

bool isConstant(const void* data, int stride, int ures, int vres,
                int pixelSize)
{
    int rowlen = pixelSize * ures;
    const char* p = (const char*) data + stride;

    // compare each row with the first
    for (int i = 1; i < vres; i++, p += stride)
        if (0 != memcmp(data, p, rowlen)) return 0;

    // make sure first row is constant
    p = (const char*) data + pixelSize;
    for (int i = 1; i < ures; i++, p += pixelSize)
        if (0 != memcmp(data, p, pixelSize)) return 0;

    return 1;
}


namespace {
    template<typename T>
    inline void interleave(const T* src, int sstride, int uw, int vw,
                           T* dst, int dstride, int nchan)
    {
        sstride /= (int)sizeof(T);
        dstride /= (int)sizeof(T);
        // for each channel
        for (T* dstend = dst + nchan; dst != dstend; dst++) {
            // for each row
            T* drow = dst;
            for (const T* rowend = src + sstride*vw; src != rowend;
                 src += sstride, drow += dstride) {
                // copy each pixel across the row
                T* dp = drow;
                for (const T* sp = src, * end = sp + uw; sp != end; dp += nchan)
                    *dp = *sp++;
            }
        }
    }
}


void interleave(const void* src, int sstride, int uw, int vw,
                void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:     interleave((const uint8_t*) src, sstride, uw, vw,
                                  (uint8_t*) dst, dstride, nchan); break;
    case dt_half:
    case dt_uint16:    interleave((const uint16_t*) src, sstride, uw, vw,
                                  (uint16_t*) dst, dstride, nchan); break;
    case dt_float:     interleave((const float*) src, sstride, uw, vw,
                                  (float*) dst, dstride, nchan); break;
    }
}

namespace {
    template<typename T>
    inline void deinterleave(const T* src, int sstride, int uw, int vw,
                             T* dst, int dstride, int nchan)
    {
        sstride /= (int)sizeof(T);
        dstride /= (int)sizeof(T);
        // for each channel
        for (const T* srcend = src + nchan; src != srcend; src++) {
            // for each row
            const T* srow = src;
            for (const T* rowend = srow + sstride*vw; srow != rowend;
                 srow += sstride, dst += dstride) {
                // copy each pixel across the row
                const T* sp = srow;
                for (T* dp = dst, * end = dp + uw; dp != end; sp += nchan)
                    *dp++ = *sp;
            }
        }
    }
}


void deinterleave(const void* src, int sstride, int uw, int vw,
                  void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:     deinterleave((const uint8_t*) src, sstride, uw, vw,
                                    (uint8_t*) dst, dstride, nchan); break;
    case dt_half:
    case dt_uint16:    deinterleave((const uint16_t*) src, sstride, uw, vw,
                                    (uint16_t*) dst, dstride, nchan); break;
    case dt_float:     deinterleave((const float*) src, sstride, uw, vw,
                                    (float*) dst, dstride, nchan); break;
    }
}


namespace {
    template<typename T>
    void encodeDifference(T* data, int size)
    {
        size /= (int)sizeof(T);
        T* p = static_cast<T*>(data), * end = p + size, tmp, prev = 0;
        while (p != end) { tmp = prev; prev = *p; *p = T(*p - tmp); p++; }
    }
}

void encodeDifference(void* data, int size, DataType dt)
{
    switch (dt) {
    case dt_uint8:    encodeDifference(static_cast<uint8_t*>(data), size); break;
    case dt_uint16:   encodeDifference(static_cast<uint16_t*>(data), size); break;
    default: break; // skip other types
    }
}


namespace {
    template<typename T>
    void decodeDifference(T* data, int size)
    {
        size /= (int)sizeof(T);
        T* p = static_cast<T*>(data), * end = p + size, prev = 0;
        while (p != end) { *p = T(*p + prev); prev = *p++; }
    }
}

void decodeDifference(void* data, int size, DataType dt)
{
    switch (dt) {
    case dt_uint8:    decodeDifference(static_cast<uint8_t*>(data), size); break;
    case dt_uint16:   decodeDifference(static_cast<uint16_t*>(data), size); break;
    default: break; // skip other types
    }
}


namespace {
    template<typename T>
    inline void reduce(const T* src, int sstride, int uw, int vw,
                       T* dst, int dstride, int nchan)
    {
        sstride /= (int)sizeof(T);
        dstride /= (int)sizeof(T);
        int rowlen = uw*nchan;
        int srowskip = 2*sstride - rowlen;
        int drowskip = dstride - rowlen/2;
        for (const T* end = src + vw*sstride; src != end;
             src += srowskip, dst += drowskip)
            for (const T* rowend = src + rowlen; src != rowend; src += nchan)
                for (const T* pixend = src+nchan; src != pixend; src++)
                    *dst++ = T(quarter(src[0] + src[nchan] + src[sstride] + src[sstride+nchan]));
    }
}

void reduce(const void* src, int sstride, int uw, int vw,
            void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:     reduce(static_cast<const uint8_t*>(src), sstride, uw, vw,
                              static_cast<uint8_t*>(dst), dstride, nchan); break;
    case dt_half:      reduce(static_cast<const PtexHalf*>(src), sstride, uw, vw,
                              static_cast<PtexHalf*>(dst), dstride, nchan); break;
    case dt_uint16:    reduce(static_cast<const uint16_t*>(src), sstride, uw, vw,
                              static_cast<uint16_t*>(dst), dstride, nchan); break;
    case dt_float:     reduce(static_cast<const float*>(src), sstride, uw, vw,
                              static_cast<float*>(dst), dstride, nchan); break;
    }
}


namespace {
    template<typename T>
    inline void reduceu(const T* src, int sstride, int uw, int vw,
                        T* dst, int dstride, int nchan)
    {
        sstride /= (int)sizeof(T);
        dstride /= (int)sizeof(T);
        int rowlen = uw*nchan;
        int srowskip = sstride - rowlen;
        int drowskip = dstride - rowlen/2;
        for (const T* end = src + vw*sstride; src != end;
             src += srowskip, dst += drowskip)
            for (const T* rowend = src + rowlen; src != rowend; src += nchan)
                for (const T* pixend = src+nchan; src != pixend; src++)
                    *dst++ = T(halve(src[0] + src[nchan]));
    }
}

void reduceu(const void* src, int sstride, int uw, int vw,
             void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:     reduceu(static_cast<const uint8_t*>(src), sstride, uw, vw,
                               static_cast<uint8_t*>(dst), dstride, nchan); break;
    case dt_half:      reduceu(static_cast<const PtexHalf*>(src), sstride, uw, vw,
                               static_cast<PtexHalf*>(dst), dstride, nchan); break;
    case dt_uint16:    reduceu(static_cast<const uint16_t*>(src), sstride, uw, vw,
                               static_cast<uint16_t*>(dst), dstride, nchan); break;
    case dt_float:     reduceu(static_cast<const float*>(src), sstride, uw, vw,
                               static_cast<float*>(dst), dstride, nchan); break;
    }
}


namespace {
    template<typename T>
    inline void reducev(const T* src, int sstride, int uw, int vw,
                        T* dst, int dstride, int nchan)
    {
        sstride /= (int)sizeof(T);
        dstride /= (int)sizeof(T);
        int rowlen = uw*nchan;
        int srowskip = 2*sstride - rowlen;
        int drowskip = dstride - rowlen;
        for (const T* end = src + vw*sstride; src != end;
             src += srowskip, dst += drowskip)
            for (const T* rowend = src + rowlen; src != rowend; src++)
                *dst++ = T(halve(src[0] + src[sstride]));
    }
}

void reducev(const void* src, int sstride, int uw, int vw,
             void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:     reducev(static_cast<const uint8_t*>(src), sstride, uw, vw,
                               static_cast<uint8_t*>(dst), dstride, nchan); break;
    case dt_half:      reducev(static_cast<const PtexHalf*>(src), sstride, uw, vw,
                               static_cast<PtexHalf*>(dst), dstride, nchan); break;
    case dt_uint16:    reducev(static_cast<const uint16_t*>(src), sstride, uw, vw,
                               static_cast<uint16_t*>(dst), dstride, nchan); break;
    case dt_float:     reducev(static_cast<const float*>(src), sstride, uw, vw,
                               static_cast<float*>(dst), dstride, nchan); break;
    }
}



namespace {
    // generate a reduction of a packed-triangle texture
    // note: this method won't work for tiled textures
    template<typename T>
    inline void reduceTri(const T* src, int sstride, int w, int /*vw*/,
                          T* dst, int dstride, int nchan)
    {
        sstride /= (int)sizeof(T);
        dstride /= (int)sizeof(T);
        int rowlen = w*nchan;
        const T* src2 = src + (w-1) * sstride + rowlen - nchan;
        int srowinc2 = -2*sstride - nchan;
        int srowskip = 2*sstride - rowlen;
        int srowskip2 = w*sstride - 2 * nchan;
        int drowskip = dstride - rowlen/2;
        for (const T* end = src + w*sstride; src != end;
             src += srowskip, src2 += srowskip2, dst += drowskip)
            for (const T* rowend = src + rowlen; src != rowend; src += nchan, src2 += srowinc2)
                for (const T* pixend = src+nchan; src != pixend; src++, src2++)
                    *dst++ = T(quarter(src[0] + src[nchan] + src[sstride] + src2[0]));
    }
}

void reduceTri(const void* src, int sstride, int w, int /*vw*/,
               void* dst, int dstride, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:     reduceTri(static_cast<const uint8_t*>(src), sstride, w, 0,
                                 static_cast<uint8_t*>(dst), dstride, nchan); break;
    case dt_half:      reduceTri(static_cast<const PtexHalf*>(src), sstride, w, 0,
                                 static_cast<PtexHalf*>(dst), dstride, nchan); break;
    case dt_uint16:    reduceTri(static_cast<const uint16_t*>(src), sstride, w, 0,
                                 static_cast<uint16_t*>(dst), dstride, nchan); break;
    case dt_float:     reduceTri(static_cast<const float*>(src), sstride, w, 0,
                                 static_cast<float*>(dst), dstride, nchan); break;
    }
}


void fill(const void* src, void* dst, int dstride,
          int ures, int vres, int pixelsize)
{
    // fill first row
    int rowlen = ures*pixelsize;
    char* ptr = (char*) dst;
    char* end = ptr + rowlen;
    for (; ptr != end; ptr += pixelsize) memcpy(ptr, src, pixelsize);

    // fill remaining rows from first row
    ptr = (char*) dst + dstride;
    end = (char*) dst + vres*dstride;
    for (; ptr != end; ptr += dstride) memcpy(ptr, dst, rowlen);
}


void copy(const void* src, int sstride, void* dst, int dstride,
          int vres, int rowlen)
{
    // regular non-tiled case
    if (sstride == rowlen && dstride == rowlen) {
        // packed case - copy in single block
        memcpy(dst, src, vres*rowlen);
    } else {
        // copy a row at a time
        const char* sptr = (const char*) src;
        char* dptr = (char*) dst;
        for (const char* end = sptr + vres*sstride; sptr != end;) {
            memcpy(dptr, sptr, rowlen);
            dptr += dstride;
            sptr += sstride;
        }
    }
}


namespace {
    template<typename T>
    inline void blend(const T* src, float weight, T* dst, int rowlen, int nchan)
    {
        for (const T* end = src + rowlen * nchan; src != end; dst++)
            *dst = T(*dst + T(weight * (float)*src++));
    }

    template<typename T>
    inline void blendflip(const T* src, float weight, T* dst, int rowlen, int nchan)
    {
        dst += (rowlen-1) * nchan;
        for (const T* end = src + rowlen * nchan; src != end;) {
            for (int i = 0; i < nchan; i++, dst++) {
                *dst = T(*dst + T(weight * (float)*src++));
            }
            dst -= nchan*2;
        }
    }
}


void blend(const void* src, float weight, void* dst, bool flip,
           int rowlen, DataType dt, int nchan)
{
    switch ((dt<<1) | int(flip)) {
    case (dt_uint8<<1):        blend(static_cast<const uint8_t*>(src), weight,
                                     static_cast<uint8_t*>(dst), rowlen, nchan); break;
    case (dt_uint8<<1 | 1):    blendflip(static_cast<const uint8_t*>(src), weight,
                                         static_cast<uint8_t*>(dst), rowlen, nchan); break;
    case (dt_half<<1):         blend(static_cast<const PtexHalf*>(src), weight,
                                     static_cast<PtexHalf*>(dst), rowlen, nchan); break;
    case (dt_half<<1 | 1):     blendflip(static_cast<const PtexHalf*>(src), weight,
                                         static_cast<PtexHalf*>(dst), rowlen, nchan); break;
    case (dt_uint16<<1):       blend(static_cast<const uint16_t*>(src), weight,
                                     static_cast<uint16_t*>(dst), rowlen, nchan); break;
    case (dt_uint16<<1 | 1):   blendflip(static_cast<const uint16_t*>(src), weight,
                                         static_cast<uint16_t*>(dst), rowlen, nchan); break;
    case (dt_float<<1):        blend(static_cast<const float*>(src), weight,
                                     static_cast<float*>(dst), rowlen, nchan); break;
    case (dt_float<<1 | 1):    blendflip(static_cast<const float*>(src), weight,
                                         static_cast<float*>(dst), rowlen, nchan); break;
    }
}


namespace {
    template<typename T>
    inline void average(const T* src, int sstride, int uw, int vw,
                        T* dst, int nchan)
    {
        float* buff = (float*) alloca(nchan*sizeof(float));
        memset(buff, 0, nchan*sizeof(float));
        sstride /= (int)sizeof(T);
        int rowlen = uw*nchan;
        int rowskip = sstride - rowlen;
        for (const T* end = src + vw*sstride; src != end; src += rowskip)
            for (const T* rowend = src + rowlen; src != rowend;)
                for (int i = 0; i < nchan; i++) buff[i] += (float)*src++;
        float scale = 1.0f/(float)(uw*vw);
        for (int i = 0; i < nchan; i++) dst[i] = T(buff[i]*scale);
    }
}

void average(const void* src, int sstride, int uw, int vw,
             void* dst, DataType dt, int nchan)
{
    switch (dt) {
    case dt_uint8:     average(static_cast<const uint8_t*>(src), sstride, uw, vw,
                               static_cast<uint8_t*>(dst), nchan); break;
    case dt_half:      average(static_cast<const PtexHalf*>(src), sstride, uw, vw,
                               static_cast<PtexHalf*>(dst), nchan); break;
    case dt_uint16:    average(static_cast<const uint16_t*>(src), sstride, uw, vw,
                               static_cast<uint16_t*>(dst), nchan); break;
    case dt_float:     average(static_cast<const float*>(src), sstride, uw, vw,
                               static_cast<float*>(dst), nchan); break;
    }
}


namespace {
    struct CompareRfaceIds {
        const FaceInfo* faces;
        CompareRfaceIds(const FaceInfo* facesArg) : faces(facesArg) {}
        bool operator() (uint32_t faceid1, uint32_t faceid2)
        {
            const Ptex::FaceInfo& f1 = faces[faceid1];
            const Ptex::FaceInfo& f2 = faces[faceid2];
            int min1 = f1.isConstant() ? 1 : PtexUtils::min(f1.res.ulog2, f1.res.vlog2);
            int min2 = f2.isConstant() ? 1 : PtexUtils::min(f2.res.ulog2, f2.res.vlog2);
            return min1 > min2;
        }
    };
}


namespace {
    template<typename T>
    inline void multalpha(T* data, int npixels, int nchannels, int alphachan, float scale)
    {
        int alphaoffset; // offset to alpha chan from data ptr
        int nchanmult;   // number of channels to alpha-multiply
        if (alphachan == 0) {
            // first channel is alpha chan: mult the rest of the channels
            data++;
            alphaoffset = -1;
            nchanmult = nchannels - 1;
        }
        else {
            // mult all channels up to alpha chan
            alphaoffset = alphachan;
            nchanmult = alphachan;
        }

        for (T* end = data + npixels*nchannels; data != end; data += nchannels) {
            float aval = scale * (float)data[alphaoffset];
            for (int i = 0; i < nchanmult; i++) data[i] = T((float)data[i] * aval);
        }
    }
}

void multalpha(void* data, int npixels, DataType dt, int nchannels, int alphachan)
{
    float scale = OneValueInv(dt);
    switch(dt) {
    case dt_uint8:    multalpha(static_cast<uint8_t*>(data), npixels, nchannels, alphachan, scale); break;
    case dt_uint16:   multalpha(static_cast<uint16_t*>(data), npixels, nchannels, alphachan, scale); break;
    case dt_half:     multalpha(static_cast<PtexHalf*>(data), npixels, nchannels, alphachan, scale); break;
    case dt_float:    multalpha(static_cast<float*>(data), npixels, nchannels, alphachan, scale); break;
    }
}


namespace {
    template<typename T>
    inline void divalpha(T* data, int npixels, int nchannels, int alphachan, float scale)
    {
        int alphaoffset; // offset to alpha chan from data ptr
        int nchandiv;    // number of channels to alpha-divide
        if (alphachan == 0) {
            // first channel is alpha chan: div the rest of the channels
            data++;
            alphaoffset = -1;
            nchandiv = nchannels - 1;
        }
        else {
            // div all channels up to alpha chan
            alphaoffset = alphachan;
            nchandiv = alphachan;
        }

        for (T* end = data + npixels*nchannels; data != end; data += nchannels) {
            T alpha = data[alphaoffset];
            if (!alpha) continue; // don't divide by zero!
            float aval = scale / (float)alpha;
            for (int i = 0; i < nchandiv; i++)  data[i] = T((float)data[i] * aval);
        }
    }
}

void divalpha(void* data, int npixels, DataType dt, int nchannels, int alphachan)
{
    float scale = OneValue(dt);
    switch(dt) {
    case dt_uint8:    divalpha(static_cast<uint8_t*>(data), npixels, nchannels, alphachan, scale); break;
    case dt_uint16:   divalpha(static_cast<uint16_t*>(data), npixels, nchannels, alphachan, scale); break;
    case dt_half:     divalpha(static_cast<PtexHalf*>(data), npixels, nchannels, alphachan, scale); break;
    case dt_float:    divalpha(static_cast<float*>(data), npixels, nchannels, alphachan, scale); break;
    }
}


void genRfaceids(const FaceInfo* faces, int nfaces,
                 uint32_t* rfaceids, uint32_t* faceids)
{
    // stable_sort faceids by smaller dimension (u or v) in descending order
    // treat const faces as having res of 1

    // init faceids
    for (int i = 0; i < nfaces; i++) faceids[i] = i;

    // sort faceids by rfaceid
    std::stable_sort(faceids, faceids + nfaces, CompareRfaceIds(faces));

    // generate mapping from faceid to rfaceid
    for (int i = 0; i < nfaces; i++) {
        // note: i is the rfaceid
        rfaceids[faceids[i]] = i;
    }
}

namespace {
    // apply to 1..4 channels, unrolled
    template<class T, int nChan>
    void ApplyConst(float weight, float* dst, void* data, int /*nChan*/)
    {
        // dst[i] += data[i] * weight for i in {0..n-1}
        VecAccum<T,nChan>()(dst, static_cast<T*>(data), weight);
    }

    // apply to N channels (general case)
    template<class T>
    void ApplyConstN(float weight, float* dst, void* data, int nChan)
    {
        // dst[i] += data[i] * weight for i in {0..n-1}
        VecAccumN<T>()(dst, static_cast<T*>(data), nChan, weight);
    }
}

ApplyConstFn
applyConstFunctions[20] = {
    ApplyConstN<uint8_t>,  ApplyConstN<uint16_t>,  ApplyConstN<PtexHalf>,  ApplyConstN<float>,
    ApplyConst<uint8_t,1>, ApplyConst<uint16_t,1>, ApplyConst<PtexHalf,1>, ApplyConst<float,1>,
    ApplyConst<uint8_t,2>, ApplyConst<uint16_t,2>, ApplyConst<PtexHalf,2>, ApplyConst<float,2>,
    ApplyConst<uint8_t,3>, ApplyConst<uint16_t,3>, ApplyConst<PtexHalf,3>, ApplyConst<float,3>,
    ApplyConst<uint8_t,4>, ApplyConst<uint16_t,4>, ApplyConst<PtexHalf,4>, ApplyConst<float,4>,
};

} // namespace PtexUtils end

#ifndef PTEX_USE_STDSTRING
String::~String()
{
    if (_str) free(_str);
}


String& String::operator=(const char* str)
{
    if (_str) free(_str);
    _str = str ? strdup(str) : 0;
    return *this;
}

std::ostream& operator << (std::ostream& stream, const String& str)
{
    stream << str.c_str();
    return stream;
}

#endif

PTEX_NAMESPACE_END
