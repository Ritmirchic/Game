import { ACHIEVEMENTS, SKILL_TREE, WARDENS } from './content';
import {
  AchievementDefinition,
  ActiveRunSave,
  EnemyId,
  GameSettings,
  LifetimeStats,
  ModuleId,
  RelicId,
  SaveDataV2,
  WardenId,
} from './types';

export const SAVE_STORAGE_KEY = 'prismforge_save_v2';
export const CORRUPT_BACKUP_KEY = 'prismforge_corrupt_backup';

export function computeChecksum(payload: Omit<SaveDataV2, 'checksum'>): number {
  const raw = JSON.stringify(payload);
  let hash = 2166136261;
  for (let i = 0; i < raw.length; i++) {
    hash ^= raw.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

export function createDefaultSettings(): GameSettings {
  return {
    masterVolume: 0.8,
    sfxVolume: 0.85,
    musicVolume: 0.55,
    muted: false,
    screenShake: true,
    damageNumbers: true,
    defaultSpeed: 1,
  };
}

export function createDefaultStats(): LifetimeStats {
  return {
    runsStarted: 0,
    victories: 0,
    highestWave: 0,
    enemiesVanquished: 0,
    bossesVanquished: 0,
    totalGoldEarned: 0,
    totalStarlightEarned: 0,
    highestPulseMult: 1,
    highestSingleHit: 0,
    modulesMerged: 0,
    pulsesEmitted: 0,
  };
}

export function createDefaultSave(): SaveDataV2 {
  const base: Omit<SaveDataV2, 'checksum'> = {
    version: 2,
    starlight: 20, // Enough for an exciting first upgrade right away if desired
    unlockedWardens: ['lyra'],
    skillRanks: {},
    achievements: [],
    discoveredModules: ['amp_lens', 'turret_blaze', 'splitter', 'mirror'],
    discoveredRelics: [],
    discoveredEnemies: ['skitterling'],
    settings: createDefaultSettings(),
    stats: createDefaultStats(),
    activeRun: null,
  };
  return {
    ...base,
    checksum: computeChecksum(base),
  };
}

function isValidSaveShape(obj: unknown): obj is SaveDataV2 {
  if (!obj || typeof obj !== 'object') return false;
  const s = obj as Record<string, unknown>;
  if (s.version !== 2) return false;
  if (typeof s.starlight !== 'number' || !Number.isFinite(s.starlight) || s.starlight < 0) return false;
  if (!Array.isArray(s.unlockedWardens) || s.unlockedWardens.length === 0) return false;
  if (!s.skillRanks || typeof s.skillRanks !== 'object') return false;
  if (!Array.isArray(s.achievements)) return false;
  if (!s.settings || typeof s.settings !== 'object') return false;
  if (!s.stats || typeof s.stats !== 'object') return false;
  return true;
}

export class SaveManager {
  private data: SaveDataV2;
  private storage: Storage | null;
  public recoveredFromCorruption = false;

  constructor(customStorage?: Storage) {
    this.storage = customStorage ?? (typeof window !== 'undefined' ? window.localStorage : null);
    this.data = this.load();
  }

  public load(): SaveDataV2 {
    if (!this.storage) {
      return createDefaultSave();
    }
    const raw = this.storage.getItem(SAVE_STORAGE_KEY);
    if (!raw) {
      const fresh = createDefaultSave();
      this.persist(fresh);
      return fresh;
    }

    try {
      const parsed = JSON.parse(raw);
      // Migrate v1 if encountered
      if (parsed && typeof parsed === 'object' && parsed.version === 1) {
        const def = createDefaultSave();
        const migrated: Omit<SaveDataV2, 'checksum'> = {
          ...def,
          starlight: typeof parsed.starlight === 'number' && parsed.starlight >= 0 ? parsed.starlight : def.starlight,
          unlockedWardens: Array.isArray(parsed.unlockedWardens) ? parsed.unlockedWardens : def.unlockedWardens,
        };
        const full: SaveDataV2 = { ...migrated, checksum: computeChecksum(migrated) };
        this.persist(full);
        return full;
      }

      if (!isValidSaveShape(parsed)) {
        throw new Error('Invalid save schema');
      }

      const { checksum, ...rest } = parsed;
      const expected = computeChecksum(rest);
      if (checksum !== expected) {
        throw new Error('Save checksum mismatch');
      }

      return parsed;
    } catch {
      this.recoveredFromCorruption = true;
      try {
        this.storage.setItem(CORRUPT_BACKUP_KEY, raw);
      } catch {
        // Ignore storage quota errors
      }
      const fresh = createDefaultSave();
      this.persist(fresh);
      return fresh;
    }
  }

  private persist(target: SaveDataV2 = this.data): void {
    const rest: Omit<SaveDataV2, 'checksum'> = {
      version: target.version,
      starlight: target.starlight,
      unlockedWardens: target.unlockedWardens,
      skillRanks: target.skillRanks,
      achievements: target.achievements,
      discoveredModules: target.discoveredModules,
      discoveredRelics: target.discoveredRelics,
      discoveredEnemies: target.discoveredEnemies,
      settings: target.settings,
      stats: target.stats,
      activeRun: target.activeRun,
    };
    target.checksum = computeChecksum(rest);
    if (!this.storage) return;
    try {
      this.storage.setItem(SAVE_STORAGE_KEY, JSON.stringify(target));
    } catch {
      // Ignore storage quota errors in restricted environments
    }
  }

  public getData(): SaveDataV2 {
    return this.data;
  }

  public resetAll(): SaveDataV2 {
    this.data = createDefaultSave();
    this.persist();
    return this.data;
  }

  public getSkillRank(nodeId: string): number {
    return this.data.skillRanks[nodeId] ?? 0;
  }

  public getSkillCost(nodeId: string): number {
    const node = SKILL_TREE.find((n) => n.id === nodeId);
    if (!node) return 999999;
    const rank = this.getSkillRank(nodeId);
    return node.baseCost + rank * node.costStep;
  }

  public canUpgradeSkill(nodeId: string): boolean {
    const node = SKILL_TREE.find((n) => n.id === nodeId);
    if (!node) return false;
    const rank = this.getSkillRank(nodeId);
    if (rank >= node.maxRank) return false;
    if (node.prereq && this.getSkillRank(node.prereq) <= 0) return false;
    return this.data.starlight >= this.getSkillCost(nodeId);
  }

  public upgradeSkill(nodeId: string): { ok: boolean; unlockedAchievements: AchievementDefinition[] } {
    if (!this.canUpgradeSkill(nodeId)) return { ok: false, unlockedAchievements: [] };
    const cost = this.getSkillCost(nodeId);
    this.data.starlight -= cost;
    this.data.skillRanks[nodeId] = this.getSkillRank(nodeId) + 1;
    const unlocked = this.evaluateMetaAchievements();
    this.persist();
    return { ok: true, unlockedAchievements: unlocked };
  }

  public unlockWarden(wardenId: WardenId): { ok: boolean; unlockedAchievements: AchievementDefinition[] } {
    if (this.data.unlockedWardens.includes(wardenId)) {
      return { ok: false, unlockedAchievements: [] };
    }
    const w = WARDENS[wardenId];
    if (!w || this.data.starlight < w.unlockCost) {
      return { ok: false, unlockedAchievements: [] };
    }
    this.data.starlight -= w.unlockCost;
    this.data.unlockedWardens.push(wardenId);
    const unlocked = this.unlockAchievement('ach_unlock_warden');
    this.persist();
    return { ok: true, unlockedAchievements: unlocked };
  }

  public addStarlight(amount: number): void {
    if (amount <= 0) return;
    const rounded = Math.round(amount);
    this.data.starlight += rounded;
    this.data.stats.totalStarlightEarned += rounded;
    this.persist();
  }

  public discoverModule(id: ModuleId): void {
    if (!this.data.discoveredModules.includes(id)) {
      this.data.discoveredModules.push(id);
      this.persist();
    }
  }

  public discoverRelic(id: RelicId): void {
    if (!this.data.discoveredRelics.includes(id)) {
      this.data.discoveredRelics.push(id);
      this.persist();
    }
  }

  public discoverEnemy(id: EnemyId): void {
    if (!this.data.discoveredEnemies.includes(id)) {
      this.data.discoveredEnemies.push(id);
      this.persist();
    }
  }

  public unlockAchievement(achId: string): AchievementDefinition[] {
    if (this.data.achievements.includes(achId)) return [];
    const def = ACHIEVEMENTS.find((a) => a.id === achId);
    if (!def) return [];
    this.data.achievements.push(achId);
    this.data.starlight += def.rewardStarlight;
    this.data.stats.totalStarlightEarned += def.rewardStarlight;
    this.persist();
    return [def];
  }

  public evaluateMetaAchievements(): AchievementDefinition[] {
    const newlyUnlocked: AchievementDefinition[] = [];
    const totalSkillRanks = Object.values(this.data.skillRanks).reduce((a, b) => a + b, 0);
    if (totalSkillRanks >= 5) {
      newlyUnlocked.push(...this.unlockAchievement('ach_skill_5'));
    }
    if (this.data.stats.enemiesVanquished >= 250) {
      newlyUnlocked.push(...this.unlockAchievement('ach_slayer_250'));
    }
    if (this.data.stats.enemiesVanquished >= 1000) {
      newlyUnlocked.push(...this.unlockAchievement('ach_slayer_1000'));
    }
    return newlyUnlocked;
  }

  public updateSettings(partial: Partial<GameSettings>): void {
    this.data.settings = { ...this.data.settings, ...partial };
    this.persist();
  }

  public saveActiveRun(run: ActiveRunSave | null): void {
    this.data.activeRun = run;
    this.persist();
  }

  public mutateStats(mutator: (stats: LifetimeStats) => void): AchievementDefinition[] {
    mutator(this.data.stats);
    const unlocked = this.evaluateMetaAchievements();
    this.persist();
    return unlocked;
  }
}
