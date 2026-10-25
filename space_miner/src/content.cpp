// ============================================================
// SPACE MINER  -  content.cpp
// ============================================================
#include "content.h"

namespace sm {

const ResourceDef RESOURCES[RES_COUNT] = {
    {"IRON",         "Fe", rgba(150, 148, 142),   8,   0, .75f,   4},
    {"COPPER",       "Cu", rgba(214, 122,  66),  16,   0, .85f,   6},
    {"NICKEL",       "Ni", rgba(168, 184, 176),  26,   1, .95f,   8},
    {"COBALT",       "Co", rgba( 78, 130, 226),  46,   1, 1.10f, 12},
    {"TITANIUM",     "Ti", rgba(196, 210, 226),  74,   2, 1.55f, 18},
    {"SILVER",       "Ag", rgba(226, 232, 240), 108,   2, 1.20f, 24},
    {"GOLD",         "Au", rgba(255, 200,  74), 168,   3, 1.35f, 34},
    {"PLATINUM",     "Pt", rgba(206, 226, 236), 245,   3, 1.70f, 48},
    {"URANIUM",      "Ur", rgba(126, 240, 110), 380,   3, 1.85f, 66},
    {"CRYSTAL",      "Cr", rgba(126, 222, 255), 520,   4, 1.45f, 90},
    {"PLASMA ORE",   "Pm", rgba(255, 122, 214), 690,   4, 2.05f, 120},
    {"IRIDIUM",      "Ir", rgba(196, 158, 255), 940,   4, 1.95f, 150},
    {"DARK MATTER",  "Dm", rgba(122,  86, 186), 1560,  5, 2.40f, 210},
    {"VOID CRYSTAL", "Vc", rgba( 90, 255, 214), 2250,  5, 2.20f, 300},
    {"QUANTUM CORE", "Qc", rgba(255, 236, 160), 4200,  5, 2.75f, 520},
};

const SectorDef SECTORS[5] = {
    {
        "STARTER BELT", "SECTOR 01", "The training belt. Dense, warm and forgiving.",
        rgba(255, 186, 92),
        {rgba(8, 11, 22), rgba(18, 14, 26), rgba(72, 44, 92), rgba(24, 56, 92), rgba(210, 220, 255),
         rgba(120, 96, 80), rgba(255, 186, 92)},
        5200.f, 140,
        {RES_IRON, RES_COPPER, RES_NICKEL, RES_SILVER},
        {58, 26, 14, 2},
        {EN_SCOUT, -1, -1, -1},
        .20f, 0, .08f, 0, 55
    },
    {
        "CRYSTAL NEBULA", "SECTOR 02", "Ionised gas and glittering crystal growths.",
        rgba(126, 222, 255),
        {rgba(10, 8, 26), rgba(26, 12, 40), rgba(96, 30, 130), rgba(20, 70, 120), rgba(200, 236, 255),
         rgba(70, 90, 150), rgba(126, 222, 255)},
        5600.f, 150,
        {RES_CRYSTAL, RES_COBALT, RES_SILVER, RES_NICKEL},
        {40, 30, 18, 12},
        {EN_SCOUT, EN_RAIDER, -1, -1},
        .38f, 2500, .18f, 1, 70
    },
    {
        "IRON GRAVEYARD", "SECTOR 03", "Hulks of a dead war. Raiders pick the bones.",
        rgba(196, 210, 226),
        {rgba(6, 10, 16), rgba(14, 20, 28), rgba(40, 56, 74), rgba(70, 52, 34), rgba(190, 205, 225),
         rgba(80, 84, 96), rgba(220, 226, 240)},
        6000.f, 160,
        {RES_TITANIUM, RES_GOLD, RES_NICKEL, RES_PLATINUM},
        {40, 26, 18, 16},
        {EN_RAIDER, EN_HEAVY, EN_SCOUT, -1},
        .55f, 12000, .22f, 2, 40
    },
    {
        "PLASMA STORM", "SECTOR 04", "Living lightning. Do not linger in the arcs.",
        rgba(255, 122, 214),
        {rgba(16, 6, 24), rgba(34, 8, 34), rgba(140, 24, 110), rgba(60, 20, 120), rgba(255, 200, 240),
         rgba(110, 50, 120), rgba(255, 122, 214)},
        6400.f, 170,
        {RES_PLASMA, RES_URANIUM, RES_IRIDIUM, RES_PLATINUM},
        {36, 28, 24, 12},
        {EN_PLASMA, EN_RAIDER, EN_HEAVY, -1},
        .74f, 45000, .72f, 3, 60
    },
    {
        "VOID FRONTIER", "SECTOR 05", "Beyond the charted dark. Nothing comes back unchanged.",
        rgba(140, 255, 226),
        {rgba(3, 6, 12), rgba(8, 14, 22), rgba(18, 44, 60), rgba(60, 22, 84), rgba(180, 255, 240),
         rgba(40, 60, 70), rgba(140, 255, 226)},
        6800.f, 180,
        {RES_VOIDCRYSTAL, RES_DARKMATTER, RES_QUANTUM, RES_IRIDIUM},
        {34, 30, 12, 24},
        {EN_SENTINEL, EN_PLASMA, EN_HEAVY, EN_RAIDER},
        .95f, 150000, .95f, 4, 85
    },
};

const UpgradeDef UPGRADES[UPG_COUNT] = {
    {"MINING LASER",   "Beam power. Cuts rock faster and drains less per hit.", 10,  900, 1.55f, .18f, "%"},
    {"MINING SPEED",   "Cycle rate of the mining emitter.",                     10,  750, 1.5f,  .12f, "%"},
    {"CARGO HOLD",     "Adds 40 units of storage to the hold.",                 12,  650, 1.45f, 40.f, "u"},
    {"ENGINE TUNING",  "Top speed and turn authority.",                          8, 1100, 1.6f,  .08f, "%"},
    {"HULL PLATING",   "Reinforced structure. More hull, less repair cost.",     8, 1400, 1.62f, .18f, "%"},
    {"SHIELD ARRAY",   "Capacity of the rechargeable deflector screen.",         8, 1600, 1.65f, .20f, "%"},
    {"ENERGY CORE",    "Reactor throughput. Faster laser recharge.",             8, 1300, 1.6f,  .15f, "%"},
    {"RESOURCE SCAN",  "Longer range on the mineral scanner.",                   6, 1000, 1.5f,  90.f, "m"},
    {"DRONE BAYS",     "Extra autonomous mining drone slots.",                   4, 5000, 2.1f,  1.f, ""},
    {"WEAPON SYSTEMS","Damage of the forward pulse cannons.",                   10, 1800, 1.6f,  .20f, "%"},
    {"TRACTOR COIL",   "Pulls loose cargo from further away.",                   6, 1200, 1.55f, 35.f, "m"},
    {"REFINERY",       "Refined ore sells for more credits.",                    8, 2200, 1.6f,  .08f, "%"},
};

const ShipDef SHIPS[SHIP_COUNT] = {
    {"PROSPECTOR", "Mining Cutter", "The dependable starter hull. Small hold, sharp handling.",
     0, 100, 50, 235, 400, 130, 1.00f, 1.00f, 9.f, 900, 1, 1, 2, rgba(86, 216, 240), 0},
    {"HARVESTER", "Industrial Rig", "Heavy industrial frame with a cavernous hold and twin drone bays.",
     6500, 175, 80, 205, 330, 340, 1.15f, .95f, 11.f, 1050, 1, 2, 3, rgba(255, 176, 74), 0},
    {"LANCER", "Assault Corvette", "Gunboat conversion. Two cannon barrels and a hard shield.",
     24000, 150, 140, 285, 480, 210, 1.10f, 1.20f, 18.f, 1200, 2, 2, 3, rgba(255, 92, 120), 0},
    {"VOIDCRAFT", "Deep Void Ark", "Prototype ark built from frontier salvage. Everything, more of it.",
     95000, 240, 220, 300, 470, 430, 1.55f, 1.55f, 26.f, 1500, 2, 4, 4, rgba(150, 255, 220), 4},
};

const DroneDef DRONES[3] = {
    {"SCRAPPER", "Salvage rotor. Slow, cheap, does the boring work.", 1000, .55f, 190.f, 14.f},
    {"PROSPECTOR", "Standard mining drone with a proper hold.",       4500, 1.00f, 235.f, 26.f},
    {"VOID-RIG", "Frontier-built rig. Fast, hungry, very good at its job.", 18000, 1.80f, 285.f, 44.f},
};

const EnemyDef ENEMIES[EN_TYPE_COUNT] = {
    {"SCOUT DRONE",   30.f,   0.f, 195.f,  5.f, 1.5f, 520.f,  14.f,  32,  26.f, rgba(255, 132, 96)},
    {"RAIDER SHIP",   78.f,  20.f, 170.f, 10.f, 1.25f, 600.f,  32.f,  85,  36.f, rgba(255, 196, 92)},
    {"HEAVY DRONE",  240.f,  60.f, 112.f, 18.f, 1.9f, 520.f,  85.f, 220,  50.f, rgba(168, 190, 255)},
    {"PLASMA HUNTER",130.f,  40.f, 215.f, 24.f, 1.05f, 560.f, 110.f, 300,  34.f, rgba(255, 96, 210)},
    {"VOID SENTINEL",460.f, 220.f, 145.f, 34.f, 1.35f, 700.f, 260.f, 900,  62.f, rgba(120, 255, 226)},
};

const AchievementDef ACHIEVEMENTS[ACH_COUNT] = {
    {"FIRST CONTACT",  "Break your first asteroid open.",            0, rgba(126, 222, 255)},
    {"ROCKBREAKER",    "Destroy 100 asteroids.",                     1, rgba(200, 210, 226)},
    {"FIRST BLOOD",    "Destroy a hostile drone.",                    2, rgba(255, 132, 96)},
    {"HUNTER",         "Destroy 25 hostiles.",                        3, rgba(255, 176, 74)},
    {"CHARTED",        "Unlock the Crystal Nebula.",                  4, rgba(126, 222, 255)},
    {"TO THE FRONTIER","Unlock the Void Frontier.",                   5, rgba(140, 255, 226)},
    {"DEEP POCKETS",   "Hold 100'000 credits at once.",               6, rgba(255, 200, 74)},
    {"TINKERER",       "Buy 10 ship upgrades.",                       7, rgba(160, 200, 255)},
    {"FULL POWER",     "Max out any upgrade branch.",                 8, rgba(255, 236, 160)},
    {"SWARM LORD",     "Fly with four mining drones.",                9, rgba(120, 230, 200)},
    {"CRYSTAL FEVER",  "Mine crystal in the nebula.",                10, rgba(126, 222, 255)},
    {"PLASMA BURN",    "Mine plasma ore in the storm.",              11, rgba(255, 122, 214)},
    {"VOID TOUCHED",   "Mine a void crystal.",                       12, rgba(90, 255, 214)},
    {"NEW HULL",       "Buy a second ship.",                         13, rgba(255, 176, 74)},
    {"THE ARK",        "Commission the Voidcraft.",                  14, rgba(150, 255, 220)},
    {"LONG HAUL",      "Fly for 30 minutes.",                        15, rgba(180, 200, 255)},
    {"CLOSE CALL",     "Survive a hit at 10% hull or less.",          16, rgba(255, 92, 120)},
    {"QUANTUM",        "Recover a quantum core.",                    17, rgba(255, 236, 160)},
    {"SENTINEL FALLS","Destroy a Void Sentinel.",                    18, rgba(120, 255, 226)},
    {"MASTER REFINER", "Fully upgrade the refinery.",                19, rgba(255, 200, 120)},
};

const char* TUTORIAL_HINTS[6] = {
    "WASD THRUST  -  MOUSE AIM  -  HOLD LMB TO MINE",
    "FLY CLOSE TO LOOT TO TRACTOR IT INTO YOUR HOLD",
    "PRESS E NEAR THE STATION TO DOCK AND SELL",
    "PRESS M TO OPEN THE SECTOR CHART",
    "RIGHT MOUSE FIRES THE PULSE CANNONS",
    "PRESS TAB FOR THE FULL INVENTORY",
};

std::string fmtBig(long long v) {
    char buf[32];
    if (v >= 1000000) snprintf(buf, sizeof(buf), "%.2fM", v / 1000000.0);
    else if (v >= 1000) snprintf(buf, sizeof(buf), "%.1fK", v / 1000.0);
    else snprintf(buf, sizeof(buf), "%lld", v);
    return std::string(buf);
}
std::string resName(int id) { return id >= 0 && id < RES_COUNT ? RESOURCES[id].name : "?"; }
Color resColor(int id) { return id >= 0 && id < RES_COUNT ? RESOURCES[id].color : rgba(255, 0, 255); }
int resValue(int id) { return id >= 0 && id < RES_COUNT ? RESOURCES[id].value : 0; }

}  // namespace sm
