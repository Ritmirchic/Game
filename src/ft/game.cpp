// FRACTURE — game core implementation
#include "game.h"
#include "audio.h"
#include <cstring>
#include <chrono>

namespace ft {

std::string g_dataDir = ".";

// ---------------------------------------------------------------- lifecycle --
bool Game::init(IBackend* backend) {
    rb = backend;
    buildMatGpuTable();
    particlesInit();
    settings.load((g_dataDir + "/ft_settings.ini").c_str());
    screen = SCR_MENU;
    sky = SkyParams();
    timeOfDay = 10.5f;
    particlesSetBudget(particleBudget(settings.effectsQuality));
    physics.maxBodies = debrisBudget(settings.debrisDensity) + 120;
    physics.maxFrags = settings.destructionQuality >= 3 ? 140 : settings.destructionQuality >= 2 ? 96 : settings.destructionQuality == 1 ? 64 : 48;
    physics.bind(&world);
    return true;
}

void Game::shutdown() {
    settings.save((g_dataDir + "/ft_settings.ini").c_str());
    particlesShutdown();
}

void Game::setCamera(Vec3 pos, f32 yawRad, f32 pitchRad) {
    playerPos = pos;
    playerYaw = yawRad;
    playerPitch = pitchRad;
    updateCameraAfterPlayer();
}

void Game::applyLook(f32 dx, f32 dy) {
    playerYaw += dx;
    playerPitch -= dy * (settings.invertY > 0.5f ? -1.f : 1.f);
    const f32 lim = 1.53f;
    playerPitch = tclamp(playerPitch, -lim, lim);
    if (playerYaw > 3.14159265f * 4) playerYaw -= 3.14159265f * 4;
    if (playerYaw < -3.14159265f * 4) playerYaw += 3.14159265f * 4;
}

// ------------------------------------------------------------------- scene ---
static void fillSphere(VoxWorld& w, Vec3 center, f32 radius, u8 mat, u16 col, u8 extra = 0) {
    int x0 = (int)std::floor((center.x - radius) / VOX), x1 = (int)std::ceil((center.x + radius) / VOX);
    int y0 = (int)std::floor((center.y - radius) / VOX), y1 = (int)std::ceil((center.y + radius) / VOX);
    int z0 = (int)std::floor((center.z - radius) / VOX), z1 = (int)std::ceil((center.z + radius) / VOX);
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        f32 cx = (x + 0.5f) * VOX - center.x, cy = (y + 0.5f) * VOX - center.y, cz = (z + 0.5f) * VOX - center.z;
        if (cx * cx + cy * cy + cz * cz <= radius * radius)
            w.set(x, y, z, mat, col, extra, EDIT_SCRIPT, true);
    }
}

static void fillSphereNoise(VoxWorld& w, Vec3 center, f32 radius, u8 mat, u16 col, u32 seed, f32 wob) {
    int x0 = (int)std::floor((center.x - radius) / VOX), x1 = (int)std::ceil((center.x + radius) / VOX);
    int y0 = (int)std::floor((center.y - radius) / VOX), y1 = (int)std::ceil((center.y + radius) / VOX);
    int z0 = (int)std::floor((center.z - radius) / VOX), z1 = (int)std::ceil((center.z + radius) / VOX);
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        f32 cx = (x + 0.5f) * VOX - center.x, cy = (y + 0.5f) * VOX - center.y, cz = (z + 0.5f) * VOX - center.z;
        f32 d = std::sqrt(cx * cx + cy * cy + cz * cz);
        f32 n = fbm2(x * 0.09f + seed, z * 0.09f + y * 0.07f, seed) * wob;
        if (d <= radius * (1.f - n))
            w.set(x, y, z, mat, col, 0, EDIT_SCRIPT, true);
    }
}

u32 Game::addBoxProp(Vec3 pos, Vec3 half, u8 mat, u16 col, f32 massKg, u8 flags) {
    Body b;
    b.pos = pos;
    b.half = half;
    b.mat = mat;
    b.col = col;
    b.mass = massKg;         //0 → auto from box
    b.flags = flags;
    const MatDef& md = matDef(mat);
    b.friction = (md.flags & MF_METAL) ? 0.45f : 0.62f;
    b.restitution = (md.flags & MF_METAL) ? 0.18f : 0.08f;
    return physics.addBody(b);
}

bool Game::buildScene(const char* name, u64 seed) {
    using std::chrono::high_resolution_clock;
    auto t0 = high_resolution_clock::now();
    sceneSeed = seed;
    currentMapName = name;
    particlesClear();
    physics.clear();
    world.clear();

    bool ok = true;
    int W = 288, H = 72, D = 288;
    if (std::strcmp(name, "test") == 0 || std::strcmp(name, "suburb") == 0) { W = 288; H = 72; D = 288; }
    ok = world.init(W, H, D);
    if (!ok) return false;

    Rng rng(seed);
    auto C = [&](int r, int g, int b) { return rgb565(r, g, b); };

    // ---- ground: soil base, grass-tinted surface ----
    world.fillBox(0, 0, 0, W - 1, 1, D - 1, MAT_SOIL, C(96, 74, 52));
    world.fillBox(0, 2, 0, W - 1, 2, D - 1, MAT_SOIL, C(74, 138, 58));      // grassy top
    world.fillBox(0, 3, 0, W - 1, 3, D - 1, MAT_SOIL, C(68, 146, 56));
    // subtle terrain bumps
    for (int z = 0; z < D; z++) for (int x = 0; x < W; x++) {
        f32 n = fbm2(x * 0.012f, z * 0.012f, seed);
        int bump = (int)(n * 3.4f);
        for (int y = 4; y < 4 + bump; y++) world.set(x, y, z, MAT_SOIL, C(70, 142, 56), 0, EDIT_SCRIPT, true);
    }

    // ---- streets (asphalt cross) ----
    int roadW = 10;
    int rx = W / 2, rz = D / 2;
    world.fillBox(0, 3, rz - roadW / 2, W - 1, 3, rz + roadW / 2, MAT_ASPHALT, C(66, 68, 72));
    world.fillBox(rx - roadW / 2, 3, 0, rx + roadW / 2, 3, D - 1, MAT_ASPHALT, C(66, 68, 72));
    // lane markings
    for (int x = 2; x < W - 2; x += 6)
        for (int d = -1; d <= 1; d += 2)
            world.fillBox(x, 4, rz + d * 2, x + 3, 4, rz + d * 2, MAT_ASPHALT, C(210, 200, 130));
    for (int z = 2; z < D - 2; z += 6)
        for (int d = -1; d <= 1; d += 2)
            world.fillBox(rx + d * 2, 4, z, rx + d * 2, 4, z + 3, MAT_ASPHALT, C(210, 200, 130));
    // sidewalks
    world.fillBox(0, 3, rz - roadW / 2 - 3, W - 1, 4, rz - roadW / 2 - 1, MAT_CONCRETE, C(158, 158, 152));
    world.fillBox(0, 3, rz + roadW / 2 + 1, W - 1, 4, rz + roadW / 2 + 3, MAT_CONCRETE, C(158, 158, 152));
    world.fillBox(rx - roadW / 2 - 3, 3, 0, rx - roadW / 2 - 1, 4, D - 1, MAT_CONCRETE, C(158, 158, 152));
    world.fillBox(rx + roadW / 2 + 1, 3, 0, rx + roadW / 2 + 3, 4, D - 1, MAT_CONCRETE, C(158, 158, 152));

    const int GY = 5;   // floor level (first air above sidewalk surface)

    // ---- helper: build a house with interior ----
    auto buildHouse = [&](int x0, int z0, int w, int d, int floors, u16 wallCol, u8 wallMat, u16 roofCol) {
        int wallH = floors * 11 + (floors - 1) * 2;  // slab between floors
        int y0 = GY;
        int y1 = y0 + wallH;
        int doorX = x0 + w / 2 - 1;
        // floor slab
        world.fillBox(x0, y0 - 1, z0, x0 + w, y0 - 1, z0 + d, MAT_WOOD, C(140, 104, 64));
        // outer walls (2 vox thick)
        for (int f = 0; f < floors; f++) {
            int fy0 = y0 + f * 13;
            int fy1 = tmin(y1, fy0 + 11);
            // 4 walls
            world.fillBox(x0, fy0, z0, x0 + w, fy1, z0 + 1, wallMat, wallCol);
            world.fillBox(x0, fy0, z0 + d - 1, x0 + w, fy1, z0 + d, wallMat, wallCol);
            world.fillBox(x0, fy0, z0 + 2, x0 + 1, fy1, z0 + d - 2, wallMat, wallCol);
            world.fillBox(x0 + w - 1, fy0, z0 + 2, x0 + w, fy1, z0 + d - 2, wallMat, wallCol);
            // windows on front (z0 face) and back
            int winY0 = fy0 + 4, winY1 = tmin(fy1 - 1, fy0 + 8);
            for (int wx = x0 + 3; wx < x0 + w - 3; wx += 6) {
                world.fillBox(wx, winY0, z0, wx + 3, winY1, z0 + 1, MAT_GLASS, C(200, 224, 232));
                world.fillBox(wx, winY0, z0 + d - 1, wx + 3, winY1, z0 + d, MAT_GLASS, C(200, 224, 232));
            }
            for (int wz = z0 + 3; wz < z0 + d - 3; wz += 6) {
                world.fillBox(x0, winY0, wz, x0 + 1, winY1, wz + 3, MAT_GLASS, C(200, 224, 232));
                world.fillBox(x0 + w - 1, winY0, wz, x0 + w, winY1, wz + 3, MAT_GLASS, C(200, 224, 232));
            }
            // door opening front side (centered) — cut after wall fill
            world.fillBox(doorX, fy0, z0, doorX + 2, fy0 + 8, z0 + 1, MAT_AIR, 0);
            // interior partition wall with doorway
            int pz = z0 + d / 2;
            world.fillBox(x0 + 2, fy0, pz, x0 + w - 2, fy1, pz, MAT_PLASTER, C(224, 218, 206));
            world.fillBox(x0 + 4, fy0, pz, x0 + 6, fy0 + 8, pz, MAT_AIR, 0);
            // ceiling / next floor slab
            if (f < floors - 1) {
                world.fillBox(x0, fy1, z0, x0 + w, fy1 + 1, z0 + d, MAT_CONCRETE, C(168, 166, 160));
                // stair hole
                world.fillBox(x0 + w - 7, fy1, z0 + 3, x0 + w - 3, fy1 + 1, z0 + 9, MAT_AIR, 0);
                // stairs (wood ramp steps)
                for (int s = 0; s < 13; s++)
                    world.fillBox(x0 + w - 7, fy1 - 12 + s, z0 + 9 - s, x0 + w - 3, fy1 - 12 + s, z0 + 9 - s, MAT_WOOD, C(150, 112, 70));
            }
        }
        // roof
        world.fillBox(x0 - 1, y1, z0 - 1, x0 + w + 1, y1 + 1, z0 + d + 1, MAT_TILE, roofCol);
        // parapet
        world.fillBox(x0 - 1, y1 + 2, z0 - 1, x0 + w + 1, y1 + 3, z0 - 1, wallMat, wallCol);
        world.fillBox(x0 - 1, y1 + 2, z0 + d + 1, x0 + w + 1, y1 + 3, z0 + d + 1, wallMat, wallCol);
        world.fillBox(x0 - 1, y1 + 2, z0 - 1, x0 - 1, y1 + 3, z0 + d + 1, wallMat, wallCol);
        world.fillBox(x0 + w + 1, y1 + 2, z0 - 1, x0 + w + 1, y1 + 3, z0 + d + 1, wallMat, wallCol);
        // door leaf (physical, hinged)
        Body door;
        door.kind = BK_DOOR;
        door.mat = MAT_WOOD;
        door.col = C(120, 84, 52);
        door.half = Vec3(0.375f, 1.0f, 0.045f);
        door.mass = 22;
        door.hp = door.maxHp = 40;
        door.pos = Vec3((doorX + 1.5f) * VOX, (y0 + 4) * VOX, (z0 + 1) * VOX);
        door.hingePivot = Vec3(doorX * VOX, (y0 + 4) * VOX, (z0 + 1) * VOX);
        door.hingeAxis = Vec3(0, 1, 0);
        door.hingeLocalOffset = Vec3(0.375f + 0.0f, 0, 0);
        door.hingeRot0 = Quat::axisAngle(Vec3(0, 1, 0), 0);
        door.hingeAngle = 0; door.hingeTarget = 0;
        door.flags = BF_HINGED;
        u32 did = physics.addBody(door);
        (void)did;
        (void)roofCol;
    };

    // house1: brick,2 floors — main entry across the road
    buildHouse(rx - 60, rz - 46, 34, 26, 2, C(156, 96, 74), MAT_BRICK, C(120, 64, 56));
    // house2: painted plaster, single floor
    buildHouse(rx + 14, rz - 40, 26, 22, 1, C(226, 220, 202), MAT_PLASTER, C(96, 110, 120));
    // house3: wood-clad small
    buildHouse(rx + 16, rz + 12, 24, 20, 1, C(172, 132, 88), MAT_WOOD, C(110, 80, 60));

    // ---- tower (collapse test):4 pillars + heavy top ----
    {
        int tx = rx - 60, tz = rz + 34;
        for (int i = 0; i < 4; i++) {
            int px = tx + (i % 2 ? 10 : 0), pz = tz + (i >= 2 ? 10 : 0);
            world.fillBox(px, GY, pz, px + 2, GY + 21, pz + 2, MAT_REBAR, C(140, 138, 134));
        }
        world.fillBox(tx - 2, GY + 22, tz - 2, tx + 14, GY + 26, tz + 14, MAT_CONCRETE, C(150, 150, 146));
        // antenna
        world.fillBox(tx + 5, GY + 27, tz + 5, tx + 6, GY + 40, tz + 6, MAT_METAL, C(180, 120, 90));
    }

    // ---- trees ----
    auto tree = [&](int x, int z, int h) {
        world.fillBox(x, GY, z, x + 1, GY + h, z + 1, MAT_WOOD, C(96, 70, 44));
        fillSphereNoise(world, Vec3((x + 1) * VOX, (GY + h + 3) * VOX, (z + 1) * VOX), 2.4f, MAT_LEAF, C(64, 118, 46), (u32)(x * 13 + z), 0.4f);
    };
    tree(18, 20, 6); tree(44, 26, 7); tree(rx + 60, rz - 60, 8); tree(rx + 74, rz - 30, 6);
    tree(rx - 30, rz + 60, 7); tree(rx + 60, rz + 60, 6);

    // ---- props ----
    // crates near sidewalk
    for (int i = 0; i < 6; i++) {
        addBoxProp(Vec3(rx + 40 + (i % 3) * 1.1f, GY * VOX + 0.35f, rz - 10 + (i / 3) * 1.1f),
                   Vec3(0.35f, 0.35f, 0.35f), MAT_CARDBOARD, C(176, 144, 104));
    }
    // metal barrels
    for (int i = 0; i < 4; i++) {
        Body b;
        b.pos = Vec3(rx - 44 + i * 1.2f, GY * VOX + 0.55f, rz + 8);
        b.half = Vec3(0.4f, 0.55f, 0.4f);
        b.mat = MAT_PAINTED;
        b.col = i == 1 ? C(190, 60, 50) : C(70, 110, 150);
        b.mass = 0;
        b.kind = BK_PROP;
        b.hp = b.maxHp = 60;
        if (i == 1) { b.flags |= BF_EXPLOSIVE; b.explosiveForce = 2; b.hp = b.maxHp = 30; }
        physics.addBody(b);
    }
    // furniture inside house1
    {
        int hx = rx - 60, hz = rz - 46;
        addBoxProp(Vec3((hx + 8) * VOX, GY * VOX + 0.4f, (hz + 7) * VOX), Vec3(1.0f, 0.4f, 0.6f), MAT_WOOD, C(140, 100, 60));
        addBoxProp(Vec3((hx + 12) * VOX, GY * VOX + 0.35f, (hz + 18) * VOX), Vec3(0.9f, 0.35f, 0.45f), MAT_FABRIC, C(90, 110, 150));
        addBoxProp(Vec3((hx + 24) * VOX, GY * VOX + 0.5f, (hz + 8) * VOX), Vec3(0.5f, 0.5f, 0.5f), MAT_WOOD, C(150, 112, 70));
    }

    // ---- ground-level details: fences ----
    for (int z = 6; z < rz - roadW / 2 - 6; z += 1) {
        // skip trees area roughly
    }
    {
        // picket fence along a backyard edge
        for (int x = rx - 90; x < rx - 66; x += 2) {
            world.fillBox(x, GY, rz - 62, x + 1, GY + 5, rz - 62, MAT_WOOD, C(190, 178, 150));
        }
        for (int z = rz - 62; z < rz - 44; z += 2) {
            world.fillBox(rx - 90, GY, z, rx - 90, GY + 5, z + 1, MAT_WOOD, C(190, 178, 150));
        }
    }

    // ---- initial remesh of everything (loading) ----
    support.bind(&world);
    support.computeAll();
    {
        int remeshed = 0;
        while (!world.dirtyChunks.empty()) {
            u32 ci = world.dirtyChunks.back();
            world.dirtyChunks.pop_back();
            Chunk& c = world.chunks[ci];
            c.dirty = 0;
            int cx = (int)(ci % (u32)world.CW);
            int cy = (int)((ci / (u32)world.CW) % (u32)world.CHh);
            int cz = (int)(ci / ((u32)world.CW * (u32)world.CHh));
            meshChunk(world, cx, cy, cz, world.meshes[ci]);
            remeshed++;
        }
        logf("scene '%s': remeshed %d chunks", name, remeshed);
    }

    // spawn
    respawnPlayer();
    auto t1 = high_resolution_clock::now();
    logf("scene '%s' built in %lld ms", name, (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    simReady = true;
    return true;
}

void Game::flushDirtyMeshes(int budget) {
    int done = 0;
    while (!world.dirtyChunks.empty() && (budget < 0 || done < budget)) {
        u32 ci = world.dirtyChunks.back();
        world.dirtyChunks.pop_back();
        Chunk& c = world.chunks[ci];
        c.dirty = 0;
        int cx = (int)(ci % (u32)world.CW);
        int cy = (int)((ci / (u32)world.CW) % (u32)world.CHh);
        int cz = (int)(ci / ((u32)world.CW * (u32)world.CHh));
        meshChunk(world, cx, cy, cz, world.meshes[ci]);
        done++;
    }
}

void Game::respawnPlayer() {
    // stand on sidewalk near road center (surface y=5 voxels →1.25 m)
    playerPos = Vec3(world.W * VOX * 0.5f + 8.f, 5.05f, world.D * VOX * 0.5f + 14.f);
    playerVel = Vec3();
    playerYaw = 3.14159f * 1.25f;   // look toward houses
    playerPitch = -0.05f;
    updateCameraAfterPlayer();
}

// ------------------------------------------------------------ simulation -----
void Game::simStep() {
    // support + extraction (unstable sections become fragments)
    std::vector<Cluster> fallen;
    int processed = support.process(settings.physicsQuality >= 2 ? 200000 : 100000, fallen);
    (void)processed;
    for (auto& cl : fallen) {
        Vec3 center((cl.ox + cl.w * 0.5f) * VOX, (cl.oy + cl.h * 0.5f) * VOX, (cl.oz + cl.d * 0.5f) * VOX);
        u32 id = physics.spawnFragment(cl, Vec3(0, -0.2f, 0), Vec3(0, 0, 0));
        if (id) {
            fxDust(center, Vec3(0.66f, 0.63f, 0.6f), tmin(14, cl.count / 12 + 4), 1.4f, 0.7f, 1.2f);
        }
    }
    physics.step(FIXED_DT);
    consumePhysicsEvents();
    updateParticles(FIXED_DT);
}

void Game::consumePhysicsEvents() {
    // chained explosions
    int chain = 0;
    while (!physics.explodes.empty() && chain++ < 8) {
        std::vector<ExplodeEvent> evs;
        evs.swap(physics.explodes);
        for (auto& e : evs) applyExplosion(e);
    }
    // breaks → debris + dust
    if (!physics.breaks.empty()) {
        std::vector<BreakEvent> evs;
        evs.swap(physics.breaks);
        for (auto& e : evs) {
            const MatDef& md = matDef(e.mat);
            Vec3 color(md.baseR / 255.f, md.baseG / 255.f, md.baseB / 255.f);
            fxDust(e.pos, color, 6, 1.4f, 0.5f, 0.8f);
            fxDebrisChips(e.pos, color, 6, 3.5f);
            if ((md.flags & MF_FLAMMABLE) && e.kind != BK_DOOR) {} // nothing
        }
    }
    // impacts → material FX (throttled per frame)
    if (!physics.impacts.empty()) {
        int n = tmin((int)physics.impacts.size(), 6);
        int snd = 0;
        for (int i = 0; i < n; i++) {
            const ImpactEvent& e = physics.impacts[i];
            if (e.energy < 120.f) continue;
            const MatDef& md = matDef(e.mat);
            if (md.flags & MF_METAL) fxSparks(e.pos, e.nrm, 6, 5.f);
            else if (e.mat == MAT_WOOD) fxWoodSplinters(e.pos, e.nrm, 4);
            else fxDustImpact(e.pos, e.nrm, 4);
            if (snd++ < 2) {
                f32 g = tclamp(e.energy / 900.f, 0.15f, 1.f);
                sfxPlay(sfxForImpact(e.mat, e.energy > 400.f), e.pos, g,
                        tclamp(0.85f + e.energy * 0.0004f, 0.85f, 1.35f));
            }
        }
        physics.impacts.clear();
    }
}

void Game::applyExplosion(const ExplodeEvent& e) {
    f32 R = e.force * 0.012f + 1.6f;      // radius from force
    sfxPlay(e.force > 400.f ? SFX_EXPLOSION_L : e.force > 150.f ? SFX_EXPLOSION_M : SFX_EXPLOSION_S,
            e.pos, 1.f, tclamp(1.4f - R * 0.04f, 0.7f, 1.3f));
    sfxPlay(SFX_DEBRIS_RAIN, e.pos, tclamp(R * 0.16f, 0.3f, 1.f), 1.f);
    f32 P = e.force * 0.017f;             // blast power scale
    std::vector<Voxel> removed;
    carveWorld(world, e.pos, R, P, 1.f, EDIT_BLAST, &removed, &physics.rng);
    physics.explosionAt(e.pos, R * 1.15f, e.force, e.level, true);
    physics.wakeNear(e.pos, R * 2.f);
    fxExplosionCore(e.pos, R * 0.65f);
    if (!removed.empty())
        fxDebrisChips(e.pos, Vec3(0.6f, 0.58f, 0.55f), tmin(20, (int)removed.size() / 4 + 6), 7.f);
    fxSparks(e.pos, Vec3(0, 1, 0), 14, 9.f);
    sky.flash = 255;
}

void Game::detonate(Vec3 pos, f32 force, u8 level) {
    applyExplosion(ExplodeEvent{pos, force, level, {0, 0, 0}});
}

// -------------------------------------------------------------- player -------
namespace {
struct PlayerDims {
    static constexpr f32 halfW = 0.32f;
    static constexpr f32 standH = 1.75f;
    static constexpr f32 crouchH = 1.15f;
    static constexpr f32 eyeStand = 1.62f;
    static constexpr f32 eyeCrouch = 1.0f;
    static constexpr f32 stepUp = 0.62f;
};
} // namespace

static bool boxHitsSolid(const VoxWorld& w, Vec3 lo, Vec3 hi) {
    int x0 = (int)std::floor(lo.x / VOX), x1 = (int)std::floor(hi.x / VOX);
    int y0 = (int)std::floor(lo.y / VOX), y1 = (int)std::floor(hi.y / VOX);
    int z0 = (int)std::floor(lo.z / VOX), z1 = (int)std::floor(hi.z / VOX);
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
        if (w.solid(x, y, z)) return true;
    return false;
}

void Game::updatePlayer(f32 dt, InputState& in) {
    if (!playing) return;
    f32 crouch = in.down[KEY_CTRL] ? 1.f : 0.f;
    f32 h = lerpf(PlayerDims::standH, PlayerDims::crouchH, crouch);

    // wish direction
    Vec3 fwd(std::sin(playerYaw), 0, -std::cos(playerYaw));
    Vec3 right(std::cos(playerYaw), 0, std::sin(playerYaw));
    Vec3 wish;
    if (in.down[KEY_W]) wish += fwd;
    if (in.down[KEY_S]) wish -= fwd;
    if (in.down[KEY_D]) wish += right;
    if (in.down[KEY_A]) wish -= right;
    bool sprint = in.down[KEY_SHIFT] && !crouch && lenSq(wish) > 0;
    f32 maxSpeed = crouch > 0.5f ? 2.3f : sprint ? 6.4f : 4.3f;
    Vec3 targetVel = lenSq(wish) > 0 ? norm(wish) * maxSpeed : Vec3();
    // horizontal accel
    f32 accel = playerGrounded ? 14.f : 4.f;
    playerVel.x = lerpf(playerVel.x, targetVel.x, tclamp(accel * dt, 0.f, 1.f));
    playerVel.z = lerpf(playerVel.z, targetVel.z, tclamp(accel * dt, 0.f, 1.f));

    // jump
    if (playerGrounded && in.pressed[KEY_SPACE]) {
        playerVel.y = 6.4f;
        playerGrounded = false;
    }
    playerVel.y += physics.gravity * dt;
    if (playerVel.y < -40.f) playerVel.y = -40.f;

    // ---- collide & move, per-axis with step-up ----
    auto collideAxis = [&](int axis, f32 delta) {
        if (delta == 0) return;
        Vec3 lo(playerPos.x - PlayerDims::halfW, playerPos.y, playerPos.z - PlayerDims::halfW);
        Vec3 hi(playerPos.x + PlayerDims::halfW, playerPos.y + h, playerPos.z + PlayerDims::halfW);
        (&lo.x)[axis] += delta;
        (&hi.x)[axis] += delta;
        if (!boxHitsSolid(world, lo, hi)) {
            (&playerPos.x)[axis] += delta;
            return;
        }
        // try step-up (horizontal axes only, grounded)
        if ((axis == 0 || axis == 2) && playerGrounded) {
            f32 maxStep = PlayerDims::stepUp;
            Vec3 baseLo = lo, baseHi = hi;
            // raise
            bool freeAfterRise = false;
            Vec3 loR = lo, hiR = hi;
            // compute rise amount to clear: try incremental
            for (f32 rise = 0.1f; rise <= maxStep + 0.01f; rise += 0.1f) {
                loR = lo; hiR = hi;
                loR.y += rise; hiR.y += rise;
                if (!boxHitsSolid(world, loR, hiR)) {
                    // check landing support: some ground under new pos
                    Vec3 probeLo(playerPos.x - PlayerDims::halfW, playerPos.y + rise - 0.08f, playerPos.z - PlayerDims::halfW);
                    Vec3 probeHi(playerPos.x + PlayerDims::halfW, playerPos.y + rise, playerPos.z + PlayerDims::halfW);
                    // accept if horizontal cell free (we're standing on the step)
                    freeAfterRise = true;
                    playerPos.y += rise;
                    (&playerPos.x)[axis] += delta;
                    (void)probeLo; (void)probeHi; (void)baseLo; (void)baseHi;
                    return;
                }
            }
            (void)freeAfterRise;
        }
        // blocked: snap back to contact
        f32 d = delta;
        int guard = 0;
        while (d != 0 && guard++ < 64) {
            f32 step = d > 0 ? tmin(d, VOX * 0.5f) : tmax(d, -VOX * 0.5f);
            Vec3 lo2(playerPos.x - PlayerDims::halfW, playerPos.y, playerPos.z - PlayerDims::halfW);
            Vec3 hi2(playerPos.x + PlayerDims::halfW, playerPos.y + h, playerPos.z + PlayerDims::halfW);
            (&lo2.x)[axis] += step;
            (&hi2.x)[axis] += step;
            if (boxHitsSolid(world, lo2, hi2)) break;
            (&playerPos.x)[axis] += step;
            d -= step;
        }
        if (axis == 1) {
            if (delta < 0) playerGrounded = true;
            playerVel.y = 0;
        } else {
            (&playerVel.x)[axis] = 0;
        }
    };

    playerGrounded = false;
    // vertical first (ground check), then horizontal
    collideAxis(1, playerVel.y * dt);
    // ground probe
    {
        Vec3 lo(playerPos.x - PlayerDims::halfW, playerPos.y - 0.08f, playerPos.z - PlayerDims::halfW);
        Vec3 hi(playerPos.x + PlayerDims::halfW, playerPos.y + 0.01f, playerPos.z + PlayerDims::halfW);
        if (playerVel.y <= 0.01f && boxHitsSolid(world, lo, hi)) playerGrounded = true;
    }
    collideAxis(0, playerVel.x * dt);
    collideAxis(2, playerVel.z * dt);

    // body push against props (simple): skip for now — props get pushed when player collides (add: overlap query impulse)
    u32 near[8];
    AABB pa(playerPos, playerPos + Vec3(0.64f, h, 0.64f));
    int nb = physics.overlapBodies(pa, near, 8);
    for (int i = 0; i < nb; i++) {
        Body* b = physics.findBody(near[i]);
        if (!b || (b->flags & (BF_STATIC | BF_DRIVEN))) continue;
        Vec3 away = b->pos - playerPos;
        away.y = 0;
        f32 dd = len(away);
        if (dd < 0.9f && dd > 1e-4f) {
            b->vel += norm(away) * (1.6f * dt * 20.f);
            b->flags &= (u16)~BF_SLEEP;
        }
    }

    // keep inside world
    playerPos.x = tclamp(playerPos.x, 0.5f, world.W * VOX - 0.5f);
    playerPos.z = tclamp(playerPos.z, 0.5f, world.D * VOX - 0.5f);
    if (playerPos.y < -10.f) respawnPlayer();

    // head bob + footstep cadence
    f32 hspeed = std::sqrt(playerVel.x * playerVel.x + playerVel.z * playerVel.z);
    if (playerGrounded && hspeed > 0.5f) {
        f32 prev = camBob;
        camBob += dt * hspeed * 1.9f;
        // every PI of bob phase = one footfall
        if (std::fmod(prev, 3.14159265f) > std::fmod(camBob, 3.14159265f)) {
            int stepIdx = ((int)(camBob / 3.14159265f)) & 3;
            u32 here = world.get((int)(playerPos.x / VOX), (int)(playerPos.y / VOX), (int)(playerPos.z / VOX)).mat;
            (void)here;
            sfxPlay((SfxId)(SFX_FOOTSTEP_1 + stepIdx), playerPos, tclamp(hspeed * 0.16f, 0.25f, 0.7f),
                    0.95f + stepIdx * 0.03f);
        }
    }

    updateCameraAfterPlayer();
}

void Game::updateCameraAfterPlayer() {
    f32 eyeH = PlayerDims::eyeStand;
    f32 bobY = std::sin(camBob * 2.f) * 0.035f;
    f32 bobX = std::cos(camBob) * 0.025f;
    cam.pos = Vec3(playerPos.x, playerPos.y + eyeH + bobY, playerPos.z);
    Vec3 fwd(std::sin(playerYaw) * std::cos(playerPitch), std::sin(playerPitch), -std::cos(playerYaw) * std::cos(playerPitch));
    cam.fwd = norm(fwd);
    cam.up = Vec3(0, 1, 0);
    cam.fovY = settings.fov * 3.14159265f / 180.f;
    (void)bobX;
}

void Game::fixedUpdate() {
    InputState* in = curInput_;
    if (in) updatePlayer(FIXED_DT, *in);
    simStep();
}

void Game::tick(f32 realDt, InputState& in) {
    curInput_ = &in;
    handleGlobalInput(in);
    realDt = tclamp(realDt, 0.f, 0.1f);
    if (realDt > 1e-5f) fpsSmoothed = lerpf(fpsSmoothed, 1.f / realDt, 0.08f);

    // sun position from time of day
    {
        f32 theta = 3.14159265f * (timeOfDay - 6.f) / 12.f;   //6h=horizon E,12h=up,18h=W
        Vec3 toSun = norm(Vec3(std::cos(theta) * 0.75f, tmax(0.02f, std::sin(theta)) + 0.02f, std::cos(theta) * 0.4f + 0.45f));
        if (timeOfDay < 6.f || timeOfDay > 18.f) {
            f32 night = 1.f;
            sky.sunDir = norm(Vec3(0.3f, -0.8f, 0.2f));
            sky.sunColor = Vec3(0.35f, 0.42f, 0.6f);
            sky.sunIntensity = 0.35f * night;
            sky.zenith = Vec3(0.03f, 0.05f, 0.1f);
            sky.horizon = Vec3(0.1f, 0.12f, 0.17f);
            sky.fogColor = Vec3(0.07f, 0.09f, 0.12f);
            sky.ambient = 0.85f;
        } else {
            sky.sunDir = -toSun;
            f32 elev = std::sin(theta);
            f32 golden = 1.f - tclamp(elev * 2.2f, 0.f, 1.f);
            sky.sunColor = lerp(Vec3(1.f, 0.96f, 0.88f), Vec3(1.f, 0.62f, 0.36f), golden);
            sky.sunIntensity = 0.75f + elev * 0.75f;
            sky.zenith = lerp(Vec3(0.20f, 0.40f, 0.75f), Vec3(0.10f, 0.14f, 0.28f), golden * 0.8f);
            sky.horizon = lerp(Vec3(0.68f, 0.76f, 0.85f), Vec3(0.95f, 0.70f, 0.45f), golden * 0.85f);
            sky.fogColor = lerp(Vec3(0.66f, 0.72f, 0.79f), Vec3(0.85f, 0.62f, 0.44f), golden * 0.7f);
            sky.ambient = 1.f;
        }
        // weather presets
        if (sky.weather == 1) { sky.sunIntensity *= 0.45f; sky.zenith = lerp(sky.zenith, Vec3(0.45f, 0.48f, 0.52f), 0.7f); sky.horizon = lerp(sky.horizon, Vec3(0.6f, 0.62f, 0.65f), 0.7f); }
        if (sky.weather == 2 || sky.weather == 3) {
            sky.sunIntensity *= 0.3f;
            sky.zenith = Vec3(0.28f, 0.3f, 0.34f);
            sky.horizon = Vec3(0.45f, 0.47f, 0.5f);
            sky.fogColor = Vec3(0.44f, 0.46f, 0.49f);
            sky.fogDensity = sky.weather == 3 ? 0.014f : 0.009f;
            sky.wetness = lerpf(sky.wetness, 1.f, 0.02f);
        } else if (sky.weather == 4) {
            sky.fogDensity = 0.035f;
            sky.sunIntensity *= 0.5f;
            sky.wetness = lerpf(sky.wetness, 0.3f, 0.02f);
        } else {
            sky.fogDensity = lerpf(sky.fogDensity, 0.0065f, 0.02f);
            sky.wetness = lerpf(sky.wetness, 0.f, 0.01f);
        }
        if (sky.flash > 0) sky.flash = (u8)tmax(0, (int)sky.flash - 26);
    }

    // audio listener follows camera; master volume from settings
    audio().setListener(cam.pos, cam.fwd);
    audio().setMaster(settings.master);
    audio().updateLoops(realDt);

    if (playing && simReady) {
        simAccum += realDt;
        int steps = 0;
        while (simAccum >= FIXED_DT && steps < 5) {
            fixedUpdate();
            simAccum -= FIXED_DT;
            steps++;
        }
        if (steps >= 5) simAccum = 0;
        flushDirtyMeshes(24);   // incremental remesh budget per frame
    }
    curInput_ = nullptr;
}

// ------------------------------------------------------------- submission ----
static void frustumPlanes(const Mat4& vp, Vec4 out[6]) {
    auto row = [&](int r) { return Vec4(vp.at(r, 0), vp.at(r, 1), vp.at(r, 2), vp.at(r, 3)); };
    Vec4 r3 = row(3);
    Vec4 r0 = row(0), r1 = row(1), r2 = row(2);
    auto add = [](Vec4 a, Vec4 b) { return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); };
    auto sub = [](Vec4 a, Vec4 b) { return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); };
    auto norm4 = [](Vec4 v) { f32 l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); if (l > 1e-8f) { v.x /= l; v.y /= l; v.z /= l; } return v; };
    out[0] = norm4(add(r3, r0));   // left
    out[1] = norm4(sub(r3, r0));   // right
    out[2] = norm4(add(r3, r1));   // bottom
    out[3] = norm4(sub(r3, r1));   // top
    out[4] = norm4(add(r3, r2));   // near
    out[5] = norm4(sub(r3, r2));   // far
}

void Game::submitScene() {
    Vec4 planes[6];
    frustumPlanes(cam.proj((f32)rb->width() / rb->height()) * cam.view(), planes);
    auto visible = [&](Vec3 lo, Vec3 hi) {
        for (int i = 0; i < 6; i++) {
            Vec3 p = planes[i].x >= 0 ? Vec3(hi.x, hi.y, hi.z) : Vec3(lo.x, lo.y, lo.z);
            if (planes[i].x * p.x + planes[i].y * p.y + planes[i].z * p.z + planes[i].w < 0) return false;
        }
        return true;
    };
    int drawn = 0;
    f32 maxD = viewDistFor(settings.viewDistance);
    Vec3 camP = cam.pos;
    for (u32 i = 0; i < (u32)world.chunks.size(); i++) {
        const Chunk& c = world.chunks[i];
        if (!c.hasSolid() && !c.hasLiquid()) continue;
        int cx = (int)(i % (u32)world.CW);
        int cy = (int)((i / (u32)world.CW) % (u32)world.CHh);
        int cz = (int)(i / ((u32)world.CW * (u32)world.CHh));
        Vec3 lo(cx * CH * VOX, cy * CH * VOX, cz * CH * VOX);
        Vec3 hi = lo + Vec3(CH * VOX, CH * VOX, CH * VOX);
        // distance cull
        Vec3 nearest = vmax(vmin(camP, hi), lo);
        if (lenSq(nearest - camP) > maxD * maxD) continue;
        if (!visible(lo, hi)) continue;
        rb->drawChunk(world, i);
        drawn++;
    }
    lastChunksDrawn = drawn;

    // fragments
    for (Fragment& f : physics.frags) {
        if (f.gpuHandle == 0) f.gpuHandle = rb->createMesh(f.mverts, f.midx);
        // revision tracking via parallel map
        auto it = fragRev_.find(f.id);
        if (it == fragRev_.end() || it->second != f.meshRev) {
            rb->updateMesh(f.gpuHandle, f.mverts, f.midx);
            fragRev_[f.id] = f.meshRev;
        }
        Mat4 xform = Mat4::translate(f.pos) * f.rot.toMat();
        rb->drawMesh(f.gpuHandle, xform, 1, 1, 1, 0);
    }
    // bodies as boxes
    for (Body& b : physics.bodies) {
        u32 h = cubeFor(b.mat);
        f32 tr = 1, tg = 1, tb = 1;
        if (b.col) {
            int r, g, bl; unpack565(b.col, r, g, bl);
            tr = r / 255.f; tg = g / 255.f; tb = bl / 255.f;
        }
        Mat4 xform = Mat4::translate(b.pos) * b.rot.toMat() * Mat4::scale(Vec3(b.half.x * 2, b.half.y * 2, b.half.z * 2));
        rb->drawMesh(h, xform, tr, tg, tb, 0);
    }
    renderParticles(rb);
}

u32 Game::cubeFor(u8 mat) {
    auto it = cubeCache_.find(mat);
    if (it != cubeCache_.end()) return it->second;
    // unit cube centered at origin, size1, white color, given material
    std::vector<u8> verts;
    std::vector<u32> idx;
    const Vec3 n[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int f = 0; f < 6; f++) {
        Vec3 N = n[f];
        Vec3 U = std::fabs(N.y) > 0.5f ? Vec3(1,0,0) : Vec3(0,1,0);
        Vec3 R = cross(U, N); U = cross(N, R);
        u32 base = (u32)(verts.size() / 32);
        for (int k = 0; k < 4; k++) {
            f32 su = (k == 1 || k == 2) ? 0.5f : -0.5f;
            f32 sv = (k >= 2) ? 0.5f : -0.5f;
            Vec3 p = N * 0.5f + R * su + U * sv;
            MVert mv;
            std::memset(&mv, 0, sizeof(mv));
            mv.p[0] = p.x; mv.p[1] = p.y; mv.p[2] = p.z;
            mv.n[0] = N.x; mv.n[1] = N.y; mv.n[2] = N.z;
            mv.col[0] = mv.col[1] = mv.col[2] = 255;
            mv.ao = 4;
            mv.mat = mat;
            mv.flags = 0;
            size_t off = verts.size();
            verts.resize(off + sizeof(MVert));
            std::memcpy(verts.data() + off, &mv, sizeof(MVert));
        }
        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 0); idx.push_back(base + 2); idx.push_back(base + 3);
    }
    u32 h = rb->createMesh(verts, idx);
    cubeCache_[mat] = h;
    return h;
}

// ---------------------------------------------------------------- render -----
void Game::render() {
    if (!rb) return;
    // flashlight
    flashlight.intensity = (playing && flashOn) ? 1.35f : 0.f;
    flashlight.pos = cam.pos;
    flashlight.dir = cam.fwd;
    flashlight.angleCos = std::cos(38.f * 3.14159f / 180.f);
    flashlight.range = 30.f;

    // frame lights: static lights from scene (kept minimal now) + none
    frameLights.clear();

    rb->beginScene(cam, (f32)rb->width() / (f32)rb->height(), sky, frameLights, flashlight);
    if (simReady) submitScene();
    rb->renderScene();

    rb->beginUI();
    renderUI();
    rb->renderUI();
    rb->present();
}

int Game::hitTest(f32 mx, f32 my) const {
    for (const UIHit& h : uiHits_)
        if (mx >= h.x && mx <= h.x + h.w && my >= h.y && my <= h.y + h.h) return h.id;
    return -1;
}

void Game::renderUI() {
    uiHits_.clear();
    int W = rb->width(), H = rb->height();
    if (playing && screen == SCR_PLAY) {
        // crosshair
        f32 cx = W * 0.5f, cy = H * 0.5f;
        rb->pushRect(cx - 9, cy - 1, 6, 2, 240, 240, 240, 210);
        rb->pushRect(cx + 3, cy - 1, 6, 2, 240, 240, 240, 210);
        rb->pushRect(cx - 1, cy - 9, 2, 6, 240, 240, 240, 210);
        rb->pushRect(cx - 1, cy + 3, 2, 6, 240, 240, 240, 210);
        // bottom bar
        rb->pushRect(0, H - 40, W, 40, 12, 14, 16, 170);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "FRACTURE // %s", currentMapName.c_str());
        uiText(rb, 14, H - 32, 16, 235, 168, 40, 235, buf, 0, true);
        std::snprintf(buf, sizeof(buf), "POS %.1f %.1f %.1f", playerPos.x, playerPos.y, playerPos.z);
        uiText(rb, 14, H - 16, 13, 180, 185, 190, 220, buf);
        if (settings.showFps) {
            std::snprintf(buf, sizeof(buf), "%d FPS  chunks %d  parts %d  bodies %zu  frags %zu",
                          (int)(fpsSmoothed + 0.5f), lastChunksDrawn, particleCount(), physics.bodies.size(), physics.frags.size());
            uiText(rb, W - 14, H - 32, 13, 180, 200, 170, 230, buf, 2);
        }
    } else if (screen == SCR_MENU) {
        // main menu layout
        rb->pushRect(0, 0, W, H, 16, 18, 21, 255);
        // left hazard band
        uiHazard(rb, 0, 0, W, 8, 245, 176, 32, 30, 32, 36, 16);
        uiHazard(rb, 0, H - 10, W, 10, 245, 176, 32, 30, 32, 36, 16);
        // title
        uiText(rb, W * 0.5f, H * 0.20f, 74, 245, 176, 32, 255, "FRACTURE", 1, true, 2.f);
        uiText(rb, W * 0.5f, H * 0.20f + 78, 30, 220, 222, 226, 245, "TOTAL DESTRUCTION", 1, false, 1.f);
        const char* items[] = {"CAMPAIGN", "SANDBOX", "SETTINGS", "CONTROLS", "QUIT"};
        int n = 5;
        f32 y0 = H * 0.48f;
        for (int i = 0; i < n; i++) {
            f32 bw = 340, bh = 44, bx = W * 0.5f - bw * 0.5f, by = y0 + i * (bh + 10);
            bool hover = (hitTest(in_cursorX_, in_cursorY_) == -1) && false;  // filled on next pass
            (void)hover;
            rb->pushRect(bx, by, bw, bh, 32, 34, 38, 245);
            rb->pushRect(bx, by, 5, bh, 245, 176, 32, 255);
            uiText(rb, W * 0.5f, by + 12, 20, 230, 232, 236, 245, items[i], 1, false);
            uiHits_.push_back({bx, by, bw, bh, 10 + i});   // 10=campaign..14=quit
        }
        uiText(rb, W * 0.5f, H - 42, 13, 150, 152, 156, 220, "v0.1 development build", 1);
    } else if (screen == SCR_PAUSE) {
        rb->pushRect(0, 0, W, H, 8, 9, 11, 150);
        uiText(rb, W * 0.5f, H * 0.3f, 44, 245, 176, 32, 255, "PAUSED", 1, true);
        const char* pitems[] = {"RESUME", "SETTINGS", "QUIT TO MENU"};
        f32 y0 = H * 0.45f;
        for (int i = 0; i < 3; i++) {
            f32 bw = 340, bh = 44, bx = W * 0.5f - bw * 0.5f, by = y0 + i * (bh + 10);
            rb->pushRect(bx, by, bw, bh, 32, 34, 38, 245);
            rb->pushRect(bx, by, 5, bh, 245, 176, 32, 255);
            uiText(rb, W * 0.5f, by + 12, 20, 230, 232, 236, 245, pitems[i], 1, false);
            uiHits_.push_back({bx, by, bw, bh, 20 + i});   // 20=resume,21=settings,22=menu
        }
    } else if (screen == SCR_SETTINGS || screen == SCR_CONTROLS || screen == SCR_CAMPAIGN ||
               screen == SCR_SANDBOX_SELECT) {
        rb->pushRect(0, 0, W, H, 16, 18, 21, 255);
        uiHazard(rb, 0, 0, W, 8, 245, 176, 32, 30, 32, 36, 16);
        const char* title = screen == SCR_SETTINGS ? "SETTINGS" :
                            screen == SCR_CONTROLS ? "CONTROLS" :
                            screen == SCR_CAMPAIGN ? "CAMPAIGN" : "SANDBOX";
        uiText(rb, W * 0.5f, 46, 42, 245, 176, 32, 255, title, 1, true);
        if (screen == SCR_CONTROLS) {
            const char* lines[] = {
                "WASD move | SHIFT sprint | SPACE jump | CTRL crouch",
                "LMB use tool | RMB aim |1-9 select tool | E interact",
                "F flashlight | TAB inventory | ESC pause | F3 stats",
                "MOUSE look | Q drop",
            };
            for (int i = 0; i < 4; i++)
                uiText(rb, W * 0.5f, 140 + i * 40, 20, 210, 214, 220, 245, lines[i], 1);
        } else if (screen == SCR_SETTINGS) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "FOV %.0f    VIEW DISTANCE %d    SHADOWS %d    TEXTURES %d",
                          (f64)settings.fov, settings.viewDistance, settings.shadowQuality, settings.textureQuality);
            uiText(rb, W * 0.5f, 150, 19, 210, 214, 220, 245, buf, 1);
            std::snprintf(buf, sizeof(buf), "DESTRUCTION %d    EFFECTS %d    LIGHTS %d    FPS LIMIT %d",
                          settings.destructionQuality, settings.effectsQuality, settings.lightingQuality, settings.fpsLimit);
            uiText(rb, W * 0.5f, 194, 19, 210, 214, 220, 245, buf, 1);
            std::snprintf(buf, sizeof(buf), "MASTER %d%%    SFX %d%%    MOUSE %.1f    INVERT Y %s",
                          (int)(settings.master * 100), (int)(settings.sfx * 100), settings.mouseSens,
                          settings.invertY > 0.5f ? "ON" : "OFF");
            uiText(rb, W * 0.5f, 238, 19, 210, 214, 220, 245, buf, 1);
            uiText(rb, W * 0.5f, 300, 17, 170, 174, 180, 240, "full settings UI arrives in the options pass", 1);
        } else if (screen == SCR_CAMPAIGN) {
            uiText(rb, W * 0.5f, 150, 21, 210, 214, 220, 245, "5 chapters / 30 missions — mission list loads with the campaign pass", 1);
        } else if (screen == SCR_SANDBOX_SELECT) {
            uiText(rb, W * 0.5f, 150, 21, 210, 214, 220, 245, "map picker loads with the mapgen pass — starting TEST map", 1);
        }
        // back button
        f32 bw = 220, bh = 44, bx = W * 0.5f - bw * 0.5f, by = H - 90;
        rb->pushRect(bx, by, bw, bh, 32, 34, 38, 245);
        rb->pushRect(bx, by, 5, bh, 245, 176, 32, 255);
        uiText(rb, W * 0.5f, by + 12, 20, 230, 232, 236, 245, "BACK", 1);
        uiHits_.push_back({bx, by, bw, bh, 30});
    }
}

// ------------------------------------------------------------- input hookup --
void Game::handleGlobalInput(InputState& in) {
    in_cursorX_ = in.cursorX;
    in_cursorY_ = in.cursorY;
    // menu / pause clicks (UI rects were built last rendered frame)
    if (in.pressed[KEY_LMB] && !playing) {
        int hit = hitTest(in.cursorX, in.cursorY);
        if (hit == 10) { screen = SCR_CAMPAIGN; }
        else if (hit == 11) {
            screen = SCR_SANDBOX_SELECT;
            buildScene("test", 1);
            screen = SCR_PLAY;
            playing = true;
        }
        else if (hit == 12) screen = SCR_SETTINGS;
        else if (hit == 13) screen = SCR_CONTROLS;
        else if (hit == 14) wantQuit = true;
        else if (hit == 20) { screen = SCR_PLAY; playing = true; }
        else if (hit == 21) screen = SCR_SETTINGS;
        else if (hit == 22) { screen = SCR_MENU; playing = false; }
        else if (hit == 30) screen = SCR_MENU;
        if (hit >= 10) sfxPlay2D(SFX_UI_CLICK, 0.9f);
    }
    if (in.pressed[KEY_F]) flashOn = !flashOn;
    if (in.pressed[KEY_F3]) settings.showFps = !settings.showFps;
    if (playing && in.pressed[KEY_ESC]) { screen = SCR_PAUSE; playing = false; }
    else if (screen == SCR_PAUSE && in.pressed[KEY_ESC]) { screen = SCR_PLAY; playing = true; }
    // entering play from sub-screens: RETURN key goes back
    if (!playing && screen != SCR_MENU && screen != SCR_PLAY && in.pressed[KEY_ESC])
        screen = (screen == SCR_PAUSE) ? SCR_PLAY : SCR_MENU;
    // time of day quick controls (dev)
    if (in.down[KEY_SHIFT] && in.pressed[KEY_LEFT]) timeOfDay = tclamp(timeOfDay - 1.f, 0.f, 24.f);
    if (in.down[KEY_SHIFT] && in.pressed[KEY_RIGHT]) timeOfDay = tclamp(timeOfDay + 1.f, 0.f, 24.f);
}

} // namespace ft
