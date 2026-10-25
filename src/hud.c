// In-game HUD and the secondary screens (pause, shop, settings, controls, end).
#include "ui.h"
#include <string.h>
#include <stdio.h>

static int count_alive_hack(Game *g) {
    int n = 0;
    for (int i = 0; i < MAX_ZOMBIES; i++)
        if (g->w->zombies[i].alive && g->w->zombies[i].state != ZS_DEAD) n++;
    return n;
}

const char *g_tip(int night) {
    static const char *tips[] = {
        "Board the windows before the sun goes down. Every opening is a way in.",
        "Zombies break wood quickly. Reinforced barricades buy minutes, not seconds.",
        "Scrap is the only currency that matters after dark. Search the aisles.",
        "A crawler can slip under a barricade built for a walker.",
        "Screamers call the whole lot down on you. Kill them first.",
        "Your flashlight is not infinite. Batteries are in the electronics aisle.",
        "The Brute does not stagger. Do not stand in front of it with a bat.",
        "The loading dock doors are the widest opening in the building.",
        "Aim down sights to tighten your spread. A pistol headshot still kills.",
        "If a friend goes down, get to them and hold E. Nobody survives alone.",
    };
    return tips[(night * 3 + 1) % 10];
}

// ---------------------------------------------------------------- small parts
static void icon_heart(Game *g, float x, float y, float s, uint32_t c) {
    Renderer *r = g->rd;
    // a simple cross/plus medical mark, drawn from rectangles
    rd2d_rect(r, x + s * 0.36f, y, s * 0.28f, s, c);
    rd2d_rect(r, x, y + s * 0.32f, s, s * 0.30f, c);
}
static void icon_stamina(Game *g, float x, float y, float s, uint32_t c) {
    Renderer *r = g->rd;
    rd2d_tri(r, x + s * 0.45f, y, x + s, y + s * 0.55f, x + s * 0.55f, y + s * 0.55f, c);
    rd2d_tri(r, x + s * 0.55f, y + s, x, y + s * 0.45f, x + s * 0.45f, y + s * 0.45f, c);
}
static void icon_ammo(Game *g, float x, float y, float s, uint32_t c) {
    Renderer *r = g->rd;
    rd2d_rect(r, x, y + s * 0.15f, s * 0.22f, s * 0.85f, c);
    rd2d_rect(r, x + s * 0.3f, y + s * 0.15f, s * 0.22f, s * 0.85f, c);
    rd2d_rect(r, x + s * 0.6f, y + s * 0.15f, s * 0.22f, s * 0.85f, c);
    rd2d_rect(r, x, y, s * 0.82f, s * 0.12f, c);
}

static void draw_crosshair(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    Player *p = &w->players[w->local_player];
    if (!p->alive) return;
    if (w->state != GS_NIGHT && w->state != GS_NIGHT_INTRO && w->state != GS_DAY && w->state != GS_NIGHT_END) return;
    float cx = (float)r->w * 0.5f, cy = (float)r->h * 0.5f;
    const WeaponDef *wd = &g_weapon_defs[p->weapon];
    float spread = wd->spread * (1.f - p->aim_t * 0.72f) * 620.f + (wd->kind == 0 ? 3.f : 1.f);
    spread = clampf(spread, 3.f, 46.f);
    uint32_t c = ui_alpha(UI_INK, 0.88f);
    if (p->weapon == WP_SNIPER && p->aim_t > 0.85f) {
        // scope reticle
        float rr = (float)r->h * 0.34f;
        rd2d_rect(r, 0, 0, (float)r->w, cy - rr, ui_alpha(BLACK, 0.97f));
        rd2d_rect(r, 0, cy + rr, (float)r->w, (float)r->h - (cy + rr), ui_alpha(BLACK, 0.97f));
        rd2d_rect(r, 0, cy - rr, cx - rr, rr * 2.f, ui_alpha(BLACK, 0.97f));
        rd2d_rect(r, cx + rr, cy - rr, (float)r->w - (cx + rr), rr * 2.f, ui_alpha(BLACK, 0.97f));
        rd2d_circle_outline(r, cx, cy, rr, 2.f, ui_alpha(BLACK, 0.95f));
        rd2d_circle_outline(r, cx, cy, rr + 1.f, 1.f, ui_alpha(0xFF607080u, 0.5f));
        rd2d_rect(r, cx - rr, cy - 0.5f, rr * 2.f, 1.f, ui_alpha(0xFF101010u, 0.85f));
        rd2d_rect(r, cx - 0.5f, cy - rr, 1.f, rr * 2.f, ui_alpha(0xFF101010u, 0.85f));
        rd2d_rect(r, cx - 1.5f, cy - 1.5f, 3.f, 3.f, ui_alpha(UI_DANGER, 0.95f));
        return;
    }
    if (p->weapon != WP_FISTS && p->weapon != WP_BAT && p->weapon != WP_AXE && p->aim_t > 0.4f) {
        // red dot
        rd2d_rect(r, cx - 1.5f, cy - 1.5f, 3.f, 3.f, ui_alpha(0xFFFF2020u, 0.95f));
        rd2d_circle_outline(r, cx, cy, 4.f, 1.f, ui_alpha(0xFFFF2020u, 0.25f));
        return;
    }
    float len = 7.f, th = 2.f;
    rd2d_rect(r, cx - spread - len, cy - th * 0.5f, len, th, c);
    rd2d_rect(r, cx + spread, cy - th * 0.5f, len, th, c);
    rd2d_rect(r, cx - th * 0.5f, cy - spread - len, th, len, c);
    rd2d_rect(r, cx - th * 0.5f, cy + spread, th, len, c);
    rd2d_rect(r, cx - 1.f, cy - 1.f, 2.f, 2.f, ui_alpha(UI_INK, 0.55f));
}

static void draw_hitmarker(Game *g) {
    World *w = g->w;
    if (w->hitmarker <= 0.f) return;
    Renderer *r = g->rd;
    float cx = (float)r->w * 0.5f, cy = (float)r->h * 0.5f;
    float a = saturatef(w->hitmarker);
    uint32_t c = w->hitmarker_kill ? ui_alpha(0xFFFF3030u, a) : ui_alpha(UI_INK, a * 0.9f);
    float s0 = 6.f, s1 = 14.f + (1.f - a) * 6.f;
    for (int i = 0; i < 4; i++) {
        float dx = (i & 1) ? 1.f : -1.f;
        float dy = (i & 2) ? 1.f : -1.f;
        rd2d_line(r, cx + dx * s0, cy + dy * s0, cx + dx * s1, cy + dy * s1, 2.f, c);
    }
}

static void draw_compass(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    Player *p = &w->players[w->local_player];
    float u = ui_scale(g);
    float W = (float)r->w;
    float cw = 360.f * u, cx = W * 0.5f, y = 16.f * u;
    // heading in degrees, 0 = north (+Z)
    float heading = RAD2DEG(p->yaw);
    while (heading < 0) heading += 360.f;
    while (heading >= 360.f) heading -= 360.f;
    static const char *dirs[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    rd2d_grad_h(r, cx - cw * 0.5f, y, cw, 22.f * u, ui_alpha(0xFF101216u, 0.55f), ui_alpha(0xFF101216u, 0.0f));
    for (int i = -8; i <= 8; i++) {
        float ang = (heading + (float)i * 15.f);
        float rel = (float)i * 15.f;
        float x = cx + rel * (cw / 180.f);
        if (x < cx - cw * 0.5f || x > cx + cw * 0.5f) continue;
        float dist = fabsf(rel) / 120.f;
        float a = 1.f - saturatef(dist);
        int deg = (int)(ang + 360.f) % 360;
        int idx = (int)(((float)deg + 22.5f) / 45.f) % 8;
        char buf[4];
        if (deg % 45 == 0) snprintf(buf, sizeof buf, "%s", dirs[idx]);
        else snprintf(buf, sizeof buf, "%d", deg);
        int major = (deg % 45 == 0);
        rd2d_rect(r, x - 0.5f, y + 3.f * u, 1.f, (major ? 8.f : 4.f) * u, ui_alpha(UI_INK, a * 0.7f));
        if (major) ui_label_c(g, g->as->f_mono, x, y + 12.f * u, 0.52f * u, buf, ui_alpha(UI_INK, a * 0.85f));
    }
    rd2d_rect(r, cx - 0.5f, y + 1.f * u, 1.f, 4.f * u, ui_alpha(UI_ACCENT, 0.95f));
}

// ---------------------------------------------------------------- HUD
void draw_hud(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    Player *p = &w->players[w->local_player];
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);

    // screen effects under the UI
    if (p->hit_flash > 0.f) rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF1418C8u, p->hit_flash * 0.35f));
    if (w->damage_flash > 0.f) {
        // radial-ish blood edge
        for (int i = 0; i < 6; i++) {
            float t = (float)i / 5.f;
            rd2d_rect_outline(r, -t * 20.f, -t * 20.f, W + t * 40.f, H + t * 40.f, 22.f,
                              ui_alpha(0xFF1018B0u, w->damage_flash * 0.16f * (1.f - t)));
        }
    }
    if (w->heal_flash > 0.f) rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF60C080u, w->heal_flash * 0.12f));
    if (g->shot_flash > 0.f) rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFFFFFFFFu, g->shot_flash * 0.045f));
    if (g->lightning > 0.f) rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFFE8F0FFu, g->lightning * 0.22f));
    // low health pulse
    if (p->alive && p->hp < 35.f) {
        float pulse = 0.5f + 0.5f * sinf(g->ui_time * 6.5f);
        rd2d_rect_outline(r, 0, 0, W, H, 26.f * u, ui_alpha(0xFF1018C8u, 0.14f + pulse * 0.16f));
    }

    draw_compass(g);
    draw_crosshair(g);
    draw_hitmarker(g);

    // ---- damage direction indicators
    float ccx = W * 0.5f, ccy = H * 0.5f;
    for (int i = 0; i < 4; i++) {
        if (g->damage_dir_t[i] <= 0.f) continue;
        float a = saturatef(g->damage_dir_t[i]);
        // 0 = front, 1 = right (only one slot is used in single player)
        float ang = (float)i * (float)M_PI * 0.5f;
        float rad = 96.f * u;
        float x = ccx + sinf(ang) * rad, y = ccy - cosf(ang) * rad;
        float s = 16.f * u;
        rd2d_tri(r, x, y - s * 0.6f, x + s * 0.6f, y + s * 0.5f, x - s * 0.6f, y + s * 0.5f, ui_alpha(UI_DANGER, a * 0.75f));
    }

    // ---- night intro banner
    if (w->state == GS_NIGHT_INTRO) {
        float t = saturatef(w->state_time / 3.6f);
        float a = t < 0.2f ? t / 0.2f : (t > 0.8f ? (1.f - t) / 0.2f : 1.f);
        rd2d_rect(r, 0, H * 0.36f, W, 128.f * u, ui_alpha(BLACK, 0.55f * a));
        char buf[64];
        snprintf(buf, sizeof buf, "NIGHT %d", w->night);
        ui_heading(g, g->as->f_title, W * 0.5f, H * 0.385f, 1.08f * u, buf, ui_alpha(UI_INK, a));
        ui_label_c(g, g->as->f_reg, W * 0.5f, H * 0.385f + 62.f * u, 0.66f * u, w->cfg.subtitle, ui_alpha(0xFFC8BCA8u, a * 0.95f));
    }

    // ---- objective card, top left
    {
        float pw = 250.f * u, ph = 96.f * u;
        float x = 22.f * u, y = 22.f * u;
        ui_panel(g, x, y, pw, ph, 0.82f);
        const char *phase = w->state == GS_DAY ? "PREPARATION" : (w->state == GS_NIGHT_END ? "DAWN" : "ASSAULT");
        ui_label(g, g->as->f_mono, x + 12.f * u, y + 10.f * u, 0.52f * u, phase, ui_alpha(UI_ACCENT, 0.95f));
        char big[64];
        if (w->state == GS_DAY) snprintf(big, sizeof big, "DAY %d", w->night);
        else snprintf(big, sizeof big, "NIGHT %d", w->night);
        ui_label(g, g->as->f_title, x + 12.f * u, y + 24.f * u, 0.72f * u, big, UI_INK);
        int mins = (int)(maxf(0.f, w->night_timer) / 60.f);
        int secs = (int)fmodf(maxf(0.f, w->night_timer), 60.f);
        char tim[32];
        snprintf(tim, sizeof tim, "%d:%02d", mins, secs);
        ui_label(g, g->as->f_mono, x + 12.f * u, y + 60.f * u, 0.72f * u, tim,
                 w->state == GS_DAY ? ui_alpha(UI_INK, 0.9f) : ui_alpha(UI_WARN, 0.95f));
        if (w->state == GS_DAY) {
            ui_label(g, g->as->f_mono, x + 12.f * u, y + 76.f * u, 0.48f * u, "UNTIL NIGHTFALL", ui_alpha(UI_MUTED, 0.85f));
        } else {
            char rem[48];
            int left = w->cfg.total_zombies - w->spawned_this_night + count_alive_hack(g);
            snprintf(rem, sizeof rem, "%d HOSTILES", maxi(0, left));
            ui_label(g, g->as->f_mono, x + 12.f * u, y + 76.f * u, 0.48f * u, rem, ui_alpha(UI_MUTED, 0.85f));
        }
    }

    // ---- boss health
    if (w->boss_active && w->boss_hp_frac > 0.f) {
        float bw = minf(W * 0.6f, 620.f * u);
        float x = W * 0.5f - bw * 0.5f, y = 64.f * u;
        ui_panel(g, x - 10.f * u, y - 8.f * u, bw + 20.f * u, 56.f * u, 0.9f);
        ui_label_c(g, g->as->f_bold, W * 0.5f, y - 2.f * u, 0.62f * u, "THE STOREKEEPER", ui_alpha(0xFF3030D8u, 0.98f));
        ui_bar(g, x, y + 20.f * u, bw, 12.f * u, w->boss_hp_frac, 0xFF2020C0u, 0xFF18100Fu, 1.f);
        char ph[48];
        snprintf(ph, sizeof ph, "PHASE %d OF 3", 1 + (w->boss_hp_frac <= 0.66f ? 1 : 0) + (w->boss_hp_frac <= 0.33f ? 1 : 0));
        ui_label_c(g, g->as->f_mono, W * 0.5f, y + 34.f * u, 0.46f * u, ph, ui_alpha(UI_MUTED, 0.9f));
    }

    // ---- health / stamina / armour (bottom left)
    {
        float x = 30.f * u, y = H - 116.f * u;
        float bw = 250.f * u;
        icon_heart(g, x, y + 3.f * u, 18.f * u, ui_alpha(0xFF4040C8u, 0.95f));
        char hp[24];
        snprintf(hp, sizeof hp, "%d", (int)ceilf(p->hp));
        ui_label(g, g->as->f_bold, x + 26.f * u, y - 2.f * u, 0.66f * u, hp, p->hp < 35.f ? ui_alpha(0xFF5050E0u, 1.f) : UI_INK);
        ui_label(g, g->as->f_mono, x + 26.f * u + ui_text_w(g->as->f_bold, 0.66f * u, hp) + 6.f * u, y + 4.f * u,
                 0.48f * u, "HP", ui_alpha(UI_MUTED, 0.85f));
        float hcol = col_lerp(0xFF3030C8u, 0xFF60C060u, p->hp / 100.f);
        if (p->hp < 35.f) hcol = 0xFF3030C8u;
        ui_bar(g, x + 26.f * u, y + 20.f * u, bw - 26.f * u, 10.f * u, p->hp / p->max_hp, hcol, 0xFF18181Cu, 1.f);
        // stamina
        icon_stamina(g, x, y + 40.f * u, 16.f * u, ui_alpha(0xFFB0D0A0u, 0.9f));
        uint32_t scol = p->exhaustion > 0.f ? ui_alpha(UI_DANGER, 0.85f) : ui_alpha(0xFFA0D0C0u, 0.9f);
        ui_bar(g, x + 26.f * u, y + 44.f * u, (bw - 26.f * u) * 0.72f, 6.f * u, p->stamina / p->max_stamina, scol, 0xFF18181Cu, 1.f);
        // armour pips
        int pips = (int)(p->armor / 0.25f + 0.5f);
        for (int i = 0; i < 2; i++) {
            rd2d_rect_outline(r, x + 26.f * u + (bw - 26.f * u) * 0.76f + (float)i * 14.f * u, y + 41.f * u, 10.f * u, 12.f * u, 1.f,
                              ui_alpha(UI_MUTED, 0.5f));
            if (i < pips) rd2d_rect(r, x + 27.f * u + (bw - 26.f * u) * 0.76f + (float)i * 14.f * u, y + 42.f * u, 8.f * u, 10.f * u,
                                    ui_alpha(0xFF90C8F0u, 0.85f));
        }
        // flashlight charge
        if (p->flashlight_on > 0.5f) {
            ui_bar(g, x + 26.f * u, y + 56.f * u, 64.f * u, 4.f * u, p->flashlight_charge / 100.f,
                   ui_alpha(0xFFF0E0A0u, 0.8f), 0xFF18181Cu, 0.8f);
            ui_label(g, g->as->f_mono, x + 96.f * u, y + 55.f * u, 0.42f * u, "LIGHT", ui_alpha(UI_MUTED, 0.7f));
        } else {
            ui_label(g, g->as->f_mono, x + 26.f * u, y + 55.f * u, 0.42f * u, "LIGHT OFF  [F]", ui_alpha(UI_MUTED, 0.6f));
        }
        if (p->heal_charges > 0 && p->hp < p->max_hp)
            ui_label(g, g->as->f_mono, x + 26.f * u, y + 66.f * u, 0.46f * u, "[H] USE MEDKIT", ui_alpha(0xFF80C8A0u, 0.75f));
    }

    // ---- ammo / weapon (bottom right)
    {
        const WeaponDef *wd = &g_weapon_defs[p->weapon];
        float x = W - 30.f * u, y = H - 122.f * u;
        ui_label_r(g, g->as->f_bold, x, y, 0.58f * u, wd->name, ui_alpha(UI_INK, 0.95f));
        char big[32];
        if (wd->kind == 0) snprintf(big, sizeof big, "--");
        else snprintf(big, sizeof big, "%d", p->mag[p->weapon]);
        ui_label_r(g, g->as->f_title, x, y + 22.f * u, 0.95f * u, big, UI_INK);
        char res[48];
        if (wd->kind == 0) snprintf(res, sizeof res, "MELEE");
        else snprintf(res, sizeof res, "/ %d  %s", p->ammo[wd->ammo_type], ammo_name(wd->ammo_type));
        ui_label_r(g, g->as->f_mono, x, y + 66.f * u, 0.58f * u, res, ui_alpha(UI_MUTED, 0.95f));
        icon_ammo(g, x - ui_text_w(g->as->f_mono, 0.58f * u, res) - 20.f * u, y + 68.f * u, 12.f * u, ui_alpha(UI_MUTED, 0.6f));
        // magazine pips
        if (wd->kind != 0 && wd->mag_size > 0) {
            int n = wd->mag_size;
            float pw = 4.f * u, gap = 2.f * u;
            float total = (float)n * (pw + gap);
            float px = x - total;
            for (int i = 0; i < n; i++) {
                int filled = i < p->mag[p->weapon];
                rd2d_rect(r, px + (float)i * (pw + gap), y + 82.f * u, pw, 9.f * u,
                          filled ? ui_alpha(UI_ACCENT, 0.95f) : ui_alpha(0xFF3A3A3Eu, 0.9f));
            }
        }
        if (p->reloading) {
            float t = 1.f - saturatef(p->reload_t / maxf(0.01f, wd->reload_time));
            ui_bar(g, x - 160.f * u, y + 94.f * u, 160.f * u, 4.f * u, t, ui_alpha(UI_ACCENT, 0.9f), 0xFF18181Cu, 1.f);
            ui_label_r(g, g->as->f_mono, x, y + 100.f * u, 0.44f * u, "RELOADING", ui_alpha(UI_ACCENT, 0.9f));
        }
    }

    // ---- weapon slots
    {
        float sw = 84.f * u, sh = 26.f * u, gap = 6.f * u;
        int owned = 0;
        for (int i = 0; i < WP_COUNT; i++) if (p->owned[i]) owned++;
        float total = (float)owned * (sw + gap) - gap;
        float x = W * 0.5f - total * 0.5f, y = H - 42.f * u;
        int k = 0;
        for (int i = 0; i < WP_COUNT; i++) {
            if (!p->owned[i]) continue;
            int cur = (p->weapon == i);
            uint32_t bg = cur ? ui_alpha(0xFF2A2620u, 0.95f) : ui_alpha(0xFF111114u, 0.72f);
            rd2d_rect(r, x + (float)k * (sw + gap), y, sw, sh, bg);
            rd2d_rect_outline(r, x + (float)k * (sw + gap), y, sw, sh, 1.f,
                              cur ? ui_alpha(UI_ACCENT, 0.95f) : ui_alpha(0xFF3C3C40u, 0.8f));
            char nm[32];
            snprintf(nm, sizeof nm, "%d %s", i + 1, g_weapon_defs[i].name);
            // truncate long names
            if (ui_text_w(g->as->f_mono, 0.44f * u, nm) > sw - 8.f * u) snprintf(nm, sizeof nm, "%d %.6s", i + 1, g_weapon_defs[i].name);
            ui_label(g, g->as->f_mono, x + (float)k * (sw + gap) + 6.f * u, y + 8.f * u, 0.44f * u, nm,
                     cur ? UI_INK : ui_alpha(UI_MUTED, 0.85f));
            k++;
        }
    }

    // ---- interaction prompt
    if (g->interact_target_t > 0.f && g->interact_text[0]) {
        float pw = ui_text_w(g->as->f_bold, 0.6f * u, g->interact_text) + 44.f * u;
        float x = W * 0.5f - pw * 0.5f, y = H * 0.60f;
        ui_panel(g, x, y, pw, 40.f * u, 0.92f);
        ui_label(g, g->as->f_bold, x + 14.f * u, y + 11.f * u, 0.6f * u, g->interact_text, UI_INK);
    }

    // ---- subtitles
    if (w->subtitle_t > 0.f && w->subtitle[0]) {
        float a = saturatef(w->subtitle_t);
        float pw = ui_text_w(g->as->f_reg, 0.62f * u, w->subtitle) + 40.f * u;
        float x = W * 0.5f - pw * 0.5f, y = H * 0.72f;
        ui_panel(g, x, y, pw, 34.f * u, 0.78f * a);
        ui_label_c(g, g->as->f_reg, W * 0.5f, y + 9.f * u, 0.62f * u, w->subtitle, ui_alpha(0xFFE8E0D0u, a));
    }
    // ---- kill notification
    if (w->kill_notify_t > 0.f) {
        ui_label_c(g, g->as->f_bold, W * 0.5f, H * 0.63f, 0.6f * u, w->kill_notify,
                   ui_alpha(UI_ACCENT, saturatef(w->kill_notify_t)));
    }
    // ---- day phase hint
    if (w->state == GS_DAY) {
        char buf[128];
        snprintf(buf, sizeof buf, "SUPPLIES  $%d      SCRAP  %d      NEXT NIGHT IN %d:%02d",
                 p->money, p->scrap, (int)(w->night_timer / 60.f), (int)fmodf(w->night_timer, 60.f));
        ui_label_c(g, g->as->f_mono, W * 0.5f, H - 62.f * u, 0.52f * u, buf, ui_alpha(0xFFB8B0A0u, 0.9f));
    }
    // ---- zombie proximity vignette (audio-visual dread)
    int near = 0;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie *z = &w->zombies[i];
        if (!z->alive || z->state == ZS_DEAD) continue;
        if (vdist(z->pos, p->pos) < 9.f) near++;
    }
    if (near > 0 && w->state != GS_DAY) {
        float a = minf(0.30f, (float)near * 0.045f) * (0.6f + 0.4f * sinf(g->ui_time * 4.f));
        rd2d_rect_outline(r, 0, 0, W, H, 40.f * u, ui_alpha(0xFF08060Au, a));
    }
    // ---- held-tab inventory
    if (g->plat->in.down[K_TAB]) {
        ui_panel(g, W * 0.5f - 190.f * u, H * 0.5f - 120.f * u, 380.f * u, 240.f * u, 0.94f);
        ui_heading(g, g->as->f_bold, W * 0.5f, H * 0.5f - 100.f * u, 0.62f * u, "INVENTORY", UI_ACCENT);
        float y = H * 0.5f - 66.f * u;
        char l1[64];
        snprintf(l1, sizeof l1, "HOSTILES DOWNED   %d", w->kills);
        ui_label(g, g->as->f_mono, W * 0.5f - 160.f * u, y, 0.52f * u, l1, UI_INK); y += 24.f * u;
        snprintf(l1, sizeof l1, "HEADSHOTS         %d", w->headshots);
        ui_label(g, g->as->f_mono, W * 0.5f - 160.f * u, y, 0.52f * u, l1, UI_INK); y += 24.f * u;
        snprintf(l1, sizeof l1, "SCRAP             %d", p->scrap);
        ui_label(g, g->as->f_mono, W * 0.5f - 160.f * u, y, 0.52f * u, l1, UI_INK); y += 24.f * u;
        snprintf(l1, sizeof l1, "MEDKITS           %d", p->heal_charges);
        ui_label(g, g->as->f_mono, W * 0.5f - 160.f * u, y, 0.52f * u, l1, UI_INK); y += 24.f * u;
        snprintf(l1, sizeof l1, "AMMO  9MM %d  12GA %d  5.56 %d  BOLT %d", p->ammo[AMMO_PISTOL], p->ammo[AMMO_SHELL],
                 p->ammo[AMMO_RIFLE], p->ammo[AMMO_BOLT]);
        ui_label(g, g->as->f_mono, W * 0.5f - 160.f * u, y, 0.46f * u, l1, ui_alpha(UI_MUTED, 0.95f));
    }
}

// ---------------------------------------------------------------- pause
void draw_pause(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF06070Au, 0.72f));
    float pw = minf(W * 0.5f, 520.f * u), ph = 470.f * u;
    float x = W * 0.5f - pw * 0.5f, y = H * 0.5f - ph * 0.5f;
    ui_panel(g, x, y, pw, ph, 0.96f);
    ui_heading(g, g->as->f_title, W * 0.5f, y + 22.f * u, 0.86f * u, "PAUSED", UI_ACCENT);
    char sub[96];
    snprintf(sub, sizeof sub, "NIGHT %d   -   %d KILLS", w->night, w->night_kills);
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + 62.f * u, 0.52f * u, sub, ui_alpha(UI_MUTED, 0.95f));
    float bw = pw - 80.f * u, bh = 44.f * u;
    for (int i = 0; i < g->menu_count; i++) {
        int hov = ui_button(g, x + 40.f * u, y + 96.f * u + (float)i * (bh + 10.f * u), bw, bh,
                            g->menu[i].label, g->menu_sel == i, 1.f, 1);
        if (hov && g->plat->in.mouse_pressed[0]) { g->menu_sel = i; }
    }
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + ph - 34.f * u, 0.46f * u,
               "ESC RESUME   -   OPTIONS ARE SAVED AUTOMATICALLY", ui_alpha(UI_MUTED, 0.7f));
}

// ---------------------------------------------------------------- shop
void draw_shop(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);
    Player *p = &w->players[w->local_player];
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF06070Au, 0.80f));
    float pw = minf(W * 0.86f, 1000.f * u), ph = minf(H * 0.86f, 620.f * u);
    float x = W * 0.5f - pw * 0.5f, y = H * 0.5f - ph * 0.5f;
    ui_panel(g, x, y, pw, ph, 0.97f);
    ui_heading(g, g->as->f_title, x + pw * 0.5f, y + 18.f * u, 0.78f * u, "SUPPLY DESK", UI_ACCENT);
    char cash[64];
    snprintf(cash, sizeof cash, "$%d", p->money);
    ui_label_r(g, g->as->f_title, x + pw - 26.f * u, y + 22.f * u, 0.72f * u, cash, ui_alpha(0xFF90D890u, 1.f));
    ui_label(g, g->as->f_mono, x + 26.f * u, y + 30.f * u, 0.5f * u, "FUNDS", ui_alpha(UI_MUTED, 0.8f));

    int n = game_shop_count();
    float list_x = x + 26.f * u, list_y = y + 74.f * u;
    float row_h = 26.f * u;
    int visible = (int)((ph - 190.f * u) / row_h);
    int first = clampi(g->shop_sel - visible / 2, 0, maxi(0, n - visible));
    // frame for the list
    rd2d_rect(r, list_x - 8.f * u, list_y - 8.f * u, pw * 0.58f, (float)visible * row_h + 16.f * u, ui_alpha(0xFF0E0F12u, 0.7f));
    for (int i = first; i < first + visible && i < n; i++) {
        char name[64], price[32];
        snprintf(name, sizeof name, "%s", game_shop_name(i));
        game_shop_label(i, g, price, sizeof price);
        int sel = (i == g->shop_sel);
        float ry = list_y + (float)(i - first) * row_h;
        float hov = (g->plat->in.mouse_x > list_x - 8.f * u && g->plat->in.mouse_x < list_x + pw * 0.58f - 16.f * u &&
                     g->plat->in.mouse_y > ry - 2.f * u && g->plat->in.mouse_y < ry + row_h - 2.f * u);
        if (sel || hov) {
            rd2d_rect(r, list_x - 8.f * u, ry - 2.f * u, pw * 0.58f, row_h, ui_alpha(0xFF2A2620u, sel ? 0.95f : 0.6f));
            rd2d_rect(r, list_x - 8.f * u, ry - 2.f * u, 3.f * u, row_h, ui_alpha(UI_ACCENT, 0.95f));
            if (hov && g->plat->in.mouse_pressed[0]) g->shop_sel = i;
        }
        ui_label(g, g->as->f_bold, list_x, ry + 3.f * u, 0.5f * u, name, sel ? UI_INK : ui_alpha(0xFFC8C4BCu, 0.95f));
        ui_label_r(g, g->as->f_mono, list_x + pw * 0.56f, ry + 4.f * u, 0.5f * u, price,
                   strcmp(price, "OWNED") == 0 || strcmp(price, "MAX") == 0 ? ui_alpha(UI_MUTED, 0.7f) : ui_alpha(0xFF90D890u, 0.95f));
    }
    // scroll bar
    if (n > visible) {
        float sbh = (float)visible / (float)n * (float)visible * row_h;
        float sby = list_y + (float)first / (float)n * (float)visible * row_h;
        rd2d_rect(r, list_x + pw * 0.58f - 6.f * u, list_y, 3.f * u, (float)visible * row_h, ui_alpha(0xFF22242Au, 0.8f));
        rd2d_rect(r, list_x + pw * 0.58f - 6.f * u, sby, 3.f * u, sbh, ui_alpha(UI_ACCENT, 0.7f));
    }
    // detail pane
    float dx = x + pw * 0.62f, dy = list_y;
    rd2d_rect(r, dx, dy, pw * 0.34f, ph - 220.f * u, ui_alpha(0xFF101114u, 0.8f));
    rd2d_rect_outline(r, dx, dy, pw * 0.34f, ph - 220.f * u, 1.f, ui_alpha(0xFF34363Cu, 0.9f));
    ui_label(g, g->as->f_bold, dx + 14.f * u, dy + 12.f * u, 0.62f * u, game_shop_name(g->shop_sel), UI_ACCENT);
    // wrap the description
    const char *desc = game_shop_desc(g->shop_sel);
    char line[64];
    int li = 0;
    float ty = dy + 44.f * u;
    for (const char *s = desc; *s;) {
        int take = 32;
        int len = (int)strlen(s);
        if (take > len) take = len;
        while (take > 8 && s[take] != ' ') take--;
        memcpy(line, s, (size_t)take);
        line[take] = 0;
        ui_label(g, g->as->f_reg, dx + 14.f * u, ty + (float)li * 19.f * u, 0.52f * u, line, ui_alpha(0xFFC0BCB4u, 0.95f));
        s += take;
        while (*s == ' ') s++;
        li++;
        if (li > 8) break;
    }
    // foot buttons
    float by = y + ph - 86.f * u;
    ui_button(g, x + 26.f * u, by, 230.f * u, 46.f * u, "BUY  [ENTER]", 1, 1.f, 1);
    int close_hot = (g->plat->in.mouse_x > x + 272.f * u && g->plat->in.mouse_x < x + 272.f * u + 230.f * u &&
                     g->plat->in.mouse_y > by && g->plat->in.mouse_y < by + 46.f * u);
    ui_button(g, x + 272.f * u, by, 230.f * u, 46.f * u, "CLOSE  [ESC]", close_hot, 1.f, 1);
    if (close_hot && g->plat->in.mouse_pressed[0]) { w->state = GS_DAY; }
    if (g->plat->in.mouse_pressed[0] && g->plat->in.mouse_x > x + 26.f * u &&
        g->plat->in.mouse_x < x + 256.f * u && g->plat->in.mouse_y > by && g->plat->in.mouse_y < by + 46.f * u)
        game_shop_buy(g, g->shop_sel);
    ui_label_c(g, g->as->f_mono, x + pw * 0.5f, y + ph - 30.f * u, 0.46f * u,
               "UP/DOWN SELECT   ENTER BUY   ESC CLOSE", ui_alpha(UI_MUTED, 0.7f));
}

// ---------------------------------------------------------------- settings
void draw_settings(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF06070Au, 0.80f));
    float pw = minf(W * 0.62f, 700.f * u), ph = minf(H * 0.8f, 540.f * u);
    float x = W * 0.5f - pw * 0.5f, y = H * 0.5f - ph * 0.5f;
    ui_panel(g, x, y, pw, ph, 0.97f);
    ui_heading(g, g->as->f_title, W * 0.5f, y + 20.f * u, 0.75f * u, "SETTINGS", UI_ACCENT);
    const char *names[8] = {"MOUSE SENSITIVITY", "INVERT VERTICAL LOOK", "RENDER QUALITY", "RENDER SCALE",
                            "UI SCALE", "DIFFICULTY", "VSYNC", "FULLSCREEN"};
    char vals[8][48];
    snprintf(vals[0], sizeof vals[0], "%d", w->mouse_sensitivity);
    snprintf(vals[1], sizeof vals[1], "%s", w->invert_y ? "ON" : "OFF");
    snprintf(vals[2], sizeof vals[2], "%s", w->quality == 0 ? "LOW" : (w->quality == 1 ? "MEDIUM" : "HIGH"));
    snprintf(vals[3], sizeof vals[3], "%s", w->render_scale == 0 ? "75%" : (w->render_scale == 1 ? "100%" : "125%"));
    snprintf(vals[4], sizeof vals[4], "%s", w->ui_scale == 0 ? "SMALL" : (w->ui_scale == 1 ? "NORMAL" : "LARGE"));
    snprintf(vals[5], sizeof vals[5], "%s", w->difficulty == 0 ? "STORY" : (w->difficulty == 1 ? "NORMAL" : "NIGHTMARE"));
    snprintf(vals[6], sizeof vals[6], "%s", g->plat->vsync ? "ON" : "OFF");
    snprintf(vals[7], sizeof vals[7], "%s", g->plat->fullscreen ? "ON" : "OFF");
    for (int i = 0; i < 8; i++) {
        float ry = y + 78.f * u + (float)i * 44.f * u;
        int sel = (g->settings_sel == i);
        if (sel) {
            rd2d_rect(r, x + 24.f * u, ry - 4.f * u, pw - 48.f * u, 38.f * u, ui_alpha(0xFF2A2620u, 0.9f));
            rd2d_rect(r, x + 24.f * u, ry - 4.f * u, 3.f * u, 38.f * u, ui_alpha(UI_ACCENT, 0.95f));
        }
        ui_label(g, g->as->f_bold, x + 40.f * u, ry + 2.f * u, 0.52f * u, names[i], sel ? UI_INK : ui_alpha(0xFFB8B4ACu, 0.9f));
        float vw = 150.f * u;
        ui_label_c(g, g->as->f_mono, x + pw - 40.f * u - vw * 0.5f, ry + 2.f * u, 0.52f * u, vals[i],
                   sel ? ui_alpha(UI_ACCENT, 1.f) : ui_alpha(UI_MUTED, 0.9f));
        // arrows
        if (sel) {
            ui_label_c(g, g->as->f_mono, x + pw - 40.f * u - vw - 12.f * u, ry + 2.f * u, 0.52f * u, "<", ui_alpha(UI_ACCENT, 0.9f));
            ui_label_c(g, g->as->f_mono, x + pw - 40.f * u + 10.f * u, ry + 2.f * u, 0.52f * u, ">", ui_alpha(UI_ACCENT, 0.9f));
        }
    }
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + ph - 32.f * u, 0.46f * u,
               "UP/DOWN SELECT   LEFT/RIGHT CHANGE   ESC BACK", ui_alpha(UI_MUTED, 0.75f));
    // click rows
    if (g->plat->in.mouse_pressed[0]) {
        for (int i = 0; i < 8; i++) {
            float ry = y + 78.f * u + (float)i * 44.f * u;
            if (g->plat->in.mouse_x > x + 24.f * u && g->plat->in.mouse_x < x + pw - 24.f * u &&
                g->plat->in.mouse_y > ry - 4.f * u && g->plat->in.mouse_y < ry + 34.f * u) {
                g->settings_sel = i;
            }
        }
    }
}

// ---------------------------------------------------------------- controls
void draw_controls(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF06070Au, 0.86f));
    float pw = minf(W * 0.78f, 860.f * u), ph = minf(H * 0.88f, 620.f * u);
    float x = W * 0.5f - pw * 0.5f, y = H * 0.5f - ph * 0.5f;
    ui_panel(g, x, y, pw, ph, 0.97f);
    ui_heading(g, g->as->f_title, W * 0.5f, y + 18.f * u, 0.7f * u, "HOW TO PLAY", UI_ACCENT);
    const char *lines[][2] = {
        {"W A S D", "Move"},
        {"MOUSE", "Look"},
        {"LEFT MOUSE", "Attack / fire"},
        {"RIGHT MOUSE", "Aim down sights"},
        {"SHIFT", "Sprint (uses stamina)"},
        {"SPACE", "Jump"},
        {"R", "Reload"},
        {"1 - 8 / WHEEL", "Switch weapon"},
        {"F", "Flashlight"},
        {"H", "Use medkit"},
        {"E", "Interact / build / repair / buy"},
        {"TAB", "Hold: inventory & statistics"},
        {"ESC", "Pause"},
    };
    float col_y = y + 70.f * u;
    for (int i = 0; i < (int)(sizeof lines / sizeof lines[0]); i++) {
        float ry = col_y + (float)i * 22.f * u;
        ui_label(g, g->as->f_mono, x + 36.f * u, ry, 0.50f * u, lines[i][0], ui_alpha(UI_ACCENT, 0.95f));
        ui_label(g, g->as->f_reg, x + 240.f * u, ry, 0.50f * u, lines[i][1], ui_alpha(0xFFC4C0B8u, 0.95f));
    }
    // objective column
    float ox = x + pw * 0.56f;
    ui_label(g, g->as->f_bold, ox, col_y - 22.f * u, 0.56f * u, "THE JOB", UI_ACCENT);
    const char *job[] = {
        "You are trapped in a SAVE-N-STOCK supermarket.",
        "By day, search the aisles for supplies, cash and",
        "scrap. Spend it at the supply desk on weapons,",
        "ammunition and barricade upgrades.",
        "",
        "By night, the doors and windows must hold. Every",
        "opening can be barricaded three times: wood,",
        "reinforced, then welded metal.",
        "",
        "Survive ten nights. Night five brings THE BUTCHER.",
        "Night ten brings THE STOREKEEPER.",
    };
    for (int i = 0; i < (int)(sizeof job / sizeof job[0]); i++) {
        ui_label(g, g->as->f_reg, ox, col_y + 8.f * u + (float)i * 19.f * u, 0.48f * u, job[i], ui_alpha(0xFFB8B4ACu, 0.92f));
    }
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + ph - 28.f * u, 0.46f * u, "PRESS ESC OR ENTER TO RETURN", ui_alpha(UI_MUTED, 0.75f));
    if (g->plat->in.pressed[K_ESC] || g->plat->in.pressed[K_ENTER] || g->plat->in.mouse_pressed[0]) {
        // handled in game_frame
    }
}

// ---------------------------------------------------------------- end screens
void draw_gameover(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF0A0406u, 0.86f));
    for (int i = 0; i < 10; i++)
        rd2d_rect(r, 0, (float)i * H / 10.f, W, H / 22.f, ui_alpha(0xFF000000u, 0.05f));
    float pw = minf(W * 0.56f, 640.f * u), ph = 420.f * u;
    float x = W * 0.5f - pw * 0.5f, y = H * 0.5f - ph * 0.5f;
    ui_panel(g, x, y, pw, ph, 0.96f);
    ui_heading(g, g->as->f_title, W * 0.5f, y + 24.f * u, 1.0f * u, "YOU DIED", ui_alpha(0xFF3030C8u, 1.f));
    char sub[96];
    snprintf(sub, sizeof sub, "NIGHT %d   -   %d KILLS", w->night, w->night_kills);
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + 96.f * u, 0.58f * u, sub, ui_alpha(UI_MUTED, 0.95f));
    char st[128];
    snprintf(st, sizeof st, "TOTAL KILLS %d      HEADSHOTS %d      ACCURACY %d%%", w->kills, w->headshots,
             w->shots_fired ? (int)(100.f * (float)w->shots_hit / (float)w->shots_fired) : 0);
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + 128.f * u, 0.5f * u, st, ui_alpha(0xFF9A968Fu, 0.9f));
    float bw = pw - 120.f * u, bh = 46.f * u;
    for (int i = 0; i < g->menu_count; i++) {
        int hov = ui_button(g, x + 60.f * u, y + 176.f * u + (float)i * (bh + 12.f * u), bw, bh, g->menu[i].label,
                            g->menu_sel == i, 1.f, 1);
        if (hov && g->plat->in.mouse_pressed[0]) g->menu_sel = i;
    }
}

void draw_victory(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF04060Au, 0.90f));
    // sunrise gradient
    rd2d_grad_v(r, 0, 0, W, H * 0.55f, ui_alpha(0xFF2050A0u, 0.20f), 0x00000000);
    float pw = minf(W * 0.6f, 700.f * u), ph = 420.f * u;
    float x = W * 0.5f - pw * 0.5f, y = H * 0.5f - ph * 0.5f;
    ui_panel(g, x, y, pw, ph, 0.94f);
    ui_heading(g, g->as->f_title, W * 0.5f, y + 26.f * u, 1.05f * u, "DAWN", ui_alpha(0xFFB0E0A0u, 1.f));
    ui_label_c(g, g->as->f_reg, W * 0.5f, y + 108.f * u, 0.74f * u,
               "Ten nights. The doors held.", ui_alpha(0xFFE8E4D8u, 0.98f));
    char st[160];
    snprintf(st, sizeof st, "KILLS %d     HEADSHOTS %d     ACCURACY %d%%     DEATHS %d", w->kills, w->headshots,
             w->shots_fired ? (int)(100.f * (float)w->shots_hit / (float)w->shots_fired) : 0, w->deaths);
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + 168.f * u, 0.54f * u, st, ui_alpha(UI_MUTED, 0.95f));
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + 200.f * u, 0.5f * u,
               "THANK YOU FOR PLAYING THE LAST NIGHT", ui_alpha(UI_ACCENT, 0.95f));
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + ph - 44.f * u, 0.5f * u,
               "PRESS ENTER TO RETURN TO THE MENU", ui_alpha(UI_MUTED, 0.8f));
}

// ------------------------------------------------------------------ lobby
void draw_lobby(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h, u = ui_scale(g);
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF06070Au, 0.9f));
    float pw = minf(W * 0.62f, 620.f * u), ph = minf(H * 0.80f, 470.f * u);
    float x = W * 0.5f - pw * 0.5f, y = H * 0.5f - ph * 0.5f;
    ui_panel(g, x, y, pw, ph, 0.97f);
    ui_heading(g, g->as->f_title, W * 0.5f, y + 16.f * u, 0.66f * u, "CO-OP DEFENCE", UI_ACCENT);
    ui_label_c(g, g->as->f_mono, W * 0.5f, y + 54.f * u, 0.42f * u,
               "DIRECT IP / LAN - UP TO 4 SURVIVORS", ui_alpha(UI_MUTED, 0.95f));

    float by = y + 86.f * u;
    for (int i = 0; i < g->menu_count; i++) {
        float bx = x + 44.f * u, bw = pw - 88.f * u, bh = 32.f * u;
        int sel = (i == g->menu_sel);
        if (ui_button(g, bx, by + (float)i * 40.f * u, bw, bh, g->menu[i].label, sel, 1.f, g->menu[i].enabled))
            g->menu_sel = i;
        if (sel) {
            rd2d_rect(r, bx - 10.f * u, by + (float)i * 40.f * u + bh * 0.5f - 3.f * u, 5.f * u, 6.f * u,
                      ui_alpha(UI_ACCENT, 0.95f));
        }
    }

    // live network status, straight from the host/client state
    float sy = by + (float)g->menu_count * 40.f * u + 12.f * u;
    const char *mode = w->net.mode == 1 ? "HOST" : (w->net.mode == 2 ? "CLIENT" : "OFFLINE");
    char line[192];
    snprintf(line, sizeof line, "STATUS  %s    PORT %d    PEERS %d/%d", mode, w->net.port,
             w->net.peer_count, MAX_PLAYERS - 1);
    ui_label(g, g->as->f_mono, x + 44.f * u, sy, 0.42f * u, line,
             w->net.running ? ui_alpha(0xFF50C878u, 0.95f) : ui_alpha(UI_MUTED, 0.9f));
    snprintf(line, sizeof line, "PING  %.0f ms", w->net.ping * 1000.f);
    ui_label(g, g->as->f_mono, x + 44.f * u, sy + 18.f * u, 0.42f * u, line, ui_alpha(0xFFC4C0B8u, 0.9f));
    if (w->net.status[0])
        ui_label(g, g->as->f_reg, x + 44.f * u, sy + 40.f * u, 0.46f * u, w->net.status, ui_alpha(UI_INK, 0.9f));

    const char *note[3] = {
        "The host runs the simulation; clients send input and",
        "receive snapshots. Zombies, barricades and waves are",
        "owned by the host - nothing here is simulated twice.",
    };
    for (int i = 0; i < 3; i++)
        ui_label(g, g->as->f_reg, x + 44.f * u, y + ph - 74.f * u + (float)i * 16.f * u, 0.42f * u,
                 note[i], ui_alpha(UI_MUTED, 0.85f));
    ui_label(g, g->as->f_mono, x + pw - 44.f * u, y + ph - 24.f * u, 0.42f * u, "ESC  BACK",
             ui_alpha(UI_MUTED, 0.8f));
}
