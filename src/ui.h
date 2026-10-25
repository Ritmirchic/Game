// UI helper layer shared by the HUD/menu screens.
#ifndef TLN_UI_H
#define TLN_UI_H

#include "game.h"

#define UI_ACCENT 0xFF30A8E8u
#define UI_ACCENT_DIM 0xBF1C6A90u
#define UI_DANGER 0xFF2030D0u
#define UI_WARN 0xFF2090E0u
#define UI_INK 0xFFF2F0ECu
#define UI_MUTED 0xFF9A968Fu
#define UI_PANEL 0xE00C0D10u
#define UI_EDGE 0x90E8A830u

float ui_scale(Game *g);
uint32_t ui_alpha(uint32_t c, float a);
void ui_panel(Game *g, float x, float y, float w, float h, float alpha);
void ui_bar(Game *g, float x, float y, float w, float h, float frac, uint32_t col, uint32_t bg, float alpha);
void ui_label(Game *g, const Font *f, float x, float y, float sc, const char *s, uint32_t c);
void ui_label_c(Game *g, const Font *f, float cx, float y, float sc, const char *s, uint32_t c);
void ui_label_r(Game *g, const Font *f, float rx, float y, float sc, const char *s, uint32_t c);
void ui_heading(Game *g, const Font *f, float cx, float y, float sc, const char *s, uint32_t c);
float ui_text_w(const Font *f, float sc, const char *s);
int  ui_button(Game *g, float x, float y, float w, float h, const char *text, int selected, float alpha, int enabled);
void ui_keycap(Game *g, float x, float y, const char *key);

void draw_hud(Game *g);
void draw_pause(Game *g);
void draw_shop(Game *g);
void draw_settings(Game *g);
void draw_controls(Game *g);
void draw_lobby(Game *g);
void draw_gameover(Game *g);
void draw_victory(Game *g);
const char *g_tip(int night);

#endif
