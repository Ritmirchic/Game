#pragma once

#include "common.h"

namespace wp {

enum class ScaleMode : int {
    Cover = 0,     // Заполнение без полей (обрезает края по пропорциям)
    FitBlur = 1,   // Вписать + размытый атмосферный фон
    FitBars = 2,   // Вписать (чёрные поля)
    Stretch = 3,   // Растянуть во весь экран
    Center = 4     // По центру 1:1
};

enum class MonitorMode : int {
    AllSame = 0,          // Одинаковые обои на всех мониторах
    SpanAcross = 1,       // Единая панорама через все мониторы
    PrimaryOnly = 2,      // Только основной монитор
    SpecificMonitor = 3   // Конкретный выбранный монитор
};

struct WallpaperItem {
    std::wstring path;
    std::wstring title;
    std::wstring thumbPath;
    uint32_t width = 0;
    uint32_t height = 0;
    double durationSec = 0.0;
    uint64_t fileSize = 0;
    bool isBuiltIn = false;
};

struct AppConfig {
    std::wstring activeVideoPath;
    bool userPaused = false;

    bool muted = true;
    float volume = 0.50f;           // 0.0 .. 1.0 (активируется при снятии mute или движении ползунка)
    bool muteWhenUnfocused = true;  // Выключать звук, когда открыты другие окна

    float playbackRate = 1.0f;      // 0.25 .. 2.0
    ScaleMode scaleMode = ScaleMode::Cover;
    MonitorMode monitorMode = MonitorMode::AllSame;
    int targetMonitorIndex = 0;

    float brightness = 0.0f;        // -0.50 .. +0.50
    float contrast = 1.0f;          //  0.50 ..  1.50
    float saturation = 1.0f;        //  0.00 ..  2.00
    float vignette = 0.15f;         //  0.00 ..  0.80
    bool mouseParallax = true;

    bool pauseOnFullscreen = true;  // 0% CPU/GPU в полноэкранных играх и развёрнутых окнах
    bool pauseOnBattery = false;
    bool closeToTray = true;
    bool startWithWindows = false;
    bool shownTrayHint = false;

    std::vector<WallpaperItem> library;

    void ResetEffects() {
        playbackRate = 1.0f;
        brightness = 0.0f;
        contrast = 1.0f;
        saturation = 1.0f;
        vignette = 0.15f;
        mouseParallax = true;
    }
};

bool LoadAppConfig(AppConfig& cfg, const std::wstring& path = GetConfigPath());
bool SaveAppConfig(const AppConfig& cfg, const std::wstring& path = GetConfigPath());

bool IsAutoStartEnabledInRegistry();
bool SetAutoStartInRegistry(bool enable);

bool IsExplorerContextMenuEnabled();
bool SetExplorerContextMenu(bool enable);

}  // namespace wp
