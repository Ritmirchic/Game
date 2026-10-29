// Headless platform (Linux) — used for automated tests, screenshots and CI.
#ifdef __linux__
#include "core/engine.h"
#include <ctime>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <string>

namespace platform {
static bool g_quit = false;
static int g_w = 1280, g_h = 720;
static bool g_init = false;
static double g_start = 0;
static double limitSec = 60.0;

static double nowSec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

bool init(const char* title, int w, int h, const Settings& st) {
    (void)title; (void)st;
    g_w = w; g_h = h;
    g_init = true;
    g_start = nowSec();
    const char* lim = getenv("FRACTURE_TEST_SECONDS");
    if (lim) limitSec = atof(lim);
    static bool logged = false;
    if (!logged) { logged = true; logMsg("[platform] headless init %dx%d", w, h); }
    return true;
}
void shutdown() { g_init = false; }
bool poll(InputState& in) {
    in.clearFrame();
    if (nowSec() - g_start > limitSec) g_quit = true;
    return !g_quit;
}
void swapBuffers() {}
double time() { return nowSec(); }
void sleepMs(int ms) { usleep(ms * 1000); }
void setTitle(const char*) {}
void* hwnd() { return nullptr; }
void* glContext() { return nullptr; }
std::string dataDir() {
    static std::string d;
    if (d.empty()) {
        const char* env = getenv("FRACTURE_DATA_DIR");
        d = env ? env : "./.fracture";
        mkdir(d.c_str(), 0755);
    }
    return d;
}
std::string exeDir() { return "."; }
void messageBox(const char* t, const char* m) { logMsg("[msgbox] %s: %s", t, m); }
void setMouseCapture(bool) {}
bool mouseCaptured() { return false; }
void setFullscreen(int, int, int) {}
void toggleFullscreen() {}
int windowWidth() { return g_w; }
int windowHeight() { return g_h; }
bool shouldQuit() { return g_quit; }
void requestQuit() { g_quit = true; }
void presentSoftware(const uint32_t*, int, int) {}
}

// ---------------- logging ----------------
#include <cstdarg>
#include <cstdio>
static FILE* g_logFile = nullptr;
static bool g_logTried = false;
void logMsg(const char* fmt, ...) {
    if (!g_logTried) {
        g_logTried = true;
        std::string path = platform::dataDir() + "/fracture.log";
        g_logFile = fopen(path.c_str(), "w");
    }
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    if (g_logFile) { fprintf(g_logFile, "%s\n", buf); fflush(g_logFile); }
}
void logFlush() { if (g_logFile) fflush(g_logFile); }

// ---------------- audio stubs (Linux builds are headless) ----------------
namespace audio {
bool init() { return false; }
void shutdown() {}
void mix(float* out, int frames) { std::memset(out, 0, sizeof(float) * 2 * frames); }
void play(SfxId, vec3, float, float, bool, int) {}
int playLoop(SfxId, vec3, float, float) { return -1; }
void updateLoop(int, vec3, float, float) {}
void stopLoop(int) {}
void setListener(vec3, vec3, vec3) {}
void setVolumes(float, float, float) {}
void startMusic(int) {}
void stopMusic() {}
void setMusicIntensity(float) {}
int activeVoices() { return 0; }
void update(float) {}
}

#endif
