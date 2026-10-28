#include "common.h"

#include <knownfolders.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <deque>

namespace wp {

namespace {
std::mutex g_logMutex;
std::deque<std::wstring> g_recentLogs;
constexpr size_t kMaxStoredLogs = 160;

void AppendLogLine(const wchar_t* level, const std::wstring& msg) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t prefix[64];
    swprintf_s(prefix, L"[%02u:%02u:%02u.%03u] [%ls] ",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
    std::wstring full = std::wstring(prefix) + msg;

    std::lock_guard<std::mutex> lock(g_logMutex);
    g_recentLogs.push_back(full);
    if (g_recentLogs.size() > kMaxStoredLogs) {
        g_recentLogs.pop_front();
    }

    std::wstring logPath = GetLogPath();
    HANDLE hFile = CreateFileW(
        logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        std::string utf8 = WideToUtf8(full) + "\r\n";
        DWORD written = 0;
        WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
        CloseHandle(hFile);
    }
}
}  // namespace

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        &out[0], needed, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                     nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        &out[0], needed);
    return out;
}

std::wstring ToLowerUnicode(const std::wstring& s) {
    if (s.empty()) return {};
    std::wstring copy = s;
    CharLowerBuffW(&copy[0], static_cast<DWORD>(copy.size()));
    return copy;
}

bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    std::wstring h = ToLowerUnicode(haystack);
    std::wstring n = ToLowerUnicode(needle);
    return h.find(n) != std::wstring::npos;
}

std::wstring GetExePath() {
    std::wstring buf(1024, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
    buf.resize(len);
    return buf;
}

std::wstring GetExeDir() {
    std::wstring p = GetExePath();
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

bool EnsureDirExists(const std::wstring& dir) {
    if (dir.empty()) return false;
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    int rc = SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    return rc == ERROR_SUCCESS || rc == ERROR_ALREADY_EXISTS || rc == ERROR_FILE_EXISTS;
}

std::wstring GetAppDataDir() {
    wchar_t envBuf[MAX_PATH * 2] = {};
    DWORD n = GetEnvironmentVariableW(L"WALLPULSE_DATA_DIR", envBuf, MAX_PATH * 2);
    if (n > 0 && n < MAX_PATH * 2) {
        std::wstring custom(envBuf, n);
        EnsureDirExists(custom);
        return custom;
    }

    PWSTR knownPath = nullptr;
    std::wstring base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &knownPath)) && knownPath) {
        base = knownPath;
        CoTaskMemFree(knownPath);
    } else {
        DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", envBuf, MAX_PATH * 2);
        if (len > 0 && len < MAX_PATH * 2) {
            base.assign(envBuf, len);
        } else {
            base = GetExeDir();
        }
    }
    std::wstring dir = base + L"\\WallPulse";
    EnsureDirExists(dir);
    return dir;
}

std::wstring GetSamplesDir() {
    std::wstring d = GetAppDataDir() + L"\\samples";
    EnsureDirExists(d);
    return d;
}

std::wstring GetThumbsDir() {
    std::wstring d = GetAppDataDir() + L"\\thumbs";
    EnsureDirExists(d);
    return d;
}

std::wstring GetConfigPath() {
    return GetAppDataDir() + L"\\config_v2.cfg";
}

std::wstring GetLogPath() {
    return GetAppDataDir() + L"\\wallpulse.log";
}

bool IsVirtualWallpaperUri(const std::wstring& path) {
    return path.rfind(L"shader://", 0) == 0;
}

bool FileExists(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool WallpaperSourceExists(const std::wstring& path) {
    if (path.empty()) return false;
    if (IsVirtualWallpaperUri(path)) return true;
    return FileExists(path);
}

uint64_t GetFileSizeBytes(const std::wstring& path) {
    if (IsVirtualWallpaperUri(path)) return 0;
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
        return 0;
    }
    return (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
}

std::wstring GetFileNameWithoutExt(const std::wstring& path) {
    if (IsVirtualWallpaperUri(path)) {
        std::wstring slug = path.substr(9);
        if (slug == L"cyber_tunnel") return L"Киберпанк Туннель (3D Shader)";
        if (slug == L"sunset_ocean") return L"Закатный Океан (GPU Shader)";
        if (slug == L"matrix_pulse") return L"Неоновая Матрица (GPU Shader)";
        return slug;
    }
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    for (wchar_t& ch : name) {
        if (ch == L'_') ch = L' ';
    }
    return name;
}

std::wstring GetFileExtensionLower(const std::wstring& path) {
    if (IsVirtualWallpaperUri(path)) return L".shader";
    size_t dot = path.find_last_of(L'.');
    size_t slash = path.find_last_of(L"\\/");
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return {};
    }
    return ToLowerUnicode(path.substr(dot));
}

WallpaperKind ClassifyWallpaperPath(const std::wstring& path) {
    if (IsVirtualWallpaperUri(path)) return WallpaperKind::Shader;
    std::wstring ext = GetFileExtensionLower(path);
    if (ext == L".gif") return WallpaperKind::Gif;
    if (ext == L".hlsl" || ext == L".shader") return WallpaperKind::Shader;
    if (ext == L".html" || ext == L".htm") return WallpaperKind::Web;
    return WallpaperKind::Video;
}

bool IsSupportedVideoExtension(const std::wstring& path) {
    std::wstring ext = GetFileExtensionLower(path);
    return ext == L".mp4" || ext == L".m4v" || ext == L".mov" ||
           ext == L".webm" || ext == L".mkv" || ext == L".avi" || ext == L".wmv";
}

bool IsSupportedWallpaperExtension(const std::wstring& path) {
    if (IsVirtualWallpaperUri(path)) return true;
    std::wstring ext = GetFileExtensionLower(path);
    return IsSupportedVideoExtension(path) ||
           ext == L".gif" || ext == L".hlsl" || ext == L".shader" ||
           ext == L".html" || ext == L".htm";
}

const wchar_t* WallpaperKindBadge(WallpaperKind kind, const std::wstring& path) {
    switch (kind) {
        case WallpaperKind::Gif:    return L"GIF";
        case WallpaperKind::Shader: return L"SHADER";
        case WallpaperKind::Web:    return L"HTML5";
        case WallpaperKind::Video: {
            std::wstring ext = GetFileExtensionLower(path);
            if (ext == L".webm") return L"WEBM";
            if (ext == L".mkv")  return L"MKV";
            if (ext == L".mov")  return L"MOV";
            return L"MP4";
        }
    }
    return L"MP4";
}

std::wstring FormatDuration(double seconds) {
    if (seconds <= 0.0 || seconds > 360000.0) return L"60 FPS";
    int total = static_cast<int>(seconds + 0.5);
    int mins = total / 60;
    int secs = total % 60;
    wchar_t buf[32];
    if (mins >= 60) {
        swprintf_s(buf, L"%d:%02d:%02d", mins / 60, mins % 60, secs);
    } else {
        swprintf_s(buf, L"%02d:%02d", mins, secs);
    }
    return buf;
}

std::wstring FormatFileSize(uint64_t bytes) {
    if (bytes == 0) return L"GPU";
    wchar_t buf[48];
    if (bytes >= (1024ULL * 1024ULL * 1024ULL)) {
        swprintf_s(buf, L"%.2f ГБ", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= (1024ULL * 1024ULL)) {
        swprintf_s(buf, L"%.1f МБ", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024ULL) {
        swprintf_s(buf, L"%llu КБ", static_cast<unsigned long long>(bytes / 1024ULL));
    } else {
        swprintf_s(buf, L"%llu Б", static_cast<unsigned long long>(bytes));
    }
    return buf;
}

std::wstring FormatRussianDate(const SYSTEMTIME& st) {
    static const wchar_t* kDays[7] = {
        L"Воскресенье", L"Понедельник", L"Вторник",
        L"Среда", L"Четверг", L"Пятница", L"Суббота"
    };
    static const wchar_t* kMonths[12] = {
        L"января", L"февраля", L"марта", L"апреля", L"мая", L"июня",
        L"июля", L"августа", L"сентября", L"октября", L"ноября", L"декабря"
    };
    const wchar_t* dayName = kDays[st.wDayOfWeek % 7];
    const wchar_t* monthName = (st.wMonth >= 1 && st.wMonth <= 12) ? kMonths[st.wMonth - 1] : L"";
    wchar_t buf[96];
    swprintf_s(buf, L"%ls, %u %ls", dayName, st.wDay, monthName);
    return buf;
}

uint64_t HashStringFNV1a(const std::wstring& s) {
    std::wstring low = ToLowerUnicode(s);
    uint64_t h = 14695981039346656037ULL;
    for (wchar_t ch : low) {
        h ^= static_cast<uint64_t>(ch & 0xFF);
        h *= 1099511628211ULL;
        h ^= static_cast<uint64_t>((ch >> 8) & 0xFF);
        h *= 1099511628211ULL;
    }
    return h;
}

void EnablePerMonitorDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetDpiCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto fn = reinterpret_cast<SetDpiCtxFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (fn && fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }
    SetProcessDPIAware();
}

float GetWindowDpiScale(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 && hwnd) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto fn = reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(user32, "GetDpiForWindow"));
        if (fn) {
            UINT dpi = fn(hwnd);
            if (dpi >= 72 && dpi <= 576) {
                return static_cast<float>(dpi) / 96.0f;
            }
        }
    }
    HDC hdc = GetDC(hwnd);
    int dpiX = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return (dpiX > 0) ? (static_cast<float>(dpiX) / 96.0f) : 1.0f;
}

void LogInfo(const std::wstring& msg) { AppendLogLine(L"INFO", msg); }
void LogWarn(const std::wstring& msg) { AppendLogLine(L"WARN", msg); }
void LogError(const std::wstring& msg) { AppendLogLine(L"ERR ", msg); }

void LogHRESULT(const std::wstring& context, HRESULT hr) {
    wchar_t buf[64];
    swprintf_s(buf, L" (HRESULT=0x%08X)", static_cast<unsigned int>(hr));
    AppendLogLine(FAILED(hr) ? L"ERR " : L"INFO", context + buf);
}

std::vector<std::wstring> GetRecentLogs(size_t maxLines) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::vector<std::wstring> out;
    size_t start = (g_recentLogs.size() > maxLines) ? (g_recentLogs.size() - maxLines) : 0;
    for (size_t i = start; i < g_recentLogs.size(); ++i) {
        out.push_back(g_recentLogs[i]);
    }
    return out;
}

}  // namespace wp
