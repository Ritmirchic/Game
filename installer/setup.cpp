// Native 64-bit Windows Setup Wizard for DON'T LOOK BACK
// Bundles DONT_LOOK_BACK.exe, UNINSTALL.exe, and README.txt via XZ+E8 EOF archive.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <shellapi.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

extern "C" intptr_t dlb_xz_decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_cap);

#pragma pack(push, 1)
struct ArchiveFooter {
    uint64_t payload_offset;
    uint64_t game_comp_size;
    uint64_t game_raw_size;
    uint64_t uninst_comp_size;
    uint64_t uninst_raw_size;
    uint64_t readme_raw_size;
    char magic[8]; // "DLBPKG01"
};
#pragma pack(pop)

enum {
    IDC_PATH_EDIT   = 101,
    IDC_BROWSE_BTN  = 102,
    IDC_CHK_STARTM  = 103,
    IDC_CHK_DESKTOP = 104,
    IDC_CHK_LAUNCH  = 105,
    IDC_PROGRESS    = 106,
    IDC_STATUS_LBL  = 107,
    IDC_INSTALL_BTN = 108,
    IDC_CANCEL_BTN  = 109,
    WM_INSTALL_PROG = WM_APP + 10,
    WM_INSTALL_DONE = WM_APP + 11,
};

static HWND g_hWnd = NULL;
static HWND g_hEditPath = NULL;
static HWND g_hChkStartMenu = NULL;
static HWND g_hChkDesktop = NULL;
static HWND g_hChkLaunch = NULL;
static HWND g_hProgress = NULL;
static HWND g_hStatus = NULL;
static HWND g_hBtnInstall = NULL;
static HWND g_hBtnBrowse = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontSub = NULL;
static HFONT g_hFontUI = NULL;
static bool g_installing = false;
static bool g_finished = false;
static wchar_t g_finalExePath[MAX_PATH] = {0};

struct InstallParams {
    std::wstring installDir;
    bool makeStartMenu;
    bool makeDesktop;
    bool launchAfter;
    bool silent;
};

static void ensure_dir_recursive(const std::wstring& dir) {
    if (dir.length() <= 3) return;
    SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
}

static bool create_shortcut(
    const wchar_t* lnkPath,
    const wchar_t* targetPath,
    const wchar_t* args,
    const wchar_t* workDir,
    const wchar_t* desc,
    const wchar_t* iconPath
) {
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr) || !psl) return false;

    psl->SetPath(targetPath);
    if (args && args[0]) psl->SetArguments(args);
    if (workDir && workDir[0]) psl->SetWorkingDirectory(workDir);
    if (desc && desc[0]) psl->SetDescription(desc);
    if (iconPath && iconPath[0]) psl->SetIconLocation(iconPath, 0);

    IPersistFile* ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    if (SUCCEEDED(hr) && ppf) {
        hr = ppf->Save(lnkPath, TRUE);
        ppf->Release();
    }
    psl->Release();
    return SUCCEEDED(hr);
}

static bool write_all_file(const std::wstring& path, const uint8_t* data, size_t len) {
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return false;
    size_t written_total = 0;
    while (written_total < len) {
        DWORD chunk = (len - written_total > 4 * 1024 * 1024) ? (4 * 1024 * 1024) : (DWORD)(len - written_total);
        DWORD wr = 0;
        if (!WriteFile(hf, data + written_total, chunk, &wr, NULL) || wr == 0) {
            CloseHandle(hf);
            return false;
        }
        written_total += wr;
    }
    CloseHandle(hf);
    return true;
}

static void register_uninstaller_registry(const std::wstring& installDir, uint64_t rawSizeBytes) {
    HKEY hKey = NULL;
    if (RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\DontLookBack",
        0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL
    ) == ERROR_SUCCESS) {
        std::wstring dispName = L"DON'T LOOK BACK";
        std::wstring dispVer  = L"1.0.0";
        std::wstring pub      = L"Kestrel Interactive";
        std::wstring exePath  = installDir + L"\\DONT_LOOK_BACK.exe";
        std::wstring uninst   = L"\"" + installDir + L"\\UNINSTALL.exe\"";
        DWORD noMod = 1;
        DWORD estKB = (DWORD)(rawSizeBytes / 1024ULL);

        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (const BYTE*)dispName.c_str(), (DWORD)((dispName.size()+1)*2));
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)dispVer.c_str(), (DWORD)((dispVer.size()+1)*2));
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (const BYTE*)pub.c_str(), (DWORD)((pub.size()+1)*2));
        RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)installDir.c_str(), (DWORD)((installDir.size()+1)*2));
        RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)exePath.c_str(), (DWORD)((exePath.size()+1)*2));
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (const BYTE*)uninst.c_str(), (DWORD)((uninst.size()+1)*2));
        RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&noMod, sizeof(DWORD));
        RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&noMod, sizeof(DWORD));
        RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (const BYTE*)&estKB, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static bool perform_installation(InstallParams* params, std::wstring& errMsg) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    wchar_t selfPath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    HANDLE hSelf = CreateFileW(selfPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSelf == INVALID_HANDLE_VALUE) {
        errMsg = L"Unable to read setup archive.";
        CoUninitialize();
        return false;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hSelf, &fileSize);
    if ((uint64_t)fileSize.QuadPart < sizeof(ArchiveFooter) + 1024) {
        CloseHandle(hSelf);
        errMsg = L"Setup archive footer missing or truncated.";
        CoUninitialize();
        return false;
    }

    LARGE_INTEGER seekPos;
    seekPos.QuadPart = fileSize.QuadPart - (LONGLONG)sizeof(ArchiveFooter);
    SetFilePointerEx(hSelf, seekPos, NULL, FILE_BEGIN);
    ArchiveFooter footer{};
    DWORD rd = 0;
    ReadFile(hSelf, &footer, sizeof(footer), &rd, NULL);
    if (rd != sizeof(footer) || memcmp(footer.magic, "DLBPKG01", 8) != 0) {
        CloseHandle(hSelf);
        errMsg = L"Invalid DLBPKG01 archive signature.";
        CoUninitialize();
        return false;
    }

    if (g_hWnd) PostMessageW(g_hWnd, WM_INSTALL_PROG, 10, (LPARAM)L"Preparing target installation directory...");
    ensure_dir_recursive(params->installDir);

    // 1. Read & Extract DONT_LOOK_BACK.exe (XZ + E8 bijective inverse)
    seekPos.QuadPart = (LONGLONG)footer.payload_offset;
    SetFilePointerEx(hSelf, seekPos, NULL, FILE_BEGIN);

    std::vector<uint8_t> compGame((size_t)footer.game_comp_size);
    size_t got = 0;
    while (got < compGame.size()) {
        DWORD chunk = (compGame.size() - got > 4 * 1024 * 1024) ? (4 * 1024 * 1024) : (DWORD)(compGame.size() - got);
        DWORD n = 0;
        if (!ReadFile(hSelf, compGame.data() + got, chunk, &n, NULL) || n == 0) break;
        got += n;
    }
    if (got != compGame.size()) {
        CloseHandle(hSelf);
        errMsg = L"Unexpected EOF while reading compressed game payload.";
        CoUninitialize();
        return false;
    }

    if (g_hWnd) PostMessageW(g_hWnd, WM_INSTALL_PROG, 30, (LPARAM)L"Decompressing DONT_LOOK_BACK.exe (64-bit Engine & Assets)...");
    std::vector<uint8_t> rawGame((size_t)footer.game_raw_size);
    intptr_t decLen = dlb_xz_decompress(compGame.data(), compGame.size(), rawGame.data(), rawGame.size());
    compGame.clear();
    compGame.shrink_to_fit();

    if (decLen != (intptr_t)footer.game_raw_size) {
        CloseHandle(hSelf);
        errMsg = L"Integrity check failed while decompressing DONT_LOOK_BACK.exe.";
        CoUninitialize();
        return false;
    }

    // Apply bijective x86 CALL (0xE8) inverse transform on first 55 MB
    size_t limit = rawGame.size() > 5 ? (rawGame.size() - 5) : 0;
    if (limit > 55ULL * 1024 * 1024) limit = 55ULL * 1024 * 1024;
    for (size_t i = 0; i < limit; ) {
        if (rawGame[i] == 0xE8) {
            uint32_t v;
            memcpy(&v, &rawGame[i + 1], 4);
            v -= (uint32_t)i;
            memcpy(&rawGame[i + 1], &v, 4);
            i += 5;
        } else {
            i += 1;
        }
    }

    if (g_hWnd) PostMessageW(g_hWnd, WM_INSTALL_PROG, 68, (LPARAM)L"Writing DONT_LOOK_BACK.exe to disk...");
    std::wstring gameExePath = params->installDir + L"\\DONT_LOOK_BACK.exe";
    if (!write_all_file(gameExePath, rawGame.data(), rawGame.size())) {
        CloseHandle(hSelf);
        errMsg = L"Failed to write DONT_LOOK_BACK.exe. Check folder permissions or disk space.";
        CoUninitialize();
        return false;
    }
    wcscpy(g_finalExePath, gameExePath.c_str());
    rawGame.clear();
    rawGame.shrink_to_fit();

    // 2. Read & Extract UNINSTALL.exe
    if (g_hWnd) PostMessageW(g_hWnd, WM_INSTALL_PROG, 82, (LPARAM)L"Installing Uninstaller & Documentation...");
    std::vector<uint8_t> compUninst((size_t)footer.uninst_comp_size);
    DWORD urd = 0;
    ReadFile(hSelf, compUninst.data(), (DWORD)compUninst.size(), &urd, NULL);
    std::vector<uint8_t> rawUninst((size_t)footer.uninst_raw_size);
    intptr_t ulen = dlb_xz_decompress(compUninst.data(), compUninst.size(), rawUninst.data(), rawUninst.size());
    if (ulen == (intptr_t)footer.uninst_raw_size) {
        write_all_file(params->installDir + L"\\UNINSTALL.exe", rawUninst.data(), rawUninst.size());
    }

    // 3. Read & Write README.txt
    if (footer.readme_raw_size > 0) {
        std::vector<uint8_t> readme((size_t)footer.readme_raw_size);
        DWORD rrd = 0;
        ReadFile(hSelf, readme.data(), (DWORD)readme.size(), &rrd, NULL);
        write_all_file(params->installDir + L"\\README.txt", readme.data(), readme.size());
    }
    CloseHandle(hSelf);

    // 4. Create Start Menu & Optional Desktop Shortcuts
    if (g_hWnd) PostMessageW(g_hWnd, WM_INSTALL_PROG, 92, (LPARAM)L"Creating Windows Shortcuts & Registry Entries...");
    if (params->makeStartMenu) {
        wchar_t programsDir[MAX_PATH] = {0};
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programsDir))) {
            std::wstring smDir = std::wstring(programsDir) + L"\\DON'T LOOK BACK";
            ensure_dir_recursive(smDir);
            create_shortcut(
                (smDir + L"\\DON'T LOOK BACK.lnk").c_str(),
                gameExePath.c_str(), L"", params->installDir.c_str(),
                L"Play DON'T LOOK BACK", gameExePath.c_str()
            );
            create_shortcut(
                (smDir + L"\\DON'T LOOK BACK (Compatibility OpenGL).lnk").c_str(),
                gameExePath.c_str(), L"--rendering-driver opengl3", params->installDir.c_str(),
                L"Play DON'T LOOK BACK in OpenGL Compatibility Mode", gameExePath.c_str()
            );
            std::wstring uninstPath = params->installDir + L"\\UNINSTALL.exe";
            create_shortcut(
                (smDir + L"\\Uninstall DON'T LOOK BACK.lnk").c_str(),
                uninstPath.c_str(), L"", params->installDir.c_str(),
                L"Uninstall DON'T LOOK BACK", uninstPath.c_str()
            );
        }
    }

    if (params->makeDesktop) {
        wchar_t desktopDir[MAX_PATH] = {0};
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopDir))) {
            std::wstring dlnk = std::wstring(desktopDir) + L"\\DON'T LOOK BACK.lnk";
            create_shortcut(
                dlnk.c_str(), gameExePath.c_str(), L"", params->installDir.c_str(),
                L"Play DON'T LOOK BACK", gameExePath.c_str()
            );
        }
    }

    register_uninstaller_registry(params->installDir, footer.game_raw_size + footer.uninst_raw_size);

    if (g_hWnd) PostMessageW(g_hWnd, WM_INSTALL_PROG, 100, (LPARAM)L"Installation complete! Ready to play.");
    CoUninitialize();
    return true;
}

static DWORD WINAPI InstallThreadProc(LPVOID lpParam) {
    InstallParams* params = (InstallParams*)lpParam;
    std::wstring err;
    bool ok = perform_installation(params, err);
    if (g_hWnd) {
        wchar_t* heapMsg = _wcsdup(ok ? L"OK" : err.c_str());
        PostMessageW(g_hWnd, WM_INSTALL_DONE, ok ? 1 : 0, (LPARAM)heapMsg);
    }
    delete params;
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            // Header Banner (Dark Brutalist Slate + Amber Accent Line)
            RECT rcHeader = {0, 0, rcClient.right, 86};
            HBRUSH hBrHeader = CreateSolidBrush(RGB(14, 16, 22));
            FillRect(hdc, &rcHeader, hBrHeader);
            DeleteObject(hBrHeader);

            RECT rcLine = {0, 86, rcClient.right, 89};
            HBRUSH hBrAmber = CreateSolidBrush(RGB(218, 165, 72));
            FillRect(hdc, &rcLine, hBrAmber);
            DeleteObject(hBrAmber);

            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hFontTitle);
            SetTextColor(hdc, RGB(242, 236, 224));
            RECT rcT = {22, 16, rcClient.right - 20, 52};
            DrawTextW(hdc, L"DON'T LOOK BACK", -1, &rcT, DT_LEFT | DT_SINGLELINE);

            SelectObject(hdc, g_hFontSub);
            SetTextColor(hdc, RGB(218, 165, 72));
            RECT rcS = {24, 52, rcClient.right - 20, 78};
            DrawTextW(hdc, L"STANDALONE WINDOWS 64-BIT SETUP WIZARD  //  VERSION 1.0.0", -1, &rcS, DT_LEFT | DT_SINGLELINE);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BROWSE_BTN && !g_installing) {
                BROWSEINFOW bi = {0};
                bi.hwndOwner = hWnd;
                bi.lpszTitle = L"Select Destination Folder for DON'T LOOK BACK:";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    wchar_t picked[MAX_PATH] = {0};
                    if (SHGetPathFromIDListW(pidl, picked)) {
                        std::wstring p = picked;
                        if (p.find(L"DontLookBack") == std::wstring::npos) {
                            if (!p.empty() && p.back() != L'\\') p += L"\\";
                            p += L"DontLookBack";
                        }
                        SetWindowTextW(g_hEditPath, p.c_str());
                    }
                    CoTaskMemFree(pidl);
                }
            } else if (id == IDC_INSTALL_BTN) {
                if (g_finished) {
                    if (SendMessageW(g_hChkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED && g_finalExePath[0]) {
                        wchar_t dir[MAX_PATH];
                        wcscpy(dir, g_finalExePath);
                        wchar_t* s = wcsrchr(dir, L'\\');
                        if (s) *s = 0;
                        ShellExecuteW(NULL, L"open", g_finalExePath, NULL, dir, SW_SHOWNORMAL);
                    }
                    DestroyWindow(hWnd);
                    return 0;
                }
                if (g_installing) return 0;

                wchar_t buf[MAX_PATH] = {0};
                GetWindowTextW(g_hEditPath, buf, MAX_PATH);
                if (wcslen(buf) < 3) {
                    MessageBoxW(hWnd, L"Please enter a valid installation folder.", L"Setup", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                g_installing = true;
                EnableWindow(g_hBtnInstall, FALSE);
                EnableWindow(g_hBtnBrowse, FALSE);
                EnableWindow(g_hEditPath, FALSE);

                InstallParams* p = new InstallParams();
                p->installDir = buf;
                p->makeStartMenu = (SendMessageW(g_hChkStartMenu, BM_GETCHECK, 0, 0) == BST_CHECKED);
                p->makeDesktop   = (SendMessageW(g_hChkDesktop, BM_GETCHECK, 0, 0) == BST_CHECKED);
                p->launchAfter   = (SendMessageW(g_hChkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED);
                p->silent = false;

                CreateThread(NULL, 0, InstallThreadProc, p, 0, NULL);
            } else if (id == IDC_CANCEL_BTN && !g_installing) {
                DestroyWindow(hWnd);
            }
            return 0;
        }

        case WM_INSTALL_PROG: {
            SendMessageW(g_hProgress, PBM_SETPOS, wParam, 0);
            SetWindowTextW(g_hStatus, (const wchar_t*)lParam);
            return 0;
        }

        case WM_INSTALL_DONE: {
            wchar_t* msgText = (wchar_t*)lParam;
            g_installing = false;
            if (wParam == 1) {
                g_finished = true;
                SendMessageW(g_hProgress, PBM_SETPOS, 100, 0);
                SetWindowTextW(g_hStatus, L"Setup complete! Click FINISH to launch DON'T LOOK BACK.");
                SetWindowTextW(g_hBtnInstall, L"FINISH");
                EnableWindow(g_hBtnInstall, TRUE);
            } else {
                MessageBoxW(hWnd, msgText, L"DON'T LOOK BACK — Setup Error", MB_OK | MB_ICONERROR);
                EnableWindow(g_hBtnInstall, TRUE);
                EnableWindow(g_hBtnBrowse, TRUE);
                EnableWindow(g_hEditPath, TRUE);
            }
            free(msgText);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t* cmdLine = GetCommandLineW();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);

    wchar_t localApp[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp))) {
        wcscpy(localApp, L"C:\\Games");
    }
    std::wstring defaultDir = std::wstring(localApp) + L"\\Programs\\DontLookBack";

    // Support silent CLI install (/S or /SILENT)
    if (cmdLine && (wcsstr(cmdLine, L" /S") || wcsstr(cmdLine, L" /SILENT"))) {
        InstallParams p{defaultDir, true, true, false, true};
        std::wstring err;
        bool ok = perform_installation(&p, err);
        CoUninitialize();
        return ok ? 0 : 1;
    }

    g_hFontTitle = CreateFontW(-26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_hFontSub = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_hFontUI = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"DontLookBackSetupWnd";
    RegisterClassExW(&wc);

    int w = 580, h = 415;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hWnd = CreateWindowExW(
        0, wc.lpszClassName, L"DON'T LOOK BACK — Setup Wizard",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        sx, sy, w, h, NULL, NULL, hInst, NULL
    );

    HWND hLblDir = CreateWindowExW(0, L"STATIC", L"Destination Installation Folder:",
        WS_CHILD | WS_VISIBLE, 22, 106, 400, 22, g_hWnd, NULL, hInst, NULL);
    SendMessageW(hLblDir, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

    g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaultDir.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 22, 130, 425, 28, g_hWnd, (HMENU)IDC_PATH_EDIT, hInst, NULL);
    SendMessageW(g_hEditPath, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

    g_hBtnBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 456, 129, 92, 30, g_hWnd, (HMENU)IDC_BROWSE_BTN, hInst, NULL);
    SendMessageW(g_hBtnBrowse, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

    g_hChkStartMenu = CreateWindowExW(0, L"BUTTON", L"Create Start Menu shortcuts (includes OpenGL Compatibility Mode)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 22, 174, 520, 24, g_hWnd, (HMENU)IDC_CHK_STARTM, hInst, NULL);
    SendMessageW(g_hChkStartMenu, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    SendMessageW(g_hChkStartMenu, BM_SETCHECK, BST_CHECKED, 0);

    g_hChkDesktop = CreateWindowExW(0, L"BUTTON", L"Create Desktop shortcut",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 22, 202, 520, 24, g_hWnd, (HMENU)IDC_CHK_DESKTOP, hInst, NULL);
    SendMessageW(g_hChkDesktop, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    SendMessageW(g_hChkDesktop, BM_SETCHECK, BST_CHECKED, 0);

    g_hChkLaunch = CreateWindowExW(0, L"BUTTON", L"Launch DON'T LOOK BACK after installation completes",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 22, 230, 520, 24, g_hWnd, (HMENU)IDC_CHK_LAUNCH, hInst, NULL);
    SendMessageW(g_hChkLaunch, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    SendMessageW(g_hChkLaunch, BM_SETCHECK, BST_CHECKED, 0);

    g_hStatus = CreateWindowExW(0, L"STATIC", L"Ready to install DON'T LOOK BACK (~78 MB required).",
        WS_CHILD | WS_VISIBLE, 22, 268, 526, 22, g_hWnd, (HMENU)IDC_STATUS_LBL, hInst, NULL);
    SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

    g_hProgress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 22, 294, 526, 22, g_hWnd, (HMENU)IDC_PROGRESS, hInst, NULL);
    SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    g_hBtnInstall = CreateWindowExW(0, L"BUTTON", L"INSTALL",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 326, 332, 108, 34, g_hWnd, (HMENU)IDC_INSTALL_BTN, hInst, NULL);
    SendMessageW(g_hBtnInstall, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

    HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 442, 332, 106, 34, g_hWnd, (HMENU)IDC_CANCEL_BTN, hInst, NULL);
    SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return 0;
}
