#ifndef PtexHalf_h
#define PtexHalf_h

/* PtexHalf - Brent Burley, Sep 2006

   half-precision (16-bit) floating point type (following IEEE 754r).
   This type should be compatible with opengl, openexr, and the like.

   From OpenGL spec 2.1.2:
   
    A 16-bit floating-point number has a 1-bit sign (S), a 5-bit
    exponent (E), and a 10-bit mantissa (M).  The value of a 16-bit
    floating-point number is determined by the following:

        (-1)^S * 0.0,                        if E == 0 and M == 0,
        (-1)^S * 2^-14 * (M / 2^10),         if E == 0 and M != 0,
        (-1)^S * 2^(E-15) * (1 + M/2^10),    if 0 < E < 31,
        (-1)^S * INF,                        if E == 31 and M == 0, or
        NaN,                                 if E == 31 and M != 0,
*/

#include <stdint.h>

struct PtexHalf {
    uint32_t bits;
    PtexHalf() {}
    PtexHalf(float val) : bits(fromFloat(val)) {}
    operator float() const { return toFloat(bits); }
    PtexHalf& operator=(float val) { bits = fromFloat(val); return *this; }

    static float toFloat(uint16_t h)
    {
	uint32_t e = h&0x7c00;
	// normalized iff (0 < e < 31)
	if (uint32_t(e-1) < ((31<<10)-1)) {
	    uint32_t s = (h & 0x8000)<<16;
	    uint32_t m = h & 0x03ff;
	    uint32_t f = s|(((e+0x1c000)|m)<<13);
	    return (float&)f;
	}
	// denormalized, inf, or nan
	return toFloat_except(h);
    }

    static uint16_t fromFloat(float val)
    {
	uint32_t& f = (uint32_t&)val;
	int32_t e = (f & 0x7f800000) - 0x38000000;

	// normalized iff (0 < e < 31)
	if (uint32_t(e-1) < ((31<<23)-1)) {
	    uint32_t s = ((f>>16) & 0x8000);
	    uint32_t m = f & 0x7fe000;
	    // add bit 12 to round
	    return (s|((e|m)>>13))+((f>>12)&1);
	}
	// denormalized, inf, or nan
	return fromFloat_except(val);
    }

 private:
    static uint16_t fromFloat_except(float val);
    static float toFloat_except(uint16_t val);
};

#endif
