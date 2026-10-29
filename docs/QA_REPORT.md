# FRACTURE: TOTAL DESTRUCTION — QA report

All results below were produced by the automated suite in `tools/test_game.cpp`
(`out/fracture_tests`), running the real game code headless with a 1280×720 software
rasteriser, on Debian 12 x86-64.

**Result: 68 checks passed, 0 failed** (`=== RESULT: 68 passed, 0 failed ===`, exit code 0).

Reproduce:

```sh
./build_linux.sh
FRACTURE_DATA_DIR=/tmp/qa ./out/fracture_tests --out /tmp/qa
```

---

## Coverage

| Block | Checks | What is actually verified |
|---|---|---|
| **A — world generation** | 8 | boot reaches the menu; all 6 maps build in range; destruction targets registered; props and vehicles spawn; meshes emit triangles |
| **B — voxel destruction** | 4 | hammer carve changes geometry (`25711 → 25581` solid voxels over 4 swings), measurable destroyed fraction, debris bodies spawn |
| **C — explosives** | 5 | crater carved (`34929 → 28711` voxels), debris bodies (`12 → 52`), chain reaction reported, smoke/fire particles |
| **D — structural collapse** | 4 | house loses voxels (`1410491 → 1408775`), unsupported mass becomes falling pieces (`bodies 5 → 139`) |
| **E — fire** | 4 | fire ignites on wood, burns the structure down, emits particles, extinguisher suppresses it |
| **F — save / load** | 5 | RLE world save written, geometry restored identical, file much smaller than the world, destruction stats survive |
| **G — campaign** | 8 | 5 chapters, 14 missions, mission starts, objectives complete, completion recorded |
| **H — vehicles** | 5 | map vehicles spawn, manual spawn, enter, drive (`odo 21.5 m`), heavy vehicle damages structures |
| **I — menus / lighting** | 3 | main menu renders, all menu screens render, night scene stays visible (ambient floor) |
| **J — view models** | 1 | all 12 first-person tools render (screenshots `tool_01..12.png`) |
| **K — security** | 9 | cameras/turrets placed, explosions destroy cameras, alarm activates turrets, turrets shoot the player (`5 shots, health 100 → 62`), stealth mission fails on alarm |
| **L — showcase** | — | six map screenshots + explosion showcase |
| **M — all twelve tools** | 1 | every tool produces a real effect (voxel removal, spawned body, cable, fire suppression or airstrike crater) |

Per-tool measurements from the final run (brick test wall, ~2.7 m aim distance):

```
SLEDGEHAMMER         carves brick         voxels 1411247 -> 1411195  bodies  5 -> 16  OK
SHOTGUN              pellet damage        voxels 1411247 -> 1411181  bodies 16 -> 16  OK
EXPLOSIVE CHARGES    charge detonation    voxels 1411209 -> 1410092  bodies 16 -> 56  OK
ROCKET LAUNCHER      rocket impact        voxels 1410837 -> 1408869  bodies 56 -> 96  OK
BLOWTORCH            torch cut            voxels 1409625 -> 1409603  bodies 96 -> 96  OK
PLANK TOOL           plank bridge         rigid plank body spawned                    OK
FIRE EXTINGUISHER    spray                fire suppressed + water particles           OK
GRAPPLING TOOL       grapple pull         hook attaches, player pulled                OK
INDUSTRIAL CUTTER    cutter               voxels 1409666 -> 1409630                   OK
GRAVITY CABLE        gravity cable        cables 0 -> 1                               OK
DEMOLITION DRILL     drill                voxels 1409666 -> 1409630                   OK
AIRSTRIKE DESIGNATOR airstrike             voxels 1409666 -> 1380589  bodies 96 -> 148 OK
```

## Map statistics (real, from the run)

| # | Map | Size (vox) | Solid voxels | Chunks | Targets | Props | Vehicles |
|---|---|---|---|---|---|---|---|
| 0 | Westhaven Suburbs | 320×72×320 | 1,410,491 | 182 | 9 | 5 | 4 |
| 1 | Blackstone Industrial | 336×96×336 | 1,326,902 | 176 | 5 | 12 | 4 |
| 2 | Downtown Collapse | 336×160×336 | 1,625,530 | 241 | 5 | 2 | 3 |
| 3 | Harbor Breakdown | 320×80×320 | 966,784 | 146 | 5 | 6 | 5 |
| 4 | Red Canyon Test Range | 352×112×352 | 4,059,191 | 227 | 7 | 5 | 3 |
| 5 | The Grand Mall | 288×96×288 | 1,187,323 | 166 | 3 | 3 | 3 |

## Performance (measured, software reference renderer, 1280×720, quality HIGH)

| Scene | ms/frame | Notes |
|---|---|---|
| Westhaven, 15:00 | 178.1 | full terrain + props |
| Blackstone, 10:00 | 155.6 | |
| Downtown, 08:00 | 225.0 | tallest skyline |
| Harbor, 13:00 | 175.5 | |
| Red Canyon, 16:00 | 184.4 | 4.06 M solid voxels |
| Grand Mall, 12:00 | 122.8 | |
| After 8 explosions | 485.3 | 322 active debris bodies, 1,903 particles, 29,540 voxels destroyed |

These numbers are the **CPU software rasteriser**, which exists for headless verification and
as a fallback. The shipping renderer is the OpenGL 3.3 path; it was validated by compiling
all 12 shader stages (vertex + fragment for world, sky, particles, boxes, UI, post) with
glslang — **all 12 compile for GL 3.3 core**. Two real shader defects were found and fixed
during this validation (`step(...) ? 1.0 : 0.0` — a float used as a boolean condition, which
would have failed to compile on a real driver).

## What was NOT verified

* The Windows `.exe` was **cross-compiled and structurally verified** (PE32+ x86-64,
  subsystem GUI, 8 sections, `.rsrc` with icon + manifest + VERSIONINFO, imports limited to
  KERNEL32/USER32/GDI32/OPENGL32/WINMM/SHELL32/UCRT) but **could not be executed** — this
  build environment is Linux with no Windows runtime, no GPU and no X server.
* Therefore no frame has been rendered by the OpenGL path on real hardware. Its shaders
  compile, its GL loader has a full function table, and the software renderer exercises the
  same game/UI/mesh code, but GPU output is untested in this environment.
* Audio is synthesised and mixed but was never heard (headless).
* No playtesting by humans; difficulty and pacing are tuned from the automated runs only.

## Fixes made during QA (all verified by re-running the suite)

1. `World::carve` measured distance to voxel **centres**, so a 0.17 m torch radius could miss
   every voxel when aimed at a face. Now measures to the voxel **box** — every tool that aims
   at a surface always removes geometry.
2. `step(a,b) ? 1.0 : 0.0` in the shadow PCF of `FS_WORLD`/`FS_BOX` — invalid GLSL in 8
   places, would not have compiled on a real driver.
3. Plank tool spawned a physics body without checking the spawn succeeded (index −1 → UB).
