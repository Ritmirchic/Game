# SPACE MINER — Deep Belt Operations

A hand-built space mining / exploration / combat game for **Windows 10/11 x64**.
Custom C engine (software renderer presented through OpenGL 1.1, WinMM audio),
every pixel, sprite and sound generated procedurally by the Python toolchain in
`tools/` — no engine branding, no imported assets, no placeholder art.

## Play

| Input | Action |
| --- | --- |
| `W` / `S` | main thrust / reverse |
| `A` / `D` | strafe left / right |
| Mouse | aim the ship, the mining beam and the guns |
| `Left mouse` | fire the equipped weapon |
| `Right mouse` | mining laser (drains energy, loads the hold) |
| `Shift` | afterburner |
| `Space` | brake / boost dash |
| `Q` | scanner pulse — paints rich deposits for a few seconds |
| `E` | dock at the refinery (fly within 260 m) |
| `Esc` | pause / leave the station |
| `F1` | performance overlay · `F2`–`F4` settings toggles |

Dock to sell ore, buy upgrades, new hulls, mining drones and sector charters.
Five sectors, sixteen resources, twelve upgrade lines with five ranks each,
three weapons, two drone classes, five enemy types, twelve achievements.

## Build from source

```bash
tools/gen_all.sh                # regenerate art, audio, assets/assets.pak
tools/build.sh headless         # Linux dev harness + automated test suite
./build/sm_headless --test --root assets
tools/build.sh win              # dist/SPACE_MINER.exe (needs the Zig toolchain)
```

`tools/setup_toolchain.sh` fetches Zig; `tools/gen_all.sh` needs `numpy` and
`Pillow`. Everything else is plain C11 with no third-party dependencies.

## Layout

```
src/core/        platform-independent game (gfx, world, combat, ui, save, …)
src/platform/    win32 window/audio/input, headless harness for tests
tools/art/       procedural art: asteroids, ships, enemies, planets, sky, icons
tools/gen_audio.py   SFX + five sector music tracks + menu theme
tools/build_pak.py   packs generated assets into assets/assets.pak
docs/            build notes, test report, known limitations
```

## Save data

`%APPDATA%\SpaceMiner\spaceminer.sav` — versioned, CRC-checked, with a rolling
backup (`spaceminer.bak`). A corrupted file is detected and replaced instead of
crashing the game.
