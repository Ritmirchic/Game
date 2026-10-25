/* ============================================================================
   SPACE MINER — core/state.h
   Shared game state, entity pools and cross-module prototypes.
   ========================================================================== */
#ifndef SM_STATE_H
#define SM_STATE_H
#include "common.h"
#include "gfx.h"
#include "content.h"
#include "platform.h"
#include "pack.h"

#define SM_WORLD_RADIUS 3200.f
#define SM_MAX_ASTEROIDS 200
#define SM_MAX_ENEMIES 40
#define SM_MAX_PROJECTILES 360
#define SM_MAX_PARTICLES 2600
#define SM_MAX_PICKUPS 220
#define SM_MAX_DRONES 6
#define SM_MAX_TEXTS 24
#define SM_MAX_HAZARDS 220

/* ------------------------------------------------------------- entities --- */
typedef struct {
    int active;
    u32 id;
    Vec2 pos, vel;
    f32 angle, spin;
    f32 radius;
    int design;
    int res;
    f32 amount, amount_total;
    f32 hull, hull_max;
    f32 flash;
    f32 crack;
    f32 spawn;
    u32 seed;
    int rich;            /* scanner-boosted content multiplier */
} Asteroid;

typedef struct {
    int active;
    int type;
    Vec2 pos, vel;
    f32 angle, angvel;
    f32 hull, hull_max;
    f32 fire_timer;
    int burst_left;
    f32 ai_timer;
    int ai_state;
    Vec2 strafe_dir;
    f32 hit_flash;
    f32 spawn;
    f32 radius;
    f32 tether;          /* sentinel beam charge */
} Enemy;

enum { PROJ_BOLT = 0, PROJ_PLASMA, PROJ_LANCE, PROJ_ARC, PROJ_MISSILE, PROJ_HEAVY, PROJ_ENEMY_BOLT, PROJ_ENEMY_PLASMA };

typedef struct {
    int active;
    Vec2 pos, vel;
    f32 life, max_life;
    f32 damage;
    f32 radius;
    int kind;
    int from_player;
    int homing;
    f32 color_hue;
    f32 trail_timer;
} Projectile;

typedef struct {
    int active;
    Vec2 pos, vel;
    int res;
    f32 amount;
    f32 life;
    f32 magnet;
    f32 spin, angle;
    f32 scale;
} Pickup;

enum { PT_SPARK, PT_SMOKE, PT_FIRE, PT_DEBRIS, PT_GLOW, PT_RING, PT_STREAK, PT_DUST };

typedef struct {
    int active;
    int kind;
    Vec2 pos, vel;
    f32 life, max_life;
    f32 size, size_end;
    u32 color;
    f32 drag;
    f32 angle, spin;
    BlendMode blend;
    f32 gravity;
} Particle;

typedef struct {
    int active;
    Vec2 pos, vel;
    f32 life, max_life;
    u32 color;
    f32 intensity;
} Hazard;

enum { DRONE_IDLE = 0, DRONE_SEEK, DRONE_MINE, DRONE_RETURN, DRONE_UNLOAD };

typedef struct {
    int active;
    int type;
    Vec2 pos, vel;
    f32 angle;
    int state;
    f32 state_timer;
    u32 target_id;
    Vec2 target_pos;
    f32 mining_progress;
    int cargo_res;
    f32 cargo_amount;
    f32 hit_flash;
    f32 beam_phase;
} Drone;

typedef struct {
    int active;
    Vec2 pos;
    char text[48];
    u32 color;
    f32 life, max_life;
    f32 vy;
    f32 scale;
} FloatText;

/* ------------------------------------------------------------ components -- */
typedef struct {
    Sheet ship[SHIP_COUNT];
    Sheet ship_thrust[SHIP_COUNT];   /* engine flame overlay (additive) */
    Sheet ship_light[SHIP_COUNT];    /* running lights overlay (additive) */
    Sheet enemy[ENEMY_COUNT];
    Sheet enemy_fx[ENEMY_COUNT];
    Sheet drone[DRONE_TYPE_COUNT];
    Sheet ast[8];
    Sheet ast_vein[8];
    Sheet ast_crack[3];
    Sheet bg[SECTOR_COUNT];
    Sheet bg_far[SECTOR_COUNT];
    Sheet station;
    Sheet station_fx;
    Sheet res_icon[RES_COUNT];
    Sheet upg_icon[UPG_COUNT];
    Sheet ach_icon;
    Sheet ui_glow;
    Sheet ui_ring;
    Sheet ui_arrow;
    Sheet ui_lock;
    Sheet ui_cross;
    Sheet ui_warn;
    Sheet ui_drone;
    Sheet ui_ship;
    Sheet ui_cargo;
    Sheet ui_mining;
    Sheet ui_shield;
    Sheet ui_energy;
    Sheet ui_skull;
    Sheet ui_star;
    Sheet title_logo;
    Sheet fx_smoke;
    Sheet fx_spark;
    Sheet fx_glow;
    Sheet fx_flame[4];
    Sheet proj[6];
    Sheet planet[SECTOR_COUNT];
    Sheet fx_chunk[RES_COUNT];
    Font font_tiny, font_small, font_body, font_head, font_title, font_big;
} Assets;

typedef struct {
    f32 master;
    f32 music;
    f32 sfx;
    int muted;
} AudioSettings;

typedef struct {
    int fullscreen;
    int bloom;
    int shake;
    int particles;
    int scanlines;
    f32 ui_scale;
} VideoSettings;

typedef struct {
    int difficulty;      /* 0 chill, 1 normal, 2 hardcore */
    int show_hints;
    int autosave;
} GameplaySettings;

typedef enum {
    SCREEN_BOOT = 0,
    SCREEN_MENU,
    SCREEN_PLAY,
    SCREEN_DOCK,
    SCREEN_PAUSED,
    SCREEN_GAMEOVER,
    SCREEN_CREDITS,
    SCREEN_INTRO
} ScreenId;

typedef enum {
    DOCK_TAB_MARKET = 0,
    DOCK_TAB_UPGRADES,
    DOCK_TAB_SHIPYARD,
    DOCK_TAB_DRONES,
    DOCK_TAB_MAP,
    DOCK_TAB_ACHIEVEMENTS,
    DOCK_TAB_SETTINGS,
    DOCK_TAB_COUNT
} DockTab;

/* --- persistent meta (saved) --- */
typedef struct {
    u32 version;
    s64 credits;
    int xp;
    int level;
    int ship;
    int ship_owned[SHIP_COUNT];
    int weapon;
    int weapon_owned[WEAPON_COUNT];
    int upgrades[UPG_COUNT];
    int sector_unlocked[SECTOR_COUNT];
    int sector_current;
    int drone_owned[DRONE_TYPE_COUNT];
    int drone_count;
    u32 achievements;
    int cargo[RES_COUNT];
    int cargo_used;
    /* lifetime stats */
    int stat_enemies_killed;
    int stat_asteroids_mined;
    s64 stat_credits_earned;
    int stat_crystals_sold;
    f32 stat_time_played;
    f32 stat_distance;
    int stat_sentinels_killed;
    GameplaySettings gameplay;
    AudioSettings audio;
    VideoSettings video;
    /* runtime-only validity */
    int valid;
} SaveData;

typedef struct {
    Vec2 pos, vel;
    f32 angle;
    f32 hull, hull_max;
    f32 shield, shield_max;
    f32 shield_timer;
    f32 shield_flash;
    f32 energy, energy_max;
    f32 cargo_used;
    int cargo[RES_COUNT];
    f32 weapon_cd;
    f32 weapon_cd_max;
    f32 hit_flash;
    f32 thrust;
    f32 mining_heat;
    f32 engine_phase;
    f32 invuln;
    int dead;
    int firing;
    int mining;
    int mining_target;
    Vec2 mining_hit;
    f32 thrust_left, thrust_right;
    f32 roll;             /* visual bank angle */
} Player;

typedef enum {
    BEAM_NONE = 0, BEAM_MINING, BEAM_WEAPON, BEAM_ENEMY
} BeamKind;

typedef struct {
    int active;
    int kind;
    Vec2 a, b;
    f32 life;
    f32 width;
    u32 color;
    f32 jitter;
} Beam;

typedef struct {
    f32 hull_flash;
    f32 shield_ripple;
    Vec2 shield_ripple_pos;
    f32 shake;
    Vec2 shake_offset;
    f32 warp;             /* warp transition 0..1 */
    f32 hitstop;
    f32 danger;
    f32 alert;
    f32 mining_glow;
    u32 alert_enemy;
} CameraFx;

typedef struct {
    Assets assets;
    int assets_loaded;
    ScreenId screen;
    ScreenId screen_prev;
    f32 screen_fade;
    f32 time;
    f32 screen_time;
    u32 frame;
    Rng rng;
    const SmPlatform *plat;
    const Pak *pak;
    Gfx *gfx;

    SaveData save;
    Player player;
    CameraFx cam;

    int sector;
    int sector_entered[SECTOR_COUNT];
    f32 sector_time;
    f32 spawn_timer;
    int station_near;
    f32 station_prompt;

    Asteroid asteroids[SM_MAX_ASTEROIDS];
    Enemy enemies[SM_MAX_ENEMIES];
    Projectile projectiles[SM_MAX_PROJECTILES];
    Particle particles[SM_MAX_PARTICLES];
    Pickup pickups[SM_MAX_PICKUPS];
    Drone drones[SM_MAX_DRONES];
    Hazard hazards[SM_MAX_HAZARDS];
    FloatText texts[SM_MAX_TEXTS];
    Beam beams[16];

    /* background star field / dust */
    Vec2 stars[420];
    f32 star_twinkle[420];
    f32 star_size[420];
    u32 star_color[420];
    int star_count;
    Vec2 dust[260];
    int dust_count;

    /* UI state */
    DockTab dock_tab;
    int dock_sel;
    int dock_scroll;
    int menu_sel;
    int pause_sel;
    int sel_ship;
    int sel_drone;
    int sel_sector;
    int settings_sel;
    int confirm_action;
    f32 ui_anim;
    f32 toast_timer;
    char toast[128];
    u32 toast_color;
    char toast2[128];
    f32 toast2_timer;
    int tooltip_index;
    f32 tooltip_timer;
    int buy_flash;
    int sound_reverb;
    f32 fps_smooth;
    f32 perf_render_ms;
    int show_perf;
    int paused;
    int game_over_timer;
    int pending_sector;

    /* audio handles are platform-managed; game only sends events */
    f32 music_fade;
    int music_track;
    f32 ambience_timer;
    f32 radio_timer;

    /* cumulative per-run counters */
    int run_kills;
    f32 scan_pulse;
    f32 hazard_timer;
} Game;

/* ---- lifecycle ---- */
Game *game_create(const SmPlatform *plat, const Pak *pak, Gfx *gfx);
void game_destroy(Game *g);
void game_update(Game *g, Input *in, f32 dt);
void game_render(Game *g);
int  game_wants_quit(Game *g);

/* ---- state transitions ---- */
void game_new_session(Game *g);
void game_set_screen(Game *g, ScreenId s);
void game_toast(Game *g, const char *msg, u32 color);
void game_toast2(Game *g, const char *msg, u32 color);

/* ---- content helpers ---- */
void assets_load(Game *g);
void assets_free(Game *g);

/* world */
void world_enter_sector(Game *g, int sector);
void world_spawn_asteroid(Game *g, Vec2 pos, int design, int res, f32 radius, f32 amount);
void world_spawn_enemy(Game *g, int type, Vec2 pos);
void world_update(Game *g, f32 dt);
void world_render(Game *g);
int  world_find_asteroid_near(Game *g, Vec2 p, f32 maxdist);
void world_clear_entities(Game *g);
void world_release_ore(Game *g, Asteroid *a, f32 amount);
void world_populate(Game *g);
void world_render_background(Game *g);

/* player / systems */
void player_reset(Game *g, int keep_progress);
void player_update(Game *g, Input *in, f32 dt);
void player_render(Game *g);
void player_apply_loadout(Game *g);
f32 player_stat_speed(const Game *g);
f32 player_stat_accel(const Game *g);
f32 player_stat_laser_dps(const Game *g);
f32 player_stat_weapon_damage(const Game *g);
f32 player_stat_weapon_rate(const Game *g);
f32 player_stat_turn(const Game *g);
f32 player_stat_pickup_radius(const Game *g);
int player_stat_cargo_max(const Game *g);
int player_stat_drone_slots(const Game *g);
void player_add_cargo(Game *g, int res, f32 amount);
int  player_cargo_space(Game *g);
void player_damage(Game *g, f32 amount, Vec2 from);
void player_gain_xp(Game *g, int xp);
void player_check_level(Game *g);

/* combat */
void combat_fire_weapon(Game *g, Vec2 origin, f32 angle);
void combat_spawn_projectile(Game *g, Vec2 pos, Vec2 vel, f32 dmg, int kind, int from_player);
void combat_update(Game *g, f32 dt);
void combat_render(Game *g);
void enemy_damage(Game *g, int idx, f32 amount, Vec2 hit_pos);
void asteroid_damage(Game *g, int idx, f32 amount, Vec2 hit_pos, f32 dt);

/* drones */
void drones_update(Game *g, f32 dt);
void drones_render(Game *g);
void drone_buy(Game *g, int type);

/* fx */
void fx_init(Game *g);
void fx_clear(Game *g);
void fx_update(Game *g, f32 dt);
void fx_render_back(Game *g);
void fx_render_front(Game *g);
void fx_spark(Game *g, Vec2 pos, Vec2 vel, u32 color, int count, f32 speed);
void fx_explosion(Game *g, Vec2 pos, f32 radius, u32 color, int big);
void fx_smoke(Game *g, Vec2 pos, Vec2 vel, u32 color);
void fx_debris(Game *g, Vec2 pos, Vec2 base_vel, int count, u32 color);
void fx_ring(Game *g, Vec2 pos, f32 radius, u32 color, f32 life);
void fx_beam(Game *g, Vec2 a, Vec2 b, f32 width, u32 color, f32 life, int kind);
void fx_text(Game *g, Vec2 pos, const char *text, u32 color);
void fx_impact(Game *g, Vec2 pos, Vec2 normal, u32 color);
void fx_shockwave(Game *g, Vec2 pos, f32 radius, u32 color);
void fx_glow(Game *g, Vec2 pos, f32 radius, u32 color, f32 life);

/* ui */
void ui_render(Game *g, Input *in);
void ui_render_hud(Game *g);
void ui_render_menu(Game *g, Input *in);
void ui_render_dock(Game *g, Input *in);
void ui_render_pause(Game *g, Input *in);
void ui_render_gameover(Game *g, Input *in);
void ui_render_intro(Game *g, Input *in);
void ui_render_boot(Game *g, Input *in);
void ui_update(Game *g, Input *in, f32 dt);
void ui_dock_action(Game *g, int tab, int index);
void ui_open_dock(Game *g);
int  ui_handle_modal(Game *g, Input *in);

/* save */
int  save_write(Game *g);
int  save_read(Game *g);
void save_apply_defaults(Game *g);
int  save_validate(SaveData *d);

/* audio hooks implemented by platform */
void snd_play(Game *g, const char *name, f32 volume, f32 pan, f32 pitch);
void snd_music(Game *g, const char *name, f32 fade);
void snd_stop_music(Game *g, f32 fade);

/* achievements */
void ach_check(Game *g);
void ach_unlock(Game *g, int id);

#endif
