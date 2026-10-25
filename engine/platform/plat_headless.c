// Headless platform: fixed-timestep loop with no window. Used for automated
// testing on Linux (renders frames to PNG, runs the real game simulation).
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "platform.h"

static double g_start = 0.0;
static double g_time = 0.0;

double plat_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9 - g_start;
}

int plat_init(Platform *p, const char *title, int w, int h) {
    (void)title;
    return plat_init_headless(p, title, w, h);
}

int plat_init_headless(Platform *p, const char *title, int w, int h) {
    (void)title;
    memset(p, 0, sizeof *p);
    g_start = 0.0;
    g_time = 0.0;
    p->width = w;
    p->height = h;
    p->headless = 1;
    p->running = 1;
    p->vsync = 0;
    snprintf(p->exe_dir, sizeof p->exe_dir, ".");
    snprintf(p->save_dir, sizeof p->save_dir, ".");
    return 1;
}

void plat_shutdown(Platform *p) { p->running = 0; }

int plat_poll(Platform *p) {
    memset(p->in.pressed, 0, sizeof p->in.pressed);
    memset(p->in.released, 0, sizeof p->in.released);
    memset(p->in.mouse_pressed, 0, sizeof p->in.mouse_pressed);
    memset(p->in.mouse_released, 0, sizeof p->in.mouse_released);
    p->in.wheel = 0.f;
    p->in.text_len = 0;
    p->in.text[0] = 0;
    // fixed timestep so automated runs are deterministic
    g_time += 1.0 / 60.0;
    p->time = g_time;
    p->dt = 1.0f / 60.0f;
    p->frame_time = 1.0f / 60.0f;
    p->fps = 60.f;
    p->frame_index++;
    return !p->quit_requested;
}

void plat_present(Platform *p, const uint32_t *rgba, int w, int h) {
    (void)p; (void)rgba; (void)w; (void)h;
    // the headless harness writes PNGs itself; drop audio frames so the mixer
    // still runs at a realistic rate
    if (p->on_audio) {
        static int16_t buf[2048 * 2];
        int frames = 735;   // 44100 / 60
        memset(buf, 0, sizeof(int16_t) * 2 * (size_t)frames);
        p->on_audio(p->audio_user, buf, frames);
    }
}

void plat_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void plat_set_title(Platform *p, const char *title) {
    (void)p;
    printf("[title] %s\n", title);
    fflush(stdout);
}

void plat_toggle_fullscreen(Platform *p) { (void)p; }
void plat_capture_mouse(Platform *p, int capture) { (void)p; p->in.mouse_locked = capture; }
int plat_audio_start(Platform *p, int frames_per_buffer) { (void)frames_per_buffer; p->audio_running = 1; return 1; }
void plat_audio_stop(Platform *p) { p->audio_running = 0; }

void plat_log_path(Platform *p, char *out, size_t n, const char *name) {
    snprintf(out, n, "%s/%s", p->save_dir, name);
}

int plat_message_box(const char *title, const char *msg) {
    printf("[message] %s: %s\n", title, msg);
    return 1;
}
#endif
