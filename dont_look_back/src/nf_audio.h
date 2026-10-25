// ============================================================================
//  nf_audio.h : 3D audio mixer, WAV loading, reverb, waveOut device
// ============================================================================
#pragma once
#include "nf_core.h"
#include <mutex>
#include <atomic>

namespace nf {

struct SoundData {
    std::vector<float> samples;   // mono, native rate
    int rate = 44100;
    bool loaded = false;
};

enum class Bus { SFX = 0, AMBIENT = 1, VOICE = 2, MUSIC = 3, UI = 4, COUNT = 5 };

struct SoundId { int v = -1; bool valid() const { return v >= 0; } };

class Audio {
public:
    bool init(int rate = 44100);
    void shutdown();
    SoundId load(const std::string& path);              // from WAV file
    SoundId addSamples(const std::vector<float>& mono, int rate); // procedural
    const SoundData* data(SoundId id) const;
    bool loaded(SoundId id) const { return id.v >= 0 && id.v < (int)sounds_.size() && sounds_[id.v].loaded; }

    // ---- playback
    int play(SoundId id, float volume = 1.0f, float pitch = 1.0f, Bus bus = Bus::SFX,
             float pan = 0.0f, bool loop = false);
    int play3D(SoundId id, const V3& pos, float volume = 1.0f, float pitch = 1.0f,
               Bus bus = Bus::SFX, bool loop = false, float range = 14.0f);
    void stop(int handle, float fadeOut = 0.0f);
    void stopAll(float fadeOut = 0.2f);
    void setListener(const V3& pos, const V3& fwd, const V3& up);
    void setBusVolume(Bus b, float v) { busVol_[(int)b] = clampf(v, 0.0f, 2.0f); }
    float busVolume(Bus b) const { return busVol_[(int)b]; }
    void setMasterVolume(float v) { master_ = clampf(v, 0.0f, 1.5f); }
    float masterVolume() const { return master_; }
    void setReverb(float wet, float roomSize);
    bool active(int handle) const;
    float timeOf(int handle) const;
    void setHandleVolume(int handle, float v);
    void setHandlePitch(int handle, float p);
    void setHandlePos(int handle, const V3& p);
    void duckAmbient(float amount, float seconds);

    // mixer thread pull
    void mix(float* out, int frames);

    // Optional backend
    bool startDevice();
    void stopDevice();
    float cpuLoad() const { return cpuLoad_; }
    int activeVoices() const;

private:
    struct Voice {
        bool active = false;
        const SoundData* snd = nullptr;
        double pos = 0;
        float pitch = 1.0f;
        float volL = 1.0f, volR = 1.0f;
        float baseVol = 1.0f;
        float targetVol = 1.0f;
        Bus bus = Bus::SFX;
        bool loop = false;
        bool threeD = false;
        V3 worldPos;
        float range = 14.0f;
        float fadeOut = 0.0f;   // seconds remaining, 0 = none
        float fadeGain = 1.0f;
        int id = 0;
        float elapsed = 0.0f;
    };
    std::vector<SoundData> sounds_;
    std::vector<Voice> voices_;
    std::mutex mx_;
    std::atomic<bool> running_{false};
    int rate_ = 44100;
    V3 listenerPos_, listenerFwd_{0, 0, 1}, listenerUp_{0, 0, 1};
    V3 listenerRight_{1, 0, 0};
    float busVol_[(int)Bus::COUNT] = {1.0f, 0.85f, 1.0f, 0.8f, 1.0f};
    float master_ = 0.9f;
    int nextHandle_ = 1;
    // reverb
    float revWet_ = 0.22f, revRoom_ = 0.6f;
    std::vector<float> comb_[4];
    int combIdx_[4] = {0, 0, 0, 0};
    std::vector<float> ap_[2];
    int apIdx_[2] = {0, 0};
    float duck_ = 1.0f, duckTarget_ = 1.0f, duckSpeed_ = 1.0f;
    float cpuLoad_ = 0.0f;
    void* device_ = nullptr;
    void mixInternal();
    void updateVoicePan(Voice& v);
};

// ------------------------------------------------------------------ WAV io
bool loadWav(const std::string& path, SoundData& out);
bool writeWav(const std::string& path, const std::vector<float>& mono, int rate);

} // namespace nf
