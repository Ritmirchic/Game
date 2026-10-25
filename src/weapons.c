// Weapon models (first-person view models + world pickups) and fire behaviour data
#include "weapons.h"
#include <string.h>

const WeaponDef g_weapon_defs[WP_COUNT] = {
    // name        kind ammo  dmg    rpm  pellets spread range recoil reload mag speed price noise pen unlock
    {"FISTS",      0, -1,   22.f,  140, 1, 0.00f, 1.9f, 0.02f, 0.0f, 0, 1.00f, 0, 0.4f, 0, 1,
     "Bare hands. Quiet, desperate, and surprisingly effective against a single walker."},
    {"BAT",        0, -1,   48.f,   95, 1, 0.00f, 2.4f, 0.06f, 0.0f, 0, 0.97f, 120, 0.6f, 0, 1,
     "Aluminium little-league bat. Dents skulls; will not stop a Brute quickly."},
    {"AXE",        0, -1,   92.f,   62, 1, 0.00f, 2.6f, 0.10f, 0.0f, 0, 0.92f, 320, 0.7f, 0, 2,
     "Fire axe from the maintenance room. Heavy, slow, and it keeps its edge."},
    {"PISTOL",     1, AMMO_PISTOL, 26.f, 320, 1, 0.021f, 55.f, 0.030f, 1.35f, 12, 1.00f, 260, 26.f, 0, 1,
     "9mm service pistol. Reliable fallback, ammunition is everywhere."},
    {"SHOTGUN",    1, AMMO_SHELL, 17.f, 82, 8, 0.075f, 22.f, 0.115f, 2.3f, 6, 0.94f, 900, 40.f, 1, 2,
     "Pump-action 12 gauge. Clears a doorway; reload it before the doorway comes back."},
    {"RIFLE",      1, AMMO_RIFLE, 33.f, 640, 1, 0.030f, 60.f, 0.026f, 1.9f, 30, 0.97f, 2400, 34.f, 1, 4,
     "Assault rifle with a red-dot. Best all-round answer to a crowd."},
    {"SNIPER",     1, AMMO_RIFLE, 145.f, 48, 1, 0.003f, 120.f, 0.180f, 2.7f, 5, 0.88f, 3200, 48.f, 3, 5,
     "Hunting rifle, scoped. Punches through a line of walkers. Loud."},
    {"CROSSBOW",   2, AMMO_BOLT, 190.f, 52, 1, 0.006f, 90.f, 0.075f, 2.9f, 1, 0.95f, 1600, 5.f, 2, 3,
     "Compound crossbow. The only truly quiet killer here; bolts are retrievable."},
};

const char *ammo_name(int type) {
    switch (type) {
        case AMMO_PISTOL: return "9mm";
        case AMMO_SHELL: return "12GA";
        case AMMO_RIFLE: return "5.56";
        case AMMO_BOLT: return "BOLTS";
    }
    return "AMMO";
}
int ammo_max(int type) {
    switch (type) {
        case AMMO_PISTOL: return AMMO_PISTOL_MAX;
        case AMMO_SHELL: return AMMO_SHELL_MAX;
        case AMMO_RIFLE: return AMMO_RIFLE_MAX;
        case AMMO_BOLT: return AMBOLT_MAX;
    }
    return 100;
}

// ---------------------------------------------------------------- builders
static void add(Mesh *ms, Vec3 mn, Vec3 mx, float uv) { mesh_add_boxf(ms, mn, mx, 255, 255, 255, uv); }

static Model *make_pistol(int viewmodel) {
    Model *m = model_new();
    Mesh *metal = model_part(m, MAT_GUN_METAL, 900);
    Mesh *poly = model_part(m, MAT_GUN_POLY, 400);
    float s = viewmodel ? 1.0f : 1.0f;
    // frame + slide (pointing +Z)
    add(metal, v3(-0.022f * s, 0.0f, 0.02f), v3(0.022f * s, 0.036f * s, 0.24f * s), 6.f);
    add(metal, v3(-0.019f * s, 0.036f * s, 0.03f), v3(0.019f * s, 0.055f * s, 0.225f * s), 6.f);
    // slide serrations
    for (int i = 0; i < 5; i++) {
        float z = 0.05f + i * 0.012f;
        add(metal, v3(-0.020f * s, 0.037f * s, z * s), v3(0.020f * s, 0.054f * s, (z + 0.005f) * s), 8.f);
    }
    // barrel
    mesh_cyl_axis(metal, v3(0, 0.046f * s, 0.22f * s), v3(0, 0.046f * s, 0.252f * s), 0.010f * s, 0.009f * s, 10, 255, 255, 255, 8.f, 1);
    // grip
    for (int i = 0; i < 6; i++) {
        float t = (float)i / 5.f;
        add(poly, v3(-0.019f * s, (-0.02f - t * 0.09f) * s, (0.01f + t * 0.035f)), v3(0.019f * s, (-0.015f - t * 0.085f) * s, (0.055f + t * 0.04f) * s), 6.f);
    }
    // trigger guard
    add(poly, v3(-0.012f * s, -0.018f * s, 0.062f * s), v3(0.012f * s, 0.004f * s, 0.10f * s), 8.f);
    add(metal, v3(-0.004f * s, -0.012f * s, 0.075f * s), v3(0.004f * s, 0.006f * s, 0.082f * s), 10.f);
    // sights
    add(metal, v3(-0.004f * s, 0.055f * s, 0.205f * s), v3(0.004f * s, 0.064f * s, 0.215f * s), 10.f);
    add(metal, v3(-0.010f * s, 0.055f * s, 0.035f * s), v3(-0.004f * s, 0.062f * s, 0.045f * s), 10.f);
    add(metal, v3(0.004f * s, 0.055f * s, 0.035f * s), v3(0.010f * s, 0.062f * s, 0.045f * s), 10.f);
    model_final(m);
    return m;
}

static Model *make_shotgun(int viewmodel) {
    Model *m = model_new();
    Mesh *metal = model_part(m, MAT_GUN_METAL, 1200);
    Mesh *wood = model_part(m, MAT_WOOD_STOCK, 700);
    Mesh *poly = model_part(m, MAT_GUN_POLY, 300);
    (void)viewmodel;
    // receiver
    add(metal, v3(-0.028f, 0.0f, 0.10f), v3(0.028f, 0.062f, 0.40f), 4.f);
    // barrel
    mesh_cyl_axis(metal, v3(0, 0.040f, 0.40f), v3(0, 0.040f, 0.86f), 0.016f, 0.015f, 12, 255, 255, 255, 4.f, 1);
    // mag tube
    mesh_cyl_axis(metal, v3(0, 0.012f, 0.40f), v3(0, 0.012f, 0.80f), 0.014f, 0.014f, 12, 255, 255, 255, 4.f, 1);
    // pump
    add(poly, v3(-0.030f, -0.010f, 0.52f), v3(0.030f, 0.036f, 0.68f), 5.f);
    for (int i = 0; i < 6; i++) {
        float z = 0.53f + i * 0.025f;
        add(poly, v3(-0.033f, -0.014f, z), v3(0.033f, 0.040f, z + 0.012f), 8.f);
    }
    // stock
    for (int i = 0; i < 6; i++) {
        float t = (float)i / 5.f;
        add(wood, v3((-0.026f - t * 0.008f), (0.004f - t * 0.012f), (0.10f - t * 0.24f)),
            v3((0.026f + t * 0.008f), (0.062f - t * 0.026f), (0.14f - t * 0.20f)), 3.f);
    }
    // grip / trigger housing
    add(poly, v3(-0.020f, -0.055f, 0.16f), v3(0.020f, 0.000f, 0.24f), 6.f);
    add(metal, v3(-0.006f, -0.06f, 0.20f), v3(0.006f, 0.0f, 0.215f), 10.f);
    // bead sight
    add(metal, v3(-0.004f, 0.055f, 0.845f), v3(0.004f, 0.062f, 0.855f), 10.f);
    model_final(m);
    return m;
}

static Model *make_rifle(int viewmodel) {
    Model *m = model_new();
    Mesh *metal = model_part(m, MAT_GUN_METAL, 1600);
    Mesh *poly = model_part(m, MAT_GUN_POLY, 800);
    Mesh *red = model_part(m, MAT_EMISSIVE_RED, 100);
    (void)viewmodel;
    add(metal, v3(-0.026f, 0.0f, 0.06f), v3(0.026f, 0.070f, 0.36f), 4.f);
    // handguard with vents
    add(poly, v3(-0.028f, 0.010f, 0.36f), v3(0.028f, 0.060f, 0.62f), 4.f);
    for (int i = 0; i < 5; i++) {
        float z = 0.39f + i * 0.045f;
        add(metal, v3(-0.030f, 0.028f, z), v3(-0.024f, 0.050f, z + 0.02f), 8.f);
        add(metal, v3(0.024f, 0.028f, z), v3(0.030f, 0.050f, z + 0.02f), 8.f);
    }
    // barrel + muzzle brake
    mesh_cyl_axis(metal, v3(0, 0.036f, 0.62f), v3(0, 0.036f, 0.86f), 0.011f, 0.011f, 10, 255, 255, 255, 6.f, 1);
    mesh_cyl_axis(metal, v3(0, 0.036f, 0.86f), v3(0, 0.036f, 0.90f), 0.017f, 0.017f, 10, 255, 255, 255, 8.f, 1);
    add(metal, v3(-0.012f, 0.052f, 0.66f), v3(0.012f, 0.075f, 0.70f), 8.f);   // front sight block
    // optic
    add(metal, v3(-0.016f, 0.070f, 0.20f), v3(0.016f, 0.105f, 0.30f), 6.f);
    add(red, v3(-0.006f, 0.080f, 0.302f), v3(0.006f, 0.095f, 0.305f), 8.f);
    // magazine (curved)
    for (int i = 0; i < 5; i++) {
        float t = (float)i / 4.f;
        add(metal, v3(-0.018f, (-0.03f - t * 0.115f), (0.19f + t * 0.03f)), v3(0.018f, (0.005f - t * 0.115f), (0.255f + t * 0.03f)), 5.f);
    }
    // pistol grip + trigger
    add(poly, v3(-0.018f, -0.07f, 0.11f), v3(0.018f, 0.0f, 0.17f), 6.f);
    add(metal, v3(-0.005f, -0.05f, 0.15f), v3(0.005f, 0.0f, 0.16f), 10.f);
    // stock
    add(poly, v3(-0.022f, 0.020f, 0.00f), v3(0.022f, 0.062f, 0.09f), 4.f);
    add(poly, v3(-0.024f, -0.02f, -0.10f), v3(0.024f, 0.062f, 0.02f), 4.f);
    add(poly, v3(-0.030f, 0.000f, -0.14f), v3(0.030f, 0.070f, -0.10f), 5.f);
    model_final(m);
    return m;
}

static Model *make_sniper(int viewmodel) {
    Model *m = model_new();
    Mesh *metal = model_part(m, MAT_GUN_METAL, 1200);
    Mesh *wood = model_part(m, MAT_WOOD_STOCK, 800);
    Mesh *lens = model_part(m, MAT_EMISSIVE_COOL, 100);
    (void)viewmodel;
    add(metal, v3(-0.024f, 0.0f, 0.12f), v3(0.024f, 0.065f, 0.42f), 4.f);
    // long barrel
    mesh_cyl_axis(metal, v3(0, 0.033f, 0.42f), v3(0, 0.033f, 1.00f), 0.012f, 0.011f, 12, 255, 255, 255, 6.f, 1);
    mesh_cyl_axis(metal, v3(0, 0.033f, 1.0f), v3(0, 0.033f, 1.05f), 0.016f, 0.016f, 10, 255, 255, 255, 8.f, 1);
    // scope with rings
    mesh_cyl_axis(metal, v3(0, 0.098f, 0.16f), v3(0, 0.098f, 0.40f), 0.028f, 0.028f, 14, 255, 255, 255, 6.f, 1);
    mesh_cyl_axis(metal, v3(0, 0.098f, 0.40f), v3(0, 0.098f, 0.45f), 0.034f, 0.034f, 14, 255, 255, 255, 6.f, 1);
    mesh_cyl_axis(lens, v3(0, 0.098f, 0.40f), v3(0, 0.098f, 0.405f), 0.026f, 0.026f, 14, 255, 255, 255, 6.f, 1);
    add(metal, v3(-0.010f, 0.065f, 0.20f), v3(0.010f, 0.095f, 0.23f), 8.f);
    add(metal, v3(-0.010f, 0.065f, 0.36f), v3(0.010f, 0.095f, 0.39f), 8.f);
    // bolt handle
    mesh_cyl_axis(metal, v3(0.024f, 0.045f, 0.30f), v3(0.055f, 0.030f, 0.28f), 0.007f, 0.007f, 6, 255, 255, 255, 8.f, 1);
    mesh_sphere(metal, v3(0.058f, 0.029f, 0.279f), 0.012f, 8, 6, 255, 255, 255, 8.f);
    // wooden stock
    for (int i = 0; i < 7; i++) {
        float t = (float)i / 6.f;
        add(wood, v3((-0.028f - t * 0.006f), (0.006f - t * 0.014f), (0.14f - t * 0.32f)),
            v3((0.028f + t * 0.006f), (0.062f - t * 0.02f), (0.18f - t * 0.28f)), 3.f);
    }
    add(wood, v3(-0.022f, -0.06f, 0.20f), v3(0.022f, 0.0f, 0.26f), 5.f);
    add(metal, v3(-0.005f, -0.05f, 0.235f), v3(0.005f, 0.0f, 0.245f), 10.f);
    // bipod
    mesh_cyl_axis(metal, v3(-0.02f, 0.0f, 0.75f), v3(-0.06f, -0.10f, 0.80f), 0.006f, 0.005f, 5, 255, 255, 255, 8.f, 1);
    mesh_cyl_axis(metal, v3(0.02f, 0.0f, 0.75f), v3(0.06f, -0.10f, 0.80f), 0.006f, 0.005f, 5, 255, 255, 255, 8.f, 1);
    model_final(m);
    return m;
}

static Model *make_crossbow(int viewmodel) {
    Model *m = model_new();
    Mesh *wood = model_part(m, MAT_WOOD_STOCK, 800);
    Mesh *metal = model_part(m, MAT_STEEL, 500);
    Mesh *string = model_part(m, MAT_RUBBER, 200);
    (void)viewmodel;
    // stock
    add(wood, v3(-0.030f, -0.02f, -0.12f), v3(0.030f, 0.030f, 0.44f), 3.f);
    add(wood, v3(-0.026f, -0.06f, 0.02f), v3(0.026f, 0.020f, 0.16f), 4.f);
    // limbs
    mesh_cyl_axis(wood, v3(-0.30f, 0.02f, 0.36f), v3(-0.05f, 0.02f, 0.40f), 0.012f, 0.018f, 8, 255, 255, 255, 3.f, 1);
    mesh_cyl_axis(wood, v3(0.30f, 0.02f, 0.36f), v3(0.05f, 0.02f, 0.40f), 0.012f, 0.018f, 8, 255, 255, 255, 3.f, 1);
    mesh_cyl_axis(wood, v3(-0.31f, 0.02f, 0.355f), v3(-0.33f, 0.02f, 0.30f), 0.012f, 0.010f, 6, 255, 255, 255, 3.f, 1);
    mesh_cyl_axis(wood, v3(0.31f, 0.02f, 0.355f), v3(0.33f, 0.02f, 0.30f), 0.012f, 0.010f, 6, 255, 255, 255, 3.f, 1);
    // string
    mesh_cyl_axis(string, v3(-0.32f, 0.02f, 0.31f), v3(0, 0.02f, 0.12f), 0.004f, 0.004f, 4, 255, 255, 255, 8.f, 1);
    mesh_cyl_axis(string, v3(0.32f, 0.02f, 0.31f), v3(0, 0.02f, 0.12f), 0.004f, 0.004f, 4, 255, 255, 255, 8.f, 1);
    // rail + bolt
    add(metal, v3(-0.012f, 0.030f, 0.10f), v3(0.012f, 0.038f, 0.42f), 5.f);
    mesh_cyl_axis(metal, v3(0, 0.045f, 0.10f), v3(0, 0.045f, 0.46f), 0.005f, 0.004f, 6, 255, 255, 255, 8.f, 1);
    // foregrip + trigger
    add(wood, v3(-0.020f, -0.03f, 0.24f), v3(0.020f, 0.02f, 0.30f), 6.f);
    add(metal, v3(-0.005f, -0.05f, 0.16f), v3(0.005f, 0.0f, 0.17f), 10.f);
    // scope rail
    add(metal, v3(-0.012f, 0.038f, 0.10f), v3(0.012f, 0.060f, 0.22f), 8.f);
    model_final(m);
    return m;
}

static Model *make_bat(void) {
    Model *m = model_new();
    Mesh *wood = model_part(m, MAT_BAT, 900);
    // tapered barrel along +Z
    mesh_cyl_axis(wood, v3(0, 0, 0.0f), v3(0, 0, 0.22f), 0.016f, 0.020f, 10, 255, 255, 255, 2.f, 1);
    mesh_cyl_axis(wood, v3(0, 0, 0.22f), v3(0, 0, 0.62f), 0.020f, 0.038f, 12, 255, 255, 255, 2.f, 1);
    mesh_sphere(wood, v3(0, 0, 0.64f), 0.040f, 12, 7, 255, 255, 255, 2.f);
    // grip tape
    Mesh *tape = model_part(m, MAT_RUBBER, 400);
    for (int i = 0; i < 5; i++) {
        float z = 0.02f + i * 0.035f;
        mesh_cyl_axis(tape, v3(0, 0, z), v3(0, 0, z + 0.025f), 0.0185f, 0.0185f, 10, 255, 255, 255, 3.f, 0);
    }
    model_final(m);
    return m;
}

static Model *make_axe(void) {
    Model *m = model_new();
    Mesh *handle = model_part(m, MAT_WOOD_STOCK, 700);
    Mesh *head = model_part(m, MAT_AXE, 700);
    mesh_cyl_axis(handle, v3(0, 0, -0.10f), v3(0, 0, 0.66f), 0.020f, 0.017f, 10, 255, 255, 255, 2.f, 1);
    // head: bit + poll
    add(head, v3(-0.012f, -0.075f, 0.60f), v3(0.012f, 0.075f, 0.66f), 3.f);
    Vec3 a = v3(0.012f, -0.070f, 0.60f), b = v3(0.012f, 0.070f, 0.60f);
    Vec3 c = v3(0.15f, 0.115f, 0.655f), d = v3(0.15f, -0.115f, 0.655f);
    mesh_quad_uv(head, a, b, c, d, v3(0, 0, 1), 0, 1, 1, 0, 255, 255, 255);
    Vec3 e = v3(0.16f, 0.10f, 0.645f), f = v3(0.16f, -0.10f, 0.645f);
    mesh_add_quad(head, a, c, e, f, v3(0, -1, 0), 1, 1, 200, 200, 200);
    mesh_add_quad(head, b, d, f, e, v3(0, 1, 0), 1, 1, 200, 200, 200);
    add(head, v3(-0.05f, -0.04f, 0.61f), v3(-0.012f, 0.04f, 0.65f), 4.f);
    model_final(m);
    return m;
}

void weapons_build(WeaponModels *wm) {
    memset(wm, 0, sizeof *wm);
    wm->view[WP_PISTOL] = make_pistol(1);
    wm->view[WP_SHOTGUN] = make_shotgun(1);
    wm->view[WP_RIFLE] = make_rifle(1);
    wm->view[WP_SNIPER] = make_sniper(1);
    wm->view[WP_CROSSBOW] = make_crossbow(1);
    wm->view[WP_BAT] = make_bat();
    wm->view[WP_AXE] = make_axe();
    wm->world[WP_PISTOL] = make_pistol(0);
    wm->world[WP_SHOTGUN] = make_shotgun(0);
    wm->world[WP_RIFLE] = make_rifle(0);
    wm->world[WP_SNIPER] = make_sniper(0);
    wm->world[WP_CROSSBOW] = make_crossbow(0);

    // view-space anchor: right hand, lower right of screen
    for (int i = 0; i < WP_COUNT; i++) {
        wm->grip_offset[i] = v3(0.30f, -0.30f, -0.62f);
        wm->ads_offset[i] = v3(0.0f, -0.10f, -0.45f);
        wm->view_scale[i] = 1.0f;
        wm->muzzle_view[i] = v3(0, 0, 0.9f);
    }
    wm->grip_offset[WP_PISTOL] = v3(0.26f, -0.25f, -0.60f);
    wm->grip_offset[WP_SHOTGUN] = v3(0.30f, -0.30f, -0.66f);
    wm->grip_offset[WP_RIFLE] = v3(0.31f, -0.30f, -0.62f);
    wm->grip_offset[WP_SNIPER] = v3(0.32f, -0.31f, -0.62f);
    wm->grip_offset[WP_CROSSBOW] = v3(0.28f, -0.30f, -0.66f);
    wm->grip_offset[WP_BAT] = v3(0.30f, -0.34f, -0.46f);
    wm->grip_offset[WP_AXE] = v3(0.30f, -0.37f, -0.44f);
    wm->view_scale[WP_BAT] = 1.10f;
    wm->view_scale[WP_AXE] = 1.05f;
    wm->view_scale[WP_CROSSBOW] = 1.00f;
    wm->muzzle_view[WP_PISTOL] = v3(0, 0.046f, 0.25f);
    wm->muzzle_view[WP_SHOTGUN] = v3(0, 0.04f, 0.88f);
    wm->muzzle_view[WP_RIFLE] = v3(0, 0.036f, 0.91f);
    wm->muzzle_view[WP_SNIPER] = v3(0, 0.033f, 1.06f);
    wm->muzzle_view[WP_CROSSBOW] = v3(0, 0.045f, 0.46f);
    wm->muzzle_view[WP_BAT] = v3(0, 0, 0.68f);
    wm->muzzle_view[WP_AXE] = v3(0.15f, 0, 0.66f);
    wm->ads_offset[WP_RIFLE] = v3(-0.16f, -0.20f, -0.42f);
    wm->ads_offset[WP_SNIPER] = v3(-0.16f, -0.20f, -0.42f);
    wm->ads_offset[WP_PISTOL] = v3(-0.14f, -0.18f, -0.40f);
    wm->ads_offset[WP_SHOTGUN] = v3(0.0f, -0.14f, -0.46f);
    wm->ads_offset[WP_CROSSBOW] = v3(0.0f, -0.14f, -0.46f);
}
void weapons_free(WeaponModels *wm) {
    for (int i = 0; i < WP_COUNT; i++) {
        model_free(wm->view[i]);
        model_free(wm->world[i]);
    }
    memset(wm, 0, sizeof *wm);
}

Mat4 weapon_view_xform(const Player *p, const WeaponModels *wm, Vec3 camera_pos, Vec3 *out_muzzle, Vec3 *out_dir, float time) {
    // camera space -> world
    Mat4 cam_rot = mat4_mul(mat4_rot_x(-(p->pitch)), mat4_rot_y(-(p->yaw + (float)M_PI)));
    Mat4 cam = mat4_mul(mat4_translate(camera_pos), mat4_transpose(cam_rot));
    int w = p->weapon;
    Vec3 pos = wm->grip_offset[w];
    // sprint/aim blend
    float aim = p->aim_t;
    pos = vlerp(pos, wm->ads_offset[w], aim);
    // idle sway and bob
    float bob = p->bob;
    float speed = vlen(v3(p->vel.x, 0, p->vel.z));
    float bob_amt = saturatef(speed / 5.0f) * (1.f - aim * 0.7f);
    pos.x += sinf(bob * 2.f) * 0.010f * bob_amt;
    pos.y += sinf(bob * 4.f) * 0.008f * bob_amt - 0.004f * bob_amt;
    pos.x += p->sway_x * 6.f;
    pos.y -= p->sway_y * 5.f;
    // recoil kick
    pos.z -= p->recoil_pitch * 0.25f;
    float reload = 0.f;
    if (p->reloading) {
        float rt = g_weapon_defs[w].reload_time;
        reload = saturatef(1.f - p->reload_t / maxf(rt, 0.01f));
    }
    // reload animation: dip the weapon and tilt
    float rl = sinf(reload * (float)M_PI);
    pos.y -= rl * 0.16f;
    pos.z -= rl * 0.05f;
    float rot_reload = rl * 0.9f;
    // melee swing
    float swing = 0.f;
    if (g_weapon_defs[w].kind == 0 && p->melee_t > 0.f) {
        float mt = 1.f - saturatef(p->melee_t / 0.42f);
        swing = sinf(mt * (float)M_PI) * (mt < 0.55f ? mt / 0.55f : 1.f);
    }
    float switchdip = saturatef(p->switch_t / 0.28f) * 0.22f;
    pos.y -= switchdip;
    pos.y -= swing * 0.10f;
    pos.x -= swing * 0.16f;
    Mat4 local = mat4_mul(mat4_translate(pos), mat4_rot_y((float)M_PI + 0.34f + rot_reload * 0.5f + swing * 0.9f));
    local = mat4_mul(local, mat4_rot_x(-0.03f + swing * 0.5f - reload * 0.3f));
    local = mat4_mul(local, mat4_rot_z(0.10f));
    local = mat4_mul(local, mat4_rot_z(swing * -0.6f + rl * 0.4f));
    local = mat4_mul(local, mat4_scale(v3s(wm->view_scale[w])));
    Mat4 world = mat4_mul(cam, local);
    if (out_muzzle) {
        Vec3 mv = wm->muzzle_view[w];
        *out_muzzle = mat4_mul_point(world, mv);
    }
    if (out_dir) {
        Vec3 a = mat4_mul_point(world, wm->muzzle_view[w]);
        Vec3 b = mat4_mul_point(world, v3(0, 0, 0));
        *out_dir = vnorm(vsub(a, b));
    }
    return world;
}
