"""Reader/writer for Roblox's binary model format (`.rbxl` / `.rbxm`).

Implements the format described by rojo-rbx/rbx-dom `docs/binary.md`.
The writer is used to build ASHES_OF_HUMANITY.rbxl from a python instance tree;
the reader exists so the build can be validated by parsing the produced file
back and comparing it to the source tree, and so reference files can be
round-tripped as an extra sanity check.
"""
import math
import struct

try:  # real LZ4 gives much smaller files; falls back to stored blocks
    import lz4.block as _lz4
except Exception:  # pragma: no cover
    _lz4 = None

from . import db

MAGIC = b"<roblox!"
SKIPPED_PROPERTIES = set()
SIG = bytes([0x89, 0xFF, 0x0D, 0x0A, 0x1A, 0x0A])


class Instance:
    """A minimal instance node: class, properties (canonical names) and children."""

    __slots__ = ("cls", "props", "children", "parent")

    def __init__(self, cls, name=None, **props):
        self.cls = cls
        self.props = {}
        if name is not None:
            self.props["Name"] = name
        self.props.update({k: v for k, v in props.items() if v is not None})
        self.children = []
        self.parent = None

    def add(self, *children):
        for c in children:
            c.parent = self
            self.children.append(c)
        return self

    def find(self, name):
        for c in self.children:
            if c.props.get("Name") == name:
                return c
        return None

    def __repr__(self):
        return "<%s %r>" % (self.cls, self.props.get("Name"))


# ---------------------------------------------------------------- primitives

def _transform_int32(x):
    x &= 0xFFFFFFFF
    if x >= 0x80000000:
        x -= 0x100000000
    return ((x << 1) ^ (x >> 31)) & 0xFFFFFFFF


def _untransform_int32(u):
    u &= 0xFFFFFFFF
    v = (u >> 1) ^ (-(u & 1) & 0xFFFFFFFF)
    if v >= 0x80000000:
        v -= 0x100000000
    return v


def _transform_int64(x):
    return ((x << 1) ^ (x >> 63)) & 0xFFFFFFFFFFFFFFFF


def _roblox_float_bytes(f):
    """Encode a python float into Roblox's sign-at-the-end 32-bit format."""
    if isinstance(f, int):
        f = float(f)
    raw = struct.unpack("<I", struct.pack("<f", f))[0]
    sign = (raw >> 31) & 1
    exp = (raw >> 23) & 0xFF
    mant = raw & 0x7FFFFF
    out = (exp << 24) | (mant << 1) | sign
    return struct.pack(">I", out & 0xFFFFFFFF)


def _from_roblox_float(b):
    v = struct.unpack(">I", b)[0]
    exp = (v >> 24) & 0xFF
    mant = (v >> 1) & 0x7FFFFF
    sign = v & 1
    raw = (sign << 31) | (exp << 23) | mant
    return struct.unpack("<f", struct.pack("<I", raw))[0]


def _interleave(chunks, width):
    """chunks is a list of equal-length byte strings; emit column-major."""
    out = bytearray()
    for col in range(width):
        for c in chunks:
            out += c[col:col + 1]
    return bytes(out)


def _interleave_structs(values, fmt):
    pack = struct.Struct(fmt)
    chunks = [pack.pack(v) for v in values]
    return _interleave(chunks, pack.size)


# ------------------------------------------------------------------ encoding

def _enc_string(v):
    if isinstance(v, bytes):
        b = v
    else:
        b = str(v).encode("utf8")
    return struct.pack("<I", len(b)) + b


def _enc_bool(v):
    return b"\x01" if v else b"\x00"


def _enc_int32(v):
    return struct.pack(">I", _transform_int32(int(v)))


def _enc_int32_array(values):
    return _interleave_structs([_transform_int32(int(v)) for v in values], ">I")


def _enc_int64_array(values):
    return _interleave_structs([_transform_int64(int(v)) for v in values], ">Q")


def _enc_f32_array(values):
    return _interleave([_roblox_float_bytes(float(v)) for v in values], 4)


def _enc_f64_array(values):
    return b"".join(struct.pack("<d", float(v)) for v in values)


def _f32_column(values):
    """One interleaved f32 component array (Roblox float format)."""
    if not values:
        return b""
    return _interleave([_roblox_float_bytes(float(v)) for v in values], 4)


def _enc_vector3_array(values):
    return (_f32_column([v[0] for v in values])
            + _f32_column([v[1] for v in values])
            + _f32_column([v[2] for v in values]))


def _enc_vector2_array(values):
    return _f32_column([v[0] for v in values]) + _f32_column([v[1] for v in values])


def _enc_color3_array(values):
    return (_f32_column([v[0] for v in values])
            + _f32_column([v[1] for v in values])
            + _f32_column([v[2] for v in values]))


def _enc_color3uint8_array(values):
    rs = bytes(int(v[0]) for v in values)
    gs = bytes(int(v[1]) for v in values)
    bs = bytes(int(v[2]) for v in values)
    return rs + gs + bs


def _enc_enum_array(values):
    return _interleave_structs([int(v) for v in values], ">I")


def _enc_ref_array(referents):
    """Referents are stored as accumulated deltas of transformed int32s."""
    deltas = []
    last = 0
    for r in referents:
        if r is None:
            r = -1
        # -1 is a legal absolute value only for the first entry; encode deltas
        deltas.append(r - last)
        last = r
    return _enc_int32_array(deltas)


def _enc_cframe_array(values):
    ids = bytearray()
    rots = bytearray()
    positions = []
    for cf in values:
        pos, rot = _split_cframe(cf)
        ids.append(0x00)
        if rot is None:
            # unreachable (we always write full matrices), keep for clarity
            for f in (1, 0, 0, 0, 1, 0, 0, 0, 1):
                rots += struct.pack("<f", float(f))
        else:
            for i in range(3):
                for j in range(3):
                    rots += struct.pack("<f", float(rot[i][j]))
        positions.append(pos)
    return bytes(ids) + bytes(rots) + _enc_vector3_array(positions)


def _split_cframe(cf):
    if isinstance(cf, dict):
        return tuple(cf["position"]), tuple(tuple(r) for r in cf["orientation"])
    if isinstance(cf, (tuple, list)) and len(cf) == 2:
        pos = tuple(cf[0])
        rot = cf[1]
        if rot is None:
            return pos, ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
        return pos, tuple(tuple(r) for r in rot)
    raise ValueError("bad cframe %r" % (cf,))


def _enc_number_sequence(values):
    out = bytearray()
    for seq in values:
        keys = seq if isinstance(seq, list) else [seq]
        out += struct.pack("<I", len(keys))
        for kp in keys:
            t, v, e = (list(kp) + [0.0])[:3]
            out += struct.pack("<fff", float(t), float(v), float(e))
    return bytes(out)


def _enc_color_sequence(values):
    out = bytearray()
    for seq in values:
        keys = seq if isinstance(seq, list) else [seq]
        out += struct.pack("<I", len(keys))
        for kp in keys:
            t = kp[0]
            col = kp[1]
            out += struct.pack("<ffff", float(t), float(col[0]), float(col[1]), float(col[2]))
            out += struct.pack("<f", 0.0)
    return bytes(out)


def _enc_number_range(values):
    out = bytearray()
    for v in values:
        lo, hi = v if isinstance(v, (tuple, list)) else (v, v)
        out += struct.pack("<ff", float(lo), float(hi))
    return bytes(out)


def _enc_rect(values):
    return b"".join(_f32_column([v[i] for v in values]) for i in range(4))


def _enc_physical_properties(values):
    out = bytearray()
    for v in values:
        if v in (None, "Default", False):
            out.append(0x00)
            continue
        density, friction, elasticity, fw, ew, absorb = (list(v) + [None] * 6)[:6]
        flags = 0x01
        if absorb is not None:
            flags |= 0x02
        out.append(flags)
        out += struct.pack("<fff", float(density), float(friction), float(elasticity))
        out += struct.pack("<ff", float(fw), float(ew))
        if absorb is not None:
            out += struct.pack("<f", float(absorb))
    return bytes(out)


def _enc_udim_array(values):
    scales = [_roblox_float_bytes(float(v[0])) for v in values]
    offsets = [_transform_int32(int(v[1])) for v in values]
    return _interleave(scales, 4) + _interleave_structs(offsets, ">I")


def _enc_udim2_array(values):
    xs = [_roblox_float_bytes(float(v[0])) for v in values]
    ys = [_roblox_float_bytes(float(v[2])) for v in values]
    xo = [_transform_int32(int(v[1])) for v in values]
    yo = [_transform_int32(int(v[3])) for v in values]
    return (_interleave(xs, 4) + _interleave(ys, 4)
            + _interleave_structs(xo, ">I") + _interleave_structs(yo, ">I"))


def _enc_ray_array(values):
    out = bytearray()
    for v in values:
        org, direction = v
        out += struct.pack("<ffffff", *[float(c) for c in org], *[float(c) for c in direction])
    return bytes(out)


def _enc_optional_cframe(values):
    cfs = [v if v is not None else ((0, 0, 0), ((1, 0, 0), (0, 1, 0), (0, 0, 1))) for v in values]
    body = _enc_cframe_array(cfs)
    flags = bytes(0x01 if v is not None else 0x00 for v in values)
    return bytes([db.WIRE_CFRAME]) + body + bytes([db.WIRE_BOOL]) + flags


def _enc_shared_string(values):
    # Only used when writing; our build never emits shared strings.
    idx = [0 for _ in values]
    return _interleave_structs(idx, ">I")


ENCODERS = {
    db.WIRE_STRING: lambda vs: b"".join(_enc_string(v) for v in vs),
    db.WIRE_BOOL: lambda vs: b"".join(_enc_bool(v) for v in vs),
    db.WIRE_INT32: _enc_int32_array,
    db.WIRE_FLOAT32: _enc_f32_array,
    db.WIRE_FLOAT64: _enc_f64_array,
    db.WIRE_UDIM: _enc_udim_array,
    db.WIRE_UDIM2: _enc_udim2_array,
    db.WIRE_RAY: _enc_ray_array,
    db.WIRE_FACES: lambda vs: bytes(int(v) for v in vs),
    db.WIRE_AXES: lambda vs: bytes(int(v) for v in vs),
    db.WIRE_BRICKCOLOR: lambda vs: _interleave_structs([int(v) for v in vs], ">I"),
    db.WIRE_COLOR3: _enc_color3_array,
    db.WIRE_VECTOR2: _enc_vector2_array,
    db.WIRE_VECTOR3: _enc_vector3_array,
    db.WIRE_CFRAME: _enc_cframe_array,
    db.WIRE_ENUM: _enc_enum_array,
    db.WIRE_REF: _enc_ref_array,
    db.WIRE_VECTOR3INT16: lambda vs: b"".join(struct.pack("<hhh", *[int(c) for c in v]) for v in vs),
    db.WIRE_NUMBERSEQUENCE: _enc_number_sequence,
    db.WIRE_COLORSEQUENCE: _enc_color_sequence,
    db.WIRE_NUMBERRANGE: _enc_number_range,
    db.WIRE_RECT: _enc_rect,
    db.WIRE_PHYSICALPROPERTIES: _enc_physical_properties,
    db.WIRE_COLOR3UINT8: _enc_color3uint8_array,
    db.WIRE_INT64: _enc_int64_array,
    db.WIRE_SHAREDSTRING: _enc_shared_string,
    db.WIRE_OPTIONALCFRAME: _enc_optional_cframe,
}


# ------------------------------------------------------------------ decoding

def _dec_string(buf, off):
    n = struct.unpack_from("<I", buf, off)[0]
    off += 4
    return buf[off:off + n].decode("utf8", "replace"), off + n


def _deinterleave(data, count, width):
    out = []
    for i in range(count):
        chunk = bytearray(width)
        for col in range(width):
            chunk[col] = data[col * count + i]
        out.append(bytes(chunk))
    return out


def _dec_int32_array(data, count):
    vals = _deinterleave(data, count, 4)
    return [_untransform_int32(struct.unpack(">I", v)[0]) for v in vals]


def _dec_float32_array(data, count):
    return [_from_roblox_float(v) for v in _deinterleave(data, count, 4)]


def _dec_f32_column(data, off, count):
    if count == 0:
        return [], off
    words = _deinterleave(data[off:off + count * 4], count, 4)
    return [_from_roblox_float(w) for w in words], off + count * 4


def _dec_vector3_array(data, offset=0, count=None):
    xs, o = _dec_f32_column(data, offset, count)
    ys, o = _dec_f32_column(data, o, count)
    zs, o = _dec_f32_column(data, o, count)
    return list(zip(xs, ys, zs))


def _dec_color3_array(data, count):
    xs, o = _dec_f32_column(data, 0, count)
    ys, o = _dec_f32_column(data, o, count)
    zs, o = _dec_f32_column(data, o, count)
    return list(zip(xs, ys, zs))


def _dec_cframe_array(data, off, count):
    ids = []
    for _ in range(count):
        ids.append(data[off])
        off += 1
    rots = []
    for k in range(count):
        if ids[k] == 0:
            vals = struct.unpack_from("<9f", data, off)
            off += 36
            rots.append((vals[0:3], vals[3:6], vals[6:9]))
        else:
            rots.append(None)
    pos = _dec_vector3_array(data, off, count)
    off += count * 12
    out = []
    for k in range(count):
        out.append((pos[k], rots[k]))
    return out, off


def decode_property(wire, data, count, ctx):
    """Return a list of python values (or raise on unsupported type)."""
    off = 0
    if wire == db.WIRE_STRING:
        out = []
        for _ in range(count):
            s, off = _dec_string(data, off)
            out.append(s)
        return out
    if wire == db.WIRE_BOOL:
        return [bool(b) for b in data[:count]]
    if wire == db.WIRE_INT32:
        return _dec_int32_array(data, count)
    if wire == db.WIRE_FLOAT32:
        return _dec_float32_array(data, count)
    if wire == db.WIRE_FLOAT64:
        return [struct.unpack_from("<d", data, i * 8)[0] for i in range(count)]
    if wire == db.WIRE_VECTOR3:
        return _dec_vector3_array(data, 0, count)
    if wire == db.WIRE_VECTOR2:
        xs, o = _dec_f32_column(data, 0, count)
        ys, o = _dec_f32_column(data, o, count)
        return list(zip(xs, ys))
    if wire == db.WIRE_COLOR3:
        return _dec_color3_array(data, count)
    if wire == db.WIRE_COLOR3UINT8:
        r = data[0:count]
        g = data[count:count * 2]
        b = data[count * 2:count * 3]
        return [(r[i], g[i], b[i]) for i in range(count)]
    if wire == db.WIRE_ENUM:
        return [struct.unpack(">I", v)[0] for v in _deinterleave(data, count, 4)]
    if wire == db.WIRE_CFRAME:
        vals, _ = _dec_cframe_array(data, 0, count)
        return vals
    if wire == db.WIRE_REF:
        deltas = _dec_int32_array(data, count)
        out = []
        acc = 0
        for d in deltas:
            acc += d
            out.append(None if acc == -1 else acc)
        return out
    if wire == db.WIRE_NUMBERSEQUENCE:
        out = []
        for _ in range(count):
            n = struct.unpack_from("<I", data, off)[0]
            off += 4
            keys = []
            for _k in range(n):
                t, v, e = struct.unpack_from("<fff", data, off)
                off += 12
                keys.append((t, v, e))
            out.append(keys)
        return out
    if wire == db.WIRE_COLORSEQUENCE:
        out = []
        for _ in range(count):
            n = struct.unpack_from("<I", data, off)[0]
            off += 4
            keys = []
            for _k in range(n):
                t, r, g, b, e = struct.unpack_from("<fffff", data, off)
                off += 20
                keys.append((t, (r, g, b)))
            out.append(keys)
        return out
    if wire == db.WIRE_NUMBERRANGE:
        return [struct.unpack_from("<ff", data, i * 8) for i in range(count)]
    if wire == db.WIRE_RECT:
        a, o = _dec_f32_column(data, 0, count)
        b, o = _dec_f32_column(data, o, count)
        c, o = _dec_f32_column(data, o, count)
        d, o = _dec_f32_column(data, o, count)
        return list(zip(a, b, c, d))
    if wire == db.WIRE_UDIM:
        scales = _deinterleave(data, count, 4)
        offsets = _deinterleave(data[count * 4:], count, 4)
        return [(_from_roblox_float(scales[i]), _untransform_int32(struct.unpack(">I", offsets[i])[0]))
                for i in range(count)]
    if wire == db.WIRE_UDIM2:
        sc = _deinterleave(data[0:count * 8], count, 4)
        ys = _deinterleave(data[count * 4:count * 8], count, 4)
        xo = _deinterleave(data[count * 8:count * 12], count, 4)
        yo = _deinterleave(data[count * 12:count * 16], count, 4)
        return [(_from_roblox_float(sc[i]), _untransform_int32(struct.unpack(">I", xo[i])[0]),
                 _from_roblox_float(ys[i]), _untransform_int32(struct.unpack(">I", yo[i])[0]))
                for i in range(count)]
    if wire == db.WIRE_PHYSICALPROPERTIES:
        out = []
        for _ in range(count):
            flags = data[off]
            off += 1
            if flags & 1:
                d, f, e, fw, ew = struct.unpack_from("<fffff", data, off)
                off += 20
                absorb = None
                if flags & 2:
                    absorb = struct.unpack_from("<f", data, off)[0]
                    off += 4
                out.append((d, f, e, fw, ew, absorb))
            else:
                out.append("Default")
        return out
    if wire == db.WIRE_INT64:
        return [_untransform_int64_signed(struct.unpack(">Q", v)[0]) for v in _deinterleave(data, count, 8)]
    if wire == db.WIRE_BRICKCOLOR:
        return [struct.unpack(">I", v)[0] for v in _deinterleave(data, count, 4)]
    if wire == db.WIRE_VECTOR3INT16:
        return [struct.unpack_from("<hhh", data, i * 6) for i in range(count)]
    if wire == db.WIRE_RAY:
        out = []
        for i in range(count):
            vals = struct.unpack_from("<ffffff", data, i * 24)
            out.append((vals[0:3], vals[3:6]))
        return out
    if wire == db.WIRE_SHAREDSTRING:
        sstr = (ctx or {}).get("sstr", [])
        idx = _dec_int32_array(data, count)
        return [sstr[i] if 0 <= i < len(sstr) else None for i in idx]
    # Unknown / non-critical property type: the reader is only used for
    # build validation, so keep the rest of the parse going.
    return [None] * count


def _untransform_int64(u):
    v = (u >> 1) ^ (0xFFFFFFFFFFFFFFFF if (u & 1) else 0)
    return v


def _untransform_int64_signed(u):
    v = (u >> 1) ^ (-(u & 1) & 0xFFFFFFFFFFFFFFFF)
    if v >= 0x8000000000000000:
        v -= 0x10000000000000000
    return v


# ------------------------------------------------------------------ chunks

def compress_chunk(data):
    if _lz4 is not None and len(data) > 256:
        comp = _lz4.compress(data, store_size=False)
        if len(comp) < len(data):
            # verify before trusting
            if _lz4.decompress(comp, uncompressed_size=len(data)) == data:
                return True, comp
    return False, data


def write_chunk(name, data):
    name_b = name.encode("ascii").ljust(4, b"\x00")
    compressed, payload = compress_chunk(data)
    comp_len = len(payload) if compressed else 0
    return name_b + struct.pack("<II", comp_len, len(data)) + b"\x00\x00\x00\x00" + payload


def read_chunks(buf):
    off = 32
    chunks = {}
    while off < len(buf):
        name = buf[off:off + 4].rstrip(b"\x00").decode("ascii")
        comp_len, uncomp_len = struct.unpack_from("<II", buf, off + 4)
        off += 16
        raw = buf[off:off + (comp_len if comp_len else uncomp_len)]
        off += len(raw)
        if comp_len:
            if raw[:4] == b"\x28\xb5\x2f\xfd":
                import zstandard
                raw = zstandard.ZstdDecompressor().decompress(raw, max_output_size=uncomp_len)
            else:
                raw = _lz4.decompress(raw, uncompressed_size=uncomp_len)
        chunks.setdefault(name, []).append(raw)
        if name == "END":
            break
    return chunks


# ------------------------------------------------------------------- writer

UNSAFE_WIRES = {db.WIRE_SHAREDSTRING, db.WIRE_BYTECODE, db.WIRE_CONTENT, db.WIRE_UNIQUEID,
                db.WIRE_FONT, db.WIRE_FACES, db.WIRE_AXES}


def write_place(root_children, path):
    """root_children: list of Instance that become children of DataModel."""
    root = Instance("DataModel")
    root.add(*root_children)

    # Flatten depth-first, assign referents
    order = []

    def walk(node):
        for c in node.children:
            walk(c)
        if node is not root:
            order.append(node)

    walk(root)
    ref_of = {}
    for idx, node in enumerate(order):
        ref_of[id(node)] = idx

    # group by class
    classes = []
    by_class = {}
    for node in order:
        if node.cls not in by_class:
            by_class[node.cls] = []
            classes.append(node.cls)
        by_class[node.cls].append(node)

    header = MAGIC + SIG + struct.pack("<H", 0) + struct.pack("<i", len(classes)) \
        + struct.pack("<i", len(order)) + b"\x00" * 8

    out = bytearray(header)

    # META
    meta = struct.pack("<I", 1) + _enc_string("ExplicitAutoJoints") + _enc_string("true")
    out += write_chunk("META", meta)

    class_ids = {c: i for i, c in enumerate(classes)}

    # INST chunks
    for cls in classes:
        nodes = by_class[cls]
        data = struct.pack("<I", class_ids[cls]) + _enc_string(cls) + b"\x00"
        data += struct.pack("<I", len(nodes))
        refs = [ref_of[id(n)] for n in nodes]
        last = 0
        deltas = []
        for r in refs:
            deltas.append(r - last)
            last = r
        data += _enc_int32_array(deltas)
        out += write_chunk("INST", data)

    # PROP chunks
    services = {"Workspace", "Lighting", "ReplicatedStorage", "ReplicatedFirst",
                "ServerScriptService", "ServerStorage", "StarterGui", "StarterPack",
                "StarterPlayer", "SoundService", "Players", "Teams", "Chat",
                "TextChatService", "MaterialService", "Lighting"}

    for cls in classes:
        nodes = by_class[cls]
        ser = db.serializable_properties(cls)
        # gather canonical props used by any instance of this class
        used = set()
        for n in nodes:
            used.update(n.props.keys())
        # set canonical -> serialized + wire
        for canon in sorted(used):
            entry = None
            for sname, (cname, wire, prop) in ser.items():
                if cname == canon:
                    entry = (sname, wire)
                    break
            if entry is None or entry[1] is None or entry[1] not in ENCODERS \
                    or entry[1] in UNSAFE_WIRES:
                SKIPPED_PROPERTIES.add("%s.%s" % (cls, canon))
                continue
            sname, wire = entry
            values = []
            missing = False
            for n in nodes:
                if canon in n.props and n.props[canon] is not None:
                    values.append(n.props[canon])
                else:
                    d = db.default_to_python(cls, canon)
                    if d is None:
                        missing = True
                        break
                    values.append(d)
            if missing:
                SKIPPED_PROPERTIES.add("%s.%s" % (cls, canon))
                continue
            if wire == db.WIRE_REF:
                values = [ref_of.get(id(v)) if isinstance(v, Instance) else v for v in values]
                values = [v if isinstance(v, int) or v is None else -1 for v in values]
            enc = ENCODERS[wire](values)
            data = struct.pack("<I", class_ids[cls]) + _enc_string(sname) + bytes([wire]) + enc
            out += write_chunk("PROP", data)

    # PRNT in depth-first post-order
    child_refs = []
    parent_refs = []
    for node in order:
        child_refs.append(ref_of[id(node)])
        parent_refs.append(ref_of[id(node.parent)] if node.parent is not root else -1)

    def enc_ref_list(refs):
        last = 0
        deltas = []
        for r in refs:
            deltas.append(r - last)
            last = r
        return _enc_int32_array(deltas)

    prnt = b"\x00" + struct.pack("<I", len(order)) + enc_ref_list(child_refs) + enc_ref_list(parent_refs)
    out += write_chunk("PRNT", prnt)

    # END
    out += b"END\x00" + struct.pack("<II", 0, 9) + b"\x00" * 4 + b"</roblox>"

    with open(path, "wb") as fh:
        fh.write(out)
    return len(out), len(order), len(classes)


# ------------------------------------------------------------------- reader

def read_file(path):
    with open(path, "rb") as fh:
        buf = fh.read()
    return read_bytes(buf)


def read_bytes(buf):
    if buf[:8] != MAGIC:
        raise ValueError("not a binary roblox file")
    class_count, instance_count = struct.unpack_from("<ii", buf, 16)
    chunks = read_chunks(buf)

    sstr = []
    for data in chunks.get("SSTR", []):
        o = 4
        n, o = struct.unpack_from("<I", data, o)[0], o + 4
        for _ in range(n):
            o += 16  # md5
            v, o = _dec_string(data, o)
            sstr.append(v)
    ctx = {"sstr": sstr}

    nodes = []
    class_of = {}
    off = 0
    for data in chunks.get("INST", []):
        off = 0
        class_id, off = struct.unpack_from("<I", data, off)[0], off + 4
        cls, off = _dec_string(data, off)
        obj_format = data[off]
        off += 1
        count = struct.unpack_from("<I", data, off)[0]
        off += 4
        deltas = _dec_int32_array(data[off:off + count * 4], count)
        off += count * 4
        refs = []
        acc = 0
        for d in deltas:
            acc += d
            refs.append(acc)
        if obj_format == 1:
            off += count
        for r in refs:
            class_of[r] = cls
        nodes.append((class_id, cls, refs))

    max_ref = max((max(refs) for _, _, refs in nodes if refs), default=-1)
    inst = [None] * (max_ref + 1)
    for _cid, cls, refs in nodes:
        for r in refs:
            inst[r] = Instance(cls)

    for data in chunks.get("PROP", []):
        off = 0
        class_id = struct.unpack_from("<I", data, off)[0]
        off += 4
        pname, off = _dec_string(data, off)
        wire = data[off]
        off += 1
        refs = None
        for cid, cls, rs in nodes:
            if cid == class_id:
                refs = rs
                cls_name = cls
                break
        count = len(refs)
        values = decode_property(wire, data[off:], count, ctx)
        canonical = None
        ser = db.serializable_properties(cls_name)
        if pname in ser:
            canonical = ser[pname][0]
        else:
            canonical = pname
        if wire == db.WIRE_REF:
            values = [inst[v] if isinstance(v, int) and v >= 0 else None for v in values]
        for r, v in zip(refs, values):
            inst[r].props[canonical] = v

    prnt = chunks["PRNT"][0]
    off = 1
    n = struct.unpack_from("<I", prnt, off)[0]
    off += 4
    cd = _dec_int32_array(prnt[off:off + n * 4], n)
    off += n * 4
    pd = _dec_int32_array(prnt[off:off + n * 4], n)
    child_list, parent_list = [], []
    acc = 0
    for d in cd:
        acc += d
        child_list.append(acc)
    acc = 0
    for d in pd:
        acc += d
        parent_list.append(acc)

    roots = []
    for child, parent in zip(child_list, parent_list):
        node = inst[child]
        if parent == -1:
            roots.append(node)
        else:
            p = inst[parent]
            node.parent = p
            p.children.append(node)
    return roots
