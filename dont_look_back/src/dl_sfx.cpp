// ============================================================================
//  dl_sfx.cpp : procedural sound design (no external audio assets)
// ============================================================================
#include "dl_game.h"

namespace dl {
using namespace nf;

namespace {

struct Synth {
    std::vector<float> buf;
    int rate = 44100;
    Rng rng{0xBEEF1234u};
    explicit Synth(float seconds, uint32_t seed = 0xBEEF1234u) {
        buf.assign((size_t)(44100 * seconds), 0.0f);
        rng = Rng(seed);
    }
    void add(int i, float v) {
        if (i >= 0 && i < (int)buf.size()) buf[i] += v;
    }
    // simple one-pole filters
    float lp(float in, float& state, float a) { state = state + a * (in - state); return state; }
    float hp(float in, float& state, float a) { state = state + a * (in - state); return in - state; }
    void normalize(float peak = 0.85f) {
        float m = 0.0f;
        for (float v : buf) m = maxf(m, std::fabs(v));
        if (m > 1e-5f) {
            float g = peak / m;
            for (float& v : buf) v *= g;
        }
    }
    void fadeOutTail(float seconds) {
        int n = (int)(seconds * rate);
        int start = (int)buf.size() - n;
        for (int i = start; i < (int)buf.size(); ++i)
            if (i >= 0) buf[i] *= 1.0f - (float)(i - start) / n;
    }
};

float envAD(int i, int n, float attack, float decay, int rate) {
    float t = (float)i / rate;
    float a = (float)(attack * rate);
    float d = (float)(decay * rate);
    if (i < a) return i / maxf(1.0f, a);
    return std::exp(-(i - a) / maxf(1.0f, d));
}

std::vector<float> footstepSound(uint32_t seed, bool heel) {
    Synth s(0.32f, seed);
    float lpst = 0, hpst = 0, lp2 = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float n = s.rng.range(-1, 1);
        // low thump
        float thump = std::sin(TAU * (heel ? 78.0f : 96.0f) * t * (1.0f - t * 1.6f)) *
                      std::exp(-t * 34.0f) * 0.55f;
        // scuff noise
        float sc = s.lp(n, lpst, 0.22f);
        sc = s.hp(sc, hpst, 0.05f);
        sc *= std::exp(-t * (heel ? 26.0f : 34.0f)) * 0.42f;
        // floor resonance
        float res = s.lp(sc + thump, lp2, 0.5f);
        s.buf[i] = (thump + sc) * 0.7f + res * 0.35f;
    }
    s.normalize(0.7f);
    s.fadeOutTail(0.04f);
    return s.buf;
}

std::vector<float> creakSound(uint32_t seed, float dur, float f0, float f1) {
    Synth s(dur, seed);
    float ph = 0;
    float lpst = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float u = t / dur;
        float f = lerpf(f0, f1, u) * (1.0f + 0.03f * std::sin(TAU * 7.0f * t));
        ph += TAU * f / s.rate;
        // stick-slip: modulated resonant pulse train
        float pul = std::fmod(ph, TAU);
        float pulse = std::exp(-pul * 3.0f) * 2.0f - 0.5f;
        float noise = s.rng.range(-1, 1) * 0.16f;
        float v = (std::sin(ph) * 0.25f + pulse * 0.35f + noise);
        v = s.lp(v, lpst, 0.35f);
        float env = std::sin(PI * u) * (0.6f + 0.4f * std::sin(TAU * 3.0f * t));
        s.buf[i] += v * env * 0.6f;
    }
    s.normalize(0.55f);
    s.fadeOutTail(0.08f);
    return s.buf;
}

std::vector<float> knockSound(uint32_t seed, int hits, float gap) {
    Synth s(0.35f * hits + 0.4f, seed);
    for (int h = 0; h < hits; ++h) {
        int start = (int)(h * gap * s.rate);
        float lpst = 0, lp2 = 0;
        for (int i = start; i < (int)s.buf.size() && i < start + s.rate / 2; ++i) {
            float t = (float)(i - start) / s.rate;
            float n = s.rng.range(-1, 1) * std::exp(-t * 200.0f) * 0.4f;
            float body = std::sin(TAU * 68.0f * t) * std::exp(-t * 26.0f);
            float wood = std::sin(TAU * 220.0f * t) * std::exp(-t * 60.0f) * 0.3f;
            float v = body * 0.7f + wood + n;
            v = s.lp(v, lp2, 0.4f);
            s.buf[i] += v * 0.8f;
        }
    }
    s.normalize(0.75f);
    s.fadeOutTail(0.15f);
    return s.buf;
}

std::vector<float> whisperFn(uint32_t seed) {
    Synth s(2.6f, seed);
    float lp1 = 0, lp2 = 0, hp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float n = s.rng.range(-1, 1);
        // formant-ish band pass on noise, syllable envelope
        float syl = 0.5f + 0.5f * std::sin(TAU * 3.4f * t + std::sin(TAU * 0.6f * t) * 2.0f);
        float f1 = 700.0f + 500.0f * std::sin(TAU * 1.1f * t);
        float mod = 0.5f + 0.5f * std::sin(TAU * f1 * 0.0005f * t);
        float v = s.lp(n, lp1, 0.10f + mod * 0.10f);
        v = s.hp(v, hp, 0.02f);
        v = s.lp(v, lp2, 0.35f);
        float env = std::pow(saturate(syl * 1.15f), 2.0f) * (0.4f + 0.6f * std::sin(PI * t / 2.6f));
        s.buf[i] += v * env * 1.6f;
    }
    s.normalize(0.5f);
    s.fadeOutTail(0.3f);
    return s.buf;
}

std::vector<float> staticSound(uint32_t seed) {
    Synth s(2.2f, seed);
    float lp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float n = s.rng.range(-1, 1);
        float crackle = (s.rng.f() > 0.9993f) ? s.rng.range(-1, 1) * 3.0f : 0.0f;
        float tone = std::sin(TAU * 440.0f * t) * 0.05f * (0.5f + 0.5f * std::sin(TAU * 0.7f * t));
        float v = s.lp(n + crackle, lp, 0.6f) * (0.35f + 0.3f * std::sin(TAU * 0.3f * t)) + tone;
        s.buf[i] = v * 0.5f;
    }
    s.normalize(0.45f);
    return s.buf;
}

std::vector<float> screamFn(uint32_t seed) {
    Synth s(2.0f, seed);
    float ph = 0, ph2 = 0, lp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float u = saturate(t / 1.6f);
        float f = lerpf(320.0f, 780.0f, u) * (1.0f + 0.05f * std::sin(TAU * 6.0f * t));
        ph += TAU * f / s.rate;
        ph2 += TAU * f * 1.51f / s.rate;
        float saw = 2.0f * (ph / TAU - std::floor(ph / TAU + 0.5f));
        float saw2 = 2.0f * (ph2 / TAU - std::floor(ph2 / TAU + 0.5f));
        float noise = s.rng.range(-1, 1) * 0.22f;
        float v = saw * 0.4f + saw2 * 0.2f + noise;
        v = s.lp(v, lp, 0.45f);
        float env = std::sin(PI * saturate(t / 1.85f)) * (0.7f + 0.3f * std::sin(TAU * 11.0f * t));
        s.buf[i] = v * env * 0.8f;
    }
    s.normalize(0.7f);
    s.fadeOutTail(0.2f);
    return s.buf;
}

std::vector<float> heartbeatFn(uint32_t seed) {
    Synth s(1.1f, seed);
    float lp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float b1 = std::sin(TAU * 46.0f * t) * std::exp(-t * 16.0f);
        float t2 = maxf(0.0f, t - 0.24f);
        float b2 = std::sin(TAU * 38.0f * t2) * std::exp(-t2 * 20.0f) * (t > 0.24f ? 0.65f : 0.0f);
        float v = (b1 + b2) * 0.9f;
        s.buf[i] = s.lp(v, lp, 0.25f) * 1.2f;
    }
    s.normalize(0.8f);
    return s.buf;
}

std::vector<float> droneFn(uint32_t seed, float dur, float base) {
    Synth s(dur, seed);
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float lfo = 0.6f + 0.4f * std::sin(TAU * 0.05f * t);
        float v = std::sin(TAU * base * t) * 0.35f
                + std::sin(TAU * base * 1.498f * t) * 0.22f
                + std::sin(TAU * base * 2.003f * t) * 0.12f
                + std::sin(TAU * base * 0.5f * t) * 0.18f;
        v *= lfo;
        // slow noise breathing
        v += s.rng.range(-1, 1) * 0.02f;
        float fade = saturate(t / 1.5f) * saturate((dur - t) / 1.5f);
        s.buf[i] = v * fade * 0.7f;
    }
    s.normalize(0.6f);
    return s.buf;
}

std::vector<float> breathSound(uint32_t seed, bool exhale) {
    Synth s(1.5f, seed);
    float lp = 0, lp2 = 0, hp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float u = t / 1.5f;
        float n = s.rng.range(-1, 1);
        float v = s.lp(n, lp, exhale ? 0.18f : 0.30f);
        v = s.hp(v, hp, 0.015f);
        v = s.lp(v, lp2, 0.6f);
        float env = std::pow(std::sin(PI * u), 1.6f);
        // rasp
        env *= 0.75f + 0.25f * std::sin(TAU * 42.0f * t);
        s.buf[i] = v * env * 2.2f;
    }
    s.normalize(0.45f);
    return s.buf;
}

std::vector<float> shriekSound(uint32_t seed) {
    Synth s(1.2f, seed);
    float ph = 0, lp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float f = 900.0f * std::exp(-t * 1.2f) + 120.0f;
        f *= 1.0f + 0.3f * std::sin(TAU * 30.0f * t);
        ph += TAU * f / s.rate;
        float v = std::sin(ph) * 0.5f + s.rng.range(-1, 1) * 0.5f;
        v = s.lp(v, lp, 0.55f);
        s.buf[i] = v * std::exp(-t * 3.2f) * 1.2f;
    }
    s.normalize(0.8f);
    s.fadeOutTail(0.1f);
    return s.buf;
}

std::vector<float> musicBoxFn(uint32_t seed) {
    // slow minor-key bell melody, 8 bars
    Synth s(16.0f, seed);
    const float notes[16] = {440.0f, 523.25f, 659.25f, 523.25f, 392.0f, 440.0f, 493.88f, 440.0f,
                             349.23f, 440.0f, 523.25f, 440.0f, 329.63f, 392.0f, 415.30f, 392.0f};
    for (int n = 0; n < 16; ++n) {
        int start = (int)(n * 0.98f * s.rate);
        float f = notes[n];
        for (int i = start; i < (int)s.buf.size() && i < start + s.rate * 2; ++i) {
            float t = (float)(i - start) / s.rate;
            float v = std::sin(TAU * f * t) * 0.5f + std::sin(TAU * f * 2.0f * t) * 0.16f
                    + std::sin(TAU * f * 3.01f * t) * 0.06f;
            s.buf[i] += v * std::exp(-t * 1.6f) * 0.25f;
        }
    }
    s.normalize(0.45f);
    s.fadeOutTail(0.5f);
    return s.buf;
}

std::vector<float> metalDragSound(uint32_t seed) {
    Synth s(1.4f, seed);
    float lp = 0, lp2 = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float n = s.rng.range(-1, 1);
        float ring = std::sin(TAU * 1800.0f * t) * std::sin(TAU * 3.0f * t) * 0.15f
                   + std::sin(TAU * 2650.0f * t) * 0.10f;
        float v = s.lp(n, lp, 0.6f) * 0.5f + ring;
        v = s.lp(v, lp2, 0.7f);
        float env = std::sin(PI * saturate(t / 1.35f)) * (0.6f + 0.4f * s.rng.f());
        s.buf[i] = v * env;
    }
    s.normalize(0.5f);
    return s.buf;
}

std::vector<float> glassBreakSound(uint32_t seed) {
    Synth s(1.3f, seed);
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float n = s.rng.range(-1, 1) * std::exp(-t * 12.0f);
        float shard = 0;
        for (int k = 0; k < 5; ++k) {
            float f = 2200.0f + k * 900.0f + s.rng.range(-100, 100);
            shard += std::sin(TAU * f * t) * std::exp(-t * (18.0f + k * 5.0f)) * 0.09f;
        }
        s.buf[i] = n * 0.55f + shard;
    }
    s.normalize(0.7f);
    s.fadeOutTail(0.2f);
    return s.buf;
}

std::vector<float> pipeBangSound(uint32_t seed) {
    Synth s(1.1f, seed);
    float lp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float v = 0;
        for (int k = 0; k < 4; ++k) {
            float f = 110.0f * (k + 1) * 1.6f;
            v += std::sin(TAU * f * t) * std::exp(-t * (7.0f + k * 3.0f)) * (0.4f / (k + 1));
        }
        v += s.rng.range(-1, 1) * std::exp(-t * 90.0f) * 0.4f;
        s.buf[i] = s.lp(v, lp, 0.3f);
    }
    s.normalize(0.8f);
    s.fadeOutTail(0.3f);
    return s.buf;
}

std::vector<float> waterDripSound(uint32_t seed) {
    Synth s(0.8f, seed);
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float f = lerpf(1600.0f, 600.0f, saturate(t / 0.15f));
        float v = std::sin(TAU * f * t) * std::exp(-t * 40.0f);
        float t2 = maxf(0.0f, t - 0.32f);
        v += std::sin(TAU * 1100.0f * t2) * std::exp(-t2 * 55.0f) * (t > 0.32f ? 0.5f : 0.0f);
        s.buf[i] = v;
    }
    s.normalize(0.4f);
    return s.buf;
}

std::vector<float> buzzLoopSound(uint32_t seed) {
    Synth s(2.0f, seed);
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float v = std::sin(TAU * 100.0f * t) * 0.35f + std::sin(TAU * 200.0f * t) * 0.18f
                + std::sin(TAU * 301.0f * t) * 0.08f;
        v *= 0.8f + 0.2f * std::sin(TAU * 7.0f * t);
        float fade = saturate(t / 0.2f) * saturate((2.0f - t) / 0.2f);
        s.buf[i] = v * fade;
    }
    s.normalize(0.35f);
    return s.buf;
}

std::vector<float> bellSoundFn(uint32_t seed, float f) {
    Synth s(2.4f, seed);
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float v = std::sin(TAU * f * t) * 0.6f + std::sin(TAU * f * 2.76f * t) * 0.25f
                + std::sin(TAU * f * 5.4f * t) * 0.12f;
        s.buf[i] = v * std::exp(-t * 2.2f);
    }
    s.normalize(0.6f);
    s.fadeOutTail(0.4f);
    return s.buf;
}

std::vector<float> chimeFn(uint32_t seed, bool good) {
    Synth s(1.6f, seed);
    float f1 = good ? 660.0f : 220.0f;
    float f2 = good ? 990.0f : 196.0f;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float a = std::sin(TAU * f1 * t) * std::exp(-t * 3.0f);
        float b = std::sin(TAU * f2 * t) * std::exp(-t * 4.5f) * 0.6f;
        s.buf[i] = (a + b) * 0.5f;
    }
    s.normalize(0.55f);
    s.fadeOutTail(0.2f);
    return s.buf;
}

std::vector<float> whooshSound(uint32_t seed, float dur) {
    Synth s(dur, seed);
    float lp = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float u = t / dur;
        float n = s.rng.range(-1, 1);
        float v = s.lp(n, lp, 0.05f + 0.5f * std::sin(PI * u));
        float env = std::sin(PI * u);
        s.buf[i] = v * env * 1.8f;
    }
    s.normalize(0.5f);
    return s.buf;
}

std::vector<float> windLoopSound(uint32_t seed) {
    Synth s(8.0f, seed);
    float lp = 0, lp2 = 0;
    for (int i = 0; i < (int)s.buf.size(); ++i) {
        float t = (float)i / s.rate;
        float n = s.rng.range(-1, 1);
        float mod = 0.5f + 0.5f * std::sin(TAU * 0.13f * t) * std::sin(TAU * 0.31f * t);
        float v = s.lp(n, lp, 0.02f + mod * 0.04f);
        v = s.lp(v, lp2, 0.4f);
        float fade = saturate(t / 2.0f) * saturate((8.0f - t) / 2.0f);
        s.buf[i] = v * fade * 4.0f;
    }
    s.normalize(0.32f);
    return s.buf;
}

} // namespace

void SoundBank::build(nf::Audio& a) {
    auto add = [&](const std::vector<float>& v) { return a.addSamples(v, 44100); };
    for (int i = 0; i < 6; ++i) step[i] = add(footstepSound(1000 + i * 77, i % 2 == 0));
    for (int i = 0; i < 4; ++i) stepRun[i] = add(footstepSound(2000 + i * 91, false));
    doorCreak = add(creakSound(3001, 1.5f, 90.0f, 240.0f));
    doorSlam = add(pipeBangSound(3002));
    doorLock = add(knockSound(3003, 2, 0.08f));
    keyUnlock = add(knockSound(3004, 3, 0.05f));
    for (int i = 0; i < 3; ++i) whisper[i] = add(whisperFn(4000 + i * 13));
    knock[0] = add(knockSound(4100, 3, 0.55f));
    knock[1] = add(knockSound(4101, 2, 0.8f));
    knock[2] = add(knockSound(4102, 4, 0.35f));
    radioStatic = add(staticSound(4200));
    scream = add(screamFn(4300));
    elevatorHum = add(droneFn(4400, 4.0f, 55.0f));
    elevatorDing = add(bellSoundFn(4410, 880.0f));
    footstepsBehind = add(footstepSound(4500, false));
    breathing = add(breathSound(4600, true));
    heartbeat = add(heartbeatFn(4700));
    entityBreath = add(breathSound(4800, false));
    entityShriek = add(shriekSound(4900));
    pickup = add(chimeFn(5000, true));
    paper = add(glassBreakSound(5010));
    uiMove = add(chimeFn(5100, true));
    uiSelect = add(chimeFn(5110, true));
    uiBack = add(chimeFn(5120, false));
    reportGood = add(chimeFn(5200, true));
    reportBad = add(chimeFn(5210, false));
    drone = add(droneFn(5300, 12.0f, 41.0f));
    rumble = add(droneFn(5310, 8.0f, 28.0f));
    waterDrip = add(waterDripSound(5400));
    pipeBang = add(pipeBangSound(5410));
    electricity = add(buzzLoopSound(5500));
    musicBox = add(musicBoxFn(5600));
    windOutside = add(windLoopSound(5700));
    metalDrag = add(metalDragSound(5800));
    glassBreak = add(glassBreakSound(5810));
    lightBuzz = add(buzzLoopSound(5900));
    whoosh = add(whooshSound(6000, 1.2f));
    stingAnomaly = add(bellSoundFn(6100, 120.0f));
    stingEntity = add(shriekSound(6200));
    endingWhoosh = add(whooshSound(6300, 2.4f));
}

} // namespace dl
