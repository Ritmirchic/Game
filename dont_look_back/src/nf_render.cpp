// ============================================================================
//  nf_render.cpp : software renderer implementation
//
//  Performance notes (this file is the game's hot path):
//   * geometry is rasterised in horizontal bands, one band per worker thread,
//     with a persistent thread pool (nf::Parallel) instead of thread spawns
//   * the inner scanline loop is fully incremental: no barycentric setup per
//     pixel, no per-pixel sqrt/normalise for the light loop, no per-pixel
//     light iteration over the whole scene (lights are culled per triangle)
//   * shading and tonemapping happen at the internal render resolution, then a
//     bilinear upscale writes the window framebuffer (UI is drawn on top)
//   * the shadow map is a small incremental software rasteriser, multithreaded
//   * frame time drives an adaptive resolution controller (tuneQuality)
// ============================================================================
#include "nf_render.h"
#include <chrono>
#include <cfloat>

namespace nf {

// ------------------------------------------------------------------ mesh
void Mesh::addTri(const Vertex& a, const Vertex& b, const Vertex& c) {
    uint32_t base = (uint32_t)verts.size();
    verts.push_back(a); verts.push_back(b); verts.push_back(c);
    idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
    if (!boundsValid) { bounds = Box3(a.p, a.p); boundsValid = true; }
    bounds.expand(a.p); bounds.expand(b.p); bounds.expand(c.p);
}

void Mesh::addQuad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d) {
    addTri(a, b, c);
    addTri(a, c, d);
}

void Mesh::finalize() {
    if (verts.empty()) return;
    bounds = Box3(verts[0].p, verts[0].p);
    for (auto& v : verts) bounds.expand(v.p);
    boundsValid = true;
}

void Mesh::transform(const M4& m) {
    for (auto& v : verts) {
        v.p = m.point(v.p);
        v.n = normalize(m.dir(v.n));
    }
    boundsValid = false;
    finalize();
}

// ------------------------------------------------------------------ textures
int TextureSet::add(const Image& img, bool /*srgb*/) {
    std::vector<Image> mips;
    img.genMips(mips, 12);
    if (mips.empty()) mips.push_back(img);
    this->mips.push_back(std::move(mips));
    return (int)this->mips.size() - 1;
}

// ------------------------------------------------------------------ helpers
static inline float ndcZtoDepth(float z) { return z * 0.5f + 0.5f; }
static inline bool alphaOpaque(const Material& m) { return m.alpha >= 0.995f && !m.alphaTest; }

// fast inverse square root (one Newton step, ~0.1% error): the inner loop
// normalises the interpolated normal and every light vector, so this replaces
// several sqrt+div pairs per pixel.
static inline float rsqrtFast(float x) {
    if (x < 1e-12f) return 0.0f;
    float x2 = x * 0.5f;
    float y = x;
    uint32_t i;
    memcpy(&i, &y, 4);
    i = 0x5f3759dfu - (i >> 1);
    memcpy(&y, &i, 4);
    return y * (1.5f - x2 * y * y);
}

// ACES tone curve, baked to a LUT (three divides per pixel was measurable)
static constexpr int ACES_LUT_N = 1024;
static constexpr float ACES_MAX = 8.0f;
static float g_acesLut[ACES_LUT_N + 1];
static bool g_acesInit = false;
static void buildAcesLut() {
    for (int i = 0; i <= ACES_LUT_N; ++i) {
        float v = ACES_MAX * (float)i / (float)ACES_LUT_N;
        const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
        g_acesLut[i] = saturate((v * (a * v + b)) / (v * (c * v + d) + e));
    }
    g_acesInit = true;
}
static inline float acesFast(float v) {
    if (v <= 0.0f) return 0.0f;
    float f = v * ((float)ACES_LUT_N / ACES_MAX);
    if (f >= (float)ACES_LUT_N) return g_acesLut[ACES_LUT_N];
    int i = (int)f;
    float t = f - i;
    return g_acesLut[i] + (g_acesLut[i + 1] - g_acesLut[i]) * t;
}

static V3 aces(V3 x) {
    auto f = [](float v) {
        const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
        return saturate((v * (a * v + b)) / (v * (c * v + d) + e));
    };
    return V3(f(x.x), f(x.y), f(x.z));
}

static inline float linToSrgb(float v) {
    v = saturate(v);
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// forward gamma LUT (linear -> 8-bit srgb); avoids three pow() per output pixel
static float srgbFwdLut[1025];
static bool srgbFwdInit = false;
static inline float linToSrgbFast(float v) {
    if (!srgbFwdInit) {
        for (int i = 0; i <= 1024; ++i) srgbFwdLut[i] = linToSrgb(i / 1024.0f);
        srgbFwdInit = true;
    }
    float f = saturate(v) * 1024.0f;
    int i = (int)f;
    float t = f - i;
    if (i >= 1024) return srgbFwdLut[1024];
    return srgbFwdLut[i] + (srgbFwdLut[i + 1] - srgbFwdLut[i]) * t;
}

static float srgbLut[256];
static bool srgbLutInit = false;
static void initSrgbLut() {
    if (srgbLutInit) return;
    for (int i = 0; i < 256; ++i) {
        float v = i / 255.0f;
        srgbLut[i] = v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
    }
    srgbLutInit = true;
}

// ------------------------------------------------------------------ fog LUT
// exp() per pixel is far too expensive on a software renderer; the fog curve is
// baked once per frame from the current density.
static constexpr int FOG_LUT_N = 512;
static constexpr float FOG_LUT_MAX = 48.0f;
static float g_fogLut[FOG_LUT_N + 1];
static float g_fogLutDensity = -1.0f;

static void buildFogLut(float density) {
    g_fogLutDensity = density;
    for (int i = 0; i <= FOG_LUT_N; ++i) {
        float d = FOG_LUT_MAX * (float)i / (float)FOG_LUT_N;
        g_fogLut[i] = saturate(1.0f - std::exp(-density * d * 1.55f));
    }
}

static inline float fogFactor(float dist) {
    float f = dist * (float)FOG_LUT_N / FOG_LUT_MAX;
    if (f <= 0.0f) return g_fogLut[0];
    int i = (int)f;
    if (i >= FOG_LUT_N) return g_fogLut[FOG_LUT_N];
    float t = f - i;
    return g_fogLut[i] + (g_fogLut[i + 1] - g_fogLut[i]) * t;
}

// ------------------------------------------------------------------ timing
using Clock = std::chrono::steady_clock;
static inline double msSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// ------------------------------------------------------------------ lifecycle
void Renderer::init(int w, int h) {
    initSrgbLut();
    buildAcesLut();
    W_ = w; H_ = h;
    resize(w, h);
    stats_.threads = Parallel::get().threads();
}

void Renderer::resize(int w, int h) {
    if (w < 8) w = 8;
    if (h < 8) h = 8;
    bool sizeChanged = (w != W_ || h != H_ || fb_.empty());
    W_ = w; H_ = h;
    if (sizeChanged) fb_.assign((size_t)W_ * H_, 0xFF000000);
    allocTargets();
}

void Renderer::allocTargets() {
    RW_ = maxi(64, (int)(W_ * scale_));
    RH_ = maxi(36, (int)(H_ * scale_));
    size_t needPx = (size_t)RW_ * RH_;
    if (hdr_.size() != needPx * 3) hdr_.assign(needPx * 3, 0.0f);
    if (depth_.size() != needPx) depth_.assign(needPx, 1.0f);
    if (ldr_.size() != needPx) ldr_.assign(needPx, 0xFF000000u);
    size_t bw = (size_t)(RW_ / 4 + 1), bh = (size_t)(RH_ / 4 + 1);
    if (bloomA_.size() != bw * bh * 3) bloomA_.assign(bw * bh * 3, 0.0f);
    if (bloomB_.size() != bloomA_.size()) bloomB_.assign(bloomA_.size(), 0.0f);
    if (scat_.size() != bw * bh) scat_.assign(bw * bh, 0.0f);
    if (blurTmp_.size() != bw * bh) blurTmp_.assign(bw * bh, 0.0f);
    if (shadow_.size() != (size_t)smSize_ * smSize_) shadow_.assign((size_t)smSize_ * smSize_, 1.0f);
    shadowValid_ = false;
}

void Renderer::setResolutionScale(float s) {
    float ns = clampf(s, 0.30f, 1.0f);
    if (std::fabs(ns - scale_) < 0.001f) return;
    scale_ = ns;
    allocTargets();
}

void Renderer::setQuality(int q) {
    quality_ = clampi(q, 0, 2);
    if (quality_ == 0) {
        shadowEvery_ = 3;
        scatterSteps_ = 5;
        smSize_ = 448;
    } else if (quality_ == 1) {
        shadowEvery_ = 2;
        scatterSteps_ = 9;
        smSize_ = 640;
    } else {
        shadowEvery_ = 1;
        scatterSteps_ = 14;
        smSize_ = SM_MAX;
    }
    if (shadow_.size() != (size_t)smSize_ * smSize_) shadow_.assign((size_t)smSize_ * smSize_, 1.0f);
    shadowValid_ = false;
}

void Renderer::shutdown() {
    fb_.clear(); hdr_.clear(); depth_.clear(); shadow_.clear();
    bloomA_.clear(); bloomB_.clear(); ldr_.clear(); scat_.clear(); blurTmp_.clear();
    bands_.clear();
}

// ------------------------------------------------------------------ frame setup
void Renderer::beginFrame() {
    items_.clear();
    lights_.clear();
}

void Renderer::setCamera(const V3& pos, const V3& forward, const V3& up, float fovYDeg) {
    eye_ = pos;
    fwd_ = normalize(forward);
    V3 upv = up;
    if (std::fabs(dot(upv, fwd_)) > 0.999f) upv = V3(0, 0, 1);
    right_ = normalize(cross(fwd_, upv));
    up_ = cross(right_, fwd_);
    fovY_ = fovYDeg * DEG;
    aspect_ = (float)RW_ / (float)RH_;
    nearZ_ = 0.045f;
    farZ_ = 80.0f;
    view_ = M4::lookAt(eye_, eye_ + fwd_, up_);
    proj_ = M4::perspective(fovY_, aspect_, nearZ_, farZ_);
    viewProj_ = proj_ * view_;
}

void Renderer::setAmbient(const V3& color, float strength) {
    ambient_ = color;
    ambientStrength_ = strength;
}

void Renderer::draw(const Mesh& m, const M4& xf, const Material& mat) {
    if (m.idx.empty() || !m.boundsValid) return;
    // bounding-sphere frustum test
    V3 c = xf.point(m.bounds.center());
    float r = length(m.bounds.size()) * 0.5f;
    V3 d = c - eye_;
    float z = dot(d, fwd_);
    if (z < -r) return;
    if (z > farZ_ + r) return;
    float tanY = std::tan(fovY_ * 0.5f), tanX = tanY * aspect_;
    float zz = maxf(z, 0.0f);
    if (std::fabs(dot(d, right_)) - r > zz * tanX * 1.15f) return;
    if (std::fabs(dot(d, up_)) - r > zz * tanY * 1.15f) return;
    Item it;
    it.mesh = &m;
    it.xf = xf;
    it.nxf = xf;   // rotation-only scale assumed; normals renormalised in shader
    it.mat = mat;
    it.castShadow = mat.castShadow && shadow_.size() > 0;
    it.distSq = dot(d, d);
    items_.push_back(it);
}

int Renderer::triCount() const {
    int n = 0;
    for (auto& i : items_) n += (int)i.mesh->idx.size() / 3;
    return n;
}

// ============================================================================
//  SHADOW MAP
//  Small, incremental, multithreaded rasteriser.  A thread only ever writes
//  rows inside its own band, so the depth buffer needs no locking.
// ============================================================================
void Renderer::buildShadowMap() {
    Clock::time_point t0 = Clock::now();
    if (!fl_.on || fl_.intensity <= 0.001f) {
        if (shadowValid_) {
            std::fill(shadow_.begin(), shadow_.end(), 1.0f);
            shadowValid_ = false;
        }
        stats_.shadowMs = (float)msSince(t0);
        return;
    }

    float fov = std::acos(clampf(fl_.outer, -0.99f, 0.99f)) * 2.0f * 1.25f;
    fov = clampf(fov, 0.5f, 1.9f);
    M4 v = M4::lookAt(fl_.pos, fl_.pos + fl_.dir, std::fabs(fl_.dir.z) > 0.95f ? V3(0, 1, 0) : V3(0, 0, 1));
    M4 p = M4::perspective(fov, 1.0f, 0.05f, fl_.range * 1.35f + 2.0f);
    smMat_ = p * v;

    // Rebuild policy: every frame at medium/high quality, every other frame at
    // low.  A large jump of the torch forces an immediate rebuild.
    // A torch that moves a few centimetres per frame does not need a fresh
    // shadow map; only a real jump (or a turn) forces one.
    bool moved = !shadowValid_ ||
                 lengthSq(fl_.pos - shadowLightPos_) > 0.12f ||
                 dot(fl_.dir, shadowLightDir_) < 0.985f;
    ++shadowFrame_;
    if (!moved && shadowEvery_ > 1 && (shadowFrame_ % shadowEvery_) != 0) {
        stats_.shadowMs = (float)msSince(t0);
        return;
    }
    shadowLightPos_ = fl_.pos;
    shadowLightDir_ = fl_.dir;

    std::fill(shadow_.begin(), shadow_.end(), 1.0f);
    int nt = mini(Parallel::get().threads(), 4);
    int nb = mini(smSize_ / 64, nt);
    if (nb < 1) nb = 1;
    if (nb == 1) {
        shadowRows(0, 1, shadow_.data(), smSize_);
    } else {
        Parallel::get().run(nb, [&](int b) { shadowRows(b, nb, shadow_.data(), smSize_); });
    }
    shadowValid_ = true;
    stats_.shadowMs = (float)msSince(t0);
}

void Renderer::shadowRows(int band, int bands, float* map, int size) {
    int y0 = size * band / bands;
    int y1 = size * (band + 1) / bands;
    const float nearW = 0.0001f;

    std::vector<V4> clipBuf;
    V4 out[8];
    for (auto& it : items_) {
        if (!it.castShadow) continue;
        V3 c = it.xf.point(it.mesh->bounds.center());
        float rad = maxComp(it.mesh->bounds.size()) * 0.7f;
        float dToItem = length(c - fl_.pos);
        if (dToItem - rad > 12.0f) continue;
        // cone cull: skip anything the torch cannot see
        if (dToItem > rad + 0.05f) {
            float cd = dot((c - fl_.pos) * (1.0f / dToItem), fl_.dir);
            float halfAngle = std::acos(clampf(fl_.outer, -0.99f, 0.99f)) * 1.45f + 0.35f;
            // allow for the item's angular radius
            float angular = std::atan2(rad, maxf(dToItem - rad, 0.25f));
            if (cd < std::cos(minf(halfAngle + angular, 3.0f))) continue;
        }
        const Mesh& m = *it.mesh;
        M4 mvp = smMat_ * it.xf;

        // transform every vertex once
        clipBuf.resize(m.verts.size());
        for (size_t i = 0; i < m.verts.size(); ++i) {
            const V3& p = m.verts[i].p;
            clipBuf[i] = mvp * V4(p.x, p.y, p.z, 1.0f);
        }

        for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
            V4 tri[8];
            int n = 3;
            tri[0] = clipBuf[m.idx[t]];
            tri[1] = clipBuf[m.idx[t + 1]];
            tri[2] = clipBuf[m.idx[t + 2]];
            bool allIn = (tri[0].z + tri[0].w >= nearW) & (tri[1].z + tri[1].w >= nearW) &
                         (tri[2].z + tri[2].w >= nearW);
            if (!allIn) {
                // clip the polygon against the near plane
                int on = 0;
                for (int k = 0; k < n; ++k) {
                    const V4& a = tri[k];
                    const V4& b = tri[(k + 1) % n];
                    float da = a.z + a.w, db = b.z + b.w;
                    if (da >= nearW) { if (on < 8) out[on++] = a; }
                    if ((da >= nearW) != (db >= nearW)) {
                        float tt = da / (da - db);
                        if (on < 8)
                            out[on++] = V4(lerpf(a.x, b.x, tt), lerpf(a.y, b.y, tt),
                                           lerpf(a.z, b.z, tt), lerpf(a.w, b.w, tt));
                    }
                }
                if (on < 3) continue;
                for (int k = 0; k + 2 < on; ++k) {
                    V4 fan[3] = {out[0], out[k + 1], out[k + 2]};
                    rasterShadowTri(fan, map, size, y0, y1);
                }
                continue;
            }
            rasterShadowTri(tri, map, size, y0, y1);
        }
    }
}

// fast incremental triangle fill into the shadow map
static inline void rasterShadowTriImpl(const V4* tri, float* map, int size, int yLo, int yHi) {
    float sx[3], sy[3], sz[3];
    for (int q = 0; q < 3; ++q) {
        if (tri[q].w <= 1e-5f) return;
        float iw = 1.0f / tri[q].w;
        sx[q] = (tri[q].x * iw * 0.5f + 0.5f) * (float)size;
        sy[q] = (0.5f - tri[q].y * iw * 0.5f) * (float)size;
        sz[q] = ndcZtoDepth(tri[q].z * iw);
    }
    float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (area >= -1e-6f) return;      // back face: cannot occlude its own front side
    float inv = 1.0f / area;

    int minX = clampi((int)std::floor(minf(sx[0], minf(sx[1], sx[2]))), 0, size - 1);
    int maxX = clampi((int)std::ceil(maxf(sx[0], maxf(sx[1], sx[2]))), 0, size - 1);
    int minY = clampi((int)std::floor(minf(sy[0], minf(sy[1], sy[2]))), yLo, yHi - 1);
    int maxY = clampi((int)std::ceil(maxf(sy[0], maxf(sy[1], sy[2]))), yLo, yHi - 1);
    if (minX > maxX || minY > maxY) return;

    // w0(x,y) = A0 + B0*x + C0*y,  w1(x,y) = A1 + B1*x + C1*y
    const float A0 = inv * (sx[1] * sy[2] - sx[2] * sy[1]);
    const float B0 = inv * (sy[1] - sy[2]);
    const float C0 = inv * (sx[2] - sx[1]);
    const float A1 = inv * (sx[2] * sy[0] - sx[0] * sy[2]);
    const float B1 = inv * (sy[2] - sy[0]);
    const float C1 = inv * (sx[0] - sx[2]);
    const float dzdx = B0 * (sz[0] - sz[2]) + B1 * (sz[1] - sz[2]);

    for (int y = minY; y <= maxY; ++y) {
        const float py = (float)y + 0.5f;
        float w0 = A0 + B0 * ((float)minX + 0.5f) + C0 * py;
        float w1 = A1 + B1 * ((float)minX + 0.5f) + C1 * py;
        float z = sz[2] + w0 * (sz[0] - sz[2]) + w1 * (sz[1] - sz[2]);
        const size_t row = (size_t)y * size;
        for (int x = minX; x <= maxX; ++x) {
            if (w0 >= -0.0005f && w1 >= -0.0005f && (w0 + w1) <= 1.0005f) {
                float& dst = map[row + x];
                if (z < dst) dst = z;
            }
            w0 += B0; w1 += B1; z += dzdx;
        }
    }
}

void Renderer::rasterShadowTri(const V4* tri, float* map, int size, int yLo, int yHi) {
    rasterShadowTriImpl(tri, map, size, yLo, yHi);
}

bool Renderer::sampleShadow(const V3& wp) const {
    return sampleShadowN(wp, V3(0, 0, 1), 1.0f);
}

bool Renderer::sampleShadowN(const V3& wp, const V3& N, float ndl) const {
    V4 c = smMat_ * V4(wp.x, wp.y, wp.z, 1.0f);
    if (c.w <= 0.0001f) return true;
    float iw = 1.0f / c.w;
    float u = (c.x * iw * 0.5f + 0.5f);
    float v = (0.5f - c.y * iw * 0.5f);
    float z = ndcZtoDepth(c.z * iw);
    if (u < 0.001f || u > 0.999f || v < 0.001f || v > 0.999f) return true;
    float slope = saturate(1.0f - ndl);
    float bias = 0.0012f + 0.004f * (1.0f - saturate(z)) + 0.012f * slope * slope;
    int ix = (int)(u * smSize_), iy = (int)(v * smSize_);
    float zz = z - bias;
    int sx0 = clampi(ix - 1, 0, smSize_ - 2), sy0 = clampi(iy - 1, 0, smSize_ - 2);
    const float* row0 = &shadow_[(size_t)sy0 * smSize_ + sx0];
    const float* row1 = row0 + smSize_;
    float lit = (zz <= row0[0]) + (zz <= row0[1]) + (zz <= row1[0]) + (zz <= row1[1]);
    return lit * 0.25f;
}

// ------------------------------------------------------------------ sampling
// Bilinear, single mip level (trilinear was one of the most expensive things in
// the previous shader and is invisible at this output size).
static inline void sampleTex(const std::vector<Image>& mips, float u, float v, int level,
                             float& r, float& g, float& b, float& a) {
    const Image& im = mips[clampi(level, 0, (int)mips.size() - 1)];
    if (!im.valid()) { r = g = b = 1.0f; a = 1.0f; return; }
    float fu = u * im.w - 0.5f, fv = v * im.h - 0.5f;
    int x0 = (int)std::floor(fu), y0 = (int)std::floor(fv);
    float tx = fu - x0, ty = fv - y0;
    int x1 = clampi(x0 + 1, 0, im.w - 1), y1 = clampi(y0 + 1, 0, im.h - 1);
    x0 = clampi(x0, 0, im.w - 1); y0 = clampi(y0, 0, im.h - 1);
    const Color* row0 = &im.px[(size_t)y0 * im.w];
    const Color* row1 = &im.px[(size_t)y1 * im.w];
    Color c00 = row0[x0], c10 = row0[x1], c01 = row1[x0], c11 = row1[x1];
    float r00 = srgbLut[colR(c00)], r10 = srgbLut[colR(c10)];
    float r01 = srgbLut[colR(c01)], r11 = srgbLut[colR(c11)];
    r = (r00 * (1 - tx) + r10 * tx) * (1 - ty) + (r01 * (1 - tx) + r11 * tx) * ty;
    float g00 = srgbLut[colG(c00)], g10 = srgbLut[colG(c10)];
    float g01 = srgbLut[colG(c01)], g11 = srgbLut[colG(c11)];
    g = (g00 * (1 - tx) + g10 * tx) * (1 - ty) + (g01 * (1 - tx) + g11 * tx) * ty;
    float b00 = srgbLut[colB(c00)], b10 = srgbLut[colB(c10)];
    float b01 = srgbLut[colB(c01)], b11 = srgbLut[colB(c11)];
    b = (b00 * (1 - tx) + b10 * tx) * (1 - ty) + (b01 * (1 - tx) + b11 * tx) * ty;
    a = ((colA(c00) * (1 - tx) + colA(c10) * tx) * (1 - ty) +
         (colA(c01) * (1 - tx) + colA(c11) * tx) * ty) * (1.0f / 255.0f);
}

// ------------------------------------------------------------------ raster types
namespace {

struct CVert {
    V4 c;
    V3 wp;
    V3 n;
    float u = 0, v = 0;
    V3 vcol{1, 1, 1};
    float vz = 0;              // view-space depth, used for fog
};

struct SVert {
    float x, y, z, invW;
    float u, v;
    V3 wp;
    V3 n;
    V3 vcol;
    float vz;
};

// per-draw constants shared by every triangle of one item
struct DrawCtx {
    const std::vector<Image>* tex = nullptr;
    V3 tint{1, 1, 1};
    float tintA = 1.0f;
    float emis = 0.0f;
    float spec = 0.1f;
    float alpha = 1.0f;
    float alphaCut = 0.4f;
    bool unlit = false, alphaTest = false, cull = true, receiveShadow = true;
    V3 ambient{0, 0, 0};
    V3 eye{0, 0, 0};
    Flashlight fl;
    const PointLight* lights = nullptr;
    int nLights = 0;
    V3 fogColor{0, 0, 0};
    float density = 0.03f;
    const M4* smMat = nullptr;
    const float* shadow = nullptr;
    int rw = 0, rh = 0;
    int smSize = 1024;
    bool opaqueDepth = true;
};

} // namespace

static void shadeSpan(const DrawCtx& ctx, const SVert* sv, float area,
                      int yLo, int yHi, int yBase, float* depthBuf, float* hdrBuf);

// ============================================================================
//  MAIN RASTER (one horizontal band)
// ============================================================================
void Renderer::renderBand(int y0, int y1, float* depthBuf, float* hdrBuf) {
    std::vector<CVert> cverts;
    std::vector<CVert> poly, clipped;

    for (auto& it : items_) {
        const Mesh& m = *it.mesh;
        const Material& mat = it.mat;
        M4 mvp = viewProj_ * it.xf;
        const std::vector<Image>* tex = nullptr;
        if (mat.tex >= 0 && mat.tex < (int)textures.mips.size()) tex = &textures.mips[mat.tex];
        if (tex && tex->empty()) tex = nullptr;

        cverts.resize(m.verts.size());
        for (size_t i = 0; i < m.verts.size(); ++i) {
            const Vertex& v = m.verts[i];
            V3 wp = it.xf.point(v.p);
            CVert cv;
            cv.c = mvp * V4(wp.x, wp.y, wp.z, 1.0f);
            cv.wp = wp;
            cv.n = normalize(it.nxf.dir(v.n));
            cv.u = v.u; cv.v = v.v;
            // ambient occlusion is folded into the vertex colour: one less
            // interpolant in the inner loop, visually identical in practice
            float ao = 0.35f + 0.65f * saturate(v.baked.x);
            cv.vcol = colToV3(v.col) * ao;
            cv.vz = dot(wp - eye_, fwd_);
            cverts[i] = cv;
        }

        DrawCtx ctx;
        ctx.tex = tex;
        ctx.tint = colToV3(mat.tint);
        ctx.tintA = colA(mat.tint) / 255.0f * mat.alpha;
        ctx.emis = mat.emis;
        ctx.spec = mat.spec;
        ctx.alpha = mat.alpha;
        ctx.alphaCut = mat.alphaCut;
        ctx.unlit = mat.unlit || mat.emis > 0.9f;
        ctx.alphaTest = mat.alphaTest;
        ctx.cull = mat.cull;
        ctx.receiveShadow = mat.receiveShadow;
        ctx.ambient = ambient_ * ambientStrength_;
        ctx.eye = eye_;
        ctx.fl = fl_;
        ctx.lights = lights_.data();
        ctx.nLights = (int)lights_.size();
        ctx.fogColor = fog_.color;
        ctx.density = fog_.density;
        ctx.smMat = &smMat_;
        ctx.shadow = shadow_.data();
        ctx.rw = RW_;
        ctx.rh = RH_;
        ctx.smSize = smSize_;
        ctx.opaqueDepth = alphaOpaque(mat);

        for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
            const CVert& ca = cverts[m.idx[t]];
            const CVert& cb = cverts[m.idx[t + 1]];
            const CVert& cc = cverts[m.idx[t + 2]];

            // cheap world-space cull: entirely behind the eye?
            if (ca.c.w <= 1e-5f && cb.c.w <= 1e-5f && cc.c.w <= 1e-5f) continue;

            float area0 = 0.0f;
            if (ca.c.w > 1e-5f && cb.c.w > 1e-5f && cc.c.w > 1e-5f) {
                // pure screen-space setup, no clipping
                SVert s[3];
                const CVert* src[3] = {&ca, &cb, &cc};
                for (int k = 0; k < 3; ++k) {
                    float iw = 1.0f / src[k]->c.w;
                    s[k].x = (src[k]->c.x * iw * 0.5f + 0.5f) * (float)RW_;
                    s[k].y = (0.5f - src[k]->c.y * iw * 0.5f) * (float)RH_;
                    s[k].z = ndcZtoDepth(src[k]->c.z * iw);
                    s[k].invW = iw;
                    s[k].u = src[k]->u; s[k].v = src[k]->v;
                    s[k].wp = src[k]->wp;
                    s[k].n = src[k]->n;
                    s[k].vcol = src[k]->vcol;
                    s[k].vz = src[k]->vz;
                }
                if (s[0].y > RH_ && s[1].y > RH_ && s[2].y > RH_) continue;
                if (s[0].y < 0 && s[1].y < 0 && s[2].y < 0) continue;
                area0 = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);
                if (std::fabs(area0) < 1e-6f) continue;
                bool front = area0 < 0.0f;
                if (mat.cull && !front) continue;
                shadeSpan(ctx, s, area0, y0, y1, y0, depthBuf, hdrBuf);
                continue;
            }

            // ---- near-plane clip path (rare: geometry poking through the eye)
            poly.clear();
            poly.push_back(ca); poly.push_back(cb); poly.push_back(cc);
            clipped.clear();
            int n = (int)poly.size();
            for (int k = 0; k < n; ++k) {
                const CVert& a = poly[k];
                const CVert& b = poly[(k + 1) % n];
                float da = a.c.z + a.c.w, db = b.c.z + b.c.w;
                if (da >= 0.0f) clipped.push_back(a);
                if ((da >= 0.0f) != (db >= 0.0f)) {
                    float tt = da / (da - db);
                    CVert m2;
                    m2.c = V4(lerpf(a.c.x, b.c.x, tt), lerpf(a.c.y, b.c.y, tt),
                              lerpf(a.c.z, b.c.z, tt), lerpf(a.c.w, b.c.w, tt));
                    m2.wp = lerp(a.wp, b.wp, tt);
                    m2.n = lerp(a.n, b.n, tt);
                    m2.u = lerpf(a.u, b.u, tt);
                    m2.v = lerpf(a.v, b.v, tt);
                    m2.vcol = lerp(a.vcol, b.vcol, tt);
                    m2.vz = lerpf(a.vz, b.vz, tt);
                    clipped.push_back(m2);
                }
            }
            for (size_t f = 1; f + 1 < clipped.size(); ++f) {
                SVert s[3];
                const CVert& v0 = clipped[0];
                const CVert& v1 = clipped[f];
                const CVert& v2 = clipped[f + 1];
                const CVert* src[3] = {&v0, &v1, &v2};
                bool ok = true;
                for (int k = 0; k < 3; ++k) {
                    if (src[k]->c.w <= 1e-5f) { ok = false; break; }
                    float iw = 1.0f / src[k]->c.w;
                    s[k].x = (src[k]->c.x * iw * 0.5f + 0.5f) * (float)RW_;
                    s[k].y = (0.5f - src[k]->c.y * iw * 0.5f) * (float)RH_;
                    s[k].z = ndcZtoDepth(src[k]->c.z * iw);
                    s[k].invW = iw;
                    s[k].u = src[k]->u; s[k].v = src[k]->v;
                    s[k].wp = src[k]->wp;
                    s[k].n = src[k]->n;
                    s[k].vcol = src[k]->vcol;
                    s[k].vz = src[k]->vz;
                }
                if (!ok) continue;
                float area = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);
                if (std::fabs(area) < 1e-6f) continue;
                if (mat.cull && !(area < 0.0f)) continue;
                shadeSpan(ctx, s, area, y0, y1, y0, depthBuf, hdrBuf);
            }
        }
    }
}

// ============================================================================
//  PIXEL SHADER (one triangle, incremental scanline traversal)
// ============================================================================
namespace {

struct LightSet {
    const PointLight* l[6];
    int n = 0;
};

static inline void gatherLights(const PointLight* lights, int count, const V3& bmin, const V3& bmax, LightSet& out) {
    V3 c = (bmin + bmax) * 0.5f;
    float rad = length((bmax - bmin) * 0.5f);
    out.n = 0;
    for (int i = 0; i < count; ++i) {
        const PointLight& lt = lights[i];
        if (!lt.enabled || lt.intensity <= 0.001f) continue;
        float reach = lt.range + rad;
        if (lengthSq(lt.pos - c) > reach * reach) continue;
        if (out.n < 6) out.l[out.n++] = &lt;
    }
}

static inline float shadowSample(const M4& sm, const float* map, int size, const V3& wp, const V3& N, float ndl) {
    V4 c = sm * V4(wp.x, wp.y, wp.z, 1.0f);
    if (c.w <= 0.0001f) return 1.0f;
    float iw = 1.0f / c.w;
    float u = (c.x * iw * 0.5f + 0.5f);
    float v = (0.5f - c.y * iw * 0.5f);
    if (u < 0.001f || u > 0.999f || v < 0.001f || v > 0.999f) return 1.0f;
    float z = ndcZtoDepth(c.z * iw);
    float bias = 0.0012f + 0.004f * (1.0f - saturate(z)) + 0.012f * saturate(1.0f - ndl);
    float zz = z - bias;
    int ix = (int)(u * size), iy = (int)(v * size);
    int sx0 = clampi(ix - 1, 0, size - 2), sy0 = clampi(iy - 1, 0, size - 2);
    const float* row0 = &map[(size_t)sy0 * size + sx0];
    const float* row1 = row0 + size;
    float lit = (zz <= row0[0]) + (zz <= row0[1]) + (zz <= row1[0]) + (zz <= row1[1]);
    return lit * 0.25f;
}

} // namespace

template <bool TEX, bool CN, bool CC>
static inline void shadeSpanT(const DrawCtx& ctx, const SVert* sv, float area,
                              int yLo, int yHi, int yBase,
                              float* __restrict depthBuf, float* __restrict hdrBuf) {
    const int RW = ctx.rw;
    const SVert& a = sv[0];
    const SVert& b = sv[1];
    const SVert& c = sv[2];

    int minX = clampi((int)minf(a.x, minf(b.x, c.x)), 0, RW - 1);
    int maxX = clampi((int)(maxf(a.x, maxf(b.x, c.x)) + 1.0f), 0, RW - 1);
    int minY = clampi((int)minf(a.y, minf(b.y, c.y)), yLo, yHi - 1);
    int maxY = clampi((int)(maxf(a.y, maxf(b.y, c.y)) + 1.0f), yLo, yHi - 1);
    if (minX > maxX || minY > maxY) return;

    const float inv = 1.0f / area;
    const float A0 = inv * (b.x * c.y - c.x * b.y);
    const float B0 = inv * (b.y - c.y);
    const float C0 = inv * (c.x - b.x);
    const float A1 = inv * (c.x * a.y - a.x * c.y);
    const float B1 = inv * (c.y - a.y);
    const float C1 = inv * (a.x - c.x);
    const float dzdx = B0 * (a.z - c.z) + B1 * (b.z - c.z);
    const float diwdx = B0 * (a.invW - c.invW) + B1 * (b.invW - c.invW);

    int level = 0;
    if (TEX) {
        float duv = std::fabs((a.u - c.u) * (b.v - c.v) - (b.u - c.u) * (a.v - c.v));
        float texels = std::sqrt(std::fabs(duv * inv)) * (float)(*ctx.tex)[0].w;
        level = (int)clampf(std::log2(maxf(1e-4f, texels)) + 0.35f, 0.0f, 8.0f);
    }

    // every per-draw constant is copied into a local before the loop: the pixel
    // stores below write through float pointers, so leaving them as ctx members
    // would force a reload on every pixel (aliasing)
    const std::vector<Image>* tex = TEX ? ctx.tex : nullptr;
    const V3 tint = ctx.tint;
    const float tintA = ctx.tintA;
    const float emis = ctx.emis;
    const float spec = ctx.spec;
    const bool unlit = ctx.unlit;
    const bool alphaTest = ctx.alphaTest;
    const float alphaCut = ctx.alphaCut;
    const bool depthWrite = ctx.opaqueDepth;
    const V3 ambient = ctx.ambient;
    const V3 eye = ctx.eye;
    const Flashlight fl = ctx.fl;
    const bool flOn = fl.on && fl.intensity > 0.0f;
    const V3 fogCol = ctx.fogColor;
    const bool doSpec = spec > 0.002f;
    const bool shadowed = ctx.receiveShadow && ctx.shadow != nullptr;
    const int smSize = ctx.smSize;
    const M4 smMat = *ctx.smMat;
    const float* smBuf = ctx.shadow;

    const float iw0 = a.invW, iw1 = b.invW, iw2 = c.invW;
    const float qu0 = a.u * iw0, qu1 = b.u * iw1, qu2 = c.u * iw2;
    const float qv0 = a.v * iw0, qv1 = b.v * iw1, qv2 = c.v * iw2;
    const float qwx0 = a.wp.x * iw0, qwx1 = b.wp.x * iw1, qwx2 = c.wp.x * iw2;
    const float qwy0 = a.wp.y * iw0, qwy1 = b.wp.y * iw1, qwy2 = c.wp.y * iw2;
    const float qwz0 = a.wp.z * iw0, qwz1 = b.wp.z * iw1, qwz2 = c.wp.z * iw2;
    const float qvz0 = a.vz * iw0, qvz1 = b.vz * iw1, qvz2 = c.vz * iw2;
    const float qnx0 = a.n.x * iw0, qnx1 = b.n.x * iw1, qnx2 = c.n.x * iw2;
    const float qny0 = a.n.y * iw0, qny1 = b.n.y * iw1, qny2 = c.n.y * iw2;
    const float qnz0 = a.n.z * iw0, qnz1 = b.n.z * iw1, qnz2 = c.n.z * iw2;
    const float qcr0 = a.vcol.x * iw0, qcr1 = b.vcol.x * iw1, qcr2 = c.vcol.x * iw2;
    const float qcg0 = a.vcol.y * iw0, qcg1 = b.vcol.y * iw1, qcg2 = c.vcol.y * iw2;
    const float qcb0 = a.vcol.z * iw0, qcb1 = b.vcol.z * iw1, qcb2 = c.vcol.z * iw2;
    const V3 constNormal = a.n;
    const V3 constColor = a.vcol;

    LightSet ls;
    gatherLights(ctx.lights, ctx.nLights,
                 vmin(a.wp, vmin(b.wp, c.wp)), vmax(a.wp, vmax(b.wp, c.wp)), ls);

    for (int y = minY; y <= maxY; ++y) {
        const float py = (float)y + 0.5f;
        const float px0 = (float)minX + 0.5f;
        float w0 = A0 + B0 * px0 + C0 * py;
        float w1 = A1 + B1 * px0 + C1 * py;
        float z = c.z + w0 * (a.z - c.z) + w1 * (b.z - c.z);
        float iw = iw2 + w0 * (iw0 - iw2) + w1 * (iw1 - iw2);
        const size_t row = (size_t)(y - yBase) * RW;

        for (int x = minX; x <= maxX; ++x) {
            float w2 = 1.0f - w0 - w1;
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                const size_t pi = row + (size_t)x;
                if (z < depthBuf[pi]) {
                    const float rcp = 1.0f / iw;
                    float ar = 1.0f, ag = 1.0f, ab = 1.0f, alpha = 1.0f;
                    if (TEX) {
                        sampleTex(*tex, (w0 * qu0 + w1 * qu1 + w2 * qu2) * rcp,
                                  (w0 * qv0 + w1 * qv1 + w2 * qv2) * rcp, level, ar, ag, ab, alpha);
                    }
                    V3 albedo(ar, ag, ab);
                    albedo = albedo * tint;
                    if (CC) albedo = albedo * constColor;
                    else albedo = albedo * V3(w0 * qcr0 + w1 * qcr1 + w2 * qcr2,
                                              w0 * qcg0 + w1 * qcg1 + w2 * qcg2,
                                              w0 * qcb0 + w1 * qcb1 + w2 * qcb2) * rcp;
                    alpha *= tintA;
                    bool keep = true;
                    if (alphaTest && alpha < alphaCut) keep = false;
                    if (keep) {
                        const V3 wp((w0 * qwx0 + w1 * qwx1 + w2 * qwx2) * rcp,
                                    (w0 * qwy0 + w1 * qwy1 + w2 * qwy2) * rcp,
                                    (w0 * qwz0 + w1 * qwz1 + w2 * qwz2) * rcp);
                        V3 N;
                        if (CN) N = constNormal;
                        else {
                            N = V3(w0 * qnx0 + w1 * qnx1 + w2 * qnx2,
                                   w0 * qny0 + w1 * qny1 + w2 * qny2,
                                   w0 * qnz0 + w1 * qnz1 + w2 * qnz2);
                            N = N * rsqrtFast(lengthSq(N));
                        }

                        V3 col = albedo * ambient * (0.55f + 0.45f * saturate(N.z * 0.5f + 0.5f));
                        if (emis > 0.0f) col += albedo * emis;

                        if (!unlit) {
                            V3 V;
                            if (doSpec) {
                                V3 ev = eye - wp;
                                V = ev * rsqrtFast(lengthSq(ev));
                            }
                            if (flOn) {
                                V3 Lv = fl.pos - wp;
                                float d2 = maxf(lengthSq(Lv), 1e-6f);
                                float invDist = rsqrtFast(d2);
                                float dist = d2 * invDist;
                                V3 L = Lv * invDist;
                                float ndl = maxf(dot(N, L), 0.0f);
                                float shadow = 1.0f;
                                if (shadowed) shadow = shadowSample(smMat, smBuf, smSize, wp, N, ndl);
                                if (shadow > 0.0f) {
                                    float cd = -(L.x * fl.dir.x + L.y * fl.dir.y + L.z * fl.dir.z);
                                    float spot = smoothstepf(fl.outer, fl.inner, cd);
                                    float atten = minf(1.0f / (0.42f + d2 * 0.105f), 0.78f) *
                                                  saturate(1.0f - dist / fl.range) *
                                                  smoothstepf(0.12f, 0.85f, dist);
                                    float k = spot * atten * shadow * fl.intensity;
                                    if (k > 0.0f) {
                                        col += albedo * fl.color * (ndl * k);
                                        if (doSpec) {
                                            V3 H = L + V;
                                            float nh = maxf(dot(N, H) * rsqrtFast(maxf(lengthSq(H), 1e-6f)), 0.0f);
                                            float n2 = nh * nh;
                                            float n4 = n2 * n2;
                                            col += fl.color * (n4 * n4 * spec * k * 2.2f);
                                        }
                                    }
                                }
                            }
                            for (int li = 0; li < ls.n; ++li) {
                                const PointLight& lt = *ls.l[li];
                                V3 Lv = lt.pos - wp;
                                float d2 = lengthSq(Lv);
                                if (d2 > lt.range * lt.range) continue;
                                float invDist = rsqrtFast(maxf(d2, 1e-6f));
                                float dist = d2 * invDist;
                                V3 L = Lv * invDist;
                                float atten = minf(1.0f / (0.22f + 0.55f * d2), 1.30f);
                                float edge = saturate(1.0f - dist / lt.range);
                                atten *= edge * edge;
                                float ndl = maxf(dot(N, L), 0.0f);
                                float k = atten * lt.intensity * lt.flicker;
                                if (ndl <= 0.0f || k <= 0.0f) continue;
                                col += albedo * lt.color * (ndl * k);
                                if (doSpec) {
                                    V3 H = L + V;
                                    float nh = maxf(dot(N, H) * rsqrtFast(maxf(lengthSq(H), 1e-6f)), 0.0f);
                                    float n2 = nh * nh;
                                    float n4 = n2 * n2;
                                    col += lt.color * (n4 * n4 * spec * k * 1.6f);
                                }
                            }
                        }

                        float vzt = (w0 * qvz0 + w1 * qvz1 + w2 * qvz2) * rcp;
                        col = lerp(col, fogCol, fogFactor(vzt));

                        const size_t hi = pi * 3;
                        if (alpha >= 0.995f) {
                            hdrBuf[hi] = col.x; hdrBuf[hi + 1] = col.y; hdrBuf[hi + 2] = col.z;
                        } else {
                            hdrBuf[hi] += col.x * alpha;
                            hdrBuf[hi + 1] += col.y * alpha;
                            hdrBuf[hi + 2] += col.z * alpha;
                        }
                        if (depthWrite) depthBuf[pi] = z;
                    }
                }
            }
            w0 += B0; w1 += B1; z += dzdx; iw += diwdx;
        }
    }
}

static void shadeSpan(const DrawCtx& ctx, const SVert* sv, float area,
                      int yLo, int yHi, int yBase, float* depthBuf, float* hdrBuf) {
    const SVert& a = sv[0];
    const SVert& b = sv[1];
    const SVert& c = sv[2];
    const bool cn = dot(a.n, b.n) > 0.9995f && dot(a.n, c.n) > 0.9995f;
    const bool cc = a.vcol.x == b.vcol.x && a.vcol.x == c.vcol.x &&
                    a.vcol.y == b.vcol.y && a.vcol.y == c.vcol.y &&
                    a.vcol.z == b.vcol.z && a.vcol.z == c.vcol.z;
    const bool tx = ctx.tex != nullptr;
    if (tx) {
        if (cn) { if (cc) shadeSpanT<true, true, true>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf);
                  else    shadeSpanT<true, true, false>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf); }
        else { if (cc) shadeSpanT<true, false, true>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf);
               else    shadeSpanT<true, false, false>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf); }
    } else {
        if (cn) { if (cc) shadeSpanT<false, true, true>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf);
                  else    shadeSpanT<false, true, false>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf); }
        else { if (cc) shadeSpanT<false, false, true>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf);
               else    shadeSpanT<false, false, false>(ctx, sv, area, yLo, yHi, yBase, depthBuf, hdrBuf); }
    }
}

// ============================================================================
//  FRAME
// ============================================================================
void Renderer::render() {
    Clock::time_point tFrame = Clock::now();
    if (g_fogLutDensity != fog_.density) buildFogLut(fog_.density);

    buildShadowMap();
    Clock::time_point tShadow = Clock::now();

    // front-to-back: the depth test then rejects most hidden shading work
    std::sort(items_.begin(), items_.end(),
              [](const Item& a, const Item& b) { return a.distSq < b.distSq; });

    int nThreads = mini(Parallel::get().threads(), RH_ / 24);
    if (nThreads < 1) nThreads = 1;
    if ((int)bands_.size() < nThreads) bands_.resize((size_t)nThreads);
    for (int b = 0; b < nThreads; ++b) {
        bands_[b].y0 = RH_ * b / nThreads;
        bands_[b].y1 = RH_ * (b + 1) / nThreads;
    }
    if (nThreads == 1) {
        Band& bd = bands_[0];
        size_t n = (size_t)RH_ * RW_;
        if (bd.depth.size() != n) { bd.depth.assign(n, 1.0f); bd.hdr.assign(n * 3, 0.0f); }
        else { std::fill(bd.depth.begin(), bd.depth.end(), 1.0f);
               std::fill(bd.hdr.begin(), bd.hdr.end(), 0.0f); }
        renderBand(0, RH_, bd.depth.data(), bd.hdr.data());
        memcpy(depth_.data(), bd.depth.data(), n * sizeof(float));
        memcpy(hdr_.data(), bd.hdr.data(), n * 3 * sizeof(float));
    } else {
        Parallel::get().run(nThreads, [&](int b) {
            Band& bd = bands_[b];
            int rows = bd.y1 - bd.y0;
            size_t n = (size_t)rows * RW_;
            if (bd.depth.size() != n) { bd.depth.assign(n, 1.0f); bd.hdr.assign(n * 3, 0.0f); }
            else { std::fill(bd.depth.begin(), bd.depth.end(), 1.0f);
                   std::fill(bd.hdr.begin(), bd.hdr.end(), 0.0f); }
            renderBand(bd.y0, bd.y1, bd.depth.data(), bd.hdr.data());
            memcpy(depth_.data() + (size_t)RW_ * bd.y0, bd.depth.data(), n * sizeof(float));
            memcpy(hdr_.data() + (size_t)RW_ * bd.y0 * 3, bd.hdr.data(), n * 3 * sizeof(float));
        });
    }
    Clock::time_point tGeom = Clock::now();

    postProcess();
    Clock::time_point tEnd = Clock::now();

    stats_.frameMs = (float)std::chrono::duration<double, std::milli>(tEnd - tFrame).count();
    stats_.shadowMs = (float)std::chrono::duration<double, std::milli>(tShadow - tFrame).count();
    stats_.geomMs = (float)std::chrono::duration<double, std::milli>(tGeom - tShadow).count();
    stats_.postMs = (float)std::chrono::duration<double, std::milli>(tEnd - tGeom).count();
    stats_.scale = scale_;
    stats_.threads = Parallel::get().threads();
    stats_.fps = stats_.frameMs > 0.01f ? 1000.0f / stats_.frameMs : 0.0f;
    tuneQuality();

}

// Resolution controller: hold the frame-time target by moving the internal
// render resolution between 40% and the quality ceiling.  Small, infrequent
// steps keep the image stable; the UI always stays at native resolution.
void Renderer::tuneQuality() {
    frameMsEma_ = lerpf(frameMsEma_, stats_.frameMs, 0.08f);
    if (!adaptive_) return;
    if (++framesSinceTune_ < 12) return;
    const float target = 1000.0f / (float)targetFps_;
    if (frameMsEma_ > target * 1.12f) {
        if (scale_ > 0.32f) {
            // converge quickly: big steps while far from the target
            float step = frameMsEma_ > target * 1.8f ? 0.70f
                       : frameMsEma_ > target * 1.35f ? 0.85f : 0.95f;
            scale_ = maxf(0.32f, scale_ * step);
            allocTargets();
            framesSinceTune_ = 0;
        } else if (quality_ > 0) {
            setQuality(quality_ - 1);
            framesSinceTune_ = 0;
        }
    } else if (frameMsEma_ < target * 0.72f) {
        if (scale_ < scaleMax_ - 0.001f) {
            scale_ = minf(scaleMax_, scale_ * 1.06f);
            allocTargets();
            framesSinceTune_ = 0;
        }
    }
}

// ============================================================================
//  POST
// ============================================================================
float Renderer::sampleHdr(float x, float y, int ch) const {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float tx = x - x0, ty = y - y0;
    int x1 = clampi(x0 + 1, 0, RW_ - 1), y1 = clampi(y0 + 1, 0, RH_ - 1);
    x0 = clampi(x0, 0, RW_ - 1); y0 = clampi(y0, 0, RH_ - 1);
    const float* p00 = &hdr_[((size_t)y0 * RW_ + x0) * 3 + ch];
    const float* p10 = &hdr_[((size_t)y0 * RW_ + x1) * 3 + ch];
    const float* p01 = &hdr_[((size_t)y1 * RW_ + x0) * 3 + ch];
    const float* p11 = &hdr_[((size_t)y1 * RW_ + x1) * 3 + ch];
    return (p00[0] * (1 - tx) + p10[0] * tx) * (1 - ty) + (p01[0] * (1 - tx) + p11[0] * tx) * ty;
}

void Renderer::postProcess() {
    const int RW = RW_, RH = RH_;
    const int bw = RW / 4 + 1, bh = RH / 4 + 1;
    const int threads = maxi(1, mini(Parallel::get().threads(), 8));

    // ---------------------------------------------------------- volumetric
    const bool doScatter = fl_.on && fl_.intensity > 0.001f && fog_.scatter > 0.001f;
    if (!doScatter) std::fill(scat_.begin(), scat_.end(), 0.0f);
    else {
        const int steps = scatterSteps_;
        const float tanF = std::tan(fovY_ * 0.5f);
        const V3 eye = eye_, fwd = fwd_, right = right_, up = up_;
        const Flashlight fl = fl_;
        const float aspect = aspect_, nearZ = nearZ_, farZ = farZ_;
        const float scatterAmt = fog_.scatter;
        const float density = fog_.density;
        const float* depth = depth_.data();
        const M4 sm = smMat_;
        const float* smBuf = shadow_.data();
        const int smSize = smSize_;
        float* scat = scat_.data();
        Parallel::get().run(threads, [&](int tid) {
            int y0 = bh * tid / threads, y1 = bh * (tid + 1) / threads;
            for (int y = y0; y < y1; ++y) {
                for (int x = 0; x < bw; ++x) {
                    int sx = mini(x * 4, RW - 1), sy = mini(y * 4, RH - 1);
                    float z = depth[(size_t)sy * RW + sx];
                    float ndcx = ((float)sx + 0.5f) / RW * 2.0f - 1.0f;
                    float ndcy = 1.0f - ((float)sy + 0.5f) / RH * 2.0f;
                    V3 rd = normalize(fwd + right * (ndcx * tanF * aspect) + up * (ndcy * tanF));
                    float zndc = z * 2.0f - 1.0f;
                    float viewZ = (2.0f * nearZ * farZ) / (farZ + nearZ - zndc * (farZ - nearZ));
                    float dist = minf(viewZ / maxf(0.05f, dot(rd, fwd)), scatterAmt * 40.0f + 6.0f);
                    float stepLen = dist / (float)steps;
                    float acc = 0.0f, trans = 1.0f;
                    for (int s = 1; s <= steps; ++s) {
                        float t = stepLen * ((float)s - 0.5f);
                        V3 p = eye + rd * t;
                        float lit = shadowSample(sm, smBuf, smSize, p, V3(0, 0, 1), 1.0f) > 0.5f ? 1.0f : 0.0f;
                        V3 toP = p - fl.pos;
                        float dl = length(toP);
                        V3 L = toP * (1.0f / maxf(dl, 1e-4f));
                        float cone = smoothstepf(fl.outer, fl.inner, dot(L, fl.dir));
                        float falloff = saturate(1.0f - dl / (fl.range * 1.1f));
                        acc += lit * cone * falloff * stepLen * trans;
                        trans *= 1.0f - minf(0.5f, density * stepLen * 1.5f);
                    }
                    scat[(size_t)y * bw + x] = acc * 0.0014f;
                }
            }
        });
        // separable blur to remove step banding
        float* src = scat_.data();
        float* tmp = blurTmp_.data();
        Parallel::get().run(threads, [&](int tid) {
            int y0 = bh * tid / threads, y1 = bh * (tid + 1) / threads;
            for (int y = y0; y < y1; ++y) {
                for (int x = 0; x < bw; ++x) {
                    float a = src[(size_t)y * bw + maxi(0, x - 1)];
                    float b = src[(size_t)y * bw + x];
                    float c = src[(size_t)y * bw + mini(bw - 1, x + 1)];
                    tmp[(size_t)y * bw + x] = (a + b * 2.0f + c) * 0.25f;
                }
            }
        });
        Parallel::get().run(threads, [&](int tid) {
            int y0 = bh * tid / threads, y1 = bh * (tid + 1) / threads;
            for (int y = y0; y < y1; ++y) {
                for (int x = 0; x < bw; ++x) {
                    float a = tmp[(size_t)maxi(0, y - 1) * bw + x];
                    float b = tmp[(size_t)y * bw + x];
                    float c = tmp[(size_t)mini(bh - 1, y + 1) * bw + x];
                    src[(size_t)y * bw + x] = (a + b * 2.0f + c) * 0.25f;
                }
            }
        });
    }

    // ---------------------------------------------------------- bloom source
    {
        const float* hdr = hdr_.data();
        float* bA = bloomA_.data();
        std::fill(bloomA_.begin(), bloomA_.end(), 0.0f);
        Parallel::get().run(threads, [&](int tid) {
            int y0 = bh * tid / threads, y1 = bh * (tid + 1) / threads;
            for (int y = y0; y < y1; ++y) {
                for (int x = 0; x < bw; ++x) {
                    int sx = mini(x * 4, RW - 1), sy = mini(y * 4, RH - 1);
                    const float* p = &hdr[((size_t)sy * RW + sx) * 3];
                    float lum = p[0] * 0.2126f + p[1] * 0.7152f + p[2] * 0.0722f;
                    float k = smoothstepf(0.55f, 1.6f, lum);
                    size_t bi = ((size_t)y * bw + x) * 3;
                    bA[bi] = p[0] * k; bA[bi + 1] = p[1] * k; bA[bi + 2] = p[2] * k;
                }
            }
        });
        // two separable blurs (horizontal into B, vertical back into A)
        {
            const int r = 3;
            float wts[4]; float wsum = 0;
            for (int i = 0; i <= r; ++i) { wts[i] = std::exp(-(float)(i * i) / 18.0f); wsum += (i == 0) ? wts[i] : 2 * wts[i]; }
            for (int i = 0; i <= r; ++i) wts[i] /= wsum;
            float* dst = bloomB_.data();
            Parallel::get().run(threads, [&](int tid) {
                int y0 = bh * tid / threads, y1 = bh * (tid + 1) / threads;
                for (int y = y0; y < y1; ++y) {
                    const float* r0 = &bA[(size_t)y * bw * 3];
                    float* d0 = &dst[(size_t)y * bw * 3];
                    for (int x = 0; x < bw; ++x) {
                        float ar = r0[x * 3] * wts[0], ag = r0[x * 3 + 1] * wts[0], ab = r0[x * 3 + 2] * wts[0];
                        for (int i = 1; i <= r; ++i) {
                            int xl = maxi(0, x - i), xr = mini(bw - 1, x + i);
                            ar += (r0[xl * 3] + r0[xr * 3]) * wts[i];
                            ag += (r0[xl * 3 + 1] + r0[xr * 3 + 1]) * wts[i];
                            ab += (r0[xl * 3 + 2] + r0[xr * 3 + 2]) * wts[i];
                        }
                        d0[x * 3] = ar; d0[x * 3 + 1] = ag; d0[x * 3 + 2] = ab;
                    }
                }
            });
            Parallel::get().run(threads, [&](int tid) {
                int y0 = bh * tid / threads, y1 = bh * (tid + 1) / threads;
                for (int y = y0; y < y1; ++y) {
                    for (int x = 0; x < bw; ++x) {
                        float ar = dst[((size_t)y * bw + x) * 3] * wts[0];
                        float ag = dst[((size_t)y * bw + x) * 3 + 1] * wts[0];
                        float ab = dst[((size_t)y * bw + x) * 3 + 2] * wts[0];
                        for (int i = 1; i <= r; ++i) {
                            int yl = maxi(0, y - i), yr = mini(bh - 1, y + i);
                            ar += (dst[((size_t)yl * bw + x) * 3] + dst[((size_t)yr * bw + x) * 3]) * wts[i];
                            ag += (dst[((size_t)yl * bw + x) * 3 + 1] + dst[((size_t)yr * bw + x) * 3 + 1]) * wts[i];
                            ab += (dst[((size_t)yl * bw + x) * 3 + 2] + dst[((size_t)yr * bw + x) * 3 + 2]) * wts[i];
                        }
                        bA[((size_t)y * bw + x) * 3] = ar;
                        bA[((size_t)y * bw + x) * 3 + 1] = ag;
                        bA[((size_t)y * bw + x) * 3 + 2] = ab;
                    }
                }
            });
        }
    }

    composite();
    upscale();
}

// ---- grade + tonemap at render resolution -------------------------------
void Renderer::composite() {
    const int RW = RW_, RH = RH_;
    const int bw = RW / 4 + 1, bh = RH / 4 + 1;
    const int threads = maxi(1, mini(Parallel::get().threads(), 8));
    const float* hdr = hdr_.data();
    const float* scat = scat_.data();
    const float* bloom = bloomA_.data();
    Color* out = ldr_.data();
    const PostSettings post = post_;
    const V3 flColor = fl_.color;
    const float flIntensity = fl_.on ? fl_.intensity : 0.0f;
    const float time = (float)nowSeconds();
    const float invRW = 1.0f / (float)RW, invRH = 1.0f / (float)RH;
    const int bwu = bw;
    const float gain = 0.55f * flIntensity;

    Parallel::get().run(threads, [&](int tid) {
        int y0 = RH * tid / threads, y1 = RH * (tid + 1) / threads;
        for (int y = y0; y < y1; ++y) {
            const float v = ((float)y + 0.5f) * invRH;
            for (int x = 0; x < RW; ++x) {
                const float u = ((float)x + 0.5f) * invRW;
                float su = u, sv = v;
                if (post.warp > 0.001f) {
                    float band = std::sin(v * 30.0f + post.warpPhase * 3.0f) * 0.5f + 0.5f;
                    su += std::sin(v * 42.0f + post.warpPhase) * post.warp * 0.006f * band;
                    sv += std::sin(u * 26.0f + post.warpPhase * 1.7f) * post.warp * 0.003f;
                }
                if (post.glitch > 0.001f) {
                    float rw = std::floor(v * 90.0f);
                    float hh = hashf((uint32_t)rw * 917u + (uint32_t)(post.warpPhase * 40.0f));
                    if (hh < post.glitch * 0.35f) su += (hh - 0.15f) * 0.10f;
                }
                // bilinear tap in the HDR buffer (no warp: direct fetch)
                float fx = saturate(su) * RW - 0.5f;
                float fy = saturate(sv) * RH - 0.5f;
                int hx0 = clampi((int)fx, 0, RW - 2);
                int hy0 = clampi((int)fy, 0, RH - 2);
                float tx = fx - (float)hx0, ty = fy - (float)hy0;
                const float* h00 = &hdr[((size_t)hy0 * RW + hx0) * 3];
                const float* h10 = h00 + 3;
                const float* h01 = h00 + (size_t)RW * 3;
                const float* h11 = h01 + 3;
                float w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty);
                float w01 = (1 - tx) * ty, w11 = tx * ty;
                V3 col(h00[0] * w00 + h10[0] * w10 + h01[0] * w01 + h11[0] * w11,
                       h00[1] * w00 + h10[1] * w10 + h01[1] * w01 + h11[1] * w11,
                       h00[2] * w00 + h10[2] * w10 + h01[2] * w01 + h11[2] * w11);

                if (post.aberration > 1.2f) {
                    float ca = post.aberration * 0.0018f * (0.25f + length(V2(u - 0.5f, v - 0.5f)));
                    col.x = sampleHdr(clampf(fx + ca * RW, 0.0f, RW - 1.0f), fy, 0);
                    col.z = sampleHdr(clampf(fx - ca * RW, 0.0f, RW - 1.0f), fy, 2);
                }

                // volumetric scatter (already blurred, bilinear fetch)
                if (gain > 0.0f && scat != nullptr) {
                    float bxf = (u * RW) * 0.25f - 0.5f, byf = (v * RH) * 0.25f - 0.5f;
                    int sx0 = clampi((int)std::floor(bxf), 0, bw - 2);
                    int sy0 = clampi((int)std::floor(byf), 0, bh - 2);
                    float stx = bxf - sx0, sty = byf - sy0;
                    const float* s00 = &scat[(size_t)sy0 * bwu + sx0];
                    const float* s01 = s00 + bwu;
                    float s = (s00[0] * (1 - stx) + s00[1] * stx) * (1 - sty) +
                              (s01[0] * (1 - stx) + s01[1] * stx) * sty;
                    col.x += flColor.x * (s * gain);
                    col.y += flColor.y * (s * gain);
                    col.z += flColor.z * (s * gain);
                }

                // bloom
                if (post.bloom > 0.001f) {
                    int bx = clampi((int)((u * RW) * 0.25f), 0, bw - 1);
                    int by = clampi((int)((v * RH) * 0.25f), 0, bh - 1);
                    const float* bl = &bloom[((size_t)by * bwu + bx) * 3];
                    col.x += bl[0] * post.bloom;
                    col.y += bl[1] * post.bloom;
                    col.z += bl[2] * post.bloom;
                }

                {
                    float ex = post.exposure;
                    float lr = acesFast(col.x * ex);
                    float lg = acesFast(col.y * ex);
                    float lb = acesFast(col.z * ex);
                    col = V3(lr, lg, lb);
                }

                float lum = col.x * 0.2126f + col.y * 0.7152f + col.z * 0.0722f;
                V3 tint = lerp(post.tintShadow, post.tintHigh, saturate(lum));
                col = col * tint;
                col = V3(lerpf(lum, col.x, post.saturation), lerpf(lum, col.y, post.saturation),
                         lerpf(lum, col.z, post.saturation));
                col = V3(saturate((col.x - 0.5f) * post.contrast + 0.5f + post.lift),
                         saturate((col.y - 0.5f) * post.contrast + 0.5f + post.lift),
                         saturate((col.z - 0.5f) * post.contrast + 0.5f + post.lift));

                float dx = u - 0.5f, dy = v - 0.5f;
                float r2 = dx * dx + dy * dy;
                float rad = r2 * rsqrtFast(maxf(r2, 1e-9f));
                float vig = clampf(1.0f - post.vignette * (rad * 1.35f), 0.0f, 1.0f);
                if (post.pulse > 0.001f) vig *= 1.0f - post.pulse * 0.55f * saturate(rad * 1.6f);
                col = col * vig;

                if (post.grain > 0.001f) {
                    float g = hashf((uint32_t)(x * 1973 + y * 9277) + (uint32_t)(time * 61.0f) * 7919u);
                    float amt = (g - 0.5f) * post.grain;
                    col.x += amt; col.y += amt; col.z += amt;
                }

                col = V3(saturate(col.x), saturate(col.y), saturate(col.z));
                if (post.fade > 0.001f) col = lerp(col, post.fadeColor, saturate(post.fade));

                int r = (int)(linToSrgbFast(col.x) * 255.0f + 0.5f);
                int g2 = (int)(linToSrgbFast(col.y) * 255.0f + 0.5f);
                int b2 = (int)(linToSrgbFast(col.z) * 255.0f + 0.5f);
                out[(size_t)y * RW + x] = rgba(r, g2, b2, 255);
            }
        }
    });
}

// ---- bilinear upscale of the graded image into the window framebuffer ----
void Renderer::upscale() {
    const int RW = RW_, RH = RH_;
    if (RW == W_ && RH == H_) {
        memcpy(fb_.data(), ldr_.data(), (size_t)RW * RH * 4);
        return;
    }
    const int threads = maxi(1, mini(Parallel::get().threads(), 8));
    const float sx = (float)RW / (float)W_;
    const float sy = (float)RH / (float)H_;
    Color* dst = fb_.data();
    const Color* src = ldr_.data();
    if (upscaleTmp_.size() != (size_t)W_ * RH) upscaleTmp_.assign((size_t)W_ * RH, 0xFF000000u);

    // pass 1: horizontal, render width -> window width (one row per source row)
    Color* tmp = upscaleTmp_.data();
    Parallel::get().run(threads, [&](int tid) {
        int y0 = RH * tid / threads, y1 = RH * (tid + 1) / threads;
        for (int y = y0; y < y1; ++y) {
            const Color* s0 = &src[(size_t)y * RW];
            Color* d = &tmp[(size_t)y * W_];
            for (int x = 0; x < W_; ++x) {
                float fx = ((float)x + 0.5f) * sx - 0.5f;
                int x0 = clampi((int)std::floor(fx), 0, RW - 2);
                float tx = saturate(fx - (float)x0);
                Color c0 = s0[x0], c1 = s0[x0 + 1];
                int r = (int)(colR(c0) + (colR(c1) - colR(c0)) * tx + 0.5f);
                int g = (int)(colG(c0) + (colG(c1) - colG(c0)) * tx + 0.5f);
                int b = (int)(colB(c0) + (colB(c1) - colB(c0)) * tx + 0.5f);
                d[x] = rgba(r, g, b, 255);
            }
        }
    });
    // pass 2: vertical, render height -> window height
    Parallel::get().run(threads, [&](int tid) {
        int y0 = H_ * tid / threads, y1 = H_ * (tid + 1) / threads;
        for (int y = y0; y < y1; ++y) {
            float fy = ((float)y + 0.5f) * sy - 0.5f;
            int y0s = clampi((int)std::floor(fy), 0, RH - 2);
            float ty = saturate(fy - (float)y0s);
            const Color* r0 = &tmp[(size_t)y0s * W_];
            const Color* r1 = r0 + W_;
            Color* d = &dst[(size_t)y * W_];
            for (int x = 0; x < W_; ++x) {
                Color c0 = r0[x], c1 = r1[x];
                int r = (int)(colR(c0) + (colR(c1) - colR(c0)) * ty + 0.5f);
                int g = (int)(colG(c0) + (colG(c1) - colG(c0)) * ty + 0.5f);
                int b = (int)(colB(c0) + (colB(c1) - colB(c0)) * ty + 0.5f);
                d[x] = rgba(r, g, b, 255);
            }
        }
    });
}

// ------------------------------------------------------------------ font
bool Font::load(const std::string& pngPath, const std::string& metaPath) {
    if (!pngLoad(pngPath.c_str(), atlas)) return false;
    std::vector<uint8_t> meta;
    if (!readFile(metaPath.c_str(), meta)) return false;
    std::string txt((const char*)meta.data(), meta.size());
    size_t pos = 0;
    glyphs.clear();
    while (pos < txt.size()) {
        size_t eol = txt.find('\n', pos);
        if (eol == std::string::npos) eol = txt.size();
        std::string line = txt.substr(pos, eol - pos);
        pos = eol + 1;
        if (line.empty()) continue;
        int cp = 0, x, y, w, h, ox, oy, adv;
        if (sscanf(line.c_str(), "glyph %d %d %d %d %d %d %d %d", &cp, &x, &y, &w, &h, &ox, &oy, &adv) == 8) {
            Glyph g; g.x = x; g.y = y; g.w = w; g.h = h; g.ox = ox; g.oy = oy; g.adv = adv;
            glyphs[cp] = g;
        } else if (sscanf(line.c_str(), "size %d %d %d %d", &size, &ascent, &descent, &lineHeight) == 4) {
        } else if (line.rfind("size ", 0) == 0) {
            sscanf(line.c_str(), "size %d %d %d %d", &size, &ascent, &descent, &lineHeight);
        }
    }
    return !glyphs.empty();
}

// ------------------------------------------------------------------ painter
void Painter::begin(Color* fb, int w, int h) {
    fb_ = fb; W_ = w; H_ = h;
    cx0_ = 0; cy0_ = 0; cx1_ = w; cy1_ = h;
}

void Painter::px(int x, int y, Color c) {
    if (x < cx0_ || y < cy0_ || x >= cx1_ || y >= cy1_) return;
    fb_[(size_t)y * W_ + x] = c;
}

void Painter::blendPx(int x, int y, Color c) {
    if (x < cx0_ || y < cy0_ || x >= cx1_ || y >= cy1_) return;
    Color& d = fb_[(size_t)y * W_ + x];
    int a = colA(c);
    if (a >= 255) { d = c; return; }
    if (a <= 0) return;
    d = colLerp(d, c | 0xFF000000u, a / 255.0f);
}

void Painter::clear(Color c) {
    std::fill(fb_, fb_ + (size_t)W_ * H_, c);
}

void Painter::rect(float x, float y, float w, float h, Color c) {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    int x1 = (int)std::ceil(x + w), y1 = (int)std::ceil(y + h);
    int a = colA(c);
    if (a >= 255) {
        for (int yy = maxi(y0, cy0_); yy < mini(y1, cy1_); ++yy)
            for (int xx = maxi(x0, cx0_); xx < mini(x1, cx1_); ++xx) fb_[(size_t)yy * W_ + xx] = c;
    } else {
        for (int yy = maxi(y0, cy0_); yy < mini(y1, cy1_); ++yy)
            for (int xx = maxi(x0, cx0_); xx < mini(x1, cx1_); ++xx) blendPx(xx, yy, c);
    }
}

void Painter::rectOutline(float x, float y, float w, float h, float th, Color c) {
    rect(x, y, w, th, c);
    rect(x, y + h - th, w, th, c);
    rect(x, y, th, h, c);
    rect(x + w - th, y, th, h, c);
}

void Painter::gradientV(float x, float y, float w, float h, Color top, Color bottom) {
    int y0 = (int)std::floor(y), y1 = (int)std::ceil(y + h);
    for (int yy = y0; yy < y1; ++yy) {
        float t = (yy - y) / maxf(1.0f, h);
        Color c = colLerp(top, bottom, saturate(t));
        int a = colA(c);
        if (a >= 250) {
            for (int xx = maxi((int)x, cx0_); xx < mini((int)(x + w), cx1_); ++xx) px(xx, yy, c);
        } else {
            for (int xx = maxi((int)x, cx0_); xx < mini((int)(x + w), cx1_); ++xx) blendPx(xx, yy, c);
        }
    }
}

void Painter::line(float x0, float y0, float x1, float y1, float th, Color c) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * th * 0.5f, ny = dx / len * th * 0.5f;
    V2 pts[4] = {V2(x0 + nx, y0 + ny), V2(x1 + nx, y1 + ny), V2(x1 - nx, y1 - ny), V2(x0 - nx, y0 - ny)};
    // simple quad raster
    float minx = minf(minf(pts[0].x, pts[1].x), minf(pts[2].x, pts[3].x));
    float maxx = maxf(maxf(pts[0].x, pts[1].x), maxf(pts[2].x, pts[3].x));
    float miny = minf(minf(pts[0].y, pts[1].y), minf(pts[2].y, pts[3].y));
    float maxy = maxf(maxf(pts[0].y, pts[1].y), maxf(pts[2].y, pts[3].y));
    for (int y = (int)miny; y <= (int)maxy; ++y) {
        for (int x = (int)minx; x <= (int)maxx; ++x) {
            V2 p(x + 0.5f, y + 0.5f);
            // inside test for two triangles of quad (0,1,2) (0,2,3)
            auto edge = [](V2 a, V2 b, V2 p2) { return (b.x - a.x) * (p2.y - a.y) - (b.y - a.y) * (p2.x - a.x); };
            float e0 = edge(pts[0], pts[1], p), e1 = edge(pts[1], pts[2], p), e2 = edge(pts[2], pts[0], p);
            bool in1 = (e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0);
            float f0 = edge(pts[0], pts[2], p), f1 = edge(pts[2], pts[3], p), f2 = edge(pts[3], pts[0], p);
            bool in2 = (f0 >= 0 && f1 >= 0 && f2 >= 0) || (f0 <= 0 && f1 <= 0 && f2 <= 0);
            if (in1 || in2) blendPx(x, y, c);
        }
    }
}

void Painter::circle(float cx, float cy, float r, Color c) {
    int x0 = (int)(cx - r - 1), x1 = (int)(cx + r + 1);
    int y0 = (int)(cy - r - 1), y1 = (int)(cy + r + 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = std::sqrt(dx * dx + dy * dy);
            float a = saturate(r + 0.5f - d);
            if (a <= 0) continue;
            Color cc = c;
            int aa = (int)(colA(c) * a);
            cc = (c & 0x00FFFFFF) | ((Color)aa << 24);
            blendPx(x, y, cc);
        }
    }
}

void Painter::circleOutline(float cx, float cy, float r, float th, Color c) {
    int x0 = (int)(cx - r - 2), x1 = (int)(cx + r + 2);
    int y0 = (int)(cy - r - 2), y1 = (int)(cy + r + 2);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = std::sqrt(dx * dx + dy * dy);
            float a = saturate(th * 0.5f + 0.5f - std::fabs(d - r));
            if (a <= 0) continue;
            Color cc = (c & 0x00FFFFFF) | ((Color)(int)(colA(c) * a) << 24);
            blendPx(x, y, cc);
        }
    }
}

void Painter::triangle(V2 a, V2 b, V2 c, Color col) {
    float minx = minf(a.x, minf(b.x, c.x)), maxx = maxf(a.x, maxf(b.x, c.x));
    float miny = minf(a.y, minf(b.y, c.y)), maxy = maxf(a.y, maxf(b.y, c.y));
    float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (std::fabs(area) < 1e-6f) return;
    float inv = 1.0f / area;
    for (int y = (int)miny; y <= (int)maxy; ++y) {
        for (int x = (int)minx; x <= (int)maxx; ++x) {
            float px2 = x + 0.5f, py = y + 0.5f;
            float w0 = ((b.x - px2) * (c.y - py) - (c.x - px2) * (b.y - py)) * inv;
            float w1 = ((c.x - px2) * (a.y - py) - (a.x - px2) * (c.y - py)) * inv;
            float w2 = 1.0f - w0 - w1;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) blendPx(x, y, col);
        }
    }
}

void Painter::blit(const Image& img, float x, float y, float w, float h, Color tint) {
    if (!img.valid() || w <= 0 || h <= 0) return;
    int x0 = maxi((int)x, cx0_), x1 = mini((int)(x + w), cx1_);
    int y0 = maxi((int)y, cy0_), y1 = mini((int)(y + h), cy1_);
    float tv = colA(tint) / 255.0f;
    for (int yy = y0; yy < y1; ++yy) {
        float v = (yy + 0.5f - y) / h;
        for (int xx = x0; xx < x1; ++xx) {
            float u = (xx + 0.5f - x) / w;
            Color c = img.sampleBilinear(u, v);
            float a = colA(c) / 255.0f * tv;
            if (a <= 0.004f) continue;
            Color cc = rgba(colR(c) * colR(tint) / 255, colG(c) * colG(tint) / 255,
                            colB(c) * colB(tint) / 255, (int)(a * 255));
            blendPx(xx, yy, cc);
        }
    }
}

void Painter::blitIcon(const Image& img, float x, float y, float w, float h, Color tint) {
    blit(img, x, y, w, h, tint);
}

int Font::measure(const std::string& s, float scale) const {
    float w = 0;
    float maxW = 0;
    int cp = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n') { maxW = maxf(maxW, w); w = 0; ++i; continue; }
        cp = c; ++i;
        const Glyph* g = find(cp);
        if (g) w += g->adv * scale;
        else w += 5 * scale;
    }
    return (int)maxf(maxW, w);
}

void Painter::text(const Font& f, const std::string& s, float x, float y, Color c, float scale) {
    float cx = x;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') { cx = x; y += f.lineHeight * scale; continue; }
        const Font::Glyph* g = f.find((int)ch);
        if (!g) { cx += 5 * scale; continue; }
        if (g->w > 0) {
            for (int gy = 0; gy < g->h; ++gy) {
                for (int gx = 0; gx < g->w; ++gx) {
                    Color sc = f.atlas.get(g->x + gx, g->y + gy);
                    float a = colR(sc) / 255.0f;
                    if (a < 0.02f) continue;
                    int px0 = (int)(cx + g->ox * scale + gx * scale);
                    int py0 = (int)(y + g->oy * scale + gy * scale);
                    int px1 = (int)(cx + g->ox * scale + (gx + 1) * scale);
                    int py1 = (int)(y + g->oy * scale + (gy + 1) * scale);
                    for (int yy = py0; yy < maxi(py0 + 1, py1); ++yy)
                        for (int xx = px0; xx < maxi(px0 + 1, px1); ++xx) {
                            Color cc = (c & 0x00FFFFFF) | ((Color)(int)(colA(c) * a) << 24);
                            blendPx(xx, yy, cc);
                        }
                }
            }
        }
        cx += g->adv * scale;
    }
}

void Painter::textCentered(const Font& f, const std::string& s, float cx, float y, Color c, float scale) {
    int w = f.measure(s, scale);
    text(f, s, cx - w * 0.5f, y, c, scale);
}

void Painter::textWrap(const Font& f, const std::string& s, float x, float y, float maxW, Color c,
                       float scale, int maxLines) {
    std::string line, word;
    int lines = 0;
    auto flushLine = [&](bool force) {
        if (f.measure(line, scale) + 0 > maxW) {
            // word wrap
            std::string cur, out;
            size_t i = 0;
            while (i < line.size()) {
                size_t j = line.find(' ', i);
                if (j == std::string::npos) j = line.size();
                std::string w = line.substr(i, j - i);
                std::string cand = cur.empty() ? w : cur + " " + w;
                if (f.measure(cand, scale) > maxW && !cur.empty()) {
                    text(f, cur, x, y, c, scale);
                    y += f.lineHeight * scale;
                    ++lines;
                    cur = w;
                } else cur = cand;
                i = j + (j < line.size() ? 1 : 0);
                if (lines >= maxLines) return;
            }
            if (!cur.empty() && lines < maxLines) {
                text(f, cur, x, y, c, scale);
                y += f.lineHeight * scale;
                ++lines;
            }
        } else {
            text(f, line, x, y, c, scale);
            y += f.lineHeight * scale;
            ++lines;
        }
        line.clear();
        (void)force;
    };
    for (size_t i = 0; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '\n') { flushLine(true); if (lines >= maxLines) return; continue; }
        line += ch;
    }
    if (!line.empty() && lines < maxLines) flushLine(true);
}

} // namespace nf
