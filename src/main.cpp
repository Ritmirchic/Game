// FRACTURE: TOTAL DESTRUCTION — entry point, renderer selection and main loop.
#include "core/engine.h"
#include "game/game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

static Game g_gameInstance;

struct LaunchOptions {
    bool forceSoft = false;
    bool forceGL = false;
    int startMap = -1;
    bool sandbox = false;
    int mission = -1;
    const char* screenshotDir = nullptr;
    int shotFrames = 0;
    int shots = 0;
    bool headless = false;
    bool quitAfter = false;
    float seconds = 0;
};

static LaunchOptions parseArgs(int argc, char** argv) {
    LaunchOptions o;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : nullptr; };
        if (!strcmp(a, "--soft")) o.forceSoft = true;
        else if (!strcmp(a, "--gl")) o.forceGL = true;
        else if (!strcmp(a, "--headless")) o.headless = true;
        else if (!strcmp(a, "--map")) { const char* v = next(); if (v) o.startMap = atoi(v); }
        else if (!strcmp(a, "--sandbox")) o.sandbox = true;
        else if (!strcmp(a, "--mission")) { const char* v = next(); if (v) o.mission = atoi(v); }
        else if (!strcmp(a, "--out")) o.screenshotDir = next();
        else if (!strcmp(a, "--shot-frames")) { const char* v = next(); if (v) o.shotFrames = atoi(v); }
        else if (!strcmp(a, "--shots")) { const char* v = next(); if (v) o.shots = atoi(v); }
        else if (!strcmp(a, "--seconds")) { const char* v = next(); if (v) o.seconds = (float)atof(v); }
    }
    o.quitAfter = (o.shots > 0) || (o.seconds > 0) || o.headless;
    return o;
}

int main(int argc, char** argv) {
    LaunchOptions opt = parseArgs(argc, argv);
    Game& game = g_gameInstance;
    Settings& st = game.settings;
    st.resolutionW = 1600;
    st.resolutionH = 900;
    bool haveConfig = false;
#if defined(_WIN32)
    haveConfig = cfg::load(st);
#else
    cfg::load(st);
#endif
    if (opt.forceSoft) st.rendererMode = 1;
    if (opt.forceGL) st.rendererMode = 0;

    // headless verification runs use a smaller frame so the software rasteriser keeps up
    bool headless = opt.headless || opt.forceSoft;
    if (headless) {
        st.resolutionW = 1280;
        st.resolutionH = 720;
        st.showFps = false;
    }
    (void)haveConfig;
#if !defined(_WIN32)
    // no GL driver in CI / Linux containers: the software renderer is the reference implementation
    headless = true;
    opt.forceSoft = true;
    st.resolutionW = 1280;
    st.resolutionH = 720;
    st.showFps = false;
#endif
    if (!platform::init("FRACTURE: TOTAL DESTRUCTION", st.resolutionW, st.resolutionH, st)) {
        platform::messageBox("FRACTURE", "Failed to create the game window.");
        return 1;
    }
    logMsg("[main] platform ready %dx%d", st.resolutionW, st.resolutionH);

    IRenderer* renderer = nullptr;
#if defined(_WIN32)
    // rendererMode 1 = user forced the software reference renderer (broken GL driver, VM, ...)
    if (st.rendererMode != 1) {
        renderer = createGLRenderer();
        if (renderer && !renderer->init(platform::hwnd(), st.resolutionW, st.resolutionH, st)) {
            logMsg("[main] GL renderer init failed, falling back to software");
            delete renderer;
            renderer = nullptr;
        }
    }
#endif
    if (!renderer) {
        renderer = createSoftRenderer();
        if (!renderer->init(nullptr, st.resolutionW, st.resolutionH, st)) {
            platform::messageBox("FRACTURE", "No renderer could be initialised.");
            return 1;
        }
    }
    st.rendererMode = (strstr(renderer->name(), "OpenGL") != nullptr) ? 0 : 1;
    logMsg("[main] renderer: %s", renderer->name());

    if (!game.init(renderer)) {
        platform::messageBox("FRACTURE", "Game initialisation failed.");
        return 1;
    }

    // launch overrides -------------------------------------------------------
    if (opt.sandbox && opt.startMap >= 0) {
        game.sandbox.timeOfDay = defaultTimeOfDay(opt.startMap);
        game.startSandbox(opt.startMap);
    } else if (opt.mission >= 0) {
        game.startMission(opt.mission);
    } else if (opt.startMap >= 0) {
        game.sandbox.timeOfDay = defaultTimeOfDay(opt.startMap);
        game.startSandbox(opt.startMap);
    }

    InputState input;
    double prev = platform::time();
    double startTime = prev;
    int frame = 0;
    int shotIndex = 0;
    double lastShot = -1000;
    int winW = st.resolutionW, winH = st.resolutionH;

    while (!platform::shouldQuit()) {
        if (!platform::poll(input)) break;
        double now = platform::time();
        float dt = (float)(now - prev);
        prev = now;
        if (dt > 0.12f) dt = 0.12f;      // never simulate more than 120 ms in one step
        if (dt < 0.0005f) dt = 0.0005f;

        // window resize
        int cw = platform::windowWidth(), ch = platform::windowHeight();
        if (cw > 0 && ch > 0 && (cw != winW || ch != winH)) {
            winW = cw;
            winH = ch;
            game.resize(cw, ch);
        }

        game.input = input;
        game.update(dt);
        game.render();

        if (strstr(renderer->name(), "Software") != nullptr) {
            platform::presentSoftware(renderer->framebuffer(), renderer->frameWidth(), renderer->frameHeight());
        }
        platform::swapBuffers();

        // ---- automated screenshot mode ----
        if (opt.screenshotDir && frame >= opt.shotFrames && (frame - (int)lastShot) >= 36 && shotIndex < opt.shots) {
            char path[512];
            snprintf(path, sizeof(path), "%s/shot%02d.png", opt.screenshotDir, shotIndex);
            renderer->screenshot(path);
            logMsg("[shot] %s", path);
            lastShot = frame;
            shotIndex++;
            // advance through one showcase scene per shot
            if (shotIndex == 1) {
                game.startSandbox(0);
                game.sandbox.timeOfDay = 15;
                game.player.pos = vec3(52, 3.4f, 34);
                game.player.yaw = 2.1f;
                game.player.flashlightOn = false;
            } else if (shotIndex == 2) {
                game.explode(vec3(66, 3.0f, 44), 5.0f, 0);
                game.player.pos = vec3(52, 3.4f, 34);
                game.player.yaw = 2.0f;
            } else if (shotIndex == 3) {
                game.startSandbox(1);
                game.sandbox.timeOfDay = 10;
                game.player.pos = vec3(64, 4.0f, 70);
                game.player.yaw = 1.2f;
            } else if (shotIndex == 4) {
                game.startSandbox(2);
                game.sandbox.timeOfDay = 8;
                game.player.pos = vec3(60, 3.4f, 44);
                game.player.yaw = 0.4f;
            } else if (shotIndex == 5) {
                game.startSandbox(3);
                game.sandbox.timeOfDay = 13;
                game.player.pos = vec3(46, 3.4f, 40);
                game.player.yaw = 1.9f;
            } else if (shotIndex == 6) {
                game.startSandbox(4);
                game.sandbox.timeOfDay = 16;
                game.player.pos = vec3(70, 3.6f, 78);
                game.player.yaw = 2.4f;
            } else if (shotIndex == 7) {
                game.startSandbox(5);
                game.sandbox.timeOfDay = 12;
                game.player.pos = vec3(44, 3.4f, 36);
                game.player.yaw = 1.55f;
            }
        }
        if (opt.quitAfter) {
            bool doneShots = opt.shots > 0 && shotIndex >= opt.shots;
            bool doneTime = opt.seconds > 0 && (now - startTime) >= opt.seconds;
            if (doneShots || doneTime) break;
        }
        frame++;
        if (st.fpsLimit > 0) {
            double minFrame = 1.0 / st.fpsLimit;
            double spent = platform::time() - now;
            if (spent < minFrame) platform::sleepMs((int)((minFrame - spent) * 1000.0));
        } else if (st.rendererMode == 1 && !opt.quitAfter) {
            // software renderer on a real window: keep the frame rate sane
            double spent = platform::time() - now;
            if (spent < 0.033) platform::sleepMs((int)((0.033 - spent) * 1000.0));
        }
    }
    logMsg("[main] shutting down after %d frames", frame);
    st.rendererMode = (strstr(renderer->name(), "OpenGL") != nullptr) ? 0 : 1;
    cfg::save(st);
    game.shutdown();
    renderer->shutdown();
    delete renderer;
    platform::shutdown();
    logFlush();
    return 0;
}
