#!/usr/bin/env bash
set -euo pipefail

ART_DIR="public/assets/art"

if [ -f "$ART_DIR/menu_hero_bg.png" ]; then
  convert "$ART_DIR/menu_hero_bg.png" -resize 1280x720^ -gravity center -extent 1280x720 -strip -quality 88 "$ART_DIR/menu_hero_bg.jpg"
  rm -f "$ART_DIR/menu_hero_bg.png"
fi

if [ -f "$ART_DIR/battle_sanctum_bg.png" ]; then
  convert "$ART_DIR/battle_sanctum_bg.png" -resize 1280x720^ -gravity center -extent 1280x720 -strip -quality 90 "$ART_DIR/battle_sanctum_bg.jpg"
  rm -f "$ART_DIR/battle_sanctum_bg.png"
fi

for w in lyra vex kael nyx; do
  if [ -f "$ART_DIR/warden_${w}.png" ]; then
    convert "$ART_DIR/warden_${w}.png" -resize 320x320^ -gravity center -extent 320x320 -strip -quality 90 "$ART_DIR/warden_${w}.jpg"
    rm -f "$ART_DIR/warden_${w}.png"
  fi
done

echo "Art optimization complete:"
ls -lh "$ART_DIR"
