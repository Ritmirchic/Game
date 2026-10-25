# THE LAST NIGHT — Design & Architecture

## Concept
Trapped in an abandoned supermarket during a zombie outbreak, 1–4 survivors
alternate between **day preparation** (looting, shopping, barricading) and
**night defense** against wave-based zombie assaults across **ten nights**,
ending in a two-phase final boss fight.

## Engine
Godot 4.3 (GDScript), GL Compatibility renderer (broad Windows hardware support,
including laptops without Vulkan). The engine binary used for this project was
compiled from source (headless-capable Linux editor for CI + Windows export).

## Project layout
```
data/                JSON design data (weapons, zombies, nights, shop, level layout)
scripts/             All gameplay code
  game_data.gd       JSON registry, cell/grid helpers, loot tables
  asset_lib.gd       Texture/sound/material cache (PBR StandardMaterial3D)
  model_factory.gd   Procedural model factory (articulated zombies, weapons, props)
  grid_nav.gd        AStar3D grid navigation with dynamic door blocking
  level_builder.gd   Builds supermarket + exterior from level_layout.json
  barricade.gd       Build/repair/upgrade defenses + nail traps
  loot.gd            Searchable supply crates
  zombie.gd          5 enemy types + 2 bosses, AI states, boss attack patterns
  player.gd          FPS controller, 7-weapon system, downed/revive co-op state
  bile_projectile.gd Boss ranged attack
  wave_director.gd   Ten-night data-driven spawning + environmental events
  audio_manager.gd   Positional 3D audio, ambient loops, storm scheduling
  save_system.gd     JSON persistence in user://
  session.gd         Orchestrator + ENet replication (authoritative host)
  main.gd            Bootstrap, input map, menu/session lifecycle
  net/game_net.gd    ENet server/client (LAN / direct IP, up to 4 players)
  ui/ui_manager.gd   All screens + HUD + minimap (code-built, custom theme)
tests/               Headless integration harness
tools/               Python asset generators + layout validator
assets/textures      Generated Pillow textures/UI art (original)
assets/sounds        Generated numpy WAV audio (original, no samples)
```

## Level
A 30×20 cell grid (2 m cells). Rooms: checkout lobby, sales floor with five
aisle rows, refrigerated section, maintenance, electrical, loading dock,
storage A/B (with roof access), staff offices, security room, back corridors.
Five exterior attack routes: main entrance, loading dock, west and two east
emergency exits. Exterior adds a fenced parking lot with six abandoned cars,
a delivery truck, curbs and six streetlights. Connectivity is validated both
in Python (`tools/validate_layout.py`, BFS) and in-engine (navigation tests).

## AI
AStar3D over walkable cells; barricades toggle door cells. States: rise →
chase → attack door / attack player → stagger → dead. Includes perception
(LOS raycast so zombies never hit through walls), flocking separation,
stuck detection with nav recovery, barricade destruction, trap triggering.
- **Walker** baseline; **Runner** fast; **Crawler** low profile; **Brute**
  armored with high barricade damage; **Screamer** calls reinforcements.
- **Hulk (Night 5 mini-boss):** charge + bile spit.
- **Overlord (Night 10):** three phases, summons, charge, triple bile, enrage.

## Combat
Two melee + five ranged weapons, each with distinct damage/rpm/magazine/spread/
recoil/reload stats, unique procedural view model, hitscan with spread,
muzzle flash, tracers, sparks and blood effects, aim zoom, recoil, head bob.

## Campaign
Each night defines timed spawn pulses across routes, weather (rain/fog/
thunder), events (power flicker, dense fog), and cash/material rewards.
Difficulty grows through enemy composition, simultaneous routes, max-alive
pressure and special enemies — not raw health multipliers.

## Co-op
ENetMultiplayerPeer, host-authoritative. Zombies, waves, loot, barricades,
resources and boss logic run on the host; clients send intents and receive
reliable spawns, unreliable transform snapshots and resource/world-state
syncs. Downed survivors bleed out for 30 s while teammates revive them.

## Save
Campaign progress (night, team resources, stats) and settings are written to
`user://savegame.json` / `user://settings.json`.
