#!/usr/bin/env python3
"""SPACE MINER — build icongen: turn the 256px logo mark into a multi-size .ico.

Windows picks the closest size from the icon group, so we bake 16/24/32/48/64/
128/256 px images with per-size sharpening; the small ones are drawn crisper by
mild contrast boost instead of a naive downscale.
"""
import os
import sys

from PIL import Image, ImageEnhance

SIZES = [16, 24, 32, 48, 64, 128, 256]


def build(src, dst):
    base = Image.open(src).convert('RGBA')
    base = base.resize((256, 256), Image.LANCZOS)
    frames = []
    for s in SIZES:
        im = base.resize((s, s), Image.LANCZOS)
        if s <= 48:
            # tiny icons need more punch to stay readable in the taskbar
            im = ImageEnhance.Contrast(im).enhance(1.18)
            im = ImageEnhance.Sharpness(im).enhance(1.6)
        frames.append(im)
    frames[-1].save(dst, format='ICO', sizes=[(s, s) for s in SIZES],
                    append_images=frames[:-1])
    print(f'wrote {dst} ({os.path.getsize(dst)} bytes)')


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else '../assets/gen/icon_256.png',
          sys.argv[2] if len(sys.argv) > 2 else 'build/gen/spaceminer.ico')
