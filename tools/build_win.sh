#!/bin/sh
# Cross-build the Windows 64-bit release of THE LAST NIGHT.
# Requires zig 0.17 (pip install ziglang) — it provides a full mingw-w64
# toolchain plus `zig rc` (Microsoft-compatible resource compiler).
set -e
if [ -z "$ZIG" ]; then
    ZIG="$(python3 -c "import ziglang, os; print(os.path.join(os.path.dirname(ziglang.__file__), 'zig'))")" || ZIG=zig
fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p build dist/win
SRC="src/*.c engine/src/core.c engine/src/render.c engine/src/threads.c engine/src/inflate.c engine/src/png.c engine/src/fileio.c engine/src/png_write.c engine/src/geom.c engine/src/anim.c engine/src/audio.c engine/platform/plat_win32.c"

echo "[1/3] compiling resources"
"$ZIG" rc /:output-format coff /:target x86_64 -fo build/win_res.obj src/win/the_last_night.rc

echo "[2/3] compiling and linking THE_LAST_NIGHT.exe"
"$ZIG" cc -target x86_64-windows-gnu -O2 -std=c11 -DTLN_GUI \
    -Wl,--subsystem,windows \
    -o dist/win/THE_LAST_NIGHT.exe $SRC build/win_res.obj \
    -lwinmm -lws2_32 -lgdi32 -luser32 -lshell32 -lole32

echo "[3/3] staging runtime files"
mkdir -p dist/win/assets
cp -r assets/tex assets/fonts assets/ui assets/models assets/audio dist/win/assets/ 2>/dev/null || true
cp assets/manifest.txt dist/win/assets/ 2>/dev/null || true
ls -la dist/win
