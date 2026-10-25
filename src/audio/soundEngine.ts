import { GameSettings, ModuleId } from '../core/types';

export type MusicMood = 'menu' | 'combat' | 'boss' | 'silent';

const LYDIAN_SCALE = [523.25, 587.33, 659.25, 739.99, 783.99, 880.0, 987.77, 1046.5, 1174.66, 1318.51, 1567.98];

export class SoundEngine {
  private ctx: AudioContext | null = null;
  private masterGain: GainNode | null = null;
  private sfxGain: GainNode | null = null;
  private musicGain: GainNode | null = null;
  private settings: GameSettings;
  private mood: MusicMood = 'silent';
  private musicTimer: ReturnType<typeof setInterval> | null = null;
  private stepIdx = 0;
  private lastSfxTime: Record<string, number> = {};

  constructor(initialSettings: GameSettings) {
    this.settings = { ...initialSettings };
  }

  public unlock(): void {
    if (typeof window === 'undefined') return;
    const AudioCtx = window.AudioContext || (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!AudioCtx) return;

    if (!this.ctx) {
      try {
        this.ctx = new AudioCtx();
        this.masterGain = this.ctx.createGain();
        this.sfxGain = this.ctx.createGain();
        this.musicGain = this.ctx.createGain();

        this.sfxGain.connect(this.masterGain);
        this.musicGain.connect(this.masterGain);
        this.masterGain.connect(this.ctx.destination);
        this.applyVolumes();
      } catch {
        this.ctx = null;
      }
    }

    if (this.ctx && this.ctx.state === 'suspended') {
      this.ctx.resume().catch(() => {});
    }
  }

  public updateSettings(settings: GameSettings): void {
    this.settings = { ...settings };
    this.applyVolumes();
  }

  private applyVolumes(): void {
    if (!this.ctx || !this.masterGain || !this.sfxGain || !this.musicGain) return;
    const now = this.ctx.currentTime;
    const m = this.settings.muted ? 0 : this.settings.masterVolume;
    this.masterGain.gain.setTargetAtTime(m, now, 0.02);
    this.sfxGain.gain.setTargetAtTime(this.settings.sfxVolume, now, 0.02);
    this.musicGain.gain.setTargetAtTime(this.settings.musicVolume * 0.35, now, 0.05);
  }

  private throttle(key: string, minGapMs: number): boolean {
    const now = Date.now();
    const prev = this.lastSfxTime[key] ?? 0;
    if (now - prev < minGapMs) return false;
    this.lastSfxTime[key] = now;
    return true;
  }

  private tone(
    freq: number,
    endFreq: number,
    duration: number,
    type: OscillatorType,
    volume = 0.22,
    delay = 0,
    toMusic = false
  ): void {
    if (!this.ctx || !this.sfxGain || !this.musicGain || this.settings.muted) return;
    try {
      const t0 = this.ctx.currentTime + delay;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = type;
      osc.frequency.setValueAtTime(Math.max(30, freq), t0);
      if (endFreq !== freq) {
        osc.frequency.exponentialRampToValueAtTime(Math.max(30, endFreq), t0 + duration);
      }
      g.gain.setValueAtTime(0.0001, t0);
      g.gain.linearRampToValueAtTime(volume, t0 + Math.min(0.015, duration * 0.25));
      g.gain.exponentialRampToValueAtTime(0.0001, t0 + duration);

      osc.connect(g);
      g.connect(toMusic ? this.musicGain : this.sfxGain);
      osc.start(t0);
      osc.stop(t0 + duration + 0.02);
    } catch {
      // Ignore WebAudio scheduling edge cases
    }
  }

  public playHover(): void {
    if (!this.throttle('hover', 45)) return;
    this.tone(880, 1080, 0.045, 'sine', 0.08);
  }

  public playClick(): void {
    this.unlock();
    if (!this.throttle('click', 35)) return;
    this.tone(520, 780, 0.07, 'triangle', 0.18);
  }

  public playPlace(): void {
    this.tone(420, 660, 0.09, 'triangle', 0.2);
    this.tone(840, 1120, 0.08, 'sine', 0.12, 0.03);
  }

  public playRotate(): void {
    this.tone(620, 920, 0.065, 'sine', 0.16);
  }

  public playMerge(tier: 2 | 3): void {
    const notes = tier === 2 ? [523.25, 659.25, 783.99, 1046.5] : [587.33, 739.99, 880, 1174.66, 1479.98];
    notes.forEach((n, idx) => {
      this.tone(n, n * 1.02, 0.18, 'triangle', 0.22, idx * 0.055);
    });
  }

  public playPulseCombo(comboStep: number): void {
    if (!this.throttle('pulse', 45)) return;
    const idx = Math.min(LYDIAN_SCALE.length - 1, Math.max(0, comboStep));
    const f = LYDIAN_SCALE[idx];
    this.tone(f, f * 1.04, 0.09, 'sine', 0.12);
  }

  public playTurretFire(defId: ModuleId): void {
    if (!this.throttle(`fire_${defId}`, 65)) return;
    switch (defId) {
      case 'turret_blaze':
        this.tone(240, 95, 0.14, 'sawtooth', 0.15);
        break;
      case 'turret_arc':
        this.tone(1240, 460, 0.08, 'square', 0.1);
        break;
      case 'turret_frost':
        this.tone(980, 1480, 0.09, 'sine', 0.13);
        break;
      case 'turret_venom':
        this.tone(480, 310, 0.06, 'triangle', 0.11);
        break;
      case 'turret_beam':
        this.tone(660, 880, 0.15, 'sawtooth', 0.12);
        break;
      case 'turret_mortar':
        this.tone(160, 58, 0.2, 'triangle', 0.2);
        break;
      default:
        this.tone(600, 800, 0.06, 'sine', 0.1);
    }
  }

  public playCoin(): void {
    if (!this.throttle('coin', 50)) return;
    this.tone(1174.66, 1567.98, 0.08, 'sine', 0.11);
  }

  public playCoreHit(): void {
    if (!this.throttle('corehit', 120)) return;
    this.tone(150, 55, 0.26, 'sawtooth', 0.28);
  }

  public playAbility(): void {
    this.tone(330, 880, 0.25, 'sawtooth', 0.2);
    this.tone(660, 1320, 0.28, 'triangle', 0.18, 0.05);
  }

  public playAchievement(): void {
    [523.25, 659.25, 783.99, 1046.5, 1318.51].forEach((n, i) => {
      this.tone(n, n, 0.22, 'triangle', 0.2, i * 0.07);
    });
  }

  public playWaveClear(): void {
    [440, 554.37, 659.25, 880].forEach((n, i) => {
      this.tone(n, n * 1.01, 0.22, 'triangle', 0.18, i * 0.065);
    });
  }

  public playVictory(): void {
    [523.25, 659.25, 783.99, 1046.5, 987.77, 1046.5, 1318.51].forEach((n, i) => {
      this.tone(n, n, 0.3, 'triangle', 0.24, i * 0.09);
    });
  }

  public playDefeat(): void {
    [392.0, 349.23, 311.13, 261.63].forEach((n, i) => {
      this.tone(n, n * 0.96, 0.35, 'sawtooth', 0.18, i * 0.14);
    });
  }

  public setMusicMood(mood: MusicMood): void {
    if (this.mood === mood) return;
    this.mood = mood;
    if (this.musicTimer) {
      clearInterval(this.musicTimer);
      this.musicTimer = null;
    }
    if (mood === 'silent') return;

    const intervalMs = mood === 'menu' ? 480 : mood === 'combat' ? 280 : 210;
    const menuNotes = [261.63, 329.63, 392.0, 493.88, 587.33, 493.88, 392.0, 329.63];
    const combatNotes = [146.83, 220.0, 293.66, 329.63, 146.83, 261.63, 293.66, 392.0];
    const bossNotes = [130.81, 196.0, 233.08, 261.63, 123.47, 185.0, 246.94, 311.13];

    this.musicTimer = setInterval(() => {
      if (!this.ctx || this.settings.muted || this.ctx.state !== 'running') return;
      const seq = this.mood === 'menu' ? menuNotes : this.mood === 'combat' ? combatNotes : bossNotes;
      const note = seq[this.stepIdx % seq.length];
      this.stepIdx++;
      const dur = this.mood === 'menu' ? 0.45 : 0.24;
      const wave: OscillatorType = this.mood === 'boss' ? 'sawtooth' : 'triangle';
      this.tone(note, note, dur, wave, 0.14, 0, true);
      if (this.stepIdx % 4 === 1) {
        this.tone(note * 0.5, note * 0.5, dur * 1.6, 'sine', 0.18, 0, true);
      }
    }, intervalMs);
  }

  public dispose(): void {
    if (this.musicTimer) clearInterval(this.musicTimer);
    this.musicTimer = null;
  }
}
