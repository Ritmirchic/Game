#!/usr/bin/env python3
"""
Looping 16:9 desktop-wallpaper animation from a single reference frame.

Pipeline
--------
1. base plate  : reference PNG -> high quality Lanczos upscale (+ micro grain/sharpening)
2. displacement: dense backward warp, driven by a smooth weight field around the
                  character -> the knight rises & descends while the gothic hall
                  stays planted (no layer seams, no holes)
3. light grade : exposure / shadow lift / saturation soften / bloom breathing
4. output      : 10 keyframes + mp4 loop(s) + wallpaper stills

Everything is driven by one phase variable e in [0..1] (10 keyframes, loop-safe).

Usage:
  python build.py keyframes            # 10 keyframe jpgs + contact sheet
  python build.py video                # smooth 30fps loop mp4s
  python build.py stills               # 4K / 8K / 16K wallpaper stills
  python build.py all
"""

from __future__ import annotations

import os
import sys
import subprocess
import numpy as np
import cv2
import imageio_ffmpeg

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ROOT, "src", "reference.png")
PLATE = os.path.join(ROOT, "src", "plate_4k.png")
KFRAMES = os.path.join(ROOT, "keyframes")
VIDEO = os.path.join(ROOT, "video")
STILLS = os.path.join(ROOT, "stills")
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()

SRC_W, SRC_H = 1376, 768          # reference resolution
MASTER = (3840, 2160)             # 16:9 render resolution for keyframes / 4K video

# --- animation constants (expressed in reference-image pixels, scaled later) ---
RISE_PX = 16.0                     # max upward travel of the knight, ref px
SWAY_PX = 5.0                     # lateral drift on the way up/down, ref px
BRIGHT_MAX = 0.10                 # exposure lift at the top of the breath
SHADOW_LIFT = 0.016               # extra lift applied to the shadows
SAT_SOFTEN = 0.09                 # slight desaturation when "quiet"
BLOOM_BASE, BLOOM_MAX = 0.15, 0.07
GRAIN = 0.0060
N_KEY = 10


# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #
def phase(i: int, n: int = N_KEY) -> float:
    """Cosine ease, 0 at frame 0 and frame n, 1 at the half-way frame."""
    return 0.5 - 0.5 * np.cos(2.0 * np.pi * i / n)


def sway(i: int, n: int = N_KEY) -> float:
    """Secondary motion, -1 .. 1. Zero at the top of the breath, opposite
    sign while rising vs falling, so all 10 keyframes are distinct."""
    return float(np.sin(2.0 * np.pi * i / n))


def upscale(img: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    """Lanczos upscale + gentle detail restoration (the source is small)."""
    out = cv2.resize(img, size, interpolation=cv2.INTER_LANCZOS4)
    small = cv2.resize(out, (size[0] // 2, size[1] // 2), interpolation=cv2.INTER_AREA)
    blurred = cv2.resize(small, size, interpolation=cv2.INTER_CUBIC)
    out = cv2.addWeighted(out, 1.35, blurred, -0.35, 0.0)   # USM-ish, keeps edges crisp
    return np.clip(out, 0, 255)


def build_plate(size: tuple[int, int]) -> np.ndarray:
    src = cv2.imread(SRC, cv2.IMREAD_COLOR)
    img = upscale(src, size)
    img = img.astype(np.float32) / 255.0
    # fixed fine grain: breaks up the "digital smooth" of the upscale
    rng = np.random.default_rng(7)
    grain = rng.normal(0.0, 1.0, (size[1], size[0], 1)).astype(np.float32)
    grain = cv2.GaussianBlur(grain[..., 0], (0, 0), 1.1)[..., None]
    img = np.clip(img + grain * 0.010, 0.0, 1.0)
    # subtle vignette to settle the upscaled edges
    h, w = size[1], size[0]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.sqrt(((xx - w * 0.5) / (w * 0.5)) ** 2 + ((yy - h * 0.5) / (h * 0.5)) ** 2)
    vign = 1.0 - 0.10 * np.clip((r - 0.55) / 0.85, 0, 1) ** 1.6
    img *= vign[..., None]
    return img.astype(np.float32)


def character_weight(size: tuple[int, int]) -> np.ndarray:
    """
    Smooth 0..1 field covering the knight, his blade and the burning hand.
    Built as a soft-max of gaussians placed in reference-image coordinates.
    Outside the character it decays to ~0, so the architecture never moves.
    """
    w, h = size
    sx, sy = w / SRC_W, h / SRC_H
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    xn, yn = xx / sx, yy / sy          # back to reference pixel space

    # (cx, cy, sigma_x, sigma_y) in reference px
    blobs = [
        (1062, 600, 175, 195),   # torso / mantle
        (1035, 250, 78, 135),    # tall pointed helm
        (1235, 205, 115, 145),   # burning hand + fire column
        (1205, 345, 95, 85),     # right pauldron / gauntlet
        (795, 300, 34, 115),     # blade, upper part
        (818, 420, 34, 115),     # blade, middle
        (848, 545, 36, 95),      # blade, lower + guard
        (860, 615, 55, 70),      # gauntlet on the hilt
    ]
    acc = np.zeros_like(xn, dtype=np.float32)
    for cx, cy, sgx, sgy in blobs:
        acc += np.exp(-0.5 * (((xn - cx) / sgx) ** 2 + ((yn - cy) / sgy) ** 2))
    wgt = 1.0 - np.exp(-acc)                      # smooth, bounded to 1
    wgt = cv2.GaussianBlur(wgt, (0, 0), 3.0 * sx)  # extra smoothing -> no creases
    return np.clip(wgt, 0.0, 1.0).astype(np.float32)


def warp(base: np.ndarray, wgt: np.ndarray, rise: float, sway: float = 0.0) -> np.ndarray:
    """
    Upward displacement field, sampled with explicit bilinear gather.

    dy > 0 means "copy content from below", i.e. the picture travels up.
    cv2.remap is avoided on purpose: the headless OpenCV build used here can
    emit NaNs from its float-map code path on repeated calls, and this gather
    is both deterministic and fast enough for 4K frames.
    """
    h, w = base.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    src_y = yy + rise * wgt                       # where each pixel is read from
    src_x = xx + sway * wgt
    y0 = np.floor(src_y)
    x0 = np.floor(src_x)
    fy = (src_y - y0)[..., None]
    fx = (src_x - x0)[..., None]
    r0 = np.clip(y0, 0, h - 1).astype(np.int32)
    r1 = np.clip(y0 + 1, 0, h - 1).astype(np.int32)
    c0 = np.clip(x0, 0, w - 1).astype(np.int32)
    c1 = np.clip(x0 + 1, 0, w - 1).astype(np.int32)
    a = base[r0, c0]
    b = base[r0, c1]
    c = base[r1, c0]
    d = base[r1, c1]
    top = a + (b - a) * fx
    bot = c + (d - c) * fx
    return top + (bot - top) * fy


def glow_mask(size: tuple[int, int]) -> np.ndarray:
    """Where the red light lives (blade + burning hand), for the ember pulse."""
    w, h = size
    sx, sy = w / SRC_W, h / SRC_H
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    xn, yn = xx / sx, yy / sy
    m = np.zeros_like(xn, dtype=np.float32)
    for cx, cy, sgx, sgy in [
        (1230, 200, 105, 120),
        (1200, 330, 80, 70),
        (800, 330, 45, 170),
        (845, 540, 45, 90),
    ]:
        m += np.exp(-0.5 * (((xn - cx) / sgx) ** 2 + ((yn - cy) / sgy) ** 2))
    m = np.clip(m, 0, 1)
    m = cv2.GaussianBlur(m, (0, 0), 26.0 * sx)
    m /= max(m.max(), 1e-6)
    return m.astype(np.float32)


def bloom(img: np.ndarray, strength: float) -> np.ndarray:
    """Cheap wide bloom: downsample -> blur -> add (keeps highlights gentle)."""
    h, w = img.shape[:2]
    small = cv2.resize(img, (w // 8, h // 8), interpolation=cv2.INTER_AREA)
    lum = small.max(axis=2)
    thr = np.clip((lum - 0.52) / 0.48, 0, 1)[..., None]
    hl = small * thr
    hl = cv2.GaussianBlur(hl, (0, 0), 7.0)
    hl = cv2.resize(hl, (w, h), interpolation=cv2.INTER_LINEAR)
    return img + hl * strength


def grade(img: np.ndarray, e: float, ember: np.ndarray, sw: float = 0.0) -> np.ndarray:
    """Exposure / shadow lift / softer colour / ember pulse for phase e."""
    out = img * (1.0 + BRIGHT_MAX * e)
    out = out + SHADOW_LIFT * e * (1.0 - out)          # shadows open up first
    # luminance-preserving desaturation ("quieter" look at the top of the breath)
    luma = out @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    out = luma[..., None] + (out - luma[..., None]) * (1.0 - SAT_SOFTEN * e)
    # red lights breathe a touch more than the stone
    out = out * (1.0 + 0.14 * e * ember[..., None] + 0.045 * sw * ember[..., None])
    out = bloom(out, BLOOM_BASE + BLOOM_MAX * e)
    return np.clip(out, 0.0, 1.0)


def finalize(img: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    """8-bit with ordered-ish dither so the dark gradients don't band."""
    h, w = size[1], size[0]
    rng = np.random.default_rng(3)
    dither = (rng.random((h, w, 1), dtype=np.float32) - 0.5) * (1.0 / 255.0)
    out = np.clip(img + dither + np.float32(GRAIN / 2) * (np.random.default_rng(5)
                   .normal(0, 1, (h, w, 1)).astype(np.float32)), 0, 1)
    return (out * 255.0 + 0.5).astype(np.uint8)


# --------------------------------------------------------------------------- #
# render
# --------------------------------------------------------------------------- #
class Renderer:
    def __init__(self, size=MASTER):
        self.size = size
        print(f"[plate] {size[0]}x{size[1]}", flush=True)
        self.base = build_plate(size)
        self.wgt = character_weight(size)
        self.ember = glow_mask(size)
        self.rise = RISE_PX * size[1] / SRC_H

    def frame(self, e: float, sw: float = 0.0) -> np.ndarray:
        warped = warp(self.base, self.wgt, self.rise * e,
                      SWAY_PX * self.size[0] / SRC_W * sw)
        graded = grade(warped, e, self.ember, sw)
        return finalize(graded, self.size)

    def key(self, i: int, n: int = N_KEY) -> np.ndarray:
        """Keyframe i of n (0-based), loop-safe."""
        return self.frame(phase(i, n), sway(i, n))


def cmd_keyframes():
    os.makedirs(KFRAMES, exist_ok=True)
    r = Renderer()
    thumbs = []
    for i in range(N_KEY):
        e = phase(i)
        img = r.key(i)
        p = os.path.join(KFRAMES, f"frame_{i + 1:02d}.jpg")
        cv2.imwrite(p, img, [cv2.IMWRITE_JPEG_QUALITY, 97])
        thumbs.append(cv2.resize(img, (480, 270), interpolation=cv2.INTER_AREA))
        print(f"[key] {p}  phase={e:.3f}", flush=True)
    sheet = np.vstack([np.hstack(thumbs[k:k + 5]) for k in (0, 5)])
    cv2.imwrite(os.path.join(KFRAMES, "contact_sheet.jpg"), sheet,
                [cv2.IMWRITE_JPEG_QUALITY, 92])


def encode(frames_iter, fps: int, out_path: str, crf: int = 18, fade_head=0):
    """Pipe raw BGR frames into x264 (yuv420p, faststart, no audio)."""
    size = MASTER
    cmd = [
        FFMPEG, "-y", "-hide_banner", "-loglevel", "error",
        "-f", "rawvideo", "-pix_fmt", "bgr24",
        "-s", f"{size[0]}x{size[1]}", "-r", str(fps), "-i", "-",
        "-an", "-c:v", "libx264", "-preset", "medium", "-crf", str(crf),
        "-pix_fmt", "yuv420p", "-profile:v", "high", "-level", "5.1",
        "-movflags", "+faststart", "-g", str(fps * 2),
        out_path,
    ]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    n = 0
    for fr in frames_iter:
        proc.stdin.write(fr.tobytes())
        n += 1
    proc.stdin.close()
    proc.wait()
    print(f"[video] {out_path}  {n} frames @ {fps}fps", flush=True)


def cmd_video():
    os.makedirs(VIDEO, exist_ok=True)
    r = Renderer()
    fps = 30
    seconds = 10
    total = fps * seconds

    def smooth():
        for k in range(total):
            yield r.frame(phase(k, total), sway(k, total))

    encode(smooth(), fps, os.path.join(VIDEO, "anvilight_knight_loop_4k.mp4"), crf=18)

    # honest "10 keyframes connected" version: a ping-pong over exactly 10 unique frames
    def pingpong():
        for i in range(N_KEY):
            yield r.key(i)

    encode(pingpong(), 5, os.path.join(VIDEO, "anvilight_knight_loop_10frames.mp4"), crf=17)


def cmd_stills():
    os.makedirs(STILLS, exist_ok=True)
    src = cv2.imread(SRC, cv2.IMREAD_COLOR)
    ember4 = glow_mask((3840, 2160))
    for name, (w, h) in {
        "wallpaper_3840x2160": (3840, 2160),
        "wallpaper_7680x4320": (7680, 4320),
        "wallpaper_15360x8640": (15360, 8640),
    }.items():
        if (w, h) == (3840, 2160):
            img = build_plate((w, h))
            img = grade(img, 1.0, glow_mask((w, h)))
            img = finalize(img, (w, h))
        else:
            img = upscale(src, (w, h))
            a = img.astype(np.float32) / 255.0
            a = grade(a, 1.0, np.zeros((h, w), np.float32))
            img = finalize(a, (w, h))
        p = os.path.join(STILLS, name + ".png")
        cv2.imwrite(p, img, [cv2.IMWRITE_PNG_COMPRESSION, 9])
        print(f"[still] {p}  {os.path.getsize(p) / 1e6:.1f} MB", flush=True)


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what in ("keyframes", "all"):
        cmd_keyframes()
    if what in ("video", "all"):
        cmd_video()
    if what in ("stills", "all"):
        cmd_stills()
