// Крошечный синтезатор: без ассетов, всё генерируется на месте.

let ctx = null;
let master = null;
let drone = null;
let muted = false;

function ensure() {
  if (ctx) return ctx;
  const AC = window.AudioContext || window.webkitAudioContext;
  if (!AC) return null;
  try {
    ctx = new AC();
    master = ctx.createGain();
    master.gain.value = 0.55;
    master.connect(ctx.destination);
  } catch (e) {
    ctx = null;
  }
  return ctx;
}

function tone({ freq = 440, type = 'sine', dur = 0.18, gain = 0.2, slide = 0, delay = 0 }) {
  if (muted) return;
  const c = ensure();
  if (!c) return;
  const t0 = c.currentTime + delay;
  const osc = c.createOscillator();
  const g = c.createGain();
  osc.type = type;
  osc.frequency.setValueAtTime(freq, t0);
  if (slide) osc.frequency.exponentialRampToValueAtTime(Math.max(30, freq + slide), t0 + dur);
  g.gain.setValueAtTime(0.0001, t0);
  g.gain.exponentialRampToValueAtTime(gain, t0 + 0.012);
  g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
  osc.connect(g).connect(master);
  osc.start(t0);
  osc.stop(t0 + dur + 0.05);
}

function noiseBurst({ dur = 0.25, gain = 0.15, freq = 900, delay = 0 }) {
  if (muted) return;
  const c = ensure();
  if (!c) return;
  const t0 = c.currentTime + delay;
  const len = Math.floor(c.sampleRate * dur);
  const buf = c.createBuffer(1, len, c.sampleRate);
  const d = buf.getChannelData(0);
  for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / len) ** 2;
  const src = c.createBufferSource();
  src.buffer = buf;
  const bp = c.createBiquadFilter();
  bp.type = 'bandpass';
  bp.frequency.value = freq;
  bp.Q.value = 1.2;
  const g = c.createGain();
  g.gain.value = gain;
  src.connect(bp).connect(g).connect(master);
  src.start(t0);
}

export const Sfx = {
  unlock() {
    const c = ensure();
    if (c && c.state === 'suspended') c.resume();
    if (c && !drone) Sfx.startDrone();
  },
  startDrone() {
    const c = ensure();
    if (!c) return;
    const g = c.createGain();
    g.gain.value = 0.0;
    g.gain.linearRampToValueAtTime(0.05, c.currentTime + 3);
    const filter = c.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 420;
    filter.Q.value = 3;
    const a = c.createOscillator();
    a.type = 'sawtooth';
    a.frequency.value = 55;
    const b = c.createOscillator();
    b.type = 'sine';
    b.frequency.value = 82.6;
    const lfo = c.createOscillator();
    lfo.frequency.value = 0.07;
    const lfoGain = c.createGain();
    lfoGain.gain.value = 220;
    lfo.connect(lfoGain).connect(filter.frequency);
    a.connect(filter);
    b.connect(filter);
    filter.connect(g).connect(master);
    a.start();
    b.start();
    lfo.start();
    drone = { g };
  },
  pickup(step = 0) {
    const scale = [523.25, 587.33, 659.25, 783.99, 880, 1046.5];
    const f = scale[Math.min(step, scale.length - 1)];
    tone({ freq: f, type: 'triangle', dur: 0.22, gain: 0.16 });
    tone({ freq: f * 2, type: 'sine', dur: 0.3, gain: 0.06, delay: 0.02 });
  },
  jump() {
    tone({ freq: 260, type: 'sine', dur: 0.16, gain: 0.08, slide: 180 });
  },
  hit() {
    noiseBurst({ dur: 0.3, gain: 0.2, freq: 260 });
    tone({ freq: 110, type: 'square', dur: 0.22, gain: 0.1, slide: -50 });
  },
  denied() {
    tone({ freq: 180, type: 'square', dur: 0.12, gain: 0.07 });
    tone({ freq: 140, type: 'square', dur: 0.16, gain: 0.07, delay: 0.13 });
  },
  lap() {
    tone({ freq: 392, type: 'sine', dur: 0.5, gain: 0.1 });
    tone({ freq: 587.33, type: 'sine', dur: 0.6, gain: 0.08, delay: 0.08 });
    noiseBurst({ dur: 0.5, gain: 0.06, freq: 1800 });
  },
  flip() {
    noiseBurst({ dur: 0.55, gain: 0.16, freq: 520 });
    tone({ freq: 196, type: 'sine', dur: 0.7, gain: 0.1, slide: -70 });
    tone({ freq: 783.99, type: 'triangle', dur: 0.9, gain: 0.07, delay: 0.05 });
    tone({ freq: 1046.5, type: 'sine', dur: 1.1, gain: 0.05, delay: 0.12 });
  },
  gateHint() {
    tone({ freq: 659.25, type: 'sine', dur: 0.45, gain: 0.08 });
    tone({ freq: 987.77, type: 'sine', dur: 0.6, gain: 0.05, delay: 0.09 });
  },
  win() {
    [523.25, 659.25, 783.99, 1046.5, 1318.5].forEach((f, i) =>
      tone({ freq: f, type: 'triangle', dur: 1.6, gain: 0.11, delay: i * 0.13 })
    );
    noiseBurst({ dur: 1.2, gain: 0.08, freq: 2400 });
  },
  toggleMute() {
    muted = !muted;
    if (master) master.gain.value = muted ? 0 : 0.55;
    return muted;
  },
  get muted() {
    return muted;
  },
};
