import { MODULES } from './content';
import {
  Direction,
  ElementType,
  GridCell,
  ModuleId,
  PlacedModule,
  PreviewSegment,
  PulsePacket,
  RelicId,
  TileMutation,
  TurretFireCommand,
  WardenId,
  WeatherId,
} from './types';

export const GRID_COLS = 5;
export const GRID_ROWS = 4;
export const BENCH_SIZE = 5;

export const DIR_DELTA: Record<Direction, { dc: number; dr: number }> = {
  0: { dc: 1, dr: 0 }, // East
  1: { dc: 0, dr: 1 }, // South
  2: { dc: -1, dr: 0 }, // West
  3: { dc: 0, dr: -1 }, // North
};

let nextUidCounter = 1;
export function makeUid(prefix: string): string {
  return `${prefix}_${nextUidCounter++}`;
}

export interface CircuitModifiers {
  wardenId: WardenId;
  weatherId: WeatherId;
  relics: RelicId[];
  bonusHops: number;
  baseCritChance: number;
  pulseSpeedScale: number;
  dualTopEmitter: boolean;
  mirrorChallengeBonus: boolean;
}

export interface CircuitStepResult {
  fireCommands: TurretFireCommand[];
  goldMinted: number;
  peakMultReached: number;
  maxElementsOnPulse: number;
  triggeredRelics: RelicId[];
}

export class CircuitEngine {
  public cells: GridCell[][] = [];
  public bench: Array<PlacedModule | null> = Array(BENCH_SIZE).fill(null);
  public activePulses: PulsePacket[] = [];

  constructor(unlockedCorners = 0, extraSpecialTiles = 0, prismaticStart = false) {
    this.initGrid(unlockedCorners, extraSpecialTiles, prismaticStart);
  }

  public initGrid(unlockedCorners = 0, extraSpecialTiles = 0, prismaticStart = false): void {
    this.cells = [];
    for (let r = 0; r < GRID_ROWS; r++) {
      const row: GridCell[] = [];
      for (let c = 0; c < GRID_COLS; c++) {
        row.push({
          col: c,
          row: r,
          mutation: 'normal',
          module: null,
        });
      }
      this.cells.push(row);
    }

    // 4 outer corners start locked unless unlocked by Skill Tree
    const corners: Array<[number, number]> = [
      [0, 0],
      [4, 0],
      [0, 3],
      [4, 3],
    ];
    for (let i = unlockedCorners; i < corners.length; i++) {
      const [cc, rr] = corners[i];
      this.cells[rr][cc].mutation = 'locked';
    }

    // Guaranteed baseline Radiant tile at center (col 2, row 1)
    this.cells[1][2].mutation = prismaticStart ? 'prismatic' : 'radiant';
    if (prismaticStart) {
      this.cells[2][3].mutation = 'prismatic';
    }

    const candidates: Array<[number, number]> = [
      [2, 2],
      [3, 1],
      [1, 2],
      [3, 2],
      [2, 0],
    ];
    let placed = 0;
    for (const [cc, rr] of candidates) {
      if (placed >= extraSpecialTiles) break;
      if (this.cells[rr][cc].mutation === 'normal') {
        this.cells[rr][cc].mutation = placed % 2 === 0 ? 'haste' : 'radiant';
        placed++;
      }
    }
  }

  public getCell(col: number, row: number): GridCell | null {
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return null;
    return this.cells[row][col];
  }

  public unlockTile(col: number, row: number): boolean {
    const cell = this.getCell(col, row);
    if (!cell || cell.mutation !== 'locked') return false;
    cell.mutation = 'normal';
    return true;
  }

  public upgradeRandomNormalTiles(count: number, mutation: TileMutation = 'radiant'): number {
    const normals: GridCell[] = [];
    for (let r = 0; r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        if (this.cells[r][c].mutation === 'normal') {
          normals.push(this.cells[r][c]);
        }
      }
    }
    let upgraded = 0;
    while (upgraded < count && normals.length > 0) {
      const idx = Math.floor(Math.random() * normals.length);
      normals[idx].mutation = mutation;
      normals.splice(idx, 1);
      upgraded++;
    }
    return upgraded;
  }

  public createModule(defId: ModuleId, tier: 1 | 2 | 3 = 1, rotation: Direction = 0): PlacedModule {
    return {
      uid: makeUid('mod'),
      defId,
      tier,
      rotation,
      pulseCounter: 0,
      totalDamageDealt: 0,
    };
  }

  public placeInFirstAvailable(mod: PlacedModule): { target: 'grid' | 'bench'; index: number; row?: number } | null {
    // Prefer bench first if bought during/between waves? Actually if grid has open normal slot on emitter row, or bench!
    // Checking bench first keeps player intentional, unless bench is full then place on grid.
    for (let i = 0; i < BENCH_SIZE; i++) {
      if (!this.bench[i]) {
        this.bench[i] = mod;
        return { target: 'bench', index: i };
      }
    }
    for (let r = 0; r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        const cell = this.cells[r][c];
        if (cell.mutation !== 'locked' && !cell.module) {
          cell.module = mod;
          return { target: 'grid', index: c, row: r };
        }
      }
    }
    return null;
  }

  public hasSpaceForPurchase(defId: ModuleId): boolean {
    for (let i = 0; i < BENCH_SIZE; i++) {
      if (!this.bench[i]) return true;
      if (this.bench[i]!.defId === defId && this.bench[i]!.tier === 1) return true;
    }
    for (let r = 0; r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        const cell = this.cells[r][c];
        if (cell.mutation === 'locked') continue;
        if (!cell.module) return true;
        if (cell.module.defId === defId && cell.module.tier === 1) return true;
      }
    }
    return false;
  }

  public buyOrAutoMergeModule(defId: ModuleId): { placed: boolean; mergedToTier: 1 | 2 | 3 | null } {
    const fresh = this.createModule(defId, 1, 0);
    const slot = this.placeInFirstAvailable(fresh);
    if (!slot) {
      // Try auto-merging directly onto a Tier 1 copy on bench or grid
      for (let i = 0; i < BENCH_SIZE; i++) {
        const bm = this.bench[i];
        if (bm && bm.defId === defId && bm.tier === 1) {
          bm.tier = 2;
          return { placed: true, mergedToTier: 2 };
        }
      }
      for (let r = 0; r < GRID_ROWS; r++) {
        for (let c = 0; c < GRID_COLS; c++) {
          const gm = this.cells[r][c].module;
          if (gm && gm.defId === defId && gm.tier === 1) {
            gm.tier = 2;
            return { placed: true, mergedToTier: 2 };
          }
        }
      }
      return { placed: false, mergedToTier: null };
    }
    return { placed: true, mergedToTier: null };
  }

  /**
   * Moves, swaps, or merges a module from source slot to destination slot.
   */
  public moveOrMerge(
    from: { zone: 'grid'; col: number; row: number } | { zone: 'bench'; idx: number },
    to: { zone: 'grid'; col: number; row: number } | { zone: 'bench'; idx: number }
  ): { moved: boolean; mergedTier: 2 | 3 | null } {
    if (
      from.zone === to.zone &&
      ((from.zone === 'bench' && to.zone === 'bench' && from.idx === to.idx) ||
        (from.zone === 'grid' && to.zone === 'grid' && from.col === to.col && from.row === to.row))
    ) {
      return { moved: false, mergedTier: null };
    }

    const getSlot = (loc: typeof from): PlacedModule | null => {
      if (loc.zone === 'bench') return this.bench[loc.idx] ?? null;
      const cell = this.getCell(loc.col, loc.row);
      return cell ? cell.module : null;
    };

    const setSlot = (loc: typeof from, val: PlacedModule | null): boolean => {
      if (loc.zone === 'bench') {
        if (loc.idx < 0 || loc.idx >= BENCH_SIZE) return false;
        this.bench[loc.idx] = val;
        return true;
      }
      const cell = this.getCell(loc.col, loc.row);
      if (!cell || cell.mutation === 'locked') return false;
      cell.module = val;
      return true;
    };

    if (to.zone === 'grid') {
      const destCell = this.getCell(to.col, to.row);
      if (!destCell || destCell.mutation === 'locked') return { moved: false, mergedTier: null };
    }

    const srcMod = getSlot(from);
    if (!srcMod) return { moved: false, mergedTier: null };
    const dstMod = getSlot(to);

    // Check Merge!
    if (dstMod && dstMod.defId === srcMod.defId && dstMod.tier === srcMod.tier && dstMod.tier < 3) {
      const nextTier = (dstMod.tier + 1) as 2 | 3;
      dstMod.tier = nextTier;
      dstMod.totalDamageDealt += srcMod.totalDamageDealt;
      setSlot(from, null);
      return { moved: true, mergedTier: nextTier };
    }

    // Swap or Move
    setSlot(to, srcMod);
    setSlot(from, dstMod);
    return { moved: true, mergedTier: null };
  }

  public rotateAt(loc: { zone: 'grid'; col: number; row: number } | { zone: 'bench'; idx: number }): boolean {
    const mod =
      loc.zone === 'bench' ? this.bench[loc.idx] : this.getCell(loc.col, loc.row)?.module ?? null;
    if (!mod) return false;
    const def = MODULES[mod.defId];
    if (!def.rotatable) return false;
    mod.rotation = ((mod.rotation + 1) % 4) as Direction;
    return true;
  }

  public sellAt(loc: { zone: 'grid'; col: number; row: number } | { zone: 'bench'; idx: number }): number {
    const mod =
      loc.zone === 'bench' ? this.bench[loc.idx] : this.getCell(loc.col, loc.row)?.module ?? null;
    if (!mod) return 0;
    const def = MODULES[mod.defId];
    const tierMult = mod.tier === 1 ? 1 : mod.tier === 2 ? 2 : 4;
    const refund = Math.max(8, Math.round(def.cost * tierMult * 0.65));
    if (loc.zone === 'bench') {
      this.bench[loc.idx] = null;
    } else {
      const cell = this.getCell(loc.col, loc.row);
      if (cell) cell.module = null;
    }
    return refund;
  }

  public getEmitterRows(mods: CircuitModifiers): Array<{ row: number; powerScale: number }> {
    const emitters: Array<{ row: number; powerScale: number }> = [{ row: 1, powerScale: 1.0 }];
    if (mods.dualTopEmitter) {
      emitters.push({ row: 0, powerScale: 0.62 });
    }
    if (mods.relics.includes('relic_twin_dynamo')) {
      emitters.push({ row: 3, powerScale: 0.8 });
    }
    return emitters;
  }

  public emitPulseWave(mods: CircuitModifiers, extraPowerMult = 1.0): number {
    const emitters = this.getEmitterRows(mods);
    const maxHops =
      10 +
      mods.bonusHops +
      (mods.wardenId === 'vex' ? 2 : 0);
    const baseSpeed =
      4.4 *
      mods.pulseSpeedScale *
      (mods.weatherId === 'ion_storm' ? 1.35 : 1.0);

    let spawned = 0;
    for (const em of emitters) {
      const startCell = this.getCell(0, em.row);
      let startCol = 0;
      if (startCell && startCell.mutation === 'locked') {
        startCol = 1;
      }
      this.activePulses.push({
        id: makeUid('pulse'),
        col: startCol,
        row: em.row,
        dir: 0,
        progress: 0,
        speedCellsPerSec: baseSpeed,
        powerMult: em.powerScale * extraPowerMult,
        critBonus: mods.weatherId === 'ion_storm' ? 0.15 : 0,
        elements: new Set<ElementType>(),
        hopsRemaining: maxHops,
        bounces: 0,
        tilesVisited: 1,
      });
      spawned++;
    }
    return spawned;
  }

  /**
   * Resolves what happens when a pulse reaches the center (progress >= 0.5) of its current cell.
   * We mark processed center via a negative sign trick or explicit Set; let's use progress threshold crossing!
   */
  public stepPulses(dt: number, mods: CircuitModifiers, forcePyroBuff = false): CircuitStepResult {
    const result: CircuitStepResult = {
      fireCommands: [],
      goldMinted: 0,
      peakMultReached: 1,
      maxElementsOnPulse: 0,
      triggeredRelics: [],
    };

    const markRelic = (r: RelicId) => {
      if (!result.triggeredRelics.includes(r)) result.triggeredRelics.push(r);
    };

    const nextPulses: PulsePacket[] = [];

    for (const p of this.activePulses) {
      const cell = this.getCell(p.col, p.row);
      if (!cell || cell.mutation === 'locked') {
        continue;
      }

      const speedMult = cell.mutation === 'haste' ? 1.5 : 1.0;
      const prevProg = p.progress;
      p.progress += dt * p.speedCellsPerSec * speedMult;

      if (forcePyroBuff) {
        p.elements.add('pyro');
      }

      // Trigger cell center interaction at progress 0.5
      if (prevProg < 0.5 && p.progress >= 0.5) {
        // Apply tile mutation effects
        if (cell.mutation === 'radiant') {
          p.powerMult *= 1.35;
        } else if (cell.mutation === 'haste') {
          p.critBonus += 0.15;
        } else if (cell.mutation === 'prismatic') {
          p.elements.add('pyro');
          p.elements.add('cryo');
          p.elements.add('volt');
        }

        const mod = cell.module;
        if (mod) {
          mod.pulseCounter++;
          const def = MODULES[mod.defId];
          const tIdx = mod.tier - 1;
          const glassBoost = mods.relics.includes('relic_glass_lens') && def.category === 'prism' ? 0.18 : 0;
          if (glassBoost > 0) markRelic('relic_glass_lens');

          if (def.element) {
            p.elements.add(def.element);
          }

          if (def.category === 'turret') {
            // Compute effective power for this turret shot
            let shotMult = p.powerMult;
            if (p.tilesVisited >= 4 && mods.relics.includes('relic_prism_crown')) {
              shotMult *= 1.55;
              markRelic('relic_prism_crown');
            }
            if (p.elements.size >= 2 && mods.relics.includes('relic_trinity')) {
              shotMult *= 1.65;
              markRelic('relic_trinity');
            }
            if (mod.tier >= 2 && mods.relics.includes('relic_archon_shard')) {
              shotMult *= 1.4;
              markRelic('relic_archon_shard');
            }

            result.peakMultReached = Math.max(result.peakMultReached, shotMult);
            result.maxElementsOnPulse = Math.max(result.maxElementsOnPulse, p.elements.size);

            result.fireCommands.push({
              moduleUid: mod.uid,
              defId: mod.defId,
              tier: mod.tier,
              col: p.col,
              row: p.row,
              powerMult: shotMult,
              critChance: Math.min(0.95, mods.baseCritChance + p.critBonus),
              elements: Array.from(p.elements),
              comboChain: p.tilesVisited + p.bounces,
            });

            // Pass-through residual pulse
            const retain = mods.relics.includes('relic_capacitor')
              ? Math.max(0.92, def.passThroughRatio[tIdx])
              : def.passThroughRatio[tIdx];
            if (mods.relics.includes('relic_capacitor')) markRelic('relic_capacitor');
            p.powerMult *= retain;
          } else {
            // PRISM / MODIFIER MODULE
            const factor = def.multFactor[tIdx] + glassBoost;

            if (mod.defId === 'splitter' || mod.defId === 'tri_prism') {
              const outDirs: Direction[] =
                mod.defId === 'splitter'
                  ? ([((mod.rotation + 1) % 4) as Direction, ((mod.rotation + 3) % 4) as Direction])
                  : ([
                      mod.rotation,
                      ((mod.rotation + 1) % 4) as Direction,
                      ((mod.rotation + 3) % 4) as Direction,
                    ]);

              const branchedPower = Math.min(150, p.powerMult * factor);
              result.peakMultReached = Math.max(result.peakMultReached, branchedPower);
              result.maxElementsOnPulse = Math.max(result.maxElementsOnPulse, p.elements.size);

              if (p.hopsRemaining > 1 && nextPulses.length + this.activePulses.length < 48) {
                for (const d of outDirs) {
                  nextPulses.push({
                    id: makeUid('pulse'),
                    col: p.col,
                    row: p.row,
                    dir: d,
                    progress: 0.52,
                    speedCellsPerSec: p.speedCellsPerSec,
                    powerMult: branchedPower,
                    critBonus: p.critBonus,
                    elements: new Set(p.elements),
                    hopsRemaining: p.hopsRemaining - 1,
                    bounces: p.bounces + 1,
                    tilesVisited: p.tilesVisited,
                  });
                }
              }
              continue; // Original pulse consumed into split branches
            } else if (mod.defId === 'mirror') {
              p.dir = ((mod.rotation + 1) % 4) as Direction;
              let mirrorMult = factor;
              if (mods.wardenId === 'vex') mirrorMult += 0.15;
              if (mods.mirrorChallengeBonus) mirrorMult += 0.35;
              if (mods.relics.includes('relic_void_mirror')) {
                mirrorMult += 0.25;
                if (p.bounces < 4) p.hopsRemaining += 2;
                markRelic('relic_void_mirror');
              }
              p.powerMult = Math.min(150, p.powerMult * mirrorMult);
              p.bounces++;
            } else if (mod.defId === 'midas_core') {
              p.powerMult = Math.min(150, p.powerMult * factor);
              if (mod.pulseCounter % 2 === 0) {
                const goldGain = mod.tier === 1 ? 2 : mod.tier === 2 ? 4 : 8;
                result.goldMinted += goldGain;
              }
            } else {
              // amp_lens, pyro_lens, cryo_lens, volt_lens
              p.powerMult = Math.min(150, p.powerMult * factor);
              if (mod.defId === 'volt_lens') {
                p.critBonus += 0.2;
              }
            }

            result.peakMultReached = Math.max(result.peakMultReached, p.powerMult);
            result.maxElementsOnPulse = Math.max(result.maxElementsOnPulse, p.elements.size);
          }
        }
      }

      // Advance to next cell when progress >= 1.0
      if (p.progress >= 1.0) {
        if (p.hopsRemaining <= 1) {
          continue;
        }
        const delta = DIR_DELTA[p.dir];
        const nc = p.col + delta.dc;
        const nr = p.row + delta.dr;
        const nextCell = this.getCell(nc, nr);
        if (!nextCell || nextCell.mutation === 'locked') {
          continue;
        }
        p.col = nc;
        p.row = nr;
        p.progress -= 1.0;
        p.hopsRemaining -= 1;
        p.tilesVisited += 1;
      }

      nextPulses.push(p);
    }

    this.activePulses = nextPulses;
    return result;
  }

  /**
   * Computes deterministic preview laser segments so players can see exact beam paths while placing/rotating!
   */
  public tracePreviewPaths(mods: CircuitModifiers): PreviewSegment[] {
    const segments: PreviewSegment[] = [];
    const emitters = this.getEmitterRows(mods);
    const maxDepth = 9 + mods.bonusHops;

    interface TraceNode {
      col: number;
      row: number;
      dir: Direction;
      power: number;
      elems: Set<ElementType>;
      depth: number;
    }

    const queue: TraceNode[] = [];
    for (const em of emitters) {
      const first = this.getCell(0, em.row);
      const startCol = first && first.mutation === 'locked' ? 1 : 0;
      const cell = this.getCell(startCol, em.row);
      if (cell && cell.mutation !== 'locked') {
        // Segment from emitter (-1) to startCol
        segments.push({
          fromCol: -0.65,
          fromRow: em.row,
          toCol: startCol,
          toRow: em.row,
          powerEstimate: em.powerScale,
          elements: [],
        });
        queue.push({
          col: startCol,
          row: em.row,
          dir: 0,
          power: em.powerScale,
          elems: new Set(),
          depth: maxDepth,
        });
      }
    }

    const visitedCap = new Map<string, number>();
    let safety = 0;

    while (queue.length > 0 && safety < 80) {
      safety++;
      const cur = queue.shift()!;
      const cell = this.getCell(cur.col, cur.row);
      if (!cell || cell.mutation === 'locked') continue;

      const key = `${cur.col},${cur.row},${cur.dir}`;
      const seen = visitedCap.get(key) ?? 0;
      if (seen >= 2) continue;
      visitedCap.set(key, seen + 1);

      let power = cur.power;
      const elems = new Set(cur.elems);
      if (cell.mutation === 'radiant') power *= 1.35;
      if (cell.mutation === 'prismatic') {
        elems.add('pyro');
        elems.add('cryo');
        elems.add('volt');
      }

      let outDirs: Direction[] = [cur.dir];
      const mod = cell.module;
      if (mod) {
        const def = MODULES[mod.defId];
        const tIdx = mod.tier - 1;
        if (def.element) elems.add(def.element);
        if (def.category === 'turret') {
          power *= def.passThroughRatio[tIdx];
        } else {
          power *= def.multFactor[tIdx];
          if (mod.defId === 'splitter') {
            outDirs = [((mod.rotation + 1) % 4) as Direction, ((mod.rotation + 3) % 4) as Direction];
          } else if (mod.defId === 'tri_prism') {
            outDirs = [
              mod.rotation,
              ((mod.rotation + 1) % 4) as Direction,
              ((mod.rotation + 3) % 4) as Direction,
            ];
          } else if (mod.defId === 'mirror') {
            outDirs = [((mod.rotation + 1) % 4) as Direction];
          }
        }
      }

      if (cur.depth <= 1) continue;

      for (const d of outDirs) {
        const delta = DIR_DELTA[d];
        const nc = cur.col + delta.dc;
        const nr = cur.row + delta.dr;
        const nextCell = this.getCell(nc, nr);
        if (nextCell && nextCell.mutation !== 'locked') {
          segments.push({
            fromCol: cur.col,
            fromRow: cur.row,
            toCol: nc,
            toRow: nr,
            powerEstimate: power,
            elements: Array.from(elems),
          });
          queue.push({
            col: nc,
            row: nr,
            dir: d,
            power,
            elems: new Set(elems),
            depth: cur.depth - 1,
          });
        } else {
          // Draw stub exit beam to edge of grid
          segments.push({
            fromCol: cur.col,
            fromRow: cur.row,
            toCol: cur.col + delta.dc * 0.45,
            toRow: cur.row + delta.dr * 0.45,
            powerEstimate: power,
            elements: Array.from(elems),
          });
        }
      }
    }

    return segments;
  }
}
