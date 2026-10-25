// screenshot harness: renders real gameplay frames (HUD included) to shots/
#include "../src/dl_game.h"
#include <cstdio>
using namespace nf; using namespace dl;

static void save(Renderer& R, const char* name) {
    Image img(R.width(), R.height());
    memcpy(img.px.data(), R.pixels(), (size_t)R.width() * R.height() * 4);
    pngWrite(name, img);
    printf("wrote %s\n", name); fflush(stdout);
}

int main() {
    Renderer R; Platform P; Audio A;
    P.headless = true; P.setHeadlessSize(1152, 648);
    R.init(1152, 648);
    A.init(44100);
    Game g;
    Input in;
    g.setInput(&in);
    if (!g.init(R, P, A, "assets")) { printf("init failed\n"); return 1; }
    g.onResize(1152, 648);
    g.testStartNewGame();
    auto run = [&](int frames) { for (int i = 0; i < frames; ++i) { g.update(1.0f / 60.0f); g.render(); } };

    struct Shot { const char* room; float yaw; float pitch; const char* file; };
    Shot shots[] = {
        {"apt47_living", 0.6f, -0.05f, "shots/g_01_living.png"},
        {"apt47_living", 3.5f, -0.05f, "shots/g_02_living_back.png"},
        {"apt47_bed", -1.2f, -0.08f, "shots/g_03_bedroom.png"},
        {"apt47_kitchen", 2.2f, -0.15f, "shots/g_04_kitchen.png"},
        {"apt47_hall", 1.9f, 0.0f, "shots/g_05_hall.png"},
        {"corridor_main", 0.0f, -0.02f, "shots/g_06_corridor.png"},
        {"corridor_main", 3.14159f, 0.0f, "shots/g_07_corridor_back.png"},
        {"corridor_north", 1.6f, 0.0f, "shots/g_08_corridor_north.png"},
        {"lobby", 1.2f, -0.02f, "shots/g_09_lobby.png"},
        {"stairwell", 2.4f, 0.10f, "shots/g_10_stairwell.png"},
        {"maintenance", 2.0f, -0.05f, "shots/g_11_maintenance.png"},
        {"boiler", 0.3f, -0.05f, "shots/g_12_boiler.png"},
        {"laundry", -0.9f, -0.05f, "shots/g_13_laundry.png"},
        {"storageA", 1.0f, -0.05f, "shots/g_14_storage.png"},
        {"hidden", 0.4f, -0.05f, "shots/g_15_hidden.png"},
        {"basement_corridor", 0.2f, -0.02f, "shots/g_16_basement.png"},
    };
    for (auto& s : shots) {
        if (!g.teleportTo(s.room)) printf("  (room %s missing)\n", s.room);
        g.testLook(s.yaw, s.pitch);
        run(8);
        save(R, s.file);
    }
    // entity shot
    g.teleportTo("corridor_main"); g.testLook(0.0f, 0.0f); run(4);
    g.forceAnomaly(25, true);
    run(20);
    save(R, "shots/g_17_entity.png");
    g.forceAnomaly(25, false);
    // anomaly shows
    struct AnomShot { int id; const char* room; float yaw; const char* file; };
    AnomShot as[] = {
        {0, "apt47_living", 3.4f, "shots/g_a00_painting.png"},
        {3, "corridor_main", 0.2f, "shots/g_a03_door.png"},
        {4, "apt47_living", 0.6f, "shots/g_a04_wallpaper.png"},
        {6, "corridor_main", 0.0f, "shots/g_a06_redlight.png"},
        {19, "corridor_north", 1.57f, "shots/g_a19_corridorlong.png"},
        {23, "corridor_main", 3.1f, "shots/g_a23_blocked.png"},
        {26, "apt47_living", 2.0f, "shots/g_a26_shadow.png"},
    };
    for (auto& a : as) {
        g.teleportTo(a.room);
        g.testLook(a.yaw, -0.03f);
        g.forceAnomaly(a.id, true);
        run(10);
        save(R, a.file);
        g.forceAnomaly(a.id, false);
        run(2);
    }
    // UI screens
    g.testSetState(GameState::Journal); run(3); save(R, "shots/g_20_journal.png");
    g.testOpenDoc(0); run(3); save(R, "shots/g_21_document.png");
    g.testSetState(GameState::Paused); g.testSetMenu(1); run(3); save(R, "shots/g_22_pause.png");
    g.testSetState(GameState::Settings); run(3); save(R, "shots/g_23_settings.png");
    g.testStartNewGame(); g.testForceEndingScreen(EndingId::Escape); run(200); save(R, "shots/g_24_ending_escape.png");
    g.testStartNewGame(); g.testForceEndingScreen(EndingId::Descent); run(200); save(R, "shots/g_25_ending_descent.png");
    g.testStartNewGame(); g.testForceEndingScreen(EndingId::Assimilation); run(200); save(R, "shots/g_26_ending_assimilation.png");
    g.testReturnToTitle(); run(30); save(R, "shots/g_27_title.png");
    printf("done\n");
    return 0;
}
