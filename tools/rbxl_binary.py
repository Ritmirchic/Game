#!/usr/bin/env python3
"""
ASHES OF HUMANITY: LAST SIGNAL
Spec-compliant Roblox Studio Binary Place (.rbxl) & Model (.rbxm) Compiler & Verifier.

Implements Roblox Binary Model Format v0 using the official Rojo/rbx-dom
Reflection Database (tools/rbx_reflection_db.msgpack) for 100% accurate:
- Class inheritance & Service flags (INST chunk)
- Canonical -> Serialized property aliasing (Size -> size, Color -> Color3uint8, Shape -> shape, Attributes -> AttributesSerialize, etc.)
- Official Roblox Enum integer values (e.g. Material.Asphalt = 1376, Concrete = 816, Lighting.Technology, etc.)
- Interleaved zigzag i32/i64, rotated-sign-bit f32, delta-encoded Referents, CFrame matrices, NumberSequence, ColorSequence, UDim2, Font, PhysicalProperties, AttributesSerialize, and CollectionService Tags
- Raw LZ4 block chunk framing + END\\0 '</roblox>' trailer
"""

from __future__ import annotations
import math
import os
import struct
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import lz4.block
import msgpack

FILE_MAGIC_HEADER = b"<roblox!"
FILE_SIGNATURE = b"\x89\xff\x0d\x0a\x1a\x0a"
FILE_VERSION = 0
FILE_FOOTER = b"</roblox>"

# Binary Type IDs (matching rbx_binary::types::Type)
TYPE_STRING = 0x01
TYPE_BOOL = 0x02
TYPE_INT32 = 0x03
TYPE_FLOAT32 = 0x04
TYPE_FLOAT64 = 0x05
TYPE_UDIM = 0x06
TYPE_UDIM2 = 0x07
TYPE_RAY = 0x08
TYPE_FACES = 0x09
TYPE_AXES = 0x0A
TYPE_BRICKCOLOR = 0x0B
TYPE_COLOR3 = 0x0C
TYPE_VECTOR2 = 0x0D
TYPE_VECTOR3 = 0x0E
TYPE_CFRAME = 0x10
TYPE_ENUM = 0x12
TYPE_REF = 0x13
TYPE_VECTOR3INT16 = 0x14
TYPE_NUMBERSEQUENCE = 0x15
TYPE_COLORSEQUENCE = 0x16
TYPE_NUMBERRANGE = 0x17
TYPE_RECT = 0x18
TYPE_PHYSICALPROPERTIES = 0x19
TYPE_COLOR3UINT8 = 0x1A
TYPE_INT64 = 0x1B
TYPE_SHAREDSTRING = 0x1C
TYPE_OPTIONALCFRAME = 0x1E
TYPE_UNIQUEID = 0x1F
TYPE_FONT = 0x20
TYPE_SECURITYCAPABILITIES = 0x21
TYPE_CONTENT = 0x22

VARIANT_TO_BINARY_TYPE = {
    "String": TYPE_STRING,
    "BinaryString": TYPE_STRING,
    "ContentId": TYPE_STRING,
    "Tags": TYPE_STRING,
    "Attributes": TYPE_STRING,
    "MaterialColors": TYPE_STRING,
    "Bool": TYPE_BOOL,
    "Int32": TYPE_INT32,
    "Float32": TYPE_FLOAT32,
    "Float64": TYPE_FLOAT64,
    "UDim": TYPE_UDIM,
    "UDim2": TYPE_UDIM2,
    "Ray": TYPE_RAY,
    "Faces": TYPE_FACES,
    "Axes": TYPE_AXES,
    "BrickColor": TYPE_BRICKCOLOR,
    "Color3": TYPE_COLOR3,
    "Vector2": TYPE_VECTOR2,
    "Vector3": TYPE_VECTOR3,
    "CFrame": TYPE_CFRAME,
    "Enum": TYPE_ENUM,
    "Ref": TYPE_REF,
    "Vector3int16": TYPE_VECTOR3INT16,
    "NumberSequence": TYPE_NUMBERSEQUENCE,
    "ColorSequence": TYPE_COLORSEQUENCE,
    "NumberRange": TYPE_NUMBERRANGE,
    "Rect": TYPE_RECT,
    "PhysicalProperties": TYPE_PHYSICALPROPERTIES,
    "Color3uint8": TYPE_COLOR3UINT8,
    "Int64": TYPE_INT64,
    "SharedString": TYPE_SHAREDSTRING,
    "OptionalCoordinateFrame": TYPE_OPTIONALCFRAME,
    "OptionalCFrame": TYPE_OPTIONALCFRAME,
    "UniqueId": TYPE_UNIQUEID,
    "Font": TYPE_FONT,
    "SecurityCapabilities": TYPE_SECURITYCAPABILITIES,
    "Content": TYPE_CONTENT,
}


# Value wrapper types for unambiguous Python construction
@dataclass(frozen=True)
class Color3Val:
    r: float
    g: float
    b: float

    @staticmethod
    def from_rgb(r: int, g: int, b: int) -> "Color3Val":
        return Color3Val(r / 255.0, g / 255.0, b / 255.0)


@dataclass(frozen=True)
class Vector2Val:
    x: float
    y: float


@dataclass(frozen=True)
class Vector3Val:
    x: float
    y: float
    z: float


@dataclass(frozen=True)
class CFrameVal:
    pos: Tuple[float, float, float] = (0.0, 0.0, 0.0)
    # Row-major 3x3 rotation matrix: (R00, R01, R02, R10, R11, R12, R20, R21, R22)
    rot: Tuple[float, float, float, float, float, float, float, float, float] = (
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    )

    @staticmethod
    def from_pos(x: float, y: float, z: float) -> "CFrameVal":
        return CFrameVal((float(x), float(y), float(z)))

    @staticmethod
    def from_euler_xyz(x: float, y: float, z: float, rx: float = 0.0, ry: float = 0.0, rz: float = 0.0) -> "CFrameVal":
        cx, sx = math.cos(rx), math.sin(rx)
        cy, sy = math.cos(ry), math.sin(ry)
        cz, sz = math.cos(rz), math.sin(rz)
        # Roblox CFrame.Angles(rx, ry, rz) = Rx * Ry * Rz
        r00 = cy * cz
        r01 = -cy * sz
        r02 = sy
        r10 = sx * sy * cz + cx * sz
        r11 = -sx * sy * sz + cx * cz
        r12 = -sx * cy
        r20 = -cx * sy * cz + sx * sz
        r21 = cx * sy * sz + sx * cz
        r22 = cx * cy
        return CFrameVal((float(x), float(y), float(z)), (r00, r01, r02, r10, r11, r12, r20, r21, r22))

    @staticmethod
    def look_at(eye: Tuple[float, float, float], target: Tuple[float, float, float], up: Tuple[float, float, float] = (0.0, 1.0, 0.0)) -> "CFrameVal":
        fx, fy, fz = target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]
        flen = math.sqrt(fx * fx + fy * fy + fz * fz)
        if flen < 1e-8:
            return CFrameVal.from_pos(*eye)
        # In Roblox, -Z is forward
        zx, zy, zz = -fx / flen, -fy / flen, -fz / flen
        ux, uy, uz = up
        # X = up cross Z
        xx = uy * zz - uz * zy
        xy = uz * zx - ux * zz
        xz = ux * zy - uy * zx
        xlen = math.sqrt(xx * xx + xy * xy + xz * xz)
        if xlen < 1e-8:
            xx, xy, xz = 1.0, 0.0, 0.0
        else:
            xx, xy, xz = xx / xlen, xy / xlen, xz / xlen
        # Y = Z cross X
        yx = zy * xz - zz * xy
        yy = zz * xx - zx * xz
        yz = zx * xy - zy * xx
        return CFrameVal(
            (float(eye[0]), float(eye[1]), float(eye[2])),
            (xx, yx, zx, xy, yy, zy, xz, yz, zz),
        )


@dataclass(frozen=True)
class UDimVal:
    scale: float
    offset: int


@dataclass(frozen=True)
class UDim2Val:
    xs: float
    xo: int
    ys: float
    yo: int


@dataclass(frozen=True)
class NumberRangeVal:
    min_v: float
    max_v: float


@dataclass(frozen=True)
class NumberSequenceVal:
    # List of (time, value, envelope)
    keypoints: List[Tuple[float, float, float]]


@dataclass(frozen=True)
class ColorSequenceVal:
    # List of (time, Color3Val)
    keypoints: List[Tuple[float, Color3Val]]


@dataclass(frozen=True)
class RectVal:
    min_x: float
    min_y: float
    max_x: float
    max_y: float


@dataclass(frozen=True)
class FontVal:
    family: str = "rbxasset://fonts/families/RobotoMono.json"
    weight: int = 400
    style: int = 0  # 0 = Normal, 1 = Italic
    cached_face_id: str = ""


@dataclass(frozen=True)
class PhysicalPropertiesVal:
    custom: bool = False
    density: float = 0.7
    friction: float = 0.3
    elasticity: float = 0.5
    friction_weight: float = 1.0
    elasticity_weight: float = 1.0


@dataclass(frozen=True)
class EnumVal:
    enum_name: str
    item_name_or_val: Any


class RbxInstance:
    _next_uid = 1

    def __init__(self, class_name: str, name: Optional[str] = None, parent: Optional["RbxInstance"] = None, **props: Any):
        self.uid = RbxInstance._next_uid
        RbxInstance._next_uid += 1
        self.class_name = class_name
        self.name = name if name is not None else class_name
        self.properties: Dict[str, Any] = {}
        self.attributes: Dict[str, Any] = {}
        self.tags: List[str] = []
        self.children: List["RbxInstance"] = []
        self.parent: Optional["RbxInstance"] = None

        for k, v in props.items():
            self.set_prop(k, v)

        if parent is not None:
            parent.add_child(self)

    def add_child(self, child: "RbxInstance") -> "RbxInstance":
        if child.parent is not None:
            child.parent.children.remove(child)
        child.parent = self
        self.children.append(child)
        return child

    def set_prop(self, name: str, val: Any) -> "RbxInstance":
        if name == "Name":
            self.name = str(val)
        else:
            self.properties[name] = val
        return self

    def set_attr(self, name: str, val: Any) -> "RbxInstance":
        self.attributes[name] = val
        return self

    def add_tag(self, tag: str) -> "RbxInstance":
        if tag not in self.tags:
            self.tags.append(tag)
        return self

    def find_first_child(self, name: str) -> Optional["RbxInstance"]:
        for c in self.children:
            if c.name == name:
                return c
        return None


class ReflectionDB:
    def __init__(self, msgpack_path: Optional[str] = None):
        if msgpack_path is None:
            msgpack_path = os.path.join(os.path.dirname(__file__), "rbx_reflection_db.msgpack")
        with open(msgpack_path, "rb") as f:
            raw = msgpack.unpackb(f.read(), raw=False, strict_map_key=False)
        self.version = raw[0]
        self.classes: Dict[str, Any] = raw[1]
        self.enums: Dict[str, Dict[str, int]] = {k: v[1] for k, v in raw[2].items()}

    def is_service(self, class_name: str) -> bool:
        cur = class_name
        while cur and cur in self.classes:
            cls = self.classes[cur]
            if "Service" in cls[1]:
                return True
            cur = cls[2]
        return False

    def resolve_enum(self, enum_name: str, val: Any) -> int:
        if isinstance(val, int):
            return val
        if isinstance(val, EnumVal):
            enum_name = val.enum_name
            val = val.item_name_or_val
            if isinstance(val, int):
                return val
        enum_map = self.enums.get(enum_name)
        if not enum_map or val not in enum_map:
            raise ValueError(f"Unknown Enum {enum_name}.{val} (available: {list((enum_map or {}).keys())[:10]})")
        return enum_map[val]

    def resolve_property(self, class_name: str, prop_name: str) -> Optional[Tuple[str, str, Any, Optional[str]]]:
        """
        Returns (canonical_name, serialized_name, data_type_dict, default_raw_dict)
        or None if property does not serialize.
        """
        cur = class_name
        found_cls = None
        found_prop = None
        while cur and cur in self.classes:
            cls = self.classes[cur]
            if prop_name in cls[3]:
                found_cls = cls
                found_prop = cls[3][prop_name]
                break
            cur = cls[2]

        if found_prop is None:
            return None

        # Unpack descriptor: [name, scriptability, data_type, tags, kind]
        kind = found_prop[4]
        if "Alias" in kind:
            alias_target = kind["Alias"][0]
            canonical_desc = found_cls[3][alias_target]
        elif "Canonical" in kind:
            canonical_desc = found_prop
        else:
            return None

        canonical_name = canonical_desc[0]
        ser_info = canonical_desc[4]["Canonical"][0]
        if ser_info == "DoesNotSerialize":
            return None
        elif ser_info == "Serializes" or (isinstance(ser_info, dict) and "Migrate" in ser_info):
            serialized_desc = canonical_desc
        elif isinstance(ser_info, dict) and "SerializesAs" in ser_info:
            ser_name = ser_info["SerializesAs"]
            serialized_desc = found_cls[3][ser_name]
        else:
            return None

        serialized_name = serialized_desc[0]
        data_type = serialized_desc[2]  # {'Value': 'Vector3'} or {'Enum': 'PartType'}

        # Find default value walking hierarchy
        default_val = None
        cur2 = class_name
        while cur2 and cur2 in self.classes:
            defaults = self.classes[cur2][4]
            if canonical_name in defaults:
                default_val = defaults[canonical_name]
                break
            if serialized_name in defaults:
                default_val = defaults[serialized_name]
                break
            cur2 = self.classes[cur2][2]

        enum_name = data_type.get("Enum")
        variant_type_name = "Enum" if enum_name else data_type.get("Value")
        return canonical_name, serialized_name, (variant_type_name, default_val), enum_name


# Low-level binary encoding helpers matching rbx_binary/src/core.rs
def transform_i32(val: int) -> int:
    val = int(val)
    if val < 0:
        u = (1 << 32) + val
    else:
        u = val
    # Signed 32-bit shift right arithmetic
    sign = -1 if (u & 0x80000000) else 0
    res = ((u << 1) ^ sign) & 0xFFFFFFFF
    return res


def untransform_i32(u: int) -> int:
    u &= 0xFFFFFFFF
    v = (u >> 1) ^ (-(u & 1) & 0xFFFFFFFF)
    if v & 0x80000000:
        return v - (1 << 32)
    return v


def transform_i64(val: int) -> int:
    val = int(val)
    u = ((1 << 64) + val) if val < 0 else val
    sign = -1 if (u & 0x8000000000000000) else 0
    return ((u << 1) ^ sign) & 0xFFFFFFFFFFFFFFFF


def rotate_f32_bits(val: float) -> int:
    bits = struct.unpack("<I", struct.pack("<f", float(val)))[0]
    return ((bits << 1) | (bits >> 31)) & 0xFFFFFFFF


def unrotate_f32_bits(u: int) -> float:
    bits = ((u >> 1) | ((u & 1) << 31)) & 0xFFFFFFFF
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def write_interleaved_bytes(items: List[bytes], width: int) -> bytes:
    n = len(items)
    if n == 0:
        return b""
    out = bytearray(n * width)
    for i, b in enumerate(items):
        for col in range(width):
            out[i + col * n] = b[col]
    return bytes(out)


def encode_interleaved_i32(vals: List[int]) -> bytes:
    return write_interleaved_bytes([struct.pack(">I", transform_i32(v)) for v in vals], 4)


def encode_interleaved_u32(vals: List[int]) -> bytes:
    return write_interleaved_bytes([struct.pack(">I", v & 0xFFFFFFFF) for v in vals], 4)


def encode_interleaved_i64(vals: List[int]) -> bytes:
    return write_interleaved_bytes([struct.pack(">Q", transform_i64(v)) for v in vals], 8)


def encode_interleaved_f32(vals: List[float]) -> bytes:
    return write_interleaved_bytes([struct.pack(">I", rotate_f32_bits(v)) for v in vals], 4)


def encode_referent_array(refs: List[int]) -> bytes:
    deltas = []
    last = 0
    for r in refs:
        deltas.append(r - last)
        last = r
    return encode_interleaved_i32(deltas)


def encode_string(s: bytes | str) -> bytes:
    b = s.encode("utf-8") if isinstance(s, str) else bytes(s)
    return struct.pack("<I", len(b)) + b


def encode_attributes_blob(attrs: Dict[str, Any]) -> bytes:
    """Encodes Roblox AttributesSerialize binary blob per docs/attributes.md."""
    if not attrs:
        return b""
    buf = bytearray()
    sorted_items = sorted(attrs.items(), key=lambda kv: kv[0])
    buf.extend(struct.pack("<I", len(sorted_items)))
    for k, v in sorted_items:
        buf.extend(encode_string(k))
        if isinstance(v, bool):
            buf.append(0x03)
            buf.append(1 if v else 0)
        elif isinstance(v, (int, float)):
            # Roblox Attributes uses Float64 (0x06) for numbers
            buf.append(0x06)
            buf.extend(struct.pack("<d", float(v)))
        elif isinstance(v, str):
            buf.append(0x02)
            buf.extend(encode_string(v))
        elif isinstance(v, Color3Val):
            buf.append(0x0F)
            buf.extend(struct.pack("<fff", v.r, v.g, v.b))
        elif isinstance(v, Vector3Val):
            buf.append(0x11)
            buf.extend(struct.pack("<fff", v.x, v.y, v.z))
        elif isinstance(v, Vector2Val):
            buf.append(0x10)
            buf.extend(struct.pack("<ff", v.x, v.y))
        elif isinstance(v, UDim2Val):
            buf.append(0x0A)
            buf.extend(struct.pack("<fi", v.xs, v.xo))
            buf.extend(struct.pack("<fi", v.ys, v.yo))
        else:
            buf.append(0x02)
            buf.extend(encode_string(str(v)))
    return bytes(buf)


def encode_tags_blob(tags: List[str]) -> bytes:
    if not tags:
        return b""
    return b"\x00".join(t.encode("utf-8") for t in tags) + b"\x00"


def encode_chunk(name: bytes, payload: bytes, compress: bool = True) -> bytes:
    assert len(name) == 4
    if compress and len(payload) > 0:
        compressed = lz4.block.compress(payload, store_size=False)
        header = name + struct.pack("<III", len(compressed), len(payload), 0)
        return header + compressed
    else:
        header = name + struct.pack("<III", 0, len(payload), 0)
        return header + payload


def convert_db_default(variant_type: str, raw_default: Optional[Dict[str, Any]]) -> Any:
    if raw_default is not None:
        if "Bool" in raw_default:
            return bool(raw_default["Bool"])
        if "Int32" in raw_default:
            return int(raw_default["Int32"])
        if "Int64" in raw_default:
            return int(raw_default["Int64"])
        if "Float32" in raw_default:
            return float(raw_default["Float32"])
        if "Float64" in raw_default:
            return float(raw_default["Float64"])
        if "String" in raw_default:
            return str(raw_default["String"])
        if "ContentId" in raw_default:
            return str(raw_default["ContentId"])
        if "BinaryString" in raw_default:
            return bytes(raw_default["BinaryString"])
        if "Enum" in raw_default:
            return int(raw_default["Enum"])
        if "BrickColor" in raw_default:
            return int(raw_default["BrickColor"])
        if "Color3" in raw_default:
            c = raw_default["Color3"]
            return Color3Val(float(c[0]), float(c[1]), float(c[2]))
        if "Color3uint8" in raw_default:
            c = raw_default["Color3uint8"]
            return Color3Val.from_rgb(int(c[0]), int(c[1]), int(c[2]))
        if "Vector2" in raw_default:
            v = raw_default["Vector2"]
            return Vector2Val(float(v[0]), float(v[1]))
        if "Vector3" in raw_default:
            v = raw_default["Vector3"]
            return Vector3Val(float(v[0]), float(v[1]), float(v[2]))
        if "CFrame" in raw_default:
            cf = raw_default["CFrame"]
            pos = tuple(float(x) for x in cf[0])
            m = cf[1]
            rot = (
                float(m[0][0]), float(m[0][1]), float(m[0][2]),
                float(m[1][0]), float(m[1][1]), float(m[1][2]),
                float(m[2][0]), float(m[2][1]), float(m[2][2]),
            )
            return CFrameVal(pos, rot)
        if "UDim" in raw_default:
            u = raw_default["UDim"]
            return UDimVal(float(u[0]), int(u[1]))
        if "UDim2" in raw_default:
            u = raw_default["UDim2"]
            return UDim2Val(float(u[0][0]), int(u[0][1]), float(u[1][0]), int(u[1][1]))
        if "NumberRange" in raw_default:
            nr = raw_default["NumberRange"]
            return NumberRangeVal(float(nr[0]), float(nr[1]))
        if "Rect" in raw_default:
            rc = raw_default["Rect"]
            return RectVal(float(rc[0][0]), float(rc[0][1]), float(rc[1][0]), float(rc[1][1]))
        if "Font" in raw_default:
            fn = raw_default["Font"]
            if isinstance(fn, dict):
                return FontVal(
                    fn.get("family", "rbxasset://fonts/families/SourceSansPro.json"),
                    int(fn.get("weight", 400)),
                    0 if fn.get("style", "Normal") == "Normal" else 1,
                    fn.get("cachedFaceId") or "",
                )

    # Safe fallbacks matching rbx_binary fallback_default_value
    fallbacks = {
        "String": "",
        "BinaryString": b"",
        "ContentId": "",
        "Tags": b"",
        "Attributes": b"",
        "Bool": False,
        "Int32": 0,
        "Int64": 0,
        "Float32": 0.0,
        "Float64": 0.0,
        "UDim": UDimVal(0.0, 0),
        "UDim2": UDim2Val(0.0, 0, 0.0, 0),
        "BrickColor": 194,
        "Color3": Color3Val(0.0, 0.0, 0.0),
        "Color3uint8": Color3Val(0.639, 0.635, 0.647),
        "Vector2": Vector2Val(0.0, 0.0),
        "Vector3": Vector3Val(0.0, 0.0, 0.0),
        "CFrame": CFrameVal(),
        "Enum": 0,
        "Ref": None,
        "NumberSequence": NumberSequenceVal([(0.0, 0.0, 0.0), (1.0, 0.0, 0.0)]),
        "ColorSequence": ColorSequenceVal([(0.0, Color3Val(1.0, 1.0, 1.0)), (1.0, Color3Val(1.0, 1.0, 1.0))]),
        "NumberRange": NumberRangeVal(0.0, 0.0),
        "Rect": RectVal(0.0, 0.0, 0.0, 0.0),
        "PhysicalProperties": PhysicalPropertiesVal(False),
        "Font": FontVal(),
        "OptionalCoordinateFrame": None,
        "OptionalCFrame": None,
        "Content": "",
    }
    return fallbacks.get(variant_type, "")


class RbxlSerializer:
    def __init__(self, db: Optional[ReflectionDB] = None, compress: bool = True):
        self.db = db or ReflectionDB()
        self.compress = compress

    def _is_subclass(self, cls_name: str, target_base: str) -> bool:
        cur: Optional[str] = cls_name
        while cur and cur in self.db.classes:
            if cur == target_base:
                return True
            cur = self.db.classes[cur][2]
        return False

    def _validate_studio_parent_rules(self, inst: RbxInstance) -> None:
        pcls = inst.parent.class_name if inst.parent else None
        if inst.class_name == "SurfaceAppearance":
            if pcls != "MeshPart":
                raise ValueError(f"Roblox Studio rule violation: SurfaceAppearance '{inst.name}' parented to {pcls} (must be MeshPart)")
        elif inst.class_name == "Attachment":
            if not pcls or not (self._is_subclass(pcls, "BasePart") or pcls == "Bone"):
                raise ValueError(f"Roblox Studio rule violation: Attachment '{inst.name}' parented to {pcls} (must be BasePart or Bone)")
        elif inst.class_name == "Clouds":
            if pcls != "Terrain":
                raise ValueError(f"Roblox Studio rule violation: Clouds '{inst.name}' parented to {pcls} (must be Terrain)")
        elif inst.class_name == "MaterialVariant":
            if pcls != "MaterialService":
                raise ValueError(f"Roblox Studio rule violation: MaterialVariant '{inst.name}' parented to {pcls} (must be MaterialService)")

    def serialize(self, root_services: List[RbxInstance]) -> bytes:
        # Depth-first post-order traversal matching rbx_binary SerializerState::add_instances
        ordered: List[RbxInstance] = []

        def visit_postorder(node: RbxInstance):
            self._validate_studio_parent_rules(node)
            for child in node.children:
                visit_postorder(child)
            ordered.append(node)

        for root in root_services:
            visit_postorder(root)

        uid_to_ref: Dict[int, int] = {inst.uid: idx for idx, inst in enumerate(ordered)}

        # Group instances by ClassName in deterministic lexical order
        by_class: Dict[str, List[RbxInstance]] = {}
        for inst in ordered:
            by_class.setdefault(inst.class_name, []).append(inst)

        sorted_classes = sorted(by_class.keys())
        class_to_type_id = {cname: tid for tid, cname in enumerate(sorted_classes)}

        out = bytearray()
        # 32-byte File Header
        out.extend(FILE_MAGIC_HEADER)
        out.extend(FILE_SIGNATURE)
        out.extend(struct.pack("<H", FILE_VERSION))
        out.extend(struct.pack("<I", len(sorted_classes)))
        out.extend(struct.pack("<I", len(ordered)))
        out.extend(b"\x00" * 8)

        # META chunk (ExplicitAutoJoints = true)
        meta_buf = bytearray()
        meta_buf.extend(struct.pack("<I", 1))
        meta_buf.extend(encode_string("ExplicitAutoJoints"))
        meta_buf.extend(encode_string("true"))
        out.extend(encode_chunk(b"META", bytes(meta_buf), self.compress))

        # INST chunks
        for cname in sorted_classes:
            insts = by_class[cname]
            tid = class_to_type_id[cname]
            is_srv = self.db.is_service(cname)
            cbuf = bytearray()
            cbuf.extend(struct.pack("<I", tid))
            cbuf.extend(encode_string(cname))
            cbuf.append(1 if is_srv else 0)
            cbuf.extend(struct.pack("<I", len(insts)))
            cbuf.extend(encode_referent_array([uid_to_ref[i.uid] for i in insts]))
            if is_srv:
                cbuf.extend(b"\x01" * len(insts))
            out.extend(encode_chunk(b"INST", bytes(cbuf), self.compress))

        # PROP chunks
        for cname in sorted_classes:
            insts = by_class[cname]
            tid = class_to_type_id[cname]

            # Gather all used properties across instances of this class
            # Map canonical_name -> (serialized_name, variant_type, default_val, enum_name, [values])
            prop_map: Dict[str, Tuple[str, str, Any, Optional[str], List[Any]]] = {}

            for idx, inst in enumerate(insts):
                effective_props = dict(inst.properties)
                if inst.attributes:
                    effective_props["AttributesSerialize"] = encode_attributes_blob(inst.attributes)
                if inst.tags:
                    effective_props["Tags"] = encode_tags_blob(inst.tags)

                for raw_pname, pval in effective_props.items():
                    resolved = self.db.resolve_property(cname, raw_pname)
                    if resolved is None:
                        continue
                    canonical_name, serialized_name, (variant_type, raw_def), enum_name = resolved
                    if canonical_name == "Name":
                        continue
                    if variant_type not in VARIANT_TO_BINARY_TYPE:
                        continue
                    if canonical_name not in prop_map:
                        def_val = convert_db_default(variant_type, raw_def)
                        prop_map[canonical_name] = (
                            serialized_name,
                            variant_type,
                            def_val,
                            enum_name,
                            [def_val] * len(insts),
                        )
                    prop_map[canonical_name][4][idx] = pval

            # Write properties sorted by canonical_name, inserting Name in lexical order
            sorted_pnames = sorted(list(prop_map.keys()) + ["Name"])
            for pname in sorted_pnames:
                if pname == "Name":
                    cbuf = bytearray()
                    cbuf.extend(struct.pack("<I", tid))
                    cbuf.extend(encode_string("Name"))
                    cbuf.append(TYPE_STRING)
                    for inst in insts:
                        cbuf.extend(encode_string(inst.name))
                    out.extend(encode_chunk(b"PROP", bytes(cbuf), self.compress))
                    continue

                serialized_name, variant_type, _, enum_name, values = prop_map[pname]
                btype = VARIANT_TO_BINARY_TYPE[variant_type]
                cbuf = bytearray()
                cbuf.extend(struct.pack("<I", tid))
                cbuf.extend(encode_string(serialized_name))
                cbuf.append(btype)
                cbuf.extend(self._encode_prop_values(btype, variant_type, enum_name, values, uid_to_ref, cname, serialized_name))
                out.extend(encode_chunk(b"PROP", bytes(cbuf), self.compress))

        # PRNT chunk
        prnt_buf = bytearray()
        prnt_buf.append(0)  # version 0
        prnt_buf.extend(struct.pack("<I", len(ordered)))
        obj_refs = [uid_to_ref[i.uid] for i in ordered]
        parent_refs = [
            uid_to_ref[i.parent.uid] if (i.parent is not None and i.parent.uid in uid_to_ref) else -1
            for i in ordered
        ]
        prnt_buf.extend(encode_referent_array(obj_refs))
        prnt_buf.extend(encode_referent_array(parent_refs))
        out.extend(encode_chunk(b"PRNT", bytes(prnt_buf), self.compress))

        # END\0 chunk (always uncompressed per spec)
        out.extend(encode_chunk(b"END\x00", FILE_FOOTER, compress=False))
        return bytes(out)

    def _encode_prop_values(
        self,
        btype: int,
        variant_type: str,
        enum_name: Optional[str],
        values: List[Any],
        uid_to_ref: Dict[int, int],
        cname: str,
        sname: str,
    ) -> bytes:
        buf = bytearray()
        if btype == TYPE_STRING:
            for v in values:
                buf.extend(encode_string(v if isinstance(v, (str, bytes)) else str(v)))
        elif btype == TYPE_BOOL:
            for v in values:
                buf.append(1 if bool(v) else 0)
        elif btype == TYPE_INT32:
            buf.extend(encode_interleaved_i32([int(v) for v in values]))
        elif btype == TYPE_INT64:
            buf.extend(encode_interleaved_i64([int(v) for v in values]))
        elif btype == TYPE_FLOAT32:
            buf.extend(encode_interleaved_f32([float(v) for v in values]))
        elif btype == TYPE_FLOAT64:
            for v in values:
                buf.extend(struct.pack("<d", float(v)))
        elif btype == TYPE_UDIM:
            scales = [float(v.scale) for v in values]
            offsets = [int(v.offset) for v in values]
            buf.extend(encode_interleaved_f32(scales))
            buf.extend(encode_interleaved_i32(offsets))
        elif btype == TYPE_UDIM2:
            xs = [float(v.xs) for v in values]
            ys = [float(v.ys) for v in values]
            xo = [int(v.xo) for v in values]
            yo = [int(v.yo) for v in values]
            buf.extend(encode_interleaved_f32(xs))
            buf.extend(encode_interleaved_f32(ys))
            buf.extend(encode_interleaved_i32(xo))
            buf.extend(encode_interleaved_i32(yo))
        elif btype == TYPE_BRICKCOLOR:
            buf.extend(encode_interleaved_u32([int(v) for v in values]))
        elif btype == TYPE_COLOR3:
            rs, gs, bs = [], [], []
            for v in values:
                c = v if isinstance(v, Color3Val) else Color3Val(*v)
                rs.append(float(c.r))
                gs.append(float(c.g))
                bs.append(float(c.b))
            buf.extend(encode_interleaved_f32(rs))
            buf.extend(encode_interleaved_f32(gs))
            buf.extend(encode_interleaved_f32(bs))
        elif btype == TYPE_COLOR3UINT8:
            rs, gs, bs = bytearray(), bytearray(), bytearray()
            for v in values:
                c = v if isinstance(v, Color3Val) else Color3Val(*v)
                rs.append(max(0, min(255, int(round(c.r * 255.0)))))
                gs.append(max(0, min(255, int(round(c.g * 255.0)))))
                bs.append(max(0, min(255, int(round(c.b * 255.0)))))
            buf.extend(rs)
            buf.extend(gs)
            buf.extend(bs)
        elif btype == TYPE_VECTOR2:
            xs, ys = [], []
            for v in values:
                vec = v if isinstance(v, Vector2Val) else Vector2Val(*v)
                xs.append(float(vec.x))
                ys.append(float(vec.y))
            buf.extend(encode_interleaved_f32(xs))
            buf.extend(encode_interleaved_f32(ys))
        elif btype == TYPE_VECTOR3:
            xs, ys, zs = [], [], []
            for v in values:
                vec = v if isinstance(v, Vector3Val) else Vector3Val(*v)
                xs.append(float(vec.x))
                ys.append(float(vec.y))
                zs.append(float(vec.z))
            buf.extend(encode_interleaved_f32(xs))
            buf.extend(encode_interleaved_f32(ys))
            buf.extend(encode_interleaved_f32(zs))
        elif btype == TYPE_CFRAME:
            xs, ys, zs = [], [], []
            for v in values:
                cf = v if isinstance(v, CFrameVal) else CFrameVal()
                xs.append(float(cf.pos[0]))
                ys.append(float(cf.pos[1]))
                zs.append(float(cf.pos[2]))
                r = cf.rot
                # Identity shortcut (orient_id = 0x02 is R00=1, R11=1, R22=1)
                if (
                    abs(r[0] - 1.0) < 1e-6 and abs(r[4] - 1.0) < 1e-6 and abs(r[8] - 1.0) < 1e-6
                    and abs(r[1]) < 1e-6 and abs(r[2]) < 1e-6 and abs(r[3]) < 1e-6
                    and abs(r[5]) < 1e-6 and abs(r[6]) < 1e-6 and abs(r[7]) < 1e-6
                ):
                    buf.append(0x02)
                else:
                    buf.append(0x00)
                    buf.extend(struct.pack("<9f", *r))
            buf.extend(encode_interleaved_f32(xs))
            buf.extend(encode_interleaved_f32(ys))
            buf.extend(encode_interleaved_f32(zs))
        elif btype == TYPE_OPTIONALCFRAME:
            xs, ys, zs = [], [], []
            present = bytearray()
            buf.append(TYPE_CFRAME)
            for v in values:
                if v is None:
                    present.append(0)
                    buf.append(0x02)
                    xs.append(0.0)
                    ys.append(0.0)
                    zs.append(0.0)
                else:
                    present.append(1)
                    cf = v if isinstance(v, CFrameVal) else CFrameVal()
                    xs.append(float(cf.pos[0]))
                    ys.append(float(cf.pos[1]))
                    zs.append(float(cf.pos[2]))
                    buf.append(0x00)
                    buf.extend(struct.pack("<9f", *cf.rot))
            buf.extend(encode_interleaved_f32(xs))
            buf.extend(encode_interleaved_f32(ys))
            buf.extend(encode_interleaved_f32(zs))
            buf.append(TYPE_BOOL)
            buf.extend(present)
        elif btype == TYPE_ENUM:
            evals = [self.db.resolve_enum(enum_name or "", v) for v in values]
            buf.extend(encode_interleaved_u32(evals))
        elif btype == TYPE_REF:
            refs = []
            for v in values:
                if isinstance(v, RbxInstance) and v.uid in uid_to_ref:
                    refs.append(uid_to_ref[v.uid])
                else:
                    refs.append(-1)
            buf.extend(encode_referent_array(refs))
        elif btype == TYPE_NUMBERSEQUENCE:
            for v in values:
                ns = v if isinstance(v, NumberSequenceVal) else NumberSequenceVal([(0.0, float(v), 0.0), (1.0, float(v), 0.0)])
                buf.extend(struct.pack("<I", len(ns.keypoints)))
                for t, val, env in ns.keypoints:
                    buf.extend(struct.pack("<fff", float(t), float(val), float(env)))
        elif btype == TYPE_COLORSEQUENCE:
            for v in values:
                cs = v
                buf.extend(struct.pack("<I", len(cs.keypoints)))
                for t, col in cs.keypoints:
                    buf.extend(struct.pack("<fffff", float(t), float(col.r), float(col.g), float(col.b), 0.0))
        elif btype == TYPE_NUMBERRANGE:
            for v in values:
                nr = v if isinstance(v, NumberRangeVal) else NumberRangeVal(float(v[0]), float(v[1]))
                buf.extend(struct.pack("<ff", float(nr.min_v), float(nr.max_v)))
        elif btype == TYPE_RECT:
            x0, y0, x1, y1 = [], [], [], []
            for v in values:
                x0.append(float(v.min_x))
                y0.append(float(v.min_y))
                x1.append(float(v.max_x))
                y1.append(float(v.max_y))
            buf.extend(encode_interleaved_f32(x0))
            buf.extend(encode_interleaved_f32(y0))
            buf.extend(encode_interleaved_f32(x1))
            buf.extend(encode_interleaved_f32(y1))
        elif btype == TYPE_PHYSICALPROPERTIES:
            for v in values:
                pp = v if isinstance(v, PhysicalPropertiesVal) else PhysicalPropertiesVal(False)
                if not pp.custom:
                    buf.append(0x00)
                else:
                    buf.append(0x01)
                    buf.extend(struct.pack("<fffff", pp.density, pp.friction, pp.elasticity, pp.friction_weight, pp.elasticity_weight))
        elif btype == TYPE_FONT:
            for v in values:
                fn = v if isinstance(v, FontVal) else FontVal(str(v))
                buf.extend(encode_string(fn.family))
                buf.extend(struct.pack("<H", int(fn.weight)))
                buf.append(int(fn.style))
                buf.extend(encode_string(fn.cached_face_id))
        elif btype == TYPE_CONTENT:
            src_types = []
            uris = []
            for v in values:
                s = str(v) if v else ""
                if not s:
                    src_types.append(0)
                else:
                    src_types.append(1)
                    uris.append(s)
            buf.extend(encode_interleaved_i32(src_types))
            buf.extend(struct.pack("<I", len(uris)))
            for u in uris:
                buf.extend(encode_string(u))
            buf.extend(struct.pack("<II", 0, 0))
        else:
            raise ValueError(f"Unsupported binary type {hex(btype)} on {cname}.{sname}")
        return bytes(buf)


def inspect_rbxl_binary(data: bytes) -> Dict[str, Any]:
    """Independently decodes and validates a binary .rbxl/.rbxm buffer."""
    assert data[:8] == FILE_MAGIC_HEADER, "Invalid magic header"
    assert data[8:14] == FILE_SIGNATURE, "Invalid file signature"
    ver, num_types, num_instances = struct.unpack("<HII", data[14:24])
    assert ver == 0, f"Unexpected format version {ver}"
    pos = 32
    chunks = []
    inst_counts = {}
    prnt_count = 0
    saw_end = False
    while pos < len(data):
        cname = data[pos : pos + 4]
        comp_len, raw_len, reserved = struct.unpack("<III", data[pos + 4 : pos + 16])
        assert reserved == 0, "Non-zero chunk reserved field"
        pos += 16
        if comp_len == 0:
            payload = data[pos : pos + raw_len]
            pos += raw_len
        else:
            comp = data[pos : pos + comp_len]
            pos += comp_len
            payload = lz4.block.decompress(comp, uncompressed_size=raw_len)
        assert len(payload) == raw_len
        chunks.append((cname, comp_len, raw_len))
        if cname == b"INST":
            tid = struct.unpack("<I", payload[:4])[0]
            slen = struct.unpack("<I", payload[4:8])[0]
            cls_name = payload[8 : 8 + slen].decode("utf-8")
            icount = struct.unpack("<I", payload[9 + slen : 13 + slen])[0]
            inst_counts[cls_name] = icount
        elif cname == b"PRNT":
            prnt_count = struct.unpack("<I", payload[1:5])[0]
        elif cname == b"END\x00":
            assert payload == FILE_FOOTER
            saw_end = True
            break
    assert saw_end, "Missing END\\0 chunk"
    assert pos == len(data), "Trailing bytes after END\\0"
    assert len(inst_counts) == num_types, f"Type count mismatch: {len(inst_counts)} vs {num_types}"
    assert sum(inst_counts.values()) == num_instances, "Total instance count mismatch"
    assert prnt_count == num_instances, "PRNT count mismatch"
    return {
        "version": ver,
        "num_types": num_types,
        "num_instances": num_instances,
        "num_chunks": len(chunks),
        "classes": inst_counts,
        "file_bytes": len(data),
    }


if __name__ == "__main__":
    # Self-test on all core classes & property types
    ws = RbxInstance("Workspace", "Workspace", Gravity=196.2)
    part = RbxInstance(
        "Part",
        "TestFloor",
        parent=ws,
        Anchored=True,
        Size=Vector3Val(64.0, 1.0, 64.0),
        CFrame=CFrameVal.from_euler_xyz(0.0, -0.5, 0.0, 0.0, 0.25, 0.0),
        Color=Color3Val.from_rgb(72, 76, 80),
        Material="Concrete",
        Shape="Block",
    )
    part.set_attr("Region", "DeadSuburbs")
    part.set_attr("RadiationLevel", 12.5)
    part.add_tag("Traversable")

    lighting = RbxInstance("Lighting", "Lighting", ClockTime=18.5, Brightness=1.4, Technology="Future", GlobalShadows=True)
    RbxInstance("Atmosphere", "Atmosphere", parent=lighting, Density=0.42, Haze=2.1, Color=Color3Val.from_rgb(135, 142, 148))

    sss = RbxInstance("ServerScriptService", "ServerScriptService")
    RbxInstance("Script", "Bootstrap", parent=sss, Source="print('Ashes of Humanity initialized')")

    blob = RbxlSerializer().serialize([ws, lighting, sss])
    os.makedirs("/tmp", exist_ok=True)
    with open("/tmp/selftest.rbxl", "wb") as f:
        f.write(blob)
    summary = inspect_rbxl_binary(blob)
    print("Self-test .rbxl serialized & verified successfully:", summary)
