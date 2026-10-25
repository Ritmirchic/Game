/* =========================================================================
   SPACE MINER — fully synthesised audio (no asset files).
   Ambient generative score + spatialised SFX.
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  function Audio() {
    this.ctx = null;
    this.ready = false;
    this.master = null;
    this.sfxGain = null;
    this.musicGain = null;
    this.enabled = { sfx: true, music: true };
    this.sector = null;
    this.musicTimer = 0;
    this.step = 0;
    this.beamNode = null;
    this.beamGain = null;
    this.engineNode = null;
    this.engineGain = null;
    this.volume = 0.7;
    this.lastPlay = {};
  }

  Audio.prototype.init = function () {
    if (this.ready) return true;
    var AC = root.AudioContext || root.webkitAudioContext;
    if (!AC) return false;
    try {
      this.ctx = new AC();
      this.master = this.ctx.createGain();
      this.master.gain.value = this.volume;
      this.master.connect(this.ctx.destination);
      this.sfxGain = this.ctx.createGain();
      this.sfxGain.gain.value = 0.85;
      this.sfxGain.connect(this.master);
      this.musicGain = this.ctx.createGain();
      this.musicGain.gain.value = 0.32;
      this.musicGain.connect(this.master);
      this.ready = true;
      this.buildEngine();
      return true;
    } catch (e) {
      return false;
    }
  };

  Audio.prototype.resume = function () {
    if (!this.ready) this.init();
    if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume();
  };

  Audio.prototype.setSettings = function (s) {
    this.enabled.sfx = s.sfx !== false;
    this.enabled.music = s.music !== false;
    if (this.master) {
      this.master.gain.value = this.volume;
    }
    if (this.musicGain) this.musicGain.gain.value = this.enabled.music ? 0.32 : 0;
    if (this.sfxGain) this.sfxGain.gain.value = this.enabled.sfx ? 0.85 : 0;
  };

  /* ---------- helpers ---------- */
  Audio.prototype.now = function () { return this.ctx ? this.ctx.currentTime : 0; };

  Audio.prototype.env = function (gain, t0, peak, attack, decay, sustainLevel, sustainTime) {
    gain.gain.cancelScheduledValues(t0);
    gain.gain.setValueAtTime(0.0001, t0);
    gain.gain.exponentialRampToValueAtTime(Math.max(0.0002, peak), t0 + attack);
    if (sustainTime) gain.gain.setValueAtTime(Math.max(0.0002, peak * (sustainLevel || 0.6)), t0 + attack + sustainTime);
    gain.gain.exponentialRampToValueAtTime(0.0001, t0 + attack + decay);
  };

  Audio.prototype.tone = function (o) {
    if (!this.ready || !this.enabled.sfx) return;
    var c = this.ctx, t0 = this.now() + (o.delay || 0);
    var osc = c.createOscillator();
    osc.type = o.type || 'sine';
    osc.frequency.setValueAtTime(o.f0, t0);
    if (o.f1 !== undefined) osc.frequency.exponentialRampToValueAtTime(Math.max(20, o.f1), t0 + (o.dur || 0.2));
    var g = c.createGain();
    var vol = (o.vol === undefined ? 0.25 : o.vol);
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(Math.max(0.0002, vol), t0 + (o.attack || 0.006));
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + (o.dur || 0.2));
    var out = g;
    if (o.filter) {
      var f = c.createBiquadFilter();
      f.type = o.filter;
      f.frequency.value = o.cutoff || 800;
      f.Q.value = o.q || 1;
      g.connect(f);
      out = f;
    }
    if (o.pan !== undefined && c.createStereoPanner) {
      var pan = c.createStereoPanner();
      pan.pan.value = Math.max(-1, Math.min(1, o.pan));
      out.connect(pan);
      pan.connect(this.sfxGain);
    } else {
      out.connect(this.sfxGain);
    }
    osc.connect(g);
    osc.start(t0);
    osc.stop(t0 + (o.dur || 0.2) + 0.05);
  };

  Audio.prototype.noise = function (o) {
    if (!this.ready || !this.enabled.sfx) return;
    var c = this.ctx, t0 = this.now() + (o.delay || 0);
    var dur = o.dur || 0.3;
    var buf = c.createBuffer(1, Math.ceil(c.sampleRate * dur), c.sampleRate);
    var data = buf.getChannelData(0);
    for (var i = 0; i < data.length; i++) data[i] = (Math.random() * 2 - 1) * (1 - i / data.length);
    var src = c.createBufferSource();
    src.buffer = buf;
    var f = c.createBiquadFilter();
    f.type = o.filter || 'lowpass';
    f.frequency.setValueAtTime(o.f0 || 2400, t0);
    if (o.f1) f.frequency.exponentialRampToValueAtTime(Math.max(40, o.f1), t0 + dur);
    f.Q.value = o.q || 0.8;
    var g = c.createGain();
    var vol = o.vol === undefined ? 0.3 : o.vol;
    g.gain.setValueAtTime(vol, t0);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    src.connect(f); f.connect(g);
    if (o.pan !== undefined && c.createStereoPanner) {
      var p = c.createStereoPanner();
      p.pan.value = Math.max(-1, Math.min(1, o.pan));
      g.connect(p); p.connect(this.sfxGain);
    } else g.connect(this.sfxGain);
    src.start(t0);
    src.stop(t0 + dur + 0.02);
  };

  /* ---------- continuous voices ---------- */
  Audio.prototype.buildEngine = function () {
    if (!this.ready) return;
    var c = this.ctx;
    var osc = c.createOscillator();
    osc.type = 'sawtooth';
    osc.frequency.value = 46;
    var f = c.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.value = 220;
    var g = c.createGain();
    g.gain.value = 0;
    osc.connect(f); f.connect(g); g.connect(this.sfxGain);
    osc.start();
    this.engineNode = osc;
    this.engineFilter = f;
    this.engineGain = g;
  };

  Audio.prototype.setEngine = function (level, throttle) {
    if (!this.ready || !this.engineGain) return;
    var t = this.now();
    var target = this.enabled.sfx ? U_clamp(level, 0, 1) * 0.055 : 0;
    this.engineGain.gain.setTargetAtTime(target, t, 0.12);
    this.engineNode.frequency.setTargetAtTime(38 + throttle * 26, t, 0.2);
    this.engineFilter.frequency.setTargetAtTime(180 + throttle * 320, t, 0.2);
  };

  Audio.prototype.setBeam = function (active, heat) {
    if (!this.ready) return;
    if (active && !this.beamNode) {
      var c = this.ctx;
      var osc = c.createOscillator();
      osc.type = 'sawtooth';
      osc.frequency.value = 128;
      var lfo = c.createOscillator();
      lfo.type = 'sine';
      lfo.frequency.value = 7.5;
      var lfoGain = c.createGain();
      lfoGain.gain.value = 10;
      lfo.connect(lfoGain); lfoGain.connect(osc.frequency);
      var f = c.createBiquadFilter();
      f.type = 'bandpass';
      f.frequency.value = 900;
      f.Q.value = 3.5;
      var g = c.createGain();
      g.gain.value = 0;
      osc.connect(f); f.connect(g); g.connect(this.sfxGain);
      osc.start(); lfo.start();
      this.beamNode = osc;
      this.beamLfo = lfo;
      this.beamGain = g;
      this.beamFilter = f;
    }
    if (this.beamGain) {
      var t = this.now();
      this.beamGain.gain.setTargetAtTime(active && this.enabled.sfx ? 0.06 : 0, t, 0.05);
      if (active) {
        this.beamNode.frequency.setTargetAtTime(96 + heat * 90, t, 0.08);
        this.beamFilter.frequency.setTargetAtTime(700 + heat * 1600, t, 0.08);
      }
    }
  };

  /* ---------- positional sfx dispatch ---------- */
  Audio.prototype.play = function (name, x, y, vol, player, camera) {
    if (!this.ready || !this.enabled.sfx) return;
    var pan = 0, atten = 1;
    if (x !== undefined && player) {
      var d = Math.hypot(x - player.x, y - player.y);
      atten = U_clamp(1 - d / 1700, 0.08, 1);
      var cam = camera || { x: player.x, y: player.y, zoom: 1 };
      pan = U_clamp((x - cam.x) / 900, -0.9, 0.9);
    }
    vol = (vol === undefined ? 1 : vol) * atten;
    if (vol < 0.02) return;
    var now = this.now();
    if (this.lastPlay[name] && now - this.lastPlay[name] < 0.035) return;
    this.lastPlay[name] = now;

    switch (name) {
      case 'pickup':
        this.tone({ type: 'triangle', f0: 720, f1: 1180, dur: 0.12, vol: 0.12 * vol, pan: pan });
        break;
      case 'unload':
        this.tone({ type: 'sine', f0: 420, f1: 620, dur: 0.18, vol: 0.16 * vol, pan: pan });
        this.tone({ type: 'sine', f0: 630, f1: 880, dur: 0.22, vol: 0.12 * vol, pan: pan, delay: 0.06 });
        break;
      case 'shoot':
        this.tone({ type: 'square', f0: 900, f1: 240, dur: 0.1, vol: 0.09 * vol, pan: pan, filter: 'lowpass', cutoff: 2200 });
        this.noise({ dur: 0.07, f0: 3400, vol: 0.05 * vol, pan: pan });
        break;
      case 'enemyShot':
        this.tone({ type: 'sawtooth', f0: 420, f1: 160, dur: 0.14, vol: 0.07 * vol, pan: pan, filter: 'lowpass', cutoff: 1200 });
        break;
      case 'enemyHit':
        this.noise({ dur: 0.09, f0: 2600, f1: 900, vol: 0.09 * vol, pan: pan });
        break;
      case 'rockBreak':
        this.noise({ dur: 0.5, f0: 1400, f1: 90, vol: 0.24 * vol, pan: pan, filter: 'lowpass' });
        this.tone({ type: 'sine', f0: 130, f1: 42, dur: 0.4, vol: 0.16 * vol, pan: pan });
        break;
      case 'explode':
        this.noise({ dur: 0.7, f0: 2200, f1: 60, vol: 0.3 * vol, pan: pan });
        this.tone({ type: 'triangle', f0: 90, f1: 30, dur: 0.5, vol: 0.2 * vol, pan: pan });
        break;
      case 'hullHit':
        this.noise({ dur: 0.24, f0: 900, f1: 140, vol: 0.24 * vol, pan: pan });
        this.tone({ type: 'square', f0: 150, f1: 60, dur: 0.2, vol: 0.12 * vol, pan: pan });
        break;
      case 'shieldHit':
        this.tone({ type: 'sine', f0: 1200, f1: 480, dur: 0.18, vol: 0.12 * vol, pan: pan });
        this.noise({ dur: 0.12, f0: 4200, vol: 0.07 * vol, pan: pan, filter: 'highpass' });
        break;
      case 'alarm':
        this.tone({ type: 'square', f0: 660, dur: 0.12, vol: 0.07 * vol });
        this.tone({ type: 'square', f0: 520, dur: 0.16, vol: 0.07 * vol, delay: 0.16 });
        break;
      case 'thunder':
        this.noise({ dur: 0.9, f0: 3000, f1: 70, vol: 0.26 * vol, pan: pan, filter: 'lowpass' });
        this.tone({ type: 'sawtooth', f0: 70, f1: 40, dur: 0.7, vol: 0.12 * vol, pan: pan });
        break;
      case 'teleport':
        this.tone({ type: 'sine', f0: 180, f1: 1400, dur: 0.3, vol: 0.12 * vol, pan: pan });
        break;
      case 'dash':
        this.noise({ dur: 0.3, f0: 700, f1: 2600, vol: 0.12 * vol, pan: pan, filter: 'bandpass' });
        break;
      case 'dock':
        this.tone({ type: 'sine', f0: 320, f1: 480, dur: 0.5, vol: 0.14 * vol });
        this.tone({ type: 'sine', f0: 640, f1: 960, dur: 0.6, vol: 0.1 * vol, delay: 0.12 });
        break;
      case 'uiClick':
        this.tone({ type: 'square', f0: 1250, f1: 900, dur: 0.035, vol: 0.05, filter: 'lowpass', cutoff: 4000 });
        break;
      case 'uiHover':
        this.tone({ type: 'sine', f0: 1600, dur: 0.025, vol: 0.018 });
        break;
      case 'buy':
        this.tone({ type: 'triangle', f0: 520, f1: 780, dur: 0.16, vol: 0.14 });
        this.tone({ type: 'triangle', f0: 780, f1: 1170, dur: 0.22, vol: 0.1, delay: 0.08 });
        break;
      case 'deny':
        this.tone({ type: 'square', f0: 220, f1: 140, dur: 0.16, vol: 0.09 });
        break;
      case 'sell':
        this.tone({ type: 'triangle', f0: 880, f1: 1320, dur: 0.2, vol: 0.13 });
        break;
      case 'levelUp':
        [0, 4, 7, 12].forEach(function (semi, i) {
          this.tone({ type: 'triangle', f0: 440 * Math.pow(2, semi / 12), dur: 0.3, vol: 0.13, delay: i * 0.09 });
        }, this);
        break;
      case 'achievement':
        [0, 7, 12, 16].forEach(function (semi, i) {
          this.tone({ type: 'sine', f0: 523 * Math.pow(2, semi / 12), dur: 0.4, vol: 0.12, delay: i * 0.11 });
        }, this);
        break;
      case 'warp':
        this.noise({ dur: 1.2, f0: 200, f1: 4000, vol: 0.2, filter: 'bandpass' });
        this.tone({ type: 'sine', f0: 120, f1: 900, dur: 1.1, vol: 0.14 });
        break;
      case 'playerDeath':
        this.noise({ dur: 1.4, f0: 2600, f1: 40, vol: 0.34 });
        this.tone({ type: 'sawtooth', f0: 180, f1: 28, dur: 1.2, vol: 0.2 });
        break;
      case 'beamLoop':
        break;
      default:
        break;
    }
  };

  /* ---------- generative music ---------- */
  Audio.prototype.setSector = function (sector) {
    this.sector = sector;
    this.step = 0;
    this.musicTimer = 0.4;
  };

  Audio.prototype.update = function (dt, dt2, playerThrottle, beamActive, heat) {
    if (!this.ready) return;
    this.setEngine(playerThrottle || 0, playerThrottle || 0);
    this.setBeam(!!beamActive, heat || 0);
    if (!this.enabled.music || !this.sector) return;
    this.musicTimer -= dt;
    if (this.musicTimer > 0) return;

    var bpm = this.sector.music.bpm;
    var beat = 60 / bpm;
    this.musicTimer = beat * (this.step % 4 === 0 ? 2 : 1);

    var scale = this.sector.music.scale;
    var root = this.sector.music.root;
    var step = this.step++;

    // sustained pad every 8 steps
    if (step % 8 === 0) {
      var deg = scale[(step / 8) % scale.length];
      this.pad(root * Math.pow(2, deg / 12), beat * 6, 0.055);
      this.pad(root * 2 * Math.pow(2, (deg + 7) / 12), beat * 6, 0.032);
      this.pad(root * 0.5 * Math.pow(2, deg / 12), beat * 6, 0.03);
    }
    // sparse arpeggio
    if (Math.random() < 0.62) {
      var octave = Math.random() < 0.35 ? 2 : 1;
      var semi = scale[Math.floor(Math.random() * scale.length)];
      var f = root * octave * Math.pow(2, semi / 12) * 2;
      this.bell(f, beat * 1.8, 0.03 + Math.random() * 0.02);
    }
  };

  Audio.prototype.pad = function (freq, dur, vol) {
    if (!this.ready) return;
    var c = this.ctx, t0 = this.now();
    var osc = c.createOscillator();
    osc.type = 'triangle';
    osc.frequency.value = freq;
    var osc2 = c.createOscillator();
    osc2.type = 'sine';
    osc2.frequency.value = freq * 1.005;
    var f = c.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.value = 900;
    var g = c.createGain();
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(vol, t0 + dur * 0.35);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    osc.connect(f); osc2.connect(f); f.connect(g); g.connect(this.musicGain);
    osc.start(t0); osc2.start(t0);
    osc.stop(t0 + dur + 0.1); osc2.stop(t0 + dur + 0.1);
  };

  Audio.prototype.bell = function (freq, dur, vol) {
    if (!this.ready) return;
    var c = this.ctx, t0 = this.now();
    var osc = c.createOscillator();
    osc.type = 'sine';
    osc.frequency.value = freq;
    var g = c.createGain();
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(vol, t0 + 0.01);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    var d = c.createDelay(0.4);
    d.delayTime.value = 0.23;
    var dg = c.createGain();
    dg.gain.value = 0.25;
    osc.connect(g); g.connect(this.musicGain);
    g.connect(d); d.connect(dg); dg.connect(this.musicGain);
    osc.start(t0);
    osc.stop(t0 + dur + 0.1);
  };

  function U_clamp(v, a, b) { return v < a ? a : v > b ? b : v; }

  SM.Audio = Audio;
})(typeof window !== 'undefined' ? window : globalThis);
