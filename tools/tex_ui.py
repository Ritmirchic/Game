"""UI art: icon atlas, logo, panels, crosshair, overlays, font atlases."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops
from texlib import *

FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"

CELL = 128
ATLAS_COLS = 8
ATLAS_ROWS = 8
ICON_NAMES = []


def _icon(d, s, kind):
    """Draw a white icon into a cell of size s (0..s coordinates)."""
    W = (255, 255, 255, 255)
    G = (190, 190, 195, 255)
    D = (120, 120, 125, 255)
    if kind == "bat":
        d.line([(s * 0.28, s * 0.80), (s * 0.60, s * 0.42)], fill=W, width=int(s * 0.075))
        d.ellipse([s * 0.50, s * 0.20, s * 0.80, s * 0.55], fill=W)
        d.line([(s * 0.30, s * 0.78), (s * 0.22, s * 0.88)], fill=G, width=int(s * 0.06))
    elif kind == "axe":
        d.line([(s * 0.24, s * 0.84), (s * 0.66, s * 0.30)], fill=(150, 110, 70, 255), width=int(s * 0.07))
        d.polygon([(s * 0.62, s * 0.14), (s * 0.86, s * 0.26), (s * 0.72, s * 0.46), (s * 0.54, s * 0.34)], fill=W)
        d.polygon([(s * 0.62, s * 0.14), (s * 0.86, s * 0.26), (s * 0.80, s * 0.34), (s * 0.58, s * 0.22)], fill=G)
    elif kind == "pistol":
        d.polygon([(s * 0.18, s * 0.36), (s * 0.80, s * 0.36), (s * 0.80, s * 0.46), (s * 0.44, s * 0.46),
                   (s * 0.40, s * 0.74), (s * 0.24, s * 0.74), (s * 0.26, s * 0.46), (s * 0.18, s * 0.46)], fill=W)
        d.rectangle([s * 0.32, s * 0.48, s * 0.40, s * 0.56], fill=D)
    elif kind == "shotgun":
        d.rectangle([s * 0.10, s * 0.42, s * 0.82, s * 0.50], fill=W)
        d.rectangle([s * 0.10, s * 0.50, s * 0.60, s * 0.55], fill=G)
        d.polygon([(s * 0.62, s * 0.55), (s * 0.86, s * 0.62), (s * 0.86, s * 0.78), (s * 0.58, s * 0.66)], fill=(170, 130, 90, 255))
        d.rectangle([s * 0.30, s * 0.55, s * 0.40, s * 0.66], fill=G)
    elif kind == "rifle":
        d.rectangle([s * 0.08, s * 0.44, s * 0.86, s * 0.51], fill=W)
        d.rectangle([s * 0.36, s * 0.30, s * 0.58, s * 0.38], fill=G)
        d.polygon([(s * 0.44, s * 0.55), (s * 0.50, s * 0.55), (s * 0.48, s * 0.74), (s * 0.40, s * 0.74)], fill=G)
        d.polygon([(s * 0.70, s * 0.51), (s * 0.86, s * 0.58), (s * 0.86, s * 0.70), (s * 0.68, s * 0.58)], fill=(60, 60, 62, 255))
    elif kind == "sniper":
        d.rectangle([s * 0.06, s * 0.46, s * 0.90, s * 0.52], fill=W)
        d.rectangle([s * 0.34, s * 0.26, s * 0.66, s * 0.36], fill=G)
        d.rectangle([s * 0.30, s * 0.28, s * 0.34, s * 0.44], fill=G)
        d.rectangle([s * 0.66, s * 0.28, s * 0.70, s * 0.44], fill=G)
        d.polygon([(s * 0.76, s * 0.52), (s * 0.92, s * 0.62), (s * 0.88, s * 0.74), (s * 0.72, s * 0.60)], fill=(140, 105, 70, 255))
    elif kind == "crossbow":
        d.line([(s * 0.12, s * 0.30), (s * 0.36, s * 0.62)], fill=G, width=int(s * 0.05))
        d.line([(s * 0.12, s * 0.30), (s * 0.88, s * 0.30)], fill=W, width=int(s * 0.05))
        d.line([(s * 0.88, s * 0.30), (s * 0.64, s * 0.62)], fill=G, width=int(s * 0.05))
        d.rectangle([s * 0.44, s * 0.28, s * 0.56, s * 0.86], fill=(160, 120, 80, 255))
        d.polygon([(s * 0.44, s * 0.26), (s * 0.56, s * 0.26), (s * 0.50, s * 0.12)], fill=W)
    elif kind == "fist":
        d.rounded_rectangle([s * 0.30, s * 0.36, s * 0.72, s * 0.72], radius=int(s * 0.1), fill=W)
        for i in range(4):
            d.rounded_rectangle([s * (0.32 + i * 0.10), s * 0.24, s * (0.40 + i * 0.10), s * 0.40],
                                radius=int(s * 0.03), fill=W)
        d.rounded_rectangle([s * 0.24, s * 0.44, s * 0.34, s * 0.62], radius=int(s * 0.04), fill=G)
    elif kind == "ammo_pistol":
        for i in range(3):
            x = s * (0.26 + i * 0.18)
            d.rounded_rectangle([x, s * 0.34, x + s * 0.12, s * 0.62], radius=int(s * 0.02), fill=G)
            d.polygon([(x, s * 0.34), (x + s * 0.06, s * 0.20), (x + s * 0.12, s * 0.34)], fill=W)
        d.rectangle([s * 0.18, s * 0.62, s * 0.84, s * 0.76], fill=(120, 90, 50, 255))
    elif kind == "ammo_shell":
        for i in range(2):
            x = s * (0.28 + i * 0.24)
            d.rounded_rectangle([x, s * 0.36, x + s * 0.16, s * 0.74], radius=int(s * 0.03), fill=(160, 50, 40, 255))
            d.rectangle([x, s * 0.28, x + s * 0.16, s * 0.40], fill=G)
    elif kind == "ammo_rifle":
        for i in range(4):
            x = s * (0.20 + i * 0.16)
            d.rounded_rectangle([x, s * 0.40, x + s * 0.09, s * 0.70], radius=int(s * 0.02), fill=G)
            d.polygon([(x, s * 0.40), (x + s * 0.045, s * 0.22), (x + s * 0.09, s * 0.40)], fill=W)
    elif kind == "bolt":
        d.line([(s * 0.20, s * 0.80), (s * 0.80, s * 0.20)], fill=W, width=int(s * 0.05))
        d.polygon([(s * 0.86, s * 0.14), (s * 0.70, s * 0.26), (s * 0.74, s * 0.10)], fill=W)
        d.polygon([(s * 0.14, s * 0.86), (s * 0.30, s * 0.86), (s * 0.14, s * 0.70)], fill=G)
    elif kind == "medkit":
        d.rounded_rectangle([s * 0.16, s * 0.28, s * 0.84, s * 0.80], radius=int(s * 0.06), fill=(230, 230, 232, 255))
        d.rectangle([s * 0.44, s * 0.34, s * 0.56, s * 0.74], fill=(200, 40, 40, 255))
        d.rectangle([s * 0.30, s * 0.48, s * 0.70, s * 0.60], fill=(200, 40, 40, 255))
        d.rectangle([s * 0.42, s * 0.20, s * 0.58, s * 0.30], fill=D)
    elif kind == "bandage":
        d.rounded_rectangle([s * 0.18, s * 0.40, s * 0.82, s * 0.64], radius=int(s * 0.1), fill=(225, 215, 200, 255))
        d.rectangle([s * 0.42, s * 0.42, s * 0.58, s * 0.62], fill=(180, 170, 155, 255))
    elif kind == "pill":
        d.rounded_rectangle([s * 0.22, s * 0.42, s * 0.78, s * 0.60], radius=int(s * 0.09), fill=W)
        d.rounded_rectangle([s * 0.22, s * 0.42, s * 0.50, s * 0.60], radius=int(s * 0.09), fill=(210, 60, 60, 255))
    elif kind == "adrenaline":
        d.polygon([(s * 0.30, s * 0.84), (s * 0.46, s * 0.30), (s * 0.58, s * 0.30), (s * 0.44, s * 0.84)], fill=(210, 220, 230, 255))
        d.rectangle([s * 0.44, s * 0.16, s * 0.60, s * 0.34], fill=G)
        d.rectangle([s * 0.34, s * 0.56, s * 0.52, s * 0.62], fill=(210, 60, 60, 255))
    elif kind == "barricade":
        for i in range(3):
            y = s * (0.30 + i * 0.20)
            d.rectangle([s * 0.10, y, s * 0.90, y + s * 0.12], fill=(160, 115, 70, 255))
            d.line([(s * 0.12, y), (s * 0.88, y + s * 0.12)], fill=(110, 75, 45, 255), width=2)
        d.rectangle([s * 0.16, s * 0.22, s * 0.24, s * 0.84], fill=G)
        d.rectangle([s * 0.76, s * 0.22, s * 0.84, s * 0.84], fill=G)
    elif kind == "barricade_reinf":
        d.rectangle([s * 0.10, s * 0.30, s * 0.90, s * 0.80], fill=(130, 135, 140, 255))
        for i in range(4):
            d.line([(s * (0.14 + i * 0.20), s * 0.30), (s * (0.14 + i * 0.20), s * 0.80)], fill=D, width=3)
        d.rectangle([s * 0.10, s * 0.50, s * 0.90, s * 0.58], fill=(90, 95, 100, 255))
    elif kind == "metal_door":
        d.rectangle([s * 0.20, s * 0.12, s * 0.80, s * 0.88], fill=(120, 125, 130, 255))
        d.rectangle([s * 0.26, s * 0.18, s * 0.74, s * 0.44], fill=(90, 95, 100, 255))
        d.rectangle([s * 0.26, s * 0.50, s * 0.74, s * 0.82], fill=(90, 95, 100, 255))
        d.ellipse([s * 0.66, s * 0.56, s * 0.72, s * 0.62], fill=W)
    elif kind == "trap":
        d.rectangle([s * 0.14, s * 0.70, s * 0.86, s * 0.80], fill=(110, 110, 115, 255))
        for i in range(5):
            x = s * (0.20 + i * 0.15)
            d.polygon([(x, s * 0.70), (x + s * 0.07, s * 0.70), (x + s * 0.035, s * 0.22)], fill=W)
    elif kind == "money":
        d.ellipse([s * 0.18, s * 0.30, s * 0.82, s * 0.72], fill=(230, 200, 80, 255))
        d.ellipse([s * 0.24, s * 0.35, s * 0.76, s * 0.67], fill=(200, 165, 55, 255))
        try:
            f = ImageFont.truetype(FONT_BOLD, int(s * 0.30))
            d.text((s * 0.5, s * 0.51), "$", font=f, fill=(60, 45, 10, 255), anchor="mm")
        except Exception:
            pass
    elif kind == "scrap":
        d.polygon([(s * 0.20, s * 0.62), (s * 0.36, s * 0.28), (s * 0.62, s * 0.22), (s * 0.80, s * 0.48),
                   (s * 0.68, s * 0.76), (s * 0.38, s * 0.80)], fill=(150, 152, 158, 255))
        d.line([(s * 0.36, s * 0.28), (s * 0.52, s * 0.52)], fill=D, width=3)
        d.line([(s * 0.52, s * 0.52), (s * 0.80, s * 0.48)], fill=D, width=3)
    elif kind == "hammer":
        d.line([(s * 0.30, s * 0.84), (s * 0.56, s * 0.42)], fill=(150, 110, 70, 255), width=int(s * 0.07))
        d.rectangle([s * 0.44, s * 0.16, s * 0.86, s * 0.32], fill=(140, 142, 148, 255))
        d.rectangle([s * 0.52, s * 0.12, s * 0.60, s * 0.36], fill=(110, 112, 118, 255))
    elif kind == "flashlight":
        d.polygon([(s * 0.14, s * 0.44), (s * 0.30, s * 0.36), (s * 0.30, s * 0.66), (s * 0.14, s * 0.58)], fill=(240, 230, 160, 255))
        d.rectangle([s * 0.30, s * 0.38, s * 0.76, s * 0.64], fill=(120, 122, 128, 255))
        d.rectangle([s * 0.76, s * 0.34, s * 0.84, s * 0.68], fill=(190, 192, 198, 255))
    elif kind == "skull":
        d.ellipse([s * 0.24, s * 0.18, s * 0.76, s * 0.66], fill=(230, 230, 228, 255))
        d.rectangle([s * 0.34, s * 0.60, s * 0.66, s * 0.76], fill=(230, 230, 228, 255))
        d.ellipse([s * 0.32, s * 0.36, s * 0.44, s * 0.50], fill=(30, 30, 32, 255))
        d.ellipse([s * 0.56, s * 0.36, s * 0.68, s * 0.50], fill=(30, 30, 32, 255))
        d.polygon([(s * 0.46, s * 0.54), (s * 0.54, s * 0.54), (s * 0.50, s * 0.62)], fill=(30, 30, 32, 255))
        for i in range(3):
            d.line([(s * (0.38 + i * 0.10), s * 0.64), (s * (0.38 + i * 0.10), s * 0.76)], fill=(60, 60, 62, 255), width=3)
    elif kind == "heart":
        d.ellipse([s * 0.22, s * 0.26, s * 0.52, s * 0.56], fill=(210, 50, 50, 255))
        d.ellipse([s * 0.48, s * 0.26, s * 0.78, s * 0.56], fill=(210, 50, 50, 255))
        d.polygon([(s * 0.22, s * 0.44), (s * 0.78, s * 0.44), (s * 0.50, s * 0.84)], fill=(210, 50, 50, 255))
    elif kind == "stamina":
        d.polygon([(s * 0.54, s * 0.14), (s * 0.30, s * 0.56), (s * 0.46, s * 0.56), (s * 0.40, s * 0.86),
                   (s * 0.70, s * 0.42), (s * 0.52, s * 0.42)], fill=(230, 220, 90, 255))
    elif kind == "eye":
        d.ellipse([s * 0.12, s * 0.34, s * 0.88, s * 0.66], fill=(220, 220, 225, 255))
        d.ellipse([s * 0.38, s * 0.38, s * 0.62, s * 0.62], fill=(60, 120, 160, 255))
        d.ellipse([s * 0.45, s * 0.44, s * 0.55, s * 0.56], fill=(20, 20, 24, 255))
    elif kind == "walker":
        d.ellipse([s * 0.40, s * 0.14, s * 0.60, s * 0.32], fill=W)
        d.line([(s * 0.50, s * 0.32), (s * 0.50, s * 0.62)], fill=W, width=int(s * 0.10))
        d.line([(s * 0.50, s * 0.62), (s * 0.40, s * 0.86)], fill=W, width=int(s * 0.07))
        d.line([(s * 0.50, s * 0.62), (s * 0.60, s * 0.86)], fill=W, width=int(s * 0.07))
        d.line([(s * 0.50, s * 0.40), (s * 0.34, s * 0.54)], fill=W, width=int(s * 0.06))
        d.line([(s * 0.50, s * 0.40), (s * 0.66, s * 0.54)], fill=W, width=int(s * 0.06))
    elif kind == "runner":
        d.ellipse([s * 0.46, s * 0.12, s * 0.66, s * 0.30], fill=W)
        d.line([(s * 0.54, s * 0.30), (s * 0.44, s * 0.56)], fill=W, width=int(s * 0.10))
        d.line([(s * 0.44, s * 0.56), (s * 0.66, s * 0.72)], fill=W, width=int(s * 0.07))
        d.line([(s * 0.44, s * 0.56), (s * 0.24, s * 0.86)], fill=W, width=int(s * 0.07))
        d.line([(s * 0.50, s * 0.38), (s * 0.78, s * 0.40)], fill=W, width=int(s * 0.05))
        d.line([(s * 0.50, s * 0.40), (s * 0.26, s * 0.28)], fill=W, width=int(s * 0.05))
    elif kind == "crawler":
        d.ellipse([s * 0.34, s * 0.50, s * 0.66, s * 0.70], fill=W)
        d.ellipse([s * 0.62, s * 0.40, s * 0.80, s * 0.58], fill=W)
        d.line([(s * 0.36, s * 0.60), (s * 0.16, s * 0.72)], fill=W, width=int(s * 0.06))
        d.line([(s * 0.40, s * 0.68), (s * 0.24, s * 0.86)], fill=W, width=int(s * 0.06))
        d.line([(s * 0.60, s * 0.70), (s * 0.70, s * 0.86)], fill=W, width=int(s * 0.06))
    elif kind == "brute":
        d.ellipse([s * 0.38, s * 0.10, s * 0.62, s * 0.30], fill=W)
        d.rounded_rectangle([s * 0.28, s * 0.28, s * 0.72, s * 0.66], radius=int(s * 0.08), fill=W)
        d.rounded_rectangle([s * 0.18, s * 0.30, s * 0.30, s * 0.62], radius=int(s * 0.05), fill=W)
        d.rounded_rectangle([s * 0.70, s * 0.30, s * 0.82, s * 0.62], radius=int(s * 0.05), fill=W)
        d.rectangle([s * 0.34, s * 0.66, s * 0.46, s * 0.90], fill=W)
        d.rectangle([s * 0.54, s * 0.66, s * 0.66, s * 0.90], fill=W)
        d.line([(s * 0.28, s * 0.36), (s * 0.72, s * 0.36)], fill=(80, 80, 84, 255), width=4)
    elif kind == "screamer":
        d.ellipse([s * 0.36, s * 0.14, s * 0.64, s * 0.42], fill=W)
        d.ellipse([s * 0.44, s * 0.30, s * 0.56, s * 0.40], fill=(30, 30, 34, 255))
        d.line([(s * 0.50, s * 0.42), (s * 0.50, s * 0.68)], fill=W, width=int(s * 0.09))
        d.line([(s * 0.50, s * 0.68), (s * 0.40, s * 0.88)], fill=W, width=int(s * 0.06))
        d.line([(s * 0.50, s * 0.68), (s * 0.60, s * 0.88)], fill=W, width=int(s * 0.06))
        for i in range(3):
            a = 0.5 + i * 0.25
            d.arc([s * (0.10 + i * 0.04), s * (0.06 + i * 0.04), s * (0.90 - i * 0.04), s * (0.50 - i * 0.04)],
                  200, 340, fill=(220, 220, 230, 200), width=3)
    elif kind == "arrow_up":
        d.polygon([(s * 0.50, s * 0.16), (s * 0.82, s * 0.60), (s * 0.60, s * 0.60), (s * 0.60, s * 0.86),
                   (s * 0.40, s * 0.86), (s * 0.40, s * 0.60), (s * 0.18, s * 0.60)], fill=W)
    elif kind == "settings":
        d.ellipse([s * 0.34, s * 0.34, s * 0.66, s * 0.66], fill=W)
        for i in range(8):
            a = i * np.pi / 4
            cx, cy = s * 0.5, s * 0.5
            x0 = cx + np.cos(a) * s * 0.28
            y0 = cy + np.sin(a) * s * 0.28
            x1 = cx + np.cos(a) * s * 0.42
            y1 = cy + np.sin(a) * s * 0.42
            d.line([(x0, y0), (x1, y1)], fill=W, width=int(s * 0.07))
        d.ellipse([s * 0.44, s * 0.44, s * 0.56, s * 0.56], fill=(30, 30, 34, 255))
    elif kind == "shield":
        d.polygon([(s * 0.50, s * 0.12), (s * 0.84, s * 0.28), (s * 0.78, s * 0.64), (s * 0.50, s * 0.88),
                   (s * 0.22, s * 0.64), (s * 0.16, s * 0.28)], fill=(150, 160, 175, 255))
        d.polygon([(s * 0.50, s * 0.22), (s * 0.74, s * 0.33), (s * 0.70, s * 0.60), (s * 0.50, s * 0.78),
                   (s * 0.30, s * 0.60), (s * 0.26, s * 0.33)], fill=(190, 200, 215, 255))
    elif kind == "clock":
        d.ellipse([s * 0.16, s * 0.16, s * 0.84, s * 0.84], fill=(210, 210, 215, 255))
        d.ellipse([s * 0.22, s * 0.22, s * 0.78, s * 0.78], fill=(40, 42, 48, 255))
        d.line([(s * 0.50, s * 0.50), (s * 0.50, s * 0.28)], fill=W, width=int(s * 0.05))
        d.line([(s * 0.50, s * 0.50), (s * 0.68, s * 0.58)], fill=W, width=int(s * 0.05))
    elif kind == "door":
        d.rectangle([s * 0.24, s * 0.10, s * 0.76, s * 0.90], fill=(150, 110, 70, 255))
        d.rectangle([s * 0.30, s * 0.16, s * 0.70, s * 0.44], outline=D, width=3)
        d.rectangle([s * 0.30, s * 0.52, s * 0.70, s * 0.84], outline=D, width=3)
        d.ellipse([s * 0.64, s * 0.50, s * 0.70, s * 0.56], fill=(230, 210, 130, 255))
    elif kind == "wave":
        d.polygon([(s * 0.14, s * 0.86), (s * 0.14, s * 0.14), (s * 0.30, s * 0.26), (s * 0.46, s * 0.14),
                   (s * 0.62, s * 0.26), (s * 0.86, s * 0.14), (s * 0.86, s * 0.86)], fill=(200, 60, 55, 255))
    elif kind == "gun_ammo_box":
        d.rectangle([s * 0.16, s * 0.34, s * 0.84, s * 0.76], fill=(80, 90, 60, 255))
        d.rectangle([s * 0.16, s * 0.34, s * 0.84, s * 0.42], fill=(60, 68, 45, 255))
        d.rectangle([s * 0.40, s * 0.50, s * 0.60, s * 0.62], fill=(200, 190, 120, 255))
    elif kind == "fuel":
        d.rounded_rectangle([s * 0.24, s * 0.28, s * 0.72, s * 0.82], radius=int(s * 0.05), fill=(170, 60, 40, 255))
        d.rectangle([s * 0.30, s * 0.34, s * 0.66, s * 0.50], fill=(60, 60, 62, 255))
        d.line([(s * 0.68, s * 0.34), (s * 0.86, s * 0.22)], fill=(120, 120, 125, 255), width=int(s * 0.05))
    else:
        d.ellipse([s * 0.3, s * 0.3, s * 0.7, s * 0.7], fill=W)


def icon_atlas(path, size=1024, cell=128):
    global ICON_NAMES
    names = ["bat", "axe", "pistol", "shotgun", "rifle", "sniper", "crossbow", "fist",
             "ammo_pistol", "ammo_shell", "ammo_rifle", "bolt", "medkit", "bandage", "pill", "adrenaline",
             "barricade", "barricade_reinf", "metal_door", "trap", "money", "scrap", "hammer", "flashlight",
             "skull", "heart", "stamina", "eye", "walker", "runner", "crawler", "brute",
             "screamer", "arrow_up", "settings", "shield", "clock", "door", "wave", "gun_ammo_box",
             "fuel", "empty", "empty", "empty", "empty", "empty", "empty", "empty",
             "empty", "empty", "empty", "empty", "empty", "empty", "empty", "empty",
             "empty", "empty", "empty", "empty", "empty", "empty", "empty", "empty"]
    ICON_NAMES = names
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    for i, name in enumerate(names):
        c = i % ATLAS_COLS
        r = i // ATLAS_COLS
        cellimg = Image.new("RGBA", (cell, cell), (0, 0, 0, 0))
        d = ImageDraw.Draw(cellimg)
        _icon(d, cell, name)
        img.paste(cellimg, (c * cell, r * cell), cellimg)
    img.save(path)
    return names


def ui_logo(path, w=1024, h=512):
    img = Image.new("RGB", (w, h), (6, 6, 8))
    d = ImageDraw.Draw(img)
    # sky gradient
    for y in range(h):
        t = y / h
        col = (int(6 + 18 * t), int(8 + 16 * t), int(14 + 18 * t))
        d.line([(0, y), (w, y)], fill=col)
    # moon
    d.ellipse([w * 0.68, h * 0.10, w * 0.68 + 130, h * 0.10 + 130], fill=(180, 190, 200))
    d.ellipse([w * 0.68 + 20, h * 0.10 + 10, w * 0.68 + 110, h * 0.10 + 100], fill=(150, 160, 172))
    # blood moon tint
    moon = img.crop((int(w * 0.66), int(h * 0.06), int(w * 0.66) + 180, int(h * 0.06) + 180))
    moon = Image.blend(moon, Image.new("RGB", moon.size, (150, 40, 30)), 0.25)
    img.paste(moon, (int(w * 0.66), int(h * 0.06)))
    # skyline silhouette
    rng = np.random.default_rng(3)
    x = 0
    while x < w:
        bw = int(rng.integers(40, 110))
        bh = int(rng.integers(40, 190))
        d.rectangle([x, h - bh - 60, x + bw, h], fill=(8, 9, 12))
        # windows
        for wy in range(h - bh - 50, h - 20, 22):
            for wx in range(x + 8, x + bw - 10, 18):
                if rng.random() < 0.18:
                    d.rectangle([wx, wy, wx + 6, wy + 9], fill=(120, 100, 50))
        x += bw + int(rng.integers(4, 20))
    # supermarket roof line in front
    d.rectangle([0, h - 90, w, h], fill=(10, 11, 14))
    # title text
    try:
        f1 = ImageFont.truetype(FONT_BOLD, 116)
        f2 = ImageFont.truetype(FONT_REG, 30)
    except Exception:
        f1 = f2 = ImageFont.load_default()
    title = "THE LAST NIGHT"
    d.text((w // 2 + 4, 190 + 6), title, font=f1, fill=(0, 0, 0), anchor="mm")
    d.text((w // 2, 190), title, font=f1, fill=(196, 34, 30), anchor="mm")
    d.text((w // 2, 190), title, font=f1, fill=(226, 208, 190), anchor="mm", stroke_width=1)
    # re-draw a red gradient band inside letters by overlaying
    d.text((w // 2, 262), "S U R V I V E   T E N   N I G H T S", font=f2, fill=(190, 170, 140), anchor="mm")
    d.text((w // 2, h - 40), "A COOPERATIVE SURVIVAL HORROR", font=f2, fill=(110, 100, 92), anchor="mm")
    arr = np.asarray(img, np.float32) / 255.0
    # film grain + vignette + scratches
    u = np.linspace(0, 1, w)[None, :].repeat(h, 0)
    v = np.linspace(0, 1, h)[:, None].repeat(w, 1)
    vig = 1 - 0.75 * (np.clip((u - 0.5) ** 2 * 1.2 + (v - 0.5) ** 2 * 1.6, 0, 1))
    arr = arr * vig[..., None]
    n = np.random.default_rng(11).normal(0, 0.02, (h, w, 1))
    arr = np.clip(arr + n, 0, 1)
    blood = np.asarray(Image.fromarray((np.clip(fbm(u, v, 4, 5, 7) * 1.4 - 0.55, 0, 1) * 255).astype(np.uint8))
                      .filter(ImageFilter.GaussianBlur(3)), np.float32) / 255.0
    arr = arr * (1 - blood[..., None] * 0.55) + np.array([0.22, 0.01, 0.01]) * (blood[..., None] * 0.55)
    Image.fromarray((arr * 255).astype(np.uint8)).save(path)


def ui_panel(path, size=192):
    u, v = uv_grid(size)
    n = fbm(u, v, 16, 4, 71)
    brush = pnoise(u * 4, v * 200, 200, 73)
    base = rgb_img(0.055 + 0.02 * n, 0.06 + 0.02 * n, 0.068 + 0.022 * n)
    base = base * (0.9 + 0.2 * brush[..., None])
    edge = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))
    rim = 1 - smoothstep(0.0, 0.02, edge)
    base = base + rim[..., None] * 0.06
    a = np.ones_like(u) * 0.92
    arr = to_u8(np.clip(base, 0, 1))
    alpha = to_u8(a)
    out = np.dstack([arr, alpha])
    Image.fromarray(out, "RGBA").save(path)


def ui_panel_solid(path, size=128):
    u, v = uv_grid(size)
    n = fbm(u, v, 12, 4, 77)
    base = rgb_img(0.045 + 0.02 * n, 0.05 + 0.02 * n, 0.058 + 0.02 * n)
    out = np.dstack([to_u8(base), to_u8(np.ones_like(u))])
    Image.fromarray(out, "RGBA").save(path)


def ui_crosshair(path, size=128):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size // 2
    for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1)]:
        x0 = c + dx * 10
        y0 = c + dy * 10
        x1 = c + dx * 44
        y1 = c + dy * 44
        d.line([(x0, y0), (x1, y1)], fill=(235, 235, 235, 220), width=3)
    d.ellipse([c - 2, c - 2, c + 2, c + 2], fill=(235, 235, 235, 230))
    d.ellipse([c - 48, c - 48, c + 48, c + 48], outline=(235, 235, 235, 90), width=2)
    img.save(path)


def ui_blood_overlay(path, size=512):
    u, v = uv_grid(size)
    cx = u - 0.5
    cy = v - 0.5
    r = np.hypot(cx, cy) * 2
    n = fbm(u, v, 5, 5, 91)
    splat = smoothstep(0.55, 1.0, r + (n - 0.5) * 0.9)
    drips = smoothstep(0.62, 0.78, fbm(u * 4, v * 1.0, 6, 4, 93)) * smoothstep(0.2, 0.7, r)
    a = np.clip(splat * 0.85 + drips * 0.5, 0, 0.92)
    base = rgb_img(0.35, 0.02, 0.02) * (0.6 + 0.6 * n[..., None])
    out = np.dstack([to_u8(base), to_u8(a)])
    Image.fromarray(out, "RGBA").save(path)


def ui_noise(path, size=256):
    rng = np.random.default_rng(5)
    n = rng.normal(0.5, 0.16, (size, size, 1)).repeat(3, axis=2)
    out = np.dstack([to_u8(n), to_u8(np.ones((size, size)))])
    Image.fromarray(out, "RGBA").save(path)


def ui_vignette(path, size=256):
    u, v = uv_grid(size)
    r = np.hypot(u - 0.5, v - 0.5) * 2
    a = smoothstep(0.45, 1.25, r) * 0.8
    out = np.dstack([to_u8(np.zeros((size, size, 3))), to_u8(a)])
    Image.fromarray(out, "RGBA").save(path)


def ui_flash(path, size=128):
    """Radial flash / muzzle bloom sprite (additive)."""
    u, v = uv_grid(size)
    r = np.hypot(u - 0.5, v - 0.5) * 2
    core = np.exp(-(r * 6) ** 2)
    halo = np.exp(-(r * 2.5) ** 2) * 0.5
    spikes = np.zeros_like(u)
    for a in range(0, 8):
        ang = a * np.pi / 4
        d = np.abs((u - 0.5) * np.sin(ang) - (v - 0.5) * np.cos(ang))
        spikes = np.maximum(spikes, np.exp(-(d * 26) ** 2) * np.exp(-r * 2.2))
    a = np.clip(core + halo + spikes * 0.8, 0, 1)
    base = np.dstack([np.ones_like(u), np.ones_like(u) * 0.95, np.ones_like(u) * 0.85]) * a[..., None]
    out = np.dstack([np.clip(base, 0, 1), a])
    Image.fromarray(to_u8(out), "RGBA").save(path) if False else Image.fromarray(
        (np.clip(out, 0, 1) * 255).astype(np.uint8), "RGBA").save(path)


def ui_smoke(path, size=128):
    u, v = uv_grid(size)
    n = fbm(u, v, 6, 4, 111)
    r = np.hypot(u - 0.5, v - 0.5) * 2
    a = np.clip((1 - smoothstep(0.2, 1.0, r)) * (0.4 + 0.8 * n), 0, 1) * 0.85
    out = np.dstack([to_u8(np.ones((size, size, 3))), to_u8(a)])
    Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8), "RGBA").save(path)


def ui_rain(path, size=64):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(9)
    for i in range(14):
        x = rng.integers(0, size)
        y = rng.integers(0, size)
        ln = rng.integers(8, 22)
        d.line([(x, y), (x - 2, y + ln)], fill=(220, 230, 240, int(rng.integers(80, 180))), width=1)
    img.save(path)


def font_atlas(ttf, px, png_path, txt_path, name="ui"):
    """Render ASCII 32..126 into an atlas + metrics file."""
    try:
        font = ImageFont.truetype(ttf, px)
    except Exception:
        font = ImageFont.load_default()
    ascent, descent = font.getmetrics()
    lineheight = ascent + descent
    glyphs = []
    pad = 2
    x = pad
    y = pad
    rowh = 0
    W = 1024
    positions = []
    for cp in range(32, 127):
        ch = chr(cp)
        try:
            bbox = font.getbbox(ch)
        except Exception:
            bbox = (0, 0, px // 2, px)
        adv = int(round(font.getlength(ch))) if hasattr(font, "getlength") else px // 2
        w = max(0, bbox[2] - bbox[0])
        h = max(0, bbox[3] - bbox[1])
        if x + w + pad >= W:
            x = pad
            y += rowh + pad
            rowh = 0
        positions.append((cp, x, y, w, h, adv, bbox[0], bbox[1]))
        x += w + pad
        rowh = max(rowh, h)
    H = y + rowh + pad
    H = (H + 3) // 4 * 4
    img = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(img)
    for (cp, gx, gy, w, h, adv, bx, by) in positions:
        ch = chr(cp)
        if w > 0 and h > 0:
            d.text((gx - bx, gy - by), ch, font=font, fill=255)
    rgba = np.zeros((H, W, 4), np.uint8)
    rgba[..., 0] = 255
    rgba[..., 1] = 255
    rgba[..., 2] = 255
    rgba[..., 3] = np.asarray(img, np.uint8)
    Image.fromarray(rgba, "RGBA").save(png_path)
    with open(txt_path, "w") as f:
        f.write(f"L {lineheight} {ascent}\n")
        for (cp, gx, gy, w, h, adv, bx, by) in positions:
            f.write(f"G {cp} {gx} {gy} {w} {h} {adv} {bx} {by}\n")
    return lineheight
