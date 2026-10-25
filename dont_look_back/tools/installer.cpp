// ============================================================================
//  DON'T LOOK BACK -- self-contained Windows installer
//
//  A single .exe that carries the game payload appended behind the PE image.
//  It installs per-user (no administrator rights required), draws its own
//  progress UI with GDI, creates Desktop and Start-menu shortcuts with the
//  game icon and finally launches the game.
//
//  Payload layout (appended to this executable):
//    for each file:  u16 nameLen | name (UTF-8, '/')  | u32 size | data
//    footer:         u64 offset | u32 count | "DLBPAK01"
// ============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#pragma pack(push, 1)
struct Footer {
    unsigned long long offset;
    unsigned int count;
    char magic[8];
};
#pragma pack(pop)

static const char kMagic[8] = {'D', 'L', 'B', 'P', 'A', 'K', '0', '1'};
static const int  WM_PROGRESS = WM_APP + 1;
static const int  WM_INSTALLED = WM_APP + 2;

static HWND g_hwnd = nullptr;
static int  g_percent = 0;
static wchar_t g_status[256] = L"Preparing...";
static wchar_t g_installDir[MAX_PATH] = L"";
static bool g_launch = true;

// ---------------------------------------------------------------- helpers
static std::wstring exePath() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    return std::wstring(buf, n);
}

static std::wstring dirName(const std::wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? std::wstring() : p.substr(0, s);
}

static bool readAll(const std::wstring& path, std::vector<unsigned char>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) { CloseHandle(h); return false; }
    out.resize((size_t)size.QuadPart);
    DWORD read = 0;
    bool ok = out.empty() || (ReadFile(h, out.data(), (DWORD)out.size(), &read, nullptr) != 0);
    CloseHandle(h);
    return ok && (out.empty() || read == out.size());
}

static void makeDirs(const std::wstring& path) {
    std::wstring cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == L'\\' || i + 1 == path.size()) {
            if (cur.size() > 3) CreateDirectoryW(cur.c_str(), nullptr);
        }
    }
}

static bool writeFileW(const std::wstring& path, const unsigned char* data, size_t n) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = n == 0 || (WriteFile(h, data, (DWORD)n, &written, nullptr) != 0);
    CloseHandle(h);
    return ok && written == n;
}

static std::wstring utf8toW(const char* s, int n) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s, n, nullptr, 0);
    std::wstring w((size_t)len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, n, &w[0], len);
    return w;
}

static std::wstring knownFolder(REFKNOWNFOLDERID id) {
    wchar_t* p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p) {
        out = p;
        CoTaskMemFree(p);
    }
    return out;
}

static bool makeShortcut(const std::wstring& lnkPath, const std::wstring& target,
                         const std::wstring& workDir, const std::wstring& icon,
                         const std::wstring& desc) {
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void**)&link)))
        return false;
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(workDir.c_str());
    link->SetDescription(desc.c_str());
    if (!icon.empty()) link->SetIconLocation(icon.c_str(), 0);
    IPersistFile* file = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&file))) {
        ok = SUCCEEDED(file->Save(lnkPath.c_str(), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}

// ---------------------------------------------------------------- UI
static void paint(HDC dc, int w, int h) {
    // dark, quiet installer window
    HBRUSH bg = CreateSolidBrush(RGB(12, 13, 16));
    RECT all = {0, 0, w, h};
    FillRect(dc, &all, bg);
    DeleteObject(bg);

    SetBkMode(dc, TRANSPARENT);
    HFONT title = CreateFontW(-30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Segoe UI");
    HFONT body = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(dc, title);
    SetTextColor(dc, RGB(226, 216, 196));
    TextOutW(dc, 34, 34, L"DON'T LOOK BACK", 15);
    SelectObject(dc, body);
    SetTextColor(dc, RGB(120, 116, 108));
    TextOutW(dc, 34, 74, L"A psychological horror about noticing what is wrong.", 51);

    // progress bar
    RECT bar = {34, 130, w - 34, 138};
    HBRUSH track = CreateSolidBrush(RGB(30, 31, 36));
    FillRect(dc, &bar, track);
    DeleteObject(track);
    RECT fill = bar;
    fill.right = bar.left + (LONG)((bar.right - bar.left) * (g_percent / 100.0f));
    HBRUSH acc = CreateSolidBrush(RGB(190, 160, 110));
    FillRect(dc, &fill, acc);
    DeleteObject(acc);

    SetTextColor(dc, RGB(150, 146, 138));
    TextOutW(dc, 34, 152, g_status, (int)wcslen(g_status));
    wchar_t pct[32];
    wsprintfW(pct, L"%d%%", g_percent);
    SetTextColor(dc, RGB(190, 160, 110));
    TextOutW(dc, w - 70, 152, pct, (int)wcslen(pct));

    if (g_percent >= 100) {
        SetTextColor(dc, RGB(200, 196, 188));
        TextOutW(dc, 34, 196, L"Installed. The game is starting...", 34);
    }

    SelectObject(dc, old);
    DeleteObject(title);
    DeleteObject(body);
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT r;
            GetClientRect(hwnd, &r);
            paint(dc, r.right, r.bottom);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

static void setProgress(int pct, const wchar_t* status) {
    g_percent = pct;
    if (status) lstrcpynW(g_status, status, 256);
    if (g_hwnd) {
        InvalidateRect(g_hwnd, nullptr, FALSE);
        UpdateWindow(g_hwnd);
    }
    // keep the window responsive while installing
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

// ---------------------------------------------------------------- install
static bool install() {
    std::vector<unsigned char> self;
    if (!readAll(exePath(), self) || self.size() < sizeof(Footer)) return false;
    Footer f;
    memcpy(&f, self.data() + self.size() - sizeof(Footer), sizeof(Footer));
    if (memcmp(f.magic, kMagic, 8) != 0) {
        MessageBoxW(nullptr, L"This installer is damaged (no payload).", L"DON'T LOOK BACK", MB_ICONERROR);
        return false;
    }
    if (f.offset + 0 >= (unsigned long long)self.size()) return false;

    // target folder: %LOCALAPPDATA%\Programs\DontLookBack (no admin needed)
    std::wstring base = knownFolder(FOLDERID_LocalAppData);
    if (base.empty()) base = knownFolder(FOLDERID_Profile);
    if (base.empty()) return false;
    std::wstring dir = base + L"\\Programs\\DontLookBack";
    lstrcpynW(g_installDir, dir.c_str(), MAX_PATH);
    makeDirs(dir);

    size_t pos = (size_t)f.offset;
    for (unsigned int i = 0; i < f.count; ++i) {
        if (pos + 2 > self.size()) return false;
        unsigned short nameLen = 0;
        memcpy(&nameLen, self.data() + pos, 2);
        pos += 2;
        if (pos + nameLen + 4 > self.size()) return false;
        std::wstring rel = utf8toW((const char*)self.data() + pos, nameLen);
        pos += nameLen;
        unsigned int size = 0;
        memcpy(&size, self.data() + pos, 4);
        pos += 4;
        if (pos + size > self.size()) return false;

        for (auto& ch : rel) if (ch == L'/') ch = L'\\';
        std::wstring full = dir + L"\\" + rel;
        makeDirs(dirName(full));
        if (!writeFileW(full, self.data() + pos, size)) return false;
        pos += size;

        wchar_t status[256];
        lstrcpynW(status, rel.c_str(), 200);
        setProgress(4 + (int)((i + 1) * 84 / (f.count ? f.count : 1)), status);
    }

    // shortcuts -----------------------------------------------------------
    setProgress(92, L"Creating shortcuts...");
    std::wstring gameExe = dir + L"\\DONT_LOOK_BACK.exe";
    std::wstring icon = dir + L"\\assets\\ui\\icon.ico";
    if (GetFileAttributesW(icon.c_str()) == INVALID_FILE_ATTRIBUTES) icon.clear();

    std::wstring desktop = knownFolder(FOLDERID_Desktop);
    if (!desktop.empty())
        makeShortcut(desktop + L"\\DON'T LOOK BACK.lnk", gameExe, dir, icon, L"DON'T LOOK BACK");

    std::wstring start = knownFolder(FOLDERID_Programs);
    if (!start.empty())
        makeShortcut(start + L"\\DON'T LOOK BACK.lnk", gameExe, dir, icon, L"DON'T LOOK BACK");

    setProgress(100, L"Done.");
    return true;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"DLBSetupWnd";
    RegisterClassExW(&wc);

    int w = 560, h = 250;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST, L"DLBSetupWnd", L"DON'T LOOK BACK - Setup",
                             WS_POPUP | WS_CAPTION | WS_SYSMENU, sx, sy, w, h,
                             nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    setProgress(2, L"Reading payload...");

    bool ok = install();

    if (ok) {
        wchar_t msg[512];
        wsprintfW(msg, L"DON'T LOOK BACK is installed to\n%s\n\nLaunch the game now?", g_installDir);
        int r = MessageBoxW(g_hwnd, msg, L"DON'T LOOK BACK", MB_YESNO | MB_ICONINFORMATION);
        if (r == IDYES) {
            std::wstring gameExe = std::wstring(g_installDir) + L"\\DONT_LOOK_BACK.exe";
            ShellExecuteW(nullptr, L"open", gameExe.c_str(), nullptr, g_installDir, SW_SHOWNORMAL);
        }
    } else {
        MessageBoxW(g_hwnd, L"Installation failed. The files could not be written.",
                    L"DON'T LOOK BACK", MB_ICONERROR);
    }
    DestroyWindow(g_hwnd);
    CoUninitialize();
    return ok ? 0 : 1;
}
