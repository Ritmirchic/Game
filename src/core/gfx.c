/* ============================================================================
   SPACE MINER — core/gfx.c   software renderer
   ========================================================================== */
#include "gfx.h"

/* ---------------------------------------------------------------- utils --- */
static inline u32 blend_alpha_px(u32 d, u32 s, u32 a) {
    u32 na = SM_GET_A(s) * a / 255u;
    if (na == 0) return d;
    if (na == 255) return s | 0xFF000000u;
    u32 ia = 255u - na;
    u32 r = (SM_GET_R(s) * na + SM_GET_R(d) * ia) / 255u;
    u32 g = (SM_GET_G(s) * na + SM_GET_G(d) * ia) / 255u;
    u32 b = (SM_GET_B(s) * na + SM_GET_B(d) * ia) / 255u;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline u32 blend_add_px(u32 d, u32 s, u32 a) {
    if (a == 0) return d;
    u32 sa = SM_GET_A(s) * a / 255u;
    if (sa == 0) return d;
    u32 r = SM_GET_R(d) + SM_GET_R(s) * sa / 255u;
    u32 g = SM_GET_G(d) + SM_GET_G(s) * sa / 255u;
    u32 b = SM_GET_B(d) + SM_GET_B(s) * sa / 255u;
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline u32 blend_mul_px(u32 d, u32 s, u32 a) {
    if (a == 0) return d;
    u32 sa = SM_GET_A(s) * a / 255u;
    u32 ia = 255u - sa;
    u32 r = (SM_GET_R(d) * (255u - sa) + SM_GET_R(d) * SM_GET_R(s) * sa / 255u) / 255u;
    u32 g = (SM_GET_G(d) * (255u - sa) + SM_GET_G(d) * SM_GET_G(s) * sa / 255u) / 255u;
    u32 b = (SM_GET_B(d) * (255u - sa) + SM_GET_B(d) * SM_GET_B(s) * sa / 255u) / 255u;
    (void)ia;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline void put_px(Gfx *g, int x, int y, u32 c, BlendMode mode, u32 a) {
    if (x < (int)g->clip.x || y < (int)g->clip.y ||
        x >= (int)(g->clip.x + g->clip.w) || y >= (int)(g->clip.y + g->clip.h)) return;
    u32 *d = &g->px[(usize)y * (usize)g->w + (usize)x];
    switch (mode) {
        case BLEND_ALPHA: *d = blend_alpha_px(*d, c, a); break;
        case BLEND_ADD:   *d = blend_add_px(*d, c, a); break;
        case BLEND_MUL:   *d = blend_mul_px(*d, c, a); break;
    }
}

/* ------------------------------------------------------------ lifecycle --- */
static void build_glow_tex(Tex *t, int size) {
    t->w = t->h = size;
    t->px = (u32 *)sm_alloc((usize)size * (usize)size * 4);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            f32 dx = (x + 0.5f) / size * 2.f - 1.f;
            f32 dy = (y + 0.5f) / size * 2.f - 1.f;
            f32 d = sqrtf(dx * dx + dy * dy);
            f32 v = 1.f - sm_saturate(d);
            v = v * v * (3.f - 2.f * v);
            v = powf(v, 1.6f);
            u32 a = (u32)(v * 255.f);
            t->px[y * size + x] = (a << 24) | 0x00FFFFFFu;
        }
    }
}

void gfx_init(Gfx *g, int w, int h) {
    memset(g, 0, sizeof(*g));
    g->w = w; g->h = h;
    g->px = (u32 *)sm_alloc((usize)w * (usize)h * 4);
    g->clip = rect(0, 0, (f32)w, (f32)h);
    g->zoom = 1.f;
    build_glow_tex(&g->glow_tex, 64);
}

void gfx_shutdown(Gfx *g) { (void)g; }

void gfx_resize(Gfx *g, int w, int h) {
    if (w == g->w && h == g->h) return;
    g->w = w; g->h = h;
    g->px = (u32 *)sm_alloc((usize)w * (usize)h * 4);
    g->clip = rect(0, 0, (f32)w, (f32)h);
    g->clip_depth = 0;
}

void gfx_push_clip(Gfx *g, Rect r) {
    if (g->clip_depth < (int)SM_ARRAY_COUNT(g->clip_stack))
        g->clip_stack[g->clip_depth] = g->clip;
    g->clip_depth++;
    f32 x0 = SM_MAX(r.x, g->clip.x), y0 = SM_MAX(r.y, g->clip.y);
    f32 x1 = SM_MIN(r.x + r.w, g->clip.x + g->clip.w), y1 = SM_MIN(r.y + r.h, g->clip.y + g->clip.h);
    g->clip = rect(x0, y0, SM_MAX(0.f, x1 - x0), SM_MAX(0.f, y1 - y0));
}

void gfx_pop_clip(Gfx *g) {
    if (g->clip_depth > 0) g->clip_depth--;
    if (g->clip_depth < (int)SM_ARRAY_COUNT(g->clip_stack))
        g->clip = g->clip_stack[g->clip_depth];
    else
        g->clip = rect(0, 0, (f32)g->w, (f32)g->h);
}

void gfx_clear(Gfx *g, u32 color) {
    usize n = (usize)g->w * (usize)g->h;
    u32 *p = g->px;
    for (usize i = 0; i < n; i++) p[i] = color;
    g->clip = rect(0, 0, (f32)g->w, (f32)g->h);
    g->clip_depth = 0;
}

/* ------------------------------------------------------------- filling ---- */
void gfx_fill(Gfx *g, Rect r, u32 color) {
    int x0 = sm_ifloor(r.x), y0 = sm_ifloor(r.y);
    int x1 = sm_ifloor(r.x + r.w), y1 = sm_ifloor(r.y + r.h);
    x0 = SM_MAX(x0, (int)g->clip.x); y0 = SM_MAX(y0, (int)g->clip.y);
    x1 = SM_MIN(x1, (int)(g->clip.x + g->clip.w)); y1 = SM_MIN(y1, (int)(g->clip.y + g->clip.h));
    if (x0 >= x1 || y0 >= y1) return;
    u32 a = SM_GET_A(color);
    if (a >= 250) {
        for (int y = y0; y < y1; y++) {
            u32 *d = &g->px[(usize)y * g->w + x0];
            for (int x = x0; x < x1; x++) *d++ = color | 0xFF000000u;
        }
    } else if (a > 0) {
        for (int y = y0; y < y1; y++) {
            u32 *d = &g->px[(usize)y * g->w + x0];
            for (int x = x0; x < x1; x++) { *d = blend_alpha_px(*d, color, 255); d++; }
        }
    }
}

void gfx_fill_add(Gfx *g, Rect r, u32 color) {
    int x0 = sm_ifloor(r.x), y0 = sm_ifloor(r.y);
    int x1 = sm_ifloor(r.x + r.w), y1 = sm_ifloor(r.y + r.h);
    x0 = SM_MAX(x0, (int)g->clip.x); y0 = SM_MAX(y0, (int)g->clip.y);
    x1 = SM_MIN(x1, (int)(g->clip.x + g->clip.w)); y1 = SM_MIN(y1, (int)(g->clip.y + g->clip.h));
    for (int y = y0; y < y1; y++) {
        u32 *d = &g->px[(usize)y * g->w + x0];
        for (int x = x0; x < x1; x++) { *d = blend_add_px(*d, color, 255); d++; }
    }
}

void gfx_fill_vgrad(Gfx *g, Rect r, u32 top, u32 bottom) {
    int y0 = SM_MAX(sm_ifloor(r.y), (int)g->clip.y);
    int y1 = SM_MIN(sm_ifloor(r.y + r.h), (int)(g->clip.y + g->clip.h));
    for (int y = y0; y < y1; y++) {
        f32 t = (f32)(y - r.y) / SM_MAX(1.f, r.h);
        u32 c = sm_col_lerp(top, bottom, t);
        gfx_fill(g, rect(r.x, (f32)y, r.w, 1.f), c);
    }
}

void gfx_fill_hgrad(Gfx *g, Rect r, u32 left, u32 right) {
    int x0 = SM_MAX(sm_ifloor(r.x), (int)g->clip.x);
    int x1 = SM_MIN(sm_ifloor(r.x + r.w), (int)(g->clip.x + g->clip.w));
    int y0 = SM_MAX(sm_ifloor(r.y), (int)g->clip.y);
    int y1 = SM_MIN(sm_ifloor(r.y + r.h), (int)(g->clip.y + g->clip.h));
    for (int x = x0; x < x1; x++) {
        f32 t = (f32)(x - r.x) / SM_MAX(1.f, r.w);
        u32 c = sm_col_lerp(left, right, t);
        int ya = y0;
        while (ya < y1) { put_px(g, x, ya, c, BLEND_ALPHA, 255); ya++; }
    }
}

void gfx_rect_outline(Gfx *g, Rect r, f32 thickness, u32 color) {
    gfx_fill(g, rect(r.x, r.y, r.w, thickness), color);
    gfx_fill(g, rect(r.x, r.y + r.h - thickness, r.w, thickness), color);
    gfx_fill(g, rect(r.x, r.y + thickness, thickness, r.h - thickness * 2), color);
    gfx_fill(g, rect(r.x + r.w - thickness, r.y + thickness, thickness, r.h - thickness * 2), color);
}

void gfx_round_rect(Gfx *g, Rect r, f32 radius, u32 color) {
    radius = SM_MIN(radius, SM_MIN(r.w, r.h) * 0.5f);
    int y0 = sm_ifloor(r.y), y1 = sm_ifloor(r.y + r.h);
    for (int y = y0; y < y1; y++) {
        f32 dy = 0.f;
        if (y < r.y + radius) dy = (r.y + radius) - (y + 0.5f);
        else if (y + 1 > r.y + r.h - radius) dy = (y + 0.5f) - (r.y + r.h - radius);
        f32 inset = 0.f;
        if (dy > 0.f) inset = radius - sqrtf(SM_MAX(0.f, radius * radius - dy * dy));
        gfx_fill(g, rect(r.x + inset, (f32)y, r.w - inset * 2.f, 1.f), color);
    }
}

void gfx_round_rect_outline(Gfx *g, Rect r, f32 radius, f32 thickness, u32 color) {
    radius = SM_MIN(radius, SM_MIN(r.w, r.h) * 0.5f);
    f32 t = SM_MAX(1.f, thickness);
    gfx_fill(g, rect(r.x + radius, r.y, r.w - radius * 2.f, t), color);
    gfx_fill(g, rect(r.x + radius, r.y + r.h - t, r.w - radius * 2.f, t), color);
    gfx_fill(g, rect(r.x, r.y + radius, t, r.h - radius * 2.f), color);
    gfx_fill(g, rect(r.x + r.w - t, r.y + radius, t, r.h - radius * 2.f), color);
    f32 rr = radius - t * 0.5f;
    if (rr > 0.5f) {
        gfx_arc(g, v2(r.x + radius, r.y + radius), rr, SM_PI, SM_PI * 1.5f, t, color);
        gfx_arc(g, v2(r.x + r.w - radius, r.y + radius), rr, SM_PI * 1.5f, SM_TAU, t, color);
        gfx_arc(g, v2(r.x + r.w - radius, r.y + r.h - radius), rr, 0.f, SM_PI * 0.5f, t, color);
        gfx_arc(g, v2(r.x + radius, r.y + r.h - radius), rr, SM_PI * 0.5f, SM_PI, t, color);
    }
}

void gfx_line(Gfx *g, Vec2 a, Vec2 b, f32 thickness, u32 color) { gfx_line_add(g, a, b, thickness, color); }

void gfx_line_add(Gfx *g, Vec2 a, Vec2 b, f32 thickness, u32 color) {
    Vec2 d = v2sub(b, a);
    f32 len = v2len(d);
    if (len < 0.01f) return;
    int steps = (int)len + 1;
    f32 r = SM_MAX(0.5f, thickness * 0.5f);
    int ri = (int)ceilf(r);
    Vec2 step = v2mul(d, 1.f / (f32)steps);
    Vec2 p = a;
    int mode = (SM_GET_A(color) == 255) ? 0 : 0;
    (void)mode;
    for (int i = 0; i <= steps; i++) {
        int cx = sm_ifloor(p.x), cy = sm_ifloor(p.y);
        for (int oy = -ri; oy <= ri; oy++) {
            for (int ox = -ri; ox <= ri; ox++) {
                f32 dist = sqrtf((f32)(ox * ox + oy * oy));
                f32 cov = sm_saturate(r + 0.5f - dist);
                if (cov <= 0.f) continue;
                put_px(g, cx + ox, cy + oy, color, BLEND_ALPHA, (u32)(cov * SM_GET_A(color)));
            }
        }
        p = v2add(p, step);
    }
}

void gfx_polyline(Gfx *g, const Vec2 *pts, int n, f32 thickness, u32 color) {
    for (int i = 0; i + 1 < n; i++) gfx_line(g, pts[i], pts[i + 1], thickness, color);
}

void gfx_circle(Gfx *g, Vec2 c, f32 radius, u32 color) {
    int r = (int)ceilf(radius);
    for (int y = -r; y <= r; y++) {
        f32 dy = (f32)y;
        f32 span = sqrtf(SM_MAX(0.f, radius * radius - dy * dy));
        int x0 = sm_ifloor(c.x - span), x1 = sm_ifloor(c.x + span);
        for (int x = x0; x <= x1; x++) put_px(g, x, sm_ifloor(c.y) + y, color, BLEND_ALPHA, SM_GET_A(color));
    }
}

void gfx_circle_add(Gfx *g, Vec2 c, f32 radius, u32 color) {
    int r = (int)ceilf(radius);
    for (int y = -r; y <= r; y++) {
        f32 dy = (f32)y;
        f32 span = sqrtf(SM_MAX(0.f, radius * radius - dy * dy));
        int x0 = sm_ifloor(c.x - span), x1 = sm_ifloor(c.x + span);
        for (int x = x0; x <= x1; x++) put_px(g, x, sm_ifloor(c.y) + y, color, BLEND_ADD, SM_GET_A(color));
    }
}

void gfx_ring(Gfx *g, Vec2 c, f32 radius, f32 thickness, u32 color) {
    f32 rr = SM_MAX(radius, 1.f);
    f32 inner = SM_MAX(0.f, rr - thickness);
    int r = (int)ceilf(rr);
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            f32 dist = sqrtf((f32)(x * x + y * y));
            f32 cov = sm_saturate(rr - dist + 0.5f) * sm_saturate(dist - inner + 0.5f);
            if (cov <= 0.f) continue;
            put_px(g, sm_ifloor(c.x) + x, sm_ifloor(c.y) + y, color, BLEND_ALPHA, (u32)(cov * SM_GET_A(color)));
        }
    }
}

void gfx_arc(Gfx *g, Vec2 c, f32 radius, f32 a0, f32 a1, f32 thickness, u32 color) {
    f32 span = a1 - a0;
    int steps = SM_MAX(4, (int)(fabsf(span) * radius * 0.35f));
    Vec2 prev = v2add(c, v2mul(v2from_angle(a0), radius));
    for (int i = 1; i <= steps; i++) {
        f32 t = (f32)i / (f32)steps;
        Vec2 p = v2add(c, v2mul(v2from_angle(a0 + span * t), radius));
        gfx_line(g, prev, p, thickness, color);
        prev = p;
    }
}

void gfx_tex(Gfx *g, const Tex *t, Rect dst, u32 tint, BlendMode mode) {
    if (!t || !t->px || dst.w <= 0 || dst.h <= 0) return;
    Sheet s; s.w = t->w; s.h = t->h; s.frames = 1; s.px = t->px;
    gfx_blit(g, &s, 0, dst, tint, SM_GET_A(tint) / 255.f, mode);
}

void gfx_glow(Gfx *g, Vec2 c, f32 radius, u32 color, f32 intensity) {
    if (radius <= 0.5f || intensity <= 0.f) return;
    Rect d = rect(c.x - radius, c.y - radius, radius * 2.f, radius * 2.f);
    u32 tint = SM_RGBA(SM_GET_R(color), SM_GET_G(color), SM_GET_B(color), 255);
    gfx_tex(g, &g->glow_tex, d, tint, BLEND_ADD);
    (void)intensity;
}

void gfx_light(Gfx *g, Vec2 screenPos, f32 radius, u32 color, f32 intensity) { gfx_glow(g, screenPos, radius, color, intensity); }

/* --------------------------------------------------------------- blits ---- */
void gfx_sheet_frame_rect(const Sheet *s, int frame, Rect *outUV) {
    outUV->x = (f32)(frame * s->w);
    outUV->y = 0;
    outUV->w = (f32)s->w;
    outUV->h = (f32)s->h;
}

void gfx_blit(Gfx *g, const Sheet *s, int frame, Rect dst, u32 tint, f32 alpha, BlendMode mode) {
    if (!s || !s->px || s->w <= 0 || s->h <= 0) return;
    if (dst.w <= 0.5f || dst.h <= 0.5f || alpha <= 0.003f) return;
    if (s->frames < 1) return;
    frame = ((frame % s->frames) + s->frames) % s->frames;

    int x0 = sm_ifloor(dst.x), y0 = sm_ifloor(dst.y);
    int x1 = sm_ifloor(dst.x + dst.w), y1 = sm_ifloor(dst.y + dst.h);
    int cx0 = SM_MAX(x0, (int)g->clip.x), cy0 = SM_MAX(y0, (int)g->clip.y);
    int cx1 = SM_MIN(x1, (int)(g->clip.x + g->clip.w)), cy1 = SM_MIN(y1, (int)(g->clip.y + g->clip.h));
    if (cx0 >= cx1 || cy0 >= cy1) return;

    const int sw = s->w, sh = s->h, stride = s->w * s->frames;
    const u32 *src = s->px + frame * s->w;
    f32 sxStep = (f32)sw / dst.w, syStep = (f32)sh / dst.h;
    u32 ta = (u32)(sm_saturate(alpha) * 255.f + 0.5f);
    u32 ti_r = SM_GET_R(tint), ti_g = SM_GET_G(tint), ti_b = SM_GET_B(tint);

    int near1 = (fabsf(dst.w - (f32)sw) < 0.5f && fabsf(dst.h - (f32)sh) < 0.5f);

    for (int y = cy0; y < cy1; y++) {
        f32 sy = (y - dst.y) * syStep;
        if (sy < 0.f) sy = 0.f;
        if (sy > sh - 1.001f) sy = sh - 1.001f;
        u32 *drow = &g->px[(usize)y * (usize)g->w];
        if (near1) {
            int iy = sm_ifloor(sy);
            const u32 *srow = src + (usize)iy * (usize)stride;
            for (int x = cx0; x < cx1; x++) {
                int ix = x - x0;
                if (ix < 0) ix = 0;
                if (ix >= sw) ix = sw - 1;
                u32 c = srow[ix];
                u32 sa = SM_GET_A(c);
                if (!sa) continue;
                if (sa == 255 && ta == 255 && ti_r == 255 && ti_g == 255 && ti_b == 255 && mode == BLEND_ALPHA) {
                    drow[x] = c | 0xFF000000u;
                    continue;
                }
                u32 r = SM_GET_R(c) * ti_r / 255u, gg = SM_GET_G(c) * ti_g / 255u, b = SM_GET_B(c) * ti_b / 255u;
                u32 col = (sa << 24) | (r << 16) | (gg << 8) | b;
                u32 a = sa * ta / 255u;
                switch (mode) {
                    case BLEND_ALPHA: drow[x] = blend_alpha_px(drow[x], col, a); break;
                    case BLEND_ADD:   drow[x] = blend_add_px(drow[x], col, a); break;
                    case BLEND_MUL:   drow[x] = blend_mul_px(drow[x], col, a); break;
                }
            }
        } else {
            int iy = sm_ifloor(sy);
            f32 fy = sy - (f32)iy;
            const u32 *r0 = src + (usize)iy * (usize)stride;
            const u32 *r1 = r0 + (iy + 1 < sh ? stride : 0);
            for (int x = cx0; x < cx1; x++) {
                f32 sxv = (x - dst.x) * sxStep;
                if (sxv < 0.f) sxv = 0.f;
                if (sxv > sw - 1.001f) sxv = sw - 1.001f;
                int ix = sm_ifloor(sxv);
                f32 fx = sxv - (f32)ix;
                int ix1 = ix + 1 < sw ? ix + 1 : ix;
                u32 c00 = r0[ix], c10 = r0[ix1], c01 = r1[ix], c11 = r1[ix1];
                /* bilinear over straight alpha (premultiply for correctness) */
                f32 w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
                f32 a00 = (f32)SM_GET_A(c00) * w00, a10 = (f32)SM_GET_A(c10) * w10;
                f32 a01 = (f32)SM_GET_A(c01) * w01, a11 = (f32)SM_GET_A(c11) * w11;
                f32 aa = a00 + a10 + a01 + a11;
                if (aa < 0.5f) continue;
                f32 inv = 1.f / aa;
                u32 r = (u32)((SM_GET_R(c00) * a00 + SM_GET_R(c10) * a10 + SM_GET_R(c01) * a01 + SM_GET_R(c11) * a11) * inv);
                u32 gg = (u32)((SM_GET_G(c00) * a00 + SM_GET_G(c10) * a10 + SM_GET_G(c01) * a01 + SM_GET_G(c11) * a11) * inv);
                u32 b = (u32)((SM_GET_B(c00) * a00 + SM_GET_B(c10) * a10 + SM_GET_B(c01) * a01 + SM_GET_B(c11) * a11) * inv);
                r = r * ti_r / 255u; gg = gg * ti_g / 255u; b = b * ti_b / 255u;
                u32 col = (((u32)aa) << 24) | (r << 16) | (gg << 8) | b;
                u32 a = ((u32)aa) * ta / 255u;
                switch (mode) {
                    case BLEND_ALPHA: drow[x] = blend_alpha_px(drow[x], col, a); break;
                    case BLEND_ADD:   drow[x] = blend_add_px(drow[x], col, a); break;
                    case BLEND_MUL:   drow[x] = blend_mul_px(drow[x], col, a); break;
                }
            }
        }
    }
}

void gfx_blit_rot(Gfx *g, const Sheet *s, int frame, Vec2 center, f32 w, f32 h, f32 angle,
                  u32 tint, f32 alpha, BlendMode mode) {
    if (!s || !s->px || s->w <= 0 || alpha <= 0.003f) return;
    frame = ((frame % s->frames) + s->frames) % s->frames;
    const int sw = s->w, sh = s->h;
    const int stride = sw * s->frames;
    const u32 *src = s->px + frame * sw;
    f32 ca = cosf(angle), sa = sinf(angle);
    f32 hw = w * 0.5f, hh = h * 0.5f;
    /* bounding box of the rotated quad */
    f32 ex = fabsf(ca) * hw + fabsf(sa) * hh;
    f32 ey = fabsf(sa) * hw + fabsf(ca) * hh;
    int x0 = sm_ifloor(center.x - ex), x1 = sm_ifloor(center.x + ex);
    int y0 = sm_ifloor(center.y - ey), y1 = sm_ifloor(center.y + ey);
    x0 = SM_MAX(x0, (int)g->clip.x); y0 = SM_MAX(y0, (int)g->clip.y);
    x1 = SM_MIN(x1, (int)(g->clip.x + g->clip.w)); y1 = SM_MIN(y1, (int)(g->clip.y + g->clip.h));
    if (x0 >= x1 || y0 >= y1) return;
    f32 invW = 1.f / SM_MAX(0.0001f, w), invH = 1.f / SM_MAX(0.0001f, h);
    u32 ta = (u32)(sm_saturate(alpha) * 255.f + 0.5f);
    for (int y = y0; y < y1; y++) {
        f32 dy = (f32)y + 0.5f - center.y;
        u32 *drow = &g->px[(usize)y * (usize)g->w];
        for (int x = x0; x < x1; x++) {
            f32 dx = (f32)x + 0.5f - center.x;
            /* inverse rotation */
            f32 lx = (dx * ca + dy * sa) * invW + 0.5f;
            f32 ly = (-dx * sa + dy * ca) * invH + 0.5f;
            if (lx < 0.f || lx >= 1.f || ly < 0.f || ly >= 1.f) continue;
            f32 sxv = lx * (f32)sw - 0.5f;
            f32 syv = ly * (f32)sh - 0.5f;
            int ix = sm_ifloor(sxv), iy = sm_ifloor(syv);
            f32 fx = sxv - (f32)ix, fy = syv - (f32)iy;
            int ix1 = SM_CLAMP(ix + 1, 0, sw - 1), iy1 = SM_CLAMP(iy + 1, 0, sh - 1);
            ix = SM_CLAMP(ix, 0, sw - 1); iy = SM_CLAMP(iy, 0, sh - 1);
            u32 c00 = src[(usize)iy * stride + ix], c10 = src[(usize)iy * stride + ix1];
            u32 c01 = src[(usize)iy1 * stride + ix], c11 = src[(usize)iy1 * stride + ix1];
            f32 w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
            f32 a00 = SM_GET_A(c00) * w00, a10 = SM_GET_A(c10) * w10;
            f32 a01 = SM_GET_A(c01) * w01, a11 = SM_GET_A(c11) * w11;
            f32 aa = a00 + a10 + a01 + a11;
            if (aa < 0.5f) continue;
            f32 inv = 1.f / aa;
            u32 r = (u32)((SM_GET_R(c00) * a00 + SM_GET_R(c10) * a10 + SM_GET_R(c01) * a01 + SM_GET_R(c11) * a11) * inv);
            u32 gg = (u32)((SM_GET_G(c00) * a00 + SM_GET_G(c10) * a10 + SM_GET_G(c01) * a01 + SM_GET_G(c11) * a11) * inv);
            u32 b = (u32)((SM_GET_B(c00) * a00 + SM_GET_B(c10) * a10 + SM_GET_B(c01) * a01 + SM_GET_B(c11) * a11) * inv);
            r = r * SM_GET_R(tint) / 255u; gg = gg * SM_GET_G(tint) / 255u; b = b * SM_GET_B(tint) / 255u;
            u32 col = (((u32)aa) << 24) | (r << 16) | (gg << 8) | b;
            u32 a = ((u32)aa) * ta / 255u;
            switch (mode) {
                case BLEND_ALPHA: drow[x] = blend_alpha_px(drow[x], col, a); break;
                case BLEND_ADD:   drow[x] = blend_add_px(drow[x], col, a); break;
                case BLEND_MUL:   drow[x] = blend_mul_px(drow[x], col, a); break;
            }
        }
    }
}

void gfx_blit_world(Gfx *g, const Sheet *s, int frame, Vec2 worldPos, f32 worldScale,
                    u32 tint, f32 alpha, BlendMode mode) {
    gfx_blit_world_ex(g, s, frame, worldPos, worldScale, tint, alpha, mode, v2(0, 0));
}

void gfx_blit_world_ex(Gfx *g, const Sheet *s, int frame, Vec2 worldPos, f32 worldScale,
                       u32 tint, f32 alpha, BlendMode mode, Vec2 offsetScreen) {
    Vec2 sp = gfx_w2s(g, worldPos);
    sp = v2add(sp, offsetScreen);
    f32 w = (f32)s->w * worldScale * g->zoom;
    f32 h = (f32)s->h * worldScale * g->zoom;
    gfx_blit(g, s, frame, rect(sp.x - w * 0.5f, sp.y - h * 0.5f, w, h), tint, alpha, mode);
}

int sheet_frame_for_angle(const Sheet *s, f32 angle) {
    if (s->frames <= 1) return 0;
    f32 turns = angle / SM_TAU;
    f32 f = turns * (f32)s->frames;
    int k = sm_iround(f);
    k %= s->frames;
    if (k < 0) k += s->frames;
    return k;
}

/* ---------------------------------------------------------------- fonts --- */
void gfx_font_load(Font *f, const Sheet *atlas, const char *meta) {
    memset(f, 0, sizeof(*f));
    f->atlas = *atlas;
    f->glyph_count = 0;
    if (!meta) return;
    const char *p = meta;
    while (*p) {
        if (!strncmp(p, "size ", 5)) { f->size = atoi(p + 5); }
        else if (!strncmp(p, "ascent ", 7)) { f->ascent = atoi(p + 7); }
        else if (!strncmp(p, "line ", 5)) { f->line_height = atoi(p + 5); }
        else if (!strncmp(p, "g ", 2)) {
            int idx, x, y, w, h, adv, bx, by;
            if (sscanf(p + 2, "%d %d %d %d %d %d %d %d", &idx, &x, &y, &w, &h, &adv, &bx, &by) == 8) {
                if (idx >= 0 && idx < SM_FONT_MAX_GLYPHS) {
                    f->g[idx].x = (s16)x; f->g[idx].y = (s16)y;
                    f->g[idx].w = (s16)w; f->g[idx].h = (s16)h;
                    f->g[idx].adv = (s16)adv; f->g[idx].bx = (s16)bx; f->g[idx].by = (s16)by;
                    if (idx + 1 > f->glyph_count) f->glyph_count = idx + 1;
                }
            }
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    if (f->line_height == 0) f->line_height = f->size + 2;
}

/* atlas pixel copy of one glyph; used by text renderers */
static void draw_glyph(Gfx *g, const Font *f, int gi, f32 px, f32 py, u32 color, BlendMode mode) {
    if (gi < 0 || gi >= f->glyph_count) return;
    const Sheet *a = &f->atlas;
    int gx = f->g[gi].x, gy = f->g[gi].y, gw = f->g[gi].w, gh = f->g[gi].h;
    if (gw <= 0 || gh <= 0) return;
    int ox = sm_iround(px) + f->g[gi].bx;
    int oy = sm_iround(py) + f->g[gi].by;
    for (int y = 0; y < gh; y++) {
        int dy = oy + y;
        if (dy < (int)g->clip.y || dy >= (int)(g->clip.y + g->clip.h)) continue;
        for (int x = 0; x < gw; x++) {
            u32 s = a->px[(usize)(gy + y) * (usize)(a->w * a->frames) + (usize)(gx + x)];
            u32 sa = SM_GET_A(s);
            if (!sa) continue;
            u32 col = (sa << 24) | (SM_GET_R(color) << 16) | (SM_GET_G(color) << 8) | SM_GET_B(color);
            put_px(g, ox + x, dy, col, mode, SM_GET_A(color));
        }
    }
}

/* atlas is not pre-multiplied by frame; glyph atlas is a single-frame sheet */
static void draw_glyph_tinted(Gfx *g, const Font *f, int gi, f32 px, f32 py, u32 color) {
    draw_glyph(g, f, gi, px, py, color, BLEND_ALPHA);
}

int gfx_text_width(const Font *f, const char *str) {
    if (!str) return 0;
    int w = 0;
    for (const char *p = str; *p; p++) {
        int gi = (u8)*p - 32;
        if (gi >= 0 && gi < f->glyph_count) w += f->g[gi].adv;
        else w += f->size / 2;
    }
    return w;
}

static void text_run(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align, f32 spacing) {
    int w = gfx_text_width(f, str);
    f32 x = pos.x;
    if (align == 1) x -= w * 0.5f;
    else if (align == 2) x -= (f32)w;
    for (const char *p = str; *p; p++) {
        int gi = (u8)*p - 32;
        if (gi >= 0 && gi < f->glyph_count) {
            draw_glyph_tinted(g, f, gi, x, pos.y, color);
            x += f->g[gi].adv + spacing;
        } else {
            x += f->size * 0.5f + spacing;
        }
    }
}

void gfx_text(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align) {
    text_run(g, f, pos, str, color, align, 0.f);
}

void gfx_text_add(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align) {
    int w = gfx_text_width(f, str);
    f32 x = pos.x;
    if (align == 1) x -= w * 0.5f;
    else if (align == 2) x -= (f32)w;
    for (const char *p = str; *p; p++) {
        int gi = (u8)*p - 32;
        if (gi >= 0 && gi < f->glyph_count) {
            draw_glyph(g, f, gi, x, pos.y, color, BLEND_ADD);
            x += f->g[gi].adv;
        } else x += f->size * 0.5f;
    }
}

void gfx_text_shadow(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align) {
    text_run(g, f, v2(pos.x + 1.5f, pos.y + 1.5f), str, 0x90000000u, align, 0.f);
    text_run(g, f, pos, str, color, align, 0.f);
}

void gfx_text_outline(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, u32 outline, int align) {
    for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++)
            if (ox || oy) text_run(g, f, v2(pos.x + ox, pos.y + oy), str, outline, align, 0.f);
    text_run(g, f, pos, str, color, align, 0.f);
}

void gfx_text_spaced(Gfx *g, const Font *f, Vec2 pos, const char *str, u32 color, int align, f32 spacing) {
    int w = gfx_text_width(f, str) + (int)(spacing * (f32)(strlen(str) ? strlen(str) - 1 : 0));
    f32 x = pos.x;
    if (align == 1) x -= w * 0.5f;
    else if (align == 2) x -= (f32)w;
    text_run(g, f, v2(x, pos.y), str, color, 0, spacing);
}

/* --------------------------------------------------------- post process --- */
void gfx_bloom(Gfx *g, f32 threshold, f32 intensity) {
    int w4 = SM_MAX(1, g->w / 4), h4 = SM_MAX(1, g->h / 4);
    static u32 *down = NULL;
    static u32 *blur = NULL;
    static int curW = 0, curH = 0;
    if (curW != w4 || curH != h4 || !down) {
        usize bytes = ((usize)w4 * (usize)h4 + 1) * 4;
        down = (u32 *)sm_alloc(bytes);
        blur = (u32 *)sm_alloc(bytes);
        curW = w4; curH = h4;
    }
    if (!down || !blur) return;
    u32 thr = (u32)(sm_saturate(threshold) * 255.f);
    for (int y = 0; y < h4; y++) {
        int sy = y * 4;
        for (int x = 0; x < w4; x++) {
            int sx = x * 4;
            u32 r = 0, gg = 0, b = 0;
            for (int j = 0; j < 4; j++) {
                const u32 *row = &g->px[(usize)SM_MIN(g->h - 1, sy + j) * (usize)g->w];
                for (int i = 0; i < 4; i++) {
                    u32 c = row[SM_MIN(g->w - 1, sx + i)];
                    r += SM_GET_R(c); gg += SM_GET_G(c); b += SM_GET_B(c);
                }
            }
            r /= 16; gg /= 16; b /= 16;
            u32 lum = (r * 77 + gg * 151 + b * 28) >> 8;
            if (lum < thr) { r = r * lum / (thr + 1); gg = gg * lum / (thr + 1); b = b * lum / (thr + 1); }
            down[y * w4 + x] = 0xFF000000u | (r << 16) | (gg << 8) | b;
        }
    }
    /* separable box blur x3 */
    u32 *a = down, *bb = blur;
    for (int pass = 0; pass < 3; pass++) {
        int rad = 2 + pass * 2;
        for (int y = 0; y < h4; y++) {
            for (int x = 0; x < w4; x++) {
                u32 r = 0, gg = 0, b = 0; int n = 0;
                for (int i = -rad; i <= rad; i++) {
                    int xx = SM_CLAMP(x + i, 0, w4 - 1);
                    u32 c = a[y * w4 + xx];
                    r += SM_GET_R(c); gg += SM_GET_G(c); b += SM_GET_B(c); n++;
                }
                bb[y * w4 + x] = 0xFF000000u | ((r / n) << 16) | ((gg / n) << 8) | (b / n);
            }
        }
        for (int y = 0; y < h4; y++) {
            for (int x = 0; x < w4; x++) {
                u32 r = 0, gg = 0, b = 0; int n = 0;
                for (int i = -rad; i <= rad; i++) {
                    int yy = SM_CLAMP(y + i, 0, h4 - 1);
                    u32 c = bb[yy * w4 + x];
                    r += SM_GET_R(c); gg += SM_GET_G(c); b += SM_GET_B(c); n++;
                }
                a[y * w4 + x] = 0xFF000000u | ((r / n) << 16) | ((gg / n) << 8) | (b / n);
            }
        }
    }
    /* add back, bilinearly sampled */
    for (int y = 0; y < g->h; y++) {
        f32 fy = (f32)y / 4.f - 0.5f;
        int iy = sm_ifloor(fy); f32 ty = fy - (f32)iy;
        int y0 = SM_CLAMP(iy, 0, h4 - 1), y1 = SM_CLAMP(iy + 1, 0, h4 - 1);
        u32 *row = &g->px[(usize)y * (usize)g->w];
        for (int x = 0; x < g->w; x++) {
            f32 fx = (f32)x / 4.f - 0.5f;
            int ix = sm_ifloor(fx); f32 tx = fx - (f32)ix;
            int x0 = SM_CLAMP(ix, 0, w4 - 1), x1 = SM_CLAMP(ix + 1, 0, w4 - 1);
            u32 c00 = a[y0 * w4 + x0], c10 = a[y0 * w4 + x1], c01 = a[y1 * w4 + x0], c11 = a[y1 * w4 + x1];
            f32 w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty), w01 = (1 - tx) * ty, w11 = tx * ty;
            u32 r = (u32)(SM_GET_R(c00) * w00 + SM_GET_R(c10) * w10 + SM_GET_R(c01) * w01 + SM_GET_R(c11) * w11);
            u32 gg = (u32)(SM_GET_G(c00) * w00 + SM_GET_G(c10) * w10 + SM_GET_G(c01) * w01 + SM_GET_G(c11) * w11);
            u32 b = (u32)(SM_GET_B(c00) * w00 + SM_GET_B(c10) * w10 + SM_GET_B(c01) * w01 + SM_GET_B(c11) * w11);
            r = (u32)(r * intensity); gg = (u32)(gg * intensity); b = (u32)(b * intensity);
            u32 c = row[x];
            u32 dr = SM_MIN(255u, SM_GET_R(c) + r);
            u32 dg = SM_MIN(255u, SM_GET_G(c) + gg);
            u32 db = SM_MIN(255u, SM_GET_B(c) + b);
            row[x] = 0xFF000000u | (dr << 16) | (dg << 8) | db;
        }
    }
}

void gfx_vignette(Gfx *g, f32 strength) {
    if (strength <= 0.001f) return;
    for (int y = 0; y < g->h; y++) {
        f32 dy = (f32)y / (f32)g->h * 2.f - 1.f;
        u32 *row = &g->px[(usize)y * (usize)g->w];
        for (int x = 0; x < g->w; x++) {
            f32 dx = (f32)x / (f32)g->w * 2.f - 1.f;
            f32 d = sqrtf(dx * dx * 0.85f + dy * dy * 1.05f);
            f32 v = sm_saturate((d - 0.55f) / 0.75f);
            v = v * v * strength;
            if (v <= 0.f) continue;
            u32 c = row[x];
            row[x] = 0xFF000000u |
                     ((u32)(SM_GET_R(c) * (1.f - v)) << 16) |
                     ((u32)(SM_GET_G(c) * (1.f - v)) << 8) |
                     ((u32)(SM_GET_B(c) * (1.f - v)));
        }
    }
}
