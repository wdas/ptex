/* 
   CONFIDENTIAL INFORMATION: This software is the confidential and
   proprietary information of Walt Disney Animation Studios ("Disney").
   This software is owned by Disney and may not be used, disclosed,
   reproduced or distributed for any purpose without prior written
   authorization and license from Disney. Reproduction of any section of
   this software must include this legend and all copyright notices.
   (c) Disney. All rights reserved.
*/
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <numeric>
#include <map>
#include <iostream>
#include <vector>
#include "SESubd.h"
#include "mesh.h"

#if 0
// map of (int,int) to int
class IntPairMap
{
 public:
    IntPairMap() : numEntries(0) {}
    // find: returns -1 if not found; overwrite to add to map
    int& find(int v1, int v2) {
	// grow if needed
	int size = table.size();
	if (numEntries >= size / 2) 
	{ 
	    reserve(size*2);
	    size = table.size(); 
	}
	// start at hash pos and look for entry or empty slot
	unsigned int hash = v1 * 1664525 + v2; // constant from Knuth
	Entry* e = &table[hash%size];
	Entry* end = &table[size];
	while (1) {
	    if (e->v1 == v1 && e->v2 == v2) return e->id;
	    if (e->id == -1) break;
	    if (++e == end) e = &table[0];
	}
	// not found, insert
	e->v1 = v1; e->v2 = v2;
	numEntries++;
	return e->id;
    }
    void reserve(int size)
    {
	size = std::max(size, 16);
	if (size > int(table.size())) {
	    IntPairMap em; em.table.resize(size);
	    Entry* end = &table[table.size()];
	    for (Entry* e = &table[0]; e != end; e++)
		em.find(e->v1, e->v2) = e->id;
	    std::swap(table, em.table);
	}
    }
    void clear() { table.clear(); numEntries = 0; }
 private:
    struct Entry { 
	int v1, v2, id; 
	Entry() : v1(-1), v2(-1), id(-1) {}
    };
    int numEntries;
    std::vector<Entry> table;
};


class Mesh {
 public:
    Mesh() {}
    int nverts()		{ return _verts.size(); }
    int nuvs()			{ return _uvs.size(); }
    int nfaces()		{ return _nvertsPerFace.size(); }
    int nfaceverts()		{ return _faceverts.size(); }
    float* verts()		{ return &_verts[0].v[0]; }
    float* uvs()		{ return &_uvs[0].v[0]; }
    int* nvertsPerFace()	{ return &_nvertsPerFace[0]; }
    int* faceverts()		{ return &_faceverts[0]; }
    int* faceuvs()		{ return &_faceuvs[0]; }
    bool getneighbors(int faceid, int adjfaces[4], int adjedges[4]);
    void subdivide();
    bool loadOBJ(const char* objName);
    bool saveOBJ(const char* objName);

 private:
    struct Edge {
	int facea, faceb;	// adjacent faces
	int v0, v1;		// vertex id's
	int uva0, uva1;		// uv id's for first face
	int uvb0, uvb1;		// uv id's for second face
    };

    void buildEdges();
    int addEdge(int faceid, int v0, int v1, int uv0, int uv1);

    // surface definition
    struct Vec3 {
	float v[3];
	Vec3(){}
	Vec3(float x, float y, float z) { v[0] = x; v[1] = y; v[2] = z; }
	Vec3(float p[3]) { v[0] = p[0]; v[1] = p[1]; v[2] = p[2]; }
    };
    struct Vec2 {
	float v[2];
	Vec2(){}
	Vec2(float x, float y) { v[0] = x; v[1] = y; }
	Vec2(float p[2]) { v[0] = p[0]; v[1] = p[1]; }
    };
    std::vector<Vec3> _verts;	     // list of verts
    std::vector<Vec2> _uvs;	     // list of uv verts
    std::vector<int> _nvertsPerFace; // list of nverts per face
    std::vector<int> _faceverts;     // packed face vert ids
    std::vector<int> _faceuvs;	     // packed face uv ids

    std::vector<Edge> _edges;	     // temp edge data
    std::vector<int> _faceedges;     // edge ids per face vert
    IntPairMap _edgemap;	     // map from v1,v2 to edge id
};


void Mesh::buildEdges()
{
    _edges.clear();
    _edges.reserve(nverts()*2);

    _faceedges.clear();
    _faceedges.resize(nfaceverts());

    _edgemap.clear();
    _edgemap.reserve(nverts()*4);

    const int* vert = &_faceverts[0];
    const int* uv = &_faceuvs[0];
    int* e = &_faceedges[0];

    // build list of edges from face verts
    for (int face = 0; face < nfaces(); face++) {
	int nverts = _nvertsPerFace[face];
	for (int i = 0; i < nverts; i++) {
	    int i2 = (i+1)%nverts;
	    *e++ = addEdge(face, vert[i], vert[i2], uv[i], uv[i2]);
	}
	vert += nverts;
	uv += nverts;
    }
}


int Mesh::addEdge(int faceid, int v0, int v1, int uv0, int uv1)
{
    // put verts in canonical order for lookup
    if (v0 > v1) { std::swap(v0, v1); std::swap(uv0, uv1); }

    // look for existing edge
    int& id = _edgemap.find(v0, v1);
    if (id < 0) {
	// edge not found, create a new one and set first face point
	id = _edges.size();
	_edges.resize(_edges.size()+1);
	Edge& e = _edges[id];
	e.facea = faceid;
	e.faceb = -1;
	e.v0 = v0;
	e.v1 = v1;
	e.uva0 = uv0;
	e.uva1 = uv1;
	e.uvb0 = e.uvb1 = -1;
    }
    else {
	// found edge, add second face and update flags
	Edge& e = _edges[id];
	e.faceb = faceid;
	e.uvb0 = uv0;
	e.uvb1 = uv1;
    }
    return id;
}


bool Mesh::getneighbors(int faceid, int adjfaces[4], int adjedges[4])
{
    if (_nvertsPerFace[faceid] != 4 || _faceedges.size() != _nvertsPerFace.size()*4)
	// not a quad mesh
	return 0;
    
    int* eids = &_faceedges[faceid*4];
    for (int i = 0; i < 4; i++) {
	int eid = eids[i];
	Edge& e = _edges[eid];
	// get opposite face
	int adjface = e.facea != faceid ? e.facea : e.faceb;
	int* adjeids = &_faceedges[adjface*4];
	adjfaces[i] = adjface;
	if (adjface == -1) adjedges[i] = 0;
	else {
	    // find corresponding edge
	    int adjedge = 0;
	    for (; adjedge < 4; adjedge++) if (adjeids[adjedge] == eid) break;
	    if (adjedge == 4) return 0; // bad mesh
	    // record edge
	    adjedges[i] = adjedge;
	}
    }
    return 1;
}


void Mesh::subdivide()
{
    std::vector<int> nvertsPerFaceOrig = _nvertsPerFace;
    std::vector<int> faceTileIds(nfaces());
    SESubd* subd = SESubd::build(nverts(), verts(), nuvs(), uvs(), nfaces(), nvertsPerFace(),
				 faceverts(), faceuvs(), &faceTileIds[0]);
    subd->subdivide(1);
    _verts.assign((Vec3*)subd->verts(), ((Vec3*)subd->verts()) + subd->nVerts());
    _uvs.assign((Vec2*)subd->uvs(), ((Vec2*)subd->uvs()) + subd->nUVs());
    _nvertsPerFace.assign(subd->nVertsPerFace(), subd->nVertsPerFace() + subd->nFaces());
    _faceverts.assign(subd->faceVerts(), subd->faceVerts() + subd->nFaceVerts());
    _faceuvs.assign(subd->faceUVs(), subd->faceUVs() + subd->nFaceVerts());

    // face orientations get rotated during subdivision - restore original orientation
    // (this makes it easier to paste the textures back together)
    int* faceverts = &_faceverts[0];
    int* faceuvs = &_faceuvs[0];
    for (int i = 0, n = nvertsPerFaceOrig.size(); i < n; i++) {
	int nverts = nvertsPerFaceOrig[i];
	if (nverts == 4) {
	    // quad subdivided into 4 quads - rotate faces back to original orientation
	    std::rotate(faceverts+4, faceverts+7, faceverts+8);
	    std::rotate(faceverts+8, faceverts+10, faceverts+12);
	    std::rotate(faceverts+12, faceverts+13, faceverts+16);
	}
	faceverts += nverts*4;
	faceuvs += nverts*4;
    }

    buildEdges();
}


bool Mesh::loadOBJ(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) return 0;

    _verts.clear();
    _uvs.clear();
    _nvertsPerFace.clear();
    _faceverts.clear();
    _faceuvs.clear();
    std::vector<int> facetileids;

    int tileId = 0;
    bool newtile = 0;
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
	char* end = &line[strlen(line)-1];
	if (*end == '\n') *end = '\0'; // strip trailing nl
	float x, y, z, u, v;
	switch (line[0]) {
	case 'g': newtile = 1; break;
	    
	case 'v':
	    switch (line[1]) {
	    case ' ':
		if (sscanf(line, "v %f %f %f", &x, &y, &z) == 3) {
		    _verts.push_back(Vec3(x,y,z));
		}
		break;
	    case 't':
		if (sscanf(line, "vt %f %f", &u, &v) == 2) {
		    _uvs.push_back(Vec2(u,v));
		}
		break;
	    }
	    break;
	case 'f':
	    if (line[1] == ' ') {
		if (newtile) { newtile = 0; tileId++; }
		facetileids.push_back(tileId);
		int vi, ti, ni;
		const char* cp = &line[2];
		int nverts = 0;
		while (1) {
		    int count = 0;
		    if (sscanf(cp, " %d//%d%n", &vi, &ni, &count) == 2)
			ti = 1;
		    else if (sscanf(cp, " %d/%s/%d%n", &vi, &ti, &ni, &count) == 3)
			;
		    else break;
		    nverts++;
		    _faceverts.push_back(vi-1);
		    _faceuvs.push_back(ti-1);
		    cp += count;
		}
		if (nverts > 0)
		    _nvertsPerFace.push_back(nverts);
	    }
	    break;
	}
    }
    fclose(file);
    buildEdges();
    return 1;
}


bool Mesh::saveOBJ(const char* filename)
{
    FILE* fp = fopen(filename, "w");

    fprintf(fp, "version 1\n");
    fprintf(fp, "surface subd per-face cage pPlane1\n");
    for (int i = 0, n = nverts(); i < n; i++)
	fprintf(fp, "v %g %g %g\n", _verts[i].v[0], _verts[i].v[1], _verts[i].v[2]);
    for (int i = 0, n = nverts(); i < n; i++) {
	Vec3 v = _verts[i];
	float scale = sqrt(v.v[0]*v.v[0] + v.v[1]*v.v[1] + v.v[2]*v.v[2]);
	if (scale) scale = 1/scale;
	fprintf(fp, "vn %g %g %g\n", scale*v.v[0], scale*v.v[1], scale*v.v[2]);
    }
    fprintf(fp, "g pPlane1\n");
    int fv = 0;
    for (int i = 0, n = nfaces(); i < n; i++) {
	fprintf(fp, "f ");
	int nverts = _nvertsPerFace[i];
	for (int v = 0; v < nverts; v++) {
	    int vi = _faceverts[fv+v];
	    fprintf(fp, " %d//%d", vi+1, vi+1);
	}
	fv += nverts;
	fprintf(fp, "\n");
    }
}
#endif

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
