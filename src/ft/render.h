// FRACTURE — render backend interface (shared by GL and software renderers)
#pragma once
#include "physics.h"

namespace ft {

struct Camera {
    Vec3 pos{0, 1.7f, 0};
    Vec3 fwd{0, 0, -1};
    Vec3 up{0, 1, 0};
    f32 fovY = 1.22f;    // ~70°
    f32 nearZ = 0.06f;
    f32 farZ = 420.f;
    Mat4 view() const { return Mat4::lookDir(pos, fwd, up); }
    Mat4 proj(f32 aspect) const { return Mat4::perspective(fovY, aspect, nearZ, farZ); }
    Vec3 right() const { return norm(cross(fwd, up)); }
};

struct SkyParams {
    Vec3 sunDir{-0.45f, -0.72f, -0.35f};   // direction sunlight travels
    Vec3 sunColor{1.0f, 0.94f, 0.82f};
    f32 sunIntensity = 1.15f;
    Vec3 zenith{0.22f, 0.42f, 0.72f};
    Vec3 horizon{0.68f, 0.76f, 0.84f};
    Vec3 fogColor{0.66f, 0.72f, 0.78f};
    f32 fogDensity = 0.0065f;
    f32 ambient = 1.0f;
    f32 wetness = 0.f;
    f32 exposure = 1.05f;
    u8  weather = 0;     // 0 clear 1 cloudy 2 rain 3 storm 4 fog
    u8  flash = 0;       // explosion flash add
    u8  pad[2] = {0, 0};
};

struct LightInfo {
    Vec3 pos;
    f32 radius = 6.f;
    Vec3 color{1, 1, 1};
    f32 intensity = 1.f;
};

struct SpotInfo {
    Vec3 pos, dir;
    f32 angleCos = 0.88f;
    f32 range = 26.f;
    Vec3 color{1, 0.95f, 0.82f};
    f32 intensity = 0.f;   //0 = off
};

// particle instance (billboard)
struct PInst {
    f32 x, y, z;
    f32 size;
    f32 r, g, b, a;
    f32 rot;
    f32 frame;      // atlas frame (dust/smoke/fire sheets) or0
    u8  kind;       //0 soft puff,1 quad,2 spark streak,3 ember,4 debris-chip(cube-ish)
    u8  pad[3];
};

struct UIQuad {
    f32 x, y, w, h;
    f32 u0, v0, u1, v1;
    u8 r, g, b, a;
    u8 tex;         //0 white solid,1 font atlas,2 material atlas? (kept for flexibility)
    u8 pad[3];
};

class IBackend {
public:
    virtual ~IBackend() {}
    virtual const char* name() const = 0;
    virtual bool init(void* window, int w, int h, int msaa, bool vsync) { (void)window; (void)msaa; (void)vsync; wW = w; wH = h; return true; }
    virtual void shutdown() {}
    virtual void resize(int w, int h) { wW = w; wH = h; }

    // dynamic mesh handles (props / fragments / doors)
    virtual u32 createMesh(const std::vector<u8>& verts, const std::vector<u32>& idx) = 0;
    virtual void updateMesh(u32 h, const std::vector<u8>& verts, const std::vector<u32>& idx) = 0;
    virtual void destroyMesh(u32 h) = 0;

    // scene
    virtual void beginScene(const Camera& cam, f32 aspect, const SkyParams& sky,
                            const std::vector<LightInfo>& lights, const SpotInfo& spot) = 0;
    virtual void drawChunk(const VoxWorld& w, u32 chunkIdx) = 0;
    virtual void drawMesh(u32 h, const Mat4& xform, f32 tintR, f32 tintG, f32 tintB, u8 flags) = 0;
    virtual void drawParticles(const PInst* p, int n) = 0;
    virtual void renderScene() = 0;       // shadow + opaque + transparent + particles (GL); direct draw (SW)
    virtual void beginUI() = 0;
    virtual void pushRect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a) = 0;
    virtual void pushQuad(const UIQuad& q) = 0;
    virtual void renderUI() = 0;
    virtual void present() = 0;
    virtual void readPixels(std::vector<u8>& rgb) = 0;   // RGB888, top-down

    int width() const { return wW; }
    int height() const { return wH; }

protected:
    int wW = 0, wH = 0;
};

IBackend* createSWBackend();
#ifndef FT_HEADLESS_ONLY
IBackend* createGLBackend();   // defined in glbackend.cpp (Windows build)
#endif

// shared material shading parameters (uploaded as uniform array)
struct MatGpu {
    f32 rough, metal, uvScale, emissive;
};
const MatGpu* matGpuTable();     // [256], built from materials.cpp
void buildMatGpuTable();

// texture atlas (RGBA albedo + RGBA normal), built procedurally
struct TexAtlas {
    int tiles = 8;        //8×8 grid
    int tilePx = 128;     // per tile
    std::vector<u8> albedo;   // tiles*tilePx squared ×4
    std::vector<u8> normal;   // ×4
    int size() const { return tiles * tilePx; }
};
bool buildTexAtlas(TexAtlas& out, int tilePx);   // tilePx =128 (high) or64 (low)
const TexAtlas& texAtlas();                       // lazy singleton (high)
void freeTexAtlas();

// particle sprite sheet (dust/smoke/fire) — generated
struct SpriteSheet {
    int w = 256, h = 256, frame = 64;   //4×4 frames of64px
    std::vector<u8> rgba;
};
const SpriteSheet& spriteSheet();
void freeSpriteSheet();

// material tile slot inside the atlas (for CPU/GPU sampling)
int matTileSlot(u8 mat, bool interior);   //0..31 base, +32 interior

// null backend (headless simulation)
IBackend* createNullBackend();

} // namespace ft
