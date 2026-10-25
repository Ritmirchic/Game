#!/usr/bin/env python3
"""
Synthesizes 13 original 16-bit PCM WAV laboratory sound effects & ambient loop
for ALCHEMY CHAOS using NumPy DSP synthesis.
"""

import os
import wave
import numpy as np

SR = 22050


def save_wav(path: str, samples: np.ndarray):
    peak = np.max(np.abs(samples))
    if peak > 1e-6:
        samples = samples / max(1.0, peak / 0.88)
    pcm = np.clip(samples * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(pcm.tobytes())


def env_adsr(n: int, a=0.02, d=0.15, s=0.5, r=0.25):
    e = np.ones(n, dtype=np.float32) * s
    na = max(1, int(n * a))
    nd = max(1, int(n * d))
    nr = max(1, int(n * r))
    e[:na] = np.linspace(0.0, 1.0, na)
    if na + nd < n:
        e[na:na + nd] = np.linspace(1.0, s, nd)
    e[-nr:] = np.linspace(e[-nr], 0.0, nr)
    return e


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "alchemy_chaos", "assets", "audio")
    os.makedirs(out_dir, exist_ok=True)
    rng = np.random.default_rng(42)

    # 1. sfx_click.wav — warm brass/wood tactile click
    t = np.linspace(0, 0.075, int(SR * 0.075), endpoint=False)
    sig = (0.65 * np.sin(2 * np.pi * 680 * t * np.exp(-18 * t)) + 0.35 * np.sin(2 * np.pi * 1360 * t)) * np.exp(-42 * t)
    save_wav(os.path.join(out_dir, "sfx_click.wav"), sig)

    # 2. sfx_bubble.wav — rising pitch liquid blips
    dur = 0.85
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    sig = np.zeros_like(t)
    for bt, f0 in [(0.02, 290), (0.14, 360), (0.25, 440), (0.38, 320), (0.50, 510), (0.63, 410), (0.72, 590)]:
        idx = t >= bt
        lt = t[idx] - bt
        freq = f0 * (1.0 + 3.2 * lt)
        sig[idx] += 0.38 * np.sin(2 * np.pi * freq * lt) * np.exp(-22 * lt)
    save_wav(os.path.join(out_dir, "sfx_bubble.wav"), sig)

    # 3. sfx_smoke.wav — filtered vapor hiss
    dur = 0.9
    n = int(SR * dur)
    noise = rng.standard_normal(n).astype(np.float32)
    kernel = np.hanning(18)
    kernel /= kernel.sum()
    sig = np.convolve(noise, kernel, mode="same") * env_adsr(n, 0.08, 0.25, 0.45, 0.45) * 0.65
    save_wav(os.path.join(out_dir, "sfx_smoke.wav"), sig)

    # 4. sfx_spark.wav — metallic flint crackle + high harmonic ping
    dur = 0.65
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    sig = np.zeros_like(t)
    for st, f0 in [(0.0, 2200), (0.08, 3100), (0.15, 2650), (0.26, 3600), (0.37, 2900)]:
        idx = t >= st
        lt = t[idx] - st
        sig[idx] += 0.35 * (np.sin(2 * np.pi * f0 * lt) + 0.5 * rng.standard_normal(lt.size)) * np.exp(-38 * lt)
    save_wav(os.path.join(out_dir, "sfx_spark.wav"), sig)

    # 5. sfx_explosion.wav — deep sub-thump + alchemical crackle
    dur = 1.1
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    boom = 0.8 * np.sin(2 * np.pi * (120 * np.exp(-4.5 * t) + 32) * t) * np.exp(-3.8 * t)
    noise = np.convolve(rng.standard_normal(t.size), np.ones(10) / 10.0, mode="same") * np.exp(-5.5 * t) * 0.55
    save_wav(os.path.join(out_dir, "sfx_explosion.wav"), boom + noise)

    # 6. sfx_crystal.wav — shimmering harmonic bell chord
    dur = 1.05
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    sig = np.zeros_like(t)
    for st, f0 in [(0.0, 1046.5), (0.07, 1318.5), (0.14, 1568.0), (0.21, 2093.0)]:
        idx = t >= st
        lt = t[idx] - st
        sig[idx] += 0.30 * (np.sin(2 * np.pi * f0 * lt) + 0.35 * np.sin(2 * np.pi * f0 * 2.01 * lt)) * np.exp(-4.2 * lt)
    save_wav(os.path.join(out_dir, "sfx_crystal.wav"), sig)

    # 7. sfx_freeze.wav — crisp crystalline ice sweep
    dur = 0.9
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    sweep = np.sin(2 * np.pi * (1800 - 900 * t) * t) * 0.35 * np.exp(-3.5 * t)
    crackle = (rng.random(t.size) > 0.985).astype(np.float32) * 0.7 * np.exp(-3.0 * t)
    save_wav(os.path.join(out_dir, "sfx_freeze.wav"), sweep + crackle)

    # 8. sfx_melt.wav — warm furnace swell + sizzle
    dur = 0.9
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    hum = 0.45 * (np.sin(2 * np.pi * 110 * t) + 0.5 * np.sin(2 * np.pi * 220 * t)) * env_adsr(t.size, 0.15, 0.2, 0.6, 0.4)
    sizzle = rng.standard_normal(t.size) * 0.18 * env_adsr(t.size, 0.05, 0.2, 0.4, 0.4)
    save_wav(os.path.join(out_dir, "sfx_melt.wav"), hum + sizzle)

    # 9. sfx_electric.wav — galvanic buzz + arc snaps
    dur = 0.75
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    saw = 2.0 * ((t * 120.0) % 1.0) - 1.0
    fm = np.sin(2 * np.pi * 880 * t + 6.0 * np.sin(2 * np.pi * 60 * t))
    sig = (0.35 * saw + 0.35 * fm) * env_adsr(t.size, 0.02, 0.15, 0.55, 0.35)
    save_wav(os.path.join(out_dir, "sfx_electric.wav"), sig)

    # 10. sfx_discover.wav — uplifting arcane arpeggio
    dur = 1.2
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    sig = np.zeros_like(t)
    for st, f0 in [(0.0, 523.25), (0.11, 659.25), (0.22, 783.99), (0.34, 1046.50)]:
        idx = t >= st
        lt = t[idx] - st
        sig[idx] += 0.34 * (np.sin(2 * np.pi * f0 * lt) + 0.3 * np.sin(2 * np.pi * f0 * 2 * lt)) * np.exp(-3.4 * lt)
    save_wav(os.path.join(out_dir, "sfx_discover.wav"), sig)

    # 11. sfx_rare_discover.wav — majestic celestial fifth-and-octave fanfare
    dur = 1.75
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    sig = np.zeros_like(t)
    for st, f0 in [(0.0, 440.0), (0.12, 554.37), (0.24, 659.25), (0.36, 880.0), (0.50, 1108.73), (0.64, 1318.51)]:
        idx = t >= st
        lt = t[idx] - st
        sig[idx] += 0.28 * (np.sin(2 * np.pi * f0 * lt) + 0.4 * np.sin(2 * np.pi * f0 * 1.5 * lt)) * np.exp(-2.4 * lt)
    save_wav(os.path.join(out_dir, "sfx_rare_discover.wav"), sig)

    # 12. sfx_upgrade.wav — brass anvil clink + harmonic rise
    dur = 0.9
    t = np.linspace(0, dur, int(SR * dur), endpoint=False)
    clink = 0.55 * np.sin(2 * np.pi * 1480 * t) * np.exp(-16 * t) + 0.35 * np.sin(2 * np.pi * 2210 * t) * np.exp(-24 * t)
    chord = 0.35 * (np.sin(2 * np.pi * 587.33 * t) + np.sin(2 * np.pi * 880.0 * t)) * env_adsr(t.size, 0.08, 0.2, 0.5, 0.4)
    save_wav(os.path.join(out_dir, "sfx_upgrade.wav"), clink + chord)

    # 13. amb_lab_loop.wav — seamless 6-second warm atmospheric arcane hum
    dur = 6.0
    n = int(SR * dur)
    t = np.linspace(0, dur, n, endpoint=False)
    # Exact integer cycles over 6.0s for seamless zero-pop loop!
    drone = (
        0.22 * np.sin(2 * np.pi * 110.0 * t)
        + 0.14 * np.sin(2 * np.pi * 165.0 * t)
        + 0.09 * np.sin(2 * np.pi * 220.0 * t) * (0.6 + 0.4 * np.sin(2 * np.pi * (1.0 / 6.0) * t))
        + 0.05 * np.sin(2 * np.pi * 330.0 * t) * (0.5 + 0.5 * np.cos(2 * np.pi * (2.0 / 6.0) * t))
    )
    save_wav(os.path.join(out_dir, "amb_lab_loop.wav"), drone * 0.45)
    print("Generated 13 WAV audio assets in", out_dir)


if __name__ == "__main__":
    main()
