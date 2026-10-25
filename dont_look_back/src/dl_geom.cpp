// ============================================================================
//  dl_geom.cpp : procedural architecture primitives + furniture library
// ============================================================================
#include "dl_world.h"

namespace dl {
using nf::V3;
using nf::Box3;
using nf::Vertex;
using nf::M4;
using nf::Color;

// ------------------------------------------------------------------ local frames
V3 GeoBuilder::toWorld(const V3& p) const {
    float c = std::cos(yaw), s = std::sin(yaw);
    return V3(origin.x + p.x * c - p.y * s, origin.y + p.x * s + p.y * c, origin.z + p.z);
}
V3 GeoBuilder::rotDir(const V3& d) const {
    float c = std::cos(yaw), s = std::sin(yaw);
    return V3(d.x * c - d.y * s, d.x * s + d.y * c, d.z);
}

// ------------------------------------------------------------------ quad / box
void GeoBuilder::quad(const V3& a, const V3& b, const V3& c, const V3& d, const V3& n,
                      float u0, float v0, float u1, float v1,
                      float aoA, float aoB, float aoC, float aoD) {
    auto ao = [&](float v) { return v < 0 ? aoDefault : v; };
    Vertex va, vb, vc, vd;
    va.p = a; vb.p = b; vc.p = c; vd.p = d;
    V3 nn = flipNormals ? -n : n;
    va.n = vb.n = vc.n = vd.n = nn;
    va.u = u0; va.v = v0;
    vb.u = u1; vb.v = v0;
    vc.u = u1; vc.v = v1;
    vd.u = u0; vd.v = v1;
    va.baked = V3(ao(aoA), 0, 0);
    vb.baked = V3(ao(aoB), 0, 0);
    vc.baked = V3(ao(aoC), 0, 0);
    vd.baked = V3(ao(aoD), 0, 0);
    va.col = vb.col = vc.col = vd.col = 0xFFFFFFFF;
    if (flipNormals) mesh.addQuad(va, vd, vc, vb);
    else mesh.addQuad(va, vb, vc, vd);
}

static void faceUV(const V3& p, int axis, float scale, float& u, float& v) {
    int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
    u = p[a1] / scale;
    v = p[a2] / scale;
}

void GeoBuilder::box(const Box3& b, bool floorTop, bool ceilBottom) {
    V3 lo = b.lo, hi = b.hi;
    const V3& p000 = lo;
    const float sx = hi.x - lo.x, sy = hi.y - lo.y, sz = hi.z - lo.z;
    float s = uvScale;
    (void)sx; (void)sy; (void)sz;
    // +X
    {
        V3 n(1, 0, 0);
        V3 a(hi.x, lo.y, lo.z), bb(hi.x, hi.y, lo.z), c(hi.x, hi.y, hi.z), d(hi.x, lo.y, hi.z);
        quad(a, bb, c, d, n, a.y / s, a.z / s, c.y / s, c.z / s);
    }
    // -X
    {
        V3 n(-1, 0, 0);
        V3 a(lo.x, lo.y, lo.z), bb(lo.x, hi.y, lo.z), c(lo.x, hi.y, hi.z), d(lo.x, lo.y, hi.z);
        quad(a, bb, c, d, n, a.y / s, a.z / s, c.y / s, c.z / s);
    }
    if (floorTop) {
        V3 n(0, 0, 1);
        V3 a(lo.x, lo.y, hi.z), bb(hi.x, lo.y, hi.z), c(hi.x, hi.y, hi.z), d(lo.x, hi.y, hi.z);
        quad(a, bb, c, d, n, a.x / s, a.y / s, c.x / s, c.y / s);
    }
    if (ceilBottom) {
        V3 n(0, 0, -1);
        V3 a(lo.x, lo.y, lo.z), bb(hi.x, lo.y, lo.z), c(hi.x, hi.y, lo.z), d(lo.x, hi.y, lo.z);
        quad(a, bb, c, d, n, a.x / s, a.y / s, c.x / s, c.y / s);
    }
    // +Y
    {
        V3 n(0, 1, 0);
        V3 a(lo.x, hi.y, lo.z), bb(hi.x, hi.y, lo.z), c(hi.x, hi.y, hi.z), d(lo.x, hi.y, hi.z);
        quad(a, bb, c, d, n, a.x / s, a.z / s, c.x / s, c.z / s);
    }
    // -Y
    {
        V3 n(0, -1, 0);
        V3 a(lo.x, lo.y, lo.z), bb(hi.x, lo.y, lo.z), c(hi.x, lo.y, hi.z), d(lo.x, lo.y, hi.z);
        quad(a, bb, c, d, n, a.x / s, a.z / s, c.x / s, c.z / s);
    }
    (void)p000;
}

void GeoBuilder::boxSimple(const Box3& b, float uv) {
    float old = uvScale;
    uvScale = uv;
    box(b, true, true);
    uvScale = old;
}

void GeoBuilder::cylinder(const V3& base, float r, float h, int seg, bool capTop, bool capBottom) {
    for (int i = 0; i < seg; ++i) {
        float a0 = nf::TAU * i / seg, a1 = nf::TAU * (i + 1) / seg;
        V3 p0(base.x + std::cos(a0) * r, base.y + std::sin(a0) * r, base.z);
        V3 p1(base.x + std::cos(a1) * r, base.y + std::sin(a1) * r, base.z);
        V3 n0(std::cos(a0), std::sin(a0), 0), n1(std::cos(a1), std::sin(a1), 0);
        Vertex va, vb, vc, vd;
        va.p = p0; va.n = n0; va.u = (float)i / seg; va.v = 0; va.baked = V3(aoDefault, 0, 0);
        vb.p = p1; vb.n = n1; vb.u = (float)(i + 1) / seg; vb.v = 0; vb.baked = V3(aoDefault, 0, 0);
        vc.p = V3(p1.x, p1.y, base.z + h); vc.n = n1; vc.u = (float)(i + 1) / seg; vc.v = h / uvScale;
        vc.baked = V3(aoDefault, 0, 0);
        vd.p = V3(p0.x, p0.y, base.z + h); vd.n = n0; vd.u = (float)i / seg; vd.v = h / uvScale;
        vd.baked = V3(aoDefault, 0, 0);
        va.col = vb.col = vc.col = vd.col = 0xFFFFFFFF;
        mesh.addQuad(va, vb, vc, vd);
        if (capTop) {
            Vertex ca, cb, cc;
            ca.p = V3(base.x, base.y, base.z + h); ca.n = V3(0, 0, 1); ca.u = 0.5f; ca.v = 0.5f;
            cb.p = V3(p1.x, p1.y, base.z + h); cb.n = V3(0, 0, 1);
            cb.u = 0.5f + std::cos(a1) * 0.5f; cb.v = 0.5f + std::sin(a1) * 0.5f;
            cc.p = V3(p0.x, p0.y, base.z + h); cc.n = V3(0, 0, 1);
            cc.u = 0.5f + std::cos(a0) * 0.5f; cc.v = 0.5f + std::sin(a0) * 0.5f;
            ca.baked = cb.baked = cc.baked = V3(aoDefault, 0, 0);
            ca.col = cb.col = cc.col = 0xFFFFFFFF;
            mesh.addTri(ca, cb, cc);
        }
        if (capBottom) {
            Vertex ca, cb, cc;
            ca.p = base; ca.n = V3(0, 0, -1);
            cb.p = V3(p0.x, p0.y, base.z); cb.n = V3(0, 0, -1);
            cc.p = V3(p1.x, p1.y, base.z); cc.n = V3(0, 0, -1);
            ca.baked = cb.baked = cc.baked = V3(aoDefault, 0, 0);
            ca.col = cb.col = cc.col = 0xFFFFFFFF;
            mesh.addTri(ca, cb, cc);
        }
    }
}

void GeoBuilder::cylinderAxis(const V3& a, const V3& b, float r, int seg) {
    V3 d = b - a;
    float len = nf::length(d);
    if (len < 1e-4f) return;
    V3 f = d / len;
    V3 up = std::fabs(f.z) > 0.9f ? V3(1, 0, 0) : V3(0, 0, 1);
    V3 rt = nf::normalize(nf::cross(f, up));
    V3 u2 = nf::cross(rt, f);
    for (int i = 0; i < seg; ++i) {
        float a0 = nf::TAU * i / seg, a1 = nf::TAU * (i + 1) / seg;
        V3 n0 = rt * std::cos(a0) + u2 * std::sin(a0);
        V3 n1 = rt * std::cos(a1) + u2 * std::sin(a1);
        Vertex va, vb, vc, vd;
        va.p = a + n0 * r; vb.p = a + n1 * r;
        vc.p = b + n1 * r; vd.p = b + n0 * r;
        va.n = n0; vb.n = n1; vc.n = n1; vd.n = n0;
        va.u = (float)i / seg * (len / uvScale); va.v = 0;
        vb.u = (float)(i + 1) / seg * (len / uvScale); vb.v = 0;
        vc.u = vb.u; vc.v = len / uvScale;
        vd.u = va.u; vd.v = len / uvScale;
        va.baked = vb.baked = vc.baked = vd.baked = V3(aoDefault, 0, 0);
        va.col = vb.col = vc.col = vd.col = 0xFFFFFFFF;
        mesh.addQuad(va, vb, vc, vd);
    }
}

void GeoBuilder::sphere(const V3& c, float r, int segU, int segV) {
    for (int j = 0; j < segV; ++j) {
        float v0 = nf::PI * j / segV, v1 = nf::PI * (j + 1) / segV;
        for (int i = 0; i < segU; ++i) {
            float u0 = nf::TAU * i / segU, u1 = nf::TAU * (i + 1) / segU;
            auto P = [&](float u, float v) {
                return V3(c.x + r * std::sin(v) * std::cos(u), c.y + r * std::sin(v) * std::sin(u),
                          c.z + r * std::cos(v));
            };
            V3 p00 = P(u0, v0), p10 = P(u1, v0), p11 = P(u1, v1), p01 = P(u0, v1);
            Vertex va, vb, vc, vd;
            va.p = p00; vb.p = p10; vc.p = p11; vd.p = p01;
            va.n = nf::normalize(p00 - c); vb.n = nf::normalize(p10 - c);
            vc.n = nf::normalize(p11 - c); vd.n = nf::normalize(p01 - c);
            va.u = (float)i / segU; va.v = (float)j / segV;
            vb.u = (float)(i + 1) / segU; vb.v = (float)j / segV;
            vc.u = (float)(i + 1) / segU; vc.v = (float)(j + 1) / segV;
            vd.u = (float)i / segU; vd.v = (float)(j + 1) / segV;
            va.baked = vb.baked = vc.baked = vd.baked = V3(aoDefault, 0, 0);
            va.col = vb.col = vc.col = vd.col = 0xFFFFFFFF;
            mesh.addQuad(va, vb, vc, vd);
        }
    }
}

void GeoBuilder::plane(const V3& center, const V3& right, const V3& upv, float w, float h, float scale) {
    V3 n = nf::normalize(nf::cross(right, upv));
    V3 a = center - right * (w * 0.5f) - upv * (h * 0.5f);
    V3 b = center + right * (w * 0.5f) - upv * (h * 0.5f);
    V3 c = center + right * (w * 0.5f) + upv * (h * 0.5f);
    V3 d = center - right * (w * 0.5f) + upv * (h * 0.5f);
    quad(a, b, c, d, n, 0, 0, w / scale, h / scale);
}

void GeoBuilder::wallWithHole(const V3& start, const V3& end, float z0, float z1,
                             float holeX0, float holeX1, float holeZ0, float holeZ1,
                             float thickness, bool bothSides, int edgeMat) {
    V3 dir = end - start;
    float len = nf::length(dir);
    if (len < 1e-4f) return;
    dir = dir / len;
    V3 n = nf::normalize(nf::V3(-dir.y, dir.x, 0));
    V3 off = n * (thickness * 0.5f);
    int savedMat = mat;
    if (edgeMat >= 0) mat = edgeMat;
    auto seg = [&](float x0, float x1, float zz0, float zz1) {
        if (x1 - x0 < 1e-4f || zz1 - zz0 < 1e-4f) return;
        V3 a = start + dir * x0 + off + V3(0, 0, zz0);
        V3 b = start + dir * x1 + off + V3(0, 0, zz0);
        V3 c = start + dir * x1 + off + V3(0, 0, zz1);
        V3 d = start + dir * x0 + off + V3(0, 0, zz1);
        quad(a, b, c, d, n, 0, 0, 0, 0);
        if (bothSides) {
            V3 a2 = a - off * 2.0f, b2 = b - off * 2.0f, c2 = c - off * 2.0f, d2 = d - off * 2.0f;
            quad(a2, b2, c2, d2, -n, 0, 0, 0, 0);
        }
    };
    seg(0, nf::minf(holeX0, len), z0, z1);
    seg(nf::maxf(holeX1, 0.0f), len, z0, z1);
    seg(nf::maxf(holeX0, 0.0f), nf::minf(holeX1, len), z0, nf::minf(holeZ0, z1));
    seg(nf::maxf(holeX0, 0.0f), nf::minf(holeX1, len), nf::maxf(holeZ1, z0), z1);
    // reveal (hole edges)
    if (edgeMat < 0) mat = savedMat;
    float t = thickness;
    {
        // left/right reveals
        V3 q0 = start + dir * holeX0 + V3(0, 0, holeZ0);
        V3 q1 = start + dir * holeX0 + V3(0, 0, holeZ1);
        V3 q2 = start + dir * holeX0 + off * 2 + V3(0, 0, holeZ1);
        V3 q3 = start + dir * holeX0 + off * 2 + V3(0, 0, holeZ0);
        quad(q0, q1, q2, q3, dir, 0, 0, t / uvScale, 1);
        V3 r0 = start + dir * holeX1 + V3(0, 0, holeZ0);
        V3 r1 = start + dir * holeX1 + V3(0, 0, holeZ1);
        V3 r2 = start + dir * holeX1 + off * 2 + V3(0, 0, holeZ1);
        V3 r3 = start + dir * holeX1 + off * 2 + V3(0, 0, holeZ0);
        quad(r0, r1, r2, r3, -dir, 0, 0, t / uvScale, 1);
        V3 s0 = start + dir * holeX0 + V3(0, 0, holeZ1);
        V3 s1 = start + dir * holeX1 + V3(0, 0, holeZ1);
        V3 s2 = start + dir * holeX1 + off * 2 + V3(0, 0, holeZ1);
        V3 s3 = start + dir * holeX0 + off * 2 + V3(0, 0, holeZ1);
        quad(s0, s1, s2, s3, V3(0, 0, -1), 0, 0, 1, 1);
    }
    mat = savedMat;
}

void GeoBuilder::finish(V3* boundsLo, V3* boundsHi) {
    mesh.finalize();
    if (boundsLo && boundsHi) { *boundsLo = mesh.bounds.lo; *boundsHi = mesh.bounds.hi; }
}

// ------------------------------------------------------------------ local helpers
void GeoBuilder::lbox(float x0, float y0, float z0, float x1, float y1, float z1, bool top, bool bottom) {
    lboxO(V3(nf::minf(x0, x1), nf::minf(y0, y1), nf::minf(z0, z1)),
          V3(nf::maxf(x0, x1), nf::maxf(y0, y1), nf::maxf(z0, z1)), top, bottom);
}

void GeoBuilder::lboxO(const V3& lo, const V3& hi, bool top, bool bottom) {
    V3 c[8];
    int k = 0;
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                c[k++] = toWorld(V3(x ? hi.x : lo.x, y ? hi.y : lo.y, z ? hi.z : lo.z));
    // index convention: k = z*4 + y*2 + x
    auto Q = [&](int a, int b, int cc, int d) {
        V3 p0 = c[a], p1 = c[b], p2 = c[cc], p3 = c[d];
        V3 n = nf::normalize(nf::cross(p1 - p0, p3 - p0));
        // uv from local coords
        V3 mid = (p0 + p2) * 0.5f;
        V3 ln = rotDir(nf::V3(0, 0, 1));
        if (std::fabs(nf::dot(n, ln)) > 0.6f) {
            p0 = p0; // top/bottom: project to local x/y
            auto uv = [&](const V3& p) { V3 l = p - origin; V3 r((l.x * std::cos(-yaw) - l.y * std::sin(-yaw)),
                                                               (l.x * std::sin(-yaw) + l.y * std::cos(-yaw)), 0);
                                        return nf::V2(r.x / uvScale, r.y / uvScale); };
            nf::V2 u0 = uv(c[a]), u1 = uv(c[b]), u2 = uv(c[cc]), u3 = uv(c[d]);
            quad(p0, p1, p2, p3, n, u0.x, u0.y, u2.x, u2.y);
        } else {
            auto uv = [&](const V3& p) { V3 l = p - origin; V3 r((l.x * std::cos(-yaw) - l.y * std::sin(-yaw)),
                                                               (l.x * std::sin(-yaw) + l.y * std::cos(-yaw)), 0);
                                        return nf::V2(r.x / uvScale, l.z / uvScale); };
            nf::V2 u0 = uv(c[a]), u2 = uv(c[cc]);
            quad(p0, p1, p2, p3, n, u0.x, u0.y, u2.x, u2.y);
        }
        (void)mid;
    };
    Q(1, 3, 7, 5);   // +x
    Q(0, 4, 6, 2);   // -x
    Q(2, 6, 7, 3);   // +y
    Q(0, 1, 5, 4);   // -y
    if (top) Q(4, 5, 7, 6);      // +z
    if (bottom) Q(0, 2, 3, 1);   // -z
}

void GeoBuilder::lcyl(float x, float y, float z, float r, float hgt, int seg, bool capTop) {
    V3 b = toWorld(V3(x, y, z));
    int old = 0;
    (void)old;
    // cylinders are rotationally symmetric: build directly
    cylinder(b, r, hgt, seg, capTop, false);
}

void GeoBuilder::lpanel(float x0, float y0, float x1, float y1, float z) {
    V3 a = toWorld(V3(x0, y0, z)), b = toWorld(V3(x1, y0, z));
    V3 c = toWorld(V3(x1, y1, z)), d = toWorld(V3(x0, y1, z));
    quad(a, b, c, d, rotDir(V3(0, 0, 1)), x0 / uvScale, y0 / uvScale, x1 / uvScale, y1 / uvScale);
}

void GeoBuilder::lquad(const V3& a, const V3& b, const V3& c, const V3& d) {
    V3 pa = toWorld(a), pb = toWorld(b), pc = toWorld(c), pd = toWorld(d);
    V3 n = nf::normalize(nf::cross(pb - pa, pd - pa));
    quad(pa, pb, pc, pd, n, 0, 0, 1, 1);
}

// ============================================================================
//  PROP LIBRARY
// ============================================================================
namespace props {

// Each prop receives a builder with its material already set; functions change
// g.mat for different parts and restore it.

static void setM(GeoBuilder& g, int m) { g.mat = m; }

void chair(GeoBuilder& g, const V3& base, float yaw, int woodMat) {
    g.setFrame(base, yaw);
    setM(g, woodMat);
    g.uvScale = 1.0f;
    g.lbox(-0.22f, -0.22f, 0.42f, 0.22f, 0.22f, 0.47f);       // seat
    for (int i = 0; i < 4; ++i) {                                // legs
        float lx = (i & 1) ? 0.18f : -0.18f;
        float ly = (i & 2) ? 0.18f : -0.18f;
        g.lbox(lx - 0.02f, ly - 0.02f, 0.0f, lx + 0.02f, ly + 0.02f, 0.43f);
    }
    g.lbox(-0.20f, 0.18f, 0.47f, 0.20f, 0.22f, 0.92f);          // backrest
    g.lbox(-0.20f, 0.15f, 0.60f, 0.20f, 0.19f, 0.64f);          // slat
    g.lbox(-0.20f, 0.15f, 0.78f, 0.20f, 0.19f, 0.83f);
    g.clearFrame();
}

void table(GeoBuilder& g, const V3& base, float w, float d, float h, int woodMat) {
    g.setFrame(base, 0);
    setM(g, woodMat);
    g.lbox(-w * 0.5f, -d * 0.5f, h - 0.04f, w * 0.5f, d * 0.5f, h);
    for (int i = 0; i < 4; ++i) {
        float lx = (i & 1) ? w * 0.5f - 0.06f : -w * 0.5f + 0.06f;
        float ly = (i & 2) ? d * 0.5f - 0.06f : -d * 0.5f + 0.06f;
        g.lbox(lx - 0.03f, ly - 0.03f, 0.0f, lx + 0.03f, ly + 0.03f, h - 0.04f);
    }
    // apron
    g.lbox(-w * 0.5f + 0.04f, -d * 0.5f + 0.04f, h - 0.12f, w * 0.5f - 0.04f, -d * 0.5f + 0.06f, h - 0.04f);
    g.lbox(-w * 0.5f + 0.04f, d * 0.5f - 0.06f, h - 0.12f, w * 0.5f - 0.04f, d * 0.5f - 0.04f, h - 0.04f);
    g.clearFrame();
}

void bed(GeoBuilder& g, const V3& base, float yaw, bool unmade) {
    g.setFrame(base, yaw);
    // frame
    setM(g, MAT_WOOD_DARK);
    g.lbox(-0.95f, -1.05f, 0.0f, 0.95f, 1.05f, 0.28f);
    g.lbox(-0.95f, 0.98f, 0.0f, 0.95f, 1.08f, 1.05f);           // headboard
    g.lbox(-0.95f, -1.08f, 0.0f, 0.95f, -1.02f, 0.52f);         // footboard
    // mattress
    setM(g, MAT_MATTRESS);
    g.uvScale = 1.2f;
    g.lbox(-0.90f, -0.98f, 0.28f, 0.90f, 0.98f, 0.50f);
    // pillow
    setM(g, MAT_COAT_WHITE);
    g.uvScale = 0.8f;
    g.lbox(-0.60f, 0.62f, 0.50f, 0.05f, 0.92f, 0.63f);
    g.lbox(0.10f, 0.62f, 0.50f, 0.62f, 0.92f, 0.62f);
    // blanket
    setM(g, unmade ? MAT_FABRIC_SOFA : MAT_VELVET);
    g.uvScale = 1.5f;
    if (unmade) {
        g.lbox(-0.92f, -1.0f, 0.48f, 0.92f, 0.45f, 0.56f);
        g.lbox(-0.92f, 0.35f, 0.50f, -0.2f, 0.5f, 0.62f);
    } else {
        g.lbox(-0.93f, -1.0f, 0.46f, 0.93f, 0.62f, 0.58f);
        g.lbox(-0.93f, 0.55f, 0.56f, 0.93f, 0.65f, 0.60f);
    }
    g.clearFrame();
}

void wardrobe(GeoBuilder& g, const V3& base, float yaw, int mat) {
    g.setFrame(base, yaw);
    setM(g, mat);
    g.uvScale = 1.0f;
    g.lbox(-0.60f, -0.30f, 0.0f, 0.60f, 0.30f, 2.05f);
    g.lbox(-0.62f, -0.32f, 2.05f, 0.62f, 0.32f, 2.10f);       // cornice
    // door split + handles
    setM(g, MAT_METAL_STEEL);
    g.lbox(-0.02f, 0.29f, 0.10f, 0.02f, 0.32f, 1.95f);
    g.lbox(-0.12f, 0.30f, 1.02f, -0.08f, 0.33f, 1.20f);
    g.lbox(0.08f, 0.30f, 1.02f, 0.12f, 0.33f, 1.20f);
    g.clearFrame();
}

void sofa(GeoBuilder& g, const V3& base, float yaw, int fabricMat) {
    g.setFrame(base, yaw);
    setM(g, MAT_WOOD_DARK);
    g.lbox(-1.0f, -0.42f, 0.0f, 1.0f, 0.42f, 0.12f);          // base
    for (int i = 0; i < 4; ++i) {
        float lx = (i & 1) ? 0.94f : -0.94f;
        float ly = (i & 2) ? 0.36f : -0.36f;
        g.lbox(lx - 0.03f, ly - 0.03f, 0.0f, lx + 0.03f, ly + 0.03f, 0.14f);
    }
    setM(g, fabricMat);
    g.uvScale = 1.0f;
    g.lbox(-1.0f, -0.42f, 0.12f, 1.0f, 0.42f, 0.42f);         // seat block
    g.lbox(-1.0f, -0.42f, 0.42f, 1.0f, -0.28f, 0.95f);          // back
    g.lbox(-1.0f, -0.42f, 0.42f, -0.88f, 0.42f, 0.68f);         // left arm
    g.lbox(0.88f, -0.42f, 0.42f, 1.0f, 0.42f, 0.68f);           // right arm
    // cushions
    g.lbox(-0.84f, -0.26f, 0.42f, -0.06f, 0.34f, 0.52f);
    g.lbox(0.06f, -0.26f, 0.42f, 0.84f, 0.34f, 0.52f);
    g.lbox(-0.80f, -0.24f, 0.52f, -0.10f, -0.10f, 0.86f);
    g.lbox(0.10f, -0.24f, 0.52f, 0.80f, -0.10f, 0.86f);
    g.clearFrame();
}

void armchair(GeoBuilder& g, const V3& base, float yaw, int fabricMat) {
    g.setFrame(base, yaw);
    setM(g, MAT_WOOD_DARK);
    g.lbox(-0.40f, -0.38f, 0.0f, 0.40f, 0.38f, 0.12f);
    for (int i = 0; i < 4; ++i) {
        float lx = (i & 1) ? 0.34f : -0.34f;
        float ly = (i & 2) ? 0.32f : -0.32f;
        g.lbox(lx - 0.025f, ly - 0.025f, 0.0f, lx + 0.025f, ly + 0.025f, 0.14f);
    }
    setM(g, fabricMat);
    g.uvScale = 1.0f;
    g.lbox(-0.40f, -0.38f, 0.12f, 0.40f, 0.38f, 0.42f);
    g.lbox(-0.40f, -0.38f, 0.42f, 0.40f, -0.22f, 0.98f);
    g.lbox(-0.40f, -0.38f, 0.42f, -0.30f, 0.38f, 0.66f);
    g.lbox(0.30f, -0.38f, 0.42f, 0.40f, 0.38f, 0.66f);
    g.lbox(-0.28f, -0.22f, 0.42f, 0.28f, 0.32f, 0.51f);
    g.clearFrame();
}

void kitchenRun(GeoBuilder& g, const V3& base, float len, int facing) {
    // built along local +x from base, counter depth 0.6 toward local +y
    g.setFrame(base, 0);
    setM(g, MAT_WOOD_LIGHT);
    g.uvScale = 1.0f;
    g.lbox(0.0f, 0.0f, 0.10f, len, 0.60f, 0.86f);            // carcass
    setM(g, MAT_METAL_STEEL);
    g.lbox(0.0f, 0.02f, 0.86f, len, 0.62f, 0.90f);            // worktop
    // doors and handles
    setM(g, MAT_WOOD_LIGHT);
    int n = (int)(len / 0.5f);
    for (int i = 0; i < n; ++i) {
        float x0 = 0.02f + i * 0.5f;
        g.lbox(x0, 0.60f, 0.12f, x0 + 0.46f, 0.62f, 0.84f);
        setM(g, MAT_METAL_STEEL);
        g.lbox(x0 + 0.18f, 0.62f, 0.74f, x0 + 0.28f, 0.64f, 0.76f);
        setM(g, MAT_WOOD_LIGHT);
    }
    // toe kick
    g.lbox(0.0f, 0.02f, 0.0f, len, 0.55f, 0.10f);
    // sink cut + basin
    setM(g, MAT_CERAMIC);
    float sx = len * 0.45f;
    g.lbox(sx - 0.22f, 0.08f, 0.70f, sx + 0.22f, 0.52f, 0.905f);
    setM(g, MAT_METAL_STEEL);
    g.lbox(sx - 0.20f, 0.10f, 0.72f, sx + 0.20f, 0.50f, 0.73f);
    g.cylinder(g.toWorld(V3(sx, 0.12f, 0.90f)), 0.018f, 0.22f, 8);
    g.lbox(sx - 0.02f, 0.10f, 1.10f, sx + 0.02f, 0.26f, 1.14f);
    // wall cabinets
    setM(g, MAT_WOOD_LIGHT);
    g.lbox(0.0f, 0.0f, 1.45f, len, 0.34f, 2.10f);
    for (int i = 0; i < n; ++i) {
        float x0 = 0.02f + i * 0.5f;
        setM(g, MAT_WOOD_LIGHT);
        g.lbox(x0, 0.34f, 1.47f, x0 + 0.46f, 0.36f, 2.08f);
        setM(g, MAT_METAL_STEEL);
        g.lbox(x0 + 0.20f, 0.36f, 1.52f, x0 + 0.26f, 0.38f, 1.56f);
    }
    (void)facing;
    g.clearFrame();
}

void fridge(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    setM(g, MAT_CERAMIC);
    g.uvScale = 1.0f;
    g.lbox(-0.30f, -0.30f, 0.0f, 0.30f, 0.30f, 1.75f);
    setM(g, MAT_METAL_STEEL);
    g.lbox(-0.30f, 0.30f, 1.20f, 0.30f, 0.32f, 1.22f);
    g.lbox(0.18f, 0.32f, 1.24f, 0.22f, 0.35f, 1.52f);
    g.lbox(0.18f, 0.32f, 0.80f, 0.22f, 0.35f, 1.10f);
    g.clearFrame();
}

void stove(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    setM(g, MAT_CERAMIC);
    g.lbox(-0.30f, -0.30f, 0.0f, 0.30f, 0.30f, 0.85f);
    setM(g, MAT_METAL_STEEL);
    g.lbox(-0.31f, -0.31f, 0.85f, 0.31f, 0.31f, 0.88f);
    for (int i = 0; i < 4; ++i) {
        float bx = (i & 1) ? 0.14f : -0.14f;
        float by = (i & 2) ? 0.14f : -0.14f;
        g.cylinder(g.toWorld(V3(bx, by, 0.88f)), 0.085f, 0.012f, 14);
    }
    g.lbox(-0.28f, 0.30f, 0.30f, 0.28f, 0.33f, 0.80f);   // oven door
    g.lbox(-0.20f, 0.33f, 0.72f, 0.20f, 0.36f, 0.76f);   // handle
    g.clearFrame();
}

void sink(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    setM(g, MAT_CERAMIC);
    g.lbox(-0.28f, -0.22f, 0.72f, 0.28f, 0.22f, 0.88f);
    g.lbox(-0.22f, -0.16f, 0.74f, 0.22f, 0.16f, 0.88f);
    g.clearFrame();
}

void toilet(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    setM(g, MAT_CERAMIC);
    g.uvScale = 1.0f;
    g.lbox(-0.20f, -0.28f, 0.0f, 0.20f, 0.28f, 0.40f);       // pedestal
    g.lbox(-0.19f, -0.26f, 0.40f, 0.19f, 0.26f, 0.44f);      // bowl rim
    g.lbox(-0.22f, -0.02f, 0.44f, 0.22f, 0.30f, 0.47f);      // seat
    g.lbox(-0.23f, 0.24f, 0.30f, 0.23f, 0.42f, 0.82f);        // cistern
    setM(g, MAT_METAL_STEEL);
    g.lbox(-0.03f, 0.20f, 0.78f, 0.03f, 0.24f, 0.82f);
    g.clearFrame();
}

void bathtub(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    setM(g, MAT_CERAMIC);
    g.uvScale = 1.0f;
    float w = 0.75f, l = 1.65f;
    // rim
    g.lbox(-w * 0.5f, 0.0f, 0.0f, w * 0.5f, l, 0.55f);
    // inner cavity walls (thin boxes)
    g.lbox(-w * 0.5f + 0.06f, 0.06f, 0.10f, w * 0.5f - 0.06f, l - 0.06f, 0.12f);
    g.lbox(-w * 0.5f + 0.06f, 0.06f, 0.12f, w * 0.5f - 0.06f, 0.10f, 0.56f);
    g.lbox(-w * 0.5f + 0.06f, l - 0.10f, 0.12f, w * 0.5f - 0.06f, l - 0.06f, 0.56f);
    g.lbox(-w * 0.5f + 0.06f, 0.10f, 0.12f, -w * 0.5f + 0.10f, l - 0.10f, 0.56f);
    g.lbox(w * 0.5f - 0.10f, 0.10f, 0.12f, w * 0.5f - 0.06f, l - 0.10f, 0.56f);
    setM(g, MAT_METAL_STEEL);
    g.cylinder(g.toWorld(V3(0, 0.07f, 0.55f)), 0.02f, 0.16f, 8);
    g.lbox(-0.10f, 0.02f, 0.70f, 0.10f, 0.10f, 0.74f);
    g.clearFrame();
}

void washbasin(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    setM(g, MAT_CERAMIC);
    g.lbox(-0.28f, -0.20f, 0.74f, 0.28f, 0.20f, 0.90f);
    g.lbox(-0.24f, -0.16f, 0.80f, 0.24f, 0.16f, 0.905f);
    g.lbox(-0.05f, -0.2f, 0.0f, 0.05f, 0.2f, 0.74f);
    setM(g, MAT_METAL_STEEL);
    g.cylinder(g.toWorld(V3(0, -0.12f, 0.90f)), 0.016f, 0.16f, 8);
    g.lbox(-0.02f, -0.16f, 1.04f, 0.02f, -0.02f, 1.07f);
    g.clearFrame();
}

void mirror(GeoBuilder& g, const V3& base, float yaw, float w, float h) {
    g.setFrame(base, yaw);
    setM(g, MAT_METAL_STEEL);
    g.lbox(-w * 0.5f - 0.03f, -0.02f, -0.03f, w * 0.5f + 0.03f, 0.02f, h + 0.03f);
    setM(g, MAT_GLASS);
    g.lbox(-w * 0.5f, 0.0f, 0.0f, w * 0.5f, 0.005f, h);
    g.clearFrame();
}

void bookshelf(GeoBuilder& g, const V3& base, float yaw, int shelves, float w) {
    g.setFrame(base, yaw);
    setM(g, MAT_WOOD_DARK);
    g.uvScale = 1.0f;
    g.lbox(-w * 0.5f, -0.16f, 0.0f, w * 0.5f, 0.16f, 0.05f);
    g.lbox(-w * 0.5f, -0.16f, 0.0f, -w * 0.5f + 0.04f, 0.16f, shelves * 0.36f + 0.1f);
    g.lbox(w * 0.5f - 0.04f, -0.16f, 0.0f, w * 0.5f, 0.16f, shelves * 0.36f + 0.1f);
    g.lbox(-w * 0.5f, -0.16f, shelves * 0.36f + 0.06f, w * 0.5f, 0.16f, shelves * 0.36f + 0.1f);
    for (int i = 1; i <= shelves; ++i) {
        float z = i * 0.36f;
        g.lbox(-w * 0.5f + 0.02f, -0.15f, z - 0.03f, w * 0.5f - 0.02f, 0.15f, z);
        // books
        int ob = g.mat;
        g.mat = MAT_BOOKS;
        g.uvScale = 1.4f;
        g.lbox(-w * 0.5f + 0.06f, -0.13f, z, w * 0.5f - 0.06f, 0.12f, z + 0.24f);
        g.mat = ob;
        g.uvScale = 1.0f;
    }
    g.clearFrame();
}

void desk(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    setM(g, MAT_WOOD_LIGHT);
    g.uvScale = 1.0f;
    g.lbox(-0.7f, -0.35f, 0.72f, 0.7f, 0.35f, 0.76f);
    g.lbox(-0.66f, -0.31f, 0.0f, -0.58f, 0.31f, 0.72f);
    g.lbox(0.42f, -0.31f, 0.0f, 0.66f, 0.31f, 0.72f);
    g.lbox(0.42f, -0.30f, 0.40f, 0.66f, 0.30f, 0.72f);
    setM(g, MAT_METAL_STEEL);
    g.lbox(-0.08f, -0.16f, 0.76f, 0.18f, 0.10f, 0.77f);        // papers
    g.clearFrame();
}

void lamp(GeoBuilder& g, const V3& base, bool floor, bool lit) {
    g.setFrame(base, 0);
    setM(g, MAT_METAL_STEEL);
    g.uvScale = 0.6f;
    if (floor) {
        g.cylinder(base, 0.16f, 0.03f, 14);
        g.cylinder(g.toWorld(V3(0, 0, 0.03f)), 0.016f, 1.35f, 8);
        g.lbox(-0.12f, -0.12f, 1.35f, 0.12f, 0.12f, 1.37f);
        setM(g, MAT_LAMPSHADE);
        g.cylinder(g.toWorld(V3(0, 0, 1.30f)), 0.17f, 0.26f, 16, false);
        setM(g, MAT_UNLIT_WHITE);
        if (lit) g.cylinder(g.toWorld(V3(0, 0, 1.38f)), 0.06f, 0.10f, 10, false);
    } else {
        g.cylinder(base, 0.11f, 0.02f, 12);
        g.cylinder(g.toWorld(V3(0, 0, 0.02f)), 0.012f, 0.32f, 8);
        setM(g, MAT_LAMPSHADE);
        g.cylinder(g.toWorld(V3(0, 0, 0.30f)), 0.13f, 0.18f, 14, false);
        setM(g, MAT_UNLIT_WHITE);
        if (lit) g.cylinder(g.toWorld(V3(0, 0, 0.34f)), 0.05f, 0.08f, 8, false);
    }
    g.clearFrame();
}

void ceilingLight(GeoBuilder& g, const V3& pos, bool fluorescent) {
    g.setFrame(pos, 0);
    setM(g, MAT_METAL_STEEL);
    g.uvScale = 0.5f;
    if (fluorescent) {
        g.lbox(-0.62f, -0.12f, -0.10f, 0.62f, 0.12f, 0.0f);
        setM(g, MAT_UNLIT_WHITE);
        g.lbox(-0.60f, -0.10f, -0.115f, 0.60f, 0.10f, -0.10f);
    } else {
        g.cylinder(g.toWorld(V3(0, 0, -0.06f)), 0.28f, 0.06f, 12);
        setM(g, MAT_LAMPSHADE);
        g.cylinder(g.toWorld(V3(0, 0, -0.02f)), 0.24f, 0.16f, 14, false);
        setM(g, MAT_UNLIT_WHITE);
        g.cylinder(g.toWorld(V3(0, 0, -0.18f)), 0.16f, 0.02f, 12);
    }
    g.clearFrame();
}

void wallLamp(GeoBuilder& g, const V3& pos, const V3& normal) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    setM(g, MAT_METAL_STEEL);
    g.lbox(-0.05f, 0.0f, -0.05f, 0.05f, 0.06f, 0.05f);
    g.lbox(-0.014f, 0.06f, -0.014f, 0.014f, 0.24f, 0.014f);
    setM(g, MAT_LAMPSHADE);
    g.cylinder(g.toWorld(V3(0, 0.22f, -0.10f)), 0.13f, 0.20f, 14, false);
    setM(g, MAT_UNLIT_WHITE);
    g.cylinder(g.toWorld(V3(0, 0.22f, -0.10f)), 0.10f, 0.01f, 10);
    g.clearFrame();
}

void painting(GeoBuilder& g, const V3& pos, const V3& normal, float w, float h, int texMat) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    setM(g, MAT_WOOD_DARK);
    g.uvScale = 0.7f;
    g.lbox(-w * 0.5f - 0.04f, 0.0f, -h * 0.5f - 0.04f, w * 0.5f + 0.04f, 0.035f, h * 0.5f + 0.04f);
    setM(g, texMat);
    g.uvScale = 1.0f;
    g.lbox(-w * 0.5f, 0.035f, -h * 0.5f, w * 0.5f, 0.045f, h * 0.5f);
    g.clearFrame();
}

void radiator(GeoBuilder& g, const V3& base, float yaw, float w) {
    g.setFrame(base, yaw);
    setM(g, MAT_METAL_PAINTED);
    g.uvScale = 0.5f;
    int fins = (int)(w / 0.08f);
    for (int i = 0; i < fins; ++i) {
        float x = -w * 0.5f + i * 0.08f;
        g.lbox(x, 0.0f, 0.12f, x + 0.045f, 0.11f, 0.68f);
    }
    setM(g, MAT_METAL_STEEL);
    g.lbox(-w * 0.5f, 0.02f, 0.66f, w * 0.5f, 0.09f, 0.70f);
    g.lbox(-w * 0.5f, 0.02f, 0.10f, w * 0.5f, 0.09f, 0.14f);
    g.cylinder(g.toWorld(V3(-w * 0.5f, 0.05f, 0.14f)), 0.02f, 0.16f, 6);
    g.cylinder(g.toWorld(V3(w * 0.5f, 0.05f, 0.68f)), 0.02f, 0.16f, 6);
    g.clearFrame();
}

void pipeRun(GeoBuilder& g, const V3& a, const V3& b, float r) {
    g.mat = MAT_METAL_RUST;
    g.uvScale = 1.2f;
    g.cylinderAxis(a, b, r, 8);
    // flanges
    V3 d = b - a;
    float L = nf::length(d);
    if (L > 0.4f) {
        int n = (int)(L / 2.4f);
        for (int i = 1; i <= n; ++i) {
            V3 p = a + d * ((float)i / (n + 1));
            V3 off = nf::normalize(d) * 0.02f;
            g.cylinderAxis(p - off, p + off, r * 1.35f, 8);
        }
    }
}

void doorFrame(GeoBuilder& g, const V3& hinge, const V3& axis, const V3& normal,
               float width, float height) {
    float yaw = std::atan2(axis.y, axis.x);
    g.setFrame(hinge, yaw);
    g.mat = MAT_WOOD_DARK;
    g.uvScale = 0.8f;
    g.lbox(-0.05f, -0.09f, 0.0f, 0.05f, 0.09f, height + 0.05f);      // hinge post
    g.lbox(width - 0.05f, -0.09f, 0.0f, width + 0.05f, 0.09f, height + 0.05f);
    g.lbox(-0.05f, -0.09f, height - 0.02f, width + 0.05f, 0.09f, height + 0.05f);
    g.clearFrame();
    (void)normal;
}

void doorLeaf(GeoBuilder& g, const V3& hinge, const V3& axis, const V3& normal,
              float width, float height, float thickness, float openAngle, int mat) {
    // leaf rotates about hinge; openAngle in radians, positive swings along +normal
    V3 dir = axis * std::cos(openAngle) + normal * std::sin(openAngle);
    V3 n = nf::normalize(nf::cross(V3(0, 0, 1), dir));
    if (nf::dot(n, normal) < 0) n = -n;
    V3 a = hinge;
    V3 b = hinge + dir * width;
    g.mat = mat;
    g.uvScale = 1.0f;
    g.flipNormals = false;
    // two faces + edges
    V3 th = n * (thickness * 0.5f);
    g.quad(a + th, b + th, V3(b.x + th.x, b.y + th.y, hinge.z + height), V3(a.x + th.x, a.y + th.y, hinge.z + height),
           n, 0, 0, width / 1.0f, height / 1.0f);
    g.quad(a - th, b - th, V3(b.x - th.x, b.y - th.y, hinge.z + height), V3(a.x - th.x, a.y - th.y, hinge.z + height),
           -n, 0, 0, width / 1.0f, height / 1.0f);
    // edges
    V3 up(0, 0, 1);
    g.quad(a + th, a - th, V3(a.x - th.x, a.y - th.y, hinge.z + height), V3(a.x + th.x, a.y + th.y, hinge.z + height),
           nf::normalize(nf::cross(-th, up)), 0, 0, thickness, height);
    g.quad(b + th, b - th, V3(b.x - th.x, b.y - th.y, hinge.z + height), V3(b.x + th.x, b.y + th.y, hinge.z + height),
           nf::normalize(nf::cross(th, up)), 0, 0, thickness, height);
    g.quad(V3(a.x, a.y, hinge.z + height), V3(b.x, b.y, hinge.z + height),
           V3(b.x, b.y, hinge.z + height) + th * 2, V3(a.x, a.y, hinge.z + height) + th * 2,
           up, 0, 0, width, thickness);
    // handle
    V3 mid = a + dir * (width * 0.82f) + n * (thickness * 0.5f + 0.02f);
    g.mat = MAT_METAL_STEEL;
    g.uvScale = 0.4f;
    g.cylinder(mid + V3(0, 0, 1.0f), 0.022f, 0.10f, 8, true, false);
    g.quad(mid + V3(0, 0, 1.0f), mid + dir * 0.10f + V3(0, 0, 1.0f),
           mid + dir * 0.10f + V3(0, 0, 1.07f), mid + V3(0, 0, 1.07f), n, 0, 0, 1, 1);
    g.mat = mat;
}

void windowOpening(GeoBuilder& g, const V3& center, const V3& normal, float w, float h) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(center, yaw);
    g.mat = MAT_WOOD_LIGHT;
    g.uvScale = 0.6f;
    // frame
    g.lbox(-w * 0.5f - 0.05f, -0.06f, -h * 0.5f - 0.05f, w * 0.5f + 0.05f, 0.06f, -h * 0.5f);
    g.lbox(-w * 0.5f - 0.05f, -0.06f, h * 0.5f, w * 0.5f + 0.05f, 0.06f, h * 0.5f + 0.05f);
    g.lbox(-w * 0.5f - 0.05f, -0.06f, -h * 0.5f, -w * 0.5f, 0.06f, h * 0.5f);
    g.lbox(w * 0.5f, -0.06f, -h * 0.5f, w * 0.5f + 0.05f, 0.06f, h * 0.5f);
    g.lbox(-0.02f, -0.04f, -h * 0.5f, 0.02f, 0.04f, h * 0.5f);    // mullion
    g.lbox(-w * 0.5f, -0.04f, -0.02f, w * 0.5f, 0.04f, 0.02f);
    g.mat = MAT_GLASS;
    g.uvScale = 1.5f;
    // glass as two planes with alpha texture
    g.quad(center - V3(0, 0, h * 0.5f) - V3(w * 0.5f, 0, 0) + normal * 0.005f,
           center - V3(0, 0, h * 0.5f) + V3(w * 0.5f, 0, 0) + normal * 0.005f,
           center + V3(0, 0, h * 0.5f) + V3(w * 0.5f, 0, 0) + normal * 0.005f,
           center + V3(0, 0, h * 0.5f) - V3(w * 0.5f, 0, 0) + normal * 0.005f,
           normal, 0, 0, w / 1.5f, h / 1.5f);
    // sill
    g.clearFrame();
}

void curtain(GeoBuilder& g, const V3& topCenter, const V3& normal, float w, float h) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(topCenter, yaw);
    g.mat = MAT_CURTAIN;
    g.uvScale = 1.0f;
    int pleats = (int)(w / 0.14f);
    for (int i = 0; i < pleats; ++i) {
        float x0 = -w * 0.5f + i * (w / pleats);
        float depth = 0.05f + 0.04f * std::sin(i * 1.7f);
        g.lbox(x0, 0.0f, -h, x0 + (w / pleats) * 0.9f, depth, 0.0f);
    }
    g.mat = MAT_METAL_STEEL;
    g.cylinder(g.toWorld(V3(-w * 0.5f - 0.1f, 0.04f, 0.03f)), 0.014f, w + 0.2f, 6);
    g.clearFrame();
}

void rug(GeoBuilder& g, const V3& center, float w, float d, int texMat, float yaw) {
    g.setFrame(center + V3(0, 0, 0.004f), yaw);
    g.mat = texMat;
    g.uvScale = 1.0f;
    g.lpanel(-w * 0.5f, -d * 0.5f, w * 0.5f, d * 0.5f, 0.0f);
    g.clearFrame();
}

void cabinet(GeoBuilder& g, const V3& base, float yaw, float w, float h) {
    g.setFrame(base, yaw);
    g.mat = MAT_PANEL_WOOD;
    g.uvScale = 1.0f;
    g.lbox(-w * 0.5f, -0.22f, 0.0f, w * 0.5f, 0.22f, h);
    g.mat = MAT_METAL_STEEL;
    int n = nf::maxi(1, (int)(w / 0.55f));
    for (int i = 0; i < n; ++i) {
        float x = -w * 0.5f + (i + 0.5f) * (w / n);
        g.lbox(x - 0.02f, 0.22f, h * 0.5f, x + 0.02f, 0.24f, h * 0.5f + 0.10f);
    }
    g.clearFrame();
}

void crate(GeoBuilder& g, const V3& base, float yaw, float s) {
    g.setFrame(base, yaw);
    g.mat = MAT_WOOD_PLANKS;
    g.uvScale = 1.0f;
    g.lbox(-s * 0.5f, -s * 0.5f, 0.0f, s * 0.5f, s * 0.5f, s);
    g.mat = MAT_WOOD_DARK;
    g.lbox(-s * 0.5f - 0.01f, -s * 0.5f - 0.01f, s * 0.7f, s * 0.5f + 0.01f, s * 0.5f + 0.01f, s * 0.78f);
    g.clearFrame();
}

void barrel(GeoBuilder& g, const V3& base, float yaw, float r, float h) {
    g.setFrame(base, yaw);
    g.mat = MAT_METAL_RUST;
    g.uvScale = 1.2f;
    g.lcyl(0, 0, 0, r, h, 16, true);
    g.mat = MAT_METAL_STEEL;
    g.lcyl(0, 0, h * 0.25f, r * 1.03f, 0.05f, 16, false);
    g.lcyl(0, 0, h * 0.70f, r * 1.03f, 0.05f, 16, false);
    g.clearFrame();
}

void lockers(GeoBuilder& g, const V3& base, float yaw, int count) {
    g.setFrame(base, yaw);
    float w = 0.42f;
    for (int i = 0; i < count; ++i) {
        float x = i * w;
        g.mat = MAT_METAL_PAINTED;
        g.uvScale = 1.0f;
        g.lbox(x, -0.25f, 0.0f, x + w - 0.02f, 0.25f, 1.85f);
        g.mat = MAT_METAL_STEEL;
        g.lbox(x + 0.03f, -0.28f, 1.55f, x + w - 0.05f, -0.25f, 1.60f);   // vents
        g.lbox(x + 0.03f, -0.28f, 1.45f, x + w - 0.05f, -0.25f, 1.50f);
        g.lbox(x + w - 0.12f, -0.29f, 0.95f, x + w - 0.08f, -0.26f, 1.15f); // handle
    }
    g.clearFrame();
}

void washingMachine(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    g.mat = MAT_CERAMIC;
    g.uvScale = 1.0f;
    g.lbox(-0.30f, -0.30f, 0.0f, 0.30f, 0.30f, 0.85f);
    g.mat = MAT_GLASS;
    g.cylinder(g.toWorld(V3(0, -0.30f, 0.45f)), 0.16f, 0.02f, 16);
    g.mat = MAT_METAL_STEEL;
    g.cylinder(g.toWorld(V3(0, -0.31f, 0.30f)), 0.19f, 0.02f, 16, false);
    g.lbox(-0.26f, -0.28f, 0.86f, 0.26f, 0.26f, 0.88f);
    g.lbox(-0.20f, -0.10f, 0.60f, 0.20f, 0.10f, 0.62f);
    g.clearFrame();
}

void boilerTank(GeoBuilder& g, const V3& base, float r, float h) {
    g.setFrame(base, 0);
    g.mat = MAT_METAL_PAINTED;
    g.uvScale = 1.5f;
    g.lcyl(0, 0, 0, r, h, 20, true);
    g.mat = MAT_METAL_STEEL;
    g.lcyl(0, 0, h + 0.0f, r * 0.35f, 0.6f, 10, true);
    g.lcyl(0, 0, 0.0f, r * 1.05f, 0.10f, 20, false);
    // gauges
    g.cylinder(g.toWorld(V3(r * 0.9f, 0, h * 0.7f)), 0.09f, 0.02f, 12);
    g.clearFrame();
}

void boilerPanel(GeoBuilder& g, const V3& base, const V3& normal) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(base, yaw);
    g.mat = MAT_METAL_STEEL;
    g.uvScale = 0.8f;
    g.lbox(-0.30f, 0.0f, 0.0f, 0.30f, 0.12f, 0.55f);
    g.mat = MAT_METAL_RUST;
    for (int i = 0; i < 3; ++i) g.lbox(-0.22f, 0.12f, 0.10f + i * 0.14f, 0.10f, 0.13f, 0.18f + i * 0.14f);
    g.mat = MAT_METAL_STEEL;
    g.lbox(0.14f, 0.12f, 0.30f, 0.22f, 0.15f, 0.38f);
    g.clearFrame();
}

void elevatorDoors(GeoBuilder& g, const V3& center, const V3& normal, float w, float h) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(center, yaw);
    g.mat = MAT_METAL_STEEL;
    g.uvScale = 1.4f;
    g.lbox(-w * 0.5f, -0.03f, 0.0f, -0.01f, 0.03f, h);
    g.lbox(0.01f, -0.03f, 0.0f, w * 0.5f, 0.03f, h);
    g.mat = MAT_METAL_PAINTED;
    g.lbox(-w * 0.5f + 0.06f, 0.03f, 0.10f, -0.05f, 0.05f, h - 0.10f);
    g.lbox(0.05f, 0.03f, 0.10f, w * 0.5f - 0.06f, 0.05f, h - 0.10f);
    g.clearFrame();
}

void elevatorFrame(GeoBuilder& g, const V3& center, const V3& normal, float w, float h) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(center, yaw);
    g.mat = MAT_METAL_PAINTED;
    g.uvScale = 1.0f;
    g.lbox(-w * 0.5f - 0.16f, -0.06f, 0.0f, -w * 0.5f, 0.06f, h + 0.16f);
    g.lbox(w * 0.5f, -0.06f, 0.0f, w * 0.5f + 0.16f, 0.06f, h + 0.16f);
    g.lbox(-w * 0.5f - 0.16f, -0.06f, h, w * 0.5f + 0.16f, 0.06f, h + 0.16f);
    // floor indicator + call button
    g.mat = MAT_METAL_STEEL;
    g.lbox(-0.10f, 0.06f, h + 0.35f, 0.10f, 0.09f, h + 0.50f);
    g.lbox(w * 0.5f + 0.22f, 0.02f, 1.15f, w * 0.5f + 0.36f, 0.05f, 1.40f);
    g.cylinder(g.toWorld(V3(w * 0.5f + 0.29f, 0.05f, 1.32f)), 0.03f, 0.02f, 10);
    g.clearFrame();
}

void stairs(GeoBuilder& g, const V3& base, int steps, float rise, float run, float width,
            float yaw, int mat) {
    g.setFrame(base, yaw);
    g.mat = mat;
    g.uvScale = 1.2f;
    for (int i = 0; i < steps; ++i) {
        float z = i * rise;
        g.lbox(0.0f, i * run, 0.0f, width, (i + 1) * run, z + rise);
        g.mat = MAT_WOOD_DARK;
        g.lbox(0.0f, i * run, z + rise - 0.03f, width, (i + 1) * run - 0.02f, z + rise);
        g.mat = mat;
    }
    g.clearFrame();
}

void railing(GeoBuilder& g, const V3& a, const V3& b, float h) {
    g.mat = MAT_METAL_STEEL;
    g.uvScale = 1.0f;
    // top rail follows the slope
    g.cylinderAxis(a + V3(0, 0, h), b + V3(0, 0, h), 0.022f, 6);
    float L = nf::length(b - a);
    int n = nf::maxi(2, (int)(L / 0.9f));
    for (int i = 0; i <= n; ++i) {
        V3 p = a + (b - a) * ((float)i / n);
        g.cylinderAxis(p, p + V3(0, 0, h), 0.014f, 6);
    }
}

void sign(GeoBuilder& g, const V3& pos, const V3& normal, float w, float h, int mat) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    g.mat = mat;
    g.uvScale = 1.0f;
    g.lbox(-w * 0.5f, 0.0f, -h * 0.5f, w * 0.5f, 0.01f, h * 0.5f);
    g.clearFrame();
}

void switchPlate(GeoBuilder& g, const V3& pos, const V3& normal) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    g.mat = MAT_CERAMIC;
    g.uvScale = 0.3f;
    g.lbox(-0.045f, 0.0f, -0.06f, 0.045f, 0.012f, 0.06f);
    g.mat = MAT_METAL_STEEL;
    g.lbox(-0.008f, 0.012f, -0.03f, 0.008f, 0.022f, 0.03f);
    g.clearFrame();
}

void socket(GeoBuilder& g, const V3& pos, const V3& normal) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    g.mat = MAT_CERAMIC;
    g.uvScale = 0.3f;
    g.lbox(-0.04f, 0.0f, -0.04f, 0.04f, 0.012f, 0.04f);
    g.mat = MAT_UNLIT_BLACK;
    g.lbox(-0.015f, 0.012f, -0.005f, 0.015f, 0.014f, 0.02f);
    g.clearFrame();
}

void cobwebMesh(GeoBuilder& g, const V3& corner, const V3& normal, float size) {
    // translucent quad with a cobweb texture, alpha-tested
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(corner, yaw);
    g.mat = MAT_COBWEB;
    g.uvScale = 1.0f;
    g.quad(corner, corner + (normal * 0.0f) + V3(std::cos(yaw + 1.57f), std::sin(yaw + 1.57f), 0) * size,
           corner + (normal * 0.0f) + V3(std::cos(yaw + 1.57f), std::sin(yaw + 1.57f), 0) * size + V3(0, 0, -size),
           corner + V3(0, 0, -size), normal, 0, 0, 1, 1);
    g.clearFrame();
}

void tv(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    g.mat = MAT_WOOD_DARK;
    g.uvScale = 1.0f;
    g.lbox(-0.46f, -0.34f, 0.0f, 0.46f, 0.34f, 0.55f);
    g.mat = MAT_TV_STATIC;
    g.lbox(-0.36f, 0.34f, 0.10f, 0.36f, 0.36f, 0.48f);
    g.mat = MAT_METAL_STEEL;
    g.lbox(-0.42f, 0.34f, 0.50f, 0.42f, 0.36f, 0.52f);
    // antenna
    g.cylinderAxis(g.toWorld(V3(0.1f, 0, 0.55f)), g.toWorld(V3(0.35f, 0.1f, 0.95f)), 0.006f, 4);
    g.cylinderAxis(g.toWorld(V3(-0.1f, 0, 0.55f)), g.toWorld(V3(-0.30f, 0.1f, 0.95f)), 0.006f, 4);
    g.clearFrame();
}

void radio(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    g.mat = MAT_RADIO;
    g.uvScale = 0.5f;
    g.lbox(-0.16f, -0.10f, 0.0f, 0.16f, 0.10f, 0.22f);
    g.mat = MAT_METAL_STEEL;
    g.lbox(-0.02f, 0.10f, 0.0f, 0.02f, 0.13f, 0.22f);
    g.clearFrame();
}

void clock(GeoBuilder& g, const V3& pos, const V3& normal) {
    painting(g, pos, normal, 0.42f, 0.42f, MAT_CLOCK);
}

void plantPot(GeoBuilder& g, const V3& base, float scale) {
    g.setFrame(base, 0);
    g.mat = MAT_CERAMIC;
    g.uvScale = 0.4f;
    g.lcyl(0, 0, 0, 0.14f * scale, 0.24f * scale, 12, true);
    g.mat = MAT_PAINT_GREEN;
    g.uvScale = 0.3f;
    for (int i = 0; i < 5; ++i) {
        float a = i * 1.2566f;
        V3 b = g.toWorld(V3(std::cos(a) * 0.05f, std::sin(a) * 0.05f, 0.22f * scale));
        V3 tip = g.toWorld(V3(std::cos(a) * 0.25f * scale, std::sin(a) * 0.25f * scale, 0.62f * scale));
        g.cylinderAxis(b, tip, 0.012f, 4);
        g.sphere(tip, 0.10f * scale, 6, 4);
    }
    g.clearFrame();
}

void suitcase(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    g.mat = MAT_WOOD_DARK;
    g.uvScale = 0.5f;
    g.lbox(-0.22f, -0.13f, 0.0f, 0.22f, 0.13f, 0.12f);
    g.lbox(-0.22f, -0.13f, 0.24f, 0.22f, 0.13f, 0.36f);
    g.mat = MAT_METAL_STEEL;
    g.lbox(-0.06f, -0.02f, 0.35f, 0.06f, 0.02f, 0.40f);
    g.clearFrame();
}

void toyBlocks(GeoBuilder& g, const V3& base) {
    g.setFrame(base, 0);
    const nf::Color cols[4] = {0xFFB03A2E, 0xFF2E86C1, 0xFFD4AC0D, 0xFF28B463};
    for (int i = 0; i < 7; ++i) {
        g.mat = MAT_WOOD_DARK;
        float x = ((i * 37) % 11) * 0.055f - 0.25f;
        float y = ((i * 53) % 7) * 0.06f - 0.18f;
        float s = 0.08f;
        g.lbox(x, y, (i % 3) * 0.08f, x + s, y + s, (i % 3) * 0.08f + s);
    }
    (void)cols;
    g.clearFrame();
}

void shoeRack(GeoBuilder& g, const V3& base, float yaw) {
    g.setFrame(base, yaw);
    g.mat = MAT_WOOD_DARK;
    g.uvScale = 0.6f;
    g.lbox(-0.45f, -0.16f, 0.0f, 0.45f, 0.16f, 0.06f);
    g.lbox(-0.45f, -0.16f, 0.28f, 0.45f, 0.16f, 0.32f);
    g.lbox(-0.45f, 0.12f, 0.0f, -0.41f, 0.16f, 0.55f);
    g.lbox(0.41f, 0.12f, 0.0f, 0.45f, 0.16f, 0.55f);
    g.mat = MAT_COAT_BLACK;
    for (int i = 0; i < 3; ++i) g.lbox(-0.36f + i * 0.26f, -0.10f, 0.06f, -0.16f + i * 0.26f, 0.08f, 0.16f);
    g.clearFrame();
}

void coatHooks(GeoBuilder& g, const V3& pos, const V3& normal) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    g.mat = MAT_WOOD_DARK;
    g.uvScale = 0.5f;
    g.lbox(-0.32f, 0.0f, -0.03f, 0.32f, 0.03f, 0.06f);
    g.mat = MAT_METAL_STEEL;
    for (int i = 0; i < 3; ++i) {
        float x = -0.24f + i * 0.24f;
        g.cylinderAxis(g.toWorld(V3(x, 0.03f, 0.06f)), g.toWorld(V3(x, 0.12f, 0.0f)), 0.012f, 5);
    }
    g.mat = MAT_COAT_BLACK;
    g.lbox(-0.15f, 0.06f, -1.15f, 0.10f, 0.14f, 0.06f);      // hanging coat
    g.clearFrame();
}

void fireExtinguisher(GeoBuilder& g, const V3& base) {
    g.setFrame(base, 0);
    g.mat = MAT_BLOOD;
    g.uvScale = 0.4f;
    g.lcyl(0, 0, 0.05f, 0.09f, 0.52f, 12, true);
    g.mat = MAT_METAL_STEEL;
    g.lcyl(0, 0, 0.57f, 0.03f, 0.06f, 8, true);
    g.lbox(-0.02f, -0.10f, 0.60f, 0.02f, 0.02f, 0.64f);
    g.clearFrame();
}

void mopBucket(GeoBuilder& g, const V3& base) {
    g.setFrame(base, 0);
    g.mat = MAT_METAL_RUST;
    g.uvScale = 0.5f;
    g.lcyl(0, 0, 0, 0.16f, 0.38f, 12, true);
    g.mat = MAT_WOOD_LIGHT;
    g.cylinderAxis(g.toWorld(V3(0.10f, 0, 0.30f)), g.toWorld(V3(0.30f, 0.10f, 1.30f)), 0.016f, 6);
    g.clearFrame();
}

void drainPipe(GeoBuilder& g, const V3& base, float h) {
    g.mat = MAT_METAL_RUST;
    g.uvScale = 1.0f;
    g.cylinder(base, 0.09f, h, 10);
    g.cylinder(g.toWorld(V3(0, 0, 0.0f)) , 0.11f, 0.06f, 10);
    g.mat = MAT_METAL_STEEL;
    for (int i = 1; i < 4; ++i) {
        g.cylinder(base + V3(0, 0, i * h / 4), 0.115f, 0.05f, 10);
    }
}

void wallCrackDecal(GeoBuilder& g, const V3& pos, const V3& normal, float s) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    g.mat = MAT_GRAYBOX;
    g.uvScale = 1.0f;
    // a thin dark fracture drawn as a zigzag of narrow quads
    V3 n = normal;
    float pts[8][2] = {{0, 0}, {0.05f, -0.10f}, {0.02f, -0.22f}, {0.10f, -0.33f},
                       {-0.04f, -0.45f}, {0.03f, -0.58f}, {-0.02f, -0.70f}, {0.06f, -0.82f}};
    for (int i = 0; i + 1 < 8; ++i) {
        V3 a = pos + V3(0, 0, 0) + nf::V3(n.y, -n.x, 0) * (pts[i][0] * s) + V3(0, 0, pts[i][1] * s);
        V3 b = pos + nf::V3(n.y, -n.x, 0) * (pts[i + 1][0] * s) + V3(0, 0, pts[i + 1][1] * s);
        g.cylinderAxis(a + n * 0.006f, b + n * 0.006f, 0.006f * s, 4);
    }
    g.clearFrame();
}

void bloodDecal(GeoBuilder& g, const V3& pos, const V3& normal, float s) {
    float yaw = std::atan2(normal.y, normal.x);
    g.setFrame(pos, yaw);
    g.mat = MAT_BLOOD;
    g.uvScale = 1.0f;
    g.lbox(-0.10f * s, 0.0f, -0.02f * s, 0.10f * s, 0.004f, 0.02f * s);
    for (int i = 0; i < 5; ++i) {
        float x = -0.30f * s + i * 0.15f * s;
        g.lbox(x, 0.0f, 0.0f, x + 0.04f * s, 0.003f, -0.16f * s);
    }
    g.clearFrame();
}

} // namespace props
} // namespace dl
