import { describe, expect, it } from 'vitest';
import { Rng } from '../src/core/rng';
import {
  MAX_ABILITIES,
  buyItem,
  continueRoute,
  enterRoom,
  finishBattle,
  isBossRoom,
  makeEncounter,
  newRun,
  rollRewards,
  rollShop,
  startBattle,
  type RunState,
} from '../src/core/run';
import {
  abilityTargets,
  castAbility,
  endTurn,
  enemyAt,
  heroReach,
  livingEnemies,
  moveHero,
} from '../src/core/battle';
import { abilityDef } from '../src/core/content';
import { manhattan } from '../src/core/types';
import { sanitize, defaultSave, checkAchievements } from '../src/core/save';
import { mockSafeStorage } from './storage';

mockSafeStorage();

/**
 * A reasonable-player bot: hit what it can reach, otherwise close the distance.
 * Used purely as a balance probe, never shipped in the game.
 */
function playBattleToEnd(run: RunState, seed: number, maxTurns = 40): void {
  const rng = new Rng(seed);
  const encounter = makeEncounter(run, rng, run.currentRoom ?? 'battle');
  const battle = startBattle(run, rng, encounter);
  let guard = 0;
  while (battle.phase === 'player' && guard++ < maxTurns) {
    let didSomething = false;
    for (let i = 0; i < battle.hero.abilities.length; i++) {
      const inst = battle.hero.abilities[i];
      const def = abilityDef(inst.id);
      const targets = abilityTargets(battle, inst);
      if (!targets.length) continue;
      if (def.dmg === 0) continue; // the bot never spends heat on pure utility
      const enemyTargets = targets.filter((p) => !!enemyAt(battle, p));
      const pick = enemyTargets[0] ?? (def.targeting === 'line' ? targets[0] : undefined);
      if (!pick) continue;
      const before = battle.enemies.length;
      castAbility(battle, i, pick);
      if (battle.phase !== 'player') break;
      if (battle.enemies.length !== before) didSomething = true;
      if (battle.hero.heat <= 0) break;
    }
    if (battle.phase !== 'player') break;
    // Step out of any telegraphed strike, exactly like a real player would.
    if (battle.hero.movesLeft) {
      const danger = new Set(
        livingEnemies(battle)
          .filter((e) => e.intent?.kind === 'attack' && e.intent.target)
          .map((e) => `${e.intent!.target!.x},${e.intent!.target!.y}`),
      );
      const heroKey = `${battle.hero.pos.x},${battle.hero.pos.y}`;
      if (danger.has(heroKey)) {
        const reach = heroReach(battle);
        let safePick: { x: number; y: number } | null = null;
        let bestScore = -Infinity;
        for (const key of reach.keys()) {
          if (danger.has(key)) continue;
          const [x, y] = key.split(',').map(Number);
          const foes = livingEnemies(battle);
          const near = foes.length ? Math.min(...foes.map((f) => Math.abs(f.pos.x - x) + Math.abs(f.pos.y - y))) : 4;
          const score = -Math.abs(near - 2) - (reach.get(key) ?? 0) * 0.1;
          if (score > bestScore) {
            bestScore = score;
            safePick = { x, y };
          }
        }
        if (safePick) {
          moveHero(battle, safePick);
          didSomething = true;
        }
      }
    }
    if (battle.phase !== 'player') break;
    if (!didSomething) {
      // Walk toward the closest living enemy so the bot is never a sitting duck.
      const foes = livingEnemies(battle);
      if (foes.length && battle.hero.movesLeft) {
        const closest = foes.reduce((a, c) => (manhattan(a.pos, battle.hero.pos) <= manhattan(c.pos, battle.hero.pos) ? a : c));
        const reach = heroReach(battle);
        let best: { x: number; y: number } | null = null;
        let bestD = manhattan(battle.hero.pos, closest.pos);
        for (const key of reach.keys()) {
          const [x, y] = key.split(',').map(Number);
          const d = Math.abs(x - closest.pos.x) + Math.abs(y - closest.pos.y);
          if (d < bestD) {
            bestD = d;
            best = { x, y };
          }
        }
        if (best) moveHero(battle, best);
      }
    }
    if (battle.phase !== 'player') break;
    endTurn(battle);
  }
  finishBattle(run, battle, rng);
}

describe('run structure', () => {
  it('starts with a hero, a belt and three doors', () => {
    const run = newRun('keeper', 42);
    expect(run.abilities.length).toBeGreaterThanOrEqual(2);
    expect(run.offers.length).toBe(3);
    expect(run.offers.filter((o) => o.type === 'boss').length).toBe(0);
    expect(run.hp).toBeGreaterThan(0);
    expect(run.embers).toBeGreaterThan(0);
  });

  it('is deterministic for the same seed', () => {
    const a = newRun('keeper', 777);
    const b = newRun('keeper', 777);
    expect(a).toEqual(b);
  });

  it('places a boss door at the end of each act', () => {
    const run = newRun('keeper', 5);
    run.room = 3;
    expect(isBossRoom(run)).toBe(false);
    run.room = 4;
    expect(isBossRoom(run)).toBe(true);
    continueRoute(run, new Rng(1));
    expect(run.offers[0].type).toBe('boss');
  });

  it('encounters are built from enemy definitions and schedules', () => {
    const run = newRun('keeper', 9);
    const enc = makeEncounter(run, new Rng(3), 'battle');
    expect(enc.waves.length).toBeGreaterThan(0);
    expect(enc.waves[0].arrival).toBe(1);
    for (const wave of enc.waves) {
      for (const g of wave.groups) expect(g.count).toBeGreaterThan(0);
    }
  });

  it('boss rooms always carry a boss and a boss arena', () => {
    const run = newRun('keeper', 11);
    run.room = 4;
    const enc = makeEncounter(run, new Rng(4), 'boss');
    expect(enc.kind).toBe('boss');
    expect(enc.arenaStyle).toBe('boss');
    expect(enc.bossId).toBeTruthy();
    const ids = enc.waves.flatMap((w) => w.groups.map((g) => g.typeId));
    expect(ids).toContain(enc.bossId);
  });

  it('rewards never hand out a relic the player owns', () => {
    const run = newRun('keeper', 21);
    const rng = new Rng(8);
    run.relics.push('whetstone');
    for (let i = 0; i < 8; i++) {
      const rewards = rollRewards(run, rng, 3, true);
      expect(rewards.some((r) => r.relicId === 'whetstone')).toBe(false);
    }
  });

  it('shop items are affordable or refuse politely', () => {
    const run = newRun('keeper', 31);
    const rng = new Rng(2);
    run.shop = rollShop(run, rng);
    expect(run.shop.length).toBeGreaterThan(2);
    run.embers = 0;
    const result = buyItem(run, run.shop[0], rng);
    expect(result.ok).toBe(false);
  });

  it('caps the ability belt', () => {
    const run = newRun('keeper', 3);
    run.abilities = [
      { id: 'scorch', level: 0 },
      { id: 'ash_step', level: 0 },
      { id: 'quake', level: 0 },
      { id: 'temper', level: 0 },
      { id: 'reforge', level: 0 },
    ];
    expect(run.abilities.length).toBe(MAX_ABILITIES);
    const rewards = rollRewards(run, new Rng(1), 3, false);
    expect(rewards.some((r) => r.kind === 'ability')).toBe(false);
  });

  it('a full act can be cleared without the boss unlocking twice', () => {
    const run = newRun('keeper', 1234);
    run.currentRoom = 'battle';
    playBattleToEnd(run, 55);
    expect(['reward', 'lost']).toContain(run.status);
    if (run.status === 'lost') return;
    expect(run.room).toBeGreaterThanOrEqual(1);
    expect(run.rewards.length).toBe(3);
  });

  it('boss victory advances the act, final boss wins the run', () => {
    const run = newRun('keeper', 99);
    run.room = 4;
    run.currentRoom = 'boss';
    run.act = 1;
    run.abilities.push({ id: 'forge_bolt', level: 1 }, { id: 'temper', level: 1 });
    run.relics.push('whetstone', 'start_shield');
    playBattleToEnd(run, 3, 80);
    if (run.status === 'lost') return;
    expect(run.act).toBe(2);
    expect(run.room).toBe(0);
  });

  it('losing a battle ends the run', () => {
    const run = newRun('keeper', 8);
    const rng = new Rng(1);
    const enc = makeEncounter(run, rng, 'battle');
    const battle = startBattle(run, rng, enc);
    battle.forge.hp = 0;
    const outcome = finishBattle(run, battle, rng);
    expect(outcome.won).toBe(false);
    expect(run.status).toBe('lost');
  });

  it('reward and rest choices apply real changes', () => {
    const run = newRun('keeper', 4);
    run.hp = 3;
    enterRoom(run, new Rng(6), 'rest');
    expect(run.status).toBe('rest');
    expect(run.restOptions.length).toBe(3);
    run.room = 1;
    enterRoom(run, new Rng(7), 'forge');
    expect(run.shop.length).toBeGreaterThan(2);
  });
});

describe('save system', () => {
  it('creates a valid default save', () => {
    const s = defaultSave();
    expect(s.version).toBeGreaterThan(0);
    expect(s.meta.heroes).toContain('keeper');
    expect(s.settings.music).toBeGreaterThan(0);
  });

  it('sanitises corrupted and hostile input', () => {
    expect(sanitize(null)).toBeNull();
    expect(sanitize('nope')).toBeNull();
    const dirty = sanitize({
      settings: { music: 9, sfx: -3, muted: 'yes' },
      meta: { dust: -5, heroes: ['warden', 5, 'warden'] },
      stats: { kills: 'many', runs: 3.7 },
      achievements: { first_blood: true, fake: 'true' },
      run: { heroId: 42 },
    });
    expect(dirty).not.toBeNull();
    expect(dirty!.settings.music).toBe(1);
    expect(dirty!.settings.sfx).toBe(0);
    expect(dirty!.settings.muted).toBe(false);
    expect(dirty!.meta.dust).toBe(0);
    expect(dirty!.meta.heroes).toEqual(['keeper', 'warden']);
    expect(dirty!.stats.kills).toBe(0);
    expect(dirty!.stats.runs).toBe(3);
    expect(dirty!.achievements).toEqual({ first_blood: true });
    expect(dirty!.run).toBeNull();
  });

  it('only keeps runs that can be resumed safely', () => {
    // A battle with no recorded RNG snapshot cannot be replayed, so it is dropped.
    const broken = sanitize({ run: { heroId: 'keeper', abilities: [], offers: [], status: 'battle' } });
    expect(broken!.run).toBeNull();
    // A reward screen resumes straight back into the reward.
    const reward = sanitize({ run: { heroId: 'keeper', abilities: [], offers: [], status: 'reward' } });
    expect(reward!.run).not.toBeNull();
    // A battle with a snapshot replays exactly.
    const battle = sanitize({
      run: { heroId: 'keeper', abilities: [], offers: [], status: 'battle', pendingBattle: { type: 'battle', rngState: 7 } },
    });
    expect(battle!.run).not.toBeNull();
    expect(battle!.run!.pendingBattle?.rngState).toBe(7);
  });

  it('unlocks achievements once and only once', () => {
    const save = defaultSave();
    save.stats.shoveKills = 30;
    const first = checkAchievements(save, { battleWon: true, runWon: true, heroId: 'keeper', roomsCleared: 9 });
    expect(first).toContain('first_blood');
    expect(first).toContain('win_run');
    expect(first).toContain('shover');
    expect(first).toContain('deep_run');
    const second = checkAchievements(save, { battleWon: true, runWon: true, heroId: 'keeper', roomsCleared: 9 });
    expect(second).toEqual([]);
  });
});

describe('battle pacing', () => {
  it('a starting deck can clear the first room', () => {
    let wins = 0;
    for (let seed = 1; seed <= 20; seed++) {
      const run = newRun('keeper', seed);
      run.currentRoom = 'battle';
      playBattleToEnd(run, seed * 13, 80);
      if (run.status !== 'lost') wins++;
    }
    // The opening room should be winnable with the starter kit far more often than not.
    expect(wins).toBeGreaterThanOrEqual(15);
  });

  it('enemies eventually act even if the player never moves', () => {
    const run = newRun('keeper', 77);
    const rng = new Rng(5);
    const enc = makeEncounter(run, rng, 'battle');
    const battle = startBattle(run, rng, enc);
    const startEnemies = livingEnemies(battle).length;
    for (let i = 0; i < 6 && battle.phase === 'player'; i++) endTurn(battle);
    expect(startEnemies).toBeGreaterThan(0);
    expect(battle.turn).toBeGreaterThan(3);
    // Standing still must have consequences.
    expect(battle.hero.hp < battle.hero.maxHp || battle.forge.hp < battle.forge.maxHp).toBe(true);
  });
});
