plugin "ptextureShadeop.so";

surface ptextest(string mapname="";
		 uniform float width=1;
		 uniform float blur=0; 
		 string filter="gaussian";
		 uniform float lerp=1;
		 varying float __faceindex=0)
{
    if (mapname != "") {
	Ci = Ptexture(mapname, 0, __faceindex, u, v, du, 0, 0, dv, "width", width, "blur", blur, "filter", filter, "lerp", lerp);
    }
    else {
	Ci = 1;
    }

#if 1
    float diffuse = clamp(-normalize(N).vector(I), 0, 1);
    Ci = Ci * diffuse;
#endif

    Oi = 1;
}
