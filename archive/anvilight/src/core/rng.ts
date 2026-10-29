// ── Deterministic RNG (mulberry32) ───────────────────────────────────────────
// The generator state lives inside the run save so a resumed run keeps rolling
// exactly the same rooms, encounters and loot.

export class Rng {
  state: number;

  constructor(seed: number) {
    this.state = seed >>> 0 || 0x9e3779b9;
  }

  next(): number {
    this.state = (this.state + 0x6d2b79f5) >>> 0;
    let t = this.state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }

  int(maxExclusive: number): number {
    return Math.floor(this.next() * maxExclusive);
  }

  range(minInclusive: number, maxInclusive: number): number {
    return minInclusive + this.int(maxInclusive - minInclusive + 1);
  }

  chance(p: number): boolean {
    return this.next() < p;
  }

  pick<T>(items: readonly T[]): T {
    if (items.length === 0) throw new Error('Rng.pick on empty array');
    return items[this.int(items.length)];
  }

  /** Weighted pick; weights must be positive. */
  pickWeighted<T>(items: readonly T[], weight: (item: T) => number): T {
    let total = 0;
    for (const it of items) total += Math.max(0, weight(it));
    let roll = this.next() * total;
    for (const it of items) {
      roll -= Math.max(0, weight(it));
      if (roll <= 0) return it;
    }
    return items[items.length - 1];
  }

  shuffle<T>(items: T[]): T[] {
    for (let i = items.length - 1; i > 0; i--) {
      const j = this.int(i + 1);
      [items[i], items[j]] = [items[j], items[i]];
    }
    return items;
  }

  sample<T>(items: readonly T[], count: number): T[] {
    return this.shuffle(items.slice()).slice(0, Math.max(0, Math.min(count, items.length)));
  }
}

export function seedFromNow(): number {
  return (Date.now() ^ Math.floor(Math.random() * 0xffffffff)) >>> 0;
}

export function shortSeed(seed: number): string {
  return (seed >>> 0).toString(36).toUpperCase().padStart(6, '0').slice(-6);
}
