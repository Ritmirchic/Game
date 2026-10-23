#!/usr/bin/env python3
"""Offline visual QA renderer.

Reads the built place file and renders simple shaded views of the baked
geometry with a z-buffer so world composition, scale and lighting can be
inspected without Roblox Studio.
"""
import math
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from rbx import binary  # noqa: E402

WIDTH = 1280
HEIGHT = 760


def collect_parts(node, out):
    for c in node.children:
        if c.cls in ("Part", "SpawnLocation", "WedgePart"):
            cf = c.props.get("CFrame")
            size = c.props.get("Size")
            if not cf or not size:
                continue
            if c.props.get("Transparency", 0.0) >= 0.95:
                continue
            out.append((cf, size, c.props.get("Color", (128, 128, 128)),
                        c.props.get("Transparency", 0.0)))
        collect_parts(c, out)


def rot_apply(rot, v):
    return tuple(rot[i][0] * v[0] + rot[i][1] * v[1] + rot[i][2] * v[2] for i in range(3))


def normalize(v):
    l = math.sqrt(sum(c * c for c in v)) or 1.0
    return (v[0] / l, v[1] / l, v[2] / l)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def render(parts, cam_pos, target, fov=62.0, out="preview.png", sun=(0.55, 0.75, 0.35)):
    img = Image.new("RGB", (WIDTH, HEIGHT), (28, 30, 34))
    draw = ImageDraw.Draw(img, "RGBA")
    for y in range(HEIGHT):
        t = y / HEIGHT
        draw.line([(0, y), (WIDTH, y)], fill=(int(96 - 50 * t), int(104 - 52 * t), int(118 - 56 * t)))
    fwd = normalize((target[0] - cam_pos[0], target[1] - cam_pos[1], target[2] - cam_pos[2]))
    right = normalize(cross((0.0, 1.0, 0.0), fwd))
    if right == (0.0, 0.0, 0.0):
        right = (1.0, 0.0, 0.0)
    up = cross(fwd, right)

    def to_cam(p):
        d = (p[0] - cam_pos[0], p[1] - cam_pos[1], p[2] - cam_pos[2])
        return (dot(d, right), dot(d, up), dot(d, fwd))

    f = (WIDTH / 2) / math.tan(math.radians(fov / 2))
    polys = []
    for cf, size, color, transp in parts:
        pos, rot = cf
        rot = rot if rot else ((1, 0, 0), (0, 1, 0), (0, 0, 1))
        hs = (size[0] / 2, size[1] / 2, size[2] / 2)
        corners = []
        for sx in (-1, 1):
            for sy_ in (-1, 1):
                for sz in (-1, 1):
                    lv = (sx * hs[0], sy_ * hs[1], sz * hs[2])
                    wv = rot_apply(rot, lv)
                    corners.append((pos[0] + wv[0], pos[1] + wv[1], pos[2] + wv[2]))
        cam = [to_cam(c) for c in corners]
        if all(c[2] < 0.8 for c in cam):
            continue
        if any(c[2] < 0.8 for c in cam):
            # crude near clip
            cam = [c if c[2] >= 0.8 else (c[0], c[1], 0.8) for c in cam]
        depth = sum(c[2] for c in cam) / len(cam)
        screen = [(WIDTH / 2 + c[0] * f / c[2], HEIGHT / 2 - c[1] * f / c[2]) for c in cam]
        # face normal lighting from the top face normal in world space
        nrm = rot_apply(rot, (0, 1, 0))
        light = max(0.0, nrm[0] * sun[0] + nrm[1] * sun[1] + nrm[2] * sun[2])
        shade = 0.38 + 0.62 * light
        fog = max(0.0, min(0.85, (depth - 180) / 1500))
        col = tuple(int(min(255, color[i] * shade * (1 - fog) + 92 * fog)) for i in range(3))
        hull = convex_hull(screen)
        if len(hull) >= 3:
            alpha = int(255 * (1 - transp))
            polys.append((depth, hull, col + (alpha,)))
    polys.sort(key=lambda p: -p[0])
    for depth, hull, col in polys:
        draw.polygon(hull, fill=col, outline=(col[0] // 3, col[1] // 3, col[2] // 3, 90))
    img.save(out)
    return out


def convex_hull(points):
    pts = sorted(set((round(p[0], 2), round(p[1], 2)) for p in points))
    if len(pts) <= 2:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower = []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    upper = []
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "ASHES_OF_HUMANITY.rbxl")
    outs = os.path.join(HERE, "..", "docs", "preview")
    os.makedirs(outs, exist_ok=True)
    roots = binary.read_file(path)
    parts = []
    for r in roots:
        if r.props.get("Name") == "Workspace":
            collect_parts(r, parts)
    print("parts:", len(parts))
    # explicit camera yaws toward each region centre
    views = [
        ("overview", (0, 900, 1500), (0, 0, 0), 70),
        ("suburbs", (0, 30, 120), (0, 6, -260), 74),
        ("house", (44, 9, -236), (0, 6, -262), 76),
        ("forest", (-1150, 24, 560), (-1150, 10, 0), 74),
        ("industrial", (1050, 34, 330), (1050, 12, -180), 74),
        ("city", (-120, 30, -700), (-120, 20, -1180), 76),
        ("military", (1150, 30, 1500), (1150, 10, 900), 74),
        ("wasteland", (-1200, 26, -700), (-1200, 14, -1150), 74),
        ("shelter", (40, -14, 20), (40, -16, -60), 80),
        ("city_street", (-120, 12, -980), (-120, 14, -1180), 72),
    ]
    for name, cam, target, fov in views:
        out = os.path.join(outs, name + ".png")
        render(parts, cam, target, fov, out)
        print("wrote", out)


if __name__ == "__main__":
    main()
