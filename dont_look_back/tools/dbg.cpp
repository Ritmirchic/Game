#include "../src/dl_world.h"
using namespace nf; using namespace dl;
int main(int argc, char** argv) {
    Renderer R; R.init(1400, 900); R.setResolutionScale(1.0f);
    World W; W.build(R, "assets/tex");
    float scat = argc > 1 ? atof(argv[1]) : 0.0f;
    struct View { const char* name; V3 pos; V3 dir; V3 up; float fov; bool flash; bool noceil; };
    View v[] = {
        {"top", V3(-1.0f, 1.0f, 26.0f), V3(0, 0, -1), V3(0, 1, 0), 60, false, true},
        {"corr_noscat", V3(-6.0f, 1.1f, 1.62f), V3(1, 0, -0.02f), V3(0, 0, 1), 70, true, false},
        {"corr_nofog", V3(-6.0f, 1.1f, 1.62f), V3(1, 0, -0.02f), V3(0, 0, 1), 70, true, false},
    };
    for (int i = 0; i < 3; ++i) {
        R.beginFrame();
        R.setCamera(v[i].pos, normalize(v[i].dir), v[i].up, v[i].fov);
        R.setAmbient(V3(0.030f, 0.033f, 0.040f), 1.0f);
        FogSettings fg; fg.density = (i == 2) ? 0.0f : 0.017f; fg.scatter = scat; R.setFog(fg);
        Flashlight fl; fl.pos = v[i].pos; fl.dir = normalize(v[i].dir); fl.on = v[i].flash;
        fl.intensity = 3.4f; fl.inner = 0.93f; fl.outer = 0.74f; fl.range = 20;
        R.setFlashlight(fl);
        for (auto& d : W.doors) {
            if (length(d.hinge - v[i].pos) > 30) continue;
            R.draw(W.doorLeaf(d.width, d.height), W.doorTransform(d), W.mats.get(MAT_DOOR_PANEL));
        }
        for (auto& g : W.groups) {
            if (!g.visible) continue;
            if (v[i].noceil) {
                // skip ceiling groups by checking the material
                if (g.mat == MAT_CEILING || g.mat == MAT_CEILING_WATER) continue;
            }
            R.draw(g.mesh, M4(), W.mats.get(g.mat));
        }
        for (auto& fx : W.fixtures) {
            if (length(fx.pos - v[i].pos) > 26) continue;
            PointLight pl; pl.pos = fx.pos; pl.color = fx.color; pl.intensity = fx.baseIntensity; pl.range = fx.range;
            R.addLight(pl);
        }
        R.render();
        Image img(R.width(), R.height());
        memcpy(img.px.data(), R.pixels(), img.px.size() * 4);
        pngWrite((std::string("shots/d_") + v[i].name + ".png").c_str(), img);
    }
    return 0;
}
