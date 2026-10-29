// FRACTURE — rendering: world pass, HUD, menus, overlays.
#include "game/game.h"
#include "game/globals.h"
#include "ui/ui.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

using namespace ui;

static const char* kTimeNames[24] = {"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11",
                                     "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23"};
static const char* kWeatherNames[5] = {"CLEAR", "CLOUDY", "RAIN", "STORM", "FOG"};
static const char* kQualityNames[4] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
static const char* kResNames[6] = {"1280x720", "1600x900", "1920x1080", "2560x1440", "3440x1440", "3840x2160"};
static const int kResW[6] = {1280, 1600, 1920, 2560, 3440, 3840};
static const int kResH[6] = {720, 900, 1080, 1440, 1440, 2160};

// ------------------------------------------------------------------ logo
void Game::drawLogo(float cx, float y, float scale) {
    float w = renderer->frameWidth(), h = renderer->frameHeight();
    (void)w;
    float titleSize = 62 * scale;
    // hazard bar under the wordmark
    ui.hazardStripe(cx - 300 * scale, y + titleSize + 46 * scale, 600 * scale, 5 * scale, ui::COL_ACCENT, 0xFF141414u);
    // wordmark with a hard drop shadow for readability over any backdrop
    ui.textSpaced(cx - ui.measure("FRACTURE", titleSize) * 0.5f / 1.0f, y + 3 * scale, titleSize, 0xFF000000u, "FRACTURE", 1.5f * scale);
    ui.textSpaced(cx - ui.measure("FRACTURE", titleSize) * 0.5f / 1.0f, y, titleSize, 0xFFFFFFFFu, "FRACTURE", 1.5f * scale);
    float subSize = 17 * scale;
    const char* sub = "TOTAL DESTRUCTION";
    ui.textSpaced(cx - ui.measure(sub, subSize) * 0.5f, y + titleSize + 16 * scale, subSize, ui::COL_ACCENT, sub, 5.0f * scale);
    (void)h;
}

void Game::drawMenuBackdrop() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    // darkening gradient over the live 3D backdrop
    ui.addQuadGrad(0, 0, w, h, 0xE6101216u, 0x40101216u);
    ui.addQuadGrad(0, 0, w * 0.62f, h, 0xF0101216u, 0x00101216u);
    // subtle scanlines + corner brackets for the industrial look
    ui.scanlines(Rect(0, 0, w, h), 0x08FFFFFFu, 3);
    float m = 18;
    ui.addRect(Rect(m, m, 2, h - m * 2), 0x30FFFFFFu);
    ui.addRect(Rect(m, m, w - m * 2, 2), 0x30FFFFFFu);
    ui.addRect(Rect(w - m - 2, m, 2, h - m * 2), 0x30FFFFFFu);
    ui.addRect(Rect(m, h - m - 2, w - m * 2, 2), 0x30FFFFFFu);
    // build tag
    ui.text(28, h - 26, 11, 0x60FFFFFFu, "FRACTURE 1.0  //  CUSTOM VOXEL ENGINE  //  BUILD 2026-10-29");
}

// ------------------------------------------------------------------ world pass
void Game::drawWorldEntities() {
    static std::vector<BoxInstance> boxes;
    boxes.clear();
    // physics bodies
    for (auto& b : physics.bodies) {
        if (!b.active) continue;
        float d = (b.pos - currentCamera().pos).len();
        if (d > 190.0f) continue;
        BoxInstance bi;
        bi.model = b.model;
        bi.color = b.color;
        bi.rough = material(b.mat).roughness;
        bi.metal = material(b.mat).metallic;
        if (b.heavy) bi.color = b.color * 0.92f;
        boxes.push_back(bi);
    }
    // placed charges
    for (auto& c : tools.charges) {
        if (!c.armed) continue;
        BoxInstance bi;
        bi.model = matTranslate(c.pos + c.normal * 0.06f) * matScale(vec3(0.22f, 0.16f, 0.09f));
        bi.color = vec3(0.85f, 0.75f, 0.15f);
        bi.emis = vec3(0.9f, 0.1f, 0.05f) * (0.35f + std::sin(g_time * 9.0f) * 0.35f);
        bi.rough = 0.4f;
        bi.metal = 0.6f;
        boxes.push_back(bi);
    }
    // rockets
    for (auto& r : tools.projectiles) {
        if (!r.active) continue;
        BoxInstance bi;
        bi.model = matTranslate(r.pos) * matScale(vec3(r.kind == 1 ? 0.3f : 0.16f, 0.16f, 0.7f));
        bi.color = r.kind == 1 ? vec3(0.35f, 0.36f, 0.34f) : vec3(0.75f, 0.62f, 0.2f);
        bi.emis = vec3(1.0f, 0.55f, 0.15f);
        boxes.push_back(bi);
    }
    // pickups
    for (auto& p : pickups) {
        if (!p.active) continue;
        float bob = std::sin(p.bob) * 0.08f;
        BoxInstance bi;
        vec3 c = p.kind == 0 ? vec3(0.52f, 0.40f, 0.24f) : (p.kind == 1 ? vec3(0.75f, 0.62f, 0.2f) : vec3(0.25f, 0.55f, 0.75f));
        bi.model = matTranslate(p.pos + vec3(0, bob, 0)) * matRotY(p.bob * 0.6f) * matScale(vec3(0.62f, 0.5f, 0.62f));
        bi.color = c;
        bi.emis = c * 0.25f;
        bi.rough = 0.6f;
        boxes.push_back(bi);
        BoxInstance glow;
        glow.model = matTranslate(p.pos + vec3(0, bob + 0.45f, 0)) * matScale(vec3(0.10f, 0.10f, 0.10f));
        glow.color = vec3(1, 1, 1);
        glow.emis = c * 1.6f;
        boxes.push_back(glow);
    }
    // door panels
    for (auto& d : doors) {
        if (!d.alive) continue;
        vec3 dir = d.dir * std::cos(d.open * 1.45f) + d.normal * std::sin(d.open * 1.45f);
        vec3 center = d.hinge + dir * (d.width * 0.5f) + vec3(0, d.height * 0.5f, 0);
        float yaw = std::atan2(dir.x, -dir.z);
        BoxInstance bi;
        bi.model = matTranslate(center) * matRotY(yaw) * matScale(vec3(d.width, d.height, 0.09f));
        bi.color = vec3(0.42f, 0.30f, 0.18f);
        bi.rough = 0.7f;
        bi.metal = 0.1f;
        boxes.push_back(bi);
    }
    security.appendBoxes(boxes, currentCamera().pos);
    if (!boxes.empty()) renderer->drawBoxes(boxes.data(), (int)boxes.size());
}

void Game::drawChargeMarkers() {
    // unarmed charges are already drawn; armed ones pulse in the entity pass
}

void Game::renderWorldView() {
    Camera cam = currentCamera();
    // dynamic lights for the GL renderer
    FXLight lights[4];
    int lc = 0;
    for (auto& l : fx.lights) {
        if (lc >= 4) break;
        lights[lc].pos = l.pos;
        lights[lc].color = l.color * std::min(1.0f, l.intensity);
        lights[lc].radius = l.radius;
        lc++;
    }
    // fire cells contribute light too, nearest first
    if (lc < 4) {
        int added = 0;
        for (auto& f : fx.fires) {
            if (lc >= 4 || added >= 2) break;
            vec3 p((f.cell.x * 4 + 2) * VOX, (f.cell.y * 4 + 2) * VOX, (f.cell.z * 4 + 2) * VOX);
            if ((p - cam.pos).len() > 40.0f) continue;
            lights[lc].pos = p;
            lights[lc].color = vec3(1.0f, 0.45f, 0.12f);
            lights[lc].radius = 9.0f + f.heat * 5.0f;
            lc++;
            added++;
        }
    }
    renderer->setExplosionLights(lights, lc);
    // flashlight when enabled and a torch beam is not venting light into the sky
    vec3 eye = cam.pos;
    renderer->setFlashlight(player.flashlightOn && player.flashlightBattery > 0, eye, cam.forward());
    renderer->setDamageFx(std::max(damageFlash, vignettePulse * 0.0f));
    renderer->beginFrame(cam, env, settings);
    renderer->drawWorld(world, settings);
    vehicles.draw(*renderer, cam.pos);
    drawWorldEntities();
    // the first-person tool rig only exists while actually playing
    if (drivenVehicle < 0 && (mode == MODE_PLAYING || mode == MODE_PAUSE || mode == MODE_MISSION_COMPLETE ||
                              mode == MODE_MISSION_FAILED))
        tools.drawViewModel(*renderer, player, cam, aspect());
    renderer->drawWater(world, env);
    fx.draw(*renderer);
    // UI list is built after; endFrame draws overlay in the renderer
    ui.begin(input, renderer->frameWidth(), renderer->frameHeight(), stats.updateMs * 0.001f);
    if (mode == MODE_PLAYING) {
        renderHUD();
    } else if (mode == MODE_PAUSE) {
        renderHUD();
        drawPauseMenu();
    } else if (mode == MODE_LOADING) {
        drawLoading();
    } else if (mode == MODE_MISSION_COMPLETE || mode == MODE_MISSION_FAILED) {
        renderHUD();
        drawMissionResult();
    } else {
        // menus
        if (mode == MODE_MAIN_MENU) drawMainMenu();
        else if (mode == MODE_CAMPAIGN) drawCampaignMenu();
        else if (mode == MODE_SANDBOX_SETUP) drawSandboxSetup();
        else if (mode == MODE_SETTINGS) drawSettings();
        else if (mode == MODE_CONTROLS) drawControls();
        else if (mode == MODE_CREDITS) drawCredits();
        else if (mode == MODE_SAVE_LOAD) drawSaveLoad();
        drawToasts();
    }
    ui.end();
    for (auto s : ui.soundQueue) audio::play(s, vec3(0), s == SFX_UI_CLICK ? 0.5f : 0.25f, 1.0f);
    ui.soundQueue.clear();
    renderer->drawOverlay(ui.list);
    renderer->endFrame();
}

void Game::render() {
    renderWorldView();
}

// ------------------------------------------------------------------ HUD
void Game::drawCrosshair() {
    float cx = renderer->frameWidth() * 0.5f, cy = renderer->frameHeight() * 0.5f;
    bool aiming = player.aimValid;
    int tool = tools.current;
    float spread = 7.0f;
    if (tool == TOOL_SHOTGUN) spread = 16.0f;
    else if (tool == TOOL_HAMMER) spread = 9.0f;
    else if (tool == TOOL_ROCKET) spread = 12.0f;
    else if (tool == TOOL_AIRSTRIKE) spread = 20.0f;
    if (player.sprinting) spread *= 1.35f;
    uint32_t col = aiming ? 0xF0FFFFFFu : 0x90FFFFFFu;
    if (tools.hitFlash > 0) col = 0xFFFF5544u;
    if (tool >= TOOL_BLOWTORCH && tool <= TOOL_DRILL) col = 0xFFFFAA33u;
    // four ticks
    float len = 6.0f;
    ui.addQuad(cx - spread - len, cy - 1, len, 2, col, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    ui.addQuad(cx + spread, cy - 1, len, 2, col, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    ui.addQuad(cx - 1, cy - spread - len, 2, len, col, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    ui.addQuad(cx - 1, cy + spread, 2, len, col, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    // centre dot
    ui.addQuad(cx - 1, cy - 1, 2, 2, 0xFFFFFFFFu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    // hitmarker
    if (hitmarker > 0) {
        float a = clampf(hitmarker, 0, 1);
        uint32_t hc = withAlpha(0xFFFFFFFFu, (int)(a * 220));
        for (int i = 0; i < 4; i++) {
            float dx = (i & 1) ? 1 : -1, dy = (i & 2) ? 1 : -1;
            ui.addQuad(cx + dx * 9 - (dx > 0 ? 0 : 8), cy + dy * 9 - (dy > 0 ? 0 : 1), 8, 1, hc,
                       ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                    }
    }
    // interaction prompt / carried guard
    drawInteractionPrompt();
}

void Game::drawInteractionPrompt() {
    float w = renderer->frameWidth(), h = renderer->frameHeight();
    const char* prompt = nullptr;
    char buf[128];
    if (player.carried) prompt = "E  THROW    Q  DROP";
    else if (drivenVehicle < 0 && player.aimValid) {
        Body* b = physics.nearestProp(player.pos + (player.aimHit.pos - player.pos) * 0.5f, 1.4f);
        int vi = -1;
        Vehicle* v = vehicles.nearest(player.pos, 3.4f, &vi);
        if (v && (v->pos - player.pos).len() < 3.4f) {
            snprintf(buf, sizeof(buf), "E  ENTER  %s", vehicleDef(v->kind).name);
            prompt = buf;
        } else if (b) {
            prompt = "E  PICK UP    ";
        } else if (player.aimHit.mat == M_GLASS) {
            prompt = nullptr;
        }
    }
    if (prompt) {
        float size = 15;
        float tw = ui.measure(prompt, size);
        Rect r(w * 0.5f - tw * 0.5f - 18, h * 0.5f + 64, tw + 36, 30);
        ui.panel(r, 0xD0121418u, false);
        ui.addRect(Rect(r.x, r.y, 3, r.h), ui::COL_ACCENT);
        ui.textCentered(r, size, 0xFFEAEAEAu, prompt);
    }
  if (player.noclip) ui.text(28, 96, 13, 0xFFFFAA00u, "NOCLIP");
}

void Game::drawObjectivesPanel() {
    if (playMode != PLAY_CAMPAIGN || !g_missions.state.active) return;
    const Mission& m = g_missions.get(g_missions.state.missionId);
    int w = renderer->frameWidth();
    float x = w - 372, y = 96;
    float panelH = 54 + m.objectives.size() * 46;
    if (g_missions.state.timed) panelH += 40;
    Rect r(x, y, 348, panelH);
    ui.panel(r, 0xC80E1116u, true);
    ui.addRect(Rect(r.x, r.y, r.w, 3), ui::COL_ACCENT);
    ui.textSpaced(r.x + 14, r.y + 12, 12, ui::COL_ACCENT, m.name, 1.6f);
    ui.text(r.x + 14, r.y + 30, 11, 0x99FFFFFFu, "OBJECTIVES");
    float yy = r.y + 50;
    for (size_t i = 0; i < m.objectives.size(); i++) {
        const Objective& o = m.objectives[i];
        uint32_t c = o.complete ? ui::COL_GOOD : (o.failed ? ui::COL_DANGER : 0xFFE8E8E8u);
        // checkbox
        ui.border(Rect(r.x + 14, yy + 3, 12, 12), c, 1.0f);
        if (o.complete) ui.addRect(Rect(r.x + 17, yy + 6, 6, 6), c);
        if (o.failed) {
            ui.addQuad(r.x + 14, yy + 8, 12, 2, c, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
        }
        ui.text(r.x + 34, yy, 13, c, o.text);
        if (!o.complete && !o.status.empty()) {
            ui.textRight(Rect(r.x, yy + 15, r.w - 14, 14), 11, 0x88FFFFFFu, o.status);
        }
        // progress bar
        if (!o.complete && o.kind != OBJ_REACH) {
            ui.progress(Rect(r.x + 34, yy + 16, r.w - 54, 3), clampf(o.progress, 0, 1),
                        o.failed ? ui::COL_DANGER : ui::COL_ACCENT);
        }
        yy += 46;
    }
    if (g_missions.state.timed) {
        float remaining = g_missions.state.timeLimit - g_missions.state.time;
        int mm = (int)std::max(0.0f, remaining) / 60;
        int ss = (int)std::max(0.0f, remaining) % 60;
        char tb[32];
        snprintf(tb, sizeof(tb), "%d:%02d", mm, ss);
        Rect tr(r.x + 14, r.y + panelH - 34, r.w - 28, 22);
        ui.addRect(tr, 0xFF1A0E0Eu);
        ui.progress(tr, clampf(remaining / std::max(1.0f, g_missions.state.timeLimit), 0, 1),
                    remaining < 20 ? ui::COL_DANGER : ui::COL_ACCENT_DIM);
        ui.textCentered(tr, 13, 0xFFFFFFFFu, std::string("TIME  ") + tb);
    }
}

void Game::drawToolBar() {
    if (drivenVehicle >= 0) return;
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    const int n = TOOL_COUNT - 1;
    float slotW = 58, gap = 3;
    float totalW = n * (slotW + gap) - gap;
    float x0 = w * 0.5f - totalW * 0.5f;
    float y = h - 76;
    for (int i = 1; i < TOOL_COUNT; i++) {
        Rect r(x0 + (i - 1) * (slotW + gap), y, slotW, 44);
        bool sel = (tools.current == i);
        ui.panel(r, sel ? 0xF01C2129u : 0xC012151Au, false);
        if (sel) {
            ui.addRect(Rect(r.x, r.y + r.h - 3, r.w, 3), ui::COL_ACCENT);
            ui.border(r, ui::COL_ACCENT, 1.0f);
        }
        char num[4];
        snprintf(num, sizeof(num), "%d", i > 9 ? 0 : i);
        ui.text(r.x + 5, r.y + 3, 10, sel ? ui::COL_ACCENT : 0x70FFFFFFu, num);
        // tool glyph: a simple icon in the UI texture space, built from rectangles
        float gx = r.x + 12, gy = r.y + 15;
        uint32_t gc = sel ? 0xFFFFFFFFu : 0xB0C8C8C8u;
        switch (i) {
            case TOOL_HAMMER:
                ui.addQuad(gx + 14, gy - 6, 5, 20, 0xFF8A5A2Bu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 4, gy - 10, 26, 8, gc, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_SHOTGUN:
                ui.addQuad(gx, gy, 34, 4, gc, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 22, gy + 4, 10, 5, 0xFF7A5A38u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_CHARGES:
                ui.addQuad(gx + 6, gy - 4, 18, 14, 0xFFB8A23Cu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 12, gy - 10, 4, 6, 0xFFCC3333u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_ROCKET:
                ui.addQuad(gx + 2, gy - 2, 30, 8, 0xFF556B2Fu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 30, gy, 6, 4, 0xFFCC8833u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_BLOWTORCH:
                ui.addQuad(gx + 2, gy, 22, 7, gc, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 24, gy + 1, 8, 5, 0xFFFF8822u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_PLANK:
                ui.addQuad(gx, gy - 2, 36, 7, 0xFF9A6E3Cu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_EXTINGUISHER:
                ui.addQuad(gx + 8, gy - 8, 14, 26, 0xFFB22222u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 20, gy - 6, 10, 3, gc, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_GRAPPLE:
                ui.addQuad(gx + 6, gy - 6, 18, 18, 0xFF666666u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 14, gy + 8, 3, 12, 0xFFAAAAAAu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_CUTTER:
                ui.addQuad(gx + 4, gy - 2, 28, 8, 0xFF444B50u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 28, gy - 6, 8, 16, 0xFF222222u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_CABLE:
                ui.addQuad(gx + 2, gy - 6, 4, 20, 0xFFDDAA33u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 4, gy + 10, 28, 4, 0xFFDDBB55u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_DRILL:
                ui.addQuad(gx + 2, gy, 20, 8, 0xFFD8A020u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 22, gy - 2, 12, 12, 0xFF333838u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
            case TOOL_AIRSTRIKE:
                ui.addQuad(gx + 12, gy - 8, 6, 26, gc, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                ui.addQuad(gx + 2, gy - 4, 28, 12, 0x6055CCFFu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
                break;
        }
        const ToolDef& d = toolDef(i);
        if (d.limitedAmmo) {
            char ab[16];
            if (tools.reloadTimer > 0) snprintf(ab, sizeof(ab), "RELOAD");
            else snprintf(ab, sizeof(ab), "%d", tools.ammo[i]);
            ui.textRight(Rect(r.x, r.y + r.h - 16, r.w - 4, 12), 10,
                         tools.ammo[i] > 0 ? 0xFFB6B6B6u : 0xFFEE5555u, ab);
        }
    }
    // tool name above the bar
    const ToolDef& d = toolDef(tools.current);
    float nw = ui.measure(d.name, 13);
    ui.text(w * 0.5f - nw * 0.5f, y - 16, 13, 0xFFDDDDDDu, d.name);
}

void Game::drawWeaponStatus() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    // health
    float x = 28, y = h - 158;
    ui.panel(Rect(x, y, 218, 74), 0xB80E1116u, false);
    ui.addRect(Rect(x, y, 3, 74), player.health > 35 ? ui::COL_GOOD : ui::COL_DANGER);
    ui.text(x + 14, y + 10, 11, 0x99FFFFFFu, "VITALS");
    ui.progress(Rect(x + 14, y + 28, 190, 10), clampf(player.health / player.maxHealth, 0, 1),
                player.health > 60 ? ui::COL_GOOD : (player.health > 30 ? 0xFFD8A020u : ui::COL_DANGER));
    char hb[48];
    snprintf(hb, sizeof(hb), "%d / %d", (int)player.health, (int)player.maxHealth);
    ui.textRight(Rect(x, y + 40, 204, 14), 11, 0xFFDDDDDDu, hb);
    // fuel / charge readouts for the burner tools
    if (tools.current == TOOL_BLOWTORCH || tools.current == TOOL_EXTINGUISHER) {
        float v = tools.current == TOOL_BLOWTORCH ? tools.torchFuel : tools.extinguisherCharge;
        ui.text(x + 14, y + 56, 10, 0x99FFFFFFu, tools.current == TOOL_BLOWTORCH ? "TORCH FUEL" : "CHARGE");
        ui.progress(Rect(x + 84, y + 58, 120, 5), clampf(v, 0, 1), 0xFFD8A020u);
    } else if (tools.current == TOOL_CUTTER || tools.current == TOOL_DRILL) {
        ui.text(x + 14, y + 56, 10, 0x99FFFFFFu, "HEAT");
        ui.progress(Rect(x + 84, y + 58, 120, 5), clampf(tools.heat, 0, 1), tools.heat > 0.85f ? ui::COL_DANGER : 0xFFFFAA33u);
    } else {
        int active = (int)tools.charges.size();
        char cb[64];
        snprintf(cb, sizeof(cb), "CHARGES %d   CABLES %d", active, (int)tools.cables.size());
        ui.text(x + 14, y + 54, 10, 0x80FFFFFFu, cb);
    }
    // tool hint bottom-centre is drawn by the toolbar; here we show the ammo/reload state
    if (tools.reloadTimer > 0) {
        float t = 1.0f - tools.reloadTimer / std::max(0.001f, tools.reloadDuration);
        Rect r(w * 0.5f - 90, h - 128, 180, 9);
        ui.addRect(r, 0xA0101014u);
        ui.progress(r, clampf(t, 0, 1), ui::COL_ACCENT);
        ui.textCentered(Rect(r.x, r.y - 18, r.w, 14), 11, 0xFFDDDDDDu, "RELOADING");
    }
    // airstrike state
    if (tools.current == TOOL_AIRSTRIKE && tools.airstrike.state == 1) {
        Rect r(w * 0.5f - 130, 120, 260, 30);
        ui.panel(r, 0xC81A0E0Eu, true);
        ui.textCentered(r, 13, 0xFFFF8833u, "AIRSTRIKE INBOUND - GET CLEAR");
    }
}

void Game::drawVehicleHUD() {
    if (drivenVehicle < 0 || drivenVehicle >= (int)vehicles.vehicles.size()) return;
    Vehicle& v = vehicles.vehicles[drivenVehicle];
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    Rect r(w * 0.5f - 190, h - 122, 380, 92);
    ui.panel(r, 0xB80E1116u, false);
    ui.addRect(Rect(r.x, r.y, 3, r.h), ui::COL_ACCENT);
    const VehicleDef& d = vehicleDef(v.kind);
    ui.text(r.x + 14, r.y + 8, 12, ui::COL_ACCENT, d.name);
    // speedometer: arc of ticks
    float cx = r.x + 74, cy = r.y + 62, rad = 38;
    for (int i = 0; i <= 20; i++) {
        float a = PI + (float)i / 20.0f * PI;
        float t = (float)i / 20.0f;
        bool lit = t <= clampf(v.speed / d.topSpeed, 0, 1);
        float x0 = cx + std::cos(a) * rad, y0 = cy + std::sin(a) * rad;
        float x1 = cx + std::cos(a) * (rad - (lit ? 9 : 5)), y1 = cy + std::sin(a) * (rad - (lit ? 9 : 5));
        uint32_t c = lit ? (t > 0.8f ? ui::COL_DANGER : ui::COL_ACCENT) : 0x40FFFFFFu;
        // approximate the tick with a small quad
        float dx = x1 - x0, dy = y1 - y0;
        ui.addQuad(std::min(x0, x1) - 1, std::min(y0, y1) - 1, std::fabs(dx) + 2, std::fabs(dy) + 2, c,
                   ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    }
    char sb[32];
    snprintf(sb, sizeof(sb), "%.0f", v.speed * 3.6f);
    ui.text(cx - ui.measure(sb, 22) * 0.5f, cy - 26, 22, 0xFFFFFFFFu, sb);
    ui.text(cx - 14, cy - 2, 10, 0x90FFFFFFu, "KM/H");
    // health + gear
    ui.text(r.x + 140, r.y + 26, 11, 0x99FFFFFFu, "HULL");
    ui.progress(Rect(r.x + 140, r.y + 42, 210, 7), clampf(v.health / 100.0f, 0, 1),
                v.health > 60 ? ui::COL_GOOD : (v.health > 25 ? 0xFFD8A020u : ui::COL_DANGER));
    char gb[32];
    const char* gear = v.speed < -0.6f ? "R" : (v.speed < 0.6f ? "N" : (v.speed < 8 ? "1" : (v.speed < 16 ? "2" : "3")));
    snprintf(gb, sizeof(gb), "GEAR %s   %s", gear, v.lightsOn ? "LAMPS ON" : "LAMPS OFF");
    ui.text(r.x + 140, r.y + 56, 11, 0xBBFFFFFFu, gb);
    ui.text(r.x + 140, r.y + 72, 10, 0x70FFFFFFu, "E EXIT   L LAMPS   SPACE HANDBRAKE");
}

void Game::drawMinimap() {
    if (!showMinimap) return;
    int w = renderer->frameWidth();
    float size = 190;
    Rect r(w - size - 28, 28, size, size);
    ui.panel(r, 0xC00A0C10u, true);
    ui.text(r.x + 8, r.y + 6, 10, 0x90FFFFFFu, "TACTICAL");
    float inner = size - 20;
    float px = r.x + 10, py = r.y + 22;
    // map extents
    float mx = world.sx * VOX, mz = world.sz * VOX;
    float scale = inner / std::max(mx, mz);
    // destruction targets as blocks
    for (auto& t : mapData.targets) {
        vec3 c = t.box.center();
        float f = t.destroyedFraction(world);
        uint32_t col = f > 0.5f ? 0x80804040u : (f > 0.1f ? 0x80605030u : 0x80505860u);
        ui.addRect(Rect(px + c.x * scale - 2, py + c.z * scale - 2, 4, 4), col);
    }
    // vehicles
    for (auto& v : vehicles.vehicles) {
        ui.addRect(Rect(px + v.pos.x * scale - 1, py + v.pos.z * scale - 1, 3, 3), 0xFFCCC060u);
    }
    // pickups
    for (auto& p : pickups) {
        if (!p.active) continue;
        ui.addRect(Rect(px + p.pos.x * scale - 1, py + p.pos.z * scale - 1, 3, 3), 0xFF66CCFFu);
    }
    // mission objectives
    if (playMode == PLAY_CAMPAIGN) {
        for (auto& o : g_missions.get(g_missions.state.missionId).objectives) {
            if (o.kind != OBJ_REACH && o.kind != OBJ_VEHICLE_ZONE) continue;
            float a = (g_time * 3.0f);
            float s = 4 + std::sin(a) * 1.5f;
            ui.addRect(Rect(px + o.point.x * scale - s * 0.5f, py + o.point.z * scale - s * 0.5f, s, s),
                       o.complete ? ui::COL_GOOD : ui::COL_ACCENT);
        }
    }
    // player arrow
    float cx = px + player.pos.x * scale, cy = py + player.pos.z * scale;
    float yaw = player.yaw;
    ui.addQuad(cx - 3, cy - 3, 6, 6, 0xFFFFFFFFu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    float ax = cx + std::sin(yaw) * 9, ay = cy - std::cos(yaw) * 9;
    ui.addQuad(std::min(cx, ax) - 1, std::min(cy, ay) - 1, std::fabs(ax - cx) + 2, std::fabs(ay - cy) + 2,
               0xFFFFFFFFu, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
    ui.border(r, 0x40FFFFFFu, 1.0f);
}

void Game::drawToasts() {
    int w = renderer->frameWidth();
    float y = 150;
    for (auto& t : toasts) {
        float a = clampf(t.life / 0.6f, 0, 1) * clampf((t.maxLife - t.life) / 0.25f, 0, 1);
        uint32_t c = t.kind == 1 ? ui::COL_GOOD : (t.kind == 2 ? ui::COL_DANGER : 0xFFE8E8E8u);
        float size = 13;
        float tw = ui.measure(t.text, size);
        Rect r(w * 0.5f - tw * 0.5f - 20, y, tw + 40, 28);
        ui.panel(r, withAlpha(0xD014171Cu, (int)(0xD0 * a)), false);
        ui.addRect(Rect(r.x, r.y, 3, r.h), withAlpha(c, (int)(255 * a)));
        ui.textCentered(r, size, withAlpha(c, (int)(255 * a)), t.text);
        y += 34;
    }
}

void Game::drawInfoPanels() {
    int w = renderer->frameWidth(), h = renderer->frameHeight();
    if (showDebug) {
        Rect r(28, 28, 330, 182);
        ui.panel(r, 0xC0080A0Du, true);
        ui.text(r.x + 10, r.y + 8, 11, ui::COL_ACCENT, "DEBUG (F1)");
        char buf[256];
        Camera cam = currentCamera();
        snprintf(buf, sizeof(buf),
                 "renderer  %s\nfps       %d  (%.1f ms)\nframe     upd %.2f ms\nvoxels    %d x %d x %d\nchunks    %d meshed / %d dirty\nbodies    %d active\nparticles %d\nfires     %d\npos       %.1f %.1f %.1f\nmap       %s",
                 renderer->name(), stats.fps, stats.avgFrameMs, stats.updateMs, world.sx, world.sy, world.sz,
                 (int)world.meshes.size(), world.pendingDirty(), physics.stats.activeBodies, fx.alive(),
                 fx.fireCount(), cam.pos.x, cam.pos.y, cam.pos.z, getMap(currentMap).info.name);
        ui.text(r.x + 10, r.y + 26, 11, 0xCCFFFFFFu, buf);
        (void)w;
        (void)h;
    }
    if (showHelp) {
        Rect r(28, 230, 380, 360);
        ui.panel(r, 0xD0080A0Du, true);
        ui.addRect(Rect(r.x, r.y, r.w, 3), ui::COL_ACCENT);
        ui.text(r.x + 14, r.y + 12, 13, ui::COL_ACCENT, "CONTROLS");
        const char* lines[] = {
            "WASD          move", "SHIFT         sprint", "CTRL          crouch",
            "SPACE         jump", "MOUSE         look", "LMB           use tool",
            "RMB           secondary", "1-9, 0, -     select tool", "WHEEL         cycle tools",
            "R             reload / detonate", "E             interact / enter vehicle",
            "Q             drop object", "F             flashlight", "V             spawn vehicle (sandbox)",
            "T             time of day", "G             weather", "N             reset map",
            "F5 / F9       quick save / load", "F3            minimap", "F1            debug",
            "ESC           pause", "TAB           toggle this help",
        };
        float y = r.y + 34;
        for (auto& l : lines) { ui.text(r.x + 14, y, 11, 0xCCE0E0E0u, l); y += 14.5f; }
    }
    if (statusTimer > 0 && !statusLine.empty()) {
        float tw = ui.measure(statusLine, 14);
        Rect r(w * 0.5f - tw * 0.5f - 18, h - 190, tw + 36, 30);
        ui.panel(r, 0xC814171Cu, false);
        ui.textCentered(r, 14, 0xFFEEEEEEu, statusLine);
    }
}

void Game::renderHUD() {
    drawCrosshair();
    drawToolBar();
    drawWeaponStatus();
    drawObjectivesPanel();
    drawToasts();
    drawInfoPanels();
    if (drivenVehicle >= 0) drawVehicleHUD();
    if (showMinimap) drawMinimap();
    // damage vignette
    if (damageFlash > 0 || (player.health < 30 && !player.dead)) {
        float a = std::max(damageFlash, player.health < 30 ? (30 - player.health) / 60.0f : 0.0f);
        int w = renderer->frameWidth(), h = renderer->frameHeight();
        float t = 40 * a + 10;
        ui.addQuadGrad(0, 0, w, t, withAlpha(0xFFAA2222u, (int)(a * 150)), 0x00AA2222u);
        ui.addQuadGrad(0, h - t, w, t, 0x00AA2222u, withAlpha(0xFFAA2222u, (int)(a * 150)));
        ui.addQuadGradH(0, 0, t, h, withAlpha(0xFFAA2222u, (int)(a * 120)), 0x00AA2222u);
        ui.addQuadGradH(w - t, 0, t, h, 0x00AA2222u, withAlpha(0xFFAA2222u, (int)(a * 120)));
    }
    if (player.dead) {
        int w = renderer->frameWidth(), h = renderer->frameHeight();
        ui.addQuad(0, 0, w, h, 0x60AA0000u, ui.uSolid(), ui.vSolid(), ui.uSolid(), ui.vSolid());
        ui.textCentered(Rect(0, h * 0.5f - 30, w, 60), 40, 0xFFFFFFFFu, "YOU ARE DOWN");
    }
    // objective flash
    if (objectiveFlash > 0) {
        int w = renderer->frameWidth();
        float a = clampf(objectiveFlash / 1.2f, 0, 1);
        Rect r(w * 0.5f - 130, 76, 260, 34);
        ui.panel(r, withAlpha(0xC8102818u, (int)(200 * a)), false);
        ui.textCentered(r, 14, withAlpha(ui::COL_GOOD, (int)(255 * a)), "OBJECTIVE COMPLETE");
    }
    // security status
    if (security.alarm || security.anyAlert()) {
        int w = renderer->frameWidth();
        float a = 0.55f + 0.45f * std::sin(g_time * 8.0f);
        Rect r(w * 0.5f - 150, 24, 300, 30);
        ui.panel(r, withAlpha(0xC81C0A08u, 210), true);
        ui.addRect(Rect(r.x, r.y, r.w, 2), withAlpha(ui::COL_DANGER, (int)(255 * a)));
        char ab[96];
        if (security.alarm && playMode == PLAY_CAMPAIGN)
            snprintf(ab, sizeof(ab), "ALARM  %d:%02d TO LOCKDOWN", (int)(security.alarmDuration - security.alarmTime) / 60,
                     (int)(security.alarmDuration - security.alarmTime) % 60);
        else
            snprintf(ab, sizeof(ab), "ALARM RAISED");
        ui.textCentered(r, 13, withAlpha(0xFFFF5544u, (int)(180 + 75 * a)), ab);
    }
    if (settings.showFps) {
        char b[32];
        snprintf(b, sizeof(b), "%d FPS", stats.fps);
        ui.textRight(Rect(0, 8, renderer->frameWidth() - 30, 16), 11,
                     stats.fps > 50 ? 0x80FFFFFFu : (stats.fps > 28 ? 0xB0FFD050u : 0xC0FF5040u), b);
    }
}
