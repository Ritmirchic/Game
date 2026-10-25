// Particles, decals, impacts and weather.
#include "game.h"
#include <string.h>

static Particle *alloc_particle(World *w) {
    if (w->particle_count >= MAX_PARTICLES) {
        // recycle the oldest
        int best = 0;
        float bl = 1e9f;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            float left = w->particles[i].life;
            if (left < bl) { bl = left; best = i; }
        }
        return &w->particles[best];
    }
    return &w->particles[w->particle_count++];
}

static void spawn(World *w, Vec3 pos, Vec3 vel, float life, float size, float grow,
                  uint32_t color, int additive, float gravity, float drag, const Tex *tex) {
    Particle *p = alloc_particle(w);
    p->pos = pos;
    p->vel = vel;
    p->life = p->max_life = life;
    p->size = size;
    p->grow = grow;
    p->color = color;
    p->additive = additive;
    p->gravity = gravity;
    p->drag = drag;
    p->rot = rng_f(&w->rng) * 6.28f;
    p->rot_spd = rng_range(&w->rng, -8.f, 8.f);
    p->tex = tex;
}

void fx_spark(Game *g, Vec3 pos, Vec3 nrm, Vec3 color, int count) {
    World *w = g->w;
    for (int i = 0; i < count; i++) {
        Vec3 dir = v3(rng_range(&w->rng, -1, 1), rng_range(&w->rng, -1, 1), rng_range(&w->rng, -1, 1));
        dir = vnorm(vadd(vnorm(dir), vscale(nrm, 1.2f)));
        Vec3 v = vscale(dir, rng_range(&w->rng, 1.5f, 6.f));
        uint32_t c = COL((uint8_t)clampf(color.x * 255, 0, 255), (uint8_t)clampf(color.y * 255, 0, 255),
                         (uint8_t)clampf(color.z * 255, 0, 255), 255);
        spawn(w, pos, v, rng_range(&w->rng, 0.15f, 0.45f), rng_range(&w->rng, 0.02f, 0.05f), 0.f, c, 1, -9.5f, 2.f, g->as->flash);
    }
}

void fx_blood(Game *g, Vec3 pos, Vec3 dir, int count) {
    World *w = g->w;
    for (int i = 0; i < count; i++) {
        Vec3 d = vadd(vscale(dir, rng_range(&w->rng, 0.4f, 2.6f)),
                      v3(rng_range(&w->rng, -1.2f, 1.2f), rng_range(&w->rng, -0.3f, 2.0f), rng_range(&w->rng, -1.2f, 1.2f)));
        float t = rng_f(&w->rng);
        uint32_t c = COL((uint8_t)(120 + t * 70), (uint8_t)(8 + t * 14), (uint8_t)(10 + t * 10), 235);
        spawn(w, pos, vscale(d, 2.2f), rng_range(&w->rng, 0.35f, 0.9f),
              rng_range(&w->rng, 0.035f, 0.09f), 0.03f, c, 0, -13.f, 1.2f, g->as->blood);
    }
}

void fx_gib(Game *g, Vec3 pos, int count) {
    World *w = g->w;
    for (int i = 0; i < count; i++) {
        Vec3 d = v3(rng_range(&w->rng, -1, 1), rng_range(&w->rng, 0.2f, 1.6f), rng_range(&w->rng, -1, 1));
        spawn(w, pos, vscale(vnorm(d), rng_range(&w->rng, 2.f, 6.5f)), rng_range(&w->rng, 0.7f, 1.4f),
              rng_range(&w->rng, 0.05f, 0.13f), 0.02f, COL(150, 22, 22, 255), 0, -14.f, 0.6f, g->as->blood);
    }
    fx_blood(g, pos, v3(0, 1, 0), count);
}

void fx_smoke(Game *g, Vec3 pos, int count) {
    World *w = g->w;
    for (int i = 0; i < count; i++) {
        Vec3 d = v3(rng_range(&w->rng, -0.5f, 0.5f), rng_range(&w->rng, 0.3f, 1.4f), rng_range(&w->rng, -0.5f, 0.5f));
        spawn(w, pos, d, rng_range(&w->rng, 0.6f, 1.6f), rng_range(&w->rng, 0.10f, 0.3f), 0.5f,
              COL(150, 148, 145, 110), 0, 0.35f, 1.4f, g->as->smoke);
    }
}
void fx_dust(Game *g, Vec3 pos, int count) {
    World *w = g->w;
    for (int i = 0; i < count; i++) {
        Vec3 d = v3(rng_range(&w->rng, -1, 1), rng_range(&w->rng, 0.f, 0.8f), rng_range(&w->rng, -1, 1));
        spawn(w, pos, vscale(d, 1.2f), rng_range(&w->rng, 0.4f, 1.1f), rng_range(&w->rng, 0.08f, 0.22f), 0.4f,
              COL(170, 165, 155, 90), 0, -0.2f, 1.8f, g->as->smoke);
    }
}
void fx_glass(Game *g, Vec3 pos, int count) {
    World *w = g->w;
    for (int i = 0; i < count; i++) {
        Vec3 d = v3(rng_range(&w->rng, -1, 1), rng_range(&w->rng, -0.2f, 1.4f), rng_range(&w->rng, -1, 1));
        spawn(w, pos, vscale(vnorm(d), rng_range(&w->rng, 1.f, 4.f)), rng_range(&w->rng, 0.5f, 1.3f),
              rng_range(&w->rng, 0.02f, 0.06f), 0.f, COL(200, 225, 230, 190), 1, -12.f, 0.5f, g->as->flash);
    }
}

void fx_muzzle_flash(Game *g, Vec3 pos) {
    World *w = g->w;
    spawn(w, pos, v3(0, 0, 0), 0.07f, 0.34f, 0.05f, COL(255, 230, 170, 255), 1, 0, 0, g->as->flash);
    for (int i = 0; i < 4; i++) {
        Vec3 d = v3(rng_range(&w->rng, -1, 1), rng_range(&w->rng, -1, 1), rng_range(&w->rng, -1, 1));
        spawn(w, pos, vscale(vnorm(d), rng_range(&w->rng, 1.f, 3.f)), 0.13f, 0.05f, 0.1f,
              COL(255, 190, 90, 220), 1, 0, 3.f, g->as->flash);
    }
    fx_smoke(g, pos, 2);
}

void fx_impact(Game *g, Vec3 pos, Vec3 nrm, int material) {
    switch (material) {
        case MAT_GLASS:
            fx_glass(g, pos, 12);
            break;
        case MAT_WALL_METAL:
        case MAT_STEEL:
        case MAT_DIAMOND:
        case MAT_WALL_RUST:
            fx_spark(g, pos, nrm, v3(1.0f, 0.72f, 0.30f), 10);
            break;
        case MAT_WOOD:
        case MAT_PLYWOOD:
        case MAT_CARDBOARD:
            fx_dust(g, pos, 6);
            fx_decal(g, vmad(pos, nrm, 0.01f), nrm, g->as->blood, 0.14f, COL(90, 70, 45, 200));
            break;
        case MAT_SKIN_ZOMBIE:
        case MAT_FLESH:
        case MAT_SKIN_BRUTE:
        case MAT_SKIN_PALE:
            fx_blood(g, pos, nrm, 8);
            break;
        case MAT_SHELF_GOODS:
        case MAT_BOTTLES:
        case MAT_FREEZER_GOODS:
            fx_spark(g, pos, nrm, v3(0.8f, 0.9f, 1.0f), 6);
            fx_dust(g, pos, 4);
            break;
        default:
            fx_dust(g, pos, 5);
            fx_spark(g, pos, nrm, v3(0.9f, 0.85f, 0.7f), 3);
            break;
    }
}

void fx_decal(Game *g, Vec3 pos, Vec3 nrm, const Tex *tex, float size, uint32_t color) {
    World *w = g->w;
    int slot = -1;
    for (int i = 0; i < MAX_DECALS; i++) {
        if (!w->decals[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        // overwrite the oldest
        float oldest = -1e9f;
        for (int i = 0; i < MAX_DECALS; i++) {
            if (w->decals[i].life > oldest) { oldest = w->decals[i].life; slot = i; }
        }
    }
    Decal *d = &w->decals[slot];
    d->active = 1;
    d->pos = pos;
    d->nrm = nrm;
    d->size = size;
    d->tex = tex;
    d->color = color;
    d->life = 0.f;
}

void fx_update(Game *g, float dt) {
    World *w = g->w;
    for (int i = 0; i < w->particle_count; i++) {
        Particle *p = &w->particles[i];
        if (p->life <= 0.f) continue;
        p->life -= dt;
        p->vel.y += p->gravity * dt;
        p->vel = vscale(p->vel, 1.f - clampf(p->drag * dt, 0.f, 0.95f));
        p->pos = vmad(p->pos, p->vel, dt);
        p->size += p->grow * dt;
        p->rot += p->rot_spd * dt;
        // floor bounce
        if (p->pos.y < 0.02f && p->gravity < 0.f) {
            p->pos.y = 0.02f;
            p->vel.y = fabsf(p->vel.y) * 0.18f;
            p->vel.x *= 0.5f;
            p->vel.z *= 0.5f;
            if (p->size > 0.05f) {
                fx_decal(g, v3(p->pos.x, 0.011f, p->pos.z), v3(0, 1, 0), g->as->blood, 0.5f, COL(90, 10, 10, 190));
                p->life = 0.f;
            }
        }
    }
    // compact the array occasionally
    if (w->particle_count > MAX_PARTICLES - 16) {
        int n = 0;
        for (int i = 0; i < w->particle_count; i++)
            if (w->particles[i].life > 0.f) w->particles[n++] = w->particles[i];
        w->particle_count = n;
    }
    for (int i = 0; i < MAX_DECALS; i++) {
        if (w->decals[i].active) {
            w->decals[i].life += dt;
            if (w->decals[i].life > 55.f) w->decals[i].active = 0;
        }
    }
    // weather
    if (g->rain_amount > 0.02f) {
        for (int i = 0; i < 26; i++) {
            float a = rng_f(&w->rng) * 6.28f;
            float r = rng_range(&w->rng, 0.5f, 14.f);
            Vec3 p = v3(w->players[0].pos.x + cosf(a) * r, rng_range(&w->rng, 2.f, 7.f), w->players[0].pos.z + sinf(a) * r);
            spawn(w, p, v3(rng_range(&w->rng, -0.6f, 0.6f), -18.f, rng_range(&w->rng, -0.6f, 0.6f)),
                  0.65f, 0.02f, 0.f, COL(150, 165, 185, 90), 0, -2.f, 0.f, g->as->rain);
        }
    }
}
