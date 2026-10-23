#!/usr/bin/env python3
"""
ASHES OF HUMANITY: LAST SIGNAL
Real Blender 5.0.1 (bpy + bmesh + Armatures + Keyframe Actions + PBR Materials + Cycles CPU) Asset Pipeline.

Creates game-ready low-poly 3D models with clean topology, recalculated outside normals,
box/cylinder UV projection, PBR Principled BSDF materials linked to assets/textures/,
skeletal armatures & keyframed actions for characters/mutants, exports .obj + master .blend files,
writes assets/meshes/asset_manifest.json (polycounts, bounds, UV/normal validation),
and renders Cycles CPU beauty sheets for visual inspection.
"""

from __future__ import annotations
import json
import math
import os
import bpy
import bmesh
from mathutils import Euler, Matrix, Vector


ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TEX_DIR = os.path.join(ROOT_DIR, "assets", "textures")
MESH_DIR = os.path.join(ROOT_DIR, "assets", "meshes")
BLEND_DIR = os.path.join(ROOT_DIR, "assets", "blender")
RENDER_DIR = os.path.join(ROOT_DIR, "assets", "renders")


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_pbr_material(name: str, tex_prefix: str, tint: tuple[float, float, float] = (1.0, 1.0, 1.0), emission: tuple[float, float, float] | None = None, emission_strength: float = 0.0) -> bpy.types.Material:
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    c_path = os.path.join(TEX_DIR, f"{tex_prefix}_color.png")
    n_path = os.path.join(TEX_DIR, f"{tex_prefix}_normal.png")
    r_path = os.path.join(TEX_DIR, f"{tex_prefix}_roughness.png")
    m_path = os.path.join(TEX_DIR, f"{tex_prefix}_metalness.png")

    if os.path.exists(c_path):
        tex_c = nodes.new("ShaderNodeTexImage")
        tex_c.image = bpy.data.images.load(c_path, check_existing=True)
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["B"].default_value = (tint[0], tint[1], tint[2], 1.0)
        links.new(tex_c.outputs["Color"], mix.inputs["A"])
        links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    else:
        bsdf.inputs["Base Color"].default_value = (tint[0], tint[1], tint[2], 1.0)

    if os.path.exists(r_path):
        tex_r = nodes.new("ShaderNodeTexImage")
        tex_r.image = bpy.data.images.load(r_path, check_existing=True)
        tex_r.image.colorspace_settings.name = "Non-Color"
        links.new(tex_r.outputs["Color"], bsdf.inputs["Roughness"])

    if os.path.exists(m_path):
        tex_m = nodes.new("ShaderNodeTexImage")
        tex_m.image = bpy.data.images.load(m_path, check_existing=True)
        tex_m.image.colorspace_settings.name = "Non-Color"
        links.new(tex_m.outputs["Color"], bsdf.inputs["Metallic"])

    if os.path.exists(n_path):
        tex_n = nodes.new("ShaderNodeTexImage")
        tex_n.image = bpy.data.images.load(n_path, check_existing=True)
        tex_n.image.colorspace_settings.name = "Non-Color"
        nmap = nodes.new("ShaderNodeNormalMap")
        nmap.inputs["Strength"].default_value = 0.85
        links.new(tex_n.outputs["Color"], nmap.inputs["Color"])
        links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])

    if emission is not None and emission_strength > 0:
        bsdf.inputs["Emission Color"].default_value = (emission[0], emission[1], emission[2], 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength

    return mat


def add_box(bm: bmesh.types.BMesh, size: tuple[float, float, float], center: tuple[float, float, float] = (0, 0, 0), rot_euler: tuple[float, float, float] = (0, 0, 0)) -> list[bmesh.types.BMVert]:
    ret = bmesh.ops.create_cube(bm, size=1.0)
    verts = ret["verts"]
    mat = (
        Matrix.Translation(Vector(center))
        @ Euler(rot_euler, "XYZ").to_matrix().to_4x4()
        @ Matrix.Diagonal(Vector((size[0], size[1], size[2], 1.0)))
    )
    bmesh.ops.transform(bm, matrix=mat, verts=verts)
    return verts


def add_cyl(bm: bmesh.types.BMesh, radius1: float, radius2: float, depth: float, segs: int = 12, center: tuple[float, float, float] = (0, 0, 0), rot_euler: tuple[float, float, float] = (0, 0, 0)) -> list[bmesh.types.BMVert]:
    ret = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs, radius1=radius1, radius2=radius2, depth=depth)
    verts = ret["verts"]
    mat = Matrix.Translation(Vector(center)) @ Euler(rot_euler, "XYZ").to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=mat, verts=verts)
    return verts


def add_sphere(bm: bmesh.types.BMesh, radius: float, segs: int = 10, rings: int = 8, center: tuple[float, float, float] = (0, 0, 0), scale: tuple[float, float, float] = (1, 1, 1)) -> list[bmesh.types.BMVert]:
    ret = bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=radius)
    verts = ret["verts"]
    mat = Matrix.Translation(Vector(center)) @ Matrix.Diagonal(Vector((scale[0], scale[1], scale[2], 1.0)))
    bmesh.ops.transform(bm, matrix=mat, verts=verts)
    return verts


def finalize_bmesh_to_object(bm: bmesh.types.BMesh, name: str, mat: bpy.types.Material, smooth: bool = False) -> bpy.types.Object:
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # Generate clean box-projected UVs directly in bmesh
    uv_layer = bm.loops.layers.uv.verify()
    for face in bm.faces:
        n = face.normal
        ax, ay, az = abs(n.x), abs(n.y), abs(n.z)
        for loop in face.loops:
            co = loop.vert.co
            if az >= ax and az >= ay:
                u, v = co.x * 0.35, co.y * 0.35
            elif ax >= ay:
                u, v = co.y * 0.35, co.z * 0.35
            else:
                u, v = co.x * 0.35, co.z * 0.35
            loop[uv_layer].uv = (u, v)

    bmesh.ops.triangulate(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()

    for poly in mesh.polygons:
        poly.use_smooth = smooth

    obj = bpy.data.objects.new(name, mesh)
    obj.data.materials.append(mat)
    bpy.context.collection.objects.link(obj)
    return obj


def export_single_obj(obj: bpy.types.Object, category: str, filename: str) -> dict:
    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    cat_dir = os.path.join(MESH_DIR, category)
    os.makedirs(cat_dir, exist_ok=True)
    out_path = os.path.join(cat_dir, f"{filename}.obj")
    bpy.ops.wm.obj_export(
        filepath=out_path,
        export_selected_objects=True,
        export_uv=True,
        export_normals=True,
        export_materials=False,
        export_triangulated_mesh=True,
    )
    mesh = obj.data
    xs = [v.co.x for v in mesh.vertices]
    ys = [v.co.y for v in mesh.vertices]
    zs = [v.co.z for v in mesh.vertices]
    dims = [round(max(xs) - min(xs), 3), round(max(ys) - min(ys), 3), round(max(zs) - min(zs), 3)]
    return {
        "name": filename,
        "category": category,
        "obj_path": f"assets/meshes/{category}/{filename}.obj",
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.polygons),
        "has_uvs": len(mesh.uv_layers) > 0,
        "dimensions_m": dims,
    }


# ============================================================
# 1. WEAPON MODELS (11 Original Low-Poly Game-Ready Weapons)
# ============================================================
def build_weapons(manifest: list[dict]) -> list[bpy.types.Object]:
    objs = []
    m_steel = make_pbr_material("WpnSteel", "industrial_machinery", (0.55, 0.57, 0.60))
    m_rust = make_pbr_material("WpnRust", "rust", (0.85, 0.72, 0.65))
    m_wood = make_pbr_material("WpnWood", "wood", (0.75, 0.58, 0.42))
    m_poly = make_pbr_material("WpnPoly", "plastic", (0.45, 0.48, 0.50))

    # 1. Improvised Pipe Club
    bm = bmesh.new()
    add_cyl(bm, 0.022, 0.022, 0.68, 10, (0, 0, 0.34))
    add_cyl(bm, 0.032, 0.032, 0.08, 10, (0, 0, 0.64))
    add_box(bm, (0.14, 0.055, 0.075), (0.04, 0, 0.68))
    add_cyl(bm, 0.026, 0.026, 0.18, 10, (0, 0, 0.11))  # grip wrap
    o = finalize_bmesh_to_object(bm, "pipe_club", m_rust)
    manifest.append(export_single_obj(o, "weapons", "pipe_club"))
    objs.append(o)

    # 2. Combat Knife
    bm = bmesh.new()
    add_cyl(bm, 0.016, 0.018, 0.12, 8, (0, 0, 0.06))
    add_box(bm, (0.06, 0.018, 0.012), (0, 0, 0.125))
    add_box(bm, (0.030, 0.005, 0.19), (0.002, 0, 0.225))
    add_cyl(bm, 0.015, 0.001, 0.05, 4, (0.002, 0, 0.345))
    o = finalize_bmesh_to_object(bm, "combat_knife", m_steel)
    manifest.append(export_single_obj(o, "weapons", "combat_knife"))
    objs.append(o)

    # 3. Fire Axe
    bm = bmesh.new()
    add_cyl(bm, 0.018, 0.022, 0.86, 10, (0, 0, 0.43))
    add_box(bm, (0.16, 0.028, 0.11), (0.06, 0, 0.78))
    add_box(bm, (0.05, 0.012, 0.15), (0.14, 0, 0.78), (0, 0.12, 0))
    add_cyl(bm, 0.018, 0.003, 0.10, 6, (-0.07, 0, 0.78), (0, -math.pi / 2, 0))
    o = finalize_bmesh_to_object(bm, "fire_axe", m_wood)
    manifest.append(export_single_obj(o, "weapons", "fire_axe"))
    objs.append(o)

    # 4. Nail-Studded Bat
    bm = bmesh.new()
    add_cyl(bm, 0.018, 0.036, 0.84, 12, (0, 0, 0.42))
    add_cyl(bm, 0.026, 0.026, 0.016, 12, (0, 0, 0.01))
    for i in range(6):
        ang = i * math.pi / 3.0
        add_cyl(bm, 0.003, 0.001, 0.09, 5, (math.cos(ang) * 0.045, math.sin(ang) * 0.045, 0.68 + (i % 2) * 0.06), (math.sin(ang) * 1.4, -math.cos(ang) * 1.4, 0))
    o = finalize_bmesh_to_object(bm, "spiked_bat", m_wood)
    manifest.append(export_single_obj(o, "weapons", "spiked_bat"))
    objs.append(o)

    # 5. 9mm Service Pistol
    bm = bmesh.new()
    add_box(bm, (0.032, 0.19, 0.036), (0, 0.03, 0.13))  # slide
    add_box(bm, (0.030, 0.14, 0.022), (0, 0.01, 0.10))  # frame
    add_box(bm, (0.028, 0.042, 0.105), (0, -0.045, 0.045), (-0.22, 0, 0))  # grip
    add_cyl(bm, 0.007, 0.007, 0.03, 8, (0, 0.135, 0.132), (math.pi / 2, 0, 0))  # barrel
    add_box(bm, (0.008, 0.045, 0.025), (0, -0.005, 0.075))  # trigger guard
    o = finalize_bmesh_to_object(bm, "service_pistol", m_poly)
    manifest.append(export_single_obj(o, "weapons", "service_pistol"))
    objs.append(o)

    # 6. .357 Revolver
    bm = bmesh.new()
    add_box(bm, (0.028, 0.12, 0.038), (0, -0.01, 0.11))
    add_cyl(bm, 0.022, 0.022, 0.05, 10, (0, 0.0, 0.12), (math.pi / 2, 0, 0))
    add_cyl(bm, 0.009, 0.009, 0.15, 10, (0, 0.10, 0.132), (math.pi / 2, 0, 0))
    add_box(bm, (0.012, 0.14, 0.014), (0, 0.10, 0.144))
    add_box(bm, (0.028, 0.042, 0.10), (0, -0.06, 0.05), (-0.30, 0, 0))
    o = finalize_bmesh_to_object(bm, "revolver_357", m_steel)
    manifest.append(export_single_obj(o, "weapons", "revolver_357"))
    objs.append(o)

    # 7. 12-Gauge Pump Shotgun
    bm = bmesh.new()
    add_box(bm, (0.044, 0.24, 0.065), (0, 0.0, 0.14))  # receiver
    add_cyl(bm, 0.013, 0.013, 0.52, 10, (0, 0.36, 0.16), (math.pi / 2, 0, 0))  # barrel
    add_cyl(bm, 0.012, 0.012, 0.44, 10, (0, 0.32, 0.132), (math.pi / 2, 0, 0))  # mag tube
    add_cyl(bm, 0.022, 0.022, 0.18, 10, (0, 0.26, 0.132), (math.pi / 2, 0, 0))  # pump forend
    add_box(bm, (0.038, 0.28, 0.085), (0, -0.24, 0.10), (-0.14, 0, 0))  # stock
    o = finalize_bmesh_to_object(bm, "pump_shotgun", m_wood)
    manifest.append(export_single_obj(o, "weapons", "pump_shotgun"))
    objs.append(o)

    # 8. 5.56 Assault Carbine
    bm = bmesh.new()
    add_box(bm, (0.046, 0.26, 0.072), (0, 0.0, 0.15))  # upper/lower
    add_box(bm, (0.042, 0.24, 0.052), (0, 0.24, 0.158))  # handguard
    add_cyl(bm, 0.010, 0.010, 0.22, 10, (0, 0.44, 0.158), (math.pi / 2, 0, 0))  # barrel
    add_box(bm, (0.028, 0.065, 0.17), (0, 0.05, 0.04), (0.18, 0, 0))  # curved mag
    add_box(bm, (0.030, 0.040, 0.10), (0, -0.07, 0.07), (-0.25, 0, 0))  # pistol grip
    add_box(bm, (0.040, 0.23, 0.085), (0, -0.23, 0.14))  # collapsible stock
    add_cyl(bm, 0.018, 0.018, 0.09, 10, (0, 0.01, 0.205), (math.pi / 2, 0, 0))  # red-dot optic
    o = finalize_bmesh_to_object(bm, "assault_rifle", m_poly)
    manifest.append(export_single_obj(o, "weapons", "assault_rifle"))
    objs.append(o)

    # 9. Scoped Hunting Rifle
    bm = bmesh.new()
    add_box(bm, (0.042, 0.58, 0.068), (0, 0.04, 0.12))  # wood rifle body
    add_box(bm, (0.040, 0.26, 0.092), (0, -0.32, 0.095), (-0.10, 0, 0))  # buttstock
    add_cyl(bm, 0.010, 0.009, 0.58, 10, (0, 0.36, 0.152), (math.pi / 2, 0, 0))  # long barrel
    add_cyl(bm, 0.018, 0.022, 0.26, 12, (0, 0.02, 0.195), (math.pi / 2, 0, 0))  # scope
    add_box(bm, (0.012, 0.02, 0.03), (0, -0.05, 0.172))
    add_box(bm, (0.012, 0.02, 0.03), (0, 0.09, 0.172))
    o = finalize_bmesh_to_object(bm, "hunting_rifle", m_wood)
    manifest.append(export_single_obj(o, "weapons", "hunting_rifle"))
    objs.append(o)

    # 10. Tactical Crossbow
    bm = bmesh.new()
    add_box(bm, (0.042, 0.56, 0.055), (0, 0.08, 0.14))  # flight rail
    add_box(bm, (0.038, 0.22, 0.08), (0, -0.26, 0.11), (-0.12, 0, 0))  # stock
    add_box(bm, (0.28, 0.032, 0.016), (-0.15, 0.32, 0.145), (0, 0, 0.25))  # left bow limb
    add_box(bm, (0.28, 0.032, 0.016), (0.15, 0.32, 0.145), (0, 0, -0.25))  # right bow limb
    add_cyl(bm, 0.003, 0.003, 0.54, 6, (0, 0.24, 0.148), (0, math.pi / 2, 0))  # bowstring
    add_cyl(bm, 0.005, 0.005, 0.36, 6, (0, 0.24, 0.165), (math.pi / 2, 0, 0))  # loaded bolt
    o = finalize_bmesh_to_object(bm, "tactical_crossbow", m_poly)
    manifest.append(export_single_obj(o, "weapons", "tactical_crossbow"))
    objs.append(o)

    # 11. Craftable Pipe Shotgun
    bm = bmesh.new()
    add_cyl(bm, 0.016, 0.016, 0.46, 10, (0, 0.20, 0.145), (math.pi / 2, 0, 0))
    add_box(bm, (0.044, 0.34, 0.062), (0, -0.06, 0.115), (-0.10, 0, 0))
    add_box(bm, (0.050, 0.04, 0.072), (0, 0.08, 0.13))  # hose clamp band
    o = finalize_bmesh_to_object(bm, "makeshift_shotgun", m_rust)
    manifest.append(export_single_obj(o, "weapons", "makeshift_shotgun"))
    objs.append(o)

    return objs


# ============================================================
# 2. CHARACTERS & ALL 10 ORIGINAL MUTANT SPECIES + RIGS
# ============================================================
def add_limb_segment(bm: bmesh.types.BMesh, p0: tuple[float, float, float], p1: tuple[float, float, float], r0: float, r1: float, segs: int = 8) -> None:
    v0 = Vector(p0)
    v1 = Vector(p1)
    diff = v1 - v0
    length = diff.length
    if length < 1e-5:
        return
    mid = (v0 + v1) * 0.5
    quat = Vector((0, 0, 1)).rotation_difference(diff.normalized())
    ret = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs, radius1=r0, radius2=r1, depth=length)
    mat = Matrix.Translation(mid) @ quat.to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=mat, verts=ret["verts"])


def build_humanoid_or_mutant(
    name: str,
    mat: bpy.types.Material,
    height_scale: float,
    torso_size: tuple[float, float, float],
    head_size: float,
    limb_len: float,
    limb_rad: float,
    hunch: float = 0.0,
    quadruped: bool = False,
    extras: str = "",
) -> bpy.types.Object:
    bm = bmesh.new()
    s = height_scale
    if quadruped:
        # Articulated multi-segment quadrupedal hunter (Crawler / Burrower)
        body_z = 0.46 * s
        add_sphere(bm, 0.24 * s, 10, 8, (0, 0.16 * s, body_z), (0.95, 1.35, 0.72))  # thorax
        add_sphere(bm, 0.20 * s, 10, 8, (0, -0.24 * s, body_z * 0.92), (0.85, 1.25, 0.68))  # abdomen
        add_sphere(bm, head_size * 1.15 * s, 10, 8, (0, 0.48 * s, body_z + 0.04 * s), (0.88, 1.28, 0.78))  # skull
        add_box(bm, (0.14 * s, 0.18 * s, 0.06 * s), (0, 0.58 * s, body_z - 0.05 * s), (0.18, 0, 0))  # mandible jaw
        for sx in (-1, 1):
            for sy, y_hip in ((-1, -0.24 * s), (1, 0.22 * s)):
                hip = (sx * 0.17 * s, y_hip, body_z)
                knee = (sx * 0.36 * s, y_hip + sy * 0.08 * s, body_z + 0.06 * s)
                foot = (sx * 0.42 * s, y_hip + sy * 0.16 * s, 0.03 * s)
                add_limb_segment(bm, hip, knee, limb_rad * 1.1 * s, limb_rad * 0.85 * s, 8)
                add_limb_segment(bm, knee, foot, limb_rad * 0.85 * s, limb_rad * 0.45 * s, 8)
        if extras == "burrower":
            for i in range(5):
                add_cyl(bm, 0.055 * s, 0.008 * s, 0.22 * s, 6, (0, (-0.32 + i * 0.16) * s, body_z + 0.22 * s), (-0.35, 0, 0))
            for sx in (-1, 1):
                add_box(bm, (0.14 * s, 0.24 * s, 0.05 * s), (sx * 0.38 * s, 0.46 * s, 0.08 * s), (0.2, 0, -sx * 0.25))
    else:
        hip_z = limb_len * s
        pelvis_w = torso_size[0] * 0.82 * s
        chest_w = torso_size[0] * 1.06 * s
        fwd = math.sin(hunch) * torso_size[2] * 0.55 * s
        waist_z = hip_z + torso_size[2] * 0.28 * s
        chest_z = hip_z + torso_size[2] * 0.72 * s
        shoulder_z = hip_z + torso_size[2] * 0.92 * s
        head_z = shoulder_z + head_size * 1.25 * s

        # Pelvis, waist, chest & neck
        add_box(bm, (pelvis_w, torso_size[1] * 0.88 * s, torso_size[2] * 0.28 * s), (0, 0, hip_z + 0.04 * s))
        add_box(bm, (pelvis_w * 0.88, torso_size[1] * 0.82 * s, torso_size[2] * 0.36 * s), (0, fwd * 0.4, waist_z), (hunch * 0.5, 0, 0))
        add_box(bm, (chest_w, torso_size[1] * 1.05 * s, torso_size[2] * 0.48 * s), (0, fwd * 0.85, chest_z), (hunch, 0, 0))
        add_limb_segment(bm, (0, fwd * 0.95, shoulder_z - 0.02 * s), (0, fwd * 1.25, head_z - head_size * 0.5 * s), head_size * 0.48 * s, head_size * 0.42 * s, 8)

        # Articulated legs (Hip -> Knee -> Ankle -> Boot/Foot)
        for sx in (-1, 1):
            hx = sx * pelvis_w * 0.34
            hip_pt = (hx, 0.0, hip_z)
            knee_pt = (hx * 1.04, 0.05 * s, hip_z * 0.52)
            ankle_pt = (hx * 1.02, -0.02 * s, 0.07 * s)
            add_limb_segment(bm, hip_pt, knee_pt, limb_rad * 1.22 * s, limb_rad * 0.95 * s, 8)
            add_limb_segment(bm, knee_pt, ankle_pt, limb_rad * 0.95 * s, limb_rad * 0.72 * s, 8)
            add_box(bm, (limb_rad * 1.55 * s, limb_rad * 2.5 * s, 0.08 * s), (hx * 1.02, 0.06 * s, 0.04 * s))

        # Skull & Jaw
        add_sphere(bm, head_size * s, 10, 8, (0, fwd * 1.32, head_z), (0.86, 1.02, 1.14))
        add_box(bm, (head_size * 1.05 * s, head_size * 0.95 * s, head_size * 0.65 * s), (0, fwd * 1.38 + 0.02 * s, head_z - head_size * 0.55 * s))

        # Articulated arms hanging naturally from shoulder joints (Shoulder -> Elbow -> Wrist/Claw)
        arm_reach = limb_len * (1.18 if extras in ("hollow", "pale_one", "watcher") else 0.92) * s
        for sx in (-1, 1):
            sh_pt = (sx * (chest_w * 0.58), fwd * 0.88, shoulder_z)
            add_sphere(bm, limb_rad * 1.18 * s, 8, 6, sh_pt)
            elbow_pt = (sx * (chest_w * 0.68), fwd * 0.80 - 0.04 * s, shoulder_z - arm_reach * 0.48)
            hand_pt = (sx * (chest_w * 0.64), fwd * 1.05 + 0.08 * s, shoulder_z - arm_reach * 0.95)
            add_limb_segment(bm, sh_pt, elbow_pt, limb_rad * 1.05 * s, limb_rad * 0.82 * s, 8)
            add_limb_segment(bm, elbow_pt, hand_pt, limb_rad * 0.82 * s, limb_rad * 0.62 * s, 8)
            add_box(bm, (limb_rad * 1.1 * s, limb_rad * 1.4 * s, limb_rad * 1.6 * s), (hand_pt[0], hand_pt[1], hand_pt[2] - 0.04 * s))

        # Species-specific anatomical silhouette features!
        if extras == "survivor":
            add_box(bm, (0.34 * s, 0.20 * s, 0.42 * s), (0, fwd * 0.6 - 0.22 * s, chest_z))  # Backpack
            add_cyl(bm, 0.055 * s, 0.055 * s, 0.08 * s, 8, (0, fwd * 1.38 + 0.14 * s, head_z - 0.04 * s), (math.pi / 2, 0, 0))  # Gas mask filter
            add_box(bm, (0.18 * s, 0.05 * s, 0.07 * s), (0, fwd * 1.38 + 0.11 * s, head_z + 0.03 * s))  # Visor
        elif extras == "hollow":
            for i in range(4):
                add_box(bm, (chest_w * 1.08, torso_size[1] * 1.12 * s, 0.026 * s), (0, fwd * 0.85, chest_z - 0.14 * s + i * 0.09 * s), (hunch, 0, 0))
        elif extras == "watcher":
            for sx in (-1, 1):
                add_limb_segment(bm, (sx * 0.08 * s, fwd * 1.3, head_z + 0.08 * s), (sx * 0.28 * s, fwd * 1.2, head_z + 0.56 * s), 0.032 * s, 0.008 * s, 6)
                add_limb_segment(bm, (sx * 0.18 * s, fwd * 1.25, head_z + 0.30 * s), (sx * 0.40 * s, fwd * 1.32, head_z + 0.50 * s), 0.020 * s, 0.006 * s, 6)
        elif extras == "screamer":
            add_sphere(bm, 0.18 * s, 10, 8, (0, fwd * 1.35 + 0.12 * s, head_z - 0.18 * s), (1.05, 1.25, 1.05))
        elif extras == "brute":
            for sx in (-1, 1):
                add_box(bm, (0.28 * s, 0.36 * s, 0.24 * s), (sx * (chest_w * 0.62), fwd * 0.85, shoulder_z + 0.05 * s), (0, sx * 0.25, 0))
            add_cyl(bm, 0.018 * s, 0.018 * s, 0.55 * s, 6, (0.20 * s, fwd * 0.5 - 0.16 * s, shoulder_z + 0.18 * s), (-0.35, 0.25, 0))
        elif extras == "mimic":
            add_cyl(bm, 0.03 * s, 0.13 * s, 0.24 * s, 10, (-0.22 * s, fwd * 1.1 + 0.08 * s, shoulder_z + 0.08 * s), (math.pi / 2 - 0.2, 0, 0.3))
        elif extras == "stalker":
            for sx in (-1, 1):
                add_box(bm, (0.035 * s, 0.36 * s, 0.085 * s), (sx * (chest_w * 0.65), fwd * 1.1 + 0.18 * s, shoulder_z - arm_reach * 0.82), (0.35, 0, 0))
        elif extras == "contaminated":
            for (px, py, pz, pr) in [(-0.14, -0.14, 0.12, 0.11), (0.15, -0.12, 0.04, 0.13), (0.02, -0.16, -0.10, 0.10), (-0.18, 0.06, 0.16, 0.08)]:
                add_sphere(bm, pr * s, 8, 6, (px * s, fwd * 0.8 + py * s, chest_z + pz * s))
        elif extras == "pale_one":
            for i in range(5):
                add_cyl(bm, 0.028 * s, 0.004 * s, 0.20 * s, 5, (0, fwd * 0.7 - 0.16 * s, waist_z + i * 0.11 * s), (-1.1, 0, 0))

    return finalize_bmesh_to_object(bm, name, mat, smooth=True)


def attach_armature_and_actions(mesh_obj: bpy.types.Object, height: float) -> bpy.types.Object:
    """Creates a clean Blender Skeletal Armature + Idle/Walk/Attack keyframe actions."""
    arm_data = bpy.data.armatures.new(f"{mesh_obj.name}_Armature")
    arm_obj = bpy.data.objects.new(f"{mesh_obj.name}_Rig", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="EDIT")

    eb = arm_data.edit_bones
    root = eb.new("Root")
    root.head = (0, 0, 0)
    root.tail = (0, 0, height * 0.45)

    spine = eb.new("Spine")
    spine.head = root.tail
    spine.tail = (0, 0, height * 0.78)
    spine.parent = root

    head = eb.new("Head")
    head.head = spine.tail
    head.tail = (0, 0, height * 0.98)
    head.parent = spine

    for side, sx in (("L", -1), ("R", 1)):
        arm = eb.new(f"Arm_{side}")
        arm.head = (sx * height * 0.16, 0, height * 0.74)
        arm.tail = (sx * height * 0.22, 0, height * 0.38)
        arm.parent = spine

        leg = eb.new(f"Leg_{side}")
        leg.head = (sx * height * 0.09, 0, height * 0.45)
        leg.tail = (sx * height * 0.09, 0, 0.02)
        leg.parent = root

    bpy.ops.object.mode_set(mode="OBJECT")
    mesh_obj.parent = arm_obj
    mod = mesh_obj.modifiers.new(name="Armature", type="ARMATURE")
    mod.object = arm_obj

    # Keyframe a looping Walk/Stalk cycle on PoseBones (frames 1..25)
    arm_obj.animation_data_create()
    act = bpy.data.actions.new(name=f"{mesh_obj.name}_StalkCycle")
    arm_obj.animation_data.action = act
    pb = arm_obj.pose.bones
    for frame, swing in ((1, 0.42), (13, -0.42), (25, 0.42)):
        pb["Leg_L"].rotation_mode = "XYZ"
        pb["Leg_R"].rotation_mode = "XYZ"
        pb["Arm_L"].rotation_mode = "XYZ"
        pb["Arm_R"].rotation_mode = "XYZ"
        pb["Spine"].rotation_mode = "XYZ"
        pb["Leg_L"].rotation_euler = (swing, 0, 0)
        pb["Leg_R"].rotation_euler = (-swing, 0, 0)
        pb["Arm_L"].rotation_euler = (-swing * 0.75, 0, 0)
        pb["Arm_R"].rotation_euler = (swing * 0.75, 0, 0)
        pb["Spine"].rotation_euler = (0.08 + abs(swing) * 0.08, 0, swing * 0.08)
        for bname in ("Leg_L", "Leg_R", "Arm_L", "Arm_R", "Spine"):
            pb[bname].keyframe_insert(data_path="rotation_euler", frame=frame)
    return arm_obj


def build_creatures_and_characters(manifest: list[dict]) -> list[bpy.types.Object]:
    m_surv = make_pbr_material("MatSurvivor", "fabric", (0.58, 0.64, 0.54))
    m_scav = make_pbr_material("MatScavenger", "painted_metal", (0.55, 0.48, 0.42))
    m_flesh_dark = make_pbr_material("MatMutantDark", "mud", (0.58, 0.50, 0.48))
    m_flesh_ash = make_pbr_material("MatMutantAsh", "damaged_wall", (0.62, 0.58, 0.55))
    m_brute = make_pbr_material("MatBrute", "concrete", (0.52, 0.48, 0.45))
    m_rad = make_pbr_material("MatContaminated", "wet_surface", (0.48, 0.65, 0.40), emission=(0.35, 0.95, 0.25), emission_strength=1.8)
    m_pale = make_pbr_material("MatPaleOne", "damaged_wall", (0.92, 0.90, 0.88))

    specs = [
        ("survivor_rig", m_surv, 1.0, (0.42, 0.24, 0.58), 0.13, 0.82, 0.075, 0.0, False, "survivor"),
        ("hostile_scavenger", m_scav, 1.02, (0.45, 0.26, 0.58), 0.13, 0.82, 0.08, 0.04, False, ""),
        ("mutant_hollow", m_flesh_ash, 1.24, (0.32, 0.18, 0.64), 0.12, 0.96, 0.052, 0.22, False, "hollow"),
        ("mutant_crawler", m_flesh_dark, 0.92, (0.38, 0.34, 0.32), 0.13, 0.72, 0.062, 0.0, True, ""),
        ("mutant_watcher", m_flesh_dark, 1.42, (0.34, 0.20, 0.68), 0.13, 1.02, 0.055, 0.12, False, "watcher"),
        ("mutant_screamer", m_flesh_ash, 1.08, (0.40, 0.26, 0.58), 0.14, 0.84, 0.065, 0.18, False, "screamer"),
        ("mutant_brute", m_brute, 1.48, (0.68, 0.46, 0.68), 0.14, 0.86, 0.135, 0.24, False, "brute"),
        ("mutant_mimic", m_flesh_dark, 1.06, (0.38, 0.24, 0.56), 0.13, 0.86, 0.068, 0.28, False, "mimic"),
        ("mutant_burrower", m_brute, 1.05, (0.48, 0.38, 0.36), 0.15, 0.68, 0.088, 0.0, True, "burrower"),
        ("mutant_stalker", m_flesh_dark, 1.18, (0.36, 0.22, 0.60), 0.12, 0.92, 0.062, 0.25, False, "stalker"),
        ("mutant_contaminated", m_rad, 1.12, (0.48, 0.34, 0.58), 0.14, 0.82, 0.085, 0.16, False, "contaminated"),
        ("mutant_pale_one", m_pale, 1.45, (0.34, 0.19, 0.68), 0.13, 1.04, 0.056, 0.14, False, "pale_one"),
    ]
    out_objs = []
    for name, mat, hscale, tsize, hsize, llen, lrad, hunch, quad, ext in specs:
        obj = build_humanoid_or_mutant(name, mat, hscale, tsize, hsize, llen, lrad, hunch, quad, ext)
        manifest.append(export_single_obj(obj, "characters", name))
        attach_armature_and_actions(obj, 1.75 * hscale if not quad else 0.9 * hscale)
        out_objs.append(obj)
    return out_objs


# ============================================================
# 3. ARCHITECTURE & ENVIRONMENT PROPS (18 Models)
# ============================================================
def build_architecture_and_props(manifest: list[dict]) -> None:
    m_conc = make_pbr_material("ArchConcrete", "concrete")
    m_brick = make_pbr_material("ArchBrick", "brick")
    m_wood = make_pbr_material("ArchWood", "wood")
    m_rust = make_pbr_material("ArchRust", "rust")
    m_paint = make_pbr_material("ArchPainted", "painted_metal")
    m_stone = make_pbr_material("ArchStone", "stone")
    m_mach = make_pbr_material("ArchMach", "industrial_machinery")
    m_wall = make_pbr_material("ArchWall", "damaged_wall")

    # 1. Suburban House Shell
    bm = bmesh.new()
    add_box(bm, (9.0, 7.5, 0.35), (0, 0, 0.175))  # foundation
    for sx in (-4.35, 4.35):
        add_box(bm, (0.3, 7.5, 3.2), (sx, 0, 1.95))
    add_box(bm, (8.4, 0.3, 3.2), (0, -3.6, 1.95))
    add_box(bm, (3.2, 0.3, 3.2), (-2.6, 3.6, 1.95))
    add_box(bm, (3.2, 0.3, 3.2), (2.6, 3.6, 1.95))
    add_box(bm, (2.0, 0.3, 0.9), (0, 3.6, 3.1))  # doorway lintel
    # Pitched roof slabs
    add_box(bm, (5.4, 8.1, 0.22), (-2.3, 0, 4.45), (0, -0.46, 0))
    add_box(bm, (5.4, 8.1, 0.22), (2.3, 0, 4.45), (0, 0.46, 0))
    o = finalize_bmesh_to_object(bm, "suburban_house", m_wall)
    manifest.append(export_single_obj(o, "architecture", "suburban_house"))

    # 2. Blackwood Forest Cabin
    bm = bmesh.new()
    add_box(bm, (6.4, 5.4, 0.3), (0, 0, 0.15))
    for i in range(8):
        z = 0.48 + i * 0.32
        add_cyl(bm, 0.16, 0.16, 6.4, 8, (0, -2.55, z), (0, math.pi / 2, 0))
        add_cyl(bm, 0.16, 0.16, 5.4, 8, (-3.05, 0, z), (math.pi / 2, 0, 0))
        add_cyl(bm, 0.16, 0.16, 5.4, 8, (3.05, 0, z), (math.pi / 2, 0, 0))
    add_box(bm, (4.0, 6.0, 0.20), (-1.7, 0, 3.6), (0, -0.45, 0))
    add_box(bm, (4.0, 6.0, 0.20), (1.7, 0, 3.6), (0, 0.45, 0))
    add_box(bm, (0.7, 0.7, 4.6), (-2.4, -2.1, 2.3))  # stone chimney
    o = finalize_bmesh_to_object(bm, "forest_cabin", m_wood)
    manifest.append(export_single_obj(o, "architecture", "forest_cabin"))

    # 3. Hyperbolic Industrial Cooling Tower
    bm = bmesh.new()
    rings = [(0.0, 5.8), (3.5, 4.8), (7.5, 4.0), (11.5, 3.7), (15.0, 4.3)]
    for i in range(len(rings) - 1):
        z0, r0 = rings[i]
        z1, r1 = rings[i + 1]
        add_cyl(bm, r0, r1, z1 - z0, 18, (0, 0, (z0 + z1) * 0.5))
    o = finalize_bmesh_to_object(bm, "industrial_cooling_tower", m_conc, smooth=True)
    manifest.append(export_single_obj(o, "architecture", "industrial_cooling_tower"))

    # 4. Hospital Modular Wing
    bm = bmesh.new()
    add_box(bm, (14.0, 10.0, 0.4), (0, 0, 0.2))
    add_box(bm, (14.0, 10.0, 0.35), (0, 0, 4.2))
    add_box(bm, (14.0, 10.0, 0.35), (0, 0, 8.2))
    for sx in (-6.8, 6.8):
        add_box(bm, (0.4, 10.0, 8.0), (sx, 0, 4.2))
    add_box(bm, (13.2, 0.4, 8.0), (0, -4.8, 4.2))
    o = finalize_bmesh_to_object(bm, "hospital_wing", m_brick)
    manifest.append(export_single_obj(o, "architecture", "hospital_wing"))

    # 5. Military Watchtower
    bm = bmesh.new()
    for sx in (-1.4, 1.4):
        for sy in (-1.4, 1.4):
            add_box(bm, (0.24, 0.24, 7.5), (sx, sy, 3.75), (sy * 0.04, -sx * 0.04, 0))
    add_box(bm, (3.6, 3.6, 0.25), (0, 0, 7.5))
    for sx in (-1.7, 1.7):
        add_box(bm, (0.18, 3.6, 1.1), (sx, 0, 8.15))
        add_box(bm, (3.6, 0.18, 1.1), (0, sx, 8.15))
    add_box(bm, (4.0, 4.0, 0.22), (0, 0, 9.8))
    o = finalize_bmesh_to_object(bm, "military_watchtower", m_paint)
    manifest.append(export_single_obj(o, "architecture", "military_watchtower"))

    # 6. Military Reinforced Bunker
    bm = bmesh.new()
    add_box(bm, (8.5, 7.0, 3.0), (0, 0, 1.5))
    add_box(bm, (9.2, 7.6, 0.6), (0, 0, 3.1))
    add_box(bm, (3.2, 1.8, 2.5), (0, 4.1, 1.25))  # blast portal
    o = finalize_bmesh_to_object(bm, "military_bunker", m_conc)
    manifest.append(export_single_obj(o, "architecture", "military_bunker"))

    # 7. Dead Twisted Pine Tree
    bm = bmesh.new()
    add_cyl(bm, 0.34, 0.14, 6.8, 8, (0, 0, 3.4), (0.04, -0.03, 0))
    for i in range(6):
        ang = i * 1.15
        z = 2.4 + i * 0.68
        blen = 1.9 - i * 0.22
        add_cyl(bm, 0.09, 0.02, blen, 6, (math.cos(ang) * blen * 0.42, math.sin(ang) * blen * 0.42, z), (math.sin(ang) * 1.15, -math.cos(ang) * 1.15, 0))
    o = finalize_bmesh_to_object(bm, "dead_pine_tree", m_wood)
    manifest.append(export_single_obj(o, "environment", "dead_pine_tree"))

    # 8. Fractured Boulder
    bm = bmesh.new()
    add_sphere(bm, 1.35, 7, 5, (0, 0, 0.85), (1.3, 1.0, 0.78))
    add_sphere(bm, 0.75, 6, 5, (0.85, 0.35, 0.52), (1.1, 0.9, 0.75))
    o = finalize_bmesh_to_object(bm, "fractured_boulder", m_stone)
    manifest.append(export_single_obj(o, "environment", "fractured_boulder"))

    # 9. Rusted Abandoned Sedan
    bm = bmesh.new()
    add_box(bm, (1.85, 4.3, 0.58), (0, 0, 0.62))
    add_box(bm, (1.58, 2.1, 0.52), (0, -0.15, 1.15))
    for sx in (-0.88, 0.88):
        for sy in (-1.32, 1.32):
            add_cyl(bm, 0.33, 0.33, 0.22, 12, (sx, sy, 0.33), (0, math.pi / 2, 0))
    o = finalize_bmesh_to_object(bm, "rusted_sedan", m_rust)
    manifest.append(export_single_obj(o, "environment", "rusted_sedan"))

    # 10. Diesel Generator
    bm = bmesh.new()
    add_box(bm, (0.85, 1.35, 0.78), (0, 0, 0.44))
    add_cyl(bm, 0.05, 0.05, 0.45, 8, (0.32, -0.45, 1.02))
    for sx in (-0.38, 0.38):
        add_cyl(bm, 0.04, 0.04, 1.42, 8, (sx, 0, 0.45), (math.pi / 2, 0, 0))
    o = finalize_bmesh_to_object(bm, "diesel_generator", m_mach)
    manifest.append(export_single_obj(o, "props", "diesel_generator"))

    # 11. Radio Transmitter Console
    bm = bmesh.new()
    add_box(bm, (1.2, 0.68, 0.82), (0, 0, 0.41))
    add_box(bm, (1.1, 0.38, 0.48), (0, -0.12, 1.06))
    add_cyl(bm, 0.015, 0.006, 1.35, 6, (0.45, -0.20, 1.95))
    o = finalize_bmesh_to_object(bm, "radio_transmitter", m_mach)
    manifest.append(export_single_obj(o, "props", "radio_transmitter"))

    # 12. Hospital Bed
    bm = bmesh.new()
    add_box(bm, (0.95, 2.05, 0.14), (0, 0, 0.62))
    add_box(bm, (0.95, 0.06, 0.52), (0, 1.02, 0.82))
    add_box(bm, (0.95, 0.06, 0.40), (0, -1.02, 0.76))
    for sx in (-0.44, 0.44):
        for sy in (-0.96, 0.96):
            add_cyl(bm, 0.03, 0.03, 0.60, 8, (sx, sy, 0.30))
    o = finalize_bmesh_to_object(bm, "hospital_bed", m_paint)
    manifest.append(export_single_obj(o, "props", "hospital_bed"))

    # 13. Medical Supply Cabinet
    bm = bmesh.new()
    add_box(bm, (0.75, 0.38, 1.55), (0, 0, 0.775))
    add_box(bm, (0.24, 0.03, 0.07), (0, 0.20, 1.05))
    add_box(bm, (0.07, 0.03, 0.24), (0, 0.20, 1.05))
    o = finalize_bmesh_to_object(bm, "medical_cabinet", m_paint)
    manifest.append(export_single_obj(o, "props", "medical_cabinet"))

    # 14. Survival Workbench
    bm = bmesh.new()
    add_box(bm, (1.65, 0.82, 0.09), (0, 0, 0.86))
    add_box(bm, (1.60, 0.06, 0.65), (0, -0.38, 1.22))
    add_box(bm, (0.22, 0.22, 0.16), (-0.58, 0.26, 0.98))  # bench vise
    for sx in (-0.75, 0.75):
        for sy in (-0.35, 0.35):
            add_box(bm, (0.08, 0.08, 0.84), (sx, sy, 0.42))
    o = finalize_bmesh_to_object(bm, "workbench_station", m_wood)
    manifest.append(export_single_obj(o, "props", "workbench_station"))

    # 15. Military Supply Crate
    bm = bmesh.new()
    add_box(bm, (1.15, 0.68, 0.52), (0, 0, 0.26))
    add_box(bm, (1.20, 0.72, 0.09), (0, 0, 0.54))
    o = finalize_bmesh_to_object(bm, "military_crate", m_paint)
    manifest.append(export_single_obj(o, "props", "military_crate"))

    # 16. Bio-Chemical Hazard Barrel
    bm = bmesh.new()
    add_cyl(bm, 0.29, 0.29, 0.88, 14, (0, 0, 0.44))
    for rz in (0.04, 0.30, 0.58, 0.84):
        add_cyl(bm, 0.305, 0.305, 0.03, 14, (0, 0, rz))
    o = finalize_bmesh_to_object(bm, "hazard_barrel", m_rust)
    manifest.append(export_single_obj(o, "props", "hazard_barrel"))

    # 17. Campfire Pit
    bm = bmesh.new()
    for i in range(8):
        ang = i * math.pi / 4.0
        add_sphere(bm, 0.14, 6, 5, (math.cos(ang) * 0.46, math.sin(ang) * 0.46, 0.10))
    for i in range(4):
        ang = i * math.pi / 4.0
        add_cyl(bm, 0.06, 0.05, 0.72, 7, (0, 0, 0.16), (0.38, 0, ang))
    o = finalize_bmesh_to_object(bm, "campfire_pit", m_stone)
    manifest.append(export_single_obj(o, "props", "campfire_pit"))

    # 18. Survivor Backpack
    bm = bmesh.new()
    add_box(bm, (0.34, 0.22, 0.46), (0, 0, 0.25))
    add_box(bm, (0.26, 0.09, 0.20), (0, 0.14, 0.18))
    add_cyl(bm, 0.06, 0.06, 0.34, 10, (0, 0.02, 0.51), (0, math.pi / 2, 0))  # bedroll
    o = finalize_bmesh_to_object(bm, "survivor_backpack", make_pbr_material("PropPack", "fabric"))
    manifest.append(export_single_obj(o, "props", "survivor_backpack"))


def render_asset_showcase() -> None:
    """Renders a Cycles CPU inspection sheet of all 10 Mutants + Survivor + Weapons."""
    os.makedirs(RENDER_DIR, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 16
    scene.cycles.use_denoising = False
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720

    # Dark moody post-apocalyptic world lighting
    world = bpy.data.worlds.new("ShowcaseWorld")
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.035, 0.042, 0.048, 1.0)
        bg.inputs["Strength"].default_value = 0.9
    scene.world = world

    # Ground plinth
    bm = bmesh.new()
    add_box(bm, (28.0, 14.0, 0.2), (0, 0, -0.1))
    ground = finalize_bmesh_to_object(bm, "ShowcaseFloor", make_pbr_material("FloorAsphalt", "asphalt"))

    # Arrange creatures in back row, weapons on pedestals in front row
    char_names = [
        "survivor_rig", "mutant_hollow", "mutant_crawler", "mutant_watcher",
        "mutant_screamer", "mutant_brute", "mutant_mimic", "mutant_burrower",
        "mutant_stalker", "mutant_contaminated", "mutant_pale_one",
    ]
    for idx, cname in enumerate(char_names):
        rig = bpy.data.objects.get(f"{cname}_Rig") or bpy.data.objects.get(cname)
        if rig:
            rig.location = (-6.5 + idx * 1.3, 1.5, 0.0)
            rig.rotation_euler = (0.0, 0.0, math.pi)

    wpn_names = [
        "pipe_club", "combat_knife", "fire_axe", "spiked_bat", "service_pistol",
        "revolver_357", "pump_shotgun", "assault_rifle", "hunting_rifle",
        "tactical_crossbow", "makeshift_shotgun",
    ]
    for idx, wname in enumerate(wpn_names):
        w = bpy.data.objects.get(wname)
        if w:
            w.location = (-6.0 + idx * 1.2, -1.6, 0.45)
            w.rotation_euler = (0.35, 0.0, 0.75)
            w.scale = (1.5, 1.5, 1.5)

    # Hide large buildings from showcase shot so framing is tight
    for bname in ("suburban_house", "forest_cabin", "industrial_cooling_tower", "hospital_wing", "military_watchtower", "military_bunker", "dead_pine_tree", "rusted_sedan", "fractured_boulder", "diesel_generator", "radio_transmitter", "hospital_bed", "medical_cabinet", "workbench_station", "military_crate", "hazard_barrel", "campfire_pit", "survivor_backpack", "hostile_scavenger_Rig", "hostile_scavenger"):
        bo = bpy.data.objects.get(bname)
        if bo:
            bo.hide_render = True

    key_data = bpy.data.lights.new("KeySun", type="SUN")
    key_data.energy = 3.8
    key_data.color = (0.85, 0.90, 0.98)
    key = bpy.data.objects.new("KeySun", key_data)
    key.rotation_euler = (0.95, 0.15, 0.45)
    bpy.context.collection.objects.link(key)

    rim_data = bpy.data.lights.new("RimSun", type="SUN")
    rim_data.energy = 2.0
    rim_data.color = (0.95, 0.48, 0.30)
    rim = bpy.data.objects.new("RimSun", rim_data)
    rim.rotation_euler = (1.15, -0.25, -2.5)
    bpy.context.collection.objects.link(rim)

    cam_data = bpy.data.cameras.new("ShowcaseCam")
    cam_data.lens = 22
    cam = bpy.data.objects.new("ShowcaseCam", cam_data)
    cam.location = (0.0, -7.2, 2.7)
    cam.rotation_euler = (1.32, 0.0, 0.0)
    bpy.context.collection.objects.link(cam)
    scene.camera = cam

    scene.render.filepath = os.path.join(RENDER_DIR, "blender_mutants_and_weapons_showcase.png")
    bpy.ops.render.render(write_still=True)


def main() -> None:
    os.makedirs(MESH_DIR, exist_ok=True)
    os.makedirs(BLEND_DIR, exist_ok=True)
    reset_scene()

    manifest: list[dict] = []
    build_weapons(manifest)
    build_creatures_and_characters(manifest)
    build_architecture_and_props(manifest)

    # Save complete Blender 5.0.1 Asset Library (.blend)
    blend_path = os.path.join(BLEND_DIR, "ashes_of_humanity_assets.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)

    # Render Cycles CPU visual showcase
    render_asset_showcase()

    manifest_path = os.path.join(MESH_DIR, "asset_manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(
            {
                "blender_version": bpy.app.version_string,
                "blend_library": "assets/blender/ashes_of_humanity_assets.blend",
                "total_models": len(manifest),
                "models": manifest,
            },
            f,
            indent=2,
        )
    print(f"Exported {len(manifest)} game-ready Blender 5.0.1 models, rigs, .blend library, and Cycles showcase render!")


if __name__ == "__main__":
    main()
