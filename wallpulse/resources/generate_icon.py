#!/usr/bin/env python3
"""Generate multi-resolution Windows ICO icon for WallPulse Studio."""

import math
import os
import struct
import zlib


def make_png(width: int, height: int, rgba_bytes: bytes) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)  # filter type 0
        raw.extend(rgba_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw), 9)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")


def clamp(v: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return lo if v < lo else (hi if v > hi else v)


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def sd_rounded_box(px: float, py: float, bx: float, by: float, r: float) -> float:
    qx = abs(px) - bx + r
    qy = abs(py) - by + r
    out = math.hypot(max(qx, 0.0), max(qy, 0.0))
    inside = min(max(qx, qy), 0.0)
    return out + inside - r


def sd_triangle_play(px: float, py: float, scale: float) -> float:
    # Centered play triangle pointing right
    # Vertices in normalized icon space:
    ax, ay = -0.11 * scale, -0.15 * scale
    bx, by = -0.11 * scale, 0.15 * scale
    cx, cy = 0.16 * scale, 0.0

    def edge_dist(x1, y1, x2, y2):
        ex, ey = x2 - x1, y2 - y1
        wx, wy = px - x1, py - y1
        h = clamp((wx * ex + wy * ey) / (ex * ex + ey * ey), 0.0, 1.0)
        dx, dy = wx - ex * h, wy - ey * h
        return math.hypot(dx, dy), ex * wy - ey * wx

    d1, s1 = edge_dist(ax, ay, bx, by)
    d2, s2 = edge_dist(bx, by, cx, cy)
    d3, s3 = edge_dist(cx, cy, ax, ay)
    d = min(d1, d2, d3)
    inside = (s1 >= 0 and s2 >= 0 and s3 >= 0) or (s1 <= 0 and s2 <= 0 and s3 <= 0)
    return -d if inside else d


def render_icon(size: int) -> bytes:
    buf = bytearray(size * size * 4)
    aa = 1.35 / size

    for y in range(size):
        ny = (y + 0.5) / size * 2.0 - 1.0
        for x in range(size):
            nx = (x + 0.5) / size * 2.0 - 1.0

            # Outer rounded app squircle
            d_outer = sd_rounded_box(nx, ny, 0.88, 0.88, 0.24)
            alpha_outer = clamp(0.5 - d_outer / (2.0 * aa))
            if alpha_outer <= 0.0:
                continue

            # Deep obsidian-indigo gradient background
            grad_t = clamp((nx * 0.45 + ny * 0.65 + 1.0) * 0.5)
            r = lerp(13.0, 22.0, grad_t)
            g = lerp(18.0, 30.0, grad_t)
            b = lerp(30.0, 52.0, grad_t)

            # Ambient cyan + electric violet aurora wave inside screen
            wave_y = 0.18 * math.sin(nx * 2.8 + 0.4) - 0.08 * math.cos(nx * 4.5 - 0.7)
            dist_wave = abs(ny - wave_y)
            glow_cyan = math.exp(-dist_wave * dist_wave * 9.5)
            glow_violet = math.exp(-((ny + 0.22 - 0.14 * math.cos(nx * 2.4)) ** 2) * 8.0)

            r += glow_cyan * 12.0 + glow_violet * 118.0
            g += glow_cyan * 195.0 + glow_violet * 46.0
            b += glow_cyan * 245.0 + glow_violet * 220.0

            # Inner monitor bezel highlight
            d_screen = sd_rounded_box(nx, ny + 0.03, 0.68, 0.52, 0.10)
            border_screen = math.exp(-((d_screen / 0.032) ** 2))
            r = lerp(r, 70.0, border_screen * 0.65)
            g = lerp(g, 225.0, border_screen * 0.75)
            b = lerp(b, 255.0, border_screen * 0.85)

            # Subtle monitor stand bar at bottom
            d_stand = sd_rounded_box(nx, ny - 0.68, 0.26, 0.035, 0.03)
            stand_mask = clamp(0.5 - d_stand / (2.0 * aa))
            r = lerp(r, 95.0, stand_mask * 0.85)
            g = lerp(g, 220.0, stand_mask * 0.85)
            b = lerp(b, 255.0, stand_mask * 0.90)

            # Crisp glowing white-cyan play triangle at center
            d_play = sd_triangle_play(nx, ny + 0.03, 1.25) - 0.025
            play_glow = math.exp(-max(d_play, 0.0) * 10.0) * 0.45
            play_fill = clamp(0.5 - d_play / (2.0 * aa))

            r = lerp(r + play_glow * 40.0, 245.0, play_fill)
            g = lerp(g + play_glow * 210.0, 252.0, play_fill)
            b = lerp(b + play_glow * 255.0, 255.0, play_fill)

            # Subtle outer rim highlight
            rim = math.exp(-((d_outer / 0.03) ** 2)) * 0.45
            r = clamp(r + rim * 60.0, 0.0, 255.0)
            g = clamp(g + rim * 180.0, 0.0, 255.0)
            b = clamp(b + rim * 255.0, 0.0, 255.0)

            idx = (y * size + x) * 4
            buf[idx + 0] = int(round(r))
            buf[idx + 1] = int(round(g))
            buf[idx + 2] = int(round(b))
            buf[idx + 3] = int(round(alpha_outer * 255.0))

    return make_png(size, size, bytes(buf))


def build_ico(out_path: str) -> None:
    sizes = [16, 24, 32, 48, 64, 128, 256]
    pngs = [(s, render_icon(s)) for s in sizes]

    header = struct.pack("<HHH", 0, 1, len(pngs))
    offset = 6 + 16 * len(pngs)
    entries = bytearray()
    payload = bytearray()

    for s, png in pngs:
        w_byte = 0 if s == 256 else s
        h_byte = 0 if s == 256 else s
        entries.extend(
            struct.pack(
                "<BBBBHHII",
                w_byte,
                h_byte,
                0,
                0,
                1,
                32,
                len(png),
                offset,
            )
        )
        payload.extend(png)
        offset += len(png)

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(header + entries + payload)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "wallpulse.ico")
    build_ico(out)
    print(f"Generated {out} ({os.path.getsize(out)} bytes)")
