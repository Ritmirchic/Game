// Scene rendering: geometry, characters, viewmodel, particles and lighting.
#include "game.h"
#include <string.h>

void rd_begin_scene(Game *g);
void scene_draw(Game *g);

static void setup_env(Game *g) {
    Renderer *r = g->rd;
    World *w = g->w;
    Player *p = &w->players[w->local_player];
    Environment *e = &g->env;
    // lightning
    if (g->lightning > 0.f) {
        float f = g->lightning * g->lightning;
        e->ambient_sky = vadd(e->ambient_sky, v3(0.35f * f, 0.38f * f, 0.45f * f));
    }
    if (g->flash > 0.f) {
        e->ambient_sky = vadd(e->ambient_sky, v3s(g->flash * 0.25f));
    }
    // low health: push the grade red
    float hurt = 0.f;
    if (p->hp < 45.f && p->alive) hurt = 1.f - p->hp / 45.f;
    if (hurt > 0.f) {
        e->grade_shadow = vlerp(e->grade_shadow, v3(1.25f, 0.62f, 0.58f), hurt * 0.65f);
        e->vignette = maxf(e->vignette, 0.42f + hurt * 0.5f);
        e->saturation = 0.92f - hurt * 0.25f;
    }
    r->env = *e;
}

static void add_light_if(Game *g, Light l) { rd_add_light(g->rd, l); }

static void setup_lights(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    rd_clear_lights(r);
    Player *p = &w->players[w->local_player];

    // ---- flashlight: the primary light source at night
    if (p->alive && p->flashlight_on > 0.5f && p->flashlight_charge > 0.f) {
        Light l;
        memset(&l, 0, sizeof l);
        l.type = 1;
        Vec3 dir = v3(sinf(p->yaw) * cosf(p->pitch), sinf(p->pitch), cosf(p->yaw) * cosf(p->pitch));
        l.pos = vadd(g->cam.pos, vscale(dir, 0.25f));
        l.dir = vnorm(dir);
        l.color = v3(1.0f, 0.95f, 0.84f);
        l.intensity = (g->flash_boost ? 0.62f : 0.46f) * (0.92f + 0.08f * sinf((float)w->time * 23.f));
        l.range = g->flash_boost ? 24.f : 19.f;
        l.cos_inner = cosf(DEG2RAD(g->flash_boost ? 9.f : 7.f));
        l.cos_outer = cosf(DEG2RAD(g->flash_boost ? 31.f : 27.f));
        l.shadows = 1;
        add_light_if(g, l);
        // a small fill light around the player so the world is never pitch black
        Light fill;
        memset(&fill, 0, sizeof fill);
        fill.pos = vadd(g->cam.pos, v3(0, 0.2f, 0));
        fill.color = v3(0.55f, 0.62f, 0.80f);
        fill.intensity = 0.11f;
        fill.range = 3.6f;
        fill.cos_inner = -1.f;
        fill.cos_outer = 0.f;
        fill.type = 0;
        add_light_if(g, fill);
    } else {
        Light fill;
        memset(&fill, 0, sizeof fill);
        fill.pos = vadd(g->cam.pos, v3(0, 0.2f, 0));
        fill.color = v3(0.60f, 0.66f, 0.85f);
        fill.intensity = 0.22f;
        fill.range = 5.0f;
        fill.cos_inner = -1.f;
        fill.cos_outer = 0.f;
        add_light_if(g, fill);
    }
    // ---- muzzle flash
    if (g->muzzle_light_t > 0.f) {
        Light l;
        memset(&l, 0, sizeof l);
        l.pos = g->muzzle_light_pos;
        l.color = v3(1.0f, 0.78f, 0.42f);
        l.intensity = g->muzzle_light_intensity * 1.5f;
        l.range = 12.f;
        l.cos_inner = -1.f;
        l.cos_outer = 0.f;
        add_light_if(g, l);
    }
    // ---- nearby static light sources as dynamic lights (characters need them)
    // The building's ceiling fixtures are baked into the static meshes; we add a
    // few real lights near the player so dynamic objects read correctly.
    int added = 0;
    for (int i = 0; i < 6 && added < 8; i++) {
        float gx = -21.f + (float)((i * 3) % 6) * 8.4f;
        float gz = -11.f + (float)((i * 2) % 5) * 6.4f;
        if (w->cfg.power == 0 && w->state != GS_DAY) continue;
        Vec3 lp = v3(gx, 4.0f, gz);
        if (vdist(lp, g->cam.pos) > 17.f) continue;
        Light l;
        memset(&l, 0, sizeof l);
        l.pos = lp;
        l.color = v3(0.95f, 0.97f, 1.0f);
        l.intensity = 0.5f;
        l.range = 7.0f;
        l.cos_inner = -1.f;
        l.cos_outer = 0.f;
        add_light_if(g, l);
        added++;
    }
    // fire barrel outside
    if (vdist(v3(-2.f, 1.2f, -22.f), g->cam.pos) < 22.f) {
        Light l;
        memset(&l, 0, sizeof l);
        l.pos = v3(-2.f, 1.1f, -22.f);
        l.color = v3(1.0f, 0.45f, 0.14f);
        l.intensity = 1.5f + 0.25f * sinf((float)w->time * 11.f);
        l.range = 9.f;
        l.cos_inner = -1.f;
        l.cos_outer = 0.f;
        add_light_if(g, l);
    }
    // exit signs glow
    Light ex;
    memset(&ex, 0, sizeof ex);
    ex.pos = v3(-25.9f, 2.6f, -2.0f);
    ex.color = v3(0.3f, 1.0f, 0.4f);
    ex.intensity = 0.5f;
    ex.range = 4.f;
    ex.cos_inner = -1.f;
    ex.cos_outer = 0.f;
    if (vdist(ex.pos, g->cam.pos) < 18.f) add_light_if(g, ex);
    memset(&ex, 0, sizeof ex);
    ex.pos = v3(25.9f, 2.6f, -2.0f);
    ex.color = v3(0.3f, 1.0f, 0.4f);
    ex.intensity = 0.5f;
    ex.range = 4.f;
    ex.cos_inner = -1.f;
    ex.cos_outer = 0.f;
    if (vdist(ex.pos, g->cam.pos) < 18.f) add_light_if(g, ex);
}

// ---------------------------------------------------------------------------
// character pose drivers
// ---------------------------------------------------------------------------
static const AnimClip *clip_for_state(CharModel *cm, const Zombie *z) {
    switch (z->state) {
        case ZS_DEAD: return cm->clip_death;
        case ZS_ATTACK: return cm->clip_attack;
        case ZS_STAGGER: return cm->clip_hit;
        case ZS_SCREAM: return cm->clip_scream ? cm->clip_scream : cm->clip_attack;
        case ZS_LUNGE: return cm->clip_run;
        case ZS_BARRICADE: return cm->clip_attack;
        case ZS_SPAWNING: return cm->clip_idle;
        default: break;
    }
    if (z->type == ZB_CRAWLER && cm->clip_crawl) return cm->clip_crawl;
    float sp = vlen(v3(z->vel.x, 0, z->vel.z));
    if (sp > 1.9f) return cm->clip_run;
    if (sp > 0.15f) return cm->clip_walk;
    return cm->clip_idle;
}

static void update_zombie_anim(Game *g, int i, float dt) {
    World *w = g->w;
    Zombie *z = &w->zombies[i];
    if (!z->alive) { g->zanim_state[i] = -2; return; }
    CharModel *cm = g->zmodel[z->type][(int)(z->limb_seed * 3.99f) & 3];
    if (!cm || !cm->sk) return;
    if (!g->zanim[i].sk) {
        g->zanim[i].sk = cm->sk;
        g->zanim[i].clip = cm->clip_idle;
        g->zanim[i].next = NULL;
        g->zanim[i].time = (float)i * 0.13f;
        g->zanim[i].blend = 0.f;
        g->zanim[i].speed = 1.f;
        g->zanim_state[i] = -3;
    }
    const AnimClip *want = clip_for_state(cm, z);
    int st = (int)z->state * 4 + (want == cm->clip_idle ? 0 : want == cm->clip_walk ? 1 : want == cm->clip_run ? 2 : 3);
    if (g->zanim_state[i] != st) {
        g->zanim_state[i] = st;
        anim_play(&g->zanim[i], want, 0.22f);
    }
    anim_update(&g->zanim[i], dt * (z->state == ZS_STAGGER ? 1.4f : 1.f));
    anim_sample(&g->zanim[i], &g->zpose[i]);
}

void game_update_char_anims(Game *g, float dt) {
    World *w = g->w;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &w->zombies[i];
        if (!z->alive) continue;
        if (g->zanim[i].sk != g->zmodel[z->type][0]->sk && g->zmodel[z->type][0]) {
            // animator must point at a skeleton of the same topology
        }
        update_zombie_anim(g, i, dt);
    }
    // player rig (third person only shown in multiplayer)
    for (int i = 0; i < w->player_count; i++) {
        Player *p = &w->players[i];
        if (!p->active) continue;
        Animator *an = &g->panim[i];
        CharModel *cm = g->pmodel[i & 3];
        if (an->sk == NULL) {
            an->sk = cm->sk;
            an->clip = cm->clip_idle;
            an->next = NULL;
            an->time = 0.f;
            an->blend = 0.f;
            an->speed = 1.f;
        }
        float sp = vlen(v3(p->vel.x, 0, p->vel.z));
        const AnimClip *want = sp > 5.2f ? cm->clip_run : (sp > 0.3f ? cm->clip_walk : cm->clip_idle);
        if (an->clip != want) anim_play(an, want, 0.18f);
        anim_update(an, dt * (0.7f + sp * 0.16f));
        anim_sample(an, &g->ppose[i]);
    }
}

// ---------------------------------------------------------------------------
static int g_dbg_drawn = 0;
static void draw_characters(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &w->zombies[i];
        if (!z->alive) continue;
        // frustum-ish rejection
        Vec3 d = vsub(z->pos, g->cam.pos);
        if (vlen(d) > 46.f) continue;
        // gameplay-space view direction (camera yaw is offset by pi internally)
        float gyaw = g->cam.yaw - (float)M_PI;
        Vec3 fwd = v3(sinf(gyaw) * cosf(g->cam.pitch), sinf(g->cam.pitch), cosf(gyaw) * cosf(g->cam.pitch));
        if (vdot(vnorm(d), fwd) < -0.45f) continue;
        CharModel *cm = g->zmodel[z->type][(int)(z->limb_seed * 3.99f) & 3];
        Mat4 root = mat4_mul(mat4_translate(z->pos), mat4_rot_y(z->yaw));
        if (z->state == ZS_DEAD) {
            // sink into the floor over time
            float t = saturatef((z->death_time - 4.2f) / 2.3f);
            root = mat4_mul(mat4_translate(v3(z->pos.x, z->pos.y - t * 1.2f, z->pos.z)), mat4_rot_y(z->yaw));
        }
        if (z->state == ZS_SPAWNING) {
            float a = saturatef(z->state_time / 0.75f);
            root = mat4_mul(mat4_translate(v3(z->pos.x, z->pos.y - (1.f - a) * 1.4f, z->pos.z)), mat4_rot_y(z->yaw));
        }
        char_draw(r, cm, g->as->mats, &g->zpose[i], root, z->scale);
        g_dbg_drawn++;
        if (g->show_debug && i == 0) {
            Mat4 bone[MAX_BONES];
            anim_world_mats(cm->sk, &g->zpose[i], bone);
            Mesh *ms = cm->parts[3].mesh;
            Mat4 wm = mat4_mul(root, mat4_mul(bone[cm->parts[3].bone], mat4_scale(v3s(z->scale))));
            Vec3 mn = v3(1e9f, 1e9f, 1e9f), mx = v3(-1e9f, -1e9f, -1e9f);
            for (uint32_t v = 0; v < ms->vert_used; v++) {
                Vec3 vp = mat4_mul_point(g->cam_view, mat4_mul_point(wm, ms->pos[v]));
                mn = vmin3(mn, vp);
                mx = vmax3(mx, vp);
            }
            LOGI("char0 torso view bbox x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f mat=%d tris=%u",
                 mn.x, mx.x, mn.y, mx.y, mn.z, mx.z, ms->mat, ms->tri_used);
            Vec3 sp = mat4_mul_point(g->cam_view, v3(z->pos.x, z->pos.y + 1.0f, z->pos.z));
            LOGI("char0 view pos=(%.2f %.2f %.2f)", sp.x, sp.y, sp.z);
        }
    }
    // other players
    for (int i = 0; i < w->player_count; i++) {
        Player *p = &w->players[i];
        if (!p->active || i == w->local_player) continue;
        CharModel *cm = g->pmodel[i & 3];
        Mat4 root = mat4_mul(mat4_translate(p->pos), mat4_rot_y(p->yaw));
        char_draw(r, cm, g->as->mats, &g->ppose[i], root, 1.0f);
    }
}

static void draw_barricades(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    for (int i = 0; i < MAX_BARRICADES; i++) {
        Barricade *b = &w->barricades[i];
        if (!b->active) continue;
        Model *m = g->barricade_model[clampi(b->level - 1, 0, 2)];
        if (!m) continue;
        float grow = saturatef(b->build_t * 3.2f + 0.15f);
        float shake = b->damage_flash * 0.045f * sinf((float)w->time * 47.f);
        Mat4 xf = mat4_mul(mat4_translate(v3(b->pos.x, b->pos.y - (1.f - grow) * 0.6f, b->pos.z)),
                           mat4_mul(mat4_rot_y(b->yaw + shake), mat4_scale(v3(b->width / 4.0f, grow, 1.f))));
        for (int k = 0; k < m->nparts; k++) {
            if (!m->part[k] || !m->part[k]->tri_used) continue;
            rd_draw_mesh(r, m->part[k], &xf, g->as->mats, MAT_COUNT);
        }
        b->damage_flash = maxf(0.f, b->damage_flash - g->plat->dt * 2.f);
        b->build_t = minf(1.f, b->build_t + g->plat->dt * 1.6f);
    }
    for (int i = 0; i < 8; i++) {
        Trap *t = &w->traps[i];
        if (!t->active) continue;
        Model *m = g->trap_model[clampi(t->kind, 0, 1)];
        if (!m) continue;
        Mat4 xf = mat4_mul(mat4_translate(t->pos), mat4_rot_y(0.f));
        for (int k = 0; k < m->nparts; k++)
            if (m->part[k] && m->part[k]->tri_used) rd_draw_mesh(r, m->part[k], &xf, g->as->mats, MAT_COUNT);
    }
}

static void draw_pickups(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    const Tex *icons = g->as->icons;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &w->pickups[i];
        if (!p->active) continue;
        float bob = sinf(p->bob) * 0.05f;
        Vec3 pos = vadd(p->pos, v3(0, bob + 0.12f, 0));
        // simple glowing item sprite rather than a box
        uint32_t col = COL(255, 255, 255, 255);
        switch (p->kind) {
            case IT_AMMO_PISTOL: case IT_AMMO_SHELL: case IT_AMMO_RIFLE: case IT_AMMO_BOLT:
                col = COL(255, 214, 140, 255); break;
            case IT_MEDKIT: case IT_BANDAGE: col = COL(240, 240, 245, 255); break;
            case IT_PILLS: col = COL(200, 235, 255, 255); break;
            case IT_ADRENALINE: col = COL(180, 255, 190, 255); break;
            case IT_SCRAP: col = COL(200, 205, 210, 255); break;
            case IT_MONEY: col = COL(160, 230, 160, 255); break;
            case IT_BATTERY: col = COL(255, 240, 120, 255); break;
            default: break;
        }
        rd_draw_billboard(r, pos, 0.30f, 0.30f, icons, col, 1, 0.f);
        // faint ground glow decal
        rd_draw_decal(r, v3(p->pos.x, 0.014f, p->pos.z), v3(0, 1, 0), 0.7f, g->as->flash, COL(90, 80, 60, 255));
        if (vdist(p->pos, g->cam.pos) > 34.f) continue;
    }
}

static void draw_particles(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    for (int i = 0; i < w->particle_count; i++) {
        Particle *p = &w->particles[i];
        if (p->life <= 0.f) continue;
        float t = p->life / maxf(p->max_life, 0.001f);
        uint32_t c = p->color;
        // fade alpha with life
        uint32_t a = (c >> 24) & 0xFF;
        a = (uint32_t)((float)a * saturatef(t * 1.4f));
        c = (c & 0x00FFFFFFu) | (a << 24);
        rd_draw_billboard(r, p->pos, p->size, p->size, p->tex, c, p->additive, p->rot);
    }
    // decals
    for (int i = 0; i < MAX_DECALS; i++) {
        Decal *d = &w->decals[i];
        if (!d->active) continue;
        uint32_t c = d->color;
        uint32_t a = (c >> 24) & 0xFF;
        float fade = 1.f - saturatef((d->life - 40.f) / 15.f);
        a = (uint32_t)((float)a * fade);
        c = (c & 0x00FFFFFFu) | (a << 24);
        rd_draw_decal(r, d->pos, d->nrm, d->size, d->tex, c);
    }
}

static void draw_viewmodel(Game *g) {
    // the aimed pose would put the optic in the player's face, so the model is
    // lowered away once the sights are up (the reticle takes over)
    (void)0;
    World *w = g->w;
    Renderer *r = g->rd;
    Player *p = &w->players[w->local_player];
    if (!p->alive) return;
    int wi = p->weapon;
    Model *m = g->wm.view[wi];
    if (!m) return;
    if (p->aim_t > 0.62f && (wi == WP_PISTOL || wi == WP_RIFLE || wi == WP_SNIPER)) return;
    if (w->state == GS_MAIN_MENU || w->state == GS_LOADING) return;
    Vec3 muzzle, dir;
    Mat4 xf = weapon_view_xform(p, &g->wm, g->cam.pos, &muzzle, &dir, (float)w->time);
    g->vm_muzzle = muzzle;
    g->vm_dir = dir;
    // camera-relative projection of the flash: place the flash slightly forward
    for (int k = 0; k < m->nparts; k++) {
        Mesh *ms = m->part[k];
        if (!ms || !ms->tri_used) continue;
        rd_draw_mesh(r, ms, &xf, g->as->mats, MAT_COUNT);
    }
    if (g->show_debug) {
        Mesh *ms = m->part[0];
        Vec3 mn = v3(1e9f, 1e9f, 1e9f), mx = v3(-1e9f, -1e9f, -1e9f);
        for (uint32_t v = 0; v < ms->vert_used; v++) {
            Vec3 wp = mat4_mul_point(xf, ms->pos[v]);
            Vec3 vp = mat4_mul_point(r->view, wp);
            mn = vmin3(mn, vp);
            mx = vmax3(mx, vp);
        }
        LOGI("vm: view x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f", mn.x, mx.x, mn.y, mx.y, mn.z, mx.z);
    }
}

void scene_draw(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    Player *p = &w->players[w->local_player];
    // pick the internal resolution from the quality setting
    int target_w = 640, target_h = 360;
    if (w->quality == 0) { target_w = 480; target_h = 270; }
    else if (w->quality == 1) { target_w = 800; target_h = 450; }
    else { target_w = 1024; target_h = 576; }
    if (w->render_scale == 0) { target_w = target_w * 3 / 4; target_h = target_h * 3 / 4; }
    if (r->w != target_w || r->h != target_h) rd_resize(r, target_w, target_h);

    setup_env(g);
    Camera cam = g->cam;
    if (w->state == GS_MAIN_MENU || w->state == GS_CONTROLS || w->state == GS_LOADING) {
        // slow cinematic drift through the store for the menus
        float t = g->ui_time * 0.08f;
        cam.pos = v3(sinf(t) * 8.f, 2.15f + sinf(t * 1.7f) * 0.18f, 6.f + cosf(t) * 9.f);
        cam.yaw = t + 2.4f;
        cam.pitch = -0.06f;
        cam.fov = DEG2RAD(66.f);
    }
    rd_begin(r, &cam, &g->env);
    g->cam_view = r->view;
    setup_lights(g);
    // static world
    Mat4 ident = mat4_identity();
    for (int i = 0; i < w->level.mesh_count; i++) {
        Mesh *m = w->level.meshes[i];
        if (!m || !m->tri_used) continue;
        rd_draw_mesh(r, m, &ident, g->as->mats, MAT_COUNT);
    }
    draw_barricades(g);
    draw_characters(g);
    draw_pickups(g);
    draw_particles(g);
    if (w->state != GS_MAIN_MENU && w->state != GS_LOADING && p->alive) draw_viewmodel(g);
    rd_lighting(r);
    rd_post(r);
    static int frame = 0;
    if (frame == 0) {
        CharModel *cm = g->zmodel[ZB_WALKER][0];
        LOGI("dbg char parts=%d tri0=%u tri3=%u sk=%d bones=%d", cm->nparts,
             cm->parts[0].mesh ? cm->parts[0].mesh->tri_used : 0,
             cm->parts[3].mesh ? cm->parts[3].mesh->tri_used : 0, cm->sk != NULL, cm->sk ? cm->sk->count : 0);
        Model *wm0 = g->wm.view[WP_PISTOL];
        LOGI("dbg weapon parts=%d tri0=%u", wm0->nparts, wm0->part[0]->tri_used);
        Vec3 mz, dr;
        Mat4 xf = weapon_view_xform(p, &g->wm, g->cam.pos, &mz, &dr, 0.f);
        Vec3 vp = mat4_mul_point(g->rd->view, mz);
        LOGI("dbg muzzle world=(%.2f,%.2f,%.2f) view=(%.2f,%.2f,%.2f) cam=(%.2f,%.2f,%.2f)",
             mz.x, mz.y, mz.z, vp.x, vp.y, vp.z, g->cam.pos.x, g->cam.pos.y, g->cam.pos.z);
        Vec3 zp = mat4_mul_point(g->rd->view, v3(-1.39f, 0.9f, 7.1f));
        LOGI("dbg zombie view=(%.2f,%.2f,%.2f) fov=%.1f", zp.x, zp.y, zp.z, RAD2DEG(g->cam.fov));
    }
    if ((frame++ % 120) == 0)
        LOGI("scene: chars=%d zombies=%d pos=(%.1f,%.1f) yaw=%.2f", g_dbg_drawn, w->zombie_count,
             p->pos.x, p->pos.z, p->yaw);
    g_dbg_drawn = 0;
}
