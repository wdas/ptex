plugin "ptextureShadeop.so";

surface ptextest(string mapname=""; uniform float sblur=1; uniform float sharpness=1.0;
		 uniform float __faceindex=0)
{
    if (mapname != "") {
	//Ci = ptexture(mapname, 0, __faceindex, u, v, du*sblur, dv*sblur, sharpness);
	Ci = ptexture(mapname, 0, __faceindex, u, v, sblur, sblur, sharpness);
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
