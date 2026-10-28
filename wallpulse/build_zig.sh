#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/../release}"
mkdir -p "$OUT_DIR"

ZIG_CMD="${ZIG_CMD:-python3 -m ziglang}"
if [ -x /tmp/zig-venv/bin/python ]; then
  ZIG_CMD="/tmp/zig-venv/bin/python -m ziglang"
fi

TMP_RES="$(mktemp --suffix=.res)"
trap 'rm -f "$TMP_RES"' EXIT

$ZIG_CMD rc /fo "$TMP_RES" "$SCRIPT_DIR/resources/wallpulse.rc"

$ZIG_CMD c++ -target x86_64-windows-gnu -std=c++17 -O2 -s \
  -Wall -Wextra -Wno-unused-parameter -municode -Wl,--subsystem,windows \
  -I"$SCRIPT_DIR/src" \
  "$SCRIPT_DIR/src/common.cpp" \
  "$SCRIPT_DIR/src/config.cpp" \
  "$SCRIPT_DIR/src/desktop_host.cpp" \
  "$SCRIPT_DIR/src/smart_pause.cpp" \
  "$SCRIPT_DIR/src/media_tools.cpp" \
  "$SCRIPT_DIR/src/video_wallpaper.cpp" \
  "$SCRIPT_DIR/src/studio_ui.cpp" \
  "$SCRIPT_DIR/src/main.cpp" \
  "$TMP_RES" \
  -ld3d11 -ldxgi -ld3dcompiler_47 -ld2d1 -ldwrite \
  -lmfplat -lmf -lmfreadwrite -lmfuuid -lpropsys \
  -ldwmapi -lole32 -loleaut32 -lshell32 -luuid -lcomdlg32 -lshlwapi -ladvapi32 -lgdi32 -luser32 \
  -o "$OUT_DIR/WallPulse.exe"

echo "Built $OUT_DIR/WallPulse.exe ($(stat -c%s "$OUT_DIR/WallPulse.exe") bytes)"
