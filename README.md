# PRISMFORGE: Aether Bastion

**PRISMFORGE: Aether Bastion** is an original 2D Runic Pulse-Circuit Roguelite Tower Defense game built with **TypeScript**, **Phaser 3**, and **Vite** — running natively in any modern web browser with zero engine installation required.

---

## Core Gameplay Loop

1. **Route Kinetic Aether Pulses (Left Sanctum — 5x4 Runic Circuit Grid)**
   - Your **Solar Emitter** fires luminous Aether Pulses across the sanctum grid.
   - Place and rotate **Splitters**, **Trinity Refractors**, **Echo Mirrors**, **Solar Magnifiers**, and **Elemental Crucibles** (`Pyro`, `Cryo`, `Volt`, `Midas`) to split, bounce, infuse, and exponentially multiply each Pulse (`x2` -> `x8` -> `x25+`).
   - Drag or click two identical modules together (`Tier I + Tier I -> Tier II`, `Tier II + Tier II -> Tier III Ascended`) to fuse evolved conduits!

2. **Repel the Voidkin Siege (Right Sanctum — 3-Lane Causeway Battlefield)**
   - Whenever an energized Aether Pulse passes through a **Turret Conduit** (*Blaze Cannon*, *Arc Spire*, *Frostrail Lancer*, *Venom Needler*, *Sunbeam Obelisk*, *Singularity Mortar*), it unleashes an amplified real-time volley onto marching Voidkin and multi-phase Void Titans.
   - Click enemies on the causeways to mark a **Priority Target** (`+20%` damage vulnerability) and fire manual **Arcane Overwatch** bolts from the Sunspire Core.

3. **Draft Relics, Weather Celestial Anomalies & Ascend the Starforge Tree**
   - Collect **18 passive Astral Relics** that bend circuit and combat math.
   - Adapt to **5 dynamic Celestial Weather Anomalies** (*Solar Flare*, *Ion Tempest*, *Cryo Nebula*, *Blood Eclipse*).
   - Spend harvested **Starlight** in the permanent **16-node Starforge Skill Tree**, unlock **4 distinct Bastion Wardens** (*Lyra*, *Vex*, *Kael*, *Nyx*), complete **5 Expedition Modes & Tactical Trials**, fill the **Arcane Codex**, and earn **20 Achievements**.

---

## Controls

| Action | Control |
|---|---|
| Place / Move / Swap / Merge Modules | **Drag & Drop** or **Click Source -> Click Destination** |
| Rotate Selected or Hovered Prism | **`R`** key, **Right-Click**, or **`ROTATE [R]`** button |
| Launch Wave / Emit Surge Pulse | **`Space`** or **`START WAVE`** button |
| Warden Tactical Abilities | **`1`** (Pulse Surge), **`2`** (Solar Nova), **`3`** (Warden Ultimate) |
| Focus Enemy + Manual Core Bolt | **Left-Click** on Battlefield Causeways |
| Pause / Options Overlay | **`Esc`** or **`MENU`** button |

---

## Local Development, Testing & Production Build

```bash
# Install dependencies
npm install

# Regenerate 2D vector sprites & optimize artwork
npm run assets

# Start Vite development server
npm run dev

# Run ESLint + TypeScript check + Vite production build (outputs to ./docs)
npm run lint
npm run build

# Run automated unit/integration tests (Vitest) & E2E subpath bundle smoke test
npm test
npm run test:e2e

# Package standalone web build archive (PRISMFORGE_WEB.zip)
npm run package:zip
```

See [`DESIGN.md`](./DESIGN.md) for the market research synthesis, 10 generated hybrid concepts, and 10-criteria selection matrix.
