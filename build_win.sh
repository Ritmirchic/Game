#!/bin/sh
# FRACTURE — Windows x64 cross-build (Zig 0.17 toolchain, no Windows SDK required).
# Produces a standalone PE32+ executables in release/ with the game icon and manifest embedded.
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-$HOME/tools/zig}"
if [ ! -x "$ZIG" ]; then
    if command -v zig >/dev/null 2>&1; then ZIG=zig; else
        echo "error: zig not found (set ZIG=/path/to/zig)" >&2; exit 1
    fi
fi
mkdir -p out release

SRC="src/main.cpp \
 src/core/png.cpp src/core/font.cpp src/core/audio.cpp src/core/platform_win32.cpp \
 src/render/render_gl.cpp src/render/render_soft.cpp \
 src/world/voxel.cpp src/world/physics.cpp \
 src/ui/ui.cpp \
 src/game/game.cpp src/game/game_render.cpp src/game/game_menus.cpp \
 src/game/mission.cpp src/game/maps.cpp src/game/tools.cpp src/game/fx.cpp \
 src/game/player.cpp src/game/vehicles.cpp src/game/security.cpp src/game/settings_io.cpp"

CXXFLAGS="-target x86_64-windows-gnu -std=c++17 -O2 -I src -DNDEBUG -Wno-narrowing"
LIBS="-static -lopengl32 -lgdi32 -lwinmm -lshell32 -luser32 -lole32"

echo "[1/3] compiling resources (icon + manifest)"
(cd assets/win && "$ZIG" rc fracture.rc ../../out/fracture.res)

echo "[2/3] compiling and linking $SRC"
# shellcheck disable=SC2086
"$ZIG" c++ $CXXFLAGS $SRC out/fracture.res -o release/FRACTURE_TOTAL_DESTRUCTION.exe \
    $LIBS -Wl,--subsystem,windows -Wl,--gc-sections

echo "[3/3] done"
ls -l release/FRACTURE_TOTAL_DESTRUCTION.exe
