// ============================================================================
// THE LAST NIGHT — level construction: "SAVE-N-STOCK" supermarket + parking lot
// All geometry, props, collision, navigation and baked lighting are generated
// here from a hand-authored layout so the level is deterministic and testable.
// ============================================================================
#include "world.h"
#include "models.h"
#include "weapons.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Layout constants (metres). +X east, +Z south, +Y up.
// ---------------------------------------------------------------------------
#define STORE_X0 (-26.0f)
#define STORE_X1 (26.0f)
#define STORE_Z0 (-20.0f)
#define STORE_Z1 (20.0f)
#define WALL_H (4.4f)
#define WALL_T (0.30f)
#define BOH_Z0 (-20.0f)      // back-of-house north wall
#define BOH_Z1 (-14.0f)      // BOH corridor south edge
#define CORR_Z0 (-15.6f)
#define CORR_Z1 (-13.6f)

typedef struct Builder {
    Assets *as;
    Level *lv;
    Mesh *mesh[MAT_COUNT];
    Rng rng;
} Builder;

static Builder *g_b;

static Mesh *mesh_for(int mat) {
    if (mat < 0 || mat >= MAT_COUNT) mat = 0;
    if (!g_b->mesh[mat]) {
        Mesh *m = mesh_new(4096, 8192);
        m->mat = mat;
        m->dynamic = 0;
        g_b->mesh[mat] = m;
    }
    return g_b->mesh[mat];
}

static void add_aabb(Vec2 mn, Vec2 mx, float ymin, float ymax, int move, int light, int bullet, int mat) {
    if (g_b->lv->aabb_count >= 1400) return;
    AABB *a = &g_b->lv->aabbs[g_b->lv->aabb_count++];
    a->mn = mn; a->mx = mx; a->ymin = ymin; a->ymax = ymax;
    a->blocks_move = move; a->blocks_light = light; a->blocks_bullet = bullet; a->material = mat;
}
static void add_wallseg(Vec2 a, Vec2 b, float t, float y0, float y1) {
    if (g_b->lv->wall_count >= 220) return;
    WallSeg *w = &g_b->lv->walls[g_b->lv->wall_count++];
    w->a = a; w->b = b; w->thickness = t; w->y0 = y0; w->y1 = y1;
}

// textured quad in world space with metre UVs
static void quad(int mat, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, float u0, float v0, float u1, float v1) {
    Mesh *m = mesh_for(mat);
    mesh_quad_uv(m, p0, p1, p2, p3, n, u0, v0, u1, v1, 255, 255, 255);
}

// axis-aligned wall from (x0,z0) to (x1,z1) with thickness t, height y0..y1
static void wall_axis(int mat, float x0, float z0, float x1, float z1, float y0, float y1, float t) {
    float mnx = minf(x0, x1), mxx = maxf(x0, x1);
    float mnz = minf(z0, z1), mxz = maxf(z0, z1);
    int horizontal = (mxx - mnx) > (mxz - mnz);
    if (horizontal) {
        mnz -= t * 0.5f; mxz += t * 0.5f;
        float len = mxx - mnx, h = y1 - y0;
        // south face (+Z)
        quad(mat, v3(mnx, y0, mxz), v3(mxx, y0, mxz), v3(mxx, y1, mxz), v3(mnx, y1, mxz), v3(0, 0, 1), 0, 0, len, h);
        // north face
        quad(mat, v3(mxx, y0, mnz), v3(mnx, y0, mnz), v3(mnx, y1, mnz), v3(mxx, y1, mnz), v3(0, 0, -1), 0, 0, len, h);
        // top
        quad(mat, v3(mnx, y1, mnz), v3(mxx, y1, mnz), v3(mxx, y1, mxz), v3(mnx, y1, mxz), v3(0, 1, 0), 0, 0, len, t);
    } else {
        mnx -= t * 0.5f; mxx += t * 0.5f;
        float len = mxz - mnz, h = y1 - y0;
        quad(mat, v3(mxx, y0, mnz), v3(mxx, y0, mxz), v3(mxx, y1, mxz), v3(mxx, y1, mnz), v3(1, 0, 0), 0, 0, len, h);
        quad(mat, v3(mnx, y0, mxz), v3(mnx, y0, mnz), v3(mnx, y1, mnz), v3(mnx, y1, mxz), v3(-1, 0, 0), 0, 0, len, h);
        quad(mat, v3(mnx, y1, mnz), v3(mxx, y1, mnz), v3(mxx, y1, mxz), v3(mnx, y1, mxz), v3(0, 1, 0), 0, 0, t, len);
    }
    if (y0 < 1.9f && y1 > 0.2f) {
        add_aabb(v2(mnx - t * 0.5f, mnz - t * 0.5f), v2(mxx + t * 0.5f, mxz + t * 0.5f), y0, y1, 1, 1, 1, mat);
        add_wallseg(v2(mnx, mnz), v2(mxx, mxz), t, y0, y1);
    }
}

static void floor_rect(int mat, float x0, float z0, float x1, float z1, float y) {
    quad(mat, v3(x0, y, z1), v3(x1, y, z1), v3(x1, y, z0), v3(x0, y, z0), v3(0, 1, 0), 0, 0, x1 - x0, z1 - z0);
    // ceiling counterpart added separately
    (void)0;
}
static void ceil_rect(int mat, float x0, float z0, float x1, float z1, float y) {
    quad(mat, v3(x0, y, z0), v3(x1, y, z0), v3(x1, y, z1), v3(x0, y, z1), v3(0, -1, 0), 0, 0, x1 - x0, z1 - z0);
}

// prop placement: copies a model's mesh into the static per-material batches
static void place_model(Model *m, Vec3 pos, float yaw, float scale) {
    Mat4 xf = mat4_mul(mat4_translate(pos), mat4_mul(mat4_rot_y(yaw), mat4_scale(v3s(scale))));
    for (int i = 0; i < m->nparts; i++) {
        Mesh *src = m->part[i];
        if (!src || !src->tri_used) continue;
        Mesh *dst = mesh_for(src->mat);
        for (uint32_t t = 0; t < src->tri_used; t++) {
            uint32_t *ix = &src->idx[t * 3];
            Vec3 p[3], n[3];
            for (int k = 0; k < 3; k++) {
                p[k] = mat4_mul_point(xf, src->pos[ix[k]]);
                n[k] = vnorm(mat4_mul_dir(xf, src->nrm[ix[k]]));
            }
            mesh_add_tri(dst, p[0], p[1], p[2], n[0], n[1], n[2],
                         src->uv[ix[0] * 2], src->uv[ix[0] * 2 + 1],
                         src->uv[ix[1] * 2], src->uv[ix[1] * 2 + 1],
                         src->uv[ix[2] * 2], src->uv[ix[2] * 2 + 1], 255, 255, 255);
        }
    }
}
// places a one-off model and releases it: its triangles are copied into the
// static material batch, so the source model is not needed afterwards
static void place_model_take(Model *m, Vec3 pos, float yaw, float scale) {
    place_model(m, pos, yaw, scale);
    model_free(m);
}
static void place_model_nofree(Model *m, Vec3 pos, float yaw, float scale) {
    place_model(m, pos, yaw, scale);
}

// add an oriented box prop collider (approximated by its AABB)
static void collider_from_model(Model *m, Vec3 pos, float yaw, float scale, int move) {
    float r = m->radius * scale;
    (void)yaw;
    add_aabb(v2(pos.x - r * 0.8f, pos.z - r * 0.8f), v2(pos.x + r * 0.8f, pos.z + r * 0.8f),
             pos.y, pos.y + m->height * scale, move, 0, 1, MAT_NONE);
}

// ---------------------------------------------------------------------------
// Lights for baking
// ---------------------------------------------------------------------------
typedef struct BakeLight {
    Vec3 pos, color;
    float intensity, radius;
    int box;   // 0 = point, 1 = soft area (ceiling panel), 2 = window shaft
    Vec3 dir;
} BakeLight;
static BakeLight g_lights[128];
static int g_nlight = 0;

static void add_light(Vec3 pos, Vec3 color, float intensity, float radius) {
    if (g_nlight >= 128) return;
    g_lights[g_nlight].pos = pos;
    g_lights[g_nlight].color = color;
    g_lights[g_nlight].intensity = intensity;
    g_lights[g_nlight].radius = radius;
    g_lights[g_nlight].box = 0;
    g_lights[g_nlight].dir = v3(0, -1, 0);
    g_nlight++;
}

// ---------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------

static Model *g_m_shelf, *g_m_shelf2, *g_m_shelf_broken, *g_m_checkout, *g_m_cart, *g_m_cartnest,
    *g_m_freezer, *g_m_fridge, *g_m_pallet, *g_m_boxes, *g_m_barrel, *g_m_barrel_fire, *g_m_table,
    *g_m_chair, *g_m_locker, *g_m_vending, *g_m_atm, *g_m_dumpster, *g_m_streetlight, *g_m_fence,
    *g_m_car[3], *g_m_truck, *g_m_truck_box, *g_m_debris[3], *g_m_generator, *g_m_ceillight_lit,
    *g_m_ceillight_off, *g_m_sign_aisle, *g_m_sign_store, *g_m_sign_exit, *g_m_sign_warn,
    *g_m_pipe, *g_m_mannequin, *g_m_palletjack, *g_m_register, *g_m_wetsign, *g_m_extinguisher,
    *g_m_crate, *g_m_lightpole_on, *g_m_lightpole_off, *g_m_cone, *g_m_roof_ac;

static void build_models(void) {
    Assets *as = g_b->as;
    (void)as;
    g_m_shelf = prop_shelf(4, 1, 1);
    g_m_shelf2 = prop_shelf(3, 1, 1);
    g_m_shelf_broken = prop_shelf_broken();
    g_m_checkout = prop_checkout();
    g_m_cart = prop_cart();
    g_m_cartnest = prop_cart_nest();
    g_m_freezer = prop_freezer(2);
    g_m_fridge = prop_fridge(3);
    g_m_pallet = prop_pallet(1);
    g_m_boxes = prop_boxes(4);
    g_m_barrel = prop_barrel(0);
    g_m_barrel_fire = prop_fire_barrel(1);
    g_m_table = prop_table();
    g_m_chair = prop_chair();
    g_m_locker = prop_locker(4);
    g_m_vending = prop_vending();
    g_m_atm = prop_atm();
    g_m_dumpster = prop_dumpster();
    g_m_streetlight = prop_streetlight();
    g_m_fence = prop_fence_segment(4.0f);
    g_m_car[0] = prop_car(0);
    g_m_car[1] = prop_car(1);
    g_m_car[2] = prop_car(2);
    g_m_truck = prop_truck(0);
    g_m_truck_box = prop_truck(1);
    for (int i = 0; i < 3; i++) g_m_debris[i] = prop_debris(i + 1);
    g_m_generator = prop_generator();
    g_m_ceillight_lit = prop_ceiling_light(1);
    g_m_ceillight_off = prop_ceiling_light(0);
    g_m_sign_aisle = prop_wall_sign(MAT_SIGN_AISLE, 1.2f, 0.7f);
    g_m_sign_store = prop_wall_sign(MAT_SIGN_STORE, 9.0f, 2.4f);
    g_m_sign_exit = prop_wall_sign(MAT_SIGN_EXIT, 0.5f, 0.35f);
    g_m_sign_warn = prop_wall_sign(MAT_SIGN_WARN, 0.9f, 1.2f);
    g_m_pipe = prop_pipe_run(4.0f);
    g_m_mannequin = prop_mannequin();
    g_m_palletjack = prop_pallet_jack();
    g_m_register = prop_register_stand();
    g_m_wetsign = prop_wet_sign();
    g_m_extinguisher = prop_extinguisher();
    g_m_crate = prop_crate_stack();
    g_m_lightpole_on = prop_light_pole(1);
    g_m_lightpole_off = prop_light_pole(0);
    g_m_cone = prop_traffic_cone();
    g_m_roof_ac = prop_boxes(3);
}

// ---- the store shell -------------------------------------------------------
static void build_shell(void) {
    Level *lv = g_b->lv;
    // floors
    floor_rect(MAT_FLOOR_TILE, -26, -14, 26, 20, 0.0f);
    floor_rect(MAT_FLOOR_LINO, -26, -20, 26, -14, 0.0f);
    floor_rect(MAT_FLOOR_CONCRETE, -8.5f, -20, 8.5f, -15.6f, 0.0f);  // loading dock floor
    // ceilings
    ceil_rect(MAT_CEIL_TILE, -26, -14, 26, 20, WALL_H);
    ceil_rect(MAT_CEIL_CONC, -26, -20, 26, -14, WALL_H);

    // ---- outer walls with openings
    // south wall (entrance + windows)
    wall_axis(MAT_WALL_BRICK, -26, 20, -18, 20, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_BRICK, 18, 20, 26, 20, 0, WALL_H, WALL_T);
    // window bays (glass panels, breakable)
    for (int side = -1; side <= 1; side += 2) {
        float xs[3] = {18.f, 11.f, 4.5f};
        for (int i = 0; i < 3; i++) {
            float x0 = xs[i] * (float)side, x1 = (xs[i] - 5.0f) * (float)side;
            float mn = minf(x0, x1), mx = maxf(x0, x1);
            // sill
            wall_axis(MAT_WALL_BRICK, mn, 20, mx, 20, 0, 0.85f, WALL_T);
            // header
            wall_axis(MAT_WALL_BRICK, mn, 20, mx, 20, 3.2f, WALL_H, WALL_T);
            // glass
            quad(MAT_GLASS, v3(mn, 0.85f, 20.06f), v3(mx, 0.85f, 20.06f), v3(mx, 3.2f, 20.06f), v3(mn, 3.2f, 20.06f),
                 v3(0, 0, 1), 0, 0, (mx - mn), 2.35f);
            quad(MAT_GLASS, v3(mx, 0.85f, 19.94f), v3(mn, 0.85f, 19.94f), v3(mn, 3.2f, 19.94f), v3(mx, 3.2f, 19.94f),
                 v3(0, 0, -1), 0, 0, (mx - mn), 2.35f);
            // mullions
            for (int k = 0; k <= 4; k++) {
                float x = mn + (mx - mn) * (float)k / 4.f;
                mesh_add_boxf(mesh_for(MAT_WALL_METAL), v3(x - 0.04f, 0.85f, 19.94f), v3(x + 0.04f, 3.2f, 20.06f),
                              255, 255, 255, 1.0f);
            }
            add_aabb(v2(mn, 19.9f), v2(mx, 20.1f), 0.85f, 3.2f, 1, 0, 1, MAT_GLASS);
            // window light shaft (moonlight) — bluish
            add_light(v3((mn + mx) * 0.5f, 2.4f, 19.6f), v3(0.30f, 0.40f, 0.62f), 0.55f, 9.f);
        }
    }
    // entrance opening: x -4.5..4.5, vestibule structure
    wall_axis(MAT_WALL_BRICK, -4.5f, 20, -4.5f + 0.0f, 20.3f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_BRICK, 4.5f, 20, 4.5f, 20.3f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_BRICK, -4.5f, 20, 4.5f, 20, 3.4f, WALL_H, WALL_T);   // header over doors
    // vestibule side walls
    wall_axis(MAT_WALL_PAINT, -6.5f, 16.0f, -6.5f, 20.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 6.5f, 16.0f, 6.5f, 20.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, -6.5f, 16.0f, -4.0f, 16.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 4.0f, 16.0f, 6.5f, 16.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, -4.0f, 16.0f, 4.0f, 16.0f, 3.0f, WALL_H, WALL_T);
    quad(MAT_CEIL_TILE, v3(-6.5f, 3.9f, 16), v3(6.5f, 3.9f, 16), v3(6.5f, 3.9f, 20), v3(-6.5f, 3.9f, 20), v3(0, -1, 0), 0, 0, 13, 4);
    // entrance mats + floor
    floor_rect(MAT_FLOOR_CONCRETE, -6.5f, 16.0f, 6.5f, 20.0f, 0.01f);

    // west wall (with fire exit at z=-2)
    wall_axis(MAT_WALL_BLOCK, -26, -20, -26, -3.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_BLOCK, -26, -1.0f, -26, 20, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_BLOCK, -26, -3.0f, -26, -1.0f, 2.2f, WALL_H, WALL_T);   // header
    // east wall (fire exit at z=-2)
    wall_axis(MAT_WALL_BLOCK, 26, -20, 26, -3.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_BLOCK, 26, -1.0f, 26, 20, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_BLOCK, 26, -3.0f, 26, -1.0f, 2.2f, WALL_H, WALL_T);
    // north wall (loading dock roll-ups at x -8..-1 and 1..8)
    wall_axis(MAT_WALL_METAL, -26, -20, -8.5f, -20, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_METAL, 8.5f, -20, 26, -20, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_METAL, -8.5f, -20, -8.5f, -20, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_METAL, -8.5f, -20, 8.5f, -20, 3.0f, WALL_H, WALL_T);  // header above dock doors

    // ---- back-of-house corridor walls
    // corridor south wall (separates sales floor) with two doorways
    wall_axis(MAT_WALL_PAINT, -26, -13.6f, -10.0f, -13.6f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, -8.0f, -13.6f, 8.0f, -13.6f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 10.0f, -13.6f, 26, -13.6f, 0, WALL_H, WALL_T);
    // corridor north wall with doors into each BOH room
    wall_axis(MAT_WALL_PAINT, -26, -15.6f, -20.0f, -15.6f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, -18.0f, -15.6f, -12.0f, -15.6f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, -10.0f, -15.6f, -2.0f, -15.6f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 0.0f, -15.6f, 9.0f, -15.6f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 11.0f, -15.6f, 18.0f, -15.6f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 20.0f, -15.6f, 26, -15.6f, 0, WALL_H, WALL_T);
    // BOH room dividers
    wall_axis(MAT_WALL_BLOCK, -19.0f, -20, -19.0f, -15.6f, 0, WALL_H, WALL_T);   // storage A | storage B
    wall_axis(MAT_WALL_BLOCK, -11.0f, -20, -11.0f, -15.6f, 0, WALL_H, WALL_T);   // storage B | break room
    wall_axis(MAT_WALL_BLOCK, -1.0f, -20, -1.0f, -15.6f, 0, WALL_H, WALL_T);     // break | dock
    wall_axis(MAT_WALL_BLOCK, 10.0f, -20, 10.0f, -15.6f, 0, WALL_H, WALL_T);     // dock | maintenance
    wall_axis(MAT_WALL_BLOCK, 19.0f, -20, 19.0f, -15.6f, 0, WALL_H, WALL_T);     // maintenance | electrical
    // door headers in BOH
    float bh_doors[5] = {-19.0f, -11.0f, -1.0f, 10.0f, 19.0f};
    for (int i = 0; i < 5; i++) {
        // door frames (header) already handled by slit walls above
        (void)bh_doors[i];
    }
    // security office / manager office block (east side of sales floor)
    wall_axis(MAT_WALL_PAINT, 16.0f, -13.6f, 16.0f, -4.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 16.0f, -2.0f, 16.0f, 2.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, 16.0f, 2.0f, 26, 2.0f, 0, WALL_H, WALL_T);
    // glass front for security room
    quad(MAT_GLASS, v3(16.06f, 0.0f, -4.0f), v3(16.06f, 0.0f, -2.0f), v3(16.06f, WALL_H, -2.0f), v3(16.06f, WALL_H, -4.0f),
         v3(1, 0, 0), 0, 0, 2, 4.4f);
    // interior partition: pharmacy (north-west)
    wall_axis(MAT_WALL_PAINT, -26, -8.0f, -17.0f, -8.0f, 0, WALL_H, WALL_T);
    wall_axis(MAT_WALL_PAINT, -17.0f, -13.6f, -17.0f, -10.5f, 0, WALL_H, WALL_T);
    // bakery (north-east)
    wall_axis(MAT_WALL_TILE, 18.0f, -8.0f, 26, -8.0f, 0, WALL_H, WALL_T);
    // produce (south-west)
    wall_axis(MAT_WALL_TILE, -26, 8.0f, -18.0f, 8.0f, 0, WALL_H, WALL_T);

    // baseboards (thin dark strip) for depth
    (void)0;

    // roof slab + parapet (visible from outside)
    quad(MAT_FLOOR_CONCRETE, v3(-26, 4.4f, 20), v3(26, 4.4f, 20), v3(26, 4.4f, -20), v3(-26, 4.4f, -20), v3(0, 1, 0), 0, 0, 52, 40);
    for (int i = 0; i < 4; i++) {
        float x0 = (i & 1) ? 26 : -26, x1 = (i & 2) ? 26 : 26;
        (void)x0; (void)x1;
    }
    wall_axis(MAT_WALL_BLOCK, -26.2f, 20.2f, 26.2f, 20.2f, 4.4f, 5.1f, 0.4f);
    wall_axis(MAT_WALL_BLOCK, -26.2f, -20.2f, 26.2f, -20.2f, 4.4f, 5.1f, 0.4f);
    wall_axis(MAT_WALL_BLOCK, -26.2f, -20.2f, -26.2f, 20.2f, 4.4f, 5.1f, 0.4f);
    wall_axis(MAT_WALL_BLOCK, 26.2f, -20.2f, 26.2f, 20.2f, 4.4f, 5.1f, 0.4f);
    // roof clutter
    for (int i = 0; i < 4; i++) {
        place_model(g_m_roof_ac, v3(-16.f + i * 10.f, 4.4f, -10.f), 0.2f * (float)i, 1.0f);
    }
    place_model(g_m_roof_ac, v3(12.f, 4.4f, 6.f), 0.7f, 1.2f);
    // exterior sign on the facade
    place_model(g_m_sign_store, v3(0, 5.2f, 20.35f), 0.0f, 1.0f);
    place_model(g_m_sign_store, v3(0, 5.2f, 20.30f), 3.14159f, 1.0f);
    lv->store_center = v3(0, 0, 0);
}

// ---- interior fit-out ------------------------------------------------------
static void build_sales_floor(void) {
    Level *lv = g_b->lv;
    Rng *r = &g_b->rng;
    // ceiling light fixtures + baked lights
    for (int gx = 0; gx < 6; gx++) {
        for (int gz = 0; gz < 5; gz++) {
            float x = -21.f + gx * 8.4f;
            float z = -11.f + gz * 6.4f;
            if (x > 14.f && z < 3.f) continue;   // office block
            place_model(g_m_ceillight_lit, v3(x, WALL_H - 0.02f, z), 0.0f, 1.0f);
            add_light(v3(x, WALL_H - 0.35f, z), v3(0.92f, 0.95f, 1.0f), 0.80f, 13.0f);
        }
    }
    // emergency lights near the exits
    add_light(v3(0, 3.4f, 18.5f), v3(1.0f, 0.30f, 0.22f), 0.65f, 7.f);
    add_light(v3(0, 3.4f, 17.4f), v3(1.0f, 0.30f, 0.22f), 0.45f, 5.f);

    // ---- aisles (gondolas)
    float aisle_x[8] = {-20.0f, -15.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f, 15.0f};
    for (int i = 0; i < 8; i++) {
        float x = aisle_x[i];
        Model *m = (i == 4) ? g_m_shelf2 : g_m_shelf;
        // two units end to end along Z
        place_model(m, v3(x, 0, -6.0f), 0.0f, 1.0f);
        place_model(m, v3(x, 0, 0.3f), 0.0f, 1.0f);
        collider_from_model(m, v3(x, 0, -6.0f), 0, 1.0f, 1);
        collider_from_model(m, v3(x, 0, 0.3f), 0, 1.0f, 1);
        // aisle sign hanging above
        place_model(g_m_sign_aisle, v3(x, 3.0f, 1.5f), (i & 1) ? 1.5708f : -1.5708f, 1.0f);
        // one broken shelf per few aisles
        if (i == 2 || i == 6) {
            place_model(g_m_shelf_broken, v3(x + 2.2f, 0, -2.4f), 1.2f, 1.0f);
        }
    }
    // end caps with snacks: crates/boxes
    for (int i = 0; i < 8; i += 2) {
        place_model(g_m_boxes, v3(aisle_x[i], 0, 3.4f), 0.6f, 1.0f);
    }
    // ---- checkout lanes
    for (int i = 0; i < 6; i++) {
        float x = -15.f + i * 5.0f;
        place_model(g_m_checkout, v3(x, 0, 11.0f), 0.0f, 1.0f);
        add_aabb(v2(x - 2.5f, 10.5f), v2(x + 2.5f, 11.5f), 0, 1.05f, 1, 0, 1, MAT_WOOD);
        place_model(g_m_register, v3(x + 2.9f, 0, 11.0f), 3.14159f, 1.0f);
        // lane number sign
        place_model(g_m_sign_aisle, v3(x, 2.7f, 10.2f), 3.14159f, 0.7f);
        // impulse buy racks
        if (i % 2 == 0) place_model(g_m_boxes, v3(x + 1.9f, 0, 12.4f), 2.1f, 0.9f);
    }
    // queue barriers / carts
    place_model(g_m_cartnest, v3(-19.0f, 0, 14.5f), 0.4f, 1.0f);
    place_model(g_m_cartnest, v3(-19.2f, 0, 15.5f), 0.5f, 1.0f);
    place_model(g_m_cart, v3(6.2f, 0, 14.2f), 2.4f, 1.0f);
    place_model(g_m_cart, v3(7.4f, 0, 13.4f), 1.0f, 1.0f);
    place_model(g_m_cart, v3(-11.5f, 0, 9.0f), 0.2f, 1.0f);
    add_aabb(v2(6.0f, 13.2f), v2(7.6f, 14.5f), 0, 1.0f, 1, 0, 1, MAT_STEEL);

    // ---- refrigerated section (west wall)
    for (int i = 0; i < 4; i++) {
        float z = -9.0f + i * 4.2f;
        place_model(g_m_fridge, v3(-24.6f, 0, z), 1.5708f, 1.0f);
        add_aabb(v2(-25.4f, z - 1.8f), v2(-23.5f, z + 1.8f), 0, 2.1f, 1, 0, 1, MAT_STEEL);
    }
    add_light(v3(-23.5f, 2.6f, -2.0f), v3(0.62f, 0.86f, 1.0f), 0.5f, 9.f);
    // frozen section (east wall)
    for (int i = 0; i < 3; i++) {
        float z = -9.0f + i * 5.4f;
        place_model(g_m_freezer, v3(24.2f, 0, z), 1.5708f, 1.0f);
        add_aabb(v2(23.6f, z - 2.0f), v2(25.2f, z + 2.0f), 0, 1.1f, 1, 0, 1, MAT_STEEL);
    }
    add_light(v3(23.2f, 2.6f, -2.0f), v3(0.66f, 0.88f, 1.0f), 0.45f, 8.f);

    // ---- produce (south-west)
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 3; k++) {
            place_model(g_m_pallet, v3(-24.0f + k * 1.6f, 0, 9.5f + i * 1.3f), 0.0f, 0.9f);
        }
    }
    add_aabb(v2(-25.0f, 9.0f), v2(-18.4f, 14.2f), 0, 0.8f, 1, 0, 1, MAT_PRODUCE);
    // wet floor sign + mop bucket debris
    place_model(g_m_wetsign, v3(-8.0f, 0, 7.5f), 0.8f, 1.0f);

    // ---- bakery (south-east)
    for (int i = 0; i < 3; i++) {
        place_model(g_m_table, v3(19.5f + i * 2.2f, 0, 10.5f), 0.0f, 1.0f);
        place_model(g_m_boxes, v3(19.5f + i * 2.2f, 0.76f, 10.5f), 0.4f, 0.7f);
    }
    add_aabb(v2(18.5f, 9.8f), v2(24.4f, 11.4f), 0, 1.0f, 1, 0, 1, MAT_PLYWOOD);

    // ---- pharmacy (north-west)
    place_model(g_m_shelf2, v3(-25.0f, 0, -11.0f), 1.5708f, 0.9f);
    place_model(g_m_table, v3(-19.0f, 0, -11.0f), 0.0f, 0.9f);
    add_light(v3(-21.0f, 3.0f, -11.0f), v3(0.95f, 0.92f, 0.86f), 0.42f, 6.f);
    place_model(g_m_sign_warn, v3(-17.1f, 1.6f, -12.0f), 1.5708f, 1.0f);

    // ---- security office interior (east)
    place_model(g_m_table, v3(20.0f, 0, -10.0f), 0.0f, 1.1f);
    place_model(g_m_atm, v3(22.6f, 0, -9.0f), 1.5708f, 1.0f);
    // monitor wall
    {
        Mesh *s = mesh_for(MAT_SCREEN);
        for (int i = 0; i < 6; i++) {
            float x = 16.2f;
            float z = -12.4f + (float)(i % 3) * 0.9f;
            float y = 1.5f + (float)(i / 3) * 0.85f;
            mesh_quad_uv(s, v3(x, y, z), v3(x, y, z + 0.72f), v3(x, y + 0.58f, z + 0.72f), v3(x, y + 0.58f, z),
                         v3(1, 0, 0), 0, 1, 1, 0, 255, 255, 255);
        }
    }
    add_light(v3(18.0f, 2.4f, -10.0f), v3(0.35f, 0.75f, 0.55f), 0.35f, 6.f);
    // office desks
    place_model(g_m_table, v3(19.0f, 0, -3.0f), 0.3f, 1.0f);
    place_model(g_m_chair, v3(19.0f, 0, -4.0f), 0.6f, 1.0f);
    place_model(g_m_locker, v3(25.2f, 0, 0.0f), 3.14159f, 1.0f);
    // directory board on the wall by the entrance
    place_model_take(prop_wall_sign(MAT_DIRECTORY, 1.6f, 1.6f), v3(-3.9f, 1.2f, 17.5f), 1.5708f, 1.0f);

    // ---- entrance area detail
    place_model(g_m_vending, v3(-6.0f, 0, 16.4f), 0.0f, 1.0f);
    place_model(g_m_atm, v3(6.2f, 0, 16.5f), 0.0f, 1.0f);
    place_model(g_m_extinguisher, v3(5.9f, 0, 19.4f), 0.0f, 1.0f);
    place_model(g_m_extinguisher, v3(-5.9f, 0, 19.4f), 0.0f, 1.0f);
    // posters on the wall
    place_model_take(prop_wall_sign(MAT_POSTER_A, 1.2f, 1.2f), v3(-12.0f, 1.6f, 19.85f), 3.14159f, 1.0f);
    place_model_take(prop_wall_sign(MAT_POSTER_B, 1.0f, 1.0f), v3(12.0f, 1.7f, 19.85f), 3.14159f, 1.0f);
    place_model_take(prop_wall_sign(MAT_PRICE, 1.4f, 0.6f), v3(-19.0f, 1.4f, 0.2f), 1.5708f, 1.0f);

    // scattered debris and blood
    for (int i = 0; i < 7; i++) {
        float x = rng_range(r, -22.f, 22.f);
        float z = rng_range(r, -12.f, 15.f);
        place_model(g_m_debris[i % 3], v3(x, 0, z), rng_range(r, 0, 6.28f), 1.0f);
    }
    place_model(g_m_mannequin, v3(13.0f, 0, 6.0f), 2.2f, 1.0f);
    place_model(g_m_palletjack, v3(-7.5f, 0, 12.5f), 1.1f, 1.0f);
    add_aabb(v2(-8.3f, 11.6f), v2(-6.7f, 13.4f), 0, 1.2f, 1, 0, 1, MAT_STEEL);

    // barricade material: stacked boards near the entrance
    place_model(g_m_boxes, v3(9.0f, 0, 17.5f), 0.3f, 1.0f);
}

static void build_back_of_house(void) {
    Level *lv = g_b->lv;
    Rng *r = &g_b->rng;
    // corridor lights
    for (int i = 0; i < 6; i++) {
        float x = -22.f + i * 8.6f;
        place_model(g_m_ceillight_lit, v3(x, WALL_H - 0.02f, -14.6f), 0.0f, 0.85f);
        add_light(v3(x, WALL_H - 0.4f, -14.6f), v3(0.95f, 0.93f, 0.85f), 0.75f, 7.5f);
    }
    // storage A (west): shelving + boxes
    for (int i = 0; i < 3; i++) {
        place_model(g_m_shelf, v3(-23.0f, 0, -18.2f + i * 1.6f), 1.5708f, 1.0f);
        collider_from_model(g_m_shelf, v3(-23.0f, 0, -18.2f + i * 1.6f), 0, 1.0f, 1);
    }
    for (int i = 0; i < 4; i++) place_model(g_m_pallet, v3(-15.5f + (i % 2) * 1.5f, 0, -18.5f + (i / 2) * 1.4f), 0.1f * i, 1.0f);
    place_model(g_m_locker, v3(-20.6f, 0, -15.9f), 0.0f, 1.0f);
    add_light(v3(-21.0f, 3.4f, -17.5f), v3(0.9f, 0.88f, 0.78f), 0.55f, 7.f);
    // storage B: tall racking
    for (int i = 0; i < 3; i++) {
        place_model(g_m_shelf, v3(-16.5f, 0, -18.4f + i * 1.7f), 1.5708f, 1.0f);
        collider_from_model(g_m_shelf, v3(-16.5f, 0, -18.4f + i * 1.7f), 0, 1.0f, 1);
    }
    place_model(g_m_crate, v3(-13.0f, 0, -17.0f), 0.4f, 1.0f);
    add_light(v3(-14.0f, 3.4f, -17.5f), v3(0.86f, 0.9f, 0.95f), 0.45f, 6.f);
    // staff break room
    place_model(g_m_table, v3(-6.0f, 0, -18.0f), 0.0f, 1.0f);
    for (int i = 0; i < 3; i++) place_model(g_m_chair, v3(-7.4f + i * 1.4f, 0, -19.0f), 0.2f * i, 1.0f);
    place_model(g_m_locker, v3(-10.6f, 0, -17.0f), 1.5708f, 1.0f);
    place_model(g_m_vending, v3(-3.0f, 0, -18.9f), 0.0f, 1.0f);
    add_light(v3(-6.0f, 3.2f, -17.5f), v3(0.95f, 0.85f, 0.7f), 0.5f, 6.5f);
    // loading dock
    place_model(g_m_pallet, v3(-6.0f, 0, -18.0f), 0.0f, 1.0f);
    place_model(g_m_pallet, v3(4.0f, 0, -18.5f), 0.2f, 1.0f);
    place_model(g_m_boxes, v3(1.5f, 0, -16.8f), 0.5f, 1.0f);
    place_model(g_m_palletjack, v3(-2.0f, 0, -17.5f), 0.8f, 1.0f);
    add_light(v3(0.0f, 3.6f, -18.0f), v3(0.8f, 0.85f, 0.95f), 0.55f, 10.f);
    // dock roll-up doors (openings at x -8.5..-1 and 1..8.5)
    place_model_take(prop_door(2), v3(-4.75f, 0, -20.0f), 0.0f, 1.4f);
    place_model_take(prop_door(2), v3(4.75f, 0, -20.0f), 0.0f, 1.4f);
    // maintenance room: generator, pipes, tools
    place_model(g_m_generator, v3(14.5f, 0, -18.0f), 0.0f, 1.0f);
    add_aabb(v2(13.4f, -18.7f), v2(15.6f, -17.3f), 0, 1.2f, 1, 0, 1, MAT_STEEL);
    place_model(g_m_pipe, v3(14.0f, 3.6f, -17.5f), 0.0f, 1.0f);
    place_model(g_m_pipe, v3(14.0f, 3.3f, -17.0f), 0.0f, 0.9f);
    place_model(g_m_table, v3(17.5f, 0, -17.0f), 0.0f, 1.0f);
    place_model(g_m_barrel, v3(11.5f, 0, -19.0f), 0.0f, 1.0f);
    place_model(g_m_barrel, v3(12.4f, 0, -19.2f), 0.0f, 1.0f);
    add_light(v3(15.0f, 3.4f, -17.5f), v3(1.0f, 0.8f, 0.55f), 0.6f, 7.f);
    // electrical room
    for (int i = 0; i < 3; i++) {
        place_model(g_m_locker, v3(21.0f + i * 0.02f, 0, -19.2f + i * 0.9f), 3.14159f, 1.1f);
    }
    // breaker panels
    {
        Mesh *s = mesh_for(MAT_WALL_METAL);
        for (int i = 0; i < 3; i++) {
            mesh_add_boxf(s, v3(25.4f, 1.1f + i * 0.9f, -18.6f), v3(25.6f, 1.7f + i * 0.9f, -17.6f), 255, 255, 255, 1.4f);
        }
    }
    add_light(v3(22.0f, 3.2f, -17.5f), v3(0.86f, 0.95f, 1.0f), 0.45f, 6.f);
    // emergency exit signs glowing above the fire exits
    mesh_add_boxf(mesh_for(MAT_EMISSIVE_GREEN), v3(-26.2f, 2.4f, -2.4f), v3(-26.0f, 2.9f, -1.6f), 255, 255, 255, 1.f);
    mesh_add_boxf(mesh_for(MAT_EMISSIVE_GREEN), v3(26.0f, 2.4f, -2.4f), v3(26.2f, 2.9f, -1.6f), 255, 255, 255, 1.f);

    // blood trail from the loading dock into the store (storytelling)
    place_model(g_m_debris[0], v3(2.0f, 0, -16.0f), 1.2f, 1.0f);
    // roof access stair (visual, blocked by a chained door)
    {
        Mesh *steel = mesh_for(MAT_WALL_METAL);
        for (int i = 0; i < 12; i++) {
            float y = 0.18f * (float)i;
            mesh_add_boxf(steel, v3(24.2f, y, -6.0f + (float)i * 0.28f), v3(25.6f, y + 0.05f, -5.75f + (float)i * 0.28f), 255, 255, 255, 1.2f);
        }
        mesh_add_boxf(steel, v3(24.2f, 2.2f, 3.2f), v3(25.6f, 2.3f, 3.4f), 255, 255, 255, 1.2f);
        add_aabb(v2(24.0f, -6.2f), v2(25.8f, 3.6f), 0, 2.4f, 1, 0, 1, MAT_STEEL);
    }
    lv->roof_access = v3(24.8f, 0, 3.4f);
    lv->security_room = v3(20.0f, 0, -9.0f);
    lv->generator_pos = v3(14.5f, 0, -18.0f);
    lv->counter_pos = v3(-15.0f, 0, 11.0f);
}

static void build_exterior(void) {
    Level *lv = g_b->lv;
    Rng *r = &g_b->rng;
    // ground plane: parking lot, apron, road, verges
    floor_rect(MAT_ASPHALT, -46, 20, 46, 58, -0.02f);
    floor_rect(MAT_ASPHALT, -46, -30, 46, -20, -0.02f);
    floor_rect(MAT_ASPHALT, -70, 58, 70, 74, -0.02f);
    floor_rect(MAT_GRAVEL, -46, -34, 46, -30, -0.02f);
    floor_rect(MAT_DIRT, -70, 74, 70, 88, -0.02f);
    floor_rect(MAT_DIRT, -70, -34, -46, 88, -0.02f);
    floor_rect(MAT_DIRT, 46, -34, 70, 88, -0.02f);
    // grass verge strips
    floor_rect(MAT_DIRT, -46, 58, 46, 60, -0.015f);

    // parking bay lines
    {
        Mesh *line = mesh_for(MAT_SIGN_WARN);
        (void)line;
        for (int i = 0; i < 12; i++) {
            float x = -40.f + i * 6.4f;
            quad(MAT_SIGN_WARN, v3(x - 0.06f, 0.005f, 24.f), v3(x + 0.06f, 0.005f, 24.f),
                 v3(x + 0.06f, 0.005f, 31.f), v3(x - 0.06f, 0.005f, 31.f), v3(0, 1, 0), 0, 0, 0.12f, 7.f);
            quad(MAT_SIGN_WARN, v3(x - 0.06f, 0.005f, 34.f), v3(x + 0.06f, 0.005f, 34.f),
                 v3(x + 0.06f, 0.005f, 41.f), v3(x - 0.06f, 0.005f, 41.f), v3(0, 1, 0), 0, 0, 0.12f, 7.f);
        }
    }
    // scattered cars in the lot
    for (int i = 0; i < 11; i++) {
        float x = -38.f + (float)i * 6.4f + rng_range(r, -0.4f, 0.4f);
        float z = (i % 2 == 0) ? 27.5f : 37.5f;
        float yaw = (i % 2 == 0) ? 0.02f : 3.16f;
        yaw += rng_range(r, -0.12f, 0.12f);
        Model *car = g_m_car[i % 3];
        place_model(car, v3(x, 0, z), yaw, 1.0f);
        add_aabb(v2(x - 1.1f, z - 2.3f), v2(x + 1.1f, z + 2.3f), 0, 1.4f, 1, 1, 1, MAT_STEEL);
    }
    // delivery trucks at the loading dock and one crashed by the entrance
    place_model(g_m_truck_box, v3(-14.0f, 0, -25.0f), 1.5708f, 1.0f);
    add_aabb(v2(-18.3f, -26.2f), v2(-9.7f, -23.8f), 0, 3.2f, 1, 1, 1, MAT_STEEL);
    place_model(g_m_truck_box, v3(10.0f, 0, -26.5f), 1.62f, 1.0f);
    add_aabb(v2(5.7f, -27.7f), v2(14.3f, -25.3f), 0, 3.2f, 1, 1, 1, MAT_STEEL);
    place_model(g_m_truck, v3(16.0f, 0, 30.0f), 0.1f, 1.0f);
    add_aabb(v2(14.7f, 25.6f), v2(17.3f, 34.4f), 0, 2.8f, 1, 1, 1, MAT_STEEL);
    // dumpsters behind the store
    place_model(g_m_dumpster, v3(-20.0f, 0, -23.0f), 0.0f, 1.0f);
    place_model(g_m_dumpster, v3(-20.0f, 0, -24.6f), 0.0f, 1.0f);
    place_model(g_m_dumpster, v3(20.0f, 0, -24.0f), 0.06f, 1.0f);
    add_aabb(v2(-21.0f, -25.6f), v2(-19.0f, -22.4f), 0, 1.4f, 1, 1, 1, MAT_STEEL);
    add_aabb(v2(19.0f, -25.0f), v2(21.0f, -23.0f), 0, 1.4f, 1, 1, 1, MAT_STEEL);
    // pallets, barrels, cones outside
    for (int i = 0; i < 5; i++) place_model(g_m_pallet, v3(-8.0f + i * 2.0f, 0, -22.0f), rng_range(r, 0, 3.14f), 1.0f);
    for (int i = 0; i < 4; i++) place_model(g_m_barrel, v3(4.0f + i * 0.8f, 0, -23.0f), 0.0f, 1.0f);
    for (int i = 0; i < 6; i++) place_model(g_m_cone, v3(-6.0f + i * 2.5f, 0, 21.5f), 0.0f, 1.0f);
    // shopping carts scattered
    for (int i = 0; i < 5; i++) {
        place_model(g_m_cart, v3(rng_range(r, -30, 30), 0, rng_range(r, 21, 26)), rng_range(r, 0, 6.28f), 1.0f);
    }
    // street lights along the lot + outside
    float lx[4] = {-36.f, -12.f, 12.f, 36.f};
    for (int i = 0; i < 4; i++) {
        place_model(g_m_streetlight, v3(lx[i], 0, 23.0f), 0.0f, 1.0f);
        add_aabb(v2(lx[i] - 0.2f, 22.8f), v2(lx[i] + 0.2f, 23.2f), 0, 6.5f, 1, 0, 1, MAT_STEEL);
        add_light(v3(lx[i], 6.6f, 24.4f), v3(1.0f, 0.72f, 0.36f), 1.35f, 20.f);
    }
    // parking lot light poles (smaller)
    for (int i = 0; i < 6; i++) {
        float x = -30.f + i * 12.f;
        place_model(g_m_lightpole_on, v3(x, 0, 46.0f), 3.14159f, 1.0f);
        add_light(v3(x, 1.75f, 46.5f), v3(0.85f, 0.92f, 1.0f), 0.75f, 13.f);
    }
    // perimeter chain fence with a gate at the road
    for (int i = 0; i < 12; i++) {
        float x = -44.f + i * 8.f;
        place_model(g_m_fence, v3(x, 0, 58.0f), 0.0f, 1.0f);
        add_aabb(v2(x - 2.0f, 57.8f), v2(x + 2.0f, 58.2f), 0, 2.2f, 1, 0, 1, MAT_CHAINLINK);
    }
    for (int i = 0; i < 11; i++) {
        float z = 20.f + i * 4.f;
        place_model(g_m_fence, v3(-45.0f, 0, z), 1.5708f, 1.0f);
        place_model(g_m_fence, v3(45.0f, 0, z), 1.5708f, 1.0f);
        add_aabb(v2(-45.2f, z - 2.0f), v2(-44.8f, z + 2.0f), 0, 2.2f, 1, 0, 1, MAT_CHAINLINK);
        add_aabb(v2(44.8f, z - 2.0f), v2(45.2f, z + 2.0f), 0, 2.2f, 1, 0, 1, MAT_CHAINLINK);
    }
    for (int i = 0; i < 12; i++) {
        float x = -44.f + i * 8.f;
        place_model(g_m_fence, v3(x, 0, -28.0f), 0.0f, 1.0f);
        add_aabb(v2(x - 2.0f, -28.2f), v2(x + 2.0f, -27.8f), 0, 2.2f, 1, 0, 1, MAT_CHAINLINK);
    }
    // ==== neighbouring buildings (silhouette) across the road
    Mesh *bld = mesh_for(MAT_WALL_BLOCK);
    Rng rng2;
    rng_seed(&rng2, 991);
    for (int i = 0; i < 14; i++) {
        float x = -66.f + i * 10.f + rng_range(&rng2, -2, 2);
        float w = rng_range(&rng2, 8, 14);
        float h = rng_range(&rng2, 6, 20);
        float z = 80.f + rng_range(&rng2, -3, 3);
        mesh_add_boxf(bld, v3(x, 0, z), v3(x + w, h, z + 12), 255, 255, 255, 0.5f);
        // lit windows
        Mesh *win = mesh_for(MAT_EMISSIVE_WARM);
        for (int wy = 1; wy < (int)(h / 3.f); wy++)
            for (int wx = 0; wx < 4; wx++) {
                if (rng_f(&rng2) > 0.25f) continue;
                float px = x + 1.0f + wx * (w - 2.f) / 4.f;
                float py = 1.6f + wy * 3.f;
                mesh_add_boxf(win, v3(px, py, z - 0.06f), v3(px + 1.1f, py + 1.5f, z - 0.02f), 255, 255, 255, 1.f);
            }
    }
    // road markings + street lights along the road
    for (int i = 0; i < 16; i++) {
        float x = -64.f + i * 8.6f;
        quad(MAT_SIGN_WARN, v3(x, 0.005f, 65.5f), v3(x + 4.f, 0.005f, 65.5f), v3(x + 4.f, 0.005f, 65.9f), v3(x, 0.005f, 65.9f),
             v3(0, 1, 0), 0, 0, 4.f, 0.4f);
    }
    for (int i = 0; i < 3; i++) {
        float x = -40.f + i * 30.f;
        place_model(g_m_streetlight, v3(x, 0, 62.0f), 3.14159f, 1.15f);
        add_light(v3(x, 7.4f, 60.6f), v3(1.0f, 0.72f, 0.4f), 1.5f, 22.f);
    }
    // fire barrel by the loading dock (warm light)
    place_model(g_m_barrel_fire, v3(-2.0f, 0, -22.0f), 0.0f, 1.0f);
    add_light(v3(-2.0f, 1.2f, -22.0f), v3(1.0f, 0.45f, 0.15f), 1.2f, 10.f);
    // exterior debris + abandoned camp
    for (int i = 0; i < 6; i++) {
        place_model(g_m_debris[i % 3], v3(rng_range(r, -40, 40), 0, rng_range(r, 21, 45)), rng_range(r, 0, 6.28f), 1.0f);
    }
    place_model(g_m_truck, v3(-34.0f, 0, 21.5f), 0.3f, 1.0f);
    add_aabb(v2(-35.3f, 17.1f), v2(-32.7f, 25.9f), 0, 2.8f, 1, 1, 1, MAT_STEEL);
    lv->boss_arena = v3(0, 0, 32.0f);
}

// ---- doors, barricade slots, spawn points ---------------------------------
static void add_door(Vec3 pos, float yaw, float width, float height, int kind, const char *name) {
    Level *lv = g_b->lv;
    if (lv->door_count >= 16) return;
    Door *d = &lv->doors[lv->door_count];
    memset(d, 0, sizeof *d);
    d->pos = pos;
    d->yaw = yaw;
    d->width = width;
    d->height = height;
    d->kind = kind;
    d->open = 1;
    snprintf(d->name, sizeof d->name, "%s", name);
    lv->door_count++;
    place_model_take(prop_door(kind), pos, yaw, width / 1.3f);
}
static void add_slot(Vec3 pos, float yaw, float width, int door_index, int is_window, const char *name) {
    Level *lv = g_b->lv;
    if (lv->slot_count >= MAX_BARRICADES) return;
    BarricadeSlot *s = &lv->slots[lv->slot_count];
    memset(s, 0, sizeof *s);
    s->pos = pos;
    s->yaw = yaw;
    s->width = width;
    s->door_index = door_index;
    s->barricade = -1;
    s->is_window = is_window;
    snprintf(s->name, sizeof s->name, "%s", name);
    lv->slot_count++;
}
static void add_spawn(Vec3 pos, float yaw, int room) {
    Level *lv = g_b->lv;
    if (lv->spawn_count >= 48) return;
    SpawnPoint *s = &lv->spawns[lv->spawn_count++];
    s->pos = pos;
    s->yaw = yaw;
    s->room = room;
}

static void build_openings(void) {
    Level *lv = g_b->lv;
    // main entrance: two glass door leaves, centred at x = +/-1.5
    add_door(v3(-1.6f, 0, 20.0f), 3.14159f, 3.2f, 2.6f, 1, "Main Entrance L");
    add_door(v3(1.6f, 0, 20.0f), 3.14159f, 3.2f, 2.6f, 1, "Main Entrance R");
    add_slot(v3(0.0f, 0, 19.2f), 0.0f, 6.0f, -1, 0, "Main Entrance");
    // window bays (breakable windows double as entry points)
    float wx[6] = {-20.5f, -13.0f, -6.75f, 6.75f, 13.0f, 20.5f};
    for (int i = 0; i < 6; i++) {
        char nm[32];
        snprintf(nm, sizeof nm, "Window %d", i + 1);
        add_slot(v3(wx[i], 0, 19.0f), 0.0f, 4.6f, -1, 1, nm);
    }
    // side fire exits
    add_door(v3(-26.0f, 0, -2.0f), 1.5708f, 1.2f, 2.2f, 3, "West Fire Exit");
    add_door(v3(26.0f, 0, -2.0f), -1.5708f, 1.2f, 2.2f, 3, "East Fire Exit");
    add_slot(v3(-25.2f, 0, -2.0f), 1.5708f, 1.6f, -1, 0, "West Exit");
    add_slot(v3(25.2f, 0, -2.0f), -1.5708f, 1.6f, -1, 0, "East Exit");
    // loading dock roll-ups
    add_slot(v3(-4.75f, 0, -19.0f), 0.0f, 5.2f, -1, 0, "Dock Door A");
    add_slot(v3(4.75f, 0, -19.0f), 0.0f, 5.2f, -1, 0, "Dock Door B");
    // interior BOH doors
    add_door(v3(-9.0f, 0, -13.6f), 0.0f, 1.1f, 2.1f, 0, "Corridor Door W");
    add_door(v3(9.0f, 0, -13.6f), 0.0f, 1.1f, 2.1f, 0, "Corridor Door E");

    // player spawn in the vestibule, facing into the store
    lv->player_spawn = v3(0, 0, 17.6f);
    lv->player_spawn_yaw = 3.14159f;

    // zombie spawn points outside, arranged in rings
    for (int i = 0; i < 10; i++) {
        float a = (float)i / 10.f * LN2PI;
        add_spawn(v3(cosf(a) * 44.f, 0, 30.f + sinf(a) * 12.f), a + (float)M_PI, 0);
    }
    for (int i = 0; i < 8; i++) {
        float x = -38.f + (float)i * 11.f;
        add_spawn(v3(x, 0, 52.f), 3.14159f, 0);
    }
    for (int i = 0; i < 6; i++) {
        float x = -30.f + (float)i * 12.f;
        add_spawn(v3(x, 0, -26.f), 0.0f, 0);
    }
    for (int i = 0; i < 6; i++) {
        float x = -44.f + (float)i * 16.f;
        add_spawn(v3(x, 0, 22.f), 1.5708f, 0);
    }
    // interior spawns (for late nights: they are already inside)
    add_spawn(v3(-22.0f, 0, -17.5f), 0.0f, 1);
    add_spawn(v3(15.0f, 0, 12.0f), 3.14159f, 1);
    add_spawn(v3(-20.0f, 0, 6.0f), 1.5708f, 1);
    add_spawn(v3(20.0f, 0, -17.0f), 0.0f, 1);
}

// ---------------------------------------------------------------------------
// Navigation grid
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Baked lighting
// ---------------------------------------------------------------------------
static float g_bake_grid_r[200 * 200];
static float g_bake_grid_g[200 * 200];
static float g_bake_grid_b[200 * 200];
static int g_bake_w, g_bake_h;
static Vec2 g_bake_origin;
static float g_bake_cell;

static void bake_lighting(Level *lv) {
    g_bake_origin = v2(-48.f, -36.f);
    g_bake_cell = 0.75f;
    g_bake_w = (int)(96.f / g_bake_cell);
    g_bake_h = (int)(92.f / g_bake_cell);
    if (g_bake_w > 200) g_bake_w = 200;
    if (g_bake_h > 200) g_bake_h = 200;
    size_t n = (size_t)g_bake_w * g_bake_h;
    memset(g_bake_grid_r, 0, n * sizeof(float));
    memset(g_bake_grid_g, 0, n * sizeof(float));
    memset(g_bake_grid_b, 0, n * sizeof(float));
    // per-light BFS visibility
    static float dist[200 * 200];
    for (int li = 0; li < g_nlight; li++) {
        BakeLight *L = &g_lights[li];
        for (size_t i = 0; i < n; i++) dist[i] = 1e30f;
        int lx = (int)((L->pos.x - g_bake_origin.x) / g_bake_cell);
        int lz = (int)((L->pos.z - g_bake_origin.y) / g_bake_cell);
        if (lx < 0 || lz < 0 || lx >= g_bake_w || lz >= g_bake_h) continue;
        // simple Dijkstra with a bucket queue
        static int qx[200 * 200], qz[200 * 200];
        int qh = 0, qt = 0;
        dist[(size_t)lz * g_bake_w + lx] = 0.f;
        qx[qt] = lx; qz[qt] = lz; qt++;
        float maxd = L->radius / g_bake_cell;
        while (qh < qt) {
            int cx = qx[qh], cz = qz[qh];
            qh++;
            if (qh > 200 * 200 - 8) break;
            float d0 = dist[(size_t)cz * g_bake_w + cx];
            if (d0 > maxd) continue;
            static const int ox[8] = {1, -1, 0, 0, 1, 1, -1, -1};
            static const int oz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
            for (int k = 0; k < 8; k++) {
                int nx = cx + ox[k], nz = cz + oz[k];
                if (nx < 0 || nz < 0 || nx >= g_bake_w || nz >= g_bake_h) continue;
                float wcost = (k >= 4) ? 1.4142f : 1.f;
                float nd = d0 + wcost;
                if (nd > maxd) continue;
                size_t ni = (size_t)nz * g_bake_w + nx;
                if (nd >= dist[ni]) continue;
                // check blocking in the world
                float wx = g_bake_origin.x + ((float)nx + 0.5f) * g_bake_cell;
                float wz = g_bake_origin.y + ((float)nz + 0.5f) * g_bake_cell;
                int blocked = 0;
                for (int a = 0; a < lv->aabb_count; a++) {
                    const AABB *ab = &lv->aabbs[a];
                    if (!ab->blocks_light) continue;
                    if (ab->ymin > L->pos.y - 0.2f && ab->ymin > 2.0f) continue;
                    if (wx > ab->mn.x && wx < ab->mx.x && wz > ab->mn.y && wz < ab->mx.y && ab->ymax > 1.0f) {
                        blocked = 1;
                        break;
                    }
                }
                if (blocked) continue;
                dist[ni] = nd;
                if (qt < 200 * 200 - 8) { qx[qt] = nx; qz[qt] = nz; qt++; }
            }
        }
        // accumulate with attenuation
        float r2 = L->radius * L->radius;
        for (int z = 0; z < g_bake_h; z++)
            for (int x = 0; x < g_bake_w; x++) {
                size_t i = (size_t)z * g_bake_w + x;
                if (dist[i] > maxd) continue;
                float d = dist[i] * g_bake_cell;
                float att = 1.f / (1.f + d * d * 0.55f);
                att *= (1.f - saturatef(d / L->radius)) * 0.55f + 0.45f;
                float k = L->intensity * att;
                g_bake_grid_r[i] += L->color.x * k;
                g_bake_grid_g[i] += L->color.y * k;
                g_bake_grid_b[i] += L->color.z * k;
            }
    }
    // apply to mesh vertices
    for (int mi = 0; mi < lv->mesh_count; mi++) {
        Mesh *m = lv->meshes[mi];
        int emissive = (m->mat == MAT_EMISSIVE_WARM || m->mat == MAT_EMISSIVE_COOL ||
                        m->mat == MAT_EMISSIVE_RED || m->mat == MAT_EMISSIVE_GREEN ||
                        m->mat == MAT_EMISSIVE_SCREEN || m->mat == MAT_LAMPGLASS);
        for (uint32_t v = 0; v < m->vert_used; v++) {
            Vec3 p = m->pos[v];
            float gx = (p.x - g_bake_origin.x) / g_bake_cell - 0.5f;
            float gz = (p.z - g_bake_origin.y) / g_bake_cell - 0.5f;
            int x0 = (int)floorf(gx), z0 = (int)floorf(gz);
            float tx = gx - x0, tz = gz - z0;
            int x1 = x0 + 1, z1 = z0 + 1;
            x0 = clampi(x0, 0, g_bake_w - 1); x1 = clampi(x1, 0, g_bake_w - 1);
            z0 = clampi(z0, 0, g_bake_h - 1); z1 = clampi(z1, 0, g_bake_h - 1);
            float rr = 0, gg = 0, bb = 0;
            for (int k = 0; k < 4; k++) {
                int sx = (k & 1) ? x1 : x0;
                int sz = (k & 2) ? z1 : z0;
                float w = ((k & 1) ? tx : 1.f - tx) * ((k & 2) ? tz : 1.f - tz);
                size_t i = (size_t)sz * g_bake_w + sx;
                rr += g_bake_grid_r[i] * w;
                gg += g_bake_grid_g[i] * w;
                bb += g_bake_grid_b[i] * w;
            }
            // vertical falloff: everything is lit from ceiling fixtures
            float yf = 1.0f;
            if (p.y > 2.4f) yf = 1.f - (p.y - 2.4f) * 0.20f;
            if (p.y < 0.6f) yf = 0.72f + p.y * 0.45f;
            yf = clampf(yf, 0.20f, 1.35f);
            // ambient (moonlight through the glass, plus bounce)
            Vec3 amb = v3(0.052f, 0.058f, 0.078f);
            if (p.y < 0.4f) amb = vscale(amb, 0.8f);
            float r = rr * yf + amb.x;
            float g = gg * yf + amb.y;
            float b = bb * yf + amb.z;
            if (emissive) { r = g = b = 1.0f; }
            m->lit[v * 3 + 0] = (uint8_t)clampi((int)(r * 255.f), 0, 255);
            m->lit[v * 3 + 1] = (uint8_t)clampi((int)(g * 255.f), 0, 255);
            m->lit[v * 3 + 2] = (uint8_t)clampi((int)(b * 255.f), 0, 255);
        }
    }
}

// ---------------------------------------------------------------------------
void level_build(Assets *as, Level *lv) {
    memset(lv, 0, sizeof *lv);
    Builder b;
    memset(&b, 0, sizeof b);
    b.as = as;
    b.lv = lv;
    rng_seed(&b.rng, 20261025ULL);
    g_b = &b;
    g_nlight = 0;
    build_models();
    build_shell();
    build_sales_floor();
    build_back_of_house();
    build_exterior();
    build_openings();

    // finalise static meshes
    for (int i = 0; i < MAT_COUNT; i++) {
        if (!b.mesh[i]) continue;
        if (b.mesh[i]->tri_used == 0) { mesh_free(b.mesh[i]); continue; }
        lv->meshes[lv->mesh_count++] = b.mesh[i];
    }
    LOGI("level: %d meshes, %d triangles, %d colliders, %d lights, %d slots",
         lv->mesh_count, 0, lv->aabb_count, g_nlight, lv->slot_count);
    bake_lighting(lv);
    nav_build(&lv->nav, lv);
    // free the temporary prop models (their geometry was copied into the batches)
    struct Model **all[] = {&g_m_shelf, &g_m_shelf2, &g_m_shelf_broken, &g_m_checkout, &g_m_cart, &g_m_cartnest,
                            &g_m_freezer, &g_m_fridge, &g_m_pallet, &g_m_boxes, &g_m_barrel, &g_m_barrel_fire,
                            &g_m_table, &g_m_chair, &g_m_locker, &g_m_vending, &g_m_atm, &g_m_dumpster,
                            &g_m_streetlight, &g_m_fence, &g_m_truck, &g_m_truck_box, &g_m_generator,
                            &g_m_ceillight_lit, &g_m_ceillight_off, &g_m_sign_aisle, &g_m_sign_store,
                            &g_m_sign_exit, &g_m_sign_warn, &g_m_pipe, &g_m_mannequin, &g_m_palletjack,
                            &g_m_register, &g_m_wetsign, &g_m_extinguisher, &g_m_crate, &g_m_lightpole_on,
                            &g_m_lightpole_off, &g_m_cone, &g_m_roof_ac};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        model_free(*all[i]);
        *all[i] = NULL;
    }
    for (int i = 0; i < 3; i++) { model_free(g_m_car[i]); g_m_car[i] = NULL; }
    for (int i = 0; i < 3; i++) { model_free(g_m_debris[i]); g_m_debris[i] = NULL; }
    (void)place_model_nofree;
    g_b = NULL;
}

void level_free(Level *lv) {
    for (int i = 0; i < lv->mesh_count; i++) mesh_free(lv->meshes[i]);
    lv->mesh_count = 0;
    nav_free(&lv->nav);
}

// ---------------------------------------------------------------------------
// Collision queries
// ---------------------------------------------------------------------------
int level_point_blocked(const Level *lv, Vec3 p, float radius) {
    for (int i = 0; i < lv->aabb_count; i++) {
        const AABB *a = &lv->aabbs[i];
        if (!a->blocks_move) continue;
        if (p.y + 1.7f < a->ymin || p.y > a->ymax) continue;
        float cx = clampf(p.x, a->mn.x, a->mx.x);
        float cz = clampf(p.z, a->mn.y, a->mx.y);
        float dx = p.x - cx, dz = p.z - cz;
        if (dx * dx + dz * dz < radius * radius) return 1;
    }
    return 0;
}

int nav_line_clear(const Level *lv, Vec3 a, Vec3 b, float radius) {
    Vec3 d = vsub(b, a);
    float dist = vlen(d);
    if (dist < 1e-4f) return 1;
    Vec3 dir = vscale(d, 1.f / dist);
    int steps = (int)(dist / 0.35f) + 1;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        Vec3 p = vmad(a, dir, dist * t);
        p.y = a.y;
        if (level_point_blocked(lv, p, radius * 0.85f)) return 0;
    }
    return 1;
}
