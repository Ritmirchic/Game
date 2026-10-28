// FRACTURE — pooled particle system with typed emitters
#pragma once
#include "render.h"

namespace ft {

struct ParticleSpawn {
    PInst inst{};          // pos/size/color/rot/frame/kind preset by caller
    Vec3 vel;
    f32  life = 1.f;
    f32  growth = 0.f;     // size change over life (metres)
    f32  drag = 3.f;       // velocity damping /s
    f32  gravity = 0.f;    // extra gravity on particle
    f32  fadeStart = 0.4f; // fraction of life where fade begins
    f32  fadeIn = 0.f;     // initial fade-in seconds (0 = instant)
};

void particlesInit();
void particlesShutdown();
void particlesClear();
void particlesSetBudget(int maxAlive);
void spawnParticle(const ParticleSpawn& s);
void updateParticles(f32 dt);
void renderParticles(IBackend* rb);
int  particleCount();

// ---- ready-made emitters (pooled, budget-aware) ----
void fxDust(Vec3 pos, Vec3 color, int count = 8, f32 speed = 1.6f, f32 size = 0.55f, f32 life = 0.9f);
void fxConcreteDust(Vec3 pos, int count = 10);
void fxWoodSplinters(Vec3 pos, Vec3 dir, int count = 7);
void fxGlassShards(Vec3 pos, int count = 8);
void fxSparks(Vec3 pos, Vec3 dir, int count = 10, f32 speed = 6.f);
void fxSmoke(Vec3 pos, int count = 4, f32 size = 1.1f, f32 life = 1.6f);
void fxExplosionCore(Vec3 pos, f32 scale);
void fxShockRing(Vec3 pos, f32 scale);
void fxFire(Vec3 pos, f32 scale = 1.f);
void fxEmber(Vec3 pos, int count = 3);
void fxDebrisChips(Vec3 pos, Vec3 color, int count = 6, f32 speed = 4.f);
void fxWaterSplash(Vec3 pos, int count = 8);
void fxDustImpact(Vec3 pos, Vec3 nrm, int count = 5);

} // namespace ft
