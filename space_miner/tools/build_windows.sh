#!/usr/bin/env bash
# ============================================================
# SPACE MINER - Windows build
# Cross-compiles native 64-bit Windows binaries with zig:
#   SPACE_MINER.exe       - the game
#   SPACE_MINER_SETUP.exe - installer with the game embedded
# ============================================================
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)
ZIG=${ZIG:-/usr/local/lib/python3.11/dist-packages/ziglang/zig}
OUT=release
mkdir -p "$OUT" build
COMMON="-std=c++17 -I src -I build -O2"
SRC="src/core.cpp src/gfx.cpp src/texture.cpp src/art.cpp src/content.cpp src/game_core.cpp src/game_update.cpp src/game_render.cpp src/ui.cpp src/audio.cpp"

echo "==> [1/4] icon + resource metadata"
$ZIG c++ -target x86_64-windows-gnu $COMMON -o build/gen_assets.exe tools/gen_assets.cpp $SRC src/platform_headless.cpp || exit 1
echo "    (icon is also generated natively for the .ico resource)"
g++ -O2 -std=c++17 -I src -o build/gen_assets tools/gen_assets.cpp $SRC src/platform_headless.cpp || exit 1
./build/gen_assets build || exit 1

echo "==> [2/4] resource object (icon + version info)"
$ZIG rc /fo build/rc_resources.o build/space_miner.rc 2>&1 | tail -2 || \
  ( echo "    zig rc unavailable, embedding icon only through the manifest-less path" )

echo "==> [3/4] SPACE_MINER.exe"
$ZIG c++ -target x86_64-windows-gnu $COMMON -static -static-libgcc -static-libstdc++ \
  -Wl,--subsystem,windows -o "$OUT/SPACE_MINER.exe" src/main.cpp $SRC src/platform_win32.cpp \
  build/rc_resources.o -lgdi32 -luser32 -lwinmm -lshell32 -lole32 || exit 1

echo "==> [4/4] SPACE_MINER_SETUP.exe"
./build/gen_assets build || exit 1
$ZIG c++ -target x86_64-windows-gnu $COMMON -static -static-libgcc -static-libstdc++ \
  -Wl,--subsystem,windows -o "$OUT/SPACE_MINER_SETUP.exe" tools/installer_win32.cpp build/setup_payload.cpp \
  -lcomctl32 -lole32 -lshell32 -luser32 -lgdi32 -ladvapi32 || exit 1

ls -la "$OUT"
