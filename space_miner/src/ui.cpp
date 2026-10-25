// ============================================================
// SPACE MINER  -  ui.cpp
// Custom immediate-mode UI: panels, buttons, menus, station,
// sector chart, settings, achievements, loading, death, victory.
// ============================================================
#include "game.h"
#include "content.h"
#include "art.h"
#include "platform.h"

namespace sm {

// ---------------------------------------------------------------- primitives
void Game::panel(Canvas& fb, int x, int y, int w, int h, Color accent, float alpha) {
    if (w <= 0 || h <= 0) return;
    fb.fillRect(x, y, w, h, withAlpha(rgba(9, 14, 22), (int)(205 * alpha)));
    // inner subtle gradient
    fb.vGradient(x + 1, y + 1, w - 2, std::min(h - 2, 40), withAlpha(rgba(40, 60, 78), (int)(90 * alpha)),
                 withAlpha(rgba(9, 14, 22), 0));
    // border
    fb.rectOutline((float)x, (float)y, (float)w, (float)h, 1.f, withAlpha(accent, (int)(120 * alpha)));
    fb.rectOutline((float)x + 1, (float)y + 1, (float)w - 2, (float)h - 2, 1.f, withAlpha(rgba(255, 255, 255), (int)(14 * alpha)));
    // corner ticks
    int cl = 14;
    int a = (int)(220 * alpha);
    Color c = withAlpha(accent, a);
    fb.fillRect(x - 1, y - 1, cl, 2, c);
    fb.fillRect(x - 1, y - 1, 2, cl, c);
    fb.fillRect(x + w - cl + 1, y - 1, cl, 2, c);
    fb.fillRect(x + w - 1, y - 1, 2, cl, c);
    fb.fillRect(x - 1, y + h - 1, cl, 2, c);
    fb.fillRect(x - 1, y + h - cl + 1, 2, cl, c);
    fb.fillRect(x + w - cl + 1, y + h - 1, cl, 2, c);
    fb.fillRect(x + w - 1, y + h - cl + 1, 2, cl, c);
}

bool Game::button(Canvas& fb, int id, int x, int y, int w, int h, const std::string& label,
                  Color accent, bool enabled, bool toggled) {
    bool hover = false;
    if (curInput) hover = mouseIn(*curInput, x, y, w, h);
    if (hover && hoverId != id) {
        hoverId = id;
        audio::playUiHover();
    }
    bool clicked = false;
    if (hover && curInput && curInput->mouseReleased[0] && activeId == id) {
        clicked = true;
    }
    if (hover && curInput && curInput->mousePressed[0] && enabled) activeId = id;
    if (!hover && curInput && curInput->mouseReleased[0] && activeId == id) activeId = -1;

    float pulse = hover ? 1.f : 0.f;
    Color bg = toggled ? withAlpha(accent, 70) : withAlpha(rgba(16, 24, 34), 230);
    if (hover && enabled) bg = withAlpha(accent, toggled ? 110 : 52);
    if (!enabled) bg = withAlpha(rgba(14, 18, 24), 200);
    fb.fillRect(x, y, w, h, bg);
    if (pulse > 0 && enabled) {
        fb.fillRect(x, y, 3, h, withAlpha(accent, 240));
        fb.fillRect(x, y + h - 1, w, 1, withAlpha(accent, 140));
    }
    fb.rectOutline((float)x, (float)y, (float)w, (float)h, 1.f,
                   withAlpha(enabled ? accent : rgba(90, 100, 110), (int)(hover ? 220 : 120)));
    Color tc = enabled ? (hover ? rgba(255, 255, 255) : rgba(214, 232, 244)) : rgba(110, 122, 134);
    int tw = textWidth(label, 2, 1);
    drawText(fb, x + (w - tw) / 2, y + (h - 10) / 2, label, tc, 2, 1);
    if (activeId == id && hover && curInput && curInput->mouseDown[0] && enabled) {
        fb.fillRect(x, y, w, h, withAlpha(rgba(255, 255, 255), 16));
    }
    return clicked && enabled;
}

void Game::screenHeader(Canvas& fb, const std::string& title, const std::string& sub, Color accent) {
    fb.fillRect(0, 0, VW, 66, withAlpha(rgba(6, 10, 16), 220));
    fb.fillRect(0, 66, VW, 1, withAlpha(accent, 160));
    drawHeading(fb, 40 + textWidth(title, 3, 4) / 2, 20, title, rgba(240, 248, 255), accent, 3, 4);
    drawText(fb, 40, 46, sub, withAlpha(rgba(170, 196, 214), 210), 1, 1);
    // station emblem
    fb.circleFill(VW - 46, 33, 17.f, withAlpha(accent, 40));
    fb.ring(VW - 46, 33, 17.f, 1.4f, withAlpha(accent, 200));
    fb.ring(VW - 46, 33, 9.f, 1.2f, withAlpha(accent, 140));
    fb.line({VW - 46 - 17, 33}, {VW - 46 + 17, 33}, withAlpha(accent, 90), 1.f);
}

static void statBar(Canvas& fb, int x, int y, int w, float frac, Color c, const std::string& label) {
    fb.fillRect(x, y, w, 5, withAlpha(rgba(12, 18, 26), 220));
    fb.fillRect(x, y, (int)(w * clampf(frac, 0.f, 1.f)), 5, c);
    if (!label.empty()) drawTextRight(fb, x - 3, y, label, withAlpha(rgba(160, 186, 204), 190), 1, 1);
}

// ---------------------------------------------------------------- menu backdrop
static void menuBackdrop(Game& g, Canvas& fb) {
    // slow drifting nebula + stars from sector 0/2
    int sec = 1;
    const Canvas& nb = g.art().nebula[sec][0];
    fb.vGradient(0, 0, VW, VH, rgba(4, 7, 14), rgba(10, 6, 20));
    float t = g.uiTime * .012f;
    fb.blitScaled(nb, (int)(-260 + sinf(t) * 90.f), (int)(-120 + cosf(t * .7f) * 60.f), 1800, 1000, .95f, true);
    const Canvas& nb2 = g.art().nebula[sec][1];
    fb.blitScaled(nb2, (int)(-160 + cosf(t * .5f) * 120.f), (int)(-60 + sinf(t * .4f) * 80.f), 1700, 960, .5f, true);
    const Canvas& stars = g.art().stars[sec];
    int sx = (int)(g.uiTime * 4.f) % stars.w;
    fb.blitScaled(stars, -sx, 0, stars.w, stars.h, .9f, true);
    fb.blitScaled(stars, stars.w - sx, 0, stars.w, stars.h, .9f, true);
    // planet
    int pw = 760, ph = 760;
    int px = VW - 250, py = VH - 300;
    fb.blitScaled(g.art().planetsGlow[1], px - pw / 2, py - ph / 2, pw, ph, .9f, true);
    fb.blitScaled(g.art().planets[1], px - pw / 2, py - ph / 2, pw, ph, 1.f, false);
    // hero ship drifting
    Vec2 hip(VW * .70f + sinf(g.uiTime * .5f) * 20.f, VH * .52f + cosf(g.uiTime * .7f) * 14.f);
    const ShipArt& art = g.art().ships[SHIP_LANCER];
    float ang = -.12f + sinf(g.uiTime * .35f) * .06f;
    for (auto& ep : art.engines) {
        Vec2 wp = hip + rot(ep.pos, ang) * .78f;
        Vec2 back = wp - fromAngle(ang) * 40.f;
        fb.glowLine(wp, back, withAlpha(ep.color, 150), 3.f, 10.f, 1.f);
    }
    fb.blitRotated(art.body, hip, ang, .78f, 1.f);
    fb.blitScaled(g.art().stationGlow, -180, VH - 520, 700, 700, .35f, true);
    fb.radialGlow(VW * .18f, VH * .82f, 420.f, withAlpha(rgba(120, 60, 180), 55), 1.f);
}

// ---------------------------------------------------------------- loading
void Game::renderLoading(Canvas& fb) {
    fb.clear(rgba(3, 5, 10));
    fb.vGradient(0, 0, VW, VH, rgba(6, 10, 20), rgba(2, 3, 6));
    float t = uiTime;
    for (int i = 0; i < 3; i++) {
        float a = (uiTime * .4f + i * 2.1f);
        fb.radialGlow(VW * .5f + cosf(a) * 300, VH * .5f + sinf(a * .8f) * 150, 320.f,
                      withAlpha(i == 0 ? rgba(60, 140, 220) : i == 1 ? rgba(140, 60, 200) : rgba(40, 200, 190), 40), 1.f);
    }
    drawHeading(fb, VW / 2, VH / 2 - 70, "SPACE MINER", rgba(232, 248, 255), rgba(90, 210, 255), 5, 8);
    drawTextCenter(fb, VW / 2, VH / 2 - 30, "DEEP BELT OPERATIONS", withAlpha(rgba(150, 200, 225), 200), 1, 2);
    int bw = 520, bx = VW / 2 - bw / 2, by = VH / 2 + 24;
    fb.fillRect(bx - 2, by - 2, bw + 4, 18, withAlpha(rgba(20, 30, 42), 220));
    fb.fillRect(bx, by, (int)(bw * loadProgress), 14, rgba(90, 210, 255));
    fb.fillRect(bx, by, (int)(bw * loadProgress), 6, rgba(190, 245, 255));
    for (int i = 0; i < 26; i++)
        fb.fillRect(bx + i * (bw / 26), by - 2, 1, 18, withAlpha(rgba(0, 0, 0), 60));
    drawTextCenter(fb, VW / 2, by + 26, loadStage + "...", withAlpha(rgba(160, 200, 220), 210), 1, 1);
    drawTextCenter(fb, VW / 2, VH - 40, "ORIGINAL ENGINE  -  PROCEDURAL ASSETS  -  NO EXTERNAL DEPENDENCIES",
                   withAlpha(rgba(110, 150, 175), 150), 1, 1);
}

// ---------------------------------------------------------------- main menu
void Game::renderMenu(Canvas& fb) {
    menuBackdrop(*this, fb);
    const SectorDef& sd = SECTORS[0];
    // left column
    fb.fillRect(0, 0, 700, VH, withAlpha(rgba(4, 7, 12), 120));
    fb.vGradient(0, 0, 660, VH, withAlpha(rgba(6, 10, 18), 205), withAlpha(rgba(6, 10, 18), 20));

    // title
    int tx = 78, ty = 96;
    for (int k = 0; k < 8; k++) {
        float a = k * TAU / 8;
        drawText(fb, tx + (int)(cosf(a) * 3), ty + (int)(sinf(a) * 3), "SPACE", withAlpha(rgba(70, 190, 255), 60), 8, 2);
    }
    drawText(fb, tx, ty, "SPACE", rgba(236, 250, 255), 8, 2);
    drawText(fb, tx, ty + 52, "MINER", rgba(120, 224, 255), 8, 2);
    fb.fillRect(tx, ty + 108, 420, 2, withAlpha(rgba(120, 224, 255), 200));
    fb.fillRect(tx, ty + 114, 240, 1, withAlpha(rgba(120, 224, 255), 90));
    drawText(fb, tx + 2, ty + 124, "DEEP BELT OPERATIONS  //  BUILD 3.0", withAlpha(rgba(150, 196, 220), 210), 1, 2);

    // menu items
    int my = 372;
    bool hasSave = false;
    if (curInput) hasSave = true;
    struct Item { const char* label; int id; };
    Item items[6] = {{"CONTINUE", 100}, {"NEW GAME", 101}, {"SECTOR CHART", 102},
                     {"ACHIEVEMENTS", 103}, {"SETTINGS", 104}, {"QUIT", 105}};
    int startIdx = hasSave ? 0 : 1;
    for (int i = startIdx; i < 6; i++) {
        int y = my + (i - startIdx) * 44;
        bool disabled = (i == 0) && !hasSave;
        if (button(fb, items[i].id, tx, y, 300, 36, items[i].label, sd.accent, !disabled)) {
            switch (i) {
                case 0: if (loadGame()) { audio::playUiClick(); } else { toast("NO SAVE DATA", rgba(255,150,90), 2.f); } break;
                case 1: {
                    newGame();
                    saveGame(true);
                    break;
                }
                case 2: screen = 4; mapSel = sector; break;
                case 3: screen = 6; break;
                case 4: pendingScreen = 5; settingsFrom = 1; break;
                default: requestQuit(); break;
            }
        }
        // little marker
        if (curInput && curInput->mouseIn(tx, y, 300, 36)) {
            int mx = tx - 18;
            Vec2 tri[3] = {{(float)mx, (float)y + 13}, {(float)mx, (float)y + 23}, {(float)mx + 9, (float)y + 18}};
            fb.polyFill(tri, 3, sd.accent);
        }
    }
    // footer
    drawText(fb, tx, VH - 64, "WASD FLY   LMB MINE   RMB FIRE   E DOCK   M CHART", withAlpha(rgba(140, 175, 195), 160), 1, 1);
    drawText(fb, tx, VH - 50, "ORIGINAL ART  -  PROCEDURAL GENERATION  -  64-BIT WINDOWS BUILD",
             withAlpha(rgba(100, 130, 150), 140), 1, 1);
    drawTextRight(fb, VW - 24, VH - 24, "V3.0  /  ARENA BUILD", withAlpha(rgba(120, 150, 170), 130), 1, 1);

    // save summary card when a save exists
    if (hasSave) {
        int cx = 78, cy = 252, cw = 470, ch = 88;
        panel(fb, cx, cy, cw, ch, sd.accent, .7f);
        drawText(fb, cx + 12, cy + 10, "PILOT RECORD", withAlpha(rgba(160, 200, 220), 210), 1, 2);
        drawText(fb, cx + 12, cy + 26, fmt("SECTOR %s   LV %d", SECTORS[save.currentSector].subtitle, save.level),
                 rgba(220, 240, 250), 1, 1);
        drawText(fb, cx + 12, cy + 40, fmt("CREDITS %s", money(save.credits).c_str()), rgba(255, 216, 120), 1, 1);
        drawText(fb, cx + 12, cy + 56, fmt("MINED %lld   KILLS %lld", save.asteroidsMined, save.enemiesKilled),
                 withAlpha(rgba(170, 200, 215), 200), 1, 1);
        drawText(fb, cx + 12, cy + 72, fmt("FLIGHT TIME %d:%02d", (int)(playTime / 60), (int)playTime % 60),
                 withAlpha(rgba(170, 200, 215), 170), 1, 1);
        // ship portrait
        fb.blitScaled(art().ships[save.ship].body, cx + cw - 130, cy + 12, 120, 120, .95f, false);
    }
    if (saveFlashT > 0) {
        drawTextRight(fb, VW - 24, 24, "PROGRESS SAVED", withAlpha(rgba(140, 255, 190), (int)(255 * clampf(saveFlashT, 0, 1))), 1, 1);
    }
    if (statusT > 0) {
        drawTextCenter(fb, VW / 2, VH - 100, statusLine, withAlpha(rgba(255, 200, 120), (int)(255 * clampf(statusT, 0, 1))), 1, 1);
    }
    if (hud.toastT > 0) {
        int w = textWidth(hud.toast, 2, 1) + 40;
        panel(fb, VW / 2 - w / 2, 84, w, 30, hud.toastColor, .7f);
        drawTextCenter(fb, VW / 2, 94, hud.toast, hud.toastColor, 2, 1);
    }
}

// ---------------------------------------------------------------- station
void Game::renderStation(Canvas& fb) {
    const SectorDef& sd = SECTORS[sector];
    fb.fillRect(0, 0, VW, VH, withAlpha(rgba(3, 6, 10), 210));
    // backdrop: station render, rotating slowly, behind the UI
    Vec2 st(VW * .5f, VH * .52f);
    fb.blitRotatedAdd(art().stationGlow, st, stationAngle * .5f, 1.9f, .5f);
    fb.blitRotated(art().station, st, stationAngle * .35f, 1.9f, .25f);

    screenHeader(fb, docked ? "HELIOS STATION" : "REMOTE LINK",
                 docked ? fmt("DOCKED  //  %s  //  LV %d PILOT", SECTORS[sector].name, save.level)
                        : fmt("CARGO MANIFEST  //  %s  //  DOCK TO TRADE", SECTORS[sector].name),
                 sd.accent);
    if (!docked) {
        float pulse = .6f + .4f * sinf(uiTime * 4.f);
        fb.fillRect(0, 68, VW, 18, withAlpha(rgba(60, 40, 12), 200));
        drawTextCenter(fb, VW / 2, 72, "REMOTE LINK - FLY TO THE STATION AND PRESS E TO TRADE",
                       withAlpha(rgba(255, 200, 120), (int)(200 + 55 * pulse)), 1, 1);
    }
    drawTextRight(fb, VW - 90, 20, money(save.credits), rgba(255, 216, 120), 2, 1);
    drawTextRight(fb, VW - 90, 40, "CREDITS", withAlpha(rgba(160, 190, 210), 180), 1, 1);

    // tabs
    const char* tabs[5] = {"CARGO", "UPGRADES", "SHIPYARD", "DRONES", "ACHIEVEMENTS"};
    for (int i = 0; i < 5; i++) {
        int x = 40 + i * 176;
        if (button(fb, 200 + i, x, 78, 166, 30, tabs[i], sd.accent, true, uiTab == i)) {
            uiTab = i;
            audio::playUiClick();
        }
    }
    int contentY = 124;
    int contentH = VH - contentY - 74;

    // ---------------------------------------------------------- CARGO
    if (uiTab == 0) {
        int lw = 660;
        panel(fb, 40, contentY, lw, contentH, sd.accent, .88f);
        drawText(fb, 56, contentY + 12, "HOLD CONTENTS", rgba(220, 240, 250), 2, 1);
        drawTextRight(fb, 40 + lw - 16, contentY + 14, fmt("%d / %d UNITS", (int)cargoUsed, (int)statCargo()),
                      withAlpha(sd.accent, 220), 1, 1);
        // hold fill bar
        fb.fillRect(56, contentY + 32, lw - 32, 8, withAlpha(rgba(12, 18, 26), 230));
        fb.fillRect(56, contentY + 32, (int)((lw - 32) * cargoFraction()), 8, rgba(110, 220, 170));
        int rowY = contentY + 52;
        int shown = 0;
        for (int i = 0; i < RES_COUNT; i++) {
            if (save.cargo[i] <= .005f) continue;
            int y = rowY + shown * 34;
            if (y > contentY + contentH - 44) break;
            bool hov = curInput && mouseIn(*curInput, 52, y, lw - 24, 30);
            fb.fillRect(52, y, lw - 24, 30, withAlpha(hov ? rgba(32, 50, 62) : rgba(16, 24, 34), 210));
            fb.fillRect(52, y, 3, 30, withAlpha(RESOURCES[i].color, 230));
            fb.blitScaled(art().resIcon[i], 60, y + 1, 28, 28, 1.f, false);
            drawText(fb, 96, y + 5, RESOURCES[i].name, RESOURCES[i].color, 2, 1);
            drawText(fb, 96, y + 19, fmt("%s   RARITY %d   MINING RESISTANCE %.2f", RESOURCES[i].code,
                                          RESOURCES[i].rarity + 1, RESOURCES[i].hardness),
                     withAlpha(rgba(150, 180, 200), 170), 1, 1);
            drawTextRight(fb, 470, y + 8, fmt("%.0f u", save.cargo[i]), rgba(230, 242, 250), 2, 1);
            long long val = (long long)(save.cargo[i] * RESOURCES[i].value * statRefine());
            drawTextRight(fb, 640, y + 11, fmt("%s CR", money(val).c_str()), rgba(255, 216, 120), 1, 1);
            if (hov) drawTextRight(fb, 640, y + 21, "CLICK TO SELL", withAlpha(sd.accent, 200), 1, 1);
            if (hov && curInput && curInput->mouseReleased[0]) {
                sellResource(i);
                if (!headless) saveGame();
            }
            shown++;
        }
        if (shown == 0) {
            // idle state art
            fb.radialGlow(40 + lw * .5f, contentY + contentH * .45f, 150.f, withAlpha(sd.accent, 26), 1.f);
            fb.ring(40 + lw * .5f, contentY + contentH * .45f, 60.f, 1.4f, withAlpha(sd.accent, 90));
            fb.ring(40 + lw * .5f, contentY + contentH * .45f, 40.f, 1.f, withAlpha(sd.accent, 50));
            drawTextCenter(fb, 40 + lw / 2, contentY + contentH * .45f - 6, "HOLD EMPTY", rgba(190, 215, 232), 2, 2);
            drawTextCenter(fb, 40 + lw / 2, contentY + contentH * .45f + 18, "FLY OUT AND CUT SOME ROCK",
                           withAlpha(rgba(140, 170, 190), 200), 1, 1);
            drawTextCenter(fb, 40 + lw / 2, contentY + contentH * .45f + 34,
                           "HOLD LEFT MOUSE NEAR AN ASTEROID TO ENGAGE THE MINING BEAM",
                           withAlpha(rgba(120, 150, 170), 170), 1, 1);
        }
        // ---- market panel
        int mx = 676, mw = VW - 716;
        panel(fb, mx, contentY, mw, contentH, sd.accent, .88f);
        drawText(fb, mx + 16, contentY + 12, "ORE MARKET", rgba(220, 240, 250), 2, 1);
        drawTextRight(fb, mx + mw - 16, contentY + 14, fmt("REFINERY +%d%%",
                      (int)((statRefine() - 1.f) * 100.f)), withAlpha(rgba(160, 230, 190), 220), 1, 1);
        // two columns of every resource with price + held qty
        for (int i = 0; i < RES_COUNT; i++) {
            int col = i / 8, row = i % 8;
            int cx2 = mx + 16 + col * ((mw - 32) / 2 + 4);
            int ry = contentY + 40 + row * 24;
            int cw2 = (mw - 40) / 2;
            bool have = save.cargo[i] > .005f;
            fb.fillRect(cx2, ry, cw2, 21, withAlpha(have ? rgba(28, 42, 52) : rgba(14, 20, 28), 190));
            fb.blitScaled(art().resIcon[i], cx2 + 2, ry + 1, 19, 19, have ? 1.f : .55f, false);
            drawText(fb, cx2 + 26, ry + 4, RESOURCES[i].name, withAlpha(RESOURCES[i].color, have ? 255 : 150), 1, 1);
            drawText(fb, cx2 + 26, ry + 12, fmt("%s  R%d", RESOURCES[i].code, RESOURCES[i].rarity + 1),
                     withAlpha(rgba(130, 160, 180), 170), 1, 0);
            drawTextRight(fb, cx2 + cw2 - 6, ry + 4, have ? fmt("%.0fu", save.cargo[i]) : "-",
                          have ? rgba(230, 242, 250) : withAlpha(rgba(120, 140, 155), 160), 1, 1);
            drawTextRight(fb, cx2 + cw2 - 6, ry + 13, fmt("%d", RESOURCES[i].value),
                          withAlpha(rgba(255, 216, 120), have ? 230 : 120), 1, 1);
        }
        long long total = 0;
        for (int i = 0; i < RES_COUNT; i++)
            if (save.cargo[i] > 0) total += (long long)(save.cargo[i] * RESOURCES[i].value * statRefine());
        int by = contentY + contentH - 88;
        int bw = mw - 48, bx = mx + 24;
        Color sellCol = total > 0 ? rgba(120, 240, 170) : rgba(80, 96, 108);
        fb.fillRect(bx, by, bw, 52, withAlpha(total > 0 ? rgba(22, 58, 42) : rgba(14, 20, 26), 235));
        fb.rectOutline((float)bx, (float)by, (float)bw, (float)52, 1.4f, sellCol);
        drawTextCenter(fb, bx + bw / 2, by + 10, "SELL ALL CARGO", total > 0 ? rgba(210, 255, 230) : rgba(120, 140, 150), 2, 1);
        drawTextCenter(fb, bx + bw / 2, by + 30, fmt("+%s CR", money(total).c_str()),
                       total > 0 ? rgba(255, 216, 120) : rgba(100, 120, 130), 2, 1);
        if (curInput && mouseIn(*curInput, bx, by, bw, 52) && curInput->mouseReleased[0]) {
            sellAll();
            if (!headless) saveGame();
        }
        drawText(fb, bx, by - 18, "CLICK A ROW TO SELL THAT ORE  /  [S] SELL EVERYTHING",
                 withAlpha(rgba(140, 180, 200), 170), 1, 1);
    }
    // ---------------------------------------------------------- UPGRADES
    else if (uiTab == 1) {
        int cols = 3, cardW = 386, cardH = 84;
        int ox = 40, oy = contentY;
        for (int i = 0; i < UPG_COUNT; i++) {
            int cx = ox + (i % cols) * (cardW + 14);
            int cy = oy + (i / cols) * (cardH + 9);
            const UpgradeDef& u = UPGRADES[i];
            int lvl = save.upgrades[i];
            bool maxed = lvl >= u.maxLevel;
            long long cost = (long long)(u.baseCost * powf(u.costScale, (float)lvl));
            bool afford = save.credits >= cost && !maxed;
            bool hov = curInput && mouseIn(*curInput, cx, cy, cardW, cardH);
            panel(fb, cx, cy, cardW, cardH, maxed ? rgba(120, 255, 200) : sd.accent, hov ? 1.f : .78f);
            fb.blitScaled(art().upgIcon[i], cx + 10, cy + 18, 44, 44, 1.f, false);
            drawText(fb, cx + 66, cy + 8, u.name, rgba(232, 244, 252), 2, 1);
            drawText(fb, cx + 66, cy + 24, u.desc, withAlpha(rgba(160, 190, 210), 190), 1, 1);
            // level pips
            for (int k = 0; k < u.maxLevel && k < 12; k++) {
                Color pc = k < lvl ? rgba(130, 240, 255) : rgba(40, 52, 64);
                fb.fillRect(cx + 66 + k * 9, cy + 38, 7, 5, withAlpha(pc, 230));
            }
            drawText(fb, cx + 66, cy + 48, fmt("LV %d/%d", lvl, u.maxLevel),
                     withAlpha(rgba(150, 190, 210), 200), 1, 1);
            // effect value
            float eff = 0;
            switch (i) {
                case UPG_LASER: eff = 18.f * lvl; break;
                case UPG_SPEED: eff = 12.f * lvl; break;
                case UPG_CARGO: eff = 40.f * lvl; break;
                case UPG_ENGINE: eff = 8.f * lvl; break;
                case UPG_HULL: eff = 18.f * lvl; break;
                case UPG_SHIELD: eff = 20.f * lvl; break;
                case UPG_ENERGY: eff = 15.f * lvl; break;
                case UPG_SCANNER: eff = 90.f * lvl; break;
                case UPG_DRONES: eff = (float)lvl; break;
                case UPG_WEAPON: eff = 20.f * lvl; break;
                case UPG_MAGNET: eff = 35.f * lvl; break;
                default: eff = 8.f * lvl; break;
            }
            drawTextRight(fb, cx + cardW - 14, cy + 48, fmt("+%.0f%s", eff, u.unit),
                          withAlpha(rgba(160, 240, 210), 220), 1, 1);
            // buy button row
            if (maxed) {
                drawText(fb, cx + 66, cy + 66, "FULLY UPGRADED", rgba(120, 255, 200), 1, 1);
            } else {
                Color cc = afford ? rgba(255, 216, 120) : rgba(190, 110, 90);
                drawText(fb, cx + 66, cy + 64, fmt("%s CR", money(cost).c_str()), cc, 2, 1);
                int bw = 108;
                int bx = cx + cardW - bw - 10, by = cy + cardH - 26;
                if (button(fb, 400 + i, bx, by, bw, 24, "PURCHASE", sd.accent, afford)) {
                    if (buyUpgrade(i)) {
                        if (!headless) saveGame();
                    }
                }
            }
            if (hov) {
                fb.rectOutline((float)cx, (float)cy, (float)cardW, (float)cardH, 1.4f, withAlpha(rgba(255, 255, 255), 60));
            }
        }
    }
    // ---------------------------------------------------------- SHIPYARD
    else if (uiTab == 2) {
        int cardW = 290, cardH = contentH - 8;
        for (int i = 0; i < SHIP_COUNT; i++) {
            int cx = 40 + i * (cardW + 14);
            const ShipDef& s = SHIPS[i];
            bool owned = i < save.unlockedShips;
            bool active = save.ship == i;
            bool hov = curInput && mouseIn(*curInput, cx, contentY, cardW, cardH);
            panel(fb, cx, contentY, cardW, cardH, active ? rgba(140, 255, 200) : s.accent, hov ? 1.f : .8f);
            drawTextCenter(fb, cx + cardW / 2, contentY + 10, s.name, active ? rgba(160, 255, 210) : rgba(235, 246, 252), 2, 1);
            drawTextCenter(fb, cx + cardW / 2, contentY + 26, s.role, withAlpha(s.accent, 220), 1, 1);
            // portrait
            fb.blitScaled(art().ships[i].body, cx + 10, contentY + 38, cardW - 20, cardW - 20, owned ? 1.f : .45f, false);
            int y = contentY + cardW - 6;
            auto sbar = [&](const char* label, float frac, Color c) {
                drawText(fb, cx + 14, y, label, withAlpha(rgba(160, 190, 210), 200), 1, 1);
                fb.fillRect(cx + 76, y + 1, cardW - 100, 4, withAlpha(rgba(10, 16, 22), 220));
                fb.fillRect(cx + 76, y + 1, (int)((cardW - 100) * clampf(frac, 0, 1)), 4, c);
                y += 12;
            };
            sbar("HULL", s.hull / 300.f, rgba(220, 90, 90));
            sbar("SHIELD", s.shield / 240.f, rgba(90, 160, 240));
            sbar("SPEED", s.speed / 320.f, rgba(140, 220, 255));
            sbar("CARGO", s.cargo / 450.f, rgba(120, 220, 160));
            sbar("MINING", s.miningPower / 1.7f, rgba(255, 210, 120));
            sbar("DAMAGE", s.damage / 28.f, rgba(255, 140, 160));
            y += 4;
            drawText(fb, cx + 14, y, s.desc, withAlpha(rgba(150, 180, 200), 180), 1, 1);
            // multi-line desc
            std::string d1 = s.desc;
            if (d1.size() > 42) {
                fb.fillRect(cx + 14, y, cardW - 28, 1, withAlpha(rgba(20, 30, 40), 0));
            }
            int by = contentY + cardH - 40;
            if (active) {
                drawTextCenter(fb, cx + cardW / 2, by + 10, "ACTIVE HULL", rgba(140, 255, 200), 2, 1);
            } else if (owned) {
                if (button(fb, 500 + i, cx + 14, by, cardW - 28, 30, "FLY THIS SHIP", s.accent, true)) {
                    buyShip(i);
                    if (!headless) saveGame();
                }
            } else if (i == save.unlockedShips) {
                bool afford = save.credits >= s.cost;
                if (button(fb, 500 + i, cx + 14, by, cardW - 28, 30,
                           fmt("BUY  %s CR", money(s.cost).c_str()), s.accent, afford)) {
                    if (buyShip(i)) { if (!headless) saveGame(); }
                }
            } else {
                drawTextCenter(fb, cx + cardW / 2, by + 10, "LOCKED", withAlpha(rgba(140, 150, 160), 220), 2, 1);
            }
        }
    }
    // ---------------------------------------------------------- DRONES
    else if (uiTab == 3) {
        panel(fb, 40, contentY, 560, contentH, sd.accent, .85f);
        drawText(fb, 56, contentY + 12, "DRONE FLEET", rgba(220, 240, 250), 2, 1);
        drawTextRight(fb, 584, contentY + 14, fmt("%d ONLINE", (int)drones.size()), withAlpha(sd.accent, 220), 1, 1);
        for (int i = 0; i < 8; i++) {
            int y = contentY + 42 + i * 40;
            if (y > contentY + contentH - 46) break;
            bool online = i < (int)drones.size();
            fb.fillRect(52, y, 536, 34, withAlpha(rgba(16, 24, 34), 200));
            if (online) {
                int tier = clampv((int)drones[i].tier, 0, 2);
                fb.blitScaled(art().drones[tier], 54, y + 1, 32, 32, 1.f, false);
                drawText(fb, 94, y + 5, fmt("DRONE %02d  //  %s", i + 1, DRONES[tier].name), rgba(220, 240, 250), 1, 1);
                const char* st = drones[i].state == DR_MINE ? "MINING" : drones[i].state == DR_RETURN ? "RETURNING" : "SEEKING";
                drawText(fb, 94, y + 18, fmt("STATE %s   CARGO %.0f/%.0f", st, drones[i].cargoAmt, DRONES[tier].capacity),
                         withAlpha(rgba(150, 210, 200), 200), 1, 1);
                Color sc = drones[i].state == DR_MINE ? rgba(255, 210, 120) : rgba(140, 200, 255);
                fb.fillRect(560, y + 8, 12, 12, withAlpha(sc, 220));
            } else {
                drawText(fb, 94, y + 12, fmt("SLOT %02d  //  EMPTY", i + 1), withAlpha(rgba(110, 130, 145), 180), 1, 1);
            }
        }
        panel(fb, 620, contentY, VW - 660, contentH, sd.accent, .85f);
        int tier = clampv(save.droneTier, 0, 2);
        drawText(fb, 636, contentY + 12, "DRONE BAY", rgba(220, 240, 250), 2, 1);
        for (int t = 0; t < 3; t++) {
            int y = contentY + 42 + t * 96;
            bool sel = save.droneTier == t;
            fb.fillRect(636, y, VW - 692, 86, withAlpha(sel ? rgba(30, 52, 62) : rgba(16, 24, 34), 210));
            if (sel) fb.fillRect(636, y, 3, 86, sd.accent);
            fb.blitScaled(art().drones[t], 642, y + 8, 60, 60, 1.f, false);
            drawText(fb, 710, y + 10, DRONES[t].name, rgba(230, 244, 252), 2, 1);
            drawText(fb, 710, y + 26, DRONES[t].desc, withAlpha(rgba(150, 180, 200), 180), 1, 1);
            drawText(fb, 710, y + 44, fmt("MINING %.2f   SPEED %.0f   HOLD %.0f", DRONES[t].miningPower, DRONES[t].speed,
                                          DRONES[t].capacity), withAlpha(rgba(150, 210, 200), 200), 1, 1);
            drawText(fb, 710, y + 58, fmt("UNIT COST %s CR", money(DRONES[t].cost).c_str()),
                     withAlpha(rgba(255, 216, 120), 210), 1, 1);
            if (curInput && mouseIn(*curInput, 636, y, VW - 692, 86) && curInput->mouseReleased[0]) {
                save.droneTier = t;
                rebuildDrones();
                audio::playUiClick();
            }
            if (sel) drawTextRight(fb, VW - 46, y + 10, "SELECTED", withAlpha(sd.accent, 230), 1, 1);
        }
        int maxDrones = 4 + save.upgrades[UPG_DRONES] * 2;
        int by = contentY + contentH - 76;
        drawText(fb, 636, by - 18, fmt("FLEET %d / %d   (DRONE BAYS UPGRADE RAISES THE CAP)", (int)drones.size(), maxDrones),
                 withAlpha(rgba(150, 180, 200), 190), 1, 1);
        bool canBuy = (int)drones.size() < maxDrones && save.credits >= DRONES[tier].cost;
        if (button(fb, 600, 636, by, 240, 34, fmt("BUY DRONE  %s", fmtBig(DRONES[tier].cost).c_str()), sd.accent, canBuy)) {
            if (buyDrone()) { if (!headless) saveGame(); }
        }
        if (button(fb, 601, 890, by, 240, 34, "SELL ALL CARGO", sd.accent, totalCargo() > 0)) {
            sellAll();
        }
    }
    // ---------------------------------------------------------- ACHIEVEMENTS (in station)
    else if (uiTab == 4) {
        int cols = 4, cw = 296, ch = 76;
        int unlocked = 0;
        for (auto& a : achievements) if (a.unlocked) unlocked++;
        drawTextRight(fb, VW - 44, contentY - 4, fmt("%d / %d UNLOCKED", unlocked, ACH_COUNT), sd.accent, 1, 1);
        for (int i = 0; i < ACH_COUNT; i++) {
            int cx = 40 + (i % cols) * (cw + 12);
            int cy = contentY + 14 + (i / cols) * (ch + 8);
            bool un = achievements[i].unlocked;
            panel(fb, cx, cy, cw, ch, un ? ACHIEVEMENTS[i].color : rgba(90, 100, 110), un ? .92f : .55f);
            fb.blitScaled(art().achIcon[i], cx + 8, cy + 12, 50, 50, un ? 1.f : .3f, false);
            drawText(fb, cx + 68, cy + 14, ACHIEVEMENTS[i].name, un ? ACHIEVEMENTS[i].color : rgba(130, 145, 155), 2, 1);
            drawText(fb, cx + 68, cy + 32, ACHIEVEMENTS[i].desc, un ? withAlpha(rgba(210, 228, 240), 210)
                                                                   : withAlpha(rgba(120, 135, 148), 170), 1, 1);
            drawText(fb, cx + 68, cy + 54, un ? "UNLOCKED" : "LOCKED",
                     un ? rgba(140, 255, 190) : rgba(110, 120, 130), 1, 1);
        }
    }

    // ---------------------------------------------------------- footer
    bool canUndock = true;
    if (button(fb, 700, 40, VH - 56, 200, 34, docked ? "UNDOCK  [E]" : "BACK TO FLIGHT", sd.accent, canUndock))
        undock();
    if (button(fb, 701, 252, VH - 56, 180, 34, "SECTOR CHART", sd.accent)) { screen = 4; mapSel = sector; }
    if (button(fb, 702, 444, VH - 56, 150, 34, "SETTINGS", sd.accent)) { pendingScreen = 5; settingsFrom = 3; }
    if (button(fb, 703, 606, VH - 56, 150, 34, "MAIN MENU", sd.accent)) { saveGame(true); pendingScreen = 1; }
    drawTextRight(fb, VW - 24, VH - 28, fmt("HULL %d%%   SHIELD %d%%   DRONES %d", (int)(100 * hull / hullMax),
                                            (int)(100 * shield / std::max(1.f, shieldMax)), (int)drones.size()),
                  withAlpha(rgba(140, 175, 195), 180), 1, 1);

    // keyboard shortcuts
    if (curInput) {
        if (curInput->wasPressed(keyOf('S'))) sellAll();
        if (curInput->wasPressed(keyOf('E')) || curInput->wasPressed(K_ESC)) {
            if (curInput->isDown(K_SHIFT)) { saveGame(true); pendingScreen = 1; }
            else undock();
        }
        if (curInput->wasPressed(K_TAB)) uiTab = (uiTab + 1) % 5;

    }
}

// ---------------------------------------------------------------- sector chart
void Game::renderMap(Canvas& fb) {
    const SectorDef& sd = SECTORS[sector];
    fb.fillRect(0, 0, VW, VH, withAlpha(rgba(3, 6, 11), 226));
    // star chart grid
    for (int x = 0; x < VW; x += 40) fb.fillRect(x, 0, 1, VH, withAlpha(sd.accent, 6));
    for (int y = 0; y < VH; y += 40) fb.fillRect(0, y, VW, 1, withAlpha(sd.accent, 6));
    fb.blitScaled(art().stars[sector], 0, 0, VW, VH, .55f, true);
    screenHeader(fb, "SECTOR CHART", "JUMP DRIVE ARMED  //  SELECT DESTINATION", sd.accent);

    // nodes along a curve
    for (int i = 0; i < 5; i++) {
        const SectorDef& s = SECTORS[i];
        bool unlocked = i < save.unlockedSectors;
        bool current = i == sector;
        bool next = i == save.unlockedSectors;
        float t = i / 4.f;
        int nx = 150 + (int)(t * 570.f) + (int)(sinf(t * PI) * 70.f);
        int ny = 470 - (int)(t * 220.f) + (int)(sinf(t * PI * 2.f) * 40.f);
        Vec2 np((float)nx, (float)ny);
        // connection line
        if (i < 4) {
            float t2 = (i + 1) / 4.f;
            int nx2 = 150 + (int)(t2 * 570.f) + (int)(sinf(t2 * PI) * 70.f);
            int ny2 = 470 - (int)(t2 * 220.f) + (int)(sinf(t2 * PI * 2.f) * 40.f);
            bool linkOpen = (i + 1) < save.unlockedSectors;
            Color lc = linkOpen ? withAlpha(s.accent, 130) : withAlpha(rgba(70, 85, 100), 90);
            // dashed
            for (int k = 0; k < 22; k++) {
                if (k % 2) continue;
                float f0 = k / 22.f, f1 = (k + 1) / 22.f;
                fb.line({nx + (nx2 - nx) * f0, ny + (ny2 - ny) * f0}, {nx + (nx2 - nx) * f1, ny + (ny2 - ny) * f1},
                        lc, 1.6f);
            }
        }
        float hov = (curInput && length(Vec2(curInput->mouseX, curInput->mouseY) - np) < 74.f) ? 1.f : 0.f;
        bool sel = mapSel == i;
        // node body
        Color nc = unlocked ? s.accent : rgba(90, 100, 112);
        if (current) {
            float pulse = .5f + .5f * sinf(uiTime * 3.f);
            fb.ring(np.x, np.y, 58.f + pulse * 6.f, 1.4f, withAlpha(nc, (int)(120 * pulse + 60)));
        }
        fb.circleFill(np.x, np.y, 44.f, withAlpha(unlocked ? scaleColor(nc, .22f) : rgba(20, 26, 32), 240));
        fb.ring(np.x, np.y, 44.f, 2.f, withAlpha(nc, hov > 0 ? 255 : 200));
        fb.ring(np.x, np.y, 34.f, 1.f, withAlpha(nc, 110));
        // mini planet
        fb.blitScaled(art().planets[i], nx - 34, ny - 34, 68, 68, unlocked ? .95f : .25f, false);
        if (!unlocked) {
            // lock glyph
            fb.fillRect(nx - 8, ny - 2, 16, 12, withAlpha(rgba(180, 190, 200), 200));
            fb.ring(nx, ny - 3, 7.f, 2.4f, withAlpha(rgba(180, 190, 200), 200));
        }
        drawTextCenter(fb, nx, ny + 52, s.name, unlocked ? s.accent : withAlpha(rgba(120, 130, 140), 220), 2, 1);
        drawTextCenter(fb, nx, ny + 70, s.subtitle, withAlpha(rgba(150, 180, 200), 180), 1, 1);
        // danger meter
        for (int k = 0; k < 5; k++) {
            Color dc = k < (int)(s.danger * 5.f + .5f) ? rgba(255, 110, 80) : rgba(50, 60, 70);
            fb.fillRect(nx - 20 + k * 9, ny + 82, 7, 4, withAlpha(dc, 210));
        }
        // interaction
        if (curInput && hov > 0 && curInput->mouseReleased[0]) {
            mapSel = i;
            audio::playUiClick();
        }
        if (current) drawTextCenter(fb, nx, ny + 96, "CURRENT", withAlpha(rgba(140, 255, 200), 220), 1, 1);
        else if (next) drawTextCenter(fb, nx, ny + 96, fmt("UNLOCK %s CR", fmtBig(s.unlockCost).c_str()),
                                      withAlpha(rgba(255, 216, 120), 220), 1, 1);
    }
    // detail card
    {
        const SectorDef& s = SECTORS[clampv(mapSel, 0, 4)];
        int cx = 880, cy = 150, cw = 360, ch = 400;
        panel(fb, cx, cy, cw, ch, s.accent, .92f);
        fb.blitScaled(art().planets[mapSel], cx + 30, cy + 20, 300, 300, .8f, false);
        drawText(fb, cx + 18, cy + 312, s.name, s.accent, 2, 1);
        drawText(fb, cx + 18, cy + 330, s.lore, withAlpha(rgba(180, 205, 222), 200), 1, 1);
        // resources
        int ry = cy + 346;
        for (int k = 0; k < 4; k++) {
            if (s.resList[k] < 0) continue;
            fb.blitScaled(art().resIcon[s.resList[k]], cx + 18 + k * 38, ry, 30, 30, 1.f, false);
            fb.fillRect(cx + 18 + k * 38, ry + 32, 30, 2, withAlpha(RESOURCES[s.resList[k]].color, 170));
        }
        bool unlocked = mapSel < save.unlockedSectors;
        int by = cy + ch - 44;
        if (mapSel == sector) {
            drawTextCenter(fb, cx + cw / 2, by + 8, "YOU ARE HERE", withAlpha(rgba(140, 255, 200), 230), 2, 1);
        } else if (unlocked) {
            if (button(fb, 800, cx + 18, by, cw - 36, 32, "ENGAGE JUMP DRIVE", s.accent, true)) {
                if (selectSector(mapSel)) { screen = 2; }
            }
        } else if (mapSel == save.unlockedSectors) {
            bool afford = save.credits >= s.unlockCost;
            if (button(fb, 801, cx + 18, by, cw - 36, 32, fmt("UNLOCK  %s CR", money(s.unlockCost).c_str()), s.accent, afford)) {
                if (unlockSector(mapSel)) {
                    if (selectSector(mapSel)) { screen = 2; }
                }
            }
        } else {
            drawTextCenter(fb, cx + cw / 2, by + 8, "LOCKED - CHART A PATH FIRST", withAlpha(rgba(150, 160, 170), 220), 1, 1);
        }
    }
    if (button(fb, 810, 40, VH - 56, 170, 32, "CLOSE  [M]", sd.accent)) screen = 2;
    if (curInput && (curInput->wasPressed(K_ESC) || curInput->wasPressed(keyOf('M')))) screen = 2;
    if (curInput && curInput->wasPressed(K_LEFT)) mapSel = std::max(0, mapSel - 1);
    if (curInput && curInput->wasPressed(K_RIGHT)) mapSel = std::min(4, mapSel + 1);
}

// ---------------------------------------------------------------- settings
void Game::renderSettings(Canvas& fb) {
    fb.fillRect(0, 0, VW, VH, withAlpha(rgba(3, 6, 11), 220));
    const SectorDef& sd = SECTORS[sector];
    screenHeader(fb, "SYSTEMS", "AUDIO  //  VIDEO  //  DATA", sd.accent);
    int cx = VW / 2 - 380, cy = 110, cw = 760, ch = 500;
    panel(fb, cx, cy, cw, ch, sd.accent, .92f);

    auto slider = [&](int id, int y, const char* label, float& value) {
        drawText(fb, cx + 28, y, label, rgba(220, 238, 250), 2, 1);
        int bx = cx + 250, bw = 420;
        fb.fillRect(bx, y + 6, bw, 8, withAlpha(rgba(12, 18, 26), 230));
        fb.fillRect(bx, y + 6, (int)(bw * value), 8, withAlpha(sd.accent, 220));
        for (int i = 0; i <= 10; i++) fb.fillRect(bx + i * bw / 10, y + 4, 1, 12, withAlpha(rgba(0, 0, 0), 90));
        int knobX = bx + (int)(bw * value);
        fb.fillRect(knobX - 3, y + 1, 6, 18, rgba(235, 248, 255));
        drawTextRight(fb, cx + cw - 28, y, fmt("%d%%", (int)(value * 100.f)), withAlpha(sd.accent, 230), 2, 1);
        if (curInput) {
            bool inBar = curInput->mouseX > bx - 8 && curInput->mouseX < bx + bw + 8 &&
                         curInput->mouseY > y - 2 && curInput->mouseY < y + 22;
            if (inBar && (curInput->mouseDown[0] || curInput->mousePressed[0])) {
                value = clampf((curInput->mouseX - bx) / (float)bw, 0.f, 1.f);
                applySettings();
            }
            (void)id;
        }
    };
    slider(900, cy + 34, "MASTER VOLUME", save.settings.masterVolume);
    slider(901, cy + 76, "EFFECTS VOLUME", save.settings.sfxVolume);

    auto toggle = [&](int id, int y, const char* label, bool& flag) {
        drawText(fb, cx + 28, y, label, rgba(220, 238, 250), 2, 1);
        int bx = cx + cw - 148;
        fb.fillRect(bx, y - 4, 120, 26, withAlpha(flag ? scaleColor(sd.accent, .35f) : rgba(16, 24, 32), 230));
        fb.rectOutline((float)bx, (float)y - 4, 120, 26, 1.f, withAlpha(flag ? sd.accent : rgba(90, 105, 118), 220));
        drawTextCenter(fb, bx + 60, y + 3, flag ? "ON" : "OFF", flag ? rgba(235, 255, 245) : rgba(150, 165, 178), 2, 1);
        if (curInput && mouseIn(*curInput, bx, y - 4, 120, 26) && curInput->mouseReleased[0]) {
            flag = !flag;
            audio::playUiClick();
            (void)id;
        }
    };
    toggle(902, cy + 130, "SCREEN SHAKE", save.settings.screenShake);
    toggle(903, cy + 172, "SHOW FPS", save.settings.showFps);
    toggle(904, cy + 214, "CRT SCANLINES", save.settings.scanlines);
    toggle(905, cy + 256, "AUTOSAVE", save.settings.autosave);

    // quality
    drawText(fb, cx + 28, cy + 306, "PARTICLE QUALITY", rgba(220, 238, 250), 2, 1);
    const char* q[3] = {"LOW", "NORMAL", "HIGH"};
    for (int i = 0; i < 3; i++) {
        int bx = cx + 250 + i * 140;
        if (button(fb, 910 + i, bx, cy + 300, 128, 28, q[i], sd.accent, true, save.settings.quality == i)) {
            save.settings.quality = i;
        }
    }
    // window mode
    drawText(fb, cx + 28, cy + 352, "WINDOW MODE", rgba(220, 238, 250), 2, 1);
    const char* wm[2] = {"WINDOWED", "BORDERLESS"};
    for (int i = 0; i < 2; i++) {
        int bx = cx + 250 + i * 180;
        if (button(fb, 920 + i, bx, cy + 346, 168, 28, wm[i], sd.accent, true, save.settings.fullscreen == i)) {
            save.settings.fullscreen = i;
            platformSetBorderless(i == 1);
        }
    }
    drawText(fb, cx + 28, cy + 400, "SAVE FILE", withAlpha(rgba(160, 190, 210), 200), 1, 1);
    drawText(fb, cx + 130, cy + 400, std::string(platformSaveDir()) + "space_miner_save.dat",
             withAlpha(rgba(130, 165, 185), 170), 1, 1);
    drawText(fb, cx + 28, cy + 416, "ALL PROGRESS IS STORED LOCALLY WITH CHECKSUM VALIDATION AND A BACKUP COPY.",
             withAlpha(rgba(130, 160, 180), 150), 1, 1);

    // buttons
    int by = cy + ch - 54;
    if (button(fb, 930, cx + 28, by, 200, 36, "APPLY & BACK", sd.accent)) {
        applySettings();
        saveGame(true);
        pendingScreen = settingsFrom;
    }
    if (button(fb, 931, cx + 244, by, 200, 36, "SAVE NOW", sd.accent)) saveGame(true);
    if (button(fb, 932, cx + 460, by, 272, 36, "SAVE & RETURN TO MENU", sd.accent)) {
        applySettings();
        saveGame(true);
        pendingScreen = 1;
    }
    if (curInput && curInput->wasPressed(K_ESC)) {
        applySettings();
        pendingScreen = settingsFrom;
    }
    if (saveFlashT > 0) {
        drawTextRight(fb, cx + cw - 28, by - 20, "SAVED", withAlpha(rgba(140, 255, 190), (int)(255 * clampf(saveFlashT, 0, 1))), 2, 1);
    }
}

// ---------------------------------------------------------------- achievements screen
void Game::renderAchievements(Canvas& fb) {
    const SectorDef& sd = SECTORS[sector];
    menuBackdrop(*this, fb);
    fb.fillRect(0, 0, VW, VH, withAlpha(rgba(4, 7, 12), 190));
    screenHeader(fb, "SERVICE RECORD", "EVERYTHING YOU HAVE DONE OUT THERE", sd.accent);
    int unlocked = 0;
    for (auto& a : achievements) if (a.unlocked) unlocked++;
    drawText(fb, 40, 82, fmt("%d / %d ACHIEVEMENTS", unlocked, ACH_COUNT), rgba(220, 240, 250), 2, 1);
    drawText(fb, 320, 84, fmt("MINED %lld   KILLS %lld   JUMPS %lld   FLIGHT %d:%02d", save.asteroidsMined,
                              save.enemiesKilled, save.jumps, (int)(playTime / 60), (int)playTime % 60),
             withAlpha(rgba(150, 190, 210), 200), 1, 1);
    int cols = 4, cw = 296, ch = 108;
    for (int i = 0; i < ACH_COUNT; i++) {
        int cx = 40 + (i % cols) * (cw + 12);
        int cy = 112 + (i / cols) * (ch + 10);
        bool un = achievements[i].unlocked;
        panel(fb, cx, cy, cw, ch, un ? ACHIEVEMENTS[i].color : rgba(90, 100, 110), un ? .92f : .55f);
        fb.blitScaled(art().achIcon[i], cx + 10, cy + 30, 50, 50, un ? 1.f : .3f, false);
        drawText(fb, cx + 70, cy + 18, ACHIEVEMENTS[i].name, un ? ACHIEVEMENTS[i].color : rgba(130, 145, 155), 2, 1);
        drawText(fb, cx + 70, cy + 38, ACHIEVEMENTS[i].desc, un ? withAlpha(rgba(210, 228, 240), 210)
                                                               : withAlpha(rgba(120, 135, 148), 170), 1, 1);
        drawText(fb, cx + 70, cy + 60, un ? "UNLOCKED" : "LOCKED",
                 un ? rgba(140, 255, 190) : rgba(110, 120, 130), 1, 1);
        if (un) fb.fillRect(cx, cy, 3, ch, withAlpha(ACHIEVEMENTS[i].color, 230));
    }
    if (button(fb, 950, 40, VH - 56, 200, 34, "BACK", sd.accent)) screen = 1;
    if (curInput && (curInput->wasPressed(K_ESC) || curInput->wasPressed(K_BACKSPACE))) screen = 1;
}

// ---------------------------------------------------------------- death
void Game::renderDeath(Canvas& fb) {
    fb.fillRect(0, 0, VW, VH, withAlpha(rgba(30, 4, 6), 170));
    float t = clampf(deathT / .8f, 0.f, 1.f);
    int cy = (int)(VH * .34f);
    drawHeading(fb, VW / 2, cy, "HULL BREACH", rgba(255, 120, 100), rgba(255, 60, 40), 5, 7);
    drawTextCenter(fb, VW / 2, cy + 60, deathCause.empty() ? "THE BELT CLAIMED ANOTHER ONE" : upper(deathCause),
                   withAlpha(rgba(255, 200, 180), (int)(220 * t)), 1, 2);
    int cx = VW / 2 - 260, y = cy + 110;
    panel(fb, cx, y, 520, 170, rgba(255, 120, 100), .9f);
    drawText(fb, cx + 20, y + 16, "EMERGENCY RECOVERY", rgba(255, 200, 180), 2, 1);
    drawText(fb, cx + 20, y + 42, fmt("CREDITS LOST      %s", money((long long)(save.credits * .25)).c_str()),
             withAlpha(rgba(255, 170, 150), 220), 1, 1);
    drawText(fb, cx + 20, y + 58, fmt("CARGO LOST         %d UNITS", (int)cargoUsed),
             withAlpha(rgba(255, 170, 150), 220), 1, 1);
    drawText(fb, cx + 20, y + 82, fmt("SECTOR             %s", SECTORS[sector].name), withAlpha(rgba(200, 220, 235), 210), 1, 1);
    drawText(fb, cx + 20, y + 98, fmt("MINED THIS RUN     %lld ROCKS", save.asteroidsMined), withAlpha(rgba(200, 220, 235), 210), 1, 1);
    drawText(fb, cx + 20, y + 122, "INSURANCE RESTORES YOUR SHIP AT THE STATION.",
             withAlpha(rgba(160, 180, 195), 180), 1, 1);
    int by = y + 196;
    if (button(fb, 960, VW / 2 - 262, by, 250, 40, "ACCEPT & RESPAWN", rgba(255, 170, 120))) {
        refundDeath();
        if (!headless) saveGame(true);
    }
    if (button(fb, 961, VW / 2 + 12, by, 250, 40, "RETURN TO MENU", rgba(180, 200, 220))) {
        refundDeath();
        screen = 1;
    }
    if (curInput && deathT > 1.f && curInput->wasPressed(K_ENTER)) {
        refundDeath();
    }
}

// ---------------------------------------------------------------- victory
void Game::renderVictory(Canvas& fb) {
    const SectorDef& sd = SECTORS[4];
    fb.fillRect(0, 0, VW, VH, withAlpha(rgba(3, 10, 12), 216));
    for (int i = 0; i < 5; i++) {
        float a = uiTime * .3f + i * 1.3f;
        fb.radialGlow(VW * .5f + cosf(a) * 340, VH * .45f + sinf(a * .9f) * 180, 300.f,
                      withAlpha(i % 2 ? rgba(60, 220, 200) : rgba(140, 90, 240), 40), 1.f);
    }
    drawHeading(fb, VW / 2, 120, "FRONTIER MASTERED", rgba(220, 255, 250), sd.accent, 4, 6);
    drawTextCenter(fb, VW / 2, 180, "YOU CROSSED THE VOID, BROKE A SENTINEL AND COMMISSIONED THE ARK.",
                   withAlpha(rgba(190, 230, 240), 220), 1, 1);
    int cx = VW / 2 - 300, cy = 226;
    panel(fb, cx, cy, 600, 300, sd.accent, .92f);
    drawTextCenter(fb, VW / 2, cy + 18, "FINAL RECORD", sd.accent, 2, 2);
    auto row = [&](int i, const char* label, const std::string& value) {
        int y = cy + 52 + i * 26;
        drawText(fb, cx + 24, y, label, withAlpha(rgba(180, 205, 220), 210), 1, 1);
        drawTextRight(fb, cx + 576, y, value, rgba(235, 250, 255), 1, 1);
    };
    row(0, "PILOT LEVEL", fmt("%d", save.level));
    row(1, "CREDITS", money(save.credits) + " CR");
    row(2, "TOTAL EARNED", money(save.totalEarned) + " CR");
    row(3, "ASTEROIDS MINED", intToStr(save.asteroidsMined));
    row(4, "HOSTILES DESTROYED", intToStr(save.enemiesKilled));
    row(5, "SECTORS CHARTED", fmt("%d / 5", save.unlockedSectors));
    row(6, "FLIGHT TIME", fmt("%d:%02d", (int)(playTime / 60), (int)playTime % 60));
    row(7, "DRONES COMMANDED", fmt("%d", (int)drones.size()));
    row(8, "VESSEL", SHIPS[save.ship].name);
    int by = cy + 320;
    if (button(fb, 970, VW / 2 - 320, by, 300, 40, "KEEP FLYING", sd.accent)) screen = 2;
    if (button(fb, 971, VW / 2 + 20, by, 300, 40, "MAIN MENU", sd.accent)) { saveGame(true); screen = 1; }
}

}  // namespace sm
