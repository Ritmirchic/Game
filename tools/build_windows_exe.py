#!/usr/bin/env python3
"""
Builds standalone Windows 64-bit ALCHEMY_CHAOS.exe from Godot 4.5 Windows Release
GUI template + packed alchemy_chaos.pck, patching PE .rsrc icons (16..256px) and
VS_VERSIONINFO metadata + PE 'pck' section + GDPC trailer.
"""
import io
import os
import struct
import pefile
from PIL import Image


def align4(data: bytes) -> bytes:
    rem = len(data) % 4
    return data if rem == 0 else data + b"\x00" * (4 - rem)


def make_vs_node(key: str, value_bytes: bytes, w_type: int) -> bytes:
    """Build a standard Win32 VS_VERSIONINFO tree node (UTF-16LE aligned)."""
    key_u16 = key.encode("utf-16-le") + b"\x00\x00"
    header_and_key = struct.pack("<HHH", 0, len(value_bytes) // (2 if w_type == 1 else 1), w_type) + key_u16
    body = align4(header_and_key)
    if value_bytes:
        body = align4(body + value_bytes)
    return body


def make_vs_container(key: str, value_bytes: bytes, w_type: int, children: list[bytes]) -> bytes:
    key_u16 = key.encode("utf-16-le") + b"\x00\x00"
    val_len = len(value_bytes) // 2 if w_type == 1 else len(value_bytes)
    buf = struct.pack("<HHH", 0, val_len, w_type) + key_u16
    buf = align4(buf)
    if value_bytes:
        buf = align4(buf + value_bytes)
    for ch in children:
        buf = align4(buf) + ch
    total_len = len(buf)
    return struct.pack("<H", total_len) + buf[2:]


def make_string_entry(k: str, v: str) -> bytes:
    v_u16 = v.encode("utf-16-le") + b"\x00\x00"
    key_u16 = k.encode("utf-16-le") + b"\x00\x00"
    w_value_len = len(v) + 1  # in WCHARs
    buf = struct.pack("<HHH", 0, w_value_len, 1) + key_u16
    buf = align4(buf) + v_u16
    total_len = len(buf)
    return struct.pack("<H", total_len) + buf[2:]


def build_version_info_blob() -> bytes:
    strings = [
        ("CompanyName", "Sanctum Hermetic Studios"),
        ("FileDescription", "ALCHEMY CHAOS - Grand Hermetic Sandbox"),
        ("FileVersion", "1.0.0.0"),
        ("InternalName", "ALCHEMY_CHAOS"),
        ("LegalCopyright", "Copyright (C) 2026 Sanctum Hermetic Studios"),
        ("OriginalFilename", "ALCHEMY_CHAOS.exe"),
        ("ProductName", "ALCHEMY CHAOS"),
        ("ProductVersion", "1.0.0.0"),
    ]
    str_nodes = [make_string_entry(k, v) for k, v in strings]
    str_table = make_vs_container("040904b0", b"", 1, str_nodes)
    str_file_info = make_vs_container("StringFileInfo", b"", 1, [str_table])

    # VarFileInfo -> Translation = 0x0409, 0x04B0 (US English, Unicode)
    trans_val = struct.pack("<HH", 0x0409, 0x04B0)
    var_entry = make_vs_container("Translation", trans_val, 0, [])
    var_file_info = make_vs_container("VarFileInfo", b"", 1, [var_entry])

    # VS_FIXEDFILEINFO (52 bytes)
    fixed_info = struct.pack(
        "<13I",
        0xFEEF04BD,      # dwSignature
        0x00010000,      # dwStrucVersion
        0x00010000,      # dwFileVersionMS (1.0)
        0x00000000,      # dwFileVersionLS (0.0)
        0x00010000,      # dwProductVersionMS (1.0)
        0x00000000,      # dwProductVersionLS (0.0)
        0x0000003F,      # dwFileFlagsMask
        0x00000000,      # dwFileFlags
        0x00040004,      # dwFileOS (VOS_NT_WINDOWS32)
        0x00000001,      # dwFileType (VFT_APP)
        0x00000000,      # dwFileSubtype
        0x00000000,      # dwFileDateMS
        0x00000000,      # dwFileDateLS
    )
    return make_vs_container("VS_VERSION_INFO", fixed_info, 0, [str_file_info, var_file_info])


def build_exe(template_path: str, pck_path: str, icon_png_path: str, out_exe_path: str) -> None:
    with open(template_path, "rb") as f:
        exe_buf = bytearray(f.read())

    pe = pefile.PE(data=bytes(exe_buf), fast_load=False)
    assert pe.FILE_HEADER.Machine == 0x8664, "Template must be x86_64!"
    assert pe.OPTIONAL_HEADER.Subsystem == 2, "Template must be WINDOWS_GUI (Subsystem=2)!"

    rsrc_sec = [s for s in pe.sections if s.Name.startswith(b".rsrc")][0]
    pck_sec = [s for s in pe.sections if s.Name.startswith(b"pck")][0]

    def rva_to_file_offset(rva: int) -> int:
        return rsrc_sec.PointerToRawData + (rva - rsrc_sec.VirtualAddress)

    # 1. Patch all 6 RT_ICON entries + RT_GROUP_ICON in-place with our custom app_icon_256.png!
    src_icon = Image.open(icon_png_path).convert("RGBA")
    icon_data_entries = {}
    grp_data_entry = None
    ver_data_entry = None

    for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if entry.id == 3:  # RT_ICON
            for sub in entry.directory.entries:
                icon_data_entries[sub.id] = sub.directory.entries[0].data.struct
        elif entry.id == 14:  # RT_GROUP_ICON
            grp_data_entry = entry.directory.entries[0].directory.entries[0].data.struct
        elif entry.id == 16:  # RT_VERSION
            ver_data_entry = entry.directory.entries[0].directory.entries[0].data.struct

    sizes = [16, 32, 48, 64, 128, 256]
    grp_entries_bytes = bytearray()

    for idx, sz in enumerate(sizes, 1):
        d_struct = icon_data_entries[idx]
        max_slot = d_struct.Size
        resized = src_icon.resize((sz, sz), Image.Resampling.LANCZOS)

        # Try highest quality color count that fits cleanly inside the slot
        png_bytes = b""
        for colors in [256, 192, 128, 64, 48]:
            q = resized.quantize(colors=colors, method=Image.Quantize.FASTOCTREE)
            bio = io.BytesIO()
            q.save(bio, format="PNG", optimize=True)
            cand = bio.getvalue()
            if len(cand) <= max_slot:
                png_bytes = cand
                break
        assert png_bytes, f"Could not fit {sz}x{sz} icon in {max_slot} bytes!"

        foff = rva_to_file_offset(d_struct.OffsetToData)
        exe_buf[foff : foff + max_slot] = png_bytes + b"\x00" * (max_slot - len(png_bytes))
        # Update IMAGE_RESOURCE_DATA_ENTRY.Size
        struct.pack_into("<I", exe_buf, d_struct.get_file_offset() + 4, len(png_bytes))

        w_byte = 0 if sz == 256 else sz
        h_byte = 0 if sz == 256 else sz
        grp_entries_bytes += struct.pack("<BBBBHHIH", w_byte, h_byte, 0, 0, 1, 32, len(png_bytes), idx)

    grp_blob = struct.pack("<HHH", 0, 1, len(sizes)) + bytes(grp_entries_bytes)
    assert len(grp_blob) == grp_data_entry.Size
    grp_foff = rva_to_file_offset(grp_data_entry.OffsetToData)
    exe_buf[grp_foff : grp_foff + len(grp_blob)] = grp_blob

    # 2. Write custom VS_VERSIONINFO into the unused slack of RT_ICON #6's 14.8 KB slot inside .rsrc!
    ver_blob = build_version_info_blob()
    icon6_entry = icon_data_entries[6]
    icon6_used = struct.unpack_from("<I", exe_buf, icon6_entry.get_file_offset() + 4)[0]
    new_ver_rva = (icon6_entry.OffsetToData + icon6_used + 15) & ~15
    assert new_ver_rva + len(ver_blob) <= icon6_entry.OffsetToData + 14843, "Version blob does not fit in icon6 slack!"
    new_ver_foff = rva_to_file_offset(new_ver_rva)

    exe_buf[new_ver_foff : new_ver_foff + len(ver_blob)] = ver_blob
    # Update RT_VERSION IMAGE_RESOURCE_DATA_ENTRY (OffsetToData, Size)
    struct.pack_into("<II", exe_buf, ver_data_entry.get_file_offset(), new_ver_rva, len(ver_blob))

    # 3. Append PCK + GDPC trailer & update PE 'pck' section header
    with open(pck_path, "rb") as pf:
        pck_bytes = pf.read()
    assert pck_bytes[:4] == b"GDPC", "Invalid PCK magic!"
    rem8 = len(pck_bytes) % 8
    if rem8 != 0:
        pck_bytes += b"\x00" * (8 - rem8)

    trailer = struct.pack("<Q4s", len(pck_bytes), b"GDPC")
    embedded_chunk = pck_bytes + trailer

    pck_raw_ptr = len(exe_buf)
    pck_sec_foff = pck_sec.get_file_offset()
    # Update SizeOfRawData (+16) and PointerToRawData (+20) in IMAGE_SECTION_HEADER
    struct.pack_into("<II", exe_buf, pck_sec_foff + 16, len(embedded_chunk), pck_raw_ptr)

    # Zero OptionalHeader CheckSum (+64 from OptionalHeader start)
    opt_foff = pe.OPTIONAL_HEADER.get_file_offset()
    struct.pack_into("<I", exe_buf, opt_foff + 64, 0)

    with open(out_exe_path, "wb") as out_f:
        out_f.write(exe_buf)
        out_f.write(embedded_chunk)

    total_mb = os.path.getsize(out_exe_path) / (1024 * 1024)
    print(f"[EXE] Built {out_exe_path} ({total_mb:.2f} MiB, PCK={len(pck_bytes):,} bytes)")

    # 4. Re-parse with pefile to verify structural PE64 + Resource integrity!
    vpe = pefile.PE(out_exe_path, fast_load=False)
    assert vpe.FILE_HEADER.Machine == 0x8664
    assert vpe.OPTIONAL_HEADER.Subsystem == 2
    vstrings = {}
    if hasattr(vpe, "FileInfo"):
        for fi_list in vpe.FileInfo:
            for item in fi_list:
                if hasattr(item, "StringTable"):
                    for st in item.StringTable:
                        for k, v in st.entries.items():
                            vstrings[k.decode(errors="ignore")] = v.decode(errors="ignore")
    print("[EXE] Verified PE64 VersionInfo strings:", vstrings)
    assert vstrings.get("ProductName") == "ALCHEMY CHAOS"
    assert vstrings.get("OriginalFilename") == "ALCHEMY_CHAOS.exe"


if __name__ == "__main__":
    build_exe(
        "/tmp/godot_tools/windows_release_45.exe",
        "/tmp/alchemy_chaos.pck",
        "/home/user/Game/alchemy_chaos/assets/branding/app_icon_256.png",
        "/home/user/Game/ALCHEMY_CHAOS.exe",
    )
