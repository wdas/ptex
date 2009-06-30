plugin "ptextureShadeop.so";


class test(varying float faceid=0)
{
#if 0
    public void displacement(output point P; output normal N)
    {
	fdsl
	normal Nn = normalize(N);
	float ur = u;//(u-1.0/3);
	float vr = v;//(v-1.0/3);
	float w = 1-u-v;
	float wr = w;//(w-1.0/3);
	float disp = .3 * exp(-100*(ur*ur + vr*vr + wr*wr));
	P += Nn * disp;
	N = calculatenormal(P);
    }
#endif

    public void surface(output color Ci, Oi)
    {
#if 1
	float s = u, t = v;
	//Ci = texture("uv.tx", s, t);//s+fw, t+fw, s+fw, t-fw, s-fw, t+fw, s-fw, t-fw);
	float fw = .1;
	uniform float uw1 = 0, vw1 = 0, uw2 = 0, vw2 = 0;
	uniform float width = 1, blur = 0, sharpness = 0, lerp = 0;
	Ci = Ptexture("tri.ptx", 0, faceid, s, t, uw1, vw1, uw2, vw2, width, blur, sharpness, lerp);
#else

	point sp = transform( "raster", P );
	float a = Du(xcomp(sp));
	float b = Du(ycomp(sp));
	float c = Dv(xcomp(sp));
	float d = Dv(ycomp(sp));
	float A = a*a + b*b;
	float B = (a*c + b*d)*2;
	float C = c*c + d*d;

	float w = 1-u-v;
	float uc = 1/2., vc = 1/3., wc = 1-uc-vc;
	float ur = (u-uc);
	float vr = (v-vc);
	float wr = (w-wc);
	float R = pow(10,2);
	Ci = A*ur*ur + B*ur*vr + C*vr*vr < R ? 1 : 0;

/* 	float ww = (a*c + b*d); */
/* 	float wu = a*a + b*b - ww; */
/* 	float wv = c*c + d*d - ww; */
/* 	Ci = wu*ur*ur + wv*vr*vr + ww*wr*wr< R*.6 ? .5 : 0; */
/* 	Ci += color(ur*ur < abs(R/wu) ? 1 : 0, */
/* 		    vr*vr < abs(R/wv) ? 1 : 0, */
/* 		    wr*wr < abs(R/ww) ? 1 : 0)/2; */
#endif
	Oi = 1;
    }
}
