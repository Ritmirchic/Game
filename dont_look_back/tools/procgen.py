#!/usr/bin/env python3
"""
procgen.py -- procedural texture library for DON'T LOOK BACK.
Writes PNGs (RGBA) used by the engine's material set. Everything is generated:
no external image assets are used.
"""
import os, sys, math, zlib, struct, random
from fontgen import TTF

TEX = 256

# ----------------------------------------------------------------- helpers
def h32(x):
    x &= 0xFFFFFFFF
    x ^= x >> 16; x = (x * 0x7feb352d) & 0xFFFFFFFF
    x ^= x >> 15; x = (x * 0x846ca68b) & 0xFFFFFFFF
    x ^= x >> 16
    return x

def hf(x, seed=0):
    return h32(x * 2654435761 + seed * 668265263) / 4294967296.0

class Noise:
    def __init__(self, seed=1):
        self.seed = seed
    def v2(self, x, y, period=0):
        xi, yi = math.floor(x), math.floor(y)
        xf, yf = x - xi, y - yi
        def h(a, b):
            if period:
                a %= period; b %= period
            return hf((a & 0xFFFF) * 7919 + (b & 0xFFFF) * 104729, self.seed) * 2 - 1
        u = xf * xf * (3 - 2 * xf)
        v = yf * yf * (3 - 2 * yf)
        a = h(xi, yi); b = h(xi + 1, yi); c = h(xi, yi + 1); d = h(xi + 1, yi + 1)
        return (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v
    def fbm(self, x, y, oct=4, period=0, lac=2.0, gain=0.5):
        s = 0; amp = 1; norm = 0; p = period
        for i in range(oct):
            s += self.v2(x, y, p) * amp
            norm += amp
            x *= lac; y *= lac; amp *= gain
            p = p * 2 if p else 0
        return s / norm
    def ridge(self, x, y, oct=4):
        s = 0; amp = 1; norm = 0
        for i in range(oct):
            n = 1 - abs(self.v2(x, y))
            s += n * n * amp
            norm += amp
            x *= 2; y *= 2; amp *= 0.5
        return s / norm
    def worley(self, x, y, period=0):
        xi, yi = math.floor(x), math.floor(y)
        best = 9.0; bid = 0.0
        for j in (-1, 0, 1):
            for i in (-1, 0, 1):
                cx, cy = xi + i, yi + j
                px_, py_ = cx, cy
                if period:
                    px_ %= period; py_ %= period
                hx = hf(px_ * 3571 + py_ * 7919 * 3, self.seed + 11)
                hy = hf(px_ * 1279 + py_ * 6151 * 5, self.seed + 29)
                fx = cx + hx; fy = cy + hy
                d = (fx - x) ** 2 + (fy - y) ** 2
                if d < best:
                    best = d; bid = hx
        return math.sqrt(best), bid

def cl(v, a=0.0, b=1.0):
    return a if v < a else (b if v > b else v)

def mix(c1, c2, t):
    t = cl(t)
    return tuple(c1[i] + (c2[i] - c1[i]) * t for i in range(3))

def mul(c, f):
    return tuple(c[i] * f for i in range(3))

class Img:
    def __init__(self, w=TEX, h=TEX, color=(0, 0, 0, 255)):
        self.w, self.h = w, h
        c4 = [color[0], color[1], color[2], (color[3] if len(color) > 3 else 1)]
        self.p = [c4[:] for _ in range(w * h)]
    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y * self.w + x] = [cl(c[0], 0, 1) * 255, cl(c[1], 0, 1) * 255, cl(c[2], 0, 1) * 255,
                                      (c[3] if len(c) > 3 else 1) * 255]
    def get(self, x, y):
        x %= self.w; y %= self.h
        return self.p[y * self.w + x]
    def blend(self, x, y, c, a=1.0):
        if not (0 <= x < self.w and 0 <= y < self.h):
            return
        d = self.p[y * self.w + x]
        ca = (c[3] if len(c) > 3 else 1) * a
        for i in range(3):
            d[i] = d[i] * (1 - ca) + c[i] * 255 * ca
        if len(c) > 3:
            d[3] = min(1.0, d[3] + ca)
    def rect(self, x0, y0, x1, y1, c, a=1.0):
        for y in range(int(y0), int(y1)):
            for x in range(int(x0), int(x1)):
                self.blend(x, y, c, a)
    def line(self, x0, y0, x1, y1, c, th=1.0, a=1.0):
        dx = x1 - x0; dy = y1 - y0
        L = max(1e-6, math.hypot(dx, dy))
        n = int(L * 2)
        for i in range(n + 1):
            t = i / n
            px, py = x0 + dx * t, y0 + dy * t
            r = th * 0.5
            for yy in range(int(py - r) - 1, int(py + r) + 2):
                for xx in range(int(px - r) - 1, int(px + r) + 2):
                    if (xx + .5 - px) ** 2 + (yy + .5 - py) ** 2 <= r * r:
                        self.blend(xx, yy, c, a)
    def circle(self, cx, cy, r, c, a=1.0, ring=None):
        for y in range(int(cy - r - 1), int(cy + r + 2)):
            for x in range(int(cx - r - 1), int(cx + r + 2)):
                d = math.hypot(x + .5 - cx, y + .5 - cy)
                if ring is not None:
                    if abs(d - r) <= ring:
                        self.blend(x, y, c, a)
                elif d <= r:
                    self.blend(x, y, c, a)
    def save(self, path):
        raw = bytearray()
        for y in range(self.h):
            raw.append(0)
            for x in range(self.w):
                c = self.p[y * self.w + x]
                raw.append(int(cl(c[0], 0, 255)))
                raw.append(int(cl(c[1], 0, 255)))
                raw.append(int(cl(c[2], 0, 255)))
                raw.append(int(cl(c[3], 0, 1) * 255))
        comp = zlib.compress(bytes(raw), 9)
        def chunk(t, d):
            return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', self.w, self.h, 8, 6, 0, 0, 0))
        png += chunk(b'IDAT', comp) + chunk(b'IEND', b'')
        open(path, 'wb').write(png)

FONT = None
def font():
    global FONT
    if FONT is None:
        FONT = TTF(open('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 'rb').read())
        FONT2 = TTF(open('/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf', 'rb').read())
    return FONT

def draw_text(img, text, x, y, size, color, ttf='/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', a=1.0):
    f = TTF(open(ttf, 'rb').read())
    scale = size / f.unitsPerEm
    cx = x
    for ch in text:
        gi = f.glyphIndex(ord(ch))
        contours, adv = f.outline(gi)
        if contours:
            xs = [p[0] for c in contours for p in c]
            ys = [p[1] for c in contours for p in c]
            bx0, by1 = min(xs), max(ys)
            # rasterize glyph into a temp coverage
            W = int((max(xs) - bx0) * scale) + 2
            H = int((by1 - min(ys)) * scale) + 2
            if W > 0 and H > 0:
                cov = [[0] * max(W, 1) for _ in range(max(H, 1))]
                edges = []
                for c in contours:
                    n = len(c)
                    for i in range(n):
                        x0, y0 = c[i]; x1, y1 = c[(i + 1) % n]
                        px0 = (x0 - bx0) * scale + 1; py0 = (by1 - y0) * scale + 1
                        px1 = (x1 - bx0) * scale + 1; py1 = (by1 - y1) * scale + 1
                        if py0 == py1:
                            continue
                        wd = 1 if py1 > py0 else -1
                        if py0 > py1:
                            px0, py0, px1, py1 = px1, py1, px0, py0
                        edges.append((py0, py1, px0, px1, wd))
                for sy in range(H):
                    py = sy + 0.5
                    xs2 = sorted([(x0 + (x1 - x0) * (py - y0) / (y1 - y0), wd) for (y0, y1, x0, x1, wd) in edges if y0 <= py < y1])
                    wind = 0
                    for k in range(len(xs2) - 1):
                        wind += xs2[k][1]
                        if wind:
                            for xx in range(max(0, int(xs2[k][0])), min(W, int(xs2[k + 1][0]) + 1)):
                                cov[sy][xx] = 1
                for yy in range(H):
                    for xx in range(W):
                        if cov[yy][xx]:
                            img.blend(int(cx + xx), int(y + yy), color, a)
        cx += adv * scale

# ----------------------------------------------------------------- materials
def base_noise(img, n, scale, col_lo, col_hi, oct=5, seed=1):
    for y in range(img.h):
        for x in range(img.w):
            v = 0.5 + 0.5 * n.fbm(x / img.w * scale, y / img.h * scale, oct)
            img.set(x, y, mix(col_lo, col_hi, v))

def grain(img, amount, seed=3):
    for y in range(img.h):
        for x in range(img.w):
            c = img.p[y * img.w + x]
            g = (hf(x * 1973 + y * 9277 + seed * 131, seed) - 0.5) * amount * 255
            c[0] = cl(c[0] + g, 0, 255); c[1] = cl(c[1] + g, 0, 255); c[2] = cl(c[2] + g, 0, 255)

def stains(img, n, count=6, col=(0.22, 0.18, 0.12), alpha=0.35, seed=5, rad=(12, 55)):
    rnd = random.Random(seed)
    for i in range(count):
        cx = rnd.randint(0, img.w); cy = rnd.randint(0, img.h)
        r = rnd.randint(*rad)
        for y in range(cy - r, cy + r):
            for x in range(cx - r, cx + r):
                d = math.hypot(x - cx, y - cy) / r
                if d > 1:
                    continue
                e = (1 - d) ** 1.7
                nz = 0.5 + 0.5 * n.fbm(x / 18.0, y / 18.0, 3)
                img.blend(x % img.w, y % img.h, col, alpha * e * nz)

def cracks(img, n, count=8, seed=9, col=(0.08, 0.08, 0.08), alpha=0.5):
    rnd = random.Random(seed)
    for i in range(count):
        x = rnd.uniform(0, img.w); y = rnd.uniform(0, img.h)
        ang = rnd.uniform(0, 6.28)
        L = rnd.uniform(20, 90)
        steps = int(L / 2)
        for s in range(steps):
            ang += rnd.uniform(-0.3, 0.3)
            nx = x + math.cos(ang) * 2; ny = y + math.sin(ang) * 2
            img.blend(int(x) % img.w, int(y) % img.h, col, alpha * (1 - s / steps) ** 0.5)
            x, y = nx, ny

# ---- walls
def tex_wallpaper_stripe(out):
    n = Noise(21)
    img = Img()
    for y in range(256):
        for x in range(256):
            st = (x % 32) / 32.0
            base = (0.60, 0.57, 0.46)
            if st < 0.40:
                base = (0.40, 0.46, 0.38)
            v = 0.5 + 0.5 * n.fbm(x / 40.0, y / 40.0, 4)
            c = mix(mul(base, 0.82), mul(base, 1.06), v)
            # damask-ish motif in stripe centre
            if 0.42 < st < 0.62:
                my = (y % 64) / 64.0
                d = math.hypot((st - 0.52) * 2.6, (my - 0.5) * 1.8)
                if d < 0.34:
                    c = mix(c, (0.74, 0.68, 0.50), (0.34 - d) * 1.4)
                if d < 0.13:
                    c = mix(c, (0.34, 0.38, 0.30), 0.7)
            img.set(x, y, c)
    stains(img, n, 7, alpha=0.38, seed=7)
    grain(img, 0.05)
    return img

def tex_wallpaper_damask(out):
    n = Noise(33)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 64.0, y / 64.0, 4)
            c = mix((0.55, 0.47, 0.38), (0.72, 0.64, 0.52), v)
            # repeating ornament
            ox, oy = (x % 64) / 64.0, (y % 64) / 64.0
            d = math.hypot((ox - .5) * 1.7, (oy - .5) * 1.7)
            petal = math.cos(math.atan2(oy - .5, ox - .5) * 6) * 0.10
            if d < 0.24 + petal:
                c = mix(c, (0.46, 0.38, 0.29), 0.55)
            if d < 0.10:
                c = mix(c, (0.60, 0.50, 0.38), 0.6)
            img.set(x, y, c)
    stains(img, n, 4, col=(0.25, 0.20, 0.14), alpha=0.3, seed=3)
    grain(img, 0.045)
    return img

def tex_wallpaper_floral(out):
    n = Noise(41)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 30.0, y / 30.0, 4)
            c = mix((0.70, 0.66, 0.56), (0.82, 0.78, 0.68), v)
            px, py = (x % 48) - 24, (y % 48) - 24
            d = math.hypot(px, py)
            if d < 4.5:
                c = mix(c, (0.55, 0.16, 0.14), 0.85)
            elif d < 11:
                for k in range(5):
                    a = k * 1.2566
                    fx, fy = math.cos(a) * 7.5, math.sin(a) * 7.5
                    if math.hypot(px - fx, py - fy) < 3.4:
                        c = mix(c, (0.42, 0.46, 0.30), 0.8)
            img.set(x, y, c)
    stains(img, n, 5, alpha=0.25, seed=11)
    grain(img, 0.05)
    return img

def tex_paint_cream(out):
    n = Noise(51)
    img = Img()
    base_noise(img, n, 6, (0.60, 0.58, 0.52), (0.76, 0.74, 0.68), 5)
    stains(img, n, 5, col=(0.30, 0.27, 0.20), alpha=0.22, seed=4)
    cracks(img, n, 9, seed=13, alpha=0.35)
    grain(img, 0.04)
    return img

def tex_paint_green(out):
    n = Noise(61)
    img = Img()
    base_noise(img, n, 5, (0.24, 0.31, 0.26), (0.33, 0.40, 0.33), 5)
    # worn patches showing plaster
    for y in range(256):
        for x in range(256):
            w = n.fbm(x / 22.0, y / 22.0, 3)
            if w > 0.30:
                c = img.get(x, y)
                img.set(x, y, mix((c[0] / 255, c[1] / 255, c[2] / 255), (0.52, 0.50, 0.45), (w - 0.3) * 2.2))
    cracks(img, n, 6, seed=23, alpha=0.4)
    grain(img, 0.04)
    return img

def tex_plaster_bare(out):
    n = Noise(71)
    img = Img()
    base_noise(img, n, 7, (0.36, 0.34, 0.31), (0.55, 0.53, 0.49), 5)
    # patches of exposed brick
    for y in range(256):
        for x in range(256):
            v = n.fbm(x / 40.0 + 3, y / 40.0, 4)
            if v > 0.34:
                bx = x % 32; by = y % 16 + (0 if (x // 32) % 2 else 8)
                mortar = bx < 2 or by < 2
                c = (0.24, 0.34, 0.28) if not mortar else (0.45, 0.43, 0.40)
                img.set(x, y, c)
    stains(img, n, 8, col=(0.18, 0.16, 0.12), alpha=0.4, seed=8)
    cracks(img, n, 12, seed=31, alpha=0.45)
    grain(img, 0.06)
    return img

def tex_brick(out):
    n = Noise(81)
    img = Img()
    for y in range(256):
        for x in range(256):
            row = y // 24
            off = (row % 2) * 24
            bx = (x + off) % 48
            by = y % 24
            if bx < 3 or by < 3:
                v = 0.5 + 0.5 * n.fbm(x / 12.0, y / 12.0, 3)
                c = mix((0.36, 0.35, 0.32), (0.50, 0.48, 0.44), v)
            else:
                bid = hf((row * 97 + (x + off) // 48) * 13, 7)
                v = 0.5 + 0.5 * n.fbm(x / 9.0, y / 9.0, 4)
                c = mix((0.30 + bid * 0.12, 0.13 + bid * 0.05, 0.10), (0.48, 0.22, 0.15), v)
                if n.fbm(x / 30.0, y / 30.0, 2) > 0.24:
                    c = mix(c, (0.20, 0.20, 0.19), 0.4)
            img.set(x, y, c)
    stains(img, n, 7, alpha=0.35, seed=17)
    grain(img, 0.06)
    return img

def tex_concrete_wall(out):
    n = Noise(91)
    img = Img()
    base_noise(img, n, 4, (0.32, 0.32, 0.31), (0.52, 0.52, 0.50), 5)
    # formwork seams
    for k in range(0, 256, 64):
        for y in range(256):
            img.blend(k, y, (0.22, 0.22, 0.22), 0.5)
        for x in range(256):
            img.blend(x, k, (0.24, 0.24, 0.24), 0.35)
    # tie holes
    rnd = random.Random(5)
    for yy in range(32, 256, 64):
        for xx in range(32, 256, 64):
            img.circle(xx, yy, 3.2, (0.15, 0.15, 0.15), 0.8)
    stains(img, n, 9, col=(0.20, 0.19, 0.16), alpha=0.4, seed=19, rad=(18, 70))
    cracks(img, n, 6, seed=37, alpha=0.4)
    grain(img, 0.05)
    return img

def tex_tile_bath(out):
    n = Noise(101)
    img = Img()
    for y in range(256):
        for x in range(256):
            tx, ty = x % 32, y % 32
            if tx < 2 or ty < 2:
                c = mix((0.42, 0.42, 0.38), (0.55, 0.54, 0.48), n.fbm(x / 8.0, y / 8.0, 2) * .5 + .5)
            else:
                v = 0.5 + 0.5 * n.fbm(x / 16.0, y / 16.0, 3)
                c = mix((0.72, 0.74, 0.72), (0.88, 0.90, 0.88), v)
                # gloss highlight
                g = max(0, 1 - math.hypot(tx - 12, ty - 10) / 16)
                c = mul(c, 1 + g * 0.12)
            img.set(x, y, c)
    # mold in grout
    for y in range(256):
        for x in range(256):
            if x % 32 < 3 or y % 32 < 3:
                m = 0.5 + 0.5 * n.fbm(x / 12.0 + 5, y / 12.0, 4)
                if m > 0.52:
                    img.set(x, y, mix((0.30, 0.30, 0.26), (0.18, 0.22, 0.16), (m - 0.5) * 2))
    stains(img, n, 4, col=(0.28, 0.26, 0.18), alpha=0.25, seed=27)
    grain(img, 0.03)
    return img

def tex_tile_kitchen(out):
    n = Noise(111)
    img = Img()
    for y in range(256):
        for x in range(256):
            tx, ty = x % 24, y % 24
            if tx < 2 or ty < 2:
                c = (0.34, 0.34, 0.32)
            else:
                v = 0.5 + 0.5 * n.fbm(x / 20.0, y / 20.0, 3)
                c = mix((0.52, 0.62, 0.64), (0.68, 0.78, 0.78), v)
            img.set(x, y, c)
    stains(img, n, 6, col=(0.30, 0.26, 0.16), alpha=0.3, seed=29)
    grain(img, 0.03)
    return img

def tex_panel_wood(out):
    n = Noise(121)
    img = Img()
    for y in range(256):
        for x in range(256):
            panel = x % 64
            grainv = n.fbm(x / 60.0, y / 5.0, 5)
            v = 0.5 + 0.5 * grainv
            c = mix((0.20, 0.13, 0.08), (0.34, 0.23, 0.14), v)
            if panel < 3 or panel > 61 or y % 128 < 2:
                c = mul(c, 0.55)
            # routed border
            if 8 < panel < 56 and 10 < (y % 128) < 118:
                if panel < 11 or panel > 53 or (y % 128) < 13 or (y % 128) > 115:
                    c = mul(c, 0.78)
            img.set(x, y, c)
    grain(img, 0.035)
    return img

# ---- floors
def tex_parquet(out):
    n = Noise(131)
    img = Img()
    # herringbone-ish: alternating 64x16 blocks
    for y in range(256):
        for x in range(256):
            block = (x // 16 + y // 16) % 2
            bx, by = x % 64, y % 16
            if block:
                bx, by = by * 4 % 64, bx // 4
            gv = n.fbm(x / 50.0, y / 3.0, 4)
            v = 0.5 + 0.5 * gv
            pl = hf((x // 64) * 31 + (y // 16) * 17, 3)
            c = mix((0.32 + pl * .10, 0.21 + pl * .07, 0.12 + pl * .04),
                    (0.50 + pl * .12, 0.35 + pl * .09, 0.20 + pl * .05), v)
            if x % 64 < 2 or y % 16 < 2:
                c = mul(c, 0.5)
            img.set(x, y, c)
    stains(img, n, 5, col=(0.18, 0.14, 0.10), alpha=0.3, seed=31)
    grain(img, 0.04)
    return img

def tex_lino_green(out):
    n = Noise(141)
    img = Img()
    for y in range(256):
        for x in range(256):
            w, _cw = n.worley(x / 40.0, y / 40.0)
            v = 0.5 + 0.5 * n.fbm(x / 30.0 + w, y / 30.0, 4)
            c = mix((0.28, 0.35, 0.28), (0.40, 0.47, 0.36), v)
            d = 0.5 + 0.5 * n.fbm(x / 12.0, y / 12.0, 3)
            if d > 0.62:
                c = mix(c, (0.36, 0.36, 0.30), 0.4)
            img.set(x, y, c)
    stains(img, n, 4, col=(0.20, 0.20, 0.16), alpha=0.28, seed=33)
    grain(img, 0.05)
    return img

def tex_lino_beige(out):
    n = Noise(151)
    img = Img()
    base_noise(img, n, 6, (0.52, 0.48, 0.40), (0.66, 0.62, 0.52), 4)
    # speckle
    for y in range(256):
        for x in range(256):
            if hf(x * 977 + y * 131, 9) > 0.985:
                img.blend(x, y, (0.30, 0.28, 0.22), 0.6)
    stains(img, n, 5, col=(0.28, 0.24, 0.18), alpha=0.3, seed=35)
    grain(img, 0.05)
    return img

def tex_carpet_red(out):
    n = Noise(161)
    img = Img()
    for y in range(256):
        for x in range(256):
            fuzz = 0.5 + 0.5 * n.fbm(x / 3.0, y / 3.0, 3)
            v = 0.5 + 0.5 * n.fbm(x / 50.0, y / 50.0, 4)
            c = mix((0.26, 0.10, 0.09), (0.44, 0.18, 0.14), v)
            c = mul(c, 0.85 + fuzz * 0.3)
            # border pattern
            if (x % 128) < 10 or (y % 128) < 10:
                c = mix(c, (0.34, 0.26, 0.12), 0.55)
            img.set(x, y, c)
    stains(img, n, 6, col=(0.12, 0.08, 0.06), alpha=0.35, seed=37)
    grain(img, 0.06)
    return img

def tex_concrete_floor(out):
    n = Noise(171)
    img = Img()
    base_noise(img, n, 5, (0.30, 0.30, 0.29), (0.46, 0.45, 0.43), 5)
    stains(img, n, 10, col=(0.16, 0.15, 0.12), alpha=0.42, seed=39, rad=(16, 80))
    cracks(img, n, 10, seed=41, alpha=0.5)
    grain(img, 0.06)
    return img

def tex_tile_lobby(out):
    n = Noise(181)
    img = Img()
    for y in range(256):
        for x in range(256):
            tx, ty = (x // 32) % 2, (y // 32) % 2
            base = (0.66, 0.64, 0.58) if tx ^ ty else (0.30, 0.30, 0.30)
            v = 0.5 + 0.5 * n.fbm(x / 10.0, y / 10.0, 4)
            c = mul(base, 0.9 + v * 0.2)
            if x % 32 < 2 or y % 32 < 2:
                c = mix(c, (0.42, 0.40, 0.36), 0.6)
            img.set(x, y, c)
    stains(img, n, 7, col=(0.22, 0.20, 0.16), alpha=0.35, seed=43)
    grain(img, 0.04)
    return img

def tex_stone_floor(out):
    n = Noise(191)
    img = Img()
    for y in range(256):
        for x in range(256):
            d, cid = n.worley(x / 56.0, y / 56.0)
            v = 0.5 + 0.5 * n.fbm(x / 14.0, y / 14.0, 4)
            c = mix((0.24 + cid * .10, 0.24 + cid * .09, 0.23 + cid * .08), (0.42, 0.41, 0.39), v)
            if d > 0.42:
                c = mul(c, 0.45)
            img.set(x, y, c)
    stains(img, n, 8, col=(0.14, 0.13, 0.10), alpha=0.4, seed=45)
    grain(img, 0.07)
    return img

def tex_wood_planks(out):
    n = Noise(201)
    img = Img()
    for y in range(256):
        for x in range(256):
            pi = y // 42
            gv = n.fbm(x / 70.0 + pi * 3, y / 6.0, 5)
            v = 0.5 + 0.5 * gv
            pl = hf(pi * 71, 5)
            c = mix((0.24 + pl * .1, 0.17 + pl * .06, 0.11), (0.42 + pl * .1, 0.31, 0.19), v)
            if y % 42 < 2:
                c = mul(c, 0.4)
            # nails
            if x % 128 == 12 and y % 42 == 12:
                c = (0.25, 0.25, 0.26)
            img.set(x, y, c)
    stains(img, n, 5, alpha=0.3, seed=47)
    grain(img, 0.04)
    return img

def tex_dirt_floor(out):
    n = Noise(211)
    img = Img()
    for y in range(256):
        for x in range(256):
            d, cid = n.worley(x / 22.0, y / 22.0)
            v = 0.5 + 0.5 * n.fbm(x / 18.0, y / 18.0, 5)
            c = mix((0.16, 0.14, 0.11), (0.30, 0.26, 0.20), v)
            if d > 0.5:
                c = mul(c, 0.6)
            if hf(x * 313 + y * 71, 13) > 0.99:
                c = (0.35, 0.33, 0.30)
            img.set(x, y, c)
    return img

# ---- ceilings
def tex_ceiling_stucco(out):
    n = Noise(221)
    img = Img()
    base_noise(img, n, 8, (0.62, 0.62, 0.58), (0.80, 0.80, 0.76), 5)
    # stipple
    for y in range(256):
        for x in range(256):
            d, _ = n.worley(x / 6.0, y / 6.0)
            if d < 0.22:
                img.blend(x, y, (0.50, 0.50, 0.47), 0.35)
    stains(img, n, 6, col=(0.35, 0.30, 0.20), alpha=0.3, seed=49, rad=(20, 70))
    cracks(img, n, 5, seed=51, alpha=0.3)
    grain(img, 0.03)
    return img

def tex_ceiling_water(out):
    img = tex_ceiling_stucco(out)
    n = Noise(231)
    for y in range(256):
        for x in range(256):
            v = n.fbm(x / 26.0, y / 26.0, 4)
            if v > 0.05:
                c = img.get(x, y)
                img.set(x, y, mix((c[0] / 255, c[1] / 255, c[2] / 255), (0.42, 0.34, 0.20), min(1, v * 1.8)))
    for k in range(3):
        img.circle(60 + k * 70, 90 + k * 40, 30 + k * 8, (0.40, 0.33, 0.20), 0.10, ring=2.5)
    return img

# ---- wood/metal/ceramic
def tex_wood_dark(out):
    n = Noise(241)
    img = Img()
    for y in range(256):
        for x in range(256):
            gv = n.fbm(x / 80.0, y / 4.0, 5) * 0.6 + n.fbm(x / 9.0, y / 40.0, 3) * 0.4
            v = 0.5 + 0.5 * gv
            c = mix((0.14, 0.09, 0.06), (0.30, 0.19, 0.11), v)
            img.set(x, y, c)
    grain(img, 0.03)
    return img

def tex_wood_light(out):
    n = Noise(251)
    img = Img()
    for y in range(256):
        for x in range(256):
            gv = n.fbm(x / 90.0, y / 5.0, 5)
            v = 0.5 + 0.5 * gv
            c = mix((0.46, 0.36, 0.24), (0.66, 0.54, 0.37), v)
            img.set(x, y, c)
    grain(img, 0.03)
    return img

def tex_metal_rust(out):
    n = Noise(261)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 24.0, y / 24.0, 5)
            c = mix((0.24, 0.20, 0.16), (0.42, 0.36, 0.28), v)
            r = n.fbm(x / 10.0 + 9, y / 10.0, 3)
            if r > 0.18:
                c = mix(c, (0.42, 0.22, 0.10), min(1, (r - 0.18) * 3))
            if n.fbm(x / 50.0, y / 50.0, 2) > 0.3:
                c = (0.30, 0.30, 0.31)
            # plate seams and rivets
            if x % 64 < 2 or y % 64 < 2:
                c = mul(c, 0.5)
            if x % 64 == 32 and y % 64 == 32:
                img.circle(x, y, 3, (0.35, 0.34, 0.33), 0.9)
            img.set(x, y, c)
    grain(img, 0.06)
    return img

def tex_metal_painted(out):
    n = Noise(271)
    img = Img()
    base_noise(img, n, 6, (0.26, 0.32, 0.28), (0.36, 0.42, 0.36), 4)
    for y in range(256):
        for x in range(256):
            v = n.fbm(x / 16.0, y / 16.0, 3)
            if v > 0.3:
                c = img.get(x, y)
                img.set(x, y, mix((c[0] / 255, c[1] / 255, c[2] / 255), (0.30, 0.24, 0.16), (v - .3) * 2))
    grain(img, 0.04)
    return img

def tex_metal_steel(out):
    n = Noise(281)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 80.0, y / 2.0, 4)
            c = mix((0.36, 0.37, 0.38), (0.56, 0.57, 0.58), v)
            img.set(x, y, c)
    stains(img, n, 4, col=(0.30, 0.26, 0.20), alpha=0.25, seed=53)
    grain(img, 0.04)
    return img

def tex_ceramic_white(out):
    n = Noise(291)
    img = Img()
    base_noise(img, n, 5, (0.74, 0.74, 0.72), (0.90, 0.90, 0.88), 3)
    stains(img, n, 3, col=(0.40, 0.36, 0.26), alpha=0.18, seed=55)
    grain(img, 0.02)
    return img

# ---- fabrics
def tex_fabric_sofa(out):
    n = Noise(301)
    img = Img()
    for y in range(256):
        for x in range(256):
            weave = math.sin(x * 1.6) * math.sin(y * 1.6) * 0.5 + 0.5
            v = 0.5 + 0.5 * n.fbm(x / 30.0, y / 30.0, 4)
            c = mix((0.20, 0.26, 0.22), (0.32, 0.40, 0.32), v)
            c = mul(c, 0.9 + weave * 0.2)
            img.set(x, y, c)
    stains(img, n, 5, col=(0.14, 0.12, 0.09), alpha=0.3, seed=57)
    grain(img, 0.04)
    return img

def tex_mattress(out):
    n = Noise(311)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 40.0, y / 40.0, 4)
            c = mix((0.60, 0.58, 0.50), (0.76, 0.74, 0.66), v)
            # ticking stripes
            if x % 22 < 3:
                c = mix(c, (0.52, 0.46, 0.36), 0.6)
            img.set(x, y, c)
    stains(img, n, 7, col=(0.38, 0.32, 0.18), alpha=0.35, seed=59, rad=(8, 34))
    grain(img, 0.04)
    return img

def tex_curtain_lace(out):
    n = Noise(321)
    img = Img(256, 256, (0.78, 0.76, 0.68, 0.55))
    for y in range(256):
        for x in range(256):
            m = math.sin(x * 0.7) * math.sin(y * 0.7)
            v = n.fbm(x / 12.0, y / 12.0, 3)
            a = 0.30 + 0.32 * (0.5 + 0.5 * m) + 0.18 * v
            if (x % 16 < 2) and (y % 16 < 2):
                a = 0.75
            img.set(x, y, (0.80, 0.78, 0.70, cl(a, 0.10, 0.86)))
    return img

def tex_fabric_velvet(out):
    n = Noise(331)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 26.0, y / 26.0, 4)
            c = mix((0.28, 0.10, 0.10), (0.44, 0.16, 0.14), v)
            shine = (0.5 + 0.5 * math.sin(x * 0.20)) * 0.12
            c = mul(c, 1 + shine)
            img.set(x, y, c)
    grain(img, 0.04)
    return img

# ---- paintings / photos / signs
def tex_painting_landscape(out):
    n = Noise(341)
    img = Img()
    # sky gradient
    for y in range(256):
        t = y / 256.0
        c = mix((0.72, 0.56, 0.36), (0.20, 0.26, 0.34), t * 1.4)
        for x in range(256):
            img.set(x, y, c)
    # sun
    img.circle(180, 70, 22, (0.86, 0.74, 0.50), 0.55)
    # distant treeline
    for x in range(256):
        base = 150 + int(16 * n.fbm(x / 60.0, 3.0, 3))
        for y in range(base, 256):
            v = 0.5 + 0.5 * n.fbm(x / 16.0, y / 16.0, 4)
            c = mix((0.16, 0.20, 0.14), (0.30, 0.34, 0.22), v)
            img.set(x, y, c)
    # birches
    rnd = random.Random(3)
    for i in range(7):
        bx = rnd.randint(10, 240)
        w = rnd.randint(2, 5)
        for y in range(60 + rnd.randint(0, 40), 200):
            for x in range(bx, bx + w):
                v = 0.5 + 0.5 * n.fbm(x / 20.0, y / 4.0, 3)
                c = mix((0.62, 0.60, 0.54), (0.80, 0.78, 0.72), v)
                if hf(x * 31 + y * 17, 7) > 0.90:
                    c = (0.20, 0.18, 0.16)
                img.set(x, y, c)
    # water
    for y in range(196, 256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 40.0, y / 6.0, 3)
            img.set(x, y, mix((0.24, 0.30, 0.30), (0.46, 0.50, 0.46), v))
    # varnish yellowing + craquelure
    for y in range(256):
        for x in range(256):
            c = img.get(x, y)
            img.set(x, y, mix((c[0] / 255, c[1] / 255, c[2] / 255), (0.55, 0.48, 0.30), 0.16))
    cracks(img, n, 26, seed=61, col=(0.25, 0.20, 0.14), alpha=0.25)
    grain(img, 0.05)
    return img

def tex_painting_portrait(out):
    n = Noise(351)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 50.0, y / 50.0, 4)
            img.set(x, y, mix((0.10, 0.09, 0.08), (0.24, 0.21, 0.17), v))
    # dark figure with pale face
    img.rectangle = None
    for y in range(120, 256):
        for x in range(70, 190):
            dx = (x - 130) / 60.0
            if abs(dx) < 1 - (y - 190) / 90.0 if y > 190 else True:
                v = 0.5 + 0.5 * n.fbm(x / 14.0, y / 14.0, 3)
                img.set(x, y, mix((0.12, 0.10, 0.10), (0.22, 0.18, 0.16), v))
    # face
    for y in range(70, 130):
        for x in range(105, 155):
            d = math.hypot((x - 130) / 26.0, (y - 100) / 32.0)
            if d < 1:
                v = 0.5 + 0.5 * n.fbm(x / 9.0, y / 9.0, 3)
                img.set(x, y, mix((0.52, 0.46, 0.38), (0.72, 0.66, 0.56), v))
    # eyes
    for ex in (120, 140):
        for y in range(96, 103):
            for x in range(ex - 4, ex + 4):
                img.set(x, y, (0.09, 0.08, 0.08))
        img.circle(ex, 99, 1.6, (0.80, 0.76, 0.60), 0.9)
    # mouth
    for x in range(124, 136):
        img.set(x, 116, (0.28, 0.16, 0.14))
    for y in range(256):
        for x in range(256):
            c = img.get(x, y)
            img.set(x, y, mix((c[0] / 255, c[1] / 255, c[2] / 255), (0.52, 0.44, 0.26), 0.18))
    cracks(img, n, 20, seed=63, col=(0.30, 0.24, 0.16), alpha=0.2)
    grain(img, 0.05)
    return img

def tex_painting_abstract(out):
    n = Noise(361)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 40.0, y / 40.0, 3)
            img.set(x, y, mix((0.62, 0.58, 0.50), (0.74, 0.70, 0.62), v))
    rnd = random.Random(11)
    cols = [(0.20, 0.22, 0.26), (0.44, 0.24, 0.16), (0.28, 0.34, 0.30), (0.66, 0.60, 0.42)]
    for i in range(9):
        x0 = rnd.randint(10, 180); y0 = rnd.randint(10, 180)
        w = rnd.randint(20, 80); h = rnd.randint(20, 80)
        c = cols[i % len(cols)]
        for y in range(y0, min(256, y0 + h)):
            for x in range(x0, min(256, x0 + w)):
                img.blend(x, y, c, 0.85)
    grain(img, 0.05)
    return img

def tex_photo_family(out):
    n = Noise(371)
    img = Img(256, 256)
    for y in range(256):
        for x in range(256):
            img.set(x, y, (0.42, 0.36, 0.28))
    for y in range(16, 240):
        for x in range(16, 240):
            v = 0.5 + 0.5 * n.fbm(x / 18.0, y / 18.0, 4)
            img.set(x, y, mix((0.62, 0.55, 0.42), (0.78, 0.72, 0.58), v))
    # three figures silhouettes
    for (fx, fy, fw, fh) in ((80, 120, 26, 90), (128, 108, 30, 104), (176, 128, 24, 82)):
        for y in range(fy, fy + fh):
            for x in range(fx - fw // 2, fx + fw // 2):
                if math.hypot((x - fx) / (fw * .6), (y - (fy - 14)) / 16) < 1 or y > fy:
                    v = 0.5 + 0.5 * n.fbm(x / 8.0, y / 8.0, 3)
                    img.set(x, y, mix((0.20, 0.18, 0.16), (0.34, 0.30, 0.26), v))
    stains(img, n, 6, col=(0.34, 0.28, 0.18), alpha=0.3, seed=65, rad=(10, 40))
    grain(img, 0.10)
    return img

def tex_clock_face(out):
    img = Img(256, 256, (0.72, 0.68, 0.58))
    img.circle(128, 128, 120, (0.30, 0.26, 0.20), 1.0)
    img.circle(128, 128, 112, (0.80, 0.76, 0.66), 1.0)
    for i in range(12):
        a = i * math.pi / 6
        x0 = 128 + math.cos(a) * 96; y0 = 128 + math.sin(a) * 96
        x1 = 128 + math.cos(a) * (108 if i % 3 == 0 else 103)
        y1 = 128 + math.sin(a) * (108 if i % 3 == 0 else 103)
        img.line(x0, y0, x1, y1, (0.15, 0.13, 0.11), 3 if i % 3 == 0 else 1.5)
    img.line(128, 128, 128, 60, (0.12, 0.10, 0.09), 5)     # hour
    img.line(128, 128, 176, 150, (0.12, 0.10, 0.09), 3.5)  # minute
    img.circle(128, 128, 5, (0.20, 0.16, 0.12), 1.0)
    n = Noise(381)
    stains(img, n, 3, col=(0.40, 0.34, 0.20), alpha=0.2, seed=67)
    grain(img, 0.05)
    return img

def tex_radio_face(out):
    img = Img(256, 256, (0.20, 0.17, 0.12))
    for y in range(40, 120):
        for x in range(20, 236):
            v = 0.5 + 0.5 * math.sin(x * 0.4) * 0.5 + 0.25
            img.set(x, y, mix((0.30, 0.26, 0.18), (0.46, 0.40, 0.28), v))
    for i in range(12):
        x = 26 + i * 17
        img.line(x, 100, x, 112, (0.10, 0.09, 0.08), 2)
    draw_text(img, '88  92  100  104', 22, 116, 14, (0.12, 0.10, 0.08))
    img.rect(24, 150, 232, 176, (0.10, 0.09, 0.07))
    for i in range(4):
        img.circle(48 + i * 52, 210, 15, (0.26, 0.22, 0.16), 1.0)
        img.circle(48 + i * 52, 210, 12, (0.14, 0.12, 0.10), 1.0)
    n = Noise(391)
    grain(img, 0.06)
    return img

def tex_document(out):
    img = Img(256, 256, (0.82, 0.79, 0.68))
    n = Noise(401)
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 30.0, y / 30.0, 3)
            img.set(x, y, mix((0.72, 0.69, 0.58), (0.86, 0.84, 0.74), v))
    img.rect(0, 0, 256, 256, (0, 0, 0), 0.0)
    y = 22
    draw_text(img, 'INCIDENT REPORT  1998-11', 14, 8, 12, (0.22, 0.20, 0.18))
    for i in range(16):
        wline = 150 + (i * 37 % 80)
        for x in range(14, 14 + wline):
            if hf(x * 3 + i * 91, 5) > 0.15:
                img.blend(x, y, (0.24, 0.22, 0.20), 0.75)
        y += 13
        if i == 7:
            y += 8
    stains(img, n, 4, col=(0.45, 0.38, 0.22), alpha=0.22, seed=69)
    grain(img, 0.04)
    return img

def tex_books_row(out):
    n = Noise(411)
    img = Img()
    rnd = random.Random(7)
    img.rect(0, 0, 256, 256, (0.06, 0.05, 0.04))
    x = 0
    while x < 256:
        w = rnd.randint(6, 16)
        h = rnd.randint(150, 240)
        col = (rnd.uniform(0.14, 0.44), rnd.uniform(0.10, 0.34), rnd.uniform(0.08, 0.28))
        for y in range(256 - h, 256):
            for xx in range(x, min(256, x + w)):
                v = 0.5 + 0.5 * n.fbm(xx / 5.0, y / 40.0, 3)
                c = mul(col, 0.8 + v * 0.4)
                if xx == x or xx == x + w - 1:
                    c = mul(col, 0.6)
                if y in (256 - h + 6, 256 - h + 9):
                    c = mix(c, (0.70, 0.64, 0.40), 0.5)
                img.set(xx, y, c)
        x += w + 1
    grain(img, 0.04)
    return img

def tex_door_panel(out):
    n = Noise(421)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 40.0, y / 40.0, 4)
            c = mix((0.66, 0.64, 0.58), (0.80, 0.78, 0.72), v)
            img.set(x, y, c)
    # two recessed panels
    def panel(x0, y0, x1, y1):
        for y in range(y0, y1):
            for x in range(x0, x1):
                edge = min(x - x0, x1 - 1 - x, y - y0, y1 - 1 - y)
                f = 1.0
                if edge < 3:
                    f = 0.80
                elif edge < 6:
                    f = 1.05
                c = img.get(x, y)
                img.set(x, y, mul((c[0] / 255, c[1] / 255, c[2] / 255), f))
    panel(40, 24, 216, 110)
    panel(40, 140, 216, 232)
    stains(img, n, 4, col=(0.35, 0.30, 0.20), alpha=0.22, seed=71)
    grain(img, 0.04)
    return img

def tex_door_metal(out):
    img = tex_metal_rust(out)
    n = Noise(431)
    # cross bracing
    for i in range(256):
        img.blend(i, i, (0.20, 0.19, 0.18), 0.7)
        img.blend(i, 255 - i, (0.20, 0.19, 0.18), 0.7)
    # handle plate
    img.rect(190, 118, 240, 138, (0.30, 0.29, 0.27), 0.9)
    img.circle(228, 128, 6, (0.14, 0.13, 0.12), 1.0)
    return img

def tex_glass_dirty(out):
    n = Noise(441)
    img = Img(256, 256, (0.60, 0.66, 0.70, 0.30))
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 30.0, y / 30.0, 4)
            a = 0.06 + 0.20 * v
            g = 0.55 + 0.25 * v
            img.set(x, y, (g, g * 1.02, g * 1.05, cl(a, 0.02, 0.42)))
    # streaks
    for i in range(20):
        x0 = int(hf(i * 31, 3) * 256)
        for y in range(256):
            img.blend(x0 + int(math.sin(y / 20.0) * 3), y, (0.75, 0.75, 0.72), 0.05)
    return img

def tex_light_fluorescent(out):
    img = Img(256, 256, (0.86, 0.88, 0.82))
    n = Noise(451)
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 60.0, y / 8.0, 3)
            img.set(x, y, mix((0.78, 0.80, 0.74), (0.98, 0.99, 0.94), v))
    # grille
    for y in range(0, 256, 12):
        for x in range(256):
            img.blend(x, y, (0.55, 0.56, 0.52), 0.5)
    for x in range(0, 256, 12):
        for y in range(256):
            img.blend(x, y, (0.60, 0.61, 0.57), 0.35)
    return img

def tex_lamp_shade(out):
    img = Img(256, 256, (0.74, 0.66, 0.50))
    n = Noise(461)
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 20.0, y / 20.0, 4)
            a = 0.55 + 0.35 * v
            img.set(x, y, mix((0.70, 0.60, 0.42), (0.86, 0.78, 0.60), v) + (cl(a, .3, .95),))
    return img

def tex_cobweb(out):
    img = Img(256, 256, (0, 0, 0, 0))
    cx, cy = 0, 0
    for k in range(12):
        a = k * math.pi / 2 / 6
        for t in range(0, 200):
            x = cx + math.cos(a) * t; y = cy + math.sin(a) * t
            img.blend(int(x), int(y), (0.80, 0.80, 0.78), 0.30 * (1 - t / 200.0) * 3)
    for r in range(14, 200, 16):
        for t in range(0, 300):
            a = t * math.pi / 2 / 150
            if a > math.pi / 2 * 0.98:
                break
            x = cx + math.cos(a) * r; y = cy + math.sin(a) * r
            img.blend(int(x), int(y), (0.78, 0.78, 0.76), 0.20)
    return img

def tex_exit_sign(out):
    img = Img(256, 256, (0.04, 0.10, 0.06))
    img.rect(8, 70, 248, 186, (0.06, 0.30, 0.12), 1.0)
    draw_text(img, 'EXIT', 40, 92, 60, (0.85, 0.95, 0.85))
    for i in range(40):
        img.rect(196 + i, 120 - i // 3, 200 + i, 132 + i // 3, (0.85, 0.95, 0.85), 1.0)
    return img

def tex_tv_static(out):
    img = Img()
    for y in range(256):
        for x in range(256):
            v = hf(x * 7919 + y * 104729, 3)
            c = v * 0.75
            if y % 64 < 2:
                c *= 0.6
            img.set(x, y, (c, c * 1.02, c * 1.05))
    return img

def tex_paper_old(out):
    n = Noise(471)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 40.0, y / 40.0, 4)
            img.set(x, y, mix((0.62, 0.56, 0.40), (0.80, 0.75, 0.58), v))
    stains(img, n, 8, col=(0.45, 0.36, 0.20), alpha=0.3, seed=73)
    for i in range(10):
        y = 20 + i * 22
        draw_text(img, 'lorem ipsum dolor sit', 16, y, 10, (0.25, 0.22, 0.18), a=0.7)
    return img

# ---- character
def tex_skin_pale(out):
    n = Noise(481)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 22.0, y / 22.0, 4)
            c = mix((0.52, 0.44, 0.40), (0.66, 0.56, 0.50), v)
            img.set(x, y, c)
    grain(img, 0.03)
    return img

def tex_coat_black(out):
    n = Noise(491)
    img = Img()
    for y in range(256):
        for x in range(256):
            weave = (math.sin(x * 2.2) * math.sin(y * 2.2)) * 0.5 + 0.5
            v = 0.5 + 0.5 * n.fbm(x / 20.0, y / 20.0, 4)
            c = mix((0.045, 0.045, 0.05), (0.10, 0.10, 0.11), v) 
            c = mul(c, 0.85 + weave * 0.30)
            img.set(x, y, c)
    grain(img, 0.03)
    return img

def tex_coat_white(out):
    n = Noise(501)
    img = Img()
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 24.0, y / 24.0, 4)
            c = mix((0.60, 0.60, 0.58), (0.78, 0.78, 0.75), v)
            img.set(x, y, c)
    stains(img, n, 5, col=(0.30, 0.28, 0.22), alpha=0.3, seed=75)
    grain(img, 0.03)
    return img

def tex_hair_black(out):
    n = Noise(511)
    img = Img()
    for y in range(256):
        for x in range(256):
            s = math.sin(x * 0.9 + math.sin(y * 0.05) * 3) * 0.5 + 0.5
            v = 0.5 + 0.5 * n.fbm(x / 12.0, y / 60.0, 4)
            c = mix((0.03, 0.03, 0.035), (0.12, 0.11, 0.12), s * 0.6 + v * 0.4)
            img.set(x, y, c)
    return img

def tex_face_dark(out):
    img = Img(256, 256, (0.02, 0.02, 0.025))
    return img

def tex_shadow(out):
    n = Noise(521)
    img = Img(256, 256, (0, 0, 0, 0.0))
    for y in range(256):
        for x in range(256):
            dx = (x - 128) / 128.0; dy = (y - 128) / 150.0
            d = math.hypot(dx, dy)
            a = cl(1.0 - d, 0, 1) ** 1.4
            a *= 0.55 + 0.45 * (0.5 + 0.5 * n.fbm(x / 10.0, y / 10.0, 4))
            img.set(x, y, (0.0, 0.0, 0.0, cl(a, 0, 0.92)))
    return img

def tex_blood(out):
    n = Noise(531)
    img = Img(256, 256, (0, 0, 0, 0))
    for y in range(256):
        for x in range(256):
            dx = (x - 128) / 90.0; dy = (y - 128) / 70.0
            d = math.hypot(dx, dy)
            v = 0.5 + 0.5 * n.fbm(x / 16.0, y / 16.0, 4)
            a = cl(1.0 - d - v * 0.35, 0, 1) ** 1.2
            if a > 0:
                img.set(x, y, (0.16, 0.03, 0.03, a * 0.9))
    return img

def tex_wall_sign(out):
    img = Img(256, 256, (0.70, 0.68, 0.62))
    img.rect(10, 60, 246, 196, (0.80, 0.78, 0.72), 1.0)
    img.rect(14, 64, 242, 192, (0.60, 0.58, 0.52), 1.0)
    draw_text(img, 'FLOOR 3', 52, 84, 34, (0.16, 0.15, 0.14))
    draw_text(img, 'APTS 301 - 308', 34, 130, 18, (0.20, 0.19, 0.18))
    n = Noise(541)
    stains(img, n, 5, col=(0.30, 0.26, 0.18), alpha=0.3, seed=77)
    grain(img, 0.04)
    return img

def tex_rules_rules(out):
    img = Img(256, 320, (0.84, 0.81, 0.70))
    draw_text(img, 'HOUSE RULES', 18, 14, 22, (0.18, 0.16, 0.14))
    lines = ['1. No noise after 23:00.', '2. Do not use the lift', '   between 02:00 and 03:00.',
             '3. If you hear knocking,', '   do not answer.', '4. Count the doors on',
             '   your way in.', '5. If there are more doors', '   than yesterday, go back.',
             '6. Do not look back.']
    y = 54
    for L in lines:
        draw_text(img, L, 18, y, 13, (0.22, 0.20, 0.18))
        y += 22
    n = Noise(551)
    stains(img, n, 4, col=(0.42, 0.34, 0.20), alpha=0.25, seed=79)
    return img

def tex_note_wall(out):
    img = Img(256, 256, (0.80, 0.78, 0.70))
    n = Noise(561)
    for y in range(256):
        for x in range(256):
            v = 0.5 + 0.5 * n.fbm(x / 30.0, y / 30.0, 3)
            img.set(x, y, mix((0.72, 0.70, 0.62), (0.86, 0.84, 0.76), v))
    draw_text(img, 'IT IS LEARNING', 16, 40, 20, (0.20, 0.10, 0.10))
    draw_text(img, 'YOUR VOICE', 16, 70, 20, (0.20, 0.10, 0.10))
    for i in range(6):
        for x in range(16, 16 + 120 + (i * 13 % 60)):
            img.blend(x, 116 + i * 14, (0.25, 0.22, 0.20), 0.6)
    return img


TEXTURES = {
    'wallpaper_stripe': tex_wallpaper_stripe,
    'wallpaper_damask': tex_wallpaper_damask,
    'wallpaper_floral': tex_wallpaper_floral,
    'paint_cream': tex_paint_cream,
    'paint_green': tex_paint_green,
    'plaster_bare': tex_plaster_bare,
    'brick': tex_brick,
    'concrete_wall': tex_concrete_wall,
    'tile_bath': tex_tile_bath,
    'tile_kitchen': tex_tile_kitchen,
    'panel_wood': tex_panel_wood,
    'parquet': tex_parquet,
    'lino_green': tex_lino_green,
    'lino_beige': tex_lino_beige,
    'carpet_red': tex_carpet_red,
    'concrete_floor': tex_concrete_floor,
    'tile_lobby': tex_tile_lobby,
    'stone_floor': tex_stone_floor,
    'wood_planks': tex_wood_planks,
    'dirt_floor': tex_dirt_floor,
    'ceiling_stucco': tex_ceiling_stucco,
    'ceiling_water': tex_ceiling_water,
    'wood_dark': tex_wood_dark,
    'wood_light': tex_wood_light,
    'metal_rust': tex_metal_rust,
    'metal_painted': tex_metal_painted,
    'metal_steel': tex_metal_steel,
    'ceramic_white': tex_ceramic_white,
    'fabric_sofa': tex_fabric_sofa,
    'mattress': tex_mattress,
    'curtain_lace': tex_curtain_lace,
    'fabric_velvet': tex_fabric_velvet,
    'painting_landscape': tex_painting_landscape,
    'painting_portrait': tex_painting_portrait,
    'painting_abstract': tex_painting_abstract,
    'photo_family': tex_photo_family,
    'clock_face': tex_clock_face,
    'radio_face': tex_radio_face,
    'document': tex_document,
    'books_row': tex_books_row,
    'door_panel': tex_door_panel,
    'door_metal': tex_door_metal,
    'glass_dirty': tex_glass_dirty,
    'light_fluorescent': tex_light_fluorescent,
    'lamp_shade': tex_lamp_shade,
    'cobweb': tex_cobweb,
    'exit_sign': tex_exit_sign,
    'tv_static': tex_tv_static,
    'paper_old': tex_paper_old,
    'skin_pale': tex_skin_pale,
    'coat_black': tex_coat_black,
    'coat_white': tex_coat_white,
    'hair_black': tex_hair_black,
    'face_dark': tex_face_dark,
    'shadow': tex_shadow,
    'blood': tex_blood,
    'wall_sign': tex_wall_sign,
    'rules': tex_rules_rules,
    'note_wall': tex_note_wall,
}

def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else 'assets/tex'
    os.makedirs(outdir, exist_ok=True)
    only = sys.argv[2] if len(sys.argv) > 2 else None
    for name, fn in TEXTURES.items():
        if only and only != name:
            continue
        img = fn(outdir)
        img.save(os.path.join(outdir, name + '.png'))
        print('tex', name, img.w, img.h)

if __name__ == '__main__':
    main()
