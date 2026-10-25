// ── Battle engine: telegraphs, shoves, hazards, heat ─────────────────────────
import {
  Board,
  DIRS,
  DIRS8,
  eq,
  manhattan,
  vec,
  type AbilityInst,
  type Battle,
  type BattleEvent,
  type EnemyState,
  type EnemyWave,
  type ForgeState,
  type HeroState,
  type Intent,
  type Vec,
} from './types';
import { abilityDef, abilityStats, enemyDef, heroDef, type EnemyDef } from './content';
import { Rng } from './rng';

export interface BattleSetup {
  board: Board;
  hero: HeroState;
  forge: ForgeState;
  relics: string[];
  seed: number;
  waves: EnemyWave[];
  bossId: string | null;
  eliteBonus: boolean;
  heroPassive: string;
  roomKind: 'battle' | 'elite' | 'boss';
}

export const livingEnemies = (b: Battle): EnemyState[] => b.enemies.filter((e) => e.hp > 0);

export function enemyAt(b: Battle, p: Vec): EnemyState | null {
  for (const e of b.enemies) if (e.hp > 0 && eq(e.pos, p)) return e;
  return null;
}

export function forgeAt(b: Battle, p: Vec): boolean {
  return b.forge.tiles.some((t) => eq(t, p));
}

export function addHeat(b: Battle, amount: number): void {
  b.hero.heat = Math.max(0, Math.min(b.hero.maxHeat, b.hero.heat + amount));
}

export function createBattle(setup: BattleSetup): Battle {
  const b: Battle = {
    board: setup.board,
    hero: setup.hero,
    forge: setup.forge,
    enemies: [],
    turn: 1,
    phase: 'player',
    waves: setup.waves,
    waveIndex: 0,
    seed: setup.seed,
    relics: setup.relics,
    echoUsed: false,
    bombs: [],
    kills: 0,
    killsThisTurn: 0,
    castsThisTurn: 0,
    shoveKills: 0,
    eliteBonus: setup.eliteBonus,
    bossId: setup.bossId,
    heroPassive: setup.heroPassive,
    roomKind: setup.roomKind,
    nextUid: 1,
  };
  if (b.relics.includes('start_shield')) b.hero.shield += 5;

  const rng = new Rng(setup.seed ^ 0x5f3759df);
  while (b.waveIndex < b.waves.length && b.waves[b.waveIndex].arrival <= 1) {
    spawnWave(b, b.waves[b.waveIndex], rng, []);
    b.waveIndex++;
  }
  computeIntents(b);
  return b;
}

// ── Targeting helpers ────────────────────────────────────────────────────────

function occupiedByEnemy(b: Battle, p: Vec, ignore?: EnemyState): boolean {
  return b.enemies.some((e) => e.hp > 0 && e !== ignore && eq(e.pos, p));
}

export function heroBlocked(b: Battle, p: Vec): boolean {
  return b.board.isWall(p) || forgeAt(b, p) || occupiedByEnemy(b, p);
}

export function heroReach(b: Battle): Map<string, number> {
  const dist = new Map<string, number>();
  if (!b.hero.movesLeft) return dist;
  const d = b.board.distanceField(b.hero.pos, (p) => forgeAt(b, p) || occupiedByEnemy(b, p));
  for (let y = 0; y < b.board.h; y++) {
    for (let x = 0; x < b.board.w; x++) {
      const p = vec(x, y);
      const dd = d[y * b.board.w + x];
      if (dd > 0 && dd <= b.hero.moveRange && !heroBlocked(b, p)) dist.set(`${x},${y}`, dd);
    }
  }
  return dist;
}

function inMeleeRange(a: Vec, c: Vec): boolean {
  return manhattan(a, c) === 1;
}

function effectivePush(b: Battle, base: number): number {
  let n = base;
  if (b.relics.includes('momentum')) n += 1;
  if (b.heroPassive === 'anchored') n += 1;
  return n;
}

/** Valid aim tiles for an ability, used for highlighting and for click checks. */
export function abilityTargets(b: Battle, inst: AbilityInst): Vec[] {
  const def = abilityDef(inst.id);
  const st = abilityStats(inst.id, inst.level);
  const range = inst.id === 'ash_step' && b.heroPassive === 'flicker' ? st.range + 2 : st.range;
  const hero = b.hero.pos;
  const out: Vec[] = [];
  const push = (p: Vec) => {
    if (!out.some((q) => eq(q, p))) out.push(p);
  };

  switch (def.targeting) {
    case 'self':
      push({ ...hero });
      return out;
    case 'adjacent':
      for (const d of DIRS) {
        const p = vec(hero.x + d.x, hero.y + d.y);
        if (b.board.inside(p) && enemyAt(b, p)) push(p);
      }
      return out;
    case 'enemy':
      for (const e of livingEnemies(b)) {
        if (manhattan(hero, e.pos) <= range && b.board.hasLineOfSight(hero, e.pos)) push({ ...e.pos });
      }
      return out;
    case 'line':
      for (const d of DIRS) {
        for (let i = 1; i <= range; i++) {
          const p = vec(hero.x + d.x * i, hero.y + d.y * i);
          if (!b.board.inside(p)) break;
          push(p);
          if (b.board.isWall(p)) break;
        }
      }
      return out;
    case 'tile':
    default: {
      for (let y = 0; y < b.board.h; y++) {
        for (let x = 0; x < b.board.w; x++) {
          const p = vec(x, y);
          const d = manhattan(hero, p);
          if (d === 0 || d > range) continue;
          const placeable = !b.board.isWall(p) && !forgeAt(b, p) && !occupiedByEnemy(b, p) && !eq(b.hero.pos, p);
          const reachable = inst.id === 'ash_step' || inst.id === 'pyre_dash' ? placeable : b.board.hasLineOfSight(hero, p) && placeable;
          if (reachable) push(p);
        }
      }
      return out;
    }
  }
}

export function abilityCost(b: Battle, inst: AbilityInst): number {
  if (b.heroPassive === 'flicker' && inst.id === 'ash_step') return 0;
  if (b.relics.includes('echo_anvil') && !b.echoUsed) return 0;
  return abilityStats(inst.id, inst.level).cost;
}

// ── Damage plumbing ──────────────────────────────────────────────────────────

function healHero(b: Battle, amount: number, ev: BattleEvent[]): void {
  const before = b.hero.hp;
  b.hero.hp = Math.min(b.hero.maxHp, b.hero.hp + amount);
  if (b.hero.hp > before) ev.push({ t: 'heal', at: { ...b.hero.pos }, amount: b.hero.hp - before });
}

function damageHero(b: Battle, raw: number, ev: BattleEvent[], fromAttack: boolean): void {
  let amount = raw;
  if (fromAttack) {
    if (b.relics.includes('slag_skin')) amount -= 1;
    if (b.heroPassive === 'anchored') amount -= 1;
    amount = Math.max(1, amount);
  }
  if (b.hero.shield > 0) {
    const absorbed = Math.min(b.hero.shield, amount);
    b.hero.shield -= absorbed;
    amount -= absorbed;
    ev.push({ t: 'hit', at: { ...b.hero.pos }, hero: true, amount: absorbed, blocked: true });
  }
  if (amount > 0) {
    b.hero.hp -= amount;
    ev.push({ t: 'hit', at: { ...b.hero.pos }, hero: true, amount });
    ev.push({ t: 'shake', power: Math.min(14, 4 + amount * 2) });
  }
}

function damageForge(b: Battle, raw: number, ev: BattleEvent[]): void {
  const amount = Math.max(1, raw);
  b.forge.hp -= amount;
  ev.push({ t: 'hit', at: { ...b.forge.tiles[0] }, forge: true, amount });
  ev.push({ t: 'shake', power: Math.min(16, 5 + amount * 2) });
}

function addShieldToEnemy(e: EnemyState, amount: number, ev: BattleEvent[]): void {
  e.shield = Math.min(6, e.shield + amount);
  ev.push({ t: 'shield', at: { ...e.pos }, amount });
}

/** Applies damage to an enemy, handling shields, marks, death and bonuses. */
function hurtEnemy(
  b: Battle,
  e: EnemyState,
  raw: number,
  ev: BattleEvent[],
  opts: { ability?: boolean; cause?: 'ability' | 'push' | 'vent' | 'bomb' | 'burn' } = {},
): void {
  if (e.hp <= 0) return;
  let dmg = raw;
  if (opts.ability) {
    if (b.relics.includes('whetstone')) dmg += 1;
    if (b.relics.includes('executioner') && e.hp <= e.maxHp * 0.4) dmg += 3;
    if (e.mark > 0) {
      dmg += e.mark;
      e.mark = 0;
      ev.push({ t: 'text', at: { ...e.pos }, text: 'BRAND', tone: 'ember' });
    }
  }
  if (e.shield > 0) {
    const absorbed = Math.min(e.shield, dmg);
    e.shield -= absorbed;
    dmg -= absorbed;
    ev.push({ t: 'hit', at: { ...e.pos }, uid: e.uid, amount: absorbed, blocked: true });
  }
  if (dmg > 0) {
    e.hp -= dmg;
    ev.push({ t: 'hit', at: { ...e.pos }, uid: e.uid, amount: dmg });
  }
  if (e.hp <= 0) killEnemy(b, e, ev, opts.cause ?? 'ability');
}

function killEnemy(b: Battle, e: EnemyState, ev: BattleEvent[], cause: string): void {
  if (e.hp > 0) e.hp = 0;
  b.kills++;
  b.killsThisTurn++;
  if (cause === 'push' || cause === 'vent') b.shoveKills++;
  const def = enemyDef(e.typeId);
  ev.push({ t: 'death', at: { ...e.pos }, uid: e.uid, boss: e.boss });
  if (e.boss) ev.push({ t: 'shake', power: 22 });

  let gain = b.heroPassive === 'kindling' ? 2 : 1;
  if (b.relics.includes('kindler')) gain += 1;
  addHeat(b, gain);
  if (b.relics.includes('ember_blood')) healHero(b, 1, ev);

  if (def.ai === 'exploder') {
    ev.push({ t: 'explode', at: { ...e.pos } });
    const zone = [e.pos, ...DIRS8.map((d) => vec(e.pos.x + d.x, e.pos.y + d.y))];
    if (zone.some((p) => eq(p, b.hero.pos))) damageHero(b, e.dmg, ev, false);
    const from = { ...e.pos };
    for (const p of zone) for (const other of livingEnemies(b)) if (other !== e && eq(other.pos, p)) hurtEnemy(b, other, 1, ev, { cause: 'bomb' });
    for (const f of b.forge.tiles) if (zone.some((p) => eq(p, f))) damageForge(b, 1, ev);
    void from;
  }
}

// ── Movement & hazards ───────────────────────────────────────────────────────

function applyTileHazard(b: Battle, who: 'hero' | EnemyState, ev: BattleEvent[]): void {
  if (who === 'hero') {
    const tile = b.board.at(b.hero.pos);
    if (tile?.kind === 'vent' && !b.relics.includes('vent_grace')) {
      ev.push({ t: 'vent', at: { ...b.hero.pos } });
      damageHero(b, 2, ev, false);
    }
    if (b.relics.includes('spark_trail')) {
      for (const e of livingEnemies(b)) {
        if (inMeleeRange(e.pos, b.hero.pos)) hurtEnemy(b, e, 1, ev, {});
      }
    }
    return;
  }
  const tile = b.board.at(who.pos);
  if (tile?.kind === 'vent') {
    ev.push({ t: 'vent', at: { ...who.pos } });
    hurtEnemy(b, who, 2, ev, { cause: 'vent' });
  }
}

export function moveHero(b: Battle, target: Vec): BattleEvent[] {
  const ev: BattleEvent[] = [];
  if (b.phase !== 'player' || !b.hero.movesLeft) return ev;
  if (heroBlocked(b, target)) return ev;
  const path = b.board.pathTo(b.hero.pos, (p) => eq(p, target), (p) => forgeAt(b, p) || occupiedByEnemy(b, p));
  if (!path || path.length === 0 || path.length > b.hero.moveRange) return ev;
  const from = { ...b.hero.pos };
  for (const step of path) {
    b.hero.pos = { ...step };
    applyTileHazard(b, 'hero', ev);
    if (b.hero.hp <= 0) break;
  }
  b.hero.movesLeft = false;
  ev.push({ t: 'move', who: 'hero', from, to: { ...b.hero.pos }, path });
  checkEnd(b, ev);
  return ev;
}

function pushEnemy(b: Battle, e: EnemyState, dir: Vec, steps: number, ev: BattleEvent[], opts: { ability?: boolean; armoredBlock?: boolean } = {}): void {
  if (e.hp <= 0) return;
  let count = steps;
  if (e.armored && opts.armoredBlock !== false) count = Math.max(0, count - 1);
  const from = { ...e.pos };
  let moved = 0;
  for (let s = 0; s < count; s++) {
    const next = vec(e.pos.x + dir.x, e.pos.y + dir.y);
    if (!b.board.inside(next) || b.board.isWall(next) || forgeAt(b, next)) {
      const impact = (b.relics.includes('ricochet') ? 2 : 0) + 1;
      ev.push({ t: 'slam', at: { ...e.pos }, kind: 'wall' });
      hurtEnemy(b, e, impact, ev, { cause: 'push' });
      break;
    }
    const other = enemyAt(b, next);
    if (other && other !== e) {
      hurtEnemy(b, other, 1, ev, { cause: 'push' });
      hurtEnemy(b, e, 1, ev, { cause: 'push' });
      ev.push({ t: 'slam', at: { ...next }, kind: 'enemy' });
      break;
    }
    e.pos = { ...next };
    moved++;
    if (b.board.at(e.pos)?.kind === 'vent') {
      ev.push({ t: 'vent', at: { ...e.pos } });
      hurtEnemy(b, e, 2, ev, { cause: 'vent' });
      if (e.hp <= 0) break;
    }
  }
  if (moved > 0) ev.push({ t: 'push', uid: e.uid, from, to: { ...e.pos }, steps: moved, impact: false });
}

function pullEnemy(b: Battle, e: EnemyState, steps: number, ev: BattleEvent[], opts: { ability?: boolean; stopAdjacent?: boolean } = {}): void {
  if (e.hp <= 0) return;
  const from = { ...e.pos };
  let moved = 0;
  for (let s = 0; s < steps; s++) {
    if (opts.stopAdjacent && inMeleeRange(e.pos, b.hero.pos)) break;
    const dx = Math.sign(b.hero.pos.x - e.pos.x);
    const dy = Math.sign(b.hero.pos.y - e.pos.y);
    const options = Math.abs(b.hero.pos.x - e.pos.x) >= Math.abs(b.hero.pos.y - e.pos.y)
      ? [vec(dx, 0), vec(0, dy)]
      : [vec(0, dy), vec(dx, 0)];
    let stepped = false;
    for (const d of options) {
      if (d.x === 0 && d.y === 0) continue;
      const next = vec(e.pos.x + d.x, e.pos.y + d.y);
      if (b.board.isWall(next) || forgeAt(b, next) || occupiedByEnemy(b, next, e) || eq(next, b.hero.pos)) continue;
      e.pos = next;
      moved++;
      stepped = true;
      if (b.board.at(e.pos)?.kind === 'vent') {
        ev.push({ t: 'vent', at: { ...e.pos } });
        hurtEnemy(b, e, 2, ev, { cause: 'vent' });
      }
      break;
    }
    if (!stepped) break;
  }
  if (moved > 0) ev.push({ t: 'push', uid: e.uid, from, to: { ...e.pos }, steps: moved, impact: false });
}

// ── Ability casting ──────────────────────────────────────────────────────────

export function castAbility(b: Battle, index: number, target: Vec): BattleEvent[] {
  const ev: BattleEvent[] = [];
  const inst = b.hero.abilities[index];
  if (!inst || b.phase !== 'player') return ev;
  const cost = abilityCost(b, inst);
  if (cost > b.hero.heat) return ev;
  if (!abilityTargets(b, inst).some((p) => eq(p, target))) return ev;

  b.hero.heat -= cost;
  if (cost === 0 && b.relics.includes('echo_anvil')) b.echoUsed = true;
  b.castsThisTurn++;
  const st = abilityStats(inst.id, inst.level);
  const hero = b.hero.pos;
  const def = abilityDef(inst.id);
  ev.push({ t: 'slam', at: { ...hero }, kind: `cast:${def.icon}` });

  switch (inst.id) {
    case 'scorch': {
      const e = enemyAt(b, target);
      if (e) {
        const dir = vec(Math.sign(target.x - hero.x), Math.sign(target.y - hero.y));
        hurtEnemy(b, e, st.dmg, ev, { ability: true });
        if (e.hp > 0) pushEnemy(b, e, dir, effectivePush(b, 1), ev, { ability: true });
      }
      break;
    }
    case 'ash_step':
    case 'pyre_dash': {
      const from = { ...hero };
      b.hero.pos = { ...target };
      applyTileHazard(b, 'hero', ev);
      ev.push({ t: 'move', who: 'hero', from, to: { ...target }, path: [target] });
      if (inst.id === 'pyre_dash') {
        for (const e of livingEnemies(b)) {
          if (e.pos.x >= target.x - 1 && e.pos.x <= target.x + 1 && e.pos.y >= target.y - 1 && e.pos.y <= target.y + 1) {
            hurtEnemy(b, e, st.dmg, ev, { ability: true });
            if (e.hp > 0) pushEnemy(b, e, vec(Math.sign(e.pos.x - target.x), Math.sign(e.pos.y - target.y)), 1, ev, { ability: true });
          }
        }
      }
      break;
    }
    case 'ember_lance': {
      const dir = vec(Math.sign(target.x - hero.x), Math.sign(target.y - hero.y));
      const line = b.board.line(hero, dir, st.range);
      for (const p of line) {
        const e = enemyAt(b, p);
        if (e) {
          hurtEnemy(b, e, st.dmg, ev, { ability: true });
          if (e.hp > 0) pushEnemy(b, e, dir, effectivePush(b, 1), ev, { ability: true });
          break;
        }
      }
      break;
    }
    case 'quake':
    case 'bulwark': {
      const r = inst.id === 'bulwark' ? 1 : 1;
      for (const e of livingEnemies(b)) {
        if (manhattan(e.pos, hero) > r) continue;
        hurtEnemy(b, e, st.dmg > 0 ? st.dmg : 1, ev, { ability: true });
        if (e.hp > 0) pushEnemy(b, e, vec(e.pos.x - hero.x, e.pos.y - hero.y), effectivePush(b, inst.id === 'bulwark' ? 2 : 1), ev, { ability: true });
      }
      if (inst.id === 'bulwark') {
        b.hero.shield = Math.min(20, b.hero.shield + 3 + inst.level * 2);
        ev.push({ t: 'shield', at: { ...hero }, amount: 3 + inst.level * 2 });
      }
      break;
    }
    case 'forge_bolt': {
      const e = enemyAt(b, target);
      if (e) hurtEnemy(b, e, st.dmg, ev, { ability: true });
      break;
    }
    case 'cinder_wall': {
      b.board.set(target, 'wall');
      ev.push({ t: 'wall', at: { ...target } });
      break;
    }
    case 'bellows': {
      const e = enemyAt(b, target);
      if (e) {
        pullEnemy(b, e, 2, ev, { ability: true });
        hurtEnemy(b, e, st.dmg, ev, { ability: true });
      }
      break;
    }
    case 'temper': {
      const amount = 4 + inst.level * 2;
      b.hero.shield = Math.min(20, b.hero.shield + amount);
      ev.push({ t: 'shield', at: { ...hero }, amount });
      break;
    }
    case 'reforge': {
      healHero(b, 4 + inst.level * 2, ev);
      break;
    }
    case 'slag_bomb': {
      b.bombs.push({ pos: { ...target }, dmg: st.dmg, fuse: 1, stun: b.relics.includes('bomb_kit'), from: 'hero' });
      ev.push({ t: 'bomb', at: { ...target } });
      break;
    }
    case 'molten_brand': {
      const e = enemyAt(b, target);
      if (e) {
        e.mark += 3 + inst.level * 2;
        ev.push({ t: 'mark', at: { ...e.pos }, uid: e.uid });
      }
      break;
    }
    case 'flare': {
      for (const e of livingEnemies(b)) {
        if (manhattan(e.pos, hero) > 2) continue;
        hurtEnemy(b, e, st.dmg, ev, { ability: true });
        if (e.hp > 0) e.burn = Math.max(e.burn, 2);
        ev.push({ t: 'burn', at: { ...e.pos }, uid: e.uid });
      }
      ev.push({ t: 'explode', at: { ...hero } });
      break;
    }
    case 'chain_hook': {
      const e = enemyAt(b, target);
      if (e) {
        pullEnemy(b, e, 4, ev, { ability: true, stopAdjacent: true });
        hurtEnemy(b, e, st.dmg, ev, { ability: true });
      }
      break;
    }
    case 'vent_call': {
      b.board.set(target, 'vent');
      ev.push({ t: 'vent', at: { ...target } });
      break;
    }
    case 'iron_tithe': {
      const e = enemyAt(b, target);
      if (e) {
        hurtEnemy(b, e, st.dmg, ev, { ability: true });
        if (e.hp > 0) pushEnemy(b, e, vec(Math.sign(target.x - hero.x), Math.sign(target.y - hero.y)), effectivePush(b, 1), ev, { ability: true });
      }
      b.forge.hp = Math.min(b.forge.maxHp, b.forge.hp + 2);
      ev.push({ t: 'text', at: { ...b.forge.tiles[0] }, text: '+2 FORGE', tone: 'good' });
      break;
    }
  }

  b.enemies = b.enemies.filter((e) => e.hp > 0);
  checkEnd(b, ev);
  return ev;
}

// ── Enemy planning ───────────────────────────────────────────────────────────

interface Objective {
  pos: Vec;
  kind: 'hero' | 'forge';
}

/**
 * Each archetype has one fixed quarry, so its plan never oscillates between
 * the hero and the forge. Beasts and eaters walk past you to chew the forge;
 * everything else comes for you.
 */
function objectiveFor(b: Battle, def: EnemyDef): Objective {
  const forgeTile = b.forge.tiles.reduce(
    (best, t) => (manhattan(b.hero.pos, t) < manhattan(b.hero.pos, best) ? t : best),
    b.forge.tiles[0],
  );
  const siege = def.ai === 'melee' && (def.id === 'brute' || def.id === 'eater');
  if (siege || def.boss) return { pos: forgeTile, kind: 'forge' };
  return { pos: { ...b.hero.pos }, kind: 'hero' };
}

function tileNextToForge(b: Battle, p: Vec): boolean {
  return b.forge.tiles.some((f) => manhattan(f, p) === 1);
}

function inAttackRange(b: Battle, _e: EnemyState, obj: Objective, from: Vec, range: number): boolean {
  if (obj.kind === 'forge') {
    if (tileNextToForge(b, from)) return true;
    return range > 1 && b.forge.tiles.some((f) => manhattan(f, from) <= range && b.board.hasLineOfSight(from, f));
  }
  if (range === 1) return manhattan(from, obj.pos) === 1;
  return manhattan(from, obj.pos) <= range && b.board.hasLineOfSight(from, obj.pos);
}

/** A single greedy step toward a point, used when no full path exists. */
function greedyStep(b: Battle, from: Vec, toward: Vec, blocked: (p: Vec) => boolean): Vec | null {
  let best: Vec | null = null;
  let bestD = manhattan(from, toward);
  for (const d of DIRS) {
    const n = vec(from.x + d.x, from.y + d.y);
    if (!b.board.walkable(n) || blocked(n)) continue;
    const dd = manhattan(n, toward);
    if (dd < bestD) {
      bestD = dd;
      best = n;
    }
  }
  return best;
}

export function computeIntents(b: Battle): void {
  if (b.phase === 'won' || b.phase === 'lost') return;
  for (const e of b.enemies) {
    if (e.hp <= 0) continue;
    e.intent = planIntent(b, e);
  }
}

function planIntent(b: Battle, e: EnemyState): Intent {
  const def = enemyDef(e.typeId);
  if (def.boss && def.special && b.turn % 3 === 0) {
    const label = def.special === 'quake' ? 'QUAKE' : def.special === 'summon' ? 'SUMMON' : 'UNMAKE';
    return { kind: 'special', path: [], label, style: 'splash', target: { ...e.pos } };
  }
  const obj = objectiveFor(b, def);
  const blocked = (p: Vec) => forgeAt(b, p) || occupiedByEnemy(b, p, e);
  const goal = (p: Vec) =>
    inAttackRange(b, e, obj, p, e.range) && (e.range === 1 || obj.kind === 'forge' || b.board.hasLineOfSight(p, obj.pos));
  let path = b.board.pathTo(e.pos, goal, blocked) ?? [];
  if (!path.length && !goal(e.pos)) {
    const step = greedyStep(b, e.pos, obj.pos, blocked);
    if (step) path = [step];
  }
  const steps = path.slice(0, Math.max(0, e.speed));
  const dest = steps.length ? steps[steps.length - 1] : e.pos;
  const canHit = inAttackRange(b, e, obj, dest, e.range) && (e.range === 1 || obj.kind === 'forge' || b.board.hasLineOfSight(dest, obj.pos));
  if (canHit) {
    return {
      kind: 'attack',
      target: { ...obj.pos },
      path: steps,
      label: String(e.dmg),
      style: e.range > 1 ? 'ranged' : 'melee',
    };
  }
  return { kind: 'move', target: { ...dest }, path: steps, label: '»', style: 'melee' };
}

// ── Enemy phase ──────────────────────────────────────────────────────────────

function enemyMove(b: Battle, e: EnemyState, path: Vec[], ev: BattleEvent[]): void {
  if (!path.length) return;
  const from = { ...e.pos };
  let moved = false;
  for (const step of path) {
    if (!b.board.walkable(step) || forgeAt(b, step) || occupiedByEnemy(b, step, e)) break;
    e.pos = { ...step };
    moved = true;
    if (b.board.at(e.pos)?.kind === 'vent') {
      ev.push({ t: 'vent', at: { ...e.pos } });
      hurtEnemy(b, e, 2, ev, { cause: 'vent' });
      if (e.hp <= 0) break;
    }
  }
  if (moved) ev.push({ t: 'enemyMove', uid: e.uid, from, to: { ...e.pos }, path });
}

function resolveEnemyAttack(b: Battle, e: EnemyState, ev: BattleEvent[]): void {
  const intent = e.intent;
  if (!intent || intent.kind !== 'attack' || !intent.target) return;
  const target = intent.target;
  const isForge = forgeAt(b, target);
  const heroThere = eq(b.hero.pos, target);
  if (isForge) {
    if (!(tileNextToForge(b, e.pos) || (e.range > 1 && b.forge.tiles.some((f) => manhattan(f, e.pos) <= e.range && b.board.hasLineOfSight(e.pos, f))))) {
      ev.push({ t: 'text', at: { ...e.pos }, text: 'MISS', tone: 'good' });
      return;
    }
    ev.push({ t: 'slam', at: { ...e.pos }, kind: 'forgeAtk' });
    damageForge(b, e.dmg, ev);
    // Hitting an anvil that size costs the attacker its next turn.
    e.stun = Math.max(e.stun, 1);
    ev.push({ t: 'text', at: { ...e.pos }, text: 'RECOVER', tone: 'good' });
    return;
  }
  if (!heroThere) {
    ev.push({ t: 'text', at: { ...e.pos }, text: 'MISS', tone: 'good' });
    return;
  }
  const inRange = e.range === 1 ? manhattan(e.pos, b.hero.pos) === 1 : manhattan(e.pos, b.hero.pos) <= e.range && b.board.hasLineOfSight(e.pos, b.hero.pos);
  if (!inRange) {
    ev.push({ t: 'text', at: { ...e.pos }, text: 'MISS', tone: 'good' });
    return;
  }
  if (e.range > 1) ev.push({ t: 'slam', at: { ...b.hero.pos }, kind: 'beam' });
  else ev.push({ t: 'slam', at: { ...b.hero.pos }, kind: 'slash' });
  damageHero(b, e.dmg, ev, true);
  if (enemyDef(e.typeId).id === 'sentinel') {
    // Sentinels retaliate: handled on incoming hits instead.
  }
}

function bossSpecial(b: Battle, e: EnemyState, ev: BattleEvent[]): void {
  const def = enemyDef(e.typeId);
  if (def.special === 'quake') {
    ev.push({ t: 'explode', at: { ...e.pos } });
    ev.push({ t: 'shake', power: 20 });
    const zone = [e.pos, ...DIRS8.map((d) => vec(e.pos.x + d.x, e.pos.y + d.y)), ...DIRS8.map((d) => vec(e.pos.x + d.x * 2, e.pos.y + d.y * 2))];
    if (zone.some((p) => eq(p, b.hero.pos))) damageHero(b, 2, ev, false);
    for (const f of b.forge.tiles) if (zone.some((p) => eq(p, f))) damageForge(b, 2, ev);
  } else if (def.special === 'summon') {
    ev.push({ t: 'explode', at: { ...e.pos } });
    spawnAdds(b, e, 2, ev);
  } else {
    // The Unmaker: a beam toward the forge plus a fresh grunt.
    ev.push({ t: 'explode', at: { ...e.pos } });
    const target = b.forge.tiles[0];
    const dir = vec(Math.sign(target.x - e.pos.x), Math.sign(target.y - e.pos.y));
    const line = b.board.line(e.pos, dir, 5);
    line.push(e.pos);
    if (line.some((p) => eq(p, b.hero.pos))) damageHero(b, 3, ev, false);
    for (const f of b.forge.tiles) if (line.some((p) => eq(p, f))) damageForge(b, 3, ev);
    ev.push({ t: 'slam', at: { ...(line[Math.min(2, line.length - 1)] ?? e.pos) }, kind: 'beam' });
    spawnAdds(b, e, 1, ev);
  }
}

function spawnAdds(b: Battle, near: EnemyState, count: number, ev: BattleEvent[]): void {
  const rng = new Rng(b.seed ^ (b.turn * 2654435761));
  const spots: Vec[] = [];
  for (let y = 0; y < b.board.h; y++) {
    for (let x = 0; x < b.board.w; x++) {
      const p = vec(x, y);
      if (!b.board.walkable(p) || forgeAt(b, p) || occupiedByEnemy(b, p) || eq(p, b.hero.pos)) continue;
      if (manhattan(p, near.pos) > 3 || manhattan(p, near.pos) === 0) continue;
      spots.push(p);
    }
  }
  rng.shuffle(spots);
  for (let i = 0; i < count && i < spots.length; i++) {
    addEnemy(b, 'grunt', spots[i], false, [], ev);
  }
}

function addEnemy(b: Battle, typeId: string, pos: Vec, elite: boolean, _ignored: string[], ev: BattleEvent[]): EnemyState {
  const def = enemyDef(typeId);
  const e: EnemyState = {
    uid: b.nextUid++,
    typeId,
    name: def.name,
    pos: { ...pos },
    hp: def.hp,
    maxHp: def.hp,
    shield: 0,
    dmg: def.dmg,
    range: def.range,
    speed: def.speed,
    stun: 0,
    mark: 0,
    burn: 0,
    elite: !!def.elite || elite,
    boss: !!def.boss,
    armored: !!def.armored,
    intent: null,
    bornAt: 0,
  };
  b.enemies.push(e);
  ev.push({ t: 'spawn', at: { ...pos }, uid: e.uid });
  return e;
}

function spawnWave(b: Battle, wave: EnemyWave, rng: Rng, ev: BattleEvent[]): void {
  for (const group of wave.groups) {
    for (let i = 0; i < group.count; i++) {
      const spot = pickSpawnTile(b, rng);
      if (!spot) continue;
      addEnemy(b, group.typeId, spot, false, [], ev);
    }
  }
}

function pickSpawnTile(b: Battle, rng: Rng): Vec | null {
  const spots: Vec[] = [];
  for (let y = 0; y < b.board.h; y++) {
    for (let x = 0; x < b.board.w; x++) {
      const p = vec(x, y);
      if (!b.board.walkable(p) || forgeAt(b, p) || occupiedByEnemy(b, p) || eq(p, b.hero.pos)) continue;
      if (manhattan(p, b.hero.pos) < 2) continue;
      if (y > 5) continue;
      spots.push(p);
    }
  }
  if (!spots.length) return null;
  return rng.pick(spots);
}

function explodeBombs(b: Battle, ev: BattleEvent[]): void {
  const remaining: typeof b.bombs = [];
  for (const bomb of b.bombs) {
    bomb.fuse -= 1;
    if (bomb.fuse > 0) {
      remaining.push(bomb);
      continue;
    }
    ev.push({ t: 'explode', at: { ...bomb.pos } });
    ev.push({ t: 'shake', power: 12 });
    const dmg = bomb.dmg + (b.relics.includes('bomb_kit') ? 2 : 0);
    for (const e of livingEnemies(b)) {
      if (Math.abs(e.pos.x - bomb.pos.x) <= 1 && Math.abs(e.pos.y - bomb.pos.y) <= 1) {
        hurtEnemy(b, e, dmg, ev, { ability: true });
        if (e.hp > 0) {
          if (bomb.stun) e.stun = Math.max(e.stun, 1);
          pushEnemy(b, e, vec(Math.sign(e.pos.x - bomb.pos.x), Math.sign(e.pos.y - bomb.pos.y)), 1, ev, { ability: true });
        }
      }
    }
    for (const f of b.forge.tiles) {
      if (Math.abs(f.x - bomb.pos.x) <= 1 && Math.abs(f.y - bomb.pos.y) <= 1) {
        ev.push({ t: 'text', at: { ...f }, text: 'SAFE', tone: 'good' });
      }
    }
  }
  b.bombs = remaining;
}

/** Ends the player turn and resolves everything the enemies promised. */
export function endTurn(b: Battle): BattleEvent[] {
  const ev: BattleEvent[] = [];
  if (b.phase !== 'player') return ev;
  b.phase = 'resolve';

  explodeBombs(b, ev);

  const order = livingEnemies(b);
  for (const e of order) {
    if (e.hp <= 0) continue;
    if (e.stun > 0) {
      e.stun--;
      ev.push({ t: 'text', at: { ...e.pos }, text: 'STUNNED', tone: 'good' });
      continue;
    }
    const intent = e.intent;
    if (!intent) continue;
    if (intent.kind === 'special') {
      bossSpecial(b, e, ev);
    } else {
      enemyMove(b, e, intent.path, ev);
      if (e.hp > 0 && intent.kind === 'attack') resolveEnemyAttack(b, e, ev);
    }
    if (e.burn > 0) {
      hurtEnemy(b, e, 1, ev, { cause: 'burn' });
      e.burn--;
    }
    if (b.hero.hp <= 0 || b.forge.hp <= 0) break;
  }

  // Support auras resolve after movement so they feel reactive. A chorister
  // wraps exactly one neighbour in aegis, so shields can never outpace damage.
  for (const e of livingEnemies(b)) {
    if (enemyDef(e.typeId).ai !== 'support') continue;
    const allies = livingEnemies(b).filter((o) => o !== e && manhattan(o.pos, e.pos) === 1);
    if (!allies.length) continue;
    const weakest = allies.reduce((a, c) => (c.hp - c.shield < a.hp - a.shield ? c : a));
    addShieldToEnemy(weakest, 2, ev);
  }

  b.enemies = b.enemies.filter((e) => e.hp > 0);
  checkEnd(b, ev);
  const resolvedPhase: string = b.phase;
  if (resolvedPhase === 'won' || resolvedPhase === 'lost') return ev;

  // Begin the next player turn.
  b.turn++;
  b.killsThisTurn = 0;
  b.castsThisTurn = 0;
  b.phase = 'player';
  b.hero.movesLeft = true;

  const rng = new Rng(b.seed ^ (b.turn * 747796405));
  while (b.waveIndex < b.waves.length && b.waves[b.waveIndex].arrival <= b.turn) {
    spawnWave(b, b.waves[b.waveIndex], rng, ev);
    ev.push({ t: 'wave', n: b.waveIndex + 1 });
    b.waveIndex++;
  }

  // The keep itself begins to freeze if a fight drags on, so kiting forever
  // is never a stable plan.
  if (b.turn > 14) {
    const chill = 1 + Math.floor((b.turn - 15) / 6);
    damageForge(b, chill, ev);
    ev.push({ t: 'text', at: { ...b.forge.tiles[0] }, text: 'THE COLD BITES', tone: 'bad' });
    checkEnd(b, ev);
    const chilled: string = b.phase;
    if (chilled === 'lost') return ev;
  }

  addHeat(b, b.hero.heatRegen + (b.relics.includes('cinder_core') ? 1 : 0));
  if (b.relics.includes('pyre')) {
    for (const e of livingEnemies(b)) {
      if (manhattan(e.pos, b.hero.pos) === 1) hurtEnemy(b, e, 1, ev, {});
    }
  }
  b.enemies = b.enemies.filter((e) => e.hp > 0);
  computeIntents(b);
  ev.push({ t: 'turn', n: b.turn });
  checkEnd(b, ev);
  return ev;
}

export function checkEnd(b: Battle, ev: BattleEvent[]): void {
  if (b.phase === 'won' || b.phase === 'lost') return;
  if (b.hero.hp <= 0) {
    b.phase = 'lost';
    ev.push({ t: 'lost', reason: 'The Emberkeeper goes dark.' });
    return;
  }
  if (b.forge.hp <= 0) {
    b.phase = 'lost';
    ev.push({ t: 'lost', reason: 'The last forge is cold.' });
    return;
  }
  b.enemies = b.enemies.filter((e) => e.hp > 0);
  if (b.waveIndex >= b.waves.length && livingEnemies(b).length === 0) {
    b.phase = 'won';
    ev.push({ t: 'won' });
  }
}

/** Short human description of what an enemy is going to do, for the inspect panel. */
export function intentText(b: Battle, e: EnemyState): string {
  if (!e.intent) return 'Waiting.';
  const def = enemyDef(e.typeId);
  if (e.intent.kind === 'special') {
    if (def.special === 'quake') return 'Quake: 2 damage in a 2-tile ring, next turn.';
    if (def.special === 'summon') return 'Summon: calls two Cinder Grunts next turn.';
    return 'Unmake: a beam of 3 damage and a fresh grunt.';
  }
  if (e.intent.kind === 'attack') {
    const forge = forgeAt(b, e.intent.target!);
    if (forge) return `Strikes the forge for ${e.dmg}.`;
    return `Strikes the marked tile for ${e.dmg}. Step out of it.`;
  }
  return 'Advancing.';
}

export { heroDef };
