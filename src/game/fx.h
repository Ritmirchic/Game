// FRACTURE — particle effects, explosion lights, fire simulation.
#pragma once
#include "core/engine.h"
#include "world/voxel.h"
#include <vector>
#include <unordered_map>

struct FireCell {
    ivec3 cell;
    float heat = 1.0f;
    float life = 0;
    float nextSpread = 0;
    float smokeTimer = 0;
};

class FX {
public:
    std::vector<Particle> particles;
    int maxParticles = 2600;
    int qualityLevel = 2;
    float time = 0;
    std::vector<LightInstance> lights;   // transient lights (explosions, fire)
    std::vector<FireCell> fires;
    int maxFires = 220;
    World* world = nullptr;
    float screenFlash = 0;
    vec3 flashColor{1, 0.8f, 0.5f};

    void init(World* w);
    void clear();
    void setQuality(int q);
    Particle& alloc();

    void update(float dt, const vec3& camPos);
    void draw(IRenderer& r);

    // effects
    void impact(uint8_t mat, const vec3& pos, const vec3& normal, float strength, int dirSeed);
    void dustBurst(const vec3& pos, int count, float radius, float speed, const vec3& color);
    void explosion(const vec3& pos, float radius, float power, bool large);
    void smokeTrail(const vec3& pos, const vec3& vel, float size, uint32_t color, float life);
    void sparks(const vec3& pos, const vec3& normal, int count, float speed);
    void glassShards(const vec3& pos, const vec3& normal, int count);
    void waterSplash(const vec3& pos, int count);
    void fireBurst(const vec3& pos, float radius, int count);
    void extinguishSpray(const vec3& pos, const vec3& dir, float dt);
    void beamSparks(const vec3& pos, const vec3& dir, int count);
    void muzzleFlash(const vec3& pos, const vec3& dir);
    void smokePlume(const vec3& pos, const vec3& vel, float size, float life, uint32_t color, int kind);
    void dustRing(const vec3& pos, float radius, int count);

    // fire simulation
    void ignite(int x, int y, int z, float heat);
    void igniteArea(const vec3& center, float radius);
    void updateFire(float dt);
    void extinguishArea(const vec3& center, float radius, float strength);
    int fireCount() const { return (int)fires.size(); }
    float fireLightNear(const vec3& p, float radius) const;

    // queries
    int alive() const;
    void clearLightList() { lights.clear(); }
private:
    void spawn(Particle p);
    void damageFromFire(const ivec3& cell, float dmg);
};
