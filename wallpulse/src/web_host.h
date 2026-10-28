#pragma once

#include "audio_reactive.h"
#include "common.h"

#include <memory>

namespace wp {

class WebWallpaperHost {
public:
    WebWallpaperHost();
    ~WebWallpaperHost();

    bool AttachAndNavigate(HINSTANCE hInstance, HWND parentHwnd, const RECT& bounds, const std::wstring& htmlPath);
    void SetBounds(const RECT& bounds);

    // Forwards desktop cursor coordinates + 32-band WASAPI spectrum into WebView2 / MSHTML safely from any thread
    void PushDesktopInteraction(float mouseU, float mouseV, const AudioSpectrumSnapshot& audio);

    void Close();

    bool IsActive() const { return hostHwnd_ != nullptr; }
    bool IsUsingWebView2() const;
    HWND WindowHandle() const { return hostHwnd_; }

    // Public forward declaration allows async WebView2 COM completion handlers to retain the host state.
    struct Impl;

private:
    std::shared_ptr<Impl> impl_;
    HWND hostHwnd_ = nullptr;
};

// Registers FEATURE_BROWSER_EMULATION=11001 & FEATURE_GPU_RENDERING=1 for WallPulse.exe in HKCU
void EnsureHtml5BrowserFeatureFlags();

}  // namespace wp
