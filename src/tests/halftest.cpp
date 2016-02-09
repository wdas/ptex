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

/* test code for PtexHalf class
   
   Can also be used to test OpenEXR's half class, compile with -DOPEN_EXR.
*/

#include <math.h>
#include <stdio.h>
#include <iostream>
#include <string.h>

#if defined(_WIN32) || defined(_WINDOWS) || defined(_MSC_VER)
#include <float.h>
#define isfinite _finite
#else
#include <stdint.h>
#endif

#ifdef OPEN_EXR

#include "half.h"
float h2f(uint16_t h)
{
    half hval; hval.setBits(h);
    return hval;
}
uint16_t f2h(float f)
{
    half hval(f);
    return hval.bits();
}
typedef half H;

#else

#include "PtexHalf.h"
using namespace Ptex;

float h2f(uint16_t h)
{
    return PtexHalf::toFloat(h);
}


uint16_t f2h(float f)
{
    return PtexHalf::fromFloat(f);
}
typedef PtexHalf H;

#endif

static uint32_t floatToBits(float f)
{
    union { uint32_t i; float f; } u;
    u.f = f; return u.i;
}

static float bitsToFloat(uint32_t bits)
{
    union { uint32_t i; float f; } u;
    u.i = bits; return u.f;
}

void printbits(int t, int s, int e)
{
    for (int i = s; i >= e; i--)
        printf("%c", t&(1<<i) ? '1' : '0');
    printf(" ");
}

void printhalf(uint16_t t)
{
    printbits(t, 15, 15);
    printf("   "); printbits(t, 14, 10);
    printbits(t, 9, 0);
}

void printfloat(float f)
{
    int32_t t = floatToBits(f);
    printbits(t, 31, 31);
    printbits(t, 30, 23);
    printbits(t, 22, 0);
}

int testconvert(int i)
{
    float f = h2f(i);
    int i2 = f2h(f);
    if (i != i2) {
        printf("error: 0x%x -> %g -> 0x%x\n", i, f, i2);
        return 1;
    }
    return 0;
}

int testconvertall()
{
    int count = 0;
    // just do finite values (skip inf/nan)
    for (int i = 0x0000; i < 0x7c00; i++) count += testconvert(i);
    for (int i = 0x8001; i < 0xfc00; i++) count += testconvert(i);
    return count;
}

int testround(float val)
{
    int i = f2h(val);
    float f = fabs(h2f(i)-val);
    float f1 = fabs(h2f(i-1)-val);
    float f2 = fabs(h2f(i+1)-val);
    if (f1 < f) {
        printf("error: %g->0x%x->%g, expected ->0x%x->%g\n",
               val, i, h2f(i), i-1, h2f(i-1));
        return 1;
    }
    if (f2 < f) {
        printf("error: %g->0x%x->%g, expected ->0x%x->%g\n",
               val, i, h2f(i), i+1, h2f(i+1));
        return 1;
    }
    return 0;
}


int testroundrange(int inc)
{
    int count = 0;
    // check all legal float32 values within legal float16 range
    float f1 = 2.9802320611338473e-08; // min float16
    float f2 = 65519; // max float 16
    unsigned int i1 = floatToBits(f1);
    unsigned int i2 = floatToBits(f2);
    for (unsigned int i = i1; i < i2; i+=inc) count += testround(bitsToFloat(i));

    // and the negatives
    f1 = -2.9802320611338473e-08; // min float16
    f2 = -65519; // max float 16
    i1 = floatToBits(f1);
    i2 = floatToBits(f2);
    for (unsigned int i = i1; i < i2; i+=inc) count += testround(bitsToFloat(i));
    return count;
}


int testroundall()
{
    // can be slow
    return testroundrange(1);
}

int testroundsome()
{
    // fairly dense sampling is still a good test
    return testroundrange(97);
}

int compatcheck()
{
    H h = 1.5;
    H h2 = 2.5;
    h = h + h2;
    float f = h;
    if (f != 4) return 1;
    double d = h;
    if (d != 4) return 1;
    h = d * 2;
    if (float(h) != 8) return 1;
    return 0;
}


int spotcheck(int i, float f)
{
    float f2 = h2f(i);
    if (fabs((f-f2)/f) > 1e-6) {
        printf("error: 0x%x->%.7g, expected ->%.7g, \n",
               i, f2, f);
        return 1;
    }
    return 0;
}

int spotcheckall()
{
    static float t[] = {
        1, 5.960464e-08,
        3, 1.788139e-07,
        8, 4.768372e-07,
        16, 9.536743e-07,
        33, 1.966953e-06,
        83, 4.947186e-06,
        167, 9.953976e-06,
        335, 1.996756e-05,
        838, 4.994869e-05,
        1677, 9.995699e-05,
        2701, 0.000199914,
        4120, 0.0004997253,
        5144, 0.0009994507,
        6168, 0.001998901,
        7454, 0.004997253,
        8478, 0.009994507,
        9502, 0.01998901,
        10854, 0.04998779,
        11878, 0.09997559,
        12902, 0.1999512,
        14336, 0.5,
        15360, 1,
        16384, 2,
        17664, 5,
        18688, 10,
        19712, 20,
        21056, 50,
        22080, 100,
        23104, 200,
        24528, 500,
        25552, 1000,
        26576, 2000,
        27874, 5000,
        28898, 10000,
        29922, 20000,
        31258, 49984,
        32769, -5.960464e-08,
        32771, -1.788139e-07,
        32776, -4.768372e-07,
        32784, -9.536743e-07,
        32801, -1.966953e-06,
        32851, -4.947186e-06,
        32935, -9.953976e-06,
        33103, -1.996756e-05,
        33606, -4.994869e-05,
        34445, -9.995699e-05,
        35469, -0.000199914,
        36888, -0.0004997253,
        37912, -0.0009994507,
        38936, -0.001998901,
        40222, -0.004997253,
        41246, -0.009994507,
        42270, -0.01998901,
        43622, -0.04998779,
        44646, -0.09997559,
        45670, -0.1999512,
        47104, -0.5,
        48128, -1,
        49152, -2,
        50432, -5,
        51456, -10,
        52480, -20,
        53824, -50,
        54848, -100,
        55872, -200,
        57296, -500,
        58320, -1000,
        59344, -2000,
        60642, -5000,
        61666, -10000,
        62690, -20000,
        64026, -49984,
    };

    int count = 0;
    for (unsigned int i = 0; i < (sizeof(t)/sizeof(float)); i+=2) 
        count += spotcheck(int(t[i]), t[i+1]);
    return count;
}


int excheck(uint32_t val)
{
    float f = bitsToFloat(val);
    int i = f2h(f);
    float f2 = h2f(i);
    if (memcmp(&f, &f2, 4)) {
        printf("error: %g(0x%0x)->0x%x->%g(0x%0x)\n", 
               f, floatToBits(f), i, f2, floatToBits(f2));
        return 1;
    }
    return 0;
}


int infcheck()
{
    return excheck(0x7f800000) + excheck(0xff800000);
}


int nancheck()
{
    int count = 0;
    int nan = 0x7fc00000;
    for (int i = 0; i < (1<<9); i++)
        if (excheck(nan | (i<<13))) count++;
    return count;
}


int overflowtest(float f)
{
    uint32_t fi = floatToBits(f);
    int i = f2h(f);
    int e = 0x7c00 | ((fi>>16)&0x8000);
    if (i != e) {
        printf("error: %g->0x%x->%g, expected 0x%x->%sinf\n",
               f, i, h2f(i), e, (e&0x8000) ? "-" : "");
        return 1;
    }
    return 0;
}

int overflowtestall()
{
    return overflowtest(65520) + overflowtest(-65520);
}


int test(const char* name, int (*fn)())
{
    printf("%s...\n", name);
    int count = fn();
    printf("%s %s\n\n", name, count ? "failed" : "passed");
    return count;
}


int testall(bool fulltest)
{
    int total = 0;
    total += test("Float/double compatibility", compatcheck);
    total += test("Spot checks", spotcheckall);
    total += test("Bidirectional conversion", testconvertall);
    total += test("Infinity conversion", infcheck);
    total += test("Nan conversion", nancheck);
    total += test("Overflow", overflowtestall);
    total += test("Rounding", fulltest ? testroundall : testroundsome);
    if (!total)
        printf("halftest: all tests passed.\n");
    else
        printf("halftest: total errors: %d\n", total);
    return total;
}


void f2htimingtest()
{
    int total = 0;
    float f[65536];
    for (int i = 0; i < 65536; i++) {
        f[i] = h2f(i);
        if (!isfinite(f[i])) f[i] = 1;
    }
    for (int j = 0; j < 30*1024; j++) {
        for (int i = 1024; i < 31740; i++) total += f2h(f[i]);
    }
    printf("%d\n", total);
}


void h2ftimingtest()
{
    float total = 0;
    for (int j = 0; j < 30*1024; j++) {
        for (int i = 1024; i < 31740; i++) total += h2f(i);
    }
    printf("%g\n", total);
}


void printall()
{
    for (int i = 0; i < 65536; i++) {
        float f = h2f(i);
        printf("0x%x -> %g 0x%x\n", i, f, floatToBits(f));
    }
    for (int e = -10; e < 10; e++) {
        double f = pow(10.0, e);
        int i = f2h(f);
        printf("%g -> 0x%x ->%g\n", f, i, h2f(i));
    }
}

int main()
{
    bool fullTest = 0;
    return testall(fullTest)? 1 : 0;
    //printall(); // for diff comparision of output
    //f2htimingtest();
    //h2ftimingtest();
    return 0;

}

