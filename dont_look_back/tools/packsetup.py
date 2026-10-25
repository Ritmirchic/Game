#!/usr/bin/env python3
"""Build DONT_LOOK_BACK_SETUP.exe: append a self-extracting payload to the installer stub."""
import os, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")

def payload_entries():
    entries = []
    entries.append(("DONT_LOOK_BACK.exe", os.path.join(DIST, "DONT_LOOK_BACK.exe")))
    entries.append(("README.txt", os.path.join(DIST, "README.txt")))
    assets = os.path.join(ROOT, "assets")
    for dirpath, _, files in os.walk(assets):
        for f in sorted(files):
            if f.endswith(".nfp") or f.endswith(".png") or f.endswith(".ico"):
                full = os.path.join(dirpath, f)
                rel = os.path.relpath(full, ROOT).replace(os.sep, "/")
                entries.append((rel, full))
    return entries

def main():
    stub = os.path.join(DIST, "_setup_stub.exe")
    out = os.path.join(DIST, "DONT_LOOK_BACK_SETUP.exe")
    if not os.path.exists(stub):
        print("missing", stub); return 1
    blob = open(stub, "rb").read()
    entries = payload_entries()
    body = bytearray()
    count = 0
    for rel, full in entries:
        data = open(full, "rb").read()
        rb = rel.encode("utf-8")
        body += struct.pack("<I", len(rb)) + rb + struct.pack("<I", len(data)) + data
        count += 1
    offset = len(blob)
    footer = struct.pack("<Q", offset) + struct.pack("<I", count) + b"DLBPAK01" + struct.pack("<I", 0)
    out_bytes = blob + bytes(body) + footer
    open(out, "wb").write(out_bytes)
    print("wrote %s  (%d files, %.1f MB, payload at %d)" % (out, count, len(out_bytes)/1e6, offset))
    return 0

if __name__ == "__main__":
    sys.exit(main())
