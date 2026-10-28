// FRACTURE — game core (shell that grows with the project)
#pragma once
#include "render.h"
#include "support.h"
#include "ui.h"
#include "settings.h"
#include "input.h"
#include "particles.h"
#include "tools.h"
#include "fire.h"

namespace ft { extern std::string g_dataDir; }

namespace ft {

enum Screen {
    SCR_MENU = 0,
    SCR_CAMPAIGN,
    SCR_SANDBOX_SELECT,
    SCR_SETTINGS,
    SCR_CONTROLS,
    SCR_PLAY,
    SCR_PAUSE,
    SCR_MISSION_BRIEF,
    SCR_MISSION_COMPLETE,
    SCR_MISSION_FAIL,
    SCR_SAVED_LOAD,
};

class Game {
public:
    bool init(IBackend* backend);
    void shutdown();

    // realDt in seconds; input edges prepared by platform
    void tick(f32 realDt, InputState& in);
    void render();

    // scene construction (mapgen registry — "test" scene for now)
    bool buildScene(const char* name, u64 seed);
    // rebuild dirty chunk meshes (budget<0 = flush all); called each tick + before shots
    void flushDirtyMeshes(int budget = -1);
    void respawnPlayer();

    // deterministic fixed-step simulation (used by shots & tests)
    void simStep();

    // shot helper
    void setCamera(Vec3 pos, f32 yawRad, f32 pitchRad);

    // immediate detonation (weapons, tests) — executes carve+chain+FX now
    void detonate(Vec3 pos, f32 force, u8 level = 0);
    void applyExplosion(const ExplodeEvent& e);

    IBackend* rb = nullptr;
    VoxWorld world;
    SupportSystem support;
    Physics physics;
    Settings settings;

    Camera cam;
    SkyParams sky;
    f32 timeOfDay = 10.5f;   // hours

    int screen = SCR_MENU;
    bool playing = false;
    u64 sceneSeed = 1;

    // player (basic state now; full controller in player.cpp)
    Vec3 playerPos;
    Vec3 playerVel;
    f32 playerYaw = 0, playerPitch = 0;
    bool playerGrounded = false;
    f32 camBob = 0;

    // camera math for shots
    void applyLook(f32 dx, f32 dy);

    // frame stats
    f32 fpsSmoothed = 60.f;
    int lastChunksDrawn = 0;
    f32 lastSimMs = 0, lastRenderMs = 0;

    // mission/sandbox placeholders (filled in later phases)
    int currentMapId = -1;
    std::string currentMapName;
    bool wantQuit = false;           // platform exits when set

    // UI hit zones (rebuilt every renderUI; consumed by handleGlobalInput)
    struct UIHit { f32 x, y, w, h; int id; };
    std::vector<UIHit> uiHits_;
    int hitTest(f32 mx, f32 my) const;

    // ---- helpers used across modules ----
    u32 addBoxProp(Vec3 pos, Vec3 half, u8 mat, u16 col, f32 massKg = 0, u8 flags = 0);
    void consumePhysicsEvents();

    // input hookup (called from tick)
    void handleGlobalInput(InputState& in);

    // ---- tools, projectiles, fire (phase D) ----
    void useToolFrame(f32 dt, InputState& in);
    bool fireTool(int tool, Vec3 o, Vec3 dir, bool primary);
    void updateProjectiles(f32 dt);
    void interact();
    void renderViewmodel();
    ToolRuntime& tools() { return tools_; }
    FireSystem& fire() { return fire_; }

private:
    ToolRuntime tools_;
    FireSystem fire_;
    f32 fireAudioT_ = 0.f;
    int fireLoopVoice_ = -1;
    int fireEmitRot_ = 0;
    InputState* curInput_ = nullptr;
    std::unordered_map<u32, u32> fragRev_;   // frag id → last mesh revision
    std::unordered_map<u8, u32> cubeCache_;  // material → unit cube mesh handle
    u32 cubeFor(u8 mat);
    void fixedUpdate();
    void updatePlayer(f32 dt, InputState& in);
    void updateCameraAfterPlayer();
    void submitScene();
    void renderUI();

    f32 simAccum = 0;
    bool simReady = false;
    f32 in_cursorX_ = -9999, in_cursorY_ = -9999;   // last seen cursor (hover)
    std::vector<LightInfo> frameLights;
    SpotInfo flashlight;
    bool flashOn = false;
};

} // namespace ft
