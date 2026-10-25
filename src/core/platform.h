/* ============================================================================
   SPACE MINER — platform.h
   Interface between the portable game and the host OS.
   ========================================================================== */
#ifndef SM_PLATFORM_H
#define SM_PLATFORM_H
#include "common.h"
#include "gfx.h"

/* Virtual resolution the game always renders at. */
#define SM_SCREEN_W 1280
#define SM_SCREEN_H 720

#define SM_KEY_COUNT 128

typedef struct {
    int down[SM_KEY_COUNT];
    int pressed[SM_KEY_COUNT];
    int released[SM_KEY_COUNT];
    Vec2 mouse;          /* virtual screen coords */
    Vec2 mouse_delta;
    int mouse_down[3];
    int mouse_pressed[3];
    int mouse_released[3];
    f32 wheel;
    u8 text[32];         /* typed characters this frame */
    int text_len;
    int quit;
} Input;

/* Key codes (platform layers translate to these). */
enum {
    SM_KEY_NONE = 0,
    SM_KEY_ESCAPE = 27,
    SM_KEY_SPACE = 32,
    SM_KEY_LEFT = 1000, SM_KEY_RIGHT, SM_KEY_UP, SM_KEY_DOWN,
    SM_KEY_TAB, SM_KEY_ENTER, SM_KEY_BACKSPACE, SM_KEY_SHIFT, SM_KEY_CTRL,
    SM_KEY_F1, SM_KEY_F2, SM_KEY_F3, SM_KEY_F4, SM_KEY_F5, SM_KEY_F6,
    SM_KEY_F11, SM_KEY_F12,
    SM_KEY_A = 'a', SM_KEY_B, SM_KEY_C, SM_KEY_D, SM_KEY_E, SM_KEY_F, SM_KEY_G,
    SM_KEY_H, SM_KEY_I, SM_KEY_J, SM_KEY_K, SM_KEY_L, SM_KEY_M, SM_KEY_N, SM_KEY_O,
    SM_KEY_P, SM_KEY_Q, SM_KEY_R, SM_KEY_S, SM_KEY_T, SM_KEY_U, SM_KEY_V, SM_KEY_W,
    SM_KEY_X, SM_KEY_Y, SM_KEY_Z,
    SM_KEY_0 = '0', SM_KEY_1, SM_KEY_2, SM_KEY_3, SM_KEY_4, SM_KEY_5, SM_KEY_6,
    SM_KEY_7, SM_KEY_8, SM_KEY_9
};
/* Map arbitrary platform key id into the 0..127 table. */
int sm_key_index(int key);

typedef struct {
    /* files: save directory, resolved once at startup */
    void (*read_file)(const char *name, u8 **outData, usize *outLen, void *user);
    int  (*write_file)(const char *name, const void *data, usize len, void *user);
    int  (*file_exists)(const char *name, void *user);
    void (*log)(const char *msg, void *user);
    double (*now)(void *user);
    void (*sleep_ms)(int ms, void *user);
    /* screenshot (optional) */
    void (*save_screenshot)(const u32 *px, int w, int h, void *user);
    void *user;
    int headless;
    const char *save_dir;
} SmPlatform;

/* Implemented by each platform layer. */
int  plat_init(void);
void plat_shutdown(void);
/* Fetch the asset pack bytes (embedded or from disk). */
const u8 *plat_assets(usize *outLen);

void input_begin_frame(Input *in);
void input_end_frame(Input *in);
void input_key_event(Input *in, int key, int down);
void input_mouse_move(Input *in, f32 x, f32 y);
void input_mouse_button(Input *in, int button, int down);
void input_clear(Input *in);

#endif
