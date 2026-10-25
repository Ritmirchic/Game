// ============================================================
// SPACE MINER  -  platform_headless.cpp
// Host used for automated tests and the screenshot pipeline.
// ============================================================
#include "platform.h"
#include <cstdio>
#include <chrono>

namespace sm {

static std::string gDir = "./saves/";
static std::chrono::steady_clock::time_point gStart = std::chrono::steady_clock::now();
static Canvas* gPresentTarget = nullptr;

bool platformInit(const char* title, int w, int h) {
    (void)title; (void)w; (void)h;
    gStart = std::chrono::steady_clock::now();
    system("mkdir -p ./saves");
    return true;
}
void platformShutdown() {}

bool platformPump(Input& in, float& dt) { in.beginFrame(); dt = 1.f / 60.f; return true; }
void platformPresent(Canvas& fb) { if (gPresentTarget) fb.px = gPresentTarget->px; }
void platformSetTitle(const char*) {}
void platformSetIcon(int, const uint32_t*) {}
void platformSetBorderless(bool) {}
const char* platformSaveDir() { return gDir.c_str(); }
void platformSleepMs(int ms) { (void)ms; }
double platformSeconds() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - gStart).count();
}


}  // namespace sm
