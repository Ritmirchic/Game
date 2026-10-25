/* ============================================================================
   SPACE MINER — core/content.h
   Static game content: resources, sectors, ships, upgrades, enemies, achievements.
   ========================================================================== */
#ifndef SM_CONTENT_H
#define SM_CONTENT_H
#include "common.h"

/* ----------------------------------------------------------- resources ---- */
enum {
    RES_IRON = 0, RES_COPPER, RES_NICKEL, RES_ICE, RES_TITANIUM, RES_SILVER,
    RES_SALVAGE, RES_GOLD, RES_IRIDIUM, RES_PLATINUM, RES_URANIUM, RES_CRYSTAL,
    RES_PLASMA, RES_ANTIMATTER, RES_VOIDCRYSTAL, RES_DARKMATTER,
    RES_COUNT
};

typedef struct {
    const char *name;
    const char *short_name;
    const char *symbol;
    const char *desc;
    u32 color;
    u32 glow;
    int rarity;         /* 1..9 */
    int value;          /* credits per unit */
    f32 hardness;       /* mining resistance multiplier */
    const char *sheet;  /* icon sheet name in the pak */
} ResourceDef;

extern const ResourceDef k_resources[RES_COUNT];
const char *res_name(int id);
u32 res_color(int id);
int res_rarity(int id);

/* ------------------------------------------------------------- sectors ---- */
#define SECTOR_COUNT 5
#define SECTOR_MAX_POOL 8

typedef struct {
    int res;
    f32 weight;
} ResWeight;

typedef enum {
    HAZARD_NONE = 0,
    HAZARD_CRYSTAL_DRIFT,     /* drifting crystal shards damage the hull */
    HAZARD_DEBRIS_FIELD,      /* tumbling wreckage */
    HAZARD_PLASMA_STORM,      /* lightning bolts arc through the field */
    HAZARD_VOID_SURGE         /* gravity ripples + dark energy bursts */
} HazardType;

typedef struct {
    const char *name;
    const char *subtitle;
    const char *desc;
    u32 bg_top, bg_bottom;      /* deep space gradient */
    u32 nebula_a, nebula_b;     /* nebula clouds */
    u32 star_color;
    u32 dust_color;
    u32 accent;                 /* UI + beam tint */
    u32 fog;
    int bg_variant;             /* selects background art sheet */
    int danger;                 /* 1..5 */
    int req_level;              /* mining level required */
    s64 unlock_cost;
    int asteroid_count;
    int enemy_density;          /* enemies per sector roll */
    int enemy_types[4];
    int enemy_type_count;
    HazardType hazard;
    ResWeight pool[SECTOR_MAX_POOL];
    int pool_count;
    const char *music;
} SectorDef;

extern const SectorDef k_sectors[SECTOR_COUNT];

/* --------------------------------------------------------------- ships ---- */
enum { SHIP_PROSPECTOR = 0, SHIP_BOREAS, SHIP_VANGUARD, SHIP_CHIMERA, SHIP_COUNT };

typedef struct {
    const char *name;
    const char *role;
    const char *desc;
    const char *sheet;
    int hull;
    int shield;
    int energy;
    f32 max_speed;
    f32 accel;
    f32 turn_rate;
    int cargo;
    int drone_slots;
    f32 laser_dps;
    f32 weapon_damage;
    f32 weapon_rate;
    int weapon_type;
    s64 cost;
    int req_level;
    f32 scale;
} ShipDef;

extern const ShipDef k_ships[SHIP_COUNT];

/* ------------------------------------------------------------ upgrades ---- */
enum {
    UPG_LASER_POWER = 0, UPG_MINING_SPEED, UPG_CARGO, UPG_ENGINE, UPG_ACCEL,
    UPG_HULL, UPG_SHIELD, UPG_ENERGY_REGEN, UPG_SCANNER, UPG_DRONE_TECH,
    UPG_WEAPON_DAMAGE, UPG_TRACTOR,
    UPG_COUNT
};
#define UPG_MAX_LEVEL 5

typedef struct {
    const char *name;
    const char *desc;
    const char *effect;      /* per-rank effect string */
    const char *icon;
    u32 color;
    s64 cost_base;
    f32 cost_growth;
} UpgradeDef;

extern const UpgradeDef k_upgrades[UPG_COUNT];
s64 upgrade_cost(int id, int level);

/* ------------------------------------------------------------- weapons ---- */
enum { WEAPON_PULSE = 0, WEAPON_LANCE, WEAPON_ARC, WEAPON_COUNT };
typedef struct {
    const char *name;
    const char *desc;
    f32 damage;
    f32 rate;          /* shots / second */
    f32 speed;
    f32 range;
    u32 color;
    s64 cost;
    int projectile_kind;
} WeaponDef;
extern const WeaponDef k_weapons[WEAPON_COUNT];

/* -------------------------------------------------------------- drones ---- */
enum { DRONE_HARVESTER = 0, DRONE_EXCAVATOR, DRONE_TYPE_COUNT };
typedef struct {
    const char *name;
    const char *desc;
    const char *sheet;
    f32 mining_dps;
    f32 speed;
    int capacity;
    s64 cost;
    f32 scale;
} DroneDef;
extern const DroneDef k_drones[DRONE_TYPE_COUNT];

/* ------------------------------------------------------------- enemies ---- */
enum { ENEMY_SCOUT = 0, ENEMY_RAIDER, ENEMY_HEAVY, ENEMY_HUNTER, ENEMY_SENTINEL, ENEMY_COUNT };
typedef struct {
    const char *name;
    const char *desc;
    const char *sheet;
    f32 hull;
    f32 damage;
    f32 speed;
    f32 turn;
    f32 range;
    f32 fire_interval;
    int burst;
    int projectile_kind;
    s64 credits;
    int xp;
    f32 scale;
    u32 color;
    int behavior;     /* 0 orbit, 1 strafe, 2 heavy, 3 hunt, 4 sentinel */
} EnemyDef;
extern const EnemyDef k_enemies[ENEMY_COUNT];

/* -------------------------------------------------------- achievements ---- */
enum {
    ACH_FIRST_BLOOD = 0, ACH_FIRST_ORE, ACH_MILLIONAIRE, ACH_CRYSTAL_HUNTER,
    ACH_DEEP_SPACE, ACH_VOID_WALKER, ACH_DRONE_ARMADA, ACH_SHARPSHOOTER,
    ACH_FLEET_ACE, ACH_MINER_MASTER, ACH_FULL_UPGRADE, ACH_SURVIVOR,
    ACH_COUNT
};
typedef struct {
    const char *name;
    const char *desc;
    const char *hint;
} AchievementDef;
extern const AchievementDef k_achievements[ACH_COUNT];

/* --------------------------------------------------------------- levels --- */
int  level_xp_required(int level);
f32  sector_difficulty(int sector);
const char *sector_name(int i);

#endif
