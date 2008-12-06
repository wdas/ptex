#ifndef mesh_h
#define mesh_h

#include <vector>

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
    bool getneighbor(int faceid, int edgeid, int& adjfaces, int& adjedges);
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

    bool _quadmesh;		     // true if mesh is all quads
    std::vector<Edge> _edges;	     // temp edge data
    std::vector<int> _faceedges;     // edge ids per face vert
    IntPairMap _edgemap;	     // map from v1,v2 to edge id
};

#endif
