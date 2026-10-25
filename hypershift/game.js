/* ============================================================
   ГИПЕРСДВИГ / HYPERSHIFT — 4D-платформер
   Трёхмерный срез четырёхмерного мира. Ось W — игровая механика.
   three.js r128 · WebAudio · без внешних ассетов
   ============================================================ */
(function () {
'use strict';

/* ---------------- утилиты ---------------- */
const clamp = (v, a, b) => v < a ? a : (v > b ? b : v);
const lerp = (a, b, t) => a + (b - a) * t;
const sstep = (a, b, x) => { const t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
const $ = id => document.getElementById(id);

window.onerror = function (m, s, l) {
  const e = $('err'); if (e) { e.classList.remove('hidden'); e.innerHTML = 'ОШИБКА: ' + m + '<br>строка ' + l; }
};

/* ---------------- аудио (полностью синтезированное) ---------------- */
const Sfx = {
  ctx: null, muted: false,
  init() {
    if (this.ctx) return;
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    this.ctx = new AC();
    this.master = this.ctx.createGain(); this.master.gain.value = 0.5; this.master.connect(this.ctx.destination);
    this.delay = this.ctx.createDelay(1); this.delay.delayTime.value = 0.38;
    this.fb = this.ctx.createGain(); this.fb.gain.value = 0.32;
    this.delay.connect(this.fb); this.fb.connect(this.delay); this.delay.connect(this.master);
    this.startMusic();
  },
  resume() { if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume(); },
  tone(o) {
    if (!this.ctx || this.muted) return;
    const c = this.ctx, t0 = c.currentTime + (o.at || 0), dur = o.dur || 0.2;
    const osc = c.createOscillator(); osc.type = o.type || 'sine';
    osc.frequency.setValueAtTime(o.f, t0);
    if (o.f2) osc.frequency.exponentialRampToValueAtTime(o.f2, t0 + dur);
    const g = c.createGain();
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(o.v || 0.15, t0 + 0.012);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    osc.connect(g); g.connect(o.echo ? this.delay : this.master);
    osc.start(t0); osc.stop(t0 + dur + 0.05);
  },
  noise(o) {
    if (!this.ctx || this.muted) return;
    const c = this.ctx, dur = o.dur || 0.3, t0 = c.currentTime;
    const n = c.createBufferSource(), buf = c.createBuffer(1, c.sampleRate * dur, c.sampleRate), d = buf.getChannelData(0);
    for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
    n.buffer = buf;
    const bp = c.createBiquadFilter(); bp.type = 'bandpass'; bp.Q.value = 1.3;
    bp.frequency.setValueAtTime(o.from || 300, t0);
    bp.frequency.exponentialRampToValueAtTime(o.to || 2000, t0 + dur);
    const g = c.createGain(); g.gain.setValueAtTime(o.v || 0.22, t0);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    n.connect(bp); bp.connect(g); g.connect(this.master); n.start(t0);
  },
  pickup() { this.tone({ f: 880, f2: 1318, type: 'triangle', dur: 0.12, v: 0.16 }); this.tone({ f: 1318, f2: 1760, type: 'triangle', dur: 0.28, v: 0.13, at: 0.09, echo: true }); },
  jump() { this.tone({ f: 290, f2: 520, type: 'square', dur: 0.11, v: 0.05 }); },
  land() { this.tone({ f: 150, f2: 75, type: 'sine', dur: 0.12, v: 0.09 }); },
  shift(dir) { this.noise({ from: dir > 0 ? 260 : 1900, to: dir > 0 ? 1900 : 260, dur: 0.35, v: 0.17 }); this.tone({ f: dir > 0 ? 230 : 460, f2: dir > 0 ? 460 : 230, dur: 0.3, v: 0.05 }); },
  hurt() { this.tone({ f: 420, f2: 65, type: 'sawtooth', dur: 0.42, v: 0.18 }); this.noise({ from: 900, to: 90, dur: 0.4, v: 0.18 }); },
  gate() { [523, 659, 784, 1046].forEach((f, i) => this.tone({ f, type: 'triangle', dur: 0.5, v: 0.14, at: i * 0.12, echo: true })); },
  win() { [523, 659, 784, 1046, 1318, 1568].forEach((f, i) => this.tone({ f, type: 'triangle', dur: 0.6, v: 0.15, at: i * 0.15, echo: true })); },
  startMusic() {
    const c = this.ctx;
    this.padGain = c.createGain(); this.padGain.gain.value = 0.032;
    const lp = c.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 720; lp.Q.value = 0.5;
    this.padGain.connect(lp); lp.connect(this.master);
    this.pads = [110, 164.8, 220].map(f => {
      const o = c.createOscillator(); o.type = 'sawtooth'; o.frequency.value = f;
      const g = c.createGain(); g.gain.value = 0.33;
      o.connect(g); g.connect(this.padGain); o.start(); return o;
    });
    this.chords = [[110, 130.8, 164.8], [87.3, 110, 130.8], [98, 123.5, 146.8], [110, 164.8, 196]];
    this.ci = 0; this.nextChord = c.currentTime + 8; this.nextPluck = c.currentTime + 2;
    this.scale = [220, 261.6, 293.7, 329.6, 392, 440, 523.3];
  },
  tick() {
    if (!this.ctx || this.ctx.state !== 'running') return;
    const t = this.ctx.currentTime;
    if (t >= this.nextChord) {
      const ch = this.chords[this.ci++ % this.chords.length];
      this.pads.forEach((o, i) => o.frequency.linearRampToValueAtTime(ch[i % ch.length], t + 3));
      this.nextChord = t + 8;
    }
    if (t >= this.nextPluck) {
      if (!this.muted) {
        const f = this.scale[(Math.random() * this.scale.length) | 0] * (Math.random() < 0.3 ? 2 : 1);
        const o = this.ctx.createOscillator(); o.type = 'triangle'; o.frequency.value = f;
        const g = this.ctx.createGain();
        g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.05, t + 0.02);
        g.gain.exponentialRampToValueAtTime(0.0001, t + 1.2);
        o.connect(g); g.connect(this.delay); o.start(t); o.stop(t + 1.4);
      }
      this.nextPluck = t + 1.2 + Math.random() * 2.2;
    }
  }
};

/* ---------------- константы мира ---------------- */
const LAYERS = [-2, -1, 0, 1, 2];
const LCOL = [0x8a5cff, 0x3d8bff, 0x00d6a4, 0xffab00, 0xff4d6e];
const TOTAL = 12;
const G = 24, JUMP = 8.8, SPEED = 6, SPRINT = 9.8, ACCG = 12, ACCA = 3.5;
const PR = 0.35, PH = 1.75, EYE = 1.62;

/* ---------------- уровень: платформы в 4D ----------------
   P(x, y, z, w, sx, sz, opt) — w:null значит «во всех слоях» */
const platDefs = [];
function P(x, y, z, w, sx, sz, opt) { platDefs.push(Object.assign({ x, y, z, w, sx, sz }, opt || {})); }

/* ХАБ, слой W=0 */
P(0, 0, 0, 0, 8, 8);
P(7, 0.4, 0, 0, 3, 3);        P(13, 0.8, 0, 0, 3.2, 3.2);
P(-7, 0.4, 0, 0, 3, 3);       P(-12, 1.2, 0, 0, 3.2, 3.2);
P(0, 0.4, -7, 0, 3, 3);       P(0, 1.0, -12, 0, 3.2, 3.2);
/* спираль к Вратам (существует во ВСЕХ слоях) */
P(0, 1.0, 4.6, null, 2.4, 2.4, { omni: 1 });
P(2.2, 2.0, 3.4, null, 2.2, 2.2, { omni: 1 });
P(3.4, 3.0, 1.2, null, 2.2, 2.2, { omni: 1 });
P(3.4, 4.0, -1.4, null, 2.2, 2.2, { omni: 1 });
P(1.6, 5.0, -3.2, null, 2.2, 2.2, { omni: 1 });
P(-0.8, 6.0, -3.4, null, 2.2, 2.2, { omni: 1 });
P(-2.8, 7.0, -1.8, null, 2.2, 2.2, { omni: 1 });
/* мосты-камни между слоями */
P(7.5, 0.8, 7, 1, 2.6, 2.6, { bob: 1 });    P(11.5, 1.5, 10.5, 1, 2.6, 2.6, { bob: 1 });
P(7.5, 0.8, -7, -1, 2.6, 2.6, { bob: 1 });  P(11.5, 1.5, -10.5, -1, 2.6, 2.6, { bob: 1 });
P(-7.5, 0.8, 7, -2, 2.6, 2.6, { bob: 1 });  P(-11.5, 1.5, 10.5, -2, 2.6, 2.6, { bob: 1 });
P(-7.5, 0.8, -7, 2, 2.6, 2.6, { bob: 1 });  P(-11.5, 1.5, -10.5, 2, 2.6, 2.6, { bob: 1 });
/* острова слоёв */
P(15.5, 2.1, 14.5, 1, 4, 4);   P(15.5, 2.5, 20.5, 1, 3, 3, { bob: 1 });   P(21, 2.9, 15, 1, 3, 3, { bob: 1 });
P(15.5, 2.1, -14.5, -1, 4, 4); P(15.5, 2.5, -20.5, -1, 3, 3, { bob: 1 }); P(21, 2.9, -15, -1, 3, 3, { bob: 1 });
P(-15.5, 2.1, 14.5, -2, 4, 4); P(-15.5, 2.5, 20.5, -2, 3, 3, { bob: 1 }); P(-21, 2.9, 15, -2, 3, 3, { bob: 1 });
P(-15.5, 2.1, -14.5, 2, 4, 4); P(-15.5, 2.5, -20.5, 2, 3, 3, { bob: 1 }); P(-21, 2.9, -15, 2, 3, 3, { bob: 1 });
/* аномалия: фазирует между слоями */
P(0, 2.2, -17.5, null, 2.6, 2.6, { anim: 1, baseW: -0.5, amp: 1.2, spd: 0.55 });

const crystalDefs = [
  { x: 13, y: 2.9, z: 0, w: 0 },       { x: -12, y: 3.7, z: 0, w: 0 },   { x: 0, y: 3.3, z: -12, w: 0 },
  { x: 15.5, y: 4.3, z: 20.5, w: 1 },  { x: 21, y: 6.2, z: 15, w: 1 },
  { x: 15.5, y: 4.3, z: -20.5, w: -1 },{ x: 21, y: 4.7, z: -15, w: -1 },
  { x: -15.5, y: 4.3, z: 20.5, w: -2 },{ x: -21, y: 6.2, z: 15, w: -2 },
  { x: -15.5, y: 4.3, z: -20.5, w: 2 },{ x: -21, y: 4.7, z: -15, w: 2 },
  { x: 0, y: 4.6, z: -17.5, w: 0 }
];
const shadowDefs = [
  { x: 15.5, y: 4, z: -14.5, w: -1 },
  { x: -15.5, y: 4, z: 14.5, w: -2 },
  { x: -15.5, y: 4, z: -14.5, w: 2 }
];
const GATE = { x: -2.8, y: 9.4, z: -1.8 };

const hints = [
  { t: 0, txt: 'WASD — движение · ПРОБЕЛ — прыжок · SHIFT — бег · колесо мыши / Q / E — сдвиг по 4-й оси W', dur: 7 },
  { x: 5, y: 1, z: 0, r: 4, txt: 'Всё, что ты видишь — 3D-срез. Q или E сдвигает тебя поперёк реальностей.' },
  { x: 7.5, y: 1.5, z: 7, r: 5.5, txt: 'Разгонись, прыгни к камню и В ПОЛЁТЕ нажми E — он существует в слое W=+1!' },
  { x: 0, y: 2, z: -14, r: 5, txt: 'Эта платформа фазирует между слоями. Прыгай, когда она станет плотной.' },
  { ev: 'aggro', txt: 'ТЕНЬ чувствует твою координату W! Меняй слой — она отстанет.' },
  { ev: 'gate', txt: 'ВСЕ 12 ОСКОЛКОВ! Врата сияют на вершине спирали в центре хаба.', dur: 10 }
];

/* ---------------- three.js сцена ---------------- */
let renderer;
try {
  renderer = new THREE.WebGLRenderer({ canvas: $('scene'), antialias: true });
} catch (e) {
  const el = $('err'); el.classList.remove('hidden'); el.textContent = 'WebGL недоступен: ' + e;
  return;
}
renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.outputEncoding = THREE.sRGBEncoding;

const scene = new THREE.Scene();
const fogCol = new THREE.Color(0x05060f);
scene.background = fogCol;
scene.fog = new THREE.FogExp2(fogCol.getHex(), 0.014);

const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 400);
scene.add(new THREE.HemisphereLight(0xcfe0ff, 0x1a1f3a, 0.95));
const sun = new THREE.DirectionalLight(0xfff4e0, 0.7);
sun.position.set(6, 12, 4); scene.add(sun);

/* свечение (генерируем текстуру кодом) */
const glowTex = (() => {
  const cv = document.createElement('canvas'); cv.width = cv.height = 128;
  const g = cv.getContext('2d');
  const gr = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  gr.addColorStop(0, 'rgba(255,255,255,1)');
  gr.addColorStop(0.3, 'rgba(255,255,255,0.55)');
  gr.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = gr; g.fillRect(0, 0, 128, 128);
  return new THREE.CanvasTexture(cv);
})();

/* звёзды */
{
  const n = 900, pos = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) {
    const th = Math.random() * Math.PI * 2, ph = Math.acos(2 * Math.random() - 1), r = 150 + Math.random() * 60;
    pos[i * 3] = r * Math.sin(ph) * Math.cos(th);
    pos[i * 3 + 1] = Math.abs(r * Math.cos(ph)) * 0.8 + 5;
    pos[i * 3 + 2] = r * Math.sin(ph) * Math.sin(th);
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  const m = new THREE.PointsMaterial({ color: 0xbfd2ff, size: 1.5, transparent: true, opacity: 0.85, fog: false });
  const stars = new THREE.Points(g, m); stars.name = 'stars'; scene.add(stars);
}
const starsObj = scene.getObjectByName('stars');

/* цифровая бездна */
const gridMat = new THREE.MeshBasicMaterial({ color: 0x00d6a4, wireframe: true, transparent: true, opacity: 0.055 });
const grid = new THREE.Mesh(new THREE.PlaneGeometry(400, 400, 48, 48), gridMat);
grid.rotation.x = -Math.PI / 2; grid.position.y = -13; scene.add(grid);

/* ---------------- построение объектов ---------------- */
const geoBox = new THREE.BoxGeometry(1, 1, 1);
const geoGhost = new THREE.BoxGeometry(1.02, 1.02, 1.02);
const platforms = [];

platDefs.forEach((d, i) => {
  const col = d.omni ? 0xcfe4ff : (d.anim ? 0xffffff : LCOL[LAYERS.indexOf(d.w)]);
  const mat = new THREE.MeshLambertMaterial({
    color: col, transparent: true, opacity: 1,
    emissive: new THREE.Color(col).multiplyScalar(0.16)
  });
  const mesh = new THREE.Mesh(geoBox, mat);
  mesh.scale.set(d.sx, 0.9, d.sz);
  mesh.position.set(d.x, d.y, d.z);
  const eMat = new THREE.LineBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.3 });
  mesh.add(new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(d.sx, 0.9, d.sz)), eMat));
  const gMat = new THREE.MeshBasicMaterial({ color: col, wireframe: true, transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false });
  const ghost = new THREE.Mesh(geoGhost, gMat);
  ghost.scale.set(d.sx, 0.9, d.sz);
  ghost.position.set(d.x, d.y, d.z);
  scene.add(mesh); scene.add(ghost);
  platforms.push({
    def: d, mesh, ghost, mat, eMat, gMat,
    hx: d.sx / 2, hy: 0.45, hz: d.sz / 2,
    cx: d.x, cy: d.y, cz: d.z, curW: d.w == null ? 0 : d.w,
    ph: Math.random() * 6.28, spd: 0.7 + Math.random() * 0.6,
    solidNow: false, prevY: d.y
  });
});

const geoCr = new THREE.IcosahedronGeometry(0.45, 0);
const geoCrG = new THREE.IcosahedronGeometry(0.62, 0);
const crystals = [];
crystalDefs.forEach((d, i) => {
  const col = LCOL[clamp(Math.round(d.w) + 2, 0, 4)];
  const core = new THREE.Mesh(geoCr, new THREE.MeshLambertMaterial({ color: 0xffffff, emissive: new THREE.Color(col), emissiveIntensity: 1.1, transparent: true }));
  const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: col, transparent: true, opacity: 0.8, blending: THREE.AdditiveBlending, depthWrite: false }));
  glow.scale.set(2.6, 2.6, 1);
  const ghost = new THREE.Mesh(geoCrG, new THREE.MeshBasicMaterial({ color: col, wireframe: true, transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false }));
  core.position.set(d.x, d.y, d.z); ghost.position.set(d.x, d.y, d.z);
  scene.add(core); scene.add(glow); scene.add(ghost);
  crystals.push({ def: d, core, glow, ghost, taken: false, ph: i * 0.7 });
});

/* Тени — враги, фазирующие между слоями */
const shadows = [];
shadowDefs.forEach((d, i) => {
  const g = new THREE.Group();
  const core = new THREE.Mesh(new THREE.IcosahedronGeometry(0.72, 1), new THREE.MeshBasicMaterial({ color: 0x07070f, transparent: true }));
  const shell = new THREE.Mesh(new THREE.IcosahedronGeometry(0.82, 1), new THREE.MeshBasicMaterial({ color: 0xff2244, wireframe: true, transparent: true }));
  const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: 0xff2233, transparent: true, opacity: 0.5, blending: THREE.AdditiveBlending, depthWrite: false }));
  glow.scale.set(3.6, 3.6, 1);
  g.add(core); g.add(shell); g.add(glow);
  g.position.set(d.x, d.y, d.z);
  scene.add(g);
  shadows.push({ def: d, g, core, shell, glow, w: d.w, ph: i * 2.1, aggro: false, home: { x: d.x, z: d.z } });
});

/* Врата Тессеракта */
const gate = new THREE.Group();
const gateMatA = new THREE.MeshBasicMaterial({ color: 0x2a3350, wireframe: true, transparent: true, opacity: 0.8 });
const ring1 = new THREE.Mesh(new THREE.TorusGeometry(1.7, 0.09, 10, 48), gateMatA);
const ring2 = new THREE.Mesh(new THREE.TorusGeometry(1.25, 0.07, 10, 40), gateMatA.clone());
ring2.rotation.x = Math.PI / 2;
gate.add(ring1); gate.add(ring2);
/* мини-тессеракт внутри врат */
const TV = []; for (let i = 0; i < 16; i++) TV.push([(i & 1) ? 1 : -1, (i & 2) ? 1 : -1, (i & 4) ? 1 : -1, (i & 8) ? 1 : -1]);
const TE = []; for (let i = 0; i < 16; i++) for (let j = i + 1; j < 16; j++) { const x = i ^ j; if (x && !(x & (x - 1))) TE.push([i, j]); }
const gateTessGeo = new THREE.BufferGeometry();
gateTessGeo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(TE.length * 6), 3));
const gateTessMat = new THREE.LineBasicMaterial({ color: 0x445588, transparent: true, opacity: 0.9 });
const gateTess = new THREE.LineSegments(gateTessGeo, gateTessMat);
gate.add(gateTess);
const beamMat = new THREE.MeshBasicMaterial({ color: 0x00e5c0, transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide });
const beam = new THREE.Mesh(new THREE.CylinderGeometry(0.55, 0.55, 40, 16, 1, true), beamMat);
beam.position.copy(new THREE.Vector3(GATE.x, GATE.y, GATE.z));
scene.add(beam);
gate.position.set(GATE.x, GATE.y, GATE.z);
scene.add(gate);
let gateActive = false;
function updateGateTess(t) {
  const a = t * 0.6, b = t * 0.4, c = t * 0.25, s = 0.55;
  const ca = Math.cos(a), sa = Math.sin(a), cb = Math.cos(b), sb = Math.sin(b), cc = Math.cos(c), sc = Math.sin(c);
  const p = TV.map(v => {
    let [x, y, z, u] = v;
    let x1 = x * ca - u * sa, u1 = x * sa + u * ca;
    let z1 = z * cb - u1 * sb, u2 = z * sb + u1 * cb;
    let y1 = y * cc - z1 * sc, z2 = y * sc + z1 * cc;
    const k = 1.9 / (1.9 - u2 * 0.7) * s;
    return [x1 * k, y1 * k, z2 * k];
  });
  const arr = gateTessGeo.attributes.position.array;
  TE.forEach((e, i) => {
    arr[i * 6] = p[e[0]][0]; arr[i * 6 + 1] = p[e[0]][1]; arr[i * 6 + 2] = p[e[0]][2];
    arr[i * 6 + 3] = p[e[1]][0]; arr[i * 6 + 4] = p[e[1]][1]; arr[i * 6 + 5] = p[e[1]][2];
  });
  gateTessGeo.attributes.position.needsUpdate = true;
}
updateGateTess(0);

/* эффекты-вспышки подбора */
const fx = [];
function burst(x, y, z, col) {
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: col, transparent: true, opacity: 0.95, blending: THREE.AdditiveBlending, depthWrite: false }));
  s.position.set(x, y, z); s.scale.set(1, 1, 1);
  scene.add(s); fx.push({ s, t: 0 });
}

/* ---------------- игрок и состояние ---------------- */
const player = {
  pos: new THREE.Vector3(0, 0.6, 2.5),
  vel: new THREE.Vector3(),
  yaw: -Math.PI / 2, pitch: -0.05,
  w: 0, wTarget: 0,
  onGround: false, groundPlat: null,
  coyote: 0, jbuf: 0,
  cp: { x: 0, y: 0.45, z: 2.5, w: 0 },
  invuln: 0, dying: 0
};
let state = 'menu';
let collected = 0, deaths = 0, playT = 0;
let fovKick = 0, shiftCool = 0, aggroShown = false;
const keys = {};
let lookTouch = null, joyId = null, joyVec = { x: 0, y: 0 };
const isTouch = matchMedia('(pointer:coarse)').matches || 'ontouchstart' in window;

/* HUD-слой слоёв */
const stripEl = $('strip');
const slotEls = LAYERS.map((w, i) => {
  const el = document.createElement('div'); el.className = 'slot';
  const cnt = crystalDefs.filter(c => clamp(Math.round(c.w) + 2, 0, 4) === i).length;
  el.innerHTML = '<div class="bar" style="background:#' + new THREE.Color(LCOL[i]).getHexString() + '"></div>W' + (w > 0 ? '+' : '') + w + ' · ◆' + cnt;
  stripEl.appendChild(el); return el;
});

function showHint(txt, dur) {
  const h = $('hint'); h.textContent = txt; h.style.opacity = 1;
  clearTimeout(showHint.tid);
  showHint.tid = setTimeout(() => { h.style.opacity = 0; }, (dur || 6) * 1000);
}

/* ---------------- управление ---------------- */
function shiftW(dir) {
  if (state !== 'play' || player.dying > 0) return;
  const nt = clamp(player.wTarget + dir, -2, 2);
  if (nt === player.wTarget) return;
  player.wTarget = nt;
  Sfx.shift(dir);
  fovKick = 7;
  $('shiftFx').style.opacity = 0.9;
  setTimeout(() => $('shiftFx').style.opacity = 0, 220);
  const sc = $('scene'); sc.classList.add('shift'); setTimeout(() => sc.classList.remove('shift'), 260);
}

document.addEventListener('keydown', e => {
  if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Tab'].includes(e.code)) e.preventDefault();
  keys[e.code] = true;
  if (e.code === 'KeyQ') shiftW(-1);
  if (e.code === 'KeyE') shiftW(1);
  if (e.code === 'KeyM') { Sfx.muted = !Sfx.muted; showHint(Sfx.muted ? ' звук выключен ' : ' звук включён ', 2); }
  if (e.code === 'KeyR' && state === 'play' && !player.dying) die(true);
  if (e.code === 'Escape' && state === 'play' && !plEngaged) pause();
  if (e.code === 'Space') player.jbuf = 0.13;
});
document.addEventListener('keyup', e => { keys[e.code] = false; });

const canvas = $('scene');
let plEngaged = false, plEverWorked = false, plUnavailable = isTouch;
function tryLock() {
  if (plUnavailable || !canvas.requestPointerLock) return;
  try {
    const p = canvas.requestPointerLock();
    if (p && p.catch) p.catch(() => lockFail());
  } catch (e) { lockFail(); }
}
function lockFail() {
  if (plEverWorked) return; /* кулдаун после Esc — просто работаем через ЛКМ */
  plUnavailable = true;
  $('plHint').textContent = '⟵ обзор: зажми ЛКМ и веди мышью';
}
document.addEventListener('pointerlockerror', lockFail);
document.addEventListener('pointerlockchange', () => {
  if (document.pointerLockElement === canvas) {
    plEngaged = true; plEverWorked = true; $('plHint').textContent = '';
  } else if (plEngaged) {
    plEngaged = false;
    if (state === 'play') pause();
  }
});
canvas.addEventListener('click', () => { if (state === 'play') tryLock(); });

let dragging = false;
document.addEventListener('mousedown', e => { if (e.button === 0) dragging = true; });
document.addEventListener('mouseup', e => { if (e.button === 0) dragging = false; });
document.addEventListener('mousemove', e => {
  if (state !== 'play') return;
  let dx = 0, dy = 0;
  if (document.pointerLockElement === canvas) { dx = e.movementX; dy = e.movementY; }
  else if (dragging && !isTouch) { dx = e.movementX || 0; dy = e.movementY || 0; }
  else return;
  player.yaw -= dx * 0.0022;
  player.pitch = clamp(player.pitch - dy * 0.0022, -1.45, 1.45);
});
document.addEventListener('wheel', e => {
  if (state !== 'play') return;
  e.preventDefault();
  shiftW(e.deltaY > 0 ? 1 : -1);
}, { passive: false });
document.addEventListener('contextmenu', e => e.preventDefault());

/* сенсорное управление */
if (isTouch) {
  $('touch').style.display = 'block';
  const base = $('joyBase'), stick = $('joyStick');
  document.addEventListener('touchstart', e => {
    for (const t of e.changedTouches) {
      if (t.target.closest && t.target.closest('.tbtn')) continue;
      if (t.clientX < innerWidth * 0.45 && joyId === null) {
        joyId = t.identifier;
        base.style.display = 'block';
        base.style.left = (t.clientX - 55) + 'px'; base.style.top = (t.clientY - 55) + 'px';
      } else if (lookTouch === null) {
        lookTouch = { id: t.identifier, x: t.clientX, y: t.clientY };
      }
    }
  }, { passive: true });
  document.addEventListener('touchmove', e => {
    for (const t of e.changedTouches) {
      if (t.identifier === joyId) {
        const r = base.getBoundingClientRect();
        let dx = t.clientX - (r.left + 55), dy = t.clientY - (r.top + 55);
        const l = Math.hypot(dx, dy); if (l > 48) { dx *= 48 / l; dy *= 48 / l; }
        joyVec.x = dx / 48; joyVec.y = dy / 48;
        stick.style.left = (31 + dx) + 'px'; stick.style.top = (31 + dy) + 'px';
      } else if (lookTouch && t.identifier === lookTouch.id) {
        player.yaw -= (t.clientX - lookTouch.x) * 0.005;
        player.pitch = clamp(player.pitch - (t.clientY - lookTouch.y) * 0.005, -1.45, 1.45);
        lookTouch.x = t.clientX; lookTouch.y = t.clientY;
      }
    }
  }, { passive: true });
  document.addEventListener('touchend', e => {
    for (const t of e.changedTouches) {
      if (t.identifier === joyId) { joyId = null; joyVec.x = joyVec.y = 0; base.style.display = 'none'; stick.style.left = '31px'; stick.style.top = '31px'; }
      if (lookTouch && t.identifier === lookTouch.id) lookTouch = null;
    }
  });
  const bindBtn = (id, down, up) => {
    const el = $(id);
    el.addEventListener('touchstart', e => { e.preventDefault(); down(); }, { passive: false });
    el.addEventListener('touchend', e => { e.preventDefault(); if (up) up(); }, { passive: false });
  };
  bindBtn('btnJ', () => { player.jbuf = 0.15; keys.Space = true; }, () => keys.Space = false);
  bindBtn('btnE', () => shiftW(1));
  bindBtn('btnQ', () => shiftW(-1));
  bindBtn('btnP', () => { if (state === 'play') pause(); else if (state === 'pause') resume(); });
}

/* ---------------- меню / пауза / победа ---------------- */
function startGame() {
  Sfx.init(); Sfx.resume();
  $('intro').classList.add('hidden');
  state = 'play';
  tryLock();
  showHint(hints[0].txt, hints[0].dur);
}
$('play').addEventListener('click', startGame);
$('resume').addEventListener('click', () => resume());
$('again').addEventListener('click', () => location.reload());

function pause() {
  if (state !== 'play') return;
  state = 'pause';
  $('pause').classList.remove('hidden');
  if (document.pointerLockElement) document.exitPointerLock();
}
function resume() {
  if (state !== 'pause') return;
  Sfx.resume();
  state = 'play';
  $('pause').classList.add('hidden');
  tryLock();
}
document.addEventListener('visibilitychange', () => { if (document.hidden && state === 'play') pause(); });

function die(silent) {
  if (player.dying > 0) return;
  player.dying = 0.85; deaths++;
  if (!silent) Sfx.hurt();
  $('flash').style.opacity = 0.85;
  setTimeout(() => $('flash').style.opacity = 0, 260);
  $('fade').style.opacity = 1;
}
function respawn() {
  player.pos.set(player.cp.x, player.cp.y + 0.12, player.cp.z);
  player.vel.set(0, 0, 0);
  player.w = player.cp.w; player.wTarget = player.cp.w;
  player.invuln = 2.2; player.dying = 0; player.groundPlat = null;
  $('fade').style.opacity = 0;
}
function winGame() {
  state = 'win';
  Sfx.win();
  if (document.pointerLockElement) document.exitPointerLock();
  const m = Math.floor(playT / 60), s = Math.floor(playT % 60);
  $('stTime').textContent = (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;
  $('stDeaths').textContent = deaths;
  $('rank').textContent = playT < 240 ? 'S · ГИПЕРСУЩЕСТВО' : playT < 330 ? 'A · МЕЖПРОСТРАНСТВЕННЫЙ БЕГУН' : playT < 450 ? 'B · ХОДЯЧИЙ СРЕЗ' : 'C · ТРЁХМЕРЕЦ';
  $('win').classList.remove('hidden');
}

/* ---------------- физика ---------------- */
function curWOf(p) {
  const d = p.def;
  return d.anim ? d.baseW + Math.sin(worldT * d.spd + p.ph) * d.amp : (d.w == null ? 0 : d.w);
}
function overlapXZ(p) {
  return player.pos.x + PR > p.cx - p.hx && player.pos.x - PR < p.cx + p.hx &&
         player.pos.z + PR > p.cz - p.hz && player.pos.z - PR < p.cz + p.hz;
}
function resolveAxis(axis) {
  for (const p of platforms) {
    if (!p.solidNow) continue;
    if (!overlapXZ(p)) continue;
    if (player.pos.y + PH <= p.cy - p.hy || player.pos.y >= p.cy + p.hy) continue;
    if (axis === 'y') {
      if (player.vel.y <= 0 && player.pos.y < p.cy + p.hy && player.pos.y > p.cy + p.hy - 1.75) {
        player.pos.y = p.cy + p.hy; player.vel.y = 0;
        if (!player.onGround && worldT - lastLandT > 0.25) { Sfx.land(); lastLandT = worldT; }
        player.onGround = true; player.groundPlat = p;
        /* чекпоинт — только стабильные платформы (не аномальные) */
        if (!p.def.anim) {
          player.cp = { x: p.cx, y: p.cy + p.hy, z: p.cz, w: p.curW };
        }
      } else if (player.vel.y > 0) {
        player.pos.y = p.cy - p.hy - PH; player.vel.y = 0;
      }
    } else if (axis === 'x') {
      if (player.vel.x > 0) player.pos.x = p.cx - p.hx - PR - 0.001;
      else if (player.vel.x < 0) player.pos.x = p.cx + p.hx + PR + 0.001;
      player.vel.x = 0;
    } else {
      if (player.vel.z > 0) player.pos.z = p.cz - p.hz - PR - 0.001;
      else if (player.vel.z < 0) player.pos.z = p.cz + p.hz + PR + 0.001;
      player.vel.z = 0;
    }
  }
}
let lastLandT = -1;

function physics(dt) {
  /* снап к качающейся платформе */
  if (player.groundPlat) {
    const p = player.groundPlat;
    if (p.solidNow && overlapXZ(p) && Math.abs(player.pos.y - (p.cy + p.hy)) < 0.6) {
      player.pos.y = p.cy + p.hy;
    } else player.groundPlat = null;
  }
  /* ввод */
  let ix = 0, iz = 0;
  if (keys.KeyW || keys.ArrowUp) iz += 1;
  if (keys.KeyS || keys.ArrowDown) iz -= 1;
  if (keys.KeyD || keys.ArrowRight) ix += 1;
  if (keys.KeyA || keys.ArrowLeft) ix -= 1;
  ix += joyVec.x; iz -= joyVec.y;
  const fx_ = -Math.sin(player.yaw), fz_ = -Math.cos(player.yaw);
  let wx = fx_ * iz + (-fz_) * ix, wz = fz_ * iz + fx_ * ix;
  const wl = Math.hypot(wx, wz); if (wl > 1) { wx /= wl; wz /= wl; }
  const sprint = (keys.ShiftLeft || keys.ShiftRight) ? SPRINT : SPEED;
  const acc = player.onGround ? ACCG : ACCA;
  player.vel.x = lerp(player.vel.x, wx * sprint, Math.min(1, acc * dt));
  player.vel.z = lerp(player.vel.z, wz * sprint, Math.min(1, acc * dt));

  /* прыжок */
  player.coyote = player.onGround ? 0.12 : Math.max(0, player.coyote - dt);
  player.jbuf = Math.max(0, player.jbuf - dt);
  if (player.jbuf > 0 && player.coyote > 0) {
    player.vel.y = JUMP; player.jbuf = 0; player.coyote = 0;
    player.groundPlat = null; player.onGround = false;
    Sfx.jump();
  }
  player.vel.y = Math.max(-32, player.vel.y - G * dt);

  const wasGround = player.onGround;
  player.onGround = false;
  player.pos.y += player.vel.y * dt; resolveAxis('y');
  player.pos.x += player.vel.x * dt; resolveAxis('x');
  player.pos.z += player.vel.z * dt; resolveAxis('z');
  if (!player.onGround && player.groundPlat && player.vel.y <= 0) player.onGround = true;

  /* сдвиг по W */
  player.w = lerp(player.w, player.wTarget, Math.min(1, dt * 4.5));
  if (Math.abs(player.w - player.wTarget) < 0.004) player.w = player.wTarget;

  /* пустота */
  if (player.pos.y < -22 && player.dying <= 0) die(false);
}

/* ---------------- обновление мира/визуала ---------------- */
let worldT = 0;
const fogTarget = new THREE.Color();
const colTmp = new THREE.Color();

function updateWorld(dt) {
  worldT += dt;
  /* платформы: бобинг, фазировка, видимость */
  for (const p of platforms) {
    p.prevY = p.cy;
    p.cy = p.def.y + (p.def.bob ? Math.sin(worldT * p.spd + p.ph) * 0.28 : 0);
    p.cx = p.def.x; p.cz = p.def.z;
    p.curW = curWOf(p);
    p.mesh.position.y = p.cy; p.ghost.position.y = p.cy;
    p.solidNow = p.def.omni || Math.abs(player.w - p.curW) < 0.5;
    const dw = p.def.omni ? 0 : Math.abs(player.w - p.curW);
    if (p.def.omni) {
      p.mat.opacity = 1; p.eMat.opacity = 0.3; p.mesh.visible = true; p.ghost.visible = false;
    } else if (p.def.anim) {
      const pulse = 0.5 + 0.5 * Math.sin(worldT * p.spd + p.ph);
      const so = 1 - sstep(0.3, 0.72, dw);
      p.mat.opacity = so; p.mesh.visible = so > 0.02;
      p.mat.emissiveIntensity = 0.2 + pulse * 0.9; p.mat.depthWrite = so > 0.5;
      p.eMat.opacity = so * 0.45;
      p.gMat.opacity = (1 - so) * 0.5 + 0.1;
      p.ghost.visible = true;
    } else {
      const so = 1 - sstep(0.3, 0.72, dw);
      p.mat.opacity = so; p.mesh.visible = so > 0.02; p.mat.depthWrite = so > 0.5;
      p.eMat.opacity = so * 0.3;
      const go = sstep(0.42, 1.0, dw) * (1 - sstep(1.7, 2.6, dw));
      p.gMat.opacity = go * 0.34; p.ghost.visible = go > 0.015;
    }
  }
  /* кристаллы */
  for (const c of crystals) {
    if (c.taken) continue;
    const bob = Math.sin(worldT * 1.6 + c.ph) * 0.25;
    c.core.position.y = c.def.y + bob; c.ghost.position.y = c.def.y + bob;
    c.core.rotation.y = worldT * 1.4 + c.ph; c.core.rotation.x = worldT * 0.8;
    const dw = Math.abs(player.w - c.def.w);
    const so = 1 - sstep(0.4, 0.95, dw);
    c.core.visible = so > 0.02; c.glow.material.opacity = so * 0.8;
    const go = sstep(0.5, 1.2, dw) * (1 - sstep(2.0, 2.9, dw));
    c.ghost.material.opacity = go * 0.5; c.ghost.visible = go > 0.02;
    if (!c.taken && so > 0.5) {
      const dx = player.pos.x - c.core.position.x;
      const dy = player.pos.y + 0.9 - c.core.position.y;
      const dz = player.pos.z - c.core.position.z;
      if (dx * dx + dy * dy + dz * dz < 1.3 * 1.3) {
        c.taken = true; collected++;
        c.core.visible = false; c.glow.visible = false; c.ghost.visible = false;
        burst(c.core.position.x, c.core.position.y, c.core.position.z, LCOL[clamp(Math.round(c.def.w) + 2, 0, 4)]);
        Sfx.pickup();
        $('cnt').textContent = collected;
        if (collected >= TOTAL) {
          gateActive = true;
          Sfx.gate();
          gateMatA.color.set(0xffd75e); gateMatA.opacity = 1;
          ring2.material.color.set(0x00e5c0);
          gateTessMat.color.set(0xfff2b0);
          beamMat.opacity = 0.3;
          showHint(hints[5].txt, hints[5].dur || 9);
        }
      }
    }
  }
  /* эффекты */
  for (let i = fx.length - 1; i >= 0; i--) {
    const f = fx[i]; f.t += dt;
    const k = f.t / 0.55;
    f.s.scale.setScalar(1 + k * 7); f.s.material.opacity = 0.95 * (1 - k);
    if (k >= 1) { scene.remove(f.s); fx.splice(i, 1); }
  }
  /* тени */
  let danger = 0;
  for (const s of shadows) {
    const dx = player.pos.x - s.g.position.x, dz = player.pos.z - s.g.position.z;
    const d3 = Math.hypot(dx, player.pos.y + 0.9 - s.g.position.y, dz);
    const dw = Math.abs(player.w - s.w);
    s.aggro = state === 'play' && player.dying <= 0 && d3 < 16 && dw < 1.7;
    const spd = s.aggro ? 2.7 : 0.5;
    if (d3 > 0.1) {
      s.g.position.x += (dx / d3) * spd * dt * (s.aggro ? 1 : (Math.sin(worldT * 0.4 + s.ph) > 0 ? 1 : -0.4));
      s.g.position.z += (dz / d3) * spd * dt * (s.aggro ? 1 : (Math.sin(worldT * 0.4 + s.ph) > 0 ? 1 : -0.4));
    }
    s.g.position.y = lerp(s.g.position.y, player.pos.y + 1.1 + Math.sin(worldT * 2 + s.ph) * 0.4, Math.min(1, dt * 2));
    s.w += clamp(player.w - s.w, -0.22 * dt, 0.22 * dt) * (s.aggro ? 1 : 0.3);
    s.shell.rotation.y = worldT * 1.2 + s.ph; s.shell.rotation.x = worldT * 0.7;
    const vis = 1 - sstep(0.9, 1.7, dw);
    s.g.visible = vis > 0.02 && d3 < 60;
    s.core.material.opacity = vis; s.shell.material.opacity = vis * 0.95;
    s.glow.material.opacity = vis * (s.aggro ? 0.75 : 0.4);
    if (s.aggro) {
      danger = Math.max(danger, (1 - d3 / 16) * (1 - dw / 1.7));
      if (!aggroShown && d3 < 12) { aggroShown = true; showHint(hints[4].txt, 6); }
    }
    if (state === 'play' && player.dying <= 0 && player.invuln <= 0 && dw < 0.75 && d3 < 1.5) die(false);
  }
  $('vign').style.opacity = danger > 0.15 ? (0.25 + danger * 0.6) * (0.7 + 0.3 * Math.sin(worldT * 6)) : 0;

  /* врата */
  gate.rotation.y = worldT * 0.4;
  ring1.rotation.x = worldT * 0.7; ring2.rotation.z = worldT * 0.9;
  updateGateTess(worldT);
  const gs = gateActive ? 1 + Math.sin(worldT * 3) * 0.05 : 1;
  gate.scale.setScalar(gs);
  gateTess.scale.setScalar(gateActive ? 1.6 : 1);
  if (gateActive && Math.random() < dt * 8) {
    burst(GATE.x + (Math.random() - 0.5) * 4, GATE.y - 2 + Math.random() * 3, GATE.z + (Math.random() - 0.5) * 4, 0xffd75e);
  }
  if (gateActive && state === 'play') {
    const dx = player.pos.x - GATE.x, dy = player.pos.y + 0.9 - GATE.y, dz = player.pos.z - GATE.z;
    if (dx * dx + dy * dy + dz * dz < 2.4 * 2.4) winGame();
  }

  /* атмосфера слоя */
  const li = clamp(Math.round(player.w) + 2, 0, 4);
  fogTarget.setHex(0x05060f).lerp(colTmp.setHex(LCOL[li]), 0.24);
  fogCol.lerp(fogTarget, Math.min(1, dt * 2.5));
  scene.fog.color.copy(fogCol);
  gridMat.color.lerp(colTmp.setHex(LCOL[li]), Math.min(1, dt * 2.5));
  starsObj.rotation.y = worldT * 0.004;

  /* подсказки по триггерам */
  for (let i = 1; i < hints.length - 1; i++) {
    const h = hints[i];
    if (h.done || h.r === undefined) continue;
    const d = Math.hypot(player.pos.x - h.x, player.pos.y - h.y, player.pos.z - h.z);
    if (d < h.r) { h.done = true; showHint(h.txt, 7); }
  }
}

/* ---------------- HUD ---------------- */
let hudT = 0;
function updateHUD(dt) {
  hudT += dt;
  const m = Math.floor(playT / 60), s = Math.floor(playT % 60);
  $('time').textContent = (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;
  $('wval').textContent = 'W = ' + (player.w >= 0 ? '+' : '') + player.w.toFixed(2);
  const act = clamp(Math.round(player.w) + 2, 0, 4);
  slotEls.forEach((el, i) => el.classList.toggle('act', i === act));
  drawTess(worldT);
}
/* тессеракт на 2D-канвасе */
const tessC = $('tess'), tctx = tessC.getContext('2d');
function drawTess(t) {
  const S = tessC.width, c = S / 2;
  tctx.clearRect(0, 0, S, S);
  const a = t * 0.5 + player.w * 0.8, b = t * 0.33, g = t * 0.21;
  const ca = Math.cos(a), sa = Math.sin(a), cb = Math.cos(b), sb = Math.sin(b), cg = Math.cos(g), sg = Math.sin(g);
  const pts = TV.map(v => {
    const [x, y, z, u] = v;
    const x1 = x * ca - u * sa, u1 = x * sa + u * ca;
    const z1 = z * cb - u1 * sb, u2 = z * sb + u1 * cb;
    const y1 = y * cg - z1 * sg, z2 = y * sg + z1 * cg;
    const k = 1.9 / (1.9 - u2 * 0.75);
    return [c + x1 * k * 24, c + y1 * k * 24];
  });
  tctx.strokeStyle = '#' + new THREE.Color(LCOL[clamp(Math.round(player.w) + 2, 0, 4)]).getHexString();
  tctx.globalAlpha = 0.55; tctx.lineWidth = 1.2;
  tctx.beginPath();
  for (const e of TE) { tctx.moveTo(pts[e[0]][0], pts[e[0]][1]); tctx.lineTo(pts[e[1]][0], pts[e[1]][1]); }
  tctx.stroke();
  tctx.globalAlpha = 1;
}

/* ---------------- главный цикл ---------------- */
let lastT = performance.now();
function loop(now) {
  requestAnimationFrame(loop);
  let dt = (now - lastT) / 1000; lastT = now;
  dt = Math.min(dt, 0.05);
  if (state === 'play') {
    if (player.dying > 0) {
      player.dying -= dt;
      if (player.dying <= 0) respawn();
    } else {
      playT += dt;
      physics(dt);
    }
    if (player.invuln > 0) player.invuln -= dt;
    updateWorld(dt);
    updateHUD(dt);
    Sfx.tick();
  } else {
    updateWorld(dt * 0.35); /* мир живёт даже в меню */
  }
  /* камера */
  fovKick = Math.max(0, fovKick - dt * 18);
  const sprinting = (keys.ShiftLeft || keys.ShiftRight) && (keys.KeyW || keys.KeyA || keys.KeyS || keys.KeyD) && player.onGround !== undefined;
  const targetFov = 75 + (sprinting ? 5 : 0) + fovKick;
  camera.fov = lerp(camera.fov, targetFov, Math.min(1, dt * 8));
  camera.updateProjectionMatrix();
  const bob = (player.onGround && (keys.KeyW || keys.KeyA || keys.KeyS || keys.KeyD)) ? Math.sin(worldT * 11) * 0.045 : 0;
  camera.position.set(player.pos.x, player.pos.y + EYE + bob, player.pos.z);
  camera.rotation.set(0, 0, 0);
  camera.rotateY(player.yaw); camera.rotateX(player.pitch);
  renderer.render(scene, camera);
}
requestAnimationFrame(loop);

/* ресайз */
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});
})();
