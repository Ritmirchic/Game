# SPACE MINER: BUILD & CONQUER

**Version 0.1.0 — early playable development build, not the completed production game.**

Native Godot 4.4.1 3D project for Windows x64. Kept separate from the repository's pre-existing SLICE browser game and older unrelated Space Miner releases.

## Play

Unzip the entire Windows package and run `SPACE_MINER.exe`. Keep `SPACE_MINER.pck` beside it. No editor or browser is needed. Unsigned executable: Windows may display a SmartScreen warning. Save location: `%APPDATA%/Godot/app_userdata/SPACE MINER: BUILD & CONQUER/` (Godot may normalize invalid filename characters).

Start with a cockpit, hull, engine, mining laser, 1,200 credits and 9 crystals. Purchase a construction cell, then install equipment on an empty adjacent grid position. A magnet and cannon are useful initial purchases. Launch from the expedition chart. Mine the nearby rocks, move through ore to collect it (or install a magnet), and return to the cyan dock beacon. Press E within 22m to sell ore and repair.

## Controls

W/S thrust/reverse; A/D rotate; Q inertial brake; Shift afterburner. Aim with cursor; left mouse mining; right mouse/Space weapons. E dock; Tab cargo; Escape pause. Middle-drag camera orbit; Shift+middle-drag camera pan; wheel zoom. Shipyard: left click install, right click remove, R rotate, U upgrade hovered module. F5 save.

## Implemented scope

Connected 11×11 modular construction with capacity purchases, rotation, removal validation, upgrades, unified rigid-body mass and center of mass; eleven shop module categories; third-person belt flight; damageable irregular textured asteroids; physical drifting ore and magnetic force collection; pulse/rocket projectiles; enemy pursuit and attacks; shields and repair; five sector configurations; cargo economy; forty original illustrated crew cards, collection search/sort/filter and equipment; three-card paid packs, animated reveals, duplicate conversion; three to five active upgrade slots; thirty UTC-timed claimable contracts; validated versioned saves with atomic replacement and backup recovery.

All artwork is original deterministic procedural work. It is not AAA hand-painted production art. Read `docs/STATUS.md` for omitted or simplified specification items and the exact validation scope.

## Develop / test

Open this directory in Godot 4.4.1. Python asset generator needs Pillow:

```
python tools/make_assets.py
godot --headless --path . --editor --quit
godot --headless --path . --script res://tests/test_runner.gd
godot --headless --path . -- --smoke
godot --headless --path . --export-release 'Windows x64' dist/SPACE_MINER.exe
```

`tests/test_runner.gd` exercises economy, placement, crew limits, upgrades, card probabilities, timers, save validation and recovery. `--smoke` exercises actual 3D scene construction, menus, mining destruction, enemy destruction, cargo sales, saves, and card reveal rendering nodes. The GitHub workflow also runs these against the exported Windows game and tests a silent NSIS install.
