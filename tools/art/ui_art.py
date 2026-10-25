"""SPACE MINER — font atlases, projectile sprites, misc VFX art."""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from artlib import clamp01, hex2rgb, radial

FONT_DIR = 'fonts'

FONTS = [
    # name, file, size, line height
    ('font_tiny',   'Rajdhani-SemiBold.ttf', 13, 16),
    ('font_small',  'Rajdhani-SemiBold.ttf', 17, 21),
    ('font_body',   'Rajdhani-Medium.ttf',   21, 26),
    ('font_head',   'Orbitron-SemiBold.ttf', 22, 28),
    ('font_title',  'Rajdhani-Bold.ttf',     40, 46),
    ('font_big',    'Orbitron-ExtraBold.ttf', 54, 60),
]


def bake_font(path, size, line_height, out_png, out_meta):
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    glyphs = []
    pad = 2
    total_w = 8
    max_h = 0
    for code in range(32, 128):
        ch = chr(code)
        bbox = font.getbbox(ch)
        adv = int(round(font.getlength(ch)))
        gw = max(0, bbox[2] - bbox[0])
        gh = max(0, bbox[3] - bbox[1])
        glyphs.append(dict(code=code, ch=ch, bbox=bbox, adv=adv, gw=gw, gh=gh))
        total_w += gw + pad * 2
        max_h = max(max_h, gh)
    img = Image.new('L', (total_w, max_h + 4), 0)
    d = ImageDraw.Draw(img)
    x = 4
    meta = [f'size {size}', f'ascent {ascent}', f'line {line_height}']
    for i, g in enumerate(glyphs):
        if g['gw'] > 0 and g['gh'] > 0:
            d.text((x - g['bbox'][0], 2 - g['bbox'][1]), g['ch'], font=font, fill=255)
        meta.append(f"g {i} {x} 2 {g['gw']} {g['gh']} {g['adv']} {g['bbox'][0]} {g['bbox'][1]}")
        x += g['gw'] + pad * 2
    img = img.crop((0, 0, x + 2, img.height))
    rgba = Image.new('RGBA', img.size, (255, 255, 255, 0))
    rgba.putalpha(img)
    rgba.save(out_png)
    with open(out_meta, 'w') as f:
        f.write('\n'.join(meta) + '\n')
    return rgba.size


# --------------------------------------------------------------------------- #
#  projectiles: drawn pointing +X (rotated at runtime)
# --------------------------------------------------------------------------- #
def bolt_sprite(w=64, h=20, core=(255, 255, 255), mid=(140, 225, 255), outer=(60, 140, 255)):
    cv = np.zeros((h * 4, w * 4, 4))
    H, W = h * 4, w * 4
    ys, xs = np.mgrid[0:H, 0:W]
    cx, cy = W / 2, H / 2
    # capsule along x
    ex = (xs - cx) / (W * 0.42)
    ey = (ys - cy) / (H * 0.22)
    d = np.sqrt(ex ** 2 + ey ** 2)
    core_m = clamp01(1 - d / 0.55) ** 1.5
    mid_m = clamp01(1 - d) ** 1.1
    glow = np.exp(-(d ** 2) / 0.55) * 0.55
    rgb = np.zeros((H, W, 3))
    for i in range(3):
        rgb[..., i] = outer[i] / 255 * mid_m + mid[i] / 255 * mid_m * 0.5 + core[i] / 255 * core_m
    a = clamp01(mid_m * 0.95 + glow)
    rgb = np.clip(rgb, 0, 1)
    arr = np.concatenate([rgb, a[..., None]], axis=2)
    img = Image.fromarray((arr * 255).astype(np.uint8), 'RGBA')
    return img.resize((w, h), Image.LANCZOS)


def orb_sprite(size=48, inner=(255, 235, 255), mid=(255, 90, 210), outer=(90, 20, 120)):
    H = W = size * 4
    ys, xs = np.mgrid[0:H, 0:W]
    d = np.sqrt(((xs - W / 2) / (W / 2)) ** 2 + ((ys - H / 2) / (H / 2)) ** 2)
    core = clamp01(1 - d / 0.42) ** 1.2
    halo = clamp01(1 - d) ** 2
    rgb = np.zeros((H, W, 3))
    for i in range(3):
        rgb[..., i] = outer[i] / 255 * halo * 0.8 + mid[i] / 255 * halo + inner[i] / 255 * core
    a = clamp01(halo * 0.9 + core)
    arr = np.concatenate([np.clip(rgb, 0, 1), a[..., None]], axis=2)
    return Image.fromarray((arr * 255).astype(np.uint8), 'RGBA').resize((size, size), Image.LANCZOS)


def lance_sprite(w=96, h=16, core=(255, 255, 255), mid=(255, 160, 255), outer=(120, 40, 200)):
    H, W = h * 4, w * 4
    ys, xs = np.mgrid[0:H, 0:W]
    t = xs / W
    taper = np.clip(1.0 - np.abs(ys - H / 2) / (H / 2 + 1e-6) - (t * 0.55), 0, 1)
    body = np.clip(taper * 2.2, 0, 1)
    tip = np.clip((t - 0.62) * 4.0, 0, 1) * body
    rgb = np.zeros((H, W, 3))
    for i in range(3):
        rgb[..., i] = outer[i] / 255 * body * 0.7 + mid[i] / 255 * body + core[i] / 255 * tip
    a = clamp01(body * 0.95 + tip * 0.6)
    arr = np.concatenate([np.clip(rgb, 0, 1), a[..., None]], axis=2)
    return Image.fromarray((arr * 255).astype(np.uint8), 'RGBA').resize((w, h), Image.LANCZOS)


def missile_sprite(w=40, h=14):
    """Hostile missile: dark casing with a hot exhaust."""
    H, W = h * 4, w * 4
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon([(W * 0.34, H * 0.5), (W * 0.62, H * 0.18), (W * 0.72, H * 0.5),
               (W * 0.62, H * 0.82)], fill=(190, 196, 206, 255))
    d.polygon([(W * 0.62, H * 0.18), (W * 0.92, H * 0.5), (W * 0.62, H * 0.82)], fill=(150, 158, 170, 255))
    d.polygon([(W * 0.34, H * 0.5), (W * 0.10, H * 0.30), (W * 0.16, H * 0.34),
               (W * 0.16, H * 0.66), (W * 0.10, H * 0.70)], fill=(255, 120, 60, 235))
    d.ellipse([W * 0.60, H * 0.36, W * 0.78, H * 0.64], fill=(255, 90, 70, 255))
    a = np.array(img.split()[3], dtype=np.float64) / 255.0
    blur = np.asarray(Image.fromarray((a * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3)),
                      dtype=np.float64) / 255.0
    rgb = np.array(img, dtype=np.float64)[..., :3] / 255.0
    rgb = rgb + np.clip(blur - a, 0, 1)[..., None] * np.array([1.0, 0.45, 0.15]) * 0.9
    arr = np.concatenate([np.clip(rgb, 0, 1), np.clip(np.maximum(a, blur), 0, 1)[..., None]], axis=2)
    return Image.fromarray((arr * 255).astype(np.uint8), 'RGBA').resize((w, h), Image.LANCZOS)


def chunk_sprite(size=24, color=(140, 150, 165)):
    H = W = size * 3
    rng = np.random.default_rng(5)
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pts = []
    for i in range(6):
        a = i / 6 * 6.283
        rr = W * (0.30 + rng.random() * 0.16)
        pts.append((W / 2 + np.cos(a) * rr, H / 2 + np.sin(a) * rr))
    d.polygon(pts, fill=tuple(list(color) + [255]))
    hi = tuple(min(255, int(c * 1.45)) for c in color) + (255,)
    d.polygon([pts[0], pts[1], (W / 2, H / 2)], fill=hi)
    return img.resize((size, size), Image.LANCZOS)


if __name__ == '__main__':
    import sys
    out = sys.argv[1] if len(sys.argv) > 1 else 'assets/gen'
    os.makedirs(out, exist_ok=True)
    for name, fn, size, lh in FONTS:
        w, h = bake_font(os.path.join(FONT_DIR, fn), size, lh, f'{out}/{name}.png', f'{out}/{name}.txt')
        print(name, w, h)
    bolt_sprite().save(f'{out}/proj_bolt.png')
    orb_sprite(48, (255, 240, 255), (255, 90, 200), (90, 20, 120)).save(f'{out}/proj_plasma.png')
    orb_sprite(40, (240, 255, 255), (120, 255, 255), (20, 60, 120)).save(f'{out}/proj_enemy.png')
    orb_sprite(56, (255, 255, 255), (255, 120, 80), (110, 30, 10)).save(f'{out}/proj_heavy.png')
    lance_sprite().save(f'{out}/proj_lance.png')
    missile_sprite().save(f'{out}/proj_missile.png')
    chunk_sprite(24, (150, 160, 175)).save(f'{out}/fx_chunk.png')
    print('vfx')
