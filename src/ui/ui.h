// FRACTURE — immediate-mode UI with the game's industrial visual language.
#pragma once
#include "core/engine.h"
#include <string>
#include <vector>
#include <cstdio>

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    Rect() {}
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
    bool contains(float px, float py) const { return px >= x && px <= x + w && py >= y && py <= y + h; }
    Rect inset(float i) const { return {x + i, y + i, w - 2 * i, h - 2 * i}; }
};

namespace ui {
// palette
extern const uint32_t COL_BG;
extern const uint32_t COL_PANEL;
extern const uint32_t COL_PANEL_LIGHT;
extern const uint32_t COL_BORDER;
extern const uint32_t COL_ACCENT;
extern const uint32_t COL_ACCENT_DIM;
extern const uint32_t COL_TEXT;
extern const uint32_t COL_TEXT_DIM;
extern const uint32_t COL_DANGER;
extern const uint32_t COL_GOOD;
extern const uint32_t COL_STEEL;

uint32_t rgba(int r, int g, int b, int a = 255);
uint32_t withAlpha(uint32_t c, int a);
uint32_t mixColor(uint32_t a, uint32_t b, float t);
}

class UI {
public:
    UIDrawList list;
    InputState* input = nullptr;
    float mx = 0, my = 0;
    bool click = false, down = false, rclick = false;
    int hotId = 0, activeId = 0;
    float scale = 1.0f;
    float dt = 0;
    std::vector<SfxId> soundQueue;
    int openDropdown = 0;       // id of open dropdown
    int hoveredId = 0;
    float scrollDelta = 0;

    void begin(InputState& in, int w, int h, float dt_);
    void end();
    void addQuad(float x, float y, float w, float h, uint32_t c, float u0, float v0, float u1, float v1);
    void addQuadGrad(float x, float y, float w, float h, uint32_t cTop, uint32_t cBottom);
    void addQuadGradH(float x, float y, float w, float h, uint32_t cL, uint32_t cR);
    void addRect(Rect r, uint32_t c) { addQuad(r.x, r.y, r.w, r.h, c, uSolid(), vSolid(), uSolid(), vSolid()); }
    float uSolid() const;
    float vSolid() const;

    void panel(Rect r, uint32_t col = ui::COL_PANEL, bool border = true);
    void panelGrad(Rect r, uint32_t top, uint32_t bottom);
    void border(Rect r, uint32_t col, float t = 1.0f);
    void hazardStripe(float x, float y, float w, float h, uint32_t c1, uint32_t c2);
    void scanlines(Rect r, uint32_t col, float spacing);
    void text(float x, float y, float size, uint32_t col, const std::string& s);
    void textShadow(float x, float y, float size, uint32_t col, const std::string& s);
    void textCentered(Rect r, float size, uint32_t col, const std::string& s);
    void textRight(Rect r, float size, uint32_t col, const std::string& s);
    void textLeft(Rect r, float size, uint32_t col, const std::string& s, float pad = 12);
    float measure(const std::string& s, float size) const;
    void wrapText(float x, float y, float size, uint32_t col, const std::string& s, float maxW, float lineH);
    void textSpaced(float x, float y, float size, uint32_t col, const std::string& s, float extra);

    // widgets (id derived from label hash unless given)
    bool button(Rect r, const std::string& label, int id = 0, bool enabled = true);
    bool buttonAccent(Rect r, const std::string& label, int id = 0, bool enabled = true);
    bool buttonDanger(Rect r, const std::string& label, int id = 0, bool enabled = true);
    bool iconButton(Rect r, const std::string& label, int id = 0);
    bool toggle(Rect r, const std::string& label, bool& value, int id = 0);
    bool slider(Rect r, const std::string& label, float& value, float mn, float mx_, float step = 0, int id = 0,
                const char* fmt = "%.2f");
    bool sliderInt(Rect r, const std::string& label, int& value, int mn, int mx_, int id = 0);
    int dropdown(Rect r, const std::string& label, const char* const* items, int count, int index, int id = 0);
    void progress(Rect r, float t, uint32_t col, const std::string& label = "");
    void sectionHeader(float x, float y, float w, const std::string& title);
    bool isHovered(Rect r) const { return r.contains(mx, my); }
    void sound(SfxId s) { soundQueue.push_back(s); }
    int makeId(const std::string& s);
};
