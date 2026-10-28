// FRACTURE — physics: rigid boxes, falling voxel fragments, constraints.
#pragma once
#include "voxmesh.h"
#include "support.h"

namespace ft {

enum BodyKind : u8 {
    BK_PROP = 0,      // furniture, crates, barrels…
    BK_PLANK,         // player-placed plank
    BK_DEBRIS,        // small broken piece
    BK_DOOR,          // hinged door slab
    BK_PICKUP,        // mission item
    BK_VEH_PART,      // vehicle piece after wreck
};

enum BodyFlags : u16 {
    BF_STATIC    = 1 << 0,
    BF_SLEEP     = 1 << 1,
    BF_HELD      = 1 << 2,
    BF_EXPLOSIVE = 1 << 3,   // detonates on hp<=0
    BF_NOGRAV    = 1 << 4,
    BF_DRIVEN    = 1 << 5,   // hinge being opened/closed
    BF_HINGED    = 1 << 6,
    BF_NOSPLIT   = 1 << 7,
};

struct Body {
    Vec3 pos, vel, angVel;
    Quat rot;
    Vec3 half{0.2f, 0.2f, 0.2f};
    f32 mass = 1.f, invMass = 1.f;
    f32 friction = 0.6f, restitution = 0.12f;
    u8  mat = MAT_WOOD;
    u8  kind = BK_PROP;
    u16 flags = 0;
    u16 hp = 100, maxHp = 100;
    u16 col = 0;              // rgb565 tint (0 = pure material texture)
    u8  explosiveForce = 0;   //0=none,1 small,2 medium,3 large
    u8  pad = 0;
    u32 tag = 0;              // mission tag (0=none)
    u32 id = 0;
    f32 sleepT = 0;
    f32 groundT = 0;
    // hinge (doors): pivot in world space
    Vec3 hingePivot;
    Vec3 hingeAxis{0, 1, 0};
    f32  hingeAngle = 0, hingeTarget = 0;
    Vec3 hingeLocalOffset;    // body-center offset from pivot at angle0
    Quat hingeRot0;           // orientation at angle0

    f32 boundingR() const { return len(half); }
};

struct Fragment {
    Vec3 pos, vel, angVel;
    Quat rot;
    ClusterGrid grid;                    // padded local voxels
    std::vector<u8>  mverts;             // local mesh (MVert[])
    std::vector<u32> midx;
    std::vector<u8>  tv; std::vector<u32> ti;  // transparent sub (rare)
    Vec3 localCenter;                    // offset of grid centre inside body frame
    f32 mass = 1.f, invMass = 1.f;
    f32 friction = 0.7f, restitution = 0.08f;
    int voxCount = 0;
    u32 id = 0;
    u32 gpuHandle = 0;    // renderer mesh (created lazily by game)
    u32 meshRev = 1;      // bumped when CPU mesh rebuilt
    u8  state = 0;    //0 falling,1 settling,2 sleeping (not voxelised)
    f32 contactT = 0, sleepT = 0;
    Vec3 lastSplitPoint; f32 splitCool = 0;

    Vec3 worldCenter() const { return pos; }
    AABB worldAabb() const {
        Mat4 m = rot.toMat();
        f32 hx = grid.w * VOX * 0.5f, hy = grid.h * VOX * 0.5f, hz = grid.d * VOX * 0.5f;
        Vec3 e(std::fabs(m.m[0]) * hx + std::fabs(m.m[4]) * hy + std::fabs(m.m[8]) * hz,
               std::fabs(m.m[1]) * hx + std::fabs(m.m[5]) * hy + std::fabs(m.m[9]) * hz,
               std::fabs(m.m[2]) * hx + std::fabs(m.m[6]) * hy + std::fabs(m.m[10]) * hz);
        return AABB{pos - e, pos + e};
    }
    Voxel at(int x, int y, int z) const { return grid.get(x, y, z); }
};

enum ConstraintType : u8 { CON_ROPE = 0, CON_TOW };

struct Constraint {
    u32  bodyA = 0, bodyB = 0;    //0 = static world point
    Vec3 pointA, pointB;          // used when static
    Vec3 localA, localB;          // local offsets when attached to a body
    f32  restLen = 1.f;
    f32  maxLen = 1.f;
    u8   type = CON_ROPE;
};

enum ImpactType : u8 {
    IMP_VOX = 0, IMP_BODY, IMP_FRAG, IMP_GROUND, IMP_FLESH
};
struct ImpactEvent {
    Vec3 pos, nrm;
    f32 energy;         // rough magnitude (J-ish)
    u8  mat;            // material struck
    u8  type;           // ImpactType
    u32 bodyId;         // for IMP_BODY (0 if none)
};
struct ExplodeEvent {
    Vec3 pos;
    f32  force;
    u8   level;         // explosiveForce of source (0 for weapon)
    u8   pad[3];
};
struct BreakEvent {
    Vec3 pos;
    u8   mat;
    u8   kind;
    u16  pad;
    u32  bodyId;
};

class Physics {
public:
    VoxWorld* world = nullptr;
    std::vector<Body> bodies;
    std::vector<Fragment> frags;
    std::vector<Constraint> cons;
    std::vector<ImpactEvent> impacts;
    std::vector<ExplodeEvent> explodes;
    std::vector<BreakEvent> breaks;

    f32 gravity = -19.f;
    int maxBodies = 600;
    int maxFrags = 90;
    Rng rng{12345};

    void bind(VoxWorld* w) { world = w; }
    void clear() { bodies.clear(); frags.clear(); cons.clear(); impacts.clear(); explodes.clear(); breaks.clear(); nextId = 1; }

    u32 addBody(const Body& b);
    void removeBody(u32 id);
    Body* findBody(u32 id);
    u32 spawnFragment(const Cluster& cl, Vec3 vel = Vec3(), Vec3 angVel = Vec3());
    u32 spawnFragmentBox(Vec3 pos, Vec3 half, u8 mat, u16 col, Vec3 vel = Vec3()); // single "voxel-ish" slab for debris
    void removeFragment(u32 id);

    void step(f32 dt);

    // queries
    RayHit raycast(Vec3 o, Vec3 d, f32 maxT, Body** hitBody = nullptr, Fragment** hitFrag = nullptr,
                   u32 skipBodyId = 0) const;
    int overlapBodies(const AABB& box, u32* out, int maxOut, u32 skipId = 0) const;

    // gameplay hooks
    void explosionAt(Vec3 pos, f32 radius, f32 strength, u8 srcLevel = 0, bool damageProps = true);
    void pushAt(Vec3 pos, f32 radius, Vec3 impulse);           // pure impulse (vehicle impacts, grapple)
    void damageBody(u32 id, int amount, Vec3 srcPos);
    // directional hit: ray vs bodies & fragments (nearest). outFragIdx = index into frags, -1 if body.
    bool rayBodies(Vec3 origin, Vec3 dir, f32 maxT, u32& outBodyId, int& outFragIdx, f32& outT, Vec3& outN);
    // impulse at a world point (body velocity + angular kick)
    void applyImpulseAt(u32 id, Vec3 imp, Vec3 worldPt);
    void applyImpulseFrag(int fragIdx, Vec3 imp, Vec3 worldPt);
    // projectile pinning: tether body to a static world point (harpoon)
    void pinTo(u32 bodyId, Vec3 worldPt, f32 slack = 0.02f);
    void solveConstraints();
    void wakeNear(Vec3 pos, f32 radius);

    size_t byteSize() const;

private:
    u32 nextId = 1;
    void stepBodies(f32 dt);
    void stepFrags(f32 dt);
    void stepConstraints(f32 dt);
    void stepBodyPairs(f32 dt);
    void collideBodyWorld(Body& b, bool allowDamage);
    void collideFragWorld(Fragment& f, std::vector<ImpactEvent>& localImpacts);
    bool voxelizeFragment(int fi);
    bool trySplitFragment(int fi, Vec3 planePoint, Vec3 planeNrm);
    Quat nearestAxisQuat(const Quat& q) const;
};

// world voxel damage helper shared by tools: carve with material resistance.
// returns number of voxels destroyed. pushes events into `impacts` optionally.
int carveWorld(VoxWorld& w, Vec3 center, f32 radius, f32 power, f32 matBias,
               EditCause cause, std::vector<Voxel>* removedMats = nullptr, Rng* rng = nullptr);

} // namespace ft
