// ── Procedural audio: no sample files, everything synthesised on the fly ─────

export type SfxName =
  | 'hover'
  | 'click'
  | 'back'
  | 'cast'
  | 'hit'
  | 'shove'
  | 'vent'
  | 'forgeHit'
  | 'death'
  | 'heal'
  | 'shield'
  | 'spawn'
  | 'turn'
  | 'victory'
  | 'defeat'
  | 'reward'
  | 'bomb'
  | 'wall';

export class AudioEngine {
  private ctx: AudioContext | null = null;
  private master: GainNode | null = null;
  private sfxBus: GainNode | null = null;
  private musicBus: GainNode | null = null;
  private noise: AudioBuffer | null = null;
  private ambience: { osc: OscillatorNode[]; gain: GainNode; filter: BiquadFilterNode } | null = null;
  private musicTimer: number | null = null;
  private step = 0;
  sfxVolume = 0.7;
  musicVolume = 0.45;
  muted = false;
  ready = false;

  init(): void {
    if (this.ctx) {
      void this.ctx.resume();
      return;
    }
    const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!Ctor) return;
    const ctx = new Ctor();
    this.ctx = ctx;
    this.master = ctx.createGain();
    this.master.gain.value = this.muted ? 0 : 1;
    this.master.connect(ctx.destination);

    this.sfxBus = ctx.createGain();
    this.sfxBus.gain.value = this.sfxVolume;
    this.sfxBus.connect(this.master);

    this.musicBus = ctx.createGain();
    this.musicBus.gain.value = this.musicVolume;
    this.musicBus.connect(this.master);

    const len = Math.floor(ctx.sampleRate * 1.2);
    const buffer = ctx.createBuffer(1, len, ctx.sampleRate);
    const data = buffer.getChannelData(0);
    for (let i = 0; i < len; i++) data[i] = Math.random() * 2 - 1;
    this.noise = buffer;
    this.ready = true;
  }

  setVolumes(sfx: number, music: number, muted: boolean): void {
    this.sfxVolume = sfx;
    this.musicVolume = music;
    this.muted = muted;
    if (this.sfxBus) this.sfxBus.gain.value = muted ? 0 : sfx;
    if (this.musicBus) this.musicBus.gain.value = muted ? 0 : music * 0.6;
    if (this.master) this.master.gain.value = muted ? 0 : 1;
  }

  private now(): number {
    return this.ctx ? this.ctx.currentTime : 0;
  }

  private tone(freq: number, dur: number, type: OscillatorType, gain = 0.15, slideTo?: number, delay = 0, bus?: GainNode): void {
    const ctx = this.ctx;
    if (!ctx) return;
    const t = this.now() + delay;
    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, t);
    if (slideTo !== undefined) osc.frequency.exponentialRampToValueAtTime(Math.max(20, slideTo), t + dur);
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(gain, t + Math.min(0.03, dur * 0.3));
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    osc.connect(g);
    g.connect(bus ?? this.sfxBus ?? this.master!);
    osc.start(t);
    osc.stop(t + dur + 0.05);
  }

  private noiseBurst(dur: number, gain = 0.2, filterFreq = 1200, type: BiquadFilterType = 'bandpass', slideTo?: number, delay = 0): void {
    const ctx = this.ctx;
    if (!ctx || !this.noise) return;
    const t = this.now() + delay;
    const src = ctx.createBufferSource();
    src.buffer = this.noise;
    const filter = ctx.createBiquadFilter();
    filter.type = type;
    filter.frequency.setValueAtTime(filterFreq, t);
    if (slideTo !== undefined) filter.frequency.exponentialRampToValueAtTime(Math.max(60, slideTo), t + dur);
    filter.Q.value = 0.9;
    const g = ctx.createGain();
    g.gain.setValueAtTime(gain, t);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    src.connect(filter);
    filter.connect(g);
    g.connect(this.sfxBus ?? this.master!);
    src.start(t);
    src.stop(t + dur + 0.05);
  }

  play(name: SfxName): void {
    if (!this.ctx || !this.ready || this.muted) return;
    switch (name) {
      case 'hover':
        this.tone(880, 0.05, 'sine', 0.035);
        break;
      case 'click':
        this.tone(520, 0.07, 'triangle', 0.12, 700);
        this.noiseBurst(0.05, 0.05, 3000, 'highpass');
        break;
      case 'back':
        this.tone(420, 0.09, 'triangle', 0.09, 260);
        break;
      case 'cast':
        this.noiseBurst(0.28, 0.16, 500, 'bandpass', 2600);
        this.tone(180, 0.25, 'sawtooth', 0.07, 90);
        break;
      case 'hit':
        this.noiseBurst(0.14, 0.22, 900, 'lowpass', 180);
        this.tone(140, 0.12, 'square', 0.06, 80);
        break;
      case 'shove':
        this.noiseBurst(0.22, 0.18, 400, 'bandpass', 1400);
        this.tone(90, 0.2, 'sine', 0.09, 50);
        break;
      case 'vent':
        this.noiseBurst(0.4, 0.2, 2400, 'highpass', 700);
        break;
      case 'forgeHit':
        this.tone(70, 0.5, 'sine', 0.22, 40);
        this.noiseBurst(0.3, 0.2, 300, 'lowpass', 90);
        break;
      case 'death':
        this.tone(300, 0.3, 'triangle', 0.09, 70);
        this.noiseBurst(0.3, 0.12, 1800, 'bandpass', 300);
        break;
      case 'heal':
        [523, 659, 784].forEach((f, i) => this.tone(f, 0.25, 'sine', 0.07, undefined, i * 0.06));
        break;
      case 'shield':
        this.tone(320, 0.2, 'triangle', 0.08, 520);
        break;
      case 'spawn':
        this.tone(110, 0.35, 'sawtooth', 0.06, 220);
        this.noiseBurst(0.3, 0.08, 700, 'bandpass', 1500);
        break;
      case 'turn':
        this.tone(392, 0.35, 'sine', 0.06);
        this.tone(523, 0.4, 'sine', 0.05, undefined, 0.1);
        break;
      case 'bomb':
        this.noiseBurst(0.5, 0.26, 600, 'lowpass', 60);
        this.tone(60, 0.5, 'sine', 0.2, 30);
        break;
      case 'wall':
        this.noiseBurst(0.18, 0.16, 700, 'lowpass', 240);
        this.tone(120, 0.16, 'square', 0.06);
        break;
      case 'reward':
        [659, 784, 988, 1319].forEach((f, i) => this.tone(f, 0.35, 'triangle', 0.06, undefined, i * 0.07));
        break;
      case 'victory':
        [523, 659, 784, 1047, 1319].forEach((f, i) => this.tone(f, 0.55, 'sine', 0.09, undefined, i * 0.12));
        break;
      case 'defeat':
        [392, 330, 262, 196].forEach((f, i) => this.tone(f, 0.7, 'triangle', 0.09, f * 0.98, i * 0.18));
        break;
    }
  }

  startAmbience(): void {
    const ctx = this.ctx;
    if (!ctx || this.ambience) return;
    const gain = ctx.createGain();
    gain.gain.value = 0.0001;
    gain.gain.exponentialRampToValueAtTime(0.12, this.now() + 2.5);
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 620;
    filter.Q.value = 3;
    const oscs: OscillatorNode[] = [];
    for (const [freq, detune] of [
      [55, 0],
      [82.5, 6],
      [110, -8],
      [164.8, 4],
    ] as [number, number][]) {
      const o = ctx.createOscillator();
      o.type = 'sawtooth';
      o.frequency.value = freq;
      o.detune.value = detune;
      o.connect(filter);
      o.start();
      oscs.push(o);
    }
    const lfo = ctx.createOscillator();
    const lfoGain = ctx.createGain();
    lfo.frequency.value = 0.08;
    lfoGain.gain.value = 220;
    lfo.connect(lfoGain);
    lfoGain.connect(filter.frequency);
    lfo.start();
    oscs.push(lfo);
    filter.connect(gain);
    gain.connect(this.musicBus ?? this.master!);
    this.ambience = { osc: oscs, gain, filter };
    this.scheduleMusic();
  }

  private scheduleMusic(): void {
    if (this.musicTimer !== null) return;
    const scale = [261.6, 293.7, 329.6, 392, 440, 523.3, 587.3];
    const tick = () => {
      if (!this.ctx || this.muted) return;
      this.step++;
      if (this.step % 4 === 0) {
        const f = scale[(this.step * 3) % scale.length] * (this.step % 16 < 8 ? 1 : 0.5);
        this.tone(f, 2.4, 'sine', 0.05, undefined, 0.02, this.musicBus ?? undefined);
        this.tone(f * 1.5, 1.8, 'triangle', 0.022, undefined, 0.15, this.musicBus ?? undefined);
      }
      this.musicTimer = window.setTimeout(tick, 2400);
    };
    this.musicTimer = window.setTimeout(tick, 1600);
  }

  setIntensity(v: number): void {
    if (this.ambience) this.ambience.filter.frequency.value = 420 + v * 900;
  }

  stopAmbience(): void {
    if (this.musicTimer !== null) {
      clearTimeout(this.musicTimer);
      this.musicTimer = null;
    }
    if (!this.ambience || !this.ctx) return;
    const { osc, gain } = this.ambience;
    gain.gain.exponentialRampToValueAtTime(0.0001, this.now() + 0.8);
    for (const o of osc) {
      try {
        o.stop(this.now() + 1);
      } catch {
        /* already stopped */
      }
    }
    this.ambience = null;
    void this.ctx;
  }
}

export const audio = new AudioEngine();
