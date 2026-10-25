// ============================================================================
//  installer.cpp : DON'T LOOK BACK setup (self-contained Win32 installer)
//  Reads a payload appended to this executable and installs the game into
//  %LOCALAPPDATA%\Programs\DontLookBack, then creates shortcuts and launches it.
// ============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <cstdio>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static const char kMagic[8] = {'D', 'L', 'B', 'P', 'A', 'K', '0', '1'};

struct Payload {
    std::vector<unsigned char> data;
    size_t offset = 0;
    unsigned int count = 0;
};

static bool readSelf(const wchar_t* path, std::vector<unsigned char>& out) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    GetFileSizeEx(f, &sz);
    out.resize((size_t)sz.QuadPart);
    DWORD got = 0;
    ReadFile(f, out.data(), (DWORD)out.size(), &got, nullptr);
    CloseHandle(f);
    return got == out.size();
}

static bool loadPayload(Payload& p) {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (!readSelf(self, p.data)) return false;
    if (p.data.size() < 24) return false;
    // footer: u64 offset, u32 count, 8 byte magic, u32 crc-ish
    size_t base = p.data.size() - 24;
    for (int i = 0; i < 8; ++i) if (p.data[base + 12 + i] != (unsigned char)kMagic[i]) return false;
    unsigned long long off = 0;
    memcpy(&off, &p.data[base], 8);
    memcpy(&p.count, &p.data[base + 8], 4);
    p.offset = (size_t)off;
    return p.offset > 0 && p.offset < p.data.size();
}

static std::wstring appDataDir() {
    wchar_t* p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) {
        out = p;
        CoTaskMemFree(p);
    }
    return out;
}

static bool ensureDirs(const std::wstring& path) {
    // create every component
    std::wstring cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == L'\\' || i + 1 == path.size()) {
            if (cur.size() > 3) CreateDirectoryW(cur.c_str(), nullptr);
        }
    }
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool writeAll(const std::wstring& path, const void* data, size_t n) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    WriteFile(f, data, (DWORD)n, &wrote, nullptr);
    CloseHandle(f);
    return wrote == n;
}

// ---------------------------------------------------------------- payload walk
struct Entry { std::wstring path; unsigned int size; size_t dataOff; };

static bool unpackFile(const Payload& p, size_t& cursor, Entry& e) {
    if (cursor + 6 > p.data.size()) return false;
    unsigned int plen = 0;
    memcpy(&plen, &p.data[cursor], 4); cursor += 4;
    if (cursor + plen + 8 > p.data.size()) return false;
    std::string rel((const char*)&p.data[cursor], plen); cursor += plen;
    unsigned int size = 0;
    memcpy(&size, &p.data[cursor], 4); cursor += 4;
    if (cursor + size > p.data.size()) return false;
    std::wstring w(rel.begin(), rel.end());
    for (auto& c : w) if (c == L'/') c = L'\\';
    e.path = w;
    e.size = size;
    e.dataOff = cursor;
    cursor += size;
    return true;
}

// ---------------------------------------------------------------- shortcuts
static bool makeShortcut(const std::wstring& lnkPath, const std::wstring& target,
                         const std::wstring& workDir, const std::wstring& icon,
                         const std::wstring& desc) {
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void**)&link))) return false;
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(workDir.c_str());
    link->SetDescription(desc.c_str());
    if (!icon.empty()) link->SetIconLocation(icon.c_str(), 0);
    IPersistFile* pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&pf))) {
        ok = SUCCEEDED(pf->Save(lnkPath.c_str(), TRUE));
        pf->Release();
    }
    link->Release();
    return ok;
}

// ---------------------------------------------------------------- UI
static HWND g_hwnd;
static std::wstring g_status;
static float g_progress;
static bool g_done;
static bool g_failed;
static std::wstring g_target;
static std::wstring g_exe;

static void paint(HDC dc, int W, int H) {
    RECT r{0, 0, W, H};
    HBRUSH bg = CreateSolidBrush(RGB(8, 8, 10));
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    // title
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(226, 216, 196));
    HFONT f1 = CreateFontW(34, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, L"Georgia");
    HFONT f2 = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ of = SelectObject(dc, f1);
    TextOutW(dc, 34, 44, L"DON'T LOOK BACK", 15);
    SelectObject(dc, f2);
    SetTextColor(dc, RGB(120, 116, 108));
    TextOutW(dc, 36, 92, L"an observation of the fourth floor  -  setup", 45);
    // rule
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 56, 50));
    HGDIOBJ op = SelectObject(dc, pen);
    MoveToEx(dc, 34, 126, nullptr); LineTo(dc, W - 34, 126);
    // progress track
    RECT tr{34, 170, W - 34, 176};
    HBRUSH tb = CreateSolidBrush(RGB(28, 27, 26));
    FillRect(dc, &tr, tb); DeleteObject(tb);
    RECT pr = tr;
    pr.right = tr.left + (LONG)((tr.right - tr.left) * (g_progress < 0 ? 0 : g_progress));
    HBRUSH pb = CreateSolidBrush(RGB(196, 160, 108));
    FillRect(dc, &pr, pb); DeleteObject(pb);
    // status
    SetTextColor(dc, RGB(170, 166, 158));
    TextOutW(dc, 36, 194, g_status.c_str(), (int)g_status.size());
    SelectObject(dc, op); DeleteObject(pen);
    SelectObject(dc, of);
    DeleteObject(f1); DeleteObject(f2);
}

static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT r; GetClientRect(h, &r);
            paint(dc, r.right, r.bottom);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_CLOSE:
            if (g_done) DestroyWindow(h);
            return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: break;
    }
    return DefWindowProcW(h, m, w, l);
}

static void pump(int ms) {
    DWORD end = GetTickCount() + ms;
    do {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        InvalidateRect(g_hwnd, nullptr, FALSE);
        UpdateWindow(g_hwnd);
        Sleep(16);
    } while (GetTickCount() < end);
}

static void fail(const std::wstring& why) {
    g_failed = true;
    g_status = why;
    pump(50);
    MessageBoxW(nullptr, why.c_str(), L"DON'T LOOK BACK - setup", MB_ICONERROR | MB_OK);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DLBSetup";
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);
    int W = 620, H = 250;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    g_hwnd = CreateWindowExW(0, L"DLBSetup", L"DON'T LOOK BACK - Setup",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                             sx, sy, W, H, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;
    g_status = L"Preparing...";
    g_progress = 0.02f;
    pump(120);

    Payload p;
    if (!loadPayload(p)) { fail(L"The installation payload is missing or damaged."); return 2; }

    std::wstring root = appDataDir();
    if (root.empty()) { fail(L"Could not resolve your user data folder."); return 2; }
    std::wstring dir = root + L"\\Programs\\DontLookBack";
    g_target = dir;
    if (!ensureDirs(dir)) { fail(L"Could not create " + dir); return 2; }

    g_status = L"Installing to " + dir;
    size_t cursor = p.offset;
    unsigned int done = 0;
    g_exe = dir + L"\\DONT_LOOK_BACK.exe";
    for (unsigned int i = 0; i < p.count; ++i) {
        Entry e;
        if (!unpackFile(p, cursor, e)) { fail(L"The installation payload is truncated."); return 3; }
        std::wstring full = dir + L"\\" + e.path;
        size_t slash = full.find_last_of(L'\\');
        if (slash != std::wstring::npos) ensureDirs(full.substr(0, slash));
        if (!writeAll(full, &p.data[e.dataOff], e.size)) {
            fail(L"Could not write " + full);
            return 3;
        }
        ++done;
        g_progress = 0.05f + 0.85f * ((float)done / (float)p.count);
        if ((done % 4) == 0) {
            g_status = L"Installing: " + e.path;
            pump(4);
        }
    }

    // shortcuts
    g_status = L"Creating shortcuts...";
    g_progress = 0.93f;
    pump(60);
    std::wstring icon = dir + L"\\assets\\ui\\icon.ico";
    wchar_t* desk = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desk))) {
        makeShortcut(std::wstring(desk) + L"\\DON'T LOOK BACK.lnk", g_exe, dir, icon,
                     L"DON'T LOOK BACK");
        CoTaskMemFree(desk);
    }
    wchar_t* prog = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &prog))) {
        makeShortcut(std::wstring(prog) + L"\\DON'T LOOK BACK.lnk", g_exe, dir, icon,
                     L"DON'T LOOK BACK");
        CoTaskMemFree(prog);
    }

    g_status = L"Done. Starting the game...";
    g_progress = 1.0f;
    pump(450);

    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"\"" + g_exe + L"\"";
    if (CreateProcessW(g_exe.c_str(), (LPWSTR)cmd.c_str(), nullptr, nullptr, FALSE, 0, nullptr,
                       dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        MessageBoxW(nullptr,
                    (L"Installed successfully, but the game could not be started.\n\n"
                     L"Run it from:\n" + g_exe + L"\n\nor use the desktop shortcut.").c_str(),
                    L"DON'T LOOK BACK - setup", MB_ICONINFORMATION | MB_OK);
    }
    g_done = true;
    DestroyWindow(g_hwnd);
    CoUninitialize();
    return 0;
}
