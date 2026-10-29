#include "game/tools.h"
#include "game/player.h"
#include "game/fx.h"
#include "world/physics.h"
#include "world/voxel.h"
#include <cmath>
#include <cstring>

static const ToolDef g_toolDefs[TOOL_COUNT] = {
    {"NONE", "", "", 0, false},
    {"SLEDGEHAMMER", "Heavy melee demolition. Right click for an overhead strike.",
     "Left click: swing. Right click: overhead smash.", 0, false},
    {"SHOTGUN", "Twelve-gauge breaching tool. Blasts holes through walls.",
     "Left click: fire. R: reload.", 24, true},
    {"EXPLOSIVE CHARGES", "Place sticky charges on any surface, then detonate remotely.",
     "Left click: place. R: detonate all.", 8, true},
    {"ROCKET LAUNCHER", "Shoulder-fired demolition rocket with a large blast radius.",
     "Left click: fire. R: reload.", 4, true},
    {"BLOWTORCH", "Precision cutting torch. Burns wood and cuts metal plate.",
     "Hold left click to cut and ignite.", 0, false},
    {"PLANK TOOL", "Build physical planks between surfaces for traversal or support.",
     "Left click twice to bridge two surfaces. R: cancel.", 0, false},
    {"FIRE EXTINGUISHER", "Suppresses fire and clears smoke. Refill at stations.",
     "Hold left click to spray. R: refill.", 0, false},
    {"GRAPPLING TOOL", "Fires a hook that pulls you toward the anchor point.",
     "Left click: fire/reel in. R: release.", 0, false},
    {"INDUSTRIAL CUTTER", "Heavy cutting beam, devastating against metal structures.",
     "Hold left click to cut.", 0, false},
    {"GRAVITY CABLE", "Tension cable that drags two objects together.",
     "Left click two targets to connect. R: clear cables.", 4, true},
    {"DEMOLITION DRILL", "High-torque drill for concrete, stone and terrain.",
     "Hold left click to drill.", 0, false},
    {"AIRSTRIKE DESIGNATOR", "Marks a coordinate for a simulated bombing run.",
     "Left click: designate target. 25s cooldown.", 0, false},
};

const ToolDef& toolDef(int id) { return g_toolDefs[iclamp(id, 0, TOOL_COUNT - 1)]; }

SfxId impactSoundForMaterial(uint8_t mat) {
    switch (mat) {
        case M_BRICK: case M_TILE: case M_COBBLE: return SFX_HAMMER_BRICK;
        case M_WOOD: case M_WOOD_DARK: case M_PLANK: case M_TRUNK: return SFX_HAMMER_WOOD;
        case M_METAL: case M_METAL_DARK: case M_RUST: case M_GRATE: case M_GOLD:
        case M_CONTAINER_BLUE: case M_CONTAINER_RED: case M_CONTAINER_GREEN: case M_CONTAINER_ORANGE:
            return SFX_HAMMER_METAL;
        case M_GLASS: case M_SKYLIGHT: return SFX_HAMMER_GLASS;
        default: return SFX_HAMMER_CONCRETE;
    }
}
SfxId breakSoundForMaterial(uint8_t mat) {
    switch (mat) {
        case M_WOOD: case M_WOOD_DARK: case M_PLANK: case M_TRUNK: return SFX_WOOD_BREAK;
        case M_METAL: case M_METAL_DARK: case M_RUST: case M_GRATE: return SFX_METAL_BREAK;
        case M_GLASS: case M_SKYLIGHT: return SFX_GLASS_BREAK;
        default: return SFX_CONCRETE_BREAK;
    }
}

static RayHit aimRay(const Player& p, const World& w, float maxDist) {
    vec3 eye = p.pos + vec3(0, p.cameraHeight, 0);
    vec3 dir(std::cos(p.pitch) * std::sin(p.yaw), std::sin(p.pitch), -std::cos(p.pitch) * std::cos(p.yaw));
    return w.raycast(eye, dir, maxDist);
}

void Tools::init() {
    for (int t = 0; t < TOOL_COUNT; t++) ammo[t] = g_toolDefs[t].maxAmmo;
    unlockAll();
    reset();
}

void Tools::unlockAll() {
    // every tool is available immediately; campaigns can restrict later
}

void Tools::reset() {
    charges.clear();
    projectiles.clear();
    cables.clear();
    plank = PlankPreview();
    grappleAttached = false;
    torchFuel = 1.0f;
    extinguisherCharge = 1.0f;
    airstrike = Airstrike();
    airstrikeCooldown = 0;
    for (int t = 0; t < TOOL_COUNT; t++) ammo[t] = g_toolDefs[t].maxAmmo;
    current = TOOL_HAMMER;
    cooldown = 0;
    reloadTimer = 0;
}

bool Tools::hasAmmo(int id) const {
    if (unlimitedAmmo) return true;
    if (id <= 0 || id >= TOOL_COUNT) return false;
    if (!g_toolDefs[id].limitedAmmo) return true;
    return ammo[id] > 0;
}
int Tools::ammoFor(int id) const { return id >= 0 && id < TOOL_COUNT ? ammo[id] : 0; }
void Tools::giveAmmo(int id, int n) {
    if (id <= 0 || id >= TOOL_COUNT) return;
    ammo[id] = std::min(g_toolDefs[id].maxAmmo, ammo[id] + n);
}

void Tools::select(int id) {
    if (id < 0 || id >= TOOL_COUNT || id == current) return;
    current = id;
    cooldown = 0.18f;
    reloadTimer = 0;
    swing = 0;
    vmKick = 0.25f;
    audio::play(SFX_UI_CLICK, vec3(0), 0.25f, 1.6f);
}

void Tools::nextTool(int dir) {
    int n = TOOL_COUNT - 1;
    int idx = current - 1;
    idx = (idx + dir + n) % n;
    select(idx + 1);
}

void Tools::update(float dt, InputState& in, Player& player, World& world, Physics& phys, bool uiBlocking) {
    cooldown = std::max(0.0f, cooldown - dt);
    airstrikeCooldown = std::max(0.0f, airstrikeCooldown - dt);
    hitFlash = std::max(0.0f, hitFlash - dt * 4.0f);
    swing = std::max(0.0f, swing - dt * 3.6f);
    recoil = std::max(0.0f, recoil - dt * 4.5f);
    vmKick = std::max(0.0f, vmKick - dt * 3.5f);
    chargePlaceAnim = std::max(0.0f, chargePlaceAnim - dt * 3.0f);
    if (reloadTimer > 0) {
        reloadTimer -= dt;
        if (reloadTimer <= 0) {
            int t = current;
            if (t == TOOL_SHOTGUN) ammo[t] = std::min(g_toolDefs[t].maxAmmo, ammo[t] + 6);
            if (t == TOOL_ROCKET) ammo[t] = std::min(g_toolDefs[t].maxAmmo, ammo[t] + 1);
            if (t == TOOL_CABLE) ammo[t] = std::min(g_toolDefs[t].maxAmmo, ammo[t] + 1);
            audio::play(SFX_RELOAD, player.pos, 0.35f, 1.2f);
        }
    }
    if (uiBlocking) {
        buildViewModel(player, dt);
        return;
    }
    // tool switching
    for (int i = 1; i <= 9; i++)
        if (in.isPressed(K_1 + i - 1)) select(i);
    if (in.isPressed(K_0)) select(10);
    if (in.isPressed(K_MINUS)) select(TOOL_AIRSTRIKE);
    if (in.isPressed(K_EQUALS)) select(TOOL_HAMMER);
    if (in.wheel != 0 && !plank.active) nextTool(in.wheel > 0 ? 1 : -1);
    if (in.isPressed(K_R)) {
        if (current == TOOL_CHARGES) {
            detonateAll(world, phys);
        } else if (current == TOOL_PLANK) {
            plank = PlankPreview();
        } else if (current == TOOL_EXTINGUISHER) {
            extinguisherCharge = 1.0f;
            audio::play(SFX_CRAFT, player.pos, 0.4f, 1.1f);
        } else if (current == TOOL_GRAPPLE) {
            grappleAttached = false;
        } else if (current == TOOL_CABLE) {
            cables.clear();
            plank = PlankPreview();
        } else if (current == TOOL_SHOTGUN || current == TOOL_ROCKET || current == TOOL_CABLE) {
            if (reloadTimer <= 0 && ammo[current] < g_toolDefs[current].maxAmmo) {
                reloadTimer = current == TOOL_ROCKET ? 2.0f : (current == TOOL_SHOTGUN ? 1.5f : 0.8f);
                audio::play(SFX_SHOTGUN_PUMP, player.pos, 0.3f, 1.3f);
            }
        }
    }
    bool lmb = in.isDown(K_LBUTTON);
    bool rmb = in.isDown(K_RBUTTON);
    bool lmbPress = in.isPressed(K_LBUTTON);
    bool rmbPress = in.isPressed(K_RBUTTON);
    torchActive = drillActive = cutterActive = false;
    sprayAmount = 0;
    switch (current) {
        case TOOL_HAMMER: updateHammer(dt, in, player, world, phys); break;
        case TOOL_SHOTGUN: updateShotgun(dt, in, player, world, phys); break;
        case TOOL_CHARGES: updateCharges(dt, in, player, world, phys); break;
        case TOOL_ROCKET: updateRocket(dt, in, player, world, phys); break;
        case TOOL_BLOWTORCH: updateTorch(dt, in, player, world, phys); break;
        case TOOL_PLANK: updatePlank(dt, in, player, world, phys); break;
        case TOOL_EXTINGUISHER: updateExtinguisher(dt, in, player, world, phys); break;
        case TOOL_GRAPPLE: updateGrapple(dt, in, player, world, phys); break;
        case TOOL_CUTTER: updateCutter(dt, in, player, world, phys, false); break;
        case TOOL_CABLE: updateCable(dt, in, player, world, phys); break;
        case TOOL_DRILL: updateCutter(dt, in, player, world, phys, true); break;
        case TOOL_AIRSTRIKE: updateAirstrike(dt, in, player, world, phys); break;
        default: break;
    }
    (void)lmb; (void)rmb; (void)lmbPress; (void)rmbPress;
    updateProjectiles(dt, world, phys);
    // cable physics
    for (auto& c : cables) {
        if (!c.active) continue;
        Body* a = (c.bodyA >= 0 && c.bodyA < (int)phys.bodies.size() && phys.bodies[c.bodyA].active) ? &phys.bodies[c.bodyA] : nullptr;
        Body* b = (c.bodyB >= 0 && c.bodyB < (int)phys.bodies.size() && phys.bodies[c.bodyB].active) ? &phys.bodies[c.bodyB] : nullptr;
        vec3 pa = a ? a->pos : c.a;
        vec3 pb = b ? b->pos : c.b;
        vec3 d = pb - pa;
        float len = d.len();
        if (len < 0.05f) continue;
        vec3 n = d / len;
        float stretch = len - c.restLen;
        if (stretch > 0.02f) {
            float force = stretch * 260.0f;
            if (a) { a->vel += n * (force * a->invMass * dt); a->sleeping = false; }
            if (b) { b->vel -= n * (force * b->invMass * dt); b->sleeping = false; }
        }
        c.restLen = std::min(c.restLen, len);
    }
    // grapple physics
    if (grappleAttached) {
        vec3 eye = player.pos + vec3(0, player.cameraHeight, 0);
        vec3 d = grapplePoint - eye;
        float len = d.len();
        if (len > 2.2f) {
            vec3 n = d / len;
            float pull = 42.0f;
            player.vel += n * pull * dt;
            player.vel.y += 9.8f * dt * 0.45f;   // counter some gravity while reeling
            player.grounded = false;
        }
        if (len < 1.8f) grappleAttached = false;
    }
    buildViewModel(player, dt);
}

// ---------------------------------------------------------------- hammer
void Tools::updateHammer(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    if (cooldown > 0) return;
    if (!in.isPressed(K_LBUTTON) && !in.isPressed(K_RBUTTON)) return;
    bool heavy = in.isPressed(K_RBUTTON);
    cooldown = heavy ? 0.95f : 0.46f;
    swing = 1.0f;
    vmSwingT = 0;
    p.addShake(heavy ? 0.42f : 0.16f);
    audio::play(heavy ? SFX_DEBRIS_SMALL : SFX_UI_HOVER, p.pos, 0.12f, 0.7f);
    // impact resolves a moment later through the animation timer below
    swingHit = heavy ? 0.16f : 0.12f;
    (void)dt;
    // The actual hit is applied after a short wind-up so the animation and impact line up.
    struct Pending { float t; float power; float radius; };
    static Pending pending{0, 0, 0};
    pending.t = swingHit;
    pending.power = heavy ? 1500.0f : 650.0f;
    pending.radius = heavy ? 0.80f : 0.56f;
    // resolve immediately for responsiveness (frame-accurate wind-up is handled by cooldown)
    RayHit hit = aimRay(p, w, 3.6f);
    if (!hit.hit) {
        audio::play(SFX_DRYFIRE, p.pos, 0.18f, 0.6f);
        return;
    }
    const Material& mt = material(hit.mat);
    audio::play(impactSoundForMaterial(hit.mat), hit.pos, heavy ? 0.95f : 0.75f,
                0.9f + hash3f(hit.cell.x, hit.cell.y, hit.cell.z) * 0.25f);
    g_fx->impact(hit.mat, hit.pos, hit.normal, heavy ? 1.4f : 0.6f, hit.cell.x * 31 + hit.cell.z);
    float power = pending.power;
    // material-dependent resistance is handled by the carve routine
    carveWorld(hit.pos, pending.radius, power, true, false);
    hitFlash = 1.0f;
    lastMaterialHit = hit.mat;
    // push nearby dynamic bodies
    for (int i = 0; i < (int)ph.bodies.size(); i++) {
        Body& b = ph.bodies[i];
        if (!b.active) continue;
        float d = (b.pos - hit.pos).len();
        if (d < 1.2f) {
            b.vel += (b.pos - hit.pos).norm() * (heavy ? 4.5f : 1.8f);
            b.sleeping = false;
        }
    }
    (void)mt;
}

// ---------------------------------------------------------------- shotgun
void Tools::updateShotgun(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)dt; (void)ph;
    if (!in.isPressed(K_LBUTTON) || cooldown > 0 || reloadTimer > 0) return;
    if (!hasAmmo(TOOL_SHOTGUN)) {
        audio::play(SFX_DRYFIRE, p.pos, 0.35f, 1.0f);
        cooldown = 0.3f;
        return;
    }
    if (!unlimitedAmmo) ammo[TOOL_SHOTGUN]--;
    cooldown = 0.72f;
    recoil = 1.0f;
    vmKick = 1.0f;
    p.addShake(0.35f);
    p.recoilPitch += 0.055f;
    p.recoilYaw += (hash3f((int)(g_time * 1000), 1, 2) - 0.5f) * 0.02f;
    audio::play(SFX_SHOTGUN, p.pos, 1.0f, 0.95f + hash3f((int)g_time, 3, 4) * 0.1f);
    vec3 eye = p.pos + vec3(0, p.cameraHeight, 0);
    vec3 dir(std::cos(p.pitch) * std::sin(p.yaw), std::sin(p.pitch), -std::cos(p.pitch) * std::cos(p.yaw));
    g_fx->muzzleFlash(eye + dir * 0.7f, dir);
    int pellets = 9;
    for (int i = 0; i < pellets; i++) {
        float spread = 0.052f;
        vec3 d = (dir + vec3((hash3f(i, (int)(g_time * 100), 1, 7) - 0.5f) * spread * 2,
                             (hash3f(i, (int)(g_time * 100), 2, 8) - 0.5f) * spread * 2,
                             (hash3f(i, (int)(g_time * 100), 3, 9) - 0.5f) * spread * 2)).norm();
        RayHit hit = w.raycast(eye, d, 40.0f);
        if (!hit.hit) continue;
        carveWorld(hit.pos, 0.20f, 950.0f, false, false);
        g_fx->impact(hit.mat, hit.pos, hit.normal, 0.45f, i * 17 + (int)(g_time * 10));
        if (i < 3) audio::play(impactSoundForMaterial(hit.mat), hit.pos, 0.4f, 1.1f);
    }
    if (unlimitedAmmo || ammo[TOOL_SHOTGUN] <= 0) {
        reloadTimer = 1.5f;
        audio::play(SFX_SHOTGUN_PUMP, p.pos, 0.4f, 1.2f);
    }
}

// ---------------------------------------------------------------- charges
void Tools::updateCharges(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)ph;
    // arm placed charges over time
    for (auto& c : charges) {
        c.armTime += dt;
        if (c.armTime > 0.55f && !c.armed) {
            c.armed = true;
            audio::play(SFX_CHARGE_BEEP, c.pos, 0.35f, 1.5f);
        }
    }
    if (!in.isPressed(K_LBUTTON) || cooldown > 0) return;
    if ((int)charges.size() >= (unlimitedAmmo ? 24 : 8)) {
        audio::play(SFX_DRYFIRE, p.pos, 0.2f, 1.3f);
        return;
    }
    RayHit hit = aimRay(p, w, 5.0f);
    if (!hit.hit) return;
    PlacedCharge c;
    c.pos = hit.pos + hit.normal * 0.09f;
    c.normal = hit.normal;
    charges.push_back(c);
    cooldown = 0.22f;
    chargePlaceAnim = 1.0f;
    audio::play(SFX_PLANT, c.pos, 0.5f, 1.0f + hash3f((int)c.pos.x, 1, 1) * 0.2f);
    g_fx->sparks(hit.pos, hit.normal, 3, 1.5f);
}

void Tools::detonateAll(World& w, Physics& ph) {
    (void)w;
    if (charges.empty()) return;
    int n = (int)charges.size();
    for (auto& c : charges) {
        explodeAt(c.pos, 3.4f, 1);
        ph.applyRadialImpulse(c.pos, 12.0f, 2.2f);
    }
    charges.clear();
    audio::play(SFX_DETONATE, vec3(0), 1.0f, 1.0f);
    (void)n;
}

// ---------------------------------------------------------------- rocket
void Tools::updateRocket(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)dt; (void)w; (void)ph;
    if (!in.isPressed(K_LBUTTON) || cooldown > 0 || reloadTimer > 0) return;
    if (!hasAmmo(TOOL_ROCKET)) {
        audio::play(SFX_DRYFIRE, p.pos, 0.3f, 1.0f);
        return;
    }
    if (!unlimitedAmmo) ammo[TOOL_ROCKET]--;
    cooldown = 0.9f;
    recoil = 1.2f;
    vmKick = 1.2f;
    p.addShake(0.55f);
    p.recoilPitch += 0.09f;
    audio::play(SFX_ROCKET_FIRE, p.pos, 1.0f, 1.0f);
    vec3 eye = p.pos + vec3(0, p.cameraHeight - 0.12f, 0);
    vec3 dir(std::cos(p.pitch) * std::sin(p.yaw), std::sin(p.pitch), -std::cos(p.pitch) * std::cos(p.yaw));
    Rockets r;
    r.pos = eye + dir * 0.8f + vec3(0, -0.12f, 0);
    r.vel = dir * 42.0f;
    r.life = 6.0f;
    r.active = true;
    r.kind = 0;
    projectiles.push_back(r);
    g_fx->muzzleFlash(r.pos, dir);
    if (!unlimitedAmmo && ammo[TOOL_ROCKET] <= 0) reloadTimer = 2.0f;
}

void Tools::updateProjectiles(float dt, World& w, Physics& ph) {
    for (auto& r : projectiles) {
        if (!r.active) continue;
        r.life -= dt;
        r.vel.y -= (r.kind == 2 ? 9.8f : 1.2f) * dt;
        vec3 next = r.pos + r.vel * dt;
        RayHit hit = w.raycast(r.pos, (next - r.pos).norm(), (next - r.pos).len() + 0.05f);
        // smoke trail
        g_fx->smokeTrail(r.pos, vec3(0, 0.4f, 0), 0.10f, packColor(0.65f, 0.64f, 0.62f), 0.9f);
        g_fx->smokeTrail(r.pos, vec3(0, 0.2f, 0), 0.05f, packColor(1.0f, 0.7f, 0.3f), 0.25f);
        bool hitBody = false;
        vec3 bodyPos;
        for (auto& b : ph.bodies) {
            if (!b.active) continue;
            if ((b.pos - next).len() < b.half.len() * 0.8f + 0.12f) {
                hitBody = true;
                bodyPos = b.pos;
                break;
            }
        }
        if (hit.hit || hitBody || r.life <= 0) {
            vec3 boomPos = hit.hit ? hit.pos : (hitBody ? bodyPos : r.pos);
            r.active = false;
            if (r.kind == 0) {
                explodeAt(boomPos, 4.2f, 1);
                ph.applyRadialImpulse(boomPos, 14.0f, 2.6f);
                audio::play(SFX_EXPLOSION_LARGE, boomPos, 1.0f, 0.95f);
            } else if (r.kind == 1) {
                explodeAt(boomPos, 8.5f, 2);
                ph.applyRadialImpulse(boomPos, 26.0f, 3.2f);
                audio::play(SFX_EXPLOSION_LARGE, boomPos, 1.0f, 0.8f);
            } else {
                explodeAt(boomPos, 3.0f, 1);
                ph.applyRadialImpulse(boomPos, 9.0f, 1.8f);
                audio::play(SFX_EXPLOSION_SMALL, boomPos, 0.9f, 1.1f);
            }
            continue;
        }
        r.pos = next;
    }
    for (int i = (int)projectiles.size() - 1; i >= 0; i--)
        if (!projectiles[i].active) projectiles.erase(projectiles.begin() + i);
}

// ---------------------------------------------------------------- torch
void Tools::updateTorch(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)ph;
    if (!in.isDown(K_LBUTTON)) return;
    if (torchFuel <= 0 && !infiniteFuel) {
        static float warnTimer = 0;
        warnTimer -= dt;
        if (warnTimer <= 0) { warnTimer = 1.2f; audio::play(SFX_DRYFIRE, p.pos, 0.2f, 0.7f); }
        return;
    }
    torchActive = true;
    torchFuel = std::max(0.0f, torchFuel - dt * 0.035f);
    RayHit hit = aimRay(p, w, 3.2f);
    if (!hit.hit) return;
    beamHit = true;
    beamEnd = hit.pos;
    const Material& mt = material(hit.mat);
    // cutting speed depends on the material: fast on wood/plaster, slow on metal
    float cutPower = 26000.0f;   // heat torch: fast through wood, very slow through steel
    if (hit.mat == M_METAL || hit.mat == M_METAL_DARK || hit.mat == M_RUST || hit.mat == M_GRATE) cutPower = 9000.0f;
    if (hit.mat == M_REINFORCED) cutPower = 5000.0f;
    if (mt.flammable) {
        beamTimer -= dt;
        if (beamTimer <= 0) {
            beamTimer = 0.25f;
            g_fx->ignite(hit.cell.x, hit.cell.y, hit.cell.z, 1.2f);
        }
    }
    carveWorld(hit.pos, 0.17f, cutPower * dt, false, false);
    g_fx->sparks(hit.pos, hit.normal, 2, 2.2f);
    if (hash32((uint32_t)(g_time * 1000)) % 6 == 0)
        g_fx->smokePlume(hit.pos, vec3(0, 0.6f, 0), 0.06f, 0.7f, packColor(0.7f, 0.7f, 0.7f), 0);
    static float sndTimer = 0;
    sndTimer -= dt;
    if (sndTimer <= 0) { sndTimer = 0.22f; audio::play(SFX_TORCH, hit.pos, 0.35f, 1.0f + hash3f((int)(g_time * 10), 1, 1) * 0.2f); }
}

// ---------------------------------------------------------------- plank
void Tools::updatePlank(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)dt;
    RayHit hit = aimRay(p, w, 8.0f);
    vec3 eye = p.pos + vec3(0, p.cameraHeight, 0);
    vec3 dir(std::cos(p.pitch) * std::sin(p.yaw), std::sin(p.pitch), -std::cos(p.pitch) * std::cos(p.yaw));
    if (!plank.haveA) {
        plank.active = false;
        if (hit.hit) {
            plank.b = hit.pos;
            plank.a = hit.pos;
        } else {
            plank.b = eye + dir * 4.0f;
        }
        if (in.isPressed(K_LBUTTON) && hit.hit) {
            plank.haveA = true;
            plank.a = hit.pos + hit.normal * 0.06f;
            plank.aNormal = hit.normal;
            audio::play(SFX_UI_CLICK, p.pos, 0.4f, 1.3f);
            g_fx->sparks(hit.pos, hit.normal, 4, 1.6f);
        }
        return;
    }
    plank.active = true;
    // endpoint follows the aim
    vec3 end;
    if (hit.hit && (hit.pos - plank.a).len() > 0.5f) end = hit.pos + hit.normal * 0.06f;
    else end = eye + dir * 4.0f;
    plank.b = end;
    if (in.isPressed(K_RBUTTON)) { plank = PlankPreview(); audio::play(SFX_UI_BACK, p.pos, 0.35f, 1.0f); return; }
    if (!in.isPressed(K_LBUTTON)) return;
    float len = (plank.b - plank.a).len();
    if (len < 0.4f || len > 14.0f) {
        audio::play(SFX_DRYFIRE, p.pos, 0.3f, 0.8f);
        plank = PlankPreview();
        return;
    }
    // build the plank as a static physical beam
    vec3 mid = (plank.a + plank.b) * 0.5f;
    vec3 d = (plank.b - plank.a).norm();
    float pitchAngle = std::asin(clampf(d.y, -1, 1));
    float yawAngle = std::atan2(d.x, -d.z);
    vec3 half(0.15f, 0.055f, len * 0.5f);
    // orient as a yaw/pitch rotation
    quat q = quatFromEuler(yawAngle, -pitchAngle, 0);
    int idx = ph.spawn(mid, half, M_PLANK, 60.0f, vec3(0), vec3(0), -1, false);
    if (idx < 0) { plank = PlankPreview(); return; }
    Body& b = ph.bodies[idx];
    b.rot = q;
    b.invMass = 0;
    b.invI = vec3(0);
    b.restitution = 0.02f;
    b.friction = 0.9f;
    b.sleeping = true;
    b.color = vec3(0.66f, 0.51f, 0.33f);
    ph.updateModel(b);
    audio::play(SFX_PLANK, mid, 0.6f, 1.0f);
    g_fx->dustBurst(plank.a, 4, 0.15f, 1.0f, vec3(0.6f, 0.5f, 0.35f));
    plank = PlankPreview();
    cooldown = 0.25f;
}

// ---------------------------------------------------------------- extinguisher
void Tools::updateExtinguisher(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    if (!in.isDown(K_LBUTTON) || extinguisherCharge <= 0) return;
    extinguisherCharge = std::max(0.0f, extinguisherCharge - dt * 0.14f);
    sprayAmount = 1.0f;
    vec3 eye = p.pos + vec3(0, p.cameraHeight - 0.1f, 0);
    vec3 dir(std::cos(p.pitch) * std::sin(p.yaw), std::sin(p.pitch), -std::cos(p.pitch) * std::cos(p.yaw));
    vec3 nozzle = eye + dir * 0.55f + p.makeCamera(1.0f).right() * 0.22f - vec3(0, 0.2f, 0);
    g_fx->extinguishSpray(nozzle, dir, dt);
    // suppress fire in a cone
    vec3 sprayPoint = eye + dir * 3.2f;
    g_fx->extinguishArea(sprayPoint, 2.6f, 3.2f * dt);
    g_fx->extinguishArea(eye + dir * 1.6f, 1.8f, 2.0f * dt);
    // push light debris
    for (auto& b : ph.bodies) {
        if (!b.active || b.mass > 40) continue;
        vec3 toB = b.pos - nozzle;
        float d = toB.len();
        if (d < 4.0f && dot(toB.norm(), dir) > 0.55f) {
            b.vel += dir * (9.0f * dt * b.invMass * 6.0f);
            b.sleeping = false;
        }
    }
    (void)w;
    static float snd = 0;
    snd -= dt;
    if (snd <= 0) { snd = 0.3f; audio::play(SFX_EXTINGUISH, nozzle, 0.3f, 1.0f); }
}

// ---------------------------------------------------------------- grapple
void Tools::updateGrapple(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)dt; (void)ph;
    if (in.isPressed(K_LBUTTON) && !grappleAttached) {
        RayHit hit = aimRay(p, w, 42.0f);
        if (hit.hit) {
            grappleAttached = true;
            grapplePoint = hit.pos - hit.normal * 0.15f;
            grappleRope = (grapplePoint - p.pos).len();
            audio::play(SFX_GRAPPLE, p.pos, 0.6f, 1.0f);
            g_fx->sparks(hit.pos, hit.normal, 5, 1.6f);
        } else {
            audio::play(SFX_DRYFIRE, p.pos, 0.2f, 1.2f);
        }
    }
}

// ---------------------------------------------------------------- cutter / drill
void Tools::updateCutter(float dt, InputState& in, Player& p, World& w, Physics& ph, bool drill) {
    (void)ph;
    if (!in.isDown(K_LBUTTON)) {
        heat = std::max(0.0f, heat - dt * 0.9f);
        return;
    }
    if (drill) drillActive = true; else cutterActive = true;
    heat = std::min(1.0f, heat + dt * 0.28f);
    RayHit hit = aimRay(p, w, drill ? 3.0f : 4.0f);
    if (!hit.hit) return;
    beamHit = true;
    beamEnd = hit.pos;
    float power;
    if (drill) {
        power = hit.mat == M_METAL ? 70000.0f : 90000.0f;
        if (hit.mat == M_REINFORCED) power = 45000.0f;
    } else {
        power = 70000.0f;
        if (hit.mat == M_METAL || hit.mat == M_METAL_DARK || hit.mat == M_RUST || hit.mat == M_GRATE) {
            power = 110000.0f;
        } else if (hit.mat == M_REINFORCED) {
            power = 60000.0f;
        }
    }
    carveWorld(hit.pos, drill ? 0.30f : 0.24f, power * dt, false, false);
    for (int i = 0; i < 3; i++) g_fx->sparks(hit.pos, hit.normal, 1, drill ? 3.0f : 4.5f);
    p.addShake(drill ? 0.035f : 0.02f);
    static float snd = 0;
    snd -= dt;
    if (snd <= 0) {
        snd = drill ? 0.24f : 0.26f;
        audio::play(drill ? SFX_DRILL : SFX_CUTTER, hit.pos, 0.38f, 1.0f + hash3f((int)(g_time * 20), 1, 1) * 0.15f);
    }
    if (drill) g_fx->dustBurst(hit.pos, 1, 0.12f, 0.8f, material(hit.mat).color);
}

// ---------------------------------------------------------------- cable
void Tools::updateCable(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)dt;
    if (!in.isPressed(K_LBUTTON)) return;
    if (!hasAmmo(TOOL_CABLE)) { audio::play(SFX_DRYFIRE, p.pos, 0.2f, 1.2f); return; }
    vec3 eye = p.pos + vec3(0, p.cameraHeight, 0);
    vec3 dir(std::cos(p.pitch) * std::sin(p.yaw), std::sin(p.pitch), -std::cos(p.pitch) * std::cos(p.yaw));
    if (!plank.haveA) {
        // pick a body or a solid anchor
        int bestBody = -1;
        float bestD = 4.0f;
        for (int i = 0; i < (int)ph.bodies.size(); i++) {
            Body& b = ph.bodies[i];
            if (!b.active || b.invMass <= 0) continue;
            float d = (b.pos - (eye + dir * 1.6f)).len();
            if (d < bestD) { bestD = d; bestBody = i; }
        }
        RayHit hit = aimRay(p, w, 14.0f);
        if (bestBody >= 0) {
            plank.haveA = true;
            plank.a = ph.bodies[bestBody].pos;
            plank.aNormal = vec3((float)bestBody, 0, 0);
            audio::play(SFX_UI_CLICK, p.pos, 0.4f, 1.5f);
        } else if (hit.hit) {
            plank.haveA = true;
            plank.a = hit.pos;
            plank.aNormal = vec3(-1, -1, -1);
            audio::play(SFX_UI_CLICK, p.pos, 0.4f, 1.3f);
        }
        return;
    }
    // second anchor
    int bestBody = -1;
    float bestD = 4.0f;
    for (int i = 0; i < (int)ph.bodies.size(); i++) {
        Body& b = ph.bodies[i];
        if (!b.active || b.invMass <= 0) continue;
        float d = (b.pos - (eye + dir * 1.6f)).len();
        if (d < bestD) { bestD = d; bestBody = i; }
    }
    RayHit hit = aimRay(p, w, 16.0f);
    CableLink c;
    int firstBody = (plank.aNormal.x >= 0 && plank.aNormal.y == 0 && plank.aNormal.z == 0) ? (int)plank.aNormal.x : -1;
    c.a = plank.a;
    c.bodyA = firstBody;
    if (bestBody >= 0) { c.b = ph.bodies[bestBody].pos; c.bodyB = bestBody; }
    else if (hit.hit) { c.b = hit.pos; c.bodyB = -1; }
    else { plank = PlankPreview(); return; }
    c.restLen = (c.a - c.b).len() * 0.92f;
    cables.push_back(c);
    if (cables.size() > 4) cables.erase(cables.begin());
    if (!unlimitedAmmo) ammo[TOOL_CABLE]--;
    audio::play(SFX_PLANK, c.b, 0.5f, 0.8f);
    plank = PlankPreview();
}

// ---------------------------------------------------------------- airstrike
void Tools::updateAirstrike(float dt, InputState& in, Player& p, World& w, Physics& ph) {
    (void)ph;
    if (in.isPressed(K_LBUTTON) && airstrike.state == 0 && airstrikeCooldown <= 0) {
        RayHit hit = aimRay(p, w, 120.0f);
        if (hit.hit) {
            airstrike.target = hit.pos;
            airstrike.state = 1;
            airstrike.timer = 3.2f;
            airstrikeCooldown = 26.0f;
            audio::play(SFX_UI_CLICK, p.pos, 0.6f, 0.7f);
        }
    }
    if (airstrike.state == 1) {
        airstrike.timer -= dt;
        // jet approach
        if (airstrike.timer < 1.6f && airstrike.bombsLeft == 0) {
            airstrike.bombsLeft = 3;
            airstrike.state = 2;
            airstrike.bombTimer = 0;
            airstrike.planePos = airstrike.target + vec3(-70, 46, -20);
            audio::play(SFX_AIRSTRIKE_FLYBY, airstrike.target, 1.0f, 1.0f);
        }
        g_fx->smokePlume(airstrike.target + vec3(0, 0.3f, 0), vec3(0, 1.4f, 0), 0.34f, 1.0f,
                         packColor(1.0f, 0.25f, 0.15f), 3);
    }
    if (airstrike.state == 2) {
        airstrike.planePos += vec3(30.0f, -14.0f, 9.0f) * dt;
        g_fx->smokeTrail(airstrike.planePos, vec3(0, 0, 0), 0.25f, packColor(0.6f, 0.6f, 0.65f), 2.2f);
        airstrike.bombTimer -= dt;
        if (airstrike.bombTimer <= 0 && airstrike.bombsLeft > 0) {
            airstrike.bombTimer = 0.42f;
            airstrike.bombsLeft--;
            Rockets r;
            r.pos = airstrike.planePos;
            vec3 spread((hash3f(airstrike.bombsLeft, 1, 1) - 0.5f) * 7.0f, 0, (hash3f(airstrike.bombsLeft, 2, 2) - 0.5f) * 7.0f);
            vec3 targetPoint = airstrike.target + spread;
            vec3 toTarget = targetPoint - r.pos;
            r.vel = toTarget.norm() * 62.0f;
            r.life = 8.0f;
            r.active = true;
            r.kind = 1;
            projectiles.push_back(r);
            audio::play(SFX_ROCKET_FLY, r.pos, 0.9f, 0.8f);
            if (airstrike.bombsLeft == 0) { airstrike.state = 0; airstrike.timer = 0; }
        }
    }
}

// ---------------------------------------------------------------- view model
void Tools::addBox(vec3 pos, vec3 size, vec3 color, vec3 rot, float emis, float metal, float rough) {
    ViewModelBox b;
    b.pos = pos;
    b.size = size;
    b.color = color;
    b.rot = rot;
    b.emissive = emis;
    b.metal = metal;
    b.rough = rough;
    vmBoxes.push_back(b);
}

void Tools::buildViewModel(Player& p, float dt) {
    vmBoxes.clear();
    float swayX = p.toolSwayX, swayY = p.toolSwayY;
    float bobX = p.toolBobX, bobY = p.toolBobY;
    float baseY = -0.24f + swayY + bobY - (1.0f - p.equipAnim) * 0.4f;
    float baseX = 0.20f + swayX + bobX;
    float kick = vmKick * 0.12f;
    // swing animation drives the hammer/tool arc
    float swingA = 0;
    if (swing > 0.001f) {
        float t = 1.0f - swing;               // 0..1 over the swing
        swingA = std::sin(t * PI) * 1.0f;
        swingA = std::pow(swingA, 0.65f);
    }
    (void)dt;
    switch (current) {
        case TOOL_HAMMER: {
            // swing: the whole rig arcs forward and down when the hammer lands
            float a = -0.66f + swingA * 1.55f;
            float reach = swingA * 0.20f;
            vec3 hand(baseX + 0.02f, baseY + 0.10f - swingA * 0.22f, 0.62f + reach);
            vec3 tilt(a, 0.06f, -0.30f);
            // shaft
            addBox(hand + vec3(0.0f, 0.17f, 0.17f), vec3(0.048f, 0.048f, 0.56f), vec3(0.40f, 0.26f, 0.14f), tilt, 0, 0.0f, 0.78f);
            // steel head
            addBox(hand + vec3(-0.012f, 0.40f, 0.335f), vec3(0.27f, 0.095f, 0.105f), vec3(0.30f, 0.31f, 0.33f), tilt, 0, 0.88f, 0.32f);
            // grip wrap + gloved hand
            addBox(hand + vec3(0.0f, -0.01f, -0.02f), vec3(0.068f, 0.16f, 0.072f), vec3(0.14f, 0.12f, 0.10f), tilt, 0, 0.05f, 0.9f);
            addBox(hand + vec3(0.005f, 0.045f, 0.05f), vec3(0.075f, 0.075f, 0.10f), vec3(0.58f, 0.42f, 0.26f), tilt, 0, 0.0f, 0.85f);
            break;
        }
        case TOOL_SHOTGUN: {
            vec3 hand(baseX, baseY, 0.40f - kick);
            addBox(hand, vec3(0.075f, 0.09f, 0.34f), vec3(0.16f, 0.15f, 0.16f), vec3(0, 0.06f, 0), 0, 0.7f, 0.35f);
            addBox(hand + vec3(0, 0.028f, 0.36f), vec3(0.045f, 0.045f, 0.42f), vec3(0.20f, 0.21f, 0.24f), vec3(0, 0.06f, 0), 0, 0.9f, 0.25f);
            addBox(hand + vec3(0, -0.045f, 0.14f), vec3(0.07f, 0.07f, 0.16f), vec3(0.32f, 0.20f, 0.11f), vec3(0, 0.06f, 0), 0, 0.1f, 0.7f);
            addBox(hand + vec3(0, 0.072f, 0.06f), vec3(0.02f, 0.02f, 0.02f), vec3(1.0f, 0.3f, 0.1f), vec3(0, 0, 0), 1.2f, 0, 0.4f);
            break;
        }
        case TOOL_CHARGES: {
            vec3 hand(baseX - 0.03f, baseY, 0.36f - chargePlaceAnim * 0.1f);
            addBox(hand, vec3(0.10f, 0.13f, 0.13f), vec3(0.42f, 0.10f, 0.09f), vec3(0, 0.2f, 0.1f), 0, 0.2f, 0.5f);
            addBox(hand + vec3(0.05f, 0.02f, 0.0f), vec3(0.03f, 0.03f, 0.03f), vec3(1.0f, 0.25f, 0.1f), vec3(0, 0, 0), 2.0f, 0, 0.4f);
            addBox(hand + vec3(-0.02f, 0.10f, 0.02f), vec3(0.03f, 0.06f, 0.03f), vec3(0.8f, 0.78f, 0.72f), vec3(0, 0, 0), 0, 0.4f, 0.5f);
            // extra charges on the belt
            for (int i = 0; i < 3; i++)
                addBox(hand + vec3(-0.14f + i * 0.055f, -0.13f, -0.02f), vec3(0.05f, 0.07f, 0.06f), vec3(0.4f, 0.1f, 0.08f), vec3(0, 0.3f, 0), 0, 0.2f, 0.6f);
            break;
        }
        case TOOL_ROCKET: {
            vec3 hand(baseX - 0.06f, baseY + 0.02f, 0.30f - kick * 1.6f);
            addBox(hand + vec3(0, 0, 0.18f), vec3(0.13f, 0.13f, 0.62f), vec3(0.22f, 0.24f, 0.20f), vec3(0, 0.05f, 0), 0, 0.4f, 0.55f);
            addBox(hand + vec3(0, 0.005f, -0.16f), vec3(0.16f, 0.16f, 0.10f), vec3(0.16f, 0.17f, 0.16f), vec3(0, 0.05f, 0), 0, 0.3f, 0.6f);
            addBox(hand + vec3(0.02f, 0.10f, 0.06f), vec3(0.03f, 0.05f, 0.09f), vec3(0.35f, 0.35f, 0.38f), vec3(0, 0.05f, 0), 0, 0.8f, 0.3f);
            break;
        }
        case TOOL_BLOWTORCH: {
            vec3 hand(baseX - 0.02f, baseY, 0.40f);
            addBox(hand, vec3(0.09f, 0.09f, 0.26f), vec3(0.55f, 0.20f, 0.12f), vec3(0, 0.1f, 0), 0, 0.5f, 0.45f);
            addBox(hand + vec3(0, 0.01f, 0.22f), vec3(0.03f, 0.03f, 0.14f), vec3(0.6f, 0.6f, 0.62f), vec3(0, 0.1f, 0), 0, 0.9f, 0.3f);
            if (torchActive) {
                addBox(hand + vec3(0, 0.01f, 0.34f), vec3(0.05f, 0.05f, 0.10f), vec3(0.35f, 0.65f, 1.0f), vec3(0, 0.1f, 0), 2.2f, 0, 0.2f);
            }
            break;
        }
        case TOOL_PLANK: {
            vec3 hand(baseX, baseY, 0.38f);
            addBox(hand, vec3(0.07f, 0.07f, 0.22f), vec3(0.6f, 0.4f, 0.2f), vec3(0, 0.1f, 0), 0, 0.1f, 0.7f);
            if (plank.haveA) {
                addBox(hand + vec3(0, 0.16f, 0.05f), vec3(0.05f, 0.05f, 0.5f), vec3(0.7f, 0.55f, 0.35f), vec3(0.6f, 0.2f, 0), 0, 0, 0.75f);
            } else {
                addBox(hand + vec3(0, 0.12f, 0.08f), vec3(0.055f, 0.035f, 0.42f), vec3(0.72f, 0.56f, 0.34f), vec3(0.25f, 0.1f, 0), 0, 0, 0.8f);
            }
            break;
        }
        case TOOL_EXTINGUISHER: {
            vec3 hand(baseX - 0.02f, baseY - 0.02f, 0.42f);
            addBox(hand, vec3(0.14f, 0.20f, 0.13f), vec3(0.62f, 0.12f, 0.10f), vec3(0.05f, 0.05f, 0), 0, 0.55f, 0.35f);
            addBox(hand + vec3(0, 0.12f, 0.02f), vec3(0.05f, 0.05f, 0.05f), vec3(0.25f, 0.25f, 0.26f), vec3(0, 0, 0), 0, 0.7f, 0.4f);
            addBox(hand + vec3(0.06f, 0.16f, 0.06f), vec3(0.03f, 0.03f, 0.18f), vec3(0.2f, 0.2f, 0.22f), vec3(-0.5f, 0.2f, 0), 0, 0.8f, 0.35f);
            addBox(hand + vec3(0, -0.03f, 0.09f), vec3(0.06f, 0.05f, 0.02f), vec3(0.9f, 0.85f, 0.4f), vec3(0, 0, 0), 0, 0.2f, 0.5f);
            break;
        }
        case TOOL_GRAPPLE: {
            vec3 hand(baseX - 0.02f, baseY + 0.01f, 0.38f);
            addBox(hand, vec3(0.09f, 0.11f, 0.24f), vec3(0.30f, 0.32f, 0.35f), vec3(0, 0.08f, 0), 0, 0.75f, 0.35f);
            addBox(hand + vec3(0, 0.04f, 0.18f), vec3(0.05f, 0.05f, 0.16f), vec3(0.22f, 0.24f, 0.26f), vec3(0, 0.08f, 0), 0, 0.9f, 0.3f);
            addBox(hand + vec3(0, -0.07f, 0.02f), vec3(0.05f, 0.09f, 0.06f), vec3(0.55f, 0.45f, 0.2f), vec3(0, 0, 0), 0, 0.6f, 0.4f);
            addBox(hand + vec3(0.04f, 0.06f, 0.1f), vec3(0.02f, 0.02f, 0.02f), vec3(1.0f, 0.5f, 0.2f), vec3(0, 0, 0), 1.7f, 0, 0.4f);
            break;
        }
        case TOOL_CUTTER: {
            vec3 hand(baseX - 0.02f, baseY + 0.02f, 0.40f);
            addBox(hand, vec3(0.08f, 0.10f, 0.22f), vec3(0.42f, 0.30f, 0.12f), vec3(0, 0.08f, 0), 0, 0.35f, 0.5f);
            addBox(hand + vec3(0, 0.03f, 0.24f), vec3(0.14f, 0.14f, 0.02f), vec3(0.55f, 0.57f, 0.60f), vec3(0, 0.08f, (float)fmod(g_time * 40, 6.28)), 0, 0.95f, 0.3f);
            if (cutterActive) addBox(hand + vec3(0, 0.03f, 0.31f), vec3(0.03f, 0.03f, 0.10f), vec3(1.0f, 0.55f, 0.2f), vec3(0, 0, 0), 2.4f, 0, 0.2f);
            break;
        }
        case TOOL_CABLE: {
            vec3 hand(baseX - 0.02f, baseY, 0.38f);
            addBox(hand, vec3(0.09f, 0.12f, 0.26f), vec3(0.24f, 0.36f, 0.34f), vec3(0, 0.08f, 0), 0, 0.4f, 0.5f);
            addBox(hand + vec3(0, 0.03f, 0.2f), vec3(0.04f, 0.04f, 0.14f), vec3(0.7f, 0.72f, 0.75f), vec3(0, 0.08f, 0), 0, 0.9f, 0.3f);
            addBox(hand + vec3(0, -0.09f, -0.02f), vec3(0.11f, 0.11f, 0.11f), vec3(0.35f, 0.30f, 0.22f), vec3(0, 0, 0), 0, 0.2f, 0.7f);
            break;
        }
        case TOOL_DRILL: {
            vec3 hand(baseX - 0.03f, baseY, 0.36f);
            addBox(hand, vec3(0.11f, 0.14f, 0.26f), vec3(0.28f, 0.30f, 0.33f), vec3(0, 0.08f, 0), 0, 0.7f, 0.4f);
            addBox(hand + vec3(0, 0, 0.20f), vec3(0.05f, 0.05f, 0.26f), vec3(0.55f, 0.57f, 0.60f), vec3(0, 0.08f, drillActive ? (float)fmod(g_time * 60, 6.28) : 0), 0, 0.95f, 0.3f);
            addBox(hand + vec3(0, -0.10f, -0.06f), vec3(0.09f, 0.10f, 0.12f), vec3(0.2f, 0.2f, 0.22f), vec3(0, 0, 0), 0, 0.5f, 0.5f);
            if (drillActive) addBox(hand + vec3(0, 0, 0.35f), vec3(0.04f, 0.04f, 0.06f), vec3(1.0f, 0.6f, 0.25f), vec3(0, 0, 0), 2.0f, 0, 0.3f);
            break;
        }
        case TOOL_AIRSTRIKE: {
            vec3 hand(baseX, baseY + 0.02f, 0.34f);
            addBox(hand, vec3(0.10f, 0.055f, 0.20f), vec3(0.18f, 0.22f, 0.16f), vec3(0, 0.1f, 0), 0, 0.3f, 0.5f);
            addBox(hand + vec3(0.0f, 0.055f, 0.02f), vec3(0.09f, 0.04f, 0.12f), vec3(0.25f, 0.55f, 0.35f), vec3(0, 0.1f, 0), 0.9f, 0, 0.35f);
            addBox(hand + vec3(-0.06f, 0.12f, -0.04f), vec3(0.012f, 0.14f, 0.012f), vec3(0.6f, 0.62f, 0.65f), vec3(0, 0, 0.2f), 0, 0.9f, 0.3f);
            if (airstrike.state == 1) {
                float blink = std::fmod(g_time * 6.0f, 1.0f) < 0.5f ? 1.0f : 0.15f;
                addBox(hand + vec3(0.075f, 0.03f, 0.06f), vec3(0.02f, 0.02f, 0.02f), vec3(1, 0.2f, 0.15f), vec3(0, 0, 0), 2.2f * blink, 0, 0.4f);
            }
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------- view model rendering
void Tools::drawViewModel(IRenderer& renderer, Player& p, const Camera& cam, float aspect) {
    if (vmBoxes.empty()) return;
    (void)p; (void)aspect;
    static std::vector<BoxInstance> inst;
    inst.clear();
    vec3 fwd = cam.forward(), right = cam.right(), up = cross(right, fwd);
    mat4 basis;   // identity, then replace the rotation columns with the camera basis
    basis.at(0, 0) = right.x; basis.at(0, 1) = right.y; basis.at(0, 2) = right.z;
    basis.at(1, 0) = up.x;    basis.at(1, 1) = up.y;    basis.at(1, 2) = up.z;
    basis.at(2, 0) = fwd.x;   basis.at(2, 1) = fwd.y;   basis.at(2, 2) = fwd.z;
    // view-model framing: push the rig further out and shrink it slightly so it reads as
    // a first-person tool in the lower-right instead of filling the screen
    for (auto& b : vmBoxes) {
        vec3 lp = b.pos;
        lp.z *= 1.70f;
        lp.x += 0.02f;
        lp.y -= 0.02f;
        vec3 world = cam.pos + right * lp.x + up * lp.y + fwd * lp.z;
        mat4 rot = matRotZ(b.rot.z) * matRotY(b.rot.y) * matRotX(b.rot.x);
        BoxInstance bi;
        bi.model = matTranslate(world) * basis * rot * matScale(b.size * 0.80f);
        bi.color = b.color;
        // dedicated view-model fill light: the held tool must always read clearly,
        // independent of how dark the environment is
        bi.emis = b.color * (0.20f + b.emissive) + vec3(0.015f);
        bi.rough = b.rough;
        bi.metal = b.metal;
        inst.push_back(bi);
    }
    // view model pass: clear depth so the tool is never clipped by geometry, use a tighter projection
    renderer.setViewportOverride(0, 0, renderer.frameWidth(), renderer.frameHeight(), true);
    renderer.drawBoxes(inst.data(), (int)inst.size());
    renderer.clearViewportOverride();
}
