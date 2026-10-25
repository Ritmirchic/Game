# Test results

Harness: `tools/tests.cpp`, built headless and run as `./build/smtests --selftest`.
Last run: build `3.0`, g++ 12, `-O2`, x86_64 Linux container (no GPU, software rasteriser only).

```
 RESULT: 89 passed, 0 failed
 asset + world init: ~4.0 s
 measured 18.44-18.80 ms/frame (update + full software render, 241 entities)
```

The raw log is in `docs/selftest_output.txt`.

## Coverage

| Area | Checks | What is verified |
|------|-------:|------------------|
| Movement | 4 | Thrust accelerates along the facing vector, speed is clamped to the hull stat, the ship coasts to a stop, the camera tracks the ship |
| Mining | 9 | Beam damages and destroys rock, target locking, energy drain, loot spawning, tractor pickup into the hold, stat recording, economy sanity |
| Cargo | 3 | Hold cap enforced, overflow rejected, the player is warned |
| Economy | 6 | Docking, selling earns the correct credits, cargo is cleared, price modifiers apply |
| Combat | 7 | Cannon fire, projectile lifetime, enemy damage/hit flashes, enemy destruction rewards, player damage, shield absorption, death transition |
| Drones | 4 | Purchase, autonomous flight to a rock, mining, delivery of ore to the ship |
| Progression rules | 12 | Achievement unlocks, trading/upgrades/ships/drones refused while undocked, drone bay cap (4 + 2/level, hard cap 8), XP → level curve, level → stat growth |
| Sectors | 6 | Unlock costs gate the chart, jumping changes the sector, per-sector spawn tables, hazard behaviour, victory transition |
| Save/load | 11 | File written and present, all fields restored, corrupted file recovered from backup, out-of-range values clamped (credits, level, ship index) |
| UI | 8 | Menu, settings, all five station tabs (cargo, upgrades, shipyard, drones, achievements) render; every screen renders without crashing |
| Performance | 1 | Full update + render of a 241-entity scene within the 33 ms budget |
| **Total** | **89** | |

Plus `--shots`, which renders every screen through the real renderer and writes 18 PNGs and the
asset sheet used for visual review.

## Performance

Measured as *update + complete software render* of a live scene (241 entities: 62 asteroids with
crystals, 4 drones, hostiles, ~300 particles, loot, HUD), 1280×720 virtual canvas:

| Scene | Frame time |
|-------|-----------:|
| Sector 1, calm | 9.8 ms |
| Sector 3, combat + drones | 13–15 ms |
| Stress scene (241 entities, 300 particles) | 18.4 ms |
| Worst measured case during development | 20.4 ms |

Budget at 30 fps is 33 ms and at 60 fps 16.6 ms, so the game holds 55–100 fps on this
single-threaded reference machine. Section profiling (`-DSM_PROFILE`) shows the split:

| Section | ms |
|---------|----|
| Background / nebula / planet | 2.6 |
| Asteroid layer | 2.1 |
| Station | 0.3 |
| Loot | 0.4 |
| Drones | 0.9 |
| Enemies | 1.3 (was 33.9 before the rotated-blit rewrite) |
| Player + beams | 1.2 |
| Projectiles | 0.5 |
| Particles | 1.1 |
| HUD / UI | 2.3 |

The single biggest win was rewriting `Canvas::blitRotatedAdd` / `blitRotated` to compute a
row-span bounding box with correct inverse rotation and clipping: the enemy hull layers dropped
from 33.9 ms to 1.3 ms.

## Visual checks

`--shots` produced `shots/` with: five sector shots, menu, settings, sector chart, five station
tabs, achievements, death, victory, an explosion VFX sheet, and `art_sheet.png` (nebulae, five
planets, four hulls, five hostiles, station, drones, debris, a 5×6 crystal grid, fifteen ore
icons with codes, twelve upgrade icons, VFX sprites and twenty badges).

Visual review found and fixed:

* rotated sprite blits sampling with the wrong inverse rotation (sprites mirrored/black),
* planets picking up a starfield painted on top of their own texture (white specks on every world),
* a lens flare drawn over planets instead of behind them,
* ring systems clipped by the planet canvas (disc radius reduced relative to the canvas),
* an atan2 longitude seam that pinched every planet into a starburst at the centre of the disc,
* scanner rings drawn as full circles around off-screen rocks (now four dashed arcs, on-screen only),
* minimap footer labels colliding with the panel border.

## Sanitizers

`-fsanitize=address,undefined` build (`-O1 -g`): the full suite runs clean —
`RESULT: 89 passed, 0 failed`, no ASan report, no LeakSanitizer report, no UBSan diagnostic
(it needs `ASAN_OPTIONS=verify_asan_link_order=0`).

The sanitizer run originally surfaced two real findings, both fixed:

* signed integer overflow in the value-noise / Worley hash mix (`core.cpp`) — every hash
  multiplication now happens in `uint32_t`;
* the frame-budget check failed simply because ASan builds are ~3× slower, so the harness now
  relaxes its own budget to 200 ms when a sanitizer is active (89/89 in both configurations).
