// ============================================================
// SPACE MINER  -  texture.h
// Procedural art painting helpers: coverage masks, signed distance
// fields, bevel/rim lighting, panel greebles, nebula fills.
// ============================================================
#pragma once
#include "gfx.h"

namespace sm {
namespace tex {

typedef std::vector<Vec2> Poly;
typedef std::vector<Poly> PolySet;

// Rasterise a polygon set with 2x2 supersampling into a coverage mask (0..1).
void rasterizeMask(const PolySet& polys, int w, int h, std::vector<float>& mask, int ss = 2);

// Unsigned distance (pixels) to the nearest edge plus direction to that edge.
void distanceField(const PolySet& polys, int w, int h, const std::vector<float>& mask,
                   std::vector<float>& dist, std::vector<Vec2>& dir);

// Shaded hull: metal plating with bevel, rim light, panel lines, greebles.
struct HullStyle {
    Color base = rgba(120, 130, 145);
    Color dark = rgba(40, 48, 58);
    Color accent = rgba(90, 200, 230);
    Vec2 lightDir = Vec2(-0.55f, -0.83f);
    float panelSize = 18.f;
    float noiseScale = 0.055f;
    float rim = 0.75f;
    float ao = 0.35f;
    float greebleDensity = 0.5f;
    float bevel = 5.f;
    uint32_t seed = 1;
    int greebleCount = 26;
};

void paintHull(Canvas& out, const PolySet& polys, const HullStyle& st);
// Fills the mask area with an arbitrary per-pixel shader over the shaded hull.
void paintBlob(Canvas& out, const PolySet& polys, const HullStyle& st, float blobSeed);

// Organic rock: silhouette from polar noise, height-shaded with craters and mineral veins.
struct RockStyle {
    Color rockLight = rgba(120, 112, 104);
    Color rockDark = rgba(38, 34, 34);
    Color veinColor = rgba(120, 220, 200);
    Color rimColor = rgba(180, 220, 255);
    Vec2 lightDir = Vec2(-0.6f, -0.8f);
    float veinAmount = 0.5f;
    float craterAmount = 0.55f;
    float roughness = 1.f;
    float rim = 0.9f;
    float noiseScale = 0.045f;
    uint32_t seed = 1;
    int spikes = 0;             // sharp crystal spikes
};

void paintRock(Canvas& out, Vec2 centre, float radius, const RockStyle& st);
void paintCrystalCluster(Canvas& out, Vec2 centre, float radius, Color base, Color glow, uint32_t seed);

// Radial gradient disc (for glows / lights), white premultiplied with alpha falloff.
void makeGlow(Canvas& out, int size, float power, Color tint);
// Soft smoke puff
void makeSmoke(Canvas& out, int size, uint32_t seed);
// Spark streak
void makeSpark(Canvas& out, int w, int h, Color head);

// Nebula backdrop layer (transparent, additive-friendly)
void paintNebula(Canvas& out, uint32_t seed, Color c1, Color c2, float density, float scale = 1.f);
// Dense starfield with a few bright stars
void paintStarfield(Canvas& out, uint32_t seed, int count, Color tint);

// Utility: gaussian-ish 3 pass box blur on alpha canvases
void blur(Canvas& c, int radius);

// Screen-space helpers used by menu backgrounds
void lensFlare(Canvas& cv, float x, float y, float size, Color col);

}  // namespace tex
}  // namespace sm
