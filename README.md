# ASHES OF HUMANITY: LAST SIGNAL

An original post-apocalyptic survival-horror game for Roblox, delivered as a
ready-to-open place file: **`ASHES_OF_HUMANITY.rbxl`**.

You wake on a concrete floor in a dying civil-defence shelter. The emergency
lights are failing, the radio is receiving something that should not be able to
transmit, and the city above has been quiet for eleven years. Six connected
districts, one transmitter, and a thing that has learned to use the radio.

Everything in this repository — world geometry, materials, lighting, systems,
UI, story, creatures — is original work created for this project. No third-party
assets, models, stories or audio are included.

---

## 1. Deliverable

| Item | Value |
| --- | --- |
| Place file | `ASHES_OF_HUMANITY.rbxl` (0.55 MB) |
| Instances | 16,268 |
| Parts | 14,481 |
| Scripts | 60 (17 server services, 20 shared modules, 22 client scripts, 1 bootstrap) |
| Source | 60 Luau files, ~13,200 lines |
| World | 6 regions, 236 mutant spawn anchors, road network, 51 burning barrels, 5 neon landmarks |
| Target place settings | Workspace.StreamingEnabled, Future lighting, 24 player cap |

Open it with **Roblox Studio → File → Open from File**, then press **Play**
(F5). The place loads a playable character directly into the shelter.

> Built in a Linux sandbox with a from-scratch `.rbxl` binary writer (validated
> against 57 rbx-dom reference fixtures), so the file has **not** been opened in
> Roblox Studio here. Section 8 lists exactly what is verified and what still
> needs a Studio pass.

---

## 2. What you actually get

### The world — six connected districts

| Region | Character | Threat |
| --- | --- | --- |
| The Dead Suburbs | Burnt houses, fenced yards, a shelled shop, a fuel station, the shelter entrance | Low |
| Blackwood Forest | Dense deadfall, a ranger cabin, a cult camp, tunnel caves | Medium |
| The Industrial Zone | Warehouses, a foundry, gantries, chemical spills | High |
| The Forgotten City | Apartments, hospital, police station, relay tower, streets and wrecks | High |
| Military Exclusion Zone | Checkpoints, barracks, containment block, relay array | Severe |
| The Red Wasteland | Reactor ruin, irradiated flats, glassed craters | Extreme |

The streets are dotted with 51 burning barrels — real `Fire` + `PointLight`
objects that the server also treats as heat sources for temperature — and five
broken neon signs that give each district a readable silhouette at night.

Everything is built from plain `Part`s with Roblox's built-in PBR materials
(Concrete, CorrugatedMetal, WoodPlanks, Slate, Sand, Neon…) — no external
meshes or textures are required for the place to render correctly.

### The starting shelter

The spawn is a walkable civil-defence bunker, not a sealed box. From the sleeping
room (`y = -20`, 44 × 34 studs) a lit corridor leads to a 20-step staircase that
climbs through a concrete shaft and opens onto a guard-walled deck at ground
level. The deck's only doorway faces north, out into the Dead Suburbs.

- Bunker room, corridor and stair shaft are cut from solid ground by a real hole
  in the terrain slabs (`GROUND_HOLES` in `tools/worldgen.py`) — verified clear
  of blockers along the whole route, including head room on every step.
- Furnished: two bunks, table and chair, shelves, crates, a working generator
  with warm light, and the radio that starts the story.
- Anchors placed at build time: `PlayerSpawn`, `Radio`, `Bed`, `Workbench` and
  six `Loot` points, all under `Workspace.World`, so the survival, story, loot
  and crafting services find them without any runtime guessing.

Lighting stack: `Technology = Future`, `Atmosphere`, `Clouds`,
`ColorCorrectionEffect`, `BloomEffect`, `SunRaysEffect`, `DepthOfFieldEffect`,
global shadows, fog driven live by the weather system, and procedural rain /
dust / radiation particles plus lightning on the client.

### Survival — nine interacting stats

Health, hunger, thirst, stamina, body temperature, radiation, infection,
fatigue, bleeding and psychological stress, plus per-limb damage state
(Head / Torso / arms / legs). Derived conditions (fracture, hypothermia,
radiation sickness, infection, panic, exhaustion) change speed, aim, stamina
recovery and screen effects. Cold, wet, night, being alone, nearby mutants and
standing in the open all push temperature, stress and exposure.

### Movement

Sprint with a real stamina budget, crouch (quieter, slower, harder to see),
crawl-speed when injured, vault over waist-high obstacles with a raycast check,
climb vertical surfaces while stamina allows. Injury and exhaustion visibly slow
the character.

### Combat

Eleven weapons across melee, sidearms, shotguns, rifles, a scoped hunting rifle
and a crossbow, each with damage, falloff, spread, recoil, fire rate, magazine,
reserve ammo, durability and noise signature. **The server owns the shot**: it
raycasts from its own origin, re-derives spread, validates the fire rate, spends
ammo from the authoritative profile, wears durability and applies damage.
Headshots, limb targeting and armour coverage all run server-side.

### The ten mutants

| Species | Behaviour |
| --- | --- |
| The Hollow | Blind; hunts purely by noise, screams when it finds you |
| The Crawler | Fast, low, targets legs, hunts in packs |
| The Watcher | Stares from the treeline, builds stress, withdraws when approached |
| The Screamer | Calls the horde from 200+ studs |
| The Brute | Armoured, charges, knocks the player down |
| The Mimic | Copies a human voice to lure you |
| The Burrower | Travels under the ground and erupts at your feet |
| The Stalker | Retreats when wounded, returns |
| The Contaminated | Radiation aura, resilient |
| The Pale One | Nocturnal, fast, hunts alone |

All ten run the same deterministic state machine
(`Idle → Patrol → Investigate → Search → Chase → Attack → Retreat → Return`,
plus `Stalk` and `Burrow`) driven by sight cone, hearing of gunshots and
footsteps, light sources, group size, night and fog. The server executes the
decisions; movement uses `Humanoid:MoveTo`, with a throttled
`PathfindingService` path whenever the direct line is blocked, inside a
per-player population budget (max 42 live creatures, distance-culled, despawned
when far).

### Grid inventory

Slot-based grids (Backpack 6×6, Pockets 3×2, Ammo 4×2) with multi-cell item
footprints, stacking with per-item stack limits, weight capacity, durability
bars, drag-and-drop moves, right-click actions (use / equip / drop), equipment
slots (Primary, Secondary, Melee, Flashlight, Head, Armor, Backpack, Utility),
and server-side re-validation of every move.

### Crafting and base building

24 recipes across hand crafting, campfire, workbench, chem station and forge,
with blueprints that must be found before they can be crafted, station proximity
checks, ingredient validation, weight checks and a timed craft.
14 build pieces (foundations, floors, walls, doors, windows, ramps, barricades,
workbench, bed, campfire, storage, radio relay, watchtower, spikes) snap to a
4-stud grid and 15° rotation, validate ground support and overlap, cost
materials, take damage and persist per player.

### Progression, story, co-op

12 quests (6 main chapters, 6 side contracts) built on a data-driven objective
system (collect / kill / reach / interact / craft / build / survive), with
chapter unlocks, rewards, and a journal. Story is delivered through 7 readable
documents, 5 radio transmissions and 9 environmental phenomena, plus a horror
director that schedules whispers, distant screams, sightings and power
flickers based on time, region, stress and threat.

Parties, shared revives with medkits, friendly-fire arbitration and a
downed-player window are handled server-side.

### Live systems

Day/night cycle (55 real seconds per in-game hour), seven weather states with
smooth transitions, and world events: supply drops with visible flares, mutant
migrations, radiation storms, power flickers, transmissions and night raids.

### Interface

Dark industrial UI built entirely in code: main menu, vitals HUD with compass
and quest tracker, grid inventory, crafting browser, medical screen with a body
map, regional map, journal, building palette with placement ghost, settings
(LOW / MEDIUM / HIGH / ULTRA presets, sensitivity, FOV, head bob, shake, invert
Y) and a death screen with a run summary.

### Security and persistence

Server authority on damage, ammo, loot, crafting, building, quests and
inventory; per-remote token-bucket rate limits; arity/type checks; one owner per
remote; grid validation against duping; profile sanitation on load and save;
DataStore `UpdateAsync` with retries, backoff, session locking, autosave and
`BindToClose`. If DataStore is unavailable the game runs in a graceful
no-persistence mode instead of breaking.

---

## 3. Controls

| Key | Action |
| --- | --- |
| `W A S D` | Move |
| `Left Shift` | Sprint (drains stamina, makes noise) |
| `Left Ctrl` | Crouch |
| `Space` | Jump / vault low obstacles / climb walls |
| `E` | Interact, search, confirm build placement |
| `F` | Flashlight |
| `Tab` | Inventory |
| `C` | Crafting |
| `B` | Building palette |
| `H` | Medical |
| `M` | Map |
| `J` | Journal |
| `V` | First / third person |
| Right mouse | Aim (ADS) |
| `R` | Reload weapon / rotate build piece |
| `1` `2` `3` | Equip Primary / Secondary / Melee |
| `4` | Accept party invite |
| `Escape` | Close panel / settings |

---

## 4. Repository layout

```
ASHES_OF_HUMANITY.rbxl        the playable place file
src/shared/                   20 pure Luau modules (no Roblox services needed)
src/server/                   17 services + Bootstrap.server.lua
src/client/                   21 controllers/UI modules + Main.client.lua
tools/rbx/                    .rbxl binary reader + writer (rbx-dom compatible)
tools/build.py                assembles the place file from src/ + worldgen
tools/worldgen.py             six-region world generator
tools/world_kit.py            geometry / material toolkit
tools/preview.py              offline z-buffer renderer -> docs/preview/*.png
tools/verify_place.py         reads the built .rbxl back and asserts its contents
tools/audit_refs.py           cross-module reference audit
tools/luau_rt/                Luau CLI built from the official 0.743 sources
tests/                        offline test harness, Roblox stub and 11 spec files
docs/preview/                 visual QA renders of every region
```

Service pattern: every server module is a table with `Init(registry)` and
`Start(registry)`; the registry exposes every other service by name, so no
service reaches across the tree directly. Shared logic is written as pure
functions so the same code runs on both sides and can be tested offline.

---

## 5. Rebuilding

```bash
python3 tools/build.py         # writes ASHES_OF_HUMANITY.rbxl
python3 tools/verify_place.py  # re-reads the place and asserts its contents
python3 tools/audit_refs.py    # verifies every cross-module call exists
```

Requires Python 3 with `pillow`, `numpy` and `lz4` (only for previews/reading
compressed chunks). `tools/bin/luau_rt` is a prebuilt Luau CLI used for syntax
checking and the test suite:

```bash
tools/bin/luau_rt --check $(find src -name '*.lua')
tools/bin/luau_rt --run tests/harness.lua .
```

---

## 6. Test results (actual, from this repository)

Last full run in this sandbox:

```
$ tools/bin/luau_rt --check $(find src -name '*.lua')
parsed 60 file(s) OK

$ tools/bin/luau_rt --run tests/harness.lua .
144 assertions passed, 0 failed across 11 spec files (20 modules loaded)
  all green

$ python3 tools/verify_place.py
place        : ASHES_OF_HUMANITY.rbxl
roots        : 14
scripts      : 60
parts        : 14481
checks       : 156
spawn anchors: 236
result       : ALL CHECKS PASSED

$ python3 tools/audit_refs.py
references checked : 249
problems           : 0

$ python3 tools/tests/roundtrip.py
57 files round-tripped, 0 issues
```

What each suite covers:

| Suite | Scope |
| --- | --- |
| `01_shared` | item catalogue, equipment slots, weapon wiring, use effects |
| `02_inventory` | footprint overlap, stacking limits, full-grid rejection, weight, equip/unequip, serialisation |
| `03_survival` | stamina drain/regen, hunger/thirst over a simulated hour, starvation damage, hypothermia, fire warming, radiation gain/decay, limb damage, armour, bleeding, death, consumable effects |
| `04_combat` | falloff, headshots, armour coverage, worn-weapon damage, spread cone, recoil, ammo spend, fire-rate gate, body-part selection, explosion falloff, every weapon definition |
| `05_loot` | all 17 tables reference real items, deterministic seeded rolls, mutant drops, supply drops |
| `06_quests_craft` | objective types, chapter order, reward integrity, quest state machine, completion, save round-trip, recipe validity, ingredients, station gating, blueprint locking, single consumption |
| `07_enemyai` | all ten species' stat blocks, habitat/tier validity, sight chase, melee attack, noise hunting, blind behaviour, patrol/return, nocturnal sleep, Watcher stalking, Brute charge, Screamer call, Stalker retreat, Mimic lure, Burrower eruption, despawn |
| `08_profile` | schema creation, garbage input, unknown fields, stat clamping, dead-save resurrection, deleted items, v1→v3 migration, snapshot privacy and round trip |
| `09_building` | grid snapping, 15° rotation, overlap rejection, floating-wall rejection, affordability, exact cost consumption, salvage refund, health scaling |
| `10_sources` | **no fabricated asset ids anywhere**, audio situation documented, no TODO markers, every shared module returns, every service has Init/Start, bootstrap matches disk, every client remote exists, no remote bound twice, codebase size |
| `11_server_boot` | loads all 17 services against the Roblox stub and calls `Init(registry)` — catches nil services, bad requires, missing Init/Start |

`11_server_boot` deliberately does **not** call `Start()`, because every service
starts real Heartbeat loops that the deterministic stub would never finish.

Visual QA: `docs/preview/*.png` are rendered offline from the built place file
by `tools/preview.py` (software z-buffer, flat shading — it verifies layout and
silhouette, not final lighting). Eleven views cover overview, suburbs, house
interior, forest, industrial, city, city street, military, wasteland, the
shelter with its ceiling and ground slab removed (`shelter_cut.png`) and the
surface hatch (`shelter_hatch.png`).

---

## 7. Content inventory

65 items · 11 weapons · 24 recipes · 14 build pieces · 17 loot tables ·
12 quests · 10 mutant species · 7 documents · 5 radio transmissions ·
9 horror phenomena · 6 weather states · 6 world events · 9 survival stats ·
6 body regions.

---

## 8. Known limitations — read before judging

These are real, current limitations, not disclaimers:

1. **Not opened in Roblox Studio in this environment.** The `.rbxl` writer is
   validated against 57 reference fixtures and the file re-reads cleanly, but a
   Studio open-and-play pass has not been performed here. That is the single
   most important remaining check.
2. **No animation assets.** Custom character animations were specified for the
   design, but shipping them requires published `Animation` assets, which
   cannot be created outside Studio and must not be invented. Roblox's default
   humanoid animation set plays instead. See section 9.
3. **No audio assets.** `AudioController` is complete (volume, 3D falloff,
   pitch, weather mixing) but every sound id is intentionally `""`, so the game
   is silent. No unlicensed audio was embedded.
4. **No PBR texture assets.** The lighting stack and material system are
   configured, but `SurfaceAppearance`/`MaterialVariant` maps need published
   texture ids. The world uses Roblox's built-in PBR-capable materials.
5. **Creature models are procedural block rigs**, not sculpted meshes. They are
   welded, humanoid-driven assemblies with emissive eyes; final art would
   replace them.
6. **Pathfinding is budgeted, not universal.** Creatures steer directly with
   `Humanoid:MoveTo`; only when a chaser's line to its target is blocked does it
   request a `PathfindingService` path (at most once every two seconds per
   creature) and follow the waypoints. This keeps a 40-creature fight cheap but
   means wide detours are less graceful than a full navigation mesh would be.
7. **The offline preview renderer is a stub-quality rasteriser** with flat
   shading; it is a layout check, not a lighting preview.
8. **Balance is untuned against real players.** Values come from the shared
   config tables and are consistent, but no playtest data exists yet.
9. **DataStore is unverified in production.** Session locking, retry and
   `BindToClose` paths are implemented and reviewed, not load-tested.
10. **Multiplayer was validated logically** (server authority, rate limits,
    party code paths) but not with multiple live clients.
11. **Fire is atmosphere, not simulation.** The 51 burning barrels are baked
    geometry with `Fire` and `PointLight` objects; they warm you and light the
    street, but they do not spread, burn out or consume fuel. Campfires you
    place yourself are runtime objects with a 15-minute server-side lifetime.

---

## 9. Publication checklist (exact remaining steps)

To take this from "opens and plays" to "production-ready", in Studio:

1. **Open** `ASHES_OF_HUMANITY.rbxl`, press Play, confirm the client boots
   (expected output: `[Ashes] server ready — 17 services online`, `[Ashes] client ready`).
2. **Animations** — in Studio's Animation Editor, author and publish:
   idle, walk, run, crouch-idle, crouch-walk, jump, vault, climb, melee swing
   (per weapon class), equip, reload, hit reaction, death. Upload each as an
   `Animation` asset, owned by the same account as the place, then reference the
   ids from a single table. Nothing in this repo invents an id.
3. **Audio** — upload original or licensed `.ogg` files (CC0 sources are fine),
   then fill `Audio.Library` in `src/client/AudioController.lua` with the
   published ids. The mixer already handles volume, looping and 3D falloff.
4. **Textures** — for true PBR: publish colour/normal/roughness/metalness maps,
   create `MaterialVariant`s in `MaterialService`, and attach
   `SurfaceAppearance` to hero surfaces (roads, walls, vehicles).
5. **Enable Studio API access** for DataStore testing
   (Game Settings → Security), verify save/load and `BindToClose` on a published
   place.
6. **Streaming** — confirm `StreamingEnabled` behaves with the spawn location
   and the shelter; set persistent models if the spawn streams out.
7. **Publish** the place, then run the six QA passes with real clients:
   static, asset, gameplay, multiplayer, performance, regression.

---

## 10. Design notes

The horror is deliberately quiet. There is no jump-scare sound design, because
there is no sound design yet; instead the game leans on darkness, resource
pressure, weather, and creatures that react to the noises you make. Firing a
rifle is the loudest thing in the world, and the Hollow cannot see you at all —
it only hears you.

The stress stat is the connective tissue: darkness, isolation, being watched,
radiation and near-death all raise it, and high stress blurs the screen, shakes
the camera, drains stamina and triggers phenomena. The story is delivered in
fragments you have to physically find and read.

All writing, systems, geometry and code in this repository were produced for
this project.
