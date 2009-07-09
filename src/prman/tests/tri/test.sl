plugin "ptextureShadeop.so";


class test(varying float __faceindex=0)
{
    public void surface(output color Ci, Oi)
    {
#if 1
	point Pr = transform( "raster", P );
	float a = Pr[0]*.1, b = Pr[1];
	float dadu = Du(a), dbdu = Du(b), dadv = Dv(a), dbdv = Dv(b);
	float detinv = 1.0/(dadu*dbdv - dbdu*dadv);
	float duda = dbdv*detinv, dvda = -dbdu*detinv, dudb = -dadv*detinv, dvdb = dadu*detinv;

	float s = v, t = 1-u-v;
#if 0
	// use grid derivatives and chain rule
	float dsdu = Du(s), dtdu = Du(t), dsdv = Dv(s), dtdv = Dv(t);
	float uw1 = dsdu * duda + dsdv * dvda;
	float vw1 = dtdu * duda + dtdv * dvda;
	float uw2 = dsdu * dudb + dsdv * dvdb;
	float vw2 = dtdu * dudb + dtdv * dvdb;
#else
	// use analytic derivs for s and t (dsdu = 0; dtdu = -1; dsdv = 1; dtdv = -1;)
	// (should be identical)
	float uw1 = dvda;
	float vw1 = -duda - dvda;
	float uw2 = dvdb;
	float vw2 = -dudb - dvdb;
#endif

	float width = 5, blur = 0, sharpness = 0, lerp = 0;
	Ci = Ptexture("tri.ptx", 0, __faceindex, s, t, uw1, vw1, uw2, vw2, width, blur, sharpness, lerp);
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
	float uc = 1/3., vc = 1/3., wc = 1-uc-vc;
	float ur = (u-uc);
	float vr = (v-vc);
	float wr = (w-wc);
	float R = pow(10,2);
	Ci = A*ur*ur + B*ur*vr + C*vr*vr < R ? 1 : 0;
#endif
	Oi = 1;
    }
}
