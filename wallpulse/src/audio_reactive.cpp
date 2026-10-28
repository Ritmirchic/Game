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

// ============================================================================
// IMMNotificationClient for instant hot-plug switching (Bluetooth / USB audio)
// ============================================================================
class AudioEndpointNotifyClient final : public IMMNotificationClient {
public:
    explicit AudioEndpointNotifyClient(AudioReactiveEngine* owner) : owner_(owner) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppv = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (c == 0) delete this;
        return c;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        if (owner_) owner_->TriggerEndpointHotSwitch();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        if (owner_) owner_->TriggerEndpointHotSwitch();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && (role == eMultimedia || role == eConsole) && owner_) {
            owner_->TriggerEndpointHotSwitch();
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        return S_OK;
    }

    void Detach() { owner_ = nullptr; }

private:
    long refCount_ = 1;
    AudioReactiveEngine* owner_ = nullptr;
};

// ============================================================================
// Pure ABI WinRT GlobalSystemMediaTransportControls (GSMTC) Reader via combase
// Reads exact Artist — Title even when Spotify / Yandex Music is minimized!
// ============================================================================
struct HStringHeaderAbi {
    union {
        void* Reserved1;
        char Reserved2[24];
    } Reserved;
};

using HSTRING_ABI = void*;

struct CombaseApi {
    using RoInitializeFn = HRESULT(WINAPI*)(int);
    using RoUninitializeFn = void(WINAPI*)();
    using RoGetActivationFactoryFn = HRESULT(WINAPI*)(HSTRING_ABI, REFIID, void**);
    using WindowsCreateStringReferenceFn = HRESULT(WINAPI*)(PCWSTR, UINT32, HStringHeaderAbi*, HSTRING_ABI*);
    using WindowsDeleteStringFn = HRESULT(WINAPI*)(HSTRING_ABI);
    using WindowsGetStringRawBufferFn = PCWSTR(WINAPI*)(HSTRING_ABI, UINT32*);

    RoInitializeFn RoInitialize = nullptr;
    RoUninitializeFn RoUninitialize = nullptr;
    RoGetActivationFactoryFn RoGetActivationFactory = nullptr;
    WindowsCreateStringReferenceFn WindowsCreateStringReference = nullptr;
    WindowsDeleteStringFn WindowsDeleteString = nullptr;
    WindowsGetStringRawBufferFn WindowsGetStringRawBuffer = nullptr;
    bool available = false;

    static const CombaseApi& Get() {
        static CombaseApi api = []() {
            CombaseApi a{};
            HMODULE mod = LoadLibraryW(L"combase.dll");
            if (!mod) return a;
            a.RoInitialize = reinterpret_cast<RoInitializeFn>(GetProcAddress(mod, "RoInitialize"));
            a.RoUninitialize = reinterpret_cast<RoUninitializeFn>(GetProcAddress(mod, "RoUninitialize"));
            a.RoGetActivationFactory = reinterpret_cast<RoGetActivationFactoryFn>(GetProcAddress(mod, "RoGetActivationFactory"));
            a.WindowsCreateStringReference = reinterpret_cast<WindowsCreateStringReferenceFn>(GetProcAddress(mod, "WindowsCreateStringReference"));
            a.WindowsDeleteString = reinterpret_cast<WindowsDeleteStringFn>(GetProcAddress(mod, "WindowsDeleteString"));
            a.WindowsGetStringRawBuffer = reinterpret_cast<WindowsGetStringRawBufferFn>(GetProcAddress(mod, "WindowsGetStringRawBuffer"));
            a.available = (a.RoGetActivationFactory && a.WindowsCreateStringReference &&
                           a.WindowsDeleteString && a.WindowsGetStringRawBuffer);
            return a;
        }();
        return api;
    }
};

struct RoInitScope {
    const CombaseApi& api;
    bool initialized = false;
    explicit RoInitScope(const CombaseApi& a) : api(a) {
        if (api.RoInitialize) initialized = SUCCEEDED(api.RoInitialize(1));
    }
    void Close() {
        if (initialized && api.RoUninitialize) api.RoUninitialize();
        initialized = false;
    }
    ~RoInitScope() { Close(); }
};

// Minimal WinRT ABI interfaces (IInspectable = 6 slots [0..5])
struct IInspectableAbi : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING_ABI*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(int*) = 0;
};

struct IAsyncInfoAbi : public IInspectableAbi {
    virtual HRESULT STDMETHODCALLTYPE get_Id( UINT32*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Status(int32_t* status) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};

struct IAsyncOperationGenericAbi : public IInspectableAbi {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(void** outResult) = 0;
};

struct IGsmtcSessionManagerStaticsAbi : public IInspectableAbi {
    virtual HRESULT STDMETHODCALLTYPE RequestAsync(IAsyncOperationGenericAbi** outOp) = 0;
};

struct IGsmtcSessionManagerAbi : public IInspectableAbi {
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSession(void** outSession) = 0;
};

struct IGsmtcSessionAbi : public IInspectableAbi {
    virtual HRESULT STDMETHODCALLTYPE get_SourceAppUserModelId(HSTRING_ABI*) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryGetMediaPropertiesAsync(IAsyncOperationGenericAbi** outOp) = 0;
};

struct IGsmtcMediaPropertiesAbi : public IInspectableAbi {
    virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING_ABI* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Subtitle(HSTRING_ABI* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING_ABI* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING_ABI* value) = 0;
};

static const IID kIID_IAsyncInfo =
    {0x00000036, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID kIID_IGsmtcStatics =
    {0x2050C4EE, 0x11A0, 0x57DE, {0xAE, 0xD7, 0xC9, 0x7C, 0x70, 0x33, 0x82, 0x45}};

bool WaitAsyncOpResult(IAsyncOperationGenericAbi* op, void** outObj, DWORD maxWaitMs = 160) {
    if (!op || !outObj) return false;
    *outObj = nullptr;

    IAsyncInfoAbi* info = nullptr;
    if (FAILED(op->QueryInterface(kIID_IAsyncInfo, reinterpret_cast<void**>(&info))) || !info) {
        return false;
    }

    int32_t status = 0;  // 0 = Started, 1 = Completed
    DWORD waited = 0;
    while (waited < maxWaitMs) {
        if (FAILED(info->get_Status(&status)) || status != 0) break;
        Sleep(8);
        waited += 8;
    }

    bool ok = false;
    if (status == 1) {
        ok = SUCCEEDED(op->GetResults(outObj)) && (*outObj != nullptr);
    }
    info->Close();
    info->Release();
    return ok;
}

std::wstring HStringToWideAndFree(HSTRING_ABI hs) {
    if (!hs) return {};
    const auto& cb = CombaseApi::Get();
    UINT32 len = 0;
    PCWSTR raw = cb.WindowsGetStringRawBuffer(hs, &len);
    std::wstring res = (raw && len > 0) ? std::wstring(raw, len) : std::wstring{};
    cb.WindowsDeleteString(hs);
    return res;
}

std::wstring QueryGsmtcNowPlaying(ComPtr<IGsmtcSessionManagerAbi>& cachedMgr) {
    const auto& cb = CombaseApi::Get();
    if (!cb.available) return {};

    if (!cachedMgr) {
        constexpr const wchar_t* kRuntimeClass =
            L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
        HStringHeaderAbi hdr{};
        HSTRING_ABI hsClass = nullptr;
        if (FAILED(cb.WindowsCreateStringReference(
                kRuntimeClass, static_cast<UINT32>(wcslen(kRuntimeClass)), &hdr, &hsClass))) {
            return {};
        }

        ComPtr<IGsmtcSessionManagerStaticsAbi> statics;
        HRESULT hr = cb.RoGetActivationFactory(
            hsClass, kIID_IGsmtcStatics, reinterpret_cast<void**>(statics.GetAddressOf()));
        if (FAILED(hr) || !statics) return {};

        ComPtr<IAsyncOperationGenericAbi> reqOp;
        if (FAILED(statics->RequestAsync(reqOp.GetAddressOf())) || !reqOp) return {};

        void* mgrRaw = nullptr;
        if (!WaitAsyncOpResult(reqOp.Get(), &mgrRaw, 200) || !mgrRaw) return {};
        cachedMgr.Attach(static_cast<IGsmtcSessionManagerAbi*>(mgrRaw));
    }

    void* sessionRaw = nullptr;
    if (FAILED(cachedMgr->GetCurrentSession(&sessionRaw)) || !sessionRaw) {
        return {};
    }
    ComPtr<IGsmtcSessionAbi> session;
    session.Attach(static_cast<IGsmtcSessionAbi*>(sessionRaw));

    ComPtr<IAsyncOperationGenericAbi> propOp;
    if (FAILED(session->TryGetMediaPropertiesAsync(propOp.GetAddressOf())) || !propOp) {
        return {};
    }

    void* propsRaw = nullptr;
    if (!WaitAsyncOpResult(propOp.Get(), &propsRaw, 160) || !propsRaw) {
        return {};
    }
    ComPtr<IGsmtcMediaPropertiesAbi> props;
    props.Attach(static_cast<IGsmtcMediaPropertiesAbi*>(propsRaw));

    HSTRING_ABI hsTitle = nullptr, hsArtist = nullptr;
    props->get_Title(&hsTitle);
    props->get_Artist(&hsArtist);

    std::wstring title = HStringToWideAndFree(hsTitle);
    std::wstring artist = HStringToWideAndFree(hsArtist);
    if (title.empty()) return {};

    std::wstring combined = artist.empty() ? title : (artist + L" — " + title);
    if (combined.size() > 62) combined = combined.substr(0, 59) + L"...";
    return combined;
}

// ============================================================================
// Fallback PID Window Title Scanner (supports both visible AND minimized tray players!)
// ============================================================================
struct FindPidWindowCtx {
    DWORD targetPid = 0;
    std::wstring bestVisibleTitle;
    std::wstring bestHiddenMediaTitle;
};

bool IsNoiseSystemWindowTitle(const std::wstring& t) {
    return t == L"Default IME" || t == L"MSCTFIME UI" || t == L"GDI+ Window" ||
           t == L"GDI+ Hook Window Class" || t == L"CicMarshalWnd" ||
           t == L"MediaContextNotificationWindow" || t == L"SystemTray_Main" ||
           t.rfind(L"Chrome Legacy Window", 0) == 0;
}

BOOL CALLBACK EnumWindowForPidTitle(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<FindPidWindowCtx*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->targetPid) return TRUE;

    int len = GetWindowTextLengthW(hwnd);
    if (len <= 1 || len > 512) return TRUE;

    std::wstring title(static_cast<size_t>(len) + 1, L'\0');
    int got = GetWindowTextW(hwnd, &title[0], len + 1);
    if (got <= 1) return TRUE;
    title.resize(static_cast<size_t>(got));

    if (IsNoiseSystemWindowTitle(title)) return TRUE;

    if (IsWindowVisible(hwnd)) {
        if (title.size() > ctx->bestVisibleTitle.size()) {
            ctx->bestVisibleTitle = std::move(title);
        }
    } else {
        // Tray-minimized players (Spotify, Yandex Music, AIMP, foobar2000) keep "Artist - Track" on hidden top HWND
        if ((title.find(L" - ") != std::wstring::npos || title.find(L" — ") != std::wstring::npos) &&
            title.size() > ctx->bestHiddenMediaTitle.size()) {
            ctx->bestHiddenMediaTitle = std::move(title);
        }
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
        L" — Яндекс Музыка",
        L" - Яндекс Музыка",
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
    if (t.size() > 62) {
        t = t.substr(0, 59) + L"...";
    }
    return t;
}

std::wstring QueryActiveMediaSessionTitleFallback(IMMDevice* endpoint) {
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
        if (!wctx.bestVisibleTitle.empty()) {
            return CleanMediaWindowTitle(wctx.bestVisibleTitle);
        }
        if (!wctx.bestHiddenMediaTitle.empty()) {
            return CleanMediaWindowTitle(wctx.bestHiddenMediaTitle);
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
        return ext->SubFormat.Data1 == 0x00000003 && wfx->wBitsPerSample == 32;
    }
    return false;
}

}  // namespace

AudioReactiveEngine::AudioReactiveEngine() = default;

AudioReactiveEngine::~AudioReactiveEngine() {
    Shutdown();
}

void AudioReactiveEngine::TriggerEndpointHotSwitch() {
    endpointDirty_.store(true);
    hotSwitchCount_.fetch_add(1);
}

void AudioReactiveEngine::SetEnabled(bool captureSpectrum, bool detectMediaTitle, float sensitivity) {
    sensitivity_.store(std::clamp(sensitivity, 0.25f, 3.0f));
    bool prevSpectrum = wantSpectrum_.exchange(captureSpectrum);
    wantMediaTitle_.store(detectMediaTitle);

    if (captureSpectrum && !prevSpectrum) {
        endpointDirty_.store(true);
    }

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

std::wstring AudioReactiveEngine::ProbeSystemNowPlaying() {
    const auto& cb = CombaseApi::Get();
    RoInitScope roScope(cb);
    ComPtr<IGsmtcSessionManagerAbi> mgr;
    std::wstring gsmtc = QueryGsmtcNowPlaying(mgr);
    if (!gsmtc.empty()) return gsmtc;

    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> endpoint;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.GetAddressOf()))) &&
        enumerator &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, endpoint.GetAddressOf()))) {
        return QueryActiveMediaSessionTitleFallback(endpoint.Get());
    }
    return {};
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
    const auto& cbApi = CombaseApi::Get();
    RoInitScope roScope(cbApi);

    ComPtr<IMMDeviceEnumerator> enumerator;
    CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.GetAddressOf()));

    ComPtr<AudioEndpointNotifyClient> notifyClient;
    bool notifyRegistered = false;
    if (enumerator) {
        notifyClient.Attach(new AudioEndpointNotifyClient(this));
        if (SUCCEEDED(enumerator->RegisterEndpointNotificationCallback(notifyClient.Get()))) {
            notifyRegistered = true;
        }
    }

    ComPtr<IGsmtcSessionManagerAbi> gsmtcMgr;
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

        // Instant hot-switch when Bluetooth / USB headphones or default audio output changes!
        if (endpointDirty_.exchange(false)) {
            releaseAudio();
            lastRetryMs = 0;
        }

        if ((!endpoint || (wantSpectrum_.load() && !loopbackStarted)) &&
            enumerator && (lastRetryMs == 0 || now - lastRetryMs >= 1500)) {
            releaseAudio();
            lastRetryMs = now;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, endpoint.GetAddressOf())) && endpoint) {
                if (wantSpectrum_.load()) {
                    if (SUCCEEDED(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                                     reinterpret_cast<void**>(audioClient.GetAddressOf()))) && audioClient) {
                        if (SUCCEEDED(audioClient->GetMixFormat(&mixFmt)) && mixFmt) {
                            HRESULT hr = audioClient->Initialize(
                                AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK,
                                400000,  // 40 ms shared loopback buffer
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
                lastRetryMs = 0;  // Rebind immediately on device invalidated
            } else {
                const bool isFloat = IsWaveFormatFloat32(mixFmt);
                const bool isPcm16 = (!isFloat && mixFmt->wBitsPerSample == 16);
                const bool isPcm24 = (!isFloat && mixFmt->wBitsPerSample == 24);
                const bool isPcm32 = (!isFloat && mixFmt->wBitsPerSample == 32);
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
                            } else if (isPcm24) {
                                const BYTE* p = data + (static_cast<size_t>(f) * channels * 3);
                                for (uint32_t c = 0; c < channels; ++c) {
                                    const BYTE* q = p + static_cast<size_t>(c) * 3;
                                    int32_t s24 = static_cast<int32_t>(q[0]) |
                                                  (static_cast<int32_t>(q[1]) << 8) |
                                                  (static_cast<int32_t>(q[2]) << 16);
                                    if (s24 & 0x00800000) s24 |= static_cast<int32_t>(0xFF000000);
                                    mono += static_cast<float>(s24) / 8388608.0f;
                                }
                                mono /= static_cast<float>(channels);
                            } else if (isPcm32) {
                                const int32_t* ps = reinterpret_cast<const int32_t*>(data) + f * channels;
                                for (uint32_t c = 0; c < channels; ++c) mono += static_cast<float>(ps[c]) / 2147483648.0f;
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

        if (wantMediaTitle_.load() && (now - lastTitleQueryMs >= 1200)) {
            lastTitleQueryMs = now;
            std::wstring gsmtcTitle = QueryGsmtcNowPlaying(gsmtcMgr);
            if (!gsmtcTitle.empty()) {
                curTitle = std::move(gsmtcTitle);
            } else {
                curTitle = QueryActiveMediaSessionTitleFallback(endpoint.Get());
            }
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
            snapshot_.hotSwitchCount = hotSwitchCount_.load();
            snapshot_.nowPlaying = curTitle;
        }

        Sleep(16);
    }

    if (notifyRegistered && enumerator && notifyClient) {
        enumerator->UnregisterEndpointNotificationCallback(notifyClient.Get());
    }
    if (notifyClient) {
        notifyClient->Detach();
        notifyClient.Reset();
    }
    gsmtcMgr.Reset();
    releaseAudio();
    roScope.Close();
    if (SUCCEEDED(coHr)) CoUninitialize();
}

}  // namespace wp
