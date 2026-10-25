// Platform layer — Win32 (shipping target) + headless (build/CI verification)
#ifndef TLN_PLATFORM_H
#define TLN_PLATFORM_H

#include "../src/core.h"

typedef struct Audio Audio;

#define KEY_COUNT 256
// special key codes beyond ASCII (values chosen to fit in a byte)
enum {
    K_ESC = 27, K_ENTER = 13, K_TAB = 9, K_BACKSPACE = 8, K_SPACE = 32,
    K_SHIFT = 0x80, K_CTRL, K_ALT, K_LEFT, K_RIGHT, K_UP, K_DOWN,
    K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_INSERT, K_DELETE, K_HOME, K_END, K_PGUP, K_PGDN, K_PLUS, K_MINUS, K_GRAVE
};

typedef struct Input {
    uint8_t down[KEY_COUNT];
    uint8_t pressed[KEY_COUNT];
    uint8_t released[KEY_COUNT];
    float mouse_dx, mouse_dy;
    float mouse_x, mouse_y;
    uint8_t mouse_down[3];
    uint8_t mouse_pressed[3];
    uint8_t mouse_released[3];
    float wheel;
    char text[32];
    int text_len;
    int mouse_locked;
} Input;

typedef struct Platform {
    int width, height;
    int headless;
    int running;
    double time, dt, frame_time;
    Input in;
    char exe_dir[512];
    char save_dir[512];
    void *user;
    void (*on_audio)(void *user, int16_t *buf, int frames);
    void *audio_user;
    int vsync;
    int fullscreen;
    int screenshot_request;
    char screenshot_path[512];
    float fps;
    int frame_index;
    int quit_requested;
    int windowed_w, windowed_h;
    void *hwnd;
    void *hdc_dib;
    void *bmp;
    void *hmem;
    uint32_t *dib_pixels;
    int audio_running;
    int mouse_captured;
    int last_mx, last_my;
    int raw_input;
} Platform;

int  plat_init(Platform *p, const char *title, int w, int h);
int  plat_init_headless(Platform *p, const char *title, int w, int h);
void plat_shutdown(Platform *p);
// returns 1 while the game should keep running
int  plat_poll(Platform *p);
void plat_present(Platform *p, const uint32_t *rgba, int w, int h);
void plat_sleep_ms(int ms);
double plat_time(void);
void plat_set_title(Platform *p, const char *title);
void plat_toggle_fullscreen(Platform *p);
void plat_capture_mouse(Platform *p, int capture);
int  plat_audio_start(Platform *p, int frames_per_buffer);
void plat_audio_stop(Platform *p);
void plat_log_path(Platform *p, char *out, size_t n, const char *name);
int  plat_message_box(const char *title, const char *msg);

#endif
