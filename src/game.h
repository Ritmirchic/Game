// ============================================================================
// THE LAST NIGHT — game module interfaces
// ============================================================================
#ifndef TLN_GAME_H
#define TLN_GAME_H

#include "world.h"
#include "models.h"
#include "weapons.h"

typedef struct MenuItem {
    char label[64];
    char hint[96];
    int enabled;
} MenuItem;

typedef struct Game {
    Platform *plat;
    Renderer *rd;
    Assets *as;
    World *w;
    WeaponModels wm;
    CharModel *zmodel[ZB_TYPE_COUNT][4];
    CharModel *pmodel[4];
    Model *barricade_model[3];
    Model *trap_model[2];
    Model *pickup_model[12];
    Model *door_model[4];
    Model *cart_model;
    Pose zpose[MAX_ZOMBIES];
    Animator zanim[MAX_ZOMBIES];
    Animator panim[MAX_PLAYERS];
    Pose ppose[MAX_PLAYERS];
    Camera cam;
    Mat4 cam_view;
    Environment env;
    Mat4 viewmodels_shadow;
    // ui state
    int menu_sel;
    int menu_count;
    MenuItem menu[16];
    float fade;
    float flash;
    float load_t;
    float hit_flash;
    float heartbeat_phase;
    float ui_time;
    int settings_sel;
    int shop_sel;
    int pending_night;
    char toast[128];
    float toast_t;
    float intro_t;
    int show_map;
    float shake_amt;
    Vec3 shake_dir;
    int low_health_warned;
    float footstep_accum;
    float interact_target_t;
    int interact_index;
    char interact_text[96];
    float day_timer;
    int night_cleared;
    float victory_t;
    float rain_amount;
    float lightning;
    float lightning_t;
    float thunder_delay;
    float title_glitch;
    // network
    int net_is_host;
    float net_tick;
    uint32_t net_seq;
    // frame stats
    float fps_avg;
    int show_debug;
    int flash_boost;
    int settings_from;
    float damage_dir[4];
    float damage_dir_t[4];
    // camera shake / viewmodel
    Vec3 vm_muzzle;
    Vec3 vm_dir;
    float shot_flash;
    float muzzle_light_t;
    float muzzle_light_intensity;
    Vec3 muzzle_light_pos;
    int last_weapon;
    float weapon_switch_anim;
    // zombie animator bookkeeping
    int zanim_state[MAX_ZOMBIES];
} Game;

// ---- lifecycle
void game_init(Game *g, Platform *plat, Renderer *rd, Assets *as);
void game_shutdown(Game *g);
void game_new_run(Game *g);
void game_frame(Game *g, float dt);
void game_render(Game *g);
void ui_draw(Game *g);

void run_tour(Game *g, const char *dir);
void run_net_test(Game *g, int mode, const char *addr, float seconds);

// ---- fx
void fx_update(Game *g, float dt);
void fx_spark(Game *g, Vec3 pos, Vec3 nrm, Vec3 color, int count);
void fx_blood(Game *g, Vec3 pos, Vec3 dir, int count);
void fx_impact(Game *g, Vec3 pos, Vec3 nrm, int material);
void fx_decal(Game *g, Vec3 pos, Vec3 nrm, const Tex *tex, float size, uint32_t color);
void fx_muzzle_flash(Game *g, Vec3 pos);
void fx_smoke(Game *g, Vec3 pos, int count);
void fx_glass(Game *g, Vec3 pos, int count);
void fx_gib(Game *g, Vec3 pos, int count);
void fx_dust(Game *g, Vec3 pos, int count);

// ---- ai
void ai_update(Game *g, float dt);
void ai_spawn_zombie(Game *g, ZombieType type, Vec3 pos, int night_difficulty);
void ai_damage_zombie(Game *g, Zombie *z, float dmg, Vec3 dir, int headshot, int by_player);

// ---- player / combat
void player_spawn(Game *g, Player *p, int index);
void player_update(Game *g, Player *p, float dt);
void combat_trigger(Game *g, Player *p, int held);
int  combat_melee(Game *g, Player *p);
void barricade_place(Game *g, int slot, int level);
void barricade_damage(Game *g, int index, float dmg);
void wave_start_night(Game *g, int night);
void wave_update(Game *g, float dt);
void wave_end_night(Game *g, int survived);
const NightConfig *night_config(int night);

void game_scatter_loot(Game *g, int night);
void game_update_pickups(Game *g, float dt);
void game_update_projectiles(Game *g, float dt);
int  world_point_blocked(Game *g, Vec3 p, float radius);
void game_start_day(Game *g, int night);
void game_shop_buy(Game *g, int index);
int  game_shop_count(void);
const char *game_shop_label(int index, Game *g, char *buf, int n);
const char *game_shop_name(int index);
const char *game_shop_desc(int index);

// ---- interaction
void interact_update(Game *g, Player *p);
void interact_use(Game *g, Player *p);

// ---- audio helper
void sfx(Game *g, Sound *s, Vec3 *pos, float vol, float pitch, int bus);
void sfx_loop(Game *g, Sound *s, float vol);

// ---- net
void net_start(Game *g, int mode, const char *addr);
void net_stop(Game *g);
void net_update(Game *g, float dt);

// ---- save / options
void save_write(Game *g);
int  save_read(Game *g);
void options_defaults(Game *g);


// color helpers
#define COL(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))
#define WHITE 0xFFFFFFFFu
#define BLACK 0xFF000000u
uint32_t col_lerp(uint32_t a, uint32_t b, float t);

#endif
