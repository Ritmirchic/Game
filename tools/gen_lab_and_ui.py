#!/usr/bin/env python3
"""
Generates ALL Laboratory Layered Backgrounds, Animated Equipment Sprites,
Custom Styled UI Chrome, Icons, Upgrade Illustrations, Particle Textures,
Application .ICO, Splash/Menu Art, and NSIS Installer BMPs for ALCHEMY CHAOS.
"""

import math
import os
import random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from art_helpers import (
    make_wood_texture,
    make_parchment_texture,
    make_dark_slate_panel,
    add_radial_glow,
    draw_shaded_sphere,
    add_outline_and_shadow,
    fbm_2d,
    mix_color,
)


def draw_copper_pipe(d: ImageDraw.ImageDraw, x0, y0, x1, y1, thickness=18, brass=False):
    c_dark = (88, 52, 24, 255) if not brass else (96, 68, 22, 255)
    c_main = (188, 102, 54, 255) if not brass else (206, 162, 72, 255)
    c_hi = (252, 186, 138, 235) if not brass else (255, 234, 158, 240)
    d.line([(x0, y0), (x1, y1)], fill=c_dark, width=thickness + 4)
    d.line([(x0, y0), (x1, y1)], fill=c_main, width=thickness)
    dx, dy = x1 - x0, y1 - y0
    ln = max(1.0, math.hypot(dx, dy))
    nx, ny = -dy / ln * (thickness * 0.22), dx / ln * (thickness * 0.22)
    d.line([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny)], fill=c_hi, width=max(2, thickness // 4))


def draw_pipe_flange(d: ImageDraw.ImageDraw, cx, cy, rx=16, ry=7):
    d.rounded_rectangle([cx - rx, cy - ry, cx + rx, cy + ry], radius=4, fill=(198, 148, 66, 255), outline=(78, 48, 18, 255), width=2)
    d.ellipse([cx - rx + 4, cy - 2, cx - rx + 8, cy + 2], fill=(255, 232, 168, 255))
    d.ellipse([cx + rx - 8, cy - 2, cx + rx - 4, cy + 2], fill=(255, 232, 168, 255))


def draw_decorative_bottle(canvas: Image.Image, cx, bottom_y, w, h, liq_rgb, shape_idx=0):
    d = ImageDraw.Draw(canvas)
    top_y = bottom_y - h
    body_top = bottom_y - int(h * 0.72)
    liq_hi = tuple(min(255, c + 65) for c in liq_rgb)
    if shape_idx % 3 == 0:
        # Round apothecary bulb
        d.ellipse([cx - w // 2, body_top, cx + w // 2, bottom_y], fill=(190, 230, 248, 80))
        d.pieslice([cx - w // 2 + 4, body_top + 4, cx + w // 2 - 4, bottom_y - 4], 5, 175, fill=(*liq_rgb, 235))
        d.ellipse([cx - w // 2 + 6, (body_top + bottom_y) // 2 - 5, cx + w // 2 - 6, (body_top + bottom_y) // 2 + 5], fill=(*liq_hi, 230))
        d.ellipse([cx - w // 2, body_top, cx + w // 2, bottom_y], outline=(220, 245, 255, 220), width=2)
    elif shape_idx % 3 == 1:
        # Tall square bottle
        d.rounded_rectangle([cx - w // 2, body_top, cx + w // 2, bottom_y], radius=7, fill=(190, 230, 248, 80), outline=(220, 245, 255, 220), width=2)
        d.rounded_rectangle([cx - w // 2 + 4, body_top + int((bottom_y - body_top) * 0.35), cx + w // 2 - 4, bottom_y - 4], radius=5, fill=(*liq_rgb, 235))
        d.rectangle([cx - w // 2 + 6, body_top + int((bottom_y - body_top) * 0.45), cx + w // 2 - 6, body_top + int((bottom_y - body_top) * 0.65)], fill=(232, 214, 175, 225))
    else:
        # Conical flask
        pts = [(cx - w // 5, body_top), (cx + w // 5, body_top), (cx + w // 2, bottom_y), (cx - w // 2, bottom_y)]
        d.polygon(pts, fill=(190, 230, 248, 80))
        lpts = [(cx - w // 3, (body_top + bottom_y) // 2), (cx + w // 3, (body_top + bottom_y) // 2), (cx + w // 2 - 4, bottom_y - 3), (cx - w // 2 + 4, bottom_y - 3)]
        d.polygon(lpts, fill=(*liq_rgb, 235))
        d.polygon(pts, outline=(220, 245, 255, 220), width=2)

    nw = max(8, w // 4)
    d.rectangle([cx - nw // 2, top_y + 8, cx + nw // 2, body_top + 4], fill=(205, 238, 252, 110), outline=(220, 245, 255, 210), width=2)
    d.rectangle([cx - nw // 2 + 1, top_y, cx + nw // 2 - 1, top_y + 10], fill=(162, 106, 56, 255))


def generate_laboratory_layers(lab_dir: str, fonts_dir: str):
    W, H = 1920, 1080
    rng = random.Random(2026)

    # 1. lab_bg_stone_arch.png — Vaulted stone masonry wall + Gothic arched window + warm sconces
    n = fbm_2d(H, W, base_scale=64, octaves=4, seed=101)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # Warm vignette centered around stage center (1156, 520)
    dist = np.sqrt(((xx - 1156.0) / 920.0) ** 2 + ((yy - 500.0) / 680.0) ** 2)
    vig = np.clip(1.12 - dist * 0.58, 0.35, 1.0)
    base_col = np.zeros((H, W, 4), dtype=np.uint8)
    stone_light = np.array([62, 52, 68], dtype=np.float32)
    stone_dark = np.array([24, 19, 30], dtype=np.float32)
    for c in range(3):
        val = (stone_dark[c] * (1.0 - n) + stone_light[c] * n) * vig
        base_col[:, :, c] = np.clip(val, 0, 255).astype(np.uint8)
    base_col[:, :, 3] = 255
    bg = Image.fromarray(base_col, "RGBA")
    bd = ImageDraw.Draw(bg)

    # Subtle stone ashlar brick courses & mortar joints
    brick_h = 46
    for row in range(0, H // brick_h + 1):
        y = row * brick_h
        bd.line([(0, y), (W, y)], fill=(16, 12, 22, 115), width=2)
        offset = (row % 2) * 52
        for x in range(offset, W, 104):
            shade = rng.randint(-8, 10)
            if shade > 3:
                bd.rectangle([x + 3, y + 3, x + 100, y + brick_h - 3], fill=(78, 66, 86, 22))
            bd.line([(x, y), (x, y + brick_h)], fill=(16, 12, 22, 110), width=2)

    # Vaulted stone ribs framing the laboratory stage (x=408..1904)
    bd.arc([360, -260, 1950, 920], 185, 355, fill=(44, 36, 52, 220), width=28)
    bd.arc([360, -260, 1950, 920], 185, 355, fill=(92, 78, 104, 120), width=6)

    # Gothic Arched Leaded-Glass Window centered at x=1156, y=95..445
    wx0, wy0, wx1, wy1 = 1046, 96, 1266, 444
    # Outer stone surround
    bd.pieslice([wx0 - 18, wy0 - 18, wx1 + 18, wy0 + 202], 180, 360, fill=(82, 72, 92, 255), outline=(30, 24, 38, 255), width=4)
    bd.rectangle([wx0 - 18, wy0 + 92, wx1 + 18, wy1 + 14], fill=(82, 72, 92, 255), outline=(30, 24, 38, 255), width=4)
    # Night sky inside window (clipped cleanly to exact arched window mask!)
    wmask = Image.new("L", (W, H), 0)
    wmd = ImageDraw.Draw(wmask)
    wmd.pieslice([wx0, wy0, wx1, wy0 + 184], 180, 360, fill=255)
    wmd.rectangle([wx0, wy0 + 92, wx1, wy1], fill=255)

    sky = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    skd = ImageDraw.Draw(sky)
    skd.rectangle([wx0, wy0, wx1, wy1], fill=(20, 40, 82, 255))
    # Crescent moon & stars visible through window
    skd.ellipse([1166, 134, 1222, 190], fill=(255, 244, 196, 255))
    skd.ellipse([1182, 128, 1234, 180], fill=(20, 40, 82, 255))
    for _ in range(28):
        sx = rng.randint(wx0 + 14, wx1 - 14)
        sy = rng.randint(wy0 + 28, wy1 - 16)
        skd.ellipse([sx - 1, sy - 1, sx + 2, sy + 2], fill=(235, 246, 255, 210))
    # Diamond leaded glass mullions & stained rose medallion
    for dline in range(-260, 360, 38):
        skd.line([(wx0, wy0 + 90 + dline), (wx1, wy0 + 90 + dline + 140)], fill=(34, 30, 44, 165), width=2)
        skd.line([(wx0, wy0 + 90 + dline + 140), (wx1, wy0 + 90 + dline)], fill=(34, 30, 44, 165), width=2)
    skd.line([(1156, wy0), (1156, wy1)], fill=(38, 34, 48, 245), width=6)
    skd.line([(wx0, 275), (wx1, 275)], fill=(38, 34, 48, 245), width=6)
    skd.ellipse([1120, 168, 1192, 240], outline=(218, 172, 82, 210), width=4)
    from PIL import ImageChops
    sky.putalpha(ImageChops.multiply(sky.split()[3], wmask))
    bg = Image.alpha_composite(bg, sky)
    bg = add_radial_glow(bg, 1156, 245, 260, (112, 168, 255), alpha_max=65)
    # Warm wall sconce glows left & right of window
    for scx in (825, 1465):
        bg = add_radial_glow(bg, scx, 260, 220, (255, 166, 68), alpha_max=85)
        scd = ImageDraw.Draw(bg)
        scd.polygon([(scx - 10, 270), (scx + 10, 270), (scx + 5, 318), (scx - 5, 318)], fill=(72, 58, 52, 255))
        scd.ellipse([scx - 12, 242, scx + 12, 274], fill=(255, 218, 118, 245))

    bg.save(os.path.join(lab_dir, "lab_bg_stone_arch.png"))

    # 2. lab_furniture_back.png — Cabinets, Wooden Shelves, Bottles, Pipes, Condenser, Research Board, Books
    furn = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    fd = ImageDraw.Draw(furn)

    # --- Left Wooden Storage Shelves (x=438..840, two tiers: y=215 & y=375) ---
    shelf_wood = make_wood_texture(410, 22, base_rgb=(98, 58, 34), dark_rgb=(46, 24, 12), seed=21)
    for sy in (225, 385):
        # Iron/wood brackets
        for bx in (468, 806):
            fd.polygon([(bx - 10, sy + 20), (bx + 10, sy + 20), (bx - 4, sy + 62)], fill=(54, 42, 38, 255))
        furn.alpha_composite(shelf_wood, (436, sy))
        fd = ImageDraw.Draw(furn)
        fd.rectangle([436, sy, 845, sy + 22], outline=(38, 20, 10, 240), width=2)

    # Static background bottles, herb bundles, and jars on upper & lower shelves
    shelf_colors = [
        (255, 92, 64), (72, 210, 255), (112, 232, 96), (232, 184, 52),
        (188, 96, 255), (255, 134, 198), (76, 238, 192), (255, 168, 52),
    ]
    for i, bx in enumerate(range(462, 825, 52)):
        draw_decorative_bottle(furn, bx, 225, 32, rng.randint(58, 86), shelf_colors[i % len(shelf_colors)], i)
    # Leave gaps on lower shelf for 3 animated shelf bottle sprites + put books & jars between
    for i, bx in enumerate([465, 575, 695, 805]):
        draw_decorative_bottle(furn, bx, 385, 34, rng.randint(64, 88), shelf_colors[(i + 3) % len(shelf_colors)], i + 1)

    # --- Lower-Left Carved Walnut Apothecary Cabinet (x=438..795, y=475..735) ---
    cab_wood = make_wood_texture(356, 255, base_rgb=(82, 46, 26), dark_rgb=(36, 18, 10), grain_horizontal=False, seed=44)
    furn.alpha_composite(cab_wood, (438, 480))
    fd = ImageDraw.Draw(furn)
    fd.rectangle([438, 480, 794, 735], outline=(30, 15, 8, 255), width=4)
    fd.rectangle([430, 470, 802, 486], fill=(96, 56, 32, 255), outline=(32, 16, 8, 255), width=3)
    # 3x3 Apothecary drawers with brass label plates & pulls
    for r in range(3):
        for c in range(3):
            dx0 = 452 + c * 112
            dy0 = 498 + r * 72
            fd.rounded_rectangle([dx0, dy0, dx0 + 100, dy0 + 60], radius=5, fill=(66, 36, 20, 255), outline=(34, 16, 8, 255), width=2)
            fd.rectangle([dx0 + 30, dy0 + 14, dx0 + 70, dy0 + 28], fill=(226, 208, 168, 235), outline=(142, 104, 48, 255))
            fd.ellipse([dx0 + 44, dy0 + 34, dx0 + 56, dy0 + 46], fill=(214, 168, 76, 255), outline=(88, 56, 20, 255))

    # --- Copper & Brass Distillation Pipes + Spiral Glass Condenser (x=865..1025 & x=1295..1445) ---
    # Left pipe network feeding toward mixing station
    draw_copper_pipe(fd, 895, 74, 895, 310, 18)
    draw_copper_pipe(fd, 895, 310, 985, 310, 18)
    draw_copper_pipe(fd, 985, 310, 985, 650, 16)
    draw_pipe_flange(fd, 895, 160, 15, 6)
    draw_pipe_flange(fd, 985, 440, 14, 6)
    # Spiral Liebig glass condenser on right (x=1355, y=190..495)
    draw_copper_pipe(fd, 1355, 74, 1355, 195, 16, brass=True)
    draw_copper_pipe(fd, 1355, 485, 1355, 665, 16, brass=True)
    fd.rounded_rectangle([1322, 195, 1388, 485], radius=22, fill=(168, 228, 252, 65), outline=(215, 245, 255, 215), width=3)
    for cy_coil in range(215, 465, 28):
        fd.arc([1330, cy_coil, 1380, cy_coil + 24], 0, 360, fill=(104, 236, 214, 210), width=5)
    draw_pipe_flange(fd, 1355, 195, 24, 8)
    draw_pipe_flange(fd, 1355, 485, 24, 8)

    # --- Research Board (x=1470..1868, y=132..455) ---
    board_frame = make_wood_texture(398, 324, base_rgb=(96, 58, 34), dark_rgb=(42, 22, 12), seed=63)
    furn.alpha_composite(board_frame, (1470, 132))
    fd = ImageDraw.Draw(furn)
    fd.rectangle([1470, 132, 1868, 456], outline=(30, 15, 8, 255), width=4)
    # Cork interior
    cork = make_wood_texture(366, 292, base_rgb=(138, 94, 58), dark_rgb=(98, 64, 38), seed=77)
    furn.alpha_composite(cork, (1486, 148))
    fd = ImageDraw.Draw(furn)
    fd.rectangle([1486, 148, 1852, 440], outline=(52, 28, 14, 255), width=3)

    # Pinned parchment research notes, sacred geometry ouroboros diagram, and red string links
    notes = [
        (1504, 168, 108, 126),
        (1636, 162, 128, 104),
        (1778, 174, 62, 86),
        (1518, 312, 134, 108),
        (1678, 286, 154, 134),
    ]
    pins = []
    for nx0, ny0, nw, nh in notes:
        p = make_parchment_texture(nw, nh, seed=nx0 + ny0)
        furn.alpha_composite(p, (nx0, ny0))
        fd = ImageDraw.Draw(furn)
        fd.rectangle([nx0, ny0, nx0 + nw, ny0 + nh], outline=(112, 84, 52, 220), width=2)
        px, py = nx0 + nw // 2, ny0 + 10
        pins.append((px, py))
    # Draw diagrams on the notes
    fd.ellipse([1526, 202, 1590, 266], outline=(128, 52, 38, 210), width=2)
    fd.polygon([(1558, 204), (1586, 256), (1530, 256)], outline=(62, 42, 28, 210), width=2)
    fd.ellipse([1716, 314, 1796, 394], outline=(62, 88, 148, 210), width=3)
    fd.polygon([(1756, 316), (1792, 376), (1720, 376)], outline=(168, 62, 48, 210), width=2)
    for ly in range(186, 252, 12):
        fd.line([(1650, ly), (1748, ly)], fill=(78, 56, 38, 185), width=2)
    # Red research threads linking pins
    for a, b in [(0, 1), (1, 4), (0, 3), (3, 4), (1, 2)]:
        fd.line([pins[a], pins[b]], fill=(198, 42, 48, 225), width=2)
    for px, py in pins:
        draw_shaded_sphere(furn, px, py, 6, (218, 48, 52), (255, 190, 190))

    # --- Right Bookcase & Grimoire Lectern (x=1485..1872, y=482..735) ---
    bk_wood = make_wood_texture(386, 252, base_rgb=(84, 48, 28), dark_rgb=(36, 18, 10), seed=89)
    furn.alpha_composite(bk_wood, (1485, 482))
    fd = ImageDraw.Draw(furn)
    fd.rectangle([1485, 482, 1871, 734], outline=(30, 15, 8, 255), width=4)
    fd.rectangle([1499, 498, 1857, 604], fill=(28, 16, 14, 255))
    fd.rectangle([1499, 618, 1857, 722], fill=(28, 16, 14, 255))
    # Row of ornate leather-bound alchemy tomes with gold spine bands
    book_cols = [(146, 36, 44), (38, 86, 134), (48, 108, 64), (116, 52, 138), (154, 96, 34), (88, 48, 36)]
    bx = 1506
    while bx < 1835:
        bw = rng.randint(20, 32)
        bh = rng.randint(76, 98)
        col = book_cols[(bx // 23) % len(book_cols)]
        fd.rounded_rectangle([bx, 604 - bh, bx + bw - 2, 604], radius=3, fill=(*col, 255), outline=(22, 12, 10, 255))
        fd.line([(bx + 3, 604 - bh + 14), (bx + bw - 5, 604 - bh + 14)], fill=(238, 196, 92, 230), width=2)
        fd.line([(bx + 3, 604 - 16), (bx + bw - 5, 604 - 16)], fill=(238, 196, 92, 230), width=2)
        bx += bw + 2
    # Lower shelf: wax-sealed scrolls + brass astrolabe + stacked tomes
    for sy_book, scol in [(704, (132, 38, 42)), (684, (42, 78, 124)), (666, (62, 102, 54))]:
        fd.rounded_rectangle([1514, sy_book, 1636, sy_book + 18], radius=4, fill=(*scol, 255), outline=(24, 14, 10, 255))
        fd.rectangle([1524, sy_book + 4, 1628, sy_book + 14], fill=(236, 220, 186, 235))
    # Brass Astrolabe on lower bookcase shelf
    fd.ellipse([1690, 636, 1762, 708], outline=(222, 178, 84, 255), width=5)
    fd.ellipse([1704, 650, 1748, 694], outline=(222, 178, 84, 220), width=3)
    fd.rectangle([1712, 708, 1740, 720], fill=(198, 152, 66, 255))
    # Rolled parchment scrolls
    for sx_sc in (1786, 1816):
        fd.rounded_rectangle([sx_sc, 694, sx_sc + 26, 720], radius=8, fill=(232, 210, 168, 255), outline=(118, 86, 48, 255))
        fd.ellipse([sx_sc + 8, 702, sx_sc + 18, 712], fill=(188, 34, 38, 255))

    furn.save(os.path.join(lab_dir, "lab_furniture_back.png"))

    # 3. lab_storage_extra.png — Extra gilded apothecary rack & glowing crystal jars (Storage Upgrade!)
    extra = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ed = ImageDraw.Draw(extra)
    extra_shelf = make_wood_texture(260, 18, base_rgb=(118, 72, 38), dark_rgb=(54, 28, 14), seed=55)
    extra.alpha_composite(extra_shelf, (455, 118))
    ed = ImageDraw.Draw(extra)
    ed.rectangle([455, 118, 715, 136], outline=(218, 172, 78, 230), width=2)
    jar_cols = [(255, 84, 142), (92, 246, 255), (186, 112, 255), (255, 214, 72), (96, 250, 156)]
    for i, jx in enumerate(range(482, 702, 48)):
        draw_decorative_bottle(extra, jx, 118, 30, 68, jar_cols[i % len(jar_cols)], i)
    extra.save(os.path.join(lab_dir, "lab_storage_extra.png"))

    # 4. lab_desk_foreground.png — Heavy Wooden Laboratory Workbench + Transmutation Circle + Conduits
    desk = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    oak_top = make_wood_texture(W, 380, base_rgb=(104, 62, 36), dark_rgb=(48, 26, 14), grain_horizontal=True, seed=99)
    desk.alpha_composite(oak_top, (0, 700))
    dd = ImageDraw.Draw(desk)
    # Beveled front lip of workbench at y=700..722 and y=1030..1080
    dd.rectangle([0, 700, W, 718], fill=(128, 78, 46, 255), outline=(42, 22, 12, 255), width=3)
    dd.line([(0, 704), (W, 704)], fill=(214, 156, 98, 140), width=2)
    dd.rectangle([0, 1036, W, 1080], fill=(44, 24, 14, 255))
    dd.line([(0, 1036), (W, 1036)], fill=(198, 152, 72, 210), width=3)

    # Engraved Golden Transmutation Circle inlaid into the oak desk centered at (1156, 825)
    cx, cy = 1156, 825
    dd.ellipse([cx - 380, cy - 102, cx + 380, cy + 102], outline=(228, 182, 86, 165), width=4)
    dd.ellipse([cx - 345, cy - 88, cx + 345, cy + 88], outline=(228, 182, 86, 125), width=2)
    dd.ellipse([cx - 190, cy - 52, cx + 190, cy + 52], outline=(248, 208, 108, 180), width=3)
    # Copper feed conduits connecting Left Pedestal (830, 795), Right Pedestal (1482, 795), and Catalyst Pedestal (1156, 935) to Center
    draw_copper_pipe(dd, 830, 795, 1045, 755, 12, brass=True)
    draw_copper_pipe(dd, 1482, 795, 1267, 755, 12, brass=True)
    draw_copper_pipe(dd, 1156, 935, 1156, 815, 12, brass=True)

    # Open Leather Grimoire & Inkwell on right corner of desk (x=1725..1895, y=855..995)
    dd.polygon([(1725, 955), (1752, 868), (1895, 882), (1878, 978)], fill=(86, 32, 28, 255), outline=(36, 12, 10, 255), width=3)
    dd.polygon([(1734, 948), (1756, 874), (1816, 880), (1802, 956)], fill=(238, 222, 188, 255), outline=(156, 126, 84, 255))
    dd.polygon([(1816, 880), (1886, 888), (1870, 968), (1802, 956)], fill=(230, 212, 176, 255), outline=(156, 126, 84, 255))
    # Mortar & pestle resting on far left of desk (x=485, y=885)
    dd.pieslice([450, 845, 546, 936], 0, 180, fill=(92, 96, 108, 255), outline=(44, 46, 54, 255), width=3)
    dd.ellipse([450, 880, 546, 902], fill=(64, 68, 78, 255), outline=(138, 144, 158, 255), width=2)
    dd.line([(508, 890), (536, 842)], fill=(148, 154, 168, 255), width=12)

    desk.save(os.path.join(lab_dir, "lab_desk_foreground.png"))

    # 5. Individual Animated Equipment Sprites!
    # 5a. prop_burner_base.png (240x130)
    burner = Image.new("RGBA", (240, 130), (0, 0, 0, 0))
    brd = ImageDraw.Draw(burner)
    brd.ellipse([26, 84, 214, 122], fill=(68, 48, 24, 255), outline=(214, 168, 76, 255), width=4)
    brd.rounded_rectangle([54, 52, 186, 98], radius=14, fill=(188, 138, 58, 255), outline=(82, 52, 18, 255), width=4)
    # Brass grate nozzles
    for nx in range(74, 172, 22):
        brd.rectangle([nx - 5, 34, nx + 5, 56], fill=(224, 178, 86, 255), outline=(88, 56, 20, 255), width=2)
    # Tripod iron legs supporting cauldron
    brd.line([(48, 18), (32, 108)], fill=(46, 48, 58, 255), width=12)
    brd.line([(192, 18), (208, 108)], fill=(46, 48, 58, 255), width=12)
    burner.save(os.path.join(lab_dir, "prop_burner_base.png"))

    # 5b. prop_cauldron_back.png (380x340) & prop_cauldron_front.png (380x340) & prop_cauldron_liquid_base.png (260x110)
    c_back = Image.new("RGBA", (380, 340), (0, 0, 0, 0))
    cbd = ImageDraw.Draw(c_back)
    # Brass lion-ear side handles
    cbd.ellipse([18, 108, 86, 178], outline=(214, 168, 72, 255), width=11)
    cbd.ellipse([294, 108, 362, 178], outline=(214, 168, 72, 255), width=11)
    # Dark interior mouth of cauldron
    cbd.ellipse([54, 42, 326, 126], fill=(18, 14, 26, 255), outline=(198, 152, 68, 255), width=7)
    c_back.save(os.path.join(lab_dir, "prop_cauldron_back.png"))

    liq_mask = Image.new("RGBA", (260, 100), (0, 0, 0, 0))
    lmd = ImageDraw.Draw(liq_mask)
    lmd.ellipse([4, 6, 256, 94], fill=(255, 255, 255, 255))
    liq_mask.save(os.path.join(lab_dir, "prop_cauldron_liquid_base.png"))

    c_front = Image.new("RGBA", (380, 340), (0, 0, 0, 0))
    cfd = ImageDraw.Draw(c_front)
    # Heavy rounded cast-iron belly meeting mouth rim seamlessly at y=84 down to y=308
    cfd.pieslice([44, -140, 336, 308], 0, 180, fill=(46, 48, 62, 255), outline=(24, 24, 34, 255), width=5)
    # Shading highlight on left belly
    cfd.arc([62, -110, 318, 292], 108, 168, fill=(104, 112, 134, 165), width=14)
    # Brass equatorial band + rivets
    cfd.arc([44, 118, 336, 212], 8, 172, fill=(198, 152, 68, 255), width=14)
    for rx in range(78, 310, 36):
        ry = int(165 + math.sin((rx - 44) / 292.0 * math.pi) * 44)
        cfd.ellipse([rx - 4, ry - 4, rx + 4, ry + 4], fill=(252, 224, 142, 255))
    # Front lower brass rim lip of mouth so liquid sits INSIDE the cauldron!
    cfd.arc([54, 42, 326, 126], 0, 180, fill=(224, 178, 84, 255), width=9)
    # Central arched borosilicate sight-window framed in brass (transparent center so inner liquid glow shows through!)
    cfd.rounded_rectangle([142, 142, 238, 244], radius=28, fill=(24, 32, 48, 110), outline=(218, 172, 76, 255), width=6)
    cfd.arc([152, 152, 228, 234], 195, 265, fill=(255, 255, 255, 175), width=4)
    c_front.save(os.path.join(lab_dir, "prop_cauldron_front.png"))

    # 5c. prop_stabilizer_ring_outer.png & inner.png (380x380)
    for fname, r_out, col in [("prop_stabilizer_ring_outer.png", 174, (218, 174, 82)), ("prop_stabilizer_ring_inner.png", 148, (124, 226, 255))]:
        ring = Image.new("RGBA", (380, 380), (0, 0, 0, 0))
        rd = ImageDraw.Draw(ring)
        cx, cy = 190, 190
        rd.ellipse([cx - r_out, cy - r_out, cx + r_out, cy + r_out], outline=(*col, 195), width=6)
        rd.ellipse([cx - r_out + 10, cy - r_out + 10, cx + r_out - 10, cy + r_out - 10], outline=(*col, 110), width=2)
        for k in range(8):
            ang = k * math.pi / 4
            px, py = cx + math.cos(ang) * r_out, cy + math.sin(ang) * r_out
            draw_shaded_sphere(ring, px, py, 8, col, (255, 255, 255))
        ring.save(os.path.join(lab_dir, fname))

    # 5d. prop_pedestal.png & prop_pedestal_gold.png (156x156)
    for fname, c_rim, c_gem in [("prop_pedestal.png", (196, 148, 68), (72, 198, 255)), ("prop_pedestal_gold.png", (246, 198, 82), (214, 108, 255))]:
        ped = Image.new("RGBA", (156, 156), (0, 0, 0, 0))
        pd = ImageDraw.Draw(ped)
        pd.ellipse([14, 96, 142, 146], fill=(48, 34, 24, 240), outline=(*c_rim, 255), width=4)
        pd.polygon([(46, 118), (60, 78), (96, 78), (110, 118)], fill=(*c_rim, 255), outline=(78, 52, 18, 255), width=3)
        pd.ellipse([20, 48, 136, 94], fill=(26, 22, 38, 245), outline=(*c_rim, 255), width=5)
        pd.ellipse([34, 56, 122, 86], outline=(*c_gem, 185), width=2)
        draw_shaded_sphere(ped, 78, 110, 9, c_gem, (255, 255, 255))
        ped.save(os.path.join(lab_dir, fname))

    # 5e. prop_detector_lantern.png (140x200)
    lant = Image.new("RGBA", (140, 200), (0, 0, 0, 0))
    ld = ImageDraw.Draw(lant)
    ld.ellipse([24, 160, 116, 192], fill=(184, 138, 58, 255), outline=(76, 48, 16, 255), width=3)
    ld.rounded_rectangle([36, 66, 104, 168], radius=14, fill=(44, 26, 68, 190), outline=(218, 172, 78, 255), width=5)
    # Faceted resonance crystal inside lantern
    ld.polygon([(70, 80), (90, 116), (70, 154), (50, 116)], fill=(196, 118, 255, 245), outline=(255, 236, 255, 245), width=2)
    ld.pieslice([32, 36, 108, 92], 180, 360, fill=(198, 152, 68, 255), outline=(76, 48, 16, 255), width=3)
    ld.ellipse([58, 18, 82, 42], outline=(218, 172, 78, 255), width=4)
    lant.save(os.path.join(lab_dir, "prop_detector_lantern.png"))

    # 5f. prop_automaton_arm.png (220x220)
    arm = Image.new("RGBA", (220, 220), (0, 0, 0, 0))
    ad = ImageDraw.Draw(arm)
    ad.rounded_rectangle([18, 162, 92, 206], radius=8, fill=(168, 124, 56, 255), outline=(68, 42, 16, 255), width=3)
    draw_copper_pipe(ad, 54, 166, 86, 82, 16, brass=True)
    draw_copper_pipe(ad, 86, 82, 178, 106, 14, brass=True)
    # Brass joint gears
    for gx, gy, gr in [(54, 166, 18), (86, 82, 22)]:
        ad.ellipse([gx - gr, gy - gr, gx + gr, gy + gr], fill=(218, 174, 78, 255), outline=(78, 48, 16, 255), width=3)
        ad.ellipse([gx - 7, gy - 7, gx + 7, gy + 7], fill=(42, 32, 24, 255))
    # Glass pipette dropper at tip
    ad.polygon([(172, 92), (192, 92), (186, 154), (178, 154)], fill=(192, 242, 255, 220), outline=(255, 255, 255, 240), width=2)
    draw_shaded_sphere(arm, 182, 166, 7, (112, 236, 255), (255, 255, 255))
    arm.save(os.path.join(lab_dir, "prop_automaton_arm.png"))

    # 5g. prop_gauge_dial.png & prop_gauge_needle.png (96x96)
    dial = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
    gd = ImageDraw.Draw(dial)
    gd.ellipse([6, 6, 90, 90], fill=(208, 164, 72, 255), outline=(76, 48, 16, 255), width=4)
    gd.ellipse([14, 14, 82, 82], fill=(242, 230, 202, 255), outline=(92, 66, 32, 255), width=2)
    for k in range(9):
        ang = math.radians(135 + k * 30)
        gd.line([(48 + math.cos(ang) * 24, 48 + math.sin(ang) * 24), (48 + math.cos(ang) * 31, 48 + math.sin(ang) * 31)], fill=(48, 32, 22, 255), width=2)
    dial.save(os.path.join(lab_dir, "prop_gauge_dial.png"))

    needle = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
    nd = ImageDraw.Draw(needle)
    nd.polygon([(45, 52), (51, 52), (48, 18)], fill=(204, 36, 36, 255))
    nd.ellipse([42, 42, 54, 54], fill=(58, 42, 28, 255))
    needle.save(os.path.join(lab_dir, "prop_gauge_needle.png"))

    # 5h. 6 Animated Shelf Bottles (prop_shelf_bottle_1..6.png, 72x110)
    anim_cols = [(255, 76, 128), (64, 224, 255), (124, 248, 92), (196, 102, 255), (255, 192, 52), (72, 252, 204)]
    for i, col in enumerate(anim_cols, 1):
        sb = Image.new("RGBA", (72, 110), (0, 0, 0, 0))
        sb = add_radial_glow(sb, 36, 66, 34, col, alpha_max=90)
        draw_decorative_bottle(sb, 36, 102, 40, 86, col, i)
        sb.save(os.path.join(lab_dir, f"prop_shelf_bottle_{i}.png"))

    # 5i. prop_candle_holder.png (90x140)
    cand = Image.new("RGBA", (90, 140), (0, 0, 0, 0))
    cd = ImageDraw.Draw(cand)
    cd.ellipse([14, 112, 76, 134], fill=(206, 162, 72, 255), outline=(78, 48, 16, 255), width=3)
    cd.rounded_rectangle([30, 52, 60, 122], radius=6, fill=(244, 232, 202, 255), outline=(182, 162, 126, 255), width=2)
    cd.line([(45, 38), (45, 52)], fill=(36, 28, 24, 255), width=3)
    cand.save(os.path.join(lab_dir, "prop_candle_holder.png"))


def generate_ui_and_particles(root: str):
    ui_dir = os.path.join(root, "alchemy_chaos", "assets", "ui")
    part_dir = os.path.join(root, "alchemy_chaos", "assets", "particles")
    upg_dir = os.path.join(root, "alchemy_chaos", "assets", "upgrades")
    cat_dir = os.path.join(root, "alchemy_chaos", "assets", "categories")
    brand_dir = os.path.join(root, "alchemy_chaos", "assets", "branding")
    inst_dir = os.path.join(root, "installer")

    # --- 9-Slice Ornate Panels & Cards & Buttons ---
    make_dark_slate_panel(128, 128, (30, 25, 44), (16, 13, 25), corner_radius=14, border_w=3, rivets=True).save(os.path.join(ui_dir, "ui_panel_ornate.png"))
    make_dark_slate_panel(128, 128, (22, 18, 34), (12, 10, 20), corner_radius=10, border_w=2, rivets=False).save(os.path.join(ui_dir, "ui_panel_inset.png"))

    # Parchment encyclopedia page panel
    parch = make_parchment_texture(160, 160, seed=71)
    pd = ImageDraw.Draw(parch)
    pd.rounded_rectangle([2, 2, 157, 157], radius=10, outline=(124, 88, 46, 255), width=4)
    pd.rounded_rectangle([8, 8, 151, 151], radius=7, outline=(168, 124, 68, 160), width=2)
    parch.save(os.path.join(ui_dir, "ui_panel_parchment.png"))

    # Cards
    make_dark_slate_panel(96, 96, (38, 32, 56), (24, 20, 36), (164, 126, 64), corner_radius=10, border_w=2, rivets=False).save(os.path.join(ui_dir, "ui_card_normal.png"))
    make_dark_slate_panel(96, 96, (56, 46, 82), (34, 28, 52), (244, 204, 104), corner_radius=10, border_w=3, rivets=False).save(os.path.join(ui_dir, "ui_card_hover.png"))
    make_dark_slate_panel(96, 96, (68, 52, 96), (42, 30, 64), (110, 238, 255), (220, 255, 255), corner_radius=10, border_w=3, rivets=False).save(os.path.join(ui_dir, "ui_card_selected.png"))
    make_dark_slate_panel(96, 96, (22, 20, 30), (14, 12, 20), (88, 76, 62), corner_radius=10, border_w=2, rivets=False).save(os.path.join(ui_dir, "ui_card_locked.png"))

    # Primary Gilded Buttons (Normal, Hover, Pressed, Disabled) + Gold Action Buttons
    make_dark_slate_panel(120, 52, (46, 78, 74), (24, 44, 44), (208, 166, 78), corner_radius=10, border_w=2, rivets=False).save(os.path.join(ui_dir, "ui_btn_normal.png"))
    make_dark_slate_panel(120, 52, (64, 112, 104), (36, 66, 64), (250, 214, 118), corner_radius=10, border_w=3, rivets=False).save(os.path.join(ui_dir, "ui_btn_hover.png"))
    make_dark_slate_panel(120, 52, (28, 52, 50), (18, 34, 34), (168, 128, 56), corner_radius=10, border_w=2, rivets=False).save(os.path.join(ui_dir, "ui_btn_pressed.png"))
    make_dark_slate_panel(120, 52, (34, 32, 40), (22, 20, 28), (92, 84, 74), corner_radius=10, border_w=2, rivets=False).save(os.path.join(ui_dir, "ui_btn_disabled.png"))

    # Amber-Crimson Transmute Hero Button
    make_dark_slate_panel(140, 58, (148, 44, 68), (82, 18, 38), (248, 202, 92), corner_radius=12, border_w=3, rivets=True).save(os.path.join(ui_dir, "ui_btn_transmute_normal.png"))
    make_dark_slate_panel(140, 58, (188, 58, 86), (112, 26, 52), (255, 232, 138), corner_radius=12, border_w=3, rivets=True).save(os.path.join(ui_dir, "ui_btn_transmute_hover.png"))
    make_dark_slate_panel(140, 58, (98, 24, 42), (56, 12, 24), (196, 152, 64), corner_radius=12, border_w=3, rivets=True).save(os.path.join(ui_dir, "ui_btn_transmute_pressed.png"))

    # Progress bar & Toast Banner
    make_dark_slate_panel(96, 28, (18, 15, 28), (12, 10, 20), (148, 116, 58), corner_radius=8, border_w=2, rivets=False).save(os.path.join(ui_dir, "ui_progress_bg.png"))
    make_dark_slate_panel(96, 24, (72, 198, 156), (34, 128, 108), (196, 255, 224), corner_radius=6, border_w=1, rivets=False).save(os.path.join(ui_dir, "ui_progress_fill.png"))
    make_dark_slate_panel(160, 72, (46, 32, 68), (24, 16, 38), (248, 204, 96), corner_radius=12, border_w=3, rivets=True).save(os.path.join(ui_dir, "ui_toast_banner.png"))

    # --- Custom Illustrated 64x64 UI & Category Icons (ZERO Emojis!) ---
    def make_icon_64(draw_fn, glow_rgb=(242, 198, 92)):
        im = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
        im = add_radial_glow(im, 64, 64, 54, glow_rgb, alpha_max=85)
        fg = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
        draw_fn(fg, ImageDraw.Draw(fg))
        fg = add_outline_and_shadow(fg, shadow_alpha=90)
        im = Image.alpha_composite(im, fg)
        return im.resize((64, 64), resample=Image.Resampling.LANCZOS)

    # Navigation & Action Icons
    make_icon_64(lambda im, d: (
        d.ellipse([30, 48, 92, 108], fill=(92, 214, 255, 235), outline=(235, 248, 255, 255), width=4),
        d.polygon([(52, 50), (72, 50), (104, 78), (96, 86), (68, 62), (54, 62)], fill=(215, 245, 255, 235)),
    ), (92, 214, 255)).save(os.path.join(ui_dir, "icon_nav_lab.png"))

    make_icon_64(lambda im, d: (
        d.rounded_rectangle([24, 22, 104, 106], radius=8, fill=(142, 42, 52, 255), outline=(244, 198, 88, 255), width=5),
        d.rectangle([34, 28, 96, 98], fill=(238, 222, 186, 255)),
        d.ellipse([48, 46, 82, 80], outline=(168, 52, 48, 255), width=4),
    ), (244, 198, 88)).save(os.path.join(ui_dir, "icon_nav_journal.png"))

    make_icon_64(lambda im, d: (
        d.ellipse([26, 26, 102, 102], fill=(218, 168, 68, 255), outline=(92, 58, 18, 255), width=5),
        d.ellipse([46, 46, 82, 82], fill=(32, 24, 42, 255), outline=(255, 228, 142, 255), width=4),
        d.polygon([(64, 14), (74, 30), (54, 30)], fill=(246, 206, 96, 255)),
        d.polygon([(64, 114), (74, 98), (54, 98)], fill=(246, 206, 96, 255)),
        d.polygon([(14, 64), (30, 74), (30, 54)], fill=(246, 206, 96, 255)),
        d.polygon([(114, 64), (98, 74), (98, 54)], fill=(246, 206, 96, 255)),
    ), (244, 198, 88)).save(os.path.join(ui_dir, "icon_nav_upgrades.png"))

    make_icon_64(lambda im, d: (
        d.rounded_rectangle([30, 20, 98, 108], radius=10, fill=(238, 218, 176, 255), outline=(146, 104, 48, 255), width=4),
        d.ellipse([50, 74, 78, 102], fill=(198, 36, 48, 255), outline=(255, 196, 140, 255), width=2),
        d.line([(42, 40), (86, 40)], fill=(96, 68, 38, 255), width=4),
        d.line([(42, 56), (82, 56)], fill=(96, 68, 38, 255), width=4),
    ), (255, 148, 92)).save(os.path.join(ui_dir, "icon_nav_challenges.png"))

    make_icon_64(lambda im, d: (
        d.polygon([(64, 18), (78, 48), (110, 52), (86, 74), (92, 106), (64, 90), (36, 106), (42, 74), (18, 52), (50, 48)], fill=(255, 206, 64, 255), outline=(148, 92, 12, 255), width=3),
    ), (255, 212, 84)).save(os.path.join(ui_dir, "icon_nav_achievements.png"))

    make_icon_64(lambda im, d: (
        d.line([(26, 40), (102, 40)], fill=(198, 162, 88, 255), width=6),
        d.line([(26, 68), (102, 68)], fill=(198, 162, 88, 255), width=6),
        d.line([(26, 96), (102, 96)], fill=(198, 162, 88, 255), width=6),
        draw_shaded_sphere(im, 48, 40, 11, (108, 224, 255), (255, 255, 255)),
        draw_shaded_sphere(im, 82, 68, 11, (246, 198, 82), (255, 255, 255)),
        draw_shaded_sphere(im, 56, 96, 11, (196, 118, 255), (255, 255, 255)),
    ), (180, 196, 224)).save(os.path.join(ui_dir, "icon_nav_settings.png"))

    make_icon_64(lambda im, d: (
        d.polygon([(64, 16), (102, 64), (64, 112), (26, 64)], fill=(168, 96, 255, 255), outline=(235, 220, 255, 255), width=4),
        d.polygon([(64, 16), (64, 112), (26, 64)], fill=(116, 226, 255, 240)),
    ), (178, 112, 255)).save(os.path.join(ui_dir, "icon_currency_rp.png"))

    make_icon_64(lambda im, d: (
        d.line([(76, 76), (108, 108)], fill=(186, 128, 56, 255), width=12),
        d.ellipse([22, 22, 86, 86], fill=(142, 224, 255, 130), outline=(232, 188, 86, 255), width=8),
    ), (142, 224, 255)).save(os.path.join(ui_dir, "icon_search_lens.png"))

    make_icon_64(lambda im, d: (
        d.ellipse([22, 22, 106, 106], outline=(255, 206, 84, 255), width=6),
        d.polygon([(64, 26), (98, 86), (30, 86)], outline=(255, 96, 138, 255), width=5),
        draw_shaded_sphere(im, 64, 66, 14, (255, 76, 124), (255, 240, 180)),
    ), (255, 96, 138)).save(os.path.join(ui_dir, "icon_transmute.png"))

    make_icon_64(lambda im, d: (
        d.rounded_rectangle([34, 48, 94, 106], radius=10, fill=(78, 68, 92, 255), outline=(214, 174, 88, 255), width=4),
        d.arc([44, 22, 84, 58], 180, 360, fill=(214, 174, 88, 255), width=8),
        d.ellipse([56, 68, 72, 84], fill=(24, 18, 32, 255)),
    ), (160, 138, 188)).save(os.path.join(ui_dir, "icon_lock_mystery.png"))

    # Category Icons (6)
    cat_specs = [
        ("cat_Elements.png", (255, 118, 52), lambda im, d: d.polygon([(64, 18), (98, 94), (30, 94)], fill=(255, 118, 52, 255), outline=(255, 228, 132, 255), width=4)),
        ("cat_Minerals.png", (128, 214, 255), lambda im, d: d.polygon([(64, 16), (98, 56), (82, 108), (46, 108), (30, 56)], fill=(116, 202, 252, 255), outline=(255, 255, 255, 255), width=4)),
        ("cat_Plants.png", (82, 218, 112), lambda im, d: d.ellipse([34, 24, 94, 104], fill=(74, 206, 104, 255), outline=(196, 255, 198, 255), width=4)),
        ("cat_Metals.png", (204, 216, 232), lambda im, d: d.polygon([(26, 86), (42, 42), (86, 42), (102, 86)], fill=(192, 206, 224, 255), outline=(255, 255, 255, 255), width=4)),
        ("cat_Synthetic.png", (116, 242, 92), lambda im, d: (d.polygon([(52, 26), (76, 26), (98, 98), (30, 98)], fill=(96, 228, 112, 235), outline=(235, 255, 240, 255), width=4))),
        ("cat_Rare.png", (214, 96, 255), lambda im, d: d.polygon([(64, 16), (80, 50), (112, 64), (80, 78), (64, 112), (48, 78), (16, 64), (48, 50)], fill=(214, 96, 255, 255), outline=(255, 228, 132, 255), width=3)),
    ]
    for fn, grgb, dfn in cat_specs:
        make_icon_64(dfn, grgb).save(os.path.join(cat_dir, fn))

    # 7 Upgrade Illustrations (96x96)
    upg_specs = [
        ("upg_burner.png", (255, 116, 48), lambda im, d: (
            d.rounded_rectangle([30, 78, 98, 108], radius=8, fill=(208, 158, 66, 255), outline=(92, 56, 18, 255), width=4),
            d.polygon([(64, 16), (92, 74), (64, 84), (36, 74)], fill=(68, 186, 255, 255)),
            d.polygon([(64, 34), (80, 74), (48, 74)], fill=(235, 250, 255, 255)),
        )),
        ("upg_mixer.png", (246, 198, 84), lambda im, d: (
            draw_shaded_sphere(im, 36, 44, 16, (218, 168, 68), (255, 240, 180)),
            draw_shaded_sphere(im, 92, 44, 16, (218, 168, 68), (255, 240, 180)),
            draw_shaded_sphere(im, 64, 92, 20, (124, 226, 255), (255, 255, 255)),
            d.line([(36, 44), (64, 92), (92, 44)], fill=(238, 188, 86, 255), width=6),
        )),
        ("upg_stabilizer.png", (112, 232, 255), lambda im, d: (
            d.ellipse([20, 20, 108, 108], outline=(224, 178, 82, 255), width=7),
            d.ellipse([34, 42, 94, 86], outline=(112, 232, 255, 255), width=5),
            draw_shaded_sphere(im, 64, 64, 14, (186, 112, 255), (255, 255, 255)),
        )),
        ("upg_storage.png", (188, 128, 72), lambda im, d: (
            d.rounded_rectangle([22, 26, 106, 104], radius=8, fill=(116, 68, 38, 255), outline=(228, 182, 84, 255), width=4),
            d.line([(24, 64), (104, 64)], fill=(228, 182, 84, 255), width=4),
            draw_shaded_sphere(im, 46, 46, 10, (92, 224, 255), (255, 255, 255)),
            draw_shaded_sphere(im, 82, 46, 10, (255, 112, 158), (255, 255, 255)),
            draw_shaded_sphere(im, 64, 84, 10, (124, 246, 112), (255, 255, 255)),
        )),
        ("upg_automaton.png", (232, 182, 82), lambda im, d: (
            d.ellipse([26, 36, 82, 92], fill=(214, 166, 68, 255), outline=(88, 54, 18, 255), width=4),
            d.line([(54, 64), (98, 28)], fill=(238, 196, 96, 255), width=10),
            d.polygon([(92, 28), (108, 28), (102, 74), (96, 74)], fill=(168, 238, 255, 240)),
        )),
        ("upg_research_lens.png", (168, 124, 255), lambda im, d: (
            d.polygon([(26, 92), (82, 28), (96, 42), (40, 106)], fill=(214, 168, 72, 255), outline=(88, 54, 18, 255), width=3),
            draw_shaded_sphere(im, 88, 36, 18, (148, 226, 255), (255, 255, 255)),
        )),
        ("upg_detector.png", (226, 108, 255), lambda im, d: (
            d.rounded_rectangle([40, 36, 88, 98], radius=10, fill=(58, 28, 88, 230), outline=(232, 188, 86, 255), width=5),
            d.polygon([(64, 46), (78, 68), (64, 90), (50, 68)], fill=(232, 118, 255, 255)),
            d.arc([20, 18, 108, 106], -45, 45, fill=(124, 242, 255, 230), width=4),
            d.arc([20, 18, 108, 106], 135, 225, fill=(124, 242, 255, 230), width=4),
        )),
    ]
    for fn, grgb, dfn in upg_specs:
        im = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
        im = add_radial_glow(im, 64, 64, 58, grgb, alpha_max=95)
        fg = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
        dfn(fg, ImageDraw.Draw(fg))
        fg = add_outline_and_shadow(fg, shadow_alpha=95)
        im = Image.alpha_composite(im, fg).resize((96, 96), Image.Resampling.LANCZOS)
        im.save(os.path.join(upg_dir, fn))

    # --- 9 Particle Textures (64x64 RGBA) for GPUParticles2D ---
    # 1. part_bubble.png
    bub = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    bd = ImageDraw.Draw(bub)
    bd.ellipse([6, 6, 58, 58], fill=(255, 255, 255, 45), outline=(255, 255, 255, 220), width=3)
    bd.arc([14, 14, 50, 50], 195, 260, fill=(255, 255, 255, 255), width=4)
    bub.save(os.path.join(part_dir, "part_bubble.png"))

    # 2. part_smoke.png
    smk = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    smk = add_radial_glow(smk, 32, 32, 30, (255, 255, 255), alpha_max=190)
    smk = smk.filter(ImageFilter.GaussianBlur(3))
    smk.save(os.path.join(part_dir, "part_smoke.png"))

    # 3. part_spark.png
    spk = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    spk = add_radial_glow(spk, 32, 32, 28, (255, 240, 200), alpha_max=180)
    spd = ImageDraw.Draw(spk)
    spd.polygon([(32, 4), (38, 26), (60, 32), (38, 38), (32, 60), (26, 38), (4, 32), (26, 26)], fill=(255, 255, 255, 255))
    spk.save(os.path.join(part_dir, "part_spark.png"))

    # 4. part_crystal.png
    cry = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    crd = ImageDraw.Draw(cry)
    crd.polygon([(32, 4), (52, 32), (32, 60), (12, 32)], fill=(235, 250, 255, 235), outline=(255, 255, 255, 255), width=2)
    crd.polygon([(32, 4), (32, 60), (12, 32)], fill=(255, 255, 255, 255))
    cry.save(os.path.join(part_dir, "part_crystal.png"))

    # 5. part_frost.png
    frs = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    frd = ImageDraw.Draw(frs)
    for k in range(6):
        ang = math.radians(k * 60)
        ex, ey = 32 + math.cos(ang) * 26, 32 + math.sin(ang) * 26
        frd.line([(32, 32), (ex, ey)], fill=(255, 255, 255, 245), width=3)
        bx, by = 32 + math.cos(ang) * 16, 32 + math.sin(ang) * 16
        for off in (-0.6, 0.6):
            frd.line([(bx, by), (bx + math.cos(ang + off) * 9, by + math.sin(ang + off) * 9)], fill=(255, 255, 255, 225), width=2)
    frs.save(os.path.join(part_dir, "part_frost.png"))

    # 6. part_lightning.png
    lgt = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    lgd = ImageDraw.Draw(lgt)
    lgd.line([(10, 8), (34, 24), (22, 36), (52, 56)], fill=(255, 255, 255, 255), width=4)
    lgt.save(os.path.join(part_dir, "part_lightning.png"))

    # 7. part_droplet.png
    drp = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    drd = ImageDraw.Draw(drp)
    drd.polygon([(32, 8), (50, 40), (32, 56), (14, 40)], fill=(255, 255, 255, 240))
    drd.ellipse([14, 26, 50, 58], fill=(255, 255, 255, 240))
    drp.save(os.path.join(part_dir, "part_droplet.png"))

    # 8. part_arcane_rune.png
    run = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    rnd = ImageDraw.Draw(run)
    rnd.ellipse([6, 6, 58, 58], outline=(255, 255, 255, 230), width=3)
    rnd.polygon([(32, 10), (52, 46), (12, 46)], outline=(255, 255, 255, 220), width=2)
    rnd.polygon([(32, 54), (52, 18), (12, 18)], outline=(255, 255, 255, 220), width=2)
    run.save(os.path.join(part_dir, "part_arcane_rune.png"))

    # --- Application Icon (256x256 PNG + Multi-Size Windows .ICO!) & Branding ---
    app_icon = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    aid = ImageDraw.Draw(app_icon)
    aid.rounded_rectangle([8, 8, 248, 248], radius=48, fill=(24, 16, 38, 255), outline=(238, 192, 84, 255), width=8)
    app_icon = add_radial_glow(app_icon, 128, 128, 108, (186, 68, 255), alpha_max=150)
    app_icon = add_radial_glow(app_icon, 128, 140, 74, (255, 96, 64), alpha_max=140)
    aid = ImageDraw.Draw(app_icon)
    #Sacred gold transmutation ring
    aid.ellipse([28, 28, 228, 228], outline=(248, 206, 98, 210), width=4)
    # Ornate glowing alembic flask with Philosopher's Stone core inside!
    aid.ellipse([62, 86, 194, 218], fill=(38, 24, 68, 220), outline=(228, 246, 255, 255), width=6)
    aid.pieslice([70, 94, 186, 210], 0, 180, fill=(224, 42, 108, 245))
    aid.ellipse([72, 138, 184, 164], fill=(255, 168, 72, 245))
    aid.rectangle([108, 42, 148, 94], fill=(196, 238, 255, 160), outline=(235, 250, 255, 255), width=5)
    aid.rounded_rectangle([98, 34, 158, 48], radius=6, fill=(246, 202, 88, 255))
    aid.polygon([(128, 102), (156, 142), (128, 184), (100, 142)], fill=(255, 216, 76, 255), outline=(255, 255, 255, 255), width=3)
    app_icon.save(os.path.join(brand_dir, "app_icon_256.png"))
    app_icon.save(
        os.path.join(brand_dir, "app_icon.ico"),
        format="ICO",
        sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )

    # Main Title Crest Banner (logo_main.png, 760x200)
    logo = Image.new("RGBA", (760, 200), (0, 0, 0, 0))
    logo = add_radial_glow(logo, 380, 100, 320, (214, 118, 255), alpha_max=95)
    ld = ImageDraw.Draw(logo)
    ld.rounded_rectangle([24, 24, 736, 176], radius=26, fill=(24, 18, 38, 235), outline=(242, 196, 88, 255), width=5)
    ld.rounded_rectangle([34, 34, 726, 166], radius=20, outline=(242, 196, 88, 120), width=2)
    cinzel_path = os.path.join(root, "alchemy_chaos", "assets", "fonts", "Cinzel.ttf")
    nunito_path = os.path.join(root, "alchemy_chaos", "assets", "fonts", "Nunito.ttf")
    f_big = ImageFont.truetype(cinzel_path, 58)
    f_sub = ImageFont.truetype(nunito_path, 19)
    # Left & right crest gems
    draw_shaded_sphere(logo, 76, 100, 22, (255, 48, 98), (255, 226, 140))
    draw_shaded_sphere(logo, 684, 100, 22, (112, 228, 255), (255, 255, 255))
    ld = ImageDraw.Draw(logo)
    ld.text((132, 46), "ALCHEMY CHAOS", fill=(255, 222, 126, 255), font=f_big)
    ld.text((208, 124), "GRAND ARCANUM SANDBOX LABORATORY", fill=(208, 196, 232, 245), font=f_sub)
    logo.save(os.path.join(brand_dir, "logo_main.png"))

    # NSIS Installer BMPs (24-bit RGB BMP required by NSIS MUI2!)
    # Wizard sidebar: 164x314
    wiz = Image.new("RGBA", (164, 314), (20, 14, 34, 255))
    wiz = add_radial_glow(wiz, 82, 140, 130, (176, 68, 255), alpha_max=135)
    wiz.alpha_composite(app_icon.resize((116, 116), Image.Resampling.LANCZOS), (24, 48))
    wd = ImageDraw.Draw(wiz)
    wf1 = ImageFont.truetype(cinzel_path, 18)
    wf2 = ImageFont.truetype(nunito_path, 12)
    wd.text((18, 186), "ALCHEMY", fill=(255, 218, 118, 255), font=wf1)
    wd.text((34, 210), "CHAOS", fill=(255, 218, 118, 255), font=wf1)
    wd.text((22, 248), "50 Substances", fill=(206, 198, 228, 255), font=wf2)
    wd.text((22, 266), "120 Reactions", fill=(206, 198, 228, 255), font=wf2)
    wd.text((22, 284), "Grand Sanctum", fill=(206, 198, 228, 255), font=wf2)
    wiz.convert("RGB").save(os.path.join(inst_dir, "wizard_sidebar.bmp"), format="BMP")

    # Header banner: 150x57
    hdr = Image.new("RGBA", (150, 57), (24, 18, 38, 255))
    hdr = add_radial_glow(hdr, 120, 28, 52, (238, 182, 76), alpha_max=110)
    hdr.alpha_composite(app_icon.resize((44, 44), Image.Resampling.LANCZOS), (98, 6))
    hd = ImageDraw.Draw(hdr)
    hd.text((10, 18), "ALCHEMY", fill=(255, 218, 118, 255), font=ImageFont.truetype(cinzel_path, 14))
    hdr.convert("RGB").save(os.path.join(inst_dir, "header_banner.bmp"), format="BMP")

    # Custom Boot / Loading Screen Background (loading_bg.png, 1920x1080)
    splash = Image.new("RGBA", (1920, 1080), (14, 10, 24, 255))
    splash = add_radial_glow(splash, 960, 520, 620, (132, 54, 214), alpha_max=120)
    splash = add_radial_glow(splash, 960, 520, 340, (255, 132, 68), alpha_max=95)
    spd = ImageDraw.Draw(splash)
    for r in (420, 385, 275):
        spd.ellipse([960 - r, 500 - r, 960 + r, 500 + r], outline=(242, 196, 88, 105), width=3)
    spd.polygon([(960, 115), (1295, 695), (625, 695)], outline=(242, 196, 88, 85), width=3)
    spd.polygon([(960, 885), (1295, 305), (625, 305)], outline=(124, 226, 255, 75), width=3)
    splash.alpha_composite(logo, (960 - 380, 130))
    splash.alpha_composite(app_icon, (960 - 128, 390))
    splash.save(os.path.join(brand_dir, "loading_bg.png"))


def build_stage_preview(root: str):
    """Composites all laboratory layers + props into docs/preview_laboratory_stage.png for visual verification."""
    lab = os.path.join(root, "alchemy_chaos", "assets", "lab")
    comp = Image.open(os.path.join(lab, "lab_bg_stone_arch.png")).convert("RGBA")
    comp.alpha_composite(Image.open(os.path.join(lab, "lab_furniture_back.png")))
    comp.alpha_composite(Image.open(os.path.join(lab, "lab_storage_extra.png")))
    for i, bx in enumerate([518, 635, 750], 1):
        comp.alpha_composite(Image.open(os.path.join(lab, f"prop_shelf_bottle_{i}.png")), (bx - 36, 385 - 102))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_gauge_dial.png")), (895 - 48, 210))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_gauge_needle.png")), (895 - 48, 210))
    comp.alpha_composite(Image.open(os.path.join(lab, "lab_desk_foreground.png")))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_burner_base.png")), (1156 - 120, 720))
    comp = add_radial_glow(comp, 1156, 736, 95, (255, 132, 42), alpha_max=160)
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_stabilizer_ring_outer.png")), (1156 - 190, 620 - 190))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_cauldron_back.png")), (1156 - 190, 480))
    liq = Image.open(os.path.join(lab, "prop_cauldron_liquid_base.png")).copy()
    # Tint liquid emerald-cyan for preview
    arr = np.array(liq)
    arr[:, :, 0] = 64
    arr[:, :, 1] = 232
    arr[:, :, 2] = 198
    comp.alpha_composite(Image.fromarray(arr, "RGBA"), (1156 - 130, 514))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_cauldron_front.png")), (1156 - 190, 480))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_pedestal.png")), (830 - 78, 710))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_pedestal.png")), (1482 - 78, 710))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_pedestal_gold.png")), (1156 - 78, 845))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_automaton_arm.png")), (905, 435))
    comp.alpha_composite(Image.open(os.path.join(lab, "prop_detector_lantern.png")), (1605, 635))
    comp.save(os.path.join(root, "docs", "preview_laboratory_stage.png"))


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    lab_dir = os.path.join(root, "alchemy_chaos", "assets", "lab")
    fonts_dir = os.path.join(root, "alchemy_chaos", "assets", "fonts")
    generate_laboratory_layers(lab_dir, fonts_dir)
    generate_ui_and_particles(root)
    build_stage_preview(root)
    print("Generated all Laboratory Layers, Animated Props, UI Chrome, Icons, Upgrades, Particles, App .ICO, and NSIS BMPs!")


if __name__ == "__main__":
    main()
