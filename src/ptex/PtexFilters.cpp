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
#include "Ptexture.h"
#include "PtexSeparableFilter.h"
#include "PtexSeparableKernel.h"
#include "PtexTriangleFilter.h"

PTEX_NAMESPACE_BEGIN

/** Point-sampling filter for rectangular textures */
class PtexPointFilter : public PtexFilter
{
 public:
    PtexPointFilter(PtexTexture* tx) : _tx(tx) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
                      int faceid, float u, float v,
                      float /*uw1*/, float /*vw1*/, float /*uw2*/, float /*vw2*/,
                      float /*width*/, float /*blur*/)
    {
        if (!_tx || nchannels <= 0) return;
        if (faceid < 0 || faceid >= _tx->numFaces()) return;
        const FaceInfo& f = _tx->getFaceInfo(faceid);
        int resu = f.res.u(), resv = f.res.v();
        int ui = PtexUtils::clamp(int(u*(float)resu), 0, resu-1);
        int vi = PtexUtils::clamp(int(v*(float)resv), 0, resv-1);
        _tx->getPixel(faceid, ui, vi, result, firstchan, nchannels);
    }

 private:
    PtexTexture* _tx;
};


/** Point-sampling filter for triangular textures */
class PtexPointFilterTri : public PtexFilter
{
 public:
    PtexPointFilterTri(PtexTexture* tx) : _tx(tx) {}
    virtual void release() { delete this; }
    virtual void eval(float* result, int firstchan, int nchannels,
                      int faceid, float u, float v,
                      float /*uw1*/, float /*vw1*/, float /*uw2*/, float /*vw2*/,
                      float /*width*/, float /*blur*/)
    {
        if (!_tx || nchannels <= 0) return;
        if (faceid < 0 || faceid >= _tx->numFaces()) return;
        const FaceInfo& f = _tx->getFaceInfo(faceid);
        int res = f.res.u();
        int resm1 = res - 1;
        float ut = u * (float)res, vt = v * (float)res;
        int ui = PtexUtils::clamp(int(ut), 0, resm1);
        int vi = PtexUtils::clamp(int(vt), 0, resm1);
        float uf = ut - (float)ui, vf = vt - (float)vi;

        if (uf + vf <= 1.0f) {
            // "even" triangles are stored in lower-left half-texture
            _tx->getPixel(faceid, ui, vi, result, firstchan, nchannels);
        }
        else {
            // "odd" triangles are stored in upper-right half-texture
            _tx->getPixel(faceid, resm1-vi, resm1-ui, result, firstchan, nchannels);
        }
    }

 private:
    PtexTexture* _tx;
};


/** Separable filter with width=4 support.

    The kernel width is calculated as a multiple of 4 times the filter
    width and the texture resolution is chosen such that each kernel
    axis has between 4 and 8.

    For kernel widths too large to handle (because the kernel would
    extend significantly beyond both sides of the face), a special
    Hermite smoothstep is used to interpolate the two nearest 2 samples
    along the affected axis (or axes).
*/
class PtexWidth4Filter : public PtexSeparableFilter
{
 public:
    typedef float KernelFn(float x, const float* c);

    PtexWidth4Filter(PtexTexture* tx, const PtexFilter::Options& opts, KernelFn k, const float* c = 0)
        : PtexSeparableFilter(tx, opts), _k(k), _c(c) {}

    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
                             Res faceRes)
    {
        buildKernelAxis(k.res.ulog2, k.u, k.uw, k.ku, u, uw, faceRes.ulog2);
        buildKernelAxis(k.res.vlog2, k.v, k.vw, k.kv, v, vw, faceRes.vlog2);
    }

 private:

    float blur(float x)
    {
        // 2-unit (x in -1..1) cubic hermite kernel
        // this produces a blur roughly 1.5 times that of the 4-unit b-spline kernel
        x = PtexUtils::abs(x);
        return x < 1.0f ? (2.0f*x-3.0f)*x*x+1.0f : 0.0f;
    }

    void buildKernelAxis(int8_t& k_ureslog2, int& k_u, int& k_uw, float* ku,
                         float u, float uw, int f_ureslog2)
    {
        // build 1 axis (note: "u" labels may repesent either u or v axis)

        // clamp filter width to no smaller than a texel
        uw = PtexUtils::max(uw, PtexUtils::reciprocalPow2(f_ureslog2));

        // compute desired texture res based on filter width
        k_ureslog2 = (int8_t)PtexUtils::calcResFromWidth(uw);
        int resu = 1 << k_ureslog2;
        float uwlo = 1.0f/(float)resu; // smallest filter width for this res

        // compute lerp weights (amount to blend towards next-lower res)
        float lerp2 = _options.lerp ? (uw-uwlo)/uwlo : 0;
        float lerp1 = 1.0f-lerp2;

        // adjust for large filter widths
        if (uw >= .25f) {
            if (uw < .5f) {
                k_ureslog2 = 2;
                float upix = u * 4.0f - 0.5f;
                int u1 = int(PtexUtils::ceil(upix - 2)), u2 = int(PtexUtils::ceil(upix + 2));
                u1 = u1 & ~1;       // round down to even pair
                u2 = (u2 + 1) & ~1; // round up to even pair
                k_u = u1;
                k_uw = u2-u1;
                float x1 = (float)u1-upix;
                for (int i = 0; i < k_uw; i+=2) {
                    float xa = x1 + (float)i, xb = xa + 1.0f, xc = (xa+xb)*0.25f;
                    // spread the filter gradually to approach the next-lower-res width
                    // at uw = .5, s = 1.0; at uw = 1, s = 0.8
                    float s = 1.0f/(uw + .75f);
                    float ka = _k(xa, _c), kb = _k(xb, _c), kc = blur(xc*s);
                    ku[i] = ka * lerp1 + kc * lerp2;
                    ku[i+1] = kb * lerp1 + kc * lerp2;
                }
                return;
            }
            else if (uw < 1) {
                k_ureslog2 = 1;
                float upix = u * 2.0f - 0.5f;
                k_u = int(PtexUtils::floor(u - .5f))*2;
                k_uw = 4;
                float x1 = (float)k_u-upix;
                for (int i = 0; i < k_uw; i+=2) {
                    float xa = x1 + (float)i, xb = xa + 1.0f, xc = (xa+xb)*0.5f;
                    // spread the filter gradually to approach the next-lower-res width
                    // at uw = .5, s = .8; at uw = 1, s = 0.5
                    float s = 1.0f/(uw*1.5f + .5f);
                    float ka = blur(xa*s), kb = blur(xb*s), kc = blur(xc*s);
                    ku[i] = ka * lerp1 + kc * lerp2;
                    ku[i+1] = kb * lerp1 + kc * lerp2;
                }
                return;
            }
            else {
                // use res 0 (1 texel per face) w/ no lerping
                // (future: use face-blended values for filter > 2)
                k_ureslog2 = 0;
                float upix = u - .5f;
                k_uw = 2;
                float ui = PtexUtils::floor(upix);
                k_u = int(ui);
                ku[0] = blur(upix-ui);
                ku[1] = 1-ku[0];
                return;
            }
        }

        // convert from normalized coords to pixel coords
        float upix = u * (float)resu - 0.5f;
        float uwpix = uw * (float)resu;

        // find integer pixel extent: [u,v] +/- [2*uw,2*vw]
        // (kernel width is 4 times filter width)
        float dupix = 2.0f*uwpix;
        int u1 = int(PtexUtils::ceil(upix - dupix)), u2 = int(PtexUtils::ceil(upix + dupix));

        if (lerp2) {
            // lerp kernel weights towards next-lower res
            // extend kernel width to cover even pairs
            u1 = u1 & ~1;
            u2 = (u2 + 1) & ~1;
            k_u = u1;
            k_uw = u2-u1;

            // compute kernel weights
            float step = 1.0f/uwpix, x1 = ((float)u1-upix)*(float)step;
            for (int i = 0; i < k_uw; i+=2) {
                float xa = x1 + (float)i*step, xb = xa + step, xc = (xa+xb)*0.5f;
                float ka = _k(xa, _c), kb = _k(xb, _c), kc = _k(xc, _c);
                ku[i] = ka * lerp1 + kc * lerp2;
                ku[i+1] = kb * lerp1 + kc * lerp2;
            }
        }
        else {
            k_u = u1;
            k_uw = u2-u1;
            // compute kernel weights
            float x1 = ((float)u1-upix)/uwpix, step = 1.0f/uwpix;
            for (int i = 0; i < k_uw; i++) ku[i] = _k(x1 + (float)i*step, _c);
        }
    }

    KernelFn* _k;               // kernel function
    const float* _c;            // kernel coefficients (if any)
};


/** Separable bicubic filter */
class PtexBicubicFilter : public PtexWidth4Filter
{
 public:
    PtexBicubicFilter(PtexTexture* tx, const PtexFilter::Options& opts, float sharpness)
        : PtexWidth4Filter(tx, opts, kernelFn, _coeffs)
    {
        // compute Cubic filter coefficients:
        // abs(x) < 1:
        //   1/6 * ((12 - 9*B - 6*C)*x^3 + (-18 + 12*B + 6*C)*x^2 + (6 - 2*B))
        //   == c[0]*x^3 + c[1]*x^2 + c[2]
        // abs(x) < 2:
        //   1/6 * ((-B - 6*C)*x^3 + (6*B + 30*C)*x^2 + (-12*B - 48*C)*x + (8*B + 24*C))
        //   == c[3]*x^3 + c[4]*x^2 + c[5]*x + c[6]
        // else: 0

        float B = 1.0f - sharpness; // choose C = (1-B)/2
        _coeffs[0] = 1.5f - B;
        _coeffs[1] = 1.5f * B - 2.5f;
        _coeffs[2] = 1.0f - float(1.0/3.0) * B;
        _coeffs[3] = float(1.0/3.0) * B - 0.5f;
        _coeffs[4] = 2.5f - 1.5f * B;
        _coeffs[5] = 2.0f * B - 4.0f;
        _coeffs[6] = 2.0f - float(2.0/3.0) * B;
    }

 private:
    static float kernelFn(float x, const float* c)
    {
        x = PtexUtils::abs(x);
        if (x < 1.0f)      return (c[0]*x + c[1])*x*x + c[2];
        else if (x < 2.0f) return ((c[3]*x + c[4])*x + c[5])*x + c[6];
        else               return 0.0f;
    }

    float _coeffs[7]; // filter coefficients for current sharpness
};



/** Separable gaussian filter */
class PtexGaussianFilter : public PtexWidth4Filter
{
 public:
    PtexGaussianFilter(PtexTexture* tx, const PtexFilter::Options& opts)
        : PtexWidth4Filter(tx, opts, kernelFn) {}

 private:
    static float kernelFn(float x, const float*)
    {
        return (float)exp(-2.0f*x*x);
    }
};



/** Rectangular box filter.
    The box is convolved with the texels as area samples and thus the kernel function is
    actually trapezoidally shaped.
 */
class PtexBoxFilter : public PtexSeparableFilter
{
 public:
    PtexBoxFilter(PtexTexture* tx, const PtexFilter::Options& opts)
        : PtexSeparableFilter(tx, opts) {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
                             Res faceRes)
    {
        // clamp filter width to no larger than 1.0
        uw = PtexUtils::min(uw, 1.0f);
        vw = PtexUtils::min(vw, 1.0f);

        // clamp filter width to no smaller than a texel
        uw = PtexUtils::max(uw, PtexUtils::reciprocalPow2(faceRes.ulog2));
        vw = PtexUtils::max(vw, PtexUtils::reciprocalPow2(faceRes.vlog2));

        // compute desired texture res based on filter width
        uint8_t ureslog2 = (uint8_t)PtexUtils::calcResFromWidth(uw);
        uint8_t vreslog2 = (uint8_t)PtexUtils::calcResFromWidth(vw);
        Res res(ureslog2, vreslog2);
        k.res = res;

        // convert from normalized coords to pixel coords
        u = u * (float)k.res.u();
        v = v * (float)k.res.v();
        uw *= (float)k.res.u();
        vw *= (float)k.res.v();

        // find integer pixel extent: [u,v] +/- [uw/2,vw/2]
        // (box is 1 unit wide for a 1 unit filter period)
        float u1 = u - 0.5f*uw, u2 = u + 0.5f*uw;
        float v1 = v - 0.5f*vw, v2 = v + 0.5f*vw;
        float u1floor = PtexUtils::floor(u1), u2ceil = PtexUtils::ceil(u2);
        float v1floor = PtexUtils::floor(v1), v2ceil = PtexUtils::ceil(v2);
        k.u = int(u1floor);
        k.v = int(v1floor);
        k.uw = int(u2ceil)-k.u;
        k.vw = int(v2ceil)-k.v;

        // compute kernel weights along u and v directions
        computeWeights(k.ku, k.uw, 1.0f-(u1-u1floor), 1.0f-(u2ceil-u2));
        computeWeights(k.kv, k.vw, 1.0f-(v1-v1floor), 1.0f-(v2ceil-v2));
    }

 private:
    void computeWeights(float* kernel, int size, float f1, float f2)
    {
        assert(size >= 1 && size <= 3);

        if (size == 1) {
            kernel[0] = f1 + f2 - 1.0f;
        }
        else {
            kernel[0] = f1;
            for (int i = 1; i < size-1; i++) kernel[i] = 1.0f;
            kernel[size-1] = f2;
        }
    }
};


/** Bilinear filter (for rectangular textures) */
class PtexBilinearFilter : public PtexSeparableFilter
{
 public:
    PtexBilinearFilter(PtexTexture* tx, const PtexFilter::Options& opts)
        : PtexSeparableFilter(tx, opts) {}

 protected:
    virtual void buildKernel(PtexSeparableKernel& k, float u, float v, float uw, float vw,
                             Res faceRes)
    {
        // clamp filter width to no larger than 1.0
        uw = PtexUtils::min(uw, 1.0f);
        vw = PtexUtils::min(vw, 1.0f);

        // clamp filter width to no smaller than a texel
        uw = PtexUtils::max(uw, PtexUtils::reciprocalPow2(faceRes.ulog2));
        vw = PtexUtils::max(vw, PtexUtils::reciprocalPow2(faceRes.vlog2));

        uint8_t ureslog2 = (uint8_t)PtexUtils::calcResFromWidth(uw);
        uint8_t vreslog2 = (uint8_t)PtexUtils::calcResFromWidth(vw);
        Res res(ureslog2, vreslog2);
        k.res = res;

        // convert from normalized coords to pixel coords
        float upix = u * (float)k.res.u() - 0.5f;
        float vpix = v * (float)k.res.v() - 0.5f;

        float ufloor = PtexUtils::floor(upix);
        float vfloor = PtexUtils::floor(vpix);
        k.u = int(ufloor);
        k.v = int(vfloor);
        k.uw = 2;
        k.vw = 2;

        // compute kernel weights
        float ufrac = upix-ufloor, vfrac = vpix-vfloor;
        k.ku[0] = 1.0f - ufrac;
        k.ku[1] = ufrac;
        k.kv[0] = 1.0f - vfrac;
        k.kv[1] = vfrac;
    }
};


PtexFilter* PtexFilter::getFilter(PtexTexture* tex, const PtexFilter::Options& opts)
{
    switch (tex->meshType()) {
    case Ptex::mt_quad:
        switch (opts.filter) {
        case f_point:       return new PtexPointFilter(tex);
        case f_bilinear:    return new PtexBilinearFilter(tex, opts);
        default:
        case f_box:         return new PtexBoxFilter(tex, opts);
        case f_gaussian:    return new PtexGaussianFilter(tex, opts);
        case f_bicubic:     return new PtexBicubicFilter(tex, opts, opts.sharpness);
        case f_bspline:     return new PtexBicubicFilter(tex, opts, 0.f);
        case f_catmullrom:  return new PtexBicubicFilter(tex, opts, 1.f);
        case f_mitchell:    return new PtexBicubicFilter(tex, opts, 2.f/3.f);
        }
        break;

    case Ptex::mt_triangle:
        switch (opts.filter) {
        case f_point:       return new PtexPointFilterTri(tex);
        default:            return new PtexTriangleFilter(tex, opts);
        }
        break;
    }
    return 0;
}

PTEX_NAMESPACE_END
