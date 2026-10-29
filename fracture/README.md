# FRACTURE: Total Destruction — prototype

This is a separate native Godot 4.4.1 project, not the ANVILIGHT browser game in the parent directory. It is an **early prototype**, not the requested finished release.

## Running and building

Open `project.godot` with Godot 4.4.1, import the project and run the main scene. For a Windows export, install the matching Godot export templates, then use the **Windows Desktop** export preset, or run:

```
godot --headless --path fracture --editor --quit
godot --headless --path fracture --script res://tests/test_voxels.gd
godot --headless --path fracture --export-release "Windows Desktop" FRACTURE_TOTAL_DESTRUCTION.exe
```

The GitHub Actions workflow `.github/workflows/fracture-windows.yml` is intended to run the voxel test and produce a Windows prototype artifact on changes to this branch. An artifact is not a release and is not a substitute for testing the actual executable interactively.

## Controls

WASD move, mouse look, Shift sprint, Ctrl lower camera, Space jump, 1–3 or wheel select tool, left click use, right click detonate remote charges, F flashlight, Esc menu. Select **Enter Sandbox** after the site finishes preparing. Save/load/reset are in the menu. Save files go to Godot's per-user `user://` directory.

## Prototype scope and known limitations

- One small generated site with two simple enterable buildings, a canopy, road and terrain. Every site cell uses editable voxel geometry; geometry past the terrain edge is empty.
- Half-metre cells in 16-cell chunks, exposed-face meshing, material-dependent blast resistance, dirty chunk mesh and triangle collision regeneration. Chunk work is synchronous, one dirty chunk per frame, so collision changes can lag behind impacts. No greedy meshing or asynchronous jobs.
- Sledgehammer, pellet shotgun and remotely detonated charges. Debris is a small capped set of temporary rigid bodies, not a representation of removed geometry. Impact audio is synthesized. The tools and save/load have not yet been manually verified.
- No structural connectivity or collapse: disconnected voxels can float. No fire, vehicles, campaign, missions, construction, time/weather controls, graphics quality settings, object carrying, or large maps. Crouch only lowers the camera and does not shrink collision. No ammo, reloads, or sophisticated animations. Saved state records removed voxels and player transform, not charges or debris.
- Minimal placeholder-grade art and audio; no professional visual inspection or performance measurement. Not suitable for a v1.0 release. No Windows installer or published release is included.
