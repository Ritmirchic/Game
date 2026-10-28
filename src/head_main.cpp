// FRACTURE — headless/native entry point: screenshots & simulation runs
// (Windows GUI entry lives in platform/win_main.cpp and links the same game core)
#include "ft/game.h"
#include "ft/audio.h"
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
    std::string sfxName, sfxOut;
    bool mixTest = false;
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
        else if (argMatch(argv[i], "--sfx")) { sfxName = next(); sfxOut = next(); }
        else if (argMatch(argv[i], "--mixtest")) { mixTest = true; sfxOut = next(); }
        else if (argMatch(argv[i], "--tooltest")) { mode = "tooltest"; }
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

    // ---- audio verification modes (no backend/scene needed) ----
    if (!sfxName.empty() || mixTest) {
        audioInitBank();
        if (mixTest) {
            AudioMixer mx;
            mx.init(44100);
            mx.setListener(Vec3(0, 1.6f, 0), Vec3(0, 0, -1));
            VoiceDesc a; a.sfx = SFX_GUN_RIFLE; a.pos = Vec3(4, 1.6f, -3); a.gain = 1.f;
            VoiceDesc b; b.sfx = SFX_FIRE_LOOP; b.pos = Vec3(-6, 1.f, -4); b.loop = true; b.gain = 0.8f;
            VoiceDesc c; c.sfx = SFX_UI_CLICK; c.direct2D = true; c.gain = 0.7f;
            int va = mx.play(a), vb = mx.play(b), vc = mx.play(c);
            int total = 44100 * 2;   // 2 seconds
            std::vector<s16> pcm((size_t)total * 2);
            int done = 0;
            while (done < total) {
                int chunk = tmin(1024, total - done);
                mx.mix(pcm.data() + (size_t)done * 2, chunk);
                mx.updateLoops(chunk / 44100.f);
                done += chunk;
            }
            bool okw = sfxWriteWavRaw(pcm.data(), total, sfxOut.c_str());
            logf("MIXTEST voices=%d,%d,%d active=%d -> %s ok=%d",
                 va, vb, vc, mx.activeVoices(), sfxOut.c_str(), (int)okw);
        } else {
            bool found = false;
            static const char* names[] = {
                "ui_click","ui_hover","footstep1","footstep2","footstep3","footstep4",
                "impact_wood","impact_metal","impact_concrete","impact_glass","impact_soil",
                "explosion_s","explosion_m","explosion_l","debris_rain",
                "gun_shotgun","gun_rifle","gun_smg","gun_pistol","gun_plasma",
                "rocket_launch","grenade_launch","charge_beep","charge_stick",
                "drill","saw","torch","extinguisher","spray_paint","harpoon",
                "sledge_swing","door_creak","ignite","fire_loop","vehicle_crash",
                "pickup","jump_land","body_fall","empty"
            };
            for (int i = 0; i < SFX_COUNT; i++)
                if (sfxName == names[i]) {
                    found = sfxWriteWav((SfxId)i, sfxOut.c_str());
                    const SfxSample& s = sfxSample((SfxId)i);
                    logf("SFX %s -> %s frames=%d", names[i], sfxOut.c_str(), s.frames);
                    break;
                }
            if (!found) { errf("unknown sfx '%s'", sfxName.c_str()); return 5; }
        }
        return 0;
    }

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

    if (mode == "tooltest") {
        int fails = 0;
        auto report = [&](const char* name, bool ok, const char* detail) {
            logf("%-18s %s   %s", name, ok ? "PASS" : "FAIL", detail);
            if (!ok) fails++;
        };
        game.playing = true;
        game.screen = SCR_PLAY;
        // synthetic wooden test wall 0.75-1.0 m ahead (rebuilt before every tool so
        // each test strikes fresh material regardless of previous digging)
        Vec3 O(8.f, 12.f, 8.f);
        f32 yaw = 0.f, pitch = 0.f;
        auto buildWall = [&]() {
            int x0 = (int)std::floor((O.x - 1.5f) / VOX), x1 = (int)std::floor((O.x + 1.5f) / VOX);
            int y0 = (int)std::floor((O.y - 1.5f) / VOX), y1 = (int)std::floor((O.y + 1.5f) / VOX);
            int z0 = (int)std::floor((O.z - 1.5f) / VOX), z1 = (int)std::floor((O.z - 1.0f) / VOX);
            u16 col = rgb565(160, 120, 80);
            for (int z = z0; z <= z1; z++)
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                        if (game.world.inBounds(x, y, z))
                            game.world.set(x, y, z, MAT_WOOD, col, 0, EDIT_BUILD, true);
            game.flushDirtyMeshes(-1);
        };
        auto aim = [&]() { buildWall(); game.setCamera(O, yaw, pitch); };
        aim();
        Vec3 D = game.cam.fwd;

        int c0 = 0, b0 = 0;
        auto snap = [&]() { c0 = game.world.carvedCount(); b0 = (int)game.physics.bodies.size(); };
        char buf[160];

        // --- destructive tools on open soil floor ---
        const int carveTools[] = {TOOL_SLEDGE, TOOL_SAW, TOOL_DRILL, TOOL_TORCH, TOOL_SHOTGUN, TOOL_RIFLE, TOOL_SMG};
        for (int t : carveTools) {
            aim();
            snap();
            int times = (t == TOOL_SAW || t == TOOL_DRILL || t == TOOL_TORCH) ? 8 : 3;
            bool fired = false;
            for (int i = 0; i < times; i++) {
                game.tools().cd = 0;
                fired = game.fireTool(t, O, D, true) || fired;
            }
            int d = game.world.carvedCount() - c0;
            std::snprintf(buf, sizeof(buf), "carved +%d voxels in %d uses", d, times);
            report(toolDef(t).name, fired && d > 0, buf);
        }
        if (game.fire().burning())
            game.fire().extinguishAt(game.fire().centroid(), 400.f);

        // hands
        aim();
        game.tools().cd = 0;
        report("Hands", game.fireTool(TOOL_HAND, O, D, true), "shove ray at ground");
        // paint: tints without carving
        aim(); snap(); game.tools().cd = 0;
        {
            bool ok = game.fireTool(TOOL_PAINT, O, D, true);
            std::snprintf(buf, sizeof(buf), "carved delta %d (must be 0)", game.world.carvedCount() - c0);
            report("Paint Sprayer", ok && game.world.carvedCount() == c0, buf);
        }
        // plank placement
        aim(); snap(); game.tools().cd = 0;
        {
            bool ok = game.fireTool(TOOL_PLANK, O, D, true);
            int db = (int)game.physics.bodies.size() - b0;
            std::snprintf(buf, sizeof(buf), "bodies %+d", db);
            report("Plank", ok && db >= 1, buf);
        }
        // C4 place + remote detonation
        aim(); snap(); game.tools().cd = 0;
        {
            bool placed = game.fireTool(TOOL_C4, O, D, true);
            int bAfter = (int)game.physics.bodies.size();
            game.tools().cd = 0;
            game.fireTool(TOOL_C4, O, D, false);   // RMB = detonate
            int d = game.world.carvedCount() - c0;
            std::snprintf(buf, sizeof(buf), "placed=%d detonated carved +%d remaining %d",
                          placed ? 1 : 0, d, game.tools().c4Count);
            report("C4 Charge", placed && bAfter > b0 && d > 0 && game.tools().c4Count == 0, buf);
        }

        auto settleProj = [&](int steps) {
            for (int i = 0; i < steps; i++) {
                game.updateProjectiles(1.f / 60.f);
                game.simStep();
            }
            if (game.fire().burning())
                game.fire().extinguishAt(game.fire().centroid(), 400.f);   // keep tests isolated
        };

        // grenade: bounce -> fuse -> burst
        aim(); snap(); game.tools().cd = 0;
        game.fireTool(TOOL_GRENADE, O, D, true);
        settleProj(300);
        {
            int d = game.world.carvedCount() - c0;
            std::snprintf(buf, sizeof(buf), "carved +%d after fuse burst", d);
            report("Grenade Launcher", d > 0, buf);
        }
        // rocket: impact detonation
        aim(); snap(); game.tools().cd = 0;
        game.fireTool(TOOL_ROCKET, O, D, true);
        settleProj(240);
        {
            int d = game.world.carvedCount() - c0;
            std::snprintf(buf, sizeof(buf), "carved +%d after impact", d);
            report("Rocket Launcher", d > 0, buf);
        }
        // harpoon: embeds a static shaft into the ground
        aim(); snap(); game.tools().cd = 0;
        game.fireTool(TOOL_HARPOON, O, D, true);
        settleProj(180);
        {
            int db = (int)game.physics.bodies.size() - b0;
            std::snprintf(buf, sizeof(buf), "bodies %+d (embedded shaft)", db);
            report("Harpoon Gun", db >= 1, buf);
        }

        // --- fire system: torch ignition on a wooden voxel ---
        Vec3 woodPos;
        bool foundWood = false;
        for (int y = 0; y < 28 && !foundWood; y++)
            for (int z = 16; z < 48 && !foundWood; z++)
                for (int x = 16; x < 48 && !foundWood; x++) {
                    Voxel v = game.world.get(x, y, z);
                    if (v.mat == MAT_WOOD) {
                        woodPos = Vec3((x + 0.5f) * VOX, (y + 0.5f) * VOX, (z + 0.5f) * VOX);
                        foundWood = true;
                    }
                }
        if (foundWood) {
            Vec3 wp = woodPos;
            O = Vec3(wp.x, wp.y + 1.0f, wp.z);
            pitch = -1.45f;
            game.setCamera(O, yaw, pitch);
            Vec3 D2 = game.cam.fwd;
            game.tools().cd = 0;
            game.fireTool(TOOL_TORCH, O, D2, true);
            game.tools().cd = 0;
            game.fireTool(TOOL_TORCH, O, D2, true);
            int burn1 = game.fire().burning();
            report("Blowtorch ignition", burn1 > 0, (std::snprintf(buf, sizeof buf, "burning=%d at wood", burn1), buf));
            for (int i = 0; i < 90; i++) game.simStep();   // 1.5 s of spread
            int burn2 = game.fire().burning();
            std::snprintf(buf, sizeof(buf), "burning %d -> %d after 1.5 s", burn1, burn2);
            report("Fire propagation", burn2 >= burn1, buf);
            // extinguisher puts it all out
            game.fire().extinguishAt(game.fire().centroid(), 400.f);
            int burn3 = game.fire().burning();
            report("Extinguisher", burn3 == 0, (std::snprintf(buf, sizeof buf, "burning=%d after spray", burn3), buf));
        } else {
            report("Blowtorch ignition", false, "no wooden voxel found in test map");
        }

        game.playing = false;
        logf("TOOLTEST %s  (%d failures)", fails == 0 ? "OK" : "FAILED", fails);
        return fails == 0 ? 0 : 6;
    }

    auto t0 = std::chrono::steady_clock::now();
    int frames = (mode == "run") ? runFrames : shotFrames;
    if (frames > 0) game.playing = true;
    if (frames > 0 && !menuShot) game.screen = SCR_PLAY;   // gameplay shots: HUD + viewmodel
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
        // render BEFORE logging so lastChunksDrawn reflects a real frame
        game.render();
        game.render();
        logf("RUN scene=%s frames=%d simMs=%.0f (%.2f ms/frame) bodies=%zu frags=%zu parts=%d chunks=%d",
             scene.c_str(), frames, simMs, simMs / frames,
             game.physics.bodies.size(), game.physics.frags.size(),
             particleCount(), game.lastChunksDrawn);
        // world stats
        int solid = 0;
        for (u32 ci = 0; ci < (u32)game.world.chunks.size(); ci++) solid += game.world.chunks[ci].solidCount;
        logf("RUN world solid voxels=%d carved=%d support=%s", solid, game.world.carvedCount(),
             game.support.ok() ? "ok" : "STALE");
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
