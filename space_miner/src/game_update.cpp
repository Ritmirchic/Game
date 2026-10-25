// ============================================================
// SPACE MINER  -  game_update.cpp
// Simulation: flight, mining, combat, drones, enemies, loot, VFX.
// ============================================================
#include "game.h"
#include "content.h"
#include "art.h"
#include "platform.h"

namespace sm {

static float distPointSeg(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 d = b - a;
    float l2 = lengthSq(d);
    float t = l2 > 1e-6f ? clampf(dot(p - a, d) / l2, 0.f, 1.f) : 0.f;
    return length(p - (a + d * t));
}

// ---------------------------------------------------------------- particles
void Game::spawnParticles(Vec2 at, int count, Color c, float speed, int kind, float size) {
    int budget = save.settings.quality == 0 ? count / 2 : (save.settings.quality == 2 ? count * 2 : count);
    budget = std::min(budget, 60);
    for (int i = 0; i < budget; i++) {
        if (particles.size() > 2600) break;
        Particle p;
        p.p = at + rng.insideCircle(6.f);
        float a = rng.range(0, TAU);
        float s = speed * rng.range(.35f, 1.f);
        p.v = Vec2(cosf(a), sinf(a)) * s;
        p.maxLife = p.life = rng.range(.3f, .95f);
        p.size = size * rng.range(.6f, 1.3f);
        p.color = c;
        p.kind = kind;
        p.drag = .92f;
        p.rot = rng.range(0, TAU);
        p.spin = rng.range(-6, 6);
        particles.push_back(p);
    }
}

void Game::explode(Vec2 at, float scale, Color tint) {
    // flash core
    Particle core;
    core.p = at;
    core.maxLife = core.life = .32f;
    core.size = 60.f * scale;
    core.color = mixColor(tint, rgba(255, 255, 255), .6f);
    core.kind = 0;
    core.drag = 1.f;
    particles.push_back(core);
    // shock ring
    Particle ring;
    ring.p = at;
    ring.maxLife = ring.life = .55f;
    ring.size = 30.f * scale;
    ring.color = tint;
    ring.kind = 4;
    ring.drag = 1.f;
    particles.push_back(ring);
    // sparks
    spawnParticles(at, (int)(22 * scale), rgba(255, 210, 150), 420.f * scale, 1, 3.f);
    spawnParticles(at, (int)(14 * scale), tint, 260.f * scale, 0, 9.f);
    spawnParticles(at, (int)(8 * scale), rgba(90, 84, 96), 90.f * scale, 2, 22.f);
    spawnParticles(at, (int)(10 * scale), rgba(120, 116, 110), 200.f * scale, 3, 6.f);
    if (save.settings.screenShake) {
        float mag = std::min(18.f, 6.f * scale) * (length(at - playerPos) < 900.f ? 1.f : .35f);
        cameraShake += Vec2(rng.range(-mag, mag), rng.range(-mag, mag));
    }
    flashWhite = std::max(flashWhite, std::min(.35f, .08f * scale));
}

void Game::updateParticles(float dt) {
    for (size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0) {
            particles[i] = particles.back();
            particles.pop_back();
            continue;
        }
        p.p += p.v * dt;
        float d = powf(p.drag, dt * 60.f);
        p.v *= d;
        p.rot += p.spin * dt;
        if (p.kind == 3) p.v *= powf(.985f, dt * 60.f);
        i++;
    }
}

// ---------------------------------------------------------------- loot
void Game::spawnLoot(Vec2 at, int res, float amt) {
    if (amt <= 0.01f) return;
    // split into chunks so it looks like a stream of ore
    int chunks = clampv((int)(amt / 3.f), 1, 6);
    float per = amt / chunks;
    for (int i = 0; i < chunks; i++) {
        Loot l;
        l.p = at + rng.insideCircle(18.f);
        l.v = rng.dir() * rng.range(20.f, 90.f) + normalize(playerPos - at) * 60.f;
        l.res = res;
        l.amount = per;
        l.maxLife = l.life = 30.f;
        l.spin = rng.range(0, TAU);
        l.z = 0;
        l.vz = rng.range(20.f, 70.f);
        loot.push_back(l);
    }
}

void Game::collectLoot(int idx) {
    if (idx < 0 || idx >= (int)loot.size()) return;
    Loot& l = loot[idx];
    addCargo(l.res, l.amount);
    audio::playPickup();
    for (int i = 0; i < 5; i++) {
        Particle p;
        p.p = l.p;
        p.v = rng.dir() * rng.range(30, 90);
        p.maxLife = p.life = rng.range(.15f, .35f);
        p.size = 3.f;
        p.color = RESOURCES[l.res].color;
        p.kind = 0;
        particles.push_back(p);
    }
    loot[idx] = loot.back();
    loot.pop_back();
}

// ---------------------------------------------------------------- mining / damage
void Game::damageAsteroid(int idx, float dmg, bool fromDrone) {
    if (idx < 0 || idx >= (int)asteroids.size()) return;
    Asteroid& a = asteroids[idx];
    if (!a.alive) return;
    a.hp -= dmg;
    a.lastHitT = time;
    a.wobbleV += dmg * .008f * (rng.chance(.5f) ? 1.f : -1.f);
    if (!fromDrone && rng.chance(.35f))
        spawnParticles(a.p + rng.dir() * a.radius * .8f, 2, RESOURCES[a.res[0]].color, 120.f, 1, 2.5f);
    if (a.hp <= 0) {
        a.alive = false;
        save.asteroidsMined++;
        // resource yield splits between the two minerals
        spawnLoot(a.p, a.res[0], a.amt[0] * .85f);
        if (a.amt[1] > .5f) spawnLoot(a.p, a.res[1], a.amt[1]);
        explode(a.p, a.radius / 70.f, mixColor(RESOURCES[a.res[0]].color, rgba(255, 230, 200), .25f));
        spawnParticles(a.p, 16, rgba(140, 130, 120), 260.f, 3, 8.f);
        addXp((int)(6 + a.radius * .18f + RESOURCES[a.res[0]].rarity * 9));
        audio::playExplosion(a.radius / 100.f);
        checkAchievements();
    }
}

void Game::damageEnemy(int idx, float dmg) {
    if (idx < 0 || idx >= (int)enemies.size()) return;
    Enemy& e = enemies[idx];
    if (!e.alive) return;
    e.hitFlash = 1.f;
    if (e.shield > 0) {
        float absorb = std::min(e.shield, dmg);
        e.shield -= absorb;
        dmg -= absorb;
        spawnParticles(e.p, 6, rgba(120, 200, 255), 150.f, 0, 6.f);
    }
    e.hp -= dmg;
    spawnParticles(e.p, 4, rgba(255, 200, 160), 180.f, 1, 2.f);
    if (e.hp <= 0) killEnemy(idx);
}

void Game::killEnemy(int idx) {
    if (idx < 0 || idx >= (int)enemies.size()) return;
    Enemy& e = enemies[idx];
    if (!e.alive) return;
    e.alive = false;
    explode(e.p, 1.15f + e.maxHp / 400.f, ENEMIES[e.type].tint);
    save.enemiesKilled++;
    save.credits += (long long)e.reward;
    save.totalEarned += (long long)e.reward;
    addXp((int)e.xp);
    // salvage drop: pick a resource from the sector table
    const SectorDef& sd = SECTORS[sector];
    int pool[4], n = 0;
    for (int k = 0; k < 4; k++) if (sd.resList[k] >= 0) pool[n++] = sd.resList[k];
    if (n > 0) {
        int res = pool[rng.irange(0, n)];
        spawnLoot(e.p, res, 2.f + (float)e.type * 1.5f);
    }
    if (e.type == EN_SENTINEL) unlockAchievement(ACH_SENTINEL_FALLS);
    hud.toastMsg(fmt("+%lld CR  HOSTILE DESTROYED", (long long)e.reward), rgba(255, 210, 140), 2.f);
    checkAchievements();
    audio::playExplosion(1.f);
}

void Game::damagePlayer(float dmg, const char* cause) {
    if (invuln > 0) return;
    float absorbed = 0;
    if (shield > 0) {
        absorbed = std::min(shield, dmg);
        shield -= absorbed;
        dmg -= absorbed;
        spawnParticles(playerPos + rng.dir() * 40.f, 5, rgba(120, 200, 255), 140.f, 0, 7.f);
    }
    if (dmg > 0) {
        hull -= dmg;
        hud.hitT = 1.f;
        flashWhite = std::max(flashWhite, .2f);
        if (save.settings.screenShake)
            cameraShake += Vec2(rng.range(-9, 9), rng.range(-9, 9));
        spawnParticles(playerPos, 8, rgba(255, 140, 90), 200.f, 1, 3.f);
    }
    invuln = .28f;
    if (hull <= 0) {
        hull = 0;
        deathCause = cause ? cause : "HULL BREACH";
        screen = 7;
        deathT = 0;
        explode(playerPos, 2.2f, rgba(255, 160, 90));
        audio::playExplosion(2.f);
    } else if (hull / hullMax < .1f) {
        unlockAchievement(ACH_SURVIVOR);
    }
}

// ---------------------------------------------------------------- drones
static void droneSeek(Game& g, Drone& d, float dt, int selfIdx) {
    Asteroid* target = nullptr;
    float bestD = 1e9f;
    int bestI = -1;
    for (size_t i = 0; i < g.asteroids.size(); i++) {
        Asteroid& a = g.asteroids[i];
        if (!a.alive) continue;
        float dd = lengthSq(a.p - d.p);
        if (dd > 1500.f * 1500.f) continue;
        // avoid dogpiling: two drones max per rock
        int others = 0;
        for (size_t k = 0; k < g.drones.size(); k++) {
            if ((int)k == selfIdx) continue;
            if (g.drones[k].target == (int)i && g.drones[k].state != DR_RETURN) others++;
        }
        if (others >= 2) continue;
        if (dd < bestD) { bestD = dd; bestI = (int)i; target = &a; }
    }
    d.target = bestI;
    if (!target) {
        // orbit the player
        Vec2 want = g.playerPos + Vec2(cosf(g.time * .7f + d.bob), sinf(g.time * .7f + d.bob)) * 90.f;
        d.v += normalize(want - d.p) * 260.f * dt;
        return;
    }
    Vec2 want = target->p + normalize(d.p - target->p) * (target->radius + 66.f);
    Vec2 to = want - d.p;
    float dist = length(to);
    float spd = DRONES[(int)clampf(d.tier, 0, 2)].speed;
    d.v += normalize(to) * spd * 3.2f * dt;
    if (dist < 60.f) {
        d.state = DR_MINE;
        d.mineProgress = 0;
    }
    d.angle = lerpAngle(d.angle, angleOf(target->p - d.p), .16f);
}

void Game::updateDrones(float dt) {
    for (size_t i = 0; i < drones.size(); i++) {
        Drone& d = drones[i];
        const DroneDef& dd = DRONES[clampv((int)d.tier, 0, 2)];
        d.bob += dt * 2.f;
        switch (d.state) {
            case DR_IDLE:
                if ((int)asteroids.size() > 0 || true) d.state = DR_SEEK;
                break;
            case DR_SEEK: {
                droneSeek(*this, d, dt, (int)i);
                if (d.target < 0) break;
                Asteroid& a = asteroids[d.target];
                if (!a.alive) { d.state = DR_SEEK; d.target = -1; break; }
                float dist = length(a.p - d.p) - a.radius;
                if (dist < 70.f) { d.state = DR_MINE; d.mineProgress = 0.f; }
                break;
            }
            case DR_MINE: {
                if (d.target < 0 || d.target >= (int)asteroids.size()) { d.state = DR_SEEK; break; }
                Asteroid& a = asteroids[d.target];
                if (!a.alive) { d.state = d.cargoAmt > .5f ? DR_RETURN : DR_SEEK; d.target = -1; break; }
                d.angle = lerpAngle(d.angle, angleOf(a.p - d.p), .2f);
                // hold position
                Vec2 want = a.p + normalize(d.p - a.p) * (a.radius + 62.f);
                d.v += (want - d.p) * 5.f * dt;
                d.beamT = 1.f;
                float rate = dd.miningPower * (1.f + d.tier * .15f) * 16.f;
                damageAsteroid(d.target, rate * dt, true);
                d.cargoAmt += rate * dt * .55f;
                d.cargoType = a.res[0];
                if (rng.chance(dt * 14.f)) {
                    Vec2 hit = a.p + normalize(d.p - a.p) * a.radius;
                    spawnParticles(hit, 1, RESOURCES[a.res[0]].color, 80.f, 1, 2.f);
                }
                if (d.cargoAmt >= dd.capacity) d.state = DR_RETURN;
                break;
            }
            case DR_RETURN: {
                Vec2 to = playerPos - d.p;
                float dist = length(to);
                float spd = dd.speed * 1.6f;
                d.v += normalize(to) * spd * 3.4f * dt;
                d.angle = lerpAngle(d.angle, angleOf(to), .18f);
                if (dist < 90.f) {
                    if (d.cargoAmt > .3f && d.cargoType >= 0) {
                        addCargo(d.cargoType, d.cargoAmt);
                        audio::playPickup();
                        for (int k = 0; k < 6; k++) {
                            Particle p;
                            p.p = d.p;
                            p.v = normalize(d.p - playerPos) * -1.f * rng.range(60, 160) + rng.dir() * 40.f;
                            p.maxLife = p.life = .3f;
                            p.size = 3.f;
                            p.color = RESOURCES[d.cargoType].color;
                            p.kind = 0;
                            particles.push_back(p);
                        }
                    }
                    d.cargoAmt = 0;
                    d.cargoType = -1;
                    d.state = DR_SEEK;
                    d.target = -1;
                }
                break;
            }
        }
        if (d.state != DR_MINE) d.beamT = std::max(0.f, d.beamT - dt * 4.f);
        // integrate with drag + separation from other drones
        d.v *= powf(.90f, dt * 60.f);
        float maxSpd = DRONES[clampv((int)d.tier, 0, 2)].speed * 1.7f;
        d.v = clampLen(d.v, maxSpd);
        d.p += d.v * dt;
        for (size_t k = 0; k < drones.size(); k++) {
            if (k == i) continue;
            Vec2 diff = d.p - drones[k].p;
            float dd2 = lengthSq(diff);
            if (dd2 < 60.f * 60.f && dd2 > .01f) d.p += normalize(diff) * (60.f - sqrtf(dd2)) * .35f;
        }
        // keep inside the sector
        float R = SECTORS[sector].worldRadius;
        if (length(d.p) > R) d.p = normalize(d.p) * R;
        // hit flash decay
        d.hitFlash = std::max(0.f, d.hitFlash - dt * 3.f);
    }
}

// ---------------------------------------------------------------- enemies
void Game::updateEnemies(float dt) {
    for (size_t i = 0; i < enemies.size(); i++) {
        Enemy& e = enemies[i];
        if (!e.alive) continue;
        const EnemyDef& def = ENEMIES[e.type];
        float distP = length(playerPos - e.p);
        float aggro = 1150.f + (sector > 2 ? 250.f : 0.f);
        e.spawnT += dt;
        e.hitFlash = std::max(0.f, e.hitFlash - dt * 4.f);

        // desired standoff distance by role
        float standoff = 320.f;
        switch (e.type) {
            case EN_SCOUT: standoff = 210.f; break;
            case EN_RAIDER: standoff = 300.f; break;
            case EN_HEAVY: standoff = 380.f; break;
            case EN_PLASMA: standoff = 280.f; break;
            default: standoff = 420.f; break;
        }
        Vec2 desired(0, 0);
        float speedScale = 1.f;
        if (distP < aggro && e.spawnT > .4f) {
            e.state = 1;
            Vec2 toP = playerPos - e.p;
            Vec2 dir = normalize(toP);
            // strafing
            float t = time * (e.type == EN_PLASMA ? 1.5f : .8f) + e.id * 1.7f;
            Vec2 strafe = perp(dir) * sinf(t);
            float err = distP - standoff;
            Vec2 radial = dir * clampf(err * .012f, -1.f, 1.f);
            desired = normalize(radial + strafe * .9f + e.aimJitter * .25f);
            // avoid asteroids
            for (auto& a : asteroids) {
                if (!a.alive) continue;
                Vec2 d2 = e.p - a.p;
                float dd = length(d2) - a.radius;
                if (dd < 140.f && dd > 0.f) desired += normalize(d2) * (1.f - dd / 140.f) * 1.6f;
            }
            desired = normalize(desired);
            speedScale = clampf(.4f + (1.f - standoff / std::max(standoff + 200.f, distP + 200.f)) * 1.4f, .35f, 1.25f);
            e.angle = lerpAngle(e.angle, angleOf(toP), 1.f - powf(.02f, dt));
            // shooting
            e.fireCd -= dt;
            if (e.fireCd <= 0 && distP < def.range) {
                e.fireCd = def.fireRate * rng.range(.8f, 1.25f);
                Vec2 muzzle = e.p + fromAngle(e.angle) * def.size * .7f;
                float spread = (e.type == EN_SENTINEL ? .07f : .12f);
                int barrels = e.type == EN_HEAVY ? 2 : 1;
                for (int b = 0; b < barrels; b++) {
                    Shot s;
                    s.p = muzzle;
                    float a = e.angle + rng.range(-spread, spread) + (barrels > 1 ? (b - .5f) * .06f : 0.f);
                    s.v = fromAngle(a) * (e.type == EN_SENTINEL ? 620.f : 520.f);
                    s.fromPlayer = false;
                    s.damage = def.damage;
                    s.life = 2.4f;
                    s.radius = e.type == EN_SENTINEL ? 7.f : 4.5f;
                    s.color = def.tint;
                    s.angle = a;
                    shots.push_back(s);
                }
                audio::playEnemyShot();
            }
        } else {
            // patrol
            e.state = 0;
            e.wander += dt * .6f;
            Vec2 toB = e.patrolB - e.p;
            if (length(toB) < 120.f) std::swap(e.patrolA, e.patrolB);
            desired = normalize((e.patrolB - e.p)) * .8f;
            e.angle = lerpAngle(e.angle, angleOf(e.patrolB - e.p), .04f);
            speedScale = .5f;
        }
        // integrate
        float spd = def.speed * speedScale * (1.f + sector * .05f);
        e.v = lerp(e.v, desired * spd, 1.f - powf(.05f, dt));
        e.p += e.v * dt;
        if (distP < def.size + 76.f && invuln <= 0) {
            damagePlayer(def.damage * .55f, def.name);
            e.v = -normalize(playerPos - e.p) * 160.f;
        }
        // separation between enemies
        for (size_t k = 0; k < enemies.size(); k++) {
            if (k == i || !enemies[k].alive) continue;
            Vec2 diff = e.p - enemies[k].p;
            float dd2 = lengthSq(diff);
            if (dd2 < 90.f * 90.f && dd2 > .01f) e.p += normalize(diff) * (90.f - sqrtf(dd2)) * .4f;
        }
        float R = SECTORS[sector].worldRadius * 1.05f;
        if (length(e.p) > R) e.p = normalize(e.p) * R;
        if (e.angle > PI) e.angle -= TAU;
        if (e.angle < -PI) e.angle += TAU;
    }
    // cull long-dead enemies
    if (enemies.size() > 60) {
        enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                                     [](const Enemy& e) { return !e.alive; }),
                      enemies.end());
    }
}

// ---------------------------------------------------------------- ambient spawns
void Game::spawnAmbient(float dt) {
    const SectorDef& sd = SECTORS[sector];
    static float enemyTimer = 0.f;
    enemyTimer -= dt;
    // keep population proportional to danger
    int alive = 0;
    for (auto& e : enemies) if (e.alive) alive++;
    int cap = 4 + (int)(sd.danger * 16);
    if (enemyTimer <= 0 && alive < cap) {
        enemyTimer = 6.f - sd.danger * 3.f;
        int list[4], n = 0;
        for (int k = 0; k < 4; k++) if (sd.enemyList[k] >= 0) list[n++] = sd.enemyList[k];
        if (n > 0) {
            Vec2 at;
            int tries = 0;
            do {
                at = playerPos + rng.dir() * rng.range(1500.f, 2300.f);
                tries++;
            } while (length(at) > sd.worldRadius && tries < 12);
            spawnEnemy(list[rng.irange(0, n)], at);
        }
    }
    // respawn asteroids so long sessions keep flowing
    int rockAlive = 0;
    for (auto& a : asteroids) if (a.alive) rockAlive++;
    if (rockAlive < sd.asteroidCount * 6 / 10) {
        static float rockTimer = 0;
        rockTimer -= dt;
        if (rockTimer <= 0) {
            rockTimer = 1.2f;
            for (int tries = 0; tries < 20; tries++) {
                Vec2 at = playerPos + rng.dir() * rng.range(1300.f, sd.worldRadius);
                if (length(at) > sd.worldRadius) continue;
                if (length(at - stationPos) < 460.f) continue;
                bool ok = true;
                for (auto& a : asteroids) {
                    if (!a.alive) continue;
                    float minD = (a.radius + 80.f) * 1.12f;
                    if (lengthSq(a.p - at) < minD * minD) { ok = false; break; }
                }
                for (auto& e : enemies)
                    if (e.alive && length(e.p - at) < 150.f) { ok = false; break; }
                if (length(at - playerPos) < 420.f) ok = false;   // never spawn on the player
                if (!ok) continue;
                spawnAsteroid(at, rng.chance(.12f) ? rng.range(100.f, 140.f) : rng.range(32.f, 92.f), sector);
                break;
            }
        }
    }
    // dust motes
    if (rng.chance(dt * 8.f) && particles.size() < 1200) {
        Particle p;
        p.p = playerPos + rng.insideCircle(1500.f);
        p.v = rng.dir() * rng.range(6.f, 28.f);
        p.maxLife = p.life = rng.range(1.5f, 4.f);
        p.size = rng.range(1.f, 2.4f);
        p.color = sd.pal.starTint;
        p.kind = 2;
        p.drag = 1.f;
        particles.push_back(p);
    }
}

// ---------------------------------------------------------------- main update
void Game::update(float dt, Input& in) {
    curInput = &in;
    // clamp huge dt (window drags etc)
    dt = clampf(dt, 0.f, .05f);
    lastDt = dt > 0.f ? dt : 1.f / 60.f;
    time += dt;
    uiTime += dt;
    hoverX = (int)in.mouseX;
    hoverY = (int)in.mouseY;
    flashWhite = std::max(0.f, flashWhite - dt * 2.6f);
    cameraShake *= powf(.02f, dt);
    saveFlashT = std::max(0.f, saveFlashT - dt);
    for (auto& a : achievements) a.popupT = std::max(0.f, a.popupT - dt);
    hud.toastT = std::max(0.f, hud.toastT - dt);
    hud.levelPopT = std::max(0.f, hud.levelPopT - dt);
    hud.hitT = std::max(0.f, hud.hitT - dt);
    statusT = std::max(0.f, statusT - dt);

    // fade transition between screens
    if (pendingScreen >= 0 && fadeDir == 0 && fadeT <= 0.f) fadeDir = 1;
    if (fadeDir != 0) {
        fadeT += fadeDir * dt * 3.4f;
        if (fadeT >= 1.f) {
            fadeT = 1.f;
            fadeDir = -1;
            if (pendingScreen >= 0) {
                screen = pendingScreen;
                pendingScreen = -1;
            }
        } else if (fadeT <= 0.f) {
            fadeT = 0.f;
            fadeDir = 0;
        }
    }

    if (screen == 2) {
        playTime += dt;
        tutorialT -= dt;
        if (tutorialT < 0 && tutorialStep < 6) {
            tutorialStep++;
            tutorialT = tutorialStep < 6 ? 14.f : 0.f;
            if (tutorialStep >= 6) tutorialT = 0;
        }
        if (save.settings.autosave) {
            autosaveT += dt;
            if (autosaveT > 90.f) {
                autosaveT = 0;
                if (!headless) saveGame();
            }
        }
        updatePlay(dt, in);
    } else if (screen == 3) {
        playTime += dt;
        stationAngle += dt * .12f;
        stationPulse += dt;
        // keep world simmering behind the station UI
        updateParticles(dt);
        save.settings.masterVolume = clampf(save.settings.masterVolume + (in.isDown(keyOf('[')) ? -.5f * dt : 0.f) + (in.isDown(keyOf(']')) ? .5f * dt : 0.f), 0, 1);
    } else {
        updateParticles(dt);
        menuAnim += dt;
        if (screen == 7) {
            deathT += dt;
        }
    }
    // audio mood
    audio::setAmbient(clampf(SECTORS[clampv(sector, 0, 4)].danger, 0.f, 1.f));
    audio::update(dt);
}

void Game::updatePlay(float dt, Input& in) {
    const SectorDef& sd = SECTORS[sector];
    zoom = lerpf(zoom, .86f, 1.f - powf(.02f, dt));

    // ---- warp-in / sector transition
    if (sectorTransition > 0.f || pendingSector >= 0) {
        sectorTransition += dt * sectorTransitionDir * 2.f;
        if (sectorTransition >= 1.f && pendingSector >= 0) {
            int target = pendingSector;
            pendingSector = -1;
            sectorTransition = 1.f;
            save.hull = hull;
            enterSector(target, false);
            sectorTransitionDir = -1;
        }
        if (sectorTransition <= 0.f) { sectorTransition = 0.f; sectorTransitionDir = 1; }
    }

    // ---- input: aim
    Vec2 aimWorld = cameraPos + (Vec2(in.mouseX, in.mouseY) - Vec2(VW * .5f, VH * .5f)) / zoom;
    bool keyboardAim = in.isDown(K_LEFT) || in.isDown(K_RIGHT) || in.isDown(K_UP) || in.isDown(K_DOWN);
    if (keyboardAim) {
        Vec2 dir(0, 0);
        if (in.isDown(K_LEFT)) dir.x -= 1;
        if (in.isDown(K_RIGHT)) dir.x += 1;
        if (in.isDown(K_UP)) dir.y -= 1;
        if (in.isDown(K_DOWN)) dir.y += 1;
        if (lengthSq(dir) > 0) playerAngle = lerpAngle(playerAngle, angleOf(dir), 1.f - powf(.001f, dt));
    } else {
        float target = angleOf(aimWorld - playerPos);
        playerAngle = lerpAngle(playerAngle, target, 1.f - powf(.0001f, dt));
    }

    // ---- input: thrust
    Vec2 move(0, 0);
    if (in.isDown(keyOf('A'))) move.x -= 1;
    if (in.isDown(keyOf('D'))) move.x += 1;
    if (in.isDown(keyOf('W'))) move.y -= 1;
    if (in.isDown(keyOf('S'))) move.y += 1;
    bool boost = in.isDown(K_SHIFT);
    float accel = statAccel() * (boost ? 1.55f : 1.f);
    if (lengthSq(move) > 0) {
        move = normalize(move);
        playerVel += move * accel * dt;
        float spd = statSpeed() * (boost ? 1.45f : 1.f);
        playerVel = clampLen(playerVel, spd);
    } else {
        playerVel *= powf(.02f, dt);
    }
    playerPos += playerVel * dt;
    // soft boundary
    float R = sd.worldRadius;
    float pr = length(playerPos);
    if (pr > R) {
        playerPos = normalize(playerPos) * R;
        playerVel *= .5f;
        hud.warning = "SECTOR BOUNDARY";
        hud.warnT = 1.2f;
    }
    throttle = lerpf(throttle, length(playerVel) / std::max(1.f, statSpeed()), 1.f - powf(.02f, dt));

    // ---- energy / shield regen
    float regen = statEnergyRegen() * 2.4f;
    energy = std::min(energyMax, energy + regen * dt);
    if (energy <= 1.5f) energyLocked = true;
    if (energy >= 38.f) energyLocked = false;
    if (shield < shieldMax && invuln <= 0) shield = std::min(shieldMax, shield + shieldMax * .075f * dt);
    invuln = std::max(0.f, invuln - dt);
    weaponCd = std::max(0.f, weaponCd - dt);

    // ---- collision with asteroids (push out, gentle damage on high speed)
    float shipR = 26.f;
    for (auto& a : asteroids) {
        if (!a.alive) continue;
        Vec2 d = playerPos - a.p;
        float dist = length(d);
        float minD = a.radius * .82f + shipR;
        if (dist < minD && dist > .01f) {
            Vec2 n = d / dist;
            playerPos = a.p + n * minD;
            float impact = length(playerVel);
            playerVel *= .35f;
            if (impact > 260.f) damagePlayer(impact * .035f, "ASTEROID IMPACT");
        }
    }

    // ---- mining beam
    bool wantMine = in.mouseDown[0] || in.isDown(K_SPACE);
    miningTarget = -1;
    if (wantMine && !energyLocked && energy > 2.f && !docked) {
        energy = std::max(0.f, energy - 15.f * dt);
        float range = statScan() * .85f + 180.f;
        int best = -1;
        float bestScore = 1e9f;
        for (size_t i = 0; i < asteroids.size(); i++) {
            Asteroid& a = asteroids[i];
            if (!a.alive) continue;
            Vec2 d = a.p - playerPos;
            float dist = length(d);
            if (dist > range + a.radius) continue;
            float ang = fabsf(wrapAngle(angleOf(d) - playerAngle));
            if (ang > .30f) continue;
            float score = ang * 700.f + dist * .35f;
            if (score < bestScore) { bestScore = score; best = (int)i; }
        }
        miningTarget = best;
        miningBeamT = 1.f;
        if (best >= 0) {
            Asteroid& a = asteroids[best];
            float dps = statMiningPower() * 26.f * statMiningSpeed();
            damageAsteroid(best, dps * dt, false);
            if (rng.chance(dt * 22.f)) {
                Vec2 hit = a.p - normalize(a.p - playerPos) * a.radius * .9f;
                spawnParticles(hit, 2, RESOURCES[a.res[0]].color, 110.f, 1, 2.2f);
                audio::playMining();
            }
        }
    } else {
        miningBeamT = std::max(0.f, miningBeamT - dt * 6.f);
    }

    // ---- weapons
    int barrels = SHIPS[save.ship].hardpoints;
    if ((in.mouseDown[1] || in.isDown(keyOf('F'))) && weaponCd <= 0 && energy > 8.f && !energyLocked) {
        weaponCd = .26f;
        energy -= 8.f;
        const ShipArt& art = assets->ships[save.ship];
        for (int b = 0; b < barrels && b < (int)art.hardpoints.size(); b++) {
            Vec2 hpLocal = art.hardpoints[b];
            Vec2 hpWorld = playerPos + rot(hpLocal, playerAngle);
            Shot s;
            s.p = hpWorld;
            s.angle = playerAngle + (barrels > 1 ? (b % 2 == 0 ? -.018f : .018f) : 0.f);
            s.v = fromAngle(s.angle) * 1150.f;
            s.fromPlayer = true;
            s.damage = statDamage();
            s.life = 1.4f;
            s.kind = 1;
            s.color = mixColor(rgba(120, 240, 255), SHIPS[save.ship].accent, .5f);
            s.radius = 3.4f;
            shots.push_back(s);
        }
        audio::playLaser();
        spawnParticles(playerPos + fromAngle(playerAngle) * 40.f, 3, rgba(180, 250, 255), 90.f, 0, 4.f);
    }

    // ---- shots
    for (size_t i = 0; i < shots.size();) {
        Shot& s = shots[i];
        s.p += s.v * dt;
        s.life -= dt;
        bool dead = s.life <= 0;
        if (!dead) {
            if (s.fromPlayer) {
                for (size_t k = 0; k < enemies.size(); k++) {
                    Enemy& e = enemies[k];
                    if (!e.alive) continue;
                    if (length(e.p - s.p) < ENEMIES[e.type].size + 14.f) {
                        damageEnemy((int)k, s.damage);
                        dead = true;
                        break;
                    }
                }
                if (!dead) {
                    for (size_t k = 0; k < asteroids.size(); k++) {
                        Asteroid& a = asteroids[k];
                        if (!a.alive) continue;
                        if (length(a.p - s.p) < a.radius * .8f) {
                            damageAsteroid((int)k, s.damage * .5f, true);
                            spawnParticles(s.p, 3, rgba(255, 220, 180), 120.f, 1, 2.f);
                            dead = true;
                            break;
                        }
                    }
                }
            } else {
                if (length(playerPos - s.p) < 30.f) {
                    damagePlayer(s.damage, "HOSTILE FIRE");
                    dead = true;
                }
            }
        }
        if (dead) {
            spawnParticles(s.p, 2, s.color, 90.f, 0, 3.f);
            shots[i] = shots.back();
            shots.pop_back();
            continue;
        }
        i++;
    }

    // ---- enemies / drones / hazards
    updateEnemies(dt);
    updateDrones(dt);
    for (auto& h : hazards) {
        h.phase += dt * (h.kind == 0 ? 1.6f : .7f);
        float pulse = h.kind == 0 ? powf(std::max(0.f, sinf(h.phase)), 8.f) : (sinf(h.phase) * .5f + .5f);
        if (pulse > .55f && length(playerPos - h.p) < h.radius) {
            if (rng.chance(dt * 2.2f)) damagePlayer(6.f * h.intensity * (h.kind == 0 ? 2.2f : 1.f), "PLASMA STORM");
        }
    }

    // ---- asteroids spin / decay
    for (auto& a : asteroids) {
        a.angle += a.spin * dt;
        a.wobbleV -= a.wobble * 26.f * dt;
        a.wobbleV *= powf(.90f, dt * 60.f);
        a.wobble += a.wobbleV * dt;
        a.flash = std::max(0.f, a.flash - dt * 3.f);
        if (a.hp > 0) a.crack = saturate(1.f - a.hp / a.maxHp);
    }
    if (asteroids.size() > 420) {
        asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(),
                                       [](const Asteroid& a) { return !a.alive; }),
                        asteroids.end());
    }

    // ---- loot
    float magnet = std::max(statMagnet(), 360.f);
    for (size_t i = 0; i < loot.size();) {
        Loot& l = loot[i];
        l.life -= dt;
        if (l.life <= 0) {
            loot[i] = loot.back();
            loot.pop_back();
            continue;
        }
        float d = length(playerPos - l.p);
        if (d < magnet) {
            Vec2 pull = normalize(playerPos - l.p) * (420.f * (1.f - d / magnet) + 90.f);
            l.v = lerp(l.v, pull, 1.f - powf(.02f, dt));
            l.pulled = true;
        }
        if (d < 46.f) {
            collectLoot((int)i);
            continue;
        }
        l.p += l.v * dt;
        l.v *= powf(.96f, dt * 60.f);
        l.spin += dt * 2.f;
        l.z = std::max(0.f, l.z + l.vz * dt);
        l.vz -= 220.f * dt;
        if (l.z <= 0) { l.vz = 0; l.z = 0; }
        i++;
    }

    updateParticles(dt);
    spawnAmbient(dt);
    checkAchievements();

    // ---- camera
    Vec2 camTarget = playerPos + playerVel * .22f;
    cameraPos = lerp(cameraPos, camTarget, 1.f - powf(.0015f, dt));
    cameraShake *= powf(.001f, dt);

    // ---- station docking
    float distStation = length(playerPos - stationPos);
    hud.dockHintT = distStation < 460.f ? 1.f : 0.f;
    if (in.wasPressed(keyOf('E')) && distStation < 460.f) dock();
    if (in.wasPressed(K_TAB)) { uiTab = 2; screen = 3; if (distStation > 460.f) { /* remote inventory view */ } }
    if (in.wasPressed(keyOf('M'))) { screen = 4; mapSel = sector; }
    if (in.wasPressed(K_ESC)) { pendingScreen = 5; settingsFrom = 2; uiTab = 0; }
    if (in.wasPressed(keyOf('H'))) { save.settings.showFps = !save.settings.showFps; }

    // ---- victory condition
    if (save.ship == SHIP_VOIDCRAFT && save.unlockedSectors >= 5 &&
        (save.achMask >> ACH_SENTINEL_FALLS) & 1 && screen == 2) {
        save.achMask2 |= 1 << 12;   // marker: victory reached
        screen = 8;
        saveGame(true);
    }
}

}  // namespace sm
