#pragma once

#include "common.h"

namespace wp {

class WebWallpaperHost {
public:
    WebWallpaperHost();
    ~WebWallpaperHost();

    bool AttachAndNavigate(HINSTANCE hInstance, HWND parentHwnd, const RECT& bounds, const std::wstring& htmlPath);
    void SetBounds(const RECT& bounds);
    void Close();

    bool IsActive() const { return hostHwnd_ != nullptr; }
    HWND WindowHandle() const { return hostHwnd_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    HWND hostHwnd_ = nullptr;
};

// Registers FEATURE_BROWSER_EMULATION=11001 & FEATURE_GPU_RENDERING=1 for WallPulse.exe in HKCU
void EnsureHtml5BrowserFeatureFlags();

}  // namespace wp
