/* ==========================================================================
   Минимальный, но ЧЕСТНЫЙ стаб DOM + Canvas2D + WebAudio для headless-прогона
   игры. Canvas держит настоящий пиксельный буфер: fillRect/clearRect/
   getImageData/putImageData реально пишут и читают пиксели, поэтому
   процедурные текстуры и весь raycaster исполняются по-настоящему.
   ========================================================================== */
'use strict';

let napi = null;
try { napi = require('@napi-rs/canvas'); } catch (e) { napi = null; }

function parseColor(str) {
  if (typeof str !== 'string') return null;
  let m = /^#([0-9a-f]{6})$/i.exec(str.trim());
  if (m) {
    const v = parseInt(m[1], 16);
    return [(v >> 16) & 255, (v >> 8) & 255, v & 255, 255];
  }
  m = /^#([0-9a-f]{3})$/i.exec(str.trim());
  if (m) {
    const h = m[1];
    return [parseInt(h[0] + h[0], 16), parseInt(h[1] + h[1], 16), parseInt(h[2] + h[2], 16), 255];
  }
  m = /^rgba?\(([^)]+)\)$/i.exec(str.trim());
  if (m) {
    const p = m[1].split(',').map((s) => parseFloat(s));
    return [p[0] | 0, p[1] | 0, p[2] | 0, Math.round((p.length > 3 ? p[3] : 1) * 255)];
  }
  m = /^hsla?\(([^)]+)\)$/i.exec(str.trim());
  if (m) {
    const p = m[1].split(',').map((s) => parseFloat(s));
    const h = ((p[0] % 360) + 360) % 360 / 360, s = p[1], l = p[2];
    const a = p.length > 3 ? p[3] : 1;
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s, pp = 2 * l - q;
    const f = (t) => {
      if (t < 0) t += 1; if (t > 1) t -= 1;
      if (t < 1 / 6) return pp + (q - pp) * 6 * t;
      if (t < 1 / 2) return q;
      if (t < 2 / 3) return pp + (q - pp) * (2 / 3 - t) * 6;
      return pp;
    };
    return [(f(h + 1 / 3) * 255) | 0, (f(h) * 255) | 0, (f(h - 1 / 3) * 255) | 255, Math.round(a * 255)];
  }
  return null;
}

class Gradient {
  constructor() { this.stops = []; }
  addColorStop(o, c) { this.stops.push([o, c]); }
  color() {
    for (const s of this.stops) { const c = parseColor(s[1]); if (c && c[3] > 8) return c; }
    return [128, 128, 128, 255];
  }
}

class ImageDataStub {
  constructor(w, h, data) { this.width = w; this.height = h; this.data = data || new Uint8ClampedArray(w * h * 4); }
}

class Ctx2D {
  constructor(canvas) {
    this.canvas = canvas;
    this.fillStyle = '#000'; this.strokeStyle = '#000';
    this.lineWidth = 1; this.globalAlpha = 1; this.globalCompositeOperation = 'source-over';
    this.imageSmoothingEnabled = true; this.font = ''; this.textAlign = '';
    this._stack = [];
  }
  get _buf() { return this.canvas._buf; }
  _w() { return this.canvas.width; }
  _h() { return this.canvas.height; }
  _rgba() {
    const v = typeof this.fillStyle === 'string' ? parseColor(this.fillStyle)
      : (this.fillStyle && this.fillStyle.color ? this.fillStyle.color() : null);
    return v || [0, 0, 0, 255];
  }
  clearRect(x, y, w, h) {
    const W = this._w();
    for (let j = Math.max(0, y | 0); j < Math.min(this._h(), (y + h) | 0); j++)
      for (let i = Math.max(0, x | 0); i < Math.min(W, (x + w) | 0); i++) {
        const k = (j * W + i) * 4;
        this._buf[k] = this._buf[k + 1] = this._buf[k + 2] = this._buf[k + 3] = 0;
      }
  }
  fillRect(x, y, w, h) {
    if (w <= 0 || h <= 0) return;
    const [r, g, b, a] = this._rgba();
    const al = (a / 255) * this.globalAlpha;
    if (al <= 0) return;
    const W = this._w(), H = this._h();
    const x0 = Math.max(0, Math.floor(x)), y0 = Math.max(0, Math.floor(y));
    const x1 = Math.min(W, Math.ceil(x + w)), y1 = Math.min(H, Math.ceil(y + h));
    for (let j = y0; j < y1; j++) {
      for (let i = x0; i < x1; i++) {
        const k = (j * W + i) * 4;
        if (al >= 1) { this._buf[k] = r; this._buf[k + 1] = g; this._buf[k + 2] = b; this._buf[k + 3] = 255; }
        else {
          this._buf[k] = this._buf[k] * (1 - al) + r * al;
          this._buf[k + 1] = this._buf[k + 1] * (1 - al) + g * al;
          this._buf[k + 2] = this._buf[k + 2] * (1 - al) + b * al;
          this._buf[k + 3] = Math.max(this._buf[k + 3], a);
        }
      }
    }
  }
  strokeRect(x, y, w, h) { this.fillRect(x, y, w, 1); this.fillRect(x, y + h, w, 1); this.fillRect(x, y, 1, h); this.fillRect(x + w, y, 1, h); }
  getImageData(x, y, w, h) {
    const W = this._w();
    const out = new Uint8ClampedArray(w * h * 4);
    for (let j = 0; j < h; j++) {
      const sy = (y + j) * W + x;
      out.set(this._buf.subarray(sy * 4, sy * 4 + w * 4), j * w * 4);
    }
    return new ImageDataStub(w, h, out);
  }
  putImageData(img, x, y) {
    const W = this._w(), H = this._h(), w = img.width, h = img.height;
    for (let j = 0; j < h; j++) {
      const yy = y + j;
      if (yy < 0 || yy >= H) continue;
      for (let i = 0; i < w; i++) {
        const xx = x + i;
        if (xx < 0 || xx >= W) continue;
        const dk = (yy * W + xx) * 4, sk = (j * w + i) * 4;
        this._buf[dk] = img.data[sk]; this._buf[dk + 1] = img.data[sk + 1];
        this._buf[dk + 2] = img.data[sk + 2]; this._buf[dk + 3] = img.data[sk + 3];
      }
    }
  }
  createImageData(w, h) { return new ImageDataStub(w, h); }
  createLinearGradient() { return new Gradient(); }
  createRadialGradient() { return new Gradient(); }
  createPattern() { return null; }
  /* геометрия — заглушки, но обязаны существовать */
  beginPath() {} closePath() {} moveTo() {} lineTo() {} arc() {} ellipse() {}
  bezierCurveTo() {} quadraticCurveTo() {} rect() {} fill() {} stroke() {}
  clip() {} setLineDash() {} fillText() {} strokeText() {} measureText() { return { width: 10 }; }
  save() { this._stack.push([this.fillStyle, this.strokeStyle, this.lineWidth, this.globalAlpha]); }
  restore() { const s = this._stack.pop(); if (s) { this.fillStyle = s[0]; this.strokeStyle = s[1]; this.lineWidth = s[2]; this.globalAlpha = s[3]; } }
  translate() {} rotate() {} scale() {} setTransform() {} resetTransform() {}
  drawImage(src, sx, sy, sw, sh, dx, dy, dw, dh) {
    // упрощённо: копируем пиксели, если источник — наш canvas и аргументов 9
    if (!src || !src._buf) return;
    if (arguments.length === 9) {
      const tmp = new Uint8ClampedArray(sw * sh * 4);
      for (let j = 0; j < sh; j++) {
        const s = ((sy + j) * src.width + sx) * 4;
        tmp.set(src._buf.subarray(s, s + sw * 4), j * sw * 4);
      }
      this.putImageData(new ImageDataStub(sw, sh, tmp), dx, dy);
    } else if (arguments.length === 5) {
      const tmp = new Uint8ClampedArray(src.width * src.height * 4);
      tmp.set(src._buf);
      this.putImageData(new ImageDataStub(src.width, src.height, tmp), sx, sy);
    }
  }
}

class CanvasStub {
  constructor(w, h) {
    this._w = w || 300; this._h = h || 150;
    this._buf = new Uint8ClampedArray(this._w * this._h * 4);
    this._ctx = null;
    this.style = new StyleStub();
    this.classList = new ClassListStub();
    this.listeners = {};
    this.children = [];
    this.parentNode = null;
    this.textContent = ''; this.innerHTML = '';
    this.offsetWidth = this._w;
  }
  get width() { return this._w; }
  set width(v) { v = Math.max(1, v | 0); if (v !== this._w) { this._w = v; this._realloc(); } }
  get height() { return this._h; }
  set height(v) { v = Math.max(1, v | 0); if (v !== this._h) { this._h = v; this._realloc(); } }
  _realloc() {
    const nb = new Uint8ClampedArray(this._w * this._h * 4);
    if (this._ctx) this._ctx = new Ctx2D(this);
    this._buf = nb;
  }
  getContext() { if (!this._ctx) this._ctx = new Ctx2D(this); return this._ctx; }
  toDataURL() { return 'data:image/png;base64,'; }
  addEventListener(t, f) { (this.listeners[t] = this.listeners[t] || []).push(f); }
  removeEventListener() {}
  appendChild(c) { this.children.push(c); c.parentNode = this; return c; }
  getBoundingClientRect() { return { left: 0, top: 0, width: this._w, height: this._h, right: this._w, bottom: this._h }; }
  requestPointerLock() { return undefined; }
  dispatch(t, ev) { (this.listeners[t] || []).forEach((f) => f(ev)); }
}

class StyleStub {
  constructor() { this._p = {}; return new Proxy(this, {
    get(t, k) { return k === '_p' ? t._p : (t._p[k] || ''); },
    set(t, k, v) { t._p[k] = v; return true; }
  }); }
}
class ClassListStub {
  constructor() { this.s = new Set(); }
  add(c) { this.s.add(c); } remove(c) { this.s.delete(c); }
  contains(c) { return this.s.has(c); }
  toggle(c, f) { if (f === undefined) f = !this.s.has(c); f ? this.s.add(c) : this.s.delete(c); return f; }
}

/* ------------------------- настоящий canvas (если стоит @napi-rs/canvas) ------------------------- */
class RealCanvas {
  constructor(w, h) {
    this._c = napi.createCanvas(Math.max(1, w | 0) || 300, Math.max(1, h | 0) || 150);
    this.style = new StyleStub();
    this.classList = new ClassListStub();
    this.listeners = {};
    this.children = [];
    this.parentNode = null;
    this.textContent = ''; this.innerHTML = '';
    this.offsetWidth = this._c.width;
  }
  get width() { return this._c.width; }
  set width(v) { this._c.width = Math.max(1, v | 0); this.offsetWidth = this._c.width; }
  get height() { return this._c.height; }
  set height(v) { this._c.height = Math.max(1, v | 0); }
  /* контекст заворачиваем в прокси: drawImage и пр. должны получать настоящий
     napi-canvas, а не нашу обёртку */
  getContext(t) {
    const raw = this._c.getContext(t || '2d');
    if (this._ctxProxy) return this._ctxProxy;
    const unwrap = (a) => (a instanceof RealCanvas ? a._c : a);
    this._ctxProxy = new Proxy(raw, {
      get(target, prop) {
        const v = target[prop];
        if (typeof v === 'function') return function (...args) { return v.apply(target, args.map(unwrap)); };
        return v;
      },
      set(target, prop, value) { target[prop] = unwrap(value); return true; }
    });
    return this._ctxProxy;
  }
  toDataURL(m) { return this._c.toDataURL(m || 'image/png'); }
  toBuffer(m) { return this._c.toBuffer(m || 'image/png'); }
  addEventListener(t, f) { (this.listeners[t] = this.listeners[t] || []).push(f); }
  removeEventListener() {}
  appendChild(c) { this.children.push(c); c.parentNode = this; return c; }
  getBoundingClientRect() { return { left: 0, top: 0, width: this._c.width, height: this._c.height, right: this._c.width, bottom: this._c.height }; }
  requestPointerLock() { return undefined; }
  dispatch(t, ev) { (this.listeners[t] || []).forEach((f) => f(ev)); }
  get parentElement() { return this.parentNode || null; }
}

class ElStub extends CanvasStub {
  constructor(tag) { super(1, 1); this.tagName = (tag || 'div').toUpperCase(); this.value = ''; }
  get parentElement() { return this.parentNode || null; }
}

/* ------------------------- WebAudio ------------------------- */
class ParamStub {
  constructor(v) { this.value = v === undefined ? 0 : v; this.calls = 0; }
  setValueAtTime(v) { this._chk(v); this.value = v; this.calls++; return this; }
  linearRampToValueAtTime(v) { this._chk(v); this.value = v; this.calls++; return this; }
  exponentialRampToValueAtTime(v) {
    if (v === 0) throw new RangeError('exponentialRampToValueAtTime(0)');
    this._chk(v); this.value = v; this.calls++; return this;
  }
  setTargetAtTime(v) { this._chk(v); this.value = v; this.calls++; return this; }
  cancelScheduledValues() { this.calls++; return this; }
  _chk(v) { if (!isFinite(v)) throw new TypeError('неконечное значение параметра: ' + v); }
}
class NodeStub {
  constructor(ctx) { this.ctx = ctx; this._out = []; }
  connect(n) { this._out.push(n); return n; }
  disconnect() {}
}
class GainStub extends NodeStub { constructor(c) { super(c); this.gain = new ParamStub(1); } }
class FilterStub extends NodeStub {
  constructor(c) { super(c); this.type = 'lowpass'; this.frequency = new ParamStub(350); this.Q = new ParamStub(1); this.detune = new ParamStub(0); }
}
class OscStub extends NodeStub {
  constructor(c) { super(c); this.type = 'sine'; this.frequency = new ParamStub(440); this.detune = new ParamStub(0); this.started = false; this.stopped = false; }
  start(t) { if (this.started) throw new Error('двойной start()'); if (t !== undefined && !isFinite(t)) throw new TypeError('start(NaN)'); this.started = true; this.ctx._started++; }
  stop(t) { if (!this.started) throw new Error('stop() без start()'); if (t !== undefined && !isFinite(t)) throw new TypeError('stop(NaN)'); this.stopped = true; }
}
class SrcStub extends OscStub {
  constructor(c) { super(c); this.buffer = null; this.loop = false; this.playbackRate = new ParamStub(1); }
}
class BufferStub {
  constructor(ch, len, rate) { this.numberOfChannels = ch; this.length = len; this.sampleRate = rate; this._d = []; for (let i = 0; i < ch; i++) this._d.push(new Float32Array(len)); }
  getChannelData(i) { return this._d[i]; }
}
class ConvolverStub extends NodeStub { constructor(c) { super(c); this.buffer = null; } }
class CompStub extends NodeStub {
  constructor(c) { super(c); this.threshold = new ParamStub(-24); this.ratio = new ParamStub(12); this.attack = new ParamStub(0.003); this.release = new ParamStub(0.25); this.knee = new ParamStub(30); }
}
class AudioContextStub {
  constructor() {
    this.sampleRate = 48000; this.state = 'running'; this._started = 0;
    this.destination = new NodeStub(this);
    this._t = 0;
  }
  get currentTime() { return this._t += 0.0002; }
  createGain() { return new GainStub(this); }
  createBiquadFilter() { return new FilterStub(this); }
  createOscillator() { return new OscStub(this); }
  createBufferSource() { return new SrcStub(this); }
  createConvolver() { return new ConvolverStub(this); }
  createDynamicsCompressor() { return new CompStub(this); }
  createBuffer(ch, len, rate) {
    if (!(len > 0)) throw new RangeError('createBuffer: длина ' + len);
    return new BufferStub(ch, len, rate);
  }
  resume() { this.state = 'running'; return Promise.resolve(); }
  close() { this.state = 'closed'; return Promise.resolve(); }
}

/* ------------------------- document / window ------------------------- */
function makeSandbox(opts) {
  opts = opts || {};
  const ids = {};
  const documentStub = {
    readyState: 'complete',
    pointerLockElement: null,
    hidden: false,
    listeners: {},
    createElement(tag) {
      if (tag === 'canvas') return napi ? new RealCanvas(300, 150) : new CanvasStub(300, 150);
      return new ElStub(tag);
    },
    getElementById(id) {
      if (!ids[id]) {
        const isCanvas = (id === 'view' || id === 'mini' || id === 'trueMap');
        const el = isCanvas ? (napi ? new RealCanvas(64, 36) : new CanvasStub(64, 36)) : new ElStub('div');
        if (id === 'mini') { el.width = 152; el.height = 152; }
        if (id === 'trueMap') { el.width = 624; el.height = 464; }
        ids[id] = el;
      }
      return ids[id];
    },
    addEventListener(t, f) { (documentStub.listeners[t] = documentStub.listeners[t] || []).push(f); },
    removeEventListener() {},
    exitPointerLock() { documentStub.pointerLockElement = null; },
    dispatch(t, ev) { (documentStub.listeners[t] || []).forEach((f) => f(ev)); }
  };

  const rafQueue = [];
  let clock = 0;
  const winListeners = {};
  const sandbox = {
    console,
    document: documentStub,
    performance: { now: () => clock },
    innerWidth: opts.w || 1280,
    innerHeight: opts.h || 720,
    devicePixelRatio: 1,
    matchMedia: () => ({ matches: false, addListener() {}, addEventListener() {} }),
    listeners: winListeners,
    // именно функции, а не методы: в strict-режиме голый вызов addEventListener(...)
    // даёт this === undefined, как и в настоящем окне
    addEventListener: (t, f) => { (winListeners[t] = winListeners[t] || []).push(f); },
    removeEventListener: () => {},
    dispatch: (t, ev) => { (winListeners[t] || []).forEach((f) => f(ev)); },
    requestAnimationFrame(cb) { rafQueue.push(cb); return rafQueue.length; },
    cancelAnimationFrame() {},
    setTimeout: (f, ms) => setTimeout(f, 0),
    clearTimeout: (t) => clearTimeout(t),
    setInterval: (f, ms) => setInterval(f, 1),
    clearInterval: (t) => clearInterval(t),
    AudioContext: AudioContextStub,
    Math, Date, JSON, Uint8Array, Uint8ClampedArray, Uint16Array, Uint32Array,
    Int8Array, Int16Array, Int32Array, Float32Array, Float64Array, ArrayBuffer, Proxy, Set, Map,
    isFinite, isNaN, parseFloat, parseInt, Promise, Error, RangeError, TypeError, Object, Array, String, Number, Boolean
  };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  sandbox.self = sandbox;
  sandbox.navigator = { userAgent: 'node-harness' };

  const api = {
    sandbox, document: documentStub, ids,
    pump(frames, dtMs) {
      for (let i = 0; i < frames; i++) {
        clock += dtMs === undefined ? 16.7 : dtMs;
        const q = rafQueue.splice(0, rafQueue.length);
        if (!q.length) throw new Error('rAF-очередь пуста: игровой цикл остановился');
        for (const cb of q) cb(clock);
      }
    },
    clock: () => clock,
    dispatch: (t, ev) => (winListeners[t] || []).forEach((f) => f(ev)),
    docDispatch: (t, ev) => documentStub.dispatch(t, ev)
  };
  return api;
}

module.exports = { makeSandbox, CanvasStub, RealCanvas, ElStub, AudioContextStub, parseColor, napi };
