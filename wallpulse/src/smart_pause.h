#pragma once

#include "common.h"
#include "config.h"
#include "desktop_host.h"

namespace wp {

struct SmartPauseState {
    bool shouldPauseVideo = false;
    bool shouldMuteAudio = false;
    std::wstring reason;
};

class SmartPauseDetector {
public:
    SmartPauseState Evaluate(
        const AppConfig& cfg,
        const DesktopHost& desktopHost,
        HWND studioHwnd,
        HWND renderHwnd) const;
};

}  // namespace wp
