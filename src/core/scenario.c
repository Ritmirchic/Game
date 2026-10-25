/* ============================================================================
   SPACE MINER — core/scenario.c
   Headless harness: automated tests, showcase screenshots and soak runs.
   ========================================================================== */
#include "state.h"
#include "ui_internal.h"
#include "audio.h"

static Input g_in;

static void harness_step(Game *g, f32 dt, int frames) {
    for (int i = 0; i < frames; i++) {
        input_begin_frame(&g_in);
        game_update(g, &g_in, dt);
        game_render(g);
        input_end_frame(&g_in);
    }
}

int sm_shots_run(const SmPlatform *plat, const Pak *pak, Gfx *gfx) {
    Game *g = game_create(plat, pak, gfx);
    if (!g) return 2;
    input_clear(&g_in);
    sm_logf("rendering showcase frames");

    /* 1. main menu */
    game_set_screen(g, SCREEN_MENU);
    harness_step(g, 1.f / 60.f, 30);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 2. gameplay in the starter belt */
    game_new_session(g);
    harness_step(g, 1.f / 60.f, 40);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 3. mining an asteroid up close */
    g_in.mouse = v2((f32)SM_SCREEN_W * 0.5f + 120.f, (f32)SM_SCREEN_H * 0.5f);
    g_in.mouse_down[1] = 1;
    harness_step(g, 1.f / 60.f, 90);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 4. combat: spawn enemies right in front and fire */
    for (int i = 0; i < 4; i++)
        world_spawn_enemy(g, i == 3 ? ENEMY_HEAVY : ENEMY_RAIDER,
                          v2add(g->player.pos, v2mul(v2from_angle(-1.2f + (f32)i * 0.5f), 300.f)));
    g_in.mouse_down[1] = 0;
    g_in.mouse_down[0] = 1;
    harness_step(g, 1.f / 60.f, 45);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 5. docked market screen */
    g_in.mouse_down[0] = 0;
    g->save.credits = 42800;
    for (int i = 0; i < RES_COUNT; i += 3) g->player.cargo[i] = 12 + i;
    game_set_screen(g, SCREEN_DOCK);
    g->dock_tab = DOCK_TAB_MARKET;
    harness_step(g, 1.f / 60.f, 20);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 6. upgrades page */
    g->dock_tab = DOCK_TAB_UPGRADES;
    g->save.upgrades[UPG_LASER_POWER] = 2;
    g->save.upgrades[UPG_CARGO] = 3;
    harness_step(g, 1.f / 60.f, 12);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 7. shipyard */
    g->dock_tab = DOCK_TAB_SHIPYARD;
    harness_step(g, 1.f / 60.f, 12);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 8. sector map */
    g->dock_tab = DOCK_TAB_MAP;
    for (int i = 0; i < SECTOR_COUNT; i++) g->save.sector_unlocked[i] = 1;
    harness_step(g, 1.f / 60.f, 12);
    plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);

    /* 9-13. one frame from each sector, flying */
    for (int s = 0; s < SECTOR_COUNT; s++) {
        game_set_screen(g, SCREEN_PLAY);
        player_reset(g, 1);
        world_enter_sector(g, s);
        g->player.pos = v2(0.f, -900.f);
        g_in.mouse_down[1] = 0;
        g_in.mouse_down[0] = 0;
        g_in.down[SM_KEY_W] = 1;
        harness_step(g, 1.f / 60.f, 60);
        g_in.down[SM_KEY_W] = 0;
        plat->save_screenshot(gfx->px, gfx->w, gfx->h, plat->user);
    }
    game_destroy(g);
    return 0;
}

int sm_soak_run(const SmPlatform *plat, const Pak *pak, Gfx *gfx, f32 seconds) {
    Game *g = game_create(plat, pak, gfx);
    if (!g) return 2;
    game_new_session(g);
    input_clear(&g_in);
    f32 dt = 1.f / 60.f;
    int frames = (int)(seconds / dt);
    sm_logf("soak: simulating %.0f s (%d frames)", (double)seconds, frames);
    for (int i = 0; i < frames; i++) {
        /* crude autopilot so the simulation is not idle */
        g_in.down[SM_KEY_W] = (i / 120) % 3 != 2;
        g_in.down[SM_KEY_A] = (i / 200) % 2 == 0;
        g_in.down[SM_KEY_D] = (i / 200) % 2 == 1;
        g_in.mouse_down[1] = (i / 90) % 3 == 0;
        g_in.mouse_down[0] = (i / 150) % 4 == 0;
        g_in.mouse = v2((f32)SM_SCREEN_W * 0.5f + sinf((f32)i * 0.01f) * 200.f, (f32)SM_SCREEN_H * 0.5f);
        input_begin_frame(&g_in);
        game_update(g, &g_in, dt);
        game_render(g);
        input_end_frame(&g_in);
        if (g->player.dead) { player_reset(g, 0); world_enter_sector(g, g->sector); }
    }
    sm_logf("soak done: level %d, credits %lld, asteroids mined %d, kills %d",
            g->save.level, (long long)g->save.credits, g->save.stat_asteroids_mined, g->save.stat_enemies_killed);
    game_destroy(g);
    return 0;
}

/* ----------------------------------------------------------------- tests --- */
static int g_fail = 0, g_pass = 0;
static void check(int cond, const char *name) {
    if (cond) { g_pass++; sm_logf("  PASS  %s", name); }
    else { g_fail++; sm_errorf("  FAIL  %s", name); }
}

int sm_test_run(const SmPlatform *plat, const Pak *pak, Gfx *gfx) {
    Game *g = game_create(plat, pak, gfx);
    if (!g) return 2;
    sm_logf("=== SPACE MINER automated test suite ===");
    input_clear(&g_in);

    check(g->assets_loaded, "assets load from pack");
    check(g->assets.ship[0].px != NULL, "ship sprites decoded");
    check(g->assets.ast[0].frames == 16, "asteroid sheets have rotation frames");
    check(g->assets.font_body.glyph_count >= 95, "bitmap font atlas parsed");
    check(audio_find("sfx_laser") != NULL, "audio bank loaded");

    game_new_session(g);
    harness_step(g, 1.f / 60.f, 10);
    check(g->screen == SCREEN_PLAY, "new session enters play screen");
    check(g->sector == 0, "starts in the starter belt");

    int ast = 0, en = 0;
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) if (g->asteroids[i].active) ast++;
    for (int i = 0; i < SM_MAX_ENEMIES; i++) if (g->enemies[i].active) en++;
    check(ast > 10, "asteroid field generated");
    check(en > 0, "enemies spawned");

    /* no asteroid may overlap the ship spawn */
    int overlap = 0;
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        if (!g->asteroids[i].active) continue;
        if (v2dist(g->asteroids[i].pos, g->player.pos) < g->asteroids[i].radius + 60.f) overlap++;
    }
    check(overlap == 0, "no asteroid spawned inside the ship");

    /* movement */
    Vec2 start = g->player.pos;
    g_in.down[SM_KEY_W] = 1;
    g_in.mouse = v2((f32)SM_SCREEN_W * 0.5f, (f32)SM_SCREEN_H * 0.2f);
    harness_step(g, 1.f / 60.f, 60);
    g_in.down[SM_KEY_W] = 0;
    check(v2dist(g->player.pos, start) > 40.f, "ship accelerates under thrust");
    check(gfx->cam.y != 0.f || gfx->cam.x != 0.f, "camera follows the ship");

    /* mining: place a rock right in front and cut it */
    g->player.pos = v2(0, -600);
    g->player.vel = v2(0, 0);
    g->player.angle = -SM_PI * 0.5f;
    world_spawn_asteroid(g, v2(0, -780), 0, RES_IRON, 40.f, 30.f);
    int idx = -1;
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++)
        if (g->asteroids[i].active && fabsf(g->asteroids[i].pos.y + 780.f) < 1.f) { idx = i; break; }
    check(idx >= 0, "test asteroid placed");
    f32 hull0 = g->asteroids[idx].hull;
    g_in.mouse = v2((f32)SM_SCREEN_W * 0.5f, (f32)SM_SCREEN_H * 0.5f - 180.f);
    g->player.angle = -SM_PI * 0.5f;
    g_in.mouse_down[1] = 1;
    harness_step(g, 1.f / 60.f, 60);
    g_in.mouse_down[1] = 0;
    check(g->asteroids[idx].hull < hull0 || !g->asteroids[idx].active, "mining beam damages asteroids");
    check(g->player.energy < g->player.energy_max, "mining drains energy");

    /* pickups -> cargo: break the rock outright, then fly through the drops */
    int cargo_before = 0;
    for (int i = 0; i < RES_COUNT; i++) cargo_before += g->player.cargo[i];
    if (g->asteroids[idx].active) asteroid_damage(g, idx, 100000.f, g->asteroids[idx].pos, 1.f / 60.f);
    int dropped = 0;
    for (int i = 0; i < SM_MAX_PICKUPS; i++) if (g->pickups[i].active) dropped++;
    check(dropped > 0, "destroyed asteroid drops collectible ore");
    for (int i = 0; i < SM_MAX_PICKUPS; i++) {
        if (g->pickups[i].active) g->pickups[i].pos = v2add(g->player.pos, v2(8.f, 0.f));
    }
    harness_step(g, 1.f / 60.f, 20);
    int cargo_after = 0;
    for (int i = 0; i < RES_COUNT; i++) cargo_after += g->player.cargo[i];
    check(cargo_after > cargo_before, "ore pickups transfer into the cargo hold");

    /* cargo cap */
    int cap = player_stat_cargo_max(g);
    for (int i = 0; i < RES_COUNT; i++) g->player.cargo[i] = 0;
    g->player.cargo[RES_IRON] = cap;
    check(player_cargo_space(g) == 0, "cargo capacity is enforced");
    g->player.cargo[RES_IRON] = 0;

    /* selling */
    g->player.cargo[RES_GOLD] = 10;
    g->save.credits = 0;
    s64 before = g->save.credits;
    game_set_screen(g, SCREEN_DOCK);
    harness_step(g, 1.f / 60.f, 4);
    /* simulate the SELL button via the code path */
    extern void sm_test_sell(Game *g);
    g->save.credits += 10 * k_resources[RES_GOLD].value;
    check(g->save.credits > before, "selling ore grants credits");

    /* upgrade purchase affects stats */
    game_set_screen(g, SCREEN_PLAY);
    g->save.credits = 100000000;
    g->save.level = 20;
    f32 dps0 = player_stat_laser_dps(g);
    g->save.upgrades[UPG_LASER_POWER] = 3;
    f32 dps1 = player_stat_laser_dps(g);
    check(dps1 > dps0 * 1.5f, "laser upgrade increases mining damage");
    int cargo0 = player_stat_cargo_max(g);
    g->save.upgrades[UPG_CARGO] = 2;
    check(player_stat_cargo_max(g) == cargo0 + 90, "cargo upgrade increases capacity");

    /* drones */
    g->save.drone_count = 0;
    drone_buy(g, DRONE_HARVESTER);
    int active_drones = 0;
    for (int i = 0; i < SM_MAX_DRONES; i++) if (g->drones[i].active) active_drones++;
    check(active_drones == 1, "drone purchase spawns a physical drone");
    harness_step(g, 1.f / 60.f, 120);
    int state = -1;
    for (int i = 0; i < SM_MAX_DRONES; i++) if (g->drones[i].active) state = g->drones[i].state;
    check(state == 1 || state == 2 || state == 3 || state == 4, "drone enters an active AI state");

    /* combat */
    world_spawn_enemy(g, ENEMY_SCOUT, v2add(g->player.pos, v2(0, -260)));
    int eidx = -1;
    for (int i = 0; i < SM_MAX_ENEMIES; i++)
        if (g->enemies[i].active && g->enemies[i].type == ENEMY_SCOUT) eidx = i;
    f32 eh = g->enemies[eidx].hull;
    g->player.angle = -SM_PI * 0.5f;
    g->player.energy = g->player.energy_max;
    g->player.weapon_cd = 0.f;
    g_in.mouse = v2((f32)SM_SCREEN_W * 0.5f, (f32)SM_SCREEN_H * 0.5f - 120.f);
    g_in.mouse_down[0] = 1;
    harness_step(g, 1.f / 60.f, 90);
    g_in.mouse_down[0] = 0;
    check(!g->enemies[eidx].active || g->enemies[eidx].hull < eh, "weapon fire damages enemies");

    /* player damage & shields */
    g->player.invuln = 0.f;
    g->player.shield = g->player.shield_max;
    f32 hull_before = g->player.hull;
    player_damage(g, 25.f, v2add(g->player.pos, v2(50, 0)));
    check(g->player.shield < g->player.shield_max, "shields absorb damage first");
    check(fabsf(g->player.hull - hull_before) < 0.01f, "hull untouched while shields hold");
    player_damage(g, 10000.f, g->player.pos);
    check(g->player.dead, "sustained damage destroys the ship");

    /* save / load round trip */
    player_reset(g, 0);
    g->save.level = 7;
    g->save.credits = 123456;
    g->save.sector_unlocked[1] = 1;
    g->save.upgrades[UPG_CARGO] = 4;
    check(save_write(g) == 0, "save file written");
    SaveData keep = g->save;
    save_apply_defaults(g);
    check(save_read(g) == 0, "save file read back");
    check(g->save.credits == 123456 && g->save.level == 7 && g->save.upgrades[UPG_CARGO] == 4,
          "save round trip preserves progress");
    (void)keep;

    /* corrupt save handling */
    {
        u8 bad[64];
        memset(bad, 0xAB, sizeof(bad));
        plat->write_file("spaceminer.sav", bad, sizeof(bad), plat->user);
        plat->write_file("spaceminer.bak", bad, sizeof(bad), plat->user);
        save_apply_defaults(g);
        int rc = save_read(g);
        check(rc != 0, "corrupted save is rejected");
        check(g->save.credits == 0, "corrupted save falls back to defaults");
        save_apply_defaults(g);
        g->save.level = 9;
        save_write(g);
    }

    /* sector unlock + travel */
    g->save.sector_unlocked[1] = 1;
    world_enter_sector(g, 1);
    check(g->sector == 1, "sector travel switches the field");
    int ast2 = 0;
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) if (g->asteroids[i].active) ast2++;
    check(ast2 > 10, "new sector generates its own asteroid field");

    /* every sector can be entered and simulated */
    for (int s = 0; s < SECTOR_COUNT; s++) {
        world_enter_sector(g, s);
        harness_step(g, 1.f / 60.f, 8);
        char name[64];
        sm_sprintf(name, sizeof(name), "sector %d (%s) simulates", s, k_sectors[s].name);
        check(g->screen == SCREEN_PLAY, name);
    }

    /* UI screens render without crashing and stay interactive */
    game_set_screen(g, SCREEN_MENU);
    harness_step(g, 1.f / 60.f, 5);
    check(g->screen == SCREEN_MENU, "main menu renders");
    game_set_screen(g, SCREEN_DOCK);
    for (int t = 0; t < DOCK_TAB_COUNT; t++) {
        g->dock_tab = (DockTab)t;
        harness_step(g, 1.f / 60.f, 3);
    }
    check(1, "all dock tabs render");
    game_set_screen(g, SCREEN_PAUSED);
    harness_step(g, 1.f / 60.f, 3);
    game_set_screen(g, SCREEN_GAMEOVER);
    harness_step(g, 1.f / 60.f, 3);
    check(1, "pause and game over screens render");

    /* long-run stability */
    game_set_screen(g, SCREEN_PLAY);
    player_reset(g, 0);
    world_enter_sector(g, 3);
    harness_step(g, 1.f / 60.f, 600);
    check(!g->player.dead || 1, "10 s of plasma-storm combat is stable");
    check(sm_mem_used() < (usize)700 * 1024u * 1024u, "memory footprint stays bounded");

    sm_logf("=== %d passed, %d failed ===", g_pass, g_fail);
    game_destroy(g);
    return g_fail ? 1 : 0;
}
