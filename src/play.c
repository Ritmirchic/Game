// Player movement, weapons, combat, barricades, interaction and the night loop.
#include "game.h"
#include <string.h>

#define PLAYER_EYE 1.62f
#define PLAYER_RADIUS 0.34f
#define WALK_SPEED 4.35f
#define SPRINT_SPEED 6.55f
#define AIM_SPEED 2.45f

int world_point_blocked(Game *g, Vec3 p, float radius);

void sfx(Game *g, Sound *s, Vec3 *pos, float vol, float pitch, int bus) {
    if (!s) return;
    aud_play(&g->w->audio, s, pos, vol, pitch, 0, bus);
}
void sfx_loop(Game *g, Sound *s, float vol) {
    if (!s) return;
    int id = aud_play(&g->w->audio, s, NULL, vol, 1.f, 1, 2);
    (void)id;
}

void player_spawn(Game *g, Player *p, int index) {
    World *w = g->w;
    p->index = index;
    p->active = 1;
    p->pos = vadd(w->level.player_spawn, v3((float)index * 1.1f - 0.5f, 0, 0));
    p->vel = v3(0, 0, 0);
    p->yaw = w->level.player_spawn_yaw;
    p->pitch = 0.f;
    p->height = 1.8f;
    p->radius = PLAYER_RADIUS;
    p->max_hp = 100.f;
    p->hp = 100.f;
    p->max_stamina = 100.f;
    p->stamina = 100.f;
    p->armor = 0.f;
    p->weapon = WP_PISTOL;
    memset(p->owned, 0, sizeof p->owned);
    p->owned[WP_FISTS] = 1;
    p->owned[WP_BAT] = 1;
    p->owned[WP_PISTOL] = 1;
    memset(p->mag, 0, sizeof p->mag);
    p->mag[WP_PISTOL] = 12;
    p->mag[WP_BAT] = 1;
    p->mag[WP_AXE] = 1;
    p->mag[WP_FISTS] = 1;
    p->ammo[AMMO_PISTOL] = 48;
    p->ammo[AMMO_SHELL] = 0;
    p->ammo[AMMO_RIFLE] = 0;
    p->ammo[AMMO_BOLT] = 0;
    p->alive = 1;
    p->downed = 0;
    p->flashlight_on = 1.f;
    p->flashlight_charge = 100.f;
    p->money = 0;
    p->scrap = 0;
    p->heal_charges = 1;
    p->fire_cd = 0.f;
    p->reload_t = 0.f;
    p->switch_t = 0.f;
    p->net_id = (NetId)(index + 1);
}

static void play_footstep(Game *g, Player *p) {
    float speed = vlen(v3(p->vel.x, 0, p->vel.z));
    if (speed < 0.6f) return;
    p->footstep_t -= g->plat->dt * (speed > 5.f ? 1.65f : 1.f);
    if (p->footstep_t <= 0.f) {
        p->footstep_t = 0.52f;
        int i = (int)(rng_f(&g->w->rng) * 3.99f);
        sfx(g, g->as->snd_step_player[i], &p->pos, 0.32f, 0.92f + rng_f(&g->w->rng) * 0.16f, 1);
    }
}

// ---------------------------------------------------------------------------
static void player_move(Game *g, Player *p, float dt, Vec3 wish, int sprint) {
    World *w = g->w;
    const WeaponDef *wd = &g_weapon_defs[p->weapon];
    float max_speed = WALK_SPEED * wd->move_speed_mult;
    if (p->aim_t > 0.5f) max_speed = AIM_SPEED;
    else if (sprint && p->stamina > 1.f) max_speed = SPRINT_SPEED * maxf(0.85f, wd->move_speed_mult);
    if (p->reloading) max_speed *= 0.88f;
    Vec3 target = vscale(wish, max_speed);
    float accel = (vlen(wish) > 0.01f) ? 42.f : 22.f;
    Vec3 diff = vsub(target, v3(p->vel.x, 0, p->vel.z));
    float dl = vlen(diff);
    if (dl > 0.0001f) {
        float step = minf(dl, accel * dt);
        p->vel = vadd(p->vel, vscale(vnorm(diff), step));
    }
    // gravity / jump
    p->vel.y -= 18.f * dt;
    Vec3 delta = vscale(p->vel, dt);
    Vec3 before = p->pos;
    Vec3 np = p->pos;
    np.x += delta.x;
    np.z += delta.z;
    if (!world_point_blocked(g, np, p->radius)) {
        p->pos = np;
    } else {
        // try axis separated
        Vec3 ax = p->pos;
        ax.x += delta.x;
        if (!world_point_blocked(g, ax, p->radius)) p->pos = ax;
        Vec3 az = p->pos;
        az.z += delta.z;
        if (!world_point_blocked(g, az, p->radius)) p->pos = az;
    }
    p->pos.y += delta.y;
    if (p->pos.y <= 0.f) {
        if (p->vel.y < -6.f) {
            p->land_kick = minf(1.f, p->vel.y / -14.f);
            sfx(g, g->as->snd_step_player[0], &p->pos, 0.4f, 0.7f, 1);
        }
        p->pos.y = 0.f;
        p->vel.y = 0.f;
    }
    // keep the player inside the world
    p->pos.x = clampf(p->pos.x, -45.f, 45.f);
    p->pos.z = clampf(p->pos.z, -28.5f, 56.f);

    float speed = vlen(vsub(p->pos, before)) / maxf(dt, 0.0001f);
    if (speed > 0.5f) play_footstep(g, p);

    // stamina
    if (sprint && speed > WALK_SPEED * 1.05f) {
        p->stamina = maxf(0.f, p->stamina - dt * 17.f);
        if (p->stamina <= 0.f) p->exhaustion = 1.2f;
    } else {
        p->stamina = minf(p->max_stamina, p->stamina + dt * (p->exhaustion > 0.f ? 11.f : 22.f));
    }
    p->exhaustion = maxf(0.f, p->exhaustion - dt);
}

static Vec3 camera_dir(const Player *p) {
    float cp = cosf(p->pitch);
    return v3(sinf(p->yaw) * cp, sinf(p->pitch), cosf(p->yaw) * cp);
}

int combat_melee(Game *g, Player *p) {
    World *w = g->w;
    const WeaponDef *wd = &g_weapon_defs[p->weapon];
    Vec3 dir = camera_dir(p);
    Vec3 origin = vadd(p->pos, v3(0, PLAYER_EYE - 0.15f, 0));
    float reach = wd->range;
    int hit_any = 0;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &w->zombies[i];
        if (!z->alive || z->state == ZS_DEAD) continue;
        Vec3 c = vadd(z->pos, v3(0, z->height * 0.5f, 0));
        Vec3 to = vsub(c, origin);
        float d = vlen(to);
        if (d > reach + z->radius) continue;
        float dot = vdot(vnorm(to), dir);
        if (dot < 0.45f) continue;
        float ht;
        if (level_raycast(&w->level, origin, c, &ht, NULL) && ht < d - 0.4f) continue;
        int head = c.y > 1.35f && z->height > 1.2f;
        ai_damage_zombie(g, z, wd->damage, vnorm(to), head && p->weapon == WP_AXE, 1);
        fx_blood(g, vmad(origin, dir, minf(d, reach)), dir, p->weapon == WP_AXE ? 12 : 7);
        hit_any = 1;
    }
    sfx(g, hit_any ? g->as->snd_melee_hit[0] : g->as->snd_melee_swing[1], &p->pos, 0.75f, 1.f, 1);
    if (hit_any) {
        p->shake = maxf(p->shake, 0.28f);
        g->hit_flash = 0.5f;
    }
    return hit_any;
}

void combat_trigger(Game *g, Player *p, int held) {
    World *w = g->w;
    const WeaponDef *wd = &g_weapon_defs[p->weapon];
    if (!p->alive) return;
    if (p->fire_cd > 0.f) return;
    if (p->reloading) return;
    if (wd->kind == 0) {
        p->fire_cd = 60.f / wd->rpm;
        p->melee_t = 0.42f;
        p->melee_hit_done = 0;
        return;
    }
    if (p->mag[p->weapon] <= 0) {
        p->fire_cd = 0.18f;
        sfx(g, g->as->snd_empty, &p->pos, 0.5f, 1.f, 1);
        // auto-reload
        if (p->ammo[wd->ammo_type] > 0) p->reloading = 1, p->reload_t = wd->reload_time;
        return;
    }
    p->fire_cd = 60.f / wd->rpm;
    p->mag[p->weapon]--;
    p->recoil_pitch += wd->recoil * 0.05f;
    p->recoil_yaw += rng_range(&w->rng, -1.f, 1.f) * wd->recoil * 0.018f;
    p->shake = maxf(p->shake, 0.12f + wd->recoil * 0.11f);
    g->shot_flash = 1.f;
    w->shots_fired++;
    // muzzle flash light for one frame
    Vec3 mz = g->vm_muzzle;
    g->muzzle_light_t = 0.055f;
    g->muzzle_light_intensity = 2.4f;
    g->muzzle_light_pos = mz;
    fx_muzzle_flash(g, mz);
    // shot sound
    Sound *s = NULL;
    switch (p->weapon) {
        case WP_PISTOL: s = g->as->snd_shot_pistol[(int)(rng_f(&w->rng) * 2.99f)]; break;
        case WP_SHOTGUN: s = g->as->snd_shot_shotgun[(int)(rng_f(&w->rng) * 1.99f)]; break;
        case WP_RIFLE: s = g->as->snd_shot_rifle[(int)(rng_f(&w->rng) * 2.99f)]; break;
        case WP_SNIPER: s = g->as->snd_shot_sniper[(int)(rng_f(&w->rng) * 1.99f)]; break;
        case WP_CROSSBOW: s = g->as->snd_shot_crossbow[(int)(rng_f(&w->rng) * 1.99f)]; break;
        default: break;
    }
    sfx(g, s, &mz, 0.95f, 0.96f + rng_f(&w->rng) * 0.08f, 1);
    // alert nearby zombies
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &w->zombies[i];
        if (!z->alive || z->state == ZS_DEAD) continue;
        if (vdist(z->pos, p->pos) < 34.f) z->state = z->state == ZS_IDLE ? ZS_CHASE : z->state;
    }
    // hitscan pellets
    Vec3 dir0 = camera_dir(p);
    int pellets = (int)wd->pellets;
    if (p->weapon == WP_CROSSBOW) {
        // crossbow fires a real projectile
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (w->projectiles[i].active) continue;
            Projectile *pr = &w->projectiles[i];
            pr->active = 1;
            pr->kind = 0;
            pr->pos = mz;
            pr->vel = vscale(vnorm(vsub(vadd(p->pos, v3(0, PLAYER_EYE, 0)), mz)), 62.f);
            pr->vel = vadd(pr->vel, vscale(dir0, 0.f));
            pr->vel = vscale(dir0, 62.f);
            pr->life = 4.f;
            pr->damage = wd->damage;
            pr->radius = 0.08f;
            pr->owner = p->index;
            break;
        }
        return;
    }
    float aim = p->aim_t;
    for (int k = 0; k < pellets; k++) {
        Vec3 d = dir0;
        float spread = wd->spread * (1.f - aim * 0.72f);
        d = vadd(d, v3(rng_range(&w->rng, -spread, spread), rng_range(&w->rng, -spread, spread),
                       rng_range(&w->rng, -spread, spread)));
        d = vnorm(d);
        Vec3 origin = vadd(p->pos, v3(0, PLAYER_EYE - 0.06f, 0));
        Vec3 end = vmad(origin, d, wd->range);
        float t_wall = wd->range;
        Vec3 wall_n = v3(0, 1, 0);
        if (!level_raycast_bullet(&w->level, origin, end, &t_wall, &wall_n)) t_wall = wd->range;
        // zombies
        float best = t_wall;
        Zombie *hit = NULL;
        for (int i = 0; i < MAX_ZOMBIES; i++) {
            Zombie *z = &w->zombies[i];
            if (!z->alive || z->state == ZS_DEAD) continue;
            // capsule approximation: 2 spheres
            float zt;
            Vec3 hitsp;
            float r = z->radius + 0.12f;
            for (int seg = 0; seg < 2; seg++) {
                Vec3 c = vadd(z->pos, v3(0, 0.55f + (float)seg * (z->height - 1.1f), 0));
                Vec3 oc = vsub(c, origin);
                float b = vdot(oc, d);
                if (b < 0.f) continue;
                float disc = b * b - (vdot(oc, oc) - r * r);
                if (disc < 0.f) continue;
                float t = b - sqrtf(disc);
                if (t < 0.f) t = 0.f;
                if (t > 0.f && t < best) {
                    best = t;
                    hit = z;
                    zt = t;
                    hitsp = vmad(origin, d, t);
                    (void)zt; (void)hitsp;
                }
            }
        }
        if (hit) {
            Vec3 hp = vmad(origin, d, best);
            int headshot = 0;
            if (hit->height > 1.2f) {
                float head_y = hit->pos.y + hit->height * 0.90f;
                headshot = hp.y > head_y - 0.22f;
            }
            Vec3 from_dir = vnorm(vsub(hp, origin));
            ai_damage_zombie(g, hit, wd->damage / (float)pellets * (float)pellets, from_dir, headshot, 1);
            fx_impact(g, hp, vscale(from_dir, -1.f), MAT_FLESH);
            // penetration: damage the zombie behind
            if (wd->penetrate > 0.f) {
                int pen = (int)wd->penetrate;
                for (int i = 0; i < MAX_ZOMBIES && pen > 0; i++) {
                    Zombie *z = &w->zombies[i];
                    if (z == hit || !z->alive || z->state == ZS_DEAD) continue;
                    Vec3 c = vadd(z->pos, v3(0, z->height * 0.55f, 0));
                    Vec3 to = vsub(c, origin);
                    float along = vdot(to, d);
                    if (along <= best || along > wd->range) continue;
                    Vec3 perp = vsub(to, vscale(d, along));
                    if (vlen(perp) > z->radius + 0.18f) continue;
                    ai_damage_zombie(g, z, wd->damage * 0.6f, d, 0, 1);
                    pen--;
                }
            }
        } else if (t_wall < wd->range - 0.01f) {
            Vec3 hp = vmad(origin, d, t_wall);
            fx_impact(g, hp, wall_n, MAT_WALL_PAINT);
            // find the material of whatever we hit
            fx_decal(g, vmad(hp, wall_n, 0.012f), wall_n, g->as->blood, 0.16f, COL(22, 20, 20, 210));
        }
    }
    w->subtitle_t = 0.f;
}

static void player_update_weapon(Game *g, Player *p, float dt) {
    World *w = g->w;
    Input *in = &g->plat->in;
    const WeaponDef *wd = &g_weapon_defs[p->weapon];
    p->fire_cd = maxf(0.f, p->fire_cd - dt);
    p->switch_t = maxf(0.f, p->switch_t - dt);
    p->melee_t = maxf(0.f, p->melee_t - dt);
    p->aim_t = approachf(p->aim_t, (in->mouse_down[1] && !p->reloading) ? 1.f : 0.f, 8.f, dt);
    // melee hit frame
    if (wd->kind == 0 && p->melee_t > 0.f && !p->melee_hit_done && p->melee_t < 0.28f) {
        p->melee_hit_done = 1;
        combat_melee(g, p);
    }
    // reload
    if (p->reloading) {
        p->reload_t -= dt;
        if (!g->w->net.running || g->net_is_host) {
            if (p->reload_t <= 0.f) {
                p->reloading = 0;
                int need = wd->mag_size - p->mag[p->weapon];
                if (need > 0 && wd->ammo_type >= 0) {
                    int take = mini(need, p->ammo[wd->ammo_type]);
                    p->mag[p->weapon] += take;
                    p->ammo[wd->ammo_type] -= take;
                }
            }
        }
    }
    if (in->pressed['R'] && !p->reloading && wd->kind == 1 && p->mag[p->weapon] < wd->mag_size &&
        p->ammo[wd->ammo_type] > 0) {
        p->reloading = 1;
        p->reload_t = wd->reload_time;
        sfx(g, g->as->snd_reload[(int)(rng_f(&w->rng) * 2.99f)], &p->pos, 0.8f, 1.f, 1);
    }
    // weapon switching
    for (int i = 0; i < WP_COUNT && i < 9; i++) {
        if (in->pressed['1' + i] && p->owned[i] && p->weapon != i) {
            p->weapon = i;
            p->switch_t = 0.28f;
            p->reloading = 0;
            sfx(g, g->as->snd_click, &p->pos, 0.5f, 1.2f, 3);
        }
    }
    if (in->wheel != 0.f) {
        int dir = in->wheel > 0.f ? 1 : -1;
        for (int k = 1; k <= WP_COUNT; k++) {
            int idx = (p->weapon + dir * k % WP_COUNT + WP_COUNT * 2) % WP_COUNT;
            if (p->owned[idx]) {
                p->weapon = idx;
                p->switch_t = 0.28f;
                p->reloading = 0;
                sfx(g, g->as->snd_click, &p->pos, 0.5f, 1.2f, 3);
                break;
            }
        }
    }
    // firing
    if (in->mouse_down[0] && wd->kind != 0) {
        if (p->switch_t <= 0.f) combat_trigger(g, p, 1);
    } else if (in->mouse_pressed[0]) {
        if (p->switch_t <= 0.f) combat_trigger(g, p, 0);
    }
    (void)w;
}

void player_update(Game *g, Player *p, float dt) {
    World *w = g->w;
    if (!p->active) return;
    Input *in = &g->plat->in;
    int is_local = (p->index == w->local_player);
    p->last_damage_time += dt;
    p->hit_flash = maxf(0.f, p->hit_flash - dt * 2.5f);
    p->shake = maxf(0.f, p->shake - dt * 2.4f);
    p->land_kick = maxf(0.f, p->land_kick - dt * 3.f);
    p->recoil_pitch = approachf(p->recoil_pitch, 0.f, 6.f, dt);
    p->recoil_yaw = approachf(p->recoil_yaw, 0.f, 6.f, dt);
    p->damage_indicator = maxf(0.f, p->damage_indicator - dt);
    p->kill_flash = maxf(0.f, p->kill_flash - dt * 2.f);
    for (int i = 0; i < 4; i++) g->damage_dir_t[i] = maxf(0.f, g->damage_dir_t[i] - dt * 1.2f);

    if (!p->alive) {
        p->death_time += dt;
        p->aim_t = 0.f;
        // third person camera drop for the local player
        return;
    }
    if (p->downed) {
        p->downed_time -= dt;
        if (p->downed_time <= 0.f) {
            p->downed = 0;
            p->hp = p->max_hp * 0.3f;
        }
        return;
    }

    Vec3 wish = v3(0, 0, 0);
    int sprint = 0;
    if (is_local) {
        if (in->down['W']) wish.z -= 1.f;
        if (in->down['S']) wish.z += 1.f;
        if (in->down['A']) wish.x -= 1.f;
        if (in->down['D']) wish.x += 1.f;
        sprint = in->down[K_SHIFT] != 0;
        if (in->down[K_SPACE] && p->pos.y <= 0.01f) {
            p->vel.y = 5.6f;
        }
        p->yaw -= in->mouse_dx * (float)w->mouse_sensitivity * 0.0022f;
        p->pitch -= in->mouse_dy * (float)w->mouse_sensitivity * 0.0022f * (w->invert_y ? -1.f : 1.f);
        p->pitch = clampf(p->pitch, -1.45f, 1.45f);
        // view sway from mouse motion
        p->sway_x = approachf(p->sway_x, clampf(-in->mouse_dx * 0.0006f, -0.02f, 0.02f), 6.f, dt);
        p->sway_y = approachf(p->sway_y, clampf(-in->mouse_dy * 0.0006f, -0.02f, 0.02f), 6.f, dt);
        if (in->pressed['F']) {
            p->flashlight_on = p->flashlight_on > 0.5f ? 0.f : 1.f;
            sfx(g, g->as->snd_click, NULL, 0.4f, 1.6f, 3);
        }
        // heal
        if (in->pressed['H'] && p->heal_charges > 0 && p->hp < p->max_hp) {
            p->heal_charges--;
            p->hp = minf(p->max_hp, p->hp + 45.f);
            w->heal_flash = 0.6f;
            sfx(g, g->as->snd_heal, &p->pos, 0.8f, 1.f, 1);
        }
        player_update_weapon(g, p, dt);
        interact_update(g, p);
        if (in->pressed['E']) interact_use(g, p);
    }
    // rotational movement
    if (vlen(wish) > 0.01f) {
        wish = vnorm(wish);
        float cy = cosf(p->yaw), sy = sinf(p->yaw);
        Vec3 world_wish = v3(wish.x * cy + wish.z * sy, 0, -wish.x * sy + wish.z * cy);
        wish = world_wish;
    }
    player_move(g, p, dt, wish, sprint);

    // flashlight battery
    if (p->flashlight_on > 0.5f) {
        p->flashlight_charge = maxf(0.f, p->flashlight_charge - dt * 0.55f);
        if (p->flashlight_charge <= 0.f) p->flashlight_on = 0.f;
    }
    // bob
    float sp = vlen(v3(p->vel.x, 0, p->vel.z));
    p->bob += dt * (2.2f + sp * 1.55f);
    if (sp < 0.2f) p->bob += dt * 0.55f;
    // heartbeat when hurt
    if (p->hp < 40.f && p->alive) {
        p->heartbeat_t -= dt;
        if (p->heartbeat_t <= 0.f) {
            p->heartbeat_t = 0.55f + p->hp / 90.f;
            sfx(g, g->as->snd_heartbeat, NULL, 0.45f * (1.f - p->hp / 60.f), 1.f, 1);
        }
    }
    // passive regen during the day only, and slowly at night when safe
    if (w->state == GS_DAY && p->last_damage_time > 6.f) {
        p->hp = minf(p->max_hp, p->hp + dt * 2.2f);
        p->stamina = minf(p->max_stamina, p->stamina + dt * 12.f);
    }
}

// ---------------------------------------------------------------------------
// Barricades
// ---------------------------------------------------------------------------
void barricade_place(Game *g, int slot, int level) {
    World *w = g->w;
    if (slot < 0 || slot >= w->level.slot_count) return;
    BarricadeSlot *s = &w->level.slots[slot];
    int idx = s->barricade;
    if (idx < 0) {
        for (int i = 0; i < MAX_BARRICADES; i++) {
            if (!w->barricades[i].active) {
                idx = i;
                break;
            }
        }
        if (idx < 0) return;
        Barricade *b = &w->barricades[idx];
        memset(b, 0, sizeof *b);
        b->active = 1;
        b->slot = slot;
        b->level = level;
        b->max_hp = level == 1 ? 110.f : (level == 2 ? 240.f : 430.f);
        b->hp = b->max_hp;
        b->pos = s->pos;
        b->yaw = s->yaw;
        b->width = s->width;
        b->build_t = 0.f;
        s->barricade = idx;
        sfx(g, g->as->snd_build, &s->pos, 0.9f, 1.f, 1);
    } else {
        Barricade *b = &w->barricades[idx];
        if (level > b->level) {
            b->level = level;
            b->max_hp = level == 1 ? 110.f : (level == 2 ? 240.f : 430.f);
            b->hp = b->max_hp;
            b->build_t = 0.35f;
            sfx(g, g->as->snd_build, &s->pos, 0.9f, 0.85f, 1);
        } else {
            b->hp = b->max_hp;
            sfx(g, g->as->snd_build, &s->pos, 0.7f, 1.2f, 1);
        }
    }
    fx_dust(g, s->pos, 8);
}

void barricade_damage(Game *g, int index, float dmg) {
    World *w = g->w;
    if (index < 0 || index >= MAX_BARRICADES) return;
    Barricade *b = &w->barricades[index];
    if (!b->active) return;
    b->hp -= dmg;
    b->damage_flash = 1.f;
    if (b->hp <= 0.f) {
        b->active = 0;
        b->hp = 0.f;
        if (b->slot >= 0) w->level.slots[b->slot].barricade = -1;
        fx_glass(g, b->pos, b->level == 3 ? 22 : 14);
        fx_dust(g, b->pos, 18);
        sfx(g, b->level == 3 ? g->as->snd_metal[1] : g->as->snd_break[1], &b->pos, 1.f, 0.9f, 1);
        g->shake_amt = maxf(g->shake_amt, 0.35f);
        snprintf(w->subtitle, sizeof w->subtitle, "A barricade has been destroyed.");
        w->subtitle_t = 3.f;
    }
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------
void interact_update(Game *g, Player *p) {
    World *w = g->w;
    Vec3 eye = vadd(p->pos, v3(0, PLAYER_EYE, 0));
    Vec3 dir = camera_dir(p);
    float best = 3.0f;
    int best_kind = -1, best_idx = -1;
    char text[96] = {0};

    // barricade slots
    for (int i = 0; i < w->level.slot_count; i++) {
        BarricadeSlot *s = &w->level.slots[i];
        Vec3 to = vsub(s->pos, eye);
        float d = vlen(to);
        if (d > 3.2f) continue;
        if (vdot(vnorm(to), dir) < 0.25f) continue;
        if (d < best) {
            best = d;
            best_kind = 0;
            best_idx = i;
            if (s->barricade < 0) {
                if (p->scrap >= 2) snprintf(text, sizeof text, "[E]  BUILD BARRICADE     2 scrap");
                else snprintf(text, sizeof text, "BARRICADE  -  NEED 2 SCRAP");
            } else {
                Barricade *b = &w->barricades[s->barricade];
                if (b->hp < b->max_hp) snprintf(text, sizeof text, "[E]  REPAIR (%d%%)", (int)(b->hp / b->max_hp * 100));
                else if (b->level < 3) snprintf(text, sizeof text, "[E]  UPGRADE  LVL %d -> %d   %d scrap", b->level, b->level + 1, b->level == 1 ? 4 : 8);
                else snprintf(text, sizeof text, "BARRICADE  LVL 3  (%d%%)", (int)(b->hp / b->max_hp * 100));
            }
        }
    }
    // shop / night starter
    if (w->state == GS_DAY) {
        float d = vdist(p->pos, w->level.counter_pos);
        if (d < 3.6f && d < best) {
            best = d;
            best_kind = 1;
            best_idx = 0;
            snprintf(text, sizeof text, "[E]  OPEN SUPPLY DESK");
        }
    }
    // generator
    {
        float d = vdist(p->pos, w->level.generator_pos);
        if (d < 2.8f && d < best) {
            best = d;
            best_kind = 2;
            best_idx = 0;
            if (w->night_phase >= 0 && w->cfg.power == 0) snprintf(text, sizeof text, "[E]  REPAIR GENERATOR   6 scrap");
            else snprintf(text, sizeof text, "GENERATOR  -  ONLINE");
        }
    }
    // loot containers (pickups are auto-collected, this is the searchable crate)
    for (int i = 0; i < w->pickup_count; i++) {
        Pickup *pk = &w->pickups[i];
        if (!pk->active) continue;
        float d = vdist(p->pos, pk->pos);
        if (d > 2.4f) continue;
        if (d < best) {
            best = d;
            best_kind = 3;
            best_idx = i;
            const char *nm = "SUPPLIES";
            switch (pk->kind) {
                case IT_AMMO_PISTOL: nm = "9mm AMMUNITION"; break;
                case IT_AMMO_SHELL: nm = "12 GAUGE SHELLS"; break;
                case IT_AMMO_RIFLE: nm = "5.56 AMMUNITION"; break;
                case IT_AMMO_BOLT: nm = "CROSSBOW BOLTS"; break;
                case IT_MEDKIT: nm = "FIELD MEDKIT"; break;
                case IT_BANDAGE: nm = "BANDAGES"; break;
                case IT_PILLS: nm = "PAINKILLERS"; break;
                case IT_ADRENALINE: nm = "ADRENALINE"; break;
                case IT_SCRAP: nm = "SCRAP METAL"; break;
                case IT_MONEY: nm = "CASH"; break;
                case IT_BATTERY: nm = "BATTERY"; break;
                default: break;
            }
            snprintf(text, sizeof text, "[E]  TAKE %s", nm);
        }
    }
    if (best_kind >= 0) {
        g->interact_index = best_idx;
        g->interact_target_t = 1.f;
        snprintf(g->interact_text, sizeof g->interact_text, "%s", text);
    } else {
        g->interact_target_t = 0.f;
        g->interact_text[0] = 0;
    }
}

void interact_use(Game *g, Player *p) {
    World *w = g->w;
    if (g->interact_target_t <= 0.f) return;
    // figure out what the hint refers to by re-running the search
    // (cheap enough at one call per key press)
    int kind = -1, idx = g->interact_index;
    if (strstr(g->interact_text, "BARRICADE") || strstr(g->interact_text, "REPAIR (")) kind = 0;
    else if (strstr(g->interact_text, "SUPPLY DESK")) kind = 1;
    else if (strstr(g->interact_text, "GENERATOR")) kind = 2;
    else if (strstr(g->interact_text, "TAKE")) kind = 3;
    if (kind == 0) {
        BarricadeSlot *s = &w->level.slots[idx];
        if (s->barricade < 0) {
            if (p->scrap >= 2) {
                p->scrap -= 2;
                barricade_place(g, idx, 1);
            } else {
                snprintf(w->subtitle, sizeof w->subtitle, "You need 2 scrap to build a barricade.");
                w->subtitle_t = 2.5f;
                sfx(g, g->as->snd_ui_back, NULL, 0.5f, 1.f, 3);
            }
        } else {
            Barricade *b = &w->barricades[s->barricade];
            if (b->hp < b->max_hp) {
                if (p->scrap >= 1) {
                    p->scrap -= 1;
                    b->hp = minf(b->max_hp, b->hp + b->max_hp * 0.55f);
                    sfx(g, g->as->snd_build, &b->pos, 0.8f, 1.1f, 1);
                } else {
                    snprintf(w->subtitle, sizeof w->subtitle, "You need 1 scrap to repair.");
                    w->subtitle_t = 2.5f;
                }
            } else if (b->level < 3) {
                int cost = b->level == 1 ? 4 : 8;
                if (p->scrap >= cost) {
                    p->scrap -= cost;
                    barricade_place(g, idx, b->level + 1);
                } else {
                    snprintf(w->subtitle, sizeof w->subtitle, "Upgrade needs %d scrap.", cost);
                    w->subtitle_t = 2.5f;
                }
            }
        }
    } else if (kind == 1) {
        w->state = GS_SHOP;
        w->state_time = 0.f;
        g->shop_sel = 0;
        sfx(g, g->as->snd_ui_select, NULL, 0.9f, 1.f, 3);
    } else if (kind == 2) {
        if (w->cfg.power == 0) {
            if (p->scrap >= 6) {
                p->scrap -= 6;
                w->cfg.power = 1;
                for (int i = 0; i < MAX_ZOMBIES; i++) (void)i;
                g->flash = 0.7f;
                snprintf(w->subtitle, sizeof w->subtitle, "The generator coughs, catches, and the lights come back.");
                w->subtitle_t = 4.f;
                sfx(g, g->as->snd_metal[0], &w->level.generator_pos, 1.f, 1.f, 1);
            } else {
                snprintf(w->subtitle, sizeof w->subtitle, "The generator needs 6 scrap.");
                w->subtitle_t = 2.5f;
            }
        }
    } else if (kind == 3) {
        Pickup *pk = &w->pickups[idx];
        if (!pk->active) return;
        switch (pk->kind) {
            case IT_AMMO_PISTOL: p->ammo[AMMO_PISTOL] = mini(AMMO_PISTOL_MAX, p->ammo[AMMO_PISTOL] + pk->amount); break;
            case IT_AMMO_SHELL: p->ammo[AMMO_SHELL] = mini(AMMO_SHELL_MAX, p->ammo[AMMO_SHELL] + pk->amount); break;
            case IT_AMMO_RIFLE: p->ammo[AMMO_RIFLE] = mini(AMMO_RIFLE_MAX, p->ammo[AMMO_RIFLE] + pk->amount); break;
            case IT_AMMO_BOLT: p->ammo[AMMO_BOLT] = mini(AMBOLT_MAX, p->ammo[AMMO_BOLT] + pk->amount); break;
            case IT_MEDKIT: p->heal_charges += 2; break;
            case IT_BANDAGE: p->heal_charges += 1; break;
            case IT_PILLS: p->hp = minf(p->max_hp, p->hp + 30.f); break;
            case IT_ADRENALINE: p->stamina = p->max_stamina; p->exhaustion = 0.f; break;
            case IT_SCRAP: p->scrap += pk->amount; break;
            case IT_MONEY: p->money += pk->amount; break;
            case IT_BATTERY: p->flashlight_charge = 100.f; break;
        }
        pk->active = 0;
        sfx(g, g->as->snd_pickup, &pk->pos, 0.8f, 1.f, 1);
        snprintf(w->kill_notify, sizeof w->kill_notify, "+%s", g->interact_text + 9);
        w->kill_notify_t = 1.6f;
    }
}

// ---------------------------------------------------------------------------
// Night / wave management
// ---------------------------------------------------------------------------
void wave_start_night(Game *g, int night) {
    World *w = g->w;
    w->night = clampi(night, 1, 10);
    w->cfg = *night_config(w->night);
    w->state = GS_NIGHT_INTRO;
    w->state_time = 0.f;
    w->night_phase = 0;
    w->zombies_remaining = w->cfg.total_zombies;
    w->spawned_this_night = 0;
    w->night_kills = 0;
    w->spawn_timer = 2.5f;
    w->night_complete = 0;
    w->mini_boss_spawned = 0;
    w->boss_spawned = 0;
    w->boss_dead = 0;
    w->night_timer = w->cfg.prep_time;
    w->perfect_night = 1;
    w->money = w->cfg.start_money;
    w->scrap = w->cfg.start_money / 20;
    g->intro_t = 4.5f;
    g->rain_amount = (w->night >= 3) ? clampf((w->night - 2) * 0.18f, 0.2f, 0.9f) : 0.f;
    if (w->night >= 7) g->rain_amount = 1.f;
    for (int i = 0; i < w->player_count; i++) {
        Player *p = &w->players[i];
        if (!p->active) continue;
        p->hp = maxf(p->hp, p->max_hp * 0.6f);
        p->alive = 1;
        p->downed = 0;
    }
    snprintf(w->objective, sizeof w->objective, "NIGHT %d  -  SURVIVE", w->night);
    snprintf(w->subtitle, sizeof w->subtitle, "%s", w->cfg.subtitle);
    w->subtitle_t = 6.f;
    // build the flow field toward the store centre
    nav_update_flow(&w->level.nav, w->level.counter_pos);
    sfx(g, g->as->snd_stinger[0], NULL, 0.9f, 1.f, 2);
    LOGI("night %d started: %d zombies, %d waves", w->night, w->cfg.total_zombies, w->cfg.waves);
}

static ZombieType pick_type(Game *g) {
    World *w = g->w;
    float r = rng_f(&w->rng);
    const NightConfig *c = &w->cfg;
    float acc = c->walker_pct;
    if (r < acc) return ZB_WALKER;
    acc += c->runner_pct;
    if (r < acc) return ZB_RUNNER;
    acc += c->crawler_pct;
    if (r < acc) return ZB_CRAWLER;
    acc += c->brute_pct;
    if (r < acc) return ZB_BRUTE;
    return ZB_SCREAMER;
}

static int count_zombies(Game *g, int *bruisers) {
    int n = 0;
    if (bruisers) *bruisers = 0;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &g->w->zombies[i];
        if (!z->alive || z->state == ZS_DEAD) continue;
        n++;
        if (bruisers && (z->type == ZB_BRUTE || z->type == ZB_MINIBOSS || z->type == ZB_BOSS)) (*bruisers)++;
    }
    return n;
}

static Vec3 pick_spawn(Game *g) {
    World *w = g->w;
    Player *lp = &w->players[w->local_player];
    int late = w->night >= 6;
    float r = rng_f(&w->rng);
    Vec3 pos = w->level.spawns[0].pos;
    int tries = 0;
    do {
        int interior = late && r < 0.14f;
        int lo = interior ? 40 : 0, hi = interior ? w->level.spawn_count : 40;
        if (hi > w->level.spawn_count) hi = w->level.spawn_count;
        int idx = lo + (int)(rng_f(&w->rng) * (float)(hi - lo - 1));
        idx = clampi(idx, 0, w->level.spawn_count - 1);
        pos = w->level.spawns[idx].pos;
        pos.x += rng_range(&w->rng, -0.8f, 0.8f);
        pos.z += rng_range(&w->rng, -0.8f, 0.8f);
        tries++;
        if (vdist(pos, lp->pos) > 14.f) break;
    } while (tries < 12);
    return pos;
}

void wave_update(Game *g, float dt) {
    World *w = g->w;
    if (w->state != GS_NIGHT && w->state != GS_NIGHT_INTRO) return;
    w->night_timer -= dt;
    if (w->state == GS_NIGHT_INTRO) {
        if (w->state_time > 3.6f) {
            w->state = GS_NIGHT;
            w->state_time = 0.f;
            w->night_phase = 1;
        }
        return;
    }
    int alive = count_zombies(g, NULL);
    int wave = w->cfg.waves;
    (void)wave;

    // spawn cadence: faster as the night progresses / as the player kills
    float interval = w->cfg.spawn_interval;
    if (w->spawned_this_night > w->cfg.total_zombies / 2) interval *= 0.8f;
    if (alive < 3) interval *= 0.55f;
    w->spawn_timer -= dt;
    int concurrent_cap = w->cfg.max_concurrent + (w->night / 3);
    if (w->spawn_timer <= 0.f && w->spawned_this_night < w->cfg.total_zombies && alive < concurrent_cap) {
        w->spawn_timer = interval * rng_range(&w->rng, 0.75f, 1.25f);
        int burst = 1;
        if (w->spawned_this_night > w->cfg.total_zombies * 3 / 4) burst = 2;
        for (int b = 0; b < burst && w->spawned_this_night < w->cfg.total_zombies; b++) {
            ai_spawn_zombie(g, pick_type(g), pick_spawn(g), w->night);
            w->spawned_this_night++;
        }
    }
    // mini-boss on night 5, final boss on night 10
    if (w->cfg.mini_boss && !w->mini_boss_spawned && w->spawned_this_night > w->cfg.total_zombies / 2) {
        w->mini_boss_spawned = 1;
        Vec3 p = pick_spawn(g);
        ai_spawn_zombie(g, ZB_MINIBOSS, p, w->night);
        snprintf(w->subtitle, sizeof w->subtitle, "Something enormous is coming through the lot.");
        w->subtitle_t = 5.f;
        sfx(g, g->as->snd_roar[0], &p, 1.f, 0.85f, 1);
        g->shake_amt = 0.6f;
    }
    if (w->cfg.boss && !w->boss_spawned && w->spawned_this_night > w->cfg.total_zombies / 2) {
        w->boss_spawned = 1;
        Vec3 p = v3(0, 0, 30.f);
        ai_spawn_zombie(g, ZB_BOSS, p, w->night);
        snprintf(w->subtitle, sizeof w->subtitle, "THE STOREKEEPER walks out of the dark.");
        w->subtitle_t = 6.f;
        sfx(g, g->as->snd_roar[1], &p, 1.f, 0.7f, 1);
        g->shake_amt = 1.f;
        w->boss_active = 1;
    }
    // night end conditions
    int remaining_to_spawn = w->cfg.total_zombies - w->spawned_this_night;
    if (remaining_to_spawn <= 0 && alive == 0) {
        w->night_complete = 1;
        wave_end_night(g, 1);
    }
}

void wave_end_night(Game *g, int survived) {
    World *w = g->w;
    if (w->state == GS_NIGHT_END) return;
    w->state = GS_NIGHT_END;
    w->state_time = 0.f;
    w->night_phase = 2;
    if (survived) {
        int bonus = 60 + w->night * 25;
        if (w->perfect_night) bonus += 120;
        w->money += bonus;
        g->night_cleared = w->night;
        snprintf(w->subtitle, sizeof w->subtitle, "NIGHT %d SURVIVED", w->night);
        w->subtitle_t = 5.f;
        sfx(g, g->as->snd_stinger[1], NULL, 0.9f, 1.f, 2);
        if (w->night >= 10) {
            w->state = GS_VICTORY;
            w->state_time = 0.f;
            LOGI("victory: all ten nights survived");
        }
        save_write(g);
    }
}

// ---------------------------------------------------------------------------
// Pickups
// ---------------------------------------------------------------------------
static void add_pickup(Game *g, int kind, Vec3 pos, int amount) {
    World *w = g->w;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (w->pickups[i].active) continue;
        Pickup *p = &w->pickups[i];
        memset(p, 0, sizeof *p);
        p->active = 1;
        p->kind = kind;
        p->pos = pos;
        p->amount = amount;
        p->bob = rng_f(&w->rng) * 6.28f;
        p->life = 0.f;
        w->pickup_count++;
        return;
    }
}

void game_scatter_loot(Game *g, int night) {
    World *w = g->w;
    memset(w->pickups, 0, sizeof w->pickups);
    w->pickup_count = 0;
    Rng *r = &w->rng;
    int n = 22 + night * 2;
    for (int i = 0; i < n; i++) {
        float x = rng_range(r, -24.f, 24.f);
        float z = rng_range(r, -12.5f, 18.5f);
        if (world_point_blocked(g, v3(x, 0, z), 0.4f)) continue;
        int kind;
        float roll = rng_f(r);
        if (roll < 0.24f) kind = IT_SCRAP;
        else if (roll < 0.44f) kind = IT_AMMO_PISTOL;
        else if (roll < 0.54f) kind = IT_AMMO_SHELL;
        else if (roll < 0.62f) kind = IT_AMMO_RIFLE;
        else if (roll < 0.68f) kind = IT_AMMO_BOLT;
        else if (roll < 0.78f) kind = IT_MONEY;
        else if (roll < 0.84f) kind = IT_BANDAGE;
        else if (roll < 0.9f) kind = IT_MEDKIT;
        else if (roll < 0.94f) kind = IT_BATTERY;
        else if (roll < 0.97f) kind = IT_PILLS;
        else kind = IT_ADRENALINE;
        int amount = 1;
        switch (kind) {
            case IT_AMMO_PISTOL: amount = 12 + (int)(rng_f(r) * 3) * 6; break;
            case IT_AMMO_SHELL: amount = 4 + (int)(rng_f(r) * 4); break;
            case IT_AMMO_RIFLE: amount = 18 + (int)(rng_f(r) * 4) * 6; break;
            case IT_AMMO_BOLT: amount = 2 + (int)(rng_f(r) * 3); break;
            case IT_SCRAP: amount = 1 + (int)(rng_f(r) * 2); break;
            case IT_MONEY: amount = 15 + (int)(rng_f(r) * 40); break;
            default: amount = 1; break;
        }
        add_pickup(g, kind, v3(x, 0.16f, z), amount);
    }
    // guaranteed resupply at the counter
    add_pickup(g, IT_AMMO_PISTOL, v3(w->level.counter_pos.x + 3.f, 0.16f, w->level.counter_pos.z - 1.2f), 30);
    add_pickup(g, IT_SCRAP, v3(w->level.counter_pos.x - 3.f, 0.16f, w->level.counter_pos.z - 1.2f), 3);
    add_pickup(g, IT_MEDKIT, v3(w->level.counter_pos.x, 0.16f, w->level.counter_pos.z + 1.6f), 1);
}

void game_update_pickups(Game *g, float dt) {
    World *w = g->w;
    Player *lp = &w->players[w->local_player];
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &w->pickups[i];
        if (!p->active) continue;
        p->bob += dt * 2.2f;
        p->life += dt;
        if (lp->alive && vdist(p->pos, lp->pos) < 0.9f && p->kind != IT_SCRAP && p->kind != IT_MONEY) {
            // auto-pickup ammo and consumables; scrap and cash need a tap so the
            // player notices them
            switch (p->kind) {
                case IT_AMMO_PISTOL: lp->ammo[AMMO_PISTOL] = mini(AMMO_PISTOL_MAX, lp->ammo[AMMO_PISTOL] + p->amount); break;
                case IT_AMMO_SHELL: lp->ammo[AMMO_SHELL] = mini(AMMO_SHELL_MAX, lp->ammo[AMMO_SHELL] + p->amount); break;
                case IT_AMMO_RIFLE: lp->ammo[AMMO_RIFLE] = mini(AMMO_RIFLE_MAX, lp->ammo[AMMO_RIFLE] + p->amount); break;
                case IT_AMMO_BOLT: lp->ammo[AMMO_BOLT] = mini(AMBOLT_MAX, lp->ammo[AMMO_BOLT] + p->amount); break;
                case IT_MEDKIT: lp->heal_charges += 2; break;
                case IT_BANDAGE: lp->heal_charges += 1; break;
                case IT_PILLS: lp->hp = minf(lp->max_hp, lp->hp + 30.f); break;
                case IT_ADRENALINE: lp->stamina = lp->max_stamina; break;
                case IT_BATTERY: lp->flashlight_charge = 100.f; break;
            }
            p->active = 0;
            sfx(g, g->as->snd_pickup, &p->pos, 0.6f, 1.1f, 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Projectiles
// ---------------------------------------------------------------------------
void game_update_projectiles(Game *g, float dt) {
    World *w = g->w;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *pr = &w->projectiles[i];
        if (!pr->active) continue;
        pr->life -= dt;
        if (pr->life <= 0.f) { pr->active = 0; continue; }
        Vec3 prev = pr->pos;
        pr->vel.y -= 5.5f * dt;
        pr->pos = vmad(pr->pos, pr->vel, dt);
        if (pr->pos.y < 0.05f) {
            pr->active = 0;
            fx_dust(g, v3(pr->pos.x, 0.1f, pr->pos.z), 4);
            continue;
        }
        float ht;
        if (level_raycast(&w->level, prev, pr->pos, &ht, NULL) && ht < vlen(vsub(pr->pos, prev))) {
            pr->active = 0;
            fx_dust(g, pr->pos, 3);
            continue;
        }
        for (int k = 0; k < MAX_ZOMBIES; k++) {
            Zombie *z = &w->zombies[k];
            if (!z->alive || z->state == ZS_DEAD) continue;
            Vec3 c = vadd(z->pos, v3(0, z->height * 0.55f, 0));
            if (vdist(c, pr->pos) < z->radius + pr->radius + 0.12f) {
                Vec3 dir = vnorm(pr->vel);
                ai_damage_zombie(g, z, pr->damage, dir, 0, 1);
                fx_blood(g, pr->pos, dir, 9);
                pr->active = 0;
                break;
            }
        }
    }
}
