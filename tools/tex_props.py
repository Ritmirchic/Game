"""Prop, product, signage and vehicle textures."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from texlib import *
from tex_world import mixc

FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"

PALETTE = [
    (0.66, 0.12, 0.11), (0.10, 0.30, 0.55), (0.85, 0.62, 0.10), (0.15, 0.42, 0.18),
    (0.75, 0.35, 0.55), (0.90, 0.90, 0.86), (0.20, 0.20, 0.22), (0.55, 0.20, 0.10),
    (0.30, 0.55, 0.70), (0.80, 0.75, 0.35), (0.45, 0.25, 0.55), (0.70, 0.72, 0.70),
]


def _draw_text(draw, xy, text, size, fill, font=FONT_BOLD, anchor="mm"):
    try:
        f = ImageFont.truetype(font, size)
    except Exception:
        f = ImageFont.load_default()
    draw.text(xy, text, font=f, fill=fill, anchor=anchor)


def shelf_goods(u, v, size):
    """Rows of product packages facing the viewer (used on shelf front boxes)."""
    img = Image.new("RGB", (size, size), (30, 30, 32))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(11)
    rows = 5
    rh = size // rows
    for r in range(rows):
        x = 0
        while x < size:
            w = int(rng.integers(size // 14, size // 7))
            colr = PALETTE[int(rng.integers(0, len(PALETTE)))]
            c = tuple(int(c * 255) for c in colr)
            d.rectangle([x, r * rh + 2, x + w - 2, (r + 1) * rh - 3], fill=c)
            # label band
            d.rectangle([x + 2, r * rh + rh // 3, x + w - 4, r * rh + rh // 3 + rh // 3],
                        fill=tuple(min(255, int(cc * 1.25)) for cc in c))
            # text bars to suggest packaging copy
            for k in range(2):
                yy = r * rh + 5 + k * 6
                if yy < (r + 1) * rh - 4:
                    d.rectangle([x + 4, yy, x + 4 + int(w * rng.uniform(0.35, 0.7)), yy + 2],
                                fill=(240, 240, 235))
            x += w
    arr = np.asarray(img, np.float32) / 255.0
    n = fbm(u, v, 20, 3, 5)
    arr = arr * (0.85 + 0.25 * n[..., None])
    height = fbm(u, v, 60, 2, 7) * 0.15
    rough = 0.55 + 0.2 * n
    return dict(albedo=arr, height=height, rough=rough)


def freezer_goods(u, v, size):
    img = Image.new("RGB", (size, size), (40, 48, 55))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(23)
    for r in range(4):
        x = 0
        while x < size:
            w = int(rng.integers(size // 12, size // 6))
            h = int(size // 4 * rng.uniform(0.5, 0.95))
            base = PALETTE[int(rng.integers(0, len(PALETTE)))]
            c = tuple(int(cc * 255 * 0.8 + 40) for cc in base)
            d.rounded_rectangle([x + 2, r * (size // 4) + 4, x + w - 3, r * (size // 4) + h],
                                radius=4, fill=c)
            d.line([x + 4, r * (size // 4) + 10, x + w - 6, r * (size // 4) + 10],
                   fill=(235, 235, 245), width=2)
            x += w
    arr = np.asarray(img, np.float32) / 255.0
    haze = fbm(u, v, 8, 3, 9)
    arr = mixc(arr, rgb_img(0.75, 0.82, 0.88), 0.15 + 0.25 * haze)
    return dict(albedo=arr, height=fbm(u, v, 30, 2, 11) * 0.1, rough=0.35)


def produce_crate(u, v, size):
    img = Image.new("RGB", (size, size), (60, 45, 30))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(31)
    colors = [(0.75, 0.25, 0.12), (0.85, 0.65, 0.15), (0.25, 0.55, 0.20), (0.65, 0.15, 0.25)]
    for r in range(3):
        d.rectangle([0, r * size // 3, size, r * size // 3 + 4], fill=(45, 32, 20))
        x = 0
        while x < size:
            w = int(rng.integers(size // 10, size // 6))
            c = colors[int(rng.integers(0, len(colors)))]
            d.ellipse([x + 3, r * size // 3 + 6, x + w - 3, r * size // 3 + size // 3 - 6],
                      fill=tuple(int(cc * 255) for cc in c))
            x += w
    arr = np.asarray(img, np.float32) / 255.0
    n = fbm(u, v, 24, 4, 13)
    arr = arr * (0.8 + 0.4 * n[..., None])
    return dict(albedo=arr, height=fbm(u, v, 40, 3, 15) * 0.3, rough=0.6 + 0.2 * n)


def meat_pack(u, v, size):
    img = Image.new("RGB", (size, size), (200, 200, 205))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(37)
    for r in range(2):
        for c in range(4):
            x0 = c * size // 4 + 6
            y0 = r * size // 2 + 6
            d.rectangle([x0, y0, x0 + size // 4 - 12, y0 + size // 2 - 12], fill=(235, 235, 240))
            d.rectangle([x0 + 6, y0 + 6, x0 + size // 4 - 18, y0 + size // 2 - 18],
                        fill=(int(rng.integers(120, 170)), 30, 35))
    arr = np.asarray(img, np.float32) / 255.0
    n = fbm(u, v, 30, 3, 17)
    arr = arr * (0.85 + 0.3 * n[..., None])
    return dict(albedo=arr, height=n * 0.1, rough=0.3)


def bottle_shelf(u, v, size):
    img = Image.new("RGB", (size, size), (25, 28, 30))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(41)
    x = 0
    while x < size:
        w = int(rng.integers(size // 20, size // 12))
        h = int(size * rng.uniform(0.4, 0.8))
        colr = PALETTE[int(rng.integers(0, len(PALETTE)))]
        c = tuple(int(cc * 255) for cc in colr)
        d.rectangle([x, size - h, x + w - 3, size], fill=(c[0] // 3, c[1] // 3, c[2] // 3))
        d.rectangle([x, size - h, x + w - 3, size - h + 8], fill=c)
        d.rectangle([x + 2, size - h // 2, x + w - 5, size - h // 2 + h // 4], fill=(220, 220, 210))
        x += w
    arr = np.asarray(img, np.float32) / 255.0
    n = fbm(u, v, 20, 3, 19)
    arr = arr * (0.8 + 0.4 * n[..., None])
    return dict(albedo=arr, height=fbm(u, v, 50, 2, 21) * 0.2, rough=0.3)


def signage_aisle(u, v, size):
    img = Image.new("RGB", (size, size), (16, 60, 110))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, size, size], outline=(200, 205, 210), width=8)
    _draw_text(d, (size // 2, size // 2 - 20), "AISLE 4", size // 6, (240, 240, 240))
    _draw_text(d, (size // 2, size // 2 + 50), "CANNED GOODS", size // 12, (200, 215, 230), font=FONT_REG)
    arr = np.asarray(img, np.float32) / 255.0
    dirt = fbm(u, v, 6, 3, 23)
    arr = arr * (0.85 + 0.2 * dirt[..., None])
    return dict(albedo=arr, height=np.full_like(u, 0.5), rough=0.5)


def signage_store(u, v, size):
    w, h = size, size // 2
    img = Image.new("RGB", (w, h), (10, 45, 85))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=18, fill=(12, 56, 102))
    d.rounded_rectangle([6, 6, w - 7, h - 7], radius=14, outline=(230, 190, 70), width=6)
    _draw_text(d, (w // 2, h // 2 - 24), "SAVE-N-STOCK", h // 4, (245, 245, 240))
    _draw_text(d, (w // 2, h // 2 + 40), "OPEN 24 HOURS  *  FRESH FOODS  *  PHARMACY", h // 12,
               (230, 200, 120), font=FONT_REG)
    arr = np.asarray(img, np.float32) / 255.0
    return dict(albedo=arr, height=np.full_like(arr[..., 0], 0.5), rough=0.4)


def sign_exit(u, v, size):
    img = Image.new("RGB", (size, size), (10, 70, 30))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, size, size], fill=(10, 90, 40))
    _draw_text(d, (size // 2, size // 2 - 10), "EXIT", size // 4, (235, 255, 235))
    # arrow
    d.polygon([(size // 2, size - 30), (size // 2 - 40, size - 60), (size // 2 + 40, size - 60)],
              fill=(235, 255, 235))
    arr = np.asarray(img, np.float32) / 255.0
    return dict(albedo=arr, height=np.full_like(arr[..., 0], 0.5), rough=0.5)


def sign_warning(u, v, size):
    img = Image.new("RGB", (size, size), (200, 170, 20))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, size, size], fill=(215, 175, 20))
    d.rectangle([0, 0, size, size], outline=(25, 22, 18), width=10)
    # hazard stripes at bottom
    for i in range(-size, size * 2, 40):
        d.polygon([(i, size), (i + 20, size), (i + 20 + size, size - 60), (i + size, size - 60)],
                  fill=(30, 28, 24))
    _draw_text(d, (size // 2, size // 2 - 30), "CAUTION", size // 8, (25, 22, 18))
    _draw_text(d, (size // 2, size // 2 + 30), "ZOMBIES", size // 10, (25, 22, 18))
    arr = np.asarray(img, np.float32) / 255.0
    n = fbm(u, v, 12, 3, 27)
    arr = arr * (0.75 + 0.4 * n[..., None])
    return dict(albedo=arr, height=n * 0.2, rough=0.7)


def price_tags(u, v, size):
    img = Image.new("RGB", (size, size), (235, 232, 222))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(43)
    for i in range(4):
        d.rectangle([0, i * size // 4, size, i * size // 4 + 3], fill=(150, 150, 145))
        for k in range(4):
            x = 20 + k * size // 4
            _draw_text(d, (x, i * size // 4 + 26), f"${rng.integers(1, 9)}.{rng.integers(10, 99)}",
                       size // 16, (30, 30, 35))
    arr = np.asarray(img, np.float32) / 255.0
    return dict(albedo=arr, height=np.full_like(u, 0.5), rough=0.6)


def poster_a(u, v, size):
    img = Image.new("RGB", (size, size), (20, 20, 24))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, size, size], fill=(120, 18, 20))
    d.rectangle([12, 12, size - 12, size - 12], outline=(230, 210, 190), width=4)
    _draw_text(d, (size // 2, 80), "MEGA", size // 6, (240, 225, 200))
    _draw_text(d, (size // 2, 160), "SALE", size // 4, (250, 235, 120))
    _draw_text(d, (size // 2, 260), "EVERYTHING MUST GO", size // 14, (235, 220, 210), font=FONT_REG)
    for i in range(6):
        d.line([(40, 220 + i * 16), (size - 40, 220 + i * 16)], fill=(90, 14, 16), width=6)
    arr = np.asarray(img, np.float32) / 255.0
    u2, v2 = np.meshgrid(np.linspace(0,1,size), np.linspace(0,1,size))
    n = fbm(u2.astype(np.float32), v2.astype(np.float32), 6, 3, 29)
    arr = arr * (0.8 + 0.3 * n[..., None])
    return dict(albedo=arr, height=np.full_like(arr[..., 0], 0.5), rough=0.55)


def poster_b(u, v, size):
    img = Image.new("RGB", (size, size), (18, 26, 34))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, size, size], fill=(22, 38, 55))
    _draw_text(d, (size // 2, 70), "MISSING", size // 7, (235, 235, 240))
    d.rectangle([size // 4, 120, size * 3 // 4, 120 + size // 2], fill=(60, 62, 66))
    _draw_text(d, (size // 2, 145), "PHOTO", size // 12, (120, 122, 126), font=FONT_REG)
    _draw_text(d, (size // 2, 230), "HAVE YOU SEEN HIM?", size // 16, (225, 210, 180), font=FONT_REG)
    for i in range(5):
        d.line([(40, 260 + i * 14), (size - 40 - (60 if i == 4 else 0), 260 + i * 14)],
               fill=(150, 148, 140), width=3)
    arr = np.asarray(img, np.float32) / 255.0
    u2, v2 = np.meshgrid(np.linspace(0,1,size), np.linspace(0,1,size))
    n = fbm(u2.astype(np.float32), v2.astype(np.float32), 8, 3, 31)
    arr = arr * (0.8 + 0.3 * n[..., None])
    return dict(albedo=arr, height=np.full_like(arr[..., 0], 0.5), rough=0.6)


def store_directory(u, v, size):
    img = Image.new("RGB", (size, size), (28, 30, 34))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, size, size], fill=(24, 30, 38))
    _draw_text(d, (size // 2, 40), "STORE DIRECTORY", size // 14, (230, 200, 120))
    items = ["BAKERY", "DELI", "PRODUCE", "FROZEN", "PHARMACY", "RESTROOMS", "EXIT", "STAFF ONLY"]
    for i, it in enumerate(items):
        _draw_text(d, (size // 2, 90 + i * 50), it, size // 18, (210, 215, 220), font=FONT_REG)
        d.line([(30, 105 + i * 50), (size - 30, 105 + i * 50)], fill=(60, 66, 74), width=1)
    arr = np.asarray(img, np.float32) / 255.0
    return dict(albedo=arr, height=np.full_like(u, 0.5), rough=0.35)


def computer_screen(u, v, size):
    img = Image.new("RGB", (size, size), (8, 14, 20))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, size, size], fill=(8, 16, 24))
    # camera grid
    for i in range(4):
        x0 = (i % 2) * (size // 2)
        y0 = (i // 2) * (size // 2)
        d.rectangle([x0 + 4, y0 + 4, x0 + size // 2 - 4, y0 + size // 2 - 4],
                    outline=(40, 160, 90), width=2)
        rng = np.random.default_rng(50 + i)
        for k in range(60):
            px = x0 + int(rng.integers(8, size // 2 - 8))
            py = y0 + int(rng.integers(8, size // 2 - 8))
            g = int(rng.integers(20, 90))
            d.rectangle([px, py, px + 3, py + 5], fill=(g, g + 10, g))
        _draw_text(d, (x0 + 30, y0 + 16), f"CAM {i+1}", 12, (60, 200, 110), font=FONT_MONO, anchor="lm")
    _draw_text(d, (size // 2, size - 16), "REC 03:24:11", 14, (220, 60, 50), font=FONT_MONO)
    arr = np.asarray(img, np.float32) / 255.0
    # scanlines
    scan = 0.85 + 0.15 * (np.arange(size)[None, :] % 2)
    arr = arr * scan[..., None]
    return dict(albedo=arr, height=np.full_like(u, 0.5), rough=0.25)


def paper_sheet(u, v, size):
    img = Image.new("RGB", (size, size), (225, 222, 210))
    d = ImageDraw.Draw(img)
    for i in range(14):
        d.line([(30, 40 + i * 18), (size - 30 - (60 if i % 3 == 0 else 0), 40 + i * 18)],
               fill=(90, 92, 98), width=2)
    d.rectangle([30, 20, size // 2, 30], fill=(40, 42, 48))
    arr = np.asarray(img, np.float32) / 255.0
    n = fbm(u, v, 10, 3, 33)
    arr = arr * (0.9 + 0.2 * n[..., None])
    sp = smoothstep(0.6, 0.8, fbm(u, v, 4, 3, 35))
    arr = mixc(arr, rgb_img(0.55, 0.5, 0.42), sp * 0.5)
    return dict(albedo=arr, height=n * 0.05, rough=0.75)


def car_paint(base_rgb):
    def fn(u, v, size):
        n = fbm(u, v, 20, 4, 45)
        base = rgb_img(*base_rgb) * (0.85 + 0.25 * n[..., None])
        sc = scratches(u, v, size, 12, 47, 0.1, 0.6, strength=0.5) * 0.35
        base = base + sc[..., None] * 0.12
        rust = smoothstep(0.72, 0.85, fbm(u, v, 8, 4, 49)) * 0.7
        base = mixc(base, rgb_img(0.32, 0.18, 0.08), rust)
        dust = smoothstep(0.5, 0.85, fbm(u, v, 3, 4, 51)) * 0.5
        base = mixc(base, rgb_img(0.28, 0.26, 0.22), dust * 0.6)
        return dict(albedo=base, height=n * 0.1, rough=0.25 + 0.5 * rust + 0.3 * dust)
    return fn


def tire_rubber(u, v, size):
    n = fbm(u, v, 30, 4, 53)
    tread = 0.5 + 0.5 * np.sin(v * np.pi * 2 * 26)
    base = rgb_img(0.06 + 0.03 * n, 0.06 + 0.03 * n, 0.065 + 0.03 * n)
    base = base * (0.8 + 0.4 * tread[..., None])
    dust = smoothstep(0.4, 0.8, fbm(u, v, 5, 3, 55))
    base = mixc(base, rgb_img(0.25, 0.23, 0.2), dust * 0.5)
    return dict(albedo=base, height=tread * 0.5 + n * 0.2, rough=0.9)


def vehicle_glass(u, v, size):
    n = fbm(u, v, 14, 4, 57)
    dirt = smoothstep(0.35, 0.85, fbm(u, v, 4, 4, 59))
    base = rgb_img(0.18 + 0.1 * n, 0.20 + 0.1 * n, 0.22 + 0.1 * n)
    base = mixc(base, rgb_img(0.35, 0.33, 0.28), dirt * 0.7)
    alpha = 0.25 + 0.5 * dirt
    return dict(albedo=base, height=n * 0.05, rough=0.15 + 0.4 * dirt, alpha=alpha)


def chain_fence_pole(u, v, size):
    n = fbm(u, v, 10, 3, 61)
    base = rgb_img(0.30 + 0.08 * n, 0.31 + 0.08 * n, 0.30 + 0.08 * n)
    rust = smoothstep(0.55, 0.8, fbm(u, v, 12, 4, 63))
    base = mixc(base, rgb_img(0.32, 0.17, 0.08), rust)
    return dict(albedo=base, height=n * 0.2, rough=0.5 + 0.4 * rust)


def materials():
    return {
        "shelf_goods": (shelf_goods, 512, 1.0, 0.4, False),
        "freezer_goods": (freezer_goods, 512, 1.0, 0.3, False),
        "produce_crate": (produce_crate, 256, 1.0, 0.6, False),
        "meat_pack": (meat_pack, 256, 1.0, 0.3, False),
        "bottle_shelf": (bottle_shelf, 256, 1.0, 0.5, False),
        "signage_aisle": (signage_aisle, 256, 1.0, 0.2, False),
        "signage_store": (signage_store, 512, 1.0, 0.2, False),
        "sign_exit": (sign_exit, 128, 0.6, 0.1, False),
        "sign_warning": (sign_warning, 256, 1.0, 0.2, False),
        "price_tags": (price_tags, 256, 1.0, 0.2, False),
        "poster_a": (poster_a, 256, 1.0, 0.2, False),
        "poster_b": (poster_b, 256, 1.0, 0.2, False),
        "store_directory": (store_directory, 256, 1.0, 0.2, False),
        "computer_screen": (computer_screen, 256, 1.0, 0.1, False),
        "paper_sheet": (paper_sheet, 128, 0.4, 0.1, False),
        "car_paint_red": (car_paint((0.42, 0.06, 0.05)), 256, 2.0, 0.3, False),
        "car_paint_blue": (car_paint((0.08, 0.14, 0.30)), 256, 2.0, 0.3, False),
        "car_paint_white": (car_paint((0.65, 0.65, 0.63)), 256, 2.0, 0.3, False),
        "tire_rubber": (tire_rubber, 128, 1.0, 0.6, False),
        "vehicle_glass": (vehicle_glass, 128, 1.5, 0.1, True),
        "fence_pole": (chain_fence_pole, 128, 1.5, 0.4, False),
    }
