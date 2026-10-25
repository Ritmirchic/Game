# Architecture

SPACE MINER is a single-binary Windows application written in C++17 with the standard library
only. Nothing is linked beyond the OS (`kernel32`, `user32`, `gdi32`, `winmm`, `shell32`,
`ole32`). There is no engine, no SDL, no OpenGL/Direct3D, and no asset pipeline at runtime: the
game paints its own textures and synthesises its own audio itself.

```
src/            7 690 lines of game + engine code
  core.h/.cpp         maths (Vec2, Rng, colour), string/format helpers, PNG writer
  gfx.h/.cpp          Canvas: the software rasteriser + the 3×5 pixel font
  texture.h/.cpp      procedural texture generators (rock, hull, nebula, planet, star, VFX)
  art.h/.cpp          AssetBank: builds every sprite set once at startup
  content.h/.cpp      static game data (resources, sectors, upgrades, ships, achievements)
  game.h              the Game class: state, save data, render/update declarations
  game_core.cpp       lifecycle, economy rules, docking, saving/loading
  game_update.cpp     simulation: flight, mining, drones, enemies, particles, camera
  game_render.cpp     world rendering: background, entities, HUD, screen-space effects
  ui.cpp              menus, station tabs, sector chart, settings, loading screen
  audio.cpp           additive synthesiser (voices, ambient bed, music pad)
  platform.h          host abstraction (window, input, audio pull, save path)
  platform_win32.cpp  Win32 + GDI host: window, input, waveOut audio thread
  platform_headless.cpp  host used by tests/screenshot tooling
  main.cpp            entry point, staged boot with a live loading screen
tools/
  tests.cpp           89-check harness, screenshot pipeline, asset sheet
  gen_assets.cpp      generates the .ico, .rc, icon preview and the installer payload
  installer_win32.cpp Win32 installer (self-contained, embedded payload)
  build_windows.sh    end-to-end Windows build recipe
```

## Rendering

Everything is drawn into a `Canvas`: a flat ARGB `uint32_t` buffer with clipping, alpha
blending, gradients, glows, rings, polygons and a 3×5 bitmap font.

Key routines:

* `blitScaled` — bilinear, alpha-aware up/downscale; used for the nebula, planets and glow maps.
* `blitRotated` / `blitRotatedAdd` — inverse-mapped rotation with a row-span bounding box, clipping
  and nearest-neighbour sampling; additive variant is used for engine glows, shields and lasers.
  Both use an inverse rotation (`u = c·dx − s·dy`, `v = s·dx + c·dy`) so textures rotate correctly.
* `radialGlow`, `glowLine`, `vignetteFast`, `scanlines` — cheap screen-space effects.
* The virtual canvas is a fixed **1280×720**; the window scales it with `StretchDIBits` and
  letterboxes. All game coordinates are independent of the real window size.

The background is a 2048×1280 "arena" canvas per sector (sky gradient, two nebula layers, a
starfield painted directly at arena resolution, a planet with ring system, dust lanes), sampled
with wrapped parallax. A camera `zoom` of 0.86 keeps a little more of the field visible.

## Asset synthesis

`AssetBank::init(stage)` builds every sprite group procedurally: 5 nebula pairs, 5 rock variants
with 6 crystals each, 15 ore icons, 12 upgrade icons, 20 achievement badges, 4 hulls (+ turrets,
glow maps, engine hardpoints), 5 hostile types, 3 drone tiers, 5 debris chunks, station ring,
particle sprites and the cursor. Generation is split into 5 stages driven by `bootStage()` so the
loading screen animates instead of freezing:

| Stage | What is built | Loading text |
|-------|---------------|--------------|
| 0 | glows, particles, ship hulls | HULL PLATING |
| 1 | station, docking ring, drones | STATION TELEMETRY |
| 2 | rock variants + crystals | ASTEROID SURVEY |
| 3 | nebulae, starfields, planets | NEBULA MAPPING |
| 4 | icons, badges, cursor | INSTRUMENT CALIBRATION |

Full asset generation takes ~4 s on the reference machine; the loading bar advances after each
stage, and the window is already showing content before the first stage.

## Simulation

Fixed-shape entity vectors (`asteroids`, `enemies`, `drones`, `loot`, `shots`, `particles`) are
updated per frame by `Game::update(dt, in)` with `dt` clamped to 0.1 s. The game is frame-rate
independent but expects ~60 Hz. Collisions are brute-force circle tests; with ≤ 260 entities that
costs well under a millisecond.

Randomness uses a small xorshift `Rng` seeded per run, so the game is deterministic given a seed
(tests rely on this).

## Audio

`audio.cpp` is an additive synthesiser with 24 voices, a filtered-noise ambient bed and a slow
pad. Sounds are waveforms described by an enum (laser, explosion, coin cascade, alarm, warp, …)
that are rendered on demand into the platform's output buffer. The Win32 host pulls 4 × 1024
frames (23 ms) through `waveOutWrite` callbacks; a spin lock in `audio.cpp` serialises the mixer
thread against the game thread so sounds can be triggered at any time.

The synthesiser renders faster than real time by a wide margin (well under 1 ms per 23 ms
buffer), so it costs no measurable frame time.

## Saving

`SaveData` is a plain struct plus a small header, written with a 32-bit FNV-style checksum. On
load the checksum is verified, the backup is tried if the primary fails, and every field is
clamped to its valid range afterwards. Saves live in `%APPDATA%\SpaceMiner\`.

## Why a software rasteriser

The whole game is 2D with mild rotation and glow effects, at 1280×720. A hand-written rasteriser
with careful clipping is fast enough (frame budget: ~18 ms worst case, 55–85 fps typical, see
`TEST_RESULTS.md`) and removes every dependency: no GPU capability query, no shader compiler, no
driver-specific bugs, and the same code path can render into a buffer on a headless machine — which
is exactly what the automated screenshot pipeline does.
