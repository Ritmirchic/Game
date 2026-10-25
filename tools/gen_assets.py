#!/usr/bin/env python3
"""Asset build step: generates all procedural textures, UI art and font atlases.

Usage: python3 tools/gen_assets.py [--out assets] [--only substr]
Requires: pillow, numpy  (build-time only; the shipped game needs no Python)
"""
import os
import sys
import time
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from texlib import height_to_normal, to_u8
import tex_world
import tex_props
import tex_char
import tex_ui


def write_rgba(path, arr, alpha=None):
    arr = np.clip(arr, 0, 1)
    h, w = arr.shape[:2]
    out = np.zeros((h, w, 4), np.uint8)
    out[..., 0] = (arr[..., 0] * 255 + 0.5).astype(np.uint8)
    out[..., 1] = (arr[..., 1] * 255 + 0.5).astype(np.uint8)
    out[..., 2] = (arr[..., 2] * 255 + 0.5).astype(np.uint8)
    if alpha is None:
        out[..., 3] = 255
    else:
        out[..., 3] = (np.clip(alpha, 0, 1) * 255 + 0.5).astype(np.uint8)
    Image.fromarray(out, "RGBA").save(path, optimize=True)


def downscale(arr, size):
    if arr.shape[0] == size:
        return arr
    img = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
    img = img.resize((size, size), Image.LANCZOS)
    return np.asarray(img, np.float32) / 255.0


def main():
    out_dir = "assets"
    only = None
    args = sys.argv[1:]
    for i, a in enumerate(args):
        if a == "--out":
            out_dir = args[i + 1]
        if a == "--only":
            only = args[i + 1]
    tex_dir = os.path.join(out_dir, "tex")
    os.makedirs(tex_dir, exist_ok=True)

    mats = {}
    mats.update(tex_world.materials())
    mats.update(tex_props.materials())
    mats.update(tex_char.materials())

    manifest = []
    total = 0
    t0 = time.time()
    for name, spec in sorted(mats.items()):
        fn, size, repeat, nstrength, is_alpha = spec
        if only and only not in name:
            continue
        ts = time.time()
        u = np.linspace(0, 1, size, endpoint=False, dtype=np.float32)
        uu, vv = np.meshgrid(u, u)
        res = fn(uu, vv, size)
        albedo = np.asarray(res["albedo"], np.float32)
        if albedo.shape[0] != size:
            albedo = downscale(albedo, size)
        def _arr(x):
            if x is None:
                return None
            a = np.asarray(x, np.float32)
            if a.ndim == 0:
                a = np.full((size, size), float(a), np.float32)
            return a
        alpha = _arr(res.get("alpha", None))
        rough = _arr(res.get("rough", None))
        height = _arr(res.get("height", None))

        alb_path = f"tex/{name}.png"
        write_rgba(os.path.join(out_dir, alb_path), albedo, alpha)

        norm_path = "-"
        if height is not None and not is_alpha:
            nsz = max(128, size // 2)
            hh = downscale(height[..., None].repeat(3, 2), nsz)[..., 0]
            nrm = height_to_normal(hh, nstrength)
            norm_path = f"tex/{name}_n.png"
            write_rgba(os.path.join(out_dir, norm_path), nrm, None)

        rgh_path = "-"
        if rough is not None:
            rsz = 128
            rr = downscale(rough[..., None].repeat(3, 2), rsz)[..., 0]
            rgh_path = f"tex/{name}_r.png"
            write_rgba(os.path.join(out_dir, rgh_path), np.stack([rr, rr, rr], -1), None)

        flags = "A" if is_alpha else ""
        manifest.append(f"T {name} {alb_path} {norm_path} {rgh_path} {repeat:.3f} {flags}")
        sz = (os.path.getsize(os.path.join(out_dir, alb_path)) +
              (os.path.getsize(os.path.join(out_dir, norm_path)) if norm_path != "-" else 0) +
              (os.path.getsize(os.path.join(out_dir, rgh_path)) if rgh_path != "-" else 0))
        total += sz
        print(f"  {name:24s} {size:4d}  {sz/1024:7.1f} KB  {time.time()-ts:5.2f}s")

    # ---- UI art
    ui_dir = os.path.join(out_dir, "ui")
    os.makedirs(ui_dir, exist_ok=True)
    if not only or "ui" in only:
        tex_ui.icon_atlas(os.path.join(ui_dir, "icons.png"))
        tex_ui.ui_logo(os.path.join(ui_dir, "logo.png"))
        tex_ui.ui_panel(os.path.join(ui_dir, "panel.png"))
        tex_ui.ui_panel_solid(os.path.join(ui_dir, "panel_solid.png"))
        tex_ui.ui_crosshair(os.path.join(ui_dir, "crosshair.png"))
        tex_ui.ui_blood_overlay(os.path.join(ui_dir, "blood.png"))
        tex_ui.ui_noise(os.path.join(ui_dir, "noise.png"))
        tex_ui.ui_vignette(os.path.join(ui_dir, "vignette.png"))
        tex_ui.ui_flash(os.path.join(ui_dir, "flash.png"))
        tex_ui.ui_smoke(os.path.join(ui_dir, "smoke.png"))
        tex_ui.ui_rain(os.path.join(ui_dir, "rain.png"))
        print("  ui art written")

    # ---- fonts
    font_dir = os.path.join(out_dir, "fonts")
    os.makedirs(font_dir, exist_ok=True)
    if not only or "font" in only:
        tex_ui.font_atlas(tex_ui.FONT_BOLD, 34, os.path.join(font_dir, "bold.png"),
                          os.path.join(font_dir, "bold.txt"), "bold")
        tex_ui.font_atlas(tex_ui.FONT_REG, 26, os.path.join(font_dir, "reg.png"),
                          os.path.join(font_dir, "reg.txt"), "reg")
        tex_ui.font_atlas(tex_ui.FONT_MONO, 28, os.path.join(font_dir, "mono.png"),
                          os.path.join(font_dir, "mono.txt"), "mono")
        tex_ui.font_atlas(tex_ui.FONT_BOLD, 64, os.path.join(font_dir, "title.png"),
                          os.path.join(font_dir, "title.txt"), "title")
        print("  fonts written")

    with open(os.path.join(out_dir, "textures.txt"), "w") as f:
        f.write("# name albedo normal rough repeat flags\n")
        f.write("\n".join(manifest) + "\n")

    print(f"total texture payload: {total/1024/1024:.1f} MB in {time.time()-t0:.1f}s")


if __name__ == "__main__":
    main()
