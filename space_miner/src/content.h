// ============================================================
// SPACE MINER  -  content.h
// Static game content tables (resources, sectors, upgrades, ...).
// ============================================================
#pragma once
#include "game.h"

namespace sm {

enum AchId {
    ACH_FIRST_MINE = 0, ACH_ROCKBREAKER, ACH_FIRST_BLOOD, ACH_HUNTER, ACH_EXPLORER,
    ACH_FRONTIER, ACH_RICH, ACH_UPGRADER, ACH_MAXED, ACH_FLEET, ACH_CRYSTAL,
    ACH_PLASMA, ACH_VOID, ACH_SHIPYARD, ACH_VOIDCRAFT, ACH_MARATHON, ACH_SURVIVOR,
    ACH_QUANTUM, ACH_SENTINEL_FALLS, ACH_REFINER,
    ACH_COUNT
};
struct AchievementDef {
    const char* name;
    const char* desc;
    int   icon;       // icon id drawn procedurally
    Color color;
};
extern const AchievementDef ACHIEVEMENTS[ACH_COUNT];

// tutorial hints
extern const char* TUTORIAL_HINTS[6];

// formatting helpers used across the UI
std::string fmtBig(long long v);        // 1.2K / 3.4M
std::string resName(int id);
Color resColor(int id);
int resValue(int id);

}  // namespace sm
