/* ============================================================================
   SPACE MINER — core/ui_dock.c
   Station interface: market, upgrades, shipyard, drones, sector map, awards.
   ========================================================================== */
#include "state.h"
#include "ui_internal.h"
#include "audio.h"

static s64 res_total_value(Game *g, int res) {
    return (s64)g->player.cargo[res] * (s64)k_resources[res].value;
}

static s64 cargo_value(Game *g) {
    s64 total = 0;
    for (int i = 0; i < RES_COUNT; i++) total += res_total_value(g, i);
    return total;
}

static void sell_all(Game *g) {
    s64 total = 0;
    int units = 0;
    for (int i = 0; i < RES_COUNT; i++) {
        if (g->player.cargo[i] <= 0) continue;
        total += res_total_value(g, i);
        units += g->player.cargo[i];
        if (i == RES_CRYSTAL) g->save.stat_crystals_sold += g->player.cargo[i];
        g->player.cargo[i] = 0;
        g->save.cargo[i] = 0;
    }
    if (total <= 0) {
        game_toast(g, "NOTHING TO SELL", SM_RGB(255, 160, 110));
        snd_play(g, "sfx_error", 0.5f, 0.f, 1.f);
        return;
    }
    g->save.credits += total;
    g->save.stat_credits_earned += total;
    g->player.cargo_used = 0;
    g->save.cargo_used = 0;
    char buf[96];
    char num[40];
    sm_format_num(num, sizeof(num), total);
    sm_sprintf(buf, sizeof(buf), "SOLD %d UNITS  //  +%s CR", units, num);
    game_toast(g, buf, UI_GOLD);
    snd_play(g, "sfx_coin", 0.75f, 0.f, 1.f);
    player_gain_xp(g, units / 2);
    save_write(g);
    ach_check(g);
}

/* --------------------------------------------------------------- docking --- */
void ui_render_dock(Game *g, Input *in) {
    (void)in;
    Gfx *gfx = g->gfx;
    const SectorDef *sec = &k_sectors[g->sector];

    /* backdrop: the station interior view with the field outside */
    gfx_fill_vgrad(gfx, rect(0, 0, (f32)gfx->w, (f32)gfx->h), SM_RGB(9, 13, 20), SM_RGB(4, 6, 10));
    Sheet *neb = &g->assets.bg_far[g->sector];
    if (neb->px) {
        f32 scale = 3.2f;
        f32 tw = (f32)neb->w * scale, th = (f32)neb->h * scale;
        for (f32 y = 0; y < (f32)gfx->h; y += th)
            for (f32 x = 0; x < (f32)gfx->w; x += tw)
                gfx_blit(gfx, neb, 0, rect(x, y, tw + 1, th + 1), 0xFFFFFFFF, 0.28f, BLEND_ADD);
    }
    for (int i = 0; i < g->star_count; i += 2) {
        Vec2 sp = g->stars[i];
        f32 tw = 0.5f + 0.5f * sinf(g->time * (0.5f + g->star_twinkle[i]) + g->star_twinkle[i] * 7.f);
        gfx_fill(gfx, rect(sp.x, sp.y * 0.8f, g->star_size[i], g->star_size[i]), sm_col_scale(g->star_color[i], tw * 0.8f));
    }
    /* station struts framing the viewport */
    u32 strut = SM_RGBA(28, 36, 48, 255);
    gfx_fill(gfx, rect(0, 0, (f32)gfx->w, 46.f), strut);
    gfx_fill(gfx, rect(0, (f32)gfx->h - 54.f, (f32)gfx->w, 54.f), strut);
    gfx_fill(gfx, rect(0, 46.f, 8.f, (f32)gfx->h - 100.f), SM_RGBA(22, 30, 40, 255));
    gfx_fill(gfx, rect((f32)gfx->w - 8.f, 46.f, 8.f, (f32)gfx->h - 100.f), SM_RGBA(22, 30, 40, 255));
    gfx_fill(gfx, rect(0, 46.f, (f32)gfx->w, 1.f), SM_RGBA(80, 140, 170, 120));

    /* top bar */
    gfx_text_spaced(gfx, &g->assets.font_head, v2(24.f, 12.f), "REFINERY STATION", UI_INK, 0, 2.f);
    {
        char buf[96];
        sm_sprintf(buf, sizeof(buf), "%s  //  SHIP: %s  //  LVL %d", sec->name, k_ships[g->save.ship].name, g->save.level);
        gfx_text(gfx, &g->assets.font_tiny, v2(24.f, 32.f), buf, UI_DIM, 0);
    }
    ui_money(g, v2((f32)gfx->w - 190.f, 14.f), g->save.credits, 2);
    {
        char buf[48];
        sm_sprintf(buf, sizeof(buf), "HOLD %d/%d", (int)g->player.cargo_used, player_stat_cargo_max(g));
        gfx_text(gfx, &g->assets.font_tiny, v2((f32)gfx->w - 24.f, 36.f), buf, UI_DIM, 2);
    }

    /* sidebar tabs */
    static const char *tabs[] = { "MARKET", "UPGRADES", "SHIPYARD", "DRONES", "MAP", "AWARDS", "SETTINGS" };
    f32 tabw = 150.f;
    f32 tabx = 26.f, taby = 66.f;
    for (int i = 0; i < DOCK_TAB_COUNT; i++) {
        Rect r = rect(tabx, taby + (f32)i * 56.f, tabw, 48.f);
        int hover = ui_hover(g, r);
        BtnState st = (g->dock_tab == i) ? BTN_SELECTED : (hover ? BTN_HOVER : BTN_NORMAL);
        ui_button_draw(g, r, tabs[i], NULL, st, UI_CYAN);
        if (hover && g_ui_in && g_ui_in->mouse_pressed[0] && g->dock_tab != i) {
            g->dock_tab = (DockTab)i;
            g->dock_sel = 0;
            g->dock_scroll = 0;
            snd_play(g, "sfx_click", 0.4f, 0.f, 1.18f);
        }
    }

    /* content area */
    Rect area = rect(tabx + tabw + 22.f, 66.f, (f32)gfx->w - (tabx + tabw + 22.f) - 26.f,
                     (f32)gfx->h - 66.f - 78.f);
    switch (g->dock_tab) {
        case DOCK_TAB_MARKET: ui_dock_market(g, area); break;
        case DOCK_TAB_UPGRADES: ui_dock_upgrades(g, area); break;
        case DOCK_TAB_SHIPYARD: ui_dock_shipyard(g, area); break;
        case DOCK_TAB_DRONES: ui_dock_drones(g, area); break;
        case DOCK_TAB_MAP: ui_dock_map(g, area); break;
        case DOCK_TAB_ACHIEVEMENTS: ui_dock_awards(g, area); break;
        case DOCK_TAB_SETTINGS: ui_dock_settings(g, area); break;
        default: break;
    }

    /* bottom bar: undock */
    if (ui_button(g, rect((f32)gfx->w * 0.5f - 130.f, (f32)gfx->h - 50.f, 260.f, 38.f),
                  "UNDOCK  [ESC]", "launch back into the field", 1, UI_CYAN)) {
        game_set_screen(g, SCREEN_PLAY);
        save_write(g);
    }
    ui_toasts(g);
}

/* ---------------------------------------------------------------- market --- */
void ui_dock_market(Game *g, Rect area) {
    Gfx *gfx = g->gfx;
    ui_panel(g, area, UI_GOLD);
    ui_header(g, rect(area.x + 20.f, area.y + 12.f, area.w - 40.f, 40.f),
              "ORE EXCHANGE", "station rates (credits per unit)", UI_GOLD);

    f32 rowH = 34.f;
    f32 y = area.y + 64.f;
    f32 colW = area.w - 40.f;
    gfx_text(gfx, &g->assets.font_tiny, v2(area.x + 22.f, y), "MATERIAL", UI_DIM, 0);
    gfx_text(gfx, &g->assets.font_tiny, v2(area.x + 22.f + colW * 0.52f, y), "QTY", UI_DIM, 2);
    gfx_text(gfx, &g->assets.font_tiny, v2(area.x + 22.f + colW * 0.70f, y), "VALUE", UI_DIM, 2);
    gfx_text(gfx, &g->assets.font_tiny, v2(area.x + 22.f + colW * 1.0f, y), "TOTAL", UI_DIM, 2);
    y += 18.f;
    gfx_fill(gfx, rect(area.x + 20.f, y, colW, 1.f), SM_RGBA(90, 110, 130, 90));
    y += 8.f;

    int rows = 0;
    for (int i = 0; i < RES_COUNT; i++) {
        int qty = g->player.cargo[i];
        if (qty <= 0) continue;
        if (rows >= 10) break;
        Rect r = rect(area.x + 20.f, y, colW, rowH - 4.f);
        int hover = ui_hover(g, r);
        if (hover) gfx_fill(gfx, r, SM_RGBA(60, 90, 110, 40));
        gfx_blit(gfx, &g->assets.res_icon[i], 0, rect(r.x + 2.f, r.y + 2.f, 26.f, 26.f), 0xFFFFFFFF, 1.f, BLEND_ALPHA);
        gfx_text(gfx, &g->assets.font_small, v2(r.x + 36.f, r.y + 6.f), k_resources[i].name, k_resources[i].color, 0);
        char buf[48];
        sm_sprintf(buf, sizeof(buf), "%d", qty);
        gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f + colW * 0.52f, r.y + 6.f), buf, UI_INK, 2);
        sm_format_num(buf, sizeof(buf), k_resources[i].value);
        gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f + colW * 0.70f, r.y + 6.f), buf, UI_DIM, 2);
        sm_format_num(buf, sizeof(buf), res_total_value(g, i));
        gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f + colW * 1.0f, r.y + 6.f), buf, UI_GOLD, 2);
        y += rowH;
        rows++;
    }
    if (rows == 0) {
        gfx_text(gfx, &g->assets.font_body, v2(area.x + area.w * 0.5f, area.y + area.h * 0.42f),
                 "Your hold is empty.", UI_DIM, 1);
        gfx_text(gfx, &g->assets.font_small, v2(area.x + area.w * 0.5f, area.y + area.h * 0.42f + 28.f),
                 "Undock, find an asteroid, hold the right mouse button and cut it open.", UI_DIMMER, 1);
    }
    /* totals + sell */
    f32 ty = area.y + area.h - 76.f;
    gfx_fill(gfx, rect(area.x + 20.f, ty - 10.f, colW, 1.f), SM_RGBA(90, 110, 130, 90));
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f, ty + 8.f), "CARGO VALUE", UI_DIM, 0);
    char buf[48];
    char num[40];
    sm_format_num(num, sizeof(num), cargo_value(g));
    sm_sprintf(buf, sizeof(buf), "%s CR", num);
    gfx_text(gfx, &g->assets.font_head, v2(area.x + 22.f + colW * 0.70f, ty + 2.f), buf, UI_GOLD, 2);
    if (ui_button(g, rect(area.x + colW - 200.f, ty + 2.f, 200.f, 46.f), "SELL ALL", "dump the whole hold", cargo_value(g) > 0, UI_GOLD))
        sell_all(g);
}

/* -------------------------------------------------------------- upgrades --- */
void ui_dock_upgrades(Game *g, Rect area) {
    Gfx *gfx = g->gfx;
    ui_panel(g, area, UI_CYAN);
    ui_header(g, rect(area.x + 20.f, area.y + 12.f, area.w - 40.f, 40.f),
              "SHIP SYSTEMS", "each rank has a real effect on flight and mining", UI_CYAN);
    int cols = 3, rowsn = (UPG_COUNT + cols - 1) / cols;
    f32 gap = 10.f;
    f32 cw = (area.w - 40.f - gap * (f32)(cols - 1)) / (f32)cols;
    f32 ch = SM_MIN(104.f, (area.h - 70.f - 8.f * (f32)(rowsn - 1)) / (f32)rowsn);
    for (int i = 0; i < UPG_COUNT; i++) {
        int cx = i % cols, cy = i / cols;
        Rect r = rect(area.x + 20.f + (f32)cx * (cw + gap), area.y + 68.f + (f32)cy * (ch + 8.f), cw, ch);
        const UpgradeDef *u = &k_upgrades[i];
        int lvl = g->save.upgrades[i];
        int maxed = lvl >= UPG_MAX_LEVEL;
        s64 cost = upgrade_cost(i, lvl);
        int affordable = g->save.credits >= cost && g->save.level >= 1 + lvl * 2;
        int hover = ui_hover(g, r);
        gfx_fill(gfx, r, hover ? SM_RGBA(20, 32, 46, 240) : SM_RGBA(12, 19, 28, 230));
        gfx_rect_outline(gfx, r, 1.f, maxed ? SM_RGBA(120, 240, 170, 160)
                                            : SM_RGBA(SM_GET_R(u->color) / 2, SM_GET_G(u->color) / 2, SM_GET_B(u->color) / 2, 140));
        gfx_blit(gfx, &g->assets.upg_icon[i], 0, rect(r.x + 10.f, r.y + 10.f, 34.f, 34.f), u->color, 0.95f, BLEND_ALPHA);
        gfx_text(gfx, &g->assets.font_small, v2(r.x + 50.f, r.y + 12.f), u->name, UI_INK, 0);
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 50.f, r.y + 32.f), u->desc,
                 UI_DIM, 0);
        /* level pips */
        for (int k = 0; k < UPG_MAX_LEVEL; k++) {
            Rect pip = rect(r.x + 10.f + (f32)k * 20.f, r.y + 54.f, 16.f, 8.f);
            gfx_fill(gfx, pip, k < lvl ? u->color : SM_RGBA(40, 52, 66, 200));
        }
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 112.f, r.y + 52.f), u->effect, UI_DIMMER, 0);
        if (maxed) {
            gfx_text(gfx, &g->assets.font_small, v2(r.x + 10.f, r.y + ch - 28.f), "MAXED", UI_GREEN, 0);
        } else {
            char buf[56];
            char num[32];
            sm_format_num(num, sizeof(num), cost);
            sm_sprintf(buf, sizeof(buf), "RANK %d > %d   %s CR", lvl, lvl + 1, num);
            gfx_text(gfx, &g->assets.font_small, v2(r.x + 10.f, r.y + ch - 28.f), buf,
                     affordable ? UI_GOLD : UI_DIMMER, 0);
            if (g->save.level < 1 + lvl * 2)
                gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 10.f, r.y + ch - 12.f),
                         "REQUIRES HIGHER MINING LEVEL", UI_RED, 0);
        }
        if (hover && !maxed && g_ui_in && g_ui_in->mouse_pressed[0]) {
            if (g->save.level < 1 + lvl * 2) {
                game_toast(g, "MINING LEVEL TOO LOW", UI_RED);
                snd_play(g, "sfx_error", 0.5f, 0.f, 1.f);
            } else if (g->save.credits < cost) {
                game_toast(g, "NOT ENOUGH CREDITS", UI_RED);
                snd_play(g, "sfx_error", 0.5f, 0.f, 1.f);
            } else {
                g->save.credits -= cost;
                g->save.upgrades[i]++;
                snd_play(g, "sfx_buy", 0.7f, 0.f, 1.f);
                char buf[96];
                sm_sprintf(buf, sizeof(buf), "%s -> RANK %d", u->name, g->save.upgrades[i]);
                game_toast(g, buf, u->color);
                player_apply_loadout(g);
                if (i == UPG_HULL) g->player.hull = SM_MIN(g->player.hull_max, g->player.hull + 30.f);
                if (i == UPG_SHIELD) g->player.shield = g->player.shield_max;
                if (i == UPG_ENERGY_REGEN) g->player.energy = g->player.energy_max;
                save_write(g);
                ach_check(g);
            }
        }
    }
}

/* -------------------------------------------------------------- shipyard --- */
void ui_dock_shipyard(Game *g, Rect area) {
    Gfx *gfx = g->gfx;
    ui_panel(g, area, UI_VIOLET);
    ui_header(g, rect(area.x + 20.f, area.y + 12.f, area.w - 40.f, 40.f),
              "HANGAR", "every hull has a different flight profile", UI_VIOLET);
    f32 gap = 12.f;
    f32 cw = (area.w - 40.f - gap * 3.f) / 4.f;
    f32 ch = area.h - 90.f;
    for (int i = 0; i < SHIP_COUNT; i++) {
        const ShipDef *s = &k_ships[i];
        Rect r = rect(area.x + 20.f + (f32)i * (cw + gap), area.y + 68.f, cw, ch);
        int owned = g->save.ship_owned[i];
        int selected = g->save.ship == i;
        int hover = ui_hover(g, r);
        gfx_fill(gfx, r, hover ? SM_RGBA(22, 26, 44, 240) : SM_RGBA(12, 16, 26, 230));
        gfx_rect_outline(gfx, r, selected ? 2.f : 1.f,
                         selected ? UI_VIOLET : SM_RGBA(80, 90, 130, 140));
        /* ship preview */
        Sheet *sh = &g->assets.ship[i];
        if (sh->px) {
            f32 pw = SM_MIN(cw - 30.f, 150.f);
            gfx_glow(gfx, v2(r.x + cw * 0.5f, r.y + 96.f), pw * 0.6f,
                     selected ? UI_VIOLET : SM_RGB(60, 90, 130), 0.3f);
            gfx_blit(gfx, sh, 0, rect(r.x + (cw - pw) * 0.5f, r.y + 96.f - pw * 0.5f, pw, pw),
                     0xFFFFFFFF, 0.95f, BLEND_ALPHA);
        }
        gfx_text_spaced(gfx, &g->assets.font_head, v2(r.x + cw * 0.5f, r.y + 12.f), s->name, UI_INK, 1, 1.5f);
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + cw * 0.5f, r.y + 34.f), s->role, UI_DIM, 1);
        f32 sy = r.y + 172.f;
        struct { const char *label; char value[24]; } stats[6];
        sm_sprintf(stats[0].value, sizeof(stats[0].value), "%d", s->hull);
        stats[0].label = "HULL";
        sm_sprintf(stats[1].value, sizeof(stats[1].value), "%d", s->shield);
        stats[1].label = "SHIELD";
        sm_sprintf(stats[2].value, sizeof(stats[2].value), "%d", s->cargo);
        stats[2].label = "CARGO";
        sm_sprintf(stats[3].value, sizeof(stats[3].value), "%.0f", s->max_speed);
        stats[3].label = "SPEED";
        sm_sprintf(stats[4].value, sizeof(stats[4].value), "%.0f", s->laser_dps);
        stats[4].label = "LASER";
        sm_sprintf(stats[5].value, sizeof(stats[5].value), "%d", s->drone_slots);
        stats[5].label = "DRONES";
        for (int k = 0; k < 6; k++) {
            f32 yy = sy + (f32)k * 20.f;
            gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 14.f, yy), stats[k].label, UI_DIM, 0);
            gfx_text(gfx, &g->assets.font_small, v2(r.x + cw - 14.f, yy - 2.f), stats[k].value, UI_INK, 2);
        }
        char buf[64];
        if (owned) {
            if (selected) {
                gfx_text(g->gfx, &g->assets.font_small, v2(r.x + cw * 0.5f, r.y + ch - 34.f), "IN SERVICE", UI_GREEN, 1);
            } else if (ui_button(g, rect(r.x + 12.f, r.y + ch - 46.f, cw - 24.f, 36.f), "FLY THIS SHIP", NULL, 1, UI_VIOLET)) {
                g->save.ship = i;
                player_apply_loadout(g);
                g->player.hull = g->player.hull_max;
                g->player.shield = g->player.shield_max;
                game_toast(g, "HULL SWAPPED", UI_VIOLET);
                snd_play(g, "sfx_buy", 0.7f, 0.f, 0.9f);
                save_write(g);
            }
        } else {
            char num[32];
            sm_format_num(num, sizeof(num), s->cost);
            int can = g->save.credits >= s->cost && g->save.level >= s->req_level;
            sm_sprintf(buf, sizeof(buf), "%s CR", num);
            gfx_text(g->gfx, &g->assets.font_small, v2(r.x + cw * 0.5f, r.y + ch - 76.f), buf,
                     can ? UI_GOLD : UI_DIMMER, 1);
            if (g->save.level < s->req_level) {
                sm_sprintf(buf, sizeof(buf), "LEVEL %d REQUIRED", s->req_level);
                gfx_text(g->gfx, &g->assets.font_tiny, v2(r.x + cw * 0.5f, r.y + ch - 58.f), buf, UI_RED, 1);
            }
            if (ui_button(g, rect(r.x + 12.f, r.y + ch - 46.f, cw - 24.f, 36.f), "PURCHASE", NULL, can, UI_VIOLET)) {
                g->save.credits -= s->cost;
                g->save.ship_owned[i] = 1;
                game_toast(g, "NEW HULL ACQUIRED", UI_VIOLET);
                snd_play(g, "sfx_buy", 0.8f, 0.f, 1.f);
                ach_check(g);
                save_write(g);
            }
        }
    }
}

/* ---------------------------------------------------------------- drones --- */
void ui_dock_drones(Game *g, Rect area) {
    Gfx *gfx = g->gfx;
    ui_panel(g, area, UI_GREEN);
    ui_header(g, rect(area.x + 20.f, area.y + 12.f, area.w - 40.f, 40.f),
              "DRONE BAY", "automation works the field while you fly", UI_GREEN);
    char buf[96];
    sm_sprintf(buf, sizeof(buf), "SLOTS  %d / %d   (ship %d + uplink %d)",
               g->save.drone_count, player_stat_drone_slots(g),
               k_ships[g->save.ship].drone_slots, g->save.upgrades[UPG_DRONE_TECH]);
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f, area.y + 60.f), buf, UI_DIM, 0);

    f32 cw = (area.w - 40.f - 16.f) * 0.5f;
    for (int i = 0; i < DRONE_TYPE_COUNT; i++) {
        const DroneDef *d = &k_drones[i];
        Rect r = rect(area.x + 20.f + (f32)i * (cw + 16.f), area.y + 88.f, cw, 210.f);
        int hover = ui_hover(g, r);
        gfx_fill(gfx, r, hover ? SM_RGBA(18, 30, 34, 240) : SM_RGBA(11, 19, 24, 230));
        gfx_rect_outline(gfx, r, 1.f, SM_RGBA(80, 150, 120, 140));
        Sheet *sh = &g->assets.drone[i];
        if (sh->px) {
            f32 pw = 104.f;
            gfx_glow(gfx, v2(r.x + 74.f, r.y + 104.f), 60.f, UI_GREEN, 0.25f);
            gfx_blit(gfx, sh, 0, rect(r.x + 74.f - pw * 0.5f, r.y + 104.f - pw * 0.5f, pw, pw),
                     0xFFFFFFFF, 0.95f, BLEND_ALPHA);
        }
        gfx_text_spaced(gfx, &g->assets.font_head, v2(r.x + 140.f, r.y + 16.f), d->name, UI_INK, 0, 1.4f);
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 140.f, r.y + 42.f), d->desc, UI_DIM, 0);
        struct { const char *l; char v[24]; } st[3];
        sm_sprintf(st[0].v, sizeof(st[0].v), "%.1f", d->mining_dps);
        st[0].l = "MINING / S";
        sm_sprintf(st[1].v, sizeof(st[1].v), "%.0f", d->speed);
        st[1].l = "SPEED";
        sm_sprintf(st[2].v, sizeof(st[2].v), "%d", d->capacity);
        st[2].l = "HOPPER";
        for (int k = 0; k < 3; k++) {
            f32 yy = r.y + 70.f + (f32)k * 22.f;
            gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 140.f, yy), st[k].l, UI_DIM, 0);
            gfx_text(gfx, &g->assets.font_small, v2(r.x + cw - 16.f, yy - 2.f), st[k].v, UI_INK, 2);
        }
        char num[32];
        sm_format_num(num, sizeof(num), d->cost);
        sm_sprintf(buf, sizeof(buf), "%s CR", num);
        int can = g->save.credits >= d->cost &&
                  g->save.drone_count < player_stat_drone_slots(g) &&
                  g->save.drone_count < SM_MAX_DRONES;
        gfx_text(gfx, &g->assets.font_small, v2(r.x + 140.f, r.y + 154.f), buf, can ? UI_GOLD : UI_DIMMER, 0);
        sm_sprintf(buf, sizeof(buf), "OWNED: %d", g->save.drone_owned[i]);
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 140.f, r.y + 174.f), buf, UI_DIM, 0);
        if (ui_button(g, rect(r.x + 12.f, r.y + 162.f, 150.f, 36.f), "BUY DRONE", NULL, can, UI_GREEN)) {
            drone_buy(g, i);
            save_write(g);
        }
    }

    /* active fleet */
    f32 fy = area.y + 320.f;
    gfx_fill(gfx, rect(area.x + 20.f, fy, area.w - 40.f, 1.f), SM_RGBA(90, 130, 120, 90));
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f, fy + 10.f), "ACTIVE FLEET", UI_DIM, 0);
    int shown = 0;
    for (int i = 0; i < SM_MAX_DRONES; i++) {
        Drone *d = &g->drones[i];
        if (!d->active) continue;
        f32 yy = fy + 36.f + (f32)(shown / 3) * 34.f;
        f32 xx = area.x + 22.f + (f32)(shown % 3) * ((area.w - 44.f) / 3.f);
        Rect r = rect(xx, yy, (area.w - 60.f) / 3.f, 28.f);
        gfx_fill(gfx, r, SM_RGBA(10, 18, 22, 200));
        gfx_blit(gfx, &g->assets.ui_drone, 0, rect(r.x + 4.f, r.y + 5.f, 18.f, 18.f), UI_GREEN, 0.9f, BLEND_ALPHA);
        const char *st = d->state == DRONE_MINE ? "MINING" :
                         (d->state == DRONE_RETURN || d->state == DRONE_UNLOAD) ? "HAULING" : "SEEKING";
        sm_sprintf(buf, sizeof(buf), "%s  %s", k_drones[d->type].name, st);
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 26.f, r.y + 7.f), buf, UI_INK, 0);
        shown++;
    }
    if (shown == 0)
        gfx_text(gfx, &g->assets.font_tiny, v2(area.x + 22.f, fy + 40.f),
                 "No drones deployed. Buy one and it will start cutting rock on its own.", UI_DIMMER, 0);
}

/* ------------------------------------------------------------------- map --- */
void ui_dock_map(Game *g, Rect area) {
    Gfx *gfx = g->gfx;
    ui_panel(g, area, UI_CYAN);
    ui_header(g, rect(area.x + 20.f, area.y + 12.f, area.w - 40.f, 40.f),
              "SECTOR MAP", "unlock new fields by levelling and paying the charter fee", UI_CYAN);
    f32 gap = 10.f;
    f32 cw = (area.w - 40.f - gap * 4.f) / 5.f;
    f32 ch = 196.f;
    for (int i = 0; i < SECTOR_COUNT; i++) {
        const SectorDef *s = &k_sectors[i];
        Rect r = rect(area.x + 20.f + (f32)i * (cw + gap), area.y + 70.f, cw, ch);
        int unlocked = g->save.sector_unlocked[i];
        int current = g->sector == i;
        int hover = ui_hover(g, r);
        gfx_fill_vgrad(gfx, r, unlocked ? s->bg_top : SM_RGB(12, 15, 20), unlocked ? s->bg_bottom : SM_RGB(6, 8, 12));
        /* nebula swatch */
        Sheet *neb = &g->assets.bg_far[i];
        if (neb->px && unlocked) {
            f32 sw = cw - 16.f;
            gfx_blit(gfx, neb, 0, rect(r.x + 8.f, r.y + 8.f, sw, sw * 0.55f), 0xFFFFFFFF, 0.5f, BLEND_ADD);
        }
        gfx_rect_outline(gfx, r, current ? 2.f : 1.f,
                         current ? UI_CYAN : (hover ? SM_RGBA(140, 200, 230, 160) : SM_RGBA(80, 110, 130, 120)));
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 10.f, r.y + 10.f),
                 i == 0 ? "SECTOR 01" : (i == 1 ? "SECTOR 02" : (i == 2 ? "SECTOR 03" : (i == 3 ? "SECTOR 04" : "SECTOR 05"))),
                 UI_DIM, 0);
        gfx_text(gfx, &g->assets.font_head, v2(r.x + 10.f, r.y + 58.f), s->name, unlocked ? UI_INK : UI_DIMMER, 0);
        for (int k = 0; k < 5; k++) {
            gfx_fill(gfx, rect(r.x + 10.f + (f32)k * 13.f, r.y + 86.f, 9.f, 4.f),
                     k < s->danger ? UI_RED : SM_RGBA(70, 86, 100, 140));
        }
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 10.f, r.y + 96.f), "DANGER", UI_DIMMER, 0);
        /* resource hints */
        int shown = 0;
        for (int k = 0; k < s->pool_count && shown < 4; k++) {
            int res = s->pool[k].res;
            if (k_resources[res].rarity < 3 && shown > 1) continue;
            gfx_blit(gfx, &g->assets.res_icon[res], 0, rect(r.x + 10.f + (f32)shown * 26.f, r.y + 108.f, 22.f, 22.f),
                     unlocked ? 0xFFFFFFFF : SM_RGBA(120, 130, 145, 255), unlocked ? 1.f : 0.5f, BLEND_ALPHA);
            shown++;
        }
        if (!unlocked) {
            if (g->save.level < s->req_level) {
                char buf[48];
                sm_sprintf(buf, sizeof(buf), "LEVEL %d REQUIRED", s->req_level);
                gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 10.f, r.y + ch - 46.f), buf, UI_RED, 0);
                gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 10.f, r.y + ch - 28.f),
                         "Mine and sell to level up", UI_DIMMER, 0);
            } else {
                char buf[56], num[32];
                sm_format_num(num, sizeof(num), s->unlock_cost);
                sm_sprintf(buf, sizeof(buf), "CHARTER  %s CR", num);
                gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 10.f, r.y + ch - 46.f), buf, UI_GOLD, 0);
                if (ui_button(g, rect(r.x + 10.f, r.y + ch - 34.f, cw - 20.f, 28.f), "UNLOCK", NULL,
                              g->save.credits >= s->unlock_cost, UI_CYAN)) {
                    g->save.credits -= s->unlock_cost;
                    g->save.sector_unlocked[i] = 1;
                    game_toast(g, "SECTOR CHARTERED", UI_CYAN);
                    snd_play(g, "sfx_buy", 0.8f, 0.f, 1.f);
                    ach_check(g);
                    save_write(g);
                }
            }
        } else if (current) {
            gfx_text(gfx, &g->assets.font_small, v2(r.x + 10.f, r.y + ch - 30.f), "YOU ARE HERE", UI_CYAN, 0);
        } else {
            if (ui_button(g, rect(r.x + 10.f, r.y + ch - 34.f, cw - 20.f, 28.f), "TRAVEL", NULL, 1, UI_CYAN)) {
                g->pending_sector = i;
                g->save.sector_current = i;
                g->sector = i;
                player_reset(g, 1);
                world_enter_sector(g, i);
                game_set_screen(g, SCREEN_PLAY);
                save_write(g);
            }
        }
    }

    /* sector intel */
    const SectorDef *s = &k_sectors[g->sector];
    f32 iy = area.y + 288.f;
    ui_panel_soft(g, rect(area.x + 20.f, iy, area.w - 40.f, area.h - 300.f), s->accent);
    gfx_text_spaced(gfx, &g->assets.font_head, v2(area.x + 36.f, iy + 14.f), s->name, s->accent, 0, 2.f);
    gfx_text(gfx, &g->assets.font_body, v2(area.x + 36.f, iy + 44.f), s->desc, UI_DIM, 0);
    char buf[128];
    const char *hz = "none";
    switch (s->hazard) {
        case HAZARD_CRYSTAL_DRIFT: hz = "crystal shard drift - impacts damage the hull"; break;
        case HAZARD_DEBRIS_FIELD: hz = "tumbling wreckage - high speed debris"; break;
        case HAZARD_PLASMA_STORM: hz = "plasma storm - lightning strikes the field"; break;
        case HAZARD_VOID_SURGE: hz = "void surge - gravity waves and dark matter"; break;
        default: break;
    }
    sm_sprintf(buf, sizeof(buf), "HAZARD:  %s", hz);
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 36.f, iy + 76.f), buf,
             s->hazard == HAZARD_NONE ? UI_GREEN : UI_RED, 0);
    sm_sprintf(buf, sizeof(buf), "HOSTILES:  %d types      ENCOUNTER RATE: %s",
               s->enemy_type_count, s->enemy_density >= 4 ? "heavy" : (s->enemy_density >= 3 ? "moderate" : "light"));
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 36.f, iy + 100.f), buf, UI_DIM, 0);
    sm_sprintf(buf, sizeof(buf), "DEPOSITS:  %d asteroid field", s->asteroid_count);
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 36.f, iy + 124.f), buf, UI_DIM, 0);
}

/* ---------------------------------------------------------------- awards --- */
void ui_dock_awards(Game *g, Rect area) {
    Gfx *gfx = g->gfx;
    ui_panel(g, area, UI_GOLD);
    int unlocked = 0;
    for (int i = 0; i < ACH_COUNT; i++) if (g->save.achievements & (1u << i)) unlocked++;
    char sub[64];
    sm_sprintf(sub, sizeof(sub), "%d / %d unlocked", unlocked, ACH_COUNT);
    ui_header(g, rect(area.x + 20.f, area.y + 12.f, area.w - 40.f, 40.f), "ACHIEVEMENTS", sub, UI_GOLD);
    f32 rowh = 44.f;
    int cols = 2;
    f32 cw = (area.w - 40.f) / (f32)cols;
    for (int i = 0; i < ACH_COUNT; i++) {
        int got = (g->save.achievements & (1u << i)) != 0;
        Rect r = rect(area.x + 20.f + (f32)(i % cols) * cw, area.y + 66.f + (f32)(i / cols) * rowh,
                      cw - 10.f, rowh - 8.f);
        gfx_fill(gfx, r, got ? SM_RGBA(24, 26, 18, 220) : SM_RGBA(12, 16, 22, 200));
        gfx_rect_outline(gfx, r, 1.f, got ? SM_RGBA(200, 170, 90, 140) : SM_RGBA(60, 70, 84, 100));
        gfx_blit(gfx, &g->assets.ui_star, 0, rect(r.x + 10.f, r.y + 8.f, 20.f, 20.f),
                 got ? UI_GOLD : SM_RGBA(70, 78, 92, 255), 0.95f, BLEND_ALPHA);
        gfx_text(gfx, &g->assets.font_small, v2(r.x + 38.f, r.y + 5.f), k_achievements[i].name,
                 got ? UI_INK : UI_DIMMER, 0);
        gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 38.f, r.y + 23.f),
                 got ? k_achievements[i].desc : k_achievements[i].hint,
                 got ? UI_DIM : SM_RGB(80, 92, 106), 0);
    }
    /* lifetime stats */
    f32 sy = area.y + area.h - 120.f;
    gfx_fill(gfx, rect(area.x + 20.f, sy - 8.f, area.w - 40.f, 1.f), SM_RGBA(90, 110, 130, 90));
    char buf[128];
    sm_sprintf(buf, sizeof(buf), "ASTEROIDS MINED %d     ENEMIES DESTROYED %d     CREDITS EARNED %lld",
               g->save.stat_asteroids_mined, g->save.stat_enemies_killed, (long long)g->save.stat_credits_earned);
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f, sy + 10.f), buf, UI_DIM, 0);
    char tb[32];
    sm_format_time(tb, sizeof(tb), g->save.stat_time_played);
    sm_sprintf(buf, sizeof(buf), "FLIGHT TIME %s     DISTANCE %.0f km     VOID SENTINELS KILLED %d",
               tb, (double)g->save.stat_distance, g->save.stat_sentinels_killed);
    gfx_text(gfx, &g->assets.font_small, v2(area.x + 22.f, sy + 34.f), buf, UI_DIM, 0);
}

/* -------------------------------------------------------------- settings --- */
void ui_dock_settings(Game *g, Rect area) {
    Gfx *gfx = g->gfx;
    ui_panel(g, area, UI_CYAN);
    ui_header(g, rect(area.x + 20.f, area.y + 12.f, area.w - 40.f, 40.f), "SYSTEMS", "audio / video / interface", UI_CYAN);
    f32 x = area.x + 30.f, y = area.y + 84.f, w = SM_MIN(520.f, area.w - 60.f);
    if (ui_slider(g, rect(x, y, w, 22.f), &g->save.audio.master, "MASTER VOLUME", UI_CYAN))
        audio_set_gain(g->save.audio.master, g->save.audio.music, g->save.audio.sfx);
    y += 48.f;
    if (ui_slider(g, rect(x, y, w, 22.f), &g->save.audio.music, "MUSIC", UI_CYAN))
        audio_set_gain(g->save.audio.master, g->save.audio.music, g->save.audio.sfx);
    y += 48.f;
    if (ui_slider(g, rect(x, y, w, 22.f), &g->save.audio.sfx, "EFFECTS", UI_CYAN))
        audio_set_gain(g->save.audio.master, g->save.audio.music, g->save.audio.sfx);
    y += 52.f;
    ui_toggle(g, rect(x, y, w, 26.f), "BLOOM LIGHTING", &g->save.video.bloom, UI_CYAN); y += 34.f;
    ui_toggle(g, rect(x, y, w, 26.f), "SCREEN SHAKE", &g->save.video.shake, UI_CYAN); y += 34.f;
    ui_toggle(g, rect(x, y, w, 26.f), "CRT SCANLINES", &g->save.video.scanlines, UI_CYAN); y += 34.f;
    ui_toggle(g, rect(x, y, w, 26.f), "CONTROL HINTS", &g->save.gameplay.show_hints, UI_CYAN); y += 34.f;
    ui_toggle(g, rect(x, y, w, 26.f), "AUTOSAVE", &g->save.gameplay.autosave, UI_CYAN); y += 42.f;
    ui_label_small(g, v2(x, y), "PARTICLE DENSITY", UI_DIM, 0);
    const char *dens[] = { "LOW", "MEDIUM", "HIGH" };
    for (int i = 0; i < 3; i++) {
        Rect r = rect(x + w - 300.f + (f32)i * 100.f, y - 8.f, 94.f, 28.f);
        int hover = ui_hover(g, r);
        ui_button_draw(g, r, dens[i], NULL,
                       g->save.video.particles == i + 1 ? BTN_SELECTED : (hover ? BTN_HOVER : BTN_NORMAL), UI_CYAN);
        if (hover && g_ui_in && g_ui_in->mouse_pressed[0]) g->save.video.particles = i + 1;
    }
    y += 44.f;
    ui_label_small(g, v2(x, y), "DIFFICULTY", UI_DIM, 0);
    const char *diff[] = { "RELAXED", "STANDARD", "HARDCORE" };
    for (int i = 0; i < 3; i++) {
        Rect r = rect(x + w - 300.f + (f32)i * 100.f, y - 8.f, 94.f, 28.f);
        int hover = ui_hover(g, r);
        ui_button_draw(g, r, diff[i], NULL,
                       g->save.gameplay.difficulty == i ? BTN_SELECTED : (hover ? BTN_HOVER : BTN_NORMAL), UI_CYAN);
        if (hover && g_ui_in && g_ui_in->mouse_pressed[0]) g->save.gameplay.difficulty = i;
    }
    y += 52.f;
    if (ui_button(g, rect(x, y, 220.f, 38.f), "SAVE PROGRESS", NULL, 1, UI_GREEN)) {
        if (save_write(g) == 0) game_toast(g, "PROGRESS SAVED", UI_GREEN);
    }
    if (ui_button(g, rect(x + 240.f, y, 220.f, 38.f), "MAIN MENU", "progress is saved", 1, UI_RED)) {
        save_write(g);
        g->paused = 0;
        game_set_screen(g, SCREEN_MENU);
        snd_music(g, "music_menu", 1.f);
    }
    gfx_text(gfx, &g->assets.font_tiny, v2(area.x + 30.f, area.y + area.h - 26.f),
             "F1 perf overlay    F2 screenshot    F3 bloom toggle    F11 fullscreen", UI_DIMMER, 0);
}
