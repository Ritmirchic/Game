// FRACTURE — software reference renderer (headless verification + no-GPU fallback).
#include "core/engine.h"
#include "core/png.h"
#include "core/fontatlas.h"
#include "render/shading.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace {

struct CV {
    vec4 c;
    vec3 wp;
    float ao;
};

struct SoftRenderer : IRenderer {
    int W = 0, H = 0;
    std::vector<uint32_t> color;
    std::vector<float> depth;
    Camera cam;
    EnvState env;
    Settings settings;
    std::vector<MeshGPU*> meshes;
    struct DynMeshData { std::vector<VoxVert> verts; std::vector<uint32_t> idx; };
    std::vector<DynMeshData> meshData;

    bool init(void*, int w, int h, const Settings& st) override {
        W = w; H = h; settings = st;
        color.assign((size_t)W * H, 0xFF0A0C10u);
        depth.assign((size_t)W * H, 0.0f);
        return true;
    }
    void shutdown() override {}
    void resize(int w, int h) override {
        W = w; H = h;
        color.assign((size_t)W * H, 0xFF0A0C10u);
        depth.assign((size_t)W * H, 0.0f);
    }
    void setSettings(const Settings& st) override { settings = st; }
    const char* name() const override { return "Software (CPU reference)"; }
    int frameWidth() const override { return W; }
    int frameHeight() const override { return H; }

    // ---------------- sky ----------------
    void drawSky() {
        vec3 f = cam.forward(), r = cam.right(), u = cross(r, f);
        float aspect = (float)W / (float)H;
        float th = std::tan(cam.fovY * 0.5f);
        float t = env.time;
        for (int y = 0; y < H; y++) {
            float sy = (1.0f - 2.0f * ((y + 0.5f) / H)) * th;
            for (int x = 0; x < W; x++) {
                float sx = (2.0f * ((x + 0.5f) / W) - 1.0f) * th * aspect;
                vec3 d = (f + r * sx + u * sy).norm();
                float h = clampf(d.y * 1.7f + 0.16f, 0, 1);
                vec3 sky = vlerp(env.skyHorizon, env.skyTop, smoothstepf(0, 1, h));
                sky = vlerp(sky, env.fogColor, (1.0f - clampf(std::fabs(d.y) * 4.0f, 0, 1)) * 0.55f);
                float sd = std::max(0.0f, dot(d, env.sunDir));
                float sunIntensity = env.sunIntensity * (1.0f - env.cloudCover * 0.85f);
                sky = sky + env.sunColor * (std::pow(sd, 24.0f) * 0.28f + std::pow(sd, 1600.0f) * 12.0f) * sunIntensity;
                if (env.cloudCover > 0.03f && d.y > 0.015f) {
                    vec3 cp = vec3(d.x, 0, d.z) * (1.0f / std::max(0.06f, d.y));
                    float n = valueNoise3(vec3(cp.x * 0.42f + t * 0.02f, cp.z * 0.42f, t * 0.01f), 991) * 0.62f +
                              0.38f * valueNoise3(vec3(cp.x * 1.3f, cp.z * 1.3f + t * 0.03f, 3.0f), 992);
                    float cover = smoothstepf(0.62f - env.cloudCover * 0.4f, 0.86f, n);
                    vec3 cLow = vec3(0.30f, 0.32f, 0.36f), cHigh = vec3(1.05f, 1.02f, 0.97f);
                    vec3 cloudCol = vlerp(cLow, cHigh, clampf(env.sunColor.y * 0.85f, 0, 1));
                    sky = vlerp(sky, cloudCol, cover * env.cloudCover * smoothstepf(0.0f, 0.18f, d.y));
                }
                color[(size_t)y * W + x] = packColor(sky.x, sky.y, sky.z);
            }
        }
        // lightning
        if (env.lightning > 0.01f) {
            float t2 = env.lightning * 0.8f;
            for (auto& c : color) {
                int rr = (c >> 24) & 0xFF, gg = (c >> 16) & 0xFF, bb = (c >> 8) & 0xFF;
                rr = (int)std::min(255.0f, rr + 240 * t2);
                gg = (int)std::min(255.0f, gg + 240 * t2);
                bb = (int)std::min(255.0f, bb + 255 * t2);
                c = (uint32_t)rr << 24 | (uint32_t)gg << 16 | (uint32_t)bb << 8 | 0xFF;
            }
        }
    }

    // ---------------- raster ----------------
    void shadePixel(const vec3& wp, const vec3& nv, uint8_t mat, float ao, float alphaMul, bool additive,
                    const vec3& tint, float* outR, float* outG, float* outB, float* outA) {
        const Material& mt = material(mat);
        ShadeDetail sd;
        shadeVoxelDetail(mat, wp, nv, mt.color, ao, sd);
        vec3 base = sd.color;
        if (tint.len2() > 0.0001f) base = tint;
        float ndl = std::max(0.0f, dot(nv, env.sunDir));
        float skyAmt = 0.5f + 0.5f * nv.y;
        float aof = 0.32f + 0.68f * ao;
        vec3 sun = env.sunColor * (env.sunIntensity * 1.5f);
        vec3 amb = env.ambientSky * skyAmt * aof + env.ambientGround * (1 - skyAmt) * aof * 0.5f;
        vec3 lighting = amb + sun * ndl * (0.32f + 0.68f * aof);
        vec3 V = (cam.pos - wp).norm();
        vec3 Hv = (env.sunDir + V).norm();
        float shin = 2.0f / std::max(0.04f, sd.rough) * 16.0f;
        float spec = std::pow(std::max(0.0f, dot(nv, Hv)), std::min(4000.0f, shin));
        lighting = lighting + sun * spec * (0.2f + 0.8f * mt.metallic) * (0.35f + 0.65f * (1 - sd.rough));
        if (env.wetness > 0.01f) {
            float w = env.wetness * (nv.y > 0.5f ? 1.0f : 0.5f);
            lighting = lighting + sun * std::pow(std::max(0.0f, dot(nv, Hv)), 120.0f) * w * 1.4f;
            lighting = lighting * (1.0f - w * 0.20f);
        }
        vec3 col = base * lighting + base * sd.emissive * 2.2f;
        col = col * env.exposure;
        col = vec3(col.x * (2.51f * col.x + 0.03f) / (col.x * (2.43f * col.x + 0.59f) + 0.14f),
                   col.y * (2.51f * col.y + 0.03f) / (col.y * (2.43f * col.y + 0.59f) + 0.14f),
                   col.z * (2.51f * col.z + 0.03f) / (col.z * (2.43f * col.z + 0.59f) + 0.14f));
        float dist = (wp - cam.pos).len();
        float fog = clampf(1.0f - std::exp(-dist * env.fogDensity), 0, 0.985f);
        col = vlerp(col, vlerp(env.fogColor, env.skyHorizon, 0.2f), fog);
        *outR = col.x; *outG = col.y; *outB = col.z;
        *outA = additive ? alphaMul : (mt.transparent ? alphaMul * sd.alpha : alphaMul);
    }

    void rasterTri(const CV& a, const CV& b, const CV& c, uint8_t mat, uint8_t nrm, bool additive, float alphaMul,
                   const vec3& tint) {
        float invw[3] = {1.0f / a.c.w, 1.0f / b.c.w, 1.0f / c.c.w};
        const CV* v[3] = {&a, &b, &c};
        float sx[3], sy[3];
        for (int i = 0; i < 3; i++) {
            sx[i] = (v[i]->c.x * invw[i] * 0.5f + 0.5f) * W;
            sy[i] = (1.0f - (v[i]->c.y * invw[i] * 0.5f + 0.5f)) * H;
        }
        float minx = std::max(0.0f, std::floor(std::min({sx[0], sx[1], sx[2]})));
        float maxx = std::min((float)W - 1, std::ceil(std::max({sx[0], sx[1], sx[2]})));
        float miny = std::max(0.0f, std::floor(std::min({sy[0], sy[1], sy[2]})));
        float maxy = std::min((float)H - 1, std::ceil(std::max({sy[0], sy[1], sy[2]})));
        if (minx > maxx || miny > maxy) return;
        float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
        if (std::fabs(area) < 1e-8f) return;
        float invArea = 1.0f / area;
        vec3 nv = normalOf(nrm);
        const Material& mt = material(mat);
        bool blend = additive || mt.transparent;
        for (int y = (int)miny; y <= (int)maxy; y++) {
            float py = y + 0.5f;
            for (int x = (int)minx; x <= (int)maxx; x++) {
                float px = x + 0.5f;
                float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * invArea;
                float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * invArea;
                float w2 = 1.0f - w0 - w1;
                if (w0 < -0.0001f || w1 < -0.0001f || w2 < -0.0001f) continue;
                float iw = w0 * invw[0] + w1 * invw[1] + w2 * invw[2];
                if (iw <= 0) continue;
                size_t di = (size_t)y * W + x;
                if (iw <= depth[di] && !blend) continue;
                vec3 wp = (a.wp * (w0 * invw[0]) + b.wp * (w1 * invw[1]) + c.wp * (w2 * invw[2])) * (1.0f / iw);
                float ao = (a.ao * (w0 * invw[0]) + b.ao * (w1 * invw[1]) + c.ao * (w2 * invw[2])) * (1.0f / iw);
                float r, g, bb, al;
                shadePixel(wp, nv, mat, ao, alphaMul, additive, tint, &r, &g, &bb, &al);
                uint32_t dst = color[di];
                float dr = ((dst >> 24) & 0xFF) / 255.0f, dg = ((dst >> 16) & 0xFF) / 255.0f, db = ((dst >> 8) & 0xFF) / 255.0f;
                float orr, og, ob;
                if (additive) { orr = dr + r * al; og = dg + g * al; ob = db + bb * al; }
                else if (al < 0.999f) { orr = lerpf(dr, r, al); og = lerpf(dg, g, al); ob = lerpf(db, bb, al); }
                else { orr = r; og = g; ob = bb; }
                color[di] = ((uint32_t)(clampf(orr, 0, 1) * 255) << 24) | ((uint32_t)(clampf(og, 0, 1) * 255) << 16) |
                            ((uint32_t)(clampf(ob, 0, 1) * 255) << 8) | 0xFFu;
                if (!blend) depth[di] = iw;
            }
        }
    }

    void addTri(const vec3& p0, const vec3& p1, const vec3& p2, float ao0, float ao1, float ao2, uint8_t mat,
                uint8_t nrm, bool additive = false, float alpha = 1.0f, const vec3& tint = vec3(0)) {
        mat4 vp = cam.viewProj();
        CV poly[3] = {{vp * vec4(p0, 1), p0, ao0}, {vp * vec4(p1, 1), p1, ao1}, {vp * vec4(p2, 1), p2, ao2}};
        CV out[8];
        int m = 0;
        const float eps = 0.03f;
        for (int i = 0; i < 3; i++) {
            const CV& A = poly[i];
            const CV& B = poly[(i + 1) % 3];
            float da = A.c.w - eps, db = B.c.w - eps;
            if (da >= 0) out[m++] = A;
            if ((da >= 0) != (db >= 0)) {
                float t = da / (da - db);
                CV C;
                C.c = vec4(vlerp(vec3(A.c.x, A.c.y, A.c.z), vec3(B.c.x, B.c.y, B.c.z), t), lerpf(A.c.w, B.c.w, t));
                C.wp = vlerp(A.wp, B.wp, t);
                C.ao = lerpf(A.ao, B.ao, t);
                out[m++] = C;
            }
        }
        if (m < 3) return;
        for (int i = 1; i + 1 < m; i++) rasterTri(out[0], out[i], out[i + 1], mat, nrm, additive, alpha, tint);
    }

    void drawChunkData(const std::vector<VoxVert>& verts, const std::vector<uint32_t>& idx, int pass) {
        // pass 0 = opaque, 1 = water, 2 = other transparent (glass)
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            const VoxVert& a = verts[idx[i]];
            const VoxVert& b = verts[idx[i + 1]];
            const VoxVert& c = verts[idx[i + 2]];
            bool isWater = (a.flags & 2) != 0;
            const Material& mt = material(a.mat);
            int triPass = isWater ? 1 : (mt.transparent ? 2 : 0);
            if (triPass != pass) continue;
            vec3 pa(a.x, a.y, a.z), pb(b.x, b.y, b.z), pc(c.x, c.y, c.z);
            if (isWater) {
                float t = env.time * 1.7f;
                pa.y += std::sin(pa.x * 1.9f + t) * 0.04f + std::sin(pa.z * 2.6f + t * 1.3f) * 0.03f;
                pb.y += std::sin(pb.x * 1.9f + t) * 0.04f + std::sin(pb.z * 2.6f + t * 1.3f) * 0.03f;
                pc.y += std::sin(pc.x * 1.9f + t) * 0.04f + std::sin(pc.z * 2.6f + t * 1.3f) * 0.03f;
            }
            addTri(pa, pb, pc, a.ao / 255.0f, b.ao / 255.0f, c.ao / 255.0f, a.mat, a.n);
        }
    }

    MeshGPU* createMesh() override {
        MeshGPU* m = new MeshGPU();
        meshes.push_back(m);
        meshData.emplace_back();
        return m;
    }
    void updateMesh(MeshGPU* m, const std::vector<VoxVert>& verts, const std::vector<uint32_t>& idx) override {
        for (size_t i = 0; i < meshes.size(); i++) {
            if (meshes[i] != m) continue;
            meshData[i].verts = verts;
            meshData[i].idx = idx;
            m->indexCount = (int)idx.size();
            m->ready = true;
            return;
        }
    }
    void destroyMesh(MeshGPU* m) override { (void)m; }
    void drawMesh(MeshGPU* m, const mat4& model) override {
        for (size_t i = 0; i < meshes.size(); i++) {
            if (meshes[i] != m || !m->ready) continue;
            std::vector<VoxVert> tmp = meshData[i].verts;
            for (auto& v : tmp) {
                vec3 p = model.point(vec3(v.x, v.y, v.z));
                v.x = p.x; v.y = p.y; v.z = p.z;
            }
            for (int pass = 0; pass < 3; pass++) drawChunkData(tmp, meshData[i].idx, pass);
        }
    }

    bool chunkVisible(const AABB& b) {
        if ((b.center() - cam.pos).len() > settings.viewDistance) return false;
        mat4 vp = cam.viewProj();
        for (int i = 0; i < 8; i++) {
            vec3 p((i & 1) ? b.mx.x : b.mn.x, (i & 2) ? b.mx.y : b.mn.y, (i & 4) ? b.mx.z : b.mn.z);
            vec4 c = vp * vec4(p, 1);
            if (c.w > 0.02f && std::fabs(c.x) < c.w * 1.4f && std::fabs(c.y) < c.w * 1.4f) return true;
        }
        return (b.center() - cam.pos).len() < 2.0f;
    }

    void drawWorld(World& world, const Settings& st) override {
        (void)st;
        for (int pass = 0; pass < 3; pass++)
            for (auto& kv : world.meshes) {
                ChunkMesh& m = kv.second;
                if (m.idx.empty()) continue;
                if (!chunkVisible(m.bounds)) continue;
                drawChunkData(m.verts, m.idx, pass);
            }
    }
    void drawWater(World&, const EnvState&) override {}

    void drawBoxes(const BoxInstance* boxes, int count) override {
        static const vec3 cv[8] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
                                    {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
        static const int faces[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {3, 2, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {0, 4, 7, 3}};
        static const uint8_t faceN[6] = {4, 5, 2, 3, 0, 1};
        for (int bi = 0; bi < count; bi++) {
            const BoxInstance& b = boxes[bi];
            vec3 p[8];
            for (int i = 0; i < 8; i++) p[i] = b.model.point(cv[i]);
            for (int f = 0; f < 6; f++) {
                vec3 nn = cross(p[faces[f][1]] - p[faces[f][0]], p[faces[f][2]] - p[faces[f][0]]).norm();
                float ndl = std::max(0.0f, dot(nn, env.sunDir));
                float amb = 0.45f + 0.55f * std::max(0.0f, nn.y);
                vec3 lighting = env.ambientSky * amb * 0.75f + env.sunColor * ndl * env.sunIntensity * 1.1f;
                vec3 c = b.color * lighting + b.emis;
                float spec = std::pow(std::max(0.0f, dot(nn, (env.sunDir + (cam.pos - p[faces[f][0]]).norm()).norm())), 90.0f);
                c = c + vec3(spec * (1 - b.rough) * 1.5f) * (0.3f + b.metal);
                c = c * env.exposure;
                c = vec3(c.x * (2.51f * c.x + 0.03f) / (c.x * (2.43f * c.x + 0.59f) + 0.14f),
                         c.y * (2.51f * c.y + 0.03f) / (c.y * (2.43f * c.y + 0.59f) + 0.14f),
                         c.z * (2.51f * c.z + 0.03f) / (c.z * (2.43f * c.z + 0.59f) + 0.14f));
                float dist = (p[faces[f][0]] - cam.pos).len();
                c = vlerp(c, env.fogColor, clampf(1 - std::exp(-dist * env.fogDensity), 0, 1));
                // box instances use a flat material (no voxel pattern) - pass a fake material that
                // shades as plain albedo, then tint with the requested colour.
                addTri(p[faces[f][0]], p[faces[f][1]], p[faces[f][2]], 1, 1, 1, M_AIR, faceN[f], false, 1.0f, c);
                addTri(p[faces[f][0]], p[faces[f][2]], p[faces[f][3]], 1, 1, 1, M_AIR, faceN[f], false, 1.0f, c);
            }
        }
    }

    void drawParticles(const Particle* p, int count) override {
        vec3 right = cam.right(), up = cross(right, cam.forward());
        for (int i = 0; i < count; i++) {
            const Particle& pt = p[i];
            if (pt.life <= 0 || pt.size <= 0) continue;
            float a = clampf(pt.life / std::max(0.001f, pt.maxLife), 0, 1);
            float size = pt.size;
            vec3 c0 = right * size + up * size;
            vec3 c1 = right * -size + up * size;
            vec3 c2 = right * -size - up * size;
            vec3 c3 = right * size - up * size;
            bool additive = pt.kind == 2 || pt.kind == 3;
            float alpha = additive ? a * a : ((pt.kind == 0 || pt.kind == 1 || pt.kind == 7) ? a * 0.42f : a);
            vec3 col(((pt.color >> 24) & 0xFF) / 255.0f, ((pt.color >> 16) & 0xFF) / 255.0f, ((pt.color >> 8) & 0xFF) / 255.0f);
            if (!additive) {
                col = col * (env.ambientSky * 0.95f + env.sunColor * 0.55f);
                float dist = (pt.pos - cam.pos).len();
                col = vlerp(col, env.fogColor, clampf(1 - std::exp(-dist * env.fogDensity), 0, 1));
                col = vec3(col.x / (1 + col.x * 0.6f), col.y / (1 + col.y * 0.6f), col.z / (1 + col.z * 0.6f));
            } else {
                col = col * 1.5f;
            }
            addTri(pt.pos + c0, pt.pos + c1, pt.pos + c2, 1, 1, 1, M_AIR, 0, additive, alpha, col);
            addTri(pt.pos + c0, pt.pos + c2, pt.pos + c3, 1, 1, 1, M_AIR, 0, additive, alpha, col);
        }
    }

    void drawOverlay(const UIDrawList& ui) override {
        const FontAtlas& fa = fontAtlas();
        for (size_t i = 0; i + 2 < ui.verts.size(); i += 3) uiTri(ui.verts[i], ui.verts[i + 1], ui.verts[i + 2], fa);
    }
    void uiTri(const UIVert& a, const UIVert& b, const UIVert& c, const FontAtlas& fa) {
        float minx = std::max(0.0f, std::min({a.x, b.x, c.x}));
        float maxx = std::min((float)W - 1, std::max({a.x, b.x, c.x}));
        float miny = std::max(0.0f, std::min({a.y, b.y, c.y}));
        float maxy = std::min((float)H - 1, std::max({a.y, b.y, c.y}));
        if (minx > maxx || miny > maxy) return;
        float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if (std::fabs(area) < 1e-6f) return;
        float invArea = 1.0f / area;
        for (int y = (int)miny; y <= (int)maxy; y++)
            for (int x = (int)minx; x <= (int)maxx; x++) {
                float px = x + 0.5f, py = y + 0.5f;
                float w0 = ((b.x - px) * (c.y - py) - (c.x - px) * (b.y - py)) * invArea;
                float w1 = ((c.x - px) * (a.y - py) - (a.x - px) * (c.y - py)) * invArea;
                float w2 = 1 - w0 - w1;
                if (w0 < -0.0001f || w1 < -0.0001f || w2 < -0.0001f) continue;
                float u = w0 * a.u + w1 * b.u + w2 * c.u;
                float v = w0 * a.v + w1 * b.v + w2 * c.v;
                // UI colours are packed 0xAARRGGBB
                float cr = ((a.color >> 16) & 0xFF) * w0 + ((b.color >> 16) & 0xFF) * w1 + ((c.color >> 16) & 0xFF) * w2;
                float cg = ((a.color >> 8) & 0xFF) * w0 + ((b.color >> 8) & 0xFF) * w1 + ((c.color >> 8) & 0xFF) * w2;
                float cb = (a.color & 0xFF) * w0 + (b.color & 0xFF) * w1 + (c.color & 0xFF) * w2;
                float ca = (((a.color >> 24) & 0xFF) * w0 + ((b.color >> 24) & 0xFF) * w1 + ((c.color >> 24) & 0xFF) * w2) / 255.0f;
                int tx = (int)(u * fa.width), ty = (int)(v * fa.height);
                float texA = 0;
                if (tx >= 0 && tx < fa.width && ty >= 0 && ty < fa.height) texA = fa.data[ty * fa.width + tx] / 255.0f;
                float alpha = ca * texA;
                if (alpha <= 0.002f) continue;
                size_t di = (size_t)y * W + x;
                uint32_t dst = color[di];
                float dr = ((dst >> 24) & 0xFF), dg = ((dst >> 16) & 0xFF), db = ((dst >> 8) & 0xFF);
                dr = dr + (cr - dr) * alpha;
                dg = dg + (cg - dg) * alpha;
                db = db + (cb - db) * alpha;
                color[di] = ((uint32_t)clampf(dr, 0, 255) << 24) | ((uint32_t)clampf(dg, 0, 255) << 16) |
                            ((uint32_t)clampf(db, 0, 255) << 8) | 0xFFu;
            }
    }

    void setViewportOverride(int x, int y, int w, int h, bool depthClear) override {
        if (!depthClear) return;
        for (int yy = std::max(0, y); yy < std::min(H, y + h); yy++)
            for (int xx = std::max(0, x); xx < std::min(W, x + w); xx++) depth[(size_t)yy * W + xx] = 0;
    }
    void clearViewportOverride() override {}
    void beginFrame(const Camera& c, const EnvState& e, const Settings& st) override {
        cam = c; env = e; settings = st;
        std::fill(color.begin(), color.end(), 0xFF000000u);
        std::fill(depth.begin(), depth.end(), 0.0f);
        drawSky();
    }
    void endFrame() override {}
    bool screenshot(const char* path) override { return writePNG(path, color.data(), W, H); }
    const uint32_t* framebuffer() const override { return color.data(); }
};

} // namespace

IRenderer* createSoftRenderer() { return new SoftRenderer(); }
