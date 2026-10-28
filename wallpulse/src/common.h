#pragma once

#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <unknwn.h>

#include <cstdint>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

#define IDI_WALLPULSE_ICON 101
#define WP_VERSION_STR "3.0.0"

namespace wp {

template <typename T>
class ComPtr {
public:
    ComPtr() noexcept : ptr_(nullptr) {}
    ComPtr(std::nullptr_t) noexcept : ptr_(nullptr) {}
    explicit ComPtr(T* p) noexcept : ptr_(p) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    ~ComPtr() {
        Reset();
    }

    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != &other) {
            if (other.ptr_) other.ptr_->AddRef();
            Reset();
            ptr_ = other.ptr_;
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void Reset() noexcept {
        T* tmp = ptr_;
        ptr_ = nullptr;
        if (tmp) tmp->Release();
    }

    T* Get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }
    bool operator!=(std::nullptr_t) const noexcept { return ptr_ != nullptr; }

    T** GetAddressOf() noexcept { return &ptr_; }
    T** ReleaseAndGetAddressOf() noexcept {
        Reset();
        return &ptr_;
    }

    T* Detach() noexcept {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }

    void Attach(T* p) noexcept {
        Reset();
        ptr_ = p;
    }

    template <typename U>
    HRESULT As(ComPtr<U>* out) const noexcept {
        if (!out) return E_POINTER;
        out->Reset();
        if (!ptr_) return E_NOINTERFACE;
        return ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(out->GetAddressOf()));
    }

private:
    T* ptr_;
};

enum class WallpaperKind : int {
    Video = 0,   // .mp4, .webm, .mkv, .mov, .avi, .wmv
    Gif = 1,     // .gif animated WIC frames
    Shader = 2,  // shader:// built-in or .hlsl procedural 60 FPS GPU scene
    Web = 3      // .html / .htm interactive HTML5 scene
};

// String & Unicode helpers
std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);
std::wstring ToLowerUnicode(const std::wstring& s);
bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle);

// App directories & file helpers
std::wstring GetExePath();
std::wstring GetExeDir();
std::wstring GetAppDataDir();
std::wstring GetSamplesDir();
std::wstring GetThumbsDir();
std::wstring GetConfigPath();
std::wstring GetLegacyConfigV2Path();
std::wstring GetLogPath();

bool EnsureDirExists(const std::wstring& dir);
bool FileExists(const std::wstring& path);
bool IsVirtualWallpaperUri(const std::wstring& path);
bool WallpaperSourceExists(const std::wstring& path);
uint64_t GetFileSizeBytes(const std::wstring& path);
std::wstring GetFileNameWithoutExt(const std::wstring& path);
std::wstring GetFileExtensionLower(const std::wstring& path);

WallpaperKind ClassifyWallpaperPath(const std::wstring& path);
bool IsSupportedVideoExtension(const std::wstring& path);
bool IsSupportedWallpaperExtension(const std::wstring& path);
const wchar_t* WallpaperKindBadge(WallpaperKind kind, const std::wstring& path);

std::wstring FormatDuration(double seconds);
std::wstring FormatFileSize(uint64_t bytes);
std::wstring FormatRussianDate(const SYSTEMTIME& st);
uint64_t HashStringFNV1a(const std::wstring& s);

// High-DPI initialization
void EnablePerMonitorDpiAwareness();
float GetWindowDpiScale(HWND hwnd);

// Thread-safe logger + recent lines buffer
void LogInfo(const std::wstring& msg);
void LogWarn(const std::wstring& msg);
void LogError(const std::wstring& msg);
void LogHRESULT(const std::wstring& context, HRESULT hr);
std::vector<std::wstring> GetRecentLogs(size_t maxLines = 40);

}  // namespace wp
