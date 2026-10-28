#pragma once

#include "common.h"
#include "config.h"
#include "desktop_host.h"
#include "video_wallpaper.h"

#include <d2d1.h>
#include <dwrite.h>
#include <shellapi.h>

#include <functional>
#include <unordered_map>

namespace wp {

class StudioWindow {
public:
    StudioWindow();
    ~StudioWindow();

    bool Create(
        HINSTANCE hInstance,
        AppConfig* cfg,
        DesktopHost* desktopHost,
        VideoWallpaperEngine* engine,
        bool startMinimizedToTray);

    void Destroy();

    HWND Handle() const { return hwnd_; }
    bool IsVisibleAndNotMinimized() const;

    void ShowAndActivate();
    void HideToTray();

    // Add a video file (.mp4 etc.) to the library; optionally apply immediately to desktop
    bool AddVideoFile(const std::wstring& filePath, bool applyNow);
    int ImportFolderVideos(const std::wstring& folderPath, bool applyFirst);
    void ApplyWallpaperByPath(const std::wstring& filePath);
    void StopActiveWallpaper();

    // Called from main timer loop (~30Hz when visible) to refresh live preview bitmap
    void OnUiTick(const std::wstring& smartPauseReason);

    // Save screenshot of current Direct2D client area (used by Windows CI verification)
    bool CaptureWindowClientToBmp(const std::wstring& outBmpPath);

private:
    enum class Tab : int {
        Library = 0,
        Monitors = 1,
        Performance = 2,
        Diagnostics = 3
    };

    enum class SliderId : int {
        None = -1,
        Volume = 0,
        Speed = 1,
        Brightness = 2,
        Contrast = 3,
        Saturation = 4,
        Vignette = 5
    };

    struct HitZone {
        int id = 0;
        D2D1_RECT_F rect{};
        int extra = 0;
    };

    bool InitD2D();
    bool EnsureRenderTarget();
    void DiscardDeviceResources();
    ID2D1Bitmap* GetOrLoadThumbBitmap(const WallpaperItem& item);

    void Render();
    void DrawHeader(float width);
    void DrawLibraryTab(const D2D1_RECT_F& area);
    void DrawMonitorsTab(const D2D1_RECT_F& area);
    void DrawPerformanceTab(const D2D1_RECT_F& area);
    void DrawDiagnosticsTab(const D2D1_RECT_F& area);
    void DrawInspectorSidebar(const D2D1_RECT_F& area);

    // UI widget helpers
    void DrawButton(int hitId, const D2D1_RECT_F& rc, const std::wstring& label, bool primary, bool active = false, bool danger = false);
    void DrawToggleRow(int hitId, const D2D1_RECT_F& rc, const std::wstring& title, const std::wstring& subtitle, bool checked);
    void DrawSlider(SliderId sid, const D2D1_RECT_F& rc, const std::wstring& label, const std::wstring& valueText, float norm01);

    void OnMouseDown(float x, float y, bool rightButton);
    void OnMouseMove(float x, float y);
    void OnMouseUp();
    void OnMouseWheel(short delta);
    void UpdateSliderFromMouse(SliderId sid, float mouseX, const D2D1_RECT_F& trackRc);

    void OpenFilePickerDialog();
    void OpenFolderPickerDialog();
    void HandleDroppedFiles(HDROP hDrop);

    void SetupTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayContextMenu();
    void ShowFirstMinimizeBalloon();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    AppConfig* cfg_ = nullptr;
    DesktopHost* desktopHost_ = nullptr;
    VideoWallpaperEngine* engine_ = nullptr;

    float dpiScale_ = 1.0f;
    Tab currentTab_ = Tab::Library;
    float libraryScrollY_ = 0.0f;
    float maxLibraryScrollY_ = 0.0f;

    int hoveredHitId_ = 0;
    SliderId draggingSlider_ = SliderId::None;
    D2D1_RECT_F sliderTracks_[6]{};
    std::vector<HitZone> hitZones_;

    std::wstring smartPauseText_;
    std::wstring statusBanner_;
    ULONGLONG statusBannerUntilMs_ = 0;

    bool trayAdded_ = false;
    UINT taskbarCreatedMsg_ = 0;

    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<ID2D1HwndRenderTarget> rt_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1Bitmap> livePreviewBmp_;
    uint64_t lastPreviewSeq_ = 0;
    std::unordered_map<std::wstring, ComPtr<ID2D1Bitmap>> thumbCache_;

    ComPtr<IDWriteTextFormat> fmtBrand_;
    ComPtr<IDWriteTextFormat> fmtTitle_;
    ComPtr<IDWriteTextFormat> fmtSection_;
    ComPtr<IDWriteTextFormat> fmtBody_;
    ComPtr<IDWriteTextFormat> fmtSmall_;
    ComPtr<IDWriteTextFormat> fmtBadge_;
    ComPtr<IDWriteTextFormat> fmtMono_;
};

}  // namespace wp
