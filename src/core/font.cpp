#include "core/engine.h"
#include "core/fontatlas.h"

const FontAtlas& fontAtlas() {
    static FontAtlas fa;
    static bool init = false;
    if (!init) {
        init = true;
        fa.width = FONT_ATLAS_W;
        fa.height = FONT_ATLAS_H;
        fa.cellW = FONT_CELL_W;
        fa.cellH = FONT_CELL_H;
        fa.cols = FONT_COLS;
        fa.data = FONT_ALPHA;
    }
    return fa;
}

// returns the u coordinate of the left edge of the glyph; increments pen by advance
float FontAtlas::uvGlyph(char c, float* w, float* h) const {
    if (c < 32 || c > 126) c = '?';
    int idx = c - 32;
    int cx = idx % cols, cy = idx / cols;
    *w = (float)cellW / (float)width;
    *h = (float)cellH / (float)height;
    return (float)(cx * cellW) / (float)width + (0.5f / (float)width);
}
