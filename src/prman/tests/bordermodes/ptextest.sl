plugin "ptextureShadeop.so";

surface ptextest(string mapname="";
		 uniform float width=1;
		 uniform float blur=0; 
		 uniform float sharpness=1.0;
		 uniform float lerp=1;
		 uniform float __faceindex=0)
{
    if (mapname != "") {
	Ci = Ptexture(mapname, 0, __faceindex, s, t, du, 0, 0, dv, width, blur, sharpness, lerp);
    }
    else {
	Ci = 1;
    }

#if 0
    float diffuse = clamp(-normalize(N).vector(I), 0, 1);
    Ci = Ci * diffuse;
#endif

    Oi = 1;
}
