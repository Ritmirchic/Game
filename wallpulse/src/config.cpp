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

// Locale-independent float formatting (always uses '.' even on Russian locale)
std::string FormatFloatFixed(double v) {
    if (!std::isfinite(v)) return "0.0000";
    bool neg = v < 0.0;
    double av = std::fabs(v);
    unsigned long long whole = static_cast<unsigned long long>(av);
    unsigned long long frac = static_cast<unsigned long long>(std::llround((av - static_cast<double>(whole)) * 10000.0));
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

bool SaveAppConfig(const AppConfig& cfg, const std::wstring& path) {
    std::ostringstream ss;
    ss << "# WallPulse Studio Configuration v1\n";
    ss << "active_video=" << EscapeField(cfg.activeVideoPath) << "\n";
    ss << "user_paused=" << (cfg.userPaused ? "1" : "0") << "\n";
    ss << "muted=" << (cfg.muted ? "1" : "0") << "\n";
    ss << "volume=" << FormatFloatFixed(cfg.volume) << "\n";
    ss << "mute_when_unfocused=" << (cfg.muteWhenUnfocused ? "1" : "0") << "\n";
    ss << "playback_rate=" << FormatFloatFixed(cfg.playbackRate) << "\n";
    ss << "scale_mode=" << static_cast<int>(cfg.scaleMode) << "\n";
    ss << "monitor_mode=" << static_cast<int>(cfg.monitorMode) << "\n";
    ss << "target_monitor=" << cfg.targetMonitorIndex << "\n";
    ss << "brightness=" << FormatFloatFixed(cfg.brightness) << "\n";
    ss << "contrast=" << FormatFloatFixed(cfg.contrast) << "\n";
    ss << "saturation=" << FormatFloatFixed(cfg.saturation) << "\n";
    ss << "vignette=" << FormatFloatFixed(cfg.vignette) << "\n";
    ss << "mouse_parallax=" << (cfg.mouseParallax ? "1" : "0") << "\n";
    ss << "pause_on_fullscreen=" << (cfg.pauseOnFullscreen ? "1" : "0") << "\n";
    ss << "pause_on_battery=" << (cfg.pauseOnBattery ? "1" : "0") << "\n";
    ss << "close_to_tray=" << (cfg.closeToTray ? "1" : "0") << "\n";
    ss << "start_with_windows=" << (cfg.startWithWindows ? "1" : "0") << "\n";
    ss << "shown_tray_hint=" << (cfg.shownTrayHint ? "1" : "0") << "\n";

    for (const auto& item : cfg.library) {
        ss << "item="
           << EscapeField(item.path) << "|"
           << EscapeField(item.title) << "|"
           << EscapeField(item.thumbPath) << "|"
           << item.width << "|"
           << item.height << "|"
           << FormatFloatFixed(item.durationSec) << "|"
           << item.fileSize << "|"
           << (item.isBuiltIn ? "1" : "0") << "\n";
    }

    std::string content = ss.str();
    std::wstring tmpPath = path + L".tmp";
    HANDLE hFile = CreateFileW(
        tmpPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
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
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        cfg.startWithWindows = IsAutoStartEnabledInRegistry();
        return false;
    }

    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (16LL * 1024LL * 1024LL)) {
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
        else if (key == "muted") cfg.muted = (val == "1");
        else if (key == "volume") cfg.volume = ClampF(static_cast<float>(ParseFloatFixed(val, 0.5)), 0.0f, 1.0f);
        else if (key == "mute_when_unfocused") cfg.muteWhenUnfocused = (val == "1");
        else if (key == "playback_rate") cfg.playbackRate = ClampF(static_cast<float>(ParseFloatFixed(val, 1.0)), 0.25f, 2.0f);
        else if (key == "scale_mode") {
            int m = std::atoi(val.c_str());
            if (m >= 0 && m <= 4) cfg.scaleMode = static_cast<ScaleMode>(m);
        }
        else if (key == "monitor_mode") {
            int m = std::atoi(val.c_str());
            if (m >= 0 && m <= 3) cfg.monitorMode = static_cast<MonitorMode>(m);
        }
        else if (key == "target_monitor") cfg.targetMonitorIndex = std::max(0, std::atoi(val.c_str()));
        else if (key == "brightness") cfg.brightness = ClampF(static_cast<float>(ParseFloatFixed(val, 0.0)), -0.5f, 0.5f);
        else if (key == "contrast") cfg.contrast = ClampF(static_cast<float>(ParseFloatFixed(val, 1.0)), 0.5f, 1.5f);
        else if (key == "saturation") cfg.saturation = ClampF(static_cast<float>(ParseFloatFixed(val, 1.0)), 0.0f, 2.0f);
        else if (key == "vignette") cfg.vignette = ClampF(static_cast<float>(ParseFloatFixed(val, 0.15)), 0.0f, 0.8f);
        else if (key == "mouse_parallax") cfg.mouseParallax = (val == "1");
        else if (key == "pause_on_fullscreen") cfg.pauseOnFullscreen = (val == "1");
        else if (key == "pause_on_battery") cfg.pauseOnBattery = (val == "1");
        else if (key == "close_to_tray") cfg.closeToTray = (val == "1");
        else if (key == "start_with_windows") cfg.startWithWindows = (val == "1");
        else if (key == "shown_tray_hint") cfg.shownTrayHint = (val == "1");
        else if (key == "item") {
            auto parts = SplitPipe(val);
            if (parts.size() >= 8) {
                WallpaperItem item;
                item.path = UnescapeField(parts[0]);
                item.title = UnescapeField(parts[1]);
                item.thumbPath = UnescapeField(parts[2]);
                item.width = static_cast<uint32_t>(std::strtoul(parts[3].c_str(), nullptr, 10));
                item.height = static_cast<uint32_t>(std::strtoul(parts[4].c_str(), nullptr, 10));
                item.durationSec = ParseFloatFixed(parts[5], 0.0);
                item.fileSize = std::strtoull(parts[6].c_str(), nullptr, 10);
                item.isBuiltIn = (parts[7] == "1");
                if (!item.path.empty()) {
                    cfg.library.push_back(std::move(item));
                }
            }
        }
    }

    cfg.startWithWindows = IsAutoStartEnabledInRegistry();
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
    constexpr const wchar_t* kBaseKey =
        L"Software\\Classes\\SystemFileAssociations\\.mp4\\shell\\WallPulse";
    if (!enable) {
        LSTATUS st = RegDeleteTreeW(HKEY_CURRENT_USER, kBaseKey);
        return st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND;
    }

    HKEY hShell = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kBaseKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hShell, nullptr) != ERROR_SUCCESS) {
        return false;
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

    std::wstring cmdKey = std::wstring(kBaseKey) + L"\\command";
    HKEY hCmd = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKey.c_str(), 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hCmd, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    std::wstring cmd = L"\"" + GetExePath() + L"\" \"%1\"";
    LSTATUS st = RegSetValueExW(
        hCmd, nullptr, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(cmd.c_str()),
        static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hCmd);
    return st == ERROR_SUCCESS;
}

}  // namespace wp
