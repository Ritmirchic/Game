#pragma once

#include "common.h"
#include "config.h"

namespace wp {

struct MonitorDesc {
    HMONITOR handle = nullptr;
    RECT bounds{};       // Physical desktop pixel coordinates
    RECT workArea{};     // Excluding taskbar
    std::wstring deviceName;
    bool isPrimary = false;

    int Width() const { return bounds.right - bounds.left; }
    int Height() const { return bounds.bottom - bounds.top; }
};

struct DesktopTopology {
    HWND progman = nullptr;
    HWND shellDefView = nullptr;
    HWND defViewHost = nullptr;
    HWND workerW = nullptr;
    bool isRaisedLayeredProgman = false;  // Windows 11 24H2+ architecture
    bool valid = false;

    std::wstring Describe() const;
};

class DesktopHost {
public:
    DesktopHost() = default;
    ~DesktopHost() = default;

    // Enumerate all connected physical displays
    std::vector<MonitorDesc> EnumerateMonitors() const;
    RECT GetVirtualScreenRect() const;
    RECT ComputeTargetScreenRect(MonitorMode mode, int monitorIndex) const;

    // Discover Progman / SHELLDLL_DefView / WorkerW and spawn desktop wallpaper layer
    DesktopTopology DiscoverAndPrepare();
    const DesktopTopology& CurrentTopology() const { return topology_; }

    // Parent & position a render HWND behind desktop icons
    bool AttachRenderWindow(HWND renderHwnd, const RECT& screenRect);
    bool UpdateRenderWindowBounds(HWND renderHwnd, const RECT& screenRect);
    void DetachRenderWindow(HWND renderHwnd);

    // Check if Explorer restarted or WorkerW/Progman changed
    bool IsAttachmentHealthy(HWND renderHwnd) const;
    void RefreshStaticWallpaper() const;

    // Check whether user is currently looking at the desktop (icons focused)
    bool IsDesktopForeground() const;

private:
    DesktopTopology topology_{};
    HWND attachedParent_ = nullptr;
};

}  // namespace wp
