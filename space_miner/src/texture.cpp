// ============================================================
// SPACE MINER  -  texture.cpp
// ============================================================
#include "texture.h"

namespace sm {
namespace tex {

static float segDist(Vec2 p, Vec2 a, Vec2 b, Vec2* closest = nullptr) {
    Vec2 d = b - a;
    float l2 = lengthSq(d);
    float t = l2 > 1e-9f ? clampf(dot(p - a, d) / l2, 0.f, 1.f) : 0.f;
    Vec2 q = a + d * t;
    if (closest) *closest = q;
    return length(p - q);
}

void rasterizeMask(const PolySet& polys, int w, int h, std::vector<float>& mask, int ss) {
    mask.assign((size_t)w * h, 0.f);
    if (ss < 1) ss = 1;
    int sw = w * ss, sh = h * ss;
    std::vector<uint8_t> grid((size_t)sw * sh, 0);
    std::vector<float> xs;
    for (const auto& poly : polys) {
        if (poly.size() < 3) continue;
        float miny = poly[0].y, maxy = poly[0].y;
        for (auto& p : poly) { miny = std::min(miny, p.y); maxy = std::max(maxy, p.y); }
        int y0 = std::max(0, (int)floorf(miny * ss)), y1 = std::min(sh - 1, (int)ceilf(maxy * ss));
        for (int y = y0; y <= y1; y++) {
            float sy = (y + .5f) / ss;
            xs.clear();
            for (size_t i = 0; i < poly.size(); i++) {
                Vec2 a = poly[i], b = poly[(i + 1) % poly.size()];
                if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) {
                    float t = (sy - a.y) / (b.y - a.y);
                    xs.push_back(a.x + (b.x - a.x) * t);
                }
            }
            std::sort(xs.begin(), xs.end());
            for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                int xa = std::max(0, (int)floorf(xs[i] * ss));
                int xb = std::min(sw - 1, (int)ceilf(xs[i + 1] * ss) - 1);
                for (int x = xa; x <= xb; x++) grid[(size_t)y * sw + x] = 1;
            }
        }
    }
    float inv = 1.f / (ss * ss);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int cnt = 0;
            for (int j = 0; j < ss; j++)
                for (int i = 0; i < ss; i++)
                    cnt += grid[(size_t)(y * ss + j) * sw + (x * ss + i)];
            mask[(size_t)y * w + x] = cnt * inv;
        }
}

void distanceField(const PolySet& polys, int w, int h, const std::vector<float>& mask,
                   std::vector<float>& dist, std::vector<Vec2>& dir) {
    dist.assign((size_t)w * h, 9999.f);
    dir.assign((size_t)w * h, Vec2(0, 0));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            if (mask[idx] <= 0.004f) continue;
            // search only pixels near edges (cheap reject by coarse test later)
            Vec2 p(x + .5f, y + .5f);
            float best = 9999.f;
            Vec2 bestQ;
            for (const auto& poly : polys) {
                for (size_t i = 0; i < poly.size(); i++) {
                    Vec2 q;
                    float d = segDist(p, poly[i], poly[(i + 1) % poly.size()], &q);
                    if (d < best) { best = d; bestQ = q; }
                }
            }
            dist[idx] = best;
            dir[idx] = normalize(p - bestQ);
        }
}

// ---------------------------------------------------------------- hull
static void greebles(Canvas& out, const std::vector<float>& mask, int w, int h, const HullStyle& st, Rng& rng) {
    int placed = 0, guard = 0;
    while (placed < st.greebleCount && guard++ < st.greebleCount * 12) {
        int x = rng.irange(2, w - 2), y = rng.irange(2, h - 2);
        if (mask[(size_t)y * w + x] < .95f) continue;
        // keep away from border
        bool nearBorder = false;
        for (int j = -1; j <= 1 && !nearBorder; j++)
            for (int i = -1; i <= 1; i++)
                if (mask[(size_t)(y + j) * w + (x + i)] < .9f) { nearBorder = true; break; }
        if (nearBorder) continue;
        int gw = rng.irange(3, std::max(4, (int)(st.panelSize * .7f)));
        int gh = rng.irange(2, std::max(3, (int)(st.panelSize * .45f)));
        Color col = rng.chance(.5f) ? shade(st.base, rng.range(.55f, .7f))
                                    : shade(st.base, rng.range(1.12f, 1.3f));
        out.fillRect(x, y, gw, gh, withAlpha(col, 130));
        if (rng.chance(.35f)) {
            out.fillRect(x, y, gw, 1, withAlpha(shade(st.base, 1.5f), 90));
            out.fillRect(x, y + gh - 1, gw, 1, withAlpha(st.dark, 120));
        }
        if (rng.chance(.14f)) {
            Color em = rng.chance(.6f) ? st.accent : rgba(255, 170, 70);
            out.fillRect(x + 1, y + gh / 2, std::max(1, gw - 2), 1, withAlpha(em, 200));
        }
        placed++;
    }
}

void paintHull(Canvas& out, const PolySet& polys, const HullStyle& st) {
    int w = out.w, h = out.h;
    std::vector<float> mask, dist;
    std::vector<Vec2> dir;
    rasterizeMask(polys, w, h, mask, 3);
    distanceField(polys, w, h, mask, dist, dir);
    Rng rng;
    rng.seed(st.seed * 7919 + 13);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            float cover = mask[idx];
            if (cover <= 0.004f) continue;
            float d = dist[idx];
            Vec2 nrm = dir[idx];      // points from the closest edge towards the pixel
            float n = fbm2(x * st.noiseScale, y * st.noiseScale, 4, 2.1f, .5f, st.seed);
            float fine = valueNoise2(x * .13f, y * .13f, st.seed + 5) * .6f +
                         valueNoise2(x * .42f, y * .42f, st.seed + 6) * .4f;
            float blotch = fbm2(x * .018f, y * .018f, 3, 2.f, .5f, st.seed + 8);
            float plate = 1.f;
            float pxs = st.panelSize, pys = st.panelSize * 1.31f;
            float lx = fmodf(x + n * 4.f, pxs), ly = fmodf(y + n * 4.f, pys);
            if (lx < 1.15f) plate *= .78f;
            if (ly < 1.15f) plate *= .84f;
            if (lx < .5f || ly < .5f) plate *= 1.12f;

            float t = saturate(1.f - d / st.bevel);       // 1 at rim
            float lit = dot(nrm, st.lightDir);
            float bevelL = 1.f + .55f * lit * t;
            float ao = 1.f - st.ao * expf(-d / 4.f);
            float base = .46f + .54f * n;
            base *= (0.95f + 0.10f * fine) * (0.88f + 0.26f * blotch);
            Color c = mixColor(st.dark, st.base, saturate(base));
            c = scaleColor(c, plate * (0.96f + .08f * fine));
            c = scaleColor(c, bevelL * ao);
            float rim = powf(saturate(lit * .5f + .5f), 2.2f) * t * st.rim;
            c = addColor(withAlpha(c, 255), rgbaf(rim * .55f, rim * .62f, rim * .75f, 255));
            // specular sweep
            float spec = powf(saturate(lit), 6.f) * t * t;
            c = addColor(c, rgbaf(spec * .5f, spec * .55f, spec * .6f, 255));

            Color prev = out.get(x, y);
            Color dst = mixColor(prev, c, cover);
            dst = withAlpha(dst, 255);
            out.put(x, y, dst);
        }

    // greebles on top of the plating
    greebles(out, mask, w, h, st, rng);

    // dark contour for readability against space
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            float cover = mask[idx];
            if (cover <= .05f) continue;
            float d = dist[idx];
            float t = saturate(1.f - d / 2.6f);
            if (t > .02f) {
                float lit = dot(dir[idx], st.lightDir);
                Color outline = lit > .3f ? shade(st.base, 1.45f) : scaleColor(st.dark, .45f);
                out.blend(x, y, withAlpha(outline, (int)(t * 150 * cover)));
            }
        }
}

void paintBlob(Canvas& out, const PolySet& polys, const HullStyle& st, float blobSeed) {
    paintHull(out, polys, st);
    (void)blobSeed;
}

// ---------------------------------------------------------------- rock
// Faceted asteroid: angular silhouette, per-facet plane shading, crease
// darkening along the radial fractures, craters and glowing mineral veins.
void paintRock(Canvas& out, Vec2 centre, float radius, const RockStyle& st) {
    int w = out.w, h = out.h;
    Rng rng;
    rng.seed(st.seed * 2654435761u + 7);

    int nv = rng.irange(9, 15);
    std::vector<Vec2> verts(nv);
    std::vector<float> facetShade(nv), facetHeight(nv);
    for (int i = 0; i < nv; i++) {
        float a = i * TAU / nv + rng.range(-.22f, .22f) * (TAU / nv);
        float rr = radius * rng.range(.62f, 1.0f);
        if (rng.chance(.16f)) rr *= .72f;                    // deep notch / fracture
        if (st.spikes && rng.chance(.3f)) rr *= 1.22f;       // crystalline spikes
        verts[i] = centre + Vec2(cosf(a), sinf(a)) * rr;
        facetShade[i] = rng.range(-.3f, .22f);
        facetHeight[i] = rng.range(-.18f, .18f);
    }

    PolySet set;
    set.push_back(verts);
    std::vector<float> mask, dist;
    std::vector<Vec2> dir;
    rasterizeMask(set, w, h, mask, 3);
    distanceField(set, w, h, mask, dist, dir);

    auto facetOf = [&](float x, float y) {
        float ang = atan2f(y - centre.y, x - centre.x);
        if (ang < 0) ang += TAU;
        int best = 0;
        float bestD = 1e9f;
        for (int i = 0; i < nv; i++) {
            Vec2 v = verts[i] - centre;
            float va = atan2f(v.y, v.x);
            if (va < 0) va += TAU;
            float d = fabsf(wrapAngle(va - ang));
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    };

    auto heightAt = [&](float px, float py) -> float {
        float s = st.noiseScale;
        float rr = length(Vec2(px, py) - centre) / std::max(1.f, radius);
        float dome = (1.f - saturate(rr * rr)) * .55f;
        int f = facetOf(px, py);
        float base = fbm2(px * s, py * s, 4, 2.13f, .5f, st.seed);
        float cr = 0.f;
        if (st.craterAmount > 0.f) {
            float wv = worley2(px * s * 1.5f + 10.f, py * s * 1.5f + 4.f, st.seed + 31);
            if (wv < .40f) cr = -(1.f - wv / .40f) * .55f * st.craterAmount;
            float bowl = worley2(px * s * 4.1f, py * s * 4.1f, st.seed + 77);
            if (bowl < .28f) cr += -(1.f - bowl / .28f) * .2f * st.craterAmount;
        }
        return dome + facetHeight[f] + base * .3f + cr;
    };

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            float cover = mask[idx];
            if (cover <= .004f) continue;
            float d = dist[idx];
            Vec2 nrm = dir[idx];
            float px = (float)x, py = (float)y;
            int fi = facetOf(px, py);
            // outward normal of this facet's plane
            Vec2 e0 = verts[fi] - centre, e1 = verts[(fi + 1) % nv] - centre;
            Vec2 edgeMid = normalize(e0 + e1);
            float facetNormalDot = dot(edgeMid, st.lightDir);
            // radial spokes bound the facet: crease darkening
            Vec2 p = Vec2(px, py) - centre;
            auto spokeDist = [&](Vec2 v) {
                Vec2 u = normalize(v);
                float proj = dot(p, u);
                if (proj < 0) return 999.f;
                return fabsf(cross(u, p));
            };
            float sp = std::min(spokeDist(e0), spokeDist(e1));
            float radial = saturate(length(p) / std::max(1.f, radius));
            float crease = saturate(1.f - sp / (radius * .07f)) *
                           saturate((radial - .25f) / .35f);

            float hgt = heightAt(px, py);
            float hgtX = heightAt(px + 1.6f, py), hgtY = heightAt(px, py + 1.6f);
            Vec2 grad((hgtX - hgt) * 22.f, (hgtY - hgt) * 22.f);
            Vec2 n2 = normalize(Vec2(-grad.x, -grad.y));
            float slope = saturate(length(grad) * 1.4f);
            float lam = saturate(.5f + .5f * (facetNormalDot + dot(n2, st.lightDir) * .55f + facetShade[fi]));
            float litF = saturate(.18f + 1.0f * (lam - .25f));
            Color c = mixColor(st.rockDark, st.rockLight, saturate(litF));
            c = scaleColor(c, (0.88f + 0.24f * (1.f - slope)) * (1.f - crease * .45f));
            // micro grain
            float grain = valueNoise2(px * .62f, py * .62f, st.seed + 5);
            float grain2 = valueNoise2(px * .17f, py * .17f, st.seed + 19);
            c = scaleColor(c, .84f + .2f * grain + .1f * grain2);
            // crater bowls
            float cav = saturate((hgt - .28f) * 2.4f);
            c = scaleColor(c, .84f + .26f * cav);
            // veins
            if (st.veinAmount > 0.f) {
                float vn = ridged2(px * st.noiseScale * 1.55f + 3.f, py * st.noiseScale * 1.55f, 3, st.seed + 41);
                float vmask = saturate((vn - .60f) * 8.f) * st.veinAmount * (0.45f + 0.55f * saturate(1.f - crease));
                if (vmask > .01f) {
                    c = mixColor(c, st.veinColor, saturate(vmask * 1.1f));
                    c = addColor(c, scaleColor(st.veinColor, vmask * .45f));
                }
            }
            // rim light + dark edge
            float edge = saturate(d / (radius * .07f));
            float rim = powf(saturate(1.f - edge), 2.2f) * saturate(facetNormalDot * .5f + .5f);
            c = addColor(c, scaleColor(st.rimColor, rim * st.rim * .5f));
            c = scaleColor(c, 1.f - powf(saturate(1.f - edge), 3.f) * saturate(1.f - facetNormalDot) * .55f);
            // crease crack lines
            if (crease > .55f) c = scaleColor(c, .55f + .45f * (1.f - crease));

            Color dst = mixColor(out.get(x, y), c, cover);
            out.put(x, y, withAlpha(dst, 255));
        }

    // contour
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            if (mask[idx] <= .05f) continue;
            float t = saturate(1.f - dist[idx] / 1.7f);
            if (t > .02f) out.blend(x, y, withAlpha(rgba(5, 5, 9), (int)(t * 135)));
        }
}

// Crystal cluster: overlapping prisms with translucent inner glow.
void paintCrystalCluster(Canvas& out, Vec2 centre, float radius, Color base, Color glow, uint32_t seed) {
    Rng rng;
    rng.seed(seed * 31 + 3);
    int cnt = rng.irange(4, 7);
    for (int i = 0; i < cnt; i++) {
        float ang = rng.range(0, TAU);
        float off = rng.range(0.f, radius * .45f);
        float len = radius * rng.range(.45f, 1.0f);
        float wid = radius * rng.range(.10f, .22f);
        Vec2 basep = centre + Vec2(cosf(ang), sinf(ang)) * off;
        Vec2 dirv = normalize(Vec2(cosf(ang * 1.7f), sinf(ang * 1.7f)));
        Vec2 tip = basep + dirv * len;
        Vec2 side = perp(dirv) * wid;
        Vec2 mid1 = basep + dirv * len * .28f;
        Poly tree{basep + side * .55f, mid1 + side, mid1 + side * .55f, tip, mid1 - side * .55f, mid1 - side,
                  basep - side * .55f};
        PolySet ps{tree};
        std::vector<float> mask, dist;
        std::vector<Vec2> dir;
        rasterizeMask(ps, out.w, out.h, mask, 3);
        distanceField(ps, out.w, out.h, mask, dist, dir);
        for (int y = 0; y < out.h; y++)
            for (int x = 0; x < out.w; x++) {
                size_t idx = (size_t)y * out.w + x;
                float cover = mask[idx];
                if (cover <= .004f) continue;
                float t = saturate(1.f - dist[idx] / (wid * .9f));
                Vec2 nrm = dir[idx];
                float lit = saturate(dot(nrm, Vec2(-.6f, -.8f)) * .5f + .5f);
                Color c = mixColor(scaleColor(base, .45f), base, lit);
                c = addColor(c, scaleColor(glow, (.35f + .5f * (1.f - t)) * .8f));
                // facet stripe
                float stripe = saturate(1.f - fabsf(fmodf((x + y) * .14f, 1.f) - .5f) * 2.4f);
                c = addColor(c, scaleColor(glow, stripe * .18f * t));
                // bright core
                if (t < .3f) c = addColor(c, scaleColor(rgba(255, 255, 255), (1.f - t / .3f) * .5f));
                Color dst = mixColor(out.get(x, y), c, cover);
                out.put(x, y, withAlpha(dst, 255));
            }
    }
}

// ---------------------------------------------------------------- glows / sprites
void makeGlow(Canvas& out, int size, float power, Color tint) {
    out.resize(size, size);
    float c = (size - 1) * .5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = (x - c) / c, dy = (y - c) / c;
            float d = sqrtf(dx * dx + dy * dy);
            float a = d >= 1.f ? 0.f : powf(1.f - d, power);
            out.px[(size_t)y * size + x] = rgbaf(cr(tint) / 255.f, cg(tint) / 255.f, cb(tint) / 255.f,
                                                 a * (ca(tint) / 255.f));
        }
}

void makeSmoke(Canvas& out, int size, uint32_t seed) {
    out.resize(size, size);
    float c = (size - 1) * .5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = (x - c) / c, dy = (y - c) / c;
            float d = sqrtf(dx * dx + dy * dy);
            float n = fbm2(x * .06f, y * .06f, 3, 2.f, .5f, seed);
            float a = saturate(1.f - d) * (.35f + .8f * n);
            a = saturate(a * 1.4f - .18f);
            out.px[(size_t)y * size + x] = rgbaf(.75f + .2f * n, .78f + .2f * n, .85f, a * .8f);
        }
}

void makeSpark(Canvas& out, int w, int h, Color head) {
    out.resize(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float fx = x / (float)(w - 1), fy = (y - (h - 1) * .5f) / (h * .5f);
            float a = (1.f - fabsf(fy)) * powf(1.f - fx, .8f);
            if (a <= 0) continue;
            Color c = mixColor(head, rgba(255, 255, 255), powf(1.f - fx, 3.f));
            out.px[(size_t)y * w + x] = withAlpha(c, (int)(saturate(a) * 255));
        }
}

void paintNebula(Canvas& out, uint32_t seed, Color c1, Color c2, float density, float scale) {
    int w = out.w, h = out.h;
    Rng rng;
    rng.seed(seed * 977 + 5);
    // a few cloud seeds
    struct Puff { Vec2 p; float r; float w; };
    std::vector<Puff> puffs;
    int pc = 9;
    for (int i = 0; i < pc; i++) {
        Puff p;
        p.p = Vec2(rng.range(-.1f, 1.1f) * w, rng.range(-.1f, 1.1f) * h);
        p.r = rng.range(.18f, .55f) * std::max(w, h);
        p.w = rng.range(.4f, 1.f);
        puffs.push_back(p);
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float acc = 0.f;
            for (auto& p : puffs) {
                float d = length(Vec2((float)x, (float)y) - p.p) / p.r;
                if (d < 1.f) acc += p.w * powf(1.f - d, 2.2f);
            }
            float warp = fbm2(x * .0045f * scale + 11.f, y * .0045f * scale + 3.f, 3, 2.f, .5f, seed) - .5f;
            float n = fbm2(x * .012f * scale + warp * 2.f, y * .012f * scale + warp * 2.f, 5, 2.2f, .55f, seed + 1);
            float alpha = saturate((acc * .8f + n * .75f - .55f) * 2.6f) * density;
            if (alpha <= .002f) continue;
            float t = saturate(n * 1.2f - .1f + warp * .5f);
            Color c = mixColor(c1, c2, t);
            float bright = .55f + .7f * fbm2(x * .03f, y * .03f, 2, 2.f, .5f, seed + 9);
            c = scaleColor(c, bright);
            out.px[(size_t)y * w + x] = withAlpha(c, (int)(alpha * 255));
        }
    blur(out, 2);
}

void paintStarfield(Canvas& out, uint32_t seed, int count, Color tint) {
    // Sparse, fine dust plus a handful of brighter stars. Bright stars are rare
    // on purpose: a sky full of identical flares reads as snow, not as space.
    static const Color palette[6] = {
        rgba(198, 220, 255), rgba(255, 255, 255), rgba(255, 244, 224),
        rgba(255, 222, 180), rgba(198, 246, 255), rgba(226, 214, 255)};
    Rng rng;
    rng.seed(seed + 31337);
    for (int i = 0; i < count; i++) {
        float x = rng.range(0, (float)out.w), y = rng.range(0, (float)out.h);
        float b = powf(rng.f(), 3.1f);
        Color base = palette[rng.irange(0, 5)];
        Color c = mixColor(tint, base, .45f + .55f * b);
        int br = (int)(70 + 180 * b);
        out.blend((int)x, (int)y, withAlpha(scaleColor(c, br / 255.f * 1.25f), (int)(80 + 150 * b)));
        if (b > .86f) {
            float r = .9f + 1.3f * b;
            out.radialGlow(x, y, r * 3.4f, withAlpha(c, 34), 1.f);
            out.circleFill(x, y, r, withAlpha(base, 225));
            float len = 2.2f + 5.f * b;
            out.line({x - len, y}, {x + len, y}, withAlpha(c, 40), .7f);
            out.line({x, y - len}, {x, y + len}, withAlpha(c, 40), .7f);
        }
        if (b > .9965f) {
            // the very few hero stars keep the wide diffraction flares
            float r = 1.5f;
            out.radialGlow(x, y, r * 9.f, withAlpha(c, 26), 1.f);
            out.radialGlow(x, y, r * 3.2f, withAlpha(base, 60), 1.f);
            out.circleFill(x, y, r, rgba(255, 255, 255, 240));
            float len = 16.f;
            out.line({x - len, y}, {x + len, y}, withAlpha(c, 52), .8f);
            out.line({x, y - len}, {x, y + len}, withAlpha(c, 52), .8f);
        }
    }
}

void blur(Canvas& c, int radius) {
    if (radius <= 0) return;
    int w = c.w, h = c.h;
    std::vector<Color> tmp(c.px.size());
    for (int pass = 0; pass < 3; pass++) {
        // horizontal
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int r = 0, g = 0, b = 0, a = 0, n = 0;
                for (int i = -radius; i <= radius; i++) {
                    int xx = x + i;
                    if (xx < 0 || xx >= w) continue;
                    Color s = c.px[(size_t)y * w + xx];
                    r += cr(s); g += cg(s); b += cb(s); a += ca(s); n++;
                }
                tmp[(size_t)y * w + x] = rgba(r / n, g / n, b / n, a / n);
            }
        }
        // vertical
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int r = 0, g = 0, b = 0, a = 0, n = 0;
                for (int i = -radius; i <= radius; i++) {
                    int yy = y + i;
                    if (yy < 0 || yy >= h) continue;
                    Color s = tmp[(size_t)yy * w + x];
                    r += cr(s); g += cg(s); b += cb(s); a += ca(s); n++;
                }
                c.px[(size_t)y * w + x] = rgba(r / n, g / n, b / n, a / n);
            }
        }
    }
}

void lensFlare(Canvas& cv, float x, float y, float size, Color col) {
    cv.radialGlow(x, y, size * .22f, withAlpha(rgba(255, 255, 255), 210), 1.2f);
    cv.radialGlow(x, y, size, withAlpha(col, 120), 1.f);
    cv.circleFill(x, y, size * .045f, rgba(255, 255, 255, 235));
    // streaks
    cv.line({x - size * 1.6f, y}, {x + size * 1.6f, y}, withAlpha(col, 70), 1.2f);
    cv.line({x, y - size * .9f}, {x, y + size * .9f}, withAlpha(col, 45), 1.f);
    // ghosts
    for (int i = 1; i <= 3; i++) {
        float t = i * .45f;
        float gx = x - (x - cv.w * .5f) * t, gy = y - (y - cv.h * .5f) * t;
        cv.ring(gx, gy, size * (.08f + i * .05f), 1.2f, withAlpha(col, 34));
    }
}

}  // namespace tex
}  // namespace sm
