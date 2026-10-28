#pragma once

#include "common.h"
#include "config.h"
#include "desktop_host.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>

#include <atomic>

namespace wp {

class VideoWallpaperEngine {
public:
    VideoWallpaperEngine();
    ~VideoWallpaperEngine();

    bool Initialize(HINSTANCE hInstance, DesktopHost* desktopHost);
    void Shutdown();

    // Load and apply an .mp4 (or other video) to the desktop background
    bool LoadAndApply(const std::wstring& videoPath, const AppConfig& cfg);
    void StopAndDetach();

    // Apply live settings updates (volume, mute, rate, color grading, scaling, monitor layout)
    void ApplySettings(const AppConfig& cfg, bool layoutChanged = false);
    void SetSmartPause(bool pauseVideo, bool muteAudio);

    // Called at ~60Hz while active to advance video frames, cursor parallax & D3D11 Present
    bool TickFrame(bool capturePreviewForStudio);

    // Periodic check for Explorer restart / WorkerW recreation
    void CheckDesktopAttachment(const AppConfig& cfg);

    // Accessors
    HWND RenderWindow() const { return renderHwnd_; }
    bool IsLoaded() const { return hasMedia_ && !currentPath_.empty(); }
    bool IsEffectivelyPaused() const { return userPaused_ || smartPaused_; }
    bool IsSmartPaused() const { return smartPaused_; }
    const std::wstring& CurrentVideoPath() const { return currentPath_; }
    const std::wstring& GpuAdapterName() const { return adapterName_; }
    const std::wstring& LastErrorText() const { return lastError_; }
    uint32_t VideoWidth() const { return videoW_; }
    uint32_t VideoHeight() const { return videoH_; }
    double DurationSec() const;
    double CurrentTimeSec() const;

    // Copies the latest 320x180 BGRA shader-graded preview frame if available
    bool CopyLatestPreviewFrame(uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outBGRA, uint64_t& ioSeq);

    // Internal callback invoked by IMFMediaEngineNotify
    void OnMediaEngineEvent(DWORD event, DWORD_PTR param1, DWORD param2);

private:
    bool CreateRenderWindow(HINSTANCE hInstance);
    bool InitD3D11();
    bool CreateOrResizeSwapChain(uint32_t width, uint32_t height);
    bool EnsureVideoSurface(uint32_t width, uint32_t height);
    bool EnsurePreviewSurfaces();
    bool InitShaders();
    bool InitMediaEngine();
    void UpdateAudioAndRate();
    void RenderSceneToTarget(ID3D11RenderTargetView* rtv, uint32_t targetW, uint32_t targetH, bool multiMonitorViewports);

    static LRESULT CALLBACK RenderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    DesktopHost* desktopHost_ = nullptr;
    HWND renderHwnd_ = nullptr;
    bool attachedToDesktop_ = false;

    ComPtr<ID3D11Device> d3dDevice_;
    ComPtr<ID3D11DeviceContext> d3dCtx_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> backbufferRtv_;
    uint32_t swapW_ = 0;
    uint32_t swapH_ = 0;

    ComPtr<ID3D11Texture2D> videoTex_;
    ComPtr<ID3D11ShaderResourceView> videoSrv_;
    uint32_t videoW_ = 0;
    uint32_t videoH_ = 0;

    ComPtr<ID3D11Texture2D> previewTex_;
    ComPtr<ID3D11RenderTargetView> previewRtv_;
    ComPtr<ID3D11Texture2D> previewStaging_;

    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11Buffer> constantBuf_;

    ComPtr<IMFDXGIDeviceManager> dxgiManager_;
    UINT dxgiResetToken_ = 0;
    ComPtr<IMFMediaEngine> mediaEngine_;
    ComPtr<IMFMediaEngineNotify> mediaNotify_;

    AppConfig activeCfg_{};
    std::wstring currentPath_;
    std::wstring adapterName_ = L"Direct3D 11";
    std::wstring lastError_;

    std::atomic<bool> canPlay_{false};
    std::atomic<bool> endedSignal_{false};
    std::atomic<bool> errorSignal_{false};
    bool hasMedia_ = false;
    bool userPaused_ = false;
    bool smartPaused_ = false;
    bool smartMuted_ = false;
    bool dirtyVisuals_ = true;

    float parallaxX_ = 0.0f;
    float parallaxY_ = 0.0f;
    ULONGLONG lastPreviewTickMs_ = 0;

    std::mutex previewMutex_;
    std::vector<uint8_t> previewCpuBuf_;
    uint64_t previewSeq_ = 0;
};

}  // namespace wp
