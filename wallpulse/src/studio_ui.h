#pragma once

#include "audio_reactive.h"
#include "common.h"
#include "config.h"
#include "system_tweaks.h"

#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include <unordered_map>

namespace wp {

struct StudioStatus {
    bool isPlaying = false;
    bool isUserPaused = false;
    bool isSmartPaused = false;
    std::wstring smartReason;
    std::wstring attachSummary;
    std::wstring shaderError;
    uint32_t monitorCount = 1;
    double currentTimeSec = 0.0;
    double durationSec = 0.0;
    uint32_t activeAccentRGB = 0x0EA5E9;
    AudioSpectrumSnapshot audio{};
    SystemStatsSnapshot sysStats{};
};

struct StudioCallbacks {
    std::function<void(const std::wstring& path)> onApplyWallpaper;
    std::function<void()> onStopWallpaper;
    std::function<void()> onTogglePause;
    std::function<void()> onNextWallpaper;
    std::function<void(double sec)> onSeekVideo;
    std::function<void(const std::vector<std::wstring>& files)> onAddFiles;
    std::function<void(const std::wstring& path)> onRemoveFile;
    std::function<void()> onSettingsChanged;
    std::function<void(bool enabled)> onToggleAutostart;
    std::function<void(bool enabled)> onToggleContextMenu;
    std::function<void()> onExitApp;
};

class StudioWindow {
public:
    StudioWindow();
    ~StudioWindow();

    bool Create(HINSTANCE hInstance, AppConfig* config, std::vector<WallpaperItem>* library,
                const StudioCallbacks& callbacks);
    void Destroy();
    void ShowAndRestore();
    void HideToTray();
    bool IsVisible() const;
    HWND Handle() const { return hwnd_; }

    void RefreshThumbnails();
    void UpdateStatus(const StudioStatus& status);
    void SelectItemByPath(const std::wstring& path);
    void OpenAddFileDialog();

    // Photoshop-style contextual coach-mark tour; first launch + replay from top bar / help button.
    void StartGuidedTour();
    void FinishGuidedTour();
    bool IsGuidedTourActive() const { return tourStep_ >= 0; }

    bool CaptureWindowBmp(const std::wstring& outBmpPath);

private:
    enum class Tab { Library = 0, Playlist = 1, AudioWidgets = 2, MonitorsTaskbar = 3, SystemHotkeys = 4 };
    enum class InspectorTab { BasicTrim = 0, ProShaders = 1 };
    enum class LibFilter { All = 0, Favorites = 1, Videos = 2, GifWeb = 3, Shaders = 4 };

    struct HitZone { int id = 0; D2D1_RECT_F rect{}; int indexParam = -1; };
    struct SliderZone { int id = 0; D2D1_RECT_F rect{}; float minVal = 0.0f; float maxVal = 1.0f; float* target = nullptr; };

    bool EnsureD2D();
    void DiscardDeviceResources();
    void Render();
    void RenderSidebar(float w, float h, D2D1_COLOR_F accent);
    void RenderLibraryTab(float contentLeft, float contentRight, float h, D2D1_COLOR_F accent);
    void RenderPlaylistTab(float contentLeft, float contentRight, float h, D2D1_COLOR_F accent);
    void RenderAudioWidgetsTab(float contentLeft, float contentRight, float h, D2D1_COLOR_F accent);
    void RenderMonitorsTaskbarTab(float contentLeft, float contentRight, float h, D2D1_COLOR_F accent);
    void RenderSystemHotkeysTab(float contentLeft, float contentRight, float h, D2D1_COLOR_F accent);
    void RenderInspector(float inspectorLeft, float w, float h, D2D1_COLOR_F accent);
    void RenderGuidedTourOverlay(float w, float h, D2D1_COLOR_F accent);

    std::vector<int> BuildFilteredIndices() const;
    ID2D1Bitmap* GetOrLoadThumbBitmap(const std::wstring& thumbPath);
    D2D1_COLOR_F ComputeAccentColor() const;

    void OnMouseDown(float dipX, float dipY);
    void OnMouseMove(float dipX, float dipY, bool leftDown);
    void OnMouseUp();
    void OnMouseWheel(float dipX, float dipY, short delta);
    void OnCharInput(wchar_t ch);
    void HandleButtonClick(int id, int indexParam);
    void UpdateSliderFromX(const SliderZone& s, float dipX);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    float dpiScale_ = 1.0f;

    AppConfig* config_ = nullptr;
    std::vector<WallpaperItem>* library_ = nullptr;
    StudioCallbacks callbacks_{};
    StudioStatus status_{};

    Tab activeTab_ = Tab::Library;
    InspectorTab inspectorTab_ = InspectorTab::BasicTrim;
    LibFilter libFilter_ = LibFilter::All;
    std::wstring searchQuery_;
    bool searchFocused_ = false;

    int selectedIndex_ = 0;
    int hoveredHitId_ = 0;
    int hoveredCardIdx_ = -1;
    int activeSliderId_ = 0;
    bool scrubbingTimeline_ = false;
    int timelineDragMode_ = 0; // 0=none, 1=playhead, 2=trim start, 3=trim end
    D2D1_RECT_F timelineRect_{};
    float libraryScrollY_ = 0.0f;
    float maxLibraryScrollY_ = 0.0f;
    float inspectorScrollY_ = 0.0f;
    float maxInspectorScrollY_ = 0.0f;
    float tabScrollY_ = 0.0f;
    float maxTabScrollY_ = 0.0f;

    // Interactive Photoshop-like tutorial state + contextual tooltip anchors.
    int tourStep_ = -1;
    D2D1_RECT_F tourTargets_[8]{};

    std::vector<HitZone> hitZones_;
    std::vector<SliderZone> sliderZones_;

    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<ID2D1HwndRenderTarget> rt_;
    ComPtr<IDWriteTextFormat> fmtTitle_;
    ComPtr<IDWriteTextFormat> fmtSection_;
    ComPtr<IDWriteTextFormat> fmtBody_;
    ComPtr<IDWriteTextFormat> fmtBodyBold_;
    ComPtr<IDWriteTextFormat> fmtSmall_;
    ComPtr<IDWriteTextFormat> fmtBadge_;
    std::unordered_map<std::wstring, ComPtr<ID2D1Bitmap>> thumbBitmaps_;
};

}  // namespace wp
