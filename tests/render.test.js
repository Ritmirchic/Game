/* ==========================================================================
   Рендер-тест: настоящий растеризатор (@napi-rs/canvas), сохранение PNG,
   проверка световой модели и палитры.
   node tests/render.test.js [--save=/tmp/preview]
   ========================================================================== */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const { makeSandbox, napi } = require('./harness/stub-dom.js');

if (!napi) { console.log('Пропуск: не установлен @napi-rs/canvas (npm i)'); process.exit(0); }

const ROOT = path.join(__dirname, '..');
const FILES = ['src/logic.js', 'src/textures.js', 'src/audio.js', 'src/game.js'];
const saveArg = process.argv.find((a) => a.startsWith('--save='));
const OUT = saveArg ? saveArg.split('=')[1] : null;
if (OUT) fs.mkdirSync(OUT, { recursive: true });

function boot(opts) {
  const h = makeSandbox(opts);
  const ctx = vm.createContext(h.sandbox);
  for (const f of FILES) vm.runInContext(fs.readFileSync(path.join(ROOT, f), 'utf8'), ctx, { filename: f });
  return { h, NE: h.sandbox.NE, D: h.sandbox.NE.debug };
}

function stats(img) {
  const d = img.data;
  const hist = new Uint32Array(256);
  let sum = 0, sum2 = 0, n = d.length / 4;
  const buckets = new Set();
  for (let k = 0; k < d.length; k += 4) {
    const l = (0.2126 * d[k] + 0.7152 * d[k + 1] + 0.0722 * d[k + 2]);
    sum += l; sum2 += l * l;
    hist[Math.min(255, l | 0)]++;
    buckets.add(((d[k] >> 3) << 10) | ((d[k + 1] >> 3) << 5) | (d[k + 2] >> 3));
  }
  const mean = sum / n;
  return {
    mean, std: Math.sqrt(Math.max(0, sum2 / n - mean * mean)),
    colors: buckets.size, buckets, hist,
    p05: pct(hist, n, 0.05), p50: pct(hist, n, 0.5), p95: pct(hist, n, 0.95), p99: pct(hist, n, 0.99)
  };
}
function pct(hist, n, p) {
  let acc = 0;
  for (let i = 0; i < 256; i++) { acc += hist[i]; if (acc >= n * p) return i; }
  return 255;
}

const B = boot();
const { D, NE, h } = B;
const st = D.st;

/* детерминизм: один и тот же мир в каждом прогоне */
const TEST_SEED = 20261024;
D.loadWorld(TEST_SEED);
st.mode = 'play';
h.ids.btnStart.dispatch('click', {});

/* ---------- куда смотрим ---------- */
function lookAt(tx, ty) {
  const cx = tx + 0.5, cy = ty + 0.5;
  let best = null;
  for (let r = 2; r <= 6 && !best; r++) {
    for (let a = 0; a < 16 && !best; a++) {
      const ang = (a / 16) * Math.PI * 2;
      const px = cx + Math.cos(ang) * r, py = cy + Math.sin(ang) * r;
      const ix = Math.floor(px), iy = Math.floor(py);
      if (!NE.inB(ix, iy)) continue;
      if (st.world.grid[NE.I(ix, iy)] !== NE.T.EMPTY) continue;
      st.x = px; st.y = py;
      st.ang = Math.atan2(cy - py, cx - px);
      st.pitch = 0;
      D.syncCam(); D.computeObservation();
      if (D.observed(cx, cy)) best = { x: px, y: py };
    }
  }
  return !!best;
}

function parkAngels() {
  for (const a of st.angels) { a.awake = false; a.x = 1.5; a.y = 1.5; a.path.length = 0; }
}

function frame(tag) {
  if (tag !== 'angel') parkAngels();
  h.pump(2);
  const img = D.img;
  if (OUT) {
    const scale = 3;
    const src = napi.createCanvas(D.iw, D.ih);
    src.getContext('2d').putImageData(img, 0, 0);
    const c = napi.createCanvas(D.iw * scale, D.ih * scale);
    const x = c.getContext('2d');
    x.imageSmoothingEnabled = false;
    x.drawImage(src, 0, 0, D.iw, D.ih, 0, 0, D.iw * scale, D.ih * scale);
    fs.writeFileSync(path.join(OUT, `frame-${tag}.png`), c.toBuffer('image/png'));
  }
  return stats(img);
}

const shots = [];
function shoot(tag, label) {
  const s = frame(tag);
  shots.push([label, s]);
  return s;
}

/* выбор ракурса: пробуем направления и берём то, где обзор глубже всего */
function bestView(px, py) {
  let bestAng = 0, bestScore = -1;
  for (let a = 0; a < 24; a++) {
    st.x = px; st.y = py; st.ang = (a / 24) * Math.PI * 2; st.pitch = 0;
    h.pump(1);
    const z = D.zbuf.slice().sort((u, v) => u - v);
    const med = z[(z.length * 0.55) | 0];
    const near = z[(z.length * 0.15) | 0];
    const score = Math.min(med, 9) + Math.min(near, 3) * 0.5;
    if (score > bestScore) { bestScore = score; bestAng = st.ang; }
  }
  st.x = px; st.y = py; st.ang = bestAng; st.pitch = 0;
  return bestScore;
}

/* 1. коридор от старта */
bestView(st.world.start.x, st.world.start.y);
shoot('corridor', 'коридор от старта');

/* 1b. комната */
{
  const r = st.world.rooms[0];
  if (r) { bestView(r.x + 0.5, r.y + 0.5); shoot('room', 'комната'); }
}

/* 2. разлом */
const p = st.portals[0];
if (lookAt(p.ax, p.ay)) shoot('portal', 'разлом ' + p.label);

/* 3. квантовая дверь (под взглядом — она материальна) */
const q = st.qdoors[0];
if (lookAt(q.x, q.y)) shoot('qdoor', 'квантовая дверь');

/* 4. осколок */
const f0 = st.fragments[0];
if (lookAt(f0.tx, f0.ty)) shoot('fragment', 'осколок');

/* 5. Наблюдатель */
{
  const a = st.angels[0];
  a.awake = true;
  const placed = lookAt(a.tx, a.ty);
  if (placed) { a.x = a.tx + 0.5; a.y = a.ty + 0.5; shoot('angel', 'Наблюдатель'); }
  a.awake = false;
}

/* 6. дверь выхода */
if (lookAt(st.world.exit.x, st.world.exit.y)) shoot('exit', 'дверь выхода (0/5)');

/* 7. открытый выход */
st.collected = 5; st.exitOpen = true; D.openExit();
if (lookAt(st.world.exit.x, st.world.exit.y)) shoot('exit-open', 'дверь выхода (5/5, открыта)');
st.collected = 0; st.exitOpen = false;

/* ---------- контактный лист текстур ---------- */
function sheet(file, items, cols, cellW, cellH, bg) {
  const rows = Math.ceil(items.length / cols);
  const c = napi.createCanvas(cols * cellW, rows * cellH);
  const x = c.getContext('2d');
  x.fillStyle = bg || '#0a0d12'; x.fillRect(0, 0, c.width, c.height);
  items.forEach((it, i) => {
    const cx = (i % cols) * cellW, cy = ((i / cols) | 0) * cellH;
    if (it.tex) {
      const im = x.createImageData(it.tex.w, it.tex.h);
      for (let k = 0, j = 0; k < it.tex.r.length; k++, j += 4) {
        im.data[j] = it.tex.r[k]; im.data[j + 1] = it.tex.g[k];
        im.data[j + 2] = it.tex.b[k]; im.data[j + 3] = it.tex.a ? it.tex.a[k] : 255;
      }
      const tmp = napi.createCanvas(it.tex.w, it.tex.h);
      tmp.getContext('2d').putImageData(im, 0, 0);
      const s = Math.min((cellW - 12) / it.tex.w, (cellH - 20) / it.tex.h);
      x.imageSmoothingEnabled = false;
      x.drawImage(tmp, 0, 0, it.tex.w, it.tex.h, cx + 6, cy + 6, it.tex.w * s, it.tex.h * s);
    }
    x.fillStyle = '#9fd8e8';
    x.font = '11px monospace';
    x.fillText(it.label, cx + 6, cy + cellH - 6);
  });
  if (OUT) fs.writeFileSync(path.join(OUT, file), c.toBuffer('image/png'));
  return c;
}

const tex = st.tex;
const wallItems = [];
tex.wall.forEach((t, i) => wallItems.push({ tex: t, label: 'стена · ' + NE.ZONES[i].name }));
tex.floor.forEach((t, i) => wallItems.push({ tex: t, label: 'пол · ' + NE.ZONES[i].name }));
tex.ceil.forEach((t, i) => wallItems.push({ tex: t, label: 'потолок ' + i }));
wallItems.push({ tex: tex.qdoor, label: 'квантовая дверь' });
wallItems.push({ tex: tex.exitClosed[0], label: 'выход 0/5' });
wallItems.push({ tex: tex.exitClosed[3], label: 'выход 3/5' });
wallItems.push({ tex: tex.exitOpen, label: 'выход открыт' });
wallItems.push({ tex: tex.decalRune, label: 'декаль двери' });
wallItems.push({ tex: tex.decalPortal[0], label: 'декаль разлома' });
sheet('textures.png', wallItems, 5, 140, 130);

const sprItems = [];
sprItems.push({ tex: tex.frag, label: 'осколок' });
sprItems.push({ tex: tex.angel, label: 'Наблюдатель' });
tex.portal.forEach((t, i) => sprItems.push({ tex: t, label: 'разлом ' + ['АЛЬФА', 'БЕТА', 'ГАММА', 'ДЕЛЬТА', 'ЭПСИЛОН'][i] }));
sprItems.push({ tex: tex.rune, label: 'руна выхода' });
sheet('sprites.png', sprItems, 4, 150, 190, '#05070a');

/* ---------- отчёт ---------- */
console.log('кадр              оттенков   среднее   σ     p05   p50   p95   p99');
for (const [label, s] of shots) {
  console.log(
    label.padEnd(18),
    String(s.colors).padStart(7),
    s.mean.toFixed(1).padStart(9),
    s.std.toFixed(1).padStart(7),
    String(s.p05).padStart(5), String(s.p50).padStart(5),
    String(s.p95).padStart(5), String(s.p99).padStart(5)
  );
}

/* ---------- проверки качества картинки ---------- */
let failed = 0;
function chk(name, cond, extra) {
  if (cond) console.log('✓ ' + name);
  else { failed++; console.log('✗ ' + name + (extra ? ' — ' + extra : '')); }
}

for (const [label, s] of shots) {
  // p95 ≥ 55: самый тёмный легитимный случай — запечатанная дверь вдали;
  // полностью чёрный кадр дал бы p95 ≈ 5-10, так что порог всё равно ловит провал света
  chk(`${label}: кадр читается (p95 ≥ 55)`, s.p95 >= 55, `p95=${s.p95}`);
  chk(`${label}: есть тени (p05 ≤ 40)`, s.p05 <= 40, `p05=${s.p05}`);
  chk(`${label}: кадр не плоский (≥ 24 оттенков)`, s.colors >= 24, `${s.colors}`);
  chk(`${label}: нет клиппинга в белом (p99 ≤ 252)`, s.p99 <= 252, `p99=${s.p99}`);
  chk(`${label}: контраст (σ ≥ 8)`, s.std >= 8, `σ=${s.std.toFixed(1)}`);
}
{
  const all = new Set();
  for (const [, s] of shots) if (s.buckets) s.buckets.forEach((b) => all.add(b));
  const rich = shots.filter(([, s]) => s.colors >= 120).length;
  chk(`хотя бы половина кадров богата палитрой (${rich}/${shots.length})`, rich >= Math.ceil(shots.length / 2), `${rich}`);
}

/* текстуры не пустые и не плоские */
for (const t of tex.wall.concat(tex.floor, tex.ceil)) {
  let mn = 255, mx = 0;
  for (let i = 0; i < t.r.length; i++) {
    const l = (t.r[i] + t.g[i] + t.b[i]) / 3;
    if (l < mn) mn = l; if (l > mx) mx = l;
  }
  chk(`текстура ${t.w}x${t.h} имеет диапазон`, mx - mn > 12, `${mn.toFixed(0)}..${mx.toFixed(0)}`);
}
/* спрайты имеют прозрачность и яркое ядро */
for (const [name, t] of [['осколок', tex.frag], ['Наблюдатель', tex.angel], ['разлом', tex.portal[0]], ['руна', tex.rune]]) {
  let transp = 0, opaque = 0, bright = 0;
  for (let i = 0; i < t.a.length; i++) {
    if (t.a[i] < 16) transp++;
    if (t.a[i] > 200) opaque++;
    if ((t.r[i] + t.g[i] + t.b[i]) / 3 > 180 && t.a[i] > 120) bright++;
  }
  chk(`${name}: есть прозрачный фон`, transp > t.a.length * 0.15, `${(100 * transp / t.a.length).toFixed(0)}%`);
  chk(`${name}: есть плотная фигура`, opaque > t.a.length * 0.02, `${(100 * opaque / t.a.length).toFixed(0)}%`);
  let eye = 0;
  for (let yy = 0; yy < (t.h * 0.4) | 0; yy++)
    for (let xx = 0; xx < t.w; xx++) {
      const i = yy * t.w + xx;
      if ((t.r[i] + t.g[i] + t.b[i]) / 3 > 170 && t.a[i] > 140) eye++;
    }
  chk(`${name}: есть яркие точки`, bright >= 2, String(bright));
  chk(`${name}: свет сосредоточен где надо (верх=${eye})`, name === 'Наблюдатель' ? eye >= 2 : true, String(eye));
}

if (OUT) console.log('\nPNG сохранены в ' + OUT);
process.exit(failed ? 1 : 0);
