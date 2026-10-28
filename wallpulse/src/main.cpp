#include "common.h"
#include "config.h"
#include "desktop_host.h"
#include "media_tools.h"
#include "smart_pause.h"
#include "studio_ui.h"
#include "video_wallpaper.h"

#include <mfapi.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdio>
#include <sstream>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")
#endif

namespace wp {

namespace {

int RunAutomatedSelfTest(HINSTANCE hInstance, const std::wstring& reportPath, const std::wstring& screenshotPath) {
    int passed = 0;
    int failed = 0;
    std::ostringstream report;
    report << "=== WallPulse Studio Windows Integration Self-Test ===\r\n";

    auto check = [&](const char* name, bool cond, const std::string& detail = {}) {
        if (cond) {
            ++passed;
            report << "[PASS] " << name;
        } else {
            ++failed;
            report << "[FAIL] " << name;
        }
        if (!detail.empty()) report << " -- " << detail;
        report << "\r\n";
    };

    // 1. Unicode / Cyrillic path round-trip
    std::wstring cyrSample = L"C:\\Users\\Пользователь\\Videos\\живые обои #1 (4K).mp4";
    check("UTF8_Cyrillic_RoundTrip", Utf8ToWide(WideToUtf8(cyrSample)) == cyrSample);
    check("Supported_Video_Ext_MP4", IsSupportedVideoExtension(cyrSample));
    check("Reject_NonVideo_Ext", !IsSupportedVideoExtension(L"C:\\test\\readme.txt"));

    // 2. Config round-trip with Cyrillic & locale-independent floats
    std::wstring tmpCfgPath = GetAppDataDir() + L"\\selftest_config.cfg";
    {
        AppConfig c1;
        c1.activeVideoPath = cyrSample;
        c1.muted = false;
        c1.volume = 0.73f;
        c1.playbackRate = 1.25f;
        c1.scaleMode = ScaleMode::FitBlur;
        c1.monitorMode = MonitorMode::SpanAcross;
        c1.brightness = -0.12f;
        c1.contrast = 1.18f;
        c1.saturation = 1.35f;
        c1.vignette = 0.22f;
        WallpaperItem wi;
        wi.path = cyrSample;
        wi.title = L"Неоновый Город | Тест = OK";
        wi.width = 1920;
        wi.height = 1080;
        wi.durationSec = 14.5;
        wi.fileSize = 1048576;
        c1.library.push_back(wi);

        bool saved = SaveAppConfig(c1, tmpCfgPath);
        AppConfig c2;
        bool loaded = LoadAppConfig(c2, tmpCfgPath);
        DeleteFileW(tmpCfgPath.c_str());

        check("Config_Save_Load", saved && loaded);
        check("Config_Path_Preserved", c2.activeVideoPath == cyrSample);
        check("Config_Floats_LocaleIndependent",
              std::abs(c2.volume - 0.73f) < 0.002f &&
              std::abs(c2.playbackRate - 1.25f) < 0.002f &&
              std::abs(c2.brightness - (-0.12f)) < 0.002f &&
              std::abs(c2.contrast - 1.18f) < 0.002f);
        check("Config_Library_Escaping",
              c2.library.size() == 1 && c2.library[0].title == L"Неоновый Город | Тест = OK");
    }

    // 3. Monitor & Desktop discovery
    DesktopHost desktop;
    auto mons = desktop.EnumerateMonitors();
    RECT virt = desktop.GetVirtualScreenRect();
    check("Monitors_Enumerated", !mons.empty() && mons[0].Width() > 0 && mons[0].Height() > 0,
          "count=" + std::to_string(mons.size()) + " primary=" +
          std::to_string(mons[0].Width()) + "x" + std::to_string(mons[0].Height()));
    check("Virtual_Screen_Valid", (virt.right - virt.left) > 0 && (virt.bottom - virt.top) > 0);

    DesktopTopology topo = desktop.DiscoverAndPrepare();
    report << "[INFO] DesktopTopology: " << WideToUtf8(topo.Describe()) << "\r\n";

    // 4. Starter H.264 MP4 generation & thumbnail extraction
    auto samples = EnsureStarterSampleMp4s();
    check("Starter_MP4_Generated", samples.size() == 3, "count=" + std::to_string(samples.size()));
    if (!samples.empty()) {
        check("Starter_MP4_NonEmptyFile", FileExists(samples[0].path) && GetFileSizeBytes(samples[0].path) > 4096,
              "bytes=" + std::to_string(GetFileSizeBytes(samples[0].path)));
        check("Starter_MP4_Dimensions", samples[0].width == 640 && samples[0].height == 360);

        uint32_t tw = 0, th = 0;
        std::vector<uint8_t> tbgra;
        bool thumbOk = LoadBmpBGRA(samples[0].thumbPath, tw, th, tbgra);
        uint64_t colorSum = 0;
        for (size_t i = 0; i + 3 < tbgra.size(); i += 4) {
            colorSum += tbgra[i] + tbgra[i + 1] + tbgra[i + 2];
        }
        check("Thumbnail_Extracted_320x180", thumbOk && tw == 320 && th == 180 && colorSum > 50000);
    }

    // 5. VideoWallpaperEngine D3D11 + Shader + IMFMediaEngine playback & frame decode
    VideoWallpaperEngine engine;
    bool engInit = engine.Initialize(hInstance, &desktop);
    check("Engine_D3D11_MediaEngine_Init", engInit, WideToUtf8(engine.GpuAdapterName()));

    AppConfig runCfg;
    runCfg.library = samples;
    runCfg.muted = true;
    runCfg.scaleMode = ScaleMode::Cover;

    if (engInit && !samples.empty()) {
        bool loaded = engine.LoadAndApply(samples[0].path, runCfg);
        check("Engine_LoadAndApply_MP4", loaded, WideToUtf8(engine.LastErrorText()));
        if (topo.valid) {
            HWND actualParent = GetParent(engine.RenderWindow());
            HWND expectedParent = topo.isRaisedLayeredProgman
                ? topo.progman
                : (topo.workerW ? topo.workerW : topo.progman);
            check("Desktop_HWND_Parented_Behind_Icons", actualParent == expectedParent);
        }

        // Pump messages & tick frames up to 3.5 seconds until decoded GPU video frames arrive
        bool gotFrame = false;
        uint64_t seq = 0;
        uint32_t pw = 0, ph = 0;
        std::vector<uint8_t> prevBgra;

        ULONGLONG startMs = GetTickCount64();
        while (GetTickCount64() - startMs < 3500) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (engine.TickFrame(true)) {
                gotFrame = true;
            }
            if (engine.CopyLatestPreviewFrame(pw, ph, prevBgra, seq) && pw == 320 && ph == 180) {
                uint64_t sum = 0;
                for (size_t i = 0; i + 3 < prevBgra.size(); i += 4) {
                    sum += prevBgra[i] + prevBgra[i + 1] + prevBgra[i + 2];
                }
                if (sum > 25000) {
                    gotFrame = true;
                    break;
                }
            }
            Sleep(16);
        }

        check("Engine_Decoded_And_ShaderRendered_MP4_Frame", gotFrame && pw == 320 && ph == 180,
              "video=" + std::to_string(engine.VideoWidth()) + "x" + std::to_string(engine.VideoHeight()));
    }

    // 6. StudioWindow Direct2D UI creation, render & screenshot capture
    StudioWindow studio;
    bool uiOk = studio.Create(hInstance, &runCfg, &desktop, &engine, false);
    check("StudioUI_Direct2D_Window_Created", uiOk);
    if (uiOk) {
        if (!samples.empty()) {
            runCfg.activeVideoPath = samples[0].path;
        }
        for (int i = 0; i < 6; ++i) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            engine.TickFrame(true);
            studio.OnUiTick(L"");
            Sleep(30);
        }

        if (!screenshotPath.empty()) {
            bool shotOk = studio.CaptureWindowClientToBmp(screenshotPath);
            check("StudioUI_Screenshot_Captured", shotOk && FileExists(screenshotPath));
        }
        studio.Destroy();
    }

    engine.Shutdown();

    report << "=== SUMMARY: " << passed << " passed, " << failed << " failed ===\r\n";
    std::string finalOut = report.str();

    std::wstring outFile = reportPath.empty() ? (GetAppDataDir() + L"\\selftest_report.txt") : reportPath;
    HANDLE hFile = CreateFileW(outFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(hFile, finalOut.data(), static_cast<DWORD>(finalOut.size()), &wr, nullptr);
        CloseHandle(hFile);
    }

    return (failed == 0) ? 0 : 1;
}

}  // namespace

}  // namespace wp

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    wp::EnablePerMonitorDpiAwareness();

    HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    HRESULT mfHr = MFStartup(MF_VERSION);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    bool startMinimized = false;
    bool selfTestMode = false;
    std::wstring selfTestReportPath;
    std::wstring selfTestShotPath;
    std::wstring cliVideoPath;

    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--minimized" || arg == L"-minimized" || arg == L"/minimized") {
                startMinimized = true;
            } else if (arg == L"--self-test") {
                selfTestMode = true;
                if (i + 1 < argc && argv[i + 1][0] != L'-') {
                    selfTestReportPath = argv[++i];
                }
                if (i + 1 < argc && argv[i + 1][0] != L'-') {
                    selfTestShotPath = argv[++i];
                }
            } else if (!arg.empty() && arg[0] != L'-') {
                cliVideoPath = arg;
            }
        }
        LocalFree(argv);
    }

    if (selfTestMode) {
        int rc = wp::RunAutomatedSelfTest(hInstance, selfTestReportPath, selfTestShotPath);
        if (SUCCEEDED(mfHr)) MFShutdown();
        if (SUCCEEDED(coHr)) CoUninitialize();
        return rc;
    }

    // Single-instance check: forward CLI video or bring existing Studio window to front
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Local\\WallPulse_SingleInstance_Mutex_v1");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"WallPulseStudioClass", nullptr);
        if (existing) {
            if (!cliVideoPath.empty()) {
                COPYDATASTRUCT cds{};
                cds.dwData = 0x57504D50;
                cds.cbData = static_cast<DWORD>((cliVideoPath.size() + 1) * sizeof(wchar_t));
                cds.lpData = const_cast<wchar_t*>(cliVideoPath.c_str());
                SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
                                    SMTO_ABORTIFHUNG, 3000, nullptr);
            } else if (!startMinimized) {
                PostMessageW(existing, WM_APP + 11, 0, 0);
                SetForegroundWindow(existing);
            }
        }
        CloseHandle(hMutex);
        if (SUCCEEDED(mfHr)) MFShutdown();
        if (SUCCEEDED(coHr)) CoUninitialize();
        return 0;
    }

    wp::LogInfo(L"=== Запуск WallPulse Studio v1.0.0 ===");

    wp::AppConfig cfg;
    bool hadSavedConfig = wp::LoadAppConfig(cfg);

    // Ensure built-in sample MP4 wallpapers exist and are present in library
    auto starterSamples = wp::EnsureStarterSampleMp4s();
    for (const auto& sample : starterSamples) {
        bool exists = false;
        for (const auto& item : cfg.library) {
            if (_wcsicmp(item.path.c_str(), sample.path.c_str()) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            cfg.library.push_back(sample);
        }
    }

    // Prune deleted built-in paths while keeping user library clean
    if (!hadSavedConfig && cfg.activeVideoPath.empty() && !starterSamples.empty()) {
        cfg.activeVideoPath = starterSamples[0].path;
    }

    wp::DesktopHost desktopHost;
    desktopHost.DiscoverAndPrepare();

    wp::VideoWallpaperEngine engine;
    if (!engine.Initialize(hInstance, &desktopHost)) {
        MessageBoxW(
            nullptr,
            (L"Не удалось инициализировать видеодвижок WallPulse:\n" + engine.LastErrorText()).c_str(),
            L"WallPulse — Ошибка инициализации",
            MB_ICONERROR | MB_OK);
    }

    wp::StudioWindow studio;
    if (!studio.Create(hInstance, &cfg, &desktopHost, &engine, startMinimized)) {
        engine.Shutdown();
        if (hMutex) CloseHandle(hMutex);
        if (SUCCEEDED(mfHr)) MFShutdown();
        if (SUCCEEDED(coHr)) CoUninitialize();
        return 1;
    }

    if (!cliVideoPath.empty() && wp::FileExists(cliVideoPath)) {
        studio.AddVideoFile(cliVideoPath, true);
    } else if (!cfg.activeVideoPath.empty() && wp::FileExists(cfg.activeVideoPath)) {
        engine.LoadAndApply(cfg.activeVideoPath, cfg);
    }

    wp::SaveAppConfig(cfg);

    wp::SmartPauseDetector smartPause;
    constexpr UINT_PTR kTimerFrameTick = 1;
    constexpr UINT_PTR kTimerSmartCheck = 2;
    SetTimer(studio.Handle(), kTimerFrameTick, 16, nullptr);    // ~60 FPS video/shader tick
    SetTimer(studio.Handle(), kTimerSmartCheck, 600, nullptr);  // Smart fullscreen/Explorer watchdog

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER && msg.hwnd == studio.Handle()) {
            if (msg.wParam == kTimerFrameTick) {
                bool uiVisible = studio.IsVisibleAndNotMinimized();
                engine.TickFrame(uiVisible);
                if (uiVisible) {
                    studio.OnUiTick(L"");
                }
                continue;
            } else if (msg.wParam == kTimerSmartCheck) {
                auto sp = smartPause.Evaluate(cfg, desktopHost, studio.Handle(), engine.RenderWindow());
                engine.SetSmartPause(sp.shouldPauseVideo, sp.shouldMuteAudio);
                engine.CheckDesktopAttachment(cfg);
                studio.OnUiTick(sp.reason);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(studio.Handle(), kTimerFrameTick);
    KillTimer(studio.Handle(), kTimerSmartCheck);

    wp::SaveAppConfig(cfg);
    studio.Destroy();
    engine.Shutdown();

    if (hMutex) CloseHandle(hMutex);
    if (SUCCEEDED(mfHr)) MFShutdown();
    if (SUCCEEDED(coHr)) CoUninitialize();
    return 0;
}
