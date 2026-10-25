#!/usr/bin/env python3
"""
Procedural Painterly & Material Rendering Toolkit for ALCHEMY CHAOS.
Uses NumPy + Pillow with 2x supersampling for crisp, anti-aliased original illustrations.
"""

import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageChops, ImageEnhance


def hex_to_rgb(h: str):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))


def hex_to_rgba(h: str, a: int = 255):
    r, g, b = hex_to_rgb(h)
    return (r, g, b, a)


def mix_color(c1, c2, t: float):
    t = max(0.0, min(1.0, t))
    return tuple(int(round(c1[i] * (1.0 - t) + c2[i] * t)) for i in range(len(c1)))


def brighten(c, factor: float):
    if len(c) == 4:
        return (
            min(255, int(c[0] * factor)),
            min(255, int(c[1] * factor)),
            min(255, int(c[2] * factor)),
            c[3],
        )
    return tuple(min(255, int(v * factor)) for v in c)


def value_noise_2d(h: int, w: int, scale: int = 16, seed: int = 42) -> np.ndarray:
    rng = np.random.default_rng(seed)
    gh = max(2, h // scale + 2)
    gw = max(2, w // scale + 2)
    grid = rng.random((gh, gw), dtype=np.float32)
    img = Image.fromarray((grid * 255).astype(np.uint8), mode="L")
    img = img.resize((w, h), resample=Image.Resampling.BICUBIC)
    return np.asarray(img, dtype=np.float32) / 255.0


def fbm_2d(h: int, w: int, base_scale: int = 48, octaves: int = 4, seed: int = 42) -> np.ndarray:
    acc = np.zeros((h, w), dtype=np.float32)
    amp = 0.5
    norm = 0.0
    s = base_scale
    for i in range(octaves):
        acc += amp * value_noise_2d(h, w, max(2, s), seed + i * 97)
        norm += amp
        amp *= 0.5
        s = max(2, s // 2)
    return acc / norm


def make_wood_texture(w: int, h: int, base_rgb=(68, 40, 24), dark_rgb=(34, 18, 10), grain_horizontal=True, seed=11) -> Image.Image:
    n1 = fbm_2d(h, w, base_scale=44, octaves=4, seed=seed)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    coord = (yy * 0.35 + n1 * 28.0) if grain_horizontal else (xx * 0.35 + n1 * 28.0)
    rings = 0.5 + 0.5 * np.sin(coord)
    fine = value_noise_2d(h, w, scale=4, seed=seed + 5)
    t = np.clip(0.55 * rings + 0.30 * n1 + 0.15 * fine, 0.0, 1.0)
    arr = np.zeros((h, w, 4), dtype=np.uint8)
    for ch in range(3):
        arr[:, :, ch] = np.clip(dark_rgb[ch] * (1.0 - t) + base_rgb[ch] * t, 0, 255).astype(np.uint8)
    arr[:, :, 3] = 255
    return Image.fromarray(arr, "RGBA")


def make_parchment_texture(w: int, h: int, seed=31) -> Image.Image:
    n = fbm_2d(h, w, base_scale=56, octaves=4, seed=seed)
    fine = value_noise_2d(h, w, scale=3, seed=seed + 19)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    ny = (yy / max(1, h - 1)) * 2.0 - 1.0
    nx = (xx / max(1, w - 1)) * 2.0 - 1.0
    edge = np.clip((nx * nx + ny * ny) * 0.55, 0.0, 1.0)
    light = np.array([239, 224, 191], dtype=np.float32)
    mid = np.array([214, 191, 148], dtype=np.float32)
    dark = np.array([158, 127, 86], dtype=np.float32)
    t = np.clip(0.7 * n + 0.3 * fine, 0.0, 1.0)[..., None]
    col = mid * (1.0 - t) + light * t
    col = col * (1.0 - edge[..., None] * 0.55) + dark * (edge[..., None] * 0.55)
    arr = np.zeros((h, w, 4), dtype=np.uint8)
    arr[:, :, :3] = np.clip(col, 0, 255).astype(np.uint8)
    arr[:, :, 3] = 255
    return Image.fromarray(arr, "RGBA")


def make_dark_slate_panel(
    w: int,
    h: int,
    fill_top=(28, 24, 38),
    fill_bot=(16, 13, 24),
    brass_outer=(198, 152, 74),
    brass_hi=(246, 214, 132),
    brass_dark=(96, 66, 28),
    corner_radius: int = 12,
    border_w: int = 3,
    rivets: bool = True,
    seed: int = 7,
) -> Image.Image:
    """Renders an ornate beveled alchemical slate-and-brass 9-sliceable UI frame."""
    ss = 2
    W, H = w * ss, h * ss
    R = corner_radius * ss
    B = border_w * ss

    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    ty = yy / max(1, H - 1)
    n = fbm_2d(H, W, base_scale=32, octaves=3, seed=seed)
    bg = np.zeros((H, W, 4), dtype=np.uint8)
    for c in range(3):
        grad = fill_top[c] * (1.0 - ty) + fill_bot[c] * ty
        bg[:, :, c] = np.clip(grad + (n - 0.5) * 14.0, 0, 255).astype(np.uint8)
    bg[:, :, 3] = 255
    base_img = Image.fromarray(bg, "RGBA")

    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([2, 2, W - 3, H - 3], radius=R, fill=255)

    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    # Subtle drop shadow
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle([4, 6, W - 2, H - 1], radius=R, fill=(0, 0, 0, 140))
    sh = sh.filter(ImageFilter.GaussianBlur(3))
    out = Image.alpha_composite(out, sh)

    clipped = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    clipped.paste(base_img, (0, 0), mask)
    out = Image.alpha_composite(out, clipped)

    d = ImageDraw.Draw(out)
    # Outer dark brass bevel
    d.rounded_rectangle([2, 2, W - 3, H - 3], radius=R, outline=(*brass_dark, 255), width=B + 2)
    # Main brass frame
    d.rounded_rectangle([3, 3, W - 4, H - 4], radius=R, outline=(*brass_outer, 255), width=B)
    # Top highlight inner bevel
    d.rounded_rectangle([3 + B, 3 + B, W - 4 - B, H - 4 - B], radius=max(2, R - B), outline=(*brass_hi, 110), width=2)
    # Subtle inner filigree hairline
    inset = B + 6
    if W > inset * 4 and H > inset * 4:
        d.rounded_rectangle(
            [inset, inset, W - 1 - inset, H - 1 - inset],
            radius=max(2, R - inset // 2),
            outline=(*brass_outer, 70),
            width=2,
        )

    if rivets and W >= 80 and H >= 64:
        rv_off = inset + 2
        for rx, ry in [(rv_off, rv_off), (W - 1 - rv_off, rv_off), (rv_off, H - 1 - rv_off), (W - 1 - rv_off, H - 1 - rv_off)]:
            d.ellipse([rx - 5, ry - 5, rx + 5, ry + 5], fill=(*brass_dark, 255))
            d.ellipse([rx - 4, ry - 4, rx + 4, ry + 4], fill=(*brass_outer, 255))
            d.ellipse([rx - 2, ry - 2, rx + 0, ry + 0], fill=(*brass_hi, 255))

    return out.resize((w, h), resample=Image.Resampling.LANCZOS)


def add_radial_glow(canvas: Image.Image, cx: float, cy: float, radius: float, color_rgb, alpha_max: int = 120) -> Image.Image:
    w, h = canvas.size
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    dist = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / max(1.0, radius)
    falloff = np.clip(1.0 - dist, 0.0, 1.0) ** 2.2
    arr = np.zeros((h, w, 4), dtype=np.uint8)
    arr[:, :, 0] = color_rgb[0]
    arr[:, :, 1] = color_rgb[1]
    arr[:, :, 2] = color_rgb[2]
    arr[:, :, 3] = (falloff * alpha_max).astype(np.uint8)
    glow = Image.fromarray(arr, "RGBA")
    return Image.alpha_composite(canvas, glow)


def draw_shaded_sphere(canvas: Image.Image, cx: float, cy: float, r: float, col_main, col_hi, col_dark=None, alpha: int = 255):
    """Renders a 3D Phong-shaded sphere onto canvas."""
    if col_dark is None:
        col_dark = tuple(max(0, int(c * 0.28)) for c in col_main[:3])
    w, h = canvas.size
    x0 = max(0, int(cx - r - 2))
    x1 = min(w, int(cx + r + 3))
    y0 = max(0, int(cy - r - 2))
    y1 = min(h, int(cy + r + 3))
    if x1 <= x0 or y1 <= y0:
        return
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    nx = (xx - cx) / r
    ny = (yy - cy) / r
    r2 = nx * nx + ny * ny
    inside = r2 <= 1.0
    nz = np.sqrt(np.clip(1.0 - r2, 0.0, 1.0))
    # Light from upper-left (-0.42, -0.52, 0.74)
    diff = np.clip(nx * (-0.42) + ny * (-0.52) + nz * 0.74, 0.0, 1.0)
    # Rim backlight
    rim = np.clip(1.0 - nz, 0.0, 1.0) ** 3.0 * 0.45
    spec = np.clip(nx * (-0.30) + ny * (-0.40) + nz * 0.86, 0.0, 1.0) ** 22.0
    # Anti-aliased edge
    aa = np.clip((1.0 - np.sqrt(np.maximum(r2, 1e-6))) * r, 0.0, 1.0)

    patch = np.zeros((y1 - y0, x1 - x0, 4), dtype=np.uint8)
    for c in range(3):
        base = col_dark[c] * (1.0 - diff) + col_main[c] * diff
        lit = base + col_hi[c] * (spec * 0.75 + rim * 0.5)
        patch[:, :, c] = np.clip(lit, 0, 255).astype(np.uint8)
    patch[:, :, 3] = (aa * alpha * inside).astype(np.uint8)

    sub = Image.fromarray(patch, "RGBA")
    tmp = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    tmp.paste(sub, (x0, y0))
    res = Image.alpha_composite(canvas, tmp)
    canvas.paste(res)


def add_outline_and_shadow(img: Image.Image, outline_rgba=(20, 14, 28, 220), shadow_alpha=110) -> Image.Image:
    """Adds crisp dark-ink painterly contour + soft ambient drop shadow."""
    w, h = img.size
    alpha = img.split()[3]
    dilated = alpha.filter(ImageFilter.MaxFilter(5))
    outline = Image.new("RGBA", (w, h), outline_rgba)
    outline.putalpha(ImageChops.multiply(dilated, Image.new("L", (w, h), outline_rgba[3])))

    shadow = Image.new("RGBA", (w, h), (6, 4, 12, 0))
    sh_alpha = alpha.filter(ImageFilter.GaussianBlur(6))
    sh_alpha = sh_alpha.point(lambda p: int(p * (shadow_alpha / 255.0)))
    shadow_shifted = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    tmp = Image.new("RGBA", (w, h), (8, 5, 16, 255))
    tmp.putalpha(sh_alpha)
    shadow_shifted.paste(tmp, (0, 5))

    out = Image.alpha_composite(shadow_shifted, outline)
    out = Image.alpha_composite(out, img)
    return out
