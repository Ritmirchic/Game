// ============================================================
// SPACE MINER  -  main.cpp
// Entry point, frame loop, application icon.
// ============================================================
#include "game.h"
#include "content.h"
#include "art.h"
#include "platform.h"

using namespace sm;

// application icon: stylised hex hull with a cutting beam
static void buildAppIcon(Canvas& c, int size) {
    c.resize(size, size);
    c.clear(0);
    float s = size / 64.f;
    // rounded dark plate
    float r = 12.f * s;
    c.circleFill(size * .5f, size * .5f, size * .5f, rgba(10, 16, 26, 255));
    c.circleFill(size * .5f, size * .5f, size * .5f - 2 * s, rgba(16, 26, 40, 255));
    Vec2 hex[6];
    for (int i = 0; i < 6; i++) {
        float a = i * TAU / 6 + PI / 6;
        hex[i] = Vec2(size * .5f + cosf(a) * 20.f * s, size * .5f + sinf(a) * 20.f * s);
    }
    c.polyFill(hex, 6, rgba(60, 84, 108, 255));
    c.polyOutline(hex, 6, rgba(150, 210, 235, 255), 1.6f * s);
    // cutter beam
    c.line({size * .5f - 6 * s, size * .5f - 6 * s}, {size * .5f + 22 * s, size * .5f + 22 * s},
           rgba(130, 240, 255, 255), 5.f * s);
    c.line({size * .5f - 6 * s, size * .5f - 6 * s}, {size * .5f + 22 * s, size * .5f + 22 * s},
           rgba(255, 255, 255, 255), 2.f * s);
    c.radialGlow(size * .5f - 4 * s, size * .5f - 4 * s, 18 * s, rgba(120, 230, 255, 180), 1.f);
    c.radialGlow(size * .5f + 22 * s, size * .5f + 22 * s, 16 * s, rgba(255, 220, 140, 200), 1.f);
    // mineral spark
    Vec2 star[8];
    for (int i = 0; i < 8; i++) {
        float a = i * TAU / 8;
        float rr = (i % 2 == 0) ? 9.f * s : 3.5f * s;
        star[i] = Vec2(size * .5f + cosf(a) * rr + 13 * s, size * .5f + sinf(a) * rr - 13 * s);
    }
    c.polyFill(star, 8, rgba(255, 236, 170, 235));
}

static int runGame() {
    if (!platformInit("SPACE MINER", VW, VH)) return 1;
    // show something immediately, asset generation takes a moment
    Canvas fb(VW, VH);
    fb.clear(rgba(4, 7, 13));
    drawHeading(fb, VW / 2, VH / 2 - 40, "SPACE MINER", rgba(232, 248, 255), rgba(90, 210, 255), 5, 8);
    drawTextCenter(fb, VW / 2, VH / 2 + 20, "GENERATING ASSETS", rgba(140, 190, 215), 1, 2);
    platformPresent(fb);

    Game g;
    g.headless = false;
    // staged asset generation with a live loading screen
    for (int stage = 0; stage < 5; stage++) {
        g.bootStage(stage);
        g.renderLoading(fb);
        platformPresent(fb);
    }
    g.bootFinish();
    {
        Canvas icon64, icon32;
        buildAppIcon(icon64, 64);
        buildAppIcon(icon32, 32);
        platformSetIcon(64, icon64.px.data());
        platformSetIcon(32, icon32.px.data());
    }
    audio::setMasterVolume(g.save.settings.masterVolume);
    platformSetBorderless(g.save.settings.fullscreen == 1);

    Input in;
    while (true) {
        float dt = 1.f / 60.f;
        if (!platformPump(in, dt)) break;
        if (dt > .1f) dt = .1f;
        g.update(dt, in);
        g.render(fb);
        platformPresent(fb);
        if (g.wantsQuit()) break;
    }
    g.shutdown();
    platformShutdown();
    return 0;
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return runGame(); }
#else
int main(int, char**) { return runGame(); }
#endif
