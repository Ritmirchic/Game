"""Roblox reflection database helpers.

Loads rbx_dom_lua/src/database.json (a dump of Roblox's reflection metadata)
so that we can map canonical property names to their serialized names and wire
types, and so we know the type of every enum value we write.
"""
import json
import os

_DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "roblox", "rbx_database.json")
_DB = None

# Roblox binary wire types (from the rbx-dom binary format specification)
WIRE_STRING = 0x01
WIRE_BOOL = 0x02
WIRE_INT32 = 0x03
WIRE_FLOAT32 = 0x04
WIRE_FLOAT64 = 0x05
WIRE_UDIM = 0x06
WIRE_UDIM2 = 0x07
WIRE_RAY = 0x08
WIRE_FACES = 0x09
WIRE_AXES = 0x0A
WIRE_BRICKCOLOR = 0x0B
WIRE_COLOR3 = 0x0C
WIRE_VECTOR2 = 0x0D
WIRE_VECTOR3 = 0x0E
WIRE_CFRAME = 0x10
WIRE_ENUM = 0x12
WIRE_REF = 0x13
WIRE_VECTOR3INT16 = 0x14
WIRE_NUMBERSEQUENCE = 0x15
WIRE_COLORSEQUENCE = 0x16
WIRE_NUMBERRANGE = 0x17
WIRE_RECT = 0x18
WIRE_PHYSICALPROPERTIES = 0x19
WIRE_COLOR3UINT8 = 0x1A
WIRE_INT64 = 0x1B
WIRE_SHAREDSTRING = 0x1C
WIRE_BYTECODE = 0x1D
WIRE_OPTIONALCFRAME = 0x1E
WIRE_UNIQUEID = 0x1F
WIRE_FONT = 0x20
WIRE_CONTENT = 0x22

VALUE_WIRE = {
    "String": WIRE_STRING,
    "Bool": WIRE_BOOL,
    "Int32": WIRE_INT32,
    "Float32": WIRE_FLOAT32,
    "Float64": WIRE_FLOAT64,
    "UDim": WIRE_UDIM,
    "UDim2": WIRE_UDIM2,
    "Ray": WIRE_RAY,
    "Faces": WIRE_FACES,
    "Axes": WIRE_AXES,
    "BrickColor": WIRE_BRICKCOLOR,
    "Color3": WIRE_COLOR3,
    "Vector2": WIRE_VECTOR2,
    "Vector3": WIRE_VECTOR3,
    "CFrame": WIRE_CFRAME,
    "Vector3int16": WIRE_VECTOR3INT16,
    "NumberSequence": WIRE_NUMBERSEQUENCE,
    "ColorSequence": WIRE_COLORSEQUENCE,
    "NumberRange": WIRE_NUMBERRANGE,
    "Rect": WIRE_RECT,
    "PhysicalProperties": WIRE_PHYSICALPROPERTIES,
    "Color3uint8": WIRE_COLOR3UINT8,
    "Int64": WIRE_INT64,
    "SharedString": WIRE_SHAREDSTRING,
    "BinaryString": WIRE_STRING,
    "UniqueId": WIRE_UNIQUEID,
    "ContentId": WIRE_CONTENT,
    "Content": WIRE_CONTENT,
    "ProtectedString": WIRE_STRING,
}


def database():
    global _DB
    if _DB is None:
        with open(_DB_PATH, "r", encoding="utf8") as fh:
            _DB = json.load(fh)
    return _DB


def class_info(cls):
    db = database()
    c = db["Classes"].get(cls)
    if c is None:
        raise KeyError("unknown class: " + cls)
    return c


def superclass_chain(cls):
    chain = []
    cur = cls
    while cur:
        info = database()["Classes"].get(cur)
        if info is None:
            break
        chain.append(cur)
        cur = info.get("Superclass")
    return chain


def serializable_properties(cls):
    """Map serialized property name -> (canonical name, wire type id)."""
    out = {}
    for name in superclass_chain(cls):
        info = database()["Classes"][name]
        for pname, prop in info["Properties"].items():
            kind = prop.get("Kind", {})
            canon = kind.get("Canonical")
            if canon is None:
                continue
            ser = canon.get("Serialization")
            if ser == "DoesNotSerialize" or ser is None:
                continue
            sname = ser.get("SerializesAs", pname) if isinstance(ser, dict) else pname
            dt = prop["DataType"]
            if "Enum" in dt:
                wire = WIRE_ENUM
            else:
                v = dt.get("Value", "")
                if sname == "Color3uint8":
                    wire = WIRE_COLOR3UINT8
                elif v in VALUE_WIRE:
                    wire = VALUE_WIRE[v]
                else:
                    wire = None
            out[sname] = (pname, wire, prop)
    return out


def canonical_to_serialized(cls, canonical_name):
    for sname, (cname, wire, prop) in serializable_properties(cls).items():
        if cname == canonical_name:
            return sname, wire
    raise KeyError("%s does not serialize property %s" % (cls, canonical_name))


def default_property(cls, canonical_name):
    info = class_info(cls)
    v = info.get("DefaultProperties", {}).get(canonical_name)
    if v is None:
        for name in superclass_chain(cls):
            v = database()["Classes"][name].get("DefaultProperties", {}).get(canonical_name)
            if v is not None:
                break
    if v is None:
        return None
    return v


def enum_value(enum_name, item_name):
    en = database()["Enums"][enum_name]
    items = en.get("items", en.get("Items", {}))
    if item_name in items:
        return items[item_name]
    # allow passing the numeric value directly
    if isinstance(item_name, int):
        return item_name
    raise KeyError("enum %s has no item %s" % (enum_name, item_name))


def enum_items(enum_name):
    en = database()["Enums"][enum_name]
    return en.get("items", en.get("Items", {}))


def enum_name_of(enum_name, value):
    for item, v in enum_items(enum_name).items():
        if v == value:
            return item
    return None


def property_wire(cls, canonical_name):
    return canonical_to_serialized(cls, canonical_name)[1]


def default_to_python(cls, canonical_name):
    """Convert a database default (JSON tagged value) into our python model."""
    v = default_property(cls, canonical_name)
    if v is None:
        return None
    if isinstance(v, dict):
        if "Bool" in v:
            return v["Bool"]
        if "Int32" in v or "Int64" in v:
            return v.get("Int32", v.get("Int64"))
        if "Float32" in v or "Float64" in v:
            return v.get("Float32", v.get("Float64"))
        if "String" in v:
            return v["String"]
        if "Enum" in v:
            return v["Enum"]
        if "Vector3" in v:
            return tuple(v["Vector3"])
        if "Vector2" in v:
            return tuple(v["Vector2"])
        if "Color3" in v:
            return tuple(v["Color3"])
        if "CFrame" in v:
            c = v["CFrame"]
            return {
                "position": tuple(c["position"]),
                "orientation": tuple(tuple(r) for r in c["orientation"]),
            }
        if "PhysicalProperties" in v:
            return "Default"
        if "BrickColor" in v:
            return v["BrickColor"]
        if "Content" in v or "ContentId" in v:
            return ""
        if "UDim2" in v:
            return tuple(v["UDim2"])
        if "UDim" in v:
            return tuple(v["UDim"])
        if "Faces" in v or "Axes" in v:
            return v.get("Faces", v.get("Axes"))
        if "NumberRange" in v:
            return tuple(v["NumberRange"])
        if "Rect" in v:
            return tuple(v["Rect"])
        if "Ray" in v:
            r = v["Ray"]
            return (tuple(r["origin"]), tuple(r["direction"]))
        if "NumberSequence" in v:
            return [tuple(k) for k in v["NumberSequence"]]
        if "ColorSequence" in v:
            return [(k[0], tuple(k[1])) for k in v["ColorSequence"]]
        if "OptionalCFrame" in v:
            return v["OptionalCFrame"]
    return v
