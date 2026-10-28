// FRACTURE — particle system implementation
#include "particles.h"

namespace ft {

namespace {
struct P {
    PInst inst;
    Vec3 vel;
    f32 life, maxLife;
    f32 growth, drag, grav;
    f32 fadeStart, fadeIn, age;
    u8 alive;
};
std::vector<P> g_pool;
int g_budget = 3000;
bool inited = false;

P* findDead() {
    for (auto& p : g_pool) if (!p.alive) return &p;
    if ((int)g_pool.size() < g_budget * 2) {
        g_pool.emplace_back();
        g_pool.back().alive = 0;
        return &g_pool.back();
    }
    // recycle oldest
    P* best = nullptr;
    f32 bestLife = -1;
    for (auto& p : g_pool) {
        if (p.alive && p.life > bestLife) { bestLife = p.life; best = &p; }
    }
    return best;
}
} // namespace

void particlesInit() { if (!inited) { g_pool.reserve(2048); inited = true; } }
void particlesShutdown() { g_pool.clear(); g_pool.shrink_to_fit(); inited = false; }
void particlesClear() { for (auto& p : g_pool) p.alive = 0; }
void particlesSetBudget(int maxAlive) { g_budget = tmax(64, maxAlive); }

void spawnParticle(const ParticleSpawn& s) {
    if (!inited) particlesInit();
    // budget: count alive lazily (cheap enough with dense pool scans at spawn)
    int alive = 0;
    for (auto& p : g_pool) alive += p.alive ? 1 : 0;
    if (alive >= g_budget) return;
    P* slot = findDead();
    if (!slot) return;
    *slot = P{};
    slot->inst = s.inst;
    slot->vel = s.vel;
    slot->life = slot->maxLife = tmax(0.05f, s.life);
    slot->growth = s.growth;
    slot->drag = s.drag;
    slot->grav = s.gravity;
    slot->fadeStart = s.fadeStart;
    slot->fadeIn = s.fadeIn;
    slot->age = 0;
    slot->alive = 1;
}

void updateParticles(f32 dt) {
    for (auto& p : g_pool) {
        if (!p.alive) continue;
        p.age += dt;
        p.life -= dt;
        if (p.life <= 0) { p.alive = 0; continue; }
        p.vel.y -= p.grav * dt;
        f32 d = 1.f - tclamp(p.drag * dt, 0.f, 0.9f);
        p.vel *= d;
        p.inst.x += p.vel.x * dt;
        p.inst.y += p.vel.y * dt;
        p.inst.z += p.vel.z * dt;
        p.inst.size = tmax(0.02f, p.inst.size + p.growth * dt);
        // fade
        f32 t = p.age / p.maxLife;
        f32 a = 1.f;
        if (t > p.fadeStart) a = 1.f - (t - p.fadeStart) / tmax(0.001f, 1.f - p.fadeStart);
        if (p.fadeIn > 0 && p.age < p.fadeIn) a *= p.age / p.fadeIn;
        // base alpha stored in inst.a when spawned: we store BASE alpha in pad via... use inst.a as BASE:
        // (emitters set inst.a to base alpha; runtime multiplies — so store base separately:)
        // convention: emitters set inst.a = base; update overwrites inst.a — keep base in fadeIn? No:
        // we keep base in `inst.a` ONLY at spawn — runtime needs base: store in slot via growth field?
        // => use dedicated: base alpha is preserved because emitters set fadeStart and we recompute from `b`.
    }
    // second pass with stored base alpha (kept in a parallel convention: base saved at spawn in life field?):
}

// Base alpha convention: ParticleSpawn.inst.a = BASE alpha. We move base into `fadeIn`? Too hacky —
// instead: keep base in slot->inst.a at spawn and encode runtime alpha in slot->life ratio at render.
// So updateParticles does NOT touch inst.a; render computes alpha = base * fade.

void renderParticles(IBackend* rb) {
    std::vector<PInst> out;
    out.reserve(512);
    for (auto& p : g_pool) {
        if (!p.alive) continue;
        f32 t = p.age / p.maxLife;
        f32 a = 1.f;
        if (t > p.fadeStart) a = 1.f - (t - p.fadeStart) / tmax(0.001f, 1.f - p.fadeStart);
        if (p.fadeIn > 0.f && p.age < p.fadeIn) a *= p.age / p.fadeIn;
        PInst pi = p.inst;
        pi.a = (u8)tclamp(p.inst.a * a, 0.f, 255.f);
        out.push_back(pi);
    }
    if (!out.empty()) rb->drawParticles(out.data(), (int)out.size());
}

int particleCount() {
    int alive = 0;
    for (auto& p : g_pool) alive += p.alive ? 1 : 0;
    return alive;
}

// ---------------------------------------------------------------- emitters --
static f32 rndv(int i, f32 salt = 0) {
    u32 h = hashU32((u32)i * 2654435761u ^ (u32)(salt * 7919));
    return (f32)((h & 0xFFFFFF) / 16777216.0);
}

void fxDust(Vec3 pos, Vec3 color, int count, f32 speed, f32 size, f32 life) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = size * (0.6f + rndv(i, 1) * 0.8f);
        s.inst.r = color.x; s.inst.g = color.y; s.inst.b = color.z;
        s.inst.a = (u8)(110 + (int)(rndv(i, 2) * 50));
        s.inst.rot = rndv(i, 3) * 6.28f;
        s.inst.frame = (f32)(int)(rndv(i, 4) * 4);   // dust frames0..3
        s.inst.kind = 0;
        f32 an = rndv(i, 5) * 6.28f;
        f32 up = rndv(i, 6);
        s.vel = Vec3(std::cos(an) * speed * (0.3f + rndv(i, 7)), speed * (0.35f + up * 0.8f), std::sin(an) * speed * (0.3f + rndv(i, 8)));
        s.life = life * (0.7f + rndv(i, 9) * 0.7f);
        s.growth = size * 0.9f;
        s.drag = 2.2f;
        s.gravity = 0.4f;
        s.fadeStart = 0.35f;
        spawnParticle(s);
    }
}

void fxConcreteDust(Vec3 pos, int count) { fxDust(pos, Vec3(0.72f, 0.71f, 0.68f), count, 1.8f, 0.5f, 1.1f); }

void fxWoodSplinters(Vec3 pos, Vec3 dir, int count) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = 0.09f + rndv(i, 11) * 0.1f;
        s.inst.r = 0.75f; s.inst.g = 0.55f; s.inst.b = 0.3f;
        s.inst.a = 255;
        s.inst.rot = rndv(i, 12) * 6.28f;
        s.inst.frame = 15;
        s.inst.kind = 4;
        s.vel = dir * (2.f + rndv(i, 13) * 4.f) + Vec3(rndv(i, 14) - 0.5f, rndv(i, 15) * 1.4f, rndv(i, 16) - 0.5f) * 3.5f;
        s.life = 0.7f + rndv(i, 17) * 0.8f;
        s.drag = 0.6f;
        s.gravity = 14.f;
        s.fadeStart = 0.75f;
        spawnParticle(s);
    }
}

void fxGlassShards(Vec3 pos, int count) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = 0.05f + rndv(i, 21) * 0.07f;
        s.inst.r = 0.8f; s.inst.g = 0.92f; s.inst.b = 0.96f;
        s.inst.a = 230;
        s.inst.rot = rndv(i, 22) * 6.28f;
        s.inst.frame = 15;
        s.inst.kind = 4;
        f32 an = rndv(i, 23) * 6.28f;
        s.vel = Vec3(std::cos(an), 0.2f + rndv(i, 24), std::sin(an)) * (2.f + rndv(i, 25) * 4.f);
        s.life = 0.8f + rndv(i, 26) * 0.7f;
        s.drag = 0.4f;
        s.gravity = 13.f;
        s.fadeStart = 0.8f;
        spawnParticle(s);
    }
}

void fxSparks(Vec3 pos, Vec3 dir, int count, f32 speed) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = 0.05f + rndv(i, 31) * 0.05f;
        s.inst.r = 1.f; s.inst.g = 0.92f; s.inst.b = 0.55f;
        s.inst.a = 255;
        s.inst.frame = 12;
        s.inst.kind = 2;
        Vec3 jitter(rndv(i, 32) - 0.5f, rndv(i, 33) - 0.5f, rndv(i, 34) - 0.5f);
        s.vel = norm(dir + jitter * 1.2f) * (speed * (0.4f + rndv(i, 35)));
        s.life = 0.2f + rndv(i, 36) * 0.35f;
        s.drag = 1.2f;
        s.gravity = 9.f;
        s.fadeStart = 0.4f;
        spawnParticle(s);
    }
}

void fxSmoke(Vec3 pos, int count, f32 size, f32 life) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x + (rndv(i, 41) - 0.5f) * 0.3f;
        s.inst.y = pos.y + (rndv(i, 42)) * 0.3f;
        s.inst.z = pos.z + (rndv(i, 43) - 0.5f) * 0.3f;
        s.inst.size = size * (0.5f + rndv(i, 44) * 0.8f);
        s.inst.r = 0.55f; s.inst.g = 0.54f; s.inst.b = 0.53f;
        s.inst.a = (u8)(90 + (int)(rndv(i, 45) * 40));
        s.inst.rot = rndv(i, 46) * 6.28f;
        s.inst.frame = 4 + (f32)(int)(rndv(i, 47) * 4);
        s.inst.kind = 1;
        s.vel = Vec3((rndv(i, 48) - 0.5f) * 0.6f, 0.8f + rndv(i, 49) * 0.7f, (rndv(i, 50) - 0.5f) * 0.6f);
        s.life = life * (0.7f + rndv(i, 51) * 0.7f);
        s.growth = size * 0.75f;
        s.drag = 0.8f;
        s.gravity = -0.6f;   // buoyant
        s.fadeStart = 0.3f;
        spawnParticle(s);
    }
}

void fxExplosionCore(Vec3 pos, f32 scale) {
    for (int i = 0; i < 14; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x + (rndv(i, 61) - 0.5f) * scale * 0.6f;
        s.inst.y = pos.y + (rndv(i, 62) - 0.5f) * scale * 0.6f;
        s.inst.z = pos.z + (rndv(i, 63) - 0.5f) * scale * 0.6f;
        s.inst.size = scale * (0.7f + rndv(i, 64) * 0.9f);
        s.inst.r = 1.f; s.inst.g = 0.75f; s.inst.b = 0.3f;
        s.inst.a = 245;
        s.inst.rot = rndv(i, 65) * 6.28f;
        s.inst.frame = 8 + (f32)(int)(rndv(i, 66) * 4);
        s.inst.kind = 1;
        Vec3 dirv(rndv(i, 67) - 0.5f, rndv(i, 68) - 0.3f, rndv(i, 69) - 0.5f);
        s.vel = norm(dirv) * (2.f + rndv(i, 70) * 5.f) * scale;
        s.life = 0.35f + rndv(i, 71) * 0.4f;
        s.growth = scale * 2.4f;
        s.drag = 3.5f;
        s.gravity = -1.f;
        s.fadeStart = 0.25f;
        spawnParticle(s);
    }
    fxSmoke(pos, 8, scale * 1.4f, 2.2f);
    fxShockRing(pos, scale);
}

void fxShockRing(Vec3 pos, f32 scale) {
    ParticleSpawn s;
    s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
    s.inst.size = scale * 1.4f;
    s.inst.r = 1.f; s.inst.g = 0.95f; s.inst.b = 0.85f;
    s.inst.a = 170;
    s.inst.rot = 0;
    s.inst.frame = 13;
    s.inst.kind = 1;
    s.life = 0.28f;
    s.growth = scale * 9.f;
    s.drag = 0;
    s.fadeStart = 0.05f;
    spawnParticle(s);
}

void fxFire(Vec3 pos, f32 scale) {
    for (int i = 0; i < 2; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x + (rndv(i, 81) - 0.5f) * 0.45f * scale;
        s.inst.y = pos.y + rndv(i, 82) * 0.25f;
        s.inst.z = pos.z + (rndv(i, 83) - 0.5f) * 0.45f * scale;
        s.inst.size = (0.5f + rndv(i, 84) * 0.5f) * scale;
        s.inst.r = 1.f; s.inst.g = 0.8f; s.inst.b = 0.35f;
        s.inst.a = 235;
        s.inst.rot = rndv(i, 85) * 6.28f;
        s.inst.frame = 8 + (f32)(int)(rndv(i, 86) * 4);
        s.inst.kind = 1;
        s.vel = Vec3((rndv(i, 87) - 0.5f) * 0.5f, 1.6f + rndv(i, 88) * 1.4f, (rndv(i, 89) - 0.5f) * 0.5f);
        s.life = 0.32f + rndv(i, 90) * 0.3f;
        s.growth = 0.7f * scale;
        s.drag = 2.f;
        s.gravity = -1.5f;
        s.fadeStart = 0.3f;
        spawnParticle(s);
    }
    if (((int)(pos.x * 13 + pos.z * 7) & 3) == 0) fxSmoke(Vec3(pos.x, pos.y + 0.7f * scale, pos.z), 1, 0.8f * scale, 1.8f);
}

void fxEmber(Vec3 pos, int count) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = 0.045f;
        s.inst.r = 1.f; s.inst.g = 0.6f; s.inst.b = 0.2f;
        s.inst.a = 255;
        s.inst.frame = 12;
        s.inst.kind = 3;
        s.vel = Vec3((rndv(i, 101) - 0.5f) * 1.4f, 1.2f + rndv(i, 102) * 2.f, (rndv(i, 103) - 0.5f) * 1.4f);
        s.life = 0.7f + rndv(i, 104) * 1.1f;
        s.drag = 0.7f;
        s.gravity = -0.8f;
        s.fadeStart = 0.4f;
        spawnParticle(s);
    }
}

void fxDebrisChips(Vec3 pos, Vec3 color, int count, f32 speed) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = 0.06f + rndv(i, 111) * 0.08f;
        s.inst.r = color.x; s.inst.g = color.y; s.inst.b = color.z;
        s.inst.a = 255;
        s.inst.rot = rndv(i, 112) * 6.28f;
        s.inst.frame = 15;
        s.inst.kind = 4;
        Vec3 j(rndv(i, 113) - 0.5f, rndv(i, 114) * 0.6f, rndv(i, 115) - 0.5f);
        s.vel = j * speed * 2.f;
        s.life = 0.6f + rndv(i, 116) * 0.7f;
        s.drag = 0.5f;
        s.gravity = 15.f;
        s.fadeStart = 0.8f;
        spawnParticle(s);
    }
}

void fxWaterSplash(Vec3 pos, int count) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = 0.16f + rndv(i, 121) * 0.2f;
        s.inst.r = 0.78f; s.inst.g = 0.86f; s.inst.b = 0.92f;
        s.inst.a = 200;
        s.inst.frame = 14;
        s.inst.kind = 1;
        f32 an = rndv(i, 122) * 6.28f;
        s.vel = Vec3(std::cos(an) * 1.4f, 2.2f + rndv(i, 123) * 2.f, std::sin(an) * 1.4f);
        s.life = 0.5f + rndv(i, 124) * 0.4f;
        s.drag = 1.2f;
        s.gravity = 11.f;
        s.fadeStart = 0.5f;
        spawnParticle(s);
    }
}

void fxDustImpact(Vec3 pos, Vec3 nrm, int count) {
    for (int i = 0; i < count; i++) {
        ParticleSpawn s;
        s.inst.x = pos.x; s.inst.y = pos.y; s.inst.z = pos.z;
        s.inst.size = 0.28f + rndv(i, 131) * 0.3f;
        s.inst.r = 0.7f; s.inst.g = 0.68f; s.inst.b = 0.64f;
        s.inst.a = (u8)(80 + (int)(rndv(i, 132) * 50));
        s.inst.rot = rndv(i, 133) * 6.28f;
        s.inst.frame = (f32)(int)(rndv(i, 134) * 4);
        s.inst.kind = 0;
        Vec3 jit(rndv(i, 135) - 0.5f, rndv(i, 136) - 0.5f, rndv(i, 137) - 0.5f);
        s.vel = norm(nrm + jit * 1.4f) * (1.2f + rndv(i, 138) * 2.f);
        s.life = 0.5f + rndv(i, 139) * 0.5f;
        s.growth = 0.5f;
        s.drag = 3.f;
        s.gravity = 0.5f;
        s.fadeStart = 0.4f;
        spawnParticle(s);
    }
}

} // namespace ft
