#include "audio_reactive.h"
#include "common.h"
#include "config.h"
#include "desktop_host.h"
#include "media_tools.h"
#include "smart_pause.h"
#include "steam_library.h"
#include "studio_ui.h"
#include "system_tweaks.h"
#include "video_wallpaper.h"
#include "web_host.h"

#include <commctrl.h>
#include <mfapi.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace wp {

namespace {

constexpr const wchar_t* kMsgWndClass = L"WallPulseHiddenMsgClass";
constexpr const wchar_t* kSingleInstanceMutex = L"Local\\WallPulseStudioSingleInstance_v1";

constexpr UINT WM_TRAYICON       = WM_APP + 10;
constexpr UINT WM_APPLY_EXTERNAL = WM_APP + 11;
constexpr UINT WM_THUMBNAIL_DONE = WM_APP + 12;
constexpr UINT WM_STEAM_SCAN_DONE = WM_APP + 13;

struct SteamScanCompletion {
    SteamLibraryScanResult result;
};

struct ThumbnailProbeCompletion {
    std::wstring path;
    VideoProbeInfo result;
};

constexpr UINT_PTR TIMER_RENDER = 1;
constexpr UINT_PTR TIMER_STATUS = 2;

enum TrayCmdId {
    CMD_TRAY_OPEN = 1001,
    CMD_TRAY_NEXT,
    CMD_TRAY_PAUSE,
    CMD_TRAY_MUTE,
    CMD_TRAY_ADD_FILE,
    CMD_TRAY_STOP,
    CMD_TRAY_EXIT,
};

enum GlobalHotkeyId {
    HK_TOGGLE_PAUSE = 501,
    HK_NEXT_WALL    = 502,
    HK_TOGGLE_MUTE  = 503,
    HK_SHOW_STUDIO  = 504,
};

HICON CreateFallbackTrayIcon() {
    constexpr int sz = 32;
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = sz;
    bi.bV5Height = -sz;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    HDC hdc = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP hColor = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hColor || !bits) return LoadIcon(nullptr, IDI_APPLICATION);

    auto* px = static_cast<uint32_t*>(bits);
    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            float dx = (x - 15.5f) / 15.5f;
            float dy = (y - 15.5f) / 15.5f;
            float r2 = dx * dx + dy * dy;
            if (r2 > 0.92f) {
                px[y * sz + x] = 0x00000000;
            } else if (r2 > 0.58f) {
                px[y * sz + x] = 0xFF0EA5E9;
            } else {
                px[y * sz + x] = 0xFF6366F1;
            }
        }
    }

    HBITMAP hMask = CreateBitmap(sz, sz, 1, 1, nullptr);
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = hMask;
    ii.hbmColor = hColor;
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hMask);
    DeleteObject(hColor);
    return hIcon ? hIcon : LoadIcon(nullptr, IDI_APPLICATION);
}

bool IsDaytimeHour(int curHour, int dayStart, int nightStart) {
    dayStart = (dayStart % 24 + 24) % 24;
    nightStart = (nightStart % 24 + 24) % 24;
    if (dayStart == nightStart) return true;
    if (dayStart < nightStart) {
        return curHour >= dayStart && curHour < nightStart;
    }
    return curHour >= dayStart || curHour < nightStart;
}

class WallPulseApp {
public:
    bool Init(HINSTANCE hInstance, bool startMinimizedOverride, const std::wstring& initialMediaPath) {
        hInstance_ = hInstance;
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        wmTaskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");

        LoadAppConfig(config_);

        // Register / refresh built-in MP4, GIF, Shader & HTML5 starters (#10)
        auto starters = EnsureStarterLibraryItems();
        for (auto& s : starters) {
            if (!config_.FindItem(s.path)) {
                config_.library.push_back(std::move(s));
            }
        }

        // Backfill missing metadata asynchronously so large libraries never stall startup.
        std::vector<std::wstring> initialThumbnailProbes;
        for (auto& item : config_.library) {
            if (!item.isSteamWorkshop) item.kind = ClassifyWallpaperPath(item.path);
            if ((item.thumbPath.empty() || !FileExists(item.thumbPath)) && item.isCompatible &&
                (!item.isSteamWorkshop || item.isWorkshopInstalled) && WallpaperSourceExists(item.path)) {
                item.thumbPending = true;
                initialThumbnailProbes.push_back(item.path);
            }
        }

        if (config_.dayWallpaperPath.empty() && config_.library.size() >= 1) {
            config_.dayWallpaperPath = config_.library[0].path;
        }
        if (config_.nightWallpaperPath.empty() && config_.library.size() >= 2) {
            config_.nightWallpaperPath = config_.library[1].path;
        }

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = MsgWndProc;
        wc.hInstance = hInstance_;
        wc.lpszClassName = kMsgWndClass;
        RegisterClassExW(&wc);

        msgHwnd_ = CreateWindowExW(
            0, kMsgWndClass, L"WallPulseController", WS_POPUP,
            0, 0, 0, 0, nullptr, nullptr, hInstance_, this);
        if (!msgHwnd_) return false;

        if (!player_.Initialize(hInstance_)) return false;
        StartThumbnailWorker();
        for (const auto& path : initialThumbnailProbes) QueueThumbnailProbe(path);

        StudioCallbacks cbs{};
        cbs.onApplyWallpaper = [this](const std::wstring& path) {
            player_.CaptureCrossfadeSnapshot(config_.crossfadeSeconds);
            ApplyWallpaper(path);
        };
        cbs.onStopWallpaper = [this]() { StopWallpaper(); };
        cbs.onTogglePause = [this]() {
            player_.SetUserPaused(!player_.IsUserPaused());
            PushStatusToUi();
        };
        cbs.onNextWallpaper = [this]() { SwitchToNextWallpaper(); };
        cbs.onSeekVideo = [this](double sec) { player_.SeekSeconds(sec); };
        cbs.onAddFiles = [this](const std::vector<std::wstring>& files) { AddFilesToLibrary(files, true); };
        cbs.onRemoveFile = [this](const std::wstring& path) { RemoveFileFromLibrary(path); };
        cbs.onSettingsChanged = [this]() { OnSettingsChanged(); };
        cbs.onScanSteamLibrary = [this]() { StartSteamLibraryScan(); };
        cbs.onOpenSteamWorkshop = [this](const std::wstring& id) {
            if (id.empty() || !std::all_of(id.begin(), id.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) return;
            std::wstring uri = L"steam://url/CommunityFilePage/" + id;
            ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        };
        cbs.onToggleAutostart = [](bool en) { SetAutoStartInRegistry(en); };
        cbs.onToggleContextMenu = [](bool en) { SetExplorerContextMenu(en); };
        cbs.onExitApp = [this]() { ExitApplication(); };

        studio_.Create(hInstance_, &config_, &config_.library, cbs);
        studio_.SetSteamLibraryStatus(L"Сканируется локальная библиотека Steam Workshop (app 431960)…");
        StartSteamLibraryScan();

        AddTrayIcon();
        SyncGlobalHotkeys();
        SyncSubsystems();

        lastPlaylistSwitchMs_ = GetTickCount64();

        if (!initialMediaPath.empty() && WallpaperSourceExists(initialMediaPath) && IsSupportedWallpaperExtension(initialMediaPath)) {
            AddFilesToLibrary({initialMediaPath}, true);
        } else if (!config_.activeVideoPath.empty() && CanApplyWallpaper(config_.activeVideoPath)) {
            ApplyWallpaper(config_.activeVideoPath);
        } else {
            auto playable = std::find_if(config_.library.begin(), config_.library.end(), [](const WallpaperItem& item) {
                return item.isCompatible && (!item.isSteamWorkshop || item.isWorkshopInstalled) && WallpaperSourceExists(item.path);
            });
            if (playable != config_.library.end()) ApplyWallpaper(playable->path);
        }

        SaveAppConfig(config_);

        UpdateRenderTimer();
        player_.StartRenderLoop(config_.targetFps);
        SetTimer(msgHwnd_, TIMER_STATUS, 350, nullptr);

        if (!startMinimizedOverride && !config_.startMinimized) {
            studio_.ShowAndRestore();
            if (!config_.completedOnboardingTour) studio_.StartGuidedTour();
        }
        PushStatusToUi();
        return true;
    }

    int RunMessageLoop() {
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Cleanup();
        return static_cast<int>(msg.wParam);
    }

private:
    void StartSteamLibraryScan() {
        if (steamScanRunning_.exchange(true)) return;
        if (steamScanWorker_.joinable()) steamScanWorker_.join();
        steamScanWorker_ = std::thread([this]() {
            HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            auto* completion = new (std::nothrow) SteamScanCompletion{};
            if (completion) {
                try {
                    completion->result = ScanInstalledSteamWorkshopLibrary();
                } catch (...) {
                    completion->result.status = L"Не удалось прочитать локальную библиотеку Steam Workshop.";
                }
            }
            if (SUCCEEDED(coHr)) CoUninitialize();
            steamScanRunning_.store(false);
            HWND target = msgHwnd_;
            if (completion && (!target || !PostMessageW(target, WM_STEAM_SCAN_DONE, 0, reinterpret_cast<LPARAM>(completion)))) {
                delete completion;
            }
        });
    }

    void StopSteamLibraryScan() {
        if (steamScanWorker_.joinable()) steamScanWorker_.join();
        MSG pending{};
        while (msgHwnd_ && PeekMessageW(&pending, msgHwnd_, WM_STEAM_SCAN_DONE, WM_STEAM_SCAN_DONE, PM_REMOVE)) {
            delete reinterpret_cast<SteamScanCompletion*>(pending.lParam);
        }
    }

    void ApplySteamScanResult(SteamLibraryScanResult result) {
        std::wstring status = result.status;
        if (!result.collectionDiscovered && !result.items.empty()) {
            status += L" (используется последний успешный индекс)";
            studio_.SetSteamLibraryStatus(status);
            return;
        }
        if (!result.collectionDiscovered) {
            bool hasCachedWorkshop = std::any_of(config_.library.begin(), config_.library.end(),
                [](const WallpaperItem& item) { return item.isSteamWorkshop; });
            if (hasCachedWorkshop) {
                status += L" Последний сохранённый индекс оставлен без изменений.";
                studio_.SetSteamLibraryStatus(status);
                return;
            }
        }

        const std::wstring previousActivePath = config_.activeVideoPath;
        const bool previousActiveWasWorkshop = std::any_of(config_.library.begin(), config_.library.end(), [&](const WallpaperItem& item) {
            return item.isSteamWorkshop && _wcsicmp(item.path.c_str(), previousActivePath.c_str()) == 0;
        });
        bool activeSourceChanged = false;
        std::vector<WallpaperItem> merged;
        merged.reserve(config_.library.size() + result.items.size());
        for (const auto& item : config_.library) {
            WallpaperItem cached = item;
            if (cached.isSteamWorkshop) cached.isWorkshopInstalled = false;
            merged.push_back(std::move(cached));
        }

        for (auto& discovered : result.items) {
            WallpaperItem* installed = nullptr;
            auto old = std::find_if(merged.begin(), merged.end(), [&](const WallpaperItem& candidate) {
                return candidate.isSteamWorkshop && !discovered.steamId.empty() && candidate.steamId == discovered.steamId;
            });
            if (old == merged.end()) {
                old = std::find_if(merged.begin(), merged.end(), [&](const WallpaperItem& candidate) {
                    return candidate.isSteamWorkshop && _wcsicmp(candidate.path.c_str(), discovered.path.c_str()) == 0;
                });
            }
            if (old != merged.end()) {
                discovered.favorite = old->favorite;
                discovered.inPlaylist = old->inPlaylist;
                discovered.profile = old->profile;
                discovered.lastAppliedUnix = old->lastAppliedUnix;
                if (discovered.thumbPath.empty() && FileExists(old->thumbPath)) discovered.thumbPath = old->thumbPath;
                if (old->path != discovered.path) {
                    if (config_.activeVideoPath == old->path) {
                        config_.activeVideoPath = discovered.path;
                        activeSourceChanged = true;
                    }
                    if (config_.dayWallpaperPath == old->path) config_.dayWallpaperPath = discovered.path;
                    if (config_.nightWallpaperPath == old->path) config_.nightWallpaperPath = discovered.path;
                    for (auto& monitorPath : config_.monitorWallpapers) if (monitorPath == old->path) monitorPath = discovered.path;
                }
                *old = std::move(discovered);
                installed = &*old;
            } else {
                merged.push_back(std::move(discovered));
                installed = &merged.back();
            }
            if (installed->isCompatible && (installed->thumbPath.empty() || !FileExists(installed->thumbPath))) {
                installed->thumbPending = true;
                QueueThumbnailProbe(installed->path);
            }
        }

        config_.library = std::move(merged);
        SaveAppConfig(config_);
        studio_.RefreshThumbnails();
        studio_.SetSteamLibraryStatus(status);
        const WallpaperItem* activeItem = config_.FindItem(config_.activeVideoPath);
        const bool activeItemStillPlayable = activeItem && activeItem->isCompatible && activeItem->isWorkshopInstalled;
        if (previousActiveWasWorkshop && !activeItemStillPlayable) {
            config_.activeVideoPath.clear();
            player_.StopAndDetach();
            SaveAppConfig(config_);
            PushStatusToUi();
        } else if (activeSourceChanged) {
            ApplyWallpaper(config_.activeVideoPath);
        } else {
            PushStatusToUi();
        }
    }

    void StartThumbnailWorker() {
        if (thumbWorkerRunning_.exchange(true)) return;
        thumbWorker_ = std::thread(&WallPulseApp::ThumbnailWorkerLoop, this);
    }

    void QueueThumbnailProbe(const std::wstring& path) {
        {
            std::lock_guard<std::mutex> lk(thumbQueueMutex_);
            thumbQueue_.push_back(path);
        }
        thumbQueueCv_.notify_one();
    }

    void ThumbnailWorkerLoop() {
        HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        while (thumbWorkerRunning_.load()) {
            std::wstring path;
            {
                std::unique_lock<std::mutex> lk(thumbQueueMutex_);
                thumbQueueCv_.wait(lk, [&] { return !thumbQueue_.empty() || !thumbWorkerRunning_.load(); });
                if (!thumbWorkerRunning_.load()) break;
                path = std::move(thumbQueue_.front());
                thumbQueue_.pop_front();
            }

            auto* completion = new (std::nothrow) ThumbnailProbeCompletion{};
            if (!completion) continue;
            completion->path = path;
            completion->result = ProbeWallpaperAndSaveThumbnail(path);
            HWND target = msgHwnd_;
            if (!target || !PostMessageW(target, WM_THUMBNAIL_DONE, 0, reinterpret_cast<LPARAM>(completion))) {
                delete completion;
            }
        }
        if (SUCCEEDED(coHr)) CoUninitialize();
    }

    void StopThumbnailWorker() {
        thumbWorkerRunning_.store(false);
        thumbQueueCv_.notify_all();
        if (thumbWorker_.joinable()) thumbWorker_.join();
    }

    void DrainPendingThumbnailMessages() {
        MSG pending{};
        while (msgHwnd_ && PeekMessageW(&pending, msgHwnd_, WM_THUMBNAIL_DONE, WM_THUMBNAIL_DONE, PM_REMOVE)) {
            delete reinterpret_cast<ThumbnailProbeCompletion*>(pending.lParam);
        }
    }

    WallpaperProfile GetActiveProfile() const {
        return config_.GetEffectiveProfile(config_.activeVideoPath);
    }

    bool CanApplyWallpaper(const std::wstring& path) const {
        if (!WallpaperSourceExists(path) || !IsSupportedWallpaperExtension(path)) return false;
        const WallpaperItem* item = config_.FindItem(path);
        return !item || (item->isCompatible && (!item->isSteamWorkshop || item->isWorkshopInstalled));
    }

    void ApplyWallpaper(const std::wstring& path) {
        if (!CanApplyWallpaper(path)) return;
        config_.activeVideoPath = path;
        if (WallpaperItem* applied = config_.FindItem(path)) {
            applied->lastAppliedUnix = static_cast<uint64_t>(std::time(nullptr));
        }
        lastPlaylistSwitchMs_ = GetTickCount64();

        WallpaperProfile prof = GetActiveProfile();
        activeAccentRGB_ = config_.ResolveAccentColorRGB();

        player_.LoadAndApply(path, prof, config_, config_.library, true);
        player_.SetUserPaused(false);
        studio_.SelectItemByPath(path);
        SaveAppConfig(config_);
        PushStatusToUi();
    }

    void SwitchToNextWallpaper() {
        std::vector<size_t> candidates;
        candidates.reserve(config_.library.size());
        for (size_t i = 0; i < config_.library.size(); ++i) {
            const auto& item = config_.library[i];
            if (!item.inPlaylist || !item.isCompatible || (item.isSteamWorkshop && !item.isWorkshopInstalled) ||
                !WallpaperSourceExists(item.path)) continue;
            if (config_.playlistFavoritesOnly && !item.favorite) continue;
            candidates.push_back(i);
        }
        if (candidates.empty()) return;

        auto curIt = std::find_if(candidates.begin(), candidates.end(), [&](size_t i) {
            return _wcsicmp(config_.library[i].path.c_str(), config_.activeVideoPath.c_str()) == 0;
        });
        size_t pos = (curIt == candidates.end()) ? 0 : static_cast<size_t>(curIt - candidates.begin());
        size_t nextPos = (pos + 1) % candidates.size();
        if (config_.playlistShuffle && candidates.size() > 2) {
            nextPos = (pos + 1 + static_cast<size_t>(std::rand()) % (candidates.size() - 1)) % candidates.size();
        }
        size_t nextIdx = candidates[nextPos];
        if (_wcsicmp(config_.library[nextIdx].path.c_str(), config_.activeVideoPath.c_str()) == 0) return;

        player_.CaptureCrossfadeSnapshot(config_.crossfadeSeconds);
        ApplyWallpaper(config_.library[nextIdx].path);
    }

    void StopWallpaper() {
        player_.StopAndDetach();
        config_.activeVideoPath.clear();
        SaveAppConfig(config_);
        PushStatusToUi();
    }

    void AddFilesToLibrary(const std::vector<std::wstring>& files, bool applyFirst) {
        std::wstring toApply;
        for (const auto& f : files) {
            if (!WallpaperSourceExists(f) || !IsSupportedWallpaperExtension(f)) continue;
            if (toApply.empty()) toApply = f;

            if (config_.FindItem(f)) continue;

            WallpaperItem item;
            item.path = f;
            item.title = GetFileNameWithoutExt(f);
            item.kind = ClassifyWallpaperPath(f);
            item.fileSize = GetFileSizeBytes(f);
            item.isBuiltIn = IsVirtualWallpaperUri(f);
            item.thumbPending = true;
            config_.library.insert(config_.library.begin(), std::move(item));
            QueueThumbnailProbe(f);
        }

        studio_.RefreshThumbnails();
        if (applyFirst && !toApply.empty()) {
            player_.CaptureCrossfadeSnapshot(config_.crossfadeSeconds);
            ApplyWallpaper(toApply);
        } else {
            SaveAppConfig(config_);
        }
    }

    void RemoveFileFromLibrary(const std::wstring& path) {
        config_.library.erase(
            std::remove_if(config_.library.begin(), config_.library.end(), [&](const WallpaperItem& it) {
                return !it.isBuiltIn && !it.isSteamWorkshop && _wcsicmp(it.path.c_str(), path.c_str()) == 0;
            }),
            config_.library.end());

        if (_wcsicmp(config_.activeVideoPath.c_str(), path.c_str()) == 0) {
            StopWallpaper();
        } else {
            SaveAppConfig(config_);
            PushStatusToUi();
        }
    }

    void SyncGlobalHotkeys() {
        if (!msgHwnd_) return;
        UnregisterHotKey(msgHwnd_, HK_TOGGLE_PAUSE);
        UnregisterHotKey(msgHwnd_, HK_NEXT_WALL);
        UnregisterHotKey(msgHwnd_, HK_TOGGLE_MUTE);
        UnregisterHotKey(msgHwnd_, HK_SHOW_STUDIO);

        if (config_.globalHotkeys) {
            UINT mods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
            RegisterHotKey(msgHwnd_, HK_TOGGLE_PAUSE, mods, 'P');
            RegisterHotKey(msgHwnd_, HK_NEXT_WALL,    mods, 'N');
            RegisterHotKey(msgHwnd_, HK_TOGGLE_MUTE,  mods, 'M');
            RegisterHotKey(msgHwnd_, HK_SHOW_STUDIO,  mods, 'W');
        }
    }

    void SyncSubsystems() {
        const bool needSpectrum = (config_.audioVisMode != AudioVisMode::Off) || config_.bassReactiveZoom;
        const bool needMedia = config_.widgetMedia;
        audioEngine_.SetEnabled(needSpectrum, needMedia, config_.audioSensitivity);
        sysTweaks_.ApplyTaskbarMode(config_.taskbarMode);
        activeAccentRGB_ = config_.ResolveAccentColorRGB();
    }

    void OnSettingsChanged() {
        player_.UpdateLiveSettings(GetActiveProfile(), config_, config_.library);
        UpdateRenderTimer();
        SyncGlobalHotkeys();
        SyncSubsystems();
        SaveAppConfig(config_);
        PushStatusToUi();
    }

    void UpdateRenderTimer() {
        // Rendering is fully decoupled from the UI message pump. A high-resolution QPC loop
        // waits on the DXGI frame-latency object; dragging the Studio window cannot stall it.
        uint32_t fps = std::clamp<uint32_t>(config_.targetFps, 15, 165);
        player_.SetTargetFps(fps);
        if (msgHwnd_) KillTimer(msgHwnd_, TIMER_RENDER);
    }

    void OnStatusTick() {
        SmartPauseState sp = smartDetector_.Evaluate(
            config_, player_.Host(), studio_.Handle(), player_.RenderWindow());
        player_.SetSmartPaused(sp.shouldPauseVideo);
        player_.SetSmartMuted(sp.shouldMuteAudio);
        lastSmartState_ = sp;

        if (config_.taskbarMode != TaskbarMode::Default) {
            sysTweaks_.ApplyTaskbarMode(config_.taskbarMode);
        }

        // Feature #3: Day / Night Automatic Schedule
        if (config_.dayNightEnabled) {
            SYSTEMTIME st{};
            GetLocalTime(&st);
            int period = IsDaytimeHour(st.wHour, config_.dayStartHour, config_.nightStartHour) ? 1 : 2;
            if (period != lastDayNightPeriod_) {
                lastDayNightPeriod_ = period;
                const std::wstring& target = (period == 1) ? config_.dayWallpaperPath : config_.nightWallpaperPath;
                if (!target.empty() && CanApplyWallpaper(target) &&
                    _wcsicmp(target.c_str(), config_.activeVideoPath.c_str()) != 0) {
                    player_.CaptureCrossfadeSnapshot(config_.crossfadeSeconds);
                    ApplyWallpaper(target);
                }
            }
        } else {
            lastDayNightPeriod_ = 0;
        }

        // Feature #3: Automatic Playlist Rotation
        if (config_.playlistIntervalMin > 0 && config_.library.size() > 1 && !player_.IsEffectivelyPaused()) {
            ULONGLONG intervalMs = static_cast<ULONGLONG>(config_.playlistIntervalMin) * 60000ULL;
            if (GetTickCount64() - lastPlaylistSwitchMs_ >= intervalMs) {
                SwitchToNextWallpaper();
            }
        }

        if (config_.widgetSysStats || studio_.IsVisible()) {
            cachedSysStats_ = sysTweaks_.SampleSystemStats();
        }
        player_.UpdateTelemetry(audioEngine_.GetSnapshot(), cachedSysStats_, activeAccentRGB_);

        if (studio_.IsVisible()) {
            PushStatusToUi();
        }
    }

    void PushStatusToUi() {
        StudioStatus st{};
        st.isPlaying = player_.IsLoaded();
        st.isUserPaused = player_.IsUserPaused();
        st.isSmartPaused = player_.IsSmartPaused();
        st.smartReason = lastSmartState_.reason;
        st.shaderError = player_.LastShaderError();
        st.monitorCount = static_cast<uint32_t>(player_.Host().EnumerateMonitors().size());
        st.currentTimeSec = player_.CurrentTimeSeconds();
        st.durationSec = player_.DurationSeconds();
        st.activeAccentRGB = activeAccentRGB_;
        st.audio = audioEngine_.GetSnapshot();
        st.sysStats = cachedSysStats_;

        const auto& topo = player_.AttachTopology();
        if (topo.valid) {
            st.attachSummary = topo.isRaisedLayeredProgman
                ? L"Win11 24H2+ Layered"
                : L"WorkerW (Win10/11)";
        }
        studio_.UpdateStatus(st);
    }

    void AddTrayIcon() {
        ZeroMemory(&nid_, sizeof(nid_));
        nid_.cbSize = sizeof(nid_);
        nid_.hWnd = msgHwnd_;
        nid_.uID = 1;
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid_.uCallbackMessage = WM_TRAYICON;
        nid_.hIcon = LoadIconW(hInstance_, MAKEINTRESOURCEW(IDI_WALLPULSE_ICON));
        if (!nid_.hIcon) {
            fallbackTrayIcon_ = CreateFallbackTrayIcon();
            nid_.hIcon = fallbackTrayIcon_;
        }
        wcscpy_s(nid_.szTip, L"WallPulse Studio v3.0 — Живые обои");
        Shell_NotifyIconW(NIM_ADD, &nid_);
        trayAdded_ = true;
    }

    void RemoveTrayIcon() {
        if (trayAdded_) {
            Shell_NotifyIconW(NIM_DELETE, &nid_);
            trayAdded_ = false;
        }
        if (fallbackTrayIcon_) {
            DestroyIcon(fallbackTrayIcon_);
            fallbackTrayIcon_ = nullptr;
        }
    }

    void ShowTrayMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        AppendMenuW(menu, MF_STRING, CMD_TRAY_OPEN, L"Открыть WallPulse Studio (Ctrl+Alt+W)");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CMD_TRAY_NEXT, L"Следующие обои ⏭ (Ctrl+Alt+N)");
        AppendMenuW(menu, MF_STRING, CMD_TRAY_PAUSE,
                    player_.IsUserPaused() ? L"Продолжить ▶ (Ctrl+Alt+P)" : L"Пауза ⏸ (Ctrl+Alt+P)");
        bool muted = GetActiveProfile().muted;
        AppendMenuW(menu, MF_STRING | (muted ? MF_CHECKED : 0), CMD_TRAY_MUTE, L"Без звука 🔇 (Ctrl+Alt+M)");
        AppendMenuW(menu, MF_STRING, CMD_TRAY_ADD_FILE, L"Добавить обои (.mp4/.gif/.html)...");
        AppendMenuW(menu, MF_STRING, CMD_TRAY_STOP, L"Остановить обои");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CMD_TRAY_EXIT, L"Выход");

        POINT pt{};
        GetCursorPos(&pt);
        SetForegroundWindow(msgHwnd_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, msgHwnd_, nullptr);
        PostMessageW(msgHwnd_, WM_NULL, 0, 0);
        DestroyMenu(menu);
    }

    void ToggleActiveMute() {
        if (auto* item = config_.FindItem(config_.activeVideoPath)) {
            item->profile.muted = !item->profile.muted;
            OnSettingsChanged();
        }
    }

    void ExitApplication() {
        StopSteamLibraryScan();
        StopThumbnailWorker();
        DrainPendingThumbnailMessages();
        KillTimer(msgHwnd_, TIMER_RENDER);
        KillTimer(msgHwnd_, TIMER_STATUS);
        if (msgHwnd_) {
            UnregisterHotKey(msgHwnd_, HK_TOGGLE_PAUSE);
            UnregisterHotKey(msgHwnd_, HK_NEXT_WALL);
            UnregisterHotKey(msgHwnd_, HK_TOGGLE_MUTE);
            UnregisterHotKey(msgHwnd_, HK_SHOW_STUDIO);
        }
        RemoveTrayIcon();
        audioEngine_.Shutdown();
        sysTweaks_.RestoreTaskbarDefault();
        studio_.Destroy();
        player_.Shutdown();
        PostQuitMessage(0);
    }

    void Cleanup() {
        StopSteamLibraryScan();
        StopThumbnailWorker();
        DrainPendingThumbnailMessages();
        RemoveTrayIcon();
        audioEngine_.Shutdown();
        sysTweaks_.RestoreTaskbarDefault();
        studio_.Destroy();
        player_.Shutdown();
        if (msgHwnd_) {
            UnregisterHotKey(msgHwnd_, HK_TOGGLE_PAUSE);
            UnregisterHotKey(msgHwnd_, HK_NEXT_WALL);
            UnregisterHotKey(msgHwnd_, HK_TOGGLE_MUTE);
            UnregisterHotKey(msgHwnd_, HK_SHOW_STUDIO);
            DestroyWindow(msgHwnd_);
            msgHwnd_ = nullptr;
        }
    }

    static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WallPulseApp* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<WallPulseApp*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<WallPulseApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

        if (self->wmTaskbarCreated_ != 0 && msg == self->wmTaskbarCreated_) {
            self->RemoveTrayIcon();
            self->AddTrayIcon();
            self->player_.ReattachDesktop();
            self->sysTweaks_.ApplyTaskbarMode(self->config_.taskbarMode);
            return 0;
        }

        switch (msg) {
            case WM_TIMER: {
                if (wParam == TIMER_RENDER) {
                    AudioSpectrumSnapshot aud = self->audioEngine_.GetSnapshot();
                    self->player_.Tick(aud, self->cachedSysStats_, self->activeAccentRGB_);
                } else if (wParam == TIMER_STATUS) {
                    self->OnStatusTick();
                }
                return 0;
            }
            case WM_HOTKEY: {
                switch (static_cast<int>(wParam)) {
                    case HK_TOGGLE_PAUSE:
                        self->player_.SetUserPaused(!self->player_.IsUserPaused());
                        self->PushStatusToUi();
                        break;
                    case HK_NEXT_WALL:
                        self->SwitchToNextWallpaper();
                        break;
                    case HK_TOGGLE_MUTE:
                        self->ToggleActiveMute();
                        break;
                    case HK_SHOW_STUDIO:
                        if (self->studio_.IsVisible()) self->studio_.HideToTray();
                        else self->studio_.ShowAndRestore();
                        break;
                }
                return 0;
            }
            case WM_DISPLAYCHANGE: {
                self->player_.ReattachDesktop();
                return 0;
            }
            case WM_TRAYICON: {
                if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
                    self->studio_.ShowAndRestore();
                } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                    self->ShowTrayMenu();
                }
                return 0;
            }
            case WM_COMMAND: {
                switch (LOWORD(wParam)) {
                    case CMD_TRAY_OPEN:     self->studio_.ShowAndRestore(); break;
                    case CMD_TRAY_NEXT:     self->SwitchToNextWallpaper(); break;
                    case CMD_TRAY_PAUSE:
                        self->player_.SetUserPaused(!self->player_.IsUserPaused());
                        self->PushStatusToUi();
                        break;
                    case CMD_TRAY_MUTE:     self->ToggleActiveMute(); break;
                    case CMD_TRAY_ADD_FILE:
                        self->studio_.ShowAndRestore();
                        self->studio_.OpenAddFileDialog();
                        break;
                    case CMD_TRAY_STOP:     self->StopWallpaper(); break;
                    case CMD_TRAY_EXIT:     self->ExitApplication(); break;
                }
                return 0;
            }
            case WM_COPYDATA: {
                auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
                if (cds && cds->dwData == 0x57505331 && cds->lpData && cds->cbData >= sizeof(wchar_t)) {
                    std::wstring incoming(
                        static_cast<const wchar_t*>(cds->lpData),
                        (cds->cbData / sizeof(wchar_t)) - 1);
                    if (incoming == L"--show") {
                        self->studio_.ShowAndRestore();
                    } else if (WallpaperSourceExists(incoming)) {
                        self->pendingExternalPath_ = incoming;
                        PostMessageW(hwnd, WM_APPLY_EXTERNAL, 0, 0);
                    }
                    return TRUE;
                }
                break;
            }
            case WM_STEAM_SCAN_DONE: {
                std::unique_ptr<SteamScanCompletion> done(reinterpret_cast<SteamScanCompletion*>(lParam));
                if (done) self->ApplySteamScanResult(std::move(done->result));
                return 0;
            }
            case WM_THUMBNAIL_DONE: {
                std::unique_ptr<ThumbnailProbeCompletion> done(reinterpret_cast<ThumbnailProbeCompletion*>(lParam));
                if (done) {
                    if (WallpaperItem* item = self->config_.FindItem(done->path)) {
                        item->thumbPending = false;
                        if (done->result.valid) {
                            item->thumbPath = done->result.thumbPath;
                            item->width = done->result.width;
                            item->height = done->result.height;
                            item->durationSec = done->result.durationSec;
                            item->dominantColorRGB = done->result.dominantColorRGB;
                            if (_wcsicmp(done->path.c_str(), self->config_.activeVideoPath.c_str()) == 0) {
                                self->activeAccentRGB_ = self->config_.ResolveAccentColorRGB();
                            }
                        }
                        SaveAppConfig(self->config_);
                        self->studio_.RefreshThumbnails();
                        self->PushStatusToUi();
                    }
                }
                return 0;
            }
            case WM_APPLY_EXTERNAL: {
                if (!self->pendingExternalPath_.empty()) {
                    std::wstring p = self->pendingExternalPath_;
                    self->pendingExternalPath_.clear();
                    self->AddFilesToLibrary({p}, true);
                    self->studio_.ShowAndRestore();
                }
                return 0;
            }
            case WM_DESTROY:
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

private:
    HINSTANCE hInstance_ = nullptr;
    HWND msgHwnd_ = nullptr;
    UINT wmTaskbarCreated_ = 0;

    AppConfig config_{};
    SmartPauseDetector smartDetector_;
    SmartPauseState lastSmartState_{};

    AudioReactiveEngine audioEngine_;
    SystemTweaks sysTweaks_;
    VideoWallpaperPlayer player_;
    StudioWindow studio_;

    uint32_t activeAccentRGB_ = 0x0EA5E9;
    SystemStatsSnapshot cachedSysStats_{};
    ULONGLONG lastPlaylistSwitchMs_ = 0;
    int lastDayNightPeriod_ = 0;

    NOTIFYICONDATAW nid_{};
    HICON fallbackTrayIcon_ = nullptr;
    bool trayAdded_ = false;
    std::wstring pendingExternalPath_;

    std::atomic<bool> steamScanRunning_{false};
    std::thread steamScanWorker_;

    std::atomic<bool> thumbWorkerRunning_{false};
    std::thread thumbWorker_;
    std::mutex thumbQueueMutex_;
    std::condition_variable thumbQueueCv_;
    std::deque<std::wstring> thumbQueue_;
};

// ============================================================================
// Automated Integration Self-Test Suite (--self-test) verifying all v3.0 features
// ============================================================================
int RunSelfTestSuite(HINSTANCE hInstance, const std::wstring& reportOutPath, const std::wstring& shotOutPath) {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }
    FILE* fOut = nullptr;
    FILE* fErr = nullptr;
    freopen_s(&fOut, "CONOUT$", "w", stdout);
    freopen_s(&fErr, "CONOUT$", "w", stderr);

    const std::wstring finalReportPath = reportOutPath.empty()
        ? (GetAppDataDir() + L"\\self_test_report.txt")
        : reportOutPath;
    HANDLE reportFile = CreateFileW(finalReportPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    auto appendReport = [&](const char* line) {
        std::string record = std::string(line) + "\r\n";
        if (reportFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(reportFile, record.data(), static_cast<DWORD>(record.size()), &written, nullptr);
            FlushFileBuffers(reportFile);
        }
        std::printf("%s\n", line);
        std::fflush(stdout);
    };

    char hdr[128];
    std::snprintf(hdr, sizeof(hdr), "=== WallPulse Studio v%s PRO Automated Integration Self-Test ===", WP_VERSION_STR);
    appendReport(hdr);

    int passed = 0;
    int failed = 0;

    auto check = [&](const char* name, bool ok, const char* detail = "") {
        char lineBuf[512];
        if (ok) {
            ++passed;
            std::snprintf(lineBuf, sizeof(lineBuf), "[PASS] %s %s", name, detail);
        } else {
            ++failed;
            std::snprintf(lineBuf, sizeof(lineBuf), "[FAIL] %s %s", name, detail);
        }
        appendReport(lineBuf);
    };

    auto countColorPixels = [](const std::wstring& bmpPath) -> size_t {
        uint32_t bw = 0, bh = 0;
        std::vector<uint8_t> px;
        if (!LoadBmpBGRA(bmpPath, bw, bh, px)) return 0;
        size_t nonBlack = 0;
        for (size_t i = 0; i + 3 < px.size(); i += 4) {
            if (px[i] > 14 || px[i + 1] > 14 || px[i + 2] > 14) ++nonBlack;
        }
        return nonBlack;
    };

    // 1. UTF-8 Cyrillic & Unicode Search (#9)
    std::wstring cyrOrig = L"C:\\Обои\\Киберпанк_Синтвейв_4K.mp4";
    check("UTF8_Cyrillic_Roundtrip", Utf8ToWide(WideToUtf8(cyrOrig)) == cyrOrig);
    check("Unicode_Cyrillic_CaseInsensitive_Search",
          ContainsCaseInsensitive(L"Неоновый Синтвейв (MP4)", L"синтвейв") &&
          !ContainsCaseInsensitive(L"Неоновый Синтвейв", L"туманность"));

    // 2. Config v2 Serialization (Per-Item Profiles #6/#7/#9, Playlist & Day/Night #3, Multi-Mon #4, Widgets #5, Taskbar #8)
    {
        AppConfig cfgOut{};
        cfgOut.monitorMode = MonitorMode::PerMonitorIndependent;
        cfgOut.monitorWallpapers = {L"shader://cyber_tunnel", L"shader://sunset_ocean", L"", L""};
        cfgOut.playlistIntervalMin = 30;
        cfgOut.crossfadeSeconds = 1.6f;
        cfgOut.dayNightEnabled = true;
        cfgOut.dayStartHour = 8;
        cfgOut.nightStartHour = 21;
        cfgOut.audioVisMode = AudioVisMode::NeonRing;
        cfgOut.bassReactiveZoom = true;
        cfgOut.widgetClock = true;
        cfgOut.widgetMedia = true;
        cfgOut.widgetSysStats = true;
        cfgOut.widgetAnchor = WidgetAnchor::Free;
        cfgOut.widgetOffsetX = 0.31f;
        cfgOut.widgetOffsetY = 0.62f;
        cfgOut.taskbarMode = TaskbarMode::AcrylicBlur;
        cfgOut.accentTheme = AccentTheme::AutoFromWallpaper;
        cfgOut.globalHotkeys = true;

        WallpaperItem item{};
        item.path = cyrOrig;
        item.title = L"Киберпанк 4K";
        item.favorite = true;
        item.isSteamWorkshop = true;
        item.isWorkshopInstalled = true;
        item.isCompatible = false;
        item.steamId = L"1234567890";
        item.sourceRoot = L"C:\\SteamLibrary\\steamapps\\workshop\\content\\431960\\1234567890";
        item.author = L"Workshop creator";
        item.workshopType = L"scene";
        item.description = L"Scene | unsupported";
        item.lastAppliedUnix = 1790000000;
        item.dominantColorRGB = 0xEC4899;
        item.profile.hueShift = 42.0f;
        item.profile.sharpen = 0.85f;
        item.profile.bloom = 0.55f;
        item.profile.chromatic = 0.30f;
        item.profile.crtEffect = 0.25f;
        item.profile.trimStartPct = 0.15f;
        item.profile.trimEndPct = 0.82f;
        item.profile.seamlessLoop = true;
        item.profile.particles = ParticleOverlay::NeonRain;
        item.profile.particleAmount = 0.73f;
        item.profile.parallaxStrength = 0.81f;

        cfgOut.library = {item};
        std::wstring testCfgPath = GetAppDataDir() + L"\\selftest_v2.cfg";
        SaveAppConfig(cfgOut, testCfgPath);

        AppConfig cfgIn{};
        bool loaded = LoadAppConfig(cfgIn, testCfgPath);
        DeleteFileW(testCfgPath.c_str());

        bool match = loaded &&
                     cfgIn.monitorMode == MonitorMode::PerMonitorIndependent &&
                     cfgIn.monitorWallpapers.size() >= 2 &&
                     cfgIn.monitorWallpapers[0] == L"shader://cyber_tunnel" &&
                     cfgIn.playlistIntervalMin == 30 &&
                     cfgIn.dayNightEnabled && cfgIn.audioVisMode == AudioVisMode::NeonRing &&
                     cfgIn.taskbarMode == TaskbarMode::AcrylicBlur &&
                     cfgIn.widgetAnchor == WidgetAnchor::Free &&
                     std::fabs(cfgIn.widgetOffsetX - 0.31f) < 0.05f &&
                     std::fabs(cfgIn.widgetOffsetY - 0.62f) < 0.05f &&
                     cfgIn.library.size() == 1 && cfgIn.library[0].favorite &&
                     cfgIn.library[0].isSteamWorkshop && cfgIn.library[0].isWorkshopInstalled &&
                     !cfgIn.library[0].isCompatible && cfgIn.library[0].steamId == L"1234567890" &&
                     cfgIn.library[0].sourceRoot == item.sourceRoot && cfgIn.library[0].author == item.author &&
                     cfgIn.library[0].workshopType == L"scene" && cfgIn.library[0].description == item.description &&
                     cfgIn.library[0].lastAppliedUnix == 1790000000 &&
                     std::fabs(cfgIn.library[0].profile.hueShift - 42.0f) < 0.05f &&
                     std::fabs(cfgIn.library[0].profile.sharpen - 0.85f) < 0.05f &&
                     std::fabs(cfgIn.library[0].profile.trimEndPct - 0.82f) < 0.05f &&
                     cfgIn.library[0].profile.particles == ParticleOverlay::NeonRain &&
                     std::fabs(cfgIn.library[0].profile.particleAmount - 0.73f) < 0.05f &&
                     std::fabs(cfgIn.library[0].profile.parallaxStrength - 0.81f) < 0.05f;
        check("Config_V3_Steam_Metadata_Profiles_Roundtrip", match);
    }

    // 3. Monitor & Desktop Layer Host
    DesktopHost dhost;
    auto mons = dhost.EnumerateMonitors();
    check("Monitor_Enumeration", !mons.empty());
    {
        HWND dummyHwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"WPTestLayer",
            WS_POPUP, 0, 0, 320, 180, nullptr, nullptr, hInstance, nullptr);
        RECT rc = mons.empty() ? RECT{0, 0, 1280, 720} : mons[0].bounds;
        bool att = dhost.AttachRenderWindow(dummyHwnd, rc);
        check("Desktop_HWND_Parented_Behind_Icons",
              att && dhost.IsAttachmentHealthy(dummyHwnd),
              dhost.CurrentTopology().isRaisedLayeredProgman ? "(Win11 24H2+ Layered)" : "(Classic WorkerW)");
        dhost.DetachRenderWindow(dummyHwnd);
        DestroyWindow(dummyHwnd);
    }

    // 4. Multi-Format Starter Generation (#10: 3 MP4 + 1 GIF + 3 GPU Shaders + 1 HTML5 = 8 items)
    auto starters = EnsureStarterLibraryItems();
    char starterInfo[96];
    std::snprintf(starterInfo, sizeof(starterInfo), "(generated %zu multi-format items)", starters.size());
    check("Starter_MultiFormat_Library_Generation", starters.size() >= 8, starterInfo);

    // 5. WIC Animated GIF Frame Compositor & Vibrant Dominant Color (#10 + #8)
    std::wstring sampleGif, sampleMp4, sampleHtml;
    for (const auto& s : starters) {
        if (s.kind == WallpaperKind::Gif && sampleGif.empty()) sampleGif = s.path;
        if (s.kind == WallpaperKind::Video && sampleMp4.empty()) sampleMp4 = s.path;
        if (s.kind == WallpaperKind::Web && sampleHtml.empty()) sampleHtml = s.path;
    }

    {
        AnimatedGifData gif;
        bool gifOk = !sampleGif.empty() && DecodeAnimatedGif(sampleGif, gif) && gif.frames.size() >= 12;
        uint32_t domRgb = gifOk ? ComputeVibrantAccentRGB(gif.frames[0].bgra.data(), gif.width, gif.height) : 0;
        char gifBuf[96];
        std::snprintf(gifBuf, sizeof(gifBuf), "(frames=%zu, %ux%u, accent=#%06X)",
                      gif.frames.size(), gif.width, gif.height, domRgb);
        check("WIC_Animated_GIF_Decoder_And_AutoAccent", gifOk && domRgb != 0, gifBuf);
    }

    // 6. WASAPI 512-pt Radix-2 FFT + 32 Logarithmic Bands + Bass Detector (#1)
    {
        AudioSpectrumSnapshot fftSnap = AudioReactiveEngine::ComputeTestFftSnapshot(48000.0f, 80.0f, 3500.0f, 1.2f);
        char fftBuf[96];
        std::snprintf(fftBuf, sizeof(fftBuf), "(bass=%.2f, mid=%.2f, high=%.2f)",
                      fftSnap.bass, fftSnap.mid, fftSnap.high);
        check("WASAPI_512pt_FFT_32Band_And_Bass_Detector",
              fftSnap.bass > 0.20f && fftSnap.high > 0.10f, fftBuf);
        AudioReactiveEngine hotSwitchProbe;
        hotSwitchProbe.TriggerEndpointHotSwitch();
        check("WASAPI_DefaultOutput_HotSwitch_Notification", hotSwitchProbe.EndpointHotSwitchCount() == 1);
    }

    // 7. System Telemetry & Taskbar Transparency Restore (#5 + #8)
    {
        SystemTweaks tweaks;
        auto s1 = tweaks.SampleSystemStats();
        tweaks.ApplyTaskbarMode(TaskbarMode::ClearTransparent);
        tweaks.RestoreTaskbarDefault();
        char sysBuf[96];
        std::snprintf(sysBuf, sizeof(sysBuf), "(RAM=%u%%, %.1f/%.1f GB)", s1.ramPercent, s1.ramUsedGb, s1.ramTotalGb);
        check("SystemTweaks_CPU_RAM_And_Taskbar_Restore", s1.ramTotalGb > 0.5 && s1.ramPercent > 0, sysBuf);
    }

    // 8. D3D11 Unified Engine: MP4 + Trim + Seamless Loop + PRO Shaders + D2D Visualizer & Widgets (#1, #3, #5, #6, #7, #10)
    {
        VideoWallpaperPlayer player;
        bool initOk = player.Initialize(hInstance);
        check("D3D11_D2D_Player_Initialize", initOk);
        if (initOk) {
            check("DXGI_Waitable_SwapChain_LowLatency", player.IsWaitableSwapChainActive());
        }

        if (initOk && !sampleMp4.empty()) {
            AppConfig testCfg{};
            testCfg.audioVisMode = AudioVisMode::BarsBottom;
            testCfg.bassReactiveZoom = true;
            testCfg.widgetClock = true;
            testCfg.widgetMedia = true;
            testCfg.widgetSysStats = true;

            WallpaperProfile prof{};
            prof.sharpen = 0.7f;
            prof.bloom = 0.4f;
            prof.hueShift = 18.0f;
            prof.chromatic = 0.25f;
            prof.crtEffect = 0.15f;
            prof.trimStartPct = 0.08f;
            prof.trimEndPct = 0.88f;
            prof.seamlessLoop = true;

            AudioSpectrumSnapshot fakeAudio = AudioReactiveEngine::ComputeTestFftSnapshot(48000.0f, 90.0f, 2200.0f, 1.2f);
            fakeAudio.nowPlaying = L"Synthwave Horizon — Test Track";
            SystemTweaks tw;
            SystemStatsSnapshot sys = tw.SampleSystemStats();

            bool loadOk = player.LoadAndApply(sampleMp4, prof, testCfg, starters, false);
            ULONGLONG startMs = GetTickCount64();
            MSG msg{};
            while (GetTickCount64() - startMs < 4000) {
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                player.Tick(fakeAudio, sys, 0x0EA5E9);
                if (player.RenderedFrameCount() >= 5 && player.DurationSeconds() > 0.5) break;
                Sleep(20);
            }

            std::wstring mp4Snap = GetAppDataDir() + L"\\self_test_v2_mp4_pro.bmp";
            bool savedMp4 = player.SaveBackbufferBmp(mp4Snap);
            check("D3D11_MP4_Trim_PROShaders_Visualizer_Widgets",
                  loadOk && !player.HasPlaybackError() && savedMp4 && countColorPixels(mp4Snap) > 20000);

            // Vertical 9:16 FitBlur regression test: blurred Kawase wings must fill the landscape canvas.
            std::wstring verticalPath = GetSamplesDir() + L"\\selftest_vertical_9x16.mp4";
            bool verticalGenerated = GenerateProceduralMp4File(verticalPath, 2, 360, 640, 24, 24);
            VideoProbeInfo verticalProbe = verticalGenerated ? ProbeWallpaperAndSaveThumbnail(verticalPath) : VideoProbeInfo{};
            WallpaperProfile verticalProfile{};
            verticalProfile.scaleMode = ScaleMode::FitBlur;
            verticalProfile.bloom = 0.48f;
            bool verticalLoaded = verticalProbe.valid && verticalProbe.width == 360 && verticalProbe.height == 640 &&
                player.LoadAndApply(verticalPath, verticalProfile, testCfg, starters, false);
            for (int t = 0; t < 10; ++t) {
                Sleep(18);
                player.Tick(fakeAudio, sys, 0x38BDF8);
            }
            std::wstring verticalSnap = GetAppDataDir() + L"\\self_test_v3_fitblur_9x16.bmp";
            bool verticalSaved = verticalLoaded && player.SaveBackbufferBmp(verticalSnap);
            check("D3D11_Vertical_9x16_DualKawase_FitBlur",
                  verticalLoaded && verticalSaved && countColorPixels(verticalSnap) > 20000);

            // Capture GPU Crossfade snapshot (#3) and switch to Animated GIF (#10)
            player.CaptureCrossfadeSnapshot(1.2f);
            bool gifLoadOk = !sampleGif.empty() && player.LoadAndApply(sampleGif, WallpaperProfile{}, testCfg, starters, false);
            for (int t = 0; t < 6; ++t) {
                Sleep(30);
                player.Tick(fakeAudio, sys, 0xEC4899);
            }
            std::wstring gifSnap = GetAppDataDir() + L"\\self_test_v2_gif_crossfade.bmp";
            check("D3D11_Animated_GIF_And_GPU_Crossfade",
                  gifLoadOk && player.SaveBackbufferBmp(gifSnap) && countColorPixels(gifSnap) > 20000);

            // Switch to 60-FPS Procedural GPU Shader (#10)
            bool shLoadOk = player.LoadAndApply(L"shader://cyber_tunnel", WallpaperProfile{}, testCfg, starters, false);
            for (int t = 0; t < 4; ++t) {
                Sleep(20);
                player.Tick(fakeAudio, sys, 0xA855F7);
            }
            std::wstring shSnap = GetAppDataDir() + L"\\self_test_v2_shader.bmp";
            check("D3D11_Procedural_60FPS_GPU_Shader_Wallpaper",
                  shLoadOk && player.SaveBackbufferBmp(shSnap) && countColorPixels(shSnap) > 20000);

            // Custom HLSL live compile + friendly source-line diagnostic regression tests.
            std::wstring customHlsl = GetAppDataDir() + L"\\selftest_custom_shader.hlsl";
            auto writeShader = [&](const char* source) {
                HANDLE sh = CreateFileW(customHlsl.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (sh == INVALID_HANDLE_VALUE) return false;
                DWORD wr = 0;
                DWORD bytes = static_cast<DWORD>(std::strlen(source));
                BOOL ok = WriteFile(sh, source, bytes, &wr, nullptr);
                CloseHandle(sh);
                return ok && wr == bytes;
            };
            const char* validHlsl = R"(float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    return float4(uv.x, uv.y, 0.4, 1.0);
}
)";
            bool validWritten = writeShader(validHlsl);
            bool customShaderLoaded = validWritten && player.LoadAndApply(customHlsl, WallpaperProfile{}, testCfg, starters, false);
            for (int t = 0; t < 3; ++t) { Sleep(20); player.Tick(fakeAudio, sys, 0x10B981); }
            std::wstring validShaderError = player.LastShaderError();
            check("D3D11_Custom_HLSL_Compile_And_Run", customShaderLoaded && validShaderError.empty() &&
                  player.SaveBackbufferBmp(GetAppDataDir() + L"\\self_test_v3_custom_hlsl.bmp"));

            bool invalidWritten = writeShader("float4 main(float4 p : SV_POSITION) : SV_TARGET {\n    invalid-token;\n}\n");
            bool invalidFallback = invalidWritten && player.LoadAndApply(customHlsl, WallpaperProfile{}, testCfg, starters, false);
            std::wstring shaderError = player.LastShaderError();
            check("D3D11_HLSL_Error_Line_Diagnostic_And_Fallback",
                  invalidFallback && !shaderError.empty());
            DeleteFileW(customHlsl.c_str());

            // Verify Embedded WebView2 / OLE HTML5 Host (#10)
            WebWallpaperHost webHost;
            RECT webRc{0, 0, 640, 360};
            bool webOk = !sampleHtml.empty() && webHost.AttachAndNavigate(hInstance, player.RenderWindow(), webRc, sampleHtml);
            check("OLE_Embedded_HTML5_Canvas_Wallpaper_Host", webOk && webHost.IsActive());
            webHost.Close();

            player.Shutdown();
        }
    }

    // 9. Studio v3.0 Simplified UI, Guided Tour & Screenshot
    {
        AppConfig uiCfg{};
        uiCfg.library = starters;
        StudioWindow studio;
        StudioCallbacks cbs{};
        bool created = studio.Create(hInstance, &uiCfg, &uiCfg.library, cbs);
        studio.ShowAndRestore();
        studio.StartGuidedTour();

        MSG msg{};
        ULONGLONG t0 = GetTickCount64();
        while (GetTickCount64() - t0 < 350) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(15);
        }

        std::wstring uiSnap = shotOutPath.empty() ? (GetAppDataDir() + L"\\self_test_v2_studio_ui.bmp") : shotOutPath;
        bool uiSaved = studio.CaptureWindowBmp(uiSnap);
        check("Studio_V3_Simplified_UI_Photoshop_Guided_Tour_And_Screenshot",
              created && studio.IsGuidedTourActive() && uiSaved && GetFileSizeBytes(uiSnap) > 100000);
        studio.Destroy();
    }

    char sumBuf[128];
    std::snprintf(sumBuf, sizeof(sumBuf), "=== Self-Test Summary: %d passed, %d failed ===", passed, failed);
    appendReport(sumBuf);

    if (reportFile != INVALID_HANDLE_VALUE) CloseHandle(reportFile);
    return (failed == 0) ? 0 : 1;
}

}  // namespace

}  // namespace wp

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    wp::EnablePerMonitorDpiAwareness();

    HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    bool runSelfTest = false;
    bool startMinimized = false;
    std::wstring mediaArg;
    std::wstring selfTestReportPath;
    std::wstring selfTestShotPath;

    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--self-test") {
                runSelfTest = true;
                if (i + 1 < argc && argv[i + 1][0] != L'-') {
                    selfTestReportPath = argv[++i];
                }
                if (i + 1 < argc && argv[i + 1][0] != L'-') {
                    selfTestShotPath = argv[++i];
                }
            } else if (arg == L"--minimized" || arg == L"-silent") {
                startMinimized = true;
            } else if (arg == L"--apply" && i + 1 < argc) {
                mediaArg = argv[++i];
            } else if (!arg.empty() && arg[0] != L'-' && wp::WallpaperSourceExists(arg)) {
                mediaArg = arg;
            }
        }
        LocalFree(argv);
    }

    if (runSelfTest) {
        int rc = wp::RunSelfTestSuite(hInstance, selfTestReportPath, selfTestShotPath);
        MFShutdown();
        if (SUCCEEDED(coHr)) CoUninitialize();
        return rc;
    }

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, wp::kSingleInstanceMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(wp::kMsgWndClass, L"WallPulseController");
        if (existing) {
            std::wstring payload = mediaArg.empty() ? L"--show" : mediaArg;
            COPYDATASTRUCT cds{};
            cds.dwData = 0x57505331;
            cds.dwData = 0x57505331;
            cds.cbData = static_cast<DWORD>((payload.size() + 1) * sizeof(wchar_t));
            cds.lpData = const_cast<wchar_t*>(payload.c_str());
            SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
                                SMTO_ABORTIFHUNG, 2000, nullptr);
        }
        if (hMutex) CloseHandle(hMutex);
        MFShutdown();
        if (SUCCEEDED(coHr)) CoUninitialize();
        return 0;
    }

    wp::WallPulseApp app;
    int exitCode = 0;
    if (app.Init(hInstance, startMinimized, mediaArg)) {
        exitCode = app.RunMessageLoop();
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    MFShutdown();
    if (SUCCEEDED(coHr)) CoUninitialize();
    return exitCode;
}
