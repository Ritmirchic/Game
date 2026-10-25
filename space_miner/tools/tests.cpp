// ============================================================
// SPACE MINER  -  tools/tests.cpp
// Headless harness: functional self-tests, screenshot capture,
// and a synthetic art contact sheet. Runs the real game code.
// ============================================================
#include "game.h"
#include "content.h"
#include "art.h"
#include "platform.h"
#include <cstdio>
#include <chrono>
#include <string>

using namespace sm;

static int gFail = 0, gPass = 0;
static std::string gShots = "./shots/";

static void check(bool cond, const std::string& what, const std::string& detail = "") {
    if (cond) {
        gPass++;
        printf("  [PASS] %s %s\n", what.c_str(), detail.c_str());
    } else {
        gFail++;
        printf("  [FAIL] %s %s\n", what.c_str(), detail.c_str());
    }
}

struct Harness {
    Game g;
    Canvas fb;
    Input in;
    Harness() : fb(VW, VH) {
        g.init(true);
        in.mouseX = VW * .5f;
        in.mouseY = VH * .5f;
    }
    void step(float dt = 1.f / 60.f) {
        in.beginFrame();
        g.update(dt, in);
        g.render(fb);
    }
    void steps(int n, float dt = 1.f / 60.f) {
        for (int i = 0; i < n; i++) step(dt);
    }
    void clearInput() {
        std::memset(in.down, 0, sizeof(in.down));
        for (int i = 0; i < 3; i++) in.mouseDown[i] = false;
        in.mouseX = VW * .5f;
        in.mouseY = VH * .5f;
    }
    // emulate a full click on a screen position (render-time UI)
    void click(int x, int y) {
        in.mouseX = (float)x;
        in.mouseY = (float)y;
        in.beginFrame();
        in.mouseDown[0] = true;
        in.mousePressed[0] = true;
        g.update(1.f / 60.f, in);
        g.render(fb);
        in.beginFrame();
        in.mouseDown[0] = false;
        in.mouseReleased[0] = true;
        g.update(1.f / 60.f, in);
        g.render(fb);
        in.mouseReleased[0] = false;
    }
    void shot(const std::string& name) {
        writePNG((gShots + name + ".png").c_str(), fb.px.data(), fb.w, fb.h);
    }
};

// ---------------------------------------------------------------- tests
static void testMovement(Harness& h) {
    printf("\nMOVEMENT\n");
    h.g.screen = 2;
    h.clearInput();
    h.g.playerPos = Vec2(0, 600);
    h.g.playerVel = Vec2(0, 0);
    h.steps(5);
    Vec2 start = h.g.playerPos;
    h.in.down[keyOf('W')] = true;
    h.steps(60);
    Vec2 after = h.g.playerPos;
    check(after.y < start.y - 50, "thrust moves ship up", fmt("(dy=%.1f)", after.y - start.y));
    check(length(h.g.playerVel) <= h.g.statSpeed() + 1.f, "speed clamped to stat",
          fmt("(v=%.1f max=%.1f)", length(h.g.playerVel), h.g.statSpeed()));
    h.in.down[keyOf('W')] = false;
    h.steps(60);
    check(length(h.g.playerVel) < 30.f, "ship coasts to a stop", fmt("(v=%.1f)", length(h.g.playerVel)));
    // camera follows
    float dist = length(h.g.cameraPos - h.g.playerPos);
    check(dist < 260.f, "camera follows ship", fmt("(d=%.1f)", dist));
}

static void testMining(Harness& h) {
    printf("\nMINING\n");
    h.g.screen = 2;
    h.clearInput();
    h.g.enterSector(0, true);
    h.g.playerPos = Vec2(0, 600);
    h.g.playerVel = Vec2(0, 0);
    h.g.cameraPos = h.g.playerPos;
    h.g.energy = 100.f;
    h.g.energyLocked = false;
    h.g.asteroids.clear();
    int idx = h.g.spawnAsteroid(h.g.playerPos + Vec2(240, 0), 60.f, 0, true, RES_IRON);
    check(idx >= 0, "asteroid spawned", fmt("(n=%d)", (int)h.g.asteroids.size()));
    float hp0 = h.g.asteroids[idx].hp;
    float credit0 = h.g.save.credits;
    h.clearInput();
    h.in.mouseX = VW / 2 + 300;
    h.in.mouseY = VH / 2;
    h.in.mouseDown[0] = true;
    h.steps(30);
    check(h.g.asteroids[idx].hp < hp0, "mining beam damages asteroid",
          fmt("(hp %.0f -> %.0f)", hp0, h.g.asteroids[idx].hp));
    check(h.g.miningTarget == idx, "beam locks the target in front");
    check(h.g.energy < 100.f, "mining consumes energy", fmt("(e=%.0f)", h.g.energy));
    int guard = 0;
    while (h.g.asteroids[idx].alive && guard++ < 900) h.step();
    check(!h.g.asteroids[idx].alive, "asteroid is destroyed by mining", fmt("(frames=%d)", guard));
    check(!h.g.loot.empty(), "destruction spawns collectible loot", fmt("(loot=%d)", (int)h.g.loot.size()));
    float cargo0 = h.g.cargoUsed;
    h.in.mouseDown[0] = false;
    h.steps(180);
    check(h.g.cargoUsed > cargo0, "loot is tractored into the hold", fmt("(cargo=%.0f)", h.g.cargoUsed));
    check(h.g.save.asteroidsMined >= 1, "mining stat recorded");
    check(h.g.save.xp >= 0 && credit0 >= 0, "economy state valid");
}

static void testCargoCap(Harness& h) {
    printf("\nCARGO\n");
    h.g.screen = 2;
    h.clearInput();
    for (int i = 0; i < RES_COUNT; i++) h.g.save.cargo[i] = 0;
    h.g.cargoUsed = 0;
    float cap = h.g.statCargo();
    h.g.addCargo(RES_IRON, cap * 2.f);
    check(fabsf(h.g.cargoUsed - cap) < .5f, "cargo capped at hold size", fmt("(%.0f/%.0f)", h.g.cargoUsed, cap));
    h.g.addCargo(RES_GOLD, 10.f);
    check(h.g.save.cargo[RES_GOLD] < .01f, "full hold rejects new cargo");
    check(h.g.hud.toastT > 0.f, "player is warned about a full hold");
}

static void testEconomy(Harness& h) {
    printf("\nECONOMY\n");
    h.g.screen = 2;
    h.clearInput();
    h.g.save.credits = 20000;
    h.g.save.cargo[RES_IRON] = 40;
    h.g.cargoUsed = 40;
    h.g.hull = h.g.hullMax;
    float cargoBefore = h.g.cargoUsed;
    long long cr0 = h.g.save.credits;
    h.g.playerPos = h.g.stationPos;
    h.g.dock();
    check(h.g.screen == 3, "docking opens the station");
    h.g.sellAll();
    check(h.g.save.credits > cr0, "selling cargo earns credits", fmt("(+%lld)", h.g.save.credits - cr0));
    check(h.g.cargoUsed == 0.f, "hold is emptied", fmt("(before %.0f)", cargoBefore));
    check(h.g.save.cargo[RES_IRON] == 0.f, "resources cleared after sale");

    float cargoStat0 = h.g.statCargo();
    int lvl0 = h.g.upgradeLevel(UPG_CARGO);
    bool bought = h.g.buyUpgrade(UPG_CARGO);
    check(bought, "upgrade purchase succeeds");
    check(h.g.upgradeLevel(UPG_CARGO) == lvl0 + 1, "upgrade level increased");
    check(h.g.statCargo() > cargoStat0, "cargo upgrade raises capacity",
          fmt("(%.0f -> %.0f)", cargoStat0, h.g.statCargo()));
    float hullStat0 = h.g.statHull();
    h.g.buyUpgrade(UPG_HULL);
    check(h.g.statHull() > hullStat0, "hull upgrade raises max hull");
    float dmg0 = h.g.statDamage();
    h.g.buyUpgrade(UPG_WEAPON);
    check(h.g.statDamage() > dmg0, "weapon upgrade raises damage");
    // credits are actually deducted
    long long cr1 = h.g.save.credits;
    h.g.buyUpgrade(UPG_LASER);
    check(h.g.save.credits < cr1, "upgrade costs credits", fmt("(-%lld)", cr1 - h.g.save.credits));
    // cannot buy without credits
    h.g.save.credits = 0;
    check(!h.g.buyUpgrade(UPG_SHIELD), "purchase blocked when broke");
}

static void testDrones(Harness& h) {
    printf("\nDRONES\n");
    h.g.screen = 2;
    h.clearInput();
    h.g.enterSector(0, true);
    h.g.playerPos = Vec2(0, 500);
    h.g.asteroids.clear();
    h.g.loot.clear();
    h.g.save.credits = 100000;
    h.g.save.droneTier = 1;
    h.g.docked = true;   // trading needs a docked ship
    h.g.buyDrone();
    check(h.g.droneCount() == 1, "drone purchased and online", fmt("(n=%d)", h.g.droneCount()));
    h.g.docked = false;
    h.g.spawnAsteroid(Vec2(260, 500), 55.f, 0, true, RES_IRON);
    int guard = 0;
    bool mined = false;
    while (guard++ < 2000) {
        h.step();
        if (h.g.drones[0].state == DR_MINE) mined = true;
        if (mined && h.g.cargoUsed > 1.f) break;
    }
    check(mined, "drone flies to a rock and mines", fmt("(frames=%d)", guard));
    check(h.g.cargoUsed > .5f, "drone delivers ore to the ship", fmt("(cargo=%.0f)", h.g.cargoUsed));
}

static void testCombat(Harness& h) {
    printf("\nCOMBAT\n");
    h.g.screen = 2;
    h.clearInput();
    h.g.enterSector(0, true);
    h.g.playerPos = Vec2(0, 400);
    h.g.cameraPos = h.g.playerPos;
    h.g.playerVel = Vec2(0, 0);
    h.g.energy = 100.f;
    h.g.energyLocked = false;
    h.g.enemies.clear();
    h.g.asteroids.clear();
    int e = h.g.spawnEnemy(EN_SCOUT, Vec2(260, 400));
    float ehp = h.g.enemies[e].hp;
    h.in.mouseX = VW / 2 + 300;
    h.in.mouseY = VH / 2;
    h.in.mouseDown[1] = true;
    h.steps(45);
    check(h.g.enemies[e].hp < ehp || !h.g.enemies[e].alive, "cannons damage enemies",
          fmt("(hp %.0f -> %.0f)", ehp, h.g.enemies[e].hp));
    int guard = 0;
    while (h.g.enemies[e].alive && guard++ < 900) h.step();
    check(!h.g.enemies[e].alive, "enemy destroyed by gunfire", fmt("(frames=%d)", guard));
    h.in.mouseDown[1] = false;
    check(h.g.save.enemiesKilled >= 1, "kill counter incremented");
    // player damage
    h.g.invuln = 0;
    h.g.shield = h.g.shieldMax;
    float before = h.g.hull + h.g.shield;
    h.g.damagePlayer(20.f, "TEST");
    check(h.g.hull + h.g.shield < before, "damage reduces hull/shield");
    // death flow
    h.g.invuln = 0;
    h.g.hull = 1.f;
    h.g.shield = 0;
    h.g.damagePlayer(50.f, "TEST");
    check(h.g.screen == 7, "lethal damage triggers the death screen");
    h.g.refundDeath();
    check(h.g.screen == 2 && h.g.hull > 0, "respawn restores the ship");
}

static void testSectors(Harness& h) {
    printf("\nSECTORS\n");
    h.clearInput();
    h.g.screen = 2;
    h.g.save.credits = 500000;
    while (h.g.save.unlockedSectors < 5) {
        int target = h.g.save.unlockedSectors;
        bool ok = h.g.unlockSector(target);
        check(ok, fmt("unlock sector %d", target + 1));
        if (!ok) break;
    }
    check(h.g.save.unlockedSectors == 5, "all five sectors unlocked");
    for (auto& a : h.g.achievements) a.popupT = 0.f;
    h.g.hud.toastT = 0.f;
    for (int s = 0; s < 5; s++) {
        h.g.enterSector(s, true);
        // park hostiles far away so the shot shows the environment
        for (auto& e : h.g.enemies) { e.p = h.g.playerPos + Vec2(2000, -1600); e.spawnT = 0.f; }
        for (auto& a : h.g.achievements) a.popupT = 0.f;
        h.g.hud.toastT = 0.f;
        h.g.tutorialT = 0.f;
        check((int)h.g.asteroids.size() > 100, fmt("sector %d populated with asteroids", s + 1),
              fmt("(n=%d)", (int)h.g.asteroids.size()));
        bool allOutsidePlayer = true;
        for (auto& a : h.g.asteroids)
            if (length(a.p - h.g.playerPos) < a.radius + 30.f) allOutsidePlayer = false;
        check(allOutsidePlayer, fmt("sector %d asteroids do not overlap the ship", s + 1));
        bool insideBounds = true;
        for (auto& a : h.g.asteroids)
            if (length(a.p) > SECTORS[s].worldRadius + 1.f) insideBounds = false;
        check(insideBounds, fmt("sector %d rocks inside the sector radius", s + 1));
        h.steps(3);
    }
    // sector area / free space
    float totalArea = 0;
    for (auto& a : h.g.asteroids) totalArea += PI * a.radius * a.radius;
    float sectorArea = PI * SECTORS[h.g.sector].worldRadius * SECTORS[h.g.sector].worldRadius;
    check(totalArea < sectorArea * .2f, "asteroid field leaves room to fly",
          fmt("(%.1f%%)", 100.f * totalArea / sectorArea));
}

static void testSaveLoad(Harness& h) {
    printf("\nSAVE / LOAD\n");
    h.g.save.credits = 12345;
    h.g.save.level = 7;
    h.g.save.cargo[RES_GOLD] = 42;
    h.g.save.upgrades[UPG_LASER] = 3;
    h.g.save.unlockedSectors = 3;
    h.g.headless = false;   // allow saving to disk
    bool saved = h.g.saveGame(true);
    check(saved, "save file written");
    check(h.g.saveExists(), "save file present on disk");
    long long cr = h.g.save.credits;
    h.g.save.credits = 0;
    h.g.save.upgrades[UPG_LASER] = 0;
    bool loaded = h.g.loadGame();
    check(loaded, "save file loaded");
    check(h.g.save.credits == cr, "credits restored", fmt("(%lld)", h.g.save.credits));
    check(h.g.save.upgrades[UPG_LASER] == 3, "upgrade levels restored");
    check(fabsf(h.g.save.cargo[RES_GOLD] - 42.f) < .01f, "cargo restored");
    check(h.g.save.unlockedSectors == 3, "sector unlocks restored");
    // corrupt the file and make sure the loader does not crash / produces sane data
    {
        std::string path = std::string(platformSaveDir()) + "space_miner_save.dat";
        std::string junk = "this is not a save file\nversion=99\ncredits=-99999\nlevel=999999\n";
        writeFile(path, junk.data(), junk.size());
        bool loaded2 = h.g.loadGame();
        check(loaded2, "corrupted save is recovered instead of crashing");
        check(h.g.save.credits >= 0, "corrupted credits clamped");
        check(h.g.save.level >= 1 && h.g.save.level <= 999, "corrupted level clamped");
        check(h.g.save.ship >= 0 && h.g.save.ship < SHIP_COUNT, "corrupted ship index clamped");
    }
    h.g.headless = true;
    h.g.newGame();
}

static void testProgressionRules(Harness& h) {
    printf("\nPROGRESSION\n");
    h.clearInput();
    h.g.newGame();
    h.g.screen = 2;
    // achievements fire on their conditions
    h.g.save.asteroidsMined = 1;
    h.g.checkAchievements();
    check(h.g.achievements[ACH_FIRST_MINE].unlocked, "first mine achievement unlocks");
    h.g.save.enemiesKilled = 30;
    h.g.checkAchievements();
    check(h.g.achievements[ACH_HUNTER].unlocked, "hunter achievement unlocks");
    // trading is blocked while flying
    h.g.docked = false;
    h.g.save.cargo[RES_IRON] = 20;
    h.g.cargoUsed = 20;
    long long cr0 = h.g.save.credits;
    h.g.sellAll();
    check(h.g.save.credits == cr0, "cannot sell while undocked");
    check(h.g.cargoUsed == 20.f, "cargo kept while undocked");
    h.g.docked = true;
    h.g.sellAll();
    check(h.g.save.credits > cr0, "selling works once docked");
    // upgrades blocked while undocked
    h.g.docked = false;
    h.g.save.credits = 999999;
    int lvl = h.g.upgradeLevel(UPG_SHIELD);
    check(!h.g.buyUpgrade(UPG_SHIELD), "upgrades blocked while undocked");
    check(h.g.upgradeLevel(UPG_SHIELD) == lvl, "upgrade level unchanged while undocked");
    h.g.docked = true;
    check(h.g.buyUpgrade(UPG_SHIELD), "upgrade allowed while docked");
    // drone bay cap follows the upgrade
    h.g.save.droneCount = 0;
    h.g.rebuildDrones();
    h.g.save.droneTier = 0;
    for (int i = 0; i < 12; i++) h.g.buyDrone();
    int cap = 4 + h.g.upgradeLevel(UPG_DRONES) * 2;
    check(h.g.droneCount() <= 8, "drone count never exceeds the hard cap", fmt("(n=%d)", h.g.droneCount()));
    check(h.g.droneCount() == std::min(cap, 8), "drones fill the available bays",
          fmt("(n=%d cap=%d)", h.g.droneCount(), cap));
    // level curve awards credits and raises stats
    h.g.save.level = 1;
    h.g.save.xp = 0;
    float mp0 = h.g.statMiningPower();
    h.g.addXp(100000);
    check(h.g.save.level > 1, "xp grants levels", fmt("(level=%d)", h.g.save.level));
    check(h.g.statMiningPower() > mp0, "levels improve mining power");
    h.g.docked = false;
    h.g.newGame();
}

static void testUI(Harness& h) {
    printf("\nUI\n");
    h.clearInput();
    h.g.screen = 1;
    h.steps(3);
    check(h.g.screen == 1, "main menu renders");
    h.shot("menu");
    // settings round trip
    h.g.pendingScreen = 5;
    h.g.settingsFrom = 1;
    h.steps(40);
    check(h.g.screen == 5, "settings screen reached");
    h.shot("settings");
    h.g.pendingScreen = 1;
    h.steps(40);
    // station tabs render
    h.g.screen = 3;
    for (int t = 0; t < 5; t++) {
        h.g.uiTab = t;
        h.steps(2);
        check(true, fmt("station tab %d renders", t));
    }
    h.g.uiTab = 3;
    h.steps(2);
    h.shot("station_drones");
    h.g.uiTab = 0;
    h.steps(2);
    h.shot("station_cargo");
    h.g.uiTab = 1;
    h.steps(2);
    h.shot("station_upgrades");
    h.g.uiTab = 2;
    h.steps(2);
    h.shot("station_shipyard");
    h.g.uiTab = 4;
    h.steps(2);
    h.shot("station_achievements");
    h.g.screen = 4;
    h.steps(2);
    h.shot("sector_chart");
    h.g.screen = 6;
    h.steps(2);
    h.shot("achievements");
    h.g.screen = 7;
    h.g.deathT = 1.2f;
    h.steps(2);
    h.shot("death");
    h.g.screen = 8;
    h.steps(2);
    h.shot("victory");
    h.g.screen = 2;
    h.steps(2);
    check(true, "all screens render without crashing");
}

static void testPerformance(Harness& h) {
    printf("\nPERFORMANCE\n");
    h.clearInput();
    h.g.enterSector(4, true);
    h.g.screen = 2;
    for (int i = 0; i < 10; i++) h.g.spawnEnemy(EN_HEAVY, h.g.playerPos + Vec2(300 + i * 40, 100));
    h.steps(20);   // warm up
    auto t0 = std::chrono::steady_clock::now();
    const int N = 120;
    for (int i = 0; i < N; i++) {
        h.in.mouseX = VW / 2 + 200;
        h.in.mouseY = VH / 2;
        h.step();
    }
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / N;
    printf("  measured %.2f ms/frame (update + full software render, %d entities)\n", ms,
           (int)(h.g.asteroids.size() + h.g.enemies.size() + h.g.particles.size()));
    // sanitizer builds are 3-10x slower; the budget check only means something on -O2
    bool sanitized = false;
#if defined(__SANITIZE_ADDRESS__)
    sanitized = true;
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
    sanitized = true;
#  endif
#endif
    double budget = sanitized ? 200.0 : 33.0;
    if (sanitized) printf("  (sanitizer build: budget relaxed to %.0f ms)\n", budget);
    check(ms < budget, "frame budget met (>=30 fps in software)", fmt("(%.2f ms)", ms));
}

// ---------------------------------------------------------------- screenshots
static void captureScenarios(Harness& h) {
    printf("\nCAPTURING SCREENSHOTS -> %s\n", gShots.c_str());
    h.g.save.unlockedSectors = 5;
    h.g.save.unlockedShips = 4;
    h.g.save.droneCount = 4;
    h.g.save.droneTier = 1;
    h.g.save.upgrades[UPG_DRONES] = 2;
    h.g.rebuildDrones();
    for (auto& a : h.g.achievements) a.popupT = 0.f;
    h.g.hud.toastT = 0.f;
    for (int s = 0; s < 5; s++) {
        h.g.enterSector(s, true);
        // park hostiles far away so the shot shows the environment
        for (auto& e : h.g.enemies) { e.p = h.g.playerPos + Vec2(2000, -1600); e.spawnT = 0.f; }
        for (auto& a : h.g.achievements) a.popupT = 0.f;
        h.g.hud.toastT = 0.f;
        h.g.tutorialT = 0.f;
        h.g.playerPos = h.g.stationPos + Vec2(260, 620);
        h.g.cameraPos = h.g.playerPos;
        h.g.save.ship = s == 4 ? SHIP_VOIDCRAFT : (s == 2 ? SHIP_LANCER : (s == 1 ? SHIP_HARVESTER : SHIP_PROSPECTOR));
        h.g.hullMax = h.g.statHull();
        h.g.hull = h.g.hullMax * .82f;
        h.g.shieldMax = h.g.statShield();
        h.g.shield = h.g.shieldMax * .6f;
        h.g.energy = 64.f;
        h.g.screen = 2;
        h.clearInput();
        h.in.mouseX = VW / 2 + 260;
        h.in.mouseY = VH / 2 - 90;
        h.steps(30);
        if (s == 1) {
            // show a mining beam on the closest rock
            int best = -1;
            float bd = 1e9f;
            for (size_t i = 0; i < h.g.asteroids.size(); i++) {
                float d = length(h.g.asteroids[i].p - h.g.playerPos);
                if (d < bd) { bd = d; best = (int)i; }
            }
            if (best >= 0) {
                Vec2 d = normalize(h.g.asteroids[best].p - h.g.playerPos);
                h.g.playerPos = h.g.asteroids[best].p - d * 220.f;
                h.g.playerAngle = angleOf(d);
                h.in.mouseX = VW / 2 + d.x * 300;
                h.in.mouseY = VH / 2 + d.y * 300;
                h.in.mouseDown[0] = true;
                h.steps(40);
                h.in.mouseDown[0] = false;
            }
        }
        // combat shot in sector 3
        if (s == 2) {
            for (int i = 0; i < 4; i++) {
                int e = h.g.spawnEnemy(EN_RAIDER, h.g.playerPos + Vec2(300 + i * 90, -140 + i * 70));
                h.g.enemies[e].spawnT = .5f;
            }
            h.in.mouseDown[1] = true;
            h.in.mouseX = VW / 2 + 300;
            h.in.mouseY = VH / 2 - 60;
            h.steps(26);
            h.in.mouseDown[1] = false;
            h.steps(2);
        }
        h.shot(fmt("sector%d_%s", s + 1, s == 0 ? "starter_belt" : s == 1 ? "crystal_nebula"
                                             : s == 2 ? "iron_graveyard" : s == 3 ? "plasma_storm" : "void_frontier"));
    }
    // explosion shot
    h.g.enterSector(3, true);
    h.g.screen = 2;
    h.g.playerPos = Vec2(0, 700);
    h.g.cameraPos = h.g.playerPos;
    h.steps(4);
    for (int i = 0; i < 3; i++)
        h.g.explode(h.g.playerPos + Vec2(-180 + i * 180, -220), 1.8f, rgba(255, 170, 90));
    for (int i = 0; i < 8; i++)
        h.g.spawnParticles(h.g.playerPos + Vec2(-180 + i * 50, -220), 12, rgba(255, 210, 140), 320.f, 1, 3.f);
    h.steps(6);
    h.shot("vfx_explosions");
    printf("  wrote scenario screenshots\n");
}

static void captureArtSheet() {
    AssetBank bank;
    bank.init(0);
    Canvas c(1600, 1180);
    c.clear(rgba(7, 10, 18));
    c.vGradient(0, 0, 1600, 1180, rgba(10, 14, 24), rgba(5, 7, 12));
    drawText(c, 16, 10, "SPACE MINER - PROCEDURAL ASSET SHEET (ALL ART GENERATED AT RUNTIME)", rgba(150, 220, 240), 1, 1);
    // planet + nebula strip
    for (int i = 0; i < 5; i++)
        c.blitScaled(bank.nebula[i][0], 16 + i * 220, 26, 210, 120, .55f, true);
    drawText(c, 16, 150, "NEBULA / SECTOR BACKDROPS 1-5", rgba(190, 225, 240), 1, 1);
    for (int i = 0; i < 5; i++)
        c.blitScaled(bank.planets[i], 20 + i * 120, 160, 110, 110, 1.f, false);
    drawText(c, 20, 274, "SECTOR PLANETS", rgba(190, 225, 240), 1, 1);
    drawText(c, 660, 274, "SHIP CLASSES: PROSPECTOR / HARVESTER / LANCER / VOIDCRAFT", rgba(190, 225, 240), 1, 1);
    // ships on a clean row instead of stacked
    c.blitScaled(bank.ships[0].body, 660, 20, 190, 190, 1.f, false);
    c.blitScaled(bank.ships[1].body, 850, 20, 190, 190, 1.f, false);
    c.blitScaled(bank.ships[2].body, 1040, 20, 190, 190, 1.f, false);
    c.blitScaled(bank.ships[3].body, 1230, 20, 190, 190, 1.f, false);
    // enemies
    for (int i = 0; i < EN_TYPE_COUNT; i++)
        c.blitScaled(bank.enemies[i], 20 + i * 150, 296, 140, 140, 1.f, false);
    c.blitScaled(bank.station, 790, 282, 210, 210, 1.f, false);
    drawText(c, 20, 438, "HOSTILES: SCOUT / RAIDER / HEAVY / PLASMA HUNTER / VOID SENTINEL   +  HELIOS STATION",
             rgba(190, 225, 240), 1, 1);
    // drones
    for (int i = 0; i < 3; i++) c.blitScaled(bank.drones[i], 20 + i * 110, 456, 100, 100, 1.f, false);
    drawText(c, 20, 558, "MINING DRONES T1-T3", rgba(190, 225, 240), 1, 1);
    // debris
    for (int i = 0; i < 5; i++) c.blitScaled(bank.debris[i], 380 + i * 90, 456, 84, 84, 1.f, false);
    drawText(c, 380, 544, "SALVAGE DEBRIS", rgba(190, 225, 240), 1, 1);
    // asteroid variants: 5 sectors x 6 variants
    for (int s = 0; s < 5; s++)
        for (size_t i = 0; i < bank.rocks[s].size() && i < 6; i++)
            c.blitScaled(bank.rocks[s][i], 1010 + (int)i * 96, 456 + s * 74, 92, 92, 1.f, false);
    drawText(c, 1010, 444, "ASTEROID VARIANTS  5 SECTORS x 6", rgba(190, 225, 240), 1, 1);
    // icons
    drawText(c, 24, 846, "15 RESOURCES (ICON / SYMBOL)", rgba(190, 225, 240), 1, 1);
    for (int i = 0; i < RES_COUNT; i++) {
        c.blitScaled(bank.resIcon[i], 24 + i * 46, 858, 40, 40, 1.f, false);
        drawText(c, 24 + i * 46, 902, RESOURCES[i].code, withAlpha(RESOURCES[i].color, 220), 1, 0);
    }
    drawText(c, 24, 920, "12 UPGRADE BRANCH ICONS", rgba(190, 225, 240), 1, 1);
    for (int i = 0; i < UPG_COUNT; i++) c.blitScaled(bank.upgIcon[i], 24 + i * 62, 932, 56, 56, 1.f, false);
    drawText(c, 24, 1000, "VFX SPRITES: SOFT GLOW / SHARP GLOW / SMOKE / SPARK STREAK", rgba(190, 225, 240), 1, 1);
    c.blitScaled(bank.glowSoft, 24, 1014, 76, 76, 1.f, true);
    c.blitScaled(bank.glowSharp, 108, 1014, 76, 76, 1.f, true);
    c.blitScaled(bank.smoke, 192, 1014, 76, 76, .9f, true);
    c.blitScaled(bank.spark, 276, 1014, 76, 76, 1.f, true);
    drawText(c, 800, 846, "20 ACHIEVEMENT BADGES", rgba(190, 225, 240), 1, 1);
    for (int i = 0; i < 10; i++) c.blitScaled(bank.achIcon[i], 800 + i * 62, 858, 56, 56, 1.f, false);
    for (int i = 10; i < ACH_COUNT; i++) c.blitScaled(bank.achIcon[i], 800 + (i - 10) * 62, 926, 56, 56, 1.f, false);
    writePNG((gShots + "art_sheet.png").c_str(), c.px.data(), c.w, c.h);
    printf("  wrote %sart_sheet.png\n", gShots.c_str());
}

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "--selftest";
    if (argc > 2) gShots = argv[2];
    if (gShots.size() && gShots.back() != '/') gShots += "/";
    platformInit("SPACE MINER TEST", VW, VH);
    printf("=====================================================\n");
    printf(" SPACE MINER - automated harness  (mode: %s)\n", mode.c_str());
    printf("=====================================================\n");
    auto t0 = std::chrono::steady_clock::now();
    Harness h;
    auto t1 = std::chrono::steady_clock::now();
    printf("asset + world init: %.0f ms\n", std::chrono::duration<double, std::milli>(t1 - t0).count());

    if (mode == "--shots") {
        captureScenarios(h);
        captureArtSheet();
    } else if (mode == "--selftest") {
        testMovement(h);
        testMining(h);
        testCargoCap(h);
        testEconomy(h);
        testDrones(h);
        testCombat(h);
        testSectors(h);
        testProgressionRules(h);
        testSaveLoad(h);
        testUI(h);
        testPerformance(h);
    }
    printf("\n-----------------------------------------------------\n");
    printf(" RESULT: %d passed, %d failed\n", gPass, gFail);
    printf("-----------------------------------------------------\n");
    return gFail == 0 ? 0 : 1;
}
