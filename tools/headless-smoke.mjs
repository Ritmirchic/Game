// Dev tool: headless smoke test. Emulates just enough DOM + WebGL to run the
// whole game loop for a few thousand frames and catch runtime errors.
import fs from 'node:fs';
import path from 'node:path';

const root = path.resolve(import.meta.dirname, '..');
const stats = { draws: 0, buffers: 0, textures: 0, frames: 0, errors: [] };

/* ---------------- DOM mock ---------------- */
function classList() {
  const set = new Set();
  return {
    add: (...c) => c.forEach((x) => set.add(x)),
    remove: (...c) => c.forEach((x) => set.delete(x)),
    toggle: (c, v) => (v === undefined ? (set.has(c) ? set.delete(c) : set.add(c)) : v ? set.add(c) : set.delete(c)),
    contains: (c) => set.has(c),
    _set: set
  };
}
const elements = new Map();
function makeEl(id) {
  const el = {
    id,
    style: new Proxy({ setProperty(k, v) { this['_' + k] = v; }, removeProperty() {} }, { get: (t, k) => (k in t ? t[k] : ''), set: (t, k, v) => ((t[k] = v), true) }),
    classList: classList(),
    textContent: '',
    innerHTML: '',
    children: [],
    appendChild(c) { this.children.push(c); c.parentNode = this; return c; },
    removeChild(c) { this.children = this.children.filter((x) => x !== c); },
    get firstChild() { return this.children[0]; },
    _ls: {},
    addEventListener(type, fn) { (this._ls[type] = this._ls[type] || []).push(fn); },
    removeEventListener() {},
    setProperty() {},
    getContext: () => glProxy,
    requestPointerLock() {},
    clientWidth: 1280,
    clientHeight: 720,
    width: 1280,
    height: 720,
    focus() {}
  };
  elements.set(id, el);
  return el;
}
const document = {
  _ls: {},
  fire(type, ev) { (this._ls[type] || []).forEach((f) => f(Object.assign({ preventDefault() {}, stopPropagation() {} }, ev))); },
  addEventListener(type, fn) { (this._ls[type] = this._ls[type] || []).push(fn); },
  readyState: 'complete',
  getElementById: (id) => elements.get(id) || makeEl(id),
  createElement: (tag) => makeEl('<' + tag + '>'),
  body: makeEl('body'),
  pointerLockElement: null,
  exitPointerLock() {}
};

/* ---------------- WebGL mock ---------------- */
const CONST = { LINK_STATUS: 0x8b82, COMPILE_STATUS: 0x8b81 };
let constN = 100;
const SHADERS = [];
const PROGRAMS = [];
const glProxy = new Proxy({}, {
  get(target, key) {
    if (typeof key !== 'string') return undefined;
    if (key in target) return target[key];
    // constants
    if (/^[A-Z0-9_]+$/.test(key)) {
      if (!(key in CONST)) CONST[key] = constN++;
      return CONST[key];
    }
    const fn = (...args) => {
      switch (key) {
        case 'getShaderParameter': SHADERS.push(args[0]); return true;
        case 'getProgramParameter': return args[1] === 0x8b82 || args[1] === 0x8b81 ? true : 0; // LINK/COMPILE
        case 'getShaderInfoLog': return '';
        case 'getProgramInfoLog': return '';
        case 'getProgramParameter_dummy': return 0;
        case 'createShader': return { src: '' };
        case 'createProgram': return { p: true };
        case 'createBuffer': return { b: stats.buffers++ };
        case 'createTexture': return { t: stats.textures++ };
        case 'createFramebuffer': return { f: true };
        case 'createRenderbuffer': return { r: true };
        case 'getUniformLocation': return { loc: args[1] };
        case 'getAttribLocation': return 0;
        case 'drawArrays': stats.draws++; return undefined;
        case 'getExtension': return { ANGLE_instanced_arrays: true, OES_vertex_array_object: true };
        case 'getParameter': return 8;
        case 'getError': return 0;
        default: return undefined;
      }
    };
    target[key] = fn;
    return fn;
  }
});

/* ---------------- window mock ---------------- */
let rafQueue = [];
let now = 0;
const window = {
  devicePixelRatio: 1,
  _ls: {},
  addEventListener(type, fn) { (this._ls[type] = this._ls[type] || []).push(fn); },
  removeEventListener() {},
  requestAnimationFrame: (cb) => { rafQueue.push(cb); return rafQueue.length; },
  AudioContext: null,
  performance: { now: () => now },
  document,
  setTimeout: (fn) => { try { fn(); } catch (e) { stats.errors.push(e); } },
  navigator: { userAgent: 'headless' }
};
globalThis.window = window;
globalThis.document = document;
globalThis.requestAnimationFrame = (cb) => { rafQueue.push(cb); return rafQueue.length; };
globalThis.setTimeout = (fn) => 1;
globalThis.performance = { now: () => now };
globalThis.AudioContext = undefined;

/* ---------------- run ---------------- */
// SINGLE=MEMORIA.html runs the *built* single-file distribution instead of the sources
const single = process.env.SINGLE;
let sources = [];
if (single) {
  const html = fs.readFileSync(path.join(root, single), 'utf8');
  const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map((m) => m[1]);
  console.log('single-file build:', single, '->', blocks.length, 'inline scripts,', (html.length / 1024).toFixed(1), 'KB');
  sources = blocks.map((code, i) => ['inline#' + (i + 1), code]);
} else {
  sources = ['src/world.js', 'src/gl.js', 'src/audio.js', 'src/game.js']
    .map((f) => [f, fs.readFileSync(path.join(root, f), 'utf8')]);
}

for (const [f, src] of sources) {
  try {
    new Function(src).call(globalThis);
  } catch (e) {
    console.error('LOAD FAIL', f, e);
    process.exit(1);
  }
  // expose everything the scripts published on `window` as globals, like a browser would
  for (const k of Object.keys(window)) {
    try { globalThis[k] = window[k]; } catch (e) { /* read-only global */ }
  }
  if (window.MEM) globalThis.MEM = window.MEM;
  if (window.GL) globalThis.GL = window.GL;
  if (window.M4) globalThis.M4 = window.M4;
}

const canvasEl = elements.get('gl') || makeEl('gl');
const startBtn = elements.get('startBtn');
const mode = process.argv[3] || 'play';

function frames(n) {
  for (let i = 0; i < n; i++) {
    now += 1000 / 60;
    const q = rafQueue; rafQueue = [];
    for (const cb of q) {
      try { cb(now); stats.frames++; } catch (e) { if (stats.errors.length < 4) stats.errors.push(e); }
    }
    if (!rafQueue.length) break;
  }
}
function key(code, down) {
  document.fire(down ? 'keydown' : 'keyup', { code });
}

// menu frames
frames(60);
if (mode === 'play' || mode === 'win' || mode === 'lose') {
  document.fire('click', {}); // stray click (should be ignored in menu)
  startBtn._ls.click.forEach((f) => f({ preventDefault() {}, stopPropagation() {} }));
  frames(10);
  // simulate a walk: turning + WASD
  key('KeyW', true);
  if (globalThis.__MEMDBG) console.log('dbg after start', __MEMDBG.state, __MEMDBG.keys.KeyW, __MEMDBG.pos);
  for (let i = 0; i < 40; i++) {
    // rotate by firing mousemove on document
    document.fire('mousemove', { movementX: 6, movementY: 0 });
    frames(10);
  }
  console.log('dbg after walk', __MEMDBG.pos, __MEMDBG.vel, __MEMDBG.yaw, __MEMDBG.state);
  key('ShiftLeft', true);
  frames(120);
  key('Space', true);  frames(30);
  key('Space', false); frames(60);
  key('KeyE', true); key('KeyE', false);
  frames(120);
  if (mode === 'lose' || mode === 'win') {
    frames(60 * 320); // fast-forward past the 5 minute timer
  }
}
frames(240);

const FRAMES = 0;
for (let i = 0; i < FRAMES; i++) {
  now += 1000 / 60;
  const q = rafQueue; rafQueue = [];
  for (const cb of q) {
    try { cb(now); stats.frames++; } catch (e) { stats.errors.push(e); }
  }
  if (!rafQueue.length) { console.error('loop stopped at frame', i); break; }
}

for (const id of ['pct', 'count', 'timer', 'posx', 'posz', 'modeLabel', 'verdict', 'sPct', 'sTime', 'sPulses', 'sSteps']) {
  const el = elements.get(id);
  if (el) console.log(id.padEnd(9), JSON.stringify(el.textContent));
}
console.log('end hidden?', elements.get('end') && elements.get('end').classList.contains('hidden'));
console.log(JSON.stringify(stats, (k, v) => (v instanceof Error ? v.message + '\n' + v.stack : v), 2));
if (stats.errors.length) process.exitCode = 1;
