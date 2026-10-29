// FRACTURE — rigid body physics on top of the voxel world: debris, props, collapse pieces.
#pragma once
#include "core/math.h"
#include "world/voxel.h"
#include <vector>

struct Body {
    vec3 pos, vel, angVel;
    quat rot;
    vec3 half{0.1f, 0.1f, 0.1f};
    float mass = 10, invMass = 0.1f;
    vec3 invI{1, 1, 1};
    uint8_t mat = M_CONCRETE;
    float restitution = 0.18f;
    float friction = 0.62f;
    float sleepTimer = 0;
    bool sleeping = false;
    bool active = false;
    float life = -1;          // <0 permanent
    float born = 0;
    float damage = 0;         // accumulated impact damage
    bool heavy = false;       // structural piece: carves the world on impact
    float hitCooldown = 0;
    vec3 color{0.7f, 0.7f, 0.7f};
    bool isProp = false;      // interactive prop (barrel, crate)
    int propKind = 0;
    float health = 100;
    mat4 model;
    AABB box;
};

struct DebrisStats {
    int activeBodies = 0;
    int sleepingBodies = 0;
    int spawnedTotal = 0;
};

class Physics {
public:
    World* world = nullptr;
    std::vector<Body> bodies;
    int maxBodies = 380;
    float time = 0;
    int gravityEnabled = 1;
    float gravityScale = 1.0f;
    DebrisStats stats;
    std::vector<int> impactEvents;   // body indices that hit hard this frame (for audio/FX)

    void init(World* w);
    void clear();
    int spawn(vec3 pos, vec3 half, uint8_t mat, float mass, vec3 vel, vec3 angVel, float life, bool heavy);
    void spawnDebrisBurst(const vec3& center, const AABB& area, const int* matCounts, int quality, float power);
    void spawnCollapsePiece(const vec3& pos, const vec3& half, uint8_t mat, const vec3& push);
    void skipVoxels(int x0, int y0, int z0, int x1, int y1, int z1);

    void update(float dt);
    void applyRadialImpulse(const vec3& center, float radius, float strength);
    void wakeAll();

    void integrate(float h);
    void collideVoxels(Body& b, float h);
    void collideBodies(float h);
    void updateModel(Body& b);

    // player capsule collision against the world (returns corrected position + grounded flag)
    bool moveCapsule(vec3& pos, vec3& vel, float radius, float height, float dt, bool& grounded, bool& hitCeiling);
    bool capsuleBlocked(const vec3& pos, float radius, float height);
    float capsuleGround(const vec3& pos, float radius);

    // line of sight / helpers
    bool pointBlocked(const vec3& p) const;

    Body* nearestProp(const vec3& p, float maxDist);
    void damageBody(int index, float dmg, const vec3& impulseDir, float impulse);
    bool spawnProp(vec3 pos, int kind);   // 0 barrel, 1 crate, 2 tire stack, 3 fuel tank, 4 explosive barrel
    void cullFarBodies(const vec3& viewPos, float maxDist);
    size_t memoryUse() const { return bodies.capacity() * sizeof(Body); }
};

