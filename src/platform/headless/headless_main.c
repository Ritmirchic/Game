/* ============================================================================
   SPACE MINER — platform/headless/headless_main.c
   Development harness: runs the game without a window so the simulation,
   rendering and asset pipeline can be exercised and screenshotted in CI.
     sm_headless --shots            render a set of showcase frames
     sm_headless --test             run the automated test suite
     sm_headless --soak <seconds>   long unattended simulation
   ========================================================================== */
#include <time.h>
#include "../../core/platform.h"
#include "../../core/state.h"
#include "../../core/audio.h"
#include "../../core/png.h"

static char g_root[512];
static int g_shots_written = 0;

static void hlog(const char *msg, void *user) {
    (void)user;
    printf("%s\n", msg);
}

static void hread(const char *name, u8 **outData, usize *outLen, void *user) {
    (void)user;
    char path[1024];
    sm_sprintf(path, sizeof(path), "%s/%s", g_root, name);
    FILE *f = fopen(path, "rb");
    if (!f) { *outData = NULL; if (outLen) *outLen = 0; return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *buf = (u8 *)malloc((usize)sz + 1);
    size_t got = fread(buf, 1, (usize)sz, f);
    fclose(f);
    buf[got] = 0;
    *outData = buf;
    if (outLen) *outLen = got;
}

static int hwrite(const char *name, const void *data, usize len, void *user) {
    (void)user;
    char path[1024];
    sm_sprintf(path, sizeof(path), "%s/%s", g_root, name);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t put = fwrite(data, 1, len, f);
    fclose(f);
    return put == len ? 0 : -1;
}

static int hexists(const char *name, void *user) {
    (void)user;
    char path[1024];
    sm_sprintf(path, sizeof(path), "%s/%s", g_root, name);
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static double hnow(void *user) { (void)user; return (double)clock() / (double)CLOCKS_PER_SEC; }
static void hsleep(int ms, void *user) { (void)user; (void)ms; }

static void hscreenshot(const u32 *px, int w, int h, void *user) {
    (void)user;
    char name[128];
    sm_sprintf(name, sizeof(name), "shot_%03d.png", g_shots_written++);
    char path[1200];
    sm_sprintf(path, sizeof(path), "%s/%s", g_root, name);
    usize len = 0;
    u8 *png = png_encode(px, w, h, &len);
    if (!png) return;
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(png, 1, len, f); fclose(f); }
    free(png);
    sm_logf("wrote %s", path);
}

/* written by the scenario module */
int  sm_test_run(const SmPlatform *plat, const Pak *pak, Gfx *gfx);
int  sm_shots_run(const SmPlatform *plat, const Pak *pak, Gfx *gfx);
int  sm_soak_run(const SmPlatform *plat, const Pak *pak, Gfx *gfx, f32 seconds);

const u8 *plat_assets(usize *outLen) { (void)outLen; return NULL; }

int main(int argc, char **argv) {
    const char *root = ".";
    const char *mode = "--test";
    f32 soak = 60.f;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--root") && i + 1 < argc) root = argv[++i];
        else if (!strcmp(argv[i], "--shots")) mode = "--shots";
        else if (!strcmp(argv[i], "--test")) mode = "--test";
        else if (!strcmp(argv[i], "--soak")) { mode = "--soak"; if (i + 1 < argc) soak = (f32)atof(argv[++i]); }
    }
    sm_strlcpy(g_root, root, sizeof(g_root));

    SmPlatform plat;
    memset(&plat, 0, sizeof(plat));
    plat.read_file = hread;
    plat.write_file = hwrite;
    plat.file_exists = hexists;
    plat.log = hlog;
    plat.now = hnow;
    plat.sleep_ms = hsleep;
    plat.save_screenshot = hscreenshot;
    plat.headless = 1;
    plat.save_dir = g_root;
    sm_log_set(hlog, NULL);

    u8 *pakBytes = NULL;
    usize pakLen = 0;
    hread("assets.pak", &pakBytes, &pakLen, NULL);
    if (!pakBytes || !pakLen) { sm_errorf("assets.pak not found in '%s'", g_root); return 2; }
    sm_logf("asset pack: %.2f MB", (double)pakLen / (1024.0 * 1024.0));

    Pak pak;
    if (pak_open(&pak, pakBytes, pakLen) != 0) { sm_errorf("pak_open failed"); return 2; }

    audio_init();
    audio_load_all(&pak);


    Gfx gfx;
    gfx_init(&gfx, SM_SCREEN_W, SM_SCREEN_H);


    int rc = 0;
    if (!strcmp(mode, "--shots")) rc = sm_shots_run(&plat, &pak, &gfx);
    else if (!strcmp(mode, "--soak")) rc = sm_soak_run(&plat, &pak, &gfx, soak);
    else rc = sm_test_run(&plat, &pak, &gfx);
    return rc;
}
