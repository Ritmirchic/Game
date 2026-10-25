# ALCHEMY CHAOS — Grand Sanctum Edition (v1.0.0)

**ALCHEMY CHAOS** is a standalone 64-bit Windows alchemy sandbox, crafting, discovery, and puzzle game featuring an illustrated multi-layered alchemical laboratory, **50 original illustrated substances**, **120 data-driven transmutation formulas**, **10 shader- and particle-driven physical reaction choreographies**, **7 multi-tier laboratory apparatus upgrades**, **16 commissions & timed trials**, **20 achievements**, and a versioned SHA-256 checksummed save ledger.

---

## Primary Deliverables

| Artifact | Description |
| :--- | :--- |
| **`ALCHEMY_CHAOS.exe`** | Standalone 64-bit Windows executable (`PE32+ x86_64`, `WINDOWS_GUI` subsystem with zero debug console, embedded multi-resolution icon `16x16`–`256x256`, Win32 `VS_VERSIONINFO`, custom loading screen, custom main menu, self-contained embedded `GDPC` resource pack). |
| **`ALCHEMY_CHAOS_SETUP.exe`** | Native Windows Unicode installer built with NSIS 3.10 (`MUI2`, solid LZMA compression, custom sidebar & header bitmaps, Start Menu & optional Desktop shortcuts, Windows *Apps & Features* uninstaller registration). |
| **`ALCHEMY_CHAOS_SOURCE.zip`** | Complete Godot 4.5 project source (`alchemy_chaos/`), procedural asset & DSP audio generators (`tools/`), NSIS installer script (`installer/`), and automated verification suite (`alchemy_chaos/tests/`). |
| **`TEST_RESULTS.md`** | Full automated verification report (`1,502 / 1,502` assertions passed, `0` failed), PE64 & installer inspection, reachability/economy proofs, and known limitations. |

---

## Quick Start (Windows 10 / 11 64-bit)

1. **Portable Launch**: Double-click **`ALCHEMY_CHAOS.exe`** directly — no external runtime, DLLs, or installation required.
2. **Full Installer**: Run **`ALCHEMY_CHAOS_SETUP.exe`** to install into `%LOCALAPPDATA%\Programs\Alchemy Chaos` (or any custom directory) with Start Menu / Desktop shortcuts and a clean `Uninstall.exe`.

### Controls & Shortcuts
- **Left-Click or Drag-and-Drop** an ingredient card from the left **Reagent Cabinet** onto a brass **Pedestal** (or directly into the **Crucible** mouth).
- **Click a Slotted Pedestal** to remove its reagent.
- **Right-Click** an ingredient card in the Reagent Cabinet to **Pin / Unpin** it at the top of your stock.
- **`Space`** — **Transmute** slotted reagents in the Crucible.
- **`C`** — Clear all pedestals.
- **`Feed Result`** — Load the most recently synthesized substance directly onto Pedestal 1 for multi-stage reaction chains.
- **`J`** — Open the **Grand Alchemical Encyclopedia & Recipe Compendium** (50 substances, 120 formulas, 8 rank milestones, search, category/type filters, sorting, and one-click *Load into Crucible*).
- **`U`** — Open **Sanctum Apparatus & Laboratory Upgrades** (7 upgrades × 3 tiers).
- **`K`** — Open **Royal Commissions, Riddles & Timed Trials** (16 challenges).
- **`A`** — Open **Hall of Hermetic Honors & Achievements** (20 achievements).
- **`Esc`** — Close active modal or open **Sanctum Settings & Archival Ledger**.

---

## Core Systems & Content Architecture

### 1. Multi-Layered Interactive Laboratory (`LaboratoryStage`)
- Custom `1920x1080` layered environment with vaulted stone masonry, gothic arched moonlight window with live volumetric `ambient_light_rays.gdshader`, apothecary shelves with bobbing reagent flasks, walnut storage cabinets, copper distillation pipes & spiral condenser emitting live `GPUParticles2D` vapor, animated `AnimationPlayer` pressure manometer needle, arcane wall sconces (`arcane_flame.gdshader`), pinned research board, grimoire bookcase, foreground oak workbench with inlaid transmutation runes, central cutaway cauldron with Swirling `cauldron_liquid.gdshader`, gyroscopic stabilizer rings, clockwork automaton arm, and resonance detector lantern.
- Clicking the **Research Board**, **Bookcase**, or **Resonance Detector Lantern** directly opens Commissions, the Encyclopedia, or triggers an Aetheric Resonance Scan.

### 2. 50 Original Illustrated Substances (6 Categories)
- **Elements (8)**: Fire, Water, Earth, Air *(4 Starters)*, Steam, Intense Heat, Glacial Ice, Electricity.
- **Minerals (9)**: Mud, Stone, Molten Lava, Silica Sand, Crystalline Glass, Clay, Halite Salt, Brimstone Sulfur, Obsidian Glass.
- **Plants (8)**: Verdant Moss, Arcane Herb, Sunblossom, Nightshade, Glowshroom, Mandrake Root, Frostbloom, Amber Resin.
- **Metals (9)**: Native Metal, Forged Iron, Molten Metal, Copper, Silver, Solar Gold, Quicksilver, Tempered Steel, Bronze.
- **Synthetic Compounds (9)**: Thermal Shock, Charged Water, Ceramic Porcelain, Wood Ash,Quartz Prism, Spirit of Nitre, Vitriolic Acid, Black Powder, Mercuric Amalgam, Aqua Regia *(plus Quartz in Minerals)*.
- **Rare Substances (7)**: Luminous Aether, Contained Plasma, Absolute Zero Shard, Phoenix Cinder, Void Essence, Elixir Vitae, Philosopher's Stone.
- Every icon (`128x128` PNG, supersampled from `256x256`) is procedurally painted via `tools/gen_ingredients.py` with bespoke silhouettes, specular glass/metal highlights, and zero emojis or Unicode placeholders.

### 3. 120 Working Data-Driven Recipes & 10 Animated Reactions
- **104** two-reagent formulas + **16** three-reagent Tri-Crucible multi-stage syntheses, including **24 Hidden Mysteries** and **Rare Transmutations**.
- Every combination is order-independent (`A + B == B + A` and all 6 permutations of `A + B + C`).
- **10 Distinct Animated Reaction Choreographies (`ReactionDirector`)**: `BUBBLING`, `SMOKE`, `SPARKS`, `EXPLOSION`, `CRYSTALLIZATION`, `FREEZING`, `MELTING`, `ELECTRICAL`, `LIQUID_MIXING`, and `COLOR_TRANSITION` — combining Bezier reagent orb flight, `reaction_burst.gdshader` shockwaves, `cauldron_liquid.gdshader` state transitions, 7 custom `GPUParticles2D` emitters, dynamic `Line2D` electrical arcs, camera shake, and 13 procedurally synthesized 16-bit PCM WAV sound effects.

### 4. 7 Gameplay-Affecting Laboratory Upgrades (3 Tiers Each)
1. **Aetheric Crucible Burner**: Accelerates reaction speed (`1.10s -> 0.35s`), boosts thermal/metal RP yield, and shifts live burner shader flames from orange to blue, violet, and white-magenta plasma.
2. **Tri-Crucible Mixing Manifold**: Unlocks the **3rd Catalyst Pedestal** for 3-ingredient multi-stage formulas, enables **Chain-Feed**, and boosts multi-stage RP.
3. **Gyroscopic Reaction Stabilizer**: Spins brass containment rings around the cauldron, salvages RP from failed fizzles, provides Slot-1 resonance feedback, and extends Timed Trial clocks (`+5s` per synthesis at Lv.3).
4. **Grand Apothecary Vault**: Installs an upper apothecary rack in the laboratory and expands reagent pin capacity (`4 -> 16`) and synthesis history (`3 -> 12`).
5. **Clockwork Auto-Experimenter**: Installs an animated brass automaton arm on the workbench that autonomously tests untried combinations on a timer.
6. **Hermetic Spectroscope**: Boosts discovery RP and decodes reaction types, output categories, and leading reagents of undiscovered & Hidden formulas inside the Recipe Compendium.
7. **Rare Resonance Detector**: Illuminates the crystal detector lantern when Slot 1 has undiscovered syntheses, highlights valid partner cards in your cabinet (Lv.3), and powers active Resonance Scans.

---

## Rebuilding From Source

```bash
# 1. Regenerate & verify 50 ingredients, 120 recipes, reachability, and economy (.json + .tres)
python3 tools/game_manifest.py

# 2. Regenerate all 50 illustrated ingredient icons, layered lab props, UI 9-slices, and audio WAVs
python3 tools/gen_ingredients.py
python3 tools/gen_lab_and_ui.py
python3 tools/generate_audio.py

# 3. Run the 10-suite (1,502-assertion) headless Godot 4.5 automated test suite
godot45 --headless --path alchemy_chaos res://tests/TestRunner.tscn
```
