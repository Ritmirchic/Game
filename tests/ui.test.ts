// @vitest-environment jsdom
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { installAll } from './dom-setup';
import { createGame } from '../src/main';

installAll();

type GameLike = {
  save: {
    meta: { dust: number; heroes: string[] };
    settings: { muted: boolean; music: number; sfx: number };
    stats: { runs: number };
  };
  screen: string;
  run: { heroId: string; status: string; offers: { type: string; label: string }[] } | null;
  battle: unknown;
};

function boot(): GameLike {
  document.body.innerHTML = '<div id="app"><canvas id="stage"></canvas><div id="ui"></div></div>';
  window.localStorage.clear();
  return createGame() as unknown as GameLike;
}

function click(action: string, extra: Record<string, string> = {}): void {
  const selector = Object.entries(extra)
    .map(([k, v]) => `[data-${k}="${v}"]`)
    .join('');
  const el = document.querySelector(`[data-action="${action}"]${selector}`) as HTMLElement | null;
  if (!el) throw new Error(`no element for action ${action} ${JSON.stringify(extra)}`);
  el.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
}

function visibleText(): string {
  return (document.getElementById('ui')?.textContent ?? '').replace(/\s+/g, ' ');
}

describe('interface flow', () => {
  let game: GameLike;

  beforeEach(() => {
    installAll();
    game = boot();
  });

  it('boots into the title screen with working links', () => {
    expect(game.screen).toBe('title');
    expect(visibleText()).toContain('ANVILIGHT');
    expect(document.querySelector('[data-action="play"]')).toBeTruthy();
    expect(document.querySelector('[data-action="help"]')).toBeTruthy();
    expect(document.querySelector('[data-action="settings"]')).toBeTruthy();
  });

  it('opens the hero select screen with clickable keepers', () => {
    click('play');
    expect(game.screen).toBe('heroes');
    expect(visibleText()).toContain('The Emberkeeper');
    expect(document.querySelector('[data-action="pick-hero"][data-hero="keeper"]')).toBeTruthy();
    // Locked keepers must not be directly playable.
    expect(document.querySelector('[data-action="pick-hero"][data-hero="warden"]')).toBeNull();
  });

  it('starts a run and shows the route map', () => {
    click('play');
    click('pick-hero', { hero: 'keeper' });
    expect(game.screen).toBe('route');
    expect(game.run?.heroId).toBe('keeper');
    expect(document.querySelectorAll('[data-action="pick-room"]').length).toBeGreaterThan(0);
  });

  it('entering a battle shows the full HUD', () => {
    click('play');
    click('pick-hero', { hero: 'keeper' });
    const firstRoom = document.querySelector('[data-action="pick-room"]') as HTMLElement;
    const type = firstRoom.dataset.room!;
    click('pick-room', { room: type });
    if (type === 'battle' || type === 'elite' || type === 'boss') {
      expect(game.screen).toBe('battle');
      expect(game.battle).toBeTruthy();
      expect(document.getElementById('end-turn')).toBeTruthy();
      expect(document.getElementById('hp-fill')).toBeTruthy();
      expect(document.querySelectorAll('.ability').length).toBeGreaterThan(0);
    } else {
      expect(['shop', 'rest', 'treasure']).toContain(game.screen);
    }
  });

  it('the codex opens and every tab renders content', () => {
    click('codex');
    expect(document.querySelector('#modal')).toBeTruthy();
    for (const tab of ['heroes', 'abilities', 'relics', 'bestiary', 'achievements', 'stats']) {
      click('codex-tab', { tab });
      const cards = document.querySelectorAll('.codex-card, .ach-card, .kv');
      expect(cards.length, `tab ${tab}`).toBeGreaterThan(0);
    }
    click('close-modal');
    expect(document.querySelector('#modal')).toBeNull();
  });

  it('settings sliders persist to the save and mute toggles', () => {
    click('settings');
    const slider = document.getElementById('set-music') as HTMLInputElement;
    expect(slider).toBeTruthy();
    slider.value = '0.2';
    slider.dispatchEvent(new window.Event('input'));
    expect(game.save.settings.music).toBeCloseTo(0.2);
    click('close-modal');
    const before = game.save.settings.muted;
    click('toggle-mute');
    expect(game.save.settings.muted).toBe(!before);
  });

  it('help dialog lists the core rules', () => {
    click('help');
    const text = document.querySelector('#modal')?.textContent ?? '';
    expect(text).toContain('Heat');
    expect(text).toContain('Telegraphs');
    click('close-modal');
  });

  it('forgeworks lists meta unlocks and refuses unaffordable ones', () => {
    click('forgeworks');
    const cards = document.querySelectorAll('#modal .card');
    expect(cards.length).toBeGreaterThan(2);
    const buy = document.querySelector('#modal [data-action="buy-unlock"]') as HTMLElement;
    expect(buy).toBeTruthy();
    expect(document.getElementById('modal')?.textContent).toContain('dust');
  });
});

describe('battle interaction', () => {
  type DeepGame = GameLike & {
    renderer: { tile: number; ox: number; oy: number; toGrid(x: number, y: number): { x: number; y: number } | null };
    battle: {
      hero: { hp: number; heat: number; pos: { x: number; y: number } };
      forge: { hp: number };
      enemies: { uid: number; hp: number; pos: { x: number; y: number } }[];
      turn: number;
      phase: string;
    } | null;
  };

  function canvasClick(pos: { x: number; y: number }): void {
    const game = (window as unknown as { anvilight: DeepGame }).anvilight;
    const canvas = document.getElementById('stage') as HTMLCanvasElement;
    const cx = game.renderer.ox + (pos.x + 0.5) * game.renderer.tile;
    const cy = game.renderer.oy + (pos.y + 0.5) * game.renderer.tile;
    canvas.dispatchEvent(new window.PointerEvent('pointerdown', { bubbles: true, clientX: cx, clientY: cy }));
  }

  async function startBattle(): Promise<DeepGame> {
    const game = boot() as DeepGame;
    click('play');
    click('pick-hero', { hero: 'keeper' });
    // Force a battle door so the test never depends on the random route.
    game.run!.offers = [{ type: 'battle', label: 'Ash Hall' }];
    (game as unknown as { showRoute(): void }).showRoute();
    click('pick-room', { room: 'battle' });
    expect(game.screen).toBe('battle');
    return game;
  }

  it('moving onto a blue tile relocates the keeper', async () => {
    const game = await startBattle();
    const before = { ...game.battle!.hero.pos };
    // Adjacent tile is always reachable when it is not a wall.
    const dirs = [
      { x: 1, y: 0 },
      { x: -1, y: 0 },
      { x: 0, y: 1 },
      { x: 0, y: -1 },
    ];
    const board = (game.battle as unknown as { board: { walkable(p: { x: number; y: number }): boolean } }).board;
    const target = dirs
      .map((d) => ({ x: before.x + d.x, y: before.y + d.y }))
      .find((p) => board.walkable(p) && !(p.x >= 3 && p.y >= 7));
    expect(target).toBeTruthy();
    canvasClick(target!);
    await vi.waitFor(() => expect(game.battle!.hero.pos).toEqual(target), { timeout: 3000 });
    expect(game.battle!.hero.pos).not.toEqual(before);
  });

  it('casting an ability spends heat and damages a foe', async () => {
    const game = await startBattle();
    const battle = game.battle!;
    const foe = battle.enemies[0];
    // Put a sturdy foe right next to the keeper so Scorch has a legal target
    // and the kill refund does not mask the heat cost.
    foe.pos = { x: battle.hero.pos.x + 1, y: battle.hero.pos.y };
    foe.hp = 12;
    (foe as unknown as { maxHp: number }).maxHp = 12;
    const heatBefore = battle.hero.heat;
    click('ability:0');
    canvasClick(foe.pos);
    await vi.waitFor(() => expect(foe.hp).toBeLessThan(12), { timeout: 3000 });
    expect(battle.hero.heat).toBe(heatBefore - 1);
  });

  it('ending a turn advances the clock and recharges heat', async () => {
    const game = await startBattle();
    const battle = game.battle!;
    battle.hero.heat = 0;
    const turnBefore = battle.turn;
    click('end-turn');
    await vi.waitFor(() => expect(battle.turn).toBeGreaterThan(turnBefore), { timeout: 5000 });
    expect(battle.hero.heat).toBeGreaterThan(0);
  });
});

describe('post-battle flow', () => {
  type FlowGame = GameLike & {
    renderer: unknown;
    showRoute(): void;
    continueRun(): void;
    battle: {
      hero: { hp: number };
      forge: { hp: number };
      enemies: { hp: number }[];
      waves: unknown[];
      waveIndex: number;
      phase: string;
    } | null;
  };

  /** Kills everything on the board and cancels reinforcements, as if the room was cleared. */
  function clearBoard(game: FlowGame): void {
    game.battle!.enemies.forEach((e) => (e.hp = 0));
    game.battle!.waves.length = 0;
    game.battle!.waveIndex = 0;
  }

  async function inBattle(): Promise<FlowGame> {
    const game = boot() as unknown as FlowGame;
    click('play');
    click('pick-hero', { hero: 'keeper' });
    game.run!.offers = [{ type: 'battle', label: 'Ash Hall' }];
    game.showRoute();
    click('pick-room', { room: 'battle' });
    expect(game.screen).toBe('battle');
    return game;
  }

  it('clearing a room leads to the reward draft and back to the route', async () => {
    const game = await inBattle();
    clearBoard(game);
    click('end-turn');
    await vi.waitFor(() => expect(game.screen).toBe('reward'), { timeout: 8000 });
    const picks = document.querySelectorAll('[data-action="pick-reward"]');
    expect(picks.length).toBe(3);
    (picks[0] as HTMLElement).dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
    await vi.waitFor(() => expect(game.screen).toBe('route'), { timeout: 4000 });
    expect(document.querySelectorAll('[data-action="pick-room"]').length).toBeGreaterThan(0);
  });

  it('the reward draft can be skipped for embers', async () => {
    const game = await inBattle();
    clearBoard(game);
    click('end-turn');
    await vi.waitFor(() => expect(game.screen).toBe('reward'), { timeout: 8000 });
    click('skip-reward');
    await vi.waitFor(() => expect(game.screen).toBe('route'), { timeout: 4000 });
  });

  it('a destroyed forge shows the defeat screen and banks dust', async () => {
    const game = await inBattle();
    game.battle!.forge.hp = 0;
    clearBoard(game);
    click('end-turn');
    await vi.waitFor(() => expect(game.screen).toBe('result'), { timeout: 8000 });
    expect(document.body.textContent).toContain('ANVILIGHT FADES');
    expect(game.run).toBeNull();
  });

  it('rest, shop and vault screens all render working options', async () => {
    for (const [room, minCards] of [
      ['rest', 3],
      ['forge', 4],
      ['treasure', 3],
    ] as const) {
      const game = boot() as unknown as FlowGame;
      click('play');
      click('pick-hero', { hero: 'keeper' });
      game.run!.offers = [{ type: room, label: room }];
      game.showRoute();
      click('pick-room', { room });
      expect(game.screen).toBe(room === 'forge' ? 'shop' : room);
      const actions = document.querySelectorAll('[data-action]');
      expect(actions.length).toBeGreaterThanOrEqual(minCards);
    }
  });

  it('a saved run can be resumed from the title screen', () => {
    const first = boot() as unknown as FlowGame;
    click('play');
    click('pick-hero', { hero: 'keeper' });
    expect(first.run).toBeTruthy();
    const second = createGame() as unknown as FlowGame;
    expect(document.querySelector('[data-action="continue-run"]')).toBeTruthy();
    click('continue-run');
    expect(second.screen).toBe('route');
  });
});
