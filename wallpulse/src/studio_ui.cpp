#include "studio_ui.h"
#include "desktop_host.h"
#include "media_tools.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace wp {

namespace {

constexpr const wchar_t* kStudioWndClass = L"WallPulseStudioMainWindow";

enum HitId {
    HIT_NAV_LIBRARY = 1,
    HIT_NAV_PLAYLIST,
    HIT_NAV_AUDIO_WIDGETS,
    HIT_NAV_MONITORS,
    HIT_NAV_SYSTEM,

    HIT_SEARCH_BOX,
    HIT_SEARCH_CLEAR,
    HIT_FILTER_ALL,
    HIT_FILTER_FAV,
    HIT_FILTER_VIDEO,
    HIT_FILTER_GIFWEB,
    HIT_FILTER_SHADER,
    HIT_ADD_FILE,
    HIT_CARD_SELECT,
    HIT_CARD_STAR,

    HIT_APPLY_SELECTED,
    HIT_TOGGLE_PAUSE,
    HIT_NEXT_WALLPAPER,
    HIT_STOP_WALLPAPER,
    HIT_TOGGLE_FAVORITE,
    HIT_REMOVE_SELECTED,

    HIT_INSP_TAB_BASIC,
    HIT_INSP_TAB_SHADERS,
    HIT_SCALE_FILL,
    HIT_SCALE_FIT,
    HIT_SCALE_STRETCH,
    HIT_SCALE_CENTER,
    HIT_TOGGLE_MUTE,
    HIT_TOGGLE_SEAMLESS_LOOP,
    HIT_RESET_PROFILE_FX,
    HIT_TIMELINE_SCRUB,

    // Playlist & Day/Night (#3)
    HIT_PLAYLIST_OFF,
    HIT_PLAYLIST_INT_1,
    HIT_PLAYLIST_INT_5,
    HIT_PLAYLIST_INT_15,
    HIT_PLAYLIST_INT_30,
    HIT_PLAYLIST_INT_60,
    HIT_TOGGLE_SHUFFLE,
    HIT_TOGGLE_DAY_NIGHT,
    HIT_DAY_HOUR_DEC,
    HIT_DAY_HOUR_INC,
    HIT_NIGHT_HOUR_DEC,
    HIT_NIGHT_HOUR_INC,
    HIT_SET_DAY_WALL,
    HIT_SET_NIGHT_WALL,

    // Audio Visualizer (#1) & Desktop Widgets (#5)
    HIT_VIS_OFF,
    HIT_VIS_BARS,
    HIT_VIS_WAVE,
    HIT_VIS_RING,
    HIT_TOGGLE_BASS_ZOOM,
    HIT_TOGGLE_WIDGET_CLOCK,
    HIT_TOGGLE_CLOCK_24H,
    HIT_TOGGLE_CLOCK_SEC,
    HIT_TOGGLE_WIDGET_MEDIA,
    HIT_TOGGLE_WIDGET_SYS,

    // Monitors (#4) & Taskbar / Accent (#8)
    HIT_MON_PRIMARY,
    HIT_MON_CLONE,
    HIT_MON_SPAN,
    HIT_MON_SINGLE,
    HIT_MON_INDEPENDENT,
    HIT_MON_INDEX_BASE,
    HIT_MON_ASSIGN_CURRENT,
    HIT_TASKBAR_DEFAULT,
    HIT_TASKBAR_CLEAR,
    HIT_TASKBAR_BLUR,
    HIT_ACCENT_AUTO,
    HIT_ACCENT_CYAN,
    HIT_ACCENT_PURPLE,
    HIT_ACCENT_EMERALD,
    HIT_ACCENT_AMBER,

    // System & Global Hotkeys (#9)
    HIT_TOGGLE_AUTOSTART,
    HIT_TOGGLE_MINIMIZED,
    HIT_TOGGLE_PAUSE_FS,
    HIT_TOGGLE_PAUSE_MAX,
    HIT_TOGGLE_PAUSE_BAT,
    HIT_TOGGLE_MUTE_UNFOCUS,
    HIT_TOGGLE_CTX_MENU,
    HIT_TOGGLE_HOTKEYS,
    HIT_FPS_30,
    HIT_FPS_60,
    HIT_FPS_120,
    HIT_FPS_144,
    HIT_EXIT_APP,
};

enum SliderId {
    SLIDER_VOLUME = 1,
    SLIDER_SPEED,
    SLIDER_TRIM_START,
    SLIDER_TRIM_END,
    SLIDER_BRIGHTNESS,
    SLIDER_CONTRAST,
    SLIDER_SATURATION,
    SLIDER_HUE,
    SLIDER_SHARPEN,
    SLIDER_BLOOM,
    SLIDER_CA,
    SLIDER_VHS,
    SLIDER_VIGNETTE,
    SLIDER_CROSSFADE,
    SLIDER_AUDIO_SENS,
    SLIDER_WIDGET_OPACITY,
};

D2D1_COLOR_F Rgba(uint32_t hexRgb, float alpha = 1.0f) {
    float r = static_cast<float>((hexRgb >> 16) & 0xFF) / 255.0f;
    float g = static_cast<float>((hexRgb >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>(hexRgb & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, alpha);
}

bool PtInRectF(const D2D1_RECT_F& r, float x, float y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

uint32_t KindBadgeHex(WallpaperKind k) {
    switch (k) {
        case WallpaperKind::Gif: return 0xEC4899;
        case WallpaperKind::Shader: return 0xA855F7;
        case WallpaperKind::Web: return 0x10B981;
        default: return 0x0EA5E9;
    }
}

}  // namespace

StudioWindow::StudioWindow() = default;

StudioWindow::~StudioWindow() {
    Destroy();
}

D2D1_COLOR_F StudioWindow::ComputeAccentColor() const {
    if (!config_) return Rgba(0x0EA5E9);
    return Rgba(config_->ResolveAccentColorRGB());
}

bool StudioWindow::Create(
    HINSTANCE hInstance,
    AppConfig* config,
    std::vector<WallpaperItem>* library,
    const StudioCallbacks& callbacks) {
    hInstance_ = hInstance;
    config_ = config;
    library_ = library;
    callbacks_ = callbacks;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance_, MAKEINTRESOURCEW(IDI_WALLPULSE_ICON));
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = CreateSolidBrush(RGB(13, 17, 26));
    wc.lpszClassName = kStudioWndClass;
    RegisterClassExW(&wc);

    HDC screenDc = GetDC(nullptr);
    int dpiX = screenDc ? GetDeviceCaps(screenDc, LOGPIXELSX) : 96;
    if (screenDc) ReleaseDC(nullptr, screenDc);
    dpiScale_ = std::max(1.0f, static_cast<float>(dpiX) / 96.0f);

    int winW = static_cast<int>(1200.0f * dpiScale_);
    int winH = static_cast<int>(770.0f * dpiScale_);

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int posX = work.left + std::max<int>(20, ((work.right - work.left) - winW) / 2);
    int posY = work.top + std::max<int>(20, ((work.bottom - work.top) - winH) / 2);

    hwnd_ = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        kStudioWndClass,
        L"WallPulse Studio v2.0 PRO — Живые обои, Шейдеры & Аудиовизуализация",
        WS_OVERLAPPEDWINDOW,
        posX, posY, winW, winH,
        nullptr, nullptr, hInstance_, this);

    if (!hwnd_) return false;

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    DragAcceptFiles(hwnd_, TRUE);
    EnsureD2D();
    return true;
}

void StudioWindow::Destroy() {
    DiscardDeviceResources();
    fmtBadge_.Reset();
    fmtSmall_.Reset();
    fmtBodyBold_.Reset();
    fmtBody_.Reset();
    fmtSection_.Reset();
    fmtTitle_.Reset();
    dwriteFactory_.Reset();
    d2dFactory_.Reset();

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void StudioWindow::ShowAndRestore() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_SHOW);
    if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::HideToTray() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_HIDE);
}

bool StudioWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
}

void StudioWindow::RefreshThumbnails() {
    thumbBitmaps_.clear();
    if (hwnd_ && IsWindowVisible(hwnd_)) InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::UpdateStatus(const StudioStatus& status) {
    status_ = status;
    if (hwnd_ && IsWindowVisible(hwnd_)) InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::SelectItemByPath(const std::wstring& path) {
    if (!library_) return;
    for (size_t i = 0; i < library_->size(); ++i) {
        if (_wcsicmp((*library_)[i].path.c_str(), path.c_str()) == 0) {
            selectedIndex_ = static_cast<int>(i);
            break;
        }
    }
    if (hwnd_ && IsWindowVisible(hwnd_)) InvalidateRect(hwnd_, nullptr, FALSE);
}

bool StudioWindow::EnsureD2D() {
    if (!d2dFactory_) {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf()))) {
            return false;
        }
    }
    if (!dwriteFactory_) {
        if (FAILED(DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf())))) {
            return false;
        }

        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 19.0f, L"ru-RU", fmtTitle_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 14.5f, L"ru-RU", fmtSection_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.8f, L"ru-RU", fmtBody_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.8f, L"ru-RU", fmtBodyBold_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 11.3f, L"ru-RU", fmtSmall_.GetAddressOf());
        dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 10.2f, L"ru-RU", fmtBadge_.GetAddressOf());
    }

    if (!rt_ && hwnd_) {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(
            static_cast<UINT32>(std::max<LONG>(1, rc.right - rc.left)),
            static_cast<UINT32>(std::max<LONG>(1, rc.bottom - rc.top)));

        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
        props.dpiX = 96.0f * dpiScale_;
        props.dpiY = 96.0f * dpiScale_;

        if (FAILED(d2dFactory_->CreateHwndRenderTarget(
                props, D2D1::HwndRenderTargetProperties(hwnd_, size), rt_.GetAddressOf()))) {
            return false;
        }
    }
    return rt_ != nullptr;
}

void StudioWindow::DiscardDeviceResources() {
    thumbBitmaps_.clear();
    rt_.Reset();
}

ID2D1Bitmap* StudioWindow::GetOrLoadThumbBitmap(const std::wstring& thumbPath) {
    if (thumbPath.empty() || !rt_) return nullptr;
    auto it = thumbBitmaps_.find(thumbPath);
    if (it != thumbBitmaps_.end()) return it->second.Get();

    uint32_t w = 0, h = 0;
    std::vector<uint8_t> bgra;
    if (!LoadBmpBGRA(thumbPath, w, h, bgra)) return nullptr;

    D2D1_BITMAP_PROPERTIES bp{};
    bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
    bp.dpiX = 96.0f;
    bp.dpiY = 96.0f;

    ComPtr<ID2D1Bitmap> bmp;
    if (FAILED(rt_->CreateBitmap(D2D1::SizeU(w, h), bgra.data(), w * 4, &bp, bmp.GetAddressOf()))) {
        return nullptr;
    }
    ID2D1Bitmap* raw = bmp.Get();
    thumbBitmaps_[thumbPath] = std::move(bmp);
    return raw;
}

std::vector<int> StudioWindow::BuildFilteredIndices() const {
    std::vector<int> out;
    if (!library_) return out;

    for (size_t i = 0; i < library_->size(); ++i) {
        const auto& item = (*library_)[i];
        if (libFilter_ == LibFilter::Favorites && !item.favorite) continue;
        if (libFilter_ == LibFilter::Videos && item.kind != WallpaperKind::Video) continue;
        if (libFilter_ == LibFilter::GifWeb && item.kind != WallpaperKind::Gif && item.kind != WallpaperKind::Web) continue;
        if (libFilter_ == LibFilter::Shaders && item.kind != WallpaperKind::Shader) continue;

        if (!searchQuery_.empty()) {
            if (!ContainsCaseInsensitive(item.title, searchQuery_) &&
                !ContainsCaseInsensitive(item.path, searchQuery_)) {
                continue;
            }
        }
        out.push_back(static_cast<int>(i));
    }
    return out;
}

void StudioWindow::Render() {
    if (!EnsureD2D()) return;

    hitZones_.clear();
    sliderZones_.clear();

    D2D1_SIZE_F sz = rt_->GetSize();
    const float w = sz.width;
    const float h = sz.height;
    const float sidebarW = 216.0f;
    const float inspectorW = (activeTab_ == Tab::Library) ? 336.0f : 0.0f;
    const float contentLeft = sidebarW;
    const float contentRight = std::max(contentLeft + 240.0f, w - inspectorW);
    const D2D1_COLOR_F accent = ComputeAccentColor();

    rt_->BeginDraw();
    rt_->Clear(Rgba(0x0B0F18));

    RenderSidebar(sidebarW, h, accent);

    if (activeTab_ == Tab::Library) {
        RenderLibraryTab(contentLeft, contentRight, h, accent);
        RenderInspector(contentRight, w, h, accent);
    } else if (activeTab_ == Tab::Playlist) {
        RenderPlaylistTab(contentLeft, w, h, accent);
    } else if (activeTab_ == Tab::AudioWidgets) {
        RenderAudioWidgetsTab(contentLeft, w, h, accent);
    } else if (activeTab_ == Tab::MonitorsTaskbar) {
        RenderMonitorsTaskbarTab(contentLeft, w, h, accent);
    } else {
        RenderSystemHotkeysTab(contentLeft, w, h, accent);
    }

    if (rt_->EndDraw() == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

void StudioWindow::RenderSidebar(float sidebarW, float h, D2D1_COLOR_F accent) {
    ComPtr<ID2D1SolidColorBrush> brBg, brBorder, brAccent, brAccentSoft, brText, brSub, brHover, brCard;
    rt_->CreateSolidColorBrush(Rgba(0x101624), brBg.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x1F2B42), brBorder.GetAddressOf());
    rt_->CreateSolidColorBrush(accent, brAccent.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(accent.r, accent.g, accent.b, 0.17f), brAccentSoft.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0xF1F5F9), brText.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x94A3B8), brSub.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x182236), brHover.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x141C2E), brCard.GetAddressOf());

    rt_->FillRectangle(D2D1::RectF(0, 0, sidebarW, h), brBg.Get());
    rt_->DrawLine(D2D1::Point2F(sidebarW, 0), D2D1::Point2F(sidebarW, h), brBorder.Get(), 1.0f);

    D2D1_ROUNDED_RECT logoBadge{D2D1::RectF(18, 20, 50, 52), 9.0f, 9.0f};
    rt_->FillRoundedRectangle(logoBadge, brAccent.Get());
    fmtTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmtTitle_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawTextW(L"W", 1, fmtTitle_.Get(), logoBadge.rect, brText.Get());
    fmtTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtTitle_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    rt_->DrawTextW(L"WallPulse", 9, fmtTitle_.Get(), D2D1::RectF(60, 17, sidebarW - 10, 42), brText.Get());
    rt_->DrawTextW(L"Studio v2.0 PRO", 15, fmtSmall_.Get(), D2D1::RectF(61, 40, sidebarW - 10, 58), brAccent.Get());

    struct NavItem { int hit; Tab tab; const wchar_t* label; };
    const NavItem navs[] = {
        {HIT_NAV_LIBRARY,       Tab::Library,         L"▣  Библиотека"},
        {HIT_NAV_PLAYLIST,      Tab::Playlist,        L"↻  Плейлист & Сутки"},
        {HIT_NAV_AUDIO_WIDGETS, Tab::AudioWidgets,    L"♫  Аудио & Виджеты"},
        {HIT_NAV_MONITORS,      Tab::MonitorsTaskbar, L"⊞  Мониторы & Панель"},
        {HIT_NAV_SYSTEM,        Tab::SystemHotkeys,   L"⚙  Система & Хоткеи"},
    };

    float navY = 82.0f;
    for (const auto& n : navs) {
        D2D1_RECT_F r = D2D1::RectF(12, navY, sidebarW - 12, navY + 40);
        hitZones_.push_back({n.hit, r, -1});

        bool active = (activeTab_ == n.tab);
        bool hovered = (hoveredHitId_ == n.hit);
        if (active) {
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{r, 8.0f, 8.0f}, brAccentSoft.Get());
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(r.left, r.top + 8, r.left + 3.5f, r.bottom - 8), 2.0f, 2.0f}, brAccent.Get());
        } else if (hovered) {
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{r, 8.0f, 8.0f}, brHover.Get());
        }

        fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(n.label, static_cast<UINT32>(wcslen(n.label)), fmtBodyBold_.Get(),
                       D2D1::RectF(r.left + 12, r.top, r.right - 8, r.bottom),
                       active ? brAccent.Get() : brText.Get());
        fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        navY += 46.0f;
    }

    D2D1_RECT_F stCard = D2D1::RectF(12, h - 108, sidebarW - 12, h - 14);
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{stCard, 10.0f, 10.0f}, brCard.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{stCard, 10.0f, 10.0f}, brBorder.Get(), 1.0f);

    uint32_t dotHex = 0x64748B;
    std::wstring stTitle = L"Остановлено";
    std::wstring stSub = L"Выберите сцену";

    if (status_.isPlaying) {
        if (status_.isSmartPaused) {
            dotHex = 0xF59E0B;
            stTitle = L"Умная пауза (0% GPU)";
            stSub = status_.smartReason;
        } else if (status_.isUserPaused) {
            dotHex = 0xEAB308;
            stTitle = L"На паузе";
            stSub = L"Ctrl+Alt+P — продолжить";
        } else {
            dotHex = 0x22C55E;
            wchar_t buf[64];
            swprintf_s(buf, L"Активно • %u FPS", config_ ? config_->targetFps : 60u);
            stTitle = buf;
            stSub = status_.attachSummary.empty() ? L"Позади значков раб. стола" : status_.attachSummary;
        }
    }

    ComPtr<ID2D1SolidColorBrush> brDot;
    rt_->CreateSolidColorBrush(Rgba(dotHex), brDot.GetAddressOf());
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(stCard.left + 16, stCard.top + 20), 4.5f, 4.5f), brDot.Get());
    rt_->DrawTextW(stTitle.c_str(), static_cast<UINT32>(stTitle.size()), fmtBodyBold_.Get(),
                   D2D1::RectF(stCard.left + 28, stCard.top + 10, stCard.right - 8, stCard.top + 32), brText.Get());
    rt_->DrawTextW(stSub.c_str(), static_cast<UINT32>(stSub.size()), fmtSmall_.Get(),
                   D2D1::RectF(stCard.left + 12, stCard.top + 36, stCard.right - 10, stCard.bottom - 8), brSub.Get());
}

void StudioWindow::RenderLibraryTab(float left, float right, float h, D2D1_COLOR_F accent) {
    ComPtr<ID2D1SolidColorBrush> brText, brSub, brAccent, brAccentSoft, brCard, brCardHover, brBorder, brBadgeBg, brStar;
    rt_->CreateSolidColorBrush(Rgba(0xF8FAFC), brText.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x94A3B8), brSub.GetAddressOf());
    rt_->CreateSolidColorBrush(accent, brAccent.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(accent.r, accent.g, accent.b, 0.20f), brAccentSoft.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x131B2C), brCard.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x1B263D), brCardHover.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x23314D), brBorder.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x090D16, 0.84f), brBadgeBg.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0xFBBF24), brStar.GetAddressOf());

    rt_->DrawTextW(L"Библиотека обоев", 16, fmtTitle_.Get(), D2D1::RectF(left + 22, 16, right - 180, 44), brText.Get());

    D2D1_RECT_F addBtn = D2D1::RectF(right - 172, 15, right - 20, 47);
    hitZones_.push_back({HIT_ADD_FILE, addBtn, -1});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{addBtn, 8.0f, 8.0f}, brAccent.Get());
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawTextW(L"+ Добавить обои", 15, fmtBodyBold_.Get(), addBtn, brText.Get());
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    D2D1_RECT_F searchRc = D2D1::RectF(left + 22, 56, left + 236, 86);
    hitZones_.push_back({HIT_SEARCH_BOX, searchRc, -1});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{searchRc, 7.0f, 7.0f}, brCard.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{searchRc, 7.0f, 7.0f},
                              searchFocused_ ? brAccent.Get() : brBorder.Get(), searchFocused_ ? 1.6f : 1.0f);

    std::wstring searchDisp = searchQuery_.empty()
        ? (searchFocused_ ? L"🔍 |" : L"🔍 Поиск по названию...")
        : (L"🔍 " + searchQuery_ + (searchFocused_ ? L"|" : L""));
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawTextW(searchDisp.c_str(), static_cast<UINT32>(searchDisp.size()), fmtSmall_.Get(),
                   D2D1::RectF(searchRc.left + 10, searchRc.top, searchRc.right - 24, searchRc.bottom),
                   searchQuery_.empty() ? brSub.Get() : brText.Get());
    if (!searchQuery_.empty()) {
        D2D1_RECT_F clrRc = D2D1::RectF(searchRc.right - 24, searchRc.top, searchRc.right, searchRc.bottom);
        hitZones_.push_back({HIT_SEARCH_CLEAR, clrRc, -1});
        rt_->DrawTextW(L"✕", 1, fmtSmall_.Get(), clrRc, brSub.Get());
    }
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    struct FilterPill { int hit; LibFilter f; const wchar_t* text; float w; };
    const FilterPill pills[] = {
        {HIT_FILTER_ALL,    LibFilter::All,       L"Все",     46.0f},
        {HIT_FILTER_FAV,    LibFilter::Favorites, L"★ Избр.", 68.0f},
        {HIT_FILTER_VIDEO,  LibFilter::Videos,    L"Видео",   58.0f},
        {HIT_FILTER_GIFWEB, LibFilter::GifWeb,    L"GIF/Web", 68.0f},
        {HIT_FILTER_SHADER, LibFilter::Shaders,   L"Шейдеры", 74.0f},
    };
    float px = searchRc.right + 10.0f;
    for (const auto& p : pills) {
        if (px + p.w > right - 16.0f) break;
        D2D1_RECT_F pr = D2D1::RectF(px, 56, px + p.w, 86);
        hitZones_.push_back({p.hit, pr, -1});
        bool act = (libFilter_ == p.f);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{pr, 7.0f, 7.0f}, act ? brAccentSoft.Get() : brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{pr, 7.0f, 7.0f}, act ? brAccent.Get() : brBorder.Get(), 1.0f);
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(p.text, static_cast<UINT32>(wcslen(p.text)), fmtSmall_.Get(), pr, act ? brAccent.Get() : brSub.Get());
        px += p.w + 6.0f;
    }
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    std::vector<int> indices = BuildFilteredIndices();
    const float gridTop = 98.0f;
    D2D1_RECT_F clipRect = D2D1::RectF(left, gridTop, right, h);
    rt_->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (indices.empty()) {
        std::wstring emptyMsg = searchQuery_.empty()
            ? L"В этой категории пока пусто. Перетащите сюда .mp4, .gif, .hlsl или .html!"
            : L"Ничего не найдено по вашему запросу.";
        rt_->DrawTextW(emptyMsg.c_str(), static_cast<UINT32>(emptyMsg.size()), fmtBody_.Get(),
                       D2D1::RectF(left + 28, gridTop + 40, right - 28, gridTop + 100), brSub.Get());
        maxLibraryScrollY_ = 0.0f;
    } else {
        const float padX = 20.0f;
        const float gap = 14.0f;
        const float availW = std::max(220.0f, (right - left) - padX * 2.0f);
        const int cols = std::max(1, static_cast<int>((availW + gap) / 215.0f));
        const float cardW = (availW - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
        const float thumbH = cardW * (9.0f / 16.0f);
        const float cardH = thumbH + 54.0f;

        const int rows = static_cast<int>((indices.size() + cols - 1) / cols);
        const float totalH = static_cast<float>(rows) * (cardH + gap) + 20.0f;
        maxLibraryScrollY_ = std::max(0.0f, totalH - (h - gridTop));
        libraryScrollY_ = std::clamp(libraryScrollY_, 0.0f, maxLibraryScrollY_);

        for (size_t k = 0; k < indices.size(); ++k) {
            int libIdx = indices[k];
            int col = static_cast<int>(k) % cols;
            int row = static_cast<int>(k) / cols;

            float cx = left + padX + static_cast<float>(col) * (cardW + gap);
            float cy = gridTop + static_cast<float>(row) * (cardH + gap) - libraryScrollY_;
            if (cy + cardH < gridTop - 10.0f || cy > h + 10.0f) continue;

            const auto& item = (*library_)[static_cast<size_t>(libIdx)];
            D2D1_RECT_F cardRc = D2D1::RectF(cx, cy, cx + cardW, cy + cardH);

            D2D1_RECT_F starRc = D2D1::RectF(cx + 6, cy + 6, cx + 32, cy + 30);
            hitZones_.push_back({HIT_CARD_STAR, starRc, libIdx});
            hitZones_.push_back({HIT_CARD_SELECT, cardRc, libIdx});

            bool isSel = (libIdx == selectedIndex_);
            bool isAct = config_ && (_wcsicmp(item.path.c_str(), config_->activeVideoPath.c_str()) == 0);
            bool isHov = (hoveredCardIdx_ == libIdx);

            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{cardRc, 10.0f, 10.0f}, isHov ? brCardHover.Get() : brCard.Get());

            D2D1_RECT_F thumbRc = D2D1::RectF(cx + 5, cy + 5, cx + cardW - 5, cy + 5 + thumbH - 5);
            ID2D1Bitmap* bmp = GetOrLoadThumbBitmap(item.thumbPath);
            if (bmp) {
                rt_->DrawBitmap(bmp, thumbRc, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            } else {
                rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{thumbRc, 6.0f, 6.0f}, brBadgeBg.Get());
            }

            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{starRc, 5.0f, 5.0f}, brBadgeBg.Get());
            fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            rt_->DrawTextW(item.favorite ? L"★" : L"☆", 1, fmtBodyBold_.Get(), starRc,
                           item.favorite ? brStar.Get() : brSub.Get());
            fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

            const wchar_t* kindTxt = WallpaperKindBadge(item.kind, item.path);
            ComPtr<ID2D1SolidColorBrush> brKind;
            rt_->CreateSolidColorBrush(Rgba(KindBadgeHex(item.kind), 0.90f), brKind.GetAddressOf());
            D2D1_RECT_F kindRc = D2D1::RectF(thumbRc.right - 56, thumbRc.bottom - 22, thumbRc.right - 5, thumbRc.bottom - 5);
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{kindRc, 4.0f, 4.0f}, brKind.Get());
            fmtBadge_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtBadge_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            rt_->DrawTextW(kindTxt, static_cast<UINT32>(wcslen(kindTxt)), fmtBadge_.Get(), kindRc, brText.Get());

            if (isAct) {
                D2D1_RECT_F liveRc = D2D1::RectF(thumbRc.right - 74, thumbRc.top + 5, thumbRc.right - 5, thumbRc.top + 23);
                rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{liveRc, 5.0f, 5.0f}, brAccent.Get());
                rt_->DrawTextW(L"АКТИВНО", 7, fmtBadge_.Get(), liveRc, brText.Get());
            }
            fmtBadge_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmtBadge_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

            rt_->DrawTextW(item.title.c_str(), static_cast<UINT32>(item.title.size()), fmtBodyBold_.Get(),
                           D2D1::RectF(cx + 10, cy + thumbH + 5, cx + cardW - 10, cy + thumbH + 26),
                           brText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            std::wstring meta = (item.kind == WallpaperKind::Shader)
                ? L"60 FPS GPU • 0 МБ"
                : ((item.width > 0 ? (std::to_wstring(item.width) + L"×" + std::to_wstring(item.height)) : L"Live") +
                   (item.durationSec > 0.1 ? (L" • " + FormatDuration(item.durationSec)) : L""));
            rt_->DrawTextW(meta.c_str(), static_cast<UINT32>(meta.size()), fmtSmall_.Get(),
                           D2D1::RectF(cx + 10, cy + thumbH + 27, cx + cardW - 10, cy + cardH - 5), brSub.Get());

            rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{cardRc, 10.0f, 10.0f},
                                      isSel ? brAccent.Get() : brBorder.Get(), isSel ? 2.2f : 1.0f);
        }
    }

    rt_->PopAxisAlignedClip();
}

void StudioWindow::RenderInspector(float left, float w, float h, D2D1_COLOR_F accent) {
    ComPtr<ID2D1SolidColorBrush> brBg, brBorder, brText, brSub, brAccent, brAccentSoft, brBtn, brDanger, brTrack, brStar;
    rt_->CreateSolidColorBrush(Rgba(0x101624), brBg.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x1F2B42), brBorder.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0xF8FAFC), brText.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x94A3B8), brSub.GetAddressOf());
    rt_->CreateSolidColorBrush(accent, brAccent.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(accent.r, accent.g, accent.b, 0.20f), brAccentSoft.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x1A253B), brBtn.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0xEF4444, 0.22f), brDanger.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x25334D), brTrack.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0xFBBF24), brStar.GetAddressOf());

    rt_->FillRectangle(D2D1::RectF(left, 0, w, h), brBg.Get());
    rt_->DrawLine(D2D1::Point2F(left, 0), D2D1::Point2F(left, h), brBorder.Get(), 1.0f);

    if (!library_ || library_->empty()) return;
    selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(library_->size()) - 1);
    WallpaperItem& sel = (*library_)[static_cast<size_t>(selectedIndex_)];
    WallpaperProfile& prof = sel.profile;

    const float pad = 16.0f;
    const float x0 = left + pad;
    const float x1 = w - pad;

    const float prevH = (x1 - x0) * (9.0f / 16.0f) * 0.86f;
    D2D1_RECT_F prevRc = D2D1::RectF(x0, 14, x1, 14 + prevH);
    ID2D1Bitmap* bmp = GetOrLoadThumbBitmap(sel.thumbPath);
    if (bmp) rt_->DrawBitmap(bmp, prevRc, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    else rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{prevRc, 8.0f, 8.0f}, brBtn.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{prevRc, 8.0f, 8.0f}, brBorder.Get(), 1.0f);

    D2D1_RECT_F favBtn = D2D1::RectF(prevRc.right - 36, prevRc.top + 8, prevRc.right - 8, prevRc.top + 34);
    hitZones_.push_back({HIT_TOGGLE_FAVORITE, favBtn, -1});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{favBtn, 6.0f, 6.0f}, brBg.Get());
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawTextW(sel.favorite ? L"★" : L"☆", 1, fmtBodyBold_.Get(), favBtn, sel.favorite ? brStar.Get() : brSub.Get());

    float y = prevRc.bottom + 8.0f;
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    rt_->DrawTextW(sel.title.c_str(), static_cast<UINT32>(sel.title.size()), fmtSection_.Get(),
                   D2D1::RectF(x0, y, x1, y + 22), brText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    y += 24.0f;

    float applyW = (x1 - x0) - 132.0f;
    D2D1_RECT_F applyRc = D2D1::RectF(x0, y, x0 + applyW, y + 34);
    D2D1_RECT_F pauseRc = D2D1::RectF(applyRc.right + 6, y, applyRc.right + 46, y + 34);
    D2D1_RECT_F nextRc  = D2D1::RectF(pauseRc.right + 6, y, pauseRc.right + 44, y + 34);
    D2D1_RECT_F stopRc  = D2D1::RectF(nextRc.right + 6,  y, x1,                 y + 34);

    hitZones_.push_back({HIT_APPLY_SELECTED, applyRc, -1});
    hitZones_.push_back({HIT_TOGGLE_PAUSE, pauseRc, -1});
    hitZones_.push_back({HIT_NEXT_WALLPAPER, nextRc, -1});
    hitZones_.push_back({HIT_STOP_WALLPAPER, stopRc, -1});

    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{applyRc, 7.0f, 7.0f}, brAccent.Get());
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{pauseRc, 7.0f, 7.0f}, brBtn.Get());
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{nextRc,  7.0f, 7.0f}, brBtn.Get());
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{stopRc,  7.0f, 7.0f}, brBtn.Get());

    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawTextW(L"Применить", 9, fmtBodyBold_.Get(), applyRc, brText.Get());
    rt_->DrawTextW(status_.isUserPaused ? L"▶" : L"⏸", 1, fmtBodyBold_.Get(), pauseRc, brText.Get());
    rt_->DrawTextW(L"⏭", 1, fmtBodyBold_.Get(), nextRc, brText.Get());
    rt_->DrawTextW(L"■", 1, fmtBodyBold_.Get(), stopRc, brSub.Get());
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    y += 42.0f;

    float halfW = (x1 - x0 - 6.0f) * 0.5f;
    D2D1_RECT_F sub1 = D2D1::RectF(x0, y, x0 + halfW, y + 28);
    D2D1_RECT_F sub2 = D2D1::RectF(x0 + halfW + 6, y, x1, y + 28);
    hitZones_.push_back({HIT_INSP_TAB_BASIC, sub1, -1});
    hitZones_.push_back({HIT_INSP_TAB_SHADERS, sub2, -1});

    bool b1 = (inspectorTab_ == InspectorTab::BasicTrim);
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{sub1, 6.0f, 6.0f}, b1 ? brAccentSoft.Get() : brBtn.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{sub1, 6.0f, 6.0f}, b1 ? brAccent.Get() : brBorder.Get(), 1.0f);
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{sub2, 6.0f, 6.0f}, !b1 ? brAccentSoft.Get() : brBtn.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{sub2, 6.0f, 6.0f}, !b1 ? brAccent.Get() : brBorder.Get(), 1.0f);

    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawTextW(L"Основное & Цикл", 15, fmtSmall_.Get(), sub1, b1 ? brAccent.Get() : brSub.Get());
    rt_->DrawTextW(L"PRO-Шейдеры (#6)", 16, fmtSmall_.Get(), sub2, !b1 ? brAccent.Get() : brSub.Get());
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    y += 38.0f;

    auto drawSlider = [&](int id, const wchar_t* label, float* valPtr, float minV, float maxV, const wchar_t* fmtStr, float mult = 1.0f) {
        wchar_t valBuf[48];
        swprintf_s(valBuf, fmtStr, (*valPtr) * mult);

        rt_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), fmtSmall_.Get(), D2D1::RectF(x0, y, x1 - 70, y + 16), brSub.Get());
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt_->DrawTextW(valBuf, static_cast<UINT32>(wcslen(valBuf)), fmtSmall_.Get(), D2D1::RectF(x1 - 75, y, x1, y + 16), brText.Get());
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

        y += 19.0f;
        D2D1_RECT_F hitRc = D2D1::RectF(x0, y - 5, x1, y + 13);
        sliderZones_.push_back({id, hitRc, minV, maxV, valPtr});

        D2D1_RECT_F trackRc = D2D1::RectF(x0, y + 2, x1, y + 6);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{trackRc, 2.0f, 2.0f}, brTrack.Get());

        float norm = (maxV > minV) ? std::clamp((*valPtr - minV) / (maxV - minV), 0.0f, 1.0f) : 0.0f;
        float knobX = x0 + norm * (x1 - x0);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0, y + 2, knobX, y + 6), 2.0f, 2.0f}, brAccent.Get());
        rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, y + 4), 6.0f, 6.0f), brText.Get());
        y += 18.0f;
    };

    if (inspectorTab_ == InspectorTab::BasicTrim) {
        rt_->DrawTextW(L"Масштабирование кадра", 21, fmtSmall_.Get(), D2D1::RectF(x0, y, x1, y + 16), brSub.Get());
        y += 20.0f;
        struct ScOpt { int hit; ScaleMode m; const wchar_t* lbl; };
        const ScOpt scOpts[4] = {
            {HIT_SCALE_FILL, ScaleMode::Cover, L"Заполнить"},
            {HIT_SCALE_FIT, ScaleMode::FitBars, L"Вписать"},
            {HIT_SCALE_STRETCH, ScaleMode::Stretch, L"Растянуть"},
            {HIT_SCALE_CENTER, ScaleMode::Center, L"По центру"},
        };
        float pillW = (x1 - x0 - 6.0f) * 0.5f;
        for (int i = 0; i < 4; ++i) {
            float px = x0 + (i % 2) * (pillW + 6.0f);
            float py = y + (i / 2) * 30.0f;
            D2D1_RECT_F pr = D2D1::RectF(px, py, px + pillW, py + 25.0f);
            hitZones_.push_back({scOpts[i].hit, pr, -1});
            bool on = (prof.scaleMode == scOpts[i].m);
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{pr, 5.0f, 5.0f}, on ? brAccentSoft.Get() : brBtn.Get());
            rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{pr, 5.0f, 5.0f}, on ? brAccent.Get() : brBorder.Get(), 1.0f);
            fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            rt_->DrawTextW(scOpts[i].lbl, static_cast<UINT32>(wcslen(scOpts[i].lbl)), fmtSmall_.Get(), pr, on ? brAccent.Get() : brText.Get());
        }
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        y += 66.0f;

        D2D1_RECT_F muteRc = D2D1::RectF(x1 - 92, y - 2, x1, y + 20);
        hitZones_.push_back({HIT_TOGGLE_MUTE, muteRc, -1});
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{muteRc, 5.0f, 5.0f}, prof.muted ? brDanger.Get() : brBtn.Get());
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(prof.muted ? L"🔇 Выкл" : L"🔊 Звук", 7, fmtSmall_.Get(), muteRc, brText.Get());
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        y += 24.0f;

        drawSlider(SLIDER_VOLUME, L"Громкость ролика", &prof.volume, 0.0f, 1.0f, L"%.0f%%", 100.0f);
        drawSlider(SLIDER_SPEED,  L"Скорость анимации", &prof.playbackRate, 0.25f, 2.0f, L"%.2fx");

        if (sel.kind == WallpaperKind::Video) {
            float maxDur = static_cast<float>(std::max(1.0, sel.durationSec > 0.1 ? sel.durationSec : status_.durationSec));
            wchar_t posTxt[64];
            swprintf_s(posTxt, L"Таймлайн: %s / %s",
                       FormatDuration(status_.currentTimeSec).c_str(),
                       FormatDuration(maxDur).c_str());
            rt_->DrawTextW(posTxt, static_cast<UINT32>(wcslen(posTxt)), fmtSmall_.Get(), D2D1::RectF(x0, y, x1, y + 16), brSub.Get());
            y += 18.0f;

            timelineRect_ = D2D1::RectF(x0, y, x1, y + 12);
            hitZones_.push_back({HIT_TIMELINE_SCRUB, timelineRect_, -1});
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{timelineRect_, 4.0f, 4.0f}, brTrack.Get());
            float playNorm = std::clamp(static_cast<float>(status_.currentTimeSec) / maxDur, 0.0f, 1.0f);
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0, y, x0 + playNorm * (x1 - x0), y + 12), 4.0f, 4.0f}, brAccent.Get());
            y += 18.0f;

            drawSlider(SLIDER_TRIM_START, L"Обрезка От (старт %)", &prof.trimStartPct, 0.0f, 0.85f, L"%.0f%%", 100.0f);
            drawSlider(SLIDER_TRIM_END,   L"Обрезка До (финиш %)", &prof.trimEndPct, 0.15f, 1.0f, L"%.0f%%", 100.0f);

            D2D1_RECT_F loopBtn = D2D1::RectF(x0, y, x1, y + 28);
            hitZones_.push_back({HIT_TOGGLE_SEAMLESS_LOOP, loopBtn, -1});
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{loopBtn, 6.0f, 6.0f}, prof.seamlessLoop ? brAccentSoft.Get() : brBtn.Get());
            rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{loopBtn, 6.0f, 6.0f}, prof.seamlessLoop ? brAccent.Get() : brBorder.Get(), 1.0f);
            fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            const wchar_t* loopLbl = prof.seamlessLoop ? L"✓ Бесшовный цикл (Crossfade) ВКЛ" : L"Бесшовный цикл ВЫКЛ";
            rt_->DrawTextW(loopLbl, static_cast<UINT32>(wcslen(loopLbl)), fmtSmall_.Get(), loopBtn,
                           prof.seamlessLoop ? brAccent.Get() : brSub.Get());
            fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            y += 34.0f;
        }

        if (!sel.isBuiltIn && y + 30.0f < h) {
            D2D1_RECT_F remRc = D2D1::RectF(x0, h - 40, x1, h - 12);
            hitZones_.push_back({HIT_REMOVE_SELECTED, remRc, -1});
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{remRc, 6.0f, 6.0f}, brDanger.Get());
            fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            rt_->DrawTextW(L"Убрать из библиотеки", 20, fmtSmall_.Get(), remRc, brText.Get());
            fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
    } else {
        drawSlider(SLIDER_BRIGHTNESS, L"Яркость",                 &prof.brightness, -0.5f,   0.5f,   L"%+.2f");
        drawSlider(SLIDER_CONTRAST,   L"Контраст",                &prof.contrast,    0.5f,   1.5f,   L"%.2f");
        drawSlider(SLIDER_SATURATION, L"Насыщенность",            &prof.saturation,  0.0f,   2.0f,   L"%.2f");
        drawSlider(SLIDER_HUE,        L"Оттенок (Hue Shift)",     &prof.hueShift, -180.0f, 180.0f,   L"%+.0f°");
        drawSlider(SLIDER_SHARPEN,    L"Резкость (AMD CAS)",      &prof.sharpen,     0.0f,   1.0f,   L"%.2f");
        drawSlider(SLIDER_BLOOM,      L"Свечение (Bloom)",        &prof.bloom,       0.0f,   1.0f,   L"%.2f");
        drawSlider(SLIDER_CA,         L"Хроматическая аберрация", &prof.chromatic,   0.0f,   1.0f,   L"%.2f");
        drawSlider(SLIDER_VHS,        L"Ретро VHS / ЭЛТ эффект",  &prof.crtEffect,   0.0f,   1.0f,   L"%.2f");
        drawSlider(SLIDER_VIGNETTE,   L"Виньетка по краям",       &prof.vignette,    0.0f,   0.8f,   L"%.2f");

        D2D1_RECT_F rstBtn = D2D1::RectF(x0, std::min(y + 4.0f, h - 42.0f), x1, std::min(y + 34.0f, h - 12.0f));
        hitZones_.push_back({HIT_RESET_PROFILE_FX, rstBtn, -1});
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{rstBtn, 6.0f, 6.0f}, brBtn.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{rstBtn, 6.0f, 6.0f}, brBorder.Get(), 1.0f);
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(L"Сбросить шейдеры профиля", 24, fmtSmall_.Get(), rstBtn, brText.Get());
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
}

void StudioWindow::RenderPlaylistTab(float left, float right, float, D2D1_COLOR_F accent) {
    if (!config_) return;
    ComPtr<ID2D1SolidColorBrush> brText, brSub, brCard, brBorder, brAccent, brAccentSoft, brTrack;
    rt_->CreateSolidColorBrush(Rgba(0xF8FAFC), brText.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x94A3B8), brSub.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x131B2C), brCard.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x23314D), brBorder.GetAddressOf());
    rt_->CreateSolidColorBrush(accent, brAccent.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(accent.r, accent.g, accent.b, 0.20f), brAccentSoft.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x25334D), brTrack.GetAddressOf());

    const float x0 = left + 28.0f;
    const float maxW = std::min(760.0f, (right - left) - 56.0f);
    const float x1 = x0 + maxW;

    rt_->DrawTextW(L"Плейлист, плавный Crossfade и расписание День / Ночь", 52, fmtTitle_.Get(),
                   D2D1::RectF(x0, 20, x1, 48), brText.Get());
    rt_->DrawTextW(L"Автоматическая смена живых обоев без чёрных вспышек с растворением через GPU.", 77, fmtBody_.Get(),
                   D2D1::RectF(x0, 50, x1, 74), brSub.Get());

    float y = 88.0f;

    auto drawToggleCard = [&](int hit, const wchar_t* title, const wchar_t* desc, bool on) {
        D2D1_RECT_F r = D2D1::RectF(x0, y, x1, y + 60);
        hitZones_.push_back({hit, r, -1});
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{r, 10.0f, 10.0f}, brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{r, 10.0f, 10.0f}, on ? brAccent.Get() : brBorder.Get(), 1.0f);
        rt_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), fmtBodyBold_.Get(), D2D1::RectF(r.left + 16, r.top + 10, r.right - 80, r.top + 30), brText.Get());
        rt_->DrawTextW(desc, static_cast<UINT32>(wcslen(desc)), fmtSmall_.Get(), D2D1::RectF(r.left + 16, r.top + 32, r.right - 80, r.bottom - 8), brSub.Get());

        D2D1_RECT_F sw = D2D1::RectF(r.right - 60, r.top + 18, r.right - 16, r.top + 42);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{sw, 12.0f, 12.0f}, on ? brAccent.Get() : brBorder.Get());
        rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(on ? sw.right - 12 : sw.left + 12, sw.top + 12), 9.0f, 9.0f), brText.Get());
        y += 70.0f;
    };

    rt_->DrawTextW(L"Автосмена обоев (интервал таймера плейлиста):", 45, fmtSection_.Get(), D2D1::RectF(x0, y, x1, y + 22), brText.Get());
    y += 28.0f;
    struct IntOpt { int hit; int mins; const wchar_t* lbl; };
    const IntOpt ints[] = {
        {HIT_PLAYLIST_OFF,    0,  L"Выкл"},
        {HIT_PLAYLIST_INT_1,  1,  L"1 мин"},
        {HIT_PLAYLIST_INT_5,  5,  L"5 мин"},
        {HIT_PLAYLIST_INT_15, 15, L"15 мин"},
        {HIT_PLAYLIST_INT_30, 30, L"30 мин"},
        {HIT_PLAYLIST_INT_60, 60, L"60 мин"},
    };
    float ix = x0;
    for (const auto& io : ints) {
        D2D1_RECT_F pr = D2D1::RectF(ix, y, ix + 82, y + 34);
        hitZones_.push_back({io.hit, pr, -1});
        bool act = (config_->playlistIntervalMin == io.mins);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{pr, 7.0f, 7.0f}, act ? brAccent.Get() : brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{pr, 7.0f, 7.0f}, act ? brAccent.Get() : brBorder.Get(), 1.0f);
        fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(io.lbl, static_cast<UINT32>(wcslen(io.lbl)), fmtBodyBold_.Get(), pr, brText.Get());
        ix += 90.0f;
    }
    D2D1_RECT_F nxtBtn = D2D1::RectF(ix + 6, y, std::min(x1, ix + 195), y + 34);
    hitZones_.push_back({HIT_NEXT_WALLPAPER, nxtBtn, -1});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{nxtBtn, 7.0f, 7.0f}, brAccentSoft.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{nxtBtn, 7.0f, 7.0f}, brAccent.Get(), 1.0f);
    rt_->DrawTextW(L"Следующие ⏭", 11, fmtBodyBold_.Get(), nxtBtn, brAccent.Get());
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    y += 48.0f;

    drawToggleCard(HIT_TOGGLE_SHUFFLE, L"Случайный порядок (Shuffle)",
                   L"Выбирать следующую сцену случайно вместо последовательного обхода", config_->playlistShuffle);

    wchar_t cfTxt[64];
    swprintf_s(cfTxt, L"Длительность плавного GPU Crossfade-перехода: %.1f сек", config_->crossfadeSeconds);
    rt_->DrawTextW(cfTxt, static_cast<UINT32>(wcslen(cfTxt)), fmtBodyBold_.Get(), D2D1::RectF(x0, y, x1, y + 20), brText.Get());
    y += 24.0f;
    D2D1_RECT_F cfHit = D2D1::RectF(x0, y - 4, x0 + 420, y + 14);
    sliderZones_.push_back({SLIDER_CROSSFADE, cfHit, 0.0f, 2.5f, &config_->crossfadeSeconds});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0, y + 3, x0 + 420, y + 7), 2.0f, 2.0f}, brTrack.Get());
    float cfKnob = x0 + (std::clamp(config_->crossfadeSeconds, 0.0f, 2.5f) / 2.5f) * 420.0f;
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0, y + 3, cfKnob, y + 7), 2.0f, 2.0f}, brAccent.Get());
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cfKnob, y + 5), 6.5f, 6.5f), brText.Get());
    y += 32.0f;

    drawToggleCard(HIT_TOGGLE_DAY_NIGHT, L"Авто-расписание «День / Ночь» по часам",
                   L"Утром включает светлые/дневные обои, а вечером — спокойные ночные", config_->dayNightEnabled);

    auto drawScheduleRow = [&](bool isDay, int decHit, int incHit, int assignHit, int hourVal, const std::wstring& pathVal) {
        D2D1_RECT_F row = D2D1::RectF(x0, y, x1, y + 54);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{row, 9.0f, 9.0f}, brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{row, 9.0f, 9.0f}, brBorder.Get(), 1.0f);

        wchar_t lbl[128];
        std::wstring name = pathVal.empty() ? L"(не выбрано)" : GetFileNameWithoutExt(pathVal);
        swprintf_s(lbl, L"%s с %02d:00  —  %s", isDay ? L"☀ День" : L"🌙 Ночь", hourVal, name.c_str());
        rt_->DrawTextW(lbl, static_cast<UINT32>(wcslen(lbl)), fmtBodyBold_.Get(),
                       D2D1::RectF(row.left + 16, row.top + 16, row.right - 290, row.bottom - 12), brText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        D2D1_RECT_F bDec = D2D1::RectF(row.right - 280, row.top + 11, row.right - 246, row.bottom - 11);
        D2D1_RECT_F bInc = D2D1::RectF(row.right - 240, row.top + 11, row.right - 206, row.bottom - 11);
        D2D1_RECT_F bSet = D2D1::RectF(row.right - 196, row.top + 11, row.right - 14,  row.bottom - 11);
        hitZones_.push_back({decHit, bDec, -1});
        hitZones_.push_back({incHit, bInc, -1});
        hitZones_.push_back({assignHit, bSet, -1});

        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{bDec, 6.0f, 6.0f}, brTrack.Get());
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{bInc, 6.0f, 6.0f}, brTrack.Get());
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{bSet, 6.0f, 6.0f}, brAccentSoft.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{bSet, 6.0f, 6.0f}, brAccent.Get(), 1.0f);

        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(L"-1ч", 3, fmtSmall_.Get(), bDec, brText.Get());
        rt_->DrawTextW(L"+1ч", 3, fmtSmall_.Get(), bInc, brText.Get());
        rt_->DrawTextW(L"Назначить выбранные", 19, fmtSmall_.Get(), bSet, brAccent.Get());
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        y += 64.0f;
    };

    drawScheduleRow(true,  HIT_DAY_HOUR_DEC,   HIT_DAY_HOUR_INC,   HIT_SET_DAY_WALL,   config_->dayStartHour,   config_->dayWallpaperPath);
    drawScheduleRow(false, HIT_NIGHT_HOUR_DEC, HIT_NIGHT_HOUR_INC, HIT_SET_NIGHT_WALL, config_->nightStartHour, config_->nightWallpaperPath);
}

void StudioWindow::RenderAudioWidgetsTab(float left, float right, float, D2D1_COLOR_F accent) {
    if (!config_) return;
    ComPtr<ID2D1SolidColorBrush> brText, brSub, brCard, brBorder, brAccent, brAccentSoft, brTrack;
    rt_->CreateSolidColorBrush(Rgba(0xF8FAFC), brText.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x94A3B8), brSub.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x131B2C), brCard.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x23314D), brBorder.GetAddressOf());
    rt_->CreateSolidColorBrush(accent, brAccent.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(accent.r, accent.g, accent.b, 0.22f), brAccentSoft.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x25334D), brTrack.GetAddressOf());

    const float x0 = left + 28.0f;
    const float maxW = std::min(780.0f, (right - left) - 56.0f);
    const float x1 = x0 + maxW;

    rt_->DrawTextW(L"Аудиовизуализация WASAPI (#1) и Виджеты рабочего стола (#5)", 59, fmtTitle_.Get(),
                   D2D1::RectF(x0, 20, x1, 48), brText.Get());
    rt_->DrawTextW(L"Захват системного звука Windows (Spotify, YouTube, игры) в реальном времени без микрофона.", 90, fmtBody_.Get(),
                   D2D1::RectF(x0, 50, x1, 74), brSub.Get());

    float y = 86.0f;

    rt_->DrawTextW(L"Режим эквалайзера на обоях:", 27, fmtSection_.Get(), D2D1::RectF(x0, y, x1, y + 22), brText.Get());
    y += 28.0f;

    struct VisOpt { int hit; AudioVisMode m; const wchar_t* lbl; };
    const VisOpt vopts[] = {
        {HIT_VIS_OFF,  AudioVisMode::Off,           L"Выключен"},
        {HIT_VIS_BARS, AudioVisMode::BarsBottom,    L"Столбцы (32)"},
        {HIT_VIS_WAVE, AudioVisMode::SymmetricWave, L"Неон-Волна"},
        {HIT_VIS_RING, AudioVisMode::NeonRing,      L"Неон-Кольцо"},
    };
    float vx = x0;
    for (const auto& vo : vopts) {
        D2D1_RECT_F pr = D2D1::RectF(vx, y, vx + 132, y + 36);
        hitZones_.push_back({vo.hit, pr, -1});
        bool act = (config_->audioVisMode == vo.m);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{pr, 8.0f, 8.0f}, act ? brAccent.Get() : brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{pr, 8.0f, 8.0f}, act ? brAccent.Get() : brBorder.Get(), 1.0f);
        fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(vo.lbl, static_cast<UINT32>(wcslen(vo.lbl)), fmtBodyBold_.Get(), pr, brText.Get());
        vx += 142.0f;
    }
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    if (vx + 150.0f <= x1) {
        D2D1_RECT_F meterRc = D2D1::RectF(vx + 6, y, x1, y + 36);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{meterRc, 8.0f, 8.0f}, brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{meterRc, 8.0f, 8.0f}, brBorder.Get(), 1.0f);
        float mw = (meterRc.right - meterRc.left - 16.0f) / 16.0f;
        for (int b = 0; b < 16; ++b) {
            float val = std::max(0.08f, status_.audio.bands[static_cast<size_t>(b * 2)]);
            float bh = val * 24.0f;
            float bx = meterRc.left + 8.0f + b * mw;
            rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(bx, meterRc.bottom - 5.0f - bh, bx + mw * 0.68f, meterRc.bottom - 5.0f), 2.0f, 2.0f}, brAccent.Get());
        }
    }
    y += 48.0f;

    wchar_t sensTxt[64];
    swprintf_s(sensTxt, L"Чувствительность аудиоспектра: %.0f%%", config_->audioSensitivity * 100.0f);
    rt_->DrawTextW(sensTxt, static_cast<UINT32>(wcslen(sensTxt)), fmtBodyBold_.Get(), D2D1::RectF(x0, y, x1, y + 20), brText.Get());
    y += 22.0f;
    D2D1_RECT_F sHit = D2D1::RectF(x0, y - 4, x0 + 380, y + 14);
    sliderZones_.push_back({SLIDER_AUDIO_SENS, sHit, 0.25f, 3.0f, &config_->audioSensitivity});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0, y + 3, x0 + 380, y + 7), 2.0f, 2.0f}, brTrack.Get());
    float sKnob = x0 + ((std::clamp(config_->audioSensitivity, 0.25f, 3.0f) - 0.25f) / 2.75f) * 380.0f;
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0, y + 3, sKnob, y + 7), 2.0f, 2.0f}, brAccent.Get());
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sKnob, y + 5), 6.0f, 6.0f), brText.Get());
    y += 26.0f;

    auto drawSwitchRow = [&](int hit, const wchar_t* title, const wchar_t* sub, bool on) {
        D2D1_RECT_F r = D2D1::RectF(x0, y, x1, y + 52);
        hitZones_.push_back({hit, r, -1});
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{r, 9.0f, 9.0f}, brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{r, 9.0f, 9.0f}, on ? brAccent.Get() : brBorder.Get(), 1.0f);
        rt_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), fmtBodyBold_.Get(), D2D1::RectF(r.left + 16, r.top + 8, r.right - 80, r.top + 28), brText.Get());
        rt_->DrawTextW(sub, static_cast<UINT32>(wcslen(sub)), fmtSmall_.Get(), D2D1::RectF(r.left + 16, r.top + 28, r.right - 80, r.bottom - 6), brSub.Get());

        D2D1_RECT_F sw = D2D1::RectF(r.right - 58, r.top + 14, r.right - 16, r.top + 38);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{sw, 12.0f, 12.0f}, on ? brAccent.Get() : brBorder.Get());
        rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(on ? sw.right - 12 : sw.left + 12, sw.top + 12), 9.0f, 9.0f), brText.Get());
        y += 60.0f;
    };

    drawSwitchRow(HIT_TOGGLE_BASS_ZOOM, L"Бас-реактивный пульс обоев (Bass Zoom & Glow)",
                  L"Лёгкое приближение и вспышка свечения в такт низким частотам музыки", config_->bassReactiveZoom);

    rt_->DrawTextW(L"Виджеты на слое обоев (позади значков рабочего стола):", 54, fmtSection_.Get(),
                   D2D1::RectF(x0, y + 4, x1, y + 26), brText.Get());
    y += 32.0f;

    drawSwitchRow(HIT_TOGGLE_WIDGET_CLOCK, L"Часы и дата на русском языке",
                  L"Стильная стеклянная карточка с крупными часами и днём недели", config_->widgetClock);

    D2D1_RECT_F b24 = D2D1::RectF(x0, y, x0 + 180, y + 30);
    D2D1_RECT_F bSec = D2D1::RectF(x0 + 190, y, x0 + 390, y + 30);
    hitZones_.push_back({HIT_TOGGLE_CLOCK_24H, b24, -1});
    hitZones_.push_back({HIT_TOGGLE_CLOCK_SEC, bSec, -1});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{b24, 6.0f, 6.0f}, config_->clock24h ? brAccentSoft.Get() : brCard.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{b24, 6.0f, 6.0f}, config_->clock24h ? brAccent.Get() : brBorder.Get(), 1.0f);
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{bSec, 6.0f, 6.0f}, config_->clockShowSeconds ? brAccentSoft.Get() : brCard.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{bSec, 6.0f, 6.0f}, config_->clockShowSeconds ? brAccent.Get() : brBorder.Get(), 1.0f);
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawTextW(config_->clock24h ? L"Формат: 24 часа" : L"Формат: 12 часов AM/PM", config_->clock24h ? 15 : 22, fmtSmall_.Get(), b24, brText.Get());
    rt_->DrawTextW(config_->clockShowSeconds ? L"✓ Секунды ВКЛ" : L"Секунды ВЫКЛ", config_->clockShowSeconds ? 13 : 12, fmtSmall_.Get(), bSec, brText.Get());
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    y += 40.0f;

    drawSwitchRow(HIT_TOGGLE_WIDGET_MEDIA, L"Виджет «Сейчас играет» (Now Playing)",
                  L"Автоматически отображает название трека из активного плеера или браузера", config_->widgetMedia);
    drawSwitchRow(HIT_TOGGLE_WIDGET_SYS, L"Системный монитор ЦП (CPU) и ОЗУ (RAM)",
                  L"Компактные индикаторы загрузки процессора и оперативной памяти", config_->widgetSysStats);

    wchar_t opTxt[64];
    swprintf_s(opTxt, L"Непрозрачность фона виджетов: %.0f%%", config_->widgetOpacity * 100.0f);
    rt_->DrawTextW(opTxt, static_cast<UINT32>(wcslen(opTxt)), fmtSmall_.Get(), D2D1::RectF(x0, y, x0 + 300, y + 18), brSub.Get());
    D2D1_RECT_F opHit = D2D1::RectF(x0 + 310, y - 2, x0 + 580, y + 18);
    sliderZones_.push_back({SLIDER_WIDGET_OPACITY, opHit, 0.20f, 0.95f, &config_->widgetOpacity});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0 + 310, y + 6, x0 + 580, y + 10), 2.0f, 2.0f}, brTrack.Get());
    float opKnob = (x0 + 310) + ((std::clamp(config_->widgetOpacity, 0.20f, 0.95f) - 0.20f) / 0.75f) * 270.0f;
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(x0 + 310, y + 6, opKnob, y + 10), 2.0f, 2.0f}, brAccent.Get());
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(opKnob, y + 8), 6.0f, 6.0f), brText.Get());
}

void StudioWindow::RenderMonitorsTaskbarTab(float left, float right, float, D2D1_COLOR_F accent) {
    if (!config_) return;
    ComPtr<ID2D1SolidColorBrush> brText, brSub, brCard, brBorder, brAccent, brAccentSoft;
    rt_->CreateSolidColorBrush(Rgba(0xF8FAFC), brText.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x94A3B8), brSub.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x131B2C), brCard.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x23314D), brBorder.GetAddressOf());
    rt_->CreateSolidColorBrush(accent, brAccent.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(accent.r, accent.g, accent.b, 0.20f), brAccentSoft.GetAddressOf());

    const float x0 = left + 28.0f;
    const float maxW = std::min(800.0f, (right - left) - 56.0f);
    const float x1 = x0 + maxW;

    rt_->DrawTextW(L"Мульти-монитор (#4), Прозрачная панель задач & Авто-акцент (#8)", 63, fmtTitle_.Get(),
                   D2D1::RectF(x0, 20, x1, 48), brText.Get());

    float y = 62.0f;
    struct MonModePill { int hit; MonitorMode m; const wchar_t* title; };
    const MonModePill mmodes[] = {
        {HIT_MON_PRIMARY,     MonitorMode::PrimaryOnly,           L"Основной"},
        {HIT_MON_CLONE,       MonitorMode::AllSame,               L"Клон на все"},
        {HIT_MON_SPAN,        MonitorMode::SpanAcross,            L"Панорама (Span)"},
        {HIT_MON_SINGLE,      MonitorMode::SpecificMonitor,       L"Один экран"},
        {HIT_MON_INDEPENDENT, MonitorMode::PerMonitorIndependent, L"Разные обои (#4)"},
    };

    float mx = x0;
    float pillW = (maxW - 32.0f) / 5.0f;
    for (const auto& mp : mmodes) {
        D2D1_RECT_F pr = D2D1::RectF(mx, y, mx + pillW, y + 40);
        hitZones_.push_back({mp.hit, pr, -1});
        bool act = (config_->monitorMode == mp.m);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{pr, 8.0f, 8.0f}, act ? brAccent.Get() : brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{pr, 8.0f, 8.0f}, act ? brAccent.Get() : brBorder.Get(), 1.0f);
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(mp.title, static_cast<UINT32>(wcslen(mp.title)), fmtSmall_.Get(), pr, brText.Get());
        mx += pillW + 8.0f;
    }
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    y += 56.0f;

    rt_->DrawTextW(L"Обнаруженные дисплеи (назначение независимых обоев):", 52, fmtSection_.Get(),
                   D2D1::RectF(x0, y, x1, y + 22), brText.Get());
    y += 28.0f;

    DesktopHost hostHelper;
    auto mons = hostHelper.EnumerateMonitors();
    for (size_t i = 0; i < mons.size() && i < 4; ++i) {
        D2D1_RECT_F mr = D2D1::RectF(x0, y, x1, y + 54);
        hitZones_.push_back({HIT_MON_INDEX_BASE, D2D1::RectF(x0, y, x1 - 220, y + 54), static_cast<int>(i)});
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{mr, 9.0f, 9.0f}, brCard.Get());
        bool selMon = (config_->monitorMode == MonitorMode::SpecificMonitor && config_->targetMonitorIndex == static_cast<int>(i));
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{mr, 9.0f, 9.0f}, selMon ? brAccent.Get() : brBorder.Get(), 1.0f);

        std::wstring assignedPath = (i < config_->monitorWallpapers.size() && !config_->monitorWallpapers[i].empty())
            ? GetFileNameWithoutExt(config_->monitorWallpapers[i])
            : GetFileNameWithoutExt(config_->activeVideoPath);

        wchar_t line1[160], line2[196];
        swprintf_s(line1, L"Монитор #%zu %s — %ld×%ld",
                   i + 1, mons[i].isPrimary ? L"(Основной)" : L"",
                   mons[i].bounds.right - mons[i].bounds.left,
                   mons[i].bounds.bottom - mons[i].bounds.top);
        swprintf_s(line2, L"Назначено: %s", assignedPath.empty() ? L"По умолчанию" : assignedPath.c_str());

        rt_->DrawTextW(line1, static_cast<UINT32>(wcslen(line1)), fmtBodyBold_.Get(), D2D1::RectF(mr.left + 14, mr.top + 7, mr.right - 225, mr.top + 27), brText.Get());
        rt_->DrawTextW(line2, static_cast<UINT32>(wcslen(line2)), fmtSmall_.Get(), D2D1::RectF(mr.left + 14, mr.top + 28, mr.right - 225, mr.bottom - 6), brSub.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        D2D1_RECT_F asgnBtn = D2D1::RectF(mr.right - 214, mr.top + 11, mr.right - 14, mr.bottom - 11);
        hitZones_.push_back({HIT_MON_ASSIGN_CURRENT, asgnBtn, static_cast<int>(i)});
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{asgnBtn, 6.0f, 6.0f}, brAccentSoft.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{asgnBtn, 6.0f, 6.0f}, brAccent.Get(), 1.0f);
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(L"Поставить выбранные из Библиотеки", 33, fmtSmall_.Get(), asgnBtn, brAccent.Get());
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        y += 62.0f;
    }

    y += 14.0f;
    rt_->DrawTextW(L"Прозрачность панели задач Windows (#8):", 39, fmtSection_.Get(), D2D1::RectF(x0, y, x1, y + 22), brText.Get());
    y += 28.0f;

    struct TbOpt { int hit; TaskbarMode m; const wchar_t* lbl; };
    const TbOpt tbOpts[] = {
        {HIT_TASKBAR_DEFAULT, TaskbarMode::Default,          L"Стандартная Windows"},
        {HIT_TASKBAR_CLEAR,   TaskbarMode::ClearTransparent, L"Полностью прозрачная"},
        {HIT_TASKBAR_BLUR,    TaskbarMode::AcrylicBlur,      L"Акриловое стекло (Blur)"},
    };
    float tx = x0;
    float tbW = (maxW - 16.0f) / 3.0f;
    for (const auto& to : tbOpts) {
        D2D1_RECT_F tr = D2D1::RectF(tx, y, tx + tbW, y + 40);
        hitZones_.push_back({to.hit, tr, -1});
        bool act = (config_->taskbarMode == to.m);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{tr, 8.0f, 8.0f}, act ? brAccent.Get() : brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{tr, 8.0f, 8.0f}, act ? brAccent.Get() : brBorder.Get(), 1.0f);
        fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(to.lbl, static_cast<UINT32>(wcslen(to.lbl)), fmtBodyBold_.Get(), tr, brText.Get());
        tx += tbW + 8.0f;
    }
    fmtBodyBold_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    y += 56.0f;

    rt_->DrawTextW(L"Цветовой акцент Студии и Аудиовизуализатора (#8):", 49, fmtSection_.Get(), D2D1::RectF(x0, y, x1, y + 22), brText.Get());
    y += 28.0f;
    struct AcOpt { int hit; AccentTheme t; const wchar_t* lbl; uint32_t hex; };
    const AcOpt acOpts[] = {
        {HIT_ACCENT_AUTO,    AccentTheme::AutoFromWallpaper, L"Авто под обои", status_.activeAccentRGB ? status_.activeAccentRGB : 0x0EA5E9},
        {HIT_ACCENT_CYAN,    AccentTheme::NeonCyan,          L"Неон-Циан",     0x0EA5E9},
        {HIT_ACCENT_PURPLE,  AccentTheme::CyberViolet,       L"Кибер-Фиолет",  0xA855F7},
        {HIT_ACCENT_EMERALD, AccentTheme::MatrixEmerald,     L"Изумруд",       0x10B981},
        {HIT_ACCENT_AMBER,   AccentTheme::SunsetCrimson,     L"Алый закат",    0xF43F5E},
    };
    float ax = x0;
    float acW = (maxW - 32.0f) / 5.0f;
    for (const auto& ao : acOpts) {
        D2D1_RECT_F ar = D2D1::RectF(ax, y, ax + acW, y + 38);
        hitZones_.push_back({ao.hit, ar, -1});
        bool act = (config_->accentTheme == ao.t);
        ComPtr<ID2D1SolidColorBrush> swBr;
        rt_->CreateSolidColorBrush(Rgba(ao.hex), swBr.GetAddressOf());
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{ar, 8.0f, 8.0f}, act ? brAccentSoft.Get() : brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{ar, 8.0f, 8.0f}, act ? swBr.Get() : brBorder.Get(), act ? 2.0f : 1.0f);
        rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(ar.left + 14, ar.top + 19), 5.5f, 5.5f), swBr.Get());
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(ao.lbl, static_cast<UINT32>(wcslen(ao.lbl)), fmtSmall_.Get(), D2D1::RectF(ar.left + 25, ar.top, ar.right - 4, ar.bottom), brText.Get());
        ax += acW + 8.0f;
    }
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

void StudioWindow::RenderSystemHotkeysTab(float left, float right, float, D2D1_COLOR_F accent) {
    if (!config_) return;
    ComPtr<ID2D1SolidColorBrush> brText, brSub, brCard, brBorder, brAccent, brDanger;
    rt_->CreateSolidColorBrush(Rgba(0xF8FAFC), brText.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x94A3B8), brSub.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x131B2C), brCard.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0x23314D), brBorder.GetAddressOf());
    rt_->CreateSolidColorBrush(accent, brAccent.GetAddressOf());
    rt_->CreateSolidColorBrush(Rgba(0xEF4444, 0.22f), brDanger.GetAddressOf());

    const float x0 = left + 28.0f;
    const float maxW = std::min(780.0f, (right - left) - 56.0f);
    const float x1 = x0 + maxW;

    rt_->DrawTextW(L"Умное энергосбережение, Windows и Глобальные хоткеи (#9)", 56, fmtTitle_.Get(),
                   D2D1::RectF(x0, 18, x1, 46), brText.Get());

    float y = 56.0f;
    auto drawCompactToggle = [&](int hit, const wchar_t* title, bool on) {
        D2D1_RECT_F r = D2D1::RectF(x0, y, x1, y + 40);
        hitZones_.push_back({hit, r, -1});
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{r, 8.0f, 8.0f}, brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{r, 8.0f, 8.0f}, on ? brAccent.Get() : brBorder.Get(), 1.0f);
        fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), fmtBodyBold_.Get(), D2D1::RectF(r.left + 14, r.top, r.right - 72, r.bottom), brText.Get());
        fmtBodyBold_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        D2D1_RECT_F sw = D2D1::RectF(r.right - 56, r.top + 8, r.right - 14, r.top + 32);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{sw, 12.0f, 12.0f}, on ? brAccent.Get() : brBorder.Get());
        rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(on ? sw.right - 12 : sw.left + 12, sw.top + 12), 9.0f, 9.0f), brText.Get());
        y += 46.0f;
    };

    drawCompactToggle(HIT_TOGGLE_AUTOSTART,    L"Запускать вместе с Windows (автозагрузка HKCU)", config_->startWithWindows);
    drawCompactToggle(HIT_TOGGLE_MINIMIZED,    L"Скрывать в системный трей при старте ОС", config_->startMinimized);
    drawCompactToggle(HIT_TOGGLE_PAUSE_FS,     L"Умная пауза (0% GPU/CPU) во время полноэкранных игр и окон", config_->pauseOnFullscreen);
    drawCompactToggle(HIT_TOGGLE_PAUSE_BAT,    L"Пауза на ноутбуке при работе от батареи / экономии заряда", config_->pauseOnBattery);
    drawCompactToggle(HIT_TOGGLE_MUTE_UNFOCUS, L"Приглушать звук обоев, когда рабочий стол перекрыт окнами", config_->muteWhenUnfocused);
    drawCompactToggle(HIT_TOGGLE_CTX_MENU,     L"Пункт «Поставить на рабочий стол» в меню Проводника (.mp4/.gif/.html)", config_->explorerContextMenu);
    drawCompactToggle(HIT_TOGGLE_HOTKEYS,      L"Глобальные горячие клавиши в любой программе (#9)", config_->globalHotkeys);

    D2D1_RECT_F hkCard = D2D1::RectF(x0, y + 4, x1, y + 66);
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{hkCard, 8.0f, 8.0f}, brCard.Get());
    rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{hkCard, 8.0f, 8.0f}, brBorder.Get(), 1.0f);
    const wchar_t* hkLine1 = L"⌨  Ctrl + Alt + P — Пауза / Пуск     |     Ctrl + Alt + N — Следующие обои";
    const wchar_t* hkLine2 = L"⌨  Ctrl + Alt + M — Звук Вкл/Выкл   |     Ctrl + Alt + W — Открыть Студию";
    rt_->DrawTextW(hkLine1, static_cast<UINT32>(wcslen(hkLine1)), fmtSmall_.Get(), D2D1::RectF(hkCard.left + 14, hkCard.top + 10, hkCard.right - 14, hkCard.top + 32), brAccent.Get());
    rt_->DrawTextW(hkLine2, static_cast<UINT32>(wcslen(hkLine2)), fmtSmall_.Get(), D2D1::RectF(hkCard.left + 14, hkCard.top + 35, hkCard.right - 14, hkCard.bottom - 6), brSub.Get());
    y += 78.0f;

    rt_->DrawTextW(L"Лимит частоты кадров:", 21, fmtBodyBold_.Get(), D2D1::RectF(x0, y + 6, x0 + 185, y + 32), brText.Get());
    struct FpsOpt { int hit; uint32_t fps; const wchar_t* lbl; };
    const FpsOpt fpsOpts[] = {
        {HIT_FPS_30,  30,  L"30 FPS"},
        {HIT_FPS_60,  60,  L"60 FPS"},
        {HIT_FPS_120, 120, L"120 FPS"},
        {HIT_FPS_144, 144, L"144 FPS"},
    };
    float fx = x0 + 190.0f;
    for (const auto& fo : fpsOpts) {
        D2D1_RECT_F pr = D2D1::RectF(fx, y, fx + 84, y + 32);
        hitZones_.push_back({fo.hit, pr, -1});
        bool act = (config_->targetFps == fo.fps);
        rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{pr, 7.0f, 7.0f}, act ? brAccent.Get() : brCard.Get());
        rt_->DrawRoundedRectangle(D2D1_ROUNDED_RECT{pr, 7.0f, 7.0f}, act ? brAccent.Get() : brBorder.Get(), 1.0f);
        fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawTextW(fo.lbl, static_cast<UINT32>(wcslen(fo.lbl)), fmtSmall_.Get(), pr, brText.Get());
        fx += 92.0f;
    }

    D2D1_RECT_F exitRc = D2D1::RectF(x1 - 180, y, x1, y + 32);
    hitZones_.push_back({HIT_EXIT_APP, exitRc, -1});
    rt_->FillRoundedRectangle(D2D1_ROUNDED_RECT{exitRc, 7.0f, 7.0f}, brDanger.Get());
    rt_->DrawTextW(L"Выход из WallPulse", 18, fmtSmall_.Get(), exitRc, brText.Get());
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

void StudioWindow::OpenAddFileDialog() {
    wchar_t fileBuf[4096] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter =
        L"Все поддерживаемые обои (*.mp4;*.webm;*.mkv;*.gif;*.html;*.hlsl)\0*.mp4;*.webm;*.mkv;*.mov;*.avi;*.wmv;*.m4v;*.gif;*.html;*.htm;*.hlsl\0"
        L"Видеообои (*.mp4;*.webm;*.mkv;*.mov;*.avi)\0*.mp4;*.webm;*.mkv;*.mov;*.avi;*.wmv;*.m4v\0"
        L"Анимации GIF, HTML5 и Шейдеры (*.gif;*.html;*.hlsl)\0*.gif;*.html;*.htm;*.hlsl\0"
        L"Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
    ofn.lpstrTitle = L"Выберите живые обои (.mp4, .gif, .html, .hlsl)";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&ofn)) return;

    std::vector<std::wstring> selected;
    const wchar_t* p = fileBuf;
    std::wstring first = p;
    p += first.size() + 1;
    if (*p == L'\0') {
        selected.push_back(first);
    } else {
        while (*p != L'\0') {
            std::wstring name = p;
            selected.push_back(first + L"\\" + name);
            p += name.size() + 1;
        }
    }

    if (!selected.empty() && callbacks_.onAddFiles) {
        callbacks_.onAddFiles(selected);
    }
}

void StudioWindow::UpdateSliderFromX(const SliderZone& s, float dipX) {
    if (!s.target) return;
    float w = std::max(1.0f, s.rect.right - s.rect.left);
    float norm = std::clamp((dipX - s.rect.left) / w, 0.0f, 1.0f);
    *s.target = s.minVal + norm * (s.maxVal - s.minVal);
    if (callbacks_.onSettingsChanged) callbacks_.onSettingsChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::OnMouseDown(float dipX, float dipY) {
    SetFocus(hwnd_);

    for (const auto& s : sliderZones_) {
        if (PtInRectF(s.rect, dipX, dipY)) {
            activeSliderId_ = s.id;
            SetCapture(hwnd_);
            UpdateSliderFromX(s, dipX);
            return;
        }
    }

    bool clickedSearch = false;
    for (const auto& h : hitZones_) {
        if (PtInRectF(h.rect, dipX, dipY)) {
            if (h.id == HIT_SEARCH_BOX) {
                clickedSearch = true;
                searchFocused_ = true;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
            if (h.id == HIT_TIMELINE_SCRUB) {
                scrubbingTimeline_ = true;
                SetCapture(hwnd_);
                float norm = std::clamp((dipX - timelineRect_.left) / std::max(1.0f, timelineRect_.right - timelineRect_.left), 0.0f, 1.0f);
                if (callbacks_.onSeekVideo && status_.durationSec > 0.1) {
                    callbacks_.onSeekVideo(norm * status_.durationSec);
                }
                return;
            }
            searchFocused_ = false;
            HandleButtonClick(h.id, h.indexParam);
            return;
        }
    }

    if (!clickedSearch && searchFocused_) {
        searchFocused_ = false;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void StudioWindow::OnMouseMove(float dipX, float dipY, bool leftDown) {
    if (scrubbingTimeline_ && leftDown) {
        float norm = std::clamp((dipX - timelineRect_.left) / std::max(1.0f, timelineRect_.right - timelineRect_.left), 0.0f, 1.0f);
        if (callbacks_.onSeekVideo && status_.durationSec > 0.1) {
            callbacks_.onSeekVideo(norm * status_.durationSec);
        }
        return;
    }

    if (activeSliderId_ != 0 && leftDown) {
        for (const auto& s : sliderZones_) {
            if (s.id == activeSliderId_) {
                UpdateSliderFromX(s, dipX);
                return;
            }
        }
    }

    int newHover = 0;
    int newCard = -1;
    for (const auto& h : hitZones_) {
        if (PtInRectF(h.rect, dipX, dipY)) {
            newHover = h.id;
            if (h.id == HIT_CARD_SELECT) newCard = h.indexParam;
            break;
        }
    }
    if (newHover != hoveredHitId_ || newCard != hoveredCardIdx_) {
        hoveredHitId_ = newHover;
        hoveredCardIdx_ = newCard;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void StudioWindow::OnMouseUp() {
    if (activeSliderId_ != 0 || scrubbingTimeline_) {
        activeSliderId_ = 0;
        scrubbingTimeline_ = false;
        ReleaseCapture();
    }
}

void StudioWindow::OnMouseWheel(short delta) {
    if (activeTab_ != Tab::Library) return;
    float step = (static_cast<float>(delta) / 120.0f) * 68.0f;
    libraryScrollY_ = std::clamp(libraryScrollY_ - step, 0.0f, maxLibraryScrollY_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::OnCharInput(wchar_t ch) {
    if (!searchFocused_ || activeTab_ != Tab::Library) return;
    if (ch == L'\b') {
        if (!searchQuery_.empty()) searchQuery_.pop_back();
    } else if (ch == 27) {
        searchQuery_.clear();
        searchFocused_ = false;
    } else if (ch >= 32 && searchQuery_.size() < 42) {
        searchQuery_.push_back(ch);
    }
    libraryScrollY_ = 0.0f;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::HandleButtonClick(int id, int indexParam) {
    if (!config_) return;
    bool notifySettings = false;

    switch (id) {
        case HIT_NAV_LIBRARY:       activeTab_ = Tab::Library; break;
        case HIT_NAV_PLAYLIST:      activeTab_ = Tab::Playlist; break;
        case HIT_NAV_AUDIO_WIDGETS: activeTab_ = Tab::AudioWidgets; break;
        case HIT_NAV_MONITORS:      activeTab_ = Tab::MonitorsTaskbar; break;
        case HIT_NAV_SYSTEM:        activeTab_ = Tab::SystemHotkeys; break;

        case HIT_SEARCH_CLEAR:      searchQuery_.clear(); libraryScrollY_ = 0.0f; break;
        case HIT_FILTER_ALL:        libFilter_ = LibFilter::All; libraryScrollY_ = 0.0f; break;
        case HIT_FILTER_FAV:        libFilter_ = LibFilter::Favorites; libraryScrollY_ = 0.0f; break;
        case HIT_FILTER_VIDEO:      libFilter_ = LibFilter::Videos; libraryScrollY_ = 0.0f; break;
        case HIT_FILTER_GIFWEB:     libFilter_ = LibFilter::GifWeb; libraryScrollY_ = 0.0f; break;
        case HIT_FILTER_SHADER:     libFilter_ = LibFilter::Shaders; libraryScrollY_ = 0.0f; break;
        case HIT_ADD_FILE:          OpenAddFileDialog(); break;

        case HIT_CARD_STAR: {
            if (library_ && indexParam >= 0 && indexParam < static_cast<int>(library_->size())) {
                (*library_)[static_cast<size_t>(indexParam)].favorite = !(*library_)[static_cast<size_t>(indexParam)].favorite;
                notifySettings = true;
            }
            break;
        }
        case HIT_CARD_SELECT: {
            if (library_ && indexParam >= 0 && indexParam < static_cast<int>(library_->size())) {
                if (selectedIndex_ == indexParam && callbacks_.onApplyWallpaper) {
                    callbacks_.onApplyWallpaper((*library_)[static_cast<size_t>(indexParam)].path);
                } else {
                    selectedIndex_ = indexParam;
                }
            }
            break;
        }
        case HIT_TOGGLE_FAVORITE: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size())) {
                (*library_)[static_cast<size_t>(selectedIndex_)].favorite = !(*library_)[static_cast<size_t>(selectedIndex_)].favorite;
                notifySettings = true;
            }
            break;
        }
        case HIT_APPLY_SELECTED: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size()) && callbacks_.onApplyWallpaper) {
                callbacks_.onApplyWallpaper((*library_)[static_cast<size_t>(selectedIndex_)].path);
            }
            break;
        }
        case HIT_TOGGLE_PAUSE:   if (callbacks_.onTogglePause) callbacks_.onTogglePause(); break;
        case HIT_NEXT_WALLPAPER: if (callbacks_.onNextWallpaper) callbacks_.onNextWallpaper(); break;
        case HIT_STOP_WALLPAPER: if (callbacks_.onStopWallpaper) callbacks_.onStopWallpaper(); break;
        case HIT_REMOVE_SELECTED: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size()) && callbacks_.onRemoveFile) {
                std::wstring p = (*library_)[static_cast<size_t>(selectedIndex_)].path;
                callbacks_.onRemoveFile(p);
                if (selectedIndex_ >= static_cast<int>(library_->size())) {
                    selectedIndex_ = std::max(0, static_cast<int>(library_->size()) - 1);
                }
            }
            break;
        }

        case HIT_INSP_TAB_BASIC:   inspectorTab_ = InspectorTab::BasicTrim; break;
        case HIT_INSP_TAB_SHADERS: inspectorTab_ = InspectorTab::ProShaders; break;

        case HIT_SCALE_FILL:
        case HIT_SCALE_FIT:
        case HIT_SCALE_STRETCH:
        case HIT_SCALE_CENTER: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size())) {
                auto& p = (*library_)[static_cast<size_t>(selectedIndex_)].profile;
                if (id == HIT_SCALE_FILL) p.scaleMode = ScaleMode::Cover;
                else if (id == HIT_SCALE_FIT) p.scaleMode = ScaleMode::FitBars;
                else if (id == HIT_SCALE_STRETCH) p.scaleMode = ScaleMode::Stretch;
                else p.scaleMode = ScaleMode::Center;
                notifySettings = true;
            }
            break;
        }
        case HIT_TOGGLE_MUTE: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size())) {
                auto& p = (*library_)[static_cast<size_t>(selectedIndex_)].profile;
                p.muted = !p.muted;
                notifySettings = true;
            }
            break;
        }
        case HIT_TOGGLE_SEAMLESS_LOOP: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size())) {
                auto& p = (*library_)[static_cast<size_t>(selectedIndex_)].profile;
                p.seamlessLoop = !p.seamlessLoop;
                notifySettings = true;
            }
            break;
        }
        case HIT_RESET_PROFILE_FX: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size())) {
                (*library_)[static_cast<size_t>(selectedIndex_)].profile.ResetVisuals();
                notifySettings = true;
            }
            break;
        }

        // Playlist & Day/Night (#3)
        case HIT_PLAYLIST_OFF:    config_->playlistIntervalMin = 0;  notifySettings = true; break;
        case HIT_PLAYLIST_INT_1:  config_->playlistIntervalMin = 1;  notifySettings = true; break;
        case HIT_PLAYLIST_INT_5:  config_->playlistIntervalMin = 5;  notifySettings = true; break;
        case HIT_PLAYLIST_INT_15: config_->playlistIntervalMin = 15; notifySettings = true; break;
        case HIT_PLAYLIST_INT_30: config_->playlistIntervalMin = 30; notifySettings = true; break;
        case HIT_PLAYLIST_INT_60: config_->playlistIntervalMin = 60; notifySettings = true; break;
        case HIT_TOGGLE_SHUFFLE:  config_->playlistShuffle = !config_->playlistShuffle; notifySettings = true; break;
        case HIT_TOGGLE_DAY_NIGHT: config_->dayNightEnabled = !config_->dayNightEnabled; notifySettings = true; break;
        case HIT_DAY_HOUR_DEC:    config_->dayStartHour = (config_->dayStartHour + 23) % 24; notifySettings = true; break;
        case HIT_DAY_HOUR_INC:    config_->dayStartHour = (config_->dayStartHour + 1) % 24; notifySettings = true; break;
        case HIT_NIGHT_HOUR_DEC:  config_->nightStartHour = (config_->nightStartHour + 23) % 24; notifySettings = true; break;
        case HIT_NIGHT_HOUR_INC:  config_->nightStartHour = (config_->nightStartHour + 1) % 24; notifySettings = true; break;
        case HIT_SET_DAY_WALL: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size())) {
                config_->dayWallpaperPath = (*library_)[static_cast<size_t>(selectedIndex_)].path;
                notifySettings = true;
            }
            break;
        }
        case HIT_SET_NIGHT_WALL: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size())) {
                config_->nightWallpaperPath = (*library_)[static_cast<size_t>(selectedIndex_)].path;
                notifySettings = true;
            }
            break;
        }

        // Audio Visualizer (#1) & Desktop Widgets (#5)
        case HIT_VIS_OFF:  config_->audioVisMode = AudioVisMode::Off;           notifySettings = true; break;
        case HIT_VIS_BARS: config_->audioVisMode = AudioVisMode::BarsBottom;    notifySettings = true; break;
        case HIT_VIS_WAVE: config_->audioVisMode = AudioVisMode::SymmetricWave; notifySettings = true; break;
        case HIT_VIS_RING: config_->audioVisMode = AudioVisMode::NeonRing;      notifySettings = true; break;
        case HIT_TOGGLE_BASS_ZOOM:    config_->bassReactiveZoom = !config_->bassReactiveZoom; notifySettings = true; break;
        case HIT_TOGGLE_WIDGET_CLOCK: config_->widgetClock = !config_->widgetClock; notifySettings = true; break;
        case HIT_TOGGLE_CLOCK_24H:    config_->clock24h = !config_->clock24h; notifySettings = true; break;
        case HIT_TOGGLE_CLOCK_SEC:    config_->clockShowSeconds = !config_->clockShowSeconds; notifySettings = true; break;
        case HIT_TOGGLE_WIDGET_MEDIA: config_->widgetMedia = !config_->widgetMedia; notifySettings = true; break;
        case HIT_TOGGLE_WIDGET_SYS:   config_->widgetSysStats = !config_->widgetSysStats; notifySettings = true; break;

        // Monitors (#4) & Taskbar / Accent (#8)
        case HIT_MON_PRIMARY:     config_->monitorMode = MonitorMode::PrimaryOnly; notifySettings = true; break;
        case HIT_MON_CLONE:       config_->monitorMode = MonitorMode::AllSame; notifySettings = true; break;
        case HIT_MON_SPAN:        config_->monitorMode = MonitorMode::SpanAcross; notifySettings = true; break;
        case HIT_MON_SINGLE:      config_->monitorMode = MonitorMode::SpecificMonitor; notifySettings = true; break;
        case HIT_MON_INDEPENDENT: config_->monitorMode = MonitorMode::PerMonitorIndependent; notifySettings = true; break;
        case HIT_MON_INDEX_BASE: {
            config_->monitorMode = MonitorMode::SpecificMonitor;
            config_->targetMonitorIndex = std::max(0, indexParam);
            notifySettings = true;
            break;
        }
        case HIT_MON_ASSIGN_CURRENT: {
            if (library_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(library_->size()) && indexParam >= 0) {
                size_t mIdx = static_cast<size_t>(indexParam);
                if (config_->monitorWallpapers.size() <= mIdx) {
                    config_->monitorWallpapers.resize(mIdx + 1);
                }
                config_->monitorWallpapers[mIdx] = (*library_)[static_cast<size_t>(selectedIndex_)].path;
                config_->monitorMode = MonitorMode::PerMonitorIndependent;
                notifySettings = true;
            }
            break;
        }
        case HIT_TASKBAR_DEFAULT: config_->taskbarMode = TaskbarMode::Default; notifySettings = true; break;
        case HIT_TASKBAR_CLEAR:   config_->taskbarMode = TaskbarMode::ClearTransparent; notifySettings = true; break;
        case HIT_TASKBAR_BLUR:    config_->taskbarMode = TaskbarMode::AcrylicBlur; notifySettings = true; break;
        case HIT_ACCENT_AUTO:     config_->accentTheme = AccentTheme::AutoFromWallpaper; notifySettings = true; break;
        case HIT_ACCENT_CYAN:     config_->accentTheme = AccentTheme::NeonCyan; notifySettings = true; break;
        case HIT_ACCENT_PURPLE:   config_->accentTheme = AccentTheme::CyberViolet; notifySettings = true; break;
        case HIT_ACCENT_EMERALD:  config_->accentTheme = AccentTheme::MatrixEmerald; notifySettings = true; break;
        case HIT_ACCENT_AMBER:    config_->accentTheme = AccentTheme::SunsetCrimson; notifySettings = true; break;

        // System & Hotkeys (#9)
        case HIT_TOGGLE_AUTOSTART: {
            config_->startWithWindows = !config_->startWithWindows;
            if (callbacks_.onToggleAutostart) callbacks_.onToggleAutostart(config_->startWithWindows);
            notifySettings = true;
            break;
        }
        case HIT_TOGGLE_MINIMIZED:    config_->startMinimized = !config_->startMinimized; notifySettings = true; break;
        case HIT_TOGGLE_PAUSE_FS:     config_->pauseOnFullscreen = !config_->pauseOnFullscreen; notifySettings = true; break;
        case HIT_TOGGLE_PAUSE_BAT:    config_->pauseOnBattery = !config_->pauseOnBattery; notifySettings = true; break;
        case HIT_TOGGLE_MUTE_UNFOCUS: config_->muteWhenUnfocused = !config_->muteWhenUnfocused; notifySettings = true; break;
        case HIT_TOGGLE_CTX_MENU: {
            config_->explorerContextMenu = !config_->explorerContextMenu;
            if (callbacks_.onToggleContextMenu) callbacks_.onToggleContextMenu(config_->explorerContextMenu);
            notifySettings = true;
            break;
        }
        case HIT_TOGGLE_HOTKEYS: config_->globalHotkeys = !config_->globalHotkeys; notifySettings = true; break;
        case HIT_FPS_30:  config_->targetFps = 30;  notifySettings = true; break;
        case HIT_FPS_60:  config_->targetFps = 60;  notifySettings = true; break;
        case HIT_FPS_120: config_->targetFps = 120; notifySettings = true; break;
        case HIT_FPS_144: config_->targetFps = 144; notifySettings = true; break;
        case HIT_EXIT_APP: if (callbacks_.onExitApp) callbacks_.onExitApp(); return;
        default: break;
    }

    if (notifySettings && callbacks_.onSettingsChanged) {
        callbacks_.onSettingsChanged();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool StudioWindow::CaptureWindowBmp(const std::wstring& outBmpPath) {
    if (!hwnd_) return false;
    Render();

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    HDC wndDc = GetDC(hwnd_);
    if (!wndDc) return false;
    HDC memDc = CreateCompatibleDC(wndDc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(wndDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp || !bits) {
        DeleteDC(memDc);
        ReleaseDC(hwnd_, wndDc);
        return false;
    }

    HGDIOBJ old = SelectObject(memDc, hBmp);
    PrintWindow(hwnd_, memDc, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
    BitBlt(memDc, 0, 0, w, h, wndDc, 0, 0, SRCCOPY);

    bool ok = SaveBmpBGRA(outBmpPath, static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<const uint8_t*>(bits));
    SelectObject(memDc, old);
    DeleteObject(hBmp);
    DeleteDC(memDc);
    ReleaseDC(hwnd_, wndDc);
    return ok;
}

LRESULT CALLBACK StudioWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StudioWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<StudioWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<StudioWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE: {
            if (self->rt_) {
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);
                if (width > 0 && height > 0) {
                    self->rt_->Resize(D2D1::SizeU(width, height));
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = static_cast<LONG>(960.0f * self->dpiScale_);
            mmi->ptMinTrackSize.y = static_cast<LONG>(640.0f * self->dpiScale_);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            self->Render();
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            float dx = static_cast<float>(GET_X_LPARAM(lParam)) / self->dpiScale_;
            float dy = static_cast<float>(GET_Y_LPARAM(lParam)) / self->dpiScale_;
            self->OnMouseDown(dx, dy);
            return 0;
        }
        case WM_MOUSEMOVE: {
            float dx = static_cast<float>(GET_X_LPARAM(lParam)) / self->dpiScale_;
            float dy = static_cast<float>(GET_Y_LPARAM(lParam)) / self->dpiScale_;
            self->OnMouseMove(dx, dy, (wParam & MK_LBUTTON) != 0);
            return 0;
        }
        case WM_LBUTTONUP: {
            self->OnMouseUp();
            return 0;
        }
        case WM_MOUSEWHEEL: {
            self->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        }
        case WM_CHAR: {
            self->OnCharInput(static_cast<wchar_t>(wParam));
            return 0;
        }
        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            std::vector<std::wstring> dropped;
            for (UINT i = 0; i < count; ++i) {
                UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
                std::wstring path(len + 1, L'\0');
                DragQueryFileW(hDrop, i, &path[0], len + 1);
                path.resize(len);
                if (IsSupportedWallpaperExtension(path)) {
                    dropped.push_back(std::move(path));
                }
            }
            DragFinish(hDrop);
            if (!dropped.empty() && self->callbacks_.onAddFiles) {
                self->callbacks_.onAddFiles(dropped);
            }
            return 0;
        }
        case WM_CLOSE: {
            self->HideToTray();
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

}  // namespace wp
