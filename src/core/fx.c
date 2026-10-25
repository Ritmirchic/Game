/* ============================================================================
   SPACE MINER — core/fx.c
   Particles, beams, floating text, screen flash. All world-space FX live here.
   ========================================================================== */
#include "state.h"

static Particle *part_alloc(Game *g) {
    Particle *pool = g->particles;
    int maxp = g->save.video.particles <= 0 ? SM_MAX_PARTICLES : (SM_MAX_PARTICLES * SM_CLAMP(g->save.video.particles, 1, 3)) / 3;
    for (int i = 0; i < maxp; i++) if (!pool[i].active) return &pool[i];
    /* recycle the oldest */
    Particle *oldest = &pool[0];
    for (int i = 1; i < maxp; i++) {
        if (pool[i].life < oldest->life) oldest = &pool[i];
    }
    return oldest;
}

static void part_spawn(Game *g, int kind, Vec2 pos, Vec2 vel, f32 life, f32 size, f32 size_end,
                       u32 color, BlendMode blend) {
    Particle *p = part_alloc(g);
    p->active = 1;
    p->kind = kind;
    p->pos = pos;
    p->vel = vel;
    p->life = life;
    p->max_life = life;
    p->size = size;
    p->size_end = size_end;
    p->color = color;
    p->blend = blend;
    p->drag = 1.6f;
    p->angle = rng_range(&g->rng, 0.f, SM_TAU);
    p->spin = rng_range(&g->rng, -6.f, 6.f);
    p->gravity = 0.f;
}

void fx_clear(Game *g) {
    memset(g->particles, 0, sizeof(g->particles));
    memset(g->beams, 0, sizeof(g->beams));
    memset(g->texts, 0, sizeof(g->texts));
}

void fx_init(Game *g) { fx_clear(g); }

void fx_spark(Game *g, Vec2 pos, Vec2 vel, u32 color, int count, f32 speed) {
    for (int i = 0; i < count; i++) {
        f32 a = rng_range(&g->rng, 0.f, SM_TAU);
        f32 sp = speed * rng_range(&g->rng, 0.35f, 1.f);
        Vec2 v = v2add(vel, v2mul(v2from_angle(a), sp));
        part_spawn(g, PT_SPARK, pos, v, rng_range(&g->rng, 0.18f, 0.5f),
                   rng_range(&g->rng, 1.6f, 3.4f), 0.f, color, BLEND_ADD);
    }
}

void fx_smoke(Game *g, Vec2 pos, Vec2 vel, u32 color) {
    part_spawn(g, PT_SMOKE, pos, vel, rng_range(&g->rng, 0.7f, 1.5f),
               rng_range(&g->rng, 8.f, 18.f), rng_range(&g->rng, 26.f, 44.f), color, BLEND_ALPHA);
}

void fx_debris(Game *g, Vec2 pos, Vec2 base_vel, int count, u32 color) {
    for (int i = 0; i < count; i++) {
        f32 a = rng_range(&g->rng, 0.f, SM_TAU);
        f32 sp = rng_range(&g->rng, 30.f, 190.f);
        Particle *p = part_alloc(g);
        p->active = 1;
        p->kind = PT_DEBRIS;
        p->pos = pos;
        p->vel = v2add(base_vel, v2mul(v2from_angle(a), sp));
        p->life = rng_range(&g->rng, 0.7f, 2.0f);
        p->max_life = p->life;
        p->size = rng_range(&g->rng, 2.f, 6.f);
        p->size_end = p->size * 0.7f;
        p->color = color;
        p->blend = BLEND_ALPHA;
        p->drag = 0.7f;
        p->angle = rng_range(&g->rng, 0.f, SM_TAU);
        p->spin = rng_range(&g->rng, -9.f, 9.f);
    }
}

void fx_ring(Game *g, Vec2 pos, f32 radius, u32 color, f32 life) {
    Particle *p = part_alloc(g);
    p->active = 1;
    p->kind = PT_RING;
    p->pos = pos;
    p->vel = v2(0, 0);
    p->life = life;
    p->max_life = life;
    p->size = radius * 0.25f;
    p->size_end = radius;
    p->color = color;
    p->blend = BLEND_ADD;
    p->drag = 0.f;
}

void fx_shockwave(Game *g, Vec2 pos, f32 radius, u32 color) {
    fx_ring(g, pos, radius, color, 0.55f);
    fx_ring(g, pos, radius * 0.6f, 0xFFFFFFFF, 0.35f);
}

void fx_explosion(Game *g, Vec2 pos, f32 radius, u32 color, int big) {
    int sparks = big ? 34 : 16;
    fx_spark(g, pos, v2(0, 0), color, sparks, radius * 4.2f);
    fx_spark(g, pos, v2(0, 0), 0xFFFFFFFF, sparks / 2, radius * 6.5f);
    for (int i = 0; i < (big ? 14 : 7); i++) {
        Vec2 off = v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), rng_range(&g->rng, 0.f, radius * 0.7f));
        part_spawn(g, PT_FIRE, v2add(pos, off),
                   v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), rng_range(&g->rng, 20.f, 120.f)),
                   rng_range(&g->rng, 0.25f, 0.7f), rng_range(&g->rng, radius * 0.30f, radius * 0.7f),
                   0.f, sm_col_lerp(color, 0xFFFFE0A0u, rng_range(&g->rng, 0.f, 0.5f)), BLEND_ADD);
    }
    for (int i = 0; i < (big ? 9 : 4); i++) {
        Vec2 off = v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), rng_range(&g->rng, 0.f, radius));
        fx_smoke(g, v2add(pos, off), v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), rng_range(&g->rng, 10.f, 50.f)),
                 SM_RGBA(60, 66, 78, 170));
    }
    fx_debris(g, pos, v2(0, 0), big ? 12 : 6, color);
    fx_ring(g, pos, radius * (big ? 3.4f : 2.2f), color, big ? 0.6f : 0.4f);
    g->cam.shake = SM_MAX(g->cam.shake, big ? 18.f : 7.f);
    if (g->save.video.shake == 0) g->cam.shake *= 0.4f;
}

void fx_impact(Game *g, Vec2 pos, Vec2 normal, u32 color) {
    fx_spark(g, pos, v2mul(normal, -60.f), color, 7, 130.f);
    fx_ring(g, pos, 16.f, color, 0.18f);
}

void fx_beam(Game *g, Vec2 a, Vec2 b, f32 width, u32 color, f32 life, int kind) {
    for (int i = 0; i < (int)SM_ARRAY_COUNT(g->beams); i++) {
        Beam *bm = &g->beams[i];
        if (!bm->active || bm->kind == kind) {
            bm->active = 1;
            bm->kind = kind;
            bm->a = a;
            bm->b = b;
            bm->width = width;
            bm->color = color;
            bm->life = life;
            bm->jitter = kind == BEAM_MINING ? 2.2f : 1.2f;
            return;
        }
    }
}

void fx_text(Game *g, Vec2 pos, const char *text, u32 color) {
    FloatText *t = NULL;
    for (int i = 0; i < SM_MAX_TEXTS; i++) {
        if (!g->texts[i].active) { t = &g->texts[i]; break; }
    }
    if (!t) {
        t = &g->texts[0];
        for (int i = 1; i < SM_MAX_TEXTS; i++) if (g->texts[i].life < t->life) t = &g->texts[i];
    }
    t->active = 1;
    sm_strlcpy(t->text, text, sizeof(t->text));
    t->pos = pos;
    t->color = color;
    t->life = 1.5f;
    t->max_life = 1.5f;
    t->vy = -28.f;
    t->scale = 1.f;
}

void fx_update(Game *g, f32 dt) {
    for (int i = 0; i < SM_MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.f) { p->active = 0; continue; }
        p->pos = v2add(p->pos, v2mul(p->vel, dt));
        f32 damp = sm_decay(p->drag, dt);
        p->vel = v2mul(p->vel, damp);
        p->angle += p->spin * dt;
        if (p->kind == PT_SMOKE) {
            p->size += (p->size_end - p->size) * SM_MIN(1.f, dt * 1.4f);
        }
    }
    for (int i = 0; i < (int)SM_ARRAY_COUNT(g->beams); i++) {
        Beam *bm = &g->beams[i];
        if (!bm->active) continue;
        bm->life -= dt;
        if (bm->life <= 0.f) bm->active = 0;
    }
    for (int i = 0; i < SM_MAX_TEXTS; i++) {
        FloatText *t = &g->texts[i];
        if (!t->active) continue;
        t->life -= dt;
        t->pos.y += t->vy * dt;
        t->vy *= sm_decay(1.6f, dt);
        if (t->life <= 0.f) t->active = 0;
    }
    g->cam.shake = SM_MAX(0.f, g->cam.shake - dt * 42.f);
    g->cam.hull_flash = SM_MAX(0.f, g->cam.hull_flash - dt * 2.2f);
    g->cam.shield_ripple = SM_MAX(0.f, g->cam.shield_ripple - dt * 2.6f);
    g->cam.mining_glow = SM_MAX(0.f, g->cam.mining_glow - dt * 3.f);
}

/* ------------------------------------------------------------------ draw --- */
static void draw_particle(Game *g, Particle *p) {
    Gfx *gfx = g->gfx;
    f32 t = 1.f - p->life / p->max_life;
    Vec2 sp = gfx_w2s(gfx, p->pos);
    if (sp.x < -80 || sp.y < -80 || sp.x > gfx->w + 80 || sp.y > gfx->h + 80) return;
    switch (p->kind) {
        case PT_SPARK: {
            Vec2 tail = v2sub(sp, v2mul(p->vel, -0.012f * gfx->zoom));
            f32 w = p->size * (1.f - t * 0.7f);
            gfx_line(gfx, tail, sp, w, sm_col_scale(p->color, (1.f - t) * 1.4f));
        } break;
        case PT_SMOKE: {
            f32 a = (1.f - t) * (SM_GET_A(p->color) / 255.f) * 0.55f;
            gfx_blit(gfx, &g->assets.fx_smoke, (int)(t * 12.f), rect(sp.x - p->size, sp.y - p->size,
                     p->size * 2.f, p->size * 2.f), p->color, a, BLEND_ALPHA);
        } break;
        case PT_FIRE: {
            f32 a = (1.f - t) * 0.9f;
            f32 r = p->size * (1.f + t * 0.6f) * gfx->zoom;
            gfx_glow(gfx, sp, r, p->color, a);
            gfx_glow(gfx, sp, r * 0.45f, 0xFFFFFFFF, a * 0.6f);
        } break;
        case PT_GLOW: {
            f32 a = (1.f - t) * (SM_GET_A(p->color) / 255.f);
            gfx_glow(gfx, sp, p->size * gfx->zoom * (1.f + t), p->color, a);
        } break;
        case PT_DEBRIS: {
            f32 s = p->size * gfx->zoom;
            u32 c = sm_col_scale(p->color, 0.6f + 0.4f * (1.f - t));
            f32 ca = cosf(p->angle), sa = sinf(p->angle);
            Vec2 pts[3] = {
                v2(sp.x + ca * s, sp.y + sa * s),
                v2(sp.x - ca * s * 0.6f + sa * s * 0.8f, sp.y - sa * s * 0.6f - ca * s * 0.8f),
                v2(sp.x - ca * s * 0.6f - sa * s * 0.8f, sp.y - sa * s * 0.6f + ca * s * 0.8f)
            };
            gfx_fill(gfx, rect(sp.x - s * 0.5f, sp.y - s * 0.5f, s, s), c);
            gfx_line(gfx, pts[0], pts[1], 1.2f, c);
            gfx_line(gfx, pts[1], pts[2], 1.2f, c);
            gfx_line(gfx, pts[2], pts[0], 1.2f, c);
        } break;
        case PT_RING: {
            f32 r = SM_LERP(p->size, p->size_end, sm_easeout(t)) * gfx->zoom;
            f32 a = (1.f - t) * 0.8f;
            gfx_ring(gfx, sp, r, SM_MAX(1.f, 3.f * (1.f - t) * gfx->zoom), sm_col_scale(p->color, a));
        } break;
        case PT_STREAK: {
            Vec2 tail = v2sub(sp, v2mul(p->vel, 0.02f));
            gfx_line_add(gfx, tail, sp, p->size, sm_col_scale(p->color, 1.f - t));
        } break;
        default: break;
    }
}

void fx_render_back(Game *g) {
    for (int i = 0; i < SM_MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) continue;
        if (p->kind == PT_SMOKE || p->kind == PT_DEBRIS) draw_particle(g, p);
    }
}

void fx_render_front(Game *g) {
    Gfx *gfx = g->gfx;
    /* beams first (behind sparks) */
    for (int i = 0; i < (int)SM_ARRAY_COUNT(g->beams); i++) {
        Beam *bm = &g->beams[i];
        if (!bm->active) continue;
        Vec2 a = gfx_w2s(gfx, bm->a);
        Vec2 b = gfx_w2s(gfx, bm->b);
        f32 life_t = sm_saturate(bm->life / 0.09f);
        f32 w = bm->width * gfx->zoom * life_t;
        if (bm->kind == BEAM_MINING) {
            Vec2 d = v2sub(b, a);
            f32 len = v2len(d);
            Vec2 n = len > 0.001f ? v2(d.y / len, -d.x / len) : v2(0, 1);
            f32 phase = g->time * 26.f;
            for (int k = 0; k < 3; k++) {
                Vec2 prev = a;
                for (int s = 1; s <= 6; s++) {
                    f32 f = (f32)s / 6.f;
                    f32 wob = sinf(phase + f * 12.f + k * 2.1f) * bm->jitter * f * (k == 0 ? 1.f : 0.5f);
                    Vec2 p = v2add(v2lerp(a, b, f), v2mul(n, wob));
                    gfx_line_add(gfx, prev, p, w * (k == 1 ? 1.6f : 2.6f), sm_col_scale(bm->color, 0.22f));
                    prev = p;
                }
            }
            gfx_line_add(gfx, a, b, SM_MAX(1.2f, w * 0.9f), sm_col_scale(bm->color, 0.85f));
            gfx_line_add(gfx, a, b, SM_MAX(0.8f, w * 0.35f), 0xFFFFFFFFu);
            gfx_glow(gfx, a, 18.f * gfx->zoom, bm->color, 0.7f);
            gfx_glow(gfx, b, 26.f * gfx->zoom, bm->color, 0.8f);
        } else if (bm->kind == BEAM_ENEMY) {
            gfx_line_add(gfx, a, b, w * 1.6f, sm_col_scale(bm->color, 0.35f));
            gfx_line_add(gfx, a, b, SM_MAX(1.f, w * 0.5f), bm->color);
        } else {
            gfx_line_add(gfx, a, b, w * 2.0f, sm_col_scale(bm->color, 0.3f));
            gfx_line_add(gfx, a, b, SM_MAX(1.f, w), bm->color);
        }
    }
    for (int i = 0; i < SM_MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) continue;
        if (p->kind != PT_SMOKE && p->kind != PT_DEBRIS) draw_particle(g, p);
    }
    /* floating combat/damage text */
    for (int i = 0; i < SM_MAX_TEXTS; i++) {
        FloatText *t = &g->texts[i];
        if (!t->active) continue;
        Vec2 sp = gfx_w2s(gfx, t->pos);
        f32 a = sm_saturate(t->life / t->max_life);
        u32 c = sm_col_scale(t->color, 0.4f + a * 0.6f);
        gfx_text_shadow(gfx, &g->assets.font_small, v2(sp.x, sp.y), t->text, c, 1);
    }
}

/* Soft additive glow blob (used for engine cores, lights, pickups). */
void fx_glow(Game *g, Vec2 pos, f32 radius, u32 color, f32 life) {
    if (life <= 0.f) life = 0.12f;
    Particle *p = part_alloc(g);
    p->active = 1;
    p->kind = PT_GLOW;
    p->pos = pos;
    p->vel = v2(0, 0);
    p->life = life;
    p->max_life = life;
    p->size = radius;
    p->size_end = radius * 0.55f;
    p->color = color;
    p->blend = BLEND_ADD;
    p->drag = 0.f;
    p->angle = 0.f;
    p->spin = 0.f;
}
