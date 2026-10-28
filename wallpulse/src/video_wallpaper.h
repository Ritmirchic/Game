#pragma once

#include "audio_reactive.h"
#include "common.h"
#include "config.h"
#include "desktop_host.h"
#include "media_tools.h"
#include "system_tweaks.h"
#include "web_host.h"

#include <d2d1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_3.h>
#include <mfapi.h>
#include <mfmediaengine.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace wp {

class MediaEngineNotify;

class VideoWallpaperPlayer {
public:
    VideoWallpaperPlayer();
    ~VideoWallpaperPlayer();

    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

    // Dedicated high-precision Waitable SwapChain / QPC render loop (30..165+ FPS, immune to UI drag stalls!)
    void StartRenderLoop(uint32_t targetFps);
    void SetTargetFps(uint32_t targetFps);
    void StopRenderLoop();
    void UpdateTelemetry(const AudioSpectrumSnapshot& audio, const SystemStatsSnapshot& sysStats, uint32_t accentRGB);

    bool LoadAndApply(
        const std::wstring& primaryPath,
        const WallpaperProfile& primaryProfile,
        const AppConfig& config,
        const std::vector<WallpaperItem>& library,
        bool attachToDesktop = true);

    void UpdateLiveSettings(
        const WallpaperProfile& activeProfile,
        const AppConfig& config,
        const std::vector<WallpaperItem>& library);

    void CaptureCrossfadeSnapshot(float durationSec);

    void Tick(const AudioSpectrumSnapshot& audio, const SystemStatsSnapshot& sysStats, uint32_t accentRGB);

    void SetUserPaused(bool paused);
    bool IsUserPaused() const { return userPaused_.load(); }

    void SetSmartPaused(bool paused);
    bool IsSmartPaused() const { return smartPaused_.load(); }
    bool IsEffectivelyPaused() const { return userPaused_.load() || smartPaused_.load(); }

    void SetSmartMuted(bool muted);

    void ReattachDesktop();
    void StopAndDetach();

    bool IsLoaded() const { return loaded_.load(); }
    bool IsReadyToPlay() const;
    bool HasPlaybackError() const { return hasError_.load(); }
    bool IsWaitableSwapChainActive() const { return frameLatencyWaitable_ != nullptr; }
    uint64_t RenderedFrameCount() const { return renderedFrames_.load(); }
    double DurationSeconds() const;
    double CurrentTimeSeconds() const;
    void SeekSeconds(double sec);

    std::wstring LastShaderError() const;
    const std::wstring& CurrentVideoPath() const { return currentPath_; }
    HWND RenderWindow() const { return renderHwnd_; }
    const DesktopHost& Host() const { return desktopHost_; }
    const DesktopTopology& AttachTopology() const { return desktopHost_.CurrentTopology(); }

    bool SaveBackbufferBmp(const std::wstring& bmpPath);

    void OnMediaEngineEvent(size_t slotIdx, DWORD meEvent, DWORD_PTR param1, DWORD param2);

private:
    struct MediaSlot {
        std::wstring path;
        WallpaperKind kind = WallpaperKind::Video;
        WallpaperProfile profile{};
        RECT localViewport{0, 0, 1280, 720};
        RECT localWorkRect{0, 0, 1280, 720};

        ComPtr<IMFMediaEngine> mediaEngine;
        ComPtr<MediaEngineNotify> notify;
        bool engineReady = false;
        bool loopHeadCaptured = false;
        ULONGLONG lastLoopSeekMs = 0;

        StreamingGifDecoder gifStream;
        double gifAccumMs = 0.0;
        ULONGLONG gifLastTickMs = 0;

        int builtinShaderId = 0;
        ComPtr<ID3D11PixelShader> customShaderPS;
        FILETIME customShaderWriteTime{};
        ULONGLONG lastShaderCheckMs = 0;
        double shaderTimeSec = 0.0;
        ULONGLONG shaderLastTickMs = 0;

        std::unique_ptr<WebWallpaperHost> webHost;

        uint32_t texW = 0;
        uint32_t texH = 0;
        ComPtr<ID3D11Texture2D> frameTex;
        ComPtr<ID3D11ShaderResourceView> frameSRV;
        ComPtr<ID3D11RenderTargetView> frameRTV;
        ComPtr<ID3D11Texture2D> loopHeadTex;
        ComPtr<ID3D11ShaderResourceView> loopHeadSRV;
    };

    void RenderWorkerMain();
    bool InitD3DResources();
    bool CreateOrResizeSwapChain(uint32_t width, uint32_t height);
    bool EnsureKawaseTargets(uint32_t width, uint32_t height);
    void RunDualKawaseBlur(ID3D11ShaderResourceView* srcSRV, uint32_t srcW, uint32_t srcH);
    bool RebuildD2DTarget();
    bool EnsureSlotTextures(MediaSlot& slot, uint32_t width, uint32_t height, bool needRtv);
    bool CompileCustomHlslSlot(MediaSlot& slot, const std::wstring& path);
    bool SetupSlot(size_t slotIdx, const std::wstring& path, const WallpaperProfile& prof,
                   const RECT& localVp, const RECT& localWork);
    void ShutdownSlot(MediaSlot& slot);
    void ApplyAudioAndSpeedToSlot(MediaSlot& slot);
    void UpdateSlotFrame(MediaSlot& slot, const AudioSpectrumSnapshot& audio, float mouseU, float mouseV);
    void RenderOverlaysD2D(
        const RECT& vp, const RECT& workVp,
        const AudioSpectrumSnapshot& audio,
        const SystemStatsSnapshot& sysStats,
        uint32_t accentRGB);

    static LRESULT CALLBACK RenderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    mutable std::recursive_mutex apiMutex_;

    HINSTANCE hInstance_ = nullptr;
    HWND renderHwnd_ = nullptr;
    uint32_t targetW_ = 1280;
    uint32_t targetH_ = 720;

    DesktopHost desktopHost_;

    UINT resetToken_ = 0;
    ComPtr<IMFDXGIDeviceManager> dxgiManager_;
    ComPtr<ID3D11Device> d3dDevice_;
    ComPtr<ID3D11DeviceContext> d3dContext_;
    ComPtr<IDXGISwapChain1> swapChain_;
    HANDLE frameLatencyWaitable_ = nullptr;
    ComPtr<ID3D11RenderTargetView> backBufferRTV_;

    // Dual-Kawase Multi-Pass Blur & Bloom targets
    uint32_t kawaseW_ = 0;
    uint32_t kawaseH_ = 0;
    ComPtr<ID3D11Texture2D> kawaseTexA_;
    ComPtr<ID3D11ShaderResourceView> kawaseSRVA_;
    ComPtr<ID3D11RenderTargetView> kawaseRTVA_;
    ComPtr<ID3D11Texture2D> kawaseTexB_;
    ComPtr<ID3D11ShaderResourceView> kawaseSRVB_;
    ComPtr<ID3D11RenderTargetView> kawaseRTVB_;

    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> postProcessPS_;
    ComPtr<ID3D11PixelShader> kawaseDownPS_;
    ComPtr<ID3D11PixelShader> kawaseUpPS_;
    ComPtr<ID3D11PixelShader> crossfadePS_;
    ComPtr<ID3D11PixelShader> builtinShaderPS_[3];
    ComPtr<ID3D11SamplerState> samplerLinear_;
    ComPtr<ID3D11Buffer> postCB_;
    ComPtr<ID3D11Buffer> procShaderCB_;
    ComPtr<ID3D11Buffer> kawaseCB_;
    ComPtr<ID3D11BlendState> alphaBlendState_;

    ComPtr<ID3D11Texture2D> crossfadePrevTex_;
    ComPtr<ID3D11ShaderResourceView> crossfadePrevSRV_;
    ULONGLONG crossfadeStartMs_ = 0;
    float crossfadeDurSec_ = 0.0f;

    // Direct2D + Cached Brushes (Zero per-frame COM allocations!)
    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<ID2D1RenderTarget> d2dTarget_;
    ComPtr<ID2D1SolidColorBrush> brAccent_;
    ComPtr<ID2D1SolidColorBrush> brAccentSoft_;
    ComPtr<ID2D1SolidColorBrush> brAccentFill_;
    ComPtr<ID2D1SolidColorBrush> brCardBg_;
    ComPtr<ID2D1SolidColorBrush> brCardBorder_;
    ComPtr<ID2D1SolidColorBrush> brTextMain_;
    ComPtr<ID2D1SolidColorBrush> brTextSub_;
    ComPtr<ID2D1SolidColorBrush> brBarBg_;

    ComPtr<IDWriteTextFormat> fmtClockTime_;
    ComPtr<IDWriteTextFormat> fmtClockDate_;
    ComPtr<IDWriteTextFormat> fmtWidgetBody_;
    ComPtr<IDWriteTextFormat> fmtWidgetSmall_;

    std::vector<MediaSlot> slots_;
    std::vector<RECT> cloneViewports_;
    std::vector<RECT> cloneWorkRects_;

    AppConfig config_{};
    WallpaperProfile primaryProfile_{};
    std::wstring currentPath_;
    std::wstring lastShaderError_;

    // Smoothed 2.5D Mouse Parallax inertia state
    float smoothMouseU_ = 0.5f;
    float smoothMouseV_ = 0.5f;
    ULONGLONG lastTickMs_ = 0;

    // Dedicated high-refresh render thread & telemetry state
    std::atomic<bool> renderThreadRunning_{false};
    std::atomic<uint32_t> targetFps_{60};
    std::thread renderThread_;
    mutable std::mutex telemetryMutex_;
    AudioSpectrumSnapshot latestAudio_{};
    SystemStatsSnapshot latestSysStats_{};
    uint32_t latestAccentRGB_ = 0x0EA5E9;

    std::atomic<bool> loaded_{false};
    std::atomic<bool> hasError_{false};
    std::atomic<bool> userPaused_{false};
    std::atomic<bool> smartPaused_{false};
    bool smartMuted_ = false;
    bool attachedToDesktop_ = false;
    std::atomic<uint64_t> renderedFrames_{0};
};

}  // namespace wp
