#include "audio.h"
#include <string.h>
#include <math.h>
#if defined(_WIN32)
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#endif

// ---------------------------------------------------------------- basics
static float fast_noise(Rng *r) { return rng_f(r) * 2.f - 1.f; }
static float db_to_lin(float db) { return powf(10.f, db / 20.f); }

Sound *snd_make(const char *name, int frames) {
    Sound *s = (Sound *)calloc(1, sizeof(Sound));
    s->frames = frames;
    s->rate = AUD_RATE;
    s->pcm = (int16_t *)calloc((size_t)frames * 2, sizeof(int16_t));
    snprintf(s->name, sizeof s->name, "%s", name ? name : "snd");
    return s;
}
void snd_free(Sound *s) {
    if (!s) return;
    free(s->pcm);
    free(s);
}

Sound *snd_resample(Sound *s, int rate) { (void)rate; return s; }

static inline void put(Sound *s, int i, float l, float r) {
    if (i < 0 || i >= s->frames) return;
    if (l > 1.f) l = 1.f; if (l < -1.f) l = -1.f;
    if (r > 1.f) r = 1.f; if (r < -1.f) r = -1.f;
    s->pcm[i * 2] = (int16_t)(l * 32000.f);
    s->pcm[i * 2 + 1] = (int16_t)(r * 32000.f);
}
static inline float get_l(const Sound *s, int i) {
    if (!s || i < 0 || i >= s->frames) return 0.f;
    return (float)s->pcm[i * 2] / 32768.f;
}
static inline float get_r(const Sound *s, int i) {
    if (!s || i < 0 || i >= s->frames) return 0.f;
    return (float)s->pcm[i * 2 + 1] / 32768.f;
}

// ---------------------------------------------------------------- synth cores
typedef struct {
    Rng rng;
    float lp1, lp2, hp;
} GenState;

static inline float lp(GenState *g, float x, float a) {
    g->lp1 += (x - g->lp1) * a;
    return g->lp1;
}

void snd_gen_shot(Sound *s, float base_hz, float noise, float decay, float thump) {
    Rng rng;
    rng_seed(&rng, (uint64_t)(base_hz * 13.f) + (uint64_t)(noise * 997.f));
    float ph = 0.f, ph2 = 0.f;
    float lpstate = 0.f, lpstate2 = 0.f;
    int n = s->frames;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = expf(-t * decay * 9.f);
        float env2 = expf(-t * decay * 42.f);
        float nz = fast_noise(&rng);
        lpstate += (nz - lpstate) * 0.32f;
        lpstate2 += (nz - lpstate2) * 0.06f;
        float crack = (nz - lpstate) * env2 * noise * 0.9f;
        float body = lpstate * env * noise * 1.2f;
        float f = base_hz * (1.f + 6.f * expf(-t * 26.f));
        ph += f / (float)AUD_RATE;
        ph2 += f * 0.5f / (float)AUD_RATE;
        float th = (sinf(ph * LN2PI) * 0.55f + sinf(ph2 * LN2PI) * 0.3f) * expf(-t * decay * 12.f) * thump;
        float tail = lpstate2 * expf(-t * 2.2f) * 0.25f;
        float l = body * 0.6f + crack + th + tail * 0.6f;
        float r = body * 0.55f + crack * 0.85f + th + tail * 0.8f;
        put(s, i, l, r);
    }
}

void snd_gen_melee(Sound *s, float whoosh_hz, float impact) {
    Rng rng;
    rng_seed(&rng, (uint64_t)(whoosh_hz * 17.f));
    float ph = 0.f, lpstate = 0.f, lp2 = 0.f;
    int n = s->frames;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float nz = fast_noise(&rng);
        lpstate += (nz - lpstate) * 0.09f;
        lp2 += (lpstate - lp2) * 0.2f;
        float whistle = sinf(ph * LN2PI);
        float f = whoosh_hz * (0.35f + 1.4f * t);
        ph += f / (float)AUD_RATE;
        float whoosh = (lp2 * 0.9f + whistle * 0.06f) * expf(-powf((t - 0.35f) * 4.2f, 2.f) * 3.f);
        float hit = 0.f;
        if (t > 0.28f) {
            float tt = (t - 0.28f) / 0.72f;
            float env = expf(-tt * 16.f);
            float burst = (nz - lpstate * 0.5f) * env;
            float tone = sinf(tt * 120.f * LN2PI) * expf(-tt * 22.f);
            hit = (burst * 1.1f + tone * 0.8f) * impact;
        }
        put(s, i, whoosh * 0.8f + hit, whoosh * 0.8f + hit * 0.95f);
    }
}

static void formant_vowel(float x, float *out0, float *out1, float *out2, float f1, float f2, float f3) {
    // simple additive approximation of formants
    *out0 = sinf(x * f1 / (float)AUD_RATE * LN2PI);
    *out1 = sinf(x * f2 / (float)AUD_RATE * LN2PI);
    *out2 = sinf(x * f3 / (float)AUD_RATE * LN2PI);
}

void snd_gen_groan(Sound *s, float voice_hz, float growl, float duration, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 7919u + 13);
    int n = s->frames;
    float phase = 0.f, phase2 = 0.f;
    float lpv = 0.f;
    float wobble = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float tt = t * duration;
        float env = sinf(saturatef(t * 1.6f) * (float)M_PI * 0.5f) * (1.f - smoothstepf(0.55f, 1.f, t));
        // pitch contour: rising then falling, unstable
        float f = voice_hz * (0.82f + 0.35f * sinf(tt * 1.7f + seed) + growl * 0.18f * sinf(tt * 11.f));
        wobble += (fast_noise(&rng) - wobble) * 0.0025f;
        f *= 1.f + wobble * growl * 1.4f;
        phase += f / (float)AUD_RATE;
        phase2 += f * 1.5f / (float)AUD_RATE;
        // vocal fold buzz with formants at ~600/1100/2600
        float x = phase;
        float buzz = sinf(x * LN2PI) * 0.6f + sinf(x * 2 * LN2PI) * 0.25f + sinf(x * 3 * LN2PI) * 0.15f +
                     sinf(x * 4 * LN2PI) * 0.1f;
        float f1 = sinf(x * 6.2f * LN2PI) * 0.35f;   // ~650 Hz formant
        float f2 = sinf(x * 11.f * LN2PI) * 0.18f;   // ~1150
        lpv += (buzz - lpv) * 0.22f;
        float breath = (fast_noise(&rng) * 0.35f + lpv * 0.65f);
        float src = breath * 0.55f + f1 * 0.5f + f2 * 0.3f;
        src = src * (1.f + growl * 0.8f * sinf(phase2 * 0.6f * LN2PI));
        float out = src * env * 0.55f;
        put(s, i, out, out * 0.93f);
    }
}

void snd_gen_scream(Sound *s, float pitch, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 104729u);
    int n = s->frames;
    float ph = 0.f, lp = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = sinf(saturatef(t * 6.f) * (float)M_PI * 0.5f) * (1.f - smoothstepf(0.6f, 1.f, t));
        float f = pitch * (1.15f + 0.9f * sinf(t * 3.1f) + 0.25f * fast_noise(&rng) * 0.04f);
        ph += f / (float)AUD_RATE;
        float s1 = sinf(ph * LN2PI);
        float s2 = sinf(ph * 2.02f * LN2PI) * 0.5f;
        float s3 = sinf(ph * 3.1f * LN2PI) * 0.3f;
        lp += (s1 + s2 + s3 - lp) * 0.5f;
        float rasp = fast_noise(&rng) * 0.25f * expf(-t * 3.f);
        float out = (lp * 0.6f + rasp) * env * 0.5f;
        put(s, i, out, out);
    }
}

void snd_gen_step(Sound *s, float tone, int seed, float foot) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 6151u + 7);
    int n = s->frames;
    float lpstate = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = expf(-t * 28.f);
        float nz = fast_noise(&rng);
        lpstate += (nz - lpstate) * (0.25f + foot * 0.4f);
        float thump = sinf(t * 60.f * LN2PI) * expf(-t * 26.f);
        float scuff = lpstate * expf(-t * 12.f);
        float out = (scuff * 0.8f + thump * 0.5f * tone) * env;
        put(s, i, out, out * 0.9f);
    }
}

void snd_gen_thunder(Sound *s, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 31337u);
    int n = s->frames;
    float lp1 = 0.f, lp2 = 0.f, lp3 = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float nz = fast_noise(&rng);
        lp1 += (nz - lp1) * 0.012f;
        lp2 += (nz - lp2) * 0.045f;
        lp3 += (lp1 - lp3) * 0.3f;
        float env = (1.f - expf(-t * 22.f)) * expf(-t * 2.2f);
        float rumble = (lp1 * 3.4f + lp2 * 0.9f + lp3 * 1.6f) * env;
        float crack = 0.f;
        if (t < 0.05f) crack = fast_noise(&rng) * expf(-t * 90.f) * 0.8f;
        float out = (rumble * 0.85f + crack) * 0.8f;
        put(s, i, out, out * 0.92f);
    }
}

void snd_gen_rain(Sound *s, float intensity, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 7919u);
    int n = s->frames;
    float lpl = 0.f, lpr = 0.f, hp = 0.f;
    for (int i = 0; i < n; i++) {
        float nz = fast_noise(&rng);
        float nz2 = fast_noise(&rng);
        lpl += (nz - lpl) * 0.30f;
        lpr += (nz2 - lpr) * 0.30f;
        hp += (nz - hp) * 0.01f;
        float drops = (nz - hp) * 0.10f;
        float body = (lpl + lpr) * 0.16f;
        float out = (body + drops) * intensity;
        put(s, i, out, (lpr + lpl * 0.7f) * 0.16f * intensity + drops * 0.9f);
    }
}

void snd_gen_click(Sound *s, float hz, float dur) {
    Rng rng;
    rng_seed(&rng, 12345);
    int n = s->frames;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = expf(-t * 40.f);
        float tone = sinf(t * dur * hz * LN2PI) * 0.6f;
        float nz = fast_noise(&rng) * 0.4f * expf(-t * 90.f);
        put(s, i, (tone + nz) * env, (tone + nz) * env);
    }
}

void snd_gen_impact(Sound *s, float hz, float dur) {
    Rng rng;
    rng_seed(&rng, 777);
    int n = s->frames;
    float lp = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = expf(-t * 14.f);
        float nz = fast_noise(&rng);
        lp += (nz - lp) * 0.4f;
        float tone = sinf(t * dur * hz * LN2PI) * expf(-t * 9.f);
        float flesh = lp * expf(-t * 20.f) * 0.6f;
        put(s, i, (tone * 0.7f + flesh) * env, (tone * 0.6f + flesh * 0.9f) * env);
    }
}

void snd_gen_break_wood(Sound *s, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 3571u + 3);
    int n = s->frames;
    float lpa = 0.f, lpb = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float nz = fast_noise(&rng);
        lpa += (nz - lpa) * 0.55f;
        lpb += (nz - lpb) * 0.12f;
        float crack1 = (nz - lpa) * expf(-t * 30.f) * 1.2f;
        float crack2 = (nz - lpb) * expf(-t * 9.f) * 0.7f;
        float wood = sinf(t * 180.f * LN2PI) * expf(-t * 24.f) * 0.35f;
        float splinter = 0.f;
        if (t > 0.15f && t < 0.5f) splinter = (nz - lpa) * expf(-(t - 0.15f) * 22.f) * 0.6f;
        float out = crack1 + crack2 * 0.7f + wood + splinter;
        put(s, i, out, out * 0.94f);
    }
}

void snd_gen_metal(Sound *s, float hz, float dur) {
    Rng rng;
    rng_seed(&rng, 999);
    int n = s->frames;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = expf(-t * 8.f);
        float a = sinf(t * dur * hz * LN2PI) * 0.4f;
        float b = sinf(t * dur * hz * 2.41f * LN2PI) * 0.24f;
        float c = sinf(t * dur * hz * 3.77f * LN2PI) * 0.14f;
        float nz = fast_noise(&rng) * 0.2f * expf(-t * 25.f);
        float out = (a + b + c + nz) * env;
        put(s, i, out, out * 0.92f);
    }
}

void snd_gen_heartbeat(Sound *s) {
    int n = s->frames;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float b1 = expf(-powf((t - 0.15f) * 22.f, 2.f));
        float b2 = expf(-powf((t - 0.40f) * 26.f, 2.f)) * 0.7f;
        float env = b1 + b2;
        float tone = sinf(t * 55.f * LN2PI) * 0.8f + sinf(t * 90.f * LN2PI) * 0.25f;
        float out = tone * env * 0.6f;
        put(s, i, out, out);
    }
}

void snd_gen_stinger(Sound *s, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 887u + 17);
    int n = s->frames;
    float ph[5] = {0};
    static const float ratios[5] = {1.f, 1.19f, 1.5f, 2.01f, 2.51f};
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = (1.f - expf(-t * 30.f)) * expf(-t * 1.6f);
        float acc = 0.f;
        for (int k = 0; k < 5; k++) {
            float f = (110.f + seed * 7.f) * ratios[k] * (1.f + 0.01f * sinf(t * 6.f + k));
            ph[k] += f / (float)AUD_RATE;
            acc += sinf(ph[k] * LN2PI) * (1.f / (1.f + k * 0.7f));
        }
        float scrape = fast_noise(&rng) * 0.12f * expf(-t * 4.f);
        float out = (acc * 0.18f + scrape) * env;
        put(s, i, out, out * 0.9f);
    }
}

void snd_gen_hum(Sound *s, float hz, float dur) {
    int n = s->frames;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float tt = t * dur;
        float a = sinf(tt * hz * LN2PI) * 0.32f;
        float b = sinf(tt * hz * 2.f * LN2PI) * 0.16f;
        float c = sinf(tt * hz * 3.f * LN2PI) * 0.06f;
        float mod = 0.9f + 0.1f * sinf(tt * 0.7f * LN2PI);
        float out = (a + b + c) * mod * 0.7f;
        put(s, i, out, out);
    }
}

void snd_gen_reload(Sound *s, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 61u + 5);
    int n = s->frames;
    float lp = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float out = 0.f;
        // magazine out, mag in, bolt
        float marks[3] = {0.10f, 0.42f, 0.78f};
        float amps[3] = {0.8f, 1.0f, 0.9f};
        for (int k = 0; k < 3; k++) {
            float d = t - marks[k];
            if (d > 0.f && d < 0.09f) {
                float env = expf(-d * 60.f);
                float nz = fast_noise(&rng);
                lp += (nz - lp) * 0.6f;
                float clack = (nz - lp) * 0.5f + sinf(d * 900.f * LN2PI) * 0.25f;
                out += clack * env * amps[k];
            }
        }
        put(s, i, out, out * 0.92f);
    }
}

void snd_gen_pickup(Sound *s) {
    Rng rng;
    rng_seed(&rng, 3);
    int n = s->frames;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = expf(-t * 12.f);
        float a = sinf(t * 660.f * LN2PI) * 0.3f;
        float b = sinf(t * 990.f * LN2PI) * 0.2f;
        float nz = fast_noise(&rng) * 0.1f * expf(-t * 30.f);
        put(s, i, (a + b + nz) * env, (a + b + nz) * env);
    }
}

void snd_gen_door(Sound *s, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 911u);
    int n = s->frames;
    float lp = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float creak = sinf(t * (90.f + 40.f * sinf(t * 4.f)) * LN2PI) * 0.35f;
        float nz = fast_noise(&rng);
        lp += (nz - lp) * 0.1f;
        float env = smoothstepf(0.f, 0.15f, t) * (1.f - smoothstepf(0.7f, 1.f, t));
        float slam = 0.f;
        if (t > 0.85f) slam = (nz - lp) * expf(-(t - 0.85f) * 40.f) * 1.2f;
        float out = (creak * 0.5f + lp * 0.5f) * env + slam;
        put(s, i, out, out);
    }
}

void snd_gen_explosion(Sound *s, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 4409u);
    int n = s->frames;
    float lp1 = 0.f, lp2 = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float nz = fast_noise(&rng);
        lp1 += (nz - lp1) * 0.06f;
        lp2 += (nz - lp2) * 0.25f;
        float env = expf(-t * 4.5f);
        float boom = sinf(t * 42.f * LN2PI) * expf(-t * 9.f) * 0.8f;
        float out = (lp1 * 2.6f + lp2 * 0.7f + boom) * env;
        put(s, i, out, out * 0.9f);
    }
}

void snd_gen_roar(Sound *s, int seed) {
    Rng rng;
    rng_seed(&rng, (uint64_t)seed * 1279u);
    int n = s->frames;
    float ph = 0.f, lp = 0.f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = sinf(saturatef(t * 3.f) * (float)M_PI * 0.5f) * (1.f - smoothstepf(0.65f, 1.f, t));
        float f = 62.f * (0.9f + 0.25f * sinf(t * 8.f)) * (1.f + 0.12f * fast_noise(&rng) * 0.3f);
        ph += f / (float)AUD_RATE;
        float src = sinf(ph * LN2PI) + sinf(ph * 2.01f * LN2PI) * 0.6f + sinf(ph * 3.03f * LN2PI) * 0.4f +
                    sinf(ph * 4.7f * LN2PI) * 0.3f;
        lp += (src - lp) * 0.35f;
        float nz = fast_noise(&rng) * 0.25f;
        float out = (lp * 0.5f + nz) * env * 0.8f;
        put(s, i, out, out * 0.95f);
    }
}

// ---------------------------------------------------------------- WAV
static uint32_t rd32le(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

Sound *snd_from_wav(const char *path, const char *name) {
    size_t sz = 0;
    uint8_t *d = file_read_all(path, &sz);
    if (!d) { LOGW("audio: missing %s", path); return NULL; }
    if (sz < 44 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
        LOGE("audio: bad wav %s", path);
        free(d);
        return NULL;
    }
    size_t pos = 12;
    int channels = 2, bits = 16, rate = AUD_RATE, data_off = 0, data_len = 0;
    while (pos + 8 <= sz) {
        const uint8_t *cid = d + pos;
        uint32_t clen = rd32le(d + pos + 4);
        if (!memcmp(cid, "fmt ", 4)) {
            channels = rd16le(d + pos + 10);
            rate = (int)rd32le(d + pos + 12);
            bits = rd16le(d + pos + 22);
        } else if (!memcmp(cid, "data", 4)) {
            data_off = (int)pos + 8;
            data_len = (int)clen;
            if (data_off + data_len > (int)sz) data_len = (int)sz - data_off;
        }
        pos += 8 + clen + (clen & 1);
    }
    if (!data_off || data_len <= 0) { free(d); return NULL; }
    int bps = channels * bits / 8;
    if (bps <= 0) { free(d); return NULL; }
    int frames = data_len / bps;
    Sound *s = snd_make(name ? name : path, frames);
    s->rate = rate;
    for (int i = 0; i < frames; i++) {
        const uint8_t *p = d + data_off + i * bps;
        float l, rr;
        if (bits == 16) {
            l = (float)(int16_t)rd16le(p) / 32768.f;
            rr = channels >= 2 ? (float)(int16_t)rd16le(p + 2) / 32768.f : l;
        } else if (bits == 8) {
            l = ((float)p[0] - 128.f) / 128.f;
            rr = channels >= 2 ? ((float)p[1] - 128.f) / 128.f : l;
        } else {
            l = rr = 0.f;
        }
        put(s, i, l, rr);
    }
    free(d);
    return s;
}

// ---------------------------------------------------------------- mixer
int aud_init(Audio *a) {
    memset(a, 0, sizeof *a);
    a->master = 0.85f;
    a->sfx = 1.f;
    a->ambience = 0.8f;
    a->music = 0.7f;
    a->ui = 0.85f;
    a->reverb_mix = 0.12f;
    rng_seed(&a->rng, 4242);
    a->listener.pos = v3(0, 1.7f, 0);
    a->listener.forward = v3(0, 0, 1);
    a->listener.right = v3(1, 0, 0);
    return 1;
}
void aud_shutdown(Audio *a) { (void)a; }

void aud_set_listener(Audio *a, Vec3 pos, Vec3 forward, Vec3 up) {
    (void)up;
    a->listener.pos = pos;
    a->listener.forward = vnorm(forward);
    Vec3 r = vcross(v3(0, 1, 0), a->listener.forward);
    if (vlen2(r) < 1e-5f) r = v3(1, 0, 0);
    a->listener.right = vnorm(r);
}

int aud_play(Audio *a, const Sound *s, Vec3 *pos, float volume, float pitch, int loop, int bus) {
    if (!s || s->frames <= 0 || volume <= 0.001f) return -1;
    int slot = -1;
    for (int i = 0; i < AUD_MAX_VOICES; i++) {
        if (!a->voices[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        // steal the quietest
        float best = 1e9f;
        for (int i = 0; i < AUD_MAX_VOICES; i++) {
            float v = a->voices[i].vol_l + a->voices[i].vol_r;
            if (v < best) { best = v; slot = i; }
        }
    }
    Voice *v = &a->voices[slot];
    memset(v, 0, sizeof *v);
    v->snd = s;
    v->active = 1;
    v->loop = loop;
    v->bus = bus;
    v->pitch = clampf(pitch, 0.20f, 4.f);
    v->rate = v->pitch * (float)s->rate / (float)AUD_RATE;
    v->pos_t = 0.f;
    float vol = volume;
    float pan = 0.f;
    if (pos) {
        Vec3 d = vsub(*pos, a->listener.pos);
        float dist = vlen(d);
        if (dist > 0.05f) {
            Vec3 dir = vscale(d, 1.f / dist);
            pan = clampf(vdot(dir, a->listener.right), -1.f, 1.f);
        }
        float att = 1.f / (1.f + dist * 0.16f + dist * dist * 0.012f);
        // simple occlusion-ish muffle: quieter when far + low
        vol *= att;
    }
    float l = vol * (0.72f - pan * 0.28f);
    float r = vol * (0.72f + pan * 0.28f);
    v->vol_l = l;
    v->vol_r = r;
    v->pitch_l = v->rate;
    v->pitch_r = v->rate;
    return slot;
}
void aud_stop(Audio *a, int voice) {
    if (voice < 0 || voice >= AUD_MAX_VOICES) return;
    a->voices[voice].active = 0;
}
void aud_stop_all(Audio *a, int bus) {
    for (int i = 0; i < AUD_MAX_VOICES; i++)
        if (a->voices[i].active && (bus < 0 || a->voices[i].bus == bus)) a->voices[i].active = 0;
}

void aud_mix(Audio *a, int16_t *out, int frames) {
    float bus_gain[4] = {a->sfx, a->ambience, a->music, a->ui};
    // clear
    for (int i = 0; i < frames * 2; i++) out[i] = 0;
    static float accum[AUD_RATE * 2 / 44 + 4096];  // scratch, ~1024 frames
    int maxf = frames * 2;
    if (maxf > (int)(sizeof(accum) / sizeof(float))) maxf = (int)(sizeof(accum) / sizeof(float));
    memset(accum, 0, sizeof(float) * maxf);

    for (int vi = 0; vi < AUD_MAX_VOICES; vi++) {
        Voice *v = &a->voices[vi];
        if (!v->active || !v->snd) continue;
        float g = bus_gain[v->bus & 3] * a->master;
        int n = frames;
        for (int i = 0; i < n; i++) {
            int idx = (int)v->pos_t;
            if (idx >= v->snd->frames) {
                if (v->loop) {
                    v->pos_t = v->pos_t - (float)v->snd->frames;
                    idx = (int)v->pos_t;
                } else {
                    v->active = 0;
                    break;
                }
            }
            float frac = v->pos_t - (float)idx;
            float l0 = get_l(v->snd, idx), l1 = get_l(v->snd, idx + 1);
            float r0 = get_r(v->snd, idx), r1 = get_r(v->snd, idx + 1);
            float sl = l0 + (l1 - l0) * frac;
            float sr = r0 + (r1 - r0) * frac;
            accum[i * 2] += sl * v->vol_l * g;
            accum[i * 2 + 1] += sr * v->vol_r * g;
            v->pos_t += v->pitch_l;
        }
    }
    // soft clip + tiny room reverb (single delay tap)
    const int dly = AUD_RATE / 24;
    static float dl[4096], dr[4096];
    static int dp = 0;
    for (int i = 0; i < frames; i++) {
        float l = accum[i * 2], r = accum[i * 2 + 1];
        if (a->reverb_mix > 0.f) {
            int di = (dp + i) & 4095;
            float wl = dl[di], wr = dr[di];
            dl[di] = (l + wr * 0.35f) * 0.55f;
            dr[di] = (r + wl * 0.35f) * 0.55f;
            l += wl * a->reverb_mix * 2.2f;
            r += wr * a->reverb_mix * 2.2f;
        }
        // gentle limiter
        l = tanhf(l * 1.15f);
        r = tanhf(r * 1.15f);
        out[i * 2] = (int16_t)(clampf(l, -1.f, 1.f) * 30000.f);
        out[i * 2 + 1] = (int16_t)(clampf(r, -1.f, 1.f) * 30000.f);
    }
    dp = (dp + frames) & 4095;
}
