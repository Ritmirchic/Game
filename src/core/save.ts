// ── Persistent meta progression (localStorage, versioned, corruption-safe) ───
import type { RunState } from './run';
import { SAVE_VERSION } from './run';
import { ACHIEVEMENTS } from './content';

export interface Settings {
  music: number;
  sfx: number;
  muted: boolean;
  reduceMotion: boolean;
}

export interface MetaData {
  dust: number;
  unlocks: string[];
  heroes: string[];
  relicsSeen: string[];
  abilitiesSeen: string[];
  enemiesSeen: string[];
}

export interface Stats {
  runs: number;
  wins: number;
  kills: number;
  shoveKills: number;
  eliteClears: number;
  bestRooms: number;
  bestTurnKills: number;
  bestCasts: number;
  totalDust: number;
  totalEmbersSpent: number;
  defeats: number;
}

export interface SaveData {
  version: number;
  settings: Settings;
  meta: MetaData;
  achievements: Record<string, boolean>;
  stats: Stats;
  run: RunState | null;
  lastHero: string;
  introSeen: boolean;
  updatedAt: number;
}

const KEY = 'anvilight:save:v3';
const LEGACY_KEYS = ['anvilight:save', 'anvilight:save:v1', 'anvilight:save:v2'];

export function defaultSave(): SaveData {
  return {
    version: SAVE_VERSION,
    settings: { music: 0.45, sfx: 0.7, muted: false, reduceMotion: false },
    meta: { dust: 0, unlocks: [], heroes: ['keeper'], relicsSeen: [], abilitiesSeen: [], enemiesSeen: [] },
    achievements: {},
    stats: {
      runs: 0,
      wins: 0,
      kills: 0,
      shoveKills: 0,
      eliteClears: 0,
      bestRooms: 0,
      bestTurnKills: 0,
      bestCasts: 0,
      totalDust: 0,
      totalEmbersSpent: 0,
      defeats: 0,
    },
    run: null,
    lastHero: 'keeper',
    introSeen: false,
    updatedAt: Date.now(),
  };
}

let memorySave: SaveData | null = null;
let storageOk = true;

function storage(): Storage | null {
  if (!storageOk) return null;
  try {
    const s = window.localStorage;
    const probe = '__anvilight_probe__';
    s.setItem(probe, '1');
    s.removeItem(probe);
    return s;
  } catch {
    storageOk = false;
    return null;
  }
}

function num(v: unknown, fallback: number, lo = 0, hi = 1): number {
  const n = typeof v === 'number' && Number.isFinite(v) ? v : fallback;
  return Math.max(lo, Math.min(hi, n));
}

function int(v: unknown, fallback = 0): number {
  return typeof v === 'number' && Number.isFinite(v) ? Math.max(0, Math.floor(v)) : fallback;
}

function strArray(v: unknown): string[] {
  if (!Array.isArray(v)) return [];
  return v.filter((x): x is string => typeof x === 'string');
}

/** Coerces whatever is in storage into a valid SaveData, or null. */
export function sanitize(raw: unknown): SaveData | null {
  if (!raw || typeof raw !== 'object') return null;
  const o = raw as Record<string, unknown>;
  const base = defaultSave();
  const settings = (o.settings ?? {}) as Record<string, unknown>;
  const meta = (o.meta ?? {}) as Record<string, unknown>;
  const stats = (o.stats ?? {}) as Record<string, unknown>;
  const achievements: Record<string, boolean> = {};
  if (o.achievements && typeof o.achievements === 'object') {
    for (const [k, v] of Object.entries(o.achievements as Record<string, unknown>)) {
      if (typeof v === 'boolean' && v) achievements[k] = true;
    }
  }
  const run = (o.run ?? null) as RunState | null;
  const resumable =
    !!run &&
    typeof run === 'object' &&
    typeof run.heroId === 'string' &&
    Array.isArray(run.abilities) &&
    Array.isArray(run.offers) &&
    (run.status !== 'battle' || !!run.pendingBattle);
  const validRun = resumable ? run : null;
  return {
    version: SAVE_VERSION,
    settings: {
      music: num(settings.music, base.settings.music),
      sfx: num(settings.sfx, base.settings.sfx),
      muted: settings.muted === true,
      reduceMotion: settings.reduceMotion === true,
    },
    meta: {
      dust: int(meta.dust),
      unlocks: strArray(meta.unlocks),
      heroes: Array.from(new Set(['keeper', ...strArray(meta.heroes)])),
      relicsSeen: strArray(meta.relicsSeen),
      abilitiesSeen: strArray(meta.abilitiesSeen),
      enemiesSeen: strArray(meta.enemiesSeen),
    },
    achievements,
    stats: {
      runs: int(stats.runs),
      wins: int(stats.wins),
      kills: int(stats.kills),
      shoveKills: int(stats.shoveKills),
      eliteClears: int(stats.eliteClears),
      bestRooms: int(stats.bestRooms),
      bestTurnKills: int(stats.bestTurnKills),
      bestCasts: int(stats.bestCasts),
      totalDust: int(stats.totalDust),
      totalEmbersSpent: int(stats.totalEmbersSpent),
      defeats: int(stats.defeats),
    },
    run: validRun,
    lastHero: typeof o.lastHero === 'string' ? o.lastHero : base.lastHero,
    introSeen: o.introSeen === true,
    updatedAt: int(o.updatedAt, Date.now()),
  };
}

export function loadSave(): SaveData {
  const s = storage();
  if (!s) return memorySave ?? (memorySave = defaultSave());
  for (const key of [KEY, ...LEGACY_KEYS]) {
    const text = s.getItem(key);
    if (!text) continue;
    try {
      const parsed = sanitize(JSON.parse(text));
      if (parsed) {
        if (key !== KEY) writeSave(parsed);
        return parsed;
      }
    } catch {
      // Corrupt entry: drop it and continue to the next candidate.
      s.removeItem(key);
    }
  }
  const fresh = defaultSave();
  writeSave(fresh);
  return fresh;
}

export function writeSave(data: SaveData): void {
  data.updatedAt = Date.now();
  memorySave = data;
  const s = storage();
  if (!s) return;
  try {
    s.setItem(KEY, JSON.stringify(data));
  } catch {
    // Quota or private-mode failure: the in-memory save keeps the session going.
  }
}

export function clearSave(): SaveData {
  const s = storage();
  if (s) {
    for (const key of [KEY, ...LEGACY_KEYS]) s.removeItem(key);
  }
  memorySave = null;
  const fresh = defaultSave();
  writeSave(fresh);
  return fresh;
}

export function exportSave(data: SaveData): string {
  return JSON.stringify(data, null, 2);
}

// ── Achievements ─────────────────────────────────────────────────────────────

export interface AchievementCtx {
  battleWon?: boolean;
  runWon?: boolean;
  heroId?: string;
  bossDefeated?: string | null;
  forgeUntouched?: boolean;
  roomsCleared?: number;
  turnKills?: number;
  casts?: number;
}

/** Returns the ids of achievements newly unlocked by this context. */
export function checkAchievements(save: SaveData, ctx: AchievementCtx): string[] {
  const unlocked: string[] = [];
  const grant = (id: string) => {
    if (save.achievements[id]) return;
    if (!ACHIEVEMENTS.some((a) => a.id === id)) return;
    save.achievements[id] = true;
    unlocked.push(id);
  };
  if (ctx.battleWon) grant('first_blood');
  if (ctx.runWon) {
    grant('win_run');
    if (ctx.heroId === 'warden') grant('warden_win');
    if (ctx.heroId === 'wisp') grant('wisp_win');
  }
  if (ctx.bossDefeated === 'maw') grant('boss1');
  if (ctx.bossDefeated === 'unmaker') grant('boss2');
  if (ctx.runWon && ctx.forgeUntouched) grant('no_forge_damage');
  if (save.stats.shoveKills >= 25) grant('shover');
  if ((ctx.turnKills ?? 0) >= 3 || save.stats.bestTurnKills >= 3) grant('combo');
  if ((ctx.casts ?? 0) >= 4 || save.stats.bestCasts >= 4) grant('heat_max');
  if (save.stats.eliteClears >= 5) grant('elite_hunter');
  if ((ctx.roomsCleared ?? 0) >= 8) grant('deep_run');
  return unlocked;
}

export function achievementDef(id: string) {
  return ACHIEVEMENTS.find((a) => a.id === id);
}
