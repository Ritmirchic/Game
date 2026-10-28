#include "audio_reactive.h"

#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wp {

namespace {

constexpr size_t kFftSize = 512;
constexpr float kPi = 3.14159265358979323846f;

void InPlaceRadix2Fft512(float* re, float* im) {
    // Bit-reversal permutation for N = 512
    size_t j = 0;
    for (size_t i = 1; i < kFftSize; ++i) {
        size_t bit = kFftSize >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (size_t len = 2; len <= kFftSize; len <<= 1) {
        const float ang = -2.0f * kPi / static_cast<float>(len);
        const float wlenCos = std::cos(ang);
        const float wlenSin = std::sin(ang);
        for (size_t i = 0; i < kFftSize; i += len) {
            float wCos = 1.0f;
            float wSin = 0.0f;
            const size_t half = len >> 1;
            for (size_t k = 0; k < half; ++k) {
                const float uRe = re[i + k];
                const float uIm = im[i + k];
                const float vRe = re[i + k + half] * wCos - im[i + k + half] * wSin;
                const float vIm = re[i + k + half] * wSin + im[i + k + half] * wCos;

                re[i + k] = uRe + vRe;
                im[i + k] = uIm + vIm;
                re[i + k + half] = uRe - vRe;
                im[i + k + half] = uIm - vIm;

                const float nextWCos = wCos * wlenCos - wSin * wlenSin;
                wSin = wCos * wlenSin + wSin * wlenCos;
                wCos = nextWCos;
            }
        }
    }
}

struct FindPidWindowCtx {
    DWORD targetPid = 0;
    std::wstring bestTitle;
};

BOOL CALLBACK EnumWindowForPidTitle(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<FindPidWindowCtx*>(lParam);
    if (!IsWindowVisible(hwnd)) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->targetPid) return TRUE;

    int len = GetWindowTextLengthW(hwnd);
    if (len <= 1 || len > 512) return TRUE;

    std::wstring title(static_cast<size_t>(len) + 1, L'\0');
    int got = GetWindowTextW(hwnd, &title[0], len + 1);
    if (got <= 1) return TRUE;
    title.resize(static_cast<size_t>(got));

    if (title == L"Default IME" || title == L"MSCTFIME UI") return TRUE;
    if (title.size() > ctx->bestTitle.size()) {
        ctx->bestTitle = std::move(title);
    }
    return TRUE;
}

std::wstring CleanMediaWindowTitle(std::wstring t) {
    const wchar_t* suffixes[] = {
        L" - Google Chrome",
        L" — Mozilla Firefox",
        L" - Microsoft\x200B Edge",
        L" - Microsoft Edge",
        L" - Personal - Microsoft Edge",
        L" - Яндекс Браузер",
        L" — Яндекс Браузер",
        L" - Opera",
        L" - Brave",
        L" - Медиаплеер VLC",
        L" - VLC media player",
    };
    for (const wchar_t* suf : suffixes) {
        size_t slen = wcslen(suf);
        if (t.size() > slen && t.compare(t.size() - slen, slen, suf) == 0) {
            t.erase(t.size() - slen);
            break;
        }
    }
    if (t.size() > 58) {
        t = t.substr(0, 55) + L"...";
    }
    return t;
}

std::wstring QueryActiveMediaSessionTitle(IMMDevice* endpoint) {
    if (!endpoint) return {};

    ComPtr<IAudioSessionManager2> mgr;
    HRESULT hr = endpoint->Activate(
        __uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(mgr.GetAddressOf()));
    if (FAILED(hr) || !mgr) return {};

    ComPtr<IAudioSessionEnumerator> enumerator;
    if (FAILED(mgr->GetSessionEnumerator(enumerator.GetAddressOf())) || !enumerator) {
        return {};
    }

    int count = 0;
    enumerator->GetCount(&count);
    const DWORD selfPid = GetCurrentProcessId();

    for (int i = 0; i < count; ++i) {
        ComPtr<IAudioSessionControl> ctrl;
        if (FAILED(enumerator->GetSession(i, ctrl.GetAddressOf())) || !ctrl) continue;

        AudioSessionState state = AudioSessionStateInactive;
        if (FAILED(ctrl->GetState(&state)) || state != AudioSessionStateActive) continue;

        ComPtr<IAudioSessionControl2> ctrl2;
        if (FAILED(ctrl.As(&ctrl2)) || !ctrl2) continue;
        if (ctrl2->IsSystemSoundsSession() == S_OK) continue;

        DWORD pid = 0;
        if (FAILED(ctrl2->GetProcessId(&pid)) || pid == 0 || pid == selfPid) continue;

        FindPidWindowCtx wctx{};
        wctx.targetPid = pid;
        EnumWindows(EnumWindowForPidTitle, reinterpret_cast<LPARAM>(&wctx));
        if (!wctx.bestTitle.empty()) {
            return CleanMediaWindowTitle(wctx.bestTitle);
        }

        LPWSTR dispName = nullptr;
        if (SUCCEEDED(ctrl->GetDisplayName(&dispName)) && dispName) {
            std::wstring dn(dispName);
            CoTaskMemFree(dispName);
            if (!dn.empty() && dn[0] != L'@') {
                return CleanMediaWindowTitle(dn);
            }
        }
    }
    return {};
}

bool IsWaveFormatFloat32(const WAVEFORMATEX* wfx) {
    if (!wfx) return false;
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && wfx->wBitsPerSample == 32) {
        return true;
    }
    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wfx->cbSize >= 22) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {00000003-0000-0010-8000-00aa00389b71}
        return ext->SubFormat.Data1 == 0x00000003 && wfx->wBitsPerSample == 32;
    }
    return false;
}

}  // namespace

AudioReactiveEngine::AudioReactiveEngine() = default;

AudioReactiveEngine::~AudioReactiveEngine() {
    Shutdown();
}

void AudioReactiveEngine::SetEnabled(bool captureSpectrum, bool detectMediaTitle, float sensitivity) {
    sensitivity_.store(std::clamp(sensitivity, 0.25f, 3.0f));
    wantSpectrum_.store(captureSpectrum);
    wantMediaTitle_.store(detectMediaTitle);

    bool needThread = captureSpectrum || detectMediaTitle;
    if (needThread && !running_.load()) {
        if (worker_.joinable()) worker_.join();
        running_.store(true);
        worker_ = std::thread(&AudioReactiveEngine::WorkerLoop, this);
    } else if (!needThread && running_.load()) {
        Shutdown();
    }
}

void AudioReactiveEngine::Shutdown() {
    running_.store(false);
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard<std::mutex> lk(snapMutex_);
    snapshot_ = {};
}

AudioSpectrumSnapshot AudioReactiveEngine::GetSnapshot() const {
    std::lock_guard<std::mutex> lk(snapMutex_);
    return snapshot_;
}

void AudioReactiveEngine::ProcessPcmRingToSpectrum(
    const float* ring512,
    size_t writePos,
    float sampleRate,
    float sensitivity,
    std::array<float, 32>& ioBands,
    float& ioBass,
    float& ioMid,
    float& ioHigh) {
    float re[kFftSize];
    float im[kFftSize]{};

    for (size_t i = 0; i < kFftSize; ++i) {
        float s = ring512[(writePos + i) % kFftSize];
        float hann = 0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) / static_cast<float>(kFftSize - 1)));
        re[i] = s * hann;
    }

    InPlaceRadix2Fft512(re, im);

    const float binHz = std::max(4000.0f, sampleRate) / static_cast<float>(kFftSize);
    const float minHz = 35.0f;
    const float maxHz = std::min(16000.0f, sampleRate * 0.46f);
    const float logRatio = std::log(maxHz / minHz);

    float bassAcc = 0.0f;
    float midAcc = 0.0f;
    float highAcc = 0.0f;

    for (size_t b = 0; b < 32; ++b) {
        float f0 = minHz * std::exp(logRatio * static_cast<float>(b) / 32.0f);
        float f1 = minHz * std::exp(logRatio * static_cast<float>(b + 1) / 32.0f);

        size_t k0 = std::clamp<size_t>(static_cast<size_t>(f0 / binHz), 1, kFftSize / 2 - 2);
        size_t k1 = std::clamp<size_t>(static_cast<size_t>(std::ceil(f1 / binHz)), k0 + 1, kFftSize / 2 - 1);

        float peakMag = 0.0f;
        for (size_t k = k0; k <= k1; ++k) {
            float mag = std::sqrt(re[k] * re[k] + im[k] * im[k]) / static_cast<float>(kFftSize * 0.25f);
            peakMag = std::max(peakMag, mag);
        }

        // Slight high-frequency tilt compensation so treble bars look balanced with bass
        float tilt = 1.0f + static_cast<float>(b) * 0.08f;
        float val = std::clamp(std::sqrt(peakMag * tilt * 3.8f) * sensitivity, 0.0f, 1.0f);

        float prev = ioBands[b];
        ioBands[b] = (val > prev) ? (prev + (val - prev) * 0.58f) : (prev * 0.86f);

        if (b < 6) bassAcc += ioBands[b];
        else if (b < 18) midAcc += ioBands[b];
        else highAcc += ioBands[b];
    }

    ioBass = std::clamp(bassAcc / 5.2f, 0.0f, 1.0f);
    ioMid  = std::clamp(midAcc / 11.0f, 0.0f, 1.0f);
    ioHigh = std::clamp(highAcc / 13.0f, 0.0f, 1.0f);
}

AudioSpectrumSnapshot AudioReactiveEngine::ComputeTestFftSnapshot(
    float sampleRate, float freq1Hz, float freq2Hz, float sensitivity) {
    float ring[kFftSize]{};
    for (size_t i = 0; i < kFftSize; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        ring[i] = 0.65f * std::sin(2.0f * kPi * freq1Hz * t) +
                  0.40f * std::sin(2.0f * kPi * freq2Hz * t);
    }

    AudioSpectrumSnapshot out{};
    out.deviceActive = true;
    for (int iter = 0; iter < 5; ++iter) {
        ProcessPcmRingToSpectrum(ring, 0, sampleRate, sensitivity, out.bands, out.bass, out.mid, out.high);
    }
    return out;
}

void AudioReactiveEngine::WorkerLoop() {
    HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.GetAddressOf()));

    ComPtr<IMMDevice> endpoint;
    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioCaptureClient> captureClient;
    WAVEFORMATEX* mixFmt = nullptr;
    bool loopbackStarted = false;

    float ring[kFftSize]{};
    size_t writePos = 0;
    std::array<float, 32> curBands{};
    float curBass = 0.0f, curMid = 0.0f, curHigh = 0.0f;
    std::wstring curTitle;
    ULONGLONG lastTitleQueryMs = 0;
    ULONGLONG lastRetryMs = 0;

    auto releaseAudio = [&]() {
        if (audioClient && loopbackStarted) {
            audioClient->Stop();
        }
        loopbackStarted = false;
        captureClient.Reset();
        audioClient.Reset();
        endpoint.Reset();
        if (mixFmt) {
            CoTaskMemFree(mixFmt);
            mixFmt = nullptr;
        }
    };

    while (running_.load()) {
        ULONGLONG now = GetTickCount64();

        if (!endpoint && enumerator && (now - lastRetryMs >= 2000)) {
            lastRetryMs = now;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, endpoint.GetAddressOf())) && endpoint) {
                if (wantSpectrum_.load()) {
                    if (SUCCEEDED(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                                     reinterpret_cast<void**>(audioClient.GetAddressOf()))) && audioClient) {
                        if (SUCCEEDED(audioClient->GetMixFormat(&mixFmt)) && mixFmt) {
                            HRESULT hr = audioClient->Initialize(
                                AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK,
                                400000,  // 40 ms buffer
                                0,
                                mixFmt,
                                nullptr);
                            if (SUCCEEDED(hr) &&
                                SUCCEEDED(audioClient->GetService(__uuidof(IAudioCaptureClient),
                                                                  reinterpret_cast<void**>(captureClient.GetAddressOf())))) {
                                if (SUCCEEDED(audioClient->Start())) {
                                    loopbackStarted = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (wantSpectrum_.load() && loopbackStarted && captureClient && mixFmt) {
            UINT32 packetLength = 0;
            bool gotSamples = false;
            HRESULT hr = captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                releaseAudio();
            } else {
                const bool isFloat = IsWaveFormatFloat32(mixFmt);
                const bool isPcm16 = (!isFloat && mixFmt->wBitsPerSample == 16);
                const uint32_t channels = std::max<uint32_t>(1, mixFmt->nChannels);

                while (packetLength != 0) {
                    BYTE* data = nullptr;
                    UINT32 numFrames = 0;
                    DWORD flags = 0;
                    if (FAILED(captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr))) {
                        break;
                    }

                    gotSamples = true;
                    const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr;
                    for (UINT32 f = 0; f < numFrames; ++f) {
                        float mono = 0.0f;
                        if (!silent) {
                            if (isFloat) {
                                const float* pf = reinterpret_cast<const float*>(data) + f * channels;
                                for (uint32_t c = 0; c < channels; ++c) mono += pf[c];
                                mono /= static_cast<float>(channels);
                            } else if (isPcm16) {
                                const int16_t* ps = reinterpret_cast<const int16_t*>(data) + f * channels;
                                for (uint32_t c = 0; c < channels; ++c) mono += static_cast<float>(ps[c]) / 32768.0f;
                                mono /= static_cast<float>(channels);
                            }
                        }
                        ring[writePos] = mono;
                        writePos = (writePos + 1) % kFftSize;
                    }

                    captureClient->ReleaseBuffer(numFrames);
                    if (FAILED(captureClient->GetNextPacketSize(&packetLength))) break;
                }

                if (!gotSamples) {
                    // Decay smoothly when no new audio packets arrived during tick
                    for (float& b : curBands) b *= 0.86f;
                    curBass *= 0.85f;
                    curMid  *= 0.85f;
                    curHigh *= 0.85f;
                } else {
                    ProcessPcmRingToSpectrum(
                        ring, writePos,
                        static_cast<float>(mixFmt->nSamplesPerSec),
                        sensitivity_.load(),
                        curBands, curBass, curMid, curHigh);
                }
            }
        } else {
            for (float& b : curBands) b *= 0.85f;
            curBass *= 0.85f;
            curMid  *= 0.85f;
            curHigh *= 0.85f;
        }

        if (wantMediaTitle_.load() && endpoint && (now - lastTitleQueryMs >= 1500)) {
            lastTitleQueryMs = now;
            curTitle = QueryActiveMediaSessionTitle(endpoint.Get());
        } else if (!wantMediaTitle_.load()) {
            curTitle.clear();
        }

        {
            std::lock_guard<std::mutex> lk(snapMutex_);
            snapshot_.bands = curBands;
            snapshot_.bass = curBass;
            snapshot_.mid = curMid;
            snapshot_.high = curHigh;
            snapshot_.deviceActive = (endpoint != nullptr);
            snapshot_.nowPlaying = curTitle;
        }

        Sleep(16);
    }

    releaseAudio();
    if (SUCCEEDED(coHr)) CoUninitialize();
}

}  // namespace wp
