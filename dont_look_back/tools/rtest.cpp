#include "../src/nf_render.h"
#include <cstdio>
using namespace nf;

static void addBox(Mesh& m, V3 lo, V3 hi, float uvs = 1.0f) {
    (void)uvs;
    V3 c[8] = {V3(lo.x,lo.y,lo.z),V3(hi.x,lo.y,lo.z),V3(hi.x,hi.y,lo.z),V3(lo.x,hi.y,lo.z),
               V3(lo.x,lo.y,hi.z),V3(hi.x,lo.y,hi.z),V3(hi.x,hi.y,hi.z),V3(lo.x,hi.y,hi.z)};
    int faces[6][4] = {{0,3,2,1},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{0,4,7,3}};
    for (int f=0; f<6; f++) {
        V3 a=c[faces[f][0]], b=c[faces[f][1]], d=c[faces[f][2]], e=c[faces[f][3]];
        V3 n = normalize(cross(b-a, d-a));
        if (dot(n, (a+d)*0.5f - (lo+hi)*0.5f) < 0) n = n*-1.0f;
        Vertex va, vb, vd, ve;
        va.p=a; vb.p=b; vd.p=d; ve.p=e;
        va.n=vb.n=vd.n=ve.n=n;
        va.u=0; va.v=0; vb.u=1; vb.v=0; vd.u=1; vd.v=1; ve.u=0; ve.v=1;
        va.baked=vb.baked=vd.baked=ve.baked=V3(1,0,0);
        m.addQuad(va,vb,vd,ve);
    }
    m.finalize();
}

int main() {
    Renderer R; R.init(960, 540);
    Image tex(256,256);
    Noise nz(7);
    for (int y=0;y<256;y++) for(int x=0;x<256;x++){
        float v = 0.5f + 0.5f*nz.fbm2(x*0.05f,y*0.05f,4);
        // brick pattern
        int row = y/32; int off = (row%2)*32;
        float bx = fmodf(x+off,64), by = fmodf(y,32);
        float m = (bx<3||by<3)?0.25f:1.0f;
        tex.set(x,y, colF(v*m*0.8f, v*m*0.75f, v*m*0.65f));
    }
    int ti = R.textures.add(tex);
    Mesh box; addBox(box, V3(-1,-1,0), V3(1,1,2));
    Mesh floor; addBox(floor, V3(-8,-8,-0.2f), V3(8,8,0));
    Material mat; mat.tex=ti; mat.spec=0.2f; mat.gloss=24;

    R.beginFrame();
    R.setCamera(V3(0,-3.2f,1.6f), normalize(V3(0,1,-0.12f)), V3(0,0,1), 72);
    R.setAmbient(V3(0.055f,0.062f,0.085f), 1.0f);
    FogSettings fg; fg.density=0.03f; R.setFog(fg);
    Flashlight fl; fl.pos=V3(0.1f,-3.1f,1.7f); fl.dir=normalize(V3(0,1,-0.1f));
    fl.intensity=3.2f; fl.on=true; fl.range=22; fl.inner=0.94f; fl.outer=0.70f; R.setFlashlight(fl);
    fg.scatter=0.55f; R.setFog(fg);
    R.draw(floor, M4(), mat);
    R.draw(box, M4(), mat);
    PointLight pl; pl.pos=V3(3,-3,2.2f); pl.color=V3(1.0f,0.82f,0.58f); pl.intensity=1.2f; pl.range=7; R.addLight(pl);
    R.render();
    Image out(960,540);
    memcpy(out.px.data(), R.pixels(), out.px.size()*4);
    printf("dc=%d tris=%d\n", R.drawCallCount(), R.triCount());
    bool ok = pngWrite("/home/user/Game/dont_look_back/shots/rtest.png", out);
    printf("write %d\n", (int)ok);
    return 0;
}
