/**
 * Offline render harness: draws the game's canvas art to PNG without a browser.
 * Run with:  npx vite-node tools/shots.ts
 */
import { createCanvas } from '@napi-rs/canvas';
import { writeFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';

const W = 1180;
const H = 820;

const g = globalThis as unknown as Record<string, unknown>;
g.window = g;
g.devicePixelRatio = 1;
g.requestAnimationFrame = (cb: (t: number) => void) => setTimeout(() => cb(performance.now()), 16);
g.document = {
  createElement(tag: string) {
    if (tag !== 'canvas') return {};
    return makeCanvas(16, 16);
  },
};

function makeCanvas(w: number, h: number) {
  const c = createCanvas(w, h) as unknown as HTMLCanvasElement & { toBuffer: (m: string) => Buffer };
  (c as unknown as Record<string, unknown>).getBoundingClientRect = () => ({ left: 0, top: 0, width: c.width, height: c.height, right: c.width, bottom: c.height, x: 0, y: 0 });
  return c;
}

const { Renderer } = await import('../src/render');
const { newRun, makeEncounter, startBattle, runRng } = await import('../src/core/run');
const { castAbility, computeIntents, endTurn } = await import('../src/core/battle');
const { vec } = await import('../src/core/types');

const canvas = makeCanvas(W, H);
const renderer = new Renderer(canvas as unknown as HTMLCanvasElement);
renderer.setInsets(20, 20);
renderer.resize();

mkdirSync(join(process.cwd(), 'shots'), { recursive: true });

let clock = 1000;
function frames(n: number, step = 60): void {
  for (let i = 0; i < n; i++) {
    clock += step;
    renderer.frame(clock);
  }
}

function save(name: string): void {
  const buf = (canvas as unknown as { toBuffer: (m: string) => Buffer }).toBuffer('image/png');
  writeFileSync(join(process.cwd(), 'shots', name), buf);
  console.log('wrote shots/' + name);
}

// 1. Title / ambient art
renderer.setBattle(null);
frames(40);
save('01-title.png');

// 2. A mid-run battle
const run = newRun('keeper', 20261025);
run.room = 2;
const rng = runRng(run);
run.currentRoom = 'battle';
const enc = makeEncounter(run, rng, 'battle');
const battle = startBattle(run, rng, enc);
renderer.setBattle(battle);
renderer.setInsets(24, 24);
renderer.resize();
frames(30);
save('02-battle-start.png');

// 3. Ability targeting + a couple of turns in
renderer.selectedAbility = 0;
const targets = (await import('../src/core/battle')).abilityTargets(battle, battle.hero.abilities[0]);
renderer.highlights = targets;
renderer.highlightKind = 'ability';
renderer.hover = targets[0] ?? null;
frames(6);
save('03-targeting.png');

// 4. Aftermath: some hits, a shove, a kill
if (targets.length) {
  const evs = castAbility(battle, 0, targets[0]);
  void renderer.playEvents(evs);
  frames(30, 120);
}
const evs2 = endTurn(battle);
void renderer.playEvents(evs2);
frames(60, 120);
computeIntents(battle);
frames(30);
save('04-enemy-phase.png');

// 5. Boss arena
run.act = 2;
run.room = 4;
const rng2 = runRng(run);
run.currentRoom = 'boss';
const enc2 = makeEncounter(run, rng2, 'boss');
const bossBattle = startBattle(run, rng2, enc2);
renderer.setBattle(bossBattle);
frames(30);
save('05-boss.png');

// 6. Rich ability bar state
const run3 = newRun('warden', 777);
run3.relics.push('whetstone', 'momentum', 'start_shield');
run3.abilities.push({ id: 'slag_bomb', level: 1 }, { id: 'bellows', level: 0 }, { id: 'temper', level: 0 });
run3.room = 3;
const rng3 = runRng(run3);
run3.currentRoom = 'elite';
const enc3 = makeEncounter(run3, rng3, 'elite');
const b3 = startBattle(run3, rng3, enc3);
renderer.setBattle(b3);
renderer.selectedAbility = 2;
const t3 = (await import('../src/core/battle')).abilityTargets(b3, b3.hero.abilities[2]);
renderer.highlights = t3;
renderer.highlightKind = 'ability';
frames(40);
save('06-elite.png');

console.log('done');
process.exit(0);
