// FRACTURE — engine facade: settings, input, platform, audio, renderer interface
#pragma once
#include "core/math.h"
#include "world/voxel.h"
#include <string>
#include <vector>
#include <cstdio>

// ---- key codes (values match Win32 virtual-key codes) ----
enum Key {
    K_LBUTTON = 0x01, K_RBUTTON = 0x02, K_MBUTTON = 0x04,
    K_BACK = 0x08, K_TAB = 0x09, K_ENTER = 0x0D, K_SHIFT = 0x10, K_CONTROL = 0x11,
    K_ALT = 0x12, K_ESCAPE = 0x1B, K_SPACE = 0x20, K_PAGEUP = 0x21, K_PAGEDOWN = 0x22,
    K_END = 0x23, K_HOME = 0x24, K_LEFT = 0x25, K_UP = 0x26, K_RIGHT = 0x27, K_DOWN = 0x28,
    K_INSERT = 0x2D, K_DELETE = 0x2E,
    K_MINUS = 0xBD, K_EQUALS = 0xBB, K_COMMA = 0xBC, K_PERIOD = 0xBE, K_SLASH = 0xBF,
    K_0 = 0x30, K_1 = 0x31, K_2 = 0x32, K_3 = 0x33, K_4 = 0x34, K_5 = 0x35, K_6 = 0x36, K_7 = 0x37, K_8 = 0x38, K_9 = 0x39,
    K_A = 0x41, K_B, K_C, K_D, K_E, K_F, K_G, K_H, K_I, K_J, K_K, K_L, K_M, K_N, K_O, K_P, K_Q, K_R, K_S, K_T,
    K_U, K_V, K_W, K_X, K_Y, K_Z,
    K_F1 = 0x70, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_SHIFT_L = 0xA0, K_SHIFT_R = 0xA1, K_CTRL_L = 0xA2, K_CTRL_R = 0xA3
};

struct InputState {
    bool down[256] = {false};
    bool pressed[256] = {false};   // went down this frame
    bool released[256] = {false};
    float mouseDX = 0, mouseDY = 0, wheel = 0;
    float mouseX = 0, mouseY = 0;
    bool mouseCaptured = false;
    char textInput[16] = {0};
    int textLen = 0;
    void clearFrame() {
        std::memset(pressed, 0, sizeof(pressed));
        std::memset(released, 0, sizeof(released));
        mouseDX = mouseDY = wheel = 0;
        textLen = 0;
    }
    bool isDown(int k) const { return down[k & 255]; }
    bool isPressed(int k) const { return pressed[k & 255]; }
    bool isReleased(int k) const { return released[k & 255]; }
};

// ---- settings ----
enum QualityPreset { QP_LOW = 0, QP_MEDIUM, QP_HIGH, QP_ULTRA };

struct Settings {
    int resolutionW = 1600, resolutionH = 900;
    int windowMode = 0;        // 0 windowed, 1 borderless, 2 fullscreen
    bool vsync = true;
    int fpsLimit = 0;
    int qualityPreset = QP_HIGH;
    float renderScale = 1.0f;
    int shadowQuality = 2;     // 0 off, 1 low(1024), 2 med(2048), 3 high(4096)
    int aaMode = 1;            // 0 off, 1 FXAA, 2 MSAA4 (GL)
    bool motionBlur = false;
    int viewDistance = 160;    // metres
    float fov = 78.0f;
    int destructionQuality = 2;  // 0 low .. 3 ultra
    int debrisDensity = 2;       // 0..3
    float physicsQuality = 1.0f; // 0.5,1,1.5,2
    int particleQuality = 2;
    float exposure = 1.0f;
    float mouseSensitivity = 1.0f;
    bool invertY = false;
    float masterVolume = 0.85f;
    float sfxVolume = 1.0f;
    float musicVolume = 0.55f;
    bool fullbrightShadows = false;
    bool showFps = true;
    bool reduceShake = false;
    int rendererMode = 0;       // 0 = GPU (OpenGL), 1 = software reference
    int weatherOverride = -1;   // -1 = map default
    int timeOverride = -1;      // -1 = map default (hours)
};

// ---- environment / map description ----
struct Weather {
    int kind = 0;              // 0 clear, 1 cloudy, 2 rain, 3 storm, 4 fog
    float cloud = 0.25f;
    float rain = 0.0f;
    float wetness = 0.0f;
    float wind = 2.0f;
    float fogDensity = 0.0075f;
};

struct MapInfo {
    int id = 0;
    const char* name = "";
    const char* subtitle = "";
    const char* description = "";
    int sx = 0, sy = 0, sz = 0;
    vec3 spawn{0, 3, 0};
    float spawnYaw = 0;
    vec3 sunDir{0.4f, 0.8f, 0.3f};
    float sunIntensity = 1.0f;
    vec3 sunColor{1.0f, 0.95f, 0.85f};
    vec3 skyTop{0.20f, 0.38f, 0.62f}, skyHorizon{0.62f, 0.72f, 0.82f};
    vec3 ambientSky{0.42f, 0.52f, 0.66f}, ambientGround{0.24f, 0.22f, 0.18f};
    vec3 fogColor{0.62f, 0.70f, 0.78f};
    float fogDensity = 0.006f;
    Weather weather;
    float waterLevel = -1000.0f;
    int defaultHour = 11;
};

// ---- camera ----
struct Camera {
    vec3 pos{0, 2, 0};
    float yaw = 0, pitch = 0;
    float fovY = 78 * DEG2RAD;
    float aspect = 16.0f / 9.0f;
    float zn = 0.05f, zf = 600.0f;
    float roll = 0;
    vec3 forward() const {
        return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
    }
    vec3 right() const {
        return {std::cos(yaw), 0, std::sin(yaw)};
    }
    vec3 up() const { return cross(right(), forward()); }
    mat4 view() const { return matLookAt(pos, pos + forward(), {0, 1, 0}); }
    mat4 proj() const { return matPerspective(fovY, aspect, zn, zf); }
    mat4 viewProj() const { return proj() * view(); }
};

// ---- pixels / UI ----
struct UIVert {
    float x, y;      // pixels
    float u, v;      // atlas uv
    uint32_t color;  // RGBA8
};
struct UIDrawList {
    std::vector<UIVert> verts;
    int screenW = 1280, screenH = 720;
    void reset(int w, int h) { verts.clear(); screenW = w; screenH = h; }
};

struct Particle {
    vec3 pos, vel;
    float size, sizeVel, life, maxLife;
    uint32_t color;
    float gravity;
    uint8_t kind;     // 0 smoke, 1 dust, 2 spark(additive), 3 fire, 4 glass, 5 debris-bit, 6 water splash, 7 smoke-dark
    uint8_t rot;      // random rotation seed
    uint8_t flags;
    uint8_t pad;
};

struct BoxInstance {
    mat4 model;
    vec3 color;
    vec3 emis{0, 0, 0};
    float rough = 0.8f, metal = 0.0f;
};

struct LightInstance {
    vec3 pos;
    vec3 color;
    float radius = 8;
    float intensity = 1;
};

// per-frame dynamic light handed to the renderer (explosions, fire, torch)
struct FXLight {
    vec3 pos;
    vec3 color{1, 0.6f, 0.25f};
    float radius = 8;
};

struct EnvState {
    vec3 sunDir{0.4f, 0.8f, 0.3f};
    vec3 sunColor{1, 0.96f, 0.88f};
    float sunIntensity = 1;
    vec3 skyTop{0.2f, 0.4f, 0.65f}, skyHorizon{0.65f, 0.75f, 0.85f};
    vec3 ambientSky{0.4f, 0.5f, 0.7f}, ambientGround{0.25f, 0.22f, 0.18f};
    vec3 fogColor{0.65f, 0.72f, 0.8f};
    float fogDensity = 0.006f;
    float time = 0;
    float wetness = 0;
    float cloudCover = 0.2f;
    float rainAmount = 0;
    float wind = 1.5f;
    float lightning = 0;      // 0..1 flash
    int weather = 0;
    float exposure = 1;
};

// ---- renderer interface ----
struct MeshGPU {
    uint32_t vbo = 0, ibo = 0, vao = 0;
    int indexCount = 0;
    bool ready = false;
};

class IRenderer {
public:
    virtual ~IRenderer() {}
    virtual bool init(void* hwnd, int w, int h, const Settings& st) = 0;
    virtual void shutdown() = 0;
    virtual void resize(int w, int h) = 0;
    virtual void beginFrame(const Camera& cam, const EnvState& env, const Settings& st) = 0;
    virtual void drawWorld(World& world, const Settings& st) = 0;
    // dynamic mesh: caller keeps handle; upload happens when dirty
    virtual MeshGPU* createMesh() = 0;
    virtual void updateMesh(MeshGPU* m, const std::vector<VoxVert>& verts, const std::vector<uint32_t>& idx) = 0;
    virtual void drawMesh(MeshGPU* m, const mat4& model) = 0;
    virtual void destroyMesh(MeshGPU* m) = 0;
    virtual void drawBoxes(const BoxInstance* boxes, int count) = 0;
    virtual void drawParticles(const Particle* p, int count) = 0;
    virtual void drawWater(World& world, const EnvState& env) = 0;
    virtual void drawOverlay(const UIDrawList& ui) = 0;
    virtual void endFrame() = 0;
    // optional dynamic lighting hooks (GL renderer implements them; software renderer has analytic lights)
    virtual void setFlashlight(bool on, const vec3& pos, const vec3& dir) { (void)on; (void)pos; (void)dir; }
    virtual void setExplosionLights(const FXLight* l, int n) { (void)l; (void)n; }
    virtual void setDamageFx(float d) { (void)d; }
    virtual void setViewportOverride(int x, int y, int w, int h, bool depthClear) = 0;
    virtual void clearViewportOverride() = 0;
    virtual bool screenshot(const char* path) = 0;
    virtual const uint32_t* framebuffer() const { return nullptr; }
    virtual const char* name() const = 0;
    virtual int frameWidth() const = 0;
    virtual int frameHeight() const = 0;
    virtual void setSettings(const Settings& st) = 0;
};

IRenderer* createGLRenderer();
IRenderer* createSoftRenderer();

// font atlas (grayscale, tightly packed, 8-bit) generated from a real font
struct FontAtlas {
    int width = 0, height = 0;
    int cellW = 0, cellH = 0;
    int cols = 0;
    const unsigned char* data = nullptr;
    float uvGlyph(char c, float* w, float* h) const;
};
const FontAtlas& fontAtlas();

// ---- platform ----
namespace platform {
bool init(const char* title, int w, int h, const Settings& st);
void shutdown();
bool poll(InputState& in);
void swapBuffers();
double time();
void sleepMs(int ms);
void setTitle(const char* t);
void* hwnd();
void* glContext();
std::string dataDir();          // writable: %APPDATA%/FRACTURE
std::string exeDir();
void messageBox(const char* title, const char* text);
void setMouseCapture(bool captured);
bool mouseCaptured();
void setFullscreen(int mode, int w, int h);
void toggleFullscreen();
int windowWidth();
int windowHeight();
bool shouldQuit();
void requestQuit();
void presentSoftware(const uint32_t* pixels, int w, int h);
}

void logMsg(const char* fmt, ...);

// persistent configuration (settings.cfg)
namespace cfg {
void save(const Settings& s);
bool load(Settings& s);
}
void logFlush();

// ---- audio ----
enum SfxId {
    SFX_HAMMER_CONCRETE = 0, SFX_HAMMER_BRICK, SFX_HAMMER_WOOD, SFX_HAMMER_METAL, SFX_HAMMER_GLASS,
    SFX_GLASS_BREAK, SFX_CONCRETE_BREAK, SFX_WOOD_BREAK, SFX_METAL_BREAK, SFX_DEBRIS_SMALL, SFX_DEBRIS_BIG,
    SFX_EXPLOSION_SMALL, SFX_EXPLOSION_LARGE, SFX_ROCKET_FIRE, SFX_ROCKET_FLY, SFX_SHOTGUN, SFX_SHOTGUN_PUMP,
    SFX_CHARGE_BEEP, SFX_DETONATE, SFX_TORCH, SFX_DRILL, SFX_CUTTER, SFX_EXTINGUISH, SFX_GRAPPLE,
    SFX_FOOTSTEP_CONCRETE, SFX_FOOTSTEP_WOOD, SFX_FOOTSTEP_METAL, SFX_FOOTSTEP_DIRT, SFX_FOOTSTEP_GLASS,
    SFX_JUMP, SFX_LAND, SFX_UI_CLICK, SFX_UI_HOVER, SFX_UI_BACK, SFX_CRAFT, SFX_PLANK,
    SFX_ENGINE_START, SFX_VEHICLE_LOOP, SFX_IMPACT_VEHICLE, SFX_AIRSTRIKE_FLYBY, SFX_ALARM, SFX_FIRE_LOOP,
    SFX_WIND_LOOP, SFX_RAIN_LOOP, SFX_STRUCTURE_CREAK, SFX_COLLAPSE, SFX_OBJECT_PICKUP, SFX_OBJECT_THROW,
    SFX_PICKUP_ITEM, SFX_MISSION_COMPLETE, SFX_MISSION_FAIL, SFX_RELOAD, SFX_DRYFIRE, SFX_PLANT,
    SFX_COUNT
};

namespace audio {
bool init();
void shutdown();
void mix(float* outStereo, int frames);
void play(SfxId id, vec3 pos, float volume = 1.0f, float pitch = 1.0f, bool loops = false, int loopId = -1);
int playLoop(SfxId id, vec3 pos, float volume = 1.0f, float pitch = 1.0f);   // returns handle
void updateLoop(int handle, vec3 pos, float volume, float pitch);
void stopLoop(int handle);
void setListener(vec3 pos, vec3 fwd, vec3 up);
void setVolumes(float master, float sfx, float music);
void startMusic(int track);
void stopMusic();
void setMusicIntensity(float v);
int activeVoices();
void update(float dt);
}
