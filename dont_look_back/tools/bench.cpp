// frame-time benchmark: real gameplay camera, window-resolution render target
#include "../src/dl_game.h"
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <vector>
using namespace nf; using namespace dl;

static double nowMS() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    int W = 1600, H = 900, frames = 120;
    const char* room = "corridor_main";
    if (argc > 1) W = atoi(argv[1]);
    if (argc > 2) H = atoi(argv[2]);
    if (argc > 3) frames = atoi(argv[3]);
    if (argc > 4) room = argv[4];

    Renderer R; Platform P; Audio A;
    P.headless = true; P.setHeadlessSize(W, H);
    R.init(W, H);
    A.init(44100);
    Game g;
    Input in;
    g.setInput(&in);
    if (!g.init(R, P, A, "assets")) { printf("init failed\n"); return 1; }
    if (const char* fs = getenv("NF_FIXSCALE")) { R.setAdaptive(false); R.setResolutionScale((float)atof(fs)); }
    g.onResize(W, H);
    g.testStartNewGame();
    g.teleportTo(room);
    g.testLook(0.0f, 0.0f);
    if (getenv("NF_NOFLASH")) g.playerRef().flashlightOn = false;

    // warm-up (JIT-free but warms caches / first-frame buffers)
    for (int i = 0; i < 10; ++i) { g.update(1.0f / 60.0f); g.render(); }

    std::vector<double> upd, ren;
    double t0 = nowMS();
    for (int i = 0; i < frames; ++i) {
        double a = nowMS();
        g.update(1.0f / 60.0f);
        double b = nowMS();
        g.render();
        double c = nowMS();
        upd.push_back(b - a); ren.push_back(c - b);
    }
    double total = nowMS() - t0;

    auto stat = [](std::vector<double>& v, const char* name) {
        std::sort(v.begin(), v.end());
        double sum = 0; for (double x : v) sum += x;
        printf("  %-7s avg %7.2f ms   p50 %7.2f   p95 %7.2f   max %7.2f\n",
               name, sum / v.size(), v[v.size() / 2], v[(size_t)(v.size() * 0.95)], v.back());
    };
    printf("bench %dx%d  %d frames  room=%s  items %d  tris %d\n", W, H, frames, room,
           R.drawCallCount(), R.triCount());
    stat(upd, "update");
    stat(ren, "render");
    printf("  total   %.2f ms/frame -> %.1f fps\n", total / frames, 1000.0 * frames / total);
    return 0;
}
