#!/usr/bin/env python3
"""Deterministic original art and audio pipeline for SPACE MINER.

All images are synthesized here from mathematical noise, signed-distance shapes,
hand-drawn polygons and layered lighting. No stock images or emoji are used.
"""
from pathlib import Path
import math
import random
import wave
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "assets" / "art"
AUDIO = ROOT / "assets" / "audio"
for folder in [ART, AUDIO]:
    folder.mkdir(parents=True, exist_ok=True)
for name in ["backgrounds", "asteroids", "ships", "enemies", "icons", "particles", "station", "ui"]:
    (ART / name).mkdir(parents=True, exist_ok=True)

RNG = random.Random(4102026)
TAU = math.tau


def noise(w, h, seed, octaves=6):
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w), dtype=np.float32)
    total = 0.0
    for i in range(octaves):
        ow, oh = max(2, 2 ** (i + 1)), max(2, 2 ** (i + 1))
        a = rng.random((oh, ow), dtype=np.float32)
        im = Image.fromarray(np.uint8(a * 255), "L").resize((w, h), Image.Resampling.BICUBIC)
        weight = 0.54 ** i
        out += np.asarray(im, dtype=np.float32) / 255.0 * weight
        total += weight
    out /= total
    return np.clip(out, 0, 1)


def radial(w, h, cx=0.5, cy=0.5):
    y, x = np.mgrid[0:h, 0:w]
    return np.sqrt(((x / w - cx) * w / h) ** 2 + (y / h - cy) ** 2)


def save(im, path):
    im.save(path, optimize=True)
    print(path.relative_to(ROOT))


def glow_layer(size, xy, color, radius, alpha=255):
    layer = Image.new("RGBA", size)
    d = ImageDraw.Draw(layer)
    x, y = xy
    d.ellipse((x - radius / 5, y - radius / 5, x + radius / 5, y + radius / 5), fill=(*color, alpha))
    return layer.filter(ImageFilter.GaussianBlur(radius / 2))


# ---------------------------------------------------------------------------
# SECTOR BACKGROUNDS — multi-octave nebulae, star fields, textured planets
# ---------------------------------------------------------------------------
SECTORS = [
    ("starter_belt", (12, 30, 43), (33, 104, 113), (217, 164, 91), 11),
    ("crystal_nebula", (20, 11, 49), (120, 44, 135), (39, 190, 190), 23),
    ("iron_graveyard", (20, 19, 25), (104, 61, 45), (61, 91, 110), 37),
    ("plasma_storm", (9, 19, 38), (26, 90, 160), (121, 77, 206), 51),
    ("void_frontier", (7, 5, 20), (67, 24, 103), (17, 108, 115), 79),
]


def make_background(index, name, base, cloud, accent, seed):
    w, h = 1600, 900
    n1 = noise(w, h, seed, 7)
    n2 = noise(w, h, seed + 100, 5)
    r = radial(w, h, 0.68 - index * 0.06, 0.35 + index * 0.07)
    y, x = np.mgrid[0:h, 0:w]
    swirl = np.sin(x / w * 7 + y / h * 4 + n2 * 8 + index) * 0.5 + 0.5
    density = np.clip((n1 * 0.82 + swirl * 0.30 - r * 0.23 - 0.28) * 2.0, 0, 1)
    filaments = np.clip((n2 - 0.49) * 8, 0, 1) * density
    vignette = np.clip(1.12 - r * 0.83, 0.25, 1)
    arr = np.zeros((h, w, 3), dtype=np.float32)
    for c in range(3):
        arr[:, :, c] = base[c] * 0.33 + cloud[c] * density * 0.56 + accent[c] * filaments * 0.26
    arr *= vignette[:, :, None]
    # Fine dust lane and distant galactic band.
    band = np.exp(-((y / h - (0.18 + x / w * 0.55)) ** 2) / 0.012) * (0.25 + n2 * 0.75)
    arr += band[:, :, None] * np.array(accent)[None, None, :] * 0.10
    im = Image.fromarray(np.uint8(np.clip(arr, 0, 255)), "RGB").convert("RGBA")

    # A unique, fully shaded textured planet per sector (never a plain circle).
    pr = [190, 150, 210, 135, 175][index]
    px, py = [(1280, 170), (280, 180), (1270, 650), (1320, 200), (310, 640)][index]
    pn = noise(pr * 2, pr * 2, seed + 900, 7)
    yy, xx = np.mgrid[-1:1:complex(pr * 2), -1:1:complex(pr * 2)]
    rr = xx * xx + yy * yy
    mask = rr <= 1
    z = np.sqrt(np.clip(1 - rr, 0, 1))
    light = np.clip(-xx * 0.48 - yy * 0.30 + z * 0.76, 0, 1)
    surface = np.clip(0.24 + light * 0.78 + (pn - 0.5) * 0.45, 0, 1)
    # Latitudinal gas bands / cracked rocky bands.
    bands = (np.sin((yy + pn * 0.15) * (22 + index * 5)) * 0.5 + 0.5) * 0.16
    surface = np.clip(surface + bands * (0.4 if index != 4 else 0.1), 0, 1)
    pcol = np.array([accent, cloud, (112, 105, 99), (66, 103, 153), (60, 40, 91)][index])
    pa = np.zeros((pr * 2, pr * 2, 4), dtype=np.uint8)
    for c in range(3):
        pa[:, :, c] = np.uint8(np.clip(pcol[c] * surface + 8, 0, 255))
    pa[:, :, 3] = np.where(mask, 255, 0).astype(np.uint8)
    planet = Image.fromarray(pa, "RGBA")
    atmosphere = Image.new("RGBA", im.size)
    ad = ImageDraw.Draw(atmosphere)
    ad.ellipse((px - pr - 8, py - pr - 8, px + pr + 8, py + pr + 8), outline=(*accent, 100), width=12)
    atmosphere = atmosphere.filter(ImageFilter.GaussianBlur(13))
    im = Image.alpha_composite(im, atmosphere)
    im.alpha_composite(planet, (px - pr, py - pr))

    # Stars, with restrained bloom and occasional diffraction spikes.
    stars = Image.new("RGBA", (w, h))
    sd = ImageDraw.Draw(stars)
    sr = random.Random(seed * 19)
    for i in range(720):
        sx, sy = sr.randrange(w), sr.randrange(h)
        lum = sr.randrange(70, 220)
        tint = accent if i % 9 == 0 else (190, 214, 229)
        col = tuple(int(tint[c] * 0.35 + lum * 0.65) for c in range(3))
        rad = sr.choices([1, 1, 1, 2, 2, 3], [35, 30, 20, 10, 4, 1])[0]
        sd.ellipse((sx - rad, sy - rad, sx + rad, sy + rad), fill=(*col, sr.randrange(120, 255)))
        if rad >= 3:
            sd.line((sx - 14, sy, sx + 14, sy), fill=(*col, 90), width=1)
            sd.line((sx, sy - 14, sx, sy + 14), fill=(*col, 90), width=1)
    im = Image.alpha_composite(im, stars.filter(ImageFilter.GaussianBlur(0.35)))
    # Fine foreground dust.
    dust = Image.new("RGBA", (w, h))
    dd = ImageDraw.Draw(dust)
    for i in range(110):
        sx, sy = sr.randrange(w), sr.randrange(h)
        dd.line((sx, sy, sx + sr.randrange(2, 9), sy - 1), fill=(*accent, sr.randrange(8, 28)), width=1)
    im = Image.alpha_composite(im, dust)
    save(im.convert("RGB"), ART / "backgrounds" / f"sector_{index + 1}_{name}.jpg")


for i, (name, base, cloud, accent, seed) in enumerate(SECTORS):
    make_background(i, name, base, cloud, accent, seed)


# ---------------------------------------------------------------------------
# ASTEROIDS — irregular silhouettes, lit stone, craters, veins and deposits
# ---------------------------------------------------------------------------
AST_PALETTES = [
    [(118, 124, 126), (181, 134, 85), (115, 156, 158)],
    [(73, 75, 108), (75, 214, 206), (191, 92, 218)],
    [(79, 83, 88), (168, 94, 59), (115, 135, 143)],
    [(50, 65, 86), (75, 157, 238), (150, 93, 231)],
    [(37, 33, 53), (104, 69, 154), (39, 178, 164)],
]


def make_asteroid(sector, variant):
    S = 256
    seed = 1000 + sector * 100 + variant
    rng = random.Random(seed)
    base, ore1, ore2 = AST_PALETTES[sector]
    # Harmonically distorted radial silhouette.
    count = 96
    phases = [rng.random() * TAU for _ in range(4)]
    amps = [rng.uniform(0.05, 0.14) for _ in range(4)]
    points = []
    for i in range(count):
        a = i / count * TAU
        rad = 98
        for h in range(4):
            rad += math.sin(a * (h + 2) + phases[h]) * amps[h] * 70
        rad += rng.uniform(-3.5, 3.5)
        points.append((128 + math.cos(a) * rad, 128 + math.sin(a) * rad))
    mask = Image.new("L", (S, S))
    ImageDraw.Draw(mask).polygon(points, fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(0.55))
    n = noise(S, S, seed, 6)
    fine = noise(S, S, seed + 43, 8)
    yy, xx = np.mgrid[0:S, 0:S]
    light = np.clip(0.74 - xx / S * 0.34 - yy / S * 0.28, 0.2, 1)
    shade = np.clip(0.37 + n * 0.75 + (fine - 0.5) * 0.26, 0, 1) * light
    arr = np.zeros((S, S, 4), dtype=np.uint8)
    for c in range(3):
        arr[:, :, c] = np.uint8(np.clip(base[c] * (0.45 + shade) + fine * 22, 0, 255))
    arr[:, :, 3] = np.asarray(mask)
    im = Image.fromarray(arr, "RGBA")

    # Craters are shaded bowls with lit far rims.
    craters = Image.new("RGBA", (S, S))
    cd = ImageDraw.Draw(craters)
    for _ in range(rng.randrange(9, 16)):
        cx, cy = rng.randrange(42, 215), rng.randrange(42, 215)
        rx, ry = rng.randrange(5, 20), rng.randrange(4, 16)
        cd.ellipse((cx - rx - 2, cy - ry - 1, cx + rx + 2, cy + ry + 3), fill=(190, 200, 202, 35))
        cd.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=(12, 17, 22, rng.randrange(65, 130)))
        cd.arc((cx - rx, cy - ry, cx + rx, cy + ry), 195, 338, fill=(225, 224, 207, 65), width=2)
        cd.arc((cx - rx + 2, cy - ry + 2, cx + rx - 2, cy + ry - 2), 15, 155, fill=(0, 0, 0, 70), width=2)
    craters.putalpha(Image.composite(craters.getchannel("A"), Image.new("L", (S, S)), mask))
    im = Image.alpha_composite(im, craters)

    # Branching luminous ore veins and raised mineral crystals.
    details = Image.new("RGBA", (S, S))
    d = ImageDraw.Draw(details)
    for vein in range(4 + sector):
        x, y = rng.randrange(55, 200), rng.randrange(55, 200)
        col = ore1 if vein % 2 == 0 else ore2
        path = [(x, y)]
        for k in range(rng.randrange(3, 7)):
            x += rng.randrange(-17, 18)
            y += rng.randrange(5, 19)
            path.append((x, y))
        d.line(path, fill=(8, 10, 15, 160), width=5)
        d.line(path, fill=(*col, 160), width=2)
        for x, y in path[1:-1:2]:
            r = rng.randrange(3, 7)
            poly = [(x, y - r * 2), (x + r, y), (x + r // 2, y + r), (x - r, y + r // 2)]
            d.polygon(poly, fill=(*col, 230), outline=(225, 244, 241, 170))
            d.line((x, y - r * 2, x, y + r // 2), fill=(255, 255, 255, 120), width=1)
    details.putalpha(Image.composite(details.getchannel("A"), Image.new("L", (S, S)), mask))
    glow = details.filter(ImageFilter.GaussianBlur(4))
    im = Image.alpha_composite(im, glow)
    im = Image.alpha_composite(im, details)
    # Bevel silhouette and surface pits.
    edge = mask.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.GaussianBlur(0.5))
    edge_arr = np.asarray(edge)
    ea = np.zeros((S, S, 4), dtype=np.uint8)
    ea[:, :, :3] = np.array([180, 194, 194], dtype=np.uint8)
    ea[:, :, 3] = np.uint8(edge_arr.astype(np.float32) * 0.28)
    im = Image.alpha_composite(im, Image.fromarray(ea, "RGBA"))
    save(im, ART / "asteroids" / f"sector_{sector + 1}_rock_{variant + 1}.png")


for s in range(5):
    for v in range(6):
        make_asteroid(s, v)


# ---------------------------------------------------------------------------
# SPRITE HELPERS — supersampled industrial hard-surface illustration
# ---------------------------------------------------------------------------
def canvas(size=256, scale=3):
    im = Image.new("RGBA", (size * scale, size * scale))
    return im, ImageDraw.Draw(im), scale


def poly(draw, pts, fill, outline=None, width=1, s=3):
    p = [(int(x * s), int(y * s)) for x, y in pts]
    draw.polygon(p, fill=fill)
    if outline:
        draw.line(p + [p[0]], fill=outline, width=width * s, joint="curve")


def line(draw, pts, fill, width=1, s=3):
    draw.line([(int(x * s), int(y * s)) for x, y in pts], fill=fill, width=max(1, int(width * s)), joint="curve")


def ellipse(draw, box, fill, outline=None, width=1, s=3):
    b = tuple(int(v * s) for v in box)
    draw.ellipse(b, fill=fill, outline=outline, width=max(1, int(width * s)))


def finish_sprite(im, path, size=256):
    im = im.resize((size, size), Image.Resampling.LANCZOS)
    save(im, path)


def ship_sprite(class_id):
    # Faces right. Every class has a unique hull and equipment layout.
    im, d, s = canvas()
    palettes = [(85, 119, 129), (112, 100, 71), (78, 94, 132), (77, 65, 101)]
    metal = palettes[class_id]
    dark = tuple(int(v * 0.33) for v in metal)
    mid = tuple(int(v * 0.78) for v in metal)
    light = tuple(min(255, int(v * 1.45 + 20)) for v in metal)
    accent = [(76, 224, 211), (239, 178, 80), (102, 176, 255), (186, 116, 255)][class_id]
    # Engine glow behind nozzle.
    for ey in ([100, 156] if class_id != 3 else [79, 128, 177]):
        gl = glow_layer(im.size, (43 * s, ey * s), accent, 22 * s, 190)
        im = Image.alpha_composite(im, gl)
        d = ImageDraw.Draw(im)
    if class_id == 0:  # Prospector
        hull = [(44, 91), (92, 73), (152, 79), (207, 116), (222, 128), (207, 140), (152, 177), (92, 183), (44, 165), (61, 128)]
        wings = [[(83, 83), (109, 48), (141, 50), (137, 82)], [(83, 173), (109, 208), (141, 206), (137, 174)]]
        pods = [(53, 95, 91, 111), (53, 145, 91, 161)]
    elif class_id == 1:  # Harvester
        hull = [(37, 72), (91, 52), (173, 69), (219, 109), (230, 128), (219, 147), (173, 187), (91, 204), (37, 184), (53, 128)]
        wings = [[(72, 70), (73, 31), (145, 39), (166, 72)], [(72, 186), (73, 225), (145, 217), (166, 184)]]
        pods = [(42, 66, 93, 90), (42, 166, 93, 190), (45, 113, 77, 143)]
    elif class_id == 2:  # Lancet
        hull = [(42, 112), (116, 86), (183, 97), (239, 128), (183, 159), (116, 170), (42, 144), (66, 128)]
        wings = [[(87, 96), (137, 49), (179, 56), (149, 99)], [(87, 160), (137, 207), (179, 200), (149, 157)]]
        pods = [(49, 105, 91, 119), (49, 137, 91, 151)]
    else:  # Leviathan
        hull = [(30, 69), (82, 43), (166, 54), (218, 91), (238, 128), (218, 165), (166, 202), (82, 213), (30, 187), (48, 128)]
        wings = [[(66, 63), (76, 20), (154, 30), (181, 63)], [(66, 193), (76, 236), (154, 226), (181, 193)]]
        pods = [(34, 61, 80, 87), (34, 169, 80, 195), (38, 112, 76, 144)]
    for w in wings:
        poly(d, w, dark, (20, 29, 36, 255), 2, s)
        # Wing armour inset.
        cx = sum(p[0] for p in w) / len(w); cy = sum(p[1] for p in w) / len(w)
        inner = [(cx + (x - cx) * .78, cy + (y - cy) * .72) for x, y in w]
        poly(d, inner, mid, (*light, 180), 1, s)
    poly(d, hull, dark, (12, 21, 28, 255), 3, s)
    cx, cy = 128, 128
    inner = [(cx + (x - cx) * .93, cy + (y - cy) * .84) for x, y in hull]
    poly(d, inner, mid, (*light, 220), 2, s)
    # Layered nose armour.
    nose = [(132, 92 if class_id != 1 else 83), (190, 103), (222, 128), (190, 153), (132, 164 if class_id != 1 else 173), (155, 128)]
    poly(d, nose, (*light, 255), (24, 39, 45, 255), 2, s)
    line(d, [(155, 128), (211, 128)], (*accent, 190), 2, s)
    # Cockpit glass with frame and specular strip.
    ellipse(d, (124, 105, 177, 151), (11, 29, 39, 255), (136, 185, 191, 255), 2, s)
    ellipse(d, (134, 111, 169, 140), (*accent, 150), (193, 237, 231, 170), 1, s)
    line(d, [(142, 115), (160, 115), (167, 121)], (224, 252, 255, 190), 2, s)
    # Engine pods and animated-nozzle hardware.
    for box in pods:
        x1, y1, x2, y2 = box
        ellipse(d, box, (21, 30, 37, 255), (*light, 255), 2, s)
        ellipse(d, (x1 + 3, y1 + 4, x1 + 16, y2 - 4), (5, 12, 18, 255), (*accent, 220), 2, s)
        for xx in np.linspace(x1 + 20, x2 - 4, 3):
            line(d, [(xx, y1 + 3), (xx, y2 - 3)], (19, 32, 39, 220), 2, s)
    # Panel seams, rivets, cargo vents.
    for yy in [91, 165]:
        line(d, [(88, yy), (130, yy + (5 if yy < 128 else -5))], (22, 38, 44, 220), 2, s)
    for x in range(80, 122, 9):
        line(d, [(x, 120), (x + 3, 136)], (15, 28, 33, 210), 2, s)
    for x, y in [(92, 91), (92, 165), (179, 107), (179, 149), (69, 128)]:
        ellipse(d, (x - 2, y - 2, x + 2, y + 2), (*accent, 255), None, 1, s)
    # Mining rig: articulated arms and focused emitter.
    rig_y = 78 if class_id != 3 else 58
    line(d, [(120, rig_y), (174, rig_y - 10), (205, 102)], (24, 36, 43, 255), 7, s)
    line(d, [(120, rig_y), (174, rig_y - 10), (205, 102)], (*light, 255), 3, s)
    ellipse(d, (198, 96, 211, 108), (16, 26, 33, 255), (*accent, 255), 2, s)
    # Class-specific attachments.
    if class_id in (1, 3):
        for yy in [58, 198]:
            for x in range(91, 153, 18):
                poly(d, [(x, yy - 8), (x + 14, yy - 8), (x + 14, yy + 8), (x, yy + 8)], (38, 44, 45, 255), (*accent, 130), 1, s)
    if class_id == 2:
        line(d, [(172, 86), (218, 68)], (*light, 255), 4, s)
        line(d, [(172, 170), (218, 188)], (*light, 255), 4, s)
    finish_sprite(im, ART / "ships" / f"ship_{class_id + 1}.png")


for i in range(4):
    ship_sprite(i)


def enemy_sprite(kind):
    im, d, s = canvas(192, 3)
    accents = [(238, 92, 89), (240, 139, 68), (211, 74, 65), (93, 157, 255), (179, 92, 255)]
    ac = accents[kind]
    metal = [(96, 100, 109), (114, 86, 72), (91, 94, 100), (67, 81, 113), (66, 49, 88)][kind]
    dark = tuple(int(v * .35) for v in metal)
    light = tuple(min(255, int(v * 1.55 + 10)) for v in metal)
    # All ships face right; silhouettes are deliberately distinct.
    if kind == 0:
        hull = [(25, 75), (76, 57), (135, 69), (169, 96), (135, 123), (76, 135), (25, 117), (47, 96)]
        extras = [[(54, 68), (81, 28), (117, 43), (107, 68)], [(54, 124), (81, 164), (117, 149), (107, 124)]]
    elif kind == 1:
        hull = [(20, 59), (94, 43), (148, 67), (174, 96), (148, 125), (94, 149), (20, 133), (52, 96)]
        extras = [[(61, 53), (47, 18), (120, 31), (137, 61)], [(61, 139), (47, 174), (120, 161), (137, 131)]]
    elif kind == 2:
        hull = [(20, 47), (70, 27), (136, 42), (177, 78), (177, 114), (136, 150), (70, 165), (20, 145), (38, 96)]
        extras = [[(53, 38), (67, 12), (118, 20), (134, 42)], [(53, 154), (67, 180), (118, 172), (134, 150)]]
    elif kind == 3:
        hull = [(30, 72), (96, 51), (144, 66), (177, 96), (144, 126), (96, 141), (30, 120), (55, 96)]
        extras = [[(88, 58), (137, 15), (158, 23), (129, 71)], [(88, 134), (137, 177), (158, 169), (129, 121)]]
    else:
        hull = [(34, 51), (96, 27), (154, 51), (178, 96), (154, 141), (96, 165), (34, 141), (17, 96)]
        extras = [[(50, 44), (96, 7), (142, 44), (126, 55), (96, 33), (66, 55)], [(50, 148), (96, 185), (142, 148), (126, 137), (96, 159), (66, 137)]]
    for p in extras:
        poly(d, p, dark, (*light, 220), 2, s)
    poly(d, hull, dark, (10, 10, 18, 255), 3, s)
    inner = [(96 + (x - 96) * .88, 96 + (y - 96) * .82) for x, y in hull]
    poly(d, inner, metal, (*light, 255), 2, s)
    # Armour segments and unique glowing core.
    for k in range(3 + kind):
        x = 50 + k * (76 / (2 + kind))
        line(d, [(x, 61), (x + 7, 96), (x, 131)], (*dark, 230), 3, s)
    if kind == 4:
        ellipse(d, (59, 59, 133, 133), (9, 7, 20, 255), (*ac, 255), 4, s)
        ellipse(d, (77, 77, 115, 115), (*ac, 190), (231, 207, 255, 230), 2, s)
        ellipse(d, (89, 89, 103, 103), (245, 232, 255, 255), None, 1, s)
    else:
        poly(d, [(91, 72), (137, 80), (156, 96), (137, 112), (91, 120), (107, 96)], (12, 18, 26, 255), (*ac, 255), 2, s)
        ellipse(d, (119, 87, 143, 105), (*ac, 220), (255, 228, 215, 200), 1, s)
    # Weapons and engines.
    for yy in [48, 144]:
        line(d, [(91, yy), (155, yy + (18 if yy < 96 else -18))], (*light, 255), 5, s)
        ellipse(d, (149, yy + (12 if yy < 96 else -24), 164, yy + (26 if yy < 96 else -10)), (15, 15, 22, 255), (*ac, 255), 2, s)
    for yy in [70, 122]:
        ellipse(d, (25, yy - 10, 51, yy + 10), (13, 17, 24, 255), (*ac, 230), 2, s)
    finish_sprite(im, ART / "enemies" / f"enemy_{kind + 1}.png", 192)


for i in range(5):
    enemy_sprite(i)

# Mining drone: small hexagonal industrial craft with manipulators.
im, d, s = canvas(128, 3)
poly(d, [(19, 47), (48, 25), (88, 30), (111, 64), (88, 98), (48, 103), (19, 81), (32, 64)], (40, 57, 63, 255), (137, 180, 181, 255), 2, s)
poly(d, [(46, 41), (79, 38), (98, 64), (79, 90), (46, 87), (58, 64)], (82, 111, 113, 255), (29, 43, 48, 255), 2, s)
ellipse(d, (67, 51, 91, 77), (19, 50, 54, 255), (90, 237, 213, 255), 2, s)
for yy in [28, 100]:
    line(d, [(54, yy), (90, yy + (15 if yy < 64 else -15)), (112, yy + (23 if yy < 64 else -23))], (116, 153, 153, 255), 4, s)
    ellipse(d, (105, yy + (17 if yy < 64 else -29), 117, yy + (29 if yy < 64 else -17)), (16, 27, 31, 255), (97, 235, 211, 255), 1, s)
for yy in [47, 81]:
    ellipse(d, (17, yy - 8, 36, yy + 8), (11, 24, 29, 255), (86, 216, 205, 255), 2, s)
finish_sprite(im, ART / "ships" / "mining_drone.png", 128)


# ---------------------------------------------------------------------------
# STATION — original orbital relay, rings, docks, windows, antennae
# ---------------------------------------------------------------------------
im, d, s = canvas(512, 2)
# Soft glow behind station.
im = Image.alpha_composite(im, glow_layer(im.size, (256 * s, 256 * s), (48, 168, 163), 180 * s, 50))
d = ImageDraw.Draw(im)
ellipse(d, (70, 156, 442, 356), (15, 25, 32, 110), (74, 117, 124, 220), 10, s)
ellipse(d, (97, 180, 415, 332), (5, 12, 19, 0), (31, 54, 62, 255), 22, s)
# Radial dock arms.
for a in [0, math.pi / 2, math.pi, math.pi * 1.5]:
    ux, uy = math.cos(a), math.sin(a)
    vx, vy = -uy, ux
    p = [(256 + ux * 75 + vx * 24, 256 + uy * 75 + vy * 24),
         (256 + ux * 197 + vx * 33, 256 + uy * 197 + vy * 33),
         (256 + ux * 205 - vx * 33, 256 + uy * 205 - vy * 33),
         (256 + ux * 75 - vx * 24, 256 + uy * 75 - vy * 24)]
    poly(d, p, (52, 70, 78, 255), (123, 153, 155, 255), 2, s)
    for rr in range(100, 190, 22):
        line(d, [(256 + ux * rr + vx * 19, 256 + uy * rr + vy * 19), (256 + ux * rr - vx * 19, 256 + uy * rr - vy * 19)], (20, 34, 40, 255), 3, s)
# Central armoured core.
poly(d, [(256, 135), (344, 174), (378, 256), (344, 338), (256, 377), (168, 338), (134, 256), (168, 174)], (43, 61, 69, 255), (137, 169, 168, 255), 3, s)
poly(d, [(256, 161), (322, 191), (347, 256), (322, 321), (256, 351), (190, 321), (165, 256), (190, 191)], (73, 94, 98, 255), (22, 38, 44, 255), 3, s)
ellipse(d, (199, 199, 313, 313), (13, 31, 39, 255), (103, 205, 193, 255), 4, s)
ellipse(d, (220, 220, 292, 292), (33, 83, 88, 255), (157, 232, 214, 220), 2, s)
ellipse(d, (239, 239, 273, 273), (128, 225, 206, 255), None, 1, s)
# Docking lights and window rows.
for a in np.linspace(0, TAU, 24, endpoint=False):
    x, y = 256 + math.cos(a) * 113, 256 + math.sin(a) * 113
    ellipse(d, (x - 3, y - 3, x + 3, y + 3), (239, 183, 91, 255) if int(a * 10) % 3 == 0 else (83, 219, 204, 255), None, 1, s)
for x, y in [(256, 77), (435, 256), (256, 435), (77, 256)]:
    ellipse(d, (x - 24, y - 24, x + 24, y + 24), (24, 40, 47, 255), (101, 149, 151, 255), 3, s)
    ellipse(d, (x - 9, y - 9, x + 9, y + 9), (70, 215, 201, 255), None, 1, s)
line(d, [(256, 137), (256, 71), (290, 42)], (128, 156, 159, 255), 4, s)
ellipse(d, (283, 34, 298, 49), (227, 174, 82, 255), None, 1, s)
finish_sprite(im, ART / "station" / "ceres_relay.png", 512)


# ---------------------------------------------------------------------------
# RESOURCE ICONS — 15 distinct mineral / isotope illustrations
# ---------------------------------------------------------------------------
RESOURCES = [
    ("iron", (159, 174, 178), "ore"), ("copper", (206, 119, 72), "ore"),
    ("nickel", (163, 184, 169), "ore"), ("titanium", (157, 181, 205), "ingot"),
    ("silver", (213, 226, 230), "ingot"), ("gold", (237, 190, 69), "ingot"),
    ("cobalt", (72, 125, 199), "ore"), ("platinum", (182, 210, 211), "ingot"),
    ("uranium", (118, 208, 92), "isotope"), ("lumen_crystal", (71, 224, 207), "crystal"),
    ("iridium", (177, 152, 205), "ore"), ("plasma_ore", (92, 141, 255), "plasma"),
    ("void_crystal", (178, 91, 239), "crystal"), ("neutronium", (232, 115, 170), "isotope"),
    ("quantum_core", (93, 231, 226), "core"),
]


def resource_icon(idx, name, color, shape):
    S = 96
    im, d, s = canvas(S, 4)
    # Quiet backing plate; mineral itself is original vector/raster art.
    ellipse(d, (8, 8, 88, 88), (9, 20, 28, 210), (*color, 80), 1, s)
    im = Image.alpha_composite(im, glow_layer(im.size, (48 * s, 47 * s), color, 31 * s, 105))
    d = ImageDraw.Draw(im)
    dark = tuple(int(c * .35) for c in color)
    light = tuple(min(255, int(c * 1.25 + 40)) for c in color)
    if shape == "ore":
        pts = [(22, 55), (27, 31), (45, 20), (69, 27), (78, 48), (65, 72), (40, 77)]
        poly(d, pts, (*dark, 255), (*light, 230), 2, s)
        poly(d, [(27, 32), (46, 24), (54, 46), (35, 55)], (*color, 255), (*light, 180), 1, s)
        poly(d, [(54, 46), (72, 34), (74, 51), (61, 67)], (*dark, 255), None, 1, s)
        for x, y in [(39, 37), (57, 31), (58, 58), (37, 63)]:
            poly(d, [(x, y - 6), (x + 5, y), (x, y + 5), (x - 4, y)], (*light, 255), None, 1, s)
    elif shape == "ingot":
        poly(d, [(20, 57), (36, 31), (73, 36), (79, 54), (63, 70), (28, 67)], (*dark, 255), (*light, 255), 2, s)
        poly(d, [(36, 31), (73, 36), (63, 51), (25, 47)], (*light, 255), None, 1, s)
        poly(d, [(25, 47), (63, 51), (63, 70), (28, 67)], (*color, 255), None, 1, s)
        line(d, [(36, 55), (54, 57)], (*dark, 220), 2, s)
    elif shape == "crystal":
        crystals = [((48, 13), 17, 67), ((27, 30), 12, 43), ((68, 28), 13, 46)]
        for (cx, top), rw, hh in crystals:
            poly(d, [(cx, top), (cx + rw, top + hh * .38), (cx + rw * .65, top + hh), (cx - rw * .65, top + hh), (cx - rw, top + hh * .38)], (*color, 240), (*light, 255), 2, s)
            poly(d, [(cx, top), (cx + rw, top + hh * .38), (cx, top + hh * .53)], (*light, 150), None, 1, s)
            line(d, [(cx, top), (cx, top + hh)], (238, 255, 255, 150), 1, s)
    elif shape == "isotope":
        for a in [0, TAU / 3, TAU * 2 / 3]:
            # Orbit ellipses rotated on separate layers.
            orb = Image.new("RGBA", im.size)
            od = ImageDraw.Draw(orb)
            od.ellipse((18 * s, 37 * s, 78 * s, 59 * s), outline=(*color, 210), width=2 * s)
            orb = orb.rotate(math.degrees(a), resample=Image.Resampling.BICUBIC, center=(48 * s, 48 * s))
            im = Image.alpha_composite(im, orb)
        d = ImageDraw.Draw(im)
        ellipse(d, (36, 36, 60, 60), (*color, 255), (*light, 255), 2, s)
        ellipse(d, (43, 40, 50, 47), (255, 255, 255, 200), None, 1, s)
    elif shape == "plasma":
        pts = []
        for i in range(18):
            a = i / 18 * TAU
            rr = 29 + math.sin(a * 5 + idx) * 6
            pts.append((48 + math.cos(a) * rr, 48 + math.sin(a) * rr))
        poly(d, pts, (*dark, 255), (*color, 255), 2, s)
        ellipse(d, (29, 29, 67, 67), (*color, 170), (*light, 255), 2, s)
        line(d, [(28, 51), (40, 43), (47, 53), (58, 36), (70, 45)], (233, 248, 255, 240), 2, s)
    else:
        ellipse(d, (20, 20, 76, 76), (*dark, 255), (*color, 255), 3, s)
        poly(d, [(48, 22), (69, 48), (48, 74), (27, 48)], (*color, 220), (*light, 255), 2, s)
        ellipse(d, (39, 39, 57, 57), (221, 255, 250, 255), None, 1, s)
    finish_sprite(im, ART / "icons" / f"resource_{name}.png", S)


for i, (name, color, shape) in enumerate(RESOURCES):
    resource_icon(i, name, color, shape)


# Upgrade / UI pictograms, drawn as custom line-art.
UPGRADE_ICONS = ["laser", "focus", "cargo", "engine", "hull", "shield", "reactor", "scanner", "weapon", "drone", "tractor", "research"]
for i, name in enumerate(UPGRADE_ICONS):
    im, d, s = canvas(64, 4)
    c = (100, 225, 208, 255)
    if name == "laser":
        line(d, [(10, 50), (37, 23)], c, 5, s); line(d, [(33, 14), (54, 35)], c, 3, s); line(d, [(43, 11), (55, 23)], (234, 190, 91, 255), 2, s)
    elif name == "focus":
        for r in [8, 17, 25]: ellipse(d, (32-r, 32-r, 32+r, 32+r), (0,0,0,0), c, 2, s)
        line(d, [(32, 4), (32, 60)], c, 1, s); line(d, [(4, 32), (60, 32)], c, 1, s)
    elif name == "cargo":
        poly(d, [(8,20),(32,8),(56,20),(56,47),(32,58),(8,47)], (0,0,0,0), c, 2, s); line(d, [(8,20),(32,33),(56,20)], c, 2, s); line(d, [(32,33),(32,58)], c, 2, s)
    elif name == "engine":
        poly(d, [(14,14),(43,20),(52,32),(43,44),(14,50),(24,32)], (0,0,0,0), c, 2, s); line(d, [(8,24),(20,24)], (237,180,79,255), 2, s); line(d, [(5,32),(20,32)], c, 3, s); line(d, [(8,40),(20,40)], (237,180,79,255), 2, s)
    elif name == "hull":
        poly(d, [(32,6),(53,14),(50,40),(32,57),(14,40),(11,14)], (0,0,0,0), c, 3, s); line(d, [(32,13),(32,47)], c, 2, s)
    elif name == "shield":
        for r in [11,22,28]: ellipse(d, (32-r,32-r,32+r,32+r), (0,0,0,0), (*c[:3],150), 2, s)
        line(d, [(32,7),(32,57)], c, 2, s)
    elif name == "reactor":
        for a in [0,120,240]:
            ellipse(d, (25,25,39,39), c, None, 1, s)
        for r in [15,27]: ellipse(d, (32-r,32-r,32+r,32+r), (0,0,0,0), c, 2, s)
    elif name == "scanner":
        for r in [10,20,28]: d.arc(tuple(int(v*s) for v in (32-r,32-r,32+r,32+r)), 200, 340, fill=c, width=2*s)
        line(d, [(32,32),(52,15)], c, 3, s); ellipse(d, (28,28,36,36), c, None, 1, s)
    elif name == "weapon":
        line(d, [(8,45),(48,17)], c, 6, s); line(d, [(31,13),(53,35)], c, 3, s); line(d, [(13,52),(22,43)], (237,180,79,255), 3, s)
    elif name == "drone":
        poly(d, [(32,9),(52,21),(52,43),(32,55),(12,43),(12,21)], (0,0,0,0), c, 2, s); ellipse(d, (25,25,39,39), c, None, 1, s); line(d, [(5,12),(18,22)], c, 2, s); line(d, [(59,12),(46,22)], c, 2, s)
    elif name == "tractor":
        for r in [10,20,28]: d.arc(tuple(int(v*s) for v in (32-r,32-r,32+r,32+r)), 300, 60, fill=c, width=2*s)
        poly(d, [(23,24),(41,24),(46,43),(18,43)], (0,0,0,0), c, 2, s)
    else:
        poly(d, [(32,7),(38,24),(56,25),(42,36),(47,54),(32,44),(17,54),(22,36),(8,25),(26,24)], (0,0,0,0), c, 2, s)
    finish_sprite(im, ART / "icons" / f"upgrade_{name}.png", 64)

# Particle textures.
for name, color in [("glow", (107, 235, 218)), ("spark", (238, 207, 132)), ("plasma", (111, 150, 255)), ("void", (184, 103, 255))]:
    S = 64
    y, x = np.mgrid[-1:1:complex(S), -1:1:complex(S)]
    r = np.sqrt(x*x + y*y)
    a = np.clip(1-r, 0, 1) ** 2
    arr = np.zeros((S,S,4),dtype=np.uint8)
    for c in range(3): arr[:,:,c] = color[c]
    arr[:,:,3] = np.uint8(a*255)
    save(Image.fromarray(arr,"RGBA"), ART/"particles"/f"{name}.png")
# Exhaust flame with bright core and soft transparent edges.
S=64; y,x=np.mgrid[-1:1:complex(S),-1:1:complex(S)]
a=np.clip(1-np.sqrt((x*.85)**2+(y*1.5)**2),0,1)**1.7
arr=np.zeros((S,S,4),dtype=np.uint8); arr[:,:,0]=80; arr[:,:,1]=np.uint8(np.clip(a*260,0,255)); arr[:,:,2]=np.uint8(np.clip(a*300,0,255)); arr[:,:,3]=np.uint8(a*220)
save(Image.fromarray(arr,"RGBA"),ART/"particles"/"thruster.png")

# Application icon: miniature ship over a mineral monogram.
icon = Image.new("RGBA", (256,256), (7,17,25,255))
bg = Image.fromarray(np.uint8(np.stack([noise(256,256,707)*20+4, noise(256,256,708)*35+12, noise(256,256,709)*45+20],axis=2)),"RGB").convert("RGBA")
icon = Image.blend(icon,bg,.55)
d=ImageDraw.Draw(icon)
d.rounded_rectangle((10,10,246,246),radius=48,outline=(70,207,190,220),width=5)
d.arc((38,38,218,218),205,500,fill=(53,127,130,180),width=4)
ship=Image.open(ART/"ships"/"ship_1.png").resize((205,205),Image.Resampling.LANCZOS)
icon.alpha_composite(ship,(29,26))
d=ImageDraw.Draw(icon); d.line((49,200,207,200),fill=(81,221,204,230),width=3); d.line((78,211,178,211),fill=(211,166,80,180),width=2)
save(icon,ART/"ui"/"app_icon.png")
# Loading emblem without text.
save(icon.resize((128,128),Image.Resampling.LANCZOS),ART/"ui"/"loading_emblem.png")


# ---------------------------------------------------------------------------
# AUDIO — synthesized, loopable atmospheres and responsive effects
# ---------------------------------------------------------------------------
SR = 22050

def write_wav(name, samples):
    samples = np.clip(samples, -1, 1)
    pcm = np.int16(samples * 32767)
    with wave.open(str(AUDIO / (name + ".wav")), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    print((AUDIO / (name + ".wav")).relative_to(ROOT))


def ambient(idx):
    dur = 16
    t = np.arange(SR * dur) / SR
    roots = [55.0, 61.74, 49.0, 65.41, 43.65]
    f = roots[idx]
    rng = np.random.default_rng(900 + idx)
    pad = (np.sin(2*np.pi*f*t + np.sin(2*np.pi*.07*t)*.5)*.12 +
           np.sin(2*np.pi*f*1.5*t)*.045 + np.sin(2*np.pi*f*2.005*t)*.035)
    shimmer = np.sin(2*np.pi*(f*4 + np.sin(t*.2)*2)*t)*.018
    noise_raw = rng.normal(0, 1, len(t))
    # Cheap smooth filtered noise, loop-safe enough with crossfade.
    kernel = np.ones(160)/160
    smoothed = np.convolve(noise_raw, kernel, mode="same") * .06
    pulse = (np.sin(2*np.pi*(.08+idx*.01)*t)*.5+.5)
    sig = (pad + shimmer + smoothed) * (.65 + pulse*.35)
    # Short edge crossfade prevents clicks when looping.
    edge = SR//2
    sig[:edge] *= np.linspace(0,1,edge); sig[-edge:] *= np.linspace(1,0,edge)
    write_wav(f"ambient_sector_{idx+1}",sig)


for i in range(5): ambient(i)

def effect(name,dur,fn):
    t=np.arange(int(SR*dur))/SR
    write_wav(name,fn(t))


effect("laser",.45,lambda t:(np.sin(2*np.pi*(160*t+900*t*t))*.10+np.random.default_rng(1).normal(0,.025,len(t)))*np.sin(np.pi*t/.45)**.5)
effect("cannon",.22,lambda t:np.sin(2*np.pi*(540*t-900*t*t))*.30*np.exp(-t*18))
effect("enemy_shot",.28,lambda t:np.sin(2*np.pi*(230*t-260*t*t))*.25*np.exp(-t*11))
effect("impact",.20,lambda t:np.random.default_rng(4).normal(0,1,len(t))*.22*np.exp(-t*24))
effect("explosion",.85,lambda t:(np.random.default_rng(7).normal(0,1,len(t))*.28+np.sin(2*np.pi*(70*t-25*t*t))*.18)*np.exp(-t*5))
effect("pickup",.28,lambda t:(np.sin(2*np.pi*740*t)+np.sin(2*np.pi*1110*t))*.11*np.exp(-t*10))
effect("ui",.10,lambda t:np.sin(2*np.pi*640*t)*.13*np.exp(-t*38))
effect("dock",.8,lambda t:(np.sin(2*np.pi*220*t)+np.sin(2*np.pi*330*t)+np.sin(2*np.pi*440*t))*.07*np.sin(np.pi*t/.8))
effect("achievement",1.2,lambda t:sum(np.sin(2*np.pi*f*t)*np.exp(-((t-i*.13)*5)**2) for i,f in enumerate([440,554.37,659.25,880]))*.09)
effect("alarm",.5,lambda t:np.sin(2*np.pi*180*t)*.14*np.sin(np.pi*t/.5)**2)
print("Original asset generation complete.")
