// ============================================================
// SPACE MINER  -  audio.cpp
// Fully synthesised sound: no audio files ship with the game.
// The synthesiser renders its own voices; the platform layer
// pulls the sample buffer.
// ============================================================
#include "platform.h"
#include <atomic>

namespace sm {
namespace audio {

// The mixer runs on the platform's audio callback thread while the game
// thread fires sounds, so every entry point takes this spinlock.
static std::atomic_flag gAudioBusy = ATOMIC_FLAG_INIT;
struct BusyGuard {
    BusyGuard() { while (gAudioBusy.test_and_set(std::memory_order_acquire)) {} }
    ~BusyGuard() { gAudioBusy.clear(std::memory_order_release); }
};

static const float SR = 44100.f;
static std::atomic<float> gMaster{0.8f};
static std::atomic<float> gSfx{0.9f};
static std::atomic<float> gMusic{0.0f};
static std::atomic<float> gAmbient{0.0f};
static float musicPhase = 0, musicPhase2 = 0, padPhase[3] = {0, 0, 0};
static float ambientPhase = 0;
static float musicLevel = 0.f, ambientLevel = 0.f;
static double noiseState = 0.123456789;
static float lastNoise = 0.f;

// ---------------------------------------------------------------- voices
enum VoiceType {
    V_LASER, V_MINING, V_EXPLOSION, V_PICKUP, V_HIT, V_UI_CLICK, V_UI_HOVER,
    V_SELL, V_UPGRADE, V_ALARM, V_WARP, V_ENEMY_SHOT, V_DRONE, V_COUNT
};

struct Voice {
    bool active = false;
    int type = 0;
    float t = 0.f;        // seconds since start
    float dur = 0.f;
    float p1 = 0.f, p2 = 0.f, p3 = 0.f;
    float phase = 0.f, phase2 = 0.f;
    float level = 1.f;
};

static const int MAX_VOICES = 24;
static Voice voices[MAX_VOICES];
static int nextVoice = 0;

static float noise() {
    noiseState = noiseState * 1.0 + 0.0;
    // xorshift for cheap white noise
    static uint32_t s = 0x12345678u;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    lastNoise = ((s & 0xFFFF) / 32768.f) - 1.f;
    return lastNoise;
}

static void startVoice(int type, float p1, float p2, float dur, float level) {
    Voice& v = voices[nextVoice];
    nextVoice = (nextVoice + 1) % MAX_VOICES;
    v.active = true;
    v.type = type;
    v.t = 0;
    v.dur = dur;
    v.p1 = p1;
    v.p2 = p2;
    v.p3 = 0;
    v.phase = 0;
    v.phase2 = 0;
    v.level = level;
}

// renders the contribution of one voice (mono)
static float renderVoice(Voice& v, float dt) {
    if (!v.active) return 0.f;
    v.t += dt;
    float t = v.t;
    float prog = t / std::max(.001f, v.dur);
    if (prog >= 1.f) { v.active = false; return 0.f; }
    float env = 0.f;
    float s = 0.f;
    switch (v.type) {
        case V_LASER: {
            float f = 1400.f * (1.f - prog * .55f);
            v.phase += TAU * f * dt;
            s = sinf(v.phase) * .55f + (noise() * .18f);
            env = powf(1.f - prog, 3.f);
            break;
        }
        case V_MINING: {
            // looping-ish crackle: short burst
            float f = 220.f + sinf(t * 90.f) * 40.f;
            v.phase += TAU * f * dt;
            s = (sinf(v.phase) * .3f + noise() * .5f) * (.6f + .4f * sinf(t * 140.f));
            env = sinf(PI * prog) * .9f;
            break;
        }
        case V_EXPLOSION: {
            float cutoffTone = 1.f - prog;
            v.phase += TAU * (70.f + 40.f * cutoffTone) * dt;
            float rumb = sinf(v.phase) * .5f;
            float nz = noise() * powf(1.f - prog, 1.6f);
            s = rumb * .6f + nz * .9f;
            env = powf(1.f - prog, 1.8f) * v.p1;
            break;
        }
        case V_PICKUP: {
            float f = 660.f + prog * 900.f;
            v.phase += TAU * f * dt;
            s = sinf(v.phase) * .6f;
            env = powf(1.f - prog, 2.2f);
            break;
        }
        case V_HIT: {
            v.phase += TAU * (180.f - prog * 120.f) * dt;
            s = sinf(v.phase) * .5f + noise() * .6f;
            env = powf(1.f - prog, 2.5f);
            break;
        }
        case V_UI_CLICK: {
            v.phase += TAU * 900.f * dt;
            v.phase2 += TAU * 1350.f * dt;
            s = (sinf(v.phase) * .4f + sinf(v.phase2) * .25f);
            env = powf(1.f - prog, 4.f);
            break;
        }
        case V_UI_HOVER: {
            v.phase += TAU * 1800.f * dt;
            s = sinf(v.phase) * .3f;
            env = powf(1.f - prog, 5.f);
            break;
        }
        case V_SELL: {
            // coin cascade
            float f = 880.f * (1.f + floorf(prog * 4.f) * .25f);
            v.phase += TAU * f * dt;
            s = sinf(v.phase) * .45f;
            env = powf(1.f - prog, 1.6f) * (.7f + .3f * sinf(t * 60.f));
            break;
        }
        case V_UPGRADE: {
            float f = 320.f + prog * 640.f;
            v.phase += TAU * f * dt;
            v.phase2 += TAU * f * 1.5f * dt;
            s = (sinf(v.phase) * .4f + sinf(v.phase2) * .2f);
            env = sinf(PI * powf(prog, .7f));
            break;
        }
        case V_ALARM: {
            v.phase += TAU * (520.f + sinf(t * 18.f) * 90.f) * dt;
            s = (sinf(v.phase) > 0 ? .35f : -.35f);
            env = powf(1.f - prog, 1.2f);
            break;
        }
        case V_WARP: {
            float f = 120.f + prog * 1400.f;
            v.phase += TAU * f * dt;
            s = sinf(v.phase) * .4f + noise() * .35f * prog;
            env = sinf(PI * prog) * .9f;
            break;
        }
        case V_ENEMY_SHOT: {
            float f = 900.f * (1.f - prog * .7f);
            v.phase += TAU * f * dt;
            s = sinf(v.phase) * .35f + noise() * .2f;
            env = powf(1.f - prog, 2.6f);
            break;
        }
        case V_DRONE: {
            v.phase += TAU * (520.f + sinf(t * 30.f) * 60.f) * dt;
            s = sinf(v.phase) * .3f;
            env = sinf(PI * prog);
            break;
        }
        default: break;
    }
    return s * env * v.level;
}

void init() {}
void shutdown() {}

void setMasterVolume(float v) { gMaster.store(clampf(v, 0.f, 1.f)); }
float masterVolume() { return gMaster.load(); }
void setMusicIntensity(float v) { gMusic.store(clampf(v, 0.f, 1.f)); }
void setAmbient(float mood) { gAmbient.store(clampf(mood, 0.f, 1.f)); }

void playLaser() { BusyGuard busy; startVoice(V_LASER, 0, 0, .16f, .5f); }
void playMining() { BusyGuard busy; startVoice(V_MINING, 0, 0, .07f, .18f); }
void playExplosion(float size) { BusyGuard busy; startVoice(V_EXPLOSION, clampf(size, .3f, 2.f), 0, .55f + size * .25f, .75f); }
void playPickup() { BusyGuard busy; startVoice(V_PICKUP, 0, 0, .12f, .34f); }
void playHit() { BusyGuard busy; startVoice(V_HIT, 0, 0, .18f, .5f); }
void playUiClick() { BusyGuard busy; startVoice(V_UI_CLICK, 0, 0, .09f, .3f); }
void playUiHover() { BusyGuard busy; startVoice(V_UI_HOVER, 0, 0, .05f, .12f); }
void playSell() { BusyGuard busy; startVoice(V_SELL, 0, 0, .34f, .42f); }
void playUpgrade() { BusyGuard busy; startVoice(V_UPGRADE, 0, 0, .42f, .40f); }
void playAlarm() { BusyGuard busy; startVoice(V_ALARM, 0, 0, .35f, .35f); }
void playWarp() { BusyGuard busy; startVoice(V_WARP, 0, 0, .85f, .45f); }
void playEnemyShot() { BusyGuard busy; startVoice(V_ENEMY_SHOT, 0, 0, .14f, .22f); }
void playDroneBeep() { BusyGuard busy; startVoice(V_DRONE, 0, 0, .22f, .28f); }

void update(float dt) {
    BusyGuard busy;
    musicLevel = lerpf(musicLevel, gMusic.load(), clampf(dt * 2.f, 0.f, 1.f));
    ambientLevel = lerpf(ambientLevel, gAmbient.load(), clampf(dt * .7f, 0.f, 1.f));
    // keep music phase advancing so the pad never clicks
    musicPhase += TAU * 55.f * dt;
    musicPhase2 += TAU * 82.5f * dt;
    for (int i = 0; i < 3; i++) padPhase[i] += TAU * (55.f * (1.f + i * .5f)) * dt;
    ambientPhase += TAU * .35f * dt;
}

// Called by the platform audio callback; fills interleaved stereo.
void render(float* out, int frames, int channels) {
    BusyGuard busy;
    float master = gMaster.load();
    float sfx = gSfx.load();
    for (int i = 0; i < frames; i++) {
        float dt = 1.f / SR;
        float s = 0.f;
        for (int v = 0; v < MAX_VOICES; v++) s += renderVoice(voices[v], dt);
        // ambient bed: slow filtered noise + low drone, scaled by sector danger
        static float ambLp = 0.f;
        if (ambientLevel > .001f) {
            float n = noise();
            ambLp = lerpf(ambLp, n, .002f);
            float drone = sinf(ambientPhase) * .05f + sinf(ambientPhase * 1.49f) * .03f;
            float storm = sinf(ambientPhase * .13f) * .5f + .5f;
            s += (ambLp * .5f * (0.25f + ambientLevel) + drone * (0.4f + ambientLevel)) * ambientLevel * 1.4f;
            if (storm > .93f) s += noise() * (storm - .93f) * 4.f * ambientLevel * .25f;
        }
        if (musicLevel > .001f) {
            float pad = sinf(padPhase[0]) * .32f + sinf(padPhase[1]) * .20f + sinf(padPhase[2]) * .12f;
            pad += sinf(musicPhase) * .06f + sinf(musicPhase2) * .04f;
            s += pad * musicLevel * .16f;
        }
        s = tanhf(s * 1.15f) * master;
        for (int c = 0; c < channels; c++) out[i * channels + c] = s * (c == 0 ? 1.f : .97f);
    }
    (void)sfx;
}

}  // namespace audio
}  // namespace sm
