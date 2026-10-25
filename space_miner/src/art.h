// ============================================================
// SPACE MINER  -  art.h
// Asset bank: every sprite in the game is generated at runtime
// from parametric definitions (original art, no external files).
// ============================================================
#pragma once
#include "game.h"
#include "content.h"
#include "texture.h"

namespace sm {

struct EnginePort {
    Vec2 pos;        // in sprite space (pixels, origin at canvas centre)
    float angle = 0;
    float size = 8;
    Color color = rgba(120, 210, 255);
};

struct ShipArt {
    Canvas body;
    std::vector<EnginePort> engines;
    std::vector<Vec2> hardpoints;
    Vec2 turretPos;
    float spriteRadius = 64.f;   // world/px reference
    Canvas turret;               // separable mining emitter head
};

struct AssetBank {
    std::vector<ShipArt> ships;
    std::vector<Canvas> enemies;          // EN_TYPE_COUNT
    std::vector<Canvas> enemyGlow;        // additive layer per enemy
    std::vector<Canvas> drones;           // 3 tiers
    Canvas station;
    Canvas stationGlow;
    std::vector<Canvas> planets;
    std::vector<Canvas> planetsGlow;
    std::vector<Canvas> debris;
    std::vector<Canvas> rocks[5];         // per sector variants
    std::vector<int>     rockRes[5];      // resource id of each variant
    std::vector<Canvas> nebula[5];        // 2 layers per sector
    std::vector<Canvas> stars;            // 5 starfields
    std::vector<Canvas> resIcon;
    std::vector<Canvas> upgIcon;
    std::vector<Canvas> achIcon;
    Canvas glowSoft, glowSharp, smoke, spark, halo, ringSprite;
    Canvas cursor, panelCorner, logoMark;
    void init(int stage);
    float buildProgress = 0.f;
};

// individual builders (also used by the headless preview tool)
void buildShipArt(ShipArt& out, const ShipDef& def, int index);
void buildEnemyArt(Canvas& body, Canvas& glow, int type);
void buildDroneArt(Canvas& out, float tier);
void buildStationArt(Canvas& body, Canvas& glow);
void buildPlanet(Canvas& body, Canvas& glow, int sector, uint32_t seed);
void buildResourceIcon(Canvas& out, int res, int size);
void buildUpgradeIcon(Canvas& out, int upg, int size);
void buildAchievementIcon(Canvas& out, int ach, int size);
void buildDebris(Canvas& out, int sector, uint32_t seed);

}  // namespace sm
