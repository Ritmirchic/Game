#include "video_wallpaper.h"

#include <d3d10.h>
#include <d3dcompiler.h>
#include <mmdeviceapi.h>
#include <shlwapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wp {

namespace {

constexpr const wchar_t* kRenderWindowClass = L"WallPulseDesktopRendererClass";
constexpr uint32_t kPreviewWidth = 320;
constexpr uint32_t kPreviewHeight = 180;

#pragma pack(push, 4)
struct ShaderParamsCB {
    float targetW = 1920.0f;
    float targetH = 1080.0f;
    float videoW = 1920.0f;
    float videoH = 1080.0f;

    float parallaxX = 0.0f;
    float parallaxY = 0.0f;
    int32_t scaleMode = 0;
    float brightness = 0.0f;

    float contrast = 1.0f;
    float saturation = 1.0f;
    float vignette = 0.15f;
    float parallaxActive = 1.0f;
};
#pragma pack(pop)
static_assert(sizeof(ShaderParamsCB) == 48, "ShaderParamsCB must be 16-byte aligned (48 bytes)");

const char* kWallpaperHlsl = R"(
cbuffer Params : register(b0) {
    float2 targetSize;
    float2 videoSize;
    float2 parallax;
    int    scaleMode;
    float  brightness;
    float  contrast;
    float  saturation;
    float  vignette;
    float  parallaxActive;
};

Texture2D    videoTex : register(t0);
SamplerState linSamp  : register(s0);

struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float2 ComputeCoverUV(float2 uv, float tAspect, float vAspect) {
    float2 s = (tAspect > vAspect)
        ? float2(1.0, vAspect / max(tAspect, 0.0001))
        : float2(tAspect / max(vAspect, 0.0001), 1.0);
    if (parallaxActive > 0.5) {
        s *= 0.95;
    }
    return (uv - 0.5) * s + 0.5 + parallax;
}

float3 ApplyColorGrade(float3 col, float2 screenUv) {
    col = (col - 0.5) * contrast + 0.5 + brightness;
    float luma = dot(col, float3(0.2126, 0.7152, 0.0722));
    col = lerp(float3(luma, luma, luma), col, saturation);

    float2 d = (screenUv - 0.5) * 2.0;
    float r2 = dot(d, d);
    float vig = 1.0 - saturate(r2 * 0.45) * vignette;
    // Slight overall dim component when vignette > 0.35 so desktop icon labels stay crisp
    float dimFactor = 1.0 - max(0.0, vignette - 0.25) * 0.45;
    return saturate(col * vig * dimFactor);
}

float4 PSMain(VSOut pin) : SV_Target {
    float2 uv = pin.uv;
    float tAspect = targetSize.x / max(targetSize.y, 1.0);
    float vAspect = videoSize.x / max(videoSize.y, 1.0);

    float3 rgb = float3(0.03, 0.04, 0.07);

    if (scaleMode == 0) {
        // Cover
        float2 cuv = saturate(ComputeCoverUV(uv, tAspect, vAspect));
        rgb = videoTex.Sample(linSamp, cuv).rgb;
    } else if (scaleMode == 3) {
        // Stretch
        float2 suv = (parallaxActive > 0.5) ? saturate((uv - 0.5) * 0.95 + 0.5 + parallax) : uv;
        rgb = videoTex.Sample(linSamp, suv).rgb;
    } else if (scaleMode == 4) {
        // Center 1:1
        float2 pxPos = (uv - 0.5) * targetSize + parallax * targetSize;
        float2 vuv = pxPos / max(videoSize, float2(1.0, 1.0)) + 0.5;
        if (all(vuv >= 0.0) && all(vuv <= 1.0)) {
            rgb = videoTex.Sample(linSamp, vuv).rgb;
        }
    } else {
        // FitBlur (1) or FitBars (2)
        float2 fuv = (tAspect > vAspect)
            ? float2((uv.x - 0.5) * (tAspect / max(vAspect, 0.0001)) + 0.5, uv.y)
            : float2(uv.x, (uv.y - 0.5) * (vAspect / max(tAspect, 0.0001)) + 0.5);

        if (all(fuv >= 0.0) && all(fuv <= 1.0)) {
            rgb = videoTex.Sample(linSamp, fuv).rgb;
        } else if (scaleMode == 1) {
            // Soft blurred ambient backdrop
            float2 buv = ComputeCoverUV(uv, tAspect, vAspect);
            float2 stepUv = float2(0.018, 0.018 * tAspect);
            float3 acc = videoTex.Sample(linSamp, saturate(buv)).rgb * 0.20;
            acc += videoTex.Sample(linSamp, saturate(buv + float2( stepUv.x, 0.0))).rgb * 0.12;
            acc += videoTex.Sample(linSamp, saturate(buv + float2(-stepUv.x, 0.0))).rgb * 0.12;
            acc += videoTex.Sample(linSamp, saturate(buv + float2(0.0,  stepUv.y))).rgb * 0.12;
            acc += videoTex.Sample(linSamp, saturate(buv + float2(0.0, -stepUv.y))).rgb * 0.12;
            acc += videoTex.Sample(linSamp, saturate(buv + stepUv * float2( 1.4,  1.4))).rgb * 0.08;
            acc += videoTex.Sample(linSamp, saturate(buv + stepUv * float2(-1.4,  1.4))).rgb * 0.08;
            acc += videoTex.Sample(linSamp, saturate(buv + stepUv * float2( 1.4, -1.4))).rgb * 0.08;
            acc += videoTex.Sample(linSamp, saturate(buv + stepUv * float2(-1.4, -1.4))).rgb * 0.08;
            rgb = acc * 0.42;
        }
    }

    return float4(ApplyColorGrade(rgb, uv), 1.0);
}
)";

class MediaEngineNotifyImpl final : public IMFMediaEngineNotify {
public:
    explicit MediaEngineNotifyImpl(VideoWallpaperEngine* owner)
        : refCount_(1), owner_(owner) {}

    void DetachOwner() {
        std::lock_guard<std::mutex> lk(mu_);
        owner_ = nullptr;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFMediaEngineNotify)) {
            *ppv = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = InterlockedDecrement(&refCount_);
        if (c == 0) delete this;
        return c;
    }

    HRESULT STDMETHODCALLTYPE EventNotify(DWORD event, DWORD_PTR param1, DWORD param2) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (owner_) {
            owner_->OnMediaEngineEvent(event, param1, param2);
        }
        return S_OK;
    }

private:
    ~MediaEngineNotifyImpl() = default;
    volatile LONG refCount_;
    std::mutex mu_;
    VideoWallpaperEngine* owner_;
};

std::wstring FilePathToMediaUri(const std::wstring& filePath) {
    wchar_t uriBuf[4096] = {};
    DWORD cch = 4095;
    HRESULT hr = UrlCreateFromPathW(filePath.c_str(), uriBuf, &cch, 0);
    if (SUCCEEDED(hr) && uriBuf[0] != L'\0') {
        return std::wstring(uriBuf, cch);
    }
    // Fallback manual file:/// URI conversion
    std::wstring p = filePath;
    for (wchar_t& ch : p) {
        if (ch == L'\\') ch = L'/';
    }
    return L"file:///" + p;
}

bool HasActiveAudioRenderEndpoint() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr) || !enumerator) return false;

    ComPtr<IMMDevice> defaultEndpoint;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, defaultEndpoint.GetAddressOf());
    return SUCCEEDED(hr) && defaultEndpoint != nullptr;
}

}  // namespace

VideoWallpaperEngine::VideoWallpaperEngine() = default;

VideoWallpaperEngine::~VideoWallpaperEngine() {
    Shutdown();
}

bool VideoWallpaperEngine::Initialize(HINSTANCE hInstance, DesktopHost* desktopHost) {
    desktopHost_ = desktopHost;

    if (!CreateRenderWindow(hInstance)) {
        lastError_ = L"Не удалось создать окно рендеринга обоев";
        LogError(lastError_);
        return false;
    }

    if (!InitD3D11()) {
        LogError(lastError_);
        return false;
    }

    if (!InitShaders()) {
        LogError(lastError_);
        return false;
    }

    if (!InitMediaEngine()) {
        LogError(lastError_);
        return false;
    }

    EnsurePreviewSurfaces();
    LogInfo(L"VideoWallpaperEngine инициализирован успешно. GPU: " + adapterName_);
    return true;
}

void VideoWallpaperEngine::Shutdown() {
    StopAndDetach();

    if (mediaNotify_) {
        static_cast<MediaEngineNotifyImpl*>(mediaNotify_.Get())->DetachOwner();
    }
    if (mediaEngine_) {
        mediaEngine_->Shutdown();
        mediaEngine_.Reset();
    }
    mediaNotify_.Reset();
    dxgiManager_.Reset();

    previewStaging_.Reset();
    previewRtv_.Reset();
    previewTex_.Reset();

    videoSrv_.Reset();
    videoTex_.Reset();
    backbufferRtv_.Reset();
    swapChain_.Reset();
    constantBuf_.Reset();
    sampler_.Reset();
    ps_.Reset();
    vs_.Reset();
    d3dCtx_.Reset();
    d3dDevice_.Reset();

    if (renderHwnd_ && IsWindow(renderHwnd_)) {
        DestroyWindow(renderHwnd_);
        renderHwnd_ = nullptr;
    }
}

LRESULT CALLBACK VideoWallpaperEngine::RenderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

bool VideoWallpaperEngine::CreateRenderWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = RenderWndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = CreateSolidBrush(RGB(9, 13, 22));
    wc.lpszClassName = kRenderWindowClass;
    RegisterClassExW(&wc);

    renderHwnd_ = CreateWindowExW(
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kRenderWindowClass,
        L"WallPulse Live Desktop",
        WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, 640, 360,
        nullptr, nullptr, hInstance, nullptr);

    return renderHwnd_ != nullptr;
}

bool VideoWallpaperEngine::InitD3D11() {
    UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL chosen{};

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, 4, D3D11_SDK_VERSION,
        d3dDevice_.ReleaseAndGetAddressOf(), &chosen, d3dCtx_.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        // Fallback without VIDEO_SUPPORT or to WARP software adapter (e.g. VM / CI runner)
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, 4, D3D11_SDK_VERSION,
            d3dDevice_.ReleaseAndGetAddressOf(), &chosen, d3dCtx_.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, 4, D3D11_SDK_VERSION,
            d3dDevice_.ReleaseAndGetAddressOf(), &chosen, d3dCtx_.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, 4, D3D11_SDK_VERSION,
            d3dDevice_.ReleaseAndGetAddressOf(), &chosen, d3dCtx_.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr) || !d3dDevice_ || !d3dCtx_) {
        lastError_ = L"Не удалось инициализировать Direct3D 11";
        LogHRESULT(lastError_, hr);
        return false;
    }

    // Required by IMFDXGIDeviceManager for thread-safe hardware video decoding
    ComPtr<ID3D10Multithread> mt;
    if (SUCCEEDED(d3dDevice_.As(&mt)) && mt) {
        mt->SetMultithreadProtected(TRUE);
    }

    ComPtr<IDXGIDevice> dxgiDev;
    if (SUCCEEDED(d3dDevice_.As(&dxgiDev)) && dxgiDev) {
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(dxgiDev->GetAdapter(adapter.GetAddressOf())) && adapter) {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(adapter->GetDesc(&desc)) && desc.Description[0] != L'\0') {
                adapterName_ = desc.Description;
            }
        }
    }

    return true;
}

bool VideoWallpaperEngine::InitShaders() {
    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    HRESULT hr = D3DCompile(
        kWallpaperHlsl, std::strlen(kWallpaperHlsl), "wallpaper.hlsl",
        nullptr, nullptr, "VSMain", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        vsBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr) || !vsBlob) {
        lastError_ = L"Ошибка компиляции вершинного шейдера D3D11";
        LogHRESULT(lastError_, hr);
        return false;
    }

    errBlob.Reset();
    hr = D3DCompile(
        kWallpaperHlsl, std::strlen(kWallpaperHlsl), "wallpaper.hlsl",
        nullptr, nullptr, "PSMain", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        psBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr) || !psBlob) {
        lastError_ = L"Ошибка компиляции пиксельного шейдера D3D11";
        LogHRESULT(lastError_, hr);
        return false;
    }

    hr = d3dDevice_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = d3dDevice_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = d3dDevice_->CreateSamplerState(&sampDesc, sampler_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(ShaderParamsCB);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = d3dDevice_->CreateBuffer(&cbDesc, nullptr, constantBuf_.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

bool VideoWallpaperEngine::InitMediaEngine() {
    HRESULT hr = MFCreateDXGIDeviceManager(&dxgiResetToken_, dxgiManager_.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr) && dxgiManager_) {
        dxgiManager_->ResetDevice(d3dDevice_.Get(), dxgiResetToken_);
    }

    ComPtr<IMFMediaEngineClassFactory> factory;
    hr = CoCreateInstance(
        CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMFMediaEngineClassFactory), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr) || !factory) {
        lastError_ = L"Компонент Windows Media Engine недоступен (установите Media Feature Pack, если у вас Windows N)";
        LogHRESULT(lastError_, hr);
        return false;
    }

    auto* notifyRaw = new MediaEngineNotifyImpl(this);
    mediaNotify_.Reset();
    *mediaNotify_.GetAddressOf() = notifyRaw;

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(attrs.GetAddressOf(), 4);
    attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, mediaNotify_.Get());
    attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);
    if (dxgiManager_) {
        attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, dxgiManager_.Get());
    }

    DWORD engineFlags = 0;
    if (!HasActiveAudioRenderEndpoint()) {
        engineFlags |= MF_MEDIA_ENGINE_FORCEMUTE;
        LogInfo(L"Аудиовыход не обнаружен — IMFMediaEngine запущен с системным таймером (MF_MEDIA_ENGINE_FORCEMUTE).");
    }

    hr = factory->CreateInstance(engineFlags, attrs.Get(), mediaEngine_.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !mediaEngine_) {
        // Retry without DXGI manager and with FORCEMUTE fallback
        attrs->DeleteItem(MF_MEDIA_ENGINE_DXGI_MANAGER);
        hr = factory->CreateInstance(engineFlags | MF_MEDIA_ENGINE_FORCEMUTE, attrs.Get(), mediaEngine_.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr) || !mediaEngine_) {
        lastError_ = L"Не удалось создать экземпляр IMFMediaEngine";
        LogHRESULT(lastError_, hr);
        return false;
    }

    mediaEngine_->SetAutoPlay(FALSE);
    mediaEngine_->SetLoop(TRUE);
    mediaEngine_->SetPreload(MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC);
    return true;
}

bool VideoWallpaperEngine::CreateOrResizeSwapChain(uint32_t width, uint32_t height) {
    width = std::max<uint32_t>(64, width);
    height = std::max<uint32_t>(64, height);

    if (swapChain_ && swapW_ == width && swapH_ == height && backbufferRtv_) {
        return true;
    }

    if (d3dCtx_) {
        d3dCtx_->OMSetRenderTargets(0, nullptr, nullptr);
    }
    backbufferRtv_.Reset();

    HRESULT hr = S_OK;
    if (swapChain_) {
        hr = swapChain_->ResizeBuffers(1, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        if (FAILED(hr)) {
            swapChain_.Reset();
        }
    }

    if (!swapChain_) {
        ComPtr<IDXGIDevice> dxgiDev;
        hr = d3dDevice_.As(&dxgiDev);
        if (FAILED(hr)) return false;

        ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDev->GetAdapter(adapter.GetAddressOf());
        if (FAILED(hr)) return false;

        ComPtr<IDXGIFactory> factory;
        hr = adapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(factory.GetAddressOf()));
        if (FAILED(hr) || !factory) return false;

        // Blt-model DXGI_SWAP_EFFECT_DISCARD works seamlessly on WorkerW WS_CHILD AND
        // Windows 11 24H2+ Progman WS_CHILD | WS_EX_LAYERED (SetLayeredWindowAttributes alpha=255)
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;
        sd.OutputWindow = renderHwnd_;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        hr = factory->CreateSwapChain(d3dDevice_.Get(), &sd, swapChain_.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !swapChain_) {
            LogHRESULT(L"CreateSwapChain failed", hr);
            return false;
        }
        factory->MakeWindowAssociation(renderHwnd_, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr) || !backBuffer) return false;

    hr = d3dDevice_->CreateRenderTargetView(backBuffer.Get(), nullptr, backbufferRtv_.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !backbufferRtv_) return false;

    swapW_ = width;
    swapH_ = height;
    dirtyVisuals_ = true;
    return true;
}

bool VideoWallpaperEngine::EnsureVideoSurface(uint32_t width, uint32_t height) {
    width = std::clamp<uint32_t>(width, 16, 7680);
    height = std::clamp<uint32_t>(height, 16, 4320);

    if (videoTex_ && videoSrv_ && videoW_ == width && videoH_ == height) {
        return true;
    }

    videoSrv_.Reset();
    videoTex_.Reset();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = d3dDevice_->CreateTexture2D(&td, nullptr, videoTex_.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !videoTex_) {
        LogHRESULT(L"CreateTexture2D(videoTex) failed", hr);
        return false;
    }

    hr = d3dDevice_->CreateShaderResourceView(videoTex_.Get(), nullptr, videoSrv_.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !videoSrv_) {
        LogHRESULT(L"CreateShaderResourceView(videoSrv) failed", hr);
        return false;
    }

    videoW_ = width;
    videoH_ = height;
    return true;
}

bool VideoWallpaperEngine::EnsurePreviewSurfaces() {
    if (previewTex_ && previewRtv_ && previewStaging_) return true;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = kPreviewWidth;
    td.Height = kPreviewHeight;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;

    if (FAILED(d3dDevice_->CreateTexture2D(&td, nullptr, previewTex_.ReleaseAndGetAddressOf()))) {
        return false;
    }
    if (FAILED(d3dDevice_->CreateRenderTargetView(previewTex_.Get(), nullptr, previewRtv_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return SUCCEEDED(d3dDevice_->CreateTexture2D(&td, nullptr, previewStaging_.ReleaseAndGetAddressOf()));
}

void VideoWallpaperEngine::OnMediaEngineEvent(DWORD event, DWORD_PTR param1, DWORD param2) {
    (void)param2;
    switch (event) {
        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
        case MF_MEDIA_ENGINE_EVENT_CANPLAY:
            canPlay_.store(true);
            break;
        case MF_MEDIA_ENGINE_EVENT_ENDED:
            endedSignal_.store(true);
            break;
        case MF_MEDIA_ENGINE_EVENT_ERROR:
            errorSignal_.store(true);
            LogWarn(L"Событие ошибки IMFMediaEngine param1=" + std::to_wstring(param1));
            break;
        default:
            break;
    }
}

bool VideoWallpaperEngine::LoadAndApply(const std::wstring& videoPath, const AppConfig& cfg) {
    if (!mediaEngine_ || !renderHwnd_) return false;
    if (!FileExists(videoPath)) {
        lastError_ = L"Видеофайл не найден: " + videoPath;
        LogError(lastError_);
        return false;
    }

    lastError_.clear();
    activeCfg_ = cfg;
    userPaused_ = cfg.userPaused;
    currentPath_ = videoPath;
    canPlay_.store(false);
    endedSignal_.store(false);
    errorSignal_.store(false);

    // Attach render window to Windows Desktop behind icons
    if (desktopHost_) {
        RECT targetRect = desktopHost_->ComputeTargetScreenRect(cfg.monitorMode, cfg.targetMonitorIndex);
        uint32_t w = static_cast<uint32_t>(std::max<LONG>(64, targetRect.right - targetRect.left));
        uint32_t h = static_cast<uint32_t>(std::max<LONG>(64, targetRect.bottom - targetRect.top));

        desktopHost_->AttachRenderWindow(renderHwnd_, targetRect);
        attachedToDesktop_ = true;
        CreateOrResizeSwapChain(w, h);
    }

    std::wstring uri = FilePathToMediaUri(videoPath);
    BSTR bstrUri = SysAllocString(uri.c_str());
    if (!bstrUri) return false;

    mediaEngine_->Pause();
    mediaEngine_->SetLoop(TRUE);
    UpdateAudioAndRate();

    HRESULT hr = mediaEngine_->SetSource(bstrUri);
    SysFreeString(bstrUri);
    if (FAILED(hr)) {
        lastError_ = L"Не удалось открыть видеофайл через Media Foundation";
        LogHRESULT(lastError_, hr);
        hasMedia_ = false;
        return false;
    }

    mediaEngine_->Load();
    hasMedia_ = true;
    dirtyVisuals_ = true;
    LogInfo(L"Загружено живое видео: " + videoPath);
    return true;
}

void VideoWallpaperEngine::StopAndDetach() {
    hasMedia_ = false;
    canPlay_.store(false);
    currentPath_.clear();

    if (mediaEngine_) {
        mediaEngine_->Pause();
        BSTR empty = SysAllocString(L"");
        if (empty) {
            mediaEngine_->SetSource(empty);
            SysFreeString(empty);
        }
    }

    if (desktopHost_ && renderHwnd_) {
        desktopHost_->DetachRenderWindow(renderHwnd_);
    } else if (renderHwnd_) {
        ShowWindow(renderHwnd_, SW_HIDE);
    }
    attachedToDesktop_ = false;
}

void VideoWallpaperEngine::ApplySettings(const AppConfig& cfg, bool layoutChanged) {
    activeCfg_ = cfg;
    userPaused_ = cfg.userPaused;
    dirtyVisuals_ = true;

    if (layoutChanged && attachedToDesktop_ && desktopHost_ && renderHwnd_) {
        RECT targetRect = desktopHost_->ComputeTargetScreenRect(cfg.monitorMode, cfg.targetMonitorIndex);
        uint32_t w = static_cast<uint32_t>(std::max<LONG>(64, targetRect.right - targetRect.left));
        uint32_t h = static_cast<uint32_t>(std::max<LONG>(64, targetRect.bottom - targetRect.top));
        desktopHost_->UpdateRenderWindowBounds(renderHwnd_, targetRect);
        CreateOrResizeSwapChain(w, h);
    }

    UpdateAudioAndRate();

    if (mediaEngine_ && hasMedia_ && canPlay_.load()) {
        if (IsEffectivelyPaused()) {
            mediaEngine_->Pause();
        } else if (mediaEngine_->IsPaused()) {
            mediaEngine_->Play();
        }
    }
}

void VideoWallpaperEngine::SetSmartPause(bool pauseVideo, bool muteAudio) {
    if (smartPaused_ == pauseVideo && smartMuted_ == muteAudio) return;
    smartPaused_ = pauseVideo;
    smartMuted_ = muteAudio;

    UpdateAudioAndRate();
    if (mediaEngine_ && hasMedia_ && canPlay_.load()) {
        if (IsEffectivelyPaused()) {
            mediaEngine_->Pause();
        } else if (mediaEngine_->IsPaused()) {
            mediaEngine_->Play();
        }
    }
}

void VideoWallpaperEngine::UpdateAudioAndRate() {
    if (!mediaEngine_) return;
    bool effectiveMute = activeCfg_.muted || smartMuted_ || (activeCfg_.volume <= 0.001f);
    mediaEngine_->SetMuted(effectiveMute ? TRUE : FALSE);
    mediaEngine_->SetVolume(std::clamp(static_cast<double>(activeCfg_.volume), 0.0, 1.0));
    double rate = std::clamp(static_cast<double>(activeCfg_.playbackRate), 0.25, 2.0);
    mediaEngine_->SetDefaultPlaybackRate(rate);
    mediaEngine_->SetPlaybackRate(rate);
}

void VideoWallpaperEngine::CheckDesktopAttachment(const AppConfig& cfg) {
    if (!hasMedia_ || !attachedToDesktop_ || !desktopHost_ || !renderHwnd_) return;
    if (!desktopHost_->IsAttachmentHealthy(renderHwnd_)) {
        LogWarn(L"Обнаружен перезапуск Проводника или слоя рабочего стола — переподключаем видеообои...");
        desktopHost_->DiscoverAndPrepare();
        RECT targetRect = desktopHost_->ComputeTargetScreenRect(cfg.monitorMode, cfg.targetMonitorIndex);
        desktopHost_->AttachRenderWindow(renderHwnd_, targetRect);
        CreateOrResizeSwapChain(
            static_cast<uint32_t>(std::max<LONG>(64, targetRect.right - targetRect.left)),
            static_cast<uint32_t>(std::max<LONG>(64, targetRect.bottom - targetRect.top)));
        dirtyVisuals_ = true;
    }
}

void VideoWallpaperEngine::RenderSceneToTarget(
    ID3D11RenderTargetView* rtv,
    uint32_t targetW,
    uint32_t targetH,
    bool multiMonitorViewports) {
    if (!rtv || !videoSrv_ || !vs_ || !ps_ || !constantBuf_) return;

    const float clearColor[4] = {0.03f, 0.04f, 0.07f, 1.0f};
    d3dCtx_->ClearRenderTargetView(rtv, clearColor);
    d3dCtx_->OMSetRenderTargets(1, &rtv, nullptr);

    d3dCtx_->IASetInputLayout(nullptr);
    d3dCtx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3dCtx_->VSSetShader(vs_.Get(), nullptr, 0);
    d3dCtx_->PSSetShader(ps_.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[1] = {videoSrv_.Get()};
    ID3D11SamplerState* samps[1] = {sampler_.Get()};
    ID3D11Buffer* cbs[1] = {constantBuf_.Get()};
    d3dCtx_->PSSetShaderResources(0, 1, srvs);
    d3dCtx_->PSSetSamplers(0, 1, samps);
    d3dCtx_->PSSetConstantBuffers(0, 1, cbs);

    auto drawViewport = [&](float vx, float vy, float vw, float vh) {
        if (vw <= 1.0f || vh <= 1.0f) return;
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = vx;
        vp.TopLeftY = vy;
        vp.Width = vw;
        vp.Height = vh;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        d3dCtx_->RSSetViewports(1, &vp);

        ShaderParamsCB cb{};
        cb.targetW = vw;
        cb.targetH = vh;
        cb.videoW = static_cast<float>(std::max<uint32_t>(1, videoW_));
        cb.videoH = static_cast<float>(std::max<uint32_t>(1, videoH_));
        cb.parallaxX = parallaxX_;
        cb.parallaxY = parallaxY_;
        cb.scaleMode = static_cast<int32_t>(activeCfg_.scaleMode);
        cb.brightness = activeCfg_.brightness;
        cb.contrast = activeCfg_.contrast;
        cb.saturation = activeCfg_.saturation;
        cb.vignette = activeCfg_.vignette;
        cb.parallaxActive = activeCfg_.mouseParallax ? 1.0f : 0.0f;

        d3dCtx_->UpdateSubresource(constantBuf_.Get(), 0, nullptr, &cb, 0, 0);
        d3dCtx_->Draw(3, 0);
    };

    if (multiMonitorViewports && activeCfg_.monitorMode == MonitorMode::AllSame && desktopHost_) {
        RECT virt = desktopHost_->GetVirtualScreenRect();
        auto mons = desktopHost_->EnumerateMonitors();
        for (const auto& m : mons) {
            drawViewport(
                static_cast<float>(m.bounds.left - virt.left),
                static_cast<float>(m.bounds.top - virt.top),
                static_cast<float>(m.Width()),
                static_cast<float>(m.Height()));
        }
    } else {
        drawViewport(0.0f, 0.0f, static_cast<float>(targetW), static_cast<float>(targetH));
    }

    ID3D11ShaderResourceView* nullSrv[1] = {nullptr};
    d3dCtx_->PSSetShaderResources(0, 1, nullSrv);
}

bool VideoWallpaperEngine::TickFrame(bool capturePreviewForStudio) {
    if (!hasMedia_ || !mediaEngine_) return false;

    if (errorSignal_.exchange(false)) {
        ComPtr<IMFMediaError> err;
        if (SUCCEEDED(mediaEngine_->GetError(err.GetAddressOf())) && err) {
            USHORT code = err->GetErrorCode();
            HRESULT ext = err->GetExtendedErrorCode();
            wchar_t buf[160];
            swprintf_s(buf, L"Ошибка декодирования видео (код %u, HRESULT=0x%08X)",
                       static_cast<unsigned int>(code), static_cast<unsigned int>(ext));
            lastError_ = buf;
            LogError(lastError_);
        }
    }

    if (endedSignal_.exchange(false)) {
        mediaEngine_->SetCurrentTime(0.0);
        if (!IsEffectivelyPaused()) {
            mediaEngine_->Play();
        }
    }

    if (canPlay_.load() && !IsEffectivelyPaused() && mediaEngine_->IsPaused()) {
        UpdateAudioAndRate();
        mediaEngine_->Play();
    }

    // Smooth mouse cursor parallax interpolation
    float targetPx = 0.0f, targetPy = 0.0f;
    if (activeCfg_.mouseParallax && !IsEffectivelyPaused()) {
        POINT cur{};
        if (GetCursorPos(&cur)) {
            int sw = std::max(1, GetSystemMetrics(SM_CXSCREEN));
            int sh = std::max(1, GetSystemMetrics(SM_CYSCREEN));
            float nx = std::clamp(static_cast<float>(cur.x) / static_cast<float>(sw) - 0.5f, -0.5f, 0.5f);
            float ny = std::clamp(static_cast<float>(cur.y) / static_cast<float>(sh) - 0.5f, -0.5f, 0.5f);
            targetPx = nx * 0.036f;
            targetPy = ny * 0.036f;
        }
    }
    float prevPx = parallaxX_, prevPy = parallaxY_;
    parallaxX_ += (targetPx - parallaxX_) * 0.12f;
    parallaxY_ += (targetPy - parallaxY_) * 0.12f;
    bool parallaxMoved = (std::fabs(parallaxX_ - prevPx) > 0.00025f || std::fabs(parallaxY_ - prevPy) > 0.00025f);

    bool newVideoFrame = false;
    LONGLONG pts = 0;
    if (mediaEngine_->HasVideo() && mediaEngine_->OnVideoStreamTick(&pts) == S_OK) {
        DWORD nw = 0, nh = 0;
        if (SUCCEEDED(mediaEngine_->GetNativeVideoSize(&nw, &nh)) && nw > 0 && nh > 0) {
            if (EnsureVideoSurface(nw, nh)) {
                MFVideoNormalizedRect srcNorm{0.0f, 0.0f, 1.0f, 1.0f};
                RECT dstRc{0, 0, static_cast<LONG>(videoW_), static_cast<LONG>(videoH_)};
                MFARGB border{0, 0, 0, 255};
                if (SUCCEEDED(mediaEngine_->TransferVideoFrame(videoTex_.Get(), &srcNorm, &dstRc, &border))) {
                    newVideoFrame = true;
                }
            }
        }
    }

    if (!videoSrv_) return false;

    bool shouldPresentDesktop = newVideoFrame || dirtyVisuals_ || (parallaxMoved && !IsEffectivelyPaused());
    if (shouldPresentDesktop && swapChain_ && backbufferRtv_) {
        RenderSceneToTarget(backbufferRtv_.Get(), swapW_, swapH_, true);
        swapChain_->Present(0, 0);
        dirtyVisuals_ = false;
    }

    // Update live mini-preview inside Studio Inspector (~15 FPS when Studio window is open)
    if (capturePreviewForStudio && (newVideoFrame || shouldPresentDesktop) && previewRtv_ && previewStaging_) {
        ULONGLONG now = GetTickCount64();
        if (now - lastPreviewTickMs_ >= 60 || newVideoFrame) {
            lastPreviewTickMs_ = now;
            RenderSceneToTarget(previewRtv_.Get(), kPreviewWidth, kPreviewHeight, false);
            d3dCtx_->CopyResource(previewStaging_.Get(), previewTex_.Get());

            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(d3dCtx_->Map(previewStaging_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                std::lock_guard<std::mutex> lk(previewMutex_);
                const size_t rowBytes = kPreviewWidth * 4;
                previewCpuBuf_.resize(rowBytes * kPreviewHeight);
                for (uint32_t y = 0; y < kPreviewHeight; ++y) {
                    std::memcpy(
                        previewCpuBuf_.data() + y * rowBytes,
                        static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch,
                        rowBytes);
                }
                ++previewSeq_;
                d3dCtx_->Unmap(previewStaging_.Get(), 0);
            }
        }
    }

    return newVideoFrame;
}

bool VideoWallpaperEngine::CopyLatestPreviewFrame(
    uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outBGRA, uint64_t& ioSeq) {
    std::lock_guard<std::mutex> lk(previewMutex_);
    if (previewCpuBuf_.empty() || previewSeq_ == ioSeq) {
        return false;
    }
    outW = kPreviewWidth;
    outH = kPreviewHeight;
    outBGRA = previewCpuBuf_;
    ioSeq = previewSeq_;
    return true;
}

double VideoWallpaperEngine::DurationSec() const {
    if (!mediaEngine_ || !hasMedia_) return 0.0;
    double d = mediaEngine_->GetDuration();
    return std::isfinite(d) && d > 0.0 ? d : 0.0;
}

double VideoWallpaperEngine::CurrentTimeSec() const {
    if (!mediaEngine_ || !hasMedia_) return 0.0;
    double t = mediaEngine_->GetCurrentTime();
    return std::isfinite(t) && t >= 0.0 ? t : 0.0;
}

}  // namespace wp
