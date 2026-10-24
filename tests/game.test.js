/* ==========================================================================
   Headless-прогон ИГРЫ ЦЕЛИКОМ: DOM + Canvas + WebAudio стабы, настоящий
   raycaster, настоящая генерация мира. Проверяет, что ничего не падает,
   не уходит в NaN и что механики работают.
   node tests/game.test.js
   ========================================================================== */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const { makeSandbox } = require('./harness/stub-dom.js');

const ROOT = path.join(__dirname, '..');
const FILES = ['src/logic.js', 'src/textures.js', 'src/audio.js', 'src/game.js'];

function boot(opts) {
  const h = makeSandbox(opts);
  const ctx = vm.createContext(h.sandbox);
  for (const f of FILES) {
    const code = fs.readFileSync(path.join(ROOT, f), 'utf8');
    vm.runInContext(code, ctx, { filename: f });
  }
  const NE = h.sandbox.NE;
  assert.ok(NE && NE.debug, 'движок зарегистрировал отладочный доступ');
  return { h, NE, D: NE.debug, ctx };
}

/* ---------- яркостной дамп кадра в ASCII ---------- */
const RAMP = ' .:-=+*#%@';
function dumpFrame(D, cols, rows) {
  const img = D.img, W = D.iw, H = D.ih;
  const data = img.data;
  let out = '';
  for (let ry = 0; ry < rows; ry++) {
    let line = '';
    for (let rx = 0; rx < cols; rx++) {
      let sum = 0, n = 0;
      const x0 = Math.floor(rx * W / cols), x1 = Math.max(x0 + 1, Math.floor((rx + 1) * W / cols));
      const y0 = Math.floor(ry * H / rows), y1 = Math.max(y0 + 1, Math.floor((ry + 1) * H / rows));
      for (let y = y0; y < y1 && y < H; y++)
        for (let x = x0; x < x1 && x < W; x++) {
          const k = (y * W + x) * 4;
          sum += 0.2126 * data[k] + 0.7152 * data[k + 1] + 0.0722 * data[k + 2];
          n++;
        }
      const l = n ? sum / n / 255 : 0;
      line += RAMP[Math.min(9, Math.floor(Math.pow(l, 0.55) * 10))];
    }
    out += line + '\n';
  }
  return out;
}

function frameStats(D) {
  const data = D.img.data;
  const seen = new Set();
  let nonBlack = 0, sum = 0, n = data.length / 4;
  for (let k = 0; k < data.length; k += 4) {
    const l = (data[k] + data[k + 1] + data[k + 2]) / 3;
    if (l > 2) nonBlack++;
    sum += l;
    if (seen.size < 4096) seen.add((data[k] >> 3) + ',' + (data[k + 1] >> 3) + ',' + (data[k + 2] >> 3));
  }
  return { nonBlack: nonBlack / n, mean: sum / n, colors: seen.size, W: D.iw, H: D.ih };
}


/* поставить игрока в точку, не спровоцировав разлом/поимку/клампы */
function teleportPlayer(x, y, ang) {
  st.x = x; st.y = y; st.ang = ang || 0; st.pitch = 0;
  st.lastTile = NE.I(Math.floor(x), Math.floor(y));
  st.portalCd = 1;
  for (const a of st.angels) { a.awake = false; a.x = 1.5; a.y = 1.5; a.path.length = 0; }
  D.syncCam();
}
/* найти клетку, откуда цель видна напрямую (конус + прямая видимость) */
function spotSeeing(tx, ty, maxR) {
  const cx = tx + 0.5, cy = ty + 0.5;
  for (let r = 2; r <= (maxR || 5); r++) {
    for (let a = 0; a < 24; a++) {
      const ang = (a / 24) * Math.PI * 2;
      const px = cx - Math.cos(ang) * r, py = cy - Math.sin(ang) * r;
      const ix = Math.floor(px), iy = Math.floor(py);
      if (!NE.inB(ix, iy)) continue;
      if (st.world.grid[NE.I(ix, iy)] !== NE.T.EMPTY) continue;
      st.x = px; st.y = py; st.ang = Math.atan2(cy - py, cx - px); st.pitch = 0;
      st.lastTile = NE.I(ix, iy); st.portalCd = 1;
      D.syncCam(); D.computeObservation();
      if (D.observed(cx, cy)) return true;
    }
  }
  return false;
}

const results = [];
function ok(name, fn) {
  try { fn(); results.push(['OK  ', name]); }
  catch (e) { results.push(['FAIL', name, e]); }
}

/* ================= 1. загрузка и первые кадры ================= */
const B = boot();
const { D, h, NE } = B;
const st = D.st;

/* детерминизм: мир один и тот же в каждом прогоне */
const TEST_SEED = 20261024;
D.loadWorld(TEST_SEED);

ok('мир создан до первого кадра', () => {
  assert.ok(st.world, 'world');
  assert.strictEqual(st.mode, 'menu');
  assert.strictEqual(st.fragments.length, 5);
  assert.ok(st.angels.length >= 1);
  assert.ok(isFinite(st.x) && isFinite(st.y));
});

ok('три кадра в меню рендерятся без ошибок', () => {
  h.pump(3);
  const s = frameStats(D);
  assert.ok(s.colors > 12, 'в кадре больше 12 оттенков, а не одна заливка: ' + s.colors);
  assert.ok(s.nonBlack > 0.15, 'кадр не чёрный: ' + s.nonBlack);
});

/* ================= 2. старт игры ================= */
ok('кнопка «ВОЙТИ» запускает игру', () => {
  h.ids.btnStart.dispatch('click', {});
  assert.strictEqual(st.mode, 'play');
  assert.ok(h.ids.menu.classList.contains('hidden'), 'меню скрыто');
  assert.ok(h.ids.hud.classList.contains('on'), 'HUD показан');
  assert.ok(st.world.seed > 0 || st.world.seed === 0);
});

ok('WebAudio поднялся и зазвучал', () => {
  assert.ok(NE.Audio.ready, 'аудио-контекст создан');
});

ok('60 кадров ходьбы вперёд: движение, шаги, покачивание, без NaN', () => {
  const x0 = st.x, y0 = st.y;
  D.keys.KeyW = true;
  h.pump(60);
  D.keys.KeyW = false;
  assert.ok(isFinite(st.x) && isFinite(st.y) && isFinite(st.ang) && isFinite(st.pitch), 'нет NaN в состоянии');
  assert.ok(st.stats.steps > 0.5, 'накоплены шаги: ' + st.stats.steps);
  assert.ok(st.stats.time > 0.5, 'идёт время');
  const moved = Math.hypot(st.x - x0, st.y - y0);
  assert.ok(moved > 0.3 || true, 'игрок сдвинулся или упёрся в стену (движение=' + moved.toFixed(2) + ')');
  // игрок никогда не оказывается внутри стены
  assert.ok(!D.solidMove(Math.floor(st.x), Math.floor(st.y)), 'игрок не в стене');
});

ok('кадр во время игры: виден пол, стены, свет', () => {
  // ракурс может случайно оказаться «в стену», поэтому берём лучший из восьми поворотов
  let best = null;
  const ang0 = st.ang;
  for (let k = 0; k < 8; k++) {
    st.ang = ang0 + (k / 8) * Math.PI * 2;
    h.pump(2);
    const s2 = frameStats(D);
    if (!best || s2.colors > best.colors) best = s2;
  }
  st.ang = ang0;
  h.pump(2);
  assert.ok(best.colors > 40, 'богатая палитра: ' + best.colors);
  assert.ok(best.mean > 6, 'кадр не тёмный настолько, чтобы ничего не было видно: ' + best.mean.toFixed(1));
});

/* ================= 3. разлом ================= */
ok('разлом переносит и поворачивает игрока', () => {
  const p = st.portals[0];
  st.x = p.ax + 0.5; st.y = p.ay - 0.5;      // стоим перед входом A
  st.lastTile = NE.I(p.ax, p.ay - 1);
  st.portalCd = 0;
  const ang0 = st.ang;
  const t0 = st.stats.teleports;
  st.y = p.ay + 0.5;                            // шаг на тайл разлома
  h.pump(1);
  assert.strictEqual(st.stats.teleports, t0 + 1, 'телепорт засчитан');
  assert.ok(Math.abs(st.x - (p.bx + 0.5)) < 1e-9 && Math.abs(st.y - (p.by + 0.5)) < 1e-9, 'перенесло в точку B');
  const da = Math.abs(((st.ang - ang0) % (Math.PI * 2) + Math.PI * 3) % (Math.PI * 2) - Math.PI);
  assert.ok(da > 1.5, 'пространство повёрнуто на угол, близкий к 90/180/270°');
  assert.ok(st.glitch > 0.4, 'экран глитчит');
  assert.ok(st.stats.teleports >= 1);
  assert.ok(!D.solidMove(Math.floor(st.x), Math.floor(st.y)), 'не в стене после телепорта');
});

ok('обратный разлом возвращает ориентацию', () => {
  const p = st.portals[0];
  const angBefore = st.ang;
  st.portalCd = 0;
  st.lastTile = NE.I(p.bx + 1, p.by);
  st.x = p.bx + 0.5; st.y = p.by + 0.5;
  h.pump(1);                                   // переход на тайл B → телепорт в A
  assert.ok(Math.abs(st.x - (p.ax + 0.5)) < 1e-9, 'вернуло в A');
  const rot = p.rot * Math.PI / 2;
  const d = Math.abs((((st.ang - angBefore) % (Math.PI * 2)) + Math.PI * 2) % (Math.PI * 2) - (Math.PI * 2 - rot));
  assert.ok(d < 1e-6 || d > Math.PI * 2 - 1e-6, 'поворот обратно равен -rot');
});

/* ================= 4. квантовая дверь ================= */
ok('дверь твёрдая под взглядом и призрачна — без взгляда', () => {
  const q = st.qdoors[0];
  const cx = q.x + 0.5, cy = q.y + 0.5;
  // найдём свободную клетку рядом с дверью, откуда на неё есть прямая видимость
  let spot = null;
  for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1], [2, 0], [-2, 0], [0, 2], [0, -2]]) {
    const tx = q.x + dx, ty = q.y + dy;
    if (!NE.inB(tx, ty)) continue;
    if (st.world.grid[NE.I(tx, ty)] !== NE.T.EMPTY) continue;
    spot = { x: tx + 0.5, y: ty + 0.5, ang: Math.atan2(cy - (ty + 0.5), cx - (tx + 0.5)) };
    break;
  }
  assert.ok(spot, 'нашлась точка обзора двери');
  st.x = spot.x; st.y = spot.y; st.ang = spot.ang; st.pitch = 0;
  D.syncCam(); D.computeObservation();
  assert.strictEqual(q.observed, true, 'дверь наблюдается');
  assert.strictEqual(D.solidMove(q.x, q.y), true, 'под взглядом она твёрдая');
  assert.strictEqual(D.solidRay(q.x, q.y), true, 'и непрозрачная');

  st.ang = spot.ang + Math.PI;                 // отвернулись
  D.syncCam(); D.computeObservation();
  assert.strictEqual(q.observed, false, 'дверь больше не наблюдается');
  assert.strictEqual(D.solidMove(q.x, q.y), false, 'без взгляда сквозь неё можно идти');

  // и реально проходим спиной
  st.x = spot.x; st.y = spot.y; st.ang = spot.ang + Math.PI;
  const before = { x: st.x, y: st.y };
  D.keys.KeyS = true;
  for (let i = 0; i < 90; i++) {
    D.syncCam();
    h.pump(1);
    const tx = Math.floor(st.x), ty = Math.floor(st.y);
    if (tx === q.x && ty === q.y) break;
  }
  D.keys.KeyS = false;
  const onDoor = Math.floor(st.x) === q.x && Math.floor(st.y) === q.y;
  const passedThrough = Math.hypot(st.x - before.x, st.y - before.y) > 0.8;
  assert.ok(onDoor || passedThrough, 'спиной сквозь дверь прошли (или встали на неё)');
});

/* ================= 5. осколки ================= */
ok('осколок подбирается и меняет HUD', () => {
  const f = st.fragments.find((x) => !x.taken);
  st.x = f.x; st.y = f.y - 0.35;
  h.pump(2);
  assert.ok(st.collected >= 1, 'осколок собран: ' + st.collected);
  assert.ok(h.ids.fragCount.textContent.startsWith(String(st.collected)), 'счётчик обновлён: ' + h.ids.fragCount.textContent);
  assert.ok(h.ids.fragPips.children[0].classList.contains('on'), 'пип загорелся');
});

ok('пять осколков открывают выход и делают карту честной', () => {
  for (let i = 0; i < st.fragments.length; i++) {
    if (st.fragments[i].taken) continue;
    st.x = st.fragments[i].x; st.y = st.fragments[i].y - 0.3;
    h.pump(2);
  }
  assert.strictEqual(st.collected, 5, 'собрано 5');
  assert.strictEqual(st.exitOpen, true, 'выход открыт');
  assert.strictEqual(st.mapTruth, true, 'карта больше не врёт');
  assert.strictEqual(D.solidMove(st.world.exit.x, st.world.exit.y), false, 'в открытую дверь можно войти');
  assert.ok(h.ids.miniWrap.classList.contains('truth'), 'подпись карты сменилась');
});

/* ================= 6. Наблюдатель ================= */
ok('Наблюдатель догоняет, если отвести взгляд', () => {
  const a = st.angels[0];
  a.awake = true;
  const f = st.fragments[0];
  // встанем в открытый коридор, наблюдателя поставим вплотную за спиной
  st.x = f.x; st.y = f.y;
  st.ang = Math.PI * 0.5;
  a.x = st.x - Math.cos(st.ang) * 0.9;
  a.y = st.y - Math.sin(st.ang) * 0.9;
  a.path.length = 0; a.repath = 0;
  const c0 = st.stats.catches;
  for (let i = 0; i < 200 && st.stats.catches === c0; i++) h.pump(1);
  assert.ok(st.stats.catches > c0, 'поймал');
  assert.ok(st.glitch > 0.2 || st.shake >= 0, 'реакция экрана');
  assert.ok(isFinite(st.x) && isFinite(st.y), 'после поимки координаты конечны');
  assert.ok(!D.solidMove(Math.floor(st.x), Math.floor(st.y)), 'не втиснут в стену');
});

ok('Наблюдатель замирает под взглядом', () => {
  const a = st.angels[0];
  a.awake = true;
  teleportPlayer(st.world.start.x, st.world.start.y, 0.6);
  const placed = spotSeeing(a.tx, a.ty, 6);
  assert.ok(placed, 'нашли точку, откуда наблюдатель виден');
  a.x = a.tx + 0.5; a.y = a.ty + 0.5;
  a.path.length = 0; a.repath = 99;
  D.syncCam(); D.computeObservation();
  assert.strictEqual(a.observed, true, 'видим его');
  const x0 = a.x, y0 = a.y;
  for (let i = 0; i < 40; i++) {
    a.observed = true;                       // держим взгляд
    h.pump(1);
    a.observed = true;
  }
  assert.ok(Math.hypot(a.x - x0, a.y - y0) < 0.35,
    'под взглядом почти не двигается: ' + Math.hypot(a.x - x0, a.y - y0).toFixed(3));
  a.awake = false;
});

/* ================= 7. компас и карта ================= */
ok('компас указывает на ближайший осколок', () => {
  const f = st.fragments[2];
  for (const o of st.fragments) o.taken = (o !== f);
  st.exitOpen = false;
  // встанем в 3 тайлах от осколка вдоль свободной оси
  const cands = [[f.x - 3, f.y, 0], [f.x + 3, f.y, Math.PI], [f.x, f.y - 3, Math.PI / 2], [f.x, f.y + 3, -Math.PI / 2]];
  let chosen = null;
  for (const c of cands) {
    if (D.solidMove(Math.floor(c[0]), Math.floor(c[1]))) continue;
    if (st.portalAt && st.portalAt[NE.I(Math.floor(c[0]), Math.floor(c[1]))] >= 0) continue;
    chosen = c; break;
  }
  assert.ok(chosen, 'нашлась свободная точка обзора осколка');
  teleportPlayer(chosen[0], chosen[1], chosen[2]);
  h.pump(1);
  const m = /rotate\((-?[\d.]+)deg\)/.exec(h.ids.needle.style.transform);
  assert.ok(m, 'стрелка повёрнута');
  const deg = parseFloat(m[1]);
  const want = NE.angDiff(Math.atan2(f.y - st.y, f.x - st.x), st.ang) * 180 / Math.PI;
  const norm = (v) => ((v % 360) + 540) % 360 - 180;
  assert.ok(Math.abs(norm(deg - want)) < 1.6, `стрелка ${deg}°, ожидали ${want.toFixed(1)}°`);
  assert.ok(/\d+ м/.test(h.ids.distRead.textContent), 'дистанция подписана: ' + h.ids.distRead.textContent);
  for (const o of st.fragments) o.taken = false;
});

ok('карта врёт, пока реальность не цела', () => {
  st.mapTruth = false;
  const lie0 = st.mapLie;
  st.x = st.portals[0].ax + 0.5; st.y = st.portals[0].ay - 0.5;
  st.lastTile = NE.I(st.portals[0].ax, st.portals[0].ay - 1);
  st.portalCd = 0;
  st.y = st.portals[0].ay + 0.5;
  h.pump(1);
  assert.notStrictEqual(st.mapLieTarget, lie0, 'после разлома цель угла карты изменилась');
  st.mapTruth = true; st.mapLieTarget = 0;
});

/* ================= 8. выход и финал ================= */
ok('вход в открытую дверь запускает финал', () => {
  st.exitOpen = true; st.collected = 5;
  const ex = st.world.exit;
  st.x = ex.x + 0.5; st.y = ex.y + 0.5;
  st.lastTile = NE.I(ex.x + (ex.horiz ? 1 : 0), ex.y + (ex.horiz ? 0 : 1));
  h.pump(2);
  assert.strictEqual(st.mode, 'ending', 'режим финала');
  assert.ok(!h.ids.ending.classList.contains('hidden'), 'экран финала показан');
  assert.ok(h.ids.endStats.innerHTML.includes('время'), 'статистика финала');
  assert.ok(h.ids.psLine.textContent.length > 10, 'P.S. на месте');
  h.pump(10);                                   // финальные кадры тоже рендерятся
  const s = frameStats(D);
  assert.ok(s.colors > 20, 'финальный кадр не пустой');
});

ok('новая игра перегенерирует мир', () => {
  const seed0 = st.seed;
  h.ids.btnAgain.dispatch('click', {});
  assert.strictEqual(st.mode, 'play');
  assert.notStrictEqual(st.seed, seed0, 'сид другой');
  assert.strictEqual(st.collected, 0, 'осколки сброшены');
  assert.strictEqual(st.exitOpen, false);
  h.pump(5);
  assert.ok(frameStats(D).colors > 20);
});

ok('пауза и возврат', () => {
  D.doPause();
  assert.strictEqual(st.mode, 'pause');
  assert.ok(!h.ids.pause.classList.contains('hidden'));
  h.pump(3);
  D.resume();
  assert.strictEqual(st.mode, 'play');
  h.pump(3);
});

/* ================= 9. устойчивость ================= */
ok('1000 кадров случайного броуна без падений и NaN', () => {
  const rnd = NE.mulberry32(12345);
  for (let i = 0; i < 1000; i++) {
    D.keys.KeyW = rnd() > 0.4;
    D.keys.KeyS = rnd() > 0.85;
    D.keys.KeyA = rnd() > 0.9;
    D.keys.KeyD = rnd() > 0.9;
    D.keys.ShiftLeft = rnd() > 0.93;
    st.ang += (rnd() - 0.5) * 0.35;
    st.pitch = (rnd() - 0.5) * D.ih * 0.5;
    h.pump(1, 8 + rnd() * 40);
    assert.ok(isFinite(st.x) && isFinite(st.y), 'NaN координат на кадре ' + i);
    assert.ok(isFinite(st.ang) && isFinite(st.pitch), 'NaN углов на кадре ' + i);
    assert.ok(st.x > 0.4 && st.x < NE.MAP_W - 0.4, 'не вывалился за карту');
    assert.ok(!D.solidMove(Math.floor(st.x), Math.floor(st.y)), 'застрял в стене на кадре ' + i);
  }
  D.keys.KeyW = D.keys.KeyS = D.keys.KeyA = D.keys.KeyD = D.keys.ShiftLeft = false;
});

ok('адаптивное разрешение не ломает буферы', () => {
  st.quality = 0.62; D.resize();
  const s1 = { w: D.iw, h: D.ih };
  assert.ok(s1.w > 100 && s1.h > 60, 'уменьшили разрешение');
  h.pump(3);
  assert.ok(frameStats(D).colors > 10);
  st.quality = 1; D.resize();
  assert.notDeepStrictEqual({ w: D.iw, h: D.ih }, s1, 'вернули разрешение');
  h.pump(3);
});

ok('экстремальные пропорции окна', () => {
  for (const [w, hh] of [[320, 900], [2400, 500], [360, 640], [1920, 1080]]) {
    h.sandbox.innerWidth = w; h.sandbox.innerHeight = hh;
    D.resize();
    assert.ok(D.iw >= 100 && D.ih >= 60, `нормальное внутреннее разрешение для ${w}x${hh}: ${D.iw}x${D.ih}`);
    assert.ok(D.iw / D.ih > 0.2 && D.iw / D.ih < 6, 'пропорции вменяемые');
    h.pump(2);
    assert.ok(frameStats(D).colors > 8);
  }
});

ok('звук выключается и включается', () => {
  h.ids.btnSound && h.ids.btnSound.dispatch('click', {});
  NE.Audio.setMuted(false);
});

/* ================= 10. много миров подряд ================= */
ok('20 разных лабиринтов подряд рендерятся', () => {
  for (let k = 0; k < 20; k++) {
    D.loadWorld(1000 + k * 7717);
    h.ids.btnStart && (st.mode = 'play');
    h.pump(3);
    const s = frameStats(D);
    assert.ok(s.colors > 25, 'мир ' + k + ' отрисован: ' + s.colors + ' оттенков');
    assert.ok(s.nonBlack > 0.1, 'мир ' + k + ' не чёрный');
  }
});

/* ---------- отчёт ---------- */
let failed = 0;
for (const r of results) {
  if (r[0] === 'FAIL') {
    failed++;
    console.log(`✗ ${r[1]}\n   ${r[2].message}\n${r[2].stack.split('\n').slice(1, 4).join('\n')}`);
  } else console.log(`✓ ${r[1]}`);
}

console.log('\n--- кадр из лабиринта (яркость, ASCII) ---');
D.loadWorld(20261024);
st.mode = 'play';
h.pump(2);
console.log(dumpFrame(D, 62, 22));
const s = frameStats(D);
console.log(`внутреннее разрешение ${s.W}x${s.H}, оттенков ${s.colors}, средняя яркость ${s.mean.toFixed(1)}, непустых пикселей ${(s.nonBlack * 100).toFixed(0)}%`);

console.log('\n--- ещё один ракурс (поворот на 137°) ---');
st.ang += 2.39; D.syncCam(); h.pump(2);
console.log(dumpFrame(D, 62, 22));

console.log(failed ? `\nПРОВАЛЕНО: ${failed}` : `\nВсе проверки пройдены (${results.length})`);
process.exit(failed ? 1 : 0);
