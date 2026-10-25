// ============================================================
// SPACE MINER  -  game_core.cpp
// Lifecycle, save system, stat derivation, world generation, spawning.
// ============================================================
#include "game.h"
#include "content.h"
#include "art.h"
#include "platform.h"
#include <cstdio>

namespace sm {

static const char* SAVE_FILE = "space_miner_save.dat";
static const char* SAVE_BAK = "space_miner_save.bak";

// ---------------------------------------------------------------- lifecycle
bool Game::init(bool headlessMode) {
    headless = headlessMode;
    rng.seed(0x5EED1234u);
    assets = std::unique_ptr<AssetBank>(new AssetBank());
    loadStage = "GENERATING SPRITES";
    assets->init(-1);
    loadStage = "CALIBRATING REACTOR";
    achievements.assign(ACH_COUNT, AchieveState{});
    for (int i = 0; i < RES_COUNT; i++) save.cargo[i] = 0.f;
    newGame();
    screen = 1;
    inited = true;
    audio::setMasterVolume(save.settings.masterVolume);
    return true;
}

void Game::bootStage(int stage) {
    static const char* stages[5] = {"HULL PLATING", "STATION TELEMETRY", "ASTEROID SURVEY",
                                    "NEBULA MAPPING", "INSTRUMENT CALIBRATION"};
    rng.seed(0x5EED1234u);
    if (!assets) assets = std::unique_ptr<AssetBank>(new AssetBank());
    loadStage = stages[clampv(stage, 0, 4)];
    assets->init(stage);
    loadProgress = (stage + 1) / 5.f;
    uiTime += .05f;
}

void Game::bootFinish() {
    loadStage = "CALIBRATING REACTOR";
    achievements.assign(ACH_COUNT, AchieveState{});
    for (int i = 0; i < RES_COUNT; i++) save.cargo[i] = 0.f;
    newGame();
    screen = 1;
    inited = true;
    audio::setMasterVolume(save.settings.masterVolume);
    // pick up an existing save so CONTINUE works from the menu
    if (saveExists()) {
        SaveData keep = save;
        (void)keep;
    }
}

void Game::shutdown() {
    if (!headless) saveGame();
    assets.reset();
}

void Game::newGame() {
    save = SaveData();
    save.valid = true;
    save.credits = 500;
    save.ship = SHIP_PROSPECTOR;
    save.unlockedShips = 1;
    save.unlockedSectors = 1;
    save.currentSector = 0;
    save.droneCount = 0;
    achievements.assign(ACH_COUNT, AchieveState{});
    sector = 0;
    playTime = 0;
    tutorialStep = 0;
    tutorialT = 12.f;
    enterSector(0, true);
    hull = hullMax = statHull();
    shield = shieldMax = statShield();
    energy = energyMax = 100.f;
    energyLocked = false;
    save.hull = hull;
    save.shield = shield;
    docked = false;
    screen = 2;
    toast("WELCOME TO THE STARTER BELT", SECTORS[0].accent, 4.f);
    statusT = 0;
}

// ---------------------------------------------------------------- stats
float Game::statMiningPower() const {
    return SHIPS[save.ship].miningPower * (1.f + .18f * save.upgrades[UPG_LASER]) *
           (1.f + .02f * (save.level - 1));
}
float Game::statMiningSpeed() const { return 1.f + .12f * save.upgrades[UPG_SPEED]; }
float Game::statCargo() const { return SHIPS[save.ship].cargo + 40.f * save.upgrades[UPG_CARGO]; }
float Game::statSpeed() const { return SHIPS[save.ship].speed * (1.f + .08f * save.upgrades[UPG_ENGINE]); }
float Game::statAccel() const { return SHIPS[save.ship].accel * (1.f + .10f * save.upgrades[UPG_ENGINE]); }
float Game::statHull() const { return SHIPS[save.ship].hull * (1.f + .18f * save.upgrades[UPG_HULL]); }
float Game::statShield() const { return SHIPS[save.ship].shield * (1.f + .20f * save.upgrades[UPG_SHIELD]); }
float Game::statEnergyRegen() const { return SHIPS[save.ship].energyRegen * (1.f + .15f * save.upgrades[UPG_ENERGY]); }
float Game::statScan() const { return SHIPS[save.ship].scanRange + 90.f * save.upgrades[UPG_SCANNER]; }
float Game::statDamage() const { return SHIPS[save.ship].damage * (1.f + .20f * save.upgrades[UPG_WEAPON]); }
float Game::statMagnet() const { return 130.f + 35.f * save.upgrades[UPG_MAGNET]; }
float Game::statRefine() const { return 1.f + .08f * save.upgrades[UPG_REFINERY]; }

float Game::cargoFraction() const { return statCargo() > 0 ? clampf(cargoUsed / statCargo(), 0.f, 1.f) : 0.f; }
float Game::totalCargo() const {
    float t = 0;
    for (int i = 0; i < RES_COUNT; i++) t += save.cargo[i];
    return t;
}

// ---------------------------------------------------------------- save/load
static uint32_t saveHash(const std::string& s) {
    uint32_t h = 2166136261u;
    for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; }
    return h;
}

bool Game::saveExists() {
    return fileExists(std::string(platformSaveDir()) + SAVE_FILE);
}

bool Game::saveGame(bool backup) {
    if (headless) return false;
    std::string body;
    body += "version=3\n";
    body += fmt("credits=%lld\n", save.credits);
    body += fmt("earned=%lld\n", save.totalEarned);
    body += fmt("xp=%d\n", save.xp);
    body += fmt("level=%d\n", save.level);
    body += fmt("ship=%d\n", save.ship);
    body += fmt("ships=%d\n", save.unlockedShips);
    body += fmt("sectors=%d\n", save.unlockedSectors);
    body += fmt("cur_sector=%d\n", save.currentSector);
    body += fmt("drones=%d\n", save.droneCount);
    body += fmt("drone_tier=%d\n", save.droneTier);
    body += fmt("hull=%.3f\n", hull);
    body += fmt("shield=%.3f\n", shield);
    body += fmt("ach1=%d\n", save.achMask);
    body += fmt("ach2=%d\n", save.achMask2);
    body += fmt("mined=%lld\n", save.asteroidsMined);
    body += fmt("kills=%lld\n", save.enemiesKilled);
    body += fmt("jumps=%lld\n", save.jumps);
    body += fmt("playtime=%.1f\n", playTime);
    body += fmt("px=%.1f\npy=%.1f\n", playerPos.x, playerPos.y);
    for (int i = 0; i < UPG_COUNT; i++) body += fmt("upg%d=%d\n", i, save.upgrades[i]);
    for (int i = 0; i < RES_COUNT; i++) body += fmt("res%d=%.2f\n", i, save.cargo[i]);
    // settings
    body += fmt("vol=%.2f\n", save.settings.masterVolume);
    body += fmt("sfx=%.2f\n", save.settings.sfxVolume);
    body += fmt("shake=%d\n", save.settings.screenShake ? 1 : 0);
    body += fmt("fps=%d\n", save.settings.showFps ? 1 : 0);
    body += fmt("scan=%d\n", save.settings.scanlines ? 1 : 0);
    body += fmt("quality=%d\n", save.settings.quality);
    body += fmt("autosave=%d\n", save.settings.autosave ? 1 : 0);
    body += fmt("fullscreen=%d\n", save.settings.fullscreen);
    std::string data = body + fmt("check=%08x\n", saveHash(body));

    std::string dir = platformSaveDir();
    std::string mainPath = dir + SAVE_FILE;
    if (backup && fileExists(mainPath)) {
        std::vector<uint8_t> old;
        if (readFile(mainPath, old)) writeFile(dir + SAVE_BAK, old.data(), old.size());
    }
    bool ok = writeFile(mainPath, data.data(), data.size());
    if (ok) saveFlashT = 1.6f;
    return ok;
}

bool Game::loadGame() {
    std::string dir = platformSaveDir();
    std::vector<uint8_t> buf;
    bool fromBak = false;
    if (!readFile(dir + SAVE_FILE, buf)) {
        if (!readFile(dir + SAVE_BAK, buf)) return false;
        fromBak = true;
    }
    std::string text((const char*)buf.data(), buf.size());
    SaveData d;
    std::string body;
    uint32_t storedHash = 0;
    bool haveHash = false;
    size_t pos = 0;
    int upg[UPG_COUNT] = {0}, res[RES_COUNT] = {0};
    float resf[RES_COUNT] = {0};
    int version = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "check") {
            storedHash = (uint32_t)strtoul(v.c_str(), nullptr, 16);
            haveHash = true;
            continue;
        }
        body += line + "\n";
        if (k == "version") version = atoi(v.c_str());
        else if (k == "credits") d.credits = atoll(v.c_str());
        else if (k == "earned") d.totalEarned = atoll(v.c_str());
        else if (k == "xp") d.xp = atoi(v.c_str());
        else if (k == "level") d.level = atoi(v.c_str());
        else if (k == "ship") d.ship = atoi(v.c_str());
        else if (k == "ships") d.unlockedShips = atoi(v.c_str());
        else if (k == "sectors") d.unlockedSectors = atoi(v.c_str());
        else if (k == "cur_sector") d.currentSector = atoi(v.c_str());
        else if (k == "drones") d.droneCount = atoi(v.c_str());
        else if (k == "drone_tier") d.droneTier = atoi(v.c_str());
        else if (k == "hull") d.hull = (float)atof(v.c_str());
        else if (k == "shield") d.shield = (float)atof(v.c_str());
        else if (k == "ach1") d.achMask = atoi(v.c_str());
        else if (k == "ach2") d.achMask2 = atoi(v.c_str());
        else if (k == "mined") d.asteroidsMined = atoll(v.c_str());
        else if (k == "kills") d.enemiesKilled = atoll(v.c_str());
        else if (k == "jumps") d.jumps = atoll(v.c_str());
        else if (k == "playtime") d.playTime = (float)atof(v.c_str());
        else if (k == "px") d.playerPos.x = (float)atof(v.c_str());
        else if (k == "py") d.playerPos.y = (float)atof(v.c_str());
        else if (k[0] == 'u' && k.rfind("upg", 0) == 0) { int i = atoi(k.c_str() + 3); if (i >= 0 && i < UPG_COUNT) upg[i] = atoi(v.c_str()); }
        else if (k[0] == 'r' && k.rfind("res", 0) == 0) { int i = atoi(k.c_str() + 3); if (i >= 0 && i < RES_COUNT) resf[i] = (float)atof(v.c_str()); }
        else if (k == "vol") d.settings.masterVolume = (float)atof(v.c_str());
        else if (k == "sfx") d.settings.sfxVolume = (float)atof(v.c_str());
        else if (k == "shake") d.settings.screenShake = atoi(v.c_str()) != 0;
        else if (k == "fps") d.settings.showFps = atoi(v.c_str()) != 0;
        else if (k == "scan") d.settings.scanlines = atoi(v.c_str()) != 0;
        else if (k == "quality") d.settings.quality = atoi(v.c_str());
        else if (k == "autosave") d.settings.autosave = atoi(v.c_str()) != 0;
        else if (k == "fullscreen") d.settings.fullscreen = atoi(v.c_str());
    }
    bool corrupt = false;
    if (!haveHash || saveHash(body) != storedHash) corrupt = true;
    if (version < 1 || version > 3) corrupt = true;
    if (d.level < 1 || d.level > 999) d.level = 1;
    if (d.ship < 0 || d.ship >= SHIP_COUNT) d.ship = 0;
    if (d.unlockedShips < 1 || d.unlockedShips > SHIP_COUNT) d.unlockedShips = 1;
    if (d.unlockedSectors < 1 || d.unlockedSectors > 5) d.unlockedSectors = 1;
    if (d.currentSector < 0 || d.currentSector >= 5) d.currentSector = 0;
    if (d.droneCount < 0 || d.droneCount > 12) d.droneCount = 0;
    if (corrupt) {
        // salvage what is sane instead of nuking progress entirely
        d.credits = std::max<long long>(0, std::min<long long>(d.credits, 1000000000LL));
        d.cargo[0] = 0;
        statusLine = "SAVE DATA RECOVERED (PARTIAL)";
        statusT = 5.f;
    }
    for (int i = 0; i < UPG_COUNT; i++) d.upgrades[i] = clampv(upg[i], 0, UPGRADES[i].maxLevel);
    for (int i = 0; i < RES_COUNT; i++) d.cargo[i] = std::max(0.f, std::min(resf[i], 99999.f));
    d.settings.masterVolume = clampf(d.settings.masterVolume, 0.f, 1.f);
    d.settings.sfxVolume = clampf(d.settings.sfxVolume, 0.f, 1.f);
    d.settings.quality = clampv(d.settings.quality, 0, 2);
    d.valid = true;
    save = d;
    sector = save.currentSector;
    playTime = save.playTime;
    achievements.assign(ACH_COUNT, AchieveState{});
    // rebuild achievement flags
    for (int i = 0; i < 16; i++) achievements[i].unlocked = (save.achMask >> i) & 1;
    for (int i = 0; i < ACH_COUNT - 16; i++) achievements[16 + i].unlocked = (save.achMask2 >> i) & 1;
    enterSector(sector, true);
    playerPos = save.playerPos;
    if (length(playerPos) > SECTORS[sector].worldRadius) playerPos = Vec2(0, 700);
    hullMax = statHull();
    shieldMax = statShield();
    hull = save.hull > 0 ? std::min(save.hull, hullMax) : hullMax;
    shield = save.shield >= 0 ? std::min(save.shield, shieldMax) : shieldMax;
    energy = energyMax = 100.f;
    cargoUsed = totalCargo();
    rebuildDrones();
    applySettings();
    if (corrupt && fromBak) statusLine = "RESTORED FROM BACKUP";
    return true;
}

void Game::applySettings() {
    audio::setMasterVolume(save.settings.masterVolume);
}

// ---------------------------------------------------------------- sectors
static float distToSegment(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 d = b - a;
    float l2 = lengthSq(d);
    float t = l2 > 1e-6f ? clampf(dot(p - a, d) / l2, 0.f, 1.f) : 0.f;
    return length(p - (a + d * t));
}

int Game::spawnAsteroid(Vec2 at, float radius, int sectorStyle, bool forceResource, int resId) {
    (void)sectorStyle;
    Asteroid a;
    a.p = at;
    a.radius = radius;
    const SectorDef& sd = SECTORS[sector];
    int listIdx = 0;
    if (forceResource) {
        a.res[0] = resId;
        a.res[1] = resId;
    } else {
        // weighted random resource
        int total = 0;
        for (int i = 0; i < 4; i++) if (sd.resList[i] >= 0) total += sd.resWeights[i];
        int roll = rng.irange(0, std::max(1, total));
        int acc = 0, chosen = 0;
        for (int i = 0; i < 4; i++) {
            if (sd.resList[i] < 0) continue;
            acc += sd.resWeights[i];
            if (roll < acc) { chosen = i; break; }
        }
        listIdx = chosen;
        a.res[0] = sd.resList[chosen];
        // occasionally a rare secondary
        a.res[1] = RES_IRON;
        if (rng.chance(.22f)) {
            int r2 = rng.irange(0, RES_COUNT);
            if (RESOURCES[r2].rarity >= 3) a.res[1] = r2;
        }
    }
    (void)listIdx;
    if (a.res[0] < 0 || a.res[0] >= RES_COUNT) a.res[0] = RES_IRON;
    if (a.res[1] < 0 || a.res[1] >= RES_COUNT) a.res[1] = RES_IRON;
    const ResourceDef& rd = RESOURCES[a.res[0]];
    // rock variants of the matching resource
    std::vector<int> cand;
    for (size_t i = 0; i < assets->rockRes[sector].size(); i++)
        if (assets->rockRes[sector][i] == a.res[0]) cand.push_back((int)i);
    if (cand.empty()) {
        for (size_t i = 0; i < assets->rockRes[sector].size(); i++) cand.push_back((int)i);
    }
    if (cand.empty()) return -1;
    a.sprite = cand[rng.irange(0, (int)cand.size())];
    float sizeFactor = radius / 100.f;
    // bigger rocks are denser but slower to break
    a.maxHp = a.hp = (34.f + radius * 1.15f) * (0.75f + rd.hardness * .5f);
    a.angle = rng.range(0, TAU);
    a.spin = rng.range(-.16f, .16f) / std::max(.6f, sizeFactor);
    float baseAmt = (6.f + radius * .16f) * (1.f + (5 - rd.rarity) * .06f);
    a.amt[0] = baseAmt;
    a.amt[1] = a.res[1] != a.res[0] ? baseAmt * rng.range(.12f, .35f) : 0.f;
    a.wobble = 0.f;
    asteroids.push_back(a);
    return (int)asteroids.size() - 1;
}

int Game::spawnEnemy(int type, Vec2 at) {
    Enemy e;
    e.type = clampv(type, 0, (int)EN_TYPE_COUNT - 1);
    const EnemyDef& d = ENEMIES[e.type];
    float scale = 1.f + (sector * .16f);
    e.p = at;
    e.hp = e.maxHp = d.hull * scale;
    e.shield = e.maxShield = d.shield * scale;
    e.angle = rng.range(0, TAU);
    e.fireCd = rng.range(.6f, 2.f);
    e.patrolA = at;
    e.patrolB = at + rng.insideCircle(700.f);
    e.reward = d.reward * (1.f + sector * .45f);
    e.xp = d.xp * (1.f + sector * .3f);
    e.id = (int)(enemies.size() + 1);
    e.aimJitter = rng.insideCircle(1.f);
    e.spawnT = 0.f;
    enemies.push_back(e);
    return (int)enemies.size() - 1;
}

void Game::generateSector(int idx) {
    asteroids.clear();
    enemies.clear();
    loot.clear();
    shots.clear();
    drones.clear();
    particles.clear();
    hazards.clear();
    const SectorDef& sd = SECTORS[idx];
    stationPos = Vec2(0, 0);
    playerPos = Vec2(0, 720);
    playerVel = Vec2(0, 0);
    cameraPos = playerPos;
    cargoUsed = totalCargo();
    // asteroids in a ring, kept away from the station and the player
    float R = sd.worldRadius;
    int placed = 0, guard = 0;
    while (placed < sd.asteroidCount && guard < sd.asteroidCount * 40) {
        guard++;
        float ang = rng.range(0, TAU);
        float rad = sqrtf(rng.f()) * R;
        if (rad < 420.f) rad = 420.f + rng.f() * 300.f;   // station exclusion zone
        Vec2 p(cosf(ang) * rad, sinf(ang) * rad);
        if (length(p - stationPos) < 380.f) continue;
        float r = rng.range(34.f, 96.f);
        if (rng.chance(.11f)) r = rng.range(110.f, 150.f);
        bool ok = true;
        for (auto& a : asteroids) {
            float minD = (a.radius + r) * 1.18f + 26.f;
            if (lengthSq(a.p - p) < minD * minD) { ok = false; break; }
        }
        if (!ok) continue;
        int i = spawnAsteroid(p, r, idx);
        if (i >= 0) {
            asteroids[i].angle = rng.range(0, TAU);
            placed++;
        }
    }
    // enemies
    int enemyCount = (int)(4 + sd.danger * 16);
    for (int i = 0; i < enemyCount; i++) {
        int list[4], n = 0;
        for (int k = 0; k < 4; k++) if (sd.enemyList[k] >= 0) list[n++] = sd.enemyList[k];
        if (n == 0) break;
        int type = list[rng.irange(0, n)];
        Vec2 at = rng.dir() * rng.range(1400.f, R * .95f);
        spawnEnemy(type, at);
    }
    // hazards
    if (sd.hazardLevel > .4f) {
        int hz = (int)(sd.hazardLevel * 10);
        for (int i = 0; i < hz; i++) {
            Hazard h;
            h.p = rng.dir() * rng.range(900.f, R * .9f);
            h.radius = rng.range(190.f, 330.f);
            h.phase = rng.range(0, TAU);
            h.intensity = sd.hazardLevel;
            h.kind = idx == 3 ? 0 : 1;
            hazards.push_back(h);
        }
    }
    rebuildDrones();
    backgroundTime = 0;
    uiScroll = uiScrollTarget = 0;
    screenFlash = .6f;
    save.currentSector = idx;
    save.jumps++;
}

void Game::enterSector(int idx, bool instant) {
    idx = clampv(idx, 0, 4);
    sector = idx;
    generateSector(idx);
    hullMax = statHull();
    shieldMax = statShield();
    energyMax = 100.f;
    if (instant) {
        hull = std::max(hull, hullMax * .35f);
        shield = shieldMax;
        energy = energyMax;
    } else {
        hull = clampf(hull, hullMax * .2f, hullMax);
        shield = shieldMax;
        energy = energyMax;
    }
    invuln = 2.f;
    warpT = 1.f;
    save.currentSector = idx;
    miningTarget = -1;
}

void Game::rebuildDrones() {
    int want = clampv(save.droneCount, 0, 8);
    while ((int)drones.size() > want) drones.pop_back();
    while ((int)drones.size() < want) {
        Drone d;
        d.p = playerPos + rng.insideCircle(90.f);
        d.state = DR_IDLE;
        d.tier = (float)save.droneTier;
        d.bob = rng.range(0, TAU);
        drones.push_back(d);
    }
    for (auto& d : drones) d.tier = (float)save.droneTier;
}

// ---------------------------------------------------------------- xp / levels
void Game::addXp(int amount) {
    if (amount <= 0) return;
    save.xp += amount;
    int need = (int)(120.f * powf((float)save.level, 1.55f));
    while (save.xp >= need && save.level < 60) {
        save.xp -= need;
        save.level++;
        hud.levelPop = fmt("LEVEL %d", save.level);
        hud.levelPopT = 3.2f;
        audio::playUpgrade();
        need = (int)(120.f * powf((float)save.level, 1.55f));
        // small reward per level
        save.credits += save.level * 250;
        toast(fmt("LEVEL UP  %d   +%d CR", save.level, save.level * 250), rgba(255, 236, 160), 3.f);
    }
    checkAchievements();
}

// ---------------------------------------------------------------- achievements
void Game::unlockAchievement(int idx) {
    if (idx < 0 || idx >= ACH_COUNT) return;
    if (achievements[idx].unlocked) return;
    achievements[idx].unlocked = true;
    achievements[idx].popupT = headless ? 0.f : 4.2f;
    if (idx < 16) save.achMask |= 1 << idx;
    else save.achMask2 |= 1 << (idx - 16);
    audio::playUpgrade();
    toast(std::string("ACHIEVEMENT: ") + ACHIEVEMENTS[idx].name, ACHIEVEMENTS[idx].color, 3.6f);
}

void Game::checkAchievements() {
    if (save.asteroidsMined >= 1) unlockAchievement(ACH_FIRST_MINE);
    if (save.asteroidsMined >= 100) unlockAchievement(ACH_ROCKBREAKER);
    if (save.enemiesKilled >= 1) unlockAchievement(ACH_FIRST_BLOOD);
    if (save.enemiesKilled >= 25) unlockAchievement(ACH_HUNTER);
    if (save.unlockedSectors >= 2) unlockAchievement(ACH_EXPLORER);
    if (save.unlockedSectors >= 5) unlockAchievement(ACH_FRONTIER);
    if (save.credits >= 100000) unlockAchievement(ACH_RICH);
    int upgTotal = 0, maxed = 0;
    for (int i = 0; i < UPG_COUNT; i++) {
        upgTotal += save.upgrades[i];
        if (save.upgrades[i] >= UPGRADES[i].maxLevel) maxed++;
    }
    if (upgTotal >= 10) unlockAchievement(ACH_UPGRADER);
    if (maxed > 0) unlockAchievement(ACH_MAXED);
    if (droneCount() >= 4) unlockAchievement(ACH_FLEET);
    if (save.cargo[RES_CRYSTAL] > 0) unlockAchievement(ACH_CRYSTAL);
    if (save.cargo[RES_PLASMA] > 0) unlockAchievement(ACH_PLASMA);
    if (save.cargo[RES_VOIDCRYSTAL] > 0) unlockAchievement(ACH_VOID);
    if (save.cargo[RES_QUANTUM] > 0) unlockAchievement(ACH_QUANTUM);
    if (save.unlockedShips >= 2) unlockAchievement(ACH_SHIPYARD);
    if (save.ship == SHIP_VOIDCRAFT) unlockAchievement(ACH_VOIDCRAFT);
    if (playTime >= 1800.f) unlockAchievement(ACH_MARATHON);
    if (save.upgrades[UPG_REFINERY] >= UPGRADES[UPG_REFINERY].maxLevel) unlockAchievement(ACH_REFINER);
}

// ---------------------------------------------------------------- economy
void Game::toast(const std::string& s, Color c, float t) { hud.toastMsg(s, c, t); }

void Game::addCargo(int res, float amt) {
    if (res < 0 || res >= RES_COUNT || amt <= 0) return;
    float room = statCargo() - cargoUsed;
    if (room <= 0.01f) {
        toast("CARGO HOLD FULL - DOCK TO SELL", rgba(255, 120, 90), 2.f);
        return;
    }
    float take = std::min(amt, room);
    save.cargo[res] += take;
    cargoUsed += take;
    checkAchievements();
}

void Game::sellAll() {
    if (!docked) {
        toast("DOCK AT HELIOS STATION TO TRADE", rgba(255, 180, 90), 2.4f);
        audio::playAlarm();
        return;
    }
    long long total = 0;
    for (int i = 0; i < RES_COUNT; i++) {
        if (save.cargo[i] <= 0) continue;
        long long v = (long long)(save.cargo[i] * RESOURCES[i].value * statRefine());
        total += v;
        save.cargo[i] = 0;
    }
    cargoUsed = 0;
    if (total > 0) {
        save.credits += total;
        save.totalEarned += total;
        addXp((int)(total * .06f));
        audio::playSell();
        toast(fmt("SOLD CARGO  +%s CR", money(total).c_str()), rgba(140, 255, 190), 3.f);
        checkAchievements();
        if (!headless) saveGame();
    } else {
        toast("HOLD IS EMPTY", rgba(160, 180, 200), 2.f);
    }
}

void Game::sellResource(int res) {
    if (res < 0 || res >= RES_COUNT || save.cargo[res] <= 0) return;
    if (!docked) {
        toast("DOCK AT HELIOS STATION TO TRADE", rgba(255, 180, 90), 2.4f);
        return;
    }
    long long v = (long long)(save.cargo[res] * RESOURCES[res].value * statRefine());
    cargoUsed = std::max(0.f, cargoUsed - save.cargo[res]);
    save.cargo[res] = 0;
    save.credits += v;
    save.totalEarned += v;
    addXp((int)(v * .06f));
    audio::playSell();
    toast(fmt("+%s CR", money(v).c_str()), rgba(140, 255, 190), 2.f);
    checkAchievements();
}

bool Game::buyUpgrade(int id) {
    if (id < 0 || id >= UPG_COUNT) return false;
    if (!docked) {
        toast("DOCK AT HELIOS STATION TO BUY UPGRADES", rgba(255, 180, 90), 2.4f);
        return false;
    }
    const UpgradeDef& u = UPGRADES[id];
    if (save.upgrades[id] >= u.maxLevel) return false;
    long long cost = (long long)(u.baseCost * powf(u.costScale, (float)save.upgrades[id]));
    if (save.credits < cost) {
        toast("NOT ENOUGH CREDITS", rgba(255, 120, 90), 2.f);
        audio::playAlarm();
        return false;
    }
    save.credits -= cost;
    save.upgrades[id]++;
    audio::playUpgrade();
    hullMax = statHull();
    shieldMax = statShield();
    hull = std::min(hull + (id == UPG_HULL ? hullMax * .18f : 0.f), hullMax);
    shield = std::min(shield, shieldMax);
    toast(fmt("%s UPGRADED TO LEVEL %d", u.name, save.upgrades[id]), rgba(150, 240, 255), 2.4f);
    checkAchievements();
    return true;
}

bool Game::buyShip(int id) {
    if (id < 0 || id >= SHIP_COUNT) return false;
    if (!docked) {
        toast("DOCK AT HELIOS STATION TO BUY SHIPS", rgba(255, 180, 90), 2.4f);
        return false;
    }
    if (id < save.unlockedShips) {
        if (save.ship != id) {
            save.ship = id;
            hullMax = statHull();
            shieldMax = statShield();
            hull = hullMax;
            shield = shieldMax;
            rebuildDrones();
            toast(std::string("NOW FLYING: ") + SHIPS[id].name, SHIPS[id].accent, 2.6f);
            audio::playUpgrade();
        }
        return true;
    }
    if (id != save.unlockedShips) return false;   // must buy in order
    if (save.credits < SHIPS[id].cost) {
        toast("NOT ENOUGH CREDITS", rgba(255, 120, 90), 2.f);
        audio::playAlarm();
        return false;
    }
    save.credits -= SHIPS[id].cost;
    save.unlockedShips = id + 1;
    save.ship = id;
    hullMax = statHull();
    shieldMax = statShield();
    hull = hullMax;
    shield = shieldMax;
    rebuildDrones();
    audio::playUpgrade();
    toast(std::string("COMMISSIONED: ") + SHIPS[id].name, SHIPS[id].accent, 3.2f);
    checkAchievements();
    return true;
}

bool Game::buyDrone() {
    if (!docked) {
        toast("DOCK AT HELIOS STATION TO BUY DRONES", rgba(255, 180, 90), 2.4f);
        return false;
    }
    int bayCap = std::min(8, 4 + save.upgrades[UPG_DRONES] * 2);
    if (droneCount() >= bayCap) {
        toast("NO FREE DRONE BAYS - UPGRADE DRONE BAYS", rgba(255, 180, 90), 2.4f);
        return false;
    }
    int idx = clampv(save.droneTier, 0, 2);
    long long cost = DRONES[idx].cost;
    if (save.credits < cost) {
        toast("NOT ENOUGH CREDITS", rgba(255, 120, 90), 2.f);
        audio::playAlarm();
        return false;
    }
    save.credits -= cost;
    save.droneCount++;
    rebuildDrones();
    audio::playDroneBeep();
    toast(fmt("DRONE ONLINE  (%d/%d)", droneCount(), 4 + save.upgrades[UPG_DRONES] * 2), rgba(140, 255, 210), 2.6f);
    checkAchievements();
    return true;
}

bool Game::unlockSector(int idx) {
    if (idx < 0 || idx > 4) return false;
    if (idx < save.unlockedSectors) return true;
    if (idx != save.unlockedSectors) return false;
    int cost = SECTORS[idx].unlockCost;
    if (save.credits < cost) {
        toast("NOT ENOUGH CREDITS TO JUMP", rgba(255, 120, 90), 2.f);
        audio::playAlarm();
        return false;
    }
    save.credits -= cost;
    save.unlockedSectors = idx + 1;
    audio::playWarp();
    toast(std::string("SECTOR UNLOCKED: ") + SECTORS[idx].name, SECTORS[idx].accent, 3.4f);
    checkAchievements();
    return true;
}

bool Game::selectSector(int idx) {
    if (idx < 0 || idx > 4) return false;
    if (idx >= save.unlockedSectors) {
        if (!unlockSector(idx)) return false;
    }
    if (idx == sector) return true;
    pendingSector = idx;
    sectorTransition = 0.f;
    sectorTransitionDir = 1;
    audio::playWarp();
    return true;
}

void Game::dock() {
    if (length(playerPos - stationPos) > 460.f) {
        toast("TOO FAR FROM STATION", rgba(255, 170, 90), 2.f);
        return;
    }
    docked = true;
    screen = 3;
    uiTab = 0;
    playerVel = Vec2(0, 0);
    audio::playUiClick();
    if (!headless) saveGame(true);
    checkAchievements();
}

void Game::undock() {
    docked = false;
    screen = 2;
    playerPos = stationPos + Vec2(0, 460.f);
    playerVel = Vec2(0, 60);
    invuln = 1.6f;
    audio::playUiClick();
}

void Game::refundDeath() {
    // lose 25% of credits, all cargo
    long long lost = (long long)(save.credits * .25);
    save.credits = std::max<long long>(0, save.credits - lost);
    for (int i = 0; i < RES_COUNT; i++) save.cargo[i] = 0;
    cargoUsed = 0;
    respawn();
}

void Game::respawn() {
    hullMax = statHull();
    shieldMax = statShield();
    hull = hullMax;
    shield = shieldMax;
    energy = energyMax;
    playerPos = stationPos + Vec2(0, 520.f);
    playerVel = Vec2(0, 0);
    cameraPos = playerPos;
    invuln = 3.f;
    screen = 2;
    deathCause.clear();
    if (!headless) saveGame(true);
}

}  // namespace sm
