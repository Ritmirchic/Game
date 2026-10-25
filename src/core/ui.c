/* ============================================================================
   SPACE MINER — core/ui.c
   Immediate-mode UI: widgets, main menu, HUD, pause, game over, boot, intro.
   ========================================================================== */
#include "state.h"
#include "ui_internal.h"
#include "audio.h"

Input *g_ui_in = NULL;
int g_ui_hover_id = -1;
int g_ui_active_slider = -1;
u32 g_ui_sfx_frame = 0;

/* ---------------------------------------------------------------- widgets -- */
int ui_hover(Game *g, Rect r) {
    if (!g_ui_in) return 0;
    return rect_contains(r, g_ui_in->mouse.x, g_ui_in->mouse.y);
}

int ui_clicked(Game *g, Rect r) {
    if (!g_ui_in) return 0;
    return rect_contains(r, g_ui_in->mouse.x, g_ui_in->mouse.y) && g_ui_in->mouse_pressed[0];
}

void ui_panel(Game *g, Rect r, u32 accent) {
    Gfx *gfx = g->gfx;
    gfx_fill(gfx, r, UI_PANEL);
    /* subtle top sheen */
    gfx_fill_vgrad(gfx, rect(r.x, r.y, r.w, SM_MIN(46.f, r.h * 0.25f)),
                   SM_RGBA(SM_GET_R(accent) / 4, SM_GET_G(accent) / 4, SM_GET_B(accent) / 4, 60),
                   SM_RGBA(0, 0, 0, 0));
    gfx_rect_outline(gfx, r, 1.f, UI_BORDER);
    /* corner brackets */
    f32 L = 16.f;
    u32 c = SM_RGBA(SM_GET_R(accent), SM_GET_G(accent), SM_GET_B(accent), 190);
    gfx_fill(gfx, rect(r.x, r.y, L, 2.f), c);
    gfx_fill(gfx, rect(r.x, r.y, 2.f, L), c);
    gfx_fill(gfx, rect(r.x + r.w - L, r.y, L, 2.f), c);
    gfx_fill(gfx, rect(r.x + r.w - 2.f, r.y, 2.f, L), c);
    gfx_fill(gfx, rect(r.x, r.y + r.h - 2.f, L, 2.f), c);
    gfx_fill(gfx, rect(r.x, r.y + r.h - L, 2.f, L), c);
    gfx_fill(gfx, rect(r.x + r.w - L, r.y + r.h - 2.f, L, 2.f), c);
    gfx_fill(gfx, rect(r.x + r.w - 2.f, r.y + r.h - L, 2.f, L), c);
}

void ui_panel_soft(Game *g, Rect r, u32 accent) {
    Gfx *gfx = g->gfx;
    gfx_fill(gfx, r, SM_RGBA(12, 18, 28, 190));
    gfx_rect_outline(gfx, r, 1.f, SM_RGBA(SM_GET_R(accent), SM_GET_G(accent), SM_GET_B(accent), 70));
}

void ui_header(Game *g, Rect r, const char *title, const char *sub, u32 accent) {
    Gfx *gfx = g->gfx;
    gfx_fill(gfx, rect(r.x, r.y, r.w, 1.f), SM_RGBA(SM_GET_R(accent), SM_GET_G(accent), SM_GET_B(accent), 120));
    gfx_text(gfx, &g->assets.font_head, v2(r.x, r.y + 10.f), title, UI_INK, 0);
    if (sub) gfx_text(gfx, &g->assets.font_tiny, v2(r.x + r.w, r.y + 16.f), sub, UI_DIM, 2);
}

void ui_divider(Game *g, Rect r, u32 color) { gfx_fill(g->gfx, r, color); }

void ui_bar(Game *g, Rect r, f32 frac, u32 fill, u32 back, int segments) {
    Gfx *gfx = g->gfx;
    frac = sm_saturate(frac);
    gfx_fill(gfx, r, back);
    f32 w = r.w * frac;
    if (w > 0.5f) {
        gfx_fill(gfx, rect(r.x, r.y, w, r.h), fill);
        gfx_fill(gfx, rect(r.x, r.y, w, SM_MAX(1.f, r.h * 0.35f)), sm_col_scale(fill, 1.25f));
    }
    if (segments > 1) {
        for (int i = 1; i < segments; i++) {
            f32 x = r.x + r.w * (f32)i / (f32)segments;
            gfx_fill(gfx, rect(x - 1.f, r.y, 2.f, r.h), SM_RGBA(6, 10, 16, 200));
        }
    }
    gfx_rect_outline(gfx, r, 1.f, SM_RGBA(0, 0, 0, 140));
}

static void ui_button_frame(Game *g, Rect r, BtnState state, u32 accent) {
    Gfx *gfx = g->gfx;
    u32 top = SM_RGBA(18, 26, 38, 230), bot = SM_RGBA(10, 15, 23, 240);
    u32 edge = SM_RGBA(SM_GET_R(accent) / 2, SM_GET_G(accent) / 2, SM_GET_B(accent) / 2, 110);
    if (state == BTN_HOVER) {
        top = SM_RGBA(SM_GET_R(accent) / 3, SM_GET_G(accent) / 3, SM_GET_B(accent) / 3, 245);
        bot = SM_RGBA(16, 26, 36, 250);
        edge = SM_RGBA(SM_GET_R(accent), SM_GET_G(accent), SM_GET_B(accent), 220);
    } else if (state == BTN_SELECTED || state == BTN_TOGGLE_ON) {
        top = SM_RGBA(SM_GET_R(accent) / 2, SM_GET_G(accent) / 2, SM_GET_B(accent) / 2, 240);
        bot = SM_RGBA(12, 20, 30, 245);
        edge = SM_RGBA(SM_GET_R(accent), SM_GET_G(accent), SM_GET_B(accent), 200);
    } else if (state == BTN_DISABLED) {
        top = SM_RGBA(14, 18, 24, 200);
        bot = SM_RGBA(10, 13, 18, 200);
        edge = SM_RGBA(60, 70, 82, 90);
    }
    gfx_fill_vgrad(gfx, r, top, bot);
    gfx_rect_outline(gfx, r, 1.f, edge);
    if (state == BTN_HOVER) {
        gfx_fill(gfx, rect(r.x, r.y, 3.f, r.h), SM_RGBA(SM_GET_R(accent), SM_GET_G(accent), SM_GET_B(accent), 255));
        gfx_glow(gfx, v2(r.x + 2.f, r.y + r.h * 0.5f), r.h * 0.9f, accent, 0.35f);
    } else if (state == BTN_SELECTED || state == BTN_TOGGLE_ON) {
        gfx_fill(gfx, rect(r.x, r.y, 3.f, r.h), SM_RGBA(SM_GET_R(accent), SM_GET_G(accent), SM_GET_B(accent), 220));
    }
}

void ui_button_draw(Game *g, Rect r, const char *label, const char *sublabel, BtnState state, u32 accent) {
    Gfx *gfx = g->gfx;
    ui_button_frame(g, r, state, accent);
    u32 col = (state == BTN_DISABLED) ? UI_DIMMER : UI_INK;
    if (state == BTN_HOVER) col = SM_RGBA(255, 255, 255, 255);
    f32 ty = sublabel ? r.y + r.h * 0.5f - 12.f : r.y + r.h * 0.5f - 9.f;
    gfx_text_spaced(gfx, &g->assets.font_head, v2(r.x + 18.f, ty), label, col, 0, 1.6f);
    if (sublabel) gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 18.f, ty + 22.f),
                           sublabel, state == BTN_DISABLED ? UI_DIMMER : UI_DIM, 0);
}

int ui_button(Game *g, Rect r, const char *label, const char *sublabel, int enabled, u32 accent) {
    int hover = ui_hover(g, r);
    BtnState st = !enabled ? BTN_DISABLED : (hover ? BTN_HOVER : BTN_NORMAL);
    ui_button_draw(g, r, label, sublabel, st, accent);
    if (hover && enabled && g_ui_in) {
        if (g_ui_sfx_frame != g->frame) {
            g_ui_sfx_frame = g->frame;
            snd_play(g, "sfx_hover", 0.16f, 0.f, 1.f);
        }
        if (g_ui_in->mouse_pressed[0]) {
            snd_play(g, "sfx_click", 0.45f, 0.f, 1.05f);
            return 1;
        }
    }
    return 0;
}

int ui_icon_button(Game *g, Rect r, const Sheet *icon, const char *label, int enabled, u32 accent) {
    int hover = ui_hover(g, r);
    BtnState st = !enabled ? BTN_DISABLED : (hover ? BTN_HOVER : BTN_NORMAL);
    ui_button_frame(g, r, st, accent);
    f32 ih = r.h * 0.52f;
    if (icon && icon->px)
        gfx_blit(g->gfx, icon, 0, rect(r.x + 12.f, r.y + (r.h - ih) * 0.5f, ih, ih),
                 enabled ? UI_INK : UI_DIMMER, 0.9f, BLEND_ALPHA);
    gfx_text(g->gfx, &g->assets.font_small, v2(r.x + 12.f + ih + 10.f, r.y + r.h * 0.5f - 8.f),
             label, enabled ? UI_INK : UI_DIMMER, 0);
    if (hover && enabled && g_ui_in && g_ui_in->mouse_pressed[0]) {
        snd_play(g, "sfx_click", 0.45f, 0.f, 1.05f);
        return 1;
    }
    return 0;
}

int ui_slider(Game *g, Rect r, f32 *value, const char *label, u32 accent) {
    Gfx *gfx = g->gfx;
    ui_label_small(g, v2(r.x, r.y - 16.f), label, UI_DIM, 0);
    Rect track = rect(r.x, r.y + r.h * 0.5f - 3.f, r.w - 54.f, 6.f);
    gfx_fill(gfx, track, SM_RGBA(16, 24, 34, 240));
    gfx_fill(gfx, rect(track.x, track.y, track.w * sm_saturate(*value), track.h), accent);
    f32 knobX = track.x + track.w * sm_saturate(*value);
    int hover = ui_hover(g, rect(track.x, r.y, track.w, r.h));
    gfx_circle(gfx, v2(knobX, track.y + track.h * 0.5f), hover ? 8.f : 6.f, 0xFFEFF7FF);
    gfx_glow(gfx, v2(knobX, track.y + track.h * 0.5f), 16.f, accent, 0.5f);
    char buf[16];
    sm_sprintf(buf, sizeof(buf), "%d%%", (int)(sm_saturate(*value) * 100.f + 0.5f));
    gfx_text(gfx, &g->assets.font_small, v2(r.x + r.w, r.y + r.h * 0.5f - 8.f), buf, UI_INK, 2);
    if (!g_ui_in) return 0;
    int changed = 0;
    if (hover && g_ui_in->mouse_pressed[0]) g_ui_active_slider = (int)r.y;
    if (g_ui_active_slider == (int)r.y) {
        if (g_ui_in->mouse_down[0]) {
            f32 v = sm_saturate((g_ui_in->mouse.x - track.x) / SM_MAX(1.f, track.w));
            if (fabsf(v - *value) > 0.001f) { *value = v; changed = 1; }
        } else {
            g_ui_active_slider = -1;
        }
    }
    return changed;
}

int ui_toggle(Game *g, Rect r, const char *label, int *value, u32 accent) {
    Gfx *gfx = g->gfx;
    int hover = ui_hover(g, r);
    ui_label_small(g, v2(r.x, r.y + r.h * 0.5f - 8.f), label, hover ? UI_INK : UI_DIM, 0);
    Rect sw = rect(r.x + r.w - 46.f, r.y + r.h * 0.5f - 11.f, 46.f, 22.f);
    gfx_fill(gfx, sw, *value ? SM_RGBA(SM_GET_R(accent) / 2, SM_GET_G(accent) / 2, SM_GET_B(accent) / 2, 220)
                            : SM_RGBA(22, 28, 38, 230));
    gfx_rect_outline(gfx, sw, 1.f, *value ? accent : SM_RGBA(70, 84, 100, 150));
    f32 kx = *value ? sw.x + sw.w - 12.f : sw.x + 12.f;
    gfx_circle(gfx, v2(kx, sw.y + sw.h * 0.5f), 8.f, *value ? 0xFFEFF7FF : UI_DIM);
    if (hover && g_ui_in && g_ui_in->mouse_pressed[0]) {
        *value = !*value;
        snd_play(g, "sfx_click", 0.4f, 0.f, *value ? 1.16f : 0.9f);
        return 1;
    }
    return 0;
}

void ui_scroll_capture(Game *g, Rect area, int *scroll, int content_h) {
    if (!g_ui_in) return;
    if (!rect_contains(area, g_ui_in->mouse.x, g_ui_in->mouse.y)) return;
    if (fabsf(g_ui_in->wheel) > 0.01f) {
        *scroll -= (int)(g_ui_in->wheel * 64.f);
    }
    int maxscroll = SM_MAX(0, content_h - (int)area.h);
    *scroll = SM_CLAMP(*scroll, 0, maxscroll);
}

void ui_label(Game *g, Vec2 pos, const char *text, u32 color, int align) {
    gfx_text(g->gfx, &g->assets.font_body, pos, text, color, align);
}
void ui_label_small(Game *g, Vec2 pos, const char *text, u32 color, int align) {
    gfx_text(g->gfx, &g->assets.font_small, pos, text, color, align);
}
void ui_label_head(Game *g, Vec2 pos, const char *text, u32 color, int align) {
    gfx_text_spaced(g->gfx, &g->assets.font_head, pos, text, color, align, 1.4f);
}

void ui_money(Game *g, Vec2 pos, s64 credits, int align) {
    char buf[48];
    char num[32];
    sm_format_num(num, sizeof(num), credits);
    sm_sprintf(buf, sizeof(buf), "%s CR", num);
    gfx_text(g->gfx, &g->assets.font_head, pos, buf, UI_GOLD, align);
}

void ui_progress_ring(Game *g, Vec2 c, f32 r, f32 frac, u32 color, f32 thickness) {
    gfx_ring(g->gfx, c, r, thickness, SM_RGBA(20, 28, 38, 220));
    f32 a0 = -SM_PI * 0.5f;
    gfx_arc(g->gfx, c, r, a0, a0 + SM_TAU * sm_saturate(frac), thickness, color);
}

void ui_tab_bar(Game *g, Rect r, const char **labels, int count, int *selected, u32 accent) {
    f32 w = r.w / (f32)count;
    for (int i = 0; i < count; i++) {
        Rect tr = rect(r.x + w * (f32)i + 2.f, r.y, w - 4.f, r.h);
        int hover = ui_hover(g, tr);
        BtnState st = (*selected == i) ? BTN_SELECTED : (hover ? BTN_HOVER : BTN_NORMAL);
        ui_button_frame(g, tr, st, accent);
        gfx_text(g->gfx, &g->assets.font_small, v2(tr.x + tr.w * 0.5f, tr.y + tr.h * 0.5f - 8.f),
                 labels[i], (*selected == i) ? UI_INK : (hover ? UI_INK : UI_DIM), 1);
        if (hover && g_ui_in && g_ui_in->mouse_pressed[0]) {
            if (*selected != i) snd_play(g, "sfx_click", 0.4f, 0.f, 1.2f);
            *selected = i;
        }
    }
}

void ui_toasts(Game *g) {
    Gfx *gfx = g->gfx;
    if (g->toast_timer > 0.f) {
        f32 a = sm_saturate(g->toast_timer / 0.4f) * sm_saturate((2.6f - g->toast_timer) / 0.25f);
        char buf[160];
        sm_strlcpy(buf, g->toast, sizeof(buf));
        f32 w = (f32)gfx_text_width(&g->assets.font_head, buf) + 46.f;
        Rect r = rect(gfx->w * 0.5f - w * 0.5f, gfx->h * 0.186f, w, 34.f);
        gfx_fill(gfx, r, SM_RGBA(8, 14, 22, (u32)(200 * a)));
        gfx_rect_outline(gfx, r, 1.f, SM_RGBA(SM_GET_R(g->toast_color), SM_GET_G(g->toast_color), SM_GET_B(g->toast_color), (u32)(150 * a)));
        gfx_text(gfx, &g->assets.font_head, v2(r.x + w * 0.5f, r.y + 8.f),
                 buf, sm_col_scale(g->toast_color, a), 1);
    }
    if (g->toast2_timer > 0.f) {
        f32 a = sm_saturate(g->toast2_timer / 0.5f) * sm_saturate((3.2f - g->toast2_timer) / 0.3f);
        gfx_text(gfx, &g->assets.font_small, v2(gfx->w * 0.5f, gfx->h * 0.186f + 44.f),
                 g->toast2, SM_RGBA(190, 210, 230, (u32)(220 * a)), 1);
    }
}

/* ----------------------------------------------------------- menu backdrop -- */
void ui_draw_bg_menu(Game *g, f32 t) {
    Gfx *gfx = g->gfx;
    const SectorDef *s = &k_sectors[g->sector];
    gfx_fill_vgrad(gfx, rect(0, 0, (f32)gfx->w, (f32)gfx->h), SM_RGB(7, 11, 20), SM_RGB(2, 4, 9));
    Sheet *neb = &g->assets.bg_far[1];
    if (neb->px) {
        f32 scale = 3.6f;
        f32 tw = (f32)neb->w * scale, th = (f32)neb->h * scale;
        f32 ox = fmodf(-t * 12.f, tw), oy = fmodf(-t * 7.f, th);
        if (ox > 0) ox -= tw;
        if (oy > 0) oy -= th;
        for (f32 y = oy; y < (f32)gfx->h; y += th)
            for (f32 x = ox; x < (f32)gfx->w; x += tw)
                gfx_blit(gfx, neb, 0, rect(x, y, tw + 1, th + 1), 0xFFFFFFFF, 0.5f, BLEND_ADD);
    }
    /* slowly drifting stars */
    for (int i = 0; i < g->star_count; i++) {
        f32 par = 0.2f + g->star_size[i] * 0.06f;
        Vec2 sp = v2(g->stars[i].x - t * 6.f * par, g->stars[i].y - t * 3.f * par);
        sp.x = sm_wrapf(sp.x, (f32)gfx->w + 40.f) - 20.f;
        sp.y = sm_wrapf(sp.y, (f32)gfx->h + 40.f) - 20.f;
        f32 tw2 = 0.6f + 0.4f * sinf(t * (0.6f + g->star_twinkle[i]) + g->star_twinkle[i] * 9.f);
        gfx_fill(gfx, rect(sp.x, sp.y, g->star_size[i], g->star_size[i]), sm_col_scale(g->star_color[i], tw2));
        if (g->star_size[i] > 2.2f) gfx_glow(gfx, sp, g->star_size[i] * 7.f, g->star_color[i], 0.3f * tw2);
    }
    /* a big planet on the right edge */
    Sheet *planet = &g->assets.planet[1];
    if (planet->px) {
        f32 pw = (f32)planet->w * 2.6f;
        f32 cx = (f32)gfx->w * 0.80f + sinf(t * 0.1f) * 18.f;
        f32 cy = (f32)gfx->h * 0.42f;
        gfx_blit(gfx, planet, 0, rect(cx - pw * 0.5f, cy - pw * 0.5f, pw, pw), 0xFFFFFFFF, 0.9f, BLEND_ALPHA);
        gfx_glow(gfx, v2(cx, cy), pw * 0.62f, s->accent, 0.12f);
    }
    /* drifting asteroids for depth */
    for (int i = 0; i < 5; i++) {
        f32 seed = (f32)i * 1.7f;
        f32 x = fmodf(t * (10.f + seed * 4.f) + seed * 520.f, (f32)gfx->w + 400.f) - 200.f;
        f32 y = 120.f + seed * 128.f + sinf(t * 0.3f + seed) * 30.f;
        Sheet *ast = &g->assets.ast[i % 8];
        if (!ast->px) continue;
        f32 sz = 90.f + seed * 26.f;
        int fr = (int)((t * 6.f + seed * 5.f)) % ast->frames;
        gfx_blit(gfx, ast, fr, rect(x - sz * 0.5f, y - sz * 0.5f, sz, sz), 0xFFFFFFFF, 0.42f, BLEND_ALPHA);
    }
    /* vignette + scanline grid */
    gfx_vignette(gfx, 0.6f);
    for (f32 x = 0; x < (f32)gfx->w; x += 64.f)
        gfx_fill(gfx, rect(x, 0, 1.f, (f32)gfx->h), SM_RGBA(70, 130, 160, 12));
    for (f32 y = 0; y < (f32)gfx->h; y += 64.f)
        gfx_fill(gfx, rect(0, y, (f32)gfx->w, 1.f), SM_RGBA(70, 130, 160, 12));
}

/* ------------------------------------------------------------------- boot -- */
void ui_render_boot(Game *g, Input *in) {
    (void)in;
    Gfx *gfx = g->gfx;
    gfx_clear(gfx, SM_RGB(4, 7, 12));
    f32 t = g->screen_time;
    f32 pulse = 0.5f + 0.5f * sinf(t * 3.f);
    gfx_glow(gfx, v2(gfx->w * 0.5f, gfx->h * 0.5f), 420.f, SM_RGB(40, 90, 130), 0.30f + 0.1f * pulse);
    gfx_text_spaced(gfx, &g->assets.font_head, v2(gfx->w * 0.5f, gfx->h * 0.44f), "SPACE MINER", UI_INK, 1, 8.f);
    gfx_text(gfx, &g->assets.font_tiny, v2(gfx->w * 0.5f, gfx->h * 0.44f + 42.f),
             "DEEP FIELD EXTRACTION SYSTEMS", UI_DIM, 1);
    Rect bar = rect(gfx->w * 0.5f - 220.f, gfx->h * 0.56f, 440.f, 6.f);
    gfx_fill(gfx, bar, SM_RGBA(14, 22, 32, 255));
    f32 frac = sm_saturate(t / 2.0f);
    gfx_fill(gfx, rect(bar.x, bar.y, bar.w * frac, bar.h), UI_CYAN);
    gfx_glow(gfx, v2(bar.x + bar.w * frac, bar.y + 3.f), 40.f, UI_CYAN, 0.6f);
    static const char *lines[] = {
        "initialising reactor coupling ...",
        "calibrating mining emitters ...",
        "loading sector telemetry ...",
        "drone uplink established ...",
        "hangar doors opening ..."
    };
    int idx = SM_MIN(4, (int)(t / 0.42f));
    gfx_text(gfx, &g->assets.font_small, v2(gfx->w * 0.5f, gfx->h * 0.60f), lines[idx], UI_DIM, 1);
    gfx_text(gfx, &g->assets.font_tiny, v2(gfx->w * 0.5f, (f32)gfx->h - 40.f),
             "ARENA INTERACTIVE  //  BUILD 1.0.0", UI_DIMMER, 1);
}

/* ------------------------------------------------------------------- menu -- */
typedef struct { const char *label; const char *sub; int id; } MenuItem;

void ui_render_menu(Game *g, Input *in) {
    (void)in;
    Gfx *gfx = g->gfx;
    ui_draw_bg_menu(g, g->time);

    /* logo */
    Sheet *logo = &g->assets.title_logo;
    f32 lt = g->screen_time;
    f32 logo_scale = 0.92f + 0.08f * sm_easeout(SM_MIN(1.f, lt * 1.4f));
    if (logo->px) {
        f32 lw = 640.f * logo_scale, lh = lw * (f32)logo->h / (f32)logo->w;
        f32 lx = gfx->w * 0.5f - lw * 0.5f;
        f32 ly = 54.f;
        gfx_glow(gfx, v2(gfx->w * 0.5f, ly + lh * 0.5f), lw * 0.55f, SM_RGB(60, 130, 190), 0.35f);
        gfx_blit(gfx, logo, 0, rect(lx, ly, lw, lh), 0xFFFFFFFF, sm_saturate(lt * 1.5f), BLEND_ALPHA);
    }

    /* primary buttons */
    int has_save = g->save.level > 1 || g->save.credits > 0 || g->save.stat_asteroids_mined > 0;
    MenuItem items[6];
    int n = 0;
    char cont_sub[96];
    if (has_save)
        sm_sprintf(cont_sub, sizeof(cont_sub), "Level %d  //  %s", g->save.level, sector_name(g->save.sector_current));
    else
        sm_strlcpy(cont_sub, "No flight record found", sizeof(cont_sub));
    items[n].label = "CONTINUE"; items[n].sub = cont_sub; items[n].id = 0; n++;
    items[n].label = "NEW FLIGHT"; items[n].sub = "Start over from the Starter Belt"; items[n].id = 1; n++;
    items[n].label = "SETTINGS"; items[n].sub = "Audio, video, difficulty"; items[n].id = 2; n++;
    items[n].label = "CREDITS"; items[n].sub = "Who built this thing"; items[n].id = 3; n++;
    items[n].label = "QUIT"; items[n].sub = "Return to deep space"; items[n].id = 4; n++;

    f32 bw = 372.f, bh = 60.f, gap = 12.f;
    f32 bx = gfx->w * 0.5f - bw * 0.5f;
    f32 by = gfx->h * 0.40f;
    for (int i = 0; i < n; i++) {
        Rect r = rect(bx, by + (f32)i * (bh + gap), bw, bh);
        int enabled = !(items[i].id == 0 && !has_save);
        if (ui_button(g, r, items[i].label, items[i].sub, enabled, UI_CYAN)) {
            switch (items[i].id) {
                case 0:
                    g->sector = SM_CLAMP(g->save.sector_current, 0, SECTOR_COUNT - 1);
                    player_reset(g, 1);
                    world_enter_sector(g, g->sector);
                    game_set_screen(g, SCREEN_PLAY);
                    break;
                case 1: g->confirm_action = 1; break;
                case 2: g->menu_sel = 10; break;
                case 3: game_set_screen(g, SCREEN_CREDITS); g->menu_sel = 20; break;
                case 4: g->menu_sel = 99; break;
                default: break;
            }
        }
    }

    /* settings sub-panel */
    if (g->menu_sel == 10) {
        Rect p = rect(gfx->w * 0.5f - 330.f, gfx->h * 0.30f, 660.f, 372.f);
        ui_panel(g, p, UI_CYAN);
        ui_header(g, rect(p.x + 24.f, p.y + 10.f, p.w - 48.f, 40.f), "SETTINGS", "audio / video / gameplay", UI_CYAN);
        f32 x = p.x + 30.f, y = p.y + 76.f, w = p.w - 60.f;
        if (ui_slider(g, rect(x, y, w, 22.f), &g->save.audio.master, "MASTER VOLUME", UI_CYAN)) audio_set_gain(g->save.audio.master, g->save.audio.music, g->save.audio.sfx);
        y += 46.f;
        if (ui_slider(g, rect(x, y, w, 22.f), &g->save.audio.music, "MUSIC", UI_CYAN)) audio_set_gain(g->save.audio.master, g->save.audio.music, g->save.audio.sfx);
        y += 46.f;
        if (ui_slider(g, rect(x, y, w, 22.f), &g->save.audio.sfx, "EFFECTS", UI_CYAN)) audio_set_gain(g->save.audio.master, g->save.audio.music, g->save.audio.sfx);
        y += 48.f;
        ui_toggle(g, rect(x, y, w, 26.f), "BLOOM LIGHTING", &g->save.video.bloom, UI_CYAN); y += 34.f;
        ui_toggle(g, rect(x, y, w, 26.f), "SCREEN SHAKE", &g->save.video.shake, UI_CYAN); y += 34.f;
        ui_toggle(g, rect(x, y, w, 26.f), "CRT SCANLINES", &g->save.video.scanlines, UI_CYAN); y += 34.f;
        ui_toggle(g, rect(x, y, w, 26.f), "SHOW CONTROL HINTS", &g->save.gameplay.show_hints, UI_CYAN); y += 40.f;
        /* difficulty */
        const char *diff_names[] = { "RELAXED", "STANDARD", "HARDCORE" };
        ui_label_small(g, v2(x, y - 2.f), "DIFFICULTY", UI_DIM, 0);
        for (int i = 0; i < 3; i++) {
            Rect dr = rect(x + w - 300.f + (f32)i * 100.f, y - 8.f, 94.f, 28.f);
            int hover = ui_hover(g, dr);
            ui_button_draw(g, dr, diff_names[i], NULL,
                           g->save.gameplay.difficulty == i ? BTN_SELECTED : (hover ? BTN_HOVER : BTN_NORMAL), UI_CYAN);
            if (hover && g_ui_in && g_ui_in->mouse_pressed[0]) {
                g->save.gameplay.difficulty = i;
                snd_play(g, "sfx_click", 0.4f, 0.f, 1.1f);
            }
        }
        if (ui_button(g, rect(p.x + p.w * 0.5f - 90.f, p.y + p.h - 56.f, 180.f, 40.f), "BACK", NULL, 1, UI_CYAN)) {
            g->menu_sel = 0;
            if (g->plat->write_file) save_write(g);
        }
    }

    /* new-flight confirmation */
    if (g->confirm_action == 1) {
        Rect p = rect(gfx->w * 0.5f - 280.f, gfx->h * 0.36f, 560.f, 210.f);
        ui_panel(g, p, UI_RED);
        ui_header(g, rect(p.x + 24.f, p.y + 10.f, p.w - 48.f, 40.f), "START A NEW FLIGHT?", "this erases the current save", UI_RED);
        ui_label(g, v2(p.x + p.w * 0.5f, p.y + 78.f), "Progress, ships, drones and credits will be lost.", UI_DIM, 1);
        if (ui_button(g, rect(p.x + 30.f, p.y + 130.f, 240.f, 48.f), "ERASE & START", NULL, 1, UI_RED)) {
            g->confirm_action = 0;
            game_new_session(g);
        }
        if (ui_button(g, rect(p.x + p.w - 270.f, p.y + 130.f, 240.f, 48.f), "CANCEL", NULL, 1, UI_CYAN)) {
            g->confirm_action = 0;
        }
    }

    /* footer */
    gfx_text(gfx, &g->assets.font_tiny, v2(gfx->w * 0.5f, (f32)gfx->h - 56.f),
             "MINE  -  SELL  -  UPGRADE  -  EXPLORE FIVE SECTORS", UI_DIMMER, 1);
    char ver[96];
    sm_sprintf(ver, sizeof(ver), "SPACE MINER v%s  //  procedural art + audio, zero external assets", SM_VERSION_STR);
    gfx_text(gfx, &g->assets.font_tiny, v2(gfx->w * 0.5f, (f32)gfx->h - 34.f), ver, SM_RGB(70, 92, 108), 1);

    if (g->menu_sel == 20) {
        /* credits overlay */
        Rect p = rect(gfx->w * 0.5f - 330.f, gfx->h * 0.26f, 660.f, 350.f);
        ui_panel(g, p, UI_GOLD);
        ui_header(g, rect(p.x + 24.f, p.y + 12.f, p.w - 48.f, 40.f), "CREDITS", "made with code and care", UI_GOLD);
        const char *lines[] = {
            "SPACE MINER  -  Deep Field Extraction",
            "",
            "Engine, renderer, mixer, UI: hand-written C99",
            "All art procedurally generated (numpy + PIL)",
            "All music & effects synthesised from scratch",
            "",
            "Typefaces: Rajdhani and Orbitron (SIL Open Font License)",
            "Fonts baked to bitmap atlases, no font files ship with the game",
            "",
            "Built for Windows x64. No external runtime required."
        };
        for (int i = 0; i < (int)SM_ARRAY_COUNT(lines); i++)
            gfx_text(gfx, &g->assets.font_small, v2(p.x + p.w * 0.5f, p.y + 66.f + (f32)i * 22.f),
                     lines[i], i == 0 ? UI_INK : UI_DIM, 1);
        if (ui_button(g, rect(p.x + p.w * 0.5f - 90.f, p.y + p.h - 56.f, 180.f, 40.f), "BACK", NULL, 1, UI_GOLD)) {
            g->menu_sel = 0;
            game_set_screen(g, SCREEN_MENU);
        }
    }
    ui_toasts(g);
}

/* ------------------------------------------------------------------ intro -- */
void ui_render_intro(Game *g, Input *in) {
    (void)in;
    Gfx *gfx = g->gfx;
    ui_draw_bg_menu(g, g->time);
    Rect p = rect(gfx->w * 0.5f - 420.f, gfx->h * 0.22f, 840.f, 360.f);
    ui_panel(g, p, UI_CYAN);
    ui_header(g, rect(p.x + 28.f, p.y + 14.f, p.w - 56.f, 40.f), "FLIGHT BRIEFING", "sector 01  //  starter belt", UI_CYAN);
    const char *lines[] = {
        "You have a cutter, a hold full of nothing, and a debt.",
        "",
        "The belt is thick with iron and copper. Cut the rocks with your",
        "mining beam, tractor in the ore, and sell it at the refinery.",
        "",
        "Every credit buys a better hull, a hotter laser, or another drone",
        "to work the field while you sleep. Beyond the belt lie crystal",
        "clouds, graveyards, plasma storms - and the silent edge.",
        "",
        "Fly safe. Nobody else will."
    };
    for (int i = 0; i < (int)SM_ARRAY_COUNT(lines); i++)
        gfx_text(gfx, &g->assets.font_body, v2(p.x + p.w * 0.5f, p.y + 70.f + (f32)i * 25.f),
                 lines[i], i == 0 || i == (int)SM_ARRAY_COUNT(lines) - 1 ? UI_INK : UI_DIM, 1);
    if (ui_button(g, rect(p.x + p.w * 0.5f - 120.f, p.y + p.h - 62.f, 240.f, 46.f), "BEGIN FLIGHT", NULL, 1, UI_CYAN))
        game_new_session(g);
    ui_toasts(g);
}

/* -------------------------------------------------------------------- HUD -- */
static void hud_bars(Game *g) {
    Gfx *gfx = g->gfx;
    Player *p = &g->player;
    f32 x = 26.f, y = 24.f;
    Rect panel = rect(x - 10.f, y - 12.f, 320.f, 132.f);
    ui_panel_soft(g, panel, UI_CYAN);
    /* ship name */
    gfx_text_spaced(gfx, &g->assets.font_head, v2(x, y - 2.f), k_ships[g->save.ship].name, UI_INK, 0, 1.6f);
    gfx_text(gfx, &g->assets.font_tiny, v2(x + 300.f, y + 2.f), k_ships[g->save.ship].role, UI_DIM, 2);

    struct { const char *label; f32 frac; u32 col; Sheet *icon; const char *value; } rows[3];
    char vb[3][24];
    sm_sprintf(vb[0], sizeof(vb[0]), "%d / %d", (int)ceilf(p->hull), (int)p->hull_max);
    sm_sprintf(vb[1], sizeof(vb[1]), "%d / %d", (int)ceilf(p->shield), (int)p->shield_max);
    sm_sprintf(vb[2], sizeof(vb[2]), "%d / %d", (int)ceilf(p->energy), (int)p->energy_max);
    rows[0].label = "HULL"; rows[0].frac = p->hull / SM_MAX(1.f, p->hull_max);
    rows[0].col = sm_col_lerp(UI_RED, UI_GREEN, sm_saturate(p->hull / SM_MAX(1.f, p->hull_max)));
    rows[0].icon = &g->assets.ui_ship; rows[0].value = vb[0];
    rows[1].label = "SHIELD"; rows[1].frac = p->shield / SM_MAX(1.f, p->shield_max);
    rows[1].col = SM_RGB(110, 200, 255); rows[1].icon = &g->assets.ui_shield; rows[1].value = vb[1];
    rows[2].label = "ENERGY"; rows[2].frac = p->energy / SM_MAX(1.f, p->energy_max);
    rows[2].col = SM_RGB(150, 240, 170); rows[2].icon = &g->assets.ui_energy; rows[2].value = vb[2];

    for (int i = 0; i < 3; i++) {
        f32 ry = y + 26.f + (f32)i * 30.f;
        gfx_blit(gfx, rows[i].icon, 0, rect(x, ry, 18.f, 18.f), rows[i].col, 0.95f, BLEND_ALPHA);
        gfx_text(gfx, &g->assets.font_tiny, v2(x + 24.f, ry + 2.f), rows[i].label, UI_DIM, 0);
        ui_bar(g, rect(x + 82.f, ry + 3.f, 168.f, 11.f), rows[i].frac, rows[i].col, SM_RGBA(10, 16, 24, 220),
               i == 1 ? 4 : 1);
        gfx_text(gfx, &g->assets.font_tiny, v2(x + 300.f, ry + 2.f), rows[i].value, UI_INK, 2);
    }
    /* mining heat / beam indicator */
    if (p->mining) {
        gfx_text(gfx, &g->assets.font_tiny, v2(x, y + 122.f), "MINING BEAM ACTIVE", UI_CYAN, 0);
        f32 pulse = 0.5f + 0.5f * sinf(g->time * 12.f);
        gfx_glow(gfx, v2(x + 168.f, y + 126.f), 26.f, UI_CYAN, 0.4f * pulse);
    }
}

static void hud_topbar(Game *g) {
    Gfx *gfx = g->gfx;
    const SectorDef *s = &k_sectors[g->sector];
    /* sector name, centre top */
    gfx_text_spaced(gfx, &g->assets.font_head, v2(gfx->w * 0.5f, 20.f), s->name, UI_INK, 1, 3.f);
    for (int i = 0; i < 5; i++) {
        f32 dx = gfx->w * 0.5f - 34.f + (f32)i * 18.f;
        u32 c = (i < s->danger) ? SM_RGBA(SM_GET_R(UI_RED), SM_GET_G(UI_RED), SM_GET_B(UI_RED), 230)
                                : SM_RGBA(80, 96, 112, 140);
        gfx_fill(gfx, rect(dx, 46.f, 12.f, 4.f), c);
    }
    gfx_text(gfx, &g->assets.font_tiny, v2(gfx->w * 0.5f, 54.f), s->subtitle, UI_DIMMER, 1);

    /* credits + level, right */
    char buf[64];
    ui_money(g, v2((f32)gfx->w - 26.f, 20.f), g->save.credits, 2);
    sm_sprintf(buf, sizeof(buf), "MINING LEVEL %d", g->save.level);
    gfx_text(gfx, &g->assets.font_tiny, v2((f32)gfx->w - 26.f, 46.f), buf, UI_DIM, 2);
    int need = level_xp_required(g->save.level);
    ui_bar(g, rect((f32)gfx->w - 190.f, 64.f, 164.f, 5.f), (f32)g->save.xp / (f32)SM_MAX(1, need),
           UI_CYAN, SM_RGBA(12, 18, 26, 220), 1);
}

static void hud_cargo(Game *g) {
    Gfx *gfx = g->gfx;
    int cap = player_stat_cargo_max(g);
    int used = 0;
    for (int i = 0; i < RES_COUNT; i++) used += g->player.cargo[i];
    f32 x = 26.f, y = (f32)gfx->h - 168.f;
    Rect panel = rect(x - 10.f, y - 32.f, 300.f, 168.f);
    ui_panel_soft(g, panel, UI_GOLD);
    gfx_blit(gfx, &g->assets.ui_cargo, 0, rect(x, y - 26.f, 18.f, 18.f), UI_GOLD, 0.9f, BLEND_ALPHA);
    gfx_text(gfx, &g->assets.font_tiny, v2(x + 24.f, y - 24.f), "CARGO HOLD", UI_DIM, 0);
    char buf[48];
    sm_sprintf(buf, sizeof(buf), "%d / %d", used, cap);
    gfx_text(gfx, &g->assets.font_small, v2(x + 260.f, y - 27.f), buf,
             used >= cap ? UI_RED : UI_INK, 2);
    ui_bar(g, rect(x, y - 2.f, 260.f, 6.f), (f32)used / (f32)SM_MAX(1, cap),
           used >= cap ? UI_RED : UI_GOLD, SM_RGBA(12, 18, 26, 220), 1);
    /* list resources with icons */
    int shown = 0;
    for (int i = 0; i < RES_COUNT && shown < 12; i++) {
        if (g->player.cargo[i] <= 0) continue;
        f32 iy = y + 14.f + (f32)(shown / 2) * 26.f;
        f32 ix = x + (f32)(shown % 2) * 132.f;
        gfx_blit(gfx, &g->assets.res_icon[i], 0, rect(ix, iy, 20.f, 20.f), 0xFFFFFFFF, 1.f, BLEND_ALPHA);
        gfx_text(gfx, &g->assets.font_tiny, v2(ix + 24.f, iy + 1.f), k_resources[i].short_name,
                 k_resources[i].color, 0);
        sm_sprintf(buf, sizeof(buf), "%d", g->player.cargo[i]);
        gfx_text(gfx, &g->assets.font_small, v2(ix + 108.f, iy - 1.f), buf, UI_INK, 2);
        shown++;
    }
    if (shown == 0)
        gfx_text(gfx, &g->assets.font_small, v2(x + 130.f, y + 26.f), "hold empty - go cut some rock", UI_DIMMER, 2);
}

static void hud_radar(Game *g) {
    Gfx *gfx = g->gfx;
    Vec2 c = v2((f32)gfx->w - 108.f, (f32)gfx->h - 108.f);
    f32 r = 82.f;
    gfx_fill(gfx, rect(c.x - r - 8.f, c.y - r - 8.f, (r + 8.f) * 2.f, (r + 8.f) * 2.f), SM_RGBA(6, 10, 16, 170));
    gfx_ring(gfx, c, r, 1.2f, SM_RGBA(90, 130, 160, 120));
    gfx_ring(gfx, c, r * 0.66f, 1.f, SM_RGBA(80, 110, 140, 70));
    gfx_ring(gfx, c, r * 0.33f, 1.f, SM_RGBA(80, 110, 140, 50));
    /* sweep */
    f32 sweep = fmodf(g->time * 0.9f, SM_TAU);
    for (int i = 0; i < 14; i++) {
        f32 a = sweep - (f32)i * 0.045f;
        gfx_line_add(gfx, c, v2(c.x + cosf(a) * r, c.y + sinf(a) * r), 1.2f,
                     SM_RGBA(90, 200, 230, (u32)(70 - i * 4)));
    }
    f32 world_range = 2600.f;
    f32 scale = r / world_range;
    /* station */
    {
        Vec2 d = v2mul(v2sub(v2(0, 0), g->player.pos), scale);
        if (v2len(d) < r) {
            gfx_circle(gfx, v2add(c, d), 4.f, SM_RGB(255, 210, 140));
            gfx_ring(gfx, v2add(c, d), 8.f, 1.f, SM_RGBA(255, 210, 140, 120));
        }
    }
    for (int i = 0; i < SM_MAX_ASTEROIDS; i++) {
        Asteroid *a = &g->asteroids[i];
        if (!a->active) continue;
        Vec2 d = v2mul(v2sub(a->pos, g->player.pos), scale);
        if (v2len(d) > r) continue;
        f32 s = 1.4f + a->radius * 0.035f;
        gfx_fill(gfx, rect(c.x + d.x - s * 0.5f, c.y + d.y - s * 0.5f, s, s),
                 a->rich ? k_resources[a->res].color : SM_RGBA(150, 170, 190, 170));
    }
    for (int i = 0; i < SM_MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->active) continue;
        Vec2 d = v2mul(v2sub(e->pos, g->player.pos), scale);
        if (v2len(d) > r) continue;
        gfx_circle(gfx, v2add(c, d), 3.f, UI_RED);
        gfx_glow(gfx, v2add(c, d), 9.f, UI_RED, 0.5f);
    }
    for (int i = 0; i < SM_MAX_DRONES; i++) {
        Drone *dr = &g->drones[i];
        if (!dr->active) continue;
        Vec2 d = v2mul(v2sub(dr->pos, g->player.pos), scale);
        if (v2len(d) > r) continue;
        gfx_circle(gfx, v2add(c, d), 2.2f, UI_CYAN);
    }
    /* player */
    gfx_circle(gfx, c, 3.2f, 0xFFFFFFFF);
    Vec2 fwd = v2from_angle(g->player.angle);
    gfx_line(gfx, c, v2add(c, v2mul(fwd, 10.f)), 1.6f, 0xFFFFFFFF);
    gfx_text(gfx, &g->assets.font_tiny, v2(c.x, c.y + r + 12.f), "TACTICAL", UI_DIMMER, 1);
}

static void hud_targets(Game *g) {
    Gfx *gfx = g->gfx;
    /* mining reticle */
    Player *p = &g->player;
    if (p->mining_target >= 0 && g->asteroids[p->mining_target].active) {
        Asteroid *a = &g->asteroids[p->mining_target];
        Vec2 sp = gfx_w2s(gfx, a->pos);
        f32 r = a->radius * 1.35f * gfx->zoom;
        f32 rot = g->time * 1.2f;
        for (int k = 0; k < 4; k++) {
            f32 a0 = rot + (f32)k * SM_PI * 0.5f;
            gfx_arc(gfx, sp, r, a0, a0 + 0.55f, 2.f, UI_CYAN);
        }
        gfx_text(gfx, &g->assets.font_tiny, v2(sp.x, sp.y - r - 18.f), k_resources[a->res].name,
                 k_resources[a->res].color, 1);
        ui_bar(g, rect(sp.x - r * 0.8f, sp.y + r + 8.f, r * 1.6f, 5.f),
               1.f - a->hull / SM_MAX(1.f, a->hull_max), k_resources[a->res].glow, SM_RGBA(8, 12, 18, 200), 1);
        char buf[48];
        sm_sprintf(buf, sizeof(buf), "%.1f t", a->amount);
        gfx_text(gfx, &g->assets.font_tiny, v2(sp.x, sp.y + r + 18.f), buf, UI_DIM, 1);
    }
    /* enemy markers + off-screen arrows */
    for (int i = 0; i < SM_MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->active) continue;
        Vec2 sp = gfx_w2s(gfx, e->pos);
        int onscreen = sp.x > 0 && sp.y > 0 && sp.x < (f32)gfx->w && sp.y < (f32)gfx->h;
        if (onscreen) {
            if (e->hull < e->hull_max) {
                ui_bar(g, rect(sp.x - 22.f, sp.y - e->radius * 1.5f * gfx->zoom - 12.f, 44.f, 4.f),
                       e->hull / e->hull_max, UI_RED, SM_RGBA(8, 12, 18, 200), 1);
            }
        } else {
            /* arrow at screen edge */
            Vec2 dir = v2norm(v2sub(sp, v2((f32)gfx->w * 0.5f, (f32)gfx->h * 0.5f)));
            f32 m = SM_MIN((f32)gfx->w * 0.5f - 40.f, (f32)gfx->h * 0.5f - 40.f);
            Vec2 pos = v2((f32)gfx->w * 0.5f + dir.x * m, (f32)gfx->h * 0.5f + dir.y * m);
            gfx_blit_rot(gfx, &g->assets.ui_arrow, 0, pos, 14.f, 14.f, v2angle(dir) + SM_PI * 0.5f,
                         SM_RGBA(255, 120, 100, 200), 0.9f, BLEND_ADD);
        }
    }
    /* station marker */
    if (!g->station_near) {
        Vec2 sp = gfx_w2s(gfx, v2(0, 0));
        Vec2 dir = v2norm(v2sub(sp, v2((f32)gfx->w * 0.5f, (f32)gfx->h * 0.5f)));
        f32 m = SM_MIN((f32)gfx->w * 0.5f - 60.f, (f32)gfx->h * 0.5f - 60.f);
        Vec2 pos = v2((f32)gfx->w * 0.5f + dir.x * m, (f32)gfx->h * 0.5f + dir.y * m);
        gfx_blit_rot(gfx, &g->assets.ui_arrow, 0, pos, 16.f, 16.f, v2angle(dir) + SM_PI * 0.5f,
                     SM_RGBA(255, 210, 140, 190), 0.85f, BLEND_ADD);
        char buf[32];
        sm_sprintf(buf, sizeof(buf), "REFINERY %d m", (int)v2dist(g->player.pos, v2(0, 0)));
        gfx_text(gfx, &g->assets.font_tiny, v2(pos.x, pos.y + 16.f), buf, SM_RGB(220, 190, 140), 1);
    }
    /* crosshair at the mouse */
    if (g_ui_in) {
        Vec2 m = g_ui_in->mouse;
        u32 cc = SM_RGBA(190, 240, 255, 160);
        gfx_fill(gfx, rect(m.x - 10.f, m.y - 0.5f, 6.f, 1.f), cc);
        gfx_fill(gfx, rect(m.x + 4.f, m.y - 0.5f, 6.f, 1.f), cc);
        gfx_fill(gfx, rect(m.x - 0.5f, m.y - 10.f, 1.f, 6.f), cc);
        gfx_fill(gfx, rect(m.x - 0.5f, m.y + 4.f, 1.f, 6.f), cc);
        gfx_circle(gfx, m, 1.4f, 0xFFFFFFFF);
    }
    /* drone strip */
    int dc = 0;
    for (int i = 0; i < SM_MAX_DRONES; i++) if (g->drones[i].active) dc++;
    if (dc > 0) {
        f32 x = (f32)gfx->w - 232.f, y = 88.f;
        for (int i = 0; i < SM_MAX_DRONES; i++) {
            Drone *d = &g->drones[i];
            if (!d->active) continue;
            Rect r = rect(x + (f32)(i % 2) * 106.f, y + (f32)(i / 2) * 30.f, 100.f, 26.f);
            gfx_fill(gfx, r, SM_RGBA(8, 14, 22, 190));
            gfx_rect_outline(gfx, r, 1.f, SM_RGBA(60, 100, 130, 120));
            gfx_blit(gfx, &g->assets.ui_drone, 0, rect(r.x + 4.f, r.y + 4.f, 18.f, 18.f), UI_CYAN, 0.9f, BLEND_ALPHA);
            const char *st = "SEEK";
            u32 c = UI_DIM;
            if (d->state == DRONE_MINE) { st = "MINING"; c = UI_GOLD; }
            else if (d->state == DRONE_RETURN) { st = "HAULING"; c = UI_GREEN; }
            else if (d->state == DRONE_UNLOAD) { st = "UNLOAD"; c = UI_GREEN; }
            gfx_text(gfx, &g->assets.font_tiny, v2(r.x + 26.f, r.y + 4.f), st, c, 0);
            f32 frac = sm_saturate(d->cargo_amount / (f32)k_drones[d->type].capacity);
            gfx_fill(gfx, rect(r.x + 26.f, r.y + 16.f, 66.f, 3.f), SM_RGBA(16, 24, 34, 220));
            gfx_fill(gfx, rect(r.x + 26.f, r.y + 16.f, 66.f * frac, 3.f),
                     d->cargo_res >= 0 ? k_resources[d->cargo_res].color : UI_DIM);
        }
    }
    /* interaction prompt */
    if (g->station_prompt > 0.02f) {
        f32 a = g->station_prompt;
        Vec2 c = v2((f32)gfx->w * 0.5f, (f32)gfx->h * 0.72f);
        Rect r = rect(c.x - 150.f, c.y - 20.f, 300.f, 42.f);
        gfx_fill(gfx, r, SM_RGBA(8, 14, 22, (u32)(210 * a)));
        gfx_rect_outline(gfx, r, 1.f, SM_RGBA(SM_GET_R(UI_CYAN), SM_GET_G(UI_CYAN), SM_GET_B(UI_CYAN), (u32)(200 * a)));
        gfx_text_spaced(gfx, &g->assets.font_head, v2(c.x, c.y - 9.f), "PRESS  E  TO DOCK", sm_col_scale(UI_CYAN, a), 1, 2.f);
    }
    /* low hull warning */
    if (g->player.hull / SM_MAX(1.f, g->player.hull_max) < 0.3f && !g->player.dead) {
        f32 pulse = 0.5f + 0.5f * sinf(g->time * 6.f);
        gfx_fill(gfx, rect(0, 0, (f32)gfx->w, 6.f), SM_RGBA(255, 60, 50, (u32)(120 * pulse)));
        gfx_fill(gfx, rect(0, (f32)gfx->h - 6.f, (f32)gfx->w, 6.f), SM_RGBA(255, 60, 50, (u32)(120 * pulse)));
        gfx_blit(gfx, &g->assets.ui_warn, 0, rect((f32)gfx->w * 0.5f - 16.f, 72.f, 32.f, 32.f), UI_RED, pulse, BLEND_ALPHA);
        gfx_text(gfx, &g->assets.font_small, v2((f32)gfx->w * 0.5f, 108.f), "HULL CRITICAL", UI_RED, 1);
        gfx_glow(gfx, v2((f32)gfx->w * 0.5f, 88.f), 70.f, UI_RED, 0.4f * pulse);
    }
    /* control hints */
    if (g->save.gameplay.show_hints && g->sector_time < 26.f && g->screen == SCREEN_PLAY) {
        f32 a = sm_saturate((26.f - g->sector_time) / 4.f);
        const char *hint = "W/S THRUST   A/D STRAFE   MOUSE AIM   LMB FIRE   RMB MINE   SPACE BOOST   Q SCAN   E DOCK";
        gfx_text(gfx, &g->assets.font_tiny, v2((f32)gfx->w * 0.5f, (f32)gfx->h - 26.f),
                 hint, SM_RGBA(150, 180, 200, (u32)(190 * a)), 1);
    }
}

void ui_render_hud(Game *g) {
    gfx_push_clip(g->gfx, rect(0, 0, (f32)g->gfx->w, (f32)g->gfx->h));
    hud_bars(g);
    hud_topbar(g);
    hud_cargo(g);
    hud_radar(g);
    hud_targets(g);
    ui_toasts(g);
    if (g->screen == SCREEN_PLAY && g->paused) {
        gfx_fill(g->gfx, rect(0, 0, (f32)g->gfx->w, (f32)g->gfx->h), SM_RGBA(0, 0, 0, 60));
    }
    gfx_pop_clip(g->gfx);
}

/* ------------------------------------------------------------------ pause -- */
void ui_render_pause(Game *g, Input *in) {
    (void)in;
    Gfx *gfx = g->gfx;
    gfx_fill(gfx, rect(0, 0, (f32)gfx->w, (f32)gfx->h), SM_RGBA(3, 6, 12, 190));
    Rect p = rect(gfx->w * 0.5f - 220.f, gfx->h * 0.5f - 210.f, 440.f, 420.f);
    ui_panel(g, p, UI_CYAN);
    ui_header(g, rect(p.x + 26.f, p.y + 14.f, p.w - 52.f, 40.f), "PAUSED", "systems on standby", UI_CYAN);
    const char *labels[] = { "RESUME", "SAVE GAME", "SETTINGS", "QUIT TO MENU" };
    for (int i = 0; i < 4; i++) {
        Rect r = rect(p.x + 30.f, p.y + 80.f + (f32)i * 62.f, p.w - 60.f, 50.f);
        if (ui_button(g, r, labels[i], NULL, 1, UI_CYAN)) {
            g->pause_sel = i;
            switch (i) {
                case 0: g->paused = 0; game_set_screen(g, SCREEN_PLAY); break;
                case 1:
                    if (save_write(g) == 0) game_toast(g, "PROGRESS SAVED", SM_RGB(140, 240, 180));
                    else game_toast(g, "SAVE FAILED", UI_RED);
                    break;
                case 2: g->menu_sel = 10; game_set_screen(g, SCREEN_MENU); g->paused = 0; break;
                case 3:
                    save_write(g);
                    g->paused = 0;
                    game_set_screen(g, SCREEN_MENU);
                    snd_music(g, "music_menu", 1.f);
                    break;
                default: break;
            }
        }
    }
    char buf[128];
    sm_sprintf(buf, sizeof(buf), "SECTOR %s   //   %s   //   %d CR",
               k_sectors[g->sector].name, k_ships[g->save.ship].name, (int)g->save.credits);
    gfx_text(gfx, &g->assets.font_tiny, v2(p.x + p.w * 0.5f, p.y + p.h - 26.f), buf, UI_DIM, 1);
    ui_toasts(g);
}

/* -------------------------------------------------------------- game over -- */
void ui_render_gameover(Game *g, Input *in) {
    (void)in;
    Gfx *gfx = g->gfx;
    f32 t = g->screen_time;
    gfx_fill(gfx, rect(0, 0, (f32)gfx->w, (f32)gfx->h), SM_RGBA(10, 2, 4, (u32)(120 + 90 * sm_saturate(t))));
    Rect p = rect(gfx->w * 0.5f - 300.f, gfx->h * 0.5f - 200.f, 600.f, 400.f);
    ui_panel(g, p, UI_RED);
    gfx_blit(gfx, &g->assets.ui_skull, 0, rect(p.x + p.w * 0.5f - 28.f, p.y + 26.f, 56.f, 56.f), UI_RED, 0.9f, BLEND_ALPHA);
    gfx_text_spaced(gfx, &g->assets.font_title, v2(p.x + p.w * 0.5f, p.y + 92.f), "HULL BREACHED", UI_RED, 1, 3.f);
    gfx_text(gfx, &g->assets.font_small, v2(p.x + p.w * 0.5f, p.y + 142.f),
             "Your ship is gone. The ore you carried was lost with it.", UI_DIM, 1);
    char buf[128];
    sm_sprintf(buf, sizeof(buf), "SALVAGED CREDITS KEPT:  %lld CR", (long long)g->save.credits);
    gfx_text(gfx, &g->assets.font_body, v2(p.x + p.w * 0.5f, p.y + 176.f), buf, UI_GOLD, 1);
    sm_sprintf(buf, sizeof(buf), "ENEMIES DESTROYED  %d    //    ROCKS MINED  %d",
               g->save.stat_enemies_killed, g->save.stat_asteroids_mined);
    gfx_text(gfx, &g->assets.font_tiny, v2(p.x + p.w * 0.5f, p.y + 204.f), buf, UI_DIM, 1);
    if (ui_button(g, rect(p.x + 40.f, p.y + 250.f, 240.f, 52.f), "RELAUNCH", "fly again from the refinery", 1, UI_CYAN)) {
        player_reset(g, 0);
        world_enter_sector(g, g->sector);
        g->game_over_timer = 0;
        game_set_screen(g, SCREEN_PLAY);
    }
    if (ui_button(g, rect(p.x + p.w - 280.f, p.y + 250.f, 240.f, 52.f), "MAIN MENU", NULL, 1, UI_RED)) {
        game_set_screen(g, SCREEN_MENU);
        snd_music(g, "music_menu", 1.2f);
    }
    gfx_text(gfx, &g->assets.font_tiny, v2(p.x + p.w * 0.5f, p.y + 330.f),
             "TIP: keep an eye on your shield - it recharges out of combat", UI_DIMMER, 1);
    ui_toasts(g);
}

/* ------------------------------------------------------------------ update -- */
void ui_update(Game *g, Input *in, f32 dt) {
    g_ui_in = in;
    (void)dt;
    if (g->screen == SCREEN_MENU || g->screen == SCREEN_CREDITS) {
        if (in->pressed[SM_KEY_ESCAPE]) {
            if (g->menu_sel == 10 || g->menu_sel == 20) g->menu_sel = 0;
            else if (g->confirm_action) g->confirm_action = 0;
        }
    }
    if (g->screen == SCREEN_PAUSED && in->pressed[SM_KEY_ESCAPE]) {
        g->paused = 0;
        game_set_screen(g, SCREEN_PLAY);
    }
}
