# SPACE MINER — test results

## Release gate

| Suite | Coverage | Result |
| --- | --- | --- |
| Godot source runtime | `tests/test_runner.gd` | **144 passed / 0 failed** |
| Rendered visual route | `tests/capture.gd` | **18 screens captured, no runtime errors** |
| Workflow static analysis | `actionlint` | **0 findings** |
| Windows executable and installer | `.github/workflows/godot-windows-release.yml` | See [`BUILD_REPORT.md`](BUILD_REPORT.md) for the native verification record |

The source suite is run against the real `scenes/main.tscn`, real physics entities, real UI screens and an isolated versioned save directory. It does not replace the game with a mock simulation.

## Runtime assertions

### Data, progression and economy

- Fifteen unique resource IDs and original icon files.
- Five unique sector backgrounds, four ships, five enemy classes and achievement definitions.
- New-profile starting conditions.
- Successful purchase and **a measured live-stat increase for every one of the sixteen upgrades**.
- Unaffordable purchase refusal, strict cargo capacity, rejection of overflow ore, correct sale value and empty manifest after sale.
- Sector clearances, all-sector achievement, ship purchase/selection, drone slot and drone purchase.
- XP-driven pilot level increases.

### Save system

- Atomic save write and credits, upgrades, resources and discovery round-trips.
- Exact-payload checksum validation after JSON parsing.
- Corrupted primary save detection, valid schema recovery and backup/quarantine behavior.

### Actual game world

- Main scene boots to the menu and launches into a populated physical world.
- Player is a `CharacterBody2D`; asteroids are `RigidBody2D` bodies.
- Procedural spawn clearance around player and station.
- Thrust moves the real body and the active camera follows it.
- Mining damages the scanned target, consumes energy and displays the beam.
- Depletion removes the asteroid and generates collectible resources.
- `Area2D` pickup collection updates cargo.
- Purchased drones spawn visible entities, autonomously mine a target and record real cargo delivery to the ship.
- Enemy AI acquires the player and fires real projectiles; friendly projectiles apply damage; kills grant credits and progression.
- All five sectors generate their configured fields; Void Frontier creates its gravity hazards.

### UI and stability

- Pause, settings, achievements, manual, sector map and all six station tabs render in the running engine.
- Every button on every tested screen has a connected action.
- Asteroid and enemy populations remain capped; positions and velocities stay finite.
- Pickup, spawn and hazard updates are measured against the **16.6 ms / 60 Hz** budget.

Local measurement:

```text
PERFORMANCE  simulation update average: 0.014 ms
TEST SUMMARY: 144 passed / 0 failed
```

The timing is specifically for the measured simulation subsystem, not a claim about GPU frame rate on every machine.

## Rendered visual review

A virtual-display run executes the actual game at **1600×900** and writes:

- main menu;
- Starter Belt flight;
- active mining laser;
- destruction effects;
- combat;
- trade, upgrades, drones, shipyard, sector and research station tabs;
- sector chart;
- settings;
- achievements;
- Crystal Nebula, Iron Graveyard, Plasma Storm and Void Frontier flight.

The frames are committed under `shots/`. Visual review led to corrected wrapped-label sizing, constrained background thumbnails, upright drone labels and a deterministic mining capture. The final CI route reported no script or runtime errors.

## Reproduce

```bash
# Isolated Linux source test
XDG_DATA_HOME="$PWD/.test-data" godot --headless --path . --script res://tests/test_runner.gd

# Rendered route (requires a display; CI uses Xvfb)
godot --path . --resolution 1600x900 --script res://tests/capture.gd
```

For the native Windows test, the release workflow invokes the **exported** `SPACE_MINER.exe` with the same `res://tests/test_runner.gd`, then builds the NSIS installer, performs a silent installation and runs that installed executable against the suite again.
