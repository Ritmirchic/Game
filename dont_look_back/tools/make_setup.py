#!/usr/bin/env python3
"""Append the game payload to the installer stub and write DONT_LOOK_BACK_SETUP.exe.

Payload format (read by tools/installer.cpp):
    per file: u16 nameLen | name utf-8 | u32 size | data
    footer:   u64 offset  | u32 count | b"DLBPAK01"
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")
STUB = os.path.join(DIST, "_setup_stub.exe")
OUT = os.path.join(DIST, "DONT_LOOK_BACK_SETUP.exe")

# what gets installed (paths inside the install folder, '/' separated)
ITEMS = [
    ("DONT_LOOK_BACK.exe", "DONT_LOOK_BACK.exe"),
    ("assets", "assets"),
    ("README.txt", "README.txt"),
    ("KNOWN_LIMITATIONS.txt", "KNOWN_LIMITATIONS.txt"),
    ("BUILD.md", "BUILD.md"),
    ("TEST_RESULTS.txt", "TEST_RESULTS.txt"),
]


def collect():
    files = []
    for src, dst in ITEMS:
        sp = os.path.join(DIST, src)
        if os.path.isfile(sp):
            files.append((dst, sp))
        elif os.path.isdir(sp):
            for base, _dirs, names in os.walk(sp):
                for n in sorted(names):
                    full = os.path.join(base, n)
                    rel = os.path.relpath(full, DIST).replace(os.sep, "/")
                    files.append((rel, full))
        else:
            print("  missing:", sp)
    return files


def main():
    if not os.path.isfile(STUB):
        print("build the stub first:", STUB)
        return 1
    files = collect()
    stub = open(STUB, "rb").read()
    offset = len(stub)
    blob = bytearray()
    total = 0
    for rel, full in files:
        data = open(full, "rb").read()
        name = rel.encode("utf-8")
        if len(name) > 65535:
            raise SystemExit("path too long: " + rel)
        blob += struct.pack("<H", len(name)) + name + struct.pack("<I", len(data)) + data
        total += len(data)
    footer = struct.pack("<QI8s", offset, len(files), b"DLBPAK01")
    with open(OUT, "wb") as f:
        f.write(stub)
        f.write(blob)
        f.write(footer)
    print("setup: %s" % OUT)
    print("  files %d, payload %.2f MB, stub %.1f KB, total %.2f MB"
          % (len(files), len(blob) / 1e6, len(stub) / 1024,
             (len(stub) + len(blob) + len(footer)) / 1e6))
    return 0


if __name__ == "__main__":
    sys.exit(main())
