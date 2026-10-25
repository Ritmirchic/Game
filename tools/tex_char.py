"""Character (skin, cloth, hair) and weapon textures."""
import numpy as np
from texlib import *
from tex_world import mixc


def _skin_pores(u, v, size, seed):
    pores = pnoise(u * 160, v * 160, 160, seed)
    blotch = fbm(u, v, 8, 4, seed + 1)
    return pores, blotch


def skin_zombie(u, v, size):
    pores, blotch = _skin_pores(u, v, size, 101)
    base = rgb_img(0.55 + 0.08 * blotch, 0.55 + 0.10 * blotch, 0.46 + 0.08 * blotch)
    # greenish decay sheen
    green = smoothstep(0.5, 0.8, fbm(u, v, 6, 4, 103))
    base = mixc(base, rgb_img(0.42, 0.48, 0.34), green * 0.6)
    # veins
    vein = np.zeros_like(u)
    for i in range(3):
        vein = np.maximum(vein, smoothstep(0.018, 0.0, np.abs(pnoise(u * 5 + i * 7, v * 7, 24, 105 + i) - 0.5)))
    base = mixc(base, rgb_img(0.34, 0.30, 0.38), np.clip(vein, 0, 1) * 0.45)
    # wounds / exposed flesh
    w = worley(u, v, 6, 107)
    wound = smoothstep(0.10, 0.03, w)
    base = mixc(base, rgb_img(0.42, 0.12, 0.12), wound * 0.7)
    base = mixc(base, rgb_img(0.22, 0.05, 0.06), smoothstep(0.06, 0.0, w) * 0.8)
    # dried blood streaks + grime
    streak = smoothstep(0.62, 0.88, fbm(u * 3, v * 1.2, 10, 4, 109))
    base = mixc(base, rgb_img(0.20, 0.07, 0.06), streak * 0.45)
    dirt = smoothstep(0.5, 0.85, fbm(u, v, 5, 4, 111)) * 0.5
    base = mixc(base, rgb_img(0.22, 0.20, 0.16), dirt)
    base = base * (0.9 + 0.2 * pores[..., None])
    height = pores * 0.2 + wound * 0.3
    rough = 0.55 + 0.3 * dirt - 0.15 * wound
    return dict(albedo=base, height=height, rough=rough)


def skin_pale(u, v, size):
    pores, blotch = _skin_pores(u, v, size, 121)
    base = rgb_img(0.66 + 0.06 * blotch, 0.62 + 0.06 * blotch, 0.60 + 0.06 * blotch)
    bruise = smoothstep(0.6, 0.8, fbm(u, v, 7, 4, 123))
    base = mixc(base, rgb_img(0.40, 0.34, 0.40), bruise * 0.5)
    base = base * (0.92 + 0.16 * pores[..., None])
    dirt = smoothstep(0.55, 0.85, fbm(u, v, 6, 4, 125)) * 0.4
    base = mixc(base, rgb_img(0.28, 0.26, 0.22), dirt)
    return dict(albedo=base, height=pores * 0.2, rough=0.6 + 0.2 * dirt)


def skin_brute(u, v, size):
    pores, blotch = _skin_pores(u, v, size, 131)
    base = rgb_img(0.40 + 0.10 * blotch, 0.36 + 0.09 * blotch, 0.28 + 0.08 * blotch)
    # thick scarred hide, armour plating ridges
    plate = 0.5 + 0.5 * np.sin(u * np.pi * 2 * 6) * np.sin(v * np.pi * 2 * 5)
    base = base * (0.75 + 0.45 * plate[..., None])
    scar = smoothstep(0.03, 0.0, np.abs(pnoise(u * 12, v * 14, 32, 133) - 0.5))
    base = mixc(base, rgb_img(0.30, 0.12, 0.10), scar * 0.8)
    w = worley(u, v, 4, 135)
    wound = smoothstep(0.10, 0.03, w)
    base = mixc(base, rgb_img(0.35, 0.08, 0.08), wound * 0.9)
    base = base * (0.9 + 0.2 * pores[..., None])
    return dict(albedo=base, height=plate * 0.5 + pores * 0.2, rough=0.7)


def flesh_wound(u, v, size):
    n = fbm(u, v, 20, 4, 141)
    base = rgb_img(0.40 + 0.15 * n, 0.06 + 0.05 * n, 0.07 + 0.05 * n)
    bone = smoothstep(0.72, 0.85, fbm(u, v, 12, 3, 143))
    base = mixc(base, rgb_img(0.78, 0.74, 0.62), bone * 0.8)
    return dict(albedo=base, height=n * 0.4, rough=0.45)


def cloth(base_rgb, weave_freq=90, dirt_amt=0.45, seed=151, blood=True):
    def fn(u, v, size):
        weave = (0.5 + 0.5 * np.sin(u * np.pi * 2 * weave_freq)) * (0.5 + 0.5 * np.sin(v * np.pi * 2 * weave_freq))
        n = fbm(u, v, 12, 4, seed)
        base = rgb_img(*base_rgb) * (0.82 + 0.3 * n[..., None])
        base = base * (0.92 + 0.16 * weave[..., None])
        # fold shading
        fold = fbm(u, v, 3, 4, seed + 3)
        base = base * (0.72 + 0.5 * fold[..., None])
        dirt = smoothstep(0.45, 0.8, fbm(u, v, 6, 4, seed + 5)) * dirt_amt
        base = mixc(base, rgb_img(0.20, 0.18, 0.15), dirt)
        if blood:
            sp = smoothstep(0.80, 0.90, fbm(u, v, 22, 4, seed + 7))
            base = mixc(base, rgb_img(0.26, 0.06, 0.05), sp * 0.55)
        else:
            sp = np.zeros_like(u)
        tear = smoothstep(0.86, 0.95, fbm(u, v, 16, 3, seed + 9)) * 0.5
        base = base * (1 - tear[..., None] * 0.6)
        height = weave * 0.4 + fold * 0.4 - tear * 0.3
        rough = 0.75 + 0.15 * n - 0.1 * dirt
        return dict(albedo=base, height=height, rough=rough)
    return fn


def hair_dark(u, v, size):
    strand = pnoise(u * 200, v * 12, 200, 161)
    n = fbm(u, v, 30, 4, 163)
    base = rgb_img(0.06 + 0.06 * n, 0.05 + 0.05 * n, 0.045 + 0.05 * n)
    base = base * (0.6 + 0.9 * strand[..., None])
    height = strand * 0.6
    return dict(albedo=base, height=height, rough=0.5)


def denim(u, v, size):
    return cloth((0.14, 0.20, 0.34), 70, 0.5, 171)(u, v, size)


def gun_metal(u, v, size):
    n = fbm(u, v, 40, 4, 181)
    base = rgb_img(0.11 + 0.05 * n, 0.115 + 0.05 * n, 0.125 + 0.05 * n)
    # machining lines
    mach = 0.5 + 0.5 * np.sin(v * np.pi * 2 * 160)
    base = base * (0.92 + 0.14 * mach[..., None])
    sc = scratches(u, v, size, 20, 183, 0.12, 0.6, strength=0.5)
    base = base + sc[..., None] * 0.25
    wear = smoothstep(0.55, 0.8, fbm(u, v, 6, 4, 185))
    base = mixc(base, rgb_img(0.34, 0.34, 0.33), wear * 0.7)
    return dict(albedo=base, height=mach * 0.15 + n * 0.1, rough=0.32 + 0.4 * wear)


def gun_polymer(u, v, size):
    n = fbm(u, v, 50, 3, 187)
    stipple = smoothstep(0.6, 0.75, pnoise(u * 120, v * 120, 120, 189))
    base = rgb_img(0.075 + 0.03 * n, 0.075 + 0.03 * n, 0.08 + 0.03 * n)
    base = base * (1 - stipple[..., None] * 0.4)
    sc = scratches(u, v, size, 12, 191, 0.1, 0.6, strength=0.5)
    base = base + sc[..., None] * 0.15
    return dict(albedo=base, height=stipple * 0.4 + n * 0.2, rough=0.5 + 0.2 * n)


def wood_stock(u, v, size):
    grain = fbm(u * 6, v * 0.8, 26, 4, 193)
    base = rgb_img(0.28 + 0.14 * grain, 0.14 + 0.08 * grain, 0.06 + 0.04 * grain)
    streak = pnoise(u * 2, v * 100, 100, 195)
    base = base * (0.85 + 0.3 * streak[..., None])
    varnish = smoothstep(0.4, 0.7, fbm(u, v, 4, 3, 197))
    base = base * (0.9 + 0.2 * varnish[..., None])
    sc = scratches(u, v, size, 14, 199, 0.15, 0.7, strength=0.5)
    base = base + sc[..., None] * 0.2
    return dict(albedo=base, height=grain * 0.3 + streak * 0.15, rough=0.35 + 0.3 * (1 - varnish))


def bat_wood(u, v, size):
    grain = fbm(u * 4, v * 1.2, 30, 4, 201)
    base = rgb_img(0.58 + 0.12 * grain, 0.42 + 0.10 * grain, 0.24 + 0.08 * grain)
    sc = scratches(u, v, size, 26, 203, 0.16, 0.7, strength=0.5)
    base = base + sc[..., None] * 0.2
    dirt = smoothstep(0.5, 0.85, fbm(u, v, 5, 4, 205)) * 0.5
    base = mixc(base, rgb_img(0.22, 0.16, 0.10), dirt)
    blood = smoothstep(0.74, 0.86, fbm(u, v, 14, 4, 207)) * 0.55
    base = mixc(base, rgb_img(0.20, 0.04, 0.04), blood)
    return dict(albedo=base, height=grain * 0.3 + sc * 0.2, rough=0.55 + 0.2 * dirt)


def axe_metal(u, v, size):
    n = fbm(u, v, 30, 4, 211)
    base = rgb_img(0.32 + 0.10 * n, 0.31 + 0.09 * n, 0.30 + 0.09 * n)
    # polished bevel band
    bevel = smoothstep(0.55, 0.75, v) * (1 - smoothstep(0.85, 1.0, v))
    base = mixc(base, rgb_img(0.62, 0.62, 0.64), bevel * 0.8)
    rust = smoothstep(0.5, 0.75, fbm(u, v, 8, 4, 213))
    base = mixc(base, rgb_img(0.34, 0.18, 0.08), rust * 0.8)
    blood = smoothstep(0.7, 0.82, fbm(u * 2, v * 3, 10, 4, 215)) * 0.7
    base = mixc(base, rgb_img(0.22, 0.03, 0.03), blood)
    sc = scratches(u, v, size, 18, 217, 0.12, 0.6, strength=0.5)
    base = base + sc[..., None] * 0.15
    return dict(albedo=base, height=n * 0.2 + bevel * 0.2, rough=0.3 + 0.5 * rust)


def steel_brushed(u, v, size):
    n = fbm(u, v, 60, 3, 221)
    brush = pnoise(u * 8, v * 300, 300, 223)
    base = rgb_img(0.44 + 0.10 * n, 0.45 + 0.10 * n, 0.47 + 0.10 * n)
    base = base * (0.9 + 0.2 * brush[..., None])
    dirt = smoothstep(0.55, 0.85, fbm(u, v, 5, 3, 225)) * 0.3
    base = mixc(base, rgb_img(0.22, 0.21, 0.19), dirt)
    return dict(albedo=base, height=brush * 0.2, rough=0.35 + 0.3 * dirt)


def crossbow_wood(u, v, size):
    grain = fbm(u * 5, v * 1.5, 28, 4, 231)
    base = rgb_img(0.24 + 0.10 * grain, 0.16 + 0.07 * grain, 0.10 + 0.05 * grain)
    sc = scratches(u, v, size, 16, 233, 0.14, 0.7, strength=0.5)
    base = base + sc[..., None] * 0.15
    return dict(albedo=base, height=grain * 0.3, rough=0.5)


def materials():
    return {
        "skin_zombie": (skin_zombie, 512, 1.0, 0.7, False),
        "skin_pale": (skin_pale, 256, 0.6, 0.5, False),
        "skin_brute": (skin_brute, 512, 1.2, 0.9, False),
        "flesh_wound": (flesh_wound, 256, 0.6, 0.7, False),
        "cloth_denim": (denim, 256, 0.7, 0.5, False),
        "cloth_shirt_blue": (cloth((0.16, 0.22, 0.34), 80, 0.4, 241), 256, 0.8, 0.5, False),
        "cloth_shirt_white": (cloth((0.62, 0.61, 0.57), 80, 0.55, 243), 256, 0.8, 0.5, False),
        "cloth_shirt_brown": (cloth((0.26, 0.20, 0.14), 80, 0.5, 245), 256, 0.8, 0.5, False),
        "cloth_hivis": (cloth((0.72, 0.52, 0.06), 70, 0.5, 247), 256, 0.8, 0.5, False),
        "cloth_leather": (cloth((0.14, 0.10, 0.08), 40, 0.35, 249), 256, 0.8, 0.5, False),
        "cloth_scrubs": (cloth((0.24, 0.36, 0.34), 80, 0.45, 251), 256, 0.8, 0.5, False),
        "hair_dark": (hair_dark, 256, 0.5, 0.6, False),
        "gun_metal": (gun_metal, 512, 0.5, 0.5, False),
        "gun_polymer": (gun_polymer, 256, 0.4, 0.5, False),
        "wood_stock": (wood_stock, 256, 0.5, 0.4, False),
        "bat_wood": (bat_wood, 256, 0.6, 0.4, False),
        "axe_metal": (axe_metal, 256, 0.4, 0.5, False),
        "steel_brushed": (steel_brushed, 256, 0.6, 0.4, False),
        "crossbow_wood": (crossbow_wood, 256, 0.5, 0.4, False),
    }
