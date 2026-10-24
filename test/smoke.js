/* ═══════════════════════════════════════════════════
   Headless smoke-тест СИНГУЛЯРНОСТИ:  node test/smoke.js
   Проходит весь жизненный цикл: меню → старт → еда →
   волна → импульс → слияние → смерть/победа → рестарт.
   ═══════════════════════════════════════════════════ */
'use strict';
const { step, G, assert, noNaN, frames } = require('./smoke_lib.js');

/* 1. attract-режим меню */
step(120);
assert(G.G.state === 'menu', 'menu state');
G.holes().forEach(h=>noNaN(h, 'menuHole'));

/* 2. старт игры */
G.start();
assert(G.G.state === 'play', 'play state');
step(180);
noNaN(G.player(), 'player early');
assert(G.player().M > 50, 'player ate something: M=' + G.player().M);

/* 3. движение к цели */
G.setTarget(G.player().x + 300, G.player().y + 100);
step(120);
noNaN(G.player(), 'player moving');

/* 4. первая волна должна была прийти */
assert(G.G.wave >= 1, 'wave started: ' + G.G.wave);
const rival = G.holes().find(h=>h.controller === 'ai');
assert(rival, 'rival exists');
noNaN(rival, 'rival');

/* 5. импульс */
const mBefore = G.player().M;
G.pulse();
if(mBefore >= 58) assert(G.player().M < mBefore, 'pulse costs mass');

/* 6. форсируем слияние (телепортируем соперницу к игроку) */
step(60);
const r2 = G.holes().find(h=>h.controller === 'ai');
if(r2){
  if(G.player().M >= r2.M){
    r2.x = G.player().x + 5; r2.y = G.player().y;
    step(10);
    assert(G.G.stats.merges >= 1 || G.G.state !== 'play', 'merge won or something happened');
  }else{
    r2.x = G.player().x + 5; r2.y = G.player().y;
    step(180);
    assert(G.G.state === 'over' || G.G.state === 'dying', 'death path: ' + G.G.state);
  }
}

/* 7. ещё немного жизни после событий */
if(G.G.state === 'play') step(240);
if(G.player()) noNaN(G.player(), 'player late');

console.log('PRE-RESTART — state:', G.G.state, '| волна:', G.G.wave,
  '| съедено:', G.G.stats.mass.toFixed(1), '| слияний:', G.G.stats.merges,
  '| счёт:', Math.round(G.G.score));

/* 8. рестарт */
G.start();
assert(G.G.state === 'play', 'restart works');
step(60);
noNaN(G.player(), 'player after restart');

console.log('SMOKE OK — frames:', frames());
