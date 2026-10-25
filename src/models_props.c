// Procedural prop models. Every prop is built in metres, origin at the floor
// centre, front face toward +Z.
#include "models.h"
#include <string.h>

Model *model_new(void) {
    Model *m = (Model *)calloc(1, sizeof(Model));
    m->bbmin = v3s(1e30f);
    m->bbmax = v3s(-1e30f);
    return m;
}
Mesh *model_part(Model *m, int mat, int cap_hint) {
    if (m->nparts >= 12) return NULL;
    Mesh *mesh = mesh_new(cap_hint > 0 ? cap_hint : 256, cap_hint > 0 ? cap_hint * 2 : 512);
    mesh->mat = mat;
    mesh->dynamic = 1;
    m->part[m->nparts++] = mesh;
    return mesh;
}
void model_final(Model *m) {
    m->bbmin = v3s(1e30f);
    m->bbmax = v3s(-1e30f);
    for (int i = 0; i < m->nparts; i++) {
        Mesh *p = m->part[i];
        if (!p || p->vert_used == 0) continue;
        m->bbmin = vmin3(m->bbmin, p->bbmin);
        m->bbmax = vmax3(m->bbmax, p->bbmax);
    }
    if (m->nparts == 0) { m->bbmin = v3s(-0.5f); m->bbmax = v3s(0.5f); }
    Vec3 c = vscale(vadd(m->bbmin, m->bbmax), 0.5f);
    m->radius = vlen(vsub(m->bbmax, m->bbmin)) * 0.5f;
    m->height = m->bbmax.y - m->bbmin.y;
    (void)c;
}
void model_draw(Renderer *r, const Model *m, const Material *mats, Mat4 xf) {
    if (!m) return;
    for (int i = 0; i < m->nparts; i++) {
        if (m->part[i] && m->part[i]->tri_used) rd_draw_mesh(r, m->part[i], &xf, mats, MAT_COUNT);
    }
}
void model_free(Model *m) {
    if (!m) return;
    for (int i = 0; i < m->nparts; i++) mesh_free(m->part[i]);
    free(m);
}

// helper: add a box with baked-light defaults (dynamic props get lit at draw time)
static void box(Mesh *ms, Vec3 mn, Vec3 mx, float uv) { mesh_add_boxf(ms, mn, mx, 255, 255, 255, uv); }
static void boxm(Mesh *ms, Vec3 c, Vec3 hs, float uv) { mesh_add_boxf(ms, vsub(c, hs), vadd(c, hs), 255, 255, 255, uv); }

// ------------------------------------------------------------------ shelves
Model *prop_shelf(int units, int two_sided, int fill) {
    Model *m = model_new();
    Mesh *frame = model_part(m, MAT_STEEL, 4000);
    Mesh *goods = fill ? model_part(m, MAT_SHELF_GOODS, 3000) : NULL;
    float w = 1.25f * units;            // along X
    float depth = two_sided ? 1.10f : 0.62f;
    float h = 2.05f;
    float legR = 0.035f;
    int posts = units + 1;
    for (int i = 0; i < posts; i++) {
        float x = -w * 0.5f + (float)i * (w / units);
        for (int s = 0; s < 2; s++) {
            float z = (s == 0 ? -1.f : 1.f) * (depth * 0.5f - 0.04f);
            if (!two_sided && s == 1) continue;
            mesh_cyl(frame, v3(x, h * 0.5f, z), legR, h, 6, 255, 255, 255, 2.f, 0);
        }
    }
    // shelves
    int nshelf = 5;
    for (int s = 0; s < nshelf; s++) {
        float y = 0.14f + (float)s * (h - 0.2f) / (float)(nshelf - 1);
        box(frame, v3(-w * 0.5f, y - 0.018f, -depth * 0.5f), v3(w * 0.5f, y + 0.018f, depth * 0.5f), 1.6f);
        if (two_sided) {
            // centre divider
            box(frame, v3(-0.02f, y, -depth * 0.5f), v3(0.02f, y + 0.32f, depth * 0.5f), 1.4f);
        }
        if (goods) {
            float gh = 0.30f;
            for (int side = 0; side < (two_sided ? 2 : 1); side++) {
                float z0 = side == 0 ? -depth * 0.5f + 0.03f : 0.03f;
                float z1 = side == 0 ? -0.03f : depth * 0.5f - 0.03f;
                float zc = (z0 + z1) * 0.5f;
                // product block: front face on the aisle side
                float zfc = side == 0 ? z0 : z1;
                float sign = side == 0 ? -1.f : 1.f;
                if (s == nshelf - 1) break;
                // several cartons along the shelf
                int n = units * 2;
                for (int i = 0; i < n; i++) {
                    float x0 = -w * 0.5f + (float)i * (w / (float)n) + 0.02f;
                    float x1 = x0 + (w / (float)n) - 0.04f;
                    float hh = 0.20f + 0.10f * (((i * 37 + s * 13) % 5) / 5.f);
                    Vec3 p0 = v3(x0, y + 0.02f, zfc);
                    Vec3 p1 = v3(x1, y + 0.02f, zfc + sign * 0.28f);
                    Vec3 p2 = v3(x1, y + 0.02f + hh, zfc + sign * 0.28f);
                    Vec3 p3 = v3(x0, y + 0.02f + hh, zfc);
                    Vec3 nrm = v3(0, 0, sign);
                    mesh_quad_uv(goods, p0, p1, p2, p3, nrm, 0, 1, 1, 0, 255, 255, 255);
                    // top + side faces so it reads as a solid box from above
                    mesh_add_quad(goods, p3, p2, v3(p2.x, p2.y, zc), v3(p3.x, p3.y, zc), v3(0, 1, 0), 1, 0.3f, 190, 190, 190);
                    mesh_add_quad(goods, p3, v3(p3.x, p3.y, zc), v3(p0.x, p0.y, zc), p0, v3(-1, 0, 0), 0.3f, 1, 170, 170, 170);
                }
            }
        }
    }
    // price rail
    for (int s = 0; s < nshelf - 1; s++) {
        float y = 0.14f + (float)s * (h - 0.2f) / (float)(nshelf - 1);
        box(frame, v3(-w * 0.5f, y - 0.05f, -depth * 0.5f - 0.01f), v3(w * 0.5f, y + 0.01f, -depth * 0.5f + 0.005f), 1.2f);
        if (two_sided)
            box(frame, v3(-w * 0.5f, y - 0.05f, depth * 0.5f - 0.005f), v3(w * 0.5f, y + 0.01f, depth * 0.5f + 0.01f), 1.2f);
    }
    model_final(m);
    return m;
}

Model *prop_shelf_broken(void) {
    Model *m = prop_shelf(2, 0, 1);
    // tilt parts: rotate a copy of geometry downwards for a collapsed look
    for (int i = 0; i < m->nparts; i++) {
        Mesh *p = m->part[i];
        for (uint32_t v = 0; v < p->vert_used; v++) {
            Vec3 x = p->pos[v];
            if (x.y > 1.0f) {
                p->pos[v] = v3(x.x + (x.y - 1.0f) * 0.55f, x.y - (x.y - 1.0f) * 0.35f, x.z + (x.y - 1.0f) * 0.2f);
            }
        }
    }
    Mesh *debris = model_part(m, MAT_PLYWOOD, 400);
    box(debris, v3(-0.9f, 0.0f, -0.5f), v3(-0.2f, 0.10f, 0.4f), 2.f);
    box(debris, v3(0.3f, 0.0f, -0.2f), v3(1.0f, 0.07f, 0.5f), 2.f);
    model_final(m);
    return m;
}

Model *prop_register_stand(void) {
    Model *m = model_new();
    Mesh *frame = model_part(m, MAT_STEEL, 400);
    Mesh *screen = model_part(m, MAT_SCREEN, 120);
    box(frame, v3(-0.3f, 0.0f, -0.3f), v3(0.3f, 0.9f, 0.3f), 2.f);
    box(frame, v3(-0.22f, 0.9f, -0.2f), v3(0.22f, 1.0f, 0.25f), 2.f);
    Vec3 p0 = v3(-0.2f, 1.02f, -0.02f), p1 = v3(0.2f, 1.02f, -0.02f);
    Vec3 p2 = v3(0.2f, 1.30f, -0.12f), p3 = v3(-0.2f, 1.30f, -0.12f);
    mesh_quad_uv(screen, p0, p1, p2, p3, v3(0, 0.3f, -1.f), 0, 1, 1, 0, 255, 255, 255);
    model_final(m);
    return m;
}

Model *prop_checkout(void) {
    Model *m = model_new();
    Mesh *body = model_part(m, MAT_PLYWOOD, 900);
    Mesh *steel = model_part(m, MAT_STEEL, 500);
    Mesh *belt = model_part(m, MAT_RUBBER, 200);
    Mesh *screen = model_part(m, MAT_SCREEN, 120);
    // counter run
    box(body, v3(-2.4f, 0.0f, -0.45f), v3(2.4f, 0.92f, 0.45f), 1.5f);
    box(steel, v3(-2.4f, 0.92f, -0.45f), v3(2.4f, 0.96f, -0.30f), 2.f);
    // belt
    box(belt, v3(-1.7f, 0.96f, -0.28f), v3(0.35f, 0.985f, 0.28f), 2.f);
    // bag area
    box(steel, v3(0.45f, 0.96f, -0.30f), v3(1.6f, 1.0f, 0.30f), 2.f);
    // register
    box(steel, v3(-1.9f, 0.96f, -0.20f), v3(-1.0f, 1.28f, 0.20f), 2.f);
    Vec3 p0 = v3(-1.85f, 1.30f, -0.05f), p1 = v3(-1.05f, 1.30f, -0.05f);
    Vec3 p2 = v3(-1.05f, 1.62f, -0.22f), p3 = v3(-1.85f, 1.62f, -0.22f);
    mesh_quad_uv(screen, p0, p1, p2, p3, v3(0, 0.25f, -1), 0, 1, 1, 0, 255, 255, 255);
    // divider pole
    mesh_cyl(steel, v3(2.2f, 1.3f, -0.35f), 0.03f, 1.6f, 6, 255, 255, 255, 2.f, 1);
    box(steel, v3(-2.45f, 1.7f, -0.42f), v3(-1.4f, 1.78f, -0.36f), 1.5f);   // overhead rail
    model_final(m);
    return m;
}

Model *prop_cart(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 900);
    Mesh *plastic = model_part(m, MAT_RUBBER, 200);
    // basket: nested rings
    float w = 0.58f, d = 0.95f, y0 = 0.42f, y1 = 0.95f;
    for (int i = 0; i < 9; i++) {
        float t = (float)i / 8.f;
        float y = y0 + (y1 - y0) * t;
        float sc = 1.f - 0.10f * (1.f - t);
        mesh_cyl_axis(steel, v3(-w * sc, y, 0), v3(w * sc, y, 0), 0.012f, 0.012f, 4, 255, 255, 255, 3.f, 0);
    }
    // wire basket as vertical bars + horizontal rails
    for (int i = 0; i < 12; i++) {
        float t = (float)i / 11.f;
        float x = -w + 2.f * w * t;
        mesh_cyl_axis(steel, v3(x, y0, d * 0.5f), v3(x * 0.9f, y1, d * 0.45f), 0.010f, 0.010f, 4, 255, 255, 255, 3.f, 0);
        mesh_cyl_axis(steel, v3(x, y0, -d * 0.5f), v3(x * 0.9f, y1, -d * 0.45f), 0.010f, 0.010f, 4, 255, 255, 255, 3.f, 0);
    }
    for (int i = 0; i < 6; i++) {
        float t = (float)i / 5.f;
        float z = -d * 0.5f + d * t;
        mesh_cyl_axis(steel, v3(-w, y0, z), v3(-w * 0.9f, y1, z * 0.9f), 0.010f, 0.010f, 4, 255, 255, 255, 3.f, 0);
        mesh_cyl_axis(steel, v3(w, y0, z), v3(w * 0.9f, y1, z * 0.9f), 0.010f, 0.010f, 4, 255, 255, 255, 3.f, 0);
    }
    // handle
    mesh_cyl_axis(steel, v3(-w, y1, -d * 0.45f), v3(-w * 0.9f, y1 + 0.06f, -d * 0.62f), 0.018f, 0.018f, 6, 255, 255, 255, 3.f, 1);
    mesh_cyl_axis(steel, v3(w, y1, -d * 0.45f), v3(w * 0.9f, y1 + 0.06f, -d * 0.62f), 0.018f, 0.018f, 6, 255, 255, 255, 3.f, 1);
    mesh_cyl_axis(steel, v3(-w * 0.9f, y1 + 0.06f, -d * 0.62f), v3(w * 0.9f, y1 + 0.06f, -d * 0.62f), 0.02f, 0.02f, 6, 255, 255, 255, 3.f, 1);
    // under frame + wheels
    for (int i = 0; i < 2; i++) {
        float x = i ? w * 0.8f : -w * 0.8f;
        mesh_cyl_axis(steel, v3(x, 0.10f, -d * 0.45f), v3(x, y0, 0), 0.015f, 0.015f, 4, 255, 255, 255, 3.f, 0);
        mesh_cyl_axis(steel, v3(x, 0.10f, d * 0.45f), v3(x, y0, 0), 0.015f, 0.015f, 4, 255, 255, 255, 3.f, 0);
        mesh_cyl(plastic, v3(x, 0.10f, -d * 0.45f), 0.055f, 0.04f, 8, 200, 200, 200, 2.f, 1);
        mesh_cyl(plastic, v3(x, 0.10f, d * 0.45f), 0.055f, 0.04f, 8, 200, 200, 200, 2.f, 1);
    }
    mesh_cyl_axis(steel, v3(-w * 0.8f, 0.1f, d * 0.2f), v3(w * 0.8f, 0.1f, d * 0.2f), 0.012f, 0.012f, 4, 255, 255, 255, 3.f, 0);
    model_final(m);
    return m;
}

Model *prop_cart_nest(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 2500);
    for (int n = 0; n < 5; n++) {
        float z = -0.55f * n;
        float w = 0.58f, d = 0.95f, y0 = 0.42f, y1 = 0.95f;
        for (int i = 0; i < 5; i++) {
            float t = (float)i / 4.f;
            float y = y0 + (y1 - y0) * t;
            float sc = 1.f - 0.10f * (1.f - t);
            mesh_cyl_axis(steel, v3(-w * sc, y, z), v3(w * sc, y, z), 0.012f, 0.012f, 4, 255, 255, 255, 3.f, 0);
        }
        for (int i = 0; i < 8; i++) {
            float t = (float)i / 7.f;
            float x = -w + 2.f * w * t;
            mesh_cyl_axis(steel, v3(x, y0, z + d * 0.5f), v3(x * 0.9f, y1, z + d * 0.45f), 0.010f, 0.010f, 4, 255, 255, 255, 3.f, 0);
        }
        mesh_cyl_axis(steel, v3(-w * 0.8f, 0.1f, z - d * 0.45f), v3(-w * 0.8f, y0, z), 0.015f, 0.015f, 4, 255, 255, 255, 3.f, 0);
        mesh_cyl_axis(steel, v3(w * 0.8f, 0.1f, z - d * 0.45f), v3(w * 0.8f, y0, z), 0.015f, 0.015f, 4, 255, 255, 255, 3.f, 0);
    }
    model_final(m);
    return m;
}

Model *prop_freezer(int len) {
    Model *m = model_new();
    Mesh *frame = model_part(m, MAT_STEEL, 1200);
    Mesh *glass = model_part(m, MAT_VEH_GLASS, 200);
    Mesh *goods = model_part(m, MAT_FREEZER_GOODS, 2400);
    float L = 1.9f * (float)len;
    float d = 0.95f, h = 0.85f;
    box(frame, v3(-L * 0.5f, 0.0f, -d * 0.5f), v3(L * 0.5f, h, d * 0.5f), 2.f);
    // goods inside the top (visible through glass)
    for (int i = 0; i < len * 3; i++) {
        float x = -L * 0.5f + 0.3f + (float)i * (L - 0.6f) / (float)(len * 3);
        Vec3 p0 = v3(x, h - 0.02f, d * 0.5f - 0.06f);
        Vec3 p1 = v3(x + 0.22f, h - 0.02f, d * 0.5f - 0.06f);
        Vec3 p2 = v3(x + 0.22f, h, d * 0.5f - 0.06f);
        Vec3 p3 = v3(x, h, d * 0.5f - 0.06f);
        (void)p0; (void)p1; (void)p2; (void)p3;
    }
    // slanted glass lids
    Vec3 g0 = v3(-L * 0.5f, h + 0.02f, -d * 0.5f + 0.05f);
    Vec3 g1 = v3(L * 0.5f, h + 0.02f, -d * 0.5f + 0.05f);
    Vec3 g2 = v3(L * 0.5f, h + 0.22f, d * 0.5f - 0.05f);
    Vec3 g3 = v3(-L * 0.5f, h + 0.22f, d * 0.5f - 0.05f);
    mesh_quad_uv(glass, g0, g1, g2, g3, v3(0, 0.9f, -0.4f), 0, 1, 3, 0, 220, 230, 240);
    mesh_quad_uv(glass, g3, g2, g1, g0, v3(0, -0.9f, 0.4f), 0, 1, 3, 0, 220, 230, 240);
    // side rails
    box(frame, v3(-L * 0.5f, h + 0.18f, d * 0.5f - 0.08f), v3(L * 0.5f, h + 0.24f, d * 0.5f - 0.02f), 2.f);
    box(frame, v3(-L * 0.5f, h + 0.18f, -d * 0.5f + 0.02f), v3(L * 0.5f, h + 0.24f, -d * 0.5f + 0.08f), 2.f);
    // base kick plate
    box(frame, v3(-L * 0.5f, 0.0f, -d * 0.5f), v3(L * 0.5f, 0.12f, d * 0.5f), 2.f);
    // goods visible through the glass front
    for (int i = 0; i < len * 4; i++) {
        float x = -L * 0.5f + 0.2f + (float)i * (L - 0.4f) / (float)(len * 4);
        Vec3 p0 = v3(x, h * 0.55f, d * 0.5f + 0.002f);
        Vec3 p1 = v3(x + 0.24f, h * 0.55f, d * 0.5f + 0.002f);
        Vec3 p2 = v3(x + 0.24f, h - 0.02f, d * 0.5f + 0.002f);
        Vec3 p3 = v3(x, h - 0.02f, d * 0.5f + 0.002f);
        mesh_quad_uv(goods, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, 0.25f, 0, 220, 230, 235);
    }
    model_final(m);
    return m;
}

Model *prop_fridge(int len) {
    Model *m = model_new();
    Mesh *frame = model_part(m, MAT_STEEL, 900);
    Mesh *glass = model_part(m, MAT_VEH_GLASS, 300);
    Mesh *goods = model_part(m, MAT_FREEZER_GOODS, 2400);
    float L = 1.2f * (float)len, d = 0.75f, h = 2.1f;
    box(frame, v3(-L * 0.5f, 0.0f, -d * 0.5f), v3(L * 0.5f, h, d * 0.5f), 2.f);
    // interior shelves
    for (int s = 0; s < 5; s++) {
        float y = 0.3f + (float)s * 0.42f;
        box(frame, v3(-L * 0.5f + 0.06f, y, -d * 0.5f + 0.02f), v3(L * 0.5f - 0.06f, y + 0.02f, d * 0.5f - 0.04f), 2.f);
        for (int i = 0; i < len * 5; i++) {
            float t = (float)i / (float)(len * 5);
            float x = -L * 0.5f + 0.10f + t * (L - 0.2f);
            float zf = d * 0.5f - 0.03f;
            Vec3 p0 = v3(x, y + 0.02f, zf);
            Vec3 p1 = v3(x + 0.16f, y + 0.02f, zf);
            Vec3 p2 = v3(x + 0.16f, y + 0.30f, zf);
            Vec3 p3 = v3(x, y + 0.30f, zf);
            mesh_quad_uv(goods, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, 0.2f, 0, 210, 220, 230);
        }
    }
    // glass doors
    Vec3 z0 = v3(-L * 0.5f + 0.04f, 0.28f, d * 0.5f + 0.01f);
    Vec3 z1 = v3(L * 0.5f - 0.04f, 0.28f, d * 0.5f + 0.01f);
    Vec3 z2 = v3(L * 0.5f - 0.04f, h - 0.08f, d * 0.5f + 0.01f);
    Vec3 z3 = v3(-L * 0.5f + 0.04f, h - 0.08f, d * 0.5f + 0.01f);
    mesh_quad_uv(glass, z0, z1, z2, z3, v3(0, 0, 1), 0, 1, 2, 0, 200, 215, 225);
    // door frames
    mesh_cyl(frame, v3(0, (0.28f + h - 0.08f) * 0.5f, d * 0.5f + 0.02f), 0.02f, h - 0.36f, 6, 255, 255, 255, 2.f, 1);
    model_final(m);
    return m;
}

Model *prop_pallet(int with_boxes) {
    Model *m = model_new();
    Mesh *wood = model_part(m, MAT_PLYWOOD, 400);
    float w = 1.2f, d = 1.0f;
    for (int i = 0; i < 3; i++) {
        float z = -d * 0.5f + 0.18f + (float)i * (d - 0.36f) * 0.5f;
        box(wood, v3(-w * 0.5f, 0.02f, z - 0.06f), v3(w * 0.5f, 0.14f, z + 0.06f), 1.5f);
    }
    for (int i = 0; i < 6; i++) {
        float z = -d * 0.5f + (float)i * (d / 5.f);
        box(wood, v3(-w * 0.5f, 0.14f, z - 0.045f), v3(w * 0.5f, 0.20f, z + 0.045f), 1.5f);
    }
    box(wood, v3(-w * 0.5f, 0.0f, -d * 0.5f), v3(w * 0.5f, 0.02f, d * 0.5f), 1.5f);
    if (with_boxes) {
        Mesh *card = model_part(m, MAT_CARDBOARD, 600);
        box(card, v3(-0.55f, 0.20f, -0.42f), v3(0.05f, 0.75f, 0.42f), 1.2f);
        box(card, v3(0.08f, 0.20f, -0.42f), v3(0.55f, 0.62f, 0.05f), 1.2f);
        box(card, v3(0.08f, 0.62f, -0.42f), v3(0.55f, 0.90f, 0.05f), 1.2f);
    }
    model_final(m);
    return m;
}

Model *prop_boxes(int count) {
    Model *m = model_new();
    Mesh *card = model_part(m, MAT_CARDBOARD, 900);
    Rng rng;
    rng_seed(&rng, (uint64_t)count * 13u + 7);
    float x = -0.5f;
    float y = 0.f;
    for (int i = 0; i < count; i++) {
        float w = rng_range(&rng, 0.35f, 0.7f);
        float h = rng_range(&rng, 0.25f, 0.55f);
        float d = rng_range(&rng, 0.35f, 0.6f);
        float px = x + w * 0.5f, pz = rng_range(&rng, -0.3f, 0.3f);
        box(card, v3(px - w * 0.5f, y, pz - d * 0.5f), v3(px + w * 0.5f, y + h, pz + d * 0.5f), 1.2f);
        x += w * 0.85f;
        if (x > 0.9f) { x = -0.5f; y += 0.0f; }
        if (i == count / 2) { y = 0.f; }
    }
    model_final(m);
    return m;
}

Model *prop_crate_stack(void) {
    Model *m = model_new();
    Mesh *card = model_part(m, MAT_CARDBOARD, 500);
    box(card, v3(-0.55f, 0.0f, -0.55f), v3(0.0f, 0.6f, 0.0f), 1.2f);
    box(card, v3(0.02f, 0.0f, -0.55f), v3(0.55f, 0.5f, 0.02f), 1.2f);
    box(card, v3(-0.5f, 0.6f, -0.5f), v3(-0.05f, 1.05f, -0.05f), 1.2f);
    box(card, v3(0.0f, 0.0f, 0.05f), v3(0.6f, 0.45f, 0.55f), 1.2f);
    model_final(m);
    return m;
}

Model *prop_barrel(int rusty) {
    Model *m = model_new();
    Mesh *steel = model_part(m, rusty ? MAT_WALL_RUST : MAT_STEEL, 500);
    mesh_cyl(steel, v3(0, 0.44f, 0), 0.29f, 0.88f, 16, 255, 255, 255, 1.6f, 1);
    mesh_torus(steel, v3(0, 0.30f, 0), 0.29f, 0.02f, 16, 6, 255, 255, 255);
    mesh_torus(steel, v3(0, 0.58f, 0), 0.29f, 0.02f, 16, 6, 255, 255, 255);
    mesh_cyl(steel, v3(0.12f, 0.90f, 0), 0.06f, 0.04f, 8, 255, 255, 255, 2.f, 1);
    model_final(m);
    return m;
}

Model *prop_table(void) {
    Model *m = model_new();
    Mesh *wood = model_part(m, MAT_PLYWOOD, 400);
    box(wood, v3(-0.7f, 0.70f, -0.4f), v3(0.7f, 0.76f, 0.4f), 1.5f);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? 0.62f : -0.62f;
        float z = (i & 2) ? 0.32f : -0.32f;
        box(wood, v3(x - 0.03f, 0.0f, z - 0.03f), v3(x + 0.03f, 0.70f, z + 0.03f), 2.f);
    }
    model_final(m);
    return m;
}

Model *prop_chair(void) {
    Model *m = model_new();
    Mesh *plastic = model_part(m, MAT_RUBBER, 300);
    box(plastic, v3(-0.22f, 0.42f, -0.22f), v3(0.22f, 0.47f, 0.22f), 2.f);
    box(plastic, v3(-0.22f, 0.47f, -0.24f), v3(0.22f, 0.92f, -0.18f), 2.f);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? 0.18f : -0.18f;
        float z = (i & 2) ? 0.18f : -0.18f;
        box(plastic, v3(x - 0.02f, 0.0f, z - 0.02f), v3(x + 0.02f, 0.42f, z + 0.02f), 2.f);
    }
    model_final(m);
    return m;
}

Model *prop_locker(int count) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 500 * count);
    float w = 0.42f;
    for (int i = 0; i < count; i++) {
        float x0 = -w * count * 0.5f + i * w;
        box(steel, v3(x0 + 0.01f, 0.0f, -0.25f), v3(x0 + w - 0.01f, 1.85f, 0.25f), 2.f);
        // door vents
        for (int v = 0; v < 4; v++)
            box(steel, v3(x0 + 0.06f, 1.35f + v * 0.07f, 0.24f), v3(x0 + w - 0.06f, 1.38f + v * 0.07f, 0.26f), 3.f);
        box(steel, v3(x0 + w - 0.09f, 1.0f, 0.25f), v3(x0 + w - 0.05f, 1.12f, 0.28f), 3.f);
    }
    model_final(m);
    return m;
}

Model *prop_vending(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 500);
    Mesh *goods = model_part(m, MAT_BOTTLES, 400);
    box(steel, v3(-0.5f, 0.0f, -0.4f), v3(0.5f, 1.85f, 0.35f), 2.f);
    Vec3 p0 = v3(-0.42f, 0.6f, 0.355f), p1 = v3(0.42f, 0.6f, 0.355f);
    Vec3 p2 = v3(0.42f, 1.75f, 0.355f), p3 = v3(-0.42f, 1.75f, 0.355f);
    mesh_quad_uv(goods, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, 1, 0, 255, 255, 255);
    box(steel, v3(-0.3f, 0.15f, 0.35f), v3(0.3f, 0.35f, 0.40f), 2.f);
    model_final(m);
    return m;
}

Model *prop_atm(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 400);
    Mesh *scr = model_part(m, MAT_SCREEN, 100);
    box(steel, v3(-0.45f, 0.0f, -0.35f), v3(0.45f, 1.7f, 0.30f), 2.f);
    Vec3 p0 = v3(-0.3f, 1.1f, 0.31f), p1 = v3(0.3f, 1.1f, 0.31f);
    Vec3 p2 = v3(0.3f, 1.45f, 0.31f), p3 = v3(-0.3f, 1.45f, 0.31f);
    mesh_quad_uv(scr, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, 1, 0, 255, 255, 255);
    box(steel, v3(-0.25f, 0.95f, 0.30f), v3(0.25f, 1.05f, 0.36f), 3.f);
    model_final(m);
    return m;
}

Model *prop_dumpster(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_WALL_RUST, 700);
    box(steel, v3(-0.9f, 0.12f, -0.6f), v3(0.9f, 1.15f, 0.6f), 1.6f);
    // lid (slanted)
    Vec3 a = v3(-0.92f, 1.15f, -0.62f), b = v3(0.92f, 1.15f, -0.62f);
    Vec3 c = v3(0.92f, 1.30f, 0.5f), d = v3(-0.92f, 1.30f, 0.5f);
    mesh_quad_uv(steel, a, b, c, d, v3(0, 1, -0.2f), 0, 0, 2, 1, 255, 255, 255);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? 0.7f : -0.7f;
        float z = (i & 2) ? 0.42f : -0.42f;
        mesh_cyl(steel, v3(x, 0.06f, z), 0.06f, 0.12f, 8, 255, 255, 255, 2.f, 1);
    }
    model_final(m);
    return m;
}

Model *prop_streetlight(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_WALL_RUST, 600);
    Mesh *lamp = model_part(m, MAT_EMISSIVE_WARM, 100);
    mesh_cyl(steel, v3(0, 3.4f, 0), 0.09f, 6.8f, 10, 255, 255, 255, 1.2f, 1);
    mesh_cyl(steel, v3(0, 0.15f, 0), 0.18f, 0.3f, 10, 255, 255, 255, 2.f, 1);
    // arm
    mesh_cyl_axis(steel, v3(0, 6.7f, 0), v3(0, 6.9f, 0.9f), 0.05f, 0.05f, 6, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(steel, v3(0, 6.9f, 0.9f), v3(0, 6.85f, 1.6f), 0.05f, 0.05f, 6, 255, 255, 255, 2.f, 1);
    // head
    box(steel, v3(-0.16f, 6.72f, 1.45f), v3(0.16f, 6.86f, 1.85f), 2.f);
    Vec3 p0 = v3(-0.14f, 6.71f, 1.47f), p1 = v3(0.14f, 6.71f, 1.47f);
    Vec3 p2 = v3(0.14f, 6.71f, 1.83f), p3 = v3(-0.14f, 6.71f, 1.83f);
    mesh_quad_uv(lamp, p0, p1, p2, p3, v3(0, -1, 0), 0, 0, 1, 1, 255, 255, 255);
    model_final(m);
    return m;
}

Model *prop_light_pole(int on) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 300);
    Mesh *lamp = model_part(m, on ? MAT_EMISSIVE_COOL : MAT_STEEL, 80);
    mesh_cyl(steel, v3(0, 0.8f, 0), 0.05f, 1.6f, 8, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(steel, v3(0, 1.6f, 0), v3(0, 1.75f, 0.5f), 0.04f, 0.04f, 6, 255, 255, 255, 2.f, 1);
    box(steel, v3(-0.12f, 1.6f, 0.4f), v3(0.12f, 1.75f, 0.72f), 2.f);
    Vec3 p0 = v3(-0.1f, 1.59f, 0.42f), p1 = v3(0.1f, 1.59f, 0.42f);
    Vec3 p2 = v3(0.1f, 1.59f, 0.7f), p3 = v3(-0.1f, 1.59f, 0.7f);
    mesh_quad_uv(lamp, p0, p1, p2, p3, v3(0, -1, 0), 0, 0, 1, 1, 255, 255, 255);
    model_final(m);
    return m;
}

Model *prop_fence_segment(float len) {
    Model *m = model_new();
    Mesh *mesh = model_part(m, MAT_CHAINLINK, 500);
    Mesh *post = model_part(m, MAT_FENCE, 400);
    mesh_cyl(post, v3(-len * 0.5f, 1.1f, 0), 0.035f, 2.2f, 8, 255, 255, 255, 2.f, 1);
    mesh_cyl(post, v3(len * 0.5f, 1.1f, 0), 0.035f, 2.2f, 8, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(post, v3(-len * 0.5f, 2.18f, 0), v3(len * 0.5f, 2.18f, 0), 0.025f, 0.025f, 6, 255, 255, 255, 2.f, 1);
    // chain link plane (alpha tested)
    Vec3 p0 = v3(-len * 0.5f, 0.05f, 0), p1 = v3(len * 0.5f, 0.05f, 0), p2 = v3(len * 0.5f, 2.2f, 0), p3 = v3(-len * 0.5f, 2.2f, 0);
    mesh_quad_uv(mesh, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, len * 1.0f, 0, 255, 255, 255);
    model_final(m);
    return m;
}

// ------------------------------------------------------------------ vehicles
static void add_wheel(Mesh *ms, Vec3 c, float r, float w) {
    mesh_cyl_axis(ms, v3(c.x, c.y, c.z - w * 0.5f), v3(c.x, c.y, c.z + w * 0.5f), r, r, 14, 255, 255, 255, 1.2f, 1);
}
Model *prop_car(int variant) {
    Model *m = model_new();
    int paint = variant == 0 ? MAT_CAR_RED : variant == 1 ? MAT_CAR_BLUE : MAT_CAR_WHITE;
    Mesh *body = model_part(m, paint, 1600);
    Mesh *dark = model_part(m, MAT_RUBBER, 600);
    Mesh *glass = model_part(m, MAT_VEH_GLASS, 300);
    float L = 4.3f, W = 1.8f;
    // lower body
    box(body, v3(-W * 0.5f, 0.32f, -L * 0.5f), v3(W * 0.5f, 0.78f, L * 0.5f), 1.2f);
    // hood / trunk
    box(body, v3(-W * 0.47f, 0.78f, -L * 0.5f), v3(W * 0.47f, 0.86f, -L * 0.16f), 1.2f);
    box(body, v3(-W * 0.47f, 0.78f, L * 0.22f), v3(W * 0.47f, 0.92f, L * 0.5f), 1.2f);
    // cabin
    box(body, v3(-W * 0.44f, 0.78f, -L * 0.18f), v3(W * 0.44f, 1.36f, L * 0.26f), 1.2f);
    // roof edge trim
    box(dark, v3(-W * 0.45f, 1.30f, -L * 0.19f), v3(W * 0.45f, 1.37f, L * 0.27f), 2.f);
    // windscreen + rear + sides
    Vec3 w0 = v3(-W * 0.40f, 0.86f, -L * 0.175f), w1 = v3(W * 0.40f, 0.86f, -L * 0.175f);
    Vec3 w2 = v3(W * 0.38f, 1.30f, -L * 0.05f), w3 = v3(-W * 0.38f, 1.30f, -L * 0.05f);
    mesh_quad_uv(glass, w0, w1, w2, w3, v3(0, 0.4f, -1), 0, 1, 1, 0, 255, 255, 255);
    Vec3 r0 = v3(-W * 0.40f, 0.88f, L * 0.255f), r1 = v3(W * 0.40f, 0.88f, L * 0.255f);
    Vec3 r2 = v3(W * 0.38f, 1.28f, L * 0.12f), r3 = v3(-W * 0.38f, 1.28f, L * 0.12f);
    mesh_quad_uv(glass, r1, r0, r3, r2, v3(0, 0.4f, 1), 0, 1, 1, 0, 255, 255, 255);
    for (int s = -1; s <= 1; s += 2) {
        Vec3 a0 = v3(s * W * 0.44f, 0.88f, -L * 0.15f), a1 = v3(s * W * 0.44f, 0.88f, L * 0.20f);
        Vec3 a2 = v3(s * W * 0.40f, 1.28f, L * 0.12f), a3 = v3(s * W * 0.40f, 1.28f, -L * 0.06f);
        mesh_quad_uv(glass, a0, a1, a2, a3, v3((float)s, 0.3f, 0), 0, 1, 1.6f, 0, 255, 255, 255);
    }
    // wheels
    float wr = 0.33f;
    add_wheel(dark, v3(-W * 0.5f - 0.02f, wr, -L * 0.30f), wr, 0.22f);
    add_wheel(dark, v3(W * 0.5f + 0.02f, wr, -L * 0.30f), wr, 0.22f);
    add_wheel(dark, v3(-W * 0.5f - 0.02f, wr, L * 0.30f), wr, 0.22f);
    add_wheel(dark, v3(W * 0.5f + 0.02f, wr, L * 0.30f), wr, 0.22f);
    // arches
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? W * 0.5f : -W * 0.5f;
        float z = (i & 2) ? L * 0.30f : -L * 0.30f;
        mesh_cyl_axis(dark, v3(x - 0.05f * signf(x), 0.42f, z), v3(x + 0.05f * signf(x), 0.42f, z), 0.42f, 0.42f, 12, 200, 200, 200, 1.f, 0);
    }
    // lights
    Mesh *lights = model_part(m, MAT_EMISSIVE_WARM, 100);
    box(lights, v3(-W * 0.44f, 0.62f, -L * 0.505f), v3(-W * 0.22f, 0.74f, -L * 0.48f), 2.f);
    box(lights, v3(W * 0.22f, 0.62f, -L * 0.505f), v3(W * 0.44f, 0.74f, -L * 0.48f), 2.f);
    mesh_cyl(dark, v3(-W * 0.3f, 0.55f, -L * 0.5f), 1.f, 0.02f, 4, 200, 200, 200, 1.f, 0);
    model_final(m);
    return m;
}

Model *prop_truck(int box_truck) {
    Model *m = model_new();
    Mesh *body = model_part(m, MAT_CAR_WHITE, 2200);
    Mesh *dark = model_part(m, MAT_RUBBER, 700);
    Mesh *glass = model_part(m, MAT_VEH_GLASS, 300);
    Mesh *cargo = model_part(m, MAT_WALL_METAL, 1200);
    float W = 2.4f, L = 8.5f;
    // cab
    box(body, v3(-W * 0.5f, 0.55f, -L * 0.5f), v3(W * 0.5f, 2.55f, -L * 0.5f + 2.2f), 1.2f);
    // windscreen
    Vec3 a0 = v3(-W * 0.45f, 1.65f, -L * 0.5f + 0.02f), a1 = v3(W * 0.45f, 1.65f, -L * 0.5f + 0.02f);
    Vec3 a2 = v3(W * 0.43f, 2.42f, -L * 0.5f + 0.10f), a3 = v3(-W * 0.43f, 2.42f, -L * 0.5f + 0.10f);
    mesh_quad_uv(glass, a0, a1, a2, a3, v3(0, 0.25f, -1), 0, 1, 1, 0, 255, 255, 255);
    for (int s = -1; s <= 1; s += 2) {
        Vec3 b0 = v3(s * W * 0.5f, 1.6f, -L * 0.5f + 0.2f), b1 = v3(s * W * 0.5f, 1.6f, -L * 0.5f + 1.6f);
        Vec3 b2 = v3(s * W * 0.5f, 2.35f, -L * 0.5f + 1.6f), b3 = v3(s * W * 0.5f, 2.35f, -L * 0.5f + 0.2f);
        mesh_quad_uv(glass, b0, b1, b2, b3, v3((float)s, 0, 0), 0, 1, 1, 0, 255, 255, 255);
    }
    // cargo box
    if (box_truck) {
        box(cargo, v3(-W * 0.52f, 0.9f, -L * 0.5f + 2.3f), v3(W * 0.52f, 3.1f, L * 0.5f), 1.4f);
        // rear doors
        box(cargo, v3(-W * 0.5f, 0.9f, L * 0.5f - 0.02f), v3(-0.03f, 3.0f, L * 0.5f + 0.02f), 1.6f);
        box(cargo, v3(0.03f, 0.9f, L * 0.5f - 0.02f), v3(W * 0.5f, 3.0f, L * 0.5f + 0.02f), 1.6f);
    }
    // chassis + wheels
    box(dark, v3(-W * 0.45f, 0.42f, -L * 0.5f), v3(W * 0.45f, 0.6f, L * 0.5f), 1.5f);
    float wr = 0.48f;
    float zs[3] = {-L * 0.34f, L * 0.20f, L * 0.34f};
    for (int i = 0; i < 3; i++)
        for (int s = -1; s <= 1; s += 2) {
            add_wheel(dark, v3(s * (W * 0.5f - 0.05f), wr, zs[i]), wr, 0.3f);
        }
    Mesh *lights = model_part(m, MAT_EMISSIVE_WARM, 100);
    box(lights, v3(-W * 0.45f, 1.0f, -L * 0.505f), v3(-W * 0.25f, 1.2f, -L * 0.49f), 2.f);
    box(lights, v3(W * 0.25f, 1.0f, -L * 0.505f), v3(W * 0.45f, 1.2f, -L * 0.49f), 2.f);
    model_final(m);
    return m;
}

// ------------------------------------------------------------------ doors / barricades
Model *prop_door(int kind) {
    Model *m = model_new();
    Mesh *wood = model_part(m, MAT_WOOD, 700);
    Mesh *steel = model_part(m, MAT_STEEL, 700);
    Mesh *glass = model_part(m, MAT_VEH_GLASS, 200);
    Mesh *sign = model_part(m, MAT_SIGN_EXIT, 60);
    if (kind == 0) {  // wooden staff door with a window
        box(wood, v3(-0.46f, 0.0f, -0.04f), v3(0.46f, 2.05f, 0.04f), 1.4f);
        box(steel, v3(0.30f, 0.95f, 0.04f), v3(0.40f, 1.05f, 0.12f), 3.f);
        Vec3 p0 = v3(-0.3f, 1.45f, 0.045f), p1 = v3(0.3f, 1.45f, 0.045f);
        Vec3 p2 = v3(0.3f, 1.85f, 0.045f), p3 = v3(-0.3f, 1.85f, 0.045f);
        mesh_quad_uv(glass, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, 0.6f, 0, 255, 255, 255);
        mesh_quad_uv(sign, v3(-0.2f, 2.15f, 0.05f), v3(0.2f, 2.15f, 0.05f), v3(0.2f, 2.35f, 0.05f), v3(-0.2f, 2.35f, 0.05f),
                     v3(0, 0, 1), 0, 0, 1, 1, 255, 255, 255);
    } else if (kind == 1) {  // glass entrance double door (one leaf)
        box(steel, v3(-0.02f, 0.0f, -0.05f), v3(0.02f, 2.4f, 0.05f), 2.f);
        box(steel, v3(-0.66f, 0.0f, -0.05f), v3(0.66f, 0.12f, 0.05f), 2.f);
        box(steel, v3(-0.66f, 2.3f, -0.05f), v3(0.66f, 2.42f, 0.05f), 2.f);
        Vec3 p0 = v3(-0.62f, 0.14f, 0.0f), p1 = v3(0.62f, 0.14f, 0.0f);
        Vec3 p2 = v3(0.62f, 2.28f, 0.0f), p3 = v3(-0.62f, 2.28f, 0.0f);
        mesh_quad_uv(glass, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, 1, 0, 230, 240, 245);
        box(steel, v3(0.5f, 0.95f, -0.06f), v3(0.56f, 1.35f, 0.06f), 3.f);
    } else if (kind == 2) {  // roll-up loading door (open, slats at top)
        for (int i = 0; i < 6; i++) {
            box(steel, v3(-1.6f, 2.4f + i * 0.10f, -0.08f), v3(1.6f, 2.5f + i * 0.10f, 0.08f), 2.f);
        }
        box(steel, v3(-1.7f, 0.0f, -0.10f), v3(-1.6f, 3.0f, 0.10f), 2.f);
        box(steel, v3(1.6f, 0.0f, -0.10f), v3(1.7f, 3.0f, 0.10f), 2.f);
    } else {  // fire exit: metal door, push bar
        box(steel, v3(-0.46f, 0.0f, -0.05f), v3(0.46f, 2.05f, 0.05f), 1.6f);
        box(steel, v3(-0.4f, 0.9f, 0.05f), v3(0.4f, 1.0f, 0.10f), 3.f);
        mesh_quad_uv(sign, v3(-0.25f, 2.15f, 0.05f), v3(0.25f, 2.15f, 0.05f), v3(0.25f, 2.4f, 0.05f), v3(-0.25f, 2.4f, 0.05f),
                     v3(0, 0, 1), 0, 0, 1, 1, 255, 255, 255);
    }
    model_final(m);
    return m;
}

Model *prop_barricade(int level) {
    Model *m = model_new();
    Mesh *wood = model_part(m, MAT_WOOD, 900);
    Mesh *steel = model_part(m, MAT_DIAMOND, 700);
    Mesh *plank = model_part(m, MAT_PLYWOOD, 600);
    float h = 1.35f;
    float w = 1.0f;
    int boards = level >= 2 ? 5 : 4;
    for (int b = 0; b < boards; b++) {
        float y = 0.12f + (float)b * (h - 0.2f) / (float)(boards - 1);
        float tilt = (b & 1) ? 0.02f : -0.02f;
        box(wood, v3(-w * 0.5f, y - 0.09f, -0.04f + tilt), v3(w * 0.5f, y + 0.09f, 0.04f + tilt), 1.4f);
    }
    // uprights
    for (int s = -1; s <= 1; s += 2) {
        box(wood, v3(s * w * 0.42f - 0.05f, 0.0f, -0.09f), v3(s * w * 0.42f + 0.05f, h + 0.05f, 0.09f), 1.8f);
    }
    if (level >= 2) {
        // diagonal braces + metal plates
        for (int s = -1; s <= 1; s += 2) {
            Vec3 a = v3(s * 0.42f, 0.05f, 0.10f);
            Vec3 b = v3(-s * 0.42f, h, 0.10f);
            Vec3 mid = vscale(vadd(a, b), 0.5f);
            float len = vdist(a, b);
            Mat4 rot = xform_between(a, b);
            Mesh *tmp = mesh_new(64, 64);
            mesh_add_boxf(tmp, v3(-0.04f, 0.0f, -0.02f), v3(0.04f, len, 0.02f), 255, 255, 255, 1.5f);
            // Re-base: xform_between gives rotation mapping +Y to the segment
            mesh_apply_xform(tmp, rot);
            // append into plank mesh
            for (uint32_t i = 0; i < tmp->tri_used; i++) {
                uint32_t *ix = &tmp->idx[i * 3];
                mesh_add_tri(plank, tmp->pos[ix[0]], tmp->pos[ix[1]], tmp->pos[ix[2]],
                             tmp->nrm[ix[0]], tmp->nrm[ix[1]], tmp->nrm[ix[2]],
                             tmp->uv[ix[0] * 2], tmp->uv[ix[0] * 2 + 1],
                             tmp->uv[ix[1] * 2], tmp->uv[ix[1] * 2 + 1],
                             tmp->uv[ix[2] * 2], tmp->uv[ix[2] * 2 + 1], 255, 255, 255);
            }
            mesh_free(tmp);
            (void)mid;
        }
        // sheet metal facing
        box(plank, v3(-w * 0.5f, 0.05f, 0.09f), v3(w * 0.5f, 0.55f, 0.11f), 1.2f);
    }
    if (level >= 3) {
        // full metal frame + bars
        box(steel, v3(-w * 0.52f, 0.0f, -0.12f), v3(w * 0.52f, 0.08f, 0.12f), 1.8f);
        for (int i = 0; i < 5; i++) {
            float x = -w * 0.45f + (float)i * (w * 0.9f) / 4.f;
            mesh_cyl_axis(steel, v3(x, 0.05f, 0.0f), v3(x, h + 0.25f, 0.0f), 0.035f, 0.035f, 8, 255, 255, 255, 2.f, 1);
        }
        box(steel, v3(-w * 0.52f, h + 0.15f, -0.08f), v3(w * 0.52f, h + 0.28f, 0.08f), 2.f);
        box(steel, v3(-w * 0.52f, 0.6f, -0.06f), v3(w * 0.52f, 0.72f, 0.06f), 2.f);
    }
    model_final(m);
    return m;
}

Model *prop_trap(int kind) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 800);
    Mesh *wood = model_part(m, MAT_WOOD, 400);
    if (kind == 0) {
        // spike board
        box(wood, v3(-0.7f, 0.0f, -0.35f), v3(0.7f, 0.10f, 0.35f), 1.5f);
        Rng rng;
        rng_seed(&rng, 77);
        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 3; z++) {
                float px = -0.56f + (float)x * 0.28f + rng_range(&rng, -0.04f, 0.04f);
                float pz = -0.2f + (float)z * 0.2f;
                mesh_cyl_axis(steel, v3(px, 0.10f, pz), v3(px, 0.55f, pz), 0.022f, 0.004f, 6, 255, 255, 255, 2.f, 1);
            }
    } else {
        // barbed wire coil
        for (int i = 0; i < 3; i++) {
            mesh_torus(steel, v3(0, 0.16f + i * 0.22f, 0), 0.38f - i * 0.02f, 0.015f, 18, 5, 255, 255, 255);
        }
        box(steel, v3(-0.42f, 0.0f, -0.08f), v3(0.42f, 0.05f, 0.08f), 2.f);
    }
    model_final(m);
    return m;
}

Model *prop_debris(int seed) {
    Model *m = model_new();
    Mesh *wood = model_part(m, MAT_PLYWOOD, 600);
    Mesh *steel = model_part(m, MAT_STEEL, 400);
    Mesh *card = model_part(m, MAT_CARDBOARD, 400);
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 6151u + 11);
    for (int i = 0; i < 7; i++) {
        float x = rng_range(&rng, -1.2f, 1.2f);
        float z = rng_range(&rng, -1.0f, 1.0f);
        float w = rng_range(&rng, 0.1f, 0.55f);
        float h = rng_range(&rng, 0.03f, 0.12f);
        float d = rng_range(&rng, 0.1f, 0.45f);
        float yaw = rng_range(&rng, 0, 6.28f);
        Mesh *target = (i % 3 == 0) ? wood : (i % 3 == 1) ? card : steel;
        Mat4 xf = mat4_mul(mat4_translate(v3(x, h * 0.5f, z)), mat4_rot_y(yaw));
        Mesh *tmp = mesh_new(48, 48);
        mesh_add_boxf(tmp, v3(-w * 0.5f, -h, -d * 0.5f), v3(w * 0.5f, h, d * 0.5f), 255, 255, 255, 2.f);
        mesh_apply_xform(tmp, xf);
        for (uint32_t t = 0; t < tmp->tri_used; t++) {
            uint32_t *ix = &tmp->idx[t * 3];
            mesh_add_tri(target, tmp->pos[ix[0]], tmp->pos[ix[1]], tmp->pos[ix[2]],
                         tmp->nrm[ix[0]], tmp->nrm[ix[1]], tmp->nrm[ix[2]],
                         tmp->uv[ix[0] * 2], tmp->uv[ix[0] * 2 + 1],
                         tmp->uv[ix[1] * 2], tmp->uv[ix[1] * 2 + 1],
                         tmp->uv[ix[2] * 2], tmp->uv[ix[2] * 2 + 1], 255, 255, 255);
        }
        mesh_free(tmp);
    }
    model_final(m);
    return m;
}

Model *prop_generator(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_STEEL, 900);
    Mesh *rust = model_part(m, MAT_WALL_RUST, 400);
    Mesh *panel = model_part(m, MAT_SCREEN, 120);
    box(rust, v3(-1.0f, 0.0f, -0.6f), v3(1.0f, 0.25f, 0.6f), 1.6f);
    box(steel, v3(-0.9f, 0.25f, -0.5f), v3(0.9f, 0.95f, 0.5f), 1.4f);
    mesh_cyl(steel, v3(-0.55f, 1.05f, 0), 0.18f, 0.35f, 12, 255, 255, 255, 1.6f, 1);
    mesh_cyl(steel, v3(0.3f, 1.15f, 0), 0.12f, 0.6f, 10, 255, 255, 255, 1.6f, 1);
    box(rust, v3(0.45f, 0.35f, 0.5f), v3(0.85f, 0.85f, 0.56f), 2.f);
    Vec3 p0 = v3(-0.3f, 1.0f, 0.5f), p1 = v3(0.1f, 1.0f, 0.5f);
    Vec3 p2 = v3(0.1f, 1.25f, 0.5f), p3 = v3(-0.3f, 1.25f, 0.5f);
    mesh_quad_uv(panel, p0, p1, p2, p3, v3(0, 0, 1), 0, 1, 1, 0, 255, 255, 255);
    for (int i = 0; i < 4; i++) {
        box(steel, v3(-0.95f, 0.3f + i * 0.15f, 0.5f), v3(-0.55f, 0.38f + i * 0.15f, 0.54f), 3.f);
    }
    model_final(m);
    return m;
}

Model *prop_ceiling_light(int lit) {
    Model *m = model_new();
    Mesh *frame = model_part(m, MAT_STEEL, 200);
    Mesh *tube = model_part(m, lit ? MAT_EMISSIVE_COOL : MAT_LAMPGLASS, 200);
    box(frame, v3(-0.62f, -0.06f, -0.16f), v3(0.62f, 0.0f, 0.16f), 2.f);
    for (int i = 0; i < 2; i++) {
        float z = i ? 0.075f : -0.075f;
        mesh_cyl_axis(tube, v3(-0.58f, -0.09f, z), v3(0.58f, -0.09f, z), 0.028f, 0.028f, 8, 255, 255, 255, 2.f, 1);
    }
    model_final(m);
    return m;
}

Model *prop_wall_sign(int mat, float w, float h) {
    Model *m = model_new();
    Mesh *s = model_part(m, mat, 40);
    mesh_quad_uv(s, v3(-w * 0.5f, 0, 0), v3(w * 0.5f, 0, 0), v3(w * 0.5f, h, 0), v3(-w * 0.5f, h, 0),
                 v3(0, 0, 1), 0, 1, 1, 0, 255, 255, 255);
    mesh_quad_uv(s, v3(w * 0.5f, 0, 0), v3(-w * 0.5f, 0, 0), v3(-w * 0.5f, h, 0), v3(w * 0.5f, h, 0),
                 v3(0, 0, -1), 0, 1, 1, 0, 255, 255, 255);
    model_final(m);
    return m;
}

Model *prop_pipe_run(float len) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_WALL_RUST, 600);
    mesh_cyl_axis(steel, v3(-len * 0.5f, 0.0f, 0), v3(len * 0.5f, 0.0f, 0), 0.08f, 0.08f, 10, 255, 255, 255, 1.2f, 1);
    for (int i = 0; i < 4; i++) {
        float x = -len * 0.5f + len * (float)i / 3.f;
        mesh_cyl_axis(steel, v3(x, -0.02f, 0), v3(x, 0.02f, 0), 0.11f, 0.11f, 10, 200, 200, 200, 2.f, 1);
    }
    model_final(m);
    return m;
}

Model *prop_mannequin(void) {
    Model *m = model_new();
    Mesh *plastic = model_part(m, MAT_STEEL, 500);
    mesh_cyl(plastic, v3(0, 0.05f, 0), 0.22f, 0.1f, 10, 255, 255, 255, 2.f, 1);
    mesh_cyl(plastic, v3(0, 0.75f, 0), 0.05f, 1.4f, 8, 255, 255, 255, 2.f, 1);
    mesh_ellipsoid(plastic, v3(0, 1.25f, 0), v3(0.18f, 0.28f, 0.12f), 10, 6, 255, 255, 255, 1.4f);
    mesh_sphere(plastic, v3(0, 1.62f, 0), 0.10f, 10, 6, 255, 255, 255, 1.6f);
    mesh_cyl_axis(plastic, v3(-0.16f, 1.42f, 0), v3(-0.30f, 1.05f, 0.05f), 0.04f, 0.035f, 8, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(plastic, v3(0.16f, 1.42f, 0), v3(0.30f, 1.05f, -0.05f), 0.04f, 0.035f, 8, 255, 255, 255, 2.f, 1);
    model_final(m);
    return m;
}

Model *prop_pallet_jack(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_WALL_RUST, 700);
    box(steel, v3(-0.28f, 0.06f, -0.55f), v3(0.28f, 0.12f, 0.55f), 1.6f);
    mesh_cyl_axis(steel, v3(-0.28f, 0.10f, -0.6f), v3(-0.28f, 0.10f, 0.6f), 0.04f, 0.04f, 6, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(steel, v3(0.28f, 0.10f, -0.6f), v3(0.28f, 0.10f, 0.6f), 0.04f, 0.04f, 6, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(steel, v3(0, 0.12f, -0.55f), v3(0, 1.15f, -0.62f), 0.05f, 0.05f, 8, 255, 255, 255, 2.f, 1);
    box(steel, v3(-0.22f, 0.95f, -0.75f), v3(0.22f, 1.02f, -0.1f), 2.f);
    mesh_cyl(steel, v3(0.25f, 0.13f, 0.1f), 0.09f, 0.06f, 10, 255, 255, 255, 2.f, 1);
    model_final(m);
    return m;
}

Model *prop_wet_sign(void) {
    Model *m = model_new();
    Mesh *yellow = model_part(m, MAT_SIGN_WARN, 200);
    mesh_quad_uv(yellow, v3(-0.16f, 0.02f, -0.11f), v3(0.16f, 0.02f, -0.11f), v3(0.16f, 0.62f, -0.02f), v3(-0.16f, 0.62f, -0.02f),
                 v3(0, 0.2f, -1), 0, 1, 0.5f, 0, 255, 255, 255);
    mesh_quad_uv(yellow, v3(0.16f, 0.02f, 0.11f), v3(-0.16f, 0.02f, 0.11f), v3(-0.16f, 0.62f, 0.02f), v3(0.16f, 0.62f, 0.02f),
                 v3(0, 0.2f, 1), 0, 1, 0.5f, 0, 255, 255, 255);
    model_final(m);
    return m;
}

Model *prop_extinguisher(void) {
    Model *m = model_new();
    Mesh *red = model_part(m, MAT_CAR_RED, 300);
    Mesh *steel = model_part(m, MAT_STEEL, 100);
    mesh_cyl(red, v3(0, 0.32f, 0), 0.09f, 0.62f, 12, 255, 255, 255, 1.6f, 1);
    mesh_cyl(steel, v3(0, 0.66f, 0), 0.04f, 0.10f, 8, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(steel, v3(0, 0.70f, 0), v3(0.12f, 0.74f, 0.06f), 0.02f, 0.02f, 6, 255, 255, 255, 2.f, 1);
    model_final(m);
    return m;
}

Model *prop_fire_barrel(int lit) {
    Model *m = prop_barrel(1);
    if (lit) {
        Mesh *fire = model_part(m, MAT_EMISSIVE_WARM, 100);
        Vec3 p0 = v3(-0.2f, 0.86f, -0.2f), p1 = v3(0.2f, 0.86f, -0.2f);
        Vec3 p2 = v3(0.2f, 0.9f, 0.2f), p3 = v3(-0.2f, 0.9f, 0.2f);
        mesh_quad_uv(fire, p0, p1, p2, p3, v3(0, 1, 0), 0, 0, 1, 1, 255, 255, 255);
    }
    model_final(m);
    return m;
}

Model *prop_radio_tower(void) {
    Model *m = model_new();
    Mesh *steel = model_part(m, MAT_WALL_RUST, 900);
    for (int l = 0; l < 3; l++) {
        float y = l * 2.4f;
        float r = 0.9f - l * 0.22f;
        for (int i = 0; i < 3; i++) {
            float a0 = (float)i / 3.f * LN2PI, a1 = (float)(i + 1) / 3.f * LN2PI;
            Vec3 p0 = v3(cosf(a0) * r, y, sinf(a0) * r);
            Vec3 p1 = v3(cosf(a1) * r, y, sinf(a1) * r);
            Vec3 p2 = v3(cosf(a1) * (r - 0.2f), y + 2.4f, sinf(a1) * (r - 0.2f));
            Vec3 p3 = v3(cosf(a0) * (r - 0.2f), y + 2.4f, sinf(a0) * (r - 0.2f));
            mesh_cyl_axis(steel, p0, p2, 0.04f, 0.04f, 5, 255, 255, 255, 2.f, 1);
            mesh_cyl_axis(steel, p1, p2, 0.04f, 0.04f, 5, 255, 255, 255, 2.f, 1);
            mesh_cyl_axis(steel, p0, p1, 0.03f, 0.03f, 5, 255, 255, 255, 2.f, 1);
            mesh_cyl_axis(steel, p3, p2, 0.03f, 0.03f, 5, 255, 255, 255, 2.f, 1);
        }
    }
    mesh_cyl(steel, v3(0, 7.4f, 0), 0.03f, 0.6f, 6, 255, 255, 255, 2.f, 1);
    model_final(m);
    return m;
}

Model *prop_boss_core(void) {
    Model *m = model_new();
    Mesh *flesh = model_part(m, MAT_FLESH, 1200);
    Mesh *metal = model_part(m, MAT_STEEL, 700);
    Mesh *glow = model_part(m, MAT_EMISSIVE_RED, 400);
    mesh_ellipsoid(flesh, v3(0, 1.8f, 0), v3(0.9f, 1.1f, 0.8f), 14, 10, 255, 255, 255, 1.0f);
    for (int i = 0; i < 6; i++) {
        float a = i * LN2PI / 6.f;
        Vec3 dir = v3(cosf(a), 0.3f, sinf(a));
        mesh_cyl_axis(flesh, vadd(v3(0, 1.8f, 0), vscale(dir, 0.7f)), vadd(v3(0, 1.9f, 0), vscale(dir, 1.6f)),
                      0.12f, 0.03f, 6, 255, 255, 255, 1.2f, 1);
    }
    mesh_torus(metal, v3(0, 0.4f, 0), 0.7f, 0.12f, 16, 8, 255, 255, 255);
    mesh_sphere(glow, v3(0, 2.2f, 0), 0.28f, 12, 8, 255, 255, 255, 1.f);
    model_final(m);
    return m;
}

Model *prop_traffic_cone(void) {
    Model *m = model_new();
    Mesh *orange = model_part(m, MAT_SIGN_WARN, 200);
    box(orange, v3(-0.18f, 0.0f, -0.18f), v3(0.18f, 0.04f, 0.18f), 2.f);
    mesh_cyl_axis(orange, v3(0, 0.04f, 0), v3(0, 0.72f, 0), 0.14f, 0.02f, 10, 255, 255, 255, 1.4f, 1);
    model_final(m);
    return m;
}
