#include "game/vehicles.h"
#include "game/fx.h"
#include "game/globals.h"
#include "game/player.h"
#include "world/voxel.h"
#include <cmath>
#include <algorithm>

static const VehicleDef g_defs[VK_COUNT] = {
    // name            len   wid   hgt   mass   top   accel turn  grip  wheelR wheels carve boat
    {"SEDAN",          4.30f, 1.80f, 1.35f, 1350, 33.0f, 13.0f, 2.2f, 8.5f, 0.33f, 4, 6.0f, false},
    {"PICKUP",         5.20f, 2.00f, 1.85f, 2100, 30.0f, 11.0f, 1.9f, 7.5f, 0.40f, 4, 12.0f, false},
    {"HEAVY TRUCK",    7.60f, 2.40f, 3.10f, 7500, 26.0f, 8.0f, 1.4f, 7.0f, 0.52f, 6, 34.0f, false},
    {"BULLDOZER",      5.40f, 2.90f, 2.80f, 14000, 12.5f, 6.5f, 1.5f, 12.0f, 0.42f, 0, 120.0f, false},
    {"FORKLIFT",       3.60f, 1.70f, 2.30f, 3200, 15.0f, 7.0f, 2.4f, 8.0f, 0.30f, 4, 18.0f, false},
    {"ARMORED CAR",    6.00f, 2.40f, 2.90f, 9000, 28.0f, 9.5f, 1.6f, 8.0f, 0.50f, 6, 70.0f, false},
    {"PATROL BOAT",    5.00f, 1.90f, 1.60f, 1800, 24.0f, 9.0f, 2.6f, 3.0f, 0.0f, 0, 4.0f, true},
    {"EXCAVATOR",      5.80f, 2.90f, 3.20f, 15000, 10.0f, 5.5f, 1.2f, 11.0f, 0.44f, 0, 90.0f, false},
};

const VehicleDef& vehicleDef(int kind) { return g_defs[iclamp(kind, 0, VK_COUNT - 1)]; }

void VehicleManager::init(World* w, Physics* p) {
    world = w;
    physics = p;
    vehicles.clear();
}

void VehicleManager::clear() {
    for (auto& v : vehicles)
        if (v.engineLoop >= 0) audio::stopLoop(v.engineLoop);
    vehicles.clear();
}

int VehicleManager::spawn(VehicleKind kind, const vec3& pos, float yaw) {
    if ((int)vehicles.size() >= maxVehicles) return -1;
    Vehicle v;
    v.kind = kind;
    v.pos = pos;
    v.yaw = yaw;
    v.lastPos = pos;
    v.health = 100;
    const VehicleDef& d = g_defs[kind];
    // drop the vehicle onto the surface
    float gy = world ? world->groundHeightM(pos.x, pos.z) : 0.0f;
    v.pos.y = std::max(gy, pos.y) + d.wheelRadius + 0.05f;
    vehicles.push_back(v);
    return (int)vehicles.size() - 1;
}

Vehicle* VehicleManager::nearest(const vec3& p, float maxDist, int* indexOut) {
    Vehicle* best = nullptr;
    float bestD = maxDist;
    for (auto& v : vehicles) {
        float d = (v.pos - p).len();
        if (d < bestD) { bestD = d; best = &v; if (indexOut) *indexOut = (int)(&v - &vehicles[0]); }
    }
    return best;
}

bool VehicleManager::tryEnter(const vec3& playerPos, int& outIndex) {
    int idx = -1;
    Vehicle* v = nearest(playerPos, 3.6f, &idx);
    if (!v || v->destroyed) return false;
    outIndex = idx;
    return true;
}

void VehicleManager::exitVehicle(Vehicle& v) {
    v.occupied = false;
    if (v.engineLoop >= 0) { audio::stopLoop(v.engineLoop); v.engineLoop = -1; }
}

void VehicleManager::update(float dt, int drivenIndex) {
    for (int i = 0; i < (int)vehicles.size(); i++) {
        Vehicle& v = vehicles[i];
        const VehicleDef& d = g_defs[v.kind];
        if (v.destroyed) continue;
        v.crashCooldown -= dt;
        v.carveCooldown -= dt;
        if (i == drivenIndex) {
            // active driving handled in drive() by the game
        } else {
            v.speed *= (1.0f - 2.0f * dt);
            v.steer *= (1.0f - 3.0f * dt);
            if (v.engineLoop >= 0 && v.speed < 0.1f) {
                audio::updateLoop(v.engineLoop, v.pos, 0.0f, 1.0f);
            }
        }
        // gravity + ground contact via multiple probes
        vec3 fwd(std::sin(v.yaw), 0, -std::cos(v.yaw));
        vec3 right(std::cos(v.yaw), 0, std::sin(v.yaw));
        float halfLen = d.length * 0.42f, halfWid = d.width * 0.42f;
        float maxGround = -100;
        float sumGround = 0;
        int nProbes = 0;
        vec3 probeOffsets[6] = {{halfLen, 0, halfWid}, {halfLen, 0, -halfWid}, {-halfLen, 0, halfWid},
                                {-halfLen, 0, -halfWid}, {0, 0, 0}, {halfLen * 0.2f, 0, 0}};
        for (int p = 0; p < 6; p++) {
            vec3 wp = v.pos + fwd * probeOffsets[p].x + right * probeOffsets[p].z;
            float gy = world->groundHeightM(wp.x, wp.z);
            if (gy > 0.01f) {
                maxGround = std::max(maxGround, gy);
                sumGround += gy;
                nProbes++;
            }
        }
        float targetY;
        if (d.boat) {
            float water = -999;
            // find water surface: highest water voxel at the position
            for (int yy = world->sy - 1; yy >= 0; yy--) {
                if (world->at((int)(v.pos.x / VOX), yy, (int)(v.pos.z / VOX)) == M_WATER) { water = (yy + 1) * VOX; break; }
            }
            targetY = water > -900 ? water - d.wheelRadius * 0.4f : (maxGround > -90 ? maxGround : 0.0f) + 0.4f;
            v.speed *= (1.0f - 0.55f * dt);
        } else {
            targetY = (nProbes > 0 ? sumGround / nProbes : v.pos.y) + d.wheelRadius + 0.02f;
        }
        // suspension spring
        float dy = targetY - v.pos.y;
        if (!d.boat) {
            v.vel.y += dy * 46.0f * dt - v.vel.y * 6.5f * dt;
        } else {
            v.vel.y += dy * 18.0f * dt - v.vel.y * 5.0f * dt;
        }
        v.pos.y += v.vel.y * dt;
        if (dy < -0.05f) v.vel.y = std::max(v.vel.y, -30.0f);
        // terrain following pitch/roll
        float frontG = world->groundHeightM((v.pos + fwd * halfLen).x, (v.pos + fwd * halfLen).z);
        float backG = world->groundHeightM((v.pos - fwd * halfLen).x, (v.pos - fwd * halfLen).z);
        float rightG = world->groundHeightM((v.pos + right * halfWid).x, (v.pos + right * halfWid).z);
        float leftG = world->groundHeightM((v.pos - right * halfWid).x, (v.pos - right * halfWid).z);
        float targetPitch = -std::atan2(frontG - backG, d.length * 0.84f);
        float targetRoll = std::atan2(rightG - leftG, d.width * 0.84f);
        if (d.boat) { targetPitch *= 0.3f; targetRoll *= 0.3f; }
        v.pitch = lerpf(v.pitch, targetPitch, dt * 7.0f);
        v.roll = lerpf(v.roll, targetRoll, dt * 7.0f);

        // horizontal motion
        vec3 horiz(v.vel.x, 0, v.vel.z);
        v.pos.x += horiz.x * dt;
        v.pos.z += horiz.z * dt;
        float linSpeed = horiz.len();
        v.odo += linSpeed * dt;
        if (i == drivenIndex) {
            v.speed = linSpeed;
        } else if (linSpeed > 0.2f) {
            // rolling resistance when not driven
            v.vel.x *= (1.0f - 1.6f * dt);
            v.vel.z *= (1.0f - 1.6f * dt);
        }
        // collision with the voxel world: sample the body volume
        bool crashed = false;
        vec3 hitPos;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = 0; sy < 2; sy++)
                for (int sz = -1; sz <= 1; sz += 2) {
                    vec3 off = fwd * (halfLen * sx * 0.85f) + right * (halfWid * sz * 0.95f) +
                               vec3(0, d.wheelRadius + 0.25f + sy * (d.height * 0.55f), 0);
                    vec3 wp = v.pos + off;
                    if (world->solidAtM(wp)) {
                        crashed = true;
                        hitPos = wp;
                        break;
                    }
                }
                (void)0;
        if (world->solidAtM(v.pos + vec3(0, d.height * 0.5f, 0))) {
            crashed = true;
            hitPos = v.pos;
        }
        if (crashed) {
            vec3 toC = hitPos - v.pos;
            toC.y = 0;
            if (toC.len2() > 0.0001f) {
                vec3 n = toC.norm();
                v.pos -= n * 0.06f;
                float vn = dot(v.vel, n);
                if (vn > 0) {
                    v.vel -= n * vn * 1.5f;
                    float dmg = vn * (v.kind == VK_BULLDOZER || v.kind == VK_EXCAVATOR ? 0.5f : 2.2f);
                    v.health -= std::max(0.0f, dmg - 2.0f);
                    if (v.crashCooldown <= 0 && vn > 3.0f) {
                        v.crashCooldown = 0.3f;
                        audio::play(SFX_IMPACT_VEHICLE, hitPos, std::min(0.9f, vn * 0.07f), 1.0f);
                        g_fx->impact(world->matAtM(hitPos), hitPos, -n, 0.6f, i * 13);
                        if (g_playerRef) g_playerRef->addShake(std::min(0.5f, vn * 0.02f));
                    }
                }
            }
            // heavy vehicles demolish what they hit
            if (d.carveStrength > 10.0f && v.carveCooldown <= 0 && v.speed > 2.0f) {
                v.carveCooldown = 0.11f;
                vec3 hitPoint = hitPos;
                float radius = 0.45f + d.carveStrength * 0.006f;
                float power = d.carveStrength * (0.6f + std::min(1.4f, v.speed / d.topSpeed)) * 1.6f;
                carveWorld(hitPoint, radius, power, true, false);
                g_fx->dustBurst(hitPoint, 3, 0.4f, 1.4f, vec3(0.62f, 0.58f, 0.5f));
                if (hash32((uint32_t)(g_time * 100)) % 4 == 0)
                    audio::play(SFX_CONCRETE_BREAK, hitPoint, 0.55f, 0.85f);
            }
        }
        // collide with dynamic bodies
        if (physics) {
            for (auto& b : physics->bodies) {
                if (!b.active) continue;
                vec3 d2 = b.pos - v.pos;
                if (std::fabs(d2.y) > d.height) continue;
                if (d2.x * d2.x + d2.z * d2.z < (d.length * 0.5f) * (d.length * 0.5f)) {
                    vec3 push = d2;
                    push.y = 0;
                    if (push.len2() > 0.01f) {
                        float f = linSpeed * 0.9f * (d.mass / 1400.0f);
                        b.vel += push.norm() * std::min(14.0f, f) + vec3(0, 1.2f, 0);
                        b.angVel += vec3(0, 3, 0);
                        b.sleeping = false;
                    }
                }
            }
        }
        // engine audio
        if (i == drivenIndex) {
            float rpm = std::fabs(v.speed) / std::max(1.0f, d.topSpeed);
            if (v.engineLoop < 0) v.engineLoop = audio::playLoop(SFX_VEHICLE_LOOP, v.pos, 0.5f, 0.8f);
            audio::updateLoop(v.engineLoop, v.pos, 0.35f + rpm * 0.35f, 0.7f + rpm * 1.1f);
            v.engineRPM = rpm;
            // dust and exhaust
            v.dustTimer -= dt;
            if (v.dustTimer <= 0 && (rpm > 0.35f) && !d.boat) {
                v.dustTimer = 0.11f;
                vec3 back = v.pos - fwd * (d.length * 0.5f);
                float gy = world->groundHeightM(back.x, back.z);
                if (back.y - gy < 1.0f)
                    g_fx->smokePlume(vec3(back.x, gy + 0.12f, back.z), vec3((hash3f((int)g_time,1,1)-0.5f)*1.5f, 0.6f, (hash3f((int)g_time,2,2)-0.5f)*1.5f),
                                     0.20f + rpm * 0.18f, 1.1f, packColor(0.62f, 0.57f, 0.48f), 1);
            }
            // exhaust smoke
            if (rpm > 0.1f && hash32((uint32_t)(g_time * 1000)) % 5 == 0) {
                vec3 ex = v.pos - fwd * (d.length * 0.45f) + vec3(0, d.height * 0.35f, 0);
                g_fx->smokePlume(ex, vec3(0, 0.8f, 0), 0.06f, 0.6f, packColor(0.3f, 0.3f, 0.3f), 0);
            }
        }
        v.lastPos = v.pos;
        if (v.health <= 0 && !v.destroyed) {
            v.destroyed = true;
            g_fx->explosion(v.pos + vec3(0, 0.6f, 0), 3.0f, 3.0f, false);
            audio::play(SFX_EXPLOSION_SMALL, v.pos, 1.0f, 0.9f);
            v.vel = vec3(0);
        }
    }
}

void VehicleManager::drive(Vehicle& v, float dt, InputState& in) {
    const VehicleDef& d = g_defs[v.kind];
    float throttle = 0, steerInput = 0;
    if (in.isDown(K_W)) throttle += 1;
    if (in.isDown(K_S)) throttle -= 1;
    if (in.isDown(K_A)) steerInput -= 1;
    if (in.isDown(K_D)) steerInput += 1;
    bool handbrake = in.isDown(K_SPACE);
    v.steer = lerpf(v.steer, steerInput, dt * 6.0f);
    float speed = v.speed;
    vec3 fwd(std::sin(v.yaw), 0, -std::cos(v.yaw));
    float maxSpeed = d.topSpeed * (handbrake ? 0.1f : 1.0f);
    if (throttle > 0) {
        float accel = d.accel * (1.0f - std::min(0.85f, speed / maxSpeed));
        v.vel += fwd * accel * dt;
    } else if (throttle < 0) {
        if (speed > 1.0f) {
            v.vel -= fwd * (d.accel * 1.4f) * dt;   // braking
        } else {
            v.vel -= fwd * (d.accel * 0.55f) * dt;  // reverse
        }
    }
    // friction / drag
    vec3 horiz(v.vel.x, 0, v.vel.z);
    float hs = horiz.len();
    float drag = d.boat ? 0.55f : (handbrake ? 3.2f : 0.35f);
    if (hs > 0.01f) {
        vec3 dir = horiz / hs;
        float newSpeed = hs - (drag * dt + hs * 0.02f);
        if (newSpeed < 0) newSpeed = 0;
        // lateral grip: kill sideways velocity
        vec3 right(std::cos(v.yaw), 0, std::sin(v.yaw));
        float lateral = dot(v.vel, right);
        v.vel -= right * lateral * std::min(1.0f, d.grip * dt);
        v.vel = dir * newSpeed + vec3(0, v.vel.y, 0);
        // clamp to top speed
        vec3 hv(v.vel.x, 0, v.vel.z);
        float s2 = hv.len();
        if (s2 > maxSpeed) {
            v.vel.x *= maxSpeed / s2;
            v.vel.z *= maxSpeed / s2;
        }
    }
    // steering scales with speed, reverses when backing up
    float speedFactor = clampf(std::fabs(speed) / 6.0f, 0.0f, 1.0f);
    float dirSign = speed < -0.5f ? -1.0f : 1.0f;
    v.yaw += v.steer * d.turnRate * dt * speedFactor * dirSign;
    v.wheelSpin += speed * dt / std::max(0.1f, d.wheelRadius);
    // forklift / excavator hydraulics
    if (v.kind == VK_FORKLIFT) {
        if (in.isDown(K_R)) v.forkLift = std::min(2.4f, v.forkLift + dt * 1.6f);
        if (in.isDown(K_F)) v.forkLift = std::max(0.0f, v.forkLift - dt * 1.6f);
    }
    if (v.kind == VK_EXCAVATOR) {
        if (in.isDown(K_R)) v.armAngle = std::min(1.4f, v.armAngle + dt * 0.7f);
        if (in.isDown(K_F)) v.armAngle = std::max(-0.6f, v.armAngle - dt * 0.7f);
        if (in.isDown(K_LBUTTON)) {
            // bucket digs
            vec3 bucket = v.pos + fwd * (2.6f + v.armExtend) + vec3(0, 0.4f + std::sin(v.armAngle) * 2.2f, 0);
            if (v.carveCooldown <= 0) {
                v.carveCooldown = 0.14f;
                carveWorld(bucket, 0.62f, 40.0f, true, false);
                g_fx->dustBurst(bucket, 4, 0.5f, 1.6f, vec3(0.4f, 0.33f, 0.22f));
                audio::play(SFX_HAMMER_CONCRETE, bucket, 0.5f, 0.7f);
            }
        }
    }
}

void VehicleManager::addLocalBox(std::vector<BoxInstance>& out, const mat4& base, vec3 pos, vec3 size, vec3 color,
                                 float emis, float metal, float rough) {
    BoxInstance b;
    b.model = base * matTranslate(pos) * matScale(size);
    b.color = color;
    b.emis = color * emis;
    b.metal = metal;
    b.rough = rough;
    out.push_back(b);
}

void VehicleManager::buildModel(Vehicle& v, std::vector<BoxInstance>& out) {
    const VehicleDef& d = g_defs[v.kind];
    mat4 base = matTranslate(v.pos) * matRotY(v.yaw) * matRotX(v.pitch) * matRotZ(v.roll);
    float L = d.length, W = d.width, H = d.height;
    vec3 bodyCol, dark(0.12f, 0.13f, 0.14f), glass(0.35f, 0.45f, 0.52f);
    float lift = d.wheelRadius;
    switch (v.kind) {
        case VK_SEDAN: bodyCol = vec3(0.55f, 0.12f, 0.11f); break;
        case VK_PICKUP: bodyCol = vec3(0.14f, 0.28f, 0.45f); break;
        case VK_TRUCK: bodyCol = vec3(0.72f, 0.66f, 0.20f); break;
        case VK_BULLDOZER: bodyCol = vec3(0.85f, 0.62f, 0.08f); break;
        case VK_FORKLIFT: bodyCol = vec3(0.80f, 0.55f, 0.10f); break;
        case VK_ARMORED: bodyCol = vec3(0.20f, 0.23f, 0.20f); break;
        case VK_BOAT: bodyCol = vec3(0.72f, 0.74f, 0.76f); break;
        default: bodyCol = vec3(0.80f, 0.58f, 0.10f); break;
    }
    if (v.kind == VK_SEDAN || v.kind == VK_PICKUP) {
        addLocalBox(out, base, vec3(0, lift + H * 0.18f, 0), vec3(W, H * 0.42f, L * 0.94f), bodyCol, 0, 0.55f, 0.35f);
        addLocalBox(out, base, vec3(0, lift + H * 0.52f, -L * 0.03f), vec3(W * 0.88f, H * 0.38f, L * 0.48f), bodyCol * 0.85f, 0, 0.4f, 0.3f);
        // windows
        addLocalBox(out, base, vec3(0, lift + H * 0.55f, -L * 0.255f), vec3(W * 0.80f, H * 0.27f, 0.04f), glass, 0, 0.1f, 0.08f);
        addLocalBox(out, base, vec3(0, lift + H * 0.55f, L * 0.21f), vec3(W * 0.80f, H * 0.27f, 0.04f), glass, 0, 0.1f, 0.08f);
        addLocalBox(out, base, vec3(W * 0.44f, lift + H * 0.52f, -L * 0.02f), vec3(0.05f, H * 0.30f, L * 0.42f), glass, 0, 0.1f, 0.08f);
        addLocalBox(out, base, vec3(-W * 0.44f, lift + H * 0.52f, -L * 0.02f), vec3(0.05f, H * 0.30f, L * 0.42f), glass, 0, 0.1f, 0.08f);
        if (v.kind == VK_PICKUP) {
            addLocalBox(out, base, vec3(0, lift + H * 0.30f, L * 0.28f), vec3(W * 0.92f, H * 0.30f, L * 0.34f), bodyCol * 0.8f, 0, 0.5f, 0.4f);
        }
        // bumpers
        addLocalBox(out, base, vec3(0, lift + H * 0.16f, L * 0.47f), vec3(W * 0.95f, H * 0.16f, 0.12f), dark, 0, 0.8f, 0.4f);
        addLocalBox(out, base, vec3(0, lift + H * 0.16f, -L * 0.47f), vec3(W * 0.95f, H * 0.16f, 0.12f), dark, 0, 0.8f, 0.4f);
    } else if (v.kind == VK_TRUCK || v.kind == VK_ARMORED) {
        addLocalBox(out, base, vec3(0, lift + H * 0.16f, 0), vec3(W, H * 0.30f, L), bodyCol * 0.6f, 0, 0.6f, 0.5f);
        addLocalBox(out, base, vec3(0, lift + H * 0.40f, -L * 0.30f), vec3(W * 0.96f, H * 0.52f, L * 0.36f), bodyCol, 0, 0.5f, 0.4f);
        addLocalBox(out, base, vec3(0, lift + H * 0.62f, -L * 0.38f), vec3(W * 0.86f, H * 0.24f, 0.06f), glass, 0, 0.1f, 0.1f);
        if (v.kind == VK_TRUCK) {
            addLocalBox(out, base, vec3(0, lift + H * 0.45f, L * 0.14f), vec3(W * 0.98f, H * 0.62f, L * 0.52f), vec3(0.62f, 0.60f, 0.56f), 0, 0.2f, 0.75f);
        } else {
            addLocalBox(out, base, vec3(0, lift + H * 0.46f, L * 0.12f), vec3(W * 0.96f, H * 0.44f, L * 0.55f), bodyCol * 0.9f, 0, 0.7f, 0.45f);
            addLocalBox(out, base, vec3(0, lift + H * 0.82f, L * 0.05f), vec3(0.22f, 0.22f, L * 0.3f), dark, 0, 0.9f, 0.3f);
        }
        addLocalBox(out, base, vec3(0, lift + H * 0.14f, -L * 0.49f), vec3(W * 1.0f, H * 0.18f, 0.14f), dark, 0, 0.85f, 0.4f);
    } else if (v.kind == VK_BULLDOZER || v.kind == VK_EXCAVATOR) {
        addLocalBox(out, base, vec3(0, lift + H * 0.32f, 0), vec3(W * 0.72f, H * 0.55f, L * 0.66f), bodyCol, 0, 0.6f, 0.45f);
        addLocalBox(out, base, vec3(0, lift + H * 0.70f, -L * 0.14f), vec3(W * 0.52f, H * 0.42f, L * 0.34f), bodyCol * 0.9f, 0, 0.4f, 0.35f);
        addLocalBox(out, base, vec3(0, lift + H * 0.76f, -L * 0.30f), vec3(W * 0.44f, H * 0.26f, 0.05f), glass, 0, 0.1f, 0.08f);
        // tracks
        addLocalBox(out, base, vec3(W * 0.44f, lift + H * 0.16f, 0), vec3(0.42f, H * 0.32f, L * 0.86f), dark, 0, 0.6f, 0.2f);
        addLocalBox(out, base, vec3(-W * 0.44f, lift + H * 0.16f, 0), vec3(0.42f, H * 0.32f, L * 0.86f), dark, 0, 0.6f, 0.2f);
        if (v.kind == VK_BULLDOZER) {
            addLocalBox(out, base, vec3(0, lift + H * 0.24f, L * 0.62f), vec3(W * 1.1f, H * 0.44f, 0.28f), vec3(0.55f, 0.35f, 0.12f), 0, 0.75f, 0.55f);
            addLocalBox(out, base, vec3(W * 0.30f, lift + H * 0.34f, L * 0.36f), vec3(0.18f, 0.18f, L * 0.5f), dark, 0, 0.9f, 0.35f);
            addLocalBox(out, base, vec3(-W * 0.30f, lift + H * 0.34f, L * 0.36f), vec3(0.18f, 0.18f, L * 0.5f), dark, 0, 0.9f, 0.35f);
        } else {
            // excavator arm
            float a = v.armAngle;
            vec3 shoulder(0, lift + H * 0.62f, L * 0.22f);
            vec3 elbow = shoulder + vec3(0, std::sin(a) * 2.0f, std::cos(a) * 2.0f);
            mat4 armBase = base * matTranslate((shoulder + elbow) * 0.5f) * matRotX(-a);
            addLocalBox(out, armBase, vec3(0), vec3(0.34f, 0.34f, 2.2f), vec3(0.85f, 0.60f, 0.10f), 0, 0.6f, 0.5f);
            vec3 tip = elbow + vec3(0, -0.6f, 1.4f);
            addLocalBox(out, base, elbow + vec3(0, -0.3f, 0.7f), vec3(0.3f, 0.5f, 1.6f), vec3(0.80f, 0.56f, 0.10f), 0, 0.6f, 0.5f);
            addLocalBox(out, base, tip, vec3(0.9f, 0.7f, 0.9f), vec3(0.42f, 0.42f, 0.44f), 0, 0.85f, 0.4f);
        }
    } else if (v.kind == VK_FORKLIFT) {
        addLocalBox(out, base, vec3(0, lift + H * 0.26f, 0), vec3(W * 0.8f, H * 0.44f, L * 0.8f), bodyCol, 0, 0.5f, 0.45f);
        addLocalBox(out, base, vec3(0, lift + H * 0.70f, -L * 0.12f), vec3(W * 0.62f, H * 0.5f, L * 0.4f), dark, 0, 0.7f, 0.4f);
        addLocalBox(out, base, vec3(0, lift + H * 0.80f, -L * 0.30f), vec3(W * 0.5f, H * 0.22f, 0.05f), glass, 0, 0.1f, 0.08f);
        // mast and forks
        addLocalBox(out, base, vec3(0, lift + H * 0.62f, L * 0.42f), vec3(0.14f, H * 0.9f, 0.14f), dark, 0, 0.9f, 0.3f);
        addLocalBox(out, base, vec3(0.35f, lift + H * 0.62f, L * 0.42f), vec3(0.14f, H * 0.9f, 0.14f), dark, 0, 0.9f, 0.3f);
        float fy = lift + H * 0.22f + v.forkLift;
        addLocalBox(out, base, vec3(0.35f, fy, L * 0.70f), vec3(0.12f, 0.08f, 0.9f), vec3(0.30f, 0.31f, 0.33f), 0, 0.9f, 0.35f);
        addLocalBox(out, base, vec3(-0.35f, fy, L * 0.70f), vec3(0.12f, 0.08f, 0.9f), vec3(0.30f, 0.31f, 0.33f), 0, 0.9f, 0.35f);
    } else if (v.kind == VK_BOAT) {
        addLocalBox(out, base, vec3(0, lift + H * 0.18f, 0), vec3(W * 0.9f, H * 0.5f, L * 0.9f), bodyCol, 0, 0.2f, 0.4f);
        addLocalBox(out, base, vec3(0, lift + H * 0.5f, -L * 0.22f), vec3(W * 0.6f, H * 0.4f, L * 0.3f), vec3(0.30f, 0.45f, 0.55f), 0, 0.1f, 0.1f);
        addLocalBox(out, base, vec3(0, lift + H * 0.22f, L * 0.42f), vec3(W * 0.6f, H * 0.3f, 0.5f), vec3(0.62f, 0.63f, 0.64f), 0, 0.7f, 0.4f);
        addLocalBox(out, base, vec3(0, lift + H * 0.6f, -L * 0.42f), vec3(0.1f, 0.6f, 0.1f), dark, 0, 0.9f, 0.3f);
    }
    // wheels
    if (d.wheelCount > 0) {
        float wx = W * 0.52f;
        float wz = L * 0.32f;
        vec3 wp[4] = {{wx, d.wheelRadius, wz}, {-wx, d.wheelRadius, wz}, {wx, d.wheelRadius, -wz}, {-wx, d.wheelRadius, -wz}};
        for (int i = 0; i < d.wheelCount && i < 4; i++) {
            float steerAngle = (i < 2) ? v.steer * 0.5f : 0.0f;
            mat4 wm = base * matTranslate(wp[i]) * matRotY(steerAngle) * matRotX(v.wheelSpin);
            mat4 scaled = wm * matScale(vec3(0.22f, d.wheelRadius * 2.0f, d.wheelRadius * 2.0f)) * matRotZ(PI * 0.5f);
            BoxInstance b;
            b.model = scaled;
            b.color = vec3(0.09f, 0.09f, 0.10f);
            b.rough = 0.95f;
            b.metal = 0.0f;
            out.push_back(b);
            BoxInstance hub;
            hub.model = wm * matScale(vec3(0.24f, d.wheelRadius * 0.9f, d.wheelRadius * 0.9f)) * matRotZ(PI * 0.5f);
            hub.color = vec3(0.55f, 0.56f, 0.58f);
            hub.metal = 0.8f;
            hub.rough = 0.35f;
            out.push_back(hub);
        }
    }
    // headlights
    if (v.lightsOn) {
        addLocalBox(out, base, vec3(W * 0.30f, lift + H * 0.28f, L * 0.485f), vec3(0.22f, 0.14f, 0.06f),
                    vec3(1.0f, 0.95f, 0.8f), 1.6f * v.headlights, 0, 0.2f);
        addLocalBox(out, base, vec3(-W * 0.30f, lift + H * 0.28f, L * 0.485f), vec3(0.22f, 0.14f, 0.06f),
                    vec3(1.0f, 0.95f, 0.8f), 1.6f * v.headlights, 0, 0.2f);
        addLocalBox(out, base, vec3(W * 0.32f, lift + H * 0.28f, -L * 0.485f), vec3(0.18f, 0.12f, 0.05f),
                    vec3(0.9f, 0.12f, 0.08f), 0.9f * v.headlights, 0, 0.3f);
        addLocalBox(out, base, vec3(-W * 0.32f, lift + H * 0.28f, -L * 0.485f), vec3(0.18f, 0.12f, 0.05f),
                    vec3(0.9f, 0.12f, 0.08f), 0.9f * v.headlights, 0, 0.3f);
    }
    (void)v;
}

void VehicleManager::draw(IRenderer& r, const vec3& camPos) {
    static std::vector<BoxInstance> boxes;
    boxes.clear();
    for (auto& v : vehicles) {
        float dist = (v.pos - camPos).len();
        if (dist > 220.0f) continue;
        buildModel(v, boxes);
    }
    if (!boxes.empty()) r.drawBoxes(boxes.data(), (int)boxes.size());
}
