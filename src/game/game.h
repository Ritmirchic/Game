// FRACTURE — game orchestration: modes, loading, simulation, HUD, menus, saving.
#pragma once
#include "core/engine.h"
#include "ui/ui.h"
#include "world/voxel.h"
#include "world/physics.h"
#include "game/player.h"
#include "game/tools.h"
#include "game/fx.h"
#include "game/vehicles.h"
#include "game/maps.h"
#include "game/mission.h"
#include "game/security.h"
#include <string>
#include <vector>

enum GameMode {
    MODE_MAIN_MENU = 0,
    MODE_CAMPAIGN,
    MODE_SANDBOX_SETUP,
    MODE_SETTINGS,
    MODE_CONTROLS,
    MODE_CREDITS,
    MODE_LOADING,
    MODE_PLAYING,
    MODE_PAUSE,
    MODE_MISSION_COMPLETE,
    MODE_MISSION_FAILED,
    MODE_SAVE_LOAD
};

enum PlayMode { PLAY_SANDBOX = 0, PLAY_CAMPAIGN };

struct SandboxOptions {
    bool unlimitedAmmo = false;
    bool invincible = false;
    bool infiniteFuel = false;
    bool fireEnabled = true;
    float gravityScale = 1.0f;
    float explosionPower = 1.0f;
    int debrisDensity = 2;
    bool vehicleSpawning = true;
    bool showHints = true;
    int timeOfDay = 12;
    int weather = -1;   // -1 = map default
};

struct Pickup {
    vec3 pos;
    vec3 vel{0, 0, 0};
    bool active = true;
    int kind = 0;         // 0 evidence crate, 1 ammo, 2 fuel, 3 health
    float bob = 0;
    bool held = false;
    int missionItem = -1;
};

struct Door {
    vec3 hinge;
    vec3 dir;             // direction the door panel extends (unit, horizontal)
    vec3 normal;          // panel facing
    float width = 1.1f, height = 2.2f;
    float open = 0;       // 0 closed, 1 open
    bool opening = false;
    int locked = 0;
    float health = 100;
    int id = 0;
    bool alive = true;
};

struct Toast {
    std::string text;
    float life = 3.0f;
    int kind = 0;   // 0 info, 1 good, 2 bad
    float maxLife = 3.0f;
};

struct CollapseJob {
    AABB box;
    float delay = 0;
};

struct LoadingTask {
    int stage = 0;
    int mapId = 0;
    bool campaign = false;
    int missionId = -1;
    std::vector<uint64_t> chunks;
    size_t chunkIndex = 0;
    float progress = 0;
};

struct GameStats {
    int chunksRendered = 0;
    int trisRendered = 0;
    float frameMs = 0, avgFrameMs = 16.7f;
    int fps = 60;
    float updateMs = 0, renderMs = 0;
    int bodies = 0;
    float destroyedVoxels = 0;
    int explosions = 0;
};

class Game {
public:
    IRenderer* renderer = nullptr;
    Settings settings;
    InputState input;
    UI ui;

    World world;
    Physics physics;
    Player player;
    Tools tools;
    FX fx;
    VehicleManager vehicles;
    SecuritySystem security;
    MapBuildResult mapData;
    EnvState env;
    PlayMode playMode = PLAY_SANDBOX;
    GameMode mode = MODE_MAIN_MENU;
    GameMode prevMode = MODE_MAIN_MENU;
    int currentMap = 0;
    SandboxOptions sandbox;
    std::vector<Pickup> pickups;
    std::vector<Door> doors;
    std::vector<Toast> toasts;
    std::vector<CollapseJob> collapseJobs;
    int drivenVehicle = -1;
    bool vehicleCamThirdPerson = true;
    float vehicleCamYaw = 0;
    float timeScale = 1.0f;
    float menuTime = 0;
    float playTime = 0;
    int framesRendered = 0;
    GameStats stats;
    LoadingTask loading;
    bool menuAfterLoad = true;   // return to the main menu when loading finishes
    bool showDebug = false;
    bool showHelp = false;
    bool loaded = false;
    float loadingProgress = 0;
    int loadingStage = 0;
    int pendingMap = 0;
    int pendingMission = -1;
    std::string statusLine;
    float statusTimer = 0;
    float deathTimer = 0;
    int interactEvents = 0;
    int lastInteractId = 0;
    bool wantsQuit = false;
    float saveCooldown = 0;
    int totalDestroyedVoxels = 0;
    int totalExplosions = 0;
    float missionStartTime = 0;
    bool showMinimap = false;
    float securityHudPulse = 0;
    // menu camera orbit
    float menuOrbit = 0;
    float carryHighlight = 0;
    int highlightedProp = -1;
    float objectiveFlash = 0;
    int lastCompletedObjectives = 0;
    bool missionJustCompleted = false;
    bool sandboxPauseOpenedFromGame = false;
    float hitmarker = 0;
    float damageFlash = 0;
    float vignettePulse = 0;
    std::string missionToast;

    // init / lifecycle
    bool init(IRenderer* r);
    void shutdown();
    void run();
    void update(float dt);
    void render();

    // flow
    void loadMap(int mapId, bool campaign, int missionId);
    void startSandbox(int mapId);
    void startMission(int missionId);
    void returnToMenu();
    void restartLevel();
    void setMode(GameMode m);
    void toast(const std::string& text, int kind = 0);

    // simulation
    void updatePlaying(float dt);
    void updateEnvironment(float dt);
    void updatePlayerAndTools(float dt);
    void updatePickups(float dt);
    void updateDoors(float dt);
    void updateCollapses(float dt);
    void scheduleCollapseAnalysis(const AABB& box);
    void explode(const vec3& center, float radius, int source);
    void carveRegion(const vec3& center, float radius, float power, bool structural, bool ignite);
    void applyAirstrike(const vec3& pos);
    void spawnPickup(vec3 pos, int kind, int missionItem);
    bool saveGame(const std::string& slot);
    bool loadGame(const std::string& slot);
    void resetMap();
    std::string savePath(const std::string& slot) const;
    void buildDoorsFromMap();

    // rendering
    void renderWorldView();
    void renderHUD();
    void drawMainMenu();
    void drawCampaignMenu();
    void drawSandboxSetup();
    void drawSettings();
    void drawControls();
    void drawCredits();
    void drawPauseMenu();
    void drawMissionResult();
    void drawLoading();
    void drawSaveLoad();
    void drawCrosshair();
    void drawObjectivesPanel();
    void drawToolBar();
    void drawWeaponStatus();
    void drawToasts();
    void drawVehicleHUD();
    void drawMinimap();
    void drawInfoPanels();
    void drawInteractionPrompt();
    void drawMenuBackdrop();
    void drawLogo(float cx, float y, float scale);
    void drawWorldEntities();
    void drawChargeMarkers();
    float aspect() const;
    Camera currentCamera() const;
    void applySettings();
    void resize(int w, int h);
    void keyShortcuts();
    bool uiBlocking() const { return mode != MODE_PLAYING; }
    void ensureMapLoadedForMenu();
    const char* modeName() const;
};

extern Game* g_game;
