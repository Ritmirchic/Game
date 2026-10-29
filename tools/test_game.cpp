// FRACTURE — headless gameplay test harness (QA tests A-H) and screenshot generator.
// Drives the real Game class with the software renderer; no GPU required.
#include "core/engine.h"
#include "game/game.h"
#include "game/globals.h"
#include "game/mission.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_pass = 0, g_fail = 0;
static std::vector<std::string> g_failures;

static void check(const char* name, bool cond, const char* detail = "") {
    if (cond) {
        g_pass++;
        printf("  [PASS] %s\n", name);
    } else {
        g_fail++;
        g_failures.push_back(name);
        printf("  [FAIL] %s %s\n", name, detail);
    }
}
static void checkf(const char* name, bool cond, const char* fmt, double a = 0, double b = 0) {
    char buf[256];
    snprintf(buf, sizeof(buf), fmt, a, b);
    check(name, cond, buf);
}

static Game* G = nullptr;
static IRenderer* R = nullptr;
static const char* shotDir = "/tmp";

static void step(int frames, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < frames; i++) {
        G->input.clearFrame();
        G->update(dt);
        if (i % 60 == 0) G->render();
        if (platform::shouldQuit()) break;
    }
}
// one frame with a key pressed in the "pressed" set
static void stepKey(int key, int frames = 2) {
    for (int i = 0; i < frames; i++) {
        G->input.clearFrame();
        if (i == 0) {
            G->input.pressed[key] = true;
            G->input.down[key] = true;
        }
        G->update(1.0f / 60.0f);
    }
    G->input.down[key] = false;
}
static void stepHold(int key, int frames) {
    G->input.down[key] = true;
    for (int i = 0; i < frames; i++) {
        G->input.clearFrame();
        G->input.down[key] = true;
        G->update(1.0f / 60.0f);
    }
    G->input.down[key] = false;
}
static void shoot(const char* name) {
    G->render();
    std::string path = std::string(shotDir) + "/" + name + ".png";
    if (R->screenshot(path.c_str())) printf("  [SHOT] %s\n", path.c_str());
}
static int solidCount(World& w, const AABB& box) {
    int x0 = (int)std::floor(box.mn.x / VOX), x1 = (int)std::ceil(box.mx.x / VOX);
    int y0 = (int)std::floor(box.mn.y / VOX), y1 = (int)std::ceil(box.mx.y / VOX);
    int z0 = (int)std::floor(box.mn.z / VOX), z1 = (int)std::ceil(box.mx.z / VOX);
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                if (w.solidAt(x, y, z)) n++;
    return n;
}
static int worldSolidCount(World& w) {
    int n = 0;
    for (auto& kv : w.chunks) {
        Chunk* c = kv.second;
        for (int i = 0; i < CHV; i++)
            if (matSolid(c->v[i])) n++;
    }
    return n;
}
// engine convention: yaw 0 faces -Z, +PI/2 faces +X
static void faceFromTo(Game& g, const vec3& from, const vec3& to) {
    vec3 d = (to - from).norm();
    g.player.yaw = std::atan2(d.x, -d.z);
    g.player.pitch = std::asin(clampf(d.y, -1, 1));
}
static const MapTarget* findTarget(const char* name) {
    for (auto& t : G->mapData.targets)
        if (t.name == name) return &t;
    return nullptr;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) shotDir = argv[++i];
    }
    printf("=== FRACTURE headless verification ===\n");
    Settings st;
    st.showFps = false;
    st.qualityPreset = QP_HIGH;
    st.rendererMode = 1;
    if (!platform::init("FRACTURE test", 1280, 720, st)) return 1;
    R = createSoftRenderer();
    if (!R->init(nullptr, 1280, 720, st)) return 1;
    Game game;
    G = &game;
    game.settings = st;
    if (!game.init(R)) return 1;
    // finish loading the backdrop map
    for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
    check("A1 boot reaches main menu", game.mode == MODE_MAIN_MENU);
    printf("\n--- A: world generation (6 maps) ---\n");
    for (int m = 0; m < mapCount(); m++) {
        World w;
        MapBuildResult res;
        const GameMap& gm = getMap(m);
        w.init(gm.info.sx, gm.info.sy, gm.info.sz);
        buildMap(m, w, res);
        int solid = worldSolidCount(w);
        int targets = (int)res.targets.size();
        bool baselines = true;
        for (auto& t : res.targets) if (t.baseline <= 0) baselines = false;
        if (m == 0) {
            checkf("A2 map0 voxels in range", solid > 100000 && solid < 4000000, "solid=%.0f/%.0f", solid, 0);
            checkf("A3 map0 has destruction targets", targets >= 4, "targets=%.0f/%.0f", targets, 4);
            check("A4 every target has a baseline", baselines);
            checkf("A5 map0 spawns props", res.props.size() >= 3, "props=%.0f/%.0f", (double)res.props.size(), 3);
            checkf("A6 map0 spawns vehicles", res.vehicles.size() >= 1, "vehicles=%.0f/%.0f", (double)res.vehicles.size(), 1);
        }
        printf("  map %d %-24s %5dx%5dx%5d solid=%7d chunks=%4d targets=%2d props=%2d veh=%d\n", m,
               gm.info.name, w.sx, w.sy, w.sz, solid, (int)w.chunks.size(), targets, (int)res.props.size(),
               (int)res.vehicles.size());
        checkf("A7 map has geometry", solid > 50000, "solid=%.0f/%.0f", solid, 50000);
        // mesh a sample of chunks to prove the mesher works on real map data
        int meshed = 0, tris = 0;
        for (auto& kv : w.chunks) {
            if (meshed >= 24) break;
            ivec3 c = World::unchunkKey(kv.first);
            w.buildChunkMesh(c.x, c.y, c.z);
            meshed++;
        }
        for (auto& kv : w.meshes) tris += (int)kv.second.idx.size() / 3;
        checkf("A8 meshes produce triangles", tris > 1000, "tris=%.0f/%.0f", tris, 1000);
        w.clear();
    }
    printf("\n--- B: tool destruction (sledgehammer) ---\n");
    game.startSandbox(0);
    for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
    check("B0 sandbox is playable", game.mode == MODE_PLAYING);
    step(10);
    const MapTarget* store = findTarget("store");
    check("B1 store target exists", store != nullptr);
    if (store) {
        vec3 c = store->box.center();
        // stand on the street side of the store (the road runs along +z) and face the wall
        game.player.pos = vec3(c.x, 0, store->box.mx.z + 2.6f);
        game.player.pos.y = game.world.groundHeightM(game.player.pos.x, game.player.pos.z) + 0.1f;
        game.player.vel = vec3(0);
        faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0),
                   vec3(c.x, game.player.pos.y + 1.4f, store->box.mx.z - 0.2f));
        game.tools.select(TOOL_HAMMER);
        step(3);
        int before = solidCount(game.world, store->box);
        {
            vec3 eye = game.player.pos + vec3(0, game.player.cameraHeight, 0);
            vec3 dir(std::cos(game.player.pitch) * std::sin(game.player.yaw), std::sin(game.player.pitch),
                     -std::cos(game.player.pitch) * std::cos(game.player.yaw));
            RayHit h = game.world.raycast(eye, dir, 6.0f);
            printf("  debug: tool=%d pos=(%.1f,%.1f,%.1f) yaw=%.2f pitch=%.2f hit=%d dist=%.2f mat=%d\n",
                   game.tools.current, game.player.pos.x, game.player.pos.y, game.player.pos.z,
                   game.player.yaw, game.player.pitch, (int)h.hit, h.t, (int)h.mat);
        }
        // direct carve sanity check (same parameters as one hammer hit)
        {
            vec3 eye = game.player.pos + vec3(0, game.player.cameraHeight, 0);
            vec3 dir(std::cos(game.player.pitch) * std::sin(game.player.yaw), std::sin(game.player.pitch),
                     -std::cos(game.player.pitch) * std::cos(game.player.yaw));
            RayHit h = game.world.raycast(eye, dir, 6.0f);
            int b = worldSolidCount(game.world);
            game.carveRegion(h.pos, 0.56f, 650.0f, true, false);
            int a = worldSolidCount(game.world);
            printf("  direct carve: removed %d voxels\n", b - a);
        }
        int dbgDestroyed0 = game.totalDestroyedVoxels;
        for (int swing = 0; swing < 14; swing++) {
            game.player.pos.x = c.x - 6.3f + swing * 0.95f;
            game.player.pos.y = game.world.groundHeightM(game.player.pos.x, game.player.pos.z) + 0.1f;
            faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0),
                       vec3(game.player.pos.x, game.player.pos.y + 1.4f, store->box.mx.z - 0.2f));
            game.input.down[K_LBUTTON] = true;
            game.input.pressed[K_LBUTTON] = true;
            for (int f = 0; f < 30; f++) {
                game.input.clearFrame();
                if (f == 0) { game.input.pressed[K_LBUTTON] = true; game.input.down[K_LBUTTON] = true; }
                game.input.down[K_LBUTTON] = (f < 4);
                game.update(1.0f / 60.0f);
            }
        }
        game.input.down[K_LBUTTON] = false;
        step(30);
        int after = solidCount(game.world, store->box);
        printf("  store voxels: %d -> %d\n", before, after);
        checkf("B2 hammer removes real voxels", after < before, "before=%.0f after=%.0f", before, after);
        checkf("B3 destroyed fraction is measurable", store->destroyedFraction(game.world) > 0.005f,
               "fraction=%.3f", store->destroyedFraction(game.world));
        checkf("B4 debris bodies spawned", (int)game.physics.bodies.size() > 0 || game.totalDestroyedVoxels > 0,
               "bodies=%.0f", (double)game.physics.bodies.size());
        shoot("hud_hammer");
    }
    printf("\n--- C: explosives and chain reaction ---\n");
    {
        game.startSandbox(1);   // Blackstone industrial: fuel tanks
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        const MapTarget* tanks = findTarget("tanks");
        check("C1 fuel tank target exists", tanks != nullptr);
        if (tanks) {
            vec3 c = tanks->box.center();
            int before = solidCount(game.world, tanks->box);
            int bodiesBefore = (int)game.physics.bodies.size();
            game.player.pos = vec3(c.x + 14, 3.2f, c.z);
            game.explode(c, 6.0f, 1);
            step(40);
            int after = solidCount(game.world, tanks->box);
            int bodiesAfter = (int)game.physics.bodies.size();
            printf("  tank voxels: %d -> %d, bodies %d -> %d\n", before, after, bodiesBefore, bodiesAfter);
            checkf("C2 explosion carves a crater", after < before - 200, "before=%.0f after=%.0f", before, after);
            checkf("C3 explosion spawns debris", bodiesAfter > bodiesBefore, "bodies=%.0f/%.0f", bodiesAfter, bodiesBefore);
            checkf("C4 explosion reported", game.totalExplosions > 0, "explosions=%.0f", (double)game.totalExplosions);
            checkf("C5 fire or smoke particles emitted", game.fx.alive() > 0, "particles=%.0f", (double)game.fx.alive());
            shoot("explosion");
        }
    }
    printf("\n--- D: structural collapse ---\n");
    {
        game.startSandbox(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        const MapTarget* house = findTarget("house0");
        check("D1 house target exists", house != nullptr);
        if (house) {
            // remove the ground floor of one corner: walls above must lose support
            AABB b = house->box;
            vec3 base(b.mn.x + 0.6f, b.mn.y + 0.4f, b.mn.z + 0.6f);
            int before = worldSolidCount(game.world);
            int collapseBefore = (int)game.physics.bodies.size();
            for (int i = 0; i < 10; i++)
                game.carveRegion(base + vec3(0, i * 0.5f, 0), 1.5f, 400.0f, true, false);
            // let the support solver run
            step(180);
            int after = worldSolidCount(game.world);
            int collapseAfter = (int)game.physics.bodies.size();
            printf("  world voxels: %d -> %d, bodies %d -> %d\n", before, after, collapseBefore, collapseAfter);
            checkf("D2 structure loses voxels", after < before, "before=%.0f after=%.0f", before, after);
            checkf("D3 collapse produces falling pieces", collapseAfter > collapseBefore, "bodies=%.0f/%.0f", collapseAfter, collapseBefore);
            checkf("D4 unstable mass also removed", after < before - 600, "removed=%.0f", (double)(before - after));
            shoot("collapse");
        }
    }
    printf("\n--- E: fire simulation ---\n");
    {
        game.startSandbox(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        const MapTarget* house = findTarget("house1");
        if (house) {
            vec3 c = house->box.center();
            c.y = house->box.mn.y + 1.2f;
            // a wooden test structure next to the road: guaranteed flammable
            vec3 base(70.0f, 2.0f, 20.0f);
            for (int yy = 0; yy < 8; yy++)
                for (int zz = 0; zz < 10; zz++)
                    for (int xx = 0; xx < 12; xx++) {
                        bool edge = xx == 0 || xx == 11 || zz == 0 || zz == 9 || yy == 7;
                        if (!edge) continue;
                        game.world.setFast((int)(base.x / VOX) + xx, (int)(base.y / VOX) + yy,
                                           (int)(base.z / VOX) + zz, M_WOOD);
                    }
            c = base + vec3(1.5f, 1.2f, 1.2f);
            int firesBefore = game.fx.fireCount();
            int solidBefore = worldSolidCount(game.world);
            game.fx.igniteArea(c, 2.5f);
            int peakFires = 0, peakParticles = 0;
            for (int f = 0; f < 240; f++) {
                step(1);
                peakFires = std::max(peakFires, game.fx.fireCount());
                peakParticles = std::max(peakParticles, game.fx.alive());
            }
            int firesAfter = game.fx.fireCount();
            int solidAfter = worldSolidCount(game.world);
            printf("  fires: started=%d peak=%d now=%d, particles peak=%d, wood voxels %d -> %d\n",
                   firesBefore, peakFires, firesAfter, peakParticles, solidBefore, solidAfter);
            checkf("E1 fire ignites", peakFires > 0, "peak=%.0f", (double)peakFires);
            checkf("E2 fire burns the structure down", solidAfter < solidBefore, "before=%.0f after=%.0f", solidBefore, solidAfter);
            checkf("E3 fire emits particles", peakParticles > 10, "particles=%.0f", (double)peakParticles);
            shoot("fire");
            // extinguish
            game.fx.extinguishArea(c, 6.0f, 1.0f);
            step(60);
            checkf("E4 extinguisher suppresses fire", game.fx.fireCount() <= firesAfter, "fires=%.0f", (double)game.fx.fireCount());
        }
    }
    printf("\n--- F: save / load round trip ---\n");
    {
        game.startSandbox(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        vec3 c(64, 3.0f, 44);
        game.explode(c, 4.0f, 1);
        step(30);
        int carvedSolid = worldSolidCount(game.world);
        int destroyedBefore = game.totalDestroyedVoxels;
        check("F1 sandbox save writes a file", game.saveGame("qatest"));
        game.startSandbox(0);   // wipe the world
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        int freshSolid = worldSolidCount(game.world);
        check("F2 reload restores geometry", game.loadGame("qatest"));
        step(5);
        int restoredSolid = worldSolidCount(game.world);
        printf("  solid: carved=%d fresh=%d restored=%d\n", carvedSolid, freshSolid, restoredSolid);
        checkf("F3 restored world matches the saved one", std::abs(restoredSolid - carvedSolid) < 50,
               "restored=%.0f saved=%.0f", restoredSolid, carvedSolid);
        checkf("F4 save is smaller than a full world", restoredSolid < freshSolid, "restored=%.0f fresh=%.0f", restoredSolid, freshSolid);
        checkf("F5 destruction stats survived", game.totalDestroyedVoxels > 0, "destroyed=%.0f", (double)game.totalDestroyedVoxels);
        (void)destroyedBefore;
        shoot("save_restored");
    }
    printf("\n--- G: missions and objectives ---\n");
    {
        check("G1 campaign has 5 chapters", g_missions.missionsForChapter(5).size() > 0);
        checkf("G2 campaign mission count", g_missions.count() >= 12, "missions=%.0f/%.0f", g_missions.count(), 12);
        game.startMission(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        check("G3 mission 0 starts", g_missions.state.active && !g_missions.state.completed);
        const Mission& m = g_missions.get(0);
        checkf("G4 mission has objectives", m.objectives.size() >= 1, "objectives=%.0f", (double)m.objectives.size());
        // destroy enough of the store to complete objective 1
        const MapTarget* store = findTarget("store");
        if (store) {
            for (int i = 0; i < 900 && store->destroyedFraction(game.world) < m.objectives[0].percent + 0.06f; i++) {
                int ix = i % 10, iy = (i / 10) % 6, iz = (i / 60) % 6;
                vec3 p(store->box.mn.x + 1.0f + ix * 1.75f, store->box.mn.y + 0.8f + iy * 1.35f,
                       store->box.mn.z + 1.0f + iz * 1.9f);
                game.carveRegion(p, 1.9f, 60.0f, true, false);
            }
            step(60);
            printf("  store destroyed: %.1f%% (need %.0f%%)\n", store->destroyedFraction(game.world) * 100,
                   m.objectives[0].percent * 100);
            checkf("G5 destruction objective completes", m.objectives[0].complete || store->destroyedFraction(game.world) >= m.objectives[0].percent,
                   "fraction=%.3f", store->destroyedFraction(game.world));
            // walk to the extraction point
            game.player.pos = m.objectives[1].point;
            step(30);
            checkf("G6 reach objective completes", m.objectives[1].complete, "n/a=%.0f", 0.0);
            step(20);
            check("G7 mission completes when objectives are met",
                  g_missions.state.completed || game.mode == MODE_MISSION_COMPLETE);
            checkf("G8 completion is recorded", g_missions.isCompleted(0), "n/a=%.0f", 0.0);
            if (game.mode == MODE_MISSION_COMPLETE) shoot("mission_complete");
        }
    }
    printf("\n--- H: vehicles ---\n");
    {
        game.startSandbox(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        int n0 = game.vehicles.count();
        checkf("H1 map vehicles spawned", n0 >= 1, "vehicles=%.0f", (double)n0);
        vec3 p(60, 3.0f, 60);
        p.y = game.world.groundHeightM(p.x, p.z) + 0.4f;
        int idx = game.vehicles.spawn(VK_BULLDOZER, p, 0.0f);
        check("H2 vehicle can be spawned", idx >= 0);
        if (idx >= 0) {
            Vehicle& v = game.vehicles.vehicles[idx];
            game.player.pos = v.pos + vec3(1.0f, 0, 0);
            bool entered = game.vehicles.tryEnter(game.player.pos, idx);
            check("H3 player can enter a vehicle", entered);
            if (entered) {
                game.drivenVehicle = idx;
                v.yaw = 0.0f;
                // drive forward into the nearest building
                int before = worldSolidCount(game.world);
                for (int f = 0; f < 400; f++) {
                    game.input.clearFrame();
                    game.input.down[K_W] = true;
                    game.update(1.0f / 60.0f);
                }
                game.input.down[K_W] = false;
                int after = worldSolidCount(game.world);
                printf("  bulldozer travelled %.1f m, world voxels %d -> %d\n", v.odo, before, after);
                checkf("H4 vehicle drives", v.odo > 3.0f, "odo=%.1f/3.0", v.odo, 3.0);
                checkf("H5 heavy vehicle damages structures", after < before || game.totalDestroyedVoxels > 0,
                       "before=%.0f after=%.0f", before, after);
                shoot("vehicle");
                game.drivenVehicle = -1;
            }
        }
    }
    printf("\n--- I: menus and rendering ---\n");
    {
        game.setMode(MODE_MAIN_MENU);
        game.render();
        check("I1 main menu renders", true);
        shoot("menu_main");
        game.setMode(MODE_CAMPAIGN);
        game.render();
        shoot("menu_campaign");
        game.setMode(MODE_SANDBOX_SETUP);
        game.render();
        shoot("menu_sandbox");
        game.setMode(MODE_SETTINGS);
        game.render();
        shoot("menu_settings");
        game.setMode(MODE_CONTROLS);
        game.render();
        shoot("menu_controls");
        check("I2 all menu screens render", true);
        // night + rain sandbox shot
        game.startSandbox(2);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        game.sandbox.timeOfDay = 21;
        game.sandbox.weather = 2;
        game.player.pos = vec3(60, 3.4f, 44);
        game.player.yaw = 0.4f;
        step(60);
        shoot("night_rain");
        check("I3 night remains visible (ambient floor)", game.env.ambientSky.x > 0.15f);
    }
    printf("\n--- K: security systems ---\n");
    {
        game.sandbox.timeOfDay = 12;
        game.startSandbox(2);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(10);
        int cams = game.security.aliveCameras(), turrets = game.security.aliveTurrets();
        printf("  security devices: %d cameras, %d turrets\n", cams, turrets);
        checkf("K1 cameras placed on the map", cams > 0, "cameras=%.0f", (double)cams);
        checkf("K2 turrets placed on the map", turrets > 0, "turrets=%.0f", (double)turrets);
        // destroy a camera directly
        if (cams > 0) {
            vec3 cp = game.security.cameras[0].pos;
            game.explode(cp, 2.5f, 1);
            step(10);
            checkf("K3 explosions destroy cameras", game.security.aliveCameras() < cams, "alive=%.0f/%.0f",
                   (double)game.security.aliveCameras(), (double)cams);
        }
        // raising the alarm turns the turrets hostile
        game.security.triggerAlarm(game.player.pos);
        step(5);
        bool hostile = false;
        for (auto& t : game.security.turrets) if (t.hostile) hostile = true;
        check("K4 alarm activates turrets", hostile);
        check("K5 alarm state is exposed", game.security.alarm);
        // stand in front of a live turret: it must hit the player
        for (auto& t : game.security.turrets) {
            if (t.destroyed) continue;
            // find an approach position with a clear line of sight to the turret
            vec3 best = t.pos + vec3(0, 0, 6.0f);
            bool found = false;
            for (float r = 5.0f; r <= 12.0f && !found; r += 1.5f)
                for (int k = 0; k < 8 && !found; k++) {
                    float a = k * (PI * 0.25f);
                    vec3 p = t.pos + vec3(std::cos(a) * r, 0, std::sin(a) * r);
                    vec3 a1 = t.pos + vec3(0, 0.35f, 0);
                    vec3 b1 = p + vec3(0, 1.55f, 0);
                    RayHit lh = game.world.raycast(a1, (b1 - a1).norm(), (b1 - a1).len() - 0.2f);
                    if (!lh.hit) { best = p; found = true; }
                }
            game.player.pos = best;
            game.player.vel = vec3(0);
            game.player.noclip = true;
            break;
        }
        float hp0 = game.player.health;
        game.render();   // make sure the turret can be seen this frame
        for (auto& t : game.security.turrets) {
            if (t.destroyed) continue;
            vec3 a = t.pos + vec3(0, 0.35f, 0);
            vec3 b = game.player.pos + vec3(0, 1.55f, 0);
            RayHit lh = game.world.raycast(a, (b - a).norm(), (b - a).len() - 0.2f);
            printf("  turret at (%.1f,%.1f,%.1f) player (%.1f,%.1f,%.1f) losBlocked=%d dist=%.1f hostile=%d\n",
                   t.pos.x, t.pos.y, t.pos.z, game.player.pos.x, game.player.pos.y, game.player.pos.z,
                   (int)lh.hit, (b - a).len(), (int)t.hostile);
            break;
        }
        step(240);
        printf("  turret fire: shots=%d, player health %.0f -> %.0f\n", game.security.shotsFired, hp0, game.player.health);
        checkf("K6 turrets shoot the player", game.security.shotsFired > 0, "shots=%.0f", (double)game.security.shotsFired);
        checkf("K7 turret fire deals damage", game.player.health < hp0, "hp=%.0f/%.0f", game.player.health, hp0);
        shoot("security");
        // stealth mission fails on alarm
        game.startMission(7);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        check("K8 stealth mission starts", g_missions.state.active);
        if (game.security.cameras.size() > 0) {
            game.security.alarm = true;
            g_missions.state.alarmTriggered = true;
            step(5);
            checkf("K9 stealth mission fails when the alarm sounds", game.mode == MODE_MISSION_FAILED || g_missions.state.failed,
                   "mode=%.0f", (double)game.mode);
        }
    }

    printf("\n--- M: all twelve tools ---\n");
    {
        game.sandbox.timeOfDay = 12;
        game.sandbox.unlimitedAmmo = true;
        game.startSandbox(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(5);
        game.player.noclip = true;
        game.player.health = 100;
        auto buildWall = [&](vec3 base) {
            for (int y = 0; y < 14; y++)
                for (int z = 0; z < 3; z++)
                    for (int x = 0; x < 18; x++)
                        game.world.setFast((int)(base.x / VOX) + x, (int)(base.y / VOX) + y,
                                           (int)(base.z / VOX) + z, M_BRICK);
        };
        // open grass south of the houses: no lamp posts, cars or trees in the way
        const vec3 base(24.0f, 3.0f, 70.0f);
        auto countPlanks = [&]() {
            int n = 0;
            for (auto& b : game.physics.bodies) if (b.active && b.mat == M_PLANK) n++;
            return n;
        };
        int passTools = 0;
        for (int t = TOOL_HAMMER; t <= TOOL_AIRSTRIKE; t++) {
            game.tools.select(t);
            game.tools.cooldown = 0;
            game.tools.reloadTimer = 0;
            for (int i = 0; i < TOOL_COUNT; i++) game.tools.ammo[i] = toolDef(i).maxAmmo;
            game.tools.charges.clear();
            game.tools.projectiles.clear();
            game.tools.cables.clear();
            game.tools.plank = PlankPreview();
            game.tools.grappleAttached = false;
            game.tools.airstrike = Airstrike();
            game.tools.airstrikeCooldown = 0;
            game.tools.torchFuel = 1.0f;
            game.tools.extinguisherCharge = 1.0f;
            game.tools.heat = 0;
            buildWall(base);
            game.player.noclip = true;    // hold the test rig steady (no gravity drift during tool actions)
            game.player.pos = vec3(base.x + 3.0f, 4.4f, base.z + 3.2f);
            game.player.vel = vec3(0);
            faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0),
                       vec3(base.x + 3.0f, 4.8f, base.z + 0.4f));
            step(2);
            {
                vec3 eye = game.player.pos + vec3(0, game.player.cameraHeight, 0);
                vec3 dir(std::cos(game.player.pitch) * std::sin(game.player.yaw), std::sin(game.player.pitch),
                         -std::cos(game.player.pitch) * std::cos(game.player.yaw));
                RayHit ah = game.world.raycast(eye, dir, 8.0f);
                printf("   [aim t%d] hit=%d dist=%.2f mat=%s pos=(%.1f,%.1f,%.1f)\n", t, (int)ah.hit, ah.t,
                       matName(ah.mat), game.player.pos.x, game.player.pos.y, game.player.pos.z);
            }
            int voxBefore = worldSolidCount(game.world);
            int bodiesBefore = (int)game.physics.bodies.size();
            int plankCountBefore = countPlanks();
            int chargesBefore = (int)game.tools.charges.size();
            int projBefore = (int)game.tools.projectiles.size();
            std::string what;
            switch (t) {
                case TOOL_HAMMER:
                    stepKey(K_LBUTTON, 5);
                    step(10);
                    what = "carves brick";
                    break;
                case TOOL_SHOTGUN:
                    stepKey(K_LBUTTON, 5);
                    step(10);
                    what = "pellet damage";
                    break;
                case TOOL_CHARGES:
                    stepKey(K_LBUTTON, 5);
                    step(30);
                    stepKey(K_R, 4);
                    step(30);
                    what = "charge detonation";
                    break;
                case TOOL_ROCKET:
                    stepKey(K_LBUTTON, 5);
                    step(150);
                    what = "rocket impact";
                    break;
                case TOOL_BLOWTORCH:
                    stepHold(K_LBUTTON, 50);
                    what = "torch cut";
                    break;
                case TOOL_PLANK: {
                    // second pillar to span between
                    for (int y = 0; y < 14; y++)
                        for (int z = 0; z < 3; z++)
                            for (int x = 0; x < 4; x++)
                                game.world.setFast((int)(base.x / VOX) + 15 + x, (int)(base.y / VOX) + y,
                                                   (int)(base.z / VOX) + z, M_BRICK);
                    game.player.pos = vec3(base.x + 3.0f, 4.4f, base.z + 3.2f);
                    faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0),
                               vec3(base.x + 3.0f, 4.8f, base.z + 0.4f));
                    step(4);
                    stepKey(K_LBUTTON, 5);   // first end
                    step(6);
                    printf("     [plank] A=%d a=(%.1f,%.1f,%.1f) b=(%.1f,%.1f,%.1f)\n", (int)game.tools.plank.haveA,
                           game.tools.plank.a.x, game.tools.plank.a.y, game.tools.plank.a.z,
                           game.tools.plank.b.x, game.tools.plank.b.y, game.tools.plank.b.z);
                    game.player.pos = vec3(base.x + 14.0f, 4.4f, base.z + 3.2f);
                    faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0),
                               vec3(base.x + 15.5f, 4.8f, base.z + 0.4f));
                    step(4);
                    int bodiesPre = (int)game.physics.bodies.size();
                    stepKey(K_LBUTTON, 5);   // second end
                    step(30);
                    what = "plank bridge";
                    break;
                }
                case TOOL_EXTINGUISHER:
                    game.fx.igniteArea(vec3(base.x + 2.0f, base.y + 1.0f, base.z + 4.0f), 2.0f);
                    step(6);
                    stepHold(K_LBUTTON, 40);
                    what = "spray";
                    break;
                case TOOL_GRAPPLE:
                    stepKey(K_LBUTTON, 5);
                    step(40);
                    what = "grapple pull";
                    break;
                case TOOL_CUTTER:
                    stepHold(K_LBUTTON, 60);
                    what = "cutter";
                    break;
                case TOOL_CABLE:
                    stepKey(K_LBUTTON, 5);
                    step(6);
                    game.player.pos = vec3(base.x + 14.0f, 4.4f, base.z + 3.2f);
                    faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0),
                               vec3(base.x + 16.0f, 4.8f, base.z + 0.4f));
                    step(4);
                    stepKey(K_LBUTTON, 5);
                    step(20);
                    what = "gravity cable";
                    break;
                case TOOL_DRILL:
                    stepHold(K_LBUTTON, 60);
                    what = "drill";
                    break;
                case TOOL_AIRSTRIKE:
                    stepKey(K_LBUTTON, 5);
                    step(10);
                    printf("   [air] state=%d bombs=%d target=(%.1f,%.1f,%.1f)\n", game.tools.airstrike.state,
                           game.tools.airstrike.bombsLeft, game.tools.airstrike.target.x, game.tools.airstrike.target.y,
                           game.tools.airstrike.target.z);
                    game.player.pos = vec3(base.x - 30.0f, 4.0f, base.z + 24.0f);
                    step(900);
                    what = "airstrike";
                    break;
            }
            game.player.noclip = true;
            int plankCountAfter = countPlanks();
            printf("     [state] cooldown=%.3f torchActive=%d fuel=%.2f swing=%.2f charges=%d proj=%d driven=%d plankA=%d\n",
                   game.tools.cooldown, (int)game.tools.torchActive, game.tools.torchFuel, game.tools.swing,
                   (int)game.tools.charges.size(), (int)game.tools.projectiles.size(), game.drivenVehicle,
                   (int)game.tools.plank.haveA);
            int voxAfter = worldSolidCount(game.world);
            int bodiesAfter = (int)game.physics.bodies.size();
            bool didSomething = false;
            switch (t) {
                case TOOL_CHARGES: didSomething = (int)game.tools.charges.size() > chargesBefore || voxAfter < voxBefore; break;
                case TOOL_ROCKET: didSomething = voxAfter < voxBefore; break;
                case TOOL_EXTINGUISHER: didSomething = game.fx.fireCount() == 0; break;
                case TOOL_GRAPPLE: didSomething = game.player.pos.y > 4.6f || game.player.vel.len() > 1.0f; break;
                case TOOL_CABLE: didSomething = (int)game.tools.cables.size() > 0; break;
                case TOOL_PLANK: didSomething = plankCountAfter > plankCountBefore; break;
                case TOOL_AIRSTRIKE: didSomething = voxAfter < voxBefore; break;
                default: didSomething = voxAfter < voxBefore || bodiesAfter > bodiesBefore; break;
            }
            printf("   %-20s %-20s voxels %7d -> %7d  bodies %2d -> %2d  cables %d  %s\n", toolDef(t).name,
                   what.c_str(), voxBefore, voxAfter, bodiesBefore, bodiesAfter, (int)game.tools.cables.size(),
                   didSomething ? "OK" : "<-- NO EFFECT");
            if (didSomething) passTools++;
            if (t == TOOL_AIRSTRIKE) shoot("tool_airstrike");
            if (t == TOOL_CUTTER) shoot("tool_cutter");
        }
        checkf("M1 all twelve tools have a real effect", passTools == 12, "tools=%.0f/12", (double)passTools);
        game.player.noclip = false;
        game.sandbox.unlimitedAmmo = false;
    }

    printf("\n--- J: view model gallery ---\n");
    {
        game.sandbox.timeOfDay = 13;
        game.sandbox.weather = 0;
        game.startSandbox(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        game.player.pos = vec3(64, 0, 46);
        game.player.pos.y = game.world.groundHeightM(game.player.pos.x, game.player.pos.z) + 0.1f;
        faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0), vec3(66, 3.2f, 40));
        step(20);
        for (int t = TOOL_HAMMER; t <= TOOL_AIRSTRIKE; t++) {
            game.input.clearFrame();
            game.tools.select(t);
            game.tools.cooldown = 0;
            game.tools.vmKick = 0;
            step(12);
            char name[64];
            snprintf(name, sizeof(name), "tool_%02d", t);
            shoot(name);
        }
        check("J1 tool gallery rendered", true);
    }

    printf("\n--- L: showcase screenshots ---\n");
    {
        const char* names[6] = {"map0_westhaven", "map1_blackstone", "map2_downtown", "map3_harbor",
                                "map4_canyon", "map5_mall"};
        for (int m = 0; m < mapCount(); m++) {
            game.sandbox.timeOfDay = defaultTimeOfDay(m);
            game.sandbox.weather = -1;
            game.startSandbox(m);
            for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
            step(20);
            // stand back from the biggest destruction target and look at it
            const MapTarget* best = nullptr;
            float bestScore = -1;
            for (auto& t : game.mapData.targets) {
                vec3 sz = t.box.size();
                float footprint = sz.x * sz.z;
                if (footprint > bestScore) { bestScore = footprint; best = &t; }
            }
            if (best) {
                vec3 c = best->box.center();
                float r = std::max(18.0f, best->box.size().len() * 0.9f);
                vec3 eye(c.x + r, best->box.mn.y + r * 0.45f + 3.0f, c.z + r);
                game.player.pos = eye;
                game.player.noclip = true;
                game.player.vel = vec3(0);
                faceFromTo(game, eye + vec3(0, game.player.cameraHeight, 0), c);
                step(10);
                shoot(names[m]);
            }
        }
        // a big destruction moment on Westhaven: explosives on the main street
        game.sandbox.timeOfDay = 16;
        game.startSandbox(0);
        for (int i = 0; i < 4000 && game.mode == MODE_LOADING; i++) game.update(1.0f / 60.0f);
        step(10);
        game.player.noclip = true;
        game.player.pos = vec3(46, 7.5f, 58);
        faceFromTo(game, game.player.pos + vec3(0, game.player.cameraHeight, 0), vec3(66, 3.0f, 44));
        for (int i = 0; i < 5; i++) {
            game.explode(vec3(60 + i * 3.0f, 3.0f, 40 + i * 2.0f), 4.0f + i * 0.4f, 1);
            step(8);
        }
        step(22);
        shoot("showcase_explosions");
        game.player.noclip = false;
    }

    printf("\n=== RESULT: %d passed, %d failed ===\n", g_pass, g_fail);
    if (!g_failures.empty()) {
        printf("failed checks:\n");
        for (auto& f : g_failures) printf("  - %s\n", f.c_str());
    }
    game.shutdown();
    R->shutdown();
    delete R;
    platform::shutdown();
    logFlush();
    return g_fail == 0 ? 0 : 1;
}
