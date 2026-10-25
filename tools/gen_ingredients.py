#!/usr/bin/env python3
"""
Generates 50 bespoke painterly illustrated 128x128 RGBA ingredient icons (supersampled at 256x256)
for ALCHEMY CHAOS — zero reused silhouettes, zero plain circles, zero emojis!
"""

import math
import os
import random
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from game_manifest import INGREDIENTS
from art_helpers import (
    hex_to_rgb,
    hex_to_rgba,
    mix_color,
    brighten,
    add_radial_glow,
    draw_shaded_sphere,
    add_outline_and_shadow,
)


def poly_flame(cx, bottom_y, height, width, sway=0.0):
    pts = []
    steps = 24
    for i in range(steps + 1):
        t = i / steps
        y = bottom_y - t * height
        w = width * math.sin(t * math.pi) * (1.0 - 0.35 * t)
        sx = sway * (t ** 1.6)
        pts.append((cx + sx - w, y))
    for i in range(steps, -1, -1):
        t = i / steps
        y = bottom_y - t * height
        w = width * math.sin(t * math.pi) * (1.0 - 0.35 * t)
        sx = sway * (t ** 1.6)
        pts.append((cx + sx + w, y))
    return pts


def poly_leaf(x0, y0, angle_deg, length, width):
    ang = math.radians(angle_deg)
    ca, sa = math.cos(ang), math.sin(ang)
    pts = []
    steps = 16
    for side in (-1, 1):
        rng = range(steps + 1) if side == -1 else range(steps, -1, -1)
        for i in rng:
            t = i / steps
            lx = t * length
            ly = side * width * math.sin(t * math.pi) * (1.0 - 0.2 * t)
            pts.append((x0 + lx * ca - ly * sa, y0 + lx * sa + ly * ca))
    return pts


def draw_crystal_prism(d: ImageDraw.ImageDraw, cx, cy, h, w, angle_deg, c_main, c_hi, c_dark):
    ang = math.radians(angle_deg)
    ca, sa = math.cos(ang), math.sin(ang)
    def rot(px, py):
        return (cx + px * ca - py * sa, cy + px * sa + py * ca)

    tip = rot(0, -h * 0.58)
    ul = rot(-w * 0.5, -h * 0.22)
    uc = rot(0, -h * 0.16)
    ur = rot(w * 0.5, -h * 0.22)
    bl = rot(-w * 0.42, h * 0.46)
    bc = rot(0, h * 0.54)
    br = rot(w * 0.42, h * 0.46)

    # Left bright facet, right shaded facet, top crown facets
    d.polygon([ul, uc, bc, bl], fill=(*mix_color(c_main, c_hi, 0.35), 255))
    d.polygon([uc, ur, br, bc], fill=(*mix_color(c_main, c_dark, 0.42), 255))
    d.polygon([tip, ul, uc], fill=(*c_hi, 255))
    d.polygon([tip, uc, ur], fill=(*c_main, 255))
    # Specular ridge lines
    d.line([tip, uc, bc], fill=(255, 255, 255, 210), width=3)
    d.line([ul, tip, ur, br, bc, bl, ul], fill=(*c_dark, 230), width=2)


def draw_glass_flask(art: Image.Image, cx, cy, body_r, neck_w, neck_h, liq_col, liq_hi, cork=True, round_bottom=True):
    d = ImageDraw.Draw(art)
    top_y = cy - body_r - neck_h
    neck_l = cx - neck_w // 2
    neck_r = cx + neck_w // 2
    # Back glass tint
    if round_bottom:
        d.ellipse([cx - body_r, cy - body_r, cx + body_r, cy + body_r], fill=(195, 232, 250, 75))
        # Liquid fill inside lower 68%
        liq_layer = Image.new("RGBA", art.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(liq_layer)
        ld.pieslice([cx - body_r + 7, cy - body_r + 7, cx + body_r - 7, cy + body_r - 7], start=-12, end=192, fill=(*liq_col, 240))
        ld.ellipse([cx - body_r + 10, cy - 16, cx + body_r - 10, cy + 8], fill=(*liq_hi, 235))
        art.alpha_composite(liq_layer)
        d = ImageDraw.Draw(art)
        d.ellipse([cx - body_r, cy - body_r, cx + body_r, cy + body_r], outline=(215, 242, 255, 235), width=5)
        # Specular crescent highlight
        d.arc([cx - body_r + 14, cy - body_r + 14, cx + body_r - 14, cy + body_r - 14], start=195, end=255, fill=(255, 255, 255, 220), width=6)
    else:
        # Erlenmeyer conical flask
        pts = [(neck_l, cy - body_r), (neck_r, cy - body_r), (cx + body_r, cy + body_r - 8), (cx - body_r, cy + body_r - 8)]
        d.polygon(pts, fill=(195, 232, 250, 75))
        lpts = [(cx - int(body_r * 0.52), cy - 6), (cx + int(body_r * 0.52), cy - 6), (cx + body_r - 8, cy + body_r - 14), (cx - body_r + 8, cy + body_r - 14)]
        d.polygon(lpts, fill=(*liq_col, 240))
        d.ellipse([cx - int(body_r * 0.52), cy - 14, cx + int(body_r * 0.52), cy + 2], fill=(*liq_hi, 235))
        d.polygon(pts, outline=(215, 242, 255, 235), width=5)
        d.line([(neck_l + 6, cy - body_r + 14), (cx - body_r + 18, cy + body_r - 22)], fill=(255, 255, 255, 210), width=5)

    # Neck & flared lip
    d.rectangle([neck_l, top_y, neck_r, cy - body_r + 10], fill=(205, 238, 255, 110), outline=(220, 245, 255, 230), width=4)
    d.rounded_rectangle([neck_l - 7, top_y - 6, neck_r + 7, top_y + 7], radius=5, fill=(225, 246, 255, 230), outline=(140, 185, 215, 240), width=3)
    if cork:
        d.polygon([(neck_l + 3, top_y - 4), (neck_r - 3, top_y - 4), (neck_r + 2, top_y - 24), (neck_l - 2, top_y - 24)], fill=(168, 112, 62, 255), outline=(92, 54, 24, 255))


def render_ingredient(ing: dict) -> Image.Image:
    S = 256
    base = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    c1 = hex_to_rgb(ing["primary"])
    c2 = hex_to_rgb(ing["secondary"])
    cdark = tuple(max(0, int(v * 0.35)) for v in c1)

    # Subtle arcane medallion aura behind the subject
    aura_alpha = 115 if ing["rarity"] in ("Rare", "Mythic") else 75
    base = add_radial_glow(base, S / 2, S / 2 + 4, 108, c1, alpha_max=aura_alpha)
    if ing["rarity"] in ("Rare", "Mythic"):
        base = add_radial_glow(base, S / 2, S / 2, 74, c2, alpha_max=85)
        ad = ImageDraw.Draw(base)
        ad.ellipse([26, 26, S - 26, S - 26], outline=(*c2, 95), width=2)

    art = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(art)
    sh = ing["shape"]
    rng = random.Random(ing["num"] * 1337)

    if sh == "flame_trio":
        d.polygon(poly_flame(102, 208, 132, 34, -16), fill=(*c1, 255))
        d.polygon(poly_flame(156, 208, 126, 32, 18), fill=(*mix_color(c1, c2, 0.25), 255))
        d.polygon(poly_flame(128, 214, 174, 52, 4), fill=(*c1, 255))
        d.polygon(poly_flame(128, 210, 128, 34, 3), fill=(*c2, 255))
        d.polygon(poly_flame(128, 206, 76, 18, 2), fill=(255, 250, 224, 255))
        for ex, ey, er in [(82, 82, 6), (172, 70, 5), (144, 44, 4)]:
            d.ellipse([ex - er, ey - er, ex + er, ey + er], fill=(*c2, 245))

    elif sh == "droplet_swirl":
        # Sculpted main water teardrop + crown splash droplets
        pts = poly_flame(128, 48, -158, 58, 0)
        d.polygon(pts, fill=(*c1, 255))
        draw_shaded_sphere(art, 128, 152, 54, c1, c2, cdark)
        d = ImageDraw.Draw(art)
        d.arc([88, 114, 168, 192], start=110, end=245, fill=(255, 255, 255, 225), width=8)
        d.ellipse([102, 94, 116, 114], fill=(255, 255, 255, 230))
        for dx, dy, dr in [(66, 130, 12), (190, 118, 10), (176, 172, 8)]:
            draw_shaded_sphere(art, dx, dy, dr, c1, (255, 255, 255), cdark)

    elif sh == "earth_clod":
        # Layered faceted earthen boulder + grass tuft + mineral veins
        pts = [(54, 168), (68, 102), (112, 68), (168, 74), (204, 118), (196, 176), (148, 204), (86, 198)]
        d.polygon(pts, fill=(*c1, 255))
        d.polygon([(68, 102), (112, 68), (168, 74), (138, 128), (82, 134)], fill=(*c2, 255))
        d.polygon([(138, 128), (168, 74), (204, 118), (196, 176), (148, 204)], fill=(*cdark, 255))
        d.line([(96, 108), (138, 128), (148, 200)], fill=(45, 28, 14, 220), width=4)
        for px, py, pr in [(74, 188, 14), (186, 186, 12), (116, 198, 9)]:
            draw_shaded_sphere(art, px, py, pr, c2, (235, 210, 175), cdark)

    elif sh == "wind_vortex":
        for idx, (yc, rx, ry, w) in enumerate([(82, 76, 22, 10), (124, 62, 20, 9), (162, 44, 16, 8), (194, 24, 10, 7)]):
            col = mix_color(c1, c2, idx / 3.0)
            d.arc([128 - rx, yc - ry, 128 + rx, yc + ry], start=-25, end=215, fill=(*col, 245), width=w)
            d.arc([128 - rx + 8, yc - ry + 6, 128 + rx - 4, yc + ry + 6], start=165, end=355, fill=(255, 255, 255, 215), width=max(3, w - 4))
        # Swirling leaf/air curls
        for cx0, cy0 in [(62, 92), (196, 114), (78, 166)]:
            d.arc([cx0 - 16, cy0 - 16, cx0 + 16, cy0 + 16], start=20, end=270, fill=(255, 255, 255, 230), width=5)

    elif sh == "steam_plume":
        puffs = [(128, 186, 28), (104, 152, 34), (154, 146, 32), (92, 108, 40), (142, 98, 44), (124, 68, 36)]
        for px, py, pr in puffs:
            draw_shaded_sphere(art, px, py, pr, c1, (255, 255, 255), c2, alpha=235)
        d = ImageDraw.Draw(art)
        for sx in (98, 128, 158):
            d.arc([sx - 14, 178, sx + 14, 224], start=100, end=260, fill=(255, 255, 255, 210), width=5)

    elif sh == "heat_waves":
        draw_shaded_sphere(art, 128, 166, 44, c1, c2, cdark)
        d = ImageDraw.Draw(art)
        for ox in (-46, 0, 46):
            pts = []
            for i in range(28):
                t = i / 27.0
                y = 198 - t * 148
                x = 128 + ox + math.sin(t * math.pi * 3.0 + ox) * 14
                pts.append((x, y))
            d.line(pts, fill=(*c1, 245), width=12, joint="curve")
            d.line(pts, fill=(*c2, 245), width=6, joint="curve")

    elif sh == "ice_spire":
        draw_crystal_prism(d, 92, 146, 116, 40, -18, c1, c2, (32, 96, 148))
        draw_crystal_prism(d, 166, 150, 110, 38, 20, c1, c2, (32, 96, 148))
        draw_crystal_prism(d, 128, 126, 164, 54, 0, c1, (255, 255, 255), (28, 86, 142))

    elif sh == "lightning_orb":
        draw_shaded_sphere(art, 128, 128, 48, (26, 86, 168), c1, (12, 24, 58))
        d = ImageDraw.Draw(art)
        bolt_poly = [
            (148, 36), (92, 126), (128, 126),
            (104, 220), (168, 118), (132, 118),
        ]
        d.polygon(bolt_poly, fill=(*c2, 255), outline=(*c1, 255))
        d.line([(52, 98), (88, 118), (68, 150)], fill=(*c1, 255), width=7)
        d.line([(204, 92), (168, 120), (194, 152)], fill=(*c1, 255), width=7)

    elif sh == "mud_mound":
        blobs = [(92, 168, 42), (162, 170, 40), (128, 154, 52), (128, 122, 36)]
        for bx, by, br in blobs:
            draw_shaded_sphere(art, bx, by, br, c1, c2, cdark)
        d = ImageDraw.Draw(art)
        d.ellipse([62, 178, 194, 210], fill=(*c1, 255))
        for bx, by, br in [(104, 132, 8), (154, 148, 10), (124, 176, 7)]:
            d.ellipse([bx - br, by - br, bx + br, by + br], fill=(*cdark, 255))
            d.arc([bx - br + 2, by - br + 2, bx + br - 2, by + br - 2], 190, 320, fill=(*c2, 230), width=3)

    elif sh == "stone_monolith":
        d.polygon([(62, 188), (54, 124), (96, 62), (164, 58), (202, 114), (190, 192), (124, 206)], fill=(*c1, 255))
        d.polygon([(96, 62), (164, 58), (144, 124), (78, 128), (54, 124)], fill=(*c2, 255))
        d.polygon([(144, 124), (164, 58), (202, 114), (190, 192), (124, 206)], fill=(*cdark, 255))
        d.line([(96, 62), (144, 124), (124, 206)], fill=(235, 242, 250, 180), width=3)
        d.line([(78, 128), (144, 124), (202, 114)], fill=(42, 46, 54, 200), width=3)

    elif sh == "lava_crater":
        # Dark basalt volcanic cone overflowing with glowing magma rivers
        d.polygon([(46, 196), (86, 86), (170, 86), (210, 196)], fill=(54, 38, 36, 255))
        d.ellipse([84, 72, 172, 106], fill=(*c2, 255), outline=(*c1, 255), width=6)
        rivers = [
            [(116, 98), (102, 142), (78, 194), (98, 198), (120, 148), (128, 102)],
            [(138, 100), (154, 148), (176, 196), (158, 198), (140, 152)],
        ]
        for rv in rivers:
            d.polygon(rv, fill=(*c1, 255))
        d.polygon([(118, 98), (108, 144), (90, 192), (108, 148), (126, 100)], fill=(*c2, 255))
        for sx, sy in [(106, 54), (148, 46), (128, 38)]:
            draw_shaded_sphere(art, sx, sy, 8, c1, c2)

    elif sh == "sand_dune":
        d.polygon([(42, 188), (98, 112), (146, 138), (214, 188)], fill=(*mix_color(c1, cdark, 0.3), 255))
        d.polygon([(48, 194), (134, 96), (212, 194)], fill=(*c1, 255))
        d.polygon([(134, 96), (118, 148), (142, 194), (48, 194)], fill=(*c2, 255))
        for _ in range(22):
            gx = rng.randint(68, 188)
            gy = rng.randint(120, 190)
            d.ellipse([gx - 2, gy - 2, gx + 2, gy + 2], fill=(255, 252, 230, 220))

    elif sh == "clay_tablet":
        # Sculpted potter's amphora/clay coil + stamped brick
        d.rounded_rectangle([64, 92, 192, 196], radius=24, fill=(*c1, 255), outline=(*cdark, 255), width=5)
        d.rounded_rectangle([76, 104, 180, 184], radius=16, fill=(*mix_color(c1, c2, 0.4), 255))
        # Engraved alchemical spiral on wet clay
        d.arc([98, 116, 158, 174], 0, 300, fill=(*cdark, 230), width=6)
        d.arc([110, 128, 146, 162], 40, 340, fill=(*cdark, 230), width=5)
        draw_shaded_sphere(art, 84, 184, 24, c1, c2, cdark)

    elif sh == "sulfur_cluster":
        for cx0, cy0, h0, w0, ang in [
            (84, 154, 96, 36, -28),
            (172, 152, 102, 38, 25),
            (108, 136, 132, 44, -8),
            (148, 140, 124, 42, 10),
            (128, 130, 148, 48, 0),
        ]:
            draw_crystal_prism(d, cx0, cy0, h0, w0, ang, c1, c2, (138, 108, 8))

    elif sh == "salt_cubes":
        cubes = [(88, 156, 34), (160, 158, 32), (126, 126, 44), (118, 172, 28)]
        for cx0, cy0, r in cubes:
            top = [(cx0, cy0 - r), (cx0 + r, cy0 - r * 0.45), (cx0, cy0 + r * 0.1), (cx0 - r, cy0 - r * 0.45)]
            left = [(cx0 - r, cy0 - r * 0.45), (cx0, cy0 + r * 0.1), (cx0, cy0 + r), (cx0 - r, cy0 + r * 0.45)]
            right = [(cx0, cy0 + r * 0.1), (cx0 + r, cy0 - r * 0.45), (cx0 + r, cy0 + r * 0.45), (cx0, cy0 + r)]
            d.polygon(top, fill=(255, 255, 255, 255), outline=(*c2, 240))
            d.polygon(left, fill=(*c1, 255), outline=(*c2, 240))
            d.polygon(right, fill=(*c2, 255), outline=(120, 148, 175, 240))

    elif sh == "quartz_prism":
        draw_crystal_prism(d, 86, 152, 108, 34, -24, c2, (255, 255, 255), (84, 138, 178))
        draw_crystal_prism(d, 170, 148, 114, 36, 22, c2, (255, 255, 255), (84, 138, 178))
        draw_crystal_prism(d, 128, 124, 170, 56, 2, c1, (255, 255, 255), (68, 122, 168))
        # Rainbow refraction glint
        d.line([(106, 98), (154, 128)], fill=(255, 190, 240, 210), width=4)

    elif sh == "obsidian_blade":
        pts = [(128, 38), (178, 98), (164, 162), (142, 214), (96, 198), (78, 136), (94, 78)]
        d.polygon(pts, fill=(*c1, 255))
        # Conchoidal glossy violet fractures
        d.polygon([(128, 38), (178, 98), (132, 122)], fill=(*c2, 220))
        d.polygon([(94, 78), (128, 38), (132, 122), (78, 136)], fill=(58, 44, 88, 255))
        d.polygon([(78, 136), (132, 122), (142, 214), (96, 198)], fill=(22, 16, 34, 255))
        d.line([(128, 38), (132, 122), (142, 214)], fill=(230, 215, 255, 225), width=4)

    elif sh == "moss_stone":
        d.ellipse([56, 124, 200, 206], fill=(96, 102, 112, 255))
        for mx, my, mr in [(86, 138, 28), (118, 120, 34), (154, 128, 30), (104, 152, 26), (144, 154, 26), (128, 104, 24)]:
            draw_shaded_sphere(art, mx, my, mr, c1, c2, cdark)
        d = ImageDraw.Draw(art)
        for sx in (98, 128, 154):
            d.line([(sx, 104), (sx - 4, 72)], fill=(*c2, 255), width=3)
            d.ellipse([sx - 8, 64, sx, 74], fill=(*c2, 255))

    elif sh == "herb_sprig":
        d.line([(84, 204), (116, 146), (156, 62)], fill=(42, 118, 66, 255), width=8)
        leaves = [
            (98, 174, -145, 58, 20),
            (108, 160, -25, 58, 20),
            (118, 134, -140, 62, 22),
            (128, 122, -20, 62, 22),
            (140, 94, -135, 52, 18),
            (146, 86, -15, 52, 18),
            (152, 68, -75, 50, 18),
        ]
        for lx, ly, ang, ln, wd in leaves:
            lp = poly_leaf(lx, ly, ang, ln, wd)
            d.polygon(lp, fill=(*c1, 255), outline=(*c2, 245))

    elif sh == "sun_flower":
        # Stem & leaves
        d.line([(128, 132), (128, 214)], fill=(58, 136, 52, 255), width=9)
        d.polygon(poly_leaf(128, 184, -145, 52, 18), fill=(68, 162, 62, 255))
        d.polygon(poly_leaf(128, 176, -35, 52, 18), fill=(84, 182, 74, 255))
        for k in range(12):
            ang = k * 30
            col = c1 if k % 2 == 0 else mix_color(c1, c2, 0.55)
            d.polygon(poly_leaf(128, 112, ang, 72, 20), fill=(*col, 255), outline=(255, 248, 196, 200))
        draw_shaded_sphere(art, 128, 112, 26, (186, 82, 14), c2, (92, 34, 6))

    elif sh == "frost_lily":
        d.line([(128, 134), (128, 214)], fill=(76, 158, 182, 255), width=7)
        for k in range(6):
            ang = -90 + k * 60
            d.polygon(poly_leaf(128, 116, ang + 30, 58, 16), fill=(*c1, 225))
        for k in range(6):
            ang = -90 + k * 60
            d.polygon(poly_leaf(128, 116, ang, 74, 22), fill=(*c2, 245), outline=(*c1, 255))
        draw_shaded_sphere(art, 128, 116, 16, (255, 255, 255), c2, c1)

    elif sh == "glow_mushrooms":
        d.ellipse([66, 182, 190, 210], fill=(54, 42, 62, 255))
        mush = [(84, 152, 30, 192), (170, 158, 26, 192), (128, 114, 48, 196)]
        for mx, my, mr, by in mush:
            d.polygon([(mx - mr // 4, my), (mx + mr // 4, my), (mx + mr // 3, by), (mx - mr // 3, by)], fill=(214, 245, 238, 255))
            d.pieslice([mx - mr, my - int(mr * 0.85), mx + mr, my + int(mr * 0.65)], 180, 360, fill=(*c1, 255), outline=(*c2, 255), width=3)
            for sx in (-mr // 2, 0, mr // 2):
                d.ellipse([mx + sx - 4, my - mr // 2 - 4, mx + sx + 4, my - mr // 2 + 4], fill=(*c2, 255))

    elif sh == "nightshade_berry":
        d.arc([68, 56, 188, 168], 190, 350, fill=(58, 112, 62, 255), width=8)
        d.polygon(poly_leaf(104, 78, -155, 56, 20), fill=(46, 98, 56, 255), outline=(92, 168, 96, 230))
        d.polygon(poly_leaf(152, 78, -25, 56, 20), fill=(56, 116, 66, 255), outline=(92, 168, 96, 230))
        for bx, by, br in [(96, 146, 28), (158, 148, 28), (128, 168, 32), (128, 122, 26)]:
            draw_shaded_sphere(art, bx, by, br, c1, c2, (28, 6, 48))

    elif sh == "mandrake_root":
        # Crown leaves
        for ang in (-135, -105, -75, -45):
            d.polygon(poly_leaf(128, 88, ang, 54, 18), fill=(*c2, 255), outline=(38, 98, 32, 255))
        # Gnarled anthropomorphic root body & rootlet arms/legs
        d.line([(128, 118), (78, 144), (62, 168)], fill=(*c1, 255), width=14)
        d.line([(128, 118), (178, 142), (194, 168)], fill=(*c1, 255), width=14)
        d.line([(116, 166), (96, 212)], fill=(*c1, 255), width=16)
        d.line([(140, 166), (160, 212)], fill=(*c1, 255), width=16)
        draw_shaded_sphere(art, 128, 108, 26, c1, (206, 158, 108), cdark)
        draw_shaded_sphere(art, 128, 148, 32, c1, (206, 158, 108), cdark)
        d = ImageDraw.Draw(art)
        # Subtle glowing rune eyes
        d.ellipse([115, 102, 123, 110], fill=(*c2, 255))
        d.ellipse([133, 102, 141, 110], fill=(*c2, 255))

    elif sh == "amber_teardrop":
        pts = poly_flame(128, 52, -154, 56, 0)
        d.polygon(pts, fill=(*c1, 245))
        draw_shaded_sphere(art, 128, 150, 52, c1, c2, (112, 48, 4), alpha=240)
        d = ImageDraw.Draw(art)
        # Trapped ancient golden leaf/sparkle inside amber
        d.polygon(poly_leaf(114, 156, -45, 36, 11), fill=(116, 52, 8, 210))
        d.ellipse([104, 112, 122, 134], fill=(255, 250, 214, 225))

    elif sh == "ore_nugget":
        pts = [(58, 158), (74, 88), (134, 62), (192, 92), (198, 162), (144, 198), (82, 192)]
        d.polygon(pts, fill=(78, 74, 82, 255))
        for gx, gy, gr in [(106, 108, 26), (154, 118, 24), (126, 156, 28), (88, 154, 18), (168, 158, 18)]:
            draw_shaded_sphere(art, gx, gy, gr, c1, c2, (44, 52, 66))

    elif sh == "crucible_pour":
        # Tilted dark refractory crucible pouring glowing molten metal stream
        d.polygon([(58, 92), (142, 62), (162, 136), (82, 156)], fill=(64, 58, 68, 255), outline=(128, 118, 136, 255), width=4)
        d.ellipse([122, 62, 168, 138], fill=(*c2, 255), outline=(*c1, 255), width=5)
        # Pouring stream & glowing pool
        d.polygon([(152, 96), (168, 104), (182, 190), (144, 190)], fill=(*c1, 255))
        d.polygon([(156, 100), (164, 104), (170, 188), (154, 188)], fill=(*c2, 255))
        d.ellipse([104, 178, 206, 210], fill=(*c1, 255))
        d.ellipse([122, 184, 190, 204], fill=(*c2, 255))

    elif sh == "iron_anvil_ingot":
        # Trapezoidal beveled forged iron ingot + anvil silhouette
        top = [(78, 98), (178, 82), (198, 112), (98, 128)]
        front = [(62, 148), (98, 128), (198, 112), (186, 164), (82, 184)]
        d.polygon(front, fill=(*c1, 255), outline=(*cdark, 255), width=4)
        d.polygon(top, fill=(*c2, 255), outline=(*c1, 255), width=4)
        d.line([(98, 128), (82, 184)], fill=(255, 255, 255, 180), width=4)
        # Mars symbol (circle + arrow) stamped on top
        d.ellipse([122, 96, 146, 114], outline=(*cdark, 220), width=3)

    elif sh == "copper_coil":
        for i in range(5):
            y = 74 + i * 26
            d.arc([74, y, 182, y + 42], 160, 380, fill=(*cdark, 255), width=18)
            d.arc([74, y, 182, y + 42], 165, 375, fill=(*c1, 255), width=13)
            d.arc([82, y + 3, 174, y + 38], 195, 330, fill=(*c2, 245), width=5)
        d.line([(78, 88), (78, 52)], fill=(*c1, 255), width=12)
        d.line([(178, 194), (198, 208)], fill=(*c1, 255), width=12)

    elif sh == "silver_crescent":
        moon = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        md = ImageDraw.Draw(moon)
        md.ellipse([54, 48, 202, 198], fill=(*c1, 255), outline=(*c2, 255), width=5)
        cut = Image.new("L", (S, S), 255)
        cd = ImageDraw.Draw(cut)
        cd.ellipse([98, 42, 224, 178], fill=0)
        r, g, b, a = moon.split()
        from PIL import ImageChops
        moon.putalpha(ImageChops.multiply(a, cut))
        art.alpha_composite(moon)
        d = ImageDraw.Draw(art)
        # Four-pointed silver star gleam
        for sx, sy, sr in [(164, 108, 18), (140, 74, 10)]:
            d.polygon([(sx, sy - sr), (sx + 4, sy - 4), (sx + sr, sy), (sx + 4, sy + 4), (sx, sy + sr), (sx - 4, sy + 4), (sx - sr, sy), (sx - 4, sy - 4)], fill=(255, 255, 255, 255))

    elif sh == "lead_plumb":
        # Hexagonal plumb weight with brass suspension ring
        d.ellipse([110, 46, 146, 82], outline=(*c2, 255), width=8)
        d.polygon([(78, 82), (178, 82), (194, 148), (128, 212), (62, 148)], fill=(*c1, 255), outline=(*c2, 255), width=4)
        d.polygon([(78, 82), (128, 82), (128, 212), (62, 148)], fill=(*mix_color(c1, c2, 0.35), 255))
        # Saturn cross-sickle mark
        d.line([(128, 106), (128, 162)], fill=(30, 32, 38, 230), width=5)
        d.line([(110, 120), (146, 120)], fill=(30, 32, 38, 230), width=5)

    elif sh == "mercury_flask":
        draw_glass_flask(art, 122, 144, 58, 30, 42, c1, c2, cork=True, round_bottom=True)
        # Escaping mirror-bright quicksilver droplets beside flask
        draw_shaded_sphere(art, 192, 182, 18, c1, (255, 255, 255), (72, 86, 104))
        draw_shaded_sphere(art, 168, 202, 10, c1, (255, 255, 255), (72, 86, 104))

    elif sh == "bronze_gear":
        cx, cy = 128, 128
        teeth = 12
        pts = []
        for i in range(teeth * 4):
            ang = (i / (teeth * 4)) * math.tau
            r = 84 if (i % 4 in (1, 2)) else 66
            pts.append((cx + math.cos(ang) * r, cy + math.sin(ang) * r))
        d.polygon(pts, fill=(*c1, 255), outline=(*cdark, 255))
        d.ellipse([cx - 56, cy - 56, cx + 56, cy + 56], fill=(*mix_color(c1, c2, 0.35), 255), outline=(*c2, 255), width=5)
        for k in range(4):
            ang = k * math.pi / 2 + math.pi / 4
            hx, hy = cx + math.cos(ang) * 34, cy + math.sin(ang) * 34
            d.ellipse([hx - 11, hy - 11, hx + 11, hy + 11], fill=(24, 18, 16, 255), outline=(*cdark, 255), width=3)
        d.ellipse([cx - 18, cy - 18, cx + 18, cy + 18], fill=(24, 18, 16, 255), outline=(*c2, 255), width=5)

    elif sh == "gold_sun_ingot":
        # Stacked gleaming gold bullion bars + solar emblem
        for ox, oy in [(-18, 24), (24, 24), (2, -16)]:
            bx, by = 126 + ox, 134 + oy
            d.polygon([(bx - 52, by + 24), (bx - 36, by - 14), (bx + 44, by - 24), (bx + 56, by + 12)], fill=(*c1, 255), outline=(146, 92, 0, 255), width=4)
            d.polygon([(bx - 36, by - 14), (bx + 44, by - 24), (bx + 34, by - 4), (bx - 42, by + 6)], fill=(*c2, 255))
        # Sunburst sparkles
        for sx, sy, sr in [(74, 78, 16), (186, 72, 14)]:
            d.polygon([(sx, sy - sr), (sx + 4, sy - 4), (sx + sr, sy), (sx + 4, sy + 4), (sx, sy + sr), (sx - 4, sy + 4), (sx - sr, sy), (sx - 4, sy - 4)], fill=(255, 252, 210, 255))

    elif sh == "thermal_split_sphere":
        # Left crimson fire hemisphere, right ice-cyan hemisphere, jagged fracture lightning seam
        d.pieslice([56, 56, 200, 200], 95, 265, fill=(*c1, 255), outline=(255, 210, 140, 255), width=4)
        d.pieslice([56, 56, 200, 200], -85, 85, fill=(*c2, 255), outline=(230, 252, 255, 255), width=4)
        crack = [(128, 42), (116, 86), (140, 118), (114, 152), (136, 182), (126, 214)]
        d.line(crack, fill=(255, 255, 255, 255), width=8)
        for ang in (35, 145, 215, 325):
            rad = math.radians(ang)
            d.line([(128 + math.cos(rad) * 76, 128 + math.sin(rad) * 76), (128 + math.cos(rad) * 102, 128 + math.sin(rad) * 102)], fill=(255, 240, 190, 240), width=5)

    elif sh == "leyden_jar":
        # Brass-topped galvanic Leyden jar crackling with internal lightning
        d.rounded_rectangle([78, 86, 178, 204], radius=18, fill=(18, 66, 112, 210), outline=(200, 242, 255, 245), width=5)
        d.rounded_rectangle([84, 124, 172, 198], radius=12, fill=(*c1, 235))
        # Brass lid & electrode rod + terminal sphere
        d.rectangle([86, 74, 170, 90], fill=(208, 162, 74, 255), outline=(104, 72, 24, 255), width=3)
        d.line([(128, 48), (128, 166)], fill=(238, 198, 108, 255), width=7)
        draw_shaded_sphere(art, 128, 46, 14, (238, 192, 92), (255, 245, 190))
        d = ImageDraw.Draw(art)
        d.line([(128, 142), (102, 158), (116, 180)], fill=(*c2, 255), width=4)
        d.line([(128, 136), (156, 154), (144, 182)], fill=(255, 255, 255, 255), width=4)

    elif sh == "alembic_retort":
        # Classic curved swan-neck glass retort
        draw_shaded_sphere(art, 106, 148, 52, (168, 226, 252), (255, 255, 255), (76, 138, 176), alpha=190)
        d = ImageDraw.Draw(art)
        spout = [(116, 98), (156, 86), (210, 162), (198, 170), (148, 108), (106, 118)]
        d.polygon(spout, fill=(196, 238, 255, 195), outline=(240, 252, 255, 245), width=4)
        d.ellipse([54, 96, 158, 200], outline=(240, 252, 255, 245), width=5)
        d.arc([68, 110, 144, 186], 195, 260, fill=(255, 255, 255, 235), width=7)
        draw_shaded_sphere(art, 204, 186, 10, (120, 220, 255), (255, 255, 255))

    elif sh == "porcelain_mortar":
        # Pestle sticking out of white glazed porcelain mortar with blue cobalt trim
        d.line([(146, 124), (184, 58)], fill=(*c1, 255), width=22)
        draw_shaded_sphere(art, 186, 56, 13, c1, (255, 255, 255), c2)
        d = ImageDraw.Draw(art)
        d.pieslice([58, 66, 198, 202], 0, 180, fill=(*c1, 255), outline=(*c2, 255), width=5)
        d.ellipse([58, 116, 198, 148], fill=(*c2, 255), outline=(255, 255, 255, 245), width=4)
        d.arc([68, 142, 188, 186], 15, 165, fill=(72, 118, 188, 220), width=5)
        d.rounded_rectangle([92, 194, 164, 208], radius=6, fill=(*c2, 255))

    elif sh == "ash_urn":
        # Brass dish piled with pale calcined potash & drifting embers
        d.pieslice([56, 128, 200, 208], 0, 180, fill=(168, 126, 64, 255), outline=(92, 64, 26, 255), width=4)
        d.polygon([(66, 168), (128, 92), (190, 168)], fill=(*c1, 255))
        d.polygon([(66, 168), (128, 92), (138, 168)], fill=(*c2, 255))
        for ex, ey in [(112, 146), (146, 152), (126, 72), (142, 56)]:
            d.ellipse([ex - 4, ey - 4, ex + 4, ey + 4], fill=(255, 136, 54, 235))

    elif sh == "acid_flask":
        draw_glass_flask(art, 128, 148, 58, 32, 42, c1, c2, cork=False, round_bottom=False)
        d = ImageDraw.Draw(art)
        # Green caustic vapor & bubbles rising from open neck
        for bx, by, br in [(122, 74, 9), (138, 56, 12), (116, 40, 8), (118, 164, 7), (142, 174, 6)]:
            draw_shaded_sphere(art, bx, by, br, c1, c2, (24, 96, 12))

    elif sh == "powder_horn":
        # wooden-staved powder keg + spilled black powder mound + fuse spark
        d.rounded_rectangle([68, 84, 170, 196], radius=22, fill=(108, 66, 38, 255), outline=(58, 32, 16, 255), width=5)
        for hy in (106, 168):
            d.rectangle([66, hy, 172, hy + 12], fill=(92, 98, 108, 255))
        d.polygon([(134, 202), (178, 148), (214, 202)], fill=(*c1, 255))
        # Lit fuse & starburst spark
        d.arc([118, 48, 182, 98], 180, 320, fill=(214, 188, 142, 255), width=6)
        for k in range(8):
            ang = k * 45
            rad = math.radians(ang)
            d.line([(178, 58), (178 + math.cos(rad) * 20, 58 + math.sin(rad) * 20)], fill=(*c2, 255), width=4)

    elif sh == "amalgam_swirl":
        # Yin-yang metallic liquid swirl uniting silver quicksilver & warm gold-copper
        draw_shaded_sphere(art, 128, 128, 68, c1, (255, 255, 255), (84, 74, 112))
        d = ImageDraw.Draw(art)
        d.pieslice([60, 60, 196, 196], -90, 90, fill=(*c2, 245))
        draw_shaded_sphere(art, 128, 94, 34, c1, (255, 255, 255), (84, 74, 112))
        draw_shaded_sphere(art, 128, 162, 34, c2, (255, 252, 224), (146, 98, 28))
        draw_shaded_sphere(art, 128, 94, 10, c2, (255, 255, 255))
        draw_shaded_sphere(art, 128, 162, 10, c1, (255, 255, 255))

    elif sh == "royal_vial":
        draw_glass_flask(art, 128, 150, 54, 28, 38, c1, c2, cork=False, round_bottom=True)
        d = ImageDraw.Draw(art)
        # Golden crown collar around the flask neck!
        crown = [(98, 96), (94, 72), (112, 84), (128, 66), (144, 84), (162, 72), (158, 96)]
        d.polygon(crown, fill=(255, 204, 42, 255), outline=(142, 88, 8, 255), width=3)
        for fx, fy, fr in [(120, 48, 10), (140, 38, 12)]:
            draw_shaded_sphere(art, fx, fy, fr, c1, c2, alpha=215)

    elif sh == "aether_nebula":
        for i in range(3):
            rx, ry = 82, 32
            ring = Image.new("RGBA", (S, S), (0, 0, 0, 0))
            rd = ImageDraw.Draw(ring)
            col = c1 if i % 2 == 0 else c2
            rd.ellipse([128 - rx, 128 - ry, 128 + rx, 128 + ry], outline=(*col, 230), width=7)
            ring = ring.rotate(i * 60, resample=Image.Resampling.BICUBIC)
            art.alpha_composite(ring)
        draw_shaded_sphere(art, 128, 128, 32, c2, (255, 255, 255), c1)

    elif sh == "plasma_sphere":
        d = ImageDraw.Draw(art)
        d.ellipse([54, 54, 202, 202], fill=(32, 10, 54, 220), outline=(*c2, 240), width=5)
        for ang_deg in range(15, 360, 45):
            rad = math.radians(ang_deg)
            mx = 128 + math.cos(rad + 0.25) * 38
            my = 128 + math.sin(rad + 0.25) * 38
            ex = 128 + math.cos(rad) * 70
            ey = 128 + math.sin(rad) * 70
            d.line([(128, 128), (mx, my), (ex, ey)], fill=(*c1, 255), width=6)
            d.line([(128, 128), (mx, my), (ex, ey)], fill=(*c2, 255), width=2)
        draw_shaded_sphere(art, 128, 128, 24, c1, (255, 255, 255), (86, 12, 128))
        d = ImageDraw.Draw(art)
        d.rounded_rectangle([92, 198, 164, 214], radius=6, fill=(184, 142, 72, 255), outline=(88, 58, 20, 255), width=3)

    elif sh == "cryo_obelisk":
        # Six-pointed frost halo + towering sapphire cryo obelisk
        for k in range(6):
            rad = math.radians(k * 60)
            d.line([(128, 128), (128 + math.cos(rad) * 94, 128 + math.sin(rad) * 94)], fill=(*c2, 220), width=5)
        draw_crystal_prism(d, 128, 128, 178, 62, 0, c1, c2, (10, 54, 118))
        draw_crystal_prism(d, 82, 158, 86, 28, -32, c1, c2, (10, 54, 118))
        draw_crystal_prism(d, 174, 158, 86, 28, 32, c1, c2, (10, 54, 118))

    elif sh == "phoenix_feather_flame":
        # Sweeping golden-crimson phoenix plume wreathed in living embers
        d.polygon(poly_flame(128, 212, 178, 56, 22), fill=(*c1, 255))
        d.polygon(poly_flame(128, 204, 142, 38, 18), fill=(*c2, 255))
        # Quill shaft & eye-spot
        d.line([(114, 214), (132, 136), (148, 64)], fill=(255, 252, 220, 255), width=5)
        draw_shaded_sphere(art, 138, 106, 18, (186, 14, 58), c2, (78, 4, 24))
        for ex, ey, er in [(78, 112, 6), (182, 92, 7), (168, 152, 5)]:
            draw_shaded_sphere(art, ex, ey, er, c1, c2)

    elif sh == "void_singularity":
        # Accretion halo + light-drinking abyssal black sphere
        ring = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        rd = ImageDraw.Draw(ring)
        rd.ellipse([36, 98, 220, 158], outline=(*c2, 245), width=12)
        rd.ellipse([44, 104, 212, 152], outline=(255, 220, 255, 220), width=4)
        ring = ring.rotate(-22, resample=Image.Resampling.BICUBIC)
        art.alpha_composite(ring)
        d = ImageDraw.Draw(art)
        d.ellipse([78, 78, 178, 178], fill=(8, 3, 18, 255), outline=(*c2, 255), width=6)
        d.ellipse([86, 86, 170, 170], outline=(255, 210, 255, 190), width=2)

    elif sh == "elixir_chalice":
        # Ornate jewelled golden chalice overflowing with opalescent rose-emerald light
        d.polygon([(118, 142), (138, 142), (146, 198), (110, 198)], fill=(224, 176, 68, 255), outline=(118, 78, 18, 255), width=3)
        d.rounded_rectangle([88, 194, 168, 210], radius=7, fill=(238, 192, 82, 255), outline=(118, 78, 18, 255), width=3)
        d.pieslice([68, 54, 188, 154], 0, 180, fill=(238, 192, 82, 255), outline=(128, 84, 18, 255), width=5)
        d.ellipse([72, 92, 184, 118], fill=(*c1, 255), outline=(255, 232, 148, 255), width=4)
        d.ellipse([88, 96, 168, 112], fill=(*c2, 245))
        # Ruby & emerald gems set into cup + rising luminous sparkles
        draw_shaded_sphere(art, 128, 132, 10, c1, (255, 255, 255))
        for sx, sy, sr in [(106, 68, 8), (148, 58, 10), (126, 44, 7)]:
            draw_shaded_sphere(art, sx, sy, sr, c2, (255, 255, 255), c1)

    elif sh == "philosopher_stone":
        # Sacred geometry squared-circle gold halo + crimson-gold octahedron hyper-crystal
        d.ellipse([44, 44, 212, 212], outline=(*c2, 235), width=5)
        d.polygon([(128, 44), (202, 172), (54, 172)], outline=(*c2, 215), width=4)
        d.rectangle([76, 88, 180, 172], outline=(*c2, 195), width=3)
        # Central crimson-ruby octagram stone
        top_l = [(128, 50), (76, 128), (128, 118)]
        top_r = [(128, 50), (128, 118), (180, 128)]
        bot_l = [(76, 128), (128, 206), (128, 118)]
        bot_r = [(128, 118), (128, 206), (180, 128)]
        d.polygon(top_l, fill=(255, 112, 138, 255))
        d.polygon(top_r, fill=(*c1, 255))
        d.polygon(bot_l, fill=(178, 10, 44, 255))
        d.polygon(bot_r, fill=(116, 4, 28, 255))
        d.polygon([(128, 84), (154, 128), (128, 172), (102, 128)], fill=(255, 56, 92, 255), outline=(*c2, 255), width=3)
        d.polygon([(128, 50), (180, 128), (128, 206), (76, 128)], outline=(*c2, 255), width=4)
        draw_shaded_sphere(art, 120, 108, 10, (255, 255, 255), c2)

    styled = add_outline_and_shadow(art)
    final_hi = Image.alpha_composite(base, styled)
    return final_hi.resize((128, 128), resample=Image.Resampling.LANCZOS)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "alchemy_chaos", "assets", "ingredients")
    os.makedirs(out_dir, exist_ok=True)

    cols = 10
    rows = 5
    cell_w, cell_h = 152, 172
    sheet = Image.new("RGBA", (cols * cell_w + 24, rows * cell_h + 70), (18, 15, 26, 255))
    sd = ImageDraw.Draw(sheet)
    font_path = os.path.join(root, "alchemy_chaos", "assets", "fonts", "Nunito.ttf")
    title_font = ImageFont.truetype(font_path, 24)
    lbl_font = ImageFont.truetype(font_path, 13)
    sd.text((24, 18), "ALCHEMY CHAOS — 50 ORIGINAL ILLUSTRATED INGREDIENTS", fill=(246, 214, 132, 255), font=title_font)

    for idx, ing in enumerate(INGREDIENTS):
        icon = render_ingredient(ing)
        icon.save(os.path.join(out_dir, f"{ing['id']}.png"))
        c = idx % cols
        r = idx // cols
        x0 = 12 + c * cell_w
        y0 = 58 + r * cell_h
        sd.rounded_rectangle([x0 + 4, y0 + 4, x0 + cell_w - 4, y0 + cell_h - 4], radius=10, fill=(30, 25, 42, 255), outline=(142, 108, 56, 200), width=2)
        sheet.alpha_composite(icon, (x0 + 12, y0 + 8))
        label = f"#{ing['num']} {ing['name']}"
        sd.text((x0 + 10, y0 + 140), label[:19], fill=(235, 228, 215, 255), font=lbl_font)

    preview_path = os.path.join(root, "docs", "preview_ingredients_50.png")
    sheet.save(preview_path)
    print(f"Generated all {len(INGREDIENTS)} unique ingredient icons + contact sheet {preview_path}")


if __name__ == "__main__":
    main()
