// User interface: HUD, menus, shop, settings, death & victory screens.
// Everything is drawn with the in-house 2D layer — no default widgets.
#include "ui.h"
#include <stdlib.h>
#include <string.h>

#define ACCENT UI_ACCENT
#define ACCENT_DIM UI_ACCENT_DIM
#define DANGER UI_DANGER
#define INK UI_INK
#define MUTED UI_MUTED
#define PANEL_BG UI_PANEL
#define PANEL_EDGE UI_EDGE

uint32_t col_lerp(uint32_t a, uint32_t b, float t) {
    uint32_t ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    uint32_t br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ba = (b >> 24) & 0xFF;
    float s = saturatef(t);
    return COL((uint8_t)((float)ar + ((float)br - (float)ar) * s),
               (uint8_t)((float)ag + ((float)bg - (float)ag) * s),
               (uint8_t)((float)ab + ((float)bb - (float)ab) * s),
               (uint8_t)((float)aa + ((float)ba - (float)aa) * s));
}
uint32_t ui_alpha(uint32_t c, float a) {
    uint32_t al = (uint32_t)(saturatef(a) * 255.f);
    return (c & 0x00FFFFFFu) | (al << 24);
}

float ui_scale(Game *g) {
    float base = (float)g->rd->h / 720.f;
    switch (g->w->ui_scale) {
        case 0: base *= 0.82f; break;
        case 2: base *= 1.22f; break;
        default: break;
    }
    return clampf(base, 0.62f, 2.6f);
}

// ---------------------------------------------------------------- primitives
void ui_panel(Game *g, float x, float y, float w, float h, float alpha) {
    Renderer *r = g->rd;
    uint32_t bg = ui_alpha(PANEL_BG, alpha);
    rd2d_rect(r, x, y, w, h, bg);
    // subtle inner sheen
    rd2d_grad_v(r, x + 1, y + 1, w - 2, h * 0.45f, ui_alpha(0xFFFFFFFFu, 0.035f * alpha), 0x00000000);
    rd2d_rect_outline(r, x, y, w, h, 1.f, ui_alpha(PANEL_EDGE, alpha * 0.9f));
    // corner ticks
    float t = 1.f, L = 12.f * ui_scale(g);
    rd2d_rect(r, x, y, L, t, ui_alpha(ACCENT, alpha));
    rd2d_rect(r, x, y, t, L, ui_alpha(ACCENT, alpha));
    rd2d_rect(r, x + w - L, y + h - t, L, t, ui_alpha(ACCENT, alpha * 0.7f));
    rd2d_rect(r, x + w - t, y + h - L, t, L, ui_alpha(ACCENT, alpha * 0.7f));
}

void ui_bar(Game *g, float x, float y, float w, float h, float frac, uint32_t col, uint32_t bgcol, float alpha) {
    Renderer *r = g->rd;
    frac = saturatef(frac);
    rd2d_rect(r, x, y, w, h, ui_alpha(bgcol, alpha * 0.85f));
    // segmented fill
    int segs = (int)(w / 6.f);
    if (segs < 1) segs = 1;
    for (int i = 0; i < segs; i++) {
        float f0 = (float)i / (float)segs;
        if (f0 >= frac) break;
        float f1 = minf(frac, (float)(i + 1) / (float)segs);
        rd2d_rect(r, x + f0 * w, y, (f1 - f0) * w - 1.f, h, ui_alpha(col, alpha));
    }
    rd2d_rect_outline(r, x, y, w, h, 1.f, ui_alpha(0xFFFFFFFFu, alpha * 0.18f));
}

float ui_text_w(const Font *f, float sc, const char *s) { return font_text_width(f, sc, s); }

void ui_label(Game *g, const Font *f, float x, float y, float sc, const char *s, uint32_t c) {
    rd2d_text_sh(g->rd, f, x, y, sc, s, c);
}
void ui_label_c(Game *g, const Font *f, float cx, float y, float sc, const char *s, uint32_t c) {
    rd2d_text_center_sh(g->rd, f, cx, y, sc, s, c);
}
void ui_label_r(Game *g, const Font *f, float rx, float y, float sc, const char *s, uint32_t c) {
    rd2d_text_right(g->rd, f, rx, y, sc, s, c);
}

// tracked-out uppercase text for headings
void ui_heading(Game *g, const Font *f, float cx, float y, float sc, const char *s, uint32_t c) {
    char buf[128];
    int n = 0;
    for (int i = 0; s[i] && n < 120; i++) {
        buf[n++] = s[i];
        if (s[i] != ' ' && s[i + 1] != ' ' && s[i + 1] != 0) buf[n++] = ' ';
    }
    buf[n] = 0;
    ui_label_c(g, f, cx, y, sc, buf, c);
}

// ---------------------------------------------------------------- menu button
int ui_button(Game *g, float x, float y, float w, float h, const char *text, int selected, float alpha, int enabled) {
    Renderer *r = g->rd;
    Input *in = &g->plat->in;
    int hover = enabled && in->mouse_x > x && in->mouse_x < x + w && in->mouse_y > y && in->mouse_y < y + h;
    int hot = selected || hover;
    uint32_t bg = hot ? ui_alpha(0xFF2A2620u, alpha * 0.96f) : ui_alpha(0xFF17181Bu, alpha * 0.86f);
    if (!enabled) bg = ui_alpha(0xFF141416u, alpha * 0.7f);
    rd2d_rect(r, x, y, w, h, bg);
    // left accent bar slides with selection
    float barw = hot ? 4.f : 2.f;
    rd2d_rect(r, x, y, barw, h, ui_alpha(hot ? ACCENT : 0xFF4A4A4Eu, alpha));
    rd2d_rect_outline(r, x, y, w, h, 1.f, ui_alpha(hot ? ACCENT : 0xFF3A3A3Eu, alpha * 0.9f));
    if (hot) {
        rd2d_grad_h(r, x, y, w * 0.55f, h, ui_alpha(ACCENT, 0.10f * alpha), 0x00000000);
        // chevron
        float s = h * 0.22f;
        float cxx = x + w - 18.f * ui_scale(g);
        rd2d_tri(r, cxx - s, y + h * 0.5f - s, cxx + s, y + h * 0.5f, cxx - s, y + h * 0.5f + s, ui_alpha(ACCENT, alpha));
    }
    uint32_t tc = enabled ? (hot ? INK : MUTED) : ui_alpha(MUTED, 0.45f);
    // text is sized from the font's own line height so labels stay inside the button
    int lh = g->as->f_bold && g->as->f_bold->lineheight > 0 ? g->as->f_bold->lineheight : 41;
    float ts = (h * 0.40f) / (float)lh;
    float tw = ui_text_w(g->as->f_bold, ts, text);
    ui_label(g, g->as->f_bold, x + (w - tw) * 0.5f, y + h * 0.5f - (float)lh * ts * 0.34f, ts, text, tc);
    return hover;
}

// ---------------------------------------------------------------- main UI entry
void ui_draw(Game *g) {
    World *w = g->w;
    Renderer *r = g->rd;
    float W = (float)r->w, H = (float)r->h;
    float u = ui_scale(g);
    Input *in = &g->plat->in;
    Player *p = &w->players[w->local_player];
    (void)in;

    // ---- full-screen film grain / vignette over everything
    if (g->as->vignette) rd2d_image(r, g->as->vignette, 0, 0, W, H, 0xFFFFFFFFu);
    rd2d_rect(r, 0, 0, W, H, ui_alpha(0x00000000u, 0.0f));

    switch (w->state) {
        case GS_MAIN_MENU: {
            // dark gradient bed
            rd2d_grad_v(r, 0, 0, W, H, ui_alpha(0xFF07080Au, 0.92f), ui_alpha(0xFF0B0C10u, 0.55f));
            rd2d_grad_v(r, 0, 0, W * 0.62f, H, ui_alpha(0xFF05060Au, 0.85f), 0x00000000);
            // logo
            float lw = minf(W * 0.56f, 640.f * u);
            float ly = H * 0.02f;
            float lh = lw * (g->as->logo ? (float)g->as->logo->h / (float)g->as->logo->w : 0.5f);
            if (g->as->logo) rd2d_image(r, g->as->logo, (W - lw) * 0.5f, ly, lw, lh, 0xFFFFFFFFu);
            else ui_label_c(g, g->as->f_title, W * 0.5f, ly + lh * 0.3f, 1.2f, "THE LAST NIGHT", INK);
            // menu
            float bw = minf(W * 0.34f, 460.f * u);
            float bh = 42.f * u;
            float bx = W * 0.5f - bw * 0.5f;
            float by = maxf(ly + lh + 10.f * u, H * 0.47f);
            const void *dummy = NULL;
            (void)dummy;
            for (int i = 0; i < g->menu_count; i++) {
                int en = 1;
                if (i == 0) en = (g->night_cleared > 0 || w->kills > 0);
                float a = 1.f;
                if (ui_button(g, bx, by + (float)i * (bh + 9.f * u), bw, bh, g->menu[i].label, g->menu_sel == i, a, en)) {
                    if (in->mouse_pressed[0] && en) { g->menu_sel = i; }
                }
                // small side note
                if (g->menu_sel == i) {
                    const char *hint = "";
                    if (i == 0) hint = g->night_cleared > 0 ? "RESUME YOUR RUN" : "NO SAVE FOUND";
                    else if (i == 1) hint = "BEGIN AT NIGHT ONE";
                    else if (i == 2) hint = "LAN CO-OP, UP TO FOUR";
                    else if (i == 3) hint = "AUDIO, VIDEO, INPUT";
                    else if (i == 4) hint = "CONTROLS AND OBJECTIVES";
                    else hint = "LEAVE THE STORE";
                    ui_label(g, g->as->f_mono, bx + bw + 16.f * u, by + (float)i * (bh + 9.f * u) + bh * 0.5f - 6.f * u, 0.55f * u, hint, ui_alpha(0xFF9A8F70u, 0.95f));
                }
            }
            // footer
            char foot[192];
            snprintf(foot, sizeof foot, "KILLS %d      HEADSHOTS %d      NIGHTS CLEARED %d / 10", w->kills, w->headshots, g->night_cleared);
            ui_label(g, g->as->f_mono, 28.f * u, H - 46.f * u, 0.52f * u, foot, ui_alpha(0xFF84806Fu, 0.9f));
            ui_label_r(g, g->as->f_mono, W - 28.f * u, H - 46.f * u, 0.52f * u, "V" TLN_VERSION "   BUILT WITH THE TS ENGINE", ui_alpha(0xFF84806Fu, 0.7f));
            if (g->toast_t > 0.f)
                ui_label_c(g, g->as->f_bold, W * 0.5f, H - 96.f * u, 0.62f * u, g->toast, ui_alpha(DANGER, saturatef(g->toast_t)));
            break;
        }
        case GS_LOADING: {
            rd2d_rect(r, 0, 0, W, H, ui_alpha(0xFF05060Au, 1.f));
            float t = saturatef(g->load_t / 2.6f);
            // big night number
            char big[64];
            snprintf(big, sizeof big, "NIGHT %d", g->pending_night);
            ui_label_c(g, g->as->f_title, W * 0.5f, H * 0.30f, 1.35f * u, big, INK);
            const NightConfig *c = night_config(g->pending_night);
            ui_label_c(g, g->as->f_reg, W * 0.5f, H * 0.30f + 96.f * u, 0.85f * u, c->subtitle, ui_alpha(0xFFB5AFA5u, 0.95f));
            // loading bar
            float lw = minf(W * 0.5f, 560.f * u);
            ui_bar(g, W * 0.5f - lw * 0.5f, H * 0.62f, lw, 8.f * u, t, ACCENT, 0xFF1A1A1Eu, 1.f);
            ui_label_c(g, g->as->f_mono, W * 0.5f, H * 0.62f + 20.f * u, 0.55f * u,
                    t < 0.45f ? "SEALING THE LOADING DOCK" : (t < 0.8f ? "CHECKING THE AISLES" : "LOCKING THE DOORS"), MUTED);
            // tip
            if (w->tips_t <= 0.f) {
                w->tips_t = 5.f;
                snprintf(w->tip, sizeof w->tip, "%s", g_tip(g->pending_night));
            }
            ui_label_c(g, g->as->f_reg, W * 0.5f, H * 0.80f, 0.68f * u, w->tip, ui_alpha(0xFF97A0A8u, 0.9f));
            ui_label(g, g->as->f_mono, 28.f * u, H - 34.f * u, 0.5f * u, "PRESS ANY KEY TO SKIP", ui_alpha(0xFF6E6A62u, 0.8f));
            if (in->pressed[K_ESC] || in->pressed[K_ENTER] || in->pressed[K_SPACE]) g->load_t = 2.7f;
            break;
        }
        case GS_DAY:
        case GS_NIGHT:
        case GS_NIGHT_INTRO:
        case GS_NIGHT_END:
        case GS_PAUSE:
        case GS_SHOP:
        case GS_GAMEOVER:
        case GS_VICTORY: {
            draw_hud(g);
            if (w->state == GS_PAUSE) draw_pause(g);
            if (w->state == GS_SHOP) draw_shop(g);
            if (w->state == GS_GAMEOVER) draw_gameover(g);
            if (w->state == GS_VICTORY) draw_victory(g);
            break;
        }
        case GS_SETTINGS: draw_settings(g); break;
        case GS_CONTROLS: draw_controls(g); break;
        case GS_LOBBY: draw_lobby(g); break;
        default: break;
    }
    // global fade-in
    if (g->fade > 0.f) rd2d_rect(r, 0, 0, W, H, ui_alpha(BLACK, g->fade));
}
