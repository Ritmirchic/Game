// Win32 platform: window, DIB blit, raw mouse input, waveOut audio.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <shellscalingapi.h>
#include <stdlib.h>
#include <stdio.h>
#include "platform.h"

#define AUD_FRAMES 1024
#define AUD_BUFFERS 4

static Platform *g_plat = NULL;
static HWND g_hwnd = NULL;
static HDC g_hdc = NULL;
static HBITMAP g_bmp = NULL;
static uint32_t *g_pixels = NULL;
static int g_win_w = 0, g_win_h = 0;
static int g_quit = 0;
static int g_mouse_captured = 0;
static LARGE_INTEGER g_qpc_freq;
static double g_qpc_hz = 1.0;
static LARGE_INTEGER g_qpc_start;

// ---- audio
static HWAVEOUT g_wave = NULL;
static WAVEHDR g_hdr[AUD_BUFFERS];
static int16_t *g_abuf[AUD_BUFFERS];
static int g_abuf_frames = AUD_FRAMES;
static int g_audio_running = 0;
static CRITICAL_SECTION g_audio_lock;
static int g_audio_lock_init = 0;

static void audio_fill(int idx) {
    Platform *p = g_plat;
    int16_t *buf = g_abuf[idx];
    memset(buf, 0, sizeof(int16_t) * 2 * (size_t)g_abuf_frames);
    if (p && p->on_audio) {
        EnterCriticalSection(&g_audio_lock);
        p->on_audio(p->audio_user, buf, g_abuf_frames);
        LeaveCriticalSection(&g_audio_lock);
    }
}

static int plat_audio_start_impl(Platform *p, int frames) {
    if (!p || g_audio_running) return 0;
    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = 44100;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    g_abuf_frames = frames > 0 ? frames : AUD_FRAMES;
    if (g_abuf_frames > 4096) g_abuf_frames = 4096;
    MMRESULT r = waveOutOpen(&g_wave, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR) return 0;
    for (int i = 0; i < AUD_BUFFERS; i++) {
        g_abuf[i] = (int16_t *)calloc((size_t)g_abuf_frames * 2, sizeof(int16_t));
        memset(&g_hdr[i], 0, sizeof g_hdr[i]);
        g_hdr[i].lpData = (LPSTR)g_abuf[i];
        g_hdr[i].dwBufferLength = (DWORD)(g_abuf_frames * 2 * (int)sizeof(int16_t));
        audio_fill(i);
        waveOutPrepareHeader(g_wave, &g_hdr[i], sizeof g_hdr[i]);
        waveOutWrite(g_wave, &g_hdr[i], sizeof g_hdr[i]);
    }
    g_audio_running = 1;
    p->audio_running = 1;
    return 1;
}

static void audio_service(void) {
    if (!g_audio_running) return;
    for (int i = 0; i < AUD_BUFFERS; i++) {
        if (g_hdr[i].dwFlags & WHDR_DONE) {
            audio_fill(i);
            g_hdr[i].dwFlags &= ~WHDR_DONE;
            waveOutWrite(g_wave, &g_hdr[i], sizeof g_hdr[i]);
        }
    }
}

static void plat_audio_stop_impl(void) {
    if (!g_audio_running) return;
    waveOutReset(g_wave);
    for (int i = 0; i < AUD_BUFFERS; i++) {
        waveOutUnprepareHeader(g_wave, &g_hdr[i], sizeof g_hdr[i]);
        free(g_abuf[i]);
        g_abuf[i] = NULL;
    }
    waveOutClose(g_wave);
    g_wave = NULL;
    g_audio_running = 0;
    if (g_plat) g_plat->audio_running = 0;
}

// ---- window
static void resize_dib(Platform *p, int w, int h) {
    if (w == g_win_w && h == g_win_h && g_pixels) return;
    if (g_bmp) DeleteObject(g_bmp);
    if (g_pixels) free(g_pixels);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;      // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(NULL);
    g_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&g_pixels, NULL, 0);
    ReleaseDC(NULL, screen);
    g_win_w = w;
    g_win_h = h;
    if (g_hdc) DeleteDC(g_hdc);
    g_hdc = CreateCompatibleDC(NULL);
    SelectObject(g_hdc, g_bmp);
    p->width = w;
    p->height = h;
    p->dib_pixels = g_pixels;
}

static void keycode_to_input(WPARAM vk, int down, Input *in) {
    int k = -1;
    if (vk >= 'A' && vk <= 'Z') k = (int)vk + 32;
    else if (vk >= '0' && vk <= '9') k = (int)vk;
    else {
        switch (vk) {
            case VK_ESCAPE: k = K_ESC; break;
            case VK_RETURN: k = K_ENTER; break;
            case VK_TAB: k = K_TAB; break;
            case VK_BACK: k = K_BACKSPACE; break;
            case VK_SPACE: k = K_SPACE; break;
            case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: k = K_SHIFT; break;
            case VK_CONTROL: case VK_LCONTROL: k = K_CTRL; break;
            case VK_MENU: k = K_ALT; break;
            case VK_LEFT: k = K_LEFT; break;
            case VK_RIGHT: k = K_RIGHT; break;
            case VK_UP: k = K_UP; break;
            case VK_DOWN: k = K_DOWN; break;
            case VK_F1: k = K_F1; break; case VK_F2: k = K_F2; break;
            case VK_F3: k = K_F3; break; case VK_F4: k = K_F4; break;
            case VK_F5: k = K_F5; break; case VK_F6: k = K_F6; break;
            case VK_F7: k = K_F7; break; case VK_F8: k = K_F8; break;
            case VK_F9: k = K_F9; break; case VK_F10: k = K_F10; break;
            case VK_F11: k = K_F11; break; case VK_F12: k = K_F12; break;
            case VK_INSERT: k = K_INSERT; break;
            case VK_DELETE: k = K_DELETE; break;
            case VK_HOME: k = K_HOME; break;
            case VK_END: k = K_END; break;
            case VK_PRIOR: k = K_PGUP; break;
            case VK_NEXT: k = K_PGDN; break;
            case VK_OEM_PLUS: k = K_PLUS; break;
            case VK_OEM_MINUS: k = K_MINUS; break;
            case VK_OEM_3: k = K_GRAVE; break;
            default: break;
        }
    }
    if (k < 0 || k >= KEY_COUNT) return;
    if (down) {
        if (!in->down[k]) in->pressed[k] = 1;
        in->down[k] = 1;
    } else {
        in->down[k] = 0;
        in->released[k] = 1;
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Platform *p = g_plat;
    Input *in = p ? &p->in : NULL;
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            g_quit = 1;
            if (p) p->quit_requested = 1;
            return 0;
        case WM_SIZE:
            if (p && wp != SIZE_MINIMIZED) {
                resize_dib(p, LOWORD(lp), HIWORD(lp));
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (in) keycode_to_input(wp, 1, in);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (in) keycode_to_input(wp, 0, in);
            return 0;
        case WM_CHAR:
            if (in && in->text_len < 30 && wp >= 32 && wp < 127) {
                in->text[in->text_len++] = (char)wp;
                in->text[in->text_len] = 0;
            }
            return 0;
        case WM_INPUT: {
            if (!p || !in) return DefWindowProc(hwnd, msg, wp, lp);
            UINT size = 0;
            GetRawInputData((HRAWINPUT)lp, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
            if (size > 0 && size < 256) {
                BYTE buf[256];
                if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) == size) {
                    RAWINPUT *raw = (RAWINPUT *)buf;
                    if (raw->header.dwType == RIM_TYPEMOUSE) {
                        in->mouse_dx += (float)raw->data.mouse.lLastX;
                        in->mouse_dy += (float)raw->data.mouse.lLastY;
                    }
                }
            }
            return DefWindowProc(hwnd, msg, wp, lp);
        }
        case WM_MOUSEMOVE:
            if (in) {
                in->mouse_x = (float)GET_X_LPARAM(lp);
                in->mouse_y = (float)GET_Y_LPARAM(lp);
            }
            return 0;
        case WM_LBUTTONDOWN: if (in) { in->mouse_down[0] = 1; in->mouse_pressed[0] = 1; } return 0;
        case WM_LBUTTONUP: if (in) { in->mouse_down[0] = 0; in->mouse_released[0] = 1; } return 0;
        case WM_RBUTTONDOWN: if (in) { in->mouse_down[1] = 1; in->mouse_pressed[1] = 1; } return 0;
        case WM_RBUTTONUP: if (in) { in->mouse_down[1] = 0; in->mouse_released[1] = 1; } return 0;
        case WM_MBUTTONDOWN: if (in) { in->mouse_down[2] = 1; in->mouse_pressed[2] = 1; } return 0;
        case WM_MBUTTONUP: if (in) { in->mouse_down[2] = 0; in->mouse_released[2] = 1; } return 0;
        case WM_MOUSEWHEEL:
            if (in) in->wheel += (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.f;
            return 0;
        case WM_KILLFOCUS:
            if (in) {
                memset(in->down, 0, sizeof in->down);
                memset(in->mouse_down, 0, sizeof in->mouse_down);
            }
            if (g_mouse_captured) {
                g_mouse_captured = 0;
                ClipCursor(NULL);
                ReleaseCapture();
            }
            return 0;
        default: break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int plat_init(Platform *p, const char *title, int w, int h) {
    memset(p, 0, sizeof *p);
    g_plat = p;
    p->width = w;
    p->height = h;
    p->running = 1;
    p->vsync = 0;
    p->windowed_w = w;
    p->windowed_h = h;
    if (!g_audio_lock_init) {
        InitializeCriticalSection(&g_audio_lock);
        g_audio_lock_init = 1;
    }
    QueryPerformanceFrequency(&g_qpc_freq);
    g_qpc_hz = (double)g_qpc_freq.QuadPart > 0.0 ? (double)g_qpc_freq.QuadPart : 1.0;
    QueryPerformanceCounter(&g_qpc_start);

    HINSTANCE inst = GetModuleHandle(NULL);
    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "TheLastNightWnd";
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExA(&wc)) {
        // already registered is fine
    }
    RECT rc = {0, 0, w, h};
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX);
    AdjustWindowRect(&rc, style, FALSE);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, inst, NULL);
    if (!g_hwnd) return 0;
    p->hwnd = g_hwnd;
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    resize_dib(p, w, h);
    // raw mouse input
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0;
    rid.hwndTarget = g_hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof rid);
    // icon and caption
    SetWindowTextA(g_hwnd, TLN_TITLE " - " TLN_SUBTITLE);
    // path setup
    GetModuleFileNameA(NULL, p->exe_dir, sizeof p->exe_dir);
    char *slash = strrchr(p->exe_dir, '\\');
    if (slash) *slash = 0;
    snprintf(p->save_dir, sizeof p->save_dir, "%s", p->exe_dir);
    timeBeginPeriod(1);
    return 1;
}

void plat_shutdown(Platform *p) {
    if (p && p->headless) { g_quit = 1; p->running = 0; return; }
    plat_audio_stop_impl();
    if (g_mouse_captured) ClipCursor(NULL);
    if (g_hdc) DeleteDC(g_hdc);
    if (g_bmp) DeleteObject(g_bmp);
    if (g_pixels) free(g_pixels);
    if (g_hwnd) DestroyWindow(g_hwnd);
    timeEndPeriod(1);
    g_hwnd = NULL;
    g_hdc = NULL;
    g_bmp = NULL;
    g_pixels = NULL;
    g_quit = 1;
    (void)p;
}

double plat_time_impl(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_qpc_start.QuadPart) / g_qpc_hz;
}

int plat_poll(Platform *p) {
    // age edge-triggered state
    memset(p->in.pressed, 0, sizeof p->in.pressed);
    memset(p->in.released, 0, sizeof p->in.released);
    memset(p->in.mouse_pressed, 0, sizeof p->in.mouse_pressed);
    memset(p->in.mouse_released, 0, sizeof p->in.mouse_released);
    p->in.wheel = 0.f;
    p->in.text_len = 0;
    p->in.text[0] = 0;
    if (p->headless) {
        // deterministic 60 Hz tick, exactly like the Linux harness
        static float ht = 0.f;
        ht += 1.f / 60.f;
        p->time = (double)ht;
        p->dt = 1.f / 60.f;
        p->frame_time = 1.f / 60.f;
        p->fps = 60.f;
        p->frame_index++;
        if (p->on_audio) {
            static int16_t abuf[2048 * 2];
            memset(abuf, 0, sizeof abuf);
            p->on_audio(p->audio_user, abuf, 735);
        }
        return !p->quit_requested;
    }
    double t0 = plat_time_impl();
    MSG msg;
    static double last = 0.0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_quit = 1;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (p->in.mouse_dx == 0.f && p->in.mouse_dy == 0.f && g_mouse_captured) {
        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(g_hwnd, &rc);
        int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
        if (pt.x != cx || pt.y != cy) {
            p->in.mouse_dx += (float)(pt.x - cx);
            p->in.mouse_dy += (float)(pt.y - cy);
            SetCursorPos(cx, cy);
        }
    }
    audio_service();
    double now = plat_time_impl();
    p->dt = (float)(now - last);
    if (p->dt > 0.25) p->dt = 0.25f;
    if (p->dt < 0.0001) p->dt = 0.0001f;
    last = now;
    p->time = now;
    p->frame_index++;
    p->fps = (float)(1.0 / p->dt);
    (void)t0;
    return !g_quit && !p->quit_requested;
}

void plat_present(Platform *p, const uint32_t *rgba, int w, int h) {
    if (p && p->headless) { (void)rgba; (void)w; (void)h; return; }
    if (!p || !g_hdc || !g_pixels) return;
    if (w != g_win_w || h != g_win_h) resize_dib(p, w, h);
    // RGBA -> BGRA for the DIB
    uint32_t *dst = g_pixels;
    int n = w * h;
    for (int i = 0; i < n; i++) {
        uint32_t c = rgba[i];
        dst[i] = (c & 0xFF00FF00u) | ((c & 0x00FF0000u) >> 16) | ((c & 0x000000FFu) << 16);
    }
    HDC wdc = GetDC(g_hwnd);
    BitBlt(wdc, 0, 0, g_win_w, g_win_h, g_hdc, 0, 0, SRCCOPY);
    ReleaseDC(g_hwnd, wdc);
}

void plat_sleep_ms(int ms) { Sleep((DWORD)ms); }
double plat_time(void) { return plat_time_impl(); }

void plat_set_title(Platform *p, const char *title) {
    (void)p;
    if (g_hwnd) SetWindowTextA(g_hwnd, title);
}

void plat_toggle_fullscreen(Platform *p) {
    static int full = 0;
    if (!full) {
        GetWindowRect(g_hwnd, &(RECT){0});
        MONITORINFO mi;
        mi.cbSize = sizeof mi;
        if (GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongA(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_FRAMECHANGED);
        }
        full = 1;
        p->fullscreen = 1;
    } else {
        SetWindowLongA(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX) | WS_VISIBLE);
        RECT rc = {0, 0, p->windowed_w, p->windowed_h};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(g_hwnd, HWND_TOP, 60, 60, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED);
        full = 0;
        p->fullscreen = 0;
    }
}

void plat_capture_mouse(Platform *p, int capture) {
    if (capture) {
        if (!g_mouse_captured) {
            ShowCursor(FALSE);
            g_mouse_captured = 1;
            RECT rc;
            GetWindowRect(g_hwnd, &rc);
            SetCursorPos((rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2);
        }
        p->in.mouse_locked = 1;
    } else {
        if (g_mouse_captured) {
            ShowCursor(TRUE);
            g_mouse_captured = 0;
        }
        p->in.mouse_locked = 0;
    }
}

int plat_audio_start(Platform *p, int frames_per_buffer) { return plat_audio_start_impl(p, frames_per_buffer); }
void plat_audio_stop(Platform *p) { (void)p; plat_audio_stop_impl(); }

void plat_log_path(Platform *p, char *out, size_t n, const char *name) {
    snprintf(out, n, "%s/%s", p->save_dir, name);
}

int plat_message_box(const char *title, const char *msg) {
    return MessageBoxA(NULL, msg, title, MB_OK | MB_ICONERROR) == IDOK;
}

#endif  // _WIN32

// ---------------------------------------------------------------------------
// Headless mode for automated runs on Windows. plat_poll/plat_present below
// branch on p->headless so the shipped .exe can run the same --tour harness.
// ---------------------------------------------------------------------------
int plat_init_headless(Platform *p, const char *title, int w, int h) {
    (void)title;
    memset(p, 0, sizeof *p);
    QueryPerformanceFrequency(&g_qpc_freq);
    g_qpc_hz = (double)g_qpc_freq.QuadPart > 0.0 ? (double)g_qpc_freq.QuadPart : 1.0;
    QueryPerformanceCounter(&g_qpc_start);
    p->width = w;
    p->height = h;
    p->headless = 1;
    p->running = 1;
    p->vsync = 0;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    if (n > 0) {
        for (int i = (int)n - 1; i >= 0; i--)
            if (path[i] == '\\' || path[i] == '/') { path[i] = 0; break; }
    } else {
        snprintf(path, sizeof path, ".");
    }
    snprintf(p->exe_dir, sizeof p->exe_dir, "%s", path);
    snprintf(p->save_dir, sizeof p->save_dir, "%s", path);
    return 1;
}
