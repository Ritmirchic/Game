#include "config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace wp {

namespace {

constexpr const wchar_t* kRunRegKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kRunValueName = L"WallPulseStudio";

std::string EscapeField(const std::wstring& ws) {
    std::string s = WideToUtf8(ws);
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '|':  out += "\\p"; break;
            case '=':  out += "\\e"; break;
            default:   out.push_back(static_cast<char>(c)); break;
        }
    }
    return out;
}

std::wstring UnescapeField(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[++i];
            switch (next) {
                case '\\': out.push_back('\\'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'p':  out.push_back('|'); break;
                case 'e':  out.push_back('='); break;
                default:   out.push_back(next); break;
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return Utf8ToWide(out);
}

std::string FormatFloatFixed(double v) {
    if (!std::isfinite(v)) return "0.0000";
    bool neg = v < 0.0;
    double av = std::fabs(v);
    unsigned long long whole = static_cast<unsigned long long>(av);
    unsigned long long frac = static_cast<unsigned long long>(
        std::llround((av - static_cast<double>(whole)) * 10000.0));
    if (frac >= 10000ULL) {
        whole += 1;
        frac -= 10000ULL;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s%llu.%04llu", neg ? "-" : "", whole, frac);
    return buf;
}

double ParseFloatFixed(const std::string& s, double fallback = 0.0) {
    if (s.empty()) return fallback;
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    bool neg = false;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
        neg = (s[i] == '-');
        ++i;
    }
    double val = 0.0;
    bool anyDigits = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        val = val * 10.0 + static_cast<double>(s[i] - '0');
        anyDigits = true;
        ++i;
    }
    if (i < s.size() && (s[i] == '.' || s[i] == ',')) {
        ++i;
        double place = 0.1;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            val += static_cast<double>(s[i] - '0') * place;
            place *= 0.1;
            anyDigits = true;
            ++i;
        }
    }
    if (!anyDigits) return fallback;
    return neg ? -val : val;
}

std::vector<std::string> SplitPipe(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == '|') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

float ClampF(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

}  // namespace

WallpaperItem* AppConfig::FindItem(const std::wstring& path) {
    if (path.empty()) return nullptr;
    for (auto& it : library) {
        if (_wcsicmp(it.path.c_str(), path.c_str()) == 0) return &it;
    }
    return nullptr;
}

const WallpaperItem* AppConfig::FindItem(const std::wstring& path) const {
    if (path.empty()) return nullptr;
    for (const auto& it : library) {
        if (_wcsicmp(it.path.c_str(), path.c_str()) == 0) return &it;
    }
    return nullptr;
}

WallpaperProfile AppConfig::GetEffectiveProfile(const std::wstring& path) const {
    const WallpaperItem* it = FindItem(path);
    return it ? it->profile : WallpaperProfile{};
}

uint32_t AppConfig::ResolveAccentColorRGB() const {
    switch (accentTheme) {
        case AccentTheme::NeonCyan:      return 0x0EA5E9;
        case AccentTheme::CyberViolet:   return 0xA855F7;
        case AccentTheme::MatrixEmerald: return 0x10B981;
        case AccentTheme::SunsetCrimson: return 0xF43F5E;
        case AccentTheme::AutoFromWallpaper: {
            const WallpaperItem* it = FindItem(activeVideoPath);
            if (it && it->dominantColorRGB != 0) {
                return it->dominantColorRGB;
            }
            return 0x0EA5E9;
        }
    }
    return 0x0EA5E9;
}

bool SaveAppConfig(const AppConfig& cfg, const std::wstring& path) {
    std::ostringstream ss;
    ss << "# WallPulse Studio Configuration v2.0\n";
    ss << "active_video=" << EscapeField(cfg.activeVideoPath) << "\n";
    ss << "user_paused=" << (cfg.userPaused ? "1" : "0") << "\n";
    ss << "mute_when_unfocused=" << (cfg.muteWhenUnfocused ? "1" : "0") << "\n";
    ss << "mouse_parallax=" << (cfg.mouseParallax ? "1" : "0") << "\n";

    ss << "monitor_mode=" << static_cast<int>(cfg.monitorMode) << "\n";
    ss << "target_monitor=" << cfg.targetMonitorIndex << "\n";
    for (size_t i = 0; i < 4; ++i) {
        std::wstring mp = (i < cfg.monitorWallpapers.size()) ? cfg.monitorWallpapers[i] : L"";
        ss << "mon_wp_" << i << "=" << EscapeField(mp) << "\n";
    }

    ss << "audio_vis_mode=" << static_cast<int>(cfg.audioVisMode) << "\n";
    ss << "audio_sens=" << FormatFloatFixed(cfg.audioSensitivity) << "\n";
    ss << "bass_zoom=" << (cfg.bassReactiveZoom ? "1" : "0") << "\n";
    ss << "bass_amount=" << FormatFloatFixed(cfg.bassPulseAmount) << "\n";

    ss << "playlist_min=" << cfg.playlistIntervalMin << "\n";
    ss << "playlist_shuffle=" << (cfg.playlistShuffle ? "1" : "0") << "\n";
    ss << "playlist_fav_only=" << (cfg.playlistFavoritesOnly ? "1" : "0") << "\n";
    ss << "crossfade_sec=" << FormatFloatFixed(cfg.crossfadeSeconds) << "\n";

    ss << "day_night_enabled=" << (cfg.dayNightEnabled ? "1" : "0") << "\n";
    ss << "day_start_hour=" << cfg.dayStartHour << "\n";
    ss << "night_start_hour=" << cfg.nightStartHour << "\n";
    ss << "day_wp=" << EscapeField(cfg.dayWallpaperPath) << "\n";
    ss << "night_wp=" << EscapeField(cfg.nightWallpaperPath) << "\n";

    ss << "widget_clock=" << (cfg.widgetClock ? "1" : "0") << "\n";
    ss << "clock_24h=" << (cfg.clock24h ? "1" : "0") << "\n";
    ss << "clock_sec=" << (cfg.clockShowSeconds ? "1" : "0") << "\n";
    ss << "widget_media=" << (cfg.widgetMedia ? "1" : "0") << "\n";
    ss << "widget_sys=" << (cfg.widgetSysStats ? "1" : "0") << "\n";
    ss << "widget_anchor=" << static_cast<int>(cfg.widgetAnchor) << "\n";
    ss << "widget_scale=" << FormatFloatFixed(cfg.widgetScale) << "\n";
    ss << "widget_opacity=" << FormatFloatFixed(cfg.widgetOpacity) << "\n";

    ss << "taskbar_mode=" << static_cast<int>(cfg.taskbarMode) << "\n";
    ss << "accent_theme=" << static_cast<int>(cfg.accentTheme) << "\n";

    ss << "target_fps=" << cfg.targetFps << "\n";
    ss << "pause_on_fullscreen=" << (cfg.pauseOnFullscreen ? "1" : "0") << "\n";
    ss << "pause_on_max=" << (cfg.pauseOnMaximized ? "1" : "0") << "\n";
    ss << "pause_on_battery=" << (cfg.pauseOnBattery ? "1" : "0") << "\n";
    ss << "close_to_tray=" << (cfg.closeToTray ? "1" : "0") << "\n";
    ss << "start_with_windows=" << (cfg.startWithWindows ? "1" : "0") << "\n";
    ss << "start_minimized=" << (cfg.startMinimized ? "1" : "0") << "\n";
    ss << "global_hotkeys=" << (cfg.globalHotkeys ? "1" : "0") << "\n";
    ss << "shown_tray_hint=" << (cfg.shownTrayHint ? "1" : "0") << "\n";

    for (const auto& item : cfg.library) {
        const auto& p = item.profile;
        ss << "item2="
           << EscapeField(item.path) << "|"
           << EscapeField(item.title) << "|"
           << EscapeField(item.thumbPath) << "|"
           << static_cast<int>(item.kind) << "|"
           << item.width << "|"
           << item.height << "|"
           << FormatFloatFixed(item.durationSec) << "|"
           << item.fileSize << "|"
           << (item.isBuiltIn ? "1" : "0") << "|"
           << (item.favorite ? "1" : "0") << "|"
           << (item.inPlaylist ? "1" : "0") << "|"
           << item.dominantColorRGB << "|"
           << static_cast<int>(p.scaleMode) << "|"
           << (p.muted ? "1" : "0") << "|"
           << FormatFloatFixed(p.volume) << "|"
           << FormatFloatFixed(p.playbackRate) << "|"
           << FormatFloatFixed(p.brightness) << "|"
           << FormatFloatFixed(p.contrast) << "|"
           << FormatFloatFixed(p.saturation) << "|"
           << FormatFloatFixed(p.vignette) << "|"
           << FormatFloatFixed(p.hueShift) << "|"
           << FormatFloatFixed(p.sharpen) << "|"
           << FormatFloatFixed(p.bloom) << "|"
           << FormatFloatFixed(p.chromatic) << "|"
           << FormatFloatFixed(p.crtEffect) << "|"
           << FormatFloatFixed(p.trimStartPct) << "|"
           << FormatFloatFixed(p.trimEndPct) << "|"
           << (p.seamlessLoop ? "1" : "0") << "\n";
    }

    std::string content = ss.str();
    std::wstring tmpPath = path + L".tmp";
    HANDLE hFile = CreateFileW(
        tmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, content.data(), static_cast<DWORD>(content.size()), &written, nullptr);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    if (!ok || written != content.size()) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }
    return MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool LoadAppConfig(AppConfig& cfg, const std::wstring& path) {
    HANDLE hFile = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        cfg.startWithWindows = IsAutoStartEnabledInRegistry();
        cfg.explorerContextMenu = IsExplorerContextMenuEnabled();
        return false;
    }

    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (32LL * 1024LL * 1024LL)) {
        CloseHandle(hFile);
        return false;
    }

    std::string content(static_cast<size_t>(sz.QuadPart), '\0');
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, &content[0], static_cast<DWORD>(content.size()), &bytesRead, nullptr);
    CloseHandle(hFile);
    if (!ok) return false;
    content.resize(bytesRead);

    cfg.library.clear();
    cfg.monitorWallpapers.assign(4, L"");

    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "active_video") cfg.activeVideoPath = UnescapeField(val);
        else if (key == "user_paused") cfg.userPaused = (val == "1");
        else if (key == "mute_when_unfocused") cfg.muteWhenUnfocused = (val == "1");
        else if (key == "mouse_parallax") cfg.mouseParallax = (val == "1");
        else if (key == "monitor_mode") {
            int m = std::atoi(val.c_str());
            if (m >= 0 && m <= 4) cfg.monitorMode = static_cast<MonitorMode>(m);
        }
        else if (key == "target_monitor") cfg.targetMonitorIndex = std::clamp(std::atoi(val.c_str()), 0, 15);
        else if (key == "mon_wp_0") cfg.monitorWallpapers[0] = UnescapeField(val);
        else if (key == "mon_wp_1") cfg.monitorWallpapers[1] = UnescapeField(val);
        else if (key == "mon_wp_2") cfg.monitorWallpapers[2] = UnescapeField(val);
        else if (key == "mon_wp_3") cfg.monitorWallpapers[3] = UnescapeField(val);

        else if (key == "audio_vis_mode") {
            int m = std::atoi(val.c_str());
            if (m >= 0 && m <= 3) cfg.audioVisMode = static_cast<AudioVisMode>(m);
        }
        else if (key == "audio_sens") cfg.audioSensitivity = ClampF(static_cast<float>(ParseFloatFixed(val, 1.15)), 0.25f, 3.0f);
        else if (key == "bass_zoom") cfg.bassReactiveZoom = (val == "1");
        else if (key == "bass_amount") cfg.bassPulseAmount = ClampF(static_cast<float>(ParseFloatFixed(val, 0.40)), 0.0f, 1.0f);

        else if (key == "playlist_min") cfg.playlistIntervalMin = std::clamp(std::atoi(val.c_str()), 0, 1440);
        else if (key == "playlist_shuffle") cfg.playlistShuffle = (val == "1");
        else if (key == "playlist_fav_only") cfg.playlistFavoritesOnly = (val == "1");
        else if (key == "crossfade_sec") cfg.crossfadeSeconds = ClampF(static_cast<float>(ParseFloatFixed(val, 1.1)), 0.0f, 2.5f);

        else if (key == "day_night_enabled") cfg.dayNightEnabled = (val == "1");
        else if (key == "day_start_hour") cfg.dayStartHour = std::clamp(std::atoi(val.c_str()), 0, 23);
        else if (key == "night_start_hour") cfg.nightStartHour = std::clamp(std::atoi(val.c_str()), 0, 23);
        else if (key == "day_wp") cfg.dayWallpaperPath = UnescapeField(val);
        else if (key == "night_wp") cfg.nightWallpaperPath = UnescapeField(val);

        else if (key == "widget_clock") cfg.widgetClock = (val == "1");
        else if (key == "clock_24h") cfg.clock24h = (val == "1");
        else if (key == "clock_sec") cfg.clockShowSeconds = (val == "1");
        else if (key == "widget_media") cfg.widgetMedia = (val == "1");
        else if (key == "widget_sys") cfg.widgetSysStats = (val == "1");
        else if (key == "widget_anchor") {
            int a = std::atoi(val.c_str());
            if (a >= 0 && a <= 3) cfg.widgetAnchor = static_cast<WidgetAnchor>(a);
        }
        else if (key == "widget_scale") cfg.widgetScale = ClampF(static_cast<float>(ParseFloatFixed(val, 1.0)), 0.75f, 1.50f);
        else if (key == "widget_opacity") cfg.widgetOpacity = ClampF(static_cast<float>(ParseFloatFixed(val, 0.68)), 0.20f, 0.95f);

        else if (key == "taskbar_mode") {
            int t = std::atoi(val.c_str());
            if (t >= 0 && t <= 2) cfg.taskbarMode = static_cast<TaskbarMode>(t);
        }
        else if (key == "accent_theme") {
            int a = std::atoi(val.c_str());
            if (a >= 0 && a <= 4) cfg.accentTheme = static_cast<AccentTheme>(a);
        }

        else if (key == "target_fps") cfg.targetFps = static_cast<uint32_t>(std::clamp(std::atoi(val.c_str()), 15, 240));
        else if (key == "pause_on_fullscreen") cfg.pauseOnFullscreen = (val == "1");
        else if (key == "pause_on_max") cfg.pauseOnMaximized = (val == "1");
        else if (key == "pause_on_battery") cfg.pauseOnBattery = (val == "1");
        else if (key == "close_to_tray") cfg.closeToTray = (val == "1");
        else if (key == "start_with_windows") cfg.startWithWindows = (val == "1");
        else if (key == "start_minimized") cfg.startMinimized = (val == "1");
        else if (key == "global_hotkeys") cfg.globalHotkeys = (val == "1");
        else if (key == "shown_tray_hint") cfg.shownTrayHint = (val == "1");

        else if (key == "item2") {
            auto parts = SplitPipe(val);
            if (parts.size() >= 28) {
                WallpaperItem item;
                item.path = UnescapeField(parts[0]);
                item.title = UnescapeField(parts[1]);
                item.thumbPath = UnescapeField(parts[2]);
                int k = std::atoi(parts[3].c_str());
                item.kind = (k >= 0 && k <= 3) ? static_cast<WallpaperKind>(k) : ClassifyWallpaperPath(item.path);
                item.width = static_cast<uint32_t>(std::strtoul(parts[4].c_str(), nullptr, 10));
                item.height = static_cast<uint32_t>(std::strtoul(parts[5].c_str(), nullptr, 10));
                item.durationSec = ParseFloatFixed(parts[6], 0.0);
                item.fileSize = std::strtoull(parts[7].c_str(), nullptr, 10);
                item.isBuiltIn = (parts[8] == "1");
                item.favorite = (parts[9] == "1");
                item.inPlaylist = (parts[10] == "1");
                item.dominantColorRGB = static_cast<uint32_t>(std::strtoul(parts[11].c_str(), nullptr, 10));

                auto& p = item.profile;
                int sm = std::atoi(parts[12].c_str());
                p.scaleMode = (sm >= 0 && sm <= 4) ? static_cast<ScaleMode>(sm) : ScaleMode::Cover;
                p.muted = (parts[13] == "1");
                p.volume = ClampF(static_cast<float>(ParseFloatFixed(parts[14], 0.5)), 0.0f, 1.0f);
                p.playbackRate = ClampF(static_cast<float>(ParseFloatFixed(parts[15], 1.0)), 0.25f, 2.0f);
                p.brightness = ClampF(static_cast<float>(ParseFloatFixed(parts[16], 0.0)), -0.5f, 0.5f);
                p.contrast = ClampF(static_cast<float>(ParseFloatFixed(parts[17], 1.0)), 0.5f, 1.5f);
                p.saturation = ClampF(static_cast<float>(ParseFloatFixed(parts[18], 1.0)), 0.0f, 2.0f);
                p.vignette = ClampF(static_cast<float>(ParseFloatFixed(parts[19], 0.15)), 0.0f, 0.8f);
                p.hueShift = ClampF(static_cast<float>(ParseFloatFixed(parts[20], 0.0)), -180.0f, 180.0f);
                p.sharpen = ClampF(static_cast<float>(ParseFloatFixed(parts[21], 0.15)), 0.0f, 1.0f);
                p.bloom = ClampF(static_cast<float>(ParseFloatFixed(parts[22], 0.12)), 0.0f, 1.0f);
                p.chromatic = ClampF(static_cast<float>(ParseFloatFixed(parts[23], 0.0)), 0.0f, 1.0f);
                p.crtEffect = ClampF(static_cast<float>(ParseFloatFixed(parts[24], 0.0)), 0.0f, 1.0f);
                p.trimStartPct = ClampF(static_cast<float>(ParseFloatFixed(parts[25], 0.0)), 0.0f, 0.90f);
                p.trimEndPct = ClampF(static_cast<float>(ParseFloatFixed(parts[26], 1.0)), 0.10f, 1.0f);
                if (p.trimEndPct <= p.trimStartPct + 0.05f) p.trimEndPct = std::min(1.0f, p.trimStartPct + 0.10f);
                p.seamlessLoop = (parts[27] == "1");

                if (!item.path.empty()) {
                    cfg.library.push_back(std::move(item));
                }
            }
        }
    }

    cfg.startWithWindows = IsAutoStartEnabledInRegistry();
    cfg.explorerContextMenu = IsExplorerContextMenuEnabled();
    return true;
}

bool IsAutoStartEnabledInRegistry() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunRegKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD cb = sizeof(buf);
    DWORD type = 0;
    LSTATUS st = RegQueryValueExW(hKey, kRunValueName, nullptr, &type,
                                  reinterpret_cast<LPBYTE>(buf), &cb);
    RegCloseKey(hKey);
    return (st == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && buf[0] != L'\0');
}

bool SetAutoStartInRegistry(bool enable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunRegKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    LSTATUS st = ERROR_SUCCESS;
    if (enable) {
        std::wstring cmd = L"\"" + GetExePath() + L"\" --minimized";
        DWORD cb = static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t));
        st = RegSetValueExW(hKey, kRunValueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(cmd.c_str()), cb);
    } else {
        st = RegDeleteValueW(hKey, kRunValueName);
        if (st == ERROR_FILE_NOT_FOUND) st = ERROR_SUCCESS;
    }
    RegCloseKey(hKey);
    return st == ERROR_SUCCESS;
}

bool IsExplorerContextMenuEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Classes\\SystemFileAssociations\\.mp4\\shell\\WallPulse\\command",
            0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    RegCloseKey(hKey);
    return true;
}

bool SetExplorerContextMenu(bool enable) {
    const wchar_t* exts[] = {L".mp4", L".webm", L".mkv", L".gif", L".html"};
    bool allOk = true;

    for (const wchar_t* ext : exts) {
        std::wstring baseKey = std::wstring(L"Software\\Classes\\SystemFileAssociations\\") + ext + L"\\shell\\WallPulse";
        if (!enable) {
            LSTATUS st = RegDeleteTreeW(HKEY_CURRENT_USER, baseKey.c_str());
            if (st != ERROR_SUCCESS && st != ERROR_FILE_NOT_FOUND) allOk = false;
            continue;
        }

        HKEY hShell = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, baseKey.c_str(), 0, nullptr, 0,
                            KEY_SET_VALUE, nullptr, &hShell, nullptr) != ERROR_SUCCESS) {
            allOk = false;
            continue;
        }
        std::wstring label = L"Поставить на рабочий стол (WallPulse)";
        std::wstring iconVal = L"\"" + GetExePath() + L"\",0";
        RegSetValueExW(hShell, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(label.c_str()),
                       static_cast<DWORD>((label.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hShell, L"Icon", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(iconVal.c_str()),
                       static_cast<DWORD>((iconVal.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hShell);

        std::wstring cmdKey = baseKey + L"\\command";
        HKEY hCmd = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKey.c_str(), 0, nullptr, 0,
                            KEY_SET_VALUE, nullptr, &hCmd, nullptr) == ERROR_SUCCESS) {
            std::wstring cmd = L"\"" + GetExePath() + L"\" \"%1\"";
            RegSetValueExW(hCmd, nullptr, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(cmd.c_str()),
                           static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hCmd);
        }
    }
    return allOk;
}

}  // namespace wp
