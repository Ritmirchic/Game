// FRACTURE — the 12 demolition tools. Every tool modifies real voxel geometry or physics.
#pragma once
#include "core/engine.h"
#include "game/globals.h"
#include <vector>
#include <string>

enum ToolId {
    TOOL_NONE = 0,
    TOOL_HAMMER, TOOL_SHOTGUN, TOOL_CHARGES, TOOL_ROCKET, TOOL_BLOWTORCH, TOOL_PLANK,
    TOOL_EXTINGUISHER, TOOL_GRAPPLE, TOOL_CUTTER, TOOL_CABLE, TOOL_DRILL, TOOL_AIRSTRIKE,
    TOOL_COUNT
};

struct ToolDef {
    const char* name;
    const char* shortDesc;
    const char* hint;
    int maxAmmo;
    bool limitedAmmo;
};

const ToolDef& toolDef(int id);

struct PlacedCharge {
    vec3 pos;
    vec3 normal;
    float armTime = 0;
    bool armed = false;
};

struct Rockets {
    vec3 pos, vel;
    float life = 0;
    bool active = false;
    int kind = 0;      // 0 rocket, 1 bomb, 2 grenade
};

struct CableLink {
    vec3 a, b;
    int bodyA = -1, bodyB = -1;
    float restLen = 0;
    bool active = true;
};

struct Airstrike {
    vec3 target;
    float timer = 0;
    int state = 0;      // 0 idle, 1 inbound, 2 striking
    int bombsLeft = 0;
    float bombTimer = 0;
    vec3 planePos;
};

struct PlankPreview {
    bool active = false;
    vec3 a, b;
    vec3 aNormal;
    bool haveA = false;
};

struct ViewModelBox {
    vec3 pos;        // camera space: x right, y up, z forward
    vec3 size;
    vec3 rot;        // euler
    vec3 color;
    float emissive = 0;
    float metal = 0.2f;
    float rough = 0.6f;
};

class Tools {
public:
    int current = TOOL_HAMMER;
    float cooldown = 0;
    int ammo[TOOL_COUNT] = {0};
    float reloadTimer = 0;
    float reloadDuration = 0;
    float swing = 0;              // 0..1 animation phase for melee
    float swingHit = -1;
    float recoil = 0;
    float torchFuel = 1.0f;
    float extinguisherCharge = 1.0f;
    float heat = 0;               // cutter/drill heat
    std::vector<PlacedCharge> charges;
    std::vector<Rockets> projectiles;
    std::vector<CableLink> cables;
    Airstrike airstrike;
    PlankPreview plank;
    bool grappleAttached = false;
    vec3 grapplePoint;
    float grappleRope = 0;
    float grappleAnim = 0;
    float airstrikeCooldown = 0;
    int selectedSlot = 0;
    bool unlimitedAmmo = false;
    bool infiniteFuel = false;
    float hitFlash = 0;
    int lastMaterialHit = 0;
    std::vector<ViewModelBox> vmBoxes;
    float vmSwingT = 0;
    float vmKick = 0;
    float chargePlaceAnim = 0;
    float sprayAmount = 0;
    bool drillActive = false, torchActive = false, cutterActive = false;
    vec3 beamEnd;
    bool beamHit = false;
    float beamTimer = 0;

    void init();
    void unlockAll();
    void select(int id);
    void nextTool(int dir);
    void reset();
    void update(float dt, InputState& in, Player& player, World& world, Physics& phys, bool uiBlocking);
    void fire(InputState& in, Player& player, World& world, Physics& phys, bool secondary);
    void releaseSecondary(Player& player, World& world, Physics& phys);
    void detonateAll(World& world, Physics& phys);
    void reload(Player& player);
    void drawViewModel(IRenderer& renderer, Player& player, const Camera& cam, float aspect);
    float effectiveSpread() const { return 0.028f; }
    bool hasAmmo(int id) const;
    int ammoFor(int id) const;
    void giveAmmo(int id, int n);

private:
    void updateHammer(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateShotgun(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateCharges(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateRocket(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateTorch(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updatePlank(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateExtinguisher(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateGrapple(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateCutter(float dt, InputState& in, Player& p, World& w, Physics& ph, bool drill);
    void updateCable(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateAirstrike(float dt, InputState& in, Player& p, World& w, Physics& ph);
    void updateProjectiles(float dt, World& w, Physics& ph);
    void addBox(vec3 pos, vec3 size, vec3 color, vec3 rot, float emis = 0, float metal = 0.2f, float rough = 0.6f);
    void buildViewModel(Player& p, float dt);
    void beamEffect(World& w, const RayHit& hit, uint8_t kind);
};

// helper used by game for material-based impact sound
SfxId impactSoundForMaterial(uint8_t mat);
SfxId breakSoundForMaterial(uint8_t mat);
