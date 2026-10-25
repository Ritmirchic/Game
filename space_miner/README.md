# SPACE MINER · The Helix Expedition

An original, playable Godot 4 space-mining adventure for Windows x64, with additional Linux and Web exports.

## Play
See [PLAYER_GUIDE.md](PLAYER_GUIDE.md). The Windows portable package contains `SPACE_MINER.exe`; the installer is `SPACE_MINER_SETUP.exe`.

The game implements the complete core loop: steer a detailed mining ship, mine textured procedural rocks, collect physical ore drops, trade at a refinery, buy real system upgrades and new ship chassis, research five sectors, fight hostile craft and deploy visible automated drones. Progress, settings and eight milestones are stored in validated, versioned JSON with backup recovery.

## Source layout
- `project.godot`, `main.tscn`: open this project in Godot 4.7.1.
- `scripts/game.gd`: deterministic field generation, simulation, combat, drone state machine, rendering, custom UI and synthesized audio.
- `scripts/pilot.gd`: inertial CharacterBody2D movement and cursor aiming.
- `scripts/profile.gd`: schema validation, atomic save replacement and backup recovery.
- `scripts/catalog.gd`: fifteen minerals, twelve upgrade branches, five sectors, three chassis and achievement definitions.
- `assets/`: original generated nebula backgrounds, layered ships, textured asteroid silhouettes, refinery, mineral icons and bundled fonts.
- `tools/generate_assets.py`: deterministic original artwork generator (Python + Pillow).
- `tests/suite.gd`: 44 automated integration checks against the real game code.
- `tools/installer.nsi`: per-user NSIS installer and uninstaller.
- `../.github/workflows/space-miner-build.yml`: official-engine export, automated tests, native Windows smoke and installation checks.

The project is self-contained; asset generation is not required to open or export it. Python/Pillow are only needed to regenerate artwork. There are no external asset downloads at game runtime.

## Build locally
Install Godot 4.7.1 and the matching official export templates. From the repository root:

```sh
godot --headless --path space_miner --editor --import
godot --headless --path space_miner --script res://tests/suite.gd
mkdir -p releases releases/web releases/linux
godot --headless --path space_miner --export-release Windows
godot --headless --path space_miner --export-release Linux
godot --headless --path space_miner --export-release Web
makensis space_miner/tools/installer.nsi
```

The test suite writes test saves through `user://`; isolate it with `XDG_DATA_HOME` on Linux to avoid replacing a real save. The GitHub workflow does so automatically.

Run `python3 space_miner/tools/serve.py` after a Web export and open port 8000. The server also provides cross-origin isolation headers, binds to 0.0.0.0 and uses no fixed browser-facing localhost API URLs. The supplied single-threaded build does not require SharedArrayBuffer and can run in an embedded preview.

## Scope and limitations
This is a compact, complete **2D systems-focused indie game**, not an Unreal-quality 3D production. There is no story campaign, multiplayer, controller support or localization. Each sector is a bounded, regenerated field rather than an infinite universe. Hazard presentation is sector-specific but uses a shared periodic-pulse system. Enemy classes have distinct visuals, speed, health, weapons and lead targeting; their steering shares an orbit-and-pursue base. Effects use bounded custom-rendered particles rather than Niagara or GPU particle scenes. There are no planetary surfaces.

Autosaves preserve the requested progression, not transient asteroid positions, individual drones' in-flight holds, bullets or enemy positions. Desktop settings include volume, particles and fullscreen. Windows binaries are unsigned. See [TEST_RESULTS.md](TEST_RESULTS.md) for what has and has not been tested.

## Licenses
Original generated gameplay artwork is included with its generation source. Bundled DejaVu fonts and Godot runtime third-party notices are in `licenses/`. No paid or externally licensed artwork is required. Existing unrelated game files elsewhere in this repository are preserved.
