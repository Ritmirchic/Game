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
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfmediaengine.h>

#include <memory>

namespace wp {

class MediaEngineNotify;

class VideoWallpaperPlayer {
public:
    VideoWallpaperPlayer();
    ~VideoWallpaperPlayer();

    bool Initialize(HINSTANCE hInstance);
    void Shutdown();

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
    bool IsUserPaused() const { return userPaused_; }

    void SetSmartPaused(bool paused);
    bool IsSmartPaused() const { return smartPaused_; }
    bool IsEffectivelyPaused() const { return userPaused_ || smartPaused_; }

    void SetSmartMuted(bool muted);

    void ReattachDesktop();
    void StopAndDetach();

    bool IsLoaded() const { return loaded_; }
    bool IsReadyToPlay() const;
    bool HasPlaybackError() const { return hasError_; }
    uint64_t RenderedFrameCount() const { return renderedFrames_; }
    double DurationSeconds() const;
    double CurrentTimeSeconds() const;
    void SeekSeconds(double sec);

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

        AnimatedGifData gif;
        size_t gifFrameIdx = 0;
        double gifAccumMs = 0.0;
        ULONGLONG gifLastTickMs = 0;

        int builtinShaderId = 0;
        ComPtr<ID3D11PixelShader> customShaderPS;
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

    bool InitD3DResources();
    bool CreateOrResizeSwapChain(uint32_t width, uint32_t height);
    bool RebuildD2DTarget();
    bool EnsureSlotTextures(MediaSlot& slot, uint32_t width, uint32_t height, bool needRtv);
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
    ComPtr<ID3D11RenderTargetView> backBufferRTV_;

    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> postProcessPS_;
    ComPtr<ID3D11PixelShader> crossfadePS_;
    ComPtr<ID3D11PixelShader> builtinShaderPS_[3];
    ComPtr<ID3D11SamplerState> samplerLinear_;
    ComPtr<ID3D11Buffer> postCB_;
    ComPtr<ID3D11Buffer> procShaderCB_;
    ComPtr<ID3D11BlendState> alphaBlendState_;

    ComPtr<ID3D11Texture2D> crossfadePrevTex_;
    ComPtr<ID3D11ShaderResourceView> crossfadePrevSRV_;
    ULONGLONG crossfadeStartMs_ = 0;
    float crossfadeDurSec_ = 0.0f;

    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<ID2D1RenderTarget> d2dTarget_;
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

    bool loaded_ = false;
    bool hasError_ = false;
    bool userPaused_ = false;
    bool smartPaused_ = false;
    bool smartMuted_ = false;
    bool attachedToDesktop_ = false;
    uint64_t renderedFrames_ = 0;
};

}  // namespace wp
