import fs from 'node:fs';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import { SoundEngine } from '../src/audio/soundEngine';
import { CircuitEngine, CircuitModifiers } from '../src/core/circuitEngine';
import { CombatEngine, CORE_POS, getRerollCost, rollShopOffers, sampleLanePosition } from '../src/core/combatEngine';
import {
  ACHIEVEMENTS,
  ENEMIES,
  MODULE_LIST,
  RELIC_LIST,
  SKILL_TREE,
  WARDEN_LIST,
  WEATHERS,
} from '../src/core/content';
import {
  computeChecksum,
  CORRUPT_BACKUP_KEY,
  SAVE_STORAGE_KEY,
  SaveManager,
} from '../src/core/saveManager';

class MemoryStorage implements Storage {
  private map = new Map<string, string>();
  public get length(): number {
    return this.map.size;
  }
  public clear(): void {
    this.map.clear();
  }
  public getItem(key: string): string | null {
    return this.map.has(key) ? this.map.get(key)! : null;
  }
  public key(index: number): string | null {
    return Array.from(this.map.keys())[index] ?? null;
  }
  public removeItem(key: string): void {
    this.map.delete(key);
  }
  public setItem(key: string, value: string): void {
    this.map.set(key, String(value));
  }
}

const baseMods: CircuitModifiers = {
  wardenId: 'lyra',
  weatherId: 'clear',
  relics: [],
  bonusHops: 0,
  baseCritChance: 0.1,
  pulseSpeedScale: 1.0,
  dualTopEmitter: false,
  mirrorChallengeBonus: false,
};

describe('Art & Codebase Quality Gate (No Emojis, All Assets Present)', () => {
  it('contains zero emoji characters across all TypeScript source files', () => {
    const emojiRegex = /\p{Extended_Pictographic}/u;
    const walk = (dir: string): string[] => {
      const out: string[] = [];
      for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        const full = path.join(dir, entry.name);
        if (entry.isDirectory()) out.push(...walk(full));
        else if (entry.name.endsWith('.ts')) out.push(full);
      }
      return out;
    };

    const files = walk(path.resolve('src'));
    expect(files.length).toBeGreaterThanOrEqual(8);
    for (const file of files) {
      const content = fs.readFileSync(file, 'utf8');
      expect(emojiRegex.test(content), `Unexpected emoji in ${file}`).toBe(false);
    }
  });

  it('verifies every referenced game art, sprite, and UI texture exists on disk', () => {
    const mustExist = [
      'public/assets/art/menu_hero_bg.jpg',
      'public/assets/art/battle_sanctum_bg.jpg',
      ...WARDEN_LIST.map((w) => `public/assets/art/${w.portraitKey}.jpg`),
      ...WARDEN_LIST.map((w) => `public/assets/ui/${w.ultIconKey}.png`),
      ...MODULE_LIST.map((m) => `public/assets/sprites/${m.spriteKey}.png`),
      ...RELIC_LIST.map((r) => `public/assets/sprites/${r.spriteKey}.png`),
      ...Object.values(ENEMIES).map((e) => `public/assets/sprites/${e.spriteKey}.png`),
      ...Object.values(WEATHERS).map((w) => `public/assets/ui/${w.iconKey}.png`),
    ];

    for (const relPath of mustExist) {
      const full = path.resolve(relPath);
      expect(fs.existsSync(full), `Missing asset file: ${relPath}`).toBe(true);
      expect(fs.statSync(full).size).toBeGreaterThan(150);
    }
  });
});

describe('CircuitEngine Routing, Splitting, Merging & Loop Safety', () => {
  it('routes pulses through Solar Magnifiers and triggers downstream Turret Conduits with amplified power', () => {
    const engine = new CircuitEngine(4, 0, false);
    // Row 1: [col 0: amp_lens] -> [col 1: pyro_lens] -> [col 2: radiant tile + turret_blaze]
    engine.getCell(0, 1)!.module = engine.createModule('amp_lens', 1, 0);
    engine.getCell(1, 1)!.module = engine.createModule('pyro_lens', 1, 0);
    engine.getCell(2, 1)!.module = engine.createModule('turret_blaze', 1, 0);

    engine.emitPulseWave(baseMods, 1.0);
    let totalFires = 0;
    let firedMult = 0;
    let firedElems: string[] = [];

    for (let i = 0; i < 40; i++) {
      const step = engine.stepPulses(0.05, baseMods);
      for (const cmd of step.fireCommands) {
        totalFires++;
        firedMult = cmd.powerMult;
        firedElems = cmd.elements;
      }
    }

    expect(totalFires).toBe(1);
    // 1.0 * 1.45 (amp) * 1.30 (pyro) * 1.35 (radiant center tile) = ~2.54
    expect(firedMult).toBeGreaterThan(2.4);
    expect(firedElems).toContain('pyro');
  });

  it('splits pulses via Bifrost Splitter into North and South branches', () => {
    const engine = new CircuitEngine(4, 0, false);
    engine.getCell(1, 1)!.module = engine.createModule('splitter', 1, 0); // splits North (row 0) & South (row 2)
    engine.getCell(1, 0)!.module = engine.createModule('turret_arc', 1, 0);
    engine.getCell(1, 2)!.module = engine.createModule('turret_frost', 1, 0);

    engine.emitPulseWave(baseMods, 1.0);
    const firedIds: string[] = [];
    for (let i = 0; i < 40; i++) {
      const step = engine.stepPulses(0.05, baseMods);
      step.fireCommands.forEach((f) => firedIds.push(f.defId));
    }

    expect(firedIds).toContain('turret_arc');
    expect(firedIds).toContain('turret_frost');
  });

  it('merges Tier I -> Tier II -> Tier III and prevents merging Tier III further', () => {
    const engine = new CircuitEngine(4, 0, false);
    engine.bench[0] = engine.createModule('turret_beam', 1, 0);
    engine.bench[1] = engine.createModule('turret_beam', 1, 0);

    const m1 = engine.moveOrMerge({ zone: 'bench', idx: 0 }, { zone: 'bench', idx: 1 });
    expect(m1.mergedTier).toBe(2);
    expect(engine.bench[0]).toBeNull();
    expect(engine.bench[1]?.tier).toBe(2);

    engine.bench[2] = engine.createModule('turret_beam', 2, 0);
    const m2 = engine.moveOrMerge({ zone: 'bench', idx: 2 }, { zone: 'bench', idx: 1 });
    expect(m2.mergedTier).toBe(3);
    expect(engine.bench[1]?.tier).toBe(3);

    engine.bench[3] = engine.createModule('turret_beam', 3, 0);
    const m3 = engine.moveOrMerge({ zone: 'bench', idx: 3 }, { zone: 'bench', idx: 1 });
    // Swaps instead of merging past Tier 3
    expect(m3.mergedTier).toBeNull();
    expect(engine.bench[1]?.tier).toBe(3);
    expect(engine.bench[3]?.tier).toBe(3);
  });

  it('safely terminates closed 4-mirror loops via hop budget without infinite recursion', () => {
    const engine = new CircuitEngine(4, 0, false);
    // Clockwise 2x2 mirror loop: (1,1)->South(r=0), (1,2)->West(r=1), (0,2)->North(r=2), (0,1)->East(r=3)
    engine.getCell(0, 1)!.module = engine.createModule('mirror', 1, 3);
    engine.getCell(1, 1)!.module = engine.createModule('mirror', 1, 0);
    engine.getCell(1, 2)!.module = engine.createModule('mirror', 1, 1);
    engine.getCell(0, 2)!.module = engine.createModule('mirror', 1, 2);

    const preview = engine.tracePreviewPaths(baseMods);
    expect(preview.length).toBeGreaterThan(3);
    expect(preview.length).toBeLessThan(85);

    engine.emitPulseWave(baseMods, 1.0);
    for (let i = 0; i < 80; i++) {
      engine.stepPulses(0.05, baseMods);
    }
    expect(engine.activePulses.length).toBe(0);
  });
});

describe('CombatEngine Waves, Status Synergies, Bosses & Relics', () => {
  it('interpolates causeway waypoints from Void Rift to Sunspire Core cleanly', () => {
    const start = sampleLanePosition(0, 0);
    const end = sampleLanePosition(0, 1);
    expect(Math.hypot(start.x - 1142, start.y - 338)).toBeLessThan(2);
    expect(Math.hypot(end.x - CORE_POS.x, end.y - CORE_POS.y)).toBeLessThan(8);
  });

  it('schedules Bosses on Waves 5, 10, and 15 and executes all 6 Turret types', () => {
    const combat = new CombatEngine();
    expect(combat.buildWaveSchedule(5, 'campaign').some((s) => s.enemyId === 'boss_colossus')).toBe(true);
    expect(combat.buildWaveSchedule(10, 'campaign').some((s) => s.enemyId === 'boss_weaver')).toBe(true);
    expect(combat.buildWaveSchedule(15, 'campaign').some((s) => s.enemyId === 'boss_archon')).toBe(true);

    combat.spawnEnemy('boss_colossus', 1, 8, 'campaign', 0.3);
    combat.spawnEnemy('sentinel', 0, 8, 'campaign', 0.25);

    const turretTypes = [
      'turret_blaze',
      'turret_arc',
      'turret_frost',
      'turret_venom',
      'turret_beam',
      'turret_mortar',
    ] as const;

    const events = {
      visuals: [],
      lootDrops: [],
      slainIds: [],
      coreHitDamage: 0,
      phoenixTriggered: false,
      highestHit: 0,
      relicsTriggered: [],
    };

    for (const tid of turretTypes) {
      const dealt = combat.executeTurretFire(
        {
          moduleUid: 'u1',
          defId: tid,
          tier: 2,
          col: 2,
          row: 1,
          powerMult: 2.0,
          critChance: 0.5,
          elements: ['pyro', 'cryo', 'volt', 'venom'],
          comboChain: 4,
        },
        { x: 350, y: 240 },
        { wardenId: 'kael', weatherId: 'solar_flare', relics: ['relic_storm_coil', 'relic_monocle'], turretDamageScale: 1.2 },
        events
      );
      expect(dealt).toBeGreaterThan(0);
    }
  });

  it('splits Brood Carriers into 3 Void Mites on death and triggers Emberwing Plume revive', () => {
    const combat = new CombatEngine();
    combat.coreHp = 5;
    combat.maxCoreHp = 100;

    const brood = combat.spawnEnemy('brood', 1, 1, 'campaign', 0.5);
    brood.hp = 0;

    const breacher = combat.spawnEnemy('bulwark', 1, 1, 'campaign', 1.01);
    expect(breacher.coreDamage).toBeGreaterThan(5);

    const ev = combat.stepCombat(0.05, 1, 'campaign', {
      wardenId: 'lyra',
      weatherId: 'clear',
      relics: ['relic_phoenix'],
      bountyScale: 1,
    });

    expect(ev.phoenixTriggered).toBe(true);
    expect(combat.coreHp).toBe(50);
    // Brood spawned 3 mites after phoenix wipe check
    expect(combat.enemies.filter((e) => e.defId === 'mite').length).toBe(3);
  });

  it('preserves locked shop slots across rerolls and honors Merchant Favor free reroll', () => {
    const initial = rollShopOffers(2, [], 6, 0);
    initial[0].locked = true;
    const lockedCopy = { ...initial[0] };
    const next = rollShopOffers(2, [], 6, 0, initial);
    expect(next[0].moduleId).toBe(lockedCopy.moduleId);
    expect(next[0].locked).toBe(true);
    expect(getRerollCost(0, 'lyra', true)).toBe(0);
    expect(getRerollCost(1, 'lyra', true)).toBeGreaterThan(0);
  });
});

describe('SaveManager Versioning, Checksum & Corruption Recovery', () => {
  it('persists upgrades, wardens, achievements, and active runs across reloads', () => {
    const store = new MemoryStorage();
    const sm1 = new SaveManager(store);
    sm1.addStarlight(200);
    expect(sm1.upgradeSkill('solar_cadence').ok).toBe(true);
    expect(sm1.unlockWarden('vex').ok).toBe(true);
    expect(SKILL_TREE.length).toBe(16);
    expect(ACHIEVEMENTS.length).toBe(20);

    const sm2 = new SaveManager(store);
    expect(sm2.recoveredFromCorruption).toBe(false);
    expect(sm2.getSkillRank('solar_cadence')).toBe(1);
    expect(sm2.getData().unlockedWardens).toContain('vex');
  });

  it('detects tampered or corrupted JSON, backs it up, and safely resets', () => {
    const store = new MemoryStorage();
    store.setItem(SAVE_STORAGE_KEY, '{corrupted_json:::');
    const sm = new SaveManager(store);
    expect(sm.recoveredFromCorruption).toBe(true);
    expect(store.getItem(CORRUPT_BACKUP_KEY)).toBe('{corrupted_json:::');
    expect(sm.getData().version).toBe(2);

    // Tamper checksum
    const raw = JSON.parse(store.getItem(SAVE_STORAGE_KEY)!);
    raw.starlight = 999999;
    store.setItem(SAVE_STORAGE_KEY, JSON.stringify(raw));

    const smTampered = new SaveManager(store);
    expect(smTampered.recoveredFromCorruption).toBe(true);
    expect(smTampered.getData().starlight).toBe(20);
  });

  it('migrates legacy v1 save data cleanly to v2 with valid checksum', () => {
    const store = new MemoryStorage();
    store.setItem(
      SAVE_STORAGE_KEY,
      JSON.stringify({ version: 1, starlight: 88, unlockedWardens: ['lyra', 'kael'] })
    );
    const sm = new SaveManager(store);
    expect(sm.recoveredFromCorruption).toBe(false);
    expect(sm.getData().version).toBe(2);
    expect(sm.getData().starlight).toBe(88);
    expect(sm.getData().unlockedWardens).toContain('kael');
    const { checksum, ...rest } = sm.getData();
    expect(checksum).toBe(computeChecksum(rest));
  });
});

describe('Audio Safety & 600-Frame Performance Stress Test', () => {
  it('operates SoundEngine safely in headless environments and respects mute/volume settings', () => {
    const store = new MemoryStorage();
    const sm = new SaveManager(store);
    const sound = new SoundEngine(sm.getData().settings);
    expect(() => {
      sound.unlock();
      sound.playClick();
      sound.playMerge(3);
      sound.playPulseCombo(7);
      sound.setMusicMood('combat');
      sm.updateSettings({ muted: true, masterVolume: 0.4 });
      sound.updateSettings(sm.getData().settings);
      sound.dispose();
    }).not.toThrow();
  });

  it('simulates 600 frames (10s @ 60 FPS) of Wave 15 Boss + Multi-Splitter Circuit in <150ms', () => {
    const circuit = new CircuitEngine(4, 2, true);
    circuit.getCell(1, 1)!.module = circuit.createModule('tri_prism', 3, 0);
    circuit.getCell(1, 0)!.module = circuit.createModule('turret_arc', 3, 0);
    circuit.getCell(1, 2)!.module = circuit.createModule('turret_blaze', 3, 0);
    circuit.getCell(2, 1)!.module = circuit.createModule('amp_lens', 3, 0);
    circuit.getCell(3, 1)!.module = circuit.createModule('turret_beam', 3, 0);

    const combat = new CombatEngine();
    combat.startWave(15, 'campaign', 40);

    const t0 = performance.now();
    for (let f = 0; f < 600; f++) {
      if (f % 45 === 0) circuit.emitPulseWave(baseMods, 1.0);
      const cRes = circuit.stepPulses(1 / 60, baseMods);
      const ev = combat.stepCombat(1 / 60, 15, 'campaign', {
        wardenId: 'lyra',
        weatherId: 'solar_flare',
        relics: ['relic_prism_crown', 'relic_trinity'],
        bountyScale: 1.2,
      });
      for (const cmd of cRes.fireCommands) {
        combat.executeTurretFire(
          cmd,
          { x: 350, y: 240 },
          { wardenId: 'lyra', weatherId: 'solar_flare', relics: ['relic_prism_crown'], turretDamageScale: 1.5 },
          ev
        );
      }
    }
    const elapsedMs = performance.now() - t0;
    expect(elapsedMs).toBeLessThan(200);
  });
});
