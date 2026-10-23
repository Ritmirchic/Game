#!/usr/bin/env python3
"""
ASHES OF HUMANITY: LAST SIGNAL — Master World & Place Binary Assembler
======================================================================
Builds the complete, cohesive, playable Roblox Studio place file:
  /home/user/Game/ASHES_OF_HUMANITY.rbxl

Every coordinate, region boundary, LoreId, QuestInteractId, LootTable,
WeaponTemplate, EnemyTemplate, BuildingTemplate, MaterialVariant, and
RemoteEvent matches Config.luau, QuestDefinitions.luau, WeaponDefinitions.luau,
EnemyDefinitions.luau, BuildingDefinitions.luau, and NetworkSchema.luau 1-to-1.
"""

from __future__ import annotations
import json
import math
import random
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

from rbxl_binary import (
    CFrameVal,
    Color3Val,
    ColorSequenceVal,
    NumberRangeVal,
    NumberSequenceVal,
    RbxInstance,
    RbxlSerializer,
    ReflectionDB,
    Vector3Val,
    inspect_rbxl_binary,
)

# Verified Roblox Enum.Material values
MAT_SMOOTH_PLASTIC = 272
MAT_NEON = 288
MAT_WOOD = 512
MAT_WOOD_PLANKS = 528
MAT_MARBLE = 784
MAT_BASALT = 788
MAT_SLATE = 800
MAT_CRACKED_LAVA = 804
MAT_CONCRETE = 816
MAT_BRICK = 848
MAT_CORRODED_METAL = 1040
MAT_DIAMOND_PLATE = 1056
MAT_METAL = 1088
MAT_GRASS = 1280
MAT_LEAFY_GRASS = 1284
MAT_FABRIC = 1312
MAT_GROUND = 1360
MAT_ASPHALT = 1376
MAT_MUD = 1392
MAT_GLASS = 1568

SHAPE_BALL = 0
SHAPE_BLOCK = 1
SHAPE_CYLINDER = 2


def cf_pos(x: float, y: float, z: float) -> CFrameVal:
    return CFrameVal((x, y, z), (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0))


def cf_yaw(x: float, y: float, z: float, yaw_deg: float) -> CFrameVal:
    r = math.radians(yaw_deg)
    c, s = math.cos(r), math.sin(r)
    return CFrameVal((x, y, z), (c, 0.0, s, 0.0, 1.0, 0.0, -s, 0.0, c))


def cf_euler(x: float, y: float, z: float, rx_deg: float, ry_deg: float, rz_deg: float) -> CFrameVal:
    rx, ry, rz = math.radians(rx_deg), math.radians(ry_deg), math.radians(rz_deg)
    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)
    r00 = cy * cz + sy * sx * sz
    r01 = -cy * sz + sy * sx * cz
    r02 = sy * cx
    r10 = cx * sz
    r11 = cx * cz
    r12 = -sx
    r20 = -sy * cz + cy * sx * sz
    r21 = sy * sz + cy * sx * cz
    r22 = cy * cx
    return CFrameVal((x, y, z), (r00, r01, r02, r10, r11, r12, r20, r21, r22))


def make_part(
    name: str,
    size: Tuple[float, float, float],
    cframe: CFrameVal,
    color: Tuple[int, int, int] = (110, 112, 115),
    material: int = MAT_CONCRETE,
    anchored: bool = True,
    can_collide: bool = True,
    transparency: float = 0.0,
    shape: int = SHAPE_BLOCK,
    cast_shadow: bool = True,
    pbr_variant: str = "",
    attrs: Optional[Dict[str, Any]] = None,
) -> RbxInstance:
    p = RbxInstance("Part", name)
    p.set_prop("Size", Vector3Val(*size))
    p.set_prop("CFrame", cframe)
    p.set_prop("Color", Color3Val.from_rgb(*color))
    p.set_prop("Material", material)
    p.set_prop("Anchored", anchored)
    p.set_prop("CanCollide", can_collide)
    p.set_prop("Transparency", transparency)
    p.set_prop("Shape", shape)
    p.set_prop("CastShadow", cast_shadow)
    p.set_prop("TopSurface", 0)
    p.set_prop("BottomSurface", 0)
    if pbr_variant:
        p.set_attr("PBRVariant", pbr_variant)
    if attrs:
        for k, v in attrs.items():
            p.set_attr(k, v)
    return p


def make_truss_ladder(name: str, x: float, bottom_y: float, z: float, height: float) -> RbxInstance:
    t = RbxInstance("TrussPart", name)
    t.set_prop("Size", Vector3Val(2.0, height, 2.0))
    t.set_prop("CFrame", cf_pos(x, bottom_y + height * 0.5, z))
    t.set_prop("Color", Color3Val.from_rgb(84, 78, 72))
    t.set_prop("Material", MAT_CORRODED_METAL)
    t.set_prop("Anchored", True)
    t.set_prop("CanCollide", True)
    return t


def add_point_light(
    parent: RbxInstance,
    color: Tuple[int, int, int],
    brightness: float = 1.8,
    range_studs: float = 24.0,
    shadows: bool = True,
    name: str = "Light",
) -> RbxInstance:
    pl = RbxInstance("PointLight", name)
    pl.set_prop("Color", Color3Val.from_rgb(*color))
    pl.set_prop("Brightness", brightness)
    pl.set_prop("Range", range_studs)
    pl.set_prop("Shadows", shadows)
    parent.add_child(pl)
    return pl


def add_spot_light(
    parent: RbxInstance,
    color: Tuple[int, int, int],
    brightness: float = 2.8,
    range_studs: float = 48.0,
    angle: float = 65.0,
    face: int = 5,
    shadows: bool = True,
    name: str = "SpotLight",
) -> RbxInstance:
    sl = RbxInstance("SpotLight", name)
    sl.set_prop("Color", Color3Val.from_rgb(*color))
    sl.set_prop("Brightness", brightness)
    sl.set_prop("Range", range_studs)
    sl.set_prop("Angle", angle)
    sl.set_prop("Face", face)
    sl.set_prop("Shadows", shadows)
    parent.add_child(sl)
    return sl


def read_luau(rel_path: str) -> str:
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8")


def make_script(cls: str, name: str, source: str) -> RbxInstance:
    s = RbxInstance(cls, name)
    s.set_prop("Source", source)
    return s


# ==============================================================================
# SERVERSTORAGE TEMPLATES: 10 MUTANTS, 11 WEAPONS, 12 BUILDINGS
# ==============================================================================

MUTANT_SPECS = [
    ("Hollow", (92, 86, 80), MAT_SLATE, 1.04, False, "Head", None),
    ("Crawler", (76, 68, 64), MAT_SLATE, 0.92, True, "Head", None),
    ("Watcher", (62, 64, 70), MAT_SLATE, 1.42, False, "Head", (215, 195, 130)),
    ("Screamer", (112, 92, 88), MAT_SLATE, 1.02, False, "Head", (210, 92, 72)),
    ("Brute", (86, 82, 78), MAT_CONCRETE, 1.52, False, "ChestCore", (215, 118, 58)),
    ("Mimic", (88, 92, 84), MAT_FABRIC, 1.08, False, "Head", (140, 195, 210)),
    ("Burrower", (94, 76, 58), MAT_GROUND, 0.98, True, "Head", None),
    ("Stalker", (48, 50, 54), MAT_SLATE, 1.14, False, "Head", (195, 70, 65)),
    ("Contaminated", (88, 122, 76), MAT_SLATE, 1.12, False, "SporeSac", (155, 225, 95)),
    ("PaleOne", (216, 212, 206), MAT_SMOOTH_PLASTIC, 1.46, False, "Head", (220, 62, 58)),
]


def build_mutant_template(spec: Tuple) -> RbxInstance:
    sid, col, mat, scale, is_quad, weak_name, glow_col = spec
    m = RbxInstance("Model", sid)
    m.set_attr("SpeciesId", sid)

    root_size = (2.4 * scale, 1.8 * scale, 3.2 * scale) if is_quad else (2.0 * scale, 4.2 * scale, 1.4 * scale)
    root = make_part("HumanoidRootPart", root_size, cf_pos(0, 3.0, 0), col, mat, anchored=True, can_collide=True, transparency=1.0)
    m.add_child(root)
    m.set_prop("PrimaryPart", root)

    torso_sz = (1.9 * scale, 1.1 * scale, 2.7 * scale) if is_quad else (1.75 * scale, 2.15 * scale, 1.05 * scale)
    torso = make_part("Torso", torso_sz, cf_pos(0, 3.0, 0), col, mat, anchored=False, can_collide=False, pbr_variant="stone")
    m.add_child(torso)

    wt = RbxInstance("Weld", "TorsoWeld")
    wt.set_prop("Part0", root)
    wt.set_prop("Part1", torso)
    wt.set_prop("C0", cf_pos(0, 0, 0))
    wt.set_prop("C1", cf_pos(0, 0, 0))
    m.add_child(wt)

    head_y = 0.45 * scale if is_quad else 1.65 * scale
    head_z = -1.45 * scale if is_quad else -0.18 * scale
    head = make_part("Head", (1.1 * scale, 1.2 * scale, 1.15 * scale), cf_pos(0, 3.0 + head_y, head_z), col, mat, anchored=False, can_collide=False, shape=SHAPE_BALL)
    m.add_child(head)

    wh = RbxInstance("Weld", "NeckJoint")
    wh.set_prop("Part0", torso)
    wh.set_prop("Part1", head)
    wh.set_prop("C0", cf_pos(0, head_y, head_z))
    wh.set_prop("C1", cf_pos(0, 0, 0))
    m.add_child(wh)

    if weak_name != "Head" and glow_col:
        wp_z = -0.58 * scale if weak_name == "ChestCore" else 0.62 * scale
        wp = make_part(weak_name, (0.85 * scale, 0.95 * scale, 0.55 * scale), cf_pos(0, 3.2, wp_z), glow_col, MAT_NEON, anchored=False, can_collide=False, shape=SHAPE_BALL)
        add_point_light(wp, glow_col, brightness=1.4, range_studs=11.0, shadows=False)
        m.add_child(wp)
        ww = RbxInstance("Weld", weak_name + "Weld")
        ww.set_prop("Part0", torso)
        ww.set_prop("Part1", wp)
        ww.set_prop("C0", cf_pos(0, 0.2 * scale, wp_z))
        ww.set_prop("C1", cf_pos(0, 0, 0))
        m.add_child(ww)
    elif glow_col:
        add_point_light(head, glow_col, brightness=0.9, range_studs=8.0, shadows=False, name="EyeGlow")

    limb_defs = [
        ("LeftArm", (-1.15 * scale, 0.45 * scale, -0.75 * scale if is_quad else 0.0), (0.52 * scale, 1.95 * scale, 0.52 * scale)),
        ("RightArm", (1.15 * scale, 0.45 * scale, -0.75 * scale if is_quad else 0.0), (0.52 * scale, 1.95 * scale, 0.52 * scale)),
        ("LeftLeg", (-0.65 * scale, -1.35 * scale if not is_quad else -0.65 * scale, 0.95 * scale if is_quad else 0.0), (0.58 * scale, 1.95 * scale, 0.58 * scale)),
        ("RightLeg", (0.65 * scale, -1.35 * scale if not is_quad else -0.65 * scale, 0.95 * scale if is_quad else 0.0), (0.58 * scale, 1.95 * scale, 0.58 * scale)),
    ]
    for lname, offset, lsz in limb_defs:
        lp = make_part(lname, lsz, cf_pos(offset[0], 3.0 + offset[1], offset[2]), col, mat, anchored=False, can_collide=False)
        m.add_child(lp)
        lw = RbxInstance("Weld", lname + "Joint")
        lw.set_prop("Part0", torso)
        lw.set_prop("Part1", lp)
        lw.set_prop("C0", cf_pos(*offset))
        lw.set_prop("C1", cf_pos(0, 0, 0))
        m.add_child(lw)

    hum = RbxInstance("Humanoid", "Humanoid")
    hum.set_prop("DisplayDistanceType", 2)
    m.add_child(hum)
    return m


def build_weapon_templates() -> RbxInstance:
    folder = RbxInstance("Folder", "WeaponTemplates")
    weapons = [
        ("PipeClub", (0.22, 0.22, 3.1), (112, 102, 94), MAT_CORRODED_METAL),
        ("CombatKnife", (0.14, 0.26, 1.35), (82, 86, 90), MAT_METAL),
        ("FireAxe", (0.24, 0.85, 3.2), (146, 52, 46), MAT_METAL),
        ("SpikedBat", (0.28, 0.28, 3.0), (118, 90, 62), MAT_WOOD),
        ("ServicePistol", (0.22, 0.55, 0.92), (56, 60, 64), MAT_METAL),
        ("Revolver357", (0.24, 0.62, 1.15), (96, 100, 106), MAT_METAL),
        ("MakeshiftShotgun", (0.28, 0.54, 3.05), (98, 72, 52), MAT_WOOD),
        ("PumpShotgun", (0.26, 0.58, 3.35), (78, 64, 52), MAT_METAL),
        ("AssaultRifle", (0.28, 0.72, 3.15), (52, 56, 58), MAT_METAL),
        ("HuntingRifle", (0.26, 0.62, 3.75), (98, 72, 48), MAT_WOOD),
        ("TacticalCrossbow", (1.85, 0.52, 2.65), (76, 82, 70), MAT_METAL),
    ]
    for wid, sz, col, mat in weapons:
        m = RbxInstance("Model", wid)
        body = make_part("Receiver", sz, cf_pos(0, 0, 0), col, mat, anchored=True, can_collide=False)
        barrel = make_part("Barrel", (sz[0] * 0.45, sz[0] * 0.45, sz[2] * 0.55), cf_pos(0, sz[1] * 0.22, -sz[2] * 0.5), (48, 50, 54), MAT_METAL, anchored=True, can_collide=False)
        m.add_child(body)
        m.add_child(barrel)
        m.set_prop("PrimaryPart", body)
        folder.add_child(m)
    return folder


def build_building_templates() -> RbxInstance:
    folder = RbxInstance("Folder", "BuildingTemplates")
    bdefs = [
        ("WoodFloor", (8.0, 0.8, 8.0), (108, 84, 62), MAT_WOOD_PLANKS, "wood", None),
        ("WoodWall", (8.0, 9.0, 0.8), (104, 80, 58), MAT_WOOD_PLANKS, "wood", None),
        ("ReinforcedWall", (8.0, 9.0, 1.0), (88, 92, 96), MAT_CORRODED_METAL, "rust", None),
        ("WoodRoof", (8.4, 0.6, 8.4), (92, 72, 54), MAT_WOOD_PLANKS, "wood", None),
        ("SecurityDoor", (4.5, 8.2, 0.6), (82, 88, 92), MAT_METAL, "painted_metal", None),
        ("WindowBarricade", (5.2, 4.2, 0.5), (114, 88, 62), MAT_WOOD_PLANKS, "wood", None),
        ("StorageChest", (3.6, 2.4, 2.4), (78, 84, 72), MAT_METAL, "painted_metal", None),
        ("WorkbenchStation", (5.4, 3.4, 2.8), (94, 78, 62), MAT_WOOD_PLANKS, "wood", "Workbench"),
        ("CampfireStation", (3.6, 1.2, 3.6), (76, 68, 62), MAT_SLATE, "stone", "Campfire"),
        ("DieselGenerator", (3.8, 3.0, 2.6), (138, 102, 52), MAT_METAL, "industrial_machinery", None),
        ("PerimeterFloodlight", (1.8, 6.8, 1.8), (82, 86, 90), MAT_METAL, "painted_metal", None),
        ("SpikeBarricade", (6.5, 2.8, 2.8), (98, 74, 52), MAT_WOOD, "wood", None),
    ]
    for bid, sz, col, mat, mvar, stype in bdefs:
        m = RbxInstance("Model", bid)
        attrs: Dict[str, Any] = {}
        if stype:
            attrs["StationType"] = stype
        if bid == "SecurityDoor":
            attrs["InteractiveType"] = "Door"
            attrs["DoorOpen"] = False
        prim = make_part("Primary", sz, cf_pos(0, sz[1] * 0.5, 0), col, mat, anchored=True, can_collide=True, pbr_variant=mvar, attrs=attrs)
        prim.add_child(RbxInstance("SurfaceAppearance", "PBRSurface"))

        if bid == "CampfireStation":
            add_point_light(prim, (255, 148, 64), brightness=2.4, range_studs=24.0, shadows=True, name="FireLight")
            pe = RbxInstance("ParticleEmitter", "Embers")
            pe.set_prop("Color", ColorSequenceVal([(0.0, Color3Val.from_rgb(255, 165, 68)), (1.0, Color3Val.from_rgb(90, 80, 75))]))
            pe.set_prop("Size", NumberSequenceVal([(0.0, 0.35, 0.0), (1.0, 0.9, 0.0)]))
            pe.set_prop("Lifetime", NumberRangeVal(1.2, 2.4))
            pe.set_prop("Rate", 16.0)
            pe.set_prop("Speed", NumberRangeVal(2.5, 5.0))
            prim.add_child(pe)
        elif bid == "PerimeterFloodlight":
            sl = add_spot_light(prim, (255, 242, 212), brightness=3.6, range_studs=54.0, angle=72.0, face=5, shadows=True, name="FloodBeam")
            sl.set_prop("Enabled", False)

        m.add_child(prim)
        m.set_prop("PrimaryPart", prim)
        folder.add_child(m)
    return folder


# ==============================================================================
# WORLD BUILDER — SHELTER ECHO-07 + ALL 6 CONNECTED REGIONS
# ==============================================================================

class WorldBuilder:
    def __init__(self) -> None:
        self.world = RbxInstance("Folder", "World")
        self.interactables = RbxInstance("Folder", "InteractionObjects")
        self.enemy_spawns = RbxInstance("Folder", "EnemySpawns")
        self.loot_counter = 0
        self.spawn_counter = 0

    def add_loot_container(
        self,
        parent: RbxInstance,
        loot_table: str,
        pos: Tuple[float, float, float],
        size: Tuple[float, float, float] = (3.4, 2.4, 2.2),
        color: Tuple[int, int, int] = (78, 84, 76),
        mat: int = MAT_METAL,
        label: str = "Search Supply Container",
    ) -> RbxInstance:
        self.loot_counter += 1
        cid = f"Loot_{loot_table}_{self.loot_counter:03d}"
        part = make_part(
            cid,
            size,
            cf_pos(*pos),
            color=color,
            material=mat,
            attrs={
                "IsLootContainer": True,
                "ContainerId": cid,
                "LootTable": loot_table,
                "PromptLabel": label,
            },
        )
        parent.add_child(part)
        return part

    def add_lore_note(
        self,
        parent: RbxInstance,
        lore_id: str,
        title: str,
        pos: Tuple[float, float, float],
        is_terminal: bool = False,
    ) -> RbxInstance:
        sz = (2.2, 1.6, 1.2) if is_terminal else (1.3, 0.12, 1.7)
        col = (62, 74, 72) if is_terminal else (208, 198, 176)
        mat = MAT_METAL if is_terminal else MAT_FABRIC
        part = make_part(
            lore_id,
            sz,
            cf_pos(*pos),
            color=col,
            material=mat,
            attrs={
                "LoreId": lore_id,
                "PromptLabel": f"Read: {title}",
            },
        )
        if is_terminal:
            add_point_light(part, (118, 196, 152), brightness=1.1, range_studs=9.0, shadows=False)
        parent.add_child(part)
        return part

    def add_enemy_spawn(self, x: float, y: float, z: float, region: str, species: str = "") -> None:
        self.spawn_counter += 1
        sp = make_part(
            f"Spawn_{region}_{self.spawn_counter:02d}",
            (2.0, 0.5, 2.0),
            cf_pos(x, y, z),
            color=(120, 40, 40),
            material=MAT_SLATE,
            anchored=True,
            can_collide=False,
            transparency=1.0,
            cast_shadow=False,
            attrs={"Region": region, "Species": species},
        )
        self.enemy_spawns.add_child(sp)

    def add_hanging_physics_lamp(self, parent: RbxInstance, x: float, ceiling_y: float, z: float, light_col: Tuple[int, int, int] = (255, 196, 128)) -> None:
        mount = make_part("CeilingMount", (0.8, 0.4, 0.8), cf_pos(x, ceiling_y, z), (62, 64, 68), MAT_METAL, anchored=True, can_collide=False)
        a0 = RbxInstance("Attachment", "MountAtt")
        a0.set_prop("CFrame", cf_pos(0, -0.2, 0))
        mount.add_child(a0)

        fixture = make_part("SwingingLamp", (1.5, 0.7, 1.5), cf_pos(x, ceiling_y - 3.2, z), (92, 86, 74), MAT_METAL, anchored=False, can_collide=False)
        a1 = RbxInstance("Attachment", "LampAtt")
        a1.set_prop("CFrame", cf_pos(0, 0.4, 0))
        fixture.add_child(a1)
        add_point_light(fixture, light_col, brightness=1.9, range_studs=28.0, shadows=True)

        ball = RbxInstance("BallSocketConstraint", "PendulumJoint")
        ball.set_prop("Attachment0", a0)
        ball.set_prop("Attachment1", a1)
        ball.set_prop("LimitsEnabled", True)
        ball.set_prop("UpperAngle", 28.0)
        mount.add_child(ball)

        spring = RbxInstance("SpringConstraint", "SuspensionCable")
        spring.set_prop("Attachment0", a0)
        spring.set_prop("Attachment1", a1)
        spring.set_prop("FreeLength", 3.1)
        spring.set_prop("Stiffness", 420.0)
        spring.set_prop("Damping", 14.0)
        spring.set_prop("Visible", True)
        spring.set_prop("Thickness", 0.08)
        mount.add_child(spring)

        parent.add_child(mount)
        parent.add_child(fixture)

    def add_hinged_door(self, parent: RbxInstance, x: float, y: float, z: float, width: float = 4.4, height: float = 8.0, col: Tuple[int, int, int] = (92, 76, 60), mat: int = MAT_WOOD) -> None:
        jamb = make_part("DoorHingePost", (0.5, height, 0.7), cf_pos(x - width * 0.5 - 0.25, y + height * 0.5, z), (64, 66, 70), MAT_METAL, anchored=True, can_collide=True)
        a0 = RbxInstance("Attachment", "HingeAtt0")
        a0.set_prop("CFrame", cf_euler(0.25, 0, 0, 0, 0, 90))
        jamb.add_child(a0)

        leaf = make_part(
            "InteractiveDoorLeaf",
            (width, height - 0.2, 0.45),
            cf_pos(x, y + height * 0.5, z),
            col,
            mat,
            anchored=False,
            can_collide=True,
            attrs={"InteractiveType": "Door", "DoorOpen": False},
        )
        a1 = RbxInstance("Attachment", "HingeAtt1")
        a1.set_prop("CFrame", cf_euler(-width * 0.5, 0, 0, 0, 0, 90))
        leaf.add_child(a1)

        hinge = RbxInstance("HingeConstraint", "DoorServoHinge")
        hinge.set_prop("Attachment0", a0)
        hinge.set_prop("Attachment1", a1)
        hinge.set_prop("ActuatorType", 2)  # Servo
        hinge.set_prop("AngularSpeed", 2.4)
        hinge.set_prop("ServoMaxTorque", 180000.0)
        hinge.set_prop("TargetAngle", 0.0)
        hinge.set_prop("LimitsEnabled", True)
        hinge.set_prop("LowerAngle", -95.0)
        hinge.set_prop("UpperAngle", 95.0)
        leaf.add_child(hinge)

        parent.add_child(jamb)
        parent.add_child(leaf)

    def build_enterable_building(
        self,
        parent: RbxInstance,
        name: str,
        cx: float,
        y: float,
        cz: float,
        w: float,
        h: float,
        d: float,
        wall_col: Tuple[int, int, int],
        wall_mat: int,
        mvar: str,
        has_barricade: bool = True,
        has_second_floor: bool = False,
    ) -> RbxInstance:
        mdl = RbxInstance("Model", name)
        t = 1.0
        total_h = h * 2 if has_second_floor else h

        mdl.add_child(make_part("Floor", (w, 0.8, d), cf_pos(cx, y + 0.4, cz), (78, 76, 74), MAT_CONCRETE, pbr_variant="concrete"))
        mdl.add_child(make_part("Roof", (w + 1.2, 0.9, d + 1.2), cf_pos(cx, y + total_h + 0.45, cz), (58, 56, 54), MAT_SLATE, pbr_variant="stone"))

        win_w = 5.0
        side_seg = (w - win_w) * 0.5
        mdl.add_child(make_part("BackWallL", (side_seg, total_h, t), cf_pos(cx - (win_w + side_seg) * 0.5, y + total_h * 0.5, cz - d * 0.5), wall_col, wall_mat, pbr_variant=mvar))
        mdl.add_child(make_part("BackWallR", (side_seg, total_h, t), cf_pos(cx + (win_w + side_seg) * 0.5, y + total_h * 0.5, cz - d * 0.5), wall_col, wall_mat, pbr_variant=mvar))
        mdl.add_child(make_part("BackWallSill", (win_w, 3.2, t), cf_pos(cx, y + 1.6, cz - d * 0.5), wall_col, wall_mat, pbr_variant=mvar))
        mdl.add_child(make_part("BackWallHeader", (win_w, total_h - 7.4, t), cf_pos(cx, y + 7.4 + (total_h - 7.4) * 0.5, cz - d * 0.5), wall_col, wall_mat, pbr_variant=mvar))

        mdl.add_child(
            make_part(
                "WindowGlass",
                (win_w - 0.2, 4.2, 0.2),
                cf_pos(cx, y + 5.3, cz - d * 0.5),
                (165, 192, 204),
                MAT_GLASS,
                transparency=0.55,
                cast_shadow=False,
                attrs={"DestructibleType": "Glass", "StructureHP": 18},
            )
        )
        if has_barricade:
            mdl.add_child(
                make_part(
                    "WindowBarricadePlanks",
                    (win_w + 0.6, 3.6, 0.35),
                    cf_euler(cx, y + 5.2, cz - d * 0.5 + 0.45, 0, 0, 6),
                    (112, 86, 60),
                    MAT_WOOD_PLANKS,
                    pbr_variant="wood",
                    attrs={"DestructibleType": "WoodenBarricade", "StructureHP": 85},
                )
            )

        mdl.add_child(make_part("LeftWall", (t, total_h, d), cf_pos(cx - w * 0.5, y + total_h * 0.5, cz), wall_col, wall_mat, pbr_variant=mvar))
        mdl.add_child(make_part("RightWall", (t, total_h, d), cf_pos(cx + w * 0.5, y + total_h * 0.5, cz), wall_col, wall_mat, pbr_variant=mvar))

        door_w = 5.0
        f_seg = (w - door_w) * 0.5
        mdl.add_child(make_part("FrontWallL", (f_seg, total_h, t), cf_pos(cx - (door_w + f_seg) * 0.5, y + total_h * 0.5, cz + d * 0.5), wall_col, wall_mat, pbr_variant=mvar))
        mdl.add_child(make_part("FrontWallR", (f_seg, total_h, t), cf_pos(cx + (door_w + f_seg) * 0.5, y + total_h * 0.5, cz + d * 0.5), wall_col, wall_mat, pbr_variant=mvar))
        mdl.add_child(make_part("FrontDoorHeader", (door_w, total_h - 8.4, t), cf_pos(cx, y + 8.4 + (total_h - 8.4) * 0.5, cz + d * 0.5), wall_col, wall_mat, pbr_variant=mvar))
        self.add_hinged_door(mdl, cx, y + 0.8, cz + d * 0.5, width=4.4, height=7.6)

        if has_second_floor:
            mdl.add_child(make_part("SecondFloorDeck", (w - 2.0, 0.7, d * 0.55), cf_pos(cx, y + h, cz - d * 0.2), (96, 78, 60), MAT_WOOD_PLANKS))
            mdl.add_child(make_part("InteriorStairRamp", (4.2, 0.7, 13.5), cf_euler(cx - w * 0.28, y + h * 0.5, cz + d * 0.12, -26, 0, 0), (88, 72, 56), MAT_WOOD_PLANKS))

        parent.add_child(mdl)
        return mdl

    def add_abandoned_vehicle(self, parent: RbxInstance, x: float, y: float, z: float, yaw: float, color: Tuple[int, int, int], alarm_interactive: bool = False) -> None:
        car = RbxInstance("Model", "AbandonedSedan")
        attrs = {"InteractiveType": "VehicleAlarm"} if alarm_interactive else None
        chassis = make_part("Chassis", (7.2, 2.4, 14.2), cf_yaw(x, y + 1.8, z, yaw), color, MAT_CORRODED_METAL, pbr_variant="rust", attrs=attrs)
        cabin = make_part("Cabin", (6.4, 2.1, 7.2), cf_yaw(x, y + 3.9, z - 0.5, yaw), color, MAT_CORRODED_METAL)
        car.add_child(chassis)
        car.add_child(cabin)
        parent.add_child(car)

    def add_pine_tree(self, parent: RbxInstance, x: float, y: float, z: float, scale: float = 1.0) -> None:
        tree = RbxInstance("Model", "BlackwoodPine")
        th = 16.0 * scale
        trunk = make_part("Trunk", (2.2 * scale, th, 2.2 * scale), cf_pos(x, y + th * 0.5, z), (68, 52, 40), MAT_WOOD, shape=SHAPE_CYLINDER)
        c1 = make_part("CanopyLower", (11.5 * scale, 9.0 * scale, 11.5 * scale), cf_pos(x, y + th * 0.72, z), (38, 54, 40), MAT_LEAFY_GRASS, can_collide=False)
        c2 = make_part("CanopyUpper", (7.6 * scale, 8.5 * scale, 7.6 * scale), cf_pos(x, y + th * 1.10, z), (34, 48, 36), MAT_LEAFY_GRASS, can_collide=False)
        tree.add_child(trunk)
        tree.add_child(c1)
        tree.add_child(c2)
        parent.add_child(tree)

    def assemble_all_regions(self) -> Tuple[RbxInstance, RbxInstance, RbxInstance]:
        rng = random.Random(20261023)

        # 0. CONNECTED GROUND PLATES & HIGHWAY NETWORK (2,400 x 2,400 studs)
        # Leave clean stairwell opening at (0, 0, 588..612) for Shelter Echo-07 blast ramp!
        ground_folder = RbxInstance("Folder", "ConnectedGroundAndRoads")
        ground_slabs = [
            ("Ground_NorthMain", (2400, 4, 1788), (0, -2, -306), (68, 72, 62), MAT_GROUND, "mud"),
            ("Ground_SouthFar", (2400, 4, 588), (0, -2, 906), (74, 78, 66), MAT_GRASS, "mud"),
            ("Ground_HatchFlankW", (1194, 4, 24), (-603, -2, 600), (72, 76, 66), MAT_GRASS, "mud"),
            ("Ground_HatchFlankE", (1194, 4, 24), (603, -2, 600), (72, 76, 66), MAT_GRASS, "mud"),
        ]
        for gname, gsz, gpos, gcol, gmat, gmvar in ground_slabs:
            ground_folder.add_child(make_part(gname, gsz, cf_pos(*gpos), gcol, gmat, pbr_variant=gmvar))

        for cname, csz, cpos in [
            ("PerimeterRidge_N", (2440, 95, 60), (0, 44, -1200)),
            ("PerimeterRidge_S", (2440, 95, 60), (0, 44, 1200)),
            ("PerimeterRidge_W", (60, 95, 2400), (-1200, 44, 0)),
            ("PerimeterRidge_E", (60, 95, 2400), (1200, 44, 0)),
        ]:
            ground_folder.add_child(make_part(cname, csz, cf_pos(*cpos), (58, 56, 54), MAT_SLATE))

        # Two-Lane Cracked Asphalt Highway Network linking all 6 regions
        for rname, rsz, rpos in [
            ("Highway_Route9_NS", (26, 0.35, 1960), (28, 0.18, -80)),
            ("Highway_Cross_EW", (1880, 0.35, 24), (0, 0.18, 175)),
            ("Highway_NorthBelt_EW", (1880, 0.35, 24), (0, 0.18, -680)),
        ]:
            ground_folder.add_child(make_part(rname, rsz, cf_pos(*rpos), (44, 46, 48), MAT_ASPHALT, pbr_variant="asphalt"))

        for z_lamp in range(-760, 740, 150):
            pole = make_part(f"HighwayLampPole_{z_lamp}", (0.8, 22.0, 0.8), cf_pos(43.0, 11.0, float(z_lamp)), (74, 76, 78), MAT_CORRODED_METAL)
            arm = make_part(f"HighwayLampArm_{z_lamp}", (6.5, 0.5, 0.6), cf_pos(40.2, 21.8, float(z_lamp)), (74, 76, 78), MAT_CORRODED_METAL)
            if abs(z_lamp) % 300 == 0:
                add_spot_light(arm, (255, 206, 142), brightness=2.1, range_studs=42.0, angle=68.0, face=4, shadows=False)
            ground_folder.add_child(pole)
            ground_folder.add_child(arm)

        ground_folder.add_child(make_part("CrossroadsSignPost", (1.0, 13.0, 1.0), cf_pos(44, 6.5, 195), (76, 78, 80), MAT_METAL))
        ground_folder.add_child(make_part("SignBoard_Highway", (11.0, 4.2, 0.4), cf_yaw(44, 11.2, 195, -25), (46, 74, 56), MAT_METAL))
        self.world.add_child(ground_folder)

        # ======================================================================
        # OPENING AREA — UNDERGROUND EMERGENCY SHELTER ECHO-07 (0, -19.5, 620)
        # ======================================================================
        shelter = RbxInstance("Model", "EmergencyShelter_Echo07")
        shelter.add_child(make_part("VaultFloor", (44, 1.5, 58), cf_pos(0, -21.25, 620), (62, 66, 70), MAT_DIAMOND_PLATE, pbr_variant="industrial_machinery"))
        shelter.add_child(make_part("VaultCeiling", (44, 2.0, 42), cf_pos(0, -6.0, 628), (56, 58, 62), MAT_CONCRETE, pbr_variant="concrete"))
        shelter.add_child(make_part("VaultWall_S", (44, 16, 2), cf_pos(0, -13.0, 649), (64, 66, 68), MAT_CONCRETE))
        shelter.add_child(make_part("VaultWall_W", (2, 16, 58), cf_pos(-22, -13.0, 620), (64, 66, 68), MAT_CONCRETE))
        shelter.add_child(make_part("VaultWall_E", (2, 16, 58), cf_pos(22, -13.0, 620), (64, 66, 68), MAT_CONCRETE))
        shelter.add_child(make_part("VaultWall_N", (44, 16, 2), cf_pos(0, -13.0, 591), (64, 66, 68), MAT_CONCRETE))

        # Concrete Stairwell Ramp rising from (0, -20.5, 614) to surface airlock at (0, 0, 588)
        shelter.add_child(make_part("ExitRampToSurface", (10.0, 1.2, 31.0), cf_euler(0, -10.2, 601.0, 38, 0, 0), (76, 78, 80), MAT_CONCRETE))
        shelter.add_child(make_part("HeadhouseWall_W", (1.2, 9.5, 24), cf_pos(-6.0, 4.75, 600), (72, 74, 76), MAT_CONCRETE))
        shelter.add_child(make_part("HeadhouseWall_E", (1.2, 9.5, 24), cf_pos(6.0, 4.75, 600), (72, 74, 76), MAT_CONCRETE))
        shelter.add_child(make_part("HeadhouseRoof", (14.0, 1.0, 25), cf_pos(0.0, 9.8, 600), (64, 66, 68), MAT_CONCRETE))
        self.add_hinged_door(shelter, 0.0, 0.0, 588.0, width=5.2, height=8.4, col=(78, 86, 82), mat=MAT_METAL)

        spawn_loc = RbxInstance("SpawnLocation", "Echo07AwakeningSpawn")
        spawn_loc.set_prop("Size", Vector3Val(6.0, 0.6, 6.0))
        spawn_loc.set_prop("CFrame", cf_pos(0, -20.2, 620))
        spawn_loc.set_prop("Anchored", True)
        spawn_loc.set_prop("Transparency", 1.0)
        spawn_loc.set_prop("Neutral", True)
        shelter.add_child(spawn_loc)

        shelter.add_child(make_part("EmergencyRestBunk", (4.2, 2.0, 7.6), cf_pos(-15.5, -19.5, 632.0), (76, 84, 72), MAT_FABRIC, pbr_variant="fabric", attrs={"InteractiveType": "RestBunk"}))
        shelter.add_child(make_part("CommandDesk", (6.5, 3.0, 3.2), cf_pos(6.0, -19.0, 626.0), (74, 78, 82), MAT_METAL))
        self.add_lore_note(shelter, "Lore_01_EchoTerminal", "Echo-07 Terminal Log #044", (6.0, -17.3, 626.0), is_terminal=True)
        self.add_lore_note(shelter, "Lore_02_BunkerWallNote", "Scrawled Hatch Warning", (4.2, 3.5, 590.0))

        shelter.add_child(make_part("EchoWorkbench", (6.0, 3.4, 2.8), cf_pos(-15.5, -18.8, 618.0), (96, 78, 58), MAT_WOOD_PLANKS, attrs={"StationType": "Workbench"}))
        self.add_loot_container(shelter, "Suburbs", (-16.0, -19.1, 642.0), label="Search Emergency Reserve Locker")
        self.add_loot_container(shelter, "Suburbs", (16.0, -19.1, 642.0), label="Search Ration Footlocker")
        self.add_hanging_physics_lamp(shelter, 0.0, -7.2, 626.0, (255, 178, 96))
        self.world.add_child(shelter)

        # ======================================================================
        # REGION 1 — THE DEAD SUBURBS (Center: 0, 0, 540)
        # ======================================================================
        suburbs = RbxInstance("Model", "Region1_TheDeadSuburbs")
        for idx, (hx, hz, hcol, two_st) in enumerate([
            (-88, 510, (148, 142, 132), False),
            (72, 510, (132, 140, 146), True),
            (-88, 430, (142, 128, 118), True),
            (72, 430, (136, 138, 128), False),
            (-88, 350, (126, 132, 138), True),
            (72, 350, (144, 136, 124), False),
        ]):
            h_mdl = self.build_enterable_building(suburbs, f"SuburbanHouse_{idx + 1}", hx, 0.0, hz, 24.0, 11.0, 20.0, hcol, MAT_WOOD_PLANKS, "damaged_wall", has_barricade=(idx % 2 == 0), has_second_floor=two_st)
            self.add_loot_container(h_mdl, "Suburbs", (hx - 6.5, 2.0, hz - 5.5), label="Search Kitchen Pantry")
            self.add_loot_container(h_mdl, "Suburbs", (hx + 6.5, 2.0, hz + 3.0), label="Search Garage Cabinet")

        grocery = self.build_enterable_building(suburbs, "Mart24CornerGrocery", -155, 0.0, 465, 34.0, 13.0, 26.0, (128, 114, 102), MAT_BRICK, "brick", has_barricade=True)
        self.add_loot_container(grocery, "Suburbs", (-162, 2.0, 460), label="Search Grocery Shelving")
        self.add_loot_container(grocery, "Hospital", (-146, 2.0, 460), label="Search Pharmacy Counter")

        suburbs.add_child(make_part("GasCanopyRoof", (36.0, 1.2, 22.0), cf_pos(145, 14.5, 515), (124, 62, 54), MAT_METAL, pbr_variant="rust"))
        for px in (132, 158):
            suburbs.add_child(make_part("CanopyPillar", (1.6, 14.5, 1.6), cf_pos(px, 7.25, 515), (96, 98, 102), MAT_METAL))
            self.add_loot_container(suburbs, "Industrial", (px, 2.0, 518), (2.4, 3.8, 1.8), (138, 68, 56), MAT_METAL, "Search Service Pump")

        # Maple Ridge Civil Defense Radio Relay Mast at (135, 0, 420)
        suburbs.add_child(make_part("RelayTowerLattice", (5.5, 58.0, 5.5), cf_pos(135, 29.0, 420), (112, 86, 76), MAT_CORRODED_METAL, pbr_variant="rust"))
        suburbs.add_child(make_truss_ladder("RelayClimbLadder", 131.5, 0.0, 420.0, 32.0))
        beacon = make_part("RelayBeacon", (1.4, 1.4, 1.4), cf_pos(135, 59.0, 420), (215, 72, 58), MAT_NEON, shape=SHAPE_BALL)
        add_point_light(beacon, (225, 76, 58), brightness=2.2, range_studs=38.0, shadows=False)
        suburbs.add_child(beacon)

        relay_console = make_part("RelayControlBox", (3.4, 4.2, 2.4), cf_pos(135, 2.1, 424.5), (72, 84, 78), MAT_METAL, attrs={"QuestInteractId": "Relay_Suburbs", "PromptLabel": "Power Up Civil Defense Relay Mast"})
        add_point_light(relay_console, (196, 152, 76), brightness=1.4, range_studs=12.0, shadows=False)
        self.interactables.add_child(relay_console)

        self.add_abandoned_vehicle(suburbs, 18, 0.0, 495, 14.0, (92, 68, 62), alarm_interactive=True)
        self.add_abandoned_vehicle(suburbs, 36, 0.0, 395, -22.0, (68, 78, 86))
        self.add_lore_note(suburbs, "Lore_03_EvacNotice", "Civil Defense Evacuation Order #14", (-88.0, 2.2, 510.0))
        self.add_lore_note(suburbs, "Lore_04_SuburbDiary", "Water-Stained Kitchen Journal", (72.0, 2.2, 510.0))
        self.add_lore_note(suburbs, "Lore_05_ShopkeeperLedger", "Mart-24 Backroom Note", (-155.0, 2.2, 465.0))

        for sx, sz, sp in [(-115, 535, "Hollow"), (105, 480, "Crawler"), (-95, 390, "Hollow"), (45, 310, "Screamer"), (-165, 420, "Hollow"), (165, 455, "Crawler")]:
            self.add_enemy_spawn(sx, 3.5, sz, "DeadSuburbs", sp)
        self.world.add_child(suburbs)

        # ======================================================================
        # REGION 2 — BLACKWOOD FOREST (Center: -780, 0, 160)
        # ======================================================================
        forest = RbxInstance("Model", "Region2_BlackwoodForest")
        for _ in range(96):
            tx = -780 + rng.uniform(-280, 260)
            tz = 160 + rng.uniform(-260, 260)
            if math.hypot(tx + 740, tz - 190) > 28 and math.hypot(tx + 690, tz - 120) > 25 and math.hypot(tx + 860, tz - 60) > 25:
                self.add_pine_tree(forest, tx, 0.0, tz, rng.uniform(0.85, 1.45))

        # Ranger Watchtower at (-740, 0, 190) — Act IV Waypoint!
        for lx in (-748, -732):
            for lz in (182, 198):
                forest.add_child(make_part("WatchtowerLeg", (1.6, 34.0, 1.6), cf_pos(lx, 17.0, lz), (88, 68, 50), MAT_WOOD))
        forest.add_child(make_part("WatchtowerDeck", (22.0, 1.0, 22.0), cf_pos(-740, 34.5, 190), (96, 76, 56), MAT_WOOD_PLANKS))
        forest.add_child(make_part("WatchtowerRoof", (23.0, 1.0, 23.0), cf_pos(-740, 44.0, 190), (62, 54, 48), MAT_WOOD_PLANKS))
        forest.add_child(make_truss_ladder("WatchtowerLadder", -740.0, 0.0, 179.5, 35.5))
        self.add_loot_container(forest, "Forest", (-740, 36.2, 190), label="Search Ranger Rifle Locker")
        self.add_lore_note(forest, "Lore_06_RangerRecording", "Ranger Station Reel #09", (-740, 2.2, 190), is_terminal=True)

        cabin1 = self.build_enterable_building(forest, "HunterCabin_Alpha", -690, 0.0, 120, 20.0, 10.5, 18.0, (92, 70, 52), MAT_WOOD, "wood", has_barricade=True)
        cabin1.add_child(make_part("StoneCampfire", (3.8, 1.2, 3.8), cf_pos(-690, 1.2, 136), (78, 72, 68), MAT_SLATE, attrs={"StationType": "Campfire"}))
        self.add_loot_container(cabin1, "Forest", (-694, 2.0, 116), label="Search Trapper Footlocker")
        self.add_lore_note(cabin1, "Lore_08_CamperLetter", "Unsent Hunter's Letter", (-686, 2.0, 118))

        bunker4 = self.build_enterable_building(forest, "DrainageListeningBunker", -860, 0.0, 60, 22.0, 10.0, 22.0, (68, 72, 74), MAT_CONCRETE, "concrete", has_barricade=False)
        self.add_loot_container(bunker4, "Military", (-864, 2.0, 56), label="Search SIGINT Equipment Crate")
        self.add_loot_container(bunker4, "Forest", (-856, 2.0, 56), label="Search Field Survival Cache")
        self.add_lore_note(bunker4, "Lore_07_TunnelScratchings", "Drainage Tunnel Recorder", (-860, 2.2, 60), is_terminal=True)

        for sx, sz, sp in [(-810, 210, "Watcher"), (-720, 260, "Mimic"), (-850, 140, "Stalker"), (-660, 190, "Burrower"), (-780, 90, "Watcher"), (-890, 60, "Hollow"), (-750, 40, "PaleOne")]:
            self.add_enemy_spawn(sx, 3.5, sz, "BlackwoodForest", sp)
        self.world.add_child(forest)

        # ======================================================================
        # REGION 3 — THE INDUSTRIAL ZONE (Center: 780, 0, 180)
        # ======================================================================
        industrial = RbxInstance("Model", "Region3_TheIndustrialZone")
        warehouse = self.build_enterable_building(industrial, "ChemPlantMainWarehouse", 790, 0.0, 165, 54.0, 16.0, 40.0, (88, 82, 78), MAT_CORRODED_METAL, "rust", has_barricade=True, has_second_floor=True)
        warehouse.add_child(make_part("IndustrialWorkbench", (6.5, 3.5, 3.0), cf_pos(772, 2.4, 155), (92, 78, 60), MAT_WOOD_PLANKS, attrs={"StationType": "Workbench"}))
        self.add_hanging_physics_lamp(warehouse, 790.0, 31.0, 165.0, (255, 204, 136))
        for lx in (774, 790, 806):
            self.add_loot_container(warehouse, "Industrial", (lx, 2.2, 152), label="Search Heavy Tool Locker")

        for sy_x in (845, 870):
            industrial.add_child(make_part("Smokestack", (9.5, 78.0, 9.5), cf_pos(sy_x, 39.0, 130), (96, 78, 70), MAT_BRICK, shape=SHAPE_CYLINDER))
        for silo_z in (215, 240):
            industrial.add_child(make_part("ChemicalSilo", (14.0, 28.0, 14.0), cf_pos(735, 14.0, silo_z), (104, 112, 114), MAT_METAL, shape=SHAPE_CYLINDER, pbr_variant="painted_metal"))

        substation_gen = make_part("SubstationAuxiliaryGenerator", (6.8, 5.2, 4.2), cf_pos(690, 2.6, 230), (142, 106, 52), MAT_METAL, pbr_variant="industrial_machinery", attrs={"QuestInteractId": "Generator_Industrial", "PromptLabel": "Engage Auxiliary Substation Generator"})
        add_point_light(substation_gen, (255, 172, 74), brightness=1.8, range_studs=20.0)
        self.interactables.add_child(substation_gen)
        self.add_loot_container(industrial, "Industrial", (682, 2.0, 224), label="Search Lineman Supply Box")

        for bz in (110, 148, 186):
            industrial.add_child(make_part("FreightBoxcar", (11.0, 11.5, 30.0), cf_pos(890, 6.2, bz), (108, 62, 54), MAT_CORRODED_METAL, pbr_variant="rust"))
        self.add_loot_container(industrial, "Industrial", (882, 2.0, 148), label="Search Hazmat Cargo Crate")

        self.add_lore_note(industrial, "Lore_12_SubLabManifest", "Chem-Plant Sub-Lab Manifest", (790, 2.4, 165), is_terminal=True)
        self.add_lore_note(industrial, "Lore_13_ForemanMemo", "Cooling Tower Maintenance Memo", (845, 2.2, 142))

        for sx, sz, sp in [(745, 110, "Brute"), (825, 235, "Contaminated"), (895, 165, "Hollow"), (780, 270, "Screamer"), (695, 190, "Crawler"), (720, 195, "Contaminated")]:
            self.add_enemy_spawn(sx, 3.5, sz, "IndustrialZone", sp)
        self.world.add_child(industrial)

        # ======================================================================
        # REGION 4 — THE FORGOTTEN CITY (Center: 0, 0, -340)
        # ======================================================================
        city = RbxInstance("Model", "Region4_TheForgottenCity")
        for bname, bx, bz, bw, bh, bd, bcol, two_fl in [
            ("RuinedOfficeTower_A", -55, -280, 32.0, 15.0, 28.0, (92, 94, 98), True),
            ("ApartmentBlock_B", 85, -280, 30.0, 14.0, 26.0, (104, 96, 90), True),
            ("PrecinctStation_9", -55, -420, 34.0, 13.0, 26.0, (82, 88, 96), False),
            ("CollapsedCommercial_D", 85, -420, 32.0, 15.0, 28.0, (98, 94, 88), True),
        ]:
            blk = self.build_enterable_building(city, bname, bx, 0.0, bz, bw, bh, bd, bcol, MAT_CONCRETE, "concrete", has_barricade=True, has_second_floor=two_fl)
            self.add_loot_container(blk, "City", (bx - 6, 2.0, bz - 5), label="Search Urban Scavenge Locker")
            if two_fl:
                self.add_loot_container(blk, "City", (bx + 5, bh + 2.0, bz - 5), label="Search Upper Floor Safe")

        # St. Jude Mercy Triage Hospital Annex at (-145, 0, -365) — Act III Main Quest Target!
        hospital = self.build_enterable_building(city, "StJudeMercyHospitalAnnex", -145, 0.0, -365, 48.0, 14.0, 34.0, (142, 146, 144), MAT_CONCRETE, "marble", has_barricade=True, has_second_floor=True)
        for hx in (-158, -145, -132):
            self.add_loot_container(hospital, "Hospital", (hx, 2.2, -374), (3.0, 2.6, 2.0), (164, 58, 54), MAT_METAL, "Search Quarantine Medical Cabinet")
        self.add_lore_note(hospital, "Lore_09_VanceTriageTape", "Dr. Elena Vance — Quarantine Ward Log", (-145, 2.4, -360), is_terminal=True)
        self.add_lore_note(city, "Lore_10_PrecinctDispatch", "Precinct 9 Desk Blotter", (-55, 2.2, -420))
        self.add_lore_note(city, "Lore_11_RooftopSniperNote", "Apartment Rooftop Spotter Log", (85, 16.0, -285))

        city.add_child(make_part("CollapsedOverpassSpan", (76.0, 2.2, 22.0), cf_euler(18, 14.0, -225, 0, 0, -11), (84, 86, 88), MAT_CONCRETE))
        metro = self.build_enterable_building(city, "MetroEntranceKiosk", 15, 0.0, -350, 18.0, 10.5, 18.0, (74, 78, 82), MAT_CONCRETE, "concrete", has_barricade=False)
        metro.add_child(make_part("RottedServiceDoor", (4.4, 7.6, 0.45), cf_pos(15, 4.2, -358.5), (92, 68, 52), MAT_WOOD_PLANKS, attrs={"DestructibleType": "WeakDoor", "StructureHP": 110}))
        self.add_hanging_physics_lamp(metro, 15.0, 9.6, -350.0, (195, 220, 235))
        self.add_loot_container(metro, "City", (11, 2.0, -353), label="Search Transit Maintenance Locker")
        self.add_abandoned_vehicle(city, 22, 0.0, -310, 35.0, (74, 78, 82), alarm_interactive=True)

        for sx, sz, sp in [(0, -290, "Screamer"), (65, -350, "Brute"), (-95, -340, "Stalker"), (-145, -320, "Hollow"), (115, -355, "Crawler"), (-25, -465, "PaleOne"), (85, -465, "Mimic")]:
            self.add_enemy_spawn(sx, 3.5, sz, "ForgottenCity", sp)
        self.world.add_child(city)

        # ======================================================================
        # REGION 5 — THE MILITARY EXCLUSION ZONE (Center: -780, 0, -720)
        # ======================================================================
        military = RbxInstance("Model", "Region5_MilitaryExclusionZone")
        military.add_child(make_part("BlastWall_N", (240, 14.0, 4.0), cf_pos(-780, 7.0, -840), (76, 78, 74), MAT_CONCRETE))
        military.add_child(make_part("BlastWall_S", (240, 14.0, 4.0), cf_pos(-780, 7.0, -600), (76, 78, 74), MAT_CONCRETE))
        military.add_child(make_part("BlastWall_W", (4.0, 14.0, 240), cf_pos(-900, 7.0, -720), (76, 78, 74), MAT_CONCRETE))

        for gx, gz in [(-665, -695), (-665, -745)]:
            military.add_child(make_part("GuardTowerBase", (10.0, 22.0, 10.0), cf_pos(gx, 11.0, gz), (72, 78, 68), MAT_CONCRETE))
            cab = make_part("GuardSearchlightPod", (12.0, 7.0, 12.0), cf_pos(gx, 25.5, gz), (64, 72, 60), MAT_METAL, pbr_variant="painted_metal")
            add_spot_light(cab, (255, 244, 210), brightness=3.2, range_studs=60.0, angle=60.0, face=5)
            military.add_child(cab)

        armory = self.build_enterable_building(military, "ExclusionArmoryBarracks", -745, 0.0, -665, 36.0, 12.5, 24.0, (74, 82, 68), MAT_METAL, "painted_metal", has_barricade=False)
        armory.add_child(make_part("ArmoryWorkbench", (6.0, 3.4, 2.8), cf_pos(-755, 2.2, -671), (82, 86, 80), MAT_METAL, attrs={"StationType": "Workbench"}))
        self.add_loot_container(armory, "Military", (-745, 2.2, -671), label="Search Military Weapons Rack")
        self.add_loot_container(armory, "Military", (-735, 2.2, -671), label="Search CBRN Gear Locker")

        lazarus_lab = self.build_enterable_building(military, "LazarusBioContainmentBunker", -810, 0.0, -745, 42.0, 14.0, 32.0, (66, 70, 74), MAT_CONCRETE, "industrial_machinery", has_barricade=False)
        lazarus_lab.add_child(make_part("LazarusWorkbench", (6.2, 3.4, 2.8), cf_pos(-822, 2.2, -752), (96, 108, 112), MAT_METAL, attrs={"StationType": "Workbench"}))
        for lx in (-818, -806, -794):
            self.add_loot_container(lazarus_lab, "Military", (lx, 2.2, -754), label="Search LAZARUS Vault Case")

        self.add_lore_note(military, "Lore_14_CheckpointOrders", "AEGIS Perimeter Rules of Engagement", (-668, 2.0, -720))
        self.add_lore_note(lazarus_lab, "Lore_15_LazarusDirective", "PROJECT LAZARUS — Executive Summary", (-810, 2.4, -745), is_terminal=True)
        self.add_lore_note(lazarus_lab, "Lore_16_LabAutopsy", "Site-09 Specimen Autopsy #12", (-798, 2.4, -745), is_terminal=True)

        for sx, sz, sp in [(-710, -720, "Brute"), (-785, -715, "Contaminated"), (-835, -705, "Screamer"), (-760, -795, "Stalker"), (-695, -775, "Hollow"), (-850, -805, "Brute")]:
            self.add_enemy_spawn(sx, 3.5, sz, "MilitaryExclusionZone", sp)
        self.world.add_child(military)

        # ======================================================================
        # REGION 6 — THE RED WASTELAND & BROADCAST TOWER ZERO (Center: 780, 0, -700)
        # ======================================================================
        wasteland = RbxInstance("Model", "Region6_TheRedWasteland")
        wasteland.add_child(make_part("ScorchedAshBasin", (620, 0.6, 620), cf_pos(790, 0.25, -710), (118, 66, 52), MAT_GROUND, pbr_variant="mud"))

        for i in range(18):
            ang = (i / 18.0) * math.pi * 2
            rad = 115 + (i % 4) * 45
            vx = 790 + math.cos(ang) * rad
            vz = -710 + math.sin(ang) * rad
            spire_h = 18.0 + (i % 5) * 7.0
            wasteland.add_child(make_part(f"VitrifiedSpire_{i}", (6.5, spire_h, 6.5), cf_euler(vx, spire_h * 0.45, vz, rng.uniform(-14, 14), rng.uniform(0, 180), rng.uniform(-14, 14)), (58, 38, 34), MAT_BASALT))
            if i % 3 == 0:
                vent = make_part(f"RadiationFissure_{i}", (4.5, 0.8, 4.5), cf_pos(vx + 6, 0.6, vz), (218, 92, 48), MAT_CRACKED_LAVA)
                add_point_light(vent, (235, 98, 46), brightness=2.0, range_studs=26.0, shadows=False)
                wasteland.add_child(vent)

        outpost = self.build_enterable_building(wasteland, "WastelandSurveyOutpost", 755, 0.0, -665, 26.0, 11.5, 22.0, (92, 68, 62), MAT_CORRODED_METAL, "rust", has_barricade=True)
        self.add_loot_container(outpost, "Wasteland", (751, 2.2, -669), label="Search Expedition Supply Case")
        self.add_loot_container(outpost, "Wasteland", (759, 2.2, -669), label="Search Lead-Lined Hazmat Crate")
        self.add_lore_note(outpost, "Lore_17_CraterSurvey", "Red-Zone Dosimetry Log", (755, 2.2, -663))

        tower_zero = RbxInstance("Model", "BroadcastTowerZero_SignalOrigin")
        tz_bunker = self.build_enterable_building(tower_zero, "TowerZeroTransmitterBunker", 840, 0.0, -770, 34.0, 13.5, 30.0, (68, 64, 66), MAT_CONCRETE, "industrial_machinery", has_barricade=False)
        tower_zero.add_child(make_part("TowerZeroLowerMast", (9.0, 56.0, 9.0), cf_pos(840, 41.0, -770), (128, 54, 48), MAT_METAL, pbr_variant="rust"))
        tower_zero.add_child(make_part("TowerZeroUpperSpire", (4.2, 62.0, 4.2), cf_pos(840, 98.0, -770), (186, 182, 178), MAT_METAL))
        tower_zero.add_child(make_truss_ladder("TowerZeroMaintenanceLadder", 834.0, 14.0, -770.0, 52.0))

        apex_beacon = make_part("SignalOriginApexBeacon", (2.8, 2.8, 2.8), cf_pos(840, 130.0, -770), (255, 68, 52), MAT_NEON, shape=SHAPE_BALL)
        add_point_light(apex_beacon, (255, 68, 52), brightness=4.5, range_studs=60.0, shadows=False)
        tower_zero.add_child(apex_beacon)

        tz_console = make_part("TowerZeroMasterConsole", (5.6, 4.2, 2.8), cf_pos(840, 2.5, -770), (72, 78, 84), MAT_METAL, attrs={"QuestInteractId": "Console_TowerZero", "PromptLabel": "Override 142.8 MHz LAZARUS Carrier Wave"})
        add_point_light(tz_console, (235, 96, 62), brightness=2.2, range_studs=18.0)
        self.interactables.add_child(tz_console)

        self.add_loot_container(tz_bunker, "Wasteland", (830, 2.2, -776), label="Search Command Security Vault")
        self.add_loot_container(tz_bunker, "Military", (850, 2.2, -776), label="Search Emergency Munitions Locker")
        self.add_lore_note(tz_bunker, "Lore_18_FinalTransmission", "Dr. Vance — Tower Zero Final Entry", (835, 2.4, -764), is_terminal=True)

        wasteland.add_child(tower_zero)
        for sx, sz, sp in [(730, -710, "Contaminated"), (790, -735, "Brute"), (810, -745, "Screamer"), (868, -740, "Brute"), (820, -810, "PaleOne"), (705, -670, "Burrower"), (785, -640, "Watcher")]:
            self.add_enemy_spawn(sx, 3.5, sz, "RedWasteland", sp)
        self.world.add_child(wasteland)

        return self.world, self.interactables, self.enemy_spawns


def assemble_rbxl(output_path: Path) -> Dict[str, Any]:
    db = ReflectionDB(str(REPO_ROOT / "tools" / "rbx_reflection_db.msgpack"))
    ser = RbxlSerializer(db)
    root_services: List[RbxInstance] = []

    # 1. WORKSPACE
    ws = RbxInstance("Workspace", "Workspace")
    ws.set_prop("Gravity", 196.2)
    ws.set_prop("StreamingEnabled", True)
    ws.set_prop("StreamingMinRadius", 128)
    ws.set_prop("StreamingTargetRadius", 512)

    terrain = RbxInstance("Terrain", "Terrain")
    terrain.set_prop("Anchored", True)
    clouds = RbxInstance("Clouds", "AshesClouds")
    clouds.set_prop("Cover", 0.68)
    clouds.set_prop("Density", 0.72)
    clouds.set_prop("Color", Color3Val.from_rgb(138, 142, 148))
    terrain.add_child(clouds)
    ws.add_child(terrain)

    wb = WorldBuilder()
    world_folder, interact_folder, spawns_folder = wb.assemble_all_regions()
    ws.add_child(world_folder)
    ws.add_child(interact_folder)
    ws.add_child(spawns_folder)
    ws.add_child(RbxInstance("Folder", "ActiveEnemies"))
    root_services.append(ws)

    # 2. LIGHTING
    lighting = RbxInstance("Lighting", "Lighting")
    lighting.set_prop("Technology", 4)
    lighting.set_prop("GlobalShadows", True)
    lighting.set_prop("Brightness", 1.65)
    lighting.set_prop("ClockTime", 17.25)
    lighting.set_prop("GeographicLatitude", 46.5)
    lighting.set_prop("Ambient", Color3Val.from_rgb(28, 31, 36))
    lighting.set_prop("OutdoorAmbient", Color3Val.from_rgb(58, 63, 68))
    lighting.set_prop("ColorShift_Top", Color3Val.from_rgb(214, 198, 178))
    lighting.set_prop("ColorShift_Bottom", Color3Val.from_rgb(42, 46, 52))
    lighting.set_prop("EnvironmentDiffuseScale", 0.65)
    lighting.set_prop("EnvironmentSpecularScale", 0.72)
    lighting.set_prop("ShadowSoftness", 0.25)
    lighting.set_prop("ExposureCompensation", -0.08)

    atm = RbxInstance("Atmosphere", "AshesAtmosphere")
    atm.set_prop("Density", 0.34)
    atm.set_prop("Offset", 0.12)
    atm.set_prop("Color", Color3Val.from_rgb(146, 152, 156))
    atm.set_prop("Decay", Color3Val.from_rgb(88, 92, 96))
    atm.set_prop("Glare", 0.08)
    atm.set_prop("Haze", 1.45)
    lighting.add_child(atm)

    cc = RbxInstance("ColorCorrectionEffect", "AshesColorGrading")
    cc.set_prop("Saturation", -0.22)
    cc.set_prop("Contrast", 0.14)
    cc.set_prop("Brightness", -0.02)
    cc.set_prop("TintColor", Color3Val.from_rgb(236, 234, 228))
    lighting.add_child(cc)

    bloom = RbxInstance("BloomEffect", "AshesBloom")
    bloom.set_prop("Intensity", 0.35)
    bloom.set_prop("Size", 24.0)
    bloom.set_prop("Threshold", 1.15)
    lighting.add_child(bloom)

    dof = RbxInstance("DepthOfFieldEffect", "AshesDOF")
    dof.set_prop("FarIntensity", 0.16)
    dof.set_prop("FocusDistance", 65.0)
    dof.set_prop("InFocusRadius", 48.0)
    dof.set_prop("NearIntensity", 0.0)
    lighting.add_child(dof)

    sr = RbxInstance("SunRaysEffect", "AshesSunRays")
    sr.set_prop("Intensity", 0.08)
    sr.set_prop("Spread", 0.65)
    lighting.add_child(sr)
    root_services.append(lighting)

    # 3. MATERIALSERVICE (14 PBR MaterialVariants)
    mat_service = RbxInstance("MaterialService", "MaterialService")
    pbr_manifest = json.loads((REPO_ROOT / "assets" / "textures" / "pbr_manifest.json").read_text(encoding="utf-8"))
    pbr_base_map = {
        "rust": MAT_CORRODED_METAL,
        "concrete": MAT_CONCRETE,
        "brick": MAT_BRICK,
        "wood": MAT_WOOD_PLANKS,
        "asphalt": MAT_ASPHALT,
        "mud": MAT_MUD,
        "stone": MAT_SLATE,
        "fabric": MAT_FABRIC,
        "plastic": MAT_SMOOTH_PLASTIC,
        "glass": MAT_GLASS,
        "painted_metal": MAT_METAL,
        "damaged_wall": MAT_CONCRETE,
        "wet_surface": MAT_ASPHALT,
        "industrial_machinery": MAT_DIAMOND_PLATE,
    }
    for mname, maps in pbr_manifest["materials"].items():
        mv = RbxInstance("MaterialVariant", mname)
        mv.set_prop("BaseMaterial", pbr_base_map.get(mname, MAT_CONCRETE))
        mv.set_prop("StudsPerTile", 8.0)
        mv.set_attr("LocalColorMap", f"assets/textures/{maps['color']}")
        mv.set_attr("LocalNormalMap", f"assets/textures/{maps['normal']}")
        mv.set_attr("LocalRoughnessMap", f"assets/textures/{maps['roughness']}")
        mv.set_attr("LocalMetalnessMap", f"assets/textures/{maps['metalness']}")
        mat_service.add_child(mv)
    root_services.append(mat_service)

    # 4. REPLICATEDSTORAGE
    rep_storage = RbxInstance("ReplicatedStorage", "ReplicatedStorage")
    shared_folder = RbxInstance("Folder", "Shared")
    for sf in sorted((REPO_ROOT / "src" / "ReplicatedStorage" / "Shared").glob("*.luau")):
        shared_folder.add_child(make_script("ModuleScript", sf.stem, sf.read_text(encoding="utf-8")))
    rep_storage.add_child(shared_folder)

    remotes_folder = RbxInstance("Folder", "Remotes")
    event_names = [
        "StateSync", "VitalsUpdate", "InventoryUpdate", "WeatherUpdate",
        "WorldEventAlert", "HorrorCue", "RadioTransmission", "CombatFeedback",
        "QuestUpdate", "PartyUpdate", "ContainerOpened", "Notification",
        "MovementStance", "ToggleFlashlight", "WeaponFire", "WeaponReload",
        "MeleeSwing", "InventoryAction", "MedicalTreat", "LootInteract",
        "CraftRequest", "BuildAction", "InteractWorldObject", "QuestAction",
        "PartyAction", "SettingsUpdate", "RespawnRequest",
    ]
    for ev in event_names:
        remotes_folder.add_child(RbxInstance("RemoteEvent", ev))
    remotes_folder.add_child(RbxInstance("RemoteFunction", "PingState"))
    rep_storage.add_child(remotes_folder)

    assets_folder = RbxInstance("Folder", "Assets")
    mesh_manifest_json = (REPO_ROOT / "assets" / "meshes" / "asset_manifest.json").read_text(encoding="utf-8")
    pbr_manifest_raw = (REPO_ROOT / "assets" / "textures" / "pbr_manifest.json").read_text(encoding="utf-8")
    assets_folder.add_child(make_script("ModuleScript", "BlenderAssetManifest", f"--!strict\nreturn [===[\n{mesh_manifest_json}\n]===]\n"))
    assets_folder.add_child(make_script("ModuleScript", "PBRTextureManifest", f"--!strict\nreturn [===[\n{pbr_manifest_raw}\n]===]\n"))
    rep_storage.add_child(assets_folder)
    root_services.append(rep_storage)

    # 5. SERVERSCRIPTSERVICE
    sss = RbxInstance("ServerScriptService", "ServerScriptService")
    sss.add_child(make_script("Script", "Bootstrap", read_luau("src/ServerScriptService/Bootstrap.server.luau")))
    services_folder = RbxInstance("Folder", "Services")
    for sv_file in sorted((REPO_ROOT / "src" / "ServerScriptService" / "Services").glob("*.luau")):
        services_folder.add_child(make_script("ModuleScript", sv_file.stem, sv_file.read_text(encoding="utf-8")))
    sss.add_child(services_folder)
    root_services.append(sss)

    # 6. SERVERSTORAGE
    sstorage = RbxInstance("ServerStorage", "ServerStorage")
    enemy_templates = RbxInstance("Folder", "EnemyTemplates")
    for spec in MUTANT_SPECS:
        enemy_templates.add_child(build_mutant_template(spec))
    sstorage.add_child(enemy_templates)
    sstorage.add_child(build_weapon_templates())
    sstorage.add_child(build_building_templates())
    sstorage.add_child(RbxInstance("Folder", "ItemTemplates"))
    sstorage.add_child(RbxInstance("Folder", "LootTemplates"))
    root_services.append(sstorage)

    # 7. STARTERPLAYER
    starter_player = RbxInstance("StarterPlayer", "StarterPlayer")
    starter_player.set_prop("CharacterWalkSpeed", 12.5)
    starter_player.set_prop("CharacterJumpHeight", 6.2)
    starter_player.set_prop("CameraMaxZoomDistance", 10.5)
    starter_player.set_prop("NameDisplayDistance", 32.0)
    starter_player.set_prop("HealthDisplayDistance", 0.0)

    sps = RbxInstance("StarterPlayerScripts", "StarterPlayerScripts")
    sps.add_child(make_script("LocalScript", "ClientBootstrap", read_luau("src/StarterPlayer/StarterPlayerScripts/ClientBootstrap.client.luau")))
    controllers_folder = RbxInstance("Folder", "Controllers")
    for ctrl_file in sorted((REPO_ROOT / "src" / "StarterPlayer" / "StarterPlayerScripts" / "Controllers").glob("*.luau")):
        controllers_folder.add_child(make_script("ModuleScript", ctrl_file.stem, ctrl_file.read_text(encoding="utf-8")))
    sps.add_child(controllers_folder)
    starter_player.add_child(sps)
    starter_player.add_child(RbxInstance("StarterCharacterScripts", "StarterCharacterScripts"))
    root_services.append(starter_player)

    # 8. STARTERGUI
    starter_gui = RbxInstance("StarterGui", "StarterGui")
    main_ui = RbxInstance("ScreenGui", "MainInterface")
    main_ui.set_prop("ResetOnSpawn", False)
    main_ui.set_prop("IgnoreGuiInset", True)
    starter_gui.add_child(main_ui)
    root_services.append(starter_gui)

    # 9. SOUNDSERVICE & PLAYERS
    sound_svc = RbxInstance("SoundService", "SoundService")
    sound_svc.set_prop("AmbientReverb", 16)
    sound_svc.set_prop("RespectFilteringEnabled", True)
    root_services.append(sound_svc)

    players_svc = RbxInstance("Players", "Players")
    players_svc.set_prop("CharacterAutoLoads", True)
    root_services.append(players_svc)

    raw_bytes = ser.serialize(root_services)
    output_path.write_bytes(raw_bytes)
    stats = inspect_rbxl_binary(raw_bytes)
    stats["loot_containers"] = wb.loot_counter
    stats["enemy_spawn_points"] = wb.spawn_counter
    return stats


if __name__ == "__main__":
    out_file = REPO_ROOT / "ASHES_OF_HUMANITY.rbxl"
    summary = assemble_rbxl(out_file)
    print("ASSEMBLED ASHES_OF_HUMANITY.rbxl SUCCESSFULLY:")
    print(json.dumps(summary, indent=2))
