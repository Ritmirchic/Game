/* ============================================================================
   SPACE MINER — core/gfx.h
   Software renderer: 32-bit ARGB framebuffer, alpha/additive sprite blitting,
   vector primitives, baked-font text, bloom pass, camera transforms.
   Colour format: 0xAARRGGBB.
   ========================================================================== */
#ifndef SM_GFX_H
#define SM_GFX_H
#include "common.h"

#define SM_RGB(r,g,b)   ((u32)0xFF000000u | ((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))
#define SM_RGBA(r,g,b,a) (((u32)(a) << 24) | ((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))
#define SM_GET_A(c)  ((u32)((c) >> 24) & 0xFFu)
#define SM_GET_R(c)  ((u32)((c) >> 16) & 0xFFu)
#define SM_GET_G(c)  ((u32)((c) >> 8) & 0xFFu)
#define SM_GET_B(c)  ((u32)(c) & 0xFFu)
static inline u32 sm_col_lerp(u32 a, u32 b, f32 t) {
    int ia = (int)(sm_saturate(t) * 255.f + 0.5f);
    int r = (int)(((SM_GET_R(a) * (255 - ia)) + SM_GET_R(b) * ia) / 255);
    int g = (int)(((SM_GET_G(a) * (255 - ia)) + SM_GET_G(b) * ia) / 255);
    int bl = (int)(((SM_GET_B(a) * (255 - ia)) + SM_GET_B(b) * ia) / 255);
    int al = (int)(((SM_GET_A(a) * (255 - ia)) + SM_GET_A(b) * ia) / 255);
    return SM_RGBA(r, g, bl, al);
}
static inline u32 sm_col_scale(u32 c, f32 s) {
    int r = SM_MIN(255, (int)(SM_GET_R(c) * s));
    int g = SM_MIN(255, (int)(SM_GET_G(c) * s));
    int b = SM_MIN(255, (int)(SM_GET_B(c) * s));
    int a = SM_MIN(255, (int)(SM_GET_A(c) * s));
    return SM_RGBA(r, g, b, a);
}
static inline u32 sm_col_mul(u32 c, u32 tint) {
    return SM_RGBA(SM_GET_R(c) * SM_GET_R(tint) / 255,
                   SM_GET_G(c) * SM_GET_G(tint) / 255,
                   SM_GET_B(c) * SM_GET_B(tint) / 255,
                   SM_GET_A(c) * SM_GET_A(tint) / 255);
}

/* A sprite sheet: `frames` images of (w x h) laid out horizontally. */
typedef struct {
    int w, h, frames;
    u32 *px;
} Sheet;

typedef struct {
    int w, h;
    u32 *px;
} Tex;

/* Glyph atlas font */
#define SM_FONT_MAX_GLYPHS 96
typedef struct {
    Sheet atlas;
    int size, ascent, line_height, glyph_count;
    struct { s16 x, y, w, h, adv, bx, by; } g[SM_FONT_MAX_GLYPHS];
} Font;

typedef enum { BLEND_ALPHA = 0, BLEND_ADD = 1, BLEND_MUL = 2 } BlendMode;

typedef struct {
    u32 *px;
    int w, h;
    Rect clip;
    Rect clip_stack[8];
    int clip_depth;
    /* camera */
    Vec2 cam;         /* world position at screen centre */
    f32 zoom;
    Vec2 shake;
    u32 ambient;      /* additive ambient light tint */
    /* precomputed helpers */
    Tex glow_tex;     /* 64x64 radial falloff (white) */
    Tex noise_tex;
} Gfx;

void gfx_init(Gfx *g, int w, int h);
void gfx_shutdown(Gfx *g);
void gfx_resize(Gfx *g, int w, int h);

static inline Vec2 gfx_w2s(const Gfx *g, Vec2 p) {
    return v2((p.x - g->cam.x) * g->zoom + g->w * 0.5f + g->shake.x,
              (p.y - g->cam.y) * g->zoom + g->h * 0.5f + g->shake.y);
}
static inline Vec2 gfx_s2w(const Gfx *g, Vec2 p) {
    return v2((p.x - g->w * 0.5f - g->shake.x) / g->zoom + g->cam.x,
              (p.y - g->h * 0.5f - g->shake.y) / g->zoom + g->cam.y);
}

void gfx_push_clip(Gfx *g, Rect r);
void gfx_pop_clip(Gfx *g);

void gfx_clear(Gfx *g, u32 color);
void gfx_fill(Gfx *g, Rect r, u32 color);
void gfx_fill_add(Gfx *g, Rect r, u32 color);
void gfx_fill_vgrad(Gfx *g, Rect r, u32 top, u32 bottom);
void gfx_fill_hgrad(Gfx *g, Rect r, u32 left, u32 right);
void gfx_rect_outline(Gfx *g, Rect r, f32 thickness, u32 color);
void gfx_round_rect(Gfx *g, Rect r, f32 radius, u32 color);
void gfx_round_rect_outline(Gfx *g, Rect r, f32 radius, f32 thickness, u32 color);
void gfx_line(Gfx *g, Vec2 a, Vec2 b, f32 thickness, u32 color);
void gfx_line_add(Gfx *g, Vec2 a, Vec2 b, f32 thickness, u32 color);
void gfx_polyline(Gfx *g, const Vec2 *pts, int n, f32 thickness, u32 color);
void gfx_circle(Gfx *g, Vec2 c, f32 radius, u32 color);
void gfx_circle_add(Gfx *g, Vec2 c, f32 radius, u32 color);
void gfx_ring(Gfx *g, Vec2 c, f32 radius, f32 thickness, u32 color);
void gfx_arc(Gfx *g, Vec2 c, f32 radius, f32 a0, f32 a1, f32 thickness, u32 color);
void gfx_glow(Gfx *g, Vec2 c, f32 radius, u32 color, f32 intensity);
/* Axis-aligned screen blit of one frame with scaling and tinting. */
void gfx_blit(Gfx *g, const Sheet *s, int frame, Rect dst, u32 tint, f32 alpha, BlendMode mode);
/* World-space blit: sprite scaled by camera zoom, centred at world pos. */
void gfx_blit_world(Gfx *g, const Sheet *s, int frame, Vec2 worldPos, f32 worldScale,
                    u32 tint, f32 alpha, BlendMode mode);
void gfx_blit_world_ex(Gfx *g, const Sheet *s, int frame, Vec2 worldPos, f32 worldScale,
                       u32 tint, f32 alpha, BlendMode mode, Vec2 offsetScreen);
/* Rotated sprite blit (angle in radians, clockwise); bilinear sampled. */
void gfx_blit_rot(Gfx *g, const Sheet *s, int frame, Vec2 center, f32 w, f32 h, f32 angle,
                  u32 tint, f32 alpha, BlendMode mode);
/* Draw a texture (single image) to a screen rect. */
void gfx_tex(Gfx *g, const Tex *t, Rect dst, u32 tint, BlendMode mode);
void gfx_sheet_frame_rect(const Sheet *s, int frame, Rect *outUV);
/* Screen-space additive flash with radial falloff (explosions, lights). */
void gfx_light(Gfx *g, Vec2 screenPos, f32 radius, u32 color, f32 intensity);

/* text */
void gfx_font_load(Font *f, const Sheet *atlas, const char *meta);
void gfx_text(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align);
void gfx_text_shadow(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align);
void gfx_text_outline(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, u32 outline, int align);
void gfx_text_add(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align);
int  gfx_text_width(const Font *f, const char *str);
void gfx_text_spaced(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align, f32 spacing);

/* post processing */
void gfx_bloom(Gfx *g, f32 threshold, f32 intensity);
void gfx_vignette(Gfx *g, f32 strength);

/* helpers */
int  sheet_frame_for_angle(const Sheet *s, f32 angle);

#endif
