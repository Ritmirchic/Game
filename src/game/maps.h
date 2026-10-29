// FRACTURE — procedural map construction and per-map content definitions.
#pragma once
#include "core/engine.h"
#include "world/voxel.h"
#include <string>
#include <vector>

struct MapTarget {
    std::string name;
    std::string label;
    AABB box;
    int baseline = 0;      // solid voxels at build time
    int current(const World& w) const;
    float destroyedFraction(const World& w) const;
};

struct VehSpawn { int kind; vec3 pos; float yaw; };

struct MapBuildResult {
    std::vector<MapTarget> targets;
    std::vector<std::pair<int, vec3>> props;      // prop kind + position
    std::vector<VehSpawn> vehicles;
    std::vector<std::string> hints;
    MapTarget* addTarget(const std::string& name, const std::string& label, const AABB& box);
};

struct GameMap {
    int id = 0;
    MapInfo info;
    const char* shortName = "";
    const char* tagline = "";
    void (*generate)(World& w, MapBuildResult& out) = nullptr;
};

int mapCount();
const GameMap& getMap(int id);
void buildMap(int id, World& outWorld, MapBuildResult& out);

int defaultTimeOfDay(int mapId);
int missionCount();
