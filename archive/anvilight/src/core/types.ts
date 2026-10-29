// ── Shared types & tiny geometry helpers ─────────────────────────────────────

export interface Vec {
  x: number;
  y: number;
}

export const vec = (x: number, y: number): Vec => ({ x, y });
export const eq = (a: Vec, b: Vec): boolean => a.x === b.x && a.y === b.y;
export const vkey = (p: Vec): string => `${p.x},${p.y}`;
export const manhattan = (a: Vec, b: Vec): number => Math.abs(a.x - b.x) + Math.abs(a.y - b.y);
export const clamp = (n: number, lo: number, hi: number): number => (n < lo ? lo : n > hi ? hi : n);

export const DIRS: readonly Vec[] = [
  { x: 0, y: -1 },
  { x: 1, y: 0 },
  { x: 0, y: 1 },
  { x: -1, y: 0 },
];

/** 8-way neighbourhood used for splash damage and adjacency checks. */
export const DIRS8: readonly Vec[] = [
  { x: 0, y: -1 },
  { x: 1, y: -1 },
  { x: 1, y: 0 },
  { x: 1, y: 1 },
  { x: 0, y: 1 },
  { x: -1, y: 1 },
  { x: -1, y: 0 },
  { x: -1, y: -1 },
];

export type TileKind = 'floor' | 'wall' | 'vent' | 'forge';

export interface Tile {
  kind: TileKind;
  /** Stable random value per tile, used for visual variety. */
  variant: number;
  /** Scorch marks left by explosions, purely cosmetic. */
  scorch: number;
}

export interface AbilityInst {
  id: string;
  level: number;
}

export interface Intent {
  kind: 'attack' | 'move' | 'special';
  /** Tile the enemy is committed to hitting (or watching, for moves). */
  target?: Vec;
  /** Planned steps for this enemy phase. */
  path: Vec[];
  label: string;
  /** Visual kind of telegraphed attack. */
  style?: 'melee' | 'ranged' | 'splash' | 'spawn';
}

export interface EnemyState {
  uid: number;
  typeId: string;
  name: string;
  pos: Vec;
  hp: number;
  maxHp: number;
  shield: number;
  dmg: number;
  range: number;
  speed: number;
  stun: number;
  mark: number;
  burn: number;
  elite: boolean;
  boss: boolean;
  armored: boolean;
  intent: Intent | null;
  /** Cosmetic spawn animation timer (ms). */
  bornAt: number;
}

export interface HeroState {
  heroId: string;
  name: string;
  pos: Vec;
  hp: number;
  maxHp: number;
  shield: number;
  heat: number;
  maxHeat: number;
  heatRegen: number;
  moveRange: number;
  movesLeft: boolean;
  abilities: AbilityInst[];
}

export interface ForgeState {
  /** Forge occupies a row of tiles; all of them are targets. */
  tiles: Vec[];
  hp: number;
  maxHp: number;
  cracked: number;
}

export type BattlePhase = 'player' | 'resolve' | 'won' | 'lost';

export interface Battle {
  board: Board;
  hero: HeroState;
  forge: ForgeState;
  enemies: EnemyState[];
  turn: number;
  phase: BattlePhase;
  /** Reinforcement waves still waiting to arrive. */
  waves: EnemyWave[];
  waveIndex: number;
  seed: number;
  /** Relic ids in effect for this run. */
  relics: string[];
  /** Echo Anvil already spent its free cast this battle. */
  echoUsed: boolean;
  /** Bombs placed on the board. */
  bombs: { pos: Vec; dmg: number; fuse: number; stun: boolean; from: 'hero' }[];
  kills: number;
  killsThisTurn: number;
  castsThisTurn: number;
  shoveKills: number;
  eliteBonus: boolean;
  bossId: string | null;
  heroPassive: string;
  roomKind: 'battle' | 'elite' | 'boss';
  nextUid: number;
}

export interface EnemyWave {
  /** Enemy type ids queued for this wave. */
  groups: { typeId: string; count: number }[];
  /** Player turn on which this wave walks in. */
  arrival: number;
}

export type BattleEvent =
  | { t: 'move'; who: 'hero'; from: Vec; to: Vec; path: Vec[] }
  | { t: 'enemyMove'; uid: number; from: Vec; to: Vec; path: Vec[] }
  | { t: 'hit'; at: Vec; uid?: number; hero?: boolean; forge?: boolean; amount: number; blocked?: boolean }
  | { t: 'heal'; at: Vec; amount: number }
  | { t: 'shield'; at: Vec; amount: number }
  | { t: 'push'; uid: number; from: Vec; to: Vec; steps: number; impact: boolean }
  | { t: 'slam'; at: Vec; kind: string }
  | { t: 'death'; at: Vec; uid: number; boss?: boolean }
  | { t: 'spawn'; at: Vec; uid: number }
  | { t: 'bomb'; at: Vec }
  | { t: 'explode'; at: Vec }
  | { t: 'vent'; at: Vec }
  | { t: 'wall'; at: Vec }
  | { t: 'burn'; at: Vec; uid: number }
  | { t: 'mark'; at: Vec; uid: number }
  | { t: 'turn'; n: number }
  | { t: 'wave'; n: number }
  | { t: 'shake'; power: number }
  | { t: 'text'; at: Vec; text: string; tone: 'good' | 'bad' | 'ember' }
  | { t: 'won' }
  | { t: 'lost'; reason: string };

// ── Board ────────────────────────────────────────────────────────────────────

export class Board {
  readonly w: number;
  readonly h: number;
  readonly tiles: Tile[];

  constructor(w = 8, h = 8) {
    this.w = w;
    this.h = h;
    this.tiles = Array.from({ length: w * h }, () => ({ kind: 'floor' as TileKind, variant: 0, scorch: 0 }));
  }

  clone(): Board {
    const b = new Board(this.w, this.h);
    for (let i = 0; i < this.tiles.length; i++) b.tiles[i] = { ...this.tiles[i] };
    return b;
  }

  inside(p: Vec): boolean {
    return p.x >= 0 && p.y >= 0 && p.x < this.w && p.y < this.h;
  }

  idx(p: Vec): number {
    return p.y * this.w + p.x;
  }

  at(p: Vec): Tile | null {
    return this.inside(p) ? this.tiles[this.idx(p)] : null;
  }

  isWall(p: Vec): boolean {
    const t = this.at(p);
    return !t || t.kind === 'wall';
  }

  /** Walkable for heroes and enemies (walls block). */
  walkable(p: Vec): boolean {
    const t = this.at(p);
    return !!t && t.kind !== 'wall';
  }

  set(p: Vec, kind: TileKind): void {
    const t = this.at(p);
    if (t) t.kind = kind;
  }

  setWall(p: Vec): void {
    if (this.inside(p)) this.tiles[this.idx(p)].kind = 'wall';
  }

  neighbours(p: Vec): Vec[] {
    const out: Vec[] = [];
    for (const d of DIRS) {
      const n = { x: p.x + d.x, y: p.y + d.y };
      if (this.walkable(n)) out.push(n);
    }
    return out;
  }

  /** Breadth-first distance field from a source tile (walls blocked). */
  distanceField(from: Vec, blocked?: (p: Vec) => boolean): number[] {
    const dist = new Array(this.w * this.h).fill(Infinity);
    if (!this.inside(from)) return dist;
    dist[this.idx(from)] = 0;
    const queue: Vec[] = [from];
    for (let head = 0; head < queue.length; head++) {
      const cur = queue[head];
      const d = dist[this.idx(cur)];
      for (const d4 of DIRS) {
        const n = { x: cur.x + d4.x, y: cur.y + d4.y };
        if (!this.inside(n) || !this.walkable(n)) continue;
        if (blocked && blocked(n)) continue;
        if (dist[this.idx(n)] <= d + 1) continue;
        dist[this.idx(n)] = d + 1;
        queue.push(n);
      }
    }
    return dist;
  }

  /** Shortest path of tiles (excluding start) toward the nearest tile matching `goal`. */
  pathTo(from: Vec, goal: (p: Vec) => boolean, blocked?: (p: Vec) => boolean): Vec[] | null {
    const prev = new Map<number, number>();
    const seen = new Uint8Array(this.w * this.h);
    const start = this.idx(from);
    seen[start] = 1;
    const queue: Vec[] = [from];
    for (let head = 0; head < queue.length; head++) {
      const cur = queue[head];
      if (goal(cur) && !eq(cur, from)) {
        const path: Vec[] = [];
        let ci = this.idx(cur);
        while (ci !== start) {
          path.push({ x: ci % this.w, y: Math.floor(ci / this.w) });
          const p = prev.get(ci);
          if (p === undefined) break;
          ci = p;
        }
        return path.reverse();
      }
      for (const d4 of DIRS) {
        const n = { x: cur.x + d4.x, y: cur.y + d4.y };
        if (!this.inside(n) || !this.walkable(n)) continue;
        if (blocked && blocked(n)) continue;
        const ni = this.idx(n);
        if (seen[ni]) continue;
        seen[ni] = 1;
        prev.set(ni, this.idx(cur));
        queue.push(n);
      }
    }
    return null;
  }

  /** Straight line of tiles from a point in a direction, stopping before walls. */
  line(from: Vec, dir: Vec, length: number, includeFirst = true): Vec[] {
    const out: Vec[] = [];
    for (let i = 1; i <= length; i++) {
      const p = { x: from.x + dir.x * i, y: from.y + dir.y * i };
      if (!this.walkable(p)) break;
      out.push(p);
      if (!includeFirst && i === 1) out.pop();
    }
    return out;
  }

  hasLineOfSight(a: Vec, b: Vec): boolean {
    if (a.x === b.x) {
      const step = b.y > a.y ? 1 : -1;
      for (let y = a.y + step; y !== b.y; y += step) if (!this.walkable({ x: a.x, y })) return false;
      return true;
    }
    if (a.y === b.y) {
      const step = b.x > a.x ? 1 : -1;
      for (let x = a.x + step; x !== b.x; x += step) if (!this.walkable({ x, y: a.y })) return false;
      return true;
    }
    return false;
  }
}
