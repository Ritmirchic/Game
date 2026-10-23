#!/usr/bin/env python3
"""
ASHES OF HUMANITY: LAST SIGNAL
Procedural Tileable PBR Texture Pipeline (512x512 x 4 Maps x 14 Materials = 56 PNGs)

Generates seamless periodic heightfields & masks using spectral FFT / periodic multi-octave noise,
deriving physically consistent:
  - ColorMap (_color.png)
  - Tangent-Space OpenGL/Roblox NormalMap (_normal.png)
  - RoughnessMap (_roughness.png)
  - MetalnessMap (_metalness.png)
"""

from __future__ import annotations
import json
import os
import numpy as np
from PIL import Image


def periodic_fbm(res: int, octaves: int = 5, persistence: float = 0.52, seed: int = 42, aniso: tuple[float, float] = (1.0, 1.0)) -> np.ndarray:
    """Generates a 100% seamlessly tileable 2D FBM field in [0, 1] via FFT synthesis."""
    rng = np.random.default_rng(seed)
    ky = np.fft.fftfreq(res) * res * aniso[1]
    kx = np.fft.fftfreq(res) * res * aniso[0]
    KX, KY = np.meshgrid(kx, ky)
    K = np.sqrt(KX**2 + KY**2)
    K[0, 0] = 1.0

    power = 1.0 / (K ** (1.35 + 0.85 * persistence))
    power[0, 0] = 0.0
    # Band-limit high frequencies to avoid aliasing
    power[K > res * 0.45] *= 0.15

    phases = rng.uniform(0, 2 * np.pi, size=(res, res))
    amps = rng.normal(size=(res, res)) * power
    spectrum = amps * (np.cos(phases) + 1j * np.sin(phases))
    field = np.real(np.fft.ifft2(spectrum))
    mn, mx = field.min(), field.max()
    return (field - mn) / max(1e-8, (mx - mn))


def height_to_normal(height: np.ndarray, strength: float = 3.0) -> np.ndarray:
    """Computes seamless periodic tangent-space normal map uint8 [H, W, 3]."""
    dh_dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * strength
    dh_dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * strength
    nx = -dh_dx
    ny = -dh_dy
    nz = np.ones_like(height)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx /= norm
    ny /= norm
    nz /= norm
    rgb = np.stack([(nx * 0.5 + 0.5) * 255.0, (ny * 0.5 + 0.5) * 255.0, (nz * 0.5 + 0.5) * 255.0], axis=-1)
    return np.clip(rgb, 0, 255).astype(np.uint8)


def save_pbr_set(out_dir: str, name: str, color: np.ndarray, height: np.ndarray, roughness: np.ndarray, metalness: np.ndarray, normal_strength: float = 3.2) -> dict:
    c_u8 = np.clip(color * 255.0, 0, 255).astype(np.uint8)
    n_u8 = height_to_normal(height, strength=normal_strength)
    r_u8 = np.clip(roughness * 255.0, 0, 255).astype(np.uint8)
    m_u8 = np.clip(metalness * 255.0, 0, 255).astype(np.uint8)

    paths = {
        "color": f"{name}_color.png",
        "normal": f"{name}_normal.png",
        "roughness": f"{name}_roughness.png",
        "metalness": f"{name}_metalness.png",
    }
    Image.fromarray(c_u8, mode="RGB").save(os.path.join(out_dir, paths["color"]), optimize=True)
    Image.fromarray(n_u8, mode="RGB").save(os.path.join(out_dir, paths["normal"]), optimize=True)
    Image.fromarray(r_u8, mode="L").save(os.path.join(out_dir, paths["roughness"]), optimize=True)
    Image.fromarray(m_u8, mode="L").save(os.path.join(out_dir, paths["metalness"]), optimize=True)
    return paths


def lerp_color(c0: tuple[float, float, float], c1: tuple[float, float, float], t: np.ndarray) -> np.ndarray:
    t3 = t[..., None]
    a = np.array(c0, dtype=np.float32)[None, None, :]
    b = np.array(c1, dtype=np.float32)[None, None, :]
    return a * (1.0 - t3) + b * t3


def generate_all(out_dir: str = "assets/textures", res: int = 512) -> None:
    os.makedirs(out_dir, exist_ok=True)
    y = np.linspace(0, 1, res, endpoint=False)
    x = np.linspace(0, 1, res, endpoint=False)
    X, Y = np.meshgrid(x, y)

    manifest = {}

    # 1. Rust
    n1 = periodic_fbm(res, seed=101, persistence=0.6)
    n2 = periodic_fbm(res, seed=102, persistence=0.35)
    oxide_mask = np.clip((n1 * 0.75 + n2 * 0.35 - 0.38) * 3.2, 0.0, 1.0)
    col = lerp_color((0.16, 0.17, 0.19), (0.48, 0.21, 0.09), oxide_mask)
    col = col * (0.78 + 0.35 * n2[..., None])
    h = n1 * 0.6 + oxide_mask * 0.4 - 0.25 * (n2 > 0.72)
    rough = 0.42 * (1.0 - oxide_mask) + 0.88 * oxide_mask
    metal = 0.82 * (1.0 - oxide_mask) + 0.08 * oxide_mask
    manifest["rust"] = save_pbr_set(out_dir, "rust", col, h, rough, metal, 4.5)

    # 2. Concrete
    n1 = periodic_fbm(res, seed=201, persistence=0.55)
    n2 = periodic_fbm(res, seed=202, persistence=0.25)
    cracks = np.exp(-((np.abs(n1 - 0.5)) * 48.0))
    col = lerp_color((0.36, 0.37, 0.36), (0.22, 0.23, 0.22), n2 * 0.65 + cracks * 0.35)
    h = n2 * 0.45 - cracks * 0.55
    rough = 0.82 + 0.14 * n2
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["concrete"] = save_pbr_set(out_dir, "concrete", col, h, rough, metal, 3.4)

    # 3. Brick
    rows = 8
    cols_n = 4
    ry = (Y * rows) % 1.0
    row_idx = np.floor(Y * rows)
    rx = ((X + (row_idx % 2) * 0.5 / cols_n) * cols_n) % 1.0
    mortar = (rx < 0.06) | (rx > 0.94) | (ry < 0.09) | (ry > 0.91)
    n1 = periodic_fbm(res, seed=301, persistence=0.5)
    brick_var = ((np.sin(row_idx * 12.9898 + np.floor((X + (row_idx % 2) * 0.125) * cols_n) * 78.233) * 43758.5453) % 1.0)
    bcol = lerp_color((0.39, 0.17, 0.13), (0.28, 0.13, 0.10), brick_var * 0.7 + n1 * 0.3)
    mcol = lerp_color((0.32, 0.31, 0.29), (0.20, 0.20, 0.19), n1)
    col = np.where(mortar[..., None], mcol, bcol)
    h = np.where(mortar, 0.15 + 0.15 * n1, 0.75 + 0.25 * n1)
    rough = np.where(mortar, 0.92, 0.78 + 0.12 * n1)
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["brick"] = save_pbr_set(out_dir, "brick", col, h, rough, metal, 4.8)

    # 4. Weathered Wood
    grain = periodic_fbm(res, seed=401, persistence=0.45, aniso=(1.0, 9.0))
    plank_seams = ((X * 6.0) % 1.0 < 0.03) | ((X * 6.0) % 1.0 > 0.97)
    n2 = periodic_fbm(res, seed=402, persistence=0.6)
    col = lerp_color((0.26, 0.20, 0.14), (0.14, 0.11, 0.08), grain * 0.7 + n2 * 0.3)
    col = np.where(plank_seams[..., None], col * 0.45, col)
    h = np.where(plank_seams, 0.05, 0.65 + 0.35 * grain)
    rough = 0.74 + 0.20 * grain
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["wood"] = save_pbr_set(out_dir, "wood", col, h, rough, metal, 3.8)

    # 5. Asphalt
    agg = periodic_fbm(res, seed=501, persistence=0.2)
    wear = periodic_fbm(res, seed=502, persistence=0.65)
    crack = np.exp(-np.abs(wear - 0.48) * 55.0)
    col = lerp_color((0.13, 0.14, 0.15), (0.22, 0.23, 0.24), agg * 0.65 + wear * 0.35) * (1.0 - 0.45 * crack[..., None])
    h = agg * 0.55 - crack * 0.65
    rough = 0.80 + 0.15 * agg - 0.18 * (wear > 0.75)
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["asphalt"] = save_pbr_set(out_dir, "asphalt", col, h, rough, metal, 4.2)

    # 6. Mud
    m1 = periodic_fbm(res, seed=601, persistence=0.62)
    m2 = periodic_fbm(res, seed=602, persistence=0.35)
    puddle = m1 < 0.32
    col = lerp_color((0.20, 0.15, 0.10), (0.11, 0.08, 0.06), m1 * 0.6 + m2 * 0.4)
    col = np.where(puddle[..., None], col * 0.68, col)
    h = np.where(puddle, 0.28, 0.28 + (m1 - 0.32) * 0.9 + m2 * 0.18)
    rough = np.where(puddle, 0.12, 0.62 + 0.25 * m2)
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["mud"] = save_pbr_set(out_dir, "mud", col, h, rough, metal, 3.6)

    # 7. Stone
    s1 = periodic_fbm(res, seed=701, persistence=0.58)
    s2 = periodic_fbm(res, seed=702, persistence=0.30)
    vein = np.exp(-np.abs(s1 - 0.53) * 42.0)
    col = lerp_color((0.28, 0.29, 0.28), (0.41, 0.42, 0.39), s1 * 0.7 + vein * 0.3)
    h = s1 * 0.65 + s2 * 0.35 - vein * 0.22
    rough = 0.76 + 0.18 * s2
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["stone"] = save_pbr_set(out_dir, "stone", col, h, rough, metal, 4.4)

    # 8. Fabric
    weave_x = 0.5 + 0.5 * np.sin(X * 2 * np.pi * 64)
    weave_y = 0.5 + 0.5 * np.sin(Y * 2 * np.pi * 64)
    weave = 0.5 * (weave_x + weave_y)
    dirt = periodic_fbm(res, seed=801, persistence=0.6)
    col = lerp_color((0.27, 0.28, 0.24), (0.15, 0.14, 0.12), dirt * 0.75) * (0.85 + 0.2 * weave[..., None])
    h = weave * 0.45 + dirt * 0.35
    rough = 0.88 + 0.08 * dirt
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["fabric"] = save_pbr_set(out_dir, "fabric", col, h, rough, metal, 2.5)

    # 9. Plastic
    p1 = periodic_fbm(res, seed=901, persistence=0.35)
    scratch = np.exp(-np.abs(periodic_fbm(res, seed=902, aniso=(1.0, 5.0)) - 0.5) * 65.0)
    col = lerp_color((0.18, 0.21, 0.22), (0.26, 0.29, 0.30), p1 * 0.5 + scratch * 0.5)
    h = 0.5 + 0.12 * p1 - 0.25 * scratch
    rough = 0.38 + 0.32 * scratch + 0.12 * p1
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["plastic"] = save_pbr_set(out_dir, "plastic", col, h, rough, metal, 2.0)

    # 10. Dirty Glass
    g1 = periodic_fbm(res, seed=1001, persistence=0.65)
    edge_grime = np.maximum(np.abs(X - 0.5), np.abs(Y - 0.5)) * 2.0
    grime = np.clip(edge_grime * 0.65 + g1 * 0.45 - 0.25, 0.0, 1.0)
    col = lerp_color((0.46, 0.54, 0.55), (0.28, 0.30, 0.27), grime)
    h = 0.5 + 0.15 * grime
    rough = 0.08 + 0.68 * grime
    metal = np.full((res, res), 0.05, dtype=np.float32)
    manifest["glass"] = save_pbr_set(out_dir, "glass", col, h, rough, metal, 1.4)

    # 11. Painted Metal
    pm1 = periodic_fbm(res, seed=1101, persistence=0.58)
    chip = pm1 > 0.66
    col = np.where(
        chip[..., None],
        lerp_color((0.36, 0.38, 0.40), (0.44, 0.22, 0.11), np.clip((pm1 - 0.66) * 3.0, 0, 1)),
        lerp_color((0.19, 0.25, 0.21), (0.14, 0.19, 0.16), pm1),
    )
    h = np.where(chip, 0.32, 0.68 + 0.08 * pm1)
    rough = np.where(chip, 0.45, 0.52 + 0.15 * pm1)
    metal = np.where(chip, 0.85, 0.15)
    manifest["painted_metal"] = save_pbr_set(out_dir, "painted_metal", col, h, rough, metal, 3.6)

    # 12. Damaged Wall (Plaster + damp mold)
    dw1 = periodic_fbm(res, seed=1201, persistence=0.62)
    peel = dw1 > 0.62
    damp = np.clip((1.0 - Y) * 0.7 + dw1 * 0.4 - 0.45, 0.0, 1.0)
    plaster = lerp_color((0.58, 0.57, 0.53), (0.31, 0.33, 0.28), damp)
    under = lerp_color((0.30, 0.26, 0.22), (0.20, 0.18, 0.15), dw1)
    col = np.where(peel[..., None], under, plaster)
    h = np.where(peel, 0.25 + 0.2 * dw1, 0.72 + 0.1 * dw1)
    rough = np.where(peel, 0.90, 0.72 + 0.18 * damp)
    metal = np.zeros((res, res), dtype=np.float32)
    manifest["damaged_wall"] = save_pbr_set(out_dir, "damaged_wall", col, h, rough, metal, 4.0)

    # 13. Wet Surface
    ws1 = periodic_fbm(res, seed=1301, persistence=0.52)
    film = ws1 < 0.48
    col = lerp_color((0.18, 0.19, 0.20), (0.11, 0.12, 0.13), film.astype(np.float32) * 0.7 + ws1 * 0.3)
    h = np.where(film, 0.40, 0.40 + (ws1 - 0.48) * 0.75)
    rough = np.where(film, 0.09, 0.52 + 0.22 * ws1)
    metal = np.full((res, res), 0.04, dtype=np.float32)
    manifest["wet_surface"] = save_pbr_set(out_dir, "wet_surface", col, h, rough, metal, 2.8)

    # 14. Industrial Machinery (Diamond tread + grease)
    tx = np.abs(((X * 16) % 1.0) - 0.5) * 2.0
    ty = np.abs(((Y * 16) % 1.0) - 0.5) * 2.0
    tread1 = np.clip(1.0 - (tx * 2.2 + ty * 0.7), 0.0, 1.0)
    tx2 = np.abs((((X + 0.03125) * 16) % 1.0) - 0.5) * 2.0
    ty2 = np.abs((((Y + 0.03125) * 16) % 1.0) - 0.5) * 2.0
    tread2 = np.clip(1.0 - (tx2 * 0.7 + ty2 * 2.2), 0.0, 1.0)
    tread = np.maximum(tread1, tread2)
    grime = periodic_fbm(res, seed=1401, persistence=0.58)
    col = lerp_color((0.24, 0.26, 0.28), (0.10, 0.10, 0.09), grime * 0.7) * (0.85 + 0.30 * tread[..., None])
    h = 0.35 + 0.55 * tread - 0.15 * grime
    rough = 0.32 + 0.48 * grime - 0.12 * tread
    metal = 0.88 - 0.45 * grime
    manifest["industrial_machinery"] = save_pbr_set(out_dir, "industrial_machinery", col, h, rough, metal, 4.2)

    with open(os.path.join(out_dir, "pbr_manifest.json"), "w") as f:
        json.dump({"resolution": res, "materials": manifest}, f, indent=2)
    print(f"Generated {len(manifest)} complete PBR material sets ({len(manifest)*4} PNG maps) in {out_dir}/")


if __name__ == "__main__":
    generate_all()
