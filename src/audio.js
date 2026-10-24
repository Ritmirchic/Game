export class AudioEngine {
  constructor() {
    this.enabled = true;
    this.context = null;
    this.master = null;
    this.reverb = null;
    this.ambientStarted = false;
  }

  async start() {
    if (!this.context) {
      const AudioContext = window.AudioContext || window.webkitAudioContext;
      if (!AudioContext) return;
      this.context = new AudioContext();
      this.master = this.context.createGain();
      this.master.gain.value = 0;
      this.master.connect(this.context.destination);

      const convolver = this.context.createConvolver();
      const length = Math.floor(this.context.sampleRate * 1.8);
      const impulse = this.context.createBuffer(2, length, this.context.sampleRate);
      for (let channel = 0; channel < 2; channel += 1) {
        const data = impulse.getChannelData(channel);
        for (let index = 0; index < length; index += 1) {
          data[index] = (Math.random() * 2 - 1) * Math.pow(1 - index / length, 3.2);
        }
      }
      convolver.buffer = impulse;
      const reverbGain = this.context.createGain();
      reverbGain.gain.value = 0.22;
      convolver.connect(reverbGain);
      reverbGain.connect(this.master);
      this.reverb = convolver;
      this.startAmbient();
    }
    if (this.context.state === 'suspended') await this.context.resume();
    this.applyVolume();
  }

  applyVolume() {
    if (!this.context || !this.master) return;
    const now = this.context.currentTime;
    this.master.gain.cancelScheduledValues(now);
    this.master.gain.setTargetAtTime(this.enabled ? 0.55 : 0, now, 0.06);
  }

  async setEnabled(enabled) {
    this.enabled = enabled;
    if (enabled) await this.start();
    else this.applyVolume();
  }

  startAmbient() {
    if (this.ambientStarted || !this.context) return;
    this.ambientStarted = true;
    const filter = this.context.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 420;
    filter.Q.value = 0.7;
    filter.connect(this.master);

    [55, 82.41, 110].forEach((frequency, index) => {
      const oscillator = this.context.createOscillator();
      const gain = this.context.createGain();
      oscillator.type = index === 1 ? 'triangle' : 'sine';
      oscillator.frequency.value = frequency;
      oscillator.detune.value = index * 4 - 3;
      gain.gain.value = index === 0 ? 0.025 : 0.009;
      oscillator.connect(gain);
      gain.connect(filter);
      oscillator.start();
    });

    const lfo = this.context.createOscillator();
    const lfoGain = this.context.createGain();
    lfo.frequency.value = 0.07;
    lfoGain.gain.value = 100;
    lfo.connect(lfoGain);
    lfoGain.connect(filter.frequency);
    lfo.start();
  }

  tone(frequency, duration = 0.5, volume = 0.08, delay = 0, type = 'sine') {
    if (!this.context || !this.enabled) return;
    const now = this.context.currentTime + delay;
    const oscillator = this.context.createOscillator();
    const gain = this.context.createGain();
    oscillator.type = type;
    oscillator.frequency.setValueAtTime(frequency, now);
    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.exponentialRampToValueAtTime(volume, now + 0.025);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + duration);
    oscillator.connect(gain);
    gain.connect(this.master);
    if (this.reverb) gain.connect(this.reverb);
    oscillator.start(now);
    oscillator.stop(now + duration + 0.05);
    oscillator.onended = () => {
      oscillator.disconnect();
      gain.disconnect();
    };
  }

  ui() {
    this.tone(520, 0.16, 0.025);
  }

  plate(active) {
    if (active) {
      this.tone(329.63, 0.55, 0.07);
      this.tone(493.88, 0.7, 0.035, 0.06);
    } else {
      this.tone(196, 0.28, 0.025);
    }
  }

  echo() {
    [440, 349.23, 261.63, 523.25].forEach((note, index) => {
      this.tone(note, 0.85, 0.055, index * 0.085);
    });
  }

  locked() {
    this.tone(130.81, 0.24, 0.05, 0, 'triangle');
    this.tone(138.59, 0.32, 0.025, 0.02);
  }

  complete() {
    [261.63, 329.63, 392, 523.25, 659.25].forEach((note, index) => {
      this.tone(note, 1.7, 0.065, index * 0.11);
    });
  }

  awaken() {
    [220, 277.18, 329.63, 440, 554.37, 659.25].forEach((note, index) => {
      this.tone(note, 2.8, 0.05, index * 0.28);
    });
  }
}
