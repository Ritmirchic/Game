// FRACTURE — procedural sound synthesis + positional mixer (no external assets)
#include "audio.h"
#include "rng.h"
#include <mutex>
#include <cmath>

namespace ft {

// ------------------------------------------------------------ synth helpers --
namespace {

struct Synth {
    std::vector<f32> L, R;   // float accumulators at44100
    Rng rng{0xF2AC7E};
    void reserve(int n) { L.reserve(n); R.reserve(n); }
    void push(f32 l, f32 r) { L.push_back(l); R.push_back(r); }
    int size() const { return (int)L.size(); }
    f32 noise() { return (f32)(rng.range(-1.0, 1.0)); }
};

f32 envExp(f32 t, f32 dur, f32 k) { //1 →0
    f32 x = t / dur;
    if (x >= 1) return 0;
    return std::exp(-k * x) * (1.f - x * x * x * 0.3f);
}

struct OnePole { f32 y = 0; f32 step(f32 x, f32 a) { y += a * (x - y); return y; } };
struct SVF { // state-variable bandpass/lowpass
    f32 lo = 0, band = 0;
    void process(f32 x, f32 f, f32 q, f32& lp, f32& bp) {
        lo += f * band;
        f32 hi = x - lo - q * band;
        band += f * hi;
        lp = lo; bp = band;
    }
};

SfxSample finish(Synth& s, bool loop = false) {
    SfxSample out;
    int n = s.size();
    out.frames = n;
    out.loop = loop;
    out.pcm.resize((size_t)n * 2);
    for (int i = 0; i < n; i++) {
        f32 l = s.L[i], r = s.R[i];
        l = std::tanh(l * 1.05f);
        r = std::tanh(r * 1.05f);
        out.pcm[(size_t)i * 2] = (s16)(tclamp(l, -1.f, 1.f) * 32000.f);
        out.pcm[(size_t)i * 2 + 1] = (s16)(tclamp(r, -1.f, 1.f) * 32000.f);
    }
    return out;
}

// helpers reused by generators ------------------------------------------------
void genNoiseBurst(Synth& s, f32 dur, f32 lpA0, f32 lpA1, f32 amp, f32 k = 4.f) {
    OnePole lp;
    int n = (int)(dur * 44100);
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / 44100.f;
        f32 x = t / dur;
        f32 a = lerpf(lpA0, lpA1, x);
        f32 v = lp.step(s.noise(), a);
        f32 e = envExp(t, dur, k);
        s.push(v * e * amp, v * e * amp);
    }
}
void genTone(Synth& s, f32 f0, f32 f1, f32 dur, f32 amp, f32 k = 6.f, f32 pan = 0.f) {
    int n = (int)(dur * 44100);
    f32 phase = 0;
    f32 pl = std::sqrt(tclamp(0.5f - pan * 0.5f, 0.f, 1.f)) * 1.414f;
    f32 pr = std::sqrt(tclamp(0.5f + pan * 0.5f, 0.f, 1.f)) * 1.414f;
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / 44100.f;
        f32 f = lerpf(f0, f1, t / dur);
        phase += 2.f * 3.14159265f * f / 44100.f;
        if (phase > 2.f * 3.14159265f) phase -= 2.f * 3.14159265f;
        f32 v = std::sin(phase);
        f32 e = envExp(t, dur, k);
        s.push(v * e * amp * pl, v * e * amp * pr);
    }
}
void genSweepNoise(Synth& s, f32 dur, f32 f0, f32 f1, f32 q, f32 amp, f32 k) {
    SVF filt;
    int n = (int)(dur * 44100);
    for (int i = 0; i < n; i++) {
        f32 t = (f32)i / 44100.f;
        f32 x = t / dur;
        f32 fc = lerpf(f0, f1, x);
        f32 f = 2.f * std::sin(3.14159265f * tclamp(fc, 20.f, 18000.f) / 44100.f);
        f32 lp, bp;
        filt.process(s.noise(), f, q, lp, bp);
        f32 e = envExp(t, dur, k);
        s.push(bp * e * amp, bp * e * amp);
    }
}
void genCrackle(Synth& s, f32 dur, f32 density, f32 amp) {
    int n = (int)(dur * 44100);
    int gap = (int)(44100.f / density);
    int next = s.rng.range(0, gap);
    for (int i = 0; i < n; i++) {
        f32 v = 0;
        if (i == next) {
            int len = s.rng.range(40, 220);
            f32 a = s.rng.range(0.4f, 1.f);
            OnePole hp;
            for (int j = 0; j < len && i + j < n; j++) {
                f32 nz = s.noise() * a * (1.f - (f32)j / len);
                f32 h = nz - hp.step(nz, 0.6f);
                s.L[i + j] += h * amp;
                s.R[i + j] += h * amp;
            }
            next = i + s.rng.range(gap / 2, gap * 2);
        }
        (void)v;
    }
}

SfxSample synthesize(SfxId id) {
    Synth s;
    s.rng = Rng(0x9E3779B9u ^ (u32)(id * 2654435761u));
    switch (id) {
        case SFX_UI_CLICK: genTone(s, 950, 620, 0.045f, 0.5f, 8); genNoiseBurst(s, 0.012f, 0.5f, 0.3f, 0.25f, 6); break;
        case SFX_UI_HOVER: genTone(s, 520, 560, 0.03f, 0.22f, 9); break;
        case SFX_FOOTSTEP_1: case SFX_FOOTSTEP_2: case SFX_FOOTSTEP_3: case SFX_FOOTSTEP_4: {
            f32 base = 1.f + (f32)(id - SFX_FOOTSTEP_1) * 0.07f;
            genNoiseBurst(s, 0.1f * base, 0.16f, 0.05f, 0.5f, 7);           // dull scuff
            genTone(s, 95 * base, 60, 0.07f, 0.35f, 9);                     // heel thump
            break;
        }
        case SFX_IMPACT_WOOD: {
            genNoiseBurst(s, 0.13f, 0.22f, 0.06f, 0.8f, 6);
            genTone(s, 210, 150, 0.16f, 0.55f, 7);
            genTone(s, 420, 380, 0.09f, 0.2f, 9);
            break;
        }
        case SFX_IMPACT_METAL: {
            genNoiseBurst(s, 0.04f, 0.6f, 0.3f, 0.7f, 9);
            // inharmonic partials
            genTone(s, 1180, 1160, 0.4f, 0.30f, 6);
            genTone(s, 1785, 1770, 0.34f, 0.22f, 6.5f, 0.2f);
            genTone(s, 2410, 2400, 0.28f, 0.16f, 7, -0.2f);
            genTone(s, 3150, 3140, 0.2f, 0.10f, 8);
            break;
        }
        case SFX_IMPACT_CONCRETE: {
            genNoiseBurst(s, 0.17f, 0.35f, 0.05f, 0.85f, 5.5f);
            genTone(s, 88, 62, 0.13f, 0.6f, 8);
            genCrackle(s, 0.25f, 900, 0.25f);
            break;
        }
        case SFX_IMPACT_GLASS: {
            genNoiseBurst(s, 0.05f, 0.9f, 0.5f, 0.8f, 8);
            for (int i = 0; i < 7; i++) {
                f32 f = s.rng.range(2300, 5200);
                genTone(s, f, f * s.rng.range(0.96f, 1.04f), s.rng.range(0.12f, 0.4f),
                        s.rng.range(0.06f, 0.16f), s.rng.range(5.f, 9.f), s.rng.range(-0.6f, 0.6f));
            }
            genCrackle(s, 0.5f, 1600, 0.3f);
            break;
        }
        case SFX_IMPACT_SOIL: {
            genNoiseBurst(s, 0.11f, 0.06f, 0.02f, 0.9f, 7);
            genTone(s, 70, 48, 0.1f, 0.5f, 9);
            break;
        }
        case SFX_EXPLOSION_S: {
            genNoiseBurst(s, 0.55f, 0.5f, 0.04f, 1.5f, 5);
            genTone(s, 85, 38, 0.5f, 1.2f, 5);
            genCrackle(s, 0.7f, 700, 0.5f);
            break;
        }
        case SFX_EXPLOSION_M: {
            genNoiseBurst(s, 1.0f, 0.55f, 0.02f, 1.8f, 4);
            genTone(s, 70, 28, 0.9f, 1.5f, 4);
            genCrackle(s, 1.2f, 900, 0.6f);
            // tail rumble
            genNoiseBurst(s, 0.9f, 0.03f, 0.01f, 0.5f, 2.5f);
            break;
        }
        case SFX_EXPLOSION_L: {
            genNoiseBurst(s, 1.8f, 0.6f, 0.015f, 2.0f, 3);
            genTone(s, 60, 22, 1.6f, 1.8f, 3);
            genCrackle(s, 1.9f, 1100, 0.7f);
            genNoiseBurst(s, 1.6f, 0.025f, 0.008f, 0.7f, 2);
            break;
        }
        case SFX_DEBRIS_RAIN: {
            int n = (int)(1.3f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            for (int g = 0; g < 46; g++) {
                int at = s.rng.range(0, n - 900);
                f32 amp = s.rng.range(0.15f, 0.6f);
                f32 pan = s.rng.range(-0.8f, 0.8f);
                f32 fc = s.rng.range(900, 3600);
                OnePole lp;
                int len = s.rng.range(250, 850);
                for (int j = 0; j < len; j++) {
                    f32 e = 1.f - (f32)j / len;
                    f32 v = lp.step(s.noise(), fc / 44100.f) * e * amp;
                    s.L[at + j] += v * (1.f - tclamp(pan, 0.f, 1.f));
                    s.R[at + j] += v * (1.f + tclamp(pan, -1.f, 0.f));
                }
            }
            break;
        }
        case SFX_GUN_SHOTGUN: {
            genNoiseBurst(s, 0.06f, 0.95f, 0.4f, 2.4f, 7);
            genNoiseBurst(s, 0.3f, 0.25f, 0.03f, 1.6f, 5);
            genTone(s, 130, 55, 0.3f, 1.3f, 5);
            break;
        }
        case SFX_GUN_RIFLE: {
            genNoiseBurst(s, 0.03f, 1.f, 0.7f, 2.6f, 9);
            genNoiseBurst(s, 0.16f, 0.3f, 0.04f, 1.4f, 6);
            genTone(s, 170, 70, 0.14f, 0.9f, 7);
            break;
        }
        case SFX_GUN_SMG: {
            genNoiseBurst(s, 0.02f, 1.f, 0.6f, 2.2f, 10);
            genNoiseBurst(s, 0.09f, 0.35f, 0.05f, 1.0f, 7);
            genTone(s, 210, 90, 0.08f, 0.7f, 8);
            break;
        }
        case SFX_GUN_PISTOL: {
            genNoiseBurst(s, 0.025f, 1.f, 0.65f, 2.3f, 9);
            genNoiseBurst(s, 0.12f, 0.32f, 0.05f, 1.1f, 6.5f);
            genTone(s, 190, 85, 0.11f, 0.8f, 7.5f);
            break;
        }
        case SFX_GUN_PLASMA: {
            genTone(s, 1350, 170, 0.32f, 0.7f, 5);
            genTone(s, 2700, 340, 0.28f, 0.3f, 5.5f, 0.3f);
            genNoiseBurst(s, 0.1f, 0.7f, 0.2f, 0.4f, 7);
            break;
        }
        case SFX_ROCKET_LAUNCH: {
            genNoiseBurst(s, 0.5f, 0.45f, 0.12f, 1.3f, 4);
            genTone(s, 140, 60, 0.35f, 0.9f, 5);
            genTone(s, 60, 140, 0.5f, 0.4f, 3);
            break;
        }
        case SFX_GRENADE_LAUNCH: {
            genTone(s, 320, 90, 0.18f, 1.0f, 6);
            genNoiseBurst(s, 0.1f, 0.5f, 0.1f, 0.7f, 6);
            break;
        }
        case SFX_CHARGE_BEEP: {
            genTone(s, 1200, 1200, 0.07f, 0.4f, 6);
            genTone(s, 1200, 1200, 0.07f, 0.4f, 6);
            // second beep offset
            int off = (int)(0.14f * 44100);
            int n = s.size();
            s.L.resize(n + off, 0.f); s.R.resize(n + off, 0.f);
            break;
        }
        case SFX_CHARGE_STICK: {
            genNoiseBurst(s, 0.03f, 0.4f, 0.15f, 0.7f, 8);
            genTone(s, 400, 260, 0.07f, 0.5f, 8);
            break;
        }
        case SFX_DRILL: {
            int n = (int)(1.0f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            SVF bp;
            f32 ph = 0;
            for (int i = 0; i < n; i++) {
                ph += 2.f * 3.14159f * 96.f / 44100.f;
                if (ph > 6.283f) ph -= 6.283f;
                f32 motor = (ph / 6.283f * 2.f - 1.f) * 0.35f;
                f32 f = 2.f * std::sin(3.14159f * 2600.f / 44100.f);
                f32 lp, b;
                bp.process(s.noise(), f, 0.08f, lp, b);
                f32 v = motor + b * 0.9f;
                s.L[i] = v * 0.8f;
                s.R[i] = v * 0.8f;
            }
            return finish(s, true);
        }
        case SFX_SAW: {
            int n = (int)(1.0f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            SVF bp;
            f32 ph = 0;
            for (int i = 0; i < n; i++) {
                ph += 2.f * 3.14159f * 133.f / 44100.f;
                if (ph > 6.283f) ph -= 6.283f;
                f32 motor = (ph / 6.283f) * 0.4f - 0.2f;
                f32 f = 2.f * std::sin(3.14159f * 3300.f / 44100.f);
                f32 lp, b;
                bp.process(s.noise(), f, 0.06f, lp, b);
                f32 v = motor * 0.7f + b * 1.1f;
                s.L[i] = v * 0.75f;
                s.R[i] = v * 0.75f;
            }
            return finish(s, true);
        }
        case SFX_TORCH: {
            int n = (int)(1.0f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            SVF bp;
            f32 lfo = 0;
            for (int i = 0; i < n; i++) {
                lfo += 2.f * 3.14159f * 7.f / 44100.f;
                f32 wob = 0.6f + 0.4f * std::sin(lfo);
                f32 f = 2.f * std::sin(3.14159f * (1500.f + 700.f * wob) / 44100.f);
                f32 lp, b;
                bp.process(s.noise(), f, 0.25f, lp, b);
                f32 v = b * 1.3f + lp * 0.2f;
                s.L[i] = v * 0.7f; s.R[i] = v * 0.7f;
            }
            return finish(s, true);
        }
        case SFX_EXTINGUISHER: {
            int n = (int)(1.0f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            SVF bp;
            for (int i = 0; i < n; i++) {
                f32 f = 2.f * std::sin(3.14159f * 4200.f / 44100.f);
                f32 lp, b;
                bp.process(s.noise(), f, 0.5f, lp, b);
                s.L[i] = b * 1.2f; s.R[i] = b * 1.2f;
            }
            return finish(s, true);
        }
        case SFX_SPRAY_PAINT: {
            int n = (int)(0.9f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            SVF bp;
            for (int i = 0; i < n; i++) {
                f32 f = 2.f * std::sin(3.14159f * 5400.f / 44100.f);
                f32 lp, b;
                bp.process(s.noise(), f, 0.6f, lp, b);
                f32 v = b;
                // rattle ball
                if (s.rng.range(0, 4000) < 3) v += s.noise() * 0.6f;
                s.L[i] = v * 0.8f; s.R[i] = v * 0.8f;
            }
            return finish(s, true);
        }
        case SFX_HARPOON: {
            genNoiseBurst(s, 0.07f, 0.7f, 0.2f, 1.2f, 7);
            genTone(s, 640, 180, 0.12f, 0.7f, 6);
            genTone(s, 160, 90, 0.2f, 0.6f, 6);
            break;
        }
        case SFX_SLEDGE_SWING: {
            genSweepNoise(s, 0.26f, 500, 2400, 0.4f, 0.7f, 3);
            break;
        }
        case SFX_DOOR_CREAK: {
            int n = (int)(0.55f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            f32 ph = 0;
            for (int i = 0; i < n; i++) {
                f32 t = (f32)i / 44100.f;
                f32 f = lerpf(300, 460, t / 0.55f) * (1.f + 0.06f * std::sin(t * 80.f));
                ph += 2.f * 3.14159f * f / 44100.f;
                f32 saw = 2.f * (ph / 6.283f - std::floor(ph / 6.283f + 0.5f));
                f32 e = envExp(t, 0.55f, 3.f);
                f32 v = saw * e * 0.35f;
                s.L[i] = v; s.R[i] = v;
            }
            break;
        }
        case SFX_IGNITE: {
            genNoiseBurst(s, 0.3f, 0.1f, 0.5f, 1.0f, 4);
            genTone(s, 110, 420, 0.28f, 0.5f, 5);
            break;
        }
        case SFX_FIRE_LOOP: {
            int n = (int)(1.2f * 44100);
            s.L.assign(n, 0.f); s.R.assign(n, 0.f);
            OnePole lp;
            for (int i = 0; i < n; i++) {
                f32 v = lp.step(s.noise(), 0.035f) * 3.2f;
                s.L[i] = v * 0.9f; s.R[i] = v;
            }
            genCrackle(s, 1.2f, 90, 0.5f);
            return finish(s, true);
        }
        case SFX_VEHICLE_CRASH: {
            genNoiseBurst(s, 0.5f, 0.4f, 0.05f, 1.4f, 4);
            for (int i = 0; i < 4; i++)
                genTone(s, s.rng.range(700, 2600), s.rng.range(600, 2300),
                        s.rng.range(0.1f, 0.3f), 0.2f, 6, s.rng.range(-0.5f, 0.5f));
            genTone(s, 90, 55, 0.4f, 0.9f, 5);
            break;
        }
        case SFX_PICKUP: {
            genTone(s, 660, 660, 0.05f, 0.4f, 7);
            genTone(s, 990, 990, 0.07f, 0.4f, 7);
            break;
        }
        case SFX_JUMP_LAND: {
            genNoiseBurst(s, 0.1f, 0.12f, 0.04f, 0.7f, 7);
            genTone(s, 90, 60, 0.1f, 0.6f, 8);
            break;
        }
        case SFX_BODY_FALL: {
            genNoiseBurst(s, 0.16f, 0.1f, 0.03f, 0.8f, 6);
            genTone(s, 75, 50, 0.2f, 0.7f, 6);
            break;
        }
        case SFX_EMPTY: {
            genNoiseBurst(s, 0.02f, 0.5f, 0.2f, 0.5f, 9);
            break;
        }
        default:
            genTone(s, 440, 440, 0.05f, 0.3f, 7);
            break;
    }
    return finish(s);
}

std::vector<SfxSample> g_bank;
std::once_flag g_bankFlag;

} // namespace

void audioInitBank() {
    std::call_once(g_bankFlag, [] {
        g_bank.resize(SFX_COUNT);
        for (int i = 0; i < SFX_COUNT; i++) g_bank[i] = synthesize((SfxId)i);
        int total = 0;
        for (auto& s : g_bank) total += s.frames;
        logf("audio bank: %d samples, %.1fs", SFX_COUNT, total / 44100.f);
    });
}

const SfxSample& sfxSample(SfxId id) {
    audioInitBank();
    static SfxSample empty;
    if (id < 0 || id >= SFX_COUNT) return empty;
    return g_bank[id];
}

bool sfxWriteWavRaw(const s16* pcm, int frames, const char* path) {
    if (frames <= 0) return false;
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    u32 sr = 44100, dataBytes = (u32)((size_t)frames * 4);
    u32 byteRate = sr * 2 * 2;
    auto w32 = [&](u32 v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](u16 v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    w32(36 + dataBytes);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f);
    w32(16);
    w16(1); w16(2);
    w32(sr); w32(byteRate);
    w16(4); w16(16);
    std::fwrite("data", 1, 4, f);
    w32(dataBytes);
    std::fwrite(pcm, 1, dataBytes, f);
    std::fclose(f);
    return true;
}

bool sfxWriteWav(SfxId id, const char* path) {
    const SfxSample& s = sfxSample(id);
    if (s.frames <= 0) return false;
    return sfxWriteWavRaw(s.pcm.data(), s.frames, path);
}

// ------------------------------------------------------------------ mixer ----
static std::mutex g_audioMutex;
static AudioMixer g_mixer;

AudioMixer& audio() { audioInitBank(); return g_mixer; }

void AudioMixer::init(int sampleRate) {
    sr_ = sampleRate;
    voices_.clear();
    voices_.reserve(48);
}

void AudioMixer::setVolume(SfxId, f32) {}

int AudioMixer::play(const VoiceDesc& d) {
    std::lock_guard<std::mutex> lk(g_audioMutex);
    if (sfxSample(d.sfx).frames <= 0) return -1;
    int freeSlot = -1;
    for (int i = 0; i < (int)voices_.size(); i++) {
        if (!voices_[i].active) { freeSlot = i; break; }
    }
    int slot = freeSlot;
    if (slot < 0) {
        // steal only if incoming priority beats the weakest active voice
        int worst = -1;
        f32 worstScore = 1e18f;
        for (int i = 0; i < (int)voices_.size(); i++) {
            f32 score = voices_[i].priority * 1000.f - voices_[i].age;
            if (score < worstScore) { worstScore = score; worst = i; }
        }
        f32 newScore = d.priority * 1000.f;
        if (worst < 0 || newScore <= worstScore) {
            if ((int)voices_.size() >= 48) return -1;
            voices_.emplace_back();
            slot = (int)voices_.size() - 1;
        } else {
            slot = worst;
        }
    }
    Voice& v = voices_[slot];
    v = Voice();
    v.sfx = d.sfx;
    v.pos = d.pos;
    v.gain = d.gain;
    v.pitch = tclamp(d.pitch, 0.25f, 4.f);
    v.direct2D = d.direct2D;
    v.loop = d.loop || sfxSample(d.sfx).loop;
    v.minDist = tmax(0.1f, d.minDist);
    v.maxDist = tmax(v.minDist + 1.f, d.maxDist);
    v.priority = d.priority;
    v.cursor = 0;
    v.age = 0;
    v.active = true;
    return slot;
}

void AudioMixer::stop(int voice) {
    std::lock_guard<std::mutex> lk(g_audioMutex);
    if (voice >= 0 && voice < (int)voices_.size()) voices_[voice].active = false;
}

void AudioMixer::stopAll() {
    std::lock_guard<std::mutex> lk(g_audioMutex);
    for (auto& v : voices_) v.active = false;
}

void AudioMixer::updateLoops(f32 dt) {
    std::lock_guard<std::mutex> lk(g_audioMutex);
    for (auto& v : voices_) {
        if (!v.active) continue;
        v.age += dt;
        if (!v.loop && v.cursor >= (f64)sfxSample(v.sfx).frames) v.active = false;
    }
}

int AudioMixer::activeVoices() const {
    int n = 0;
    for (auto& v : voices_) n += v.active ? 1 : 0;
    return n;
}

void AudioMixer::mix(s16* out, int frames) {
    std::lock_guard<std::mutex> lk(g_audioMutex);
    static std::vector<f32> acc;
    acc.assign((size_t)frames * 2, 0.f);
    Vec3 right = norm(cross(lfwd_, Vec3(0, 1, 0)));
    if (lenSq(right) < 1e-6f) right = Vec3(1, 0, 0);
    for (Voice& v : voices_) {
        if (!v.active) continue;
        const SfxSample& s = sfxSample(v.sfx);
        if (s.frames <= 0) { v.active = false; continue; }
        f32 att = 1.f, panL = 0.7071f, panR = 0.7071f;
        if (!v.direct2D) {
            Vec3 dp = v.pos - lpos_;
            f32 d = len(dp);
            att = v.minDist / tmax(v.minDist, d);
            f32 fade = tclamp((v.maxDist - d) / (v.maxDist * 0.3f), 0.f, 1.f);
            att *= fade;
            if (d > 0.01f) {
                f32 pan = tclamp(dot(dp / d, right) * 0.5f + 0.5f, 0.f, 1.f);
                panL = std::sqrt(1.f - pan);
                panR = std::sqrt(pan);
            }
        }
        if (att <= 0.0005f) continue;
        f32 gl = att * v.gain * master_ * panL * 1.414f;
        f32 gr = att * v.gain * master_ * panR * 1.414f;
        f64 step = v.pitch;
        for (int i = 0; i < frames; i++) {
            if (v.cursor >= (f64)s.frames) {
                if (v.loop) v.cursor = 0;
                else { v.active = false; break; }
            }
            int i0 = (int)v.cursor;
            int i1 = tmin(i0 + 1, s.frames - 1);
            f32 fr = (f32)(v.cursor - i0);
            f32 sl = lerpf((f32)s.pcm[(size_t)i0 * 2], (f32)s.pcm[(size_t)i1 * 2], fr) / 32767.f;
            f32 sr = lerpf((f32)s.pcm[(size_t)i0 * 2 + 1], (f32)s.pcm[(size_t)i1 * 2 + 1], fr) / 32767.f;
            acc[(size_t)i * 2] += sl * gl;
            acc[(size_t)i * 2 + 1] += sr * gr;
            v.cursor += step;
        }
    }
    for (int i = 0; i < frames * 2; i++)
        out[i] = (s16)(tclamp(acc[i], -1.f, 1.f) * 32767.f);
}

void sfxPlay(SfxId id, Vec3 pos, f32 gain, f32 pitch) {
    VoiceDesc d;
    d.sfx = id;
    d.pos = pos;
    d.gain = gain;
    d.pitch = pitch;
    d.priority = 2;
    audio().play(d);
}
void sfxPlay2D(SfxId id, f32 gain) {
    VoiceDesc d;
    d.sfx = id;
    d.direct2D = true;
    d.gain = gain;
    d.priority = 5;
    audio().play(d);
}

SfxId sfxForImpact(u8 mat, bool hardHit) {
    if (!hardHit) return SFX_IMPACT_SOIL;
    switch (mat) {
        case 3: return SFX_IMPACT_WOOD;              // wood
        case 4: return SFX_IMPACT_GLASS;             // glass
        case 5: case 6: case 16: return SFX_IMPACT_METAL;  // metal/rebar/painted
        case 0: return SFX_EMPTY;
        case 8: case 11: case 19: case 20: return SFX_IMPACT_SOIL;
        default: return SFX_IMPACT_CONCRETE;
    }
}

} // namespace ft
