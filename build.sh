#!/usr/bin/env bash
# FRACTURE build script
# - native: headless tests + software screenshots (Linux, g++)
# - windows: final game exe (zig cross-compile, added in platform phase)
set -e
cd "$(dirname "$0")"
mkdir -p build

CORE="src/ft/voxworld.cpp src/ft/voxmesh.cpp src/ft/support.cpp src/ft/physics.cpp \
      src/ft/textures.cpp src/ft/swbackend.cpp src/ft/font_data.cpp \
      src/ft/particles.cpp src/ft/game.cpp src/ft/settings.cpp"

case "${1:-native}" in
  native)
    echo "[build] native headless"
    g++ -std=c++20 -O2 -g0 -fno-exceptions -Isrc -o build/ft_native src/head_main.cpp $CORE -lpthread
    echo "[build] OK -> build/ft_native"
    ;;
  windows)
    echo "[build] windows x64 (zig)"
    VENV_PY="/home/user/venv/bin/python"
    "$VENV_PY" -c "import ziglang,sys; sys.exit(0)" 2>/dev/null || { echo "ziglang missing"; exit 1; }
    ZIG="$VENV_PY -m ziglang"
    $ZIG c++ -target x86_64-windows-gnu -std=c++20 -O2 -fno-exceptions -Isrc \
      -o build/FRACTURE_TOTAL_DESTRUCTION.exe \
      src/platform/win_main.cpp src/platform/win_gl.cpp src/ft/glbackend.cpp $CORE \
      -lopengl32 -lwinmm -lgdi32 -luser32 -lkernel32 -static -mwindows \
      -Wl,--subsystem,windows
    echo "[build] OK -> build/FRACTURE_TOTAL_DESTRUCTION.exe"
    ;;
  *)
    echo "usage: $0 [native|windows]"
    exit 1
    ;;
esac
