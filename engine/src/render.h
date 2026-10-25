// ============================================================================
// THE LAST NIGHT — software renderer (deferred G-buffer + tone mapping)
// ============================================================================
#ifndef TLN_RENDER_H
#define TLN_RENDER_H

#include "core.h"

// ---------------------------------------------------------------- textures
#define MAX_MIPS 10
typedef struct Tex {
    int w, h;
    uint8_t *px;  // RGBA8, w*h*4 (level 0)
    char name[48];
    int nmips;
    uint8_t *mip[MAX_MIPS];   // mip[0] == px
    int mw[MAX_MIPS], mh[MAX_MIPS];
} Tex;

Tex  *tex_create(int w, int h, const char *name);
Tex  *tex_load_png(const char *path, const char *name);
void  tex_free(Tex *t);
void  tex_sample(const Tex *t, float u, float v, float *out);
void  tex_sample_lod(const Tex *t, float u, float v, float lod, float *out);
void  tex_build_mips(Tex *t);
uint32_t tex_sample_u32(const Tex *t, float u, float v);

// ---------------------------------------------------------------- materials
enum {
    MF_NONE = 0,
    MF_UNLIT = 1 << 0,
    MF_EMISSIVE = 1 << 1,
    MF_TWOSIDED = 1 << 2,
    MF_ALPHATEST = 1 << 3,
    MF_NOFOG = 1 << 4,
    MF_NORMALMAP = 1 << 5,
    MF_METAL = 1 << 6,
};

typedef struct Material {
    const Tex *albedo;   // may be NULL
    const Tex *normal;   // may be NULL
    Vec3 tint;
    float rough, metal, emissive;
    float uvscale;
    int flags;
    char name[32];
} Material;

Material mat_make(const char *name, const Tex *albedo, const Tex *normal, Vec3 tint, float rough, float metal, float emissive, int flags);

// ---------------------------------------------------------------- meshes
typedef struct Mesh {
    Vec3 *pos;
    Vec3 *nrm;
    float *uv;          // 2 per vertex
    uint8_t *lit;       // 3 bytes per vertex, baked static lighting
    uint32_t vert_used, vert_cap;
    uint32_t *idx;
    uint32_t tri_used, tri_cap;
    int mat;
    Vec3 bbmin, bbmax;
    int dynamic;
} Mesh;

Mesh *mesh_new(uint32_t maxverts, uint32_t maxtris);
void  mesh_free(Mesh *m);
void  mesh_add_tri(Mesh *m, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 n0, Vec3 n1, Vec3 n2,
                   float u0, float v0, float u1, float v1, float u2, float v2,
                   uint8_t lr, uint8_t lg, uint8_t lb);
void  mesh_add_quad(Mesh *m, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, float us, float vs,
                    uint8_t lr, uint8_t lg, uint8_t lb);
void  mesh_add_box(Mesh *m, Vec3 c, Vec3 hs, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale);
void  mesh_add_boxf(Mesh *m, Vec3 mn, Vec3 mx, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale);
void  mesh_set_lit_all(Mesh *m, uint8_t r, uint8_t g, uint8_t b);
void  mesh_apply_xform(Mesh *m, Mat4 xf);

// procedural primitives (Y-up, centred on origin unless noted)
void mesh_cyl(Mesh *m, Vec3 center, float radius, float height, int segs, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale, int caps);
void mesh_cyl_axis(Mesh *m, Vec3 a, Vec3 b, float r0, float r1, int segs, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale, int caps);
void mesh_sphere(Mesh *m, Vec3 center, float radius, int segs, int rings, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale);
void mesh_ellipsoid(Mesh *m, Vec3 center, Vec3 radii, int segs, int rings, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale);
void mesh_torus(Mesh *m, Vec3 center, float major, float minor, int segs, int sides, uint8_t lr, uint8_t lg, uint8_t lb);
void mesh_ring(Mesh *m, Vec3 center, float r_in, float r_out, int segs, uint8_t lr, uint8_t lg, uint8_t lb);
void mesh_plane(Mesh *m, Vec3 center, Vec3 right, Vec3 up, float w, float h, uint8_t lr, uint8_t lg, uint8_t lb);
void mesh_quad_uv(Mesh *m, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, float u0, float v0, float u1, float v1, uint8_t lr, uint8_t lg, uint8_t lb);
void mesh_capsule(Mesh *m, Vec3 a, Vec3 b, float radius, int segs, int rings, uint8_t lr, uint8_t lg, uint8_t lb, float uvscale);
void mesh_bone_segment(Mesh *m, Vec3 a, Vec3 b, float r0, float r1, int segs, uint8_t lr, uint8_t lg, uint8_t lb);
Mat4 xform_between(Vec3 a, Vec3 b);

// ---------------------------------------------------------------- camera / env
typedef struct Camera {
    Vec3 pos;
    float yaw, pitch, roll;
    float fov;       // vertical, radians
    float sway_x, sway_y;
} Camera;

typedef struct Light {
    Vec3 pos;
    Vec3 color;
    float intensity;
    float range;
    float cos_inner, cos_outer;
    Vec3 dir;
    int type;         // 0 = point, 1 = spot
    int shadows;
} Light;

#define MAX_LIGHTS 24

typedef struct Environment {
    Vec3 ambient_sky, ambient_ground;
    float ambient_intensity;
    Vec3 fog_color;
    float fog_density;
    float exposure;
    float bloom;
    float vignette;
    float grain;
    float saturation;
    Vec3 grade_shadow, grade_high;
    float sky_moon_intensity;
    float time;
    float rain;          // 0..1 -> darkens sky, adds streaks
    Vec3 sky_tint;
} Environment;

// ---------------------------------------------------------------- fonts
typedef struct Glyph {
    int cp;
    short x, y, w, h, adv, bx, by;
} Glyph;
typedef struct Font {
    Tex *tex;
    int lineheight, ascent;
    int count;
    Glyph g[256];
    char name[32];
} Font;

Font *font_load(const char *metrics_path, const char *atlas_path, const char *name);
void  font_free(Font *f);
const Glyph *font_glyph(const Font *f, int cp);
float font_text_width(const Font *f, float scale, const char *s);

// ---------------------------------------------------------------- renderer
typedef struct Renderer {
    int w, h;
    float *gb_depth;
    Vec3  *gb_pos;
    Vec3  *gb_nrm;
    Vec3  *gb_alb;
    Vec3  *gb_lit;
    Vec3  *gb_emit;
    float *gb_rough;
    uint8_t *gb_sky;
    float *hdr;
    uint32_t *color;
    float *bloom_a, *bloom_b;
    int bw, bh;
    Camera cam;
    Mat4 view, view_rot, view_rot_t;
    Environment env;
    Light lights[MAX_LIGHTS];
    int nlights;
    Tex *soft_particle;
    float proj_f, proj_ax;
    Vec3 sky_lut[97*55];
    int sky_lut_w, sky_lut_h;
    unsigned int tris_drawn, tris_culled;
    float gbuf_ms, light_ms, post_ms, sky_ms;
    int chunks;
    float *shadow_mask;   // MAX_LIGHTS * sm_w * sm_h
    int sm_w, sm_h;
} Renderer;

int  rd_init(Renderer *r, int w, int h);
void rd_shutdown(Renderer *r);
void rd_resize(Renderer *r, int w, int h);

void rd_begin(Renderer *r, Camera *cam, Environment *env);
void rd_add_light(Renderer *r, Light l);
void rd_clear_lights(Renderer *r);
void rd_draw_mesh(Renderer *r, const Mesh *m, const Mat4 *model, const Material *mats, int mat_count);
void rd_lighting(Renderer *r);
void rd_post(Renderer *r);

void rd_draw_billboard(Renderer *r, Vec3 pos, float w, float h, const Tex *t, uint32_t color, int additive, float rot);
void rd_draw_decal(Renderer *r, Vec3 pos, Vec3 nrm, float size, const Tex *t, uint32_t color);
void rd_draw_beam(Renderer *r, Vec3 from, Vec3 to, float radius, Vec3 color, float intensity);

// 2D overlay helpers (draw into colour buffer, after rd_post)
void rd2d_rect(Renderer *r, float x, float y, float w, float h, uint32_t c);
void rd2d_rect_outline(Renderer *r, float x, float y, float w, float h, float t, uint32_t c);
void rd2d_grad_v(Renderer *r, float x, float y, float w, float h, uint32_t top, uint32_t bot);
void rd2d_grad_h(Renderer *r, float x, float y, float w, float h, uint32_t left, uint32_t right);
void rd2d_line(Renderer *r, float x0, float y0, float x1, float y1, float t, uint32_t c);
void rd2d_circle(Renderer *r, float cx, float cy, float rad, uint32_t c);
void rd2d_circle_outline(Renderer *r, float cx, float cy, float rad, float t, uint32_t c);
void rd2d_ring_arc(Renderer *r, float cx, float cy, float rad, float t, float a0, float a1, uint32_t c);
void rd2d_tri(Renderer *r, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t c);
void rd2d_image(Renderer *r, const Tex *t, float x, float y, float w, float h, uint32_t tint);
void rd2d_image_uv(Renderer *r, const Tex *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t tint);
void rd2d_text(Renderer *r, const Font *f, float x, float y, float scale, const char *s, uint32_t c);
void rd2d_text_sh(Renderer *r, const Font *f, float x, float y, float scale, const char *s, uint32_t c);
void rd2d_text_center(Renderer *r, const Font *f, float cx, float y, float scale, const char *s, uint32_t c);
void rd2d_text_center_sh(Renderer *r, const Font *f, float cx, float y, float scale, const char *s, uint32_t c);
void rd2d_text_right(Renderer *r, const Font *f, float rx, float y, float scale, const char *s, uint32_t c);

void draw_upscale_to(const uint32_t *src, int sw, int sh, uint32_t *dst, int dw, int dh, float sharpen);
void rd2d_flush(Renderer *r, uint32_t *dst, int dw, int dh, float sharpen);

#endif
