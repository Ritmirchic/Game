#include "system_tweaks.h"

#include <algorithm>

#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED 0x031A
#endif

namespace wp {

namespace {

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    uint32_t GradientColor;  // AABBGGRR
    int AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

SetWindowCompositionAttributeFn GetSwcaProc() {
    static SetWindowCompositionAttributeFn fn = []() -> SetWindowCompositionAttributeFn {
        HMODULE u32 = GetModuleHandleW(L"user32.dll");
        if (!u32) return nullptr;
        return reinterpret_cast<SetWindowCompositionAttributeFn>(
            GetProcAddress(u32, "SetWindowCompositionAttribute"));
    }();
    return fn;
}

void ApplyAccentToTrayWindow(HWND trayHwnd, TaskbarMode mode) {
    if (!trayHwnd || !IsWindow(trayHwnd)) return;
    auto swca = GetSwcaProc();
    if (!swca) return;

    ACCENT_POLICY policy{};
    if (mode == TaskbarMode::ClearTransparent) {
        policy.AccentState = 2;  // ACCENT_ENABLE_TRANSPARENTGRADIENT
        policy.AccentFlags = 2;
        policy.GradientColor = 0x01000000U;
    } else if (mode == TaskbarMode::AcrylicBlur) {
        policy.AccentState = 4;  // ACCENT_ENABLE_ACRYLICBLURBEHIND
        policy.AccentFlags = 2;
        policy.GradientColor = 0x661C1410U;  // Subtle dark smoked glass (AABBGGRR)
    } else {
        policy.AccentState = 0;  // ACCENT_DISABLED
    }

    WINDOWCOMPOSITIONATTRIBDATA data{};
    data.Attrib = 19;  // WCA_ACCENT_POLICY
    data.pvData = &policy;
    data.cbData = sizeof(policy);
    swca(trayHwnd, &data);

    if (mode == TaskbarMode::Default) {
        SendNotifyMessageW(trayHwnd, WM_THEMECHANGED, 0, 0);
    }
}

uint64_t FileTimeToU64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

}  // namespace

SystemTweaks::~SystemTweaks() {
    RestoreTaskbarDefault();
}

void SystemTweaks::ApplyTaskbarMode(TaskbarMode mode) {
    if (mode == TaskbarMode::Default) {
        if (wasModified_) {
            RestoreTaskbarDefault();
        }
        return;
    }

    HWND primaryTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (primaryTray) {
        ApplyAccentToTrayWindow(primaryTray, mode);
    }

    HWND secTray = nullptr;
    while ((secTray = FindWindowExW(nullptr, secTray, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        ApplyAccentToTrayWindow(secTray, mode);
    }

    wasModified_ = true;
    lastMode_ = mode;
}

void SystemTweaks::RestoreTaskbarDefault() {
    if (!wasModified_) return;
    wasModified_ = false;
    lastMode_ = TaskbarMode::Default;

    HWND primaryTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (primaryTray) {
        ApplyAccentToTrayWindow(primaryTray, TaskbarMode::Default);
    }

    HWND secTray = nullptr;
    while ((secTray = FindWindowExW(nullptr, secTray, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        ApplyAccentToTrayWindow(secTray, TaskbarMode::Default);
    }
}

SystemStatsSnapshot SystemTweaks::SampleSystemStats() {
    ULONGLONG now = GetTickCount64();
    if (now - lastSampleMs_ >= 900 || prevKernel_ == 0) {
        lastSampleMs_ = now;
        FILETIME ftIdle{}, ftKernel{}, ftUser{};
        if (GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
            uint64_t idle = FileTimeToU64(ftIdle);
            uint64_t kernel = FileTimeToU64(ftKernel);
            uint64_t user = FileTimeToU64(ftUser);

            uint64_t dIdle = idle - prevIdle_;
            uint64_t dTotal = (kernel - prevKernel_) + (user - prevUser_);
            if (prevKernel_ != 0 && dTotal > 0 && dTotal >= dIdle) {
                float pct = static_cast<float>(dTotal - dIdle) * 100.0f / static_cast<float>(dTotal);
                lastCpuPct_ = std::clamp(pct, 0.0f, 100.0f);
            }
            prevIdle_ = idle;
            prevKernel_ = kernel;
            prevUser_ = user;
        }
    }

    SystemStatsSnapshot out{};
    out.cpuPercent = lastCpuPct_;

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        out.ramPercent = ms.dwMemoryLoad;
        out.ramTotalGb = static_cast<double>(ms.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        out.ramUsedGb = static_cast<double>(ms.ullTotalPhys - ms.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
    }
    return out;
}

}  // namespace wp
