"""SPACE MINER — planets.py

Five original background worlds, one per sector. Each is a properly lit sphere:
relief-shaded surface, clouds / storms, terminator falloff, atmospheric rim,
and in two cases a ring system. No plain circles: every world gets a unique
surface language (continents, banded gas giant, cratered metal moon, volcanic
world, shattered void sphere).
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from artlib import (Canvas, clamp01, smoothstep, lerp, hex2rgb, mix,
                    fbm_periodic, value_noise, radial)

TAU = math.tau


# --------------------------------------------------------------------------- #
#  surface texturing on the sphere
# --------------------------------------------------------------------------- #
def sphere_uv(nx, ny, nz):
    """Map a unit sphere point to (u, v) in [0,1] with u wrapping at the seam."""
    lon = np.arctan2(nx, nz)
    lat = np.arcsin(np.clip(ny, -1.0, 1.0))
    u = lon / TAU + 0.5
    v = 0.5 - lat / math.pi
    return u, v


def sample(tex, u, v):
    h, w = tex.shape
    x = np.clip((u * (w - 1)).astype(np.int32), 0, w - 1)
    y = np.clip((v * (h - 1)).astype(np.int32), 0, h - 1)
    return tex[y, x]


def tex_fbm(w, h, cells, seed, octaves=5):
    return fbm_periodic(w, h, max(2, int(round(cells))), seed, octaves=octaves, gain=0.5)


def col_ramp(t, stops):
    """t in [0,1]; stops = [(pos, (r,g,b)), ...] linear gradient."""
    out = np.zeros(t.shape + (3,))
    for i in range(len(stops) - 1):
        p0, c0 = stops[i]
        p1, c1 = stops[i + 1]
        m = (t >= p0) & (t <= p1)
        f = np.clip((t - p0) / max(1e-6, p1 - p0), 0, 1)
        f = smoothstep(f)
        for k in range(3):
            out[..., k] = np.where(m, c0[k] + (c1[k] - c0[k]) * f, out[..., k])
    return out


# --------------------------------------------------------------------------- #
#  generic planet renderer
# --------------------------------------------------------------------------- #
def render_world(size, seed, stops, *, clouds=None, craters=0, relief=1.0,
                 glow_rgb=(0.35, 0.55, 0.95), glow=0.55, ring=None,
                 bands=0.0, storms=0.0, cracks=0.0, crack_rgb=(1.0, 0.42, 0.10), nightside=0.05,
                 ice=0.0, crack_scale=5.0,
                 light=(-0.52, -0.42, 0.74), tex_cells=6.0, cloud_cells=3.0):
    """stops: colour ramp for the base surface (positions 0..1)."""
    ss = 2
    S = size * ss
    ys, xs = np.mgrid[0:S, 0:S]
    dx = (xs + 0.5 - S / 2) / (S / 2)
    dy = (ys + 0.5 - S / 2) / (S / 2)
    r = np.sqrt(dx * dx + dy * dy)
    inside = r < 1.0
    rr = np.clip(r, 0, 1)
    nz = np.sqrt(np.clip(1.0 - rr * rr, 0, 1))
    nx = dx.copy()
    ny = dy.copy()
    u, v = sphere_uv(nx, ny, nz)

    tw, th = 1024, 512
    base = tex_fbm(tw, th, tex_cells, seed)
    base = clamp01((base - 0.5) * 1.7 + 0.5)
    detail = tex_fbm(tw, th, tex_cells * 5.5, seed + 11, octaves=4)
    height = base

    if bands > 0.0:
        vv = (np.arange(th, dtype=np.float64) / (th - 1))[:, None] + np.zeros((1, tw))
        latn = tex_fbm(tw, th, 2.2, seed + 31, octaves=4)
        turb = tex_fbm(tw, th, 9.0, seed + 77, octaves=5) - 0.5
        stripes = 0.5 + 0.5 * np.sin((vv * math.tau * bands) + turb * 5.2 + latn * 2.0)
        base = clamp01(0.22 * base + 0.78 * stripes)
        height = base * 0.55 + detail * 0.45

    # bring the uv textures into screen space
    bs = sample(base, u, v)
    ds = sample(detail, u, v)
    hs = sample(height, u, v)

    # surface colour
    surf = col_ramp(bs, stops) * (0.86 + 0.30 * ds)[..., None]
    if storms > 0.0:
        sn = sample(tex_fbm(tw, th, 7.0, seed + 5), u, v)
        core = smoothstep(clamp01((sn - (0.72 - storms * 0.18)) * 5.0))
        speck = col_ramp(bs, [(0.0, (1.0, 0.86, 0.66)), (1.0, (1.0, 0.74, 0.48))])
        surf = surf * (1 - core[..., None] * 0.8) + speck * (core[..., None] * 0.8)

    # craters (rocky worlds): albedo + relief
    if craters:
        rng = np.random.default_rng(seed)
        cair = np.zeros((th, tw))
        cdep = np.zeros((th, tw))
        for _ in range(craters):
            cu = 0.12 + rng.random() * 0.76
            cv = 0.08 + rng.random() * 0.84
            rad = (0.006 + rng.random() ** 2.4 * 0.075)
            cx = cu * tw
            cy = cv * th
            rx = max(2.0, rad * tw)
            ry = max(2.0, rad * th / max(0.25, math.sin(cv * math.pi)))
            y0, y1 = int(max(0, cy - ry * 1.4)), int(min(th, cy + ry * 1.4))
            x0, x1 = int(max(0, cx - rx * 1.4)), int(min(tw, cx + rx * 1.4))
            if y1 <= y0 or x1 <= x0:
                continue
            cyy, cxx = np.mgrid[y0:y1, x0:x1]
            d = np.sqrt(((cxx - cx) / rx) ** 2 + ((cyy - cy) / ry) ** 2)
            bowl = clamp01(1.0 - d)
            rim = np.exp(-((d - 0.96) ** 2) / 0.006)
            floor = smoothstep(clamp01((1.0 - d) * 4.0)) * 0.0
            cdep[y0:y1, x0:x1] += rim * 0.55 - smoothstep(bowl) * 0.75 + floor
            cair[y0:y1, x0:x1] += rim * 0.42 - smoothstep(bowl) * 0.30
        hs = hs + sample(cdep, u, v) * 0.55 * relief
        surf = surf * np.clip(1.0 + 0.30 * sample(cair, u, v), 0.55, 1.30)[..., None]

    # fine grain keeps airless worlds from looking soft
    grain = sample(tex_fbm(tw, th, tex_cells * 14.0, seed + 1234, octaves=3), u, v)
    surf = surf * (0.86 + 0.30 * grain)[..., None]

    # clouds
    cloud = None
    if clouds is not None:
        cn = sample(tex_fbm(tw, th, cloud_cells, seed + 303, octaves=5), u, v)
        cn2 = sample(tex_fbm(tw, th, cloud_cells * 3.1, seed + 404, octaves=4), u, v)
        swirl = np.sin((v * 26.0) + cn * 9.0) * 0.5 + 0.5
        cloud = smoothstep(clamp01((cn * 0.72 + cn2 * 0.28 - 0.46) * 4.2))
        cloud = clamp01(cloud * (0.55 + 0.55 * swirl) * clouds)

    # relief-shaded normal
    gy, gx = np.gradient(hs)
    k = 3.2 * relief
    vnx = nx + (-gx * k)
    vny = ny + (-gy * k)
    vnz = nz.copy()
    ln = np.sqrt(vnx ** 2 + vny ** 2 + vnz ** 2) + 1e-6
    vnx, vny, vnz = vnx / ln, vny / ln, vnz / ln

    lx, ly, lz = light
    ll = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ll, ly / ll, lz / ll
    lam = clamp01(vnx * lx + vny * ly + vnz * lz)
    diff = 0.045 + 0.955 * lam ** 1.05
    # soft terminator: slight wrap-around light
    term = smoothstep(clamp01((lam + 0.10) * 3.4))
    diff = diff * (0.35 + 0.65 * term) + nightside * (1.0 - term)

    # specular on water/ice worlds
    hx, hy, hz = lx, ly, lz + 1.0
    hl = math.sqrt(hx * hx + hy * hy + hz * hz)
    spec = clamp01((vnx * hx + vny * hy + vnz * hz) / hl) ** 28.0
    spec_mask = 1.0

    if ice > 0.0:
        cap = smoothstep(clamp01((np.abs(ny) - (0.62 - ice * 0.18)) * 6.0))
        snow = np.array([0.90, 0.94, 0.99]) * (0.35 + 0.85 * diff)[..., None]
        surf = surf * (1 - (cap * 0.9)[..., None]) + snow * (cap * 0.9)[..., None]

    color = surf * diff[..., None]
    if cloud is not None:
        csurf = np.array([0.94, 0.96, 1.0]) * (0.55 + 0.55 * diff)[..., None]
        color = color * (1 - cloud[..., None] * 0.92) + csurf * (cloud[..., None] * 0.92)
    color = color + spec[..., None] * 0.0 * spec_mask

    # glowing fissures / tech lattice
    if cracks > 0.0:
        cn = sample(tex_fbm(tw, th, crack_scale, seed + 909, octaves=5), u, v)
        cn2 = sample(tex_fbm(tw, th, crack_scale * 2.7, seed + 919, octaves=4), u, v)
        ridge = 1.0 - np.abs(cn * 2.0 - 1.0)
        ridge2 = 1.0 - np.abs(cn2 * 2.0 - 1.0)
        hot = (smoothstep(clamp01((ridge - 0.90) * 12.0)) + 0.40 * smoothstep(clamp01((ridge2 - 0.925) * 14.0))) * cracks
        color = color + hot[..., None] * np.array(crack_rgb)[None, None, :] * (0.35 + 1.1 * (1 - lam))[..., None]

    # ---- assemble with atmosphere -------------------------------------- #
    # atmosphere rim just inside the limb
    fres = clamp01((rr - 0.86) / 0.14) ** 2.2 * (0.35 + 0.65 * clamp01(lam + 0.25))
    color = color + fres[..., None] * np.array(glow_rgb) * 0.85

    out_rgb = np.zeros((S, S, 3))
    out_a = np.zeros((S, S))

    # rings drawn behind
    if ring is not None:
        rb = ring_behind(ring, S, seed, far=True)
        out_rgb += rb[0]
        out_a = np.maximum(out_a, rb[1])

    m = inside.astype(np.float64)
    out_rgb = out_rgb * (1 - m[..., None]) + color * m[..., None]
    out_a = np.maximum(out_a, m)

    if ring is not None:
        rf = ring_behind(ring, S, seed, far=False)
        lit = 1.0 - 0.55 * np.clip((dy + 0.15) * 1.4, 0, 1)
        rf_rgb = rf[0] * lit[..., None]
        out_rgb = out_rgb * (1 - rf[1][..., None]) + rf_rgb
        out_a = np.maximum(out_a, rf[1])

    # outer atmospheric halo / glow
    halo = np.exp(-((rr - 1.0) ** 2) / 0.006) * (rr > 0.985) * glow
    halo = halo * (0.4 + 0.6 * clamp01(lam * 1.4 + 0.3))
    out_rgb = out_rgb + halo[..., None] * np.array(glow_rgb)
    out_a = clamp01(out_a + halo * 0.85)

    cv = Canvas(size, size, ss)
    cv.rgb = clamp01(out_rgb)
    cv.a = clamp01(out_a)
    return cv.downsample()


def ring_behind(ring, S, seed, far):
    """Screen-space ring annulus. far=True draws the part behind the planet."""
    inner, outer, tilt, colr, alpha = ring
    ys, xs = np.mgrid[0:S, 0:S]
    cx = cy = S / 2
    dx = (xs - cx) / (S / 2)
    dy = (ys - cy) / (S / 2)
    e = np.sqrt(dx * dx + (dy / tilt) ** 2)
    band = np.zeros_like(e)
    # several ringlets
    rng = np.random.default_rng(seed + 5)
    for i in range(9):
        c = inner + (outer - inner) * (i / 8.0) + rng.uniform(-0.02, 0.02)
        wdt = rng.uniform(0.012, 0.045)
        band += np.exp(-((e - c) ** 2) / (2 * wdt * wdt)) * rng.uniform(0.35, 1.0)
    band = clamp01(band) * (e > inner) * (e < outer + 0.02)
    # subtle noise break-up
    tex = value_noise(S, S, 3.0, seed + 3, octaves=3)
    band = band * (0.55 + 0.75 * tex)
    mask = np.clip(band * alpha, 0, 1)
    # only near side (front) or far side (back) of the planet
    if far:
        mask = mask * (dy < 0.0)
    else:
        mask = mask * (dy >= 0.0)
    rgb = np.array(colr)
    return rgb[None, None, :] * mask[..., None] * 1.15, mask


# --------------------------------------------------------------------------- #
#  the five worlds
# --------------------------------------------------------------------------- #
def planet_terra(size=560):
    """Sector 1 — STARTER BELT: temperate ocean world with green continents."""
    return render_world(
        size, 1001,
        [(0.00, hex2rgb('#04121f')), (0.24, hex2rgb('#0a3252')),
         (0.40, hex2rgb('#10618f')), (0.455, hex2rgb('#1d84a8')),
         (0.475, hex2rgb('#c8bd8a')), (0.52, hex2rgb('#4b7d3c')),
         (0.64, hex2rgb('#2f6234')), (0.78, hex2rgb('#6e6a45')),
         (0.90, hex2rgb('#9a8f74')), (1.00, hex2rgb('#efe9dc'))],
        clouds=0.62, relief=0.55, glow_rgb=(0.40, 0.68, 1.0), glow=0.66,
        tex_cells=7.0, cloud_cells=3.0, nightside=0.03, ice=0.6)


def planet_crystal(size=560):
    """Sector 2 — CRYSTAL NEBULA: violet gas giant with a bright ring."""
    return render_world(
        size, 2027,
        [(0.00, hex2rgb('#160a30')), (0.26, hex2rgb('#31165e')),
         (0.48, hex2rgb('#5f31a0')), (0.66, hex2rgb('#8d55c9')),
         (0.82, hex2rgb('#c193ea')), (1.00, hex2rgb('#f4e9ff'))],
        bands=11.0, storms=0.35, relief=0.18, glow_rgb=(0.72, 0.46, 1.0), glow=0.72,
        ring=(1.24, 1.9, 0.28, np.array([0.70, 0.60, 0.96]), 0.75),
        nightside=0.06, tex_cells=3.0)


def planet_iron(size=560):
    """Sector 3 — IRON GRAVEYARD: cratered metal moon, airless and harsh."""
    return render_world(
        size, 3301,
        [(0.00, hex2rgb('#241f1b')), (0.32, hex2rgb('#4a4038')),
         (0.56, hex2rgb('#6f6357')), (0.76, hex2rgb('#94867a')),
         (1.00, hex2rgb('#d8cbbb'))],
        craters=96, relief=1.05, glow_rgb=(0.55, 0.48, 0.42), glow=0.26,
        tex_cells=9.0, nightside=0.02)


def planet_plasma(size=560):
    """Sector 4 — PLASMA STORM: volcanic world with glowing fissures."""
    return render_world(
        size, 4409,
        [(0.00, hex2rgb('#1b0505')), (0.28, hex2rgb('#4d130a')),
         (0.52, hex2rgb('#832a11')), (0.72, hex2rgb('#b9551b')),
         (0.87, hex2rgb('#e28c34')), (1.00, hex2rgb('#ffdca6'))],
        clouds=0.4, relief=1.1, cracks=0.95,
        glow_rgb=(1.0, 0.45, 0.18), glow=0.75, tex_cells=6.0, cloud_cells=2.4,
        nightside=0.08)


def planet_void(size=560):
    """Sector 5 — VOID FRONTIER: dark shell laced with cyan energy veins."""
    return render_world(
        size, 5501,
        [(0.00, hex2rgb('#0a0d18')), (0.34, hex2rgb('#152033')),
         (0.56, hex2rgb('#20344c')), (0.76, hex2rgb('#2f5470')),
         (1.00, hex2rgb('#5a8ba2'))],
        craters=30, relief=1.0, glow_rgb=(0.28, 0.92, 1.0), glow=0.72,
        cracks=0.32, crack_rgb=(0.28, 0.88, 1.0), crack_scale=10.0,
        tex_cells=5.0, nightside=0.06, light=(-0.60, -0.38, 0.70))


PLANETS = [planet_terra, planet_crystal, planet_iron, planet_plasma, planet_void]


def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    for i, fn in enumerate(PLANETS):
        im = fn()
        path = os.path.join(outdir, f'planet_{i}.png')
        im.save(path)
        print(f'wrote {path} {im.size[0]}x{im.size[1]}')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '../assets/gen')
