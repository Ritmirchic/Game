// ============================================================================
//  dl_game.cpp : game lifetime, player, physics, interaction, progression
// ============================================================================
#include "dl_game.h"

namespace dl {
using namespace nf;

// ------------------------------------------------------------------ helpers
static bool overlapsXZ(const Box3& b, const V3& p, float r) {
    return p.x + r > b.lo.x && p.x - r < b.hi.x && p.y + r > b.lo.y && p.y - r < b.hi.y;
}

// ============================================================================
//  init / shutdown
// ============================================================================
bool Game::init(Renderer& r, Platform& p, Audio& a, const std::string& assetDir) {
    r_ = &r;
    p_ = &p;
    audio_ = &a;
    assetDir_ = assetDir;
    std::string fonts = joinPath(assetDir, "fonts");
    if (!fontTitle_.load(joinPath(fonts, "serif_64.png"), joinPath(fonts, "serif_64.nfp")))
        logMsg("[ui] title font missing");
    if (!fontBody_.load(joinPath(fonts, "sans_24.png"), joinPath(fonts, "sans_24.nfp")))
        logMsg("[ui] body font missing");
    if (!fontSmall_.load(joinPath(fonts, "sansbold_28.png"), joinPath(fonts, "sansbold_28.nfp")))
        logMsg("[ui] small font missing");
    if (!fontMono_.load(joinPath(fonts, "mono_20.png"), joinPath(fonts, "mono_20.nfp")))
        logMsg("[ui] mono font missing");
    pngLoad(joinPath(assetDir, "ui/icons.png").c_str(), uiIcons_);

    sfx_.build(a);
    world_.build(r, joinPath(assetDir, "tex"));
    savePath_ = joinPath(userDataDir("DontLookBack"), "profile.sav");
    state_ = GameState::Boot;
    bootTimer_ = 0.0f;
    titleFade_ = 0.0f;
    a.startDevice();
    a.setBusVolume(Bus::AMBIENT, settings.ambientVolume);
    a.setBusVolume(Bus::SFX, settings.sfxVolume);
    a.setMasterVolume(settings.masterVolume);
    r.post().grain = settings.grain;
    r.setResolutionScale(settings.resolutionScale);
    logMsg("[game] initialised (%d anomalies available)", anomalyCount());
    return true;
}

void Game::shutdown() {
    if (audio_) {
        audio_->stopAll(0.0f);
        audio_->stopDevice();
    }
}

void Game::onResize(int w, int h) {
    if (!r_) return;
    r_->resize(w, h);
    uiScale_ = clampf(h / 900.0f, 0.72f, 2.0f);
}

// ============================================================================
//  world / cycle reset
// ============================================================================
void Game::loadWorld(bool regenerate) {
    (void)regenerate;
    for (auto& a : anoms_) if (a.active) resetAnomaly(a);
    anoms_.assign(anomalyCount(), Anomaly());
    for (int i = 0; i < anomalyCount(); ++i) anoms_[i].index = i;
    ents_.clear();
    followEntityId_ = -1;
    emergenceActive_ = false;
    useWorldBaseState();
}

// ============================================================================
//  player
// ============================================================================
void Game::startNewGame() {
    loadWorld(true);
    pl_ = Player();
    pl_.pos = world_.playerSpawn;
    pl_.pos.z = 1.62f;
    pl_.yaw = 0.0f;
    pl_.pitch = -0.03f;
    pl_.hasFlashlight = true;
    pl_.flashlightOn = true;
    pl_.battery = 1.0f;
    reported_ = 0;
    mistakes_ = 0;
    cycle_ = 1;
    playTime_ = 0.0f;
    journal_.clear();
    toasts_.clear();
    usedAnomalies_.clear();
    ending_ = EndingId::None;
    state_ = GameState::Playing;
    pendingAnomalies_ = 1;
    anomalyCooldown_ = 8.0f;
    reportedThisCycle_ = false;
    lastRoom_.clear();
    currentRoom_ = "apt47_hall";
    world_.currentFloor = 3;
    for (auto& d : world_.doors) {
        if (d.id == "apt47") { d.openness = 0.7f; d.state = DoorState::Ajar; }
        else if (d.id == "apt47_living" || d.id == "apt47_bath") { d.openness = 0.0f; d.state = DoorState::Closed; }
        if (d.opensWith == "maintenance_key") d.state = DoorState::Locked;
    }
    p_->setCursorCaptured(true);
    if (audio_) {
        audio_->stopAll(0.3f);
        droneLoop_ = audio_->play(sfx_.drone, 0.30f, 1.0f, Bus::AMBIENT, 0.0f, true);
        ambientLoop_ = audio_->play(sfx_.windOutside, 0.16f, 1.0f, Bus::AMBIENT, 0.0f, true);
        audio_->setReverb(0.30f, 0.72f);
    }
    toast("Apartment 47. You do not remember falling asleep here.", rgba(226, 214, 190), 6.0f);
    toast("F flashlight    E use    R report    TAB journal", rgba(150, 158, 172), 9.0f, true);
}

// ------------------------------------------------------------------ physics
namespace {
struct HitInfo { bool hit = false; float push = 0.0f; V3 normal; };
}

// ============================================================================
//  the real update (input passed through a member injected by update())
// ============================================================================
static Input* g_in = nullptr;

void Game::update(float dt) {
    if (!g_in) return;
    Input& in = *g_in;
    uiTime_ += dt;

    if (in.quit) { p_->destroyWindow(); return; }

    // global keys
    if (state_ == GameState::Playing) {
        if (in.wasPressed(KC_ESC)) {
            state_ = GameState::Paused;
            menuIndex_ = 0;
            p_->setCursorCaptured(false);
            if (audio_) audio_->play(sfx_.uiBack, 0.5f);
        } else if (in.wasPressed(KC_TAB)) {
            state_ = GameState::Journal;
            p_->setCursorCaptured(false);
            if (audio_) audio_->play(sfx_.uiSelect, 0.5f);
        }
    } else if (state_ == GameState::Paused) {
        if (in.wasPressed(KC_ESC)) {
            state_ = GameState::Playing;
            p_->setCursorCaptured(true);
        }
    } else if (state_ == GameState::Journal) {
        if (in.wasPressed(KC_TAB) || in.wasPressed(KC_ESC)) {
            state_ = GameState::Playing;
            p_->setCursorCaptured(true);
        }
    } else if (state_ == GameState::Document) {
        if (in.wasPressed(KC_E) || in.wasPressed(KC_ESC) || in.mousePressed[0]) {
            state_ = GameState::Playing;
            openDocument_ = -1;
            p_->setCursorCaptured(true);
        }
    }

    switch (state_) {
        case GameState::Boot: {
            bootTimer_ += dt;
            if (bootTimer_ > 1.6f) {
                state_ = GameState::Title;
                if (audio_) audio_->play(sfx_.drone, 0.25f, 1.0f, Bus::AMBIENT, 0.0f, true);
            }
            break;
        }
        case GameState::Title: {
            titleFade_ = minf(1.0f, titleFade_ + dt * 0.5f);
            if (in.wasPressed(KC_UP)) { menuIndex_ = (menuIndex_ + 5) % 6; if (audio_) audio_->play(sfx_.uiMove, 0.4f); }
            if (in.wasPressed(KC_DOWN)) { menuIndex_ = (menuIndex_ + 1) % 6; if (audio_) audio_->play(sfx_.uiMove, 0.4f); }
            if (in.wasPressed(KC_ENTER) || in.wasPressed(KC_SPACE)) {
                if (audio_) audio_->play(sfx_.uiSelect, 0.6f);
                if (menuIndex_ == 0) startNewGame();
                else if (menuIndex_ == 1) { if (loadGame()) { state_ = GameState::Playing; p_->setCursorCaptured(true); } else toast("No saved profile found."); }
                else if (menuIndex_ == 2) state_ = GameState::Settings;
                else if (menuIndex_ == 3) { /* credits/controls page */ state_ = GameState::Settings; settingsIndex_ = 9; }
                else if (menuIndex_ == 4) { if (in.wasPressed(KC_ENTER)) { in.quit = true; } }
                else in.quit = true;
            }
            break;
        }
        case GameState::Playing: {
            playTime_ += dt;
            updateCamera(dt);
            updatePlayerReal(dt, in);
            updateDoors(dt);
            updateInteractables(dt);
            updateAnomalies(dt);
            updateEntities(dt);
            updateCycleLogic(dt);
            updateAudio(dt);
            updateHud(dt);
            break;
        }
        case GameState::Paused: {
            static const char* items[5] = {"RESUME", "JOURNAL", "SETTINGS", "SAVE PROGRESS", "QUIT TO TITLE"};
            if (in.wasPressed(KC_UP)) { menuIndex_ = (menuIndex_ + 4) % 5; if (audio_) audio_->play(sfx_.uiMove, 0.4f); }
            if (in.wasPressed(KC_DOWN)) { menuIndex_ = (menuIndex_ + 1) % 5; if (audio_) audio_->play(sfx_.uiMove, 0.4f); }
            if (in.wasPressed(KC_ENTER)) {
                if (audio_) audio_->play(sfx_.uiSelect, 0.6f);
                if (menuIndex_ == 0) { state_ = GameState::Playing; p_->setCursorCaptured(true); }
                else if (menuIndex_ == 1) state_ = GameState::Journal;
                else if (menuIndex_ == 2) { state_ = GameState::Settings; prevState_ = GameState::Paused; settingsIndex_ = 0; }
                else if (menuIndex_ == 3) { saveGame(); toast("Progress saved."); }
                else if (menuIndex_ == 4) { state_ = GameState::Title; menuIndex_ = 0; if (audio_) audio_->stopAll(0.4f); }
            }
            break;
        }
        case GameState::Settings: {
            int n = 9;
            if (in.wasPressed(KC_UP)) { settingsIndex_ = (settingsIndex_ + n - 1) % n; if (audio_) audio_->play(sfx_.uiMove, 0.4f); }
            if (in.wasPressed(KC_DOWN)) { settingsIndex_ = (settingsIndex_ + 1) % n; if (audio_) audio_->play(sfx_.uiMove, 0.4f); }
            float dir = 0;
            if (in.wasPressed(KC_LEFT)) dir = -1;
            if (in.wasPressed(KC_RIGHT)) dir = 1;
            if (dir != 0) {
                switch (settingsIndex_) {
                    case 0: settings.mouseSensitivity = clampf(settings.mouseSensitivity + dir * 0.1f, 0.2f, 3.0f); break;
                    case 1: settings.masterVolume = clampf(settings.masterVolume + dir * 0.05f, 0.0f, 1.0f);
                            if (audio_) audio_->setMasterVolume(settings.masterVolume); break;
                    case 2: settings.ambientVolume = clampf(settings.ambientVolume + dir * 0.05f, 0.0f, 1.0f);
                            if (audio_) audio_->setBusVolume(Bus::AMBIENT, settings.ambientVolume); break;
                    case 3: settings.sfxVolume = clampf(settings.sfxVolume + dir * 0.05f, 0.0f, 1.0f);
                            if (audio_) audio_->setBusVolume(Bus::SFX, settings.sfxVolume); break;
                    case 4: settings.brightness = clampf(settings.brightness + dir * 0.05f, 0.6f, 1.6f); break;
                    case 5: settings.resolutionScale = clampf(settings.resolutionScale + dir * 0.1f, 0.5f, 1.0f);
                            if (r_) r_->setResolutionScale(settings.resolutionScale); break;
                    case 6: settings.grain = clampf(settings.grain + dir * 0.005f, 0.0f, 0.09f); break;
                    case 7: settings.invertY = !settings.invertY; break;
                    case 8: settings.fullscreen = !settings.fullscreen; if (p_) p_->setFullscreen(settings.fullscreen); break;
                }
                if (audio_) audio_->play(sfx_.uiMove, 0.4f);
            }
            if (in.wasPressed(KC_ESC) || in.wasPressed(KC_ENTER)) {
                state_ = (prevState_ == GameState::Title) ? GameState::Title : GameState::Paused;
                if (state_ == GameState::Paused) p_->setCursorCaptured(false);
                if (audio_) audio_->play(sfx_.uiBack, 0.5f);
            }
            break;
        }
        case GameState::Journal:
        case GameState::Document:
        case GameState::Dead:
        case GameState::Ending: {
            if (state_ == GameState::Ending) {
                endingTimer_ += dt;
                if (endingTimer_ > 3.0f && (in.wasPressed(KC_ENTER) || in.wasPressed(KC_SPACE))) {
                    state_ = GameState::Title;
                    menuIndex_ = 0;
                    if (audio_) audio_->stopAll(1.0f);
                }
            }
            break;
        }
        default: break;
    }
    in.newFrame();
}

// ============================================================================
//  player movement / collision
// ============================================================================
float groundHeightAt(const World& w, const V3& p, float z, float r) {
    float best = -100.0f;
    for (auto& c : w.colliders) {
        if (!c.box.overlaps(Box3(V3(p.x - r, p.y - r, z - 2.2f), V3(p.x + r, p.y + r, z + 0.42f)))) continue;
        if (!overlapsXZ(c.box, p, r * 0.8f)) continue;
        float top = c.box.hi.z;
        if (top <= z + 0.42f && top > best) best = top;
    }
    return best;
}

void Game::updatePlayerReal(float dt, Input& in) {
    // ---- look
    float sens = 0.0022f * settings.mouseSensitivity;
    pl_.yaw -= in.mouseDX * sens;
    pl_.pitch -= in.mouseDY * sens * (settings.invertY ? -1.0f : 1.0f);
    pl_.pitch = clampf(pl_.pitch, -1.35f, 1.30f);
    if (pl_.yaw > PI) pl_.yaw -= TAU;
    if (pl_.yaw < -PI) pl_.yaw += TAU;

    V3 fwd(std::cos(pl_.yaw), std::sin(pl_.yaw), 0);
    V3 right(std::sin(pl_.yaw), -std::cos(pl_.yaw), 0);

    float mx = 0, my = 0;
    if (in.isDown(KC_W)) my += 1;
    if (in.isDown(KC_S)) my -= 1;
    if (in.isDown(KC_D)) mx += 1;
    if (in.isDown(KC_A)) mx -= 1;
    V3 wish = fwd * my + right * mx;
    float wl = length(wish);
    if (wl > 0.001f) wish = wish / wl;

    bool wantRun = in.isDown(KC_SHIFT) && wl > 0.1f;
    pl_.running = wantRun && pl_.stamina > 0.05f;
    if (pl_.running) pl_.stamina = maxf(0.0f, pl_.stamina - dt * 0.14f);
    else pl_.stamina = minf(1.0f, pl_.stamina + dt * 0.075f);

    float speed = pl_.running ? 3.9f : 1.95f;
    if (pl_.crouching) speed *= 0.55f;
    // fear slows you
    speed *= (1.0f - pl_.fear * 0.12f);

    V3 targetVel = wish * speed;
    float accel = pl_.grounded ? 14.0f : 5.0f;
    pl_.vel.x = dampf(pl_.vel.x, targetVel.x, accel, dt);
    pl_.vel.y = dampf(pl_.vel.y, targetVel.y, accel, dt);

    // ---- horizontal move + collide
    V3 np = pl_.pos;
    np.x += pl_.vel.x * dt;
    np.y += pl_.vel.y * dt;
    float r = pl_.radius;
    for (int pass = 0; pass < 3; ++pass) {
        for (auto& c : world_.colliders) {
            if (!c.blocksWalk) continue;
            if (!overlapsXZ(c.box, np, r)) continue;
            bool vertical = np.z + 0.18f > c.box.lo.z && np.z + 0.18f < c.box.hi.z - 0.02f;
            bool stepable = c.box.hi.z - np.z <= 0.42f && c.box.hi.z - np.z > 0.0f;
            if (!vertical && !stepable) continue;
            if (stepable && c.box.hi.z - (np.z - pl_.eyeHeight) < 0.45f) continue;
            if (vertical) {
                // push out along the smallest penetration axis
                float px1 = c.box.hi.x - (np.x - r), px2 = (np.x + r) - c.box.lo.x;
                float py1 = c.box.hi.y - (np.y - r), py2 = (np.y + r) - c.box.lo.y;
                float m = minf(minf(px1, px2), minf(py1, py2));
                if (m == px1) np.x += px1 + 0.001f;
                else if (m == px2) np.x -= px2 + 0.001f;
                else if (m == py1) np.y += py1 + 0.001f;
                else np.y -= py2 + 0.001f;
            }
        }
    }
    // keep the player inside the building bounds
    np.x = clampf(np.x, -14.5f, 9.6f);
    np.y = clampf(np.y, -11.8f, 14.0f);
    pl_.pos.x = np.x;
    pl_.pos.y = np.y;

    // ---- ground / steps
    float gh = groundHeightAt(world_, pl_.pos, pl_.pos.z, r);
    if (gh > -50.0f) {
        float target = gh + pl_.eyeHeight;
        if (pl_.pos.z < target) pl_.pos.z = minf(target, pl_.pos.z + dt * 6.5f);
        else pl_.pos.z = maxf(target, pl_.pos.z - dt * 9.0f);
        pl_.grounded = true;
    } else {
        pl_.pos.z -= dt * 6.0f;
        pl_.grounded = false;
        if (pl_.pos.z < -6.0f) { pl_.pos = world_.playerSpawn; toast("You fall through the floor... and wake up again."); }
    }
    // gravity into the basement pit is prevented by the slabs; the stairs are ramps of boxes

    // ---- head bob + footsteps
    float hv = length(V3(pl_.vel.x, pl_.vel.y, 0));
    if (hv > 0.2f && pl_.grounded) {
        pl_.bobPhase += dt * (pl_.running ? 11.0f : 7.2f);
        pl_.stepTimer -= dt;
        float interval = pl_.running ? 0.34f : 0.56f;
        if (pl_.stepTimer <= 0.0f) {
            pl_.stepTimer = interval;
            int idx = (int)(hashf((uint32_t)(uiTime_ * 1000.0f)) * (pl_.running ? 4 : 6));
            if (audio_ && pl_.running) audio_->play(sfx_.stepRun[idx % 4], 0.42f, 0.94f + hashf((uint32_t)uiTime_) * 0.12f);
            else if (audio_) audio_->play(sfx_.step[idx % 6], 0.36f, 0.92f + hashf((uint32_t)uiTime_) * 0.16f);
            makeNoise(pl_.running ? 1.0f : 0.45f, pl_.pos);
        }
    } else {
        pl_.stepTimer = 0.0f;
        pl_.bobPhase = dampf(pl_.bobPhase, 0.0f, 4.0f, dt);
    }
    pl_.bob = std::sin(pl_.bobPhase) * (pl_.running ? 0.045f : 0.028f) * (settings.headBob ? 1.0f : 0.0f);

    // ---- breathing / heartbeat
    pl_.breathTimer -= dt;
    if (pl_.breathTimer <= 0.0f) {
        float rate = 3.6f - pl_.fear * 1.4f + (pl_.running ? -1.2f : 0.0f);
        pl_.breathTimer = maxf(1.6f, rate);
        if (pl_.fear > 0.45f && audio_) audio_->play(sfx_.breathing, 0.16f + pl_.fear * 0.2f, 0.9f + hashf((uint32_t)uiTime_) * 0.2f, Bus::SFX, 0.0f);
    }
    heartbeatTimer_ -= dt;
    if (heartbeatTimer_ <= 0.0f && pl_.fear > 0.28f) {
        heartbeatTimer_ = lerpf(1.15f, 0.52f, pl_.fear);
        if (audio_) audio_->play(sfx_.heartbeat, 0.16f + pl_.fear * 0.34f);
    }

    // ---- flashlight
    if (in.wasPressed(KC_F)) toggleFlashlight();
    if (pl_.flashlightOn) {
        pl_.battery -= dt * 0.0055f;
        if (pl_.battery <= 0.0f) {
            pl_.battery = 0.0f;
            pl_.flashlightOn = false;
            toast("The flashlight dies.", rgba(200, 150, 120), 4.0f);
        }
    }

    // ---- fear decay
    pl_.noise = lastNoiseTimer_;
    lastNoiseTimer_ = maxf(0.0f, lastNoiseTimer_ - dt * 1.5f);
    pl_.fear = dampf(pl_.fear, pl_.fear * 0.97f, 0.6f, dt);
    pl_.damageFlash = maxf(0.0f, pl_.damageFlash - dt * 1.6f);

    // ---- interaction / report
    if (in.wasPressed(KC_E)) interact();
    if (in.wasPressed(KC_R)) reportAnomaly();
}

// ============================================================================
//  camera
// ============================================================================
void Game::updateCamera(float dt) {
    if (pl_.shakeT > 0.0f) pl_.shakeT = maxf(0.0f, pl_.shakeT - dt);
}

// ============================================================================
//  doors
// ============================================================================
void Game::updateDoors(float dt) {
    for (auto& d : world_.doors) {
        float target = d.targetOpenness;
        if (d.state == DoorState::Locked) target = 0.0f;
        bool wasMoving = std::fabs(d.openness - target) > 0.001f;
        d.openness = dampf(d.openness, target, 5.0f, dt);
        if (std::fabs(d.openness - target) < 0.004f) d.openness = target;
        if (wasMoving) {
            world_.rebuildDoorBox(d.index);
            // creak audio while swinging
            d.creakTimer -= dt;
            if (d.creakTimer <= 0.0f && std::fabs(d.targetOpenness - d.openness) > 0.05f) {
                d.creakTimer = 0.7f;
                if (audio_) {
                    float vol = clampf(1.0f - length(d.hinge - pl_.pos) / 12.0f, 0.0f, 1.0f) * 0.5f;
                    if (vol > 0.03f) audio_->play3D(sfx_.doorCreak, d.hinge, vol * 0.7f, 0.9f + hashf((uint32_t)uiTime_) * 0.2f,
                                                  Bus::SFX, false, 14.0f);
                }
                makeNoise(0.5f, d.hinge);
            }
        }
    }
}

// ============================================================================
//  interaction
// ============================================================================
void Game::updateInteractables(float dt) {
    hoverInteract_ = -1;
    hoverText_.clear();
    V3 eye = pl_.pos;
    V3 fwd(std::cos(pl_.yaw) * std::cos(pl_.pitch), std::sin(pl_.yaw) * std::cos(pl_.pitch), std::sin(pl_.pitch));
    float bestScore = -1.0f;
    // doors
    for (auto& d : world_.doors) {
        V3 c = (d.hinge + d.hinge + d.axis * d.width) * 0.5f + V3(0, 0, 1.0f);
        V3 to = c - eye;
        float dist = length(to);
        if (dist > 2.4f) continue;
        float dotv = dot(normalize(to), fwd);
        if (dotv < 0.35f) continue;
        // skip the leaf you are standing in
        float score = dotv - dist * 0.12f;
        if (score > bestScore) {
            bestScore = score;
            hoverInteract_ = -2;
            hoverText_ = (d.state == DoorState::Locked) ? "LOCKED" : (d.openness > 0.5f ? "CLOSE DOOR" : "OPEN DOOR");
            // store the door index in the lower bits (negative space scheme)
            hoverInteract_ = -1000 - d.index;
        }
    }
    // interactables
    for (size_t i = 0; i < world_.interactables.size(); ++i) {
        auto& it = world_.interactables[i];
        if (!it.enabled) continue;
        float dist = length(it.pos - eye);
        if (dist > it.radius) continue;
        V3 to = normalize(it.pos - eye);
        float dotv = dot(to, fwd);
        if (dotv < 0.15f && dist > 1.0f) continue;
        float score = dotv * 1.2f - dist * 0.10f + 0.5f;
        if (score > bestScore) {
            bestScore = score;
            hoverInteract_ = (int)i;
            hoverText_ = it.prompt;
        }
    }
    // documents
    for (size_t i = 0; i < world_.documents.size(); ++i) {
        auto& doc = world_.documents[i];
        float dist = length(doc.pos - eye);
        if (dist > 2.0f) continue;
        V3 to = normalize(doc.pos - eye);
        float dotv = dot(to, fwd);
        if (dotv < 0.2f) continue;
        float score = dotv + 0.08f - dist * 0.06f;
        if (score > bestScore) {
            bestScore = score;
            hoverInteract_ = 10000 + (int)i;
            hoverText_ = doc.found ? "READ AGAIN" : "READ PAPER";
        }
    }
    // player start note (flashlight pickup)
    (void)dt;
}

void Game::interact() {
    if (hoverInteract_ == -1) return;
    if (hoverInteract_ <= -1000) {
        int di = -hoverInteract_ - 1000;
        if (di < 0 || di >= (int)world_.doors.size()) return;
        Door& d = world_.doors[di];
        if (d.state == DoorState::Locked) {
            if (!d.opensWith.empty()) {
                bool have = (d.opensWith == "maintenance_key" && pl_.hasMaintenanceKey);
                if (have) {
                    d.state = DoorState::Closed;
                    d.targetOpenness = 1.0f;
                    if (audio_) audio_->play3D(sfx_.keyUnlock, d.hinge, 0.7f, 1.0f);
                    toast("The key turns. The fire door opens.", rgba(180, 220, 180), 3.5f);
                    makeNoise(0.8f, d.hinge);
                } else {
                    if (audio_) audio_->play3D(sfx_.doorLock, d.hinge, 0.6f, 1.0f);
                    toast(d.lockedHint.empty() ? "It is locked." : d.lockedHint, rgba(200, 180, 160), 3.0f);
                }
            } else {
                if (audio_) audio_->play3D(sfx_.doorLock, d.hinge, 0.6f, 1.0f);
                toast("Locked. Something on the other side is very quiet.", rgba(200, 180, 160), 3.0f);
            }
            return;
        }
        bool opening = d.openness < 0.5f;
        d.targetOpenness = opening ? 1.0f : 0.0f;
        d.state = opening ? DoorState::Open : DoorState::Closed;
        if (audio_ && !opening) audio_->play3D(sfx_.doorSlam, d.hinge, 0.35f, 0.9f);
        makeNoise(opening ? 0.6f : 0.9f, d.hinge);
        return;
    }
    if (hoverInteract_ >= 10000) {
        int di = hoverInteract_ - 10000;
        if (di < 0 || di >= (int)world_.documents.size()) return;
        auto& doc = world_.documents[di];
        doc.found = true;
        openDocument_ = di;
        state_ = GameState::Document;
        docScroll_ = 0.0f;
        p_->setCursorCaptured(false);
        if (audio_) audio_->play(sfx_.paper, 0.4f, 1.0f);
        return;
    }
    if (hoverInteract_ >= 0 && hoverInteract_ < (int)world_.interactables.size()) {
        auto& it = world_.interactables[hoverInteract_];
        if (it.id == "item_fuse") {
            it.enabled = false;
            pl_.hasFuse = true;
            if (audio_) audio_->play(sfx_.pickup, 0.6f);
            toast("Found: a ceramic fuse. It is still warm.", rgba(220, 200, 160), 4.0f);
            // restoring the boiler lights
            for (auto& f : world_.fixtures) if (f.roomId == "boiler") f.dead = false;
            return;
        }
        if (it.id == "item_maintenance_key") {
            it.enabled = false;
            pl_.hasMaintenanceKey = true;
            if (audio_) audio_->play(sfx_.pickup, 0.7f, 1.0f);
            toast("Found: the maintenance key. It is warm from someone's hand.", rgba(220, 200, 160), 5.0f);
            addJournalItem("The maintenance key",
                           "It was hanging on a nail by the maintenance door, at the height a tall "
                           "man would reach without looking.", AnomKind::Environmental, true);
            return;
        }
        if (it.id == "stair_window") {
            toast("The courtyard is empty. Across it, every window on your floor is lit,", rgba(200, 205, 215), 5.0f);
            toast("and in each one a figure stands with its back turned.", rgba(200, 205, 215), 6.0f, true);
            pl_.fear = saturate(pl_.fear + 0.12f);
            addJournalItem("The courtyard", "Every window on the third floor is lit, and each has "
                           "someone standing in it, facing away.", AnomKind::Visual, true);
            makeNoise(0.1f, it.pos);
            return;
        }
        if (it.id == "item_torch_battery") {
            it.enabled = false;
            pl_.battery = 1.0f;
            if (audio_) audio_->play(sfx_.pickup, 0.6f);
            toast("Found: fresh batteries for the torch.", rgba(220, 210, 170), 4.0f);
            addJournalItem("Batteries", "A pack of D-cells from the maintenance locker. The torch is strong again.", AnomKind::Visual, true);
            return;
        }
        if (it.id == "lever_hidden") {
            if (!pl_.hasFuse) {
                toast("The valve is dead. A fuse is missing from the panel.", rgba(200, 170, 140), 3.4f);
                return;
            }
            if (!pl_.leverPulled) {
                pl_.leverPulled = true;
                if (audio_) audio_->play3D(sfx_.metalDrag, it.pos, 0.8f, 0.8f);
                p_->setCursorCaptured(true);
                toast("Somewhere below, brick grinds against brick.", rgba(200, 170, 140), 5.0f);
                makeNoise(1.0f, it.pos);
                // open the false wall: hide the panel group and its collider
                for (auto& g : world_.groups) {
                    if (g.mesh.bounds.center().z < -3.2f && std::fabs(g.mesh.bounds.center().x + 0.8f) < 1.6f &&
                        std::fabs(g.mesh.bounds.center().y - 0.1f) < 0.6f && g.mat == MAT_BRICK) {
                        // leave visible; the collider is what blocks
                    }
                }
                for (auto& c : world_.colliders) {
                    if (c.box.lo.z < -3.0f && c.box.hi.y < 0.4f && c.box.lo.x > -1.6f && c.box.hi.x < -0.1f)
                        c.blocksWalk = false;
                }
                world_.buildNav();
                for (auto& f : world_.fixtures) if (f.roomId == "hidden") f.dead = false;
                addJournalItem("The valve", "I opened the valve and the false wall moved. There is a room behind the coal store.", AnomKind::Environmental, true);
            }
            return;
        }
        if (it.id == "ending_exit") {
            if (pl_.hasMaintenanceKey) triggerEnding(EndingId::Escape);
            else toast("The fire door will not open without the maintenance key.", rgba(200, 180, 160), 3.4f);
            return;
        }
        if (it.id == "ending_descent") {
            if (pl_.leverPulled) triggerEnding(EndingId::Descent);
            else toast("The wall is warm. Something behind it is waiting to be seen.", rgba(200, 160, 140), 3.4f);
            return;
        }
        if (it.id == "elevator_panel") {
            if (elev_.moving) { toast("The car is already moving."); return; }
            if (!elev_.doorsOpen) {
                elev_.doorsOpen = true;
                if (audio_) audio_->play3D(sfx_.elevatorDing, it.pos, 0.6f, 1.0f);
                if (Door* d = world_.doorById("elevator_car")) { d->targetOpenness = 1.0f; d->state = DoorState::Open; }
                toast("The lift doors open. The car is empty. It smells of rain.", rgba(190, 200, 210), 4.0f);
            } else {
                elevatorTravel();
            }
            return;
        }
        if (it.id == "player_start_note") {
            p_->setCursorCaptured(true);
            toast("Your own handwriting on the wall: COUNT THE DOORS.", rgba(210, 190, 160), 4.5f);
            it.enabled = false;
            return;
        }
        if (!it.denyMessage.empty()) toast(it.denyMessage);
    }
}

void Game::toggleFlashlight() {
    pl_.flashlightOn = !pl_.flashlightOn && pl_.battery > 0.0f;
    if (audio_) audio_->play(sfx_.uiMove, 0.28f, 1.6f);
}

void Game::makeNoise(float loudness, const V3& at) {
    if (loudness > lastNoiseTimer_) lastNoiseTimer_ = loudness;
    // the Listener reacts to noise
    for (auto& e : ents_) {
        if (!e.active || e.type != EntityType::Listener) continue;
        float d = length(e.pos - at);
        if (d < 26.0f * loudness) {
            e.target = at;
            e.state = 1;
            e.repathTimer = 0.0f;
        }
    }
}

void Game::toast(const std::string& s, Color c, float life, bool sub) {
    Toast t;
    t.text = s;
    t.t = 0.0f;
    t.life = life;
    t.col = c;
    t.sub = sub;
    toasts_.push_back(t);
    if (toasts_.size() > 6) toasts_.erase(toasts_.begin());
}

void Game::updateHud(float dt) {
    for (auto& t : toasts_) t.t += dt;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                                 [](const Toast& t) { return t.t > t.life; }), toasts_.end());
}

// ============================================================================
//  audio runtime
// ============================================================================
void Game::updateAudio(float dt) {
    if (!audio_) return;
    V3 fwd(std::cos(pl_.yaw), std::sin(pl_.yaw), 0);
    audio_->setListener(pl_.pos, fwd, V3(0, 0, 1));
    // room-based ambience: more reverb in the stairwell / basement
    float roomSize = 0.62f;
    if (currentRoom_.find("stair") != std::string::npos) roomSize = 0.9f;
    if (currentRoom_.find("basement") != std::string::npos) roomSize = 0.85f;
    if (currentRoom_.find("lobby") != std::string::npos) roomSize = 0.8f;
    audio_->setReverb(0.24f + roomSize * 0.18f, roomSize);
    if (droneLoop_) audio_->setHandleVolume(droneLoop_, 0.16f + pl_.fear * 0.22f);
    // random ambient events
    static float ambientTimer = 8.0f;
    ambientTimer -= dt;
    if (ambientTimer <= 0.0f) {
        ambientTimer = 7.0f + hashf((uint32_t)(uiTime_ * 13.0f)) * 16.0f;
        float pick = hashf((uint32_t)(uiTime_ * 37.0f));
        V3 at = pl_.pos + V3(std::cos(pick * TAU) * 6.0f, std::sin(pick * TAU) * 6.0f, 0.0f);
        if (pick < 0.22f) audio_->play3D(sfx_.waterDrip, at, 0.5f, 0.9f + pick, Bus::SFX, false, 12.0f);
        else if (pick < 0.42f) audio_->play3D(sfx_.pipeBang, at, 0.42f, 0.8f + pick * 0.5f, Bus::SFX, false, 18.0f);
        else if (pick < 0.6f) audio_->play3D(sfx_.electricity, at, 0.25f, 1.0f, Bus::SFX, false, 8.0f);
        else if (pick < 0.78f) audio_->play3D(sfx_.metalDrag, at, 0.3f, 0.7f, Bus::SFX, false, 14.0f);
        else if (pick < 0.9f) audio_->play3D(sfx_.knock[2], at, 0.35f, 1.0f, Bus::SFX, false, 16.0f);
        else audio_->play3D(sfx_.whisper[0], at, 0.3f, 1.0f, Bus::SFX, false, 10.0f);
    }
    (void)dt;
}

// ============================================================================
//  cycle / progression
// ============================================================================
void Game::updateCycleLogic(float dt) {
    cycleTimer_ += dt;
    anomalyCooldown_ -= dt;
    // detect room changes
    RoomInfo* room = world_.roomAt(pl_.pos);
    if (room) {
        if (room->id != currentRoom_) {
            lastRoom_ = currentRoom_;
            currentRoom_ = room->id;
            roomEntered(room->id, pl_.pos);
        }
    }
    // the horror escalates
    if (cycleTimer_ > 26.0f + cycle_ * 6.0f) {
        cycleTimer_ = 0.0f;
        if (activeAnomalyCount() == 0 && pendingAnomalies_ > 0) {
            stageAnomalyForRoom(currentRoom_, pl_.pos);
        }
    }
    // keep at least one anomaly present while exploring
    if (anomalyCooldown_ <= 0.0f && activeAnomalyCount() == 0) {
        pendingAnomalies_ = maxi(1, pendingAnomalies_);
        if (stageAnomalyForRoom(currentRoom_, pl_.pos)) anomalyCooldown_ = 14.0f;
        else anomalyCooldown_ = 3.0f;
    }
    pl_.fear = saturate(pl_.fear + (activeAnomalyCount() > 0 ? dt * 0.004f : -dt * 0.02f));
    if (mistakes_ >= 5 && state_ == GameState::Playing && ending_ == EndingId::None) {
        triggerEnding(EndingId::Assimilation);
    }
    // emergency light timers
    if (emergenceActive_) {
        emergenceTimer_ -= dt;
        if (emergenceTimer_ <= 0.0f) {
            emergenceActive_ = false;
            applyAnomaly(22, false);
        }
    }
    // elevator
    if (elev_.moving) {
        elev_.t += dt;
        pl_.shakeAmp = 0.012f;
        pl_.shakeT = 0.2f;
        if (elev_.t > elev_.duration) {
            elev_.moving = false;
            pl_.shakeAmp = 0.0f;
            finalizeElevatorTravel();
        }
    }
}

void Game::roomEntered(const std::string& roomId, const V3& at) {
    // first-visit flavour text
    RoomInfo* r = world_.roomById(roomId);
    if (!r) return;
    if (activeAnomalyCount() == 0 && anomalyCooldown_ <= 0.0f && pendingAnomalies_ > 0) {
        if (!stageAnomalyForRoom(roomId, at)) {
            // fall back to a nearby room
            stageAnomalyForRoom(currentRoom_, at);
        }
        anomalyCooldown_ = 10.0f;
    }
    // the Observer is drawn to the player after enough progress
    if (reported_ >= 3 && ents_.empty() && hashf((uint32_t)(uiTime_ * 91.0f)) > 0.55f) {
        if (roomId.find("corridor") != std::string::npos || roomId.find("lobby") != std::string::npos) {
            spawnEntity(EntityType::Observer, at + V3(std::cos(pl_.yaw) * 9.0f, std::sin(pl_.yaw) * 9.0f, 0.0f));
        }
    }
}

// ============================================================================
//  reporting
// ============================================================================
void Game::reportAnomaly() {
    int found = -1;
    float bestDist = 6.5f;
    for (auto& a : anoms_) {
        if (!a.active) continue;
        float d = length(a.anchor - pl_.pos);
        bool sameRoom = (a.room == currentRoom_);
        if (sameRoom || d < bestDist) {
            if (d < bestDist || sameRoom) {
                if (d < bestDist) { bestDist = d; found = a.index; }
                else if (found < 0) { found = a.index; }
            }
        }
    }
    if (found < 0) {
        wrongReport("Nothing here is wrong. You are inventing it.");
        return;
    }
    correctReport(found);
}

void Game::correctReport(int idx) {
    Anomaly& a = anoms_[idx];
    const AnomalyDef& def = anomalyDef(idx);
    reported_++;
    reportedThisCycle_ = true;
    pendingAnomalies_ = mini(3, 1 + reported_ / 4);
    anomalyCooldown_ = 12.0f;
    addJournalItem(def.name, def.clue, def.kind, true);
    if (audio_) audio_->play(sfx_.reportGood, 0.55f);
    toast(strfmt("REPORTED: %s", def.name), rgba(170, 215, 180), 4.5f);
    pl_.fear = maxf(0.0f, pl_.fear - 0.18f);
    // the world corrects itself
    resetAnomaly(a);
    if (reported_ >= 12) {
        toast("Twelve anomalies. Somewhere a door unlocks.", rgba(220, 200, 160), 6.0f);
        if (Door* d = world_.doorById("maintenance")) { d->state = DoorState::Closed; }
        for (auto& it : world_.interactables) if (it.id == "item_maintenance_key_enable") it.enabled = true;
        // the maintenance key appears in the maintenance room
        bool has = false;
        for (auto& it : world_.interactables) if (it.id == "item_maintenance_key") has = true;
        if (!has) {
            Interactable it;
            it.id = "item_maintenance_key";
            it.pos = V3(6.4f, 11.6f, 0.92f);
            it.radius = 2.0f;
            it.type = InteractType::Item;
            world_.interactables.push_back(it);
        }
    }
    if (reported_ >= 6 && !pl_.hasFuse) {
        toast("The basement door below the stairs is no longer locked.", rgba(200, 190, 170), 5.0f);
        if (auto c = world_.colliders.size()) (void)c;
    }
}

void Game::wrongReport(const std::string& msg) {
    mistakes_++;
    if (audio_) audio_->play(sfx_.reportBad, 0.6f);
    toast(strfmt("FALSE REPORT (%d/5): %s", mistakes_, msg.c_str()), rgba(215, 140, 130), 5.0f);
    addJournalItem("A false report", msg + "\n\nThe building noticed.", AnomKind::Entity, false);
    pl_.fear = saturate(pl_.fear + 0.25f);
    // penalty: lights fail in the current room
    for (auto& f : world_.fixtures) {
        if (f.roomId == currentRoom_) f.on = false;
    }
    if (mistakes_ >= 3) {
        spawnEntity(EntityType::Imitator, pl_.pos + V3(std::cos(pl_.yaw + PI) * 7.0f,
                                                     std::sin(pl_.yaw + PI) * 7.0f, 0.0f));
    }
}

void Game::addJournalItem(const std::string& title, const std::string& body, AnomKind k, bool correct) {
    JournalEntry e;
    e.title = title;
    e.body = body;
    e.kind = k;
    e.correct = correct;
    e.time = playTime_;
    journal_.insert(journal_.begin(), e);
    if (journal_.size() > 40) journal_.pop_back();
}

// ============================================================================
//  endings
// ============================================================================
void Game::triggerEnding(EndingId e) {
    ending_ = e;
    state_ = GameState::Ending;
    endingTimer_ = 0.0f;
    p_->setCursorCaptured(false);
    if (audio_) {
        audio_->stopAll(1.2f);
        audio_->play(sfx_.endingWhoosh, 0.8f);
        audio_->play(sfx_.drone, 0.25f, 0.7f, Bus::AMBIENT, 0.0f, true);
    }
    for (auto& ent : ents_) removeEntity((int)(&ent - ents_.data()));
    ents_.clear();
}

// ============================================================================
//  save / load
// ============================================================================
bool Game::saveGame() {
    std::string s;
    s += "DLB1\n";
    s += strfmt("reported %d\n", reported_);
    s += strfmt("mistakes %d\n", mistakes_);
    s += strfmt("cycle %d\n", cycle_);
    s += strfmt("playtime %.3f\n", playTime_);
    s += strfmt("player %.3f %.3f %.3f %.3f %.3f\n", pl_.pos.x, pl_.pos.y, pl_.pos.z, pl_.yaw, pl_.pitch);
    s += strfmt("inventory %d %d %d %d\n", pl_.hasMaintenanceKey ? 1 : 0, pl_.hasFuse ? 1 : 0,
                pl_.leverPulled ? 1 : 0, pl_.flashlightOn ? 1 : 0);
    s += strfmt("battery %.3f\n", pl_.battery);
    for (auto& d : world_.documents) s += strfmt("doc %s %d\n", d.id.c_str(), d.found ? 1 : 0);
    for (auto& j : journal_) s += "entry " + j.title + "\n";
    return writeFile(savePath_.c_str(), s.data(), s.size());
}

bool Game::loadGame() {
    std::vector<uint8_t> raw;
    if (!readFile(savePath_.c_str(), raw)) return false;
    std::string s((const char*)raw.data(), raw.size());
    if (s.rfind("DLB1", 0) != 0) return false;
    loadWorld(true);
    pl_ = Player();
    pl_.hasFlashlight = true;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t eol = s.find('\n', pos);
        if (eol == std::string::npos) eol = s.size();
        std::string line = s.substr(pos, eol - pos);
        pos = eol + 1;
        if (line.rfind("reported ", 0) == 0) reported_ = atoi(line.c_str() + 9);
        else if (line.rfind("mistakes ", 0) == 0) mistakes_ = atoi(line.c_str() + 9);
        else if (line.rfind("cycle ", 0) == 0) cycle_ = atoi(line.c_str() + 6);
        else if (line.rfind("playtime ", 0) == 0) playTime_ = (float)atof(line.c_str() + 9);
        else if (line.rfind("player ", 0) == 0) {
            float x, y, z, ya, pi;
            if (sscanf(line.c_str(), "player %f %f %f %f %f", &x, &y, &z, &ya, &pi) == 5) {
                pl_.pos = V3(x, y, z); pl_.yaw = ya; pl_.pitch = pi;
            }
        } else if (line.rfind("inventory ", 0) == 0) {
            int a, b, c, d;
            if (sscanf(line.c_str(), "inventory %d %d %d %d", &a, &b, &c, &d) == 4) {
                pl_.hasMaintenanceKey = a != 0; pl_.hasFuse = b != 0; pl_.leverPulled = c != 0;
                pl_.flashlightOn = d != 0;
            }
        } else if (line.rfind("battery ", 0) == 0) pl_.battery = (float)atof(line.c_str() + 8);
        else if (line.rfind("doc ", 0) == 0) {
            char id[128]; int f;
            if (sscanf(line.c_str(), "doc %127s %d", id, &f) == 2) {
                for (auto& d : world_.documents) if (d.id == id) d.found = f != 0;
            }
        }
    }
    if (length(pl_.pos) < 0.01f) pl_.pos = world_.playerSpawn;
    state_ = GameState::Playing;
    p_->setCursorCaptured(true);
    if (audio_) {
        droneLoop_ = audio_->play(sfx_.drone, 0.28f, 1.0f, Bus::AMBIENT, 0.0f, true);
        ambientLoop_ = audio_->play(sfx_.windOutside, 0.16f, 1.0f, Bus::AMBIENT, 0.0f, true);
    }
    toast("Profile restored.", rgba(200, 200, 190), 3.0f);
    return true;
}

// ------------------------------------------------------------------ fast travel (also used by tests)
bool Game::teleportTo(const std::string& roomId) {
    RoomInfo* r = world_.roomById(roomId);
    if (!r) return false;
    V3 p = r->lightProbe;
    if (length(p) < 0.01f) p = r->bounds.center();
    V3 snapped;
    if (world_.nav.nearestWalkable(p, snapped)) {
        p.x = snapped.x;
        p.y = snapped.y;
        p.z = snapped.z;
    }
    // probes sit at eye height; keep the body on the floor
    float floorZ = groundHeightAt(world_, V3(p.x, p.y, p.z), p.z, 0.2f);
    if (floorZ > -50.0f) p.z = floorZ;
    pl_.pos = V3(p.x, p.y, p.z + pl_.eyeHeight);
    pl_.vel = V3(0, 0, 0);
    currentRoom_ = roomId;
    if (state_ == GameState::Title) state_ = GameState::Playing;
    world_.currentFloor = r->floor == 1 ? 1 : 3;
    return true;
}

// ------------------------------------------------------------------ input injection
void Game::setInput(Input* in) { g_in = in; }

void Game::useWorldBaseState() {
    for (auto& g : world_.groups) g.visible = g.id >= 0;
}

} // namespace dl
