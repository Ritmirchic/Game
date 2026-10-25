// ============================================================================
//  nf_render.cpp : software renderer implementation
// ============================================================================
#include "nf_render.h"
#include <thread>
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

// ------------------------------------------------------------------ renderer lifecycle
void Renderer::init(int w, int h) {
    initSrgbLut();
    W_ = w; H_ = h;
    resize(w, h);
}

void Renderer::resize(int w, int h) {
    W_ = w; H_ = h;
    RW_ = maxi(64, (int)(w * scale_));
    RH_ = maxi(36, (int)(h * scale_));
    fb_.assign((size_t)W_ * H_, 0xFF000000);
    hdr_.assign((size_t)RW_ * RH_ * 3, 0.0f);
    depth_.assign((size_t)RW_ * RH_, 0.0f);
    bloomA_.assign((size_t)(RW_ / 4 + 1) * (RH_ / 4 + 1) * 3, 0.0f);
    bloomB_ = bloomA_;
    shadow_.assign((size_t)SM_SIZE * SM_SIZE, 1.0f);
}

void Renderer::setResolutionScale(float s) {
    scale_ = clampf(s, 0.35f, 1.0f);
    resize(W_, H_);
}

void Renderer::shutdown() {
    fb_.clear(); hdr_.clear(); depth_.clear(); shadow_.clear();
    bloomA_.clear(); bloomB_.clear();
}

void Renderer::beginFrame() {
    items_.clear();
    lights_.clear();
    std::fill(depth_.begin(), depth_.end(), 1.0f);
    std::fill(hdr_.begin(), hdr_.end(), 0.0f);
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
    {
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
    }
    Item it;
    it.mesh = &m;
    it.xf = xf;
    it.nxf = xf;   // rotation-only scale assumed; normals renormalised in shader
    it.mat = mat;
    it.castShadow = mat.castShadow && shadow_.size() > 0;
    items_.push_back(it);
}

int Renderer::triCount() const {
    int n = 0;
    for (auto& i : items_) n += (int)i.mesh->idx.size() / 3;
    return n;
}

// ------------------------------------------------------------------ shadow map
void Renderer::buildShadowMap() {
    std::fill(shadow_.begin(), shadow_.end(), 1.0f);

    if (!fl_.on || fl_.intensity <= 0.001f) return;
    float fov = std::acos(clampf(fl_.outer, -0.99f, 0.99f)) * 2.0f * 1.25f;
    fov = clampf(fov, 0.5f, 1.9f);
    M4 v = M4::lookAt(fl_.pos, fl_.pos + fl_.dir, std::fabs(fl_.dir.z) > 0.95f ? V3(0, 1, 0) : V3(0, 0, 1));
    M4 p = M4::perspective(fov, 1.0f, 0.05f, fl_.range * 1.35f + 2.0f);
    smMat_ = p * v;

    for (auto& it : items_) {
        if (!it.castShadow) continue;
        // only geometry near the torch can occlude it
        V3 c = it.xf.point(it.mesh->bounds.center());
        if (length(c - fl_.pos) - maxComp(it.mesh->bounds.size()) > 17.0f) continue;
        const Mesh& m = *it.mesh;
        M4 mvp = smMat_ * it.xf;
        for (size_t t = 0; t < m.idx.size(); t += 3) {
            V4 c[3];
            for (int k = 0; k < 3; ++k) {
                const Vertex& vv = m.verts[m.idx[t + k]];
                c[k] = mvp * V4(vv.p.x, vv.p.y, vv.p.z, 1.0f);
            }
            // clip against near plane (z + w > 0)
            V4 in[4]; int n = 0;
            for (int k = 0; k < 3; ++k) in[k] = c[k];
            n = 3;
            V4 outv[8]; int on = 0;
            for (int k = 0; k < n; ++k) {
                const V4& a = in[k];
                const V4& b = in[(k + 1) % n];
                float da = a.z + a.w, db = b.z + b.w;
                if (da >= 0.0001f) outv[on++] = a;
                if ((da >= 0.0001f) != (db >= 0.0001f)) {
                    float tt = da / (da - db);
                    outv[on++] = V4(lerpf(a.x, b.x, tt), lerpf(a.y, b.y, tt),
                                    lerpf(a.z, b.z, tt), lerpf(a.w, b.w, tt));
                }
            }
            if (on < 3) continue;
            for (int k = 1; k + 1 < on; ++k) {
                V4 tri[3] = {outv[0], outv[k], outv[k + 1]};
                float sx[3], sy[3], sz[3];
                bool ok = true;
                for (int q = 0; q < 3; ++q) {
                    if (tri[q].w <= 1e-5f) { ok = false; break; }
                    float iw = 1.0f / tri[q].w;
                    sx[q] = (tri[q].x * iw * 0.5f + 0.5f) * SM_SIZE;
                    sy[q] = (0.5f - tri[q].y * iw * 0.5f) * SM_SIZE;
                    sz[q] = ndcZtoDepth(tri[q].z * iw);
                }
                if (!ok) continue;
                float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
                if (std::fabs(area) < 1e-7f) continue;
                int minX = clampi((int)std::floor(minf(sx[0], minf(sx[1], sx[2]))), 0, SM_SIZE - 1);
                int maxX = clampi((int)std::ceil(maxf(sx[0], maxf(sx[1], sx[2]))), 0, SM_SIZE - 1);
                int minY = clampi((int)std::floor(minf(sy[0], minf(sy[1], sy[2]))), 0, SM_SIZE - 1);
                int maxY = clampi((int)std::ceil(maxf(sy[0], maxf(sy[1], sy[2]))), 0, SM_SIZE - 1);
                float inv = 1.0f / area;
                for (int y = minY; y <= maxY; ++y) {
                    float py = y + 0.5f;
                    for (int x = minX; x <= maxX; ++x) {
                        float px = x + 0.5f;
                        float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * inv;
                        float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * inv;
                        float w2 = 1.0f - w0 - w1;
                        if (w0 < -0.002f || w1 < -0.002f || w2 < -0.002f) continue;
                        float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                        float& dst = shadow_[(size_t)y * SM_SIZE + x];
                        if (z < dst) dst = z;
                    }
                }
            }
        }
    }
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
    int ix = (int)(u * SM_SIZE), iy = (int)(v * SM_SIZE);
    float zz = z - bias;
    int sx0 = clampi(ix - 1, 0, SM_SIZE - 2), sy0 = clampi(iy - 1, 0, SM_SIZE - 2);
    const float* row0 = &shadow_[(size_t)sy0 * SM_SIZE + sx0];
    const float* row1 = row0 + SM_SIZE;
    float lit = (zz <= row0[0]) + (zz <= row0[1]) + (zz <= row1[0]) + (zz <= row1[1]);
    return lit * 0.25f;
}

// ------------------------------------------------------------------ sampling
static inline void sampleTex(const std::vector<Image>& mips, float u, float v, float level,
                             V3& outLinear, float& alpha) {
    int nm = (int)mips.size();
    int l0 = clampi((int)level, 0, nm - 1);
    const Image& im = mips[l0];
    if (!im.valid()) { outLinear = V3(1, 1, 1); alpha = 1; return; }
    float fu = u * im.w - 0.5f, fv = v * im.h - 0.5f;
    int x0 = (int)std::floor(fu), y0 = (int)std::floor(fv);
    float tx = fu - x0, ty = fv - y0;
    auto getpx = [&](int x, int y) -> Color {
        x = clampi(x, 0, im.w - 1); y = clampi(y, 0, im.h - 1);
        return im.px[(size_t)y * im.w + x];
    };
    Color c00 = getpx(x0, y0), c10 = getpx(x0 + 1, y0), c01 = getpx(x0, y0 + 1), c11 = getpx(x0 + 1, y0 + 1);
    float r = lerpf(lerpf((float)colR(c00), (float)colR(c10), tx), lerpf((float)colR(c01), (float)colR(c11), tx), ty);
    float g = lerpf(lerpf((float)colG(c00), (float)colG(c10), tx), lerpf((float)colG(c01), (float)colG(c11), tx), ty);
    float b = lerpf(lerpf((float)colB(c00), (float)colB(c10), tx), lerpf((float)colB(c01), (float)colB(c11), tx), ty);
    float a = lerpf(lerpf((float)colA(c00), (float)colA(c10), tx), lerpf((float)colA(c01), (float)colA(c11), tx), ty);
    float fr = level - (float)l0;
    if (fr > 0.01f && l0 + 1 < nm) {
        const Image& im2 = mips[l0 + 1];
        float fu2 = u * im2.w - 0.5f, fv2 = v * im2.h - 0.5f;
        int xx0 = (int)std::floor(fu2), yy0 = (int)std::floor(fv2);
        float tx2 = fu2 - xx0, ty2 = fv2 - yy0;
        auto getpx2 = [&](int x, int y) -> Color {
            x = clampi(x, 0, im2.w - 1); y = clampi(y, 0, im2.h - 1);
            return im2.px[(size_t)y * im2.w + x];
        };
        Color d00 = getpx2(xx0, yy0), d10 = getpx2(xx0 + 1, yy0), d01 = getpx2(xx0, yy0 + 1), d11 = getpx2(xx0 + 1, yy0 + 1);
        float r2 = lerpf(lerpf((float)colR(d00), (float)colR(d10), tx2), lerpf((float)colR(d01), (float)colR(d11), tx2), ty2);
        float g2 = lerpf(lerpf((float)colG(d00), (float)colG(d10), tx2), lerpf((float)colG(d01), (float)colG(d11), tx2), ty2);
        float b2 = lerpf(lerpf((float)colB(d00), (float)colB(d10), tx2), lerpf((float)colB(d01), (float)colB(d11), tx2), ty2);
        float a2 = lerpf(lerpf((float)colA(d00), (float)colA(d10), tx2), lerpf((float)colA(d01), (float)colA(d11), tx2), ty2);
        r = lerpf(r, r2, fr); g = lerpf(g, g2, fr); b = lerpf(b, b2, fr); a = lerpf(a, a2, fr);
    }
    outLinear = V3(srgbLut[(int)clampf(r, 0, 255)], srgbLut[(int)clampf(g, 0, 255)], srgbLut[(int)clampf(b, 0, 255)]);
    alpha = a / 255.0f;
}

// ------------------------------------------------------------------ main raster
namespace {
struct SVert {
    float x, y, z, invW;
    V3 wp;
    V3 n;
    float u, v;
    V3 baked;
    Color col;
};
struct RtCtx {
    float areaInv = 0;
    bool cull = true;
};

inline bool insideTri(float w0, float w1, float w2, float e) {
    return w0 >= -e && w1 >= -e && w2 >= -e;
}
} // namespace

namespace {
struct CVert {
    V4 c;
    V3 wp;
    V3 n;
    float u = 0, v = 0;
    V3 baked;
    V3 vcol{1, 1, 1};
};
}

// ------------------------------------------------------------------ banded raster
// Renders scanlines [y0,y1) into caller-provided depth / hdr buffers.
void Renderer::renderBand(int y0, int y1, float* depthBuf, float* hdrBuf) {
    (void)y1;
    std::vector<CVert> cverts;
    std::vector<CVert> poly, clipped;

    for (auto& it : items_) {
        const Mesh& m = *it.mesh;
        const Material& mat = it.mat;
        M4 mvp = viewProj_ * it.xf;
        const std::vector<Image>* tex = nullptr;
        if (mat.tex >= 0 && mat.tex < (int)textures.mips.size()) tex = &textures.mips[mat.tex];
        if (tex && (*tex).empty()) tex = nullptr;

        cverts.resize(m.verts.size());
        for (size_t i = 0; i < m.verts.size(); ++i) {
            const Vertex& v = m.verts[i];
            V3 wp = it.xf.point(v.p);
            V3 wn = normalize(it.nxf.dir(v.n));
            CVert cv;
            cv.c = mvp * V4(wp.x, wp.y, wp.z, 1.0f);
            cv.wp = wp;
            cv.n = wn;
            cv.u = v.u; cv.v = v.v;
            cv.baked = v.baked;
            cv.vcol = colToV3(v.col);
            cverts[i] = cv;
        }

        V3 tint = colToV3(mat.tint);
        float tintA = colA(mat.tint) / 255.0f * mat.alpha;

        for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
            poly.clear();
            poly.push_back(cverts[m.idx[t]]);
            poly.push_back(cverts[m.idx[t + 1]]);
            poly.push_back(cverts[m.idx[t + 2]]);

            // ---- clip against near plane (z + w >= 0)
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
                    m2.baked = lerp(a.baked, b.baked, tt);
                    m2.vcol = lerp(a.vcol, b.vcol, tt);
                    clipped.push_back(m2);
                }
            }
            if (clipped.size() < 3) continue;

            for (size_t f = 1; f + 1 < clipped.size(); ++f) {
                SVert s[3];
                bool ok = true;
                for (int k = 0; k < 3; ++k) {
                    const CVert& cv = clipped[(k == 0) ? 0 : (f + k - 1)];
                    if (cv.c.w <= 1e-5f) { ok = false; break; }
                    float iw = 1.0f / cv.c.w;
                    s[k].x = (cv.c.x * iw * 0.5f + 0.5f) * RW_;
                    s[k].y = (0.5f - cv.c.y * iw * 0.5f) * RH_;
                    s[k].z = ndcZtoDepth(cv.c.z * iw);
                    s[k].invW = iw;
                    s[k].wp = cv.wp;
                    s[k].n = cv.n;
                    s[k].u = cv.u; s[k].v = cv.v;
                    s[k].baked = cv.baked;
                    s[k].col = v3ToCol(cv.vcol);
                }
                if (!ok) continue;
                SVert a = s[0], b = s[1], c = s[2];
                float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
                if (std::fabs(area) < 1e-6f) continue;
                bool front = area < 0.0f;
                if (mat.cull && !front) continue;
                float inv = 1.0f / area;

                float level = 0.0f;
                if (tex) {
                    float duv = std::fabs((a.u - c.u) * (b.v - c.v) - (b.u - c.u) * (a.v - c.v));
                    float px = std::sqrt(std::fabs(duv * inv)) * (float)(*tex)[0].w;
                    level = std::log2(maxf(1e-4f, px));
                }

                int minX = clampi((int)std::floor(minf(a.x, minf(b.x, c.x))), 0, RW_ - 1);
                int maxX = clampi((int)std::ceil(maxf(a.x, maxf(b.x, c.x))), 0, RW_ - 1);
                int minY = clampi((int)std::floor(minf(a.y, minf(b.y, c.y))), 0, RH_ - 1);
                int maxY = clampi((int)std::ceil(maxf(a.y, maxf(b.y, c.y))), 0, RH_ - 1);
                if (minX > maxX || minY > maxY) continue;
                bool depthWrite = alphaOpaque(mat);

                for (int y = minY; y <= maxY; ++y) {
                    float py = y + 0.5f;
                    for (int x = minX; x <= maxX; ++x) {
                        float px2 = x + 0.5f;
                        float w0 = ((b.x - px2) * (c.y - py) - (c.x - px2) * (b.y - py)) * inv;
                        float w1 = ((c.x - px2) * (a.y - py) - (a.x - px2) * (c.y - py)) * inv;
                        float w2 = 1.0f - w0 - w1;
                        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                        float z = w0 * a.z + w1 * b.z + w2 * c.z;
                        size_t pi = (size_t)(y - y0) * RW_ + x;
                        if (z >= depthBuf[pi]) continue;
                        float iw = w0 * a.invW + w1 * b.invW + w2 * c.invW;
                        if (iw <= 1e-8f) continue;
                        float u = (w0 * a.u * a.invW + w1 * b.u * b.invW + w2 * c.u * c.invW) / iw;
                        float v = (w0 * a.v * a.invW + w1 * b.v * b.invW + w2 * c.v * c.invW) / iw;
                        V3 wp = (a.wp * (w0 * a.invW) + b.wp * (w1 * b.invW) + c.wp * (w2 * c.invW)) / iw;
                        V3 nn = normalize(a.n * (w0 * a.invW) + b.n * (w1 * b.invW) + c.n * (w2 * c.invW));
                        V3 baked = a.baked * w0 + b.baked * w1 + c.baked * w2;
                        V3 vcol = (colToV3(a.col) * w0 + colToV3(b.col) * w1 + colToV3(c.col) * w2);

                        V3 albedo(1, 1, 1);
                        float alpha = 1.0f;
                        if (tex) sampleTex(*tex, u, v, level, albedo, alpha);
                        albedo = albedo * tint * vcol;
                        alpha *= tintA;
                        if (mat.alphaTest && alpha < mat.alphaCut) continue;

                        V3 N = nn;
                        V3 V = normalize(eye_ - wp);
                        float ao = saturate(baked.x);

                        V3 col = albedo * ambient_ * ambientStrength_ *
                                 (0.55f + 0.45f * saturate(N.z * 0.5f + 0.5f)) * ao;
                        col += albedo * mat.emis;

                        if (!mat.unlit) {
                            float ndlF = 1.0f;
                            if (fl_.on) {
                                V3 Lv0 = fl_.pos - wp;
                                float dl0 = length(Lv0);
                                ndlF = maxf(dot(N, Lv0 / maxf(dl0, 1e-4f)), 0.0f);
                            }
                            float shadow = mat.receiveShadow ? sampleShadowN(wp, N, ndlF) : 1.0f;
                            if (fl_.on && fl_.intensity > 0.0f) {
                                V3 Lv = fl_.pos - wp;
                                float dist = length(Lv);
                                V3 L = Lv / maxf(dist, 1e-4f);
                                float cd = dot(-L, fl_.dir);
                                float spot = smoothstepf(fl_.outer, fl_.inner, cd);
                                float atten = minf(1.0f / (0.35f + dist * dist * 0.075f), 0.95f) *
                                              saturate(1.0f - dist / fl_.range) *
                                              smoothstepf(0.05f, 0.45f, dist);
                                float ndl = maxf(dot(N, L), 0.0f);
                                float k = spot * atten * shadow * fl_.intensity;
                                col += albedo * fl_.color * (ndl * k);
                                V3 H = L + V;
                                float nh = maxf(dot(N, H) / maxf(length(H), 1e-4f), 0.0f);
                                float n2 = nh * nh, n4 = n2 * n2;
                                float sp = n4 * n4 * mat.spec;
                                col += fl_.color * (sp * k * 2.2f);
                            }
                            for (auto& lt : lights_) {
                                if (!lt.enabled || lt.intensity <= 0.001f) continue;
                                V3 Lv = lt.pos - wp;
                                float d2 = dot(Lv, Lv);
                                if (d2 > lt.range * lt.range) continue;
                                float dist = std::sqrt(d2);
                                V3 L = Lv / maxf(dist, 1e-4f);
                                float atten = minf(1.0f / (0.22f + 0.55f * d2), 1.30f);
                                float edge = saturate(1.0f - dist / lt.range);
                                atten *= edge * edge;
                                float ndl = maxf(dot(N, L), 0.0f);
                                float k = atten * lt.intensity * lt.flicker;
                                col += albedo * lt.color * (ndl * k);
                                V3 H = L + V;
                                float nh = maxf(dot(N, H) / maxf(length(H), 1e-4f), 0.0f);
                                float n2 = nh * nh, n4 = n2 * n2;
                                float sp = n4 * n4 * mat.spec * 1.6f;
                                col += lt.color * (sp * k);
                            }
                        }

                        float dist = length(wp - eye_);
                        float fogAmt = 1.0f - std::exp(-fog_.density * dist * 1.55f);
                        col = lerp(col, fog_.color, saturate(fogAmt));

                        size_t hi = pi * 3;
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
        }
    }
}

// ------------------------------------------------------------------ frame
void Renderer::render() {
    static const bool prof = getenv("NF_PROFILE") != nullptr;
    auto nowMS = [] { return std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count(); };
    double t0 = prof ? nowMS() : 0.0;
    buildShadowMap();
    double tShadow = prof ? nowMS() : 0.0;

    int nThreads = 1;
    if (!getenv("NF_SINGLE")) {
        unsigned hc = std::thread::hardware_concurrency();
        nThreads = (int)clampi((int)(hc ? hc : 1), 1, 8);
    }
    if (nThreads <= 1) {
        renderBand(0, RH_, depth_.data(), hdr_.data());
    } else {
        size_t needD = (size_t)RW_ * RH_;
        size_t needH = needD * 3;
        if (bandDepth_.size() < needD * nThreads) bandDepth_.resize(needD * nThreads);
        if (bandHdr_.size() < needH * nThreads) bandHdr_.resize(needH * nThreads);
        std::vector<std::thread> pool;
        std::vector<int> bands(nThreads + 1);
        for (int b = 0; b <= nThreads; ++b) bands[b] = (int)((long long)RH_ * b / nThreads);
        for (int b = 0; b < nThreads; ++b) {
            int ya = bands[b], yb = bands[b + 1];
            if (yb <= ya) continue;
            float* d = bandDepth_.data() + (size_t)RW_ * ya;
            float* h = bandHdr_.data() + (size_t)RW_ * ya * 3;
            pool.emplace_back([this, ya, yb, d, h] { renderBand(ya, yb, d, h); });
        }
        for (auto& t : pool) t.join();
        // merge the bands
        for (int b = 0; b < nThreads; ++b) {
            int ya = bands[b], yb = bands[b + 1];
            if (yb <= ya) continue;
            size_t n = (size_t)RW_ * (yb - ya);
            memcpy(depth_.data() + (size_t)RW_ * ya, bandDepth_.data() + (size_t)RW_ * ya, n * sizeof(float));
            memcpy(hdr_.data() + (size_t)RW_ * ya * 3, bandHdr_.data() + (size_t)RW_ * ya * 3, n * 3 * sizeof(float));
        }
        // clear the scratch so band tops start at "far"
        std::fill(bandDepth_.begin(), bandDepth_.begin() + needD * nThreads, 1.0f);
        std::fill(bandHdr_.begin(), bandHdr_.begin() + needH * nThreads, 0.0f);
    }

    double tGeom = prof ? nowMS() : 0.0;
    postProcess();
    if (prof) {
        static int frames = 0; static double accS = 0, accG = 0, accP = 0;
        double tEnd = nowMS();
        accS += tShadow - t0; accG += tGeom - tShadow; accP += tEnd - tGeom;
        if (++frames % 10 == 0 && getenv("NF_PROFILE")) {
            logMsg("[prof] shadow %.1f  geom %.1f  post %.1f ms  (items %d)",
                   accS / 10, accG / 10, accP / 10, (int)items_.size());
            accS = accG = accP = 0;
        }
    }
}

// ------------------------------------------------------------------ post
// We keep the composite in a separate buffer at window resolution, written by postProcess.
namespace {
struct PostCtx {};
}

void Renderer::postProcess() {
    const int RW = RW_, RH = RH_;
    const int bw = RW / 4 + 1, bh = RH / 4 + 1;

    // ---- volumetric beam (quarter res)
    std::vector<float> scat((size_t)bw * bh, 0.0f);
    if (fl_.on && fl_.intensity > 0.001f && fog_.scatter > 0.001f) {
        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                int sx = clampi(x * 4, 0, RW - 1), sy = clampi(y * 4, 0, RH - 1);
                float z = depth_[ (size_t)sy * RW + sx ];
                float ndcx = ((float)sx + 0.5f) / RW * 2.0f - 1.0f;
                float ndcy = 1.0f - ((float)sy + 0.5f) / RH * 2.0f;
                float tanF = std::tan(fovY_ * 0.5f);
                V3 rd = normalize(fwd_ + right_ * (ndcx * tanF * aspect_) + up_ * (ndcy * tanF));
                // view depth -> distance
                float zndc = z * 2.0f - 1.0f;
                float viewZ = (2.0f * nearZ_ * farZ_) / (farZ_ + nearZ_ - zndc * (farZ_ - nearZ_));
                float dist = viewZ / maxf(0.05f, dot(rd, fwd_));
                dist = minf(dist, fog_.scatter * 40.0f + 6.0f);
                const int STEPS = 14;
                float acc = 0.0f;
                float stepLen = dist / STEPS;
                float trans = 1.0f;
                float jitter = 0.5f;
                for (int s = 1; s <= STEPS; ++s) {
                    float t = stepLen * (s - 0.5f + jitter);
                    V3 p = eye_ + rd * t;
                    float lit = sampleShadow(p) > 0.5f ? 1.0f : 0.0f;
                    // cone factor from the light
                    V3 toP = (p - fl_.pos);
                    float dl = length(toP);
                    V3 L = toP / maxf(dl, 1e-4f);
                    float cone = smoothstepf(fl_.outer, fl_.inner, dot(L, fl_.dir));
                    float falloff = saturate(1.0f - dl / (fl_.range * 1.1f));
                    acc += lit * cone * falloff * stepLen * trans;
                    trans *= std::exp(-fog_.density * stepLen * 1.5f);
                }
                scat[(size_t)y * bw + x] = acc * 0.011f;
            }
        }
        // separable 3-tap blur to remove step banding
        std::vector<float> tmp(scat.size());
        for (int y = 0; y < bh; ++y)
            for (int x = 0; x < bw; ++x) {
                float a = scat[(size_t)y * bw + maxi(0, x - 1)];
                float b = scat[(size_t)y * bw + x];
                float c = scat[(size_t)y * bw + mini(bw - 1, x + 1)];
                tmp[(size_t)y * bw + x] = (a + b * 2.0f + c) * 0.25f;
            }
        for (int y = 0; y < bh; ++y)
            for (int x = 0; x < bw; ++x) {
                float a = tmp[(size_t)maxi(0, y - 1) * bw + x];
                float b = tmp[(size_t)y * bw + x];
                float c = tmp[(size_t)mini(bh - 1, y + 1) * bw + x];
                scat[(size_t)y * bw + x] = (a + b * 2.0f + c) * 0.25f;
            }
    }

    // ---- bloom (quarter res)
    const float* scatSrc = scat.data();
    std::vector<float>& bA = bloomA_;
    std::vector<float>& bB = bloomB_;
    std::fill(bA.begin(), bA.end(), 0.0f);
    for (int y = 0; y < bh; ++y) {
        for (int x = 0; x < bw; ++x) {
            int sx = clampi(x * 4, 0, RW - 1), sy = clampi(y * 4, 0, RH - 1);
            size_t si = ((size_t)sy * RW + sx) * 3;
            V3 c(hdr_[si], hdr_[si + 1], hdr_[si + 2]);
            float lum = dot(c, V3(0.2126f, 0.7152f, 0.0722f));
            float k = smoothstepf(0.55f, 1.6f, lum);
            size_t bi = ((size_t)y * bw + x) * 3;
            bA[bi] = c.x * k; bA[bi + 1] = c.y * k; bA[bi + 2] = c.z * k;
        }
    }
    // two separable blurs
    auto blur = [&](std::vector<float>& src, std::vector<float>& dst, bool horiz, float radius) {
        int r = maxi(1, (int)radius);
        float wsum = 0;
        std::vector<float> wts(r + 1);
        for (int i = 0; i <= r; ++i) { wts[i] = std::exp(-(float)(i * i) / (2.0f * radius * radius)); wsum += (i == 0) ? wts[i] : 2 * wts[i]; }
        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                V3 acc(0, 0, 0);
                for (int i = -r; i <= r; ++i) {
                    int xx = horiz ? clampi(x + i, 0, bw - 1) : x;
                    int yy = horiz ? y : clampi(y + i, 0, bh - 1);
                    size_t si = ((size_t)yy * bw + xx) * 3;
                    float w = wts[abs(i)] / wsum;
                    acc += V3(src[si], src[si + 1], src[si + 2]) * w;
                }
                size_t di = ((size_t)y * bw + x) * 3;
                dst[di] = acc.x; dst[di + 1] = acc.y; dst[di + 2] = acc.z;
            }
        }
    };
    blur(bA, bB, true, 3.0f);
    blur(bB, bA, false, 3.0f);

    // ---- composite
    if ((int)fb_.size() != W_ * H_) fb_.assign((size_t)W_ * H_, 0xFF000000);
    const float invS = 1.0f / (float)RW;
    const float invSy = 1.0f / (float)RH;
    float time = (float)(nowSeconds());
    for (int y = 0; y < H_; ++y) {
        for (int x = 0; x < W_; ++x) {
            float u = (x + 0.5f) / (float)W_;
            float v = (y + 0.5f) / (float)H_;

            // screen warp (anomaly / possession / elevator)
            float warpX = 0, warpY = 0;
            if (post_.warp > 0.001f) {
                float band = std::sin(v * 30.0f + post_.warpPhase * 3.0f) * 0.5f + 0.5f;
                warpX = std::sin(v * 42.0f + post_.warpPhase) * post_.warp * 0.006f * band;
                warpY = std::sin(u * 26.0f + post_.warpPhase * 1.7f) * post_.warp * 0.003f;
            }
            if (post_.glitch > 0.001f) {
                float row = std::floor(v * 90.0f);
                float h = hashf((uint32_t)row * 917u + (uint32_t)(post_.warpPhase * 40.0f));
                if (h < post_.glitch * 0.35f) warpX += (h - 0.15f) * 0.10f;
            }
            float su = clampf(u + warpX, 0.0f, 1.0f) * RW - 0.5f;
            float sv = clampf(v + warpY, 0.0f, 1.0f) * RH - 0.5f;
            int hx0 = (int)su, hy0 = (int)sv;
            float htx = su - hx0, hty = sv - hy0;
            hx0 = clampi(hx0, 0, RW_ - 2); hy0 = clampi(hy0, 0, RH_ - 2);
            const float* h00 = &hdr_[((size_t)hy0 * RW_ + hx0) * 3];
            const float* h10 = h00 + 3;
            const float* h01 = h00 + (size_t)RW_ * 3;
            const float* h11 = h01 + 3;
            float w00 = (1 - htx) * (1 - hty), w10 = htx * (1 - hty);
            float w01 = (1 - htx) * hty, w11 = htx * hty;

            V3 col(h00[0] * w00 + h10[0] * w10 + h01[0] * w01 + h11[0] * w11,
                   h00[1] * w00 + h10[1] * w10 + h01[1] * w01 + h11[1] * w11,
                   h00[2] * w00 + h10[2] * w10 + h01[2] * w01 + h11[2] * w11);
            if (post_.aberration > 1.2f) {
                float ca = post_.aberration * 0.0018f * (0.25f + length(V2(u - 0.5f, v - 0.5f)));
                col.x = sampleHdr(clampf(su + ca * RW, 0.0f, RW_ - 1.0f), sv, 0);
                col.z = sampleHdr(clampf(su - ca * RW, 0.0f, RW_ - 1.0f), sv, 2);
            }

            // volumetric scatter (blurred quarter-res buffer, bilinear fetch)
            {
                float bxf = (u * RW) * 0.25f - 0.5f, byf = (v * RH) * 0.25f - 0.5f;
                int x0 = (int)bxf, y0 = (int)byf;
                float tx = bxf - x0, ty = byf - y0;
                x0 = clampi(x0, 0, bw - 2); y0 = clampi(y0, 0, bh - 2);
                const float* s00 = &scatSrc[(size_t)y0 * bw + x0];
                const float* s01 = s00 + bw;
                float s = (s00[0] * (1 - tx) + s00[1] * tx) * (1 - ty) +
                          (s01[0] * (1 - tx) + s01[1] * tx) * ty;
                col.x += fl_.color.x * (s * fl_.intensity * 0.55f);
                col.y += fl_.color.y * (s * fl_.intensity * 0.55f);
                col.z += fl_.color.z * (s * fl_.intensity * 0.55f);
            }

            // bloom (already blurred: a nearest fetch is enough)
            {
                int bx = clampi((int)((u * RW) * 0.25f), 0, bw - 1);
                int by = clampi((int)((v * RH) * 0.25f), 0, bh - 1);
                const float* bl = &bA[((size_t)by * bw + bx) * 3];
                col.x += bl[0] * post_.bloom;
                col.y += bl[1] * post_.bloom;
                col.z += bl[2] * post_.bloom;
            }

            // exposure + tonemap
            col = col * post_.exposure;
            col = aces(col);

            // grade
            float lum = dot(col, V3(0.2126f, 0.7152f, 0.0722f));
            V3 tint = lerp(post_.tintShadow, post_.tintHigh, saturate(lum));
            col = col * tint;
            col = V3(lerpf(lum, col.x, post_.saturation), lerpf(lum, col.y, post_.saturation), lerpf(lum, col.z, post_.saturation));
            col = V3(saturate((col.x - 0.5f) * post_.contrast + 0.5f + post_.lift),
                     saturate((col.y - 0.5f) * post_.contrast + 0.5f + post_.lift),
                     saturate((col.z - 0.5f) * post_.contrast + 0.5f + post_.lift));

            // vignette + heartbeat
            float vig = 1.0f - post_.vignette * (length(V2(u - 0.5f, v - 0.5f)) * 1.35f);
            vig = clampf(vig, 0.0f, 1.0f);
            vig *= 1.0f - post_.pulse * 0.55f * saturate(length(V2(u - 0.5f, v - 0.5f)) * 1.6f);
            col *= vig;

            // film grain
            if (post_.grain > 0.001f) {
                float g = hashf((uint32_t)(x * 1973 + y * 9277) + (uint32_t)(time * 61.0f) * 7919u);
                float amt = (g - 0.5f) * post_.grain;
                col += V3(amt, amt, amt);
            }

            col = V3(saturate(col.x), saturate(col.y), saturate(col.z));
            if (post_.fade > 0.001f) col = lerp(col, post_.fadeColor, saturate(post_.fade));

            int r = (int)(linToSrgbFast(col.x) * 255.0f + 0.5f);
            int g = (int)(linToSrgbFast(col.y) * 255.0f + 0.5f);
            int b = (int)(linToSrgbFast(col.z) * 255.0f + 0.5f);
            fb_[(size_t)y * W_ + x] = rgba(r, g, b, 255);
        }
    }
}


float Renderer::sampleHdr(float x, float y, int ch) const {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float tx = x - x0, ty = y - y0;
    auto get = [&](int xx, int yy) {
        xx = clampi(xx, 0, RW_ - 1); yy = clampi(yy, 0, RH_ - 1);
        return hdr_[((size_t)yy * RW_ + xx) * 3 + ch];
    };
    return lerpf(lerpf(get(x0, y0), get(x0 + 1, y0), tx), lerpf(get(x0, y0 + 1), get(x0 + 1, y0 + 1), tx), ty);
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
