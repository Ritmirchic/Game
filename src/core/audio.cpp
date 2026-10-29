// FRACTURE — fully procedural audio engine. Every sound is synthesised at start-up from
// oscillators, filtered noise and envelopes; mixed in 3D and streamed through WinMM.
#include "core/engine.h"
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

const int SR = 44100;

struct Clip {
    std::vector<float> samples;   // mono
    float baseGain = 1.0f;
    bool loopable = false;
};
Clip g_clips[SFX_COUNT];

struct Voice {
    int sfx = -1;
    bool active = false;
    float pos = 0;        // sample position (fractional)
    float rate = 1.0f;
    float gainL = 1, gainR = 1;
    float baseGain = 1;
    bool loop = false;
    int loopId = -1;
    float lpL = 0, lpR = 0;   // simple distance low-pass state
    float lpCoef = 1.0f;
};
const int MAX_VOICES = 48;
Voice g_voices[MAX_VOICES];

vec3 g_listenerPos(0, 0, 0), g_listenerFwd(0, 0, 1), g_listenerUp(0, 1, 0);
float g_master = 0.85f, g_sfx = 1.0f, g_music = 0.55f;
float g_musicIntensity = 0.5f;
int g_musicTrack = -1;
double g_musicPhase = 0;

// ---------------------------------------------------------------- synthesis helpers
struct Noise {
    uint32_t s;
    explicit Noise(uint32_t seed = 12345) : s(seed) {}
    float next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return ((int32_t)s) * (1.0f / 2147483648.0f);
    }
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void lowpass(float freq, float q) {
        float w0 = 2 * PI * freq / SR;
        float alpha = std::sin(w0) / (2 * q);
        float c = std::cos(w0);
        float a0 = 1 + alpha;
        b0 = (1 - c) * 0.5f / a0; b1 = (1 - c) / a0; b2 = b0;
        a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
    }
    void highpass(float freq, float q) {
        float w0 = 2 * PI * freq / SR;
        float alpha = std::sin(w0) / (2 * q);
        float c = std::cos(w0);
        float a0 = 1 + alpha;
        b0 = (1 + c) * 0.5f / a0; b1 = -(1 + c) / a0; b2 = b0;
        a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
    }
    void bandpass(float freq, float q) {
        float w0 = 2 * PI * freq / SR;
        float alpha = std::sin(w0) / (2 * q);
        float a0 = 1 + alpha;
        b0 = alpha / a0; b1 = 0; b2 = -alpha / a0;
        a1 = -2 * std::cos(w0) / a0; a2 = (1 - alpha) / a0;
    }
    float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

static float envExp(float t, float attack, float decay) {
    if (t < attack) return t / std::max(0.0001f, attack);
    return std::exp(-(t - attack) / std::max(0.0001f, decay));
}
static float envAD(float t, float attack, float hold, float decay) {
    if (t < attack) return t / std::max(0.0001f, attack);
    if (t < attack + hold) return 1.0f;
    float d = (t - attack - hold) / std::max(0.0001f, decay);
    return std::exp(-d * 4.0f);
}
static void normalise(std::vector<float>& v, float peak = 0.92f) {
    float m = 0;
    for (float x : v) m = std::max(m, std::fabs(x));
    if (m < 1e-6f) return;
    float k = peak / m;
    for (float& x : v) x *= k;
}
static void fadeEdges(std::vector<float>& v, int ms = 4) {
    int n = std::min((int)v.size() / 2, SR * ms / 1000);
    for (int i = 0; i < n; i++) {
        float k = (float)i / n;
        v[i] *= k;
        v[v.size() - 1 - i] *= k;
    }
}

// ---------------------------------------------------------------- clip generators
static void genImpact(int id, float dur, float lowFreq, float noiseAmt, float ring, float decay, uint32_t seed) {
    int n = (int)(dur * SR);
    std::vector<float> out(n, 0);
    Noise nz(seed);
    Biquad bp, lp;
    bp.bandpass(std::min(18000.0f, lowFreq * 3.2f), 1.2f);
    lp.lowpass(std::min(18000.0f, lowFreq * 1.6f), 0.8f);
    float phase = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float e = envExp(t, 0.0012f, decay);
        float tone = std::sin(phase) * std::exp(-t * (1.0f / std::max(0.01f, decay * 0.5f)));
        phase += 2 * PI * lowFreq * (1.0f + 2.0f * std::exp(-t * 30)) / SR;
        float noise = lp.process(nz.next()) * noiseAmt + bp.process(nz.next()) * noiseAmt * 0.7f;
        out[i] = (tone * (1.0f - noiseAmt * 0.5f) + noise) * e;
    }
    // add a resonant ring for metal
    if (ring > 0) {
        for (int k = 0; k < 4; k++) {
            float f = lowFreq * (1.7f + k * 0.83f);
            float ph = 0;
            float amp = ring / (1 + k);
            for (int i = 0; i < n; i++) {
                float t = (float)i / SR;
                out[i] += std::sin(ph) * amp * std::exp(-t * (5.0f + k * 3.0f));
                ph += 2 * PI * f / SR;
            }
        }
    }
    normalise(out, 0.9f);
    fadeEdges(out);
    g_clips[id].samples = std::move(out);
}

static void genBreak(int id, float dur, float brightness, int pieces, float bodyFreq, uint32_t seed) {
    int n = (int)(dur * SR);
    std::vector<float> out(n, 0);
    Noise nz(seed);
    Biquad hp;
    hp.highpass(brightness, 0.7f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float e = envExp(t, 0.002f, dur * 0.34f);
        out[i] = hp.process(nz.next()) * e * 0.8f;
    }
    // discrete debris impacts
    for (int p = 0; p < pieces; p++) {
        float start = (float)(p * dur / pieces) * (0.4f + 0.6f * (float)((p * 37) % 11) / 11.0f);
        float freq = bodyFreq * (0.6f + 1.2f * ((p * 53) % 13) / 13.0f);
        int s0 = (int)(start * SR);
        int len = std::min(n - s0, (int)(0.09f * SR));
        if (len <= 0) continue;
        float ph = 0;
        for (int i = 0; i < len; i++) {
            float t = (float)i / SR;
            float e = std::exp(-t * 45.0f);
            out[s0 + i] += std::sin(ph) * e * 0.55f;
            ph += 2 * PI * freq / SR;
        }
    }
    normalise(out, 0.92f);
    fadeEdges(out);
    g_clips[id].samples = std::move(out);
}

static void genExplosion(int id, float dur, float size, uint32_t seed) {
    int n = (int)(dur * SR);
    std::vector<float> out(n, 0);
    Noise nz(seed);
    Biquad lp1, lp2;
    lp1.lowpass(220 * size, 0.9f);
    lp2.lowpass(900 * size, 0.6f);
    float ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float e = envExp(t, 0.004f, dur * 0.28f);
        float boom = std::sin(ph) * envExp(t, 0.01f, dur * 0.18f);
        ph += 2 * PI * (58.0f * size * std::exp(-t * 2.2f) + 28.0f) / SR;
        float n1 = lp1.process(nz.next());
        float n2 = lp2.process(nz.next());
        out[i] = (boom * 0.9f + n1 * 1.4f + n2 * 0.6f) * e;
        // crackle
        if (nz.next() > 0.9985f) out[i] += (nz.next()) * 0.5f * std::exp(-t * 1.5f);
    }
    normalise(out, 0.98f);
    fadeEdges(out, 6);
    g_clips[id].samples = std::move(out);
}

static void genLoopNoise(int id, float dur, float freq, float q, bool band, uint32_t seed, float crackle) {
    int n = (int)(dur * SR);
    std::vector<float> out(n, 0);
    Noise nz(seed);
    Biquad f;
    if (band) f.bandpass(freq, q); else f.lowpass(freq, q);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float v = f.process(nz.next());
        if (crackle > 0 && nz.next() > 1.0f - crackle) v += nz.next() * 0.6f;
        // slow amplitude modulation for movement
        v *= 0.75f + 0.25f * std::sin(t * 2 * PI * 0.7f);
        out[i] = v;
    }
    // make it seamlessly loopable
    int fade = SR / 20;
    for (int i = 0; i < fade; i++) {
        float k = (float)i / fade;
        out[i] = out[i] * k + out[n - fade + i] * (1 - k);
    }
    out.resize(n - fade);
    normalise(out, 0.8f);
    g_clips[id].samples = std::move(out);
    g_clips[id].loopable = true;
}

static void genTone(int id, float dur, float f0, float f1, float attack, float decay, int harmonics = 1, float noiseAmt = 0) {
    int n = (int)(dur * SR);
    std::vector<float> out(n, 0);
    Noise nz(91 + id * 7);
    float ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float f = f0 + (f1 - f0) * (t / dur);
        float e = envAD(t, attack, 0.0f, decay);
        float s = 0;
        for (int hh = 1; hh <= harmonics; hh++) s += std::sin(ph * hh) / hh;
        ph += 2 * PI * f / SR;
        out[i] = (s * (1 - noiseAmt) + nz.next() * noiseAmt) * e;
    }
    normalise(out, 0.85f);
    fadeEdges(out);
    g_clips[id].samples = std::move(out);
}

// metallic screech / creak
static void genCreak(int id, float dur, float base, uint32_t seed) {
    int n = (int)(dur * SR);
    std::vector<float> out(n, 0);
    Noise nz(seed);
    Biquad bp;
    bp.bandpass(base, 8.0f);
    float ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float mod = 0.5f + 0.5f * std::sin(t * 2 * PI * 3.1f + std::sin(t * 17));
        float e = envAD(t, 0.06f, dur * 0.4f, dur * 0.5f);
        out[i] = (bp.process(nz.next()) * 1.2f + std::sin(ph) * 0.18f) * e * mod;
        ph += 2 * PI * base * (0.96f + 0.08f * std::sin(t * 9.0f)) / SR;
    }
    normalise(out, 0.8f);
    fadeEdges(out, 8);
    g_clips[id].samples = std::move(out);
}

static void genVehicleLoop(int id, float f0, float f1, float dur, uint32_t seed) {
    int n = (int)(dur * SR);
    std::vector<float> out(n, 0);
    Noise nz(seed);
    Biquad lp;
    lp.lowpass(320, 0.9f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float rpm = 0.5f + 0.5f * std::sin(t * 2 * PI * 0.35f);
        float f = f0 + (f1 - f0) * rpm;
        float s = 0;
        for (int hh = 1; hh <= 6; hh++) s += std::sin(2 * PI * f * hh * t) / (hh * 1.6f);
        out[i] = s * 0.7f + lp.process(nz.next()) * 0.5f;
    }
    int fade = SR / 10;
    for (int i = 0; i < fade; i++) {
        float k = (float)i / fade;
        out[i] = out[i] * k + out[n - fade + i] * (1 - k);
    }
    out.resize(n - fade);
    normalise(out, 0.78f);
    g_clips[id].samples = std::move(out);
    g_clips[id].loopable = true;
}

static void buildAll() {
    for (int i = 0; i < SFX_COUNT; i++) g_clips[i].samples.clear();
    genImpact(SFX_HAMMER_CONCRETE, 0.34f, 210, 0.85f, 0.15f, 0.09f, 11);
    genImpact(SFX_HAMMER_BRICK, 0.30f, 320, 0.9f, 0.1f, 0.07f, 12);
    genImpact(SFX_HAMMER_WOOD, 0.26f, 160, 0.6f, 0.25f, 0.06f, 13);
    genImpact(SFX_HAMMER_METAL, 0.55f, 420, 0.35f, 0.9f, 0.16f, 14);
    genImpact(SFX_HAMMER_GLASS, 0.24f, 1400, 1.0f, 0.3f, 0.05f, 15);
    genBreak(SFX_GLASS_BREAK, 0.85f, 3200, 16, 2600, 21);
    genBreak(SFX_CONCRETE_BREAK, 1.10f, 700, 10, 180, 22);
    genBreak(SFX_WOOD_BREAK, 0.70f, 1400, 9, 260, 23);
    genBreak(SFX_METAL_BREAK, 0.95f, 1800, 12, 520, 24);
    genImpact(SFX_DEBRIS_SMALL, 0.18f, 380, 1.0f, 0.05f, 0.035f, 25);
    genImpact(SFX_DEBRIS_BIG, 0.75f, 120, 0.95f, 0.1f, 0.24f, 26);
    genExplosion(SFX_EXPLOSION_SMALL, 1.1f, 1.0f, 31);
    genExplosion(SFX_EXPLOSION_LARGE, 2.6f, 1.7f, 32);
    genTone(SFX_ROCKET_FIRE, 0.55f, 220, 90, 0.01f, 0.22f, 3, 0.65f);
    genLoopNoise(SFX_ROCKET_FLY, 0.5f, 1400, 1.4f, true, 33, 0.0f);
    // shotgun: sharp crack + tail
    {
        int n = (int)(0.6f * SR);
        std::vector<float> out(n, 0);
        Noise nz(41);
        Biquad hp, lp;
        hp.highpass(600, 0.8f);
        lp.lowpass(1400, 0.7f);
        for (int i = 0; i < n; i++) {
            float t = (float)i / SR;
            float e = envExp(t, 0.0008f, 0.10f);
            float tail = envExp(t, 0.02f, 0.22f) * 0.3f;
            out[i] = hp.process(nz.next()) * e + lp.process(nz.next()) * tail;
        }
        normalise(out, 0.98f);
        fadeEdges(out);
        g_clips[SFX_SHOTGUN].samples = std::move(out);
    }
    genTone(SFX_SHOTGUN_PUMP, 0.28f, 420, 220, 0.005f, 0.06f, 2, 0.55f);
    genTone(SFX_CHARGE_BEEP, 0.12f, 1700, 1700, 0.004f, 0.03f, 2, 0.0f);
    genTone(SFX_DETONATE, 0.22f, 900, 1400, 0.002f, 0.05f, 3, 0.1f);
    genLoopNoise(SFX_TORCH, 0.6f, 2600, 1.1f, true, 51, 0.02f);
    genLoopNoise(SFX_DRILL, 0.5f, 900, 4.0f, true, 52, 0.0f);
    {
        // drill: motor hum + gravel crunch
        std::vector<float> out((int)(0.5f * SR), 0);
        Noise nz(53);
        Biquad lp;
        lp.lowpass(1800, 0.7f);
        for (size_t i = 0; i < out.size(); i++) {
            float t = (float)i / SR;
            float hum = std::sin(2 * PI * 118 * t) * 0.5f + std::sin(2 * PI * 236 * t) * 0.25f;
            float grind = lp.process(nz.next()) * (0.6f + 0.4f * std::sin(t * 2 * PI * 22));
            out[i] = hum * 0.55f + grind * 0.8f;
        }
        normalise(out, 0.8f);
        g_clips[SFX_DRILL].samples = std::move(out);
        g_clips[SFX_DRILL].loopable = true;
    }
    genLoopNoise(SFX_CUTTER, 0.45f, 2200, 6.0f, true, 54, 0.0f);
    genLoopNoise(SFX_EXTINGUISH, 0.7f, 1200, 0.5f, false, 55, 0.0f);
    genTone(SFX_GRAPPLE, 0.35f, 700, 1600, 0.005f, 0.12f, 2, 0.35f);
    genImpact(SFX_FOOTSTEP_CONCRETE, 0.14f, 150, 0.9f, 0.02f, 0.03f, 61);
    genImpact(SFX_FOOTSTEP_WOOD, 0.16f, 110, 0.7f, 0.15f, 0.035f, 62);
    genImpact(SFX_FOOTSTEP_METAL, 0.22f, 260, 0.4f, 0.5f, 0.05f, 63);
    genImpact(SFX_FOOTSTEP_DIRT, 0.15f, 90, 1.0f, 0.0f, 0.03f, 64);
    genImpact(SFX_FOOTSTEP_GLASS, 0.20f, 2200, 1.0f, 0.4f, 0.04f, 65);
    genTone(SFX_JUMP, 0.12f, 320, 420, 0.004f, 0.03f, 1, 0.25f);
    genImpact(SFX_LAND, 0.22f, 120, 0.9f, 0.0f, 0.05f, 66);
    genTone(SFX_UI_CLICK, 0.10f, 900, 1250, 0.002f, 0.03f, 2, 0.0f);
    genTone(SFX_UI_HOVER, 0.06f, 1400, 1500, 0.002f, 0.02f, 1, 0.0f);
    genTone(SFX_UI_BACK, 0.12f, 700, 480, 0.002f, 0.04f, 2, 0.0f);
    // construction / plank
    {
        int n = (int)(0.45f * SR);
        std::vector<float> out(n, 0);
        Noise nz(71);
        Biquad bp;
        bp.bandpass(700, 3.0f);
        for (int i = 0; i < n; i++) {
            float t = (float)i / SR;
            float e = envExp(t, 0.01f, 0.09f);
            out[i] = (std::sin(2 * PI * 190 * t) * 0.6f * std::exp(-t * 22) + bp.process(nz.next()) * 0.7f) * e;
        }
        normalise(out, 0.85f);
        fadeEdges(out);
        g_clips[SFX_PLANK].samples = std::move(out);
        g_clips[SFX_CRAFT].samples = g_clips[SFX_PLANK].samples;
    }
    genVehicleLoop(SFX_ENGINE_START, 42, 130, 0.9f, 81);
    genVehicleLoop(SFX_VEHICLE_LOOP, 48, 105, 1.2f, 82);
    genImpact(SFX_IMPACT_VEHICLE, 0.65f, 90, 0.9f, 0.3f, 0.18f, 83);
    genLoopNoise(SFX_AIRSTRIKE_FLYBY, 1.6f, 700, 0.8f, false, 84, 0.0f);
    {
        // alarm: alternating two-tone siren
        int n = (int)(1.4f * SR);
        std::vector<float> out(n, 0);
        float ph = 0;
        for (int i = 0; i < n; i++) {
            float t = (float)i / SR;
            float f = (std::fmod(t, 0.7f) < 0.35f) ? 660 : 880;
            ph += 2 * PI * f / SR;
            float e = std::min(1.0f, t * 8) * std::min(1.0f, (n - i) / (float)(SR * 0.05f));
            out[i] = (std::sin(ph) * 0.6f + std::sin(ph * 2) * 0.15f) * e;
        }
        normalise(out, 0.7f);
        g_clips[SFX_ALARM].samples = std::move(out);
        g_clips[SFX_ALARM].loopable = true;
    }
    genLoopNoise(SFX_FIRE_LOOP, 1.0f, 800, 0.5f, false, 91, 0.06f);
    genLoopNoise(SFX_WIND_LOOP, 2.0f, 420, 0.4f, false, 92, 0.0f);
    genLoopNoise(SFX_RAIN_LOOP, 1.6f, 4200, 0.4f, false, 93, 0.02f);
    genCreak(SFX_STRUCTURE_CREAK, 1.6f, 420, 101);
    genExplosion(SFX_COLLAPSE, 3.2f, 1.3f, 102);
    genImpact(SFX_OBJECT_PICKUP, 0.2f, 380, 0.6f, 0.2f, 0.04f, 103);
    genTone(SFX_OBJECT_THROW, 0.2f, 500, 260, 0.005f, 0.06f, 1, 0.4f);
    genTone(SFX_PICKUP_ITEM, 0.5f, 660, 1320, 0.01f, 0.18f, 3, 0.0f);
    // mission complete: three-note fanfare
    {
        int n = (int)(1.6f * SR);
        std::vector<float> out(n, 0);
        const float notes[3] = {523.25f, 659.25f, 783.99f};
        for (int k = 0; k < 3; k++) {
            int s0 = (int)(k * 0.18f * SR);
            for (int i = 0; i < n - s0; i++) {
                float t = (float)i / SR;
                float e = std::exp(-t * 3.2f) * std::min(1.0f, t * 200);
                out[s0 + i] += (std::sin(2 * PI * notes[k] * t) * 0.6f + std::sin(4 * PI * notes[k] * t) * 0.2f +
                                std::sin(6 * PI * notes[k] * t) * 0.08f) * e;
            }
        }
        normalise(out, 0.85f);
        fadeEdges(out, 10);
        g_clips[SFX_MISSION_COMPLETE].samples = std::move(out);
    }
    genTone(SFX_MISSION_FAIL, 1.1f, 300, 120, 0.02f, 0.45f, 3, 0.08f);
    genTone(SFX_RELOAD, 0.16f, 700, 400, 0.003f, 0.04f, 2, 0.4f);
    genTone(SFX_DRYFIRE, 0.08f, 1200, 900, 0.001f, 0.02f, 1, 0.5f);
    genImpact(SFX_PLANT, 0.2f, 500, 0.7f, 0.1f, 0.04f, 111);
}

// ---------------------------------------------------------------- output
#ifdef _WIN32
HWAVEOUT g_wave = nullptr;
const int BUF_FRAMES = 1024;
const int NUM_BUFS = 4;
int16_t g_buffers[NUM_BUFS][BUF_FRAMES * 2];
WAVEHDR g_headers[NUM_BUFS];
std::vector<float> g_mixBuf;
bool g_audioOk = false;
#endif

void mixInto(float* outStereo, int frames) {
    std::memset(outStereo, 0, sizeof(float) * frames * 2);
    // music bed
    if (g_musicTrack >= 0) {
        static double chordPhase = 0;
        const float roots[3] = {55.0f, 65.41f, 49.0f};
        for (int i = 0; i < frames; i++) {
            double t = (g_musicPhase += 1.0 / SR);
            float beat = (float)std::fmod(t * 0.6, 1.0);
            int root = (int)(std::floor(t * 0.15)) % 3;
            float base = roots[root];
            float pad = std::sin(2 * PI * base * (float)t) * 0.10f +
                        std::sin(2 * PI * base * 1.5f * (float)t) * 0.07f +
                        std::sin(2 * PI * base * 2.0f * (float)t) * 0.05f;
            float pulse = std::exp(-beat * 7.0f) * (0.05f + 0.05f * g_musicIntensity);
            float drum = std::sin(2 * PI * 46 * (float)t) * pulse;
            float v = (pad * (0.55f + g_musicIntensity * 0.45f) + drum) * g_music;
            outStereo[i * 2] += v;
            outStereo[i * 2 + 1] += v * (0.92f + 0.08f * std::sin((float)t * 0.7f));
        }
    }
    for (int v = 0; v < MAX_VOICES; v++) {
        Voice& vo = g_voices[v];
        if (!vo.active) continue;
        const Clip& clip = g_clips[vo.sfx];
        int n = (int)clip.samples.size();
        if (n <= 0) { vo.active = false; continue; }
        for (int i = 0; i < frames; i++) {
            int idx = (int)vo.pos;
            if (idx >= n) {
                if (vo.loop && clip.loopable) {
                    vo.pos -= (float)n;
                    idx = (int)vo.pos;
                    if (idx < 0) idx = 0;
                } else {
                    vo.active = false;
                    break;
                }
            }
            float frac = vo.pos - idx;
            float s0 = clip.samples[idx];
            float s1 = idx + 1 < n ? clip.samples[idx + 1] : s0;
            float s = s0 + (s1 - s0) * frac;
            // distance low-pass (muffled far away)
            vo.lpL += (s - vo.lpL) * vo.lpCoef;
            vo.lpR += (s - vo.lpR) * vo.lpCoef;
            outStereo[i * 2] += vo.lpL * vo.gainL * vo.baseGain;
            outStereo[i * 2 + 1] += vo.lpR * vo.gainR * vo.baseGain;
            vo.pos += vo.rate;
        }
    }
    // soft limiter
    for (int i = 0; i < frames * 2; i++) {
        float x = outStereo[i] * g_master * g_sfx;
        outStereo[i] = std::tanh(x * 1.15f) * 0.92f;
    }
}

#ifdef _WIN32
void CALLBACK waveProc(HWAVEOUT h, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2) {
    if (msg != WOM_DONE) return;
    WAVEHDR* hdr = (WAVEHDR*)p1;
    mixInto(g_mixBuf.data(), BUF_FRAMES);
    int16_t* dst = (int16_t*)hdr->lpData;
    for (int i = 0; i < BUF_FRAMES * 2; i++) {
        float s = clampf(g_mixBuf[i], -1.0f, 1.0f);
        dst[i] = (int16_t)(s * 32000);
    }
    waveOutWrite(h, hdr, sizeof(WAVEHDR));
    (void)inst; (void)p2;
}
#endif

int findVoice() {
    for (int i = 0; i < MAX_VOICES; i++) if (!g_voices[i].active) return i;
    // steal the quietest
    int best = 0;
    float bestGain = 1e9f;
    for (int i = 0; i < MAX_VOICES; i++) {
        float gv = g_voices[i].gainL + g_voices[i].gainR;
        if (gv < bestGain) { bestGain = gv; best = i; }
    }
    return best;
}

void spatialize(Voice& v, vec3 pos, float volume) {
    vec3 rel = pos - g_listenerPos;
    float dist = rel.len();
    vec3 fwd = g_listenerFwd.norm();
    vec3 right = cross(fwd, g_listenerUp).norm();
    float d = std::max(0.3f, dist);
    float atten = 1.0f / (1.0f + d * 0.10f + d * d * 0.010f);
    float pan = 0.0f;
    if (dist > 0.4f) pan = clampf(dot(rel.norm(), right), -1, 1) * 0.85f;
    // near sounds are louder and centred
    if (dist < 2.0f) pan *= dist / 2.0f;
    v.gainL = volume * atten * (1.0f - std::max(0.0f, pan) * 0.65f) * 1.35f;
    v.gainR = volume * atten * (1.0f + std::min(0.0f, pan) * 0.65f) * 1.35f;
    v.lpCoef = clampf(1.0f - dist / 90.0f, 0.12f, 1.0f);
}

} // namespace

namespace audio {

bool init() {
    buildAll();
#ifdef _WIN32
    WAVEFORMATEX wf = {0};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = SR;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = SR * 4;
    if (waveOutOpen(&g_wave, WAVE_MAPPER, &wf, (DWORD_PTR)waveProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        logMsg("[audio] waveOutOpen failed");
        return false;
    }
    g_mixBuf.assign(BUF_FRAMES * 2, 0.f);
    for (int i = 0; i < NUM_BUFS; i++) {
        std::memset(&g_headers[i], 0, sizeof(WAVEHDR));
        g_headers[i].lpData = (LPSTR)g_buffers[i];
        g_headers[i].dwBufferLength = BUF_FRAMES * 2 * sizeof(int16_t);
        waveOutPrepareHeader(g_wave, &g_headers[i], sizeof(WAVEHDR));
        std::memset(g_buffers[i], 0, sizeof(g_buffers[i]));
        waveOutWrite(g_wave, &g_headers[i], sizeof(WAVEHDR));
    }
    g_audioOk = true;
    logMsg("[audio] WinMM output %d Hz stereo, %d voices", SR, MAX_VOICES);
    return true;
#else
    return false;
#endif
}

void shutdown() {
#ifdef _WIN32
    if (g_wave) {
        waveOutReset(g_wave);
        for (int i = 0; i < NUM_BUFS; i++) waveOutUnprepareHeader(g_wave, &g_headers[i], sizeof(WAVEHDR));
        waveOutClose(g_wave);
        g_wave = nullptr;
    }
#endif
}

void mix(float* outStereo, int frames) { mixInto(outStereo, frames); }

void play(SfxId id, vec3 pos, float volume, float pitch, bool loops, int loopId) {
    if (id < 0 || id >= SFX_COUNT) return;
    if (g_clips[id].samples.empty()) return;
    int vi = findVoice();
    Voice& v = g_voices[vi];
    v.sfx = id;
    v.active = true;
    v.pos = 0;
    v.rate = clampf(pitch, 0.25f, 4.0f);
    v.loop = loops;
    v.loopId = loopId;
    v.baseGain = g_clips[id].baseGain;
    spatialize(v, pos, volume);
}

int playLoop(SfxId id, vec3 pos, float volume, float pitch) {
    if (id < 0 || id >= SFX_COUNT) return -1;
    int vi = findVoice();
    Voice& v = g_voices[vi];
    v.sfx = id;
    v.active = true;
    v.pos = 0;
    v.rate = clampf(pitch, 0.25f, 4.0f);
    v.loop = true;
    v.loopId = vi;
    spatialize(v, pos, volume);
    return vi;
}

void updateLoop(int handle, vec3 pos, float volume, float pitch) {
    if (handle < 0 || handle >= MAX_VOICES) return;
    Voice& v = g_voices[handle];
    if (!v.active) return;
    v.rate = clampf(pitch, 0.25f, 4.0f);
    spatialize(v, pos, volume);
}

void stopLoop(int handle) {
    if (handle < 0 || handle >= MAX_VOICES) return;
    g_voices[handle].active = false;
}

void setListener(vec3 pos, vec3 fwd, vec3 up) {
    g_listenerPos = pos;
    g_listenerFwd = fwd;
    g_listenerUp = up;
}

void setVolumes(float master, float sfx, float music) {
    g_master = master; g_sfx = sfx; g_music = music;
}

void startMusic(int track) { g_musicTrack = track; }
void stopMusic() { g_musicTrack = -1; }
void setMusicIntensity(float v) { g_musicIntensity = clampf(v, 0, 1); }

int activeVoices() {
    int n = 0;
    for (int i = 0; i < MAX_VOICES; i++) if (g_voices[i].active) n++;
    return n;
}

void update(float dt) {
    (void)dt;
#ifdef _WIN32
    if (!g_audioOk) return;
#endif
}

} // namespace audio
