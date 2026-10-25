"""SPACE MINER — procedural audio synthesis.

No sample libraries: every sound effect and every music track is synthesised
here with numpy (oscillators, filtered noise, envelopes, comb reverb).
Outputs 32 kHz mono/stereo PCM ready for the game mixer.
"""
import math
import os
import numpy as np

SR = 32000
rng = np.random.default_rng(20261025)


# --------------------------------------------------------------------------- #
#  primitives
# --------------------------------------------------------------------------- #
def t_axis(dur):
    return np.arange(int(dur * SR)) / SR


def sine(freq, dur, phase=0.0):
    t = t_axis(dur)
    return np.sin(2 * np.pi * freq * t + phase)


def saw(freq, dur, phase=0.0):
    t = t_axis(dur)
    return 2.0 * ((freq * t + phase / (2 * np.pi)) % 1.0) - 1.0


def square(freq, dur, duty=0.5, phase=0.0):
    t = t_axis(dur)
    return np.where(((freq * t + phase / (2 * np.pi)) % 1.0) < duty, 1.0, -1.0)


def noise(dur, seed=None):
    r = np.random.default_rng(seed) if seed is not None else rng
    return r.uniform(-1, 1, int(dur * SR))


def env_adsr(n, a=0.005, d=0.05, s=0.6, r=0.2):
    a_n, d_n, r_n = int(a * SR), int(d * SR), int(r * SR)
    s_n = max(0, n - a_n - d_n - r_n)
    e = np.concatenate([
        np.linspace(0, 1, max(1, a_n)),
        np.linspace(1, s, max(1, d_n)),
        np.full(s_n, s),
        np.linspace(s, 0, max(1, r_n)),
    ])
    if len(e) < n:
        e = np.concatenate([e, np.zeros(n - len(e))])
    return e[:n]


def env_exp(n, decay=6.0):
    t = np.arange(n) / SR
    return np.exp(-t * decay)


def fft_filter(x, lo=None, hi=None, order=2):
    """Zero-phase band filter applied in the frequency domain."""
    n = len(x)
    if n < 8:
        return x
    spec = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(n, 1.0 / SR)
    resp = np.ones_like(freqs)
    if hi is not None:
        resp *= 1.0 / (1.0 + (freqs / hi) ** order)
    if lo is not None:
        with np.errstate(divide='ignore'):
            resp *= 1.0 / (1.0 + (lo / np.maximum(freqs, 1e-6)) ** order)
    return np.fft.irfft(spec * resp, n)


def comb_reverb(x, delay_s=0.045, feedback=0.55, taps=6, mix=0.35):
    out = x.copy()
    d = int(delay_s * SR)
    for i in range(1, taps + 1):
        off = d * i
        if off >= len(x):
            break
        out[off:] += x[:-off] * (feedback ** i) * 0.6
    return x * (1 - mix) + out * mix


def normalize(x, peak=0.92):
    m = np.max(np.abs(x))
    return x if m < 1e-6 else x * (peak / m)


def pad_stereo(x, y):
    n = max(len(x), len(y))
    out = np.zeros((n, 2))
    out[:len(x), 0] = x
    out[:len(y), 1] = y
    return out


def to_pcm(xy, peak=0.9):
    if xy.ndim == 1:
        xy = np.stack([xy, xy], axis=1)
    xy = xy - xy.mean()
    m = np.max(np.abs(xy))
    if m > 1e-6:
        xy = xy * (peak / m)
    return (np.clip(xy, -1, 1) * 32767).astype(np.int16)


# --------------------------------------------------------------------------- #
#  SFX
# --------------------------------------------------------------------------- #
def sfx_laser(dur=0.16, f0=1500, f1=320, bright=1.0):
    t = t_axis(dur)
    f = f0 * (f1 / f0) ** (t / dur)
    ph = 2 * np.pi * np.cumsum(f) / SR
    body = np.sin(ph) * 0.7 + np.sin(ph * 2.01) * 0.25 + np.sin(ph * 3.02) * 0.12
    body *= np.exp(-t * (22 / bright))
    hiss = fft_filter(noise(dur, 11), lo=900, hi=6000) * np.exp(-t * 40) * 0.3
    return normalize(body + hiss, 0.85)


def sfx_lance(dur=0.42):
    t = t_axis(dur)
    f = 220 * np.exp(-t * 3.2) + 60
    body = np.sin(2 * np.pi * np.cumsum(f) / SR) * 0.8
    body += np.sin(2 * np.pi * np.cumsum(f * 2.02) / SR) * 0.2
    body *= np.exp(-t * 7)
    rush = fft_filter(noise(dur, 12), lo=300, hi=3500) * np.exp(-t * 6) * 0.5
    return normalize(body + rush, 0.9)


def sfx_arc(dur=0.22):
    t = t_axis(dur)
    n = noise(dur, 13)
    crackle = np.sign(n) * np.exp(-((t * 90) % 1) * 6)
    tone = square(180 * np.exp(-t * 5) + 40, dur) * np.exp(-t * 9) * 0.5
    return normalize(fft_filter(crackle, lo=1200, hi=9000) * 0.7 + tone, 0.85)


def sfx_mine_loop(dur=1.0):
    t = t_axis(dur)
    # beam hum with slow pulsing
    base = np.sin(2 * np.pi * 96 * t) * 0.35 + np.sin(2 * np.pi * 192.5 * t) * 0.2
    pulse = 0.6 + 0.4 * np.sin(2 * np.pi * 12 * t)
    hi = fft_filter(noise(dur, 21), lo=2200, hi=7000) * 0.22
    x = (base * pulse + hi) * (0.5 + 0.5 * np.sin(2 * np.pi * 3 * t) ** 2)
    # smooth loop join
    k = int(0.02 * SR)
    x[:k] *= np.linspace(0, 1, k)
    x[-k:] *= np.linspace(1, 0, k)
    return normalize(x, 0.55)


def sfx_explosion(dur=0.9, size=1.0, seed=31):
    t = t_axis(dur)
    n = noise(dur, seed)
    low = fft_filter(n, hi=260 * size) * np.exp(-t * (3.0 / size))
    mid = fft_filter(n, lo=200, hi=1800) * np.exp(-t * (7.0 / size)) * 0.6
    thump = np.sin(2 * np.pi * np.cumsum(90 * np.exp(-t * 5) + 35) / SR) * np.exp(-t * (5 / size))
    x = low * 1.2 + mid + thump * 0.9
    return normalize(comb_reverb(x, 0.03, 0.35, 4, 0.2), 0.95)


def sfx_pickup(dur=0.12, f=1180):
    t = t_axis(dur)
    x = np.sin(2 * np.pi * f * t) * np.exp(-t * 26)
    x += np.sin(2 * np.pi * f * 2 * t) * np.exp(-t * 40) * 0.4
    return normalize(x, 0.6)


def sfx_coin(dur=0.5):
    a = sfx_pickup(0.18, 1560)
    b = sfx_pickup(0.28, 2090)
    out = np.zeros(int(dur * SR))
    out[:len(a)] += a
    out[int(0.07 * SR):int(0.07 * SR) + len(b)] += b * 0.8
    return normalize(out, 0.7)


def sfx_buy(dur=0.4):
    out = np.zeros(int(dur * SR))
    for i, f in enumerate((520, 700, 940)):
        s = sfx_pickup(0.2, f) * 0.6
        st = int(i * 0.06 * SR)
        out[st:st + len(s)] += s[:len(out) - st]
    return normalize(out, 0.7)


def sfx_error(dur=0.22):
    t = t_axis(dur)
    x = square(120, dur, 0.35) * np.exp(-t * 8) * 0.5
    x += fft_filter(noise(dur, 41), lo=200, hi=1200) * np.exp(-t * 12) * 0.3
    return normalize(x, 0.7)


def sfx_click(dur=0.05):
    t = t_axis(dur)
    x = np.sin(2 * np.pi * 2200 * t) * np.exp(-t * 90)
    x += fft_filter(noise(dur, 51), lo=1500, hi=8000) * np.exp(-t * 140) * 0.4
    return normalize(x, 0.45)


def sfx_hover(dur=0.04):
    t = t_axis(dur)
    x = np.sin(2 * np.pi * 1600 * t) * np.exp(-t * 120) * 0.5
    return normalize(x, 0.28)


def sfx_shield_hit(dur=0.45):
    t = t_axis(dur)
    x = np.sin(2 * np.pi * 860 * t) * np.exp(-t * 11)
    x += np.sin(2 * np.pi * 1270 * t) * np.exp(-t * 15) * 0.6
    x += fft_filter(noise(dur, 61), lo=2000, hi=9000) * np.exp(-t * 20) * 0.35
    return normalize(comb_reverb(x, 0.01, 0.3, 3, 0.15), 0.8)


def sfx_hull_hit(dur=0.5):
    t = t_axis(dur)
    x = fft_filter(noise(dur, 71), hi=1400) * np.exp(-t * 9)
    x += np.sin(2 * np.pi * np.cumsum(180 * np.exp(-t * 8) + 50) / SR) * np.exp(-t * 6) * 0.8
    return normalize(x, 0.9)


def sfx_thruster_loop(dur=1.2):
    t = t_axis(dur)
    rumble = fft_filter(noise(dur, 81), lo=40, hi=380) * (0.6 + 0.4 * np.sin(2 * np.pi * 7 * t))
    hiss = fft_filter(noise(dur, 82), lo=600, hi=3200) * 0.18
    x = rumble + hiss
    k = int(0.05 * SR)
    x[:k] *= np.linspace(0, 1, k)
    x[-k:] *= np.linspace(1, 0, k)
    return normalize(x, 0.5)


def sfx_alarm(dur=1.1):
    t = t_axis(dur)
    f = 640 + 220 * np.sin(2 * np.pi * 2.2 * t)
    x = np.sin(2 * np.pi * np.cumsum(f) / SR) * 0.5
    x *= (np.sin(2 * np.pi * 2.2 * t) > -0.3)
    x = fft_filter(x, hi=4000)
    return normalize(x, 0.55)


def sfx_levelup(dur=1.1):
    out = np.zeros(int(dur * SR))
    notes = (523.25, 659.25, 783.99, 1046.5)
    for i, f in enumerate(notes):
        s = sfx_pickup(0.6, f) * 0.7
        st = int(i * 0.1 * SR)
        out[st:st + len(s)] += s[:len(out) - st]
    return normalize(comb_reverb(out, 0.06, 0.4, 4, 0.3), 0.8)


def sfx_warp(dur=1.4):
    t = t_axis(dur)
    f = 80 * np.exp(t * 2.0)
    x = np.sin(2 * np.pi * np.cumsum(f) / SR) * 0.5
    x += fft_filter(noise(dur, 91), lo=400, hi=5000) * (t / dur) ** 2 * 0.7
    x *= np.minimum(1, (dur - t) * 6)
    return normalize(x, 0.85)


def sfx_achievement(dur=1.2):
    out = np.zeros(int(dur * SR))
    for i, f in enumerate((784, 988, 1175, 1568)):
        s = sfx_pickup(0.7, f) * 0.6
        st = int(i * 0.09 * SR)
        out[st:st + len(s)] += s[:len(out) - st]
    return normalize(comb_reverb(out, 0.08, 0.45, 5, 0.35), 0.85)


def sfx_dock(dur=1.2):
    t = t_axis(dur)
    x = np.sin(2 * np.pi * np.cumsum(60 * np.exp(-t * 4) + 28) / SR) * np.exp(-t * 3)
    x += fft_filter(noise(dur, 101), hi=900) * np.exp(-t * 5) * 0.5
    x += np.sin(2 * np.pi * 300 * t) * np.exp(-t * 12) * 0.3
    return normalize(comb_reverb(x, 0.05, 0.4, 4, 0.25), 0.9)


def sfx_scan(dur=0.7):
    t = t_axis(dur)
    f = 400 + 900 * (t / dur)
    x = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 3) * 0.5
    x += np.sin(2 * np.pi * np.cumsum(f * 1.5) / SR) * np.exp(-t * 6) * 0.2
    return normalize(x, 0.5)


def sfx_drone_loop(dur=1.0):
    t = t_axis(dur)
    x = np.sin(2 * np.pi * 210 * t) * 0.3 + np.sin(2 * np.pi * 315 * t) * 0.15
    x *= 0.7 + 0.3 * np.sin(2 * np.pi * 9 * t)
    x += fft_filter(noise(dur, 111), lo=1200, hi=4000) * 0.10
    k = int(0.03 * SR)
    x[:k] *= np.linspace(0, 1, k)
    x[-k:] *= np.linspace(1, 0, k)
    return normalize(x, 0.4)


# --------------------------------------------------------------------------- #
#  music
# --------------------------------------------------------------------------- #
SCALES = {
    'minor':     [0, 2, 3, 5, 7, 8, 10],
    'dorian':    [0, 2, 3, 5, 7, 9, 10],
    'phrygian':  [0, 1, 3, 5, 7, 8, 10],
    'lydian':    [0, 2, 4, 6, 7, 9, 11],
    'pent_minor': [0, 3, 5, 7, 10],
    'aeolian':   [0, 2, 3, 5, 7, 8, 10],
}


def note_hz(root, scale, degree, octave=0):
    sc = SCALES[scale]
    n = len(sc)
    idx = degree % n
    oct_add = degree // n
    semi = sc[idx] + 12 * (oct_add + octave)
    return root * (2 ** (semi / 12.0))


def synth_pad(freqs, dur, detune=0.004, brightness=1400, seed=0):
    n = int(dur * SR)
    out = np.zeros(n)
    t = np.arange(n) / SR
    for i, f in enumerate(freqs):
        for k, dt in enumerate((-detune, detune, 0.0)):
            s = saw(f * (1 + dt), dur) * 0.30
            s += sine(f * (1 + dt * 2), dur) * 0.18
            out += s
    out = fft_filter(out, lo=60, hi=brightness, order=2)
    lfo = 0.75 + 0.25 * np.sin(2 * np.pi * 0.13 * t + seed)
    out *= lfo
    # slow fade in/out to guarantee a seamless loop
    k = int(0.8 * SR)
    out[:k] *= np.linspace(0, 1, k)
    out[-k:] *= np.linspace(1, 0, k)
    return out / max(1, len(freqs) * 3)


def synth_pluck(freq, dur=0.6, bright=2600):
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for h in range(1, 7):
        x += np.sin(2 * np.pi * freq * h * t + h * 0.3) * (0.6 ** h) / h
    x *= np.exp(-t * 5.5)
    x = fft_filter(x, hi=bright)
    return x * 0.5


def synth_bass(freq, dur=1.0):
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = np.sin(2 * np.pi * freq * t) * 0.8 + np.sin(2 * np.pi * freq * 2 * t) * 0.2
    x = np.tanh(x * 1.6) * np.exp(-t * 2.2)
    k = int(0.01 * SR)
    x[:k] *= np.linspace(0, 1, k)
    return x


def synth_hat(dur=0.09, bright=9000, decay=60):
    t = t_axis(dur)
    x = fft_filter(noise(dur, int(decay)), lo=4000, hi=bright) * np.exp(-t * decay)
    return x * 0.22


def synth_kick(dur=0.4):
    t = t_axis(dur)
    f = 120 * np.exp(-t * 18) + 44
    x = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 8)
    return x * 0.55


def synth_sweep(dur, f0, f1, seed=0):
    t = t_axis(dur)
    n = noise(dur, seed)
    x = fft_filter(n, lo=min(f0, f1), hi=max(f0, f1)) * (np.sin(np.pi * t / dur) ** 2)
    return x * 0.25


def make_track(root, scale, bpm, bars, mood, seed=1, arp_speed=4, bass_speed=2):
    """Compose a seamless loop from procedural layers."""
    spb = 60.0 / bpm
    dur = bars * 4 * spb
    n = int(dur * SR)
    left = np.zeros(n)
    right = np.zeros(n)
    r = np.random.default_rng(seed)

    chords = {
        'calm':    [[0, 2, 4], [3, 5, 0], [5, 0, 2], [4, 6, 1]],
        'wonder':  [[0, 2, 4, 6], [3, 5, 0, 2], [4, 6, 1, 3], [5, 0, 2, 4]],
        'tense':   [[0, 2, 4], [1, 3, 5], [0, 2, 4], [4, 6, 1]],
        'hostile': [[0, 3, 5], [1, 4, 6], [0, 3, 5], [5, 1, 3]],
        'vast':    [[0, 4, 7], [2, 5, 0], [3, 6, 1], [5, 2, 4]],
    }[mood]

    # --- pad layer (one chord per bar) ---
    for bar in range(bars):
        ch = chords[bar % len(chords)]
        freqs = [note_hz(root, scale, d, 0) for d in ch]
        pad = synth_pad(freqs, 4 * spb * 1.02, detune=0.0035,
                        brightness=mood in ('calm', 'vast') and 1100 or 1700, seed=seed + bar)
        start = int(bar * 4 * spb * SR)
        end = min(n, start + len(pad))
        left[start:end] += pad[:end - start] * 0.9
        right[start:end] += np.roll(pad, 220)[:end - start] * 0.9

    # --- bass ---
    step = spb * (4 / bass_speed)
    for i in range(int(dur / step)):
        bar = int((i * step) / (4 * spb)) % len(chords)
        ch = chords[bar]
        deg = ch[0] - 7
        f = note_hz(root, scale, deg, 0)
        b = synth_bass(f, step * 0.95)
        st = int(i * step * SR)
        en = min(n, st + len(b))
        if en > st:
            left[st:en] += b[:en - st] * 0.55
            right[st:en] += b[:en - st] * 0.55

    # --- arpeggio ---
    if mood != 'vast':
        step = spb * (4 / arp_speed)
        for i in range(int(dur / step)):
            bar = int((i * step) / (4 * spb)) % len(chords)
            ch = chords[bar]
            deg = ch[i % len(ch)] + 7
            f = note_hz(root, scale, deg, 0)
            if r.random() < 0.18:
                continue
            p = synth_pluck(f, step * 2.2, bright=3200)
            st = int(i * step * SR)
            en = min(n, st + len(p))
            pan = 0.5 + 0.35 * math.sin(i * 0.7)
            if en > st:
                left[st:en] += p[:en - st] * (1 - pan) * 0.5
                right[st:en] += p[:en - st] * pan * 0.5

    # --- percussion ---
    beat = spb
    if mood in ('tense', 'hostile', 'wonder'):
        for i in range(int(dur / beat)):
            k = synth_kick(0.32)
            st = int(i * beat * SR)
            en = min(n, st + len(k))
            if en > st:
                left[st:en] += k[:en - st] * (0.5 if i % 2 == 0 else 0.25)
                right[st:en] += k[:en - st] * (0.5 if i % 2 == 0 else 0.25)
            for hh in range(2):
                h = synth_hat(0.07, decay=70)
                st2 = int((i * beat + hh * beat / 2) * SR)
                en2 = min(n, st2 + len(h))
                if en2 > st2:
                    left[st2:en2] += h[:en2 - st2]
                    right[st2:en2] += h[:en2 - st2] * 0.8

    # --- atmosphere sweeps ---
    for i in range(3):
        d = dur * 0.5
        sw = synth_sweep(d, 200 + i * 300, 3000 + i * 900, seed + i * 7)
        st = int(i * dur / 3 * SR)
        en = min(n, st + len(sw))
        if en > st:
            left[st:en] += sw[:en - st] * 0.5
            right[st:en] += sw[:en - st] * 0.35

    # --- reverb + master ---
    left = comb_reverb(left, 0.075, 0.45, 5, 0.28)
    right = comb_reverb(right, 0.083, 0.45, 5, 0.28)
    left = fft_filter(left, lo=40, hi=7000)
    right = fft_filter(right, lo=40, hi=7000)
    mx = max(np.max(np.abs(left)), np.max(np.abs(right)), 1e-6)
    left *= 0.72 / mx
    right *= 0.72 / mx
    # seamless loop: crossfade the tail into the head
    xf = int(0.35 * SR)
    for ch in (left, right):
        head = ch[:xf].copy()
        tail = ch[-xf:].copy()
        ramp = np.linspace(0, 1, xf)
        ch[:xf] = head * ramp + tail * (1 - ramp)
        ch[-xf:] *= np.linspace(1, 0, xf)
    return np.stack([left, right], axis=1)


def make_ambience(root, scale, bpm, dur=24.0, seed=3):
    """Menu / dock theme: sparse and calm."""
    n = int(dur * SR)
    left = np.zeros(n)
    right = np.zeros(n)
    r = np.random.default_rng(seed)
    chords = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]
    for bar in range(4):
        freqs = [note_hz(root, scale, d, -1) for d in chords[bar]]
        pad = synth_pad(freqs, dur / 4 * 1.05, detune=0.002, brightness=900, seed=seed + bar)
        st = int(bar * dur / 4 * SR)
        en = min(n, st + len(pad))
        left[st:en] += pad[:en - st] * 0.8
        right[st:en] += np.roll(pad, 300)[:en - st] * 0.8
    for i in range(int(dur / 2.4)):
        f = note_hz(root, scale, int(r.integers(0, 7)) + 7, 0)
        p = synth_pluck(f, 2.0, bright=2400)
        st = int(i * 2.4 * SR)
        en = min(n, st + len(p))
        if en > st:
            left[st:en] += p[:en - st] * 0.30
            right[st:en] += p[:en - st] * 0.22
    left = comb_reverb(left, 0.09, 0.5, 6, 0.4)
    right = comb_reverb(right, 0.1, 0.5, 6, 0.4)
    mx = max(np.max(np.abs(left)), np.max(np.abs(right)), 1e-6)
    left *= 0.6 / mx
    right *= 0.6 / mx
    xf = int(0.4 * SR)
    for ch in (left, right):
        head = ch[:xf].copy()
        tail = ch[-xf:].copy()
        ramp = np.linspace(0, 1, xf)
        ch[:xf] = head * ramp + tail * (1 - ramp)
        ch[-xf:] *= np.linspace(1, 0, xf)
    return np.stack([left, right], axis=1)


TRACKS = [
    ('music_belt',   dict(root=110.0, scale='dorian', bpm=86, bars=8, mood='calm', seed=11, arp_speed=4)),
    ('music_nebula', dict(root=123.47, scale='lydian', bpm=96, bars=8, mood='wonder', seed=23, arp_speed=6)),
    ('music_grave',  dict(root=98.0, scale='phrygian', bpm=104, bars=8, mood='tense', seed=37, arp_speed=4)),
    ('music_storm',  dict(root=87.31, scale='minor', bpm=132, bars=8, mood='hostile', seed=53, arp_speed=8)),
    ('music_void',   dict(root=73.42, scale='aeolian', bpm=78, bars=8, mood='vast', seed=71, arp_speed=4)),
]

SFX = {
    'sfx_laser': lambda: sfx_laser(),
    'sfx_lance': lambda: sfx_lance(),
    'sfx_arc': lambda: sfx_arc(),
    'sfx_mine': lambda: sfx_mine_loop(),
    'sfx_explode_small': lambda: sfx_explosion(0.75, 0.7, 31),
    'sfx_explode_big': lambda: sfx_explosion(1.5, 1.6, 41),
    'sfx_pickup': lambda: sfx_pickup(),
    'sfx_coin': lambda: sfx_coin(),
    'sfx_buy': lambda: sfx_buy(),
    'sfx_error': lambda: sfx_error(),
    'sfx_click': lambda: sfx_click(),
    'sfx_hover': lambda: sfx_hover(),
    'sfx_shield': lambda: sfx_shield_hit(),
    'sfx_hull': lambda: sfx_hull_hit(),
    'sfx_thruster': lambda: sfx_thruster_loop(),
    'sfx_alarm': lambda: sfx_alarm(),
    'sfx_levelup': lambda: sfx_levelup(),
    'sfx_warp': lambda: sfx_warp(),
    'sfx_achieve': lambda: sfx_achievement(),
    'sfx_dock': lambda: sfx_dock(),
    'sfx_scan': lambda: sfx_scan(),
    'sfx_drone': lambda: sfx_drone_loop(),
}


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    import wave
    for name, fn in SFX.items():
        x = fn()
        pcm = to_pcm(x, 0.85)
        with wave.open(f'{out_dir}/{name}.wav', 'wb') as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(SR)
            w.writeframes(pcm.tobytes())
        print(name, len(pcm) / SR, 's')
    for name, kw in TRACKS:
        xy = make_track(**kw)
        pcm = to_pcm(xy, 0.8)
        with wave.open(f'{out_dir}/{name}.wav', 'wb') as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(SR)
            w.writeframes(pcm.tobytes())
        print(name, round(len(pcm) / SR, 1), 's')
    amb = make_ambience(130.81, 'lydian', 70, 26.0, seed=5)
    pcm = to_pcm(amb, 0.75)
    with wave.open(f'{out_dir}/music_menu.wav', 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print('music_menu', round(len(pcm) / SR, 1), 's')


if __name__ == '__main__':
    import sys
    main(sys.argv[1] if len(sys.argv) > 1 else 'assets/gen/audio')
