"""SPACE MINER — nebula tiles, particle sprites, thruster flames, station art."""
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from artlib import Canvas, clamp01, smoothstep, hex2rgb, value_noise, fbm_periodic, radial

TILE = 320


def nebula_tile(seed, c_a, c_b, c_c, cloud=2.6, wispy=1.0):
    """Tileable deep-space gas cloud.

    Deliberately baked dark (empty sky is black) so the backdrop can be blended
    additively in-game without washing the scene out; only the gas itself glows.
    Structure: smooth billow -> soft envelope -> gradient-lit rims -> hot cores.
    """
    n = fbm_periodic(TILE, TILE, 3, seed, octaves=5, gain=0.55)
    big = fbm_periodic(TILE, TILE, 2, seed + 9, octaves=3, gain=0.5)
    w = fbm_periodic(TILE, TILE, 7, seed + 21, octaves=5, gain=0.55)
    dens = clamp01((n * 0.70 + w * 0.30 - 0.30) / 0.55) ** 1.5
    env = smoothstep(clamp01((big - 0.22) / 0.5))
    dens = clamp01(dens * env * 1.35 * wispy)
    gy, gx = np.gradient(dens)
    edge = clamp01(np.sqrt(gx * gx + gy * gy) * 6.0)

    t = clamp01(fbm_periodic(TILE, TILE, 3, seed + 41, octaves=3) * 1.7 - 0.3)
    a, b, c = np.array(hex2rgb(c_a)), np.array(hex2rgb(c_b)), np.array(hex2rgb(c_c))
    col = a[None, None, :] * (1 - t[..., None]) + b[None, None, :] * t[..., None]
    rgb = col * np.power(dens, 0.72)[..., None] * 1.18
    rgb = rgb + c[None, None, :] * (edge * dens ** 0.5)[..., None] * 0.60
    core = clamp01((dens - 0.70) / 0.30) ** 2
    rgb = rgb + c[None, None, :] * (core * 0.65)[..., None]
    # a faint uniform haze keeps the tile from clipping to pure black
    rgb = rgb + (env * dens * 0.05)[..., None] * col
    return Image.fromarray((clamp01(rgb) * 255).astype(np.uint8), 'RGB').convert('RGBA')


def soft_sprite(size, power=2.0, color=(255, 255, 255)):
    r = radial(size, size, power=power)
    rgb = np.ones((size, size, 3)) * (np.array(color, dtype=np.float64) / 255.0)
    arr = np.concatenate([rgb, clamp01(r)[..., None]], axis=2)
    return Image.fromarray((clamp01(arr) * 255).astype(np.uint8), 'RGBA')


def smoke_sheet(frames=12, size=72):
    strip = Image.new('RGBA', (size * frames, size), (0, 0, 0, 0))
    for f in range(frames):
        t = f / (frames - 1.0)
        n = fbm_periodic(size, size, 4, 700 + f, octaves=4, gain=0.55)
        r = np.sqrt(((np.mgrid[0:size, 0:size][1] - size / 2) / (size / 2)) ** 2 +
                    ((np.mgrid[0:size, 0:size][0] - size / 2) / (size / 2)) ** 2)
        puff = clamp01(1.0 - r / (0.55 + 0.42 * t)) * (0.55 + 0.55 * n)
        alpha = clamp01(puff * (1.0 - t * 0.9)) * 0.85
        val = (0.35 + 0.5 * (1 - t)) * (0.7 + 0.6 * n)
        arr = np.stack([val, val * 0.98, val * 1.0, alpha], axis=2)
        img = Image.fromarray((clamp01(arr) * 255).astype(np.uint8), 'RGBA')
        strip.paste(img, (f * size, 0))
    return strip


def spark_sheet(frames=6, size=32):
    strip = Image.new('RGBA', (size * frames, size), (0, 0, 0, 0))
    for f in range(frames):
        im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        t = f / (frames - 1.0)
        L = size * (0.9 - 0.45 * t)
        w = max(1, int(size * 0.09 * (1 - t) + 1))
        d.line([(size / 2 - L / 2, size / 2), (size / 2 + L / 2, size / 2)], fill=(255, 255, 255, 255), width=w)
        im = im.filter(ImageFilter.GaussianBlur(1.2))
        strip.paste(im, (f * size, 0))
    return strip


def flame_sheet(w=64, h=128, frames=10, core='#cfefff', mid='#4fb6ff', outer='#1d4fff', tail=True):
    strip = Image.new('RGBA', (w * frames, h), (0, 0, 0, 0))
    cc, cm, co = hex2rgb(core), hex2rgb(mid), hex2rgb(outer)
    for f in range(frames):
        ph = f / frames * math.tau
        cv = Canvas(w, h, 3)
        img = Image.new('L', (w * 3, h * 3), 0)
        d = ImageDraw.Draw(img)
        # three nested teardrops with a flickering tip
        for layer, (col, scale, alpha) in enumerate([(co, 1.0, 0.55), (cm, 0.68, 0.75), (cc, 0.36, 1.0)]):
            lw = w * 3 * 0.42 * scale
            lh = h * 3 * (0.62 + 0.16 * math.sin(ph + layer)) * scale
            cx = w * 3 / 2
            pts = [(cx - lw / 2, h * 3 * 0.02), (cx - lw * 0.42, lh * 0.55),
                   (cx + math.sin(ph * 1.7) * lw * 0.10, lh),
                   (cx + lw * 0.42, lh * 0.55), (cx + lw / 2, h * 3 * 0.02)]
            m = np.zeros((h * 3, w * 3))
            tmp = Image.new('L', (w * 3, h * 3), 0)
            ImageDraw.Draw(tmp).polygon(pts, fill=255)
            m = np.asarray(tmp, dtype=np.float64) / 255.0
            cv.paint(m, col, alpha, over=(layer == 0))
        out = cv.downsample()
        strip.paste(out, (f * w, 0))
    return strip


def station_art(size=320):
    """Orbital refinery: the dock the player trades at."""
    p_ss = 3
    cv = Canvas(size, size, p_ss)
    S = size * p_ss
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    hull = hex2rgb('#8e97a4')
    dark = hex2rgb('#3a414c')
    plate = hex2rgb('#6e7784')
    glowc = hex2rgb('#ffd27a')
    cx, cy = S / 2, S / 2
    R = S * 0.44
    # main ring
    for rr, wd, col in [(R, S * 0.075, hull), (R * 0.80, S * 0.05, plate),
                        (R * 0.62, S * 0.03, dark)]:
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=tuple(int(c * 255) for c in col),
                  width=max(2, int(wd)))
    # spokes
    for i in range(6):
        a = i * math.tau / 6
        x0, y0 = cx + math.cos(a) * R * 0.28, cy + math.sin(a) * R * 0.28
        x1, y1 = cx + math.cos(a) * R, cy + math.sin(a) * R
        d.line([(x0, y0), (x1, y1)], fill=tuple(int(c * 255) for c in plate), width=max(2, int(S * 0.028)))
    # hub
    d.ellipse([cx - R * 0.32, cy - R * 0.32, cx + R * 0.32, cy + R * 0.32],
              fill=tuple(int(c * 255) for c in hull))
    d.ellipse([cx - R * 0.24, cy - R * 0.24, cx + R * 0.24, cy + R * 0.24],
              fill=tuple(int(c * 255) for c in plate))
    d.ellipse([cx - R * 0.14, cy - R * 0.14, cx + R * 0.14, cy + R * 0.14],
              fill=tuple(int(c * 255) for c in dark))
    # docking arms
    for i in range(3):
        a = i * math.tau / 3 + 0.5
        x0, y0 = cx + math.cos(a) * R * 0.95, cy + math.sin(a) * R * 0.95
        x1, y1 = cx + math.cos(a) * R * 1.35, cy + math.sin(a) * R * 1.35
        d.line([(x0, y0), (x1, y1)], fill=tuple(int(c * 255) for c in hull), width=max(2, int(S * 0.022)))
        d.ellipse([x1 - S * 0.028, y1 - S * 0.028, x1 + S * 0.028, y1 + S * 0.028],
                  fill=tuple(int(c * 255) for c in dark))
    # windows
    mask_img = np.asarray(img, dtype=np.float64) / 255.0
    m = mask_img[..., 3]
    m = np.clip(m * 1.4, 0, 1)
    mask = smoothstep(m)
    # shade using gradients + noise
    ys, xs = np.mgrid[0:S, 0:S]
    dx = (xs - cx) / (S / 2)
    dy = (ys - cy) / (S / 2)
    r = np.sqrt(dx * dx + dy * dy)
    n = value_noise(S, S, 26, 5, octaves=3) - 0.5
    shade = 0.62 + 0.5 * clamp01(0.5 - (dx * -0.5 + dy * -0.6) * 0.5) + 0.10 * n
    rgb = np.zeros((S, S, 3))
    base = mask_img[..., :3]
    rgb = base * shade[..., None]
    # ring highlight
    band = np.exp(-((r - 0.44) ** 2) / 0.0008) + np.exp(-((r - 0.35) ** 2) / 0.0005)
    rgb += (band * 0.5)[..., None] * np.array([0.35, 0.42, 0.52])
    # running lights
    for i in range(12):
        a = i * math.tau / 12
        px, py = cx + math.cos(a) * R, cy + math.sin(a) * R
        d2 = np.sqrt((xs - px) ** 2 + (ys - py) ** 2)
        dot = np.exp(-(d2 ** 2) / (2 * (S * 0.012) ** 2))
        rgb += dot[..., None] * glowc * 0.9
    cv.rgb = clamp01(rgb)
    cv.a = mask
    return cv.downsample()


def dust_sprite(size=12):
    return soft_sprite(size, power=1.4)


if __name__ == '__main__':
    import sys, os
    out = sys.argv[1] if len(sys.argv) > 1 else 'assets/gen'
    os.makedirs(out, exist_ok=True)
    tiles = [
        ('bg_nebula0', 11, '#2f5c9e', '#8a6a38', '#a8d4ff'),
        ('bg_nebula1', 23, '#7a2fb0', '#1f86a8', '#ff9ae0'),
        ('bg_nebula2', 37, '#2c6b56', '#9a6a34', '#9cffc4'),
        ('bg_nebula3', 53, '#b03a1c', '#5c1e94', '#ffc27a'),
        ('bg_nebula4', 71, '#4a2a9c', '#160f42', '#c8a4ff'),
    ]
    for name, seed, a, b, c in tiles:
        nebula_tile(seed, a, b, c).save(f'{out}/{name}.png')
        print(name)
    soft_sprite(64, 2.0).save(f'{out}/fx_glow.png')
    dust_sprite(16).save(f'{out}/fx_dust.png')
    smoke_sheet().save(f'{out}/fx_smoke.png')
    spark_sheet().save(f'{out}/fx_spark.png')
    flame_sheet().save(f'{out}/fx_flame_blue.png')
    flame_sheet(core='#fff2d0', mid='#ffb14f', outer='#ff5a1f').save(f'{out}/fx_flame_orange.png')
    flame_sheet(core='#ffffff', mid='#8affc8', outer='#22a06b').save(f'{out}/fx_flame_green.png')
    flame_sheet(core='#ffffff', mid='#c9a4ff', outer='#6a3cff').save(f'{out}/fx_flame_violet.png')
    station_art().save(f'{out}/station.png')
    print('station')
