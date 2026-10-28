#pragma once

#include "common.h"

namespace wp {

enum class ScaleMode : int {
    Cover = 0,     // Заполнить экран без полей
    FitBlur = 1,   // Вписать + атмосферный размытый фон (идеально для 9:16!)
    FitBars = 2,   // Вписать (чёрные поля)
    Stretch = 3,   // Растянуть во весь экран
    Center = 4     // По центру 1:1
};

enum class MonitorMode : int {
    AllSame = 0,                 // Одинаковые обои на всех мониторах
    SpanAcross = 1,              // Единая панорама через все мониторы
    PrimaryOnly = 2,             // Только основной монитор
    SpecificMonitor = 3,         // Конкретный выбранный монитор
    PerMonitorIndependent = 4    // Разные независимые обои на каждый монитор
};

enum class AudioVisMode : int {
    Off = 0,
    BarsBottom = 1,     // Неоновый эквалайзер внизу экрана
    SymmetricWave = 2,  // Гладкая кривая Безье (Неон-Волна)
    NeonRing = 3        // Круговой спектр в центре
};

enum class WidgetAnchor : int {
    TopRight = 0,       // Правый верх
    TopLeft = 1,        // Левый верх
    Center = 2,         // По центру экрана
    BottomLeft = 3,     // Левый низ
    BottomRight = 4,    // Правый низ
    Free = 5            // Произвольное положение по нормализованным X/Y
};

enum class TaskbarMode : int {
    Default = 0,           // Стандартная панель задач Windows
    ClearTransparent = 1,  // Полностью прозрачная панель задач
    AcrylicBlur = 2        // Матовое размытое стекло (Blur)
};

enum class AccentTheme : int {
    AutoFromWallpaper = 0, // Автоподбор под цвет активных обоев
    NeonCyan = 1,          // Неоновый циан (#0EA5E9)
    CyberViolet = 2,       // Кибер-фиолетовый (#A855F7)
    MatrixEmerald = 3,     // Изумрудный (#10B981)
    SunsetCrimson = 4      // Алый закат (#F43F5E)
};

enum class ParticleOverlay : int {
    Off = 0,          // Без частиц
    GoldenBokeh = 1,  // ✨ Тёплое боке и пыль
    NeonRain = 2,     // 🌧 Неоновый дождь
    SoftSnow = 3,     // ❄ Мягкий снег
    CyberEmbers = 4   // 🔥 Огненные искры
};

enum class QuickVisualStyle : int {
    Original = 0,     // Оригинал
    Cinema = 1,       // Сочное кино
    Neon = 2,         // Кибер-неон
    Warm = 3,         // Тёплый уют
    RetroVhs = 4,     // Ретро VHS
    Custom = 5        // Своя настройка
};

// Индивидуальный профиль настроек для каждого элемента библиотеки
struct WallpaperProfile {
    ScaleMode scaleMode = ScaleMode::Cover;
    bool muted = true;
    float volume = 0.50f;
    float playbackRate = 1.0f;

    // 2.5D Параллакс от мыши и Живые GPU-частицы / Погода
    float parallaxStrength = 0.35f;                 // 0.00 .. 1.00
    ParticleOverlay particles = ParticleOverlay::Off;
    float particleAmount = 0.58f;                   // 0.10 .. 1.00

    // Базовая цветокоррекция
    float brightness = 0.0f;   // -0.50 .. +0.50
    float contrast = 1.0f;     //  0.50 ..  1.50
    float saturation = 1.0f;   //  0.00 ..  2.00
    float vignette = 0.15f;    //  0.00 ..  0.80

    // PRO-шейдеры (AMD CAS + Dual-Kawase Bloom + Hue + CA + VHS)
    float hueShift = 0.0f;     // -180.0 .. +180.0 град.
    float sharpen = 0.15f;     //  0.00 ..  1.00
    float bloom = 0.12f;       //  0.00 ..  1.00 (Dual-Kawase Multi-Pass Bloom)
    float chromatic = 0.0f;    //  0.00 ..  1.00
    float crtEffect = 0.0f;    //  0.00 ..  1.00

    // Обрезка фрагмента и бесшовный цикл [От — До]
    float trimStartPct = 0.0f; //  0.00 ..  0.90
    float trimEndPct = 1.0f;   //  0.10 ..  1.00
    bool seamlessLoop = true;

    void ApplyQuickStyle(QuickVisualStyle style);
    QuickVisualStyle DetectQuickStyle() const;

    void ResetVisuals() {
        playbackRate = 1.0f;
        parallaxStrength = 0.35f;
        particles = ParticleOverlay::Off;
        particleAmount = 0.58f;
        brightness = 0.0f;
        contrast = 1.0f;
        saturation = 1.0f;
        vignette = 0.15f;
        hueShift = 0.0f;
        sharpen = 0.15f;
        bloom = 0.12f;
        chromatic = 0.0f;
        crtEffect = 0.0f;
        trimStartPct = 0.0f;
        trimEndPct = 1.0f;
        seamlessLoop = true;
    }
};

struct WallpaperItem {
    std::wstring path;
    std::wstring title;
    std::wstring thumbPath;
    WallpaperKind kind = WallpaperKind::Video;
    uint32_t width = 0;
    uint32_t height = 0;
    double durationSec = 0.0;
    uint64_t fileSize = 0;
    bool isBuiltIn = false;
    bool favorite = false;
    bool inPlaylist = true;
    bool thumbPending = false;  // Async background thumbnail generation flag
    uint32_t dominantColorRGB = 0x0EA5E9;
    WallpaperProfile profile{};
};

struct AppConfig {
    std::wstring activeVideoPath;
    bool userPaused = false;
    bool muteWhenUnfocused = true;
    bool mouseParallax = true;

    MonitorMode monitorMode = MonitorMode::AllSame;
    int targetMonitorIndex = 0;
    std::vector<std::wstring> monitorWallpapers = {L"", L"", L"", L""};

    // Аудиовизуализатор WASAPI
    AudioVisMode audioVisMode = AudioVisMode::Off;
    float audioSensitivity = 1.15f;    // 0.25 .. 3.00
    bool bassReactiveZoom = false;     // Пульсация обоев под бас музыки
    float bassPulseAmount = 0.40f;     // 0.00 .. 1.00

    // Плейлист, Crossfade и Расписание «День / Ночь»
    int playlistIntervalMin = 0;       // 0 = Выкл, 1, 5, 15, 30, 60
    bool playlistShuffle = false;
    bool playlistFavoritesOnly = false;
    float crossfadeSeconds = 1.10f;    // 0.00 .. 2.50 сек

    bool dayNightEnabled = false;
    int dayStartHour = 7;              // 07:00
    int nightStartHour = 20;           // 20:00
    std::wstring dayWallpaperPath;
    std::wstring nightWallpaperPath;

    // Виджеты рабочего стола
    bool widgetClock = false;
    bool clock24h = true;
    bool clockShowSeconds = false;
    bool widgetMedia = false;
    bool widgetSysStats = false;
    WidgetAnchor widgetAnchor = WidgetAnchor::TopRight;
    float widgetOffsetX = 0.82f;        // 0.00 .. 1.00 within the usable monitor area (Free anchor)
    float widgetOffsetY = 0.08f;        // 0.00 .. 1.00 within the usable monitor area (Free anchor)
    float widgetScale = 1.0f;          // 0.75 .. 1.50
    float widgetOpacity = 0.68f;       // 0.20 .. 0.95

    // Панель задач Windows и Акцент темы
    TaskbarMode taskbarMode = TaskbarMode::Default;
    AccentTheme accentTheme = AccentTheme::AutoFromWallpaper;

    // Умная пауза, Высокая герцовка, Хоткеи и Интерактивный гид (как в Photoshop)
    uint32_t targetFps = 60;           // 30, 60, 120, 144, 165
    bool pauseOnFullscreen = true;
    bool pauseOnMaximized = true;
    bool pauseOnBattery = false;
    bool closeToTray = true;
    bool startWithWindows = false;
    bool startMinimized = false;
    bool explorerContextMenu = false;
    bool globalHotkeys = true;
    bool shownTrayHint = false;
    bool completedOnboardingTour = false;

    std::vector<WallpaperItem> library;

    // Helpers to resolve item & profile
    WallpaperItem* FindItem(const std::wstring& path);
    const WallpaperItem* FindItem(const std::wstring& path) const;
    WallpaperProfile GetEffectiveProfile(const std::wstring& path) const;
    uint32_t ResolveAccentColorRGB() const;
};

bool LoadAppConfig(AppConfig& cfg, const std::wstring& path = GetConfigPath());
bool SaveAppConfig(const AppConfig& cfg, const std::wstring& path = GetConfigPath());

bool IsAutoStartEnabledInRegistry();
bool SetAutoStartInRegistry(bool enable);

bool IsExplorerContextMenuEnabled();
bool SetExplorerContextMenu(bool enable);

}  // namespace wp
