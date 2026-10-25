/* ============================================================================
   SPACE MINER — core/game.c
   Session lifecycle, screen flow, camera, audio glue, achievements.
   ========================================================================== */
#include "state.h"
#include "audio.h"

/* ------------------------------------------------------------------ audio -- */
void snd_play(Game *g, const char *name, f32 volume, f32 pan, f32 pitch) {
    if (g->save.audio.muted) return;
    audio_play(name, volume, SM_CLAMP(pan, -1.f, 1.f), pitch);
}
void snd_music(Game *g, const char *name, f32 fade) {
    if (g->save.audio.muted) { audio_music_stop(0.3f); return; }
    audio_music_play(name, fade);
}
void snd_stop_music(Game *g, f32 fade) { audio_music_stop(fade); }

/* ------------------------------------------------------------------ toast -- */
void game_toast(Game *g, const char *msg, u32 color) {
    sm_strlcpy(g->toast, msg, sizeof(g->toast));
    g->toast_color = color;
    g->toast_timer = 2.6f;
}
void game_toast2(Game *g, const char *msg, u32 color) {
    sm_strlcpy(g->toast2, msg, sizeof(g->toast2));
    g->toast2_timer = 3.2f;
    (void)color;
}

/* ----------------------------------------------------------- achievements -- */
void ach_unlock(Game *g, int id) {
    if (id < 0 || id >= ACH_COUNT) return;
    u32 bit = 1u << id;
    if (g->save.achievements & bit) return;
    g->save.achievements |= bit;
    char buf[128];
    sm_sprintf(buf, sizeof(buf), "ACHIEVEMENT: %s", k_achievements[id].name);
    game_toast(g, buf, SM_RGB(255, 210, 120));
    snd_play(g, "sfx_achieve", 0.7f, 0.f, 1.f);
    if (g->plat->write_file) save_write(g);
}

void ach_check(Game *g) {
    SaveData *d = &g->save;
    if (d->stat_asteroids_mined >= 1) ach_unlock(g, ACH_FIRST_ORE);
    if (d->stat_enemies_killed >= 1) ach_unlock(g, ACH_FIRST_BLOOD);
    if (d->stat_enemies_killed >= 50) ach_unlock(g, ACH_SHARPSHOOTER);
    if (d->stat_credits_earned >= 1000000) ach_unlock(g, ACH_MILLIONAIRE);
    if (d->stat_crystals_sold >= 250) ach_unlock(g, ACH_CRYSTAL_HUNTER);
    if (d->sector_unlocked[2]) ach_unlock(g, ACH_DEEP_SPACE);
    if (d->sector_unlocked[4]) ach_unlock(g, ACH_VOID_WALKER);
    if (d->drone_count >= 4) ach_unlock(g, ACH_DRONE_ARMADA);
    if (d->level >= 20) ach_unlock(g, ACH_MINER_MASTER);
    int all_ships = 1;
    for (int i = 0; i < SHIP_COUNT; i++) if (!d->ship_owned[i]) all_ships = 0;
    if (all_ships) ach_unlock(g, ACH_FLEET_ACE);
    for (int i = 0; i < UPG_COUNT; i++)
        if (d->upgrades[i] >= UPG_MAX_LEVEL) { ach_unlock(g, ACH_FULL_UPGRADE); break; }
}

/* ------------------------------------------------------------------ screens -- */
void game_set_screen(Game *g, ScreenId s) {
    if (g->screen == s) return;
    g->screen_prev = g->screen;
    g->screen = s;
    g->screen_time = 0.f;
    g->ui_anim = 0.f;
    if (s == SCREEN_DOCK) {
        snd_play(g, "sfx_dock", 0.7f, 0.f, 1.f);
        snd_music(g, "music_menu", 1.2f);
        g->dock_tab = DOCK_TAB_MARKET;
        g->dock_sel = 0;
        g->dock_scroll = 0;
        if (g->save.gameplay.autosave) save_write(g);
    } else if (s == SCREEN_PLAY) {
        snd_music(g, k_sectors[g->sector].music, 1.2f);
    } else if (s == SCREEN_MENU) {
        snd_music(g, "music_menu", 1.4f);
    }
}

void ui_open_dock(Game *g) {
    if (g->screen != SCREEN_PLAY) return;
    game_set_screen(g, SCREEN_DOCK);
}

/* ------------------------------------------------------------------- create -- */
static void init_background(Game *g) {
    Rng r;
    rng_seed(&r, 0xC0FFEEull);
    g->star_count = 380;
    for (int i = 0; i < g->star_count; i++) {
        g->stars[i] = v2(rng_range(&r, 0.f, (f32)SM_SCREEN_W), rng_range(&r, 0.f, (f32)SM_SCREEN_H));
        g->star_size[i] = rng_range(&r, 0.8f, 2.8f);
        g->star_twinkle[i] = rng_f(&r);
        f32 hue = rng_f(&r);
        if (hue < 0.6f) g->star_color[i] = SM_RGB(220, 235, 255);
        else if (hue < 0.85f) g->star_color[i] = SM_RGB(255, 240, 210);
        else g->star_color[i] = SM_RGB(190, 210, 255);
    }
    g->dust_count = 220;
    for (int i = 0; i < g->dust_count; i++) {
        g->dust[i] = v2(rng_range(&r, 0.f, (f32)SM_SCREEN_W), rng_range(&r, 0.f, (f32)SM_SCREEN_H));
    }
}

Game *game_create(const SmPlatform *plat, const Pak *pak, Gfx *gfx) {
    Game *g = (Game *)sm_calloc(1, sizeof(Game));
    if (!g) return NULL;
    g->plat = plat;
    g->pak = pak;
    g->gfx = gfx;
    rng_seed(&g->rng, 0x5EED5EEDull);
    init_background(g);
    save_apply_defaults(g);
    if (plat->file_exists && plat->file_exists("spaceminer.sav", plat->user)) {
        if (save_read(g) != 0) {
            sm_warnf("save file present but unreadable; starting fresh (old file kept)");
            save_apply_defaults(g);
        }
    }
    audio_set_gain(g->save.audio.master, g->save.audio.music, g->save.audio.sfx);
    assets_load(g);
    g->screen = SCREEN_BOOT;
    g->screen_time = 0.f;
    g->menu_sel = 0;
    g->pause_sel = 0;
    g->sel_ship = g->save.ship;
    g->sel_drone = 0;
    g->sel_sector = g->save.sector_current;
    g->show_perf = 0;
    player_reset(g, 1);
    g->sector = SM_CLAMP(g->save.sector_current, 0, SECTOR_COUNT - 1);
    g->music_track = -1;
    return g;
}

void game_destroy(Game *g) {
    if (!g) return;
    if (g->plat->write_file) save_write(g);
    sm_free(g);
}

int game_wants_quit(Game *g) { return g->screen == SCREEN_MENU && g->menu_sel == 99; }

/* --------------------------------------------------------------- new game -- */
void game_new_session(Game *g) {
    save_apply_defaults(g);
    g->sector = 0;
    memset(g->sector_entered, 0, sizeof(g->sector_entered));
    player_reset(g, 0);
    world_clear_entities(g);
    g->player.pos = v2(0, -760.f);
    world_populate(g);
    game_set_screen(g, SCREEN_PLAY);
    snd_music(g, k_sectors[0].music, 1.6f);
    game_toast(g, "WELCOME TO THE STARTER BELT", SM_RGB(140, 240, 255));
    game_toast2(g, "Hold RIGHT MOUSE on a rock to mine  -  press E at the station to dock", SM_RGB(180, 205, 225));
    if (g->plat->write_file) save_write(g);
}

/* ------------------------------------------------------------------ update -- */
static void update_camera(Game *g, f32 dt) {
    Player *p = &g->player;
    f32 spd = v2len(p->vel);
    Vec2 lead = v2mul(v2norm(p->vel), SM_MIN(160.f, spd * 0.28f));
    Vec2 want = v2add(p->pos, lead);
    f32 follow = 1.f - sm_decay(4.2f, dt);
    g->gfx->cam = v2lerp(g->gfx->cam, want, follow);
    f32 zoom_target = 1.f - SM_MIN(0.10f, spd / 5200.f);
    if (g->screen == SCREEN_DOCK) zoom_target = 1.f;
    g->gfx->zoom = sm_lerpf(g->gfx->zoom, zoom_target, SM_MIN(1.f, dt * 3.f));
    /* shake */
    f32 shake = g->cam.shake;
    if (g->save.video.shake == 0) shake *= 0.25f;
    f32 t = g->time * 46.f;
    g->gfx->shake = v2(sinf(t * 1.7f) * shake, cosf(t * 2.3f) * shake);
    g->cam.warp = SM_MAX(0.f, g->cam.warp - dt * 0.8f);
}

void game_update(Game *g, Input *in, f32 dt) {
    g->frame++;
    g->time += dt;
    g->screen_time += dt;
    g->ui_anim += dt;
    if (g->toast_timer > 0.f) g->toast_timer -= dt;
    if (g->toast2_timer > 0.f) g->toast2_timer -= dt;
    fx_update(g, dt);
    audio_update(dt);

    /* global hotkeys */
    if (in->pressed[SM_KEY_F1]) g->show_perf = !g->show_perf;
    if ((in->pressed[SM_KEY_F2] || in->pressed[SM_KEY_F12]) && g->plat->save_screenshot) {
        g->plat->save_screenshot(g->gfx->px, g->gfx->w, g->gfx->h, g->plat->user);
        game_toast(g, "SCREENSHOT SAVED", SM_RGB(160, 220, 255));
    }
    if (in->pressed[SM_KEY_F3]) {
        g->save.video.bloom = !g->save.video.bloom;
        game_toast(g, g->save.video.bloom ? "BLOOM ON" : "BLOOM OFF", SM_RGB(180, 210, 235));
    }

    switch (g->screen) {
        case SCREEN_BOOT:
            if (g->screen_time > 2.2f || in->mouse_pressed[0] || in->text_len > 0) game_set_screen(g, SCREEN_MENU);
            break;
        case SCREEN_MENU:
            ui_update(g, in, dt);
            break;
        case SCREEN_INTRO:
            ui_update(g, in, dt);
            break;
        case SCREEN_PLAY: {
            if (in->pressed[SM_KEY_ESCAPE]) {
                g->paused = 1;
                g->pause_sel = 0;
                game_set_screen(g, SCREEN_PAUSED);
                snd_play(g, "sfx_click", 0.5f, 0.f, 1.f);
                break;
            }
            g->save.stat_time_played += dt;
            player_update(g, in, dt);
            combat_update(g, dt);
            drones_update(g, dt);
            world_update(g, dt);
            update_camera(g, dt);
            /* dock */
            if (g->station_near && in->pressed[SM_KEY_E]) {
                ui_open_dock(g);
                break;
            }
            /* scanner pulse */
            if (in->pressed[SM_KEY_Q]) {
                f32 radius = 900.f + 260.f * (f32)g->save.upgrades[UPG_SCANNER];
                fx_ring(g, g->player.pos, radius, SM_RGB(120, 230, 255), 1.1f);
                g->scan_pulse = 1.f;
                snd_play(g, "sfx_scan", 0.6f, 0.f, 1.f);
                int marked = 0;
                for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
                    Asteroid *a = &g->asteroids[i];
                    if (!a->active) continue;
                    if (v2dist(a->pos, g->player.pos) < radius) {
                        a->rich = 1;
                        marked++;
                        fx_ring(g, a->pos, a->radius * 2.2f, k_resources[a->res].glow, 0.6f);
                    }
                }
                char buf[64];
                sm_sprintf(buf, sizeof(buf), "SCAN: %d DEPOSITS", marked);
                game_toast(g, buf, SM_RGB(140, 220, 255));
            }
            g->scan_pulse = SM_MAX(0.f, g->scan_pulse - dt * 1.2f);
            if (g->player.dead && !g->game_over_timer) {
                g->game_over_timer = 1;
                g->screen_fade = 0.f;
            }
            if (g->game_over_timer) {
                g->game_over_timer++;
                if (g->game_over_timer > 90) {
                    g->game_over_timer = 0;
                    game_set_screen(g, SCREEN_GAMEOVER);
                    snd_music(g, "music_void", 2.f);
                }
            }
            /* autosave every 90 seconds */
            static f32 autosave_t = 0.f;
            autosave_t += dt;
            if (autosave_t > 90.f) {
                autosave_t = 0.f;
                if (g->save.gameplay.autosave && g->plat->write_file) save_write(g);
            }
        } break;
        case SCREEN_DOCK:
            ui_update(g, in, dt);
            if (in->pressed[SM_KEY_ESCAPE]) {
                game_set_screen(g, SCREEN_PLAY);
                save_write(g);
                snd_play(g, "sfx_click", 0.5f, 0.f, 1.f);
            }
            /* drones keep working while docked */
            drones_update(g, dt);
            fx_update(g, 0.f);
            break;
        case SCREEN_PAUSED:
            ui_update(g, in, dt);
            break;
        case SCREEN_GAMEOVER:
            ui_update(g, in, dt);
            break;
        case SCREEN_CREDITS:
            ui_update(g, in, dt);
            break;
        default: break;
    }
    if (g->screen == SCREEN_PLAY || g->screen == SCREEN_DOCK) {
        ach_check(g);
    }
    g->fps_smooth = g->fps_smooth * 0.92f + (1.f / SM_MAX(0.0001f, dt)) * 0.08f;
}

/* ------------------------------------------------------------------ render -- */
void game_render(Game *g) {
    Gfx *gfx = g->gfx;
    gfx->shake = v2(0, 0);
    gfx->zoom = 1.f;
    switch (g->screen) {
        case SCREEN_BOOT:
            ui_render_boot(g, NULL);
            break;
        case SCREEN_MENU:
            ui_render_menu(g, NULL);
            break;
        case SCREEN_INTRO:
            ui_render_intro(g, NULL);
            break;
        case SCREEN_PLAY:
        case SCREEN_PAUSED:
        case SCREEN_GAMEOVER: {
            /* rebuild camera transform for the frame */
            Player *p = &g->player;
            gfx->zoom = 1.f - SM_MIN(0.10f, v2len(p->vel) / 5200.f);
            world_render_background(g);
            update_camera(g, 0.f);
            world_render(g);
            drones_render(g);
            player_render(g);
            combat_render(g);
            fx_render_front(g);
            if (g->save.video.bloom) gfx_bloom(gfx, 0.74f, 0.34f);
            gfx_vignette(gfx, 0.55f);
            ui_render_hud(g);
            if (g->screen == SCREEN_PAUSED) ui_render_pause(g, NULL);
            if (g->screen == SCREEN_GAMEOVER) ui_render_gameover(g, NULL);
        } break;
        case SCREEN_DOCK:
            ui_render_dock(g, NULL);
            break;
        case SCREEN_CREDITS:
            ui_render_menu(g, NULL);
            break;
        default: break;
    }
    /* scanlines / filters */
    if (g->save.video.scanlines) {
        for (int y = 0; y < gfx->h; y += 3) {
            gfx_fill(gfx, rect(0, (f32)y, (f32)gfx->w, 1.f), SM_RGBA(0, 0, 0, 40));
        }
    }
    if (g->show_perf) {
        char buf[128];
        sm_sprintf(buf, sizeof(buf), "%.0f FPS  |  %.1f MB  |  parts %d  |  voices %d",
                   g->fps_smooth, (double)sm_mem_used() / 1048576.0, SM_MAX_PARTICLES, audio_active_voices());
        gfx_text_shadow(gfx, &g->assets.font_tiny, v2(12, (f32)gfx->h - 18), buf, SM_RGB(160, 200, 220), 0);
    }
}
