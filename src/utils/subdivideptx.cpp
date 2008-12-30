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

bool subdivideptx(const char* inobjname, const char* inptxname, const char* outptxname)
{
    // copy texture from unsubdivided mesh to subdivided mesh

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
    PtexPtr<PtexTexture> inptx = PtexTexture::open(inptxname, error);
    if (!inptx) {
	std::cerr << error << std::endl;
	return 0;
    }

    if (inptx->numFaces() != nUnSubFaces) {
	std::cerr << "Texture has incorrect number of faces for mesh: " << inptx->numFaces()
		  << " (expected " << nUnSubFaces << ")" << std::endl;
	return 0;
    }

    // open output texture
    PtexPtr<PtexWriter> outptx
	= PtexWriter::open(outptxname, Ptex::mt_quad, inptx->dataType(),
			   inptx->numChannels(), inptx->alphaChannel(), nSubFaces, error);
    if (!outptx) {
	std::cerr << error << std::endl;
	return 0;
    }

    int pixelsize = inptx->numChannels() * Ptex::DataSize(inptx->dataType());

    // for each face in base mesh
    for (int i = 0, n = basemesh.nfaces(), ifaceid = 0; i < n; i++) {
	int nverts = nvertsPerFace[i];
	if (nverts == 4) {
	    // got a quad face - split source texture from inptx into four textures in outptx
	    Ptex::Res ires = inptx->getFaceInfo(ifaceid).res;
	    // output res = input res / 2  (but can't be smaller than 1 texel!)
	    Ptex::Res ores(std::max(ires.ulog2-1, 0), (std::max(ires.vlog2-1, 0)));
	    
	    // read source texture
	    void* buffer = malloc(ires.size()*pixelsize);
	    inptx->getData(ifaceid, buffer, 0);
	    int stride = ires.u()*pixelsize;
	    int uoffset = (ires.u()/2) * pixelsize; // note: 1/2 == 0; this is what we want
	    int voffset = (ires.v()/2) * stride;

	    for (int f = 0; f < 4; f++) {
		// gather adjacency info
		int adjfaces[4], adjedges[4];
		for (int eid = 0; eid < 4; eid++) {
		    submesh.getneighbor(sfaceids[i]+f, eid, adjfaces[eid], adjedges[eid]);
		}
		outptx->writeFace(sfaceids[i]+f, Ptex::FaceInfo(ores, adjfaces, adjedges, false),
				  (char*)buffer + uoffset*(f==1 || f==2) + voffset*(f>=2), stride);
	    }
	    free(buffer);
	    ifaceid++;
	} else {
	    // got a non-quad input face - copy n subfaces from inptx as-is
	    for (int f = 0; f < nverts; f++, ifaceid++) {
		Ptex::Res ires = inptx->getFaceInfo(ifaceid).res;
		void* buffer = malloc(ires.size()*pixelsize);
		inptx->getData(ifaceid, buffer, 0);
		// gather adjacency info
		int adjfaces[4], adjedges[4];
		for (int eid = 0; eid < 4; eid++) {
		    submesh.getneighbor(sfaceids[i]+f, eid, adjfaces[eid], adjedges[eid]);
		}
		outptx->writeFace(sfaceids[i]+f, Ptex::FaceInfo(ires, adjfaces, adjedges, false), buffer);
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
	std::cerr << "Usage: subdivideptx <in.obj> <in.ptx> <out.ptx>\n" << std::endl;
	return 1;
    }
    const char* inobjname = argv[1];
    const char* inptxname = argv[2];
    const char* outptxname = argv[3];

    if (!subdivideptx(inobjname, inptxname, outptxname)) return 1;
    return 0;
}
