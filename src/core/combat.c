/* ============================================================================
   SPACE MINER — core/combat.c
   Weapons, projectiles, enemy AI, asteroid damage and destruction.
   ========================================================================== */
#include "state.h"

/* --------------------------------------------------------------- weaponry -- */
void combat_spawn_projectile(Game *g, Vec2 pos, Vec2 vel, f32 dmg, int kind, int from_player) {
    for (int i = 0; i < SM_MAX_PROJECTILES; i++) {
        Projectile *p = &g->projectiles[i];
        if (p->active) continue;
        p->active = 1;
        p->pos = pos;
        p->vel = vel;
        p->damage = dmg;
        p->kind = kind;
        p->from_player = from_player;
        p->max_life = 2.6f;
        p->life = p->max_life;
        p->radius = (kind == PROJ_LANCE) ? 12.f : (kind == PROJ_MISSILE ? 9.f : 6.f);
        p->homing = (kind == PROJ_MISSILE) ? 2.2f : 0.f;
        p->trail_timer = 0.f;
        return;
    }
}

void combat_fire_weapon(Game *g, Vec2 origin, f32 angle) {
    const WeaponDef *w = &k_weapons[g->save.weapon];
    f32 dmg = player_stat_weapon_damage(g);
    Vec2 dir = v2from_angle(angle);
    int kind = PROJ_BOLT;
    int shots = 1;
    f32 spread = 0.f;
    switch (g->save.weapon) {
        case WEAPON_PULSE: kind = PROJ_BOLT; shots = 1; break;
        case WEAPON_LANCE: kind = PROJ_LANCE; shots = 1; break;
        case WEAPON_ARC: kind = PROJ_PLASMA; shots = 3; spread = 0.12f; break;
        default: break;
    }
    f32 muzzle = v2len(origin) > 0.f ? 0.f : 0.f;
    (void)muzzle;
    for (int i = 0; i < shots; i++) {
        f32 a = angle + (shots > 1 ? ((f32)i - (f32)(shots - 1) * 0.5f) * spread : 0.f);
        Vec2 v = v2add(v2mul(v2from_angle(a), w->speed), g->player.vel);
        combat_spawn_projectile(g, origin, v, dmg, kind, 1);
    }
    fx_spark(g, origin, v2mul(dir, 90.f), w->color, 4, 120.f);
    g->cam.shake = SM_MAX(g->cam.shake, 2.2f);
    const char *sfx = g->save.weapon == WEAPON_LANCE ? "sfx_lance" : (g->save.weapon == WEAPON_ARC ? "sfx_arc" : "sfx_laser");
    snd_play(g, sfx, 0.40f, rng_range(&g->rng, -0.25f, 0.25f), rng_range(&g->rng, 0.96f, 1.06f));
}

/* -------------------------------------------------------------- asteroid --- */
void asteroid_damage(Game *g, int idx, f32 amount, Vec2 hit_pos, f32 dt) {
    if (idx < 0 || idx >= SM_MAX_ASTEROIDS) return;
    Asteroid *a = &g->asteroids[idx];
    if (!a->active) return;
    a->hull -= amount;
    a->flash = SM_MIN(1.f, a->flash + amount * 0.02f + dt * 1.4f);
    f32 dmg_frac = 1.f - SM_MAX(0.f, a->hull) / SM_MAX(1.f, a->hull_max);
    f32 release_target = a->amount_total * dmg_frac;
    f32 released = a->amount_total - a->amount;
    if (release_target - released > 0.6f) {
        f32 give = SM_MIN(a->amount, release_target - released);
        a->amount -= give;
        world_release_ore(g, a, give);
    }
    if (a->hull <= 0.f) {
        f32 rest = a->amount;
        a->active = 0;
        g->save.stat_asteroids_mined++;
        world_release_ore(g, a, rest * 1.15f);
        fx_explosion(g, a->pos, a->radius * 0.9f, k_resources[a->res].glow, 0);
        fx_debris(g, a->pos, v2(0, 0), 10, 0xFF8A8A8Au);
        snd_play(g, "sfx_explode_small", 0.55f, rng_range(&g->rng, -0.4f, 0.4f), rng_range(&g->rng, 0.8f, 1.05f));
        player_gain_xp(g, 3 + (int)a->amount_total / 3);
        ach_check(g);
        if (g->player.mining_target == idx) g->player.mining_target = -1;
    }
}

void enemy_damage(Game *g, int idx, f32 amount, Vec2 hit_pos) {
    if (idx < 0 || idx >= SM_MAX_ENEMIES) return;
    Enemy *e = &g->enemies[idx];
    if (!e->active) return;
    e->hull -= amount;
    e->hit_flash = 1.f;
    fx_spark(g, hit_pos, v2(0, 0), 0xFFFFD0A0u, 3, 90.f);
    if (e->hull <= 0.f) {
        const EnemyDef *d = &k_enemies[e->type];
        e->active = 0;
        fx_explosion(g, e->pos, e->radius * 2.4f, d->color, e->type >= ENEMY_HEAVY);
        snd_play(g, e->type >= ENEMY_HEAVY ? "sfx_explode_big" : "sfx_explode_small", 0.7f,
                 rng_range(&g->rng, -0.4f, 0.4f), rng_range(&g->rng, 0.85f, 1.1f));
        s64 cr = (s64)(d->credits * (0.85f + 0.3f * rng_f(&g->rng)));
        g->save.credits += cr;
        g->save.stat_credits_earned += cr;
        g->save.stat_enemies_killed++;
        g->run_kills++;
        char buf[64];
        char num[32];
        sm_format_num(num, sizeof(num), cr);
        sm_sprintf(buf, sizeof(buf), "+%s CR", num);
        fx_text(g, e->pos, buf, SM_RGB(255, 220, 120));
        player_gain_xp(g, d->xp);
        /* drops: a little salvage */
        if (rng_f(&g->rng) < 0.55f) {
            int res = g->sector == 2 ? RES_SALVAGE : (g->sector >= 3 ? RES_PLATINUM : RES_IRON);
            int n = 1 + (int)(rng_f(&g->rng) * 3.f);
            for (int i = 0; i < n; i++) {
                for (int k = 0; k < SM_MAX_PICKUPS; k++) {
                    if (g->pickups[k].active) continue;
                    g->pickups[k].active = 1;
                    g->pickups[k].pos = e->pos;
                    g->pickups[k].vel = v2mul(v2from_angle(rng_range(&g->rng, 0.f, SM_TAU)), rng_range(&g->rng, 40.f, 140.f));
                    g->pickups[k].res = res;
                    g->pickups[k].amount = 1.f + rng_f(&g->rng) * 2.f;
                    g->pickups[k].life = 22.f;
                    g->pickups[k].angle = rng_range(&g->rng, 0.f, SM_TAU);
                    g->pickups[k].spin = rng_range(&g->rng, -3.f, 3.f);
                    g->pickups[k].scale = 0.8f;
                    break;
                }
            }
        }
        if (e->type == ENEMY_SENTINEL) { ach_unlock(g, ACH_SURVIVOR); g->save.stat_sentinels_killed++; }
        ach_unlock(g, ACH_FIRST_BLOOD);
        ach_check(g);
    }
}

/* --------------------------------------------------------------- enemies --- */
static void enemy_fire(Game *g, Enemy *e, const EnemyDef *d, Vec2 target) {
    Vec2 dir = v2norm(v2sub(target, e->pos));
    f32 spread = (d->behavior == 4) ? 0.18f : 0.06f;
    for (int i = 0; i < d->burst; i++) {
        f32 a = v2angle(dir) + ((f32)i - (f32)(d->burst - 1) * 0.5f) * spread;
        Vec2 v = v2mul(v2from_angle(a), d->projectile_kind == 2 ? 620.f : 560.f);
        v = v2add(v, e->vel);
        int idx = -1;
        for (int k = 0; k < SM_MAX_PROJECTILES; k++) {
            if (!g->projectiles[k].active) { idx = k; break; }
        }
        if (idx < 0) break;
        Projectile *p = &g->projectiles[idx];
        p->active = 1;
        p->pos = v2add(e->pos, v2mul(dir, e->radius * 0.8f));
        p->vel = v;
        p->damage = d->damage;
        p->kind = d->projectile_kind == 0 ? PROJ_ENEMY_BOLT : (d->projectile_kind == 1 ? PROJ_HEAVY : PROJ_MISSILE);
        p->from_player = 0;
        p->max_life = 3.2f;
        p->life = p->max_life;
        p->radius = p->kind == PROJ_MISSILE ? 9.f : 7.f;
        p->homing = p->kind == PROJ_MISSILE ? 1.6f : 0.f;
    }
    fx_spark(g, v2add(e->pos, v2mul(dir, e->radius)), v2mul(dir, 60.f), d->color, 3, 100.f);
    snd_play(g, d->projectile_kind == 2 ? "sfx_lance" : "sfx_laser", 0.22f,
             SM_CLAMP((e->pos.x - g->player.pos.x) / 800.f, -1.f, 1.f), 0.72f);
}

static void update_enemies(Game *g, f32 dt) {
    Player *pl = &g->player;
    for (int i = 0; i < SM_MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->active) continue;
        const EnemyDef *d = &k_enemies[e->type];
        e->spawn = SM_MAX(0.f, e->spawn - dt * 1.2f);
        e->hit_flash = SM_MAX(0.f, e->hit_flash - dt * 2.6f);
        f32 dist = v2dist(e->pos, pl->pos);
        Vec2 to = v2sub(pl->pos, e->pos);
        Vec2 dir = v2norm(to);
        f32 ang = v2angle(dir);
        f32 speed = d->speed * (0.9f + 0.2f * rng_f(&g->rng));
        Vec2 desired = v2(0, 0);
        f32 preferred = d->range * 0.62f;
        int aggro = dist < 1500.f && !pl->dead;

        switch (d->behavior) {
            case 0: { /* orbit */
                f32 radial = (dist - preferred) / 320.f;
                Vec2 tangent = v2mul(v2(-dir.y, dir.x), 0.9f);
                desired = v2add(v2mul(dir, SM_CLAMP(radial, -1.f, 1.f)), tangent);
            } break;
            case 1: { /* strafe */
                f32 radial = (dist - preferred * 0.8f) / 260.f;
                Vec2 tangent = v2mul(e->strafe_dir, 1.f);
                if (v2dot(tangent, to) > 0.f) tangent = v2(-tangent.x, -tangent.y);
                e->ai_timer -= dt;
                if (e->ai_timer <= 0.f) {
                    e->ai_timer = rng_range(&g->rng, 1.4f, 2.8f);
                    if (rng_f(&g->rng) < 0.5f) e->strafe_dir = v2(-e->strafe_dir.y, e->strafe_dir.x);
                }
                desired = v2add(v2mul(dir, SM_CLAMP(radial, -1.f, 1.f)), v2mul(e->strafe_dir, 0.95f));
            } break;
            case 2: { /* heavy: hold the line */
                desired = v2mul(dir, dist > preferred ? 1.f : -0.35f);
            } break;
            case 3: { /* hunter: ram the player */
                desired = v2mul(dir, 1.f);
                if (dist < 120.f) desired = v2mul(dir, -0.6f);
            } break;
            case 4: { /* sentinel: keep distance, sweep */
                f32 radial = (dist - preferred * 1.25f) / 400.f;
                desired = v2add(v2mul(dir, SM_CLAMP(radial, -1.f, 1.f)),
                                v2mul(v2(-dir.y, dir.x), 0.5f * sinf(g->time * 0.6f)));
            } break;
            default: break;
        }
        if (!aggro) {
            /* idle patrol */
            desired = v2from_angle(g->time * 0.25f + (f32)(i * 1.7f));
            speed *= 0.35f;
        }
        desired = v2norm(desired);
        Vec2 target_vel = v2mul(desired, speed);
        e->vel = v2lerp(e->vel, target_vel, SM_MIN(1.f, dt * 1.8f));
        e->pos = v2add(e->pos, v2mul(e->vel, dt));
        f32 target_angle = aggro ? ang : v2angle(e->vel);
        f32 da = sm_angle_diff(target_angle, e->angle);
        e->angle += SM_CLAMP(da, -d->turn * dt, d->turn * dt);

        /* keep out of asteroids (simple avoidance) */
        for (int k = 0; k < SM_MAX_ASTEROIDS; k++) {
            Asteroid *a = &g->asteroids[k];
            if (!a->active) continue;
            f32 dd = v2dist(a->pos, e->pos);
            if (dd < a->radius + e->radius) {
                Vec2 push = v2norm(v2sub(e->pos, a->pos));
                e->pos = v2add(e->pos, v2mul(push, (a->radius + e->radius - dd) * 0.5f));
                e->vel = v2add(e->vel, v2mul(push, 60.f));
            }
        }

        /* fire */
        e->fire_timer -= dt;
        if (aggro && e->fire_timer <= 0.f && e->spawn <= 0.f && dist < d->range * 1.15f) {
            e->fire_timer = d->fire_interval * rng_range(&g->rng, 0.85f, 1.25f);
            enemy_fire(g, e, d, pl->pos);
            if (e->type >= ENEMY_HEAVY) g->cam.shake = SM_MAX(g->cam.shake, 2.f);
        }
        if (e->type == ENEMY_SENTINEL && aggro && dist < d->range) {
            e->tether += dt;
            if (e->tether > 3.4f) {
                e->tether = 0.f;
                fx_beam(g, e->pos, pl->pos, 3.f, d->color, 0.35f, BEAM_ENEMY);
                player_damage(g, 12.f, e->pos);
            }
            fx_beam(g, e->pos, pl->pos, 1.2f, SM_RGBA(SM_GET_R(d->color), SM_GET_G(d->color), SM_GET_B(d->color), 120), 0.06f, BEAM_ENEMY);
        }
        if (dist < e->radius + 26.f && !pl->dead) {
            player_damage(g, d->damage * 0.5f, e->pos);
            Vec2 push = v2norm(v2sub(pl->pos, e->pos));
            pl->vel = v2add(pl->vel, v2mul(push, 180.f));
            e->vel = v2add(e->vel, v2mul(push, -120.f));
        }
        /* despawn if very far from the player */
        if (dist > 3200.f) e->active = 0;
    }
}

/* ----------------------------------------------------------- projectiles --- */
static void projectile_hit_effect(Game *g, Projectile *p) {
    u32 col = p->from_player ? SM_RGB(140, 230, 255) : SM_RGB(255, 140, 110);
    if (p->kind == PROJ_LANCE) col = SM_RGB(255, 160, 255);
    if (p->kind == PROJ_MISSILE) col = SM_RGB(255, 160, 90);
    fx_impact(g, p->pos, v2norm(v2mul(p->vel, -1.f)), col);
}

static void update_projectiles(Game *g, f32 dt) {
    Player *pl = &g->player;
    for (int i = 0; i < SM_MAX_PROJECTILES; i++) {
        Projectile *p = &g->projectiles[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.f) { p->active = 0; continue; }
        if (p->homing > 0.f) {
            Vec2 target = p->from_player ? v2(0, 0) : pl->pos;
            if (!p->from_player) {
                int best = -1; f32 bd = 1e9f;
                for (int k = 0; k < SM_MAX_ENEMIES; k++) {
                    if (!g->enemies[k].active) continue;
                    f32 d = v2dist(g->enemies[k].pos, p->pos);
                    if (d < bd) { bd = d; best = k; }
                }
                if (best >= 0 && bd < 900.f) target = g->enemies[best].pos;
            }
            Vec2 want = v2norm(v2sub(target, p->pos));
            Vec2 cur = v2norm(p->vel);
            f32 spd = v2len(p->vel);
            f32 a = v2angle(cur), b = v2angle(want);
            f32 da = sm_angle_diff(b, a);
            f32 na = a + SM_CLAMP(da, -p->homing * dt, p->homing * dt);
            p->vel = v2mul(v2from_angle(na), spd);
        }
        p->pos = v2add(p->pos, v2mul(p->vel, dt));

        /* trail */
        p->trail_timer -= dt;
        if (p->trail_timer <= 0.f) {
            p->trail_timer = 0.02f;
            u32 col = p->from_player ? SM_RGB(120, 210, 255) : SM_RGB(255, 120, 90);
            if (p->kind == PROJ_HEAVY) col = SM_RGB(255, 160, 80);
            if (p->kind == PROJ_LANCE) col = SM_RGB(220, 140, 255);
            for (int k = 0; k < SM_MAX_PARTICLES; k++) {
                if (g->particles[k].active) continue;
                Particle *pp = &g->particles[k];
                pp->active = 1;
                pp->kind = PT_GLOW;
                pp->pos = p->pos;
                pp->vel = v2mul(p->vel, -0.05f);
                pp->life = pp->max_life = 0.22f;
                pp->size = p->radius * 1.1f;
                pp->color = col;
                pp->blend = BLEND_ADD;
                pp->drag = 5.f;
                break;
            }
        }

        if (p->from_player) {
            /* enemies */
            int hit = 0;
            for (int k = 0; k < SM_MAX_ENEMIES; k++) {
                Enemy *e = &g->enemies[k];
                if (!e->active) continue;
                if (v2dist2(p->pos, e->pos) < (e->radius + p->radius) * (e->radius + p->radius)) {
                    enemy_damage(g, k, p->damage, p->pos);
                    projectile_hit_effect(g, p);
                    hit = 1;
                    break;
                }
            }
            if (hit) { p->active = 0; continue; }
            /* rocks stop stray shots */
            for (int k = 0; k < SM_MAX_ASTEROIDS; k++) {
                Asteroid *a = &g->asteroids[k];
                if (!a->active) continue;
                if (v2dist2(p->pos, a->pos) < (a->radius * 0.85f) * (a->radius * 0.85f)) {
                    asteroid_damage(g, k, p->damage * 0.6f, p->pos, dt);
                    projectile_hit_effect(g, p);
                    hit = 1;
                    break;
                }
            }
            if (hit) { p->active = 0; continue; }
        } else {
            if (!pl->dead && v2dist2(p->pos, pl->pos) < (30.f + p->radius) * (30.f + p->radius)) {
                player_damage(g, p->damage, p->pos);
                projectile_hit_effect(g, p);
                p->active = 0;
                continue;
            }
            for (int k = 0; k < SM_MAX_ASTEROIDS; k++) {
                Asteroid *a = &g->asteroids[k];
                if (!a->active) continue;
                if (v2dist2(p->pos, a->pos) < (a->radius * 0.8f) * (a->radius * 0.8f)) {
                    projectile_hit_effect(g, p);
                    p->active = 0;
                    break;
                }
            }
        }
    }
}

void combat_update(Game *g, f32 dt) {
    update_enemies(g, dt);
    update_projectiles(g, dt);
}

/* ----------------------------------------------------------------- render -- */
static const Sheet *proj_sheet(Game *g, int kind) {
    switch (kind) {
        case PROJ_BOLT: return &g->assets.proj[0];
        case PROJ_PLASMA: case PROJ_LANCE: return kind == PROJ_LANCE ? &g->assets.proj[2] : &g->assets.proj[1];
        case PROJ_ENEMY_BOLT: return &g->assets.proj[3];
        case PROJ_HEAVY: return &g->assets.proj[4];
        case PROJ_MISSILE: return &g->assets.proj[5];
        default: return &g->assets.proj[0];
    }
}

void combat_render(Game *g) {
    Gfx *gfx = g->gfx;
    f32 zoom = gfx->zoom;

    /* enemies */
    for (int i = 0; i < SM_MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->active) continue;
        const EnemyDef *d = &k_enemies[e->type];
        Sheet *sh = &g->assets.enemy[e->type];
        if (!sh->px) continue;
        Vec2 sp = gfx_w2s(gfx, e->pos);
        f32 size = (f32)sh->w * d->scale * 1.5f * zoom;
        if (sp.x < -size || sp.y < -size || sp.x > (f32)gfx->w + size || sp.y > (f32)gfx->h + size) continue;
        int frame = (int)(sm_wrapf(e->angle + SM_PI * 0.5f, SM_TAU) / SM_TAU * (f32)sh->frames) % sh->frames;
        f32 spawn = 1.f;
        if (e->spawn > 0.f) {
            spawn = 0.2f + 0.8f * (1.f - e->spawn);
            gfx_glow(gfx, sp, size * e->spawn * 1.2f, d->color, 0.9f * e->spawn);
            gfx_ring(gfx, sp, size * (0.4f + e->spawn), 2.f, sm_col_scale(d->color, e->spawn));
        }
        u32 tint = 0xFFFFFFFF;
        if (e->hit_flash > 0.f) tint = sm_col_lerp(0xFFFFFFFF, SM_RGBA(255, 255, 255, 255), e->hit_flash);
        f32 s = size * spawn;
        gfx_blit(gfx, sh, frame, rect(sp.x - s * 0.5f, sp.y - s * 0.5f, s, s), tint, spawn, BLEND_ALPHA);
        /* engine glow behind the hostile */
        Vec2 back = v2add(sp, v2mul(v2from_angle(e->angle + SM_PI), s * 0.28f));
        gfx_glow(gfx, back, s * 0.18f, d->color, 0.55f);
        if (e->hit_flash > 0.05f) gfx_glow(gfx, sp, s * 0.7f, SM_RGB(255, 220, 190), e->hit_flash * 0.5f);
        /* damage ring */
        if (e->hull < e->hull_max) {
            f32 frac = SM_CLAMP(e->hull / e->hull_max, 0.f, 1.f);
            f32 r = s * 0.62f;
            gfx_arc(gfx, sp, r, -SM_PI * 0.5f, -SM_PI * 0.5f + SM_TAU * frac, 2.f,
                    sm_col_lerp(SM_RGB(255, 90, 70), SM_RGB(255, 210, 90), frac));
        }
    }

    /* projectiles */
    for (int i = 0; i < SM_MAX_PROJECTILES; i++) {
        Projectile *p = &g->projectiles[i];
        if (!p->active) continue;
        Vec2 sp = gfx_w2s(gfx, p->pos);
        if (sp.x < -80 || sp.y < -80 || sp.x > (f32)gfx->w + 80 || sp.y > (f32)gfx->h + 80) continue;
        const Sheet *sh = proj_sheet(g, p->kind);
        f32 ang = v2angle(p->vel);
        f32 len = 44.f * zoom, wid = 20.f * zoom;
        u32 glow_col = SM_RGB(140, 220, 255);
        if (p->kind == PROJ_LANCE) { len = 92.f * zoom; wid = 26.f * zoom; glow_col = SM_RGB(220, 150, 255); }
        else if (p->kind == PROJ_HEAVY) { len = 40.f * zoom; wid = 26.f * zoom; glow_col = SM_RGB(255, 150, 70); }
        else if (p->kind == PROJ_MISSILE) { len = 40.f * zoom; wid = 16.f * zoom; glow_col = SM_RGB(255, 140, 90); }
        else if (!p->from_player) { glow_col = SM_RGB(255, 130, 110); }
        if (sh->px) {
            f32 scale = len / (f32)sh->w;
            gfx_blit_rot(gfx, sh, 0, sp, (f32)sh->w * scale * 1.1f, (f32)sh->h * scale * 1.6f, ang,
                         0xFFFFFFFFu, 0.95f, BLEND_ADD);
        }
        gfx_glow(gfx, sp, wid * 1.4f, glow_col, 0.55f);
        gfx_glow(gfx, sp, wid * 0.6f, 0xFFFFFFFFu, 0.5f);
    }
}

/* drones use this too */
void combat_render_beam_at(Game *g, Vec2 a, Vec2 b, u32 color) { fx_beam(g, a, b, 2.0f, color, 0.06f, BEAM_WEAPON); }
