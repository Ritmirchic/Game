// ============================================================================
//  dl_anom.cpp : the thirty anomalies -- implementation, staging, reset
// ============================================================================
#include "dl_game.h"

namespace dl {
using namespace nf;

// ------------------------------------------------------------------ registry
static const AnomalyDef kAnoms[] = {
    // ---- visual (1-10)
    {"painting_changes", "The painting has changed",
     "A landscape of birches hung here before. Now the trees stand in water and there is a "
     "figure on the far bank that was not painted.", AnomKind::Visual, "*_living"},
    {"furniture_moves", "The furniture has moved",
     "The armchair sat against the wall. It has turned to face the doorway, and the hollow in "
     "the cushion is still warm.", AnomKind::Visual, "*_living"},
    {"object_disappears", "Something is missing",
     "The clock on the wall is gone. The nail is still there, and a clean rectangle of wallpaper "
     "where it hung.", AnomKind::Visual, "*"},
    {"new_door", "A door that should not exist",
     "A door has appeared in a stretch of wall that has no room behind it. The handle is warm "
     "and it is breathing very slightly with the building.", AnomKind::Visual, "corridor*"},
    {"wallpaper_changes", "The wallpaper is wrong",
     "The pattern has changed from stripes to something older and darker. In places it looks "
     "like it was written on rather than printed.", AnomKind::Visual, "*"},
    {"window_impossible", "The window shows an impossible view",
     "There is no fourth floor on the plans, and yet from the window you can see the building "
     "across the courtyard, and in every window the same face looking back.", AnomKind::Visual,
     "stairwell"},
    {"lighting_changes", "The light is wrong",
     "The lamps have turned a deep red, the colour of a darkroom, and everything casts two "
     "shadows instead of one.", AnomKind::Visual, "*"},
    {"object_wrong_room", "A familiar object in the wrong place",
     "Someone's armchair is standing in the middle of the corridor, facing the wall. It has "
     "been dragged here recently.", AnomKind::Visual, "corridor*"},
    {"ceiling_fixture_moves", "The ceiling fixture has moved",
     "The lamp has crawled a metre along the ceiling. The paint around its old mount is "
     "unbroken, as though it was never there.", AnomKind::Visual, "corridor*"},
    {"object_shape_change", "An object has changed shape",
     "The wardrobe is taller than it was. It reaches the ceiling now, and something inside it "
     "shifts when you listen.", AnomKind::Visual, "*_bed"},

    // ---- audio (11-18)
    {"footsteps_behind", "Footsteps behind you",
     "Three steps, matching your own pace, half a second behind. They stopped when you stopped.",
     AnomKind::Audio, "*"},
    {"whispering", "Whispering from a room",
     "Someone is speaking in the next room, patiently, the way you would speak to a child who "
     "will not go to sleep.", AnomKind::Audio, "*"},
    {"knocking", "Knocking behind a door",
     "Knocking from inside a locked room -- four knocks, then a pause, then four again. It "
     "matches the number of doors in the corridor.", AnomKind::Audio, "*"},
    {"radio_interference", "Radio interference",
     "The radio is receiving a carrier wave with no station. Under the static, a voice counts "
     "backwards from twelve.", AnomKind::Audio, "*_living"},
    {"distant_scream", "A distant scream",
     "A woman's voice, two floors down, cut off mid-breath. The building does not echo it.",
     AnomKind::Audio, "*"},
    {"elevator_sounds", "The lift is moving without you",
     "The lift is running, its cables groaning, and the floor indicator is passing floors that "
     "do not exist.", AnomKind::Audio, "lobby"},
    {"footsteps_above", "Footsteps on another floor",
     "The same slow walk, directly above the ceiling -- in a part of the building that has no "
     "fourth floor.", AnomKind::Audio, "*"},
    {"breathing", "Breathing that is not yours",
     "Something is breathing just behind your shoulder. Not close enough to touch. Close "
     "enough that the air moves.", AnomKind::Audio, "*"},

    // ---- environmental (19-25)
    {"door_locks", "A door locked behind you",
     "The door you just came through will not open. The bolt is on the other side, and it has "
     "been thrown from there.", AnomKind::Environmental, "*"},
    {"corridor_longer", "The corridor is longer than it was",
     "You have walked this corridor a hundred times. There are four more metres of it, and the "
     "far door is exactly as far away as before.", AnomKind::Environmental, "corridor*"},
    {"room_arrangement", "The rooms have rearranged themselves",
     "The kitchen is where the bedroom was. The bed is made, and there is someone's shape "
     "under the blanket.", AnomKind::Environmental, "*"},
    {"elevator_wrong_floor", "The lift arrives at the wrong floor",
     "The doors open on a corridor that is a copy of this one, lit in green, empty, and one "
     "degree colder.", AnomKind::Environmental, "lobby"},
    {"emergency_lights", "The emergency lighting has come on",
     "Every lamp has dropped to a stuttering green emergency glow. That only happens when the "
     "building loses mains power -- and the lifts are still running.", AnomKind::Environmental, "*"},
    {"passage_blocked", "Furniture is blocking the way",
     "A wardrobe has been pushed across the corridor. It is too heavy to move, and there are "
     "scratches on the floor leading to it, not away.", AnomKind::Environmental, "corridor*"},
    {"locked_room_opens", "A locked room is open",
     "The door that was nailed shut is standing open. Behind it the room is clean, recently "
     "swept, and someone has left a chair facing the door.", AnomKind::Environmental, "*"},

    // ---- entity (26-30)
    {"figure_at_end", "A figure at the end of the corridor",
     "Someone is standing at the far end of the corridor with their back to you. They are not "
     "moving. Do not look at them for long.", AnomKind::Entity, "corridor*"},
    {"shadow_crosses", "A shadow crosses the room",
     "A shadow moved across the far wall, low and fast, thrown by nothing.", AnomKind::Entity, "*"},
    {"creature_in_window", "Something is watching through the window",
     "There is a face outside the window -- too high up for a person to stand, too still to be "
     "a reflection.", AnomKind::Entity, "stairwell"},
    {"entity_behind_door", "Something is standing behind the half-open door",
     "In the gap between the door and the frame, a hand is resting on the door's inner face, "
     "holding it open exactly this much.", AnomKind::Entity, "*"},
    {"creature_follows", "Something is following you",
     "It walks when you walk, stops when you stop, and never makes a sound. It is always as far "
     "behind you as it was when you first noticed it.", AnomKind::Entity, "*"},
};

int Game::anomalyCount() { return (int)(sizeof(kAnoms) / sizeof(kAnoms[0])); }
const AnomalyDef& Game::anomalyDef(int i) const {
    static const AnomalyDef dummy{"", "", "", AnomKind::Visual, ""};
    if (i < 0 || i >= anomalyCount()) return dummy;
    return kAnoms[i];
}

// ------------------------------------------------------------------ helpers
static bool roomMatches(const char* pattern, const std::string& room) {
    std::string p(pattern);
    if (p == "*") return true;
    if (!p.empty() && p.back() == '*') {
        std::string pre = p.substr(0, p.size() - 1);
        return room.rfind(pre, 0) == 0;
    }
    if (!p.empty() && p[0] == '*') {
        std::string suf = p.substr(1);
        return room.size() >= suf.size() && room.compare(room.size() - suf.size(), suf.size(), suf) == 0;
    }
    return room == p;
}

int Game::activeAnomalyCount() const {
    int n = 0;
    for (auto& a : anoms_) if (a.active) ++n;
    return n;
}

void Game::resetAnomaly(Anomaly& a) {
    if (!a.active) return;
    applyAnomaly(a.index, false);
    a.active = false;
    a.timer = 0.0f;
    a.stage = 0;
    a.entityId = -1;
    a.handle = 0;
    if (activeAnomIndex_ == a.index) activeAnomIndex_ = -1;
}

// ------------------------------------------------------------------ staging
bool Game::stageAnomalyForRoom(const std::string& roomId, const V3& at) {
    // build the candidate list
    std::vector<int> cand;
    for (int i = 0; i < anomalyCount(); ++i) {
        if (anoms_[i].active) continue;
        bool used = std::find(usedAnomalies_.begin(), usedAnomalies_.end(), i) != usedAnomalies_.end();
        if (used) continue;
        if (!roomMatches(kAnoms[i].roomHint, roomId)) continue;
        cand.push_back(i);
    }
    if (cand.empty()) return false;
    // choose deterministically-but-varied
    uint32_t seed = (uint32_t)(playTime_ * 977.0f) ^ (uint32_t)(at.x * 131.0f) ^ (uint32_t)(at.y * 57.0f);
    int idx = cand[hash32(seed) % cand.size()];
    Anomaly& a = anoms_[idx];
    a.index = idx;
    a.active = true;
    a.room = roomId;
    a.anchor = at;
    a.timer = 0.0f;
    a.stage = 0;
    usedAnomalies_.push_back(idx);
    activeAnomIndex_ = idx;
    applyAnomaly(idx, true);
    logMsg("[anomaly] staged %d (%s) in %s", idx, kAnoms[idx].id, roomId.c_str());
    return true;
}

bool Game::forceAnomaly(int index, bool on) {
    if (index < 0 || index >= anomalyCount()) return false;
    Anomaly& a = anoms_[index];
    if (on) {
        if (a.active) return true;
        a.index = index;
        a.active = true;
        a.anchor = pl_.pos;
        a.room = currentRoom_;
        RoomInfo* r = world_.roomAt(pl_.pos);
        if (r) {
            a.room = r->id;
            a.anchor = r->lightProbe;
        }
        a.timer = 0;
        applyAnomaly(index, true);
        activeAnomIndex_ = index;
    } else {
        resetAnomaly(a);
    }
    return true;
}

// ------------------------------------------------------------------ find helpers
static int findGroupByMatNear(World& w, int mat, const V3& near, float maxDist) {
    int best = -1;
    float bd = maxDist;
    for (auto& g : w.groups) {
        if (g.mat != mat) continue;
        float d = length(g.mesh.bounds.center() - near);
        if (d < bd) { bd = d; best = g.id; }
    }
    return best;
}

static V3 roomLight(World& w, const std::string& roomId) {
    RoomInfo* r = w.roomById(roomId);
    if (r) return r->lightProbe;
    return V3(0, 0, 1.6f);
}

// ------------------------------------------------------------------ apply / reset
void Game::applyAnomaly(int index, bool on) {
    Anomaly& a = anoms_[index];
    const AnomalyDef& def = kAnoms[index];
    (void)def;
    V3 at = a.anchor;
    RoomInfo* room = world_.roomById(a.room);
    if (room && length(at) < 0.01f) at = room->lightProbe;

    switch (index) {
        // ================================================== 0 painting changes
        case 0: {
            if (on) {
                int best = -1; float bd = 1e9f;
                auto isPainting = [](int m) {
                    return m == MAT_PAINTING_LANDSCAPE || m == MAT_PAINTING_ABSTRACT ||
                           m == MAT_PAINTING_PORTRAIT;
                };
                for (auto& g : world_.groups) {
                    if (!isPainting(g.mat)) continue;
                    float d = length(g.mesh.bounds.center() - at);
                    if (d < bd) { bd = d; best = g.id; }
                }
                if (best < 0) {
                    for (auto& g : world_.groups) if (isPainting(g.mat)) { best = g.id; break; }
                }
                if (best >= 0) {
                    a.savedGroup = best;
                    a.savedMat = world_.groups[best].mat;
                    world_.groups[best].mat = MAT_PAINTING_PORTRAIT;
                }
            } else if (a.savedGroup >= 0) {
                world_.groups[a.savedGroup].mat = a.savedMat;
                a.savedGroup = -1;
            }
            break;
        }
        // ================================================== 1 furniture moves
        case 1: {
            if (on) {
                // find a sofa or armchair group near the anchor and swing it to face the door
                int best = -1; float bd = 1e9f;
                for (auto& g : world_.groups) {
                    if (g.mat != MAT_FABRIC_SOFA && g.mat != MAT_VELVET && g.mat != MAT_FABRIC_SOFA) continue;
                    float d = length(g.mesh.bounds.center() - at);
                    if (d < bd) { bd = d; best = g.id; }
                }
                if (best >= 0) {
                    a.savedGroup = best;
                    a.savedXf = world_.groups[best].xf;
                    V3 c = world_.groups[best].mesh.bounds.center();
                    M4 rot = M4::translate(c) * M4::rotZ(1.9f) * M4::translate(-c);
                    M4 mv = M4::translate(V3(1.1f, 0.5f, 0.0f));
                    world_.groups[best].xf = mv * rot;
                }
            } else if (a.savedGroup >= 0) {
                world_.groups[a.savedGroup].xf = a.savedXf;
                a.savedGroup = -1;
            }
            break;
        }
        // ================================================== 2 object disappears
        case 2: {
            if (on) {
                int best = -1; float bd = 1e9f;
                for (auto& g : world_.groups) {
                    if (g.mat != MAT_CLOCK && g.mat != MAT_PAINTING_ABSTRACT && g.mat != MAT_GLASS &&
                        g.mat != MAT_PHOTO && g.mat != MAT_LAMPSHADE && g.mat != MAT_PAINTING_LANDSCAPE)
                        continue;
                    float d = length(g.mesh.bounds.center() - at);
                    if (d < bd) { bd = d; best = g.id; }
                }
                if (best >= 0) {
                    a.savedGroup = best;
                    a.savedVisible = world_.groups[best].visible;
                    world_.groups[best].visible = false;
                }
            } else if (a.savedGroup >= 0) {
                world_.groups[a.savedGroup].visible = a.savedVisible;
                a.savedGroup = -1;
            }
            break;
        }
        // ================================================== 3 new door
        case 3: {
            if (on) {
                if (phantomDoorGroup_ < 0) {
                    int gi = world_.beginGroup(MAT_DOOR_PANEL, "phantom_door");
                    GeoBuilder g;
                    g.mat = MAT_DOOR_PANEL;
                    RoomInfo* r = world_.roomAt(at);
                    V3 p = r ? r->lightProbe : at;
                    // find the nearest wall along the room's x axis and plant a door there
                    V3 d = V3(0, 0, 0);
                    float bestD = 1e9f;
                    for (auto& door : world_.doors) {
                        V3 dd = door.hinge - p;
                        if (dot(V3(dd.x, dd.y, 0), V3(dd.x, dd.y, 0)) < bestD) {
                            bestD = length(dd);
                            d = door.hinge;
                        }
                    }
                    V3 hinge = p + normalize(V3(d.x - p.x, d.y - p.y, 0)) * 3.2f;
                    g.setFrame(hinge, 0);
                    g.lbox(-0.02f, -0.55f, 0.0f, 0.06f, 0.02f, 2.05f);
                    g.mat = MAT_WOOD_DARK;
                    g.lbox(-0.05f, -0.58f, 0.0f, 0.10f, 0.06f, 2.10f);
                    g.lbox(-0.05f, -0.02f, 0.0f, 0.10f, 0.06f, 2.10f);
                    g.lbox(-0.05f, -0.58f, 2.02f, 0.10f, 0.06f, 2.10f);
                    g.clearFrame();
                    g.finish();
                    world_.groups[gi].mesh = std::move(g.mesh);
                    phantomDoorGroup_ = gi;
                }
                world_.setGroupVisible(phantomDoorGroup_, true);
                // a collider so it is a real (if fake) barrier until reported
                world_.addCollider(world_.groups[phantomDoorGroup_].mesh.bounds, phantomDoorGroup_);
                world_.buildNav();
                if (audio_) audio_->play3D(sfx_.doorLock, at, 0.3f, 1.4f);
            } else {
                if (phantomDoorGroup_ >= 0) world_.setGroupVisible(phantomDoorGroup_, false);
                for (auto& c : world_.colliders)
                    if (c.ownerId == phantomDoorGroup_) c.blocksWalk = false;
                world_.buildNav();
            }
            break;
        }
        // ================================================== 4 wallpaper changes
        case 4: {
            if (on) {
                int best = -1; float bd = 1e9f;
                for (auto& g : world_.groups) {
                    if (g.mat != MAT_WALLPAPER_STRIPE && g.mat != MAT_WALLPAPER_DAMASK && g.mat != MAT_WALLPAPER_FLORAL)
                        continue;
                    float d = length(g.mesh.bounds.center() - at);
                    if (d < bd) { bd = d; best = g.id; }
                }
                if (best >= 0) {
                    a.savedGroup = best;
                    a.savedMat = world_.groups[best].mat;
                    world_.groups[best].mat = MAT_PLASTER_BARE;
                }
            } else if (a.savedGroup >= 0) {
                world_.groups[a.savedGroup].mat = a.savedMat;
                a.savedGroup = -1;
            }
            break;
        }
        // ================================================== 5 window impossible
        case 5: {
            if (on) {
                if (windowViewGroup_ < 0) {
                    int gi = world_.beginGroup(MAT_TV_STATIC, "impossible_view");
                    GeoBuilder g;
                    g.mat = MAT_TV_STATIC;
                    g.uvScale = 4.0f;
                    // a wide plane beyond the stairwell window
                    RoomInfo* r = world_.roomById("stairwell");
                    V3 c = r ? r->lightProbe : at;
                    V3 plane = V3(-10.5f, 9.2f, 1.9f);
                    g.plane(plane, V3(0, 1, 0), V3(0, 0, 1), 14.0f, 9.0f, 3.0f);
                    g.finish();
                    world_.groups[gi].mesh = std::move(g.mesh);
                    windowViewGroup_ = gi;
                    (void)c;
                }
                world_.setGroupVisible(windowViewGroup_, true);
                for (auto& f : world_.fixtures) if (f.roomId == "stairwell") f.color = V3(0.35f, 0.55f, 0.75f);
                if (audio_) audio_->play3D(sfx_.windOutside, at, 0.5f, 0.8f, Bus::AMBIENT, false, 20.0f);
            } else {
                if (windowViewGroup_ >= 0) world_.setGroupVisible(windowViewGroup_, false);
                for (auto& f : world_.fixtures) if (f.roomId == "stairwell") f.color = V3(0.92f, 0.9f, 0.8f);
            }
            break;
        }
        // ================================================== 6 lighting changes
        case 6: {
            for (auto& f : world_.fixtures) {
                if (a.room.empty() || f.roomId == a.room || (a.room.find("corridor") == 0 && f.roomId.find("corridor") == 0)) {
                    if (on) {
                        if (a.savedIntensity == 0.0f) {
                            a.savedIntensity = f.baseIntensity;
                            a.savedColor = f.color;
                        }
                        f.color = V3(0.95f, 0.22f, 0.18f);
                        f.baseIntensity = nf::maxf(f.baseIntensity, 0.9f);
                    } else if (a.savedIntensity > 0.0f) {
                        f.color = a.savedColor;
                        f.baseIntensity = a.savedIntensity;
                    }
                }
            }
            if (!on) { a.savedIntensity = 0.0f; }
            break;
        }
        // ================================================== 7 object in wrong room
        case 7: {
            if (on) {
                int best = -1; float bd = 1e9f;
                for (auto& g : world_.groups) {
                    if (g.mat != MAT_FABRIC_SOFA && g.mat != MAT_VELVET && g.mat != MAT_METAL_RUST &&
                        g.mat != MAT_WOOD_DARK) continue;
                    float d = length(g.mesh.bounds.center() - at);
                    if (d < 1.5f) continue;
                    if (d < bd) { bd = d; best = g.id; }
                }
                if (best >= 0) {
                    a.savedGroup = best;
                    a.savedXf = world_.groups[best].xf;
                    V3 c = world_.groups[best].mesh.bounds.center();
                    V3 delta = at - c;
                    delta.z = 0;
                    if (length(delta) > 6.0f) delta = normalize(delta) * 6.0f;
                    world_.groups[best].xf = M4::translate(delta) * M4::rotZ(0.8f);
                }
            } else if (a.savedGroup >= 0) {
                world_.groups[a.savedGroup].xf = a.savedXf;
                a.savedGroup = -1;
            }
            break;
        }
        // ================================================== 8 ceiling fixture moves
        case 8: {
            if (on) {
                int best = -1; float bd = 1e9f;
                for (auto& g : world_.groups) {
                    if (g.mat != MAT_LIGHTBASE) continue;
                    float d = length(g.mesh.bounds.center() - at);
                    if (d < bd) { bd = d; best = g.id; }
                }
                if (best >= 0) {
                    a.savedGroup = best;
                    a.savedXf = world_.groups[best].xf;
                    world_.groups[best].xf = M4::translate(V3(1.4f, 0.7f, 0.0f));
                }
                // move the matching light too
                float bd2 = 1e9f; int bi = -1;
                for (size_t i = 0; i < world_.fixtures.size(); ++i) {
                    float d = length(world_.fixtures[i].pos - at);
                    if (d < bd2) { bd2 = d; bi = (int)i; }
                }
                if (bi >= 0) {
                    a.stage = bi;
                    a.savedColor = world_.fixtures[bi].pos;
                    world_.fixtures[bi].pos += V3(1.4f, 0.7f, 0.0f);
                }
            } else {
                if (a.savedGroup >= 0) { world_.groups[a.savedGroup].xf = a.savedXf; a.savedGroup = -1; }
                if (a.stage >= 0 && a.stage < (int)world_.fixtures.size()) world_.fixtures[a.stage].pos = a.savedColor;
                a.stage = 0;
            }
            break;
        }
        // ================================================== 9 object changes shape
        case 9: {
            if (on) {
                int best = -1; float bd = 1e9f;
                for (auto& g : world_.groups) {
                    if (g.mat != MAT_WOOD_DARK && g.mat != MAT_PANEL_WOOD && g.mat != MAT_WOOD_LIGHT) continue;
                    V3 sz = g.mesh.bounds.size();
                    if (sz.z < 1.4f) continue;
                    float d = length(g.mesh.bounds.center() - at);
                    if (d < bd) { bd = d; best = g.id; }
                }
                if (best >= 0) {
                    a.savedGroup = best;
                    a.savedXf = world_.groups[best].xf;
                    V3 lo = world_.groups[best].mesh.bounds.lo;
                    world_.groups[best].xf = M4::translate(V3(0, 0, lo.z * 0.0f)) * M4::translate(lo) *
                                              M4::scale(V3(1.0f, 1.0f, 1.45f)) * M4::translate(-lo);
                }
            } else if (a.savedGroup >= 0) {
                world_.groups[a.savedGroup].xf = a.savedXf;
                a.savedGroup = -1;
            }
            break;
        }
        // ================================================== 10 footsteps behind
        case 10: {
            if (on) {
                a.stage = 1;
                a.timer = 0.0f;
            } else if (audio_ && a.handle) {
                audio_->stop(a.handle, 0.2f);
                a.handle = 0;
            }
            break;
        }
        // ================================================== 11 whispering
        case 11: {
            if (on) {
                V3 src = at + V3(3.0f, 2.0f, 0.0f);
                if (audio_) a.handle = audio_->play3D(sfx_.whisper[1], src, 0.55f, 1.0f, Bus::SFX, true, 9.0f);
                a.anchor = src;
            } else if (audio_ && a.handle) {
                audio_->stop(a.handle, 0.4f);
                a.handle = 0;
            }
            break;
        }
        // ================================================== 12 knocking
        case 12: {
            if (on) {
                V3 src = at + V3(-2.5f, 1.0f, 0.4f);
                if (audio_) a.handle = audio_->play3D(sfx_.knock[0], src, 0.7f, 1.0f, Bus::SFX, true, 14.0f);
                a.anchor = src;
                a.timer = 0.0f;
            } else if (audio_ && a.handle) {
                audio_->stop(a.handle, 0.3f);
                a.handle = 0;
            }
            break;
        }
        // ================================================== 13 radio interference
        case 13: {
            if (on) {
                int gi = -1;
                for (auto& g : world_.groups) if (g.mat == MAT_RADIO) gi = g.id;
                if (gi >= 0) {
                    a.savedGroup = gi;
                    a.savedMat = world_.groups[gi].mat;
                    world_.groups[gi].mat = MAT_TV_STATIC;
                }
                if (audio_) a.handle = audio_->play3D(sfx_.radioStatic, at, 0.5f, 1.0f, Bus::SFX, true, 8.0f);
            } else {
                if (a.savedGroup >= 0) { world_.groups[a.savedGroup].mat = a.savedMat; a.savedGroup = -1; }
                if (audio_ && a.handle) { audio_->stop(a.handle, 0.3f); a.handle = 0; }
            }
            break;
        }
        // ================================================== 14 distant scream
        case 14: {
            if (on) {
                V3 src = at + V3(0, 0, -3.4f);
                if (audio_) a.handle = audio_->play3D(sfx_.scream, src, 0.55f, 0.95f, Bus::SFX, false, 30.0f);
                a.timer = 0.0f;
            } else if (audio_ && a.handle) {
                audio_->stop(a.handle, 0.2f);
                a.handle = 0;
            }
            break;
        }
        // ================================================== 15 elevator sounds
        case 15: {
            if (on) {
                V3 src = V3(5.8f, 9.35f, 1.4f);
                if (audio_) a.handle = audio_->play3D(sfx_.elevatorHum, src, 0.65f, 1.0f, Bus::SFX, true, 16.0f);
                for (auto& f : world_.fixtures) if (f.roomId == "elevator") f.on = true;
            } else if (audio_ && a.handle) {
                audio_->stop(a.handle, 0.6f);
                a.handle = 0;
            }
            break;
        }
        // ================================================== 16 footsteps above
        case 16: {
            if (on) {
                a.timer = 0.0f;
                a.stage = 1;
                V3 src = V3(pl_.pos.x, pl_.pos.y, pl_.pos.z + 2.4f);
                a.anchor = src;
            }
            break;
        }
        // ================================================== 17 breathing
        case 17: {
            if (on) {
                a.timer = 0.0f;
                a.stage = 1;
            }
            break;
        }
        // ================================================== 18 door locks
        case 18: {
            if (on) {
                // lock the door nearest to the player (never the fire exit)
                int best = -1; float bd = 6.0f;
                for (auto& d : world_.doors) {
                    if (d.opensWith == "maintenance_key") continue;
                    float dd = length(d.hinge - pl_.pos);
                    if (dd < bd && dd > 0.5f) { bd = dd; best = d.index; }
                }
                if (best >= 0) {
                    a.stage = best;
                    world_.doors[best].state = DoorState::Locked;
                    world_.doors[best].targetOpenness = 0.0f;
                    if (audio_) audio_->play3D(sfx_.doorLock, world_.doors[best].hinge, 0.7f, 1.0f);
                }
            } else if (a.stage > 0 && a.stage < (int)world_.doors.size()) {
                world_.doors[a.stage].state = DoorState::Closed;
                a.stage = 0;
            }
            break;
        }
        // ================================================== 19 corridor longer
        case 19: {
            if (on) {
                if (a.savedGroup < 0) {
                    int gi = world_.beginGroup(MAT_WALLPAPER_STRIPE, "extended_corridor");
                    GeoBuilder g;
                    g.mat = MAT_WALLPAPER_STRIPE;
                    g.uvScale = 1.35f;
                    // extend past the north corridor's end wall
                    g.box(Box3(V3(-1.1f, 7.6f, 0.0f), V3(1.1f, 11.6f, 0.16f)), true, false);
                    g.box(Box3(V3(-1.1f, 7.6f, 2.69f), V3(1.1f, 11.6f, 2.85f)), false, true);
                    g.box(Box3(V3(-1.2f, 7.6f, 0.0f), V3(-1.0f, 11.6f, 2.85f)), true, true);
                    g.box(Box3(V3(1.0f, 7.6f, 0.0f), V3(1.2f, 11.6f, 2.85f)), true, true);
                    g.box(Box3(V3(-1.2f, 11.4f, 0.0f), V3(1.2f, 11.6f, 2.85f)), true, true);
                    g.finish();
                    world_.groups[gi].mesh = std::move(g.mesh);
                    a.savedGroup = gi;
                    world_.setGroupVisible(gi, true);
                    world_.addCollider(world_.groups[gi].mesh.bounds, gi);
                }
                world_.setGroupVisible(a.savedGroup, true);
                // open the existing end wall by hiding one wall group
                for (auto& g : world_.groups) {
                    if (g.mat != MAT_WALLPAPER_STRIPE && g.mat != MAT_PAINT_GREEN) continue;
                    V3 lo = g.mesh.bounds.lo, hi = g.mesh.bounds.hi;
                    if (hi.y > 7.4f && hi.y < 7.9f && lo.x < -0.5f && hi.x > 0.5f && hi.z < 3.0f && (hi.z - lo.z) > 2.0f) {
                        if (g.id != a.savedGroup && a.savedMat != g.id + 1000) {
                            a.savedMat = g.id + 1000;
                            g.visible = false;
                            for (auto& c : world_.colliders) if (c.ownerId == g.id) c.blocksWalk = false;
                        }
                    }
                }
                world_.buildNav();
            } else {
                if (a.savedGroup >= 0) world_.setGroupVisible(a.savedGroup, false);
                if (a.savedMat > 1000) {
                    int gid = a.savedMat - 1000;
                    world_.setGroupVisible(gid, true);
                    for (auto& c : world_.colliders) if (c.ownerId == gid) c.blocksWalk = true;
                }
                a.savedMat = -1;
                world_.buildNav();
            }
            break;
        }
        // ================================================== 20 room arrangement
        case 20: {
            if (on) {
                // drag a bed into the corridor and swap the living room's sofa in
                int bedG = -1, sofaG = -1;
                for (auto& g : world_.groups) {
                    if (g.mat == MAT_MATTRESS) bedG = g.id;
                    if (g.mat == MAT_FABRIC_SOFA && sofaG < 0) sofaG = g.id;
                }
                if (bedG >= 0) {
                    a.savedGroup = bedG;
                    a.savedXf = world_.groups[bedG].xf;
                    V3 c = world_.groups[bedG].mesh.bounds.center();
                    V3 target = at + V3(0, 0, 0);
                    V3 delta = target - c;
                    delta.z = 0;
                    if (length(delta) > 4.0f) delta = normalize(delta) * 4.0f;
                    world_.groups[bedG].xf = M4::translate(delta) * M4::rotZ(1.5708f);
                }
                if (sofaG >= 0) {
                    a.stage = sofaG;
                    a.savedColor = world_.groups[sofaG].mesh.bounds.center();
                    V3 c = world_.groups[sofaG].mesh.bounds.center();
                    world_.groups[sofaG].xf = M4::translate(V3(at.x - c.x, at.y - c.y, 0)) * M4::rotZ(-1.2f);
                }
            } else {
                if (a.savedGroup >= 0) world_.groups[a.savedGroup].xf = a.savedXf;
                if (a.stage > 0 && a.stage < (int)world_.groups.size()) world_.groups[a.stage].xf = M4();
                a.savedGroup = -1;
                a.stage = 0;
            }
            break;
        }
        // ================================================== 21 elevator wrong floor
        case 21: {
            if (on) {
                a.stage = 1;
                if (audio_) a.handle = audio_->play3D(sfx_.rumble, V3(5.8f, 9.35f, 1.0f), 0.6f, 0.8f, Bus::SFX, true, 14.0f);
                for (auto& f : world_.fixtures) {
                    if (f.roomId == "elevator" || f.roomId == "lobby") {
                        if (a.savedIntensity == 0.0f) { a.savedIntensity = f.baseIntensity; }
                        f.color = V3(0.25f, 0.95f, 0.4f);
                    }
                }
            } else {
                if (audio_ && a.handle) { audio_->stop(a.handle, 0.5f); a.handle = 0; }
                for (auto& f : world_.fixtures) if (f.roomId == "elevator" || f.roomId == "lobby") {
                    f.color = V3(0.95f, 0.86f, 0.68f);
                    f.baseIntensity = a.savedIntensity > 0 ? a.savedIntensity : f.baseIntensity;
                }
                a.savedIntensity = 0;
                a.stage = 0;
            }
            break;
        }
        // ================================================== 22 emergency lights
        case 22: {
            for (auto& f : world_.fixtures) {
                if (on) {
                    if (f.baseIntensity > 0.0f && !f.emergency && a.savedIntensity == 0.0f) a.savedIntensity = 1.0f;
                    if (f.baseIntensity > 0.0f) {
                        f.color = V3(0.25f, 0.95f, 0.45f);
                        f.baseIntensity = maxf(0.35f, f.baseIntensity * 0.6f);
                        f.flicker = 0.5f;
                    }
                } else {
                    f.color = f.emergency ? V3(0.55f, 1.0f, 0.62f) : V3(1.0f, 0.86f, 0.66f);
                    if (f.roomId.find("maintenance") != std::string::npos) f.color = V3(0.85f, 0.9f, 1.0f);
                    f.flicker = 0.0f;
                }
            }
            if (!on) a.savedIntensity = 0.0f;
            break;
        }
        // ================================================== 23 passage blocked
        case 23: {
            if (on) {
                if (a.savedGroup < 0) {
                    int gi = world_.beginGroup(MAT_PANEL_WOOD, "blocking_wardrobe");
                    GeoBuilder g;
                    g.mat = MAT_PANEL_WOOD;
                    g.uvScale = 1.0f;
                    RoomInfo* r = world_.roomAt(at);
                    V3 p = r ? r->lightProbe : at;
                    // block the nearest corridor pinch point
                    V3 c = V3(0, 1.1f, 1.4f);
                    if (room && room->id.find("corridor") != std::string::npos) c = V3(room->bounds.center().x, 1.1f, 1.4f);
                    g.setFrame(c, p.y > 1.1f ? 0.0f : 0.0f);
                    g.box(Box3(V3(-0.55f, -0.18f, 0.0f), V3(0.55f, 0.18f, 2.1f)), true, true);
                    g.clearFrame();
                    g.finish();
                    world_.groups[gi].mesh = std::move(g.mesh);
                    a.savedGroup = gi;
                }
                world_.setGroupVisible(a.savedGroup, true);
                world_.addCollider(world_.groups[a.savedGroup].mesh.bounds, a.savedGroup);
                world_.buildNav();
                if (audio_) audio_->play3D(sfx_.metalDrag, at, 0.6f, 0.8f);
            } else {
                if (a.savedGroup >= 0) world_.setGroupVisible(a.savedGroup, false);
                for (auto& c : world_.colliders) if (a.savedGroup >= 0 && c.ownerId == a.savedGroup) c.blocksWalk = false;
                world_.buildNav();
            }
            break;
        }
        // ================================================== 24 locked room opens
        case 24: {
            if (on) {
                int best = -1;
                for (auto& d : world_.doors) {
                    if (d.state == DoorState::Locked) { best = d.index; break; }
                }
                if (best < 0) {
                    // otherwise crack open the farthest interior door
                    float bd = 0;
                    for (auto& d : world_.doors) {
                        float dd = length(d.hinge - pl_.pos);
                        if (dd > bd && d.opensWith.empty()) { bd = dd; best = d.index; }
                    }
                }
                if (best >= 0) {
                    a.stage = best;
                    a.savedIntensity = (float)world_.doors[best].state;
                    world_.doors[best].state = DoorState::Open;
                    world_.doors[best].targetOpenness = 1.0f;
                }
            } else if (a.stage > 0 && a.stage < (int)world_.doors.size()) {
                world_.doors[a.stage].state = (DoorState)(int)a.savedIntensity;
                world_.doors[a.stage].targetOpenness = 0.0f;
                a.stage = 0;
            }
            break;
        }
        // ================================================== 25 figure at corridor end
        case 25: {
            if (on) {
                V3 p = at;
                RoomInfo* r = world_.roomAt(at);
                if (r) {
                    V3 c = r->bounds.center();
                    V3 d = c - pl_.pos;
                    d.z = 0;
                    float dist = length(d);
                    if (dist < 0.1f) d = V3(1, 0, 0);
                    p = pl_.pos + normalize(d) * minf(8.5f, maxf(5.0f, dist * 0.7f));
                }
                spawnEntity(EntityType::Observer, p);
                if (!ents_.empty()) {
                    ents_.back().state = 2;   // statue
                    ents_.back().yaw = std::atan2(pl_.pos.y - p.y, pl_.pos.x - p.x) + PI;
                    a.entityId = (int)ents_.size() - 1;
                }
            } else if (a.entityId >= 0) {
                removeEntity(a.entityId);
                a.entityId = -1;
            }
            break;
        }
        // ================================================== 26 shadow crosses
        case 26: {
            if (on) {
                if (shadowBlobGroup_ < 0) {
                    int gi = world_.beginGroup(MAT_SHADOW, "crossing_shadow");
                    GeoBuilder g;
                    g.mat = MAT_SHADOW;
                    g.uvScale = 1.0f;
                    RoomInfo* r = world_.roomAt(at);
                    V3 c = r ? r->lightProbe : at;
                    V3 wallDir = V3(std::cos(pl_.yaw + HALF_PI), std::sin(pl_.yaw + HALF_PI), 0);
                    g.plane(c + wallDir * 1.8f + V3(0, 0, 0.2f), wallDir, V3(0, 0, 1), 2.4f, 2.4f, 1.0f);
                    g.finish();
                    world_.groups[gi].mesh = std::move(g.mesh);
                    shadowBlobGroup_ = gi;
                }
                world_.setGroupVisible(shadowBlobGroup_, true);
                a.stage = 1;
                a.timer = 0.0f;
                a.anchor = at;
                // and something crosses the room, low and fast
                V3 side(std::cos(pl_.yaw + HALF_PI), std::sin(pl_.yaw + HALF_PI), 0);
                V3 p = at - side * 4.0f;
                spawnEntity(EntityType::Observer, p);
                if (!ents_.empty()) {
                    ents_.back().state = 5;          // crossing
                    ents_.back().yaw = std::atan2(side.y, side.x);
                    ents_.back().speed = 3.2f;
                    a.entityId = (int)ents_.size() - 1;
                }
            } else {
                if (shadowBlobGroup_ >= 0) world_.setGroupVisible(shadowBlobGroup_, false);
                if (a.entityId >= 0) { removeEntity(a.entityId); a.entityId = -1; }
            }
            break;
        }
        // ================================================== 27 creature in window
        case 27: {
            if (on) {
                V3 p(-8.3f, 9.2f, 1.9f);
                spawnEntity(EntityType::Listener, p);
                if (!ents_.empty()) {
                    ents_.back().state = 2;
                    ents_.back().speed = 0.0f;
                    a.entityId = (int)ents_.size() - 1;
                }
            } else if (a.entityId >= 0) {
                removeEntity(a.entityId);
                a.entityId = -1;
            }
            break;
        }
        // ================================================== 28 entity behind door
        case 28: {
            if (on) {
                int best = -1; float bd = 8.0f;
                for (auto& d : world_.doors) {
                    float dd = length(d.hinge - pl_.pos);
                    if (dd < bd) { bd = dd; best = d.index; }
                }
                if (best >= 0) {
                    Door& d = world_.doors[best];
                    d.targetOpenness = 0.35f;
                    V3 p = d.hinge + d.axis * (d.width * 0.5f) + d.normal * (-0.55f);
                    spawnEntity(EntityType::Imitator, p);
                    if (!ents_.empty()) {
                        ents_.back().state = 2;
                        ents_.back().yaw = std::atan2(d.normal.y, d.normal.x);
                        a.entityId = (int)ents_.size() - 1;
                    }
                }
            } else if (a.entityId >= 0) {
                removeEntity(a.entityId);
                a.entityId = -1;
            }
            break;
        }
        // ================================================== 29 creature follows
        case 29: {
            if (on) {
                V3 p = pl_.pos - V3(std::cos(pl_.yaw), std::sin(pl_.yaw), 0) * 7.0f;
                spawnEntity(EntityType::Imitator, p);
                if (!ents_.empty()) {
                    ents_.back().state = 4;   // stalk
                    followEntityId_ = (int)ents_.size() - 1;
                    a.entityId = followEntityId_;
                }
            } else if (a.entityId >= 0) {
                removeEntity(a.entityId);
                a.entityId = -1;
                followEntityId_ = -1;
            }
            break;
        }
        default: break;
    }
}

// ------------------------------------------------------------------ per-frame anomaly updates
void Game::updateAnomalies(float dt) {
    for (auto& a : anoms_) {
        if (!a.active) continue;
        a.timer += dt;
        switch (a.index) {
            case 10: {   // footsteps behind
                if (a.stage == 0) break;
                static float acc = 0.0f;
                acc += dt;
                if (acc > 0.52f) {
                    acc = 0.0f;
                    V3 back = pl_.pos - V3(std::cos(pl_.yaw), std::sin(pl_.yaw), 0) * 2.4f;
                    if (audio_) audio_->play3D(sfx_.footstepsBehind, back, 0.55f, 1.0f, Bus::SFX, false, 7.0f);
                }
                break;
            }
            case 12: {   // knocking: retrigger every few seconds
                if (a.timer > 6.0f) {
                    a.timer = 0.0f;
                    if (audio_) audio_->play3D(sfx_.knock[(int)(hashf((uint32_t)(uiTime_ * 7.0f)) * 3) % 3], a.anchor, 0.6f, 1.0f, Bus::SFX, false, 14.0f);
                }
                break;
            }
            case 14: {
                if (a.timer > 9.0f) { a.timer = 0.0f; if (audio_) audio_->play3D(sfx_.scream, a.anchor + V3(0, 0, -3.0f), 0.5f, 0.9f, Bus::SFX, false, 30.0f); }
                break;
            }
            case 16: {   // footsteps above: positional bursts on the ceiling
                static float acc2 = 0.0f;
                acc2 += dt;
                if (acc2 > 0.62f) {
                    acc2 = 0.0f;
                    V3 above(pl_.pos.x + std::cos(uiTime_ * 0.4f) * 2.0f, pl_.pos.y + std::sin(uiTime_ * 0.4f) * 2.0f,
                             pl_.pos.z + 2.3f);
                    if (audio_) audio_->play3D(sfx_.footstepsBehind, above, 0.6f, 0.85f, Bus::SFX, false, 9.0f);
                }
                break;
            }
            case 17: {   // breathing behind the shoulder
                static float acc3 = 0.0f;
                acc3 += dt;
                if (acc3 > 3.4f) {
                    acc3 = 0.0f;
                    V3 back = pl_.pos - V3(std::cos(pl_.yaw), std::sin(pl_.yaw), 0) * 0.8f + V3(0, 0, 0.2f);
                    if (audio_) audio_->play3D(sfx_.entityBreath, back, 0.45f, 1.0f, Bus::SFX, false, 4.0f);
                    pl_.fear = saturate(pl_.fear + 0.06f);
                }
                break;
            }
            case 26: {   // shadow slides across the wall
                if (shadowBlobGroup_ >= 0) {
                    M4& xf = world_.groups[shadowBlobGroup_].xf;
                    float t = a.timer * 0.8f;
                    xf = M4::translate(V3(std::sin(t) * 1.6f, std::cos(t * 0.7f) * 0.4f, 0));
                }
                break;
            }
            case 6: {    // flickering red light
                for (auto& f : world_.fixtures) {
                    if (f.color.x > 0.5f && f.color.y < 0.4f) {
                        float fl = 0.55f + 0.45f * std::sin(uiTime_ * 9.0f + f.phase);
                        f.flicker = fl;
                    }
                }
                break;
            }
            case 22: {
                static float acc4 = 0.0f;
                acc4 += dt;
                if (acc4 > 4.0f) {
                    acc4 = 0.0f;
                    if (audio_) audio_->play3D(sfx_.lightBuzz, pl_.pos, 0.25f, 1.0f, Bus::SFX, false, 8.0f);
                }
                break;
            }
            case 11: {   // whispering swells when close
                if (audio_ && a.handle) {
                    float d = length(a.anchor - pl_.pos);
                    audio_->setHandleVolume(a.handle, clampf(1.2f - d / 9.0f, 0.0f, 0.8f));
                }
                break;
            }
            default: break;
        }
    }
    // reported anomalies reset after a short correction beat
}

} // namespace dl
