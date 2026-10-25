/* ============================================================
   СРЕЗ / SLICE — игровая оболочка
   ============================================================ */
(function () {
  'use strict';
  const WCOL = Render.WCOL;
  const WHEX = ['#38e8d6', '#ffbe54', '#ff5c9e', '#ac8aff'];
  const WNAME = ['ГЛУБИНА 0 · ЯДРО', 'ГЛУБИНА 1 · РАЗЛОМ', 'ГЛУБИНА 2 · ОБРАТНАЯ СТОРОНА', 'ГЛУБИНА 3 · ТИШИНА'];
  const FOV = 0.95;
  const EYE = 1.62;
  const RAD = 0.29;
  const WALK = 3.35, RUN = 5.6;

  const world = World.makeWorld(20261025);
  const occ = world.occ, P = world.P, S = world.S, lamps = world.lamps;
  const N = world.N;

  /* ---------------------------------------------------------- canvases */
  const view = document.getElementById('view');
  const vctx = view.getContext('2d', { alpha: false });
  let VW = 1, VH = 1, DPR = 1;
  const low = document.createElement('canvas');
  const lctx = low.getContext('2d', { alpha: false });
  let RW = 2, RH = 2, img = null, pix = null, dep = null;
  let scale = matchMedia('(pointer:coarse)').matches ? 0.38 : 0.5;

  function resize() {
    DPR = Math.min(window.devicePixelRatio || 1, 1.5);
    VW = view.width = Math.floor(window.innerWidth * DPR);
    VH = view.height = Math.floor(window.innerHeight * DPR);
    view.style.width = window.innerWidth + 'px';
    view.style.height = window.innerHeight + 'px';
    allocScale(scale);
    vctx.imageSmoothingEnabled = false;
  }
  function allocScale(s) {
    scale = Math.max(0.2, Math.min(0.9, s));
    // не больше ~380k пикселей на кадр
    const maxPx = 380000;
    if (VW * VH * scale * scale > maxPx) scale = Math.sqrt(maxPx / (VW * VH));
    RW = Math.max(120, Math.round(VW * scale));
    RH = Math.max(80, Math.round(VH * scale));
    low.width = RW; low.height = RH;
    img = lctx.createImageData(RW, RH);
    pix = new Uint32Array(img.data.buffer);
    dep = new Float32Array(RW * RH);
    lctx.imageSmoothingEnabled = false;
  }
  addEventListener('resize', resize);

  /* ---------------------------------------------------------- state */
  const G = {
    mode: 'intro',          // intro | play | pause | win
    x: world.SPAWN.x, z: world.SPAWN.z, y: EYE,
    yaw: 0, pitch: 0,
    w: 0,
    vx: 0, vz: 0, bob: 0, moveDist: 0,
    frags: [false, false, false, false],
    shifts: 0, time: 0, t0: 0,
    glitch: 0, punch: 0, shake: 0,
    demoT: 0, demoW: 0,
  };
  let mouseDX = 0, mouseDY = 0, locked = false;

  function reset() {
    G.x = world.SPAWN.x; G.z = world.SPAWN.z; G.y = EYE;
    G.yaw = 0; G.pitch = 0; G.w = 0;
    G.frags = [false, false, false, false];
    G.shifts = 0; G.time = 0; G.t0 = performance.now();
    G.glitch = 0; G.punch = 0; G.shake = 0; G.vx = 0; G.vz = 0;
    G.moveDist = 0; G.done = false;
    updateDepthHUD(); updateFragHUD();
  }

  /* ---------------------------------------------------------- input */
  const keys = Object.create(null);
  addEventListener('keydown', (e) => {
    const k = e.key.length === 1 ? e.key.toLowerCase() : e.key.toLowerCase();
    keys[k] = true;
    if ([' ', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright', 'tab'].includes(k)) e.preventDefault();
    if (G.mode === 'play') {
      if (k === 'e' || k === 'arrowup' || k === ' ') tryShift(1);
      if (k === 'q' || k === 'arrowdown' || k === 'z') tryShift(-1);
      if (k === 'escape') setPause(true);
      if (k === 'm') toggleMute();
      if (k === 'b') bloomOn = !bloomOn;
    } else if (G.mode === 'pause' && k === 'escape') setPause(false);
  });
  addEventListener('keyup', (e) => { keys[e.key.toLowerCase()] = false; });
  addEventListener('blur', () => { for (const k in keys) keys[k] = false; });

  view.addEventListener('mousedown', (e) => {
    if (G.mode === 'intro' || G.mode === 'win') return;
    if (G.mode === 'pause') { setPause(false); return; }
    if (!locked) { lockPointer(); return; }
    if (e.button === 0) tryShift(1);
    if (e.button === 2) tryShift(-1);
  });
  view.addEventListener('contextmenu', (e) => e.preventDefault());
  addEventListener('mousemove', (e) => {
    if (!locked) return;
    mouseDX += e.movementX; mouseDY += e.movementY;
  });
  document.addEventListener('pointerlockchange', () => {
    locked = document.pointerLockElement === view;
    if (!locked && G.mode === 'play') setPause(true);
  });
  function lockPointer() {
    if (view.requestPointerLock) { try { view.requestPointerLock(); } catch (e) {} }
  }
  document.addEventListener('visibilitychange', () => { if (document.hidden && G.mode === 'play') setPause(true); });

  /* ---------------------------------------------------------- touch */
  const touchWrap = document.getElementById('touch');
  const stick = document.getElementById('stick'), knob = document.getElementById('knob');
  const tmove = { x: 0, y: 0 };
  let stickId = null, lookId = null, lastLook = { x: 0, y: 0 };
  function isTouch() { return matchMedia('(pointer:coarse)').matches; }
  if (isTouch()) touchWrap.classList.add('on');
  stick.addEventListener('touchstart', (e) => {
    const t = e.changedTouches[0]; stickId = t.identifier; e.preventDefault();
  }, { passive: false });
  addEventListener('touchmove', (e) => {
    for (const t of e.changedTouches) {
      if (t.identifier === stickId) {
        const r = stick.getBoundingClientRect();
        const dx = t.clientX - (r.left + r.width / 2), dy = t.clientY - (r.top + r.height / 2);
        const m = Math.min(1, Math.hypot(dx, dy) / (r.width * 0.42));
        const a = Math.atan2(dy, dx);
        tmove.x = Math.cos(a) * m; tmove.y = -Math.sin(a) * m;
        knob.style.transform = `translate(${Math.cos(a) * m * 30}px,${Math.sin(a) * m * 30}px)`;
      } else if (lookId === null && t.clientX > window.innerWidth * 0.4) {
        lookId = t.identifier; lastLook = { x: t.clientX, y: t.clientY };
      } else if (t.identifier === lookId) {
        G.yaw -= (t.clientX - lastLook.x) * 0.005;
        G.pitch = clamp(G.pitch - (t.clientY - lastLook.y) * 0.005, -0.9, 0.9);
        lastLook = { x: t.clientX, y: t.clientY };
      }
    }
  }, { passive: true });
  addEventListener('touchend', (e) => {
    for (const t of e.changedTouches) {
      if (t.identifier === stickId) { stickId = null; tmove.x = tmove.y = 0; knob.style.transform = ''; }
      if (t.identifier === lookId) lookId = null;
    }
  });
  document.getElementById('bwup').addEventListener('touchstart', (e) => { e.preventDefault(); tryShift(1); }, { passive: false });
  document.getElementById('bwdn').addEventListener('touchstart', (e) => { e.preventDefault(); tryShift(-1); }, { passive: false });
  document.getElementById('bjump').addEventListener('touchstart', (e) => { e.preventDefault(); probeFlash(); }, { passive: false });

  /* ---------------------------------------------------------- audio */
  let AC = null, master = null, muted = false, padOsc = [], padGain = null, padFilter = null;
  let bloomOn = true;
  function initAudio() {
    if (AC) return;
    try { AC = new (window.AudioContext || window.webkitAudioContext)(); } catch (e) { return; }
    master = AC.createGain(); master.gain.value = 0.85; master.connect(AC.destination);
    // атмосферный пэд
    padFilter = AC.createBiquadFilter(); padFilter.type = 'lowpass'; padFilter.frequency.value = 420; padFilter.Q.value = 3;
    padGain = AC.createGain(); padGain.gain.value = 0.0001;
    padFilter.connect(padGain); padGain.connect(master);
    for (let i = 0; i < 3; i++) {
      const o = AC.createOscillator();
      o.type = i === 2 ? 'triangle' : 'sawtooth';
      o.frequency.value = [55, 55.4, 82.5][i];
      o.detune.value = [0, 7, -5][i];
      o.connect(padFilter); o.start();
      padOsc.push(o);
    }
    padGain.gain.linearRampToValueAtTime(0.075, AC.currentTime + 3);
    // медленное дыхание фильтра
    const lfo = AC.createOscillator(), lg = AC.createGain();
    lfo.frequency.value = 0.06; lg.gain.value = 180;
    lfo.connect(lg); lg.connect(padFilter.frequency); lfo.start();
    // шум для шагов
    noiseBuf = AC.createBuffer(1, AC.sampleRate * 0.25, AC.sampleRate);
    const d = noiseBuf.getChannelData(0);
    for (let i = 0; i < d.length; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / d.length);
  }
  let noiseBuf = null;
  function setPadDepth(w) {
    if (!AC || !padOsc.length) return;
    const base = [55, 61.7, 73.4, 87.3][w];
    const now = AC.currentTime;
    padOsc[0].frequency.cancelScheduledValues(now); padOsc[0].frequency.linearRampToValueAtTime(base, now + 0.6);
    padOsc[1].frequency.cancelScheduledValues(now); padOsc[1].frequency.linearRampToValueAtTime(base * 1.007, now + 0.6);
    padOsc[2].frequency.cancelScheduledValues(now); padOsc[2].frequency.linearRampToValueAtTime(base * 1.5, now + 0.6);
    padFilter.frequency.cancelScheduledValues(now);
    padFilter.frequency.linearRampToValueAtTime(360 + w * 170, now + 0.8);
  }
  function toggleMute() {
    muted = !muted;
    if (master) master.gain.linearRampToValueAtTime(muted ? 0 : 0.85, AC.currentTime + 0.15);
    toast(muted ? 'ЗВУК ВЫКЛЮЧЕН' : 'ЗВУК ВКЛЮЧЁН', 1100);
  }
  function tone(freq, dur, type, vol, slideTo, when) {
    if (!AC || muted) return;
    const t = AC.currentTime + (when || 0);
    const o = AC.createOscillator(), g = AC.createGain();
    o.type = type || 'sine';
    o.frequency.setValueAtTime(freq, t);
    if (slideTo) o.frequency.exponentialRampToValueAtTime(Math.max(20, slideTo), t + dur);
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(vol, t + Math.min(0.02, dur * 0.3));
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    o.connect(g); g.connect(master);
    o.start(t); o.stop(t + dur + 0.03);
  }
  function sfxShift(dir) {
    if (!AC) return;
    tone(dir > 0 ? 220 : 300, 0.42, 'sawtooth', 0.07, dir > 0 ? 900 : 90);
    tone(dir > 0 ? 440 : 660, 0.6, 'sine', 0.05, dir > 0 ? 1760 : 220);
    // шум-«схлопывание»
    if (noiseBuf && !muted) {
      const s = AC.createBufferSource(); s.buffer = noiseBuf;
      const f = AC.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = 1400; f.Q.value = 1.2;
      const g = AC.createGain(); g.gain.value = 0.11;
      s.connect(f); f.connect(g); g.connect(master);
      s.playbackRate.value = 1.2; s.start();
    }
  }
  function sfxStep() {
    if (!AC || !noiseBuf || muted) return;
    const s = AC.createBufferSource(); s.buffer = noiseBuf;
    const f = AC.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 520 + Math.random() * 120;
    const g = AC.createGain(); g.gain.value = 0.045;
    s.connect(f); f.connect(g); g.connect(master); s.start();
  }
  function sfxDenied() { tone(120, 0.18, 'square', 0.05, 70); tone(90, 0.22, 'sawtooth', 0.04); }
  function sfxPick() {
    [880, 1174, 1568, 2093].forEach((f, i) => tone(f, 0.42, 'triangle', 0.055, null, i * 0.07));
    tone(2637, 0.5, 'sine', 0.03, null, 0.28);
  }
  function sfxWin() {
    [261.6, 329.6, 392, 523, 659, 784, 1046].forEach((f, i) => tone(f, 1.6, 'triangle', 0.05, null, i * 0.12));
    tone(130.8, 3.2, 'sine', 0.06);
  }

  /* ---------------------------------------------------------- helpers */
  function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
  function solidAt(w, x, z, y0, y1) {
    const xx = Math.floor(x), zz = Math.floor(z);
    if (xx < 0 || zz < 0 || xx >= N || zz >= N) return true;
    const g = world.grid[w];
    for (let y = y0; y <= y1; y++) { const v = g[world.idx(xx, y, zz)]; if (v === 1 || v === 2) return true; }
    return false;
  }
  function bodyBlocked(w, x, z) {
    const r = RAD;
    return solidAt(w, x - r, z - r, 1, 2) || solidAt(w, x + r, z - r, 1, 2) ||
           solidAt(w, x - r, z + r, 1, 2) || solidAt(w, x + r, z + r, 1, 2) ||
           solidAt(w, x, z, 1, 2);
  }
  // биты «мне солидно» (0..3) и «свечение чужих измерений» (4..7) — из occ-маски
  function cellBits(x, z) {
    const xx = Math.floor(x), zz = Math.floor(z);
    if (xx < 0 || zz < 0 || xx >= N || zz >= N) return { solid: 15, ghost: 0 };
    const o = occ[(1 + 1) * S + (zz + 1) * P + (xx + 1)];
    return { solid: o & 15, ghost: (o >> 4) & 15 };
  }

  /* ---------------------------------------------------------- HUD */
  const depthEl = document.getElementById('depth');
  for (let i = 0; i < 4; i++) {
    const d = document.createElement('div');
    d.className = 'dseg'; d.style.color = WHEX[i];
    depthEl.appendChild(d);
  }
  const chipsEl = document.getElementById('chips');
  for (let i = 0; i < 4; i++) {
    const c = document.createElement('div');
    c.className = 'chip'; c.style.setProperty('--c', WHEX[i]); c.textContent = i;
    chipsEl.appendChild(c);
  }
  const dlabel = document.getElementById('dlabel');
  const toastEl = document.getElementById('toast');
  const probeEl = document.getElementById('probe');
  let toastT = null;
  function toast(msg, ms) {
    toastEl.textContent = msg;
    toastEl.classList.toggle('on', !!msg);
    clearTimeout(toastT);
    if (msg) toastT = setTimeout(() => toastEl.classList.remove('on'), ms || 2000);
  }
  function updateDepthHUD() {
    const w = G.w;
    [...depthEl.children].forEach((d, i) => {
      d.style.background = i === w ? WHEX[i] : WHEX[i] + '22';
      d.style.boxShadow = i === w ? '0 0 16px ' + WHEX[i] : 'none';
      d.style.opacity = i === w ? 1 : 0.5;
    });
    dlabel.textContent = WNAME[w];
    dlabel.style.color = WHEX[w];
    document.getElementById('probe').style.color = WHEX[w];
  }
  function updateFragHUD() {
    const n = G.frags.filter(Boolean).length;
    document.getElementById('frag').textContent = n + '/4';
    [...chipsEl.children].forEach((c, i) => c.classList.toggle('got', G.frags[i]));
  }
  function fmt(t) {
    const m = Math.floor(t / 60), s = Math.floor(t % 60);
    return String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0');
  }

  /* ---------------------------------------------------------- shift */
  function tryShift(dir) {
    if (G.mode !== 'play') return;
    const nw = G.w + dir;
    if (nw < 0 || nw > 3) { denied('ЭТО ГРАНИЦА ИЗМЕРЕНИЙ'); return; }
    if (bodyBlocked(nw, G.x, G.z)) { denied('ЗДЕСЬ ГЛУБИНА ЗАНЯТА · ОТОЙДИ ОТ СТЕНЫ'); return; }
    G.w = nw;
    G.shifts++;
    G.glitch = 1; G.punch = 1; G.shake = 1;
    sfxShift(dir);
    setPadDepth(nw);
    updateDepthHUD();
    if (G.shifts === 1) toast('ТЫ СМЕНИЛ СРЕЗ. МИР ТОТ ЖЕ — ГЕОМЕТРИЯ ДРУГАЯ', 3000);
  }
  function denied(msg) {
    G.shake = 0.55; G.glitch = 0.35;
    sfxDenied();
    toast(msg, 1400);
  }

  /* ---------------------------------------------------------- pickups */
  function checkPickups() {
    for (let i = 0; i < 4; i++) {
      const f = world.FRAGS[i];
      if (G.frags[i] || f.w !== G.w) continue;
      const dx = f.x - G.x, dz = f.z - G.z;
      if (dx * dx + dz * dz < 1.1) {
        G.frags[i] = true;
        sfxPick(); G.shake = 0.8; G.glitch = 0.5; G.punch = 0.8;
        updateFragHUD();
        const n = G.frags.filter(Boolean).length;
        if (n < 4) toast('ФРАГМЕНТ ' + n + '/4 · ГЛУБИНА ' + f.w + ' СИНХРОНИЗИРОВАНА', 2400);
        else toast('ВСЕ ФРАГМЕНТЫ СОБРАНЫ · ВЕРНИСЬ К ЯДРУ НА ГЛУБИНЕ 0', 4200);
      }
    }
    if (!G.done && G.frags.every(Boolean)) {
      const dx = world.CORE.x - G.x, dz = world.CORE.z - G.z;
      if (dx * dx + dz * dz < 1.6 && G.w === 0) doWin();
    }
  }
  G.done = false;

  function doWin() {
    G.done = true;
    G.mode = 'win';
    if (document.pointerLockElement) document.exitPointerLock();
    sfxWin();
    if (padGain && AC) padGain.gain.linearRampToValueAtTime(0.14, AC.currentTime + 1.5);
    document.getElementById('winTime').textContent = fmt(G.time);
    document.getElementById('winShifts').textContent = G.shifts;
    const res = (G.shifts <= 12 && G.time < 150) ? 'ТЕНЗОР' : G.shifts <= 20 ? 'НАБЛЮДАТЕЛЬ' : 'ПУТНИК';
    document.getElementById('winRank').textContent = res;
    document.getElementById('win').classList.remove('gone');
    document.getElementById('hud').style.opacity = 0.25;
  }

  /* ---------------------------------------------------------- modes */
  function startGame() {
    initAudio();
    if (AC && AC.state === 'suspended') AC.resume();
    reset();
    G.mode = 'play';
    document.getElementById('intro').classList.add('gone');
    document.getElementById('pause').classList.add('gone');
    document.getElementById('win').classList.add('gone');
    document.getElementById('hud').style.opacity = 1;
    lockPointer();
    setPadDepth(0);
    toast('НАЙДИ ЧЕТЫРЕ ФРАГМЕНТА · ПО ОДНОМУ НА КАЖДОЙ ГЛУБИНЕ', 3600);
  }
  function setPause(p) {
    if (G.mode === 'win') return;
    if (p) {
      G.mode = 'pause';
      document.getElementById('pause').classList.remove('gone');
      if (document.pointerLockElement) document.exitPointerLock();
    } else {
      G.mode = 'play';
      document.getElementById('pause').classList.add('gone');
      lockPointer();
    }
  }
  document.getElementById('btnStart').addEventListener('click', startGame);
  document.getElementById('btnResume').addEventListener('click', () => { setPause(false); });
  document.getElementById('btnRestart').addEventListener('click', startGame);
  document.getElementById('btnAgain').addEventListener('click', startGame);

  /* ---------------------------------------------------------- update */
  let stepAcc = 0;
  function update(dt) {
    if (G.mode === 'intro') {
      G.demoT += dt;
      G.yaw += dt * 0.22;
      G.pitch = Math.sin(G.demoT * 0.35) * 0.16 + 0.02;
      const a = G.demoT * 0.35;
      G.x = 15.6 + Math.cos(a) * 2.6;
      G.z = 17.4 + Math.sin(a) * 1.7;
      if (Math.floor(G.demoT / 4.2) !== G.demoW) {
        G.demoW = Math.floor(G.demoT / 4.2);
        G.w = (G.w + 1) % 4;
        G.glitch = 1; G.shake = 0.5;
        setPadDepthSilent(G.w);
      }
      return;
    }
    if (G.mode !== 'play') return;

    G.time = (performance.now() - G.t0) / 1000;
    document.getElementById('timer').textContent = fmt(G.time);

    // обзор
    const sens = 0.0023;
    G.yaw -= mouseDX * sens;
    G.pitch = clamp(G.pitch - mouseDY * sens, -0.85, 0.85);
    mouseDX = 0; mouseDY = 0;

    // движение
    let f = 0, s = 0;
    if (keys['w'] || keys['ц']) f += 1;
    if (keys['s'] || keys['ы']) f -= 1;
    if (keys['a'] || keys['ф']) s -= 1;
    if (keys['d'] || keys['в']) s += 1;
    f += tmove.y; s += tmove.x;
    const run = keys['shift'] || keys['shiftleft'] || keys['shiftright'];
    const sp = (run ? RUN : WALK) * dt;
    const len = Math.hypot(f, s);
    if (len > 1e-4) {
      f /= Math.max(1, len); s /= Math.max(1, len);
      const sy = Math.sin(G.yaw), cy = Math.cos(G.yaw);
      const mx = (sy * f + cy * s) * sp;
      const mz = (cy * f - sy * s) * sp;
      if (!bodyBlocked(G.w, G.x + mx, G.z)) G.x += mx; else G.x += mx * 0.0;
      if (!bodyBlocked(G.w, G.x, G.z + mz)) G.z += mz;
      G.moveDist += Math.hypot(mx, mz);
      G.bob += dt * (run ? 13 : 9);
      stepAcc += Math.hypot(mx, mz);
      if (stepAcc > (run ? 1.5 : 1.15)) { stepAcc = 0; sfxStep(); }
    } else {
      G.bob += dt * 1.3;
    }
    G.y = EYE;

    checkPickups();
    G.glitch = Math.max(0, G.glitch - dt * 3.2);
    G.punch = Math.max(0, G.punch - dt * 2.6);
    G.shake = Math.max(0, G.shake - dt * 3.4);
    updateProbe();
  }

  function setPadDepthSilent(w) { if (AC) setPadDepth(w); }

  let probeT = 0, probeText = '', probeShown = false;
  function updateProbe() {
    probeT += 1;
    if (probeT % 6 !== 0) return;
    const fx = Math.sin(G.yaw), fz = Math.cos(G.yaw);
    let msg = '';
    for (const d of [0.8, 1.4, 2.2, 3.2, 4.6, 6.2]) {
      const x = G.x + fx * d, z = G.z + fz * d;
      const m = cellBits(x, z);
      if (m.solid & (1 << G.w)) break;        // своя стена — не интересно
      const other = m.ghost & ~(1 << G.w);
      if (other) {
        const arr = [];
        for (let k = 0; k < 4; k++) if (other & (1 << k)) arr.push(k);
        const cur = [...depthEl.children];
        msg = 'СКВОЗЬ ЭТУ ПУСТОТУ ПРОСВЕЧИВАЕТ ГЛУБИНА ' + arr.join('+') +
              (arr.includes(G.w + 1) ? ' · W+' : '') + (arr.includes(G.w - 1) ? ' · W−' : '');
        for (let k = 0; k < 4; k++) {
          cur[k].style.outline = arr.includes(k) ? '1px solid ' + WHEX[k] : 'none';
        }
        break;
      }
    }
    if (!msg) {
      for (const el of depthEl.children) el.style.outline = 'none';
    }
    if (msg !== probeText) {
      probeText = msg;
      probeEl.textContent = msg;
    }
    probeEl.classList.toggle('on', !!msg);
  }
  function probeFlash() { if (probeText) toast(probeText, 1800); }

  /* ---------------------------------------------------------- minimap */
  const mapC = document.getElementById('map');
  const mctx = mapC.getContext('2d');
  let mapTick = 0;
  function drawMap() {
    if (mapTick++ % 4 !== 0) return;
    const S0 = mapC.width, cs = S0 / N;
    mctx.clearRect(0, 0, S0, S0);
    mctx.fillStyle = '#04121a'; mctx.fillRect(0, 0, S0, S0);
    const w = G.w;
    for (let z = 0; z < N; z++) {
      for (let x = 0; x < N; x++) {
        const v = world.grid[w][world.idx(x, 1, z)];
        const m = (v === 1 || v === 2) ? 1 : 0;
        let other = 0;
        if (!m) {
          for (let k = 0; k < 4; k++) {
            const vv = world.grid[k][world.idx(x, 1, z)];
            if (k !== w && (vv === 1 || vv === 2)) other |= 1 << k;
          }
        }
        if (m) {
          mctx.fillStyle = '#dff2f5';
          mctx.fillRect(x * cs, z * cs, cs + 0.6, cs + 0.6);
        } else if (other) {
          mctx.fillStyle = WHEX[31 - Math.clz32(other)];
          mctx.globalAlpha = 0.30;
          mctx.fillRect(x * cs, z * cs, cs + 0.6, cs + 0.6);
          mctx.globalAlpha = 1;
        }
      }
    }
    // фрагмент текущей глубины
    const f = world.FRAGS[w];
    if (!G.frags[w]) {
      const pulse = 0.55 + 0.45 * Math.sin(performance.now() / 260);
      mctx.fillStyle = WHEX[w]; mctx.globalAlpha = pulse;
      mctx.beginPath(); mctx.arc(f.x * cs, f.z * cs, cs * 1.15, 0, 7); mctx.fill();
      mctx.globalAlpha = 1;
    }
    // ядро
    if (G.frags.every(Boolean)) {
      mctx.strokeStyle = '#fff'; mctx.lineWidth = 1.5;
      mctx.beginPath(); mctx.arc(world.CORE.x * cs, world.CORE.z * cs, cs * 1.4, 0, 7); mctx.stroke();
    }
    // игрок
    const px = G.x * cs, pz = G.z * cs;
    mctx.save();
    mctx.translate(px, pz); mctx.rotate(-G.yaw + Math.PI);
    mctx.fillStyle = '#fff';
    mctx.beginPath(); mctx.moveTo(0, -cs * 1.1); mctx.lineTo(cs * 0.8, cs * 0.9); mctx.lineTo(-cs * 0.8, cs * 0.9); mctx.closePath(); mctx.fill();
    mctx.restore();
  }

  /* ---------------------------------------------------------- sprites */
  function project(x, y, z) {
    const sy = Math.sin(G.yaw), cy = Math.cos(G.yaw), sp = Math.sin(G.pitch), cp = Math.cos(G.pitch);
    const fx = sy * cp, fy = sp, fz = cy * cp;
    const rx = cy, rz = -sy;
    const ux = -sy * sp, uy = cp, uz = -cy * sp;
    const dx = x - G.x, dy = y - G.y, dz = z - G.z;
    const cz = dx * fx + dy * fy + dz * fz;
    const cx = dx * rx + dz * rz;
    const cyy = dx * ux + dy * uy + dz * uz;
    const tanX = FOV, tanY = FOV * VH / VW;
    const ndcX = (cx / cz) / tanX, ndcY = (cyy / cz) / tanY;
    return { sx: (ndcX + 1) / 2 * VW, sy: (1 - ndcY) / 2 * VH, cz };
  }
  function depthAt(nx, ny) {
    const x = clamp(nx * RW | 0, 0, RW - 1), y = clamp(ny * RH | 0, 0, RH - 1);
    return dep[y * RW + x];
  }

  function drawSprites(time) {
    /* --- маркеры фрагментов и ядра (только в своём срезе) --- */
    const items = [];
    for (let i = 0; i < 4; i++) {
      if (G.frags[i] || world.FRAGS[i].w !== G.w) continue;
      items.push({ x: world.FRAGS[i].x, y: 1.15, z: world.FRAGS[i].z, w: world.FRAGS[i].w, kind: 'frag', i });
    }
    const allDone = G.frags.every(Boolean);
    if (allDone && !G.done) items.push({ x: world.CORE.x, y: 1.2, z: world.CORE.z, w: 0, kind: 'core' });

    for (const it of items) {
      const pr = project(it.x, it.y, it.z);
      if (pr.cz < 0.25) continue;
      const behind = pr.cz > depthAt(pr.sx / VW, pr.sy / VH) + 0.6;
      const size = Math.min(VH * 1.6, VH * 0.62 / pr.cz);
      const x = pr.sx, y = pr.sy;
      const col = it.kind === 'core' ? '#ffffff' : WHEX[it.w];
      const pulse = 0.72 + 0.28 * Math.sin(time * 3.4 + (it.i || 0));
      vctx.save();
      vctx.globalCompositeOperation = 'lighter';
      vctx.globalAlpha = behind ? 0.3 : 1;
      const gr = vctx.createRadialGradient(x, y, 0, x, y, size * 0.9);
      gr.addColorStop(0, col + 'e0');
      gr.addColorStop(0.28, col + '55');
      gr.addColorStop(1, col + '00');
      vctx.fillStyle = gr;
      vctx.beginPath(); vctx.arc(x, y, size * 0.9 * pulse, 0, 7); vctx.fill();
      vctx.fillStyle = col;
      vctx.beginPath();
      vctx.moveTo(x, y - size * 0.19); vctx.lineTo(x + size * 0.14, y);
      vctx.lineTo(x, y + size * 0.19); vctx.lineTo(x - size * 0.14, y);
      vctx.closePath(); vctx.fill();
      vctx.strokeStyle = col + 'aa';
      vctx.lineWidth = Math.max(1, size * 0.012);
      vctx.beginPath(); vctx.ellipse(x, y, size * 0.3 * pulse, size * 0.1 * pulse, time * 1.1, 0, 7); vctx.stroke();
      vctx.beginPath(); vctx.ellipse(x, y, size * 0.3 * pulse, size * 0.1 * pulse, -time * 1.4 + 1.1, 0, 7); vctx.stroke();
      const g2 = vctx.createLinearGradient(x, y - size * 2.4, x, y);
      g2.addColorStop(0, col + '00'); g2.addColorStop(1, col + '40');
      vctx.fillStyle = g2; vctx.fillRect(x - size * 0.02, y - size * 2.4, size * 0.04, size * 2.4);
      vctx.restore();
    }

    /* --- навигатор: ближайшая цель в ЛЮБОМ измерении --- */
    let target = null, best = 1e9;
    for (let i = 0; i < 4; i++) {
      if (G.frags[i]) continue;
      const f = world.FRAGS[i];
      const d = Math.hypot(f.x - G.x, f.z - G.z);
      if (d < best) { best = d; target = f; }
    }
    if (allDone) { target = world.CORE; best = Math.hypot(world.CORE.x - G.x, world.CORE.z - G.z); }
    G.target = target; G.targetDist = best;

    if (target) {
      const pr = project(target.x, 1.2, target.z);
      const inside = pr.cz > 0.3 && pr.sx > 60 && pr.sx < VW - 60 && pr.sy > 60 && pr.sy < VH - 60;
      if (!inside) {
        const ang = Math.atan2(pr.sx - VW / 2, -(pr.sy - VH / 2));
        const R = Math.min(VW, VH) * 0.34;
        const ax = VW / 2 + Math.sin(ang) * R, ay = VH / 2 - Math.cos(ang) * R;
        const tw = target.w !== undefined ? target.w : 0;
        const col = allDone ? '#ffffff' : WHEX[tw];
        const sameSlice = allDone || tw === G.w;
        vctx.save();
        vctx.translate(ax, ay); vctx.rotate(ang);
        vctx.globalAlpha = sameSlice ? 0.95 : 0.62;
        vctx.fillStyle = col;
        vctx.beginPath(); vctx.moveTo(0, -15 * DPR); vctx.lineTo(9 * DPR, 9 * DPR); vctx.lineTo(-9 * DPR, 9 * DPR); vctx.closePath(); vctx.fill();
        vctx.restore();
        vctx.save();
        vctx.font = '600 ' + Math.round(10.5 * DPR) + 'px ui-monospace,monospace';
        vctx.textAlign = 'center';
        vctx.fillStyle = col;
        vctx.globalAlpha = 0.9;
        const label = allDone ? 'ЯДРО · ' + Math.round(best) + ' м'
          : sameSlice ? Math.round(best) + ' м'
          : Math.round(best) + ' м · ГЛУБИНА ' + tw + (tw > G.w ? ' →' : ' ←');
        vctx.fillText(label, ax, ay + 30 * DPR);
        vctx.restore();
      }
    }
  }

  /* ---------------------------------------------------------- frame */
  let lastT = performance.now(), frameMs = 14, frames = 0, scaleTimer = 0;
  let bloomOk = !matchMedia('(pointer:coarse)').matches;

  function frame(now) {
    let dt = (now - lastT) / 1000; lastT = now;
    if (dt > 0.05) dt = 0.05;
    const time = now / 1000;
    update(dt);

    // тряска и «удар» при переходе
    const shakeAmp = G.shake * 0.011;
    const shx = (Math.sin(time * 71) * 0.6 + (Math.random() - 0.5)) * shakeAmp * VW;
    const shy = (Math.cos(time * 83) * 0.6 + (Math.random() - 0.5)) * shakeAmp * VH;

    const camY = G.y + Math.sin(G.bob) * 0.035;
    const fov = FOV * (1 + G.punch * 0.16);
    const t0 = performance.now();
    Render.render(world, {
      fov, x: G.x, y: camY, z: G.z, yaw: G.yaw, pitch: G.pitch, w: G.w, occ, P, lamps,
    }, RW, RH, pix, dep, time);
    lctx.putImageData(img, 0, 0);
    const tRender = performance.now() - t0;

    vctx.save();
    vctx.imageSmoothingEnabled = false;
    vctx.translate(shx, shy);
    const gl = G.glitch;
    if (gl > 0.02) {
      // глитч: горизонтальные полосы со сдвигом + хроматическая раздвоенность
      const bands = 14;
      const bh = VH / bands;
      for (let i = 0; i < bands; i++) {
        const off = (Math.random() - 0.5) * gl * 46 * DPR * (0.4 + Math.random());
        const h = bh + 1;
        const sy = i * bh * RH / VH;
        const sh = Math.min(RH - sy, h * RH / VH);
        vctx.drawImage(low, 0, sy, RW, sh, off, i * bh, VW, h);
      }
      vctx.globalCompositeOperation = 'lighter';
      vctx.globalAlpha = gl * 0.35;
      vctx.drawImage(low, 0, 0, RW, RH, -6 * DPR * gl, 0, VW, VH);
      vctx.globalAlpha = gl * 0.28;
      vctx.drawImage(low, 0, 0, RW, RH, 6 * DPR * gl, 0, VW, VH);
      vctx.globalAlpha = 1;
      vctx.globalCompositeOperation = 'source-over';
    } else {
      vctx.drawImage(low, 0, 0, RW, RH, 0, 0, VW, VH);
    }
    vctx.restore();

    // блум
    if (bloomOn && bloomOk) {
      vctx.save();
      vctx.globalCompositeOperation = 'lighter';
      vctx.globalAlpha = 0.22;
      try { vctx.filter = 'blur(' + Math.round(7 * DPR) + 'px)'; } catch (e) {}
      vctx.drawImage(low, 0, 0, RW, RH, 0, 0, VW, VH);
      vctx.filter = 'none';
      vctx.restore();
    }

    drawSprites(time);
    drawMap();

    // авто-качество
    frameMs += (tRender - frameMs) * 0.08;
    frames++;
    scaleTimer++;
    if (scaleTimer > 45) {
      scaleTimer = 0;
      if (frameMs > 21 && scale > 0.26) allocScale(scale * 0.9);
      else if (frameMs < 11 && scale < 0.62) allocScale(scale * 1.06);
    }
    if (frames === 120) {
      const fps = 1000 / (performance.now() - fpsRef);
      if (fps < 38) bloomOk = false;
      document.getElementById('fps').textContent = 'Σ ' + scale.toFixed(2) + 'x  ·  ' + frameMs.toFixed(1) + 'ms';
    }
    requestAnimationFrame(frame);
  }
  let fpsRef = performance.now();

  /* ---------------------------------------------------------- boot */
  resize();
  reset();
  updateDepthHUD();
  updateFragHUD();
  requestAnimationFrame(frame);
})();
