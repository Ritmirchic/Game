// Automated visual tour + gameplay smoke test. Used by the headless harness
// (--tour) to verify that every important room is lit, reachable and rendering.
#include "game.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

extern void scene_draw(Game *g);
extern void game_update_char_anims(Game *g, float dt);
int png_write_rgba(const char *path, const uint8_t *px, int w, int h);

static char g_shot_dir_done[256];
static void ensure_dir(const char *dir) {
    if (!dir || !dir[0] || strcmp(g_shot_dir_done, dir) == 0) return;
    snprintf(g_shot_dir_done, sizeof g_shot_dir_done, "%s", dir);
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0777);
#endif
}

static void shot(Game *g, const char *dir, const char *name) {
    ensure_dir(dir);
    char path[512];
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    png_write_rgba(path, (const uint8_t *)g->rd->color, g->rd->w, g->rd->h);
    LOGI("tour capture: %s (%dx%d)", path, g->rd->w, g->rd->h);
}

static void step(Game *g, int frames) {
    for (int i = 0; i < frames; i++) {
        game_frame(g, 1.f / 60.f);
        game_update_char_anims(g, 1.f / 60.f);
    }
}

static void look(Game *g, Vec3 pos, float yaw, float pitch) {
    World *w = g->w;
    Player *p = &w->players[w->local_player];
    p->pos = pos;
    p->vel = v3(0, 0, 0);
    p->yaw = yaw;
    p->pitch = pitch;
    p->alive = 1;
    p->hp = 100.f;
}

static void render_only(Game *g) {
    scene_draw(g);
    ui_draw(g);
}

void run_tour(Game *g, const char *dir) {
    World *w = g->w;
    char path[512];
    snprintf(path, sizeof path, "%s", dir);
    LOGI("tour: begin, assets ok, meshes=%d colliders=%d", w->level.mesh_count, w->level.aabb_count);
    // ---------- daytime store
    game_start_day(g, 1);
    w->state = GS_DAY;
    step(g, 6);
    struct { const char *name; Vec3 pos; float yaw, pitch; } views[] = {
        {"01_entrance_inside", v3(0, 0, 17.5f), 3.14159f, -0.02f},
        {"02_aisles", v3(-2.5f, 0, 8.0f), 3.14159f, -0.06f},
        {"03_checkouts", v3(0, 0, 6.0f), 0.0f, -0.05f},
        {"04_produce_west", v3(-14.f, 0, 11.0f), 4.0f, -0.10f},
        {"05_refrigerated", v3(-18.f, 0, 0.0f), 4.712f, -0.05f},
        {"06_frozen_east", v3(18.f, 0, 0.0f), 1.5708f, -0.05f},
        {"07_pharmacy", v3(-18.f, 0, -7.0f), 3.4f, -0.05f},
        {"08_security_office", v3(19.f, 0, -10.0f), 0.0f, -0.02f},
        {"09_boh_corridor", v3(-6.f, 0, -14.2f), 1.5708f, -0.02f},
        {"10_storage", v3(-20.f, 0, -17.5f), 0.0f, -0.05f},
        {"11_loading_dock", v3(0.0f, 0, -16.5f), 0.0f, -0.02f},
        {"12_maintenance", v3(14.f, 0, -17.0f), 1.2f, -0.05f},
        {"13_electrical", v3(22.f, 0, -17.0f), 0.4f, -0.02f},
        {"14_breakroom", v3(-6.f, 0, -18.0f), 0.4f, -0.05f},
    };
    for (size_t i = 0; i < sizeof views / sizeof views[0]; i++) {
        look(g, views[i].pos, views[i].yaw, views[i].pitch);
        step(g, 3);
        render_only(g);
        shot(g, dir, views[i].name);
    }
    // exterior
    look(g, v3(0, 0, 26.f), 3.14159f, 0.02f);
    step(g, 3);
    render_only(g);
    shot(g, dir, "15_parking_lot");
    look(g, v3(-14.f, 0, 30.f), 2.2f, 0.03f);
    step(g, 2);
    render_only(g);
    shot(g, dir, "16_cars");
    look(g, v3(0, 0, 22.f), 0.0f, 0.05f);
    step(g, 2);
    render_only(g);
    shot(g, dir, "17_storefront");

    // ---------- night with zombies
    wave_start_night(g, 3);
    w->state = GS_NIGHT;
    w->state_time = 5.f;
    // spawn a mixed crowd in front of the player
    look(g, v3(0, 0, 12.0f), 3.14159f, -0.04f);
    Vec3 spots[10] = {
        v3(-2.5f, 0, 6.0f), v3(1.5f, 0, 5.0f), v3(-5.0f, 0, 4.0f), v3(4.0f, 0, 3.5f),
        v3(-1.0f, 0, 2.0f), v3(6.0f, 0, 6.5f), v3(-7.0f, 0, 7.0f), v3(2.0f, 0, 1.0f),
        v3(-3.0f, 0, -1.0f), v3(5.0f, 0, -2.0f)};
    int types[10] = {ZB_WALKER, ZB_RUNNER, ZB_WALKER, ZB_CRAWLER, ZB_BRUTE,
                     ZB_WALKER, ZB_SCREAMER, ZB_RUNNER, ZB_WALKER, ZB_WALKER};
    for (int i = 0; i < 10; i++) ai_spawn_zombie(g, types[i], spots[i], 3);
    LOGI("tour: spawned, zombie_count=%d", w->zombie_count);
    for (int i = 0; i < MAX_ZOMBIES; i++)
        if (w->zombies[i].alive)
            LOGI("  z%d type=%d state=%d pos=(%.2f,%.2f) hp=%.0f", i, w->zombies[i].type, w->zombies[i].state,
                 w->zombies[i].pos.x, w->zombies[i].pos.z, w->zombies[i].hp);
    g->zanim_state[0] = -2;
    step(g, 50);
    g->show_debug = 1;
    step(g, 3);
    g->show_debug = 0;
    step(g, 70);
    look(g, v3(0, 0, 12.0f), 3.14159f, -0.05f);
    step(g, 2);
    render_only(g);
    shot(g, dir, "18_night_zombies");
    for (int i = 0; i < MAX_ZOMBIES; i++)
        if (w->zombies[i].alive)
            LOGI("  post z%d state=%d pos=(%.2f,%.2f)", i, w->zombies[i].state,
                 w->zombies[i].pos.x, w->zombies[i].pos.z);
    // fire the pistol at the crowd
    g->w->players[0].weapon = WP_PISTOL;
    g->w->players[0].owned[WP_RIFLE] = 1;
    g->w->players[0].mag[WP_PISTOL] = 12;
    g->show_debug = 1;
    render_only(g);
    g->show_debug = 0;
    combat_trigger(g, &g->w->players[0], 1);
    step(g, 1);
    render_only(g);
    shot(g, dir, "19_firing");
    // rifle + shotgun views
    g->w->players[0].weapon = WP_SHOTGUN;
    g->w->players[0].owned[WP_SHOTGUN] = 1;
    g->w->players[0].mag[WP_SHOTGUN] = 6;
    g->w->players[0].switch_t = 0.f;
    step(g, 4);
    combat_trigger(g, &g->w->players[0], 0);
    step(g, 1);
    render_only(g);
    shot(g, dir, "20_shotgun");
    g->w->players[0].weapon = WP_RIFLE;
    g->w->players[0].mag[WP_RIFLE] = 30;
    g->w->players[0].switch_t = 0.f;
    g->w->players[0].aim_t = 1.f;
    step(g, 3);
    render_only(g);
    shot(g, dir, "21_rifle_ads");
    g->w->players[0].weapon = WP_SNIPER;
    g->w->players[0].owned[WP_SNIPER] = 1;
    g->w->players[0].mag[WP_SNIPER] = 5;
    g->w->players[0].switch_t = 0.f;
    step(g, 3);
    render_only(g);
    shot(g, dir, "22_sniper_scope");
    g->w->players[0].weapon = WP_CROSSBOW;
    g->w->players[0].mag[WP_CROSSBOW] = 1;
    g->w->players[0].aim_t = 0.f;
    step(g, 3);
    render_only(g);
    shot(g, dir, "23_crossbow");
    g->w->players[0].weapon = WP_AXE;
    g->w->players[0].owned[WP_AXE] = 1;
    g->w->players[0].switch_t = 0.f;
    step(g, 3);
    render_only(g);
    shot(g, dir, "24_axe");
    // flashlight off (dark) to prove the world is never fully black
    g->w->players[0].flashlight_on = 0.f;
    g->w->players[0].weapon = WP_PISTOL;
    step(g, 3);
    render_only(g);
    shot(g, dir, "25_flashlight_off");
    g->w->players[0].flashlight_on = 1.f;
    // barricade built at the main entrance
    g->w->players[0].scrap = 12;
    barricade_place(g, 0, 1);
    step(g, 20);
    look(g, v3(0, 0, 14.f), 0.0f, 0.0f);
    step(g, 3);
    render_only(g);
    shot(g, dir, "26_barricade_wood");
    barricade_place(g, 0, 3);
    step(g, 30);
    render_only(g);
    shot(g, dir, "27_barricade_metal");
    // HUD states
    w->players[0].hp = 28.f;
    w->players[0].money = 780;
    w->subtitle_t = 3.f;
    snprintf(w->subtitle, sizeof w->subtitle, "They are inside the store.");
    step(g, 2);
    render_only(g);
    shot(g, dir, "28_hud_low_health");
    w->state = GS_SHOP;
    step(g, 1);
    render_only(g);
    shot(g, dir, "29_shop");
    w->state = GS_SETTINGS;
    step(g, 1);
    render_only(g);
    shot(g, dir, "30_settings");
    w->state = GS_CONTROLS;
    step(g, 1);
    render_only(g);
    shot(g, dir, "31_controls");
    w->state = GS_LOBBY;
    g->menu_sel = 0;
    w->net.mode = 1;
    w->net.running = 1;
    w->net.peer_count = 1;
    snprintf(w->net.status, sizeof w->net.status, "HOSTING - 1 CLIENT(S) CONNECTED");
    step(g, 1);
    render_only(g);
    shot(g, dir, "32_lobby");
    w->net.mode = 0;
    w->net.running = 0;
    w->net.peer_count = 0;
    w->net.status[0] = 0;
    w->state = GS_PAUSE;
    step(g, 1);
    render_only(g);
    shot(g, dir, "33_pause");
    w->state = GS_MAIN_MENU;
    g->menu_count = 0;
    w->state_time = 0.f;
    step(g, 30);
    render_only(g);
    shot(g, dir, "34_main_menu");
    w->state = GS_GAMEOVER;
    w->night = 4;
    w->night_kills = 33;
    step(g, 2);
    render_only(g);
    shot(g, dir, "35_gameover");
    w->state = GS_VICTORY;
    step(g, 2);
    render_only(g);
    shot(g, dir, "36_victory");
    // boss encounter
    w->state = GS_NIGHT;
    wave_start_night(g, 10);
    w->state = GS_NIGHT;
    w->spawned_this_night = 200;
    ai_spawn_zombie(g, ZB_BOSS, v3(0, 0, 18.f), 10);
    look(g, v3(0, 0, 22.f), 0.0f, 0.06f);
    step(g, 60);
    render_only(g);
    shot(g, dir, "37_boss");
    LOGI("tour: complete");
}

// ---------------------------------------------------------------------------
// Real LAN test: runs one process as host or as client, stepping the game in
// real time (so UDP packets actually flow) and logging what it observed.
// `--nettest host` and `--nettest client <ip>` are launched side by side by
// tools/nettest.sh; the logs are then compared to prove replication works.
// ---------------------------------------------------------------------------
#ifdef _WIN32
#include <windows.h>
static void net_sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <unistd.h>
static void net_sleep_ms(int ms) { usleep((unsigned int)ms * 1000u); }
#endif

void run_net_test(Game *g, int mode, const char *addr, float seconds) {
    World *w = g->w;
    game_new_run(g);
    wave_start_night(g, 2);
    w->state = GS_NIGHT;
    w->state_time = 4.f;
    net_start(g, mode, addr);
    LOGI("nettest: mode=%d running=%d status=%s", mode, w->net.running, w->net.status);
    int frames = (int)(seconds * 60.f);
    int last_z = -1, last_peers = -1, last_night = -1;
    int shots = 0;
    float worst_delta = 0.f;
    for (int i = 0; i < frames; i++) {
        Input *in = &g->plat->in;
        // the client walks forward and fires, so we can see input reach the host
        memset(in->down, 0, sizeof in->down);
        memset(in->pressed, 0, sizeof in->pressed);
        memset(in->mouse_pressed, 0, sizeof in->mouse_pressed);
        if (i > 60) in->down['W'] = 1;
        if (i > 120 && (i % 20) == 0) in->mouse_pressed[0] = 1;
        game_frame(g, 1.f / 60.f);
        game_update_char_anims(g, 1.f / 60.f);
        if (w->net.running) net_update(g, 1.f / 60.f);
        net_sleep_ms(16);
        if (w->zombie_count != last_z || w->net.peer_count != last_peers || w->night != last_night) {
            last_z = w->zombie_count;
            last_peers = w->net.peer_count;
            last_night = w->night;
            LOGI("nettest t=%5.2fs zombies=%d peers=%d night=%d state=%d status=%s",
                 (float)i / 60.f, w->zombie_count, w->net.peer_count, w->night, (int)w->state, w->net.status);
        }
        // how far did the client's prediction have to be corrected by the host?
        if (mode == 2 && i > 30 && w->net.rep_delta > worst_delta) worst_delta = w->net.rep_delta;
        if (i == frames / 2 || i == frames - 1) {
            char name[64];
            snprintf(name, sizeof name, "net_%s_%d", mode == 1 ? "host" : "client", shots++);
            render_only(g);
            shot(g, "shots/net", name);
        }
    }
    LOGI("nettest: done zombies=%d players=%d peers=%d running=%d", w->zombie_count, w->player_count,
         w->net.peer_count, w->net.running);
    if (mode == 2) {
        Player *lp = &w->players[w->local_player];
        LOGI("nettest: client local player=%d pos=(%.2f,%.2f) hp=%.0f worst-reconcile=%.2f m",
             w->local_player, lp->pos.x, lp->pos.z, lp->hp, worst_delta);
    }
    net_stop(g);
}
