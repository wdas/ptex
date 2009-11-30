plugin "ptextureShadeop.so";

surface ptexenvtest(string mapname=""; float blur=0)
{
    vector R = transform("object", reflect(normalize(I), normalize(N)));

    vector dRdu = Du(R)*du/2;
    vector dRdv = Dv(R)*dv/2;

    vector R1 = R-dRdu-dRdv;
    vector R2 = R-dRdu+dRdv;
    vector R3 = R+dRdu-dRdv;
    vector R4 = R+dRdu+dRdv;

    if (mapname != "") Ci = Ptexenv (mapname, 0, R1, R2, R3, R4, blur);
    else Ci = 1;
    Oi = 1;
}
