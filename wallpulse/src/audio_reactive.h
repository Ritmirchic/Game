#pragma once

#include "common.h"

#include <array>
#include <atomic>
#include <thread>

namespace wp {

struct AudioSpectrumSnapshot {
    std::array<float, 32> bands{};  // 0.0 .. 1.0 smoothed logarithmic bands
    float bass = 0.0f;              // 0.0 .. 1.0 low-end beat energy
    float mid = 0.0f;               // 0.0 .. 1.0 mid-range energy
    float high = 0.0f;              // 0.0 .. 1.0 treble energy
    bool deviceActive = false;
    uint32_t hotSwitchCount = 0;    // Number of IMMNotificationClient endpoint hot-switches
    std::wstring nowPlaying;        // Active GSMTC / background media session Artist — Title
};

class AudioReactiveEngine {
public:
    AudioReactiveEngine();
    ~AudioReactiveEngine();

    void SetEnabled(bool captureSpectrum, bool detectMediaTitle, float sensitivity);
    void Shutdown();

    AudioSpectrumSnapshot GetSnapshot() const;

    // Triggers an immediate endpoint rebind (used by IMMNotificationClient & --self-test)
    void TriggerEndpointHotSwitch();
    uint32_t EndpointHotSwitchCount() const { return hotSwitchCount_.load(); }

    // Deterministic FFT & band-mapping verification helper for --self-test
    static AudioSpectrumSnapshot ComputeTestFftSnapshot(float sampleRate, float freq1Hz, float freq2Hz, float sensitivity);

    // Safe one-shot GSMTC + AudioSession query helper (also callable in --self-test)
    static std::wstring ProbeSystemNowPlaying();

private:
    void WorkerLoop();
    static void ProcessPcmRingToSpectrum(
        const float* ring512,
        size_t writePos,
        float sampleRate,
        float sensitivity,
        std::array<float, 32>& ioBands,
        float& ioBass,
        float& ioMid,
        float& ioHigh);

private:
    std::atomic<bool> running_{false};
    std::atomic<bool> wantSpectrum_{false};
    std::atomic<bool> wantMediaTitle_{false};
    std::atomic<bool> endpointDirty_{false};
    std::atomic<uint32_t> hotSwitchCount_{0};
    std::atomic<float> sensitivity_{1.15f};
    std::thread worker_;

    mutable std::mutex snapMutex_;
    AudioSpectrumSnapshot snapshot_{};
};

}  // namespace wp
