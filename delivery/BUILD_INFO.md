# SPACE MINER — Release delivery

Built 2026-10-25 using **Godot 4.7.1 stable**. Unreal Engine was not installed, so the requested Godot fallback was used.

## Downloads

| Package | Contents |
|---|---|
| [SPACE_MINER_WINDOWS.zip](SPACE_MINER_WINDOWS.zip) | Standalone **SPACE_MINER.exe**, player guide and runtime/font notices |
| [SPACE_MINER_SETUP.exe](SPACE_MINER_SETUP.exe) | NSIS per-user Windows installer; Start-menu entry and uninstaller |
| [SPACE_MINER_SOURCE.zip](SPACE_MINER_SOURCE.zip) | Complete Godot project, original assets, generation tools, integration suite and release workflow |
| [SPACE_MINER_WEB.zip](SPACE_MINER_WEB.zip) | Playable single-threaded Godot Web build |
| [SPACE_MINER_LINUX.zip](SPACE_MINER_LINUX.zip) | Standalone Linux x86_64 executable |
| [SHA256SUMS.txt](SHA256SUMS.txt) | SHA-256 hashes of the downloadable packages |

The Windows executable embeds its game data; no separate PCK file or engine installation is required. Extract the portable archive before launching it. The installer does not need administrator privileges. Binaries are unsigned.

## Verified release build

- Gameplay source revision: `4e5e920263bcc1f2ea5fb3e73765d4a68b6828b6`.
- Final Windows CI run: [40201387463](https://github.com/Ritmirchic/Game/actions/runs/40201387463), **success**.
- `windows-release`: **success**, including official engine/template installation, 44 integration checks, Windows export and installer generation.
- `windows-smoke`: **success**, including native Windows portable startup, silent installation, installed startup, silent uninstall and removal check.
- Local Linux packaged release startup: **passed**.
- Actual browser mining, movement, collection and UI inspection: **passed**, zero console/page errors in the final interaction test.

## Scope

Playable original 2D mining/exploration/combat/automation game: five sectors, fifteen minerals, twelve five-tier system branches, three chassis, visible mining drones, five hostile classes, trading, research, saves/settings and eight achievements.

This is a compact indie implementation, not a premium-budget Unreal 3D production. Native Windows execution was tested **headlessly** and installation **silently**; no manual Windows GPU playthrough or interactive installer-wizard test is claimed. No multiplayer, narrative campaign, gamepad/touch support or localization. See `space_miner/TEST_RESULTS.md` inside the source archive for detailed limitations and raw test reproduction.
