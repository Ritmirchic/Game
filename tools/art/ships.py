"""SPACE MINER — procedural ship / enemy / drone hull art.

Ships are painted as layered vector shapes (top-down view, nose pointing up),
then shaded: directional metal lighting, bevels, panel seams, rivets, emissive
accents and cockpit glass.  Each hull is baked into a 32-frame rotation sheet so
the game can spin it without runtime resampling.
"""
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from artlib import Canvas, clamp01, smoothstep, hex2rgb, mix, value_noise

FRAMES = 32
SS = 3


class Painter:
    """Layered vector painter on a supersampled canvas."""

    def __init__(self, size, ss=SS):
        self.size = size
        self.ss = ss
        self.S = size * ss
        self.layers = []          # (mask, rgb, additive)

    def _d(self, fn):
        img = Image.new('L', (self.S, self.S), 0)
        d = ImageDraw.Draw(img)
        fn(d)
        return np.asarray(img, dtype=np.float64) / 255.0

    # --- primitives (coordinates in 0..1 of the canvas) -------------------
    def poly(self, pts, color, alpha=1.0, add=False):
        p = [(x * self.S, y * self.S) for x, y in pts]
        m = self._d(lambda d: d.polygon(p, fill=255))
        self.layers.append((m * alpha, np.asarray(color, dtype=np.float64), add))

    def polyline(self, pts, width, color, alpha=1.0, add=False, closed=False):
        p = [(x * self.S, y * self.S) for x, y in pts]
        if closed:
            p = p + [p[0]]
        w = max(1, int(width * self.S))
        m = self._d(lambda d: d.line(p, fill=255, width=w, joint='curve'))
        self.layers.append((m * alpha, np.asarray(color, dtype=np.float64), add))

    def ring(self, cx, cy, r, width, color, alpha=1.0, add=False):
        cx, cy, r, width = cx * self.S, cy * self.S, r * self.S, max(1, width * self.S)
        def f(d):
            d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=255, width=int(width))
        m = self._d(f)
        self.layers.append((m * alpha, np.asarray(color, dtype=np.float64), add))

    def disc(self, cx, cy, r, color, alpha=1.0, add=False):
        cx, cy, r = cx * self.S, cy * self.S, r * self.S
        m = self._d(lambda d: d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255))
        self.layers.append((m * alpha, np.asarray(color, dtype=np.float64), add))

    def rect(self, x, y, w, h, color, alpha=1.0, add=False):
        X, Y, W, H = x * self.S, y * self.S, w * self.S, h * self.S
        m = self._d(lambda d: d.rectangle([X, Y, X + W, Y + H], fill=255))
        self.layers.append((m * alpha, np.asarray(color, dtype=np.float64), add))

    def arc(self, cx, cy, r, a0, a1, width, color, alpha=1.0, add=False):
        cx, cy, r = cx * self.S, cy * self.S, r * self.S
        w = max(1, int(width * self.S))
        m = self._d(lambda d: d.arc([cx - r, cy - r, cx + r, cy + r],
                                    int(math.degrees(a0)), int(math.degrees(a1)), fill=255, width=w))
        self.layers.append((m * alpha, np.asarray(color, dtype=np.float64), add))

    # --- composition ------------------------------------------------------
    def compose(self, hull_mask=None, light=(-0.55, -0.62)):
        S = self.S
        rgb = np.zeros((S, S, 3))
        a = np.zeros((S, S))
        ys, xs = np.mgrid[0:S, 0:S]
        gxn = (xs / S - 0.5) * 2.0
        gyn = (ys / S - 0.5) * 2.0
        ll = math.hypot(light[0], light[1])
        lxn, lyn = light[0] / ll, light[1] / ll
        big_area = 0.012 * S * S
        for m, col, add in self.layers:
            if add:
                rgb += m[..., None] * col
                continue
            area = float(m.sum())
            c = np.asarray(col, dtype=np.float64)
            if area > big_area:
                # large plate: directional gradient + inner drop shadow feel
                t = clamp01(0.5 - (gxn * lxn + gyn * lyn) * 0.45)
                shade = (0.58 + 0.62 * t)[..., None]
                plate = c * shade
                plate = plate + (clamp01(1.0 - t) ** 4)[..., None] * c * 0.16
                sel = m[..., None]
                rgb = rgb * (1 - sel) + plate * sel
            else:
                rgb = rgb * (1 - m[..., None]) + c * m[..., None]
            a = clamp01(a + m * 0.999)
        return rgb, clamp01(a)

    def finish(self, rgb, a, edge_hi=(0.85, 0.92, 1.0), edge_lo=(0.02, 0.03, 0.05),
               light=(-0.55, -0.62)):
        """Directional bevel: bright chamfer facing the key light, dark rim away."""
        S = self.S
        a_img = Image.fromarray((a * 255).astype(np.uint8), 'L')
        blur = np.asarray(a_img.filter(ImageFilter.GaussianBlur(2.0 * self.ss)), dtype=np.float64) / 255.0
        gy, gx = np.gradient(blur)
        ln = np.sqrt(gx * gx + gy * gy) + 1e-6
        nx, ny = -gx / ln, -gy / ln        # outward normals
        lx, ly = light
        ll = math.hypot(lx, ly)
        lx, ly = lx / ll, ly / ll
        lam = clamp01(nx * lx + ny * ly)
        edge = clamp01(blur * (1.0 - blur) * 8.0)       # boundary band
        edge = edge * clamp01(ln * 14.0)
        rgb = rgb + (edge * lam)[..., None] * np.asarray(edge_hi) * 0.85
        rgb = rgb * (1.0 - (edge * (1.0 - lam) * 0.62)[..., None])
        rgb = rgb * (1.0 - (edge * (1.0 - lam))[..., None] * np.asarray(edge_lo) * 2.0)
        # overall key/fill gradient across the sprite
        ys, xs = np.mgrid[0:S, 0:S]
        gxn = (xs / S - 0.5) * 2
        gyn = (ys / S - 0.5) * 2
        key = clamp01(0.5 - (gxn * lx + gyn * ly) * 0.42)
        rgb = rgb * (0.80 + 0.38 * key)[..., None]
        rgb = rgb + (clamp01(1.0 - key) ** 3)[..., None] * np.array([0.03, 0.05, 0.10])
        # brushed metal micro-texture + panel grit
        n1 = value_noise(S, S, max(8, S // 6), 4242, octaves=2, gain=0.5) - 0.5
        n2 = value_noise(S, S, max(20, S // 2), 99, octaves=1) - 0.5
        grit = (1.0 + 0.10 * n1 + 0.05 * n2)[..., None]
        rgb = rgb * grit
        # specular sheen band along the light axis
        sheen = np.exp(-(((gxn - lx * 0.55) ** 2 + (gyn - ly * 0.55) ** 2)) / 0.10)
        rgb = rgb + (sheen * clamp01(a))[..., None] * 0.10
        return rgb, clamp01(a)

    def to_image(self, rgb, a):
        ss = self.ss
        h, w = self.size, self.size
        rgb = rgb.reshape(h, ss, w, ss, 3).mean(axis=(1, 3))
        am = a.reshape(h, ss, w, ss).mean(axis=(1, 3))
        arr = np.concatenate([clamp01(rgb), clamp01(am)[..., None]], axis=2)
        return Image.fromarray((clamp01(arr) * 255 + 0.5).astype(np.uint8), 'RGBA')


# --------------------------------------------------------------------------- #
#  shared helpers
# --------------------------------------------------------------------------- #
def mirror(pts):
    return [(1 - x, y) for x, y in pts]


def panel_seams(p, pts, count, color, width=0.004, alpha=0.5, vertical=True):
    """Straight hull seams from one side of a polygon to the other."""
    xs = [q[0] for q in pts]
    ys = [q[1] for q in pts]
    x0, x1 = min(xs), max(xs)
    y0, y1 = min(ys), max(ys)
    for i in range(count):
        t = (i + 1) / (count + 1)
        if vertical:
            p.polyline([(x0 + (x1 - x0) * t, y0), (x0 + (x1 - x0) * t, y1)], width, color, alpha)
        else:
            p.polyline([(x0, y0 + (y1 - y0) * t), (x1, y0 + (y1 - y0) * t)], width, color, alpha)


DARK = hex2rgb('#10151d')
SEAM = hex2rgb('#0b0f15')


# --------------------------------------------------------------------------- #
#  SHIP: PROSPECTOR — balanced mining cutter
# --------------------------------------------------------------------------- #
def paint_prospector(p):
    body = hex2rgb('#c8d3e0')
    body2 = hex2rgb('#8f9cad')
    dark = hex2rgb('#39424f')
    accent = hex2rgb('#f2b134')
    cyan = hex2rgb('#39e8ff')
    glass = hex2rgb('#12313f')

    # engine nacelles (behind hull)
    for side in (-1, 1):
        x = 0.5 + side * 0.175
        p.poly([(x - 0.052, 0.52), (x + 0.052, 0.52), (x + 0.060, 0.80), (x + 0.030, 0.86),
                (x - 0.030, 0.86), (x - 0.060, 0.80)], body2)
        p.poly([(x - 0.040, 0.80), (x + 0.040, 0.80), (x + 0.030, 0.865), (x - 0.030, 0.865)], dark)
        p.disc(x, 0.845, 0.026, hex2rgb('#0a0d12'))
        p.ring(x, 0.845, 0.030, 0.007, hex2rgb('#6d7b8c'))

    # main hull
    hull = [(0.50, 0.085), (0.575, 0.20), (0.605, 0.36), (0.615, 0.56), (0.585, 0.74),
            (0.520, 0.83), (0.480, 0.83), (0.415, 0.74), (0.385, 0.56), (0.395, 0.36),
            (0.425, 0.20)]
    p.poly(hull, body)

    # wings / mining arms
    for side in (-1, 1):
        wing = [(0.5 + side * 0.10, 0.34), (0.5 + side * 0.315, 0.545),
                (0.5 + side * 0.300, 0.635), (0.5 + side * 0.135, 0.60),
                (0.5 + side * 0.095, 0.50)]
        p.poly(wing if side > 0 else wing, body2)
        # wing accent stripe
        stripe = [(0.5 + side * 0.13, 0.385), (0.5 + side * 0.285, 0.545),
                  (0.5 + side * 0.283, 0.575), (0.5 + side * 0.128, 0.425)]
        p.poly(stripe, accent)
        # mining arm
        arm = [(0.5 + side * 0.085, 0.16), (0.5 + side * 0.145, 0.245),
               (0.5 + side * 0.132, 0.335), (0.5 + side * 0.075, 0.30)]
        p.poly(arm, dark)
        p.poly([(0.5 + side * 0.098, 0.175), (0.5 + side * 0.128, 0.235),
                (0.5 + side * 0.120, 0.265), (0.5 + side * 0.092, 0.215)], body2)
        # emitter tip
        p.disc(0.5 + side * 0.108, 0.168, 0.019, hex2rgb('#0a0f16'))
        p.ring(0.5 + side * 0.108, 0.168, 0.021, 0.008, cyan)
        # wing tip light
        p.disc(0.5 + side * 0.300, 0.590, 0.016, hex2rgb('#20150a'))
        p.disc(0.5 + side * 0.300, 0.590, 0.009, accent)

    # nose cone
    nose = [(0.5, 0.085), (0.548, 0.185), (0.452, 0.185)]
    p.poly(nose, body2)
    p.poly([(0.5, 0.10), (0.533, 0.175), (0.467, 0.175)], body)

    # spine plating + seams
    for yy in (0.26, 0.365, 0.47, 0.575, 0.68):
        p.polyline([(0.415, yy), (0.585, yy)], 0.0035, SEAM, 0.55)
    p.polyline([(0.45, 0.20), (0.445, 0.78)], 0.003, SEAM, 0.35)
    p.polyline([(0.55, 0.20), (0.555, 0.78)], 0.003, SEAM, 0.35)

    # cargo pod detail
    p.poly([(0.462, 0.44), (0.538, 0.44), (0.545, 0.66), (0.455, 0.66)], body2)
    for i in range(4):
        yy = 0.46 + i * 0.05
        p.polyline([(0.465, yy), (0.535, yy)], 0.004, SEAM, 0.6)
    p.rect(0.472, 0.685, 0.056, 0.030, dark)

    # cockpit canopy
    canopy = [(0.5, 0.215), (0.545, 0.275), (0.535, 0.355), (0.5, 0.385),
              (0.465, 0.355), (0.455, 0.275)]
    p.poly(canopy, glass)
    p.polyline(canopy, 0.006, hex2rgb('#0a1620'), 0.9, closed=True)
    p.poly([(0.487, 0.235), (0.512, 0.255), (0.505, 0.325), (0.483, 0.30)], hex2rgb('#5fd7f0'), 0.55)

    # hull striping and greebles
    p.poly([(0.5, 0.40), (0.56, 0.42), (0.56, 0.435), (0.5, 0.415),
            (0.44, 0.435), (0.44, 0.42)], accent)
    for side in (-1, 1):
        for i in range(3):
            p.rect(0.5 + side * 0.16, 0.60 + i * 0.035, 0.030, 0.016, dark, 0.85)
    # scanner dome
    p.disc(0.5, 0.735, 0.030, dark)
    p.disc(0.5, 0.735, 0.019, hex2rgb('#1b3a4a'))
    p.ring(0.5, 0.735, 0.030, 0.006, hex2rgb('#7a8b9e'))
    # running lights
    for side in (-1, 1):
        p.disc(0.5 + side * 0.118, 0.475, 0.012, accent)
        p.disc(0.5 + side * 0.118, 0.475, 0.028, accent, 0.25, add=True)
    p.disc(0.5, 0.735, 0.012, cyan, 0.9, add=True)
    p.disc(0.5, 0.735, 0.036, cyan, 0.20, add=True)


# --------------------------------------------------------------------------- #
#  SHIP: BOREAS — heavy excavator
# --------------------------------------------------------------------------- #
def paint_boreas(p):
    body = hex2rgb('#b8bec4')
    body2 = hex2rgb('#7c848e')
    dark = hex2rgb('#2f353d')
    orange = hex2rgb('#ff8a2b')
    cyan = hex2rgb('#5ce1ff')
    glass = hex2rgb('#152b2e')
    rust = hex2rgb('#6b4a32')

    # ore bins
    for side in (-1, 1):
        bin_ = [(0.5 + side * 0.10, 0.40), (0.5 + side * 0.40, 0.50),
                (0.5 + side * 0.415, 0.79), (0.5 + side * 0.12, 0.79)]
        p.poly(bin_ if side > 0 else bin_, body2)
        p.poly([(0.5 + side * 0.14, 0.44), (0.5 + side * 0.375, 0.525),
                (0.5 + side * 0.385, 0.755), (0.5 + side * 0.155, 0.755)], rust, 0.55)
        for i in range(4):
            yy = 0.50 + i * 0.075
            p.polyline([(0.5 + side * 0.135, yy), (0.5 + side * 0.395, yy + 0.018)], 0.005, SEAM, 0.7)
        p.polyline([(0.5 + side * 0.27, 0.455), (0.5 + side * 0.285, 0.78)], 0.004, SEAM, 0.6)
        # engine block
        p.poly([(0.5 + side * 0.145, 0.72), (0.5 + side * 0.245, 0.72),
                (0.5 + side * 0.235, 0.855), (0.5 + side * 0.155, 0.855)], dark)
        p.disc(0.5 + side * 0.195, 0.855, 0.042, hex2rgb('#12161c'))
        p.ring(0.5 + side * 0.195, 0.855, 0.046, 0.008, hex2rgb('#8a94a0'))
        p.disc(0.5 + side * 0.195, 0.855, 0.020, orange, 0.85, add=True)

    # central hull
    hull = [(0.5, 0.10), (0.585, 0.19), (0.60, 0.42), (0.585, 0.72), (0.545, 0.82),
            (0.455, 0.82), (0.415, 0.72), (0.40, 0.42), (0.415, 0.19)]
    p.poly(hull, body)
    for yy in (0.24, 0.32, 0.55, 0.63, 0.71):
        p.polyline([(0.42, yy), (0.58, yy)], 0.004, SEAM, 0.55)

    # drill rig at the nose
    p.poly([(0.455, 0.19), (0.545, 0.19), (0.535, 0.09), (0.465, 0.09)], dark)
    drill = [(0.5, 0.012), (0.532, 0.075), (0.518, 0.10), (0.5, 0.085),
             (0.482, 0.10), (0.468, 0.075)]
    p.poly(drill, hex2rgb('#aab4c0'))
    p.polyline(drill, 0.005, SEAM, 0.9, closed=True)
    p.disc(0.5, 0.055, 0.011, cyan, 0.9, add=True)
    p.disc(0.5, 0.055, 0.040, cyan, 0.18, add=True)
    for i in range(3):
        p.polyline([(0.470 + i * 0.020, 0.095), (0.478 + i * 0.020, 0.16)], 0.004, orange, 0.8)

    # side stabilisers
    for side in (-1, 1):
        p.poly([(0.5 + side * 0.12, 0.24), (0.5 + side * 0.245, 0.30),
                (0.5 + side * 0.235, 0.34), (0.5 + side * 0.115, 0.30)], body2)
        p.polyline([(0.5 + side * 0.12, 0.245), (0.5 + side * 0.245, 0.305)], 0.005, orange, 0.7)

    # cockpit (twin viewports)
    for side in (-1, 1):
        p.poly([(0.5 + side * 0.035, 0.215), (0.5 + side * 0.085, 0.245),
                (0.5 + side * 0.080, 0.325), (0.5 + side * 0.032, 0.30)], glass)
        p.polyline([(0.5 + side * 0.035, 0.215), (0.5 + side * 0.085, 0.245),
                    (0.5 + side * 0.080, 0.325), (0.5 + side * 0.032, 0.30)], 0.005,
                   hex2rgb('#0a1418'), 0.9, closed=True)
    p.rect(0.47, 0.355, 0.06, 0.02, orange, 0.9)
    p.disc(0.5, 0.46, 0.026, dark)
    p.disc(0.5, 0.46, 0.014, hex2rgb('#5c1d12'))
    p.disc(0.5, 0.46, 0.040, orange, 0.16, add=True)


# --------------------------------------------------------------------------- #
#  SHIP: VANGUARD — strike miner
# --------------------------------------------------------------------------- #
def paint_vanguard(p):
    body = hex2rgb('#d5dbe2')
    body2 = hex2rgb('#8b939d')
    dark = hex2rgb('#242a33')
    red = hex2rgb('#ff4d5e')
    white = hex2rgb('#f4f8ff')
    glass = hex2rgb('#0e2233')
    steel = hex2rgb('#aab3c0')

    for side in (-1, 1):
        # swept wing
        wing = [(0.5 + side * 0.075, 0.36), (0.5 + side * 0.34, 0.62),
                (0.5 + side * 0.335, 0.70), (0.5 + side * 0.20, 0.73),
                (0.5 + side * 0.085, 0.56)]
        p.poly(wing, body2)
        p.poly([(0.5 + side * 0.10, 0.40), (0.5 + side * 0.315, 0.625),
                (0.5 + side * 0.312, 0.655), (0.5 + side * 0.108, 0.44)], red, 0.95)
        p.polyline([(0.5 + side * 0.09, 0.60), (0.5 + side * 0.30, 0.705)], 0.005, SEAM, 0.6)
        # wingtip pod
        p.poly([(0.5 + side * 0.30, 0.60), (0.5 + side * 0.352, 0.635),
                (0.5 + side * 0.352, 0.675), (0.5 + side * 0.30, 0.66)], steel)
        p.disc(0.5 + side * 0.338, 0.655, 0.014, red, 0.9, add=True)
        # gun barrels
        p.poly([(0.5 + side * 0.145, 0.16), (0.5 + side * 0.175, 0.17),
                (0.5 + side * 0.165, 0.40), (0.5 + side * 0.140, 0.40)], steel)
        p.poly([(0.5 + side * 0.148, 0.13), (0.5 + side * 0.172, 0.14),
                (0.5 + side * 0.170, 0.17), (0.5 + side * 0.146, 0.16)], dark)
        # engine nacelle
        p.poly([(0.5 + side * 0.115, 0.58), (0.5 + side * 0.215, 0.60),
                (0.5 + side * 0.225, 0.82), (0.5 + side * 0.125, 0.80)], body2)
        p.poly([(0.5 + side * 0.135, 0.80), (0.5 + side * 0.215, 0.82),
                (0.5 + side * 0.205, 0.865), (0.5 + side * 0.145, 0.845)], dark)
        p.disc(0.5 + side * 0.175, 0.838, 0.024, hex2rgb('#12161c'))
        p.ring(0.5 + side * 0.175, 0.838, 0.027, 0.006, steel)

    # fuselage
    hull = [(0.5, 0.055), (0.565, 0.19), (0.578, 0.40), (0.560, 0.62), (0.520, 0.78),
            (0.480, 0.78), (0.440, 0.62), (0.422, 0.40), (0.435, 0.19)]
    p.poly(hull, body)
    p.poly([(0.5, 0.055), (0.545, 0.16), (0.455, 0.16)], body2)
    for yy in (0.22, 0.30, 0.47, 0.56, 0.65):
        p.polyline([(0.44, yy), (0.56, yy)], 0.0035, SEAM, 0.5)
    p.polyline([(0.5, 0.185), (0.5, 0.78)], 0.003, SEAM, 0.35)

    # canopy
    canopy = [(0.5, 0.195), (0.538, 0.265), (0.530, 0.345), (0.5, 0.375),
              (0.470, 0.345), (0.462, 0.265)]
    p.poly(canopy, glass)
    p.polyline(canopy, 0.005, hex2rgb('#050b12'), 0.9, closed=True)
    p.poly([(0.492, 0.215), (0.516, 0.245), (0.508, 0.325), (0.486, 0.30)],
           hex2rgb('#6fe2ff'), 0.5)
    # spine plate
    p.poly([(0.478, 0.40), (0.522, 0.40), (0.532, 0.70), (0.468, 0.70)], white, 0.35)
    p.poly([(0.5, 0.42), (0.515, 0.45), (0.515, 0.66), (0.5, 0.68),
            (0.485, 0.66), (0.485, 0.45)], red)
    # vents
    for i in range(4):
        p.rect(0.5 - 0.055, 0.72 + i * 0.012, 0.11, 0.005, SEAM, 0.7)
    p.disc(0.5, 0.20, 0.010, red, 0.8, add=True)


# --------------------------------------------------------------------------- #
#  SHIP: CHIMERA — void-class hybrid
# --------------------------------------------------------------------------- #
def paint_chimera(p):
    body = hex2rgb('#3a3452')
    body2 = hex2rgb('#241f36')
    edge = hex2rgb('#7c6bb0')
    violet = hex2rgb('#b478ff')
    cyan = hex2rgb('#6ff2ff')
    glass = hex2rgb('#1a1030')
    steel = hex2rgb('#8d87a8')

    # three nacelles
    for side, sc in ((-1, 1.0), (1, 1.0), (0, 0.0)):
        if sc == 0.0:
            continue
        x = 0.5 + side * 0.215
        p.poly([(x - 0.06, 0.46), (x + 0.06, 0.46), (x + 0.075, 0.78),
                (x + 0.04, 0.87), (x - 0.04, 0.87), (x - 0.075, 0.78)], body2)
        p.polyline([(x - 0.06, 0.47), (x - 0.075, 0.78), (x - 0.04, 0.865)], 0.006, edge, 0.7)
        p.polyline([(x + 0.06, 0.47), (x + 0.075, 0.78), (x + 0.04, 0.865)], 0.006, edge, 0.7)
        p.disc(x, 0.845, 0.035, hex2rgb('#0b0a14'))
        p.ring(x, 0.845, 0.042, 0.010, violet, 0.9)
        p.disc(x, 0.845, 0.024, violet, 0.65, add=True)

    # wings: forward-swept blades
    for side in (-1, 1):
        wing = [(0.5 + side * 0.09, 0.30), (0.5 + side * 0.395, 0.395),
                (0.5 + side * 0.375, 0.475), (0.5 + side * 0.095, 0.44)]
        p.poly(wing, body)
        p.polyline([(0.5 + side * 0.10, 0.315), (0.5 + side * 0.39, 0.40)], 0.006, violet, 0.8)
        p.polyline([(0.5 + side * 0.10, 0.435), (0.5 + side * 0.372, 0.47)], 0.006, edge, 0.7)
        # blade emitter
        p.poly([(0.5 + side * 0.36, 0.40), (0.5 + side * 0.40, 0.415),
                (0.5 + side * 0.398, 0.47), (0.5 + side * 0.358, 0.462)], steel)
        p.disc(0.5 + side * 0.38, 0.44, 0.014, cyan, 0.95, add=True)
        p.disc(0.5 + side * 0.38, 0.44, 0.036, cyan, 0.16, add=True)

    # hull: angular organic
    hull = [(0.5, 0.05), (0.560, 0.16), (0.585, 0.34), (0.560, 0.55), (0.545, 0.79),
            (0.5, 0.865), (0.455, 0.79), (0.440, 0.55), (0.415, 0.34), (0.440, 0.16)]
    p.poly(hull, body)
    p.polyline(hull, 0.008, edge, 0.85, closed=True)
    # dark inner spine
    p.poly([(0.5, 0.09), (0.545, 0.20), (0.552, 0.44), (0.5, 0.60),
            (0.448, 0.44), (0.455, 0.20)], body2)
    # energy channels
    for side in (-1, 1):
        for i in range(3):
            y0 = 0.24 + i * 0.10
            p.polyline([(0.5 + side * 0.028, y0), (0.5 + side * 0.045, y0 + 0.05),
                        (0.5 + side * 0.030, y0 + 0.09)], 0.006, violet, 0.55, add=True)
    # canopy slit
    p.poly([(0.5, 0.185), (0.527, 0.245), (0.5, 0.30), (0.473, 0.245)], glass)
    p.polyline([(0.5, 0.185), (0.527, 0.245), (0.5, 0.30), (0.473, 0.245)], 0.005, violet, 0.9, closed=True)
    p.disc(0.5, 0.245, 0.035, cyan, 0.20, add=True)
    # armour ridges
    for i in range(5):
        yy = 0.34 + i * 0.085
        p.polyline([(0.455, yy), (0.5, yy + 0.03), (0.545, yy)], 0.004, SEAM, 0.65)
    p.disc(0.5, 0.80, 0.028, hex2rgb('#0d0b16'))
    p.ring(0.5, 0.80, 0.030, 0.006, edge, 0.8)
    p.disc(0.5, 0.80, 0.012, violet, 0.8, add=True)


SHIP_PAINTERS = [paint_prospector, paint_boreas, paint_vanguard, paint_chimera]


# --------------------------------------------------------------------------- #
#  ENEMIES
# --------------------------------------------------------------------------- #
def paint_scout(p):
    dark = hex2rgb('#3b2a2e')
    body = hex2rgb('#8b5a4e')
    hot = hex2rgb('#ff6a4d')
    steel = hex2rgb('#b9c2cc')
    # splayed legs
    for side in (-1, 1):
        p.poly([(0.5 + side * 0.12, 0.42), (0.5 + side * 0.38, 0.56),
                (0.5 + side * 0.36, 0.63), (0.5 + side * 0.13, 0.56)], body)
        p.disc(0.5 + side * 0.365, 0.60, 0.022, steel)
        p.disc(0.5 + side * 0.365, 0.60, 0.010, hot, 0.9, add=True)
        p.poly([(0.5 + side * 0.10, 0.52), (0.5 + side * 0.22, 0.62),
                (0.5 + side * 0.19, 0.70), (0.5 + side * 0.09, 0.64)], dark)
    # core body
    p.poly([(0.5, 0.20), (0.575, 0.34), (0.585, 0.60), (0.5, 0.76),
            (0.415, 0.60), (0.425, 0.34)], body)
    p.poly([(0.5, 0.26), (0.545, 0.37), (0.552, 0.57), (0.5, 0.69),
            (0.448, 0.57), (0.455, 0.37)], dark)
    p.disc(0.5, 0.47, 0.062, hex2rgb('#1a0e12'))
    p.ring(0.5, 0.47, 0.070, 0.010, hot, 0.95)
    p.disc(0.5, 0.47, 0.038, hot, 0.55, add=True)
    p.disc(0.5, 0.47, 0.085, hot, 0.14, add=True)
    p.polyline([(0.5, 0.20), (0.575, 0.34), (0.585, 0.60), (0.5, 0.76),
                (0.415, 0.60), (0.425, 0.34)], 0.008, hex2rgb('#e8b3a0'), 0.55, closed=True)
    # antenna
    p.polyline([(0.5, 0.20), (0.5, 0.09)], 0.006, steel, 0.9)
    p.disc(0.5, 0.085, 0.012, hot, 0.9, add=True)


def paint_raider(p):
    body = hex2rgb('#7d3b3b')
    body2 = hex2rgb('#4a2226')
    steel = hex2rgb('#aeb6c2')
    hot = hex2rgb('#ff4a3d')
    bone = hex2rgb('#d8d2c4')
    for side in (-1, 1):
        p.poly([(0.5 + side * 0.08, 0.30), (0.5 + side * 0.40, 0.46),
                (0.5 + side * 0.395, 0.54), (0.5 + side * 0.10, 0.50)], body)
        p.poly([(0.5 + side * 0.12, 0.34), (0.5 + side * 0.37, 0.475),
                (0.5 + side * 0.368, 0.50), (0.5 + side * 0.125, 0.40)], bone, 0.85)
        p.polyline([(0.5 + side * 0.09, 0.485), (0.5 + side * 0.39, 0.532)], 0.006, SEAM, 0.7)
        p.poly([(0.5 + side * 0.36, 0.45), (0.5 + side * 0.405, 0.47),
                (0.5 + side * 0.40, 0.545), (0.5 + side * 0.355, 0.53)], steel)
        # gun
        p.poly([(0.5 + side * 0.15, 0.12), (0.5 + side * 0.185, 0.135),
                (0.5 + side * 0.175, 0.34), (0.5 + side * 0.145, 0.33)], steel)
        p.disc(0.5 + side * 0.165, 0.125, 0.014, hot, 0.8, add=True)
    p.poly([(0.5, 0.10), (0.575, 0.26), (0.585, 0.52), (0.545, 0.68),
            (0.5, 0.74), (0.455, 0.68), (0.415, 0.52), (0.425, 0.26)], body)
    p.poly([(0.5, 0.155), (0.548, 0.28), (0.555, 0.50), (0.5, 0.66),
            (0.445, 0.50), (0.452, 0.28)], body2)
    p.poly([(0.5, 0.20), (0.53, 0.29), (0.5, 0.38), (0.47, 0.29)], hex2rgb('#2b1014'))
    p.polyline([(0.5, 0.10), (0.575, 0.26), (0.585, 0.52), (0.545, 0.68),
                (0.5, 0.74), (0.455, 0.68), (0.415, 0.52), (0.425, 0.26)], 0.007, bone, 0.5, closed=True)
    for i in range(3):
        p.polyline([(0.46, 0.44 + i * 0.06), (0.54, 0.44 + i * 0.06)], 0.004, SEAM, 0.6)
    p.disc(0.5, 0.60, 0.030, hex2rgb('#1a0a0c'))
    p.disc(0.5, 0.60, 0.016, hot, 0.9, add=True)
    p.disc(0.5, 0.60, 0.05, hot, 0.18, add=True)


def paint_heavy(p):
    body = hex2rgb('#6f6250')
    body2 = hex2rgb('#3d362c')
    steel = hex2rgb('#9aa3ad')
    warn = hex2rgb('#ffb02e')
    for side in (-1, 1):
        p.poly([(0.5 + side * 0.10, 0.28), (0.5 + side * 0.44, 0.36),
                (0.5 + side * 0.45, 0.72), (0.5 + side * 0.12, 0.78)], body)
        p.polyline([(0.5 + side * 0.14, 0.33), (0.5 + side * 0.42, 0.40),
                    (0.5 + side * 0.43, 0.70), (0.5 + side * 0.16, 0.75)], 0.006, SEAM, 0.6)
        for i in range(3):
            x = 0.5 + side * (0.20 + i * 0.07)
            p.disc(x, 0.52, 0.030, body2)
            p.ring(x, 0.52, 0.032, 0.006, steel, 0.75)
        # barrel
        p.poly([(0.5 + side * 0.20, 0.10), (0.5 + side * 0.255, 0.12),
                (0.5 + side * 0.245, 0.34), (0.5 + side * 0.195, 0.33)], steel)
        p.rect(0.5 + side * 0.205 - 0.012, 0.14, 0.024, 0.016, warn, 0.9)
        p.disc(0.5 + side * 0.222, 0.105, 0.016, warn, 0.6, add=True)
    p.poly([(0.5, 0.16), (0.60, 0.30), (0.615, 0.62), (0.56, 0.80),
            (0.5, 0.84), (0.44, 0.80), (0.385, 0.62), (0.40, 0.30)], body)
    p.poly([(0.5, 0.22), (0.565, 0.33), (0.575, 0.60), (0.5, 0.74),
            (0.425, 0.60), (0.435, 0.33)], body2)
    p.disc(0.5, 0.50, 0.085, hex2rgb('#16130e'))
    p.ring(0.5, 0.50, 0.092, 0.012, warn, 0.9)
    p.disc(0.5, 0.50, 0.050, warn, 0.35, add=True)
    p.disc(0.5, 0.50, 0.13, warn, 0.10, add=True)
    for i in range(4):
        p.arc(0.5, 0.50, 0.11, i * 1.57, i * 1.57 + 0.9, 0.008, steel, 0.7)
    p.polyline([(0.5, 0.16), (0.60, 0.30), (0.615, 0.62), (0.56, 0.80),
                (0.5, 0.84), (0.44, 0.80), (0.385, 0.62), (0.40, 0.30)], 0.008,
               hex2rgb('#c9b48a'), 0.5, closed=True)


def paint_hunter(p):
    body = hex2rgb('#4a2a52')
    body2 = hex2rgb('#2a1530')
    hot = hex2rgb('#ff56d8')
    steel = hex2rgb('#b98cc9')
    # four blades
    for side in (-1, 1):
        for k in range(2):
            y0 = 0.30 + k * 0.26
            p.poly([(0.5 + side * 0.07, y0), (0.5 + side * 0.40, y0 + 0.14),
                    (0.5 + side * 0.38, y0 + 0.20), (0.5 + side * 0.075, y0 + 0.11)], body)
            p.polyline([(0.5 + side * 0.08, y0 + 0.005), (0.5 + side * 0.395, y0 + 0.145)],
                       0.006, hot, 0.8)
            p.disc(0.5 + side * 0.38, y0 + 0.17, 0.014, hot, 0.85, add=True)
    p.poly([(0.5, 0.12), (0.565, 0.26), (0.575, 0.52), (0.54, 0.74),
            (0.5, 0.80), (0.46, 0.74), (0.425, 0.52), (0.435, 0.26)], body)
    p.poly([(0.5, 0.18), (0.54, 0.28), (0.545, 0.50), (0.5, 0.70),
            (0.455, 0.50), (0.46, 0.28)], body2)
    p.polyline([(0.5, 0.12), (0.565, 0.26), (0.575, 0.52), (0.54, 0.74),
                (0.5, 0.80), (0.46, 0.74), (0.425, 0.52), (0.435, 0.26)], 0.007, steel, 0.6, closed=True)
    # eye
    p.poly([(0.5, 0.30), (0.535, 0.38), (0.5, 0.46), (0.465, 0.38)], hex2rgb('#1c0a22'))
    p.disc(0.5, 0.38, 0.020, hot, 0.95, add=True)
    p.disc(0.5, 0.38, 0.055, hot, 0.22, add=True)
    p.disc(0.5, 0.64, 0.026, hex2rgb('#150a1a'))
    p.ring(0.5, 0.64, 0.032, 0.008, hot, 0.8)
    p.disc(0.5, 0.64, 0.014, hot, 0.7, add=True)


def paint_sentinel(p):
    body = hex2rgb('#2b2547')
    body2 = hex2rgb('#181330')
    edge = hex2rgb('#9f7bff')
    glow = hex2rgb('#c9a4ff')
    steel = hex2rgb('#7d74a8')
    # outer halo ring
    p.ring(0.5, 0.5, 0.40, 0.016, edge, 0.55)
    p.ring(0.5, 0.5, 0.33, 0.008, edge, 0.35)
    for i in range(6):
        a = i * math.tau / 6 + 0.26
        x0, y0 = 0.5 + math.cos(a) * 0.33, 0.5 + math.sin(a) * 0.33
        x1, y1 = 0.5 + math.cos(a) * 0.40, 0.5 + math.sin(a) * 0.40
        p.polyline([(x0, y0), (x1, y1)], 0.010, steel, 0.8)
    # spikes
    for i in range(6):
        a = i * math.tau / 6 + 0.52
        pts = [(0.5 + math.cos(a) * 0.12, 0.5 + math.sin(a) * 0.12),
               (0.5 + math.cos(a + 0.18) * 0.34, 0.5 + math.sin(a + 0.18) * 0.34),
               (0.5 + math.cos(a + 0.42) * 0.30, 0.5 + math.sin(a + 0.42) * 0.30)]
        p.poly(pts, body)
        p.polyline(pts, 0.006, edge, 0.7)
    # core
    p.disc(0.5, 0.5, 0.215, body2)
    p.ring(0.5, 0.5, 0.215, 0.014, edge, 0.9)
    p.disc(0.5, 0.5, 0.155, hex2rgb('#0d0a18'))
    p.ring(0.5, 0.5, 0.155, 0.008, edge, 0.6)
    p.disc(0.5, 0.5, 0.075, glow, 0.75, add=True)
    p.disc(0.5, 0.5, 0.135, glow, 0.22, add=True)
    p.disc(0.5, 0.5, 0.26, glow, 0.08, add=True)
    for i in range(3):
        p.arc(0.5, 0.5, 0.185, i * 2.09, i * 2.09 + 1.4, 0.008, glow, 0.7)


ENEMY_PAINTERS = [paint_scout, paint_raider, paint_heavy, paint_hunter, paint_sentinel]


# --------------------------------------------------------------------------- #
#  DRONES
# --------------------------------------------------------------------------- #
def paint_harvester(p):
    body = hex2rgb('#c9d4e2')
    body2 = hex2rgb('#7f8b9c')
    dark = hex2rgb('#2c333d')
    cyan = hex2rgb('#46e6ff')
    for side in (-1, 1):
        p.poly([(0.5 + side * 0.10, 0.34), (0.5 + side * 0.38, 0.44),
                (0.5 + side * 0.36, 0.52), (0.5 + side * 0.11, 0.48)], body2)
        p.disc(0.5 + side * 0.34, 0.48, 0.030, dark)
        p.ring(0.5 + side * 0.34, 0.48, 0.032, 0.007, cyan)
    p.poly([(0.5, 0.24), (0.575, 0.36), (0.575, 0.62), (0.5, 0.74),
            (0.425, 0.62), (0.425, 0.36)], body)
    p.polyline([(0.5, 0.24), (0.575, 0.36), (0.575, 0.62), (0.5, 0.74),
                (0.425, 0.62), (0.425, 0.36)], 0.008, dark, 0.8, closed=True)
    p.poly([(0.5, 0.30), (0.545, 0.40), (0.545, 0.58), (0.5, 0.68),
            (0.455, 0.58), (0.455, 0.40)], hex2rgb('#22303f'))
    p.disc(0.5, 0.49, 0.045, cyan, 0.8, add=True)
    p.disc(0.5, 0.49, 0.10, cyan, 0.15, add=True)
    p.disc(0.5, 0.49, 0.024, hex2rgb('#0a1420'))
    p.ring(0.5, 0.49, 0.024, 0.007, cyan)
    # cargo hopper
    p.poly([(0.455, 0.66), (0.545, 0.66), (0.528, 0.76), (0.472, 0.76)], dark)
    p.polyline([(0.455, 0.66), (0.472, 0.76), (0.528, 0.76), (0.545, 0.66)], 0.006, body2, 0.9)
    p.disc(0.5, 0.86, 0.014, cyan, 0.7, add=True)


def paint_excavator(p):
    body = hex2rgb('#d0a870')
    body2 = hex2rgb('#8b6a3e')
    dark = hex2rgb('#2e2a22')
    orange = hex2rgb('#ffb347')
    for side in (-1, 1):
        p.poly([(0.5 + side * 0.12, 0.28), (0.5 + side * 0.44, 0.36),
                (0.5 + side * 0.44, 0.60), (0.5 + side * 0.12, 0.68)], body2)
        p.polyline([(0.5 + side * 0.14, 0.33), (0.5 + side * 0.42, 0.40),
                    (0.5 + side * 0.42, 0.57), (0.5 + side * 0.14, 0.63)], 0.006, dark, 0.7)
        p.disc(0.5 + side * 0.37, 0.48, 0.042, dark)
        p.ring(0.5 + side * 0.37, 0.48, 0.045, 0.008, orange)
        p.disc(0.5 + side * 0.37, 0.48, 0.020, orange, 0.5, add=True)
    p.poly([(0.5, 0.20), (0.60, 0.32), (0.60, 0.66), (0.5, 0.78),
            (0.40, 0.66), (0.40, 0.32)], body)
    p.poly([(0.5, 0.26), (0.565, 0.36), (0.565, 0.62), (0.5, 0.72),
            (0.435, 0.62), (0.435, 0.36)], body2)
    p.polyline([(0.5, 0.20), (0.60, 0.32), (0.60, 0.66), (0.5, 0.78),
                (0.40, 0.66), (0.40, 0.32)], 0.008, dark, 0.85, closed=True)
    # cutter head
    p.disc(0.5, 0.36, 0.055, hex2rgb('#20201a'))
    p.ring(0.5, 0.36, 0.058, 0.010, orange)
    for i in range(6):
        a = i * math.tau / 6
        p.polyline([(0.5 + math.cos(a) * 0.02, 0.36 + math.sin(a) * 0.02),
                    (0.5 + math.cos(a) * 0.075, 0.36 + math.sin(a) * 0.075)], 0.008,
                   hex2rgb('#c8ccd4'), 0.9)
    p.disc(0.5, 0.36, 0.10, orange, 0.12, add=True)
    p.rect(0.46, 0.50, 0.08, 0.10, dark)
    p.rect(0.47, 0.52, 0.06, 0.02, orange, 0.8)
    p.rect(0.47, 0.56, 0.06, 0.02, orange, 0.8)


DRONE_PAINTERS = [paint_harvester, paint_excavator]


# --------------------------------------------------------------------------- #
#  baking
# --------------------------------------------------------------------------- #
def bake(painter_fn, size, frames=FRAMES):
    p = Painter(size)
    painter_fn(p)
    rgb, a = p.compose()
    rgb, a = p.finish(rgb, a)
    rgb = clamp01(rgb)
    img = p.to_image(rgb, a)
    strip = Image.new('RGBA', (size * frames, size), (0, 0, 0, 0))
    for k in range(frames):
        ang = -360.0 * k / frames
        strip.paste(img.rotate(ang, resample=Image.BICUBIC, expand=False), (k * size, 0))
    return strip


if __name__ == '__main__':
    import sys, os
    out = sys.argv[1] if len(sys.argv) > 1 else 'assets/gen'
    os.makedirs(out, exist_ok=True)
    names = ['ship_prospector', 'ship_boreas', 'ship_vanguard', 'ship_chimera']
    for i, fn in enumerate(SHIP_PAINTERS):
        bake(fn, 168).save(f'{out}/{names[i]}.png')
        print(names[i])
    enames = ['enemy_scout', 'enemy_raider', 'enemy_heavy', 'enemy_hunter', 'enemy_sentinel']
    for i, fn in enumerate(ENEMY_PAINTERS):
        bake(fn, 136).save(f'{out}/{enames[i]}.png')
        print(enames[i])
    dnames = ['drone_harvester', 'drone_excavator']
    for i, fn in enumerate(DRONE_PAINTERS):
        bake(fn, 84).save(f'{out}/{dnames[i]}.png')
        print(dnames[i])
