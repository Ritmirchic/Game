// FRACTURE — headless/native entry point: screenshots & simulation runs
// (Windows GUI entry lives in platform/win_main.cpp and links the same game core)
#include "ft/game.h"
#include <string>
#include <chrono>

using namespace ft;

static bool argMatch(const char* a, const char* name) { return std::strcmp(a, name) == 0; }

int main(int argc, char** argv) {
    std::string mode = "run";
    std::string outPath = "/tmp/shot.rgba";
    std::string scene = "test";
    int W = 1280, H = 720;
    int runFrames = 0, shotFrames = 0;
    Vec3 camPos(0, 0, 0);
    bool havePos = false;
    f32 yawDeg = 180, pitchDeg = -8;
    f32 timeH = 10.5f;
    int weather = 0;
    f32 fovDeg = 75;
    bool menuShot = false;
    u64 seed = 1;
    f32 fogOverride = -1.f;
    bool boom = false; Vec3 boomPos; f32 boomR = 3.f, boomF = 300.f;

    for (int i = 1; i < argc; i++) {
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (argMatch(argv[i], "--shot")) { mode = "shot"; outPath = next(); if (outPath[0] == '-') { outPath = "/tmp/shot.rgba"; i--; } }
        else if (argMatch(argv[i], "--run")) { mode = "run"; runFrames = std::atoi(next()); }
        else if (argMatch(argv[i], "--w")) W = std::atoi(next());
        else if (argMatch(argv[i], "--h")) H = std::atoi(next());
        else if (argMatch(argv[i], "--scene")) scene = next();
        else if (argMatch(argv[i], "--seed")) seed = (u64)std::atoll(next());
        else if (argMatch(argv[i], "--frames")) shotFrames = std::atoi(next());
        else if (argMatch(argv[i], "--time")) timeH = (f32)std::atof(next());
        else if (argMatch(argv[i], "--weather")) weather = std::atoi(next());
        else if (argMatch(argv[i], "--fov")) fovDeg = (f32)std::atof(next());
        else if (argMatch(argv[i], "--yaw")) yawDeg = (f32)std::atof(next());
        else if (argMatch(argv[i], "--pitch")) pitchDeg = (f32)std::atof(next());
        else if (argMatch(argv[i], "--menu")) menuShot = true;
        else if (argMatch(argv[i], "--fog")) fogOverride = (f32)std::atof(next());
        else if (argMatch(argv[i], "--boom")) {
            float x = 0, y = 0, z = 0, r = 3, ff = 300;
            if (std::sscanf(next(), "%f,%f,%f,%f,%f", &x, &y, &z, &r, &ff) >= 3) {
                boomPos = Vec3(x, y, z); boomR = r; boomF = ff; boom = true;
            }
        }
        else if (argMatch(argv[i], "--pos")) {
            float x = 0, y = 0, z = 0;
            if (std::sscanf(next(), "%f,%f,%f", &x, &y, &z) == 3) { camPos = Vec3(x, y, z); havePos = true; }
        }
    }

    if (mode == "run" && runFrames <= 0) runFrames = 600;

    // backend
    IBackend* backend = nullptr;
    if (mode == "shot") backend = createSWBackend();
    else backend = createNullBackend();
    if (!backend || !backend->init(nullptr, W, H, 1, false)) {
        errf("backend init failed");
        return 2;
    }

    Game game;
    if (!game.init(backend)) { errf("game init failed"); return 3; }
    game.settings.fov = fovDeg;
    game.timeOfDay = timeH;
    game.sky.weather = (u8)weather;
    if (fogOverride >= 0.f) game.sky.fogDensity = fogOverride;

    if (!game.buildScene(scene.c_str(), seed)) {
        errf("scene build failed");
        return 4;
    }

    auto t0 = std::chrono::steady_clock::now();
    int frames = (mode == "run") ? runFrames : shotFrames;
    if (frames > 0) game.playing = true;
    InputState in;
    for (int i = 0; i < frames; i++) {
        in.beginFrame();
        if (boom && i == frames / 2) {
            logf("BOOM at %.1f,%.1f,%.1f r=%.1f f=%.0f", boomPos.x, boomPos.y, boomPos.z, boomR, boomF);
            (void)boomR;
            game.detonate(boomPos, boomF);
        }
        game.tick(1.f / 60.f, in);
    }
    if (fogOverride >= 0.f) game.sky.fogDensity = fogOverride;
    game.flushDirtyMeshes(-1);   // ensure all destruction is visible in shots
    auto t1 = std::chrono::steady_clock::now();
    f64 simMs = std::chrono::duration<f64, std::milli>(t1 - t0).count();

    if (mode == "run") {
        logf("RUN scene=%s frames=%d simMs=%.0f (%.2f ms/frame) bodies=%zu frags=%zu parts=%d chunks=%d",
             scene.c_str(), frames, simMs, simMs / frames,
             game.physics.bodies.size(), game.physics.frags.size(),
             particleCount(), game.lastChunksDrawn);
        // world stats
        int solid = 0;
        for (u32 ci = 0; ci < (u32)game.world.chunks.size(); ci++) solid += game.world.chunks[ci].solidCount;
        logf("RUN world solid voxels=%d carved=%d support=%s", solid, game.world.carvedCount(),
             game.support.ok() ? "ok" : "STALE");
        // a few sim renders to exercise the renderer headlessly
        game.render();
        game.render();
        logf("RUN render ok");
    } else {
        // shot camera
        if (menuShot) { game.screen = SCR_MENU; game.playing = false; }
        else {
            game.screen = SCR_PLAY;
            if (havePos) game.setCamera(camPos, yawDeg * 3.14159265f / 180.f, pitchDeg * 3.14159265f / 180.f);
            else {
                // default: stand at spawn looking at house1
                Vec3 p(game.world.W * VOX * 0.5f + 6.f, 6.2f, game.world.D * VOX * 0.5f + 16.f);
                game.setCamera(p, yawDeg * 3.14159265f / 180.f, pitchDeg * 3.14159265f / 180.f);
            }
            // keep menu chrome off in play mode
        }
        game.render();
        // read back
        std::vector<u8> px;
        backend->readPixels(px);
        if (px.size() != (size_t)W * H * 3) {
            errf("readPixels failed (%zu)", px.size());
            return 5;
        }
        FILE* f = std::fopen(outPath.c_str(), "wb");
        if (!f) { errf("cannot open output"); return 6; }
        std::fwrite(px.data(), 1, px.size(), f);
        std::fclose(f);
        logf("SHOT %dx%d -> %s (%zu bytes) carved=%d frags=%zu bodies=%zu",
             W, H, outPath.c_str(), px.size(), game.world.carvedCount(),
             game.physics.frags.size(), game.physics.bodies.size());
    }

    game.shutdown();
    backend->shutdown();
    delete backend;
    return 0;
}
