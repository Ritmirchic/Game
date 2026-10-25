# ANVILIGHT — design notes

## Fantasy

The world went cold. One forge still burns, and everything that hates warmth is walking
toward it. You are a keeper with a lantern; the anvil behind you is the thing that must
survive.

## The loop

```
choose a door ─▶ battle (read intents, shove, kill) ─▶ choose 1 of 3 rewards
      ▲                                                        │
      └──────────── cold forge / rest / vault ◀─────────────────┘
                              │
                     boss at the end of each act (2 acts)
```

A run is 4 rooms plus a boss per act, roughly 12–18 minutes. Death is permanent for the
run; dust, unlocks and achievements persist.

## Core rules

- **8×8 arena.** The keeper starts near the forge; enemies enter from the top lanes.
- **One move + any number of abilities per turn**, limited by Heat (6 for the keeper,
  +2 regeneration per turn, +refunds on kills).
- **The forge** occupies two tiles and has its own health. If either the keeper or the
  forge hits zero, the run ends. Enemies that reach the forge strike it, then spend a
  turn recovering — that rhythm is what makes interception possible.
- **Perfect information.** Every enemy displays exactly the tile it will hit, plus the
  damage, before you act. If you move, it misses the tile and hits nothing.
- **Displacement is the primary weapon.** Shoves push along a direction; a foe shoved
  into a wall takes impact damage, into a steam vent takes 2 extra, into another foe
  damages both. Momentum-style relics extend shove distance.
- **The battle closes in.** After turn 14 the freezing keep begins to chew the forge
  each turn, so kiting forever is not a strategy.

## Content

- **3 keepers** — Emberkeeper (balanced, kill refunds), Slagwarden (tanky, +1 shove,
  anchored damage reduction), Ashwisp (glass, free blink, extra reach).
- **18 abilities** across 4 rarities and 5 targeting shapes: adjacent, line, single
  target, tile, self. Each has one upgrade tier with different numbers, and some change
  behaviour (Vent Call becomes free, Cinder Wall becomes permanent).
- **18 relics** — damage, heat, shields, shove extension, vent immunity, bombs,
  execution damage, adjacency damage, forge repair, economy.
- **13 enemies** — melee swarm, flyer, armoured brute, rooted-in-place spitter,
  exploding bloater, shield chorister, burrowing ambusher, forge eater, plus elite
  sentinel and witch and three bosses with distinct special beats (quake ring, summon,
  beam + spawn).
- **Room types** — Ash Hall, Deep Foundry (elite), Cold Forge (shop, rerollable),
  Ember Rest (heal / repair / sharpen), Sealed Vault (relic draft), Heart of the Keep.
- **12 achievements**, a codex (keepers, abilities, relics, bestiary, achievements,
  statistics) and a Forgeworks meta shop (hero unlocks, extra heat, deeper foundations,
  a common relic at the start of every run).

## Interface

- Title → keeper select → route → battle / shop / rest / vault → reward → result.
- Battle HUD: keeper plate with shield, forge plate, turn + wave counter, Heat pips,
  ability belt (click or `1`–`5`), End Turn (`Space`), inspect panel for hovered foes,
  pause (`Esc`) with settings and abandon.
- All interactive elements are real: settings sliders write to the save immediately,
  codex tabs filter content, shop reroll costs Embers, achievements pop in-session.
- Responsive down to phone widths: board scales, ability belt becomes a scroll strip,
  HUD stacks. `prefers-reduced-motion` and an in-game toggle both calm effects.

## Art & audio

Everything is generated at runtime — no external images or audio files ship with the game.

- Grid tiles, bricks, vent grates, floor plates, forge basin and anvil are drawn with
  canvas gradients and rounded paths, cached into an offscreen board canvas.
- Creatures and keepers are hand-built path/cluster illustrations (carapaces, hoods,
  lanterns, crowns, masks) with ember eyes, rim glow, ground shadows and tier rings for
  elites and bosses. Portraits reuse the same drawing code at a different scale.
- FX: pooled particles (sparks, smoke, debris, embers), floating damage text, screen
  shake, telegraphed dash-arrows, danger tiles, beams and bomb blasts.
- Audio: a WebAudio synth with a noise buffer, filtered bursts for impacts, a drone pad
  whose brightness tracks board tension, and short tonal motifs for rewards and results.
  Music starts only after the first user gesture (autoplay policy).

## Saving

Single versioned JSON document in `localStorage` (`anvilight:save:v3`), sanitised on read:
unknown fields are dropped, numbers clamped, achievements coerced to booleans, and a run
is only kept if it can be resumed (including a mid-battle RNG snapshot so the same arena
and wave schedule come back). Corrupt payloads are removed and replaced with a fresh save;
storage failures fall back to an in-memory save for the session.
