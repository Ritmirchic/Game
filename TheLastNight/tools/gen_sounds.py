#!/usr/bin/env python3
"""
THE LAST NIGHT — procedural sound design generator.
All SFX/ambience are synthesized from scratch with numpy (no samples, no external
audio). Output: 16-bit PCM WAV files in ../assets/sounds, ready for AudioStreamWAV.
"""
import math
import os
import wave

import numpy as np

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "sounds")
os.makedirs(OUT, exist_ok=True)
SR = 44100
RNG = np.random.default_rng(20261025)


def t(dur):
    return np.linspace(0, dur, int(SR * dur), endpoint=False)


def noise(n):
    return RNG.standard_normal(n)


def onepole_lp(x, cutoff):
    """Simple one-pole lowpass."""
    a = math.exp(-2 * math.pi * cutoff / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc += (1 - a) * (x[i] - acc)
        y[i] = acc
    return y


def lp(x, cutoff):
    # FFT-free fast approximation via chunked lfilter
    from scipy.signal import lfilter  # type: ignore
    a = math.exp(-2 * math.pi * cutoff / SR)
    return lfilter([1 - a], [1, -a], x).astype(np.float32)


def hp(x, cutoff):
    from scipy.signal import lfilter  # type: ignore
    a = math.exp(-2 * math.pi * cutoff / SR)
    return lfilter([1, -1], [1, -a], x).astype(np.float32)


def env(n, attack=0.005, decay=0.2):
    e = np.zeros(n, dtype=np.float32)
    na = min(n, max(1, int(attack * SR)))
    nd = min(n - na, max(1, int(decay * SR)))
    e[:na] = np.linspace(0, 1, na)
    e[na:na + nd] = np.linspace(1, 0, nd)
    return e


def exp_decay(n, rate=8.0, attack=0.002):
    tt = t(n / SR)
    e = np.exp(-rate * tt)
    na = max(1, int(attack * SR))
    e[:na] *= np.linspace(0, 1, na)
    return e.astype(np.float32)


def norm(x, peak=0.9):
    m = np.max(np.abs(x)) + 1e-9
    return (x / m * peak).astype(np.float32)


def save(name, x, loop=False):
    x = np.clip(x, -1, 1)
    pcm = (x * 32767).astype("<i2")
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        if loop:
            # cue chunk for loop points (Godot reads loop mode via import settings instead)
            pass
        w.writeframes(pcm.tobytes())
    print("sound:", name, f"{len(pcm)/SR:.2f}s")


def gunshot(name, dur, low_hz, noise_level, crack=True):
    tt = t(dur); n = len(tt)
    body = np.sin(2 * math.pi * low_hz * tt) * exp_decay(n, rate=18)
    nz = noise(n) * exp_decay(n, rate=26) * noise_level
    nz = onepole_lp(nz, 3200)
    x = body + nz
    if crack:
        cn = int(0.03 * SR)
        x[:cn] += noise(cn) * 0.7
    save(name, norm(x))


def groan(name, dur, base, rough=0.4, vibrato=6.0):
    tt = t(dur); n = len(tt)
    vib = 1 + 0.06 * np.sin(2 * math.pi * vibrato * tt)
    phase = 2 * math.pi * base * np.cumsum(vib) / SR
    saw = 2 * ((phase / (2 * math.pi)) % 1) - 1
    g = onepole_lp(saw, 420) * 0.6
    breath = onepole_lp(noise(n), 700) * rough
    e = env(n, attack=0.08, decay=dur * 0.9) * (0.6 + 0.4 * np.sin(math.pi * tt / dur))
    save(name, norm((g + breath) * e))


def main():
    # --- weapons
    gunshot("sfx_pistol", 0.22, 130, 0.8)
    gunshot("sfx_shotgun", 0.5, 72, 1.1)
    gunshot("sfx_rifle", 0.18, 160, 0.7)
    gunshot("sfx_hunting", 0.55, 90, 1.0)
    # crossbow: string twang + thunk
    tt = t(0.28); n = len(tt)
    tw = np.sin(2 * math.pi * (240 * np.exp(-6 * tt) + 90) * tt) * exp_decay(n, 10) * 0.5
    th = np.sin(2 * math.pi * 120 * tt) * exp_decay(n, 30) * 0.6
    save("sfx_crossbow", norm(tw + th))
    # melee
    tt = t(0.25); n = len(tt)
    sw = onepole_lp(noise(n), 1800) * np.sin(math.pi * tt / 0.25) * 0.7
    save("sfx_swing", norm(sw))
    tt = t(0.16); n = len(tt)
    hit = onepole_lp(noise(n), 900) * exp_decay(n, 22) + np.sin(2 * math.pi * 90 * tt) * exp_decay(n, 20) * 0.7
    save("sfx_hit", norm(hit))
    # reload: two mechanical clicks
    x = np.zeros(int(SR * 0.5))
    for start, pitch in ((0.05, 2600), (0.3, 1900)):
        i = int(start * SR); cn = int(0.04 * SR)
        x[i:i + cn] += noise(cn) * np.linspace(1, 0, cn) * 0.5
        x[i:i + cn] += np.sin(2 * math.pi * pitch * np.arange(cn) / SR) * 0.25
    save("sfx_reload", norm(x))
    x = np.zeros(int(SR * 0.08)); cn = len(x)
    x[:cn] = noise(cn) * np.linspace(1, 0, cn) * 0.4 + np.sin(2 * math.pi * 2200 * np.arange(cn) / SR) * 0.2
    save("sfx_empty_click", norm(x))
    # pickups / UI
    tt = t(0.18); n = len(tt)
    blip = np.concatenate([np.sin(2 * math.pi * 660 * t(0.09)), np.sin(2 * math.pi * 880 * t(0.09))]) * 0.4
    save("sfx_pickup", norm(blip))
    save("sfx_ui_click", norm(np.sin(2 * math.pi * 520 * t(0.07)) * 0.35))
    save("sfx_ui_hover", norm(np.sin(2 * math.pi * 380 * t(0.05)) * 0.22))
    tt = t(0.3); ar = np.concatenate([np.sin(2 * math.pi * f * t(0.1)) * 0.35 for f in (523, 659, 784)])
    save("sfx_buy", norm(ar))
    save("sfx_revive", norm(np.sin(2 * math.pi * np.linspace(300, 720, int(SR * 0.5))) * 0.3 * np.linspace(0, 1, int(SR * 0.5))))

    # --- zombies
    groan("sfx_walker", 1.1, 78, rough=0.45, vibrato=4.5)
    groan("sfx_runner", 0.55, 140, rough=0.55, vibrato=9)
    tt = t(0.8); n = len(tt)
    craw = onepole_lp(noise(n), 1100) * env(n, 0.05, 0.7) * 0.6 + onepole_lp(noise(n), 300) * 0.3
    save("sfx_crawler", norm(craw))
    groan("sfx_brute", 1.5, 48, rough=0.6, vibrato=3.0)
    tt = t(1.3); n = len(tt)
    sweep = np.sin(2 * math.pi * np.linspace(280, 920, n) * tt) * 0.4
    scream = (sweep + np.sin(2 * math.pi * np.linspace(420, 1380, n) * tt) * 0.25) * env(n, 0.03, 1.2)
    save("sfx_scream", norm(scream + onepole_lp(noise(n), 2400) * 0.25))
    tt = t(0.9); n = len(tt)
    gurg = onepole_lp(noise(n), 500) * np.linspace(1, 0.1, n) + np.sin(2 * math.pi * np.linspace(110, 45, n) * tt) * 0.5
    save("sfx_zdeath", norm(gurg * env(n, 0.02, 0.85)))
    groan("sfx_boss_roar", 2.2, 42, rough=0.75, vibrato=2.2)

    # --- barricades / impacts
    tt = t(0.12); n = len(tt)
    knock = np.sin(2 * math.pi * 160 * tt) * exp_decay(n, 26) * 0.6 + onepole_lp(noise(n), 700) * exp_decay(n, 30) * 0.5
    save("sfx_wood_hit", norm(knock))
    tt = t(0.45); n = len(tt)
    brk = onepole_lp(noise(n), 1400) * exp_decay(n, 9) * 0.8 + np.sin(2 * math.pi * 80 * tt) * exp_decay(n, 12) * 0.5
    save("sfx_wood_break", norm(brk))
    tt = t(0.5); n = len(tt)
    clang = sum(np.sin(2 * math.pi * f * tt) * exp_decay(n, r) for f, r in ((420, 7), (640, 9), (910, 12))) * 0.3
    save("sfx_metal_hit", norm(clang + onepole_lp(noise(n), 3000) * exp_decay(n, 16) * 0.3))
    tt = t(0.09); n = len(tt)
    step = onepole_lp(noise(n), 600) * exp_decay(n, 34) * 0.4
    save("sfx_footstep", norm(step))

    # --- ambience
    # rain: 4s loopable filtered noise with crossfade
    n = SR * 4
    r = onepole_lp(noise(n), 2600) * 0.22 + onepole_lp(noise(n), 500) * 0.12
    fade = int(0.2 * SR)
    r[:fade] *= np.linspace(0, 1, fade); r[-fade:] *= np.linspace(1, 0, fade)
    save("amb_rain", norm(r, 0.7), loop=True)
    # electrical hum: 60Hz + harmonics, 2s loop
    tt = t(2.0); n = len(tt)
    hum = (np.sin(2 * math.pi * 60 * tt) * 0.5 + np.sin(2 * math.pi * 120 * tt) * 0.22 + np.sin(2 * math.pi * 180 * tt) * 0.1) * 0.16
    save("amb_hum", hum.astype(np.float32), loop=True)
    # horror drone: minor chord pad, 4s loop
    tt = t(4.0); n = len(tt)
    lfo = 0.75 + 0.25 * np.sin(2 * math.pi * 0.25 * tt)
    drone = sum(np.sin(2 * math.pi * f * tt) for f in (55, 65.4, 82.4)) / 3 * 0.35 * lfo
    save("amb_drone", (drone + onepole_lp(noise(n), 220) * 0.05).astype(np.float32), loop=True)
    # thunder: swell + crack, 3s
    tt = t(3.0); n = len(tt)
    swell = onepole_lp(noise(n), 240) * (np.sin(math.pi * np.clip(tt / 2.4, 0, 1)) ** 2) * 0.9
    cn = int(0.12 * SR)
    swell[:cn] += noise(cn) * 0.8
    save("sfx_thunder", norm(swell, 0.95))
    print("ALL SOUNDS DONE")


if __name__ == "__main__":
    try:
        import scipy  # noqa: F401
        HAVE_SCIPY = True
    except Exception:
        HAVE_SCIPY = False
    if not HAVE_SCIPY:
        # fallback: make hp/lp use the pure one-pole
        lp = onepole_lp  # noqa: F811
        def hp(x, cutoff):  # noqa: F811
            return x - onepole_lp(x, cutoff)
    main()
