#!/usr/bin/env bash
# Installs the Zig cross-toolchain used to build SPACE MINER for Windows x64.
# Zig ships its own clang + lld + mingw-w64 CRT, so no other build deps exist.
set -euo pipefail
DEST="${1:-/tmp/sm_toolchain}"
mkdir -p "$DEST"
if [ ! -x "$DEST/zig/zig" ]; then
  python3 -m pip download --no-deps -d "$DEST/pkg" "ziglang==0.13.0"
  cd "$DEST" && python3 -m zipfile -e pkg/ziglang-*.whl .
  mv ziglang zig
fi
"$DEST/zig/zig" version
