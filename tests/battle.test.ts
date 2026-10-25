import { describe, expect, it } from 'vitest';
import { Board, vec, type Battle, type EnemyState } from '../src/core/types';
import { abilityTargets, castAbility, computeIntents, createBattle, endTurn, moveHero } from '../src/core/battle';
import { makeArena } from '../src/core/arena';
import { Rng } from '../src/core/rng';

function enemy(partial: Partial<EnemyState> & { pos: { x: number; y: number } }): EnemyState {
  const base: EnemyState = {
    uid: partial.uid ?? 1,
    typeId: partial.typeId ?? 'grunt',
    name: 'Cinder Grunt',
    pos: { x: 0, y: 0 },
    hp: partial.hp ?? 3,
    maxHp: partial.maxHp ?? partial.hp ?? 3,
    shield: 0,
    dmg: partial.dmg ?? 1,
    range: partial.range ?? 1,
    speed: partial.speed ?? 2,
    stun: 0,
    mark: 0,
    burn: 0,
    elite: false,
    boss: false,
    armored: false,
    intent: null,
    bornAt: 0,
  };
  return { ...base, ...partial, pos: { ...partial.pos } };
}

function testBattle(opts: {
  enemies?: EnemyState[];
  abilities?: { id: string; level: number }[];
  relics?: string[];
  heroPos?: { x: number; y: number };
  vents?: { x: number; y: number }[];
  walls?: { x: number; y: number }[];
  heat?: number;
  forgeHp?: number;
  heroHp?: number;
} = {}): Battle {
  const board = new Board(8, 8);
  board.set(vec(3, 7), 'forge');
  board.set(vec(4, 7), 'forge');
  for (const w of opts.walls ?? []) board.setWall(w);
  for (const v of opts.vents ?? []) board.set(v, 'vent');
  const b = createBattle({
    board,
    hero: {
      heroId: 'keeper',
      name: 'The Emberkeeper',
      pos: opts.heroPos ?? vec(3, 5),
      hp: opts.heroHp ?? 12,
      maxHp: 12,
      shield: 0,
      heat: opts.heat ?? 6,
      maxHeat: 6,
      heatRegen: 2,
      moveRange: 4,
      movesLeft: true,
      abilities: opts.abilities ?? [
        { id: 'scorch', level: 0 },
        { id: 'ash_step', level: 0 },
      ],
    },
    forge: { tiles: [vec(3, 7), vec(4, 7)], hp: opts.forgeHp ?? 18, maxHp: 18, cracked: 0 },
    relics: opts.relics ?? [],
    seed: 12345,
    waves: [],
    bossId: null,
    eliteBonus: false,
    heroPassive: 'kindling',
    roomKind: 'battle',
  });
  b.enemies = opts.enemies ?? [];
  computeIntents(b);
  return b;
}

describe('arenas', () => {
  it('are deterministic for a given seed', () => {
    const a = makeArena(new Rng(999), 'hall');
    const c = makeArena(new Rng(999), 'hall');
    expect(a.board.tiles.map((t) => t.kind)).toEqual(c.board.tiles.map((t) => t.kind));
    expect(a.heroStart).toEqual(c.heroStart);
  });

  it('always leave a walkable lane from the hero to the forge', () => {
    for (let seed = 1; seed < 60; seed++) {
      const arena = makeArena(new Rng(seed * 7919), 'hall');
      const path = arena.board.pathTo(arena.heroStart, (p) => arena.forgeTiles.some((f) => Math.abs(f.x - p.x) + Math.abs(f.y - p.y) === 1));
      expect(path, `seed ${seed}`).not.toBeNull();
    }
  });

  it('spawns heroes on a walkable tile', () => {
    for (let seed = 1; seed < 40; seed++) {
      const arena = makeArena(new Rng(seed * 31), 'foundry');
      expect(arena.board.walkable(arena.heroStart)).toBe(true);
    }
  });
});

describe('telegraphs', () => {
  it('commit enemies to a tile, so stepping aside makes them miss', () => {
    const b = testBattle({ enemies: [enemy({ pos: vec(3, 6) })] });
    const e = b.enemies[0];
    expect(e.intent?.kind).toBe('attack');
    expect(e.intent?.target).toEqual(vec(3, 5));

    const events = moveHero(b, vec(5, 5));
    expect(events.some((ev) => ev.t === 'move')).toBe(true);
    const hpBefore = b.hero.hp;
    endTurn(b);
    expect(b.hero.hp).toBe(hpBefore);
    expect(b.phase).toBe('player');
  });

  it('hits the hero who stands still', () => {
    const b = testBattle({ enemies: [enemy({ pos: vec(3, 6) })] });
    const hpBefore = b.hero.hp;
    endTurn(b);
    expect(b.hero.hp).toBe(hpBefore - 1);
  });

  it('never damages the forge through a wall', () => {
    const b = testBattle({
      enemies: [enemy({ typeId: 'spitter', range: 3, dmg: 2, pos: vec(4, 3) })],
      walls: [vec(4, 5)],
    });
    b.forge.hp = 18;
    endTurn(b);
    expect(b.forge.hp).toBe(18);
  });
});

describe('shoves and hazards', () => {
  it('shove into a wall deals impact damage', () => {
    const b = testBattle({ enemies: [enemy({ pos: vec(3, 4), hp: 6, maxHp: 6 })], walls: [vec(3, 3)] });
    castAbility(b, 0, vec(3, 4));
    expect(b.enemies[0].hp).toBe(6 - 2 - 1);
  });

  it('shove into a vent adds vent damage and can kill', () => {
    const b = testBattle({ enemies: [enemy({ pos: vec(3, 4), hp: 3 }), enemy({ uid: 2, pos: vec(5, 4), hp: 1 })], vents: [vec(3, 3), vec(5, 3)] });
    castAbility(b, 0, vec(3, 4));
    // 2 damage + 2 vent = dead
    expect(b.enemies.find((e) => e.uid === 1)).toBeUndefined();
  });

  it('momentum relic extends shoves', () => {
    const b = testBattle({ enemies: [enemy({ pos: vec(3, 4), hp: 9, maxHp: 9 })], vents: [vec(3, 2)], relics: ['momentum'] });
    castAbility(b, 0, vec(3, 4));
    expect(b.enemies[0].pos).toEqual(vec(3, 2));
    // 2 from Scorch + 2 from vent
    expect(b.enemies[0].hp).toBe(9 - 4);
  });

  it('hero takes vent damage when walking through one', () => {
    const b = testBattle({ vents: [vec(4, 5)] });
    b.hero.pos = vec(3, 5);
    moveHero(b, vec(4, 5));
    expect(b.hero.hp).toBe(10);
  });

  it('ventwalker relic ignores vents', () => {
    const b = testBattle({ vents: [vec(4, 5)], relics: ['vent_grace'] });
    moveHero(b, vec(4, 5));
    expect(b.hero.hp).toBe(12);
  });
});

describe('heat economy', () => {
  it('spends heat and refuses unaffordable casts', () => {
    const b = testBattle({ heat: 1, enemies: [enemy({ pos: vec(3, 4), hp: 9, maxHp: 9 })] });
    castAbility(b, 0, vec(3, 4));
    expect(b.hero.heat).toBe(0);
    const hp = b.enemies[0].hp;
    castAbility(b, 0, vec(3, 4));
    expect(b.enemies[0].hp).toBe(hp);
  });

  it('refunds extra heat on kill for the keeper', () => {
    const b = testBattle({ heat: 0, enemies: [enemy({ pos: vec(3, 4), hp: 1 })] });
    b.hero.abilities = [{ id: 'scorch', level: 1 }];
    // Cost 1 but heat is 0 -> cast must fail, so grant 1 heat.
    b.hero.heat = 1;
    castAbility(b, 0, vec(3, 4));
    expect(b.enemies.length).toBe(0);
    expect(b.hero.heat).toBe(2);
  });

  it('regenerates heat at the start of a new turn', () => {
    const b = testBattle({ heat: 0, enemies: [enemy({ pos: vec(7, 0), speed: 0 })] });
    endTurn(b);
    expect(b.hero.heat).toBe(2);
    expect(b.hero.movesLeft).toBe(true);
  });

  it('Echo Anvil makes the first ability free', () => {
    const b = testBattle({ heat: 0, relics: ['echo_anvil'], enemies: [enemy({ pos: vec(3, 4), hp: 9, maxHp: 9 })] });
    castAbility(b, 0, vec(3, 4));
    expect(b.enemies[0].hp).toBe(7);
    expect(b.echoUsed).toBe(true);
  });
});

describe('abilities', () => {
  it('ash step teleports and validates range', () => {
    const b = testBattle({ enemies: [] });
    expect(abilityTargets(b, b.hero.abilities[1]).some((p) => p.x === 6 && p.y === 5)).toBe(true);
    expect(abilityTargets(b, b.hero.abilities[1]).some((p) => p.x === 7 && p.y === 5)).toBe(false);
    castAbility(b, 1, vec(5, 5));
    expect(b.hero.pos).toEqual(vec(5, 5));
    expect(b.hero.heat).toBe(5);
  });

  it('cinder wall blocks movement', () => {
    const b = testBattle({ abilities: [{ id: 'cinder_wall', level: 0 }] });
    castAbility(b, 0, vec(3, 4));
    expect(b.board.at(vec(3, 4))?.kind).toBe('wall');
  });

  it('belt abilities only hit the first enemy in the line', () => {
    const b = testBattle({
      abilities: [{ id: 'ember_lance', level: 0 }],
      enemies: [enemy({ pos: vec(3, 3), hp: 9, maxHp: 9 }), enemy({ uid: 2, pos: vec(3, 2), hp: 9, maxHp: 9 })],
    });
    castAbility(b, 0, vec(3, 2));
    // Lance 3 damage + 1 collision from the shove: the second foe is only clipped.
    expect(b.enemies[0].hp).toBe(5);
    expect(b.enemies[1].hp).toBe(8);
  });

  it('bombs detonate for splash on the next enemy phase', () => {
    const b = testBattle({
      abilities: [{ id: 'slag_bomb', level: 0 }],
      enemies: [enemy({ pos: vec(3, 4), hp: 9, maxHp: 9 })],
      heat: 6,
    });
    castAbility(b, 0, vec(3, 3));
    expect(b.bombs.length).toBe(1);
    endTurn(b);
    expect(b.bombs.length).toBe(0);
    expect(b.enemies[0].hp).toBeLessThan(9);
  });
});

describe('objectives', () => {
  it('winning clears the battle', () => {
    const b = testBattle({ enemies: [enemy({ pos: vec(3, 4), hp: 1 })] });
    castAbility(b, 0, vec(3, 4));
    expect(b.phase).toBe('won');
  });

  it('a destroyed forge loses the battle', () => {
    const b = testBattle({
      forgeHp: 1,
      enemies: [enemy({ typeId: 'eater', pos: vec(3, 6), dmg: 3, hp: 7, maxHp: 7 })],
    });
    endTurn(b);
    expect(b.phase).toBe('lost');
  });

  it('reinforcements arrive on schedule', () => {
    const b = testBattle({ enemies: [] });
    b.waves = [{ groups: [{ typeId: 'grunt', count: 2 }], arrival: 2 }];
    b.waveIndex = 0;
    endTurn(b);
    expect(b.enemies.length).toBe(2);
    expect(b.waveIndex).toBe(1);
  });
});
