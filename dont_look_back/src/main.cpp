// ============================================================================
//  main.cpp : DON'T LOOK BACK -- entry point, window, frame loop
// ============================================================================
#include "dl_game.h"
#include <chrono>

using namespace nf;
using namespace dl;

// ---- headless self-test (also used by the test suite: --selftest)
namespace dl { class Game; }
int runSelfTest(dl::Game& game);

int main(int argc, char** argv) {
    std::string exeDirStr = exeDir();
    // asset dir: next to the exe, in "assets", or in the parent (dev layout)
    std::string assetDir;
    {
        const char* tries[] = {"assets", "Assets", "../assets", "../../assets"};
        for (auto t : tries) {
            std::string p = joinPath(exeDirStr, t);
            if (fileExists(joinPath(p, "fonts/sans_24.png").c_str())) { assetDir = p; break; }
        }
        if (assetDir.empty()) {
            for (auto t : tries) {
                if (fileExists(joinPath(t, "fonts/sans_24.png").c_str())) { assetDir = t; break; }
            }
        }
        if (assetDir.empty()) assetDir = joinPath(exeDirStr, "assets");
    }

    bool selftest = false;
    bool bench = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selftest") selftest = true;
        if (a == "--bench") bench = true;
    }

    logMsg("DON'T LOOK BACK -- NIGHTFALL engine");
    logMsg("exe dir: %s", exeDirStr.c_str());
    logMsg("asset dir: %s", assetDir.c_str());

    Renderer renderer;
    Platform platform;
    Audio audio;

    if (!audio.init(44100)) logMsg("[audio] mixer init failed");
    audio.setMasterVolume(0.9f);

    Game game;

    if (selftest || bench) {
        platform.headless = true;
        platform.setHeadlessSize(1280, 720);
        renderer.init(1280, 720);
        {
        Image icon;
        if (pngLoad(joinPath(assetDir, "ui/icon.png").c_str(), icon))
            platform.setWindowIconFromImage(icon);
    }
    if (!game.init(renderer, platform, audio, assetDir)) {
            logMsg("FATAL: init failed");
            return 2;
        }
        int rc = runSelfTest(game);
        game.shutdown();
        renderer.shutdown();
        return rc;
    }

    WindowOptions wo;
    wo.width = 1600;
    wo.height = 900;
    wo.title = "DON'T LOOK BACK";
    wo.fullscreen = false;
    wo.resizable = true;
    if (!platform.createWindow(wo)) {
        logMsg("FATAL: could not create window");
        return 1;
    }
    int W = 0, H = 0;
    platform.getSize(W, H);
    renderer.init(W, H);
    {
        Image icon;
        if (pngLoad(joinPath(assetDir, "ui/icon.png").c_str(), icon))
            platform.setWindowIconFromImage(icon);
    }
    if (!game.init(renderer, platform, audio, assetDir)) {
        platform.showMessage("DON'T LOOK BACK", "The game could not load its assets.\n\nExpected an 'assets' "
                                                "folder next to DONT_LOOK_BACK.exe.");
        return 2;
    }
    game.onResize(W, H);

    Input in;
    game.setInput(&in);

    auto prev = std::chrono::high_resolution_clock::now();
    double accum = 0.0;
    int frames = 0;
    bool running = true;
    while (running) {
        if (!platform.pumpEvents(in)) { running = false; break; }
        if (in.quit) { running = false; break; }
        if ((int)renderer.width() != 0) {
            int cw = 0, ch = 0;
            platform.getSize(cw, ch);
            if (cw != renderer.width() || ch != renderer.height()) {
                renderer.resize(cw, ch);
                game.onResize(cw, ch);
            }
        }
        auto now = std::chrono::high_resolution_clock::now();
        double dtReal = std::chrono::duration<double>(now - prev).count();
        prev = now;
        accum += dtReal;
        float dt = (float)std::min(dtReal, 0.05);
        if (accum < 1.0 / 120.0) { platform.sleepMs(1); continue; }
        accum = 0.0;

        game.update(dt);
        game.render();
        ++frames;
    }

    game.shutdown();
    renderer.shutdown();
    platform.destroyWindow();
    return 0;
}
