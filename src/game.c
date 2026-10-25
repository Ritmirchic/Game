// Game lifecycle, state machine, camera, day/night progression, shop, saves.
#include "game.h"
#include <string.h>
#include <stdio.h>

void sounds_build(Assets *a);
void sounds_free(Assets *a);

static const char *g_tips[] = {
    "Board the windows before the sun goes down. Every opening is a way in.",
    "Zombies break wood quickly. Reinforced barricades buy you minutes, not seconds.",
    "Scrap is the only currency that matters after dark. Search the aisles.",
    "A crawler can slip under a barricade you built for a walker.",
    "Screamers call the whole lot down on you. Kill them first.",
    "Your flashlight is not infinite. Batteries are in the electronics aisle.",
    "The Brute does not stagger. Do not stand in front of it with a bat.",
    "The loading dock doors are the widest opening in the building.",
    "Aim down sights to tighten your spread. A pistol headshot still kills.",
    "If a friend goes down, get to them and hold E. Nobody survives alone.",
};

static void reset_world(Game *g) {
    World *w = g->w;
    // NOTE: the World owns the built Level and the Audio mixer, so only the
    // gameplay state is reset here — never a blanket memset.
    w->state = GS_MAIN_MENU;
    w->state_time = 0.f;
    w->time = 0.f;
    w->night = 1;
    w->time_scale = 1.f;
    w->local_player = 0;
    w->player_count = 1;
    w->paused_from = GS_DAY;
    w->zombie_count = 0;
    w->particle_count = 0;
    memset(w->particles, 0, sizeof w->particles);
    memset(w->decals, 0, sizeof w->decals);
    memset(w->projectiles, 0, sizeof w->projectiles);
    memset(w->pickups, 0, sizeof w->pickups);
    memset(w->barricades, 0, sizeof w->barricades);
    g->flash_boost = 0;
    for (int i = 0; i < w->level.slot_count; i++) w->level.slots[i].barricade = -1;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        memset(&w->zombies[i], 0, sizeof(Zombie));
        g->zanim_state[i] = -2;
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        memset(&w->players[i], 0, sizeof(Player));
        w->players[i].active = 0;
    }
    player_spawn(g, &w->players[0], 0);
    w->players[0].active = 1;
    g->rain_amount = 0.f;
}

void options_defaults(Game *g) {
    World *w = g->w;
    w->mouse_sensitivity = 5;
    w->invert_y = 0;
    w->quality = 1;
    w->render_scale = 1;
    w->ui_scale = 1;
    w->difficulty = 1;
}

void game_init(Game *g, Platform *plat, Renderer *rd, Assets *as) {
    memset(g, 0, sizeof *g);
    g->plat = plat;
    g->rd = rd;
    g->as = as;
    g->w = (World *)calloc(1, sizeof(World));
    World *w = g->w;
    w->assets = as;
    aud_init(&w->audio);
    LOGI("game: building level...");
    level_build(as, &w->level);
    LOGI("game: level built");
    weapons_build(&g->wm);
    for (int i = 0; i < 3; i++) g->barricade_model[i] = prop_barricade(i + 1);
    for (int i = 0; i < 2; i++) g->trap_model[i] = prop_trap(i);
    LOGI("game: barricade + trap models built");
    LOGI("game: weapon models built");
    for (int t = 0; t < ZB_TYPE_COUNT; t++)
        for (int v = 0; v < 4; v++) g->zmodel[t][v] = char_zombie(t, v, as);
    for (int v = 0; v < 4; v++) g->pmodel[v] = char_player(v, as);
    LOGI("game: character models built");
    sounds_build(as);
    LOGI("game: audio synthesized");
    reset_world(g);
    options_defaults(g);
    g->env.ambient_sky = v3(0.16f, 0.20f, 0.30f);
    g->env.ambient_ground = v3(0.10f, 0.09f, 0.08f);
    g->env.ambient_intensity = 0.55f;
    g->env.fog_color = v3(0.09f, 0.12f, 0.16f);
    g->env.fog_density = 0.018f;
    g->env.exposure = 1.02f;
    g->env.bloom = 0.30f;
    g->env.vignette = 0.55f;
    g->env.grain = 0.05f;
    g->env.saturation = 0.92f;
    g->env.grade_shadow = v3(0.72f, 0.86f, 1.10f);
    g->env.grade_high = v3(1.08f, 1.02f, 0.92f);
    g->env.sky_moon_intensity = 0.9f;
    g->env.sky_tint = v3(0.10f, 0.14f, 0.22f);
    g->env.time = 0.f;
    aud_set_listener(&w->audio, v3(0, 1.6f, 0), v3(0, 0, 1), v3(0, 1, 0));
    save_read(g);
    LOGI("game: init complete");
}

void game_shutdown(Game *g) {
    if (!g->w) return;
    aud_shutdown(&g->w->audio);
    sounds_free(g->as);
    for (int t = 0; t < ZB_TYPE_COUNT; t++)
        for (int v = 0; v < 4; v++) char_free(g->zmodel[t][v]);
    for (int v = 0; v < 4; v++) char_free(g->pmodel[v]);
    weapons_free(&g->wm);
    for (int i = 0; i < 3; i++) model_free(g->barricade_model[i]);
    for (int i = 0; i < 2; i++) model_free(g->trap_model[i]);
    for (int i = 0; i < 12; i++) model_free(g->pickup_model[i]);
    level_free(&g->w->level);
    free(g->w);
    g->w = NULL;
}

void game_new_run(Game *g) {
    reset_world(g);
    g->w->state = GS_LOADING;
    g->w->state_time = 0.f;
    g->load_t = 0.f;
    g->pending_night = 1;
    g->night_cleared = 0;
    g->rain_amount = 0.f;
    g->w->money = 0;
    g->w->kills = 0;
    g->w->headshots = 0;
    g->w->deaths = 0;
}

void game_start_day(Game *g, int night) {
    World *w = g->w;
    w->night = night;
    w->cfg = *night_config(night);
    w->state = GS_DAY;
    w->state_time = 0.f;
    w->night_phase = 0;
    w->night_timer = w->cfg.prep_time;
    w->money = maxi(w->money, w->cfg.start_money);
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        if (w->zombies[i].alive && w->zombies[i].state != ZS_DEAD) {
            w->zombies[i].alive = 0;
        }
    }
    w->zombie_count = 0;
    w->boss_active = 0;
    game_scatter_loot(g, night);
    // keep the barricades the player built, but restore some hp overnight
    for (int i = 0; i < MAX_BARRICADES; i++) {
        Barricade *b = &w->barricades[i];
        if (!b->active) continue;
        b->hp = minf(b->max_hp, b->hp + b->max_hp * 0.35f);
    }
    snprintf(w->objective, sizeof w->objective, "DAY %d  -  PREPARE", night);
    snprintf(w->subtitle, sizeof w->subtitle, "Sun's up. Search the store, then start the night at the supply desk.");
    w->subtitle_t = 7.f;
    g->rain_amount = 0.f;
    nav_update_flow(&w->level.nav, w->level.counter_pos);
}

// ---------------------------------------------------------------------------
// Shop
// ---------------------------------------------------------------------------
typedef struct ShopItem {
    const char *name;
    int price;
    int kind;    // 0 = weapon, 1 = ammo, 2 = consumable, 3 = upgrade
    int arg;
    const char *desc;
} ShopItem;

static const ShopItem g_shop[] = {
    {"AXE", 260, 0, WP_AXE, "Heavy melee. Cracks a brute's leg; keeps its edge."},
    {"SHOTGUN", 620, 0, WP_SHOTGUN, "8 pellets, close range. Devastating at a window."},
    {"ASSAULT RIFLE", 1400, 0, WP_RIFLE, "30 rounds, full auto, red dot. The all-round answer."},
    {"CROSSBOW", 980, 0, WP_CROSSBOW, "Silent, retrievable bolts, hits hard. Slow to reload."},
    {"HUNTING RIFLE", 1750, 0, WP_SNIPER, "Scoped. Punches through a line of walkers."},
    {"9MM AMMO  x36", 90, 1, AMMO_PISTOL, "Pistol rounds."},
    {"12GA SHELLS x12", 150, 1, AMMO_SHELL, "Shotgun shells."},
    {"5.56 AMMO x60", 220, 1, AMMO_RIFLE, "Rifle ammunition."},
    {"BOLTS x8", 120, 1, AMMO_BOLT, "Crossbow bolts."},
    {"MEDKIT x2", 180, 2, IT_MEDKIT, "Two field medkits, use with H."},
    {"SCRAP x4", 140, 2, IT_SCRAP, "Building material for barricades."},
    {"BATTERY", 70, 2, IT_BATTERY, "Refills your flashlight."},
    {"BODY ARMOUR +25%", 700, 3, 25, "Reduces incoming damage permanently."},
    {"STAMINA TRAINING", 500, 3, 10, "Increases sprint duration."},
    {"FLASHLIGHT LENS", 380, 3, 1, "Brighter, wider flashlight beam."},
};
#define SHOP_COUNT ((int)(sizeof g_shop / sizeof g_shop[0]))

int game_shop_count(void) { return SHOP_COUNT; }
const char *game_shop_name(int index) {
    if (index < 0 || index >= SHOP_COUNT) return "";
    return g_shop[index].name;
}
const char *game_shop_desc(int index) {
    if (index < 0 || index >= SHOP_COUNT) return "";
    return g_shop[index].desc;
}

const char *game_shop_label(int index, Game *g, char *buf, int n) {
    if (index < 0 || index >= SHOP_COUNT) return buf;
    const ShopItem *it = &g_shop[index];
    Player *p = &g->w->players[g->w->local_player];
    if (it->kind == 0) {
        if (p->owned[it->arg]) snprintf(buf, n, "OWNED");
        else snprintf(buf, n, "$%d", it->price);
    } else if (it->kind == 3) {
        if (it->arg == 25 && p->armor >= 0.5f) snprintf(buf, n, "MAX");
        else if (it->arg == 10 && p->max_stamina >= 140.f) snprintf(buf, n, "MAX");
        else if (it->arg == 1 && g->flash_boost) snprintf(buf, n, "OWNED");
        else snprintf(buf, n, "$%d", it->price);
    } else {
        snprintf(buf, n, "$%d", it->price);
    }
    return buf;
}

void game_shop_buy(Game *g, int index) {
    World *w = g->w;
    Player *p = &w->players[w->local_player];
    if (index < 0 || index >= SHOP_COUNT) return;
    const ShopItem *it = &g_shop[index];
    if (p->money < it->price) {
        snprintf(w->subtitle, sizeof w->subtitle, "Not enough cash.");
        w->subtitle_t = 2.f;
        sfx(g, g->as->snd_ui_back, NULL, 0.7f, 1.f, 3);
        return;
    }
    int ok = 1;
    switch (it->kind) {
        case 0:
            if (p->owned[it->arg]) ok = 0;
            else {
                p->owned[it->arg] = 1;
                p->weapon = it->arg;
                p->switch_t = 0.28f;
                if (it->arg >= WP_PISTOL) p->mag[it->arg] = g_weapon_defs[it->arg].mag_size;
            }
            break;
        case 1: {
            int type = it->arg;
            int add = type == AMMO_PISTOL ? 36 : (type == AMMO_SHELL ? 12 : (type == AMMO_RIFLE ? 60 : 8));
            p->ammo[type] = mini(ammo_max(type), p->ammo[type] + add);
            break;
        }
        case 2:
            if (it->arg == IT_MEDKIT) p->heal_charges += 2;
            else if (it->arg == IT_SCRAP) p->scrap += 4;
            else if (it->arg == IT_BATTERY) p->flashlight_charge = 100.f;
            break;
        case 3:
            if (it->arg == 25) {
                if (p->armor >= 0.5f) ok = 0;
                else p->armor = minf(0.5f, p->armor + 0.25f);
            } else if (it->arg == 10) {
                if (p->max_stamina >= 140.f) ok = 0;
                else { p->max_stamina += 15.f; p->stamina = p->max_stamina; }
            } else if (it->arg == 1) {
                if (g->flash_boost) ok = 0;
                else g->flash_boost = 1;
            }
            break;
    }
    if (!ok) {
        snprintf(w->subtitle, sizeof w->subtitle, "Already owned.");
        w->subtitle_t = 2.f;
        sfx(g, g->as->snd_ui_back, NULL, 0.7f, 1.f, 3);
        return;
    }
    p->money -= it->price;
    sfx(g, g->as->snd_ui_select, NULL, 0.9f, 1.f, 3);
    snprintf(w->subtitle, sizeof w->subtitle, "Purchased %s.", it->name);
    w->subtitle_t = 2.f;
}

// ---------------------------------------------------------------------------
// Save / options
// ---------------------------------------------------------------------------
static void save_path(Game *g, char *out, size_t n) {
    const char *dir = g->plat->save_dir[0] ? g->plat->save_dir : ".";
    snprintf(out, n, "%s/the_last_night.sav", dir);
}
void save_write(Game *g) {
    char path[600];
    save_path(g, path, sizeof path);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    World *w = g->w;
    struct {
        char magic[8];
        int version;
        int night_cleared;
        int money;
        int kills, headshots, deaths;
        int sens, invert, quality, ui_scale, difficulty;
        float armor, max_stamina;
        int flash_boost;
    } hdr;
    memcpy(hdr.magic, "TLNSAVE", 8);
    hdr.version = 1;
    hdr.night_cleared = g->night_cleared;
    hdr.money = w->players[0].money;
    hdr.kills = w->kills;
    hdr.headshots = w->headshots;
    hdr.deaths = w->deaths;
    hdr.sens = w->mouse_sensitivity;
    hdr.invert = w->invert_y;
    hdr.quality = w->quality;
    hdr.ui_scale = w->ui_scale;
    hdr.difficulty = w->difficulty;
    hdr.armor = w->players[0].armor;
    hdr.max_stamina = w->players[0].max_stamina;
    hdr.flash_boost = g->flash_boost;
    fwrite(&hdr, sizeof hdr, 1, f);
    int owned[WP_COUNT];
    memcpy(owned, w->players[0].owned, sizeof owned);
    fwrite(owned, sizeof owned, 1, f);
    fclose(f);
    LOGI("save written (%d nights cleared)", g->night_cleared);
}
int save_read(Game *g) {
    char path[600];
    save_path(g, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    struct {
        char magic[8];
        int version;
        int night_cleared;
        int money;
        int kills, headshots, deaths;
        int sens, invert, quality, ui_scale, difficulty;
        float armor, max_stamina;
        int flash_boost;
    } hdr;
    if (fread(&hdr, sizeof hdr, 1, f) != 1 || memcmp(hdr.magic, "TLNSAVE", 8)) {
        fclose(f);
        return 0;
    }
    World *w = g->w;
    g->night_cleared = hdr.night_cleared;
    w->players[0].money = hdr.money;
    w->kills = hdr.kills;
    w->headshots = hdr.headshots;
    w->deaths = hdr.deaths;
    w->mouse_sensitivity = hdr.sens;
    w->invert_y = hdr.invert;
    w->quality = hdr.quality;
    w->ui_scale = hdr.ui_scale;
    w->difficulty = hdr.difficulty;
    w->players[0].armor = hdr.armor;
    if (hdr.max_stamina > 100.f) w->players[0].max_stamina = hdr.max_stamina;
    g->flash_boost = hdr.flash_boost;
    int owned[WP_COUNT];
    if (fread(owned, sizeof owned, 1, f) == 1) {
        for (int i = 0; i < WP_COUNT; i++) w->players[0].owned[i] = owned[i];
    }
    fclose(f);
    LOGI("save loaded: night %d cleared", g->night_cleared);
    return 1;
}

// ---------------------------------------------------------------------------
// Camera & environment
// ---------------------------------------------------------------------------
static void update_camera(Game *g) {
    World *w = g->w;
    Player *p = &w->players[w->local_player];
    Camera *c = &g->cam;
    float eye = 1.62f;
    if (!p->alive) eye = 0.55f;
    Vec3 pos = vadd(p->pos, v3(0, eye, 0));
    // bob
    float sp = vlen(v3(p->vel.x, 0, p->vel.z));
    float bobamt = saturatef(sp / 6.f) * (1.f - p->aim_t * 0.8f);
    pos.y += sinf(p->bob * 4.f) * 0.032f * bobamt;
    pos.x += sinf(p->bob * 2.f) * 0.022f * bobamt;
    // land kick
    pos.y -= p->land_kick * 0.18f;
    // recoil
    float pitch = p->pitch + p->recoil_pitch;
    float yaw = p->yaw + p->recoil_yaw;
    // shake
    float sh = p->shake + g->shake_amt;
    if (sh > 0.001f) {
        float t = (float)w->time * 34.f;
        pos.x += sinf(t * 1.7f) * sh * 0.055f;
        pos.y += sinf(t * 2.3f + 1.3f) * sh * 0.045f;
        pitch += sinf(t * 2.9f + 0.7f) * sh * 0.012f;
        yaw += sinf(t * 1.9f + 2.1f) * sh * 0.012f;
    }
    c->pos = pos;
    // the renderer's view matrix looks down -Z at yaw 0; the gameplay forward
    // vector is +Z at yaw 0, so the camera yaw is offset by pi.
    c->yaw = yaw + (float)M_PI;
    c->pitch = pitch;
    c->roll = sinf(p->bob * 2.f) * 0.006f * bobamt - p->sway_x * 0.6f;
    c->fov = DEG2RAD(74.f - p->aim_t * (p->weapon == WP_SNIPER ? 40.f : 18.f));
    aud_set_listener(&w->audio, pos, v3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)), v3(0, 1, 0));
}

static void update_environment(Game *g) {
    World *w = g->w;
    Environment *e = &g->env;
    float dark = 0.35f;
    if (w->state == GS_DAY) dark = 0.0f;
    else {
        const NightConfig *c = &w->cfg;
        dark = 0.25f + (float)c->darkness * 0.22f;
        if (c->power == 0) dark += 0.12f;
    }
    float t = (float)w->time;
    e->time = t;
    e->fog_density = 0.012f + dark * 0.030f + g->rain_amount * 0.02f;
    e->ambient_intensity = 0.42f - dark * 0.14f;
    e->sky_moon_intensity = 0.5f + dark * 0.5f;
    e->exposure = 0.98f + dark * 0.18f;
    e->vignette = 0.42f + dark * 0.32f;
    e->rain = g->rain_amount;
    if (w->state == GS_DAY) {
        e->ambient_sky = v3(0.62f, 0.68f, 0.78f);
        e->ambient_ground = v3(0.34f, 0.32f, 0.28f);
        e->fog_color = v3(0.55f, 0.58f, 0.62f);
        e->sky_tint = v3(0.55f, 0.62f, 0.72f);
        e->saturation = 1.02f;
    } else {
        e->ambient_sky = v3(0.10f, 0.13f, 0.20f);
        e->ambient_ground = v3(0.07f, 0.07f, 0.08f);
        e->fog_color = v3(0.06f, 0.08f, 0.12f);
        e->sky_tint = v3(0.05f, 0.07f, 0.13f);
        e->saturation = 0.82f;
    }
    // lightning flashes during storms
    if (g->rain_amount > 0.4f) {
        g->lightning_t -= g->plat->dt;
        if (g->lightning_t <= 0.f) {
            g->lightning_t = rng_range(&w->rng, 7.f, 22.f);
            g->lightning = 1.f;
            g->thunder_delay = rng_range(&w->rng, 0.5f, 2.2f);
        }
    }
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------
static void menu_begin(Game *g, const char *a, const char *b, const char *c, const char *d, const char *e, const char *f) {
    const char *items[6] = {a, b, c, d, e, f};
    g->menu_count = 0;
    for (int i = 0; i < 6 && items[i]; i++) {
        snprintf(g->menu[i].label, sizeof g->menu[i].label, "%s", items[i]);
        g->menu[i].enabled = 1;
        g->menu_count++;
    }
    if (g->menu_sel >= g->menu_count) g->menu_sel = 0;
}

static void menu_activate(Game *g, int idx);

void game_frame(Game *g, float dt) {
    World *w = g->w;
    Input *in = &g->plat->in;
    w->time += dt;
    w->state_time += dt;
    g->ui_time += dt;
    g->shake_amt = maxf(0.f, g->shake_amt - dt * 2.2f);
    g->flash = maxf(0.f, g->flash - dt * 3.f);
    g->shot_flash = maxf(0.f, g->shot_flash - dt * 12.f);
    g->muzzle_light_t = maxf(0.f, g->muzzle_light_t - dt);
    g->hit_flash = maxf(0.f, g->hit_flash - dt * 3.f);
    g->toast_t = maxf(0.f, g->toast_t - dt);
    g->lightning = maxf(0.f, g->lightning - dt * 2.6f);
    w->damage_flash = maxf(0.f, w->damage_flash - dt * 1.6f);
    w->heal_flash = maxf(0.f, w->heal_flash - dt * 1.2f);
    w->hitmarker = maxf(0.f, w->hitmarker - dt * 3.f);
    w->kill_notify_t = maxf(0.f, w->kill_notify_t - dt);
    w->subtitle_t = maxf(0.f, w->subtitle_t - dt);
    w->amb_timer -= dt;
    update_environment(g);

    // ambient audio
    if (w->state == GS_DAY || w->state == GS_NIGHT || w->state == GS_NIGHT_INTRO) {
        if (g->rain_amount > 0.05f && w->amb_timer <= 0.f) {
            w->amb_timer = 7.f;
            aud_play(&w->audio, g->as->snd_rain, NULL, 0.22f * g->rain_amount, 1.f, 1, 2);
        }
        if (w->cfg.power == 0 && w->amb_timer <= 0.f) {
            w->amb_timer = 6.f;
            aud_play(&w->audio, g->as->snd_hum, NULL, 0.10f, 0.5f, 0, 2);
        }
        if (g->thunder_delay > 0.f) {
            g->thunder_delay -= dt;
            if (g->thunder_delay <= 0.f) {
                aud_play(&w->audio, g->as->snd_thunder[(int)(rng_f(&w->rng) * 2.99f)], NULL, 0.55f, 1.f, 0, 2);
            }
        }
    }

    switch (w->state) {
        case GS_BOOT:
            w->state = GS_MAIN_MENU;
            break;
        case GS_LOADING:
            g->load_t += dt;
            if (g->load_t > 2.6f) {
                game_start_day(g, g->pending_night);
            }
            break;
        case GS_MAIN_MENU: {
            menu_begin(g, "CONTINUE", "NEW GAME", "MULTIPLAYER", "SETTINGS", "HOW TO PLAY", "QUIT");
            if (in->pressed[K_UP]) { g->menu_sel = (g->menu_sel + g->menu_count - 1) % g->menu_count; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_DOWN]) { g->menu_sel = (g->menu_sel + 1) % g->menu_count; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_ENTER] || in->mouse_pressed[0]) {
                if (in->mouse_pressed[0]) {
                    // mouse hit-testing against the menu rectangles
                    float cx = g->rd->w * 0.5f;
                    float y0 = g->rd->h * 0.44f;
                    for (int i = 0; i < g->menu_count; i++) {
                        float by = y0 + (float)i * (26.f * (float)w->ui_scale + 16.f);
                        float bh = 30.f * (float)w->ui_scale;
                        if (in->mouse_x > cx - 190.f && in->mouse_x < cx + 190.f &&
                            in->mouse_y > by && in->mouse_y < by + bh) {
                            g->menu_sel = i;
                            menu_activate(g, i);
                            break;
                        }
                    }
                } else {
                    menu_activate(g, g->menu_sel);
                }
            }
            if (in->pressed[K_ESC]) g->plat->quit_requested = 1;
            break;
        }
        case GS_DAY: {
            Player *p = &w->players[w->local_player];
            for (int i = 0; i < w->player_count; i++)
                if (w->players[i].active) player_update(g, &w->players[i], dt);
            ai_update(g, dt);
            game_update_pickups(g, dt);
            game_update_projectiles(g, dt);
            fx_update(g, dt);
            w->night_timer -= dt;
            if (in->pressed[K_ESC]) { w->paused_from = GS_DAY; w->state = GS_PAUSE; w->state_time = 0.f; }
            if (in->pressed['T'] && !w->net.running) {
                // debug: skip to night (also reachable from the supply desk)
                g->pending_night = w->night;
                w->state = GS_LOADING;
                g->load_t = 0.f;
            }
            if (w->night_timer <= 0.f) {
                snprintf(w->subtitle, sizeof w->subtitle, "The sun is down. They are coming.");
                w->subtitle_t = 4.f;
                wave_start_night(g, w->night);
            }
            if (p->hp < p->max_hp * 0.25f && !g->low_health_warned) {
                g->low_health_warned = 1;
                snprintf(w->subtitle, sizeof w->subtitle, "You are badly hurt. Find a medkit.");
                w->subtitle_t = 3.f;
            }
            break;
        }
        case GS_NIGHT_INTRO:
        case GS_NIGHT: {
            for (int i = 0; i < w->player_count; i++)
                if (w->players[i].active) player_update(g, &w->players[i], dt);
            ai_update(g, dt);
            wave_update(g, dt);
            game_update_pickups(g, dt);
            game_update_projectiles(g, dt);
            fx_update(g, dt);
            if (in->pressed[K_ESC]) { w->paused_from = w->state; w->state = GS_PAUSE; w->state_time = 0.f; }
            // all players dead
            int alive = 0;
            for (int i = 0; i < w->player_count; i++)
                if (w->players[i].active && w->players[i].alive) alive++;
            if (!alive) {
                if (w->state == GS_NIGHT || w->state == GS_NIGHT_INTRO) {
                    w->state = GS_GAMEOVER;
                    w->state_time = 0.f;
                    aud_play(&w->audio, g->as->snd_stinger[0], NULL, 1.f, 0.7f, 0, 2);
                }
            }
            break;
        }
        case GS_NIGHT_END:
            for (int i = 0; i < w->player_count; i++)
                if (w->players[i].active) player_update(g, &w->players[i], dt);
            ai_update(g, dt);
            fx_update(g, dt);
            game_update_pickups(g, dt);
            if (w->state_time > 5.5f) {
                if (w->night >= 10) {
                    w->state = GS_VICTORY;
                } else {
                    game_start_day(g, w->night + 1);
                    w->state = GS_LOADING;
                    g->pending_night = w->night;
                    g->load_t = 0.f;
                }
                w->state_time = 0.f;
            }
            break;
        case GS_SHOP: {
            int n = game_shop_count();
            if (in->pressed[K_UP]) { g->shop_sel = (g->shop_sel + n - 1) % n; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_DOWN]) { g->shop_sel = (g->shop_sel + 1) % n; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_ENTER]) game_shop_buy(g, g->shop_sel);
            if (in->pressed[K_ESC] || in->pressed['E']) { w->state = GS_DAY; w->state_time = 0.f; }
            fx_update(g, dt);
            break;
        }
        case GS_PAUSE: {
            menu_begin(g, "RESUME", "SETTINGS", "RESTART NIGHT", "QUIT TO MENU", "QUIT GAME", NULL);
            if (in->pressed[K_UP]) { g->menu_sel = (g->menu_sel + g->menu_count - 1) % g->menu_count; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_DOWN]) { g->menu_sel = (g->menu_sel + 1) % g->menu_count; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_ESC]) { w->state = w->paused_from; w->state_time = 0.f; }
            if (in->pressed[K_ENTER]) menu_activate(g, g->menu_sel);
            break;
        }
        case GS_LOBBY: {
            menu_begin(g, "HOST LAN GAME", "JOIN 127.0.0.1", "BACK", NULL, NULL, NULL);
            if (!w->net.running && w->state_time > 6.f) w->state_time = 6.f;
            if (in->pressed[K_UP]) { g->menu_sel = (g->menu_sel + g->menu_count - 1) % g->menu_count; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_DOWN]) { g->menu_sel = (g->menu_sel + 1) % g->menu_count; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_ENTER]) {
                if (g->menu_sel == 0) {
                    if (w->net.running) net_stop(g);
                    game_new_run(g);
                    net_start(g, 1, NULL);
                    w->state = GS_LOADING;
                    g->load_t = 0.f;
                } else if (g->menu_sel == 1) {
                    if (w->net.running) net_stop(g);
                    game_new_run(g);
                    net_start(g, 2, "127.0.0.1");
                    w->state = GS_NIGHT;
                    w->player_count = 1;
                    snprintf(w->subtitle, sizeof w->subtitle, "Waiting for the host...");
                    w->subtitle_t = 6.f;
                } else {
                    if (w->net.running) net_stop(g);
                    w->state = GS_MAIN_MENU;
                }
            }
            if (in->pressed[K_ESC]) {
                if (w->net.running) net_stop(g);
                w->state = GS_MAIN_MENU;
            }
            // once the host sends a snapshot, the client is live
            if (w->net.mode == 2 && w->net.running && w->state == GS_NIGHT && w->zombie_count > 0) {
                /* snapshots are applied by net_update */
            }
            break;
        }
        case GS_SETTINGS: {
            if (in->pressed[K_UP]) { g->settings_sel = (g->settings_sel + 7) % 8; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            if (in->pressed[K_DOWN]) { g->settings_sel = (g->settings_sel + 1) % 8; sfx(g, g->as->snd_ui_move, NULL, 0.5f, 1.f, 3); }
            int dir = 0;
            if (in->pressed[K_LEFT]) dir = -1;
            if (in->pressed[K_RIGHT]) dir = 1;
            if (dir) {
                switch (g->settings_sel) {
                    case 0: w->mouse_sensitivity = clampi(w->mouse_sensitivity + dir, 1, 10); break;
                    case 1: w->invert_y = !w->invert_y; break;
                    case 2: w->quality = clampi(w->quality + dir, 0, 2); break;
                    case 3: w->render_scale = clampi(w->render_scale + dir, 0, 2); break;
                    case 4: w->ui_scale = clampi(w->ui_scale + dir, 0, 2); break;
                    case 5: w->difficulty = clampi(w->difficulty + dir, 0, 2); break;
                    case 6: g->plat->vsync = !g->plat->vsync; break;
                    case 7: g->plat->fullscreen = !g->plat->fullscreen; plat_toggle_fullscreen(g->plat); break;
                }
                sfx(g, g->as->snd_ui_move, NULL, 0.6f, 1.2f, 3);
            }
            if (in->pressed[K_ESC] || in->pressed[K_ENTER]) {
                if (g->settings_from == GS_MAIN_MENU) {
                    w->state = GS_MAIN_MENU;
                    w->state_time = 0.f;
                } else {
                    w->state = GS_PAUSE;
                    w->paused_from = GS_DAY;
                }
                save_write(g);
            }
            break;
        }
        case GS_CONTROLS:
            if (in->pressed[K_ESC] || in->pressed[K_ENTER]) w->state = GS_MAIN_MENU;
            break;
        case GS_GAMEOVER: {
            menu_begin(g, "RETRY NIGHT", "QUIT TO MENU", NULL, NULL, NULL, NULL);
            if (in->pressed[K_UP]) g->menu_sel = (g->menu_sel + 1) % 2;
            if (in->pressed[K_DOWN]) g->menu_sel = (g->menu_sel + 1) % 2;
            if (in->pressed[K_ENTER]) menu_activate(g, g->menu_sel);
            break;
        }
        case GS_VICTORY:
            if (in->pressed[K_ENTER] || in->pressed[K_ESC]) {
                w->state = GS_MAIN_MENU;
                w->state_time = 0.f;
            }
            break;
        default:
            break;
    }
    if (g->menu_count == 0) g->menu_sel = 0;
    update_camera(g);
}

static void menu_activate(Game *g, int idx) {
    World *w = g->w;
    Input *in = &g->plat->in;
    (void)in;
    switch (w->state) {
        case GS_MAIN_MENU:
            if (idx == 0) {          // continue
                if (g->night_cleared <= 0 && w->kills == 0) {
                    g->toast_t = 2.5f;
                    snprintf(g->toast, sizeof g->toast, "No saved run. Start a new game.");
                    break;
                }
                g->pending_night = clampi(g->night_cleared, 1, 10);
                w->state = GS_LOADING;
                g->load_t = 0.f;
            } else if (idx == 1) {   // new game
                if (w->net.running) net_stop(g);
                game_new_run(g);
            } else if (idx == 2) {   // multiplayer lobby
                w->state = GS_LOBBY;
                w->state_time = 0.f;
                g->menu_sel = 0;
            } else if (idx == 3) {
                g->settings_from = GS_MAIN_MENU;
                w->state = GS_SETTINGS;
                g->settings_sel = 0;
            } else if (idx == 4) {
                w->state = GS_CONTROLS;
            } else if (idx == 5) {
                g->plat->quit_requested = 1;
            }
            sfx(g, g->as->snd_ui_select, NULL, 0.9f, 1.f, 3);
            break;
        case GS_PAUSE:
            if (idx == 0) w->state = w->paused_from;
            else if (idx == 1) { g->settings_from = GS_PAUSE; w->state = GS_SETTINGS; g->settings_sel = 0; }
            else if (idx == 2) {
                g->pending_night = w->night;
                w->state = GS_LOADING;
                g->load_t = 0.f;
                game_start_day(g, w->night);
                w->state = GS_LOADING;
            } else if (idx == 3) { w->state = GS_MAIN_MENU; w->state_time = 0.f; }
            else if (idx == 4) g->plat->quit_requested = 1;
            sfx(g, g->as->snd_ui_select, NULL, 0.9f, 1.f, 3);
            break;
        case GS_GAMEOVER:
            if (idx == 0) {
                game_start_day(g, w->night);
                g->pending_night = w->night;
                w->state = GS_LOADING;
                g->load_t = 0.f;
            } else {
                w->state = GS_MAIN_MENU;
            }
            break;
        default:
            break;
    }
}
