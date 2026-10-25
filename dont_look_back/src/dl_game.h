// ============================================================================
//  dl_game.h : gameplay state, player, anomalies, entities, progression
// ============================================================================
#pragma once
#include "dl_world.h"
#include "nf_audio.h"
#include "nf_platform.h"

namespace dl {

// ------------------------------------------------------------------ game states
enum class GameState {
    Boot, Title, Playing, Paused, Journal, Settings, Document, Ending, Loading, Dead
};

enum class EndingId { None = 0, Escape, Descent, Assimilation };

// ------------------------------------------------------------------ sound bank
struct SoundBank {
    nf::SoundId step[6], stepRun[4], doorCreak, doorSlam, doorLock, keyUnlock;
    nf::SoundId whisper[3], knock[3], radioStatic, scream, elevatorHum, elevatorDing;
    nf::SoundId footstepsBehind, breathing, heartbeat, entityBreath, entityShriek;
    nf::SoundId pickup, paper, uiMove, uiSelect, uiBack, reportGood, reportBad;
    nf::SoundId drone, rumble, waterDrip, pipeBang, electricity, musicBox;
    nf::SoundId windOutside, metalDrag, glassBreak, lightBuzz, whoosh;
    nf::SoundId stingAnomaly, stingEntity, endingWhoosh;
    void build(nf::Audio& a);
};

// ------------------------------------------------------------------ anomaly system
enum class AnomKind { Visual, Audio, Environmental, Entity };

struct AnomalyDef {
    const char* id;
    const char* name;
    const char* clue;          // journal text shown after a correct report
    AnomKind kind;
    const char* roomHint;      // room id used for placement filtering ("" = any)
};

class Game;

struct Anomaly {
    int index = -1;
    bool active = false;
    float timer = 0.0f;
    int stage = 0;
    std::string room;          // room where it was staged
    nf::V3 anchor;
    // captured state for reset
    bool savedVisible = false;
    nf::M4 savedXf;
    int savedMat = -1;
    float savedIntensity = 0.0f;
    nf::V3 savedColor;
    int savedGroup = -1;
    int handle = 0;
    int entityId = -1;
};

// ------------------------------------------------------------------ entity
enum class EntityType { Observer, Listener, Imitator };

struct Entity {
    EntityType type = EntityType::Observer;
    bool active = false;
    nf::V3 pos;
    nf::V3 target;
    float yaw = 0.0f;
    int state = 0;             // 0 idle, 1 hunting, 2 frozen, 3 attacking, 4 fleeing
    float timer = 0.0f;
    float speed = 1.4f;
    float animPhase = 0.0f;
    std::vector<nf::V3> path;
    int pathIdx = 0;
    float repathTimer = 0.0f;
    bool visibleNow = false;
    float visibility = 0.0f;   // 0..1 how much the player can see it
    int soundHandle = 0;
    float attackCooldown = 0.0f;
    float frozenTime = 0.0f;
    float scale = 1.0f;
};

// ------------------------------------------------------------------ player
struct Player {
    nf::V3 pos{0, 0, 1.62f};
    nf::V3 vel;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float eyeHeight = 1.62f;
    float radius = 0.28f;
    float height = 1.78f;
    float bob = 0.0f, bobPhase = 0.0f;
    float stamina = 1.0f;
    bool grounded = false;
    bool running = false;
    bool crouching = false;
    float stepTimer = 0.0f;
    float breathTimer = 0.0f;
    float noise = 0.0f;        // current noise level for the Listener
    float fear = 0.0f;         // 0..1 drives post effects
    float damageFlash = 0.0f;
    float shakeT = 0.0f, shakeAmp = 0.0f;
    bool hasFlashlight = false;
    bool flashlightOn = false;
    float battery = 1.0f;
    bool hasMaintenanceKey = false;
    bool hasFuse = false;
    bool leverPulled = false;
    float lookBackCount = 0.0f;
};

// ------------------------------------------------------------------ HUD / UI data
struct Toast {
    std::string text;
    float t = 0.0f;
    float life = 3.0f;
    nf::Color col = 0xFFFFFFFF;
    bool sub = false;
};

struct JournalEntry {
    std::string title;
    std::string body;
    AnomKind kind = AnomKind::Visual;
    float time = 0.0f;
    bool correct = true;
};

// ------------------------------------------------------------------ the game
class Game {
public:
    // ---- lifetime
    bool init(nf::Renderer& r, nf::Platform& p, nf::Audio& a, const std::string& assetDir);
    void shutdown();
    void setFrameTarget(int fps);
    void update(float dt);
    void render();
    void onResize(int w, int h);
    bool teleportTo(const std::string& roomId);
    void runHeadless(int frames, const std::string& shotPrefix);

    // ---- settings
    struct Settings {
        float mouseSensitivity = 1.0f;
        float masterVolume = 0.9f;
        float sfxVolume = 1.0f;
        float ambientVolume = 0.8f;
        bool invertY = false;
        bool fullscreen = false;
        float resolutionScale = 0.85f;   // ceiling for the adaptive controller
        float brightness = 1.0f;
        float grain = 0.022f;
        int quality = 1;           // 0 low, 1 medium, 2 high
        int targetFps = 60;        // frame-time target for the adaptive resolution
        bool adaptiveQuality = true;
        bool headBob = true;
    } settings;

    // ---- debug / test hooks
    bool forceAnomaly(int index, bool on);
    int activeAnomalyCount() const;
    const AnomalyDef& anomalyDef(int i) const;
    static int anomalyCount();
    void setGodMode(bool g) { godMode_ = g; }
    bool saveGame();
    bool loadGame();
    std::string lastTestLog;

    // ---- runtime access for tests
    const Player& player() const { return pl_; }
    Player& playerRef() { return pl_; }
    World& world() { return world_; }
    GameState state() const { return state_; }
    int reported() const { return reported_; }
    int mistakes() const { return mistakes_; }
    int cycle() const { return cycle_; }
    EndingId ending() const { return ending_; }
    const std::vector<Entity>& entities() const { return ents_; }
    void forceEnding(EndingId e) { triggerEnding(e); }
    void forceReport() { reportAnomaly(); }
    // ---- test hooks (used by the built-in self test)
    void testStartNewGame() { startNewGame(); }
    void testReturnToTitle() { state_ = GameState::Title; ending_ = EndingId::None; endingTimer_ = 0.0f; menuIndex_ = 0; }
    void testSetState(GameState s) { state_ = s; }
    void testOpenDoc(int i) { openDocument_ = i; if (i >= 0 && i < (int)world_.documents.size()) world_.documents[i].found = true; state_ = GameState::Document; }
    void testSetMenu(int i) { menuIndex_ = i; }
    void testForceEndingScreen(EndingId e) { ending_ = e; endingTimer_ = 0.0f; state_ = GameState::Ending; }
    void testLook(float yaw, float pitch) { pl_.yaw = yaw; pl_.pitch = pitch; }
    bool testVisible(const nf::V3& p, float& d) const { return isPointVisible(p, d); }
    void testNoise(float loud, const nf::V3& at) { makeNoise(loud, at); }
    bool testAnomalyActive(int i) const { return i >= 0 && i < (int)anoms_.size() && anoms_[i].active; }
    int  testActiveAnomalyIndex() const { return activeAnomIndex_; }
    const std::vector<JournalEntry>& testJournal() const { return journal_; }
    int testAudioVoices() const;
    std::string testWorldSignature();

private:
    // ---- subsystems
    void buildUi();
    void updatePlayerReal(float dt, nf::Input& in);
    void updateCamera(float dt);
    void useWorldBaseState();
    void addJournalItem(const std::string& title, const std::string& body, AnomKind k, bool correct);
    void elevatorTravel();
    void finalizeElevatorTravel();
public:
    void setInput(nf::Input* in);
private:
    void updateDoors(float dt);
    void updateInteractables(float dt);
    void updateAnomalies(float dt);
    void updateEntities(float dt);
    void updateAudio(float dt);
    void updateHud(float dt);
    void renderWorld();
    void renderHud(nf::Painter& p);
    void renderMenu(nf::Painter& p);
    void renderJournal(nf::Painter& p);
    void renderSettings(nf::Painter& p);
    void renderDocument(nf::Painter& p);
    void renderEnding(nf::Painter& p);
    void drawEntity(nf::Painter* p);
    void buildEntityMesh(const Entity& e, std::vector<std::pair<int, nf::Mesh>>& out);

    // ---- gameplay
    void startNewGame();
    void loadWorld(bool regenerate);
    bool stageAnomalyForRoom(const std::string& roomId, const nf::V3& at);
    void reportAnomaly();
    void wrongReport(const std::string& msg);
    void correctReport(int anomalyIdx);
    void triggerEnding(EndingId e);
    void applyAnomaly(int index, bool on);
    void resetAnomaly(Anomaly& a);
    bool isPointVisible(const nf::V3& p, float& outDot) const;
    bool lineBlocked(const nf::V3& a, const nf::V3& b) const;
    void spawnEntity(EntityType t, const nf::V3& pos);
    void removeEntity(int id);
    void makeNoise(float loudness, const nf::V3& at);
    void toast(const std::string& s, nf::Color c = 0xFFFFFFFF, float life = 3.2f, bool sub = false);
    void interact();
    void toggleFlashlight();
    void playRandom(nf::SoundId* arr, int n, const nf::V3& at, float vol, float range);
    void saveSnapshot(const std::string& name);
    void updateCycleLogic(float dt);
    void roomEntered(const std::string& roomId, const nf::V3& at);

    nf::Renderer* r_ = nullptr;
    nf::Platform* p_ = nullptr;
    nf::Audio* audio_ = nullptr;
    World world_;
    nf::Painter painter_;
    nf::Font fontTitle_, fontBody_, fontSmall_, fontMono_;
    nf::Image uiIcons_;
    SoundBank sfx_;

    GameState state_ = GameState::Boot;
    GameState prevState_ = GameState::Playing;
    Player pl_;
    std::vector<Anomaly> anoms_;
    std::vector<Entity> ents_;
    std::vector<Toast> toasts_;
    std::vector<JournalEntry> journal_;
    std::string assetDir_;
    std::string savePath_;

    // progression
    int reported_ = 0;
    int mistakes_ = 0;
    int cycle_ = 1;
    int pendingAnomalies_ = 0;
    float cycleTimer_ = 0.0f;
    float playTime_ = 0.0f;
    float anomalyCooldown_ = 6.0f;
    std::vector<int> usedAnomalies_;
    std::string currentRoom_;
    std::string lastRoom_;
    EndingId ending_ = EndingId::None;
    float endingTimer_ = 0.0f;
    bool godMode_ = false;

    // interaction
    int hoverInteract_ = -1;
    float hoverTimer_ = 0.0f;
    std::string hoverText_;
    int openDocument_ = -1;
    float docScroll_ = 0.0f;
    int menuIndex_ = 0;
    int settingsIndex_ = 0;
    float uiTime_ = 0.0f;
    float titleFade_ = 0.0f;
    float bootTimer_ = 0.0f;

    // elevator
    struct ElevatorState {
        bool moving = false;
        float t = 0.0f;
        float duration = 3.0f;
        int fromFloor = 3;
        int toFloor = 3;
        bool doorsOpen = false;
        float doorT = 0.0f;
        bool atBasement = false;
    } elev_;

    // anomaly visual state helpers
    int phantomDoorGroup_ = -1;
    int windowViewGroup_ = -1;
    int shadowBlobGroup_ = -1;
    int followEntityId_ = -1;
    bool emergenceActive_ = false;
    float emergenceTimer_ = 0.0f;
    int activeAnomIndex_ = -1;
    bool reportedThisCycle_ = false;
    float lastNoiseTimer_ = 0.0f;

    // flashlight & lights runtime
    float flicker_ = 0.0f;
    float sanityPulse_ = 0.0f;
    float heartbeatTimer_ = 0.0f;
    int ambientLoop_ = 0;
    int droneLoop_ = 0;
    float uiScale_ = 1.0f;
};

} // namespace dl
