# SPACE MINER — Deep Belt Operations

A complete, self-contained space mining adventure for **Windows 10/11, 64-bit**.
Mine asteroids, sell ore, upgrade the ship, deploy autonomous drones, fight five
kinds of hostiles and push deeper into five hand-built sectors. Every frame of
art, every effect and every sound is generated inside the build — the game ships
as one 358 KB HTML file plus a native Windows launcher.

**Download the release:** <https://github.com/Ritmirchic/Game/releases/tag/space-miner-v1.0.0>

| Asset | Direct link |
| --- | --- |
| `SPACE_MINER_SETUP.exe` | <https://github.com/Ritmirchic/Game/releases/download/space-miner-v1.0.0/SPACE_MINER_SETUP.exe> |
| `SPACE_MINER_PORTABLE.zip` | <https://github.com/Ritmirchic/Game/releases/download/space-miner-v1.0.0/SPACE_MINER_PORTABLE.zip> |
| `SPACE_MINER.exe` | <https://github.com/Ritmirchic/Game/releases/download/space-miner-v1.0.0/SPACE_MINER.exe> |
| `SPACE_MINER_SOURCE.zip` | <https://github.com/Ritmirchic/Game/releases/download/space-miner-v1.0.0/SPACE_MINER_SOURCE.zip> |

The same files live in this repository under `dist/` (see `node tools/release.js`).

---

## 1. Quick start

**Installer (recommended)**

1. Double-click `SPACE_MINER_SETUP.exe`.
2. Keep the default location (`%LOCALAPPDATA%\Programs\SpaceMiner`) or pick your own.
3. Press **INSTALL**. A Start Menu shortcut and (optionally) a desktop shortcut are
   created, plus an entry in *Settings → Apps* for a clean uninstall.
4. The game launches automatically when the installer finishes.

**Portable**

1. Unzip `SPACE_MINER_PORTABLE.zip` anywhere (USB stick included).
2. Run `SPACE_MINER.exe` from inside the `SPACE_MINER` folder.

The launcher starts a private loopback web server on `127.0.0.1`, then opens the
game in a chromeless Microsoft Edge / Google Chrome app window (own profile, no
tabs, no browser UI). Nothing leaves the machine and no internet connection is
used. If neither browser is present the game opens in the system default browser.

---

## 2. Controls

| Input | Action |
| --- | --- |
| `W A S D` / arrow keys | Thrust and strafe |
| Mouse | Aim the ship and the mining beam |
| Left mouse (hold) | Fire the mining laser at the asteroid under the cursor (drains energy) |
| `Space` / right mouse | Pulse cannons |
| `Shift` | Afterburner — extra speed at the cost of reactor energy |
| `E` | Dock at the station when in range |
| `M` | Sector chart |
| `Tab` | Expand the cargo readout |
| `Esc` | Pause / close the current panel |
| Drone panel (HUD, bottom right) | Drone autonomy and bay management |

Everything is playable with mouse alone (UI is click-driven) — the keyboard is
only required for flying.

---

## 3. What is in the game

**Five sectors** — Starter Belt, Crystal Nebula, Iron Graveyard, Plasma Storm,
Void Frontier. Each has its own palette, nebula/starfield backdrop, asteroid
design language, resource table, hazard (electrical storms, plasma surges,
gravity shears, void radiation), enemy mix, ambient particle layer and station
economy. Sectors unlock through progression and ship class.

**Fifteen resources** — Silicate, Ferrite, Nickel, Cuprite, Titanium, Silver,
Cobalt, Gold, Lumen Crystal, Iridium, Platinum, Uranium, Plasma Ore, Void
Crystal, Neutronium. Each has a procedurally drawn icon, rarity tier, value,
mining hardness and a distinct deposit look on the rock face.

**Four ships** — Prospector Mk II (starter), Harvester-class Bulk, Lancet-class
Interceptor, Leviathan-class Forge. Different hull silhouettes, engine blocks,
mining rigs, thruster positions, paint schemes, hardpoints and stats.

**Sixteen upgrades in a real dependency tree** — mining laser power, beam focus,
cargo bay, ore compactor, ion thrusters, inertial dampers, hull plating, shield
capacitor, fusion reactor, beam economiser, deep scanner, tractor field, pulse
cannons, drone bay, drone overclock, nano repair bay. Every level changes real
numbers used by the simulation (damage per tick, tick interval, capacity, thrust,
mass, regeneration, range, drone count and speed).

**Mining** — asteroids carry health, hardness, a resource mix and a damage state.
The beam must be aimed and held; it consumes energy, chips the rock through
fracture stages, throws sparks and fragments, then breaks into collectable chunks
that fly to the ship (or are pulled in by the tractor field). Asteroids spawn
only after a collision check against the ship and the station.

**Drones** — visible autonomous units that undock, scan for ore, fly out, mine,
carry the ore back and repeat. Their count, speed, capacity and mining rate are
upgrade-driven and shown in the drone panel.

**Combat** — five hostiles (Survey Drone, Raider Skiff, Heavy Breaker, Plasma
Hunter, Void Sentinel) with distinct art, AI (orbit, ram, strafe, snipe, siege),
attack patterns, shields, rewards and death effects. Projectiles, hit sparks,
shield impacts, screen shake and target reticles are all in.

**Progression** — credits, XP and levels, sector and ship unlocks, 18
achievements, rare mineral finds, research/automation upgrades, and a versioned,
validated save that survives corrupt or partial data (it falls back to a clean
profile instead of breaking the game).

**UI** — title screen, settings, achievements, flight manual, HUD (hull, shield,
energy, cargo, credits, scanner, radar, warnings, toasts), pause, death screen,
station (trade, upgrades, drones, shipyard, sectors), sector map, cargo overlay.
All of it is drawn in-engine with a consistent industrial-teal identity and
animated transitions.

---

## 4. Technical overview

| Layer | Implementation |
| --- | --- |
| Game | Vanilla ES5-compatible JavaScript, one canvas (2D), no libraries, no network |
| Rendering | Layered procedural art: background nebula/starfield, parallax dust, asteroid fields, sprite ships, particles, bloom-style glows, custom UI |
| Audio | WebAudio synthesis (reactor hum, laser, impacts, explosions, UI) — no audio files |
| Save | Versioned JSON in `localStorage` with schema validation and corruption recovery |
| Packaging | `tools/build.js` inlines 5 script groups into one HTML file (358 KB) |
| Launcher | C (Win32) compiled to a 317 KB GUI executable — loopback HTTP file server, splash window, browser app-window launch, `/uninstall` |
| Installer | C (Win32) self-contained setup with an embedded payload (stored ZIP + 16-byte footer), progress UI, shortcuts, per-user uninstall registration |
| Icons | Procedurally drawn multi-size `.ico` (16–256 px), injected as real PE resources (`RT_ICON`, `RT_GROUP_ICON`, `RT_VERSION`) |

No Electron, no WebView wrapper hacks, no branding, no debug console windows:
both executables are `subsystem: windows` GUI binaries with no console output.

---

## 5. Building from source

Requirements: Node.js 18+ (for the build/test tooling) and a Zig toolchain for the
native executables (`zig cc` cross-compiles to `x86_64-windows-gnu`; no MSVC or
MinGW install needed). `skia-canvas` is used only by the screenshot/icon tooling.

```bash
cd space-miner
node tools/build.js                 # → index.html (single file)
node tools/test.js                  # gameplay + engine suite
node tools/smoke.js                 # boots the shipped file in a mocked DOM
node tools/testhttp.js              # launcher server, over real TCP
ZIG=/path/to/zig node tools/win.js  # → dist/*.exe, portable + source archives
node tools/testzip.js               # installer payload round-trip
node tools/release.js               # everything above, one verdict
```

`node tools/release.js` is the release gate; it exits non-zero if any suite fails.

Screenshots can be regenerated with `node tools/shots.js` (writes `shots/*.png`).

---

## 6. Verification

Latest release gate on the build machine:

| Suite | Result |
| --- | --- |
| Gameplay + engine (`tools/test.js`) | **112 passed, 0 failed** |
| Shipped single file in a mocked DOM (`tools/smoke.js`) | **26 passed, 0 failed** |
| Launcher file server over TCP (`tools/testhttp.js`) | **45 passed, 0 failed** |
| Installer payload round-trip (`tools/testzip.js`) | **15 passed, 0 failed** |

Details, methodology and the measured performance numbers are in
[`docs/TEST_RESULTS.md`](docs/TEST_RESULTS.md). Known limitations — including what
could not be exercised without a Windows machine — are listed in
[`docs/KNOWN_LIMITATIONS.md`](docs/KNOWN_LIMITATIONS.md).

---

## 7. Project layout

```
space-miner/
  index.html               built game (open this in any browser)
  src/
    core/                  math, noise, utilities
    data/                  resources, sectors, upgrades, ships, enemies, achievements
    art/                   theme, backgrounds, asteroid generator, ship sprites, icons
    game/                  profile/save, entities, particles, world, render, audio, input, game loop
    ui/                    widgets, HUD, station screens, menus, dispatcher
    index.template.html    build template
  tools/                   build, headless harness, tests, screenshots, icon + PE tooling
  win/                     launcher, installer, portable headers (serve/zip/http_util)
  dist/                    release artifacts
  docs/                    test results, deployment, limitations
  shots/                   rendered screens
```

---

## 8. Uninstall

*Installed build:* **Settings → Apps → SPACE MINER → Uninstall**, or run
`SPACE_MINER.exe /uninstall`. Shortcuts, game files, the browser profile holding
the save and the registry entries are removed.

*Portable build:* delete the folder. The browser profile in
`%LOCALAPPDATA%\SpaceMiner` can be removed by hand.

---

© 2026 Arena.ai. Built as a single reproducible pipeline: art is generated, not
imported; the release is produced by one command and gated by four test suites.
