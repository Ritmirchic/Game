// ============================================================================
//  dl_ai.cpp : the three entities, visibility, pathing, elevator travel
// ============================================================================
#include "dl_game.h"

namespace dl {
using namespace nf;

// ------------------------------------------------------------------ LOS
bool Game::lineBlocked(const V3& a, const V3& b) const {
    V3 d = b - a;
    float dist = length(d);
    if (dist < 0.01f) return false;
    int steps = maxi(4, (int)(dist / 0.22f));
    for (int i = 1; i < steps; ++i) {
        V3 p = a + d * ((float)i / steps);
        for (auto& c : world_.colliders) {
            if (!c.blocksSight) continue;
            if (c.box.contains(p)) return true;
        }
    }
    return false;
}

bool Game::isPointVisible(const V3& p, float& outDot) const {
    V3 eye = pl_.pos;
    V3 to = p - eye;
    float dist = length(to);
    V3 dir = dist > 0.001f ? to / dist : V3(1, 0, 0);
    V3 fwd(std::cos(pl_.yaw) * std::cos(pl_.pitch), std::sin(pl_.yaw) * std::cos(pl_.pitch), std::sin(pl_.pitch));
    float dotv = dot(dir, fwd);
    outDot = dotv;
    if (dotv < 0.62f) return false;               // outside a ~52 degree cone
    if (dist > 24.0f) return false;
    return !lineBlocked(eye, p + V3(0, 0, 0.0f));
}

// ------------------------------------------------------------------ spawn / remove
void Game::spawnEntity(EntityType t, const V3& pos) {
    // keep the cast small but never silently re-use another anomaly's entity
    if (ents_.size() >= 4) ents_.erase(ents_.begin());
    Entity e;
    e.type = t;
    e.active = true;
    e.pos = pos;
    e.yaw = std::atan2(pl_.pos.y - pos.y, pl_.pos.x - pos.x) + PI;
    e.state = 1;
    e.animPhase = 0.0f;
    e.repathTimer = 0.0f;
    switch (t) {
        case EntityType::Observer: e.speed = 1.15f; e.scale = 1.0f; break;
        case EntityType::Listener: e.speed = 1.5f; e.scale = 0.92f; break;
        case EntityType::Imitator: e.speed = 1.9f; e.scale = 1.0f; break;
    }
    V3 snapped;
    if (world_.nav.nearestWalkable(pos, snapped)) e.pos = snapped;
    ents_.push_back(e);
    if (audio_) audio_->play3D(sfx_.stingEntity, e.pos, 0.22f, 1.4f, Bus::SFX, false, 12.0f);
    logMsg("[ai] spawned entity type %d at %.1f %.1f %.1f", (int)t, pos.x, pos.y, pos.z);
}

void Game::removeEntity(int id) {
    if (id < 0 || id >= (int)ents_.size()) return;
    if (ents_[id].soundHandle && audio_) audio_->stop(ents_[id].soundHandle, 0.4f);
    ents_[id].active = false;
    if (followEntityId_ == id) followEntityId_ = -1;
}

// ------------------------------------------------------------------ update
void Game::updateEntities(float dt) {
    V3 eye = pl_.pos;
    for (size_t i = 0; i < ents_.size(); ++i) {
        Entity& e = ents_[i];
        if (!e.active) continue;
        e.animPhase += dt * (0.8f + e.speed * 0.8f);
        e.repathTimer -= dt;
        float visDot = 0;
        bool visible = isPointVisible(e.pos + V3(0, 0, 1.2f * e.scale), visDot);
        e.visibleNow = visible;
        float dist = length(e.pos - eye);

        // ---------- Observer: moves only when unobserved
        if (e.type == EntityType::Observer) {
            if (e.state == 5) {   // crossing the room in a straight line, then gone
                V3 dir(std::cos(e.yaw), std::sin(e.yaw), 0);
                e.pos += dir * e.speed * dt;
                e.frozenTime += dt;
                if (e.frozenTime > 2.2f) e.active = false;
                continue;
            }
            if (e.state == 2) {
                // statue pose (anomaly 25)
                e.frozenTime += dt;
                if (!visible && e.frozenTime > 0.8f) e.state = 1;
                if (e.frozenTime > 18.0f) { e.active = false; continue; }
                continue;
            }
            if (visible) {
                e.frozenTime += dt;
                // being stared at is dangerous
                pl_.fear = saturate(pl_.fear + dt * 0.10f);
                if (e.frozenTime > 2.4f) {
                    pl_.lookBackCount += 1.0f;
                    if (audio_) audio_->play3D(sfx_.stingEntity, e.pos, 0.4f, 1.2f);
                    // it is gone when you blink
                    e.active = false;
                    pl_.fear = saturate(pl_.fear + 0.22f);
                    if (pl_.lookBackCount >= 3.0f && ending_ == EndingId::None && mistakes_ < 5) {
                        toast("You have looked at it three times. It knows your face now.", rgba(210, 140, 130), 6.0f);
                        mistakes_ = maxi(mistakes_, 4);
                    }
                    continue;
                }
            } else {
                e.frozenTime = maxf(0.0f, e.frozenTime - dt * 1.5f);
                // path toward the player
                if (e.repathTimer <= 0.0f) {
                    e.repathTimer = 0.65f;
                    std::vector<V3> path;
                    if (world_.nav.findPath(e.pos, eye, path)) {
                        e.path = path;
                        e.pathIdx = 0;
                    }
                }
                if (e.pathIdx < (int)e.path.size()) {
                    V3 wp = e.path[e.pathIdx];
                    V3 d = wp - e.pos;
                    d.z = 0;
                    float l = length(d);
                    if (l < 0.25f) {
                        e.pathIdx++;
                    } else {
                        d = d / l;
                        float sp = e.speed * (1.0f + pl_.fear * 0.35f);
                        V3 np = e.pos + d * sp * dt;
                        e.pos.x = np.x;
                        e.pos.y = np.y;
                        e.yaw = std::atan2(d.y, d.x);
                    }
                }
                // footstep audio, faint
                static float stepAcc = 0.0f;
                stepAcc += dt;
                if (stepAcc > 0.62f && dist < 14.0f) {
                    stepAcc = 0.0f;
                    if (audio_) audio_->play3D(sfx_.footstepsBehind, e.pos, 0.30f, 0.82f, Bus::SFX, false, 12.0f);
                }
            }
            // attack when it has gotten close and is unseen
            if (!visible && dist < 1.35f) {
                pl_.fear = 1.0f;
                pl_.damageFlash = 1.0f;
                pl_.shakeT = 0.7f;
                pl_.shakeAmp = 0.03f;
                if (audio_) audio_->play(sfx_.entityShriek, 0.9f, 0.9f);
                toast("Something touches the back of your neck.", rgba(220, 120, 110), 5.0f);
                e.active = false;
                mistakes_ = mini(5, mistakes_ + 1);
            }
            continue;
        }

        // ---------- Listener: hunts noise
        if (e.type == EntityType::Listener) {
            if (e.state == 2) { e.frozenTime += dt; if (e.frozenTime > 25.0f) e.active = false; continue; }
            if (e.repathTimer <= 0.0f) {
                e.repathTimer = 0.8f;
                std::vector<V3> path;
                V3 goal = e.target;
                if (length(goal) < 0.01f) goal = eye;
                if (world_.nav.findPath(e.pos, goal, path)) { e.path = path; e.pathIdx = 0; }
            }
            if (e.pathIdx < (int)e.path.size()) {
                V3 wp = e.path[e.pathIdx];
                V3 d = wp - e.pos;
                d.z = 0;
                float l = length(d);
                if (l < 0.3f) e.pathIdx++;
                else {
                    d = d / l;
                    float sp = e.speed * (pl_.noise > 0.5f ? 1.5f : 1.0f);
                    e.pos += d * sp * dt;
                    e.yaw = std::atan2(d.y, d.x);
                }
            }
            if (dist < 1.8f) {
                if (audio_) audio_->play(sfx_.entityShriek, 0.85f, 0.75f);
                pl_.damageFlash = 1.0f;
                pl_.fear = 1.0f;
                pl_.shakeT = 0.6f;
                pl_.shakeAmp = 0.025f;
                toast("It heard you. It is very close.", rgba(215, 140, 130), 4.5f);
                e.target = e.pos - (eye - e.pos) * 2.0f;
                e.state = 0;
                e.repathTimer = 0.0f;
                mistakes_ = mini(5, mistakes_ + 1);
            }
            continue;
        }

        // ---------- Imitator: follows and lies
        if (e.type == EntityType::Imitator) {
            if (e.state == 2) { e.frozenTime += dt; if (e.frozenTime > 25.0f) e.active = false; continue; }
            float want = 6.5f;
            if (dist > want + 1.5f) {
                if (e.repathTimer <= 0.0f) {
                    e.repathTimer = 1.0f;
                    std::vector<V3> path;
                    if (world_.nav.findPath(e.pos, eye, path)) { e.path = path; e.pathIdx = 0; }
                }
                if (e.pathIdx < (int)e.path.size()) {
                    V3 wp = e.path[e.pathIdx];
                    V3 d = wp - e.pos;
                    d.z = 0;
                    float l = length(d);
                    if (l < 0.3f) e.pathIdx++;
                    else {
                        d = d / l;
                        e.pos += d * e.speed * dt;
                        e.yaw = std::atan2(d.y, d.x);
                    }
                }
            } else if (dist < want - 2.5f) {
                V3 away = e.pos - eye;
                away.z = 0;
                e.pos += normalize(away) * e.speed * 0.6f * dt;
                e.yaw = std::atan2(-away.y, -away.x);
            }
            // false cues
            static float cueAcc = 0.0f;
            cueAcc += dt;
            if (cueAcc > 7.5f) {
                cueAcc = 0.0f;
                int pick = (int)(hashf((uint32_t)(uiTime_ * 31.0f)) * 3);
                if (audio_) {
                    if (pick == 0) audio_->play3D(sfx_.footstepsBehind, e.pos, 0.5f, 1.05f, Bus::SFX, false, 14.0f);
                    else if (pick == 1) audio_->play3D(sfx_.whisper[2], e.pos, 0.45f, 1.1f, Bus::SFX, false, 12.0f);
                    else audio_->play3D(sfx_.doorCreak, e.pos, 0.4f, 1.2f, Bus::SFX, false, 12.0f);
                }
                pl_.fear = saturate(pl_.fear + 0.05f);
            }
            if (dist < 1.6f && !visible) {
                pl_.damageFlash = 1.0f;
                pl_.fear = 1.0f;
                if (audio_) audio_->play(sfx_.entityShriek, 0.8f, 1.05f);
                toast("It has your voice. It said your name backwards.", rgba(215, 140, 130), 5.0f);
                e.active = false;
                mistakes_ = mini(5, mistakes_ + 1);
            }
            continue;
        }
    }
    // housekeeping
    ents_.erase(std::remove_if(ents_.begin(), ents_.end(), [](const Entity& e) { return !e.active; }), ents_.end());
    for (size_t i = 0; i < ents_.size(); ++i) {
        if (followEntityId_ >= 0 && (int)i == followEntityId_) continue;
    }
}

// ------------------------------------------------------------------ entity mesh
// The three entities are built from material-tagged volumes so each one is drawn with
// its own textures (coat, skin, face plate) instead of a single flat silhouette.
namespace {
struct EBox { float x0, y0, z0, x1, y1, z1; int mat; };
}

void Game::buildEntityMesh(const Entity& e, std::vector<std::pair<int, nf::Mesh>>& out) {
    out.clear();
    float walk = (e.type == EntityType::Observer || e.state == 1) ? std::sin(e.animPhase * 4.0f) : 0.0f;
    float s = e.scale;
    std::vector<EBox> boxes;
    auto B = [&](int m, float x0, float y0, float z0, float x1, float y1, float z1) {
        boxes.push_back({x0 * s, y0 * s, z0 * s, x1 * s, y1 * s, z1 * s, m});
    };
    if (e.type == EntityType::Observer) {
        // tall, thin, elongated: a coat that is too long and a face that is too smooth
        B(MAT_COAT_BLACK, -0.20f, -0.12f, 0.00f, 0.20f, 0.12f, 1.05f);      // lower body
        B(MAT_COAT_BLACK, -0.24f, -0.14f, 1.05f, 0.24f, 0.14f, 1.55f);      // torso
        B(MAT_COAT_BLACK, -0.20f, -0.12f, 1.55f, 0.20f, 0.12f, 1.70f);      // shoulders
        B(MAT_COAT_BLACK, -0.16f, -0.10f, 1.62f, 0.16f, 0.10f, 1.72f);      // collar
        B(MAT_SKIN,       -0.115f, -0.10f, 1.70f, 0.115f, 0.10f, 1.90f);    // head
        B(MAT_FACE,       -0.10f, 0.093f, 1.74f, 0.10f, 0.108f, 1.86f);     // blank face
        B(MAT_HAIR,       -0.12f, -0.11f, 1.86f, 0.12f, 0.11f, 1.94f);      // crown
        for (int side = -1; side <= 1; side += 2) {
            float ax = side * 0.26f;
            float swing = walk * 0.10f * side;
            B(MAT_COAT_BLACK, ax - 0.05f, -0.05f + swing, 1.05f, ax + 0.05f, 0.05f + swing, 1.62f);
            B(MAT_COAT_BLACK, ax - 0.045f, -0.045f - swing, 0.72f, ax + 0.045f, 0.045f - swing, 1.10f);
            B(MAT_SKIN,       ax - 0.04f, -0.04f - swing, 0.58f, ax + 0.04f, 0.04f - swing, 0.74f);
        }
        for (int side = -1; side <= 1; side += 2) {
            float lx = side * 0.10f;
            B(MAT_COAT_BLACK, lx - 0.07f, -0.07f + walk * 0.12f * side, 0.0f,
                              lx + 0.07f, 0.07f + walk * 0.12f * side, 1.06f);
        }
    } else if (e.type == EntityType::Listener) {
        // hunched and broad, with a head that is mostly ear
        B(MAT_COAT_BLACK, -0.30f, -0.20f, 0.00f, 0.30f, 0.20f, 0.55f);
        B(MAT_COAT_BLACK, -0.34f, -0.24f, 0.55f, 0.34f, 0.24f, 1.15f);
        B(MAT_COAT_BLACK, -0.42f, -0.20f, 1.05f, 0.42f, 0.20f, 1.30f);
        B(MAT_SKIN,       -0.20f, -0.16f, 1.26f, 0.20f, 0.16f, 1.58f);
        B(MAT_FACE,       -0.155f, 0.148f, 1.32f, 0.155f, 0.165f, 1.54f);
        B(MAT_HAIR,       -0.21f, -0.17f, 1.54f, 0.21f, 0.17f, 1.62f);
        for (int side = -1; side <= 1; side += 2) {
            float ax = side * 0.40f;
            B(MAT_SKIN, ax - 0.05f, -0.05f, 0.55f, ax + 0.05f, 0.05f, 1.10f);
            for (int f = 0; f < 3; ++f) {
                B(MAT_SKIN, ax - 0.04f + f * 0.03f, -0.03f + walk * 0.05f, 0.34f,
                            ax - 0.01f + f * 0.03f, 0.0f, 0.56f);
            }
        }
    } else {
        // the Imitator wears what you wear; its face is a plate
        B(MAT_COAT_WHITE, -0.18f, -0.11f, 0.00f, 0.18f, 0.11f, 0.88f);
        B(MAT_COAT_WHITE, -0.22f, -0.13f, 0.88f, 0.22f, 0.13f, 1.42f);
        B(MAT_COAT_WHITE, -0.19f, -0.11f, 1.36f, 0.19f, 0.11f, 1.44f);
        B(MAT_SKIN,       -0.10f, -0.09f, 1.42f, 0.10f, 0.09f, 1.64f);
        B(MAT_FACE,       -0.085f, 0.083f, 1.46f, 0.085f, 0.098f, 1.60f);
        B(MAT_HAIR,       -0.105f, -0.095f, 1.60f, 0.105f, 0.095f, 1.67f);
        for (int side = -1; side <= 1; side += 2) {
            float ax = side * 0.24f;
            B(MAT_COAT_WHITE, ax - 0.045f, -0.045f + walk * 0.09f, 0.90f,
                              ax + 0.045f, 0.045f + walk * 0.09f, 1.42f);
            B(MAT_SKIN, ax - 0.038f, -0.038f + walk * 0.09f, 0.78f,
                        ax + 0.038f, 0.038f + walk * 0.09f, 0.92f);
        }
        for (int side = -1; side <= 1; side += 2) {
            float lx = side * 0.09f;
            B(MAT_COAT_WHITE, lx - 0.06f, -0.06f + walk * 0.10f * side, 0.0f,
                              lx + 0.06f, 0.06f + walk * 0.10f * side, 0.90f);
        }
    }
    // bucket the boxes by material and emit one mesh per material
    const int mats[4] = {MAT_COAT_BLACK, MAT_COAT_WHITE, MAT_SKIN, MAT_FACE};
    for (int mi = 0; mi < 4; ++mi) {
        GeoBuilder g;
        g.mat = mats[mi];
        g.uvScale = 0.55f;
        bool any = false;
        for (auto& b : boxes) {
            if (b.mat != mats[mi]) continue;
            g.lbox(b.x0, b.y0, b.z0, b.x1, b.y1, b.z1, true, true);
            any = true;
        }
        if (!any) continue;
        g.finish();
        out.emplace_back(mats[mi], std::move(g.mesh));
    }
    GeoBuilder gh;
    gh.mat = MAT_HAIR;
    gh.uvScale = 0.5f;
    bool hair = false;
    for (auto& b : boxes) {
        if (b.mat != MAT_HAIR) continue;
        gh.lbox(b.x0, b.y0, b.z0, b.x1, b.y1, b.z1, true, true);
        hair = true;
    }
    if (hair) { gh.finish(); out.emplace_back(MAT_HAIR, std::move(gh.mesh)); }
}

// ------------------------------------------------------------------ elevator
void Game::elevatorTravel() {
    elev_.moving = true;
    elev_.t = 0.0f;
    elev_.duration = 3.4f;
    elev_.doorsOpen = false;
    if (Door* d = world_.doorById("elevator_car")) { d->targetOpenness = 0.0f; d->state = DoorState::Closed; }
    if (audio_) {
        audio_->play(sfx_.elevatorDing, 0.6f, 0.85f);
        audio_->play(sfx_.elevatorHum, 0.8f, 0.8f);
    }
    toast("The doors close. The car begins to move down.", rgba(190, 200, 210), 3.0f);
    makeNoise(0.4f, pl_.pos);
}

void Game::finalizeElevatorTravel() {
    // arrive in the basement stairwell landing (the lift has no basement stop in the plans)
    pl_.pos = V3(-5.1f, 10.8f, -3.4f + pl_.eyeHeight);
    pl_.yaw = PI;
    elev_.atBasement = !elev_.atBasement;
    world_.currentFloor = elev_.atBasement ? 1 : 3;
    elev_.doorsOpen = true;
    if (Door* d = world_.doorById("elevator_car")) { d->targetOpenness = 1.0f; d->state = DoorState::Open; }
    if (audio_) audio_->play(sfx_.elevatorDing, 0.7f, 0.8f);
    toast("The doors open somewhere else. The floor indicator reads 1.", rgba(200, 200, 190), 5.0f);
}

} // namespace dl
