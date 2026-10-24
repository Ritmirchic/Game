/* ==========================================================================
   НЕЕВКЛИД — движок: raycasting от первого лица, невозможная геометрия,
   Наблюдатели, квантовые двери, врущая карта. Чистый JS, без зависимостей.
   ========================================================================== */
(function (root) {
  'use strict';
  const NE = root.NE;
  const { MAP_W, MAP_H, T, ZONES, isWall, mulberry32, bfs, astar, generateWorld,
          clamp, lerp, I, inB } = NE;
  const { Audio } = NE;

  const TEX = 64;
  const FOV_SCALE = 1.0;          // planeLen = aspect/2 → квадратное остаётся квадратным
  const MAX_FLOOR = 13;           // дальше пол не текстурируем, только туман
  const MAX_WALL = 22;
  const LUT_N = 512, LUT_MAX = 26, LUT_S = LUT_N / LUT_MAX;
  const OBS_RANGE = 21;           // дальность «взгляда»
  const PLAYER_R = 0.24;

  /* ---------- энднесс и упаковка пикселя ---------- */
  const LE = (function () {
    const b = new ArrayBuffer(4);
    new Uint32Array(b)[0] = 0x01020304;
    return new Uint8Array(b)[0] === 0x04;
  })();
  const pack = LE
    ? function (r, g, b) { return (0xff000000 | (b << 16) | (g << 8) | r) >>> 0; }
    : function (r, g, b) { return ((r << 24) | (g << 16) | (b << 8) | 0xff) >>> 0; };

  /* ---------- LUT тумана и света ---------- */
  const lutFog = new Float32Array(LUT_N + 1);
  const lutLit = new Float32Array(LUT_N + 1);
  (function buildLUT() {
    const dens = 0.082, R = 12.5;
    for (let i = 0; i <= LUT_N; i++) {
      const d = i / LUT_S;
      lutFog[i] = 1 - Math.exp(-d * dens * (1 + d * 0.045));
      lutLit[i] = Math.pow(Math.max(0, 1 - d / R), 0.95);
    }
  })();
  const fogIdx = (d) => (d <= 0 ? 0 : d >= LUT_MAX ? LUT_N : (d * LUT_S) | 0);

  /* ---------- DOM ---------- */
  const $ = (id) => document.getElementById(id);
  const view = $('view'), ctx = view.getContext('2d', { alpha: false });
  const mini = $('mini'), mctx = mini.getContext('2d');
  const hud = $('hud');
  const elMsg = $('msg'), elHint = $('hint'), elDread = $('dread');
  const elZone = $('zoneBox'), elZoneName = $('zoneName'), elZoneSub = $('zoneSub');
  const elPips = $('fragPips'), elCount = $('fragCount'), elFragBox = $('fragBox');
  const elNeedle = $('needle'), elDist = $('distRead');
  const elMenu = $('menu'), elPause = $('pause'), elEnd = $('ending');
  const elFlash = $('flash'), elDreadVig = $('dreadVig'), elGrain = $('grain');
  const elMiniWrap = $('miniWrap'), elMiniCap = $('miniCaption');

  /* ---------- состояние ---------- */
  const st = {
    mode: 'menu',
    seed: 0, rnd: mulberry32(1),
    world: null, tex: null, grid: null, zoneGrid: null, decals: null, decalHue: null,
    qState: null, portalAt: null,
    x: 0, y: 0, ang: 0, pitch: 0,
    vx: 0, vy: 0, bob: 0, bobPx: 0, stepPhase: 0,
    speedNow: 0, run: false,
    collected: 0, lastTaken: -1, exitOpen: false,
    fragments: [], angels: [], portals: [], qdoors: [],
    explored: null, trail: [], trailT: 0,
    mapLie: 0, mapLieTarget: 0, mapTruth: false,
    glitch: 0, shake: 0, flashV: 0, dread: 0, flick: 1,
    lastTile: -1, portalCd: 0,
    stats: { time: 0, steps: 0, teleports: 0, catches: 0, seen: 0, doors: 0, started: 0 },
    zone: -1, zoneTimer: 0,
    chimeT: 0, heartT: 0, msgQueue: [], msgUntil: 0, hintUntil: 0, _lastStep: 0,
    shown: {}, quality: 1, frames: 0, ft: 0, fps: 60,
    endT: 0, touch: false, joyX: 0, joyY: 0, lookTouch: null
  };

  /* ---------- буферы ---------- */
  let iw = 320, ih = 180, img = null, buf32 = null, buf8 = null, zbuf = null, coneArr = null;
  let mapCanvas = null, mapCtx = null;

  function resize() {
    const cw = Math.max(240, root.innerWidth | 0), chh = Math.max(160, root.innerHeight | 0);
    let w = clamp(Math.round(cw / 3.3), 220, 420);
    let h = Math.round(w * chh / cw);
    if (h > 320) { h = 320; w = Math.round(h * cw / chh); }
    if (h < 130) { h = 130; w = Math.round(h * cw / chh); }
    w = Math.max(160, Math.round(w * st.quality));
    h = Math.max(100, Math.round(h * st.quality));
    if (w === iw && h === ih && img) return;
    iw = w; ih = h;
    view.width = w; view.height = h;
    img = ctx.createImageData(w, h);
    buf8 = img.data;
    buf32 = new Uint32Array(buf8.buffer);
    zbuf = new Float32Array(w);
    coneArr = new Float32Array(w);
    ctx.imageSmoothingEnabled = false;
  }

  /* ==========================================================================
     МИР
     ========================================================================== */
  function loadWorld(seed) {
    st.seed = seed >>> 0;
    st.rnd = mulberry32(st.seed ^ 0x9e3779b9);
    const w = generateWorld(st.seed);
    st.world = w;
    st.grid = w.grid; st.zoneGrid = w.zone; st.decals = w.decals; st.decalHue = w.decalHue;
    st.qState = new Uint8Array(MAP_W * MAP_H);
    st.portalAt = new Int16Array(MAP_W * MAP_H).fill(-1);
    w.portals.forEach((p, i) => { st.portalAt[I(p.ax, p.ay)] = i; st.portalAt[I(p.bx, p.by)] = i; });

    st.tex = NE.Textures.build(st.seed);
    st.fragments = w.fragments.map((f) => ({ x: f.x, y: f.y, tx: f.tx, ty: f.ty, taken: false, phase: f.phase, home: f }));
    st.angels = w.angels.map((a, i) => ({
      x: a.x, y: a.y, tx: a.tx, ty: a.ty,
      path: [], repath: 0, observed: false, still: 0,
      awake: false, wasObs: false, speed: 1.42, wander: null, jitter: a.jitter, id: i
    }));
    st.portals = w.portals;
    st.qdoors = w.qdoors.map((q) => ({ x: q.x, y: q.y, observed: false, hum: 0 }));

    st.x = w.start.x; st.y = w.start.y;
    st.ang = Math.PI * 0.25; st.pitch = 0;
    st.collected = 0; st.lastTaken = -1; st.exitOpen = false;
    st.explored = new Uint8Array(MAP_W * MAP_H);
    st.trail = []; st.trailT = 0;
    st.mapLie = (st.rnd() * 6.28); st.mapLieTarget = st.mapLie; st.mapTruth = false;
    st.glitch = 0; st.shake = 0; st.dread = 0;
    st.lastTile = I(Math.floor(st.x), Math.floor(st.y));
    st.portalCd = 0;
    st.zone = -1; st.zoneTimer = 0;
    st.stats = { time: 0, steps: 0, teleports: 0, catches: 0, seen: 0, doors: 0, started: performance.now() };
    st.shown = {};
    st.msgQueue = []; st.msgUntil = 0; st.hintUntil = 0;

    // карта исследованных тайлов (инкрементальный offscreen)
    mapCanvas = document.createElement('canvas');
    mapCanvas.width = MAP_W * 4; mapCanvas.height = MAP_H * 4;
    mapCtx = mapCanvas.getContext('2d');
    mapCtx.clearRect(0, 0, mapCanvas.width, mapCanvas.height);

    // первый кадр: открыть зону вокруг старта
    markExplored(true);
    updateZone(true);
    Audio.setZone(zoneAt(st.x, st.y));
  }

  const zoneAt = (x, y) => {
    const tx = clamp(Math.floor(x), 0, MAP_W - 1), ty = clamp(Math.floor(y), 0, MAP_H - 1);
    return st.zoneGrid[I(tx, ty)] || 0;
  };

  function markExplored(initial) {
    const px = Math.floor(st.x), py = Math.floor(st.y);
    const R = initial ? 3 : 2;
    let changed = false;
    for (let dy = -R; dy <= R; dy++) {
      for (let dx = -R; dx <= R; dx++) {
        const x = px + dx, y = py + dy;
        if (!inB(x, y)) continue;
        if (dx * dx + dy * dy > R * R + 1) continue;
        const i = I(x, y);
        if (st.explored[i]) continue;
        st.explored[i] = 1; changed = true;
        const c = st.grid[i];
        if (c === T.EXIT) continue;                       // выход на карте не виден, пока не открыт
        const z = st.zoneGrid[i];
        const col = c === T.EMPTY || c === T.QDOOR
          ? ['rgba(120,180,205,.34)', 'rgba(205,160,105,.34)', 'rgba(185,120,225,.34)', 'rgba(120,215,165,.34)'][z]
          : ['rgba(150,205,230,.10)', 'rgba(230,185,130,.10)', 'rgba(210,145,250,.10)', 'rgba(145,240,185,.10)'][z];
        mapCtx.fillStyle = col;
        mapCtx.fillRect(x * 4, y * 4, 4, 4);
        if (c === T.QDOOR) {
          mapCtx.fillStyle = 'rgba(255,120,235,.85)';
          mapCtx.fillRect(x * 4 + 1, y * 4 + 1, 2, 2);
        }
      }
    }
    return changed;
  }

  function updateZone(force) {
    const z = zoneAt(st.x, st.y);
    if (z === st.zone && !force) return;
    st.zone = z;
    Audio.setZone(z);
    elZoneName.textContent = ZONES[z].name;
    elZoneSub.textContent = ZONES[z].sub;
    elZone.classList.add('show');
    clearTimeout(st.zoneTimer);
    st.zoneTimer = setTimeout(() => elZone.classList.remove('show'), force ? 2600 : 3600);
  }

  /* ==========================================================================
     ВЗГЛЯД: что существует, только когда на него смотрят
     ========================================================================== */
  let dirX = 1, dirY = 0, planeX = 0, planeY = 0.5, hor = 90, cosHalf = Math.cos(0.62);

  /* базис камеры обязан быть свежим и до логики, и до рендера */
  function syncCam() {
    dirX = Math.cos(st.ang); dirY = Math.sin(st.ang);
    const planeLen = (iw / (2 * ih)) * FOV_SCALE;
    planeX = -dirY * planeLen; planeY = dirX * planeLen;
    // конус «взгляда» чуть уже поля зрения: дверь материализуется на периферии
    cosHalf = Math.cos(Math.atan(planeLen) * 0.86);
    return planeLen;
  }

  function lineOfSight(x0, y0, x1, y1) {
    const dx = x1 - x0, dy = y1 - y0;
    const d = Math.sqrt(dx * dx + dy * dy);
    const steps = Math.ceil(d / 0.09);
    for (let i = 1; i < steps; i++) {
      const t = i / steps;
      const tx = Math.floor(x0 + dx * t), ty = Math.floor(y0 + dy * t);
      if (!inB(tx, ty)) return false;
      if (isWall(st.grid[I(tx, ty)])) return false;
    }
    return true;
  }

  function observed(x, y) {
    const dx = x - st.x, dy = y - st.y;
    const d2 = dx * dx + dy * dy;
    if (d2 > OBS_RANGE * OBS_RANGE) return false;
    const d = Math.sqrt(d2);
    if (d < 0.001) return true;
    if ((dx * dirX + dy * dirY) / d < cosHalf) return false;
    return lineOfSight(st.x, st.y, x, y);
  }

  function computeObservation() {
    let anyDoor = 0;
    for (const q of st.qdoors) {
      const o = observed(q.x + 0.5, q.y + 0.5);
      q.observed = o;
      st.qState[I(q.x, q.y)] = o ? 1 : 0;
      q.hum = lerp(q.hum, o ? 1 : 0, 0.18);
      if (o) anyDoor = Math.max(anyDoor, q.hum);
    }
    Audio.doorHum(anyDoor);
    for (const a of st.angels) a.observed = observed(a.x, a.y);
  }

  /* ==========================================================================
     СОЛИДНОСТЬ
     ========================================================================== */
  function solidRay(tx, ty) {
    if (!inB(tx, ty)) return true;
    const c = st.grid[I(tx, ty)];
    if (isWall(c)) return true;
    if (c === T.QDOOR) return st.qState[I(tx, ty)] === 1;
    if (c === T.EXIT) return true;
    return false;
  }
  function solidMove(tx, ty) {
    if (!inB(tx, ty)) return true;
    const c = st.grid[I(tx, ty)];
    if (isWall(c)) return true;
    if (c === T.QDOOR) return st.qState[I(tx, ty)] === 1;
    if (c === T.EXIT) return !st.exitOpen;
    return false;
  }
  function canOccupy(x, y) {
    return !solidMove(Math.floor(x - PLAYER_R), Math.floor(y - PLAYER_R)) &&
           !solidMove(Math.floor(x + PLAYER_R), Math.floor(y - PLAYER_R)) &&
           !solidMove(Math.floor(x - PLAYER_R), Math.floor(y + PLAYER_R)) &&
           !solidMove(Math.floor(x + PLAYER_R), Math.floor(y + PLAYER_R));
  }

  /* ==========================================================================
     ВВОД
     ========================================================================== */
  const keys = Object.create(null);
  let sens = 0.0022;

  function onKeyDown(e) {
    if (e.repeat) return;
    keys[e.code] = true;
    if (e.code === 'Space' || e.code.indexOf('Arrow') === 0) e.preventDefault();
    if (e.code === 'KeyM') toggleMute();
    if (e.code === 'KeyP' && st.mode === 'play') doPause();
    if (e.code === 'Enter' && st.mode === 'menu') startGame();
  }
  function onKeyUp(e) { keys[e.code] = false; }

  function onMouse(e) {
    if (st.mode !== 'play') return;
    if (document.pointerLockElement === view) {
      st.ang += e.movementX * sens;
      st.pitch = clamp(st.pitch - e.movementY * sens * ih * 0.55, -ih * 0.30, ih * 0.30);
      return;
    }
    // pointer lock недоступен (iframe, тач-ноутбук, отказ браузера) —
    // включаем свободный взгляд мышью без удержания кнопки
    if (!drag.active) return;
    if (!drag.has) { drag.x = e.clientX; drag.y = e.clientY; drag.has = true; return; }
    st.ang += (e.clientX - drag.x) * 0.0042;
    st.pitch = clamp(st.pitch - (e.clientY - drag.y) * 0.0022 * ih, -ih * 0.30, ih * 0.30);
    drag.x = e.clientX; drag.y = e.clientY;
  }
  const drag = { active: false, has: false, x: 0, y: 0 };

  function bindInput() {
    addEventListener('keydown', onKeyDown, { passive: false });
    addEventListener('keyup', onKeyUp);
    addEventListener('mousemove', onMouse);
    addEventListener('resize', () => { resize(); });
    addEventListener('blur', () => { for (const k in keys) keys[k] = false; if (st.mode === 'play') doPause(); });

    view.addEventListener('mousedown', (e) => {
      if (st.mode !== 'play') return;
      if (document.pointerLockElement !== view) {
        drag.active = true; drag.has = true; drag.x = e.clientX; drag.y = e.clientY;
        tryLock();
      }
    });
    addEventListener('mouseup', () => { if (!st.lockFailed) drag.active = false; });

    document.addEventListener('pointerlockchange', () => {
      if (document.pointerLockElement !== view && st.mode === 'play') doPause();
    });
    document.addEventListener('pointerlockerror', () => {
      // в песочницах и iframe pointer lock часто запрещён политикой разрешений:
      // тогда взгляд мышью работает без удержания кнопки
      st.lockFailed = true;
      drag.active = true; drag.has = false;
      once('nolock', () => hint('pointer lock недоступен — смотри мышью без клика', 5000));
    });

    // тач
    st.touch = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;
    if (st.touch) $('touch').classList.add('on');
    const stick = $('stick'), knob = $('knob');
    let stickId = null, lookId = null, lookLast = null, sc = { x: 0, y: 0, r: 60 };

    function stickRect() {
      const r = stick.getBoundingClientRect();
      sc.x = r.left + r.width / 2; sc.y = r.top + r.height / 2; sc.r = r.width / 2;
    }
    const touchRoot = $('stage') || view.parentElement || document.body;
    touchRoot.addEventListener('touchstart', (e) => {
      if (st.mode !== 'play') return;
      stickRect();
      for (const t of e.changedTouches) {
        const dx = t.clientX - sc.x, dy = t.clientY - sc.y;
        if (stickId === null && Math.hypot(dx, dy) < sc.r * 1.7) {
          stickId = t.identifier; moveStick(t);
        } else if (lookId === null) {
          lookId = t.identifier; lookLast = { x: t.clientX, y: t.clientY };
        }
      }
    }, { passive: true });
    addEventListener('touchmove', (e) => {
      for (const t of e.changedTouches) {
        if (t.identifier === stickId) moveStick(t);
        else if (t.identifier === lookId && lookLast) {
          st.ang += (t.clientX - lookLast.x) * 0.006;
          st.pitch = clamp(st.pitch - (t.clientY - lookLast.y) * 0.0035 * ih, -ih * 0.3, ih * 0.3);
          lookLast = { x: t.clientX, y: t.clientY };
        }
      }
    }, { passive: true });
    function endTouch(e) {
      for (const t of e.changedTouches) {
        if (t.identifier === stickId) { stickId = null; st.joyX = 0; st.joyY = 0; knob.style.transform = 'translate(-50%,-50%)'; }
        if (t.identifier === lookId) { lookId = null; lookLast = null; }
      }
    }
    addEventListener('touchend', endTouch); addEventListener('touchcancel', endTouch);
    function moveStick(t) {
      let dx = (t.clientX - sc.x) / sc.r, dy = (t.clientY - sc.y) / sc.r;
      const m = Math.hypot(dx, dy);
      if (m > 1) { dx /= m; dy /= m; }
      st.joyX = dx; st.joyY = dy;
      st.run = m > 0.92;
      knob.style.transform = `translate(calc(-50% + ${dx * 34}px), calc(-50% + ${dy * 34}px))`;
    }
  }

  function tryLock() {
    try {
      const p = view.requestPointerLock();
      if (p && p.catch) p.catch(() => {});
    } catch (e) { /* не критично */ }
  }

  /* ==========================================================================
     СООБЩЕНИЯ
     ========================================================================== */
  function say(html, ms, force) {
    if (!force && st.msgUntil > performance.now()) { st.msgQueue.push([html, ms]); return; }
    elMsg.innerHTML = html;
    elMsg.classList.add('show');
    st.msgUntil = performance.now() + (ms || 4200);
    clearTimeout(say._t);
    say._t = setTimeout(() => {
      elMsg.classList.remove('show');
      st.msgUntil = 0;
      const nx = st.msgQueue.shift();
      if (nx) setTimeout(() => say(nx[0], nx[1], true), 500);
    }, ms || 4200);
  }
  function hint(text, ms) {
    elHint.textContent = text;
    elHint.classList.add('show');
    clearTimeout(hint._t);
    hint._t = setTimeout(() => elHint.classList.remove('show'), ms || 3600);
  }
  function once(key, fn) { if (st.shown[key]) return; st.shown[key] = 1; fn(); }

  /* ==========================================================================
     ОБНОВЛЕНИЕ
     ========================================================================== */
  function update(dt) {
    syncCam();
    computeObservation();

    /* --- ввод и движение --- */
    let fwd = 0, side = 0;
    if (keys.KeyW || keys.ArrowUp) fwd += 1;
    if (keys.KeyS || keys.ArrowDown) fwd -= 1;
    if (keys.KeyD || keys.ArrowRight) side += 1;
    if (keys.KeyA || keys.ArrowLeft) side -= 1;
    if (st.touch) { fwd += -st.joyY; side += st.joyX; }
    st.run = !!(keys.ShiftLeft || keys.ShiftRight);
    if (st.touch) st.run = st.run || Math.hypot(st.joyX, st.joyY) > 0.9;
    const mag = Math.hypot(fwd, side);
    if (mag > 1) { fwd /= mag; side /= mag; }

    const spd = (st.run ? 4.35 : 2.62) * (mag > 0 ? 1 : 0);
    let mx = (dirX * fwd + (-dirY) * side);
    let my = (dirY * fwd + (dirX) * side);
    const ml = Math.hypot(mx, my);
    if (ml > 0.0001) { mx = mx / ml * spd * dt; my = my / ml * spd * dt; }

    const ox = st.x, oy = st.y;
    if (canOccupy(st.x + mx, st.y)) st.x += mx;
    if (canOccupy(st.x, st.y + my)) st.y += my;
    st.x = clamp(st.x, 0.6, MAP_W - 0.6);
    st.y = clamp(st.y, 0.6, MAP_H - 0.6);

    const moved = Math.hypot(st.x - ox, st.y - oy);
    st.speedNow = moved / Math.max(dt, 0.0001);
    st.stats.steps += moved;

    /* --- покачивание и шаги --- */
    if (st.speedNow > 0.25) {
      st.stepPhase += dt * (st.run ? 11.4 : 7.6);
      st.bobPx = Math.sin(st.stepPhase) * (st.run ? 3.6 : 2.4);
      if (st.stepPhase - (st._lastStep || 0) > Math.PI) {
        st._lastStep = st.stepPhase;
        Audio.footstep(st.run ? 1.5 : 1);
      }
    } else {
      st.bobPx = lerp(st.bobPx, 0, 0.15);
      st.stepPhase = 0; st._lastStep = 0;
    }

    /* --- смена тайла: разломы, след, исследование --- */
    const tile = I(Math.floor(st.x), Math.floor(st.y));
    if (tile !== st.lastTile) {
      st.lastTile = tile;
      markExplored(false);
      updateZone(false);
      const pIdx = st.portalAt[tile];
      if (pIdx >= 0 && st.portalCd <= 0) usePortal(pIdx, st.portals[pIdx].ax === Math.floor(st.x) && st.portals[pIdx].ay === Math.floor(st.y));
      if (st.grid[tile] === T.EXIT && st.exitOpen) return win();
    }
    st.portalCd = Math.max(0, st.portalCd - dt);

    st.trailT -= dt;
    if (st.trailT <= 0) { st.trailT = 0.18; st.trail.push(st.x, st.y); if (st.trail.length > 9000) st.trail.splice(0, 600); }

    /* --- осколки --- */
    let bestF = null, bestD = 1e9;
    for (let i = 0; i < st.fragments.length; i++) {
      const f = st.fragments[i];
      if (f.taken) continue;
      f.phase += dt * 1.6;
      const d = Math.hypot(f.x - st.x, f.y - st.y);
      if (d < bestD) { bestD = d; bestF = f; }
      if (d < 0.52) takeFragment(i);
    }
    if (bestF) {
      const rel = Math.atan2(bestF.y - st.y, bestF.x - st.x) - st.ang;
      elNeedle.style.transform = `rotate(${(rel * 180 / Math.PI).toFixed(1)}deg)`;
      elDist.textContent = (bestD * 1.7).toFixed(0) + ' м';
      st.chimeT -= dt;
      if (st.chimeT <= 0) {
        st.chimeT = clamp(bestD * 0.10, 0.20, 1.7);
        Audio.ping(clamp(1 - bestD / 22, 0, 1));
      }
    } else {
      elDist.textContent = st.exitOpen ? 'выход открыт' : '—';
      if (st.exitOpen) {
        const d = Math.hypot(st.world.exit.x + 0.5 - st.x, st.world.exit.y + 0.5 - st.y);
        const rel = Math.atan2(st.world.exit.y + 0.5 - st.y, st.world.exit.x + 0.5 - st.x) - st.ang;
        elNeedle.style.transform = `rotate(${(rel * 180 / Math.PI).toFixed(1)}deg)`;
        elDist.textContent = 'выход · ' + (d * 1.7).toFixed(0) + ' м';
      } else elNeedle.style.transform = 'rotate(0deg)';
    }

    /* --- Наблюдатели --- */
    updateAngels(dt);
    st.heartT -= dt;
    if (st.heartT <= 0) {
      if (st.dread > 0.12) { Audio.heartbeat(); st.heartT = lerp(1.15, 0.32, st.dread); }
      else st.heartT = 0.4;
    }

    /* --- выход --- */
    const ex = st.world.exit;
    const dex = Math.hypot(ex.x + 0.5 - st.x, ex.y + 0.5 - st.y);
    if (!st.exitOpen && dex < 1.9) {
      once('exitClosed', () => {
        say('Печать на двери пуста. <b>Пять гнёзд — пять осколков.</b>', 4200);
        Audio.denied();
      });
    }

    /* --- подсказки --- */
    tutorial();

    /* --- статистика и эффекты --- */
    st.stats.time += dt;
    st.glitch = Math.max(0, st.glitch - dt * 1.5);
    st.shake = Math.max(0, st.shake - dt * 2.2);
    st.flashV = Math.max(0, st.flashV - dt * 2.6);
    st.mapLie = lerp(st.mapLie, st.mapLieTarget, 1 - Math.pow(0.02, dt));
    elFlash.style.opacity = (st.flashV * 0.85).toFixed(3);
    elDreadVig.style.opacity = (st.dread * 0.9).toFixed(3);
    if (st.shake > 0.01) {
      const s = st.shake * 7;
      view.style.transform = `scale(1.03) translate(${((st.rnd() - 0.5) * s).toFixed(2)}px,${((st.rnd() - 0.5) * s).toFixed(2)}px)`;
    } else if (view.style.transform) view.style.transform = '';
    elDread.classList.toggle('show', st.dread > 0.35);
  }

  function tutorial() {
    if (!st.shown.controls) {
      st.shown.controls = 1;
      setTimeout(() => {
        if (st.mode !== 'play') return;
        say('Найди <b>пять осколков реальности</b>. Компас внизу указывает на ближайший — и, в отличие от карты, <em>не врёт</em>.', 6200, true);
      }, 900);
    }
    for (const p of st.portals) {
      for (const t of [[p.ax, p.ay], [p.bx, p.by]]) {
        if (Math.hypot(t[0] + 0.5 - st.x, t[1] + 0.5 - st.y) < 3.1) {
          once('portal', () => say(`Разлом <b>${p.label}</b>. Пространство здесь сшито наизнанку: шагнёшь — окажешься не там <em>и не так повёрнутым</em>.`, 5600));
        }
      }
    }
    for (const q of st.qdoors) {
      if (Math.hypot(q.x + 0.5 - st.x, q.y + 0.5 - st.y) < 3.4) {
        once('qdoor', () => {
          say('Дверь существует, <em>только когда ты на неё смотришь</em>. Отвернись и пройди сквозь неё спиной.', 6400);
          hint('S — шаг назад, не отводя глаз… или отвернись совсем', 6000);
        });
      }
    }
    for (const a of st.angels) {
      if (a.observed && !a.wasObs) {
        st.stats.seen++;
        if (Math.hypot(a.x - st.x, a.y - st.y) < 16)
          once('angel', () => say('<b>НЕ ОТВОДИ ВЗГЛЯД.</b> Оно не двигается, пока ты на него смотришь.', 5600));
      }
      a.wasObs = a.observed;
    }
  }

  /* ---------- осколки ---------- */
  function takeFragment(i) {
    const f = st.fragments[i];
    f.taken = true;
    st.collected++;
    st.lastTaken = i;
    Audio.pickup(st.collected);
    st.flashV = Math.max(st.flashV, 0.45);
    st.glitch = Math.max(st.glitch, 0.5);
    elFragBox.classList.remove('pulse'); void elFragBox.offsetWidth; elFragBox.classList.add('pulse');
    syncPips();
    const lines = [
      'Осколок <b>1</b>. Свет в лабиринте стал чуть менее вежливым.',
      'Осколок <b>2</b>. Где-то далеко что-то перестало притворяться стеной.',
      'Осколок <b>3</b>. Ты слышишь шаги, которых не делал.',
      'Осколок <b>4</b>. Наблюдатели больше не ждут. Они идут.',
      'Осколок <b>5</b>. Реальность цела. <em>Дверь открыта. Карта перестала врать.</em>'
    ];
    say(lines[clamp(st.collected - 1, 0, 4)], 5200, true);
    if (st.collected >= 5) openExit();
  }

  function openExit() {
    st.exitOpen = true;
    st.mapTruth = true;
    st.mapLieTarget = 0;
    elMiniWrap.classList.add('truth');
    elMiniCap.textContent = 'карта · теперь честно';
    Audio.openDoor();
    st.glitch = Math.max(st.glitch, 1.2);
    // перерисовать всю карту как исследованную — награда за честность
    for (let y = 0; y < MAP_H; y++) for (let x = 0; x < MAP_W; x++) {
      const i = I(x, y);
      if (st.explored[i]) continue;
      st.explored[i] = 1;
      const c = st.grid[i];
      const z = st.zoneGrid[i];
      mapCtx.fillStyle = c === T.EMPTY || c === T.QDOOR
        ? ['rgba(120,180,205,.34)', 'rgba(205,160,105,.34)', 'rgba(185,120,225,.34)', 'rgba(120,215,165,.34)'][z]
        : ['rgba(150,205,230,.10)', 'rgba(230,185,130,.10)', 'rgba(210,145,250,.10)', 'rgba(145,240,185,.10)'][z];
      mapCtx.fillRect(x * 4, y * 4, 4, 4);
    }
  }

  function syncPips() {
    elCount.textContent = st.collected + ' / 5';
    const pips = elPips.children;
    for (let i = 0; i < pips.length; i++) pips[i].classList.toggle('on', i < st.collected);
  }

  /* ---------- разломы ---------- */
  function usePortal(idx, fromA) {
    const p = st.portals[idx];
    const nx = fromA ? p.bx : p.ax, ny = fromA ? p.by : p.ay;
    st.x = nx + 0.5; st.y = ny + 0.5;
    const rot = p.rot * Math.PI / 2 * (fromA ? 1 : -1);
    st.ang = (st.ang + rot) % (Math.PI * 2);
    st.lastTile = I(nx, ny);
    st.portalCd = 0.55;
    st.stats.teleports++;
    st.glitch = 1.25; st.shake = 0.9; st.flashV = 0.35;
    Audio.whoosh(fromA ? 1 : -1);
    // карта врёт сильнее
    if (!st.mapTruth) st.mapLieTarget = st.mapLie + (st.rnd() * 2 - 1) * Math.PI * 1.5;
    once('portalUsed', () => say(`Тебя переставили. Разлом <b>${p.label}</b> повернул пространство на ${(p.rot * 90)}°. <em>Карта теперь врёт.</em>`, 5600));
  }

  /* ---------- Наблюдатели ---------- */
  function farTile(fromX, fromY, minD) {
    const r = bfs(st.world, Math.floor(fromX), Math.floor(fromY), (c) => c === T.EMPTY || c === T.QDOOR, false);
    const cand = [];
    for (let i = 0; i < r.dist.length; i++) {
      if (r.dist[i] < minD) continue;
      if (st.grid[i] !== T.EMPTY) continue;
      if (st.portalAt[i] >= 0) continue;
      cand.push(i);
    }
    if (!cand.length) return null;
    return cand[(st.rnd() * cand.length) | 0];
  }

  function updateAngels(dt) {
    let dread = 0;
    const px = Math.floor(st.x), py = Math.floor(st.y);
    for (const a of st.angels) {
      const d = Math.hypot(a.x - st.x, a.y - st.y);
      if (!a.awake) {
        if (d < 15 || (st.collected >= 1 && d < 26)) a.awake = true;
        else continue;
      }
      const speed = a.speed + st.collected * 0.16 + (st.run && d < 14 ? 0.5 : 0);

      if (a.observed) {
        a.still += dt;
        a.path.length = 0;
        if (a.still > 4.5 && d < 12) {
          a.still = 0;
          const t = farTile(a.x, a.y, 14);
          if (t) { a.x = (t % MAP_W) + 0.5; a.y = ((t / MAP_W) | 0) + 0.5; Audio.glitch(); st.glitch = Math.max(st.glitch, 0.3); }
        }
      } else {
        a.still = 0;
        a.repath -= dt * (st.run ? 1.8 : 1);
        if (a.repath <= 0 || !a.path.length) {
          a.repath = 0.5 + st.rnd() * 0.35;
          const p = astar(st.world, Math.floor(a.x), Math.floor(a.y), px, py, { maxExpand: 3000 });
          a.path = p.length ? p : [];
          if (!a.path.length) {
            const t = farTile(a.x, a.y, 3);
            a.wander = t;
          }
        }
        let tx, ty;
        if (a.path.length) { const n = a.path[0]; tx = (n % MAP_W) + 0.5; ty = ((n / MAP_W) | 0) + 0.5; }
        else if (a.wander != null) { tx = (a.wander % MAP_W) + 0.5; ty = ((a.wander / MAP_W) | 0) + 0.5; }
        else { tx = st.x; ty = st.y; }
        const dx = tx - a.x, dy = ty - a.y, dd = Math.hypot(dx, dy);
        if (dd < 0.08) { if (a.path.length) a.path.shift(); else a.wander = null; }
        else {
          const k = Math.min(1, speed * dt / dd);
          a.x += dx * k; a.y += dy * k;
        }
      }

      const nd = Math.hypot(a.x - st.x, a.y - st.y);
      if (nd < 9) dread = Math.max(dread, clamp((9 - nd) / 9, 0, 1) * (a.observed ? 0.45 : 1));
      if (nd < 0.55) { caught(a); continue; }
      a.jitter += dt;
    }
    st.dread = dread;
    Audio.angelDread(dread);
  }

  function caught(a) {
    st.stats.catches++;
    st.glitch = 1.8; st.shake = 1.4; st.flashV = 0.8;
    Audio.hurt();
    if (st.collected > 0 && st.lastTaken >= 0 && st.fragments[st.lastTaken]) {
      const f = st.fragments[st.lastTaken];
      f.taken = false;
      const t = farTile(st.x, st.y, 9);
      if (t) { f.x = (t % MAP_W) + 0.5; f.y = ((t / MAP_W) | 0) + 0.5; }
      st.collected--;
      st.lastTaken = st.fragments.reduce((acc, x, i) => (x.taken ? i : acc), -1);
      syncPips();
      if (st.exitOpen) { st.exitOpen = false; st.mapTruth = false; elMiniWrap.classList.remove('truth'); elMiniCap.textContent = 'карта · доверяй на свой страх'; }
      say('Наблюдатель забрал осколок и перепрятал его. <em>Компас уже ищет новый.</em>', 5200, true);
    } else {
      const t = farTile(st.x, st.y, 12);
      if (t) { st.x = (t % MAP_W) + 0.5; st.y = ((t / MAP_W) | 0) + 0.5; st.lastTile = t; }
      say('Он перенёс тебя. Ты не видел как — в этом и была его цель.', 5200, true);
    }
    const t2 = farTile(a.x, a.y, 16);
    if (t2) { a.x = (t2 % MAP_W) + 0.5; a.y = ((t2 / MAP_W) | 0) + 0.5; a.awake = false; a.path.length = 0; }
    once('caught1', () => hint('оно не понимает дверей ◊ — за закрытой взглядом дверью ты в безопасности', 7000));
    if (st.stats.catches === 2 && st.angels.length < 3) {
      const t3 = farTile(st.x, st.y, 18);
      if (t3) {
        st.angels.push({ x: (t3 % MAP_W) + 0.5, y: ((t3 / MAP_W) | 0) + 0.5, path: [], repath: 0, observed: false, wasObs: false, still: 0, awake: false, speed: 1.55, wander: null, jitter: 0, id: 9 });
        say('Их стало <b>двое</b>. Нет. Трое.', 4600, true);
      }
    }
  }

  /* ==========================================================================
     РЕНДЕР
     ========================================================================== */
  const sprites = [];
  const halos = [];

  function render(dt) {
    const W = iw, H = ih;
    const tex = st.tex, grid = st.grid, zone = st.zoneGrid;
    const t = performance.now() / 1000;

    syncCam();
    hor = (H >> 1) + st.pitch + st.bobPx;

    for (let x = 0; x < W; x++) {
      const c = Math.abs(2 * (x + 0.5) / W - 1);
      coneArr[x] = 0.40 + 0.60 * Math.pow(1 - c, 1.05);
    }

    const zi = zoneAt(st.x, st.y);
    const zf = ZONES[zi].fog, zl = ZONES[zi].light;
    const fogR = zf[0], fogG = zf[1], fogB = zf[2];
    const fogC = pack(fogR, fogG, fogB);
    const dreadDim = 1 - st.dread * 0.45;
    st.flick = lerp(st.flick, 1 + (st.rnd() - 0.5) * (0.06 + st.dread * 0.5), 0.4);
    const amb = 0.095 * dreadDim;
    const fl = Math.min(0.905, 0.905 * dreadDim * st.flick);

    /* ---------- потолок: плоский градиент ---------- */
    const hTop = clamp(Math.ceil(hor), 0, H);
    for (let y = 0; y < hTop; y++) {
      const p = hor - y;
      let c;
      if (p < 0.7) c = fogC;
      else {
        const rd = (0.5 * H) / p;
        const fi = fogIdx(rd);
        const f = lutFog[fi];
        const lit = amb + fl * lutLit[fi] * 0.8;
        const cr = zl[0] * 0.10, cg = zl[1] * 0.10, cb = zl[2] * 0.11;
        c = pack(
          (cr * lit * (1 - f) + fogR * f) | 0,
          (cg * lit * (1 - f) + fogG * f) | 0,
          (cb * lit * (1 - f) + fogB * f) | 0
        );
      }
      buf32.fill(c, y * W, y * W + W);
    }

    /* ---------- пол ---------- */
    const rdx0 = dirX - planeX, rdy0 = dirY - planeY;
    const rdx1 = dirX + planeX, rdy1 = dirY + planeY;
    const texFloor = tex.floor, decals = st.decals, decalHue = st.decalHue;
    const dRune = tex.decalRune, dPort = tex.decalPortal;
    const pulse = 0.6 + 0.4 * Math.sin(t * 2.1);
    for (let y = Math.max(0, Math.floor(hor) + 1); y < H; y++) {
      const p = y - hor;
      const off = y * W;
      if (p < 0.7) { buf32.fill(fogC, off, off + W); continue; }
      const rd = (0.5 * H) / p;
      if (rd > MAX_FLOOR) { buf32.fill(fogC, off, off + W); continue; }
      const fi = fogIdx(rd);
      const f = lutFog[fi], invF = 1 - f;
      const dl = lutLit[fi];
      let fx = st.x + rd * rdx0, fy = st.y + rd * rdy0;
      const sx = rd * (rdx1 - rdx0) / W, sy = rd * (rdy1 - rdy0) / W;
      const fR = fogR * f, fG = fogG * f, fB = fogB * f;
      for (let x = 0; x < W; x++, fx += sx, fy += sy) {
        const tx = Math.floor(fx), ty = Math.floor(fy);
        if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) { buf32[off + x] = fogC; continue; }
        const cell = ty * MAP_W + tx;
        const ft = texFloor[zone[cell]];
        const ti = ((((fy - ty) * TEX) | 0) & 63) * TEX + ((((fx - tx) * TEX) | 0) & 63);
        const lit = amb + fl * dl * coneArr[x];
        let r = ft.r[ti] * lit, g = ft.g[ti] * lit, b = ft.b[ti] * lit;
        const dc = decals[cell];
        if (dc) {
          const dt2 = dc === 1 ? dRune : dPort[decalHue[cell]];
          const aa = dt2.a[ti] / 255 * (dc === 1 ? 0.55 + 0.35 * pulse : 0.75);
          r += dt2.r[ti] * aa; g += dt2.g[ti] * aa; b += dt2.b[ti] * aa;
          if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
        }
        buf32[off + x] = pack((r * invF + fR) | 0, (g * invF + fG) | 0, (b * invF + fB) | 0);
      }
    }

    /* ---------- стены ---------- */
    const qOff = ((t * 9) | 0) & 63;
    for (let x = 0; x < W; x++) {
      const cx = 2 * (x + 0.5) / W - 1;
      const rdx = dirX + planeX * cx, rdy = dirY + planeY * cx;
      let mapX = Math.floor(st.x), mapY = Math.floor(st.y);
      const ddx = rdx === 0 ? 1e30 : Math.abs(1 / rdx);
      const ddy = rdy === 0 ? 1e30 : Math.abs(1 / rdy);
      let stepX, stepY, sideDistX, sideDistY;
      if (rdx < 0) { stepX = -1; sideDistX = (st.x - mapX) * ddx; } else { stepX = 1; sideDistX = (mapX + 1 - st.x) * ddx; }
      if (rdy < 0) { stepY = -1; sideDistY = (st.y - mapY) * ddy; } else { stepY = 1; sideDistY = (mapY + 1 - st.y) * ddy; }
      let side = 0, code = T.WALL_A, guard = 0;
      while (guard++ < 320) {
        if (sideDistX < sideDistY) { sideDistX += ddx; mapX += stepX; side = 0; }
        else { sideDistY += ddy; mapY += stepY; side = 1; }
        if (!inB(mapX, mapY)) { code = T.WALL_A; break; }
        const c = grid[I(mapX, mapY)];
        if (c === T.EMPTY) continue;
        if (c === T.QDOOR && st.qState[I(mapX, mapY)] !== 1) continue;
        code = c; break;
      }
      const perp = side === 0 ? (sideDistX - ddx) : (sideDistY - ddy);
      const dist = perp < 0.02 ? 0.02 : perp;
      zbuf[x] = dist;
      if (dist > MAX_WALL) continue;

      const lineH = H / dist;
      const y0 = hor - lineH * 0.5;
      let ds = Math.ceil(y0), de = Math.floor(y0 + lineH);
      if (ds < 0) ds = 0;
      if (de > H) de = H;
      if (de <= ds) continue;

      let wallX = side === 0 ? st.y + dist * rdy : st.x + dist * rdx;
      wallX -= Math.floor(wallX);
      let texX = (wallX * TEX) | 0;
      if ((side === 0 && rdx > 0) || (side === 1 && rdy < 0)) texX = TEX - texX - 1;
      texX &= 63;

      const tt = code === T.EXIT
        ? (st.exitOpen ? tex.exitOpen : tex.exitClosed[clamp(st.collected, 0, 5)])
        : code === T.QDOOR ? tex.qdoor : tex.wall[code - 1];
      const vShift = code === T.QDOOR ? qOff : 0;

      const fi = fogIdx(dist);
      const f = lutFog[fi], invF = 1 - f;
      let lit = (amb + fl * lutLit[fi] * coneArr[x]) * (side === 1 ? 0.72 : 1);
      // двери живут сами: их видно и без фонаря — иначе механика не читается
      if (code === T.QDOOR) lit = Math.max(lit, 0.46);
      else if (code === T.EXIT) lit = Math.max(lit, st.exitOpen ? 0.82 : 0.40);
      if (lit > 1) lit = 1;
      const step = TEX / lineH;
      let texPos = (ds - y0) * step;
      const fR = fogR * f, fG = fogG * f, fB = fogB * f;
      let o = ds * W + x;
      for (let y = ds; y < de; y++, o += W) {
        const ti = (((texPos | 0) + vShift) & 63) * TEX + texX;
        texPos += step;
        buf32[o] = pack((tt.r[ti] * lit * invF + fR) | 0, (tt.g[ti] * lit * invF + fG) | 0, (tt.b[ti] * lit * invF + fB) | 0);
      }
    }

    /* ---------- спрайты ---------- */
    sprites.length = 0;
    for (const f of st.fragments) if (!f.taken) {
      const bob = Math.sin(f.phase) * 0.06;
      sprites.push({ x: f.x, y: f.y, t: tex.frag, zB: 0.30 + bob, zT: 0.76 + bob, emit: 1, fogMul: 0.45, glow: [150, 230, 255, 0.55], halo: 1.0 });
    }
    for (const p of st.portals) {
      sprites.push({ x: p.ax + 0.5, y: p.ay + 0.5, t: tex.portal[p.hue], zB: 0.0, zT: 1.32, emit: 1, fogMul: 0.5, glow: hueRGB(p.hue, 0.45), halo: 1.15 });
      sprites.push({ x: p.bx + 0.5, y: p.by + 0.5, t: tex.portal[p.hue], zB: 0.0, zT: 1.32, emit: 1, fogMul: 0.5, glow: hueRGB(p.hue, 0.45), halo: 1.15 });
    }
    for (const a of st.angels) if (a.awake) {
      sprites.push({ x: a.x, y: a.y, t: tex.angel, zB: 0, zT: 1.62, emit: 0.30 + st.dread * 0.1, fogMul: 0.8, glow: [255, 60, 40, 0.18], halo: 0.7, angel: a });
    }
    if (st.exitOpen) {
      const ex = st.world.exit;
      sprites.push({ x: ex.x + 0.5, y: ex.y + 0.5, t: tex.rune, zB: 0.95, zT: 1.45, emit: 1, fogMul: 0.35, glow: [200, 240, 255, 0.6], halo: 1.3 });
    }

    halos.length = 0;
    const invDet = 1 / (planeX * dirY - dirX * planeY);
    for (const s of sprites) {
      const sx = s.x - st.x, sy = s.y - st.y;
      s.d = sx * sx + sy * sy;
    }
    sprites.sort((a, b) => b.d - a.d);

    for (const s of sprites) {
      const sx = s.x - st.x, sy = s.y - st.y;
      const trX = invDet * (dirY * sx - dirX * sy);
      const trY = invDet * (-planeY * sx + planeX * sy);
      if (trY < 0.22) continue;
      const K = H / trY;
      const scrX = (W * 0.5) * (1 + trX / trY);
      const yTop = hor - (s.zT - 0.5) * K;
      const yBot = hor - (s.zB - 0.5) * K;
      const hpx = yBot - yTop;
      if (hpx < 0.7) continue;
      const tw = s.t.w, th = s.t.h;
      const wpx = hpx * (tw / th);
      const left = scrX - wpx * 0.5;
      let x0 = Math.ceil(left), x1 = Math.floor(left + wpx);
      if (x0 < 0) x0 = 0;
      if (x1 >= W) x1 = W - 1;
      let y0 = Math.ceil(yTop), y1 = Math.floor(yBot);
      if (y0 < 0) y0 = 0;
      if (y1 >= H) y1 = H - 1;
      if (x1 < x0 || y1 < y0) continue;

      const fi = fogIdx(trY);
      const f = lutFog[fi] * s.fogMul, invF = 1 - f;
      const sceneLit = amb + fl * lutLit[fi];
      const baseLit = Math.min(1, lerp(sceneLit, s.emit, s.emit));
      const fR = fogR * f, fG = fogG * f, fB = fogB * f;
      const jitter = s.angel ? Math.sin(s.angel.jitter * 22) * 1.4 * st.dread : 0;

      for (let x = x0; x <= x1; x++) {
        if (zbuf[x] <= trY) continue;
        const txs = clamp((((x - left) * tw / wpx) | 0), 0, tw - 1);
        let o = y0 * W + x;
        for (let y = y0; y <= y1; y++, o += W) {
          const tys = clamp((((y - yTop + jitter) * th / hpx) | 0), 0, th - 1);
          const ti = tys * tw + txs;
          const al = s.t.a[ti];
          if (al < 8) continue;
          const af = al / 255;
          let r = s.t.r[ti] * baseLit, g = s.t.g[ti] * baseLit, b = s.t.b[ti] * baseLit;
          r = r * invF + fR; g = g * invF + fG; b = b * invF + fB;
          if (af < 0.985) {
            const dst = buf32[o];
            const dr = LE ? dst & 255 : (dst >>> 24) & 255;
            const dg = LE ? (dst >>> 8) & 255 : (dst >>> 16) & 255;
            const db = LE ? (dst >>> 16) & 255 : (dst >>> 8) & 255;
            r = dr + (r - dr) * af; g = dg + (g - dg) * af; b = db + (b - db) * af;
          }
          buf32[o] = pack(r | 0, g | 0, b | 0);
        }
      }
      if (s.glow && trY < 18) halos.push({ x: scrX, y: (yTop + yBot) * 0.5, r: hpx * s.halo * 0.85, c: s.glow, d: trY });
    }

    /* ---------- вывод ---------- */
    ctx.putImageData(img, 0, 0);

    ctx.globalCompositeOperation = 'lighter';
    for (const h of halos) {
      if (h.r < 1) continue;
      const a = clamp(h.c[3] * (1 - h.d / 20), 0, 1);
      if (a <= 0.01) continue;
      const g = ctx.createRadialGradient(h.x, h.y, 0, h.x, h.y, h.r);
      g.addColorStop(0, `rgba(${h.c[0]},${h.c[1]},${h.c[2]},${(a * 0.85).toFixed(3)})`);
      g.addColorStop(0.45, `rgba(${h.c[0]},${h.c[1]},${h.c[2]},${(a * 0.28).toFixed(3)})`);
      g.addColorStop(1, `rgba(${h.c[0]},${h.c[1]},${h.c[2]},0)`);
      ctx.fillStyle = g;
      ctx.fillRect(h.x - h.r, h.y - h.r, h.r * 2, h.r * 2);
    }
    ctx.globalCompositeOperation = 'source-over';

    if (st.glitch > 0.02) {
      const n = (st.glitch * 16) | 0;
      for (let k = 0; k < n; k++) {
        const gy = (st.rnd() * H) | 0;
        const gh = 1 + ((st.rnd() * 9) | 0);
        const gx = ((st.rnd() - 0.5) * 46 * st.glitch) | 0;
        if (gy + gh > H) continue;
        ctx.drawImage(view, 0, gy, W, gh, gx, gy, W, gh);
      }
      ctx.globalCompositeOperation = 'lighter';
      ctx.globalAlpha = 0.22 * st.glitch;
      ctx.drawImage(view, ((st.rnd() - 0.5) * 12 * st.glitch) | 0, 0);
      ctx.globalAlpha = 1;
      ctx.globalCompositeOperation = 'source-over';
    }

    drawMinimap();
  }

  function hueRGB(hueIdx, alpha) {
    const h = NE.Textures.PORTAL_HUES[hueIdx] || 190;
    const c = hsl2rgb(h / 360, 0.9, 0.62);
    return [c[0], c[1], c[2], alpha];
  }
  function hsl2rgb(h, s, l) {
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
    const f = (t) => {
      if (t < 0) t += 1; if (t > 1) t -= 1;
      if (t < 1 / 6) return p + (q - p) * 6 * t;
      if (t < 1 / 2) return q;
      if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
      return p;
    };
    return [(f(h + 1 / 3) * 255) | 0, (f(h) * 255) | 0, (f(h - 1 / 3) * 255) | 0];
  }

  /* ---------- миникарта ---------- */
  function drawMinimap() {
    const S2 = mini.width, half = S2 / 2;
    mctx.clearRect(0, 0, S2, S2);
    mctx.fillStyle = 'rgba(3,7,10,.85)';
    mctx.fillRect(0, 0, S2, S2);
    const sc = 4;
    mctx.save();
    mctx.translate(half, half);
    mctx.rotate(st.mapLie);
    mctx.translate(-st.x * sc, -st.y * sc);
    mctx.drawImage(mapCanvas, 0, 0);
    // порталы — видны только рядом или когда карта честная
    for (const p of st.portals) {
      const d = Math.min(Math.hypot(p.ax - st.x, p.ay - st.y), Math.hypot(p.bx - st.x, p.by - st.y));
      if (!st.mapTruth && d > 8) continue;
      const hue = NE.Textures.PORTAL_HUES[p.hue];
      mctx.strokeStyle = `hsla(${hue},95%,65%,.9)`;
      mctx.lineWidth = 0.8;
      mctx.beginPath();
      mctx.arc(p.ax * sc + 2, p.ay * sc + 2, 2.6, 0, 7); mctx.stroke();
      mctx.beginPath();
      mctx.arc(p.bx * sc + 2, p.by * sc + 2, 2.6, 0, 7); mctx.stroke();
      if (st.mapTruth) {
        mctx.strokeStyle = `hsla(${hue},95%,65%,.35)`;
        mctx.beginPath();
        mctx.moveTo(p.ax * sc + 2, p.ay * sc + 2);
        mctx.lineTo(p.bx * sc + 2, p.by * sc + 2);
        mctx.stroke();
      }
    }
    // выход
    if (st.mapTruth && st.world.exit) {
      const ex = st.world.exit;
      mctx.fillStyle = st.exitOpen ? 'rgba(255,255,255,.95)' : 'rgba(255,196,106,.75)';
      mctx.fillRect(ex.x * sc, ex.y * sc, sc, sc);
    }
    mctx.restore();
    // игрок — всегда в центре
    mctx.save();
    mctx.translate(half, half);
    mctx.rotate(st.ang + Math.PI / 2);
    mctx.fillStyle = '#eaf7ff';
    mctx.beginPath();
    mctx.moveTo(0, -5.4); mctx.lineTo(3.6, 4.2); mctx.lineTo(0, 2.2); mctx.lineTo(-3.6, 4.2);
    mctx.closePath(); mctx.fill();
    mctx.restore();
    // рамка-компас
    mctx.strokeStyle = 'rgba(127,233,255,.14)';
    mctx.lineWidth = 1;
    mctx.strokeRect(0.5, 0.5, S2 - 1, S2 - 1);
  }

  /* ==========================================================================
     ФИНАЛ
     ========================================================================== */
  function win() {
    if (st.mode !== 'play') return;
    st.mode = 'ending';
    Audio.win();
    Audio.ambience(false);
    st.flashV = 1.4; st.glitch = 2.2;
    document.exitPointerLock && document.exitPointerLock();
    hud.classList.remove('on');
    showEnding();
  }

  function fmtTime(s) {
    const m = Math.floor(s / 60), ss = Math.floor(s % 60);
    return m + ':' + (ss < 10 ? '0' : '') + ss;
  }

  function showEnding() {
    const s = st.stats;
    const ps = [
      'Наблюдатели не злые. Они просто не выносят, когда на них смотрят. Знакомо.',
      'Геометрия не обязана быть честной. Ты — тоже.',
      'Пока ты это читал, лабиринт уже собрался заново. Он не обидчивый.',
      'Компас указывал на осколки. Карта указывала на твоё самомнение.',
      'Ты вышел наружу. Наружу — это тоже чей-то лабиринт.'
    ][Math.floor(st.rnd() * 5)];

    const text =
`Лабиринт отпустил тебя через ${fmtTime(s.time)}.

Шагов: ${Math.round(s.steps)}. Разломов пройдено: ${s.teleports}.
Взглядов выдержано: ${s.seen}. Встреч с Наблюдателем: ${s.catches}.
Осколков: 5 из 5.

Карта всё это время врала: после каждого разлома она
поворачивала мир на случайный угол и делала вид, что так и надо.
Врать она перестала ровно тогда, когда реальность снова стала целой.`;

    elEnd.classList.remove('hidden');
    $('endStats').innerHTML = [
      ['время', fmtTime(s.time)], ['шагов', Math.round(s.steps)],
      ['разломов', s.teleports], ['встреч', s.catches],
      ['осколков', '5 / 5'], ['сид', st.seed]
    ].map(([k, v]) => `<div><b>${k}</b><span>${v}</span></div>`).join('');
    $('psLine').textContent = 'P.S. ' + ps;
    drawTrueMap();

    const el = $('endText');
    el.innerHTML = '<span class="cur">▌</span>';
    let i = 0;
    clearInterval(showEnding._t);
    showEnding._t = setInterval(() => {
      i += 2;
      if (i >= text.length) { clearInterval(showEnding._t); el.textContent = text; return; }
      el.innerHTML = text.slice(0, i).replace(/</g, '&lt;') + '<span class="cur">▌</span>';
    }, 16);
  }

  function drawTrueMap() {
    const c = $('trueMap'), g = c.getContext('2d');
    const sc = c.width / MAP_W;
    g.fillStyle = '#04070a'; g.fillRect(0, 0, c.width, c.height);
    const zc = ['#2b4a5c', '#5c4326', '#4a2b5c', '#2b5c46'];
    const zf = ['#0d1a22', '#1e1509', '#1a0d22', '#0d1e17'];
    for (let y = 0; y < MAP_H; y++) {
      for (let x = 0; x < MAP_W; x++) {
        const i = I(x, y), cd = st.grid[i], z = st.zoneGrid[i];
        g.fillStyle = cd === T.EMPTY || cd === T.QDOOR ? zc[z] : zf[z];
        g.fillRect(x * sc, y * sc, sc + 0.6, sc + 0.6);
      }
    }
    // связи разломов
    for (const p of st.portals) {
      const hue = NE.Textures.PORTAL_HUES[p.hue];
      g.strokeStyle = `hsla(${hue},95%,62%,.85)`;
      g.lineWidth = 2;
      g.setLineDash([6, 5]);
      g.beginPath();
      g.moveTo((p.ax + 0.5) * sc, (p.ay + 0.5) * sc);
      g.lineTo((p.bx + 0.5) * sc, (p.by + 0.5) * sc);
      g.stroke();
      g.setLineDash([]);
      g.fillStyle = `hsla(${hue},95%,68%,1)`;
      [[p.ax, p.ay], [p.bx, p.by]].forEach((t) => {
        g.beginPath(); g.arc((t[0] + 0.5) * sc, (t[1] + 0.5) * sc, 4.5, 0, 7); g.fill();
      });
    }
    // квантовые двери
    g.fillStyle = '#ff6ad5';
    for (const q of st.qdoors) g.fillRect(q.x * sc + 2, q.y * sc + 2, sc - 4, sc - 4);
    // выход
    const ex = st.world.exit;
    g.fillStyle = '#ffe9a8';
    g.fillRect(ex.x * sc, ex.y * sc, sc, sc);
    // путь игрока
    g.strokeStyle = 'rgba(127,233,255,.55)';
    g.lineWidth = 1.6;
    g.beginPath();
    for (let k = 0; k < st.trail.length; k += 2) {
      const X = st.trail[k] * sc, Y = st.trail[k + 1] * sc;
      k ? g.lineTo(X, Y) : g.moveTo(X, Y);
    }
    g.stroke();
    // осколки
    for (const f of st.fragments) {
      g.strokeStyle = 'rgba(255,255,255,.9)'; g.lineWidth = 1.6;
      g.beginPath();
      g.moveTo(f.x * sc, f.y * sc - 5); g.lineTo(f.x * sc + 4, f.y * sc);
      g.lineTo(f.x * sc, f.y * sc + 5); g.lineTo(f.x * sc - 4, f.y * sc);
      g.closePath(); g.stroke();
    }
    // старт
    g.fillStyle = '#7fe9ff';
    g.beginPath(); g.arc(st.world.start.x * sc, st.world.start.y * sc, 5, 0, 7); g.fill();
  }

  /* ==========================================================================
     ЭКРАНЫ
     ========================================================================== */
  function startGame() {
    Audio.init(); Audio.resume(); Audio.ambience(true);
    elMenu.classList.add('hidden');
    elPause.classList.add('hidden');
    elEnd.classList.add('hidden');
    clearInterval(showEnding._t);
    hud.classList.add('on');
    st.mode = 'play';
    syncPips();
    tryLock();
  }

  function doPause() {
    if (st.mode !== 'play') return;
    st.mode = 'pause';
    Audio.ambience(false);
    document.exitPointerLock && document.exitPointerLock();
    $('pauseStats').innerHTML = [
      ['время', fmtTime(st.stats.time)], ['осколки', st.collected + ' / 5'],
      ['разломы', st.stats.teleports], ['встречи', st.stats.catches]
    ].map(([k, v]) => `<div><b>${k}</b><span>${v}</span></div>`).join('');
    elPause.classList.remove('hidden');
  }

  function resume() {
    elPause.classList.add('hidden');
    st.mode = 'play';
    Audio.resume(); Audio.ambience(true);
    tryLock();
  }

  function newGame(seed) {
    elEnd.classList.add('hidden');
    elPause.classList.add('hidden');
    elMenu.classList.add('hidden');
    clearInterval(showEnding._t);
    loadWorld(seed == null ? (Math.random() * 1e9) | 0 : seed);
    elMiniWrap.classList.remove('truth');
    elMiniCap.textContent = 'карта · доверяй на свой страх';
    st.mode = 'play';
    hud.classList.add('on');
    Audio.init(); Audio.resume(); Audio.ambience(true);
    syncPips();
    st.shown = {};
    tutorial();
    tryLock();
  }

  let muted = false;
  function toggleMute() {
    muted = !muted;
    Audio.setMuted(muted);
    const b = $('btnSound');
    if (b) b.textContent = 'звук: ' + (muted ? 'выкл' : 'вкл');
  }

  /* ==========================================================================
     ГЛАВНЫЙ ЦИКЛ
     ========================================================================== */
  let last = 0;
  function frame(now) {
    requestAnimationFrame(frame);
    if (!st.world) return;
    let dt = (now - last) / 1000;
    last = now;
    if (!isFinite(dt) || dt <= 0) dt = 0.016;
    dt = Math.min(dt, 0.05);

    if (st.mode === 'play') update(dt);
    else if (st.mode === 'ending') {
      st.glitch = Math.max(0, st.glitch - dt * 0.9);
      st.flashV = Math.max(0, st.flashV - dt * 1.1);
      elFlash.style.opacity = (st.flashV * 0.8).toFixed(3);
      st.ang += dt * 0.05;
    }

    resize();
    render(dt);

    // адаптивное разрешение
    st.frames++; st.ft += dt * 1000;
    if (st.frames >= 45) {
      const avg = st.ft / st.frames;
      st.fps = 1000 / Math.max(1, avg);
      if (avg > 21 && st.quality > 0.62) st.quality = Math.max(0.62, st.quality - 0.12);
      else if (avg < 12.5 && st.quality < 1) { st.quality = Math.min(1, st.quality + 0.08); }
      st.frames = 0; st.ft = 0;
    }
  }

  /* ==========================================================================
     СТАРТ
     ========================================================================== */
  function boot() {
    for (let i = 0; i < 5; i++) elPips.appendChild(document.createElement('i'));
    bindInput();

    // зерно плёнки
    const gc = document.createElement('canvas'); gc.width = gc.height = 140;
    const gg = gc.getContext('2d');
    const id = gg.createImageData(140, 140);
    const rr = mulberry32(7);
    for (let i = 0; i < id.data.length; i += 4) {
      const v = 120 + rr() * 135;
      id.data[i] = id.data[i + 1] = id.data[i + 2] = v;
      id.data[i + 3] = 255;
    }
    gg.putImageData(id, 0, 0);
    elGrain.style.backgroundImage = `url(${gc.toDataURL()})`;

    const seed = (Math.random() * 1e9) | 0;
    $('seedShow').textContent = seed;
    resize();
    loadWorld(seed);
    st.mode = 'menu';

    $('btnStart').addEventListener('click', () => { Audio.ui(true); startGame(); });
    $('btnResume').addEventListener('click', () => { Audio.ui(true); resume(); });
    $('btnNew').addEventListener('click', () => { Audio.ui(true); newGame(); });
    $('btnSound').addEventListener('click', () => { toggleMute(); Audio.ui(true); });
    $('btnAgain').addEventListener('click', () => { Audio.ui(true); newGame(); });
    $('btnMenu').addEventListener('click', () => {
      Audio.ui(false);
      clearInterval(showEnding._t);
      elEnd.classList.add('hidden');
      elMenu.classList.add('hidden');
      newGame();
    });

    requestAnimationFrame((t) => { last = t; requestAnimationFrame(frame); });
  }

  /* ---------- отладочный доступ (нужен headless-тестам и тебе, если захочешь покопаться) ---------- */
  NE.debug = {
    get st() { return st; }, get keys() { return keys; },
    get iw() { return iw; }, get ih() { return ih; }, get img() { return img; },
    get hor() { return hor; }, get zbuf() { return zbuf; },
    get sprites() { return sprites; }, get halos() { return halos; },
    say, hint, loadWorld, newGame, startGame, doPause, resume, win, caught,
    takeFragment, usePortal, openExit, update, render, resize, syncCam,
    computeObservation, solidMove, solidRay, canOccupy, observed, drawMinimap,
    fmtTime, showEnding, drawTrueMap, farTile
  };

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
  else boot();

})(typeof window !== 'undefined' ? window : globalThis);
