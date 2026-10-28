// FRACTURE — tools: definitions, runtime state, projectiles
#pragma once
#include "common.h"
#include "math.h"
#include "audio.h"
#include <vector>

namespace ft {

enum ToolId : int {
    TOOL_HAND = 0,
    TOOL_SLEDGE,
    TOOL_SAW,
    TOOL_DRILL,
    TOOL_TORCH,
    TOOL_EXTINGUISHER,
    TOOL_PAINT,
    TOOL_PLANK,
    TOOL_SHOTGUN,
    TOOL_RIFLE,
    TOOL_SMG,
    TOOL_GRENADE,
    TOOL_ROCKET,
    TOOL_C4,
    TOOL_HARPOON,
    TOOL_COUNT
};

enum ToolClass : u8 {
    TC_MELEE = 0,
    TC_CUT,          // saw/drill/torch — continuous carve
    TC_SPRAY,        // extinguisher/paint
    TC_PLACE,        // planks / C4
    TC_GUN,          // hitscan
    TC_LAUNCHER,     // projectiles
    TC_REMOTE        // detonate C4
};

struct ToolDef {
    const char* name;
    u8   cls;
    f32  cooldown;      // seconds between primary activations
    f32  range;         // metres
    bool autoFire;      // hold-to-repeat
    SfxId sfx;
    f32  swaySpeed;     // viewmodel bob factor
};

const ToolDef& toolDef(int id);

// wheel-cycle groups (digits map to first10, wheel cycles all)
inline int toolNext(int cur, int dir) {
    int t = cur + dir;
    if (t < 0) t = TOOL_COUNT - 1;
    if (t >= TOOL_COUNT) t = 0;
    return t;
}

struct Projectile {
    Vec3 pos, vel;
    u8  kind = 0;       // 0 grenade, 1 rocket, 2 harpoon
    f32 fuse = -1.f;    // grenade: -1 = unfired (starts on first bounce)
    f32 life = 6.f;
    bool alive = false;
    bool smokeT = false;
};

struct ToolRuntime {
    int   cur = TOOL_HAND;
    f32   cd = 0.f;
    f32   recoil = 0.f;       // visual kick 0..1
    f32   swing = 0.f;        // melee swing anim 0..1
    f32   cutHeat = 0.f;      // continuous tool heat for HUD
    int   paintIdx = 0;       // palette index
    u32   c4Ids[16] = {};
    int   c4Count = 0;
    int   planksLeft = 64;
    f32   sfxLoopT = 0.f;     // continuous sfx cadence
    int   loopVoice = -1;
    std::vector<Projectile> projs;
};

// paint palette (rgb565)
u16 toolPaintColor(int idx);

} // namespace ft
