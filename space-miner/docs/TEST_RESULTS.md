# SPACE MINER — test results

All numbers below come from `node tools/release.js` on the build machine
(Linux x86-64, Node 22, headless Skia canvas, no GPU). Reproduce with one command:

```bash
ZIG=/path/to/zig node tools/release.js
```

| Suite | File | Result | Time |
| --- | --- | --- | --- |
| Gameplay + engine | `tools/test.js` | **112 passed, 0 failed** | ~11 s |
| Shipped single file, mocked DOM | `tools/smoke.js` | **26 passed, 0 failed** | ~6.9 s |
| Launcher file server, real TCP | `tools/testhttp.js` | **45 passed, 0 failed** | ~0.3 s |
| Installer payload round-trip | `tools/testzip.js` | **15 passed, 0 failed** | ~0.3 s |
| **Total** | | **198 assertions** | ~19 s |

The gate is deterministic: the economy run uses a fixed world seed, and the game
test suite was executed three times back to back with identical results.

---

## 1. Gameplay + engine — `tools/test.js` (112)

Loads every source module into a Node VM with a canvas implementation, builds a
real `Game` and drives the public API (`world.update`, `game.update`, UI draw
calls). Groups and what they prove:

| Group | Covered |
| --- | --- |
| `profile` | starting credits, base stats, every one of the 16 upgrades changes a real stat, costs scale, unaffordable purchases are refused, level/XP curve |
| `save system` | versioned save round-trip, unknown fields tolerated, corrupted JSON rejected, truncated/partial data rejected, schema mismatch falls back to a fresh profile, autosave counters |
| `world / asteroids` | sector world sizes, asteroid count per sector, no asteroid overlaps the player spawn or the station, radius/size spread, resource validity per sector, rotation |
| `flight + camera` | thrust accelerates, drag settles, boost increases top speed, ship stays inside world bounds, camera follows with damping and clamps at edges |
| `drones` | drone slots from upgrades, launch/recall, drones travel to rocks, mine, return and deposit, capacity respected, idle behaviour when the hold is full |
| `combat` | projectile spawn, enemy AI states, damage applied to both sides, shields absorb, cooldowns, kill rewards, death and respawn, enemy variants differ |
| `hazards + sectors` | 5 sectors, unlock rules, hazard tick per sector, sector switch rebuilds the world, locked sectors refuse travel |
| `economy balance` | a seeded two-minute mining run produces 1936 CR of ore (threshold 1500), the first upgrade is affordable after one run, more than 8 rocks destroyed, simulation stays fast |
| `ui rendering` | every screen draws without throwing: boot, menu, settings, achievements, controls, flight HUD, cargo, station tabs (trade, upgrades, drones, shipyard, sectors), map, pause, death |
| `stability soak` | 3 simulated minutes of mixed activity with particle/enemy caps enforced, no NaN positions, no unbounded growth |
| `render performance` | 1600×900 frame timing in the headless software rasteriser |

Measured on this machine:

* 3 simulated minutes in ~1.0–1.2 s → **155–175× realtime** for the full simulation.
* Particles peak at ~205–225 with the configured caps; enemies peak at 4 concurrently.
* 1600×900 render: **5.8–6.8 ms/frame** with the CPU-only software canvas
  (this is a worst case — the shipped game runs on GPU-backed browsers).

## 2. Shipped file smoke test — `tools/smoke.js` (26)

This suite tests the *artifact*, not the source: it reads the built `index.html`,
extracts the five inline script blocks, runs them in a mocked DOM with a Skia
canvas, and then plays the game through the real UI entry points.

* five script groups execute and expose the expected globals;
* boot → menu → flight, with the title screen rendering;
* ship moves, camera follows, mining beam damages an asteroid, rock breaks;
* chunks are collected, cargo fills, HUD values change;
* pause, sector map and cargo overlay open and close;
* docking, selling the hold, buying an upgrade, launching a drone;
* save → reload → progress preserved;
* a locked sector refuses travel and shows a warning;
* `draw()` never throws across states.

## 3. Launcher file server — `tools/testhttp.js` (45)

The launcher's serving code (`win/serve.h`, `win/http_util.h`) is compiled into a
POSIX host and driven over **real TCP sockets** on 127.0.0.1:

* `/` and `/index.html` serve the shell with `text/html; charset=utf-8`;
* nested assets get correct MIME types (JS, CSS, ICO, WOFF2, JSON, binary);
* HTML is `no-store`, assets are `max-age=3600`;
* `HEAD` returns headers only while keeping `Content-Length`;
* unknown routes fall back to the single-page shell;
* path traversal is refused in all encodings (`/../`, `/%2e%2e/`, mixed
  separators, `%5c`), NUL bytes and control characters are refused, drive-letter
  paths never leak a file outside the game folder;
* `POST` is refused with 405, malformed requests with 400, oversized paths with
  400 (no truncation);
* `/__ping` answers `pong` with and without a query string;
* 13 `url_to_rel` cases and 8 MIME cases are asserted directly.

## 4. Installer payload — `tools/testzip.js` (15)

The installer's extraction engine (`win/zip.h`) is compiled into a host tool and
run against payloads built by the release script — including the real release
installer:

* the 16-byte footer magic, payload offset and length are inside the file;
* extracting `dist/SPACE_MINER_SETUP.exe` yields byte-identical
  `SPACE_MINER.exe`, `game/index.html` and `game/icon.ico`;
* the extracted launcher is a valid `MZ`/PE image;
* deflate-compressed entries decode correctly (text, binary, empty file);
* a payload corrupted inside the compressed stream is **rejected via CRC32**
  instead of writing broken files.

## 5. Static binary checks

Performed on both shipped executables after every build (scripted, `pefile`):

* PE32+ / `machine 0x8664` (x64);
* `Subsystem = 2` (Windows GUI) — **no console window** opens in the release build;
* sections: `.text .rdata .buildid .data .pdata .tls .reloc .rsrc`, parser reports
  zero warnings;
* resources: `RT_ICON` ×7 (16–256 px), `RT_GROUP_ICON` ×1, `RT_VERSION` ×1;
* version info: SPACE MINER 1.0.0.1, product/description/company strings present;
* the setup payload sits at the declared offset and the footer magic matches.

## 6. Visual review

`node tools/shots.js` renders 19 screens at 1600×900 into `shots/`; each was
inspected and iterated on (palette, asteroid facet shading, planet placement,
HUD warnings, station layouts). Screens include boot, menu, four sectors in
flight, active mining, rock break, combat, all five station tabs, sector map,
settings, achievements, pause and death.

## What is *not* covered here

Runtime execution of the two `.exe` files requires a Windows machine — see
[`KNOWN_LIMITATIONS.md`](KNOWN_LIMITATIONS.md) for the exact split between what is
verified automatically and what is verified by code review plus PE/structural
analysis.
