// FRACTURE — immediate UI text helpers
#pragma once
#include "render.h"
#include "font_data.h"

namespace ft {

// align:0 left,1 center,2 right. y = top of text line (ascender line).
inline f32 uiTextWidth(const char* s, f32 sizePx, bool bold = false) {
    f32 scale = sizePx / (f32)FT_FONT_PX;
    f32 w = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        int ch = *p;
        if (ch < 32 || ch > 126) ch = '?';
        w += FT_FONT_GLYPHS[bold ? 1 : 0][ch - 32].adv / 64.f * scale;
    }
    return w;
}

inline void uiText(IBackend* rb, f32 x, f32 y, f32 sizePx, u8 r, u8 g, u8 b, u8 a,
                   const char* s, int align = 0, bool bold = false, f32 outline = 1.f) {
    f32 scale = sizePx / (f32)FT_FONT_PX;
    if (align) {
        f32 w = uiTextWidth(s, sizePx, bold);
        if (align == 1) x -= w * 0.5f;
        else if (align == 2) x -= w;
    }
    f32 pen = x;
    const f32 cell = (f32)FT_FONT_CELL;
    auto emit = [&](f32 ox, f32 oy, u8 rr, u8 gg, u8 bb, u8 aa) {
        f32 px = pen;
        for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
            int ch = *p;
            if (ch < 32 || ch > 126) ch = '?';
            const FTGlyph& gl = FT_FONT_GLYPHS[bold ? 1 : 0][ch - 32];
            if (ch != ' ' && gl.w > 0) {
                int gi = ch - 32;
                int col = gi % FT_FONT_COLS;
                int row = gi / FT_FONT_COLS + (bold ? (FT_FONT_ROWS / 2) : 0);
                f32 gx = (f32)(col * FT_FONT_CELL) + gl.x / 64.f;
                f32 gy = (f32)(row * FT_FONT_CELL) + gl.y / 64.f;
                UIQuad q;
                q.x = px + gl.x / 64.f * scale + ox;
                q.y = y + gl.y / 64.f * scale + oy;
                q.w = gl.w / 64.f * scale;
                q.h = gl.h / 64.f * scale;
                q.u0 = gx / FT_FONT_W; q.v0 = gy / FT_FONT_H;
                q.u1 = (gx + gl.w / 64.f) / FT_FONT_W; q.v1 = (gy + gl.h / 64.f) / FT_FONT_H;
                q.r = rr; q.g = gg; q.b = bb; q.a = aa; q.tex = 1;
                rb->pushQuad(q);
                (void)cell;
            }
            px += gl.adv / 64.f * scale;
        }
    };
    if (outline > 0.f) {
        u8 oa = (u8)tmin(255.f, a * 0.85f);
        emit(-outline, 0, 8, 10, 12, oa);
        emit(outline, 0, 8, 10, 12, oa);
        emit(0, -outline, 8, 10, 12, oa);
        emit(0, outline, 8, 10, 12, oa);
    }
    emit(0, 0, r, g, b, a);
}

// hazard-stripe filled rect (industrial identity): diagonal stripes
inline void uiHazard(IBackend* rb, f32 x, f32 y, f32 w, f32 h, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2, f32 stripe = 14.f) {
    rb->pushRect(x, y, w, h, r2, g2, b2, 255);
    int n = (int)(w + h) / (int)stripe + 1;
    // draw stripes as rotated quads is unsupported — approximate with slanted stacks of small rects
    for (int i = -((int)(h / stripe)); i < (int)(w / stripe) + 1; i++) {
        f32 sx = x + i * stripe;
        // one column-strip per stripe (leaning via per-row offset)
        int steps = (int)(h / (stripe * 0.5f)) + 1;
        for (int srow = 0; srow < steps; srow++) {
            f32 sy = y + srow * (stripe * 0.5f);
            f32 off = ((f32)steps - srow) * stripe * 0.5f;
            f32 rx = sx + off;
            if (rx + stripe * 0.5f < x || rx > x + w) continue;
            f32 cw = tmin(x + w, rx + stripe * 0.5f) - tmax(x, rx);
            if (cw <= 0) continue;
            rb->pushRect(tmax(x, rx), sy, cw, tmin(y + h, sy + stripe * 0.5f) - sy, r1, g1, b1, 255);
        }
        (void)n;
    }
}

} // namespace ft
