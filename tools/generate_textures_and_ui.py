#!/usr/bin/env python3
"""
Procedural PBR Texture, Artwork, Custom UI Icon, Boot Splash & Windows ICO Generator
for DON'T LOOK BACK (Godot 4.2 Psychological Horror).
Zero placeholders, zero emojis — seamless tileable albedo/normal/roughness maps.
"""
import os
import math
import numpy as np
from scipy.ndimage import gaussian_filter, sobel
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TEX_DIR = os.path.join(ROOT, "assets", "textures")
ICON_DIR = os.path.join(ROOT, "assets", "icons")
UI_DIR = os.path.join(ROOT, "assets", "ui")

os.makedirs(TEX_DIR, exist_ok=True)
os.makedirs(ICON_DIR, exist_ok=True)
os.makedirs(UI_DIR, exist_ok=True)


def tileable_fbm(h, w, octaves=5, base_freq=4, seed=42):
    """Generates seamless periodic 2D fractional Brownian motion in [0, 1]."""
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w), dtype=np.float32)
    amp = 1.0
    total_amp = 0.0
    y_idx = np.arange(h, dtype=np.float32)[:, None] / h
    x_idx = np.arange(w, dtype=np.float32)[None, :] / w

    for o in range(octaves):
        freq = base_freq * (2 ** o)
        if freq >= min(h, w) // 2:
            # High-frequency wrapped Gaussian noise
            raw = rng.standard_normal((h, w)).astype(np.float32)
            sigma = max(0.6, min(h, w) / float(freq * 2.5))
            layer = gaussian_filter(raw, sigma=sigma, mode="wrap")
        else:
            grid = rng.standard_normal((freq, freq)).astype(np.float32)
            # Upsample smoothly via FFT/bicubic periodic interpolation
            gy = (y_idx * freq)
            gx = (x_idx * freq)
            iy0 = np.floor(gy).astype(int) % freq
            ix0 = np.floor(gx).astype(int) % freq
            iy1 = (iy0 + 1) % freq
            ix1 = (ix0 + 1) % freq
            fy = gy - np.floor(gy)
            fx = gx - np.floor(gx)
            # Quintic smoothstep
            uy = fy * fy * fy * (fy * (fy * 6 - 15) + 10)
            ux = fx * fx * fx * (fx * (fx * 6 - 15) + 10)
            c00 = grid[iy0, ix0]
            c10 = grid[iy0, ix1]
            c01 = grid[iy1, ix0]
            c11 = grid[iy1, ix1]
            layer = (c00 * (1 - ux) + c10 * ux) * (1 - uy) + (c01 * (1 - ux) + c11 * ux) * uy
        layer = (layer - layer.min()) / (layer.max() - layer.min() + 1e-6)
        acc += layer * amp
        total_amp += amp
        amp *= 0.5
    acc /= total_amp
    return np.clip((acc - acc.min()) / (acc.max() - acc.min() + 1e-6), 0.0, 1.0)


def height_to_normal(height_map, strength=3.0):
    """Converts a [0,1] seamless heightmap into an OpenGL tangent-space RGB normal map."""
    dx = sobel(height_map, axis=1, mode="wrap") * strength
    dy = sobel(height_map, axis=0, mode="wrap") * strength
    nx = -dx
    ny = -dy
    nz = np.ones_like(height_map)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz) + 1e-6
    rgb = np.stack([
        (nx / norm * 0.5 + 0.5) * 255.0,
        (ny / norm * 0.5 + 0.5) * 255.0,
        (nz / norm * 0.5 + 0.5) * 255.0,
    ], axis=-1)
    return np.clip(rgb, 0, 255).astype(np.uint8)


def save_pbr_set(name, albedo_rgb, height_map, roughness_map, normal_strength=2.8):
    albedo_u8 = np.clip(albedo_rgb * 255.0, 0, 255).astype(np.uint8)
    nrm_u8 = height_to_normal(height_map.astype(np.float32), strength=normal_strength)
    rgh_u8 = np.clip(roughness_map * 255.0, 0, 255).astype(np.uint8)
    Image.fromarray(albedo_u8, mode="RGB").save(os.path.join(TEX_DIR, f"{name}_alb.png"), optimize=True)
    Image.fromarray(nrm_u8, mode="RGB").save(os.path.join(TEX_DIR, f"{name}_nrm.png"), optimize=True)
    Image.fromarray(rgh_u8, mode="L").save(os.path.join(TEX_DIR, f"{name}_rgh.png"), optimize=True)


def build_pbr_materials():
    S = 512
    y = np.linspace(0, 1, S, endpoint=False)[:, None]
    x = np.linspace(0, 1, S, endpoint=False)[None, :]

    # 1. Wall Wallpaper Beige (1970s faded apartment corridor stripes + subtle damp stains)
    n1 = tileable_fbm(S, S, octaves=5, base_freq=6, seed=101)
    n2 = tileable_fbm(S, S, octaves=4, base_freq=3, seed=102)
    stripes = 0.5 + 0.5 * np.sin(x * 2 * math.pi * 16.0)
    pinstripe = (np.abs(np.sin(x * 2 * math.pi * 32.0)) > 0.92).astype(np.float32)
    damp = np.clip((1.0 - y) ** 2.4 * 0.45 + (n2 - 0.55) * 0.65, 0, 1)
    base = np.zeros((S, S, 3), dtype=np.float32)
    base[..., 0] = 0.66 - 0.07 * stripes - 0.05 * pinstripe + (n1 - 0.5) * 0.08
    base[..., 1] = 0.62 - 0.06 * stripes - 0.04 * pinstripe + (n1 - 0.5) * 0.08
    base[..., 2] = 0.51 - 0.06 * stripes - 0.03 * pinstripe + (n1 - 0.5) * 0.07
    # Grime darkening
    base *= (1.0 - 0.36 * damp[..., None])
    h_map = 0.45 * n1 + 0.35 * stripes + 0.2 * pinstripe
    r_map = np.clip(0.76 + 0.14 * n1 - 0.12 * damp, 0.35, 0.98)
    save_pbr_set("wall_wallpaper_beige", base, h_map, r_map, 2.2)

    # 2. Wall Wallpaper Anomaly (Peeling crimson-ochre rot damask with organic dark veins)
    na = tileable_fbm(S, S, octaves=6, base_freq=5, seed=205)
    nb = tileable_fbm(S, S, octaves=5, base_freq=8, seed=206)
    damask = np.sin(x * 2 * math.pi * 8) * np.sin(y * 2 * math.pi * 8)
    veins = np.exp(-((nb - 0.5) ** 2) / 0.0022)
    peel = (na > 0.72).astype(np.float32)
    alb = np.zeros((S, S, 3), dtype=np.float32)
    alb[..., 0] = 0.42 + 0.12 * damask - 0.25 * veins - 0.15 * peel + 0.08 * na
    alb[..., 1] = 0.20 + 0.06 * damask - 0.14 * veins + 0.06 * peel + 0.05 * na
    alb[..., 2] = 0.16 + 0.04 * damask - 0.10 * veins + 0.06 * peel + 0.04 * na
    save_pbr_set("wall_wallpaper_anomaly", np.clip(alb, 0.03, 0.85), 0.5 * na + 0.4 * peel - 0.3 * veins, np.clip(0.65 - 0.28 * veins + 0.2 * peel, 0.2, 0.95), 4.0)

    # 3. Wall Plaster Aged
    np1 = tileable_fbm(S, S, octaves=6, base_freq=4, seed=301)
    np2 = tileable_fbm(S, S, octaves=5, base_freq=12, seed=302)
    cracks = np.exp(-((np2 - 0.50) ** 2) / 0.0005) * (np1 > 0.52)
    alb = np.stack([
        0.70 + (np1 - 0.5) * 0.11 - 0.28 * cracks,
        0.69 + (np1 - 0.5) * 0.10 - 0.28 * cracks,
        0.64 + (np1 - 0.5) * 0.10 - 0.26 * cracks,
    ], axis=-1)
    save_pbr_set("wall_plaster_aged", np.clip(alb, 0.1, 0.9), np1 * 0.7 - cracks * 0.6, np.clip(0.82 + 0.1 * np1, 0.5, 0.98), 2.5)

    # 4. Brutalist Concrete (Stairwell / Basement)
    nc1 = tileable_fbm(S, S, octaves=6, base_freq=6, seed=401)
    plank_seams = (np.mod(y * 8.0, 1.0) < 0.025).astype(np.float32)
    v_seams = (np.mod(x * 4.0, 1.0) < 0.018).astype(np.float32)
    alb = np.stack([
        0.44 + (nc1 - 0.5) * 0.16 - 0.14 * plank_seams - 0.10 * v_seams,
        0.45 + (nc1 - 0.5) * 0.16 - 0.14 * plank_seams - 0.10 * v_seams,
        0.46 + (nc1 - 0.5) * 0.15 - 0.13 * plank_seams - 0.10 * v_seams,
    ], axis=-1)
    save_pbr_set("wall_concrete_brutalist", np.clip(alb, 0.08, 0.8), nc1 * 0.75 - 0.4 * (plank_seams + v_seams), np.clip(0.85 + 0.1 * nc1, 0.6, 0.99), 3.4)

    # 5. Floor Wood Parquet / Oak Planks
    nw = tileable_fbm(S, S, octaves=5, base_freq=4, seed=501)
    grain = 0.5 + 0.5 * np.sin((y * 28.0 + nw * 3.5) * 2 * math.pi)
    plank_id = np.floor(x * 8.0)
    plank_var = (np.sin(plank_id * 12.9898) * 0.5 + 0.5) * 0.12
    seam_x = (np.mod(x * 8.0, 1.0) < 0.03).astype(np.float32)
    seam_y = (np.mod(y * 2.0 + plank_id * 0.37, 1.0) < 0.016).astype(np.float32)
    seams = np.clip(seam_x + seam_y, 0, 1)
    alb = np.stack([
        0.42 + plank_var - 0.09 * grain + (nw - 0.5) * 0.06,
        0.27 + plank_var * 0.7 - 0.07 * grain + (nw - 0.5) * 0.05,
        0.15 + plank_var * 0.4 - 0.04 * grain + (nw - 0.5) * 0.03,
    ], axis=-1) * (1.0 - 0.65 * seams[..., None])
    save_pbr_set("floor_wood_parquet", np.clip(alb, 0.03, 0.8), 0.35 * grain + 0.25 * nw - 0.8 * seams, np.clip(0.38 + 0.22 * nw + 0.4 * seams, 0.2, 0.95), 2.8)

    # 6. Corridor Carpet (Worn burgundy/ochre geometric hotel runner)
    ncp = tileable_fbm(S, S, octaves=6, base_freq=16, seed=601)
    wear = tileable_fbm(S, S, octaves=4, base_freq=3, seed=602)
    gx = np.abs(np.sin(x * 2 * math.pi * 6))
    gy = np.abs(np.sin(y * 2 * math.pi * 6))
    diamond = ((gx + gy) > 1.15) & ((gx + gy) < 1.38)
    border = (x < 0.09) | (x > 0.91)
    pat = diamond.astype(np.float32)
    bdr = border.astype(np.float32)
    alb = np.zeros((S, S, 3), dtype=np.float32)
    alb[..., 0] = 0.36 + 0.18 * pat + 0.14 * bdr + (ncp - 0.5) * 0.08
    alb[..., 1] = 0.11 + 0.14 * pat + 0.11 * bdr + (ncp - 0.5) * 0.05
    alb[..., 2] = 0.12 + 0.05 * pat + 0.04 * bdr + (ncp - 0.5) * 0.05
    alb *= (0.72 + 0.35 * wear[..., None])
    save_pbr_set("floor_carpet_corridor", np.clip(alb, 0.04, 0.8), ncp * 0.8 + 0.2 * pat, np.full((S, S), 0.90, dtype=np.float32), 2.4)

    # 7. Linoleum Tile (Laundry / Kitchen / Maintenance)
    nt = tileable_fbm(S, S, octaves=5, base_freq=8, seed=701)
    tx = np.floor(x * 8.0).astype(int)
    ty = np.floor(y * 8.0).astype(int)
    checker = ((tx + ty) % 2).astype(np.float32)
    grout = ((np.mod(x * 8.0, 1.0) < 0.035) | (np.mod(y * 8.0, 1.0) < 0.035)).astype(np.float32)
    alb = np.stack([
        0.58 - 0.18 * checker + (nt - 0.5) * 0.09,
        0.60 - 0.16 * checker + (nt - 0.5) * 0.09,
        0.56 - 0.14 * checker + (nt - 0.5) * 0.08,
    ], axis=-1) * (1.0 - 0.55 * grout[..., None])
    save_pbr_set("floor_tile_linoleum", np.clip(alb, 0.06, 0.88), 0.3 * nt - 0.85 * grout, np.clip(0.32 + 0.22 * nt + 0.5 * grout, 0.18, 0.95), 3.0)

    # 8. Acoustic Ceiling Tile
    nce = tileable_fbm(S, S, octaves=6, base_freq=14, seed=801)
    stain = tileable_fbm(S, S, octaves=4, base_freq=3, seed=802)
    grid = ((np.mod(x * 4.0, 1.0) < 0.025) | (np.mod(y * 4.0, 1.0) < 0.025)).astype(np.float32)
    speck = (nce < 0.24).astype(np.float32)
    w_stain = np.clip((stain - 0.68) * 2.2, 0, 1)
    alb = np.stack([
        0.72 - 0.15 * speck - 0.12 * w_stain - 0.22 * grid,
        0.71 - 0.15 * speck - 0.16 * w_stain - 0.22 * grid,
        0.67 - 0.14 * speck - 0.24 * w_stain - 0.20 * grid,
    ], axis=-1)
    save_pbr_set("ceiling_acoustic", np.clip(alb, 0.1, 0.9), 0.5 - 0.4 * speck + 0.3 * grid, np.full((S, S), 0.92, dtype=np.float32), 2.4)

    # 9. Walnut Wood (Doors, Furniture, Frames)
    nwl = tileable_fbm(S, S, octaves=5, base_freq=5, seed=901)
    wgrain = 0.5 + 0.5 * np.sin((y * 22.0 + nwl * 4.2) * 2 * math.pi)
    alb = np.stack([
        0.31 - 0.10 * wgrain + (nwl - 0.5) * 0.05,
        0.19 - 0.07 * wgrain + (nwl - 0.5) * 0.04,
        0.11 - 0.04 * wgrain + (nwl - 0.5) * 0.03,
    ], axis=-1)
    save_pbr_set("wood_walnut_furniture", np.clip(alb, 0.04, 0.65), 0.6 * wgrain + 0.4 * nwl, np.clip(0.36 + 0.16 * wgrain, 0.22, 0.72), 2.0)

    # 10. Fabric Upholstery
    nf = tileable_fbm(S, S, octaves=5, base_freq=6, seed=1001)
    weave = 0.5 + 0.25 * np.sin(x * 2 * math.pi * 64) + 0.25 * np.sin(y * 2 * math.pi * 64)
    alb = np.stack([
        0.34 + 0.06 * weave + (nf - 0.5) * 0.08,
        0.37 + 0.06 * weave + (nf - 0.5) * 0.08,
        0.26 + 0.05 * weave + (nf - 0.5) * 0.06,
    ], axis=-1)
    save_pbr_set("fabric_upholstery", np.clip(alb, 0.06, 0.75), weave * 0.7 + nf * 0.3, np.full((S, S), 0.88, dtype=np.float32), 2.6)

    # 11. Brushed Steel & Rusted Iron
    nm = tileable_fbm(S, S, octaves=5, base_freq=8, seed=1101)
    brush = tileable_fbm(S, S, octaves=3, base_freq=32, seed=1102)
    brush_h = gaussian_filter(brush, sigma=(0.5, 6.0), mode="wrap")
    alb_s = np.stack([
        0.58 + (brush_h - 0.5) * 0.16 + (nm - 0.5) * 0.08,
        0.60 + (brush_h - 0.5) * 0.16 + (nm - 0.5) * 0.08,
        0.62 + (brush_h - 0.5) * 0.16 + (nm - 0.5) * 0.08,
    ], axis=-1)
    save_pbr_set("metal_brushed_steel", np.clip(alb_s, 0.2, 0.9), brush_h * 0.6 + nm * 0.4, np.clip(0.28 + 0.2 * nm, 0.15, 0.6), 1.8)

    nr = tileable_fbm(S, S, octaves=6, base_freq=7, seed=1201)
    rust = np.clip((nr - 0.42) * 2.4, 0, 1)
    alb_r = np.stack([
        0.30 * (1 - rust) + 0.46 * rust + (nm - 0.5) * 0.06,
        0.31 * (1 - rust) + 0.21 * rust + (nm - 0.5) * 0.05,
        0.32 * (1 - rust) + 0.11 * rust + (nm - 0.5) * 0.04,
    ], axis=-1)
    save_pbr_set("metal_rusted_iron", np.clip(alb_r, 0.05, 0.8), nr * 0.8 + rust * 0.4, np.clip(0.35 + 0.52 * rust, 0.25, 0.95), 3.6)


def build_paintings_and_windows():
    # Normal Painting: Moody coastal lighthouse oil painting (512x384)
    W, H = 512, 384
    img = Image.new("RGB", (W, H), (24, 34, 48))
    d = ImageDraw.Draw(img)
    # Gradient twilight sky
    px = np.zeros((H, W, 3), dtype=np.float32)
    n = tileable_fbm(H, W, octaves=5, base_freq=4, seed=77)
    yy = np.linspace(0, 1, H)[:, None]
    xx = np.linspace(0, 1, W)[None, :]
    # Sky top to horizon (0..0.62)
    px[..., 0] = (1 - yy) * 0.16 + yy * 0.58 + (n - 0.5) * 0.12
    px[..., 1] = (1 - yy) * 0.22 + yy * 0.52 + (n - 0.5) * 0.10
    px[..., 2] = (1 - yy) * 0.34 + yy * 0.44 + (n - 0.5) * 0.10
    # Sea below 0.62
    sea = yy > 0.62
    waves = 0.5 + 0.5 * np.sin(yy * 190.0 + n * 8.0)
    px[..., 0] = np.where(sea, 0.10 + 0.12 * waves, px[..., 0])
    px[..., 1] = np.where(sea, 0.18 + 0.15 * waves, px[..., 1])
    px[..., 2] = np.where(sea, 0.25 + 0.16 * waves, px[..., 2])
    img = Image.fromarray(np.clip(px * 255, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # Cliff & Lighthouse silhouette
    d.polygon([(0, 255), (90, 210), (195, 225), (240, 265), (0, 285)], fill=(32, 30, 28))
    d.polygon([(142, 220), (152, 118), (166, 118), (176, 220)], fill=(215, 208, 192))
    d.rectangle([148, 104, 170, 118], fill=(45, 42, 40))
    d.ellipse([151, 106, 167, 117], fill=(255, 232, 155))
    img = img.filter(ImageFilter.GaussianBlur(0.8))
    img.save(os.path.join(TEX_DIR, "painting_coastal_normal.png"), optimize=True)

    # Anomaly Painting: Distorted hollow-eyed portrait bleeding oil streaks
    pa = np.zeros((H, W, 3), dtype=np.float32)
    na = tileable_fbm(H, W, octaves=6, base_freq=5, seed=88)
    pa[..., 0] = 0.10 + 0.14 * na
    pa[..., 1] = 0.06 + 0.07 * na
    pa[..., 2] = 0.06 + 0.06 * na
    # Pale elongated face oval
    face_d = ((xx - 0.5) / 0.19) ** 2 + ((yy - 0.44) / 0.30) ** 2
    skin = np.clip(1.0 - face_d, 0, 1) ** 0.55
    pa[..., 0] += skin * (0.62 - 0.18 * na)
    pa[..., 1] += skin * (0.56 - 0.22 * na)
    pa[..., 2] += skin * (0.50 - 0.22 * na)
    # Hollow dripping sockets & gaping mouth
    le = ((xx - 0.43) / 0.042) ** 2 + ((yy - 0.39) / 0.055) ** 2
    re_ = ((xx - 0.57) / 0.045) ** 2 + ((yy - 0.38) / 0.060) ** 2
    mo = ((xx - 0.50) / 0.052) ** 2 + ((yy - 0.58) / 0.105) ** 2
    void_mask = np.clip(np.maximum.reduce([1.2 - le, 1.2 - re_, 1.15 - mo]), 0, 1)
    pa *= (1.0 - 0.95 * void_mask[..., None])
    # Pinprick pale pupils inside dark sockets
    pup = (le < 0.06) | (re_ < 0.06)
    pa[pup] = [0.88, 0.18, 0.14]
    # Vertical dark crimson oil streaks beneath eyes
    streaks = ((np.abs(xx - 0.43) < 0.022) | (np.abs(xx - 0.57) < 0.022)) & (yy > 0.42) & (na > 0.35)
    pa[..., 0] = np.where(streaks, 0.32, pa[..., 0])
    pa[..., 1] = np.where(streaks, 0.03, pa[..., 1])
    pa[..., 2] = np.where(streaks, 0.03, pa[..., 2])
    Image.fromarray(np.clip(pa * 255, 0, 255).astype(np.uint8)).save(
        os.path.join(TEX_DIR, "painting_hollow_anomaly.png"), optimize=True
    )

    # Normal 4th-Floor Night City Window View (512x512)
    S = 512
    wy = np.linspace(0, 1, S)[:, None]
    wx = np.linspace(0, 1, S)[None, :]
    wn = tileable_fbm(S, S, octaves=4, base_freq=4, seed=91)
    city = np.zeros((S, S, 3), dtype=np.float32)
    city[..., 0] = 0.04 + 0.08 * wy + 0.03 * wn
    city[..., 1] = 0.06 + 0.09 * wy + 0.03 * wn
    city[..., 2] = 0.11 + 0.12 * wy + 0.04 * wn
    c_img = Image.fromarray(np.clip(city * 255, 0, 255).astype(np.uint8))
    cd = ImageDraw.Draw(c_img)
    rng = np.random.default_rng(1994)
    for bx in range(0, S, 54):
        bh = int(rng.integers(150, 340))
        bw = int(rng.integers(42, 62))
        cd.rectangle([bx, S - bh, bx + bw, S], fill=(14, 17, 24))
        for wy_i in range(S - bh + 14, S - 20, 18):
            for wx_i in range(bx + 6, bx + bw - 8, 12):
                if rng.random() < 0.34:
                    col = (235, 195, 118) if rng.random() < 0.78 else (140, 195, 220)
                    cd.rectangle([wx_i, wy_i, wx_i + 6, wy_i + 9], fill=col)
    c_img.save(os.path.join(TEX_DIR, "window_city_night.png"), optimize=True)

    # Anomaly Window View: Impossible Crimson Abyssal Sky & Inverted Brutalist Monoliths
    vn = tileable_fbm(S, S, octaves=6, base_freq=5, seed=666)
    ring = np.exp(-((np.sqrt((wx - 0.5) ** 2 + (wy - 0.38) ** 2) - 0.18) ** 2) / 0.0018)
    void = np.zeros((S, S, 3), dtype=np.float32)
    void[..., 0] = 0.28 + 0.45 * vn + 0.65 * ring
    void[..., 1] = 0.03 + 0.08 * vn + 0.22 * ring
    void[..., 2] = 0.14 + 0.24 * vn + 0.15 * ring
    v_img = Image.fromarray(np.clip(void * 255, 0, 255).astype(np.uint8))
    vd = ImageDraw.Draw(v_img)
    # Inverted floating monoliths hanging from the sky
    for mx, mw, mh, my0 in [(65, 48, 210, 0), (190, 62, 275, 20), (335, 54, 240, 0), (430, 42, 190, 35)]:
        vd.polygon([(mx, my0), (mx + mw, my0), (mx + mw - 12, my0 + mh), (mx + 12, my0 + mh)], fill=(12, 6, 14))
    v_img.save(os.path.join(TEX_DIR, "window_void_anomaly.png"), optimize=True)

    # Document Parchment Texture
    doc = Image.new("RGB", (256, 320), (224, 216, 196))
    dd = ImageDraw.Draw(doc)
    dd.rectangle([16, 18, 240, 42], fill=(45, 40, 38))
    for ly in range(60, 295, 14):
        lw = 190 if (ly // 14) % 4 != 3 else 125
        dd.rectangle([22, ly, 22 + lw, ly + 4], fill=(65, 60, 56))
    dd.rectangle([168, 255, 232, 296], outline=(155, 38, 34), width=3)
    doc.save(os.path.join(TEX_DIR, "document_page.png"), optimize=True)


def make_icon(name, draw_fn, size=64):
    # Supersample 4x for razor-sharp anti-aliased custom vector icons
    ss = size * 4
    img = Image.new("RGBA", (ss, ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    draw_fn(d, ss)
    out = img.resize((size, size), Image.Resampling.LANCZOS)
    out.save(os.path.join(ICON_DIR, f"{name}.png"), optimize=True)
    return out


def build_ui_icons_and_branding():
    WHITE = (236, 232, 224, 255)
    AMBER = (235, 186, 98, 255)
    CRIMSON = (208, 62, 58, 255)
    DIM = (150, 156, 166, 220)

    # 1. Flashlight Icon
    def d_flash(d, s):
        d.rounded_rectangle([int(s*0.12), int(s*0.40), int(s*0.52), int(s*0.60)], radius=12, fill=WHITE)
        d.polygon([(int(s*0.50), int(s*0.40)), (int(s*0.68), int(s*0.28)), (int(s*0.68), int(s*0.72)), (int(s*0.50), int(s*0.60))], fill=WHITE)
        for y0, y1 in [(0.34, 0.22), (0.50, 0.50), (0.66, 0.78)]:
            d.line([(int(s*0.74), int(s*y0)), (int(s*0.92), int(s*y1))], fill=AMBER, width=10)
    make_icon("icon_flashlight", d_flash)

    # 2. Anomaly Field Scanner Icon
    def d_scan(d, s):
        m = int(s*0.14)
        d.ellipse([m, m, s-m, s-m], outline=WHITE, width=10)
        d.ellipse([int(s*0.36), int(s*0.36), int(s*0.64), int(s*0.64)], outline=AMBER, width=8)
        for a, b in [((s//2, 8), (s//2, int(s*0.26))), ((s//2, int(s*0.74)), (s//2, s-8)),
                     ((8, s//2), (int(s*0.26), s//2)), ((int(s*0.74), s//2), (s-8, s//2))]:
            d.line([a, b], fill=AMBER, width=8)
    make_icon("icon_scanner", d_scan)

    # 3. Journal / Inspector Dossier Icon
    def d_journal(d, s):
        d.rounded_rectangle([int(s*0.20), int(s*0.12), int(s*0.82), int(s*0.88)], radius=14, outline=WHITE, width=10)
        d.line([(int(s*0.31), int(s*0.12)), (int(s*0.31), int(s*0.88))], fill=AMBER, width=8)
        for ry in [0.32, 0.48, 0.64]:
            d.line([(int(s*0.42), int(s*ry)), (int(s*0.70), int(s*ry))], fill=WHITE, width=8)
    make_icon("icon_journal", d_journal)

    # 4. Acoustic Stealth / Noise Icon
    def d_sound(d, s):
        d.polygon([(int(s*0.16), int(s*0.40)), (int(s*0.32), int(s*0.40)), (int(s*0.48), int(s*0.24)),
                   (int(s*0.48), int(s*0.76)), (int(s*0.32), int(s*0.60)), (int(s*0.16), int(s*0.60))], fill=WHITE)
        d.arc([int(s*0.42), int(s*0.30), int(s*0.70), int(s*0.70)], -55, 55, fill=AMBER, width=10)
        d.arc([int(s*0.34), int(s*0.18), int(s*0.86), int(s*0.82)], -55, 55, fill=WHITE, width=10)
    make_icon("icon_sound", d_sound)

    # 5. Category Visual (Eye)
    def d_eye(d, s):
        d.arc([int(s*0.10), int(s*0.24), int(s*0.90), int(s*0.96)], 205, 335, fill=WHITE, width=11)
        d.arc([int(s*0.10), int(s*0.04), int(s*0.90), int(s*0.76)], 25, 155, fill=WHITE, width=11)
        d.ellipse([int(s*0.38), int(s*0.38), int(s*0.62), int(s*0.62)], fill=AMBER)
        d.ellipse([int(s*0.45), int(s*0.45), int(s*0.55), int(s*0.55)], fill=(18, 18, 22, 255))
    make_icon("icon_cat_visual", d_eye)

    # 6. Category Audio (Waveform)
    def d_wave(d, s):
        bars = [0.22, 0.42, 0.72, 0.34, 0.64, 0.28]
        for i, bh in enumerate(bars):
            cx = int(s * (0.18 + i * 0.128))
            hh = int(s * bh * 0.5)
            d.rounded_rectangle([cx - 7, s//2 - hh, cx + 7, s//2 + hh], radius=5, fill=AMBER if i in (2, 4) else WHITE)
    make_icon("icon_cat_audio", d_wave)

    # 7. Category Environmental (Door/Threshold)
    def d_door(d, s):
        d.rectangle([int(s*0.24), int(s*0.12), int(s*0.76), int(s*0.88)], outline=WHITE, width=10)
        d.polygon([(int(s*0.26), int(s*0.14)), (int(s*0.62), int(s*0.22)), (int(s*0.62), int(s*0.80)), (int(s*0.26), int(s*0.86))], fill=DIM)
        d.ellipse([int(s*0.51), int(s*0.48), int(s*0.58), int(s*0.55)], fill=AMBER)
    make_icon("icon_cat_env", d_door)

    # 8. Category Entity (Watcher Silhouette)
    def d_entity(d, s):
        d.ellipse([int(s*0.37), int(s*0.10), int(s*0.63), int(s*0.38)], fill=CRIMSON)
        d.polygon([(int(s*0.24), int(s*0.88)), (int(s*0.33), int(s*0.42)), (int(s*0.67), int(s*0.42)), (int(s*0.76), int(s*0.88))], fill=WHITE)
        d.ellipse([int(s*0.46), int(s*0.20), int(s*0.54), int(s*0.28)], fill=(15, 15, 18, 255))
    make_icon("icon_cat_entity", d_entity)

    # 9. Key / Clearance / Tape / Document / Lock Icons
    def d_key(d, s):
        d.ellipse([int(s*0.14), int(s*0.30), int(s*0.48), int(s*0.64)], outline=AMBER, width=11)
        d.line([(int(s*0.46), int(s*0.47)), (int(s*0.86), int(s*0.47))], fill=AMBER, width=11)
        d.line([(int(s*0.70), int(s*0.47)), (int(s*0.70), int(s*0.64))], fill=AMBER, width=10)
        d.line([(int(s*0.82), int(s*0.47)), (int(s*0.82), int(s*0.60))], fill=AMBER, width=10)
    make_icon("icon_key", d_key)

    def d_tape(d, s):
        d.rounded_rectangle([int(s*0.12), int(s*0.24), int(s*0.88), int(s*0.76)], radius=12, outline=WHITE, width=10)
        d.ellipse([int(s*0.24), int(s*0.38), int(s*0.44), int(s*0.58)], outline=AMBER, width=8)
        d.ellipse([int(s*0.56), int(s*0.38), int(s*0.76), int(s*0.58)], outline=AMBER, width=8)
        d.line([(int(s*0.34), int(s*0.58)), (int(s*0.66), int(s*0.58))], fill=WHITE, width=6)
    make_icon("icon_tape", d_tape)

    def d_lock(d, s):
        d.arc([int(s*0.30), int(s*0.14), int(s*0.70), int(s*0.56)], 180, 0, fill=WHITE, width=11)
        d.rounded_rectangle([int(s*0.22), int(s*0.44), int(s*0.78), int(s*0.86)], radius=12, fill=CRIMSON)
        d.ellipse([int(s*0.45), int(s*0.58), int(s*0.55), int(s*0.68)], fill=WHITE)
    make_icon("icon_lock", d_lock)

    # 10. Custom App Icon (256x256 PNG + Multi-size Windows .ico)
    A = 512
    app = Image.new("RGBA", (A, A), (12, 13, 17, 255))
    ad = ImageDraw.Draw(app)
    # Recessed brutalist corridor perspective lines
    for inset, col in [(24, (36, 39, 48, 255)), (76, (52, 46, 44, 255)), (134, (74, 34, 34, 255))]:
        ad.rectangle([inset, inset, A - inset, A - inset], outline=col, width=6)
    for corner in [(24, 24, 134, 134), (A-24, 24, A-134, 134), (24, A-24, 134, A-134), (A-24, A-24, A-134, A-134)]:
        ad.line(corner, fill=(58, 52, 50, 255), width=4)
    # Glowing crimson-amber doorway slit & watching eye
    ad.rectangle([216, 148, 296, 384], fill=(195, 48, 42, 255))
    ad.polygon([(216, 384), (296, 384), (370, 488), (142, 488)], fill=(95, 24, 22, 180))
    # Tall figure silhouette standing inside doorway
    ad.ellipse([241, 188, 271, 224], fill=(10, 10, 14, 255))
    ad.polygon([(232, 228), (280, 228), (274, 384), (238, 384)], fill=(10, 10, 14, 255))
    ad.ellipse([251, 200, 261, 208], fill=(248, 214, 140, 255))
    app_256 = app.resize((256, 256), Image.Resampling.LANCZOS)
    app_256.save(os.path.join(UI_DIR, "app_icon.png"), optimize=True)
    app_256.save(
        os.path.join(UI_DIR, "app_icon.ico"),
        format="ICO",
        sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )

    # 11. Custom Boot Splash Screen (1280x720) — zero Godot branding!
    BW, BH = 1280, 720
    splash = Image.new("RGB", (BW, BH), (9, 10, 13))
    sd = ImageDraw.Draw(splash)
    # Atmospheric corridor perspective art
    cx, cy = BW // 2, BH // 2 - 20
    for i in range(12, 0, -1):
        rx = int(520 * (i / 12.0))
        ry = int(310 * (i / 12.0))
        shade = int(12 + (12 - i) * 3.5)
        sd.rectangle([cx - rx, cy - ry, cx + rx, cy + ry], outline=(shade + 8, shade + 4, shade + 4), width=2)
    # Lit distant doorway & silhouette
    sd.rectangle([cx - 34, cy - 86, cx + 34, cy + 88], fill=(168, 52, 40))
    sd.ellipse([cx - 11, cy - 52, cx + 11, cy - 26], fill=(11, 11, 14))
    sd.polygon([(cx - 19, cy - 22), (cx + 19, cy - 22), (cx + 15, cy + 88), (cx - 15, cy + 88)], fill=(11, 11, 14))
    # Subtle title bar geometry
    sd.line([(cx - 260, BH - 135), (cx + 260, BH - 135)], fill=(185, 62, 50), width=2)
    try:
        f_big = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 42)
        f_sub = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16)
    except Exception:
        f_big = ImageFont.load_default()
        f_sub = ImageFont.load_default()
    sd.text((cx, BH - 102), "D O N ' T   L O O K   B A C K", fill=(232, 226, 216), font=f_big, anchor="mm")
    sd.text((cx, BH - 62), "KESTREL COURT MUNICIPAL CONTAINMENT ARCHIVE  //  LOADING REALITY STATE", fill=(142, 138, 134), font=f_sub, anchor="mm")
    splash.save(os.path.join(UI_DIR, "boot_splash.png"), optimize=True)

    # 12. Installer Banner BMP (600x90) for Windows Setup Wizard
    ib = splash.resize((600, 338), Image.Resampling.LANCZOS).crop((0, 210, 600, 310))
    ib.save(os.path.join(UI_DIR, "installer_banner.bmp"), format="BMP")


if __name__ == "__main__":
    print("Generating seamless PBR texture sets...")
    build_pbr_materials()
    print("Generating narrative paintings & window views...")
    build_paintings_and_windows()
    print("Generating custom vector UI icons, boot splash & multi-size Windows .ico...")
    build_ui_icons_and_branding()
    print("Done! Generated textures:", len(os.listdir(TEX_DIR)), "icons:", len(os.listdir(ICON_DIR)))
