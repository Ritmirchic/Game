/* ============================================================================
   SPACE MINER — core/world.c
   Sector backgrounds, asteroid field generation, pickups, hazards, station.
   ========================================================================== */
#include "state.h"

/* ------------------------------------------------------------------ noise -- */
static f32 wnoise(f32 x, f32 y) {
    /* cheap smooth 2D value noise for nebula turbulence */
    int ix = sm_ifloor(x), iy = sm_ifloor(y);
    f32 fx = x - (f32)ix, fy = y - (f32)iy;
    f32 sx = sm_smoothstep(fx), sy = sm_smoothstep(fy);
    f32 a = hash1((u32)(ix * 374761393 + iy * 668265263));
    f32 b = hash1((u32)((ix + 1) * 374761393 + iy * 668265263));
    f32 c = hash1((u32)(ix * 374761393 + (iy + 1) * 668265263));
    f32 d = hash1((u32)((ix + 1) * 374761393 + (iy + 1) * 668265263));
    return SM_LERP(SM_LERP(a, b, sx), SM_LERP(c, d, sx), sy);
}

static int sector_res_by_weight(Game *g, int sector) {
    const SectorDef *s = &k_sectors[sector];
    f32 total = 0.f;
    for (int i = 0; i < s->pool_count; i++) total += s->pool[i].weight;
    f32 r = rng_range(&g->rng, 0.f, total);
    for (int i = 0; i < s->pool_count; i++) {
        r -= s->pool[i].weight;
        if (r <= 0.f) return s->pool[i].res;
    }
    return s->pool[0].res;
}

/* ------------------------------------------------------------------ spawn -- */
void world_clear_entities(Game *g) {
    memset(g->asteroids, 0, sizeof(g->asteroids));
    memset(g->enemies, 0, sizeof(g->enemies));
    memset(g->projectiles, 0, sizeof(g->projectiles));
    memset(g->pickups, 0, sizeof(g->pickups));
    memset(g->drones, 0, sizeof(g->drones));
    memset(g->hazards, 0, sizeof(g->hazards));
    fx_clear(g);
}

static int design_for_resource(int res) {
    /* pick an asteroid design whose palette suits the resource */
    switch (res) {
        case RES_ICE: return 3;
        case RES_CRYSTAL: case RES_VOIDCRYSTAL: return 6;
        case RES_GOLD: case RES_COPPER: return 1;
        case RES_IRON: case RES_TITANIUM: case RES_SILVER: return 4;
        case RES_PLATINUM: case RES_DARKMATTER: return 7;
        case RES_URANIUM: return 2;
        case RES_PLASMA: case RES_ANTIMATTER: return 5;
        case RES_SALVAGE: return 5;
        default: return (int)(hash1((u32)res * 97u) * 8.f) % 8;
    }
}

void world_spawn_asteroid(Game *g, Vec2 pos, int design, int res, f32 radius, f32 amount) {
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (a->active) continue;
        a->active = 1;
        a->id = (u32)(g->frame * 2654435761u + (u32)i * 40503u + 7u);
        a->pos = pos;
        f32 dir = rng_range(&g->rng, 0.f, SM_TAU);
        f32 spd = rng_range(&g->rng, 1.5f, 12.f);
        a->vel = v2mul(v2from_angle(dir), spd);
        a->angle = rng_range(&g->rng, 0.f, SM_TAU);
        a->spin = rng_range(&g->rng, -0.35f, 0.35f);
        a->radius = radius;
        a->design = design & 7;
        a->res = res;
        a->amount_total = amount;
        a->amount = amount;
        a->hull_max = amount * k_resources[res].hardness * 4.5f + radius * 1.4f;
        a->hull = a->hull_max;
        a->flash = 0.f;
        a->crack = 0.f;
        a->spawn = 0.f;
        a->seed = rng_u32(&g->rng);
        a->rich = 0;
        return;
    }
}

static int point_free_of(Game *g, Vec2 p, f32 radius) {
    if (v2dist(p, g->player.pos) < radius + 190.f) return 0;
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (!a->active) continue;
        if (v2dist(p, a->pos) < radius + a->radius + 26.f) return 0;
    }
    for (int i = 0; i < SM_MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->active) continue;
        if (v2dist(p, e->pos) < radius + e->radius + 30.f) return 0;
    }
    if (v2dist(p, v2(0, 0)) < radius + 420.f) return 0;   /* keep the station clear */
    return 1;
}

void world_populate(Game *g) {
    const SectorDef *s = &k_sectors[g->sector];
    int want = s->asteroid_count;
    for (int tries = 0; tries < want * 14 && want > 0; tries++) {
        int count = 0;
        for (int i = 0; i < SM_MAX_ASTEROIDS; i++) if (g->asteroids[i].active) count++;
        if (count >= want) break;
        f32 ang = rng_range(&g->rng, 0.f, SM_TAU);
        f32 dist = rng_range(&g->rng, 780.f, SM_WORLD_RADIUS * 0.94f);
        Vec2 p = v2mul(v2from_angle(ang), dist);
        f32 r = rng_range(&g->rng, 26.f, 62.f);
        if (hash1(rng_u32(&g->rng)) > 0.86f) r = rng_range(&g->rng, 64.f, 96.f);
        if (!point_free_of(g, p, r)) continue;
        int res = sector_res_by_weight(g, g->sector);
        f32 amount = (r * r) * 0.0072f * rng_range(&g->rng, 0.7f, 1.5f);
        /* rarer ore lives in bigger rocks */
        if (k_resources[res].rarity >= 6) amount *= 0.45f;
        world_spawn_asteroid(g, p, design_for_resource(res), res, r, SM_MAX(4.f, amount));
    }
    /* enemies */
    int ecount = s->enemy_density * 2 + 2;
    for (int i = 0; i < ecount; i++) {
        int type = s->enemy_types[rng_range_u32(&g->rng, (u32)s->enemy_type_count)];
        f32 ang = rng_range(&g->rng, 0.f, SM_TAU);
        f32 dist = rng_range(&g->rng, 1400.f, SM_WORLD_RADIUS * 0.9f);
        world_spawn_enemy(g, type, v2mul(v2from_angle(ang), dist));
    }
}

void world_spawn_enemy(Game *g, int type, Vec2 pos) {
    for (int i = 0; i < SM_MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        if (e->active) continue;
        const EnemyDef *d = &k_enemies[type];
        f32 diff = sector_difficulty(g->sector);
        e->active = 1;
        e->type = type;
        e->pos = pos;
        e->vel = v2(0, 0);
        e->angle = rng_range(&g->rng, 0.f, SM_TAU);
        e->angvel = 0.f;
        e->hull_max = d->hull * (0.8f + diff * 0.35f);
        e->hull = e->hull_max;
        e->fire_timer = rng_range(&g->rng, 0.6f, 2.4f);
        e->burst_left = 0;
        e->ai_timer = 0.f;
        e->ai_state = 0;
        e->strafe_dir = v2from_angle(rng_range(&g->rng, 0.f, SM_TAU));
        e->hit_flash = 0.f;
        e->spawn = 1.f;   /* warp-in animation */
        e->radius = 26.f * d->scale + 12.f;
        e->tether = 0.f;
        return;
    }
}

/* --------------------------------------------------------------- entering -- */
void world_enter_sector(Game *g, int sector) {
    g->sector = SM_CLAMP(sector, 0, SECTOR_COUNT - 1);
    g->sector_entered[g->sector] = 1;
    world_clear_entities(g);
    g->player.pos = v2(0, -760.f);
    g->player.vel = v2(0, 0);
    g->player.angle = SM_PI * 0.5f;
    g->cam.warp = 1.f;
    g->cam.shake = 0.f;
    g->sector_time = 0.f;
    g->spawn_timer = 3.f;
    g->hazard_timer = 2.f;
    g->toast_timer = 0.f;
    world_populate(g);
    snd_music(g, k_sectors[g->sector].music, 1.6f);
    snd_play(g, "sfx_warp", 0.7f, 0.f, 1.f);
}

/* ------------------------------------------------------------------ update -- */
static void pickup_spawn(Game *g, Vec2 pos, int res, f32 amount) {
    for (int i = 0; i < SM_MAX_PICKUPS; i++) {
        Pickup *p = &g->pickups[i];
        if (p->active) continue;
        p->active = 1;
        p->pos = pos;
        p->vel = v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), rng_range(&g->rng, 20.f, 90.f));
        p->res = res;
        p->amount = amount;
        p->life = 26.f;
        p->magnet = 0.f;
        p->angle = rng_range(&g->rng, 0.f, SM_TAU);
        p->spin = rng_range(&g->rng, -2.5f, 2.5f);
        p->scale = 0.5f + SM_MIN(1.f, amount / 6.f) * 0.5f;
        return;
    }
}

void world_release_ore(Game *g, Asteroid *a, f32 amount) {
    if (amount <= 0.f) return;
    int chunks = SM_MAX(1, SM_MIN(4, (int)(amount / 2.2f) + 1));
    f32 per = amount / (f32)chunks;
    for (int i = 0; i < chunks; i++) {
        Vec2 off = v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), a->radius * 0.55f);
        pickup_spawn(g, v2add(a->pos, off), a->res, per);
    }
}

static void update_pickups(Game *g, f32 dt) {
    f32 pickup_radius = player_stat_pickup_radius(g);
    for (int i = 0; i < SM_MAX_PICKUPS; i++) {
        Pickup *p = &g->pickups[i];
        if (!p->active) continue;
        p->life -= dt;
        p->angle += p->spin * dt;
        f32 d = v2dist(p->pos, g->player.pos);
        if (d < pickup_radius) {
            p->magnet = SM_MIN(1.f, p->magnet + dt * 3.4f);
            Vec2 to = v2norm(v2sub(g->player.pos, p->pos));
            f32 pull = 340.f + (pickup_radius - d) * 2.2f;
            p->vel = v2add(v2mul(p->vel, sm_decay(2.2f, dt)), v2mul(to, pull * dt * (0.6f + p->magnet)));
        } else {
            p->magnet = SM_MAX(0.f, p->magnet - dt * 2.f);
        }
        p->vel = v2mul(p->vel, sm_decay(0.8f, dt));
        p->pos = v2add(p->pos, v2mul(p->vel, dt));
        d = v2dist(p->pos, g->player.pos);
        if (d < 26.f) {
            if (player_cargo_space(g) <= 0) {
                /* bounce off when the hold is full */
                Vec2 away = v2norm(v2sub(p->pos, g->player.pos));
                p->vel = v2mul(away, 80.f);
                if (g->toast_timer <= 0.f) game_toast(g, "CARGO FULL", SM_RGB(255, 120, 90));
            } else {
                int units = (int)ceilf(p->amount);
                if (units < 1) units = 1;
                int space = player_cargo_space(g);
                if (units > space) units = space;
                player_add_cargo(g, p->res, (f32)units);
                char buf[64];
                sm_sprintf(buf, sizeof(buf), "+%d %s", units, k_resources[p->res].short_name);
                fx_text(g, p->pos, buf, k_resources[p->res].color);
                snd_play(g, "sfx_pickup", 0.35f, rng_range(&g->rng, -0.5f, 0.5f), rng_range(&g->rng, 0.92f, 1.14f));
                p->active = 0;
                continue;
            }
        }
        if (p->life <= 0.f) p->active = 0;
    }
}

static void update_asteroids(Game *g, f32 dt) {
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (!a->active) continue;
        a->pos = v2add(a->pos, v2mul(a->vel, dt));
        a->angle += a->spin * dt;
        a->spawn = SM_MIN(1.f, a->spawn + dt * 1.6f);
        a->flash = SM_MAX(0.f, a->flash - dt * 4.5f);
        f32 dmg = 1.f - a->hull / SM_MAX(1.f, a->hull_max);
        a->crack = sm_approach(a->crack, dmg, dt * 1.4f);
        /* keep the field inside the sector */
        f32 d = v2len(a->pos);
        if (d > SM_WORLD_RADIUS) {
            Vec2 in = v2norm(v2mul(a->pos, -1.f));
            a->vel = v2add(a->vel, v2mul(in, 26.f * dt));
        }
    }
}

static void update_hazards(Game *g, f32 dt) {
    const SectorDef *s = &k_sectors[g->sector];
    if (s->hazard == HAZARD_NONE) return;
    g->hazard_timer -= dt;
    if (g->hazard_timer <= 0.f) {
        switch (s->hazard) {
            case HAZARD_CRYSTAL_DRIFT: {
                g->hazard_timer = rng_range(&g->rng, 2.2f, 4.5f);
                f32 ang = rng_range(&g->rng, 0.f, SM_TAU);
                Vec2 p = v2add(g->player.pos, v2mul(v2from_angle(ang), rng_range(&g->rng, 520.f, 760.f)));
                for (int i = 0; i < SM_MAX_HAZARDS; i++) {
                    if (g->hazards[i].active) continue;
                    Hazard *h = &g->hazards[i];
                    h->active = 1;
                    h->pos = p;
                    h->vel = v2mul(v2norm(v2sub(g->player.pos, p)), rng_range(&g->rng, 210.f, 330.f));
                    h->life = h->max_life = 3.2f;
                    h->color = SM_RGB(150, 235, 255);
                    h->intensity = 1.f;
                    break;
                }
            } break;
            case HAZARD_DEBRIS_FIELD: {
                g->hazard_timer = rng_range(&g->rng, 1.6f, 3.2f);
                f32 ang = rng_range(&g->rng, 0.f, SM_TAU);
                Vec2 p = v2add(g->player.pos, v2mul(v2from_angle(ang), rng_range(&g->rng, 620.f, 880.f)));
                for (int i = 0; i < SM_MAX_HAZARDS; i++) {
                    if (g->hazards[i].active) continue;
                    Hazard *h = &g->hazards[i];
                    h->active = 1;
                    h->pos = p;
                    h->vel = v2mul(v2norm(v2sub(g->player.pos, p)), rng_range(&g->rng, 300.f, 430.f));
                    h->life = h->max_life = 3.4f;
                    h->color = SM_RGB(200, 190, 170);
                    h->intensity = 1.4f;
                    break;
                }
            } break;
            case HAZARD_PLASMA_STORM: {
                g->hazard_timer = rng_range(&g->rng, 4.0f, 7.5f);
                /* lightning strike near the player, telegraphed */
                Vec2 target = v2add(g->player.pos, v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)),
                                                           rng_range(&g->rng, 60.f, 260.f)));
                for (int i = 0; i < SM_MAX_HAZARDS; i++) {
                    if (g->hazards[i].active) continue;
                    Hazard *h = &g->hazards[i];
                    h->active = 1;
                    h->pos = target;
                    h->vel = v2(0, 0);
                    h->life = h->max_life = 1.35f;
                    h->color = SM_RGB(255, 170, 90);
                    h->intensity = 2.f;
                    break;
                }
                snd_play(g, "sfx_alarm", 0.25f, 0.f, 1.3f);
            } break;
            case HAZARD_VOID_SURGE: {
                g->hazard_timer = rng_range(&g->rng, 5.f, 9.f);
                fx_ring(g, g->player.pos, 900.f, SM_RGB(150, 110, 255), 1.2f);
                f32 ang = rng_range(&g->rng, 0.f, SM_TAU);
                for (int i = 0; i < 3; i++) {
                    for (int k = 0; k < SM_MAX_HAZARDS; k++) {
                        if (g->hazards[k].active) continue;
                        Hazard *h = &g->hazards[k];
                        h->active = 1;
                        h->pos = v2add(g->player.pos, v2mul(v2from_angle(ang + i * 0.7f), 520.f));
                        h->vel = v2mul(v2from_angle(ang + i * 0.7f + 3.14f), 380.f);
                        h->life = h->max_life = 2.2f;
                        h->color = SM_RGB(170, 130, 255);
                        h->intensity = 1.6f;
                        break;
                    }
                }
            } break;
            default: break;
        }
    }
    for (int i = 0; i < SM_MAX_HAZARDS; i++) {
        Hazard *h = &g->hazards[i];
        if (!h->active) continue;
        h->life -= dt;
        h->pos = v2add(h->pos, v2mul(h->vel, dt));
        if (s->hazard == HAZARD_PLASMA_STORM) {
            if (h->life <= 1.0f && h->life > 0.98f) {
                /* strike */
                if (v2dist(h->pos, g->player.pos) < 190.f) player_damage(g, 14.f, h->pos);
                fx_explosion(g, h->pos, 44.f, SM_RGB(255, 190, 110), 0);
                snd_play(g, "sfx_explode_small", 0.5f, 0.f, 1.4f);
            }
        } else {
            if (v2dist(h->pos, g->player.pos) < 40.f + 26.f) {
                player_damage(g, (s->hazard == HAZARD_DEBRIS_FIELD ? 16.f : 10.f), h->pos);
                fx_explosion(g, h->pos, 24.f, h->color, 0);
                h->active = 0;
                continue;
            }
        }
        if (h->life <= 0.f) h->active = 0;
    }
}

void world_update(Game *g, f32 dt) {
    g->sector_time += dt;
    update_asteroids(g, dt);
    update_pickups(g, dt);
    update_hazards(g, dt);
    /* trickle respawns so the belt never runs dry */
    g->spawn_timer -= dt;
    if (g->spawn_timer <= 0.f) {
        g->spawn_timer = 7.5f;
        int count = 0;
        for (int i = 0; i < SM_MAX_ASTEROIDS; i++) if (g->asteroids[i].active) count++;
        if (count < k_sectors[g->sector].asteroid_count) world_populate(g);
        int enemies = 0;
        for (int i = 0; i < SM_MAX_ENEMIES; i++) if (g->enemies[i].active) enemies++;
        const SectorDef *s = &k_sectors[g->sector];
        int want = s->enemy_density + 2;
        if (enemies < want && g->sector_time > 12.f) {
            int type = s->enemy_types[rng_range_u32(&g->rng, (u32)s->enemy_type_count)];
            f32 ang = rng_range(&g->rng, 0.f, SM_TAU);
            Vec2 p = v2add(g->player.pos, v2mul(v2from_angle(ang), rng_range(&g->rng, 1500.f, 2100.f)));
            world_spawn_enemy(g, type, p);
        }
    }
    /* station proximity */
    f32 dstat = v2dist(g->player.pos, v2(0, 0));
    g->station_near = dstat < 420.f;
    g->station_prompt = sm_approach(g->station_prompt, g->station_near ? 1.f : 0.f, dt * 4.f);
}

int world_find_asteroid_near(Game *g, Vec2 p, f32 maxdist) {
    int best = -1;
    f32 bestd = maxdist * maxdist;
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (!a->active) continue;
        f32 d = v2dist2(p, a->pos) - a->radius * a->radius * 0.5f;
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

/* ------------------------------------------------------------------ render -- */
void world_render_background(Game *g) {
    Gfx *gfx = g->gfx;
    const SectorDef *s = &k_sectors[g->sector];
    gfx_fill_vgrad(gfx, rect(0, 0, (f32)gfx->w, (f32)gfx->h), s->bg_top, s->bg_bottom);

    /* ---- painted deep-space backdrop ---- */
    Sheet *sky = &g->assets.bg[g->sector];
    if (sky->px) {
        f32 scale = 1.16f;
        f32 sw = (f32)sky->w * scale, sh = (f32)sky->h * scale;
        /* the sky drifts slowly against the camera so the field still feels deep */
        f32 ox = (sw - (f32)gfx->w) * -0.5f - gfx->cam.x * 0.012f;
        f32 oy = (sh - (f32)gfx->h) * -0.5f - gfx->cam.y * 0.012f;
        ox = SM_CLAMP(ox, (f32)gfx->w - sw, 0.f);
        oy = SM_CLAMP(oy, (f32)gfx->h - sh, 0.f);
        gfx_blit(gfx, sky, 0, rect(ox, oy, sw, sh), 0xFFFFFFFF, 1.f, BLEND_ALPHA);
    }

    /* ---- drifting gas wisps for parallax depth ---- */
    Sheet *neb = &g->assets.bg_far[g->sector];
    if (neb->px) {
        f32 scale2 = 4.4f;
        f32 tw2 = (f32)neb->w * scale2, th2 = (f32)neb->h * scale2;
        f32 ox2 = fmodf(-gfx->cam.x * 0.045f * gfx->zoom + 160.f, tw2);
        f32 oy2 = fmodf(-gfx->cam.y * 0.045f * gfx->zoom + 90.f, th2);
        if (ox2 > 0) ox2 -= tw2;
        if (oy2 > 0) oy2 -= th2;
        for (f32 y = oy2; y < (f32)gfx->h; y += th2)
            for (f32 x = ox2; x < (f32)gfx->w; x += tw2)
                gfx_blit(gfx, neb, 0, rect(x, y, tw2 + 1, th2 + 1), s->accent, 0.18f, BLEND_ADD);
    }

    /* ---- stars ---- */
    for (int i = 0; i < g->star_count; i++) {
        f32 par = 0.22f + g->star_size[i] * 0.06f;
        Vec2 sp = v2(g->stars[i].x - gfx->cam.x * par, g->stars[i].y - gfx->cam.y * par);
        sp.x = sm_wrapf(sp.x, (f32)gfx->w + 40.f) - 20.f;
        sp.y = sm_wrapf(sp.y, (f32)gfx->h + 40.f) - 20.f;
        f32 size = g->star_size[i];
        f32 tw = 0.65f + 0.35f * sinf(g->time * (0.7f + g->star_twinkle[i] * 2.2f) + g->star_twinkle[i] * 12.f);
        u32 c = sm_col_scale(g->star_color[i], tw * 0.9f);
        if (size > 1.6f) {
            gfx_fill(gfx, rect(sp.x, sp.y, size, size), c);
            gfx_glow(gfx, sp, size * 5.5f, g->star_color[i], 0.35f * tw);
            /* diffraction spikes for the brightest stars */
            if (size > 2.3f) {
                gfx_line_add(gfx, v2(sp.x - size * 4.f, sp.y), v2(sp.x + size * 4.f, sp.y), 1.f, sm_col_scale(g->star_color[i], 0.30f * tw));
                gfx_line_add(gfx, v2(sp.x, sp.y - size * 4.f), v2(sp.x, sp.y + size * 4.f), 1.f, sm_col_scale(g->star_color[i], 0.30f * tw));
            }
        } else {
            gfx_fill(gfx, rect(sp.x, sp.y, size, size), c);
        }
    }

    /* ---- planets / distant bodies ---- */
    Sheet *planet = &g->assets.planet[g->sector];
    if (planet->px) {
        f32 t = (f32)g->sector;
        f32 baseScale = 1.05f + t * 0.12f;
        f32 pw = (f32)planet->w * baseScale;
        f32 cx = (f32)gfx->w * (0.80f + 0.04f * sinf(t * 2.1f)) - gfx->cam.x * 0.010f * gfx->zoom;
        f32 cy = (f32)gfx->h * (0.24f + 0.03f * cosf(t * 1.7f)) - gfx->cam.y * 0.010f * gfx->zoom;
        gfx_blit(gfx, planet, 0, rect(cx - pw * 0.5f, cy - pw * 0.5f, pw, pw), 0xFFFFFFFF, 1.f, BLEND_ALPHA);
        gfx_glow(gfx, v2(cx, cy), pw * 0.30f, s->accent, 0.05f);
    }

    /* ---- far dust motes ---- */
    for (int i = 0; i < g->dust_count; i++) {
        Vec2 dp = v2(g->dust[i].x - gfx->cam.x * 0.6f, g->dust[i].y - gfx->cam.y * 0.6f);
        dp.x = sm_wrapf(dp.x, (f32)gfx->w + 60.f) - 30.f;
        dp.y = sm_wrapf(dp.y, (f32)gfx->h + 60.f) - 30.f;
        gfx_glow(gfx, dp, 3.2f, s->dust_color, 0.35f);
    }
}

void world_render(Game *g) {
    Gfx *gfx = g->gfx;
    const SectorDef *s = &k_sectors[g->sector];

    /* sector tint over everything far away */
    gfx_fill_add(gfx, rect(0, 0, (f32)gfx->w, (f32)gfx->h), SM_RGBA(SM_GET_R(s->fog), SM_GET_G(s->fog), SM_GET_B(s->fog), 40));

    /* asteroid shadows / field */
    fx_render_back(g);

    /* station */
    if (g->assets.station.px) {
        f32 dist = v2dist(g->player.pos, v2(0, 0));
        f32 alpha = sm_saturate(1.f - (dist - 2600.f) / 3400.f);
        if (alpha > 0.01f) {
            Vec2 sp = gfx_w2s(gfx, v2(0, 0));
            f32 size = 300.f * gfx->zoom;
            f32 rot = g->time * 0.06f;
            gfx_glow(gfx, sp, size * 0.75f, s->accent, 0.22f * alpha);
            gfx_blit(gfx, &g->assets.station, 0, rect(sp.x - size * 0.5f, sp.y - size * 0.5f, size, size),
                     0xFFFFFFFF, alpha, BLEND_ALPHA);
            /* docking lights blink around the ring */
            for (int i = 0; i < 12; i++) {
                f32 a = rot + (f32)i * SM_TAU / 12.f;
                Vec2 lp = v2(sp.x + cosf(a) * size * 0.44f, sp.y + sinf(a) * size * 0.44f);
                f32 blink = 0.35f + 0.65f * (0.5f + 0.5f * sinf(g->time * 2.4f + i * 0.7f));
                gfx_glow(gfx, lp, 5.f * gfx->zoom, SM_RGB(255, 214, 140), 0.7f * blink * alpha);
            }
            /* "docking" prompt ring */
            if (g->station_prompt > 0.01f) {
                gfx_ring(gfx, sp, size * 0.55f, 1.5f, SM_RGBA(120, 230, 255, (u32)(150.f * g->station_prompt)));
            }
        }
    }

    /* asteroids */
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (!a->active) continue;
        Vec2 sp = gfx_w2s(gfx, a->pos);
        f32 draw = a->radius * 2.35f * gfx->zoom;
        if (sp.x < -draw || sp.y < -draw || sp.x > (f32)gfx->w + draw || sp.y > (f32)gfx->h + draw) continue;
        Sheet *sh = &g->assets.ast[a->design];
        if (!sh->px) continue;
        int frame = (int)(sm_wrapf(a->angle, SM_TAU) / SM_TAU * (f32)sh->frames) % sh->frames;
        f32 pop = 0.82f + 0.18f * sm_easeout(a->spawn);
        f32 w = draw * pop;
        u32 tint = 0xFFFFFFFF;
        if (a->flash > 0.f) {
            tint = sm_col_lerp(0xFFFFFFFF, SM_RGBA(255, 240, 210, 255), sm_saturate(a->flash));
        }
        f32 alpha = sm_saturate(a->spawn * 1.4f);
        gfx_blit(gfx, sh, frame, rect(sp.x - w * 0.5f, sp.y - w * 0.5f, w, w), tint, alpha, BLEND_ALPHA);
        /* ore glints: tiny coloured motes in the rock */
        f32 res_phase = (f32)(a->seed % 1000) * 0.01f;
        for (int k = 0; k < 3; k++) {
            f32 ang = res_phase + (f32)k * 2.1f + g->time * 0.4f;
            f32 rr = a->radius * (0.30f + 0.22f * (f32)k) * gfx->zoom;
            Vec2 gp = v2(sp.x + cosf(ang) * rr, sp.y + sinf(ang * 1.7f) * rr);
            f32 tw = 0.35f + 0.65f * (0.5f + 0.5f * sinf(g->time * 2.6f + (f32)k * 2.f + res_phase * 9.f));
            gfx_glow(gfx, gp, 7.f * gfx->zoom, k_resources[a->res].glow, 0.5f * tw * alpha);
        }
        /* fracture overlay as the rock takes damage */
        if (a->crack > 0.12f) {
            int lvl = a->crack > 0.72f ? 2 : (a->crack > 0.4f ? 1 : 0);
            Sheet *cr = &g->assets.ast_crack[lvl];
            if (cr->px) {
                f32 pulse = 0.55f + 0.45f * sinf(g->time * 7.f);
                gfx_blit(gfx, cr, 0, rect(sp.x - w * 0.5f, sp.y - w * 0.5f, w, w),
                         k_resources[a->res].glow, sm_saturate(a->crack) * 0.55f * pulse, BLEND_ADD);
            }
        }
        /* mining scorch glow */
        if (a->flash > 0.2f) {
            gfx_glow(gfx, sp, a->radius * 1.5f * gfx->zoom, k_resources[a->res].glow, 0.4f * a->flash);
        }
    }

    /* asteroid-friendly "scanner" HUD markers drawn in ui.c */

    /* hazards */
    for (int i = 0; i < SM_MAX_HAZARDS; i++) {
        Hazard *h = &g->hazards[i];
        if (!h->active) continue;
        Vec2 sp = gfx_w2s(gfx, h->pos);
        f32 t = 1.f - h->life / h->max_life;
        if (s->hazard == HAZARD_PLASMA_STORM) {
            f32 warn = sm_saturate(1.f - t * 1.4f);
            gfx_ring(gfx, sp, 60.f * gfx->zoom * (1.f - t * 0.5f), 2.f, SM_RGBA(255, 200, 120, (u32)(190.f * warn)));
            gfx_glow(gfx, sp, 90.f * gfx->zoom, h->color, 0.3f * warn);
            if (t > 0.28f) {
                /* the bolt itself */
                Vec2 p0 = v2(sp.x, 0);
                Vec2 prev = p0;
                for (int k = 1; k <= 9; k++) {
                    f32 f = (f32)k / 9.f;
                    Vec2 p = v2(sp.x + sinf(f * 9.f + g->time * 30.f) * 26.f * (1.f - f), sp.y * f);
                    gfx_line_add(gfx, prev, p, 3.5f, SM_RGBA(255, 190, 120, 220));
                    gfx_line_add(gfx, prev, p, 1.4f, SM_RGBA(255, 255, 255, 240));
                    prev = p;
                }
            }
        } else if (s->hazard == HAZARD_VOID_SURGE) {
            gfx_glow(gfx, sp, 54.f * gfx->zoom, h->color, 0.7f);
            gfx_ring(gfx, sp, 30.f * gfx->zoom, 2.f, SM_RGBA(190, 160, 255, 160));
        } else {
            f32 ang = v2angle(h->vel);
            Vec2 dir = v2from_angle(ang);
            f32 len = 16.f * gfx->zoom;
            Vec2 tip = v2add(sp, v2mul(dir, len));
            gfx_line_add(gfx, sp, tip, 3.f, h->color);
            gfx_line_add(gfx, v2sub(sp, v2mul(dir, len * 0.2f)), sp, 4.f, sm_col_scale(h->color, 0.4f));
            gfx_glow(gfx, tip, 12.f * gfx->zoom, h->color, 0.7f);
        }
    }
}
