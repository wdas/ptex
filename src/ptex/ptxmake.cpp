#include <iostream>
#include <vector>
#include <ItImage.h>
#include <ItImageIO.h>
#include "Ptexture.h"
#include "PtexUtils.h"
#include "subd.h"


Subd* loadOBJ(const char* filename)
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
    return new Subd(verts.size()/3, &verts[0], uvs.size()/2, &uvs[0],
		    nvertsPerFace.size(), &facetileids[0], &nvertsPerFace[0],
		    &faceverts[0], &faceuvs[0]);
}



int main(int argc, char** argv)
{
    if (argc != 4) {
	std::cerr << "Usage: ptxmake objfile texture output.ptx\n" << std::endl;
	return 1;
    }

    Subd* subd = loadOBJ(argv[1]);
    if (!subd) {
	std::cerr << "Error reading input obj: " << argv[1] << std::endl;
	return 1;
    }
    int nfaces = subd->nfaces();
    const int* nvertsPerFace = subd->nvertsPerFace();
    const float* uvs = subd->uvs();
    const int* faceuvs = subd->faceuvs();

    for (int i = 0; i < nfaces; i++)
    {
	if (nvertsPerFace[i] != 4) {
	    std::cerr << "Not a quad mesh: " << argv[1] << std::endl;
	    return 1;
	}
    }

    ItImage img;
    if (ItImageIO().load(argv[2], img) != ItStatus::Ok) {
	std::cerr << "Error reading input texture: " << argv[2] << std::endl;
	return 1;
    }

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
    PtexWriter* pt = PtexWriter::open(argv[3], Ptex::mt_quad, dt, nchannels, alpha, 
				      nfaces, error);
    if (!pt) {
	std::cerr << error << std::endl;
	return 1;
    }
    
    int imgw = img.getWidth(), imgh = img.getHeight();
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
	int x = int(floor(u[0] * imgw));
	int w = int(ceil(u[1] * imgw)) - x;
	int y = int(floor(v[0] * imgh));
	int h = int(ceil(v[3] * imgh)) - y;

	// make sure it's a power of two
	if (!PtexUtils::isPowerOfTwo(w) || !PtexUtils::isPowerOfTwo(h))
	{
	    std::cerr << "Invalid uvs for per-face texture: faceid=" << i << std::endl;
	    std::cerr << "   pixel size not power of two: " << w << 'x' << h << std::endl;
	    return 1;
	}

	// buid face info
	Ptex::FaceInfo face;
	face.res = (PtexUtils::floor_log2(w), PtexUtils::floor_log2(h));

	// get neighbor info
	int f[4], e[4];
	subd->getneighbors(i, f, e);
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

    if (!pt->close(error)) {
	std::cerr << error << std::endl;
	return 1;
    }
    pt->release();

    return 0;
}
