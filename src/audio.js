/* MEMORIA audio — tiny generative WebAudio engine (no samples, no libraries) */
(function (root) {
  'use strict';

  function Audio() {
    this.ctx = null;
    this.muted = false;
    this.ready = false;
  }

  Audio.prototype.init = function () {
    if (this.ready) { if (this.ctx.state === 'suspended') this.ctx.resume(); return; }
    var Ctx = root.AudioContext || root.webkitAudioContext;
    if (!Ctx) return;
    var ctx = this.ctx = new Ctx();
    this.master = ctx.createGain();
    this.master.gain.value = 0.0;
    this.master.connect(ctx.destination);

    this.comp = ctx.createDynamicsCompressor();
    this.comp.threshold.value = -18;
    this.comp.knee.value = 22;
    this.comp.ratio.value = 3;
    this.comp.connect(this.master);

    // --- shared reverb-ish feedback delay
    this.delay = ctx.createDelay(1.0);
    this.delay.delayTime.value = 0.42;
    this.fb = ctx.createGain(); this.fb.gain.value = 0.42;
    this.delayWet = ctx.createGain(); this.delayWet.gain.value = 0.3;
    var dlp = ctx.createBiquadFilter(); dlp.type = 'lowpass'; dlp.frequency.value = 2400;
    this.delay.connect(dlp); dlp.connect(this.fb); this.fb.connect(this.delay);
    dlp.connect(this.delayWet); this.delayWet.connect(this.comp);

    this.padGain = ctx.createGain(); this.padGain.gain.value = 0.16;
    this.padFilter = ctx.createBiquadFilter();
    this.padFilter.type = 'lowpass'; this.padFilter.frequency.value = 620; this.padFilter.Q.value = 1.1;
    this.padGain.connect(this.padFilter); this.padFilter.connect(this.comp);
    this.padFilter.connect(this.delay);

    // drone: five detuned voices over a low root
    var base = 55; // A1
    var ratios = [1, 1.5, 2, 2.9966, 4.5];
    var types = ['sine', 'sine', 'triangle', 'sine', 'triangle'];
    var gains = [0.5, 0.32, 0.2, 0.13, 0.07];
    this.voices = [];
    for (var i = 0; i < ratios.length; i++) {
      var o = ctx.createOscillator();
      o.type = types[i];
      o.frequency.value = base * ratios[i] * (1 + (i % 2 ? 0.0018 : -0.0012));
      var g = ctx.createGain(); g.gain.value = gains[i];
      var pan = ctx.createStereoPanner ? ctx.createStereoPanner() : null;
      if (pan) { pan.pan.value = (i - 2) * 0.28; o.connect(g); g.connect(pan); pan.connect(this.padGain); }
      else { o.connect(g); g.connect(this.padGain); }
      o.start();
      this.voices.push(o);
    }
    // slow filter breathing
    var lfo = ctx.createOscillator(); lfo.type = 'sine'; lfo.frequency.value = 0.055;
    var lfoG = ctx.createGain(); lfoG.gain.value = 240;
    lfo.connect(lfoG); lfoG.connect(this.padFilter.frequency); lfo.start();

    // --- wind / air noise
    var len = ctx.sampleRate * 3;
    var buf = ctx.createBuffer(1, len, ctx.sampleRate);
    var d = buf.getChannelData(0);
    var last = 0;
    for (var n = 0; n < len; n++) {
      var white = Math.random() * 2 - 1;
      last = (last + 0.02 * white) / 1.02;
      d[n] = last * 3.2;
    }
    this.noiseBuf = buf;
    var src = ctx.createBufferSource();
    src.buffer = buf; src.loop = true;
    var nf = ctx.createBiquadFilter(); nf.type = 'bandpass'; nf.frequency.value = 420; nf.Q.value = 0.7;
    this.windGain = ctx.createGain(); this.windGain.gain.value = 0.09;
    src.connect(nf); nf.connect(this.windGain); this.windGain.connect(this.comp);
    src.start();

    // gentle master fade-in
    var t = ctx.currentTime;
    this.master.gain.setValueAtTime(0.0001, t);
    this.master.gain.exponentialRampToValueAtTime(this.muted ? 0.0001 : 0.85, t + 2.5);
    this.ready = true;
  };

  Audio.prototype.setMuted = function (m) {
    this.muted = m;
    if (!this.ready) return;
    var t = this.ctx.currentTime;
    this.master.gain.cancelScheduledValues(t);
    this.master.gain.setTargetAtTime(m ? 0.0001 : 0.85, t, 0.15);
  };

  Audio.prototype.toggleMute = function () {
    this.setMuted(!this.muted);
    return this.muted;
  };

  /* bell: revealing something */
  Audio.prototype.bell = function (freq, vol, timbre) {
    if (!this.ready) return;
    var ctx = this.ctx, t = ctx.currentTime;
    vol = vol == null ? 0.22 : vol;
    var g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(vol, t + 0.012);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 2.1);
    var lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 5200;
    g.connect(lp); lp.connect(this.comp); lp.connect(this.delay);
    var partials = timbre === 'warm' ? [1, 2.0, 3.01, 4.2] : [1, 2.76, 5.4];
    for (var i = 0; i < partials.length; i++) {
      var o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.value = freq * partials[i] * (1 + (Math.random() - 0.5) * 0.002);
      var pg = ctx.createGain(); pg.gain.value = 1 / (1 + i * 1.6);
      o.connect(pg); pg.connect(g);
      o.start(t); o.stop(t + 2.2);
    }
  };

  Audio.prototype.whoosh = function (power) {
    if (!this.ready) return;
    var ctx = this.ctx, t = ctx.currentTime;
    var src = ctx.createBufferSource(); src.buffer = this.noiseBuf; src.loop = true;
    var bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.Q.value = 1.4;
    bp.frequency.setValueAtTime(180, t);
    bp.frequency.exponentialRampToValueAtTime(2600, t + 0.35);
    bp.frequency.exponentialRampToValueAtTime(300, t + 1.5);
    var g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(0.30 * (power || 1), t + 0.10);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 1.7);
    src.connect(bp); bp.connect(g); g.connect(this.comp); g.connect(this.delay);
    src.start(t); src.stop(t + 1.8);
    var sub = ctx.createOscillator(); sub.type = 'sine';
    sub.frequency.setValueAtTime(140, t);
    sub.frequency.exponentialRampToValueAtTime(46, t + 0.7);
    var sg = ctx.createGain();
    sg.gain.setValueAtTime(0.28, t);
    sg.gain.exponentialRampToValueAtTime(0.0001, t + 0.9);
    sub.connect(sg); sg.connect(this.comp);
    sub.start(t); sub.stop(t + 1.0);
  };

  Audio.prototype.step = function (grass) {
    if (!this.ready) return;
    var ctx = this.ctx, t = ctx.currentTime;
    var src = ctx.createBufferSource(); src.buffer = this.noiseBuf;
    src.playbackRate.value = 1.6 + Math.random() * 0.6;
    var bp = ctx.createBiquadFilter(); bp.type = 'bandpass';
    bp.frequency.value = 900 + Math.random() * 900; bp.Q.value = 1.8;
    var g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(0.05, t + 0.008);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.10);
    src.connect(bp); bp.connect(g); g.connect(this.comp);
    src.start(t, Math.random() * 2, 0.12);
  };

  Audio.prototype.chord = function (rootHz, up, vol) {
    if (!this.ready) return;
    var ratio = [1, 1.25, 1.5, 2, 2.5];
    for (var i = 0; i < ratio.length; i++) {
      var f = rootHz * ratio[i] * (up ? 1 : 1);
      var self = this;
      (function (self, f, i) {
        setTimeout(function () { self.bell(f, (vol || 0.16) * (1 - i * 0.12), 'warm'); }, i * 130);
      })(self, f, i);
    }
  };

  Audio.prototype.heart = function () {
    if (!this.ready) return;
    var ctx = this.ctx, t = ctx.currentTime;
    var o = ctx.createOscillator(); o.type = 'sine';
    o.frequency.setValueAtTime(62, t); o.frequency.exponentialRampToValueAtTime(34, t + 0.22);
    var g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(0.34, t + 0.02);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.34);
    o.connect(g); g.connect(this.comp); o.start(t); o.stop(t + 0.4);
  };

  Audio.prototype.vortex = function () {
    if (!this.ready) return;
    var ctx = this.ctx, t = ctx.currentTime;
    for (var i = 0; i < 9; i++) {
      var o = ctx.createOscillator(); o.type = 'sine';
      var f = 160 * Math.pow(2, i / 4);
      o.frequency.setValueAtTime(f, t + i * 0.16);
      o.frequency.exponentialRampToValueAtTime(f * 2, t + 3.4 + i * 0.16);
      var g = ctx.createGain();
      g.gain.setValueAtTime(0.0001, t + i * 0.16);
      g.gain.exponentialRampToValueAtTime(0.09, t + 0.4 + i * 0.16);
      g.gain.exponentialRampToValueAtTime(0.0001, t + 3.6 + i * 0.16);
      o.connect(g); g.connect(this.comp); g.connect(this.delay);
      o.start(t + i * 0.16); o.stop(t + 4.4 + i * 0.16);
    }
  };

  Audio.prototype.setTension = function (k) {
    if (!this.ready) return;
    var t = this.ctx.currentTime;
    this.padFilter.frequency.setTargetAtTime(560 + 900 * k, t, 0.6);
    this.windGain.gain.setTargetAtTime(0.07 + 0.11 * k, t, 0.9);
  };

  root.MEM = root.MEM || {};
  root.MEM.Audio = Audio;
})(typeof window !== 'undefined' ? window : globalThis);
