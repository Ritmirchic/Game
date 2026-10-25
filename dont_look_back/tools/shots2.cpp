#include "../src/dl_game.h"
using namespace nf; using namespace dl;
static void save(Renderer& R, const char* name) {
    Image img(R.width(), R.height());
    memcpy(img.px.data(), R.pixels(), (size_t)R.width() * R.height() * 4);
    pngWrite(name, img); printf("wrote %s\n", name); fflush(stdout);
}
int main() {
    Renderer R; Platform P; Audio A;
    P.headless = true; P.setHeadlessSize(1152, 648);
    R.init(1152, 648); A.init(44100);
    Game g; Input in; g.setInput(&in);
    g.init(R, P, A, "assets"); g.onResize(1152, 648);
    g.testStartNewGame();
    auto run = [&](int f) { for (int i = 0; i < f; ++i) { g.update(1.0f / 60.0f); g.render(); } };
    struct S { const char* room; float yaw, pitch; const char* file; };
    S ss[] = {
        {"corridor_main", 0.0f, -0.02f, "shots/t_01_corridor.png"},
        {"apt47_living", 1.9f, -0.06f, "shots/t_02_living.png"},
        {"apt47_bed", 0.4f, -0.05f, "shots/t_03_bed.png"},
        {"lobby", 3.0f, -0.02f, "shots/t_04_lobby.png"},
        {"stairwell", 3.6f, 0.06f, "shots/t_05_stairwell.png"},
        {"maintenance", 1.0f, -0.02f, "shots/t_06_maintenance.png"},
        {"basement_corridor", 0.0f, -0.02f, "shots/t_07_basement.png"},
    };
    for (auto& s : ss) { g.teleportTo(s.room); g.testLook(s.yaw, s.pitch); run(6); save(R, s.file); }
    g.teleportTo("corridor_main"); g.testLook(0, 0); run(4);
    g.forceAnomaly(25, true); run(16); save(R, "shots/t_08_entity.png"); g.forceAnomaly(25, false);
    g.testSetState(GameState::Journal); run(3); save(R, "shots/t_09_journal.png");
    return 0;
}
