/* ==========================================================================
   НЕЕВКЛИД — звук. Всё синтезируется WebAudio на лету: ни одного семпла.
   ========================================================================== */
(function (root) {
  'use strict';
  root.NE = root.NE || {};

  const A = {
    ctx: null, master: null, wet: null, dry: null, conv: null,
    drone: [], noiseSrc: null, noiseGain: null, doorOsc: null, doorGain: null,
    heartGain: null, heartOsc: null, whisperGain: null,
    zone: 0, ready: false, muted: false, lastStep: 0
  };

  function noiseBuffer(ctx, seconds, color) {
    const len = Math.floor(ctx.sampleRate * seconds);
    const buf = ctx.createBuffer(1, len, ctx.sampleRate);
    const d = buf.getChannelData(0);
    let b0 = 0, b1 = 0, b2 = 0;
    for (let i = 0; i < len; i++) {
      const w = Math.random() * 2 - 1;
      if (color === 'pink') {
        b0 = 0.99765 * b0 + w * 0.0990460;
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        d[i] = (b0 + b1 + b2 + w * 0.1848) * 0.22;
      } else d[i] = w;
    }
    return buf;
  }

  function impulse(ctx, seconds, decay) {
    const len = Math.floor(ctx.sampleRate * seconds);
    const buf = ctx.createBuffer(2, len, ctx.sampleRate);
    for (let c = 0; c < 2; c++) {
      const d = buf.getChannelData(c);
      for (let i = 0; i < len; i++) {
        d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, decay);
      }
    }
    return buf;
  }

  function init() {
    if (A.ready) return true;
    try {
      const Ctx = root.AudioContext || root.webkitAudioContext;
      if (!Ctx) return false;
      const ctx = new Ctx();
      A.ctx = ctx;

      A.master = ctx.createGain();
      A.master.gain.value = A.muted ? 0 : 0.9;
      const comp = ctx.createDynamicsCompressor();
      comp.threshold.value = -18; comp.ratio.value = 6; comp.attack.value = 0.004; comp.release.value = 0.25;
      A.master.connect(comp); comp.connect(ctx.destination);

      A.conv = ctx.createConvolver();
      A.conv.buffer = impulse(ctx, 3.2, 2.6);
      A.wet = ctx.createGain(); A.wet.gain.value = 0.42;
      A.conv.connect(A.wet); A.wet.connect(A.master);

      A.dry = ctx.createGain(); A.dry.gain.value = 1; A.dry.connect(A.master);

      /* низкий дрон */
      const base = [41.2, 61.7, 82.4, 123.5];
      base.forEach((f, i) => {
        const o = ctx.createOscillator();
        o.type = i % 2 ? 'sine' : 'triangle';
        o.frequency.value = f;
        o.detune.value = (i - 1.5) * 7;
        const g = ctx.createGain(); g.gain.value = 0.0;
        const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 260; lp.Q.value = 0.7;
        o.connect(g); g.connect(lp); lp.connect(A.dry); lp.connect(A.conv);
        o.start();
        A.drone.push({ o, g, lp, base: f });
      });

      /* воздушный шум */
      A.noiseSrc = ctx.createBufferSource();
      A.noiseSrc.buffer = noiseBuffer(ctx, 4, 'pink');
      A.noiseSrc.loop = true;
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 420; bp.Q.value = 0.6;
      A.noiseGain = ctx.createGain(); A.noiseGain.gain.value = 0.0;
      A.noiseSrc.connect(bp); bp.connect(A.noiseGain); A.noiseGain.connect(A.dry);
      A.noiseSrc.start();

      /* гул квантовой двери */
      A.doorOsc = ctx.createOscillator();
      A.doorOsc.type = 'sawtooth'; A.doorOsc.frequency.value = 880;
      const dq = ctx.createBiquadFilter(); dq.type = 'bandpass'; dq.frequency.value = 1600; dq.Q.value = 12;
      A.doorGain = ctx.createGain(); A.doorGain.gain.value = 0;
      A.doorOsc.connect(dq); dq.connect(A.doorGain); A.doorGain.connect(A.dry); A.doorGain.connect(A.conv);
      A.doorOsc.start();

      /* сердцебиение рядом с Наблюдателем */
      A.heartOsc = ctx.createOscillator();
      A.heartOsc.type = 'sine'; A.heartOsc.frequency.value = 48;
      A.heartGain = ctx.createGain(); A.heartGain.gain.value = 0;
      A.heartOsc.connect(A.heartGain); A.heartGain.connect(A.dry);
      A.heartOsc.start();

      /* шёпот */
      A.whisperSrc = ctx.createBufferSource();
      A.whisperSrc.buffer = noiseBuffer(ctx, 3, 'white');
      A.whisperSrc.loop = true;
      const wb = ctx.createBiquadFilter(); wb.type = 'bandpass'; wb.frequency.value = 1800; wb.Q.value = 6;
      A.whisperGain = ctx.createGain(); A.whisperGain.gain.value = 0;
      A.whisperSrc.connect(wb); wb.connect(A.whisperGain); A.whisperGain.connect(A.conv);
      A.whisperSrc.start();

      A.ready = true;
      setZone(0);
      return true;
    } catch (e) { console.warn('звук недоступен', e); return false; }
  }

  function resume() { if (A.ctx && A.ctx.state === 'suspended') A.ctx.resume(); }

  const ZONE_TONE = [
    { f: 240, det: 0,  noise: 0.018, rate: 1.00 },
    { f: 190, det: -18, noise: 0.026, rate: 0.86 },
    { f: 320, det: 24, noise: 0.014, rate: 1.18 },
    { f: 150, det: -34, noise: 0.034, rate: 0.72 }
  ];

  function setZone(z) {
    if (!A.ready) { A.zone = z; return; }
    A.zone = z;
    const t = ZONE_TONE[z] || ZONE_TONE[0];
    const now = A.ctx.currentTime;
    A.drone.forEach((d, i) => {
      d.g.gain.cancelScheduledValues(now);
      d.g.gain.setTargetAtTime(0.055 / (i + 1), now, 1.6);
      d.lp.frequency.setTargetAtTime(t.f * (1 + i * 0.4), now, 2.2);
      d.o.detune.setTargetAtTime(t.det * (i + 1) * 0.6, now, 2.0);
    });
    A.noiseGain.gain.setTargetAtTime(t.noise, now, 2.0);
  }

  function ambience(on) {
    if (!A.ready) return;
    const now = A.ctx.currentTime;
    A.master.gain.cancelScheduledValues(now);
    A.master.gain.setTargetAtTime(on && !A.muted ? 0.9 : 0.0, now, 0.5);
  }

  function env(node, t0, a, d, peak) {
    node.gain.cancelScheduledValues(t0);
    node.gain.setValueAtTime(0.0001, t0);
    node.gain.linearRampToValueAtTime(peak, t0 + a);
    node.gain.exponentialRampToValueAtTime(0.0001, t0 + a + d);
  }

  function tone(freq, t0, dur, type, peak, detune, send) {
    const ctx = A.ctx;
    const o = ctx.createOscillator();
    o.type = type || 'sine';
    o.frequency.setValueAtTime(freq, t0);
    if (detune) o.detune.value = detune;
    const g = ctx.createGain();
    o.connect(g);
    g.connect(A.dry);
    if (send) g.connect(A.conv);
    env(g, t0, Math.min(0.02, dur * 0.3), dur, peak);
    o.start(t0); o.stop(t0 + dur + 0.1);
    return o;
  }

  function burst(t0, dur, freq, q, peak, type) {
    const ctx = A.ctx;
    const s = ctx.createBufferSource();
    s.buffer = A._nb || (A._nb = noiseBuffer(ctx, 1, 'white'));
    const f = ctx.createBiquadFilter();
    f.type = type || 'bandpass'; f.frequency.value = freq; f.Q.value = q;
    const g = ctx.createGain();
    s.connect(f); f.connect(g); g.connect(A.dry); g.connect(A.conv);
    env(g, t0, 0.005, dur, peak);
    s.start(t0); s.stop(t0 + dur + 0.05);
  }

  /* ---------- события ---------- */
  function footstep(power) {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    burst(t, 0.09 + Math.random() * 0.05, 180 + Math.random() * 220, 1.1, 0.10 * power, 'lowpass');
    burst(t, 0.03, 2200 + Math.random() * 900, 3, 0.022 * power);
  }

  function pickup(n) {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    const scale = [0, 4, 7, 11, 14, 19];
    const root = 261.6;
    for (let k = 0; k < 4; k++) {
      const f = root * Math.pow(2, (scale[(n + k) % scale.length] + 12 * (k > 2 ? 1 : 0)) / 12);
      tone(f, t + k * 0.075, 1.5 - k * 0.2, 'sine', 0.16, 0, true);
      tone(f * 2.005, t + k * 0.075, 0.9, 'triangle', 0.05, 4, true);
    }
    burst(t, 0.5, 5200, 1.2, 0.05);
  }

  function whoosh(dir) {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    const ctx = A.ctx;
    const s = ctx.createBufferSource();
    s.buffer = A._nb || (A._nb = noiseBuffer(ctx, 1, 'white'));
    const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 2.2;
    f.frequency.setValueAtTime(dir > 0 ? 200 : 2600, t);
    f.frequency.exponentialRampToValueAtTime(dir > 0 ? 2600 : 200, t + 0.42);
    const g = ctx.createGain();
    s.connect(f); f.connect(g); g.connect(A.dry); g.connect(A.conv);
    env(g, t, 0.08, 0.5, 0.30);
    s.start(t); s.stop(t + 0.7);
    tone(dir > 0 ? 90 : 140, t, 0.6, 'sine', 0.22, 0, true);
    tone(dir > 0 ? 137 : 205, t + 0.02, 0.5, 'triangle', 0.08, 12, true);
  }

  function doorHum(level) {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    A.doorGain.gain.setTargetAtTime(level * 0.045, t, 0.08);
    A.doorOsc.frequency.setTargetAtTime(760 + level * 420, t, 0.15);
  }

  function angelDread(level) {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    A.whisperGain.gain.setTargetAtTime(level * 0.05, t, 0.25);
    A.heartGain.gain.setTargetAtTime(level * 0.0001, t, 0.1);
    A._dread = level;
  }

  function heartbeat() {
    if (!A.ready || !A._dread) return;
    const t = A.ctx.currentTime;
    const p = A._dread;
    tone(52, t, 0.16, 'sine', 0.30 * p, 0, false);
    tone(41, t + 0.19, 0.20, 'sine', 0.20 * p, 0, false);
  }

  function hurt() {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    [110, 116.5, 155.6, 233].forEach((f, i) => tone(f, t + i * 0.012, 1.1, 'sawtooth', 0.13, (i - 1) * 22, true));
    burst(t, 0.7, 700, 0.6, 0.28, 'lowpass');
    burst(t + 0.05, 0.5, 3600, 1.4, 0.10);
  }

  function denied() {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    tone(150, t, 0.25, 'square', 0.07, 0, false);
    tone(112, t + 0.09, 0.35, 'square', 0.06, 0, false);
  }

  function openDoor() {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    [130.8, 196, 261.6, 392, 523.3].forEach((f, i) => tone(f, t + i * 0.11, 2.6 - i * 0.2, 'sine', 0.14, 0, true));
    burst(t, 1.4, 300, 0.5, 0.16, 'lowpass');
  }

  function win() {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    [261.6, 329.6, 392, 523.3, 659.3, 784].forEach((f, i) => {
      tone(f, t + i * 0.14, 3.4, 'sine', 0.13, 0, true);
      tone(f * 1.5, t + i * 0.14 + 0.02, 2.4, 'triangle', 0.045, 3, true);
    });
    burst(t, 2.0, 900, 0.4, 0.10, 'lowpass');
  }

  function ping(level) {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    const f = 480 + level * 1180;
    tone(f, t, 0.16, 'sine', 0.028 + level * 0.05, 0, true);
    tone(f * 1.5, t + 0.01, 0.10, 'triangle', 0.012 + level * 0.02, 3, true);
  }

  function ui(up) {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    tone(up ? 620 : 380, t, 0.09, 'triangle', 0.06, 0, false);
  }

  function glitch() {
    if (!A.ready) return;
    const t = A.ctx.currentTime;
    for (let k = 0; k < 6; k++) burst(t + k * 0.028, 0.05, 300 + Math.random() * 4200, 8, 0.07);
  }

  function setMuted(m) {
    A.muted = m;
    if (A.master && A.ctx) A.master.gain.setTargetAtTime(m ? 0 : 0.9, A.ctx.currentTime, 0.08);
  }

  root.NE.Audio = {
    init, resume, setZone, ambience, footstep, pickup, whoosh, doorHum, ping,
    angelDread, heartbeat, hurt, denied, openDoor, win, ui, glitch, setMuted,
    get ready() { return A.ready; }
  };
})(typeof window !== 'undefined' ? window : globalThis);
