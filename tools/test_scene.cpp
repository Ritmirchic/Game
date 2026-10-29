// Temporary validation harness: builds a voxel scene and renders it with the software renderer.
#include "core/engine.h"
#include "core/png.h"
#include <cstdio>

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "/tmp/scene.png";
    World w;
    w.init(96, 64, 96);
    w.groundVox = 10;

    // terrain: soil + asphalt road + grass top
    for (int z = 0; z < 96; z++)
        for (int x = 0; x < 96; x++) {
            for (int y = 0; y <= 8; y++) w.setRaw(x, y, z, y == 8 ? M_GRAVEL : M_SOIL);
            bool road = (z > 10 && z < 22) || (x > 60 && x < 72);
            w.setRaw(x, 8, z, road ? M_ASPHALT : M_GRAVEL);
            (void)road;
        }

    // house: 12x12 m concrete shell with brick trim, windows, doorway
    int bx0 = 20, bx1 = 68, bz0 = 28, bz1 = 68, by0 = 9, by1 = 44;
    for (int y = by0; y <= by1; y++)
        for (int z = bz0; z <= bz1; z++)
            for (int x = bx0; x <= bx1; x++) {
                bool wallX = (x == bx0 || x == bx1), wallZ = (z == bz0 || z == bz1);
                bool edge = wallX || wallZ || y == by0 || y == by1;
                if (!edge) continue;
                uint8_t m = M_CONCRETE;
                if (y == by1) m = M_ROOF;
                if (y < by0 + 2) m = M_BRICK;
                if (y > by0 + 12 && y < by1 - 3 && (wallX || wallZ)) m = M_PLASTER;
                w.setRaw(x, y, z, m);
            }
    // doorway
    for (int y = by0; y < by0 + 12; y++)
        for (int z = bz0; z < bz0 + 6; z++) w.setRaw(44 + z, y, bz0, M_AIR);
    // windows
    for (int wx : {26, 50})
        for (int y = by0 + 14; y < by0 + 24; y++)
            for (int z = bz0; z < bz0 + 10; z++) w.setRaw(wx + z, y, bz0, M_GLASS);
    // interior floor + furniture
    for (int z = bz0 + 1; z < bz1; z++)
        for (int x = bx0 + 1; x < bx1; x++) w.setRaw(x, by0 + 1, z, M_WOOD);
    w.box(30, by0 + 2, 40, 44, by0 + 8, 46, M_WOOD_DARK);
    w.box(50, by0 + 2, 50, 62, by0 + 6, 56, M_METAL_DARK);

    // trees
    for (auto p : {ivec3(14, 9, 60), ivec3(80, 9, 40), ivec3(14, 9, 14)}) {
        w.box(p.x, p.y, p.z, p.x + 1, p.y + 18, p.z + 1, M_TRUNK);
        w.sphere(vec3(p.x + 1, p.y + 24, p.z + 1), 5.5f, M_FOLIAGE, false);
    }
    // a low wall to smash
    w.box(74, 9, 8, 90, 17, 12, M_BRICK);
    // rubble spheres
    w.sphere(vec3(30, 11, 78), 3, M_STONE);
    w.sphere(vec3(40, 11, 82), 2, M_METAL);

    w.markAllDirty();
    int guard = 0;
    while (w.pendingDirty() > 0 && guard++ < 10000) w.rebuildDirtyMeshes(64);
    size_t tris = 0;
    for (auto& kv : w.meshes) tris += kv.second.idx.size() / 3;
    printf("chunks=%d verts=%zu tris=%zu mem=%.1fMB\n", w.allocChunks, (size_t)0, tris, w.memoryEstimate() / 1e6);

    IRenderer* r = createSoftRenderer();
    Settings st;
    r->init(nullptr, 960, 540, st);
    EnvState env;
    env.sunDir = vec3(0.42f, 0.78f, 0.36f).norm();
    env.sunColor = vec3(1.0f, 0.94f, 0.84f);
    env.sunIntensity = 1.1f;
    env.skyTop = vec3(0.18f, 0.34f, 0.62f);
    env.skyHorizon = vec3(0.68f, 0.76f, 0.84f);
    env.fogColor = vec3(0.66f, 0.73f, 0.80f);
    env.fogDensity = 0.008f;
    env.ambientSky = vec3(0.42f, 0.50f, 0.62f);
    env.ambientGround = vec3(0.26f, 0.23f, 0.19f);
    env.cloudCover = 0.35f;
    env.time = 12;

    Camera cam;
    cam.pos = vec3(3.0f, 6.0f, 27.0f);
    cam.yaw = 0.55f;
    cam.pitch = -0.14f;
    cam.aspect = 960.0f / 540.0f;
    cam.fovY = 78 * DEG2RAD;
    r->beginFrame(cam, env, st);
    r->drawWorld(w, st);
    // crude UI test
    r->endFrame();
    r->screenshot(out);
    printf("wrote %s\n", out);
    r->shutdown();
    return 0;
}
