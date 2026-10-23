export type SoundName =
  | 'click'
  | 'wood'
  | 'scrap'
  | 'crystal'
  | 'swing'
  | 'slide'
  | 'hit'
  | 'hurt'
  | 'craft'
  | 'core'
  | 'night'
  | 'day'
  | 'turret'
  | 'win'
  | 'lose';

/** Small procedural sound palette: no downloads and no external audio assets. */
export class Soundscape {
  private context: AudioContext | null = null;
  private master: GainNode | null = null;
  private wind: AudioBufferSourceNode | null = null;
  private windFilter: BiquadFilterNode | null = null;
  private windGain: GainNode | null = null;
  enabled = true;

  unlock(): void {
    if (!this.context) {
      this.context = new AudioContext();
      this.master = this.context.createGain();
      this.master.gain.value = 0.22;
      this.master.connect(this.context.destination);
      this.startWind();
    }
    if (this.context.state === 'suspended') void this.context.resume();
  }

  toggle(): boolean {
    this.enabled = !this.enabled;
    if (this.master && this.context) {
      this.master.gain.setTargetAtTime(this.enabled ? 0.22 : 0, this.context.currentTime, 0.04);
    }
    return this.enabled;
  }

  setNight(night: boolean): void {
    if (!this.context || !this.windFilter || !this.windGain) return;
    const now = this.context.currentTime;
    this.windFilter.frequency.setTargetAtTime(night ? 520 : 900, now, 1.5);
    this.windGain.gain.setTargetAtTime(night ? 0.065 : 0.025, now, 1.5);
  }

  play(name: SoundName): void {
    if (!this.enabled || !this.context || !this.master) return;
    const now = this.context.currentTime;
    switch (name) {
      case 'click': this.tone(420, 0.045, 0.035, 'sine', now, 540); break;
      case 'wood': this.noise(0.09, 0.11, 420, now); this.tone(110, 0.1, 0.05, 'triangle', now, 75); break;
      case 'scrap': this.noise(0.08, 0.08, 1800, now); this.tone(260, 0.12, 0.04, 'square', now, 170); break;
      case 'crystal': this.tone(660, 0.2, 0.06, 'sine', now, 990); this.tone(990, 0.25, 0.035, 'sine', now + 0.04, 1320); break;
      case 'swing': this.noise(0.12, 0.075, 950, now); break;
      case 'slide':
        this.noise(0.32, 0.085, 720, now)
        this.noise(0.16, 0.035, 1800, now + 0.035)
        this.tone(95, 0.16, 0.025, 'triangle', now, 58)
        break;
      case 'hit': this.noise(0.09, 0.14, 600, now); this.tone(90, 0.12, 0.07, 'sawtooth', now, 55); break;
      case 'hurt': this.tone(130, 0.2, 0.1, 'sawtooth', now, 65); break;
      case 'craft': this.tone(280, 0.09, 0.06, 'square', now, 420); this.tone(560, 0.15, 0.05, 'triangle', now + 0.08, 720); break;
      case 'core':
        [220, 330, 440, 660].forEach((frequency, index) => this.tone(frequency, 0.6, 0.055, 'sine', now + index * 0.09, frequency * 1.01));
        break;
      case 'night': this.tone(110, 1.2, 0.08, 'sawtooth', now, 55); this.tone(164, 1.4, 0.04, 'sine', now, 82); break;
      case 'day': this.tone(330, 0.5, 0.045, 'sine', now, 440); this.tone(495, 0.6, 0.04, 'sine', now + 0.12, 660); break;
      case 'turret': this.tone(190, 0.055, 0.035, 'square', now, 90); break;
      case 'win':
        [262, 330, 392, 523].forEach((frequency, index) => this.tone(frequency, 1.1, 0.065, 'triangle', now + index * 0.16, frequency));
        break;
      case 'lose': this.tone(180, 1.3, 0.09, 'sawtooth', now, 48); break;
    }
  }

  private tone(frequency: number, duration: number, volume: number, type: OscillatorType, start: number, endFrequency: number): void {
    if (!this.context || !this.master) return;
    const oscillator = this.context.createOscillator();
    const gain = this.context.createGain();
    oscillator.type = type;
    oscillator.frequency.setValueAtTime(frequency, start);
    oscillator.frequency.exponentialRampToValueAtTime(Math.max(20, endFrequency), start + duration);
    gain.gain.setValueAtTime(0.0001, start);
    gain.gain.exponentialRampToValueAtTime(volume, start + Math.min(0.02, duration * 0.2));
    gain.gain.exponentialRampToValueAtTime(0.0001, start + duration);
    oscillator.connect(gain);
    gain.connect(this.master);
    oscillator.start(start);
    oscillator.stop(start + duration + 0.03);
  }

  private noise(duration: number, volume: number, cutoff: number, start: number): void {
    if (!this.context || !this.master) return;
    const length = Math.ceil(this.context.sampleRate * duration);
    const buffer = this.context.createBuffer(1, length, this.context.sampleRate);
    const data = buffer.getChannelData(0);
    for (let i = 0; i < length; i += 1) data[i] = Math.random() * 2 - 1;
    const source = this.context.createBufferSource();
    const filter = this.context.createBiquadFilter();
    const gain = this.context.createGain();
    filter.type = 'lowpass';
    filter.frequency.value = cutoff;
    gain.gain.setValueAtTime(volume, start);
    gain.gain.exponentialRampToValueAtTime(0.0001, start + duration);
    source.buffer = buffer;
    source.connect(filter);
    filter.connect(gain);
    gain.connect(this.master);
    source.start(start);
  }

  private startWind(): void {
    if (!this.context || !this.master) return;
    const length = this.context.sampleRate * 3;
    const buffer = this.context.createBuffer(1, length, this.context.sampleRate);
    const data = buffer.getChannelData(0);
    let last = 0;
    for (let i = 0; i < length; i += 1) {
      last = last * 0.985 + (Math.random() * 2 - 1) * 0.015;
      data[i] = last;
    }
    this.wind = this.context.createBufferSource();
    this.wind.buffer = buffer;
    this.wind.loop = true;
    this.windFilter = this.context.createBiquadFilter();
    this.windFilter.type = 'lowpass';
    this.windFilter.frequency.value = 900;
    this.windGain = this.context.createGain();
    this.windGain.gain.value = 0.025;
    this.wind.connect(this.windFilter);
    this.windFilter.connect(this.windGain);
    this.windGain.connect(this.master);
    this.wind.start();
  }
}
