"""SPACE MINER — procedural asteroid art.

Each asteroid design is a unique lumpy body: irregular silhouette, cratered
terrain, subsurface mineral glints and a rim light.  16 baked rotation frames
per design keep runtime rotation free of resampling cost.
"""
import math
import numpy as np
from PIL import Image
from artlib import (Canvas, value_noise, smoothstep, clamp01, normal_from_height,
                    light_shade, mix, hex2rgb)

SIZE = 176
FRAMES = 16

DESIGNS = [
    # name, base rock colour, secondary, speckle tint, metallic, crater density
    dict(name='carbon',  base='#4e5560', sec='#2b3038', tint='#7d8593', metal=0.35, craters=9,  lumps=1.00, glint='#8fd6ff'),
    dict(name='ferrous', base='#6d5347', sec='#382a24', tint='#a4795c', metal=0.55, craters=12, lumps=1.12, glint='#ffb066'),
    dict(name='basalt',  base='#3d434d', sec='#1d2127', tint='#5d6672', metal=0.25, craters=14, lumps=0.92, glint='#9fe8ff'),
    dict(name='icy',     base='#8ba3b8', sec='#4c6274', tint='#cfe6f7', metal=0.18, craters=6,  lumps=1.05, glint='#bfefff'),
    dict(name='metallic',base='#7b8390', sec='#333a44', tint='#c8d2de', metal=0.85, craters=7,  lumps=0.88, glint='#ffd98a'),
    dict(name='rust',    base='#7a5540', sec='#402a20', tint='#c1845a', metal=0.45, craters=13, lumps=1.16, glint='#ffcf8a'),
    dict(name='crystal', base='#546070', sec='#252c38', tint='#93a7bd', metal=0.30, craters=5,  lumps=0.96, glint='#7ef7ff'),
    dict(name='dense',   base='#454b55', sec='#22262d', tint='#6e7683', metal=0.70, craters=10, lumps=1.08, glint='#c79bff'),
]


def make_asteroid(design, seed, size=SIZE):
    d = DESIGNS[design]
    ss = 4
    S = size * ss
    cv = Canvas(size, size, ss)
    rng = np.random.default_rng(seed)

    ys, xs = np.mgrid[0:S, 0:S]
    cx, cy = S / 2.0, S / 2.0
    dx = (xs - cx) / (S / 2.0)
    dy = (ys - cy) / (S / 2.0)
    r = np.sqrt(dx * dx + dy * dy)
    th = np.arctan2(dy, dx)

    # ---- silhouette: harmonic lumpiness + noise modulation -----------------
    ph = rng.random(4) * math.tau
    harm = (1.0
            + 0.055 * np.sin(3 * th + ph[0])
            + 0.035 * np.sin(5 * th + ph[1])
            + 0.022 * np.sin(7 * th + ph[2])
            + 0.012 * np.sin(11 * th + ph[3]))
    n = value_noise(S, S, 5, seed + 11, octaves=2, gain=0.5)
    edge = (0.895 * harm * d['lumps']) + 0.10 * (n - 0.5)
    mask = smoothstep((edge - r) * 18.0)
    # one soft gouge in the rim (a bite, not a hole)
    ang = rng.random() * math.tau
    cd = 0.88 + rng.random() * 0.10
    gx, gy = math.cos(ang) * cd, math.sin(ang) * cd
    gr = 0.30 + rng.random() * 0.14
    dent = smoothstep((gr - np.sqrt((dx - gx) ** 2 + (dy - gy) ** 2)) * 6.0)
    mask = smoothstep(clamp01(mask - dent * 0.55) * 1.2)

    # ---- height field ------------------------------------------------------
    dome = np.sqrt(np.clip(1.0 - (r / 0.96) ** 2, 0, 1)) * 0.62
    bumps = value_noise(S, S, 8, seed + 3, octaves=5, gain=0.52) - 0.5
    fine = value_noise(S, S, 40, seed + 7, octaves=2, gain=0.5) - 0.5
    height = dome + bumps * 0.34 + fine * 0.06
    crater_core = np.zeros((S, S))
    crater_rim = np.zeros((S, S))
    ncr = d['craters']
    for _ in range(ncr):
        a = rng.random() * math.tau
        rr = (rng.random() ** 0.62) * 0.78
        px, py = math.cos(a) * rr, math.sin(a) * rr
        rad = 0.05 + rng.random() ** 2 * 0.15
        dist = np.sqrt((dx - px) ** 2 + (dy - py) ** 2)
        bowl = smoothstep((rad - dist) / (rad * 0.65))
        rim = np.exp(-((dist - rad * 1.02) ** 2) / (2 * (rad * 0.22) ** 2))
        height = height - bowl * (0.85 * rad / 0.12) + rim * 0.22
        crater_core = np.maximum(crater_core, bowl)
        crater_rim = np.maximum(crater_rim, rim)
    height = height * mask

    nx_, ny_, nz_ = normal_from_height(height * 7.0)
    lam = light_shade(nx_, ny_, nz_, -0.50, -0.64, 0.58, ambient=0.07)
    fill = light_shade(nx_, ny_, nz_, 0.66, 0.66, 0.36, ambient=0.0) * 0.22

    base = hex2rgb(d['base'])
    sec = hex2rgb(d['sec'])
    tint = hex2rgb(d['tint'])

    albedo = np.zeros((S, S, 3))
    tex = value_noise(S, S, 18, seed + 21, octaves=4, gain=0.55)
    tex2 = value_noise(S, S, 70, seed + 29, octaves=2, gain=0.5)
    for c in range(3):
        albedo[..., c] = base[c] * (0.45 + 0.62 * tex) + tint[c] * (0.20 * (tex2 - 0.48))
    albedo = albedo * 0.82 + sec * 0.22 * (1.0 - tex)[..., None]
    # lighting
    albedo *= (0.20 + 1.05 * lam)[..., None]
    albedo += np.array([0.07, 0.13, 0.24]) * fill[..., None]
    # craters: dark floors, bright rims
    albedo *= (1.0 - 0.45 * crater_core)[..., None]
    albedo += (crater_rim * clamp01(lam * 1.4))[..., None] * np.array([0.16, 0.15, 0.13])
    # crevice darkening
    crev = clamp01((value_noise(S, S, 12, seed + 5, 3) - 0.42) * 2.2)
    albedo *= (1.0 - 0.35 * crev)[..., None]

    # specular (metallic designs shine)
    spec_pow = 20.0 + 70.0 * d['metal']
    spec = clamp01((lam - 0.70) / 0.30) ** spec_pow * (0.35 + 1.7 * d['metal'])
    albedo += spec[..., None] * 0.5 * np.array([1.0, 0.98, 0.92])

    # rim light from the lower right with a cool tone
    grad = (nx_ * 0.55 + ny_ * 0.72 + 0.30) * 0.5 + 0.5
    rim = clamp01(grad) ** 6.0
    albedo += rim[..., None] * np.array([0.18, 0.30, 0.46]) * 0.7

    # ---- embedded mineral seams -------------------------------------------
    glint = hex2rgb(d['glint'])
    glow = np.zeros((S, S))
    vein_img = Image.new('L', (S, S), 0)
    from PIL import ImageDraw
    vd = ImageDraw.Draw(vein_img)
    for _ in range(int(2 + rng.random() * 3)):
        a = rng.random() * math.tau
        rr = (rng.random() ** 0.7) * 0.55
        x, y = math.cos(a) * rr, math.sin(a) * rr
        a2 = rng.random() * math.tau
        pts = [(S / 2 + x * S / 2, S / 2 + y * S / 2)]
        for step in range(int(5 + rng.random() * 6)):
            a2 += (rng.random() - 0.5) * 1.0
            nl = 0.05 + rng.random() * 0.06
            x, y = x + math.cos(a2) * nl, y + math.sin(a2) * nl
            pts.append((S / 2 + x * S / 2, S / 2 + y * S / 2))
        for i in range(len(pts) - 1):
            if rng.random() < 0.22:
                continue
            w = max(1, int((0.014 - i * 0.0012) * S))
            vd.line([pts[i], pts[i + 1]], fill=255, width=w)
    vein_mask = np.asarray(vein_img, dtype=np.float64) / 255.0
    vein_mask = vein_mask * smoothstep((0.93 - r) * 6.0)
    for _ in range(int(2 + rng.random() * 3)):
        a = rng.random() * math.tau
        rr = (rng.random() ** 0.8) * 0.6
        px, py = math.cos(a) * rr, math.sin(a) * rr
        rad = 0.022 + rng.random() * 0.035
        d2 = np.sqrt((dx - px) ** 2 + (dy - py) ** 2)
        core = smoothstep((rad - d2) / (rad * 0.8))
        halo = np.exp(-(d2 ** 2) / (2 * (rad * 3.0) ** 2))
        vein_mask = np.maximum(vein_mask, core)
        glow += halo * 0.7
    albedo += vein_mask[..., None] * glint * (0.20 + 0.42 * lam[..., None])

    # dust / speckle
    albedo *= (1.0 - 0.18 * (value_noise(S, S, 90, seed + 41, 1) - 0.5))[..., None]

    cv.rgb = clamp01(albedo)
    cv.a = clamp01(mask)
    cv.rgb += glow[..., None] * glint * 0.35 * mask[..., None]
    img = cv.downsample()

    # bake rotation frames
    frames = []
    for k in range(FRAMES):
        ang = -360.0 * k / FRAMES
        frames.append(img.rotate(ang, resample=Image.BICUBIC, expand=False))
    strip = Image.new('RGBA', (size * FRAMES, size), (0, 0, 0, 0))
    for k, f in enumerate(frames):
        strip.paste(f, (k * size, 0))
    return strip


def make_crack(level, size=SIZE):
    """Overlay of glowing fracture lines, drawn additively at runtime."""
    ss = 2
    S = size * ss
    cv = Canvas(size, size, ss)
    rng = np.random.default_rng(900 + level)
    img = Image.new('L', (S, S), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(img)
    branches = 3 + level * 3
    for b in range(branches):
        a = rng.random() * math.tau
        x, y = S / 2, S / 2
        x += math.cos(a) * S * 0.1
        y += math.sin(a) * S * 0.1
        steps = 8 + level * 4
        for s in range(steps):
            a += (rng.random() - 0.5) * 0.9
            nx = x + math.cos(a) * S * 0.055
            ny = y + math.sin(a) * S * 0.055
            w = max(1, int((3.0 - s * 0.12) * ss))
            d.line([(x, y), (nx, ny)], fill=255, width=w)
            x, y = nx, ny
    m = np.asarray(img, dtype=np.float64) / 255.0
    ys, xs = np.mgrid[0:S, 0:S]
    r = np.sqrt(((xs - S / 2) / (S / 2)) ** 2 + ((ys - S / 2) / (S / 2)) ** 2)
    m = m * smoothstep((0.95 - r) * 8)
    cv.rgb = np.ones((S, S, 3))
    cv.a = m
    return cv.downsample()


if __name__ == '__main__':
    import sys, os
    out = sys.argv[1] if len(sys.argv) > 1 else 'assets/gen'
    os.makedirs(out, exist_ok=True)
    for i, d in enumerate(DESIGNS):
        img = make_asteroid(i, 1000 + i * 37)
        img.save(f'{out}/ast_{i}.png')
        print('ast_%d' % i, img.size)
    for lv in range(3):
        make_crack(lv).save(f'{out}/ast_crack_{lv}.png')
    print('ok')
