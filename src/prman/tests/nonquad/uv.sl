surface uv(uniform float __faceindex=0)
{
    uniform float subface = (__faceindex >= 5 && __faceindex <= 10) ? 1 : 0;
    uniform float scale = subface != 0 ? .5 : 1;
    //    Ci = color(max(du,dv),min(du,dv),0)*100 * scale;
    Ci = color(u,v,0);
    Oi = 1;
}
