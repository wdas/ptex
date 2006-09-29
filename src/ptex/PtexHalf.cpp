#include <math.h>
#include "PtexHalf.h"


uint16_t PtexHalf::fromFloat_except(float val)
{
    uint32_t& f = (uint32_t&)val;
    uint32_t s = ((f>>16) & 0x8000);
    int32_t e = ((f>>13) & 0x3fc00) - 0x1c000;

    if (e <= 0) {
	// denormalized
	return s | int(fabs(val)*1.6777216e7 + .5);
    }

    if (e == 0x23c00)
	// inf/nan, preserve msb bits of m for nan code
	return s|0x7c00|((f&0x7fffff)>>13);
    else
	// overflow - convert to inf
	return s|0x7c00;
}


float PtexHalf::toFloat_except(uint16_t h)
{
    uint32_t s = (h & 0x8000)<<16;
    uint32_t m = h & 0x03ff;
    uint32_t e = h&0x7c00;
    if (e == 0) {
	// denormalized
	if (!(h&0x8000)) return 5.9604644775390625e-08*m;
	return -5.9604644775390625e-08*m;
    }

    // inf/nan, preserve low bits of m for nan code
    uint32_t f = s|0x7f800000|(m<<13);
    return (float&)f;
}
