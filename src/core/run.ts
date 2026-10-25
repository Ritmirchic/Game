// ── Run structure: rooms, encounters, rewards, shops, rest ───────────────────
import { Rng } from './rng';
import {
  ACTS,
  ABILITIES,
  BOSSES,
  ENEMIES,
  RELICS,
  abilityDef,
  abilityStats,
  enemyDef,
  heroDef,
  relicDef,
  type Rarity,
  type RoomType,
} from './content';
import { makeArena, type ArenaStyle } from './arena';
import { createBattle } from './battle';
import type { AbilityInst, Battle, ForgeState, HeroState, Vec } from './types';

export const SAVE_VERSION = 3;
export const MAX_ABILITIES = 5;

export interface RoomOffer {
  type: RoomType;
  label: string;
}

export interface RewardOffer {
  key: string;
  kind: 'ability' | 'relic' | 'upgrade' | 'heal' | 'repair' | 'embers';
  title: string;
  desc: string;
  icon: string;
  rarity: Rarity;
  abilityId?: string;
  relicId?: string;
}

export interface ShopItem {
  key: string;
  kind: 'relic' | 'ability' | 'heal' | 'repair' | 'upgrade' | 'embers';
  title: string;
  desc: string;
  icon: string;
  rarity: Rarity;
  price: number;
  abilityId?: string;
  relicId?: string;
}

export interface RunState {
  seed: number;
  rngState: number;
  heroId: string;
  hp: number;
  maxHp: number;
  forgeHp: number;
  maxForgeHp: number;
  heatMax: number;
  heatRegen: number;
  moveRange: number;
  abilities: AbilityInst[];
  relics: string[];
  embers: number;
  act: number;
  room: number;
  roomsCleared: number;
  dustEarned: number;
  status: 'route' | 'battle' | 'reward' | 'forge' | 'rest' | 'treasure' | 'won' | 'lost';
  offers: RoomOffer[];
  rewards: RewardOffer[];
  shop: ShopItem[];
  rerollCost: number;
  restOptions: RewardOffer[];
  currentRoom: RoomType | null;
  battleSeed: number;
  encounterKind: 'battle' | 'elite' | 'boss';
  /** Room + RNG snapshot so a mid-battle reload can replay the same fight. */
  pendingBattle: { type: RoomType; rngState: number } | null;
  runKills: number;
  shoveKills: number;
  elitesCleared: number;
  forgePeakDamage: number;
  lastBattle: { kills: number; shoves: number; turns: number; maxCasts: number; turnKills: number } | null;
  log: string[];
}

export interface RunOptions {
  extraHeat?: number;
  extraForge?: number;
  warmStart?: boolean;
}

export function newRun(heroId: string, seed: number, opts: RunOptions = {}): RunState {
  const hero = heroDef(heroId);
  const rng = new Rng(seed);
  const run: RunState = {
    seed,
    rngState: rng.state,
    heroId: hero.id,
    hp: hero.hp,
    maxHp: hero.hp,
    forgeHp: hero.forgeHp + (opts.extraForge ?? 0),
    maxForgeHp: hero.forgeHp + (opts.extraForge ?? 0),
    heatMax: hero.heatMax + (opts.extraHeat ?? 0),
    heatRegen: hero.heatRegen,
    moveRange: hero.moveRange,
    abilities: hero.starter.map((a) => ({ ...a })),
    relics: [],
    embers: 30,
    act: 1,
    room: 0,
    roomsCleared: 0,
    dustEarned: 0,
    status: 'route',
    offers: [],
    rewards: [],
    shop: [],
    rerollCost: 12,
    restOptions: [],
    currentRoom: null,
    battleSeed: 0,
    encounterKind: 'battle',
    pendingBattle: null,
    runKills: 0,
    shoveKills: 0,
    elitesCleared: 0,
    forgePeakDamage: 0,
    lastBattle: null,
    log: [],
  };
  if (opts.warmStart) {
    const pool = RELICS.filter((r) => r.rarity === 'common');
    run.relics.push(rng.pick(pool).id);
    applyRelicStats(run);
  }
  run.offers = rollRooms(run, rng);
  run.rngState = rng.state;
  return run;
}

export function runRng(run: RunState): Rng {
  const rng = new Rng(run.rngState);
  return rng;
}

export function persistRng(run: RunState, rng: Rng): void {
  run.rngState = rng.state;
}

// ── Relics & stats ───────────────────────────────────────────────────────────

export function applyRelicStats(run: RunState): void {
  for (const id of run.relics) {
    if (id === 'bellows2') run.heatMax += 2;
    if (id === 'forge_bond') {
      run.maxForgeHp += 6;
      run.forgeHp = run.maxForgeHp;
    }
    if (id === 'tempered') {
      run.maxHp += 3;
      run.hp = Math.min(run.maxHp, run.hp + 3);
    }
  }
}

export function addRelic(run: RunState, id: string): void {
  if (run.relics.includes(id)) return;
  run.relics.push(id);
  applyRelicStats(run);
}

export function hasRelic(run: RunState, id: string): boolean {
  return run.relics.includes(id);
}

export function addAbility(run: RunState, id: string): boolean {
  if (run.abilities.length >= MAX_ABILITIES) return false;
  if (run.abilities.some((a) => a.id === id)) return false;
  run.abilities.push({ id, level: 0 });
  return true;
}

export function upgradeAbility(run: RunState, id: string): boolean {
  const inst = run.abilities.find((a) => a.id === id);
  if (!inst || inst.level >= 1) return false;
  inst.level = 1;
  return true;
}

// ── Room offers ──────────────────────────────────────────────────────────────

export function isBossRoom(run: RunState): boolean {
  return run.room >= (ACTS[Math.min(run.act, ACTS.length) - 1]?.rooms ?? 4);
}

export function actName(run: RunState): string {
  return ACTS[Math.min(run.act, ACTS.length) - 1]?.name ?? ACTS[0].name;
}

export function rollRooms(run: RunState, rng: Rng): RoomOffer[] {
  if (isBossRoom(run)) {
    return [{ type: 'boss', label: 'Heart of the Keep' }];
  }
  const pool: RoomType[] = ['battle', 'battle', 'elite', 'forge', 'rest', 'treasure'];
  const offers: RoomOffer[] = [];
  const used = new Set<RoomType>();
  let guard = 0;
  while (offers.length < 3 && guard++ < 60) {
    const t = rng.pick(pool);
    if (used.has(t)) continue;
    used.add(t);
    offers.push({ type: t, label: roomLabel(t) });
  }
  // Guarantee at least one safe-ish option every second room.
  if (!offers.some((o) => o.type === 'battle' || o.type === 'forge' || o.type === 'rest')) {
    offers[offers.length - 1] = { type: 'battle', label: roomLabel('battle') };
  }
  return offers;
}

export function roomLabel(t: RoomType): string {
  switch (t) {
    case 'battle':
      return 'Ash Hall';
    case 'elite':
      return 'Deep Foundry';
    case 'forge':
      return 'Cold Forge';
    case 'rest':
      return 'Ember Rest';
    case 'treasure':
      return 'Sealed Vault';
    case 'boss':
      return 'Heart of the Keep';
  }
}

export function roomBlurb(t: RoomType): string {
  switch (t) {
    case 'battle':
      return 'A handful of cinderspawn. Worth Embers.';
    case 'elite':
      return 'An armoured horror. Better loot, sharper teeth.';
    case 'forge':
      return 'Spend Embers on relics, repairs and sharpening.';
    case 'rest':
      return 'Mend the keeper, repair the forge, or hone an ability.';
    case 'treasure':
      return 'Three relics. One choice.';
    case 'boss':
      return 'The thing that wants the forge. No way around it.';
  }
}

// ── Encounters ───────────────────────────────────────────────────────────────

export interface EncounterInfo {
  waves: { groups: { typeId: string; count: number }[]; arrival: number }[];
  bossId: string | null;
  kind: 'battle' | 'elite' | 'boss';
  arenaStyle: ArenaStyle;
  title: string;
  subtitle: string;
}

const ENCOUNTER_NAMES = [
  ['The Cooling Halls', 'Sparks drift down the corridor.'],
  ['Rust Processional', 'Something drags iron across the floor.'],
  ['Ash Gallery', 'The vents are warm. So is the floor.'],
  ['Broken Bellows', 'Old machinery groans in the dark.'],
  ['Cinder Stair', 'Every step exhales smoke.'],
];

export function makeEncounter(run: RunState, rng: Rng, type: RoomType): EncounterInfo {
  const act = run.act;
  const kind: 'battle' | 'elite' | 'boss' = type === 'boss' ? 'boss' : type === 'elite' ? 'elite' : 'battle';
  const style: ArenaStyle = kind === 'boss' ? 'boss' : kind === 'elite' ? 'foundry' : rng.chance(0.25) ? 'vault' : 'hall';
  const pool = ENEMIES.filter(
    (e) => !e.boss && (act >= 2 || (e.minAct ?? 1) <= act) && run.room >= (e.minRoom ?? 0),
  );
  const basic = pool.filter((e) => !e.elite);
  const elites = pool.filter((e) => e.elite || e.id === 'sentinel' || e.id === 'witch');

  if (kind === 'boss') {
    const bossId = BOSSES[Math.min(act, 2)] ?? 'unmaker';
    const adds: { typeId: string; count: number }[] = [{ typeId: 'fly', count: 2 }];
    if (act >= 2) adds.push({ typeId: 'grunt', count: 2 });
    return {
      waves: [
        { groups: [{ typeId: bossId, count: 1 }, ...adds], arrival: 1 },
        { groups: [{ typeId: act >= 2 ? 'brute' : 'grunt', count: act >= 2 ? 2 : 3 }], arrival: 4 },
      ],
      bossId,
      kind,
      arenaStyle: 'boss',
      title: enemyDef(bossId).name,
      subtitle: enemyDef(bossId).desc,
    };
  }

  // Budget is in enemy "cost" points: the first room is a gentle handful,
  // and later rooms layer on pressure without ever becoming a wall of grunts.
  let budget = 4 + act * 2 + Math.floor(run.room * 0.75) + rng.int(2);
  if (kind === 'elite') budget += 4 + act;
  const waves: { groups: { typeId: string; count: number }[]; arrival: number }[] = [];
  const groups: { typeId: string; count: number }[] = [];
  let spent = 0;
  let guard = 0;
  const chosen: string[] = [];
  if (kind === 'elite') {
    const eliteDef = elites.length ? rng.pick(elites) : basic[basic.length - 1];
    chosen.push(eliteDef.id);
    spent += eliteDef.cost;
  }
  while (spent < budget && guard++ < 40) {
    const options = basic.filter((e) => (act >= 2 ? true : e.cost <= budget - spent + 1));
    if (!options.length) break;
    const def = rng.pickWeighted(options, (e) => (e.cost <= budget - spent ? 2 : 0.4));
    if (def.cost + spent > budget + 2) break;
    spent += def.cost;
    chosen.push(def.id);
  }
  for (const id of chosen) {
    const g = groups.find((x) => x.typeId === id);
    if (g) g.count++;
    else groups.push({ typeId: id, count: 1 });
  }
  const first = groups.map((g) => ({ ...g }));
  const second = first.map((g) => ({ typeId: g.typeId, count: Math.max(0, Math.floor(g.count / 2)) })).filter((g) => g.count > 0);
  first.forEach((g) => (g.count = Math.max(0, g.count - (second.find((s) => s.typeId === g.typeId)?.count ?? 0))));
  waves.push({ groups: first.filter((g) => g.count > 0), arrival: 1 });
  if (second.length) waves.push({ groups: second, arrival: act >= 2 ? 3 : 4 });

  const nameIdx = Math.min(ENCOUNTER_NAMES.length - 1, run.room + act - 1);
  const [title, subtitle] = ENCOUNTER_NAMES[Math.max(0, nameIdx)];
  return {
    waves,
    bossId: null,
    kind,
    arenaStyle: style,
    title: kind === 'elite' ? `Elite · ${title}` : title,
    subtitle,
  };
}

/** Builds the board + hero/forge state and returns a fresh battle. */
export function startBattle(run: RunState, rng: Rng, encounter: EncounterInfo): Battle {
  const arena = makeArena(rng, encounter.arenaStyle);
  const hero = heroDef(run.heroId);
  const abilities = run.abilities.map((a) => ({ ...a }));
  const heroState: HeroState = {
    heroId: hero.id,
    name: hero.name,
    pos: { ...arena.heroStart },
    hp: run.hp,
    maxHp: run.maxHp,
    shield: 0,
    heat: run.heatMax,
    maxHeat: run.heatMax,
    heatRegen: run.heatRegen,
    moveRange: run.moveRange,
    movesLeft: true,
    abilities,
  };
  const forge: ForgeState = {
    tiles: arena.forgeTiles.map((t: Vec) => ({ ...t })),
    hp: run.forgeHp,
    maxHp: run.maxForgeHp,
    cracked: 0,
  };
  run.battleSeed = rng.state;
  return createBattle({
    board: arena.board,
    hero: heroState,
    forge,
    relics: run.relics.slice(),
    seed: rng.next() * 0xffffffff,
    waves: encounter.waves.map((w) => ({ groups: w.groups.map((g) => ({ ...g })), arrival: w.arrival })),
    bossId: encounter.bossId,
    eliteBonus: encounter.kind === 'elite',
    heroPassive: hero.id === 'warden' ? 'anchored' : hero.id === 'wisp' ? 'flicker' : 'kindling',
    roomKind: encounter.kind,
  });
}

// ── Rewards ──────────────────────────────────────────────────────────────────

export function rewardPool(run: RunState): RewardOffer[] {
  const pool: RewardOffer[] = [];
  const ownedRelics = new Set(run.relics);
  const ownedAbilities = new Set(run.abilities.map((a) => a.id));
  const relicChoices = RELICS.filter((r) => !ownedRelics.has(r.id));
  for (const r of relicChoices) {
    pool.push({
      key: `relic:${r.id}`,
      kind: 'relic',
      title: r.name,
      desc: r.desc,
      icon: r.icon,
      rarity: r.rarity,
      relicId: r.id,
    });
  }
  if (run.abilities.length < MAX_ABILITIES) {
    for (const a of ABILITIES) {
      if (ownedAbilities.has(a.id)) continue;
      if (a.heroOnly && a.heroOnly !== run.heroId) continue;
      pool.push({
        key: `ability:${a.id}`,
        kind: 'ability',
        title: a.name,
        desc: a.text(0),
        icon: a.icon,
        rarity: a.rarity,
        abilityId: a.id,
      });
    }
  }
  const upgradable = run.abilities.filter((a) => a.level < 1);
  if (upgradable.length) {
    pool.push({
      key: 'upgrade:random',
      kind: 'upgrade',
      title: 'Sharpen',
      desc: 'Upgrade one of your abilities (random).',
      icon: 'hammer',
      rarity: 'common',
    });
  }
  pool.push({
    key: 'heal:4',
    kind: 'heal',
    title: 'Ember Draught',
    desc: 'Heal 5 health.',
    icon: 'heal',
    rarity: 'common',
  });
  pool.push({
    key: 'repair:4',
    kind: 'repair',
    title: 'Patch the Forge',
    desc: 'Repair the forge by 5.',
    icon: 'anvil',
    rarity: 'common',
  });
  pool.push({
    key: 'embers:20',
    kind: 'embers',
    title: 'Pouch of Embers',
    desc: 'Gain 25 Embers.',
    icon: 'coin',
    rarity: 'common',
  });
  return pool;
}

export function rollRewards(run: RunState, rng: Rng, count = 3, elite = false): RewardOffer[] {
  const pool = rewardPool(run);
  const picked: RewardOffer[] = [];
  const rarityBoost = elite ? 1.8 : 1;
  const weight = (o: RewardOffer) => {
    if (o.rarity === 'epic') return 0.5 * rarityBoost;
    if (o.rarity === 'rare') return 1.4 * rarityBoost;
    if (o.kind === 'ability' || o.kind === 'relic') return 2;
    return 1;
  };
  let guard = 0;
  while (picked.length < count && guard++ < 100) {
    const choice = rng.pickWeighted(pool, weight);
    if (picked.some((p) => p.key === choice.key)) continue;
    picked.push(choice);
  }
  return picked;
}

export function grantReward(run: RunState, offer: RewardOffer, rng: Rng): string {
  switch (offer.kind) {
    case 'relic':
      addRelic(run, offer.relicId!);
      return `Gained relic: ${relicDef(offer.relicId!).name}`;
    case 'ability':
      addAbility(run, offer.abilityId!);
      return `Learned ${abilityDef(offer.abilityId!).name}`;
    case 'upgrade': {
      const upgradable = run.abilities.filter((a) => a.level < 1);
      if (!upgradable.length) return 'Nothing left to sharpen.';
      const target = rng.pick(upgradable);
      upgradeAbility(run, target.id);
      return `Sharpened ${abilityDef(target.id).name}`;
    }
    case 'heal': {
      const before = run.hp;
      run.hp = Math.min(run.maxHp, run.hp + 5);
      return `Healed ${run.hp - before}.`;
    }
    case 'repair': {
      const before = run.forgeHp;
      run.forgeHp = Math.min(run.maxForgeHp, run.forgeHp + 5);
      return `Forge repaired by ${run.forgeHp - before}.`;
    }
    case 'embers':
      run.embers += 25;
      return 'Gained 25 Embers.';
  }
}

// ── Shop ─────────────────────────────────────────────────────────────────────

export function rollShop(run: RunState, rng: Rng): ShopItem[] {
  const pool = rewardPool(run).filter((r) => r.kind === 'relic' || r.kind === 'ability');
  const items: ShopItem[] = [];
  const chosen: RewardOffer[] = [];
  let guard = 0;
  while (chosen.length < 3 && guard++ < 100) {
    const rarityBias = (o: RewardOffer) => (o.rarity === 'epic' ? 0.5 : o.rarity === 'rare' ? 1.3 : 2);
    const pick = rng.pickWeighted(pool, rarityBias);
    if (chosen.some((c) => c.key === pick.key)) continue;
    chosen.push(pick);
  }
  const costFor = (r: Rarity) => (r === 'epic' ? 90 : r === 'rare' ? 70 : 48);
  for (const c of chosen) {
    items.push({
      key: c.key,
      kind: c.kind === 'relic' ? 'relic' : 'ability',
      title: c.title,
      desc: c.desc,
      icon: c.icon,
      rarity: c.rarity,
      price: costFor(c.rarity) + rng.int(10),
      relicId: c.relicId,
      abilityId: c.abilityId,
    });
  }
  items.push({
    key: `heal:shop`,
    kind: 'heal',
    title: 'Ember Draught',
    desc: 'Heal 6 health.',
    icon: 'heal',
    rarity: 'common',
    price: 26,
  });
  items.push({
    key: `repair:shop`,
    kind: 'repair',
    title: 'Forge Repairs',
    desc: 'Repair 7 forge health.',
    icon: 'anvil',
    rarity: 'common',
    price: 30,
  });
  return items;
}

export function buyItem(run: RunState, item: ShopItem, rng: Rng): { ok: boolean; message: string } {
  if (run.embers < item.price) return { ok: false, message: 'Not enough Embers.' };
  if (item.kind === 'relic') {
    if (run.relics.includes(item.relicId!)) return { ok: false, message: 'Already owned.' };
    run.embers -= item.price;
    addRelic(run, item.relicId!);
    return { ok: true, message: `Bought ${item.title}.` };
  }
  if (item.kind === 'ability') {
    if (run.abilities.length >= MAX_ABILITIES) return { ok: false, message: 'Your belt is full (5 abilities).' };
    if (run.abilities.some((a) => a.id === item.abilityId)) return { ok: false, message: 'Already known.' };
    run.embers -= item.price;
    addAbility(run, item.abilityId!);
    return { ok: true, message: `Learned ${item.title}.` };
  }
  if (item.kind === 'heal') {
    if (run.hp >= run.maxHp) return { ok: false, message: 'Already at full health.' };
    run.embers -= item.price;
    run.hp = Math.min(run.maxHp, run.hp + 6);
    return { ok: true, message: 'Healed 6.' };
  }
  if (item.kind === 'repair') {
    if (run.forgeHp >= run.maxForgeHp) return { ok: false, message: 'The forge is intact.' };
    run.embers -= item.price;
    run.forgeHp = Math.min(run.maxForgeHp, run.forgeHp + 7);
    return { ok: true, message: 'Forge repaired by 7.' };
  }
  void rng;
  return { ok: false, message: 'Nothing happens.' };
}

// ── Rest ─────────────────────────────────────────────────────────────────────

export function rollRest(run: RunState): RewardOffer[] {
  const options: RewardOffer[] = [
    { key: 'rest:heal', kind: 'heal', title: 'Mend', desc: 'Heal 6 health.', icon: 'heal', rarity: 'common' },
    { key: 'rest:repair', kind: 'repair', title: 'Repair', desc: 'Restore 7 forge health.', icon: 'anvil', rarity: 'common' },
    { key: 'rest:sharpen', kind: 'upgrade', title: 'Sharpen', desc: 'Upgrade one ability (random).', icon: 'hammer', rarity: 'rare' },
  ];
  if (run.abilities.every((a) => a.level >= 1)) {
    options[2] = { key: 'rest:embers', kind: 'embers', title: 'Bank the Coals', desc: 'Gain 30 Embers.', icon: 'coin', rarity: 'common' };
  }
  return options;
}

export function applyRest(run: RunState, offer: RewardOffer, rng: Rng): string {
  if (offer.key === 'rest:heal' || offer.key === 'rest:repair') {
    const g = grantReward(run, { ...offer, kind: offer.key === 'rest:heal' ? 'heal' : 'repair' }, rng);
    // Rest heals are slightly stronger than battle rewards.
    if (offer.key === 'rest:heal') {
      run.hp = Math.min(run.maxHp, run.hp + 1);
    } else {
      run.forgeHp = Math.min(run.maxForgeHp, run.forgeHp + 2);
    }
    return g;
  }
  if (offer.key === 'rest:embers') {
    run.embers += 30;
    return 'Gained 30 Embers.';
  }
  return grantReward(run, offer, rng);
}

// ── Progression helpers ──────────────────────────────────────────────────────

export interface BattleOutcome {
  won: boolean;
  reason: string;
  embers: number;
  dust: number;
  kills: number;
  shoveKills: number;
}

export function finishBattle(run: RunState, battle: Battle, rng: Rng): BattleOutcome {
  run.hp = Math.max(0, battle.hero.hp);
  run.forgeHp = Math.max(0, battle.forge.hp);
  run.runKills += battle.kills;
  run.shoveKills += battle.shoveKills;
  const maxCasts = Math.max(run.lastBattle?.maxCasts ?? 0, battle.castsThisTurn);
  const maxTurnKills = Math.max(run.lastBattle?.turnKills ?? 0, battle.killsThisTurn);
  run.lastBattle = {
    kills: battle.kills,
    shoves: battle.shoveKills,
    turns: battle.turn,
    maxCasts,
    turnKills: maxTurnKills,
  };
  if (battle.phase === 'lost' || battle.hero.hp <= 0 || battle.forge.hp <= 0) {
    run.status = 'lost';
    return {
      won: false,
      reason: battle.hero.hp <= 0 ? 'The Emberkeeper goes dark.' : 'The last forge is cold.',
      embers: 0,
      dust: Math.round(run.dustEarned),
      kills: battle.kills,
      shoveKills: battle.shoveKills,
    };
  }
  const elite = battle.roomKind === 'elite';
  const boss = battle.roomKind === 'boss';
  const embers = Math.round((elite ? 34 : boss ? 60 : 20) + run.act * 6 + (hasRelic(run, 'hoarder') ? 1 : 0)) + rng.int(6);
  const dust = Math.round((elite ? 22 : boss ? 70 : 10) + run.act * 8 + battle.kills * 2);
  run.embers += embers;
  run.dustEarned += dust;
  run.roomsCleared++;
  if (elite) run.elitesCleared++;
  run.log.push(`${boss ? 'Boss' : elite ? 'Elite' : 'Room'} cleared: +${embers} Embers`);
  if (boss) {
    if (run.act >= 2) {
      run.status = 'won';
      return { won: true, reason: 'The forge breathes again.', embers, dust, kills: battle.kills, shoveKills: battle.shoveKills };
    }
    run.act++;
    run.room = 0;
    run.status = 'reward';
    run.rewards = rollRewards(run, rng, 3, true);
    run.offers = rollRooms(run, rng);
    return { won: true, reason: 'The keep opens deeper.', embers, dust, kills: battle.kills, shoveKills: battle.shoveKills };
  }
  run.room++;
  run.status = 'reward';
  run.rewards = rollRewards(run, rng, 3, elite);
  run.offers = rollRooms(run, rng);
  return { won: true, reason: 'The hall falls quiet.', embers, dust, kills: battle.kills, shoveKills: battle.shoveKills };
}

export function enterRoom(run: RunState, rng: Rng, type: RoomType): void {
  run.currentRoom = type;
  if (type === 'forge') {
    run.shop = rollShop(run, rng);
    run.status = 'forge';
  } else if (type === 'rest') {
    run.restOptions = rollRest(run);
    run.status = 'rest';
  } else if (type === 'treasure') {
    run.rewards = rollRewards(run, rng, 3, true);
    run.status = 'treasure';
  } else {
    run.encounterKind = type === 'boss' ? 'boss' : type === 'elite' ? 'elite' : 'battle';
    run.status = 'battle';
    run.pendingBattle = { type, rngState: rng.state };
  }
  persistRng(run, rng);
}

export function continueRoute(run: RunState, rng: Rng): void {
  run.status = 'route';
  run.pendingBattle = null;
  run.currentRoom = null;
  run.rewards = [];
  run.shop = [];
  run.restOptions = [];
  if (isBossRoom(run) && run.offers[0]?.type !== 'boss') {
    run.offers = [{ type: 'boss', label: roomLabel('boss') }];
  } else if (!run.offers.length) {
    run.offers = rollRooms(run, rng);
  }
  persistRng(run, rng);
}

export function abilityPreview(run: RunState, inst: AbilityInst): { cost: number; text: string; name: string } {
  const def = abilityDef(inst.id);
  return {
    cost: abilityStats(inst.id, inst.level).cost,
    text: def.text(inst.level),
    name: def.name + (inst.level > 0 ? ' +' : ''),
  };
}
