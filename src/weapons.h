// ============================================================================
// THE LAST NIGHT — weapons: definitions, view models, animation state
// ============================================================================
#ifndef TLN_WEAPONS_H
#define TLN_WEAPONS_H

#include "world.h"
#include "models.h"

#define AMMO_PISTOL_MAX 240
#define AMMO_SHELL_MAX 90
#define AMMO_RIFLE_MAX 360
#define AMBOLT_MAX 40

typedef struct WeaponModels {
    Model *view[WP_COUNT];
    Model *world[WP_COUNT];
    Vec3 muzzle_view[WP_COUNT];
    Vec3 grip_offset[WP_COUNT];
    Vec3 ads_offset[WP_COUNT];
    float view_scale[WP_COUNT];
} WeaponModels;

extern const WeaponDef g_weapon_defs[WP_COUNT];

void weapons_build(WeaponModels *wm);
void weapons_free(WeaponModels *wm);
// returns model matrix for the view model in world space
Mat4 weapon_view_xform(const Player *p, const WeaponModels *wm, Vec3 camera_pos, Vec3 *out_muzzle_world, Vec3 *out_flash_dir, float time);
const char *ammo_name(int type);
int ammo_max(int type);

#endif
