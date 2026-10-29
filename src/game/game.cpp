#include "game/game.h"
#include "game/globals.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

Game* g_game = nullptr;
float g_time = 0;
float g_gameTime = 0;
FX* g_fx = nullptr;
Physics* g_physics = nullptr;
Player* g_playerRef = nullptr;
float g_lastMouseDX = 0, g_lastMouseDY = 0;

void explodeAt(const vec3& center, float radius, int source) {
    if (g_game) g_game->explode(center, radius, source);
}
void carveWorld(const vec3& center, float radius, float power, bool structural, bool ignite) {
    if (g_game) g_game->carveRegion(center, radius, power, structural, ignite);
}
void playFootstepSound(uint8_t mat, const vec3& pos, float vol) {
    SfxId s = SFX_FOOTSTEP_CONCRETE;
    const Material& m = material(mat);
    switch (m.footstep) {
        case 1: s = SFX_FOOTSTEP_WOOD; break;
        case 2: s = SFX_FOOTSTEP_METAL; break;
        case 3: s = SFX_FOOTSTEP_DIRT; break;
        case 4: s = SFX_FOOTSTEP_DIRT; break;
        case 5: s = SFX_FOOTSTEP_GLASS; break;
        default: s = SFX_FOOTSTEP_CONCRETE; break;
    }
    audio::play(s, pos, vol, 0.9f + hash3f((int)(g_time * 100), 1, 1) * 0.25f);
}

// ---------------------------------------------------------------- init
bool Game::init(IRenderer* r) {
    renderer = r;
    g_game = this;
    fx.init(&world);
    g_fx = &fx;
    g_physics = &physics;
    g_playerRef = &player;
    physics.init(&world);
    vehicles.init(&world, &physics);
    security.init(&world, &physics);
    tools.init();
    g_missions.init();
    audio::setVolumes(settings.masterVolume, settings.sfxVolume, settings.musicVolume);
    // tool ammo defaults for sandbox
    for (int i = 0; i < TOOL_COUNT; i++) tools.ammo[i] = toolDef(i).maxAmmo;
    // load the first map for the menu backdrop; the menu appears once meshing finishes
    menuAfterLoad = true;
    loadMap(0, false, -1);
    audio::startMusic(0);
    logMsg("[game] initialised; map0 chunks=%d", world.allocChunks);
    return true;
}

void Game::shutdown() {
    audio::stopMusic();
    vehicles.clear();
    physics.clear();
    world.clear();
}

std::string Game::savePath(const std::string& slot) const {
    return platform::dataDir() + "/" + slot + ".fracture";
}

// ---------------------------------------------------------------- loading
void Game::loadMap(int mapId, bool campaign, int missionId) {
    currentMap = iclamp(mapId, 0, mapCount() - 1);
    playMode = campaign ? PLAY_CAMPAIGN : PLAY_SANDBOX;
    // reset gameplay state
    vehicles.clear();
    physics.clear();
    fx.clear();
    pickups.clear();
    security.clear();
    doors.clear();
    toasts.clear();
    collapseJobs.clear();
    tools.reset();
    drivenVehicle = -1;
    player = Player();
    world.clear();
    const GameMap& m = getMap(currentMap);
    world.init(m.info.sx, m.info.sy, m.info.sz);
    mapData = MapBuildResult();
    buildMap(currentMap, world, mapData);
    // queue all non-empty chunks for progressive meshing
    loading.mapId = currentMap;
    loading.campaign = campaign;
    loading.missionId = missionId;
    loading.stage = 1;
    loading.chunks.clear();
    loading.chunkIndex = 0;
    for (auto& kv : world.chunks) {
        Chunk* c = kv.second;
        bool any = false;
        for (int i = 0; i < CHV && !any; i++)
            if (c->v[i] != M_AIR) any = true;
        if (any) loading.chunks.push_back(kv.first);
    }
    world.dirtyChunks.clear();
    loadingProgress = 0;
    mode = MODE_LOADING;
    // environment from the map
    const MapInfo& mi = m.info;
    env.sunDir = mi.sunDir;
    env.sunColor = mi.sunColor;
    env.sunIntensity = 1.0f;
    env.skyTop = mi.skyTop;
    env.skyHorizon = mi.skyHorizon;
    env.ambientSky = mi.ambientSky;
    env.ambientGround = mi.ambientGround;
    env.fogColor = mi.fogColor;
    env.fogDensity = mi.fogDensity;
    env.wetness = mi.weather.wetness;
    env.cloudCover = mi.weather.cloud;
    env.rainAmount = mi.weather.rain;
    env.weather = mi.weather.kind;
    env.wind = mi.weather.wind;
    env.time = 0;
    env.exposure = 1.0f;
    if (campaign && missionId >= 0) {
        g_missions.start(missionId);
        missionStartTime = 0;
    }
    logMsg("[game] map %d queued for mesh build: %d chunks", currentMap, (int)loading.chunks.size());
}

// advances loading when in MODE_LOADING
static void advanceLoading(Game& g) {
    if (g.loading.stage == 1) {
        // build chunk meshes in slices
        const int slice = 24;
        for (int i = 0; i < slice && g.loading.chunkIndex < g.loading.chunks.size(); i++) {
            ivec3 c = World::unchunkKey(g.loading.chunks[g.loading.chunkIndex++]);
            g.world.buildChunkMesh(c.x, c.y, c.z);
        }
        g.loadingProgress = g.loading.chunks.empty() ? 0.6f
            : 0.65f * (float)g.loading.chunkIndex / (float)g.loading.chunks.size();
        if (g.loading.chunkIndex >= g.loading.chunks.size()) {
            g.loading.stage = 2;
        }
        return;
    }
    if (g.loading.stage == 2) {
        // spawn props, vehicles and pickups
        const GameMap& m = getMap(g.currentMap);
        g.world.rebuildDirtyMeshes(4096);
        for (auto& p : g.mapData.props) {
            vec3 pos = p.second;
            float gy = g.world.groundHeightM(pos.x, pos.z);
            pos.y = std::max(pos.y, gy + 0.45f);
            g.physics.spawnProp(pos, p.first);
        }
        for (auto& v : g.mapData.vehicles) g.vehicles.spawn((VehicleKind)v.kind, v.pos, v.yaw);
        // security devices are placed onto the mission-critical structures of each map
        {
            std::vector<std::pair<vec3, vec3>> secBoxes;
            for (auto& t : g.mapData.targets) {
                unsigned h = (unsigned)(m.info.id * 7919 + (int)t.box.center().x * 131 + (int)t.box.center().z);
                if (h % 3 == 0) secBoxes.push_back({t.box.mn, t.box.mx});
            }
            g.security.init(&g.world, &g.physics);
            g.security.placeForTargets(secBoxes);
        }
        g.buildDoorsFromMap();
        // evidence pickups for the extraction mission
        if (g.playMode == PLAY_CAMPAIGN && g_missions.state.missionId >= 0) {
            const Mission& mi = g_missions.get(g_missions.state.missionId);
            for (auto& o : mi.objectives) {
                if (o.kind == OBJ_COLLECT) {
                    // scatter the required crates around the player's half of the map
                    vec3 base(m.info.spawn.x, 0, m.info.spawn.z);
                    for (int i = 0; i < o.count; i++) {
                        float a = i * 2.1f + 0.7f;
                        vec3 p = base + vec3(std::cos(a) * 12.0f, 0, std::sin(a) * 12.0f);
                        p.y = g.world.groundHeightM(p.x, p.z) + 0.4f;
                        g.spawnPickup(p, 0, i);
                    }
                }
            }
        }
        g.player.pos = m.info.spawn;
        g.player.pos.y = std::max(g.player.pos.y, g.world.groundHeightM(m.info.spawn.x, m.info.spawn.z) + 0.1f);
        // nudge the spawn out of solid geometry
        for (int i = 0; i < 40 && g.physics.capsuleBlocked(g.player.pos, g.player.radius, g.player.height); i++)
            g.player.pos.y += 0.25f;
        g.player.yaw = m.info.spawnYaw;
        g.player.pitch = -0.05f;
        g.player.vel = vec3(0);
        g.player.cameraHeight = 1.66f;
        g.tools.unlimitedAmmo = g.sandbox.unlimitedAmmo;
        g.tools.infiniteFuel = g.sandbox.infiniteFuel;
        g.physics.maxBodies = 160 + g.sandbox.debrisDensity * 90;
        g.fx.setQuality(g.settings.particleQuality);
        g.loaded = true;
        g.loadingProgress = 1.0f;
        if (g.menuAfterLoad) {
            g.mode = MODE_MAIN_MENU;
            platform::setMouseCapture(false);
        } else if (g.playMode == PLAY_CAMPAIGN) {
            g.mode = MODE_PLAYING;
            const Mission& mi = g_missions.get(g_missions.state.missionId);
            g.toast(std::string("MISSION: ") + mi.name, 1);
        } else {
            g.mode = MODE_PLAYING;
            g.toast("SANDBOX - destroy anything. TAB for tools, ESC for menu.", 0);
        }
        g.playTime = 0;
        g.stats.destroyedVoxels = 0;
        platform::setMouseCapture(true);
        g.loading.stage = 0;
        return;
    }
}

// ---------------------------------------------------------------- run / update
void Game::run() {
    // main loop is driven from main.cpp; kept for API symmetry
}

float Game::aspect() const {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    return h > 0 ? (float)w / (float)h : 16.0f / 9.0f;
}

Camera Game::currentCamera() const {
    if (mode == MODE_MAIN_MENU || mode == MODE_CAMPAIGN || mode == MODE_SANDBOX_SETUP || mode == MODE_SETTINGS ||
        mode == MODE_CONTROLS || mode == MODE_CREDITS || mode == MODE_SAVE_LOAD || !loaded) {
        // slow orbit over the loaded map
        Camera c;
        const MapInfo& mi = getMap(currentMap).info;
        float t = menuTime * 0.055f;
        vec3 center = mi.spawn;
        float radius = 46.0f;
        c.pos = vec3(center.x + std::cos(t) * radius, 27.0f + std::sin(t * 0.7f) * 3.0f, center.z + std::sin(t) * radius);
        c.yaw = std::atan2(center.x - c.pos.x, -(center.z - c.pos.z));
        c.pitch = -0.30f;
        c.fovY = 62 * DEG2RAD;
        c.aspect = aspect();
        c.zn = 0.1f;
        c.zf = 900;
        return c;
    }
    if (drivenVehicle >= 0 && drivenVehicle < (int)vehicles.vehicles.size()) {
        const Vehicle& v = vehicles.vehicles[drivenVehicle];
        Camera c;
        float yaw = player.yaw;
        float pitch = clampf(player.pitch, -0.9f, 0.7f);
        vec3 fwd(std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw));
        int kind = v.kind;
        float dist = (kind == VK_TRUCK || kind == VK_EXCAVATOR || kind == VK_BULLDOZER) ? 13.0f : 9.0f;
        float height = (kind == VK_TRUCK || kind == VK_EXCAVATOR) ? 4.2f : 2.8f;
        c.pos = v.pos - fwd * dist + vec3(0, height, 0);
        c.yaw = yaw;
        c.pitch = pitch - 0.12f;
        c.fovY = 72 * DEG2RAD;
        c.aspect = aspect();
        c.zn = 0.1f;
        c.zf = 900;
        return c;
    }
    return player.makeCamera(aspect());
}

void Game::setMode(GameMode m) {
    prevMode = mode;
    mode = m;
    bool wantsMouse = (m == MODE_PLAYING);
    platform::setMouseCapture(wantsMouse);
}

void Game::toast(const std::string& text, int kind) {
    // never queue the same message twice in a row
    for (auto& existing : toasts) {
        if (existing.text == text) {
            existing.life = existing.maxLife;
            return;
        }
    }
    Toast t;
    t.text = text;
    t.kind = kind;
    t.life = kind == 1 ? 5.0f : 4.0f;
    t.maxLife = t.life;
    toasts.push_back(t);
    if (toasts.size() > 6) toasts.erase(toasts.begin());
}

void Game::ensureMapLoadedForMenu() {
    if (world.sx == 0) loadMap(0, false, -1);
}

void Game::applySettings() {
    if (renderer) renderer->setSettings(settings);
    audio::setVolumes(settings.masterVolume, settings.sfxVolume, settings.musicVolume);
    audio::setVolumes(settings.masterVolume, settings.sfxVolume, settings.musicVolume);
    player.fov = settings.fov;
    fx.setQuality(settings.particleQuality);
    physics.maxBodies = 140 + settings.debrisDensity * 90 + settings.qualityPreset * 40;
}

void Game::resize(int w, int h) {
    if (renderer) renderer->resize(w, h);
}

// ---------------------------------------------------------------- update
void Game::update(float dt) {
    g_time += dt;
    float mdx = input.mouseDX, mdy = input.mouseDY;
    g_lastMouseDX = mdx;
    g_lastMouseDY = mdy;
    menuTime += dt;
    // fps tracking
    stats.updateMs = dt * 1000.0f;
    static float accum = 0;
    static int count = 0;
    accum += dt;
    count++;
    if (accum > 0.5f) {
        stats.avgFrameMs = accum / count * 1000.0f;
        stats.fps = (int)(1.0f / std::max(0.0001f, accum / count));
        accum = 0;
        count = 0;
    }
    if (mode == MODE_LOADING) {
        advanceLoading(*this);
        return;
    }
    // toasts
    for (auto& t : toasts) t.life -= dt;
    for (int i = (int)toasts.size() - 1; i >= 0; i--)
        if (toasts[i].life <= 0) toasts.erase(toasts.begin() + i);
    statusTimer = std::max(0.0f, statusTimer - dt);
    damageFlash = std::max(0.0f, damageFlash - dt * 1.6f);
    hitmarker = std::max(0.0f, hitmarker - dt * 3.0f);
    objectiveFlash = std::max(0.0f, objectiveFlash - dt);
    saveCooldown = std::max(0.0f, saveCooldown - dt);

    if (mode == MODE_PLAYING) {
        updatePlaying(dt);
        keyShortcuts();
    } else if (mode == MODE_PAUSE) {
        // keep the world alive but frozen
        if (input.isPressed(K_ESCAPE)) {
            setMode(MODE_PLAYING);
        }
    } else {
        // menus: animate the backdrop world
        updateEnvironment(dt);
        if (mode == MODE_MAIN_MENU && input.isPressed(K_ESCAPE)) {
            // no-op; ESC on main menu does nothing
        }
        if (mode == MODE_CAMPAIGN && input.isPressed(K_ESCAPE)) setMode(MODE_MAIN_MENU);
        if (mode == MODE_SANDBOX_SETUP && input.isPressed(K_ESCAPE)) setMode(MODE_MAIN_MENU);
        if (mode == MODE_SETTINGS && input.isPressed(K_ESCAPE)) { setMode(MODE_MAIN_MENU); applySettings(); }
        if (mode == MODE_CONTROLS && input.isPressed(K_ESCAPE)) setMode(MODE_MAIN_MENU);
        if (mode == MODE_CREDITS && input.isPressed(K_ESCAPE)) setMode(MODE_MAIN_MENU);
        if (mode == MODE_SAVE_LOAD && input.isPressed(K_ESCAPE)) setMode(MODE_MAIN_MENU);
        physics.update(dt * 0.35f);
        fx.update(dt, currentCamera().pos);
        vehicles.update(dt * 0.35f, -1);
    }
}

void Game::keyShortcuts() {
    if (input.isPressed(K_ESCAPE)) {
        setMode(MODE_PAUSE);
        return;
    }
    if (input.isPressed(K_F1)) showDebug = !showDebug;
    if (input.isPressed(K_F2)) showHelp = !showHelp;
    if (input.isPressed(K_F5)) {
        if (playMode == PLAY_SANDBOX) { if (saveGame("quicksave")) toast("Sandbox saved", 1); }
        else toast("Quick save is only available in sandbox", 2);
    }
    if (input.isPressed(K_F9)) {
        if (playMode == PLAY_SANDBOX) { if (loadGame("quicksave")) toast("Sandbox loaded", 1); }
    }
    if (input.isPressed(K_F3)) showMinimap = !showMinimap;
    if (playMode == PLAY_SANDBOX && drivenVehicle < 0) {
        if (input.isPressed(K_V)) {
            if (!sandbox.vehicleSpawning) { toast("Vehicle spawning disabled in sandbox options", 2); }
            else {
                vec3 eye = player.pos + vec3(0, player.cameraHeight, 0);
                vec3 fwd(std::cos(player.pitch) * std::sin(player.yaw), 0, -std::cos(player.pitch) * std::cos(player.yaw));
                vec3 p = eye + fwd.norm() * 6.0f;
                p.y = world.groundHeightM(p.x, p.z) + 0.2f;
                int kinds[3] = {VK_SEDAN, VK_PICKUP, VK_BULLDOZER};
                static int vk = 0;
                int kind = kinds[vk++ % 3];
                if (vehicles.spawn((VehicleKind)kind, p, player.yaw) >= 0)
                    toast(std::string("Spawned ") + vehicleDef(kind).name, 1);
            }
        }
        if (input.isPressed(K_T)) {
            sandbox.timeOfDay = (sandbox.timeOfDay + 3) % 24;
            updateEnvironment(0.0f);
            char buf[64];
            snprintf(buf, sizeof(buf), "Time of day: %02d:00", sandbox.timeOfDay);
            toast(buf, 0);
        }
        if (input.isPressed(K_G)) {
            sandbox.weather = (sandbox.weather + 2) % 6;
            updateEnvironment(0.0f);
            static const char* names[6] = {"Clear", "Cloudy", "Rain", "Storm", "Fog", "Clear"};
            toast(std::string("Weather: ") + names[sandbox.weather], 0);
        }
        if (input.isPressed(K_B)) {
            vec3 eye = player.pos + vec3(0, player.cameraHeight, 0);
            vec3 fwd(std::cos(player.pitch) * std::sin(player.yaw), std::sin(player.pitch), -std::cos(player.pitch) * std::cos(player.yaw));
            RayHit h = world.raycast(eye, fwd, 8.0f);
            if (h.hit) {
                physics.spawnProp(h.pos + h.normal * 0.5f, (int)(hash32((uint32_t)(g_time * 100)) % 5));
                toast("Prop spawned", 0);
            }
        }
        if (input.isPressed(K_N)) {
            resetMap();
            toast("Map reset", 0);
        }
    }
    if (input.isPressed(K_TAB)) showHelp = !showHelp;
}

void Game::updatePlaying(float dt) {
    playTime += dt;
    g_gameTime += dt;
    updateEnvironment(dt);
    updatePlayerAndTools(dt);
    updatePickups(dt);
    updateDoors(dt);
    updateCollapses(dt);
    // world streaming: rebuild a slice of dirty chunk meshes each frame
    world.rebuildDirtyMeshes(3);
    // security systems: cameras, turrets, alarm clock
    {
        bool visible = drivenVehicle < 0 ? true : true;
        security.update(dt, player.pos, visible, &player);
        if (security.alarm) {
            securityHudPulse += dt;
            if (playMode == PLAY_CAMPAIGN) g_missions.state.alarmTriggered = true;
        }
    }
    // mission logic
    if (playMode == PLAY_CAMPAIGN && g_missions.state.active) {
        int before = 0;
        for (auto& o : g_missions.get(g_missions.state.missionId).objectives) if (o.complete) before++;
        g_missions.update(dt, world, mapData, player, vehicles.vehicles, interactEvents);
        int after = 0;
        for (auto& o : g_missions.get(g_missions.state.missionId).objectives) if (o.complete) after++;
        if (after > before) {
            objectiveFlash = 3.0f;
            audio::play(SFX_PICKUP_ITEM, player.pos, 0.6f, 1.0f);
            toast("Objective complete", 1);
        }
        if (g_missions.state.failed) {
            mode = MODE_MISSION_FAILED;
            audio::play(SFX_MISSION_FAIL, player.pos, 0.8f, 1.0f);
            platform::setMouseCapture(false);
        } else if (g_missions.state.completed) {
            mode = MODE_MISSION_COMPLETE;
            audio::play(SFX_MISSION_COMPLETE, player.pos, 0.9f, 1.0f);
            platform::setMouseCapture(false);
        }
        if (g_missions.state.timed) {
            float remaining = g_missions.state.timeLimit - g_missions.state.time;
            if (remaining < 10.0f && remaining > 0) vignettePulse += dt * 3.0f;
        }
    }
    // death
    if (player.dead && !(playMode == PLAY_SANDBOX && sandbox.invincible)) {
        deathTimer += dt;
        if (deathTimer > 1.2f) {
            if (playMode == PLAY_CAMPAIGN) {
                g_missions.state.failed = true;
                g_missions.state.failReason = "You were killed";
                mode = MODE_MISSION_FAILED;
                platform::setMouseCapture(false);
            } else {
                player.pos = getMap(currentMap).info.spawn;
                player.pos.y += 1.0f;
                player.health = 100;
                player.dead = false;
                player.vel = vec3(0);
                deathTimer = 0;
                toast("Respawned", 0);
            }
        }
    }
    // ambient sounds
    static float ambTimer = 0;
    ambTimer -= dt;
    if (ambTimer <= 0) {
        ambTimer = 4.0f + hash3f((int)(g_time * 10), 5, 5) * 6.0f;
        if (env.weather == 3) audio::play(SFX_STRUCTURE_CREAK, player.pos + vec3(20, 10, 20), 0.18f, 0.8f);
    }
    interactEvents = 0;
}

void Game::updateEnvironment(float dt) {
    const MapInfo& mi = getMap(currentMap).info;
    int hour = (playMode == PLAY_SANDBOX && sandbox.timeOfDay >= 0) ? sandbox.timeOfDay : mi.defaultHour;
    if (playMode == PLAY_SANDBOX && sandbox.timeOfDay != hour) hour = sandbox.timeOfDay;
    float t = (float)hour / 24.0f;
    // sun arc
    float sunAngle = (t - 0.25f) * 2.0f * PI;
    vec3 sunDir(std::cos(sunAngle) * 0.75f, std::sin(sunAngle), std::cos(sunAngle) * 0.45f + 0.15f);
    sunDir = sunDir.norm();
    float dayFactor = clampf(sunDir.y * 1.6f + 0.15f, 0.0f, 1.0f);
    int weather = (playMode == PLAY_SANDBOX && sandbox.weather >= 0) ? std::min(sandbox.weather, 4) : mi.weather.kind;
    // base colours from the map, modulated by time of day
    vec3 daySky = mi.skyTop, dayHor = mi.skyHorizon;
    float dusk = 1.0f - std::min(1.0f, std::fabs(sunDir.y) * 3.2f);
    vec3 duskTint(dusk * 0.55f, dusk * 0.20f, dusk * 0.02f);
    env.sunDir = sunDir;
    env.sunColor = vlerp(vec3(0.55f, 0.66f, 0.95f), mi.sunColor, dayFactor) + duskTint * 1.2f;
    env.sunIntensity = clampf(dayFactor * 1.25f, 0.22f, 1.3f);   // moonlight floor at night
    vec3 nightSky(0.015f, 0.025f, 0.055f), nightHor(0.05f, 0.07f, 0.13f);
    env.skyTop = vlerp(nightSky, daySky, dayFactor * 0.9f + 0.1f);
    env.skyHorizon = vlerp(nightHor, dayHor, dayFactor * 0.85f + 0.1f) + duskTint * 0.35f;
    env.ambientSky = vlerp(vec3(0.06f, 0.08f, 0.13f), mi.ambientSky, dayFactor * 0.9f + 0.1f);
    env.ambientGround = vlerp(vec3(0.03f, 0.03f, 0.04f), mi.ambientGround, dayFactor * 0.9f + 0.1f);
    env.fogColor = vlerp(vec3(0.06f, 0.08f, 0.13f), mi.fogColor, dayFactor * 0.85f + 0.12f) + duskTint * 0.25f;
    // night keeps enough ambient light to navigate: never go pitch black
    // playability floor: interiors and nights stay readable instead of pitch black
    float minLight = 0.42f;
    env.ambientSky = vmax(env.ambientSky, vec3(minLight * 0.72f, minLight * 0.80f, minLight));
    env.ambientGround = vmax(env.ambientGround, vec3(minLight * 0.40f, minLight * 0.38f, minLight * 0.36f));
    // weather
    float targetCloud = mi.weather.cloud, targetRain = mi.weather.rain, targetWet = mi.weather.wetness;
    float targetFog = mi.fogDensity;
    if (weather == 0) { targetCloud = 0.12f; targetRain = 0; targetWet = 0; }
    else if (weather == 1) { targetCloud = 0.7f; targetRain = 0; targetWet = 0.15f; }
    else if (weather == 2) { targetCloud = 0.85f; targetRain = 1.0f; targetWet = 1.0f; targetFog *= 1.6f; }
    else if (weather == 3) { targetCloud = 1.0f; targetRain = 1.0f; targetWet = 1.0f; targetFog *= 2.4f; }
    else if (weather == 4) { targetCloud = 0.55f; targetRain = 0.0f; targetWet = 0.35f; targetFog *= 4.0f; }
    env.cloudCover = lerpf(env.cloudCover, targetCloud, dt * 0.7f + 0.02f);
    env.rainAmount = lerpf(env.rainAmount, targetRain, dt * 1.2f);
    env.wetness = lerpf(env.wetness, targetWet, dt * 0.4f);
    env.fogDensity = lerpf(env.fogDensity, targetFog, dt * 0.5f + 0.02f);
    env.weather = weather;
    env.time += dt;
    env.exposure = settings.exposure;
    if (env.weather == 3 && dt > 0) {
        static float lightningTimer = 6.0f;
        lightningTimer -= dt;
        if (lightningTimer <= 0) {
            lightningTimer = 7.0f + hash3f((int)(g_time * 10), 3, 3) * 12.0f;
            env.lightning = 1.0f;
            audio::play(SFX_EXPLOSION_SMALL, player.pos + vec3(60, 60, 60), 0.35f, 0.5f);
        }
        env.lightning = std::max(0.0f, env.lightning - dt * 3.5f);
    } else {
        env.lightning = 0;
    }
    // rain particles around the player
    if (env.rainAmount > 0.2f && mode == MODE_PLAYING) {
        int drops = (int)(env.rainAmount * 26.0f * std::max(1, settings.particleQuality) / 2);
        for (int i = 0; i < drops; i++) {
            vec3 p = player.pos + vec3((hash3f((int)(g_time * 300), i, 1) - 0.5f) * 34.0f,
                                       hash3f((int)(g_time * 300), i, 2) * 18.0f + 2.0f,
                                       (hash3f((int)(g_time * 300), i, 3) - 0.5f) * 34.0f);
            Particle& pp = fx.alloc();
            pp.pos = p;
            pp.vel = vec3(env.wind * 0.3f, -22.0f, 0);
            pp.size = 0.012f;
            pp.sizeVel = 0;
            pp.maxLife = 0.55f;
            pp.life = 0.55f;
            pp.gravity = 4.0f;
            pp.color = packColor(0.7f, 0.78f, 0.88f, 0.7f);
            pp.kind = 6;
            pp.rot = (uint8_t)i;
        }
    }
}

void Game::updatePlayerAndTools(float dt) {
    // camera-relative aiming
    if (drivenVehicle >= 0 && drivenVehicle < (int)vehicles.vehicles.size()) {
        Vehicle& v = vehicles.vehicles[drivenVehicle];
        vehicles.drive(v, dt, input);
        player.pos = v.pos;
        player.yaw += input.mouseDX * 0.0022f;
        player.pitch = clampf(player.pitch - input.mouseDY * 0.0022f, -0.9f, 0.7f);
        if (input.isPressed(K_E) && player.vehicleEnterCooldown <= 0) {
            vehicles.exitVehicle(v);
            player.pos = v.pos + vec3(0, 1.2f, 0);
            drivenVehicle = -1;
            player.vehicleEnterCooldown = 0.5f;
            toast("Exited vehicle", 0);
        }
        // aim ray for HUD
        vec3 eye = v.pos + vec3(0, 2.0f, 0);
        vec3 dir(std::cos(player.pitch) * std::sin(player.yaw), std::sin(player.pitch), -std::cos(player.pitch) * std::cos(player.yaw));
        player.aimHit = world.raycast(eye, dir, 6.0f);
        player.aimValid = player.aimHit.hit;
        if (input.isPressed(K_L)) v.lightsOn = !v.lightsOn;
    } else {
        player.vehicleEnterCooldown = std::max(0.0f, player.vehicleEnterCooldown - dt);
        player.updateInput(input, world, physics, dt, false);
        // standing on physics bodies (planks, crates)
        for (auto& b : physics.bodies) {
            if (!b.active) continue;
            vec3 d = player.pos - b.pos;
            float top = b.box.mx.y;
            float bottom = b.box.mn.y;
            if (player.pos.y >= top - 0.35f && player.pos.y <= top + 0.10f && player.vel.y <= 0.05f) {
                if (std::fabs(d.x) < b.box.size().x * 0.55f + player.radius &&
                    std::fabs(d.z) < b.box.size().z * 0.55f + player.radius) {
                    player.pos.y = top;
                    player.vel.y = 0;
                    player.grounded = true;
                }
            }
            // simple horizontal push out of solid bodies
            if (player.pos.y + 0.9f > bottom && player.pos.y < top - 0.1f) {
                float rx = b.box.size().x * 0.5f + player.radius;
                float rz = b.box.size().z * 0.5f + player.radius;
                if (std::fabs(d.x) < rx && std::fabs(d.z) < rz) {
                    float px = rx - std::fabs(d.x), pz = rz - std::fabs(d.z);
                    if (px < pz) player.pos.x += (d.x > 0 ? px : -px);
                    else player.pos.z += (d.z > 0 ? pz : -pz);
                }
            }
        }
        // enter vehicles
        if (input.isPressed(K_E) && player.vehicleEnterCooldown <= 0 && !player.carried) {
            int idx = -1;
            if (vehicles.tryEnter(player.pos, idx)) {
                drivenVehicle = idx;
                Vehicle& v = vehicles.vehicles[idx];
                player.yaw = v.yaw;
                player.pitch = -0.1f;
                player.vehicleEnterCooldown = 0.35f;
                audio::play(SFX_ENGINE_START, v.pos, 0.7f, 1.0f);
                toast(std::string("Driving ") + vehicleDef(v.kind).name + " - WASD drive, E exit", 0);
            } else {
                player.interact(world, physics);
            }
        } else if (input.isPressed(K_E) && player.carried) {
            // throw the carried object
            Body* b = player.carried;
            b->active = true;
            b->sleeping = false;
            vec3 dir(std::cos(player.pitch) * std::sin(player.yaw), std::sin(player.pitch), -std::cos(player.pitch) * std::cos(player.yaw));
            b->vel = dir * 11.0f + vec3(0, 2.0f, 0);
            b->angVel = vec3(3, 2, 1);
            player.carried = nullptr;
            audio::play(SFX_OBJECT_THROW, player.pos, 0.5f, 1.0f);
        }
        // carried object follows the view
        if (player.carried) {
            vec3 eye = player.pos + vec3(0, player.cameraHeight, 0);
            vec3 dir(std::cos(player.pitch) * std::sin(player.yaw), std::sin(player.pitch), -std::cos(player.pitch) * std::cos(player.yaw));
            player.carried->pos = eye + dir * player.carryDist;
            player.carried->vel = vec3(0);
            player.carried->angVel = vec3(0);
            player.carried->sleeping = true;
            physics.updateModel(*player.carried);
        }
    }
    // aim ray
    if (drivenVehicle < 0) {
        vec3 eye = player.pos + vec3(0, player.cameraHeight, 0);
        vec3 dir(std::cos(player.pitch) * std::sin(player.yaw), std::sin(player.pitch), -std::cos(player.pitch) * std::cos(player.yaw));
        player.aimHit = world.raycast(eye, dir, 6.0f);
        player.aimValid = player.aimHit.hit;
    }
    // tool hits knock out security hardware
    if (player.aimHit.hit) security.damageRay(player.aimHit, 26.0f * dt * 6.0f);
    // tools
    tools.unlimitedAmmo = (playMode == PLAY_SANDBOX && sandbox.unlimitedAmmo) || false;
    tools.infiniteFuel = (playMode == PLAY_SANDBOX && sandbox.infiniteFuel);
    tools.update(dt, input, player, world, physics, drivenVehicle >= 0);
    // physics
    physics.update(dt);
    for (auto& v : vehicles.vehicles) {
        if (v.destroyed) continue;
        if ((v.pos - player.pos).len() > 260.0f) continue;
    }
    vehicles.update(dt, drivenVehicle);
    // vehicle impacts -> sound
    for (int idx : physics.impactEvents) {
        if (idx < 0 || idx >= (int)physics.bodies.size()) continue;
        Body& b = physics.bodies[idx];
        float vol = clampf(b.mass / 300.0f, 0.15f, 0.9f);
        audio::play(material(b.mat).density > 2000 ? SFX_DEBRIS_BIG : SFX_DEBRIS_SMALL, b.pos, vol, 0.8f + hash3f(idx, 1, 1) * 0.4f);
        if (b.mass > 400.0f) {
            g_fx->dustBurst(b.pos, 3, 0.6f, 1.0f, vec3(0.6f, 0.58f, 0.52f));
            player.addShake(clampf(b.mass / 4000.0f, 0.05f, 0.4f) * clampf(1.0f - (b.pos - player.pos).len() / 40.0f, 0, 1));
        }
    }
    physics.impactEvents.clear();
    // particles
    fx.update(dt, player.pos);
    // audio listener
    Camera cam = currentCamera();
    audio::setListener(cam.pos, cam.forward(), vec3(0, 1, 0));
    audio::update(dt);
    // damage flash
    if (player.health < 40 && playMode == PLAY_SANDBOX && !sandbox.invincible) {
        vignettePulse += dt * 2.0f;
    }
    // sandbox death guard
    if (playMode == PLAY_SANDBOX) {
        if (sandbox.invincible) player.health = player.maxHealth;
        if (player.pos.y < -30.0f) {
            player.pos = getMap(currentMap).info.spawn;
            player.vel = vec3(0);
            player.health = player.maxHealth;
            toast("Out of bounds - respawned", 2);
        }
    }
}

void Game::updatePickups(float dt) {
    for (auto& p : pickups) {
        if (!p.active) continue;
        p.bob += dt * 2.0f;
        // gravity + ground
        p.vel.y -= 9.8f * dt;
        p.pos += p.vel * dt;
        float gy = world.groundHeightM(p.pos.x, p.pos.z);
        if (p.pos.y < gy + 0.25f) {
            p.pos.y = gy + 0.25f;
            p.vel = vec3(0);
        }
        vec3 d = p.pos - player.pos;
        if (d.len() < 2.1f) {
            if (p.missionItem >= 0) {
                g_missions.state.collected++;
                char buf[64];
                snprintf(buf, sizeof(buf), "Evidence recovered (%d)", g_missions.state.collected);
                toast(buf, 1);
            } else if (p.kind == 1) {
                for (int i = 0; i < TOOL_COUNT; i++) tools.ammo[i] = toolDef(i).maxAmmo;
                toast("Ammunition restocked", 1);
            } else if (p.kind == 2) {
                tools.torchFuel = 1.0f;
                tools.extinguisherCharge = 1.0f;
                toast("Fuel and extinguisher refilled", 1);
            } else if (p.kind == 3) {
                player.heal(50);
                toast("Health restored", 1);
            }
            audio::play(SFX_PICKUP_ITEM, p.pos, 0.7f, 1.0f);
            g_fx->sparks(p.pos, vec3(0, 1, 0), 6, 3.0f);
            p.active = false;
        }
    }
    for (int i = (int)pickups.size() - 1; i >= 0; i--)
        if (!pickups[i].active) pickups.erase(pickups.begin() + i);
}

void Game::spawnPickup(vec3 pos, int kind, int missionItem) {
    Pickup p;
    p.pos = pos;
    p.kind = kind;
    p.missionItem = missionItem;
    pickups.push_back(p);
}

void Game::buildDoorsFromMap() {
    // doors are decorative + interactive panels scattered in interiors; generated from the map's doorways
    // (kept simple and robust: they swing open when the player is close and can be smashed)
    doors.clear();
}

void Game::updateDoors(float dt) {
    for (auto& d : doors) {
        if (!d.alive) continue;
        if (d.opening) d.open = std::min(1.0f, d.open + dt * 2.4f);
        else d.open = std::max(0.0f, d.open - dt * 2.0f);
        vec3 panelCenter = d.hinge + d.dir * (d.width * 0.5f);
        if ((panelCenter - player.pos).len() < 2.6f) d.opening = true;
        else if ((panelCenter - player.pos).len() > 3.4f) d.opening = false;
    }
}

void Game::scheduleCollapseAnalysis(const AABB& box) {
    if (collapseJobs.size() >= 4) return;
    CollapseJob j;
    j.box = box;
    j.delay = 0.12f;
    collapseJobs.push_back(j);
}

void Game::updateCollapses(float dt) {
    int budget = 1;
    for (int i = (int)collapseJobs.size() - 1; i >= 0; i--) {
        collapseJobs[i].delay -= dt;
        if (collapseJobs[i].delay > 0) continue;
        if (budget-- <= 0) continue;
        AABB region = collapseJobs[i].box;
        collapseJobs.erase(collapseJobs.begin() + i);
        std::vector<FloatingCluster> clusters;
        analyzeSupport(world, ivec3((int)(region.mn.x / VOX), (int)(region.mn.y / VOX), (int)(region.mn.z / VOX)),
                       ivec3((int)(region.mx.x / VOX), (int)(region.mx.y / VOX), (int)(region.mx.z / VOX)), clusters);
        int piecesBudget = 6 + settings.destructionQuality * 7;
        int collapseCount = 0;
        for (auto& c : clusters) {
            if (c.cells.size() < 4) continue;
            if (collapseCount++ > 6) break;
            // group cells into pieces of ~2x2x2 coarse cells
            struct Group {
                ivec3 mn{1 << 20, 1 << 20, 1 << 20}, mx{-1, -1, -1};
                std::vector<SupportCell> cells;
            };
            std::vector<Group> groups;
            for (auto& cell : c.cells) {
                ivec3 g(cell.x >> 1, cell.y >> 1, cell.z >> 1);
                Group* found = nullptr;
                for (auto& gr : groups) {
                    if ((gr.mn.x >> 1) == g.x && (gr.mn.y >> 1) == g.y && (gr.mn.z >> 1) == g.z) { found = &gr; break; }
                }
                if (!found) {
                    groups.push_back(Group());
                    groups.back().mn = {g.x * 2, g.y * 2, g.z * 2};
                    groups.back().mx = {g.x * 2 + 1, g.y * 2 + 1, g.z * 2 + 1};
                    found = &groups.back();
                }
                found->cells.push_back(cell);
            }
            for (auto& gr : groups) {
                if ((int)piecesBudget <= 0) break;
                piecesBudget--;
                // determine dominant material and clear the voxels
                int counts[M_COUNT] = {0};
                int x0 = gr.mn.x * 4, y0 = gr.mn.y * 4, z0 = gr.mn.z * 4;
                int x1 = (gr.mx.x + 1) * 4, y1 = (gr.mx.y + 1) * 4, z1 = (gr.mx.z + 1) * 4;
                int removed = 0;
                for (int y = y0; y < y1; y++)
                    for (int z = z0; z < z1; z++)
                        for (int x = x0; x < x1; x++) {
                            uint8_t m = world.at(x, y, z);
                            if (!matSolid(m)) continue;
                            counts[m]++;
                            world.setFast(x, y, z, M_AIR);
                            removed++;
                        }
                if (removed == 0) continue;
                int dom = M_CONCRETE, best = 0;
                for (int m = 1; m < M_COUNT; m++) if (counts[m] > best) { best = counts[m]; dom = m; }
                vec3 mn((float)x0 * VOX, (float)y0 * VOX, (float)z0 * VOX);
                vec3 mx((float)x1 * VOX, (float)y1 * VOX, (float)z1 * VOX);
                vec3 center = (mn + mx) * 0.5f;
                vec3 half = (mx - mn) * 0.5f;
                // shrink the body a little so pieces don't interpenetrate
                half = vec3(std::max(0.16f, half.x * 0.94f), std::max(0.16f, half.y * 0.94f), std::max(0.16f, half.z * 0.94f));
                physics.spawnCollapsePiece(center, half, (uint8_t)dom, vec3((hash3f(x0, y0, z0) - 0.5f) * 0.8f, -0.4f, (hash3f(z0, x0, y0) - 0.5f) * 0.8f));
                totalDestroyedVoxels += removed;
                stats.destroyedVoxels += (float)removed;
                g_fx->dustBurst(center, 2, half.len() * 0.5f, 1.2f, material(dom).color);
            }
            // collapse sound + dust once per cluster
            audio::play(SFX_STRUCTURE_CREAK, c.boundsM.center(), clampf(c.cells.size() / 60.0f, 0.25f, 0.9f), 1.0f);
            audio::play(SFX_COLLAPSE, c.boundsM.center(), clampf(c.cells.size() / 80.0f, 0.2f, 0.85f), 0.9f);
            g_fx->dustBurst(c.boundsM.center(), (int)clampf(c.cells.size() * 0.4f, 3, 16), c.boundsM.size().len() * 0.2f, 2.0f,
                            vec3(0.62f, 0.58f, 0.52f));
            player.addShake(clampf(c.cells.size() / 200.0f, 0.05f, 0.5f) *
                            clampf(1.0f - (c.boundsM.center() - player.pos).len() / 80.0f, 0, 1));
        }
        world.markChunkDirty((int)(region.mn.x / VOX), (int)(region.mn.y / VOX), (int)(region.mn.z / VOX));
        for (auto& c : clusters) {
            for (auto& cell : c.cells) world.markChunkDirty(cell.x * 4, cell.y * 4, cell.z * 4);
        }
    }
}

void Game::carveRegion(const vec3& center, float radius, float power, bool structural, bool ignite) {
    World::CarveParams cp;
    cp.centerM = center;
    cp.radiusM = radius;
    cp.power = power;
    cp.damageMul = (playMode == PLAY_SANDBOX) ? sandbox.explosionPower * 0.5f + 0.5f : 1.0f;
    cp.structural = structural;
    cp.ignite = ignite;
    World::CarveResult res = world.carve(cp);
    if (!res.any) return;
    totalDestroyedVoxels += res.removed;
    stats.destroyedVoxels += (float)res.removed;
    if (res.removed > 2) {
        AABB box = res.box;
        // spawn a few physics chunks for medium+ impacts
        if (res.removed > 22) {
            int q = playMode == PLAY_SANDBOX ? sandbox.debrisDensity : settings.debrisDensity;
            physics.spawnDebrisBurst(center, box, res.matCounts, q, std::max(1.0f, power * 0.05f));
        }
        // structural check on a generous region around the damage
        if (structural && res.removed > 8) {
            float expand = 9.0f;
            AABB region = AABB(vmax(box.mn - vec3(expand), vec3(0)),
                               vmin(box.mx + vec3(expand), vec3((float)world.sx * VOX, (float)world.sy * VOX, (float)world.sz * VOX)));
            scheduleCollapseAnalysis(region);
        }
        if (ignite) fx.igniteArea(center, radius * 1.4f);
    }
}

void Game::explode(const vec3& center, float radius, int source) {
    totalExplosions++;
    stats.explosions++;
    float power = (playMode == PLAY_SANDBOX ? sandbox.explosionPower : 1.0f);
    float r = radius * (0.85f + power * 0.15f);
    fx.explosion(center, r, 3.0f * power, r > 5.0f);
    audio::play(SFX_EXPLOSION_LARGE, center, 1.0f, 0.9f + hash3f((int)center.x, 7, (int)center.z) * 0.2f);
    physics.applyRadialImpulse(center, r * 4.0f, 2.4f * power);
    security.damageArea(center, r * 2.2f, 140.0f * power);
    // carve multiple shells for a rounded crater with rough edges
    carveRegion(center, r * 0.62f, 12000.0f * power, true, true);
    carveRegion(center, r * 0.95f, 6000.0f * power, true, false);
    // light
    LightInstance lf;
    lf.pos = center;
    lf.color = vec3(1.0f, 0.62f, 0.28f);
    lf.radius = r * 6.0f;
    lf.intensity = 2.2f;
    fx.lights.push_back(lf);
    // damage the player if too close
    float pd = (player.pos + vec3(0, 0.9f, 0) - center).len();
    if (pd < r * 1.6f && !(playMode == PLAY_SANDBOX && sandbox.invincible)) {
        float dmg = (1.0f - pd / (r * 1.6f)) * 65.0f * power;
        player.damage(dmg, g_time);
        damageFlash = 1.0f;
        player.vel += (player.pos - center).norm() * (1.0f - pd / (r * 1.6f)) * 14.0f;
        player.vel.y += 4.0f;
        player.grounded = false;
    }
    player.addShake(clampf((1.0f - pd / (r * 8.0f)) * 1.4f, 0, 1.2f));
    if (source == 2) {
        // airstrike bombing: extra debris spray
        for (int i = 0; i < 6; i++)
            physics.spawn(center + vec3((hash3f(i, 1, 1) - 0.5f) * r, hash3f(i, 2, 2) * 2.0f, (hash3f(i, 3, 3) - 0.5f) * r),
                          vec3(0.14f + hash3f(i, 4, 4) * 0.2f), M_CONCRETE, 24.0f,
                          vec3((hash3f(i, 5, 5) - 0.5f) * 12, 6.0f + hash3f(i, 6, 6) * 10, (hash3f(i, 7, 7) - 0.5f) * 12),
                          vec3(4, 4, 4), 12.0f, false);
    }
}

// ---------------------------------------------------------------- save / load
namespace {
struct SaveHeader {
    uint32_t magic = 0x46524354;   // FRCT
    uint32_t version = 2;
    int mapId = 0;
    int playMode = 0;
    int missionId = -1;
    float px = 0, py = 0, pz = 0, yaw = 0, pitch = 0;
    float health = 100;
    float playTime = 0;
    int totalDestroyed = 0;
    int totalExplosions = 0;
    SandboxOptions sandbox;
    int ammo[TOOL_COUNT] = {0};
    float torchFuel = 1, extCharge = 1;
    int pickupCount = 0;
    size_t worldBytes = 0;
};
}

bool Game::saveGame(const std::string& slot) {
    if (playMode != PLAY_SANDBOX) return false;
    std::vector<uint8_t> blob;
    serializeWorld(world, blob);
    SaveHeader h;
    h.mapId = currentMap;
    h.playMode = (int)playMode;
    h.missionId = g_missions.state.missionId;
    h.px = player.pos.x; h.py = player.pos.y; h.pz = player.pos.z;
    h.yaw = player.yaw; h.pitch = player.pitch;
    h.health = player.health;
    h.playTime = playTime;
    h.totalDestroyed = totalDestroyedVoxels;
    h.totalExplosions = totalExplosions;
    h.sandbox = sandbox;
    for (int i = 0; i < TOOL_COUNT; i++) h.ammo[i] = tools.ammo[i];
    h.torchFuel = tools.torchFuel;
    h.extCharge = tools.extinguisherCharge;
    h.worldBytes = blob.size();
    std::string path = savePath(slot);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(&h, sizeof(h), 1, f);
    fwrite(blob.data(), 1, blob.size(), f);
    // pickups
    uint32_t np = (uint32_t)pickups.size();
    fwrite(&np, 4, 1, f);
    for (auto& p : pickups) {
        fwrite(&p.pos, sizeof(vec3), 1, f);
        fwrite(&p.kind, 4, 1, f);
        fwrite(&p.missionItem, 4, 1, f);
    }
    fclose(f);
    logMsg("[save] wrote %s (%zu world bytes)", path.c_str(), blob.size());
    return true;
}

bool Game::loadGame(const std::string& slot) {
    std::string path = savePath(slot);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    SaveHeader h;
    if (fread(&h, sizeof(h), 1, f) != 1 || h.magic != 0x46524354) { fclose(f); return false; }
    if (h.mapId < 0 || h.mapId >= mapCount()) { fclose(f); return false; }
    std::vector<uint8_t> blob(h.worldBytes);
    if (h.worldBytes && fread(blob.data(), 1, blob.size(), f) != blob.size()) { fclose(f); return false; }
    std::vector<Pickup> savedPickups;
    uint32_t np = 0;
    if (fread(&np, 4, 1, f) == 1 && np < 10000) {
        for (uint32_t i = 0; i < np; i++) {
            Pickup p;
            int kind = 0, mi = -1;
            if (fread(&p.pos, sizeof(vec3), 1, f) != 1) break;
            if (fread(&kind, 4, 1, f) != 1) break;
            if (fread(&mi, 4, 1, f) != 1) break;
            p.kind = kind;
            p.missionItem = mi;
            savedPickups.push_back(p);
        }
    }
    fclose(f);
    // rebuild the map then apply the saved voxel edits
    loadMap(h.mapId, false, -1);
    // finish loading synchronously (loadMap queued meshes)
    while (loading.stage != 0) advanceLoading(*this);
    playMode = PLAY_SANDBOX;
    sandbox = h.sandbox;
    if (!deserializeWorld(world, blob.data(), blob.size())) {
        logMsg("[load] world deserialize failed");
    }
    world.markAllDirty();
    pickups = savedPickups;
    player.pos = vec3(h.px, h.py, h.pz);
    player.yaw = h.yaw;
    player.pitch = h.pitch;
    player.health = h.health;
    playTime = h.playTime;
    totalDestroyedVoxels = h.totalDestroyed;
    totalExplosions = h.totalExplosions;
    for (int i = 0; i < TOOL_COUNT; i++) tools.ammo[i] = h.ammo[i];
    tools.torchFuel = h.torchFuel;
    tools.extinguisherCharge = h.extCharge;
    mode = MODE_PLAYING;
    platform::setMouseCapture(true);
    toast("Sandbox restored", 1);
    logMsg("[load] restored %s", path.c_str());
    return true;
}

void Game::restartLevel() {
    menuAfterLoad = false;
    if (playMode == PLAY_CAMPAIGN && g_missions.state.missionId >= 0) startMission(g_missions.state.missionId);
    else startSandbox(currentMap);
}

void Game::resetMap() {
    int mapId = currentMap;
    bool campaign = playMode == PLAY_CAMPAIGN;
    int mission = g_missions.state.missionId;
    loadMap(mapId, campaign, mission);
}

void Game::startSandbox(int mapId) {
    menuAfterLoad = false;
    playMode = PLAY_SANDBOX;
    loadMap(mapId, false, -1);
}

void Game::startMission(int missionId) {
    menuAfterLoad = false;
    const Mission& m = g_missions.get(missionId);
    playMode = PLAY_CAMPAIGN;
    g_missions.start(missionId);
    loadMap(m.mapId, true, missionId);
}

void Game::returnToMenu() {
    menuAfterLoad = true;
    g_missions.abort();
    audio::startMusic(0);
    loading.campaign = false;
    loading.missionId = -1;
    loadMap(0, false, -1);
    mode = MODE_LOADING;
}

const char* Game::modeName() const {
    switch (mode) {
        case MODE_MAIN_MENU: return "main menu";
        case MODE_CAMPAIGN: return "campaign";
        case MODE_SANDBOX_SETUP: return "sandbox setup";
        case MODE_SETTINGS: return "settings";
        case MODE_CONTROLS: return "controls";
        case MODE_CREDITS: return "credits";
        case MODE_LOADING: return "loading";
        case MODE_PLAYING: return "playing";
        case MODE_PAUSE: return "paused";
        case MODE_MISSION_COMPLETE: return "complete";
        case MODE_MISSION_FAILED: return "failed";
        case MODE_SAVE_LOAD: return "save/load";
    }
    return "?";
}
