// ============================================================
// SPACE MINER  -  game.h
// Core data model: resources, sectors, upgrades, ships, drones,
// enemies, world entities and the Game object.
// ============================================================
#pragma once
#include "core.h"
#include "gfx.h"
#include "input.h"
#include <memory>

namespace sm {

// ---------------------------------------------------------------- resources
enum ResId {
    RES_IRON = 0, RES_COPPER, RES_NICKEL, RES_COBALT, RES_TITANIUM, RES_SILVER,
    RES_GOLD, RES_PLATINUM, RES_URANIUM, RES_CRYSTAL, RES_PLASMA, RES_IRIDIUM,
    RES_DARKMATTER, RES_VOIDCRYSTAL, RES_QUANTUM,
    RES_COUNT
};
struct ResourceDef {
    const char* name;
    const char* code;
    Color color;
    int value;         // credits per unit
    int rarity;        // 0..5
    float hardness;    // mining resistance
    int xp;
};
extern const ResourceDef RESOURCES[RES_COUNT];

// ---------------------------------------------------------------- sectors
struct SectorPalette {
    Color skyTop, skyBottom;
    Color nebA, nebB;
    Color starTint;
    Color fog;
    Color ui;
};
struct SectorDef {
    const char* name;
    const char* subtitle;
    const char* lore;
    Color accent;
    SectorPalette pal;
    float worldRadius;
    int asteroidCount;
    int resList[6];
    int resWeights[6];
    int enemyList[4];
    float danger;         // 0..1 difficulty scalar
    int unlockCost;
    float hazardLevel;    // plasma / storm intensity
    int rockStyle;        // 0 rocky, 1 crystal, 2 metal, 3 plasma
    int nebDensity;
};
extern const SectorDef SECTORS[5];

// ---------------------------------------------------------------- upgrades
enum UpgId {
    UPG_LASER = 0, UPG_SPEED, UPG_CARGO, UPG_ENGINE, UPG_HULL, UPG_SHIELD,
    UPG_ENERGY, UPG_SCANNER, UPG_DRONES, UPG_WEAPON, UPG_MAGNET, UPG_REFINERY,
    UPG_COUNT
};
struct UpgradeDef {
    const char* name;
    const char* desc;
    int maxLevel;
    int baseCost;
    float costScale;
    float perLevel;    // display value of the effect per level
    const char* unit;
};
extern const UpgradeDef UPGRADES[UPG_COUNT];

// ---------------------------------------------------------------- ships
enum ShipId { SHIP_PROSPECTOR = 0, SHIP_HARVESTER, SHIP_LANCER, SHIP_VOIDCRAFT, SHIP_COUNT };
struct ShipDef {
    const char* name;
    const char* role;
    const char* desc;
    int cost;
    float hull, shield, speed, accel, cargo, miningPower, energyRegen, damage, scanRange;
    int hardpoints;      // weapon barrels
    int droneSlots;
    int engineCount;
    Color accent;
    int unlockSector;
};
extern const ShipDef SHIPS[SHIP_COUNT];

// ---------------------------------------------------------------- drones
struct DroneDef {
    const char* name;
    const char* desc;
    int cost;
    float miningPower, speed, capacity;
};
extern const DroneDef DRONES[3];

// ---------------------------------------------------------------- enemies
enum EnemyType { EN_SCOUT = 0, EN_RAIDER, EN_HEAVY, EN_PLASMA, EN_SENTINEL, EN_TYPE_COUNT };
struct EnemyDef {
    const char* name;
    float hull, shield, speed, damage, fireRate, range, xp;
    int reward;
    float size;
    Color tint;
};
extern const EnemyDef ENEMIES[EN_TYPE_COUNT];

// ---------------------------------------------------------------- entities
struct Asteroid {
    Vec2 p;
    float radius = 60.f;
    float angle = 0.f, spin = .1f;
    int sprite = 0;
    float hp = 100.f, maxHp = 100.f;
    int res[2] = {RES_IRON, RES_IRON};
    float amt[2] = {8, 0};
    float flash = 0.f;
    float crack = 0.f;
    bool alive = true;
    bool scanned = false;
    float lastHitT = -10.f;
    float wobble = 0.f;      // rotational kick when hit
    float wobbleV = 0.f;
};

struct Loot {
    Vec2 p, v;
    int res = 0;
    float amount = 1.f;
    float life = 24.f, maxLife = 24.f;
    float spin = 0.f;
    float z = 0.f, vz = 0.f;    // little hop
    bool pulled = false;
};

struct Shot {
    Vec2 p, v;
    float life = 2.f;
    float damage = 10.f;
    bool fromPlayer = false;
    int kind = 0;      // 0 plasma bolt, 1 laser, 2 missile
    Color color = 0;
    float radius = 3.f;
    float angle = 0.f;
    int target = -1;
};

struct Enemy {
    int type = EN_SCOUT;
    Vec2 p, v;
    float angle = 0.f;
    float hp = 40.f, maxHp = 40.f, shield = 0.f, maxShield = 0.f;
    float fireCd = 1.f;
    float hitFlash = 0.f;
    float state = 0.f;       // 0 idle patrol, 1 chase, 2 strafe, 3 flee
    Vec2 patrolA, patrolB;
    float wander = 0.f;
    float wobble = 0.f;
    float reward = 10;
    float xp = 5;
    bool alive = true;
    float spawnT = 0.f;
    int id = 0;
    Vec2 aimJitter;
    float jitterT = 0;
    float deathT = -1.f;
};

enum DroneState { DR_IDLE = 0, DR_SEEK, DR_MINE, DR_RETURN };
struct Drone {
    Vec2 p, v;
    float angle = 0.f;
    int state = DR_IDLE;
    int target = -1;
    float mineProgress = 0.f;
    int cargoType = -1;
    float cargoAmt = 0.f;
    float beamT = 0.f;
    float bob = 0.f;
    float hitFlash = 0.f;
    float tier = 0;
};

struct Particle {
    Vec2 p, v;
    float life = 1.f, maxLife = 1.f;
    float size = 4.f;
    float drag = 1.f;
    Color color = 0;
    int kind = 0;      // 0 soft glow, 1 spark streak, 2 smoke, 3 debris chunk, 4 shock ring, 5 crystal shard
    float rot = 0.f, spin = 0.f;
    float grav = 0.f;
};

struct Hazard {
    Vec2 p;
    float radius = 200.f;
    float phase = 0.f;
    float intensity = 1.f;
    int kind = 0;     // 0 plasma arc, 1 ion cloud, 2 debris field
};

struct AchieveState { bool unlocked = false; float popupT = 0.f; };

struct Settings {
    float masterVolume = .8f;
    float sfxVolume = .9f;
    bool  screenShake = true;
    bool  showFps = true;
    bool  scanlines = false;
    int   quality = 1;        // 0 low, 1 normal, 2 high
    bool  autosave = true;
    float uiScale = 1.f;
    int   fullscreen = 0;    // 0 windowed, 1 borderless
};

struct SaveData {
    int version = 3;
    long long credits = 0;
    long long totalEarned = 0;
    int xp = 0, level = 1;
    int ship = SHIP_PROSPECTOR;
    int unlockedShips = 1;
    int unlockedSectors = 1;
    int currentSector = 0;
    int upgrades[UPG_COUNT] = {};
    int droneTier = 0;
    int droneCount = 0;
    float cargo[RES_COUNT] = {};
    float hull = -1, shield = -1;
    int achMask = 0;
    int achMask2 = 0;
    long long asteroidsMined = 0, enemiesKilled = 0, jumps = 0;
    float playTime = 0;
    Settings settings;
    Vec2 playerPos;
    bool valid = false;
};

// ---------------------------------------------------------------- input actions
enum UiAction { ACT_NONE = 0, ACT_LEFT, ACT_RIGHT, ACT_UP, ACT_DOWN, ACT_SELECT, ACT_BACK, ACT_TAB, ACT_DOCK, ACT_MAP };

struct Game;

// ---------------------------------------------------------------- render layers
struct ShipArt;
struct AssetBank;

// ---------------------------------------------------------------- game
struct HUDState {
    float energy = 1.f, hull = 1.f, shield = 1.f;
    float cargo = 0.f, cargoMax = 1.f;
    bool  mining = false;
    float toastT = 0.f;
    std::string toast;
    Color toastColor = 0;
    float warnT = 0.f;
    std::string warning;
    float hitT = 0.f;
    float levelPopT = 0.f;
    std::string levelPop;
    float dockHintT = 0.f;
    void toastMsg(const std::string& s, Color c, float t = 3.2f) {
        toast = s; toastColor = c; toastT = t;
    }
};

class Game {
public:
    // ---- lifecycle
    bool init(bool headless = false);
    void bootStage(int stage);      // build one asset group (loading screen)
    void bootFinish();              // finish initialisation after all stages
    void shutdown();
    void update(float dt, Input& in);
    void render(Canvas& fb);
    bool wantsQuit() const { return quit; }
    void requestQuit() { quit = true; }

    // ---- asset access
    const AssetBank& art() const { return *assets; }
    AssetBank& artM() { return *assets; }

    // ---- state
    int   screen = 0;              // 0 loading, 1 menu, 2 game, 3 station, 4 map, 5 settings, 6 credits, 7 death, 8 victory
    bool  quit = false;
    bool  headless = false;
    float time = 0.f;
    float uiTime = 0.f;
    float menuAnim = 0.f;
    int   menuIndex = 0;
    int   settingsFrom = 1;
    float fadeT = 0.f;
    int   fadeDir = 0;
    int   pendingScreen = -1;
    float loadProgress = 0.f;
    std::string loadStage;

    // gameplay
    SaveData save;
    int   sector = 0;
    Vec2  playerPos;
    Vec2  playerVel;
    float playerAngle = 0.f;
    float throttle = 0.f;
    float hull = 100.f, hullMax = 100.f;
    float shield = 50.f, shieldMax = 50.f;
    float energy = 100.f, energyMax = 100.f;
    bool energyLocked = false;
    float weaponCd = 0.f;
    float miningBeamT = 0.f;
    int   miningTarget = -1;
    float invuln = 0.f;
    float cargoUsed = 0.f, cargoMax = 100.f;
    Vec2  cameraPos;
    Vec2  cameraShake;
    float zoom = 1.f;
    float flashWhite = 0.f;
    float warpT = 0.f;

    std::vector<Asteroid> asteroids;
    std::vector<Loot> loot;
    std::vector<Shot> shots;
    std::vector<Enemy> enemies;
    std::vector<Drone> drones;
    std::vector<Particle> particles;
    std::vector<Hazard> hazards;
    std::vector<AchieveState> achievements;
    std::vector<Vec2> starsNear;       // parallax star specks

    Canvas bgArena;
    Canvas bgScaled;
    int bgSector = -1;
    Vec2 stationPos;
    bool docked = false;
    float stationAngle = 0.f;
    float stationPulse = 0.f;

    // ui interaction
    int   uiTab = 0;
    int   uiSel = 0;
    float uiScroll = 0.f;
    float uiScrollTarget = 0.f;
    int   hoverId = -1, hotId = -1, activeId = -1;
    float tooltipT = 0.f;
    std::string tooltip;
    std::string statusLine;
    float statusT = 0.f;
    float screenFlash = 0.f;
    int   mapSel = 0;
    float sectorTransition = 0.f;
    int   sectorTransitionDir = 1;
    int   pendingSector = -1;
    float deathT = 0.f;
    std::string deathCause;

    HUDState hud;
    Rng rng;
    float autosaveT = 0.f;
    float playTime = 0.f;
    float saveFlashT = 0.f;
    float tutorialT = 0.f;
    int   tutorialStep = 0;

    // ---- helpers
    void newGame();
    void enterSector(int idx, bool instant = false);
    bool saveGame(bool backup = false);
    bool loadGame();
    bool saveExists();
    void applySettings();
    float cargoFraction() const;
    float totalCargo() const;
    void refundDeath();

    // gameplay actions used by UI and tests
    void sellAll();
    void sellResource(int res);
    bool buyUpgrade(int id);
    bool buyShip(int id);
    bool buyDrone();
    bool unlockSector(int idx);
    bool selectSector(int idx);
    void dock();
    void undock();
    void addCargo(int res, float amt);
    void addXp(int amount);
    void checkAchievements();
    void unlockAchievement(int idx);
    int  droneCount() const { return (int)drones.size(); }
    void rebuildDrones();
    void toast(const std::string& s, Color c, float t = 3.f);

    // stats derived from upgrades
    float statMiningPower() const;
    float statMiningSpeed() const;
    float statCargo() const;
    float statSpeed() const;
    float statAccel() const;
    float statHull() const;
    float statShield() const;
    float statEnergyRegen() const;
    float statScan() const;
    float statDamage() const;
    float statMagnet() const;
    float statRefine() const;
    int   upgradeLevel(int id) const { return save.upgrades[id]; }

    // spawning for tests / gameplay
    int  spawnAsteroid(Vec2 at, float radius, int sectorStyle, bool forceResource = false, int resId = -1);
    int  spawnEnemy(int type, Vec2 at);
    void spawnLoot(Vec2 at, int res, float amt);
    void spawnParticles(Vec2 at, int count, Color c, float speed, int kind, float size);
    void explode(Vec2 at, float scale, Color tint);
    void damageAsteroid(int idx, float dmg, bool fromDrone = false);
    void collectLoot(int idx);
    void damagePlayer(float dmg, const char* cause);
    void respawn();
    void damageEnemy(int idx, float dmg);
    void killEnemy(int idx);

    // world
    void generateSector(int idx);
    void spawnAmbient(float dt);
    void updateBackground(float dt);
    float backgroundTime = 0.f;

    // render
    void buildBackground();
    void renderWorld(Canvas& fb);
    void renderEntityLayer(Canvas& fb);
    void renderBeams(Canvas& fb);
    void renderStarfield(Canvas& fb);
    void renderHUD(Canvas& fb);
    void renderMenu(Canvas& fb);
    void renderStation(Canvas& fb);
    void renderMap(Canvas& fb);
    void renderSettings(Canvas& fb);
    void renderLoading(Canvas& fb);
    void renderDeath(Canvas& fb);
    void renderVictory(Canvas& fb);
    void renderAchievements(Canvas& fb);
    float lastDt = 1.f / 60.f;
    void updateParticles(float dt);
    void updateDrones(float dt);
    void updatePlay(float dt, Input& in);
    void updateEnemies(float dt);

    // ui helpers (ui.cpp)
    bool button(Canvas& fb, int id, int x, int y, int w, int h, const std::string& label,
                Color accent, bool enabled = true, bool toggled = false);
    void panel(Canvas& fb, int x, int y, int w, int h, Color accent, float alpha = 1.f);
    void screenHeader(Canvas& fb, const std::string& title, const std::string& sub, Color accent);

    bool mouseIn(const Input& in, int x, int y, int w, int h) const {
        return in.mouseX >= x && in.mouseX < x + w && in.mouseY >= y && in.mouseY < y + h;
    }
    int hoverX = 0, hoverY = 0;
    Input* curInput = nullptr;

private:
    std::unique_ptr<AssetBank> assets;
    bool inited = false;
};

}  // namespace sm
