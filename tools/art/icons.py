"""SPACE MINER — icons, logo and UI furniture (all drawn procedurally)."""
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from artlib import clamp01, hex2rgb, smoothstep, value_noise, radial
from ships import Painter

RES = [
    # name, colour, accent, shape
    ('res_iron',       '#8f9aa8', '#d6e0ec', 'chunk'),
    ('res_copper',     '#c9773f', '#ffb27a', 'chunk'),
    ('res_nickel',     '#9fb3ad', '#d9fff2', 'chunk'),
    ('res_ice',        '#8fd0f5', '#e2f7ff', 'crystal'),
    ('res_titanium',   '#c3cad4', '#ffffff', 'ingot'),
    ('res_silver',     '#dde6f0', '#ffffff', 'ingot'),
    ('res_salvage',    '#b08050', '#e8c193', 'shard'),
    ('res_gold',       '#ffcc55', '#fff2b0', 'ingot'),
    ('res_iridium',    '#c9ecff', '#ffffff', 'shard'),
    ('res_platinum',   '#e9f4ff', '#ffffff', 'ingot'),
    ('res_uranium',    '#8cff6a', '#e2ffb0', 'canister'),
    ('res_crystal',    '#6ee8ff', '#d6ffff', 'crystal'),
    ('res_plasma',     '#ff7a3c', '#ffd6a8', 'canister'),
    ('res_antimatter', '#ff4fd8', '#ffc2f2', 'sphere'),
    ('res_voidcrystal','#b48cff', '#e8d9ff', 'crystal'),
    ('res_darkmatter', '#7e60ff', '#c9b8ff', 'sphere'),
]


def icon_chunk(p, base, acc, seed):
    rng = np.random.default_rng(seed)
    # irregular ore lump
    pts = []
    n = 9
    for i in range(n):
        a = i / n * math.tau - 0.4
        rr = 0.26 + rng.random() * 0.09
        pts.append((0.5 + math.cos(a) * rr, 0.5 + math.sin(a) * rr * 0.95))
    shade = hex2rgb('#000000')
    dark = np.clip(np.asarray(base) * 0.55 - 0.02, 0, 1)
    p.poly(pts, dark)
    inner = [(0.5 + (x - 0.5) * 0.72 + 0.012, 0.5 + (y - 0.5) * 0.72 - 0.02) for x, y in pts]
    p.poly(inner, base)
    # facets
    for i in range(0, n, 2):
        q = [inner[i], inner[(i + 1) % n], (0.5, 0.5)]
        p.poly(q, np.clip(np.asarray(base) * 1.15, 0, 1), 0.55)
    # mineral glints
    for i in range(4):
        a = rng.random() * math.tau
        rr = rng.random() * 0.16
        p.disc(0.5 + math.cos(a) * rr, 0.5 + math.sin(a) * rr, 0.018 + rng.random() * 0.012,
               acc, 0.9)
    p.polyline(pts, 0.010, np.clip(np.asarray(base) * 0.35, 0, 1), 0.9, closed=True)


def icon_crystal(p, base, acc, seed):
    rng = np.random.default_rng(seed)
    shards = [(-0.10, 0.34, 0.30, -0.10), (0.10, 0.28, 0.26, 0.12), (0.0, 0.40, 0.00, -0.02)]
    for cx, h, w, lean in shards:
        x = 0.5 + cx
        top = 0.5 - h * 0.9
        bot = 0.5 + 0.26
        pts = [(x + lean * 0.10, top), (x + w * 0.5, top + h * 0.55),
               (x + w * 0.40, bot), (x - w * 0.40, bot), (x - w * 0.5, top + h * 0.55)]
        p.poly(pts, np.asarray(base) * 0.8)
        p.poly([pts[0], pts[1], (x, top + h * 0.6)], acc, 0.55)
        p.poly([pts[0], pts[4], (x, top + h * 0.6)], np.clip(np.asarray(base) * 1.25, 0, 1), 0.5)
        p.polyline(pts, 0.009, np.clip(np.asarray(base) * 0.45, 0, 1), 0.95, closed=True)
    p.disc(0.5, 0.52, 0.10, acc, 0.28, add=True)
    p.disc(0.5, 0.52, 0.035, acc, 0.45, add=True)


def icon_ingot(p, base, acc, seed):
    # stacked bars, isometric
    for i, (dy, sc) in enumerate([(0.0, 1.0), (-0.16, 0.86)]):
        y = 0.58 + dy
        w = 0.30 * sc
        top = [(0.5 - w, y - 0.10), (0.5 - w + 0.07, y - 0.19),
               (0.5 + w + 0.07, y - 0.19), (0.5 + w, y - 0.10)]
        left = [(0.5 - w, y - 0.10), (0.5 - w, y + 0.04), (0.5 + w, y + 0.04), (0.5 + w, y - 0.10)]
        p.poly(left, np.asarray(base) * 0.62)
        p.poly(top, np.clip(np.asarray(base) * 1.12, 0, 1))
        p.polyline(top, 0.008, acc, 0.6, closed=True)
    p.polyline([(0.5 - 0.10, 0.70), (0.5 + 0.10, 0.70)], 0.010, acc, 0.5)
    p.disc(0.5, 0.34, 0.06, acc, 0.18, add=True)


def icon_shard(p, base, acc, seed):
    rng = np.random.default_rng(seed)
    pts = [(0.5, 0.14), (0.72, 0.42), (0.66, 0.74), (0.5, 0.86), (0.30, 0.70), (0.26, 0.40)]
    p.poly(pts, np.asarray(base) * 0.78)
    p.poly([pts[0], pts[1], (0.52, 0.55), (0.44, 0.30)], np.clip(np.asarray(base) * 1.2, 0, 1), 0.75)
    p.poly([pts[0], pts[5], (0.44, 0.55), (0.44, 0.30)], acc, 0.45)
    p.polyline(pts, 0.010, np.clip(np.asarray(base) * 0.4, 0, 1), 0.9, closed=True)
    # scrap edges
    for i in range(3):
        a = rng.random()
        p.polyline([(0.33 + a * 0.3, 0.78), (0.38 + a * 0.28, 0.86)], 0.008, acc, 0.5)


def icon_canister(p, base, acc, seed):
    # fuel cell: rounded canister with energy window
    p.poly([(0.36, 0.20), (0.64, 0.20), (0.66, 0.76), (0.34, 0.76)], np.asarray(base) * 0.55)
    p.poly([(0.40, 0.24), (0.60, 0.24), (0.615, 0.72), (0.385, 0.72)], np.asarray(base) * 0.8)
    p.rect(0.42, 0.38, 0.16, 0.26, acc, 0.85)
    p.rect(0.42, 0.38, 0.16, 0.26, acc, 0.25, add=True)
    p.rect(0.43, 0.13, 0.14, 0.07, np.asarray(base) * 0.45)
    p.rect(0.43, 0.735, 0.14, 0.06, np.asarray(base) * 0.45)
    p.polyline([(0.36, 0.20), (0.34, 0.76), (0.66, 0.76), (0.64, 0.20)], 0.010, acc, 0.7)
    for i in range(3):
        p.polyline([(0.46, 0.42 + i * 0.05), (0.54, 0.42 + i * 0.05)], 0.006, np.asarray(base) * 0.3, 0.8)
    p.disc(0.5, 0.51, 0.16, acc, 0.14, add=True)


def icon_sphere(p, base, acc, seed):
    # contained orb with containment rings
    p.disc(0.5, 0.5, 0.26, np.asarray(base) * 0.70)
    p.disc(0.5, 0.5, 0.20, np.asarray(base) * 0.95)
    p.disc(0.47, 0.45, 0.10, acc, 0.55)
    p.disc(0.5, 0.5, 0.30, base, 0.20, add=True)
    p.arc(0.5, 0.5, 0.31, 0.2, 3.0, 0.012, acc, 0.8)
    p.arc(0.5, 0.5, 0.31, 3.4, 5.9, 0.012, acc, 0.8)
    p.ring(0.5, 0.5, 0.345, 0.010, acc, 0.55)
    p.arc(0.5, 0.5, 0.345, 0.6, 2.2, 0.012, np.array([1, 1, 1]), 0.85)


SHAPES = {'chunk': icon_chunk, 'crystal': icon_crystal, 'ingot': icon_ingot,
          'shard': icon_shard, 'canister': icon_canister, 'sphere': icon_sphere}


def make_resource_icon(base, acc, shape, seed, size=128):
    p = Painter(size, ss=3)
    SHAPES[shape](p, np.asarray(hex2rgb(base)), np.asarray(hex2rgb(acc)), seed)
    # subtle glow puck behind
    rgb, a = p.compose()
    rgb, a = p.finish(rgb, a, light=(-0.5, -0.6))
    return p.to_image(clamp01(rgb), a)


# --------------------------------------------------------------------------- #
def glyph_icon(kind, size=96):
    """Monochrome-ish white icons for the UI (tinted at runtime)."""
    p = Painter(size, ss=3)
    W = np.array([1.0, 1.0, 1.0])
    G = np.array([0.72, 0.80, 0.9])
    if kind == 'upg_laser':
        p.poly([(0.5, 0.10), (0.58, 0.30), (0.56, 0.62), (0.44, 0.62), (0.42, 0.30)], W)
        p.polyline([(0.5, 0.10), (0.42, 0.30)], 0.012, G, 0.8)
        p.poly([(0.30, 0.58), (0.42, 0.62), (0.42, 0.72), (0.30, 0.72)], G)
        p.poly([(0.70, 0.58), (0.58, 0.62), (0.58, 0.72), (0.70, 0.72)], G)
        p.disc(0.5, 0.78, 0.10, W, 0.85)
        p.ring(0.5, 0.78, 0.10, 0.012, G)
        p.disc(0.5, 0.78, 0.20, W, 0.18, add=True)
    elif kind == 'upg_speed':
        for i in range(3):
            t = i * 0.16
            p.poly([(0.16 + t, 0.30 + i * 0.16), (0.66 - t * 0.5, 0.22 + i * 0.16),
                    (0.86 - t * 0.5, 0.30 + i * 0.16), (0.66 - t * 0.5, 0.38 + i * 0.16)], W, 0.9 - i * 0.2)
    elif kind == 'upg_cargo':
        p.poly([(0.20, 0.30), (0.80, 0.30), (0.80, 0.78), (0.20, 0.78)], G)
        p.polyline([(0.20, 0.30), (0.20, 0.78), (0.80, 0.78), (0.80, 0.30)], 0.014, W, 0.9)
        p.polyline([(0.20, 0.30), (0.80, 0.30)], 0.014, W, 0.9)
        p.rect(0.30, 0.42, 0.16, 0.24, W, 0.8)
        p.rect(0.54, 0.42, 0.16, 0.10, W, 0.55)
    elif kind == 'upg_engine':
        p.poly([(0.34, 0.16), (0.66, 0.16), (0.72, 0.48), (0.62, 0.58), (0.38, 0.58), (0.28, 0.48)], G)
        p.polyline([(0.34, 0.16), (0.28, 0.48), (0.38, 0.58)], 0.014, W, 0.9)
        p.polyline([(0.66, 0.16), (0.72, 0.48), (0.62, 0.58)], 0.014, W, 0.9)
        p.poly([(0.40, 0.60), (0.60, 0.60), (0.54, 0.86), (0.46, 0.86)], W, 0.9)
        p.disc(0.5, 0.80, 0.16, W, 0.14, add=True)
    elif kind == 'upg_accel':
        for i in range(3):
            y = 0.30 + i * 0.18
            p.poly([(0.20, y), (0.60, y + 0.02), (0.80, y + 0.06), (0.60, y + 0.10), (0.20, y + 0.10)], W, 0.9 - i * 0.15)
    elif kind == 'upg_hull':
        p.poly([(0.5, 0.12), (0.82, 0.26), (0.76, 0.62), (0.5, 0.86), (0.24, 0.62), (0.18, 0.26)], G, 0.9)
        p.polyline([(0.5, 0.12), (0.18, 0.26), (0.24, 0.62), (0.5, 0.86), (0.76, 0.62), (0.82, 0.26)], 0.014, W, 0.9, closed=True)
        p.polyline([(0.5, 0.22), (0.5, 0.74)], 0.012, W, 0.6)
        p.polyline([(0.30, 0.40), (0.70, 0.40)], 0.010, W, 0.4)
    elif kind == 'upg_shield':
        p.poly([(0.5, 0.12), (0.80, 0.24), (0.78, 0.58), (0.5, 0.86), (0.22, 0.58), (0.20, 0.24)], G, 0.55)
        p.polyline([(0.5, 0.12), (0.20, 0.24), (0.22, 0.58), (0.5, 0.86), (0.78, 0.58), (0.80, 0.24)], 0.016, W, 1.0, closed=True)
        p.arc(0.5, 0.48, 0.22, 0.4, 2.6, 0.014, W, 0.8)
    elif kind == 'upg_energy':
        p.poly([(0.56, 0.10), (0.30, 0.52), (0.48, 0.52), (0.42, 0.90), (0.70, 0.44), (0.52, 0.44)], W)
        p.disc(0.5, 0.5, 0.34, W, 0.12, add=True)
    elif kind == 'upg_scanner':
        for r in (0.14, 0.26, 0.38):
            p.arc(0.5, 0.66, r, math.pi * 1.15, math.pi * 1.85, 0.016, W, 0.9)
        p.poly([(0.5, 0.66), (0.62, 0.30), (0.38, 0.30)], W, 0.25)
        p.disc(0.5, 0.66, 0.035, W)
    elif kind == 'upg_drone':
        p.poly([(0.5, 0.24), (0.68, 0.40), (0.68, 0.66), (0.5, 0.78), (0.32, 0.66), (0.32, 0.40)], G)
        p.polyline([(0.5, 0.24), (0.68, 0.40), (0.68, 0.66), (0.5, 0.78), (0.32, 0.66), (0.32, 0.40)], 0.014, W, 0.95, closed=True)
        for x in (0.20, 0.80):
            p.ring(x, 0.40, 0.10, 0.012, W, 0.85)
            p.polyline([(0.32, 0.42), (0.27, 0.40)], 0.012, W, 0.7)
            p.polyline([(0.68, 0.42), (0.73, 0.40)], 0.012, W, 0.7)
        p.disc(0.5, 0.50, 0.06, W, 0.9)
    elif kind == 'upg_weapon':
        p.poly([(0.5, 0.10), (0.56, 0.34), (0.56, 0.70), (0.44, 0.70), (0.44, 0.34)], W)
        p.poly([(0.44, 0.70), (0.34, 0.82), (0.40, 0.86), (0.50, 0.74)], G)
        p.poly([(0.56, 0.70), (0.66, 0.82), (0.60, 0.86), (0.50, 0.74)], G)
        p.rect(0.40, 0.40, 0.20, 0.06, G, 0.9)
    elif kind == 'upg_tractor':
        p.ring(0.5, 0.5, 0.14, 0.014, W, 0.9)
        p.ring(0.5, 0.5, 0.26, 0.012, W, 0.65)
        p.ring(0.5, 0.5, 0.38, 0.012, W, 0.4)
        for a in range(4):
            ang = a * math.tau / 4 + 0.4
            p.polyline([(0.5 + math.cos(ang) * 0.30, 0.5 + math.sin(ang) * 0.30),
                        (0.5 + math.cos(ang) * 0.42, 0.5 + math.sin(ang) * 0.42)], 0.012, W, 0.6)
        p.disc(0.5, 0.5, 0.07, W, 0.9)
    elif kind == 'ui_drone':
        p.poly([(0.5, 0.22), (0.72, 0.38), (0.72, 0.68), (0.5, 0.82), (0.28, 0.68), (0.28, 0.38)], W, 0.85)
        p.polyline([(0.5, 0.22), (0.72, 0.38), (0.72, 0.68), (0.5, 0.82), (0.28, 0.68), (0.28, 0.38)], 0.012, G, 1.0, closed=True)
        p.disc(0.5, 0.50, 0.09, G)
    elif kind == 'ui_ship':
        p.poly([(0.5, 0.10), (0.60, 0.34), (0.80, 0.56), (0.80, 0.66), (0.58, 0.62),
                (0.56, 0.82), (0.44, 0.82), (0.42, 0.62), (0.20, 0.66), (0.20, 0.56),
                (0.40, 0.34)], W, 0.9)
        p.polyline([(0.5, 0.10), (0.60, 0.34), (0.80, 0.56), (0.80, 0.66), (0.58, 0.62),
                    (0.56, 0.82), (0.44, 0.82), (0.42, 0.62), (0.20, 0.66), (0.20, 0.56),
                    (0.40, 0.34)], 0.012, G, 1.0, closed=True)
        p.poly([(0.5, 0.24), (0.55, 0.34), (0.45, 0.34)], G, 0.9)
    elif kind == 'ui_cargo':
        p.poly([(0.18, 0.24), (0.82, 0.24), (0.86, 0.82), (0.14, 0.82)], G, 0.85)
        p.polyline([(0.18, 0.24), (0.14, 0.82), (0.86, 0.82), (0.82, 0.24)], 0.014, W, 1.0)
        p.polyline([(0.18, 0.24), (0.82, 0.24)], 0.014, W, 1.0)
        p.polyline([(0.26, 0.34), (0.30, 0.74)], 0.010, W, 0.5)
        p.polyline([(0.74, 0.34), (0.70, 0.74)], 0.010, W, 0.5)
    elif kind == 'ui_mining':
        p.poly([(0.5, 0.14), (0.60, 0.40), (0.58, 0.66), (0.42, 0.66), (0.40, 0.40)], W, 0.9)
        p.polyline([(0.5, 0.14), (0.40, 0.40), (0.42, 0.66)], 0.012, G, 0.9)
        p.polyline([(0.5, 0.14), (0.60, 0.40), (0.58, 0.66)], 0.012, G, 0.9)
        for a in (0.6, 1.2, 1.9):
            p.polyline([(0.5 + math.cos(a) * 0.14, 0.86 - math.sin(a) * 0.0),
                        (0.5 + math.cos(a) * 0.22, 0.90)], 0.012, W, 0.7)
    elif kind == 'ui_shield':
        p.arc(0.5, 0.52, 0.30, 1.1, 2.05, 0.020, W, 1.0)
        p.arc(0.5, 0.52, 0.30, 4.2, 5.15, 0.014, W, 0.6)
        p.poly([(0.5, 0.26), (0.68, 0.34), (0.66, 0.58), (0.5, 0.72), (0.34, 0.58), (0.32, 0.34)], G, 0.5)
    elif kind == 'ui_energy':
        p.poly([(0.58, 0.08), (0.30, 0.54), (0.48, 0.54), (0.40, 0.92), (0.72, 0.44), (0.52, 0.44)], W)
    elif kind == 'ui_skull':
        p.disc(0.5, 0.42, 0.24, W, 0.95)
        p.poly([(0.34, 0.56), (0.66, 0.56), (0.62, 0.82), (0.38, 0.82)], W, 0.95)
        p.disc(0.42, 0.40, 0.065, np.zeros(3))
        p.disc(0.58, 0.40, 0.065, np.zeros(3))
        p.poly([(0.5, 0.48), (0.545, 0.56), (0.455, 0.56)], np.zeros(3))
        for i in range(3):
            p.rect(0.42 + i * 0.06, 0.66, 0.035, 0.14, np.zeros(3))
    elif kind == 'ui_star':
        pts = []
        for i in range(10):
            a = -math.pi / 2 + i * math.pi / 5
            rr = 0.42 if i % 2 == 0 else 0.18
            pts.append((0.5 + math.cos(a) * rr, 0.5 + math.sin(a) * rr))
        p.poly(pts, W)
    elif kind == 'ui_arrow':
        p.poly([(0.5, 0.14), (0.86, 0.62), (0.62, 0.62), (0.62, 0.86), (0.38, 0.86),
                (0.38, 0.62), (0.14, 0.62)], W)
    elif kind == 'ui_lock':
        p.arc(0.5, 0.42, 0.18, math.pi, math.tau, 0.05, W, 0.95)
        p.poly([(0.22, 0.44), (0.78, 0.44), (0.78, 0.86), (0.22, 0.86)], W, 0.95)
        p.disc(0.5, 0.62, 0.07, np.zeros(3))
        p.rect(0.475, 0.62, 0.05, 0.14, np.zeros(3))
    elif kind == 'ui_cross':
        p.ring(0.5, 0.5, 0.30, 0.016, W, 0.9)
        for a in range(4):
            ang = a * math.tau / 4
            p.polyline([(0.5 + math.cos(ang) * 0.14, 0.5 + math.sin(ang) * 0.14),
                        (0.5 + math.cos(ang) * 0.40, 0.5 + math.sin(ang) * 0.40)], 0.014, W, 0.8)
        p.disc(0.5, 0.5, 0.035, W)
    elif kind == 'ui_warn':
        p.poly([(0.5, 0.10), (0.92, 0.84), (0.08, 0.84)], W, 0.95)
        p.poly([(0.5, 0.26), (0.80, 0.76), (0.20, 0.76)], np.zeros(3))
        p.rect(0.472, 0.40, 0.056, 0.20, np.zeros(3))
        p.disc(0.5, 0.68, 0.032, np.zeros(3))
    elif kind == 'ui_ring':
        p.ring(0.5, 0.5, 0.40, 0.02, W, 0.8)
    elif kind == 'ui_glow':
        p.disc(0.5, 0.5, 0.10, W, 1.0)
        p.disc(0.5, 0.5, 0.28, W, 0.25, add=True)
    else:
        p.disc(0.5, 0.5, 0.3, W)
    rgb, a = p.compose()
    rgb = clamp01(rgb)
    return p.to_image(rgb, a)


def logo_art(w=760, h=260):
    """SPACEMINER wordmark: metallic fill, bevel, orbit accents."""
    from PIL import ImageFont
    tmp_w, tmp_h = w * 4, h * 4
    txt, txt2 = 'SPACE', 'MINER'
    meas = Image.new('L', (10, 10), 0)
    md0 = ImageDraw.Draw(meas)
    probe = ImageFont.truetype('fonts/Orbitron-ExtraBold.ttf', 200)
    b1 = md0.textbbox((0, 0), txt, font=probe)
    b2 = md0.textbbox((0, 0), txt2, font=probe)
    wprobe = (b1[2] - b1[0]) + (b2[2] - b2[0])
    size_px = int(200 * (tmp_w * 0.92) / max(1, wprobe))
    f1 = ImageFont.truetype('fonts/Orbitron-ExtraBold.ttf', size_px)
    f2 = ImageFont.truetype('fonts/Rajdhani-Medium.ttf', max(10, int(h * 0.105)))
    mask = Image.new('L', (tmp_w, tmp_h), 0)
    md = ImageDraw.Draw(mask)
    bb = md.textbbox((0, 0), txt, font=f1)
    bb2 = md.textbbox((0, 0), txt2, font=f1)
    gap = tmp_w * 0.008
    tw = (bb[2] - bb[0]) + gap + (bb2[2] - bb2[0])
    th = max(bb[3] - bb[1], bb2[3] - bb2[1])
    ox = (tmp_w - tw) / 2
    oy = (tmp_h * 0.50 - th / 2)
    md.text((ox - bb[0], oy - bb[1]), txt, font=f1, fill=255)
    md.text((ox + (bb[2] - bb[0]) + gap - bb2[0], oy - bb2[1]), txt2, font=f1, fill=255)
    mb = mask.getbbox()
    mask = mask.crop(mb)
    scale = min((w * 0.94) / mask.width, (h * 0.52) / mask.height)
    mask = mask.resize((max(1, int(mask.width * scale)), max(1, int(mask.height * scale))), Image.LANCZOS)
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    mw, mh = mask.size
    px0, py0 = (w - mw) // 2, int(h * 0.06)
    layer.paste(mask, (px0, py0))
    m = np.zeros((h, w))
    m[py0:py0 + mh, px0:px0 + mw] = np.asarray(mask, dtype=np.float64) / 255.0
    ys, xs = np.mgrid[0:h, 0:w]
    t = np.clip((ys - py0) / max(1, mh), 0, 1)
    top = np.array([1.00, 1.00, 1.00])
    mid = np.array([0.78, 0.86, 0.96])
    bot = np.array([0.30, 0.40, 0.56])
    grad = np.where(t[..., None] < 0.5,
                    top[None, None, :] * (1 - t[..., None] * 2) + mid[None, None, :] * (t[..., None] * 2),
                    mid[None, None, :] * (1 - (t[..., None] - 0.5) * 2) + bot[None, None, :] * ((t[..., None] - 0.5) * 2))
    sheen = np.exp(-(((xs / w - 0.44) ** 2) / 0.004)) * 0.45
    sheen += np.exp(-(((xs / w - 0.72) ** 2) / 0.002)) * 0.18
    rgb = grad + sheen[..., None]
    # bevel: bright top edge, warm bottom edge
    a_img = Image.fromarray((m * 255).astype(np.uint8), 'L')
    blur = np.asarray(a_img.filter(ImageFilter.GaussianBlur(1.6)), dtype=np.float64) / 255.0
    gy, gx = np.gradient(blur)
    ln = np.sqrt(gx * gx + gy * gy) + 1e-6
    ny = -gy / ln
    edge = np.clip(blur * (1 - blur) * 8, 0, 1) * np.clip(ln * 20, 0, 1)
    rgb = rgb + (edge * np.clip(-ny, 0, 1))[..., None] * np.array([0.9, 0.95, 1.0]) * 0.7
    rgb = rgb + (edge * np.clip(ny, 0, 1))[..., None] * np.array([1.0, 0.55, 0.20]) * 0.45
    arr = np.concatenate([np.clip(rgb, 0, 1), np.clip(m * 1.0, 0, 1)[..., None]], axis=2)
    img = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8), 'RGBA')
    # accent rules + subtitle
    d = ImageDraw.Draw(img)
    ly = py0 + mh + int(h * 0.045)
    d.line([(w * 0.10, ly), (w * 0.90, ly)], fill=(110, 230, 255, 110), width=2)
    d.line([(w * 0.18, ly + 5), (w * 0.52, ly + 5)], fill=(255, 180, 80, 150), width=2)
    d.line([(w * 0.60, ly + 5), (w * 0.82, ly + 5)], fill=(110, 230, 255, 70), width=1)
    sub = 'DEEP  FIELD  EXTRACTION'
    bbs = d.textbbox((0, 0), sub, font=f2)
    d.text(((w - (bbs[2] - bbs[0])) / 2, ly + h * 0.048 - bbs[1]), sub, font=f2, fill=(168, 202, 228, 225))
    return img


if __name__ == '__main__':
    import sys, os
    out = sys.argv[1] if len(sys.argv) > 1 else 'assets/gen'
    os.makedirs(out, exist_ok=True)
    for i, (name, base, acc, shape) in enumerate(RES):
        make_resource_icon(base, acc, shape, 40 + i).save(f'{out}/{name}.png')
    print('resources')
    for k in ['upg_laser', 'upg_speed', 'upg_cargo', 'upg_engine', 'upg_accel', 'upg_hull',
              'upg_shield', 'upg_energy', 'upg_scanner', 'upg_drone', 'upg_weapon', 'upg_tractor',
              'ui_drone', 'ui_ship', 'ui_cargo', 'ui_mining', 'ui_shield', 'ui_energy', 'ui_skull',
              'ui_star', 'ui_arrow', 'ui_lock', 'ui_cross', 'ui_warn', 'ui_ring', 'ui_glow']:
        glyph_icon(k).save(f'{out}/{k}.png')
    print('glyphs')
    logo_art().save(f'{out}/logo.png')
    # window / exe icon
    ico_src = Image.open(f'{out}/logo.png').convert('RGBA')
    logo_art(256, 256).save(f'{out}/icon_256.png')
    print('logo')
