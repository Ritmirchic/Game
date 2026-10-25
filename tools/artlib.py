"""
SPACE MINER — procedural art utilities.

Everything the game draws is generated from code: no external image files, no
stock assets.  Images are built with numpy (signed-distance + noise fields),
composited in linear-ish space and supersampled for clean antialiased edges.
"""
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

SS = 4  # supersampling factor for vector shapes


# --------------------------------------------------------------------------- #
#  small helpers
# --------------------------------------------------------------------------- #
def clamp01(x):
    return np.clip(x, 0.0, 1.0)


def smoothstep(t):
    t = clamp01(t)
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def hex2rgb(h):
    h = h.lstrip('#')
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float64) / 255.0


def rgb2hex(c):
    c = np.clip(c, 0, 1)
    return '#%02x%02x%02x' % tuple(int(round(v * 255)) for v in c)


def mix(c1, c2, t):
    return lerp(np.asarray(c1, dtype=np.float64), np.asarray(c2, dtype=np.float64), t)


def value_noise(w, h, freq, seed, octaves=1, gain=0.5, lacunarity=2.0):
    """Cheap multi-octave value noise on a w*h grid, returns [0,1]."""
    rng = np.random.default_rng(seed)
    total = np.zeros((h, w), dtype=np.float64)
    amp = 1.0
    norm = 0.0
    f = float(freq)
    for _ in range(octaves):
        gw = max(2, int(f) + 1)
        gh = max(2, int(f * h / max(1, w)) + 1)
        grid = rng.random((gh, gw))
        # bilinear upsample of the lattice
        xs = np.linspace(0, gw - 1, w)
        ys = np.linspace(0, gh - 1, h)
        x0 = np.floor(xs).astype(int)
        y0 = np.floor(ys).astype(int)
        x1 = np.minimum(x0 + 1, gw - 1)
        y1 = np.minimum(y0 + 1, gh - 1)
        fx = (xs - x0)[None, :]
        fy = (ys - y0)[:, None]
        g00 = grid[np.ix_(y0, x0)]
        g10 = grid[np.ix_(y0, x1)]
        g01 = grid[np.ix_(y1, x0)]
        g11 = grid[np.ix_(y1, x1)]
        top = g00 * (1 - fx) + g10 * fx
        bot = g01 * (1 - fx) + g11 * fx
        layer = top * (1 - fy) + bot * fy
        # smooth the lattice interpolation
        layer = smoothstep(layer)
        total += layer * amp
        norm += amp
        amp *= gain
        f *= lacunarity
    return total / norm


def periodic_noise(w, h, cells_x, cells_y, seed):
    """Seamless (tileable) value noise using a wrapping lattice."""
    rng = np.random.default_rng(seed)
    grid = rng.random((cells_y, cells_x))
    xs = np.linspace(0, cells_x, w, endpoint=False)
    ys = np.linspace(0, cells_y, h, endpoint=False)
    x0 = np.floor(xs).astype(int) % cells_x
    y0 = np.floor(ys).astype(int) % cells_y
    x1 = (x0 + 1) % cells_x
    y1 = (y0 + 1) % cells_y
    fx = smoothstep(xs - np.floor(xs))[None, :]
    fy = smoothstep(ys - np.floor(ys))[:, None]
    g00 = grid[np.ix_(y0, x0)]
    g10 = grid[np.ix_(y0, x1)]
    g01 = grid[np.ix_(y1, x0)]
    g11 = grid[np.ix_(y1, x1)]
    top = g00 * (1 - fx) + g10 * fx
    bot = g01 * (1 - fx) + g11 * fx
    return top * (1 - fy) + bot * fy


def fbm_periodic(w, h, base_cells, seed, octaves=5, gain=0.5):
    total = np.zeros((h, w))
    amp, norm, cells = 1.0, 0.0, base_cells
    for o in range(octaves):
        total += periodic_noise(w, h, max(2, cells), max(2, int(cells * h / w)), seed + o * 771) * amp
        norm += amp
        amp *= gain
        cells = int(cells * 2)
    return total / norm


class Canvas:
    """RGBA float canvas in [0,1], drawn at SS x resolution."""

    def __init__(self, w, h, ss=SS):
        self.w, self.h, self.ss = w, h, ss
        self.rgb = np.zeros((h * ss, w * ss, 3))
        self.a = np.zeros((h * ss, w * ss))

    def mask_from_draw(self, fn):
        img = Image.new('L', (self.w * self.ss, self.h * self.ss), 0)
        d = ImageDraw.Draw(img)
        fn(d, self.ss)
        return np.asarray(img, dtype=np.float64) / 255.0

    def paint(self, mask, color, alpha=1.0, over=True):
        m = (mask * alpha)[..., None]
        col = np.asarray(color, dtype=np.float64)
        if over:
            self.rgb = self.rgb * (1 - m) + col * m
            self.a = clamp01(self.a + mask * alpha)
        else:
            self.rgb = self.rgb + col * m
            self.a = clamp01(self.a + mask * alpha)

    def shade(self, mask, fn):
        """Multiply existing colour under a mask by fn(rgb, x, y)."""
        ys, xs = np.mgrid[0:self.h * self.ss, 0:self.w * self.ss]
        out = fn(self.rgb, xs, ys)
        m = mask[..., None]
        self.rgb = self.rgb * (1 - m) + out * m

    def add(self, amount_rgb, mask=None):
        add = amount_rgb
        if mask is not None:
            add = add * mask[..., None]
        self.rgb = self.rgb + add

    def downsample(self):
        """Box downsample to final resolution, returns PIL RGBA image."""
        ss = self.ss
        h, w = self.h, self.w
        rgb = self.rgb.reshape(h, ss, w, ss, 3).mean(axis=(1, 3))
        a = self.a.reshape(h, ss, w, ss).mean(axis=(1, 3))
        out = np.concatenate([clamp01(rgb), clamp01(a)[..., None]], axis=2)
        arr = (clamp01(out) * 255.0 + 0.5).astype(np.uint8)
        return Image.fromarray(arr, 'RGBA')


# --------------------------------------------------------------------------- #
#  texture helpers
# --------------------------------------------------------------------------- #
def normal_from_height(height):
    gy, gx = np.gradient(height)
    nx = -gx * 2.0
    ny = -gy * 2.0
    nz = np.ones_like(height)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    return nx / ln, ny / ln, nz / ln


def light_shade(nx, ny, nz, lx=-0.55, ly=-0.65, lz=0.62, ambient=0.22):
    ln = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ln, ly / ln, lz / ln
    lam = clamp01(nx * lx + ny * ly + nz * lz)
    return ambient + (1 - ambient) * lam ** 1.15


def radial(w, h, cx=None, cy=None, power=1.0):
    if cx is None:
        cx, cy = w / 2.0, h / 2.0
    ys, xs = np.mgrid[0:h, 0:w]
    d = np.sqrt(((xs - cx) / (w / 2)) ** 2 + ((ys - cy) / (h / 2)) ** 2)
    return clamp01(1.0 - d) ** power
