#include "game/security.h"
#include "game/globals.h"
#include "game/player.h"
#include "game/fx.h"
#include <cmath>
#include <algorithm>

#ifndef PI
#define PI 3.14159265358979f
#endif

void SecuritySystem::init(World* w, Physics* p) {
    world = w;
    physics = p;
    clear();
}

void SecuritySystem::clear() {
    cameras.clear();
    turrets.clear();
    alarm = false;
    alarmTime = 0;
    detections = 0;
    shotsFired = 0;
    alarmBlink = 0;
}

void SecuritySystem::addCamera(const vec3& pos, float yaw) {
    SecurityCamera c;
    c.pos = pos;
    c.baseYaw = yaw;
    c.yaw = yaw;
    c.phase = hash3f((int)(pos.x * 7), (int)(pos.y * 3), (int)(pos.z * 5)) * 6.2f;
    c.health = 40;
    cameras.push_back(c);
}

void SecuritySystem::addTurret(const vec3& pos, float yaw) {
    SecurityTurret t;
    t.pos = pos;
    t.yaw = yaw;
    t.targetYaw = yaw;
    turrets.push_back(t);
}

void SecuritySystem::placeForTargets(const std::vector<std::pair<vec3, vec3>>& boxes) {
    int n = 0;
    for (auto& b : boxes) {
        if (n >= 6) break;
        vec3 mn = b.first, mx = b.second;
        vec3 c = (mn + mx) * 0.5f;
        float radius = (mx - mn).len() * 0.5f;
        // camera high on a corner, looking along the face
        vec3 camPos(mx.x - 0.6f, std::min(mx.y - 1.0f, mn.y + 5.5f), mn.z + 0.6f);
        if (world) camPos.y = std::min(camPos.y, world->groundHeightM(camPos.x, camPos.z) + 6.0f);
        addCamera(camPos, -PI * 0.75f);
        // sentry turret on the ground near the opposite corner
        if (n % 2 == 0) {
            vec3 tpos(mn.x + 1.2f, 0, mx.z - 1.2f);
            if (world) tpos.y = world->groundHeightM(tpos.x, tpos.z) + 0.55f;
            addTurret(tpos, PI * 0.4f);
        }
        (void)radius;
        n++;
    }
}

void SecuritySystem::triggerAlarm(const vec3& pos) {
    if (alarm) return;
    alarm = true;
    alarmTime = 0;
    lastAlarmPos = pos;
    for (auto& t : turrets) t.hostile = true;
    audio::play(SFX_ALARM, pos, 0.85f, 1.0f);
}

void SecuritySystem::resetAlarm() {
    alarm = false;
    alarmTime = 0;
    for (auto& t : turrets) t.hostile = false;
    for (auto& c : cameras) c.alert = false;
}

bool SecuritySystem::anyAlert() const {
    for (auto& c : cameras)
        if (!c.destroyed && c.alert) return true;
    return alarm;
}

int SecuritySystem::aliveCameras() const {
    int n = 0;
    for (auto& c : cameras) if (!c.destroyed) n++;
    return n;
}
int SecuritySystem::aliveTurrets() const {
    int n = 0;
    for (auto& t : turrets) if (!t.destroyed) n++;
    return n;
}

static bool lineOfSight(World* w, const vec3& a, const vec3& b) {
    if (!w) return false;
    vec3 d = b - a;
    float len = d.len();
    if (len < 0.1f) return true;
    RayHit h = w->raycast(a, d / len, len - 0.2f);
    return !h.hit;
}

void SecuritySystem::update(float dt, const vec3& playerPos, bool playerVisible, Player* player) {
    alarmBlink += dt;
    if (alarm) alarmTime += dt;
    vec3 eye = playerPos + vec3(0, 1.55f, 0);
    // ---- cameras sweep and detect
    for (auto& c : cameras) {
        if (c.destroyed) {
            c.sparkTimer -= dt;
            if (c.sparkTimer <= 0 && g_fx && (c.pos - playerPos).len() < 40) {
                c.sparkTimer = 1.2f + hash3f((int)c.pos.x, 3, (int)c.pos.z) * 2.0f;
                g_fx->sparks(c.pos, vec3(0, 1, 0), 3, 1.4f);
            }
            continue;
        }
        c.phase += dt * c.sweepSpeed;
        c.yaw = c.baseYaw + std::sin(c.phase) * c.sweep;
        vec3 look(std::sin(c.yaw), -0.18f, -std::cos(c.yaw));
        look = look.norm();
        vec3 toPlayer = eye - c.pos;
        float dist = toPlayer.len();
        bool sees = false;
        if (playerVisible && dist > 0.3f && dist < c.viewRange) {
            vec3 dir = toPlayer / dist;
            float cosang = dot(dir, look);
            if (cosang > std::cos(c.fov)) {
                if (lineOfSight(world, c.pos, eye)) sees = true;
            }
        }
        if (sees) {
            c.suspicion = std::min(1.0f, c.suspicion + dt * (c.alert ? 0.25f : 0.75f));
            if (c.suspicion >= 1.0f && !c.alert) {
                c.alert = true;
                detections++;
                triggerAlarm(c.pos);
                if (g_fx) g_fx->sparks(c.pos + look * 0.4f, look, 4, 2.0f);
            }
        } else {
            c.suspicion = std::max(0.0f, c.suspicion - dt * 0.35f);
            if (c.suspicion <= 0.0f) c.alert = false;
        }
        if (c.alert && hash32((uint32_t)(c.pos.x * 100 + c.pos.z * 7)) % 3 == 0) {
            // blinking alert lamp
            if (g_fx && std::fmod(alarmBlink, 0.8f) < 0.05f)
                g_fx->sparks(c.pos + look * 0.5f, look, 1, 0.8f);
        }
    }
    // ---- turrets
    for (auto& t : turrets) {
        if (t.destroyed) {
            t.sparkTimer -= dt;
            if (t.sparkTimer <= 0 && g_fx && (t.pos - playerPos).len() < 40) {
                t.sparkTimer = 1.5f + hash3f((int)t.pos.x, 5, (int)t.pos.z) * 2.0f;
                g_fx->smokePlume(t.pos + vec3(0, 0.5f, 0), vec3(0, 0.5f, 0), 0.25f, 2.0f,
                                 packColor(0.18f, 0.18f, 0.18f), 7);
            }
            continue;
        }
        t.fireCooldown -= dt;
        t.muzzleFlash = std::max(0.0f, t.muzzleFlash - dt * 6.0f);
        vec3 toPlayer = eye - t.pos;
        float dist = toPlayer.len();
        bool canSee = t.hostile && playerVisible && dist < 26.0f && lineOfSight(world, t.pos + vec3(0, 0.35f, 0), eye);
        if (canSee) {
            float want = std::atan2(toPlayer.x, -toPlayer.z);
            // shortest angular path
            float diff = want - t.yaw;
            while (diff > PI) diff -= 2 * PI;
            while (diff < -PI) diff += 2 * PI;
            t.yaw += clampf(diff, -dt * 2.4f, dt * 2.4f);
            t.pitch = clampf(std::atan2(toPlayer.y, std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z)),
                             -0.5f, 0.4f);
            if (t.fireCooldown <= 0 && std::fabs(diff) < 0.20f) {
                t.fireCooldown = 0.55f;
                t.muzzleFlash = 1.0f;
                shotsFired++;
                lastShotTime = g_time;
                vec3 muzzle = t.pos + vec3(std::sin(t.yaw) * 0.5f, 0.35f, -std::cos(t.yaw) * 0.5f);
                vec3 aim = (eye - muzzle).norm();
                if (g_fx) {
                    g_fx->sparks(muzzle, aim, 5, 5.0f);
                    g_fx->smokePlume(muzzle + aim * 0.2f, aim * 2.0f, 0.10f, 0.5f, packColor(0.7f, 0.7f, 0.7f), 0);
                    LightInstance l;
                    l.pos = muzzle;
                    l.color = vec3(1.0f, 0.8f, 0.3f);
                    l.radius = 5.0f;
                    l.intensity = 0.8f;
                    g_fx->lights.push_back(l);
                }
                audio::play(SFX_ALARM, muzzle, 0.35f, 1.8f);
                if (player) {
                    // hitscan with slight spread, real damage
                    float miss = hash3f((int)(g_time * 100), (int)t.pos.x, (int)t.pos.z);
                    if (miss > 0.28f) {
                        player->damage(7.5f, g_time);
                        player->addShake(0.18f);
                    } else if (g_fx) {
                        g_fx->impact(M_CONCRETE, eye + vec3((miss - 0.5f) * 2.0f, 0.3f, 0), -aim, 0.4f, 5);
                    }
                }
            }
        } else {
            t.pitch *= (1.0f - dt * 2.0f);
        }
    }
}

bool SecuritySystem::damageArea(const vec3& center, float radius, float power) {
    bool any = false;
    for (auto& c : cameras) {
        if (c.destroyed) continue;
        float d = (c.pos - center).len();
        if (d > radius) continue;
        c.health -= power * (1.0f - d / radius);
        if (c.health <= 0) {
            c.destroyed = true;
            any = true;
            if (g_fx) {
                g_fx->sparks(c.pos, vec3(0, 1, 0), 10, 4.0f);
                g_fx->smokePlume(c.pos, vec3(0, 1.0f, 0), 0.3f, 2.0f, packColor(0.2f, 0.2f, 0.2f), 7);
            }
            audio::play(SFX_METAL_BREAK, c.pos, 0.6f, 1.2f);
        } else any = true;
    }
    for (auto& t : turrets) {
        if (t.destroyed) continue;
        float d = (t.pos - center).len();
        if (d > radius) continue;
        t.health -= power * 1.4f * (1.0f - d / radius);
        any = true;
        if (t.health <= 0) {
            t.destroyed = true;
            t.hostile = false;
            if (g_fx) {
                g_fx->explosion(t.pos + vec3(0, 0.3f, 0), 1.4f, 0.5f, false);
                g_fx->sparks(t.pos, vec3(0, 1, 0), 14, 5.0f);
            }
            audio::play(SFX_EXPLOSION_SMALL, t.pos, 0.7f, 1.1f);
        }
    }
    return any;
}

bool SecuritySystem::damageRay(const RayHit& hit, float power) {
    if (!hit.hit) return false;
    // test the hit point against device bounding spheres
    for (auto& c : cameras) {
        if (c.destroyed) continue;
        if ((c.pos - hit.pos).len() < 0.55f) {
            c.health -= power;
            if (g_fx) g_fx->sparks(hit.pos, hit.normal, 6, 3.0f);
            if (c.health <= 0) {
                c.destroyed = true;
                audio::play(SFX_METAL_BREAK, c.pos, 0.6f, 1.3f);
            }
            return true;
        }
    }
    for (auto& t : turrets) {
        if (t.destroyed) continue;
        if ((t.pos + vec3(0, 0.35f, 0) - hit.pos).len() < 0.8f) {
            t.health -= power;
            if (g_fx) g_fx->sparks(hit.pos, hit.normal, 6, 3.0f);
            if (t.health <= 0) {
                t.destroyed = true;
                if (g_fx) g_fx->explosion(t.pos + vec3(0, 0.3f, 0), 1.5f, 0.5f, false);
                audio::play(SFX_EXPLOSION_SMALL, t.pos, 0.7f, 1.1f);
            }
            return true;
        }
    }
    return false;
}

void SecuritySystem::appendBoxes(std::vector<BoxInstance>& out, const vec3& camPos) const {
    for (auto& c : cameras) {
        float d = (c.pos - camPos).len();
        if (d > 120.0f) continue;
        // wall bracket
        BoxInstance arm;
        arm.model = matTranslate(c.pos + c.mountDir * 0.18f) * matScale(vec3(0.10f, 0.10f, 0.36f));
        arm.color = vec3(0.24f, 0.25f, 0.27f);
        arm.metal = 0.7f;
        arm.rough = 0.45f;
        out.push_back(arm);
        // camera body, yawed
        vec3 look(std::sin(c.yaw), 0, -std::cos(c.yaw));
        (void)look;
        mat4 rot = matRotY(c.yaw) * matRotX(-0.18f);
        BoxInstance body;
        body.model = matTranslate(c.pos + vec3(0, -0.10f, 0)) * rot * matScale(vec3(0.20f, 0.19f, 0.46f));
        body.color = c.destroyed ? vec3(0.16f, 0.15f, 0.14f) : vec3(0.62f, 0.63f, 0.65f);
        body.metal = 0.55f;
        body.rough = 0.4f;
        out.push_back(body);
        // lens housing
        BoxInstance lens;
        lens.model = matTranslate(c.pos + vec3(0, -0.10f, 0) + rot.dir(vec3(0, 0, 0.26f))) * rot * matScale(vec3(0.11f, 0.11f, 0.14f));
        lens.color = vec3(0.10f, 0.11f, 0.13f);
        lens.metal = 0.9f;
        lens.rough = 0.15f;
        out.push_back(lens);
        // status lamp: green when idle, red when alerting
        float blink = 0.55f + 0.45f * std::sin(g_time * (c.alert ? 12.0f : 2.0f));
        BoxInstance lamp;
        lamp.model = matTranslate(c.pos + vec3(0.0f, -0.02f, -0.08f)) * matScale(vec3(0.05f, 0.05f, 0.05f));
        lamp.color = c.destroyed ? vec3(0.1f, 0.1f, 0.1f) : (c.alert ? vec3(0.9f, 0.1f, 0.08f) : vec3(0.1f, 0.8f, 0.25f));
        lamp.emis = lamp.color * (c.destroyed ? 0.0f : blink * 2.2f);
        out.push_back(lamp);
    }
    for (auto& t : turrets) {
        float d = (t.pos - camPos).len();
        if (d > 130.0f) continue;
        // base
        BoxInstance base;
        base.model = matTranslate(t.pos) * matScale(vec3(0.72f, 0.34f, 0.72f));
        base.color = vec3(0.26f, 0.27f, 0.29f);
        base.metal = 0.6f;
        base.rough = 0.5f;
        out.push_back(base);
        if (t.destroyed) {
            BoxInstance wreck;
            wreck.model = matTranslate(t.pos + vec3(0.1f, 0.18f, 0.05f)) * matRotZ(0.5f) * matScale(vec3(0.5f, 0.3f, 0.6f));
            wreck.color = vec3(0.14f, 0.13f, 0.12f);
            wreck.metal = 0.5f;
            wreck.rough = 0.9f;
            out.push_back(wreck);
            continue;
        }
        // pillar + rotating head
        BoxInstance column;
        column.model = matTranslate(t.pos + vec3(0, 0.42f, 0)) * matScale(vec3(0.28f, 0.5f, 0.28f));
        column.color = vec3(0.20f, 0.21f, 0.23f);
        column.metal = 0.7f;
        column.rough = 0.4f;
        out.push_back(column);
        mat4 head = matTranslate(t.pos + vec3(0, 0.80f, 0)) * matRotY(t.yaw) * matRotX(-t.pitch);
        BoxInstance body;
        body.model = head * matScale(vec3(0.52f, 0.42f, 0.62f));
        body.color = t.hostile ? vec3(0.34f, 0.16f, 0.15f) : vec3(0.30f, 0.32f, 0.34f);
        body.metal = 0.65f;
        body.rough = 0.45f;
        out.push_back(body);
        // twin barrels
        for (int i = -1; i <= 1; i += 2) {
            BoxInstance barrel;
            barrel.model = head * matTranslate(vec3(i * 0.14f, 0.02f, 0.44f)) * matScale(vec3(0.09f, 0.09f, 0.55f));
            barrel.color = vec3(0.16f, 0.17f, 0.18f);
            barrel.metal = 0.9f;
            barrel.rough = 0.3f;
            out.push_back(barrel);
        }
        // muzzle flash
        if (t.muzzleFlash > 0.01f) {
            BoxInstance fl;
            fl.model = head * matTranslate(vec3(0, 0.02f, 0.74f)) * matScale(vec3(0.26f, 0.26f, 0.26f) * t.muzzleFlash);
            fl.color = vec3(1.0f, 0.85f, 0.45f);
            fl.emis = vec3(3.0f, 2.0f, 0.8f) * t.muzzleFlash;
            out.push_back(fl);
        }
        // hostile indicator lamp
        BoxInstance lamp;
        lamp.model = head * matTranslate(vec3(0, 0.26f, 0.0f)) * matScale(vec3(0.07f, 0.07f, 0.07f));
        lamp.color = t.hostile ? vec3(1.0f, 0.15f, 0.1f) : vec3(0.2f, 0.8f, 0.3f);
        lamp.emis = lamp.color * (t.hostile ? (1.5f + std::sin(g_time * 10) * 0.8f) : 0.9f);
        out.push_back(lamp);
    }
}
