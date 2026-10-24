/* ═══════════════════════════════════════════════════
   СИНГУЛЯРНОСТЬ — процедурный звук (WebAudio, 0 файлов)
   Дрон зависит от массы, слияние озвучено «чирпом»
   гравитационных волн (оммаж LIGO GW150914).
   ═══════════════════════════════════════════════════ */
(function(){
'use strict';

class GameAudio{
  constructor(){
    this.ctx = null;
    this.muted = false;
    this.started = false;
    this._last = {};           // rate limiters
    this.padTimer = null;
    this.padIndex = 0;
    this._thumpTimer = 0;
  }

  /* ── инициализация по первому жесту пользователя ── */
  init(){
    if(this.ctx) { if(this.ctx.state === 'suspended') this.ctx.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext;
    if(!AC){ return; }
    try{
      const ctx = this.ctx = new AC();

      this.master = ctx.createGain();
      this.master.gain.value = this.muted ? 0 : 0.85;

      this.comp = ctx.createDynamicsCompressor();
      this.comp.threshold.value = -18;
      this.comp.ratio.value = 8;
      this.comp.attack.value = 0.003;
      this.comp.release.value = 0.25;

      this.master.connect(this.comp);
      this.comp.connect(ctx.destination);

      // реверберация: процедурный импульсный отклик (шум с экспоненциальным затуханием)
      this.reverb = ctx.createConvolver();
      this.reverb.buffer = this._makeIR(2.6, 2.2);
      this.reverbGain = ctx.createGain();
      this.reverbGain.gain.value = 0.55;
      this.reverb.connect(this.reverbGain);
      this.reverbGain.connect(this.master);

      // общий буфер шума
      this.noiseBuf = this._makeNoise(2.0);
    }catch(e){
      this.ctx = null;
    }
  }

  _makeNoise(sec){
    const ctx = this.ctx;
    const buf = ctx.createBuffer(1, Math.floor(ctx.sampleRate*sec), ctx.sampleRate);
    const d = buf.getChannelData(0);
    for(let i=0;i<d.length;i++) d[i] = Math.random()*2-1;
    return buf;
  }

  _makeIR(sec, decay){
    const ctx = this.ctx;
    const len = Math.floor(ctx.sampleRate*sec);
    const buf = ctx.createBuffer(2, len, ctx.sampleRate);
    for(let ch=0;ch<2;ch++){
      const d = buf.getChannelData(ch);
      for(let i=0;i<len;i++){
        d[i] = (Math.random()*2-1) * Math.pow(1 - i/len, decay);
      }
    }
    return buf;
  }

  _ok(minGapMs, key){
    if(!this.ctx || this.muted) return false;
    const now = performance.now();
    key = key || 'default';
    if(this._last[key] && now - this._last[key] < minGapMs) return false;
    this._last[key] = now;
    return true;
  }

  _pan(x){
    if(!this.ctx) return null;
    try{
      const p = this.ctx.createStereoPanner();
      p.pan.value = Math.max(-1, Math.min(1, x || 0));
      p.connect(this.master);
      return p;
    }catch(e){ return this.master; }
  }

  toggleMute(){
    this.muted = !this.muted;
    if(this.master){
      this.master.gain.setTargetAtTime(this.muted ? 0 : 0.85, this.ctx.currentTime, 0.05);
    }
    return !this.muted;
  }

  /* ── эмбиент: дрон + генеративные пэды ── */
  startAmbient(){
    this.init();
    if(!this.ctx || this.started) return;
    this.started = true;
    const ctx = this.ctx, t = ctx.currentTime;

    // низкий дрон: саб + пила сквозь фильтр
    this.droneSub = ctx.createOscillator();
    this.droneSub.type = 'sine';
    this.droneSub.frequency.value = 22;
    this.droneSubGain = ctx.createGain();
    this.droneSubGain.gain.value = 0;
    this.droneSub.connect(this.droneSubGain);
    this.droneSubGain.connect(this.master);
    this.droneSub.start(t);

    this.droneSaw = ctx.createOscillator();
    this.droneSaw.type = 'sawtooth';
    this.droneSaw.frequency.value = 44;
    this.droneFilter = ctx.createBiquadFilter();
    this.droneFilter.type = 'lowpass';
    this.droneFilter.frequency.value = 160;
    this.droneFilter.Q.value = 2;
    this.droneSawGain = ctx.createGain();
    this.droneSawGain.gain.value = 0;
    this.droneSaw.connect(this.droneFilter);
    this.droneFilter.connect(this.droneSawGain);
    this.droneSawGain.connect(this.master);
    this.droneSaw.start(t);

    // медленное дыхание фильтра
    this.lfo = ctx.createOscillator();
    this.lfo.frequency.value = 0.07;
    this.lfoGain = ctx.createGain();
    this.lfoGain.gain.value = 60;
    this.lfo.connect(this.lfoGain);
    this.lfoGain.connect(this.droneFilter.frequency);
    this.lfo.start(t);

    // пэды — аккордовая прогрессия A minor, каждые 9 секунд
    this.pads = [
      [110.0, 130.8, 164.8],   // Am
      [ 87.3, 110.0, 130.8],   // F
      [130.8, 164.8, 196.0],   // C
      [ 98.0, 123.5, 146.8],   // G
    ];
    this.padTimer = setInterval(()=>this._playPad(), 9000);
    this._playPad();

    this.droneSubGain.gain.setTargetAtTime(0.05, t, 3);
    this.droneSawGain.gain.setTargetAtTime(0.022, t, 3);
  }

  _playPad(){
    if(!this.ctx || this.muted) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const chord = this.pads[this.padIndex % this.pads.length];
    this.padIndex++;

    const g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(0.030, t + 3.0);
    g.gain.setTargetAtTime(0.0001, t + 6.5, 1.6);

    const f = ctx.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.value = 620;

    const send = ctx.createGain();
    send.gain.value = 0.7;

    g.connect(f);
    f.connect(this.master);
    f.connect(send);
    send.connect(this.reverb);

    chord.forEach((freq, i)=>{
      for(let d=-1; d<=1; d+=2){
        const o = ctx.createOscillator();
        o.type = 'triangle';
        o.frequency.value = freq * 2;
        o.detune.value = d * (3 + i*2);
        o.connect(g);
        o.start(t);
        o.stop(t + 12);
      }
    });
    setTimeout(()=>{ try{ g.disconnect(); f.disconnect(); send.disconnect(); }catch(e){} }, 13000);
  }

  /* дрон реагирует на массу игрока */
  setMass(m){
    if(!this.ctx || !this.started) return;
    const t = this.ctx.currentTime;
    const base = 36 + 11 * Math.log2(Math.max(1, m/50));   // растёшь — голос ниже и плотнее
    const f = Math.max(28, Math.min(70, base));
    this.droneSaw.frequency.setTargetAtTime(f, t, 0.8);
    this.droneSub.frequency.setTargetAtTime(f/2, t, 0.8);
    this.droneSawGain.gain.setTargetAtTime(Math.min(0.05, 0.022 + 0.006*Math.log2(Math.max(1,m/50))), t, 1);
  }

  /* ── события ── */

  /* поглощение материи: пентатонические капли */
  blip(size, pan){
    if(!this._ok(45, 'blip')) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const scale = [261.6, 293.7, 329.6, 392.0, 440.0, 523.3, 587.3, 659.3];
    const idx = Math.floor(Math.random()*scale.length * Math.min(1, 0.35 + size*0.12));
    const o = ctx.createOscillator();
    o.type = 'triangle';
    o.frequency.value = scale[idx];
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.055 * Math.min(1, 0.4+size*0.15), t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.11);
    o.connect(g);
    g.connect(this._pan(pan) || this.master);
    o.start(t); o.stop(t + 0.13);
  }

  laser(pan){
    if(!this._ok(120, 'laser')) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const o = ctx.createOscillator();
    o.type = 'square';
    o.frequency.setValueAtTime(1250, t);
    o.frequency.exponentialRampToValueAtTime(280, t + 0.14);
    const f = ctx.createBiquadFilter();
    f.type = 'lowpass'; f.frequency.value = 2400;
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.022, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.16);
    o.connect(f); f.connect(g);
    g.connect(this._pan(pan) || this.master);
    o.start(t); o.stop(t + 0.18);
  }

  absorbLight(pan){
    if(!this._ok(90, 'absorb')) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const o = ctx.createOscillator();
    o.type = 'sine';
    o.frequency.setValueAtTime(2100, t);
    o.frequency.exponentialRampToValueAtTime(500, t + 0.06);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.014, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.07);
    o.connect(g); g.connect(this._pan(pan) || this.master);
    o.start(t); o.stop(t + 0.09);
  }

  eatShip(pan){
    if(!this.ctx || this.muted) return;
    const ctx = this.ctx, t = ctx.currentTime;
    // хруст: шум сквозь highpass + низкий толчок
    const n = ctx.createBufferSource();
    n.buffer = this.noiseBuf;
    const hp = ctx.createBiquadFilter();
    hp.type = 'highpass';
    hp.frequency.setValueAtTime(3000, t);
    hp.frequency.exponentialRampToValueAtTime(300, t + 0.22);
    const gn = ctx.createGain();
    gn.gain.setValueAtTime(0.10, t);
    gn.gain.exponentialRampToValueAtTime(0.0001, t + 0.28);
    n.connect(hp); hp.connect(gn); gn.connect(this._pan(pan) || this.master);
    n.start(t); n.stop(t + 0.3);

    const o = ctx.createOscillator();
    o.type = 'sine';
    o.frequency.setValueAtTime(110, t);
    o.frequency.exponentialRampToValueAtTime(38, t + 0.25);
    const g2 = ctx.createGain();
    g2.gain.setValueAtTime(0.22, t);
    g2.gain.exponentialRampToValueAtTime(0.0001, t + 0.3);
    o.connect(g2); g2.connect(this.master);
    o.start(t); o.stop(t + 0.32);
  }

  pulse(pan){
    if(!this.ctx || this.muted) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const n = ctx.createBufferSource();
    n.buffer = this.noiseBuf;
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass'; bp.Q.value = 1.2;
    bp.frequency.setValueAtTime(900, t);
    bp.frequency.exponentialRampToValueAtTime(70, t + 0.5);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.16, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.55);
    n.connect(bp); bp.connect(g); g.connect(this.master);
    n.start(t); n.stop(t + 0.6);

    const o = ctx.createOscillator();
    o.type = 'sine';
    o.frequency.setValueAtTime(95, t);
    o.frequency.exponentialRampToValueAtTime(32, t + 0.4);
    const g2 = ctx.createGain();
    g2.gain.setValueAtTime(0.3, t);
    g2.gain.exponentialRampToValueAtTime(0.0001, t + 0.45);
    o.connect(g2); g2.connect(this.master);
    o.start(t); o.stop(t + 0.5);
  }

  /* ═══ СЛИЯНИЕ: тот самый чирп гравитационных волн ═══ */
  chirp(big){
    if(!this.ctx || this.muted) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const vol = big ? 1 : 0.5;

    // чирп: частота растёт экспоненциально 48 → 900 Гц, амплитуда нарастает — точь-в-точь сигнал GW150914
    const o = ctx.createOscillator();
    o.type = 'sine';
    o.frequency.setValueAtTime(48, t);
    o.frequency.exponentialRampToValueAtTime(900, t + 0.85);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.005, t);
    g.gain.exponentialRampToValueAtTime(0.42*vol, t + 0.8);
    g.gain.setValueAtTime(0.42*vol, t + 0.86);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 1.15);
    const send = ctx.createGain();
    send.gain.value = 0.9;
    o.connect(g);
    g.connect(this.master);
    g.connect(send); send.connect(this.reverb);
    o.start(t); o.stop(t + 1.2);

    // удар после слияния: ringdown + бух
    const o2 = ctx.createOscillator();
    o2.type = 'sine';
    o2.frequency.setValueAtTime(240, t + 0.86);
    o2.frequency.exponentialRampToValueAtTime(26, t + 2.4);
    const g2 = ctx.createGain();
    g2.gain.setValueAtTime(0.0001, t + 0.86);
    g2.gain.linearRampToValueAtTime(0.5*vol, t + 0.92);
    g2.gain.exponentialRampToValueAtTime(0.0001, t + 2.6);
    const send2 = ctx.createGain();
    send2.gain.value = 0.8;
    o2.connect(g2);
    g2.connect(this.master);
    g2.connect(send2); send2.connect(this.reverb);
    o2.start(t + 0.86); o2.stop(t + 2.7);

    // шумовая волна
    const n = ctx.createBufferSource();
    n.buffer = this.noiseBuf;
    const lp = ctx.createBiquadFilter();
    lp.type = 'lowpass';
    lp.frequency.setValueAtTime(4000, t + 0.85);
    lp.frequency.exponentialRampToValueAtTime(60, t + 2.2);
    const gn = ctx.createGain();
    gn.gain.setValueAtTime(0.0001, t + 0.85);
    gn.gain.linearRampToValueAtTime(0.20*vol, t + 0.95);
    gn.gain.exponentialRampToValueAtTime(0.0001, t + 2.4);
    n.connect(lp); lp.connect(gn); gn.connect(this.master);
    n.start(t + 0.85); n.stop(t + 2.5);
  }

  /* сердцебиение, когда соперница тяжелее и близко */
  thump(intensity){
    if(!this.ctx || this.muted) return;
    const ctx = this.ctx;
    const beat = (dt)=>{
      const t = ctx.currentTime + dt;
      const o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.setValueAtTime(62, t);
      o.frequency.exponentialRampToValueAtTime(36, t + 0.14);
      const g = ctx.createGain();
      g.gain.setValueAtTime(0.0001, t);
      g.gain.linearRampToValueAtTime(0.30*intensity, t + 0.02);
      g.gain.exponentialRampToValueAtTime(0.0001, t + 0.2);
      o.connect(g); g.connect(this.master);
      o.start(t); o.stop(t + 0.22);
    };
    beat(0); beat(0.22);
  }

  waveBell(){
    if(!this.ctx || this.muted) return;
    const ctx = this.ctx, t = ctx.currentTime;
    [523.3, 659.3, 784.0].forEach((f, i)=>{
      const o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.value = f;
      const g = ctx.createGain();
      const tt = t + i*0.09;
      g.gain.setValueAtTime(0.0001, tt);
      g.gain.linearRampToValueAtTime(0.07, tt + 0.02);
      g.gain.exponentialRampToValueAtTime(0.0001, tt + 0.7);
      const send = ctx.createGain();
      send.gain.value = 0.6;
      o.connect(g); g.connect(this.master);
      g.connect(send); send.connect(this.reverb);
      o.start(tt); o.stop(tt + 0.75);
    });
  }

  ui(){
    if(!this._ok(60, 'ui')) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const o = ctx.createOscillator();
    o.type = 'sine';
    o.frequency.setValueAtTime(720, t);
    o.frequency.exponentialRampToValueAtTime(540, t + 0.06);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.06, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.08);
    o.connect(g); g.connect(this.master);
    o.start(t); o.stop(t + 0.1);
  }

  lose(){
    if(!this.ctx || this.muted) return;
    const ctx = this.ctx, t = ctx.currentTime;
    const o = ctx.createOscillator();
    o.type = 'sawtooth';
    o.frequency.setValueAtTime(190, t);
    o.frequency.exponentialRampToValueAtTime(38, t + 1.6);
    const f = ctx.createBiquadFilter();
    f.type = 'lowpass';
    f.frequency.setValueAtTime(900, t);
    f.frequency.exponentialRampToValueAtTime(120, t + 1.6);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(0.28, t + 0.1);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 1.8);
    const send = ctx.createGain();
    send.gain.value = 0.8;
    o.connect(f); f.connect(g);
    g.connect(this.master);
    g.connect(send); send.connect(this.reverb);
    o.start(t); o.stop(t + 1.9);
  }
}

window.AUDIO = new GameAudio();
})();
