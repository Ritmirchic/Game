/* Тесты чистой логики: node tests/gen.test.js */
'use strict';
const assert = require('assert');
const L = require('../src/logic.js');

const seeds = [];
for (let i = 1; i <= 60; i++) seeds.push(i * 7919);
seeds.push(12345, 999983, 42);

let checked = 0;
const stats = { portals: 0, qdoors: 0, angels: 0, maxDist: 0, rooms: 0 };

for (const seed of seeds) {
  const w = L.generateWorld(seed);
  checked++;

  assert.strictEqual(w.grid.length, L.MAP_W * L.MAP_H, 'размер сетки');
  assert.strictEqual(w.fragments.length, 5, 'ровно 5 осколков: ' + seed);
  assert.ok(w.portals.length >= 4, 'порталов >= 4: ' + seed);
  assert.ok(w.qdoors.length >= 2, 'квантовых дверей >= 2: ' + seed);
  assert.ok(w.angels.length >= 1, 'наблюдателей >= 1: ' + seed);
  assert.ok(w.exit, 'выход есть');
  assert.strictEqual(w.grid[L.I(w.exit.x, w.exit.y)], L.T.EXIT, 'выход стоит в сетке');
  {   // выход — настоящая дверь в коридоре: пол с двух противоположных сторон
    const ex = w.exit;
    const g = (x, y) => w.grid[L.I(x, y)];
    const horiz = g(ex.x - 1, ex.y) === L.T.EMPTY && g(ex.x + 1, ex.y) === L.T.EMPTY;
    const vert = g(ex.x, ex.y - 1) === L.T.EMPTY && g(ex.x, ex.y + 1) === L.T.EMPTY;
    assert.ok(horiz || vert, 'выход стоит в проходе');
    assert.strictEqual(w.exit.horiz, horiz, 'ориентация выхода совпадает');
  }

  // границы карты — всегда стена
  for (let x = 0; x < L.MAP_W; x++) {
    assert.ok(L.isWall(w.grid[L.I(x, 0)]), 'верхняя граница стена');
    assert.ok(L.isWall(w.grid[L.I(x, L.MAP_H - 1)]), 'нижняя граница стена');
  }
  for (let y = 0; y < L.MAP_H; y++) {
    assert.ok(L.isWall(w.grid[L.I(0, y)]), 'левая граница стена');
    assert.ok(L.isWall(w.grid[L.I(L.MAP_W - 1, y)]), 'правая граница стена');
  }

  // старт, осколки, порталы — на полу
  assert.strictEqual(w.grid[L.I(w.start.tx, w.start.ty)], L.T.EMPTY, 'старт на полу');
  for (const f of w.fragments) {
    assert.strictEqual(w.grid[L.I(f.tx, f.ty)], L.T.EMPTY, 'осколок на полу');
    assert.ok(w.reach[L.I(f.tx, f.ty)] >= 0, 'осколок достижим');
  }
  for (const p of w.portals) {
    assert.strictEqual(w.grid[L.I(p.ax, p.ay)], L.T.EMPTY, 'портал A на полу');
    assert.strictEqual(w.grid[L.I(p.bx, p.by)], L.T.EMPTY, 'портал B на полу');
    assert.ok(p.rot >= 1 && p.rot <= 3, 'поворот 90/180/270');
    assert.ok(Math.hypot(p.ax - p.bx, p.ay - p.by) > 5, 'концы разлома далеко');
    for (const q of w.portals) {
      if (q === p) continue;
      for (const u of [[p.ax, p.ay], [p.bx, p.by]])
        for (const v of [[q.ax, q.ay], [q.bx, q.by]])
          assert.ok(Math.hypot(u[0] - v[0], u[1] - v[1]) >= 4.9, 'разломы не слипаются');
    }
  }
  for (const q of w.qdoors) assert.strictEqual(w.grid[L.I(q.x, q.y)], L.T.QDOOR, 'дверь в сетке');
  for (const a of w.angels) assert.strictEqual(w.grid[L.I(a.tx, a.ty)], L.T.EMPTY, 'наблюдатель на полу');

  // никакие две сущности не стоят на одном тайле
  const occ = new Map();
  const put = (x, y, tag) => {
    const k = L.I(x, y);
    assert.ok(!occ.has(k), `тайл ${x},${y} занят дважды: ${occ.get(k)} и ${tag}`);
    occ.set(k, tag);
  };
  put(w.start.tx, w.start.ty, 'start');
  w.fragments.forEach((f, i) => put(f.tx, f.ty, 'frag' + i));
  w.portals.forEach((p, i) => { put(p.ax, p.ay, 'portalA' + i); put(p.bx, p.by, 'portalB' + i); });
  w.qdoors.forEach((q, i) => put(q.x, q.y, 'qdoor' + i));
  w.angels.forEach((a, i) => put(a.tx, a.ty, 'angel' + i));

  // квантовые двери стоят в узком коридоре
  for (const q of w.qdoors) {
    const e = w.grid[L.I(q.x + 1, q.y)] === L.T.EMPTY, we = w.grid[L.I(q.x - 1, q.y)] === L.T.EMPTY;
    const s = w.grid[L.I(q.x, q.y + 1)] === L.T.EMPTY, n = w.grid[L.I(q.x, q.y - 1)] === L.T.EMPTY;
    assert.ok((e && we && !s && !n) || (n && s && !e && !we), 'дверь — проход 1x1');
  }

  // A* находит путь от старта до каждого осколка (сквозь квантовые двери)
  for (const f of w.fragments) {
    const path = L.astar(w, w.start.tx, w.start.ty, f.tx, f.ty, { qDoors: true, maxExpand: 6000 });
    assert.ok(path.length > 1, 'A* нашёл путь до осколка ' + f.tx + ',' + f.ty);
    assert.strictEqual(path[path.length - 1], L.I(f.tx, f.ty), 'A* привёл к осколку');
    // путь непрерывен и не проходит сквозь стены
    for (let k = 1; k < path.length; k++) {
      const a = path[k - 1], b = path[k];
      const ax = a % L.MAP_W, ay = (a / L.MAP_W) | 0, bx = b % L.MAP_W, by = (b / L.MAP_W) | 0;
      assert.ok(Math.abs(ax - bx) + Math.abs(ay - by) === 1, 'шаг на соседнюю клетку');
      assert.ok(!L.isSolidCode(w.grid[b]), 'путь не проходит сквозь стену');
    }
  }
  // Наблюдатель доходит до старта, если двери ему не мешают
  {
    const a = w.angels[0];
    const p1 = L.astar(w, a.tx, a.ty, w.start.tx, w.start.ty, { maxExpand: 6000 });
    const p2 = L.astar(w, a.tx, a.ty, w.start.tx, w.start.ty, { qDoors: true, maxExpand: 6000 });
    assert.ok(p2.length > 0, 'с дверями наблюдатель путь находит всегда');
    assert.ok(p1.length === 0 || p1.length >= p2.length, 'без дверей путь не короче');
  }

  // детерминизм
  const w2 = L.generateWorld(seed);
  assert.deepStrictEqual(Array.from(w2.grid), Array.from(w.grid), 'детерминированная генерация');
  assert.deepStrictEqual(w2.portals.map(p => [p.ax, p.ay, p.bx, p.by]), w.portals.map(p => [p.ax, p.ay, p.bx, p.by]));

  stats.portals += w.portals.length;
  stats.qdoors += w.qdoors.length;
  stats.angels += w.angels.length;
  stats.rooms += w.rooms.length;
  stats.maxDist = Math.max(stats.maxDist, w.maxDist);
}

console.log(`OK: ${checked} миров сгенерировано и проверено`);
console.log(`  порталов всего: ${stats.portals} (Ø ${(stats.portals / checked).toFixed(2)})`);
console.log(`  квантовых дверей: ${stats.qdoors} (Ø ${(stats.qdoors / checked).toFixed(2)})`);
console.log(`  наблюдателей: ${stats.angels} (Ø ${(stats.angels / checked).toFixed(2)})`);
console.log(`  комнат: ${stats.rooms} (Ø ${(stats.rooms / checked).toFixed(1)})`);
console.log(`  максимальная удалённость от старта: ${stats.maxDist}`);

const demo = L.generateWorld(20261024);
console.log('\nДемо-мир (сид 20261024): @ старт, * осколок, + разлом, ◊ квантовая дверь, Ж выход, A наблюдатель\n');
console.log(L.dump(demo));
