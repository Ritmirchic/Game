# Gameplay Reference

## Controls

| Input | Action |
|-------|--------|
| `W A S D` / arrow keys | Thrust (vector-style; the ship drifts and coasts) |
| Mouse | Aim the turret / point the ship |
| Left mouse, or `SPACE` | Mining beam (hold) |
| Right mouse, or `F` | Pulse cannons |
| `SHIFT` | Afterburner (extra thrust, more energy draw) |
| `E` | Dock / undock at Helios Station |
| `TAB` | Cargo manifest & station interface (5 tabs) |
| `M` | Sector chart / jump drive |
| `ESC` | Settings & pause menu |
| `H` | Frame counter |
| `F11` | Borderless fullscreen toggle |

The interface is fully mouse-driven; every button has a hover state and an audible click.

## The loop

1. **Mine.** Hold the beam near a rock. Rock hardness (0.75–2.75) and your mining-power stat
   decide how fast it breaks. While the beam cuts, ore chips fly off, and loot is tractored into
   the hold once you are close enough.
2. **Haul.** Fly back to **Helios Station** (the ring in the middle of the sector) and press `E`.
   Selling, upgrades, ships and drones are only available **while docked** — flying far away and
   opening `TAB` gives you a read-only "remote link" view.
3. **Upgrade.** Twelve independent branches, ten levels each (see the table below), plus four
   hulls and three drone tiers.
4. **Expand.** Unlock the next sector from the sector chart; each one has new ore, tougher
   hostiles and a darker mood.
5. **Automate.** Drones fly out on their own, cut rock and bring ore back, so income continues
   while you hunt hostiles or explore.

## Sectors

| # | Sector | Unlock | Ore focus | Hostiles | Notes |
|---|--------|--------|-----------|----------|-------|
| 1 | STARTER BELT | free | Iron, copper, nickel, silver | Scout drones | Tutorial, gentle |
| 2 | CRYSTAL NEBULA | 2 500 CR | Crystal, cobalt, silver, nickel | Scouts + raiders | Ion storm ambience |
| 3 | IRON GRAVEYARD | 12 000 CR | Titanium, gold, nickel, platinum | Raiders + heavies | Best early combat income |
| 4 | PLASMA STORM | 45 000 CR | Plasma ore, uranium, iridium | Raiders + plasma hunters | Storm hazard, reduced visibility |
| 5 | VOID FRONTIER | 150 000 CR | Void crystal, dark matter, quantum core | Sentinels | Endgame; victory on first jump home |

## Resources

Values are base sale prices in credits; the refinery upgrade multiplies them.

| Resource | Code | Base value | Rarity | Hardness |
|----------|------|-----------:|-------:|---------:|
| Iron | Fe | 8 | 0 | 0.75 |
| Copper | Cu | 16 | 0 | 0.85 |
| Nickel | Ni | 26 | 1 | 0.95 |
| Cobalt | Co | 46 | 1 | 1.10 |
| Titanium | Ti | 74 | 2 | 1.55 |
| Silver | Ag | 108 | 2 | 1.20 |
| Gold | Au | 168 | 3 | 1.35 |
| Platinum | Pt | 245 | 3 | 1.70 |
| Uranium | Ur | 380 | 3 | 1.85 |
| Crystal | Cr | 520 | 4 | 1.45 |
| Plasma ore | Pm | 690 | 4 | 2.05 |
| Iridium | Ir | 940 | 4 | 1.95 |
| Dark matter | Dm | 1 560 | 5 | 2.40 |
| Void crystal | Vc | 2 250 | 5 | 2.20 |
| Quantum core | Qc | 4 200 | 5 | 2.75 |

## Upgrades

| Branch | Effect | Levels | First cost |
|--------|--------|-------:|-----------:|
| Mining laser | Beam power | 10 | 900 |
| Mining speed | Emitter cycle rate | 10 | 750 |
| Cargo hold | +40 units per level | 12 | 650 |
| Engine tuning | Speed & turn authority | 8 | 1 100 |
| Hull plating | Maximum hull | 8 | 1 400 |
| Shield array | Shield capacity | 8 | 1 600 |
| Energy core | Reactor throughput | 8 | 1 300 |
| Resource scan | Scanner range | 6 | 1 000 |
| Drone bays | +2 drone slots per level | 4 | 5 000 |
| Weapon systems | Cannon damage | 10 | 1 800 |
| Tractor coil | Loot pickup range | 6 | 1 200 |
| Refinery | Sale price bonus | 8 | 2 200 |

Costs grow geometrically (×1.45–2.1 per level), so a branch is roughly a 20–60× investment from
first to last level.

## Ships

| Ship | Role | Price | Hull | Shield | Speed | Hold |
|------|------|------:|-----:|-------:|------:|-----:|
| Prospector | Starter cutter | free | 100 | 50 | 235 | 130 |
| Harvester | Industrial rig | 6 500 | 175 | 80 | 205 | 340 |
| Lancer | Assault corvette | 24 000 | 150 | 140 | 285 | 210 |
| Voidcraft | Endgame ark | 95 000 | 240 | 220 | 300 | 430 |

## Drones

| Tier | Name | Price | Mining power | Range |
|------|------|------:|-------------:|------:|
| 1 | Scrapper | 1 000 | 0.55× | 190 m |
| 2 | Prospector | 4 500 | 0.85× | 260 m |
| 3 | Harvester | 18 000 | 1.25× | 340 m |

The drone bay count starts at 4 and grows by 2 per *Drone bays* upgrade (hard cap 8).

## Achievements (20)

First contact · Rockbreaker (100 rocks) · First blood · Hunter (25 kills) · Charted ·
To the frontier · Deep pockets (100 000 CR) · Tinkerer (10 upgrades) · Full power
(max a branch) · Swarm lord (4 drones) · Crystal fever · Plasma burn · Void touched ·
New hull · The ark · Long haul (30 min) · Close call · Quantum · Sentinel falls ·
Master refiner.

Achievements unlock with a popup, are listed on the achievements screen and inside the station,
and persist in the save file.

## Saving

* Location: `%APPDATA%\SpaceMiner\space_miner_save.dat` (+ `.bak` backup).
* Format: binary with a header, a version, all state, and a 32-bit checksum.
* Autosave: on docking and every 90 seconds of flight; also on "Save now" and on quit.
* Recovery: a corrupt file is detected by checksum and replaced from the backup; individual
  values are additionally clamped to sane ranges, so a hand-edited file cannot crash the game.
