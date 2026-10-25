#!/usr/bin/env python3
"""
Blender 5.0 (bpy) 3D Asset, Rigged Horror Entity & Cycles Artwork Pipeline
for DON'T LOOK BACK.
Generates 33 multi-part, beveled, UV-mapped PBR .glb models + 3 skeletal-animated
entities + Cycles raytraced paintings & menu backdrop.
"""
import os
import math
import bpy
import bmesh
from mathutils import Vector, Euler

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MOD_DIR = os.path.join(ROOT, "assets", "models")
TEX_DIR = os.path.join(ROOT, "assets", "textures")
UI_DIR = os.path.join(ROOT, "assets", "ui")
os.makedirs(MOD_DIR, exist_ok=True)


def clear_scene():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for m in list(bpy.data.meshes):
        if m.users == 0:
            bpy.data.meshes.remove(m)
    for a in list(bpy.data.armatures):
        if a.users == 0:
            bpy.data.armatures.remove(a)
    for act in list(bpy.data.actions):
        if act.users == 0:
            bpy.data.actions.remove(act)
    for l in list(bpy.data.lights):
        if l.users == 0:
            bpy.data.lights.remove(l)
    for c in list(bpy.data.cameras):
        if c.users == 0:
            bpy.data.cameras.remove(c)


def make_mat(name, color=(0.5, 0.5, 0.5, 1.0), rough=0.65, metal=0.0, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if emission is not None and emission_strength > 0.0:
        if "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = (*emission[:3], 1.0)
            bsdf.inputs["Emission Strength"].default_value = emission_strength
        elif "Emission" in bsdf.inputs:
            bsdf.inputs["Emission"].default_value = (*emission[:3], 1.0)
    return mat


def add_box(name, size, loc, mat, bevel=0.008, rot=(0, 0, 0), subsurf=0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0:
        bv = obj.modifiers.new("Bevel", 'BEVEL')
        bv.width = bevel
        bv.segments = 2
        bpy.ops.object.modifier_apply(modifier=bv.name)
    if subsurf > 0:
        ss = obj.modifiers.new("Sub", 'SUBSURF')
        ss.levels = subsurf
        bpy.ops.object.modifier_apply(modifier=ss.name)
        bpy.ops.object.shade_smooth()
    obj.data.materials.append(mat)
    smart_uv(obj)
    return obj


def add_cyl(name, radius, depth, loc, mat, verts=20, rot=(0, 0, 0), smooth=True):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    if smooth:
        bpy.ops.object.shade_smooth()
    obj.data.materials.append(mat)
    smart_uv(obj)
    return obj


def add_sphere(name, radius, loc, mat, scale=(1, 1, 1), segs=20, rings=14):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segs, ring_count=rings, radius=radius, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bpy.ops.object.shade_smooth()
    obj.data.materials.append(mat)
    smart_uv(obj)
    return obj


def add_cone(name, r1, r2, depth, loc, mat, verts=24, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r1, radius2=r2, depth=depth, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    bpy.ops.object.shade_smooth()
    obj.data.materials.append(mat)
    smart_uv(obj)
    return obj


def smart_uv(obj):
    bpy.context.view_layer.objects.active = obj
    if len(obj.data.uv_layers) == 0:
        obj.data.uv_layers.new(name="UVMap")


def export_glb(filename, animations=False):
    path = os.path.join(MOD_DIR, filename)
    kwargs = dict(
        filepath=path,
        export_format='GLB',
        export_apply=True,
        export_yup=True,
        export_animations=animations,
    )
    if animations:
        kwargs["export_animation_mode"] = 'NLA_TRACKS'
        kwargs["export_apply"] = False
    bpy.ops.export_scene.gltf(**kwargs)
    print(f"  -> Exported {filename} ({os.path.getsize(path)//1024} KB)")


# ============================================================
# 1. ARCHITECTURAL DOORS (Pivoted at Hinge X=0, Y=0, Z=0..2.22)
# ============================================================
def build_doors():
    # Wood 4-panel apartment door
    clear_scene()
    m_wood = make_mat("WalnutDoor", (0.28, 0.17, 0.10, 1), rough=0.45)
    m_trim = make_mat("DarkTrim", (0.20, 0.12, 0.07, 1), rough=0.50)
    m_brass = make_mat("AgedBrass", (0.72, 0.56, 0.24, 1), rough=0.28, metal=0.88)
    W, T, H = 1.06, 0.052, 2.22
    # Main slab offset so X=0 is hinge edge
    add_box("Slab", (W, T, H), (W * 0.5, 0, H * 0.5), m_wood, bevel=0.005)
    # 4 raised/recessed panel moldings on both faces
    for px in (0.30, 0.76):
        for pz, ph in [(0.56, 0.72), (1.56, 0.86)]:
            for side in (-1, 1):
                add_box(f"Panel_{px}_{pz}_{side}", (0.32, 0.012, ph), (px, side * (T * 0.5 + 0.004), pz), m_trim, bevel=0.004)
    # Lockplate + Lever Handle on both sides at X = W - 0.11, Z = 1.02
    hx = W - 0.11
    add_box("PlateF", (0.045, 0.008, 0.18), (hx, -T * 0.5 - 0.004, 1.00), m_brass, bevel=0.002)
    add_box("PlateB", (0.045, 0.008, 0.18), (hx, T * 0.5 + 0.004, 1.00), m_brass, bevel=0.002)
    add_cyl("Spindle", 0.012, T + 0.09, (hx, 0, 1.03), m_brass, rot=(math.pi * 0.5, 0, 0))
    add_box("LeverF", (0.11, 0.018, 0.022), (hx - 0.045, -T * 0.5 - 0.045, 1.03), m_brass, bevel=0.004)
    add_box("LeverB", (0.11, 0.018, 0.022), (hx - 0.045, T * 0.5 + 0.045, 1.03), m_brass, bevel=0.004)
    for hz in (0.32, 1.11, 1.90):
        add_cyl(f"Hinge_{hz}", 0.014, 0.09, (0.005, 0, hz), m_brass)
    export_glb("door_wood_panel.glb")

    # Steel utility / maintenance door
    clear_scene()
    m_steel = make_mat("UtilitySteel", (0.34, 0.37, 0.39, 1), rough=0.42, metal=0.72)
    m_dark = make_mat("IronFrame", (0.16, 0.17, 0.19, 1), rough=0.55, metal=0.80)
    m_glass = make_mat("WireGlass", (0.18, 0.24, 0.26, 1), rough=0.15, metal=0.1)
    add_box("SteelSlab", (W, 0.056, H), (W * 0.5, 0, H * 0.5), m_steel, bevel=0.005)
    add_box("WindowBezel", (0.34, 0.072, 0.44), (W * 0.5, 0, 1.66), m_dark, bevel=0.004)
    add_box("WindowGlass", (0.28, 0.062, 0.38), (W * 0.5, 0, 1.66), m_glass, bevel=0.001)
    for i in range(6):
        add_box(f"Louver_{i}", (0.56, 0.068, 0.024), (W * 0.5, 0, 0.34 + i * 0.055), m_dark, rot=(0.35, 0, 0))
    add_cyl("HandleShaft", 0.014, 0.15, (W - 0.12, 0, 1.03), m_dark, rot=(math.pi * 0.5, 0, 0))
    add_box("HandleF", (0.13, 0.02, 0.024), (W - 0.17, -0.072, 1.03), m_dark, bevel=0.004)
    add_box("HandleB", (0.13, 0.02, 0.024), (W - 0.17, 0.072, 1.03), m_dark, bevel=0.004)
    export_glb("door_steel_utility.glb")

    # Emergency Exit Door with crash bar
    clear_scene()
    m_exit = make_mat("FireDoorRed", (0.48, 0.13, 0.12, 1), rough=0.45, metal=0.35)
    m_chrome = make_mat("ChromeBar", (0.78, 0.80, 0.82, 1), rough=0.22, metal=0.92)
    m_glow = make_mat("ExitSignGlow", (0.15, 0.88, 0.35, 1), rough=0.3, emission=(0.15, 0.95, 0.38), emission_strength=2.2)
    add_box("ExitSlab", (1.08, 0.058, H), (0.54, 0, H * 0.5), m_exit, bevel=0.005)
    add_box("CrashMountL", (0.08, 0.05, 0.12), (0.18, -0.045, 1.02), m_chrome, bevel=0.004)
    add_box("CrashMountR", (0.08, 0.05, 0.12), (0.90, -0.045, 1.02), m_chrome, bevel=0.004)
    add_box("CrashPushBar", (0.78, 0.036, 0.055), (0.54, -0.082, 1.02), m_chrome, bevel=0.006)
    add_box("VisionFrame", (0.20, 0.07, 0.58), (0.78, 0, 1.62), m_chrome, bevel=0.003)
    add_box("ExitBadge", (0.34, 0.068, 0.11), (0.54, 0, 2.04), m_glow, bevel=0.003)
    export_glb("door_emergency_exit.glb")


# ============================================================
# 2. FURNITURE, APPLIANCES & ARCHITECTURAL PROPS
# ============================================================
def build_furniture_and_props():
    m_walnut = make_mat("Walnut", (0.27, 0.16, 0.09, 1), rough=0.42)
    m_oak = make_mat("AgedOak", (0.38, 0.25, 0.14, 1), rough=0.48)
    m_fabric = make_mat("OliveFabric", (0.30, 0.33, 0.23, 1), rough=0.88)
    m_fabric2 = make_mat("BurgundyFabric", (0.36, 0.15, 0.14, 1), rough=0.86)
    m_linen = make_mat("AgedLinen", (0.72, 0.69, 0.61, 1), rough=0.85)
    m_quilt = make_mat("MutedBlueQuilt", (0.22, 0.28, 0.35, 1), rough=0.88)
    m_brass = make_mat("Brass", (0.72, 0.56, 0.24, 1), rough=0.28, metal=0.88)
    m_steel = make_mat("Steel", (0.56, 0.58, 0.60, 1), rough=0.30, metal=0.85)
    m_enamel = make_mat("AgedEnamel", (0.76, 0.75, 0.70, 1), rough=0.26, metal=0.15)
    m_dark = make_mat("MatteBlack", (0.10, 0.11, 0.12, 1), rough=0.55, metal=0.35)
    m_glass = make_mat("DarkGlass", (0.08, 0.12, 0.13, 1), rough=0.08, metal=0.20)
    m_iron = make_mat("RustedIron", (0.26, 0.18, 0.14, 1), rough=0.68, metal=0.65)
    m_red = make_mat("ValveRed", (0.58, 0.12, 0.10, 1), rough=0.45, metal=0.45)
    m_card = make_mat("Cardboard", (0.52, 0.40, 0.26, 1), rough=0.88)

    # Sofa Vintage
    clear_scene()
    add_box("BaseFrame", (1.96, 0.82, 0.22), (0, 0, 0.23), m_fabric, bevel=0.02)
    add_box("BackRest", (1.96, 0.20, 0.52), (0, 0.32, 0.58), m_fabric, bevel=0.04, subsurf=1)
    for sx in (-0.60, 0.0, 0.60):
        add_box(f"Seat_{sx}", (0.58, 0.62, 0.15), (sx, -0.06, 0.40), m_fabric, bevel=0.035, subsurf=1)
        add_box(f"BackCush_{sx}", (0.57, 0.16, 0.38), (sx, 0.21, 0.60), m_fabric, bevel=0.035, subsurf=1)
    for side in (-1, 1):
        add_box(f"Arm_{side}", (0.18, 0.84, 0.40), (side * 0.94, -0.01, 0.44), m_fabric, bevel=0.04, subsurf=1)
        for ly in (-0.34, 0.34):
            add_cyl(f"Leg_{side}_{ly}", 0.03, 0.14, (side * 0.88, ly, 0.07), m_walnut)
    export_glb("sofa_vintage.glb")

    # Armchair Worn
    clear_scene()
    add_box("ChairBase", (0.86, 0.82, 0.22), (0, 0, 0.24), m_fabric2, bevel=0.025)
    add_box("SeatCushion", (0.64, 0.64, 0.16), (0, -0.05, 0.41), m_fabric2, bevel=0.035, subsurf=1)
    add_box("BackCushion", (0.68, 0.20, 0.50), (0, 0.28, 0.62), m_fabric2, bevel=0.04, subsurf=1)
    for s in (-1, 1):
        add_box(f"Arm_{s}", (0.15, 0.82, 0.36), (s * 0.39, 0, 0.46), m_fabric2, bevel=0.035, subsurf=1)
        for ly in (-0.32, 0.32):
            add_cyl(f"Leg_{s}_{ly}", 0.028, 0.15, (s * 0.36, ly, 0.075), m_walnut)
    export_glb("armchair_worn.glb")

    # Coffee Table
    clear_scene()
    add_box("TableTop", (1.20, 0.60, 0.045), (0, 0, 0.43), m_oak, bevel=0.01)
    add_box("LowerShelf", (1.02, 0.46, 0.025), (0, 0, 0.16), m_oak, bevel=0.005)
    for sx in (-0.52, 0.52):
        for sy in (-0.24, 0.24):
            add_cyl(f"Leg_{sx}_{sy}", 0.024, 0.42, (sx, sy, 0.21), m_walnut)
    export_glb("coffee_table.glb")

    # Dining Table & Wooden Chair
    clear_scene()
    add_box("DiningTop", (1.36, 0.82, 0.045), (0, 0, 0.74), m_walnut, bevel=0.008)
    add_box("ApronX1", (1.22, 0.03, 0.08), (0, -0.34, 0.68), m_walnut)
    add_box("ApronX2", (1.22, 0.03, 0.08), (0, 0.34, 0.68), m_walnut)
    for sx in (-0.60, 0.60):
        for sy in (-0.34, 0.34):
            add_box(f"DLeg_{sx}_{sy}", (0.055, 0.055, 0.72), (sx, sy, 0.36), m_walnut, bevel=0.005)
    export_glb("dining_table.glb")

    clear_scene()
    add_box("Seat", (0.45, 0.44, 0.038), (0, 0, 0.45), m_oak, bevel=0.008)
    for sx in (-0.19, 0.19):
        add_box(f"FrontLeg_{sx}", (0.038, 0.038, 0.44), (sx, -0.18, 0.22), m_walnut, bevel=0.004)
        add_box(f"BackPost_{sx}", (0.038, 0.038, 0.88), (sx, 0.19, 0.44), m_walnut, bevel=0.004)
    add_box("TopRail", (0.42, 0.03, 0.07), (0, 0.19, 0.84), m_walnut, bevel=0.005)
    for sp in (-0.09, 0.0, 0.09):
        add_cyl(f"Spindle_{sp}", 0.011, 0.34, (sp, 0.19, 0.64), m_oak)
    export_glb("wooden_chair.glb")

    # Apartment Bed
    clear_scene()
    add_box("BedFrame", (1.44, 2.04, 0.16), (0, 0, 0.24), m_walnut, bevel=0.008)
    add_box("Headboard", (1.48, 0.07, 0.72), (0, 0.99, 0.62), m_walnut, bevel=0.01)
    add_box("Footboard", (1.48, 0.06, 0.44), (0, -0.99, 0.38), m_walnut, bevel=0.01)
    add_box("Mattress", (1.36, 1.92, 0.22), (0, 0, 0.42), m_linen, bevel=0.035, subsurf=1)
    add_box("Quilt", (1.39, 1.36, 0.24), (0, -0.26, 0.43), m_quilt, bevel=0.04, subsurf=1)
    for px in (-0.34, 0.34):
        add_box(f"Pillow_{px}", (0.52, 0.34, 0.12), (px, 0.70, 0.58), m_linen, bevel=0.04, subsurf=1)
    for sx in (-0.68, 0.68):
        for sy in (-0.98, 0.98):
            add_box(f"Post_{sx}_{sy}", (0.07, 0.07, 0.28), (sx, sy, 0.14), m_walnut)
    export_glb("bed_apartment.glb")

    # Wardrobe Cabinet
    clear_scene()
    add_box("Carcass", (1.14, 0.56, 1.94), (0, 0, 1.01), m_walnut, bevel=0.006)
    add_box("Crown", (1.22, 0.61, 0.07), (0, -0.01, 2.01), m_walnut, bevel=0.008)
    add_box("Plinth", (1.18, 0.58, 0.08), (0, 0, 0.04), m_walnut, bevel=0.005)
    for s in (-1, 1):
        add_box(f"Door_{s}", (0.52, 0.03, 1.76), (s * 0.27, -0.29, 1.02), m_oak, bevel=0.005)
        add_cyl(f"Knob_{s}", 0.016, 0.04, (s * 0.06, -0.32, 1.02), m_brass, rot=(math.pi * 0.5, 0, 0))
    export_glb("wardrobe_cabinet.glb")

    # Study Desk & Filing Cabinet
    clear_scene()
    add_box("DeskTop", (1.36, 0.68, 0.045), (0, 0, 0.74), m_walnut, bevel=0.008)
    add_box("PedestalR", (0.42, 0.62, 0.70), (0.44, 0, 0.36), m_walnut, bevel=0.006)
    add_box("LegPanelL", (0.05, 0.62, 0.72), (-0.62, 0, 0.36), m_walnut, bevel=0.005)
    add_box("ModestyBack", (1.02, 0.03, 0.46), (-0.08, 0.26, 0.48), m_walnut)
    for dz in (0.18, 0.38, 0.58):
        add_box(f"Drawer_{dz}", (0.36, 0.025, 0.16), (0.44, -0.32, dz), m_oak, bevel=0.004)
        add_box(f"Pull_{dz}", (0.10, 0.025, 0.02), (0.44, -0.34, dz), m_brass, bevel=0.003)
    export_glb("desk_study.glb")

    clear_scene()
    add_box("FileBody", (0.48, 0.62, 1.32), (0, 0, 0.66), m_steel, bevel=0.006)
    for i in range(4):
        zc = 0.19 + i * 0.30
        add_box(f"FDrawer_{i}", (0.42, 0.025, 0.26), (0, -0.32, zc), m_steel, bevel=0.004)
        add_box(f"Label_{i}", (0.11, 0.015, 0.045), (0, -0.335, zc + 0.05), m_brass, bevel=0.002)
        add_box(f"Handle_{i}", (0.13, 0.03, 0.02), (0, -0.34, zc - 0.03), m_dark, bevel=0.003)
    export_glb("filing_cabinet.glb")

    # Tall Bookshelf with 3D Books
    clear_scene()
    add_box("BackBoard", (1.08, 0.025, 2.00), (0, 0.16, 1.00), m_walnut)
    for sx in (-0.53, 0.53):
        add_box(f"Side_{sx}", (0.035, 0.35, 2.00), (sx, 0, 1.00), m_walnut, bevel=0.004)
    book_mats = [
        make_mat("BookRed", (0.42, 0.12, 0.11, 1), 0.7),
        make_mat("BookGreen", (0.14, 0.28, 0.18, 1), 0.7),
        make_mat("BookNavy", (0.13, 0.18, 0.34, 1), 0.7),
        make_mat("BookTan", (0.48, 0.38, 0.24, 1), 0.75),
    ]
    for si, sz in enumerate([0.08, 0.46, 0.84, 1.22, 1.60, 1.98]):
        add_box(f"Shelf_{si}", (1.04, 0.34, 0.03), (0, 0, sz), m_oak, bevel=0.003)
        if si < 5:
            bx = -0.44
            bi = 0
            while bx < 0.40:
                bw = 0.038 + ((si * 7 + bi * 3) % 4) * 0.012
                bh = 0.22 + ((si * 5 + bi * 11) % 5) * 0.022
                tilt = 0.12 if bi == 6 else 0.0
                add_box(
                    f"Book_{si}_{bi}",
                    (bw, 0.21, bh),
                    (bx + bw * 0.5, 0.02, sz + 0.015 + bh * 0.5),
                    book_mats[(si + bi) % len(book_mats)],
                    bevel=0.003,
                    rot=(0, tilt, 0),
                )
                bx += bw + (0.045 if bi == 5 else 0.006)
                bi += 1
    export_glb("bookshelf_tall.glb")

    # Metal Storage Rack with Archive Boxes
    clear_scene()
    for sx in (-0.60, 0.60):
        for sy in (-0.23, 0.23):
            add_box(f"Post_{sx}_{sy}", (0.04, 0.04, 1.94), (sx, sy, 0.97), m_iron)
    for si, sz in enumerate([0.12, 0.70, 1.28, 1.86]):
        add_box(f"RackShelf_{si}", (1.22, 0.48, 0.03), (0, 0, sz), m_steel, bevel=0.003)
    for bi, (bx, bz, bw, bh) in enumerate([
        (-0.34, 0.29, 0.38, 0.30), (0.22, 0.27, 0.42, 0.26),
        (-0.28, 0.88, 0.44, 0.32), (0.30, 0.85, 0.34, 0.26),
        (-0.10, 1.45, 0.46, 0.30),
    ]):
        add_box(f"Box_{bi}", (bw, 0.36, bh), (bx, 0, bz), m_card, bevel=0.006)
        add_box(f"BoxLid_{bi}", (bw + 0.02, 0.38, 0.04), (bx, 0, bz + bh * 0.5 - 0.01), m_card, bevel=0.004)
    export_glb("shelving_metal_boxes.glb")

    # CRT Television Console
    clear_scene()
    add_box("TVBody", (0.66, 0.44, 0.48), (0, 0, 0.28), m_walnut, bevel=0.012)
    add_box("TVBezel", (0.50, 0.04, 0.38), (-0.06, -0.22, 0.28), m_dark, bevel=0.008)
    add_sphere("CRTScreen", 0.24, (-0.06, -0.21, 0.28), m_glass, scale=(0.94, 0.22, 0.72))
    for kz in (0.36, 0.27):
        add_cyl(f"Dial_{kz}", 0.028, 0.03, (0.24, -0.23, kz), m_brass, rot=(math.pi * 0.5, 0, 0))
    for s in (-1, 1):
        add_cyl(f"Antenna_{s}", 0.005, 0.46, (s * 0.12, -0.05, 0.70), m_steel, rot=(0, s * 0.45, 0))
        for ly in (-0.16, 0.16):
            add_cyl(f"TVLeg_{s}_{ly}", 0.02, 0.08, (s * 0.26, ly, 0.04), m_dark)
    export_glb("tv_crt_console.glb")

    # Vintage Radio
    clear_scene()
    m_dial = make_mat("RadioDialGlow", (0.95, 0.74, 0.34, 1), rough=0.25, emission=(0.95, 0.70, 0.28), emission_strength=1.5)
    add_box("RadioCase", (0.38, 0.18, 0.22), (0, 0, 0.11), m_walnut, bevel=0.01)
    add_box("SpeakerCloth", (0.20, 0.015, 0.14), (-0.06, -0.09, 0.11), m_fabric, bevel=0.003)
    add_box("TuningWindow", (0.11, 0.015, 0.05), (0.11, -0.09, 0.15), m_dial, bevel=0.002)
    for kx in (0.08, 0.14):
        add_cyl(f"RKnob_{kx}", 0.018, 0.025, (kx, -0.098, 0.07), m_brass, rot=(math.pi * 0.5, 0, 0))
    add_cyl("WhipAnt", 0.004, 0.38, (0.15, 0.05, 0.38), m_steel, rot=(0, 0.22, 0))
    export_glb("radio_vintage.glb")

    # Magnetic Tape Recorder (for the 4 playable distorted recordings!)
    clear_scene()
    add_box("DeckChassis", (0.36, 0.26, 0.085), (0, 0, 0.045), m_dark, bevel=0.008)
    add_box("FacePlate", (0.33, 0.23, 0.012), (0, 0, 0.090), m_steel, bevel=0.003)
    for rx in (-0.085, 0.085):
        add_cyl(f"Reel_{rx}", 0.068, 0.014, (rx, 0.03, 0.104), m_brass, verts=24)
        add_cyl(f"TapeHub_{rx}", 0.042, 0.016, (rx, 0.03, 0.104), m_dark, verts=20)
    for bi in range(5):
        add_box(f"Key_{bi}", (0.028, 0.042, 0.018), (-0.08 + bi * 0.036, -0.095, 0.096), m_red if bi == 2 else m_steel, bevel=0.003)
    add_box("VUMeter", (0.06, 0.035, 0.014), (0.115, -0.085, 0.096), m_dial, bevel=0.002)
    export_glb("tape_recorder.glb")

    # Retro Refrigerator & Kitchen Counter/Stove
    clear_scene()
    add_box("FridgeBody", (0.76, 0.70, 1.72), (0, 0, 0.88), m_enamel, bevel=0.035, subsurf=1)
    add_box("FreezerDoor", (0.72, 0.04, 0.46), (0, -0.35, 1.46), m_enamel, bevel=0.02)
    add_box("MainDoor", (0.72, 0.04, 1.12), (0, -0.35, 0.62), m_enamel, bevel=0.02)
    add_box("HandleTop", (0.03, 0.05, 0.22), (-0.28, -0.39, 1.42), m_steel, bevel=0.005)
    add_box("HandleBot", (0.03, 0.05, 0.34), (-0.28, -0.39, 0.92), m_steel, bevel=0.005)
    export_glb("refrigerator_retro.glb")

    clear_scene()
    add_box("CounterBase", (1.64, 0.60, 0.86), (0, 0, 0.43), m_walnut, bevel=0.006)
    add_box("CounterTop", (1.68, 0.64, 0.045), (0, -0.01, 0.88), m_enamel, bevel=0.008)
    add_box("SinkBasin", (0.54, 0.42, 0.03), (-0.42, 0, 0.905), m_steel, bevel=0.005)
    add_cyl("FaucetStem", 0.016, 0.24, (-0.42, 0.18, 1.02), m_steel)
    add_box("FaucetSpout", (0.024, 0.15, 0.024), (-0.42, 0.11, 1.13), m_steel, bevel=0.005)
    add_box("OvenWindow", (0.52, 0.025, 0.36), (0.40, -0.31, 0.42), m_glass, bevel=0.004)
    for bx in (0.24, 0.54):
        for by in (-0.12, 0.12):
            add_cyl(f"Burner_{bx}_{by}", 0.095, 0.018, (bx, by, 0.91), m_dark, verts=20)
    export_glb("kitchen_counter_stove.glb")

    # Commercial Washing Machine
    clear_scene()
    add_box("WasherBody", (0.68, 0.68, 0.92), (0, 0, 0.46), m_enamel, bevel=0.018)
    add_box("ControlHeader", (0.68, 0.16, 0.14), (0, 0.26, 0.96), m_steel, bevel=0.008)
    add_cyl("ProgramDial", 0.034, 0.03, (0.20, 0.17, 0.96), m_dark, rot=(math.pi * 0.5, 0, 0))
    add_cyl("DoorRing", 0.22, 0.05, (0, -0.34, 0.48), m_steel, verts=28, rot=(math.pi * 0.5, 0, 0))
    add_sphere("DrumGlass", 0.17, (0, -0.31, 0.48), m_glass, scale=(1.0, 0.45, 1.0))
    export_glb("washing_machine.glb")

    # Electrical Panel (Wall-mounted at Y=0 back face)
    clear_scene()
    add_box("PanelBox", (0.54, 0.14, 0.76), (0, -0.07, 0), m_steel, bevel=0.006)
    add_box("InnerRecess", (0.44, 0.03, 0.64), (0, -0.135, 0), m_dark, bevel=0.003)
    for row in range(6):
        zc = -0.20 + row * 0.075
        for side in (-1, 1):
            add_box(f"Breaker_{row}_{side}", (0.08, 0.03, 0.035), (side * 0.09, -0.15, zc), m_dark, bevel=0.003)
    add_box("MainThrow", (0.10, 0.05, 0.05), (0, -0.16, 0.25), m_red, bevel=0.004)
    for cx in (-0.16, 0.0, 0.16):
        add_cyl(f"Conduit_{cx}", 0.022, 0.42, (cx, -0.06, 0.58), m_steel)
    export_glb("electrical_panel.glb")

    # Industrial Pipe Assembly & Boiler
    clear_scene()
    for pz, r in [(0.16, 0.055), (-0.12, 0.038)]:
        add_cyl(f"Pipe_{pz}", r, 2.80, (0, 0, pz), m_iron, rot=(0, math.pi * 0.5, 0))
        for fx in (-1.1, 0.0, 1.1):
            add_cyl(f"Flange_{pz}_{fx}", r * 1.45, 0.035, (fx, 0, pz), m_iron, rot=(0, math.pi * 0.5, 0))
    add_cyl("ValveStem", 0.018, 0.14, (0.35, -0.08, 0.16), m_brass, rot=(math.pi * 0.5, 0, 0))
    bpy.ops.mesh.primitive_torus_add(major_radius=0.11, minor_radius=0.016, location=(0.35, -0.15, 0.16), rotation=(math.pi * 0.5, 0, 0))
    wheel = bpy.context.active_object
    wheel.data.materials.append(m_red)
    export_glb("pipe_assembly.glb")

    clear_scene()
    add_cyl("BoilerTank", 0.68, 2.35, (0, 0, 0.92), m_iron, verts=28, rot=(0, math.pi * 0.5, 0))
    for sx in (-1.16, 1.16):
        add_sphere(f"Cap_{sx}", 0.68, (sx, 0, 0.92), m_iron, scale=(0.35, 1.0, 1.0))
    for lx in (-0.75, 0.75):
        add_box(f"Cradle_{lx}", (0.18, 1.05, 0.52), (lx, 0, 0.26), m_dark, bevel=0.008)
    add_cyl("SteamDome", 0.24, 0.55, (0.0, 0, 1.72), m_iron)
    add_cyl("Hatch", 0.28, 0.08, (-1.38, 0, 0.86), m_red, rot=(0, math.pi * 0.5, 0))
    export_glb("boiler_industrial.glb")

    # Coat Rack Normal vs Anomaly #10 Twisted Sculpture
    clear_scene()
    add_cyl("BaseDisc", 0.26, 0.04, (0, 0, 0.02), m_walnut, verts=24)
    add_cyl("MainPole", 0.028, 1.78, (0, 0, 0.91), m_walnut)
    for i in range(4):
        ang = i * math.pi * 0.5
        hx = math.cos(ang) * 0.14
        hy = math.sin(ang) * 0.14
        add_cyl(f"Hook_{i}", 0.012, 0.26, (hx, hy, 1.68), m_walnut, rot=(math.sin(ang) * 0.7, -math.cos(ang) * 0.7, 0))
    export_glb("coat_rack_normal.glb")

    clear_scene()
    m_bone = make_mat("TwistedWoodBone", (0.34, 0.24, 0.20, 1), rough=0.58)
    add_cyl("WarpBase", 0.30, 0.06, (0, 0, 0.03), m_bone)
    # Gnarled organic twisting spine + asymmetric rib-hooks
    for seg in range(9):
        zf = 0.14 + seg * 0.19
        ox = math.sin(seg * 0.85) * 0.11
        oy = math.cos(seg * 0.70) * 0.09
        add_cyl(f"Spine_{seg}", 0.045 - seg * 0.002, 0.24, (ox, oy, zf), m_bone, rot=(0.25 * math.cos(seg), 0.28 * math.sin(seg), 0))
        if seg >= 3:
            side = 1 if seg % 2 == 0 else -1
            add_cone(f"Rib_{seg}", 0.032, 0.006, 0.48, (ox + side * 0.20, oy, zf + 0.06), m_bone, rot=(0.2, side * 1.15, 0.3 * seg))
    export_glb("coat_rack_twisted_anomaly.glb")

    # Framed Painting (Frame + separate UV-mapped CanvasSurface mesh!)
    clear_scene()
    PW, PH = 1.04, 0.80
    for bx, bz, sx, sz in [
        (0, PH * 0.5 - 0.035, PW, 0.07),
        (0, -PH * 0.5 + 0.035, PW, 0.07),
        (-PW * 0.5 + 0.035, 0, 0.07, PH - 0.14),
        (PW * 0.5 - 0.035, 0, 0.07, PH - 0.14),
    ]:
        add_box(f"Frame_{bx}_{bz}", (sx, 0.055, sz), (bx, -0.028, bz), m_walnut, bevel=0.008)
    # Dedicated Canvas mesh on -Y front face (faces -Z / +Z cleanly in Godot)
    mesh = bpy.data.meshes.new("CanvasMesh")
    cw, ch = (PW - 0.13) * 0.5, (PH - 0.13) * 0.5
    verts = [(-cw, -0.032, -ch), (cw, -0.032, -ch), (cw, -0.032, ch), (-cw, -0.032, ch)]
    mesh.from_pydata(verts, [], [(0, 1, 2, 3)])
    uv_layer = mesh.uv_layers.new(name="UVMap")
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for loop_i, uv in enumerate(uvs):
        uv_layer.data[loop_i].uv = uv
    c_obj = bpy.data.objects.new("CanvasSurface", mesh)
    bpy.context.scene.collection.objects.link(c_obj)
    c_obj.data.materials.append(make_mat("CanvasDefault", (0.8, 0.8, 0.8, 1), rough=0.75))
    add_cyl("BrassLight", 0.018, 0.34, (0, -0.09, PH * 0.5 + 0.04), m_brass, rot=(0, math.pi * 0.5, 0))
    export_glb("painting_framed.glb")


# ============================================================
# 3. LIGHTING FIXTURES & HANDHELD TOOLS
# ============================================================
def build_lights_and_tools():
    m_brass = make_mat("LampBrass", (0.72, 0.56, 0.24, 1), rough=0.28, metal=0.88)
    m_shade = make_mat("WarmShade", (0.88, 0.78, 0.56, 1), rough=0.75, emission=(0.95, 0.78, 0.48), emission_strength=1.4)
    m_tube = make_mat("FluorTube", (0.94, 0.96, 0.90, 1), rough=0.2, emission=(0.96, 0.95, 0.86), emission_strength=2.8)
    m_red_glow = make_mat("RedBeacon", (0.95, 0.12, 0.10, 1), rough=0.2, emission=(1.0, 0.12, 0.08), emission_strength=3.2)
    m_steel = make_mat("FixtureSteel", (0.58, 0.60, 0.62, 1), rough=0.35, metal=0.82)
    m_black = make_mat("AnodizedBlack", (0.09, 0.10, 0.12, 1), rough=0.38, metal=0.78)
    m_lens = make_mat("TorchLens", (0.98, 0.96, 0.88, 1), rough=0.1, emission=(1.0, 0.96, 0.85), emission_strength=3.5)

    # Floor Lamp
    clear_scene()
    add_cyl("LampBase", 0.21, 0.04, (0, 0, 0.02), m_brass, verts=24)
    add_cyl("LampStem", 0.018, 1.42, (0, 0, 0.73), m_brass)
    add_cone("LampShade", 0.24, 0.14, 0.30, (0, 0, 1.48), m_shade, verts=24)
    export_glb("lamp_floor.glb")

    # Desk Lamp
    clear_scene()
    add_cyl("DeskBase", 0.10, 0.025, (0, 0, 0.012), m_brass)
    add_cyl("Arm1", 0.010, 0.28, (0, 0.04, 0.15), m_brass, rot=(-0.35, 0, 0))
    add_cyl("Arm2", 0.010, 0.22, (0, -0.02, 0.34), m_brass, rot=(0.55, 0, 0))
    add_cone("Hood", 0.09, 0.035, 0.13, (0, -0.10, 0.39), m_shade, rot=(0.45, 0, 0))
    export_glb("lamp_desk.glb")

    # Wall Sconce
    clear_scene()
    add_box("BackPlate", (0.14, 0.025, 0.24), (0, -0.012, 0), m_brass, bevel=0.004)
    add_box("ArmBracket", (0.03, 0.10, 0.03), (0, -0.06, -0.05), m_brass)
    add_cone("FrostedGlass", 0.065, 0.095, 0.18, (0, -0.11, 0.04), m_shade, verts=20)
    export_glb("sconce_wall.glb")

    # Ceiling Fluorescent Fixture (Origin at ceiling Z=0 hanging down)
    clear_scene()
    add_box("TrofferHousing", (1.24, 0.28, 0.06), (0, 0, -0.03), m_steel, bevel=0.005)
    for ty in (-0.065, 0.065):
        add_cyl(f"Tube_{ty}", 0.019, 1.16, (0, ty, -0.072), m_tube, rot=(0, math.pi * 0.5, 0))
    export_glb("fluorescent_fixture.glb")

    # Emergency Red Strobe Beacon
    clear_scene()
    add_cyl("BeaconBase", 0.095, 0.04, (0, -0.02, 0), m_steel, rot=(math.pi * 0.5, 0, 0))
    add_sphere("RedDome", 0.078, (0, -0.07, 0), m_red_glow, scale=(1.0, 1.2, 1.0))
    export_glb("emergency_beacon.glb")

    # Handheld Tactical Flashlight (aligned along -Y forward -> -Z in Godot)
    clear_scene()
    add_cyl("Barrel", 0.022, 0.18, (0, 0, 0), m_black, verts=20, rot=(math.pi * 0.5, 0, 0))
    add_cone("HeadBezel", 0.036, 0.022, 0.06, (0, -0.11, 0), m_black, verts=24, rot=(-math.pi * 0.5, 0, 0))
    add_cyl("FrontLens", 0.032, 0.008, (0, -0.14, 0), m_lens, verts=24, rot=(math.pi * 0.5, 0, 0))
    add_box("ThumbSwitch", (0.014, 0.026, 0.010), (0, -0.04, 0.023), m_red_glow, bevel=0.002)
    export_glb("flashlight_handheld.glb")

    # Inspector's Anomaly Field Scanner / Clipboard
    clear_scene()
    m_board = make_mat("ClipboardWood", (0.34, 0.21, 0.11, 1), rough=0.55)
    m_paper = make_mat("ReportSheet", (0.86, 0.83, 0.76, 1), rough=0.85)
    m_led = make_mat("ScannerLED", (0.20, 0.90, 0.55, 1), rough=0.2, emission=(0.20, 0.95, 0.55), emission_strength=2.2)
    add_box("Board", (0.23, 0.31, 0.012), (0, 0, 0), m_board, bevel=0.004)
    add_box("Sheet", (0.20, 0.26, 0.004), (0, -0.01, 0.008), m_paper)
    add_box("BrassClip", (0.09, 0.04, 0.022), (0, 0.125, 0.015), m_brass, bevel=0.003)
    add_box("SensorUnit", (0.075, 0.11, 0.028), (0.075, 0.08, 0.018), m_black, bevel=0.004)
    add_box("LEDStrip", (0.05, 0.018, 0.006), (0.075, 0.10, 0.033), m_led)
    export_glb("scanner_clipboard.glb")


# ============================================================
# 4. THREE RIGGED & SKELETAL-ANIMATED HORROR ENTITIES
# ============================================================
def create_humanoid_rig(rig_name, height_scale=1.0, neck_len=0.25, arm_len=0.72):
    arm_data = bpy.data.armatures.new(rig_name)
    arm_obj = bpy.data.objects.new(rig_name, arm_data)
    bpy.context.scene.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode='EDIT')
    eb = arm_data.edit_bones

    def bone(name, head, tail, parent=None):
        b = eb.new(name)
        b.head = head
        b.tail = tail
        if parent:
            b.parent = parent
        return b

    pelvis_z = 0.98 * height_scale
    chest_z = 1.48 * height_scale
    neck_top_z = chest_z + neck_len
    head_top_z = neck_top_z + 0.28 * height_scale

    b_root = bone("Root", (0, 0, 0), (0, 0, 0.35))
    b_pelvis = bone("Pelvis", (0, 0, pelvis_z - 0.12), (0, 0, pelvis_z + 0.12), b_root)
    b_spine = bone("Spine", (0, 0, pelvis_z + 0.12), (0, 0, chest_z), b_pelvis)
    b_neck = bone("Neck", (0, 0, chest_z), (0, 0, neck_top_z), b_spine)
    b_head = bone("Head", (0, 0, neck_top_z), (0, 0, head_top_z), b_neck)

    for s_tag, sx in [("L", 1.0), ("R", -1.0)]:
        sh_x = sx * 0.22 * height_scale
        b_uarm = bone(f"UpperArm.{s_tag}", (sh_x, 0, chest_z - 0.04), (sh_x + sx * 0.06, 0, chest_z - arm_len * 0.52), b_spine)
        bone(f"ForeArm.{s_tag}", (sh_x + sx * 0.06, 0, chest_z - arm_len * 0.52), (sh_x + sx * 0.09, -0.04, chest_z - arm_len), b_uarm)
        hip_x = sx * 0.12 * height_scale
        b_thigh = bone(f"Thigh.{s_tag}", (hip_x, 0, pelvis_z - 0.08), (hip_x, 0.02, pelvis_z * 0.48), b_pelvis)
        bone(f"Shin.{s_tag}", (hip_x, 0.02, pelvis_z * 0.48), (hip_x, 0, 0.03), b_thigh)

    bpy.ops.object.mode_set(mode='OBJECT')
    return arm_obj


def bind_parts_to_rig(parts_by_bone, arm_obj, mesh_name):
    # Join all body parts into one clean skinned mesh with exact bone vertex groups
    bpy.ops.object.select_all(action='DESELECT')
    all_objs = []
    for bone_name, objs in parts_by_bone.items():
        for o in objs:
            vg = o.vertex_groups.new(name=bone_name)
            vg.add(list(range(len(o.data.vertices))), 1.0, 'REPLACE')
            all_objs.append(o)
            o.select_set(True)
    bpy.context.view_layer.objects.active = all_objs[0]
    bpy.ops.object.join()
    joined = bpy.context.active_object
    joined.name = mesh_name
    joined.parent = arm_obj
    mod = joined.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm_obj
    return joined


def push_nla_action(arm_obj, anim_name, keyframes_dict):
    if not arm_obj.animation_data:
        arm_obj.animation_data_create()
    act = bpy.data.actions.new(name=anim_name)
    arm_obj.animation_data.action = act
    if hasattr(act, "slots") and len(act.slots) == 0:
        slot = act.slots.new(id_type='OBJECT', name="RigSlot")
        arm_obj.animation_data.action_slot = slot

    # Reset pose first
    for pb in arm_obj.pose.bones:
        pb.rotation_mode = 'XYZ'
        pb.rotation_euler = (0, 0, 0)
        pb.location = (0, 0, 0)

    all_frames = sorted({f for frames in keyframes_dict.values() for f in frames.keys()})
    for f in all_frames:
        bpy.context.scene.frame_set(f)
        for bname, fdict in keyframes_dict.items():
            if f in fdict:
                pb = arm_obj.pose.bones[bname]
                rot = fdict[f].get("rot")
                loc = fdict[f].get("loc")
                if rot is not None:
                    pb.rotation_euler = rot
                    pb.keyframe_insert(data_path="rotation_euler", frame=f)
                if loc is not None:
                    pb.location = loc
                    pb.keyframe_insert(data_path="location", frame=f)

    track = arm_obj.animation_data.nla_tracks.new()
    track.name = anim_name
    strip = track.strips.new(anim_name, 1, act)
    strip.name = anim_name
    arm_obj.animation_data.action = None


def build_horror_entities():
    # ------------------------------------------------------------
    # ENTITY 1: THE OBSERVER (2.32m gaunt pale inspector in tattered coat)
    # ------------------------------------------------------------
    clear_scene()
    m_coat = make_mat("ObserverCoat", (0.09, 0.10, 0.12, 1), rough=0.82)
    m_pale = make_mat("ObserverSkin", (0.76, 0.73, 0.69, 1), rough=0.45)
    m_void = make_mat("ObserverVoid", (0.02, 0.01, 0.02, 1), rough=0.95)
    m_eye = make_mat("ObserverPinEye", (0.95, 0.85, 0.55, 1), rough=0.2, emission=(0.95, 0.80, 0.45), emission_strength=2.2)

    rig = create_humanoid_rig("ObserverRig", height_scale=1.18, neck_len=0.36, arm_len=0.96)
    parts = {
        "Pelvis": [
            add_cone("CoatSkirt", 0.34, 0.19, 0.86, (0, 0, 0.82), m_coat, verts=18),
        ],
        "Spine": [
            add_cone("TorsoCoat", 0.18, 0.27, 0.68, (0, 0, 1.48), m_coat, verts=18),
            add_box("CollarL", (0.12, 0.18, 0.18), (0.13, -0.02, 1.76), m_coat, rot=(0, 0.25, 0)),
            add_box("CollarR", (0.12, 0.18, 0.18), (-0.13, -0.02, 1.76), m_coat, rot=(0, -0.25, 0)),
        ],
        "Neck": [
            add_cyl("LongNeck", 0.055, 0.38, (0, -0.01, 1.92), m_pale, verts=16),
        ],
        "Head": [
            add_sphere("GauntHead", 0.145, (0, -0.03, 2.24), m_pale, scale=(0.82, 0.95, 1.38)),
            add_sphere("HollowSocket", 0.058, (0, -0.155, 2.26), m_void, scale=(1.25, 0.45, 0.85)),
            add_sphere("PinPupil", 0.016, (0, -0.172, 2.26), m_eye),
        ],
        "UpperArm.L": [add_cyl("UArmL", 0.055, 0.50, (0.28, 0, 1.46), m_coat)],
        "ForeArm.L": [
            add_cyl("FArmL", 0.042, 0.48, (0.34, -0.02, 0.98), m_pale),
            add_box("HandL", (0.04, 0.07, 0.18), (0.35, -0.04, 0.68), m_pale, bevel=0.008),
        ],
        "UpperArm.R": [add_cyl("UArmR", 0.055, 0.50, (-0.28, 0, 1.46), m_coat)],
        "ForeArm.R": [
            add_cyl("FArmR", 0.042, 0.48, (-0.34, -0.02, 0.98), m_pale),
            add_box("HandR", (0.04, 0.07, 0.18), (-0.35, -0.04, 0.68), m_pale, bevel=0.008),
        ],
        "Thigh.L": [add_cyl("ThighL", 0.075, 0.56, (0.14, 0, 0.80), m_coat)],
        "Shin.L": [add_cyl("ShinL", 0.055, 0.56, (0.14, 0, 0.28), m_void)],
        "Thigh.R": [add_cyl("ThighR", 0.075, 0.56, (-0.14, 0, 0.80), m_coat)],
        "Shin.R": [add_cyl("ShinR", 0.055, 0.56, (-0.14, 0, 0.28), m_void)],
    }
    bind_parts_to_rig(parts, rig, "ObserverMesh")

    push_nla_action(rig, "FrozenA", {
        "Head": {1: {"rot": (0.12, 0.42, 0.18)}, 24: {"rot": (0.12, 0.42, 0.18)}},
        "Neck": {1: {"rot": (0.18, -0.15, 0.0)}, 24: {"rot": (0.18, -0.15, 0.0)}},
        "UpperArm.L": {1: {"rot": (-0.35, 0.0, 0.18)}, 24: {"rot": (-0.35, 0.0, 0.18)}},
        "ForeArm.R": {1: {"rot": (-0.65, 0.0, -0.12)}, 24: {"rot": (-0.65, 0.0, -0.12)}},
    })
    push_nla_action(rig, "FrozenB", {
        "Spine": {1: {"rot": (0.22, 0.14, 0.0)}, 24: {"rot": (0.22, 0.14, 0.0)}},
        "Head": {1: {"rot": (-0.15, -0.52, -0.22)}, 24: {"rot": (-0.15, -0.52, -0.22)}},
        "UpperArm.R": {1: {"rot": (-1.15, 0.0, -0.25)}, 24: {"rot": (-1.15, 0.0, -0.25)}},
        "UpperArm.L": {1: {"rot": (0.20, 0.0, 0.22)}, 24: {"rot": (0.20, 0.0, 0.22)}},
    })
    push_nla_action(rig, "StalkWalk", {
        "Thigh.L": {1: {"rot": (0.55, 0, 0)}, 12: {"rot": (-0.55, 0, 0)}, 24: {"rot": (0.55, 0, 0)}},
        "Thigh.R": {1: {"rot": (-0.55, 0, 0)}, 12: {"rot": (0.55, 0, 0)}, 24: {"rot": (-0.55, 0, 0)}},
        "UpperArm.L": {1: {"rot": (-0.45, 0, 0.1)}, 12: {"rot": (0.35, 0, 0.1)}, 24: {"rot": (-0.45, 0, 0.1)}},
        "UpperArm.R": {1: {"rot": (0.35, 0, -0.1)}, 12: {"rot": (-0.45, 0, -0.1)}, 24: {"rot": (0.35, 0, -0.1)}},
        "Head": {1: {"rot": (0.15, 0.28, 0)}, 12: {"rot": (0.18, -0.28, 0)}, 24: {"rot": (0.15, 0.28, 0)}},
    })
    push_nla_action(rig, "Lunge", {
        "Spine": {1: {"rot": (0.1, 0, 0)}, 8: {"rot": (-0.45, 0, 0)}, 18: {"rot": (0.1, 0, 0)}},
        "UpperArm.L": {1: {"rot": (0, 0, 0)}, 8: {"rot": (-1.45, 0, 0.2)}, 18: {"rot": (0, 0, 0)}},
        "UpperArm.R": {1: {"rot": (0, 0, 0)}, 8: {"rot": (-1.45, 0, -0.2)}, 18: {"rot": (0, 0, 0)}},
    })
    export_glb("observer_entity.glb", animations=True)

    # ------------------------------------------------------------
    # ENTITY 2: THE LISTENER (Ash-grey eyeless acoustic predator)
    # ------------------------------------------------------------
    clear_scene()
    m_flesh = make_mat("ListenerFlesh", (0.36, 0.33, 0.31, 1), rough=0.58)
    m_rib = make_mat("ListenerBone", (0.58, 0.53, 0.46, 1), rough=0.42)
    m_inner = make_mat("ListenerMaw", (0.32, 0.06, 0.06, 1), rough=0.35)

    rig2 = create_humanoid_rig("ListenerRig", height_scale=1.04, neck_len=0.22, arm_len=0.92)
    spine_parts = [add_sphere("Thorax", 0.22, (0, 0.02, 1.32), m_flesh, scale=(0.95, 0.78, 1.35))]
    for ri in range(4):
        rz = 1.18 + ri * 0.09
        spine_parts.append(add_box(f"RibL_{ri}", (0.24, 0.14, 0.025), (0.09, -0.06, rz), m_rib, bevel=0.006, rot=(0, 0.2, 0.2)))
        spine_parts.append(add_box(f"RibR_{ri}", (0.24, 0.14, 0.025), (-0.09, -0.06, rz), m_rib, bevel=0.006, rot=(0, -0.2, -0.2)))

    parts2 = {
        "Pelvis": [add_sphere("Hip", 0.16, (0, 0.04, 0.96), m_flesh, scale=(0.9, 0.8, 0.9))],
        "Spine": spine_parts,
        "Neck": [add_cyl("Neck", 0.06, 0.24, (0, -0.02, 1.66), m_flesh)],
        "Head": [
            add_sphere("BlindSkull", 0.15, (0, -0.05, 1.88), m_rib, scale=(0.85, 1.15, 1.05)),
            add_cone("CrestL", 0.11, 0.02, 0.26, (0.14, -0.02, 1.94), m_inner, rot=(0, 0.65, 0)),
            add_cone("CrestR", 0.11, 0.02, 0.26, (-0.14, -0.02, 1.94), m_inner, rot=(0, -0.65, 0)),
        ],
        "UpperArm.L": [add_cyl("UAL", 0.05, 0.48, (0.24, 0, 1.30), m_flesh)],
        "ForeArm.L": [
            add_cyl("FAL", 0.042, 0.50, (0.30, -0.05, 0.84), m_flesh),
            add_cone("ClawL", 0.045, 0.005, 0.24, (0.31, -0.08, 0.52), m_rib, rot=(math.pi, 0, 0)),
        ],
        "UpperArm.R": [add_cyl("UAR", 0.05, 0.48, (-0.24, 0, 1.30), m_flesh)],
        "ForeArm.R": [
            add_cyl("FAR", 0.042, 0.50, (-0.30, -0.05, 0.84), m_flesh),
            add_cone("ClawR", 0.045, 0.005, 0.24, (-0.31, -0.08, 0.52), m_rib, rot=(math.pi, 0, 0)),
        ],
        "Thigh.L": [add_cyl("TL", 0.07, 0.50, (0.13, 0, 0.72), m_flesh)],
        "Shin.L": [add_cyl("SL", 0.048, 0.50, (0.13, 0.03, 0.26), m_rib)],
        "Thigh.R": [add_cyl("TR", 0.07, 0.50, (-0.13, 0, 0.72), m_flesh)],
        "Shin.R": [add_cyl("SR", 0.048, 0.50, (-0.13, 0.03, 0.26), m_rib)],
    }
    bind_parts_to_rig(parts2, rig2, "ListenerMesh")

    push_nla_action(rig2, "IdleListen", {
        "Spine": {1: {"rot": (-0.25, 0, 0)}, 20: {"rot": (-0.30, 0, 0)}, 40: {"rot": (-0.25, 0, 0)}},
        "Head": {1: {"rot": (0.1, 0.48, 0.25)}, 20: {"rot": (0.1, -0.48, -0.25)}, 40: {"rot": (0.1, 0.48, 0.25)}},
    })
    push_nla_action(rig2, "PatrolWalk", {
        "Spine": {1: {"rot": (-0.32, 0, 0)}, 24: {"rot": (-0.32, 0, 0)}},
        "Thigh.L": {1: {"rot": (0.5, 0, 0)}, 12: {"rot": (-0.5, 0, 0)}, 24: {"rot": (0.5, 0, 0)}},
        "Thigh.R": {1: {"rot": (-0.5, 0, 0)}, 12: {"rot": (0.5, 0, 0)}, 24: {"rot": (-0.5, 0, 0)}},
        "Head": {1: {"rot": (0, 0.35, 0.15)}, 12: {"rot": (0, -0.35, -0.15)}, 24: {"rot": (0, 0.35, 0.15)}},
    })
    push_nla_action(rig2, "HuntSprint", {
        "Spine": {1: {"rot": (-0.52, 0, 0)}, 14: {"rot": (-0.52, 0, 0)}},
        "Thigh.L": {1: {"rot": (0.85, 0, 0)}, 7: {"rot": (-0.85, 0, 0)}, 14: {"rot": (0.85, 0, 0)}},
        "Thigh.R": {1: {"rot": (-0.85, 0, 0)}, 7: {"rot": (0.85, 0, 0)}, 14: {"rot": (-0.85, 0, 0)}},
        "UpperArm.L": {1: {"rot": (-0.75, 0, 0.15)}, 7: {"rot": (0.45, 0, 0.15)}, 14: {"rot": (-0.75, 0, 0.15)}},
        "UpperArm.R": {1: {"rot": (0.45, 0, -0.15)}, 7: {"rot": (-0.75, 0, -0.15)}, 14: {"rot": (0.45, 0, -0.15)}},
    })
    push_nla_action(rig2, "Strike", {
        "UpperArm.L": {1: {"rot": (0.2, 0, 0)}, 6: {"rot": (-1.6, 0, 0.3)}, 16: {"rot": (0.2, 0, 0)}},
        "UpperArm.R": {1: {"rot": (0.2, 0, 0)}, 8: {"rot": (-1.6, 0, -0.3)}, 16: {"rot": (0.2, 0, 0)}},
    })
    export_glb("listener_entity.glb", animations=True)

    # ------------------------------------------------------------
    # ENTITY 3: THE IMITATOR (Uncanny mimic silhouette)
    # ------------------------------------------------------------
    clear_scene()
    m_mimic_wood = make_mat("ImitatorShell", (0.26, 0.17, 0.11, 1), rough=0.46)
    m_mimic_flesh = make_mat("ImitatorCore", (0.16, 0.12, 0.15, 1), rough=0.65)
    m_mimic_glow = make_mat("ImitatorEye", (0.88, 0.25, 0.18, 1), rough=0.2, emission=(0.95, 0.22, 0.14), emission_strength=2.4)

    rig3 = create_humanoid_rig("ImitatorRig", height_scale=1.10, neck_len=0.30, arm_len=0.88)
    parts3 = {
        "Pelvis": [add_cyl("StemPelvis", 0.12, 0.42, (0, 0, 0.98), m_mimic_wood)],
        "Spine": [add_cone("LampMantle", 0.24, 0.11, 0.62, (0, 0, 1.44), m_mimic_flesh)],
        "Neck": [add_cyl("ThinSpindleNeck", 0.038, 0.32, (0, 0, 1.82), m_mimic_wood)],
        "Head": [
            add_cone("ShadeMask", 0.22, 0.13, 0.28, (0, 0, 2.06), m_mimic_wood),
            add_sphere("InnerEyeL", 0.022, (0.045, -0.12, 2.02), m_mimic_glow),
            add_sphere("InnerEyeR", 0.022, (-0.045, -0.12, 2.02), m_mimic_glow),
        ],
        "UpperArm.L": [add_cyl("BranchUL", 0.032, 0.46, (0.24, 0, 1.42), m_mimic_wood)],
        "ForeArm.L": [add_cone("BranchFL", 0.03, 0.008, 0.48, (0.31, -0.02, 0.98), m_mimic_wood, rot=(math.pi, 0, 0))],
        "UpperArm.R": [add_cyl("BranchUR", 0.032, 0.46, (-0.24, 0, 1.42), m_mimic_wood)],
        "ForeArm.R": [add_cone("BranchFR", 0.03, 0.008, 0.48, (-0.31, -0.02, 0.98), m_mimic_wood, rot=(math.pi, 0, 0))],
        "Thigh.L": [add_cyl("StiltTL", 0.045, 0.54, (0.12, 0, 0.76), m_mimic_flesh)],
        "Shin.L": [add_cyl("StiltSL", 0.035, 0.54, (0.12, 0, 0.26), m_mimic_wood)],
        "Thigh.R": [add_cyl("StiltTR", 0.045, 0.54, (-0.12, 0, 0.76), m_mimic_flesh)],
        "Shin.R": [add_cyl("StiltSR", 0.035, 0.54, (-0.12, 0, 0.26), m_mimic_wood)],
    }
    bind_parts_to_rig(parts3, rig3, "ImitatorMesh")

    push_nla_action(rig3, "DisguisePose", {
        "UpperArm.L": {1: {"rot": (0, 0, -0.35)}, 24: {"rot": (0, 0, -0.35)}},
        "UpperArm.R": {1: {"rot": (0, 0, 0.35)}, 24: {"rot": (0, 0, 0.35)}},
    })
    push_nla_action(rig3, "Twitch", {
        "Head": {1: {"rot": (0, 0, 0)}, 5: {"rot": (0.18, 0, 0.38)}, 9: {"rot": (-0.12, 0, -0.25)}, 16: {"rot": (0, 0, 0)}},
    })
    push_nla_action(rig3, "UnfoldWalk", {
        "UpperArm.L": {1: {"rot": (-0.5, 0, 0.65)}, 10: {"rot": (0.4, 0, 0.85)}, 20: {"rot": (-0.5, 0, 0.65)}},
        "UpperArm.R": {1: {"rot": (0.4, 0, -0.65)}, 10: {"rot": (-0.5, 0, -0.85)}, 20: {"rot": (0.4, 0, -0.65)}},
        "Thigh.L": {1: {"rot": (0.65, 0, 0)}, 10: {"rot": (-0.65, 0, 0)}, 20: {"rot": (0.65, 0, 0)}},
        "Thigh.R": {1: {"rot": (-0.65, 0, 0)}, 10: {"rot": (0.65, 0, 0)}, 20: {"rot": (-0.65, 0, 0)}},
    })
    push_nla_action(rig3, "Ambush", {
        "Spine": {1: {"rot": (0, 0, 0)}, 6: {"rot": (-0.55, 0, 0)}, 16: {"rot": (0, 0, 0)}},
        "UpperArm.L": {1: {"rot": (0, 0, 0)}, 6: {"rot": (-1.5, 0, 0.5)}, 16: {"rot": (0, 0, 0)}},
        "UpperArm.R": {1: {"rot": (0, 0, 0)}, 6: {"rot": (-1.5, 0, -0.5)}, 16: {"rot": (0, 0, 0)}},
    })
    export_glb("imitator_entity.glb", animations=True)


# ============================================================
# 5. CYCLES RAYTRACED PAINTINGS & HERO MENU BACKDROP
# ============================================================
def render_cycles_artwork():
    # A. Raytraced Coastal Lighthouse Oil Painting (512x384)
    clear_scene()
    scn = bpy.context.scene
    scn.render.engine = 'CYCLES'
    scn.cycles.device = 'CPU'
    scn.cycles.samples = 24
    scn.render.resolution_x = 512
    scn.render.resolution_y = 384

    world = bpy.data.worlds.new("NightCoast")
    scn.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.025, 0.042, 0.078, 1.0)
    bg.inputs[1].default_value = 1.1

    m_sea = make_mat("DarkSea", (0.03, 0.07, 0.11, 1), rough=0.16, metal=0.25)
    m_rock = make_mat("CliffRock", (0.08, 0.07, 0.06, 1), rough=0.92)
    m_stone = make_mat("TowerStone", (0.72, 0.69, 0.62, 1), rough=0.75)
    m_beacon = make_mat("BeaconGlow", (1.0, 0.85, 0.45, 1), rough=0.1, emission=(1.0, 0.84, 0.42), emission_strength=18.0)

    # Displaced wavy ocean mesh
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=48, y_subdivisions=48, size=18.0, location=(0, 3.0, -0.35))
    sea = bpy.context.active_object
    for v in sea.data.vertices:
        v.co.z += 0.09 * math.sin(v.co.x * 2.8 + v.co.y * 1.9) + 0.05 * math.cos(v.co.y * 4.5)
    bpy.ops.object.shade_smooth()
    sea.data.materials.append(m_sea)

    add_cone("Cliff", 2.4, 0.6, 1.8, (-1.35, 2.2, 0.2), m_rock, verts=9)
    add_cone("Cliff2", 1.8, 0.8, 1.2, (-0.3, 2.5, -0.05), m_rock, verts=8)
    add_cone("Tower", 0.34, 0.21, 2.05, (-1.15, 2.1, 1.65), m_stone, verts=20)
    add_cyl("Lantern", 0.19, 0.28, (-1.15, 2.1, 2.75), m_beacon, verts=16)
    add_cone("Roof", 0.25, 0.02, 0.25, (-1.15, 2.1, 2.98), m_rock, verts=16)
    add_sphere("Moon", 0.55, (2.2, 9.5, 4.1), make_mat("MoonGlow", (0.85, 0.88, 0.96, 1), emission=(0.82, 0.88, 1.0), emission_strength=6.0))

    ldata = bpy.data.lights.new("MoonKey", type='AREA')
    ldata.energy = 260
    ldata.color = (0.65, 0.78, 1.0)
    lobj = bpy.data.objects.new("MoonKey", ldata)
    scn.collection.objects.link(lobj)
    lobj.location = (3.5, -2.5, 4.5)
    lobj.rotation_euler = (1.1, 0, 0.6)

    cdata = bpy.data.cameras.new("Cam")
    cdata.lens = 42
    cam = bpy.data.objects.new("Cam", cdata)
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.location = (0.2, -3.4, 1.25)
    cam.rotation_euler = (1.46, 0, 0.06)

    scn.render.filepath = os.path.join(TEX_DIR, "painting_coastal_normal.png")
    bpy.ops.render.render(write_still=True)

    # B. Raytraced Hollow-Eyed Portrait Anomaly Painting (512x384)
    clear_scene()
    scn = bpy.context.scene
    scn.render.engine = 'CYCLES'
    scn.cycles.device = 'CPU'
    scn.cycles.samples = 24
    scn.render.resolution_x = 512
    scn.render.resolution_y = 384

    world = bpy.data.worlds.new("DarkPortrait")
    scn.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.018, 0.010, 0.010, 1.0)

    m_wax = make_mat("PallidWax", (0.58, 0.50, 0.44, 1), rough=0.48)
    m_hole = make_mat("AbyssBlack", (0.008, 0.004, 0.004, 1), rough=0.98)
    m_blood = make_mat("DarkCrimsonOil", (0.26, 0.015, 0.015, 1), rough=0.18)
    m_iris = make_mat("RedPin", (0.95, 0.16, 0.12, 1), emission=(1.0, 0.15, 0.10), emission_strength=4.5)
    m_suit = make_mat("SuitCoat", (0.05, 0.04, 0.045, 1), rough=0.88)

    add_box("CanvasBackdrop", (3.2, 0.1, 2.6), (0, 0.55, 0), make_mat("Back", (0.04, 0.022, 0.02, 1), rough=0.9))
    add_sphere("Cranium", 0.36, (0, 0, 0.18), m_wax, scale=(0.82, 0.88, 1.24))
    add_cyl("GauntNeck", 0.13, 0.45, (0, 0.03, -0.32), m_wax)
    add_cone("Shoulders", 0.58, 0.22, 0.55, (0, 0.05, -0.62), m_suit)
    # Recessed dark eye sockets & elongated scream mouth
    for sx in (-0.115, 0.115):
        add_sphere(f"Socket_{sx}", 0.078, (sx, -0.285, 0.22), m_hole, scale=(1.0, 0.45, 1.18))
        add_sphere(f"Pin_{sx}", 0.016, (sx, -0.318, 0.22), m_iris)
        # Organic tapering crimson oil streaks
        for di, ox in enumerate([0.0, sx * 0.12]):
            add_cyl(f"Drip_{sx}_{di}", 0.014 - di * 0.004, 0.42 - di * 0.12, (sx + ox, -0.305, -0.02 + di * 0.05), m_blood)
    add_sphere("GapingMouth", 0.095, (0, -0.285, -0.03), m_hole, scale=(0.68, 0.45, 1.48))

    key = bpy.data.lights.new("ChiaroscuroKey", type='SPOT')
    key.energy = 38
    key.color = (1.0, 0.78, 0.62)
    kobj = bpy.data.objects.new("ChiaroscuroKey", key)
    scn.collection.objects.link(kobj)
    kobj.location = (0.95, -1.45, 0.85)
    kobj.rotation_euler = (1.12, 0, 0.54)

    rim = bpy.data.lights.new("RedRim", type='POINT')
    rim.energy = 12
    rim.color = (0.95, 0.18, 0.14)
    robj = bpy.data.objects.new("RedRim", rim)
    scn.collection.objects.link(robj)
    robj.location = (-0.85, -0.8, -0.3)

    cdata = bpy.data.cameras.new("PCam")
    cdata.lens = 55
    cam = bpy.data.objects.new("PCam", cdata)
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.location = (0, -1.75, 0.05)
    cam.rotation_euler = (math.pi * 0.5, 0, 0)

    scn.render.filepath = os.path.join(TEX_DIR, "painting_hollow_anomaly.png")
    bpy.ops.render.render(write_still=True)

    # C. Raytraced Main Menu Hero Corridor Render (1280x720)
    clear_scene()
    scn = bpy.context.scene
    scn.render.engine = 'CYCLES'
    scn.cycles.device = 'CPU'
    scn.cycles.samples = 20
    scn.render.resolution_x = 960
    scn.render.resolution_y = 540

    m_wall = make_mat("HallWall", (0.34, 0.31, 0.25, 1), rough=0.78)
    m_wainscot = make_mat("Wainscot", (0.16, 0.10, 0.06, 1), rough=0.48)
    m_floor = make_mat("HallCarpet", (0.24, 0.08, 0.08, 1), rough=0.88)
    m_ceil = make_mat("HallCeil", (0.42, 0.41, 0.38, 1), rough=0.88)
    m_warm = make_mat("SconceBulb", (1.0, 0.82, 0.52, 1), emission=(1.0, 0.78, 0.42), emission_strength=8.0)
    m_fig = make_mat("Silhouette", (0.03, 0.03, 0.04, 1), rough=0.9)

    add_box("Floor", (3.0, 22.0, 0.1), (0, 8.0, -0.05), m_floor)
    add_box("Ceil", (3.0, 22.0, 0.1), (0, 8.0, 3.15), m_ceil)
    for sx in (-1.45, 1.45):
        add_box(f"Wall_{sx}", (0.1, 22.0, 3.2), (sx, 8.0, 1.6), m_wall)
        add_box(f"Wain_{sx}", (0.14, 22.0, 1.05), (sx, 8.0, 0.52), m_wainscot)
        for dy in (2.0, 7.0, 12.0, 17.0):
            add_box(f"DoorFrame_{sx}_{dy}", (0.18, 1.12, 2.25), (sx, dy, 1.12), m_wainscot)
    add_box("EndWall", (3.0, 0.15, 3.2), (0, 19.0, 1.6), m_wall)
    add_box("EndDoor", (1.1, 0.18, 2.25), (0, 18.92, 1.12), m_wainscot)

    # Sconces + practical warm lights + red emergency glow at far corridor end
    for ly in (1.5, 6.5, 11.5):
        add_sphere(f"Bulb_{ly}", 0.08, (-1.32, ly, 2.25), m_warm)
        pl = bpy.data.lights.new(f"PL_{ly}", type='POINT')
        pl.energy = 28 if ly < 10 else 12
        pl.color = (1.0, 0.78, 0.48)
        pobj = bpy.data.objects.new(f"PL_{ly}", pl)
        scn.collection.objects.link(pobj)
        pobj.location = (-1.15, ly, 2.22)

    red_l = bpy.data.lights.new("EndRed", type='POINT')
    red_l.energy = 42
    red_l.color = (1.0, 0.18, 0.12)
    robj = bpy.data.objects.new("EndRed", red_l)
    scn.collection.objects.link(robj)
    robj.location = (0.0, 17.8, 2.35)

    # Tall Observer silhouette at corridor end
    add_cone("FigBody", 0.28, 0.16, 1.85, (0.08, 16.2, 0.92), m_fig)
    add_sphere("FigHead", 0.14, (0.12, 16.2, 2.08), m_fig, scale=(0.8, 0.9, 1.3))

    cdata = bpy.data.cameras.new("MenuCam")
    cdata.lens = 28
    cam = bpy.data.objects.new("MenuCam", cdata)
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.location = (0.18, -1.6, 1.58)
    cam.rotation_euler = (1.55, 0.03, 0.02)

    scn.render.filepath = os.path.join(UI_DIR, "menu_hero_bg.png")
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    print("=== Building Architectural Doors ===")
    build_doors()
    print("=== Building Furniture, Appliances & Props ===")
    build_furniture_and_props()
    print("=== Building Lighting Fixtures & Inspector Tools ===")
    build_lights_and_tools()
    print("=== Building 3 Rigged & Skeletal-Animated Horror Entities ===")
    build_horror_entities()
    print("=== Rendering Cycles Raytraced Paintings & Hero Backdrop ===")
    render_cycles_artwork()
    print("ALL BLENDER ASSETS COMPLETE! Total models:", len(os.listdir(MOD_DIR)))
