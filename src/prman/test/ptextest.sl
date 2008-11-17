plugin "ptextureShadeop.so";

surface ptextest(string mapname=""; uniform float sharpness=1.0; uniform float __faceindex=0)
{
    Ci = ptexture(mapname, 0, __faceindex, u, v, du, dv, sharpness);
    float diffuse = clamp(-normalize(N).vector(I), 0, 1);
    Ci = Ci * diffuse;
    Oi = 1;
}
