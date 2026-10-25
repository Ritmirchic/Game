// ── Game content: heroes, abilities, relics, enemies, achievements ───────────
import type { AbilityInst } from './types';

export type Rarity = 'starter' | 'common' | 'rare' | 'epic';

export interface HeroDef {
  id: string;
  name: string;
  epithet: string;
  blurb: string;
  hp: number;
  forgeHp: number;
  heatMax: number;
  heatRegen: number;
  moveRange: number;
  passiveName: string;
  passiveDesc: string;
  starter: AbilityInst[];
  unlockDust: number;
  playstyle: string;
}

export const HEROES: HeroDef[] = [
  {
    id: 'keeper',
    name: 'The Emberkeeper',
    epithet: 'keeper of the last coal',
    blurb: 'A wanderer with a lantern full of borrowed fire. Balanced, forgiving, and the only one the forge still answers.',
    hp: 12,
    forgeHp: 18,
    heatMax: 6,
    heatRegen: 2,
    moveRange: 4,
    passiveName: 'Kindling',
    passiveDesc: 'Kills refund 2 Heat instead of 1.',
    starter: [
      { id: 'scorch', level: 0 },
      { id: 'ash_step', level: 0 },
    ],
    unlockDust: 0,
    playstyle: 'Balanced · momentum',
  },
  {
    id: 'warden',
    name: 'The Slagwarden',
    epithet: 'wall of cooled iron',
    blurb: 'A foundry guard who never learned to step back. Slow to move, impossible to move.',
    hp: 16,
    forgeHp: 14,
    heatMax: 5,
    heatRegen: 2,
    moveRange: 3,
    passiveName: 'Anchored',
    passiveDesc: 'Takes 1 less damage from enemy attacks. Shoves travel 1 tile further.',
    starter: [
      { id: 'bulwark', level: 0 },
      { id: 'quake', level: 0 },
    ],
    unlockDust: 120,
    playstyle: 'Tanky · displacement',
  },
  {
    id: 'wisp',
    name: 'The Ashwisp',
    epithet: 'a spark that learned to walk',
    blurb: 'Half cinder, half intention. Fragile, slippery, and always somewhere you did not expect.',
    hp: 9,
    forgeHp: 14,
    heatMax: 7,
    heatRegen: 3,
    moveRange: 5,
    passiveName: 'Flicker',
    passiveDesc: 'Ash Step is free and reaches 2 tiles further. Starts with Forge Bolt.',
    starter: [
      { id: 'ash_step', level: 0 },
      { id: 'forge_bolt', level: 0 },
    ],
    unlockDust: 260,
    playstyle: 'Glass · mobility',
  },
];

export function heroDef(id: string): HeroDef {
  return HEROES.find((h) => h.id === id) ?? HEROES[0];
}

// ── Abilities ────────────────────────────────────────────────────────────────

export type AbilityTargeting = 'adjacent' | 'tile' | 'enemy' | 'self' | 'line';

export interface AbilityDef {
  id: string;
  name: string;
  icon: string;
  cost: number;
  range: number;
  dmg: number;
  push: number;
  targeting: AbilityTargeting;
  rarity: Rarity;
  flavor: string;
  text: (level: number) => string;
  /** Numbers after a single upgrade. */
  upgraded: (level: number) => { cost: number; range: number; dmg: number; push: number };
  heroOnly?: string;
}

const up = (base: Partial<AbilityDef>, lvl: number, delta: Partial<{ cost: number; range: number; dmg: number; push: number }>) => {
  const cost = Math.max(0, (base.cost ?? 0) + (lvl > 0 ? (delta.cost ?? 0) : 0));
  const range = Math.max(1, (base.range ?? 1) + (lvl > 0 ? (delta.range ?? 0) : 0));
  const dmg = Math.max(0, (base.dmg ?? 0) + (lvl > 0 ? (delta.dmg ?? 0) : 0));
  const push = Math.max(0, (base.push ?? 0) + (lvl > 0 ? (delta.push ?? 0) : 0));
  return { cost, range, dmg, push };
};

export const ABILITIES: AbilityDef[] = [
  {
    id: 'scorch',
    name: 'Scorch',
    icon: 'flame',
    cost: 1,
    range: 1,
    dmg: 2,
    push: 1,
    targeting: 'adjacent',
    rarity: 'starter',
    flavor: 'A palmful of forge-fire, thrown politely.',
    text: (l) => `Deal ${2 + (l ? 1 : 0)} damage to an adjacent foe and shove it 1 tile.`,
    upgraded: (l) => up({ cost: 1, range: 1, dmg: 2, push: 1 }, l, { dmg: 1 }),
  },
  {
    id: 'ash_step',
    name: 'Ash Step',
    icon: 'step',
    cost: 1,
    range: 3,
    dmg: 0,
    push: 0,
    targeting: 'tile',
    rarity: 'starter',
    flavor: 'Become the smoke, briefly.',
    text: (l) => `Blink to any open tile within ${3 + (l ? 1 : 0)}. Walls are only a suggestion.`,
    upgraded: (l) => up({ cost: 1, range: 3, dmg: 0, push: 0 }, l, { range: 1 }),
  },
  {
    id: 'ember_lance',
    name: 'Ember Lance',
    icon: 'lance',
    cost: 2,
    range: 4,
    dmg: 3,
    push: 1,
    targeting: 'line',
    rarity: 'common',
    flavor: 'Draw a line. Erase what stands on it.',
    text: (l) => `Fire a lance along a straight line. First foe hit takes ${3 + (l ? 1 : 0)} damage and is shoved 1.`,
    upgraded: (l) => up({ cost: 2, range: 4, dmg: 3, push: 1 }, l, { dmg: 1 }),
  },
  {
    id: 'quake',
    name: 'Quake',
    icon: 'quake',
    cost: 2,
    range: 1,
    dmg: 1,
    push: 1,
    targeting: 'self',
    rarity: 'common',
    flavor: 'The floor remembers being lava.',
    text: (l) => `All adjacent foes take ${1 + (l ? 1 : 0)} damage and are shoved 1 tile away.`,
    upgraded: (l) => up({ cost: 2, range: 1, dmg: 1, push: 1 }, l, { dmg: 1 }),
  },
  {
    id: 'forge_bolt',
    name: 'Forge Bolt',
    icon: 'bolt',
    cost: 2,
    range: 5,
    dmg: 3,
    push: 0,
    targeting: 'enemy',
    rarity: 'common',
    flavor: 'A spark with a grudge.',
    text: (l) => `Deal ${3 + (l ? 2 : 0)} damage to a foe within 5 tiles.`,
    upgraded: (l) => up({ cost: 2, range: 5, dmg: 3, push: 0 }, l, { dmg: 2 }),
  },
  {
    id: 'cinder_wall',
    name: 'Cinder Wall',
    icon: 'wall',
    cost: 1,
    range: 3,
    dmg: 0,
    push: 0,
    targeting: 'tile',
    rarity: 'common',
    flavor: 'Architecture, but fast.',
    text: () => 'Raise a wall on an open tile within 3. Walls block everything until the battle ends.',
    upgraded: (l) => up({ cost: 1, range: 3, dmg: 0, push: 0 }, l, { range: 1 }),
  },
  {
    id: 'bellows',
    name: 'Bellows',
    icon: 'bellows',
    cost: 1,
    range: 4,
    dmg: 1,
    push: 2,
    targeting: 'enemy',
    rarity: 'common',
    flavor: 'Inhale. Then un-inhale at them.',
    text: (l) => `Yank a foe within 4 two tiles closer and deal ${1 + (l ? 1 : 0)} damage.`,
    upgraded: (l) => up({ cost: 1, range: 4, dmg: 1, push: 2 }, l, { dmg: 1 }),
  },
  {
    id: 'temper',
    name: 'Temper',
    icon: 'shield',
    cost: 1,
    range: 0,
    dmg: 0,
    push: 0,
    targeting: 'self',
    rarity: 'common',
    flavor: 'Harden the edge of yourself.',
    text: (l) => `Gain ${4 + (l ? 2 : 0)} Shield. Shield soaks damage before health.`,
    upgraded: (l) => up({ cost: 1, range: 0, dmg: 0, push: 0 }, l, {}),
  },
  {
    id: 'reforge',
    name: 'Reforge',
    icon: 'heal',
    cost: 2,
    range: 0,
    dmg: 0,
    push: 0,
    targeting: 'self',
    rarity: 'common',
    flavor: 'Dents hammered out from the inside.',
    text: (l) => `Heal ${4 + (l ? 2 : 0)} health.`,
    upgraded: (l) => up({ cost: 2, range: 0, dmg: 0, push: 0 }, l, {}),
  },
  {
    id: 'slag_bomb',
    name: 'Slag Bomb',
    icon: 'bomb',
    cost: 2,
    range: 3,
    dmg: 4,
    push: 1,
    targeting: 'tile',
    rarity: 'rare',
    flavor: 'Set it. Forget it. Regret it nearby.',
    text: (l) => `Plant a bomb within 3. Next enemy phase it erupts: ${4 + (l ? 2 : 0)} damage and a shove in a 3×3.`,
    upgraded: (l) => up({ cost: 2, range: 3, dmg: 4, push: 1 }, l, { dmg: 2 }),
  },
  {
    id: 'molten_brand',
    name: 'Molten Brand',
    icon: 'brand',
    cost: 1,
    range: 3,
    dmg: 0,
    push: 0,
    targeting: 'enemy',
    rarity: 'common',
    flavor: 'A signature that burns to read.',
    text: (l) => `Brand a foe within 3: the next damage it takes is increased by ${3 + (l ? 2 : 0)}.`,
    upgraded: (l) => up({ cost: 1, range: 3, dmg: 0, push: 0 }, l, {}),
  },
  {
    id: 'flare',
    name: 'Flashfire',
    icon: 'flare',
    cost: 3,
    range: 2,
    dmg: 2,
    push: 0,
    targeting: 'self',
    rarity: 'rare',
    flavor: 'For one heartbeat the forge fits in your chest.',
    text: (l) => `Foes within 2 take ${2 + (l ? 1 : 0)} damage and Burn for 2 turns.`,
    upgraded: (l) => up({ cost: 3, range: 2, dmg: 2, push: 0 }, l, { dmg: 1 }),
  },
  {
    id: 'chain_hook',
    name: 'Chain Hook',
    icon: 'hook',
    cost: 1,
    range: 3,
    dmg: 2,
    push: 0,
    targeting: 'enemy',
    rarity: 'common',
    flavor: "The forge's old crane, repurposed.",
    text: (l) => `Hook a foe within 3, drag it next to you, deal ${2 + (l ? 1 : 0)} damage.`,
    upgraded: (l) => up({ cost: 1, range: 3, dmg: 2, push: 0 }, l, { dmg: 1 }),
  },
  {
    id: 'vent_call',
    name: 'Vent Call',
    icon: 'vent',
    cost: 2,
    range: 4,
    dmg: 0,
    push: 0,
    targeting: 'tile',
    rarity: 'rare',
    flavor: 'Ask the deep for a favour.',
    text: (l) => `Crack open a steam vent on an open tile within 4 (${l ? 'no cost' : 'permanent'}). Anything entering takes 2.`,
    upgraded: (l) => up({ cost: 2, range: 4, dmg: 0, push: 0 }, l, { cost: -1 }),
  },
  {
    id: 'bulwark',
    name: 'Bulwark',
    icon: 'bulwark',
    cost: 2,
    range: 1,
    dmg: 1,
    push: 2,
    targeting: 'self',
    rarity: 'starter',
    flavor: 'The foundry does not move. You do.',
    text: (l) => `Shove all adjacent foes 2 tiles and gain ${3 + (l ? 2 : 0)} Shield.`,
    upgraded: (l) => up({ cost: 2, range: 1, dmg: 1, push: 2 }, l, {}),
    heroOnly: 'warden',
  },
  {
    id: 'pyre_dash',
    name: 'Pyre Dash',
    icon: 'dash',
    cost: 2,
    range: 4,
    dmg: 2,
    push: 0,
    targeting: 'tile',
    rarity: 'rare',
    flavor: 'Arrive already on fire.',
    text: (l) => `Dash up to 4 tiles, then deal ${2 + (l ? 1 : 0)} damage to foes adjacent to your landing spot.`,
    upgraded: (l) => up({ cost: 2, range: 4, dmg: 2, push: 0 }, l, { dmg: 1 }),
  },
  {
    id: 'iron_tithe',
    name: 'Iron Tithe',
    icon: 'tithe',
    cost: 2,
    range: 3,
    dmg: 3,
    push: 1,
    targeting: 'enemy',
    rarity: 'epic',
    flavor: 'The forge always collects.',
    text: (l) => `Deal ${3 + (l ? 1 : 0)} damage within 3, shove 1, and repair the forge for 2.`,
    upgraded: (l) => up({ cost: 2, range: 3, dmg: 3, push: 1 }, l, { dmg: 1 }),
  },
];

export function abilityDef(id: string): AbilityDef {
  return ABILITIES.find((a) => a.id === id) ?? ABILITIES[0];
}

export function abilityStats(id: string, level: number) {
  const def = abilityDef(id);
  return def.upgraded(level);
}

// ── Relics ───────────────────────────────────────────────────────────────────

export interface RelicDef {
  id: string;
  name: string;
  icon: string;
  rarity: Rarity;
  desc: string;
}

export const RELICS: RelicDef[] = [
  { id: 'whetstone', name: 'Whetstone', icon: 'whetstone', rarity: 'common', desc: 'All ability damage +1.' },
  { id: 'bellows2', name: 'Second Bellows', icon: 'bellows', rarity: 'common', desc: 'Maximum Heat +2.' },
  { id: 'cinder_core', name: 'Cinder Core', icon: 'core', rarity: 'rare', desc: 'Regenerate 1 extra Heat each turn.' },
  { id: 'start_shield', name: 'Kiln Plating', icon: 'shield', rarity: 'common', desc: 'Start every battle with 5 Shield.' },
  { id: 'momentum', name: 'Momentum', icon: 'momentum', rarity: 'rare', desc: 'All shoves travel 1 tile further.' },
  { id: 'vent_grace', name: 'Ventwalker', icon: 'vent', rarity: 'common', desc: 'You ignore vent damage. Foes do not.' },
  { id: 'executioner', name: 'Executioner', icon: 'axe', rarity: 'rare', desc: '+3 damage to foes below 40% health.' },
  { id: 'kindler', name: 'Kindler', icon: 'flame', rarity: 'rare', desc: 'Kills refund an additional 1 Heat.' },
  { id: 'echo_anvil', name: 'Echo Anvil', icon: 'echo', rarity: 'epic', desc: 'The first ability each battle costs 0 Heat.' },
  { id: 'forge_bond', name: 'Forge Bond', icon: 'anvil', rarity: 'rare', desc: 'Forge max health +6, repaired to full.' },
  { id: 'tempered', name: 'Tempered Body', icon: 'heart', rarity: 'common', desc: 'Hero max health +3, healed by 3.' },
  { id: 'slag_skin', name: 'Slag Skin', icon: 'armor', rarity: 'rare', desc: 'Enemy attacks deal 1 less damage to you (min 1).' },
  { id: 'spark_trail', name: 'Spark Trail', icon: 'spark', rarity: 'rare', desc: 'Moving next to a foe burns it for 1.' },
  { id: 'pyre', name: 'Standing Pyre', icon: 'pyre', rarity: 'epic', desc: 'Adjacent foes take 1 damage at the start of each turn.' },
  { id: 'bomb_kit', name: 'Bombardier Kit', icon: 'bomb', rarity: 'rare', desc: 'Bombs deal +2 damage and stun for 1 turn.' },
  { id: 'hoarder', name: 'Ash Hoarder', icon: 'coin', rarity: 'common', desc: 'Gain 1 extra Ember from every room.' },
  { id: 'ricochet', name: 'Ricochet', icon: 'ricochet', rarity: 'rare', desc: 'Shoving a foe into a wall deals +2 impact damage.' },
  { id: 'ember_blood', name: 'Ember Blood', icon: 'blood', rarity: 'epic', desc: 'Heal 1 whenever you kill something.' },
];

export function relicDef(id: string): RelicDef {
  return RELICS.find((r) => r.id === id) ?? RELICS[0];
}

// ── Enemies ──────────────────────────────────────────────────────────────────

export type EnemyAI = 'melee' | 'ranged' | 'support' | 'exploder' | 'special';

export interface EnemyDef {
  id: string;
  name: string;
  hp: number;
  dmg: number;
  speed: number;
  range: number;
  cost: number;
  ai: EnemyAI;
  armored?: boolean;
  boss?: boolean;
  elite?: boolean;
  special?: 'quake' | 'summon' | 'beam';
  desc: string;
  minAct?: number;
  /** First run-room (0-indexed) in which this enemy may appear. */
  minRoom?: number;
}

export const ENEMIES: EnemyDef[] = [
  { id: 'grunt', name: 'Cinder Grunt', hp: 3, dmg: 1, speed: 2, range: 1, cost: 1, ai: 'melee', desc: 'Small, cracked, and flammable. Walks at the nearest warm thing.' },
  { id: 'fly', name: 'Emberfly', hp: 1, dmg: 1, speed: 3, range: 1, cost: 1, ai: 'melee', desc: 'Fast. Loud. Dies to a stern look.' },
  { id: 'brute', name: 'Rust Brute', hp: 8, dmg: 2, speed: 1, range: 1, cost: 3, ai: 'melee', armored: true, minRoom: 2, desc: 'Slow iron mass. Shrugs off the first tile of every shove. Headed for the forge.' },
  { id: 'spitter', name: 'Ash Spitter', hp: 3, dmg: 2, speed: 1, range: 3, cost: 2, ai: 'ranged', desc: 'Shuffles for a clear line, then spits cinders down it. Break line of sight.' },
  { id: 'bloater', name: 'Slag Bloater', hp: 4, dmg: 2, speed: 1, range: 1, cost: 2, ai: 'exploder', minRoom: 1, desc: 'Bursts when it dies, and it always gets close enough to matter.' },
  { id: 'shielder', name: 'Chorister', hp: 4, dmg: 1, speed: 1, range: 1, cost: 2, ai: 'support', minRoom: 1, desc: 'Hums armour onto its neighbours. Kill it before the wall of iron arrives.' },
  { id: 'burrower', name: 'Deep Burrower', hp: 5, minRoom: 1, dmg: 2, speed: 1, range: 1, cost: 3, ai: 'melee', desc: 'Surfaces where you are standing. Cannot be blocked by furniture.' },
  { id: 'eater', name: 'Forge Eater', hp: 7, dmg: 3, speed: 1, range: 1, cost: 3, ai: 'melee', minRoom: 2, desc: 'Ignores you completely. Only the forge will do.' },
  { id: 'sentinel', name: 'Rust Sentinel', hp: 11, dmg: 2, speed: 1, range: 1, cost: 4, ai: 'melee', armored: true, elite: true, desc: 'A knight with no kingdom. Hits back when struck in melee.' },
  { id: 'witch', name: 'Cinder Witch', hp: 8, dmg: 3, speed: 1, range: 4, cost: 4, ai: 'ranged', elite: true, desc: 'Fires long beams and blinks away when hurt.' },
  { id: 'maw', name: 'The Slagmaw', hp: 26, dmg: 3, speed: 1, range: 1, cost: 0, ai: 'special', boss: true, special: 'quake', desc: 'A mouth in the floor. Every third turn the whole board shakes.' },
  { id: 'regent', name: 'The Rust Regent', hp: 32, dmg: 2, speed: 1, range: 3, cost: 0, ai: 'special', boss: true, special: 'summon', desc: 'Calls its court of grunts and beams across the hall.' },
  { id: 'unmaker', name: 'The Unmaker', hp: 44, dmg: 4, speed: 1, range: 1, cost: 0, ai: 'special', boss: true, special: 'beam', desc: 'The thing the forge has been dreading. Three turns, three hungers.' },
];

export function enemyDef(id: string): EnemyDef {
  return ENEMIES.find((e) => e.id === id) ?? ENEMIES[0];
}

// ── Room / progression flavour ───────────────────────────────────────────────

export interface RoomTypeDef {
  id: RoomType;
  name: string;
  icon: string;
  desc: string;
}

export type RoomType = 'battle' | 'elite' | 'forge' | 'rest' | 'treasure' | 'boss';

export const ROOMS: RoomTypeDef[] = [
  { id: 'battle', name: 'Ash Hall', icon: 'sword', desc: 'Sparks in the dark. A fight.' },
  { id: 'elite', name: 'Deep Foundry', icon: 'skull', desc: 'Something armoured is waiting.' },
  { id: 'forge', name: 'Cold Forge', icon: 'anvil', desc: 'Trade Embers for relics and repairs.' },
  { id: 'rest', name: 'Ember Rest', icon: 'camp', desc: 'Heal, repair, or sharpen an ability.' },
  { id: 'treasure', name: 'Sealed Vault', icon: 'chest', desc: 'Pick one relic from three.' },
  { id: 'boss', name: 'Heart of the Keep', icon: 'boss', desc: 'The forge\'s enemy arrives.' },
];

export const ACTS = [
  { name: 'Act I · The Cooling Halls', rooms: 4 },
  { name: 'Act II · The Rusted Deep', rooms: 4 },
];

export const BOSSES: Record<number, string> = { 1: 'maw', 2: 'unmaker' };

// ── Achievements ─────────────────────────────────────────────────────────────

export interface AchievementDef {
  id: string;
  name: string;
  desc: string;
  icon: string;
  secret?: boolean;
}

export const ACHIEVEMENTS: AchievementDef[] = [
  { id: 'first_blood', name: 'First Sparks', desc: 'Win your first battle.', icon: 'flame' },
  { id: 'win_run', name: 'Keeper of the Last Coal', desc: 'Complete a run.', icon: 'trophy' },
  { id: 'boss1', name: 'Mouth Full of Rust', desc: 'Defeat the Slagmaw.', icon: 'skull' },
  { id: 'boss2', name: 'Unmaking Undone', desc: 'Defeat the Unmaker.', icon: 'crown' },
  { id: 'no_forge_damage', name: 'Untouched Anvil', desc: 'Finish a run with the forge at full health.', icon: 'anvil', secret: true },
  { id: 'shover', name: 'Terrain Tactician', desc: 'Kill 25 foes with shoves, walls or vents over your career.', icon: 'momentum' },
  { id: 'combo', name: 'Perfect Turn', desc: 'Kill 3 foes in a single turn.', icon: 'spark' },
  { id: 'heat_max', name: 'White Hot', desc: 'Cast 4 abilities in one turn.', icon: 'core', secret: true },
  { id: 'elite_hunter', name: 'Foundry Sweeper', desc: 'Clear 5 elite rooms over your career.', icon: 'sword' },
  { id: 'warden_win', name: 'Iron Verdict', desc: 'Win a run with the Slagwarden.', icon: 'armor' },
  { id: 'wisp_win', name: 'Nothing But Smoke', desc: 'Win a run with the Ashwisp.', icon: 'step', secret: true },
  { id: 'deep_run', name: 'Long Shift', desc: 'Clear 8 rooms in a single run.', icon: 'camp' },
];

// ── Unlock shop (meta) ───────────────────────────────────────────────────────

export interface MetaUnlockDef {
  id: string;
  name: string;
  desc: string;
  cost: number;
  icon: string;
}

export const META_UNLOCKS: MetaUnlockDef[] = [
  { id: 'hero_warden', name: 'The Slagwarden', desc: 'Unlock a tanky displacement hero.', cost: 120, icon: 'armor' },
  { id: 'hero_wisp', name: 'The Ashwisp', desc: 'Unlock a fragile mobility hero.', cost: 260, icon: 'step' },
  { id: 'start_common', name: 'Warm Start', desc: 'Begin every run with a common relic.', cost: 150, icon: 'chest' },
  { id: 'extra_heat', name: 'Metal Lungs', desc: '+1 maximum Heat on every hero.', cost: 200, icon: 'bellows' },
  { id: 'extra_forge', name: 'Deep Foundations', desc: '+4 forge health on every hero.', cost: 220, icon: 'anvil' },
];
