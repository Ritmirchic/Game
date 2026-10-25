/* ============================================================================
   SPACE MINER — core/content.c  (game balance tables)
   ========================================================================== */
#include "content.h"
#include "gfx.h"

const ResourceDef k_resources[RES_COUNT] = {
    [RES_IRON]         = { "Iron", "Iron", "Fe", "Common structural metal. The backbone of every hull.", SM_RGB(150, 164, 182), SM_RGB(120, 150, 190), 1, 8, 1.00f, "res_iron" },
    [RES_COPPER]       = { "Copper", "Copper", "Cu", "Conductive wiring metal, always in demand.", SM_RGB(214, 132, 74), SM_RGB(255, 150, 70), 1, 13, 1.10f, "res_copper" },
    [RES_NICKEL]       = { "Nickel", "Nickel", "Ni", "Corrosion-resistant plating alloy.", SM_RGB(176, 196, 188), SM_RGB(140, 200, 200), 2, 19, 1.30f, "res_nickel" },
    [RES_ICE]          = { "Cryo Ice", "Ice", "H2O", "Volatile ice. Valuable to station life support.", SM_RGB(150, 214, 255), SM_RGB(90, 180, 255), 1, 7, 0.62f, "res_ice" },
    [RES_TITANIUM]     = { "Titanium", "Titanium", "Ti", "Aerospace grade, tough and light.", SM_RGB(206, 210, 220), SM_RGB(160, 190, 255), 3, 35, 1.75f, "res_titanium" },
    [RES_SILVER]       = { "Silver", "Silver", "Ag", "High-purity conductor and mirror coating.", SM_RGB(226, 236, 246), SM_RGB(180, 220, 255), 3, 47, 1.52f, "res_silver" },
    [RES_SALVAGE]      = { "Salvage Alloy", "Salvage", "Sl", "Recovered wreck plating, stripped from derelicts.", SM_RGB(190, 142, 92), SM_RGB(220, 150, 70), 2, 29, 1.45f, "res_salvage" },
    [RES_GOLD]         = { "Gold", "Gold", "Au", "Dense precious metal, fat station margins.", SM_RGB(255, 204, 84), SM_RGB(255, 190, 60), 4, 82, 1.70f, "res_gold" },
    [RES_IRIDIUM]      = { "Iridium", "Iridium", "Ir", "Extreme-melting noble metal. Rare find.", SM_RGB(214, 244, 255), SM_RGB(150, 230, 255), 4, 104, 2.15f, "res_iridium" },
    [RES_PLATINUM]     = { "Platinum", "Platinum", "Pt", "Catalyst metal prized by station labs.", SM_RGB(238, 246, 255), SM_RGB(200, 235, 255), 5, 148, 2.10f, "res_platinum" },
    [RES_URANIUM]      = { "Uranium", "Uranium", "U", "Reactor fuel. Handle your beams carefully.", SM_RGB(140, 255, 106), SM_RGB(110, 255, 90), 5, 172, 2.40f, "res_uranium" },
    [RES_CRYSTAL]      = { "Resonant Crystal", "Crystal", "Rc", "Lattice that sings under mining lasers.", SM_RGB(110, 240, 255), SM_RGB(80, 220, 255), 4, 126, 1.60f, "res_crystal" },
    [RES_PLASMA]       = { "Plasma Ore", "Plasma", "Pl", "Superheated ore, still crackling with charge.", SM_RGB(255, 122, 60), SM_RGB(255, 100, 40), 6, 248, 2.60f, "res_plasma" },
    [RES_ANTIMATTER]   = { "Antimatter Cell", "Antimatter", "Am", "Contained annihilation. Worth a fortune.", SM_RGB(255, 79, 216), SM_RGB(255, 60, 220), 7, 432, 3.00f, "res_antimatter" },
    [RES_VOIDCRYSTAL]  = { "Void Crystal", "Void", "Vc", "Crystal that drinks light near the frontier.", SM_RGB(180, 140, 255), SM_RGB(140, 90, 255), 8, 690, 3.40f, "res_voidcrystal" },
    [RES_DARKMATTER]   = { "Dark Matter Shard", "Dark Matter", "Dm", "Mass without light. The rarest harvest.", SM_RGB(126, 96, 255), SM_RGB(90, 60, 255), 9, 1180, 3.80f, "res_darkmatter" },
};

const char *res_name(int id) { return (id >= 0 && id < RES_COUNT) ? k_resources[id].name : "?"; }
u32 res_color(int id) { return (id >= 0 && id < RES_COUNT) ? k_resources[id].color : SM_RGB(255,255,255); }
int res_rarity(int id) { return (id >= 0 && id < RES_COUNT) ? k_resources[id].rarity : 1; }

const SectorDef k_sectors[SECTOR_COUNT] = {
    [0] = {
        "STARTER BELT", "Sector 01 / Asteroid Belt",
        "A calm tag-along belt of common rock. Every pilot starts here, and most of them stay a little too long.",
        SM_RGB(10, 17, 30), SM_RGB(4, 8, 16),
        SM_RGB(44, 78, 120), SM_RGB(120, 92, 52),
        SM_RGB(200, 226, 255), SM_RGB(120, 150, 180), SM_RGB(120, 214, 255), SM_RGB(10, 20, 34),
        0, 1, 1, 0,
        96, 1, { ENEMY_SCOUT, 0, 0, 0 }, 1, HAZARD_NONE,
        { { RES_IRON, 30 }, { RES_COPPER, 22 }, { RES_NICKEL, 16 }, { RES_ICE, 18 },
          { RES_TITANIUM, 9 }, { RES_SILVER, 4 }, { RES_GOLD, 2 } }, 7,
        "music_belt"
    },
    [1] = {
        "CRYSTAL NEBULA", "Sector 02 / Luminous Clouds",
        "Ionised gas and singing crystal. The nebula's glow hides shard drifts that will cut a careless hull open.",
        SM_RGB(26, 10, 40), SM_RGB(8, 6, 22),
        SM_RGB(120, 40, 140), SM_RGB(30, 120, 160),
        SM_RGB(255, 226, 255), SM_RGB(190, 150, 255), SM_RGB(255, 110, 220), SM_RGB(22, 10, 34),
        1, 2, 3, 6000,
        112, 2, { ENEMY_SCOUT, ENEMY_RAIDER, 0, 0 }, 2, HAZARD_CRYSTAL_DRIFT,
        { { RES_ICE, 12 }, { RES_SILVER, 16 }, { RES_CRYSTAL, 26 }, { RES_TITANIUM, 14 },
          { RES_NICKEL, 10 }, { RES_GOLD, 12 }, { RES_PLATINUM, 5 }, { RES_IRIDIUM, 5 } }, 8,
        "music_nebula"
    },
    [2] = {
        "IRON GRAVEYARD", "Sector 03 / Derelict Reach",
        "A battlefield nobody cleaned up. Hulls drift here by the hundred, and the drones that killed them never left.",
        SM_RGB(14, 20, 18), SM_RGB(5, 9, 10),
        SM_RGB(56, 82, 66), SM_RGB(96, 70, 40),
        SM_RGB(220, 240, 220), SM_RGB(150, 170, 140), SM_RGB(140, 255, 190), SM_RGB(10, 20, 18),
        2, 3, 6, 24000,
        126, 3, { ENEMY_RAIDER, ENEMY_HEAVY, ENEMY_SCOUT, 0 }, 3, HAZARD_DEBRIS_FIELD,
        { { RES_SALVAGE, 30 }, { RES_IRON, 16 }, { RES_TITANIUM, 18 }, { RES_NICKEL, 10 },
          { RES_GOLD, 10 }, { RES_IRIDIUM, 9 }, { RES_URANIUM, 5 }, { RES_PLATINUM, 3 } }, 8,
        "music_grave"
    },
    [3] = {
        "PLASMA STORM", "Sector 04 / Storm Corridor",
        "Permanent electrical hurricane. Lightning walks across the hull while the ore beneath glows hot enough to see.",
        SM_RGB(26, 12, 26), SM_RGB(10, 4, 12),
        SM_RGB(160, 60, 30), SM_RGB(90, 30, 120),
        SM_RGB(255, 240, 220), SM_RGB(255, 170, 110), SM_RGB(255, 140, 60), SM_RGB(26, 10, 20),
        3, 4, 10, 78000,
        138, 4, { ENEMY_HUNTER, ENEMY_HEAVY, ENEMY_RAIDER, 0 }, 3, HAZARD_PLASMA_STORM,
        { { RES_PLASMA, 24 }, { RES_URANIUM, 20 }, { RES_IRIDIUM, 14 }, { RES_PLATINUM, 14 },
          { RES_ANTIMATTER, 10 }, { RES_GOLD, 8 }, { RES_CRYSTAL, 10 } }, 7,
        "music_storm"
    },
    [4] = {
        "VOID FRONTIER", "Sector 05 / The Silent Edge",
        "Past the charted lanes the light gives up. Sentinels patrol a dark that answers back.",
        SM_RGB(12, 8, 26), SM_RGB(3, 3, 10),
        SM_RGB(58, 34, 120), SM_RGB(20, 10, 60),
        SM_RGB(230, 220, 255), SM_RGB(160, 140, 255), SM_RGB(180, 140, 255), SM_RGB(12, 8, 26),
        4, 5, 15, 240000,
        150, 5, { ENEMY_SENTINEL, ENEMY_HUNTER, ENEMY_HEAVY, 0 }, 3, HAZARD_VOID_SURGE,
        { { RES_VOIDCRYSTAL, 26 }, { RES_DARKMATTER, 13 }, { RES_ANTIMATTER, 18 },
          { RES_PLASMA, 12 }, { RES_URANIUM, 12 }, { RES_PLATINUM, 14 }, { RES_IRIDIUM, 7 } }, 7,
        "music_void"
    },
};

const char *sector_name(int i) { return (i >= 0 && i < SECTOR_COUNT) ? k_sectors[i].name : "UNKNOWN"; }
f32 sector_difficulty(int sector) { return 1.f + (f32)sector * 0.42f; }

const ShipDef k_ships[SHIP_COUNT] = {
    [SHIP_PROSPECTOR] = { "PROSPECTOR", "Mining Cutter",
        "The classic single-seat cutter. Balanced, forgiving, and endlessly upgradable.",
        "ship_prospector", 110, 45, 100, 265.f, 540.f, 2.6f, 140, 2, 14.f, 9.f, 2.6f, WEAPON_PULSE,
        0, 1, 0.62f },
    [SHIP_BOREAS] = { "BOREAS", "Heavy Excavator",
        "Industrial spine with twin mining booms. Slow to turn, but it eats asteroid fields for breakfast.",
        "ship_boreas", 190, 70, 140, 205.f, 430.f, 1.9f, 340, 4, 21.f, 7.f, 2.0f, WEAPON_PULSE,
        14500, 4, 0.86f },
    [SHIP_VANGUARD] = { "VANGUARD", "Strike Miner",
        "Fast interceptor hull with a mining rig bolted on. For pilots who clear the field the hard way.",
        "ship_vanguard", 145, 105, 120, 340.f, 700.f, 3.1f, 160, 2, 12.f, 16.f, 3.2f, WEAPON_PULSE,
        32000, 7, 0.66f },
    [SHIP_CHIMERA] = { "CHIMERA", "Void-Class Hybrid",
        "Prototype long-range hull. Void-shielded, drone-bayed, and armed like a warship.",
        "ship_chimera", 275, 165, 200, 315.f, 660.f, 2.8f, 420, 6, 26.f, 20.f, 3.0f, WEAPON_LANCE,
        118000, 12, 0.92f },
};

const UpgradeDef k_upgrades[UPG_COUNT] = {
    [UPG_LASER_POWER]   = { "Mining Laser Power", "Focused emitter output. Cuts harder rock faster.", "+22% mining damage per rank", "upg_laser", SM_RGB(120, 240, 255), 900, 1.85f },
    [UPG_MINING_SPEED]  = { "Beam Cycling", "Faster pulse cycling means more ore per second.", "+18% mining rate per rank", "upg_speed", SM_RGB(140, 255, 190), 780, 1.8f },
    [UPG_CARGO]         = { "Cargo Expansion", "Modular holds bolted to the spine.", "+45 cargo capacity per rank", "upg_cargo", SM_RGB(255, 200, 110), 650, 1.75f },
    [UPG_ENGINE]        = { "Engine Tuning", "Reinforced nozzles and hotter burn.", "+9% top speed per rank", "upg_engine", SM_RGB(255, 140, 90), 820, 1.8f },
    [UPG_ACCEL]         = { "Thruster Array", "Manoeuvring jets with bigger flow chambers.", "+14% acceleration per rank", "upg_accel", SM_RGB(255, 170, 120), 700, 1.75f },
    [UPG_HULL]          = { "Hull Plating", "Layered ablative plating over the frame.", "+30 max hull per rank", "upg_hull", SM_RGB(190, 200, 215), 1000, 1.85f },
    [UPG_SHIELD]        = { "Shield Capacitor", "Extra capacitance banks for the deflector.", "+26 max shield per rank", "upg_shield", SM_RGB(120, 180, 255), 1150, 1.9f },
    [UPG_ENERGY_REGEN]  = { "Reactor Cooling", "Better heat exchange, faster recharge.", "+20% energy regeneration per rank", "upg_energy", SM_RGB(190, 255, 120), 950, 1.85f },
    [UPG_SCANNER]       = { "Deep Scanner", "Longer range, richer deposits revealed.", "+18% scanner range, richer ore per rank", "upg_scanner", SM_RGB(200, 140, 255), 1100, 1.9f },
    [UPG_DRONE_TECH]    = { "Drone Uplink", "Smarter automation firmware for your fleet.", "+1 drone slot, +12% drone speed", "upg_drone", SM_RGB(255, 120, 200), 2400, 2.0f },
    [UPG_WEAPON_DAMAGE] = { "Weapon Capacitors", "Divert reactor power to the guns.", "+20% weapon damage per rank", "upg_weapon", SM_RGB(255, 90, 90), 1300, 1.9f },
    [UPG_TRACTOR]       = { "Tractor Field", "Widens the collection envelope and pulls ore in.", "+35% pickup radius per rank", "upg_tractor", SM_RGB(140, 230, 255), 880, 1.8f },
};

s64 upgrade_cost(int id, int level) {
    if (id < 0 || id >= UPG_COUNT) return 0;
    const UpgradeDef *u = &k_upgrades[id];
    f32 c = (f32)u->cost_base;
    for (int i = 0; i < level; i++) c *= u->cost_growth;
    return (s64)(c * 0.5f) * 2;
}

const WeaponDef k_weapons[WEAPON_COUNT] = {
    [WEAPON_PULSE] = { "Pulse Repeater", "Rapid light bolts. Reliable and cheap to run.", 9.f, 3.0f, 900.f, 620.f, SM_RGB(120, 220, 255), 0, 0 },
    [WEAPON_LANCE] = { "Plasma Lance", "Slow heavy plasma spears that punch through armour.", 26.f, 1.35f, 1050.f, 700.f, SM_RGB(255, 140, 255), 16000, 1 },
    [WEAPON_ARC]   = { "Arc Emitter", "Short-range chain lightning. Brutal up close.", 15.f, 4.0f, 1400.f, 330.f, SM_RGB(180, 160, 255), 26000, 2 },
};

const DroneDef k_drones[DRONE_TYPE_COUNT] = {
    [DRONE_HARVESTER] = { "Harvester MKI", "Light drone. Finds a rock, cuts it, hauls the ore home.", "drone_harvester", 5.5f, 165.f, 14, 3200, 0.34f },
    [DRONE_EXCAVATOR] = { "Excavator MKII", "Heavy drone with a reinforced cutter head and big hopper.", "drone_excavator", 12.5f, 138.f, 34, 9800, 0.46f },
};

const EnemyDef k_enemies[ENEMY_COUNT] = {
    [ENEMY_SCOUT]    = { "Scout Drone", "Cheap patrol unit. Circles, probes, stings.",
        "enemy_scout", 32, 6, 168, 3.0f, 430, 1.35f, 1, 0, 45, 14, 0.40f, SM_RGB(255, 120, 90), 0 },
    [ENEMY_RAIDER]   = { "Raider", "Pirate skiff. Strafes and burns your hull with bursts.",
        "enemy_raider", 78, 13, 205, 2.4f, 470, 0.95f, 2, 0, 115, 34, 0.52f, SM_RGB(255, 90, 80), 1 },
    [ENEMY_HEAVY]    = { "Heavy Drone", "Armoured gun platform. Slow, patient, very hard to kill.",
        "enemy_heavy", 240, 26, 96, 1.2f, 400, 1.9f, 3, 1, 290, 88, 0.74f, SM_RGB(255, 160, 70), 2 },
    [ENEMY_HUNTER]   = { "Plasma Hunter", "Storm-born assassin. Fast, homing plasma, no mercy.",
        "enemy_hunter", 150, 21, 250, 3.2f, 520, 1.15f, 1, 1, 240, 70, 0.58f, SM_RGB(255, 110, 220), 3 },
    [ENEMY_SENTINEL] = { "Void Sentinel", "Frontier guardian. Beam sweep and missile volleys.",
        "enemy_sentinel", 460, 38, 128, 1.6f, 600, 1.6f, 4, 2, 640, 215, 0.92f, SM_RGB(170, 130, 255), 4 },
};

const AchievementDef k_achievements[ACH_COUNT] = {
    [ACH_FIRST_BLOOD]   = { "First Blood", "Destroy an enemy ship.", "Open fire on a hostile." },
    [ACH_FIRST_ORE]     = { "Rock Bottom", "Mine your first asteroid.", "Hold the mining laser on a rock." },
    [ACH_MILLIONAIRE]   = { "Millionaire", "Bank 1,000,000 credits.", "Sell ore until the counter turns over." },
    [ACH_CRYSTAL_HUNTER]= { "Crystal Hunter", "Deliver 250 Resonant Crystals.", "Sell crystal in bulk." },
    [ACH_DEEP_SPACE]    = { "Deep Space", "Unlock the Iron Graveyard.", "Reach mining level 6." },
    [ACH_VOID_WALKER]   = { "Void Walker", "Unlock the Void Frontier.", "Reach mining level 15." },
    [ACH_DRONE_ARMADA]  = { "Drone Armada", "Have 4 drones active at once.", "Buy drones and keep them flying." },
    [ACH_SHARPSHOOTER]  = { "Sharpshooter", "Destroy 50 enemies.", "Keep the field clear." },
    [ACH_FLEET_ACE]     = { "Fleet Ace", "Own every ship class.", "Buy the whole hangar." },
    [ACH_MINER_MASTER]  = { "Miner Master", "Reach mining level 20.", "Mine, sell, repeat." },
    [ACH_FULL_UPGRADE]  = { "Fully Loaded", "Max out any upgrade line.", "Five ranks in one system." },
    [ACH_SURVIVOR]      = { "Survivor", "Survive a sentinel ambush and win.", "Kill a Void Sentinel." },
};

int level_xp_required(int level) {
    if (level < 1) level = 1;
    return (int)(120.0 * powf((f32)level, 1.42f));
}
