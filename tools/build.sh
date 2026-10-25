#!/usr/bin/env bash
# ============================================================================
#  SPACE MINER — build script
#    tools/build.sh headless   -> build/sm_headless  (dev harness, native Linux)
#    tools/build.sh win        -> dist/SPACE_MINER.exe (Windows x64, no console)
#  Requires the Zig toolchain (see tools/setup_toolchain.sh).
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."

ZIG="${ZIG:-/tmp/sm_toolchain/zig/zig}"
[ -x "$ZIG" ] || ZIG=/tmp/venv/lib/python3.11/site-packages/ziglang/zig
if [ ! -x "$ZIG" ]; then
  echo "Zig toolchain not found. Run tools/setup_toolchain.sh first." >&2
  exit 1
fi

CORE_SRC=(
  src/core/common.c src/core/inflate.c src/core/png.c src/core/gfx.c
  src/core/pack.c src/core/input.c src/core/audio.c src/core/content.c
  src/core/assets.c src/core/fx.c src/core/world.c src/core/player.c
  src/core/combat.c src/core/drones.c src/core/game.c src/core/ui.c
  src/core/ui_dock.c src/core/save.c src/core/scenario.c
)
CFLAGS=(-std=gnu11 -O2 -ffast-math -fno-strict-aliasing -w -Isrc/core)

mkdir -p build dist

case "${1:-headless}" in
  headless)
    "$ZIG" cc -target x86_64-linux-gnu "${CFLAGS[@]}" \
      "${CORE_SRC[@]}" src/platform/headless/headless_main.c \
      -o build/sm_headless -lm
    echo "built build/sm_headless"
    ;;
  win)
    # asset pack -> object file via .incbin (no objcopy dependency)
    mkdir -p build/gen
    cat > build/gen/assets_blob.s <<'ASM'
    .section .rdata,"dr"
    .globl sm_assets_pak
    .globl sm_assets_pak_len
    .p2align 6
sm_assets_pak:
    .incbin "assets/assets.pak"
sm_assets_pak_end:
    .p2align 3
sm_assets_pak_len:
    .quad sm_assets_pak_end - sm_assets_pak
ASM
    # icon + version info + DPI-aware manifest
    /tmp/venv/bin/python tools/make_icon.py assets/gen/icon_256.png build/gen/spaceminer.ico >/dev/null
    cp build/gen/spaceminer.ico src/platform/win32/spaceminer.ico
    ( cd src/platform/win32 && "$ZIG" rc /fo "$OLDPWD/build/gen/spaceminer.res" spaceminer.rc )
    "$ZIG" cc -target x86_64-windows-gnu -Wl,--subsystem,windows "${CFLAGS[@]}" \
      "${CORE_SRC[@]}" \
      src/platform/win32/win_main.c src/platform/win32/win_audio.c \
      build/gen/assets_blob.s build/gen/spaceminer.res \
      -o dist/SPACE_MINER.exe \
      -lwinmm -lopengl32 -lgdi32 -luser32 -lcomctl32 -lm
    ls -la dist/SPACE_MINER.exe
    ;;
  *)
    echo "usage: $0 [headless|win]" >&2
    exit 1
    ;;
esac
