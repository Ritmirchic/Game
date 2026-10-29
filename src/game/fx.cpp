#include "game/fx.h"
#include "game/globals.h"
#include "world/physics.h"
#include <cmath>
#include <algorithm>

static uint32_t matDustColor(uint8_t m) {
    const Material& mt = material(m);
    vec3 c = mt.color * 0.82f + vec3(0.16f);
    return packColor(c.x, c.y, c.z, 1);
}

void FX::init(World* w) {
    world = w;
    particles.clear();
    particles.reserve(maxParticles + 64);
    fires.clear();
}

void FX::clear() {
    particles.clear();
    fires.clear();
    lights.clear();
    screenFlash = 0;
}

void FX::setQuality(int q) {
    qualityLevel = clampi(q, 0, 3);
    static const int counts[4] = {700, 1300, 2400, 3800};
    maxParticles = counts[qualityLevel];
    static const int firesMax[4] = {80, 140, 220, 320};
    maxFires = firesMax[qualityLevel];
}

Particle& FX::alloc() {
    // reuse a dead particle, else overwrite the oldest
    for (int i = 0; i < (int)particles.size(); i++)
        if (particles[i].life <= 0) return particles[i];
    if ((int)particles.size() < maxParticles) {
        particles.push_back(Particle());
        return particles.back();
    }
    // exponential decay budget: drop the particle with lowest life
    int idx = 0;
    float lowest = 1e9f;
    for (int i = 0; i < (int)particles.size(); i += 7) {
        if (particles[i].life < lowest) { lowest = particles[i].life; idx = i; }
    }
    return particles[idx];
}

void FX::spawn(Particle p) {
    if ((int)particles.size() >= maxParticles && qualityLevel == 0) {
        if (hash32((uint32_t)(time * 1000)) % 3 != 0) return;
    }
    Particle& dst = alloc();
    dst = p;
}

void FX::update(float dt, const vec3& camPos) {
    time += dt;
    int budget = maxParticles;
    for (auto& p : particles) {
        if (p.life <= 0) continue;
        p.life -= dt;
        if (p.life <= 0) continue;
        p.vel.y -= p.gravity * dt;
        p.vel *= (1.0f - 1.1f * dt);
        p.pos += p.vel * dt;
        p.size += p.sizeVel * dt;
        if (p.size < 0.001f) p.size = 0.001f;
        // simple world collision for solid debris particles and sparks
        if ((p.kind == 2 || p.kind == 4 || p.kind == 5) && world) {
            if (world->solidAtM(p.pos)) {
                if (p.kind == 2) {
                    p.vel.y = std::fabs(p.vel.y) * 0.35f;
                    p.vel.x *= 0.6f;
                    p.vel.z *= 0.6f;
                    p.life *= 0.6f;
                } else if (p.kind == 4) {
                    p.life = 0;
                } else {
                    p.vel *= 0.35f;
                    p.vel.y = std::fabs(p.vel.y) * 0.2f;
                }
            }
        }
        // distance culling of expensive far particles
        if ((int)particles.size() > budget / 2) {
            float d2 = (p.pos - camPos).len2();
            if (d2 > 120.0f * 120.0f) p.life = 0;
        }
    }
    // fire
    updateFire(dt);
    // lights decay
    for (auto& l : lights) l.intensity *= (1.0f - 4.0f * dt);
    for (int i = (int)lights.size() - 1; i >= 0; i--)
        if (lights[i].intensity < 0.02f) lights.erase(lights.begin() + i);
    screenFlash *= (1.0f - 6.0f * dt);
}

void FX::draw(IRenderer& r) {
    int count = 0;
    for (auto& p : particles) if (p.life > 0) count++;
    if (count == 0) return;
    // build a compact array for the renderer
    static std::vector<Particle> buf;
    buf.clear();
    buf.reserve(count);
    for (auto& p : particles)
        if (p.life > 0) buf.push_back(p);
    r.drawParticles(buf.data(), (int)buf.size());
}

int FX::alive() const {
    int n = 0;
    for (auto& p : particles) if (p.life > 0) n++;
    return n;
}

// ---------------------------------------------------------------- effects
void FX::impact(uint8_t mat, const vec3& pos, const vec3& normal, float strength, int dirSeed) {
    const Material& mt = material(mat);
    RNG rng(0x9E3779B9u ^ (uint32_t)(dirSeed * 2654435761u) ^ (uint32_t)(time * 977));
    int n = (int)(6 + strength * 9);
    n = std::min(n, 46) * std::max(1, qualityLevel) / 2;
    vec3 base = vlerp(pos, mt.color * 0.75f, 0.0f);
    (void)base;
    uint32_t col = matDustColor(mat);
    // chipped material
    for (int i = 0; i < n; i++) {
        Particle p;
        vec3 dir = (normal + rng.dir() * 1.15f).norm();
        p.pos = pos + normal * 0.04f + rng.dir() * 0.06f;
        p.vel = dir * rng.range(1.2f, 3.4f + strength * 1.6f);
        p.size = rng.range(0.018f, 0.052f) * (1 + strength * 0.4f);
        p.sizeVel = 0;
        p.maxLife = rng.range(0.35f, 1.05f);
        p.life = p.maxLife;
        p.color = col;
        p.gravity = 9.0f;
        p.kind = 5;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
    // dust puff
    int dn = (int)(3 + strength * 5) * std::max(1, qualityLevel);
    for (int i = 0; i < dn; i++) {
        Particle p;
        p.pos = pos + normal * 0.08f + rng.dir() * 0.16f;
        p.vel = (normal * 0.7f + rng.dir() * 0.9f) * rng.range(0.25f, 1.15f);
        p.size = rng.range(0.12f, 0.28f);
        p.sizeVel = rng.range(0.22f, 0.6f);
        p.maxLife = rng.range(0.7f, 1.7f);
        p.life = p.maxLife;
        p.gravity = -0.35f;
        p.color = col;
        p.kind = 1;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
    // material specific
    if (mt.spark > 0.15f || mat == M_METAL || mat == M_METAL_DARK || mat == M_GRATE) {
        sparks(pos, normal, (int)(4 + strength * 14), 3.5f + strength * 2.5f);
    }
    if (mat == M_GLASS || mat == M_SKYLIGHT) glassShards(pos, normal, (int)(5 + strength * 14));
    if (mat == M_WOOD || mat == M_PLANK || mat == M_WOOD_DARK || mat == M_TRUNK) {
        // splinters
        for (int i = 0; i < 5; i++) {
            Particle p;
            p.pos = pos + normal * 0.05f;
            p.vel = (normal + rng.dir()).norm() * rng.range(1.6f, 4.0f);
            p.size = rng.range(0.012f, 0.03f);
            p.sizeVel = -0.004f;
            p.maxLife = rng.range(0.5f, 1.2f);
            p.life = p.maxLife;
            p.color = packColor(0.52f, 0.36f, 0.19f);
            p.gravity = 8.0f;
            p.kind = 5;
            p.rot = (uint8_t)(rng.u32() & 0xFF);
            spawn(p);
        }
    }
}

void FX::dustBurst(const vec3& pos, int count, float radius, float speed, const vec3& color) {
    RNG rng((uint32_t)(pos.x * 977) ^ (uint32_t)(time * 733));
    uint32_t col = packColor(color.x, color.y, color.z);
    for (int i = 0; i < count * std::max(1, qualityLevel) / 2; i++) {
        Particle p;
        p.pos = pos + rng.dir() * radius * rng.f();
        p.vel = (rng.dir() * 0.6f + vec3(0, 0.35f, 0)) * speed * rng.range(0.4f, 1.4f);
        p.size = rng.range(0.16f, 0.4f);
        p.sizeVel = rng.range(0.3f, 0.85f);
        p.maxLife = rng.range(0.9f, 2.2f);
        p.life = p.maxLife;
        p.gravity = -0.5f;
        p.color = col;
        p.kind = 1;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
}

void FX::dustRing(const vec3& pos, float radius, int count) {
    RNG rng((uint32_t)(pos.x * 131) ^ (uint32_t)(pos.z * 977) ^ (uint32_t)(time * 31));
    for (int i = 0; i < count * std::max(1, qualityLevel); i++) {
        float a = rng.range(0, 2 * PI);
        vec3 dir(std::cos(a), 0.1f, std::sin(a));
        Particle p;
        p.pos = pos + dir * radius * rng.range(0.35f, 1.0f);
        p.vel = dir * rng.range(2.0f, 7.0f) + vec3(0, rng.range(0.2f, 1.2f), 0);
        p.size = rng.range(0.2f, 0.55f);
        p.sizeVel = rng.range(0.5f, 1.4f);
        p.maxLife = rng.range(1.0f, 2.4f);
        p.life = p.maxLife;
        p.gravity = -0.3f;
        p.color = packColor(0.62f, 0.58f, 0.52f);
        p.kind = 1;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
}

void FX::sparks(const vec3& pos, const vec3& normal, int count, float speed) {
    RNG rng((uint32_t)(pos.x * 311) ^ (uint32_t)(pos.y * 733) ^ (uint32_t)(time * 97));
    for (int i = 0; i < count * std::max(1, qualityLevel) / 2; i++) {
        Particle p;
        p.pos = pos + normal * 0.03f;
        p.vel = (normal * 0.7f + rng.dir() * 1.1f).norm() * rng.range(speed * 0.35f, speed);
        p.size = rng.range(0.008f, 0.02f);
        p.sizeVel = -0.01f;
        p.maxLife = rng.range(0.2f, 0.85f);
        p.life = p.maxLife;
        p.gravity = 9.5f;
        p.color = packColor(1.0f, 0.85f, 0.45f);
        p.kind = 2;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
}

void FX::glassShards(const vec3& pos, const vec3& normal, int count) {
    RNG rng((uint32_t)(pos.x * 617) ^ (uint32_t)(pos.z * 131) ^ (uint32_t)(time * 53));
    for (int i = 0; i < count * std::max(1, qualityLevel) / 2; i++) {
        Particle p;
        p.pos = pos + normal * 0.02f + rng.dir() * 0.12f;
        p.vel = (normal * 0.85f + rng.dir() * 0.8f).norm() * rng.range(1.4f, 5.2f);
        p.size = rng.range(0.012f, 0.038f);
        p.sizeVel = 0;
        p.maxLife = rng.range(0.6f, 1.6f);
        p.life = p.maxLife;
        p.gravity = 9.5f;
        p.color = packColor(0.78f, 0.90f, 0.96f);
        p.kind = 4;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
    // glinting dust
    for (int i = 0; i < 4; i++) {
        Particle p;
        p.pos = pos + normal * 0.05f;
        p.vel = rng.dir() * 0.7f;
        p.size = rng.range(0.03f, 0.07f);
        p.sizeVel = 0.05f;
        p.maxLife = 0.5f;
        p.life = 0.5f;
        p.gravity = 0.4f;
        p.color = packColor(0.9f, 0.95f, 1.0f);
        p.kind = 2;
        spawn(p);
    }
}

void FX::waterSplash(const vec3& pos, int count) {
    RNG rng((uint32_t)(pos.x * 191) ^ (uint32_t)(time * 17));
    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos = pos;
        p.vel = vec3(rng.range(-1.6f, 1.6f), rng.range(1.5f, 5.0f), rng.range(-1.6f, 1.6f));
        p.size = rng.range(0.02f, 0.07f);
        p.sizeVel = 0;
        p.maxLife = rng.range(0.3f, 0.8f);
        p.life = p.maxLife;
        p.gravity = 11.0f;
        p.color = packColor(0.72f, 0.85f, 0.92f);
        p.kind = 6;
        spawn(p);
    }
}

void FX::explosion(const vec3& pos, float radius, float power, bool large) {
    RNG rng((uint32_t)(pos.x * 733) ^ (uint32_t)(pos.z * 311) ^ (uint32_t)(time * 131));
    int scale = std::max(1, qualityLevel);
    // fireball
    int fn = (int)(10 + radius * 8) * scale;
    for (int i = 0; i < fn; i++) {
        Particle p;
        vec3 d = rng.dir();
        p.pos = pos + d * rng.range(0, radius * 0.55f);
        p.vel = d * rng.range(1.5f, radius * 2.6f) + vec3(0, rng.range(0.5f, 2.5f), 0);
        p.size = rng.range(0.25f, 0.75f) * (0.7f + radius * 0.12f);
        p.sizeVel = rng.range(0.6f, 1.9f);
        p.maxLife = rng.range(0.35f, 1.1f);
        p.life = p.maxLife;
        p.gravity = -2.5f;
        p.color = packColor(1.0f, rng.range(0.45f, 0.8f), rng.range(0.1f, 0.25f));
        p.kind = 3;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
    // smoke
    int sn = (int)(12 + radius * 10) * scale;
    for (int i = 0; i < sn; i++) {
        Particle p;
        vec3 d = rng.dir();
        p.pos = pos + d * rng.range(0, radius * 0.7f);
        p.vel = d * rng.range(0.8f, radius * 1.9f) + vec3(0, rng.range(1.0f, 4.0f), 0);
        p.size = rng.range(0.4f, 1.15f) * (0.7f + radius * 0.1f);
        p.sizeVel = rng.range(0.7f, 2.1f);
        p.maxLife = rng.range(1.6f, 4.2f);
        p.life = p.maxLife;
        p.gravity = -1.2f;
        p.color = packColor(0.20f, 0.19f, 0.18f);
        p.kind = 7;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
    // debris chunks
    int dn = (int)(6 + radius * 3) * scale;
    for (int i = 0; i < dn; i++) {
        Particle p;
        vec3 d = rng.dir();
        p.pos = pos + d * rng.range(0, radius * 0.4f);
        p.vel = d * rng.range(3.0f, radius * 4.0f) + vec3(0, rng.range(1.5f, 6.0f), 0);
        p.size = rng.range(0.03f, 0.10f);
        p.sizeVel = 0;
        p.maxLife = rng.range(1.0f, 2.4f);
        p.life = p.maxLife;
        p.gravity = 9.8f;
        p.color = packColor(0.42f, 0.40f, 0.37f);
        p.kind = 5;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
    // sparks
    sparks(pos, vec3(0, 1, 0), (int)(8 + radius * 5), 12.0f + radius * 2.0f);
    // ground ring
    dustRing(vec3(pos.x, pos.y - 0.2f, pos.z), radius * 0.85f, (int)(4 + radius));
    // light
    LightInstance l;
    l.pos = pos;
    l.color = vec3(1.0f, 0.68f, 0.32f);
    l.radius = radius * 5.0f;
    l.intensity = large ? 3.0f : 1.7f;
    lights.push_back(l);
    screenFlash = std::max(screenFlash, large ? 0.75f : 0.34f);
    flashColor = vec3(1.0f, 0.72f, 0.4f);
    // ignite surroundings
    if (power > 3.0f) igniteArea(pos, radius * 0.9f);
}

void FX::smokeTrail(const vec3& pos, const vec3& vel, float size, uint32_t color, float life) {
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.size = size;
    p.sizeVel = size * 0.9f;
    p.maxLife = life;
    p.life = life;
    p.gravity = -0.3f;
    p.color = color;
    p.kind = 0;
    p.rot = (uint8_t)(hash32((uint32_t)(time * 1000)) & 0xFF);
    spawn(p);
}

void FX::smokePlume(const vec3& pos, const vec3& vel, float size, float life, uint32_t color, int kind) {
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.size = size;
    p.sizeVel = size * 0.7f;
    p.maxLife = life;
    p.life = life;
    p.gravity = -0.4f;
    p.color = color;
    p.kind = (uint8_t)kind;
    spawn(p);
}

void FX::fireBurst(const vec3& pos, float radius, int count) {
    RNG rng((uint32_t)(pos.x * 61) ^ (uint32_t)(time * 7));
    for (int i = 0; i < count * std::max(1, qualityLevel) / 2; i++) {
        Particle p;
        p.pos = pos + rng.dir() * radius * rng.f();
        p.vel = vec3(rng.range(-0.7f, 0.7f), rng.range(1.0f, 3.2f), rng.range(-0.7f, 0.7f));
        p.size = rng.range(0.14f, 0.42f);
        p.sizeVel = rng.range(0.2f, 0.7f);
        p.maxLife = rng.range(0.35f, 1.0f);
        p.life = p.maxLife;
        p.gravity = -2.0f;
        p.color = packColor(1.0f, rng.range(0.4f, 0.75f), 0.12f);
        p.kind = 3;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
}

void FX::extinguishSpray(const vec3& pos, const vec3& dir, float dt) {
    RNG rng((uint32_t)(time * 100000));
    int n = (int)(dt * 260 * std::max(1, qualityLevel) / 2);
    for (int i = 0; i < n; i++) {
        Particle p;
        p.pos = pos;
        vec3 d = (dir + rng.dir() * 0.22f).norm();
        p.vel = d * rng.range(5.0f, 11.0f);
        p.size = rng.range(0.03f, 0.10f);
        p.sizeVel = rng.range(0.3f, 0.9f);
        p.maxLife = rng.range(0.35f, 0.8f);
        p.life = p.maxLife;
        p.gravity = 1.2f;
        p.color = packColor(0.92f, 0.95f, 0.98f);
        p.kind = 1;
        p.rot = (uint8_t)(rng.u32() & 0xFF);
        spawn(p);
    }
}

void FX::beamSparks(const vec3& pos, const vec3& dir, int count) {
    sparks(pos, dir, count, 6.0f);
}

void FX::muzzleFlash(const vec3& pos, const vec3& dir) {
    Particle p;
    p.pos = pos + dir * 0.2f;
    p.vel = dir * 3.0f;
    p.size = 0.22f;
    p.sizeVel = 0.4f;
    p.maxLife = 0.07f;
    p.life = 0.07f;
    p.gravity = 0;
    p.color = packColor(1.0f, 0.86f, 0.55f);
    p.kind = 3;
    spawn(p);
    LightInstance l;
    l.pos = pos + dir * 0.3f;
    l.color = vec3(1.0f, 0.85f, 0.55f);
    l.radius = 5;
    l.intensity = 1.2f;
    lights.push_back(l);
}

// ---------------------------------------------------------------- fire
void FX::ignite(int x, int y, int z, float heat) {
    if ((int)fires.size() >= maxFires) return;
    for (auto& f : fires)
        if (f.cell.x == x && f.cell.y == y && f.cell.z == z) {
            f.heat = std::min(2.5f, f.heat + heat * 0.5f);
            f.life = std::max(f.life, 4.0f);
            return;
        }
    FireCell c;
    c.cell = {x, y, z};
    c.heat = heat;
    c.life = 3.0f + heat * 2.0f;
    c.nextSpread = 1.0f + hash3f(x, y, z) * 2.0f;
    fires.push_back(c);
}

void FX::igniteArea(const vec3& center, float radius) {
    if (!world) return;
    int r = (int)(radius / VOX);
    ivec3 c((int)(center.x / VOX), (int)(center.y / VOX), (int)(center.z / VOX));
    int attempts = 0;
    for (int dy = -r; dy <= r && attempts < 260; dy += 2)
        for (int dz = -r; dz <= r && attempts < 260; dz += 2)
            for (int dx = -r; dx <= r && attempts < 260; dx += 2) {
                if (dx * dx + dy * dy + dz * dz > r * r) continue;
                int x = c.x + dx, y = c.y + dy, z = c.z + dz;
                uint8_t m = world->at(x, y, z);
                if (!matSolid(m)) continue;
                if (!material(m).flammable) continue;
                ignite(x, y, z, 1.0f);
                attempts++;
            }
}

void FX::damageFromFire(const ivec3& cell, float dmg) {
    if (!world) return;
    uint8_t m = world->at(cell.x, cell.y, cell.z);
    if (!matSolid(m)) return;
    uint64_t k = World::keyOf(cell.x, cell.y, cell.z);
    int cur = world->damageAt(cell.x, cell.y, cell.z) + (int)dmg;
    if (cur >= (int)material(m).resistance) {
        world->setFast(cell.x, cell.y, cell.z, M_AIR);
    } else {
        world->damage[k] = (uint8_t)std::min(255, cur);
    }
}

void FX::updateFire(float dt) {
    if (!world) return;
    static float spreadAccum = 0;
    spreadAccum += dt;
    int qualityCap = qualityLevel == 0 ? 0 : 1;
    for (int i = (int)fires.size() - 1; i >= 0; i--) {
        FireCell& f = fires[i];
        uint8_t m = world->at(f.cell.x, f.cell.y, f.cell.z);
        if (!matSolid(m) || !material(m).flammable) {
            fires.erase(fires.begin() + i);
            continue;
        }
        f.life -= dt;
        f.heat = std::max(0.0f, f.heat - dt * 0.055f);
        // burn damage
        static float dmgAccum = 0;
        dmgAccum += dt;
        if (dmgAccum > 0.22f) {
            damageFromFire(f.cell, f.heat * 2.4f);
            dmgAccum = 0;
        }
        // visuals
        f.smokeTimer -= dt;
        vec3 wp((f.cell.x + 0.5f) * VOX, (f.cell.y + 0.5f) * VOX, (f.cell.z + 0.5f) * VOX);
        RNG rng(hash32((uint32_t)(f.cell.x * 31 + f.cell.y * 17 + f.cell.z * 7) + (uint32_t)(time * 13)));
        if (f.smokeTimer <= 0) {
            f.smokeTimer = 0.07f + rng.f() * 0.10f;
            Particle p;
            p.pos = wp + rng.dir() * 0.1f;
            p.vel = vec3(rng.range(-0.3f, 0.3f), rng.range(0.9f, 2.4f), rng.range(-0.3f, 0.3f));
            p.size = rng.range(0.06f, 0.16f) * (0.7f + f.heat);
            p.sizeVel = rng.range(0.1f, 0.3f);
            p.maxLife = rng.range(0.3f, 0.7f);
            p.life = p.maxLife;
            p.gravity = -1.4f;
            p.color = packColor(1.0f, rng.range(0.35f, 0.7f), 0.1f);
            p.kind = 3;
            spawn(p);
            if (f.heat > 0.6f && (qualityLevel >= 1)) {
                Particle s;
                s.pos = wp + rng.dir() * 0.2f;
                s.vel = vec3(rng.range(-0.2f, 0.2f), rng.range(1.0f, 2.2f), rng.range(-0.2f, 0.2f));
                s.size = rng.range(0.14f, 0.34f);
                s.sizeVel = rng.range(0.25f, 0.6f);
                s.maxLife = rng.range(1.2f, 2.6f);
                s.life = s.maxLife;
                s.gravity = -0.8f;
                s.color = packColor(0.16f, 0.15f, 0.14f);
                s.kind = 7;
                spawn(s);
            }
        }
        // spread
        f.nextSpread -= dt;
        if (f.nextSpread <= 0 && qualityCap) {
            f.nextSpread = 0.8f + rng.f() * 1.4f;
            static const int off[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            for (auto& o : off) {
                if (rng.f() > 0.24f) continue;
                int nx = f.cell.x + o[0], ny = f.cell.y + o[1], nz = f.cell.z + o[2];
                uint8_t nm = world->at(nx, ny, nz);
                if (matSolid(nm) && material(nm).flammable) ignite(nx, ny, nz, f.heat * 0.7f);
            }
        }
        // ignite nearby explosive props
        if (g_physics) {
            for (auto& b : g_physics->bodies) {
                if (!b.active || !b.isProp) continue;
                if ((b.pos - wp).len2() < 1.2f) {
                    if (b.propKind == 4) {
                        b.health -= dt * 22.0f;
                        if (b.health <= 0) {
                            b.active = false;
                            extern void explodeAt(const vec3&, float, int);
                            explodeAt(b.pos, 6.0f, 1);
                        }
                    }
                }
            }
        }
        if (f.life <= 0 || f.heat <= 0.02f) fires.erase(fires.begin() + i);
    }
}

void FX::extinguishArea(const vec3& center, float radius, float strength) {
    for (int i = (int)fires.size() - 1; i >= 0; i--) {
        vec3 wp((fires[i].cell.x + 0.5f) * VOX, (fires[i].cell.y + 0.5f) * VOX, (fires[i].cell.z + 0.5f) * VOX);
        if ((wp - center).len() < radius) {
            fires[i].heat -= strength;
            if (fires[i].heat <= 0) fires.erase(fires.begin() + i);
        }
    }
}

float FX::fireLightNear(const vec3& p, float radius) const {
    float total = 0;
    for (auto& f : fires) {
        vec3 wp((f.cell.x + 0.5f) * VOX, (f.cell.y + 0.5f) * VOX, (f.cell.z + 0.5f) * VOX);
        float d = (wp - p).len();
        if (d < radius) total += (1.0f - d / radius) * f.heat;
    }
    return std::min(1.6f, total * 0.35f);
}
