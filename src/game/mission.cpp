#include "game/mission.h"
#include "game/globals.h"
#include <algorithm>
#include <cstdio>

MissionManager g_missions;

static Objective destroy(const std::string& text, const std::string& target, float pct, bool optional = false) {
    Objective o;
    o.kind = OBJ_DESTROY_TARGET;
    o.text = text;
    o.targetNames.push_back(target);
    o.percent = pct;
    o.optional = optional;
    return o;
}
static Objective destroyMany(const std::string& text, const std::vector<std::string>& targets, float pct) {
    Objective o;
    o.kind = OBJ_DESTROY_COUNT;
    o.text = text;
    o.targetNames = targets;
    o.percent = pct;
    o.count = (int)targets.size();
    return o;
}
static Objective reach(const std::string& text, vec3 p, float r) {
    Objective o;
    o.kind = OBJ_REACH;
    o.text = text;
    o.point = p;
    o.radius = r;
    return o;
}
static Objective protect(const std::string& text, const std::string& target, float threshold) {
    Objective o;
    o.kind = OBJ_PROTECT;
    o.text = text;
    o.targetNames.push_back(target);
    o.percent = threshold;
    return o;
}
static Objective collect(const std::string& text, int n) {
    Objective o;
    o.kind = OBJ_COLLECT;
    o.text = text;
    o.count = n;
    return o;
}
static Objective vehicleZone(const std::string& text, vec3 p, float r) {
    Objective o;
    o.kind = OBJ_VEHICLE_ZONE;
    o.text = text;
    o.point = p;
    o.radius = r;
    return o;
}

void MissionManager::init() {
    missions.clear();
    auto add = [&](Mission m) { m.id = (int)missions.size(); missions.push_back(m); };
    // ---------------- Chapter 1: First Contracts ----------------
    {
        Mission m;
        m.chapter = 1;
        m.mapId = 0;
        m.name = "LEARNING CURVE";
        m.brief = "The old convenience store on Westhaven's main street has to go before the new block opens. "
                  "Management wants it open to the street so the crew can clear the lot. Take the sledgehammer and "
                  "make a hole where the customers used to walk. Everything in there is fair game - counters, shelves, "
                  "the walls themselves.";
        m.debrief = "Half the store is rubble and the crew has started hauling. Faster than the estimate, too.";
        m.objectives.push_back(destroy("Demolish the store (50%)", "store", 0.5f));
        m.objectives.push_back(reach("Return to the street outside", vec3(64, 2.2f, 40), 6.0f));
        m.parTime = 180;
        m.unlockHint = "Sledgehammer: left click to swing. Right click for an overhead smash.";
        add(m);
    }
    {
        Mission m;
        m.chapter = 1;
        m.mapId = 0;
        m.name = "OPEN HOUSE";
        m.brief = "A developer bought a row of houses for redevelopment but the demolition crew is two weeks behind. "
                  "Number 6 has to be opened up for a structural survey. Get inside, knock out the downstairs walls, "
                  "and make sure the surveyor has somewhere to stand.";
        m.debrief = "The surveyor walked straight through the front room. He looked almost impressed.";
        m.objectives.push_back(destroy("Open up the first house (40%)", "house0", 0.4f));
        m.objectives.push_back(reach("Stand in the living room", vec3(11.5f, 2.5f, 10.5f), 2.4f));
        m.parTime = 240;
        add(m);
    }
    {
        Mission m;
        m.chapter = 1;
        m.mapId = 0;
        m.name = "YARD WORK";
        m.brief = "The half-finished apartment frame at the construction site is unsafe after last month's storm. "
                  "Bring it down before a worker does it for us. The crane operator will not be calling again.";
        m.debrief = "The frame folded clean into the lot. Scaffolding came with it.";
        m.objectives.push_back(destroy("Demolish the construction frame (55%)", "construction", 0.55f));
        m.parTime = 300;
        m.unlockHint = "The four corner columns carry the frame. Hit them all.";
        add(m);
    }
    {
        Mission m;
        m.chapter = 1;
        m.mapId = 0;
        m.name = "DEMO DERBY";
        m.brief = "The bulldozer at the site will not start for the regular crew, but you know diesel engines. "
                  "Take it for a run through the garage annex on the east side of the district and see how much of "
                  "the structure survives.";
        m.debrief = "Turns out a 14-tonne bulldozer is a very effective demolition tool. Who knew.";
        m.objectives.push_back(vehicleZone("Bring a heavy vehicle to the east garage", vec3(36, 2.2f, 10), 9.0f));
        m.objectives.push_back(destroy("Damage the garage annex (30%)", "house2", 0.3f));
        m.parTime = 260;
        m.unlockHint = "Press E near a vehicle to get in. W/S drive, A/D steer.";
        add(m);
    }
    // ---------------- Chapter 2: Industrial Damage ----------------
    {
        Mission m;
        m.chapter = 2;
        m.mapId = 1;
        m.name = "CHAIN REACTION";
        m.brief = "Blackstone's fuel tank farm has been condemned. The tanks are welded, so cutting them is a waste "
                  "of time. Place charges on the supports and let the fuel do the rest. You will want to be somewhere "
                  "else when it goes.";
        m.debrief = "Four tanks, one detonator, forty metres of scorched concrete. The insurance form was short.";
        m.objectives.push_back(destroy("Destroy the fuel tank farm (60%)", "tanks", 0.6f));
        m.parTime = 260;
        m.unlockHint = "Explosive charges: left click to place, R to detonate everything.";
        add(m);
    }
    {
        Mission m;
        m.chapter = 2;
        m.mapId = 1;
        m.name = "FACTORY FLOOR";
        m.brief = "The machinery hall at Blackstone still has tenants. Clear the production floor to the frame, "
                  "but the site offices on the east side are occupied - no structural damage there. Read the "
                  "layout before you pull a trigger.";
        m.debrief = "The hall is open skies. The office staff never even looked up from their coffee.";
        m.objectives.push_back(destroy("Clear the factory hall (65%)", "factory", 0.65f));
        m.objectives.push_back(protect("Protect the site offices (max 15%)", "offices", 0.15f));
        m.parTime = 320;
        add(m);
    }
    {
        Mission m;
        m.chapter = 2;
        m.mapId = 1;
        m.name = "CRANE DOWN";
        m.brief = "The gantry crane has been listed for scrapping but nobody wants to climb it. Take out the legs "
                  "and let gravity handle the paperwork. Then get clear - the bridge section weighs 40 tonnes.";
        m.debrief = "The crane went down across the yard. Scrap dealers are already on the phone.";
        m.objectives.push_back(destroy("Topple the gantry crane (70%)", "crane", 0.7f));
        m.parTime = 300;
        add(m);
    }
    // ---------------- Chapter 3: The Corporate War ----------------
    {
        Mission m;
        m.chapter = 3;
        m.mapId = 2;
        m.name = "PRECISION ENTRY";
        m.brief = "The north-east tower's top floor is sealed - security owns the stairwell. The owner wants a "
                  "structural survey done from the eighth floor up. There are no doors. Make one.";
        m.debrief = "You reached the eighth floor. The elevator company is going to be furious.";
        m.objectives.push_back(reach("Reach the top floor of the north-east tower", vec3(71, 26.0f, 16), 6.0f));
        m.parTime = 300;
        m.stealth = true;
        m.unlockHint = "Cameras sweep the lobby and the towers are watched by sentry turrets. Shoot them out first.";
        m.brief += " The building is under active security: cameras and sentry turrets. If an alarm is raised the "
                   "floor seals and the survey is off, so take the hardware out before you make your entrance.";
        add(m);
    }
    {
        Mission m;
        m.chapter = 3;
        m.mapId = 2;
        m.name = "CORPORATE SABOTAGE";
        m.brief = "Two towers are competing for the same contract. Both of them lose. Bring the north-west office "
                  "block and the north-east tower down to at least 45% and get out before the street fills with "
                  "litigation.";
        m.debrief = "Two towers, one afternoon. The skyline is not what it was.";
        m.objectives.push_back(destroyMany("Demolish both north towers (45% each)", {"nw_tower", "ne_tower"}, 0.45f));
        m.parTime = 420;
        add(m);
    }
    {
        Mission m;
        m.chapter = 3;
        m.mapId = 2;
        m.name = "TIMED EXTRACTION";
        m.brief = "Three sealed evidence crates are inside the south-west apartments. Legal is stalling, so the "
                  "clocks are ours. Get in, get all three crates out into the street, and do it before the injunction "
                  "lands. Nothing about this needs to be clean.";
        m.debrief = "Three crates, one street corner, two hundred seconds to spare.";
        m.objectives.push_back(collect("Recover all 3 evidence crates", 3));
        m.timed = true;
        m.timeLimit = 200;
        m.parTime = 160;
        m.unlockHint = "E picks up light objects. Crates can also be carried by the pickup bed.";
        add(m);
    }
    // ---------------- Chapter 4: Critical Failure ----------------
    {
        Mission m;
        m.chapter = 4;
        m.mapId = 3;
        m.name = "HARBOR BREAK";
        m.brief = "The cargo ship at pier two is a total loss after the fire, and the owners want it gone before the "
                  "insurers inspect. Break up the deck containers and open the hull. Heavy equipment is on the quay.";
        m.debrief = "The ship is a shipping hazard now instead of a fire hazard. Progress.";
        m.objectives.push_back(destroy("Break up the cargo ship (55%)", "cargo_ship", 0.55f));
        m.parTime = 380;
        add(m);
    }
    {
        Mission m;
        m.chapter = 4;
        m.mapId = 3;
        m.name = "FUEL DEPOT";
        m.brief = "A leaking fuel depot is ten minutes from taking the east quay with it. The fire crew will not go "
                  "in. Ignite it from a distance, then get to the west gate and let it burn out.";
        m.debrief = "The depot burned for four hours. The west gate held.";
        m.objectives.push_back(destroy("Trigger the depot (70%)", "fuel_depot", 0.7f));
        m.objectives.push_back(reach("Escape to the west gate", vec3(6, 2.2f, 22), 8.0f));
        m.timed = true;
        m.timeLimit = 240;
        m.parTime = 200;
        m.unlockHint = "The rocket launcher reaches further than a charge. Fire from cover.";
        add(m);
    }
    // ---------------- Chapter 5: Total Fracture ----------------
    {
        Mission m;
        m.chapter = 5;
        m.mapId = 4;
        m.name = "TOTAL FRACTURE";
        m.brief = "The Red Canyon range has one last demonstration to run: a full bridge collapse with a bunker "
                  "breach. Two reinforced piers hold the bridge. The bunker is rated for artillery. You have "
                  "everything you need and authorisation for airstrikes.";
        m.debrief = "The bridge went down onto an empty ravine and the bunker came apart like wet cardboard. "
                    "The client has already signed for the next site.";
        m.objectives.push_back(destroy("Collapse the canyon bridge (70%)", "canyon_bridge", 0.7f));
        m.objectives.push_back(destroy("Breach the reinforced bunker (55%)", "bunker", 0.55f));
        m.parTime = 480;
        m.unlockHint = "Airstrike designator: mark a target, then get well clear.";
        add(m);
    }
    {
        Mission m;
        m.chapter = 5;
        m.mapId = 5;
        m.name = "GRAND FINALE";
        m.brief = "The Grand Mall is being demolished to make way for a transit hub, but the atrium columns are "
                  "holding three floors of shops in the air. Take the atrium down, drop the cinema block with it, "
                  "and keep the frontage intact for the press photos.";
        m.debrief = "Three floors came down in sequence. The frontage is still standing for the cameras.";
        m.objectives.push_back(destroy("Bring down the atrium (55%)", "mall_atrium", 0.55f));
        m.objectives.push_back(destroy("Demolish the cinema block (60%)", "cinema", 0.6f));
        m.objectives.push_back(protect("Keep the mall frontage intact (max 25%)", "mall_front", 0.25f));
        m.parTime = 500;
        add(m);
    }
    state = MissionState();
}

const Mission& MissionManager::get(int id) const {
    static Mission dummy;
    if (id < 0 || id >= (int)missions.size()) return dummy;
    return missions[id];
}

bool MissionManager::isCompleted(int id) const {
    char key[16];
    snprintf(key, sizeof(key), "m%d", id);
    return std::find(completedMissions.begin(), completedMissions.end(), key) != completedMissions.end();
}

bool MissionManager::start(int missionId) {
    if (missionId < 0 || missionId >= (int)missions.size()) return false;
    const Mission& m = missions[missionId];
    state = MissionState();
    state.missionId = missionId;
    state.active = true;
    state.timed = m.timed;
    state.timeLimit = m.timeLimit;
    state.objectiveProgress.assign(m.objectives.size(), 0.0f);
    return true;
}

const char* MissionManager::chapterName(int chapter) const {
    switch (chapter) {
        case 1: return "CHAPTER 1 - FIRST CONTRACTS";
        case 2: return "CHAPTER 2 - INDUSTRIAL DAMAGE";
        case 3: return "CHAPTER 3 - THE CORPORATE WAR";
        case 4: return "CHAPTER 4 - CRITICAL FAILURE";
        case 5: return "CHAPTER 5 - TOTAL FRACTURE";
        default: return "CHAPTER";
    }
}
int MissionManager::chapterFor(int missionId) const {
    return (missionId >= 0 && missionId < (int)missions.size()) ? missions[missionId].chapter : 1;
}
std::vector<int> MissionManager::missionsForChapter(int chapter) const {
    std::vector<int> out;
    for (auto& m : missions) if (m.chapter == chapter) out.push_back(m.id);
    return out;
}
float MissionManager::totalScore() const {
    return (float)completedMissions.size() / std::max(1, (int)missions.size());
}

void MissionManager::update(float dt, World& world, const MapBuildResult& mapData, const Player& player,
                            const std::vector<Vehicle>& vehicles, int interactEvents) {
    (void)interactEvents;
    if (!state.active || state.completed || state.failed) return;
    Mission& m = missions[state.missionId];
    state.time += dt;
    if (m.stealth && state.alarmTriggered) {
        state.failed = true;
        state.failReason = "Security alarm raised - the floor locked down";
        return;
    }
    if (state.timed) {
        float remaining = state.timeLimit - state.time;
        if (remaining <= 0) {
            state.failed = true;
            state.failReason = "Out of time";
            return;
        }
    }
    auto findTarget = [&](const std::string& name) -> const MapTarget* {
        for (auto& t : mapData.targets) if (t.name == name) return &t;
        return nullptr;
    };
    int completeCount = 0;
    int requiredCount = 0;
    for (size_t i = 0; i < m.objectives.size(); i++) {
        Objective& o = m.objectives[i];
        if (!o.optional) requiredCount++;
        if (o.complete) { completeCount++; continue; }
        if (o.failed) continue;
        switch (o.kind) {
            case OBJ_DESTROY_TARGET: {
                const MapTarget* t = findTarget(o.targetNames[0]);
                if (!t) { o.progress = 0; break; }
                float f = t->destroyedFraction(world);
                o.progress = clampf(f / std::max(0.01f, o.percent), 0, 1);
                char buf[96];
                snprintf(buf, sizeof(buf), "%.0f%% / %.0f%%", f * 100, o.percent * 100);
                o.status = buf;
                if (f >= o.percent) { o.complete = true; }
                break;
            }
            case OBJ_DESTROY_COUNT: {
                int done = 0;
                float sum = 0;
                for (auto& name : o.targetNames) {
                    const MapTarget* t = findTarget(name);
                    if (!t) continue;
                    float f = t->destroyedFraction(world);
                    sum += std::min(1.0f, f / std::max(0.01f, o.percent));
                    if (f >= o.percent) done++;
                }
                o.progress = sum / std::max(1, (int)o.targetNames.size());
                char buf[96];
                snprintf(buf, sizeof(buf), "%d / %d", done, (int)o.targetNames.size());
                o.status = buf;
                if (done >= o.count) o.complete = true;
                break;
            }
            case OBJ_PROTECT: {
                const MapTarget* t = findTarget(o.targetNames[0]);
                if (!t) break;
                float f = t->destroyedFraction(world);
                o.progress = clampf(1.0f - f / std::max(0.01f, o.percent), 0, 1);
                char buf[96];
                snprintf(buf, sizeof(buf), "%.0f%% damage (limit %.0f%%)", f * 100, o.percent * 100);
                o.status = buf;
                if (f > o.percent) {
                    o.failed = true;
                    state.failed = true;
                    state.failReason = "Protected structure destroyed";
                }
                break;
            }
            case OBJ_REACH: {
                float d = (player.pos - o.point).len();
                o.progress = clampf(1.0f - (d - o.radius) / 40.0f, 0, 1);
                char buf[64];
                snprintf(buf, sizeof(buf), "%.0f m", d);
                o.status = buf;
                if (d <= o.radius) o.complete = true;
                break;
            }
            case OBJ_COLLECT: {
                o.progress = clampf((float)state.collected / std::max(1, o.count), 0, 1);
                char buf[48];
                snprintf(buf, sizeof(buf), "%d / %d", state.collected, o.count);
                o.status = buf;
                if (state.collected >= o.count) o.complete = true;
                break;
            }
            case OBJ_VEHICLE_ZONE: {
                bool ok = false;
                for (auto& v : vehicles)
                    if ((v.pos - o.point).len() <= o.radius) { ok = true; break; }
                o.progress = ok ? 1.0f : 0.0f;
                o.status = ok ? "arrived" : "vehicle required";
                if (ok) o.complete = true;
                break;
            }
            default: break;
        }
    }
    if (completeCount >= requiredCount && requiredCount > 0) {
        state.completed = true;
        char key[16];
        snprintf(key, sizeof(key), "m%d", m.id);
        if (!isCompleted(m.id)) completedMissions.push_back(key);
    }
}
