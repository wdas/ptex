#include <algorithm>
#include <map>
#include <iostream>
#include <numeric>
#include <vector>
#include <ItImage.h>
#include <ItImageIO.h>
#include "Ptexture.h"
#include "PtexUtils.h"

namespace {

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
}


class Mesh {
 public:
    Mesh(int nverts, const float* verts, int nuvs, const float* uvs,
	 int nfaces, const int* nvertsPerFace,
	 const int* faceverts, const int* faceuvs);
    int nverts()		{ return _verts.size(); }
    int nuvs()			{ return _uvs.size(); }
    int nfaces()		{ return _nvertsPerFace.size(); }
    int nfaceverts()		{ return _faceverts.size(); }
    float* verts()		{ return (float*)&_verts[0]; }
    float* uvs()		{ return (float*)&_uvs[0]; }
    int* nvertsPerFace()	{ return &_nvertsPerFace[0]; }
    int* faceverts()		{ return &_faceverts[0]; }
    int* faceuvs()		{ return &_faceuvs[0]; }
    bool getneighbors(int faceid, int adjfaces[4], int adjedges[4]);

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
    struct Vec3 { float v[3]; };
    struct Vec2 { float v[2]; };
    std::vector<Vec3> _verts;	     // list of verts
    std::vector<Vec2> _uvs;	     // list of uv verts
    std::vector<int> _nvertsPerFace; // list of nverts per face
    std::vector<int> _faceverts;     // packed face vert ids
    std::vector<int> _faceuvs;	     // packed face uv ids

    std::vector<Edge> _edges;	     // temp edge data
    std::vector<int> _faceedges;     // edge ids per face vert
    IntPairMap _edgemap;	     // map from v1,v2 to edge id
};


Mesh::Mesh(int nverts, const float* verts,
	   int nuvs, const float* uvs,
	   int nfaces, const int* nvertsPerFace, const int* faceverts,
	   const int* faceuvs)
{
    _verts.assign((Vec3*)verts, ((Vec3*)verts)+nverts);
    _uvs.assign((Vec2*)uvs, ((Vec2*)uvs)+nuvs);
    _nvertsPerFace.assign(nvertsPerFace, nvertsPerFace + nfaces);
    int nfaceverts = std::accumulate(_nvertsPerFace.begin(),
				     _nvertsPerFace.end(), 0);
    _faceverts.assign(faceverts, faceverts + nfaceverts);
    _faceuvs.assign(faceuvs, faceuvs + nfaceverts);
    buildEdges();
}


void Mesh::buildEdges()
{
    _edges.reserve(nverts()*2);
    _faceedges.resize(nfaceverts());
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



Mesh* loadOBJ(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) return 0;
    std::vector<float> verts;
    std::vector<float> uvs;
    std::vector<int> nvertsPerFace;
    std::vector<int> faceverts;
    std::vector<int> faceuvs;
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
		    verts.push_back(x);
		    verts.push_back(y);
		    verts.push_back(z);
		}
		break;
	    case 't':
		if (sscanf(line, "vt %f %f", &u, &v) == 2) {
		    uvs.push_back(u);
		    uvs.push_back(v);
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
		while (*cp == ' ') cp++;
		int nverts = 0;
		while (sscanf(cp, "%d/%d/%d", &vi, &ti, &ni) == 3) {
		    nverts++;
		    faceverts.push_back(vi-1);
		    faceuvs.push_back(ti-1);
		    while (*cp && *cp != ' ') cp++;
		    while (*cp == ' ') cp++;
		}
		nvertsPerFace.push_back(nverts);
	    }
	    break;
	}
    }
    fclose(file);
    return new Mesh(verts.size()/3, &verts[0], uvs.size()/2, &uvs[0],
		    nvertsPerFace.size(), &nvertsPerFace[0],
		    &faceverts[0], &faceuvs[0]);
}



int main(int argc, char** argv)
{
    if (argc < 3 || argc > 4) {
	std::cerr << "Usage: ptxmake [objfile] texture output.ptx\n" << std::endl;
	return 1;
    }
    int nextarg = 1;

    int nfaces = 1;
    Mesh* mesh = 0;
    if (argc > 3) {
	const char* objname = argv[nextarg++];
	mesh = loadOBJ(objname);
	if (!mesh) {
	    std::cerr << "Error reading input obj: " << objname << std::endl;
	    return 1;
	}

	const int* nvertsPerFace = mesh->nvertsPerFace();
	nfaces = mesh->nfaces();
	for (int i = 0; i < nfaces; i++)
	{
	    if (nvertsPerFace[i] != 4) {
		std::cerr << "Not a quad mesh: " << objname << std::endl;
		return 1;
	    }
	}

    }

    const char* txname = argv[nextarg++];
    const char* ptxname = argv[nextarg++];

    ItImage img;
    if (ItImageIO().load(txname, img) != ItStatus::Ok) {
	std::cerr << "Error reading input texture: " << txname << std::endl;
	return 1;
    }
    int imgw = img.getWidth(), imgh = img.getHeight();

    Ptex::DataType dt;
    switch (img.getDataType().getValue()) {
    case ItDataType::Int8:     dt = Ptex::dt_uint8; break;
    case ItDataType::Int16:    dt = Ptex::dt_uint16; break;
    case ItDataType::Float32:  dt = Ptex::dt_float; break;
    default:
	std::cerr << "Unsupported img data type: " << img.getDataType().getValue() << std::endl;
	return 1;
    }

    int nchannels = img.numChannels();

    int alpha;
    switch (img.getColorModel().getValue()) {
    case ItColorModel::RGBA: alpha = 3; break;
    case ItColorModel::LumA: alpha = 1; break;
    default: alpha = -1;
    }

    std::string error;
    PtexWriter* pt = PtexWriter::open(ptxname, Ptex::mt_quad, dt, nchannels, alpha, 
				      nfaces, error);
    if (!pt) {
	std::cerr << error << std::endl;
	return 1;
    }
    

    if (mesh) {
	const float* uvs = mesh->uvs();
	const int* faceuvs = mesh->faceuvs();

	for (int i = 0; i < nfaces; i++) {
	    // get face uvs
	    float u[4], v[4];
	    const int* uvids = faceuvs + i*4;
	    for (int j = 0; j < 4; j++)
	    {
		const float* uv = &uvs[uvids[j]*2];
		u[j] = uv[0];
		v[j] = uv[1];
	    }

	    // validate them
	    if (u[0] != u[3] || u[1] != u[2] || v[0] != v[1] || v[2] != v[3] ||
		u[0] >= u[1] || v[0] >= v[3])
	    {
		std::cerr << "Invalid uvs for per-face texture: faceid=" << i << std::endl;
		for (int j = 0; j < 4; j++) 
		    std::cerr << "    uv" << j << ": " << u[j] << ", " << v[j] << std::endl;
		return 1;
	    } 
	
	    // convert uvs to integer extent
	    int x = int(floor(u[0] * imgw + .5));
	    int w = int(ceil(u[1] * imgw - .5)) - x;
	    int y = int(floor(v[0] * imgh + .5));
	    int h = int(ceil(v[3] * imgh - .5)) - y;

	    // make sure it's a power of two
	    if (!PtexUtils::isPowerOfTwo(w) || !PtexUtils::isPowerOfTwo(h))
	    {
		std::cerr << "Invalid uvs for per-face texture: faceid=" << i << std::endl;
		std::cerr << "   pixel size not power of two: " << w << 'x' << h << std::endl;
		return 1;
	    }

	    // buid face info
	    Ptex::FaceInfo face;
	    face.res = Ptex::Res(PtexUtils::floor_log2(w), PtexUtils::floor_log2(h));

	    // get neighbor info
	    int f[4], e[4];
	    mesh->getneighbors(i, f, e);
	    face.setadjedges(e[0], e[1], e[2], e[3]);
	    face.setadjfaces(f[0], f[1], f[2], f[3]);

	    // extract subimage
	    ItImage subimg;
	    if (subimg.mkSubImage(img, x,y,w,h) != ItStatus::Ok) {
		std::cout << "ItImage::mkSubImage failed" << std::endl;
		return 1;
	    }
	
	    // write to file
	    pt->writeFace(i, face, subimg.getData(), subimg.getRowStride());
	}
    }
    else {
	// no mesh given, write a single face

	// make sure image is a power of two
	if (!PtexUtils::isPowerOfTwo(imgw) || !PtexUtils::isPowerOfTwo(imgh))
	{
	    std::cerr << "Image size not a power of two: " << imgw << 'x' << imgh << std::endl;
	    return 1;
	}

	char* data = (char*) img.getData();
	int stride = img.getRowStride();
	data += (imgh-1) * stride;
	pt->writeFace(0, Ptex::Res(PtexUtils::floor_log2(imgw), PtexUtils::floor_log2(imgh)),
		      data, -stride);
    }
    if (!pt->close(error)) {
	std::cerr << error << std::endl;
	return 1;
    }
    pt->release();

    return 0;
}
