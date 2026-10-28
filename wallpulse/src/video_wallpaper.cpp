#include "video_wallpaper.h"

#include <d3dcompiler.h>
#include <mferror.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wp {

namespace {

constexpr const wchar_t* kRenderWndClass = L"WallPulseDesktopRenderWnd";

struct alignas(16) PostProcessCBData {
    float uvScaleX;
    float uvScaleY;
    float uvOffsetX;
    float uvOffsetY;

    float brightness;
    float contrast;
    float saturation;
    float hueShiftRad;

    float sharpen;
    float bloom;
    float chromatic;
    float crtEffect;

    float vignette;
    float loopBlend;
    float bassPulse;
    float timeSec;

    float invTexW;
    float invTexH;
    float pad0;
    float pad1;
};

struct alignas(16) ProcShaderCBData {
    float timeSec;
    float aspect;
    float mouseU;
    float mouseV;

    float bass;
    float mid;
    float high;
    float pad0;
};

const char* kQuadVS = R"(
struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut main(uint id : SV_VertexID) {
    VSOut o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}
)";

const char* kPostProcessPS = R"(
Texture2D texCur  : register(t0);
Texture2D texHead : register(t1);
SamplerState samp : register(s0);

cbuffer PostCB : register(b0) {
    float4 uvScaleOffset;   // xy=scale, zw=offset
    float4 colorParams;     // x=bright, y=contrast, z=sat, w=hueRad
    float4 proParams;       // x=sharpen, y=bloom, z=ca, w=vhs
    float4 dynParams;       // x=vignette, y=loopBlend, z=bassPulse, w=timeSec
    float4 texelParams;     // xy=1/texSize
};

float3 SampleBlended(float2 uv) {
    float3 c0 = texCur.Sample(samp, saturate(uv)).rgb;
    if (dynParams.y > 0.001) {
        float3 c1 = texHead.Sample(samp, saturate(uv)).rgb;
        c0 = lerp(c0, c1, saturate(dynParams.y));
    }
    return c0;
}

float3 RotateHue(float3 col, float angle) {
    if (abs(angle) < 0.001) return col;
    const float3 k = float3(0.57735027, 0.57735027, 0.57735027);
    float cosA = cos(angle);
    return col * cosA + cross(k, col) * sin(angle) + k * dot(k, col) * (1.0 - cosA);
}

float4 main(float4 pos : SV_POSITION, float2 inUv : TEXCOORD0) : SV_TARGET {
    float zoom = 1.0 - dynParams.z * 0.042;
    float2 screenUv = (inUv - 0.5) * zoom + 0.5;

    if (proParams.w > 0.001) {
        float wave = sin(screenUv.y * 95.0 + dynParams.w * 9.0) * 0.0018 * proParams.w;
        float glitchBand = smoothstep(0.96, 1.0, sin(screenUv.y * 6.0 - dynParams.w * 2.3));
        screenUv.x += wave + glitchBand * 0.009 * proParams.w;
    }

    float2 uv = screenUv * uvScaleOffset.xy + uvScaleOffset.zw;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float2 centered = inUv - 0.5;
    float caAmt = (proParams.z * 0.012 + proParams.w * 0.0035) * (0.35 + dot(centered, centered) * 2.2);
    float3 col;
    if (caAmt > 0.0002) {
        float2 dir = centered * caAmt;
        float r = SampleBlended(uv + dir).r;
        float g = SampleBlended(uv).g;
        float b = SampleBlended(uv - dir).b;
        col = float3(r, g, b);
    } else {
        col = SampleBlended(uv);
    }

    // Contrast-Adaptive Sharpening (AMD CAS inspired)
    if (proParams.x > 0.005) {
        float2 tx = texelParams.xy;
        float3 n = SampleBlended(uv + float2(0.0, -tx.y));
        float3 s = SampleBlended(uv + float2(0.0,  tx.y));
        float3 w = SampleBlended(uv + float2(-tx.x, 0.0));
        float3 e = SampleBlended(uv + float2( tx.x, 0.0));
        float3 mn = min(col, min(min(n, s), min(w, e)));
        float3 mx = max(col, max(max(n, s), max(w, e)));
        float3 amp = saturate(min(mn, 1.0 - mx) / max(mx, 0.001));
        float3 weight = -sqrt(amp) * (0.18 * proParams.x);
        col = saturate((col + (n + s + w + e) * weight) / (1.0 + 4.0 * weight));
    }

    // Highlight Bloom + Bass Glow
    float bloomAmt = proParams.y + dynParams.z * 0.28;
    if (bloomAmt > 0.005) {
        float2 bStep = texelParams.xy * 4.5;
        float3 acc = 0.0;
        acc += max(0.0, SampleBlended(uv + float2( bStep.x,  0.0))     - 0.42);
        acc += max(0.0, SampleBlended(uv + float2(-bStep.x,  0.0))     - 0.42);
        acc += max(0.0, SampleBlended(uv + float2( 0.0,      bStep.y)) - 0.42);
        acc += max(0.0, SampleBlended(uv + float2( 0.0,     -bStep.y)) - 0.42);
        acc += max(0.0, SampleBlended(uv + float2( bStep.x,  bStep.y)) - 0.42);
        acc += max(0.0, SampleBlended(uv + float2(-bStep.x,  bStep.y)) - 0.42);
        acc += max(0.0, SampleBlended(uv + float2( bStep.x, -bStep.y)) - 0.42);
        acc += max(0.0, SampleBlended(uv + float2(-bStep.x, -bStep.y)) - 0.42);
        col += (acc * 0.16) * bloomAmt;
    }

    if (proParams.w > 0.005) {
        float scan = 0.90 + 0.10 * sin(pos.y * 3.14159);
        col *= lerp(1.0, scan, proParams.w * 0.85);
    }

    col = (col - 0.5) * colorParams.y + 0.5 + colorParams.x;
    float luma = dot(col, float3(0.2126, 0.7152, 0.0722));
    col = lerp(float3(luma, luma, luma), col, colorParams.z);
    col = RotateHue(col, colorParams.w);

    if (dynParams.x > 0.001) {
        float2 d = inUv - 0.5;
        float vig = saturate(1.0 - dot(d, d) * (1.85 * dynParams.x));
        col *= vig;
    }

    return float4(saturate(col), 1.0);
}
)";

const char* kCrossfadePS = R"(
Texture2D prevTex : register(t0);
SamplerState samp : register(s0);

cbuffer CrossCB : register(b0) {
    float4 params;
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float3 c = prevTex.Sample(samp, uv).rgb;
    return float4(c, saturate(params.x));
}
)";

const char* kShaderCyberTunnel = R"(
cbuffer ProcCB : register(b0) {
    float timeSec; float aspect; float mouseU; float mouseV;
    float bass; float mid; float high; float pad0;
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 p = (uv - 0.5) * float2(aspect, 1.0) * 2.0;
    p += (float2(mouseU, mouseV) - 0.5) * 0.35;
    float r = max(0.04, length(p));
    float a = atan2(p.y, p.x);
    float z = 0.65 / r + timeSec * (1.15 + bass * 0.45);

    float hexWave = sin(a * 6.0 + sin(z * 1.4) * 1.5);
    float ringGrid = exp(-abs(sin(z * 3.14159)) * (11.0 - bass * 5.0));
    float spokeGrid = exp(-abs(sin(a * 6.0 + z * 0.4)) * 12.0);
    float grid = max(ringGrid, spokeGrid);

    float fog = saturate(r * 1.45);
    float3 neonA = float3(0.06, 0.72, 0.98);
    float3 neonB = float3(0.92, 0.20, 0.82);
    float3 lineCol = lerp(neonA, neonB, 0.5 + 0.5 * hexWave);

    float3 col = float3(0.03, 0.04, 0.09) + lineCol * grid * fog * (1.15 + bass * 0.9);
    col += float3(0.35, 0.12, 0.55) * exp(-r * 3.2) * (0.7 + mid * 0.8);
    return float4(saturate(col), 1.0);
}
)";

const char* kShaderSunsetOcean = R"(
cbuffer ProcCB : register(b0) {
    float timeSec; float aspect; float mouseU; float mouseV;
    float bass; float mid; float high; float pad0;
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 p = (uv - 0.5) * float2(aspect, 1.0) * 2.0;
    float horizon = 0.04 + (mouseV - 0.5) * 0.12;
    float3 col = float3(0.05, 0.04, 0.14);

    if (p.y < horizon) {
        float t = saturate((horizon - p.y) / 1.15);
        col = lerp(float3(0.98, 0.42, 0.34), float3(0.11, 0.07, 0.28), pow(t, 0.72));

        float2 sunPos = float2((mouseU - 0.5) * 0.35, horizon - 0.26);
        float sd = length(p - sunPos);
        float sunR = 0.34 + bass * 0.035;
        if (sd < sunR) {
            float stripe = sin((p.y - sunPos.y) * 58.0 + timeSec * 2.5);
            if (p.y < sunPos.y - 0.04 || stripe > -0.25) {
                col = lerp(float3(1.0, 0.92, 0.55), float3(1.0, 0.28, 0.52), saturate((p.y - (sunPos.y - sunR)) / (2.0 * sunR)));
            }
        }
        col += float3(0.95, 0.32, 0.48) * exp(-max(0.0, sd - sunR) * 4.2) * (0.45 + bass * 0.45);
    } else {
        float dy = max(0.012, p.y - horizon);
        float z = 0.42 / dy;
        float w1 = sin(p.x * z * 2.8 + timeSec * 2.4) * 0.5;
        float w2 = cos(p.x * z * 5.3 - timeSec * 3.1 + z * 1.9) * 0.5;
        float ripple = (w1 + w2) * 0.035;

        float sunRefl = exp(-abs(p.x - (mouseU - 0.5) * 0.35 + ripple) * (4.2 - bass * 1.4));
        float3 waterBase = lerp(float3(0.14, 0.12, 0.30), float3(0.03, 0.08, 0.18), saturate(dy * 1.4));
        col = waterBase + float3(1.0, 0.48, 0.36) * sunRefl * (0.55 + 0.45 * sin(z * 4.0 - timeSec * 4.0));
    }
    return float4(saturate(col), 1.0);
}
)";

const char* kShaderMatrixPulse = R"(
cbuffer ProcCB : register(b0) {
    float timeSec; float aspect; float mouseU; float mouseV;
    float bass; float mid; float high; float pad0;
};

float Hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 gridUv = uv * float2(48.0 * aspect, 32.0);
    float2 cell = floor(gridUv);
    float2 f = frac(gridUv);

    float speed = 1.6 + Hash21(float2(cell.x, 1.0)) * 2.4;
    float rainY = frac(uv.y * 1.4 - timeSec * speed * 0.22 + Hash21(float2(cell.x, 7.0)));
    float trail = exp(-rainY * 7.5);
    float head = exp(-rainY * 42.0);

    float glyph = step(0.22, Hash21(cell + floor(timeSec * 8.0)));
    float border = step(0.12, f.x) * step(f.x, 0.88) * step(0.12, f.y) * step(f.y, 0.88);
    float intensity = (trail * 0.85 + head * 1.5) * glyph * border;

    float2 mp = (uv - float2(mouseU, mouseV)) * float2(aspect, 1.0);
    float pulseRing = exp(-abs(length(mp) - frac(timeSec * 0.45) * 0.9) * 24.0) * (0.4 + bass * 0.9);

    float3 col = float3(0.01, 0.03, 0.03);
    col += float3(0.08, 0.95, 0.52) * intensity * (0.85 + high * 0.6);
    col += float3(0.75, 1.0, 0.90) * head * glyph * border;
    col += float3(0.06, 0.72, 0.95) * pulseRing;
    return float4(saturate(col), 1.0);
}
)";

D2D1_COLOR_F HexToColorF(uint32_t rgb, float alpha = 1.0f) {
    return D2D1::ColorF(
        ((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >> 8) & 0xFF) / 255.0f,
        (rgb & 0xFF) / 255.0f,
        alpha);
}

ComPtr<ID3D11PixelShader> CompilePSFromSource(ID3D11Device* dev, const char* src, size_t len) {
    ComPtr<ID3DBlob> blob;
    if (FAILED(D3DCompile(src, len, nullptr, nullptr, nullptr, "main", "ps_4_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob.GetAddressOf(), nullptr)) || !blob) {
        return {};
    }
    ComPtr<ID3D11PixelShader> ps;
    dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ps.GetAddressOf());
    return ps;
}

}  // namespace

class MediaEngineNotify final : public IMFMediaEngineNotify {
public:
    MediaEngineNotify(VideoWallpaperPlayer* owner, size_t slotIdx)
        : owner_(owner), slotIdx_(slotIdx) {}

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
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2) override {
        if (owner_) owner_->OnMediaEngineEvent(slotIdx_, meEvent, param1, param2);
        return S_OK;
    }

    void DetachOwner() { owner_ = nullptr; }

private:
    long refCount_ = 1;
    VideoWallpaperPlayer* owner_ = nullptr;
    size_t slotIdx_ = 0;
};

VideoWallpaperPlayer::VideoWallpaperPlayer() = default;

VideoWallpaperPlayer::~VideoWallpaperPlayer() {
    Shutdown();
}

LRESULT CALLBACK VideoWallpaperPlayer::RenderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool VideoWallpaperPlayer::Initialize(HINSTANCE hInstance) {
    hInstance_ = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = RenderWndProc;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wc.lpszClassName = kRenderWndClass;
    RegisterClassExW(&wc);

    renderHwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kRenderWndClass,
        L"WallPulse Render Layer",
        WS_POPUP,
        0, 0, 1280, 720,
        nullptr, nullptr, hInstance_, this);

    if (!renderHwnd_) return false;
    return InitD3DResources();
}

bool VideoWallpaperPlayer::InitD3DResources() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
        d3dDevice_.GetAddressOf(), &createdLevel, d3dContext_.GetAddressOf());

    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            d3dDevice_.GetAddressOf(), &createdLevel, d3dContext_.GetAddressOf());
    }
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            d3dDevice_.GetAddressOf(), &createdLevel, d3dContext_.GetAddressOf());
    }
    if (FAILED(hr) || !d3dDevice_) return false;

    ComPtr<ID3D10Multithread> mt;
    if (SUCCEEDED(d3dDevice_.As(&mt)) && mt) {
        mt->SetMultithreadProtected(TRUE);
    }

    if (SUCCEEDED(MFCreateDXGIDeviceManager(&resetToken_, dxgiManager_.GetAddressOf())) && dxgiManager_) {
        dxgiManager_->ResetDevice(d3dDevice_.Get(), resetToken_);
    }

    ComPtr<ID3DBlob> vsBlob;
    if (FAILED(D3DCompile(kQuadVS, std::strlen(kQuadVS), nullptr, nullptr, nullptr,
                          "main", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                          vsBlob.GetAddressOf(), nullptr))) {
        return false;
    }
    if (FAILED(d3dDevice_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs_.GetAddressOf()))) {
        return false;
    }

    postProcessPS_ = CompilePSFromSource(d3dDevice_.Get(), kPostProcessPS, std::strlen(kPostProcessPS));
    crossfadePS_ = CompilePSFromSource(d3dDevice_.Get(), kCrossfadePS, std::strlen(kCrossfadePS));
    builtinShaderPS_[0] = CompilePSFromSource(d3dDevice_.Get(), kShaderCyberTunnel, std::strlen(kShaderCyberTunnel));
    builtinShaderPS_[1] = CompilePSFromSource(d3dDevice_.Get(), kShaderSunsetOcean, std::strlen(kShaderSunsetOcean));
    builtinShaderPS_[2] = CompilePSFromSource(d3dDevice_.Get(), kShaderMatrixPulse, std::strlen(kShaderMatrixPulse));
    if (!postProcessPS_ || !crossfadePS_) return false;

    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    d3dDevice_->CreateSamplerState(&sampDesc, samplerLinear_.GetAddressOf());

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(PostProcessCBData);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d3dDevice_->CreateBuffer(&cbDesc, nullptr, postCB_.GetAddressOf());

    cbDesc.ByteWidth = sizeof(ProcShaderCBData);
    d3dDevice_->CreateBuffer(&cbDesc, nullptr, procShaderCB_.GetAddressOf());

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    d3dDevice_->CreateBlendState(&blendDesc, alphaBlendState_.GetAddressOf());

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));

    if (dwriteFactory_) {
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 54.0f, L"ru-RU", fmtClockTime_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 15.5f, L"ru-RU", fmtClockDate_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 13.5f, L"ru-RU", fmtWidgetBody_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ru-RU", fmtWidgetSmall_.GetAddressOf());
    }

    return CreateOrResizeSwapChain(targetW_, targetH_);
}

bool VideoWallpaperPlayer::RebuildD2DTarget() {
    d2dTarget_.Reset();
    if (!d2dFactory_ || !swapChain_) return false;

    ComPtr<IDXGISurface> dxgiSurf;
    if (FAILED(swapChain_->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(dxgiSurf.GetAddressOf()))) || !dxgiSurf) {
        return false;
    }

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    return SUCCEEDED(d2dFactory_->CreateDxgiSurfaceRenderTarget(dxgiSurf.Get(), &props, d2dTarget_.GetAddressOf()));
}

bool VideoWallpaperPlayer::CreateOrResizeSwapChain(uint32_t width, uint32_t height) {
    width = std::max<uint32_t>(64, width);
    height = std::max<uint32_t>(64, height);
    targetW_ = width;
    targetH_ = height;

    d2dTarget_.Reset();
    backBufferRTV_.Reset();
    crossfadePrevSRV_.Reset();
    crossfadePrevTex_.Reset();

    if (swapChain_) {
        HRESULT hr = swapChain_->ResizeBuffers(2, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        if (FAILED(hr)) swapChain_.Reset();
    }

    if (!swapChain_) {
        ComPtr<IDXGIDevice> dxgiDev;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory2;
        if (FAILED(d3dDevice_.As(&dxgiDev)) ||
            FAILED(dxgiDev->GetAdapter(adapter.GetAddressOf())) ||
            FAILED(adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory2.GetAddressOf())))) {
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 scDesc{};
        scDesc.Width = width;
        scDesc.Height = height;
        scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        scDesc.SampleDesc.Count = 1;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.BufferCount = 2;
        scDesc.Scaling = DXGI_SCALING_STRETCH;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        HRESULT hr = factory2->CreateSwapChainForHwnd(
            d3dDevice_.Get(), renderHwnd_, &scDesc, nullptr, nullptr, swapChain_.GetAddressOf());
        if (FAILED(hr)) {
            scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            hr = factory2->CreateSwapChainForHwnd(
                d3dDevice_.Get(), renderHwnd_, &scDesc, nullptr, nullptr, swapChain_.GetAddressOf());
            if (FAILED(hr)) return false;
        }
        factory2->MakeWindowAssociation(renderHwnd_, DXGI_MWA_NO_ALT_ENTER);
    }

    ComPtr<ID3D11Texture2D> backBuf;
    if (FAILED(swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuf.GetAddressOf())))) {
        return false;
    }
    if (FAILED(d3dDevice_->CreateRenderTargetView(backBuf.Get(), nullptr, backBufferRTV_.GetAddressOf()))) {
        return false;
    }

    RebuildD2DTarget();
    return true;
}

bool VideoWallpaperPlayer::EnsureSlotTextures(MediaSlot& slot, uint32_t width, uint32_t height, bool needRtv) {
    width = std::max<uint32_t>(16, width);
    height = std::max<uint32_t>(16, height);
    if (slot.frameTex && slot.texW == width && slot.texH == height && (!needRtv || slot.frameRTV)) {
        return true;
    }

    slot.frameRTV.Reset();
    slot.frameSRV.Reset();
    slot.frameTex.Reset();
    slot.loopHeadSRV.Reset();
    slot.loopHeadTex.Reset();
    slot.loopHeadCaptured = false;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(d3dDevice_->CreateTexture2D(&td, nullptr, slot.frameTex.GetAddressOf()))) return false;
    if (FAILED(d3dDevice_->CreateShaderResourceView(slot.frameTex.Get(), nullptr, slot.frameSRV.GetAddressOf()))) return false;
    if (needRtv) {
        if (FAILED(d3dDevice_->CreateRenderTargetView(slot.frameTex.Get(), nullptr, slot.frameRTV.GetAddressOf()))) return false;
    }

    if (SUCCEEDED(d3dDevice_->CreateTexture2D(&td, nullptr, slot.loopHeadTex.GetAddressOf()))) {
        d3dDevice_->CreateShaderResourceView(slot.loopHeadTex.Get(), nullptr, slot.loopHeadSRV.GetAddressOf());
    }

    slot.texW = width;
    slot.texH = height;
    return true;
}

void VideoWallpaperPlayer::ShutdownSlot(MediaSlot& slot) {
    if (slot.notify) {
        slot.notify->DetachOwner();
        slot.notify.Reset();
    }
    if (slot.mediaEngine) {
        slot.mediaEngine->Pause();
        slot.mediaEngine->Shutdown();
        slot.mediaEngine.Reset();
    }
    if (slot.webHost) {
        slot.webHost->Close();
        slot.webHost.reset();
    }
    slot.gif = {};
    slot.customShaderPS.Reset();
    slot.frameRTV.Reset();
    slot.frameSRV.Reset();
    slot.frameTex.Reset();
    slot.loopHeadSRV.Reset();
    slot.loopHeadTex.Reset();
    slot.engineReady = false;
}

bool VideoWallpaperPlayer::SetupSlot(
    size_t slotIdx, const std::wstring& path, const WallpaperProfile& prof,
    const RECT& localVp, const RECT& localWork) {
    if (slotIdx >= slots_.size()) return false;
    MediaSlot& slot = slots_[slotIdx];
    ShutdownSlot(slot);

    slot.path = path;
    slot.kind = ClassifyWallpaperPath(path);
    slot.profile = prof;
    slot.localViewport = localVp;
    slot.localWorkRect = localWork;

    if (slot.kind == WallpaperKind::Web) {
        slot.webHost = std::make_unique<WebWallpaperHost>();
        if (slot.webHost->AttachAndNavigate(hInstance_, renderHwnd_, localVp, path)) {
            slot.engineReady = true;
            return true;
        }
        return false;
    }

    if (slot.kind == WallpaperKind::Gif) {
        if (!DecodeAnimatedGif(path, slot.gif) || slot.gif.frames.empty()) return false;
        if (!EnsureSlotTextures(slot, slot.gif.width, slot.gif.height, false)) return false;
        d3dContext_->UpdateSubresource(slot.frameTex.Get(), 0, nullptr,
                                       slot.gif.frames[0].bgra.data(), slot.gif.width * 4, 0);
        slot.gifFrameIdx = 0;
        slot.gifAccumMs = 0.0;
        slot.gifLastTickMs = GetTickCount64();
        slot.engineReady = true;
        return true;
    }

    if (slot.kind == WallpaperKind::Shader) {
        if (path == L"shader://cyber_tunnel") slot.builtinShaderId = 0;
        else if (path == L"shader://sunset_ocean") slot.builtinShaderId = 1;
        else if (path == L"shader://matrix_pulse") slot.builtinShaderId = 2;
        else {
            ComPtr<ID3DBlob> blob;
            if (SUCCEEDED(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ps_4_0",
                                             D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob.GetAddressOf(), nullptr)) && blob) {
                d3dDevice_->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                             nullptr, slot.customShaderPS.GetAddressOf());
            }
            if (!slot.customShaderPS) slot.builtinShaderId = 0;
        }

        uint32_t vpW = static_cast<uint32_t>(std::max<LONG>(320, localVp.right - localVp.left));
        uint32_t vpH = static_cast<uint32_t>(std::max<LONG>(180, localVp.bottom - localVp.top));
        if (!EnsureSlotTextures(slot, vpW, vpH, true)) return false;
        slot.shaderTimeSec = 0.0;
        slot.shaderLastTickMs = GetTickCount64();
        slot.engineReady = true;
        return true;
    }

    ComPtr<IMFMediaEngineClassFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMFMediaEngineClassFactory), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr) || !factory) return false;

    slot.notify.Attach(new MediaEngineNotify(this, slotIdx));

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(attrs.GetAddressOf(), 4);
    attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, static_cast<IMFMediaEngineNotify*>(slot.notify.Get()));
    attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);
    if (dxgiManager_) {
        attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, dxgiManager_.Get());
    }

    hr = factory->CreateInstance(0, attrs.Get(), slot.mediaEngine.GetAddressOf());
    if (FAILED(hr) || !slot.mediaEngine) return false;

    slot.mediaEngine->SetLoop(TRUE);
    slot.mediaEngine->SetAutoPlay(TRUE);
    ApplyAudioAndSpeedToSlot(slot);

    BSTR bstrUrl = SysAllocString(path.c_str());
    hr = slot.mediaEngine->SetSource(bstrUrl);
    SysFreeString(bstrUrl);
    if (FAILED(hr)) return false;

    slot.mediaEngine->Load();
    return true;
}

void VideoWallpaperPlayer::CaptureCrossfadeSnapshot(float durationSec) {
    if (durationSec <= 0.05f || !loaded_ || !swapChain_ || !d3dDevice_ || !d3dContext_) {
        crossfadeDurSec_ = 0.0f;
        return;
    }

    ComPtr<ID3D11Texture2D> backBuf;
    if (FAILED(swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuf.GetAddressOf()))) || !backBuf) {
        return;
    }

    D3D11_TEXTURE2D_DESC td{};
    backBuf->GetDesc(&td);
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;

    crossfadePrevSRV_.Reset();
    crossfadePrevTex_.Reset();
    if (SUCCEEDED(d3dDevice_->CreateTexture2D(&td, nullptr, crossfadePrevTex_.GetAddressOf())) && crossfadePrevTex_) {
        d3dContext_->CopyResource(crossfadePrevTex_.Get(), backBuf.Get());
        if (SUCCEEDED(d3dDevice_->CreateShaderResourceView(crossfadePrevTex_.Get(), nullptr, crossfadePrevSRV_.GetAddressOf()))) {
            crossfadeStartMs_ = GetTickCount64();
            crossfadeDurSec_ = std::clamp(durationSec, 0.1f, 4.0f);
        }
    }
}

bool VideoWallpaperPlayer::LoadAndApply(
    const std::wstring& primaryPath,
    const WallpaperProfile& primaryProfile,
    const AppConfig& config,
    const std::vector<WallpaperItem>& library,
    bool attachToDesktop) {
    if (!WallpaperSourceExists(primaryPath)) return false;
    if (!d3dDevice_ && !InitD3DResources()) return false;

    config_ = config;
    primaryProfile_ = primaryProfile;
    currentPath_ = primaryPath;
    attachedToDesktop_ = attachToDesktop;
    hasError_ = false;
    renderedFrames_ = 0;

    for (auto& s : slots_) ShutdownSlot(s);
    slots_.clear();
    cloneViewports_.clear();
    cloneWorkRects_.clear();

    auto monitors = desktopHost_.EnumerateMonitors();
    RECT screenBounds = desktopHost_.ComputeTargetScreenRect(config_.monitorMode, config_.targetMonitorIndex);

    auto findProfileForPath = [&](const std::wstring& p) -> WallpaperProfile {
        for (const auto& item : library) {
            if (_wcsicmp(item.path.c_str(), p.c_str()) == 0) return item.profile;
        }
        return primaryProfile;
    };

    if (!attachToDesktop) {
        CreateOrResizeSwapChain(targetW_, targetH_);
        slots_.resize(1);
        RECT vp{0, 0, static_cast<LONG>(targetW_), static_cast<LONG>(targetH_)};
        loaded_ = SetupSlot(0, primaryPath, primaryProfile, vp, vp);
        return loaded_;
    }

    if (config_.monitorMode == MonitorMode::SpanAcross) {
        uint32_t w = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.right - screenBounds.left));
        uint32_t h = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.bottom - screenBounds.top));
        CreateOrResizeSwapChain(w, h);
        slots_.resize(1);
        RECT vp{0, 0, static_cast<LONG>(w), static_cast<LONG>(h)};
        loaded_ = SetupSlot(0, primaryPath, primaryProfile, vp, vp);
    } else if (config_.monitorMode == MonitorMode::AllSame && !monitors.empty()) {
        uint32_t w = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.right - screenBounds.left));
        uint32_t h = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.bottom - screenBounds.top));
        CreateOrResizeSwapChain(w, h);

        for (const auto& m : monitors) {
            cloneViewports_.push_back(RECT{
                m.bounds.left - screenBounds.left, m.bounds.top - screenBounds.top,
                m.bounds.right - screenBounds.left, m.bounds.bottom - screenBounds.top});
            cloneWorkRects_.push_back(RECT{
                m.workArea.left - screenBounds.left, m.workArea.top - screenBounds.top,
                m.workArea.right - screenBounds.left, m.workArea.bottom - screenBounds.top});
        }
        slots_.resize(1);
        loaded_ = SetupSlot(0, primaryPath, primaryProfile, cloneViewports_[0], cloneWorkRects_[0]);
    } else if (config_.monitorMode == MonitorMode::PerMonitorIndependent && !monitors.empty()) {
        uint32_t w = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.right - screenBounds.left));
        uint32_t h = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.bottom - screenBounds.top));
        CreateOrResizeSwapChain(w, h);

        slots_.resize(monitors.size());
        bool anyOk = false;
        for (size_t i = 0; i < monitors.size(); ++i) {
            std::wstring mPath = primaryPath;
            if (i < config_.monitorWallpapers.size() && WallpaperSourceExists(config_.monitorWallpapers[i])) {
                mPath = config_.monitorWallpapers[i];
            }
            WallpaperProfile mProf = findProfileForPath(mPath);
            if (i > 0 && !slots_[0].profile.muted) {
                mProf.muted = true;
            }
            RECT vp{
                monitors[i].bounds.left - screenBounds.left, monitors[i].bounds.top - screenBounds.top,
                monitors[i].bounds.right - screenBounds.left, monitors[i].bounds.bottom - screenBounds.top};
            RECT wk{
                monitors[i].workArea.left - screenBounds.left, monitors[i].workArea.top - screenBounds.top,
                monitors[i].workArea.right - screenBounds.left, monitors[i].workArea.bottom - screenBounds.top};
            if (SetupSlot(i, mPath, mProf, vp, wk)) {
                anyOk = true;
            }
        }
        loaded_ = anyOk;
    } else {
        // PrimaryOnly or SpecificMonitor
        MonitorDesc chosen = monitors.empty() ? MonitorDesc{} : monitors[0];
        if (!monitors.empty()) {
            if (config_.monitorMode == MonitorMode::SpecificMonitor &&
                config_.targetMonitorIndex >= 0 &&
                static_cast<size_t>(config_.targetMonitorIndex) < monitors.size()) {
                chosen = monitors[static_cast<size_t>(config_.targetMonitorIndex)];
            } else {
                for (const auto& m : monitors) {
                    if (m.isPrimary) { chosen = m; break; }
                }
            }
            screenBounds = chosen.bounds;
        }
        uint32_t w = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.right - screenBounds.left));
        uint32_t h = static_cast<uint32_t>(std::max<LONG>(64, screenBounds.bottom - screenBounds.top));
        CreateOrResizeSwapChain(w, h);
        slots_.resize(1);
        RECT vp{0, 0, static_cast<LONG>(w), static_cast<LONG>(h)};
        RECT wk = vp;
        if (chosen.workArea.right > chosen.workArea.left) {
            wk = RECT{
                chosen.workArea.left - screenBounds.left, chosen.workArea.top - screenBounds.top,
                chosen.workArea.right - screenBounds.left, chosen.workArea.bottom - screenBounds.top};
        }
        loaded_ = SetupSlot(0, primaryPath, primaryProfile, vp, wk);
    }

    desktopHost_.AttachRenderWindow(renderHwnd_, screenBounds);
    return loaded_;
}

void VideoWallpaperPlayer::ReattachDesktop() {
    if (!loaded_ || !renderHwnd_ || !attachedToDesktop_) return;
    desktopHost_.DiscoverAndPrepare();
    LoadAndApply(currentPath_, primaryProfile_, config_, config_.library, true);
}

void VideoWallpaperPlayer::UpdateLiveSettings(
    const WallpaperProfile& activeProfile,
    const AppConfig& config,
    const std::vector<WallpaperItem>& library) {
    const bool layoutChanged =
        (config.monitorMode != config_.monitorMode) ||
        (config.targetMonitorIndex != config_.targetMonitorIndex) ||
        (config.monitorWallpapers != config_.monitorWallpapers);

    config_ = config;
    primaryProfile_ = activeProfile;

    if (!loaded_) return;
    if (layoutChanged && attachedToDesktop_) {
        LoadAndApply(currentPath_, primaryProfile_, config_, library, true);
        return;
    }

    if (!slots_.empty()) {
        if (config_.monitorMode == MonitorMode::PerMonitorIndependent) {
            for (size_t i = 0; i < slots_.size(); ++i) {
                for (const auto& item : library) {
                    if (_wcsicmp(item.path.c_str(), slots_[i].path.c_str()) == 0) {
                        slots_[i].profile = item.profile;
                        if (i > 0 && !slots_[0].profile.muted) slots_[i].profile.muted = true;
                        break;
                    }
                }
                ApplyAudioAndSpeedToSlot(slots_[i]);
            }
        } else {
            slots_[0].profile = activeProfile;
            ApplyAudioAndSpeedToSlot(slots_[0]);
        }
    }
}

void VideoWallpaperPlayer::ApplyAudioAndSpeedToSlot(MediaSlot& slot) {
    if (!slot.mediaEngine) return;
    const bool effMuted = slot.profile.muted || smartMuted_ || IsEffectivelyPaused();
    slot.mediaEngine->SetMuted(effMuted ? TRUE : FALSE);
    slot.mediaEngine->SetVolume(std::clamp(static_cast<double>(slot.profile.volume), 0.0, 1.0));

    double rate = std::clamp(static_cast<double>(slot.profile.playbackRate), 0.25, 2.5);
    slot.mediaEngine->SetDefaultPlaybackRate(rate);
    slot.mediaEngine->SetPlaybackRate(rate);
}

void VideoWallpaperPlayer::SetUserPaused(bool paused) {
    userPaused_ = paused;
    for (auto& s : slots_) {
        if (!s.mediaEngine || !s.engineReady) continue;
        if (IsEffectivelyPaused()) s.mediaEngine->Pause();
        else s.mediaEngine->Play();
        ApplyAudioAndSpeedToSlot(s);
    }
}

void VideoWallpaperPlayer::SetSmartPaused(bool paused) {
    if (smartPaused_ == paused) return;
    smartPaused_ = paused;
    for (auto& s : slots_) {
        if (!s.mediaEngine || !s.engineReady) continue;
        if (IsEffectivelyPaused()) s.mediaEngine->Pause();
        else s.mediaEngine->Play();
        ApplyAudioAndSpeedToSlot(s);
    }
}

void VideoWallpaperPlayer::SetSmartMuted(bool muted) {
    if (smartMuted_ == muted) return;
    smartMuted_ = muted;
    for (auto& s : slots_) ApplyAudioAndSpeedToSlot(s);
}

void VideoWallpaperPlayer::OnMediaEngineEvent(size_t slotIdx, DWORD meEvent, DWORD_PTR, DWORD) {
    if (slotIdx >= slots_.size()) return;
    MediaSlot& slot = slots_[slotIdx];

    switch (meEvent) {
        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
        case MF_MEDIA_ENGINE_EVENT_CANPLAY: {
            slot.engineReady = true;
            if (slot.mediaEngine) {
                DWORD vw = 0, vh = 0;
                if (SUCCEEDED(slot.mediaEngine->GetNativeVideoSize(&vw, &vh)) && vw > 0 && vh > 0) {
                    EnsureSlotTextures(slot, vw, vh, false);
                }
                ApplyAudioAndSpeedToSlot(slot);
                double dur = slot.mediaEngine->GetDuration();
                if (!std::isnan(dur) && dur > 0.5 && slot.profile.trimStartPct > 0.01f) {
                    slot.mediaEngine->SetCurrentTime(dur * std::clamp(slot.profile.trimStartPct, 0.0f, 0.90f));
                }
                if (!IsEffectivelyPaused()) {
                    slot.mediaEngine->Play();
                }
            }
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_ENDED: {
            if (slot.mediaEngine && !IsEffectivelyPaused()) {
                double dur = slot.mediaEngine->GetDuration();
                double startSec = (!std::isnan(dur) && dur > 0.5)
                    ? (dur * std::clamp(slot.profile.trimStartPct, 0.0f, 0.90f))
                    : 0.0;
                slot.mediaEngine->SetCurrentTime(startSec);
                slot.mediaEngine->Play();
            }
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_ERROR: {
            if (slotIdx == 0) hasError_ = true;
            break;
        }
        default:
            break;
    }
}

bool VideoWallpaperPlayer::IsReadyToPlay() const {
    return !slots_.empty() && slots_[0].engineReady;
}

double VideoWallpaperPlayer::DurationSeconds() const {
    if (slots_.empty()) return 0.0;
    const auto& s = slots_[0];
    if (s.kind == WallpaperKind::Video && s.mediaEngine) {
        double d = s.mediaEngine->GetDuration();
        return (std::isnan(d) || std::isinf(d) || d < 0.0) ? 0.0 : d;
    }
    if (s.kind == WallpaperKind::Gif) {
        uint64_t ms = 0;
        for (const auto& f : s.gif.frames) ms += f.delayMs;
        return static_cast<double>(ms) / 1000.0;
    }
    return 0.0;
}

double VideoWallpaperPlayer::CurrentTimeSeconds() const {
    if (slots_.empty()) return 0.0;
    const auto& s = slots_[0];
    if (s.kind == WallpaperKind::Video && s.mediaEngine) {
        double t = s.mediaEngine->GetCurrentTime();
        return (std::isnan(t) || std::isinf(t) || t < 0.0) ? 0.0 : t;
    }
    if (s.kind == WallpaperKind::Shader) return s.shaderTimeSec;
    return 0.0;
}

void VideoWallpaperPlayer::SeekSeconds(double sec) {
    if (slots_.empty()) return;
    auto& s = slots_[0];
    if (s.kind == WallpaperKind::Video && s.mediaEngine && s.engineReady) {
        s.mediaEngine->SetCurrentTime(std::max(0.0, sec));
        s.loopHeadCaptured = false;
    }
}

void VideoWallpaperPlayer::UpdateSlotFrame(MediaSlot& slot, const AudioSpectrumSnapshot& audio, float mouseU, float mouseV) {
    ULONGLONG nowMs = GetTickCount64();

    if (slot.kind == WallpaperKind::Video) {
        if (!slot.mediaEngine || !slot.engineReady) return;

        double dur = slot.mediaEngine->GetDuration();
        double effStart = 0.0;
        if (!std::isnan(dur) && !std::isinf(dur) && dur > 0.5) {
            effStart = dur * std::clamp(slot.profile.trimStartPct, 0.0f, 0.90f);
            double effEnd = dur * std::clamp(slot.profile.trimEndPct, 0.10f, 1.00f);
            if (effEnd <= effStart + 0.3) effEnd = dur;
            double cur = slot.mediaEngine->GetCurrentTime();
            if (!std::isnan(cur) && (cur < effStart - 0.15 || cur >= effEnd - 0.03) && (nowMs - slot.lastLoopSeekMs > 250)) {
                slot.lastLoopSeekMs = nowMs;
                slot.mediaEngine->SetCurrentTime(effStart);
            }
        }

        LONGLONG pts = 0;
        if (slot.mediaEngine->OnVideoStreamTick(&pts) == S_OK) {
            DWORD vw = 0, vh = 0;
            if (SUCCEEDED(slot.mediaEngine->GetNativeVideoSize(&vw, &vh)) && vw > 0 && vh > 0) {
                EnsureSlotTextures(slot, vw, vh, false);
            }
            if (slot.frameTex) {
                MFVideoNormalizedRect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
                RECT dstRect{0, 0, static_cast<LONG>(slot.texW), static_cast<LONG>(slot.texH)};
                MFARGB border{0, 0, 0, 255};
                if (SUCCEEDED(slot.mediaEngine->TransferVideoFrame(slot.frameTex.Get(), &srcRect, &dstRect, &border))) {
                    double cur = slot.mediaEngine->GetCurrentTime();
                    if (!slot.loopHeadCaptured && slot.loopHeadTex && cur >= effStart && cur <= effStart + 0.35) {
                        d3dContext_->CopyResource(slot.loopHeadTex.Get(), slot.frameTex.Get());
                        slot.loopHeadCaptured = true;
                    }
                }
            }
        }
    } else if (slot.kind == WallpaperKind::Gif) {
        if (slot.gif.frames.empty() || !slot.frameTex) return;
        double dtMs = static_cast<double>(nowMs - slot.gifLastTickMs) * std::clamp(slot.profile.playbackRate, 0.25f, 2.5f);
        slot.gifLastTickMs = nowMs;
        slot.gifAccumMs += std::min(dtMs, 250.0);

        bool changed = false;
        while (slot.gifAccumMs >= slot.gif.frames[slot.gifFrameIdx].delayMs) {
            slot.gifAccumMs -= slot.gif.frames[slot.gifFrameIdx].delayMs;
            slot.gifFrameIdx = (slot.gifFrameIdx + 1) % slot.gif.frames.size();
            changed = true;
        }
        if (changed) {
            d3dContext_->UpdateSubresource(
                slot.frameTex.Get(), 0, nullptr,
                slot.gif.frames[slot.gifFrameIdx].bgra.data(), slot.gif.width * 4, 0);
        }
    } else if (slot.kind == WallpaperKind::Shader) {
        if (!slot.frameRTV) return;
        double dtSec = static_cast<double>(nowMs - slot.shaderLastTickMs) / 1000.0;
        slot.shaderLastTickMs = nowMs;
        slot.shaderTimeSec += std::min(dtSec, 0.1) * std::clamp(slot.profile.playbackRate, 0.25f, 2.5f);

        ProcShaderCBData pcb{};
        pcb.timeSec = static_cast<float>(slot.shaderTimeSec);
        pcb.aspect = static_cast<float>(slot.texW) / static_cast<float>(std::max<uint32_t>(1, slot.texH));
        pcb.mouseU = mouseU;
        pcb.mouseV = mouseV;
        pcb.bass = audio.bass;
        pcb.mid = audio.mid;
        pcb.high = audio.high;
        d3dContext_->UpdateSubresource(procShaderCB_.Get(), 0, nullptr, &pcb, 0, 0);

        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(slot.texW);
        vp.Height = static_cast<float>(slot.texH);
        vp.MaxDepth = 1.0f;

        ID3D11RenderTargetView* rtv = slot.frameRTV.Get();
        d3dContext_->OMSetRenderTargets(1, &rtv, nullptr);
        d3dContext_->RSSetViewports(1, &vp);
        d3dContext_->IASetInputLayout(nullptr);
        d3dContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dContext_->VSSetShader(vs_.Get(), nullptr, 0);

        ID3D11PixelShader* ps = slot.customShaderPS
            ? slot.customShaderPS.Get()
            : builtinShaderPS_[std::clamp(slot.builtinShaderId, 0, 2)].Get();
        d3dContext_->PSSetShader(ps, nullptr, 0);
        ID3D11Buffer* cb = procShaderCB_.Get();
        d3dContext_->PSSetConstantBuffers(0, 1, &cb);
        d3dContext_->Draw(3, 0);
    }
}

void VideoWallpaperPlayer::RenderOverlaysD2D(
    const RECT& vp, const RECT& workVp,
    const AudioSpectrumSnapshot& audio,
    const SystemStatsSnapshot& sysStats,
    uint32_t accentRGB) {
    if (!d2dTarget_) return;

    const bool wantVis = (config_.audioVisMode != AudioVisMode::Off);
    const bool wantWidgets = config_.widgetClock || config_.widgetMedia || config_.widgetSysStats;
    if (!wantVis && !wantWidgets) return;

    const float vpX = static_cast<float>(vp.left);
    const float vpY = static_cast<float>(vp.top);
    const float vpW = static_cast<float>(vp.right - vp.left);
    const float vpH = static_cast<float>(vp.bottom - vp.top);

    const float wkL = static_cast<float>(workVp.left);
    const float wkT = static_cast<float>(workVp.top);
    const float wkR = static_cast<float>(workVp.right);
    const float wkB = static_cast<float>(workVp.bottom);
    const float wkW = std::max(200.0f, wkR - wkL);

    d2dTarget_->BeginDraw();

    ComPtr<ID2D1SolidColorBrush> brAccent, brAccentSoft, brCardBg, brCardBorder, brTextMain, brTextSub, brBarBg;
    d2dTarget_->CreateSolidColorBrush(HexToColorF(accentRGB, 0.90f), brAccent.GetAddressOf());
    d2dTarget_->CreateSolidColorBrush(HexToColorF(accentRGB, 0.30f), brAccentSoft.GetAddressOf());
    d2dTarget_->CreateSolidColorBrush(D2D1::ColorF(0.05f, 0.07f, 0.12f, config_.widgetOpacity), brCardBg.GetAddressOf());
    d2dTarget_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f), brCardBorder.GetAddressOf());
    d2dTarget_->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.98f, 1.0f, 0.96f), brTextMain.GetAddressOf());
    d2dTarget_->CreateSolidColorBrush(D2D1::ColorF(0.68f, 0.75f, 0.86f, 0.90f), brTextSub.GetAddressOf());
    d2dTarget_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), brBarBg.GetAddressOf());

    if (wantVis && brAccent && brAccentSoft) {
        if (config_.audioVisMode == AudioVisMode::BarsBottom) {
            const float totalW = std::min(wkW * 0.72f, 960.0f);
            const float startX = wkL + (wkW - totalW) * 0.5f;
            const float stepX = totalW / 32.0f;
            const float barW = std::max(4.0f, stepX * 0.64f);
            const float maxBarH = std::min(150.0f, vpH * 0.20f);
            const float baseY = wkB - 10.0f;

            for (size_t i = 0; i < 32; ++i) {
                float v = std::max(0.03f, audio.bands[i]);
                float bh = v * maxBarH;
                float bx = startX + static_cast<float>(i) * stepX;

                D2D1_ROUNDED_RECT glowRR{D2D1::RectF(bx - 2.0f, baseY - bh - 3.0f, bx + barW + 2.0f, baseY + 2.0f), 4.0f, 4.0f};
                d2dTarget_->FillRoundedRectangle(glowRR, brAccentSoft.Get());

                D2D1_ROUNDED_RECT barRR{D2D1::RectF(bx, baseY - bh, bx + barW, baseY), 3.0f, 3.0f};
                d2dTarget_->FillRoundedRectangle(barRR, brAccent.Get());
            }
        } else if (config_.audioVisMode == AudioVisMode::SymmetricWave) {
            const float totalW = wkW * 0.84f;
            const float startX = wkL + (wkW - totalW) * 0.5f;
            const float baseY = wkB - 55.0f;
            const float maxAmp = std::min(95.0f, vpH * 0.14f);

            D2D1_POINT_2F prevPt{startX, baseY};
            for (size_t i = 0; i < 32; ++i) {
                float x = startX + (static_cast<float>(i) / 31.0f) * totalW;
                float sign = (i % 2 == 0) ? -1.0f : 1.0f;
                float y = baseY + sign * audio.bands[i] * maxAmp;
                D2D1_POINT_2F curPt{x, y};
                if (i > 0) {
                    d2dTarget_->DrawLine(prevPt, curPt, brAccentSoft.Get(), 7.0f);
                    d2dTarget_->DrawLine(prevPt, curPt, brAccent.Get(), 2.6f);
                }
                prevPt = curPt;
            }
        } else if (config_.audioVisMode == AudioVisMode::NeonRing) {
            const float cx = vpX + vpW * 0.5f;
            const float cy = vpY + vpH * 0.48f;
            const float baseR = std::min(vpW, vpH) * 0.14f * (1.0f + audio.bass * 0.15f);

            D2D1_ELLIPSE innerEl{D2D1::Point2F(cx, cy), baseR, baseR};
            d2dTarget_->DrawEllipse(innerEl, brAccentSoft.Get(), 5.0f);
            d2dTarget_->DrawEllipse(innerEl, brAccent.Get(), 2.0f);

            for (size_t i = 0; i < 64; ++i) {
                size_t bi = (i < 32) ? i : (63 - i);
                float ang = static_cast<float>(i) * (6.2831853f / 64.0f) - 1.570796f;
                float len = 6.0f + audio.bands[bi] * (baseR * 0.72f);
                float c = std::cos(ang), s = std::sin(ang);
                D2D1_POINT_2F p0{cx + c * (baseR + 4.0f), cy + s * (baseR + 4.0f)};
                D2D1_POINT_2F p1{cx + c * (baseR + 4.0f + len), cy + s * (baseR + 4.0f + len)};
                d2dTarget_->DrawLine(p0, p1, brAccent.Get(), 3.2f);
            }
        }
    }

    if (wantWidgets && brCardBg && brTextMain && brTextSub && brAccent) {
        const float cardW = 316.0f;
        const float cardX = std::max(wkL + 24.0f, wkR - cardW - 34.0f);
        float curY = wkT + 34.0f;

        auto drawWidgetCard = [&](float h) -> D2D1_RECT_F {
            D2D1_RECT_F r = D2D1::RectF(cardX, curY, cardX + cardW, curY + h);
            D2D1_ROUNDED_RECT rr{r, 14.0f, 14.0f};
            d2dTarget_->FillRoundedRectangle(rr, brCardBg.Get());
            d2dTarget_->DrawRoundedRectangle(rr, brCardBorder.Get(), 1.0f);
            D2D1_ROUNDED_RECT topBar{D2D1::RectF(r.left + 16.0f, r.top, r.left + 84.0f, r.top + 3.0f), 1.5f, 1.5f};
            d2dTarget_->FillRoundedRectangle(topBar, brAccent.Get());
            curY += h + 12.0f;
            return r;
        };

        if (config_.widgetClock && fmtClockTime_ && fmtClockDate_) {
            SYSTEMTIME st{};
            GetLocalTime(&st);

            int hr = st.wHour;
            const wchar_t* ampm = L"";
            if (!config_.clock24h) {
                ampm = (hr >= 12) ? L" PM" : L" AM";
                hr = hr % 12;
                if (hr == 0) hr = 12;
            }

            wchar_t timeBuf[64];
            if (config_.clockShowSeconds) {
                swprintf_s(timeBuf, L"%02d:%02d:%02d%s", hr, st.wMinute, st.wSecond, ampm);
            } else {
                swprintf_s(timeBuf, L"%02d:%02d%s", hr, st.wMinute, ampm);
            }
            std::wstring dateStr = FormatRussianDate(st);

            D2D1_RECT_F r = drawWidgetCard(108.0f);
            D2D1_RECT_F tr = D2D1::RectF(r.left + 18.0f, r.top + 8.0f, r.right - 14.0f, r.top + 74.0f);
            d2dTarget_->DrawTextW(timeBuf, static_cast<UINT32>(wcslen(timeBuf)), fmtClockTime_.Get(), tr, brTextMain.Get());

            D2D1_RECT_F dr = D2D1::RectF(r.left + 20.0f, r.top + 74.0f, r.right - 14.0f, r.bottom - 8.0f);
            d2dTarget_->DrawTextW(dateStr.c_str(), static_cast<UINT32>(dateStr.size()), fmtClockDate_.Get(), dr, brTextSub.Get());
        }

        if (config_.widgetMedia && fmtWidgetBody_ && fmtWidgetSmall_) {
            D2D1_RECT_F r = drawWidgetCard(62.0f);
            std::wstring header = audio.nowPlaying.empty() ? L"♪ АУДИОСИСТЕМА АКТИВНА" : L"♪ СЕЙЧАС ИГРАЕТ";
            std::wstring track = audio.nowPlaying.empty() ? GetFileNameWithoutExt(currentPath_) : audio.nowPlaying;

            D2D1_RECT_F hrRc = D2D1::RectF(r.left + 18.0f, r.top + 10.0f, r.right - 16.0f, r.top + 28.0f);
            d2dTarget_->DrawTextW(header.c_str(), static_cast<UINT32>(header.size()), fmtWidgetSmall_.Get(), hrRc, brAccent.Get());

            D2D1_RECT_F trRc = D2D1::RectF(r.left + 18.0f, r.top + 30.0f, r.right - 16.0f, r.bottom - 8.0f);
            d2dTarget_->DrawTextW(track.c_str(), static_cast<UINT32>(track.size()), fmtWidgetBody_.Get(),
                                  trRc, brTextMain.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        if (config_.widgetSysStats && fmtWidgetSmall_ && brBarBg) {
            D2D1_RECT_F r = drawWidgetCard(72.0f);

            wchar_t cpuLbl[64], ramLbl[96];
            swprintf_s(cpuLbl, L"ЦП (CPU): %.0f%%", sysStats.cpuPercent);
            swprintf_s(ramLbl, L"ОЗУ (RAM): %u%%  (%.1f / %.1f ГБ)",
                       sysStats.ramPercent, sysStats.ramUsedGb, sysStats.ramTotalGb);

            D2D1_RECT_F cRc = D2D1::RectF(r.left + 18.0f, r.top + 10.0f, r.right - 18.0f, r.top + 26.0f);
            d2dTarget_->DrawTextW(cpuLbl, static_cast<UINT32>(wcslen(cpuLbl)), fmtWidgetSmall_.Get(), cRc, brTextMain.Get());

            float barL = r.left + 18.0f, barW = cardW - 36.0f;
            d2dTarget_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(barL, r.top + 28.0f, barL + barW, r.top + 33.0f), 2.5f, 2.5f}, brBarBg.Get());
            d2dTarget_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(barL, r.top + 28.0f, barL + barW * std::clamp(sysStats.cpuPercent / 100.0f, 0.02f, 1.0f), r.top + 33.0f), 2.5f, 2.5f}, brAccent.Get());

            D2D1_RECT_F mRc = D2D1::RectF(r.left + 18.0f, r.top + 38.0f, r.right - 18.0f, r.top + 54.0f);
            d2dTarget_->DrawTextW(ramLbl, static_cast<UINT32>(wcslen(ramLbl)), fmtWidgetSmall_.Get(), mRc, brTextSub.Get());

            d2dTarget_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(barL, r.top + 57.0f, barL + barW, r.top + 62.0f), 2.5f, 2.5f}, brBarBg.Get());
            d2dTarget_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(barL, r.top + 57.0f, barL + barW * std::clamp(sysStats.ramPercent / 100.0f, 0.02f, 1.0f), r.top + 62.0f), 2.5f, 2.5f}, brAccent.Get());
        }
    }

    if (d2dTarget_->EndDraw() == D2DERR_RECREATE_TARGET) {
        RebuildD2DTarget();
    }
}

void VideoWallpaperPlayer::Tick(
    const AudioSpectrumSnapshot& audio,
    const SystemStatsSnapshot& sysStats,
    uint32_t accentRGB) {
    if (!loaded_ || IsEffectivelyPaused() || !swapChain_ || !backBufferRTV_) return;

    POINT curPt{};
    GetCursorPos(&curPt);
    RECT vBounds = desktopHost_.GetVirtualScreenRect();
    float mouseU = static_cast<float>(curPt.x - vBounds.left) / static_cast<float>(std::max<LONG>(1, vBounds.right - vBounds.left));
    float mouseV = static_cast<float>(curPt.y - vBounds.top)  / static_cast<float>(std::max<LONG>(1, vBounds.bottom - vBounds.top));
    mouseU = std::clamp(mouseU, 0.0f, 1.0f);
    mouseV = std::clamp(mouseV, 0.0f, 1.0f);

    bool hasRenderable = false;
    for (auto& slot : slots_) {
        if (slot.kind == WallpaperKind::Web) continue;
        UpdateSlotFrame(slot, audio, mouseU, mouseV);
        if (slot.frameSRV) hasRenderable = true;
    }
    if (!hasRenderable) return;

    const float clearBlack[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D11RenderTargetView* rtv = backBufferRTV_.Get();
    d3dContext_->OMSetRenderTargets(1, &rtv, nullptr);
    d3dContext_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    d3dContext_->ClearRenderTargetView(rtv, clearBlack);

    d3dContext_->IASetInputLayout(nullptr);
    d3dContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3dContext_->VSSetShader(vs_.Get(), nullptr, 0);
    d3dContext_->PSSetShader(postProcessPS_.Get(), nullptr, 0);
    ID3D11SamplerState* samp = samplerLinear_.Get();
    d3dContext_->PSSetSamplers(0, 1, &samp);

    const float timeSec = static_cast<float>(GetTickCount64() % 1000000ULL) * 0.001f;

    auto drawSlotIntoRect = [&](const MediaSlot& slot, const RECT& r) {
        if (!slot.frameSRV) return;
        float vpW = static_cast<float>(std::max<LONG>(1, r.right - r.left));
        float vpH = static_cast<float>(std::max<LONG>(1, r.bottom - r.top));
        float vidAspect = static_cast<float>(std::max<uint32_t>(1, slot.texW)) / static_cast<float>(std::max<uint32_t>(1, slot.texH));
        float dstAspect = vpW / vpH;

        PostProcessCBData cb{};
        cb.uvScaleX = 1.0f; cb.uvScaleY = 1.0f;
        cb.uvOffsetX = 0.0f; cb.uvOffsetY = 0.0f;

        if (slot.profile.scaleMode == ScaleMode::Cover) {
            if (dstAspect > vidAspect) {
                cb.uvScaleY = vidAspect / dstAspect;
                cb.uvOffsetY = (1.0f - cb.uvScaleY) * 0.5f;
            } else {
                cb.uvScaleX = dstAspect / vidAspect;
                cb.uvOffsetX = (1.0f - cb.uvScaleX) * 0.5f;
            }
        } else if (slot.profile.scaleMode == ScaleMode::FitBars || slot.profile.scaleMode == ScaleMode::FitBlur) {
            if (dstAspect > vidAspect) {
                cb.uvScaleX = dstAspect / vidAspect;
                cb.uvOffsetX = (1.0f - cb.uvScaleX) * 0.5f;
            } else {
                cb.uvScaleY = vidAspect / dstAspect;
                cb.uvOffsetY = (1.0f - cb.uvScaleY) * 0.5f;
            }
        } else if (slot.profile.scaleMode == ScaleMode::Center) {
            cb.uvScaleX = vpW / static_cast<float>(std::max<uint32_t>(1, slot.texW));
            cb.uvScaleY = vpH / static_cast<float>(std::max<uint32_t>(1, slot.texH));
            cb.uvOffsetX = (1.0f - cb.uvScaleX) * 0.5f;
            cb.uvOffsetY = (1.0f - cb.uvScaleY) * 0.5f;
        }

        cb.brightness = slot.profile.brightness;
        cb.contrast = slot.profile.contrast;
        cb.saturation = slot.profile.saturation;
        cb.hueShiftRad = slot.profile.hueShift * (3.14159265f / 180.0f);

        cb.sharpen = slot.profile.sharpen;
        cb.bloom = slot.profile.bloom;
        cb.chromatic = slot.profile.chromatic;
        cb.crtEffect = slot.profile.crtEffect;

        cb.vignette = slot.profile.vignette;
        cb.bassPulse = config_.bassReactiveZoom ? (audio.bass * config_.bassPulseAmount * 2.0f) : 0.0f;
        cb.timeSec = timeSec;
        cb.invTexW = 1.0f / static_cast<float>(std::max<uint32_t>(1, slot.texW));
        cb.invTexH = 1.0f / static_cast<float>(std::max<uint32_t>(1, slot.texH));

        cb.loopBlend = 0.0f;
        if (slot.kind == WallpaperKind::Video && slot.profile.seamlessLoop && slot.loopHeadCaptured && slot.mediaEngine) {
            double dur = slot.mediaEngine->GetDuration();
            double cur = slot.mediaEngine->GetCurrentTime();
            if (!std::isnan(dur) && dur > 1.8 && !std::isnan(cur)) {
                double effStart = dur * std::clamp(slot.profile.trimStartPct, 0.0f, 0.90f);
                double effEnd = dur * std::clamp(slot.profile.trimEndPct, 0.10f, 1.00f);
                if (effEnd <= effStart + 1.0) effEnd = dur;
                const double fadeWin = 0.65;
                if (effEnd - effStart > 1.6 && cur > effEnd - fadeWin && cur <= effEnd) {
                    float t = static_cast<float>((cur - (effEnd - fadeWin)) / fadeWin);
                    t = std::clamp(t, 0.0f, 1.0f);
                    cb.loopBlend = t * t * (3.0f - 2.0f * t);
                }
            }
        }

        d3dContext_->UpdateSubresource(postCB_.Get(), 0, nullptr, &cb, 0, 0);

        D3D11_VIEWPORT vp{};
        vp.TopLeftX = static_cast<float>(r.left);
        vp.TopLeftY = static_cast<float>(r.top);
        vp.Width = vpW;
        vp.Height = vpH;
        vp.MaxDepth = 1.0f;
        d3dContext_->RSSetViewports(1, &vp);

        ID3D11Buffer* cbs[1] = {postCB_.Get()};
        d3dContext_->PSSetConstantBuffers(0, 1, cbs);
        ID3D11ShaderResourceView* srvs[2] = {
            slot.frameSRV.Get(),
            slot.loopHeadSRV ? slot.loopHeadSRV.Get() : slot.frameSRV.Get()};
        d3dContext_->PSSetShaderResources(0, 2, srvs);
        d3dContext_->Draw(3, 0);

        ID3D11ShaderResourceView* nullSrvs[2] = {nullptr, nullptr};
        d3dContext_->PSSetShaderResources(0, 2, nullSrvs);
    };

    if (config_.monitorMode == MonitorMode::AllSame && !cloneViewports_.empty() && !slots_.empty()) {
        for (const RECT& r : cloneViewports_) {
            drawSlotIntoRect(slots_[0], r);
        }
    } else {
        for (const auto& slot : slots_) {
            if (slot.kind != WallpaperKind::Web) {
                drawSlotIntoRect(slot, slot.localViewport);
            }
        }
    }

    if (crossfadeDurSec_ > 0.01f && crossfadePrevSRV_ && crossfadePS_ && alphaBlendState_) {
        float elapsed = static_cast<float>(GetTickCount64() - crossfadeStartMs_) * 0.001f;
        if (elapsed < crossfadeDurSec_) {
            float a = 1.0f - (elapsed / crossfadeDurSec_);
            a = a * a * (3.0f - 2.0f * a);

            PostProcessCBData xcb{};
            xcb.uvScaleX = a;
            d3dContext_->UpdateSubresource(postCB_.Get(), 0, nullptr, &xcb, 0, 0);

            D3D11_VIEWPORT fullVp{0.0f, 0.0f, static_cast<float>(targetW_), static_cast<float>(targetH_), 0.0f, 1.0f};
            d3dContext_->RSSetViewports(1, &fullVp);
            float bf[4] = {0, 0, 0, 0};
            d3dContext_->OMSetBlendState(alphaBlendState_.Get(), bf, 0xFFFFFFFF);
            d3dContext_->PSSetShader(crossfadePS_.Get(), nullptr, 0);
            ID3D11Buffer* cbs[1] = {postCB_.Get()};
            d3dContext_->PSSetConstantBuffers(0, 1, cbs);
            ID3D11ShaderResourceView* srv = crossfadePrevSRV_.Get();
            d3dContext_->PSSetShaderResources(0, 1, &srv);
            d3dContext_->Draw(3, 0);

            ID3D11ShaderResourceView* nullSrv = nullptr;
            d3dContext_->PSSetShaderResources(0, 1, &nullSrv);
            d3dContext_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        } else {
            crossfadeDurSec_ = 0.0f;
            crossfadePrevSRV_.Reset();
            crossfadePrevTex_.Reset();
        }
    }

    if (!slots_.empty()) {
        if (config_.monitorMode == MonitorMode::AllSame && !cloneViewports_.empty()) {
            for (size_t i = 0; i < cloneViewports_.size(); ++i) {
                RenderOverlaysD2D(cloneViewports_[i], cloneWorkRects_[i], audio, sysStats, accentRGB);
            }
        } else {
            RenderOverlaysD2D(slots_[0].localViewport, slots_[0].localWorkRect, audio, sysStats, accentRGB);
        }
    }

    swapChain_->Present(1, 0);
    ++renderedFrames_;
}

bool VideoWallpaperPlayer::SaveBackbufferBmp(const std::wstring& bmpPath) {
    if (!swapChain_ || !d3dDevice_ || !d3dContext_) return false;

    ComPtr<ID3D11Texture2D> backBuf;
    if (FAILED(swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuf.GetAddressOf()))) || !backBuf) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    backBuf->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(d3dDevice_->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())) || !staging) {
        return false;
    }

    d3dContext_->CopyResource(staging.Get(), backBuf.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(d3dContext_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(desc.Width) * desc.Height * 4);
    for (uint32_t y = 0; y < desc.Height; ++y) {
        std::memcpy(
            pixels.data() + static_cast<size_t>(y) * desc.Width * 4,
            static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch,
            static_cast<size_t>(desc.Width) * 4);
    }
    d3dContext_->Unmap(staging.Get(), 0);
    return SaveBmpBGRA(bmpPath, desc.Width, desc.Height, pixels.data());
}

void VideoWallpaperPlayer::StopAndDetach() {
    for (auto& s : slots_) ShutdownSlot(s);
    slots_.clear();
    loaded_ = false;
    hasError_ = false;
    currentPath_.clear();

    if (renderHwnd_) {
        desktopHost_.DetachRenderWindow(renderHwnd_);
    }
}

void VideoWallpaperPlayer::Shutdown() {
    StopAndDetach();

    fmtWidgetSmall_.Reset();
    fmtWidgetBody_.Reset();
    fmtClockDate_.Reset();
    fmtClockTime_.Reset();
    d2dTarget_.Reset();
    dwriteFactory_.Reset();
    d2dFactory_.Reset();

    crossfadePrevSRV_.Reset();
    crossfadePrevTex_.Reset();
    alphaBlendState_.Reset();
    procShaderCB_.Reset();
    postCB_.Reset();
    samplerLinear_.Reset();
    for (auto& ps : builtinShaderPS_) ps.Reset();
    crossfadePS_.Reset();
    postProcessPS_.Reset();
    vs_.Reset();
    backBufferRTV_.Reset();
    swapChain_.Reset();
    dxgiManager_.Reset();
    d3dContext_.Reset();
    d3dDevice_.Reset();

    if (renderHwnd_) {
        DestroyWindow(renderHwnd_);
        renderHwnd_ = nullptr;
    }
}

}  // namespace wp
