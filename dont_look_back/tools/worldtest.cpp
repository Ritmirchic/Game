#include "../src/dl_world.h"
using namespace nf;
using namespace dl;

static Image renderView(Renderer& R, World& W, V3 pos, V3 dir, float fov, bool flash, int frames = 1) {
    for (int f = 0; f < frames; ++f) {
        R.beginFrame();
        R.setCamera(pos, normalize(dir), V3(0, 0, 1), fov);
        R.setAmbient(V3(0.042f, 0.046f, 0.058f), 1.0f);
        FogSettings fg; fg.density = 0.019f; fg.scatter = 0.42f; R.setFog(fg);
        Flashlight fl;
        fl.pos = pos + V3(0, 0, 0.12f);
        fl.dir = normalize(dir);
        fl.on = flash; fl.intensity = flash ? 3.4f : 0.0f;
        fl.inner = 0.93f; fl.outer = 0.74f; fl.range = 20.0f;
        R.setFlashlight(fl);
        for (auto& gr : W.groups) {
            if (gr.visible) R.draw(gr.mesh, M4(), W.mats.get(gr.mat));
        }
        // lights
        V3 camPos = pos;
        for (auto& fx : W.fixtures) {
            float d = length(fx.pos - camPos);
            if (d > 22.0f) continue;
            PointLight pl; pl.pos = fx.pos; pl.color = fx.color;
            pl.intensity = fx.baseIntensity; pl.range = fx.range;
            R.addLight(pl);
        }
        R.render();
    }
    Image out(R.width(), R.height());
    memcpy(out.px.data(), R.pixels(), out.px.size() * 4);
    return out;
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "shots";
    Renderer R;
    R.init(1280, 720);
    R.setResolutionScale(1.0f);
    World W;
    W.build(R, "assets/tex");
    int nTri = 0, nVis = 0;
    for (auto& g : W.groups) { nTri += (int)g.mesh.idx.size() / 3; nVis++; }
    printf("world: groups=%d tris=%d colliders=%zu doors=%zu docs=%zu rooms=%zu lights=%zu\n",
           nVis, nTri, W.colliders.size(), W.doors.size(), W.documents.size(), W.rooms.size(),
           W.fixtures.size());
    printf("nav: %dx%dx%d\n", W.nav.nx, W.nav.ny, W.nav.nz);
    // spawn walkability
    printf("spawn walkable: %d\n", (int)W.nav.pointWalkable(W.playerSpawn));
    // path tests across the building
    struct PT { V3 a; V3 b; const char* name; };
    PT tests[] = {
        {V3(-11.2f, 1.1f, 1.62f), V3(0, 1.1f, 1.62f), "apt47 -> corridor"},
        {V3(0, 1.1f, 1.62f), V3(0, 9.0f, 1.62f), "corridor -> lobby"},
        {V3(0, 9.0f, 1.62f), V3(-6.5f, 9.0f, 1.62f), "lobby -> stairwell"},
        {V3(-3.0f, 4.0f, 1.62f), V3(-7.0f, 4.0f, 1.62f), "boiler area"},
        {V3(-1.0f, 10.5f, -1.78f), V3(1.7f, 6.0f, -1.78f), "basement corridor"},
    };
    for (auto& t : tests) {
        std::vector<V3> path;
        bool ok = W.nav.findPath(t.a, t.b, path);
        printf("path %-28s ok=%d nodes=%zu\n", t.name, (int)ok, path.size());
    }
    if (std::string(mode) == "shots") {
        struct View { const char* name; V3 pos; V3 dir; bool flash; };
        View views[] = {
            {"01_spawn", V3(-11.2f, 1.1f, 1.62f), V3(1, 0.05f, -0.06f), true},
            {"02_hall", V3(-11.0f, 0.9f, 1.62f), V3(0.6f, 0.8f, -0.05f), true},
            {"03_living", V3(-12.8f, 2.6f, 1.62f), V3(0.7f, 0.6f, -0.08f), true},
            {"04_corridor", V3(-6.0f, 1.1f, 1.62f), V3(1, 0, -0.02f), true},
            {"05_corridor_e", V3(2.0f, 1.1f, 1.62f), V3(1, 0, 0), true},
            {"06_lobby", V3(0, 8.0f, 1.62f), V3(0, 1, 0.02f), true},
            {"07_stairwell", V3(-5.5f, 9.4f, 1.62f), V3(-1, 0.15f, -0.35f), true},
            {"08_apt305", V3(-6.0f, 4.0f, 1.62f), V3(0.7f, 0.2f, -0.05f), true},
            {"09_basement", V3(1.7f, 8.0f, -1.78f), V3(0, -1, -0.05f), true},
            {"10_boiler", V3(-1.0f, 3.0f, -1.78f), V3(-1, 0.1f, -0.05f), true},
            {"11_nolight", V3(-6.0f, 1.1f, 1.62f), V3(1, 0, 0), false},
        };
        for (auto& v : views) {
            Image img = renderView(R, W, v.pos, v.dir, 70.0f, v.flash);
            std::string p = std::string("shots/w_") + v.name + ".png";
            pngWrite(p.c_str(), img);
            printf("shot %s\n", p.c_str());
        }
    }
    return 0;
}
