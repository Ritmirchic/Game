// Asset loading: PBR texture sets from textures.txt, UI art, fonts, sounds.
#include "world.h"
#include <string.h>

typedef struct TexEntry { char name[40]; Tex *albedo, *normal, *rough; float repeat; int alpha; } TexEntry;
static TexEntry g_tex[192];
static int g_ntex = 0;

Assets *g_assets_ref = NULL;

static Tex *find_albedo(const char *name) {
    for (int i = 0; i < g_ntex; i++) if (!strcmp(g_tex[i].name, name)) return g_tex[i].albedo;
    return NULL;
}
static float find_repeat(const char *name) {
    for (int i = 0; i < g_ntex; i++) if (!strcmp(g_tex[i].name, name)) return g_tex[i].repeat;
    return 1.f;
}
static Tex *find_normal(const char *name) {
    for (int i = 0; i < g_ntex; i++) if (!strcmp(g_tex[i].name, name)) return g_tex[i].normal;
    return NULL;
}

static void load_manifest(const char *path) {
    char *txt = file_read_all(path, NULL);
    if (!txt) { LOGE("textures.txt missing at %s", path); return; }
    char *line = txt;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (line[0] == 'T' && line[1] == ' ') {
            char name[40] = {0}, alb[96] = {0}, nrm[96] = {0}, rgh[96] = {0};
            float rep = 1.f;
            char extra[8] = {0};
            int got = sscanf(line + 2, "%39s %95s %95s %95s %f %7s", name, alb, nrm, rgh, &rep, extra);
            if (got >= 4 && g_ntex < 192) {
                TexEntry *e = &g_tex[g_ntex];
                memset(e, 0, sizeof *e);
                snprintf(e->name, sizeof e->name, "%s", name);
                e->repeat = rep;
                char full[160];
                snprintf(full, sizeof full, "assets/%s", alb);
                e->albedo = tex_load_png(full, name);
                if (strcmp(nrm, "-")) {
                    snprintf(full, sizeof full, "assets/%s", nrm);
                    e->normal = tex_load_png(full, name);
                }
                if (strcmp(rgh, "-")) {
                    snprintf(full, sizeof full, "assets/%s", rgh);
                    e->rough = tex_load_png(full, name);
                }
                e->alpha = (extra[0] == 'A');
                if (e->albedo) g_ntex++;
            }
        }
        line = nl ? nl + 1 : NULL;
    }
    free(txt);
    LOGI("assets: %d texture sets loaded", g_ntex);
}

// Bind material slots. Missing textures fall back to a tinted procedural noise
// so nothing ever renders as untextured white.
static void bind(int mat, const char *name, Vec3 tint, float rough, float metal, float emissive, int flags, int uvscale) {
    const Tex *a = find_albedo(name);
    const Tex *n = find_normal(name);
    if (!a) LOGW("assets: texture '%s' for material %d missing", name, mat);
    float rep = find_repeat(name);
    if (rep <= 0.f) rep = 1.f;
    g_assets_ref->mats[mat] = mat_make(name, a, n, tint, rough, metal, emissive, flags | (n ? MF_NORMALMAP : 0));
    // geometry UVs are authored in metres; the manifest repeat is tiles-per-metre
    g_assets_ref->mats[mat].uvscale = rep;
    (void)uvscale;
}

void assets_load(Assets *a) {
    memset(a, 0, sizeof *a);
    g_assets_ref = a;
    g_ntex = 0;
    load_manifest("assets/textures.txt");
    a->ntex = g_ntex;
    for (int i = 0; i < g_ntex; i++) a->tex[i] = g_tex[i].albedo;

    // ---- world materials
    bind(MAT_FLOOR_TILE, "floor_tile", v3(1, 1, 1), 0.35f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_FLOOR_LINO, "floor_linoleum", v3(1, 1, 1), 0.45f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_FLOOR_CONCRETE, "floor_concrete", v3(1, 1, 1), 0.72f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_FLOOR_CARPET, "floor_carpet", v3(1, 1, 1), 0.9f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_ASPHALT, "floor_asphalt", v3(1, 1, 1), 0.85f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_GRAVEL, "floor_gravel", v3(1, 1, 1), 0.9f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_DIRT, "floor_dirt", v3(1, 1, 1), 0.95f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_WALL_PAINT, "wall_paint", v3(1, 1, 1), 0.62f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_WALL_BLOCK, "wall_block", v3(1, 1, 1), 0.8f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_WALL_BRICK, "wall_brick", v3(1, 1, 1), 0.82f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_WALL_TILE, "wall_tile", v3(1, 1, 1), 0.3f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_WALL_METAL, "wall_metal", v3(1, 1, 1), 0.42f, 0.6f, 0.f, MF_METAL, 0);
    bind(MAT_WALL_RUST, "wall_metal_rust", v3(1, 1, 1), 0.72f, 0.3f, 0.f, MF_METAL, 0);
    bind(MAT_CEIL_TILE, "ceiling_tile", v3(1, 1, 1), 0.85f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_CEIL_CONC, "ceiling_concrete", v3(1, 1, 1), 0.9f, 0.f, 0.f, MF_NONE, 0);
    bind(MAT_WOOD, "wood_plank", v3(1, 1, 1), 0.66f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_PLYWOOD, "plywood", v3(1, 1, 1), 0.72f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_DIAMOND, "metal_diamond", v3(1, 1, 1), 0.5f, 0.7f, 0.f, MF_METAL, 0);
    bind(MAT_GLASS, "glass_dirty", v3(1, 1, 1), 0.05f, 0.1f, 0.05f, MF_TWOSIDED, 1);
    bind(MAT_CHAINLINK, "chainlink", v3(1, 1, 1), 0.6f, 0.5f, 0.f, MF_ALPHATEST | MF_TWOSIDED, 1);
    bind(MAT_CARDBOARD, "cardboard", v3(1, 1, 1), 0.9f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_TARP, "fabric_tarp", v3(1, 1, 1), 0.95f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_RUBBER, "rubber", v3(1, 1, 1), 0.85f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_SHELF_GOODS, "shelf_goods", v3(1, 1, 1), 0.6f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_FREEZER_GOODS, "freezer_goods", v3(1, 1, 1), 0.4f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_PRODUCE, "produce_crate", v3(1, 1, 1), 0.7f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_MEAT, "meat_pack", v3(1, 1, 1), 0.45f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_BOTTLES, "bottle_shelf", v3(1, 1, 1), 0.3f, 0.1f, 0.f, MF_NONE, 1);
    bind(MAT_SIGN_AISLE, "signage_aisle", v3(1, 1, 1), 0.5f, 0.f, 0.35f, MF_EMISSIVE, 1);
    bind(MAT_SIGN_STORE, "signage_store", v3(1, 1, 1), 0.4f, 0.f, 0.9f, MF_EMISSIVE, 1);
    bind(MAT_SIGN_EXIT, "sign_exit", v3(1, 1, 1), 0.4f, 0.f, 1.0f, MF_EMISSIVE, 1);
    bind(MAT_SIGN_WARN, "sign_warning", v3(1, 1, 1), 0.55f, 0.f, 0.1f, MF_NONE, 1);
    bind(MAT_PRICE, "price_tags", v3(1, 1, 1), 0.6f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_POSTER_A, "poster_a", v3(1, 1, 1), 0.6f, 0.f, 0.12f, MF_NONE, 1);
    bind(MAT_POSTER_B, "poster_b", v3(1, 1, 1), 0.6f, 0.f, 0.12f, MF_NONE, 1);
    bind(MAT_DIRECTORY, "store_directory", v3(1, 1, 1), 0.6f, 0.f, 0.25f, MF_NONE, 1);
    bind(MAT_SCREEN, "computer_screen", v3(1, 1, 1), 0.15f, 0.f, 0.85f, MF_EMISSIVE, 1);
    bind(MAT_PAPER, "paper_sheet", v3(1, 1, 1), 0.85f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_CAR_RED, "car_paint_red", v3(1, 1, 1), 0.28f, 0.75f, 0.f, MF_METAL, 1);
    bind(MAT_CAR_BLUE, "car_paint_blue", v3(1, 1, 1), 0.28f, 0.75f, 0.f, MF_METAL, 1);
    bind(MAT_CAR_WHITE, "car_paint_white", v3(1, 1, 1), 0.3f, 0.7f, 0.f, MF_METAL, 1);
    bind(MAT_TIRE, "tire_rubber", v3(1, 1, 1), 0.9f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_VEH_GLASS, "vehicle_glass", v3(1, 1, 1), 0.05f, 0.2f, 0.02f, MF_TWOSIDED, 1);
    bind(MAT_FENCE, "fence_pole", v3(1, 1, 1), 0.6f, 0.6f, 0.f, MF_METAL, 1);
    // ---- characters
    bind(MAT_SKIN_ZOMBIE, "skin_zombie", v3(0.46f, 0.50f, 0.42f), 0.74f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_SKIN_PALE, "skin_pale", v3(0.58f, 0.52f, 0.48f), 0.70f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_SKIN_BRUTE, "skin_brute", v3(0.42f, 0.36f, 0.32f), 0.72f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_FLESH, "flesh_wound", v3(1, 1, 1), 0.6f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_DENIM, "cloth_denim", v3(0.85f, 0.88f, 0.95f), 0.9f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_SHIRT_BLUE, "cloth_shirt_blue", v3(0.75f, 0.78f, 0.85f), 0.88f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_SHIRT_WHITE, "cloth_shirt_white", v3(0.70f, 0.70f, 0.68f), 0.9f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_SHIRT_BROWN, "cloth_shirt_brown", v3(1, 1, 1), 0.88f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_HIVIS, "cloth_hivis", v3(1, 1, 1), 0.7f, 0.f, 0.08f, MF_NONE, 1);
    bind(MAT_LEATHER, "cloth_leather", v3(0.8f, 0.75f, 0.7f), 0.6f, 0.05f, 0.f, MF_NONE, 1);
    bind(MAT_SCRUBS, "cloth_scrubs", v3(1, 1, 1), 0.85f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_HAIR, "hair_dark", v3(0.55f, 0.52f, 0.50f), 0.75f, 0.f, 0.f, MF_NONE, 1);
    // ---- weapons
    bind(MAT_GUN_METAL, "gun_metal", v3(0.62f, 0.64f, 0.68f), 0.44f, 0.55f, 0.f, MF_METAL, 1);
    bind(MAT_GUN_POLY, "gun_polymer", v3(0.55f, 0.56f, 0.58f), 0.62f, 0.05f, 0.f, MF_NONE, 1);
    bind(MAT_WOOD_STOCK, "wood_stock", v3(0.78f, 0.70f, 0.60f), 0.55f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_BAT, "bat_wood", v3(1, 1, 1), 0.35f, 0.2f, 0.f, MF_NONE, 1);
    bind(MAT_AXE, "axe_metal", v3(1, 1, 1), 0.35f, 0.85f, 0.f, MF_METAL, 1);
    bind(MAT_STEEL, "steel_brushed", v3(0.7f, 0.72f, 0.76f), 0.42f, 0.6f, 0.f, MF_METAL, 1);
    bind(MAT_CROSSBOW, "crossbow_wood", v3(1, 1, 1), 0.45f, 0.f, 0.f, MF_NONE, 1);
    bind(MAT_EMISSIVE_WARM, "paper_sheet", v3(1.0f, 0.86f, 0.62f), 0.f, 0.f, 2.2f, MF_EMISSIVE | MF_UNLIT, 1);
    bind(MAT_EMISSIVE_COOL, "paper_sheet", v3(0.62f, 0.86f, 1.0f), 0.f, 0.f, 2.0f, MF_EMISSIVE | MF_UNLIT, 1);
    bind(MAT_EMISSIVE_RED, "sign_warning", v3(1.0f, 0.30f, 0.22f), 0.f, 0.f, 2.2f, MF_EMISSIVE | MF_UNLIT, 1);
    bind(MAT_EMISSIVE_GREEN, "sign_exit", v3(0.45f, 1.0f, 0.55f), 0.f, 0.f, 2.0f, MF_EMISSIVE | MF_UNLIT, 1);
    bind(MAT_EMISSIVE_SCREEN, "computer_screen", v3(0.6f, 1.0f, 0.8f), 0.f, 0.f, 1.8f, MF_EMISSIVE, 1);
    bind(MAT_LAMPGLASS, "glass_dirty", v3(1.0f, 0.95f, 0.85f), 0.1f, 0.f, 1.6f, MF_EMISSIVE, 1);
    // fallback
    g_assets_ref->mats[MAT_NONE] = mat_make("default", find_albedo("floor_concrete"), find_normal("floor_concrete"),
                                            v3(0.7f, 0.7f, 0.7f), 0.8f, 0.f, 0.f, 0);

    // ---- UI art
    a->logo = tex_load_png("assets/ui/logo.png", "logo");
    a->panel = tex_load_png("assets/ui/panel.png", "panel");
    a->panel_solid = tex_load_png("assets/ui/panel_solid.png", "panel_solid");
    a->crosshair = tex_load_png("assets/ui/crosshair.png", "crosshair");
    a->blood = tex_load_png("assets/ui/blood.png", "blood");
    a->noise = tex_load_png("assets/ui/noise.png", "noise");
    a->vignette = tex_load_png("assets/ui/vignette.png", "vignette");
    a->flash = tex_load_png("assets/ui/flash.png", "flash");
    a->smoke = tex_load_png("assets/ui/smoke.png", "smoke");
    a->rain = tex_load_png("assets/ui/rain.png", "rain");
    a->icons = tex_load_png("assets/ui/icons.png", "icons");

    // ---- fonts
    a->f_title = font_load("assets/fonts/title.txt", "assets/fonts/title.png", "title");
    a->f_bold = font_load("assets/fonts/bold.txt", "assets/fonts/bold.png", "bold");
    a->f_reg = font_load("assets/fonts/reg.txt", "assets/fonts/reg.png", "reg");
    a->f_mono = font_load("assets/fonts/mono.txt", "assets/fonts/mono.png", "mono");
    if (!a->f_bold) LOGE("assets: bold font failed");
    LOGI("assets: %d textures, %d ui, fonts %d%d%d%d", g_ntex, 11,
         a->f_title != NULL, a->f_bold != NULL, a->f_reg != NULL, a->f_mono != NULL);
}

void assets_free(Assets *a) {
    for (int i = 0; i < g_ntex; i++) {
        tex_free(g_tex[i].albedo);
        tex_free(g_tex[i].normal);
        tex_free(g_tex[i].rough);
    }
    g_ntex = 0;
    tex_free(a->logo); tex_free(a->panel); tex_free(a->panel_solid); tex_free(a->crosshair);
    tex_free(a->blood); tex_free(a->noise); tex_free(a->vignette); tex_free(a->flash);
    tex_free(a->smoke); tex_free(a->rain); tex_free(a->icons);
    a->logo = NULL; a->panel = NULL; a->panel_solid = NULL; a->crosshair = NULL;
    a->blood = NULL; a->noise = NULL; a->vignette = NULL; a->flash = NULL;
    a->smoke = NULL; a->rain = NULL; a->icons = NULL;
    font_free(a->f_title); font_free(a->f_bold); font_free(a->f_reg); font_free(a->f_mono);
    memset(a, 0, sizeof *a);
}
