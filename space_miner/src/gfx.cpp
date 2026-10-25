// ============================================================
// SPACE MINER  -  gfx.cpp
// ============================================================
#include "gfx.h"
#include <chrono>

namespace sm {

double gProfSections[8] = {0};
const char* gProfNames[8] = {"asteroids", "station", "loot", "drones", "enemies", "player", "shots", "particles"};
static double profNow() {
    return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count() / 1e6;
}
static double profLast = 0;
static int profSection = -1;
void profMark(int i) {
    double now = profNow();
    if (profSection >= 0 && profSection < 8) gProfSections[profSection] += now - profLast;
    profLast = now;
    profSection = i;
}
void profFlush() {
    double now = profNow();
    if (profSection >= 0 && profSection < 8) gProfSections[profSection] += now - profLast;
    profSection = -1;
}

void Canvas::clear(Color c) { std::fill(px.begin(), px.end(), c); }

void Canvas::blend(int x, int y, Color c) {
    if (!inside(x, y)) return;
    int a = ca(c);
    if (a == 0) return;
    Color& d = px[(size_t)y * w + x];
    if (a == 255) { int da = ca(d); d = c | ((Color)da << 24); return; }
    int ia = 255 - a;
    uint32_t r = ((uint32_t)cr(c) * a + (uint32_t)cr(d) * ia + 127) / 255;
    uint32_t g = ((uint32_t)cg(c) * a + (uint32_t)cg(d) * ia + 127) / 255;
    uint32_t b = ((uint32_t)cb(c) * a + (uint32_t)cb(d) * ia + 127) / 255;
    d = rgba((int)r, (int)g, (int)b, ca(d));
}

void Canvas::additive(int x, int y, Color c) {
    if (!inside(x, y)) return;
    int a = ca(c);
    if (a == 0) return;
    Color& d = px[(size_t)y * w + x];
    int r = cr(d) + cr(c) * a / 255;
    int g = cg(d) + cg(c) * a / 255;
    int b = cb(d) + cb(c) * a / 255;
    int da = ca(d) + a;   // glow layers accumulate coverage
    d = rgba(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, da > 255 ? 255 : da);
}

void Canvas::screen(int x, int y, Color c) {
    if (!inside(x, y)) return;
    Color& d = px[(size_t)y * w + x];
    int r = 255 - (255 - cr(d)) * (255 - cr(c)) / 255;
    int g = 255 - (255 - cg(d)) * (255 - cg(c)) / 255;
    int b = 255 - (255 - cb(d)) * (255 - cb(c)) / 255;
    d = rgba(r, g, b, std::max(ca(d), ca(c)));
}

void Canvas::fillRect(int x, int y, int W, int H, Color c) {
    clipRect(x, y, W, H);
    if (W == 0 || H == 0) return;
    if (ca(c) == 255 && x >= 0 && y >= 0 && x + W <= w && y + H <= h) {
        Color* row = &px[(size_t)y * w + x];
        for (int j = 0; j < H; j++) {
            std::fill(row, row + W, c | 0xFF000000u);
            row += w;
        }
        return;
    }
    if (ca(c) == 255) {
        for (int j = 0; j < H; j++)
            for (int i = 0; i < W; i++) put(x + i, y + j, c);
        return;
    }
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) blend(x + i, y + j, c);
}

void Canvas::fillRectF(float fx, float fy, float fW, float fH, Color c) {
    int x = (int)floorf(fx + .5f), y = (int)floorf(fy + .5f);
    int W = (int)ceilf(fW), H = (int)ceilf(fH);
    fillRect(x, y, W, H, c);
}

void Canvas::rectOutline(float fx, float fy, float fW, float fH, float t, Color c) {
    line({fx, fy}, {fx + fW, fy}, c, t);
    line({fx + fW, fy}, {fx + fW, fy + fH}, c, t);
    line({fx + fW, fy + fH}, {fx, fy + fH}, c, t);
    line({fx, fy + fH}, {fx, fy}, c, t);
}

void Canvas::line(Vec2 a, Vec2 b, Color c, float thickness) {
    float halfT = std::max(0.5f, thickness * .5f);
    int x0 = (int)floorf(std::min(a.x, b.x) - halfT - 1), x1 = (int)ceilf(std::max(a.x, b.x) + halfT + 1);
    int y0 = (int)floorf(std::min(a.y, b.y) - halfT - 1), y1 = (int)ceilf(std::max(a.y, b.y) + halfT + 1);
    if (x1 < 0 || y1 < 0 || x0 >= w || y0 >= h) return;
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, w - 1); y1 = std::min(y1, h - 1);
    Vec2 d = b - a;
    float len2 = lengthSq(d);
    float baseA = ca(c) / 255.f;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            Vec2 p(x + .5f, y + .5f);
            float t = len2 > 1e-6f ? clampf(sm::dot(p - a, d) / len2, 0.f, 1.f) : 0.f;
            Vec2 q = a + d * t;
            float dist = length(p - q);
            float cov = saturate(halfT + .5f - dist);
            if (cov > 0) blend(x, y, withAlpha(c, (int)(baseA * cov * 255)));
        }
}

void Canvas::glowLine(Vec2 a, Vec2 b, Color core, float thickness, float glowRadius, float glowInt) {
    if (glowRadius > 1.f) {
        int passes = 3;
        for (int i = passes; i >= 1; i--) {
            float r = glowRadius * i / passes;
            int alpha = (int)(ca(core) * glowInt * (0.13f / i));
            line(a, b, withAlpha(core, alpha), thickness + r * 2.f);
        }
    }
    line(a, b, core, thickness);
    line(a, b, withAlpha(255 | (255 << 8) | (255 << 16), (int)(ca(core) * .7f)), std::max(.7f, thickness * .35f));
}

void Canvas::circleFill(float cx, float cy, float r, Color c) {
    if (r <= 0) return;
    int x0 = (int)floorf(cx - r - 1), x1 = (int)ceilf(cx + r + 1);
    int y0 = (int)floorf(cy - r - 1), y1 = (int)ceilf(cy + r + 1);
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, w - 1); y1 = std::min(y1, h - 1);
    float baseA = ca(c) / 255.f;
    float inner = r - .5f, outer = r + .5f;
    float in2 = inner > 0 ? inner * inner : -1.f, out2 = outer * outer;
    for (int y = y0; y <= y1; y++) {
        float dy = y + .5f - cy;
        float dy2 = dy * dy;
        for (int x = x0; x <= x1; x++) {
            float dx = x + .5f - cx;
            float d2 = dx * dx + dy2;
            if (d2 >= out2) continue;
            float cov = d2 <= in2 ? 1.f : (outer - sqrtf(d2));
            if (cov > 0) blend(x, y, withAlpha(c, (int)(baseA * (cov > 1.f ? 1.f : cov) * 255)));
        }
    }
}

void Canvas::ring(float cx, float cy, float r, float thickness, Color c) {
    float halfT = std::max(.5f, thickness * .5f);
    if (r < halfT) { circleFill(cx, cy, r + halfT, c); return; }
    int x0 = (int)floorf(cx - r - halfT - 1), x1 = (int)ceilf(cx + r + halfT + 1);
    int y0 = (int)floorf(cy - r - halfT - 1), y1 = (int)ceilf(cy + r + halfT + 1);
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, w - 1); y1 = std::min(y1, h - 1);
    float baseA = ca(c) / 255.f;
    float ri = r - halfT - .5f, ro = r + halfT + .5f;
    float ri2 = ri > 0 ? ri * ri : 0.f, ro2 = ro * ro;
    for (int y = y0; y <= y1; y++) {
        float dy = y + .5f - cy;
        float dy2 = dy * dy;
        if (dy2 > ro2) continue;
        for (int x = x0; x <= x1; x++) {
            float dx = x + .5f - cx;
            float d2 = dx * dx + dy2;
            if (d2 > ro2 || d2 < ri2) continue;
            float cov = saturate(halfT + .5f - fabsf(sqrtf(d2) - r));
            if (cov > 0) blend(x, y, withAlpha(c, (int)(baseA * cov * 255)));
        }
    }
}

void Canvas::arc(float cx, float cy, float r, float a0, float a1, float thickness, Color c) {
    int steps = std::max(6, (int)(fabsf(a1 - a0) * r * .35f) + 6);
    float halfT = std::max(.5f, thickness * .5f);
    Vec2 prev(cx + cosf(a0) * r, cy + sinf(a0) * r);
    for (int i = 1; i <= steps; i++) {
        float a = a0 + (a1 - a0) * i / steps;
        Vec2 p(cx + cosf(a) * r, cy + sinf(a) * r);
        line(prev, p, c, halfT * 2.f);
        prev = p;
    }
}

void Canvas::radialGlow(float cx, float cy, float r, Color c, float intensity) {
    if (r <= 1) return;
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r);
    int y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, w - 1); y1 = std::min(y1, h - 1);
    float inv = 1.f / r;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = (x + .5f - cx) * inv, dy = (y + .5f - cy) * inv;
            float d = sqrtf(dx * dx + dy * dy);
            if (d >= 1.f) continue;
            float f = (1.f - d); f = f * f;
            int a = (int)(f * intensity * ca(c));
            if (a > 0) additive(x, y, withAlpha(c, a));
        }
}

void Canvas::ellipseGlow(float cx, float cy, float rx, float ry, Color c, float intensity, float angle) {
    float ca_ = cosf(-angle), sa_ = sinf(-angle);
    int x0 = (int)floorf(cx - rx - 1), x1 = (int)ceilf(cx + rx + 1);
    int y0 = (int)floorf(cy - ry - 1), y1 = (int)ceilf(cy + ry + 1);
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, w - 1); y1 = std::min(y1, h - 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = x + .5f - cx, dy = y + .5f - cy;
            float lx = dx * ca_ - dy * sa_, ly = dx * sa_ + dy * ca_;
            float d = sqrtf((lx * lx) / (rx * rx) + (ly * ly) / (ry * ry));
            if (d >= 1.f) continue;
            float f = (1.f - d); f = f * f;
            int a = (int)(f * intensity * ca(c));
            if (a > 0) additive(x, y, withAlpha(c, a));
        }
}

void Canvas::polyFill(const Vec2* pts, int n, Color c) {
    if (n < 3) return;
    float minx = pts[0].x, maxx = pts[0].x, miny = pts[0].y, maxy = pts[0].y;
    for (int i = 1; i < n; i++) {
        minx = std::min(minx, pts[i].x); maxx = std::max(maxx, pts[i].x);
        miny = std::min(miny, pts[i].y); maxy = std::max(maxy, pts[i].y);
    }
    int x0 = std::max(0, (int)floorf(minx)), x1 = std::min(w - 1, (int)ceilf(maxx));
    int y0 = std::max(0, (int)floorf(miny)), y1 = std::min(h - 1, (int)ceilf(maxy));
    float baseA = ca(c) / 255.f;
    for (int y = y0; y <= y1; y++) {
        float sy = y + .5f;
        float xs[32];
        int cnt = 0;
        for (int i = 0; i < n && cnt < 31; i++) {
            Vec2 a = pts[i], b = pts[(i + 1) % n];
            if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) {
                float t = (sy - a.y) / (b.y - a.y);
                xs[cnt++] = a.x + (b.x - a.x) * t;
            }
        }
        std::sort(xs, xs + cnt);
        for (int i = 0; i + 1 < cnt; i += 2) {
            float xa = xs[i], xb = xs[i + 1];
            int ix0 = std::max(x0, (int)floorf(xa));
            int ix1 = std::min(x1, (int)ceilf(xb));
            for (int x = ix0; x <= ix1; x++) {
                float covA = saturate(x + 1.f - xa);
                float covB = saturate(xb - x);
                float cov = std::min(covA, covB);
                if (cov > 0) blend(x, y, withAlpha(c, (int)(baseA * cov * 255)));
            }
        }
    }
}

void Canvas::polyOutline(const Vec2* pts, int n, Color c, float thickness) {
    for (int i = 0; i < n; i++) line(pts[i], pts[(i + 1) % n], c, thickness);
}

void Canvas::disc(float x, float y, float r, Color c) { circleFill(x, y, r, c); }

void Canvas::blit(const Canvas& src, int x, int y, float alpha) {
    for (int j = 0; j < src.h; j++) {
        int ty = y + j;
        if ((unsigned)ty >= (unsigned)h) continue;
        for (int i = 0; i < src.w; i++) {
            int tx = x + i;
            if ((unsigned)tx >= (unsigned)w) continue;
            Color s = src.px[(size_t)j * src.w + i];
            int a = (int)(ca(s) * alpha);
            if (a <= 0) continue;
            blend(tx, ty, withAlpha(s, a));
        }
    }
}

void Canvas::blitAdd(const Canvas& src, int x, int y, float alpha) {
    for (int j = 0; j < src.h; j++) {
        int ty = y + j;
        if ((unsigned)ty >= (unsigned)h) continue;
        for (int i = 0; i < src.w; i++) {
            int tx = x + i;
            if ((unsigned)tx >= (unsigned)w) continue;
            Color s = src.px[(size_t)j * src.w + i];
            int a = (int)(ca(s) * alpha);
            if (a <= 0) continue;
            additive(tx, ty, withAlpha(s, a));
        }
    }
}

void Canvas::blitRotated(const Canvas& src, Vec2 centre, float angle, float scale, float alpha) {
    if (scale <= 0.001f || src.w <= 0 || src.h <= 0 || alpha <= .001f) return;
    // world -> sprite transform
    float cs = cosf(-angle) / scale, sn = sinf(-angle) / scale;
    float scx = src.w * .5f, scy = src.h * .5f;
    float ext = (sqrtf((float)(src.w * src.w + src.h * src.h)) * .5f + 1.f) * scale;
    int x0 = std::max(0, (int)floorf(centre.x - ext)), x1 = std::min(w - 1, (int)ceilf(centre.x + ext));
    int y0 = std::max(0, (int)floorf(centre.y - ext)), y1 = std::min(h - 1, (int)ceilf(centre.y + ext));
    if (x0 > x1 || y0 > y1) return;
    const int sw = src.w, sh = src.h;
    const Color* sdat = src.px.data();
    const int amul = (int)(alpha * 256.f + .5f);
    for (int y = y0; y <= y1; y++) {
        float dy = y + .5f - centre.y;
        // u and v as functions of x: u = ux0 + cs*x, v = vx0 + sn*x  (direction along the row)
        float ux0 = (cs * (x0 + .5f - centre.x) - sn * dy) + scx;
        float vx0 = (sn * (x0 + .5f - centre.x) + cs * dy) + scy;
        // clip the row to the sprite rectangle
        int rx0 = x0, rx1 = x1;
        auto clipAxis = [&](float start, float step, int limit, int& a, int& b) {
            if (fabsf(step) < 1e-9f) {
                if (start < 0.f || start >= limit) { b = a - 1; }
                return;
            }
            float t0 = (0.f - start) / step + x0;          // x where value == 0
            float t1 = (limit - start) / step + x0;        // x where value == limit
            float lo = std::min(t0, t1), hi = std::max(t0, t1);
            int ia = (int)ceilf(lo), ib = (int)floorf(hi);
            a = std::max(a, ia);
            b = std::min(b, ib);
        };
        clipAxis(ux0, cs, sw, rx0, rx1);
        clipAxis(vx0, sn, sh, rx0, rx1);
        if (rx0 > rx1) continue;
        float u = ux0 + cs * (rx0 - x0);
        float v = vx0 + sn * (rx0 - x0);
        Color* dstRow = &px[(size_t)y * w];
        for (int x = rx0; x <= rx1; x++, u += cs, v += sn) {
            int iu = (int)u, iv = (int)v;
            if ((unsigned)iu >= (unsigned)(sw - 1) || (unsigned)iv >= (unsigned)(sh - 1)) {
                // edge pixels: clamp to nearest sample
                if (iu < 0 || iv < 0 || iu >= sw || iv >= sh) continue;
                Color c = sdat[(size_t)iv * sw + iu];
                int a = (ca(c) * amul) >> 8;
                if (a > 0) blend(x, y, withAlpha(c, a));
                continue;
            }
            float fu = u - iu, fv = v - iv;
            int wu = (int)(fu * 256.f), wv = (int)(fv * 256.f);
            const Color* p0 = sdat + (size_t)iv * sw + iu;
            const Color* p1 = p0 + sw;
            Color c00 = p0[0], c10 = p0[1], c01 = p1[0], c11 = p1[1];
            int w00 = ((256 - wu) * (256 - wv)) >> 8;
            int w10 = (wu * (256 - wv)) >> 8;
            int w01 = ((256 - wu) * wv) >> 8;
            int w11 = (wu * wv) >> 8;
            // w1 = R (bits 16-23) | B (bits 0-7);  w2 = A (bits 16-23) | G (bits 0-7)
            uint32_t w1 = (((c00 & 0x00FF00FFu) * (uint32_t)w00) + ((c10 & 0x00FF00FFu) * (uint32_t)w10) +
                           ((c01 & 0x00FF00FFu) * (uint32_t)w01) + ((c11 & 0x00FF00FFu) * (uint32_t)w11)) >> 8;
            uint32_t w2 = ((((c00 >> 8) & 0x00FF00FFu) * (uint32_t)w00) + (((c10 >> 8) & 0x00FF00FFu) * (uint32_t)w10) +
                           (((c01 >> 8) & 0x00FF00FFu) * (uint32_t)w01) + (((c11 >> 8) & 0x00FF00FFu) * (uint32_t)w11)) >> 8;
            int a = (int)((((w2 >> 16) & 0xFF) * (uint32_t)amul) >> 8);
            if (a <= 0) continue;
            Color& d = dstRow[x];
            if (a >= 254) {
                d = ((w1 & 0x00FF00FFu) | ((w2 & 0xFFu) << 8) | 0xFF000000u);
                d &= 0x00FFFFFFu;
                d |= ((Color)ca(dstRow[x]) << 24) & 0xFF000000u;
            } else {
                uint32_t ia = 255u - (uint32_t)a;
                uint32_t drb = d & 0x00FF00FFu;
                uint32_t dg = (d >> 8) & 0xFFu;
                drb = ((w1 & 0x00FF00FFu) * (uint32_t)a + drb * ia) >> 8;
                uint32_t g = (((w2 & 0xFFu) * (uint32_t)a + dg * ia) >> 8) & 0xFFu;
                d = (drb & 0x00FF00FFu) | (g << 8) | ((Color)ca(d) << 24);
            }
        }
    }
}

void Canvas::blitRotatedAdd(const Canvas& src, Vec2 centre, float angle, float scale, float alpha) {
    if (scale <= 0.001f || src.w <= 0 || src.h <= 0 || alpha <= .001f) return;
    float cs = cosf(-angle) / scale, sn = sinf(-angle) / scale;
    float scx = src.w * .5f, scy = src.h * .5f;
    float ext = (sqrtf((float)(src.w * src.w + src.h * src.h)) * .5f + 1.f) * scale;
    int x0 = std::max(0, (int)floorf(centre.x - ext)), x1 = std::min(w - 1, (int)ceilf(centre.x + ext));
    int y0 = std::max(0, (int)floorf(centre.y - ext)), y1 = std::min(h - 1, (int)ceilf(centre.y + ext));
    if (x0 > x1 || y0 > y1) return;
    const int sw = src.w, sh = src.h;
    const Color* sdat = src.px.data();
    const int amul = (int)(alpha * 256.f + .5f);
    for (int y = y0; y <= y1; y++) {
        float dy = y + .5f - centre.y;
        float ux0 = (cs * (x0 + .5f - centre.x) - sn * dy) + scx;
        float vx0 = (sn * (x0 + .5f - centre.x) + cs * dy) + scy;
        int rx0 = x0, rx1 = x1;
        auto clipAxis = [&](float start, float step, int limit, int& a, int& b) {
            if (fabsf(step) < 1e-9f) {
                if (start < 0.f || start >= limit) b = a - 1;
                return;
            }
            float t0 = (0.f - start) / step + x0, t1 = (limit - start) / step + x0;
            int ia = (int)ceilf(std::min(t0, t1)), ib = (int)floorf(std::max(t0, t1));
            a = std::max(a, ia);
            b = std::min(b, ib);
        };
        clipAxis(ux0, cs, sw, rx0, rx1);
        clipAxis(vx0, sn, sh, rx0, rx1);
        if (rx0 > rx1) continue;
        float u = ux0 + cs * (rx0 - x0);
        float v = vx0 + sn * (rx0 - x0);
        Color* dstRow = &px[(size_t)y * w];
        for (int x = rx0; x <= rx1; x++, u += cs, v += sn) {
            int iu = (int)u, iv = (int)v;
            if ((unsigned)iu >= (unsigned)sw || (unsigned)iv >= (unsigned)sh) continue;
            Color sc = sdat[(size_t)iv * sw + iu];
            int a = (ca(sc) * amul) >> 8;
            if (a <= 0) continue;
            Color d = dstRow[x];
            int r = cr(d) + (cr(sc) * a >> 8);
            int g = cg(d) + (cg(sc) * a >> 8);
            int b = cb(d) + (cb(sc) * a >> 8);
            int da = ca(d) + a;
            dstRow[x] = rgba(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, da > 255 ? 255 : da);
        }
    }
}

void Canvas::blitScaled(const Canvas& src, int x, int y, int w, int h, float alpha, bool additive) {
    if (w <= 0 || h <= 0 || src.w <= 0 || src.h <= 0) return;
    for (int j = 0; j < h; j++) {
        int ty = y + j;
        if ((unsigned)ty >= (unsigned)this->h) continue;
        int sy = j * src.h / h;
        if ((unsigned)sy >= (unsigned)src.h) sy = src.h - 1;
        for (int i = 0; i < w; i++) {
            int tx = x + i;
            if ((unsigned)tx >= (unsigned)this->w) continue;
            int sx = i * src.w / w;
            if ((unsigned)sx >= (unsigned)src.w) sx = src.w - 1;
            Color s = src.px[(size_t)sy * src.w + sx];
            int a = (int)(ca(s) * alpha);
            if (a <= 0) continue;
            if (additive) this->additive(tx, ty, withAlpha(s, a));
            else blend(tx, ty, withAlpha(s, a));
        }
    }
}

void Canvas::vGradient(int x, int y, int W, int H, Color top, Color bottom) {
    for (int j = 0; j < H; j++) {
        float t = H > 1 ? (float)j / (H - 1) : 0;
        Color c = mixColor(top, bottom, t);
        fillRect(x, y + j, W, 1, c);
    }
}

void Canvas::scanlines(int step, Color c) {
    for (int y = 0; y < h; y += step) fillRect(0, y, w, 1, c);
}

void Canvas::vignetteFast(float strength) {
    static std::vector<uint8_t> lut;
    static int lw = 0, lh = 0;
    static float lastStrength = -1.f;
    if (lw != w || lh != h || lastStrength != strength) {
        lut.resize((size_t)w * h);
        float cx = w * .5f, cy = h * .5f;
        float maxd = sqrtf(cx * cx + cy * cy);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float dx = x + .5f - cx, dy = y + .5f - cy;
                float d = sqrtf(dx * dx + dy * dy) / maxd;
                float f = saturate((d - .35f) / .65f);
                lut[(size_t)y * w + x] = (uint8_t)(f * f * strength * 255.f);
            }
        lw = w; lh = h; lastStrength = strength;
    }
    for (size_t i = 0, n = (size_t)w * h; i < n; i++) {
        int a = lut[i];
        if (!a) continue;
        Color& d = px[i];
        int ia = 255 - a;
        d = rgba(cr(d) * ia / 255, cg(d) * ia / 255, cb(d) * ia / 255, ca(d));
    }
}

void Canvas::vignette(float strength) {
    float cx = w * .5f, cy = h * .5f;
    float maxd = sqrtf(cx * cx + cy * cy);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x + .5f - cx, dy = y + .5f - cy;
            float d = sqrtf(dx * dx + dy * dy) / maxd;
            float f = saturate((d - .35f) / .65f);
            int a = (int)(f * f * strength * 255);
            if (a > 0) blend(x, y, rgba(0, 0, 0, a));
        }
}

void Canvas::downscaleTo(Canvas& dst) const {
    // box filter
    for (int y = 0; y < dst.h; y++) {
        int sy0 = y * h / dst.h, sy1 = std::max(sy0 + 1, (y + 1) * h / dst.h);
        for (int x = 0; x < dst.w; x++) {
            int sx0 = x * w / dst.w, sx1 = std::max(sx0 + 1, (x + 1) * w / dst.w);
            int r = 0, g = 0, b = 0, n = 0;
            for (int j = sy0; j < sy1 && j < h; j++)
                for (int i = sx0; i < sx1 && i < w; i++) {
                    Color c = px[(size_t)j * w + i];
                    r += cr(c); g += cg(c); b += cb(c); n++;
                }
            if (n == 0) n = 1;
            dst.px[(size_t)y * dst.w + x] = rgba(r / n, g / n, b / n, 255);
        }
    }
}

// ================================================================ font
// 3x5 glyphs, rows separated by '/'
static const char* GLYPHS[95] = {
    ".../.../.../.../...",  // space
    "..#/..#/..#/.../..#",  // !
    "#.#/#.#/.../.../...",  // "
    "#.#/###/#.#/###/#.#",  // #
    ".##/#../.##/..#/##.",  // $
    "#.#/..#/.#./#../#.#",  // %
    ".#./#.#/.#./#.#/.##",  // &
    "..#/..#/.../.../...",  // '
    "..#/.#./.#./.#./..#",  // (
    "#../.#./.#./.#./#..",  // )
    "#.#/.#./###/.#./#.#",  // *
    ".../.#./###/.#./...",  // +
    ".../.../.../.#./#..",  // ,
    ".../.../###/.../...",  // -
    ".../.../.../.../..#",  // .
    "..#/..#/.#./#../#..",  // /
    "###/#.#/#.#/#.#/###",  // 0
    ".#./##./.#./.#./###",  // 1
    "##./..#/.#./#../###",  // 2
    "##./..#/.#./..#/##.",  // 3
    "#.#/#.#/###/..#/..#",  // 4
    "###/#../##./..#/##.",  // 5
    ".##/#../###/#.#/###",  // 6
    "###/..#/.#./.#./.#.",  // 7
    "###/#.#/###/#.#/###",  // 8
    "###/#.#/###/..#/##.",  // 9
    ".../..#/.../..#/...",  // :
    ".../..#/.../..#/#..",  // ;
    "..#/.#./#../.#./..#",  // <
    ".../###/.../###/...",  // =
    "#../.#./..#/.#./#..",  // >
    "##./..#/.#./.../.#.",  // ?
    "###/#.#/###/#../###",  // @
    "###/#.#/###/#.#/#.#",  // A
    "##./#.#/##./#.#/##.",  // B
    ".##/#../#../#../.##",  // C
    "##./#.#/#.#/#.#/##.",  // D
    "###/#../##./#../###",  // E
    "###/#../##./#../#..",  // F
    ".##/#../#.#/#.#/.##",  // G
    "#.#/#.#/###/#.#/#.#",  // H
    "###/.#./.#./.#./###",  // I
    "..#/..#/..#/#.#/.#.",  // J
    "#.#/#.#/##./#.#/#.#",  // K
    "#../#../#../#../###",  // L
    "#.#/###/###/#.#/#.#",  // M
    "#.#/###/###/###/#.#",  // N
    "###/#.#/#.#/#.#/###",  // O
    "###/#.#/###/#../#..",  // P
    "###/#.#/#.#/###/..#",  // Q
    "###/#.#/##./#.#/#.#",  // R
    ".##/#../.#./..#/##.",  // S
    "###/.#./.#./.#./.#.",  // T
    "#.#/#.#/#.#/#.#/###",  // U
    "#.#/#.#/#.#/#.#/.#.",  // V
    "#.#/#.#/###/###/#.#",  // W
    "#.#/#.#/.#./#.#/#.#",  // X
    "#.#/#.#/.#./.#./.#.",  // Y
    "###/..#/.#./#../###",  // Z
    ".##/.#./.#./.#./.##",  // [
    "#../#../.#./..#/..#",  // backslash
    "##./.#./.#./.#./##.",  // ]
    ".#./#.#/.../.../...",  // ^
    ".../.../.../.../###",  // _
    "#../.#./.../.../...",  // `
    "###/#.#/###/#.#/#.#",  // a
    "##./#.#/##./#.#/##.",  // b
    ".##/#../#../#../.##",  // c
    "##./#.#/#.#/#.#/##.",  // d
    "###/#../##./#../###",  // e
    "###/#../##./#../#..",  // f
    ".##/#../#.#/#.#/.##",  // g
    "#.#/#.#/###/#.#/#.#",  // h
    "###/.#./.#./.#./###",  // i
    "..#/..#/..#/#.#/.#.",  // j
    "#.#/#.#/##./#.#/#.#",  // k
    "#../#../#../#../###",  // l
    "#.#/###/###/#.#/#.#",  // m
    "#.#/###/###/###/#.#",  // n
    "###/#.#/#.#/#.#/###",  // o
    "###/#.#/###/#../#..",  // p
    "###/#.#/#.#/###/..#",  // q
    "###/#.#/##./#.#/#.#",  // r
    ".##/#../.#./..#/##.",  // s
    "###/.#./.#./.#./.#.",  // t
    "#.#/#.#/#.#/#.#/###",  // u
    "#.#/#.#/#.#/#.#/.#.",  // v
    "#.#/#.#/###/###/#.#",  // w
    "#.#/#.#/.#./#.#/#.#",  // x
    "#.#/#.#/.#./.#./.#.",  // y
    "###/..#/.#./#../###",  // z
    "..#/.#./#../.#./..#",  // {
    "..#/..#/..#/..#/..#",  // |
    "#../.#./..#/.#./#..",  // }
    ".../.#./#.#/.../...",  // ~
};

static inline bool glyphBit(int gi, int row, int col) {
    const char* g = GLYPHS[gi];
    int r = 0, c = 0;
    for (const char* p = g; *p; p++) {
        if (*p == '/') { r++; c = 0; continue; }
        if (r == row && c == col) return *p == '#';
        c++;
    }
    return false;
}

int textWidth(const std::string& s, int scale, int spacing) {
    int n = 0, best = 0;
    for (char ch : s) {
        if (ch == '\n') { best = std::max(best, n); n = 0; continue; }
        n++;
    }
    if (n == 0) n = 1;
    best = std::max(best, n);
    int adv = (3 + spacing) * scale;
    // don't count trailing gap
    return best * adv - spacing * scale;
}

int textHeight(int scale) { return 5 * scale; }

void drawText(Canvas& c, int x, int y, const std::string& s, Color col, int scale, int spacing) {
    if (scale < 1) scale = 1;
    int adv = (3 + spacing) * scale;
    int ox = x, oy = y;
    int alpha = ca(col);
    for (char ch : s) {
        if (ch == '\n') { ox = x; oy += (5 + 2) * scale; continue; }
        int gi = (int)(unsigned char)ch - 32;
        if (gi < 0 || gi > 94) gi = 63;  // '?'
        if (ch != ' ') {
            for (int r = 0; r < 5; r++)
                for (int q = 0; q < 3; q++)
                    if (glyphBit(gi, r, q)) c.fillRect(ox + q * scale, oy + r * scale, scale, scale, col);
        }
        ox += adv;
    }
    (void)alpha;
}

void drawTextShadow(Canvas& c, int x, int y, const std::string& s, Color col, Color shadow,
                    int scale, int spacing, int off) {
    drawText(c, x + off, y + off, s, shadow, scale, spacing);
    drawText(c, x, y, s, col, scale, spacing);
}

void drawTextCenter(Canvas& c, int cx, int y, const std::string& s, Color col, int scale, int spacing) {
    drawText(c, cx - textWidth(s, scale, spacing) / 2, y, s, col, scale, spacing);
}

void drawTextRight(Canvas& c, int rx, int y, const std::string& s, Color col, int scale, int spacing) {
    drawText(c, rx - textWidth(s, scale, spacing), y, s, col, scale, spacing);
}

void drawTextGlow(Canvas& c, int x, int y, const std::string& s, Color col, Color glow,
                  int scale, int spacing) {
    // cheap glow: draw offset copies with low alpha
    int a = ca(glow);
    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};
    for (int i = 0; i < 4; i++)
        drawText(c, x + dx[i] * scale, y + dy[i] * scale, s, withAlpha(glow, a / 3), scale, spacing);
    drawText(c, x, y, s, col, scale, spacing);
}

void drawHeading(Canvas& c, int cx, int y, const std::string& s, Color col, Color glow, int scale, int spacingPx) {
    int total = 0;
    int adv = (3 * scale) + spacingPx;
    total = (int)s.size() * adv - (s.size() ? spacingPx : 0);
    int x = cx - total / 2;
    for (size_t i = 0; i < s.size(); i++) {
        std::string one(1, s[i]);
        if (s[i] != ' ') {
            // glow halo
            for (int k = 0; k < 6; k++) {
                float ang = k * TAU / 6.f;
                drawText(c, x + (int)(cosf(ang) * 2), y + (int)(sinf(ang) * 2), one, withAlpha(glow, 40), scale, 1);
            }
            drawText(c, x, y, one, col, scale, 1);
        }
        x += adv;
    }
}

}  // namespace sm
