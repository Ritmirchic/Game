"""World material generators: floors, walls, ceilings, wood, metal, decals."""
import numpy as np
from texlib import *

S = 512


def floor_tile(u, v, size):
    # 30cm vinyl tiles -> 4x4 per 1.2m repeat
    interior, e = tile_pattern(u, v, 6, 0.014)
    n = fbm(u, v, 24, 4, 3)
    tone = 0.55 + 0.14 * n
    # two-tone checker of off-white and light grey
    ix = np.floor(u * 6).astype(int)
    iy = np.floor(v * 6).astype(int)
    checker = ((ix + iy) % 2).astype(np.float32)
    base = rgb_img(tone * (0.90 + 0.05 * checker), tone * (0.88 + 0.05 * checker), tone * (0.80 + 0.04 * checker))
    # grime in grout
    grout = 1 - interior
    base = base * (1 - grout * 0.28)[..., None]
    # scuffs & dirt traffic
    dirt = fbm(u, v, 5, 5, 21)
    dirt = smoothstep(0.42, 0.72, dirt) * 0.45
    base = base * (1 - dirt)[..., None]
    sc = scratches(u, v, size, 18, 7, 0.10, 0.7, strength=0.5) * 0.35
    base = base + sc[..., None] * 0.18
    # subtle stains
    sp = stains(u, v, 33, 3, 4, 0.62, 0.16) * 0.35
    base = base * (1 - sp[..., None])
    height = interior * 0.6 + n * 0.25 - grout * 0.5
    rough = 0.32 + 0.2 * (1 - interior) + 0.25 * dirt + 0.15 * sp
    return dict(albedo=base, height=height, rough=rough)


def floor_linoleum(u, v, size):
    n = fbm(u, v, 8, 5, 5)
    n2 = fbm(u, v, 48, 3, 9)
    base = rgb_img(0.62 + 0.10 * n, 0.58 + 0.10 * n, 0.48 + 0.09 * n)
    # worn patches
    wear = smoothstep(0.5, 0.8, fbm(u, v, 4, 4, 12)) * 0.5
    base = mixc(base, rgb_img(0.45, 0.43, 0.38), wear)
    base = base * (1 - 0.25 * (n2 > 0.7)[..., None])
    sc = scratches(u, v, size, 40, 3, 0.12, 0.7, strength=0.5) * 0.4
    base = base + sc[..., None] * 0.22
    sp = stains(u, v, 44, 3, 5, 0.55, 0.2)
    base = base * (1 - sp[..., None] * 0.45)
    height = n * 0.4 + n2 * 0.05 - sp * 0.1
    rough = 0.30 + 0.35 * wear + 0.3 * sp
    return dict(albedo=base, height=height, rough=rough)


def floor_concrete(u, v, size):
    n = fbm(u, v, 16, 5, 2)
    coarse = fbm(u, v, 3, 3, 8)
    base = rgb_img(0.24 + 0.15 * n, 0.24 + 0.15 * n, 0.235 + 0.14 * n)
    base = base * (1 - 0.25 * smoothstep(0.55, 0.85, coarse)[..., None])
    # cracks (worley edges)
    w1 = worley(u, v, 5, 11)
    w2 = worley(u, v, 9, 17)
    crack = (1 - smoothstep(0.0, 0.045, w1)) * 0.7 + (1 - smoothstep(0.0, 0.03, w2)) * 0.4
    crack = np.clip(crack, 0, 1)
    base = base * (1 - 0.75 * crack[..., None])
    sp = stains(u, v, 55, 3, 5, 0.58, 0.2) * 0.45
    base = base * (1 - sp[..., None])
    oil = smoothstep(0.62, 0.8, fbm(u, v, 2, 3, 61)) * 0.4
    base = base * (1 - oil[..., None] * 0.6)
    height = n * 0.15 + 0.5 * (1 - 0.9 * crack) + pnoise(u * 96, v * 96, 96, 4) * 0.12
    rough = 0.68 + 0.2 * n - 0.45 * oil
    return dict(albedo=base, height=height, rough=rough)


def floor_carpet(u, v, size):
    n = fbm(u, v, 128, 3, 6)
    n2 = fbm(u, v, 12, 4, 15)
    base = rgb_img(0.16 + 0.06 * n2, 0.18 + 0.06 * n2, 0.24 + 0.07 * n2)
    base = base * (0.85 + 0.3 * n[..., None])
    sp = stains(u, v, 71, 3, 4, 0.6, 0.2) * 0.5
    base = base * (1 - sp[..., None])
    wear = smoothstep(0.45, 0.8, fbm(u, v, 5, 4, 88)) * 0.4
    base = mixc(base, rgb_img(0.3, 0.29, 0.27), wear)
    height = n * 0.8 + n2 * 0.2
    rough = 0.92 - 0.1 * n
    return dict(albedo=base, height=height, rough=rough)


def floor_asphalt(u, v, size):
    n = fbm(u, v, 16, 5, 4)
    grit = pnoise(u * 200, v * 200, 200, 23) * 0.5 + pnoise(u * 90, v * 90, 90, 31) * 0.5
    base = rgb_img(0.10 + 0.10 * n, 0.105 + 0.10 * n, 0.11 + 0.10 * n)
    base = base * (0.75 + 0.5 * grit[..., None])
    # cracks
    w1 = worley(u, v, 4, 21)
    crack = (1 - smoothstep(0.0, 0.035, w1)) * 0.85
    base = base * (1 - 0.7 * crack[..., None])
    puddle = puddle_pattern(u, v, 77)
    base = base * (1 - 0.35 * puddle[..., None])
    base = mixc(base, rgb_img(0.18, 0.19, 0.22), puddle * 0.5)
    height = grit * 0.5 + n * 0.3 - crack * 0.6
    rough = 0.86 - 0.6 * puddle + 0.1 * n
    return dict(albedo=base, height=height, rough=rough, wet=puddle)


def floor_gravel(u, v, size):
    w = 1 - worley(u, v, 26, 5)
    n = fbm(u, v, 40, 4, 12)
    base = rgb_img(0.20 + 0.16 * w, 0.19 + 0.15 * w, 0.17 + 0.13 * w)
    base = base * (0.8 + 0.4 * n[..., None])
    height = w * 0.8 + n * 0.3
    rough = 0.88 + 0.1 * n
    return dict(albedo=base, height=height, rough=rough)


def floor_dirt(u, v, size):
    n = fbm(u, v, 24, 5, 9)
    n2 = fbm(u, v, 6, 4, 19)
    base = rgb_img(0.15 + 0.12 * n, 0.13 + 0.11 * n, 0.09 + 0.09 * n)
    grass = smoothstep(0.58, 0.8, n2)
    base = mixc(base, rgb_img(0.10, 0.14, 0.06), grass * 0.7)
    height = n * 0.5 + grass * 0.4
    rough = 0.92
    return dict(albedo=base, height=height, rough=rough)


def wall_paint(u, v, size):
    n = fbm(u, v, 8, 5, 7)
    n2 = fbm(u, v, 40, 3, 13)
    base = rgb_img(0.52 + 0.08 * n, 0.515 + 0.08 * n, 0.48 + 0.075 * n)
    # peeling paint revealing plaster
    peel = smoothstep(0.6, 0.72, fbm(u, v, 6, 5, 41))
    base = mixc(base, rgb_img(0.38, 0.36, 0.32), peel * 0.8)
    # grime rising from the floor (bottom of the texture = v near 1)
    grime = smoothstep(0.72, 1.0, v) * 0.55
    grime = grime + smoothstep(0.6, 0.95, fbm(u, v, 12, 4, 55)) * 0.25
    base = base * (1 - grime[..., None] * 0.8)
    sp = stains(u, v, 66, 4, 4, 0.62, 0.15) * 0.35
    base = base * (1 - sp[..., None])
    sc = scratches(u, v, size, 12, 3, 0.14, 0.7, strength=0.4) * 0.3
    base = base + sc[..., None] * 0.15
    height = peel * 0.4 + n2 * 0.1
    rough = 0.62 + 0.2 * peel + 0.15 * grime
    return dict(albedo=base, height=height, rough=rough)


def wall_block(u, v, size):
    rows, cols = 8, 4
    m, row, fu = bricks(u, v, rows, cols, 0.012)
    n = fbm(u, v, 20, 5, 14)
    tone = 0.36 + 0.14 * n
    base = rgb_img(tone, tone * 0.99, tone * 0.94)
    # block-level variation
    idn = pnoise(np.floor(u * cols), np.floor(v * rows), 16, 3)
    base = base * (0.85 + 0.3 * idn[..., None])
    base = base * (1 - (1 - m)[..., None] * 0.45)
    grime = smoothstep(0.7, 1.0, v) * 0.4 + stains(u, v, 91, 3, 4, 0.6, 0.2) * 0.3
    base = base * (1 - grime[..., None])
    height = m * 0.4 + n * 0.3
    rough = 0.8 + 0.1 * n
    return dict(albedo=base, height=height, rough=rough)


def wall_brick(u, v, size):
    m, row, fu = bricks(u, v, 14, 4, 0.02)
    n = fbm(u, v, 30, 4, 16)
    base = rgb_img(0.30 + 0.10 * n, 0.14 + 0.06 * n, 0.10 + 0.05 * n)
    idn = pnoise(np.floor(u * 4), np.floor(v * 14), 32, 7)
    base = base * (0.8 + 0.4 * idn[..., None])
    mortar = rgb_img(0.34, 0.33, 0.30) * (0.9 + 0.2 * n[..., None])
    base = mixc(mortar, base, m)
    grime = stains(u, v, 12, 3, 5, 0.55, 0.2) * 0.35 + smoothstep(0.75, 1.0, v) * 0.3
    base = base * (1 - grime[..., None])
    height = m * 0.5 + n * 0.2
    rough = 0.85
    return dict(albedo=base, height=height, rough=rough)


def wall_tile(u, v, size):
    # white subway tile
    rows, cols = 10, 5
    m, row, fu = bricks(u, v, rows, cols, 0.014)
    n = fbm(u, v, 40, 4, 18)
    base = rgb_img(0.72 + 0.05 * n, 0.72 + 0.05 * n, 0.68 + 0.05 * n)
    grout = rgb_img(0.32, 0.31, 0.28)
    base = mixc(grout, base, m)
    grime = smoothstep(0.5, 1.0, v) * 0.45 + stains(u, v, 21, 4, 4, 0.6, 0.15) * 0.3
    base = base * (1 - grime[..., None])
    height = m * 0.5 + n * 0.1
    rough = 0.25 + 0.4 * (1 - m) + 0.2 * grime
    return dict(albedo=base, height=height, rough=rough)


def wall_metal(u, v, size):
    # painted steel cladding panels with rivets
    n = fbm(u, v, 12, 4, 22)
    base = rgb_img(0.30 + 0.06 * n, 0.33 + 0.06 * n, 0.35 + 0.06 * n)
    # panel seams
    seam = np.minimum((u * 2) % 1, 1 - (u * 2) % 1)
    seamV = np.minimum((v * 4) % 1, 1 - (v * 4) % 1)
    line = (1 - smoothstep(0.0, 0.006, seam)) + (1 - smoothstep(0.0, 0.008, seamV))
    # rivets near seams
    rx = np.abs(((u * 12) % 1) - 0.5)
    rivet = (1 - smoothstep(0.06, 0.12, rx)) * (1 - smoothstep(0.0, 0.02, np.abs(((v * 24) % 1) - 0.5)))
    sc = scratches(u, v, size, 22, 5, 0.16, 0.7, strength=0.5) * 0.35
    base = base + sc[..., None] * 0.2
    base = base * (1 - np.clip(line, 0, 1)[..., None] * 0.4)
    base = base + rivet[..., None] * 0.06
    rust = smoothstep(0.58, 0.75, fbm(u, v, 7, 5, 33))
    base = mixc(base, rgb_img(0.32, 0.17, 0.07), rust * 0.65)
    grime = smoothstep(0.7, 1.0, v) * 0.35
    base = base * (1 - grime[..., None])
    height = rivet * 0.6 + n * 0.15 - np.clip(line, 0, 1) * 0.5
    rough = 0.36 + 0.4 * rust + 0.1 * n + 0.15 * grime
    return dict(albedo=base, height=height, rough=rough)


def wall_metal_rust(u, v, size):
    n = fbm(u, v, 10, 5, 25)
    n2 = fbm(u, v, 45, 3, 27)
    base = rgb_img(0.34, 0.33, 0.31)
    rust = smoothstep(0.45, 0.72, n)
    base = mixc(base, rgb_img(0.36, 0.19, 0.08), rust)
    base = mixc(base, rgb_img(0.22, 0.11, 0.05), smoothstep(0.65, 0.9, n) * 0.7)
    base = base * (0.85 + 0.3 * n2[..., None])
    pit = 1 - worley(u, v, 30, 9)
    base = base * (1 - np.clip(pit - 0.62, 0, 1)[..., None] * 0.7)
    height = rust * 0.5 + n2 * 0.2 - np.clip(pit - 0.6, 0, 1) * 0.6
    rough = 0.5 + 0.4 * rust
    return dict(albedo=base, height=height, rough=rough)


def ceiling_tile(u, v, size):
    interior, e = tile_pattern(u, v, 2, 0.01)
    n = fbm(u, v, 64, 3, 29)
    base = rgb_img(0.50 + 0.06 * n, 0.50 + 0.06 * n, 0.48 + 0.05 * n)
    # acoustic holes
    holes = smoothstep(0.72, 0.85, pnoise(u * 128, v * 128, 128, 33))
    base = base * (1 - holes[..., None] * 0.35)
    base = base * (1 - (1 - interior)[..., None] * 0.5)
    # water stains
    sp = stains(u, v, 45, 2, 4, 0.6, 0.25)
    base = mixc(base, rgb_img(0.36, 0.31, 0.22), sp * 0.6)
    height = interior * 0.4 + n * 0.2 - holes * 0.2
    rough = 0.9
    return dict(albedo=base, height=height, rough=rough)


def ceiling_concrete(u, v, size):
    n = fbm(u, v, 20, 5, 31)
    base = rgb_img(0.34 + 0.12 * n, 0.34 + 0.12 * n, 0.33 + 0.11 * n)
    # formwork lines
    line = 1 - smoothstep(0, 0.01, np.minimum((v * 3) % 1, 1 - (v * 3) % 1))
    base = base * (1 - line[..., None] * 0.3)
    sp = stains(u, v, 12, 3, 4, 0.55, 0.2)
    base = base * (1 - sp[..., None] * 0.4)
    height = n * 0.4
    rough = 0.85
    return dict(albedo=base, height=height, rough=rough)


def wood_plank(u, v, size):
    rows, m, fu, fv = planks(u, v, rows=5, offset=0.37, gap=0.02)
    grain = fbm(u * 1.0, v * 6.0, 20, 4, 35)
    grain2 = pnoise(u * 60 + fbm(u, v, 6, 3, 36) * 6, v * 8, 60, 37)
    base = rgb_img(0.44 + 0.14 * grain, 0.32 + 0.12 * grain, 0.19 + 0.09 * grain)
    base = base * (0.85 + 0.3 * grain2[..., None])
    idn = pnoise(rows % 8, np.floor(fu * 3), 16, 41)
    base = base * (0.85 + 0.3 * idn[..., None])
    # knots
    knot = smoothstep(0.0, 0.06, worley(u * 2, v * 2, 3, 43))
    base = base * (0.7 + 0.3 * knot[..., None])
    base = base * (1 - (1 - m)[..., None] * 0.75)
    dirt = stains(u, v, 51, 4, 4, 0.6, 0.18) * 0.35
    base = base * (1 - dirt[..., None])
    height = grain * 0.4 + m * 0.4
    rough = 0.55 + 0.3 * grain + 0.2 * dirt
    return dict(albedo=base, height=height, rough=rough)


def plywood(u, v, size):
    grain = fbm(u * 3, v, 24, 4, 47)
    base = rgb_img(0.55 + 0.10 * grain, 0.43 + 0.09 * grain, 0.27 + 0.07 * grain)
    streak = pnoise(u * 4, v * 120, 120, 49)
    base = base * (0.9 + 0.2 * streak[..., None])
    edge = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))
    base = base * (1 - (1 - smoothstep(0.0, 0.02, edge))[..., None] * 0.4)
    dirt = stains(u, v, 61, 3, 4, 0.6, 0.2)
    base = base * (1 - dirt[..., None] * 0.4)
    height = grain * 0.3
    rough = 0.7
    return dict(albedo=base, height=height, rough=rough)


def metal_diamond(u, v, size):
    # diamond plate: two rotated grids of raised lozenges
    a = np.sin((u + v) * np.pi * 16)
    b = np.sin((u - v) * np.pi * 16)
    plate = np.clip(a * b, 0, 1) ** 0.6
    n = fbm(u, v, 16, 4, 53)
    base = rgb_img(0.38 + 0.08 * n, 0.39 + 0.08 * n, 0.40 + 0.08 * n)
    base = base * (0.8 + 0.4 * plate[..., None])
    rust = smoothstep(0.62, 0.8, fbm(u, v, 8, 4, 57)) * 0.55
    base = mixc(base, rgb_img(0.33, 0.18, 0.08), rust)
    sc = scratches(u, v, size, 20, 9, 0.1, 0.7, strength=0.5)
    base = base + sc[..., None] * 0.10
    height = plate * 0.8 + n * 0.1
    rough = 0.42 + 0.35 * rust + 0.1 * n
    return dict(albedo=base, height=height, rough=rough)


def glass_dirty(u, v, size):
    n = fbm(u, v, 10, 4, 59)
    smear = fbm(u, v, 3, 3, 63)
    base = rgb_img(0.30 + 0.1 * n, 0.34 + 0.1 * n, 0.36 + 0.1 * n)
    # streaks
    streak = smoothstep(0.3, 0.9, pnoise(u * 40, v * 3, 40, 65))
    base = base * (0.85 + 0.3 * streak[..., None])
    alpha = np.clip(0.22 + 0.4 * n + 0.3 * streak, 0, 0.8)
    height = n * 0.2
    rough = 0.2 + 0.3 * n
    return dict(albedo=base, height=height, rough=rough, alpha=alpha)


def chainlink(u, v, size):
    # wire mesh
    a = np.abs(((u * 18 + v * 18) % 1) - 0.5)
    b = np.abs(((u * 18 - v * 18) % 1) - 0.5)
    wire = np.clip(1 - smoothstep(0.02, 0.06, np.minimum(a, b)), 0, 1)
    n = fbm(u, v, 20, 3, 67)
    base = rgb_img(0.45 + 0.1 * n, 0.45 + 0.1 * n, 0.44 + 0.1 * n)
    alpha = wire
    height = wire
    rough = 0.5
    return dict(albedo=base, height=height, rough=rough, alpha=alpha)


def cardboard(u, v, size):
    n = fbm(u, v, 30, 4, 71)
    flute = 0.5 + 0.5 * np.sin(v * np.pi * 2 * 90)
    base = rgb_img(0.42 + 0.08 * n, 0.32 + 0.07 * n, 0.21 + 0.05 * n)
    base = base * (0.92 + 0.16 * flute[..., None])
    tape = (np.abs(u - 0.5) < 0.06).astype(np.float32)
    base = mixc(base, rgb_img(0.55, 0.52, 0.44), tape * 0.8)
    sp = stains(u, v, 73, 4, 3, 0.62, 0.2)
    base = base * (1 - sp[..., None] * 0.5)
    height = flute * 0.2 + n * 0.2
    rough = 0.88
    return dict(albedo=base, height=height, rough=rough)


def fabric_tarp(u, v, size):
    weave = (0.5 + 0.5 * np.sin(u * np.pi * 2 * 120)) * (0.5 + 0.5 * np.sin(v * np.pi * 2 * 120))
    n = fbm(u, v, 8, 4, 75)
    base = rgb_img(0.13 + 0.05 * n, 0.20 + 0.06 * n, 0.15 + 0.05 * n)
    base = base * (0.9 + 0.2 * weave[..., None])
    dirt = stains(u, v, 77, 3, 4, 0.55, 0.2) * 0.4
    base = base * (1 - dirt[..., None])
    height = weave * 0.3
    rough = 0.7
    return dict(albedo=base, height=height, rough=rough)


def rubber(u, v, size):
    n = fbm(u, v, 40, 3, 79)
    base = rgb_img(0.055 + 0.03 * n, 0.055 + 0.03 * n, 0.06 + 0.03 * n)
    height = n * 0.3
    rough = 0.85
    return dict(albedo=base, height=height, rough=rough)


def tarmac_line(u, v, size):
    return None


# ---------------------------------------------------------------- decals
def decal_blood_pool(u, v, size):
    cx, cy = 0.5, 0.5
    r = np.hypot(u - cx, v - cy) * 2
    edge = fbm(u, v, 6, 4, 83)
    a = 1 - smoothstep(0.32 + edge * 0.35, 0.45 + edge * 0.35, r)
    drip = fbm(u, v, 12, 3, 87)
    a = np.clip(a * (0.75 + 0.35 * drip), 0, 1)
    a = smoothstep(0.15, 0.6, a)
    tone = 0.25 + 0.15 * fbm(u, v, 20, 3, 89)
    base = rgb_img(0.22 * tone, 0.02 * tone, 0.02 * tone)
    base = base * (0.6 + 0.8 * smoothstep(0.2, 0.9, fbm(u, v, 8, 4, 91))[..., None])
    rough = 0.25 + 0.3 * fbm(u, v, 30, 2, 93)
    return dict(albedo=base, height=fbm(u, v, 30, 2, 95) * 0.1, rough=rough, alpha=a)


def decal_blood_splat(u, v, size):
    r = np.hypot(u - 0.5, v - 0.5) * 2
    n = fbm(u, v, 8, 5, 97)
    a = 1 - smoothstep(0.15, 0.85, r + n * 0.6)
    drops = smoothstep(0.55, 0.8, fbm(u, v, 22, 3, 99)) * (1 - smoothstep(0.4, 1.0, r))
    a = np.clip(a + drops * 0.8, 0, 1)
    base = rgb_img(0.30, 0.02, 0.02) * (0.6 + 0.8 * n[..., None])
    return dict(albedo=base, height=np.zeros_like(u), rough=0.3, alpha=a)


def decal_bullet(u, v, size):
    r = np.hypot(u - 0.5, v - 0.5) * 2
    n = fbm(u, v, 24, 3, 101)
    hole = 1 - smoothstep(0.07, 0.13, r + n * 0.04)
    ring = (1 - smoothstep(0.13, 0.30, r + n * 0.10)) - (1 - smoothstep(0.07, 0.13, r))
    soot = (1 - smoothstep(0.18, 0.85, r + n * 0.25)) * 0.5
    base = rgb_img(0.02, 0.02, 0.02)
    base = base + rgb_img(0.55, 0.53, 0.48) * np.clip(ring, 0, 1)[..., None]
    base = base + rgb_img(0.30, 0.28, 0.25) * soot[..., None]
    alpha = np.clip(hole + np.clip(ring, 0, 1) * 0.9 + soot, 0, 1)
    return dict(albedo=base, height=np.zeros_like(u), rough=0.85, alpha=alpha)


def decal_scorch(u, v, size):
    r = np.hypot(u - 0.5, v - 0.5) * 2
    n = fbm(u, v, 6, 4, 103)
    a = (1 - smoothstep(0.1, 1.0, r + n * 0.4)) * 0.85
    base = rgb_img(0.04, 0.035, 0.03)
    return dict(albedo=base, height=np.zeros_like(u), rough=0.9, alpha=a)


def decal_graffiti(u, v, size):
    n = fbm(u, v, 5, 4, 105)
    scrib = np.zeros_like(u)
    for i in range(7):
        p = pnoise(u * 5 + i * 3.1, v * 3 + i, 16, 107 + i)
        scrib = np.maximum(scrib, smoothstep(0.4, 0.45, p) * (1 - smoothstep(0.55, 0.62, p)))
    a = scrib * (0.35 + 0.4 * n) * 0.75
    base = rgb_img(0.55, 0.10, 0.30)
    return dict(albedo=base, height=np.zeros_like(u), rough=0.7, alpha=a)


def mixc(a, b, t):
    t = np.asarray(t)
    if t.ndim == 2:
        t = t[..., None]
    return a * (1 - t) + b * t


def materials():
    return {
        # name: (fn, size, uv repeat metres, normal strength, is_alpha)
        "floor_tile": (floor_tile, 512, 2.0, 0.6, False),
        "floor_linoleum": (floor_linoleum, 512, 2.0, 0.4, False),
        "floor_concrete": (floor_concrete, 512, 3.0, 0.7, False),
        "floor_carpet": (floor_carpet, 256, 1.5, 0.8, False),
        "floor_asphalt": (floor_asphalt, 512, 4.0, 0.8, False),
        "floor_gravel": (floor_gravel, 256, 2.0, 1.0, False),
        "floor_dirt": (floor_dirt, 512, 4.0, 0.8, False),
        "wall_paint": (wall_paint, 512, 2.5, 0.5, False),
        "wall_block": (wall_block, 512, 2.5, 0.8, False),
        "wall_brick": (wall_brick, 512, 2.5, 0.9, False),
        "wall_tile": (wall_tile, 512, 1.6, 0.5, False),
        "wall_metal": (wall_metal, 512, 2.5, 0.7, False),
        "wall_metal_rust": (wall_metal_rust, 512, 2.5, 0.9, False),
        "ceiling_tile": (ceiling_tile, 512, 2.4, 0.4, False),
        "ceiling_concrete": (ceiling_concrete, 512, 3.0, 0.4, False),
        "wood_plank": (wood_plank, 512, 1.5, 0.7, False),
        "plywood": (plywood, 256, 1.2, 0.4, False),
        "metal_diamond": (metal_diamond, 512, 1.5, 1.0, False),
        "glass_dirty": (glass_dirty, 256, 1.5, 0.2, True),
        "chainlink": (chainlink, 256, 1.2, 0.6, True),
        "cardboard": (cardboard, 256, 1.0, 0.3, False),
        "fabric_tarp": (fabric_tarp, 256, 1.5, 0.4, False),
        "rubber": (rubber, 128, 0.8, 0.4, False),
        "decal_blood_pool": (decal_blood_pool, 256, 1.0, 0.1, True),
        "decal_blood_splat": (decal_blood_splat, 256, 1.0, 0.1, True),
        "decal_bullet": (decal_bullet, 128, 0.3, 0.1, True),
        "decal_scorch": (decal_scorch, 128, 0.6, 0.1, True),
        "decal_graffiti": (decal_graffiti, 256, 1.5, 0.1, True),
    }
