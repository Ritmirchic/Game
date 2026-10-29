#include "ui/ui.h"
#include "core/fontatlas.h"
#include <cmath>
#include <cstring>

namespace ui {
// All UI colours are packed 0xAARRGGBB.
const uint32_t COL_BG = 0xFF0B0D10;
const uint32_t COL_PANEL = 0xFF14171C;
const uint32_t COL_PANEL_LIGHT = 0xFF1D2228;
const uint32_t COL_BORDER = 0xFF2C333B;
const uint32_t COL_ACCENT = 0xFFFF8A1E;
const uint32_t COL_ACCENT_DIM = 0xFF8A4B12;
const uint32_t COL_TEXT = 0xFFE9E6DF;
const uint32_t COL_TEXT_DIM = 0xFF8A929B;
const uint32_t COL_DANGER = 0xFFE0463C;
const uint32_t COL_GOOD = 0xFF66B36A;
const uint32_t COL_STEEL = 0xFF5F7C8F;

uint32_t rgba(int r, int g, int b, int a) {
    return ((uint32_t)(a & 0xFF) << 24) | ((uint32_t)(r & 0xFF) << 16) | ((uint32_t)(g & 0xFF) << 8) | (uint32_t)(b & 0xFF);
}
uint32_t withAlpha(uint32_t c, int a) { return (c & 0x00FFFFFFu) | ((uint32_t)(a & 0xFF) << 24); }
uint32_t mixColor(uint32_t a, uint32_t b, float t) {
    int aa = (a >> 24) & 0xFF, ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int ba = (b >> 24) & 0xFF, br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    return rgba((int)lerpf(ar, br, t), (int)lerpf(ag, bg, t), (int)lerpf(ab, bb, t), (int)lerpf(aa, ba, t));
}
}

void UI::begin(InputState& in, int w, int h, float dt_) {
    list.reset(w, h);
    input = &in;
    mx = in.mouseX;
    my = in.mouseY;
    click = in.isPressed(K_LBUTTON);
    down = in.isDown(K_LBUTTON);
    rclick = in.isPressed(K_RBUTTON);
    dt = dt_;
    scrollDelta = in.wheel;
    hotId = 0;
    hoveredId = 0;
    if (click) {
        // clicking outside an open dropdown closes it
    }
}

void UI::end() {
    if (!down) activeId = 0;
}

float UI::uSolid() const {
    const FontAtlas& fa = fontAtlas();
    // the space glyph cell holds a solid white block in its centre
    int idx = ' ' - 32;
    int cx = idx % fa.cols, cy = idx / fa.cols;
    float u = (cx * fa.cellW + fa.cellW * 0.5f) / fa.width;
    return u;
}
float UI::vSolid() const {
    const FontAtlas& fa = fontAtlas();
    int idx = ' ' - 32;
    int cy = idx / fa.cols;
    float v = (cy * fa.cellH + fa.cellH * 0.5f) / fa.height;
    return v;
}

void UI::addQuad(float x, float y, float w, float h, uint32_t c, float u0, float v0, float u1, float v1) {
    // UIDrawList is a non-indexed triangle list: two triangles per quad
    UIVert a = {x, y, u0, v0, c};
    UIVert b = {x + w, y, u1, v0, c};
    UIVert cc = {x + w, y + h, u1, v1, c};
    UIVert d = {x, y + h, u0, v1, c};
    list.verts.push_back(a); list.verts.push_back(b); list.verts.push_back(cc);
    list.verts.push_back(a); list.verts.push_back(cc); list.verts.push_back(d);
}

void UI::addQuadGrad(float x, float y, float w, float h, uint32_t cTop, uint32_t cBottom) {
    float u = uSolid(), vv = vSolid();
    UIVert a = {x, y, u, vv, cTop};
    UIVert b = {x + w, y, u, vv, cTop};
    UIVert cc = {x + w, y + h, u, vv, cBottom};
    UIVert d = {x, y + h, u, vv, cBottom};
    list.verts.push_back(a); list.verts.push_back(b); list.verts.push_back(cc);
    list.verts.push_back(a); list.verts.push_back(cc); list.verts.push_back(d);
}

void UI::addQuadGradH(float x, float y, float w, float h, uint32_t cL, uint32_t cR) {
    float u = uSolid(), vv = vSolid();
    UIVert a = {x, y, u, vv, cL};
    UIVert b = {x + w, y, u, vv, cR};
    UIVert cc = {x + w, y + h, u, vv, cR};
    UIVert d = {x, y + h, u, vv, cL};
    list.verts.push_back(a); list.verts.push_back(b); list.verts.push_back(cc);
    list.verts.push_back(a); list.verts.push_back(cc); list.verts.push_back(d);
}

void UI::panel(Rect r, uint32_t col, bool borderOn) {
    addRect(r, col);
    if (borderOn) border(r, ui::COL_BORDER, 1);
}

void UI::panelGrad(Rect r, uint32_t top, uint32_t bottom) { addQuadGrad(r.x, r.y, r.w, r.h, top, bottom); }

void UI::border(Rect r, uint32_t col, float t) {
    addRect({r.x, r.y, r.w, t}, col);
    addRect({r.x, r.y + r.h - t, r.w, t}, col);
    addRect({r.x, r.y, t, r.h}, col);
    addRect({r.x + r.w - t, r.y, t, r.h}, col);
}

void UI::hazardStripe(float x, float y, float w, float h, uint32_t c1, uint32_t c2) {
    addRect({x, y, w, h}, c2);
    float step = h * 1.6f;
    int slices = 10;
    for (float px = x - h * 2; px < x + w + h; px += step) {
        for (int i = 0; i < slices; i++) {
            float t0 = (float)i / slices, t1 = (float)(i + 1) / slices;
            float x0 = px + (1.0f - t0) * h;
            float x1 = px + h * 0.5f + (1.0f - t1) * h;
            float cx0 = std::max(x, x0);
            float cx1 = std::min(x + w, x1);
            if (cx1 <= cx0) continue;
            addRect({cx0, y + t0 * h, cx1 - cx0, h / slices + 0.6f}, c1);
        }
    }
}

void UI::scanlines(Rect r, uint32_t col, float spacing) {
    for (float y = r.y; y < r.y + r.h; y += spacing) addRect({r.x, y, r.w, 1}, col);
}

void UI::text(float x, float y, float size, uint32_t col, const std::string& s) {
    const FontAtlas& fa = fontAtlas();
    float glyphW = size * fa.cellW / 26.0f;
    float glyphH = size * fa.cellH / 26.0f;
    float advance = size * 0.602f;
    float uStep = (float)fa.cellW / fa.width;
    float vStep = (float)fa.cellH / fa.height;
    float px = x;
    float lineY = y;
    for (char ch : s) {
        if (ch == '\n') {
            px = x;
            lineY += glyphH * 1.16f;
            continue;
        }
        unsigned char c = (unsigned char)ch;
        if (c < 32 || c > 126) c = '?';
        if (c != ' ') {
            int idx = c - 32;
            int cx = idx % fa.cols, cy = idx / fa.cols;
            float u0 = (float)(cx * fa.cellW) / fa.width;
            float v0 = (float)(cy * fa.cellH) / fa.height;
            addQuad(px, lineY - glyphH * 0.5f, glyphW, glyphH, col, u0, v0, u0 + uStep, v0 + vStep);
        }
        px += advance;
    }
}

void UI::textShadow(float x, float y, float size, uint32_t col, const std::string& s) {
    text(x + 1.5f, y + 1.5f, size, ui::rgba(0, 0, 0, 180), s);
    text(x, y, size, col, s);
}

void UI::textCentered(Rect r, float size, uint32_t col, const std::string& s) {
    float w = measure(s, size);
    text(r.x + (r.w - w) * 0.5f, r.y + r.h * 0.5f, size, col, s);
}

void UI::textRight(Rect r, float size, uint32_t col, const std::string& s) {
    float w = measure(s, size);
    text(r.x + r.w - w, r.y + r.h * 0.5f, size, col, s);
}

void UI::textLeft(Rect r, float size, uint32_t col, const std::string& s, float pad) {
    text(r.x + pad, r.y + r.h * 0.5f, size, col, s);
}

float UI::measure(const std::string& s, float size) const { return s.size() * size * 0.602f; }

void UI::textSpaced(float x, float y, float size, uint32_t col, const std::string& s, float extra) {
    const FontAtlas& fa = fontAtlas();
    float glyphW = size * fa.cellW / 26.0f;
    float glyphH = size * fa.cellH / 26.0f;
    float advance = size * 0.602f + extra;
    float top = y - glyphH * 0.5f;
    float uStep = (float)fa.cellW / fa.width;
    float vStep = (float)fa.cellH / fa.height;
    float px = x;
    float startX = x;
    for (char ch : s) {
        if (ch == '\n') { px = startX; continue; }
        unsigned char c = (unsigned char)ch;
        if (c < 32 || c > 126) c = '?';
        if (c != ' ') {
            int idx = c - 32;
            int cx = idx % fa.cols, cy = idx / fa.cols;
            float u0 = (float)(cx * fa.cellW) / fa.width;
            float v0 = (float)(cy * fa.cellH) / fa.height;
            addQuad(px, top, glyphW, glyphH, col, u0, v0, u0 + uStep, v0 + vStep);
        }
        px += advance;
    }
}

void UI::wrapText(float x, float y, float size, uint32_t col, const std::string& s, float maxW, float lineH) {
    float advance = size * 0.602f;
    int perLine = std::max(1, (int)(maxW / advance));
    int i = 0, line = 0;
    while (i < (int)s.size()) {
        int len = std::min(perLine, (int)s.size() - i);
        if (i + len < (int)s.size()) {
            int brk = len;
            for (int k = len; k > 0; k--)
                if (s[i + k - 1] == ' ') { brk = k; break; }
            len = brk;
        }
        text(x, y + line * lineH, size, col, s.substr(i, len));
        i += len;
        line++;
    }
}

int UI::makeId(const std::string& s) {
    uint32_t h = 2166136261u;
    for (char c : s) { h ^= (unsigned char)c; h *= 16777619u; }
    return (int)(h & 0x7FFFFFFF);
}

bool UI::button(Rect r, const std::string& label, int id, bool enabled) {
    if (id == 0) id = makeId(label);
    bool hover = enabled && isHovered(r);
    bool pressed = hover && click;
    uint32_t top = hover ? ui::COL_PANEL_LIGHT : ui::COL_PANEL;
    if (activeId == id && down) top = ui::withAlpha(ui::COL_ACCENT, 40) | 0xFF000000;
    if (!enabled) top = ui::rgba(16, 18, 21, 220);
    addQuadGrad(r.x, r.y, r.w, r.h, top, ui::mixColor(top, ui::rgba(0, 0, 0, 255), 0.25f));
    border(r, hover && enabled ? ui::COL_ACCENT : ui::COL_BORDER, 1);
    if (hover && enabled) addRect({r.x, r.y, 3, r.h}, ui::COL_ACCENT);
    textCentered(r, 17, enabled ? (hover ? ui::COL_TEXT : ui::rgba(210, 208, 200, 255)) : ui::COL_TEXT_DIM, label);
    if (pressed) { activeId = id; }
    if (hover && click && enabled) {
        sound(SFX_UI_CLICK);
        return true;
    }
    return false;
}

bool UI::buttonAccent(Rect r, const std::string& label, int id, bool enabled) {
    if (id == 0) id = makeId(label);
    bool hover = enabled && isHovered(r);
    uint32_t top = hover ? ui::rgba(255, 152, 48, 255) : ui::COL_ACCENT;
    uint32_t bot = hover ? ui::rgba(214, 108, 20, 255) : ui::rgba(196, 96, 16, 255);
    addQuadGrad(r.x, r.y, r.w, r.h, top, bot);
    border(r, hover ? ui::rgba(255, 200, 140, 255) : ui::rgba(120, 60, 10, 255), 1);
    textCentered(r, 17, ui::rgba(20, 14, 8, 255), label);
    if (hover && click && enabled) {
        sound(SFX_UI_CLICK);
        return true;
    }
    return false;
}

bool UI::buttonDanger(Rect r, const std::string& label, int id, bool enabled) {
    if (id == 0) id = makeId(label);
    bool hover = enabled && isHovered(r);
    uint32_t top = hover ? ui::rgba(226, 84, 70, 255) : ui::rgba(58, 24, 22, 255);
    addQuadGrad(r.x, r.y, r.w, r.h, top, ui::mixColor(top, ui::rgba(0, 0, 0, 255), 0.3f));
    border(r, hover ? ui::COL_DANGER : ui::rgba(90, 40, 36, 255), 1);
    textCentered(r, 17, hover ? ui::rgba(255, 240, 236, 255) : ui::rgba(230, 170, 165, 255), label);
    if (hover && click && enabled) {
        sound(SFX_UI_CLICK);
        return true;
    }
    return false;
}

bool UI::iconButton(Rect r, const std::string& label, int id) {
    return button(r, label, id);
}

bool UI::toggle(Rect r, const std::string& label, bool& value, int id) {
    if (id == 0) id = makeId(label);
    bool hover = isHovered(r);
    if (hover && click) {
        value = !value;
        sound(SFX_UI_CLICK);
        return true;
    }
    addRect(r, ui::rgba(10, 12, 15, 255));
    border(r, hover ? ui::COL_ACCENT : ui::COL_BORDER, 1);
    textLeft(r, 16, ui::COL_TEXT, label, 12);
    Rect box(r.x + r.w - 46, r.y + (r.h - 22) * 0.5f, 38, 22);
    addQuad(box.x, box.y, box.w, box.h, value ? ui::COL_ACCENT : ui::rgba(44, 50, 58, 255), uSolid(), vSolid(), uSolid(), vSolid());
    border(box, ui::rgba(0, 0, 0, 120), 1);
    float kx = value ? box.x + box.w - 19 : box.x + 2;
    addRect({kx, box.y + 2, 17, box.h - 4}, ui::rgba(238, 236, 230, 255));
    return false;
}

bool UI::slider(Rect r, const std::string& label, float& value, float mn, float mx_, float step, int id, const char* fmt) {
    if (id == 0) id = makeId(label);
    bool hover = isHovered(r);
    Rect track(r.x + 220, r.y + r.h * 0.5f - 4, r.w - 320, 8);
    if (hover && click) activeId = id;
    if (activeId == id && down) {
        float t = clampf((mx - track.x) / std::max(1.0f, track.w), 0, 1);
        value = mn + (mx_ - mn) * t;
        if (step > 0) value = mn + std::round((value - mn) / step) * step;
    }
    value = clampf(value, std::min(mn, mx_), std::max(mn, mx_));
    textLeft(r, 16, ui::COL_TEXT, label, 12);
    addRect(track, ui::rgba(8, 10, 12, 255));
    border(track, ui::COL_BORDER, 1);
    float t = (mx_ != mn) ? (value - mn) / (mx_ - mn) : 0;
    addRect({track.x, track.y, track.w * t, track.h}, activeId == id || hover ? ui::COL_ACCENT : ui::COL_ACCENT_DIM);
    addRect({track.x + track.w * t - 3, track.y - 5, 6, track.h + 10}, ui::rgba(240, 238, 232, 255));
    char buf[64];
    snprintf(buf, sizeof(buf), fmt, value);
    Rect valR(r.x + r.w - 92, r.y, 80, r.h);
    textRight(valR, 16, ui::COL_TEXT, buf);
    return activeId == id && down;
}

bool UI::sliderInt(Rect r, const std::string& label, int& value, int mn, int mx_, int id) {
    float f = (float)value;
    bool ch = slider(r, label, f, (float)mn, (float)mx_, 1.0f, id, "%.0f");
    int nv = (int)std::round(f);
    if (nv != value) { value = nv; return true; }
    return ch;
}

int UI::dropdown(Rect r, const std::string& label, const char* const* items, int count, int index, int id) {
    if (id == 0) id = makeId(label);
    bool hover = isHovered(r);
    textLeft(r, 16, ui::COL_TEXT, label, 12);
    Rect box(r.x + 220, r.y + 3, r.w - 232, r.h - 6);
    addRect(box, ui::rgba(10, 12, 15, 255));
    border(box, hover || openDropdown == id ? ui::COL_ACCENT : ui::COL_BORDER, 1);
    const char* cur = (index >= 0 && index < count) ? items[index] : "-";
    textLeft(box, 16, ui::COL_TEXT, cur, 10);
    // arrow
    float ax = box.x + box.w - 18, ay = box.y + box.h * 0.5f;
    for (int i = 0; i < 4; i++) addRect({ax + i * 2.0f, ay - 3 + i * 1.5f, 8 - i * 2.0f, 2}, ui::COL_TEXT_DIM);
    if (hover && click) {
        openDropdown = (openDropdown == id) ? 0 : id;
        sound(SFX_UI_CLICK);
    }
    if (openDropdown == id) {
        Rect menu(box.x, box.y + box.h + 2, box.w, count * 26.0f + 4);
        // draw at the end of the frame so it is on top: stash a deferred draw
        addRect(menu, ui::rgba(12, 14, 18, 250));
        border(menu, ui::COL_ACCENT, 1);
        int result = index;
        for (int i = 0; i < count; i++) {
            Rect ir(menu.x + 2, menu.y + 2 + i * 26.0f, menu.w - 4, 26);
            bool ih = isHovered(ir);
            if (ih) addRect(ir, ui::rgba(40, 46, 54, 255));
            textLeft(ir, 15, i == index ? ui::COL_ACCENT : ui::COL_TEXT, items[i], 10);
            if (ih && click) {
                openDropdown = 0;
                sound(SFX_UI_CLICK);
                result = i;
            }
        }
        if (click && !menu.contains(mx, my) && !r.contains(mx, my)) openDropdown = 0;
        if (result != index) return result;
    }
    return index;
}

void UI::progress(Rect r, float t, uint32_t col, const std::string& label) {
    addRect(r, ui::rgba(8, 10, 12, 255));
    border(r, ui::COL_BORDER, 1);
    addRect({r.x + 1, r.y + 1, (r.w - 2) * clampf(t, 0, 1), r.h - 2}, col);
    if (!label.empty()) textCentered(r, 14, ui::rgba(240, 238, 232, 255), label);
}

void UI::sectionHeader(float x, float y, float w, const std::string& title) {
    textSpaced(x, y, 15, ui::COL_ACCENT, title, 1.6f);
    float tw = measure(title, 15) + title.size() * 1.6f;
    addRect({x, y + 14, w, 1}, ui::COL_BORDER);
    addRect({x, y + 14, std::min(46.0f, w), 1}, ui::COL_ACCENT);
    (void)tw;
}
