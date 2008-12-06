#include <math.h>
#include <SESubd.h>
#include "mesh.h"


void Mesh::buildEdges()
{
    _quadmesh = true;
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
	if (nverts != 4) _quadmesh = false;
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


bool Mesh::getneighbor(int faceid, int edgeid, int& adjface, int& adjedge)
{
    if (!_quadmesh) {
	// not a quad mesh
	adjface = -1;
	adjedge = 0;
	return 0;
    }
    
    int eid = _faceedges[faceid*4+edgeid];
    Edge& e = _edges[eid];
    // get opposite face
    adjface = (e.facea != faceid) ? e.facea : e.faceb;
    int* adjeids = &_faceedges[adjface*4];
    if (adjface == -1) adjedge = 0;
    else {
	// find corresponding edge
	adjedge = 0;
	for (; adjedge < 4; adjedge++) if (adjeids[adjedge] == eid) break;
	if (adjedge == 4) {
	    // not adj edge found: bad mesh
	    adjface = -1;
	    adjedge = 0;
	    return 0;
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
		    else if (sscanf(cp, " %d/%d/%d%n", &vi, &ti, &ni, &count) == 3)
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
    if (!fp) return 0;

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
    fclose(fp);
    return 1;
}

