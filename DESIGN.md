# PRISMFORGE: Aether Bastion — Market Research & Game Design Document

## 1. Autonomous Market Research Summary

Research conducted across Steam, itch.io, Roblox, and web game portals identified five mechanical pillars driving high player retention and replayability in modern 2D indie hits:

1. **Transparent Multiplicative Engine-Building (*Balatro*, *CloverPit*, *Ballionaire*, *Luck Be a Landlord*)**
   - *Balatro* (LocalThunk / Playstack, 2024; 97–98% positive across 105k+ English Steam reviews) demonstrated that exposing clear base-value × multiplier chains and allowing passive rule-bending Relics/Jokers to stack multiplicatively produces extraordinary excitement.
   - *Ballionaire* (newobject / Raw Fury, Dec 2024) and *Peglin* showed the immediate kinetic joy of "autobonker" / trigger-board routing where launched orbs cascade across placed widgets.
   - *CloverPit* (Panik Arcade, Sep 2025; 90% positive across 12k+ Steam reviews) reinforced how round-by-round escalating threats paired with charm drafting create intense "one more wave" pacing.

2. **Compact Spatial Adjacency Grids (*Backpack Battles*, *Backpack Hero*, *9 Kings*)**
   - *Backpack Battles* (PlayWithFurcifer, 2025; 92% positive on Steam) and *9 Kings* (Sad Socket / Hooded Horse, May 2025; 92% positive across 10k+ Steam reviews) proved that small, readable spatial grids (such as 9–20 tiles) where placement and orientation directly determine synergies give players deep tactical mastery with minimal UI friction.

3. **Short Action Waves + High-Agency Shop Phases (*Brotato*, *Vampire Survivors*, *Bloons TD 6*)**
   - *Brotato* refined wave survival into crisp 25–45 second combat rounds separated by a deterministic-feeling shop with rerolls, item locking, tier merging (combining matching items into Tier II / Tier III), and strongly differentiated playable characters.

4. **Dynamic Environmental Catalysts (*Grow a Garden*, *Loop Hero*)**
   - *Grow a Garden* (Roblox, 2025; surpassed 21M peak concurrent users during Summer 2025 events) highlighted how periodic weather/celestial anomalies (Solar Flares, Storms, Blood Eclipses) dynamically mutate placed systems and create exciting run-to-run variance.

5. **Tactile Web Meta-Progression (*Nodebuster*, *Outhold*, *Retromine* on Steam & itch.io)**
   - Browser-friendly roguelites thrive when every 10–15 minute run awards persistent currency spent in a multi-branch visual skill tree alongside unlockable classes, codex discovery, and achievements.

---

## 2. Ten Generated Original Hybrid Game Concepts

| # | Concept Title | Hybrid Genre Combination | Core Hook |
|---|---|---|---|
| 1 | **PRISMFORGE: Aether Bastion** | Spatial Pulse-Circuit Routing + Tower Defense + Multiplicative Relics | Route kinetic Aether Pulses through rotatable splitters, mirrors, and elemental lenses on a 5×4 sanctum grid to trigger and multiply live turret volleys against 3-lane Void sieges. |
| 2 | **Chrono-Caravan** | Path-Placement (*Loop Hero*) + Squad Auto-Battler + Oasis Trading | Lay biome tiles around a looping desert rail track while an armored locomotive battles raiders and trades mutated cargo. |
| 3 | **Sporechef Alchemist** | Weather-Mutation Farming + Card-Stack Crafting + Tribute Roguelite | Grow bioluminescent fungi mutated by greenhouse storms, stack reagent cards into elixirs, and meet escalating guild quotas. |
| 4 | **Orbweaver Sanctum** | Kinetic Pachinko Physics + Turn-Based Boss Deckbuilder | Drop elemental spheres down a customizable pegboard where bumper combos cast spells in 1v1 monster duels. |
| 5 | **Ironhive Automata** | Micro-Conveyor Factory + Arena Wave Survival | Pilot an extractor mech in a crater while building short ammo-forging belts that feed auto-firing weapon mounts. |
| 6 | **Shardvault Satchel** | Spatial Grid Inventory + Real-Time Dungeon Crawler | Arrange rotating gems and blades inside an expandable satchel; item adjacencies auto-trigger during dungeon encounters. |
| 7 | **Tidecaller's Reef** | Tension-Reel Fishing + 5-Slot Creature Auto-Battler | Catch abyssal leviathans, graft pearl mutations, and pit pelagic squads against deep-sea bosses. |
| 8 | **Crown of Nine Realms** | 3×3 Micro-Kingdom Decree Builder + Siege Defense | Play royal decree cards onto a 9-tile citadel, chaining adjacency buffs before nightly monster sieges. |
| 9 | **Starbreaker Protocol** | Kinetic Breaker + Sprawling Incremental Node-Tree | Command gravitic paddles and autonomous drones in 60-second data-core breaches to fund a 50-node cybernetic skill tree. |
| 10 | **Verdant Bastion** | 5-Lane Botanical Defense + Mana Deckbuilder + Storm Evolutions | Play botanical cards to graft symbiote plants across lanes while weather events trigger cross-lane evolutions. |

---

## 3. Ten-Criteria Concept Evaluation Matrix

Each concept was scored from 1–10 across all 10 required criteria (Max = 100):

| Concept | Orig. | Replay | Fun | Visuals | Prog. | Scale | Feasib. | Perf. | Scope Fit | Testability | **Total** |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| **1. PRISMFORGE: Aether Bastion** | **9** | **10** | **10** | **10** | **9** | **9** | **9** | **10** | **9** | **9** | **94** |
| 4. Orbweaver Sanctum | 8 | 9 | 9 | 9 | 8 | 8 | 8 | 8 | 8 | 7 | 82 |
| 8. Crown of Nine Realms | 8 | 9 | 8 | 8 | 8 | 8 | 9 | 9 | 9 | 9 | 85 |
| 6. Shardvault Satchel | 7 | 9 | 8 | 8 | 9 | 8 | 8 | 9 | 8 | 8 | 82 |
| 9. Starbreaker Protocol | 7 | 8 | 9 | 8 | 9 | 7 | 9 | 10 | 9 | 8 | 84 |
| 10. Verdant Bastion | 7 | 8 | 8 | 9 | 8 | 8 | 8 | 9 | 8 | 8 | 81 |
| 3. Sporechef Alchemist | 9 | 8 | 8 | 8 | 8 | 8 | 8 | 9 | 8 | 8 | 82 |
| 2. Chrono-Caravan | 8 | 8 | 7 | 8 | 8 | 7 | 7 | 8 | 7 | 8 | 76 |
| 5. Ironhive Automata | 8 | 8 | 8 | 8 | 8 | 7 | 7 | 7 | 6 | 7 | 74 |
| 7. Tidecaller's Reef | 8 | 8 | 8 | 8 | 8 | 7 | 7 | 9 | 7 | 7 | 77 |

### Selection Decision: **PRISMFORGE: Aether Bastion** (`94/100`)

**Why PRISMFORGE wins:**
- **Immediate 15-second hook**: Watching a luminous Aether Pulse leave the Solar Emitter, hit a Splitter Prism, branch North and South through Pyro and Volt lenses, and trigger simultaneous fireball and chain-lightning volleys onto 3 causeways of marching Voidkin requires zero tutorial text to understand and feels immediately exhilarating.
- **Multiplicative build-craft**: Combines spatial circuit routing, 3-tier drag-and-drop merging, 18 rule-warping Astral Relics, 5 Celestial Weather mutations, 4 distinct Wardens, 3 Game Modes, and a 16-node permanent Starforge Skill Tree.
- **Clean architecture & high testability**: Deterministic TypeScript circuit/combat/progression core paired with hardware-accelerated Phaser 3 rendering, object-pooled VFX, and synthesized WebAudio.
