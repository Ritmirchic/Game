// ── Arena generation ─────────────────────────────────────────────────────────
import { Board, DIRS, vec, type Vec } from './types';

const ADJ: readonly Vec[] = DIRS;
import type { Rng } from './rng';

export const GRID_W = 8;
export const GRID_H = 8;
export const FORGE_TILES: Vec[] = [vec(3, 7), vec(4, 7)];
export const HERO_SPAWN: Vec = vec(3, 5);

export type ArenaStyle = 'hall' | 'foundry' | 'vault' | 'boss';

export interface Arena {
  board: Board;
  heroStart: Vec;
  forgeTiles: Vec[];
  spawnTiles: Vec[];
}

function neighboursFree(board: Board, p: Vec): number {
  return board.neighbours(p).length;
}

/**
 * Build a playable arena: an open forge hall at the bottom, scattered cover in
 * the middle, steam vents to shove things into, and open lanes from the top so
 * enemies always have a route down.
 */
export function makeArena(rng: Rng, style: ArenaStyle): Arena {
  const board = new Board(GRID_W, GRID_H);
  for (const t of board.tiles) t.variant = rng.next();

  const forgeTiles: Vec[] = [vec(3, 7), vec(4, 7)];
  for (const f of forgeTiles) board.set(f, 'forge');

  const heroStart = style === 'boss' ? vec(3, 6) : HERO_SPAWN;
  board.set(heroStart, 'floor');

  const wallCount = style === 'boss' ? 3 : style === 'foundry' ? 6 : 7;
  const ventCount = style === 'boss' ? 3 : style === 'vault' ? 4 : 2;

  const reserved = new Set<string>();
  const mark = (p: Vec) => reserved.add(`${p.x},${p.y}`);
  for (const f of forgeTiles) mark(f);
  mark(heroStart);
  for (let x = heroStart.x - 1; x <= heroStart.x + 1; x++) mark(vec(x, heroStart.y));
  for (const f of forgeTiles) {
    mark(vec(f.x, f.y - 1));
    mark(vec(f.x + 1, f.y - 1));
    mark(vec(f.x - 1, f.y - 1));
  }

  const candidates: Vec[] = [];
  for (let y = 1; y <= 5; y++) {
    for (let x = 0; x < GRID_W; x++) {
      const p = vec(x, y);
      if (reserved.has(`${x},${y}`)) continue;
      candidates.push(p);
    }
  }
  rng.shuffle(candidates);

  // Walls: never in the two middle spawn lanes, never sealing the board edge.
  let placed = 0;
  for (const p of candidates) {
    if (placed >= wallCount) break;
    if (p.x >= 3 && p.x <= 4 && p.y >= 4) continue;
    if (neighboursFree(board, p) < 3) continue;
    board.setWall(p);
    if (!lanesOpen(board, forgeTiles, heroStart)) {
      board.set(p, 'floor');
      continue;
    }
    placed++;
  }

  // Vents: hazards for shoving enemies into.
  const ventCandidates = candidates.filter((p) => board.at(p)?.kind === 'floor' && !reserved.has(`${p.x},${p.y}`));
  rng.shuffle(ventCandidates);
  let vents = 0;
  for (const p of ventCandidates) {
    if (vents >= ventCount) break;
    if (p.y > 6) continue;
    board.set(p, 'vent');
    vents++;
  }

  const spawnTiles: Vec[] = [];
  for (let x = 0; x < GRID_W; x++) {
    for (const y of [0, 1]) {
      const p = vec(x, y);
      if (board.walkable(p)) spawnTiles.push(p);
    }
  }

  return { board, heroStart, forgeTiles, spawnTiles };
}

/**
 * Validates that the arena is one connected region: every walkable tile can
 * reach the tiles next to the forge, so no spawned enemy is ever stranded.
 */
function lanesOpen(board: Board, forgeTiles: Vec[], heroStart: Vec): boolean {
  const goal = (p: Vec) => forgeTiles.some((f) => Math.abs(f.x - p.x) + Math.abs(f.y - p.y) === 1);
  if (!board.pathTo(heroStart, goal)) return false;
  for (const f of forgeTiles) {
    const reach = board.distanceField(f, (p) => forgeTiles.some((g) => g.x === p.x && g.y === p.y));
    for (let y = 0; y < board.h; y++) {
      for (let x = 0; x < board.w; x++) {
        const p = vec(x, y);
        if (!board.walkable(p)) continue;
        if (forgeTiles.some((g) => ADJ.some((d) => g.x + d.x === x && g.y + d.y === y))) continue;
        if (reach[y * board.w + x] === Infinity) return false;
      }
    }
  }
  return true;
}

/** Free walkable tiles, optionally filtered by a predicate. */
export function freeTiles(board: Board, isFree: (p: Vec) => boolean): Vec[] {
  const out: Vec[] = [];
  for (let y = 0; y < board.h; y++) {
    for (let x = 0; x < board.w; x++) {
      const p = vec(x, y);
      if (board.walkable(p) && isFree(p)) out.push(p);
    }
  }
  return out;
}
