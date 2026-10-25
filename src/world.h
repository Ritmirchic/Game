// ============================================================================
// THE LAST NIGHT — gameplay world types
// ============================================================================
#ifndef TLN_WORLD_H
#define TLN_WORLD_H

#include "../engine/src/render.h"
#include "../engine/src/audio.h"
#include "../engine/src/anim.h"
#include "../engine/platform/platform.h"

#define MAX_ZOMBIES 96
#define MAX_BARRICADES 24
#define MAX_PICKUPS 64
#define MAX_DECOYS 12
#define MAX_PROJECTILES 48
#define MAX_PARTICLES 1200
#define MAX_DECALS 96
#define MAX_MESHES 512
#define MAX_PLAYERS 4

typedef uint32_t NetId;
typedef struct Model Model;

// ---------------------------------------------------------------- materials
enum {
    MAT_NONE = 0, MAT_FLOOR_TILE, MAT_FLOOR_LINO, MAT_FLOOR_CONCRETE, MAT_FLOOR_CARPET,
    MAT_ASPHALT, MAT_GRAVEL, MAT_DIRT, MAT_WALL_PAINT, MAT_WALL_BLOCK, MAT_WALL_BRICK,
    MAT_WALL_TILE, MAT_WALL_METAL, MAT_WALL_RUST, MAT_CEIL_TILE, MAT_CEIL_CONC,
    MAT_WOOD, MAT_PLYWOOD, MAT_DIAMOND, MAT_GLASS, MAT_CHAINLINK, MAT_CARDBOARD,
    MAT_TARP, MAT_RUBBER, MAT_SHELF_GOODS, MAT_FREEZER_GOODS, MAT_PRODUCE, MAT_MEAT,
    MAT_BOTTLES, MAT_SIGN_AISLE, MAT_SIGN_STORE, MAT_SIGN_EXIT, MAT_SIGN_WARN,
    MAT_PRICE, MAT_POSTER_A, MAT_POSTER_B, MAT_DIRECTORY, MAT_SCREEN, MAT_PAPER,
    MAT_CAR_RED, MAT_CAR_BLUE, MAT_CAR_WHITE, MAT_TIRE, MAT_VEH_GLASS, MAT_FENCE,
    MAT_SKIN_ZOMBIE, MAT_SKIN_PALE, MAT_SKIN_BRUTE, MAT_FLESH, MAT_DENIM,
    MAT_SHIRT_BLUE, MAT_SHIRT_WHITE, MAT_SHIRT_BROWN, MAT_HIVIS, MAT_LEATHER,
    MAT_SCRUBS, MAT_HAIR, MAT_GUN_METAL, MAT_GUN_POLY, MAT_WOOD_STOCK, MAT_BAT,
    MAT_AXE, MAT_STEEL, MAT_CROSSBOW, MAT_EMISSIVE_WARM, MAT_EMISSIVE_COOL,
    MAT_EMISSIVE_RED, MAT_EMISSIVE_GREEN, MAT_EMISSIVE_SCREEN, MAT_LAMPGLASS,
    MAT_COUNT
};

// ---------------------------------------------------------------- assets
typedef struct Assets {
    Tex *tex[192];
    int ntex;
    Material mats[MAT_COUNT];
    Tex *icons;
    Tex *logo, *panel, *panel_solid, *crosshair, *blood, *noise, *vignette, *flash, *smoke, *rain;
    Font *f_title, *f_bold, *f_reg, *f_mono;
    Sound *snd_shot_pistol[3];
    Sound *snd_shot_shotgun[2];
    Sound *snd_shot_rifle[3];
    Sound *snd_shot_sniper[2];
    Sound *snd_shot_crossbow[2];
    Sound *snd_melee_swing[3];
    Sound *snd_melee_hit[3];
    Sound *snd_groan[6];
    Sound *snd_scream[3];
    Sound *snd_step_player[4];
    Sound *snd_step_zombie[4];
    Sound *snd_thunder[3];
    Sound *snd_rain;
    Sound *snd_click;
    Sound *snd_hit;
    Sound *snd_break[3];
    Sound *snd_metal[2];
    Sound *snd_heartbeat;
    Sound *snd_stinger[2];
    Sound *snd_hum;
    Sound *snd_reload[3];
    Sound *snd_pickup;
    Sound *snd_door[2];
    Sound *snd_explosion;
    Sound *snd_roar[2];
    Sound *snd_empty;
    Sound *snd_ui_move, *snd_ui_select, *snd_ui_back;
    Sound *snd_heal;
    Sound *snd_build;
} Assets;

// ---------------------------------------------------------------- collision
typedef struct AABB {
    Vec2 mn, mx;
    float ymin, ymax;
    int blocks_move;     // 1 = blocks player/zombie movement
    int blocks_light;    // 1 = blocks baked light
    int blocks_bullet;   // 1 = blocks hitscan
    int material;        // impact fx material
} AABB;

typedef struct WallSeg {
    Vec2 a, b;
    float thickness;
    float y0, y1;
} WallSeg;

// ---------------------------------------------------------------- nav
typedef struct NavGrid {
    int w, h;
    float cell;
    Vec2 origin;
    uint8_t *walk;      // 0 = blocked
    float *cost;        // extra traversal cost (e.g. barricade)
    uint8_t *room;      // room id per cell
    Vec2 *flow;         // computed per-frame toward target
    float *dist;
    uint32_t *visited;
    int target_cx, target_cy;
} NavGrid;

// ---------------------------------------------------------------- level
typedef struct Door {
    Vec3 pos;
    float yaw;
    float width, height;
    int kind;            // 0 = wooden swing, 1 = double glass, 2 = metal roll-up, 3 = fire exit
    int open;
    int locked;
    float open_t;
    int mesh_index;
    int aabb_index;
    char name[32];
} Door;

typedef struct BarricadeSlot {
    Vec3 pos;
    float yaw;
    float width;
    int door_index;      // -1 = window/opening
    int barricade;       // index into world barricades, -1 = empty
    int is_window;
    char name[32];
} BarricadeSlot;

typedef struct SpawnPoint {
    Vec3 pos;
    float yaw;
    int room;
} SpawnPoint;

typedef struct Interactable {
    Vec3 pos;
    float radius;
    int kind;   // 0 = loot, 1 = upgrade bench, 2 = generator, 3 = plan, 4 = stairs
    int active;
    int index;
    char label[40];
} Interactable;

typedef struct Level {
    Mesh *meshes[MAX_MESHES];
    int mesh_count;
    AABB aabbs[1400];
    int aabb_count;
    WallSeg walls[220];
    int wall_count;
    NavGrid nav;
    Vec3 *bake_probe;      // per-vertex baked light is stored in mesh->lit
    Door doors[16];
    int door_count;
    BarricadeSlot slots[MAX_BARRICADES];
    int slot_count;
    SpawnPoint spawns[48];
    int spawn_count;
    Interactable interactables[32];
    int interactable_count;
    Vec3 player_spawn;
    float player_spawn_yaw;
    Vec3 bounds_mn, bounds_mx;
    // key room reference points (for AI + UI)
    Vec3 store_center;
    Vec3 counter_pos;
    Vec3 security_room;
    Vec3 generator_pos;
    Vec3 roof_access;
    Vec3 boss_arena;
    int lights_lit;
} Level;

// ---------------------------------------------------------------- entities
typedef enum { ZB_WALKER = 0, ZB_RUNNER, ZB_CRAWLER, ZB_BRUTE, ZB_SCREAMER, ZB_MINIBOSS, ZB_BOSS, ZB_TYPE_COUNT } ZombieType;

typedef enum { ZS_SPAWNING, ZS_IDLE, ZS_CHASE, ZS_ATTACK, ZS_BARRICADE, ZS_STAGGER, ZS_DEAD, ZS_SCREAM, ZS_LUNGE, ZS_SPECIAL } ZombieState;

typedef struct Zombie {
    Vec3 pos;
    Vec3 vel;
    float yaw;
    float hp, max_hp;
    ZombieType type;
    ZombieState state;
    float state_time;
    float attack_cd;
    float speed;
    float damage;
    float radius, height;
    float stagger;
    float death_time;
    float hit_flash;
    float anim_phase;
    float groan_timer;
    float stuck_timer;
    float path_timer;
    Vec2 path[16];
    int path_len, path_idx;
    Vec3 target;
    int target_player;
    float barricade_attack_timer;
    int barricade_idx;
    float scream_cd;
    float spawn_alpha;
    float scale;
    float limb_seed;
    float lunge_time;
    int alive;
    int flagged;
    int boss_phase;
    float special_cd;
    float spawn_ground_y;
    float crawl_bob;
    NetId net_id;
} Zombie;

typedef struct Barricade {
    int active;
    int slot;
    float hp, max_hp;
    int level;          // 1 = wood, 2 = reinforced, 3 = metal
    float build_t;
    float damage_flash;
    int mesh_index;
    Vec3 pos;
    float yaw;
    float width;
} Barricade;

typedef struct Trap {
    int active, kind;
    Vec3 pos;
    float armed_t;
    float hp;
} Trap;

typedef enum { IT_AMMO_PISTOL, IT_AMMO_SHELL, IT_AMMO_RIFLE, IT_AMMO_BOLT, IT_MEDKIT, IT_BANDAGE,
               IT_PILLS, IT_ADRENALINE, IT_SCRAP, IT_MONEY, IT_BATTERY, IT_COUNT } ItemKind;

typedef struct Pickup {
    int active;
    int kind;
    Vec3 pos;
    int amount;
    float bob;
    float life;
    float magnet;
} Pickup;

typedef struct Particle {
    Vec3 pos, vel;
    float life, max_life;
    float size, grow;
    uint32_t color;
    int additive;
    float gravity;
    float drag;
    float rot, rot_spd;
    const Tex *tex;
} Particle;

typedef struct Decal {
    int active;
    Vec3 pos, nrm;
    float size;
    const Tex *tex;
    uint32_t color;
    float life;
} Decal;

typedef struct Projectile {
    int active, kind;      // 0 = arrow/bolt, 1 = grenade, 2 = bile
    Vec3 pos, vel;
    float life, damage;
    float radius;
    int owner;
} Projectile;

// ---------------------------------------------------------------- weapons
typedef enum {
    WP_FISTS = 0, WP_BAT, WP_AXE, WP_PISTOL, WP_SHOTGUN, WP_RIFLE, WP_SNIPER, WP_CROSSBOW, WP_COUNT
} WeaponId;

typedef enum { AMMO_PISTOL, AMMO_SHELL, AMMO_RIFLE, AMMO_BOLT, AMMO_COUNT } AmmoType;

typedef struct WeaponDef {
    const char *name;
    int kind;             // 0 = melee, 1 = hitscan, 2 = projectile
    int ammo_type;
    float damage;
    float rpm;
    float pellets;
    float spread;
    float range;
    float recoil;
    float reload_time;
    int mag_size;
    float move_speed_mult;
    int	price;
    float noise_radius;
    float penetrate;
    int unlock_night;
    const char *desc;
} WeaponDef;

typedef struct Player {
    int active;
    int index;
    Vec3 pos;
    Vec3 vel;
    float yaw, pitch;
    float height, radius;
    float hp, max_hp;
    float stamina, max_stamina;
    float exhaustion;
    float armor;
    int weapon;                 // current WeaponId
    int owned[WP_COUNT];
    int ammo[AMMO_COUNT];
    int mag[WP_COUNT];
    float fire_cd;
    float reload_t;
    float switch_t;
    float aim_t;                // 0..1 ADS
    float recoil_pitch, recoil_yaw;
    float bob;
    float breath;
    float flashlight_on;
    float flashlight_charge;
    float footstep_t;
    float interact_hint_t;
    int money;
    int scrap;
    int heal_charges;
    int alive;
    float death_time;
    float downed_time;
    int downed;
    float revive_progress;
    float hit_flash;
    float shake;
    float damage_indicator;
    float kill_flash;
    float melee_t;
    int melee_hit_done;
    float sway_x, sway_y;
    float land_kick;
    int reloading;
    float last_damage_time;
    float heartbeat_t;
    float scream_t;
    int last_attacker;
    // net
    NetId net_id;
    float net_send_t;
    float respawn_t;
} Player;

// ---------------------------------------------------------------- game state
typedef enum {
    GS_BOOT, GS_MAIN_MENU, GS_LOADING, GS_DAY, GS_NIGHT, GS_PAUSE, GS_SHOP,
    GS_GAMEOVER, GS_VICTORY, GS_SETTINGS, GS_CONTROLS, GS_NIGHT_INTRO, GS_NIGHT_END, GS_CREDITS, GS_LOBBY
} GameState;

typedef struct NightConfig {
    int night;
    int total_zombies;
    int max_concurrent;
    float spawn_interval;
    float prep_time;         // seconds to prepare (or hits to start)
    float night_duration;    // 0 = survive until all waves dead
    int waves;
    float walker_pct, runner_pct, crawler_pct, brute_pct, screamer_pct;
    int mini_boss, boss;
    int start_money;
    int power;               // whether the store has power that night
    float fog;
    int darkness;            // 0..3 lighting level
    const char *subtitle;
} NightConfig;

typedef struct NetState {
    int mode;                // 0 = single, 1 = host, 2 = client
    int running;
    int port;
    int peer_count;
    float send_timer;
    float ping;
    float rep_delta;         // client: last |predicted - authoritative| correction
    char status[128];
} NetState;

typedef struct World {
    Assets *assets;
    Level level;
    Player players[MAX_PLAYERS];
    int local_player;
    int player_count;
    Zombie zombies[MAX_ZOMBIES];
    int zombie_count;
    Barricade barricades[MAX_BARRICADES];
    Trap traps[8];
    Pickup pickups[MAX_PICKUPS];
    int pickup_count;
    Particle particles[MAX_PARTICLES];
    int particle_count;
    Decal decals[MAX_DECALS];
    Projectile projectiles[MAX_PROJECTILES];
    Rng rng;
    float time;
    float game_time;
    int night;
    NightConfig cfg;
    GameState state;
    float state_time;
    // wave control
    int zombies_remaining;
    int wave_zombies_alive;
    int spawned_this_night;
    float spawn_timer;
    int night_complete;
    int mini_boss_spawned;
    int boss_spawned;
    int boss_dead;
    // economy / progression
    int money;
    int scrap;
    int reputation;
    int kills;
    int headshots;
    int night_kills;
    int deaths;
    float survival_time;
    int perfect_night;
    // feedback
    float damage_flash;
    float heal_flash;
    float hitmarker;
    int hitmarker_kill;
    float kill_notify_t;
    char kill_notify[64];
    char objective[128];
    char subtitle[128];
    float subtitle_t;
    float tips_t;
    char tip[128];
    int show_help;
    // audio
    Audio audio;
    float amb_timer;
    float thunder_timer;
    float stinger_timer;
    float heartbeat_intensity;
    // multiplayer
    NetState net;
    int mouse_sensitivity;
    int invert_y;
    int quality;
    int render_scale;        // 1 = 100%
    int ui_scale;
    int difficulty;
    int save_slot;
    // boss
    float boss_hp_frac;
    int boss_active;
    int shake;
    float time_scale;
    float night_timer;
    int night_phase;         // 0 = prep, 1 = assault, 2 = aftermath
    float gate_health;
    int paused_from;
    float ai_budget;
    // statistics
    int zombies_nav_fail;
    int shots_fired;
    int shots_hit;
} World;

// ---------------------------------------------------------------- shared funcs
void assets_load(Assets *a);
void assets_free(Assets *a);
void level_build(Assets *a, Level *lv);
void level_free(Level *lv);
int  level_point_blocked(const Level *lv, Vec3 p, float radius);
int  level_raycast(const Level *lv, Vec3 from, Vec3 to, float *hit_t, Vec3 *hit_pos);
int  level_raycast_bullet(const Level *lv, Vec3 from, Vec3 to, float *hit_t, Vec3 *hit_nrm);
void nav_build(NavGrid *nav, const Level *lv);
void nav_free(NavGrid *nav);
void nav_update_flow(NavGrid *nav, Vec3 target);
int  nav_walkable(const NavGrid *nav, Vec3 p);
Vec2 nav_flow_dir(const NavGrid *nav, Vec3 p);
int  nav_line_clear(const Level *lv, Vec3 a, Vec3 b, float radius);

#endif
