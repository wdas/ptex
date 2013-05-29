/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include <string>
#include <iostream>
#include <numeric>
#include <vector>
#include "Ptexture.h"
#include "PtexReader.h"

class objmesh {
public:
  objmesh(int nverts, const float* verts, int nfaces, const int* nvertsPerFace, const int* faceverts);

  static objmesh              * loadOBJ(const char* filename, int axis);
  
         int                   nverts() const { return _verts.size()/3; }

  const  std::vector<float>  & verts() const { return _verts; }

         int                   nfaces() const { return _nvertsPerFace.size(); }

  const  std::vector<int>    & nvertsperface() const { return _nvertsPerFace; }

  const  std::vector<int>    & faceverts() const { return _faceverts; }

private:
  std::vector<float>  _verts;
  std::vector<int>    _nvertsPerFace;  
  std::vector<int>    _faceverts;        
}; 

objmesh::objmesh(int nverts, const float* verts, int nfaces, const int* nvertsPerFace, const int* faceverts)
{
     _verts.assign( verts, verts+nverts*3);
     
     _nvertsPerFace.assign(nvertsPerFace, nvertsPerFace + nfaces);
       
     int nfaceverts = std::accumulate(_nvertsPerFace.begin(), _nvertsPerFace.end(), 0);
        
     _faceverts.assign(faceverts, faceverts + nfaceverts);
}

objmesh * objmesh::loadOBJ(const char* filename, int axis )
{ 
    FILE* file = fopen(filename, "r");
    if (!file) 
        return 0;                             

    std::vector<float> verts;                         
    std::vector<float> uvs;                         
    std::vector<int> nvertsPerFace;                     
    std::vector<int> faceverts;                         
    std::vector<int> faceuvs;                         
    std::vector<int> facetileids;                      

    int tileId = 0;                             
    bool newtile = 0;                             
    char line[256];                             

    while(fgets(line, sizeof(line), file)) { 

        char* end = &line[strlen(line)-1];                                                      
        if (*end == '\n') // strip trailing nl 
            *end = '\0'; 
            
        float x, y, z, u, v;                         
        switch (line[0]) {
        
            case 'g': newtile = 1; break;                     

            case 'v': switch (line[1]) {
                           case ' ': if(sscanf(line, "v %f %f %f", &x, &y, &z) == 3) 
                                      switch( axis ) {
                                          case 0 : verts.push_back(x); 
                                                   verts.push_back(-z); 
                                                   verts.push_back(y); break;     // 0 is "Y up" - flip
                                          case 1 : verts.push_back(x); 
                                                   verts.push_back(y); 
                                                   verts.push_back(z); break;     // 1 is "Z up"
                                     } break; 
                           case 't': if(sscanf(line, "vt %f %f", &u, &v) == 2) {
                                         uvs.push_back(u); 
                                         uvs.push_back(v); 
                                     } break;
                      } break;
                      
            case 'f': if(line[1] == ' ') {
                          if(newtile) { 
                              newtile = 0; 
                              tileId++; 
                          }                                
                          facetileids.push_back(tileId);                  
                          int vi, ti, ni;                       
                          const char* cp = &line[2];                  
                          while (*cp == ' ') 
                              cp++;                  
                          int nverts = 0, nitems=0;                       
                          while((nitems=sscanf(cp, "%d/%d/%d", &vi, &ti, &ni))>0) {
                               nverts++;                    
                               faceverts.push_back(vi-1);                   
                               if(nitems >= 1) faceuvs.push_back(ti-1);                     
                               while (*cp && *cp != ' ')
                                   cp++;            
                               while (*cp == ' ')
                                   cp++;                     
                          }                              
                          nvertsPerFace.push_back(nverts);              
                      }
                      break;
        }
    }
    fclose(file);                              
    return new objmesh(verts.size()/3, &verts[0], nvertsPerFace.size(), &nvertsPerFace[0], &faceverts[0]);
}

void usage()
{
    std::cerr << "Usage: ptxmeta file.ptx topo.obj\n"
        << "  Insert the geometry from an OBJ file as metadata keys";
    exit(1);
}

int main(int argc, char** argv)
{
    if (argc!=3)
        usage();

    Ptex::String error;
    PtexTexture * r = PtexTexture::open(argv[1], error);
    if (!r) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }
    
    objmesh * obj = objmesh::loadOBJ( argv[2], 0 );
    if (!obj) {
        std::cerr << "Cannot open " << argv[2] << std::endl;
        return 1;
    }
    
    Ptex::MeshType mt = r->meshType();
    Ptex::DataType dt = r->dataType();
    
    int nchan = r->numChannels(), 
        achan = r->alphaChannel(),
        nfaces = r->numFaces();
    
    r->release();

    PtexWriter* w = PtexWriter::edit( argv[1], false, mt, dt, nchan, achan, nfaces, error );

    if (!w) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }

    w->writeMeta("PtexFaceVertCounts",  & (obj->nvertsperface()[0] ), (int)obj->nvertsperface().size() );
    w->writeMeta("PtexFaceVertIndices", & (obj->faceverts()[0]), (int)obj->faceverts().size());
    w->writeMeta("PtexVertPositions",   & (obj->verts()[0]), (int)obj->verts().size());

    if (!w->close(error)) {
        std::cerr << error.c_str() << std::endl;
        return 1;
    }

    w->release();
 
    return 0;
}
