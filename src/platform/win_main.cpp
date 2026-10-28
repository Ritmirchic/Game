// FRACTURE — Windows entry point (Win32 + GL3.3, SW fallback via StretchDIBits)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "../ft/game.h"
#include <cstring>

namespace ft {
IBackend* createGLBackend();
// createSWBackend/createNullBackend declared in render.h
}

using namespace ft;

// ---- platform bridge (win_gl.cpp) ----
namespace ft {
struct WglState;                 // defined in win_gl.cpp
bool wglInit(HWND hwnd, WglState& st);
void wglShutdown(WglState& st);
}
// (we keep the state here through a fixed-size opaque storage)
alignas(8) static unsigned char g_wglMem[64];
static WglState* wglState() { return reinterpret_cast<WglState*>(g_wglMem); }

static HWND g_hwnd = nullptr;
static InputState g_in;
static bool g_quit = false;
static bool g_captured = false;
static f64 g_qpcFreq = 1.0, g_qpcLast = 0;
static f32 g_mouseAccX = 0, g_mouseAccY = 0;

static int keyFromVK(WPARAM vk) {
    switch (vk) {
        case 'W': return KEY_W; case 'A': return KEY_A; case 'S': return KEY_S; case 'D': return KEY_D;
        case 'Q': return KEY_Q; case 'E': return KEY_E; case 'R': return KEY_R; case 'F': return KEY_F;
        case 'G': return KEY_G; case 'H': return KEY_H; case 'Z': return KEY_Z; case 'X': return KEY_X;
        case 'C': return KEY_C; case 'V': return KEY_V; case 'B': return KEY_B;
        case VK_SPACE: return KEY_SPACE;
        case VK_SHIFT: return KEY_SHIFT;
        case VK_CONTROL: return KEY_CTRL;
        case VK_TAB: return KEY_TAB;
        case VK_ESCAPE: return KEY_ESC;
        case VK_RETURN: return KEY_ENTER;
        case VK_BACK: return KEY_BACKSPACE;
        case '1': return KEY_1; case '2': return KEY_2; case '3': return KEY_3; case '4': return KEY_4;
        case '5': return KEY_5; case '6': return KEY_6; case '7': return KEY_7; case '8': return KEY_8;
        case '9': return KEY_9; case '0': return KEY_0;
        case VK_F1: return KEY_F1; case VK_F2: return KEY_F2; case VK_F3: return KEY_F3;
        case VK_F4: return KEY_F4; case VK_F5: return KEY_F5; case VK_F11: return KEY_F11;
        case VK_UP: return KEY_UP; case VK_DOWN: return KEY_DOWN;
        case VK_LEFT: return KEY_LEFT; case VK_RIGHT: return KEY_RIGHT;
        case VK_LBUTTON: return KEY_LMB; case VK_RBUTTON: return KEY_RMB; case VK_MBUTTON: return KEY_MMB;
        default: return KEY_NONE;
    }
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int k = keyFromVK(wp);
            if (k != KEY_NONE) g_in.key(k, true);
            if (wp == VK_ESCAPE || wp == VK_F4) {}   // handled by game / default
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            int k = keyFromVK(wp);
            if (k != KEY_NONE) g_in.key(k, false);
            return 0;
        }
        case WM_KILLFOCUS:
            for (int i = 0; i < KEY_COUNT; i++) g_in.key(i, false);
            return 0;
        case WM_LBUTTONDOWN: g_in.key(KEY_LMB, true); SetCapture(hwnd); return 0;
        case WM_LBUTTONUP:   g_in.key(KEY_LMB, false); ReleaseCapture(); return 0;
        case WM_RBUTTONDOWN: g_in.key(KEY_RMB, true); return 0;
        case WM_RBUTTONUP:   g_in.key(KEY_RMB, false); return 0;
        case WM_MBUTTONDOWN: g_in.key(KEY_MMB, true); return 0;
        case WM_MBUTTONUP:   g_in.key(KEY_MMB, false); return 0;
        case WM_MOUSEMOVE: {
            int x = (int)(short)LOWORD(lp), y = (int)(short)HIWORD(lp);
            if (g_captured) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int cx = (rc.right - rc.left) / 2, cy = (rc.bottom - rc.top) / 2;
                g_mouseAccX += (f32)(x - cx);
                g_mouseAccY += (f32)(y - cy);
                POINT cp{cx, cy};
                ClientToScreen(hwnd, &cp);
                SetCursorPos(cp.x, cp.y);
            } else {
                g_in.cursorX = (f32)x;
                g_in.cursorY = (f32)y;
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
            g_in.wheel += (f32)GET_WHEEL_DELTA_WPARAM(wp) / (f32)WHEEL_DELTA;
            return 0;
        case WM_CLOSE:
            g_quit = true;
            return 0;
        case WM_DESTROY:
            g_quit = true;
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static std::string exeDir() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(n ? buf : ".", n ? n : 1);
    size_t p = s.find_last_of("\\/");
    if (p != std::string::npos) s.resize(p);
    return s.empty() ? std::string(".") : s;
}

// SW fallback blit (no GL): backend framebuffer → window via StretchDIBits
static void blitSW(IBackend* be, HDC dc, int w, int h) {
    static std::vector<u8> px;
    be->readPixels(px);
    if (px.size() != (size_t)w * h * 3) return;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;   // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(dc, 0, 0, w, h, 0, 0, w, h, px.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    (void)hInst; (void)nCmdShow;
    // timing
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    g_qpcFreq = (f64)f.QuadPart;
    g_qpcLast = (f64)t.QuadPart;

    // window (fixed size for MVP)
    int W = 1280, H = 720;
    WNDCLASSA wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "FractureWnd";
    RegisterClassA(&wc);
    RECT rc{0, 0, W, H};
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    g_hwnd = CreateWindowA("FractureWnd", "FRACTURE — TOTAL DESTRUCTION",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           (GetSystemMetrics(SM_CXSCREEN) - ww) / 2,
                           (GetSystemMetrics(SM_CYSCREEN) - wh) / 2,
                           ww, wh, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW);

    // backend: GL first, software fallback
    IBackend* backend = nullptr;
    WglState* ws = wglState();
    bool glOk = false;
    if (wglInit(g_hwnd, *ws)) {
        backend = createGLBackend();
        if (!backend->init(g_hwnd, W, H, 1, true)) {
            backend->shutdown();
            delete backend;
            backend = nullptr;
        } else {
            glOk = true;
        }
    }
    if (!glOk) {
        ft::logf("GL unavailable — software renderer fallback");
        backend = createSWBackend();
        if (!backend->init(g_hwnd, W, H, 1, false)) return 2;
    }
    ft::logf("renderer: %s", backend->name());

    g_dataDir = exeDir();
    Game game;
    if (!game.init(backend)) return 3;
    if (!game.buildScene("test", 1)) return 4;
    game.screen = SCR_MENU;
    game.playing = false;

    f64 last = g_qpcLast;
    while (!g_quit && !game.wantQuit && !g_in.quitRequested) {
        g_in.beginFrame();
        // pump messages (updates g_in edges / mouse)
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // mouse look deltas accumulated by wndProc (captured mode)
        if (g_captured) {
            g_in.mx += g_mouseAccX;
            g_in.my += g_mouseAccY;
        }
        g_mouseAccX = g_mouseAccY = 0;
        // cursor capture when playing
        bool wantCapture = game.playing && game.screen == SCR_PLAY;
        if (wantCapture != g_captured) {
            g_captured = wantCapture;
            g_in.mouseCaptured = g_captured;
            ShowCursor(g_captured ? FALSE : TRUE);
            if (g_captured) {
                RECT rc2;
                GetClientRect(g_hwnd, &rc2);
                POINT cp{(rc2.right - rc2.left) / 2, (rc2.bottom - rc2.top) / 2};
                ClientToScreen(g_hwnd, &cp);
                SetCursorPos(cp.x, cp.y);
                RECT clip = rc2;
                MapWindowPoints(g_hwnd, nullptr, (POINT*)&clip, 2);
                ClipCursor(&clip);
            } else {
                ClipCursor(nullptr);
            }
        }
        // cursor position in captured mode = center (menus use absolute)
        if (g_captured) {
            RECT rc2;
            GetClientRect(g_hwnd, &rc2);
            g_in.cursorX = (f32)((rc2.right - rc2.left) / 2);
            g_in.cursorY = (f32)((rc2.bottom - rc2.top) / 2);
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        f64 fdt = ((f64)now.QuadPart - last) / g_qpcFreq;
        last = (f64)now.QuadPart;
        f32 dt = (f32)tclamp((f64)fdt, 0.0, 0.1);

        game.tick(dt, g_in);
        game.render();
        if (glOk) {
            SwapBuffers(GetDC(g_hwnd));
        } else {
            blitSW(backend, GetDC(g_hwnd), W, H);
        }
        // simple frame pacing when vsync unavailable
        if (game.settings.fpsLimit > 0) {
            f64 target = 1.0 / game.settings.fpsLimit;
            LARGE_INTEGER n2;
            QueryPerformanceCounter(&n2);
            f64 elapsed = ((f64)n2.QuadPart - last) / g_qpcFreq;
            if (elapsed < target - 0.002) Sleep((DWORD)((target - elapsed) * 1000.0));
        }
    }

    game.shutdown();
    backend->shutdown();
    delete backend;
    wglShutdown(*ws);
    DestroyWindow(g_hwnd);
    return 0;
}

// MinGW/zig entry shim
#ifdef FRATURE_MINIMAL_MAIN
int main() { return 0; }
#endif
#else
int main() { return 1; }
#endif
