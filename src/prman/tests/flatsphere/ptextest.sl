plugin "ptextureShadeop.so";

surface ptextest(string mapname=""; uniform float sharpness=1.0; uniform float faceid=0; uniform float blur=1.0)
{
//    Ci = ptexture(mapname, 0, faceid, u, v, du*blur, dv*blur, sharpness);
    Ci = ptexture(mapname, 0, faceid, u, v, du, dv, 0, blur, sharpness, 1);
    float diffuse = clamp(-normalize(N).vector(I), 0, 1);
    Ci = Ci * diffuse;
    Oi = 1;
}
