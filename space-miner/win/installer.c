/* =========================================================================
   SPACE MINER — Windows setup (SPACE_MINER_SETUP.exe)
   -------------------------------------------------------------------------
   A self-contained per-user installer:
     * carries the game payload as a stored (uncompressed) ZIP appended to the
       executable, located through a 16-byte footer,
     * extracts it to %LOCALAPPDATA%\Programs\SpaceMiner by default (no UAC),
     * creates Start Menu + optional desktop shortcuts with the game icon,
     * registers an uninstall entry (handled by SPACE_MINER.exe /uninstall).

   Supports: /S silent install, /D=<dir> custom directory.
   ========================================================================= */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include ".gen_icon.h"

/* ---- host hooks required by win/zip.h ---- */
static void make_dirs(const char *path);
void zip_mkdir(const char *path) { make_dirs(path); }
int zip_write(const char *path, const unsigned char *data, unsigned int len) {
    HANDLE f;
    DWORD wrote = 0;
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    if (len && (!WriteFile(f, data, len, &wrote, NULL) || wrote != len)) {
        CloseHandle(f);
        return 0;
    }
    CloseHandle(f);
    return 1;
}
#include "zip.h"

static void make_dirs(const char *path)
{
    char tmp[MAX_PATH];
    size_t i, len;
    _snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    for (i = 3; i < len; i++) {
        if (tmp[i] == '\\' || tmp[i] == '/') {
            char c = tmp[i];
            tmp[i] = 0;
            CreateDirectoryA(tmp, NULL);
            tmp[i] = c;
        }
    }
    CreateDirectoryA(tmp, NULL);
}


#define APP_TITLE "SPACE MINER"
#define APP_VER   "1.0.0"
#define FOOT_MAGIC "SPMNRSET"
#define WM_INSTALL_PROGRESS (WM_APP + 1)
#define WM_INSTALL_DONE     (WM_APP + 2)

static HWND g_hwnd, g_editDir, g_btnInstall, g_btnCancel, g_btnBrowse, g_status;
static HWND g_chkDesktop, g_chkLaunch;
static int  g_percent = 0;
static int  g_installed = 0;
static char g_dir[MAX_PATH];
static char g_exePath[MAX_PATH];
static HICON g_iconBig, g_iconSmall;
static HFONT g_font;

static HICON make_app_icon(int which)
{
    if (which < 0 || which >= kIconCount) return NULL;
    return CreateIconFromResourceEx((PBYTE)kIconData + kIconOffset[which], (DWORD)kIconSize[which],
                                    TRUE, 0x00030000, which == 0 ? 32 : 16, which == 0 ? 32 : 16,
                                    LR_DEFAULTCOLOR);
}

/* ------------------------------------------------------------------ */
/*  payload                                                           */
/* ------------------------------------------------------------------ */
static int load_payload(unsigned char **out, DWORD *outSize)
{
    HANDLE h = CreateFileA(g_exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    LARGE_INTEGER sz;
    char foot[16];
    DWORD off = 0, got = 0;
    unsigned char *buf;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 1024) { CloseHandle(h); return 0; }
    SetFilePointer(h, (LONG)(sz.QuadPart - 16), NULL, FILE_BEGIN);
    if (!ReadFile(h, foot, 16, &got, NULL) || got != 16) { CloseHandle(h); return 0; }
    if (memcmp(foot, FOOT_MAGIC, 8) != 0) { CloseHandle(h); return 0; }
    memcpy(&off, foot + 8, 4);
    memcpy(outSize, foot + 12, 4);
    if (off == 0 || *outSize == 0 || (LONGLONG)off + *outSize > sz.QuadPart) { CloseHandle(h); return 0; }
    buf = (unsigned char *)malloc(*outSize);
    if (!buf) { CloseHandle(h); return 0; }
    SetFilePointer(h, (LONG)off, NULL, FILE_BEGIN);
    if (!ReadFile(h, buf, *outSize, &got, NULL) || got != *outSize) { free(buf); CloseHandle(h); return 0; }
    CloseHandle(h);
    *out = buf;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  shortcuts + registry                                              */
/* ------------------------------------------------------------------ */
static int make_shortcut(const char *lnkPath, const char *target, const char *args,
                         const char *workDir, const char *iconPath, const char *desc)
{
    IShellLinkA *link = NULL;
    IPersistFile *file = NULL;
    wchar_t wide[MAX_PATH * 2];
    HRESULT hr;
    int ok = 0;

    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkA, (void **)&link);
    if (FAILED(hr) || !link) return 0;
    link->lpVtbl->SetPath(link, target);
    if (args) link->lpVtbl->SetArguments(link, args);
    if (workDir) link->lpVtbl->SetWorkingDirectory(link, workDir);
    if (desc) link->lpVtbl->SetDescription(link, desc);
    if (iconPath) link->lpVtbl->SetIconLocation(link, iconPath, 0);
    link->lpVtbl->SetShowCmd(link, SW_SHOWNORMAL);

    hr = link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void **)&file);
    if (SUCCEEDED(hr) && file) {
        MultiByteToWideChar(CP_ACP, 0, lnkPath, -1, wide, MAX_PATH * 2);
        if (SUCCEEDED(file->lpVtbl->Save(file, wide, TRUE))) ok = 1;
        file->lpVtbl->Release(file);
    }
    link->lpVtbl->Release(link);
    return ok;
}

static void register_uninstall(const char *dir, int fileCount, DWORD bytes)
{
    HKEY k;
    char exe[MAX_PATH], uninst[MAX_PATH], sizeStr[32], icon[MAX_PATH];
    DWORD sizeKb = bytes / 1024;

    _snprintf(exe, sizeof(exe), "%s\\SPACE_MINER.exe", dir);
    _snprintf(uninst, sizeof(uninst), "\"%s\" /uninstall", exe);
    _snprintf(icon, sizeof(icon), "%s,0", exe);
    _snprintf(sizeStr, sizeof(sizeStr), "%lu", (unsigned long)sizeKb);
    (void)fileCount;

    if (RegCreateKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SpaceMiner",
        0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    RegSetValueExA(k, "DisplayName", 0, REG_SZ, (const BYTE *)"SPACE MINER", 12);
    RegSetValueExA(k, "DisplayVersion", 0, REG_SZ, (const BYTE *)APP_VER, (DWORD)strlen(APP_VER) + 1);
    RegSetValueExA(k, "Publisher", 0, REG_SZ, (const BYTE *)"Arena.ai", 9);
    RegSetValueExA(k, "DisplayIcon", 0, REG_SZ, (const BYTE *)icon, (DWORD)strlen(icon) + 1);
    RegSetValueExA(k, "InstallLocation", 0, REG_SZ, (const BYTE *)dir, (DWORD)strlen(dir) + 1);
    RegSetValueExA(k, "UninstallString", 0, REG_SZ, (const BYTE *)uninst, (DWORD)strlen(uninst) + 1);
    RegSetValueExA(k, "EstimatedSize", 0, REG_DWORD, (const BYTE *)&sizeKb, 4);
    {
        DWORD one = 1;
        RegSetValueExA(k, "NoModify", 0, REG_DWORD, (const BYTE *)&one, 4);
        RegSetValueExA(k, "NoRepair", 0, REG_DWORD, (const BYTE *)&one, 4);
    }
    RegCloseKey(k);

    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SpaceMiner", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)
        == ERROR_SUCCESS) {
        RegSetValueExA(k, "InstallDir", 0, REG_SZ, (const BYTE *)dir, (DWORD)strlen(dir) + 1);
        RegSetValueExA(k, "Version", 0, REG_SZ, (const BYTE *)APP_VER, (DWORD)strlen(APP_VER) + 1);
        RegCloseKey(k);
    }
}

static void zip_progress(int percent, void *ctx)
{
    g_percent = percent * 96 / 100;
    if (ctx) PostMessageA((HWND)ctx, WM_INSTALL_PROGRESS, 0, 0);
}

/* ------------------------------------------------------------------ */
/*  install routine                                                   */
/* ------------------------------------------------------------------ */
static int do_install(HWND hwnd, const char *dest, int desktopShortcut, DWORD *outBytes)
{
    unsigned char *payload = NULL;
    DWORD size = 0;
    char game[MAX_PATH], exe[MAX_PATH], iconPath[MAX_PATH];
    char programs[MAX_PATH], lnk[MAX_PATH];
    int ok;

    PostMessageA(hwnd, WM_INSTALL_PROGRESS, 0, 0);
    make_dirs(dest);

    if (!load_payload(&payload, &size)) {
        MessageBoxA(hwnd, "The installer payload is damaged. Please download SPACE MINER again.",
                    APP_TITLE, MB_ICONERROR | MB_OK);
        return 0;
    }

    _snprintf(game, sizeof(game), "%s\\game", dest);
    make_dirs(game);
    g_percent = 0;
    ok = zip_extract(payload, size, dest, zip_progress, hwnd);
    free(payload);
    {
        char probe[MAX_PATH];
        _snprintf(probe, sizeof(probe), "%s\\game\\index.html", dest);
        if (!PathFileExistsA(probe)) ok = 0;
    }
    if (!ok) {
        char msg[MAX_PATH + 160];
        _snprintf(msg, sizeof(msg),
                  "Extraction failed. Check that you have write access to:\n\n%s", dest);
        msg[sizeof(msg) - 1] = 0;
        MessageBoxA(hwnd, msg, APP_TITLE, MB_ICONERROR | MB_OK);
        return 0;
    }
    *outBytes = size + 512 * 1024;

    _snprintf(exe, sizeof(exe), "%s\\SPACE_MINER.exe", dest);
    _snprintf(iconPath, sizeof(iconPath), "%s", exe);   /* icon lives in the exe */

    /* Start menu */
    if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, programs) == S_OK) {
        _snprintf(lnk, sizeof(lnk), "%s\\SPACE MINER.lnk", programs);
        make_shortcut(lnk, exe, "", dest, iconPath, "SPACE MINER — Deep Belt Operations");
    }
    if (desktopShortcut) {
        char desktop[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) == S_OK) {
            _snprintf(lnk, sizeof(lnk), "%s\\SPACE MINER.lnk", desktop);
            make_shortcut(lnk, exe, "", dest, iconPath, "SPACE MINER — Deep Belt Operations");
        }
    }
    register_uninstall(dest, 0, *outBytes);
    g_percent = 100;
    PostMessageA(hwnd, WM_INSTALL_PROGRESS, 0, 0);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  window                                                            */
/* ------------------------------------------------------------------ */
static HFONT font(int size, int weight, const char *face)
{
    return CreateFontA(-size, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

static void paint_window(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    HBRUSH b;
    int i;
    HFONT f, old;

    GetClientRect(hwnd, &rc);
    for (i = 0; i < rc.bottom; i++) {
        int t = i * 255 / (rc.bottom ? rc.bottom : 1);
        b = CreateSolidBrush(RGB(6 + t * 6 / 255, 13 + t * 12 / 255, 21 + t * 20 / 255));
        {
            RECT r;
            r.left = 0; r.top = i; r.right = rc.right; r.bottom = i + 1;
            FillRect(dc, &r, b);
        }
        DeleteObject(b);
    }
    /* header band */
    b = CreateSolidBrush(RGB(9, 24, 36));
    {
        RECT r;
        r.left = 0; r.top = 0; r.right = rc.right; r.bottom = 96;
        FillRect(dc, &r, b);
    }
    DeleteObject(b);
    b = CreateSolidBrush(RGB(56, 200, 186));
    {
        RECT r;
        r.left = 0; r.top = 95; r.right = rc.right; r.bottom = 97;
        FillRect(dc, &r, b);
    }
    DeleteObject(b);

    SetBkMode(dc, TRANSPARENT);
    f = font(38, FW_BOLD, "Segoe UI");
    old = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, RGB(240, 252, 255));
    {
        const char *title = APP_TITLE;
        int x = 28, y = 20;
        SIZE sz;
        for (i = 0; title[i]; i++) {
            char ch[2] = { title[i], 0 };
            TextOutA(dc, x, y, ch, 1);
            GetTextExtentPoint32A(dc, ch, 1, &sz);
            x += sz.cx + 2;
        }
    }
    SelectObject(dc, old);
    DeleteObject(f);

    f = font(12, FW_NORMAL, "Consolas");
    old = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, RGB(95, 226, 212));
    TextOutA(dc, 30, 62, "DEEP BELT OPERATIONS  \xB7  VERSION " APP_VER, 40);
    SelectObject(dc, old);
    DeleteObject(f);

    /* progress bar */
    {
        RECT r;
        r.left = 28; r.top = rc.bottom - 92; r.right = rc.right - 28; r.bottom = rc.bottom - 82;
        b = CreateSolidBrush(RGB(16, 32, 44));
        FillRect(dc, &r, b);
        DeleteObject(b);
        if (g_percent > 0) {
            r.right = r.left + (rc.right - 56) * g_percent / 100;
            b = CreateSolidBrush(g_installed ? RGB(120, 224, 160) : RGB(70, 220, 200));
            FillRect(dc, &r, b);
            DeleteObject(b);
        }
    }

    f = font(11, FW_NORMAL, "Consolas");
    old = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, RGB(120, 155, 175));
    TextOutA(dc, 28, rc.bottom - 76, "PER-USER INSTALL \xB7 NO ADMINISTRATOR RIGHTS REQUIRED", 51);
    SelectObject(dc, old);
    DeleteObject(f);

    EndPaint(hwnd, &ps);
}

static void run_install_thread(void)
{
    char dir[MAX_PATH];
    DWORD bytes = 0;
    int desktop = (SendMessageA(g_chkDesktop, BM_GETCHECK, 0, 0) == BST_CHECKED);
    GetWindowTextA(g_editDir, dir, MAX_PATH);
    if (strlen(dir) < 4) {
        PostMessageA(g_hwnd, WM_INSTALL_DONE, 0, 0);
        return;
    }
    g_installed = do_install(g_hwnd, dir, desktop, &bytes);
    PostMessageA(g_hwnd, WM_INSTALL_DONE, 0, 0);
}

static DWORD WINAPI install_thread_proc(LPVOID p)
{
    (void)p;
    run_install_thread();
    return 0;
}

static void browse_folder(HWND hwnd)
{
    BROWSEINFOA bi;
    LPITEMIDLIST idl;
    char path[MAX_PATH];
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = hwnd;
    bi.lpszTitle = "Choose where to install SPACE MINER";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    idl = SHBrowseForFolderA(&bi);
    if (idl) {
        if (SHGetPathFromIDListA(idl, path)) {
            char joined[MAX_PATH];
            _snprintf(joined, sizeof(joined), "%s\\SpaceMiner", path);
            SetWindowTextA(g_editDir, joined);
        }
        CoTaskMemFree(idl);
    }
}

static void launch_game(void)
{
    char dir[MAX_PATH], exe[MAX_PATH];
    GetWindowTextA(g_editDir, dir, MAX_PATH);
    _snprintf(exe, sizeof(exe), "%s\\SPACE_MINER.exe", dir);
    ShellExecuteA(NULL, "open", exe, NULL, dir, SW_SHOWNORMAL);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_window(hwnd);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(200, 222, 236));
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, RGB(4, 12, 20));
        SetTextColor(dc, RGB(150, 232, 220));
        return (LRESULT)GetStockObject(BLACK_BRUSH);
    }
    case WM_INSTALL_PROGRESS:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_INSTALL_DONE:
        g_installed = g_installed ? 1 : 0;
        if (g_installed) {
            EnableWindow(g_editDir, FALSE);
            EnableWindow(g_btnBrowse, FALSE);
            EnableWindow(g_chkDesktop, FALSE);
            SetWindowTextA(g_status, "Installation complete — SPACE MINER is ready to fly.");
            SetWindowTextA(g_btnInstall, "LAUNCH GAME");
            SetWindowTextA(g_btnCancel, "CLOSE");
            if (SendMessageA(g_chkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                Sleep(150);
                launch_game();
                PostMessageA(hwnd, WM_CLOSE, 0, 0);
            }
        } else {
            SetWindowTextA(g_status, "Installation failed. See the message for details.");
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == 1001) {            /* install / launch */
            if (g_installed) { launch_game(); }
            else {
                if (!PathFileExistsA(g_exePath)) return 0;
                EnableWindow(g_btnInstall, FALSE);
                SetWindowTextA(g_status, "Extracting game files...");
                CreateThread(NULL, 0, install_thread_proc, NULL, 0, NULL);
            }
            return 0;
        }
        if (LOWORD(wp) == 1002) {            /* cancel / close */
            PostMessageA(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        if (LOWORD(wp) == 1003) { browse_folder(hwnd); return 0; }
        return 0;
    case WM_CLOSE:
        if (!g_installed) {
            if (MessageBoxA(hwnd, "Cancel the installation?", APP_TITLE,
                            MB_ICONQUESTION | MB_YESNO) != IDYES) return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void silent_install(const char *dir)
{
    DWORD bytes = 0;
    g_hwnd = NULL;
    do_install(NULL, dir, 1, &bytes);      /* /S: no UI, exit code only */
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int show)
{
    WNDCLASSEXA wc;
    MSG msg;
    char programs[MAX_PATH], local[MAX_PATH];
    int silent = 0;
    (void)prev; (void)show;

    GetModuleFileNameA(NULL, g_exePath, MAX_PATH);
    SetProcessDPIAware();
    InitCommonControls();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (cmdLine) {
        if (strstr(cmdLine, "/S") || strstr(cmdLine, "-silent")) silent = 1;
        {
            char *d = strstr(cmdLine, "/D=");
            if (d) {
                _snprintf(g_dir, sizeof(g_dir), "%s", d + 3);
                if (g_dir[0] == '"') {
                    char tmp[MAX_PATH];
                    _snprintf(tmp, sizeof(tmp), "%s", g_dir + 1);
                    {
                        char *q = strchr(tmp, '"');
                        if (q) *q = 0;
                    }
                    _snprintf(g_dir, sizeof(g_dir), "%s", tmp);
                }
            }
        }
    }

    if (!g_dir[0]) {
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local) == S_OK) {
            char progs[MAX_PATH];
            _snprintf(progs, sizeof(progs), "%s\\Programs", local);
            make_dirs(progs);
            _snprintf(g_dir, sizeof(g_dir), "%s\\SpaceMiner", progs);
        } else {
            _snprintf(g_dir, sizeof(g_dir), "C:\\SpaceMiner");
        }
    }
    (void)programs;

    if (silent) { silent_install(g_dir); CoUninitialize(); return 0; }

    g_iconBig = make_app_icon(0);
    g_iconSmall = make_app_icon(1);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hIcon = g_iconBig;
    wc.hIconSm = g_iconSmall;
    wc.lpszClassName = "SpaceMinerSetup";
    RegisterClassExA(&wc);

    g_hwnd = CreateWindowExA(0, "SpaceMinerSetup", "SPACE MINER — Setup",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 720, 500,
                             NULL, NULL, inst, NULL);
    if (!g_hwnd) { CoUninitialize(); return 1; }

    /* controls */
    CreateWindowA("STATIC", "INSTALLATION", WS_CHILD | WS_VISIBLE, 28, 112, 400, 18,
                  g_hwnd, NULL, inst, NULL);
    CreateWindowA("STATIC", "Destination folder", WS_CHILD | WS_VISIBLE, 28, 140, 400, 18,
                  g_hwnd, NULL, inst, NULL);
    g_editDir = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_dir,
                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 28, 160, 560, 26,
                                g_hwnd, (HMENU)1010, inst, NULL);
    g_btnBrowse = CreateWindowA("BUTTON", "Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                596, 160, 96, 26, g_hwnd, (HMENU)1003, inst, NULL);

    g_chkDesktop = CreateWindowA("BUTTON", "Create a desktop shortcut",
                                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 28, 200, 320, 22,
                                 g_hwnd, (HMENU)1004, inst, NULL);
    SendMessageA(g_chkDesktop, BM_SETCHECK, BST_CHECKED, 0);
    g_chkLaunch = CreateWindowA("BUTTON", "Launch SPACE MINER when finished",
                                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 28, 226, 320, 22,
                                g_hwnd, (HMENU)1005, inst, NULL);
    SendMessageA(g_chkLaunch, BM_SETCHECK, BST_CHECKED, 0);

    CreateWindowA("STATIC",
                  "Install for the current user only — no administrator rights required. "
                  "Your pilot profile is stored in the game itself.",
                  WS_CHILD | WS_VISIBLE, 28, 262, 660, 34, g_hwnd, NULL, inst, NULL);

    g_status = CreateWindowA("STATIC", "Ready to install.",
                             WS_CHILD | WS_VISIBLE, 28, 340, 660, 20, g_hwnd, NULL, inst, NULL);

    g_btnInstall = CreateWindowA("BUTTON", "INSTALL", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                 28, 372, 180, 40, g_hwnd, (HMENU)1001, inst, NULL);
    g_btnCancel = CreateWindowA("BUTTON", "CANCEL", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                220, 372, 140, 40, g_hwnd, (HMENU)1002, inst, NULL);

    {
        HFONT f = font(15, FW_NORMAL, "Segoe UI");
        HWND kids[] = { g_editDir, g_btnBrowse, g_chkDesktop, g_chkLaunch, g_btnInstall,
                        g_btnCancel, g_status };
        int i;
        for (i = 0; i < (int)(sizeof(kids) / sizeof(kids[0])); i++)
            SendMessageA(kids[i], WM_SETFONT, (WPARAM)f, TRUE);
        /* every static child gets the same face */
        {
            HWND c = GetWindow(g_hwnd, GW_CHILD);
            while (c) {
                char cls[32];
                GetClassNameA(c, cls, sizeof(cls));
                if (!_stricmp(cls, "STATIC")) SendMessageA(c, WM_SETFONT, (WPARAM)f, TRUE);
                c = GetWindow(c, GW_HWNDNEXT);
            }
        }
        g_font = f;
    }

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    if (g_iconBig) SendMessageA(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_iconBig);
    if (g_iconSmall) SendMessageA(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_iconSmall);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(g_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    CoUninitialize();
    return 0;
}
