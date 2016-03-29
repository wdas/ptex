#include "PtexHalf.h"
#include <stdio.h>

/** Table initializations. */
static bool PtexHalfInit(uint32_t* h2fTable, uint16_t* f2hTable)
{
    union { int i; float f; } u;

    for (int h = 0; h < 65536; h++) {
        int s = (h & 0x8000)<<16;
        int m = h & 0x03ff;
        int e = h&0x7c00;

        if (unsigned(e-1) < ((31<<10)-1)) {
            // normal case
            u.i = s|(((e+0x1c000)|m)<<13);
        }
        else if (e == 0) {
            // denormalized
            if (!(h&0x8000)) u.f = float(5.9604644775390625e-08*m);
            else u.f = float(-5.9604644775390625e-08*m);
        }
        else {
            // inf/nan, preserve low bits of m for nan code
            u.i = s|0x7f800000|(m<<13);
        }
        h2fTable[h] = u.i;
    }

    for (int i = 0; i < 512; i++) {
        int f = i << 23;
        int e = (f & 0x7f800000) - 0x38000000;
        // normalized iff (0 < e < 31)
        if (unsigned(e-1) < ((31<<23)-1)) {
            int s = ((f>>16) & 0x8000);
            int m = f & 0x7fe000;
            // add bit 12 to round
            f2hTable[i] = (uint16_t)((s|((e|m)>>13))+((f>>12)&1));
        }
    }

    return 1;
}


int main()
{
    FILE* fp = fopen("PtexHalfTables.h", "w");
    if (!fp) {
        perror("Can't write PtexHalfTable.h");
        return 1;
    }
    uint32_t h2fTable[65536];
    uint16_t f2hTable[512];
    PtexHalfInit(h2fTable, f2hTable);
    fprintf(fp, "PTEXAPI uint32_t PtexHalf::h2fTable[65536] = {");
    for (int i = 0; i < 65536; i++) {
        if (i % 8 == 0) fprintf(fp, "\n");
        fprintf(fp, "    0x%08x", h2fTable[i]);
        if (i != 65535) fprintf(fp, ",");
    }
    fprintf(fp, "\n};\n");
    fprintf(fp, "PTEXAPI uint16_t PtexHalf::f2hTable[512] = {");
    for (int i = 0; i < 512; i++) {
        if (i % 8 == 0) fprintf(fp, "\n");
        fprintf(fp, "    0x%04x", f2hTable[i]);
        if (i != 511) fprintf(fp, ",");
    }
    fprintf(fp, "\n};\n");
    fclose(fp);
    return 0;
}
