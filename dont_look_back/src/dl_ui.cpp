// ============================================================================
//  dl_ui.cpp : world rendering, HUD, menus, journal, settings, endings
// ============================================================================
#include "dl_game.h"

namespace dl {
using namespace nf;

// ------------------------------------------------------------------ palette
static const Color C_BONE   = rgba(226, 216, 196);
static const Color C_DIM    = rgba(140, 140, 148);
static const Color C_DARK   = rgba(12, 12, 15);
static const Color C_PANEL  = rgba(9, 9, 12, 224);
static const Color C_ACCENT = rgba(196, 160, 108);
static const Color C_BAD    = rgba(206, 116, 104);
static const Color C_GOOD   = rgba(140, 190, 150);

static void panelFrame(Painter& p, float x, float y, float w, float h) {
    p.rect(x, y, w, h, C_PANEL);
    p.rectOutline(x, y, w, h, 1.0f, rgba(80, 78, 74, 120));
    p.rect(x + 1, y + 1, w - 2, 1, rgba(150, 140, 120, 40));
}

// ============================================================================
//  WORLD RENDER
// ============================================================================
void Game::renderWorld() {
    if (!r_) return;
    r_->beginFrame();

    // camera
    V3 fwd(std::cos(pl_.yaw) * std::cos(pl_.pitch), std::sin(pl_.yaw) * std::cos(pl_.pitch),
           std::sin(pl_.pitch));
    float shakeX = 0, shakeY = 0;
    if (pl_.shakeT > 0.0f) {
        float k = pl_.shakeT * pl_.shakeAmp;
        shakeX = (hashf((uint32_t)(uiTime_ * 971.0f)) - 0.5f) * k;
        shakeY = (hashf((uint32_t)(uiTime_ * 1733.0f)) - 0.5f) * k;
    }
    V3 eye = pl_.pos + V3(0, 0, pl_.bob) + V3(shakeX, shakeY, 0);
    float fov = 72.0f;
    r_->setCamera(eye, fwd, V3(0, 0, 1), fov);

    // lighting: very dark ambient, flashlight dominant
    float lampScale = 0.75f;
    r_->setAmbient(V3(0.060f, 0.066f, 0.082f) * settings.brightness, 1.0f);
    FogSettings fog;
    fog.color = V3(0.016f, 0.019f, 0.024f);
    fog.density = 0.016f;
    fog.scatter = 0.28f;
    if (currentRoom_.find("basement") != std::string::npos || currentRoom_.find("boiler") != std::string::npos ||
        currentRoom_.find("laundry") != std::string::npos || currentRoom_.find("storage") != std::string::npos) {
        fog.color = V3(0.010f, 0.013f, 0.014f);
        fog.density = 0.026f;
    }
    if (currentRoom_.find("hidden") != std::string::npos) {
        fog.color = V3(0.020f, 0.012f, 0.010f);
        fog.density = 0.03f;
    }
    r_->setFog(fog);

    // flashlight
    Flashlight fl;
    V3 right = normalize(cross(fwd, V3(0, 0, 1)));
    fl.pos = eye + right * 0.15f + V3(0, 0, -0.10f);
    fl.dir = fwd;
    fl.on = pl_.flashlightOn;
    float fade = 1.0f;
    if (pl_.battery < 0.25f) {
        float t = uiTime_ * 9.0f;
        fade = 0.55f + 0.45f * (std::sin(t) * std::sin(t * 0.37f) > 0.1f ? 1.0f : 0.25f);
    }
    if (pl_.fear > 0.6f) fade *= 0.85f + 0.15f * std::sin(uiTime_ * 17.0f);
    fl.intensity = 2.35f * fade * settings.brightness;
    fl.color = V3(1.0f, 0.95f, 0.85f);
    fl.inner = 0.945f;
    fl.outer = 0.70f;
    fl.range = 22.0f;
    r_->setFlashlight(fl);

    // point lights: the few nearest active fixtures only (a software raster keeps its
    // per-pixel light loop short -- anything further is already invisible in this darkness)
    int lightCand[5] = {-1, -1, -1, -1, -1};
    float lightDist[5] = {1e9f, 1e9f, 1e9f, 1e9f, 1e9f};
    for (size_t fi = 0; fi < world_.fixtures.size(); ++fi) {
        auto& f = world_.fixtures[fi];
        if (!f.on || f.dead || f.baseIntensity < 0.05f) continue;
        float d = length(f.pos - eye) / maxf(0.35f, f.baseIntensity);
        for (int k = 0; k < 5; ++k) {
            if (d < lightDist[k]) {
                for (int j = 4; j > k; --j) { lightDist[j] = lightDist[j - 1]; lightCand[j] = lightCand[j - 1]; }
                lightDist[k] = d; lightCand[k] = (int)fi;
                break;
            }
        }
    }
    int lightsAdded = 0;
    for (int k = 0; k < 5; ++k) {
        if (lightCand[k] < 0) continue;
        auto& f = world_.fixtures[lightCand[k]];
        float d = length(f.pos - eye);
        if (d > 22.0f) continue;
        float flick = 1.0f;
        if (f.flicker > 0.01f) {
            flick = 0.45f + 0.55f * std::fabs(std::sin(uiTime_ * 11.0f + f.phase) *
                                              std::sin(uiTime_ * 3.1f + f.phase * 2.0f));
        }
        if (f.baseIntensity < 0.05f) continue;
        PointLight pl;
        pl.pos = f.pos;
        pl.color = f.color;
        pl.intensity = f.baseIntensity * lampScale * (1.0f - pl_.fear * 0.1f);
        pl.range = f.range;
        pl.flicker = flick;
        r_->addLight(pl);
        if (++lightsAdded >= 4) break;
    }
    // entity glow (so they read against the dark)
    for (auto& e : ents_) {
        if (!e.active) continue;
        PointLight pl;
        pl.pos = e.pos + V3(0, 0, 1.25f);
        pl.color = V3(0.52f, 0.56f, 0.68f);
        pl.intensity = 0.55f;
        pl.range = 3.6f;
        r_->addLight(pl);
    }
    // muzzle-like glow from the elevator and emergency lights
    if (emergenceActive_) {
        PointLight pl;
        pl.pos = eye + V3(0, 0, 2.2f);
        pl.color = V3(0.2f, 0.8f, 0.35f);
        pl.intensity = 0.35f;
        pl.range = 8.0f;
        r_->addLight(pl);
    }

    // ---- geometry
    for (auto& g : world_.groups) {
        if (!g.visible) continue;
        if (!g.mesh.boundsValid) continue;
        // frustum-ish cull by distance
        float d = length(g.mesh.bounds.center() - eye) - maxComp(g.mesh.bounds.size()) * 0.5f;
        static float cut = getenv("NF_CUT") ? (float)atof(getenv("NF_CUT")) : 34.0f;
        if (d > cut) continue;
        r_->draw(g.mesh, g.xf, world_.mats.get(g.mat));
    }
    for (auto& d : world_.doors) {
        float dist = length(d.hinge - eye);
        if (dist > 30.0f) continue;
        r_->draw(world_.doorLeaf(d.width, d.height), world_.doorTransform(d),
                 world_.mats.get(MAT_DOOR_PANEL));
    }
    // entities: one draw per material so coat, skin and face all keep their textures
    static std::vector<std::pair<int, Mesh>> eParts;
    for (auto& e : ents_) {
        if (!e.active) continue;
        if (length(e.pos - eye) > 30.0f) continue;
        buildEntityMesh(e, eParts);
        M4 xf = M4::translate(e.pos) * M4::rotZ(e.yaw);
        for (auto& p : eParts) r_->draw(p.second, xf, world_.mats.get(p.first));
    }

    // ---- post
    PostSettings& post = r_->post();
    post.exposure = 1.0f * settings.brightness;
    post.bloom = 0.30f;
    post.grain = settings.grain * (1.0f + pl_.fear * 1.6f);
    post.vignette = 0.34f + pl_.fear * 0.20f;
    post.saturation = 0.80f - pl_.fear * 0.28f;
    post.contrast = 1.06f;
    post.aberration = 0.5f + pl_.fear * 2.4f;
    post.warp = pl_.fear * 0.30f;
    post.warpPhase = uiTime_;
    post.pulse = pl_.fear * 0.55f;
    post.glitch = (pl_.damageFlash > 0.2f) ? pl_.damageFlash : 0.0f;
    post.fade = 0.0f;
    if (state_ == GameState::Ending) post.fade = saturate(endingTimer_ * 0.5f);
    if (pl_.damageFlash > 0.0f) post.fadeColor = V3(0.25f, 0.02f, 0.02f);
    else post.fadeColor = V3(0, 0, 0);
    r_->render();
}

// ------------------------------------------------------------------ helpers
static void drawKeyHint(Painter& p, const Font& f, float x, float y, const char* key, const char* label) {
    p.rect(x, y, 30, 24, rgba(20, 20, 24, 220));
    p.rectOutline(x, y, 30, 24, 1, rgba(120, 112, 96, 180));
    p.textCentered(f, key, x + 15, y + 4, C_ACCENT, 0.72f);
    p.text(f, label, x + 38, y + 4, C_DIM, 0.72f);
}

// ============================================================================
//  HUD
// ============================================================================
void Game::renderHud(Painter& p) {
    int W = p.width(), H = p.height();
    float s = uiScale_;

    // crosshair: a small hollow dot
    {
        float cx = W * 0.5f, cy = H * 0.5f;
        p.circleOutline(cx, cy, 3.0f * s, 1.2f * s, rgba(210, 200, 180, 70));
        if (hoverInteract_ != -1) p.circle(cx, cy, 1.6f * s, rgba(220, 190, 140, 200));
    }

    // interaction prompt
    if (!hoverText_.empty()) {
        float cx = W * 0.5f;
        float y = H * 0.5f + 30 * s;
        int tw = fontSmall_.measure(hoverText_, 0.78f * s);
        p.rect(cx - tw * 0.5f - 18 * s, y - 6 * s, tw + 36 * s, 30 * s, rgba(8, 8, 10, 170));
        p.textCentered(fontSmall_, hoverText_, cx, y, C_BONE, 0.78f * s);
        p.textCentered(fontBody_, "[E]", cx, y + 26 * s, C_ACCENT, 0.55f * s);
    }

    // toasts (bottom-left)
    float ty = H - 150 * s;
    for (int i = (int)toasts_.size() - 1; i >= 0; --i) {
        const Toast& t = toasts_[i];
        float a = saturate(minf(t.t * 3.0f, (t.life - t.t) * 1.4f));
        Color c = t.col;
        c = (c & 0x00FFFFFF) | ((Color)(int)(colA(c) * a) << 24);
        int tw = fontSmall_.measure(t.text, (t.sub ? 0.62f : 0.70f) * s);
        p.rect(26 * s, ty - 4 * s, tw + 24 * s, 28 * s, rgba(4, 4, 6, (int)(215 * a)));
        p.rect(26 * s, ty - 4 * s, 2.0f * s, 28 * s, (C_ACCENT & 0x00FFFFFF) | ((Color)(int)(180 * a) << 24));
        p.text(fontSmall_, t.text, 36 * s, ty, c, (t.sub ? 0.62f : 0.70f) * s);
        ty -= 32 * s;
    }

    // ---- bottom-left status: battery / progress
    float bx = 30 * s, by = H - 46 * s;
    p.text(fontMono_, "LUCIDITY", bx, by - 26 * s, rgba(120, 118, 112, 200), 0.62f * s);
    int slots = 12;
    for (int i = 0; i < slots; ++i) {
        float x = bx + i * 17 * s;
        bool on = i < reported_;
        p.rect(x, by, 12 * s, 14 * s, on ? rgba(196, 160, 108, 220) : rgba(60, 58, 54, 160));
    }
    for (int i = 0; i < mistakes_; ++i) {
        p.rect(bx + i * 17 * s, by + 20 * s, 12 * s, 5 * s, C_BAD);
    }
    if (pl_.hasFlashlight) {
        float bw = 120 * s;
        p.rect(H * 0 + bx, by + 32 * s, bw, 4 * s, rgba(50, 48, 46, 180));
        p.rect(bx, by + 32 * s, bw * pl_.battery, 4 * s,
               pl_.battery < 0.25f ? C_BAD : rgba(190, 180, 150, 200));
        p.text(fontMono_, pl_.flashlightOn ? "TORCH ON" : "TORCH OFF", bx + bw + 10 * s, by + 26 * s,
               pl_.flashlightOn ? rgba(190, 180, 150, 190) : rgba(96, 94, 90, 190), 0.55f * s);
    }

    // ---- top-right: current room + floor
    RoomInfo* room = world_.roomAt(pl_.pos);
    if (room) {
        std::string name = room->name;
        int tw = fontMono_.measure(name, 0.66f * s);
        p.text(fontMono_, name, W - tw - 30 * s, 24 * s, rgba(150, 146, 138, 190), 0.66f * s);
        std::string fl = strfmt("FLOOR %d", room->floor == 1 ? 1 : 3);
        int tw2 = fontMono_.measure(fl, 0.55f * s);
        p.text(fontMono_, fl, W - tw2 - 30 * s, 48 * s, rgba(110, 108, 102, 170), 0.55f * s);
    }

    // ---- anomaly availability hint
    if (activeAnomalyCount() > 0) {
        float pulse = 0.55f + 0.45f * std::sin(uiTime_ * 2.6f);
        p.circle(W - 42 * s, H - 42 * s, 4.0f * s, rgba(200, 160, 100, (int)(180 * pulse)));
        p.text(fontMono_, "SOMETHING IS WRONG", W - 232 * s, H - 78 * s,
               rgba(190, 150, 100, (int)(170 * pulse)), 0.55f * s);
    }

    // ---- key hints (bottom right, out of the way of toasts)
    {
        const char* kl[3] = {"R", "TAB", "ESC"};
        const char* vl[3] = {"report", "journal", "pause"};
        float x = W - 30 * s;
        for (int i = 2; i >= 0; --i) {
            int tw = fontMono_.measure(vl[i], 0.62f * s);
            p.text(fontMono_, vl[i], x - tw, H - 34 * s, rgba(130, 126, 118, 190), 0.62f * s);
            x -= tw + 8 * s;
            p.rect(x - 28 * s, H - 38 * s, 26 * s, 22 * s, rgba(18, 18, 22, 210));
            p.textCentered(fontMono_, kl[i], x - 15 * s, H - 36 * s, C_ACCENT, 0.58f * s);
            x -= 38 * s;
        }
    }

    // ---- damage vignette
    if (pl_.damageFlash > 0.01f) {
        float a = pl_.damageFlash;
        p.rect(0, 0, W, H, rgba(70, 6, 6, (int)(90 * a)));
    }
    if (pl_.fear > 0.5f) {
        float a = (pl_.fear - 0.5f) * 1.4f;
        p.rect(0, 0, W, 2 + (int)(6 * s), rgba(0, 0, 0, (int)(150 * a)));
        p.rect(0, H - 2 - (int)(6 * s), W, 2 + (int)(6 * s), rgba(0, 0, 0, (int)(150 * a)));
    }
}

// ============================================================================
//  TITLE
// ============================================================================
void Game::renderMenu(Painter& p) {
    int W = p.width(), H = p.height();
    float s = uiScale_;
    p.clear(rgba(6, 6, 8));

    // an atmospheric backdrop: the last rendered world frame is reused, dimmed
    if (!r_->pixelsVec().empty()) {
        Image bg(W, H);
        memcpy(bg.px.data(), r_->pixels(), (size_t)W * H * 4);
        p.blit(bg, 0, 0, W, H, rgba(70, 70, 78, 255));
    }
    p.rect(0, 0, W, H, rgba(6, 6, 9, 180));
    p.gradientV(0, 0, W, H * 0.5f, rgba(8, 8, 10, 235), rgba(8, 8, 10, 60));
    p.gradientV(0, H * 0.55f, W, H * 0.45f, rgba(8, 8, 10, 30), rgba(4, 4, 6, 245));

    float cy = H * 0.26f;
    p.textCentered(fontTitle_, "DON'T LOOK BACK", W * 0.5f, cy, C_BONE, 0.92f * s);
    p.textCentered(fontMono_, "an observation of the fourth floor", W * 0.5f, cy + 74 * s,
                   rgba(150, 142, 126, 220), 0.62f * s);
    p.rect(W * 0.5f - 120 * s, cy + 104 * s, 240 * s, 1, rgba(150, 140, 120, 90));

    const char* items[6] = {"BEGIN", "CONTINUE", "SETTINGS", "CONTROLS", "QUIT", ""};
    int count = 5;
    float iy = H * 0.47f;
    for (int i = 0; i < count; ++i) {
        bool sel = (i == menuIndex_);
        Color c = sel ? C_BONE : rgba(120, 118, 112, 200);
        float x = W * 0.5f;
        if (sel) {
            p.rect(x - 120 * s, iy + i * 40 * s - 4 * s, 240 * s, 32 * s, rgba(24, 22, 20, 200));
            p.rect(x - 120 * s, iy + i * 40 * s - 4 * s, 2 * s, 32 * s, C_ACCENT);
        }
        p.textCentered(fontSmall_, items[i], x + 8 * s, iy + i * 40 * s, c, 0.80f * s);
    }
    p.textCentered(fontMono_, "arrows / W S to select      ENTER to confirm", W * 0.5f, H - 74 * s,
                   rgba(90, 88, 84, 200), 0.56f * s);
    p.textCentered(fontMono_, "All geometry, textures, fonts and audio are generated procedurally.",
                   W * 0.5f, H - 46 * s, rgba(66, 64, 60, 180), 0.50f * s);
}

// ============================================================================
//  JOURNAL
// ============================================================================
void Game::renderJournal(Painter& p) {
    int W = p.width(), H = p.height();
    float s = uiScale_;
    if (!r_->pixelsVec().empty()) {
        Image bg(W, H);
        memcpy(bg.px.data(), r_->pixels(), (size_t)W * H * 4);
        p.blit(bg, 0, 0, W, H, rgba(40, 40, 46, 255));
    }
    p.rect(0, 0, W, H, rgba(5, 5, 7, 215));
    float m = 70 * s;
    panelFrame(p, m, m, W - m * 2, H - m * 2);
    p.text(fontTitle_, "JOURNAL", m + 34 * s, m + 18 * s, C_BONE, 0.62f * s);
    p.text(fontMono_, strfmt("REPORTED %d / 12     FALSE REPORTS %d / 5", reported_, mistakes_),
           m + 34 * s, m + 80 * s, C_ACCENT, 0.60f * s);
    p.rect(m + 34 * s, m + 108 * s, W - m * 2 - 68 * s, 1, rgba(120, 112, 96, 110));

    float x = m + 34 * s;
    float y = m + 124 * s;
    p.text(fontMono_, "FINDINGS", x, y, rgba(150, 144, 130, 200), 0.58f * s);
    y += 26 * s;
    int shown = 0;
    for (auto& e : journal_) {
        if (shown >= 11) break;
        Color tc = e.correct ? C_BONE : C_BAD;
        p.text(fontSmall_, strfmt("%s%s", e.correct ? "" : "! ", e.title.c_str()), x, y, tc, 0.60f * s);
        y += 22 * s;
        float hgt = 0;
        p.textWrap(fontBody_, e.body, x + 14 * s, y, W * 0.52f, rgba(150, 148, 140, 210), 0.52f * s, 3);
        hgt = 3 * fontBody_.lineHeight * 0.52f * s;
        y += minf(hgt, 60 * s) + 6 * s;
        ++shown;
    }
    if (journal_.empty()) {
        p.textWrap(fontBody_,
                   "Nothing recorded yet.\n\nWalk the building. When something does not match, "
                   "press R to report it. A true report steadies the building. A false one does not.",
                   x, y, W * 0.5f, rgba(150, 148, 140, 200), 0.56f * s, 6);
    }

    // right column: objectives
    float rx = W * 0.63f;
    float ry = m + 124 * s;
    p.text(fontMono_, "OBJECTIVE", rx, ry, rgba(150, 144, 130, 200), 0.58f * s);
    ry += 28 * s;
    std::string obj;
    if (reported_ < 12) obj = strfmt("Report twelve anomalies. %d remain.", 12 - reported_);
    else if (!pl_.hasMaintenanceKey) obj = "The maintenance key is in the maintenance room, off the stair lobby.";
    else obj = "The fire door on the north landing will open now.";
    p.textWrap(fontBody_, obj, rx, ry, W * 0.28f, rgba(180, 176, 166, 220), 0.56f * s, 4);
    ry += 90 * s;
    p.text(fontMono_, "INVENTORY", rx, ry, rgba(150, 144, 130, 200), 0.58f * s);
    ry += 26 * s;
    p.text(fontBody_, pl_.hasFlashlight ? "* flashlight" : "-", rx, ry, C_DIM, 0.52f * s); ry += 22 * s;
    p.text(fontBody_, pl_.hasFuse ? "* ceramic fuse" : "-", rx, ry, C_DIM, 0.52f * s); ry += 22 * s;
    p.text(fontBody_, pl_.hasMaintenanceKey ? "* maintenance key" : "-", rx, ry, C_DIM, 0.52f * s); ry += 22 * s;
    ry += 30 * s;
    p.text(fontMono_, "DOCUMENTS", rx, ry, rgba(150, 144, 130, 200), 0.58f * s);
    ry += 26 * s;
    for (auto& d : world_.documents) {
        if (!d.found) continue;
        p.text(fontBody_, strfmt("- %s", d.title.c_str()), rx, ry, rgba(170, 166, 158, 210), 0.48f * s);
        ry += 20 * s;
    }
    p.textCentered(fontMono_, "TAB  /  ESC  to close", W * 0.5f, H - m - 34 * s, rgba(110, 106, 98, 200), 0.56f * s);
}

// ============================================================================
//  SETTINGS
// ============================================================================
void Game::renderSettings(Painter& p) {
    int W = p.width(), H = p.height();
    float s = uiScale_;
    p.clear(rgba(5, 5, 7));
    float m = 70 * s;
    panelFrame(p, m, m, W - m * 2, H - m * 2);
    p.text(fontTitle_, "SETTINGS", m + 34 * s, m + 18 * s, C_BONE, 0.58f * s);
    struct Row { const char* name; std::string value; };
    std::vector<Row> rows;
    rows.push_back({"Mouse sensitivity", strfmt("%.2f", settings.mouseSensitivity)});
    rows.push_back({"Master volume", strfmt("%d%%", (int)(settings.masterVolume * 100))});
    rows.push_back({"Ambience volume", strfmt("%d%%", (int)(settings.ambientVolume * 100))});
    rows.push_back({"Effects volume", strfmt("%d%%", (int)(settings.sfxVolume * 100))});
    rows.push_back({"Brightness", strfmt("%d%%", (int)(settings.brightness * 100))});
    rows.push_back({"Render scale", strfmt("%d%%", (int)(settings.resolutionScale * 100))});
    rows.push_back({"Film grain", strfmt("%d%%", (int)(settings.grain / 0.09f * 100))});
    rows.push_back({"Invert mouse Y", settings.invertY ? "ON" : "OFF"});
    rows.push_back({"Fullscreen", settings.fullscreen ? "ON" : "OFF"});
    float y = m + 100 * s;
    for (size_t i = 0; i < rows.size(); ++i) {
        bool sel = (int)i == settingsIndex_;
        if (sel) {
            p.rect(m + 26 * s, y - 5 * s, W - m * 2 - 52 * s, 32 * s, rgba(26, 24, 22, 200));
            p.rect(m + 26 * s, y - 5 * s, 2 * s, 32 * s, C_ACCENT);
        }
        p.text(fontBody_, rows[i].name, m + 44 * s, y, sel ? C_BONE : C_DIM, 0.62f * s);
        p.text(fontMono_, rows[i].value, W - m - 80 * s, y + 2 * s, sel ? C_ACCENT : rgba(110, 108, 102, 200), 0.60f * s);
        y += 40 * s;
    }
    float hy = m + 100 * s + rows.size() * 40 * s + 24 * s;
    p.text(fontMono_, "CONTROLS", m + 44 * s, hy, rgba(150, 144, 130, 200), 0.58f * s);
    hy += 26 * s;
    const char* ctr[6] = {"W A S D  -  walk          SHIFT  -  run          F  -  flashlight",
                          "E  -  use / open           R  -  report anomaly       TAB  -  journal",
                          "MOUSE  -  look             ESC  -  pause",
                          "LEFT / RIGHT  -  adjust setting        ENTER / ESC  -  back",
                          "", ""};
    for (int i = 0; i < 4; ++i) {
        p.text(fontBody_, ctr[i], m + 44 * s, hy, rgba(140, 138, 130, 210), 0.50f * s);
        hy += 22 * s;
    }
    p.textCentered(fontMono_, "ESC  to return", W * 0.5f, H - m - 34 * s, rgba(110, 106, 98, 200), 0.56f * s);
}

// ============================================================================
//  DOCUMENT
// ============================================================================
void Game::renderDocument(Painter& p) {
    int W = p.width(), H = p.height();
    float s = uiScale_;
    if (!r_->pixelsVec().empty()) {
        Image bg(W, H);
        memcpy(bg.px.data(), r_->pixels(), (size_t)W * H * 4);
        p.blit(bg, 0, 0, W, H, rgba(30, 30, 34, 255));
    }
    p.rect(0, 0, W, H, rgba(4, 4, 6, 210));
    if (openDocument_ < 0 || openDocument_ >= (int)world_.documents.size()) return;
    auto& d = world_.documents[openDocument_];
    float pw = minf(W * 0.66f, 780 * s), ph = minf(H * 0.82f, 720 * s);
    float px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
    p.rect(px + 6 * s, py + 8 * s, pw, ph, rgba(0, 0, 0, 120));
    p.rect(px, py, pw, ph, rgba(222, 214, 192));
    p.gradientV(px, py, pw, ph * 0.25f, rgba(210, 200, 176, 255), rgba(222, 214, 192, 0));
    p.rectOutline(px, py, pw, ph, 2, rgba(120, 110, 90, 160));
    p.text(fontSmall_, d.title, px + 40 * s, py + 34 * s, rgba(48, 42, 34), 0.82f * s);
    p.rect(px + 40 * s, py + 76 * s, pw - 80 * s, 1, rgba(90, 82, 66, 120));
    p.textWrap(fontBody_, d.body, px + 40 * s, py + 96 * s, pw - 80 * s, rgba(44, 40, 34), 0.62f * s, 20);
    p.textCentered(fontMono_, "[E] or click to put the paper down", px + pw * 0.5f, py + ph - 34 * s,
                   rgba(90, 82, 66, 200), 0.52f * s);
}

// ============================================================================
//  ENDING
// ============================================================================
void Game::renderEnding(Painter& p) {
    int W = p.width(), H = p.height();
    float s = uiScale_;
    p.clear(rgba(3, 3, 4));
    float t = endingTimer_;
    const char* title = "";
    const char* body = "";
    Color tint = C_BONE;
    switch (ending_) {
        case EndingId::Escape:
            title = "ESCAPE";
            body = "The fire door gives, and the stairwell beyond it smells of rain.\n"
                   "You do not look back. You count the doors as you go: twelve, and not one more.\n\n"
                   "Behind you, on the third floor, a light comes on in a window that has no room "
                   "behind it. Then it goes out.\n\n"
                   "The building keeps its shape. For now.";
            tint = rgba(198, 210, 220);
            break;
        case EndingId::Descent:
            title = "DESCENT";
            body = "The brick is warm, and behind it the room is smaller than a room.\n"
                   "She has been waiting in the millimetres the architects did not use.\n\n"
                   "You do not look back, because there is nothing behind you any more.\n"
                   "There is only the inside of the wall, and it is very patient, and it is home.\n\n"
                   "The building has one more resident. The register still says: occupied.";
            tint = rgba(210, 150, 130);
            break;
        case EndingId::Assimilation:
            title = "ASSIMILATION";
            body = "You told it twelve things that were not true, and it believed you were one of them.\n"
                   "Now you walk the corridor the way it walks: only when no one is looking.\n\n"
                   "The next tenant will wake in apartment 47 and find your handwriting on the wall.\n"
                   "COUNT THE DOORS, it will say. DO NOT LOOK BACK.\n\n"
                   "They will not listen. No one ever does.";
            tint = rgba(170, 140, 150);
            break;
        default: break;
    }
    float a = saturate(t * 0.6f);
    Color tc = (tint & 0x00FFFFFF) | ((Color)(int)(255 * a) << 24);
    p.textCentered(fontTitle_, title, W * 0.5f, H * 0.22f, tc, 1.05f * s);
    p.rect(W * 0.5f - 100 * s, H * 0.22f + 96 * s, 200 * s, 1,
           rgba(150, 140, 120, (int)(120 * a)));
    Color bc = rgba(190, 186, 176, (int)(230 * a));
    p.textWrap(fontBody_, body, W * 0.5f - 330 * s, H * 0.38f, 660 * s, bc, 0.66f * s, 16);
    if (t > 3.0f) {
        float pulse = 0.55f + 0.45f * std::sin(uiTime_ * 2.4f);
        p.textCentered(fontMono_, "ENTER  to return to the title", W * 0.5f, H - 90 * s,
                       rgba(160, 152, 138, (int)(200 * pulse)), 0.58f * s);
    }
    p.textCentered(fontMono_, strfmt("TIME  %s      ANOMALIES REPORTED  %d      FALSE REPORTS  %d",
                                     strfmt("%d:%02d", (int)(playTime_ / 60), (int)playTime_ % 60).c_str(),
                                     reported_, mistakes_).c_str(),
                   W * 0.5f, H - 48 * s, rgba(110, 106, 98, (int)(200 * a)), 0.50f * s);
}

// ============================================================================
//  master render
// ============================================================================
void Game::render() {
    if (!r_ || !p_) return;
    int W = 0, H = 0;
    p_->getSize(W, H);
    if (W < 8 || H < 8) return;
    painter_.begin(r_->pixelsVec().data(), W, H);
    if ((int)r_->pixelsVec().size() != W * H) r_->resize(W, H);

    if (state_ == GameState::Boot) {
        float s2 = uiScale_;
        float a = saturate(bootTimer_ * 2.2f);
        painter_.clear(rgba(4, 4, 6));
        painter_.textCentered(fontTitle_, "DON'T LOOK BACK", W * 0.5f, H * 0.38f,
                              rgba(226, 216, 196, (int)(235 * a)), 0.9f * s2);
        painter_.textCentered(fontMono_, "NIGHTFALL ENGINE", W * 0.5f, H * 0.38f + 76 * s2,
                              rgba(120, 116, 108, (int)(200 * a)), 0.52f * s2);
        float bw = 280 * s2, bx = (W - bw) * 0.5f, by = H * 0.62f;
        painter_.rect(bx, by, bw, 2 * s2, rgba(40, 38, 36, 220));
        float prog = saturate(bootTimer_ / 1.5f);
        painter_.rect(bx, by, bw * prog, 2 * s2, rgba(190, 160, 110, 230));
        painter_.textCentered(fontMono_, "LOADING THE BUILDING", W * 0.5f, by + 18 * s2,
                              rgba(110, 106, 98, (int)(210 * a)), 0.5f * s2);
        p_->present(r_->pixels(), W, H);
        return;
    }
    if (state_ == GameState::Title) {
        // render a slow drift shot behind the title
        static float t = 0.0f;
        t += 0.016f;
        if (pl_.pos.z < 0.5f) { pl_.pos = world_.playerSpawn; }
        pl_.yaw = 0.35f + std::sin(t * 0.10f) * 0.25f;
        pl_.pitch = -0.04f;
        pl_.pos = V3(-10.6f + std::sin(t * 0.07f) * 0.35f, 1.2f, 1.62f);
        pl_.flashlightOn = true;
        pl_.fear = 0.25f;
        currentRoom_ = "apt47_hall";
        renderWorld();
        painter_.begin(r_->pixelsVec().data(), W, H);
        renderMenu(painter_);
    } else if (state_ == GameState::Settings) {
        renderSettings(painter_);
    } else {
        renderWorld();
        if (getenv("NF_DUMPSHOT")) {
            static int n = 0;
            if (n++ == 3) {
                Image dbg(W, H);
                memcpy(dbg.px.data(), r_->pixels(), (size_t)W * H * 4);
                pngWrite("shots/dbg_world.png", dbg);
                uint64_t sum = 0;
                for (int i = 0; i < W * H; ++i) sum += colR(r_->pixels()[i]);
                logMsg("[dbg] world mean R = %d", (int)(sum / (W * H)));
            }
        }
        painter_.begin(r_->pixelsVec().data(), W, H);
        switch (state_) {
            case GameState::Playing: renderHud(painter_); break;
            case GameState::Document: renderDocument(painter_); break;
            case GameState::Journal: renderJournal(painter_); break;
            case GameState::Ending: renderEnding(painter_); break;
            case GameState::Paused: {
                renderHud(painter_);
                painter_.clearClip();
                painter_.rect(0, 0, W, H, rgba(4, 4, 6, 180));
                float s = uiScale_;
                painter_.textCentered(fontTitle_, "PAUSED", W * 0.5f, H * 0.22f, C_BONE, 0.9f * s);
                const char* items[5] = {"RESUME", "JOURNAL", "SETTINGS", "SAVE PROGRESS", "QUIT TO TITLE"};
                for (int i = 0; i < 5; ++i) {
                    bool sel = i == menuIndex_;
                    if (sel) {
                        painter_.rect(W * 0.5f - 130 * s, H * 0.38f + i * 40 * s - 4 * s, 260 * s, 32 * s,
                                      rgba(26, 24, 22, 210));
                        painter_.rect(W * 0.5f - 130 * s, H * 0.38f + i * 40 * s - 4 * s, 2 * s, 32 * s, C_ACCENT);
                    }
                    painter_.textCentered(fontSmall_, items[i], W * 0.5f, H * 0.38f + i * 40 * s,
                                          sel ? C_BONE : C_DIM, 0.78f * s);
                }
                break;
            }
            default: break;
        }
    }
    p_->present(r_->pixels(), W, H);
}

// ------------------------------------------------------------------ headless test run
void Game::runHeadless(int frames, const std::string& shotPrefix) {
    Input in;
    setInput(&in);
    int W = 0, H = 0;
    p_->getSize(W, H);
    for (int i = 0; i < frames; ++i) {
        update(1.0f / 60.0f);
        render();
        if (i % 30 == 0 && !shotPrefix.empty()) {
            Image img(W, H);
            memcpy(img.px.data(), r_->pixels(), (size_t)W * H * 4);
            pngWrite(strfmt("%s_%03d.png", shotPrefix.c_str(), i).c_str(), img);
        }
    }
}

} // namespace dl
