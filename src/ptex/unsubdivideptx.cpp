/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <numeric>
#include <map>
#include <iostream>
#include <vector>
#include "Ptexture.h"
#include "mesh.h"

bool unsubptx(const char* inobjname, const char* inptxname, const char* outptxname)
{
    // copy texture from subdivided mesh to unsubdivided mesh

    Mesh mesh;
    if (!mesh.loadOBJ(inobjname)) {
	std::cerr << "Error reading input obj: " << inobjname << std::endl;
	return 0;
    }
    Mesh submesh = mesh;
    submesh.subdivide();

    std::string err;
    PtexTexture* inptx = PtexTexture::open(inptxname, err);
    if (!inptx) {
	std::cerr << err << std::endl;
	return 0;
    }

    int* nvertsPerFace = mesh.nvertsPerFace();
    std::vector<int> ifaceids(submesh.nfaces());
    std::vector<int> sfaceids(mesh.nfaces());
    std::vector<int> ofaceids(mesh.nfaces());
    std::vector<int> isQuad(mesh.nfaces());
    int nOutputFaces = 0;
    int nSubFaces = 0;
    
    // determine mappings:
    //    ifaceids - input (unsubdivided) faceids from subdivided faceids
    //    sfaceids - subdivided faceids from input faceids
    //    ofaceids - output faceids from input faceids
    for (int i = 0, n = mesh.nfaces(); i < n; i++) {
	int nverts = nvertsPerFace[i];
	bool quad = (nverts == 4);
	isQuad[i] = quad;

	// there's one subdivided quad face per input vert
	sfaceids[i] = nSubFaces;
	for (int e = 0; e < nverts; e++)
	    ifaceids[nSubFaces++] = i;

	// for output, there's one quad face per input quad
	// and one quad subface per vert for non-quad input faces
	ofaceids[i] = nOutputFaces;
	nOutputFaces += quad ? 1 : nverts;
    }
    assert(nSubFaces == submesh.nfaces());

    PtexWriter* outptx = PtexWriter::open(outptxname, Ptex::mt_quad, inptx->dataType(),
					  inptx->numChannels(), inptx->alphaChannel(), nOutputFaces, err);
    if (!outptx) {
	std::cerr << err << std::endl;
	inptx->release();
	return 0;
    }

    int pixelsize = inptx->numChannels() * Ptex::DataSize(inptx->dataType());

    // for each face in mesh
    //    else just copy subfaces
    for (int i = 0, n = mesh.nfaces(), ifaceid = 0; i < n; i++) {
	Ptex::FaceInfo fi = inptx->getFaceInfo(ifaceid);
	int nverts = nvertsPerFace[i];
	if (nverts == 4) {
	    // got a quad input face - combine four subfaces from inptx
	    Ptex::Res ores(fi.res.ulog2+1, fi.res.vlog2+1);
	    void* buffer = malloc(ores.size()*pixelsize);
	    int istride = fi.res.u() * pixelsize;
	    int ostride = ores.u() * pixelsize;
	    for (int f = 0; f < 4; f++, ifaceid++) {
		if (fi.res != inptx->getFaceInfo(ifaceid).res) {
		    std::cerr << "Subface texture resolutions don't match for quad face:\n"
			      << "  Subface id's " << ifaceid << ", " << (ifaceid+f) << std::endl;
		    continue;
		}
		int offset = 0;
		switch (f) {
		case 1: offset = istride; break;
		case 2: offset = istride * (fi.res.v() * 2 + 1); break;
		case 3: offset = istride * (fi.res.v() * 2); break;
		}
		inptx->getData(ifaceid, (char*)buffer+offset, ostride);
	    }
	    int adjfaces[4], adjedges[4];
	    for (int eid = 0; eid < 4; eid++) {
		int afid;
		submesh.getneighbor(sfaceids[i]+eid, eid, afid, adjedges[eid]);
		// convert subfaceid to ofaceid
		if (afid < 0) {
		    adjfaces[eid] = -1;
		}
		else {
		    int afid_in = ifaceids[afid];
		    adjfaces[eid] = ofaceids[afid_in];
		    // for non-quad neighbors: adjust for particular subface
		    if (!isQuad[afid_in]) 
			adjfaces[eid] += (afid - sfaceids[afid_in]);
		}
	    }
	    outptx->writeFace(ofaceids[i], Ptex::FaceInfo(ores, adjfaces, adjedges, false), buffer);
	    free(buffer);
	} else {
	    // got a non-quad input face - copy n subfaces from inptx as-is
	    for (int f = 0; f < nverts; f++, ifaceid++) {
		Ptex::FaceInfo fi = inptx->getFaceInfo(ifaceid);
		void* buffer = malloc(fi.res.size()*pixelsize);
		inptx->getData(ifaceid, buffer, 0);
		static int adjfaces[4], adjedges[4];
		outptx->writeFace(ofaceids[i]+f, Ptex::FaceInfo(fi.res, adjfaces, adjedges, true), buffer);
		free(buffer);
	    }
	}
    }

    inptx->release();
    if (!outptx->close(err)) {
	std::cerr << err << std::endl;
	outptx->release();
	return 0;
    }
    outptx->release();
    return 1;
}

int main(int argc, char** argv)
{
    if (argc != 4) {
	std::cerr << "Usage: unsubptx <in.obj> <in.ptx> <out.ptx>\n" << std::endl;
	return 1;
    }
    const char* inobjname = argv[1];
    const char* inptxname = argv[2];
    const char* outptxname = argv[3];

    int nfaces = 1;
    if (!unsubptx(inobjname, inptxname, outptxname)) return 1;
    return 0;
}
