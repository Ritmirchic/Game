#!/usr/bin/env bash
# ============================================================================
#  SPACE MINER — regenerate every generated asset and the packed .pak
#  Requires: python3 with numpy + Pillow  (pip install numpy pillow)
#  Usage:  tools/gen_all.sh [path/to/python]
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."

PY="${1:-python3}"
"$PY" -c "import numpy, PIL" || {
  echo "numpy and Pillow are required (pip install numpy pillow)" >&2
  exit 1
}

mkdir -p assets/gen assets/gen/audio

echo "[1/5] asteroids"
( cd tools && PYTHONPATH=. "$PY" art/asteroids.py ../assets/gen )
echo "[2/5] ships, enemies, drones"
( cd tools && PYTHONPATH=. "$PY" art/ships.py ../assets/gen )
echo "[3/5] nebulae, sky, station, effects"
( cd tools && PYTHONPATH=. "$PY" art/world_art.py ../assets/gen )
( cd tools && PYTHONPATH=. "$PY" art/sky.py ../assets/gen )
( cd tools && PYTHONPATH=. "$PY" art/planets.py ../assets/gen )
echo "[4/5] icons, logo, font atlases"
( cd tools && PYTHONPATH=. "$PY" art/icons.py ../assets/gen )
( cd tools && PYTHONPATH=. "$PY" art/ui_art.py ../assets/gen )
echo "[5/5] audio + pack"
( cd tools && "$PY" gen_audio.py ../assets/gen/audio )
"$PY" tools/build_pak.py assets/gen assets/assets.pak

echo
echo "assets/assets.pak regenerated."
echo "Next:  tools/build.sh headless   (dev harness + tests)"
echo "       tools/build.sh win        (dist/SPACE_MINER.exe)"
