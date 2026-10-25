SPACE MINER — Deep Belt Operations. Release build for **Windows 10/11 x64**.

## Files

| File | Size | What it is |
| --- | --- | --- |
| **SPACE_MINER_SETUP.exe** | 1.13 MB | Per-user installer: Start Menu / desktop shortcut, entry in *Settings → Apps*, no administrator rights. Silent install: `/S [/D=<path>]` |
| **SPACE_MINER_PORTABLE.zip** | 314 KB | Unzip anywhere and run `SPACE_MINER.exe` (whole folder needed) |
| **SPACE_MINER.exe** | 317 KB | Launcher only — expects a `game\index.html` folder beside it |
| **SPACE_MINER_SOURCE.zip** | 260 KB | Full sources: game JS, C launcher/installer, tests, build scripts |

## Controls

`W A S D` thrust · mouse aims the ship and the mining beam · left mouse mines (drains energy) · `Space` / right mouse fires pulse cannons · `Shift` afterburner · `E` dock · `M` sector chart · `Tab` cargo · `Esc` pause.

## What is inside

Five sectors (Starter Belt, Crystal Nebula, Iron Graveyard, Plasma Storm, Void Frontier), four ships, fifteen resources with individual icons and rarities, sixteen upgrades that change real gameplay numbers, autonomous mining drones, five enemy types with their own AI, hazards, station economy, eighteen achievements, and a versioned save that recovers from corrupted data.

Everything is generated at runtime — no imported art, no placeholder assets, no engine branding. The launcher hosts the game on a loopback server and opens it in a chromeless Edge/Chrome app window; nothing leaves the machine.

## Verification

* 112 gameplay/engine assertions — flight, mining, destruction, collection, cargo, selling, upgrades, drone AI, enemy AI, combat, sector unlocks, saving, UI, performance soak
* 26 assertions against the **shipped** `index.html` running in a mocked DOM (boot → flight → dock → sell → upgrade → save/load)
* 45 assertions on the launcher's file server over real TCP, including path-traversal defences
* 15 assertions on the installer payload — byte-identical extraction of this very setup executable, CRC rejection of corrupted archives
* PE checks on both binaries: PE32+ x64, GUI subsystem, 7-size icon resource, version info

## Notes

* Unsigned build: SmartScreen will ask for confirmation on first run — **More info → Run anyway**.
* Windows 10/11 x64 only (UCRT API sets).
* The executables were cross-compiled and structurally verified; see `docs/KNOWN_LIMITATIONS.md` in the source archive for the exact split between what is tested automatically and what is verified by review.
