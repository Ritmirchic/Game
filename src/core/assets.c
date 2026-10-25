/* ============================================================================
   SPACE MINER — core/assets.c
   Loads every sheet, atlas and font from the packed asset blob.
   ========================================================================== */
#include "state.h"

static int load_sheet(Game *g, Sheet *out, const char *name, int expect_frames) {
    if (pak_load_sheet(g->pak, name, out) != 0) {
        sm_errorf("asset missing: %s", name);
        return -1;
    }
    if (expect_frames > 1 && out->frames != expect_frames) {
        sm_warnf("%s: expected %d frames, got %d", name, expect_frames, out->frames);
    }
    return 0;
}

static void load_font(Game *g, Font *f, const char *name) {
    Sheet atlas;
    if (pak_load_sheet(g->pak, name, &atlas) != 0) {
        sm_errorf("font atlas missing: %s", name);
        return;
    }
    char meta_name[64];
    sm_sprintf(meta_name, sizeof(meta_name), "%s_meta", name);
    const char *meta = pak_blob_str(g->pak, meta_name);
    if (!meta) {
        sm_errorf("font meta missing: %s", meta_name);
        return;
    }
    gfx_font_load(f, &atlas, meta);
}

void assets_load(Game *g) {
    Assets *a = &g->assets;
    memset(a, 0, sizeof(*a));

    static const char *ships[] = { "ship_prospector", "ship_boreas", "ship_vanguard", "ship_chimera" };
    for (int i = 0; i < SHIP_COUNT; i++) load_sheet(g, &a->ship[i], ships[i], 32);
    static const char *enemies[] = { "enemy_scout", "enemy_raider", "enemy_heavy", "enemy_hunter", "enemy_sentinel" };
    for (int i = 0; i < ENEMY_COUNT; i++) load_sheet(g, &a->enemy[i], enemies[i], 32);
    static const char *drones[] = { "drone_harvester", "drone_excavator" };
    for (int i = 0; i < DRONE_TYPE_COUNT; i++) load_sheet(g, &a->drone[i], drones[i], 32);

    for (int i = 0; i < 8; i++) {
        char n[24];
        sm_sprintf(n, sizeof(n), "ast_%d", i);
        load_sheet(g, &a->ast[i], n, 16);
        a->ast_vein[i].w = 0;
    }
    for (int i = 0; i < 3; i++) {
        char n[24];
        sm_sprintf(n, sizeof(n), "ast_crack_%d", i);
        load_sheet(g, &a->ast_crack[i], n, 1);
    }
    for (int i = 0; i < SECTOR_COUNT; i++) {
        char n[24];
        sm_sprintf(n, sizeof(n), "bg_nebula%d", i);
        load_sheet(g, &a->bg_far[i], n, 1);
        sm_sprintf(n, sizeof(n), "sky_%d", i);
        load_sheet(g, &a->bg[i], n, 1);
    }
    load_sheet(g, &a->station, "station", 1);

    static const char *res_icons[RES_COUNT] = {
        "res_iron", "res_copper", "res_nickel", "res_ice", "res_titanium", "res_silver",
        "res_salvage", "res_gold", "res_iridium", "res_platinum", "res_uranium", "res_crystal",
        "res_plasma", "res_antimatter", "res_voidcrystal", "res_darkmatter"
    };
    for (int i = 0; i < RES_COUNT; i++) load_sheet(g, &a->res_icon[i], res_icons[i], 1);

    static const char *upg_icons[UPG_COUNT] = {
        "upg_laser", "upg_speed", "upg_cargo", "upg_engine", "upg_accel", "upg_hull",
        "upg_shield", "upg_energy", "upg_scanner", "upg_drone", "upg_weapon", "upg_tractor"
    };
    for (int i = 0; i < UPG_COUNT; i++) load_sheet(g, &a->upg_icon[i], upg_icons[i], 1);

    load_sheet(g, &a->ui_drone, "ui_drone", 1);
    load_sheet(g, &a->ui_ship, "ui_ship", 1);
    load_sheet(g, &a->ui_cargo, "ui_cargo", 1);
    load_sheet(g, &a->ui_mining, "ui_mining", 1);
    load_sheet(g, &a->ui_shield, "ui_shield", 1);
    load_sheet(g, &a->ui_energy, "ui_energy", 1);
    load_sheet(g, &a->ui_skull, "ui_skull", 1);
    load_sheet(g, &a->ui_star, "ui_star", 1);
    load_sheet(g, &a->ui_arrow, "ui_arrow", 1);
    load_sheet(g, &a->ui_lock, "ui_lock", 1);
    load_sheet(g, &a->ui_cross, "ui_cross", 1);
    load_sheet(g, &a->ui_warn, "ui_warn", 1);
    load_sheet(g, &a->ui_ring, "ui_ring", 1);
    load_sheet(g, &a->ui_glow, "ui_glow", 1);
    load_sheet(g, &a->title_logo, "logo", 1);
    load_sheet(g, &a->fx_smoke, "fx_smoke", 12);
    load_sheet(g, &a->fx_spark, "fx_spark", 6);
    load_sheet(g, &a->fx_glow, "fx_glow", 1);
    static const char *flames[] = { "fx_flame_blue", "fx_flame_orange", "fx_flame_green", "fx_flame_violet" };
    for (int i = 0; i < 4; i++) load_sheet(g, &a->fx_flame[i], flames[i], 10);
    static const char *projs[] = { "proj_bolt", "proj_plasma", "proj_lance", "proj_enemy", "proj_heavy", "proj_missile" };
    for (int i = 0; i < 6; i++) load_sheet(g, &a->proj[i], projs[i], 1);
    load_sheet(g, &a->fx_chunk[0], "fx_chunk", 1);
    for (int i = 0; i < SECTOR_COUNT; i++) {
        char n[24];
        sm_sprintf(n, sizeof(n), "planet_%d", i);
        load_sheet(g, &a->planet[i], n, 1);
    }

    load_font(g, &a->font_tiny, "font_tiny");
    load_font(g, &a->font_small, "font_small");
    load_font(g, &a->font_body, "font_body");
    load_font(g, &a->font_head, "font_head");
    load_font(g, &a->font_title, "font_title");
    load_font(g, &a->font_big, "font_big");
    g->assets_loaded = 1;
    sm_logf("assets loaded (%.1f MB resident)", (double)sm_mem_used() / 1048576.0);
}

void assets_free(Game *g) { g->assets_loaded = 0; }
