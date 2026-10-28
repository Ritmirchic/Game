#!/usr/bin/env python3
"""
16K wallpaper still built from the 8K master, in row tiles.

A full 15360x8640 float32 buffer is ~1.6 GB per copy and this box has 4 GB,
so the upscale/sharpening/grain pass is done in overlapping horizontal strips
and written straight to disk.
"""

import os
import numpy as np
import cv2

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC8K = os.path.join(ROOT, "stills", "wallpaper_7680x4320.png")
OUT_PNG = os.path.join(ROOT, "stills", "wallpaper_15360x8640.png")
OUT_JPG = os.path.join(ROOT, "stills", "wallpaper_15360x8640.jpg")

TARGET = (15360, 8640)
TILE_H = 540          # output rows per strip
OVERLAP = 40          # extra source rows to avoid seams


def main():
    src = cv2.imread(SRC8K, cv2.IMREAD_COLOR)
    assert src is not None, "missing 8K master"
    sh, sw = src.shape[:2]
    tw, th = TARGET
    sy, sx = th / sh, tw / sw

    # stream raw rows straight into ffmpeg: OpenCV's VideoWriter tops out well
    # below 16K, and a single 16K PNG buffer is too big to hold in this much RAM.
    import imageio_ffmpeg
    import subprocess
    ff = imageio_ffmpeg.get_ffmpeg_exe()
    proc = subprocess.Popen(
        [ff, "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "bgr24",
         "-s", f"{tw}x{th}", "-i", "-", "-frames:v", "1",
         "-compression_level", "3", OUT_PNG],
        stdin=subprocess.PIPE,
    )
    rng = np.random.default_rng(11)

    for y0 in range(0, th, TILE_H):
        y1 = min(y0 + TILE_H, th)
        # source strip (plus overlap)
        s0 = max(int((y0 - OVERLAP) / sy), 0)
        s1 = min(int(np.ceil((y1 + OVERLAP) / sy)), sh)
        strip = src[s0:s1]
        up = cv2.resize(strip, (tw, int(round((s1 - s0) * sy))), interpolation=cv2.INTER_LANCZOS4)

        # gentle detail restoration (unsharp on the tile)
        blur = cv2.GaussianBlur(up, (0, 0), 1.6)
        up = cv2.addWeighted(up, 1.22, blur, -0.22, 0.0)

        # grain + dither so the huge dark gradients don't band
        f = up.astype(np.float32)
        f += rng.normal(0, 3.0, (f.shape[0], f.shape[1], 1)).astype(np.float32)
        f += (rng.random((f.shape[0], f.shape[1], 1), dtype=np.float32) - 0.5)
        up = np.clip(f, 0, 255).astype(np.uint8)

        top = int(round(s0 * sy))
        oy0 = max(y0 - top, 0)
        tile = up[oy0:oy0 + (y1 - y0)]
        assert tile.shape[0] == (y1 - y0), (tile.shape, y0, y1)
        proc.stdin.write(tile.tobytes())
        print(f"[16k] rows {y0}-{y1}", flush=True)

    proc.stdin.close()
    if proc.wait() != 0:
        raise SystemExit("ffmpeg failed writing the 16K PNG")
    subprocess.run([ff, "-y", "-loglevel", "error", "-i", OUT_PNG,
                    "-q:v", "2", OUT_JPG], check=True)
    for p in (OUT_PNG, OUT_JPG):
        print(f"[16k] {p}  {os.path.getsize(p)/1e6:.1f} MB", flush=True)


if __name__ == "__main__":
    main()
