// FRACTURE — menus and full-screen states.
#include "game/game.h"
#include "game/globals.h"
#include "ui/ui.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

using namespace ui;

static const char* kWeatherNames2[5] = {"CLEAR", "CLOUDY", "RAIN", "STORM", "FOG"};
static const char* kResNames2[6] = {"1280x720", "1600x900", "1920x1080", "2560x1440", "3440x1440", "3840x2160"};
static const int kResW2[6] = {1280, 1600, 1920, 2560, 3440, 3840};
static const int kResH2[6] = {720, 900, 1080, 1440, 1440, 2160};
static const char* kQual[4] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
static const char* kWindowModes[3] = {"WINDOWED", "BORDERLESS", "FULLSCREEN"};
static const char* kAAModes[3] = {"OFF", "FXAA", "MSAA 4X"};
static const int kDebrisMax[4] = {0, 1, 2, 3};

void setPreset(Settings& s, int q) {
    s.qualityPreset = q;
    switch (q) {
        case QP_LOW:
            s.shadowQuality = 1; s.aaMode = 0; s.viewDistance = 90; s.destructionQuality = 0;
            s.debrisDensity = 0; s.physicsQuality = 0.5f; s.particleQuality = 0; s.renderScale = 0.8f;
            s.motionBlur = false;
            break;
        case QP_MEDIUM:
            s.shadowQuality = 2; s.aaMode = 1; s.viewDistance = 130; s.destructionQuality = 1;
            s.debrisDensity = 1; s.physicsQuality = 1.0f; s.particleQuality = 1; s.renderScale = 1.0f;
            break;
        case QP_HIGH:
            s.shadowQuality = 2; s.aaMode = 1; s.viewDistance = 180; s.destructionQuality = 2;
            s.debrisDensity = 2; s.physicsQuality = 1.5f; s.particleQuality = 2; s.renderScale = 1.0f;
            s.motionBlur = true;
            break;
        default:
            s.shadowQuality = 3; s.aaMode = 2; s.viewDistance = 260; s.destructionQuality = 3;
            s.debrisDensity = 3; s.physicsQuality = 2.0f; s.particleQuality = 3; s.renderScale = 1.0f;
            s.motionBlur = true;
            break;
    }
}

// ------------------------------------------------------------------ main menu
void Game::drawMainMenu() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    drawMenuBackdrop();
    float left = 96;
    drawLogo(left + 210, h * 0.26f, std::max(0.75f, h / 900.0f));

    float bx = left, by = h * 0.46f, bw = 300, bh = 42, gap = 9;
    // vertical accent rule
    ui.addRect(Rect(bx - 22, by - 12, 2, bh * 5 + gap * 5 + 12), 0x40FFFFFFu);

    if (ui.buttonAccent(Rect(bx, by, bw, bh), "CAMPAIGN", 100)) {
        audio::play(SFX_UI_CLICK, vec3(0), 0.6f, 1.0f);
        setMode(MODE_CAMPAIGN);
    }
    by += bh + gap;
    if (ui.button(Rect(bx, by, bw, bh), "SANDBOX", 101)) {
        audio::play(SFX_UI_CLICK, vec3(0), 0.6f, 1.0f);
        sandbox.timeOfDay = defaultTimeOfDay(currentMap);
        sandbox.weather = -1;
        setMode(MODE_SANDBOX_SETUP);
    }
    by += bh + gap;
    if (ui.button(Rect(bx, by, bw, bh), "SETTINGS", 102)) { audio::play(SFX_UI_CLICK, vec3(0), 0.6f, 1.0f); setMode(MODE_SETTINGS); }
    by += bh + gap;
    if (ui.button(Rect(bx, by, bw, bh), "CONTROLS", 103)) { audio::play(SFX_UI_CLICK, vec3(0), 0.6f, 1.0f); setMode(MODE_CONTROLS); }
    by += bh + gap;
    if (ui.button(Rect(bx, by, bw, bh), "CREDITS", 104)) { audio::play(SFX_UI_CLICK, vec3(0), 0.6f, 1.0f); setMode(MODE_CREDITS); }
    by += bh + gap;
    if (ui.button(Rect(bx, by, bw, bh - 4), "EXIT", 105)) {
        audio::play(SFX_UI_BACK, vec3(0), 0.6f, 1.0f);
        platform::requestQuit();
    }

    // right-hand info panel: engine + map stats
    float px = w - 400, py = h * 0.30f;
    Rect panel(px, py, 360, 300);
    ui.panel(panel, 0xB00C0F13u, true);
    ui.addRect(Rect(panel.x, panel.y, panel.w, 3), ui::COL_ACCENT);
    ui.textSpaced(panel.x + 16, panel.y + 14, 11, ui::COL_ACCENT, "SYSTEM STATUS", 2.0f);
    char buf[768];
    snprintf(buf, sizeof(buf),
             "RENDERER    %s\nENGINE      VOXEL 1.0 / C++17\nMAP BUFFER  %.0f x %.0f x %.0f m\nVOXEL SIZE  %.2f m\nMAP         %s\nCAMPAIGN    %d / %d MISSIONS\nSANDBOX     UNLOCKED\n\nAll geometry is destructible voxels.\nChunked meshing, structural support\nanalysis, debris physics, fire.",
             renderer->name(), world.sx * VOX, world.sy * VOX, world.sz * VOX, VOX,
             getMap(currentMap).shortName, (int)g_missions.completedMissions.size(), g_missions.count());
    ui.text(panel.x + 16, panel.y + 38, 11, 0xC0E8E8E8u, buf);

    // bottom strip: modes and version
    float sy = h - 78;
    ui.addRect(Rect(0, sy, w, 1), 0x30FFFFFFu);
    ui.text(96, sy + 14, 11, 0x80FFFFFFu, "FRACTURE: TOTAL DESTRUCTION  -  A DEMOLITION SANDBOX");
    char sb[96];
    snprintf(sb, sizeof(sb), "%s  //  %d FPS", renderer->name(), stats.fps);
    ui.textRight(Rect(0, sy + 14, w - 96, 14), 11, 0x60FFFFFFu, sb);
}

// ------------------------------------------------------------------ campaign
void Game::drawCampaignMenu() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    drawMenuBackdrop();
    // header
    ui.textSpaced(72, 46, 30, 0xFFFFFFFFu, "CAMPAIGN", 2.5f);
    ui.hazardStripe(72, 82, w - 144, 4, ui::COL_ACCENT, 0x80141414u);
    char pb[64];
    snprintf(pb, sizeof(pb), "%d / %d COMPLETE", (int)g_missions.completedMissions.size(), g_missions.count());
    ui.textRight(Rect(0, 48, w - 72, 20), 12, ui::COL_ACCENT, pb);

    if (ui.button(Rect(w - 180, 92, 108, 32), "BACK", 900)) setMode(MODE_MAIN_MENU);

    // mission list: one column per chapter, laid out in a 3 x 2 grid
    float colW = (w - 144 - 2 * 24) / 3.0f;
    float rowH = 216;
    for (int c = 0; c < 5; c++) {
        int col = c % 3, row = c / 3;
        float x = 72 + col * (colW + 24);
        float y = 116 + row * rowH;
        ui.textSpaced(x, y, 11, ui::COL_ACCENT, g_missions.chapterName(c + 1), 1.3f);
        ui.addRect(Rect(x, y + 16, colW, 1), 0x30FFFFFFu);
        auto ids = g_missions.missionsForChapter(c + 1);
        float my = y + 24;
        for (int id : ids) {
            const Mission& m = g_missions.get(id);
            bool done = g_missions.isCompleted(id);
            bool unlocked = done || id == 0 || g_missions.isCompleted(id - 1);
            Rect r(x, my, colW, 40);
            bool hovered = ui.isHovered(r);
            ui.panel(r, hovered && unlocked ? 0xC0222832u : (done ? 0xB0202820u : (unlocked ? 0xB0141820u : 0x700C0E12u)), false);
            ui.addRect(Rect(r.x, r.y, done ? r.w : 3, done ? 2 : r.h),
                       done ? ui::COL_GOOD : (unlocked ? ui::COL_ACCENT : 0x40FFFFFFu));
            ui.text(r.x + 12, r.y + 9, 12.5f, unlocked ? 0xFFF0F0F0u : 0x60FFFFFFu, m.name);
            char sub[96];
            int secs = (int)m.parTime;
            snprintf(sub, sizeof(sub), "%s  //  PAR %d:%02d", getMap(m.mapId).shortName, secs / 60, secs % 60);
            ui.text(r.x + 12, r.y + 25, 9.5f, 0x80FFFFFFu, sub);
            if (done) ui.textRight(Rect(r.x, r.y + 9, r.w - 12, 12), 9.5f, ui::COL_GOOD, "COMPLETE");
            else if (!unlocked) ui.textRight(Rect(r.x, r.y + 9, r.w - 12, 12), 9.5f, 0x70FFFFFFu, "LOCKED");
            else if (m.timed) ui.textRight(Rect(r.x, r.y + 9, r.w - 12, 12), 9.5f, 0xFFCC8833u, "TIMED");
            else if (m.stealth) ui.textRight(Rect(r.x, r.y + 9, r.w - 12, 12), 9.5f, 0xFF6699CCu, "STEALTH");
            if (unlocked && hovered) {
                // hovered mission brief
                ui.panel(Rect(72, h - 168, w - 144, 112), 0xB00A0D11u, true);
                ui.addRect(Rect(72, h - 168, 3, 112), ui::COL_ACCENT);
                ui.text(88, h - 154, 13, 0xFFFFFFFFu, m.name);
                ui.wrapText(88, h - 132, 11, 0xB0E0E0E0u, m.brief, w - 200, 15);
                if (ui.click) {
                    audio::play(SFX_UI_CLICK, vec3(0), 0.6f, 1.0f);
                    startMission(id);
                    return;
                }
            }
            my += 44;
        }
    }
    // mission detail on hover is not needed; the brief appears on load
    if (g_missions.completedMissions.empty()) {
        ui.text(72, h - 44, 11, 0x80FFFFFFu,
                "Complete a mission to unlock the next. Missions are replayable. Hover a mission for its briefing.");
    }
}

// ------------------------------------------------------------------ sandbox setup
void Game::drawSandboxSetup() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    drawMenuBackdrop();
    ui.textSpaced(72, 46, 30, 0xFFFFFFFFu, "SANDBOX", 2.5f);
    ui.text(72, 84, 12, 0x90FFFFFFu, "Every map is fully destructible. Pick a site, then a time and a sky.");
    if (ui.button(Rect(w - 180, 92, 108, 32), "BACK", 900)) setMode(MODE_MAIN_MENU);

    // map tiles
    float tw = 196, th = 118, gap = 12;
    for (int i = 0; i < mapCount(); i++) {
        const GameMap& m = getMap(i);
        Rect r(72 + (i % 3) * (tw + gap), 118 + (i / 3) * (th + gap), tw, th);
        bool sel = pendingMap == i;
        ui.panel(r, sel ? 0xD0222832u : 0xB00E1116u, true);
        if (sel) ui.border(r, ui::COL_ACCENT, 2.0f);
        ui.text(r.x + 12, r.y + 12, 14, sel ? 0xFFFFFFFFu : 0xC8E0E0E0u, m.shortName);
        ui.text(r.x + 12, r.y + 34, 10, ui::COL_ACCENT, m.tagline);
        char dim[64];
        snprintf(dim, sizeof(dim), "%.0f x %.0f x %.0f m", m.info.sx * VOX, m.info.sy * VOX, m.info.sz * VOX);
        ui.text(r.x + 12, r.y + th - 20, 10, 0x70FFFFFFu, dim);
        if (ui.isHovered(r) && ui.click) {
            pendingMap = i;
            sandbox.timeOfDay = defaultTimeOfDay(i);
            audio::play(SFX_UI_CLICK, vec3(0), 0.5f, 1.0f);
        }
    }
    const GameMap& selMap = getMap(iclamp(pendingMap, 0, mapCount() - 1));
    float cx = 72;
    float cy = 118 + 2 * (th + gap) + 14;
    // description
    Rect desc(cx, cy, 3 * (tw + gap) - gap, 64);
    ui.panel(desc, 0x90101318u, false);
    ui.wrapText(cx + 14, cy + 12, 11, 0xA8E8E8E8u, selMap.info.description, desc.w - 28, 14);

    // options column on the right
    float ox = 72 + 3 * (tw + gap) + 34;
    float oy = 118;
    float ow = w - ox - 72;
    Rect op(ox, oy, ow, 392);
    ui.panel(op, 0xB00C0F13u, true);
    ui.addRect(Rect(op.x, op.y, op.w, 3), ui::COL_ACCENT);
    ui.textSpaced(op.x + 16, op.y + 12, 11, ui::COL_ACCENT, "SANDBOX OPTIONS", 1.8f);
    float y = op.y + 36, lh = 34;
    ui.toggle(Rect(op.x + 16, y, ow - 32, 26), "Unlimited ammunition", sandbox.unlimitedAmmo, 201); y += lh;
    ui.toggle(Rect(op.x + 16, y, ow - 32, 26), "Invulnerability", sandbox.invincible, 202); y += lh;
    ui.toggle(Rect(op.x + 16, y, ow - 32, 26), "Infinite torch fuel", sandbox.infiniteFuel, 203); y += lh;
    ui.toggle(Rect(op.x + 16, y, ow - 32, 26), "Vehicle spawning (V)", sandbox.vehicleSpawning, 204); y += lh;
    ui.sliderInt(Rect(op.x + 16, y, ow - 32, 26), "Explosion power", sandbox.debrisDensity, 0, 3, 205); y += lh;
    int hr = iclamp(sandbox.timeOfDay, 0, 23);
    if (ui.sliderInt(Rect(op.x + 16, y, ow - 32, 26), "Time of day (hour)", hr, 0, 23, 206)) {
        sandbox.timeOfDay = hr;
    } else sandbox.timeOfDay = hr;
    y += lh;
    int widx = sandbox.weather < 0 ? selMap.info.weather.kind : sandbox.weather;
    int nw = ui.dropdown(Rect(op.x + 16, y, ow - 32, 26), "Weather", kWeatherNames2, 5, iclamp(widx, 0, 4), 207);
    if (nw != iclamp(widx, 0, 4)) sandbox.weather = nw;
    y += lh;
    // launch
    if (ui.buttonAccent(Rect(op.x + 16, op.y + op.h - 54, ow - 32, 38), "LAUNCH SANDBOX", 210)) {
        audio::play(SFX_UI_CLICK, vec3(0), 0.7f, 1.0f);
        startSandbox(pendingMap);
    }
    // quick notes
    ui.text(72, h - 96, 11, 0x70FFFFFFu, "TAB opens the control sheet. F5 saves the destroyed geometry; F9 restores it.");
    ui.text(72, h - 76, 11, 0x70FFFFFFu, "Everything you see - walls, floors, roofs, machines, trees - is voxels you can break.");
}

// ------------------------------------------------------------------ settings
void Game::drawSettings() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    drawMenuBackdrop();
    ui.textSpaced(72, 40, 28, 0xFFFFFFFFu, "SETTINGS", 2.5f);
    ui.hazardStripe(72, 74, w - 144, 4, ui::COL_ACCENT, 0x80141414u);
    if (ui.button(Rect(w - 180, 88, 108, 32), "BACK", 900)) { applySettings(); setMode(MODE_MAIN_MENU); }

    float colW = (w - 200) / 2.0f - 20;
    float x0 = 72, y0 = 100;
    float lh = 30;
    // ---- left column: display
    Rect lp(x0, y0, colW, 420);
    ui.panel(lp, 0xB00C0F13u, true);
    ui.sectionHeader(lp.x + 14, lp.y + 12, lp.w - 28, "DISPLAY");
    float y = lp.y + 44;
    int resIdx = 1;
    for (int i = 0; i < 6; i++) if (settings.resolutionW == kResW2[i] && settings.resolutionH == kResH2[i]) resIdx = i;
    int nr = ui.dropdown(Rect(lp.x + 14, y, lp.w - 28, 24), "Resolution", kResNames2, 6, resIdx, 300);
    if (nr != resIdx) {
        settings.resolutionW = kResW2[nr];
        settings.resolutionH = kResH2[nr];
        if (mode == MODE_PLAYING) resize(settings.resolutionW, settings.resolutionH);
    }
    y += lh;
    int nm = ui.dropdown(Rect(lp.x + 14, y, lp.w - 28, 24), "Window mode", kWindowModes, 3, iclamp(settings.windowMode, 0, 2), 301);
    if (nm != settings.windowMode) { settings.windowMode = nm; platform::setFullscreen(nm, settings.resolutionW, settings.resolutionH); }
    y += lh;
    ui.toggle(Rect(lp.x + 14, y, lp.w - 28, 24), "V-Sync", settings.vsync, 302); y += lh;
    int naa = ui.dropdown(Rect(lp.x + 14, y, lp.w - 28, 24), "Anti-aliasing", kAAModes, 3, iclamp(settings.aaMode, 0, 2), 303);
    if (naa != settings.aaMode) { settings.aaMode = naa; applySettings(); }
    y += lh;
    ui.slider(Rect(lp.x + 14, y, lp.w - 28, 24), "Field of view", settings.fov, 60, 110, 1, 304, "%.0f deg"); y += lh;
    ui.sliderInt(Rect(lp.x + 14, y, lp.w - 28, 24), "View distance", settings.viewDistance, 60, 320, 305); y += lh;
    ui.slider(Rect(lp.x + 14, y, lp.w - 28, 24), "Render scale", settings.renderScale, 0.5f, 1.5f, 0.05f, 306, "%.2f"); y += lh;
    ui.slider(Rect(lp.x + 14, y, lp.w - 28, 24), "Exposure", settings.exposure, 0.6f, 1.8f, 0.05f, 307, "%.2f"); y += lh;
    ui.toggle(Rect(lp.x + 14, y, lp.w - 28, 24), "Motion blur", settings.motionBlur, 308); y += lh;
    ui.toggle(Rect(lp.x + 14, y, lp.w - 28, 24), "Show FPS", settings.showFps, 309); y += lh;

    // ---- right column: quality + destruction + audio + input
    Rect rp(x0 + colW + 40, y0, colW, 420);
    ui.panel(rp, 0xB00C0F13u, true);
    ui.sectionHeader(rp.x + 14, rp.y + 12, rp.w - 28, "QUALITY / DESTRUCTION");
    y = rp.y + 44;
    int np = ui.dropdown(Rect(rp.x + 14, y, rp.w - 28, 24), "Preset", kQual, 4, iclamp(settings.qualityPreset, 0, 3), 320);
    if (np != settings.qualityPreset) { setPreset(settings, np); applySettings(); }
    y += lh;
    ui.sliderInt(Rect(rp.x + 14, y, rp.w - 28, 24), "Shadow quality", settings.shadowQuality, 0, 3, 321); y += lh;
    ui.sliderInt(Rect(rp.x + 14, y, rp.w - 28, 24), "Destruction detail", settings.destructionQuality, 0, 3, 322); y += lh;
    ui.sliderInt(Rect(rp.x + 14, y, rp.w - 28, 24), "Debris density", settings.debrisDensity, 0, 3, 323); y += lh;
    ui.sliderInt(Rect(rp.x + 14, y, rp.w - 28, 24), "Particle quality", settings.particleQuality, 0, 3, 324); y += lh;
    ui.slider(Rect(rp.x + 14, y, rp.w - 28, 24), "Physics quality", settings.physicsQuality, 0.5f, 2.0f, 0.5f, 325, "%.1f"); y += lh;
    ui.sectionHeader(rp.x + 14, y + 4, rp.w - 28, "AUDIO / INPUT");
    y += 30;
    ui.slider(Rect(rp.x + 14, y, rp.w - 28, 24), "Master volume", settings.masterVolume, 0, 1, 0.05f, 330, "%.2f"); y += lh;
    ui.slider(Rect(rp.x + 14, y, rp.w - 28, 24), "SFX volume", settings.sfxVolume, 0, 1, 0.05f, 331, "%.2f"); y += lh;
    ui.slider(Rect(rp.x + 14, y, rp.w - 28, 24), "Music volume", settings.musicVolume, 0, 1, 0.05f, 332, "%.2f"); y += lh;
    ui.slider(Rect(rp.x + 14, y, rp.w - 28, 24), "Mouse sensitivity", settings.mouseSensitivity, 0.2f, 3.0f, 0.1f, 333, "%.1f"); y += lh;
    ui.toggle(Rect(rp.x + 14, y, rp.w - 28, 24), "Invert mouse Y", settings.invertY, 334); y += lh;
    ui.toggle(Rect(rp.x + 14, y, rp.w - 28, 24), "Reduce camera shake", settings.reduceShake, 335); y += lh;
    ui.toggle(Rect(rp.x + 14, y, rp.w - 28, 24), "Fullbright interiors", settings.fullbrightShadows, 336); y += lh;

    // bottom: apply / defaults
    if (ui.buttonAccent(Rect(x0, h - 76, 220, 40), "APPLY SETTINGS", 340)) {
        applySettings();
        toast("Settings applied", 1);
    }
    if (ui.button(Rect(x0 + 236, h - 76, 180, 40), "DEFAULT", 341)) {
        settings = Settings();
        applySettings();
    }
    if (ui.button(Rect(x0 + 432, h - 76, 180, 40), "CONTROLS", 342)) setMode(MODE_CONTROLS);
    char rb[128];
    snprintf(rb, sizeof(rb), "Renderer: %s", renderer->name());
    ui.textRight(Rect(0, h - 66, w - 72, 20), 12, 0x80FFFFFFu, rb);
    ui.text(x0, h - 108, 11, 0x70FFFFFFu,
            "Quality presets change real parameters: shadow resolution, mesh detail budget, debris count, particle count.");
}

// ------------------------------------------------------------------ controls
void Game::drawControls() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    drawMenuBackdrop();
    ui.textSpaced(72, 40, 28, 0xFFFFFFFFu, "CONTROLS", 2.5f);
    ui.hazardStripe(72, 74, w - 144, 4, ui::COL_ACCENT, 0x80141414u);
    if (ui.button(Rect(w - 180, 88, 108, 32), "BACK", 900)) setMode(MODE_MAIN_MENU);
    struct Row { const char* k; const char* d; };
    static const Row rows[] = {
        {"W A S D", "Move"},
        {"MOUSE", "Look / aim"},
        {"SHIFT", "Sprint"},
        {"CTRL", "Crouch"},
        {"SPACE", "Jump (hold at a ledge to mantle)"},
        {"LEFT CLICK", "Primary tool action"},
        {"RIGHT CLICK", "Secondary action (scope, place, brace)"},
        {"1 ... 9, 0, -", "Select tool from the quick bar"},
        {"MOUSE WHEEL", "Cycle tools"},
        {"R", "Reload / detonate all charges"},
        {"E", "Interact: pick up, carry, enter or exit vehicle"},
        {"Q", "Drop carried object"},
        {"F", "Flashlight"},
        {"V", "Spawn vehicle (sandbox)"},
        {"T", "Cycle time of day (sandbox)"},
        {"G", "Cycle weather (sandbox)"},
        {"N", "Reset the map (sandbox)"},
        {"B", "Spawn a prop at the crosshair (sandbox)"},
        {"F3", "Tactical minimap"},
        {"F5 / F9", "Quick save / quick load sandbox"},
        {"F1", "Debug overlay"},
        {"TAB", "Toggle in-game control sheet"},
        {"ESC", "Pause menu"},
    };
    float colW = 380;
    for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); i++) {
        int col = i / 12, row = i % 12;
        float x = 72 + col * (colW + 40), y = 108 + row * 30;
        ui.text(x, y, 12, ui::COL_ACCENT, rows[i].k);
        ui.text(x + 150, y, 12, 0xD0E0E0E0u, rows[i].d);
    }
    ui.text(72, h - 90, 11, 0x70FFFFFFu,
            "Every destruction effect is physics-driven: explosives remove voxels, unsupported structures collapse,");
    ui.text(72, h - 72, 11, 0x70FFFFFFu,
            "debris is simulated with a fixed budget and fire spreads across flammable materials.");
}

// ------------------------------------------------------------------ credits
void Game::drawCredits() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    drawMenuBackdrop();
    drawLogo(w * 0.5f, 90, 1.0f);
    const char* lines[] = {
        "FRACTURE: TOTAL DESTRUCTION",
        "An original voxel demolition sandbox.",
        "",
        "DESIGN, CODE, ENGINE, ASSETS",
        "Built as a single custom C++17 engine",
        "with a hand-written voxel mesher, rigid body physics,",
        "structural support solver, software and OpenGL renderers,",
        "procedural audio synthesis and procedural map generation.",
        "",
        "No third-party engine, no external art assets.",
        "All geometry is generated at runtime.",
        "",
        "THANK YOU FOR PLAYING",
    };
    float y = 260;
    for (auto& l : lines) {
        float tw = ui.measure(l, 13);
        ui.text(w * 0.5f - tw * 0.5f, y, 13, 0xB0E8E8E8u, l);
        y += 26;
    }
    if (ui.button(Rect(w * 0.5f - 90, h - 90, 180, 38), "BACK", 900)) setMode(MODE_MAIN_MENU);
}

// ------------------------------------------------------------------ pause
void Game::drawPauseMenu() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    ui.addQuad(0, 0, w, h, 0xA0080A0Du, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    float pw = 420, ph = 470;
    Rect p(w * 0.5f - pw * 0.5f, h * 0.5f - ph * 0.5f, pw, ph);
    ui.panel(p, 0xF00C0F13u, true);
    ui.addRect(Rect(p.x, p.y, p.w, 4), ui::COL_ACCENT);
    ui.textSpaced(p.x + 24, p.y + 18, 22, 0xFFFFFFFFu, "PAUSED", 2.0f);
    ui.text(p.x + 24, p.y + 50, 11, 0x90FFFFFFu,
            playMode == PLAY_CAMPAIGN ? g_missions.get(g_missions.state.missionId).name : getMap(currentMap).shortName);
    float y = p.y + 82, bw = p.w - 48, bh = 42, gap = 10;
    if (ui.buttonAccent(Rect(p.x + 24, y, bw, bh), "RESUME", 400)) { setMode(MODE_PLAYING); return; }
    y += bh + gap;
    if (ui.button(Rect(p.x + 24, y, bw, bh), "SETTINGS", 401)) { setMode(MODE_SETTINGS); return; }
    y += bh + gap;
    if (playMode == PLAY_SANDBOX) {
        if (ui.button(Rect(p.x + 24, y, bw, bh), "SAVE / LOAD", 402)) { setMode(MODE_SAVE_LOAD); return; }
        y += bh + gap;
    }
    if (ui.button(Rect(p.x + 24, y, bw, bh), "RESTART LEVEL", 403)) { restartLevel(); return; }
    y += bh + gap;
    if (ui.button(Rect(p.x + 24, y, bw, bh), "CONTROLS", 404)) { setMode(MODE_CONTROLS); return; }
    y += bh + gap;
    if (ui.buttonDanger(Rect(p.x + 24, y, bw, bh), "QUIT TO MENU", 405)) { returnToMenu(); return; }
    // top-right stats
    char st[256];
    snprintf(st, sizeof(st), "TIME  %d:%02d\nVOXELS REMOVED  %d\nEXPLOSIONS  %d\nFPS  %d",
             (int)playTime / 60, (int)playTime % 60, totalDestroyedVoxels, totalExplosions, stats.fps);
    ui.text(p.x + 24, p.y + ph - 84, 10, 0x70FFFFFFu, st);
}

// ------------------------------------------------------------------ mission result
void Game::drawMissionResult() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    bool win = mode == MODE_MISSION_COMPLETE;
    ui.addQuad(0, 0, w, h, win ? 0xB0060E0Au : 0xB00E0606u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    float pw = 720, ph = 430;
    Rect p(w * 0.5f - pw * 0.5f, h * 0.5f - ph * 0.5f, pw, ph);
    ui.panel(p, 0xF00A0D11u, true);
    ui.addRect(Rect(p.x, p.y, p.w, 5), win ? ui::COL_GOOD : ui::COL_DANGER);
    const Mission& m = g_missions.get(g_missions.state.missionId);
    ui.textSpaced(p.x + 28, p.y + 22, 26, win ? ui::COL_GOOD : ui::COL_DANGER,
                  win ? "MISSION COMPLETE" : "MISSION FAILED", 2.2f);
    ui.text(p.x + 28, p.y + 60, 14, 0xFFFFFFFFu, m.name);
    if (!win) {
        ui.text(p.x + 28, p.y + 84, 12, 0xFFFFAAAAu, g_missions.state.failReason.empty() ? "Objective not met." : g_missions.state.failReason);
    }
    ui.addRect(Rect(p.x + 28, p.y + 108, p.w - 56, 1), 0x30FFFFFFu);
    const std::string& body = win ? m.debrief : m.brief;
    ui.wrapText(p.x + 28, p.y + 124, 12, 0xC8E0E0E0u, body, p.w - 56, 18);
    // stats
    float sy = p.y + 240;
    char buf[256];
    snprintf(buf, sizeof(buf), "TIME              %d:%02d\nPAR TIME          %d:%02d\nVOXELS REMOVED    %d\nEXPLOSIONS        %d",
             (int)g_missions.state.time / 60, (int)g_missions.state.time % 60, (int)m.parTime / 60, (int)m.parTime % 60,
             totalDestroyedVoxels, totalExplosions);
    ui.text(p.x + 28, sy, 11, 0xA8FFFFFFu, buf);
    if (win) {
        float ratio = m.parTime / std::max(1.0f, g_missions.state.time);
        const char* rank = ratio > 1.6f ? "A" : (ratio > 1.15f ? "B" : (ratio > 0.85f ? "C" : "D"));
        ui.textSpaced(p.x + p.w - 150, sy - 16, 58, ui::COL_ACCENT, rank, 2.0f);
        ui.text(p.x + p.w - 150, sy + 52, 10, 0x80FFFFFFu, "RANK");
    }
    float y = p.y + ph - 70;
    if (win) {
        if (ui.buttonAccent(Rect(p.x + 28, y, 210, 42), "CONTINUE", 500)) {
            int next = g_missions.state.missionId + 1;
            if (next < g_missions.count()) startMission(next);
            else returnToMenu();
        }
    } else {
        if (ui.buttonAccent(Rect(p.x + 28, y, 210, 42), "RETRY", 501)) { restartLevel(); return; }
    }
    if (ui.button(Rect(p.x + 254, y, 190, 42), "REPLAY", 502)) { restartLevel(); return; }
    if (ui.button(Rect(p.x + 460, y, 200, 42), "MAIN MENU", 503)) { returnToMenu(); return; }
}

// ------------------------------------------------------------------ loading
void Game::drawLoading() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    ui.addQuad(0, 0, w, h, 0xFF0A0C10u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    ui.scanlines(Rect(0, 0, w, h), 0x08FFFFFFu, 3);
    drawLogo(w * 0.5f, h * 0.30f, 0.9f);
    const GameMap& m = getMap(iclamp(loading.mapId, 0, mapCount() - 1));
    float bw = 560, bx = w * 0.5f - bw * 0.5f, by = h * 0.60f;
    Rect bar(bx, by, bw, 12);
    ui.addRect(bar, 0x40202830u);
    ui.progress(bar, clampf(loadingProgress, 0, 1), ui::COL_ACCENT);
    char buf[192];
    int pct = (int)(loadingProgress * 100);
    const char* stage = loading.stage == 1 ? "MESHING CHUNKS" : (loading.stage == 2 ? "SPAWNING OBJECTS" : "READY");
    snprintf(buf, sizeof(buf), "%s  //  %s  //  %d%%", m.shortName, stage, pct);
    ui.text(bx, by - 22, 12, 0xA8FFFFFFu, buf);
    snprintf(buf, sizeof(buf), "%d / %d chunks", (int)loading.chunkIndex, (int)loading.chunks.size());
    ui.textRight(Rect(bx, by - 22, bw, 14), 11, 0x70FFFFFFu, buf);
    if (loading.campaign && loading.missionId >= 0) {
        const Mission& ms = g_missions.get(loading.missionId);
        ui.textSpaced(bx, by + 40, 12, ui::COL_ACCENT, ms.name, 1.6f);
        ui.wrapText(bx, by + 62, 11, 0x90FFFFFFu, ms.brief, bw, 16);
    } else {
        ui.textSpaced(bx, by + 40, 12, ui::COL_ACCENT, m.shortName, 1.6f);
        ui.wrapText(bx, by + 62, 11, 0x90FFFFFFu, m.info.description, bw, 16);
    }
    if (loading.stage == 0) {
        // safety: never leave the loading screen stuck
        loadingProgress = 1.0f;
    }
}

// ------------------------------------------------------------------ save / load
void Game::drawSaveLoad() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    drawMenuBackdrop();
    ui.textSpaced(72, 46, 28, 0xFFFFFFFFu, "SAVE / LOAD", 2.5f);
    ui.text(72, 84, 12, 0x90FFFFFFu, "Sandbox saves store the destroyed world geometry, the player state and the physics props.");
    if (ui.button(Rect(w - 180, 92, 108, 32), "BACK", 900)) setMode(MODE_PAUSE);
    const char* slots[3] = {"quicksave", "slot1", "slot2"};
    const char* names[3] = {"QUICKSAVE", "SLOT 1", "SLOT 2"};
    for (int i = 0; i < 3; i++) {
        Rect r(72, 130 + i * 92, w - 144, 78);
        ui.panel(r, 0xB00C0F13u, true);
        ui.addRect(Rect(r.x, r.y, 3, r.h), ui::COL_ACCENT);
        ui.text(r.x + 18, r.y + 14, 15, 0xFFF0F0F0u, names[i]);
        std::string path = savePath(slots[i]);
        FILE* f = fopen(path.c_str(), "rb");
        char meta[192];
        if (f) {
            uint32_t magic = 0, version = 0;
            int mapId = 0, playMode = 0, missionId = -1;
            fread(&magic, 4, 1, f);
            fread(&version, 4, 1, f);
            fread(&mapId, 4, 1, f);
            fread(&playMode, 4, 1, f);
            fread(&missionId, 4, 1, f);
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fclose(f);
            snprintf(meta, sizeof(meta), "%s   //   %.1f KB   //   %s", getMap(iclamp(mapId, 0, mapCount() - 1)).shortName,
                     sz / 1024.0, magic == 0x46524354 ? "VALID" : "UNKNOWN");
        } else {
            snprintf(meta, sizeof(meta), "EMPTY SLOT");
        }
        ui.text(r.x + 18, r.y + 40, 11, 0x80FFFFFFu, meta);
        if (ui.button(Rect(r.x + r.w - 340, r.y + 20, 100, 36), "SAVE", 600 + i)) {
            if (saveGame(slots[i])) toast("Saved", 1);
            else toast("Save failed", 2);
        }
        if (ui.button(Rect(r.x + r.w - 230, r.y + 20, 100, 36), "LOAD", 610 + i)) {
            if (loadGame(slots[i])) return;
            else toast("No save in this slot", 2);
        }
        if (ui.buttonDanger(Rect(r.x + r.w - 120, r.y + 20, 100, 36), "DEL", 620 + i)) {
            remove(path.c_str());
            toast("Slot cleared", 0);
        }
    }
}
