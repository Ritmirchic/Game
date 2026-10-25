// ============================================================================
// THE LAST NIGHT — renderer implementation
// Geometry G-buffer -> lighting (baked + dynamic + screen-space shadows)
// -> transparent/particles -> bloom -> filmic tonemap -> 2D overlay
// ============================================================================
#include "render.h"
#include "threads.h"
#include <string.h>
#include <stdlib.h>

Tex *tex_load_png(const char *path, const char *name);
uint8_t *png_load_file(const char *path, int *w, int *h);

// ============================================================ textures
Tex *tex_create(int w, int h, const char *name) {
    Tex *t = (Tex *)calloc(1, sizeof(Tex));
    t->w = w; t->h = h;
    t->px = (uint8_t *)calloc((size_t)w * h * 4, 1);
    snprintf(t->name, sizeof t->name, "%s", name ? name : "tex");
    return t;
}
void tex_build_mips(Tex *t) {
    if (!t || !t->px) return;
    t->nmips = 1;
    t->mip[0] = t->px;
    t->mw[0] = t->w;
    t->mh[0] = t->h;
    int w = t->w, h = t->h;
    while (t->nmips < MAX_MIPS && w > 4 && h > 4) {
        int nw = w / 2, nh = h / 2;
        uint8_t *dst = (uint8_t *)calloc((size_t)nw * nh * 4, 1);
        if (!dst) break;
        const uint8_t *src = t->mip[t->nmips - 1];
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++) {
                int o = (y * nw + x) * 4;
                const uint8_t *s0 = src + ((size_t)(y * 2) * w + (size_t)(x * 2)) * 4;
                const uint8_t *s1 = s0 + 4;
                const uint8_t *s2 = s0 + (size_t)w * 4;
                const uint8_t *s3 = s2 + 4;
                for (int c = 0; c < 4; c++)
                    dst[o + c] = (uint8_t)((s0[c] + s1[c] + s2[c] + s3[c] + 2) >> 2);
            }
        t->mip[t->nmips] = dst;
        t->mw[t->nmips] = nw;
        t->mh[t->nmips] = nh;
        t->nmips++;
        w = nw;
        h = nh;
    }
}
Tex *tex_load_png(const char *path, const char *name) {
    int w = 0, h = 0;
    uint8_t *px = png_load_file(path, &w, &h);
    if (!px) return NULL;
    Tex *t = (Tex *)calloc(1, sizeof(Tex));
    t->w = w; t->h = h; t->px = px;
    snprintf(t->name, sizeof t->name, "%s", name ? name : path);
    tex_build_mips(t);
    return t;
}
void tex_free(Tex *t) {
    if (!t) return;
    for (int i = 1; i < t->nmips; i++) free(t->mip[i]);
    free(t->px);
    free(t);
}
void tex_sample_lod(const Tex *t, float u, float v, float lod, float *out) {
    if (!t || !t->px) { out[0] = out[1] = out[2] = out[3] = 1.f; return; }
    if (t->nmips <= 1) { tex_sample(t, u, v, out); return; }
    if (lod <= 0.f) { tex_sample(t, u, v, out); return; }
    if (lod >= (float)(t->nmips - 1)) lod = (float)(t->nmips - 1) - 0.001f;
    int l0 = (int)lod;
    float f = lod - (float)l0;
    int w0 = t->mw[l0], h0 = t->mh[l0];
    int w1 = t->mw[l0 + 1], h1 = t->mh[l0 + 1];
    const uint8_t *p0 = t->mip[l0];
    const uint8_t *p1 = t->mip[l0 + 1];
    float uu = u - floorf(u), vv = v - floorf(v);
    float fx0 = uu * (float)w0 - 0.5f, fy0 = vv * (float)h0 - 0.5f;
    float fx1 = uu * (float)w1 - 0.5f, fy1 = vv * (float)h1 - 0.5f;
    int ax = (int)floorf(fx0), ay = (int)floorf(fy0);
    int bx = (int)floorf(fx1), by = (int)floorf(fy1);
    float tx0 = fx0 - (float)ax, ty0 = fy0 - (float)ay;
    float tx1 = fx1 - (float)bx, ty1 = fy1 - (float)by;
    int ax1 = ((ax + 1) % w0 + w0) % w0, ay1 = ((ay + 1) % h0 + h0) % h0;
    int bx1 = ((bx + 1) % w1 + w1) % w1, by1 = ((by + 1) % h1 + h1) % h1;
    ax = ((ax % w0) + w0) % w0; ay = ((ay % h0) + h0) % h0;
    bx = ((bx % w1) + w1) % w1; by = ((by % h1) + h1) % h1;
    const uint8_t *c0 = p0 + ((size_t)ay * w0 + ax) * 4;
    const uint8_t *c1 = p0 + ((size_t)ay * w0 + ax1) * 4;
    const uint8_t *c2 = p0 + ((size_t)ay1 * w0 + ax) * 4;
    const uint8_t *c3 = p0 + ((size_t)ay1 * w0 + ax1) * 4;
    const uint8_t *d0 = p1 + ((size_t)by * w1 + bx) * 4;
    const uint8_t *d1 = p1 + ((size_t)by * w1 + bx1) * 4;
    const uint8_t *d2 = p1 + ((size_t)by1 * w1 + bx) * 4;
    const uint8_t *d3 = p1 + ((size_t)by1 * w1 + bx1) * 4;
    for (int i = 0; i < 4; i++) {
        float t0 = c0[i] + (c1[i] - c0[i]) * tx0;
        float t1 = c2[i] + (c3[i] - c2[i]) * tx0;
        float va = t0 + (t1 - t0) * ty0;
        float s0 = d0[i] + (d1[i] - d0[i]) * tx1;
        float s1 = d2[i] + (d3[i] - d2[i]) * tx1;
        float vb = s0 + (s1 - s0) * ty1;
        out[i] = (va + (vb - va) * f) * (1.f / 255.f);
    }
}
void tex_sample(const Tex *t, float u, float v, float *out) {
    if (!t) { out[0] = out[1] = out[2] = 1.f; out[3] = 1.f; return; }
    u = u - floorf(u);
    v = v - floorf(v);
    float fx = u * (float)t->w - 0.5f, fy = v * (float)t->h - 0.5f;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    float tx = fx - (float)x0, ty = fy - (float)y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = ((x0 % t->w) + t->w) % t->w; x1 = ((x1 % t->w) + t->w) % t->w;
    y0 = ((y0 % t->h) + t->h) % t->h; y1 = ((y1 % t->h) + t->h) % t->h;
    const uint8_t *a = t->px + ((size_t)y0 * t->w + x0) * 4;
    const uint8_t *b = t->px + ((size_t)y0 * t->w + x1) * 4;
    const uint8_t *c = t->px + ((size_t)y1 * t->w + x0) * 4;
    const uint8_t *d = t->px + ((size_t)y1 * t->w + x1) * 4;
    for (int i = 0; i < 4; i++) {
        float top = a[i] + (b[i] - a[i]) * tx;
        float bot = c[i] + (d[i] - c[i]) * tx;
        out[i] = (top + (bot - top) * ty) * (1.f / 255.f);
    }
}
uint32_t tex_sample_u32(const Tex *t, float u, float v) {
    float f[4];
    tex_sample(t, u, v, f);
    return rgbaf(f[0], f[1], f[2], f[3]);
}

Material mat_make(const char *name, const Tex *albedo, const Tex *normal, Vec3 tint, float rough, float metal, float emissive, int flags) {
    Material m;
    memset(&m, 0, sizeof m);
    m.albedo = albedo; m.normal = normal; m.tint = tint;
    m.rough = rough; m.metal = metal; m.emissive = emissive;
    m.uvscale = 1.f; m.flags = flags;
    snprintf(m.name, sizeof m.name, "%s", name ? name : "mat");
    return m;
}

// ============================================================ meshes
static void mesh_grow_verts(Mesh *m, uint32_t need) {
    if (need <= m->vert_cap) return;
    uint32_t cap = m->vert_cap ? m->vert_cap : 64;
    while (cap < need) cap *= 2;
    m->pos = (Vec3 *)realloc(m->pos, cap * sizeof(Vec3));
    m->nrm = (Vec3 *)realloc(m->nrm, cap * sizeof(Vec3));
    m->uv = (float *)realloc(m->uv, (size_t)cap * 2 * sizeof(float));
    m->lit = (uint8_t *)realloc(m->lit, (size_t)cap * 3);
    m->vert_cap = cap;
}
static void mesh_grow_tris(Mesh *m, uint32_t need) {
    if (need <= m->tri_cap) return;
    uint32_t cap = m->tri_cap ? m->tri_cap : 128;
    while (cap < need) cap *= 2;
    m->idx = (uint32_t *)realloc(m->idx, (size_t)cap * 3 * sizeof(uint32_t));
    m->tri_cap = cap;
}
Mesh *mesh_new(uint32_t maxverts, uint32_t maxtris) {
    Mesh *m = (Mesh *)calloc(1, sizeof(Mesh));
    m->mat = 0; m->dynamic = 1;
    mesh_grow_verts(m, maxverts < 8 ? 8 : maxverts);
    mesh_grow_tris(m, maxtris < 8 ? 8 : maxtris);
    m->bbmin = v3s(1e30f); m->bbmax = v3s(-1e30f);
    return m;
}
void mesh_free(Mesh *m) {
    if (!m) return;
    free(m->pos); free(m->nrm); free(m->uv); free(m->lit); free(m->idx);
    free(m);
}
static uint32_t mesh_push_vert(Mesh *m, Vec3 p, Vec3 n, float u, float v, uint8_t lr, uint8_t lg, uint8_t lb) {
    uint32_t i = m->vert_used;
    mesh_grow_verts(m, i + 1);
    m->pos[i] = p; m->nrm[i] = n;
    m->uv[i * 2] = u; m->uv[i * 2 + 1] = v;
    m->lit[i * 3] = lr; m->lit[i * 3 + 1] = lg; m->lit[i * 3 + 2] = lb;
    m->vert_used++;
    m->bbmin = vmin3(m->bbmin, p);
    m->bbmax = vmax3(m->bbmax, p);
    return i;
}
void mesh_add_tri(Mesh *m, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 n0, Vec3 n1, Vec3 n2,
                  float u0, float v0, float u1, float v1, float u2, float v2,
                  uint8_t lr, uint8_t lg, uint8_t lb) {
    uint32_t a = mesh_push_vert(m, p0, n0, u0, v0, lr, lg, lb);
    uint32_t b = mesh_push_vert(m, p1, n1, u1, v1, lr, lg, lb);
    uint32_t c = mesh_push_vert(m, p2, n2, u2, v2, lr, lg, lb);
    mesh_grow_tris(m, m->tri_used + 1);
    m->idx[m->tri_used * 3] = a; m->idx[m->tri_used * 3 + 1] = b; m->idx[m->tri_used * 3 + 2] = c;
    m->tri_used++;
}
void mesh_add_quad(Mesh *m, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, float us, float vs,
                   uint8_t lr, uint8_t lg, uint8_t lb) {
    mesh_add_tri(m, p0, p1, p2, n, n, n, 0, 0, us, 0, us, vs, lr, lg, lb);
    mesh_add_tri(m, p0, p2, p3, n, n, n, 0, 0, us, vs, 0, vs, lr, lg, lb);
}
void mesh_add_boxf(Mesh *m, Vec3 mn, Vec3 mx, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale) {
    float sx = (mx.x - mn.x) * uvscale, sy = (mx.y - mn.y) * uvscale, sz = (mx.z - mn.z) * uvscale;
    mesh_add_quad(m, v3(mx.x, mn.y, mx.z), v3(mx.x, mn.y, mn.z), v3(mx.x, mx.y, mn.z), v3(mx.x, mx.y, mx.z), v3(1, 0, 0), sz, sy, lr, lg, lb);
    mesh_add_quad(m, v3(mn.x, mn.y, mn.z), v3(mn.x, mn.y, mx.z), v3(mn.x, mx.y, mx.z), v3(mn.x, mx.y, mn.z), v3(-1, 0, 0), sz, sy, lr, lg, lb);
    mesh_add_quad(m, v3(mn.x, mx.y, mx.z), v3(mx.x, mx.y, mx.z), v3(mx.x, mx.y, mn.z), v3(mn.x, mx.y, mn.z), v3(0, 1, 0), sx, sz, lr, lg, lb);
    mesh_add_quad(m, v3(mn.x, mn.y, mn.z), v3(mx.x, mn.y, mn.z), v3(mx.x, mn.y, mx.z), v3(mn.x, mn.y, mx.z), v3(0, -1, 0), sx, sz, lr, lg, lb);
    mesh_add_quad(m, v3(mn.x, mn.y, mx.z), v3(mx.x, mn.y, mx.z), v3(mx.x, mx.y, mx.z), v3(mn.x, mx.y, mx.z), v3(0, 0, 1), sx, sy, lr, lg, lb);
    mesh_add_quad(m, v3(mx.x, mn.y, mn.z), v3(mn.x, mn.y, mn.z), v3(mn.x, mx.y, mn.z), v3(mx.x, mx.y, mn.z), v3(0, 0, -1), sx, sy, lr, lg, lb);
}
void mesh_add_box(Mesh *m, Vec3 c, Vec3 hs, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale) {
    mesh_add_boxf(m, vsub(c, hs), vadd(c, hs), lr, lg, lb, uvscale);
}
void mesh_set_lit_all(Mesh *m, uint8_t r, uint8_t g, uint8_t b) {
    for (uint32_t i = 0; i < m->vert_used; i++) {
        m->lit[i * 3] = r; m->lit[i * 3 + 1] = g; m->lit[i * 3 + 2] = b;
    }
}
void mesh_apply_xform(Mesh *m, Mat4 xf) {
    for (uint32_t i = 0; i < m->vert_used; i++) {
        m->pos[i] = mat4_mul_point(xf, m->pos[i]);
        m->nrm[i] = vnorm(mat4_mul_dir(xf, m->nrm[i]));
    }
    m->bbmin = v3s(1e30f); m->bbmax = v3s(-1e30f);
    for (uint32_t i = 0; i < m->vert_used; i++) {
        m->bbmin = vmin3(m->bbmin, m->pos[i]);
        m->bbmax = vmax3(m->bbmax, m->pos[i]);
    }
}

// ============================================================ renderer setup
static float *falloc(size_t n) {
    float *p = (float *)malloc(n * sizeof(float) + 64);
    if (!p) { LOGE("renderer OOM (%zu floats)", n); exit(1); }
    return p;
}
static void free_buffers(Renderer *r) {
    free(r->gb_depth); free(r->gb_pos); free(r->gb_nrm); free(r->gb_alb);
    free(r->gb_lit); free(r->gb_emit); free(r->gb_rough); free(r->gb_sky);
    free(r->hdr); free(r->color); free(r->bloom_a); free(r->bloom_b); free(r->shadow_mask);
    r->shadow_mask = NULL;
    memset(&r->gb_depth, 0, sizeof(r->gb_depth));
    r->gb_pos = NULL; r->gb_nrm = NULL; r->gb_alb = NULL; r->gb_lit = NULL;
    r->gb_emit = NULL; r->gb_rough = NULL; r->gb_sky = NULL;
    r->hdr = NULL; r->color = NULL; r->bloom_a = NULL; r->bloom_b = NULL;
}
static void make_soft_particle(Renderer *r) {
    int s = 64;
    r->soft_particle = tex_create(s, s, "soft_particle");
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            float dx = (x + 0.5f) / s * 2.f - 1.f, dy = (y + 0.5f) / s * 2.f - 1.f;
            float d = sqrtf(dx * dx + dy * dy);
            float a = powf(saturatef(1.f - d), 2.1f);
            uint8_t *p = r->soft_particle->px + ((size_t)y * s + x) * 4;
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(a * 255.f);
        }
}
int rd_init(Renderer *r, int w, int h) {
    memset(r, 0, sizeof *r);
    rd_resize(r, w, h);
    make_soft_particle(r);
    { int nc = sys_cpu_count(); r->chunks = clampi(nc * 2, 1, 16); }
    return 1;
}
void rd_resize(Renderer *r, int w, int h) {
    if (w < 64) w = 64;
    if (h < 64) h = 64;
    free_buffers(r);
    r->w = w; r->h = h;
    size_t n = (size_t)w * h;
    r->gb_depth = falloc(n);
    r->gb_pos = (Vec3 *)calloc(n, sizeof(Vec3));
    r->gb_nrm = (Vec3 *)calloc(n, sizeof(Vec3));
    r->gb_alb = (Vec3 *)calloc(n, sizeof(Vec3));
    r->gb_lit = (Vec3 *)calloc(n, sizeof(Vec3));
    r->gb_emit = (Vec3 *)calloc(n, sizeof(Vec3));
    r->gb_rough = falloc(n);
    r->gb_sky = (uint8_t *)calloc(n, 1);
    r->hdr = falloc(n * 3);
    r->color = (uint32_t *)calloc(n, 4);
    r->bw = (w + 3) / 4; r->bh = (h + 3) / 4;
    r->bloom_a = falloc((size_t)r->bw * r->bh * 3);
    r->bloom_b = falloc((size_t)r->bw * r->bh * 3);
    r->sm_w = (w + 3) / 4; r->sm_h = (h + 3) / 4;
    r->shadow_mask = (float *)calloc((size_t)r->sm_w * r->sm_h * MAX_LIGHTS, sizeof(float));
    LOGI("renderer: %dx%d  gbuffer %d KB", w, h, (int)((n * 60) / 1024));
}
void rd_shutdown(Renderer *r) {
    free_buffers(r);
    if (r->soft_particle) { tex_free(r->soft_particle); r->soft_particle = NULL; }
}

// ============================================================ sky
static Vec3 sky_color(const Environment *e, Vec3 d) {
    float y = clampf(d.y, -1.f, 1.f);
    Vec3 zenith = v3(0.010f, 0.014f, 0.030f);
    Vec3 horizon = v3(0.050f, 0.058f, 0.082f);
    float skyT = powf(saturatef(1.f - fabsf(y)), 2.2f);
    Vec3 col = vlerp(zenith, horizon, skyT);
    col = vmul(col, e->sky_tint);
    // storm clouds
    float cx = d.x / (0.16f + fabsf(y) + 0.04f);
    float cz = d.z / (0.16f + fabsf(y) + 0.04f);
    float cl = fbm2(cx * 2.2f + e->time * 0.008f, cz * 2.2f + e->time * 0.003f, 4, 11);
    cl = smoothstepf(0.40f, 0.80f, cl) * smoothstepf(-0.02f, 0.30f, y);
    float cloudLight = 0.16f + 0.10f * e->sky_moon_intensity;
    Vec3 cloud_col = v3(cloudLight, cloudLight * 1.06f, cloudLight * 1.25f);
    col = vlerp(col, cloud_col, cl * 0.92f * (0.6f + 0.4f * e->rain));
    // moon + halo
    Vec3 moon = vnorm(v3(-0.42f, 0.30f, 0.86f));
    float md = maxf(vdot(d, moon), 0.f);
    float disc = smoothstepf(0.9988f, 0.9995f, md);
    float glow = powf(md, 220.f) * 0.5f + powf(md, 24.f) * 0.10f + powf(md, 6.f) * 0.03f;
    col = vadd(col, vscale(v3(0.62f, 0.70f, 0.92f), (disc * 0.55f + glow * 0.6f) * e->sky_moon_intensity * (1.f - 0.65f * e->rain)));
    // city light pollution at horizon
    float haze = powf(saturatef(1.f - fabsf(y)), 22.f);
    col = vadd(col, vscale(v3(0.075f, 0.052f, 0.033f), haze * 0.55f));
    // ground beyond the world (below horizon): dark asphalt/earth
    if (y < -0.02f) {
        float g = smoothstepf(-0.02f, -0.35f, y);
        col = vlerp(col, v3(0.013f, 0.014f, 0.013f), g);
    }
    return col;
}

// ============================================================ raster
typedef struct {
    Vec3 vp, wp, n;
    float u, v;
    float lit[3];
} RV;

static inline void project(const Renderer *r, Vec3 vp, float *sx, float *sy, float *iw, float *depth) {
    float d = -vp.z;
    if (d < 1e-4f) d = 1e-4f;
    float inv = 1.f / d;
    float nx = vp.x * r->proj_ax * inv;
    float ny = vp.y * r->proj_f * inv;
    *sx = (nx * 0.5f + 0.5f) * (float)r->w;
    *sy = (0.5f - ny * 0.5f) * (float)r->h;
    *iw = inv;
    *depth = d;
}

static int near_clip(RV *in, int n, RV *out) {
    const float zn = 0.05f;
    int m = 0;
    for (int i = 0; i < n; i++) {
        RV *a = &in[i], *b = &in[(i + 1) % n];
        float da = -a->vp.z - zn;
        float db = -b->vp.z - zn;
        if (da >= 0) out[m++] = *a;
        if ((da >= 0) != (db >= 0)) {
            float t = da / (da - db);
            RV c;
            c.vp = vlerp(a->vp, b->vp, t);
            c.wp = vlerp(a->wp, b->wp, t);
            c.n = vlerp(a->n, b->n, t);
            c.u = lerpf(a->u, b->u, t);
            c.v = lerpf(a->v, b->v, t);
            for (int k = 0; k < 3; k++) c.lit[k] = lerpf(a->lit[k], b->lit[k], t);
            out[m++] = c;
        }
    }
    return m;
}

void rd_draw_mesh(Renderer *r, const Mesh *m, const Mat4 *model, const Material *mats, int mat_count) {
    if (!m || m->tri_used == 0) return;
    const Material *mat = (mats && m->mat < mat_count) ? &mats[m->mat] : NULL;
    Mat4 mv = *model;
    int two = mat ? (mat->flags & MF_TWOSIDED) != 0 : 0;
    float uvscale = mat ? mat->uvscale : 1.f;

    RV poly[8];
    for (uint32_t t = 0; t < m->tri_used; t++) {
        const uint32_t *ix = &m->idx[t * 3];
        RV w3[3];
        for (int k = 0; k < 3; k++) {
            uint32_t vi = ix[k];
            w3[k].wp = mat4_mul_point(mv, m->pos[vi]);
            w3[k].n = vnorm(mat4_mul_dir(mv, m->nrm[vi]));
            w3[k].u = m->uv[vi * 2] * uvscale;
            w3[k].v = m->uv[vi * 2 + 1] * uvscale;
            w3[k].lit[0] = m->lit[vi * 3];
            w3[k].lit[1] = m->lit[vi * 3 + 1];
            w3[k].lit[2] = m->lit[vi * 3 + 2];
            w3[k].vp = mat4_mul_point(r->view, w3[k].wp);
        }
        if (!two) {
            Vec3 e1 = vsub(w3[1].vp, w3[0].vp), e2 = vsub(w3[2].vp, w3[0].vp);
            Vec3 cr = vcross(e1, e2);
            Vec3 ctr = vscale(vadd(vadd(w3[0].vp, w3[1].vp), w3[2].vp), 1.f / 3.f);
            if (vdot(cr, vneg(ctr)) <= 0.f) { r->tris_culled++; continue; }
        }
        // whole-triangle offscreen reject
        int np = near_clip(w3, 3, poly);
        if (np < 3) { r->tris_culled++; continue; }
        for (int fan = 2; fan < np; fan++) {
            RV tri[3] = {poly[0], poly[fan - 1], poly[fan]};
            r->tris_drawn++;
            float sx[3], sy[3], iw[3], dep[3];
            for (int k = 0; k < 3; k++) project(r, tri[k].vp, &sx[k], &sy[k], &iw[k], &dep[k]);
            float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (fabsf(area) < 1e-7f) continue;
            float invarea = 1.f / area;
            int minx = clampi((int)floorf(minf(sx[0], minf(sx[1], sx[2]))), 0, r->w - 1);
            int maxx = clampi((int)ceilf(maxf(sx[0], maxf(sx[1], sx[2]))), 0, r->w - 1);
            int miny = clampi((int)floorf(minf(sy[0], minf(sy[1], sy[2]))), 0, r->h - 1);
            int maxy = clampi((int)ceilf(maxf(sy[0], maxf(sy[1], sy[2]))), 0, r->h - 1);
            if (minx > maxx || miny > maxy) continue;
            // per-triangle texture LOD from the UV-to-screen area ratio
            float lod = 0.f, lodn = 0.f;
            if (mat && mat->albedo) {
                float du1 = tri[1].u - tri[0].u, dv1 = tri[1].v - tri[0].v;
                float du2 = tri[2].u - tri[0].u, dv2 = tri[2].v - tri[0].v;
                float uvarea = fabsf(du1 * dv2 - du2 * dv1);
                float tarea = uvarea * (float)mat->albedo->w * (float)mat->albedo->h;
                float sarea = fabsf(area);
                if (tarea > 1e-6f && sarea > 1e-6f) {
                    lod = 0.5f * log2f(tarea / sarea);
                    if (lod < 0.f) lod = 0.f;
                    if (lod > (float)(mat->albedo->nmips - 1)) lod = (float)(mat->albedo->nmips - 1) - 0.01f;
                }
                lodn = lod + 1.0f;
            }

            Vec3 T = v3(1, 0, 0), B = v3(0, 0, 1);
            if (mat && mat->normal) {
                Vec3 e1w = vsub(tri[1].wp, tri[0].wp), e2w = vsub(tri[2].wp, tri[0].wp);
                float du1 = tri[1].u - tri[0].u, dv1 = tri[1].v - tri[0].v;
                float du2 = tri[2].u - tri[0].u, dv2 = tri[2].v - tri[0].v;
                float det = du1 * dv2 - du2 * dv1;
                if (fabsf(det) > 1e-6f) {
                    float f = 1.f / det;
                    T = vnorm(vscale(vsub(vscale(e1w, dv2), vscale(e2w, dv1)), f));
                    B = vnorm(vscale(vsub(vscale(e2w, du1), vscale(e1w, du2)), f));
                }
            }
            int emissive_only = mat ? (mat->flags & MF_EMISSIVE) && mat->albedo == NULL : 0;
            for (int y = miny; y <= maxy; y++) {
                float py = (float)y + 0.5f;
                for (int x = minx; x <= maxx; x++) {
                    float px = (float)x + 0.5f;
                    float b0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * invarea;
                    float b1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * invarea;
                    float b2 = 1.f - b0 - b1;
                    if (b0 < 0.f || b1 < 0.f || b2 < 0.f) continue;
                    float wsum = b0 * iw[0] + b1 * iw[1] + b2 * iw[2];
                    if (wsum <= 1e-8f) continue;
                    float depth = 1.f / wsum;
                    size_t o = (size_t)y * r->w + x;
                    if (depth >= r->gb_depth[o]) continue;
                    float inv = 1.f / wsum;
                    float u = (b0 * tri[0].u * iw[0] + b1 * tri[1].u * iw[1] + b2 * tri[2].u * iw[2]) * inv;
                    float v = (b0 * tri[0].v * iw[0] + b1 * tri[1].v * iw[1] + b2 * tri[2].v * iw[2]) * inv;
                    Vec3 wp = v3(
                        (b0 * tri[0].wp.x * iw[0] + b1 * tri[1].wp.x * iw[1] + b2 * tri[2].wp.x * iw[2]) * inv,
                        (b0 * tri[0].wp.y * iw[0] + b1 * tri[1].wp.y * iw[1] + b2 * tri[2].wp.y * iw[2]) * inv,
                        (b0 * tri[0].wp.z * iw[0] + b1 * tri[1].wp.z * iw[1] + b2 * tri[2].wp.z * iw[2]) * inv);
                    Vec3 nn = vnorm(v3(
                        (b0 * tri[0].n.x * iw[0] + b1 * tri[1].n.x * iw[1] + b2 * tri[2].n.x * iw[2]) * inv,
                        (b0 * tri[0].n.y * iw[0] + b1 * tri[1].n.y * iw[1] + b2 * tri[2].n.y * iw[2]) * inv,
                        (b0 * tri[0].n.z * iw[0] + b1 * tri[1].n.z * iw[1] + b2 * tri[2].n.z * iw[2]) * inv));
                    if (emissive_only) nn = tri[0].n;

                    Vec3 alb = mat ? mat->tint : v3s(0.7f);
                    Vec3 emit = v3s(0.f);
                    float rough = mat ? mat->rough : 0.85f;
                    if (mat && mat->albedo) {
                        float t4[4];
                        tex_sample_lod(mat->albedo, u, v, lod, t4);
                        if ((mat->flags & MF_ALPHATEST) && t4[3] < 0.4f) continue;
                        alb = v3(alb.x * t4[0], alb.y * t4[1], alb.z * t4[2]);
                    }
                    if (mat && mat->emissive > 0.f) emit = vscale(alb, mat->emissive);
                    if (mat && mat->normal) {
                        float t4[4];
                        tex_sample_lod(mat->normal, u, v, lodn, t4);
                        Vec3 nt = v3(t4[0] * 2.f - 1.f, t4[1] * 2.f - 1.f, t4[2] * 2.f - 1.f);
                        nn = vnorm(vadd(vadd(vscale(T, nt.x), vscale(nn, nt.z)), vscale(B, nt.y)));
                    }
                    if (two) {
                        Vec3 tocam = vsub(r->cam.pos, wp);
                        if (vdot(nn, tocam) < 0.f) nn = vneg(nn);
                    }
                    r->gb_depth[o] = depth;
                    r->gb_pos[o] = wp;
                    r->gb_nrm[o] = nn;
                    r->gb_alb[o] = alb;
                    r->gb_lit[o] = v3(
                        (b0 * tri[0].lit[0] + b1 * tri[1].lit[0] + b2 * tri[2].lit[0]) * (1.f / 255.f),
                        (b0 * tri[0].lit[1] + b1 * tri[1].lit[1] + b2 * tri[2].lit[1]) * (1.f / 255.f),
                        (b0 * tri[0].lit[2] + b1 * tri[1].lit[2] + b2 * tri[2].lit[2]) * (1.f / 255.f));
                    r->gb_emit[o] = emit;
                    r->gb_rough[o] = rough;
                    r->gb_sky[o] = 0;
                }
            }
        }
    }
}

// ============================================================ frame
void rd_begin(Renderer *r, Camera *cam, Environment *env) {
    r->cam = *cam;
    if (r->cam.fov <= 0.01f) r->cam.fov = DEG2RAD(72.f);
    r->env = *env;
    float yaw = cam->yaw + cam->sway_x, pitch = cam->pitch + cam->sway_y;
    Mat4 rot = mat4_mul(mat4_rot_x(-pitch), mat4_rot_y(-yaw));
    if (cam->roll != 0.f) rot = mat4_mul(rot, mat4_rot_z(-cam->roll));
    r->view_rot = rot;
    r->view_rot_t = mat4_transpose(rot);
    r->view = mat4_mul(rot, mat4_translate(vneg(cam->pos)));
    r->proj_f = 1.f / tanf(r->cam.fov * 0.5f);
    r->proj_ax = r->proj_f / ((float)r->w / (float)r->h);
    r->tris_drawn = 0; r->tris_culled = 0;
    size_t n = (size_t)r->w * r->h;
    memset(r->gb_sky, 1, n);
    for (size_t i = 0; i < n; i++) {
        r->gb_depth[i] = 1e30f;
        r->gb_lit[i] = v3s(0.f);
        r->gb_emit[i] = v3s(0.f);
        r->gb_alb[i] = v3s(0.f);
    }
    r->nlights = 0;
}
void rd_add_light(Renderer *r, Light l) {
    if (r->nlights >= MAX_LIGHTS) return;
    r->lights[r->nlights++] = l;
}
void rd_clear_lights(Renderer *r) { r->nlights = 0; }

static int ss_occluded(const Renderer *r, Vec3 P) {
    Vec3 d = vsub(P, r->cam.pos);
    float dist = vlen(d);
    if (dist < 0.5f) return 0;
    d = vscale(d, 1.f / dist);
    const int steps = 9;
    float t = 0.30f;
    float step = (dist - 0.30f) / (float)steps;
    if (step < 0.02f) return 0;
    for (int i = 0; i < steps; i++) {
        t += step;
        Vec3 p = vmad(r->cam.pos, d, t);
        Vec3 vp = mat4_mul_point(r->view, p);
        if (-vp.z <= 0.12f) continue;
        float sx, sy, iw, dep;
        project(r, vp, &sx, &sy, &iw, &dep);
        int x = (int)sx, y = (int)sy;
        if (x < 1 || y < 1 || x >= r->w - 1 || y >= r->h - 1) continue;
        size_t o = (size_t)y * r->w + x;
        if (r->gb_sky[o]) continue;
        if (r->gb_depth[o] < t - 0.30f) return 1;
    }
    return 0;
}

static inline Vec3 aces_tonemap(Vec3 x) {
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return v3(
        (x.x * (a * x.x + b)) / (x.x * (c * x.x + d) + e),
        (x.y * (a * x.y + b)) / (x.y * (c * x.y + d) + e),
        (x.z * (a * x.z + b)) / (x.z * (c * x.z + d) + e));
}

static float g_gamma_lut[1025];
static int g_gamma_init = 0;
static void gamma_lut_init(void) {
    for (int i = 0; i <= 1024; i++) g_gamma_lut[i] = powf((float)i / 1024.f, 1.f / 2.2f);
    g_gamma_init = 1;
}
static inline float gamma1(float x) {
    float f = saturatef(x) * 1024.f;
    int i = (int)f;
    if (i >= 1024) return 1.f;
    float t = f - (float)i;
    return g_gamma_lut[i] + (g_gamma_lut[i + 1] - g_gamma_lut[i]) * t;
}

#define SKY_LUT_W 96
#define SKY_LUT_H 54
static void build_sky_lut(Renderer *r) {
    const Environment *env = &r->env;
    float tf = tanf(r->cam.fov * 0.5f);
    float aspect = (float)r->w / (float)r->h;
    for (int y = 0; y < SKY_LUT_H; y++)
        for (int x = 0; x < SKY_LUT_W; x++) {
            float px = ((float)x + 0.5f) / SKY_LUT_W * 2.f - 1.f;
            float py = 1.f - ((float)y + 0.5f) / SKY_LUT_H * 2.f;
            Vec3 vdir = v3(px * tf * aspect, py * tf, -1.f);
            r->sky_lut[y * SKY_LUT_W + x] = sky_color(env, vnorm(mat4_mul_dir(r->view_rot_t, vdir)));
        }
    r->sky_lut_w = SKY_LUT_W; r->sky_lut_h = SKY_LUT_H;
}
static inline Vec3 sample_sky_lut(const Renderer *r, int x, int y) {
    float u = ((float)x + 0.5f) / (float)r->w * SKY_LUT_W - 0.5f;
    float v = ((float)y + 0.5f) / (float)r->h * SKY_LUT_H - 0.5f;
    int x0 = (int)floorf(u), y0 = (int)floorf(v);
    float tx = u - x0, ty = v - y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = clampi(x0, 0, SKY_LUT_W - 1); x1 = clampi(x1, 0, SKY_LUT_W - 1);
    y0 = clampi(y0, 0, SKY_LUT_H - 1); y1 = clampi(y1, 0, SKY_LUT_H - 1);
    Vec3 a = vlerp(r->sky_lut[y0 * SKY_LUT_W + x0], r->sky_lut[y0 * SKY_LUT_W + x1], tx);
    Vec3 b = vlerp(r->sky_lut[y1 * SKY_LUT_W + x0], r->sky_lut[y1 * SKY_LUT_W + x1], tx);
    return vlerp(a, b, ty);
}

typedef struct { Renderer *r; } RowCtx;
typedef struct { Renderer *r; int li; } MaskCtx;

// Build a quarter-res soft shadow mask for light li by marching camera->pixel points.
static void shadow_mask_rows(void *vctx, int y0, int y1) {
    MaskCtx *mc = (MaskCtx *)vctx;
    Renderer *r = mc->r;
    const Light *L = &r->lights[mc->li];
    float *mask = r->shadow_mask + (size_t)mc->li * r->sm_w * r->sm_h;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < r->sm_w; x++) {
            mask[(size_t)y * r->sm_w + x] = 0.f;
            int sx = clampi(x * 4 + 2, 0, r->w - 1), sy = clampi(y * 4 + 2, 0, r->h - 1);
            size_t o = (size_t)sy * r->w + sx;
            if (r->gb_sky[o]) continue;
            Vec3 P = r->gb_pos[o];
            if (vlen2(vsub(L->pos, P)) > L->range * L->range) continue;
            mask[(size_t)y * r->sm_w + x] = ss_occluded(r, P) ? 1.f : 0.f;
        }
}
static inline float sample_shadow_mask(const Renderer *r, int li, int x, int y) {
    const float *mask = r->shadow_mask + (size_t)li * r->sm_w * r->sm_h;
    float u = ((float)x + 0.5f) / 4.f - 0.5f, v = ((float)y + 0.5f) / 4.f - 0.5f;
    int x0 = (int)floorf(u), y0 = (int)floorf(v);
    float tx = u - x0, ty = v - y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = clampi(x0, 0, r->sm_w - 1); x1 = clampi(x1, 0, r->sm_w - 1);
    y0 = clampi(y0, 0, r->sm_h - 1); y1 = clampi(y1, 0, r->sm_h - 1);
    float a = mask[(size_t)y0 * r->sm_w + x0] * (1.f - tx) + mask[(size_t)y0 * r->sm_w + x1] * tx;
    float b = mask[(size_t)y1 * r->sm_w + x0] * (1.f - tx) + mask[(size_t)y1 * r->sm_w + x1] * tx;
    return a + (b - a) * ty;
}

static void lighting_rows(void *vctx, int y0, int y1) {
    Renderer *r = ((RowCtx *)vctx)->r;
    const Camera *cam = &r->cam;
    const Environment *env = &r->env;
    for (int y = y0; y < y1; y++) {
        for (int x = 0; x < r->w; x++) {
            size_t o = (size_t)y * r->w + x;
            if (r->gb_sky[o]) {
                Vec3 sk = sample_sky_lut(r, x, y);
                r->hdr[o * 3] = sk.x; r->hdr[o * 3 + 1] = sk.y; r->hdr[o * 3 + 2] = sk.z;
                continue;
            }
            Vec3 P = r->gb_pos[o];
            Vec3 N = r->gb_nrm[o];
            Vec3 alb = r->gb_alb[o];
            Vec3 emit = r->gb_emit[o];
            float rough = r->gb_rough[o];
            Vec3 baked = r->gb_lit[o];
            Vec3 col = v3(alb.x * baked.x, alb.y * baked.y, alb.z * baked.z);
            col = vadd(col, emit);
            Vec3 V = vnorm(vsub(cam->pos, P));
            float shin = 4.f + 120.f * (1.f - rough) * (1.f - rough);
            for (int li = 0; li < r->nlights; li++) {
                const Light *L = &r->lights[li];
                Vec3 toL = vsub(L->pos, P);
                float dist2 = vlen2(toL);
                if (dist2 > L->range * L->range) continue;
                float dist = sqrtf(dist2);
                Vec3 Ld = vscale(toL, 1.f / maxf(dist, 1e-4f));
                float att = 1.f - dist / L->range;
                att *= att;
                if (L->type == 1) {
                    float cd = vdot(vneg(Ld), vnorm(L->dir));
                    att *= smoothstepf(L->cos_outer, L->cos_inner, cd);
                }
                if (att <= 0.0008f) continue;
                float ndl = maxf(vdot(N, Ld), 0.f);
                if (ndl <= 0.f) continue;
                float shadow = 1.f;
                if (L->shadows) {
                    shadow = 1.f - sample_shadow_mask(r, li, x, y) * 0.92f;
                    if (shadow <= 0.08f) continue;
                }
                float k = L->intensity * att * shadow;
                col.x += alb.x * L->color.x * ndl * k;
                col.y += alb.y * L->color.y * ndl * k;
                col.z += alb.z * L->color.z * ndl * k;
                Vec3 H = vnorm(vadd(Ld, V));
                float sp2 = powf(maxf(vdot(N, H), 0.f), shin) * (1.f - rough) * k * (0.25f + 0.75f * (1.f - rough));
                col.x += L->color.x * sp2;
                col.y += L->color.y * sp2;
                col.z += L->color.z * sp2;
            }
            float dcam = vlen(vsub(P, cam->pos));
            float fogAmt = saturatef(1.f - expf(-dcam * env->fog_density));
            col = vlerp(col, env->fog_color, fogAmt);
            r->hdr[o * 3] = col.x; r->hdr[o * 3 + 1] = col.y; r->hdr[o * 3 + 2] = col.z;
        }
    }
}

void rd_lighting(Renderer *r) {
    double t0 = core_now();
    if (!g_gamma_init) gamma_lut_init();
    build_sky_lut(r);
    r->sky_ms = (float)((core_now() - t0) * 1000.0);
    for (int li = 0; li < r->nlights; li++) {
        if (!r->lights[li].shadows) continue;
        MaskCtx mc = {r, li};
        parallel_for(r->sm_h, shadow_mask_rows, &mc, r->chunks);
    }
    RowCtx ctx = {r};
    parallel_for(r->h, lighting_rows, &ctx, r->chunks);
    r->light_ms = (float)((core_now() - t0) * 1000.0);
}

static inline float light_at(const Renderer *r, Vec3 P) {
    float f = 0.30f;
    for (int i = 0; i < r->nlights; i++) {
        const Light *L = &r->lights[i];
        Vec3 d = vsub(L->pos, P);
        float dist = vlen(d);
        if (dist > L->range) continue;
        float att = 1.f - dist / L->range;
        att *= att;
        if (L->type == 1) {
            float cd = vdot(vscale(d, 1.f / maxf(dist, 1e-4f)), vnorm(L->dir));
            att *= smoothstepf(L->cos_outer, L->cos_inner, cd);
        }
        f += att * L->intensity * 0.55f;
    }
    return saturatef(f);
}

static void blend_hdr(Renderer *r, int x, int y, Vec3 c, float a, int additive) {
    if (x < 0 || y < 0 || x >= r->w || y >= r->h) return;
    size_t o = (size_t)y * r->w + x;
    float *h = &r->hdr[o * 3];
    if (additive) {
        h[0] += c.x * a; h[1] += c.y * a; h[2] += c.z * a;
    } else {
        h[0] = lerpf(h[0], c.x, a);
        h[1] = lerpf(h[1], c.y, a);
        h[2] = lerpf(h[2], c.z, a);
    }
}

// generic textured quad in world space (alpha blended into HDR, depth tested)
static void raster_quad_hdr(Renderer *r, const Vec3 wc[4], const float uvc[4][2], const Tex *t,
                            Vec3 color, float alpha, int additive) {
    RV vv[4];
    for (int i = 0; i < 4; i++) {
        vv[i].wp = wc[i];
        vv[i].n = v3(0, 1, 0);
        vv[i].u = uvc[i][0];
        vv[i].v = uvc[i][1];
        vv[i].vp = mat4_mul_point(r->view, wc[i]);
        vv[i].lit[0] = vv[i].lit[1] = vv[i].lit[2] = 255;
    }
    static const int tri[2][3] = {{0, 1, 2}, {0, 2, 3}};
    for (int ti = 0; ti < 2; ti++) {
        RV w3[3] = {vv[tri[ti][0]], vv[tri[ti][1]], vv[tri[ti][2]]};
        RV poly[8];
        int np = near_clip(w3, 3, poly);
        if (np < 3) continue;
        for (int fan = 2; fan < np; fan++) {
            RV t3[3] = {poly[0], poly[fan - 1], poly[fan]};
            float sx[3], sy[3], iw[3], dep[3];
            for (int k = 0; k < 3; k++) project(r, t3[k].vp, &sx[k], &sy[k], &iw[k], &dep[k]);
            float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (fabsf(area) < 1e-7f) continue;
            float invarea = 1.f / area;
            int minx = clampi((int)floorf(minf(sx[0], minf(sx[1], sx[2]))), 0, r->w - 1);
            int maxx = clampi((int)ceilf(maxf(sx[0], maxf(sx[1], sx[2]))), 0, r->w - 1);
            int miny = clampi((int)floorf(minf(sy[0], minf(sy[1], sy[2]))), 0, r->h - 1);
            int maxy = clampi((int)ceilf(maxf(sy[0], maxf(sy[1], sy[2]))), 0, r->h - 1);
            for (int y = miny; y <= maxy; y++) {
                float py = (float)y + 0.5f;
                for (int x = minx; x <= maxx; x++) {
                    float px = (float)x + 0.5f;
                    float b0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * invarea;
                    float b1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * invarea;
                    float b2 = 1.f - b0 - b1;
                    if (b0 < 0.f || b1 < 0.f || b2 < 0.f) continue;
                    float wsum = b0 * iw[0] + b1 * iw[1] + b2 * iw[2];
                    if (wsum <= 1e-8f) continue;
                    float depth = 1.f / wsum;
                    size_t o = (size_t)y * r->w + x;
                    if (!r->gb_sky[o] && r->gb_depth[o] < depth - 0.06f) continue;
                    float inv = 1.f / wsum;
                    float u = (b0 * t3[0].u * iw[0] + b1 * t3[1].u * iw[1] + b2 * t3[2].u * iw[2]) * inv;
                    float v = (b0 * t3[0].v * iw[0] + b1 * t3[1].v * iw[1] + b2 * t3[2].v * iw[2]) * inv;
                    float tc[4] = {1, 1, 1, 1};
                    if (t) tex_sample(t, u, v, tc);
                    float a = tc[3] * alpha;
                    if (a < 0.004f) continue;
                    blend_hdr(r, x, y, v3(color.x * tc[0], color.y * tc[1], color.z * tc[2]), a, additive);
                }
            }
        }
    }
}

static void camera_basis(const Renderer *r, Vec3 *right, Vec3 *up, Vec3 *fwd) {
    Mat4 m = r->view_rot_t;
    *right = v3(m.m[0], m.m[4], m.m[8]);
    *up = v3(m.m[1], m.m[5], m.m[9]);
    *fwd = vneg(v3(m.m[2], m.m[6], m.m[10]));
}

void rd_draw_billboard(Renderer *r, Vec3 pos, float wsize, float hsize, const Tex *t, uint32_t color, int additive, float rot) {
    Vec3 right, up, fwd;
    camera_basis(r, &right, &up, &fwd);
    float cr = cosf(rot), sr = sinf(rot);
    Vec3 rr = vadd(vscale(right, cr), vscale(up, sr));
    Vec3 uu = vadd(vscale(right, -sr), vscale(up, cr));
    float hw = wsize * 0.5f, hh = hsize * 0.5f;
    Vec3 wc[4];
    wc[0] = vadd(vadd(pos, vscale(rr, -hw)), vscale(uu, -hh));
    wc[1] = vadd(vadd(pos, vscale(rr, hw)), vscale(uu, -hh));
    wc[2] = vadd(vadd(pos, vscale(rr, hw)), vscale(uu, hh));
    wc[3] = vadd(vadd(pos, vscale(rr, -hw)), vscale(uu, hh));
    float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    float lit = additive ? 1.f : light_at(r, pos);
    Vec3 cc = v3((float)c_r(color) / 255.f * lit, (float)c_g(color) / 255.f * lit, (float)c_b(color) / 255.f * lit);
    float alpha = (float)c_a(color) / 255.f;
    if (alpha <= 0.004f) return;
    raster_quad_hdr(r, wc, uv, t, cc, alpha, additive);
}

void rd_draw_decal(Renderer *r, Vec3 pos, Vec3 nrm, float size, const Tex *t, uint32_t color) {
    Vec3 up = fabsf(nrm.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    Vec3 right = vnorm(vcross(up, nrm));
    Vec3 upv = vnorm(vcross(nrm, right));
    Vec3 cpos = vmad(pos, nrm, 0.015f);
    float h = size * 0.5f;
    Vec3 wc[4];
    wc[0] = vadd(vadd(cpos, vscale(right, -h)), vscale(upv, -h));
    wc[1] = vadd(vadd(cpos, vscale(right, h)), vscale(upv, -h));
    wc[2] = vadd(vadd(cpos, vscale(right, h)), vscale(upv, h));
    wc[3] = vadd(vadd(cpos, vscale(right, -h)), vscale(upv, h));
    float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    float lit = light_at(r, cpos);
    Vec3 cc = v3((float)c_r(color) / 255.f * lit, (float)c_g(color) / 255.f * lit, (float)c_b(color) / 255.f * lit);
    raster_quad_hdr(r, wc, uv, t, cc, (float)c_a(color) / 255.f, 0);
}

void rd_draw_beam(Renderer *r, Vec3 from, Vec3 to, float radius, Vec3 color, float intensity) {
    Vec3 d = vsub(to, from);
    float len = vlen(d);
    if (len < 0.01f) return;
    int steps = clampi((int)(len * 2.2f), 4, 26);
    for (int i = 0; i < steps; i++) {
        float t = (float)i / (float)(steps - 1);
        Vec3 p = vmad(from, d, t * 0.92f);
        float rad = radius * (0.25f + 0.85f * t);
        float fade = (1.f - t) * (1.f - t) * intensity;
        int c = rgbaf(color.x, color.y, color.z, fade * 0.11f);
        rd_draw_billboard(r, p, rad * 2.f, rad * 2.f, r->soft_particle, c, 1, 0.f);
    }
}

// ============================================================ post
static void bloom_down_rows(void *vctx, int y0, int y1) {
    Renderer *r = ((RowCtx *)vctx)->r;
    int bw = r->bw;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < bw; x++) {
            Vec3 sum = v3s(0.f);
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) {
                    int px2 = clampi(x * 4 + sx, 0, r->w - 1), py2 = clampi(y * 4 + sy, 0, r->h - 1);
                    size_t o = ((size_t)py2 * r->w + px2) * 3;
                    sum.x += r->hdr[o]; sum.y += r->hdr[o + 1]; sum.z += r->hdr[o + 2];
                }
            sum = vscale(sum, 1.f / 16.f);
            float br = maxf(sum.x, maxf(sum.y, sum.z));
            float th = maxf(0.f, br - 0.55f);
            float sc = br > 1e-5f ? th / br : 0.f;
            size_t bo = ((size_t)y * bw + x) * 3;
            r->bloom_a[bo] = sum.x * sc; r->bloom_a[bo + 1] = sum.y * sc; r->bloom_a[bo + 2] = sum.z * sc;
        }
}
static const float g_bloom_wts[5] = {0.227f, 0.194f, 0.122f, 0.054f, 0.016f};
typedef struct { Renderer *r; float rad; } BlurCtx;
static void blur_x_rows(void *vctx, int y0, int y1) {
    BlurCtx *bc = (BlurCtx *)vctx;
    Renderer *r = bc->r;
    int bw = r->bw, bh = r->bh;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < bw; x++) {
            Vec3 acc = v3s(0.f);
            for (int k = -4; k <= 4; k++) {
                int xx = clampi(x + (int)((float)k * bc->rad), 0, bw - 1);
                size_t so = ((size_t)y * bw + xx) * 3;
                float w = g_bloom_wts[abs(k)];
                acc.x += r->bloom_a[so] * w; acc.y += r->bloom_a[so + 1] * w; acc.z += r->bloom_a[so + 2] * w;
            }
            size_t dof_ = ((size_t)y * bw + x) * 3;
            r->bloom_b[dof_] = acc.x; r->bloom_b[dof_ + 1] = acc.y; r->bloom_b[dof_ + 2] = acc.z;
        }
}
static void blur_y_rows(void *vctx, int y0, int y1) {
    BlurCtx *bc = (BlurCtx *)vctx;
    Renderer *r = bc->r;
    int bw = r->bw, bh = r->bh;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < bw; x++) {
            Vec3 acc = v3s(0.f);
            for (int k = -4; k <= 4; k++) {
                int yy = clampi(y + (int)((float)k * bc->rad), 0, bh - 1);
                size_t so = ((size_t)yy * bw + x) * 3;
                float w = g_bloom_wts[abs(k)];
                acc.x += r->bloom_b[so] * w; acc.y += r->bloom_b[so + 1] * w; acc.z += r->bloom_b[so + 2] * w;
            }
            size_t dof_ = ((size_t)y * bw + x) * 3;
            r->bloom_a[dof_] = acc.x; r->bloom_a[dof_ + 1] = acc.y; r->bloom_a[dof_ + 2] = acc.z;
        }
}
static void composite_rows(void *vctx, int y0, int y1) {
    Renderer *r = ((RowCtx *)vctx)->r;
    const Environment *env = &r->env;
    int bw = r->bw, bh = r->bh;
    for (int y = y0; y < y1; y++) {
        for (int x = 0; x < r->w; x++) {
            size_t o = (size_t)y * r->w + x;
            Vec3 c = v3(r->hdr[o * 3], r->hdr[o * 3 + 1], r->hdr[o * 3 + 2]);
            float bx = ((float)x + 0.5f) / 4.f - 0.5f, by = ((float)y + 0.5f) / 4.f - 0.5f;
            int ix = (int)floorf(bx), iy = (int)floorf(by);
            float tx = bx - ix, ty = by - iy;
            Vec3 bsum = v3s(0.f);
            for (int k = 0; k < 4; k++) {
                int sx2 = clampi(ix + (k & 1), 0, bw - 1);
                int sy2 = clampi(iy + ((k >> 1) & 1), 0, bh - 1);
                float w = ((k & 1) ? tx : 1.f - tx) * (((k >> 1) & 1) ? ty : 1.f - ty);
                size_t bo = ((size_t)sy2 * bw + sx2) * 3;
                bsum.x += r->bloom_a[bo] * w; bsum.y += r->bloom_a[bo + 1] * w; bsum.z += r->bloom_a[bo + 2] * w;
            }
            c = vadd(c, vscale(bsum, env->bloom));
            c = vscale(c, env->exposure);
            c = aces_tonemap(c);
            float lum = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
            float gt = smoothstepf(0.04f, 0.65f, lum);
            c = v3(c.x * lerpf(env->grade_shadow.x, env->grade_high.x, gt),
                   c.y * lerpf(env->grade_shadow.y, env->grade_high.y, gt),
                   c.z * lerpf(env->grade_shadow.z, env->grade_high.z, gt));
            float g = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
            c = v3(lerpf(g, c.x, env->saturation), lerpf(g, c.y, env->saturation), lerpf(g, c.z, env->saturation));
            float dx = ((float)x / (float)r->w - 0.5f) * 2.f;
            float dy = ((float)y / (float)r->h - 0.5f) * 2.f;
            float vig = 1.f - env->vignette * saturatef((dx * dx + dy * dy) * 0.62f);
            c = vscale(c, vig);
            if (env->grain > 0.f) {
                float hsh = (float)(hash32((uint32_t)(x * 1973 + y * 9277 + (uint32_t)(env->time * 61.f) * 26699)) & 0xFFFF) / 65535.f;
                c = vadd(c, v3s((hsh - 0.5f) * env->grain));
            }
            r->color[o] = rgba8((int)(gamma1(c.x) * 255.f + 0.5f), (int)(gamma1(c.y) * 255.f + 0.5f),
                                (int)(gamma1(c.z) * 255.f + 0.5f), 255);
        }
    }
}

void rd_post(Renderer *r) {
    double t0 = core_now();
    if (!g_gamma_init) gamma_lut_init();
    RowCtx ctx = {r};
    parallel_for(r->bh, bloom_down_rows, &ctx, r->chunks);
    for (int pass = 0; pass < 3; pass++) {
        BlurCtx bc = {r, 1.f + (float)pass * 1.7f};
        parallel_for(r->bh, blur_x_rows, &bc, r->chunks);
        parallel_for(r->bh, blur_y_rows, &bc, r->chunks);
    }
    parallel_for(r->h, composite_rows, &ctx, r->chunks);
    r->post_ms = (float)((core_now() - t0) * 1000.0);
}

// ============================================================ 2D overlay
static inline void px_blend(Renderer *r, int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= r->w || y >= r->h) return;
    int a = c_a(c);
    if (a == 0) return;
    size_t o = (size_t)y * r->w + x;
    uint32_t d = r->color[o];
    if (a >= 255) { r->color[o] = c | 0xFF000000u; return; }
    float t = (float)a / 255.f;
    r->color[o] = rgba8(
        (int)(c_r(d) + (c_r(c) - c_r(d)) * t),
        (int)(c_g(d) + (c_g(c) - c_g(d)) * t),
        (int)(c_b(d) + (c_b(c) - c_b(d)) * t), 255);
}
void rd2d_rect(Renderer *r, float x, float y, float w, float h, uint32_t c) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    int x1 = (int)ceilf(x + w), y1 = (int)ceilf(y + h);
    x0 = clampi(x0, 0, r->w); y0 = clampi(y0, 0, r->h);
    x1 = clampi(x1, 0, r->w); y1 = clampi(y1, 0, r->h);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) px_blend(r, xx, yy, c);
}
void rd2d_rect_outline(Renderer *r, float x, float y, float w, float h, float t, uint32_t c) {
    rd2d_rect(r, x, y, w, t, c);
    rd2d_rect(r, x, y + h - t, w, t, c);
    rd2d_rect(r, x, y, t, h, c);
    rd2d_rect(r, x + w - t, y, t, h, c);
}
void rd2d_grad_v(Renderer *r, float x, float y, float w, float h, uint32_t top, uint32_t bot) {
    int y0 = clampi((int)floorf(y), 0, r->h), y1 = clampi((int)ceilf(y + h), 0, r->h);
    for (int yy = y0; yy < y1; yy++) {
        float t = ((float)yy + 0.5f - y) / (h + 1e-5f);
        rd2d_rect(r, x, (float)yy, w, 1.f, c_lerp(top, bot, saturatef(t)));
    }
}
void rd2d_grad_h(Renderer *r, float x, float y, float w, float h, uint32_t left, uint32_t right) {
    int x0 = clampi((int)floorf(x), 0, r->w), x1 = clampi((int)ceilf(x + w), 0, r->w);
    for (int xx = x0; xx < x1; xx++) {
        float t = ((float)xx + 0.5f - x) / (w + 1e-5f);
        rd2d_rect(r, (float)xx, y, 1.f, h, c_lerp(left, right, saturatef(t)));
    }
}
void rd2d_line(Renderer *r, float x0, float y0, float x1, float y1, float t, uint32_t c) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    int steps = (int)(len * 1.6f) + 1;
    for (int i = 0; i <= steps; i++) {
        float f = (float)i / (float)steps;
        rd2d_rect(r, x0 + dx * f - t * 0.5f, y0 + dy * f - t * 0.5f, t, t, c);
    }
}
void rd2d_circle(Renderer *r, float cx, float cy, float rad, uint32_t c) {
    int y0 = clampi((int)floorf(cy - rad), 0, r->h), y1 = clampi((int)ceilf(cy + rad), 0, r->h);
    for (int y = y0; y < y1; y++) {
        float dy = (float)y + 0.5f - cy;
        float d2 = rad * rad - dy * dy;
        if (d2 <= 0) continue;
        float hw = sqrtf(d2);
        int x0 = clampi((int)floorf(cx - hw), 0, r->w), x1 = clampi((int)ceilf(cx + hw), 0, r->w);
        for (int x = x0; x < x1; x++) px_blend(r, x, y, c);
    }
}
void rd2d_circle_outline(Renderer *r, float cx, float cy, float rad, float t, uint32_t c) {
    int seg = (int)(rad * 6.f) + 12;
    for (int i = 0; i < seg; i++) {
        float a0 = (float)i / seg * LN2PI, a1 = (float)(i + 1) / seg * LN2PI;
        rd2d_line(r, cx + cosf(a0) * rad, cy + sinf(a0) * rad, cx + cosf(a1) * rad, cy + sinf(a1) * rad, t, c);
    }
}
void rd2d_ring_arc(Renderer *r, float cx, float cy, float rad, float t, float a0, float a1, uint32_t c) {
    int seg = (int)(fabsf(a1 - a0) * rad * 0.7f) + 6;
    for (int i = 0; i < seg; i++) {
        float u0 = a0 + (a1 - a0) * (float)i / seg;
        float u1 = a0 + (a1 - a0) * (float)(i + 1) / seg;
        rd2d_line(r, cx + cosf(u0) * rad, cy + sinf(u0) * rad, cx + cosf(u1) * rad, cy + sinf(u1) * rad, t, c);
    }
}
void rd2d_tri(Renderer *r, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t c) {
    int minx = clampi((int)floorf(minf(x0, minf(x1, x2))), 0, r->w);
    int maxx = clampi((int)ceilf(maxf(x0, maxf(x1, x2))), 0, r->w);
    int miny = clampi((int)floorf(minf(y0, minf(y1, y2))), 0, r->h);
    int maxy = clampi((int)ceilf(maxf(y0, maxf(y1, y2))), 0, r->h);
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (fabsf(area) < 1e-6f) return;
    float ia = 1.f / area;
    for (int y = miny; y < maxy; y++)
        for (int x = minx; x < maxx; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float b0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * ia;
            float b1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * ia;
            float b2 = 1.f - b0 - b1;
            if (b0 < 0 || b1 < 0 || b2 < 0) continue;
            px_blend(r, x, y, c);
        }
}
void rd2d_image_uv(Renderer *r, const Tex *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t tint) {
    if (!t) return;
    int x0 = clampi((int)floorf(x), 0, r->w), x1 = clampi((int)ceilf(x + w), 0, r->w);
    int y0 = clampi((int)floorf(y), 0, r->h), y1 = clampi((int)ceilf(y + h), 0, r->h);
    float tr = (float)c_r(tint) / 255.f, tg = (float)c_g(tint) / 255.f, tb = (float)c_b(tint) / 255.f;
    float ta = (float)c_a(tint) / 255.f;
    for (int yy = y0; yy < y1; yy++) {
        float v = v0 + (v1 - v0) * (((float)yy + 0.5f - y) / h);
        for (int xx = x0; xx < x1; xx++) {
            float u = u0 + (u1 - u0) * (((float)xx + 0.5f - x) / w);
            int tx = (int)(u * t->w), ty = (int)(v * t->h);
            tx = clampi(tx, 0, t->w - 1); ty = clampi(ty, 0, t->h - 1);
            const uint8_t *p = t->px + ((size_t)ty * t->w + tx) * 4;
            float a = (float)p[3] / 255.f * ta;
            if (a <= 0.002f) continue;
            px_blend(r, xx, yy, rgba8((int)(p[0] * tr), (int)(p[1] * tg), (int)(p[2] * tb), (int)(a * 255.f)));
        }
    }
}
void rd2d_image(Renderer *r, const Tex *t, float x, float y, float w, float h, uint32_t tint) {
    rd2d_image_uv(r, t, x, y, w, h, 0, 0, 1, 1, tint);
}

// ============================================================ fonts
Font *font_load(const char *metrics_path, const char *atlas_path, const char *name) {
    size_t sz = 0;
    uint8_t *txt = file_read_all(metrics_path, &sz);
    if (!txt) { LOGE("font: missing metrics %s", metrics_path); return NULL; }
    Font *f = (Font *)calloc(1, sizeof(Font));
    snprintf(f->name, sizeof f->name, "%s", name ? name : "font");
    f->tex = tex_load_png(atlas_path, name);
    char *p = (char *)txt;
    // entries: "<char> <x> <y> <w> <h> <adv> <bx> <by>"
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        if (p[0] == 'L' && p[1] == ' ') {
            sscanf(p + 2, "%d %d", &f->lineheight, &f->ascent);
        } else if (p[0] == 'G' && p[1] == ' ') {
            int cp, x, y, w, h, adv, bx, by;
            if (sscanf(p + 2, "%d %d %d %d %d %d %d %d", &cp, &x, &y, &w, &h, &adv, &bx, &by) == 8) {
                if (f->count < 256) {
                    Glyph *g = &f->g[f->count++];
                    g->cp = cp; g->x = (short)x; g->y = (short)y; g->w = (short)w; g->h = (short)h;
                    g->adv = (short)adv; g->bx = (short)bx; g->by = (short)by;
                }
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    free(txt);
    if (!f->tex) { LOGE("font: missing atlas %s", atlas_path); free(f); return NULL; }
    if (f->lineheight <= 0) f->lineheight = 16;
    return f;
}
void font_free(Font *f) {
    if (!f) return;
    if (f->tex) tex_free(f->tex);
    free(f);
}
const Glyph *font_glyph(const Font *f, int cp) {
    for (int i = 0; i < f->count; i++)
        if (f->g[i].cp == cp) return &f->g[i];
    return NULL;
}
float font_text_width(const Font *f, float scale, const char *s) {
    if (!f) return 0;
    float w = 0;
    for (const char *p = s; *p; p++) {
        const Glyph *g = font_glyph(f, (unsigned char)*p);
        w += g ? (float)g->adv * scale : 6.f * scale;
    }
    return w;
}
void rd2d_text(Renderer *r, const Font *f, float x, float y, float scale, const char *s, uint32_t c) {
    if (!f) return;
    float cx = x;
    for (const char *p = s; *p; p++) {
        const Glyph *g = font_glyph(f, (unsigned char)*p);
        if (!g) { cx += 6.f * scale; continue; }
        if (g->w > 0 && g->h > 0) {
            float gx = cx + g->bx * scale, gy = y + g->by * scale;
            rd2d_image_uv(r, f->tex, gx, gy, g->w * scale, g->h * scale,
                          (float)g->x / f->tex->w, (float)g->y / f->tex->h,
                          (float)(g->x + g->w) / f->tex->w, (float)(g->y + g->h) / f->tex->h, c);
        }
        cx += g->adv * scale;
    }
}
void rd2d_text_sh(Renderer *r, const Font *f, float x, float y, float scale, const char *s, uint32_t c) {
    uint32_t sh = rgba8(0, 0, 0, (int)(c_a(c) * 0.85f));
    rd2d_text(r, f, x + scale, y + scale, scale, s, sh);
    rd2d_text(r, f, x, y, scale, s, c);
}
void rd2d_text_center(Renderer *r, const Font *f, float cx, float y, float scale, const char *s, uint32_t c) {
    rd2d_text(r, f, cx - font_text_width(f, scale, s) * 0.5f, y, scale, s, c);
}
void rd2d_text_center_sh(Renderer *r, const Font *f, float cx, float y, float scale, const char *s, uint32_t c) {
    rd2d_text_sh(r, f, cx - font_text_width(f, scale, s) * 0.5f, y, scale, s, c);
}
void rd2d_text_right(Renderer *r, const Font *f, float rx, float y, float scale, const char *s, uint32_t c) {
    rd2d_text(r, f, rx - font_text_width(f, scale, s), y, scale, s, c);
}

// ============================================================ upscale
void draw_upscale_to(const uint32_t *src, int sw, int sh, uint32_t *dst, int dw, int dh, float sharpen) {
    float sxr = (float)sw / (float)dw, syr = (float)sh / (float)dh;
    for (int y = 0; y < dh; y++) {
        float fy = ((float)y + 0.5f) * syr - 0.5f;
        int y0 = (int)floorf(fy);
        float ty = fy - y0;
        int y1 = y0 + 1;
        y0 = clampi(y0, 0, sh - 1); y1 = clampi(y1, 0, sh - 1);
        for (int x = 0; x < dw; x++) {
            float fx = ((float)x + 0.5f) * sxr - 0.5f;
            int x0 = (int)floorf(fx);
            float tx = fx - x0;
            int x1 = x0 + 1;
            x0 = clampi(x0, 0, sw - 1); x1 = clampi(x1, 0, sw - 1);
            uint32_t a = src[(size_t)y0 * sw + x0], b = src[(size_t)y0 * sw + x1];
            uint32_t c = src[(size_t)y1 * sw + x0], d = src[(size_t)y1 * sw + x1];
            float r = lerpf(lerpf((float)c_r(a), (float)c_r(b), tx), lerpf((float)c_r(c), (float)c_r(d), tx), ty);
            float g = lerpf(lerpf((float)c_g(a), (float)c_g(b), tx), lerpf((float)c_g(c), (float)c_g(d), tx), ty);
            float bl = lerpf(lerpf((float)c_b(a), (float)c_b(b), tx), lerpf((float)c_b(c), (float)c_b(d), tx), ty);
            if (sharpen != 0.f) {
                // cheap local contrast
                float avg = ((float)c_r(a) + (float)c_r(b) + (float)c_r(c) + (float)c_r(d)) * 0.25f;
                r += (r - avg) * sharpen;
                avg = ((float)c_g(a) + (float)c_g(b) + (float)c_g(c) + (float)c_g(d)) * 0.25f;
                g += (g - avg) * sharpen;
                avg = ((float)c_b(a) + (float)c_b(b) + (float)c_b(c) + (float)c_b(d)) * 0.25f;
                bl += (bl - avg) * sharpen;
            }
            dst[(size_t)y * dw + x] = rgba8((int)clampf(r, 0, 255), (int)clampf(g, 0, 255), (int)clampf(bl, 0, 255), 255);
        }
    }
}
void rd2d_flush(Renderer *r, uint32_t *dst, int dw, int dh, float sharpen) {
    draw_upscale_to(r->color, r->w, r->h, dst, dw, dh, sharpen);
}
