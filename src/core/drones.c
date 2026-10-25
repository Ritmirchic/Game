/* ============================================================================
   SPACE MINER — core/drones.c
   Autonomous mining drones: they seek rocks, cut them, haul ore back and
   deposit it into the ship's hold. Each drone is a real entity in the world.
   ========================================================================== */
#include "state.h"


void drone_buy(Game *g, int type) {
    int slots = player_stat_drone_slots(g);
    if (g->save.drone_count >= SM_MAX_DRONES || g->save.drone_count >= slots) {
        game_toast(g, "NO FREE DRONE SLOTS", SM_RGB(255, 140, 100));
        snd_play(g, "sfx_error", 0.5f, 0.f, 1.f);
        return;
    }
    s64 cost = k_drones[type].cost;
    if (g->save.credits < cost) {
        game_toast(g, "NOT ENOUGH CREDITS", SM_RGB(255, 140, 100));
        snd_play(g, "sfx_error", 0.5f, 0.f, 1.f);
        return;
    }
    g->save.credits -= cost;
    g->save.drone_owned[type]++;
    g->save.drone_count++;
    /* spawn it next to the ship */
    for (int i = 0; i < SM_MAX_DRONES; i++) {
        Drone *d = &g->drones[i];
        if (d->active) continue;
        d->active = 1;
        d->type = type;
        d->pos = v2add(g->player.pos, v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)));
        d->vel = v2(0, 0);
        d->angle = 0;
        d->state = DRONE_SEEK;
        d->state_timer = 0.f;
        d->target_id = 0;
        d->cargo_amount = 0.f;
        d->cargo_res = -1;
        d->mining_progress = 0.f;
        d->hit_flash = 0.f;
        break;
    }
    game_toast(g, type == 0 ? "HARVESTER DRONE ONLINE" : "EXCAVATOR DRONE ONLINE", SM_RGB(140, 240, 255));
    snd_play(g, "sfx_buy", 0.7f, 0.f, 1.f);
    ach_check(g);
}

static int find_target_asteroid(Game *g, Drone *d) {
    int best = -1;
    f32 bestd = 2400.f * 2400.f;
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (!a->active) continue;
        /* skip rocks already claimed by another drone */
        int claimed = 0;
        for (int k = 0; k < SM_MAX_DRONES; k++) {
            Drone *o = &g->drones[k];
            if (!o->active || o == d) continue;
            if (o->target_id == a->id && o->state == DRONE_MINE) { claimed = 1; break; }
        }
        if (claimed) continue;
        f32 score = v2dist2(d->pos, a->pos) - a->amount * 18.f;
        if (score < bestd) { bestd = score; best = i; }
    }
    return best;
}

static Asteroid *asteroid_by_id(Game *g, u32 id) {
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++)
        if (g->asteroids[i].active && g->asteroids[i].id == id) return &g->asteroids[i];
    return NULL;
}

static int index_by_id(Game *g, u32 id) {
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++)
        if (g->asteroids[i].active && g->asteroids[i].id == id) return i;
    return -1;
}

void drones_update(Game *g, f32 dt) {
    for (int i = 0; i < SM_MAX_DRONES; i++) {
        Drone *d = &g->drones[i];
        if (!d->active) continue;
        const DroneDef *def = &k_drones[d->type];
        Player *pl = &g->player;
        d->state_timer += dt;
        d->hit_flash = SM_MAX(0.f, d->hit_flash - dt * 2.f);
        f32 speed = def->speed * (1.f + 0.12f * (f32)g->save.upgrades[UPG_DRONE_TECH]);
        Vec2 goal = d->pos;
        f32 arrive = 40.f;

        switch (d->state) {
            case DRONE_SEEK: {
                Asteroid *a = asteroid_by_id(g, d->target_id);
                if (!a) {
                    int idx = find_target_asteroid(g, d);
                    if (idx < 0) {
                        /* nothing to do: loiter near the ship */
                        goal = v2add(pl->pos, v2mul(v2from_angle(g->time * 0.5f + (f32)i * 1.4f), 120.f + (f32)i * 26.f));
                        arrive = 24.f;
                        d->state_timer = 0.f;
                    } else {
                        d->target_id = g->asteroids[idx].id;
                        a = &g->asteroids[idx];
                    }
                }
                if (a) {
                    goal = v2add(a->pos, v2mul(v2norm(v2sub(d->pos, a->pos)), a->radius * 0.72f));
                    arrive = 26.f;
                    if (v2dist(d->pos, a->pos) < a->radius + 46.f) {
                        d->state = DRONE_MINE;
                        d->state_timer = 0.f;
                        d->mining_progress = 0.f;
                    }
                }
            } break;
            case DRONE_MINE: {
                int idx = index_by_id(g, d->target_id);
                if (idx < 0) { d->state = DRONE_SEEK; d->target_id = 0; break; }
                Asteroid *a = &g->asteroids[idx];
                Vec2 hover = v2add(a->pos, v2mul(v2norm(v2sub(d->pos, a->pos)), a->radius * 0.72f));
                goal = hover;
                arrive = 18.f;
                f32 dist = v2dist(d->pos, a->pos);
                if (dist < a->radius + 70.f) {
                    /* cutting beam */
                    Vec2 hit = v2add(a->pos, v2mul(v2norm(v2sub(d->pos, a->pos)), a->radius * 0.6f));
                    fx_beam(g, v2add(d->pos, v2mul(v2from_angle(v2angle(v2sub(hit, d->pos))), 10.f)), hit,
                            1.4f, SM_RGB(120, 240, 255), 0.06f, BEAM_MINING);
                    f32 dps = def->mining_dps * (1.f + 0.10f * (f32)g->save.upgrades[UPG_LASER_POWER]);
                    asteroid_damage(g, idx, dps * dt / k_resources[a->res].hardness, hit, dt);
                    d->mining_progress += dt;
                    /* the drone scoops part of the yield into its own hopper */
                    if (d->cargo_amount < (f32)def->capacity) {
                        f32 take = dps * dt * 0.45f;
                        if (a->amount > take) {
                            a->amount -= take;
                            if (d->cargo_res != a->res && d->cargo_amount <= 0.01f) d->cargo_res = a->res;
                            if (d->cargo_res == a->res) d->cargo_amount += take;
                        }
                    } else {
                        d->state = DRONE_RETURN;
                        d->state_timer = 0.f;
                    }
                    if (hash1(g->frame * 7u + (u32)i) > 0.7f)
                        fx_spark(g, hit, v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), 80.f),
                                 k_resources[a->res].glow, 1, 60.f);
                } else if (dist > a->radius + 220.f) {
                    d->state = DRONE_SEEK;
                }
                if (d->state_timer > 22.f) { d->state = DRONE_RETURN; d->state_timer = 0.f; }
            } break;
            case DRONE_RETURN: {
                goal = pl->pos;
                arrive = 46.f;
                if (v2dist(d->pos, pl->pos) < 64.f) {
                    d->state = DRONE_UNLOAD;
                    d->state_timer = 0.f;
                }
            } break;
            case DRONE_UNLOAD: {
                goal = pl->pos;
                arrive = 40.f;
                if (d->cargo_amount > 0.5f && d->cargo_res >= 0) {
                    int units = (int)ceilf(d->cargo_amount);
                    int space = player_cargo_space(g);
                    if (space > 0) {
                        int give = SM_MIN(units, space);
                        player_add_cargo(g, d->cargo_res, (f32)give);
                        d->cargo_amount -= (f32)give;
                        if (hash1(g->frame * 11u) > 0.6f) {
                            char buf[48];
                            sm_sprintf(buf, sizeof(buf), "+%d", give);
                            fx_text(g, d->pos, buf, k_resources[d->cargo_res].color);
                        }
                        fx_beam(g, d->pos, pl->pos, 1.0f, k_resources[d->cargo_res].glow, 0.04f, BEAM_WEAPON);
                    }
                    if (d->cargo_amount <= 0.5f) { d->cargo_amount = 0.f; d->cargo_res = -1; }
                } else {
                    d->cargo_amount = 0.f;
                    d->cargo_res = -1;
                    d->state = DRONE_SEEK;
                    d->target_id = 0;
                    d->state_timer = 0.f;
                }
                if (d->state_timer > 4.f) { d->state = DRONE_SEEK; d->target_id = 0; d->state_timer = 0.f; }
            } break;
            default: break;
        }

        /* steering */
        Vec2 to = v2sub(goal, d->pos);
        f32 dist = v2len(to);
        if (dist > arrive * 0.5f) {
            Vec2 want = v2mul(v2norm(to), speed * SM_MIN(1.f, dist / 130.f));
            d->vel = v2lerp(d->vel, want, SM_MIN(1.f, dt * 3.2f));
        } else {
            d->vel = v2mul(d->vel, sm_decay(3.0f, dt));
        }
        /* avoid stacking on top of each other */
        for (int k = 0; k < SM_MAX_DRONES; k++) {
            if (k == i || !g->drones[k].active) continue;
            Vec2 away = v2sub(d->pos, g->drones[k].pos);
            f32 dd = v2len(away);
            if (dd < 46.f && dd > 0.01f) d->vel = v2add(d->vel, v2mul(away, (46.f - dd) * 2.4f * dt * 60.f / 60.f));
        }
        d->pos = v2add(d->pos, v2mul(d->vel, dt));
        f32 target_angle = v2len(d->vel) > 12.f ? v2angle(d->vel) : d->angle;
        if (d->state == DRONE_MINE) {
            Asteroid *a = asteroid_by_id(g, d->target_id);
            if (a) target_angle = v2angle(v2sub(a->pos, d->pos));
        } else if (d->state == DRONE_UNLOAD || d->state == DRONE_RETURN) {
            target_angle = v2angle(v2sub(pl->pos, d->pos));
        }
        f32 da = sm_angle_diff(target_angle, d->angle);
        d->angle += SM_CLAMP(da, -3.4f * dt, 3.4f * dt);
    }
}

void drones_render(Game *g) {
    Gfx *gfx = g->gfx;
    for (int i = 0; i < SM_MAX_DRONES; i++) {
        Drone *d = &g->drones[i];
        if (!d->active) continue;
        const DroneDef *def = &k_drones[d->type];
        Sheet *sh = &g->assets.drone[d->type];
        if (!sh->px) continue;
        Vec2 sp = gfx_w2s(gfx, d->pos);
        f32 size = (f32)sh->w * def->scale * 2.3f * gfx->zoom;
        int frame = (int)(sm_wrapf(d->angle + SM_PI * 0.5f, SM_TAU) / SM_TAU * (f32)sh->frames) % sh->frames;
        /* thruster */
        Vec2 back = v2add(sp, v2mul(v2from_angle(d->angle + SM_PI), size * 0.3f));
        gfx_glow(gfx, back, size * 0.22f, SM_RGB(120, 220, 255), 0.7f);
        gfx_blit(gfx, sh, frame, rect(sp.x - size * 0.5f, sp.y - size * 0.5f, size, size),
                 0xFFFFFFFF, 1.f, BLEND_ALPHA);
        /* state indicator */
        u32 col = SM_RGB(140, 240, 255);
        if (d->state == DRONE_MINE) col = SM_RGB(255, 200, 90);
        else if (d->state == DRONE_RETURN || d->state == DRONE_UNLOAD) col = SM_RGB(140, 255, 160);
        gfx_ring(gfx, sp, size * 0.56f, 1.2f, SM_RGBA(SM_GET_R(col), SM_GET_G(col), SM_GET_B(col), 90));
        /* cargo bar */
        if (d->cargo_amount > 0.f) {
            f32 frac = sm_saturate(d->cargo_amount / (f32)def->capacity);
            Rect bar = rect(sp.x - size * 0.3f, sp.y + size * 0.52f, size * 0.6f, 3.f);
            gfx_fill(gfx, bar, SM_RGBA(10, 20, 30, 200));
            gfx_fill(gfx, rect(bar.x, bar.y, bar.w * frac, bar.h), k_resources[d->cargo_res < 0 ? 0 : d->cargo_res].color);
        }
    }
}
