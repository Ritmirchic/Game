#include "smart_pause.h"

#include <dwmapi.h>
#include <algorithm>

#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

namespace wp {

namespace {

bool IsIgnoredShellWindow(HWND hwnd, HWND studioHwnd, HWND renderHwnd) {
    if (!hwnd || hwnd == studioHwnd || hwnd == renderHwnd) return true;
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return true;

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0) {
        return true;
    }

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) {
        return true;
    }

    wchar_t cls[96] = {};
    GetClassNameW(hwnd, cls, 96);
    if (wcscmp(cls, L"Progman") == 0 ||
        wcscmp(cls, L"WorkerW") == 0 ||
        wcscmp(cls, L"Shell_TrayWnd") == 0 ||
        wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 ||
        wcscmp(cls, L"NotifyIconOverflowWindow") == 0 ||
        wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0 ||
        wcscmp(cls, L"XamlExplorerHostIslandWindow") == 0) {
        return true;
    }
    return false;
}

bool DoesWindowCoverMonitor(HWND hwnd, const MonitorDesc& mon) {
    RECT winRect{};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &winRect, sizeof(winRect)))) {
        if (!GetWindowRect(hwnd, &winRect)) return false;
    }

    // Check against workArea (covers maximized Chrome/VSCode/Explorer) or full monitor bounds (games)
    RECT inter{};
    if (!IntersectRect(&inter, &winRect, &mon.workArea)) return false;

    const long long interW = std::max<long>(0, inter.right - inter.left);
    const long long interH = std::max<long>(0, inter.bottom - inter.top);
    const long long workW = std::max<long>(1, mon.workArea.right - mon.workArea.left);
    const long long workH = std::max<long>(1, mon.workArea.bottom - mon.workArea.top);

    const long long interArea = interW * interH;
    const long long workArea = workW * workH;
    return (interArea * 100LL) >= (workArea * 95LL);
}

struct CoverScanCtx {
    HWND studioHwnd = nullptr;
    HWND renderHwnd = nullptr;
    std::vector<MonitorDesc> activeMonitors;
    std::vector<bool> monitorCovered;
};

BOOL CALLBACK EnumCoveringWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<CoverScanCtx*>(lParam);
    if (IsIgnoredShellWindow(hwnd, ctx->studioHwnd, ctx->renderHwnd)) {
        return TRUE;
    }

    bool allCovered = true;
    for (size_t i = 0; i < ctx->activeMonitors.size(); ++i) {
        if (!ctx->monitorCovered[i] && DoesWindowCoverMonitor(hwnd, ctx->activeMonitors[i])) {
            ctx->monitorCovered[i] = true;
        }
        if (!ctx->monitorCovered[i]) {
            allCovered = false;
        }
    }
    return allCovered ? FALSE : TRUE;
}

}  // namespace

SmartPauseState SmartPauseDetector::Evaluate(
    const AppConfig& cfg,
    const DesktopHost& desktopHost,
    HWND studioHwnd,
    HWND renderHwnd) const {
    SmartPauseState out{};

    // If WallPulse Studio itself is the active foreground window, keep live preview + desktop running
    HWND fg = GetForegroundWindow();
    bool studioIsActive = (studioHwnd != nullptr && fg == studioHwnd && !IsIconic(studioHwnd));

    if (cfg.pauseOnBattery) {
        SYSTEM_POWER_STATUS sps{};
        if (GetSystemPowerStatus(&sps) && sps.ACLineStatus == 0) {
            out.shouldPauseVideo = true;
            out.shouldMuteAudio = true;
            out.reason = L"Умная пауза: питание от батареи";
            return out;
        }
    }

    if (cfg.pauseOnFullscreen && !studioIsActive) {
        auto allMons = desktopHost.EnumerateMonitors();
        CoverScanCtx scan{};
        scan.studioHwnd = studioHwnd;
        scan.renderHwnd = renderHwnd;

        if (cfg.monitorMode == MonitorMode::PrimaryOnly) {
            for (const auto& m : allMons) {
                if (m.isPrimary) scan.activeMonitors.push_back(m);
            }
        } else if (cfg.monitorMode == MonitorMode::SpecificMonitor &&
                   cfg.targetMonitorIndex >= 0 &&
                   static_cast<size_t>(cfg.targetMonitorIndex) < allMons.size()) {
            scan.activeMonitors.push_back(allMons[static_cast<size_t>(cfg.targetMonitorIndex)]);
        } else {
            scan.activeMonitors = allMons;
        }
        if (scan.activeMonitors.empty() && !allMons.empty()) {
            scan.activeMonitors.push_back(allMons.front());
        }
        scan.monitorCovered.assign(scan.activeMonitors.size(), false);

        if (!scan.activeMonitors.empty()) {
            EnumWindows(EnumCoveringWindowsProc, reinterpret_cast<LPARAM>(&scan));
            bool everyMonitorCovered = true;
            for (bool c : scan.monitorCovered) {
                if (!c) {
                    everyMonitorCovered = false;
                    break;
                }
            }
            if (everyMonitorCovered) {
                out.shouldPauseVideo = true;
                out.shouldMuteAudio = true;
                out.reason = L"Умная пауза: экран закрыт приложением / игрой";
                return out;
            }
        }
    }

    if (cfg.muteWhenUnfocused && !studioIsActive && !desktopHost.IsDesktopForeground()) {
        out.shouldMuteAudio = true;
    }

    return out;
}

}  // namespace wp
