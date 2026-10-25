#include "../src/dl_world.h"
using namespace nf; using namespace dl;
int main() {
    Renderer R; R.init(1200, 700); R.setResolutionScale(1.0f);
    World W; W.build(R, "assets/tex");
    V3 pos(-6.0f, 1.1f, 1.62f), dir = normalize(V3(1, 0, -0.02f));
    R.beginFrame();
    R.setCamera(pos, dir, V3(0, 0, 1), 70);
    R.setAmbient(V3(1.0f, 1.0f, 1.0f), 1.0f);     // flat albedo view
    FogSettings fg; fg.density = 0; R.setFog(fg);
    Flashlight fl; fl.on = false; R.setFlashlight(fl);
    R.post().grain = 0; R.post().vignette = 0; R.post().bloom = 0;
    R.post().saturation = 1.0f; R.post().contrast = 1.0f; R.post().lift = 0;
    for (auto& g : W.groups) if (g.visible) R.draw(g.mesh, M4(), W.mats.get(g.mat));
    R.render();
    Image img(R.width(), R.height());
    memcpy(img.px.data(), R.pixels(), img.px.size() * 4);
    pngWrite("shots/d_albedo.png", img);
    return 0;
}
