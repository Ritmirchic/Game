# SPACE MINER — The Helix Expedition

A standalone 2D mining, combat and automation adventure. No account or network connection is required to play the desktop release.

## Start
Windows: extract the portable ZIP, then run `SPACE_MINER.exe`, or run `SPACE_MINER_SETUP.exe` to install for the current user. The installer does not require administrator privileges. Builds are unsigned; Windows may display a reputation warning. Only run downloads from a source you trust.

Choose **Continue Expedition**. Fly right to the nearby asteroids, aim at one, and hold the left mouse button. Fly into the mineral fragments. Return left to the Helix refinery and press E to sell. Install cargo and laser upgrades, build a visible mining drone, and research new sectors through the sector chart.

## Controls
- **WASD**: directional thrust; ship aims at mouse cursor
- **Left mouse**: mine the asteroid under the cursor, within scanner range
- **Right mouse / Space**: shoot
- **E**: dock (within 240 metres of the refinery)
- **I**: cargo manifest
- **U**: upgrade workshop
- **M**: sector map / research / jump
- **O**: drone operations
- **P**: achievement record
- **F5**: save
- **Escape**: pause / close panel

## Progression
Fifteen resources, five sectors, three ship chassis, twelve upgrade branches with five tiers each, six maximum visible mining drones, five fleet-research tiers, and eight persistent achievements. All upgrades affect actual gameplay. Equipment and drones can be purchased only at a refinery. Sector research is sequential; unlocked sectors allow free travel. Every sector contains a refinery.

Mining drains energy. The reactor and shields recharge over time. A drone finds rocks, flies to them, mines, collects the released ore and returns to unload. A full ship hold blocks unloading. The refinery repairs all systems for free and protects you from enemy fire. Red hazard rings warn of periodic discharges in advanced sectors.

Hull failure triggers rescue: you retain equipment and ships but lose 12% of credits and half the cargo. There is no permanent death.

## Saves
The desktop save is `user://space_miner_v1.json`, normally `%APPDATA%\Godot\app_userdata\SPACE MINER | The Helix Expedition\` on Windows (Godot may sanitize the project-name folder). The game autosaves every 20 seconds and after purchases and travel. JSON schema validation and a previous-save backup protect against corruption. Settings and achievements are saved. Browser saves are isolated from desktop saves and depend on the browser's persistent storage.

## Requirements
Windows 10/11 x64, OpenGL 3.3-compatible graphics. The Linux build requires an x86_64 Linux desktop with OpenGL 3.3. Browser builds require WebGL 2 (the supplied Web export is single-threaded). Mouse and keyboard recommended; no touch/controller support is claimed.

See `TEST_RESULTS.md` for performed verification and limitations. Native Windows execution and installer interaction must be verified on a Windows machine; Linux/headless tests do not prove Windows compatibility.
