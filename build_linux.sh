#!/bin/sh
# Linux/CI build: software renderer, used for automated verification and screenshots.
set -e
cd "$(dirname "$0")"
mkdir -p out
g++ -O2 -std=c++17 -I src -o out/fracture_linux \
  src/main.cpp src/core/png.cpp src/core/font.cpp src/core/platform_null.cpp \
  src/render/render_soft.cpp src/world/voxel.cpp src/world/physics.cpp \
  src/ui/ui.cpp src/game/game.cpp src/game/game_render.cpp src/game/game_menus.cpp \
  src/game/mission.cpp src/game/maps.cpp src/game/tools.cpp src/game/fx.cpp \
  src/game/player.cpp src/game/vehicles.cpp src/game/security.cpp src/game/settings_io.cpp -DNDEBUG
echo "built out/fracture_linux"

# headless gameplay verification suite
g++ -O2 -std=c++17 -I src -o out/fracture_tests \
  tools/test_game.cpp src/core/png.cpp src/core/font.cpp src/core/platform_null.cpp \
  src/render/render_soft.cpp src/world/voxel.cpp src/world/physics.cpp \
  src/ui/ui.cpp src/game/game.cpp src/game/game_render.cpp src/game/game_menus.cpp \
  src/game/mission.cpp src/game/maps.cpp src/game/tools.cpp src/game/fx.cpp \
  src/game/player.cpp src/game/vehicles.cpp src/game/security.cpp src/game/settings_io.cpp -DNDEBUG
echo "built out/fracture_tests"
