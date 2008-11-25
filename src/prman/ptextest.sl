plugin "ptextureShadeop.so";

surface ptextest(string mapname=""; uniform float sharpness=1.0; uniform float __faceindex=0)
{
    uniform float i;
    Ci = ptexture(mapname, 0, __faceindex, u, v, du, dv, sharpness);
#if 1
    float diffuse = clamp(-normalize(N).vector(I), 0, 1);
    Ci = Ci * diffuse;
#endif
    Oi = 1;
}
