"""SPACE MINER — sky.py

One full-screen, seamless deep-space backdrop per sector (1280x720).

These are painted as a single composition instead of a tiled texture, so there
is no visible repeat: a vertical gradient, a galactic dust band, layered gas
clouds with lit rims and dark lanes, distant star clusters of three brightness
classes, and sector-specific weather (ion streaks, lightning, drifting wreck
sparks, void motes).
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from artlib import (clamp01, smoothstep, hex2rgb, value_noise, fbm_periodic,
                    radial, Canvas)

W, H = 1280, 720


# --------------------------------------------------------------------------- #
#  helpers
# --------------------------------------------------------------------------- #
def fbm(w, h, cells, seed, octaves=5, gain=0.5):
    """Non-periodic fbm at a given resolution (square cells)."""
    return value_noise(w, h, float(cells), seed, octaves=octaves, gain=gain)


def blur(a, r):
    if r <= 0:
        return a
    im = Image.fromarray((clamp01(a) * 255).astype(np.uint8), 'L')
    im = im.filter(ImageFilter.GaussianBlur(r))
    return np.asarray(im, dtype=np.float64) / 255.0


def star_layer(w, h, count, seed, bright=1.0, size_max=2.0, hue_shift=0.0):
    """Returns (rgb_add, sparkle_mask) for a set of point stars."""
    rng = np.random.default_rng(seed)
    add = np.zeros((h, w, 3))
    ys, xs = np.mgrid[0:h, 0:w]
    pts = []
    for _ in range(count):
        x = rng.random() * w
        y = rng.random() * h
        # brighter stars concentrate toward the dust band in the middle third
        if rng.random() < 0.55:
            y = h * 0.5 + rng.normal() * h * 0.19
            y = min(max(y, 0.0), h - 1.0)
        mag = rng.random() ** 3.2          # most stars are faint
        size = 0.9 + mag * size_max * bright
        tint = rng.random()
        if tint < 0.18:
            col = (0.72, 0.84, 1.0)        # blue-white
        elif tint < 0.34:
            col = (1.0, 0.88, 0.72)        # warm
        elif tint < 0.42:
            col = (1.0, 0.78, 0.86)        # pink giant
        else:
            col = (1.0, 0.98, 0.94)        # white
        if hue_shift:
            col = (col[0], col[1] * (1 - hue_shift * 0.3), col[2])
        pts.append((x, y, size, mag, col))
    # draw in a few passes with local gaussians (cheap, exact control of falloff)
    for x, y, size, mag, col in pts:
        rad = size * 2.4
        x0, x1 = int(max(0, x - rad - 1)), int(min(w, x + rad + 2))
        y0, y1 = int(max(0, y - rad - 1)), int(min(h, y + rad + 2))
        if x1 <= x0 or y1 <= y0:
            continue
        sx = xs[y0:y1, x0:x1] - x
        sy = ys[y0:y1, x0:x1] - y
        d2 = (sx * sx + sy * sy) / (size * size + 1e-6)
        g = np.exp(-d2 * 0.9)
        core = np.exp(-d2 * 5.0)
        inten = (0.25 + 0.95 * mag) * bright
        for k in range(3):
            add[y0:y1, x0:x1, k] += (g * 0.55 + core * 0.9) * inten * col[k]
        if size > 2.1:
            # diffraction spikes on the brightest handful
            lw = max(1, int(size * 0.35))
            if 0 <= int(y) < h:
                add[int(y), x0:x1, :] += np.array(col)[None, :] * 0.16 * inten
            if 0 <= int(x) < w:
                add[y0:y1, int(x), :] += np.array(col)[None, :] * 0.16 * inten
            del lw
    return add


def gas_layer(seed, c_a, c_b, cover=1.0, detail=3.0, warps=1.0):
    """Return an RGB additive gas cloud (already soft, no hard edges)."""
    n1 = fbm(W, H, 2.1 * detail, seed, octaves=5, gain=0.52)
    n2 = fbm(W, H, 4.3 * detail, seed + 31, octaves=5, gain=0.5)
    n3 = fbm(W, H, 8.7 * detail, seed + 77, octaves=4, gain=0.52)
    # domain warp so the clouds curl instead of blobbing
    ys, xs = np.mgrid[0:H, 0:W]
    warp_x = (n2 - 0.5) * 90.0 * warps
    warp_y = (n3 - 0.5) * 70.0 * warps
    sx = np.clip(xs + warp_x, 0, W - 1).astype(np.int32)
    sy = np.clip(ys + warp_y, 0, H - 1).astype(np.int32)
    n1w = n1[sy, sx]
    # vertical envelope: gas tends to pool in a band across the middle
    env = np.exp(-((ys / H - 0.5) ** 2) / (2 * 0.26 ** 2))
    env2 = fbm(W, H, 1.3 * detail, seed + 5, octaves=3, gain=0.5)
    dens = clamp01((n1w * 0.68 + n2 * 0.32 - (0.66 - 0.18 * cover)) * 2.9)
    dens = smoothstep(dens) * (0.35 + 0.85 * env) * (0.45 + 0.85 * env2)
    dens = clamp01(dens * cover)

    # lit rims: gradient magnitude picks out the edges of the cloud
    gy, gx = np.gradient(dens)
    rim = clamp01(np.sqrt(gx * gx + gy * gy) * 22.0)
    core = clamp01((dens - 0.45) / 0.55) ** 2

    a = np.array(hex2rgb(c_a))
    b = np.array(hex2rgb(c_b))
    t = clamp01(n3 * 1.5 - 0.2)
    col = a[None, None, :] * (1 - t[..., None]) + b[None, None, :] * t[..., None]
    rgb = col * (dens * 0.62)[..., None]
    rgb = rgb + col * (rim * dens * 1.5)[..., None] * 0.55
    rgb = rgb + np.array([1.0, 0.98, 0.95])[None, None, :] * (core * dens * 0.32)[..., None]
    # dark lanes
    lane = smoothstep(clamp01((fbm(W, H, 3.4 * detail, seed + 211, octaves=4) - 0.56) * 3.2))
    rgb = rgb * (1.0 - lane[..., None] * 0.55)
    return rgb


def galaxy_band(seed, color, strength=1.0, tilt=0.28):
    ys, xs = np.mgrid[0:H, 0:W]
    u = xs / W - 0.5
    v = ys / H - 0.5
    d = v - (u * tilt + 0.05 * np.sin(u * 6.0 + seed))
    band = np.exp(-(d ** 2) / (2 * 0.075 ** 2))
    n = fbm(W, H, 6.0, seed, octaves=5, gain=0.55)
    n2 = fbm(W, H, 14.0, seed + 3, octaves=4)
    dust = smoothstep(clamp01((n2 - 0.52) * 3.6))
    glow = band * (0.35 + 0.9 * n) * (1.0 - dust * 0.75)
    rgb = np.array(hex2rgb(color))[None, None, :] * glow[..., None] * strength
    return rgb


def vignette(rgb, amount=0.55):
    ys, xs = np.mgrid[0:H, 0:W]
    d = np.sqrt(((xs / W - 0.5) * 1.9) ** 2 + ((ys / H - 0.5) * 1.9) ** 2)
    v = clamp01(1.0 - d * 0.55) ** 1.4
    return rgb * (1.0 - amount + amount * v)[..., None]


# --------------------------------------------------------------------------- #
#  sector backdrops
# --------------------------------------------------------------------------- #
def sky_belt():
    """Sector 1 — calm navy void, one warm dust lane, quiet star field."""
    ys = np.mgrid[0:H, 0:W][0]
    rgb = np.zeros((H, W, 3))
    top = np.array(hex2rgb('#070d18'))
    bot = np.array(hex2rgb('#0d1524'))
    f = (ys / H)[..., None]
    rgb += top[None, None, :] * (1 - f) + bot[None, None, :] * f
    rgb += galaxy_band(101, '#5d86b8', 0.55, tilt=0.30)
    rgb += gas_layer(103, '#2c4a74', '#8a6b3c', cover=0.75, detail=2.6)
    rgb += star_layer(W, H, 1500, 107, bright=1.0)
    rgb += star_layer(W, H, 90, 109, bright=2.0)
    return vignette(rgb, 0.5)


def sky_nebula():
    """Sector 2 — luminous magenta/violet nebula, thick and bright."""
    ys = np.mgrid[0:H, 0:W][0]
    rgb = np.zeros((H, W, 3))
    top = np.array(hex2rgb('#120822'))
    bot = np.array(hex2rgb('#1d0a2e'))
    f = (ys / H)[..., None]
    rgb += top[None, None, :] * (1 - f) + bot[None, None, :] * f
    rgb += gas_layer(201, '#8c2fb4', '#2b74c8', cover=1.25, detail=2.2)
    rgb += gas_layer(207, '#ff5fd0', '#7a3cff', cover=0.85, detail=4.0, warps=1.6)
    rgb += galaxy_band(209, '#c98bff', 0.45, tilt=-0.22)
    rgb += star_layer(W, H, 1800, 211, bright=0.9)
    rgb += star_layer(W, H, 120, 213, bright=2.1)
    return vignette(rgb, 0.52)


def sky_grave():
    """Sector 3 — cold teal murk over a debris field."""
    ys = np.mgrid[0:H, 0:W][0]
    rgb = np.zeros((H, W, 3))
    top = np.array(hex2rgb('#070f0e'))
    bot = np.array(hex2rgb('#0b1512'))
    f = (ys / H)[..., None]
    rgb += top[None, None, :] * (1 - f) + bot[None, None, :] * f
    rgb += gas_layer(301, '#2a5c48', '#7a6240', cover=0.9, detail=2.8)
    rgb += galaxy_band(303, '#79b79a', 0.30, tilt=0.4)
    rgb += star_layer(W, H, 1100, 307, bright=0.85)
    rgb += star_layer(W, H, 70, 309, bright=1.8)
    return vignette(rgb, 0.58)


def sky_storm():
    """Sector 4 — burning plasma hurricane."""
    ys = np.mgrid[0:H, 0:W][0]
    rgb = np.zeros((H, W, 3))
    top = np.array(hex2rgb('#160710'))
    bot = np.array(hex2rgb('#1d0a08'))
    f = (ys / H)[..., None]
    rgb += top[None, None, :] * (1 - f) + bot[None, None, :] * f
    rgb += gas_layer(401, '#8c2a12', '#5b1a86', cover=1.15, detail=2.4, warps=1.4)
    rgb += gas_layer(407, '#ff8a3c', '#c02a4a', cover=0.8, detail=4.6, warps=2.0)
    rgb += galaxy_band(409, '#ffb066', 0.35, tilt=0.16)
    rgb += star_layer(W, H, 900, 411, bright=0.7)
    rgb += star_layer(W, H, 60, 413, bright=1.6)
    return vignette(rgb, 0.5)


def sky_void():
    """Sector 5 — almost nothing, and what there is, is wrong."""
    ys = np.mgrid[0:H, 0:W][0]
    rgb = np.zeros((H, W, 3))
    top = np.array(hex2rgb('#04030a'))
    bot = np.array(hex2rgb('#070612'))
    f = (ys / H)[..., None]
    rgb += top[None, None, :] * (1 - f) + bot[None, None, :] * f
    rgb += gas_layer(501, '#2b1a5c', '#0b3a4e', cover=0.7, detail=2.2, warps=1.8)
    rgb += galaxy_band(503, '#8f7bff', 0.28, tilt=-0.35)
    rgb += star_layer(W, H, 800, 507, bright=0.75)
    rgb += star_layer(W, H, 45, 509, bright=2.2)
    return vignette(rgb, 0.6)


SKIES = [sky_belt, sky_nebula, sky_grave, sky_storm, sky_void]


def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    for i, fn in enumerate(SKIES):
        rgb = clamp01(fn())
        im = Image.fromarray((rgb * 255).astype(np.uint8), 'RGB')
        path = os.path.join(outdir, f'sky_{i}.png')
        im.save(path)
        print(f'wrote {path}')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '../assets/gen')
