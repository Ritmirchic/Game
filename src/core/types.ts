export type Direction = 0 | 1 | 2 | 3; // 0: East, 1: South, 2: West, 3: North

export type ModuleCategory = 'turret' | 'prism';
export type ElementType = 'pyro' | 'cryo' | 'volt' | 'venom';
export type Rarity = 'common' | 'rare' | 'legendary';
export type TileMutation = 'normal' | 'radiant' | 'haste' | 'prismatic' | 'locked';

export type ModuleId =
  | 'turret_blaze'
  | 'turret_arc'
  | 'turret_frost'
  | 'turret_venom'
  | 'turret_beam'
  | 'turret_mortar'
  | 'splitter'
  | 'tri_prism'
  | 'mirror'
  | 'amp_lens'
  | 'pyro_lens'
  | 'cryo_lens'
  | 'volt_lens'
  | 'midas_core';

export interface ModuleDefinition {
  id: ModuleId;
  name: string;
  category: ModuleCategory;
  rarity: Rarity;
  cost: number;
  spriteKey: string;
  rotatable: boolean;
  tags: string[];
  shortDesc: string;
  // Base stats indexed by tier (0 -> Tier I, 1 -> Tier II, 2 -> Tier III)
  baseDamage: [number, number, number];
  multFactor: [number, number, number];
  passThroughRatio: [number, number, number];
  element?: ElementType;
}

export interface PlacedModule {
  uid: string;
  defId: ModuleId;
  tier: 1 | 2 | 3;
  rotation: Direction; // 0=East, 1=South, 2=West, 3=North
  pulseCounter: number;
  totalDamageDealt: number;
}

export interface GridCell {
  col: number;
  row: number;
  mutation: TileMutation;
  module: PlacedModule | null;
}

export interface PulsePacket {
  id: string;
  col: number;
  row: number;
  dir: Direction;
  progress: number; // 0..1 across current cell
  speedCellsPerSec: number;
  powerMult: number;
  critBonus: number;
  elements: Set<ElementType>;
  hopsRemaining: number;
  bounces: number;
  tilesVisited: number;
}

export interface PreviewSegment {
  fromCol: number;
  fromRow: number;
  toCol: number;
  toRow: number;
  powerEstimate: number;
  elements: ElementType[];
}

export interface TurretFireCommand {
  moduleUid: string;
  defId: ModuleId;
  tier: 1 | 2 | 3;
  col: number;
  row: number;
  powerMult: number;
  critChance: number;
  elements: ElementType[];
  comboChain: number;
}

export type RelicId =
  | 'relic_prism_crown'
  | 'relic_pendulum'
  | 'relic_glass_lens'
  | 'relic_alembic'
  | 'relic_storm_coil'
  | 'relic_shatter_core'
  | 'relic_solar_heart'
  | 'relic_abacus'
  | 'relic_void_mirror'
  | 'relic_twin_dynamo'
  | 'relic_exec_sigil'
  | 'relic_compass'
  | 'relic_phoenix'
  | 'relic_capacitor'
  | 'relic_trinity'
  | 'relic_monocle'
  | 'relic_vampiric'
  | 'relic_archon_shard';

export interface RelicDefinition {
  id: RelicId;
  name: string;
  rarity: Rarity;
  cost: number;
  spriteKey: string;
  description: string;
}

export type WardenId = 'lyra' | 'vex' | 'kael' | 'nyx';

export interface WardenDefinition {
  id: WardenId;
  name: string;
  title: string;
  portraitKey: string;
  ultIconKey: string;
  ultName: string;
  ultDesc: string;
  passiveName: string;
  passiveDesc: string;
  unlockCost: number;
  startingModules: Array<{ defId: ModuleId; col: number; row: number; rotation?: Direction }>;
  startingGoldBonus: number;
  emitterIntervalScale: number;
}

export type WeatherId = 'clear' | 'solar_flare' | 'ion_storm' | 'cryo_nebula' | 'blood_eclipse';

export interface WeatherDefinition {
  id: WeatherId;
  name: string;
  iconKey: string;
  tintHex: number;
  description: string;
}

export type GameModeId =
  | 'campaign'
  | 'endless'
  | 'challenge_mirror'
  | 'challenge_swarm'
  | 'challenge_fragile';

export interface GameModeDefinition {
  id: GameModeId;
  name: string;
  subtitle: string;
  targetWaves: number; // 0 = infinite
  starlightMult: number;
  rulesDesc: string;
}

export type EnemyId =
  | 'skitterling'
  | 'bulwark'
  | 'wraith'
  | 'brood'
  | 'mite'
  | 'acolyte'
  | 'sprinter'
  | 'sentinel'
  | 'boss_colossus'
  | 'boss_weaver'
  | 'boss_archon';

export interface EnemyDefinition {
  id: EnemyId;
  name: string;
  spriteKey: string;
  isBoss: boolean;
  baseHp: number;
  speed: number; // path progress per second (0..1 full lane)
  armor: number;
  shield: number;
  goldDrop: number;
  coreDamage: number;
  scale: number;
  description: string;
}

export interface EnemyInstance {
  uid: string;
  defId: EnemyId;
  isBoss: boolean;
  lane: 0 | 1 | 2;
  progress: number; // 0 = Void Rift, 1 = Sunspire Core
  x: number;
  y: number;
  maxHp: number;
  hp: number;
  maxShield: number;
  shield: number;
  armor: number;
  baseSpeed: number;
  goldDrop: number;
  coreDamage: number;
  // Status effects
  burnDps: number;
  burnTimer: number;
  chillStacks: number;
  chillTimer: number;
  freezeTimer: number;
  shockTimer: number;
  venomStacks: number;
  venomTimer: number;
  phaseTimer: number;
  isPhased: boolean;
  specialCooldown: number;
  enraged: boolean;
}

export interface ShopOffer {
  slotIndex: number;
  kind: 'module' | 'relic';
  moduleId?: ModuleId;
  relicId?: RelicId;
  cost: number;
  locked: boolean;
  purchased: boolean;
}

export interface SkillNodeDefinition {
  id: string;
  branch: 'solar' | 'bastion' | 'economy';
  name: string;
  description: string;
  maxRank: number;
  baseCost: number;
  costStep: number;
  prereq?: string;
  x: number; // normalized layout coords
  y: number;
}

export interface AchievementDefinition {
  id: string;
  name: string;
  description: string;
  rewardStarlight: number;
}

export interface GameSettings {
  masterVolume: number;
  sfxVolume: number;
  musicVolume: number;
  muted: boolean;
  screenShake: boolean;
  damageNumbers: boolean;
  defaultSpeed: 1 | 1.5 | 2;
}

export interface LifetimeStats {
  runsStarted: number;
  victories: number;
  highestWave: number;
  enemiesVanquished: number;
  bossesVanquished: number;
  totalGoldEarned: number;
  totalStarlightEarned: number;
  highestPulseMult: number;
  highestSingleHit: number;
  modulesMerged: number;
  pulsesEmitted: number;
}

export interface ActiveRunSave {
  wardenId: WardenId;
  modeId: GameModeId;
  wave: number;
  gold: number;
  coreHp: number;
  maxCoreHp: number;
  coreShield: number;
  weatherId: WeatherId;
  relics: RelicId[];
  grid: Array<{
    col: number;
    row: number;
    mutation: TileMutation;
    module: { defId: ModuleId; tier: 1 | 2 | 3; rotation: Direction } | null;
  }>;
  bench: Array<{ defId: ModuleId; tier: 1 | 2 | 3; rotation: Direction } | null>;
  rerollCount: number;
}

export interface SaveDataV2 {
  version: 2;
  checksum: number;
  starlight: number;
  unlockedWardens: WardenId[];
  skillRanks: Record<string, number>;
  achievements: string[];
  discoveredModules: ModuleId[];
  discoveredRelics: RelicId[];
  discoveredEnemies: EnemyId[];
  settings: GameSettings;
  stats: LifetimeStats;
  activeRun: ActiveRunSave | null;
}
