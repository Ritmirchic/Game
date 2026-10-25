// Procedural mesh primitives
#include "render.h"

static Vec3 ortho_basis(Vec3 n, Vec3 *t) {
    Vec3 up = fabsf(n.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    *t = vnorm(vcross(up, n));
    return vnorm(vcross(n, *t));
}

void mesh_cyl_axis(Mesh *m, Vec3 a, Vec3 b, float r0, float r1, int segs, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale, int caps) {
    if (segs < 3) segs = 3;
    Vec3 axis = vsub(b, a);
    float len = vlen(axis);
    if (len < 1e-6f) return;
    Vec3 dir = vscale(axis, 1.f / len);
    Vec3 t, u;
    ortho_basis(dir, &t);
    u = vnorm(vcross(dir, t));
    Vec3 prev0 = v3(0, 0, 0), prev1 = v3(0, 0, 0);
    Vec3 prev_n0 = v3(0, 0, 0), prev_n1 = v3(0, 0, 0);
    float slope = (r0 - r1) / maxf(len, 1e-4f);
    float circum = LN2PI * maxf(r0, r1);
    for (int i = 0; i <= segs; i++) {
        float ang = (float)i / (float)segs * LN2PI;
        Vec3 radial = vadd(vscale(t, cosf(ang)), vscale(u, sinf(ang)));
        Vec3 p0 = vadd(a, vscale(radial, r0));
        Vec3 p1 = vadd(b, vscale(radial, r1));
        Vec3 n0 = vnorm(vadd(radial, vscale(dir, slope)));
        if (i > 0) {
            float u0 = (float)(i - 1) / (float)segs * circum * uvscale;
            float u1 = (float)i / (float)segs * circum * uvscale;
            float v1 = len * uvscale;
            mesh_add_tri(m, prev0, p0, p1, prev_n0, n0, n0, u0, 0, u1, 0, u1, v1, lr, lg, lb);
            mesh_add_tri(m, prev0, p1, prev1, prev_n0, n0, prev_n1, u0, 0, u1, v1, u0, v1, lr, lg, lb);
        }
        prev0 = p0; prev1 = p1; prev_n0 = n0; prev_n1 = n0;
    }
    if (caps) {
        for (int c = 0; c < 2; c++) {
            Vec3 ctr = c == 0 ? a : b;
            float r = c == 0 ? r0 : r1;
            Vec3 n = c == 0 ? vneg(dir) : dir;
            if (r < 1e-5f) continue;
            for (int i = 0; i < segs; i++) {
                float a0 = (float)i / (float)segs * LN2PI;
                float a1 = (float)(i + 1) / (float)segs * LN2PI;
                Vec3 d0 = vadd(vscale(t, cosf(a0)), vscale(u, sinf(a0)));
                Vec3 d1 = vadd(vscale(t, cosf(a1)), vscale(u, sinf(a1)));
                Vec3 p0 = vadd(ctr, vscale(d0, r));
                Vec3 p1 = vadd(ctr, vscale(d1, r));
                mesh_add_tri(m, ctr, p0, p1, n, n, n, 0.5f, 0.5f, 0.5f + d0.x * 0.5f, 0.5f + d0.z * 0.5f,
                             0.5f + d1.x * 0.5f, 0.5f + d1.z * 0.5f, lr, lg, lb);
            }
        }
    }
}
void mesh_cyl(Mesh *m, Vec3 center, float radius, float height, int segs, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale, int caps) {
    mesh_cyl_axis(m, v3(center.x, center.y - height * 0.5f, center.z),
                  v3(center.x, center.y + height * 0.5f, center.z), radius, radius, segs, lr, lg, lb, uvscale, caps);
}
void mesh_sphere(Mesh *m, Vec3 center, float radius, int segs, int rings, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale) {
    mesh_ellipsoid(m, center, v3s(radius), segs, rings, lr, lg, lb, uvscale);
}
void mesh_ellipsoid(Mesh *m, Vec3 center, Vec3 radii, int segs, int rings, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale) {
    if (segs < 3) segs = 3;
    if (rings < 2) rings = 2;
    for (int r = 0; r < rings; r++) {
        float v0 = (float)r / (float)rings, v1 = (float)(r + 1) / (float)rings;
        float th0 = v0 * (float)M_PI, th1 = v1 * (float)M_PI;
        for (int s = 0; s < segs; s++) {
            float u0 = (float)s / (float)segs, u1 = (float)(s + 1) / (float)segs;
            float ph0 = u0 * LN2PI, ph1 = u1 * LN2PI;
            Vec3 p[4];
            float uu[4] = {u0, u1, u1, u0};
            float vv[4] = {v0, v0, v1, v1};
            float th[4] = {th0, th0, th1, th1};
            float ph[4] = {ph0, ph1, ph1, ph0};
            for (int k = 0; k < 4; k++) {
                p[k] = v3(center.x + radii.x * sinf(th[k]) * cosf(ph[k]),
                          center.y + radii.y * cosf(th[k]),
                          center.z + radii.z * sinf(th[k]) * sinf(ph[k]));
            }
            Vec3 n[4];
            for (int k = 0; k < 4; k++) {
                n[k] = vnorm(v3((p[k].x - center.x) / (radii.x * radii.x), (p[k].y - center.y) / (radii.y * radii.y),
                                (p[k].z - center.z) / (radii.z * radii.z)));
            }
            float us = (float)LN2PI * maxf(radii.x, radii.z) * uvscale;
            float vs = (float)M_PI * radii.y * uvscale;
            mesh_add_tri(m, p[0], p[1], p[2], n[0], n[1], n[2], uu[0] * us, vv[0] * vs, uu[1] * us, vv[1] * vs, uu[2] * us, vv[2] * vs, lr, lg, lb);
            mesh_add_tri(m, p[0], p[2], p[3], n[0], n[2], n[3], uu[0] * us, vv[0] * vs, uu[2] * us, vv[2] * vs, uu[3] * us, vv[3] * vs, lr, lg, lb);
        }
    }
}
void mesh_torus(Mesh *m, Vec3 center, float major, float minor, int segs, int sides, uint8_t lr, uint8_t lg, uint8_t lb) {
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * LN2PI, a1 = (float)(i + 1) / segs * LN2PI;
        for (int j = 0; j < sides; j++) {
            float b0 = (float)j / sides * LN2PI, b1 = (float)(j + 1) / sides * LN2PI;
            Vec3 p[4];
            float ang[2] = {a0, a1}, sid[2] = {b0, b1};
            int idx = 0;
            for (int ia = 0; ia < 2; ia++)
                for (int ib = 0; ib < 2; ib++) {
                    float ax = ang[ia], bx = sid[ib];
                    float rr = major + minor * cosf(bx);
                    p[idx++] = v3(center.x + rr * cosf(ax), center.y + minor * sinf(bx), center.z + rr * sinf(ax));
                }
            Vec3 n0 = vnorm(vsub(p[0], v3(center.x + major * cosf(a0), center.y, center.z + major * sinf(a0))));
            Vec3 n1 = vnorm(vsub(p[1], v3(center.x + major * cosf(a0), center.y, center.z + major * sinf(a0))));
            Vec3 n2 = vnorm(vsub(p[3], v3(center.x + major * cosf(a1), center.y, center.z + major * sinf(a1))));
            Vec3 n3 = vnorm(vsub(p[2], v3(center.x + major * cosf(a1), center.y, center.z + major * sinf(a1))));
            mesh_add_tri(m, p[0], p[1], p[2], n0, n1, n3, 0, 0, 1, 0, 1, 1, lr, lg, lb);
            mesh_add_tri(m, p[0], p[2], p[3], n0, n3, n2, 0, 0, 1, 1, 0, 1, lr, lg, lb);
        }
    }
}
void mesh_ring(Mesh *m, Vec3 center, float r_in, float r_out, int segs, uint8_t lr, uint8_t lg, uint8_t lb) {
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * LN2PI, a1 = (float)(i + 1) / segs * LN2PI;
        Vec3 i0 = v3(center.x + cosf(a0) * r_in, center.y, center.z + sinf(a0) * r_in);
        Vec3 i1 = v3(center.x + cosf(a1) * r_in, center.y, center.z + sinf(a1) * r_in);
        Vec3 o0 = v3(center.x + cosf(a0) * r_out, center.y, center.z + sinf(a0) * r_out);
        Vec3 o1 = v3(center.x + cosf(a1) * r_out, center.y, center.z + sinf(a1) * r_out);
        Vec3 n = v3(0, 1, 0);
        mesh_add_tri(m, i0, o0, o1, n, n, n, 0, 0, 1, 0, 1, 1, lr, lg, lb);
        mesh_add_tri(m, i0, o1, i1, n, n, n, 0, 0, 1, 1, 0, 1, lr, lg, lb);
    }
}
void mesh_plane(Mesh *m, Vec3 center, Vec3 right, Vec3 up, float w, float h, uint8_t lr, uint8_t lg, uint8_t lb) {
    Vec3 r = vscale(vnorm(right), w * 0.5f), u = vscale(vnorm(up), h * 0.5f);
    Vec3 p0 = vsub(vsub(center, r), u), p1 = vadd(vsub(center, r), u);
    Vec3 p2 = vadd(vadd(center, r), u), p3 = vadd(vsub(center, r), u);
    Vec3 n = vnorm(vcross(right, up));
    mesh_add_tri(m, p0, p1, p2, n, n, n, 0, 1, 0, 0, 1, 0, lr, lg, lb);
    mesh_add_tri(m, p0, p2, p3, n, n, n, 0, 1, 1, 0, 1, 1, lr, lg, lb);
}
void mesh_quad_uv(Mesh *m, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, float u0, float v0, float u1, float v1,
                  uint8_t lr, uint8_t lg, uint8_t lb) {
    mesh_add_tri(m, p0, p1, p2, n, n, n, u0, v1, u0, v0, u1, v0, lr, lg, lb);
    mesh_add_tri(m, p0, p2, p3, n, n, n, u0, v1, u1, v0, u1, v1, lr, lg, lb);
}
void mesh_capsule(Mesh *m, Vec3 a, Vec3 b, float radius, int segs, int rings, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale) {
    Vec3 axis = vsub(b, a);
    float len = vlen(axis);
    if (len < 1e-5f) { mesh_sphere(m, a, radius, segs, rings, lr, lg, lb, uvscale); return; }
    Vec3 dir = vscale(axis, 1.f / len);
    mesh_cyl_axis(m, a, b, radius, radius, segs, lr, lg, lb, uvscale, 0);
    mesh_sphere(m, a, radius, segs, rings / 2 + 1, lr, lg, lb, uvscale);
    mesh_sphere(m, b, radius, segs, rings / 2 + 1, lr, lg, lb, uvscale);
    (void)dir;
}
void mesh_bone_segment(Mesh *m, Vec3 a, Vec3 b, float r0, float r1, int segs, uint8_t lr, uint8_t lg, uint8_t lb) {
    mesh_cyl_axis(m, a, b, r0, r1, segs, lr, lg, lb, 1.6f, 1);
    mesh_sphere(m, a, r0 * 1.02f, segs, segs / 2 + 1, lr, lg, lb, 1.6f);
    mesh_sphere(m, b, r1 * 1.02f, segs, segs / 2 + 1, lr, lg, lb, 1.6f);
}
Mat4 xform_between(Vec3 a, Vec3 b) {
    Vec3 d = vsub(b, a);
    float len = vlen(d);
    if (len < 1e-6f) return mat4_translate(a);
    Vec3 dir = vscale(d, 1.f / len);
    // build rotation from +Y to dir
    Vec3 up = v3(0, 1, 0);
    float c = vdot(up, dir);
    Mat4 rot;
    if (c > 0.9999f) rot = mat4_id();
    else if (c < -0.9999f) rot = mat4_rot_z((float)M_PI);
    else {
        Vec3 axis = vnorm(vcross(up, dir));
        float ang = acosf(clampf(c, -1.f, 1.f));
        float s = sinf(ang), cc = cosf(ang), t = 1.f - cc;
        Mat4 r = mat4_id();
        r.m[0] = t * axis.x * axis.x + cc;
        r.m[1] = t * axis.x * axis.y - s * axis.z;
        r.m[2] = t * axis.x * axis.z + s * axis.y;
        r.m[4] = t * axis.x * axis.y + s * axis.z;
        r.m[5] = t * axis.y * axis.y + cc;
        r.m[6] = t * axis.y * axis.z - s * axis.x;
        r.m[8] = t * axis.x * axis.z - s * axis.y;
        r.m[9] = t * axis.y * axis.z + s * axis.x;
        r.m[10] = t * axis.z * axis.z + cc;
        rot = r;
    }
    return mat4_mul(mat4_translate(a), rot);
}
