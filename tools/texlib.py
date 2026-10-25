"""texlib — small procedural texture toolkit (numpy only, no external assets).

All generators are tileable: they use periodic noise / wrapped coordinates so
textures repeat seamlessly on level geometry.
"""
import numpy as np

# ---------------------------------------------------------------- noise
def _hash2(ix, iy, seed):
    h = (ix * 374761393 + iy * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF) / 16777216.0

def pnoise(x, y, period, seed=0):
    """Periodic value noise. x,y in lattice units; period = lattice period."""
    ix = np.floor(x).astype(np.int64)
    iy = np.floor(y).astype(np.int64)
    fx = x - ix
    fy = y - iy
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    ix0 = ix % period
    iy0 = iy % period
    ix1 = (ix + 1) % period
    iy1 = (iy + 1) % period
    a = _hash2(ix0, iy0, seed)
    b = _hash2(ix1, iy0, seed)
    c = _hash2(ix0, iy1, seed)
    d = _hash2(ix1, iy1, seed)
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy

def fbm(u, v, freq=4, octaves=5, seed=0, gain=0.5):
    """u,v in [0,1) tileable. Returns float array."""
    total = 0.0
    amp = 1.0
    norm = 0.0
    f = freq
    for o in range(octaves):
        total = total + pnoise(u * f, v * f, f, seed + o * 17) * amp
        norm += amp
        amp *= gain
        f *= 2
    return total / norm

def worley(u, v, cells=8, seed=0):
    """Tileable worley (cellular) F1 distance, normalized 0..1."""
    x = u * cells
    y = v * cells
    ix = np.floor(x).astype(np.int64)
    iy = np.floor(y).astype(np.int64)
    best = np.full(x.shape, 9.0)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            cx = ix + dx
            cy = iy + dy
            hx = _hash2(cx % cells, cy % cells, seed)
            hy = _hash2(cx % cells, cy % cells, seed + 91)
            px = cx + hx
            py = cy + hy
            d = np.hypot(x - px, y - py)
            best = np.minimum(best, d)
    return np.clip(best, 0, 1)

def uv_grid(size):
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    return x / size, y / size

def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a + 1e-9), 0, 1)
    return t * t * (3 - 2 * t)

def lerp(a, b, t):
    return a + (b - a) * t

def noise_dir(u, v, size, freq, seed=0, octaves=4):
    return fbm(u, v, freq, octaves, seed)

# ---------------------------------------------------------------- normal maps
def height_to_normal(h, strength=2.0, wrap=True):
    """h: (H,W) height. Returns (H,W,3) tangent-space normal in 0..1."""
    dx = np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)
    dy = np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)
    nx = -dx * strength
    ny = -dy * strength
    nz = np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / ln, ny / ln, nz / ln], axis=-1)
    return n * 0.5 + 0.5

def boxes_normal(size):
    """Flat normal map (128,128,255)."""
    n = np.zeros((size, size, 3), np.float32)
    n[..., 0] = 0.5
    n[..., 1] = 0.5
    n[..., 2] = 1.0
    return n

# ---------------------------------------------------------------- pattern helpers
def tile_pattern(u, v, n, grout=0.03):
    """Returns (mask of tile interior, distance-to-edge normalized)."""
    fu = (u * n) % 1.0
    fv = (v * n) % 1.0
    eu = np.minimum(fu, 1 - fu)
    ev = np.minimum(fv, 1 - fv)
    e = np.minimum(eu, ev)
    interior = smoothstep(grout * 0.4, grout, e)
    return interior, e

def planks(u, v, rows=6, offset=0.5, gap=0.012):
    """Horizontally laid planks. Returns (plank id, mask, local coords)."""
    fv = (v * rows) % 1.0
    row = np.floor(v * rows)
    shift = (row % 2) * offset
    fu = (u + shift) % 1.0
    edge = np.minimum(fv, 1 - fv)
    seam = np.minimum(fu, 1 - fu)
    m = smoothstep(0, gap, np.minimum(edge, seam * 0.5))
    return row, m, fu, fv

def bricks(u, v, rows=10, cols=5, mortar=0.04):
    fv = (v * rows) % 1.0
    row = np.floor(v * rows)
    fu = (u + (row % 2) * 0.5) % 1.0
    eu = np.minimum((fu * cols) % 1.0, 1 - (fu * cols) % 1.0) / cols
    ev = np.minimum(fv, 1 - fv) / rows
    e = np.minimum(eu, ev)
    return smoothstep(0.0, mortar, e), row, fu

def corrugate(u, v, freq=24):
    return 0.5 + 0.5 * np.sin(u * np.pi * 2 * freq)

def scratches(u, v, size, count=60, seed=0, length=0.25, width=1.2, angle=None, strength=1.0):
    """Line scratches mask in 0..1."""
    rng = np.random.default_rng(seed)
    mask = np.zeros((size, size), np.float32)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    for i in range(count):
        cx = rng.random()
        cy = rng.random()
        ang = rng.uniform(0, np.pi * 2) if angle is None else angle + rng.normal(0, 0.15)
        ln = rng.uniform(0.3, 1.0) * length
        dx, dy = np.cos(ang) * ln, np.sin(ang) * ln
        # distance from point to segment in uv space, wrapping
        px = xx / size - cx
        py = yy / size - cy
        px = (px + 0.5) % 1.0 - 0.5
        py = (py + 0.5) % 1.0 - 0.5
        t = np.clip((px * dx + py * dy) / (dx * dx + dy * dy + 1e-9), 0, 1)
        qx = px - dx * t
        qy = py - dy * t
        d = np.hypot(qx, qy)
        w = width / size
        mask = np.maximum(mask, smoothstep(w, 0.0, d) * rng.uniform(0.4, 1.0) * strength)
    return np.clip(mask, 0, 1)

def speckle(u, v, density=0.06, seed=0, size=512):
    n = fbm(u, v, 96, 3, seed + 5)
    return smoothstep(1.0 - density, 1.0, n)

def stains(u, v, seed=0, freq=3, octaves=4, thresh=0.55, soft=0.18):
    n = fbm(u, v, freq, octaves, seed)
    return smoothstep(thresh, thresh + soft, n)

def puddle_pattern(u, v, seed=0):
    n = fbm(u, v, 3, 5, seed)
    w = 1.0 - worley(u, v, 5, seed + 3)
    return smoothstep(0.5, 0.62, n * 0.7 + w * 0.45)

def edge_darken(u, v, amount=0.25, scale=1.0):
    """darker near texture edges/tile boundaries"""
    e = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v)) * scale
    return 1.0 - amount * (1 - smoothstep(0.0, 0.25, e))

# ---------------------------------------------------------------- colour helpers
def col(rgb):
    return np.array(rgb, np.float32)

def tint(img, rgb):
    return img * col(rgb)

def srgb(img):
    return np.clip(img, 0, 1)

def overlay(base, top, amount):
    return base * (1 - amount) + top * amount

def height_to_rough(base_rough, height, lo=0.55, hi=0.95):
    return np.clip(base_rough * lerp(lo, hi, height), 0, 1)

def to_u8(img):
    return (np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)

def rgb_img(r, g, b):
    return np.stack([r, g, b], axis=-1).astype(np.float32)

def fill(size, rgb, variation=0.0, seed=0, freq=6):
    u, v = uv_grid(size)
    base = np.ones((size, size, 3), np.float32) * col(rgb)
    if variation > 0:
        n = fbm(u, v, freq, 4, seed)
        base = base * (1.0 + (n - 0.5) * 2 * variation)
    return base
