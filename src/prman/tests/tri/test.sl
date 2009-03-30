plugin "ptextureShadeop.so";

class test()
{
    public void surface(output color Ci, Oi)
    {
	Ci = ptexture("tri.ptx", 0, 0, u, v, du, dv, 0);
	//Ci = texture("uv.tx", u, v);
	//	Ci *= abs(normalize(vector(4,1,-1)).normalize(N));
	Oi = 1;
    }
}
