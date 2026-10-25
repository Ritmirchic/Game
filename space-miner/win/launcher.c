/* =========================================================================
   SPACE MINER — native Windows launcher  (SPACE_MINER.exe)
   -------------------------------------------------------------------------
   Hosts the game from a tiny loopback HTTP server (so browser storage works
   reliably) and opens it in a chromeless app window via Edge/Chrome.
   Falls back to the default browser over file:// if no Chromium is present.

   Also implements /uninstall so the installer can register a clean removal.

   Built with:  zig cc -target x86_64-windows-gnu -Wl,--subsystem,windows
   ========================================================================= */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_util.h"
#include "serve.h"
#include ".gen_icon.h"

#define APP_TITLE   "SPACE MINER"
#define APP_SUB     "DEEP BELT OPERATIONS"
#define APP_VER     "1.0.0"
#define SRV_PORT_BASE      47821     /* stable origin for browser storage */
#define SRV_PORT_TRIES     10
#define SRV_IDLE_EXIT_MS   25000     /* exit once the page stops pinging */
#define SRV_HARD_LIMIT_MS  21600000  /* 6h safety stop */

static HWND  g_splash = NULL;
static volatile LONG g_lastRequest = 0;
static volatile LONG g_stopServer = 0;
static volatile LONG g_gotRequest = 0;
static int   g_port = 0;
static HANDLE g_portEvent = NULL;
static char  g_root[MAX_PATH];       /* ...\game                    */
static char  g_base[MAX_PATH];       /* ...\                        */
static char  g_profile[MAX_PATH];    /* %LOCALAPPDATA%\SpaceMiner   */
static char  g_url[768];
static char  g_status[256] = "INITIALISING FLIGHT SYSTEMS";

/* ------------------------------------------------------------------ */
/*  small helpers                                                     */
/* ------------------------------------------------------------------ */
static int file_exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int read_whole_file(const char *path, unsigned char **out, DWORD *outLen)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    DWORD got = 0;
    unsigned char *buf;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > 32 * 1024 * 1024) { CloseHandle(h); return 0; }
    buf = (unsigned char *)malloc((size_t)sz.QuadPart + 1);
    if (!buf) { CloseHandle(h); return 0; }
    if (!ReadFile(h, buf, (DWORD)sz.QuadPart, &got, NULL)) { free(buf); CloseHandle(h); return 0; }
    CloseHandle(h);
    buf[got] = 0;
    *out = buf;
    *outLen = got;
    return 1;
}

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

/* ------------------------------------------------------------------ */
/*  HTTP server                                                       */
/* ------------------------------------------------------------------ */
static int send_all(SOCKET s, const char *buf, int len)
{
    int sent = 0;
    while (sent < len) {
        int r = send(s, buf + sent, len - sent, 0);
        if (r <= 0) return 0;
        sent += r;
    }
    return 1;
}

/* ---- host hooks for serve.h ---- */
const char *srv_root = g_root;

int srv_file_exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int srv_read_file(const char *path, unsigned char **out, unsigned int *len)
{
    DWORD got = 0;
    return read_whole_file(path, out, &got) ? (*len = got, 1) : 0;
}

void srv_free_file(unsigned char *buf) { free(buf); }

void srv_note_request(void)
{
    InterlockedExchange(&g_lastRequest, (LONG)GetTickCount());
    InterlockedExchange(&g_gotRequest, 1);
}

static int socket_write(void *ctx, const char *data, int len)
{
    return send_all(*(SOCKET *)ctx, data, len);
}

static void serve_client(SOCKET c)
{
    char req[4096];
    int total = 0;
    char method[8] = { 0 }, target[1024] = { 0 };
    DWORD t0 = GetTickCount();
    srv_out out;
    int headOnly = 0;

    for (;;) {
        int r = recv(c, req + total, (int)sizeof(req) - 1 - total, 0);
        if (r <= 0) break;
        total += r;
        req[total] = 0;
        if (strstr(req, "\r\n\r\n") || strstr(req, "\n\n")) break;
        if (total > (int)sizeof(req) - 2) break;
        if (GetTickCount() - t0 > 4000) break;
    }
    if (total < 4) { closesocket(c); return; }

    if (sscanf(req, "%7s %1023s", method, target) != 2) {
        srv_out bad;
        bad.write = socket_write;
        bad.ctx = &c;
        srv_text(&bad, 400, "bad request", 0);
        closesocket(c);
        return;
    }

    out.write = socket_write;
    out.ctx = &c;
    headOnly = !strcmp(method, "HEAD");
    serve_request(method, target, headOnly, &out);
    closesocket(c);
}

static DWORD WINAPI server_thread(LPVOID param)
{
    WSADATA wsa;
    SOCKET srv;
    struct sockaddr_in addr;
    int alen;
    HANDLE ev = (HANDLE)param;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { SetEvent(ev); return 0; }
    srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) { SetEvent(ev); return 0; }

    {
        int yes = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    /* A fixed port keeps http://127.0.0.1:<port> stable, which is what makes
       browser localStorage (the save game) persist between sessions. We scan a
       short range so a port already taken by an unrelated process cannot block
       the game, and fall back to an ephemeral port as the last resort. */
    {
        int i, bound = 0;
        for (i = 0; i < SRV_PORT_TRIES; i++) {
            addr.sin_port = htons((unsigned short)(SRV_PORT_BASE + i));
            if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == 0) { bound = 1; break; }
        }
        if (!bound) {
            addr.sin_port = 0;
            if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
                closesocket(srv); SetEvent(ev); return 0;
            }
        }
    }
    alen = sizeof(addr);
    if (getsockname(srv, (struct sockaddr *)&addr, &alen) != 0) {
        closesocket(srv); SetEvent(ev); return 0;
    }
    g_port = (int)ntohs(addr.sin_port);
    if (listen(srv, 16) != 0) { closesocket(srv); g_port = 0; SetEvent(ev); return 0; }
    SetEvent(ev);

    while (!InterlockedCompareExchange(&g_stopServer, 0, 0)) {
        fd_set set;
        struct timeval tv;
        int sel;
        FD_ZERO(&set);
        FD_SET(srv, &set);
        tv.tv_sec = 0;
        tv.tv_usec = 250000;
        sel = select(0, &set, NULL, NULL, &tv);
        if (sel > 0 && FD_ISSET(srv, &set)) {
            SOCKET c = accept(srv, NULL, NULL);
            if (c != INVALID_SOCKET) {
                DWORD opt = 4000;
                setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char *)&opt, sizeof(opt));
                setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, (const char *)&opt, sizeof(opt));
                serve_client(c);
            }
        }
        if (g_lastRequest && GetTickCount() - (DWORD)g_lastRequest > SRV_HARD_LIMIT_MS) break;
    }
    closesocket(srv);
    WSACleanup();
    return 0;
}

/* ------------------------------------------------------------------ */
/*  browser discovery + launch                                        */
/* ------------------------------------------------------------------ */
static int reg_app_path(HKEY rootKey, const char *exe, char *out, size_t n)
{
    HKEY k;
    char key[512];
    DWORD type = 0, sz = (DWORD)n;
    LONG r;
    _snprintf(key, sizeof(key),
              "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%s", exe);
    r = RegOpenKeyExA(rootKey, key, 0, KEY_QUERY_VALUE, &k);
    if (r != ERROR_SUCCESS) return 0;
    r = RegQueryValueExA(k, NULL, NULL, &type, (LPBYTE)out, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return 0;
    out[n - 1] = 0;
    return file_exists(out);
}

static int env_path(const char *envVar, const char *suffix, char *out, size_t n)
{
    char base[MAX_PATH], joined[MAX_PATH];
    DWORD got = GetEnvironmentVariableA(envVar, base, sizeof(base));
    if (!got || got >= sizeof(base)) return 0;
    path_join(joined, sizeof(joined), base, suffix);
    if (!file_exists(joined)) return 0;
    _snprintf(out, n, "%s", joined);
    return 1;
}

static int find_browser(char *out, size_t n)
{
    static const char *suffixes[] = {
        "Microsoft\\Edge\\Application\\msedge.exe",
        "Google\\Chrome\\Application\\chrome.exe",
        "BraveSoftware\\Brave-Browser\\Application\\brave.exe",
        "Microsoft\\Edge Beta\\Application\\msedge.exe"
    };
    static const char *appPaths[] = { "msedge.exe", "chrome.exe", "brave.exe" };
    int i;

    for (i = 0; i < (int)(sizeof(appPaths) / sizeof(appPaths[0])); i++) {
        if (reg_app_path(HKEY_LOCAL_MACHINE, appPaths[i], out, n)) return 1;
        if (reg_app_path(HKEY_CURRENT_USER, appPaths[i], out, n)) return 1;
    }
    for (i = 0; i < (int)(sizeof(suffixes) / sizeof(suffixes[0])); i++) {
        if (env_path("ProgramFiles(x86)", suffixes[i], out, n)) return 1;
        if (env_path("ProgramFiles", suffixes[i], out, n)) return 1;
        if (env_path("LOCALAPPDATA", suffixes[i], out, n)) return 1;
    }
    return 0;
}

static HANDLE launch_browser(const char *browser, const char *url, int *usedFallback)
{
    char cmd[2048];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    *usedFallback = 0;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    memset(&pi, 0, sizeof(pi));

    if (browser && browser[0]) {
        _snprintf(cmd, sizeof(cmd),
            "\"%s\" --app=\"%s\" --window-size=1600,900 --window-position=80,40 "
            "--user-data-dir=\"%s\" --no-first-run --no-default-browser-check "
            "--allow-file-access-from-files --disable-features=Translate,TranslateUI "
            "--disable-background-timer-throttling --autoplay-policy=no-user-gesture-required",
            browser, url, g_profile);
        cmd[sizeof(cmd) - 1] = 0;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, g_base, &si, &pi)) {
            CloseHandle(pi.hThread);
            return pi.hProcess;
        }
    }

    /* fallback: whatever the system considers the default handler */
    if ((INT_PTR)ShellExecuteA(NULL, "open", url, NULL, g_base, SW_SHOWNORMAL) > 32) {
        *usedFallback = 1;
        return NULL;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  splash window                                                     */
/* ------------------------------------------------------------------ */
static HICON make_app_icon(int which)
{
    int size = (which == 0) ? 32 : 16;
    if (which < 0 || which >= kIconCount) return NULL;
    return CreateIconFromResourceEx((PBYTE)kIconData + kIconOffset[which], (DWORD)kIconSize[which],
                                    TRUE, 0x00030000, size, size, LR_DEFAULTCOLOR);
}

static void draw_splash(HDC dc, RECT *rc)
{
    int w = rc->right, h = rc->bottom, i;
    HBRUSH b;
    RECT r;
    HFONT f, old;
    int tick = (int)(GetTickCount() / 16) % 1000;

    for (i = 0; i < h; i++) {
        int c = i * 255 / (h > 0 ? h : 1);
        int rr = 4 + c * 8 / 255, gg = 10 + c * 16 / 255, bb = 18 + c * 26 / 255;
        b = CreateSolidBrush(RGB(rr, gg, bb));
        r.left = 0; r.top = i; r.right = w; r.bottom = i + 1;
        FillRect(dc, &r, b);
        DeleteObject(b);
    }

    /* accent rules */
    b = CreateSolidBrush(RGB(38, 120, 128));
    r.left = 28; r.top = 40; r.right = w - 28; r.bottom = 41;
    FillRect(dc, &r, b);
    r.top = h - 41; r.bottom = h - 40;
    FillRect(dc, &r, b);
    DeleteObject(b);

    SetBkMode(dc, TRANSPARENT);

    f = CreateFontA(-34, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    old = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, RGB(238, 250, 255));
    {
        /* manual letter spacing */
        const char *title = APP_TITLE;
        int x = 30, y = 96;
        SIZE sz;
        for (i = 0; title[i]; i++) {
            char ch[2] = { title[i], 0 };
            TextOutA(dc, x, y, ch, 1);
            GetTextExtentPoint32A(dc, ch, 1, &sz);
            x += sz.cx + 3;
        }
    }
    SelectObject(dc, old);
    DeleteObject(f);

    f = CreateFontA(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Consolas");
    old = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, RGB(95, 220, 206));
    TextOutA(dc, 32, 140, APP_SUB, (int)strlen(APP_SUB));
    SetTextColor(dc, RGB(120, 155, 175));
    TextOutA(dc, 32, 162, "VERSION " APP_VER "   \xB7   NATIVE WINDOWS BUILD", 38);
    SetTextColor(dc, RGB(190, 215, 230));
    TextOutA(dc, 32, h - 96, g_status, (int)strlen(g_status));
    SelectObject(dc, old);
    DeleteObject(f);

    /* animated progress bar */
    r.left = 32; r.top = h - 74; r.right = w - 32; r.bottom = h - 68;
    b = CreateSolidBrush(RGB(16, 32, 44));
    FillRect(dc, &r, b);
    DeleteObject(b);
    {
        int span = (r.right - r.left);
        int pos = (tick * 4) % (span + 80) - 80;
        int x0 = r.left + (pos < 0 ? 0 : pos);
        int x1 = r.left + (pos + 80 > span ? span : pos + 80);
        r.left = x0; r.right = x1;
        b = CreateSolidBrush(RGB(70, 220, 200));
        FillRect(dc, &r, b);
        DeleteObject(b);
    }
}

static LRESULT CALLBACK splash_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        draw_splash(dc, &rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1) InvalidateRect(hwnd, NULL, FALSE);
        if (wp == 2) {
            if (g_gotRequest || GetTickCount() > 9000) {
                KillTimer(hwnd, 2);
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        g_splash = NULL;
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void show_splash(void)
{
    WNDCLASSEXA wc;
    RECT work;
    int w = 520, h = 300, x, y;
    HICON ic = make_app_icon(0);
    HICON icSm = make_app_icon(1);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = splash_proc;
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "SpaceMinerSplash";
    wc.hIcon = ic;
    wc.hIconSm = icSm;
    RegisterClassExA(&wc);

    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    x = work.left + ((work.right - work.left) - w) / 2;
    y = work.top + ((work.bottom - work.top) - h) / 2;

    g_splash = CreateWindowExA(WS_EX_TOPMOST, "SpaceMinerSplash", "SPACE MINER",
                               WS_POPUP | WS_BORDER, x, y, w, h,
                               NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!g_splash) return;
    if (ic) SendMessageA(g_splash, WM_SETICON, ICON_BIG, (LPARAM)ic);
    if (icSm) SendMessageA(g_splash, WM_SETICON, ICON_SMALL, (LPARAM)icSm);
    SetTimer(g_splash, 1, 40, NULL);   /* animation  */
    SetTimer(g_splash, 2, 200, NULL);  /* close check */
    ShowWindow(g_splash, SW_SHOW);
    UpdateWindow(g_splash);
}

/* ------------------------------------------------------------------ */
/*  uninstall                                                         */
/* ------------------------------------------------------------------ */
static void delete_tree(const char *dir)
{
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    path_join(pattern, sizeof(pattern), dir, "*");
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char full[MAX_PATH];
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        path_join(full, sizeof(full), dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) delete_tree(full);
        else {
            SetFileAttributesA(full, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(full);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    RemoveDirectoryA(dir);
}

static void remove_shortcut(int csidl, const char *name)
{
    char dir[MAX_PATH], file[MAX_PATH];
    if (SHGetFolderPathA(NULL, csidl, NULL, 0, dir) != S_OK) return;
    path_join(file, sizeof(file), dir, name);
    DeleteFileA(file);
}

static int do_uninstall(void)
{
    char exe[MAX_PATH], root[MAX_PATH], dir[MAX_PATH], cmd[1024];
    int rc;
    HKEY k;

    if (MessageBoxA(NULL, "Remove SPACE MINER from this computer?\n\n"
                           "Save files stored by the game will also be removed.",
                    "SPACE MINER — Uninstall", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES)
        return 0;

    GetModuleFileNameA(NULL, exe, MAX_PATH);
    _snprintf(dir, sizeof(dir), "%s", exe);
    { char *p = strrchr(dir, '\\'); if (p) *p = 0; }

    remove_shortcut(CSIDL_DESKTOPDIRECTORY, "SPACE MINER.lnk");
    remove_shortcut(CSIDL_PROGRAMS, "SPACE MINER.lnk");
    remove_shortcut(CSIDL_PROGRAMS, "SPACE MINER\\SPACE MINER.lnk");
    {
        char startdir[MAX_PATH], sub[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, startdir) == S_OK) {
            path_join(sub, sizeof(sub), startdir, "SPACE MINER");
            delete_tree(sub);
        }
    }
    path_join(root, sizeof(root), dir, "game");
    delete_tree(root);

    {
        char localApp[MAX_PATH], profile[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp) == S_OK) {
            path_join(profile, sizeof(profile), localApp, "SpaceMiner");
            delete_tree(profile);          /* pilot profile / save data */
        }
    }

    RegDeleteKeyA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SpaceMiner");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SpaceMiner");
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegDeleteKeyA(k, "SpaceMiner");
        RegCloseKey(k);
    }

    /* remove the executable itself a moment after we exit */
    _snprintf(cmd, sizeof(cmd),
              "cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"%s\" & rmdir \"%s\" 2>nul", exe, dir);
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    MessageBoxA(NULL, "SPACE MINER has been removed.", "SPACE MINER", MB_ICONINFORMATION | MB_OK);
    rc = 0;
    return rc;
}

/* ------------------------------------------------------------------ */
/*  entry point                                                       */
/* ------------------------------------------------------------------ */
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int show)
{
    HANDLE mutex;
    char exe[MAX_PATH];
    char html[MAX_PATH];
    char browser[MAX_PATH];
    char localApp[MAX_PATH];
    HANDLE proc = NULL;
    DWORD startedAt;
    int usedFallback = 0;
    int haveBrowser = 0;
    (void)inst; (void)prev; (void)show;

    if (cmdLine && (strstr(cmdLine, "/uninstall") || strstr(cmdLine, "-uninstall") ||
                    strstr(cmdLine, "--uninstall")))
        return do_uninstall();

    SetProcessDPIAware();

    /* single instance */
    mutex = CreateMutexA(NULL, TRUE, "SpaceMinerSingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowA("SpaceMinerSplash", NULL);
        if (existing) SetForegroundWindow(existing);
        return 0;
    }

    GetModuleFileNameA(NULL, exe, MAX_PATH);
    _snprintf(g_base, sizeof(g_base), "%s", exe);
    { char *p = strrchr(g_base, '\\'); if (p) *p = 0; }

    path_join(g_root, sizeof(g_root), g_base, "game");
    path_join(html, sizeof(html), g_root, "index.html");
    if (!file_exists(html)) {
        /* also accept a flat portable layout */
        path_join(g_root, sizeof(g_root), g_base, ".");
        _snprintf(html, sizeof(html), "%s\\index.html", g_base);
        if (!file_exists(html)) {
            MessageBoxA(NULL,
                "The game files are missing.\n\nExpected:\n  game\\index.html next to SPACE_MINER.exe\n\n"
                "Reinstall SPACE MINER or extract the portable archive completely.",
                "SPACE MINER", MB_ICONERROR | MB_OK);
            return 1;
        }
    }

    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp) == S_OK) {
        path_join(g_profile, sizeof(g_profile), localApp, "SpaceMiner");
        make_dirs(g_profile);
    } else {
        _snprintf(g_profile, sizeof(g_profile), "%s", g_base);
    }

    show_splash();

    /* ---- try the loopback server ---- */
    g_portEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (g_portEvent) {
        HANDLE th = CreateThread(NULL, 0, server_thread, g_portEvent, 0, NULL);
        if (th) CloseHandle(th);
        WaitForSingleObject(g_portEvent, 3000);
        CloseHandle(g_portEvent);
        g_portEvent = NULL;
    }
    InterlockedExchange(&g_lastRequest, (LONG)GetTickCount());

    haveBrowser = find_browser(browser, sizeof(browser));
    if (g_port > 0) {
        _snprintf(g_url, sizeof(g_url), "http://127.0.0.1:%d/index.html", g_port);
        _snprintf(g_status, sizeof(g_status), "HOSTING LOCAL SESSION  \xB7  PORT %d", g_port);
    } else {
        char fixed[MAX_PATH];
        int i;
        _snprintf(fixed, sizeof(fixed), "%s", html);
        for (i = 0; fixed[i]; i++) if (fixed[i] == '\\') fixed[i] = '/';
        _snprintf(g_url, sizeof(g_url), "file:///%s", fixed);
        _snprintf(g_status, sizeof(g_status), "STARTING SYSTEM BROWSER");
    }
    if (g_splash) InvalidateRect(g_splash, NULL, FALSE);

    startedAt = GetTickCount();
    proc = launch_browser(haveBrowser ? browser : NULL, g_url, &usedFallback);

    if (!proc && !usedFallback) {
        MessageBoxA(NULL,
            "Could not start a browser to host SPACE MINER.\n\n"
            "Install Microsoft Edge or Google Chrome, then run SPACE_MINER.exe again.\n"
            "Alternatively, open game\\index.html in any modern browser.",
            "SPACE MINER", MB_ICONWARNING | MB_OK);
        InterlockedExchange(&g_stopServer, 1);
        if (g_splash) DestroyWindow(g_splash);
        return 2;
    }

    /* keep the splash around until the page loads, then fade it out */
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_splash) break;
        if (GetTickCount() - startedAt > 12000) break;
        if (g_gotRequest) { Sleep(350); break; }
        Sleep(50);
    }
    if (g_splash) DestroyWindow(g_splash);

    /* run until the game window closes (heartbeat stops) */
    for (;;) {
        MSG msg;
        DWORD idle;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        idle = GetTickCount() - (DWORD)g_lastRequest;
        if (g_gotRequest && idle > SRV_IDLE_EXIT_MS) break;
        if (!g_gotRequest && GetTickCount() - startedAt > 90000) break;
        if (proc) {
            DWORD r = WaitForSingleObject(proc, 0);
            if (r == WAIT_OBJECT_0 && !g_gotRequest) break;
        }
        Sleep(200);
    }

    InterlockedExchange(&g_stopServer, 1);
    if (proc) CloseHandle(proc);
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    return 0;
}
