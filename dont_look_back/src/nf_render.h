// ============================================================================
//  nf_render.h : software 3D renderer (z-buffer raster, shadow maps, HDR post)
// ============================================================================
#pragma once
#include "nf_core.h"

namespace nf {

// ------------------------------------------------------------------ mesh
struct Vertex {
    V3 p;
    V3 n;
    float u = 0, v = 0;
    V3 baked;            // x = ambient occlusion, y = wear/grime, z = spare
    Color col = 0xFFFFFFFF;
};

struct Mesh {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    Box3 bounds;
    bool boundsValid = false;

    void clear() { verts.clear(); idx.clear(); boundsValid = false; }
    bool empty() const { return idx.empty(); }
    void addTri(const Vertex& a, const Vertex& b, const Vertex& c);
    void addQuad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d);
    void finalize();
    void transform(const M4& m);
};

// ------------------------------------------------------------------ material
struct Material {
    int tex = -1;
    Color tint = 0xFFFFFFFF;
    float spec = 0.10f;
    float gloss = 18.0f;
    float emis = 0.0f;
    float bump = 0.35f;
    float alpha = 1.0f;
    bool unlit = false;
    bool alphaTest = false;
    float alphaCut = 0.4f;
    bool cull = true;
    bool castShadow = true;
    bool receiveShadow = true;
    bool doubleSided = false;
};

// ------------------------------------------------------------------ texture set
struct TextureSet {
    std::vector<std::vector<Image>> mips;
    int add(const Image& img, bool srgb = true);
    int count() const { return (int)mips.size(); }
};

// ------------------------------------------------------------------ lights
struct PointLight {
    V3 pos;
    V3 color{1, 1, 1};
    float intensity = 1.0f;
    float range = 6.0f;
    float flicker = 0.0f;      // current multiplier (game driven)
    bool enabled = true;
};

struct Flashlight {
    V3 pos;
    V3 dir{0, 0, 1};
    V3 color{1.0f, 0.94f, 0.82f};
    float intensity = 0.0f;
    float inner = 0.80f;       // cos of inner cone
    float outer = 0.55f;       // cos of outer cone
    float range = 16.0f;
    bool on = false;
    float battery = 1.0f;
};

struct FogSettings {
    V3 color{0.020f, 0.024f, 0.030f};
    float density = 0.030f;
    float heightBase = 0.0f;
    float heightFalloff = 0.35f;
    float scatter = 0.30f;     // volumetric beam strength
};

struct PostSettings {
    float exposure = 1.0f;
    float bloom = 0.34f;
    float grain = 0.022f;
    float vignette = 0.34f;
    float aberration = 0.6f;
    float warp = 0.0f;         // screen warp (anomaly / possession)
    float warpPhase = 0.0f;
    float saturation = 0.82f;
    float contrast = 1.06f;
    float lift = 0.012f;
    V3 tintShadow{0.86f, 0.94f, 1.08f};
    V3 tintHigh{1.05f, 1.00f, 0.92f};
    float pulse = 0.0f;        // heartbeat vignette pulse
    float desatPulse = 0.0f;
    float fade = 0.0f;         // black fade 0..1
    V3 fadeColor{0, 0, 0};
    float glitch = 0.0f;
};

// ------------------------------------------------------------------ renderer
class Renderer {
public:
    void init(int w, int h);
    void shutdown();
    void resize(int w, int h);
    int  width() const { return W_; }
    int  height() const { return H_; }
    void setResolutionScale(float s);
    float resolutionScale() const { return scale_; }

    // ---- performance ------------------------------------------------------
    // The renderer keeps a frame-time target and moves the internal resolution
    // (and the shadow-map budget) to hold it, so the game stays playable on
    // weak hardware instead of crawling.
    void setTargetFps(int fps) { targetFps_ = fps > 0 ? fps : 60; }
    void setAdaptive(bool on) { adaptive_ = on; }
    void setQuality(int q);                 // 0 low, 1 medium, 2 high
    int  quality() const { return quality_; }
    struct Stats {
        float frameMs = 0, shadowMs = 0, geomMs = 0, postMs = 0;
        float scale = 1.0f;
        int   threads = 1;
        float fps = 0;
    };
    const Stats& stats() const { return stats_; }

    TextureSet textures;

    // frame setup
    void beginFrame();
    void setCamera(const V3& pos, const V3& forward, const V3& up, float fovYDeg);
    void setAmbient(const V3& color, float strength);
    void setFog(const FogSettings& f) { fog_ = f; }
    FogSettings& fog() { return fog_; }
    PostSettings& post() { return post_; }
    void setFlashlight(const Flashlight& f) { fl_ = f; }
    Flashlight& flashlight() { return fl_; }
    void clearLights() { lights_.clear(); }
    void addLight(const PointLight& l) { if (lights_.size() < 24) lights_.push_back(l); }
    void setSky(const V3& skyColor) { sky_ = skyColor; }

    // submit geometry (may be called any time between beginFrame and render)
    void draw(const Mesh& m, const M4& xf, const Material& mat);

    void render();   // resolve HDR -> framebuffer
    const Color* pixels() const { return fb_.data(); }
    std::vector<Color>& pixelsVec() { return fb_; }

    // helpers used by gameplay (visibility queries)
    bool sampleShadow(const V3& worldPos) const;
    bool sampleShadowN(const V3& worldPos, const V3& n, float ndl) const;
    float shadowMapTexel() const { return 1.0f / (float)smSize_; }
    const M4& shadowMatrix() const { return smMat_; }
    int  drawCallCount() const { return (int)items_.size(); }
    int  triCount() const;
    bool buffersInitialized() const { return !hdr_.empty(); }

private:
    struct Item {
        const Mesh* mesh;
        M4 xf;
        M4 nxf;
        Material mat;
        bool castShadow;
        float distSq = 0.0f;
    };
    static constexpr int SM_MAX = 768;
    int smSize_ = 768;

    int W_ = 0, H_ = 0, RW_ = 0, RH_ = 0;
    float scale_ = 1.0f;
    std::vector<Color> fb_;
    std::vector<float> hdr_;     // 3 floats / px
    std::vector<float> depth_;
    std::vector<float> shadow_;
    std::vector<float> bloomA_, bloomB_;
    std::vector<float> scat_, blurTmp_;
    std::vector<Color> ldr_;            // graded LDR image at render resolution
    std::vector<Color> upscaleTmp_;
    struct Band {
        std::vector<float> depth;       // rows [y0,y1) of the render target
        std::vector<float> hdr;
        int y0 = 0, y1 = 0;
    };
    std::vector<Band> bands_;
    std::vector<Item> items_;

    // performance state
    Stats stats_;
    int   quality_ = 1;
    bool  adaptive_ = true;
    int   targetFps_ = 60;
    float scaleMax_ = 1.0f;
    float frameMsEma_ = 16.0f;
    int   framesSinceTune_ = 0;
    int   shadowEvery_ = 1;
    int   shadowFrame_ = 0;
    int   scatterSteps_ = 10;
    bool  shadowValid_ = false;
    V3    shadowLightPos_;
    V3    shadowLightDir_;


    M4 view_, proj_, viewProj_, smMat_;
    V3 eye_, fwd_, up_, right_;
    float fovY_ = 1.2f, aspect_ = 1.7f, nearZ_ = 0.06f, farZ_ = 90.0f;

    V3 ambient_{0.05f, 0.06f, 0.08f};
    float ambientStrength_ = 1.0f;
    V3 sky_{0.01f, 0.012f, 0.016f};
    FogSettings fog_;
    PostSettings post_;
    Flashlight fl_;
    std::vector<PointLight> lights_;

    float sampleHdr(float x, float y, int ch) const;
    void allocTargets();
    void renderBand(int y0, int y1, float* depthBuf, float* hdrBuf);
    void buildShadowMap();
    void shadowRows(int band, int bands, float* map, int size);
    void rasterShadowTri(const V4* tri, float* map, int size, int yLo, int yHi);
    void postProcess();
    void composite();
    void upscale();
    void tuneQuality();
};

// ------------------------------------------------------------------ 2D painter
struct Font {
    struct Glyph { int x, y, w, h; int ox, oy, adv; };
    std::unordered_map<int, Glyph> glyphs;
    Image atlas;            // alpha in R channel
    int size = 0, ascent = 0, descent = 0, lineHeight = 0;
    int glyphIndex(int cp) const {
        auto it = glyphs.find(cp);
        return it == glyphs.end() ? -1 : (int)0;
    }
    const Glyph* find(int cp) const {
        auto it = glyphs.find(cp);
        return it == glyphs.end() ? nullptr : &it->second;
    }
    int measure(const std::string& s, float scale = 1.0f) const;
    bool load(const std::string& pngPath, const std::string& metaPath);
};

class Painter {
public:
    void begin(Color* fb, int w, int h);
    void clear(Color c);
    void rect(float x, float y, float w, float h, Color c);
    void rectOutline(float x, float y, float w, float h, float th, Color c);
    void gradientV(float x, float y, float w, float h, Color top, Color bottom);
    void line(float x0, float y0, float x1, float y1, float th, Color c);
    void circle(float cx, float cy, float r, Color c);
    void circleOutline(float cx, float cy, float r, float th, Color c);
    void triangle(V2 a, V2 b, V2 c, Color col);
    void blit(const Image& img, float x, float y, float w, float h, Color tint = 0xFFFFFFFF);
    void blitIcon(const Image& img, float x, float y, float w, float h, Color tint);
    void text(const Font& f, const std::string& s, float x, float y, Color c, float scale = 1.0f);
    void textCentered(const Font& f, const std::string& s, float cx, float y, Color c, float scale = 1.0f);
    void textWrap(const Font& f, const std::string& s, float x, float y, float maxW, Color c,
                  float scale = 1.0f, int maxLines = 99);
    int  width() const { return W_; }
    int  height() const { return H_; }
    void setClip(float x, float y, float w, float h) { cx0_ = (int)x; cy0_ = (int)y; cx1_ = (int)(x + w); cy1_ = (int)(y + h); }
    void clearClip() { cx0_ = 0; cy0_ = 0; cx1_ = W_; cy1_ = H_; }
    Color* fb() { return fb_; }

private:
    Color* fb_ = nullptr;
    int W_ = 0, H_ = 0;
    int cx0_ = 0, cy0_ = 0, cx1_ = 0, cy1_ = 0;
    void px(int x, int y, Color c);
    void blendPx(int x, int y, Color c);
};

} // namespace nf
