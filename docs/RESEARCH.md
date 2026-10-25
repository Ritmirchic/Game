# ANVILIGHT — research & concept selection

## What was actually researched

I looked at what is currently pulling players into short browser sessions and into long
roguelite sessions, and at *why* those loops work.

**Browser / instant-play market**

- CrazyGames' roguelike category is led by idle-defence hybrids, card roguelikes and
  survival wave games (`Lumber Hero: Idle Defense`, `Tavern Rumble: Roguelike Card`,
  `Cardlike` at a 9.0 rating with "deep progress / can't stop playing" tags).
  Source: <https://www.crazygames.com/t/roguelike>, <https://www.crazygames.com/game/cardlike>
- Browser roguelike round-ups keep recommending the same shapes: deckbuilder runs of
  20–30 minutes, fog-of-war dungeon crawls of 10–20, procedural puzzle runs of 5–15.
  Source: <https://playbrain.games/blog/best-roguelike-games-browser-free>
- 2025–2026 roguelite discussion consistently names *StarVaders* (deckbuilding + grid
  tactics + telegraphed enemy march), *Slay the Spire*, *Balatro*, *Ball x Pit*,
  *Into the Breach*-style tactics.
  Source: <https://rogueliker.com/roguelike-deckbuilders/>,
  <https://www.reddit.com/r/roguelites/comments/1pn6j0j/its_time_best_roguelites_of_2025/>,
  <https://www.reddit.com/r/roguelites/comments/1opkh20/what_are_the_must_play_rogue_lites/>

**Design lessons (not popularity claims)**

- *Into the Breach*: full information beats hidden percentages. Every enemy intent is
  telegraphed, so a turn becomes a solvable puzzle rather than a gamble. This is the
  single most transferable mechanic for a browser game with 30-second onboarding
  ("you can read the board immediately"). Sources:
  <https://www.gamedeveloper.com/game-platforms/road-to-the-igf-subset-games-i-into-the-breach-i->,
  <https://www.rockpapershotgun.com/into-the-breach-interface-design>
- *Vampire Survivors / Brotato*: a run is a build, not a story. Fast restarts, tiny
  numbers, exponential growth.
- *Balatro / Slay the Spire*: drafting one of three options is the most reliably
  satisfying decision shape in roguelites.
- *Emberward* (maze-building tower defence, 2024–2026, "Overwhelmingly Positive"):
  proves that the *board itself* can be the resource — but it is a large project and
  a direct competitor in theme, which pushed me away from a maze-builder.
- *Loop Hero / Backpack Battles / Super Auto Pets*: small boards + adjacency rules
  create depth without content volume.

## Ten generated concepts and their scores

Scoring is 1–5 per criterion, 10 criteria (originality, replayability, fun, visual
potential, progression depth, content scalability, technical feasibility, browser
performance, development complexity *inverted*, automated testability). Max 50.

| # | Concept | Core fusion | Notes | Score |
|---|---------|-------------|-------|-------|
| 1 | **ANVILIGHT** | *Into the Breach* telegraphed grid tactics + *Vampire Survivors* run pacing + *Slay the Spire* drafting + displacement/terrain kills | Perfect information, 8×8 board, protect a forge, shove enemies into vents and walls | **46** |
| 2 | Cog & Candle | Tower defence + deckbuilding on a hex grid | Very content hungry (towers × cards × waves); balance is hard to test automatically | 39 |
| 3 | Ashfall Courier | 2D side-scrolling delivery roguelite | Needs level art volume and physics tuning; weaker testability | 33 |
| 4 | Kiln Keeper | Idle foundry tycoon + roguelite reset | Addictive but shallow decisions; hard to make "fun in 30 seconds" without waiting | 34 |
| 5 | Rune Draft | Auto-battler + card drafting (Super Auto Pets style) | Strong, but AI-only matches feel less personal; visual identity is harder | 40 |
| 6 | Vent Crawler | Grid dungeon crawl with fog of war | Fog removes the "read the board" hook; art-heavy | 35 |
| 7 | Slag & Steel | Backpack-style inventory Tetris + combat | Very original, but UI-heavy and confusing to first-time players | 37 |
| 8 | Ember Chess | Puzzle roguelite of forced-move boards | Great testability, poor replayability once solved | 31 |
| 9 | Wisp Run | Endless runner with upgrades | Cheap to build, no depth, no progression hooks | 27 |
| 10 | Foundry Tactics PvP | Async PvP tactical duels | Needs backend, accounts, matchmaking — explicitly out of scope | 22 |

### Why concept #1 won

1. **Fun in the first 30 seconds** — the board *shows* the future: red tiles tell you what
   will happen. No tutorial required, and the first shove-into-a-vent kill is a story.
2. **Testability** — nearly all mechanics are pure functions over a small board, so the
   AI's plans, damage maths, hazards and progression can be asserted in unit tests
   (54 automated tests in this repo), and a scripted bot can measure the difficulty curve.
3. **Content scalability** — 18 abilities, 18 relics, 13 enemy types and three heroes are
   combinatorial: the value comes from synergies, not from new art per update.
4. **Visual identity** — a forge-lit grid with volumetric glow, ember particles, hand-built
   procedural creatures and an anvil as the thing you protect reads instantly and is
   cheap to render (one canvas, no sprite atlas, no WebGL requirement).
5. **Scope** — one board size, one act structure, no backend, no 3D, no multiplayer.

## What was deliberately *not* copied

No names, characters, art, music, UI layouts, maps or storylines from any reference game.
Everything visual is drawn procedurally in `src/render.ts`; every sound is synthesised in
`src/audio.ts`. The fusion (telegraphed intent + terrace-displacement lethality + forge
defence + heat economy) is the original part.

## Name check (October 2026)

A web search for "ANVILIGHT" returns a streamer handle (a Rumble/Twitch creator posting
under *Anvilight*) and no shipped video game, store listing or trademark use in games.
The name is therefore free to use for this project, but it is not a registered mark and
the streamer overlap is acknowledged here rather than hidden.
