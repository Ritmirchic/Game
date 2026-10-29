// FRACTURE — campaign missions with objective detection driven by real world state.
#pragma once
#include "core/engine.h"
#include "game/maps.h"
#include "game/player.h"
#include "game/vehicles.h"
#include <string>
#include <vector>

enum ObjKind {
    OBJ_DESTROY_TARGET = 0,   // destroy >= percent of a named target
    OBJ_REACH,                // reach a point
    OBJ_PROTECT,              // keep a target's destruction below a threshold
    OBJ_COLLECT,              // pick up N items
    OBJ_CARRY,                // bring a carried object into a zone
    OBJ_INTERACT,             // use a switch / console
    OBJ_VEHICLE_ZONE,         // bring a vehicle into a zone
    OBJ_DESTROY_COUNT,        // destroy N of a set of targets
    OBJ_SURVIVE               // stay alive for N seconds
};

struct Objective {
    ObjKind kind = OBJ_DESTROY_TARGET;
    std::string text;
    std::vector<std::string> targetNames;
    vec3 point{0, 0, 0};
    float radius = 3.0f;
    float percent = 0.6f;
    int count = 1;
    bool optional = false;
    bool complete = false;
    bool failed = false;
    float progress = 0;      // 0..1
    std::string status;      // live hint text
};

struct Mission {
    int id = 0;
    int chapter = 1;
    std::string name;
    std::string brief;
    std::string debrief;
    int mapId = 0;
    std::vector<Objective> objectives;
    bool timed = false;
    float timeLimit = 300;
    float parTime = 240;
    std::string unlockHint;
    bool stealth = false;   // raising the alarm fails the mission
};

struct MissionState {
    int missionId = -1;
    bool active = false;
    bool completed = false;
    bool failed = false;
    float time = 0;
    float timeLimit = 0;
    bool timed = false;
    int collected = 0;
    float failTimer = 0;
    std::string failReason;
    std::vector<float> objectiveProgress;
    bool alarmTriggered = false;
};

class MissionManager {
public:
    std::vector<Mission> missions;
    MissionState state;
    std::vector<std::string> completedMissions;  // persisted ids "m3"

    void init();
    const Mission& get(int id) const;
    int count() const { return (int)missions.size(); }
    bool start(int missionId);
    void update(float dt, World& world, const MapBuildResult& mapData, const Player& player,
                const std::vector<Vehicle>& vehicles, int interactEvents);
    void abort() { state.active = false; }
    bool isCompleted(int id) const;
    const char* chapterName(int chapter) const;
    int chapterFor(int missionId) const;
    std::vector<int> missionsForChapter(int chapter) const;
    float totalScore() const;
};

extern MissionManager g_missions;
