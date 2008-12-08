/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
#include <iostream>
#include <ri.h>
#include "mesh.h"



int main(int argc, char** argv)
{
    if (argc != 3) {
	std::cerr << "Usage: objtorib <in.obj> <out.rib>\n" << std::endl;
	return 1;
    }

    const char* inobj = argv[1];
    const char* outrib = argv[2];
    Mesh mesh;
    if (!mesh.loadOBJ(inobj)) {
	std::cerr << "Error reading input obj: " << inobj << std::endl;
	return 1;
    }

    char* tags[] = { "interpolateboundary", "facevaryinginterpolateboundary" };
    int ntags = sizeof(tags)/sizeof(char*);
    int nargs[] = { 1, 0, 1, 0 };
    int intargs[] = { 2, 0 };
    float floatargs[] = {0}; // dummy arg
    int __faceindex = 0;

    RiBegin((RtToken)outrib);
    RiArchiveRecord("structure", "RenderMan RIB-Structure 1.1");
    RiSubdivisionMesh("catmull-clark", mesh.nfaces(), mesh.nvertsPerFace(),
		      mesh.faceverts(), ntags, tags, nargs, intargs, floatargs,
		      "P", mesh.verts(),
		      "constant integer __faceindex", &__faceindex, RI_NULL);
    RiEnd();

    return 0;
}
