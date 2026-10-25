// ============================================================
// SPACE MINER  -  gfx.h
// Software framebuffer + primitive rasteriser + bitmap font.
// ============================================================
#pragma once
#include "core.h"

namespace sm {

// lightweight render profiler (enabled with -DSM_PROFILE)
extern double gProfSections[8];
extern const char* gProfNames[8];
void profMark(int i);
void profFlush();
#ifdef SM_PROFILE
#define PROF(i) profMark(i)
#else
#define PROF(i) ((void)0)
#endif

struct Canvas {
    int w = 0, h = 0;
    std::vector<Color> px;

    Canvas() = default;
    Canvas(int W, int H) { resize(W, H); }
    void resize(int W, int H) { w = W; h = H; px.assign((size_t)W * H, 0u); }

    inline bool inside(int x, int y) const { return (unsigned)x < (unsigned)w && (unsigned)y < (unsigned)h; }
    inline void put(int x, int y, Color c) { if (inside(x, y)) px[(size_t)y * w + x] = c; }
    inline Color get(int x, int y) const { return inside(x, y) ? px[(size_t)y * w + x] : 0u; }
    inline Color& at(int x, int y) { return px[(size_t)y * w + x]; }

    void clear(Color c = 0u);
    void blend(int x, int y, Color c);      // straight alpha
    void additive(int x, int y, Color c);   // additive glow
    void screen(int x, int y, Color c);

    void fillRect(int x, int y, int W, int H, Color c);
    void fillRectF(float x, float y, float W, float H, Color c);
    void rectOutline(float x, float y, float W, float H, float t, Color c);
    void line(Vec2 a, Vec2 b, Color c, float thickness = 1.f);
    void glowLine(Vec2 a, Vec2 b, Color core, float thickness, float glowRadius, float glowIntensity);
    void circleFill(float cx, float cy, float r, Color c);
    void ring(float cx, float cy, float r, float thickness, Color c);
    void arc(float cx, float cy, float r, float a0, float a1, float thickness, Color c);
    void radialGlow(float cx, float cy, float r, Color c, float intensity = 1.f);
    void ellipseGlow(float cx, float cy, float rx, float ry, Color c, float intensity, float angle = 0);
    void polyFill(const Vec2* pts, int n, Color c);
    void polyOutline(const Vec2* pts, int n, Color c, float thickness = 1.f);
    void disc(float x, float y, float r, Color c);
    // draws src at (x,y) with alpha multiplier, straight alpha blend
    void blit(const Canvas& src, int x, int y, float alpha = 1.f);
    void blitAdd(const Canvas& src, int x, int y, float alpha = 1.f);
    void blitRotated(const Canvas& src, Vec2 centre, float angle, float scale, float alpha = 1.f);
    void blitRotatedAdd(const Canvas& src, Vec2 centre, float angle, float scale, float alpha = 1.f);
    void blitScaled(const Canvas& src, int x, int y, int w, int h, float alpha = 1.f, bool additive = false);
    void vGradient(int x, int y, int W, int H, Color top, Color bottom);
    void scanlines(int step, Color c);
    void vignette(float strength);
    void vignetteFast(float strength);
    void downscaleTo(Canvas& dst) const;

    // clipped rect drawing helper
    inline void clipRect(int& x, int& y, int& W, int& H) const {
        if (x < 0) { W += x; x = 0; }
        if (y < 0) { H += y; y = 0; }
        if (x + W > w) W = w - x;
        if (y + H > h) H = h - y;
        if (W <= 0 || H <= 0) { W = 0; H = 0; }
    }
};

// ------------------------------------------------------------ bitmap font (3x5)
int  textWidth(const std::string& s, int scale, int spacing = 1);
int  textHeight(int scale);
void drawText(Canvas& c, int x, int y, const std::string& s, Color col, int scale = 1, int spacing = 1);
void drawTextShadow(Canvas& c, int x, int y, const std::string& s, Color col, Color shadow,
                    int scale = 1, int spacing = 1, int off = 1);
void drawTextCenter(Canvas& c, int cx, int y, const std::string& s, Color col, int scale = 1, int spacing = 1);
void drawTextRight(Canvas& c, int rx, int y, const std::string& s, Color col, int scale = 1, int spacing = 1);
void drawTextGlow(Canvas& c, int x, int y, const std::string& s, Color col, Color glow,
                  int scale = 1, int spacing = 1);

// letter-spaced heading used by menus
void drawHeading(Canvas& c, int cx, int y, const std::string& s, Color col, Color glow, int scale, int spacingPx);

}  // namespace sm
