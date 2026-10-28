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
    std::wstring nowPlaying;        // Active external audio session title (#5)
};

class AudioReactiveEngine {
public:
    AudioReactiveEngine();
    ~AudioReactiveEngine();

    void SetEnabled(bool captureSpectrum, bool detectMediaTitle, float sensitivity);
    void Shutdown();

    AudioSpectrumSnapshot GetSnapshot() const;

    // Deterministic FFT & band-mapping verification helper for --self-test
    static AudioSpectrumSnapshot ComputeTestFftSnapshot(float sampleRate, float freq1Hz, float freq2Hz, float sensitivity);

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
    std::atomic<float> sensitivity_{1.15f};
    std::thread worker_;

    mutable std::mutex snapMutex_;
    AudioSpectrumSnapshot snapshot_{};
};

}  // namespace wp
