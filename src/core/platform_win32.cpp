// FRACTURE — Win32 platform layer: window, WGL 3.3 context, input, timing, paths.
#ifdef _WIN32
#include "core/engine.h"
#include "render/gl_loader.h"
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <vector>

static HWND g_hwnd = nullptr;
static HDC g_hdc = nullptr;
static HGLRC g_glrc = nullptr;
static bool g_quit = false;
static bool g_captured = false;
static int g_w = 1280, g_h = 720;
static int g_windowMode = 0;
static bool g_init = false;
static LARGE_INTEGER g_freq;
static double g_startTime = 0;
static POINT g_lastMouse{0, 0};
static bool g_firstMouse = true;
static WINDOWPLACEMENT g_prevPlacement = {sizeof(WINDOWPLACEMENT)};
static int g_centerX = 0, g_centerY = 0;
static bool g_skipInputThisFrame = false;
typedef BOOL(WINAPI* PFN_SetProcessDPIAware)(void);

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

InputState g_input;   // global input state filled by wndProc, consumed by platform::poll

static void enterFullscreen(bool on) {
    if (!g_hwnd) return;
    if (on) {
        DWORD style = GetWindowLong(g_hwnd, GWL_STYLE);
        MONITORINFO mi = {sizeof(MONITORINFO)};
        if (GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(g_hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        DWORD style = GetWindowLong(g_hwnd, GWL_STYLE);
        SetWindowLong(g_hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_hwnd, &g_prevPlacement);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

namespace platform {

bool init(const char* title, int w, int h, const Settings& st) {
    if (g_init) return true;
    g_w = w; g_h = h;
    QueryPerformanceFrequency(&g_freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    g_startTime = (double)now.QuadPart / (double)g_freq.QuadPart;

    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32) {
        PFN_SetProcessDPIAware f = (PFN_SetProcessDPIAware)GetProcAddress(user32, "SetProcessDPIAware");
        if (f) f();
    }
    HINSTANCE hi = GetModuleHandleA(nullptr);
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconA(hi, "MAINICON");
    wc.lpszClassName = "FRACTUREWND";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!RegisterClassExA(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            logMsg("[platform] RegisterClass failed %lu", GetLastError());
        }
    }
    // scale to fit the desktop
    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if (ww > sw - 40) { ww = sw - 40; }
    if (wh > sh - 80) { wh = sh - 80; }
    g_hwnd = CreateWindowExA(0, "FRACTUREWND", title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                             (sw - ww) / 2, (sh - wh) / 2, ww, wh, nullptr, nullptr, hi, nullptr);
    if (!g_hwnd) {
        logMsg("[platform] CreateWindow failed %lu", GetLastError());
        return false;
    }
    GetWindowPlacement(g_hwnd, &g_prevPlacement);
    g_hdc = GetDC(g_hwnd);

    // ---- WGL: dummy context, then a 3.3 core context ----
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_hdc, pf, &pfd)) {
        logMsg("[platform] SetPixelFormat failed");
        return false;
    }
    HGLRC dummy = wglCreateContext(g_hdc);
    if (!dummy) { logMsg("[platform] wglCreateContext failed"); return false; }
    wglMakeCurrent(g_hdc, dummy);
    PFN_wglCreateContextAttribsARB createCtx = (PFN_wglCreateContextAttribsARB)wglGetProcAddress("wglCreateContextAttribsARB");
    if (createCtx) {
        const int attribs[] = {0x2091, 3, 0x2092, 3, 0x9126, 0x00000001, 0}; // major 3, minor 3, core profile
        HGLRC core = createCtx(g_hdc, nullptr, attribs);
        if (core) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(dummy);
            dummy = core;
            if (!wglMakeCurrent(g_hdc, dummy)) {
                logMsg("[platform] wglMakeCurrent(core) failed");
            }
        } else {
            logMsg("[platform] 3.3 core context unavailable, using compatibility context");
        }
    }
    g_glrc = dummy;
    PFN_wglSwapIntervalEXT swapInterval = (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInterval) swapInterval(st.vsync ? 1 : 0);
    glLoad();
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_hwnd);
    g_init = true;
    g_centerX = ww / 2;
    g_centerY = wh / 2;
    logMsg("[platform] window %dx%d", ww, wh);
    return true;
}

void shutdown() {
    if (g_glrc) wglMakeCurrent(nullptr, nullptr);
    if (g_glrc) wglDeleteContext(g_glrc);
    if (g_hdc) ReleaseDC(g_hwnd, g_hdc);
    if (g_hwnd) DestroyWindow(g_hwnd);
    g_glrc = nullptr;
    g_hdc = nullptr;
    g_hwnd = nullptr;
    g_init = false;
}

static void pushKey(UINT msg, WPARAM wp, bool down) {
    int vk = (int)wp;
    if (vk < 0 || vk > 255) return;
    if (down) {
        if (!g_input.down[vk]) g_input.pressed[vk] = true;
        g_input.down[vk] = true;
    } else {
        g_input.down[vk] = false;
        g_input.released[vk] = true;
    }
    if (down) {
        if (msg == WM_CHAR) {
            char c = (char)wp;
            if (g_input.textLen < 15) g_input.textInput[g_input.textLen++] = c;
        }
    }
}

bool poll(InputState& in) {
    in.clearFrame();
    // move the accumulated state into the caller's struct
    static bool first = true;
    static uint32_t lastPressed[256] = {0};
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) g_quit = true;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    in = g_input;
    // pressed/released flags are consumed once
    std::memset(g_input.pressed, 0, sizeof(g_input.pressed));
    std::memset(g_input.released, 0, sizeof(g_input.released));
    g_input.mouseDX = 0;
    g_input.mouseDY = 0;
    g_input.wheel = 0;
    (void)first; (void)lastPressed;
    return !g_quit;
}

void swapBuffers() {
    if (g_hdc) SwapBuffers(g_hdc);
}
double time() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)g_freq.QuadPart - g_startTime;
}
void sleepMs(int ms) { Sleep(ms); }
void setTitle(const char* t) { if (g_hwnd) SetWindowTextA(g_hwnd, t); }
void* hwnd() { return g_hwnd; }
void* glContext() { return g_glrc; }

std::string dataDir() {
    static std::string d;
    if (!d.empty()) return d;
    char path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        d = std::string(path) + "\\FRACTURE";
    } else {
        d = ".\\FRACTURE";
    }
    CreateDirectoryA(d.c_str(), nullptr);
    return d;
}

std::string exeDir() {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string s(path);
    size_t p = s.find_last_of("\\/");
    return p == std::string::npos ? std::string(".") : s.substr(0, p);
}

void messageBox(const char* title, const char* text) {
    MessageBoxA(g_hwnd, text, title, MB_OK | MB_ICONINFORMATION);
}

void setMouseCapture(bool captured) {
    if (captured == g_captured) return;
    g_captured = captured;
    if (captured) {
        SetCapture(g_hwnd);
        RECT rc;
        GetClientRect(g_hwnd, &rc);
        POINT c{(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
        ClientToScreen(g_hwnd, &c);
        SetCursorPos(c.x, c.y);
        g_firstMouse = true;
        ShowCursor(FALSE);
        ClipCursor(nullptr);
    } else {
        ReleaseCapture();
        ShowCursor(TRUE);
        ClipCursor(nullptr);
    }
}
bool mouseCaptured() { return g_captured; }

void setFullscreen(int mode, int w, int h) {
    g_windowMode = mode;
    if (mode == 2) {
        enterFullscreen(true);
    } else if (mode == 1) {
        enterFullscreen(false);
        // borderless windowed at the requested size
        DWORD style = GetWindowLong(g_hwnd, GWL_STYLE);
        SetWindowLong(g_hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        RECT rc = {0, 0, w, h};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(g_hwnd, HWND_TOP, (sw - w) / 2, (sh - h) / 2, w, h, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        enterFullscreen(false);
        RECT rc = {0, 0, w, h};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(g_hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    g_w = w; g_h = h;
}
void toggleFullscreen() {
    setFullscreen(g_windowMode == 2 ? 0 : 2, g_w, g_h);
}
int windowWidth() {
    RECT rc;
    if (g_hwnd && GetClientRect(g_hwnd, &rc)) return rc.right - rc.left;
    return g_w;
}
int windowHeight() {
    RECT rc;
    if (g_hwnd && GetClientRect(g_hwnd, &rc)) return rc.bottom - rc.top;
    return g_h;
}
bool shouldQuit() { return g_quit; }
void requestQuit() { g_quit = true; }

// software fallback presentation (GDI)
void presentSoftware(const uint32_t* pixels, int w, int h) {
    if (!g_hdc || !pixels) return;
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    SetStretchBltMode(g_hdc, HALFTONE);
    StretchDIBits(g_hdc, 0, 0, rc.right, rc.bottom, 0, 0, w, h, pixels, &bi, DIB_RGB_COLORS, SRCCOPY);
}

} // namespace platform

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    static bool sizing = false;
    switch (msg) {
        case WM_CLOSE:
            g_quit = true;
            return 0;
        case WM_DESTROY:
            g_quit = true;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                g_w = LOWORD(lp);
                g_h = HIWORD(lp);
            }
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int vk = (int)wp;
            // Alt+Enter fullscreen toggle
            if (vk == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000)) {
                platform::toggleFullscreen();
                return 0;
            }
            if (vk == VK_F4 && (GetKeyState(VK_MENU) & 0x8000)) {
                g_quit = true;
                return 0;
            }
            if (vk < 256) {
                if (!g_input.down[vk]) g_input.pressed[vk] = true;
                g_input.down[vk] = true;
            }
            // swallow alt-enter default beep
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            int vk = (int)wp;
            if (vk < 256) {
                g_input.down[vk] = false;
                g_input.released[vk] = true;
            }
            return 0;
        }
        case WM_CHAR: {
            char c = (char)wp;
            if (c >= 32 && c < 127 && g_input.textLen < 15) g_input.textInput[g_input.textLen++] = c;
            return 0;
        }
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
            g_input.down[VK_LBUTTON] = true; g_input.pressed[VK_LBUTTON] = true;
            if (!g_captured) platform::setMouseCapture(true);
            return 0;
        case WM_LBUTTONUP:
            g_input.down[VK_LBUTTON] = false; g_input.released[VK_LBUTTON] = true;
            return 0;
        case WM_RBUTTONDOWN:
            g_input.down[VK_RBUTTON] = true; g_input.pressed[VK_RBUTTON] = true;
            return 0;
        case WM_RBUTTONUP:
            g_input.down[VK_RBUTTON] = false; g_input.released[VK_RBUTTON] = true;
            return 0;
        case WM_MBUTTONDOWN:
            g_input.down[VK_MBUTTON] = true; g_input.pressed[VK_MBUTTON] = true;
            return 0;
        case WM_MBUTTONUP:
            g_input.down[VK_MBUTTON] = false; g_input.released[VK_MBUTTON] = true;
            return 0;
        case WM_MOUSEWHEEL:
            g_input.wheel += (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.0f;
            return 0;
        case WM_MOUSEMOVE: {
            int mx = (int)(short)LOWORD(lp), my = (int)(short)HIWORD(lp);
            g_input.mouseX = (float)mx;
            g_input.mouseY = (float)my;
            if (g_captured) {
                RECT rc;
                GetClientRect(h, &rc);
                int cx = (rc.right - rc.left) / 2, cy = (rc.bottom - rc.top) / 2;
                int dx = mx - cx, dy = my - cy;
                if (dx != 0 || dy != 0) {
                    g_input.mouseDX += (float)dx;
                    g_input.mouseDY += (float)dy;
                    POINT p{cx, cy};
                    ClientToScreen(h, &p);
                    SetCursorPos(p.x, p.y);
                }
            }
            return 0;
        }
        case WM_ACTIVATEAPP:
            if (!wp) {
                // lost focus: release the mouse so the player can alt-tab
                if (g_captured) platform::setMouseCapture(false);
            }
            return 0;
        case WM_KILLFOCUS:
            std::memset(g_input.down, 0, sizeof(g_input.down));
            return 0;
        case WM_SETCURSOR:
            if (g_captured) { SetCursor(nullptr); return TRUE; }
            break;
        case WM_SIZING:
            sizing = true;
            return TRUE;
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

// ---------------- logging ----------------
#include <cstdarg>
static FILE* g_logFile = nullptr;
static bool g_logTried = false;
void logMsg(const char* fmt, ...) {
    if (!g_logTried) {
        g_logTried = true;
        std::string path = platform::dataDir() + "\\fracture.log";
        g_logFile = fopen(path.c_str(), "w");
    }
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (g_logFile) { fprintf(g_logFile, "%s\n", buf); fflush(g_logFile); }
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}
void logFlush() { if (g_logFile) fflush(g_logFile); }

#endif // _WIN32
