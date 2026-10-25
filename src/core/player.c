/* ============================================================================
   SPACE MINER — core/player.c
   Ship handling, mining beam, cargo, progression.
   ========================================================================== */
#include "state.h"

/* ------------------------------------------------------------- statistics -- */
f32 player_stat_speed(const Game *g) {
    const ShipDef *s = &k_ships[g->save.ship];
    return s->max_speed * (1.f + 0.09f * (f32)g->save.upgrades[UPG_ENGINE]);
}
f32 player_stat_accel(const Game *g) {
    const ShipDef *s = &k_ships[g->save.ship];
    return s->accel * (1.f + 0.14f * (f32)g->save.upgrades[UPG_ACCEL]);
}
f32 player_stat_turn(const Game *g) {
    return k_ships[g->save.ship].turn_rate;
}
f32 player_stat_laser_dps(const Game *g) {
    const ShipDef *s = &k_ships[g->save.ship];
    return s->laser_dps * (1.f + 0.22f * (f32)g->save.upgrades[UPG_LASER_POWER])
                         * (1.f + 0.18f * (f32)g->save.upgrades[UPG_MINING_SPEED]);
}
f32 player_stat_weapon_damage(const Game *g) {
    const ShipDef *s = &k_ships[g->save.ship];
    const WeaponDef *w = &k_weapons[g->save.weapon];
    f32 base = s->weapon_damage + (w->damage - k_weapons[WEAPON_PULSE].damage) * 0.55f;
    return base * (1.f + 0.20f * (f32)g->save.upgrades[UPG_WEAPON_DAMAGE]);
}
f32 player_stat_weapon_rate(const Game *g) {
    const ShipDef *s = &k_ships[g->save.ship];
    const WeaponDef *w = &k_weapons[g->save.weapon];
    return (s->weapon_rate + (w->rate - k_weapons[WEAPON_PULSE].rate) * 0.35f);
}
f32 player_stat_pickup_radius(const Game *g) {
    return 110.f * (1.f + 0.35f * (f32)g->save.upgrades[UPG_TRACTOR]);
}
int player_stat_cargo_max(const Game *g) {
    return k_ships[g->save.ship].cargo + 45 * g->save.upgrades[UPG_CARGO];
}
int player_stat_drone_slots(const Game *g) {
    return k_ships[g->save.ship].drone_slots + g->save.upgrades[UPG_DRONE_TECH];
}

/* ------------------------------------------------------------------ setup -- */
void player_apply_loadout(Game *g) {
    const ShipDef *s = &k_ships[g->save.ship];
    f32 hull_max = (f32)(s->hull + 30 * g->save.upgrades[UPG_HULL]);
    f32 shield_max = (f32)(s->shield + 26 * g->save.upgrades[UPG_SHIELD]);
    f32 energy_max = (f32)(s->energy + 18 * g->save.upgrades[UPG_ENERGY_REGEN]);
    Player *p = &g->player;
    p->hull_max = hull_max;
    p->shield_max = shield_max;
    p->energy_max = energy_max;
    if (p->hull > hull_max) p->hull = hull_max;
    if (p->shield > shield_max) p->shield = shield_max;
    if (p->energy > energy_max) p->energy = energy_max;
    p->weapon_cd_max = 1.f / SM_MAX(0.5f, player_stat_weapon_rate(g));
}

void player_reset(Game *g, int keep_progress) {
    Player *p = &g->player;
    memset(p, 0, sizeof(*p));
    const ShipDef *s = &k_ships[g->save.ship];
    p->hull_max = (f32)(s->hull + 30 * g->save.upgrades[UPG_HULL]);
    p->shield_max = (f32)(s->shield + 26 * g->save.upgrades[UPG_SHIELD]);
    p->energy_max = (f32)(s->energy + 18 * g->save.upgrades[UPG_ENERGY_REGEN]);
    p->hull = p->hull_max;
    p->shield = p->shield_max;
    p->energy = p->energy_max;
    p->angle = -SM_PI * 0.5f;
    p->weapon_cd_max = 1.f / SM_MAX(0.5f, player_stat_weapon_rate(g));
    p->mining_target = -1;
    p->invuln = 2.f;
    if (!keep_progress) {
        memset(p->cargo, 0, sizeof(p->cargo));
        p->cargo_used = 0.f;
    } else {
        p->cargo_used = (f32)g->save.cargo_used;
        for (int i = 0; i < RES_COUNT; i++) p->cargo[i] = g->save.cargo[i];
    }
    g->cam.hull_flash = 0.f;
    g->cam.shield_ripple = 0.f;
}

/* ------------------------------------------------------------------ cargo -- */
int player_cargo_space(Game *g) {
    int cap = player_stat_cargo_max(g);
    int used = 0;
    for (int i = 0; i < RES_COUNT; i++) used += g->player.cargo[i];
    g->player.cargo_used = (f32)used;
    return SM_MAX(0, cap - used);
}

void player_add_cargo(Game *g, int res, f32 amount) {
    if (res < 0 || res >= RES_COUNT) return;
    int units = (int)ceilf(amount);
    if (units < 1) units = 1;
    int space = player_cargo_space(g);
    if (units > space) units = space;
    if (units <= 0) return;
    g->player.cargo[res] += units;
    g->player.cargo_used += (f32)units;
    g->save.cargo[res] = g->player.cargo[res];
    g->save.cargo_used = (int)g->player.cargo_used;
    player_gain_xp(g, units / 4 + 1);
}

/* ------------------------------------------------------------- progression -- */
void player_gain_xp(Game *g, int xp) {
    if (xp <= 0) return;
    g->save.xp += xp;
    player_check_level(g);
}

void player_check_level(Game *g) {
    while (g->save.xp >= level_xp_required(g->save.level) && g->save.level < 40) {
        g->save.xp -= level_xp_required(g->save.level);
        g->save.level++;
        char buf[96];
        sm_sprintf(buf, sizeof(buf), "MINING LEVEL %d", g->save.level);
        game_toast(g, buf, SM_RGB(140, 240, 255));
        game_toast2(g, "New upgrades available at the station", SM_RGB(180, 200, 220));
        snd_play(g, "sfx_levelup", 0.8f, 0.f, 1.f);
        fx_ring(g, g->player.pos, 160.f, SM_RGB(140, 240, 255), 0.8f);
        ach_check(g);
    }
    if (g->save.level >= 20) ach_unlock(g, ACH_MINER_MASTER);
}

/* ----------------------------------------------------------------- damage -- */
void player_damage(Game *g, f32 amount, Vec2 from) {
    Player *p = &g->player;
    if (p->dead || p->invuln > 0.f) return;
    f32 remaining = amount;
    if (p->shield > 0.f) {
        f32 absorbed = SM_MIN(p->shield, remaining);
        p->shield -= absorbed;
        remaining -= absorbed;
        p->shield_flash = 1.f;
        p->shield_timer = 4.5f;
        g->cam.shield_ripple = 1.f;
        g->cam.shield_ripple_pos = v2norm(v2sub(from, p->pos));
        snd_play(g, "sfx_shield", 0.55f, rng_range(&g->rng, -0.4f, 0.4f), rng_range(&g->rng, 0.95f, 1.1f));
        if (remaining <= 0.f) {
            fx_spark(g, from, v2(0, 0), SM_RGB(120, 200, 255), 6, 90.f);
            return;
        }
    }
    p->hull -= remaining;
    p->hit_flash = 1.f;
    g->cam.hull_flash = 0.85f;
    g->cam.shake = SM_MAX(g->cam.shake, 10.f + remaining * 0.35f);
    snd_play(g, "sfx_hull", 0.6f, rng_range(&g->rng, -0.5f, 0.5f), rng_range(&g->rng, 0.9f, 1.05f));
    fx_impact(g, v2lerp(from, p->pos, 0.85f), v2norm(v2sub(p->pos, from)), SM_RGB(255, 170, 110));
    if (p->hull <= 0.f) {
        p->hull = 0.f;
        p->dead = 1;
        fx_explosion(g, p->pos, 90.f, SM_RGB(120, 200, 255), 1);
        g->cam.shake = 26.f;
    }
}

/* ------------------------------------------------------------------ input -- */
static void mining_beam(Game *g, f32 aim, f32 dt) {
    Player *p = &g->player;
    f32 range = 430.f + 40.f * (f32)g->save.upgrades[UPG_SCANNER];
    int target = -1;
    f32 best = 1e30f;
    Vec2 nose = v2add(p->pos, v2mul(v2from_angle(aim), 30.f));
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (!a->active) continue;
        Vec2 to = v2sub(a->pos, p->pos);
        f32 dist = v2len(to);
        if (dist > range + a->radius) continue;
        f32 ang = v2angle(to);
        f32 diff = fabsf(sm_angle_diff(ang, aim));
        f32 ang_radius = atan2f(a->radius * 0.95f, SM_MAX(1.f, dist)) + 0.05f;
        if (diff > ang_radius) continue;
        f32 score = diff * 200.f + dist * 0.05f;
        if (score < best) { best = score; target = i; }
    }
    p->mining_target = target;
    if (target < 0) {
        p->mining = 0;
        return;
    }
    Asteroid *a = &g->asteroids[target];
    p->mining = 1;
    f32 dist = v2dist(p->pos, a->pos);
    Vec2 hit = v2add(p->pos, v2mul(v2from_angle(v2angle(v2sub(a->pos, p->pos))), SM_MAX(0.f, dist - a->radius * 0.55f)));
    p->mining_hit = hit;
    fx_beam(g, nose, hit, 2.4f, k_resources[a->res].glow, 0.07f, BEAM_MINING);
    if (p->energy <= 0.5f) {
        p->mining = 0;
        if (g->toast_timer <= 0.f) game_toast(g, "ENERGY DEPLETED", SM_RGB(255, 150, 90));
        return;
    }
    f32 dps = player_stat_laser_dps(g);
    f32 hardness = k_resources[a->res].hardness;
    asteroid_damage(g, target, dps * dt / hardness, hit, dt);
    p->energy = SM_MAX(0.f, p->energy - 13.f * dt);
    g->cam.mining_glow = 1.f;
    if (hash1(g->frame * 31u + (u32)target) > 0.55f)
        fx_spark(g, hit, v2mul(v2from_angle(v2angle(v2sub(p->pos, a->pos)) + rng_range(&g->rng, -0.9f, 0.9f)),
                               rng_range(&g->rng, 60.f, 200.f)), k_resources[a->res].glow, 1, 120.f);
    if (hash1(g->frame * 17u) > 0.86f) snd_play(g, "sfx_mine", 0.16f, 0.f, rng_range(&g->rng, 0.9f, 1.1f));
}

void player_update(Game *g, Input *in, f32 dt) {
    Player *p = &g->player;
    if (p->dead) return;

    /* ---- aim ---- */
    Vec2 mouse_world = gfx_s2w(g->gfx, in->mouse);
    Vec2 to_mouse = v2sub(mouse_world, p->pos);
    f32 aim = p->angle;
    int mouse_control = (v2len(to_mouse) > 30.f);
    if (mouse_control) aim = v2angle(to_mouse);
    else {
        /* keyboard-only fallback: face travel/enemy direction */
        int nearest = -1;
        f32 bd = 700.f;
        for (int i = 0; i < SM_MAX_ENEMIES; i++) {
            if (!g->enemies[i].active) continue;
            f32 d = v2dist(g->enemies[i].pos, p->pos);
            if (d < bd) { bd = d; nearest = i; }
        }
        if (nearest >= 0) aim = v2angle(v2sub(g->enemies[nearest].pos, p->pos));
        else if (v2len(p->vel) > 20.f) aim = v2angle(p->vel);
    }
    if (in->down[SM_KEY_LEFT] || in->down[SM_KEY_RIGHT] || (!mouse_control && (in->mouse_down[0] || in->mouse_down[1]))) {
        if (in->down[SM_KEY_LEFT]) aim = p->angle - 1.2f;
        if (in->down[SM_KEY_RIGHT]) aim = p->angle + 1.2f;
    }
    f32 turn = player_stat_turn(g);
    f32 d = sm_angle_diff(aim, p->angle);
    f32 max_turn = turn * dt;
    if (fabsf(d) > max_turn) d = SM_SIGN(d) * max_turn;
    else d = d * SM_MIN(1.f, dt * 14.f);
    p->angle += d;
    p->roll = sm_approach(p->roll, SM_CLAMP(d / SM_MAX(0.0001f, max_turn), -1.f, 1.f) * 0.35f + p->roll * 0.9f, dt * 6.f);

    /* ---- thrust ---- */
    const ShipDef *sd = &k_ships[g->save.ship];
    f32 accel = player_stat_accel(g);
    f32 maxspeed = player_stat_speed(g);
    Vec2 fwd = v2from_angle(p->angle);
    Vec2 right = v2(-fwd.y, fwd.x);
    Vec2 thrust = v2(0, 0);
    if (in->down[SM_KEY_W] || in->down[SM_KEY_UP]) thrust = v2add(thrust, fwd);
    if (in->down[SM_KEY_S] || in->down[SM_KEY_DOWN]) thrust = v2add(thrust, v2mul(fwd, -0.55f));
    if (in->down[SM_KEY_A]) thrust = v2add(thrust, v2mul(right, -0.7f));
    if (in->down[SM_KEY_D]) thrust = v2add(thrust, v2mul(right, 0.7f));
    f32 tlen = v2len(thrust);
    int boosting = in->down[SM_KEY_SPACE] && p->energy > 4.f;
    f32 boost_mul = boosting ? 1.85f : 1.f;
    if (tlen > 0.01f) {
        Vec2 dir = v2mul(thrust, 1.f / tlen);
        p->vel = v2add(p->vel, v2mul(dir, accel * boost_mul * dt));
        p->thrust = SM_MIN(1.f, p->thrust + dt * 5.f);
        if (boosting) p->energy = SM_MAX(0.f, p->energy - 16.f * dt);
    } else {
        p->thrust = SM_MAX(0.f, p->thrust - dt * 4.f);
    }
    /* drag / speed clamp */
    f32 drag = sm_decay(tlen > 0.01f ? 0.35f : 1.4f, dt);
    p->vel = v2mul(p->vel, drag);
    f32 spd = v2len(p->vel);
    f32 cap = maxspeed * boost_mul;
    if (spd > cap) p->vel = v2mul(p->vel, cap / spd);
    p->pos = v2add(p->pos, v2mul(p->vel, dt));
    g->save.stat_distance += v2len(p->vel) * dt * 0.001f;

    /* keep the ship inside the sector */
    f32 dist = v2len(p->pos);
    if (dist > SM_WORLD_RADIUS) {
        Vec2 inw = v2mul(p->pos, -1.f / SM_MAX(1.f, dist));
        p->pos = v2mul(v2norm(p->pos), SM_WORLD_RADIUS);
        p->vel = v2add(p->vel, v2mul(inw, 220.f * dt));
        if (g->toast_timer <= 0.f) game_toast(g, "SECTOR BOUNDARY", SM_RGB(255, 180, 90));
    }

    /* engine trail */
    p->engine_phase += dt * (6.f + p->thrust * 16.f);
    if (p->thrust > 0.15f && hash1(g->frame * 13u) > 0.35f) {
        Vec2 back = v2sub(p->pos, v2mul(fwd, sd->scale * 46.f));
        Vec2 off = v2mul(right, rng_range(&g->rng, -8.f, 8.f));
        fx_glow(g, v2add(back, off), 9.f, SM_RGB(120, 200, 255), 0.10f);
        Particle *pp = NULL;
        for (int i = 0; i < SM_MAX_PARTICLES; i++) if (!g->particles[i].active) { pp = &g->particles[i]; break; }
        if (pp) {
            pp->active = 1;
            pp->kind = PT_GLOW;
            pp->pos = v2add(back, off);
            pp->vel = v2add(v2mul(p->vel, 0.3f), v2mul(v2from_angle(p->angle + SM_PI + rng_range(&g->rng, -0.3f, 0.3f)),
                                                     rng_range(&g->rng, 60.f, 150.f)));
            pp->life = pp->max_life = rng_range(&g->rng, 0.15f, 0.4f);
            pp->size = rng_range(&g->rng, 3.f, 6.f);
            pp->size_end = 0.f;
            pp->color = SM_RGB(110, 190, 255);
            pp->blend = BLEND_ADD;
            pp->drag = 3.f;
        }
    }

    /* ---- weapons & mining ---- */
    p->weapon_cd -= dt;
    Vec2 nose = v2add(p->pos, v2mul(fwd, sd->scale * 40.f));
    int firing = in->mouse_down[0] || in->down[SM_KEY_CTRL];
    if (firing && p->weapon_cd <= 0.f && p->energy > 2.f) {
        combat_fire_weapon(g, nose, p->angle);
        p->weapon_cd = p->weapon_cd_max;
    }
    int mining_input = in->mouse_down[1] || in->down[SM_KEY_F];
    if (mining_input) mining_beam(g, p->angle, dt);
    else p->mining = 0;

    /* ---- energy & shield regeneration ---- */
    f32 regen = 9.f * (1.f + 0.20f * (f32)g->save.upgrades[UPG_ENERGY_REGEN]);
    if (!mining_input) p->energy = SM_MIN(p->energy_max, p->energy + regen * dt);
    p->shield_timer = SM_MAX(0.f, p->shield_timer - dt);
    if (p->shield_timer <= 0.f && p->shield < p->shield_max) {
        p->shield = SM_MIN(p->shield_max, p->shield + (6.f + 2.f * g->save.upgrades[UPG_SHIELD]) * dt);
    }
    p->shield_flash = SM_MAX(0.f, p->shield_flash - dt * 3.f);
    p->hit_flash = SM_MAX(0.f, p->hit_flash - dt * 2.6f);
    p->invuln = SM_MAX(0.f, p->invuln - dt);
}

/* ------------------------------------------------------------------ render -- */
void player_render(Game *g) {
    Gfx *gfx = g->gfx;
    Player *p = &g->player;
    const ShipDef *sd = &k_ships[g->save.ship];
    Sheet *sh = &g->assets.ship[g->save.ship];
    if (!sh->px) return;
    f32 zoom = gfx->zoom;
    Vec2 sp = gfx_w2s(gfx, p->pos);
    f32 size = (f32)sh->w * sd->scale * 1.35f * zoom;
    int frame = (int)(sm_wrapf(p->angle + SM_PI * 0.5f, SM_TAU) / SM_TAU * (f32)sh->frames) % sh->frames;
    Vec2 fwd = v2from_angle(p->angle);
    Vec2 right = v2(-fwd.y, fwd.x);

    /* engine flames first (under the hull) */
    Sheet *flame = &g->assets.fx_flame[0];
    if (flame->px && p->thrust > 0.05f) {
        for (int side = -1; side <= 1; side += 2) {
            Vec2 ep = v2add(p->pos, v2add(v2mul(fwd, -sd->scale * 36.f), v2mul(right, (f32)side * sd->scale * 17.f)));
            Vec2 eps = gfx_w2s(gfx, ep);
            f32 fl = (0.72f + 0.36f * sinf(p->engine_phase * 0.6f + (f32)side * 1.7f)) * p->thrust;
            f32 fw = 26.f * sd->scale * zoom * (0.85f + 0.3f * fl);
            f32 fh = 128.f * sd->scale * zoom * (0.55f + 0.55f * fl);
            int fframe = ((int)(p->engine_phase * 1.6f) + side + 10) % flame->frames;
            /* the flame art points down, so it is rotated to trail the ship */
            Vec2 centre = v2add(eps, v2mul(fwd, -fh * 0.30f));
            gfx_blit_rot(gfx, flame, fframe, centre, fw, fh, p->angle - SM_PI * 0.5f,
                         SM_RGBA(255, 255, 255, 255), 0.85f * p->thrust, BLEND_ADD);
            gfx_glow(gfx, eps, fw * 0.9f, SM_RGB(140, 210, 255), 0.7f * p->thrust);
        }
    }

    /* hull */
    u32 tint = 0xFFFFFFFF;
    if (p->hit_flash > 0.f) tint = sm_col_lerp(0xFFFFFFFF, SM_RGBA(255, 130, 110, 255), p->hit_flash * 0.85f);
    if (p->invuln > 0.f && fmodf(g->time, 0.16f) < 0.08f) tint = SM_RGBA(200, 235, 255, 255);
    gfx_blit(gfx, sh, frame, rect(sp.x - size * 0.5f, sp.y - size * 0.5f, size, size), tint, 1.f, BLEND_ALPHA);

    /* mining emitters glow while the beam is live */
    if (p->mining) {
        for (int side = -1; side <= 1; side += 2) {
            Vec2 ep = v2add(p->pos, v2add(v2mul(fwd, sd->scale * 30.f), v2mul(right, (f32)side * sd->scale * 15.f)));
            gfx_glow(gfx, gfx_w2s(gfx, ep), 12.f * zoom, SM_RGB(120, 230, 255), 0.9f);
        }
    }
    /* running lights */
    gfx_glow(gfx, v2add(sp, v2mul(right, size * 0.31f)), 4.5f * zoom, SM_RGB(255, 200, 90), 0.8f);
    gfx_glow(gfx, v2add(sp, v2mul(right, -size * 0.31f)), 4.5f * zoom, SM_RGB(255, 90, 80), 0.8f);

    /* shield bubble */
    if (p->shield_flash > 0.02f || p->shield > 0.01f) {
        f32 a = p->shield_flash * 0.75f;
        if (a > 0.02f) {
            gfx_ring(gfx, sp, size * 0.62f, 2.2f, SM_RGBA(130, 200, 255, (u32)(a * 255.f)));
            gfx_ring(gfx, sp, size * 0.58f, 1.f, SM_RGBA(200, 240, 255, (u32)(a * 160.f)));
        }
        if (p->shield_timer > 0.f && p->shield <= 0.f) {
            gfx_arc(gfx, sp, size * 0.6f, g->time * 3.f, g->time * 3.f + 2.f, 2.f, SM_RGBA(255, 120, 90, 120));
        }
    }
}
