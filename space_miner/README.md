# SPACE MINER — Deep Belt Operations (v3.0)

A complete 2D space mining game for 64-bit Windows, written from scratch in C++17 with **no
external libraries, no engine, no asset files and no runtime dependencies**. Every sprite,
nebula, planet, sound and music pad is generated procedurally at runtime by the game itself.

```
release/SPACE_MINER.exe        1.2 MB   the game (single file, self-contained)
release/SPACE_MINER_SETUP.exe  2.1 MB   optional installer (copies the game + shortcuts)
release/README.txt                      player-facing readme (also embedded in the installer)
release/version.txt
docs/                                   engineering documentation (this folder)
shots/                                  screenshots rendered by the test harness
```

## Documentation index

| File | Contents |
|------|----------|
| `docs/GAMEPLAY.md` | Controls, the game loop, sectors, economy and progression tables |
| `docs/ARCHITECTURE.md` | Engine design: rasteriser, asset synthesiser, game loop, data layout |
| `docs/BUILD.md` | How to build for Windows (cross-compile) and Linux (headless), regenerate assets |
| `docs/TEST_RESULTS.md` | Automated test suite results and performance measurements |
| `docs/KNOWN_LIMITATIONS.md` | Honest list of what is not finished or not ideal |
| `docs/selftest_output.txt` | Raw output of the last `--selftest` run |

## Quick start (players)

Double-click `release/SPACE_MINER.exe`. Fly with **WASD**, aim with the mouse, hold
**LMB** to mine, **E** to dock at the station, **TAB** for the station interface,
**M** for the sector chart.

## Quick start (developers)

```bash
# Linux / macOS: build the headless test harness and run it
g++ -O2 -std=c++17 -I src -o build/smtests tools/tests.cpp src/{core,gfx,texture,art,content,game_core,game_update,game_render,ui,audio,platform_headless}.cpp
./build/smtests --selftest          # 89 checks
./build/smtests --shots shots/      # re-render every screenshot + the asset sheet

# Windows: cross-compile with zig (no MSVC or MinGW install needed)
zig c++ -target x86_64-windows-gnu -std=c++17 -I src -I build -O2 -static \
        -Wl,--subsystem,windows -o release/SPACE_MINER.exe \
        src/main.cpp src/{core,gfx,texture,art,content,game_core,game_update,game_render,ui,audio,platform_win32}.cpp \
        build/rc_resources.o -lgdi32 -luser32 -lwinmm -lshell32 -lole32
```

See `docs/BUILD.md` for the full pipeline including icon generation and the installer.

## Provenance

All art, audio, code and documentation in this repository are original work produced for this
project. There are no third-party libraries, fonts, textures, sound files or trademarks.
© 2026.
