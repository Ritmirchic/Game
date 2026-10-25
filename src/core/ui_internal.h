/* SPACE MINER — ui_internal.h : shared UI helpers (ui.c / ui_dock.c) */
#ifndef SM_UI_INTERNAL_H
#define SM_UI_INTERNAL_H
#include "state.h"

#define UI_INK     SM_RGB(228, 238, 248)
#define UI_DIM     SM_RGB(138, 158, 178)
#define UI_DIMMER  SM_RGB(92, 110, 128)
#define UI_CYAN    SM_RGB(112, 226, 255)
#define UI_GOLD    SM_RGB(255, 202, 110)
#define UI_GREEN   SM_RGB(120, 240, 168)
#define UI_RED     SM_RGB(255, 108, 92)
#define UI_VIOLET  SM_RGB(178, 140, 255)
#define UI_PANEL   SM_RGBA(9, 14, 22, 232)
#define UI_PANEL_2 SM_RGBA(14, 21, 32, 240)
#define UI_BORDER  SM_RGBA(86, 128, 160, 120)
#define UI_BORDER_HOT SM_RGBA(140, 226, 255, 190)

typedef enum { BTN_NORMAL = 0, BTN_HOVER, BTN_DISABLED, BTN_SELECTED, BTN_TOGGLE_ON } BtnState;

/* shared state for immediate-mode interaction */
extern Input *g_ui_in;
extern int g_ui_hover_id;
extern int g_ui_active_slider;
extern u32 g_ui_sfx_frame;

void ui_begin(Font *font) ;
void ui_end(void);

void ui_panel(Game *g, Rect r, u32 accent);
void ui_panel_soft(Game *g, Rect r, u32 accent);
void ui_header(Game *g, Rect r, const char *title, const char *sub, u32 accent);
void ui_divider(Game *g, Rect r, u32 color);
void ui_bar(Game *g, Rect r, f32 frac, u32 fill, u32 back, int segments);
void ui_button_draw(Game *g, Rect r, const char *label, const char *sublabel, BtnState state, u32 accent);
int  ui_button(Game *g, Rect r, const char *label, const char *sublabel, int enabled, u32 accent);
int  ui_icon_button(Game *g, Rect r, const Sheet *icon, const char *label, int enabled, u32 accent);
int  ui_hover(Game *g, Rect r);
int  ui_clicked(Game *g, Rect r);
int  ui_slider(Game *g, Rect r, f32 *value, const char *label, u32 accent);
int  ui_toggle(Game *g, Rect r, const char *label, int *value, u32 accent);
void ui_scroll_capture(Game *g, Rect area, int *scroll, int content_h);
void ui_label(Game *g, Vec2 pos, const char *text, u32 color, int align);
void ui_label_small(Game *g, Vec2 pos, const char *text, u32 color, int align);
void ui_label_head(Game *g, Vec2 pos, const char *text, u32 color, int align);
void ui_money(Game *g, Vec2 pos, s64 credits, int align);
void ui_draw_bg_menu(Game *g, f32 t);
void ui_tab_bar(Game *g, Rect r, const char **labels, int count, int *selected, u32 accent);
void ui_progress_ring(Game *g, Vec2 c, f32 r, f32 frac, u32 color, f32 thickness);
void ui_toasts(Game *g);

/* dock page renderers (ui_dock.c) */
void ui_dock_market(Game *g, Rect area);
void ui_dock_upgrades(Game *g, Rect area);
void ui_dock_shipyard(Game *g, Rect area);
void ui_dock_drones(Game *g, Rect area);
void ui_dock_map(Game *g, Rect area);
void ui_dock_awards(Game *g, Rect area);
void ui_dock_settings(Game *g, Rect area);

#endif
