#include "desktop_host.h"

#include <cstdio>

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

namespace wp {

namespace {

struct EnumMonitorsCtx {
    std::vector<MonitorDesc>* list = nullptr;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC, LPRECT lprcMonitor, LPARAM dwData) {
    auto* ctx = reinterpret_cast<EnumMonitorsCtx*>(dwData);
    if (!ctx || !ctx->list) return FALSE;

    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    MonitorDesc desc;
    desc.handle = hMon;
    if (GetMonitorInfoW(hMon, &mi)) {
        desc.bounds = mi.rcMonitor;
        desc.workArea = mi.rcWork;
        desc.deviceName = mi.szDevice;
        desc.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    } else if (lprcMonitor) {
        desc.bounds = *lprcMonitor;
        desc.workArea = *lprcMonitor;
        desc.deviceName = L"DISPLAY";
        desc.isPrimary = ctx->list->empty();
    }
    ctx->list->push_back(std::move(desc));
    return TRUE;
}

struct FindDefViewCtx {
    HWND shellDefView = nullptr;
    HWND defViewHost = nullptr;
    HWND siblingWorkerW = nullptr;
};

BOOL CALLBACK EnumTopWindowsForDefView(HWND topHwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<FindDefViewCtx*>(lParam);
    HWND def = FindWindowExW(topHwnd, nullptr, L"SHELLDLL_DefView", nullptr);
    if (def != nullptr) {
        ctx->shellDefView = def;
        ctx->defViewHost = topHwnd;
        ctx->siblingWorkerW = FindWindowExW(nullptr, topHwnd, L"WorkerW", nullptr);
        return FALSE;  // Found
    }
    return TRUE;
}

HWND GetLastChildOf(HWND parent) {
    HWND child = GetWindow(parent, GW_CHILD);
    if (!child) return nullptr;
    return GetWindow(child, GW_HWNDLAST);
}

}  // namespace

std::wstring DesktopTopology::Describe() const {
    if (!valid) {
        return L"Рабочий стол Explorer не обнаружен (автономный режим)";
    }
    wchar_t buf[256];
    if (isRaisedLayeredProgman) {
        swprintf_s(buf, L"Windows 11 24H2+ Layered Progman (Progman=0x%p, DefView=0x%p, WorkerW=0x%p)",
                   progman, shellDefView, workerW);
    } else {
        swprintf_s(buf, L"Классический слой WorkerW (Progman=0x%p, DefView=0x%p, WorkerW=0x%p)",
                   progman, shellDefView, workerW);
    }
    return buf;
}

std::vector<MonitorDesc> DesktopHost::EnumerateMonitors() const {
    std::vector<MonitorDesc> list;
    EnumMonitorsCtx ctx{&list};
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&ctx));
    if (list.empty()) {
        MonitorDesc fallback;
        fallback.bounds = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        fallback.workArea = fallback.bounds;
        fallback.deviceName = L"DISPLAY1";
        fallback.isPrimary = true;
        list.push_back(fallback);
    }
    return list;
}

RECT DesktopHost::GetVirtualScreenRect() const {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw <= 0 || vh <= 0) {
        vw = GetSystemMetrics(SM_CXSCREEN);
        vh = GetSystemMetrics(SM_CYSCREEN);
        vx = 0;
        vy = 0;
    }
    return RECT{vx, vy, vx + vw, vy + vh};
}

RECT DesktopHost::ComputeTargetScreenRect(MonitorMode mode, int monitorIndex) const {
    auto monitors = EnumerateMonitors();
    if (mode == MonitorMode::SpanAcross || mode == MonitorMode::AllSame) {
        return GetVirtualScreenRect();
    }
    if (mode == MonitorMode::PrimaryOnly) {
        for (const auto& m : monitors) {
            if (m.isPrimary) return m.bounds;
        }
        return monitors.front().bounds;
    }
    if (monitorIndex >= 0 && static_cast<size_t>(monitorIndex) < monitors.size()) {
        return monitors[static_cast<size_t>(monitorIndex)].bounds;
    }
    return monitors.front().bounds;
}

DesktopTopology DesktopHost::DiscoverAndPrepare() {
    DesktopTopology topo{};
    topo.progman = FindWindowW(L"Progman", nullptr);
    if (!topo.progman) {
        LogWarn(L"Окно Progman не найдено (возможно, explorer.exe не запущен).");
        topology_ = topo;
        return topo;
    }

    // Request Progman to raise/split desktop icons from the static background wallpaper
    DWORD_PTR msgRes = 0;
    SendMessageTimeoutW(topo.progman, 0x052C, 0xD, 0x1, SMTO_NORMAL, 1000, &msgRes);

    LONG_PTR progEx = GetWindowLongPtrW(topo.progman, GWL_EXSTYLE);
    bool hasNoRedir = (progEx & WS_EX_NOREDIRECTIONBITMAP) != 0;

    FindDefViewCtx findCtx{};
    EnumWindows(EnumTopWindowsForDefView, reinterpret_cast<LPARAM>(&findCtx));

    // Fallback with traditional (0, 0) parameters if older build didn't spawn WorkerW yet
    if (!findCtx.siblingWorkerW && !hasNoRedir) {
        SendMessageTimeoutW(topo.progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &msgRes);
        progEx = GetWindowLongPtrW(topo.progman, GWL_EXSTYLE);
        hasNoRedir = (progEx & WS_EX_NOREDIRECTIONBITMAP) != 0;
        findCtx = {};
        EnumWindows(EnumTopWindowsForDefView, reinterpret_cast<LPARAM>(&findCtx));
    }

    topo.shellDefView = findCtx.shellDefView;
    topo.defViewHost = findCtx.defViewHost;
    topo.workerW = findCtx.siblingWorkerW;

    // Detect Windows 11 24H2+ Raised Layered Desktop where SHELLDLL_DefView & WorkerW
    // are both direct children of Progman instead of separate top-level HWNDs.
    HWND childWorkerUnderProgman = FindWindowExW(topo.progman, nullptr, L"WorkerW", nullptr);
    if (hasNoRedir || (!topo.workerW && childWorkerUnderProgman != nullptr && topo.defViewHost == topo.progman)) {
        topo.isRaisedLayeredProgman = true;
        topo.workerW = childWorkerUnderProgman;
        if (!topo.shellDefView) {
            topo.shellDefView = FindWindowExW(topo.progman, nullptr, L"SHELLDLL_DefView", nullptr);
            topo.defViewHost = topo.progman;
        }
        topo.valid = (topo.progman != nullptr && topo.shellDefView != nullptr);
    } else {
        topo.isRaisedLayeredProgman = false;
        topo.valid = (topo.workerW != nullptr || topo.progman != nullptr);
    }

    topology_ = topo;
    LogInfo(L"Топология рабочего стола: " + topo.Describe());
    return topo;
}

bool DesktopHost::AttachRenderWindow(HWND renderHwnd, const RECT& screenRect) {
    if (!renderHwnd || !IsWindow(renderHwnd)) return false;

    if (!topology_.valid || !IsWindow(topology_.progman)) {
        DiscoverAndPrepare();
    }

    const int w = screenRect.right - screenRect.left;
    const int h = screenRect.bottom - screenRect.top;

    if (!topology_.valid) {
        // Fallback if explorer.exe is absent (e.g. headless CI environment): place at bottom Z-order
        attachedParent_ = nullptr;
        SetWindowPos(renderHwnd, HWND_BOTTOM, screenRect.left, screenRect.top, w, h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        return false;
    }

    if (topology_.isRaisedLayeredProgman) {
        // Windows 11 24H2+ architecture:
        // Attach as WS_CHILD | WS_EX_LAYERED (alpha=255) directly inside Progman,
        // Z-ordered immediately below SHELLDLL_DefView and above child WorkerW.
        POINT pt{screenRect.left, screenRect.top};
        ScreenToClient(topology_.progman, &pt);

        LONG_PTR style = GetWindowLongPtrW(renderHwnd, GWL_STYLE);
        style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        style |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS);
        SetWindowLongPtrW(renderHwnd, GWL_STYLE, style);

        // Apply WS_EX_LAYERED + bAlpha=255 BEFORE SetParent (prevents child layered attachment quirks)
        LONG_PTR exStyle = GetWindowLongPtrW(renderHwnd, GWL_EXSTYLE);
        exStyle |= (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
        SetWindowLongPtrW(renderHwnd, GWL_EXSTYLE, exStyle);
        SetLayeredWindowAttributes(renderHwnd, 0, 255, LWA_ALPHA);

        SetParent(renderHwnd, topology_.progman);
        attachedParent_ = topology_.progman;

        HWND insertAfter = topology_.shellDefView ? topology_.shellDefView : HWND_TOP;
        SetWindowPos(renderHwnd, insertAfter, pt.x, pt.y, w, h,
                     SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        if (topology_.workerW && IsWindow(topology_.workerW)) {
            if (GetLastChildOf(topology_.progman) != topology_.workerW) {
                SetWindowPos(topology_.workerW, HWND_BOTTOM, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
        LogInfo(L"Окно видео успешно прикреплено в режиме Windows 11 24H2+ (Progman Layered Child).");
        return true;
    }

    // Classic Windows 10 & Windows 11 (21H2–23H2) WorkerW architecture
    HWND targetParent = topology_.workerW ? topology_.workerW : topology_.progman;
    ShowWindow(targetParent, SW_SHOWNA);

    POINT pt{screenRect.left, screenRect.top};
    ScreenToClient(targetParent, &pt);

    LONG_PTR style = GetWindowLongPtrW(renderHwnd, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS);
    SetWindowLongPtrW(renderHwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrW(renderHwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_LAYERED;
    exStyle |= (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    SetWindowLongPtrW(renderHwnd, GWL_EXSTYLE, exStyle);

    SetParent(renderHwnd, targetParent);
    attachedParent_ = targetParent;

    SetWindowPos(renderHwnd, HWND_TOP, pt.x, pt.y, w, h,
                 SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    LogInfo(L"Окно видео успешно прикреплено к слою WorkerW за иконками рабочего стола.");
    return true;
}

bool DesktopHost::UpdateRenderWindowBounds(HWND renderHwnd, const RECT& screenRect) {
    if (!renderHwnd || !IsWindow(renderHwnd)) return false;
    const int w = screenRect.right - screenRect.left;
    const int h = screenRect.bottom - screenRect.top;

    HWND parent = GetParent(renderHwnd);
    if (!parent || !topology_.valid) {
        return SetWindowPos(renderHwnd, HWND_BOTTOM, screenRect.left, screenRect.top, w, h,
                            SWP_NOACTIVATE | SWP_NOZORDER) != FALSE;
    }

    POINT pt{screenRect.left, screenRect.top};
    ScreenToClient(parent, &pt);

    if (topology_.isRaisedLayeredProgman && topology_.shellDefView && IsWindow(topology_.shellDefView)) {
        BOOL ok = SetWindowPos(renderHwnd, topology_.shellDefView, pt.x, pt.y, w, h,
                               SWP_NOACTIVATE | SWP_SHOWWINDOW);
        if (topology_.workerW && IsWindow(topology_.workerW) &&
            GetLastChildOf(topology_.progman) != topology_.workerW) {
            SetWindowPos(topology_.workerW, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return ok != FALSE;
    }

    return SetWindowPos(renderHwnd, HWND_TOP, pt.x, pt.y, w, h,
                        SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
}

void DesktopHost::DetachRenderWindow(HWND renderHwnd) {
    if (renderHwnd && IsWindow(renderHwnd)) {
        ShowWindow(renderHwnd, SW_HIDE);
        SetParent(renderHwnd, nullptr);

        LONG_PTR style = GetWindowLongPtrW(renderHwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_POPUP;
        SetWindowLongPtrW(renderHwnd, GWL_STYLE, style);
    }
    attachedParent_ = nullptr;
    RefreshStaticWallpaper();
}

bool DesktopHost::IsAttachmentHealthy(HWND renderHwnd) const {
    if (!renderHwnd || !IsWindow(renderHwnd)) return false;
    if (!topology_.valid) return false;
    if (!topology_.progman || !IsWindow(topology_.progman)) return false;
    if (attachedParent_ && !IsWindow(attachedParent_)) return false;
    if (GetParent(renderHwnd) != attachedParent_) return false;
    if (topology_.shellDefView && !IsWindow(topology_.shellDefView)) return false;
    return true;
}

void DesktopHost::RefreshStaticWallpaper() const {
    if (topology_.isRaisedLayeredProgman) {
        if (topology_.progman && IsWindow(topology_.progman)) {
            InvalidateRect(topology_.progman, nullptr, TRUE);
        }
        if (topology_.workerW && IsWindow(topology_.workerW)) {
            InvalidateRect(topology_.workerW, nullptr, TRUE);
        }
        return;
    }
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, nullptr, SPIF_UPDATEINIFILE);
}

bool DesktopHost::IsDesktopForeground() const {
    HWND fg = GetForegroundWindow();
    if (!fg) return true;
    if (fg == topology_.progman || fg == topology_.defViewHost || fg == topology_.workerW) {
        return true;
    }
    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, 64);
    return (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0);
}

}  // namespace wp
