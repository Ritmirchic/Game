#!/usr/bin/env python3
"""
Procedural Acoustic & Physical-Modeling Horror Sound Design Suite
for DON'T LOOK BACK (Godot 4.2 Spatial Audio).
Generates 32 clean, normalized 16-bit PCM WAV files (mono for 3D spatialization,
stereo for atmospheric drones & menu music).
"""
import os
import wave
import math
import numpy as np
from scipy.signal import butter, lfilter, iirpeak

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
AUD_DIR = os.path.join(ROOT, "assets", "audio")
os.makedirs(AUD_DIR, exist_ok=True)

SR = 22050


def bp(sig, low, high, order=2):
    nyq = SR * 0.5
    lo = max(20.0, min(low, nyq * 0.92)) / nyq
    hi = max(lo + 0.02, min(high, nyq * 0.96)) / nyq
    b, a = butter(order, [lo, hi], btype="band")
    return lfilter(b, a, sig)


def lp(sig, cutoff, order=2):
    nyq = SR * 0.5
    c = max(20.0, min(cutoff, nyq * 0.95)) / nyq
    b, a = butter(order, c, btype="low")
    return lfilter(b, a, sig)


def hp(sig, cutoff, order=2):
    nyq = SR * 0.5
    c = max(20.0, min(cutoff, nyq * 0.95)) / nyq
    b, a = butter(order, c, btype="high")
    return lfilter(b, a, sig)


def resonator(sig, freq, q=12.0):
    nyq = SR * 0.5
    w0 = max(0.005, min(freq / nyq, 0.95))
    b, a = iirpeak(w0, q)
    return lfilter(b, a, sig)


def simple_reverb(sig, delays_ms=(31, 47, 73, 113), decay=0.38):
    out = sig.copy().astype(np.float64)
    for i, dm in enumerate(delays_ms):
        d = int(SR * dm / 1000.0)
        gain = (decay ** (i + 1))
        if d < len(out):
            padded = np.zeros_like(out)
            padded[d:] = sig[:-d] * gain
            out += padded
    return out


def save_wav(name, data, peak=0.88, stereo=False, fade_ms=15):
    arr = np.asarray(data, dtype=np.float64)
    m = np.max(np.abs(arr)) + 1e-8
    arr = (arr / m) * peak
    fade = int(SR * fade_ms / 1000.0)
    if fade > 0 and arr.shape[0] > fade * 2:
        env = np.linspace(0.0, 1.0, fade)
        if arr.ndim == 1:
            arr[:fade] *= env
            arr[-fade:] *= env[::-1]
        else:
            arr[:fade, :] *= env[:, None]
            arr[-fade:, :] *= env[::-1, None]
    pcm = np.clip(arr * 32767.0, -32767, 32767).astype(np.int16)
    path = os.path.join(AUD_DIR, f"{name}.wav")
    with wave.open(path, "wb") as wf:
        wf.setnchannels(2 if stereo else 1)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(pcm.tobytes())


def gen_footsteps(rng):
    # Wood footsteps (hollow sub-thump + mid wood body + sole scuff)
    for i in range(1, 4):
        n = int(SR * 0.28)
        t = np.arange(n) / SR
        heel = np.sin(2 * np.pi * (92 + i * 7) * t * np.exp(-t * 14)) * np.exp(-t * 28)
        body = resonator(rng.standard_normal(n) * np.exp(-t * 34), 240 + i * 25, q=5.0)
        toe_t = np.maximum(0.0, t - 0.055)
        scuff = bp(rng.standard_normal(n), 600, 2800) * np.exp(-toe_t * 38) * (t > 0.055) * 0.35
        sig = simple_reverb(heel * 0.9 + body * 0.55 + scuff, (19, 37), 0.18)
        save_wav(f"step_wood_0{i}", sig, peak=0.78, fade_ms=4)

    # Concrete footsteps (crisp grit transient + tight slap)
    for i in range(1, 4):
        n = int(SR * 0.24)
        t = np.arange(n) / SR
        thud = np.sin(2 * np.pi * (115 + i * 9) * t * np.exp(-t * 18)) * np.exp(-t * 36)
        grit = bp(rng.standard_normal(n), 1100, 5200) * np.exp(-t * 45) * 0.65
        sig = simple_reverb(thud * 0.7 + grit, (23, 53), 0.24)
        save_wav(f"step_concrete_0{i}", sig, peak=0.75, fade_ms=4)

    # Linoleum tile footsteps (sharp sole click + room reflection)
    for i in range(1, 3):
        n = int(SR * 0.22)
        t = np.arange(n) / SR
        click = bp(rng.standard_normal(n), 1600, 6500) * np.exp(-t * 58)
        low = np.sin(2 * np.pi * (140 + i * 10) * t) * np.exp(-t * 40) * 0.55
        save_wav(f"step_tile_0{i}", simple_reverb(click + low, (17, 41), 0.26), peak=0.74, fade_ms=3)

    # Metal stair/elevator footsteps (metallic ring harmonics)
    for i in range(1, 3):
        n = int(SR * 0.34)
        t = np.arange(n) / SR
        imp = rng.standard_normal(n) * np.exp(-t * 42)
        ring = (
            resonator(imp, 410 + i * 35, q=18) * 0.6
            + resonator(imp, 875 + i * 45, q=22) * 0.4
            + resonator(imp, 1490, q=16) * 0.25
        )
        thud = np.sin(2 * np.pi * 105 * t) * np.exp(-t * 28) * 0.7
        save_wav(f"step_metal_0{i}", simple_reverb(ring + thud, (29, 61), 0.30), peak=0.76, fade_ms=4)


def gen_doors_and_props(rng):
    # Door open creak (stick-slip FM wood/hinge resonance + latch release)
    n = int(SR * 0.85)
    t = np.arange(n) / SR
    latch = bp(rng.standard_normal(n), 1200, 4800) * np.exp(-t * 65) * 0.9
    f_creak = 290 + 115 * np.sin(t * 7.5) + 35 * np.sin(t * 31.0)
    phase = np.cumsum(2 * np.pi * f_creak / SR)
    stick = np.tanh(2.6 * np.sin(phase) + 1.2 * np.sin(phase * 2.03))
    creak_env = np.clip((t - 0.08) * 8.0, 0, 1) * np.exp(-np.maximum(0, t - 0.18) * 3.4)
    creak = bp(stick * creak_env, 250, 2900) * 0.65
    save_wav("door_open_creak", simple_reverb(latch + creak, (29, 59), 0.25), peak=0.80)

    # Door close latch
    n = int(SR * 0.48)
    t = np.arange(n) / SR
    thump = np.sin(2 * np.pi * 78 * t * np.exp(-t * 10)) * np.exp(-t * 20)
    click = bp(rng.standard_normal(n), 1400, 5200) * np.exp(-np.maximum(0, t - 0.04) * 75) * (t > 0.04) * 0.75
    save_wav("door_close_latch", simple_reverb(thump + click, (31, 67), 0.28), peak=0.84, fade_ms=4)

    # Door locked rattle
    n = int(SR * 0.55)
    t = np.arange(n) / SR
    rattle = np.zeros(n)
    for onset in (0.02, 0.14, 0.27):
        dt = np.maximum(0, t - onset)
        mask = (t >= onset).astype(float)
        rattle += mask * bp(rng.standard_normal(n), 900, 4600) * np.exp(-dt * 42)
        rattle += mask * resonator(rng.standard_normal(n) * np.exp(-dt * 35), 620, q=14) * 0.5
    save_wav("door_locked_rattle", simple_reverb(rattle, (19, 43), 0.22), peak=0.82, fade_ms=4)

    # Heavy door slam (Anomaly #19 / Entity trap)
    n = int(SR * 0.95)
    t = np.arange(n) / SR
    boom = np.sin(2 * np.pi * 56 * t * np.exp(-t * 7)) * np.exp(-t * 11) * 1.2
    crack = bp(rng.standard_normal(n), 200, 3800) * np.exp(-t * 24) * 0.9
    save_wav("door_slam_heavy", simple_reverb(boom + crack, (37, 79, 137), 0.46), peak=0.94, fade_ms=4)

    # Floor creaking (2 variants)
    for idx, base_f in [(1, 185), (2, 235)]:
        n = int(SR * 0.75)
        t = np.arange(n) / SR
        fm = base_f + 45 * np.sin(t * 11.0)
        pulses = (np.sin(np.cumsum(2 * np.pi * (28 + 14 * np.sin(t * 5)) / SR)) > 0.72).astype(float)
        raw = pulses * np.sin(np.cumsum(2 * np.pi * fm / SR))
        sig = resonator(raw, base_f * 2.1, q=8.0) * np.sin(np.pi * t / 0.75)
        save_wav(f"floor_creak_0{idx}", simple_reverb(sig, (23, 51), 0.24), peak=0.72)

    # Flashlight click
    n = int(SR * 0.11)
    t = np.arange(n) / SR
    c1 = bp(rng.standard_normal(n), 2000, 7500) * np.exp(-t * 120)
    c2 = bp(rng.standard_normal(n), 1500, 6000) * np.exp(-np.maximum(0, t - 0.028) * 110) * (t > 0.028) * 0.8
    save_wav("flashlight_click", c1 + c2, peak=0.78, fade_ms=2)

    # Scanner UI sounds
    n = int(SR * 0.35)
    t = np.arange(n) / SR
    ok = (np.sin(2 * np.pi * 523.25 * t) * (t < 0.14) + np.sin(2 * np.pi * 783.99 * t) * (t >= 0.12)) * np.exp(-t * 6.5)
    save_wav("scanner_confirm", simple_reverb(ok, (23, 47), 0.25), peak=0.70, fade_ms=5)

    err = (np.sin(2 * np.pi * 195 * t) + 0.5 * np.sin(2 * np.pi * 207 * t)) * np.exp(-t * 5.5)
    save_wav("scanner_error", lp(np.tanh(err * 2.2), 1400), peak=0.75, fade_ms=5)


def gen_anomaly_and_ambience_sounds(rng):
    # 1. Electrical fluorescent hum 60Hz loop (3.0s seamless-friendly)
    n = int(SR * 3.0)
    t = np.arange(n) / SR
    hum = (
        0.45 * np.sin(2 * np.pi * 60 * t)
        + 0.35 * np.sin(2 * np.pi * 120 * t)
        + 0.18 * np.sin(2 * np.pi * 180 * t)
        + 0.12 * np.sin(2 * np.pi * 300 * t)
        + 0.05 * bp(rng.standard_normal(n), 1800, 4500) * (0.5 + 0.5 * np.sin(2 * np.pi * 120 * t))
    )
    save_wav("fluorescent_hum_60hz", hum, peak=0.45, fade_ms=40)

    # 2. Knocking behind locked door (Anomaly #13: 3 urgent knocks on hollow wood)
    n = int(SR * 1.45)
    t = np.arange(n) / SR
    knocks = np.zeros(n)
    for onset, amp in [(0.05, 0.78), (0.31, 0.86), (0.60, 1.0)]:
        dt = np.maximum(0, t - onset)
        m = (t >= onset).astype(float)
        wood1 = np.sin(2 * np.pi * 128 * dt * np.exp(-dt * 8)) * np.exp(-dt * 26)
        wood2 = resonator(rng.standard_normal(n) * np.exp(-dt * 45), 340, q=7.0) * 0.55
        knocks += m * amp * (wood1 + wood2)
    save_wav("anom_knock_3x", simple_reverb(knocks, (29, 67, 113), 0.36), peak=0.90, fade_ms=8)

    # 3. Whispering from a room (Anomaly #12: eerie multi-voice formant aspiration)
    n = int(SR * 3.6)
    t = np.arange(n) / SR
    breath = rng.standard_normal(n)
    syll_env = np.clip(np.sin(2 * np.pi * 2.7 * t) + 0.5 * np.sin(2 * np.pi * 4.3 * t + 0.8), 0, 1.4)
    f1 = resonator(breath, 620, q=9.0) * (0.5 + 0.5 * np.sin(2 * np.pi * 1.1 * t))
    f2 = resonator(breath, 1780, q=11.0) * (0.5 + 0.5 * np.cos(2 * np.pi * 1.7 * t))
    sibil = bp(breath, 3800, 7200) * (np.sin(2 * np.pi * 3.9 * t) > 0.55) * 0.45
    whisper = (f1 * 0.5 + f2 * 0.6 + sibil) * syll_env
    save_wav("anom_whisper_loop", simple_reverb(whisper, (41, 89, 149), 0.45), peak=0.78, fade_ms=60)

    # 4. Radio interference (Anomaly #14: AM static + sweeping heterodyne whistle + Morse/numbers tones)
    n = int(SR * 3.4)
    t = np.arange(n) / SR
    static = bp(rng.standard_normal(n), 350, 3400) * (0.55 + 0.35 * np.sin(2 * np.pi * 5.2 * t))
    het = np.sin(np.cumsum(2 * np.pi * (980 + 440 * np.sin(t * 2.3)) / SR)) * 0.25
    tones = np.sin(2 * np.pi * 660 * t) * ((np.sin(2 * np.pi * 3.2 * t) > 0.4).astype(float)) * 0.28
    save_wav("anom_radio_static", np.tanh((static + het + tones) * 1.6), peak=0.76, fade_ms=40)

    # 5. Distant scream (Anomaly #15: muffled stairwell human-like shriek with heavy shaft reverb)
    n = int(SR * 2.4)
    t = np.arange(n) / SR
    f0 = 510 + 180 * np.sin(np.minimum(t * 2.4, np.pi)) - 95 * t + 14 * np.sin(2 * np.pi * 6.5 * t)
    ph = np.cumsum(2 * np.pi * f0 / SR)
    glottal = np.sin(ph) + 0.65 * np.sin(2 * ph) + 0.45 * np.sin(3 * ph) + 0.25 * rng.standard_normal(n)
    env = np.clip(t / 0.16, 0, 1) * np.exp(-np.maximum(0, t - 0.35) * 1.65)
    scream = lp(resonator(glottal * env, 1150, q=5.5), 1850)
    save_wav("anom_distant_scream", simple_reverb(scream, (59, 113, 197, 311), 0.62), peak=0.82, fade_ms=25)

    # 6. Elevator sounds (motor cable rumble + clean chime + warped Imitator chime)
    n = int(SR * 3.0)
    t = np.arange(n) / SR
    motor = (
        0.5 * np.sin(2 * np.pi * 48 * t)
        + 0.35 * np.sin(2 * np.pi * 96 * t)
        + 0.3 * bp(rng.standard_normal(n), 90, 420) * (0.7 + 0.3 * np.sin(2 * np.pi * 3.5 * t))
    )
    save_wav("elevator_move_loop", motor, peak=0.74, fade_ms=50)

    n_d = int(SR * 1.3)
    td = np.arange(n_d) / SR
    ding = (np.sin(2 * np.pi * 659.25 * td) + 0.35 * np.sin(2 * np.pi * 1318.5 * td) * np.exp(-td * 6)) * np.exp(-td * 2.8)
    save_wav("elevator_ding", simple_reverb(ding, (37, 83), 0.35), peak=0.80, fade_ms=5)

    # Warped off-pitch chime (Imitator / Anomaly #22)
    w_freq = 618.0 - 42.0 * td + 9.0 * np.sin(2 * np.pi * 8.0 * td)
    w_ding = np.sin(np.cumsum(2 * np.pi * w_freq / SR)) * np.exp(-td * 2.3)
    save_wav("elevator_ding_warped", simple_reverb(w_ding, (43, 97), 0.44), peak=0.80, fade_ms=5)

    # 7. Unexplained close breathing (Anomaly #18)
    n = int(SR * 3.8)
    t = np.arange(n) / SR
    cycle = np.mod(t, 1.9)
    inhale = np.exp(-((cycle - 0.45) ** 2) / 0.045)
    exhale = np.exp(-((cycle - 1.25) ** 2) / 0.085) * 0.85
    air = rng.standard_normal(n)
    br = resonator(air, 480, q=6.0) * inhale + resonator(air, 360, q=5.0) * exhale + bp(air, 1200, 3200) * (inhale + exhale) * 0.3
    save_wav("anom_breathing_close", br, peak=0.78, fade_ms=50)

    # 8. Boiler industrial rumble
    n = int(SR * 3.2)
    t = np.arange(n) / SR
    boiler = lp(rng.standard_normal(n), 180) * 0.7 + 0.35 * np.sin(2 * np.pi * 41 * t) + 0.2 * np.sin(2 * np.pi * 83 * t)
    save_wav("boiler_rumble", boiler, peak=0.68, fade_ms=50)

    # 9. Stereo Dark Psychological Ambience & Main Menu Theme
    for name, dur, root_hz in [("ambience_building", 6.0, 55.0), ("menu_theme", 8.0, 46.25)]:
        n = int(SR * dur)
        t = np.arange(n) / SR
        # Seamless periodic LFOs matching exact duration
        w_loop = 2 * np.pi / dur
        lfo1 = 0.5 + 0.5 * np.sin(w_loop * t)
        lfo2 = 0.5 + 0.5 * np.cos(w_loop * 2 * t)
        pad_l = (
            0.45 * np.sin(2 * np.pi * root_hz * t)
            + 0.30 * np.sin(2 * np.pi * (root_hz * 1.498) * t) * lfo1
            + 0.20 * np.sin(2 * np.pi * (root_hz * 2.08) * t) * lfo2
            + 0.08 * bp(rng.standard_normal(n), 140, 750)
        )
        pad_r = (
            0.45 * np.sin(2 * np.pi * (root_hz * 1.003) * t)
            + 0.30 * np.sin(2 * np.pi * (root_hz * 1.502) * t) * lfo2
            + 0.20 * np.sin(2 * np.pi * (root_hz * 2.99) * t) * lfo1
            + 0.08 * bp(rng.standard_normal(n), 140, 750)
        )
        stereo_arr = np.stack([pad_l, pad_r], axis=-1)
        save_wav(name, stereo_arr, peak=0.58, stereo=True, fade_ms=120)


def gen_entity_and_tape_sounds(rng):
    # Observer tension drone & spotted metallic sting
    n = int(SR * 2.5)
    t = np.arange(n) / SR
    obs_hum = 0.5 * np.sin(2 * np.pi * 43 * t) + 0.35 * np.sin(2 * np.pi * 61.5 * t) + 0.25 * bp(rng.standard_normal(n), 1800, 5000) * (np.sin(2 * np.pi * 17 * t) > 0.75)
    save_wav("entity_observer_hum", obs_hum, peak=0.72, fade_ms=40)

    n = int(SR * 1.35)
    t = np.arange(n) / SR
    sting = (
        np.sin(np.cumsum(2 * np.pi * (740 + 380 * np.exp(-t * 5)) / SR)) * 0.45
        + np.sin(np.cumsum(2 * np.pi * (787 + 410 * np.exp(-t * 5)) / SR)) * 0.45
        + np.sin(2 * np.pi * 52 * t) * 0.65
    ) * np.exp(-t * 2.4)
    save_wav("entity_observer_sting", simple_reverb(np.tanh(sting * 2.0), (31, 71, 131), 0.45), peak=0.90, fade_ms=5)

    # Listener bone-clicks & hunt shriek
    n = int(SR * 1.8)
    t = np.arange(n) / SR
    clicks = np.zeros(n)
    for onset in (0.04, 0.19, 0.31, 0.68, 0.81, 1.22, 1.35, 1.44):
        dt = np.maximum(0, t - onset)
        m = (t >= onset).astype(float)
        clicks += m * resonator(rng.standard_normal(n) * np.exp(-dt * 95), 1450 + (int(onset * 100) % 5) * 180, q=14)
    save_wav("entity_listener_clicks", simple_reverb(clicks, (23, 53), 0.30), peak=0.82, fade_ms=8)

    n = int(SR * 1.5)
    t = np.arange(n) / SR
    shriek_f = 380 + 520 * np.exp(-t * 2.8) + 45 * np.sin(2 * np.pi * 29 * t)
    shriek = np.tanh(2.5 * np.sin(np.cumsum(2 * np.pi * shriek_f / SR)) + rng.standard_normal(n) * 0.5) * np.exp(-t * 2.0)
    save_wav("entity_listener_shriek", simple_reverb(bp(shriek, 280, 4200), (29, 67, 113), 0.42), peak=0.90, fade_ms=6)

    # Imitator unfold rattle
    n = int(SR * 1.2)
    t = np.arange(n) / SR
    unfold = bp(rng.standard_normal(n), 500, 3800) * (0.5 + 0.5 * np.sin(2 * np.pi * 22 * t)) * np.exp(-t * 2.5)
    unfold += np.sin(np.cumsum(2 * np.pi * (210 + 190 * t) / SR)) * np.exp(-t * 3.0) * 0.5
    save_wav("entity_imitator_unfold", simple_reverb(unfold, (23, 59), 0.34), peak=0.84, fade_ms=6)

    # Heartbeat tension loop
    n = int(SR * 1.1)
    t = np.arange(n) / SR
    lub = np.sin(2 * np.pi * 52 * t * np.exp(-t * 8)) * np.exp(-t * 22)
    dt2 = np.maximum(0, t - 0.24)
    dub = (t >= 0.24) * np.sin(2 * np.pi * 46 * dt2 * np.exp(-dt2 * 9)) * np.exp(-dt2 * 24) * 0.8
    save_wav("heartbeat_tension", lub + dub, peak=0.82, fade_ms=8)

    # 4 Distorted Magnetic Tape Logs (distinct vocal formant contours + tape click + hiss + warble)
    for idx, pitch_hz in [(1, 118.0), (2, 176.0), (3, 102.0), (4, 142.0)]:
        dur = 4.6
        n = int(SR * dur)
        t = np.arange(n) / SR
        local_rng = np.random.default_rng(900 + idx)
        click_in = bp(local_rng.standard_normal(n), 900, 5000) * np.exp(-t * 75) * 0.9
        # Vocal glottal pulse train with wow/flutter & phrasing pauses
        f0 = pitch_hz * (1.0 + 0.04 * np.sin(2 * np.pi * 1.8 * t) + 0.018 * np.sin(2 * np.pi * 6.2 * t))
        ph = np.cumsum(2 * np.pi * f0 / SR)
        voice_src = np.tanh(2.2 * (np.sin(ph) + 0.55 * np.sin(2 * ph) + 0.35 * np.sin(3 * ph)))
        # Syllable envelope
        syll = np.clip(np.sin(2 * np.pi * (3.1 + idx * 0.35) * t) + 0.35 * np.cos(2 * np.pi * 1.3 * t), 0, 1.2)
        syll *= (t > 0.22) * (t < dur - 0.25)
        f1 = resonator(voice_src, 520 + idx * 55, q=6.0)
        f2 = resonator(voice_src, 1480 + idx * 90, q=8.0)
        hiss = bp(local_rng.standard_normal(n), 800, 4200) * 0.14
        click_out_t = np.maximum(0, t - (dur - 0.16))
        click_out = (t >= dur - 0.16) * bp(local_rng.standard_normal(n), 900, 5000) * np.exp(-click_out_t * 80) * 0.8
        tape = bp((f1 * 0.65 + f2 * 0.45) * syll, 280, 2900) + hiss + click_in + click_out
        save_wav(f"tape_log_0{idx}", tape, peak=0.80, fade_ms=10)


if __name__ == "__main__":
    rng = np.random.default_rng(20261025)
    gen_footsteps(rng)
    gen_doors_and_props(rng)
    gen_anomaly_and_ambience_sounds(rng)
    gen_entity_and_tape_sounds(rng)
    files = sorted(os.listdir(AUD_DIR))
    print(f"Generated {len(files)} WAV audio assets in {AUD_DIR}")
