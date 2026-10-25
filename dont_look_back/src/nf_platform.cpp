// ============================================================================
//  nf_platform.cpp : Win32 window / input / present; headless fallback
// ============================================================================
#include "nf_platform.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <windowsx.h>
#  include <dwmapi.h>
#  pragma comment(lib, "dwmapi.lib")
#endif

namespace nf {

#ifdef _WIN32

static Platform* g_plat = nullptr;
static Input* g_input = nullptr;
static bool g_ignoreNextMove = false;

static int mapVk(WPARAM vk, LPARAM lp) {
    switch (vk) {
        case VK_ESCAPE: return KC_ESC;
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return KC_SHIFT;
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return KC_CTRL;
        case VK_MENU: return KC_ALT;
        case VK_SPACE: return KC_SPACE;
        case VK_TAB: return KC_TAB;
        case VK_RETURN: return KC_ENTER;
        case VK_BACK: return KC_BACK;
        case VK_UP: return KC_UP;
        case VK_DOWN: return KC_DOWN;
        case VK_LEFT: return KC_LEFT;
        case VK_RIGHT: return KC_RIGHT;
        case VK_DELETE: return KC_DELETE;
        case VK_OEM_COMMA: return KC_COMMA;
        case VK_OEM_PERIOD: return KC_PERIOD;
        case VK_OEM_MINUS: return KC_MINUS;
        case VK_OEM_PLUS: return KC_EQUALS;
        default: break;
    }
    if (vk >= 'A' && vk <= 'Z') return (int)vk;
    if (vk >= '0' && vk <= '9') return (int)vk;
    if (vk >= VK_F1 && vk <= VK_F12) return KC_F1 + (int)(vk - VK_F1);
    (void)lp;
    return KC_NONE;
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE: if (g_input) g_input->quit = true; return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_SIZE:
            if (g_plat) {
                int w = LOWORD(lp), h = HIWORD(lp);
                if (w > 0 && h > 0) {
                    extern void platformOnResize(int, int);
                    platformOnResize(w, h);
                }
            }
            return 0;
        case WM_ERASEBKGND: return 1;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (!g_input) return 0;
            int k = mapVk(wp, lp);
            if (k > 0 && k < KC_COUNT) {
                if (!g_input->down[k]) g_input->pressed[k] = true;
                g_input->down[k] = true;
            }
            if (wp == VK_F4 && (GetKeyState(VK_MENU) & 0x8000)) g_input->quit = true;
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            if (!g_input) return 0;
            int k = mapVk(wp, lp);
            if (k > 0 && k < KC_COUNT) {
                g_input->down[k] = false;
                g_input->released[k] = true;
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!g_input) return 0;
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            g_input->mouseX = x; g_input->mouseY = y;
            if (g_input->captured) {
                extern void platformOnMouseMove(int, int);
                platformOnMouseMove(x, y);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: if (g_input) { g_input->mouse[0] = true; g_input->mousePressed[0] = true; SetCapture(hwnd); } return 0;
        case WM_LBUTTONUP:   if (g_input) { g_input->mouse[0] = false; ReleaseCapture(); } return 0;
        case WM_RBUTTONDOWN: if (g_input) { g_input->mouse[1] = true; g_input->mousePressed[1] = true; } return 0;
        case WM_RBUTTONUP:   if (g_input) g_input->mouse[1] = false; return 0;
        case WM_MBUTTONDOWN: if (g_input) { g_input->mouse[2] = true; g_input->mousePressed[2] = true; } return 0;
        case WM_MBUTTONUP:   if (g_input) g_input->mouse[2] = false; return 0;
        case WM_MOUSEWHEEL:  if (g_input) g_input->wheel += GET_WHEEL_DELTA_WPARAM(wp) / 120.0f; return 0;
        case WM_SETFOCUS: if (g_input) g_input->focus = true; return 0;
        case WM_KILLFOCUS:
            if (g_input) {
                g_input->focus = false;
                memset(g_input->down, 0, sizeof(g_input->down));
                g_input->mouse[0] = g_input->mouse[1] = g_input->mouse[2] = false;
            }
            return 0;
        default: break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// resize callback hooked by the app
static std::function<void(int, int)> g_resizeCb;
static std::function<void(int, int)> g_mouseCb;
void platformSetResizeCallback(std::function<void(int, int)> cb) { g_resizeCb = cb; }
void platformSetMouseCallback(std::function<void(int, int)> cb) { g_mouseCb = cb; }
void platformOnResize(int w, int h) { if (g_resizeCb) g_resizeCb(w, h); }
void platformOnMouseMove(int x, int y) { if (g_mouseCb) g_mouseCb(x, y); }

bool Platform::createWindow(const WindowOptions& opt) {
    g_plat = this;
    SetProcessDPIAware();
    HINSTANCE hinst = GetModuleHandleA(nullptr);
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "NightfallWindow";
    wc.hIcon = opt.iconResourceId ? LoadIconA(hinst, MAKEINTRESOURCEA(opt.iconResourceId)) : nullptr;
    wc.hIconSm = wc.hIcon;
    RegisterClassExA(&wc);

    RECT r = {0, 0, opt.width, opt.height};
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!opt.resizable) style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowExA(0, "NightfallWindow", opt.title.c_str(), style,
                                CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                                nullptr, nullptr, hinst, nullptr);
    if (!hwnd) return false;
    hwnd_ = hwnd;
    winW_ = opt.width; winH_ = opt.height;
    w_ = opt.width; h_ = opt.height;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    // dark title bar on Win10/11
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    SetForegroundWindow(hwnd);
    // GDI backbuffer
    HDC hdc = GetDC(hwnd);
    dcMem_ = CreateCompatibleDC(hdc);
    ReleaseDC(hwnd, hdc);
    return true;
}

void Platform::destroyWindow() {
    if (hwnd_) {
        if (bmp_) { DeleteObject((HBITMAP)bmp_); bmp_ = nullptr; }
        if (dcMem_) { DeleteDC((HDC)dcMem_); dcMem_ = nullptr; }
        DestroyWindow((HWND)hwnd_);
        hwnd_ = nullptr;
    }
}

bool Platform::pumpEvents(Input& in) {
    g_input = &in;
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (in.captured && GetForegroundWindow() == (HWND)hwnd_) {
        POINT p;
        GetCursorPos(&p);
        POINT c = {winX_ + w_ / 2, winY_ + h_ / 2};
        if (p.x != c.x || p.y != c.y) {
            in.mouseDX += (float)(p.x - c.x);
            in.mouseDY += (float)(p.y - c.y);
            SetCursorPos(c.x, c.y);
        }
        // keep the window rect fresh
        RECT rr;
        GetClientRect((HWND)hwnd_, &rr);
        POINT tl = {0, 0};
        ClientToScreen((HWND)hwnd_, &tl);
        winX_ = tl.x; winY_ = tl.y;
    }
    return !in.quit;
}

void Platform::setTitle(const std::string& t) {
    if (hwnd_) SetWindowTextA((HWND)hwnd_, t.c_str());
}

void Platform::setCursorCaptured(bool c) {
    captured_ = c;
    if (c) {
        ShowCursor(FALSE);
        RECT rr;
        GetClientRect((HWND)hwnd_, &rr);
        MapWindowPoints((HWND)hwnd_, nullptr, (LPPOINT)&rr, 2);
        ClipCursor(&rr);
        POINT tl = {0, 0};
        ClientToScreen((HWND)hwnd_, &tl);
        SetCursorPos(tl.x + w_ / 2, tl.y + h_ / 2);
    } else {
        ShowCursor(TRUE);
        ClipCursor(nullptr);
    }
}

void Platform::setFullscreen(bool full) {
    fullscreen_ = full;
    if (!hwnd_) return;
    HWND hwnd = (HWND)hwnd_;
    if (full) {
        RECT before = {0, 0, 0, 0};
        GetWindowRect(hwnd, &before);
        winX_ = before.left; winY_ = before.top;
        winW_ = before.right - before.left; winH_ = before.bottom - before.top;
        SetWindowLongPtrA(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        MONITORINFO mi = {sizeof(MONITORINFO)};
        GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        SetWindowLongPtrA(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        RECT r = {0, 0, winW_, winH_};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(hwnd, HWND_TOP, 80, 60, r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}

void Platform::toggleFullscreen() { setFullscreen(!fullscreen_); }

void Platform::showMessage(const std::string& title, const std::string& msg) {
    MessageBoxA((HWND)hwnd_, msg.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

void Platform::sleepMs(int ms) { Sleep(ms); }

void Platform::present(const Color* pixels, int w, int h) {
    if (!hwnd_ || !dcMem_) return;
    if (!bmp_ || w != w_ || h != h_) {
        if (bmp_) DeleteObject((HBITMAP)bmp_);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;   // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        bmp_ = CreateDIBSection((HDC)dcMem_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        SelectObject((HDC)dcMem_, (HBITMAP)bmp_);
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetDIBits((HDC)dcMem_, (HBITMAP)bmp_, 0, h, pixels, &bi, DIB_RGB_COLORS);
    HDC hdc = GetDC((HWND)hwnd_);
    BitBlt(hdc, 0, 0, w, h, (HDC)dcMem_, 0, 0, SRCCOPY);
    ReleaseDC((HWND)hwnd_, hdc);
}

void Platform::setWindowIconFromImage(const Image& img) {
    if (!hwnd_) return;
    // build a 32x32 icon from the image
    std::vector<Color> px(32 * 32);
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x)
        px[y * 32 + x] = img.sampleBilinear((x + 0.5f) / 32.0f, (y + 0.5f) / 32.0f);
    HBITMAP color = CreateBitmap(32, 32, 1, 32, px.data());
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, nullptr);
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    HICON ic = CreateIconIndirect(&ii);
    SendMessageA((HWND)hwnd_, WM_SETICON, ICON_BIG, (LPARAM)ic);
    SendMessageA((HWND)hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)ic);
    DeleteObject(color);
    DeleteObject(mask);
}

int systemMemoryMB() {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    return (int)(ms.ullTotalPhys / (1024 * 1024));
#else
    return 8192;
#endif
}

#else  // ------------------------------------------------------------- headless

bool Platform::createWindow(const WindowOptions& opt) {
    headless = true;
    w_ = opt.width; h_ = opt.height;
    return true;
}
void Platform::destroyWindow() {}
bool Platform::pumpEvents(Input& in) { return !in.quit; }
void Platform::setTitle(const std::string&) {}
void Platform::setCursorCaptured(bool c) { captured_ = c; }
void Platform::setFullscreen(bool f) { fullscreen_ = f; }
void Platform::toggleFullscreen() {}
void Platform::showMessage(const std::string& t, const std::string& m) {
    logMsg("[message] %s: %s", t.c_str(), m.c_str());
}
void Platform::sleepMs(int ms) { sleepMillis(ms); }
void Platform::present(const Color*, int, int) {}
void Platform::setWindowIconFromImage(const Image&) {}
int systemMemoryMB() { return 8192; }
void platformSetResizeCallback(std::function<void(int, int)>) {}
void platformSetMouseCallback(std::function<void(int, int)>) {}

#endif

} // namespace nf
