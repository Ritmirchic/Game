// FRACTURE — software renderer (headless screenshots / fallback path).
// Shares the exact scene submission path with the GL backend; shading math
// mirrors the GLSL (minus shadow maps and normal mapping).
#include "render.h"
#include "font_data.h"
#include <unordered_map>

namespace ft {

// ---------------------------------------------------------------- null ------
namespace {
class NullBackend : public IBackend {
public:
    const char* name() const override { return "null"; }
    u32 createMesh(const std::vector<u8>&, const std::vector<u32>&) override { return nextH++; }
    void updateMesh(u32, const std::vector<u8>&, const std::vector<u32>&) override {}
    void destroyMesh(u32) override {}
    void beginScene(const Camera&, f32, const SkyParams&, const std::vector<LightInfo>&, const SpotInfo&) override {}
    void drawChunk(const VoxWorld&, u32) override {}
    void drawMesh(u32, const Mat4&, f32, f32, f32, u8) override {}
    void drawParticles(const PInst*, int) override {}
    void renderScene() override {}
    void beginUI() override {}
    void pushRect(f32, f32, f32, f32, u8, u8, u8, u8) override {}
    void pushQuad(const UIQuad&) override {}
    void renderUI() override {}
    void present() override {}
    void readPixels(std::vector<u8>& rgb) override { rgb.clear(); }
private:
    u32 nextH = 1;
};
} // namespace

IBackend* createNullBackend() { return new NullBackend(); }

// ------------------------------------------------------------- software -----
namespace {

struct VSOut {
    f32 sx, sy, ndcz, invW;
    f32 wx, wy, wz;
    f32 viewDist;
    Vec3 lit;
    f32 ao;
};

class SWBackend : public IBackend {
public:
    const char* name() const override { return "software"; }

    bool init(void*, int w, int h, int, bool) override { resize(w, h); return true; }
    void resize(int w, int h) override {
        wW = w; wH = h;
        fb.assign((size_t)w * h * 4, 0);
        zb.assign((size_t)w * h, 1.f);
    }

    u32 createMesh(const std::vector<u8>& verts, const std::vector<u32>& idx) override {
        u32 h = nextH++;
        meshes[h] = DynMesh{verts, idx};
        return h;
    }
    void updateMesh(u32 h, const std::vector<u8>& verts, const std::vector<u32>& idx) override {
        auto it = meshes.find(h);
        if (it != meshes.end()) { it->second.verts = verts; it->second.idx = idx; }
        else meshes[h] = DynMesh{verts, idx};
    }
    void destroyMesh(u32 h) override { meshes.erase(h); }

    void beginScene(const Camera& cam, f32 aspect, const SkyParams& sky,
                    const std::vector<LightInfo>& lights, const SpotInfo& spot) override {
        cam_ = cam; sky_ = sky; lights_ = &lights; spot_ = spot;
        view_ = cam.view();
        proj_ = cam.proj(aspect);
        viewProj_ = proj_ * view_;
        invViewProj_ = viewProj_.inverse();
        draws_.clear(); parts_.clear(); waterDraws_.clear();
        curWorld_ = nullptr;
        tanHalf_ = std::tan(cam.fovY * 0.5f);
        aspect_ = aspect;
    }
    void drawChunk(const VoxWorld& w, u32 chunkIdx) override {
        curWorld_ = &w;
        const ChunkMesh& m = w.meshes[chunkIdx];
        if (m.idx.empty() && m.tidx.empty()) return;
        Draw d; d.kind = 0; d.chunk = chunkIdx; d.flags = 0;
        draws_.push_back(d);
        if (!m.tidx.empty()) waterDraws_.push_back(d);
    }
    void drawMesh(u32 h, const Mat4& xform, f32 tr, f32 tg, f32 tb, u8 flags) override {
        auto it = meshes.find(h);
        if (it == meshes.end() || it->second.idx.empty()) return;
        Draw d; d.kind = 1; d.handle = h; d.xform = xform;
        d.tint[0] = tr; d.tint[1] = tg; d.tint[2] = tb; d.flags = flags;
        if (flags & 1) waterDraws_.push_back(d);   // transparent
        else draws_.push_back(d);
    }
    void drawParticles(const PInst* p, int n) override {
        parts_.insert(parts_.end(), p, p + n);
    }

    void renderScene() override {
        std::fill(fb.begin(), fb.end(), 0);
        std::fill(zb.begin(), zb.end(), 1.f);
        // sky
        drawSky();
        // opaque
        for (const Draw& d : draws_) drawOne(d);
        // transparent water / blended meshes (depth test, no sort — single layer typical)
        for (const Draw& d : waterDraws_) drawOne(d);
        // particles
        drawParticlesNow();
    }

    void beginUI() override { quads_.clear(); }
    void pushRect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a) override {
        UIQuad q; q.x = x; q.y = y; q.w = w; q.h = h;
        q.u0 = 0; q.v0 = 0; q.u1 = 1; q.v1 = 1;
        q.r = r; q.g = g; q.b = b; q.a = a; q.tex = 0;
        quads_.push_back(q);
    }
    void pushQuad(const UIQuad& q) override { quads_.push_back(q); }
    void renderUI() override {
        for (const UIQuad& q : quads_) drawUIQuad(q);
    }
    void present() override {}
    void readPixels(std::vector<u8>& rgb) override {
        rgb.resize((size_t)wW * wH * 3);
        size_t o = 0;
        for (size_t i = 0; i < fb.size(); i += 4) {
            rgb[o++] = fb[i]; rgb[o++] = fb[i + 1]; rgb[o++] = fb[i + 2];
        }
    }

private:
    struct DynMesh { std::vector<u8> verts; std::vector<u32> idx; };
    struct Draw {
        u8 kind;             //0 chunk,1 dynamic
        u32 chunk, handle;
        Mat4 xform;
        f32 tint[3] = {1, 1, 1};
        u8 flags = 0;
    };

    std::vector<u8> fb;
    std::vector<f32> zb;
    std::unordered_map<u32, DynMesh> meshes;
    u32 nextH = 1;

    Camera cam_;
    SkyParams sky_;
    const std::vector<LightInfo>* lights_ = nullptr;
    SpotInfo spot_;
    Mat4 view_, proj_, viewProj_, invViewProj_;
    std::vector<Draw> draws_, waterDraws_;
    const VoxWorld* curWorld_ = nullptr;
    std::vector<PInst> parts_;
    std::vector<UIQuad> quads_;
    f32 tanHalf_ = 0.7f, aspect_ = 1.77f;

    inline void putPixel(int x, int y, f32 z, u8 r, u8 g, u8 b, u8 a, bool testZ, bool writeZ) {
        if (x < 0 || y < 0 || x >= wW || y >= wH) return;
        size_t i = (size_t)y * wW + x;
        if (testZ && z >= zb[i]) return;
        if (a == 255) {
            fb[i * 4 + 0] = r; fb[i * 4 + 1] = g; fb[i * 4 + 2] = b; fb[i * 4 + 3] = 255;
        } else {
            f32 af = a / 255.f;
            fb[i * 4 + 0] = (u8)(r * af + fb[i * 4 + 0] * (1 - af));
            fb[i * 4 + 1] = (u8)(g * af + fb[i * 4 + 1] * (1 - af));
            fb[i * 4 + 2] = (u8)(b * af + fb[i * 4 + 2] * (1 - af));
            fb[i * 4 + 3] = 255;
        }
        if (writeZ) zb[i] = z;
    }

    // ---- shading (mirrors GLSL) -------------------------------------------
    Vec3 shadeVertex(const Vec3& wp, const Vec3& N, const Vec3& base, f32 aoF, u8 mat) const {
        const MatGpu* mg = matGpuTable();
        Vec3 L = norm(-sky_.sunDir);
        f32 diff = tmax(0.f, dot(N, L));
        Vec3 hemi = lerp(sky_.horizon * 0.40f, sky_.zenith * 0.62f, N.y * 0.5f + 0.5f) * sky_.ambient;
        Vec3 sunC = sky_.sunColor * (sky_.sunIntensity * diff);
        Vec3 V = norm(cam_.pos - wp);
        Vec3 H = norm(L + V);
        f32 shin = 2.f + (1.f - tclamp(mg[mat].rough, 0.05f, 1.f)) * 90.f;
        f32 specAmt = mg[mat].metal > 0.5f ? 0.55f : 0.10f;
        Vec3 spec = sky_.sunColor * (specAmt * diff * std::pow(tmax(0.f, dot(N, H)), shin) * sky_.sunIntensity);
        Vec3 total = hemi + sunC + spec;
        if (lights_) {
            for (const LightInfo& li : *lights_) {
                Vec3 dl = li.pos - wp;
                f32 d = len(dl);
                if (d >= li.radius) continue;
                f32 att = 1.f - d / li.radius;
                att *= att;
                f32 nd = tmax(0.f, dot(N, dl / tmax(d, 1e-4f)));
                total += li.color * (nd * att * li.intensity);
            }
        }
        if (spot_.intensity > 0) {
            Vec3 toP = cam_.pos - wp;
            f32 d = len(toP);
            if (d < spot_.range) {
                f32 cd = dot(norm(-toP), spot_.dir);  // frag → light dir vs spot dir
                f32 cone = tclamp((cd - spot_.angleCos) / (1.f - spot_.angleCos), 0.f, 1.f);
                f32 att = (1.f - d / spot_.range);
                total += spot_.color * (cone * cone * att * spot_.intensity * tmax(0.f, dot(N, norm(toP))) );
            }
        }
        if (sky_.flash) total += Vec3(1.f, 0.96f, 0.88f) * (sky_.flash / 255.f * 2.4f);
        // fresh (interior) faces read darker raw material — handled by texture slot
        total = total * aoF;
        (void)mg;
        return base.mul(total);
    }

    f32 fogFactor(f32 dist) const {
        f32 f = sky_.fogDensity * dist;
        f = std::exp(-f * f);
        return tclamp(1.f - f, 0.f, 1.f);
    }

    // ---- triangle raster ---------------------------------------------------
    void rasterTri(VSOut v[3], u8 mat, u8 vflags, bool blend, u8 alphaMul, bool cull) {
        // near clip (simple reject if any vertex behind near plane → clip would be better;
        // do a cheap clip against ndc z <1 & w>near using parametric clipping)
        const f32 NEAR = 0.02f;
        VSOut buf[6];
        int n = 0;
        // clip against w >= NEAR (view depth)
        auto clipEdge = [&](const VSOut& a, const VSOut& b, f32& outN) -> VSOut {
            f32 da = a.invW > 0 ? (1.f / a.invW) - NEAR : -(NEAR);   // viewDist - NEAR
            f32 db = b.invW > 0 ? (1.f / b.invW) - NEAR : -(NEAR);
            f32 t = da / (da - db);
            VSOut r;
            auto lerpS = [&](f32 A, f32 B) { return lerpf(A, B, t); };
            r.sx = lerpS(a.sx, b.sx); r.sy = lerpS(a.sy, b.sy); r.ndcz = lerpS(a.ndcz, b.ndcz);
            r.invW = lerpS(a.invW, b.invW); r.wx = lerpS(a.wx, b.wx); r.wy = lerpS(a.wy, b.wy); r.wz = lerpS(a.wz, b.wz);
            r.viewDist = lerpS(a.viewDist, b.viewDist);
            r.lit = lerp(a.lit, b.lit, t); r.ao = lerpS(a.ao, b.ao);
            outN = t;
            return r;
        };
        // Sutherland–Hodgman vs viewDist >= NEAR
        VSOut poly[6]; int pn = 0;
        poly[pn++] = v[0]; poly[pn++] = v[1]; poly[pn++] = v[2];
        VSOut outp[6]; int on = 0;
        for (int i = 0; i < pn; i++) {
            const VSOut& a = poly[i];
            const VSOut& b = poly[(i + 1) % pn];
            bool ain = a.viewDist >= NEAR, bin = b.viewDist >= NEAR;
            if (ain) { outp[on++] = a; }
            if (ain != bin) { f32 t; VSOut m = clipEdge(a, b, t); (void)t; if (on < 6) outp[on++] = m; }
        }
        for (int i = 0; i < on; i++) buf[n++] = outp[i];
        if (n < 3) return;

        // project + emit fan
        for (int i = 1; i + 1 < n; i++) {
            VSOut tri[3] = {buf[0], buf[i], buf[i + 1]};
            f32 x0 = tri[0].sx, y0 = tri[0].sy;
            f32 x1 = tri[1].sx, y1 = tri[1].sy;
            f32 x2 = tri[2].sx, y2 = tri[2].sy;
            f32 area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
            if (std::fabs(area) < 1e-6f) continue;
            if (cull && area > 0) continue;   // y-down screen flips CCW
            int minX = tmax(0, (int)std::floor(tmin(x0, tmin(x1, x2))));
            int maxX = tmin(wW - 1, (int)std::ceil(tmax(x0, tmax(x1, x2))));
            int minY = tmax(0, (int)std::floor(tmin(y0, tmin(y1, y2))));
            int maxY = tmin(wH - 1, (int)std::ceil(tmax(y0, tmax(y1, y2))));
            if (minX > maxX || minY > maxY) continue;
            f32 invArea = 1.f / area;
            // flat normal from triangle (world positions)
            Vec3 nrm = norm(cross(Vec3(tri[1].wx - tri[0].wx, tri[1].wy - tri[0].wy, tri[1].wz - tri[0].wz),
                                   Vec3(tri[2].wx - tri[0].wx, tri[2].wy - tri[0].wy, tri[2].wz - tri[0].wz)));
            int ax = 0;
            f32 aabs = std::fabs(nrm.x);
            if (std::fabs(nrm.y) > aabs) { aabs = std::fabs(nrm.y); ax = 1; }
            if (std::fabs(nrm.z) > aabs) { ax = 2; }
            int upAxis = ax;   // triplanar dominant
            const MatGpu* mg = matGpuTable();
            f32 uvScale = mg[mat].uvScale;
            bool interior = (vflags & VF_FRESH) != 0;
            int tile = matTileSlot(mat, interior);
            const TexAtlas& at = texAtlas();
            int T = at.tilePx, S = at.size();
            int tx = (tile % at.tiles) * T, ty = (tile / at.tiles) * T;

            for (int py = minY; py <= maxY; py++) {
                for (int px = minX; px <= maxX; px++) {
                    f32 cxp = px + 0.5f, cyp = py + 0.5f;
                    f32 w0 = ((x1 - cxp) * (y2 - cyp) - (y1 - cyp) * (x2 - cxp)) * invArea;
                    f32 w1 = ((x2 - cxp) * (y0 - cyp) - (y2 - cyp) * (x0 - cxp)) * invArea;
                    f32 w2 = 1.f - w0 - w1;
                    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                    // perspective-correct depth + attrs
                    f32 iw = w0 * tri[0].invW + w1 * tri[1].invW + w2 * tri[2].invW;
                    if (iw <= 1e-9f) continue;
                    f32 ndcz = (w0 * tri[0].ndcz * tri[0].invW + w1 * tri[1].ndcz * tri[1].invW + w2 * tri[2].ndcz * tri[2].invW) / iw;
                    if (ndcz < -1.f || ndcz > 1.f) continue;
                    f32 zbuf = ndcz * 0.5f + 0.5f;
                    size_t di = (size_t)py * wW + px;
                    if (zbuf >= zb[di]) continue;
                    f32 wx = (w0 * tri[0].wx * tri[0].invW + w1 * tri[1].wx * tri[1].invW + w2 * tri[2].wx * tri[2].invW) / iw;
                    f32 wy = (w0 * tri[0].wy * tri[0].invW + w1 * tri[1].wy * tri[1].invW + w2 * tri[2].wy * tri[2].invW) / iw;
                    f32 wz = (w0 * tri[0].wz * tri[0].invW + w1 * tri[1].wz * tri[1].invW + w2 * tri[2].wz * tri[2].invW) / iw;
                    Vec3 lit(
                        (w0 * tri[0].lit.x * tri[0].invW + w1 * tri[1].lit.x * tri[1].invW + w2 * tri[2].lit.x * tri[2].invW) / iw,
                        (w0 * tri[0].lit.y * tri[0].invW + w1 * tri[1].lit.y * tri[1].invW + w2 * tri[2].lit.y * tri[2].invW) / iw,
                        (w0 * tri[0].lit.z * tri[0].invW + w1 * tri[1].lit.z * tri[1].invW + w2 * tri[2].lit.z * tri[2].invW) / iw);
                    f32 ao = (w0 * tri[0].ao * tri[0].invW + w1 * tri[1].ao * tri[1].invW + w2 * tri[2].ao * tri[2].invW) / iw;
                    f32 vd = (w0 * tri[0].viewDist * tri[0].invW + w1 * tri[1].viewDist * tri[1].invW + w2 * tri[2].viewDist * tri[2].invW) / iw;
                    // triplanar uv from world pos
                    f32 u, v;
                    if (upAxis == 0) { u = wy; v = wz; }
                    else if (upAxis == 1) { u = wx; v = wz; }
                    else { u = wx; v = wy; }
                    u *= uvScale; v *= uvScale;
                    // mirror-wrap so procedural tiles are seamless (GL shader mirrors too)
                    f32 fu = u - std::floor(u), fv = v - std::floor(v);
                    if (((int)std::floor(u)) & 1) fu = 1.f - fu;
                    if (((int)std::floor(v)) & 1) fv = 1.f - fv;
                    int sxp = tx + (int)(fu * T), syp = ty + (int)(fv * T);
                    if (sxp >= tx + T) sxp = tx + T - 1;
                    if (syp >= ty + T) syp = ty + T - 1;
                    size_t ti = ((size_t)syp * S + sxp) * 4;
                    f32 tr = at.albedo[ti] / 255.f, tg = at.albedo[ti + 1] / 255.f, tb = at.albedo[ti + 2] / 255.f;
                    f32 fr = tr * lit.x * ao, fg = tg * lit.y * ao, fb2 = tb * lit.z * ao;
                    f32 fog = fogFactor(vd);
                    fr = lerpf(fr, sky_.fogColor.x, fog);
                    fg = lerpf(fg, sky_.fogColor.y, fog);
                    fb2 = lerpf(fb2, sky_.fogColor.z, fog);
                    // exposure + gamma
                    fr *= sky_.exposure; fg *= sky_.exposure; fb2 *= sky_.exposure;
                    fr = std::pow(tclamp(fr, 0.f, 1.f), 1.f / 2.2f);
                    fg = std::pow(tclamp(fg, 0.f, 1.f), 1.f / 2.2f);
                    fb2 = std::pow(tclamp(fb2, 0.f, 1.f), 1.f / 2.2f);
                    u8 R = (u8)(fr * 255.f), G = (u8)(fg * 255.f), B = (u8)(fb2 * 255.f);
                    u8 A = blend ? (u8)((mat == MAT_WATER ? 168 : 255) * (alphaMul / 255.f)) : alphaMul;
                    putPixel(px, py, zbuf, R, G, B, A, true, !blend);
                }
            }
        }
    }

    void drawOne(const Draw& d) {
        if (d.kind == 0) {
            if (!curWorld_) return;
            const ChunkMesh& m = curWorld_->meshes[d.chunk];
            int cx = (int)(d.chunk % (u32)curWorld_->CW);
            int cy = (int)((d.chunk / (u32)curWorld_->CW) % (u32)curWorld_->CHh);
            int cz = (int)(d.chunk / ((u32)curWorld_->CW * (u32)curWorld_->CHh));
            processVerts((const MVert*)m.verts.data(), m.idx.data(), (int)m.idx.size(),
                         Mat4::translate(Vec3(cx * CH * VOX, cy * CH * VOX, cz * CH * VOX)),
                         nullptr, false, d.flags);
        } else {
            auto it = meshes.find(d.handle);
            if (it == meshes.end()) return;
            processVerts((const MVert*)it->second.verts.data(), it->second.idx.data(), (int)it->second.idx.size(),
                         d.xform, d.tint, true, d.flags);
        }
    }

    void processVerts(const MVert* mv, const u32* idx, int ni, const Mat4& xform, const f32* tint, bool useTint, u8 flags) {
        if (!mv || !idx || ni < 3) return;
        bool blend = (flags & 1) != 0;
        bool doubleSide = (flags & 2) != 0 || blend;
        for (int t = 0; t + 2 < ni; t += 3) {
            VSOut vv[3];
            u8 mat = mv[idx[t]].mat;
            u8 vf = mv[idx[t]].flags;
            bool ok = true;
            for (int k = 0; k < 3; k++) {
                const MVert& src = mv[idx[t + k]];
                Vec3 lp(src.p[0], src.p[1], src.p[2]);
                Vec3 wp = xform.mulPoint(lp);
                Vec3 ln(src.n[0], src.n[1], src.n[2]);
                Vec3 wn = norm(xform.mulDir(ln));
                Vec3 base(src.col[0] / 255.f, src.col[1] / 255.f, src.col[2] / 255.f);
                if (useTint && tint) { base.x *= tint[0]; base.y *= tint[1]; base.z *= tint[2]; }
                f32 aoF = aoFactor(src.ao);
                Vec3 lit = shadeVertex(wp, wn, base, aoF, src.mat);
                // view transform
                Vec4 cp = viewProj_.mul(Vec4(wp, 1.f));
                if (cp.w <= 1e-5f) { ok = false; break; }
                vv[k].invW = 1.f / cp.w;
                vv[k].sx = (cp.x / cp.w * 0.5f + 0.5f) * wW;
                vv[k].sy = (1.f - (cp.y / cp.w * 0.5f + 0.5f)) * wH;
                vv[k].ndcz = cp.z / cp.w;
                vv[k].wx = wp.x; vv[k].wy = wp.y; vv[k].wz = wp.z;
                Vec3 vp = wp - cam_.pos;
                vv[k].viewDist = len(vp);
                vv[k].lit = lit;
                vv[k].ao = 1.f;   // ao folded into lit
                if (vv[k].viewDist < 0.001f) vv[k].viewDist = 0.001f;
            }
            if (!ok) continue;
            if (mat == MAT_AIR) mat = mv[idx[t]].mat;
            rasterTri(vv, mat, vf, blend, 255, !doubleSide);
            (void)doubleSide;
        }
    }

    void drawSky() {
        Vec3 fwd = cam_.fwd, up = cam_.up, right = cam_.right();
        f32 th = tanHalf_, asp = aspect_;
        Vec3 sunT = norm(-sky_.sunDir);
        for (int y = 0; y < wH; y++) {
            for (int x = 0; x < wW; x++) {
                f32 nx = ((x + 0.5f) / wW * 2.f - 1.f) * th * asp;
                f32 ny = (1.f - (y + 0.5f) / wH * 2.f) * th;
                Vec3 rd = norm(fwd + right * nx + up * ny);
                f32 t = tclamp(rd.y * 1.15f, 0.f, 1.f);
                f32 grad = std::pow(t, 0.62f);
                Vec3 col = lerp(sky_.horizon, sky_.zenith, grad);
                // sun
                f32 sd = dot(rd, sunT);
                f32 disc = smoothstepf(0.9965f, 0.9993f, sd);
                f32 glow = std::pow(tmax(0.f, sd), 36.f) * 0.35f;
                col += sky_.sunColor * (disc * 1.6f + glow) * sky_.sunIntensity;
                // clouds
                if (rd.y > 0.02f && sky_.weather != 4) {
                    f32 cu = (rd.x / rd.y) * 0.06f, cv = (rd.z / rd.y) * 0.06f;
                    f32 n = fbm2(cu + (f32)sky_.weather * 3.f, cv, 42);
                    f32 cover = tclamp((n - (sky_.weather >= 1 ? 0.35f : 0.55f)) * 3.f, 0.f, 1.f);
                    cover *= tclamp(rd.y * 4.f, 0.f, 1.f);
                    f32 dim = sky_.weather == 3 ? 0.35f : sky_.weather == 2 ? 0.5f : sky_.weather == 1 ? 0.62f : 0.85f;
                    Vec3 cloudC = lerp(sky_.horizon * 1.06f, Vec3(1.05f, 1.04f, 1.02f) * (0.4f + dim * 0.7f), n);
                    if (sky_.weather >= 3) cloudC *= 0.55f;
                    col = lerp(col, cloudC, cover * 0.9f);
                }
                // night stars handled by dark zenith (time-of-day controlled by game)
                // horizon haze: dense at horizon, clear at zenith
                f32 haze = tclamp((1.f - rd.y * 2.4f) * sky_.fogDensity * 70.f, 0.f, 0.92f);
                col = lerp(col, sky_.fogColor, haze);
                if (sky_.flash) col += Vec3(1, 0.97f, 0.9f) * (sky_.flash / 255.f * 1.5f);
                col = col.mul(Vec3(sky_.exposure, sky_.exposure, sky_.exposure));
                size_t i = ((size_t)y * wW + x) * 4;
                fb[i + 0] = (u8)(tclamp(std::pow(tclamp(col.x, 0.f, 1.f), 1.f / 2.2f), 0.f, 1.f) * 255);
                fb[i + 1] = (u8)(tclamp(std::pow(tclamp(col.y, 0.f, 1.f), 1.f / 2.2f), 0.f, 1.f) * 255);
                fb[i + 2] = (u8)(tclamp(std::pow(tclamp(col.z, 0.f, 1.f), 1.f / 2.2f), 0.f, 1.f) * 255);
                fb[i + 3] = 255;
                zb[(size_t)y * wW + x] = 1.f;
            }
        }
    }

    void drawParticlesNow() {
        // sort back-to-front
        std::vector<std::pair<f32, int>> order;
        order.reserve(parts_.size());
        for (int i = 0; i < (int)parts_.size(); i++) {
            Vec3 dp(parts_[i].x - cam_.pos.x, parts_[i].y - cam_.pos.y, parts_[i].z - cam_.pos.z);
            order.push_back({-len(dp), i});
        }
        std::sort(order.begin(), order.end());
        const SpriteSheet& sh = spriteSheet();
        f32 scrScale = wH / (2.f * tanHalf_);
        for (auto& pr : order) {
            const PInst& p = parts_[pr.second];
            Vec3 dp(p.x - cam_.pos.x, p.y - cam_.pos.y, p.z - cam_.pos.z);
            f32 zv = dot(dp, cam_.fwd);
            if (zv < 0.05f) continue;
            f32 dist = len(dp);
            f32 fog = fogFactor(dist);
            f32 sxp = wW * 0.5f + (dot(dp, cam_.right()) / (zv * tanHalf_ * aspect_)) * (wW * 0.5f);
            f32 syp = wH * 0.5f - (dot(dp, cam_.up) / (zv * tanHalf_)) * (wH * 0.5f);
            f32 sizePx = p.size * scrScale / zv;
            if (sizePx < 0.4f) continue;
            // depth occlusion at center
            size_t ci = (size_t)tclamp((int)syp, 0, wH - 1) * wW + tclamp((int)sxp, 0, wW - 1);
            Vec4 cp = viewProj_.mul(Vec4(Vec3(p.x, p.y, p.z), 1.f));
            f32 pz = (cp.z / cp.w) * 0.5f + 0.5f;
            if (pz > zb[ci] + 0.002f) continue;
            // quad
            f32 cr = std::cos(p.rot), sr = std::sin(p.rot);
            f32 hz = sizePx * 0.5f;
            f32 us = 0, vs = 0, uw = 0.25f, vw = 0.25f;
            int frame = (int)p.frame;
            if (frame < 0) frame = 0; if (frame > 15) frame = 15;
            us = (frame % 4) * 0.25f; vs = (frame / 4) * 0.25f;
            int minX = tmax(0, (int)std::floor(sxp - sizePx));
            int maxX = tmin(wW - 1, (int)std::ceil(sxp + sizePx));
            int minY = tmax(0, (int)std::floor(syp - sizePx));
            int maxY = tmin(wH - 1, (int)std::ceil(syp + sizePx));
            f32 vis = 1.f - fog;
            f32 pr2 = p.r * vis, pg = p.g * vis, pb = p.b * vis;
            if (sky_.exposure != 1.f) { pr2 *= sky_.exposure; pg *= sky_.exposure; pb *= sky_.exposure; }
            for (int y = minY; y <= maxY; y++) for (int x = minX; x <= maxX; x++) {
                f32 lx = x + 0.5f - sxp, ly = y + 0.5f - syp;
                f32 rx = lx * cr + ly * sr, ry = -lx * sr + ly * cr;
                if (std::fabs(rx) > hz || std::fabs(ry) > hz) continue;
                f32 fu = (rx / hz * 0.5f + 0.5f), fv = (ry / hz * 0.5f + 0.5f);
                int tx = (int)((us + fu * uw) * sh.w), ty = (int)((vs + fv * vw) * sh.h);
                if (tx < 0 || ty < 0 || tx >= sh.w || ty >= sh.h) continue;
                size_t ti = ((size_t)ty * sh.w + tx) * 4;
                f32 a = sh.rgba[ti + 3] / 255.f * p.a;
                if (a <= 0.003f) continue;
                size_t di = (size_t)y * wW + x;
                if (pz >= zb[di] && p.kind != 2) continue;  // sparks ignore depth slightly
                f32 R = (sh.rgba[ti] / 255.f) * pr2;
                f32 G = (sh.rgba[ti + 1] / 255.f) * pg;
                f32 B = (sh.rgba[ti + 2] / 255.f) * pb;
                R = std::pow(tclamp(R, 0.f, 1.f), 1.f / 2.2f);
                G = std::pow(tclamp(G, 0.f, 1.f), 1.f / 2.2f);
                B = std::pow(tclamp(B, 0.f, 1.f), 1.f / 2.2f);
                putPixel(x, y, pz, (u8)(R * 255), (u8)(G * 255), (u8)(B * 255), (u8)(a * 255), true, false);
            }
        }
    }

    void drawUIQuad(const UIQuad& q) {
        if (q.w <= 0 || q.h <= 0) return;
        int x0 = tmax(0, (int)std::floor(q.x)), y0 = tmax(0, (int)std::floor(q.y));
        int x1 = tmin(wW, (int)std::ceil(q.x + q.w)), y1 = tmin(wH, (int)std::ceil(q.y + q.h));
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
            f32 u = (x + 0.5f - q.x) / q.w;
            f32 v = (y + 0.5f - q.y) / q.h;
            f32 a = q.a / 255.f;
            if (q.tex == 1) {
                // q.u0..v1 hold atlas-normalized glyph bounds
                f32 au = q.u0 + u * (q.u1 - q.u0);
                f32 av2 = q.v0 + v * (q.v1 - q.v0);
                f32 fx = tclamp(au * FT_FONT_W, 0.f, (f32)FT_FONT_W - 1.001f);
                f32 fy = tclamp(av2 * FT_FONT_H, 0.f, (f32)FT_FONT_H - 1.001f);
                int ix = (int)fx, iy = (int)fy;
                f32 tx = fx - ix, ty = fy - iy;
                int ix1 = tmin(ix + 1, FT_FONT_W - 1), iy1 = tmin(iy + 1, FT_FONT_H - 1);
                f32 a00 = FT_FONT_DATA[(size_t)iy * FT_FONT_W + ix];
                f32 a10 = FT_FONT_DATA[(size_t)iy * FT_FONT_W + ix1];
                f32 a01 = FT_FONT_DATA[(size_t)iy1 * FT_FONT_W + ix];
                f32 a11 = FT_FONT_DATA[(size_t)iy1 * FT_FONT_W + ix1];
                f32 av = lerpf(lerpf(a00, a10, tx), lerpf(a01, a11, tx), ty) / 255.f;
                a *= av;
            }
            if (a <= 0.004f) continue;
            putPixel(x, y, 0, q.r, q.g, q.b, (u8)(a * 255.f), false, false);
        }
    }
};

} // namespace

IBackend* createSWBackend() { return new SWBackend(); }

} // namespace ft
