// Zombie artificial intelligence: navigation, barricade assault, attack,
// stagger, stuck recovery and the two boss encounters.
#include "game.h"
#include <string.h>

typedef struct ZStats {
    float hp, speed, damage, radius, height, attack_range, attack_cd, scale;
    const char *name;
} ZStats;

static const ZStats g_zstats[ZB_TYPE_COUNT] = {
    {70.f, 1.15f, 11.f, 0.34f, 1.80f, 1.35f, 1.25f, 1.00f, "Walker"},
    {55.f, 2.65f, 9.f, 0.32f, 1.78f, 1.30f, 0.95f, 0.97f, "Runner"},
    {45.f, 0.85f, 8.f, 0.34f, 0.65f, 1.20f, 1.10f, 1.00f, "Crawler"},
    {260.f, 1.35f, 26.f, 0.52f, 2.20f, 1.75f, 1.70f, 1.30f, "Brute"},
    {60.f, 1.45f, 6.f, 0.32f, 1.75f, 1.30f, 1.40f, 0.95f, "Screamer"},
    {1400.f, 1.55f, 34.f, 0.70f, 2.9f, 2.10f, 2.0f, 1.65f, "The Butcher"},
    {5200.f, 1.70f, 46.f, 0.95f, 3.8f, 2.60f, 2.2f, 2.20f, "The Storekeeper"},
};

// Is a point blocked for walking? static level + active barricades
int world_point_blocked(Game *g, Vec3 p, float radius) {
    if (level_point_blocked(&g->w->level, p, radius)) return 1;
    for (int i = 0; i < MAX_BARRICADES; i++) {
        const Barricade *b = &g->w->barricades[i];
        if (!b->active || b->build_t < 0.99f) continue;
        float hw = b->width * 0.5f, hd = 0.22f;
        // rotate into the barricade frame
        float cs = cosf(-b->yaw), sn = sinf(-b->yaw);
        float lx = (p.x - b->pos.x) * cs - (p.z - b->pos.z) * sn;
        float lz = (p.x - b->pos.x) * sn + (p.z - b->pos.z) * cs;
        lx = clampf(lx, -hw, hw);
        lz = clampf(lz, -hd, hd);
        // back to world
        float wx = lx * cs + lz * sn + b->pos.x;
        float wz = -lx * sn + lz * cs + b->pos.z;
        float dx = p.x - wx, dz = p.z - wz;
        if (dx * dx + dz * dz < radius * radius) return 1;
    }
    return 0;
}

// Slide-aware movement: try full move, then each axis.
static void move_entity(Game *g, Vec3 *pos, Vec3 delta, float radius) {
    Vec3 p = *pos;
    Vec3 tryp = v3(p.x + delta.x, p.y, p.z + delta.z);
    if (!world_point_blocked(g, tryp, radius)) { *pos = tryp; return; }
    tryp = v3(p.x + delta.x, p.y, p.z);
    if (!world_point_blocked(g, tryp, radius)) { *pos = tryp; return; }
    tryp = v3(p.x, p.y, p.z + delta.z);
    if (!world_point_blocked(g, tryp, radius)) { *pos = tryp; return; }
}

static void zombie_pick_barricade(Game *g, Zombie *z) {
    // Find the closest barricaded opening that lies roughly toward the target.
    World *w = g->w;
    float best = 1e9f;
    int bi = -1;
    for (int i = 0; i < MAX_BARRICADES; i++) {
        Barricade *b = &w->barricades[i];
        if (!b->active) continue;
        float d = vdist(z->pos, b->pos);
        if (d > 26.f) continue;
        // prefer barricades on the way to the player
        float dp = vdist(b->pos, z->target);
        if (dp > vdist(z->pos, z->target) + 4.f) continue;
        if (d < best) { best = d; bi = i; }
    }
    z->barricade_idx = bi;
}

void ai_spawn_zombie(Game *g, ZombieType type, Vec3 pos, int night_difficulty) {
    World *w = g->w;
    int slot = -1;
    for (int i = 0; i < MAX_ZOMBIES; i++) if (!w->zombies[i].alive) { slot = i; break; }
    if (slot < 0) return;
    Zombie *z = &w->zombies[slot];
    const ZStats *s = &g_zstats[type];
    memset(z, 0, sizeof *z);
    float hp_scale = 1.f + (float)(night_difficulty - 1) * 0.13f;
    if (type == ZB_MINIBOSS) hp_scale = 1.f + (float)(night_difficulty - 5) * 0.10f;
    if (type == ZB_BOSS) hp_scale = 1.f;
    z->type = type;
    z->max_hp = s->hp * hp_scale;
    z->hp = z->max_hp;
    z->speed = s->speed * (1.f + (float)(night_difficulty - 1) * 0.018f);
    z->damage = s->damage * (1.f + (float)(night_difficulty - 1) * 0.05f);
    z->radius = s->radius;
    z->height = s->height;
    z->scale = s->scale * (0.93f + rng_f(&w->rng) * 0.16f);
    z->pos = pos;
    z->pos.y = 0.f;
    z->spawn_ground_y = 0.f;
    z->yaw = rng_f(&w->rng) * 6.28f;
    z->state = ZS_SPAWNING;
    z->state_time = 0.f;
    z->spawn_alpha = 0.f;
    z->attack_cd = 0.4f + rng_f(&w->rng) * 0.6f;
    z->limb_seed = rng_f(&w->rng) * 6.28f;
    z->anim_phase = rng_f(&w->rng);
    z->groan_timer = rng_range(&w->rng, 1.f, 9.f);
    z->scream_cd = rng_range(&w->rng, 5.f, 14.f);
    z->alive = 1;
    z->target_player = -1;
    z->barricade_idx = -1;
    z->net_id = 0;
    w->zombie_count++;
    if (g->zanim_state[slot] != -1) {
        g->zanim_state[slot] = -1;   // force an animator restart
    }
}

static void choose_target(Game *g, Zombie *z) {
    World *w = g->w;
    float best = 1e9f;
    int bi = -1;
    for (int i = 0; i < w->player_count; i++) {
        Player *p = &w->players[i];
        if (!p->active || !p->alive) continue;
        float d = vdist(z->pos, p->pos);
        if (d < best) { best = d; bi = i; }
    }
    z->target_player = bi;
    if (bi >= 0) z->target = w->players[bi].pos;
    else z->target = w->level.player_spawn;
}

void ai_damage_zombie(Game *g, Zombie *z, float dmg, Vec3 dir, int headshot, int by_player) {
    World *w = g->w;
    if (!z->alive || z->state == ZS_DEAD) return;
    if (headshot) dmg *= 2.35f;
    z->hp -= dmg;
    z->hit_flash = 1.f;
    if (by_player) {
        w->shots_hit++;
        w->hitmarker = 1.f;
        w->hitmarker_kill = 0;
    }
    if (z->hp <= 0.f) {
        z->state = ZS_DEAD;
        z->state_time = 0.f;
        z->death_time = 0.f;
        z->vel = vscale(vnorm(dir), 1.4f);
        w->night_kills++;
        w->kills++;
        if (headshot) w->headshots++;
        if (by_player) { w->hitmarker_kill = 1; w->hitmarker = 1.4f; }
        fx_blood(g, vadd(z->pos, v3(0, z->height * 0.65f, 0)), v3(0, 1, 0), 16);
        fx_gib(g, vadd(z->pos, v3(0, z->height * 0.5f, 0)), 8);
        fx_decal(g, v3(z->pos.x, 0.012f, z->pos.z), v3(0, 1, 0), g->as->blood, 1.5f, COL(95, 12, 12, 210));
        sfx(g, g->as->snd_groan[(int)(rng_f(&w->rng) * 5.99f)], &z->pos, 0.7f, 0.7f, 1);
        if (z->type == ZB_BOSS) {
            w->boss_dead = 1;
            w->boss_active = 0;
            g->shake_amt = 0.9f;
            sfx(g, g->as->snd_roar[0], &z->pos, 1.f, 0.6f, 1);
        } else if (z->type == ZB_MINIBOSS) {
            w->mini_boss_spawned = 2;
            g->shake_amt = 0.7f;
        }
        if (by_player && z->type >= ZB_MINIBOSS) {
            snprintf(w->kill_notify, sizeof w->kill_notify, "%s DOWN", g_zstats[z->type].name);
            w->kill_notify_t = 4.f;
        }
        return;
    }
    // stagger: brutes and bosses mostly ignore it
    float chance = z->type == ZB_BRUTE ? 0.10f : (z->type == ZB_RUNNER ? 0.5f : 0.32f);
    if (z->type >= ZB_MINIBOSS) chance = 0.02f;
    if (rng_f(&w->rng) < chance && z->state != ZS_BARRICADE) {
        z->state = ZS_STAGGER;
        z->state_time = 0.f;
        z->stagger = z->type == ZB_BRUTE ? 0.35f : 0.55f;
    }
}

static int zombie_attack_player(Game *g, Zombie *z, float dt) {
    World *w = g->w;
    if (z->target_player < 0) return 0;
    Player *p = &w->players[z->target_player];
    if (!p->active || !p->alive || p->downed) return 0;
    Vec3 to = vsub(p->pos, z->pos);
    float dist = vlen(to);
    const ZStats *s = &g_zstats[z->type];
    if (dist > s->attack_range) return 0;
    // never attack through geometry
    Vec3 eye = vadd(z->pos, v3(0, z->height * 0.8f, 0));
    Vec3 peye = vadd(p->pos, v3(0, 1.2f, 0));
    float ht;
    if (level_raycast(&w->level, eye, peye, &ht, NULL) && ht < dist - 0.35f) return 0;
    if (z->attack_cd > 0.f) return 0;
    z->attack_cd = s->attack_cd;
    z->state = ZS_ATTACK;
    z->state_time = 0.f;
    // damage is applied at the animation hit frame (handled in the state machine)
    return 1;
}

static void zombie_hit_player(Game *g, Zombie *z) {
    World *w = g->w;
    if (z->target_player < 0) return;
    Player *p = &w->players[z->target_player];
    if (!p->active || !p->alive) return;
    Vec3 to = vsub(p->pos, z->pos);
    if (vlen(to) > g_zstats[z->type].attack_range + 0.6f) return;
    float dmg = z->damage;
    p->hp -= dmg * (1.f - p->armor * 0.5f);
    p->hit_flash = 1.f;
    p->last_damage_time = 0.f;
    p->shake = 0.6f;
    p->last_attacker = z->target_player;
    w->damage_flash = minf(1.f, w->damage_flash + 0.35f);
    g->damage_dir_t[0] = 1.f;
    Vec3 fwd = vnorm(v3(sinf(p->yaw), 0, cosf(p->yaw)));
    g->damage_dir[0] = vdot(vnorm(to), fwd);
    if (p->hp <= 0.f) {
        p->hp = 0.f;
        p->alive = 0;
        p->death_time = 0.f;
        w->deaths++;
        sfx(g, g->as->snd_scream[0], &p->pos, 1.f, 0.8f, 1);
    }
    fx_blood(g, vadd(z->pos, v3(0, 1.2f, 0)), vnorm(to), 6);
    sfx(g, g->as->snd_hit, &p->pos, 0.9f, 1.f, 1);
}

static void boss_special(Game *g, Zombie *z, int which) {
    World *w = g->w;
    switch (which) {
        case 0: {  // ground slam: area damage + shockwave
            for (int i = 0; i < w->player_count; i++) {
                Player *p = &w->players[i];
                if (!p->active || !p->alive) continue;
                float d = vdist(p->pos, z->pos);
                if (d < 6.5f) {
                    p->hp -= 34.f * (1.f - p->armor * 0.5f);
                    p->shake = 1.f;
                    w->damage_flash = 1.f;
                    if (p->hp <= 0.f) { p->hp = 0.f; p->alive = 0; p->death_time = 0.f; w->deaths++; }
                }
            }
            for (int i = 0; i < 40; i++) fx_dust(g, v3(z->pos.x + rng_range(&w->rng, -3, 3), 0.1f, z->pos.z + rng_range(&w->rng, -3, 3)), 1);
            g->shake_amt = 1.2f;
            sfx(g, g->as->snd_explosion, &z->pos, 1.f, 0.75f, 1);
            break;
        }
        case 1: {  // summon a pack
            Vec3 base = z->pos;
            for (int i = 0; i < 4; i++) {
                Vec3 p = v3(base.x + rng_range(&w->rng, -4.f, 4.f), 0, base.z + rng_range(&w->rng, -4.f, 4.f));
                ai_spawn_zombie(g, i < 3 ? ZB_WALKER : ZB_RUNNER, p, w->night);
            }
            sfx(g, g->as->snd_roar[1], &z->pos, 1.f, 1.f, 1);
            break;
        }
        case 2: {  // charge
            z->state = ZS_LUNGE;
            z->state_time = 0.f;
            z->lunge_time = 1.15f;
            z->vel = vscale(vnorm(vsub(z->target, z->pos)), 12.f);
            sfx(g, g->as->snd_roar[0], &z->pos, 1.f, 1.2f, 1);
            break;
        }
    }
}

void ai_update(Game *g, float dt) {
    World *w = g->w;
    if (w->state != GS_NIGHT && w->state != GS_NIGHT_INTRO && w->state != GS_NIGHT_END) {
        // still animate deaths during the day
        dt *= 1.f;
    }
    // boss health fraction for the HUD
    w->boss_hp_frac = 0.f;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &w->zombies[i];
        if (z->alive && z->type == ZB_BOSS) {
            w->boss_hp_frac = z->hp / z->max_hp;
            w->boss_active = 1;
        }
    }
    int alive_count = 0;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &w->zombies[i];
        if (!z->alive) continue;
        alive_count++;
        const ZStats *s = &g_zstats[z->type];
        z->state_time += dt;
        z->attack_cd = maxf(0.f, z->attack_cd - dt);
        z->hit_flash = maxf(0.f, z->hit_flash - dt * 4.f);
        z->groan_timer -= dt;
        z->scream_cd = maxf(0.f, z->scream_cd - dt);
        z->special_cd = maxf(0.f, z->special_cd - dt);
        z->spawn_alpha = minf(1.f, z->spawn_alpha + dt * 1.6f);
        z->anim_phase += dt * (z->state == ZS_CHASE ? maxf(0.7f, vlen(z->vel) * 0.55f) : 0.45f);

        if (z->groan_timer <= 0.f && vdist(z->pos, w->players[w->local_player].pos) < 26.f) {
            z->groan_timer = rng_range(&w->rng, 3.5f, 11.f);
            int gi = (int)(rng_f(&w->rng) * 5.99f);
            sfx(g, g->as->snd_groan[gi], &z->pos, 0.55f, 0.85f + rng_f(&w->rng) * 0.3f, 1);
        }

        if (z->state == ZS_DEAD) {
            z->death_time += dt;
            z->pos = vmad(z->pos, z->vel, dt);
            z->vel = vscale(z->vel, 1.f - 6.f * dt);
            if (z->death_time > 6.5f) {
                z->alive = 0;
                w->zombie_count--;
                if (z->type == ZB_BOSS) w->boss_active = 0;
            }
            continue;
        }
        if (z->state == ZS_SPAWNING) {
            if (z->state_time > 0.75f) {
                z->state = ZS_CHASE;
                z->state_time = 0.f;
            }
            continue;
        }

        choose_target(g, z);
        if (z->target_player < 0) {
            // no players alive: shamble toward the entrance and idle
            z->state = ZS_IDLE;
            continue;
        }
        Player *tp = &w->players[z->target_player];
        float dist_to_player = vdist(z->pos, tp->pos);

        // ---- boss pattern control
        if (z->type == ZB_MINIBOSS || z->type == ZB_BOSS) {
            float hpf = z->hp / z->max_hp;
            if (z->type == ZB_BOSS) {
                z->boss_phase = hpf > 0.66f ? 0 : (hpf > 0.33f ? 1 : 2);
            }
            if (z->special_cd <= 0.f && z->state != ZS_LUNGE) {
                if (z->type == ZB_MINIBOSS) {
                    if (dist_to_player < 4.5f) { boss_special(g, z, 0); z->special_cd = 7.5f; }
                } else {
                    int pick = (int)(rng_f(&w->rng) * 3.f);
                    if (pick == 0 && dist_to_player < 7.f) { boss_special(g, z, 0); z->special_cd = 6.f - z->boss_phase * 1.2f; }
                    else if (pick == 1) { boss_special(g, z, 1); z->special_cd = 9.f - z->boss_phase * 1.5f; }
                    else if (dist_to_player > 8.f) { boss_special(g, z, 2); z->special_cd = 8.f; }
                }
            }
        }

        // ---- screamer behaviour
        if (z->type == ZB_SCREAMER && z->scream_cd <= 0.f && dist_to_player < 24.f) {
            z->state = ZS_SCREAM;
            z->state_time = 0.f;
            z->scream_cd = rng_range(&w->rng, 13.f, 20.f);
            sfx(g, g->as->snd_scream[1 + (int)(rng_f(&w->rng) * 1.9f)], &z->pos, 1.f, 1.f, 1);
            // buff nearby zombies
            for (int k = 0; k < MAX_ZOMBIES; k++) {
                Zombie *o = &w->zombies[k];
                if (!o->alive || o == z || o->state == ZS_DEAD) continue;
                if (vdist(o->pos, z->pos) < 16.f) {
                    o->speed = minf(o->speed * 1.18f, g_zstats[o->type].speed * 1.6f);
                }
            }
            w->subtitle_t = 2.5f;
            snprintf(w->subtitle, sizeof w->subtitle, "A screamer is calling them in.");
        }

        // ---- barricade handling
        if (z->barricade_idx < 0 || !w->barricades[z->barricade_idx].active) {
            zombie_pick_barricade(g, z);
        }
        if (z->barricade_idx >= 0) {
            Barricade *b = &w->barricades[z->barricade_idx];
            float bd = vdist(z->pos, b->pos);
            if (b->active && bd < 2.2f && b->pos.y < 1.5f) {
                // check the barricade is actually between us and the store
                z->state = ZS_BARRICADE;
                z->state_time += 0.f;
                z->barricade_attack_timer += dt;
                z->vel = v3(0, 0, 0);
                float interval = z->type == ZB_BRUTE ? 0.85f : (z->type == ZB_RUNNER ? 0.7f : 1.15f);
                if (z->barricade_attack_timer >= interval) {
                    z->barricade_attack_timer = 0.f;
                    float dmg = z->damage * (z->type == ZB_BRUTE ? 2.6f : 0.95f);
                    barricade_damage(g, z->barricade_idx, dmg);
                    fx_dust(g, b->pos, 3);
                    sfx(g, g->as->snd_break[0], &b->pos, 0.7f, 1.1f, 1);
                    if (b->hp <= 0.f) z->barricade_idx = -1;
                }
                // face the barricade
                Vec3 d = vsub(b->pos, z->pos);
                z->yaw = atan2f(d.x, d.z);
                continue;
            }
        }

        // ---- state machine
        switch (z->state) {
            case ZS_IDLE:
                if (dist_to_player < 30.f) { z->state = ZS_CHASE; z->state_time = 0.f; }
                break;
            case ZS_STAGGER:
                z->vel = vscale(z->vel, 1.f - 4.f * dt);
                if (z->state_time > z->stagger) { z->state = ZS_CHASE; z->state_time = 0.f; }
                continue;
            case ZS_ATTACK:
                z->vel = v3(0, 0, 0);
                if (z->state_time > 0.42f && z->state_time - dt <= 0.42f) zombie_hit_player(g, z);
                if (z->state_time > 0.9f) { z->state = ZS_CHASE; z->state_time = 0.f; }
                continue;
            case ZS_SCREAM:
                z->vel = v3(0, 0, 0);
                if (z->state_time > 1.4f) { z->state = ZS_CHASE; z->state_time = 0.f; }
                continue;
            case ZS_LUNGE:
                z->lunge_time -= dt;
                move_entity(g, &z->pos, vscale(z->vel, dt), z->radius * 0.9f);
                if (dist_to_player < 2.6f) {
                    // impact
                    for (int k = 0; k < w->player_count; k++) {
                        Player *p = &w->players[k];
                        if (!p->active || !p->alive) continue;
                        if (vdist(p->pos, z->pos) < 3.2f) {
                            p->hp -= 30.f * (1.f - p->armor * 0.5f);
                            p->shake = 1.f;
                            w->damage_flash = 0.8f;
                            if (p->hp <= 0.f) { p->hp = 0.f; p->alive = 0; p->death_time = 0.f; w->deaths++; }
                        }
                    }
                    g->shake_amt = maxf(g->shake_amt, 0.6f);
                    z->lunge_time = 0.f;
                }
                if (z->lunge_time <= 0.f) { z->state = ZS_CHASE; z->state_time = 0.f; }
                continue;
            default: break;
        }

        // ---- attack check
        if (z->state == ZS_CHASE) {
            if (zombie_attack_player(g, z, dt)) continue;
        }

        // ---- navigation
        Vec2 flow = nav_flow_dir(&w->level.nav, z->pos);
        Vec3 desired;
        float know = 1.f;
        if (dist_to_player < 14.f && nav_line_clear(&w->level, z->pos, tp->pos, z->radius)) {
            Vec3 d = vsub(tp->pos, z->pos);
            d.y = 0;
            float l = vlen(d);
            if (l > 0.01f) { desired = vscale(d, 1.f / l); know = 1.f; }
            else desired = v3(0, 0, 0);
        } else {
            desired = v3(flow.x, 0, flow.y);
            if (v2len(flow) < 0.01f) {
                // fall back to steering directly at the player
                Vec3 d = vsub(tp->pos, z->pos);
                d.y = 0;
                float l = vlen(d);
                if (l > 0.01f) desired = vscale(d, 1.f / l);
                w->zombies_nav_fail++;
            }
        }
        (void)know;
        // separation from other zombies
        Vec3 sep = v3(0, 0, 0);
        int close = 0;
        for (int k = 0; k < MAX_ZOMBIES; k++) {
            if (k == i) continue;
            Zombie *o = &w->zombies[k];
            if (!o->alive || o->state == ZS_DEAD) continue;
            Vec3 d = vsub(z->pos, o->pos);
            d.y = 0;
            float l2 = vlen(d);
            if (l2 < (z->radius + o->radius) * 1.6f && l2 > 0.01f) {
                sep = vadd(sep, vscale(d, 1.f / (l2 * l2 + 0.05f)));
                close++;
            }
        }
        if (close) desired = vadd(desired, vscale(sep, 0.55f));
        desired.y = 0;
        if (vlen(desired) > 0.001f) desired = vnorm(desired);
        float spd = z->speed;
        if (z->type == ZB_BOSS) spd *= 0.85f + 0.15f * (float)z->boss_phase;
        Vec3 before = z->pos;
        if (i == 0 && g->show_debug) {
            Vec3 probe = vmad(z->pos, vscale(desired, 0.3f), 1.f);
            LOGI("dbg z0 flow=(%.3f,%.3f) desired=(%.3f,%.3f) spd=%.2f blocked_fwd=%d blocked_here=%d",
                 flow.x, flow.y, desired.x, desired.z, spd,
                 world_point_blocked(g, vmad(z->pos, desired, 0.3f), z->radius),
                 world_point_blocked(g, z->pos, z->radius));
            (void)probe;
        }
        move_entity(g, &z->pos, vscale(desired, spd * dt), z->radius);
        Vec3 moved = vsub(z->pos, before);
        moved.y = 0;
        z->vel = vscale(moved, 1.f / maxf(dt, 0.0001f));
        z->pos.y = 0.f;
        z->state = ZS_CHASE;
        if (vlen(moved) > dt * spd * 0.25f) {
            float target_yaw = atan2f(desired.x, desired.z);
            float diff = target_yaw - z->yaw;
            while (diff > (float)M_PI) diff -= 2.f * (float)M_PI;
            while (diff < -(float)M_PI) diff += 2.f * (float)M_PI;
            z->yaw += diff * minf(1.f, dt * 7.f);
            z->stuck_timer = maxf(0.f, z->stuck_timer - dt);
        } else {
            z->stuck_timer += dt;
            if (z->stuck_timer > 1.4f) {
                // stuck recovery: sidestep, then re-evaluate the path
                z->stuck_timer = 0.f;
                float a = rng_f(&w->rng) * 6.28f;
                Vec3 side = v3(cosf(a), 0, sinf(a));
                for (int k = 0; k < 6; k++) {
                    Vec3 probe = vmad(z->pos, v3(cosf(a + (float)k), 0, sinf(a + (float)k)), 1.2f);
                    if (!world_point_blocked(g, probe, z->radius)) {
                        move_entity(g, &z->pos, vscale(vnorm(v3(cosf(a + (float)k), 0, sinf(a + (float)k))), 0.6f), z->radius);
                        break;
                    }
                }
                (void)side;
                w->zombies_nav_fail++;
            }
        }
    }
    w->wave_zombies_alive = alive_count;
}
