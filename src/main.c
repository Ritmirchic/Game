// THE LAST NIGHT — entry point and main loop.
#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#endif
#include "game.h"
#include "models.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

extern void scene_draw(Game *g);
extern void game_update_char_anims(Game *g, float dt);
void sounds_build(Assets *a);
int png_write_rgba(const char *path, const uint8_t *px, int w, int h);

static Game g_game;
static Assets g_assets;
static Platform g_plat;
static Renderer g_rd;

static void audio_callback(void *user, int16_t *buf, int frames) {
    (void)user;
    aud_mix(&g_game.w->audio, buf, frames);
}

// ---------------------------------------------------------------- logging
static void log_open(const char *dir) {
    char path[640];
    snprintf(path, sizeof path, "%s/the_last_night.log", dir);
    log_set_file(path);
}
static int write_png(const char *path, const uint32_t *rgba, int w, int h) {
    return png_write_rgba(path, (const uint8_t *)rgba, w, h);
}


#ifdef TLN_GUI
// ---------------------------------------------------------------- Windows GUI entry
// The archived build is a windowed application: no console window is created.
int tln_main(int argc, char **argv);
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    (void)hInst; (void)hPrev; (void)nShow;
    static char cmd[4096];
    static char *argv[64];
    snprintf(cmd, sizeof cmd, "%s", lpCmdLine ? lpCmdLine : "");
    int argc = 0;
    argv[argc++] = "THE_LAST_NIGHT.exe";
    char *p = cmd;
    while (*p && argc < 63) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"') { argv[argc++] = ++p; while (*p && *p != '"') p++; }
        else { argv[argc++] = p; while (*p && *p != ' ' && *p != '\t') p++; }
        if (*p) *p++ = 0;
    }
    return tln_main(argc, argv);
}
#define main tln_main
#endif

// ---------------------------------------------------------------- main loop
int main(int argc, char **argv) {
    int headless = 0;
    int test_frames = 0;
    const char *shot_dir = "shots";
    const char *start_night_arg = NULL;
    int nettest_mode = 0;
    const char *net_addr = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) test_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shot_dir = argv[++i];
        else if (!strcmp(argv[i], "--night") && i + 1 < argc) start_night_arg = argv[++i];
        else if (!strcmp(argv[i], "--tour")) headless = 1;
        else if (!strcmp(argv[i], "--autoplay")) headless = 1;
        else if (!strcmp(argv[i], "--nettest") && i + 1 < argc) {
            headless = 1;
            const char *m = argv[++i];
            if (!strcmp(m, "host")) nettest_mode = 1;
            else if (!strcmp(m, "client")) {
                nettest_mode = 2;
                if (i + 1 < argc) net_addr = argv[++i];
            }
        }
    }
    log_open(".");
    LOGI("runtime log opened");
    LOGI("THE LAST NIGHT %s starting (%s)", TLN_VERSION, headless ? "headless" : "windowed");

    if (headless) {
        if (!plat_init_headless(&g_plat, TLN_TITLE, 1280, 720)) {
            LOGE("platform init failed");
            return 1;
        }
    } else {
        if (!plat_init(&g_plat, TLN_TITLE, 1280, 720)) {
            LOGE("platform init failed");
            return 1;
        }
        file_set_asset_root(g_plat.exe_dir);
    }
    plat_set_title(&g_plat, TLN_TITLE " - " TLN_SUBTITLE ", " TLN_TAGLINE);

    if (!rd_init(&g_rd, 800, 450)) {
        LOGE("renderer init failed");
        return 1;
    }
    // asset root: relative to the executable when running from a build tree
    assets_load(&g_assets);
    game_init(&g_game, &g_plat, &g_rd, &g_assets);
    if (start_night_arg) {
        int n = atoi(start_night_arg);
        if (n >= 1 && n <= 10) {
            g_game.pending_night = n;
            game_start_day(&g_game, n);
            g_game.w->state = GS_DAY;
            g_game.w->state_time = 0.f;
        }
    }
    // audio
    g_plat.on_audio = audio_callback;
    g_plat.audio_user = &g_game;
    plat_audio_start(&g_plat, 1024);

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--tour")) {
            run_tour(&g_game, shot_dir);
            game_shutdown(&g_game);
            assets_free(&g_assets);
            rd_shutdown(&g_rd);
            plat_shutdown(&g_plat);
            return 0;
        }
        if (!strcmp(argv[i], "--nettest") && nettest_mode) {
            run_net_test(&g_game, nettest_mode, net_addr, 12.f);
            game_shutdown(&g_game);
            assets_free(&g_assets);
            rd_shutdown(&g_rd);
            plat_shutdown(&g_plat);
            return 0;
        }
    }
    double accum = 0.0;
    const double step = 1.0 / 60.0;
    int frame = 0;
    double t_start = plat_time();
    double next_shot = 0.0;
    int shot_index = 0;
    while (plat_poll(&g_plat)) {
        double now = plat_time();
        float dt = g_plat.dt;
        if (!headless) {
            if (dt > 0.1f) dt = 0.1f;
        } else {
            dt = (float)step;
        }
        accum += dt;
        int guard = 0;
        while (accum >= step && guard++ < 5) {
            accum -= step;
            game_frame(&g_game, (float)step);
            game_update_char_anims(&g_game, (float)step);
            if (g_game.w->net.running) net_update(&g_game, (float)step);
        }
        scene_draw(&g_game);
        ui_draw(&g_game);
        // F12 screenshot
        if (g_plat.in.pressed[K_F12]) {
            char path[600];
            snprintf(path, sizeof path, "%s/screenshot_%03d.png", headless ? shot_dir : g_plat.exe_dir, shot_index++);
            write_png(path, g_rd.color, g_rd.w, g_rd.h);
            LOGI("screenshot: %s", path);
        }
        if (headless && test_frames > 0) {
            // capture a frame every 30 steps for the automated report
            if (frame == 30 || frame == 120 || (frame > 240 && frame % 240 == 0)) {
                char path[600];
                snprintf(path, sizeof path, "%s/auto_%04d.png", shot_dir, frame);
                write_png(path, g_rd.color, g_rd.w, g_rd.h);
                LOGI("auto capture: %s (t=%.2fs state=%d night=%d zombies=%d)", path, plat_time() - t_start,
                     (int)g_game.w->state, g_game.w->night, g_game.w->zombie_count);
            }
        }
        (void)next_shot;
        // present
        if (!headless) {
            plat_present(&g_plat, g_rd.color, g_rd.w, g_rd.h);
            // simple frame limiter when vsync is off
            double target = now + 1.0 / 75.0;
            double t2 = plat_time();
            if (t2 < target) plat_sleep_ms((int)((target - t2) * 1000.0));
        }
        frame++;
        if (test_frames > 0 && frame >= test_frames) break;
        if (g_game.w->state == GS_MAIN_MENU && headless && test_frames > 0) {
            // drive the automated run into the game
            if (frame == 20) {
                game_new_run(&g_game);
            }
        }
    }
    LOGI("shutting down after %d frames (%.1fs)", frame, plat_time() - t_start);
    plat_audio_stop(&g_plat);
    game_shutdown(&g_game);
    assets_free(&g_assets);
    rd_shutdown(&g_rd);
    plat_shutdown(&g_plat);
    return 0;
}
