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

    Mesh basemesh;
    if (!basemesh.loadOBJ(inobjname)) {
	std::cerr << "Error reading input obj: " << inobjname << std::endl;
	return 0;
    }
    Mesh submesh = basemesh;
    submesh.subdivide();

    int* nvertsPerFace = basemesh.nvertsPerFace();

    // determine faceid mappings:  (subdivided mesh <--> basemesh --> unsubdivided mesh)
    std::vector<int> bfaceids(submesh.nfaces());  // basemesh id's from subdivided id's
    std::vector<int> sfaceids(basemesh.nfaces()); // subdivided id's from basemesh id's
    std::vector<int> ufaceids(basemesh.nfaces()); // unsubdivided id's from basemesh id's
    std::vector<int> isQuad(basemesh.nfaces());   // is basemesh face a quad?
    int nUnSubFaces = 0;
    int nSubFaces = 0;

    for (int i = 0, n = basemesh.nfaces(); i < n; i++) {
	int nverts = nvertsPerFace[i];
	bool quad = (nverts == 4);
	isQuad[i] = quad;

	// there's one subdivided quad face per input vert
	sfaceids[i] = nSubFaces;
	for (int e = 0; e < nverts; e++)
	    bfaceids[nSubFaces++] = i;

	// for output, there's one quad face per input quad
	// and one quad subface per vert for non-quad input faces
	ufaceids[i] = nUnSubFaces;
	nUnSubFaces += quad ? 1 : nverts;
    }
    assert(submesh.nfaces() == nSubFaces);

    // open input texture and check number of faces
    Ptex::String error;
    PtexPtr<PtexTexture> inptx ( PtexTexture::open(inptxname, error) );
    if (!inptx) {
	std::cerr << error << std::endl;
	return 0;
    }

    if (inptx->numFaces() != nSubFaces) {
	std::cerr << "Texture has incorrect number of faces for mesh: " << inptx->numFaces()
		  << " (expected " << nSubFaces << ")" << std::endl;
	return 0;
    }

    // open output texture
    PtexPtr<PtexWriter> outptx
	( PtexWriter::open(outptxname, Ptex::mt_quad, inptx->dataType(),
			   inptx->numChannels(), inptx->alphaChannel(), nUnSubFaces, error) );
    if (!outptx) {
	std::cerr << error << std::endl;
	return 0;
    }

    int pixelsize = inptx->numChannels() * Ptex::DataSize(inptx->dataType());

    // for each face in base mesh
    for (int i = 0, n = basemesh.nfaces(), ifaceid = 0; i < n; i++) {
	int nverts = nvertsPerFace[i];
	if (nverts == 4) {
	    // got a quad face - combine four textures from inptx
	    // first, find minimum res of input faces
	    Ptex::Res ires = inptx->getFaceInfo(ifaceid).res;
	    for (int f = 1; f < 4; f++) {
		Ptex::Res sres = inptx->getFaceInfo(ifaceid+f).res;
		if (sres != ires) {
		    static int warned = 0;
		    if (!warned) {
			warned = true;
			std::cerr << "Warning: inconsistent res for quad subfaces"
				  << " (id's " << ifaceid << ".." << (ifaceid+3) << ")"
				  << ", reducing to lowest common res." << std::endl;
			std::cerr << "(Only first instance reported)" << std::endl;
		    }
		    ires.ulog2 = std::min(ires.ulog2, sres.ulog2);
		    ires.vlog2 = std::min(ires.vlog2, sres.vlog2);
		}
	    }

	    // output res = 2x input res
	    Ptex::Res ores(ires.ulog2+1, ires.vlog2+1);
	    void* buffer = malloc(ores.size()*pixelsize);
	    int istride = ires.u() * pixelsize;
	    int ostride = ores.u() * pixelsize;

	    // read source textures, pack into single texture
	    for (int f = 0; f < 4; f++, ifaceid++) {
		int offset = 0;
		switch (f) {
		case 1: offset = istride; break;
		case 2: offset = istride * (ires.v() * 2 + 1); break;
		case 3: offset = istride * (ires.v() * 2); break;
		}
		inptx->getData(ifaceid, (char*)buffer+offset, ostride, ires);
	    }
	    // gather adjacency info for face
	    int adjfaces[4], adjedges[4];
	    for (int eid = 0; eid < 4; eid++) {
		int afid;
		submesh.getneighbor(sfaceids[i]+eid, eid, afid, adjedges[eid]);
		// convert subfaceid to ofaceid
		if (afid < 0) {
		    adjfaces[eid] = -1;
		}
		else {
		    int afid_in = bfaceids[afid];
		    adjfaces[eid] = ufaceids[afid_in];
		    // for non-quad neighbors: adjust for particular subface
		    if (!isQuad[afid_in]) 
			adjfaces[eid] += (afid - sfaceids[afid_in]);
		}
	    }
	    outptx->writeFace(ufaceids[i], Ptex::FaceInfo(ores, adjfaces, adjedges, false), buffer);
	    free(buffer);
	} else {
	    // got a non-quad input face - copy n subfaces from inptx as-is
	    for (int f = 0; f < nverts; f++, ifaceid++) {
		Ptex::Res ires = inptx->getFaceInfo(ifaceid).res;
		void* buffer = malloc(ires.size()*pixelsize);
		inptx->getData(ifaceid, buffer, 0);
		// gather adjacency info for face
		int adjfaces[4], adjedges[4];
		for (int eid = 0; eid < 4; eid++) {
		    int afid;
		    submesh.getneighbor(ifaceid, eid, afid, adjedges[eid]);
		    // convert subfaceid to ofaceid
		    if (afid < 0) {
			adjfaces[eid] = -1;
		    }
		    else {
			int afid_in = bfaceids[afid];
			adjfaces[eid] = ufaceids[afid_in];
			// for non-quad neighbors: adjust for particular subface
			if (!isQuad[afid_in]) 
			    adjfaces[eid] += (afid - sfaceids[afid_in]);
		    }
		}

		outptx->writeFace(ufaceids[i]+f, Ptex::FaceInfo(ires, adjfaces, adjedges, true), buffer);
		free(buffer);
	    }
	}
    }

    if (!outptx->close(error)) {
	std::cerr << error << std::endl;
	return 0;
    }
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

    if (!unsubptx(inobjname, inptxname, outptxname)) return 1;
    return 0;
}
