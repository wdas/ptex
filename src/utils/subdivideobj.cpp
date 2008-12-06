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
#include "mesh.h"

int main(int argc, char** argv)
{
    if (argc != 3) {
	std::cerr << "Usage: subdivideobj <in.obj> <out.obj>\n" << std::endl;
	return 1;
    }

    const char* inobj = argv[1];
    const char* outobj = argv[2];
    Mesh mesh;
    if (!mesh.loadOBJ(inobj)) {
	std::cerr << "Error reading input obj: " << inobj << std::endl;
	return 1;
    }
    mesh.subdivide();
    mesh.saveOBJ(outobj);
    return 0;
}
