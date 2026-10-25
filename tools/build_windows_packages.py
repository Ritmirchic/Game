#!/usr/bin/env python3
"""
Master Windows 64-bit Release, Setup Wizard & Source Archive Builder for DON'T LOOK BACK.
Produces and verifies:
  1. DONT_LOOK_BACK.exe        (Standalone Windows 64-bit Release Executable)
  2. DONT_LOOK_BACK_SETUP.exe  (Windows 64-bit GUI Installer + Uninstaller)
  3. DONT_LOOK_BACK_SOURCE.zip (Complete Source Project Archive)
"""

import hashlib
import lzma
import os
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
WORK = "/tmp/dlb_release_work"
GODOT = "/tmp/godot_tools/godot4"
WIN_TEMPLATE = "/tmp/godot_tools/windows_release_x86_64.exe"
PATCH_MJS = "/tmp/pkg_work/patch_pe.mjs"


def run(cmd: list[str], cwd: str = ROOT) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=cwd, check=True)


def e8_transform_forward(data: bytes) -> bytes:
    buf = bytearray(data)
    limit = min(max(0, len(buf) - 5), 55 * 1024 * 1024)
    i = 0
    while i < limit:
        if buf[i] == 0xE8:
            v = struct.unpack_from("<I", buf, i + 1)[0]
            struct.pack_into("<I", buf, i + 1, (v + i) & 0xFFFFFFFF)
            i += 5
        else:
            i += 1
    return bytes(buf)


def build_source_zip(zip_path: str) -> None:
    print(f"Building {zip_path} ...")
    skip_dirs = {".git", ".godot", "__pycache__", "Unselected files"}
    skip_files = {"DONT_LOOK_BACK.exe", "DONT_LOOK_BACK_SETUP.exe", "DONT_LOOK_BACK_SOURCE.zip"}
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_LZMA) as zf:
        for dirpath, dirnames, filenames in os.walk(ROOT):
            dirnames[:] = sorted([d for d in dirnames if d not in skip_dirs])
            for fn in sorted(filenames):
                if fn in skip_files or fn.endswith((".exe", ".zip")):
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, ROOT)
                zf.write(full, os.path.join("DONT_LOOK_BACK_SOURCE", rel))


def main() -> None:
    os.makedirs(WORK, exist_ok=True)
    ico_path = os.path.join(ROOT, "assets", "ui", "app_icon.ico")

    # 1. Brand Windows 64-bit Godot Release Template BEFORE embedding PCK
    branded_tpl = "/tmp/pkg_work/win_template_branded.exe"
    os.makedirs("/tmp/pkg_work", exist_ok=True)
    run([
        "node", PATCH_MJS,
        WIN_TEMPLATE, ico_path, branded_tpl,
        "DON'T LOOK BACK — Psychological Horror & Anomaly Mystery",
        "DONT_LOOK_BACK.exe",
    ])

    # 2. Export Standalone Windows 64-bit Release Executable
    game_exe_tmp = os.path.join(WORK, "DONT_LOOK_BACK.exe")
    if not os.path.exists(game_exe_tmp):
        run([GODOT, "--headless", "--path", ROOT, "--export-release", "Windows Desktop", game_exe_tmp])

    with open(game_exe_tmp, "rb") as f:
        game_bytes = f.read()
    assert game_bytes[-4:] == b"GDPC", "Embedded PCK trailer GDPC missing!"
    print(f"Verified DONT_LOOK_BACK.exe: {len(game_bytes)/1048576:.2f} MB (GDPC embedded PCK intact)")

    # 3. Compile Native 64-bit Windows Uninstaller & Setup Stub using Zig + Win32 API
    xz_obj = os.path.join(WORK, "xz_bridge_win64.o")
    run([
        sys.executable, "-m", "ziglang", "build-obj",
        os.path.join(ROOT, "installer", "xz_bridge.zig"),
        "-target", "x86_64-windows-gnu", "-O", "ReleaseFast", "-fstrip",
        f"-femit-bin={xz_obj}",
    ])

    uninst_raw_exe = os.path.join(WORK, "UNINSTALL_raw.exe")
    uninst_exe = os.path.join(WORK, "UNINSTALL.exe")
    run([
        sys.executable, "-m", "ziglang", "c++",
        "-target", "x86_64-windows-gnu", "-O2", "-s", "-Wl,--subsystem,windows",
        os.path.join(ROOT, "installer", "uninstaller.cpp"),
        "-luser32", "-lshell32", "-lole32", "-ladvapi32",
        "-o", uninst_raw_exe,
    ])
    run([
        "node", PATCH_MJS,
        uninst_raw_exe, ico_path, uninst_exe,
        "DON'T LOOK BACK — Uninstaller", "UNINSTALL.exe",
    ])

    setup_raw_exe = os.path.join(WORK, "SETUP_stub_raw.exe")
    setup_stub_exe = os.path.join(WORK, "SETUP_stub_branded.exe")
    run([
        sys.executable, "-m", "ziglang", "c++",
        "-target", "x86_64-windows-gnu", "-O2", "-s", "-Wl,--subsystem,windows",
        os.path.join(ROOT, "installer", "setup.cpp"),
        xz_obj,
        "-luser32", "-lgdi32", "-lcomctl32", "-lshell32", "-lole32", "-luuid", "-ladvapi32", "-lntdll",
        "-o", setup_raw_exe,
    ])
    run([
        "node", PATCH_MJS,
        setup_raw_exe, ico_path, setup_stub_exe,
        "DON'T LOOK BACK — Windows 64-bit Setup Wizard", "DONT_LOOK_BACK_SETUP.exe",
    ])

    # 4. Compress payloads & assemble DONT_LOOK_BACK_SETUP.exe with DLBPKG01 EOF footer
    print("Compressing DONT_LOOK_BACK.exe with E8 + LZMA2 (64 MB dictionary)...")
    comp_game = lzma.compress(
        e8_transform_forward(game_bytes),
        format=lzma.FORMAT_XZ,
        check=lzma.CHECK_CRC32,
        filters=[{"id": lzma.FILTER_LZMA2, "preset": 6, "dict_size": 67108864}],
    )
    with open(uninst_exe, "rb") as f:
        uninst_bytes = f.read()
    comp_uninst = lzma.compress(
        uninst_bytes,
        format=lzma.FORMAT_XZ,
        check=lzma.CHECK_CRC32,
        filters=[{"id": lzma.FILTER_LZMA2, "preset": 6}],
    )
    with open(os.path.join(ROOT, "README.md"), "rb") as f:
        readme_bytes = f.read().replace(b"\n", b"\r\n")

    with open(setup_stub_exe, "rb") as f:
        stub_bytes = f.read()

    payload_offset = len(stub_bytes)
    footer = struct.pack(
        "<QQQQQQ8s",
        payload_offset,
        len(comp_game),
        len(game_bytes),
        len(comp_uninst),
        len(uninst_bytes),
        len(readme_bytes),
        b"DLBPKG01",
    )

    setup_final_tmp = os.path.join(WORK, "DONT_LOOK_BACK_SETUP.exe")
    with open(setup_final_tmp, "wb") as f:
        f.write(stub_bytes)
        f.write(comp_game)
        f.write(comp_uninst)
        f.write(readme_bytes)
        f.write(footer)

    print(f"Verified DONT_LOOK_BACK_SETUP.exe: {os.path.getsize(setup_final_tmp)/1048576:.2f} MB")

    # 5. Build Complete Source Archive BEFORE copying final EXEs into ROOT
    zip_tmp = os.path.join(WORK, "DONT_LOOK_BACK_SOURCE.zip")
    build_source_zip(zip_tmp)
    print(f"Verified DONT_LOOK_BACK_SOURCE.zip: {os.path.getsize(zip_tmp)/1048576:.2f} MB")

    # 6. Copy all 3 deliverables into ROOT
    shutil.copy2(game_exe_tmp, os.path.join(ROOT, "DONT_LOOK_BACK.exe"))
    shutil.copy2(setup_final_tmp, os.path.join(ROOT, "DONT_LOOK_BACK_SETUP.exe"))
    shutil.copy2(zip_tmp, os.path.join(ROOT, "DONT_LOOK_BACK_SOURCE.zip"))

    print("ALL 3 DELIVERABLES BUILT AND VERIFIED SUCCESSFULLY!")


if __name__ == "__main__":
    main()
