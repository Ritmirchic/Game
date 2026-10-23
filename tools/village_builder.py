#!/usr/bin/env python3
"""
ASHES OF HUMANITY: LAST SIGNAL — Dead Suburbs village & Shelter Echo-07 builders.

Generates grounded, yaw-aware architecture (real wall openings, gable roofs,
porches, chimneys, fences, driveways, streets, curbs, streetlights, utility
poles) so The Dead Suburbs reads as a hand-planned abandoned village instead of
floating boxes on a flat plane, and so the shelter is furnished & lit.
"""

from __future__ import annotations
import math
import random
from typing import Any, Dict, List, Optional, Tuple

from rbxl_binary import CFrameVal, RbxInstance, Vector3Val

from world_assembler import (
    stamp_swing_door,
    MAT_CONCRETE,
    MAT_WOOD,
    MAT_WOOD_PLANKS,
    MAT_BRICK,
    MAT_METAL,
    MAT_CORRODED_METAL,
    MAT_ASPHALT,
    MAT_GLASS,
    MAT_SMOOTH_PLASTIC,
    MAT_FABRIC,
    MAT_SLATE,
    MAT_BASALT,
    MAT_NEON,
    MAT_LEAFY_GRASS,
    MAT_PAVEMENT,
    MAT_DIAMOND_PLATE,
    SHAPE_BALL,
    SHAPE_CYLINDER,
    cf_pos,
    cf_euler,
    make_part,
    make_truss_ladder,
    add_point_light,
    add_spot_light,
)

MAT_SHINGLE = 2308  # Enum.Material.RoofShingles
MAT_PLASTER = 2310  # Enum.Material.Plaster
MAT_MULCH = 1344  # Enum.Material.Mud
MAT_CARD = 2304  # Enum.Material.Cardboard

# ------------------------------------------------------------------- frames
def _rx(a: float) -> Tuple[float, ...]:
    c, s = math.cos(a), math.sin(a)
    return (1.0, 0.0, 0.0, 0.0, c, -s, 0.0, s, c)


def _ry(a: float) -> Tuple[float, ...]:
    c, s = math.cos(a), math.sin(a)
    return (c, 0.0, s, 0.0, 1.0, 0.0, -s, 0.0, c)


def _rz(a: float) -> Tuple[float, ...]:
    c, s = math.cos(a), math.sin(a)
    return (c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0)


def _mm(a: Tuple[float, ...], b: Tuple[float, ...]) -> Tuple[float, ...]:
    out: List[float] = []
    for i in range(3):
        for j in range(3):
            out.append(a[i * 3 + 0] * b[0 * 3 + j] + a[i * 3 + 1] * b[1 * 3 + j] + a[i * 3 + 2] * b[2 * 3 + j])
    return tuple(out)


class Lot:
    """Local frame helper: +X = lot width axis, +Z = house front axis."""

    def __init__(self, cx: float, cz: float, yaw_deg: float = 0.0) -> None:
        self.cx = float(cx)
        self.cz = float(cz)
        self.yaw = math.radians(float(yaw_deg))

    def pos(self, lx: float, ly: float, lz: float) -> Tuple[float, float, float]:
        c, s = math.cos(self.yaw), math.sin(self.yaw)
        return (self.cx + lx * c + lz * s, float(ly), self.cz - lx * s + lz * c)

    def cf(self, lx: float, ly: float, lz: float, pitch_deg: float = 0.0, roll_deg: float = 0.0) -> CFrameVal:
        rot = _mm(_ry(self.yaw), _mm(_rx(math.radians(pitch_deg)), _rz(math.radians(roll_deg))))
        return CFrameVal(self.pos(lx, ly, lz), rot)

    def part(self, name: str, size: Tuple[float, float, float], lpos: Tuple[float, float, float],
             color: Tuple[int, int, int], mat: int, pitch: float = 0.0, roll: float = 0.0,
             **kw: Any) -> RbxInstance:
        return make_part(name, size, self.cf(lpos[0], lpos[1], lpos[2], pitch, roll), color, mat, **kw)


def _box(parent: RbxInstance, name: str, size: Tuple[float, float, float], pos: Tuple[float, float, float],
         color: Tuple[int, int, int], mat: int, pitch: float = 0.0, roll: float = 0.0,
         yaw: float = 0.0, **kw: Any) -> RbxInstance:
    rot = _mm(_ry(math.radians(yaw)), _mm(_rx(math.radians(pitch)), _rz(math.radians(roll))))
    p = make_part(name, size, CFrameVal((float(pos[0]), float(pos[1]), float(pos[2])), rot), color, mat, **kw)
    parent.add_child(p)
    return p


# --------------------------------------------------------------- wall system
def wall_run(lot: Lot, parent: RbxInstance, axis: str, at: float, length: float, y_base: float,
             height: float, thick: float, color: Tuple[int, int, int], mat: int, mvar: str,
             openings: List[Dict[str, Any]], name: str) -> None:
    """Build a wall as solid segments, kick-bases and headers around openings."""
    idx = 0
    cursor = -length / 2.0

    def seg(lc: float, lw: float, ly0: float, lh: float) -> None:
        nonlocal idx
        if lw <= 0.06 or lh <= 0.06:
            return
        if axis == "x":
            lp = (lc, y_base + ly0 + lh * 0.5, at)
            size = (lw, lh, thick)
        else:
            lp = (at, y_base + ly0 + lh * 0.5, lc)
            size = (thick, lh, lw)
        parent.add_child(lot.part(f"{name}_{idx}", size, lp, color, mat, pbr_variant=mvar))
        idx += 1

    for op in sorted(openings, key=lambda o: o["center"]):
        o0 = op["center"] - op["width"] * 0.5
        o1 = op["center"] + op["width"] * 0.5
        seg((cursor + o0) * 0.5, o0 - cursor, 0.0, height)
        bottom = float(op.get("bottom", 0.0))
        top = float(op.get("top", height))
        if bottom > 0.06:
            seg(op["center"], op["width"], 0.0, bottom)
        if height - top > 0.06:
            seg(op["center"], op["width"], top, height - top)
        cursor = max(cursor, o1)
    seg((cursor + length * 0.5) * 0.5, length * 0.5 - cursor, 0.0, height)


def window_units(lot: Lot, parent: RbxInstance, axis: str, face_at: float, y_base: float,
                 openings: List[Dict[str, Any]], trim_col: Tuple[int, int, int],
                 rng: random.Random, board_chance: float = 0.0) -> None:
    """Glass panes, frames and sills (or boarded-up planks) for glazed openings."""
    for oi, op in enumerate(sorted(openings, key=lambda o: o["center"])):
        if op.get("kind") not in ("glass", "shutter"):
            continue
        lc = float(op["center"])
        w = float(op["width"])
        bottom = float(op.get("bottom", 2.6))
        top = float(op.get("top", 6.4))
        h = top - bottom
        ym = y_base + (bottom + top) * 0.5
        outward = face_at + (0.42 if axis == "x" else 0.0)
        outz = face_at + (0.42 if axis == "z" else 0.0)

        if rng.random() < board_chance:
            for bi in range(3):
                yy = y_base + bottom + (bi + 0.5) * (h / 3.0)
                tilt = rng.uniform(-8.0, 8.0)
                if axis == "x":
                    parent.add_child(lot.part(f"Window_{oi}_Board{bi}", (w + 1.0, 0.55, 0.3),
                                             (lc, yy, outz), (110, 82, 54), MAT_WOOD_PLANKS, roll=tilt,
                                             pbr_variant="wood",
                                             attrs={"DestructibleType": "WoodenBarricade", "StructureHP": 70}))
                else:
                    parent.add_child(lot.part(f"Window_{oi}_Board{bi}", (0.3, 0.55, w + 1.0),
                                             (outward, yy, lc), (110, 82, 54), MAT_WOOD_PLANKS, pitch=tilt,
                                             pbr_variant="wood",
                                             attrs={"DestructibleType": "WoodenBarricade", "StructureHP": 70}))
            continue

        if axis == "x":
            parent.add_child(lot.part(f"Window_{oi}_Glass", (w - 0.3, h - 0.3, 0.18), (lc, ym, face_at),
                                     (146, 172, 186), MAT_GLASS, transparency=0.5, cast_shadow=False,
                                     attrs={"DestructibleType": "Glass", "StructureHP": 18}))
            for sgn in (-1, 1):
                parent.add_child(lot.part(f"Window_{oi}_FrameH{sgn}", (w + 0.6, 0.34, 0.42),
                                         (lc, y_base + (top if sgn > 0 else bottom), outz),
                                         trim_col, MAT_WOOD_PLANKS))
                parent.add_child(lot.part(f"Window_{oi}_FrameV{sgn}", (0.34, h + 0.6, 0.42),
                                         (lc + sgn * w * 0.5, ym, outz), trim_col, MAT_WOOD_PLANKS))
            parent.add_child(lot.part(f"Window_{oi}_Mullion", (0.22, h, 0.3), (lc, ym, outz),
                                     trim_col, MAT_WOOD_PLANKS))
            parent.add_child(lot.part(f"Window_{oi}_Sill", (w + 1.1, 0.3, 1.1),
                                     (lc, y_base + bottom - 0.15, outz), (128, 122, 110), MAT_PLASTER))
        else:
            parent.add_child(lot.part(f"Window_{oi}_Glass", (0.18, h - 0.3, w - 0.3), (face_at, ym, lc),
                                     (146, 172, 186), MAT_GLASS, transparency=0.5, cast_shadow=False,
                                     attrs={"DestructibleType": "Glass", "StructureHP": 18}))
            for sgn in (-1, 1):
                parent.add_child(lot.part(f"Window_{oi}_FrameH{sgn}", (0.42, 0.34, w + 0.6),
                                         (outward, y_base + (top if sgn > 0 else bottom), lc),
                                         trim_col, MAT_WOOD_PLANKS))
                parent.add_child(lot.part(f"Window_{oi}_FrameV{sgn}", (0.42, h + 0.6, 0.34),
                                         (outward, ym, lc + sgn * w * 0.5), trim_col, MAT_WOOD_PLANKS))
            parent.add_child(lot.part(f"Window_{oi}_Mullion", (0.3, h, 0.22), (outward, ym, lc),
                                     trim_col, MAT_WOOD_PLANKS))
            parent.add_child(lot.part(f"Window_{oi}_Sill", (1.1, 0.3, w + 1.1),
                                     (outward, y_base + bottom - 0.15, lc), (128, 122, 110), MAT_PLASTER))


def sgn(v: float) -> float:
    return 1.0 if v >= 0 else -1.0


def lot_door(lot: Lot, parent: RbxInstance, axis: str, at: float, lc: float, y_base: float,
             width: float, height: float, col: Tuple[int, int, int], mat: int,
             swing_dir: float = 1.0) -> None:
    """Frame + anchored leaf with baked closed/open poses, oriented with the lot."""
    if axis == "x":
        jamb = (lc - width * 0.5 - 0.24, y_base + height * 0.5, at)
        jamb_size = (0.5, height + 1.0, 1.1)
        head = (lc, y_base + height + 0.4, at)
        head_size = (width + 1.0, 0.8, 1.1)
        leaf_pos = (lc, y_base + height * 0.5, at)
        leaf_size = (width, height - 0.3, 0.42)
        off = -width * 0.5
    else:
        jamb = (at, y_base + height * 0.5, lc - width * 0.5 - 0.24)
        jamb_size = (1.1, height + 1.0, 0.5)
        head = (at, y_base + height + 0.4, lc)
        head_size = (1.1, 0.8, width + 1.0)
        leaf_pos = (at, y_base + height * 0.5, lc)
        leaf_size = (0.42, height - 0.3, width)
        off = -width * 0.5

    jamb_p = lot.part("DoorFramePost", jamb_size, jamb, (64, 60, 56), MAT_WOOD_PLANKS)
    leaf = lot.part("InteractiveDoorLeaf", leaf_size, leaf_pos, col, mat, anchored=True,
                    attrs={"InteractiveType": "Door", "DoorOpen": False}, pbr_variant="wood")
    parent.add_child(lot.part("DoorFrameHeader", head_size, head, (64, 60, 56), MAT_WOOD_PLANKS))
    parent.add_child(jamb_p)
    parent.add_child(leaf)
    # Hinge axis = the frame post at the leaf's height; poses baked so opening is exact.
    y = lot.yaw
    if axis == "x":
        ox, oz = math.sin(y) * sgn(at), math.cos(y) * sgn(at)
    else:
        ox, oz = math.cos(y) * sgn(at), -math.sin(y) * sgn(at)
    stamp_swing_door(leaf, lot.pos(jamb[0], leaf_pos[1], jamb[2]), (ox, oz), abs(swing_dir) * 88.0)

    # Peephole light + interior handle plate
    parent.add_child(lot.part("DoorHandle", (0.5, 0.5, 0.16),
                            (lc + width * 0.34, y_base + height * 0.45, at + (0.42 if axis == "x" else 0.0)),
                            (150, 146, 138), MAT_METAL))


def gable_roof(lot: Lot, parent: RbxInstance, w: float, d: float, y_eave: float, rise: float,
               overhang: float, col: Tuple[int, int, int], wall_col: Tuple[int, int, int],
               wall_mat: int, mvar: str, damaged: bool = False) -> None:
    half_d = d * 0.5 + overhang
    pitch = math.degrees(math.atan2(rise, half_d))
    slope = math.hypot(rise, half_d) + 0.6
    for si, sgn in enumerate((1, -1)):
        if damaged and si == 1:
            for bi in range(4):
                parent.add_child(lot.part(f"RoofRafterStub{bi}", (w + 2 * overhang, 0.44, 0.44),
                                        (0, y_eave + 0.5 + bi * 0.12, sgn * (0.6 + bi * 1.9)),
                                        (92, 70, 50), MAT_WOOD, pbr_variant="wood"))
            continue
        parent.add_child(lot.part(f"RoofSlope{'S' if sgn > 0 else 'N'}", (w + 2 * overhang, 0.45, slope),
                                (0, y_eave + rise * 0.5, sgn * half_d * 0.5), col, MAT_SHINGLE,
                                pitch=sgn * pitch, pbr_variant="stone"))
    parent.add_child(lot.part("RoofRidgeCap", (w + 2 * overhang + 0.9, 0.55, 1.45),
                             (0, y_eave + rise + 0.18, 0), (72, 62, 58), MAT_SHINGLE))
    for sx in (-1, 1):
        for bi in range(3):
            band_h = rise / 3.0
            band_w = d * (1.0 - bi / 3.0)
            if band_w <= 0.3:
                continue
            parent.add_child(lot.part(f"Gable_{sx}_{bi}", (0.55, band_h + 0.15, band_w),
                                     (sx * w * 0.5, y_eave + band_h * (bi + 0.5), 0),
                                     wall_col, wall_mat, pbr_variant=mvar))


HOUSE_STYLES: Dict[str, Dict[str, Any]] = {
    "cottage": {"w": 22.0, "d": 18.0, "h": 9.0, "rise": 5.2, "floors": 1},
    "family": {"w": 27.0, "d": 21.0, "h": 9.8, "rise": 6.0, "floors": 1},
    "twoStory": {"w": 24.0, "d": 20.0, "h": 8.6, "rise": 5.6, "floors": 2},
    "bungalow": {"w": 21.0, "d": 17.0, "h": 8.2, "rise": 4.4, "floors": 1},
    "colonial": {"w": 31.0, "d": 23.0, "h": 9.8, "rise": 6.4, "floors": 2},
}

PALETTE: List[Tuple[Tuple[int, int, int], Tuple[int, int, int], int, str]] = [
    ((168, 160, 144), (86, 64, 54), MAT_PLASTER, "damaged_wall"),
    ((150, 126, 106), (74, 58, 48), MAT_WOOD_PLANKS, "wood"),
    ((142, 148, 154), (68, 64, 62), MAT_PLASTER, "damaged_wall"),
    ((156, 142, 122), (92, 70, 56), MAT_BRICK, "brick"),
    ((134, 138, 128), (78, 70, 62), MAT_WOOD, "wood"),
    ((176, 164, 142), (96, 74, 58), MAT_WOOD_PLANKS, "wood"),
]


def build_house(b: Any, parent: RbxInstance, rng: random.Random, cx: float, cz: float, yaw: float,
                style_name: str, name: str, ruined: bool = False, has_garage: bool = False) -> Dict[str, Any]:
    style = HOUSE_STYLES[style_name]
    w = style["w"] + rng.uniform(-1.0, 1.5)
    d = style["d"] + rng.uniform(-1.0, 1.5)
    single_h = style["h"]
    wall_h = single_h * style["floors"] + 0.8 * (style["floors"] - 1)
    wall_col, roof_col, wall_mat, mvar = PALETTE[rng.randrange(len(PALETTE))]
    if ruined:
        wall_col = (108, 104, 98)

    lot = Lot(cx, cz, yaw)
    model = RbxInstance("Model", name)
    t = 0.9
    y0 = 0.95
    fz = d * 0.5
    bz = -d * 0.5

    _box(model, "Foundation", (w + 1.6, 1.0, d + 1.6), lot.pos(0, 0.5, 0), (118, 114, 106), MAT_CONCRETE,
         pbr_variant="concrete")
    model.add_child(lot.part("FloorSlab", (w - 0.6, 0.35, d - 0.6), (0, y0 + 0.18, 0),
                            (138, 114, 84), MAT_WOOD_PLANKS, pbr_variant="wood"))

    front_ops: List[Dict[str, Any]] = [
        {"center": -w * 0.17, "width": 4.4, "bottom": 0.0, "top": 7.6, "kind": "door"},
        {"center": w * 0.22, "width": 5.4, "bottom": 2.8, "top": 6.6, "kind": "glass"},
    ]
    back_ops: List[Dict[str, Any]] = [
        {"center": -w * 0.22, "width": 4.6, "bottom": 3.0, "top": 6.4, "kind": "glass"},
        {"center": w * 0.22, "width": 4.6, "bottom": 3.0, "top": 6.4, "kind": "glass"},
    ]
    side_ops: List[Dict[str, Any]] = [
        {"center": -d * 0.2, "width": 4.0, "bottom": 3.0, "top": 6.2, "kind": "glass"},
        {"center": d * 0.2, "width": 4.0, "bottom": 3.0, "top": 6.2, "kind": "glass"},
    ]
    if style["floors"] == 2:
        front_ops.append({"center": w * 0.22, "width": 4.6, "bottom": single_h + 2.6,
                         "top": single_h + 6.2, "kind": "glass"})
        front_ops.append({"center": -w * 0.2, "width": 4.6, "bottom": single_h + 2.6,
                          "top": single_h + 6.2, "kind": "glass"})
        back_ops.append({"center": 0.0, "width": 4.6, "bottom": single_h + 2.6,
                        "top": single_h + 6.2, "kind": "glass"})

    if ruined:
        front_ops = front_ops[:1]
        back_ops = []
        side_ops = side_ops[:1]

    wall_run(lot, model, "x", fz, w, y0, wall_h, t, wall_col, wall_mat, mvar, front_ops, "WallFront")
    wall_run(lot, model, "x", bz, w, y0, wall_h, t, wall_col, wall_mat, mvar, back_ops, "WallBack")
    wall_run(lot, model, "z", -w * 0.5, d, y0, wall_h, t, wall_col, wall_mat, mvar, side_ops, "WallWest")
    wall_run(lot, model, "z", w * 0.5, d, y0, wall_h, t, wall_col, wall_mat, mvar, side_ops, "WallEast")

    board = 0.0 if ruined else 0.35
    window_units(lot, model, "x", fz + t * 0.5, y0, front_ops, (78, 64, 54), rng, board)
    window_units(lot, model, "x", bz - t * 0.5, y0, back_ops, (78, 64, 54), rng, board)
    window_units(lot, model, "z", -w * 0.5 - t * 0.5, y0, side_ops, (78, 64, 54), rng, board)
    window_units(lot, model, "z", w * 0.5 + t * 0.5, y0, side_ops, (78, 64, 54), rng, board)

    for op in front_ops:
        if op.get("kind") == "door":
            lot_door(lot, model, "x", fz, op["center"], y0, op["width"], min(op["top"], wall_h) - 0.3,
                     (114, 78, 48), MAT_WOOD)

    model.add_child(lot.part("CeilingDeck", (w - 0.5, 0.32, d - 0.5), (0, y0 + wall_h + 0.16, 0),
                            (134, 124, 110), MAT_PLASTER, transparency=1.0 if ruined else 0.0,
                            cast_shadow=not ruined))
    gable_roof(lot, model, w, d, y0 + wall_h + 0.35, style["rise"], 1.6, roof_col, wall_col, wall_mat,
               mvar, damaged=ruined)

    chim_h = wall_h + style["rise"] + 3.0
    model.add_child(lot.part("Chimney", (2.0, chim_h, 2.0), (w * 0.3, chim_h * 0.5 + 0.9, -d * 0.3),
                            (136, 94, 76), MAT_BRICK, pbr_variant="brick"))
    model.add_child(lot.part("ChimneyCap", (2.7, 0.4, 2.7), (w * 0.3, chim_h + 0.9, -d * 0.3),
                            (96, 90, 86), MAT_CONCRETE))

    if style["floors"] == 2 and not ruined:
        model.add_child(lot.part("UpperDeck", (w - 1.4, 0.4, d - 1.4), (0, y0 + single_h + 0.2, 0),
                                (132, 108, 80), MAT_WOOD_PLANKS, pbr_variant="wood"))
        steps = 8
        for si in range(steps):
            model.add_child(lot.part(f"Stair_{si}", (4.0, 0.34, 1.45),
                                    (-w * 0.34, y0 + 0.9 + si * (single_h / steps),
                                     -d * 0.32 + si * (d * 0.42 / steps)), (112, 90, 66), MAT_WOOD_PLANKS))
        model.add_child(lot.part("UpperRail", (0.3, 2.2, d * 0.34), (-w * 0.34 + 2.2,
                                y0 + single_h + 1.4, -d * 0.32 + d * 0.21), (112, 90, 66), MAT_WOOD_PLANKS))

    if not ruined:
        porch_d = 6.5
        pcx = front_ops[0]["center"] + w * 0.05
        model.add_child(lot.part("PorchDeck", (w * 0.66, 0.42, porch_d), (pcx, y0 - 0.25, fz + porch_d * 0.5 - 0.3),
                                (126, 102, 74), MAT_WOOD_PLANKS, pbr_variant="wood"))
        for ci, clx in enumerate((pcx - w * 0.28, pcx + w * 0.28)):
            model.add_child(lot.part(f"PorchColumn_{ci}", (0.75, wall_h - 1.7, 0.75),
                                    (clx, y0 + (wall_h - 1.7) * 0.5 - 0.25, fz + porch_d - 0.8),
                                    (162, 156, 142), MAT_PLASTER))
        model.add_child(lot.part("PorchRoof", (w * 0.7, 0.45, porch_d + 1.8),
                                (pcx, y0 + wall_h - 1.5, fz + porch_d * 0.5 - 0.1), roof_col, MAT_SHINGLE,
                                pitch=-8.0, pbr_variant="stone"))
        for sti in range(3):
            model.add_child(lot.part(f"PorchStep_{sti}", (5.6, 0.32, 1.25),
                                    (pcx, y0 - 0.82 + sti * 0.29, fz + porch_d + 1.25 - sti * 1.2),
                                    (118, 96, 70), MAT_WOOD_PLANKS))
        for ri, rx_ in enumerate((pcx - w * 0.28, pcx + w * 0.28)):
            model.add_child(lot.part(f"PorchRail_{ri}", (0.26, 1.6, porch_d * 0.8),
                                    (rx_, y0 + 0.75, fz + porch_d * 0.5), (132, 110, 80), MAT_WOOD_PLANKS))
        model.add_child(lot.part("PorchRailTop", (w * 0.66, 0.24, 0.4), (pcx, y0 + 1.6, fz + porch_d * 0.5),
                                (132, 110, 80), MAT_WOOD_PLANKS))
    else:
        for di in range(7):
            model.add_child(lot.part(f"Rubble_{di}", (rng.uniform(1.6, 3.8), rng.uniform(0.6, 1.5),
                                    rng.uniform(1.6, 3.6)),
                                (rng.uniform(-w * 0.42, w * 0.42), 1.4 + rng.uniform(-0.4, 0.7),
                                 rng.uniform(-d * 0.42, d * 0.42)),
                                (124, 118, 110), MAT_CONCRETE, roll=rng.uniform(-26, 26)))

    if not ruined:
        model.add_child(lot.part("PartitionWall", (0.55, wall_h - 0.6, d * 0.4), (w * 0.12,
                                y0 + (wall_h - 0.6) * 0.5, -d * 0.26), (154, 148, 134), MAT_PLASTER))
        model.add_child(lot.part("BedFrame", (4.6, 1.5, 7.0), (-w * 0.3, y0 + 0.95, -d * 0.28),
                                (102, 82, 58), MAT_WOOD_PLANKS))
        model.add_child(lot.part("Mattress", (4.2, 0.8, 6.6), (-w * 0.3, y0 + 2.1, -d * 0.28),
                                (166, 158, 140), MAT_FABRIC, pbr_variant="fabric"))
        model.add_child(lot.part("KitchenCounter", (7.6, 2.3, 2.1), (w * 0.26, y0 + 1.35, -d * 0.38),
                                (130, 124, 116), MAT_CONCRETE))
        model.add_child(lot.part("DiningTable", (5.4, 0.36, 3.4), (w * 0.02, y0 + 2.6, -d * 0.02),
                                (118, 88, 58), MAT_WOOD_PLANKS))
        for li, (ox, oz) in enumerate(((-2.2, -1.3), (2.2, -1.3), (-2.2, 1.3), (2.2, 1.3))):
            model.add_child(lot.part(f"TableLeg_{li}", (0.42, 2.2, 0.42), (w * 0.02 + ox, y0 + 1.3,
                                    -d * 0.02 + oz), (94, 72, 48), MAT_WOOD))
        for ci, (ox, oz) in enumerate(((0.0, -3.2), (0.0, 3.2))):
            model.add_child(lot.part(f"Chair_{ci}", (2.0, 1.1, 2.0), (w * 0.02 + ox, y0 + 0.75,
                                    -d * 0.02 + oz), (104, 80, 54), MAT_WOOD_PLANKS))
            model.add_child(lot.part(f"ChairBack_{ci}", (2.0, 2.4, 0.32), (w * 0.02 + ox, y0 + 2.4,
                                    -d * 0.02 + oz + (1.0 if oz < 0 else -1.0) * 0.95),
                                    (104, 80, 54), MAT_WOOD_PLANKS))
        model.add_child(lot.part("ShelfUnit", (5.4, 5.0, 1.3), (w * 0.42, y0 + 2.6, d * 0.36),
                                (110, 88, 62), MAT_WOOD_PLANKS, pbr_variant="wood"))
        lamp = lot.part("CeilingLamp", (1.5, 0.55, 1.5), (0, y0 + wall_h - 0.8, -d * 0.04),
                       (244, 226, 178), MAT_NEON, cast_shadow=False, can_collide=False)
        add_point_light(lamp, (255, 210, 150), brightness=1.6, range_studs=30.0, shadows=False)
        model.add_child(lamp)

    if has_garage and not ruined:
        gw, gd, gh = 13.5, 12.5, 8.0
        gx = w * 0.5 + gw * 0.5 - 0.5
        gz = fz - gd * 0.5 + 1.6
        model.add_child(lot.part("GarageFloor", (gw, 0.8, gd), (gx, 0.4, gz), (112, 110, 106),
                                MAT_CONCRETE))
        model.add_child(lot.part("GarageWallN", (gw, gh, 0.8), (gx, gh * 0.5 + 0.8, gz - gd * 0.5),
                                wall_col, wall_mat, pbr_variant=mvar))
        model.add_child(lot.part("GarageWallE", (0.8, gh, gd), (gx + gw * 0.5, gh * 0.5 + 0.8, gz),
                                wall_col, wall_mat, pbr_variant=mvar))
        model.add_child(lot.part("GarageWallW", (0.8, gh, gd), (gx - gw * 0.5, gh * 0.5 + 0.8, gz),
                                wall_col, wall_mat, pbr_variant=mvar))
        model.add_child(lot.part("GarageRoof", (gw + 1.4, 0.5, gd + 1.2), (gx, gh + 1.1, gz), roof_col,
                                MAT_SHINGLE, pitch=-6.5, pbr_variant="stone"))
        model.add_child(lot.part("GarageDoorPanel", (gw - 1.8, gh - 1.8, 0.45),
                                (gx, (gh - 1.8) * 0.5 + 0.9, gz + gd * 0.5 + 0.1), (120, 116, 110),
                                MAT_CORRODED_METAL, pbr_variant="rust",
                                attrs={"DestructibleType": "WeakDoor", "StructureHP": 150}))

    parent.add_child(model)
    return {"model": model, "lot": lot, "w": w, "d": d, "front_z": fz, "name": name, "yaw": yaw,
            "cx": float(cx), "cz": float(cz)}


def fence_run(lot: Lot, parent: RbxInstance, axis: str, at: float, a0: float, a1: float,
              gaps: List[Tuple[float, float]], col: Tuple[int, int, int]) -> None:
    spans: List[Tuple[float, float]] = []
    cursor = a0
    for g0, g1 in sorted(gaps):
        if g0 > cursor:
            spans.append((cursor, min(g0, a1)))
        cursor = max(cursor, g1)
    if cursor < a1:
        spans.append((cursor, a1))
    pi = 0
    for s0, s1 in spans:
        if s1 - s0 < 0.8:
            continue
        n = max(2, int(round((s1 - s0) / 6.0)) + 1)
        for k in range(n):
            tpos = s0 + (s1 - s0) * k / (n - 1)
            if axis == "x":
                parent.add_child(lot.part(f"FencePost_{pi}", (0.42, 3.6, 0.42), (tpos, 1.8, at),
                                         col, MAT_WOOD_PLANKS))
            else:
                parent.add_child(lot.part(f"FencePost_{pi}", (0.42, 3.6, 0.42), (at, 1.8, tpos),
                                         col, MAT_WOOD_PLANKS))
            pi += 1
        step = (s1 - s0) / (n - 1)
        for k in range(n - 1):
            mid = s0 + step * (k + 0.5)
            for hi, hy in enumerate((2.75, 1.35)):
                if axis == "x":
                    parent.add_child(lot.part(f"FenceRail_{pi}_{hi}", (step + 0.3, 0.36, 0.24),
                                            (mid, hy, at), (128, 106, 78), MAT_WOOD_PLANKS))
                else:
                    parent.add_child(lot.part(f"FenceRail_{pi}_{hi}", (0.24, 0.36, step + 0.3),
                                            (at, hy, mid), (128, 106, 78), MAT_WOOD_PLANKS))
            pi += 1


def add_dead_tree(parent: RbxInstance, lot: Lot, lx: float, lz: float, rng: random.Random,
                  scale: float = 1.0, live: bool = False) -> None:
    h = rng.uniform(15.0, 23.0) * scale
    parent.add_child(lot.part("TreeTrunk", (1.8 * scale, h, 1.8 * scale), (lx, h * 0.5, lz),
                             (76, 58, 44), MAT_WOOD, shape=SHAPE_CYLINDER, roll=rng.uniform(-5, 5)))
    for bi in range(4):
        bl = rng.uniform(4.0, 7.0) * scale
        ang = rng.uniform(0, 360)
        parent.add_child(lot.part(f"TreeBranch_{bi}", (bl, 0.5, 0.5),
                                 (lx + math.cos(math.radians(ang)) * bl * 0.35,
                                  h * rng.uniform(0.52, 0.94),
                                  lz + math.sin(math.radians(ang)) * bl * 0.35),
                                 (68, 52, 42), MAT_WOOD, roll=rng.uniform(-30, 30)))
    if live:
        parent.add_child(lot.part("PineCanopyLower", (9.0 * scale, 7.2 * scale, 9.0 * scale),
                                 (lx, h * 0.92, lz), (46, 64, 44), MAT_LEAFY_GRASS, can_collide=False))
        parent.add_child(lot.part("PineCanopyUpper", (6.2 * scale, 6.0 * scale, 6.2 * scale),
                                 (lx, h * 1.24, lz), (40, 56, 38), MAT_LEAFY_GRASS, can_collide=False))


def build_lot(b: Any, parent: RbxInstance, rng: random.Random, info: Dict[str, Any],
              loot_table: str, lot_w: float = 46.0, lot_d: float = 34.0) -> None:
    lot: Lot = info["lot"]
    model: RbxInstance = info["model"]
    w, d, fz = info["w"], info["d"], info["front_z"]

    model.add_child(lot.part("YardLawn", (lot_w, 0.28, lot_d), (0, 0.14, fz + lot_d * 0.5 - d * 0.5 - 4.0),
                           (94, 104, 70), MAT_LEAFY_GRASS))
    model.add_child(lot.part("YardDirtPatch", (rng.uniform(6, 12), 0.3, rng.uniform(5, 9)),
                           (rng.uniform(-10, 10), 0.16, fz + rng.uniform(4, 12)),
                           (118, 100, 72), MAT_MULCH))
    front_fence_z = fz + lot_d - d * 0.5 - 6.5
    fence_run(lot, model, "x", front_fence_z, -lot_w * 0.5, lot_w * 0.5,
              [(-w * 0.17 - 4.2, -w * 0.17 + 4.2), (w * 0.5 + 0.4, w * 0.5 + 11.6)], (112, 92, 66))
    for side in (-1, 1):
        fence_run(lot, model, "z", side * lot_w * 0.5, -d * 0.5 - 4.0, front_fence_z, [], (112, 92, 66))
    model.add_child(lot.part("Driveway", (11.0, 0.3, 15.0), (w * 0.5 + 6.0, 0.16, fz + 11.0),
                            (66, 66, 68), MAT_ASPHALT, pbr_variant="asphalt"))
    model.add_child(lot.part("Walkway", (3.6, 0.3, 10.0), (-w * 0.17, 0.17, fz + 5.6),
                            (136, 132, 124), MAT_PAVEMENT))
    model.add_child(lot.part("MailboxPost", (0.42, 3.6, 0.42), (-w * 0.17 - 3.4, 1.8, front_fence_z - 1.6),
                            (94, 74, 54), MAT_WOOD))
    model.add_child(lot.part("Mailbox", (1.6, 1.0, 2.4), (-w * 0.17 - 3.4, 4.0, front_fence_z - 1.6),
                            (104, 108, 112), MAT_METAL))
    if rng.random() < 0.6:
        add_dead_tree(model, lot, rng.uniform(-lot_w * 0.38, lot_w * 0.38), fz + rng.uniform(9, 16), rng,
                     scale=rng.uniform(0.75, 1.2), live=rng.random() < 0.35)
    for hi in range(2):
        model.add_child(lot.part(f"Hedge_{hi}", (4.0, 1.7, 1.7), (-w * 0.17 + (hi * 2 - 1) * 4.8, 0.95,
                        fz + 7.5), (66, 80, 54), MAT_LEAFY_GRASS, can_collide=False))
    model.add_child(lot.part("PorchCrate", (2.0, 1.3, 1.5), (-w * 0.17 + 3.2, 1.3, fz + 2.4),
                            (118, 92, 60), MAT_WOOD_PLANKS, pbr_variant="wood"))
    model.add_child(lot.part("TrashBin", (1.9, 2.4, 1.9), (w * 0.52, 1.2, -d * 0.32),
                            (72, 78, 74), MAT_SMOOTH_PLASTIC, shape=SHAPE_CYLINDER))
    model.add_child(lot.part("ACUnit", (2.8, 1.6, 2.2), (-w * 0.5 - 1.8, 0.8, -d * 0.1),
                            (118, 116, 110), MAT_CORRODED_METAL, pbr_variant="rust"))

    b.add_loot_container(model, loot_table, lot.pos(-w * 0.3, 1.52, -d * 0.24), (3.0, 2.2, 2.0),
                         (102, 84, 58), MAT_WOOD_PLANKS, "Search Kitchen Cabinets")
    b.add_loot_container(model, loot_table, lot.pos(w * 0.1 + 2.4, 1.45, fz + 13.5), (2.8, 2.4, 1.9),
                         (84, 90, 92), MAT_METAL, "Search Garden Shed Tools")


def street(parent: RbxInstance, name: str, cx: float, cz: float, length: float, width: float,
           along: str = "z") -> None:
    if along == "z":
        _box(parent, f"{name}_Road", (width, 0.32, length), (cx, 0.17, cz), (62, 63, 66), MAT_ASPHALT,
             pbr_variant="asphalt")
    else:
        _box(parent, f"{name}_Road", (length, 0.32, width), (cx, 0.17, cz), (62, 63, 66), MAT_ASPHALT,
             pbr_variant="asphalt")
    for sgn in (-1, 1):
        if along == "z":
            tag = "W" if sgn < 0 else "E"
            _box(parent, f"{name}_Curb{tag}", (1.2, 0.66, length), (cx + sgn * (width * 0.5 + 0.4), 0.33, cz),
                 (140, 136, 128), MAT_CONCRETE)
            _box(parent, f"{name}_Sidewalk{tag}", (4.8, 0.44, length), (cx + sgn * (width * 0.5 + 3.2),
                 0.4, cz), (138, 134, 126), MAT_PAVEMENT)
        else:
            tag = "S" if sgn < 0 else "N"
            _box(parent, f"{name}_Curb{tag}", (length, 0.66, 1.2), (cx, 0.33, cz + sgn * (width * 0.5 + 0.4)),
                 (140, 136, 128), MAT_CONCRETE)
            _box(parent, f"{name}_Sidewalk{tag}", (length, 0.44, 4.8), (cx, 0.4, cz + sgn * (width * 0.5 + 3.2)),
                 (138, 134, 126), MAT_PAVEMENT)
    n_dash = max(1, int(length / 24.0))
    for di in range(n_dash):
        tpos = -length * 0.5 + (di + 0.5) * (length / n_dash)
        if along == "z":
            _box(parent, f"{name}_Dash{di}", (1.0, 0.06, 7.4), (cx, 0.36, cz + tpos), (196, 186, 126),
                 MAT_ASPHALT, can_collide=False)
        else:
            _box(parent, f"{name}_Dash{di}", (7.4, 0.06, 1.0), (cx + tpos, 0.36, cz), (196, 186, 126),
                 MAT_ASPHALT, can_collide=False)


def streetlight(parent: RbxInstance, x: float, z: float, along: str, sgn: int) -> None:
    h = 17.0
    _box(parent, "LampBase", (2.0, 0.8, 2.0), (x, 0.4, z), (86, 84, 80), MAT_CONCRETE)
    _box(parent, "LampPole", (0.9, h, 0.9), (x, h * 0.5, z), (96, 98, 100), MAT_METAL)
    if along == "z":
        _box(parent, "LampArm", (5.0, 0.55, 0.7), (x + sgn * 2.5, h - 0.5, z), (96, 98, 100), MAT_METAL)
        head = _box(parent, "LampHead", (2.3, 0.7, 1.4), (x + sgn * 4.7, h - 1.1, z), (238, 220, 172),
                    MAT_NEON, cast_shadow=False)
    else:
        _box(parent, "LampArm", (0.7, 0.55, 5.0), (x, h - 0.5, z + sgn * 2.5), (96, 98, 100), MAT_METAL)
        head = _box(parent, "LampHead", (1.4, 0.7, 2.3), (x, h - 1.1, z + sgn * 4.7), (238, 220, 172),
                    MAT_NEON, cast_shadow=False)
    add_spot_light(head, (255, 214, 152), brightness=3.0, range_studs=52.0, angle=74.0, face=4, shadows=False)
    add_point_light(head, (255, 208, 148), brightness=0.85, range_studs=24.0, shadows=False)


def utility_pole(parent: RbxInstance, x: float, z: float, along: str) -> None:
    h = 20.0
    _box(parent, "UtilityPole", (1.2, h, 1.2), (x, h * 0.5, z), (92, 76, 58), MAT_WOOD)
    if along == "z":
        _box(parent, "CrossArm", (8.0, 0.6, 0.7), (x, h - 1.8, z), (86, 70, 54), MAT_WOOD)
        offs = ((-3.3, 0.0), (3.3, 0.0), (0.0, -2.6), (0.0, 2.6))
    else:
        _box(parent, "CrossArm", (0.7, 0.6, 8.0), (x, h - 1.8, z), (86, 70, 54), MAT_WOOD)
        offs = ((0.0, -3.3), (0.0, 3.3), (-2.6, 0.0), (2.6, 0.0))
    for oi, (ox, oz) in enumerate(offs):
        _box(parent, f"Insulator_{oi}", (0.6, 0.7, 0.6), (x + ox, h - 1.1, z + oz), (124, 118, 108),
             MAT_SMOOTH_PLASTIC)


def beam_between(parent: RbxInstance, name: str, p0: Tuple[float, float, float],
                 p1: Tuple[float, float, float], thick: float, color: Tuple[int, int, int],
                 mat: int, **kw: Any) -> RbxInstance:
    """A part stretched exactly between two points (its long axis is local X).

    cf_euler(rx=0, ry=yaw, rz=tilt) maps local +X to (cos t cos y, sin t, -cos t sin y),
    so yaw must be atan2(-dz, dx) for the beam to land on the far endpoint instead of
    mirrored across the X axis.
    """
    dx, dy, dz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
    horiz = math.hypot(dx, dz)
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(-dz, dx)) if horiz > 1e-6 else 0.0
    tilt = math.degrees(math.atan2(dy, horiz)) if length > 1e-6 else 0.0
    mid = ((p0[0] + p1[0]) * 0.5, (p0[1] + p1[1]) * 0.5, (p0[2] + p1[2]) * 0.5)
    part = make_part(name, (max(length, 0.2), thick, thick), cf_euler(mid[0], mid[1], mid[2], 0, yaw, tilt),
                     color, mat, **kw)
    parent.add_child(part)
    return part


def wire_span(parent: RbxInstance, x0: float, z0: float, x1: float, z1: float, y_top: float,
              sag: float, idx: int) -> None:
    """Two straight spans with a mid-point droop (cheap power line sag)."""
    mx, mz = (x0 + x1) * 0.5, (z0 + z1) * 0.5
    my = y_top - sag
    beam_between(parent, f"PowerWire_{idx}_0", (x0, y_top, z0), (mx, my, mz), 0.16, (36, 36, 38),
                 MAT_METAL, can_collide=False, cast_shadow=False)
    beam_between(parent, f"PowerWire_{idx}_1", (mx, my, mz), (x1, y_top, z1), 0.16, (36, 36, 38),
                 MAT_METAL, can_collide=False, cast_shadow=False)


def wrecked_car(parent: RbxInstance, x: float, z: float, yaw: float, col: Tuple[int, int, int],
                flipped: bool = False, burned: bool = False, model_name: str = "AbandonedSedan") -> None:
    if burned:
        col = (52, 50, 48)
    lot = Lot(x, z, yaw)
    car = RbxInstance("Model", model_name)
    attrs = {"InteractiveType": "VehicleAlarm", "PromptLabel": "Pop The Trunk"}
    body_col = col
    car.add_child(lot.part("CarBody", (6.8, 2.4, 13.6), (0, 1.9, 0), body_col, MAT_CORRODED_METAL,
                          pbr_variant="rust", roll=-16.0 if flipped else 0.0, attrs=attrs))
    car.add_child(lot.part("CarCabin", (6.0, 2.4, 6.6), (0, 4.2, -0.6),
                          (max(34, col[0] - 30), max(34, col[1] - 30), max(36, col[2] - 26)),
                          MAT_CORRODED_METAL, pbr_variant="rust", roll=-16.0 if flipped else 0.0))
    car.add_child(lot.part("CarHood", (6.4, 0.55, 3.8), (0, 3.25, 5.2), body_col, MAT_CORRODED_METAL,
                          pbr_variant="rust", roll=-16.0 if flipped else 0.0))
    car.add_child(lot.part("CarTrunk", (6.4, 0.5, 2.8), (0, 3.1, -6.0), body_col, MAT_CORRODED_METAL,
                          pbr_variant="rust", roll=-16.0 if flipped else 0.0))
    for wi, (wx, wz) in enumerate(((-3.4, 4.5), (3.4, 4.5), (-3.4, -4.5), (3.4, -4.5))):
        car.add_child(lot.part(f"Wheel_{wi}", (1.6, 2.8, 2.8), (wx, 1.4 if not flipped else 0.7, wz),
                              (38, 36, 36), MAT_FABRIC, shape=SHAPE_CYLINDER, roll=90.0))
    if not burned:
        for gi, gz in enumerate((2.6, -3.4)):
            car.add_child(lot.part(f"CarGlass_{gi}", (5.6, 1.7, 0.16), (0, 4.4, gz), (128, 150, 162),
                                  MAT_GLASS, transparency=0.62, cast_shadow=False, can_collide=False))
    car.add_child(lot.part("FlatTireSkid", (7.0, 0.1, 2.2), (0, 0.22, 6.0), (34, 32, 30), MAT_ASPHALT,
                          can_collide=False))
    parent.add_child(car)


def debris_pile(parent: RbxInstance, x: float, z: float, rng: random.Random, radius: float = 4.0,
                kind: str = "rubble") -> None:
    for i in range(rng.randint(4, 8)):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(0.2, radius)
        px, pz = x + math.cos(a) * r, z + math.sin(a) * r
        if kind == "wood":
            parent.add_child(make_part(f"DebrisPlank_{i}", (rng.uniform(2.4, 6.4), 0.45, 0.45),
                                      cf_euler(px, 0.5, pz, 0, rng.uniform(0, 360), rng.uniform(-18, 18)),
                                      (108, 86, 60), MAT_WOOD_PLANKS))
        elif kind == "sandbag":
            parent.add_child(make_part(f"Sandbag_{i}", (2.5, 0.85, 1.4),
                                      cf_euler(px, 0.5 + (i % 2) * 0.8, pz, 0, rng.uniform(-28, 28), 0),
                                      (146, 136, 106), MAT_FABRIC))
        else:
            parent.add_child(make_part(f"DebrisSlab_{i}", (rng.uniform(1.8, 4.6), rng.uniform(0.5, 1.4),
                                                           rng.uniform(1.8, 4.2)),
                                      cf_euler(px, 0.6, pz, rng.uniform(-14, 14), rng.uniform(0, 360),
                                               rng.uniform(-20, 20)),
                                      (126, 122, 116), MAT_CONCRETE))


# ---------------------------------------------------------- village assembly
def build_dead_suburbs(b: Any, parent: RbxInstance, rng: random.Random) -> None:
    """The Dead Suburbs: Maple Ridge, a small abandoned village around the Echo-07 hatch.

    Layout is a real street plan (one north-south high street, one east-west lane, a
    church green at the south end) with eight houses on surveyed lots, a corner grocery,
    a fuel stop, a chapel and a guyed relay mast. Scatter props are rejected if they
    land inside any built footprint, so nothing intersects a wall or a road.
    """
    # ------------------------------------------------------- street skeleton
    street(parent, "MapleMain", 0.0, 420.0, 320.0, 24.0, along="z")
    street(parent, "CrescentLane", 0.0, 400.0, 420.0, 18.0, along="x")
    # Concrete apron in front of the shelter hatch door (street ends at z = 580)
    _box(parent, "HatchApron", (34.0, 0.38, 24.0), (0.0, 0.19, 565.0), (134, 130, 122), MAT_PAVEMENT,
         pbr_variant="concrete")
    for ai in range(4):
        _box(parent, f"ApronJoint_{ai}", (33.0, 0.1, 0.5), (0.0, 0.4, 556.0 + ai * 6.0), (110, 106, 100),
             MAT_CONCRETE, can_collide=False)

    # ---------------------------------------------------- furniture & services
    streetlight(parent, -16.0, 540.0, "z", 1)
    streetlight(parent, 16.0, 462.0, "z", -1)
    streetlight(parent, -16.0, 384.0, "z", 1)
    streetlight(parent, 16.0, 300.0, "z", -1)
    streetlight(parent, -120.0, 414.0, "x", -1)
    streetlight(parent, 120.0, 414.0, "x", -1)

    pole_zs = [556.0, 448.0, 340.0, 268.0]
    for pz in pole_zs:
        utility_pole(parent, 24.0, pz, "z")
    for wi in range(len(pole_zs) - 1):
        for oi, off in enumerate((-3.3, 3.3, 0.0)):
            wire_span(parent, 24.0 + off, pole_zs[wi], 24.0 + off, pole_zs[wi + 1], 18.2, 1.7, wi * 3 + oi)
    utility_pole(parent, 26.0, 378.0, "x")
    for oi, off in enumerate((-3.3, 3.3)):
        wire_span(parent, 24.0 + off, 340.0, 26.0 + off, 378.0, 18.2, 1.4, 40 + oi)

    # ------------------------------------------------------------- house lots
    lots: List[Tuple[float, float, float, str, bool, bool]] = [
        (-74.0, 500.0, 90.0, "family", False, True),
        (74.0, 500.0, -90.0, "twoStory", False, True),
        (-74.0, 310.0, 90.0, "bungalow", True, False),
        (74.0, 310.0, -90.0, "cottage", False, True),
        (-160.0, 456.0, 180.0, "colonial", False, True),
        (160.0, 456.0, 180.0, "cottage", True, False),
        (-160.0, 344.0, 0.0, "bungalow", False, False),
        (160.0, 344.0, 0.0, "family", False, True),
    ]
    built: List[Dict[str, Any]] = []
    for hi, (hx, hz, hyaw, style, ruined, garage) in enumerate(lots):
        info = build_house(b, parent, rng, hx, hz, hyaw, style, f"SuburbanHouse_{hi + 1}",
                           ruined=ruined, has_garage=garage)
        build_lot(b, parent, rng, info, "Suburbs")
        built.append(info)

    def lot_aabb(info: Dict[str, Any], pad: float = 4.0) -> Tuple[float, float, float, float]:
        """World-space bounding box of a built house (works for 0/90/180/270 layouts)."""
        cx, cz, yaw = info["cx"], info["cz"], info["yaw"]
        hw, hd = info["w"] * 0.5 + pad, info["d"] * 0.5 + 10.0
        if abs(math.sin(math.radians(yaw))) > 0.5:
            return (cx - hd, cx + hd, cz - hw, cz + hw)
        return (cx - hw, cx + hw, cz - hd, cz + hd)

    reserved: List[Tuple[float, float, float, float]] = [
        (-21.0, 21.0, 250.0, 585.0),      # MapleMain corridor + sidewalks
        (-216.0, 216.0, 378.0, 424.0),    # CrescentLane corridor + sidewalks
        (-10.0, 10.0, 570.0, 620.0),       # shelter hatch headhouse & apron
    ]
    for info in built:
        reserved.append(lot_aabb(info))

    def clear_of(x: float, z: float, margin: float = 0.0) -> bool:
        for x0, x1, z0, z1 in reserved:
            if x0 - margin <= x <= x1 + margin and z0 - margin <= z <= z1 + margin:
                return False
        return True

    # -------------------------------------------------------- corner grocery
    grocery = build_house(b, parent, rng, 76.0, 224.0, -90.0, "colonial", "Mart24CornerGrocery",
                         ruined=False, has_garage=False)
    reserved.append(lot_aabb(grocery))
    gl: Lot = grocery["lot"]
    for si in range(6):
        # Aisle grid kept clear of the counters and the wreck that came through the frontage,
        # otherwise shelves end up welded through a table.
        grocery["model"].add_child(gl.part(f"GroceryShelf_{si}", (1.6, 5.6, 8.6),
                                          (-8.5 + (si % 3) * 5.5, 3.0, -8.5 + (si // 3) * 6.5),
                                          (146, 138, 116), MAT_SMOOTH_PLASTIC))
    sign = gl.part("GroceryFrontSign", (18.0, 2.8, 0.7), (0.0, 12.2, grocery["d"] * 0.5 + 2.6),
                   (222, 92, 58), MAT_NEON, cast_shadow=False)
    grocery["model"].add_child(sign)
    add_point_light(sign, (255, 112, 72), brightness=1.5, range_studs=34.0, shadows=False)
    _box(parent, "GroceryCanopy", (20.0, 0.7, 7.0), gl.pos(0.0, 9.6, grocery["d"] * 0.5 + 3.4),
         (150, 74, 52), MAT_METAL, pbr_variant="painted_metal")
    for ci in range(2):
        _box(parent, f"CanopyBracket_{ci}", (0.5, 3.0, 0.5),
             gl.pos(-8.0 + ci * 16.0, 8.0, grocery["d"] * 0.5 + 3.4), (118, 112, 104), MAT_METAL)
    b.add_loot_container(grocery["model"], "Suburbs", gl.pos(-2.0, 1.62, 2.0),
                        label="Search Grocery Shelving")
    b.add_loot_container(grocery["model"], "Hospital", gl.pos(7.0, 1.62, -4.0), (2.9, 2.4, 1.9),
                        (166, 66, 58), MAT_METAL, "Search Pharmacy Counter")
    b.add_lore_note(parent, "Lore_05_ShopkeeperLedger", "Mart-24 Backroom Note", gl.pos(0.0, 4.0, 0.0))

    # ------------------------------------------------------------- fuel stop
    gx0, gz0 = 206.0, 250.0
    _box(parent, "FuelPad", (46.0, 0.38, 36.0), (gx0, 0.19, gz0), (110, 108, 104), MAT_CONCRETE,
         pbr_variant="concrete")
    reserved.append((gx0 - 24.0, gx0 + 24.0, gz0 - 19.0, gz0 + 30.0))
    for px in (-14.0, 14.0):
        for pz in (-9.0, 9.0):
            _box(parent, "CanopyColumn", (1.7, 16.2, 1.7), (gx0 + px, 8.1, gz0 + pz), (152, 148, 142),
                 MAT_CONCRETE)
    _box(parent, "GasCanopyRoof", (38.0, 1.2, 24.0), (gx0, 16.2, gz0), (168, 86, 64), MAT_METAL,
         pbr_variant="painted_metal")
    _box(parent, "GasCanopyTrimN", (38.4, 1.5, 1.0), (gx0, 15.6, gz0 - 12.4), (228, 208, 120), MAT_METAL)
    _box(parent, "GasCanopyTrimS", (38.4, 1.5, 1.0), (gx0, 15.6, gz0 + 12.4), (228, 208, 120), MAT_METAL)
    for ci in range(2):
        can_light = _box(parent, f"CanopyLight_{ci}", (4.0, 0.5, 1.6), (gx0 - 9.0 + ci * 18.0, 15.2, gz0),
                         (250, 240, 214), MAT_NEON, cast_shadow=False, can_collide=False)
        add_point_light(can_light, (255, 244, 214), brightness=2.6, range_studs=46.0, shadows=False)
    for pi in range(4):
        pxx = gx0 - 9.0 + (pi % 2) * 18.0
        pzz = gz0 - 6.0 + (pi // 2) * 12.0
        pump = _box(parent, f"FuelPump_{pi}", (2.8, 5.2, 2.0), (pxx, 3.1, pzz), (176, 74, 56), MAT_METAL,
                    pbr_variant="painted_metal",
                    attrs={"InteractiveType": "FuelPump", "PromptLabel": "Siphon Remaining Diesel"})
        add_point_light(pump, (255, 196, 128), brightness=0.7, range_studs=11.0, shadows=False)
        _box(parent, f"PumpIsland_{pi}", (4.4, 0.7, 3.4), (pxx, 0.55, pzz), (134, 130, 124), MAT_CONCRETE)
        _box(parent, f"PumpHose_{pi}", (0.4, 0.4, 2.2), (pxx + 1.6, 3.4, pzz + 1.4), (48, 46, 44),
             MAT_FABRIC, can_collide=False)
    _box(parent, "PriceTotem", (9.0, 13.0, 1.4), (gx0 - 26.0, 6.5, gz0 + 6.0), (150, 146, 140), MAT_METAL,
         pbr_variant="painted_metal")
    _box(parent, "PriceTotemBoard", (7.4, 4.6, 0.4), (gx0 - 26.0, 10.4, gz0 + 5.2), (36, 40, 44),
         MAT_SMOOTH_PLASTIC)
    for di in range(3):
        _box(parent, f"PriceDigit_{di}", (1.4, 2.0, 0.25), (gx0 - 28.6 + di * 2.6, 10.4, gz0 + 4.9),
             (252, 132, 60), MAT_NEON, cast_shadow=False, can_collide=False)
    kiosk = build_house(b, parent, rng, gx0, gz0 + 30.0, 180.0, "bungalow", "FuelStopKiosk",
                        ruined=False, has_garage=False)
    reserved.append(lot_aabb(kiosk))
    b.add_loot_container(kiosk["model"], "Suburbs", kiosk["lot"].pos(0.0, 1.62, 0.0),
                        label="Search Register Draw")

    # ---------------------------------------------------------------- chapel
    chapel = RbxInstance("Model", "MapleRidgeChapel")
    cl = Lot(-92.0, 216.0, 90.0)
    _box(chapel, "ChapelFloor", (28.0, 0.9, 36.0), cl.pos(0, 0.45, 0), (128, 124, 116), MAT_CONCRETE)
    for si, sx in enumerate((-1, 1)):
        wall_run(cl, chapel, "z", sx * 14.0, 36.0, 0.9, 15.0, 1.0, (162, 156, 142), MAT_PLASTER,
                 "damaged_wall", [{"center": zz, "width": 3.6, "bottom": 6.4, "top": 11.6, "kind": "glass"}
                                  for zz in (-9.0, 0.0, 9.0)], f"ChapelWall{'W' if sx < 0 else 'E'}")
        window_units(cl, chapel, "z", sx * 14.5, 0.9, [{"center": zz, "width": 3.6, "bottom": 6.4,
                                                        "top": 11.6, "kind": "glass"} for zz in
                                                        (-9.0, 0.0, 9.0)], (74, 62, 54), rng, 0.0)
    wall_run(cl, chapel, "x", -18.0, 28.0, 0.9, 15.0, 1.0, (162, 156, 142), MAT_PLASTER, "damaged_wall",
             [], "ChapelWallBack")
    wall_run(cl, chapel, "x", 18.0, 28.0, 0.9, 15.0, 1.0, (162, 156, 142), MAT_PLASTER, "damaged_wall",
             [{"center": 0.0, "width": 6.6, "bottom": 0.0, "top": 9.6, "kind": "door"},
              {"center": -9.0, "width": 3.2, "bottom": 7.0, "top": 11.8, "kind": "glass"},
              {"center": 9.0, "width": 3.2, "bottom": 7.0, "top": 11.8, "kind": "glass"}], "ChapelFront")
    window_units(cl, chapel, "x", 18.5, 0.9, [{"center": cc_, "width": 3.2, "bottom": 7.0, "top": 11.8,
                                              "kind": "glass"} for cc_ in (-9.0, 9.0)], (74, 62, 54), rng, 0.0)
    lot_door(cl, chapel, "x", 18.0, 0.0, 0.9, 6.6, 9.4, (92, 64, 42), MAT_WOOD)
    gable_roof(cl, chapel, 28.0, 36.0, 16.0, 7.5, 2.0, (86, 70, 60), (162, 156, 142), MAT_PLASTER,
               "damaged_wall")
    for ri in range(6):
        _box(chapel, f"PewRow_{ri}", (18.0, 1.1, 1.3), cl.pos(0.0, 1.6, -12.0 + ri * 4.2),
             (112, 84, 58), MAT_WOOD_PLANKS)
    _box(chapel, "Pulpit", (5.0, 3.2, 3.4), cl.pos(0.0, 2.5, -15.4), (120, 92, 64), MAT_WOOD_PLANKS)
    _box(chapel, "SteepleTower", (7.0, 22.0, 7.0), cl.pos(-14.0, 26.0, 0.0), (166, 160, 146), MAT_PLASTER)
    _box(chapel, "SteepleRoof", (8.2, 3.4, 8.2), cl.pos(-14.0, 38.6, 0.0), (86, 70, 60), MAT_SHINGLE)
    _box(chapel, "SteepleSpire", (3.0, 8.0, 3.0), cl.pos(-14.0, 43.4, 0.0), (86, 70, 60), MAT_SHINGLE)
    cross_v = _box(chapel, "ChapelCross", (0.55, 4.6, 0.55), cl.pos(-14.0, 49.4, 0.0), (244, 228, 176),
                   MAT_NEON, cast_shadow=False)
    _box(chapel, "ChapelCrossH", (2.9, 0.55, 0.55), cl.pos(-14.0, 50.2, 0.0), (244, 228, 176), MAT_NEON,
         cast_shadow=False)
    add_point_light(cross_v, (255, 232, 176), brightness=1.5, range_studs=58.0, shadows=False)
    for li in range(2):
        lantern = _box(chapel, f"ChapelLantern_{li}", (1.0, 1.6, 1.0), cl.pos(0.0, 3.4, -6.0 + li * 12.0),
                       (250, 220, 150), MAT_NEON, cast_shadow=False, can_collide=False)
        add_point_light(lantern, (255, 206, 138), brightness=1.5, range_studs=30.0, shadows=False)
    b.add_loot_container(chapel, "City", cl.pos(0.0, 2.07, -13.0), (3.2, 2.3, 2.1), (112, 90, 62),
                        MAT_WOOD_PLANKS, "Search Vicar's Supply Chest")
    deck = _box(chapel, "SermonTapeDeck", (2.2, 1.0, 1.5), cl.pos(0.0, 3.1, -15.4), (96, 90, 78),
                MAT_SMOOTH_PLASTIC, can_collide=False)
    _box(chapel, "SermonReelA", (0.7, 0.24, 0.7), cl.pos(-0.5, 3.7, -15.4), (52, 48, 44), MAT_FABRIC,
         shape=SHAPE_CYLINDER, can_collide=False)
    _box(chapel, "SermonReelB", (0.7, 0.24, 0.7), cl.pos(0.5, 3.7, -15.4), (52, 48, 44), MAT_FABRIC,
         shape=SHAPE_CYLINDER, can_collide=False)
    parent.add_child(chapel)
    reserved.append((-124.0, -60.0, 188.0, 244.0))

    # --------------------------------------------------- village green & props
    _box(parent, "GreenLawn", (56.0, 0.3, 36.0), (0.0, 0.16, 236.0), (104, 112, 78), MAT_LEAFY_GRASS,
         pbr_variant="mud")
    bench = RbxInstance("Model", "GreenPicnicBench")
    bl = Lot(-26.0, 246.0, 24.0)
    _box(bench, "BenchSeat", (7.0, 0.5, 2.0), bl.pos(0, 1.7, 0), (112, 88, 60), MAT_WOOD_PLANKS)
    _box(bench, "BenchBack", (7.0, 1.8, 0.4), bl.pos(0, 2.7, -0.9), (112, 88, 60), MAT_WOOD_PLANKS)
    for li in (-1, 1):
        _box(bench, f"BenchLeg{li}", (0.5, 1.7, 1.8), bl.pos(li * 3.0, 0.85, 0), (86, 82, 78), MAT_METAL)
    parent.add_child(bench)
    fire = RbxInstance("Model", "AbandonedCampfire")
    fl = Lot(-14.0, 262.0, 0.0)
    for ri in range(8):
        ang = math.radians(ri * 45)
        _box(fire, f"FireRingStone_{ri}", (1.5, 0.9, 1.1), fl.pos(math.cos(ang) * 2.6, 0.45,
             math.sin(ang) * 2.6), (118, 114, 108), MAT_BASALT)
    _box(fire, "FireAshPit", (3.4, 0.3, 3.4), fl.pos(0, 0.2, 0), (58, 52, 48), MAT_MULCH)
    for ci in range(4):
        ca = math.radians(30 + ci * 90)
        fire.add_child(make_part(f"CharredLog_{ci}", (3.0, 0.55, 0.55),
                                cf_euler(fl.pos(math.cos(ca) * 1.0, 0.6, math.sin(ca) * 1.0)[0], 0.6,
                                         fl.pos(math.cos(ca) * 1.0, 0.6, math.sin(ca) * 1.0)[2], 0,
                                         math.degrees(ca), -12), (44, 38, 34), MAT_WOOD, can_collide=False))
    parent.add_child(fire)
    aid = RbxInstance("Model", "RedCrossAidStation")
    # Kept clear of the delivery truck that crashed into the grocery frontage.
    al = Lot(26.0, 264.0, -30.0)
    _box(aid, "AidTentFloor", (12.0, 0.4, 10.0), al.pos(0, 0.2, 0), (142, 138, 130), MAT_PAVEMENT)
    for px_ in (-5.4, 5.4):
        for pz_ in (-4.2, 4.2):
            _box(aid, f"AidPole_{px_}_{pz_}", (0.45, 8.0, 0.45), al.pos(px_, 4.0, pz_), (148, 146, 142),
                 MAT_METAL)
    _box(aid, "AidTentCanopy", (13.0, 0.6, 11.4), al.pos(0, 8.2, 0), (206, 202, 192), MAT_FABRIC,
         pbr_variant="fabric")
    _box(aid, "AidCrossV", (0.9, 3.4, 0.25), al.pos(0, 6.0, -5.8), (198, 62, 52), MAT_NEON,
         cast_shadow=False)
    _box(aid, "AidCrossH", (3.4, 0.9, 0.25), al.pos(0, 6.0, -5.8), (198, 62, 52), MAT_NEON,
         cast_shadow=False)
    _box(aid, "AidCabinet", (3.4, 4.4, 1.6), al.pos(0, 2.4, 2.6), (214, 210, 200), MAT_SMOOTH_PLASTIC,
         attrs={"StationType": "MedCabinet", "PromptLabel": "Raid Field Triage Cabinet"})
    parent.add_child(aid)
    b.add_loot_container(aid, "Hospital", al.pos(-3.4, 1.35, -1.0), (2.6, 2.2, 1.8), (196, 190, 176),
                        MAT_SMOOTH_PLASTIC, "Search Triage Bag")
    reserved.append((34.0, 60.0, 232.0, 268.0))
    reserved.append((-40.0, -6.0, 236.0, 274.0))

    # ------------------------------------- roadblock at the southern entrance
    for si in range(5):
        sx = -36.0 + si * 18.0
        _box(parent, f"RoadBarrier_{si}", (9.4, 1.6, 0.8), (sx, 0.9, 262.0), (186, 88, 52), MAT_PLASTER,
             pbr_variant="damaged_wall")
        _box(parent, f"BarrierStripe_{si}", (9.4, 0.6, 0.9), (sx, 1.5, 262.0), (228, 222, 210), MAT_PLASTER)
    for oi in range(3):
        _box(parent, f"TrafficCone_{oi}", (1.1, 1.7, 1.1), (-16.0 + oi * 15.0, 0.85, 274.0 + oi * 3.0),
             (214, 108, 44), MAT_SMOOTH_PLASTIC)
    for sbi in range(6):
        _box(parent, f"SandbagWall_{sbi}", (4.0, 1.1, 2.2), (-10.0 + (sbi % 3) * 10.0, 0.55 + (sbi // 3) * 1.0,
             268.0), (150, 140, 108), MAT_FABRIC, pbr_variant="fabric")

    # ------------------------------------- notice boards for the story notes
    for ni, (nx, nz, nyaw) in enumerate(((-42.0, 520.0, -90.0), (52.0, 520.0, 90.0))):
        nb = RbxInstance("Model", f"EvacNoticeBoard_{ni}")
        nl = Lot(nx, nz, nyaw)
        for pi in (-1, 1):
            _box(nb, f"NoticePost_{pi}", (0.7, 6.0, 0.7), nl.pos(pi * 3.2, 3.0, 0), (96, 78, 58), MAT_WOOD)
        _box(nb, "NoticeBoard", (7.4, 4.2, 0.5), nl.pos(0, 4.6, 0), (172, 164, 146), MAT_CARD,
             pbr_variant="damaged_wall")
        _box(nb, "NoticeRoof", (8.4, 0.4, 1.8), nl.pos(0, 6.9, 0.4), (108, 88, 70), MAT_SHINGLE)
        for pi in range(3):
            if pi == 1:
                continue  # the middle slot carries the readable story note
            _box(nb, f"NoticeFlyer_{pi}", (1.9, 2.4, 0.08), nl.pos(-2.2 + pi * 2.2, 4.4 + (pi % 2) * 0.5,
                 -0.32), (226, 220, 198), MAT_FABRIC, can_collide=False, cast_shadow=False)
        # The Act I order/diary is pinned to the board facing the street instead of hovering
        # next to it, so the interaction raycast lands on paper.
        b.add_lore_note(nb, ("Lore_03_EvacNotice", "Lore_04_SuburbDiary")[ni],
                        ("Civil Defense Evacuation Order #14", "Water-Stained Kitchen Journal")[ni],
                        nl.pos(0.0, 4.9, -0.36), size=(2.6, 1.9, 0.1), yaw_deg=nyaw)
        parent.add_child(nb)

    # ------------------------------------------------------------- wrecks
    for vi, (vx, vz, vyaw, vcol, flipped, burned) in enumerate([
        (-8.0, 545.0, 6.0, (146, 108, 82), False, False),
        (9.0, 470.0, -174.0, (104, 120, 136), False, True),
        (-11.0, 350.0, 4.0, (138, 132, 124), True, False),
        (8.0, 288.0, -8.0, (126, 86, 76), False, False),
        (60.0, 405.0, 4.0, (154, 144, 118), False, True),
        (-70.0, 396.0, -176.0, (108, 124, 106), False, False),
        (170.0, 415.0, 8.0, (132, 96, 74), True, False),
        (196.0, 262.0, 84.0, (120, 126, 136), False, True),
    ]):
        wrecked_car(parent, vx, vz, vyaw, vcol, flipped=flipped, burned=burned,
                    model_name=f"WreckedSedan_{vi + 1}")

    # Delivery truck that punched through the grocery frontage
    truck = Lot(44.0, 240.0, 66.0)
    tm = RbxInstance("Model", "OverturnedDeliveryTruck")
    tm.add_child(truck.part("TruckTrailer", (9.6, 9.2, 26.0), (0, 5.8, -7.0), (158, 152, 140), MAT_METAL,
                           pbr_variant="painted_metal"))
    tm.add_child(truck.part("TruckCab", (9.2, 6.4, 8.4), (0.6, 4.2, 9.4), (176, 96, 62),
                           MAT_CORRODED_METAL, pbr_variant="rust",
                           attrs={"InteractiveType": "VehicleAlarm", "PromptLabel": "Search Truck Cab"}))
    for wi, (wx, wz) in enumerate(((-4.4, 11.0), (4.4, 11.0), (-4.4, -9.0), (4.4, -9.0))):
        tm.add_child(truck.part(f"TruckWheel_{wi}", (2.0, 3.8, 3.8), (wx, 1.9, wz), (38, 36, 36),
                               MAT_FABRIC, shape=SHAPE_CYLINDER, roll=90.0))
    parent.add_child(tm)
    reserved.append((22.0, 70.0, 214.0, 266.0))
    reserved.append((-1200.0, 1200.0, 158.0, 192.0))
    reserved.append((12.0, 48.0, -1060.0, 160.0))
    reserved.append((12.0, 48.0, 640.0, 1180.0))

    # ------------------------------- civil defense relay mast (Act II target)
    mast_x, mast_z = 150.0, 560.0
    reserved.append((mast_x - 22.0, mast_x + 30.0, mast_z - 24.0, mast_z + 24.0))
    leg_xy = []
    for li in range(4):
        ang = math.radians(45 + li * 90)
        lx = mast_x + math.cos(ang) * 3.4
        lz = mast_z + math.sin(ang) * 3.4
        leg_xy.append((lx, lz))
        _box(parent, f"MastLeg_{li}", (0.95, 60.0, 0.95), (lx, 30.0, lz), (136, 112, 98),
             MAT_CORRODED_METAL, pbr_variant="rust")
    for li in range(4):
        nx = (li + 1) % 4
        for bi in range(8):
            y0 = 1.2 + bi * 7.2
            y1 = y0 + 7.2
            if (li + bi) % 2 == 0:
                beam_between(parent, f"MastBrace_{li}_{bi}", (leg_xy[li][0], y0, leg_xy[li][1]),
                             (leg_xy[nx][0], y1, leg_xy[nx][1]), 0.42, (128, 104, 92), MAT_CORRODED_METAL)
            else:
                beam_between(parent, f"MastBrace_{li}_{bi}", (leg_xy[nx][0], y0, leg_xy[nx][1]),
                             (leg_xy[li][0], y1, leg_xy[li][1]), 0.42, (128, 104, 92), MAT_CORRODED_METAL)
        beam_between(parent, f"MastRing_{li}", (leg_xy[li][0], 57.6, leg_xy[li][1]),
                     (leg_xy[(li + 1) % 4][0], 57.6, leg_xy[(li + 1) % 4][1]), 0.5, (134, 110, 96),
                     MAT_CORRODED_METAL)
    _box(parent, "MastPlatform", (11.5, 0.7, 11.5), (mast_x, 57.0, mast_z), (124, 120, 116),
         MAT_DIAMOND_PLATE, pbr_variant="industrial_machinery")
    _box(parent, "MastAntenna", (1.1, 17.0, 1.1), (mast_x, 66.0, mast_z), (148, 144, 140), MAT_METAL)
    _box(parent, "MastDish", (7.0, 7.0, 1.0), (mast_x - 4.6, 51.0, mast_z), (142, 138, 130),
         MAT_SMOOTH_PLASTIC, shape=SHAPE_CYLINDER, roll=90.0)
    beacon = _box(parent, "RelayBeacon", (1.8, 1.8, 1.8), (mast_x, 75.0, mast_z), (238, 84, 62), MAT_NEON,
                 shape=SHAPE_BALL, cast_shadow=False)
    add_point_light(beacon, (255, 84, 60), brightness=3.0, range_studs=64.0, shadows=False)
    parent.add_child(make_truss_ladder("RelayClimbLadder", mast_x - 4.0, 0.0, mast_z, 57.0))
    for gi in range(4):
        ga = math.radians(45 + gi * 90)
        ax_, az_ = mast_x + math.cos(ga) * 16.0, mast_z + math.sin(ga) * 16.0
        _box(parent, f"MastGuyAnchor_{gi}", (1.5, 1.7, 1.5), (ax_, 0.85, az_), (108, 104, 100),
             MAT_CONCRETE)
        beam_between(parent, f"MastGuyWire_{gi}", (ax_, 1.5, az_), (mast_x + math.cos(ga) * 3.0, 55.5,
                     mast_z + math.sin(ga) * 3.0), 0.16, (44, 44, 46), MAT_METAL, can_collide=False,
                     cast_shadow=False)
    relay_console = _box(parent, "RelayControlBox", (3.8, 4.6, 2.8), (mast_x, 2.3, mast_z + 6.4),
                        (92, 104, 96), MAT_METAL, pbr_variant="painted_metal",
                        attrs={"QuestInteractId": "Relay_Suburbs",
                               "PromptLabel": "Power Up Civil Defense Relay Mast"})
    add_point_light(relay_console, (214, 174, 96), brightness=1.6, range_studs=15.0, shadows=False)
    b.interactables.add_child(relay_console)
    _box(parent, "RelayShack", (12.0, 6.6, 10.0), (mast_x - 18.0, 3.3, mast_z + 12.0), (132, 118, 98),
         MAT_WOOD_PLANKS, pbr_variant="wood")
    _box(parent, "RelayShackRoof", (14.0, 0.6, 12.0), (mast_x - 18.0, 6.9, mast_z + 12.0), (84, 68, 58),
         MAT_SHINGLE)
    b.add_loot_container(parent, "Suburbs", (mast_x - 18.0, 1.35, mast_z + 8.0), (2.8, 2.2, 1.9),
                        (96, 84, 62), MAT_WOOD_PLANKS, "Search Lineman Tool Chest")

    # --------------------------------------------- scattered debris & treeline
    for di in range(26):
        for _ in range(8):
            dx = rng.uniform(-200.0, 200.0)
            dz = rng.uniform(170.0, 600.0)
            if clear_of(dx, dz, 2.0):
                break
        else:
            continue
        debris_pile(parent, dx, dz, rng, radius=rng.uniform(2.2, 4.6),
                    kind=("rubble", "wood", "sandbag")[di % 3])
    for ti in range(120):
        for _ in range(6):
            tx = rng.uniform(-330.0, 330.0)
            tz = rng.uniform(130.0, 640.0)
            if clear_of(tx, tz, 9.0):
                break
        else:
            continue
        add_dead_tree(parent, Lot(tx, tz, rng.uniform(0, 360)), 0.0, 0.0, rng,
                     scale=rng.uniform(0.7, 1.35), live=rng.random() < 0.28)
    for vi in range(30):
        vx = -330.0 + (vi % 2) * 660.0 + rng.uniform(-14.0, 14.0)
        vz = rng.uniform(140.0, 640.0)
        if not clear_of(vx, vz, 6.0):
            continue
        add_dead_tree(parent, Lot(vx, vz, rng.uniform(0, 360)), 0.0, 0.0, rng,
                     scale=rng.uniform(0.8, 1.4), live=False)
    for fi in range(9):
        fx = rng.uniform(-190.0, 190.0)
        fz = rng.uniform(180.0, 600.0)
        if not clear_of(fx, fz, 3.0):
            continue
        _box(parent, f"OverturnedTrashBin_{fi}", (2.6, 1.6, 1.9), (fx, 0.9, fz), (96, 104, 96),
             MAT_CORRODED_METAL, pbr_variant="rust")
        parent.children[-1].set_prop("CFrame", cf_euler(fx, 0.9, fz, rng.uniform(-24, 24),
                                                        rng.uniform(0, 360), rng.uniform(-18, 18)))

    # ---------------------------------------------------------- story & threats
    b.add_loot_container(built[0]["model"], "Suburbs", built[0]["lot"].pos(5.0, 1.62, -3.0),
                        (3.2, 2.4, 2.1), (98, 88, 72), MAT_METAL, "Search Evacuation Crate")

    for sx, sz, sp in [(-24.0, 560.0, "Hollow"), (26.0, 486.0, "Crawler"), (30.0, 400.0, "Screamer"),
                       (-64.0, 402.0, "Hollow"), (148.0, 424.0, "Crawler"), (-146.0, 300.0, "Hollow"),
                       (14.0, 292.0, "Stalker"), (0.0, 246.0, "Hollow"), (128.0, 546.0, "Brute")]:
        b.add_enemy_spawn(sx, 1.6, sz, "DeadSuburbs", sp)


# --------------------------------------------------------- shelter assembly
def build_shelter_echo07(b: Any, parent: RbxInstance, rng: random.Random) -> None:
    """Underground Emergency Shelter Echo-07 — furnished, lit, readable at spawn."""
    shelter = RbxInstance("Model", "EmergencyShelter_Echo07")
    floor_y, ceil_y = -20.75, -6.0

    _box(shelter, "VaultFloor", (44.0, 1.5, 88.0), (0, floor_y, 621), (142, 136, 126), MAT_CONCRETE,
         pbr_variant="concrete")
    _box(shelter, "VaultSumpPlate", (11.0, 1.6, 12.0), (-14.0, floor_y + 0.06, 640), (132, 134, 138),
         MAT_DIAMOND_PLATE, pbr_variant="industrial_machinery")
    # Ceiling leaves a hole exactly where the stairwell rises (|x| < 7, z 578..616)
    _box(shelter, "VaultCeiling_Main", (44.0, 2.0, 33.0), (0, ceil_y, 632.5), (124, 120, 114), MAT_CONCRETE,
         pbr_variant="concrete")
    for sgn in (-1, 1):
        _box(shelter, f"VaultCeiling_Wing{sgn}", (15.0, 2.0, 25.0), (sgn * 14.5, ceil_y, 603.5),
             (124, 120, 114), MAT_CONCRETE, pbr_variant="concrete")
    for wn, wsz, wpos in (
        ("VaultWall_S", (44.0, 16.0, 2.0), (0, -13.0, 649.0)),
        ("VaultWall_N_W", (15.3, 16.0, 2.0), (-14.35, -13.0, 591.0)),
        ("VaultWall_N_E", (15.3, 16.0, 2.0), (14.35, -13.0, 591.0)),
        ("VaultWall_W", (2.0, 16.0, 58.0), (-22.0, -13.0, 620.0)),
        ("VaultWall_E", (2.0, 16.0, 58.0), (22.0, -13.0, 620.0)),
    ):
        _box(shelter, wn, wsz, wpos, (150, 144, 132), MAT_CONCRETE, pbr_variant="damaged_wall")
    # Two-tone wall bands so walls separate from the floor at a glance
    for bi in range(3):
        yy = -19.4 + bi * 4.2
        _box(shelter, f"Wainscot_N{bi}", (43.4, 0.5, 0.32), (0, yy, 647.4), (86, 104, 78), MAT_PLASTER)
        _box(shelter, f"Wainscot_W{bi}", (0.32, 0.5, 57.4), (-20.6, yy, 620), (86, 104, 78), MAT_PLASTER)
        _box(shelter, f"Wainscot_E{bi}", (0.32, 0.5, 57.4), (20.6, yy, 620), (86, 104, 78), MAT_PLASTER)
    for pi in range(5):
        _box(shelter, "CeilingPipe", (1.0, 1.0, 54.0), (-16.0 + pi * 8.0, -7.6, 622), (118, 110, 100),
             MAT_CORRODED_METAL, pbr_variant="rust", can_collide=False)
    _box(shelter, "CeilingConduit", (40.0, 0.7, 0.7), (0, -8.3, 610), (108, 106, 102), MAT_METAL,
         can_collide=False)
    _box(shelter, "FloorCableRun", (2.6, 0.2, 46.0), (-8.0, -19.9, 622), (42, 40, 38), MAT_FABRIC,
         can_collide=False)

    # Stairwell up to the headhouse (walkable steps + rails)
    # Ramp spans exactly the shaft: floor level at the hatch door (y = 0, z = 578.6)
    # down to the vault floor (y = -20, z = 613) at a walkable 30-degree slope.
    shelter.add_child(make_part("ExitRampToSurface", (10.0, 1.1, 39.8),
                                cf_euler(0, -10.0, 595.8, 30.15, 0, 0), (136, 132, 124), MAT_CONCRETE,
                                pbr_variant="concrete"))
    for ri in range(18):
        shelter.add_child(make_part(f"ExitStep_{ri}", (10.0, 0.36, 0.68),
                                    cf_pos(0, -19.2 + ri * 1.08, 612.0 - ri * 1.85), (158, 152, 142),
                                    MAT_CONCRETE, cast_shadow=False))
    for side in (-1, 1):
        shelter.add_child(make_part(f"RampRail{side}", (0.45, 2.4, 39.8),
                                    cf_euler(side * 4.9, -7.4, 595.8, 30.15, 0, 0), (108, 110, 112),
                                    MAT_METAL))
    for sgn in (-1, 1):
        _box(shelter, f"HeadhouseWall_{'W' if sgn < 0 else 'E'}", (1.2, 30.0, 38.0), (sgn * 6.0, -5.0, 597.0),
             (148, 142, 132), MAT_CONCRETE, pbr_variant="damaged_wall")
    _box(shelter, "HeadhouseWall_N", (13.2, 30.0, 1.2), (0.0, -5.0, 615.4), (148, 142, 132), MAT_CONCRETE,
         pbr_variant="damaged_wall")
    # South face carries the hatch door (opening 5.4 wide, floor to 8.4)
    for sgn in (-1, 1):
        _box(shelter, f"HeadhouseDoorPier_{sgn}", (3.6, 10.0, 1.2), (sgn * 4.5, 5.0, 578.6), (152, 146, 136),
             MAT_CONCRETE, pbr_variant="damaged_wall")
    _box(shelter, "HeadhouseDoorHeader", (12.6, 1.8, 1.2), (0.0, 9.3, 578.6), (152, 146, 136), MAT_CONCRETE)
    _box(shelter, "HeadhouseSign", (10.0, 2.2, 0.55), (0.0, 11.4, 577.9), (72, 116, 82), MAT_METAL,
         pbr_variant="painted_metal")
    _box(shelter, "HeadhouseSignGlow", (8.6, 1.1, 0.2), (0.0, 11.4, 577.5), (168, 232, 186), MAT_NEON,
         cast_shadow=False, can_collide=False)
    b.add_hinged_door(shelter, 0.0, 0.0, 578.2, width=5.4, height=8.4, outward=(0.0, -1.0))
    for sgn in (-1, 1):
        _box(shelter, f"HeadhouseVents{sgn}", (1.6, 2.6, 1.6), (sgn * 3.6, 11.5, 608.0), (128, 122, 112),
             MAT_CORRODED_METAL, shape=SHAPE_CYLINDER)
    _box(shelter, "HeadhouseRoof", (15.0, 0.9, 40.0), (0.0, 9.9, 597.0), (128, 124, 118), MAT_CONCRETE,
         pbr_variant="concrete")
    _box(shelter, "HeadhouseRoofRim", (16.4, 0.7, 41.4), (0.0, 10.6, 597.0), (98, 96, 92), MAT_CONCRETE)

    spawn = RbxInstance("SpawnLocation", "Echo07AwakeningSpawn")
    spawn.set_prop("Size", Vector3Val(7.0, 0.6, 7.0))
    spawn.set_prop("CFrame", cf_pos(0.0, -19.7, 618.0))
    spawn.set_prop("Anchored", True)
    spawn.set_prop("Transparency", 1.0)
    spawn.set_prop("Neutral", True)
    shelter.add_child(spawn)

    # Sleeping bay
    _box(shelter, "BunkPlatform", (10.0, 0.7, 12.0), (-15.5, -20.1, 632.0), (124, 118, 110), MAT_CONCRETE)
    _box(shelter, "EmergencyRestBunk", (4.4, 2.0, 8.2), (-17.4, -18.6, 632.0), (140, 146, 128),
         MAT_FABRIC, pbr_variant="fabric",
         attrs={"InteractiveType": "RestBunk", "PromptLabel": "Rest on Bunk (Recover Fatigue)"})
    _box(shelter, "BunkBlanket", (4.2, 0.45, 4.8), (-17.4, -17.35, 633.8), (104, 112, 96), MAT_FABRIC)
    _box(shelter, "BunkPillow", (2.6, 0.8, 1.7), (-17.4, -17.3, 629.0), (180, 176, 160), MAT_FABRIC)
    _box(shelter, "BunkLadder", (1.6, 2.0, 0.4), (-13.0, -19.5, 626.5), (118, 114, 108), MAT_METAL)

    # Command desk + terminal + field radio + handwritten note
    _box(shelter, "CommandDesk", (7.6, 3.0, 3.4), (14.5, -18.9, 626.0), (122, 124, 126), MAT_METAL)
    _box(shelter, "DeskChair", (2.2, 3.0, 2.2), (14.5, -18.9, 621.8), (104, 108, 110), MAT_METAL)
    term = _box(shelter, "Echo07Terminal", (3.0, 1.9, 0.6), (14.5, -16.9, 627.4), (158, 208, 172),
               MAT_NEON, cast_shadow=False, can_collide=False)
    add_point_light(term, (140, 226, 176), brightness=1.6, range_studs=18.0, shadows=False)
    b.add_lore_note(shelter, "Lore_01_EchoTerminal", "Echo-07 Terminal Log #044", (14.5, -17.1, 626.4),
                   is_terminal=True)
    _box(shelter, "RadioCrate", (5.4, 3.2, 3.6), (6.0, -18.8, 634.0), (118, 96, 66), MAT_WOOD_PLANKS,
         pbr_variant="wood")
    _box(shelter, "FieldRadio", (2.4, 1.2, 1.6), (6.0, -16.6, 634.0), (84, 96, 88), MAT_METAL,
         pbr_variant="painted_metal",
         attrs={"InteractiveType": "Radio", "PromptLabel": "Tune 142.8 MHz Field Receiver"})
    _box(shelter, "RadioAntenna", (0.16, 3.6, 0.16), (7.0, -14.4, 634.0), (160, 160, 158), MAT_METAL,
         can_collide=False)
    _box(shelter, "HandwrittenNote", (1.5, 0.12, 1.9), (4.6, -16.9, 632.6), (226, 218, 194), MAT_FABRIC,
         cast_shadow=False, can_collide=False)
    b.add_lore_note(shelter, "Lore_02_BunkerWallNote", "Scrawled Hatch Warning", (21.34, -16.4, 606.0),
                    size=(0.12, 1.7, 1.3))

    # Workbench, lockers, med cabinet, generator, barrels
    _box(shelter, "EchoWorkbench", (6.8, 3.4, 3.2), (-15.5, -18.6, 618.0), (140, 116, 82),
         MAT_WOOD_PLANKS, pbr_variant="wood",
         attrs={"StationType": "Workbench", "PromptLabel": "Use Shelter Workbench"})
    for ti in range(3):
        _box(shelter, f"BenchTool_{ti}", (0.55, 0.4, 1.6), (-17.6 + ti * 2.0, -16.7, 617.2),
             (140, 138, 134), MAT_METAL)
    for li in range(5):
        _box(shelter, f"LockerRow_{li}", (2.5, 8.0, 1.6), (-4.0 + li * 2.7, -16.3, 647.4),
             (118, 126, 130), MAT_METAL, pbr_variant="painted_metal")
        _box(shelter, f"LockerVent_{li}", (1.6, 0.4, 0.2), (-4.0 + li * 2.7, -13.2, 646.5),
             (72, 78, 82), MAT_METAL, can_collide=False)
    _box(shelter, "MedCabinet", (3.2, 3.6, 1.3), (20.4, -15.5, 640.0), (208, 202, 190),
         MAT_SMOOTH_PLASTIC, attrs={"StationType": "MedCabinet", "PromptLabel": "Open Trauma Cabinet"})
    _box(shelter, "MedCabinetCrossH", (1.8, 0.45, 0.14), (20.4, -14.3, 639.3), (206, 66, 54), MAT_NEON,
         can_collide=False, cast_shadow=False)
    _box(shelter, "MedCabinetCrossV", (0.45, 1.8, 0.14), (20.4, -15.2, 639.3), (206, 66, 54), MAT_NEON,
         can_collide=False, cast_shadow=False)
    gen = _box(shelter, "ShelterGenerator", (6.4, 4.6, 4.2), (-18.0, -18.0, 606.0), (146, 112, 60),
              MAT_METAL, pbr_variant="industrial_machinery",
              attrs={"InteractiveType": "Generator", "PromptLabel": "Crank Emergency Generator"})
    add_point_light(gen, (255, 182, 92), brightness=1.3, range_studs=18.0, shadows=False)
    _box(shelter, "GeneratorExhaust", (0.8, 6.0, 0.8), (-18.0, -13.0, 604.5), (108, 100, 92),
         MAT_CORRODED_METAL, shape=SHAPE_CYLINDER, can_collide=False)
    for bi, (bx, bz, bcol) in enumerate(((18.5, 610.0, (108, 118, 100)), (15.6, 610.0, (118, 102, 84)),
                                        (18.5, 606.0, (96, 104, 96)))):
        _box(shelter, f"CoolantBarrel_{bi}", (2.3, 3.4, 2.3), (bx, -18.5, bz), bcol, MAT_METAL,
             shape=SHAPE_CYLINDER, pbr_variant="rust")

    # Overhead fixtures (bright enough to read the room) + hazard floor guide strip
    for ci in range(3):
        cx_ = -12.0 + ci * 12.0
        _box(shelter, f"LampHousing_{ci}", (3.4, 0.8, 2.2), (cx_, -7.4, 622.0), (134, 132, 128), MAT_METAL,
             can_collide=False)
        fixture = _box(shelter, f"CeilingFixture_{ci}", (2.8, 0.5, 1.5), (cx_, -7.0, 622.0),
                      (246, 234, 198), MAT_NEON, cast_shadow=False, can_collide=False)
        add_point_light(fixture, (255, 224, 176), brightness=2.9, range_studs=52.0, shadows=False)
    sl = _box(shelter, "StairwellLight", (1.6, 0.5, 1.2), (0, -9.0, 604.0), (248, 236, 202), MAT_NEON,
              cast_shadow=False, can_collide=False)
    add_point_light(sl, (255, 226, 178), brightness=2.2, range_studs=38.0, shadows=False)
    _box(shelter, "EmergencyFloorStrip", (40.0, 0.14, 0.8), (0, -19.9, 620.0), (212, 92, 62), MAT_NEON,
         cast_shadow=False, can_collide=False)
    for si in range(6):
        _box(shelter, f"FloorChevron_{si}", (1.6, 0.16, 0.5), (-3.0 + (si % 2) * 6.0, -19.9,
             619.0 + si * 5.0), (226, 210, 120), MAT_PLASTER, can_collide=False)
    b.add_hanging_physics_lamp(shelter, 0.0, -7.2, 636.0, (255, 208, 150))
    b.add_hanging_physics_lamp(shelter, -14.0, -7.2, 612.0, (255, 196, 138))

    # Grow bay (environmental storytelling)
    for gi in range(6):
        gx_ = -2.0 + (gi % 3) * 3.8
        gz_ = 640.0 + (gi // 3) * 2.6
        _box(shelter, f"GrowPlanter_{gi}", (3.4, 1.3, 1.7), (gx_, -19.75, gz_), (88, 96, 74), MAT_MULCH)
        _box(shelter, f"GrowPlant_{gi}", (1.4, 1.2, 1.4), (gx_, -18.5, gz_), (72, 96, 60),
             MAT_LEAFY_GRASS, can_collide=False)
    glamp = _box(shelter, "GrowLamp", (9.0, 0.5, 1.6), (1.9, -12.0, 641.3), (196, 132, 210), MAT_NEON,
                 cast_shadow=False, can_collide=False)
    add_point_light(glamp, (206, 152, 220), brightness=1.1, range_studs=20.0, shadows=False)

    b.add_loot_container(shelter, "Suburbs", (-19.6, -18.4, 642.0), (3.4, 3.2, 2.4), (110, 118, 122),
                        MAT_METAL, "Search Emergency Reserve Locker")
    b.add_loot_container(shelter, "Suburbs", (19.6, -18.4, 642.0), (3.4, 3.2, 2.4), (118, 106, 88),
                        MAT_METAL, "Search Ration Footlocker")
    b.add_loot_container(shelter, "Suburbs", (-4.0, -18.3, 644.5), (2.8, 2.4, 2.0), (124, 100, 70),
                        MAT_WOOD_PLANKS, "Search Spare Parts Bin")

    parent.add_child(shelter)
