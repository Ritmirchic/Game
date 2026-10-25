#!/usr/bin/env python3
"""
THE LAST NIGHT — procedural texture & UI art generator.
Generates original PBR-style albedo textures and UI assets with Pillow.
All outputs are written to ../assets/textures. No external assets used.
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")
os.makedirs(OUT, exist_ok=True)
RNG = random.Random(20261025)


def img(w, h, color=(0, 0, 0, 255)):
    return Image.new("RGBA", (w, h), color)


def noise_layer(im, amount=24, mono=True):
    w, h = im.size
    px = im.load()
    for y in range(h):
        for x in range(w):
            n = RNG.randint(-amount, amount)
            r, g, b, a = px[x, y]
            if mono:
                r = max(0, min(255, r + n)); g = max(0, min(255, g + n)); b = max(0, min(255, b + n))
            else:
                r = max(0, min(255, r + RNG.randint(-amount, amount)))
                g = max(0, min(255, g + RNG.randint(-amount, amount)))
                b = max(0, min(255, b + RNG.randint(-amount, amount)))
            px[x, y] = (r, g, b, a)
    return im


def blotches(im, count, color, radius_range, alpha_range, blur=6):
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for _ in range(count):
        x = RNG.randint(0, im.size[0]); y = RNG.randint(0, im.size[1])
        r = RNG.randint(*radius_range); a = RNG.randint(*alpha_range)
        c = color + (a,)
        d.ellipse([x - r, y - r, x + r, y + r], fill=c)
    layer = layer.filter(ImageFilter.GaussianBlur(blur))
    im.alpha_composite(layer)
    return im


def cracks(im, count, color=(20, 18, 16, 200), width=1):
    d = ImageDraw.Draw(im)
    w, h = im.size
    for _ in range(count):
        x = RNG.randint(0, w); y = RNG.randint(0, h)
        pts = [(x, y)]
        for _ in range(RNG.randint(3, 7)):
            x += RNG.randint(-40, 40); y += RNG.randint(10, 45)
            pts.append((x, y))
        d.line(pts, fill=color, width=width)
    return im


def save(im, name):
    im.save(os.path.join(OUT, name))
    print("texture:", name, im.size)


def font(size, bold=True):
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    ]
    for c in candidates:
        if os.path.exists(c):
            return ImageFont.truetype(c, size)
    return ImageFont.load_default()


# ---------------------------------------------------------------- surfaces
def floor_tile():
    s = 512; t = 128
    im = img(s, s, (201, 199, 189, 255))
    d = ImageDraw.Draw(im)
    for gy in range(4):
        for gx in range(4):
            v = RNG.randint(-12, 12)
            d.rectangle([gx * t + 3, gy * t + 3, (gx + 1) * t - 3, (gy + 1) * t - 3],
                        fill=(201 + v, 199 + v, 189 + v, 255))
    d.rectangle([0, 0, s - 1, s - 1], outline=(90, 88, 82, 255), width=4)
    for i in range(1, 4):
        d.line([(i * t, 0), (i * t, s)], fill=(90, 88, 82, 255), width=4)
        d.line([(0, i * t), (s, i * t)], fill=(90, 88, 82, 255), width=4)
    noise_layer(im, 14)
    blotches(im, 26, (70, 66, 58), (10, 55), (10, 38))
    blotches(im, 8, (120, 90, 60), (12, 40), (12, 30))
    cracks(im, 5)
    save(im, "floor_tile.png")


def floor_concrete():
    im = img(512, 512, (128, 128, 124, 255))
    noise_layer(im, 22)
    blotches(im, 30, (60, 60, 58), (15, 70), (14, 40))
    blotches(im, 10, (150, 148, 140), (20, 80), (10, 26))
    cracks(im, 9, width=2)
    d = ImageDraw.Draw(im)
    d.line([(0, 256), (512, 256)], fill=(70, 70, 68, 160), width=3)
    save(im, "floor_concrete.png")


def wall_paint():
    im = img(512, 512, (158, 168, 158, 255))
    noise_layer(im, 12)
    blotches(im, 24, (90, 96, 88), (18, 80), (12, 34))
    blotches(im, 12, (60, 64, 58), (10, 45), (14, 30))
    # grime rising from bottom, faded stripe at dado height
    grime = Image.new("RGBA", im.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(grime)
    for y in range(380, 512):
        a = int((y - 380) / 132 * 70)
        gd.line([(0, y), (512, y)], fill=(50, 52, 46, a))
    im.alpha_composite(grime)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 330, 512, 342], fill=(110, 118, 108, 255))
    cracks(im, 4, width=1)
    save(im, "wall_paint.png")


def wall_brick():
    im = img(512, 512, (60, 56, 52, 255))
    d = ImageDraw.Draw(im)
    bh, bw = 32, 76
    for row in range(16):
        off = -(bw // 2) if row % 2 else 0
        for col in range(8):
            x = col * bw + off; y = row * bh
            v = RNG.randint(-18, 18)
            base = (128 + v, 74 + v // 2, 58 + v // 2, 255)
            d.rectangle([x + 2, y + 2, x + bw - 2, y + bh - 2], fill=base)
    noise_layer(im, 16)
    blotches(im, 18, (40, 36, 34), (14, 55), (16, 44))
    save(im, "wall_brick.png")


def ceiling():
    im = img(256, 256, (214, 212, 204, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 255, 255], outline=(120, 118, 112), width=3)
    for x in range(8, 250, 10):
        for y in range(8, 250, 10):
            d.point((x, y), fill=(170, 168, 160))
    noise_layer(im, 8)
    blotches(im, 6, (120, 116, 100), (8, 30), (10, 26))
    save(im, "ceiling.png")


def asphalt():
    im = img(512, 512, (48, 49, 52, 255))
    noise_layer(im, 26)
    blotches(im, 22, (20, 20, 22), (20, 90), (18, 46))
    blotches(im, 8, (90, 84, 70), (8, 34), (16, 34))
    cracks(im, 12, color=(15, 15, 16, 220), width=2)
    save(im, "asphalt.png")


def wood_planks():
    im = img(512, 512, (150, 104, 62, 255))
    d = ImageDraw.Draw(im)
    ph = 64
    for row in range(8):
        v = RNG.randint(-18, 16)
        y = row * ph
        d.rectangle([0, y, 512, y + ph], fill=(150 + v, 104 + v, 62 + v, 255))
        d.line([(0, y), (512, y)], fill=(86, 58, 34, 255), width=3)
        for _ in range(9):
            gy = y + RNG.randint(6, ph - 6)
            d.line([(0, gy), (512, gy + RNG.randint(-4, 4))], fill=(110, 74, 42, 120), width=1)
        for nx in (40, 256, 470):
            d.ellipse([nx - 3, y + 8, nx + 3, y + 14], fill=(70, 46, 26, 255))
    noise_layer(im, 12)
    blotches(im, 10, (70, 48, 28), (10, 40), (12, 30))
    save(im, "wood_planks.png")


def metal(base, name, streak=(255, 255, 255)):
    im = img(256, 256, base + (255,))
    d = ImageDraw.Draw(im)
    for y in range(0, 256, 2):
        v = RNG.randint(-10, 10)
        d.line([(0, y), (256, y)], fill=tuple(max(0, min(255, c + v)) for c in base) + (255,))
    for _ in range(40):
        y = RNG.randint(0, 256)
        d.line([(RNG.randint(0, 60), y), (RNG.randint(180, 256), y)], fill=streak + (40,), width=1)
    blotches(im, 8, (40, 40, 42), (6, 26), (14, 36))
    save(im, name)


def cardboard():
    im = img(256, 256, (168, 130, 84, 255))
    d = ImageDraw.Draw(im)
    for x in range(0, 256, 8):
        d.line([(x, 0), (x, 256)], fill=(150, 114, 72, 90), width=1)
    d.rectangle([110, 0, 146, 255], fill=(196, 162, 118, 255))
    noise_layer(im, 10)
    blotches(im, 6, (110, 84, 52), (10, 34), (12, 30))
    save(im, "cardboard.png")


def products():
    im = img(512, 256, (60, 60, 64, 255))
    d = ImageDraw.Draw(im)
    colors = [(190, 60, 50), (230, 180, 40), (60, 120, 190), (90, 160, 80),
              (200, 110, 50), (150, 90, 170), (220, 220, 210), (70, 170, 170)]
    for row in range(4):
        x = 6
        while x < 500:
            w = RNG.randint(20, 34); h = RNG.randint(44, 56)
            c = RNG.choice(colors)
            y = row * 64 + 6
            d.rectangle([x, y, x + w, y + h], fill=c + (255,))
            d.rectangle([x + 3, y + 6, x + w - 3, y + 18], fill=(255, 255, 255, 180))
            d.rectangle([x, y + h - 8, x + w, y + h], fill=(0, 0, 0, 90))
            x += w + RNG.randint(4, 10)
    noise_layer(im, 8)
    save(im, "products.png")


def hazard():
    im = img(256, 64, (20, 20, 20, 255))
    d = ImageDraw.Draw(im)
    for x in range(-64, 320, 48):
        d.polygon([(x, 64), (x + 24, 64), (x + 88, 0), (x + 64, 0)], fill=(225, 180, 30, 255))
    save(im, "hazard.png")


def blood_splat():
    im = img(256, 256, (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = 128, 128
    d.ellipse([70, 70, 186, 186], fill=(110, 12, 10, 235))
    for _ in range(26):
        a = RNG.uniform(0, math.tau); r = RNG.randint(50, 118)
        x = cx + math.cos(a) * r; y = cy + math.sin(a) * r
        rr = RNG.randint(4, 18)
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(120, 14, 12, RNG.randint(140, 220)))
    for _ in range(6):
        x = RNG.randint(40, 216)
        d.line([(x, 60), (x + RNG.randint(-8, 8), RNG.randint(190, 240))], fill=(110, 12, 10, 160), width=RNG.randint(2, 5))
    im = im.filter(ImageFilter.GaussianBlur(1.2))
    save(im, "blood_splat.png")


def door_metal():
    im = img(256, 512, (96, 108, 120, 255))
    d = ImageDraw.Draw(im)
    noise_layer(im, 10)
    for y in (24, 280):
        d.rectangle([18, y, 238, y + 200], outline=(60, 70, 80, 255), width=4)
    d.rectangle([200, 250, 226, 286], fill=(50, 54, 58, 255))
    blotches(im, 10, (140, 90, 50), (8, 30), (14, 40))
    save(im, "door_metal.png")


def sign(text, name, bg=(30, 60, 46), fg=(220, 230, 210)):
    im = img(512, 128, bg + (255,))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 511, 127], outline=(180, 190, 170, 255), width=6)
    f = font(54)
    bbox = d.textbbox((0, 0), text, font=f)
    d.text(((512 - (bbox[2] - bbox[0])) / 2, (128 - (bbox[3] - bbox[1])) / 2 - 6), text, font=f, fill=fg + (255,))
    noise_layer(im, 6)
    save(im, name)


# ---------------------------------------------------------------- UI art
def crosshair():
    im = img(64, 64, (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = (235, 235, 230, 235)
    d.ellipse([29, 29, 34, 34], fill=c)
    for a in (0, 90, 180, 270):
        rad = math.radians(a)
        x1 = 32 + math.cos(rad) * 10; y1 = 32 + math.sin(rad) * 10
        x2 = 32 + math.cos(rad) * 22; y2 = 32 + math.sin(rad) * 22
        d.line([(x1, y1), (x2, y2)], fill=c, width=3)
    save(im, "crosshair.png")


def ui_panel():
    im = img(32, 32, (16, 18, 22, 235))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 31, 31], outline=(120, 40, 36, 255), width=2)
    save(im, "ui_panel.png")
    for state, col in (("normal", (38, 42, 48, 255)), ("hover", (96, 30, 28, 255)), ("pressed", (140, 44, 40, 255))):
        b = img(32, 32, col)
        bd = ImageDraw.Draw(b)
        bd.rectangle([0, 0, 31, 31], outline=(170, 150, 120, 255), width=2)
        save(b, f"button_{state}.png")


def icon():
    im = img(256, 256, (12, 13, 16, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 255, 255], fill=(18, 20, 26, 255))
    d.rectangle([10, 10, 245, 245], outline=(120, 36, 32, 255), width=6)
    # stylized skull
    d.ellipse([66, 52, 190, 178], fill=(214, 210, 198, 255))
    d.rectangle([92, 168, 164, 212], fill=(214, 210, 198, 255))
    d.ellipse([92, 96, 128, 134], fill=(20, 18, 18, 255))
    d.ellipse([128, 96, 164, 134], fill=(20, 18, 18, 255))
    d.polygon([(128, 128), (118, 152), (138, 152)], fill=(30, 24, 22, 255))
    for x in (104, 122, 140, 156):
        d.line([(x, 178), (x, 206)], fill=(60, 56, 50, 255), width=4)
    # blood drip
    d.polygon([(190, 60), (206, 60), (200, 110), (204, 140), (196, 150), (190, 138)], fill=(140, 20, 18, 255))
    save(im, "icon.png")


def logo():
    im = img(1024, 512, (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    f1 = font(118); f2 = font(150)
    title1 = "THE LAST"
    bbox = d.textbbox((0, 0), title1, font=f1)
    d.text(((1024 - (bbox[2] - bbox[0])) / 2, 70), title1, font=f1, fill=(220, 220, 214, 255))
    title2 = "NIGHT"
    bbox = d.textbbox((0, 0), title2, font=f2)
    x = (1024 - (bbox[2] - bbox[0])) / 2
    d.text((x, 200), title2, font=f2, fill=(168, 26, 24, 255))
    d.line([(x, 372), (x + (bbox[2] - bbox[0]), 372)], fill=(180, 160, 130, 255), width=4)
    f3 = font(34, bold=False)
    sub = "SURVIVE THE TEN NIGHTS"
    bbox = d.textbbox((0, 0), sub, font=f3)
    d.text(((1024 - (bbox[2] - bbox[0])) / 2, 396), sub, font=f3, fill=(190, 186, 176, 255))
    save(im, "logo.png")


def loading():
    im = img(1920, 1080, (10, 11, 14, 255))
    # faint aisle perspective
    d = ImageDraw.Draw(im)
    for i in range(9):
        t = i / 8.0
        x = 960 + (t - 0.5) * 1500
        d.polygon([(960, 300), (x, 1080), (x + 60, 1080)], fill=(24 + i, 26 + i, 30 + i, 255))
    for y in range(360, 1080, 120):
        d.line([(300, y), (1620, y)], fill=(50, 52, 58, 120), width=2)
    blotches(im, 14, (90, 20, 18), (40, 160), (10, 26), blur=18)
    logo_im = Image.open(os.path.join(OUT, "logo.png")).resize((640, 320))
    im.alpha_composite(logo_im, (640, 200))
    f = font(36, bold=False)
    d.text((960 - 90, 940), "LOADING", font=f, fill=(200, 196, 186, 255))
    im = im.convert("RGB")
    im.save(os.path.join(OUT, "loading.jpg"), quality=88)
    print("texture: loading.jpg", im.size)


def weapon_icons():
    specs = {
        "w_bat": ("bat", (190, 150, 90)), "w_axe": ("axe", (170, 170, 175)),
        "w_pistol": ("pistol", (150, 150, 155)), "w_shotgun": ("shotgun", (140, 110, 70)),
        "w_rifle": ("rifle", (90, 96, 104)), "w_hunting": ("hunting", (120, 90, 60)),
        "w_crossbow": ("crossbow", (150, 120, 80)),
    }
    for name, (kind, col) in specs.items():
        im = img(128, 128, (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        c = col + (255,)
        if kind == "bat":
            d.line([(34, 98), (96, 34)], fill=c, width=12)
        elif kind == "axe":
            d.line([(44, 100), (88, 40)], fill=(120, 84, 50, 255), width=9)
            d.polygon([(84, 30), (108, 44), (96, 66), (76, 56)], fill=c)
        elif kind == "pistol":
            d.rounded_rectangle([34, 52, 92, 70], 4, fill=c)
            d.rectangle([52, 68, 70, 96], fill=c)
        elif kind == "shotgun":
            d.line([(24, 70), (104, 60)], fill=c, width=10)
            d.rectangle([24, 74, 60, 88], fill=(90, 60, 36, 255))
        elif kind == "rifle":
            d.line([(20, 64), (108, 64)], fill=c, width=9)
            d.rectangle([44, 70, 62, 96], fill=(60, 64, 70, 255))
            d.line([(86, 56), (108, 50)], fill=c, width=5)
        elif kind == "hunting":
            d.line([(22, 66), (106, 60)], fill=c, width=8)
            d.line([(30, 78), (70, 92)], fill=(90, 60, 36, 255), width=8)
            d.ellipse([96, 52, 112, 68], outline=c, width=4)
        elif kind == "crossbow":
            d.arc([30, 26, 96, 102], 270, 90, fill=c, width=7)
            d.line([(62, 28), (62, 102)], fill=(120, 84, 50, 255), width=7)
            d.line([(36, 64), (100, 64)], fill=(220, 220, 210, 255), width=2)
        save(im, f"icon_{name}.png")


def zombie_icons():
    specs = {"z_walker": (90, 120, 84), "z_runner": (140, 70, 60), "z_crawler": (110, 100, 90),
             "z_brute": (70, 84, 70), "z_screamer": (150, 120, 70)}
    for name, col in specs.items():
        im = img(96, 96, (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        d.ellipse([22, 14, 74, 70], fill=col + (255,))
        d.ellipse([32, 34, 44, 46], fill=(20, 20, 20, 255))
        d.ellipse([52, 34, 64, 46], fill=(20, 20, 20, 255))
        d.rectangle([30, 66, 66, 88], fill=tuple(max(0, c - 25) for c in col) + (255,))
        save(im, f"icon_{name}.png")


if __name__ == "__main__":
    floor_tile(); floor_concrete(); wall_paint(); wall_brick(); ceiling(); asphalt()
    wood_planks()
    metal((132, 136, 142), "metal_shelf.png")
    metal((52, 56, 62), "metal_dark.png")
    metal((176, 180, 184), "fridge_metal.png")
    cardboard(); products(); hazard(); blood_splat(); door_metal()
    sign("CHECKOUT", "sign_checkout.png", bg=(28, 70, 110))
    sign("STORAGE", "sign_storage.png")
    sign("STAFF ONLY", "sign_staff.png", bg=(90, 60, 30))
    sign("EXIT", "sign_exit.png", bg=(90, 30, 26))
    sign("ELECTRICAL", "sign_electrical.png", bg=(120, 90, 20), fg=(30, 26, 18))
    sign("SECURITY", "sign_security.png", bg=(30, 40, 60))
    crosshair(); ui_panel(); icon(); logo(); loading(); weapon_icons(); zombie_icons()
    print("ALL TEXTURES DONE")
