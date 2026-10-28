#include "studio_ui.h"
#include "media_tools.h"

#include <dwmapi.h>
#include <shobjidl.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace wp {

namespace {

constexpr const wchar_t* kStudioWindowClass = L"WallPulseStudioClass";
constexpr UINT WM_TRAYICON = WM_APP + 10;
constexpr UINT WM_SHOW_EXISTING = WM_APP + 11;
constexpr UINT kTrayIconUid = 7001;

// Hit IDs
enum HitIds : int {
    HIT_TAB_LIBRARY     = 10,
    HIT_TAB_MONITORS    = 11,
    HIT_TAB_PERF        = 12,
    HIT_TAB_DIAG        = 13,
    HIT_BTN_OPEN_MP4    = 20,
    HIT_BTN_OPEN_FOLDER = 21,

    HIT_BTN_PLAY_PAUSE  = 30,
    HIT_BTN_MUTE        = 31,
    HIT_BTN_STOP_WALL   = 32,
    HIT_BTN_RESET_FX    = 33,
    HIT_BTN_REVEAL_FILE = 34,

    HIT_SCALE_COVER     = 40,
    HIT_SCALE_FITBLUR   = 41,
    HIT_SCALE_FITBARS   = 42,
    HIT_SCALE_STRETCH   = 43,

    HIT_MON_MODE_ALL    = 50,
    HIT_MON_MODE_SPAN   = 51,
    HIT_MON_MODE_PRIM   = 52,
    HIT_MON_MODE_SPEC   = 53,
    HIT_BTN_REATTACH    = 54,
    HIT_MON_SELECT_BASE = 60,   // + monitor index

    HIT_TOG_FULLSCREEN  = 80,
    HIT_TOG_UNFOC_MUTE  = 81,
    HIT_TOG_BATTERY     = 82,
    HIT_TOG_PARALLAX    = 83,
    HIT_TOG_CLOSETRAY   = 84,
    HIT_TOG_AUTOSTART   = 85,
    HIT_TOG_CTXMENU     = 86,

    HIT_BTN_OPEN_APPDATA = 90,

    HIT_CARD_SELECT_BASE = 1000, // + library index
    HIT_CARD_REMOVE_BASE = 5000, // + library index
};

// Tray context menu commands
enum TrayCmds : UINT {
    CMD_TRAY_OPEN       = 40001,
    CMD_TRAY_PICK_MP4   = 40002,
    CMD_TRAY_PLAY_PAUSE = 40003,
    CMD_TRAY_MUTE       = 40004,
    CMD_TRAY_STOP_WALL  = 40005,
    CMD_TRAY_AUTOSTART  = 40006,
    CMD_TRAY_EXIT       = 40007,
};

D2D1_COLOR_F HexColor(uint32_t rgb, float a = 1.0f) {
    return D2D1::ColorF(
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        a);
}

bool PointInRectF(float x, float y, const D2D1_RECT_F& r) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

void ApplyDarkTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    // Attribute 20 = Windows 10 2004+ / Windows 11; 19 = older Windows 10
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
}

}  // namespace

StudioWindow::StudioWindow() = default;

StudioWindow::~StudioWindow() {
    Destroy();
}

bool StudioWindow::Create(
    HINSTANCE hInstance,
    AppConfig* cfg,
    DesktopHost* desktopHost,
    VideoWallpaperEngine* engine,
    bool startMinimizedToTray) {
    hInstance_ = hInstance;
    cfg_ = cfg;
    desktopHost_ = desktopHost;
    engine_ = engine;

    if (!InitD2D()) {
        LogError(L"Не удалось инициализировать Direct2D / DirectWrite для интерфейса студии");
        return false;
    }

    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_WALLPULSE_ICON));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(13, 17, 25));
    wc.lpszClassName = kStudioWindowClass;
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    float initScale = GetWindowDpiScale(nullptr);
    int winW = std::min(static_cast<int>(1180 * initScale), std::max(840, screenW - 80));
    int winH = std::min(static_cast<int>(740 * initScale), std::max(580, screenH - 80));
    int posX = std::max(20, (screenW - winW) / 2);
    int posY = std::max(20, (screenH - winH) / 2);

    hwnd_ = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        kStudioWindowClass,
        L"WallPulse Studio — Живые MP4 обои рабочего стола",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        posX, posY, winW, winH,
        nullptr, nullptr, hInstance, this);

    if (!hwnd_) {
        LogError(L"CreateWindowExW для StudioWindow вернул NULL");
        return false;
    }

    dpiScale_ = GetWindowDpiScale(hwnd_);
    ApplyDarkTitleBar(hwnd_);
    DragAcceptFiles(hwnd_, TRUE);
    SetupTrayIcon();

    if (!startMinimizedToTray) {
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        UpdateWindow(hwnd_);
    }

    return true;
}

void StudioWindow::Destroy() {
    RemoveTrayIcon();
    DiscardDeviceResources();
    fmtBrand_.Reset();
    fmtTitle_.Reset();
    fmtSection_.Reset();
    fmtBody_.Reset();
    fmtSmall_.Reset();
    fmtBadge_.Reset();
    fmtMono_.Reset();
    dwriteFactory_.Reset();
    d2dFactory_.Reset();

    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool StudioWindow::IsVisibleAndNotMinimized() const {
    return hwnd_ && IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
}

void StudioWindow::ShowAndActivate() {
    if (!hwnd_) return;
    if (IsIconic(hwnd_)) {
        ShowWindow(hwnd_, SW_RESTORE);
    } else {
        ShowWindow(hwnd_, SW_SHOW);
    }
    SetForegroundWindow(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::HideToTray() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_HIDE);
    ShowFirstMinimizeBalloon();
}

bool StudioWindow::InitD2D() {
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        nullptr,
        reinterpret_cast<void**>(d2dFactory_.ReleaseAndGetAddressOf()));
    if (FAILED(hr) || !d2dFactory_) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory_.ReleaseAndGetAddressOf()));
    if (FAILED(hr) || !dwriteFactory_) return false;

    auto makeFmt = [&](float sizePt, DWRITE_FONT_WEIGHT weight, const wchar_t* family, ComPtr<IDWriteTextFormat>& out, bool ellipsis = false) {
        if (SUCCEEDED(dwriteFactory_->CreateTextFormat(
                family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                sizePt, L"ru-ru", out.ReleaseAndGetAddressOf())) && out) {
            out->SetWordWrapping(ellipsis ? DWRITE_WORD_WRAPPING_NO_WRAP : DWRITE_WORD_WRAPPING_WRAP);
            if (ellipsis) {
                DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
                ComPtr<IDWriteInlineObject> sign;
                if (SUCCEEDED(dwriteFactory_->CreateEllipsisTrimmingSign(out.Get(), sign.GetAddressOf()))) {
                    out->SetTrimming(&trim, sign.Get());
                }
            }
        }
    };

    makeFmt(18.0f, DWRITE_FONT_WEIGHT_EXTRA_BOLD, L"Segoe UI", fmtBrand_, true);
    makeFmt(15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"Segoe UI", fmtTitle_, true);
    makeFmt(12.5f, DWRITE_FONT_WEIGHT_BOLD, L"Segoe UI", fmtSection_, true);
    makeFmt(13.0f, DWRITE_FONT_WEIGHT_REGULAR, L"Segoe UI", fmtBody_, false);
    makeFmt(11.5f, DWRITE_FONT_WEIGHT_REGULAR, L"Segoe UI", fmtSmall_, true);
    makeFmt(10.5f, DWRITE_FONT_WEIGHT_BOLD, L"Segoe UI", fmtBadge_, true);
    makeFmt(11.0f, DWRITE_FONT_WEIGHT_REGULAR, L"Consolas", fmtMono_, true);

    return fmtBody_ != nullptr;
}

bool StudioWindow::EnsureRenderTarget() {
    if (rt_) return true;
    if (!hwnd_ || !d2dFactory_) return false;

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(1, rc.right - rc.left)),
        static_cast<UINT32>(std::max<LONG>(1, rc.bottom - rc.top)));

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    props.dpiX = 96.0f * dpiScale_;
    props.dpiY = 96.0f * dpiScale_;

    HRESULT hr = d2dFactory_->CreateHwndRenderTarget(
        props,
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        rt_.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !rt_) return false;

    rt_->CreateSolidColorBrush(HexColor(0xFFFFFF), brush_.ReleaseAndGetAddressOf());
    thumbCache_.clear();
    livePreviewBmp_.Reset();
    lastPreviewSeq_ = 0;
    return brush_ != nullptr;
}

void StudioWindow::DiscardDeviceResources() {
    thumbCache_.clear();
    livePreviewBmp_.Reset();
    brush_.Reset();
    rt_.Reset();
}

ID2D1Bitmap* StudioWindow::GetOrLoadThumbBitmap(const WallpaperItem& item) {
    if (!rt_ || item.thumbPath.empty()) return nullptr;
    auto it = thumbCache_.find(item.thumbPath);
    if (it != thumbCache_.end()) {
        return it->second.Get();
    }

    uint32_t w = 0, h = 0;
    std::vector<uint8_t> bgra;
    if (!LoadBmpBGRA(item.thumbPath, w, h, bgra) || w == 0 || h == 0) {
        thumbCache_[item.thumbPath] = nullptr;
        return nullptr;
    }

    D2D1_BITMAP_PROPERTIES bp{};
    bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
    bp.dpiX = 96.0f;
    bp.dpiY = 96.0f;

    ComPtr<ID2D1Bitmap> bmp;
    if (SUCCEEDED(rt_->CreateBitmap(D2D1::SizeU(w, h), bgra.data(), w * 4, &bp, bmp.GetAddressOf()))) {
        thumbCache_[item.thumbPath] = bmp;
        return bmp.Get();
    }
    return nullptr;
}

bool StudioWindow::AddVideoFile(const std::wstring& filePath, bool applyNow) {
    if (!cfg_ || !FileExists(filePath)) return false;

    if (!IsSupportedVideoExtension(filePath)) {
        statusBanner_ = L"Формат файла не поддерживается (выберите .mp4, .webm, .mkv, .mov, .avi или .wmv)";
        statusBannerUntilMs_ = GetTickCount64() + 5000;
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        return false;
    }

    // Check if already in library
    int existingIdx = -1;
    for (size_t i = 0; i < cfg_->library.size(); ++i) {
        if (_wcsicmp(cfg_->library[i].path.c_str(), filePath.c_str()) == 0) {
            existingIdx = static_cast<int>(i);
            break;
        }
    }

    if (existingIdx < 0) {
        VideoProbeInfo probe = ProbeVideoAndSaveThumbnail(filePath);
        WallpaperItem item;
        item.path = filePath;
        item.title = GetFileNameWithoutExt(filePath);
        item.thumbPath = probe.thumbPath;
        item.width = probe.width;
        item.height = probe.height;
        item.durationSec = probe.durationSec;
        item.fileSize = GetFileSizeBytes(filePath);
        item.isBuiltIn = false;
        cfg_->library.insert(cfg_->library.begin(), std::move(item));
    }

    if (applyNow) {
        ApplyWallpaperByPath(filePath);
    } else {
        SaveAppConfig(*cfg_);
    }

    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    return true;
}

int StudioWindow::ImportFolderVideos(const std::wstring& folderPath, bool applyFirst) {
    if (folderPath.empty()) return 0;
    std::wstring pattern = folderPath + L"\\*.*";
    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    int added = 0;
    std::wstring firstPath;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring full = folderPath + L"\\" + fd.cFileName;
        if (IsSupportedVideoExtension(full)) {
            if (AddVideoFile(full, false)) {
                if (firstPath.empty()) firstPath = full;
                ++added;
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    if (added > 0 && applyFirst && !firstPath.empty()) {
        ApplyWallpaperByPath(firstPath);
    }

    wchar_t msg[160];
    swprintf_s(msg, L"Импортировано видеофайлов из папки: %d", added);
    statusBanner_ = msg;
    statusBannerUntilMs_ = GetTickCount64() + 4500;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    return added;
}

void StudioWindow::ApplyWallpaperByPath(const std::wstring& filePath) {
    if (!cfg_ || !engine_) return;
    cfg_->activeVideoPath = filePath;
    cfg_->userPaused = false;
    if (engine_->LoadAndApply(filePath, *cfg_)) {
        statusBanner_ = L"Живые обои установлены на рабочий стол: " + GetFileNameWithoutExt(filePath);
        statusBannerUntilMs_ = GetTickCount64() + 4000;
    } else {
        statusBanner_ = engine_->LastErrorText().empty()
            ? L"Не удалось запустить выбранный видеофайл"
            : engine_->LastErrorText();
        statusBannerUntilMs_ = GetTickCount64() + 6000;
    }
    SaveAppConfig(*cfg_);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::StopActiveWallpaper() {
    if (!cfg_ || !engine_) return;
    engine_->StopAndDetach();
    cfg_->activeVideoPath.clear();
    SaveAppConfig(*cfg_);
    statusBanner_ = L"Живые обои отключены — восстановлен стандартный фон Windows";
    statusBannerUntilMs_ = GetTickCount64() + 4000;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::OnUiTick(const std::wstring& smartPauseReason) {
    bool changed = (smartPauseText_ != smartPauseReason);
    smartPauseText_ = smartPauseReason;

    if (!IsVisibleAndNotMinimized()) return;

    if (engine_ && rt_) {
        uint32_t pw = 0, ph = 0;
        std::vector<uint8_t> bgra;
        if (engine_->CopyLatestPreviewFrame(pw, ph, bgra, lastPreviewSeq_) && pw > 0 && ph > 0) {
            if (!livePreviewBmp_ ||
                livePreviewBmp_->GetPixelSize().width != pw ||
                livePreviewBmp_->GetPixelSize().height != ph) {
                D2D1_BITMAP_PROPERTIES bp{};
                bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
                bp.dpiX = 96.0f;
                bp.dpiY = 96.0f;
                rt_->CreateBitmap(D2D1::SizeU(pw, ph), bgra.data(), pw * 4, &bp, livePreviewBmp_.ReleaseAndGetAddressOf());
            } else {
                D2D1_RECT_U dst = D2D1::RectU(0, 0, pw, ph);
                livePreviewBmp_->CopyFromMemory(&dst, bgra.data(), pw * 4);
            }
            changed = true;
        }
    }

    if (statusBannerUntilMs_ != 0 && GetTickCount64() > statusBannerUntilMs_) {
        statusBanner_.clear();
        statusBannerUntilMs_ = 0;
        changed = true;
    }

    if (changed && hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void StudioWindow::DrawButton(
    int hitId, const D2D1_RECT_F& rc, const std::wstring& label,
    bool primary, bool active, bool danger) {
    hitZones_.push_back({hitId, rc, 0});
    bool hovered = (hoveredHitId_ == hitId);

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rc, 7.0f, 7.0f);
    if (primary) {
        brush_->SetColor(hovered ? HexColor(0x38BDF8) : HexColor(0x0284C7));
        rt_->FillRoundedRectangle(rr, brush_.Get());
        brush_->SetColor(HexColor(0x7DD3FC, 0.55f));
        rt_->DrawRoundedRectangle(rr, brush_.Get(), 1.0f);
        brush_->SetColor(HexColor(0xFFFFFF));
    } else if (danger) {
        brush_->SetColor(hovered ? HexColor(0xDC2626, 0.35f) : HexColor(0x7F1D1D, 0.25f));
        rt_->FillRoundedRectangle(rr, brush_.Get());
        brush_->SetColor(HexColor(0xEF4444, hovered ? 0.85f : 0.45f));
        rt_->DrawRoundedRectangle(rr, brush_.Get(), 1.0f);
        brush_->SetColor(HexColor(0xFCA5A5));
    } else if (active) {
        brush_->SetColor(HexColor(0x0EA5E9, 0.22f));
        rt_->FillRoundedRectangle(rr, brush_.Get());
        brush_->SetColor(HexColor(0x38BDF8, 0.90f));
        rt_->DrawRoundedRectangle(rr, brush_.Get(), 1.25f);
        brush_->SetColor(HexColor(0xE0F2FE));
    } else {
        brush_->SetColor(hovered ? HexColor(0x243044) : HexColor(0x182130));
        rt_->FillRoundedRectangle(rr, brush_.Get());
        brush_->SetColor(HexColor(hovered ? 0x475569 : 0x293548));
        rt_->DrawRoundedRectangle(rr, brush_.Get(), 1.0f);
        brush_->SetColor(HexColor(hovered ? 0xF8FAFC : 0xCBD5E1));
    }

    fmtSection_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmtSection_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawText(label.c_str(), static_cast<UINT32>(label.size()), fmtSection_.Get(), rc, brush_.Get());
    fmtSection_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtSection_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

void StudioWindow::DrawToggleRow(
    int hitId, const D2D1_RECT_F& rc,
    const std::wstring& title, const std::wstring& subtitle, bool checked) {
    hitZones_.push_back({hitId, rc, 0});
    bool hovered = (hoveredHitId_ == hitId);

    D2D1_ROUNDED_RECT card = D2D1::RoundedRect(rc, 9.0f, 9.0f);
    brush_->SetColor(hovered ? HexColor(0x192334) : HexColor(0x131B28));
    rt_->FillRoundedRectangle(card, brush_.Get());
    brush_->SetColor(HexColor(hovered ? 0x334155 : 0x1E293B));
    rt_->DrawRoundedRectangle(card, brush_.Get(), 1.0f);

    D2D1_RECT_F titleRc = D2D1::RectF(rc.left + 16.0f, rc.top + 11.0f, rc.right - 76.0f, rc.top + 31.0f);
    brush_->SetColor(HexColor(0xF1F5F9));
    rt_->DrawText(title.c_str(), static_cast<UINT32>(title.size()), fmtTitle_.Get(), titleRc, brush_.Get());

    D2D1_RECT_F subRc = D2D1::RectF(rc.left + 16.0f, rc.top + 33.0f, rc.right - 76.0f, rc.bottom - 8.0f);
    brush_->SetColor(HexColor(0x94A3B8));
    rt_->DrawText(subtitle.c_str(), static_cast<UINT32>(subtitle.size()), fmtBody_.Get(), subRc, brush_.Get());

    // Modern pill toggle switch
    float swW = 44.0f, swH = 24.0f;
    float swX = rc.right - 18.0f - swW;
    float swY = (rc.top + rc.bottom - swH) * 0.5f;
    D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(D2D1::RectF(swX, swY, swX + swW, swY + swH), 12.0f, 12.0f);
    brush_->SetColor(checked ? HexColor(0x0EA5E9) : HexColor(0x334155));
    rt_->FillRoundedRectangle(pill, brush_.Get());

    float knobX = checked ? (swX + swW - 12.0f) : (swX + 12.0f);
    float knobY = swY + 12.0f;
    brush_->SetColor(HexColor(0xFFFFFF));
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, knobY), 9.0f, 9.0f), brush_.Get());
}

void StudioWindow::DrawSlider(
    SliderId sid, const D2D1_RECT_F& rc,
    const std::wstring& label, const std::wstring& valueText, float norm01) {
    norm01 = std::clamp(norm01, 0.0f, 1.0f);
    int idx = static_cast<int>(sid);

    D2D1_RECT_F lblRc = D2D1::RectF(rc.left, rc.top, rc.right - 64.0f, rc.top + 18.0f);
    brush_->SetColor(HexColor(0xCBD5E1));
    rt_->DrawText(label.c_str(), static_cast<UINT32>(label.size()), fmtSmall_.Get(), lblRc, brush_.Get());

    D2D1_RECT_F valRc = D2D1::RectF(rc.right - 64.0f, rc.top, rc.right, rc.top + 18.0f);
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    brush_->SetColor(HexColor(0x38BDF8));
    rt_->DrawText(valueText.c_str(), static_cast<UINT32>(valueText.size()), fmtSmall_.Get(), valRc, brush_.Get());
    fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    float trackY = rc.top + 27.0f;
    D2D1_RECT_F track = D2D1::RectF(rc.left + 6.0f, trackY - 3.0f, rc.right - 6.0f, trackY + 3.0f);
    if (idx >= 0 && idx < 6) {
        sliderTracks_[idx] = D2D1::RectF(rc.left, trackY - 11.0f, rc.right, trackY + 11.0f);
    }

    brush_->SetColor(HexColor(0x1E293B));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(track, 3.0f, 3.0f), brush_.Get());

    float fillX = track.left + (track.right - track.left) * norm01;
    D2D1_RECT_F filled = D2D1::RectF(track.left, track.top, fillX, track.bottom);
    brush_->SetColor(HexColor(0x0EA5E9));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(filled, 3.0f, 3.0f), brush_.Get());

    bool activeKnob = (draggingSlider_ == sid);
    brush_->SetColor(activeKnob ? HexColor(0x7DD3FC) : HexColor(0xF8FAFC));
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(fillX, trackY), activeKnob ? 7.5f : 6.0f, activeKnob ? 7.5f : 6.0f), brush_.Get());
}

void StudioWindow::DrawHeader(float width) {
    D2D1_RECT_F bar = D2D1::RectF(0.0f, 0.0f, width, 62.0f);
    brush_->SetColor(HexColor(0x111723));
    rt_->FillRectangle(bar, brush_.Get());
    brush_->SetColor(HexColor(0x1E293B));
    rt_->DrawLine(D2D1::Point2F(0.0f, 62.0f), D2D1::Point2F(width, 62.0f), brush_.Get(), 1.0f);

    // Logo icon badge
    D2D1_ROUNDED_RECT logoBox = D2D1::RoundedRect(D2D1::RectF(18.0f, 13.0f, 54.0f, 49.0f), 9.0f, 9.0f);
    brush_->SetColor(HexColor(0x0EA5E9, 0.20f));
    rt_->FillRoundedRectangle(logoBox, brush_.Get());
    brush_->SetColor(HexColor(0x38BDF8));
    rt_->DrawRoundedRectangle(logoBox, brush_.Get(), 1.5f);
    // Play symbol inside logo
    rt_->DrawLine(D2D1::Point2F(32.0f, 24.0f), D2D1::Point2F(43.0f, 31.0f), brush_.Get(), 2.2f);
    rt_->DrawLine(D2D1::Point2F(43.0f, 31.0f), D2D1::Point2F(32.0f, 38.0f), brush_.Get(), 2.2f);
    rt_->DrawLine(D2D1::Point2F(32.0f, 24.0f), D2D1::Point2F(32.0f, 38.0f), brush_.Get(), 2.2f);

    // Brand text + live engine state
    brush_->SetColor(HexColor(0xF8FAFC));
    const wchar_t* brand = L"WALLPULSE";
    rt_->DrawText(brand, static_cast<UINT32>(wcslen(brand)), fmtBrand_.Get(),
                  D2D1::RectF(64.0f, 11.0f, 235.0f, 34.0f), brush_.Get());

    std::wstring subStatus = L"○ Обои не выбраны";
    uint32_t statusCol = 0x64748B;
    if (engine_ && engine_->IsLoaded()) {
        if (engine_->IsSmartPaused()) {
            subStatus = L"⏸ Умная пауза (0% GPU)";
            statusCol = 0xF59E0B;
        } else if (cfg_ && cfg_->userPaused) {
            subStatus = L"⏸ На паузе";
            statusCol = 0x38BDF8;
        } else {
            subStatus = L"● Активно на рабочем столе";
            statusCol = 0x10B981;
        }
    }
    brush_->SetColor(HexColor(statusCol));
    rt_->DrawText(subStatus.c_str(), static_cast<UINT32>(subStatus.size()), fmtSmall_.Get(),
                  D2D1::RectF(64.0f, 34.0f, 265.0f, 52.0f), brush_.Get());

    // Navigation Tabs
    struct TabItem { int hit; Tab tab; const wchar_t* name; float w; };
    const TabItem tabs[] = {
        {HIT_TAB_LIBRARY,  Tab::Library,     L"Библиотека",         108.0f},
        {HIT_TAB_MONITORS, Tab::Monitors,    L"Мониторы",           100.0f},
        {HIT_TAB_PERF,     Tab::Performance, L"Производительность", 162.0f},
        {HIT_TAB_DIAG,     Tab::Diagnostics, L"Диагностика",        112.0f},
    };
    float tx = 270.0f;
    for (const auto& t : tabs) {
        D2D1_RECT_F trc = D2D1::RectF(tx, 14.0f, tx + t.w, 48.0f);
        DrawButton(t.hit, trc, t.name, false, currentTab_ == t.tab);
        tx += t.w + 8.0f;
    }

    // Primary Open MP4 + Folder buttons on right
    DrawButton(HIT_BTN_OPEN_MP4, D2D1::RectF(width - 282.0f, 13.0f, width - 118.0f, 49.0f),
               L"+ Выбрать MP4", true);
    DrawButton(HIT_BTN_OPEN_FOLDER, D2D1::RectF(width - 110.0f, 13.0f, width - 18.0f, 49.0f),
               L"+ Папка", false);
}

void StudioWindow::DrawLibraryTab(const D2D1_RECT_F& area) {
    // Top dropzone / hint bar
    D2D1_RECT_F hintRc = D2D1::RectF(area.left + 18.0f, area.top + 14.0f, area.right - 18.0f, area.top + 54.0f);
    brush_->SetColor(HexColor(0x131C2B));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(hintRc, 8.0f, 8.0f), brush_.Get());
    brush_->SetColor(HexColor(0x0EA5E9, 0.35f));
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(hintRc, 8.0f, 8.0f), brush_.Get(), 1.0f);

    std::wstring banner = !statusBanner_.empty()
        ? statusBanner_
        : L"Перетащите любой .MP4 файл прямо в это окно или нажмите «+ Выбрать MP4», чтобы поставить видео на фон рабочего стола!";
    brush_->SetColor(!statusBanner_.empty() ? HexColor(0x38BDF8) : HexColor(0x94A3B8));
    fmtBody_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    rt_->DrawText(banner.c_str(), static_cast<UINT32>(banner.size()), fmtBody_.Get(),
                  D2D1::RectF(hintRc.left + 14.0f, hintRc.top, hintRc.right - 14.0f, hintRc.bottom), brush_.Get());
    fmtBody_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    if (!cfg_) return;

    D2D1_RECT_F gridClip = D2D1::RectF(area.left, area.top + 64.0f, area.right, area.bottom);
    rt_->PushAxisAlignedClip(gridClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    const float padX = 18.0f;
    const float gap = 14.0f;
    const float availW = std::max(220.0f, (gridClip.right - gridClip.left) - padX * 2.0f);
    int cols = std::max(1, static_cast<int>((availW + gap) / (224.0f + gap)));
    const float cardW = (availW - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float thumbH = std::round(cardW * 9.0f / 16.0f);
    const float cardH = thumbH + 54.0f;

    int totalItems = static_cast<int>(cfg_->library.size());
    int rows = (totalItems + cols - 1) / cols;
    float contentH = static_cast<float>(rows) * (cardH + gap) + 20.0f;
    maxLibraryScrollY_ = std::max(0.0f, contentH - (gridClip.bottom - gridClip.top));
    libraryScrollY_ = std::clamp(libraryScrollY_, 0.0f, maxLibraryScrollY_);

    for (int i = 0; i < totalItems; ++i) {
        const auto& item = cfg_->library[static_cast<size_t>(i)];
        int col = i % cols;
        int row = i / cols;

        float x = gridClip.left + padX + static_cast<float>(col) * (cardW + gap);
        float y = gridClip.top + 4.0f + static_cast<float>(row) * (cardH + gap) - libraryScrollY_;
        D2D1_RECT_F cardRc = D2D1::RectF(x, y, x + cardW, y + cardH);

        if (cardRc.bottom < gridClip.top || cardRc.top > gridClip.bottom) {
            continue;
        }

        int hitCard = HIT_CARD_SELECT_BASE + i;
        int hitRem = HIT_CARD_REMOVE_BASE + i;

        // Register remove button FIRST if non-built-in so hit-testing prefers it over card body
        D2D1_RECT_F remRc = D2D1::RectF(cardRc.right - 28.0f, cardRc.top + 6.0f, cardRc.right - 6.0f, cardRc.top + 28.0f);
        if (!item.isBuiltIn) {
            hitZones_.push_back({hitRem, remRc, i});
        }
        hitZones_.push_back({hitCard, cardRc, i});

        bool isActive = !cfg_->activeVideoPath.empty() &&
                        (_wcsicmp(item.path.c_str(), cfg_->activeVideoPath.c_str()) == 0);
        bool isHover = (hoveredHitId_ == hitCard || hoveredHitId_ == hitRem);

        D2D1_ROUNDED_RECT crr = D2D1::RoundedRect(cardRc, 10.0f, 10.0f);
        brush_->SetColor(isHover ? HexColor(0x1B2638) : HexColor(0x141C2B));
        rt_->FillRoundedRectangle(crr, brush_.Get());

        // Thumbnail image
        D2D1_RECT_F imgRc = D2D1::RectF(cardRc.left + 2.0f, cardRc.top + 2.0f, cardRc.right - 2.0f, cardRc.top + thumbH);
        ID2D1Bitmap* bmp = GetOrLoadThumbBitmap(item);
        if (bmp) {
            rt_->DrawBitmap(bmp, imgRc, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else {
            brush_->SetColor(HexColor(0x0F172A));
            rt_->FillRectangle(imgRc, brush_.Get());
        }

        // Format badge top-left
        std::wstring extBadge = GetFileExtensionLower(item.path);
        if (!extBadge.empty() && extBadge[0] == L'.') extBadge.erase(0, 1);
        for (wchar_t& c : extBadge) c = static_cast<wchar_t>(towupper(c));
        if (extBadge.empty()) extBadge = L"MP4";

        D2D1_RECT_F badgeRc = D2D1::RectF(cardRc.left + 8.0f, cardRc.top + 8.0f, cardRc.left + 50.0f, cardRc.top + 26.0f);
        brush_->SetColor(HexColor(0x090D16, 0.80f));
        rt_->FillRoundedRectangle(D2D1::RoundedRect(badgeRc, 4.0f, 4.0f), brush_.Get());
        brush_->SetColor(HexColor(0x7DD3FC));
        fmtBadge_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtBadge_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt_->DrawText(extBadge.c_str(), static_cast<UINT32>(extBadge.size()), fmtBadge_.Get(), badgeRc, brush_.Get());

        // Active badge or Remove button
        if (isActive) {
            D2D1_RECT_F actRc = D2D1::RectF(cardRc.right - 82.0f, cardRc.top + 8.0f, cardRc.right - 8.0f, cardRc.top + 26.0f);
            brush_->SetColor(HexColor(0x059669, 0.92f));
            rt_->FillRoundedRectangle(D2D1::RoundedRect(actRc, 4.0f, 4.0f), brush_.Get());
            brush_->SetColor(HexColor(0xFFFFFF));
            const wchar_t* actTxt = L"АКТИВНО";
            rt_->DrawText(actTxt, static_cast<UINT32>(wcslen(actTxt)), fmtBadge_.Get(), actRc, brush_.Get());
        } else if (!item.isBuiltIn && isHover) {
            brush_->SetColor(hoveredHitId_ == hitRem ? HexColor(0xDC2626) : HexColor(0x090D16, 0.82f));
            rt_->FillRoundedRectangle(D2D1::RoundedRect(remRc, 5.0f, 5.0f), brush_.Get());
            brush_->SetColor(HexColor(0xF8FAFC));
            const wchar_t* xTxt = L"✕";
            rt_->DrawText(xTxt, 1, fmtBadge_.Get(), remRc, brush_.Get());
        }
        fmtBadge_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        fmtBadge_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        // Card title & metadata
        D2D1_RECT_F titleRc = D2D1::RectF(cardRc.left + 10.0f, cardRc.top + thumbH + 7.0f, cardRc.right - 10.0f, cardRc.top + thumbH + 28.0f);
        brush_->SetColor(HexColor(0xF1F5F9));
        rt_->DrawText(item.title.c_str(), static_cast<UINT32>(item.title.size()), fmtTitle_.Get(), titleRc, brush_.Get());

        wchar_t meta[128];
        if (item.width > 0 && item.height > 0) {
            swprintf_s(meta, L"%ux%u • %ls • %ls",
                       item.width, item.height,
                       FormatDuration(item.durationSec).c_str(),
                       FormatFileSize(item.fileSize).c_str());
        } else {
            swprintf_s(meta, L"%ls • %ls",
                       FormatDuration(item.durationSec).c_str(),
                       FormatFileSize(item.fileSize).c_str());
        }
        D2D1_RECT_F metaRc = D2D1::RectF(cardRc.left + 10.0f, cardRc.top + thumbH + 29.0f, cardRc.right - 10.0f, cardRc.bottom - 5.0f);
        brush_->SetColor(HexColor(0x94A3B8));
        rt_->DrawText(meta, static_cast<UINT32>(wcslen(meta)), fmtSmall_.Get(), metaRc, brush_.Get());

        // Card border (glowing cyan when active)
        brush_->SetColor(isActive ? HexColor(0x38BDF8) : HexColor(isHover ? 0x475569 : 0x1E293B));
        rt_->DrawRoundedRectangle(crr, brush_.Get(), isActive ? 2.0f : 1.0f);
    }

    rt_->PopAxisAlignedClip();
}

void StudioWindow::DrawMonitorsTab(const D2D1_RECT_F& area) {
    float x = area.left + 22.0f;
    float y = area.top + 18.0f;
    float w = (area.right - area.left) - 44.0f;

    brush_->SetColor(HexColor(0xF8FAFC));
    const wchar_t* hdr = L"Расположение живых обоев на мониторах";
    rt_->DrawText(hdr, static_cast<UINT32>(wcslen(hdr)), fmtTitle_.Get(), D2D1::RectF(x, y, x + w, y + 24.0f), brush_.Get());
    y += 32.0f;

    // Monitor visual layout box
    D2D1_RECT_F boxRc = D2D1::RectF(x, y, x + w, y + 170.0f);
    brush_->SetColor(HexColor(0x111827));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(boxRc, 10.0f, 10.0f), brush_.Get());
    brush_->SetColor(HexColor(0x1E293B));
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(boxRc, 10.0f, 10.0f), brush_.Get(), 1.0f);

    if (desktopHost_) {
        auto mons = desktopHost_->EnumerateMonitors();
        RECT virt = desktopHost_->GetVirtualScreenRect();
        float vw = static_cast<float>(std::max<LONG>(1, virt.right - virt.left));
        float vh = static_cast<float>(std::max<LONG>(1, virt.bottom - virt.top));
        float scale = std::min((w - 60.0f) / vw, 120.0f / vh);
        float totalDrawW = vw * scale;
        float totalDrawH = vh * scale;
        float originX = x + (w - totalDrawW) * 0.5f;
        float originY = y + (170.0f - totalDrawH) * 0.5f;

        for (size_t i = 0; i < mons.size(); ++i) {
            const auto& m = mons[i];
            D2D1_RECT_F mrc = D2D1::RectF(
                originX + static_cast<float>(m.bounds.left - virt.left) * scale + 3.0f,
                originY + static_cast<float>(m.bounds.top - virt.top) * scale + 3.0f,
                originX + static_cast<float>(m.bounds.right - virt.left) * scale - 3.0f,
                originY + static_cast<float>(m.bounds.bottom - virt.top) * scale - 3.0f);

            int hitMon = HIT_MON_SELECT_BASE + static_cast<int>(i);
            hitZones_.push_back({hitMon, mrc, static_cast<int>(i)});
            bool selected = (cfg_ && cfg_->targetMonitorIndex == static_cast<int>(i));

            brush_->SetColor(selected ? HexColor(0x0EA5E9, 0.28f) : HexColor(0x1E293B));
            rt_->FillRoundedRectangle(D2D1::RoundedRect(mrc, 6.0f, 6.0f), brush_.Get());
            brush_->SetColor(selected ? HexColor(0x38BDF8) : HexColor(0x475569));
            rt_->DrawRoundedRectangle(D2D1::RoundedRect(mrc, 6.0f, 6.0f), brush_.Get(), selected ? 2.0f : 1.0f);

            wchar_t lbl[128];
            swprintf_s(lbl, L"Экран %zu%ls\n%dx%d",
                       i + 1, m.isPrimary ? L" (Осн.)" : L"", m.Width(), m.Height());
            fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            brush_->SetColor(HexColor(0xF8FAFC));
            rt_->DrawText(lbl, static_cast<UINT32>(wcslen(lbl)), fmtSmall_.Get(), mrc, brush_.Get());
            fmtSmall_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmtSmall_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
    }
    y += 186.0f;

    // 4 MonitorMode buttons
    if (cfg_) {
        float halfW = (w - 12.0f) * 0.5f;
        DrawButton(HIT_MON_MODE_ALL,  D2D1::RectF(x, y, x + halfW, y + 42.0f),
                   L"Одинаковые обои на всех экранах", false, cfg_->monitorMode == MonitorMode::AllSame);
        DrawButton(HIT_MON_MODE_SPAN, D2D1::RectF(x + halfW + 12.0f, y, x + w, y + 42.0f),
                   L"Единая панорама на все экраны", false, cfg_->monitorMode == MonitorMode::SpanAcross);
        y += 52.0f;
        DrawButton(HIT_MON_MODE_PRIM, D2D1::RectF(x, y, x + halfW, y + 42.0f),
                   L"Только основной монитор", false, cfg_->monitorMode == MonitorMode::PrimaryOnly);
        DrawButton(HIT_MON_MODE_SPEC, D2D1::RectF(x + halfW + 12.0f, y, x + w, y + 42.0f),
                   L"Только выбранный монитор выше", false, cfg_->monitorMode == MonitorMode::SpecificMonitor);
        y += 62.0f;
    }

    // Windows Desktop Integration Status
    D2D1_RECT_F topoCard = D2D1::RectF(x, y, x + w, y + 96.0f);
    brush_->SetColor(HexColor(0x131B28));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(topoCard, 9.0f, 9.0f), brush_.Get());
    brush_->SetColor(HexColor(0x1E293B));
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(topoCard, 9.0f, 9.0f), brush_.Get(), 1.0f);

    std::wstring topoStr = desktopHost_ ? desktopHost_->CurrentTopology().Describe() : L"—";
    brush_->SetColor(HexColor(0x94A3B8));
    rt_->DrawText(topoStr.c_str(), static_cast<UINT32>(topoStr.size()), fmtSmall_.Get(),
                  D2D1::RectF(x + 16.0f, y + 14.0f, x + w - 16.0f, y + 40.0f), brush_.Get());

    DrawButton(HIT_BTN_REATTACH, D2D1::RectF(x + 16.0f, y + 46.0f, x + 310.0f, y + 82.0f),
               L"Переподключить слой рабочего стола", false);
}

void StudioWindow::DrawPerformanceTab(const D2D1_RECT_F& area) {
    if (!cfg_) return;
    float x = area.left + 22.0f;
    float y = area.top + 14.0f;
    float w = (area.right - area.left) - 44.0f;
    const float rowH = 62.0f;
    const float gap = 8.0f;

    DrawToggleRow(HIT_TOG_FULLSCREEN, D2D1::RectF(x, y, x + w, y + rowH),
                  L"Умная пауза в полноэкранных играх и окнах",
                  L"Автоматически останавливает рендеринг (0% CPU/GPU), когда экран закрыт игрой или развёрнутым окном.",
                  cfg_->pauseOnFullscreen);
    y += rowH + gap;

    DrawToggleRow(HIT_TOG_UNFOC_MUTE, D2D1::RectF(x, y, x + w, y + rowH),
                  L"Выключать звук обоев в других программах",
                  L"Воспроизводить аудиодорожку MP4 только когда открыт рабочий стол или студия WallPulse.",
                  cfg_->muteWhenUnfocused);
    y += rowH + gap;

    DrawToggleRow(HIT_TOG_BATTERY, D2D1::RectF(x, y, x + w, y + rowH),
                  L"Экономия заряда при работе ноутбука от батареи",
                  L"Ставить живые обои на паузу при отключении питания от сети.",
                  cfg_->pauseOnBattery);
    y += rowH + gap;

    DrawToggleRow(HIT_TOG_PARALLAX, D2D1::RectF(x, y, x + w, y + rowH),
                  L"Интерактивный параллакс за курсором мыши",
                  L"Плавное смещение глубины видеообоев вслед за движением курсора мыши.",
                  cfg_->mouseParallax);
    y += rowH + gap;

    DrawToggleRow(HIT_TOG_CLOSETRAY, D2D1::RectF(x, y, x + w, y + rowH),
                  L"Сворачивать в системный трей при закрытии [×]",
                  L"Живые обои продолжат работать в фоне (значок около часов Windows).",
                  cfg_->closeToTray);
    y += rowH + gap;

    DrawToggleRow(HIT_TOG_AUTOSTART, D2D1::RectF(x, y, x + w, y + rowH),
                  L"Запускать живые обои вместе с Windows",
                  L"Автоматически восстанавливать выбранный MP4 на рабочем столе после перезагрузки ПК.",
                  cfg_->startWithWindows);
    y += rowH + gap;

    DrawToggleRow(HIT_TOG_CTXMENU, D2D1::RectF(x, y, x + w, y + rowH),
                  L"Пункт «Поставить на рабочий стол» в меню Проводника",
                  L"Нажмите ПКМ по любому .MP4 файлу в Windows, чтобы мгновенно сделать его обоями.",
                  IsExplorerContextMenuEnabled());
}

void StudioWindow::DrawDiagnosticsTab(const D2D1_RECT_F& area) {
    float x = area.left + 22.0f;
    float y = area.top + 18.0f;
    float w = (area.right - area.left) - 44.0f;

    D2D1_RECT_F infoRc = D2D1::RectF(x, y, x + w, y + 132.0f);
    brush_->SetColor(HexColor(0x131B28));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(infoRc, 10.0f, 10.0f), brush_.Get());
    brush_->SetColor(HexColor(0x1E293B));
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(infoRc, 10.0f, 10.0f), brush_.Get(), 1.0f);

    std::wstring gpu = engine_ ? engine_->GpuAdapterName() : L"—";
    std::wstring topo = desktopHost_ ? desktopHost_->CurrentTopology().Describe() : L"—";
    std::wstring summary = L"WallPulse Wallpaper Engine v1.0.0 (Native Win32 + Direct3D 11 + Media Foundation)\n"
                           L"Видеоускоритель: " + gpu + L"\n" +
                           topo;

    brush_->SetColor(HexColor(0xE2E8F0));
    rt_->DrawText(summary.c_str(), static_cast<UINT32>(summary.size()), fmtBody_.Get(),
                  D2D1::RectF(x + 16.0f, y + 12.0f, x + w - 16.0f, y + 84.0f), brush_.Get());

    DrawButton(HIT_BTN_OPEN_APPDATA, D2D1::RectF(x + 16.0f, y + 88.0f, x + 290.0f, y + 122.0f),
               L"Открыть папку настроек и логов", false);
    y += 146.0f;

    // Log console box
    D2D1_RECT_F logBox = D2D1::RectF(x, y, x + w, area.bottom - 18.0f);
    brush_->SetColor(HexColor(0x0B0F17));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(logBox, 8.0f, 8.0f), brush_.Get());
    brush_->SetColor(HexColor(0x1E293B));
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(logBox, 8.0f, 8.0f), brush_.Get(), 1.0f);

    auto logs = GetRecentLogs(22);
    float ly = logBox.top + 10.0f;
    brush_->SetColor(HexColor(0x94A3B8));
    for (const auto& line : logs) {
        if (ly + 16.0f > logBox.bottom - 6.0f) break;
        rt_->DrawText(line.c_str(), static_cast<UINT32>(line.size()), fmtMono_.Get(),
                      D2D1::RectF(logBox.left + 12.0f, ly, logBox.right - 12.0f, ly + 16.0f), brush_.Get());
        ly += 16.0f;
    }
}

void StudioWindow::DrawInspectorSidebar(const D2D1_RECT_F& area) {
    brush_->SetColor(HexColor(0x111723));
    rt_->FillRectangle(area, brush_.Get());
    brush_->SetColor(HexColor(0x1E293B));
    rt_->DrawLine(D2D1::Point2F(area.left, area.top), D2D1::Point2F(area.left, area.bottom), brush_.Get(), 1.0f);

    if (!cfg_) return;

    float x = area.left + 18.0f;
    float w = (area.right - area.left) - 36.0f;
    float y = area.top + 16.0f;

    // 16:9 Live Preview Frame
    float prevH = std::round(w * 9.0f / 16.0f);
    D2D1_RECT_F prevRc = D2D1::RectF(x, y, x + w, y + prevH);
    brush_->SetColor(HexColor(0x090D16));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(prevRc, 8.0f, 8.0f), brush_.Get());

    const WallpaperItem* activeItem = nullptr;
    for (const auto& it : cfg_->library) {
        if (!cfg_->activeVideoPath.empty() && _wcsicmp(it.path.c_str(), cfg_->activeVideoPath.c_str()) == 0) {
            activeItem = &it;
            break;
        }
    }

    if (livePreviewBmp_ && engine_ && engine_->IsLoaded()) {
        rt_->DrawBitmap(livePreviewBmp_.Get(), prevRc, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else if (activeItem) {
        ID2D1Bitmap* tb = GetOrLoadThumbBitmap(*activeItem);
        if (tb) rt_->DrawBitmap(tb, prevRc, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }

    brush_->SetColor(HexColor(0x334155));
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(prevRc, 8.0f, 8.0f), brush_.Get(), 1.0f);
    y += prevH + 10.0f;

    // Selected Title + Resolution
    std::wstring selTitle = activeItem ? activeItem->title
                          : (!cfg_->activeVideoPath.empty() ? GetFileNameWithoutExt(cfg_->activeVideoPath) : L"Обои не выбраны");
    brush_->SetColor(HexColor(0xF8FAFC));
    rt_->DrawText(selTitle.c_str(), static_cast<UINT32>(selTitle.size()), fmtTitle_.Get(),
                  D2D1::RectF(x, y, x + w - 34.0f, y + 22.0f), brush_.Get());

    if (!cfg_->activeVideoPath.empty()) {
        DrawButton(HIT_BTN_REVEAL_FILE, D2D1::RectF(x + w - 30.0f, y - 2.0f, x + w, y + 24.0f), L"📂", false);
    }
    y += 25.0f;

    if (engine_ && engine_->IsLoaded()) {
        wchar_t infoLine[128];
        swprintf_s(infoLine, L"%ux%u • %ls / %ls",
                   engine_->VideoWidth(), engine_->VideoHeight(),
                   FormatDuration(engine_->CurrentTimeSec()).c_str(),
                   FormatDuration(engine_->DurationSec()).c_str());
        brush_->SetColor(HexColor(0x94A3B8));
        rt_->DrawText(infoLine, static_cast<UINT32>(wcslen(infoLine)), fmtSmall_.Get(),
                      D2D1::RectF(x, y, x + w, y + 18.0f), brush_.Get());
    }
    y += 24.0f;

    // Playback Controls Row: Play/Pause | Mute | Stop
    std::wstring playLbl = cfg_->userPaused ? L"▶ Играть" : L"⏸ Пауза";
    std::wstring muteLbl = (cfg_->muted || cfg_->volume <= 0.001f) ? L"🔇 Звук" : L"🔊 Звук";
    DrawButton(HIT_BTN_PLAY_PAUSE, D2D1::RectF(x, y, x + 104.0f, y + 34.0f), playLbl, false, !cfg_->userPaused && engine_ && engine_->IsLoaded());
    DrawButton(HIT_BTN_MUTE,       D2D1::RectF(x + 110.0f, y, x + 194.0f, y + 34.0f), muteLbl, false, !cfg_->muted && cfg_->volume > 0.001f);
    DrawButton(HIT_BTN_STOP_WALL,  D2D1::RectF(x + 200.0f, y, x + w, y + 34.0f), L"Убрать", false, false, true);
    y += 44.0f;

    // Scaling mode 2x2 pills
    brush_->SetColor(HexColor(0x94A3B8));
    const wchar_t* scHdr = L"РЕЖИМ МАСШТАБИРОВАНИЯ";
    rt_->DrawText(scHdr, static_cast<UINT32>(wcslen(scHdr)), fmtBadge_.Get(), D2D1::RectF(x, y, x + w, y + 16.0f), brush_.Get());
    y += 19.0f;

    float hw = (w - 8.0f) * 0.5f;
    DrawButton(HIT_SCALE_COVER,   D2D1::RectF(x, y, x + hw, y + 30.0f),
               L"Заполнение", false, cfg_->scaleMode == ScaleMode::Cover);
    DrawButton(HIT_SCALE_FITBLUR, D2D1::RectF(x + hw + 8.0f, y, x + w, y + 30.0f),
               L"Вписать + Фон", false, cfg_->scaleMode == ScaleMode::FitBlur);
    y += 35.0f;
    DrawButton(HIT_SCALE_FITBARS, D2D1::RectF(x, y, x + hw, y + 30.0f),
               L"Вписать (Поля)", false, cfg_->scaleMode == ScaleMode::FitBars);
    DrawButton(HIT_SCALE_STRETCH, D2D1::RectF(x + hw + 8.0f, y, x + w, y + 30.0f),
               L"Растянуть", false, cfg_->scaleMode == ScaleMode::Stretch);
    y += 40.0f;

    // Sliders
    wchar_t vbuf[32];
    float dispVol = cfg_->muted ? 0.0f : cfg_->volume;
    swprintf_s(vbuf, L"%d%%", static_cast<int>( std::round(dispVol * 100.0f)));
    DrawSlider(SliderId::Volume, D2D1::RectF(x, y, x + w, y + 36.0f), L"Громкость звука", vbuf, dispVol);
    y += 38.0f;

    swprintf_s(vbuf, L"%.2f×", cfg_->playbackRate);
    DrawSlider(SliderId::Speed, D2D1::RectF(x, y, x + w, y + 36.0f),
               L"Скорость видео", vbuf, (cfg_->playbackRate - 0.25f) / 1.75f);
    y += 38.0f;

    swprintf_s(vbuf, L"%+d%%", static_cast<int>(std::round(cfg_->brightness * 100.0f)));
    DrawSlider(SliderId::Brightness, D2D1::RectF(x, y, x + w, y + 36.0f),
               L"Яркость", vbuf, cfg_->brightness + 0.5f);
    y += 38.0f;

    swprintf_s(vbuf, L"%d%%", static_cast<int>(std::round(cfg_->contrast * 100.0f)));
    DrawSlider(SliderId::Contrast, D2D1::RectF(x, y, x + w, y + 36.0f),
               L"Контрастность", vbuf, cfg_->contrast - 0.5f);
    y += 38.0f;

    swprintf_s(vbuf, L"%d%%", static_cast<int>(std::round(cfg_->saturation * 100.0f)));
    DrawSlider(SliderId::Saturation, D2D1::RectF(x, y, x + w, y + 36.0f),
               L"Насыщенность", vbuf, cfg_->saturation * 0.5f);
    y += 38.0f;

    swprintf_s(vbuf, L"%d%%", static_cast<int>(std::round(cfg_->vignette * 100.0f)));
    DrawSlider(SliderId::Vignette, D2D1::RectF(x, y, x + w, y + 36.0f),
               L"Затемнение под иконки", vbuf, cfg_->vignette / 0.80f);
    y += 42.0f;

    if (y + 30.0f <= area.bottom) {
        DrawButton(HIT_BTN_RESET_FX, D2D1::RectF(x, y, x + w, y + 31.0f), L"Сбросить эффекты", false);
    }
}

void StudioWindow::Render() {
    if (!EnsureRenderTarget()) return;

    hitZones_.clear();
    D2D1_SIZE_F sz = rt_->GetSize();
    const float sidebarW = 340.0f;
    const float headerH = 62.0f;

    rt_->BeginDraw();
    rt_->Clear(HexColor(0x0D1119));

    DrawHeader(sz.width);

    D2D1_RECT_F mainArea = D2D1::RectF(0.0f, headerH, std::max(200.0f, sz.width - sidebarW), sz.height);
    D2D1_RECT_F sideArea = D2D1::RectF(mainArea.right, headerH, sz.width, sz.height);

    switch (currentTab_) {
        case Tab::Library:     DrawLibraryTab(mainArea); break;
        case Tab::Monitors:    DrawMonitorsTab(mainArea); break;
        case Tab::Performance: DrawPerformanceTab(mainArea); break;
        case Tab::Diagnostics: DrawDiagnosticsTab(mainArea); break;
    }

    DrawInspectorSidebar(sideArea);

    HRESULT hr = rt_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

void StudioWindow::UpdateSliderFromMouse(SliderId sid, float mouseX, const D2D1_RECT_F& trackRc) {
    if (!cfg_) return;
    float t = std::clamp((mouseX - (trackRc.left + 6.0f)) / std::max(1.0f, (trackRc.right - trackRc.left - 12.0f)), 0.0f, 1.0f);

    switch (sid) {
        case SliderId::Volume:
            cfg_->volume = t;
            cfg_->muted = (t <= 0.005f);
            break;
        case SliderId::Speed:
            cfg_->playbackRate = std::round((0.25f + t * 1.75f) * 20.0f) / 20.0f;
            break;
        case SliderId::Brightness:
            cfg_->brightness = std::round((t - 0.5f) * 100.0f) / 100.0f;
            break;
        case SliderId::Contrast:
            cfg_->contrast = std::round((0.5f + t * 1.0f) * 100.0f) / 100.0f;
            break;
        case SliderId::Saturation:
            cfg_->saturation = std::round((t * 2.0f) * 100.0f) / 100.0f;
            break;
        case SliderId::Vignette:
            cfg_->vignette = std::round((t * 0.80f) * 100.0f) / 100.0f;
            break;
        default:
            break;
    }

    if (engine_) engine_->ApplySettings(*cfg_, false);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::OnMouseDown(float x, float y, bool rightButton) {
    if (!cfg_) return;

    // Check sliders first (left button)
    if (!rightButton) {
        for (int i = 0; i < 6; ++i) {
            if (PointInRectF(x, y, sliderTracks_[i])) {
                draggingSlider_ = static_cast<SliderId>(i);
                SetCapture(hwnd_);
                UpdateSliderFromMouse(draggingSlider_, x, sliderTracks_[i]);
                return;
            }
        }
    }

    int hit = 0;
    int extra = 0;
    for (const auto& hz : hitZones_) {
        if (PointInRectF(x, y, hz.rect)) {
            hit = hz.id;
            extra = hz.extra;
            break;
        }
    }
    if (hit == 0) return;

    if (rightButton && hit >= HIT_CARD_SELECT_BASE && hit < HIT_CARD_REMOVE_BASE) {
        if (extra >= 0 && static_cast<size_t>(extra) < cfg_->library.size()) {
            std::wstring param = L"/select,\"" + cfg_->library[static_cast<size_t>(extra)].path + L"\"";
            ShellExecuteW(hwnd_, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
        }
        return;
    }
    if (rightButton) return;

    if (hit >= HIT_CARD_REMOVE_BASE) {
        if (extra >= 0 && static_cast<size_t>(extra) < cfg_->library.size()) {
            std::wstring remPath = cfg_->library[static_cast<size_t>(extra)].path;
            cfg_->library.erase(cfg_->library.begin() + extra);
            if (_wcsicmp(remPath.c_str(), cfg_->activeVideoPath.c_str()) == 0) {
                StopActiveWallpaper();
            } else {
                SaveAppConfig(*cfg_);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return;
    }

    if (hit >= HIT_CARD_SELECT_BASE) {
        if (extra >= 0 && static_cast<size_t>(extra) < cfg_->library.size()) {
            ApplyWallpaperByPath(cfg_->library[static_cast<size_t>(extra)].path);
        }
        return;
    }

    if (hit >= HIT_MON_SELECT_BASE && hit < HIT_MON_SELECT_BASE + 16) {
        cfg_->targetMonitorIndex = extra;
        cfg_->monitorMode = MonitorMode::SpecificMonitor;
        if (engine_) engine_->ApplySettings(*cfg_, true);
        SaveAppConfig(*cfg_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    bool layoutChanged = false;
    bool settingsChanged = false;

    switch (hit) {
        case HIT_TAB_LIBRARY:  currentTab_ = Tab::Library; break;
        case HIT_TAB_MONITORS: currentTab_ = Tab::Monitors; break;
        case HIT_TAB_PERF:     currentTab_ = Tab::Performance; break;
        case HIT_TAB_DIAG:     currentTab_ = Tab::Diagnostics; break;

        case HIT_BTN_OPEN_MP4:    OpenFilePickerDialog(); break;
        case HIT_BTN_OPEN_FOLDER: OpenFolderPickerDialog(); break;

        case HIT_BTN_PLAY_PAUSE:
            cfg_->userPaused = !cfg_->userPaused;
            settingsChanged = true;
            break;
        case HIT_BTN_MUTE:
            cfg_->muted = !cfg_->muted;
            if (!cfg_->muted && cfg_->volume <= 0.02f) cfg_->volume = 0.50f;
            settingsChanged = true;
            break;
        case HIT_BTN_STOP_WALL:
            StopActiveWallpaper();
            break;
        case HIT_BTN_RESET_FX:
            cfg_->ResetEffects();
            settingsChanged = true;
            break;
        case HIT_BTN_REVEAL_FILE:
            if (!cfg_->activeVideoPath.empty()) {
                std::wstring param = L"/select,\"" + cfg_->activeVideoPath + L"\"";
                ShellExecuteW(hwnd_, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
            }
            break;

        case HIT_SCALE_COVER:   cfg_->scaleMode = ScaleMode::Cover; settingsChanged = true; break;
        case HIT_SCALE_FITBLUR: cfg_->scaleMode = ScaleMode::FitBlur; settingsChanged = true; break;
        case HIT_SCALE_FITBARS: cfg_->scaleMode = ScaleMode::FitBars; settingsChanged = true; break;
        case HIT_SCALE_STRETCH: cfg_->scaleMode = ScaleMode::Stretch; settingsChanged = true; break;

        case HIT_MON_MODE_ALL:  cfg_->monitorMode = MonitorMode::AllSame; layoutChanged = true; settingsChanged = true; break;
        case HIT_MON_MODE_SPAN: cfg_->monitorMode = MonitorMode::SpanAcross; layoutChanged = true; settingsChanged = true; break;
        case HIT_MON_MODE_PRIM: cfg_->monitorMode = MonitorMode::PrimaryOnly; layoutChanged = true; settingsChanged = true; break;
        case HIT_MON_MODE_SPEC: cfg_->monitorMode = MonitorMode::SpecificMonitor; layoutChanged = true; settingsChanged = true; break;
        case HIT_BTN_REATTACH:
            if (desktopHost_) desktopHost_->DiscoverAndPrepare();
            if (!cfg_->activeVideoPath.empty() && engine_) {
                engine_->LoadAndApply(cfg_->activeVideoPath, *cfg_);
            }
            break;

        case HIT_TOG_FULLSCREEN: cfg_->pauseOnFullscreen = !cfg_->pauseOnFullscreen; settingsChanged = true; break;
        case HIT_TOG_UNFOC_MUTE: cfg_->muteWhenUnfocused = !cfg_->muteWhenUnfocused; settingsChanged = true; break;
        case HIT_TOG_BATTERY:    cfg_->pauseOnBattery = !cfg_->pauseOnBattery; settingsChanged = true; break;
        case HIT_TOG_PARALLAX:   cfg_->mouseParallax = !cfg_->mouseParallax; settingsChanged = true; break;
        case HIT_TOG_CLOSETRAY:  cfg_->closeToTray = !cfg_->closeToTray; settingsChanged = true; break;
        case HIT_TOG_AUTOSTART:
            cfg_->startWithWindows = !cfg_->startWithWindows;
            SetAutoStartInRegistry(cfg_->startWithWindows);
            settingsChanged = true;
            break;
        case HIT_TOG_CTXMENU:
            SetExplorerContextMenu(!IsExplorerContextMenuEnabled());
            break;

        case HIT_BTN_OPEN_APPDATA:
            ShellExecuteW(hwnd_, L"open", GetAppDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
    }

    if (settingsChanged) {
        if (engine_) engine_->ApplySettings(*cfg_, layoutChanged);
        SaveAppConfig(*cfg_);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void StudioWindow::OnMouseMove(float x, float y) {
    if (draggingSlider_ != SliderId::None) {
        int idx = static_cast<int>(draggingSlider_);
        UpdateSliderFromMouse(draggingSlider_, x, sliderTracks_[idx]);
        return;
    }

    int hit = 0;
    for (const auto& hz : hitZones_) {
        if (PointInRectF(x, y, hz.rect)) {
            hit = hz.id;
            break;
        }
    }
    if (hit != hoveredHitId_) {
        hoveredHitId_ = hit;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void StudioWindow::OnMouseUp() {
    if (draggingSlider_ != SliderId::None) {
        draggingSlider_ = SliderId::None;
        ReleaseCapture();
        if (cfg_) SaveAppConfig(*cfg_);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void StudioWindow::OnMouseWheel(short delta) {
    if (currentTab_ == Tab::Library && maxLibraryScrollY_ > 0.0f) {
        libraryScrollY_ = std::clamp(libraryScrollY_ - static_cast<float>(delta) * 0.45f, 0.0f, maxLibraryScrollY_);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void StudioWindow::OpenFilePickerDialog() {
    ComPtr<IFileOpenDialog> dlg;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IFileOpenDialog), reinterpret_cast<void**>(dlg.GetAddressOf()));
    if (FAILED(hr) || !dlg) return;

    const COMDLG_FILTERSPEC filters[] = {
        {L"Видеофайлы живых обоев (*.mp4; *.webm; *.mkv; *.mov; *.avi; *.wmv)", L"*.mp4;*.m4v;*.webm;*.mkv;*.mov;*.avi;*.wmv"},
        {L"MP4 Видео (*.mp4)", L"*.mp4"},
        {L"Все файлы (*.*)", L"*.*"},
    };
    dlg->SetFileTypes(3, filters);
    dlg->SetTitle(L"Выберите MP4 видео для установки на рабочий стол");

    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts))) {
        dlg->SetOptions(opts | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT);
    }

    if (SUCCEEDED(dlg->Show(hwnd_))) {
        ComPtr<IShellItemArray> items;
        if (SUCCEEDED(dlg->GetResults(items.GetAddressOf())) && items) {
            DWORD count = 0;
            items->GetCount(&count);
            std::wstring firstChosen;
            for (DWORD i = 0; i < count; ++i) {
                ComPtr<IShellItem> si;
                if (SUCCEEDED(items->GetItemAt(i, si.GetAddressOf())) && si) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(si->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                        std::wstring ws(path);
                        CoTaskMemFree(path);
                        AddVideoFile(ws, false);
                        if (firstChosen.empty()) firstChosen = ws;
                    }
                }
            }
            if (!firstChosen.empty()) {
                ApplyWallpaperByPath(firstChosen);
            }
        }
    }
}

void StudioWindow::OpenFolderPickerDialog() {
    ComPtr<IFileOpenDialog> dlg;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IFileOpenDialog), reinterpret_cast<void**>(dlg.GetAddressOf()));
    if (FAILED(hr) || !dlg) return;

    dlg->SetTitle(L"Выберите папку с MP4 обоями");
    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts))) {
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    }
    if (SUCCEEDED(dlg->Show(hwnd_))) {
        ComPtr<IShellItem> si;
        if (SUCCEEDED(dlg->GetResult(si.GetAddressOf())) && si) {
            PWSTR path = nullptr;
            if (SUCCEEDED(si->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                std::wstring folder(path);
                CoTaskMemFree(path);
                ImportFolderVideos(folder, true);
            }
        }
    }
}

void StudioWindow::HandleDroppedFiles(HDROP hDrop) {
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::wstring lastDroppedVideo;
    for (UINT i = 0; i < count; ++i) {
        UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
        if (len == 0) continue;
        std::wstring path(len + 1, L'\0');
        DragQueryFileW(hDrop, i, &path[0], len + 1);
        path.resize(len);

        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            ImportFolderVideos(path, true);
        } else if (AddVideoFile(path, false)) {
            lastDroppedVideo = path;
        }
    }
    DragFinish(hDrop);

    if (!lastDroppedVideo.empty()) {
        currentTab_ = Tab::Library;
        ApplyWallpaperByPath(lastDroppedVideo);
    }
}

void StudioWindow::SetupTrayIcon() {
    if (!hwnd_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayIconUid;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(hInstance_, MAKEINTRESOURCEW(IDI_WALLPULSE_ICON));
    if (!nid.hIcon) nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"WallPulse — Живые MP4 обои");

    if (trayAdded_) {
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    } else {
        trayAdded_ = (Shell_NotifyIconW(NIM_ADD, &nid) != FALSE);
    }
}

void StudioWindow::RemoveTrayIcon() {
    if (!trayAdded_ || !hwnd_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayIconUid;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    trayAdded_ = false;
}

void StudioWindow::ShowFirstMinimizeBalloon() {
    if (!cfg_ || cfg_->shownTrayHint || !trayAdded_ || !hwnd_) return;
    cfg_->shownTrayHint = true;
    SaveAppConfig(*cfg_);

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayIconUid;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcscpy_s(nid.szInfoTitle, L"WallPulse работает в фоне");
    wcscpy_s(nid.szInfo, L"Живые обои продолжают играть на рабочем столе. Нажмите на значок в трее для настроек.");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void StudioWindow::ShowTrayContextMenu() {
    if (!hwnd_ || !cfg_) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, CMD_TRAY_OPEN, L"Открыть WallPulse Studio");
    AppendMenuW(menu, MF_STRING, CMD_TRAY_PICK_MP4, L"Выбрать MP4 видео...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_TRAY_PLAY_PAUSE,
                cfg_->userPaused ? L"Продолжить воспроизведение" : L"Пауза обоев");
    AppendMenuW(menu, MF_STRING | (!cfg_->muted && cfg_->volume > 0.001f ? MF_CHECKED : 0),
                CMD_TRAY_MUTE, L"Звук обоев");
    AppendMenuW(menu, MF_STRING, CMD_TRAY_STOP_WALL, L"Убрать живые обои с фона");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (cfg_->startWithWindows ? MF_CHECKED : 0),
                CMD_TRAY_AUTOSTART, L"Запускать вместе с Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_TRAY_EXIT, L"Выход");

    SetMenuDefaultItem(menu, CMD_TRAY_OPEN, FALSE);

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

bool StudioWindow::CaptureWindowClientToBmp(const std::wstring& outBmpPath) {
    if (!hwnd_) return false;
    Render();

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    uint32_t w = static_cast<uint32_t>(std::max<LONG>(1, rc.right - rc.left));
    uint32_t h = static_cast<uint32_t>(std::max<LONG>(1, rc.bottom - rc.top));

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC wndDC = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(wndDC);
    void* bits = nullptr;
    HBITMAP hDib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hDib || !bits) {
        if (hDib) DeleteObject(hDib);
        DeleteDC(memDC);
        ReleaseDC(hwnd_, wndDC);
        return false;
    }

    HGDIOBJ old = SelectObject(memDC, hDib);
    if (!PrintWindow(hwnd_, memDC, PW_CLIENTONLY | PW_RENDERFULLCONTENT)) {
        BitBlt(memDC, 0, 0, static_cast<int>(w), static_cast<int>(h), wndDC, 0, 0, SRCCOPY);
    }
    GdiFlush();

    bool ok = SaveBmpBGRA(outBmpPath, w, h, static_cast<const uint8_t*>(bits));

    SelectObject(memDC, old);
    DeleteObject(hDib);
    DeleteDC(memDC);
    ReleaseDC(hwnd_, wndDC);
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

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT StudioWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreatedMsg_ != 0 && msg == taskbarCreatedMsg_) {
        SetupTrayIcon();
        if (desktopHost_) desktopHost_->DiscoverAndPrepare();
        if (engine_ && cfg_) engine_->CheckDesktopAttachment(*cfg_);
        return 0;
    }

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd_, &ps);
            Render();
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_SIZE:
            if (rt_ && wParam != SIZE_MINIMIZED) {
                UINT w = LOWORD(lParam);
                UINT h = HIWORD(lParam);
                rt_->Resize(D2D1::SizeU(std::max<UINT>(1, w), std::max<UINT>(1, h)));
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = static_cast<LONG>(920 * dpiScale_);
            mmi->ptMinTrackSize.y = static_cast<LONG>(600 * dpiScale_);
            return 0;
        }

        case WM_DPICHANGED: {
            dpiScale_ = static_cast<float>(HIWORD(wParam)) / 96.0f;
            if (rt_) rt_->SetDpi(96.0f * dpiScale_, 96.0f * dpiScale_);
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            if (suggested) {
                SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_DISPLAYCHANGE:
            if (engine_ && cfg_) {
                if (desktopHost_) desktopHost_->DiscoverAndPrepare();
                engine_->ApplySettings(*cfg_, true);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            OnMouseDown(GET_X_LPARAM(lParam) / dpiScale_, GET_Y_LPARAM(lParam) / dpiScale_, false);
            return 0;

        case WM_RBUTTONDOWN:
            OnMouseDown(GET_X_LPARAM(lParam) / dpiScale_, GET_Y_LPARAM(lParam) / dpiScale_, true);
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam) / dpiScale_, GET_Y_LPARAM(lParam) / dpiScale_);
            return 0;

        case WM_LBUTTONUP:
            OnMouseUp();
            return 0;

        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;

        case WM_DROPFILES:
            HandleDroppedFiles(reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_SHOW_EXISTING:
            ShowAndActivate();
            return 0;

        case WM_COPYDATA: {
            auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
            if (cds && cds->dwData == 0x57504D50 && cds->lpData && cds->cbData >= sizeof(wchar_t)) {
                std::wstring receivedPath(
                    static_cast<const wchar_t*>(cds->lpData),
                    cds->cbData / sizeof(wchar_t));
                while (!receivedPath.empty() && receivedPath.back() == L'\0') {
                    receivedPath.pop_back();
                }
                if (!receivedPath.empty()) {
                    AddVideoFile(receivedPath, true);
                    ShowAndActivate();
                }
                return 1;
            }
            return 0;
        }

        case WM_TRAYICON:
            if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                ShowAndActivate();
            } else if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
                ShowTrayContextMenu();
            }
            return 0;

        case WM_COMMAND:
            if (cfg_) {
                switch (LOWORD(wParam)) {
                    case CMD_TRAY_OPEN:
                        ShowAndActivate();
                        break;
                    case CMD_TRAY_PICK_MP4:
                        ShowAndActivate();
                        OpenFilePickerDialog();
                        break;
                    case CMD_TRAY_PLAY_PAUSE:
                        cfg_->userPaused = !cfg_->userPaused;
                        if (engine_) engine_->ApplySettings(*cfg_, false);
                        SaveAppConfig(*cfg_);
                        break;
                    case CMD_TRAY_MUTE:
                        cfg_->muted = !cfg_->muted;
                        if (!cfg_->muted && cfg_->volume <= 0.02f) cfg_->volume = 0.50f;
                        if (engine_) engine_->ApplySettings(*cfg_, false);
                        SaveAppConfig(*cfg_);
                        break;
                    case CMD_TRAY_STOP_WALL:
                        StopActiveWallpaper();
                        break;
                    case CMD_TRAY_AUTOSTART:
                        cfg_->startWithWindows = !cfg_->startWithWindows;
                        SetAutoStartInRegistry(cfg_->startWithWindows);
                        SaveAppConfig(*cfg_);
                        break;
                    case CMD_TRAY_EXIT:
                        PostQuitMessage(0);
                        break;
                }
            }
            return 0;

        case WM_CLOSE:
            if (cfg_ && cfg_->closeToTray && engine_ && engine_->IsLoaded()) {
                HideToTray();
                return 0;
            }
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon();
            return 0;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

}  // namespace wp
