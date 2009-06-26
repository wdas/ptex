plugin "ptextureShadeop.so";

    
surface test(string mapname="";
	     uniform float width=1;
	     uniform float blur=0; 
	     uniform float sharpness=1.0;
	     uniform float lerp=1;
	     uniform float dispScale=.01;
	     uniform float faceid=0;
	     varying float weights[]={})
{
    float i;
    print(weights[0], weights[1]);
    if (mapname != "") {
	float sharp = 0; // sharpness;
	color Ctex = ptexture(mapname, 0, faceid, u, 1-v, du, dv, width, blur, sharp, lerp);
	normal Nn = normalize(N);
	P += Nn * ((Ctex[0]-.5) * dispScale);
	Ci = 1;//mix(color 1, color cellnoise(faceid), .5);
    }
    else {
	Ci = 1;
    }

#if 1
    normal Nd = normalize(calculatenormal(P));
    float diffuse = clamp(-normalize(Nd).vector(I), 0, 1);
    Ci = Ci * diffuse;
#endif

    Oi = 1;
}
