/* ============================================================================
   SPACE MINER — platform/win32/win_main.c
   Win32 window, OpenGL 1.1 present path, input translation, main loop.
   ========================================================================== */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <mmsystem.h>

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#include "../../core/platform.h"
#include "../../core/state.h"
#include "../../core/audio.h"

/* Provided by the generated asset blob (see tools/build_win.sh). */
extern const unsigned char sm_assets_pak[];
extern const unsigned long sm_assets_pak_len;

void waudio_init(void);
void waudio_shutdown(void);
void waudio_set_settings(f32 master, f32 music, f32 sfx, int muted);
int  waudio_ok(void);

static HWND g_hwnd;
static HDC g_hdc;
static HGLRC g_glrc;
static int g_running = 1;
static int g_fullscreen = 0;
static RECT g_windowed_rect;
static GLuint g_tex;
static int g_win_w = 1280, g_win_h = 720;
static Input g_input;
static Game *g_game;
static u8 *g_assets = NULL;
static usize g_assets_len = 0;
static int g_vsync = 0;
static char g_save_prefix[MAX_PATH];
static f64 g_qpc_freq = 1.0;
static LARGE_INTEGER g_qpc_start;
static int g_minimized = 0;

typedef void (APIENTRY *PFN_wglSwapIntervalEXT)(int);
static PFN_wglSwapIntervalEXT p_wglSwapIntervalEXT = NULL;

/* ------------------------------------------------------------- file I/O --- */
static void win_log(const char *msg, void *user) {
    (void)user;
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

static void path_for(char *out, usize cap, const char *name) {
    sm_sprintf(out, cap, "%s%s", g_save_prefix, name);
}

static void win_read_file(const char *name, u8 **outData, usize *outLen, void *user) {
    (void)user;
    char path[MAX_PATH * 2];
    path_for(path, sizeof(path), name);
    FILE *f = fopen(path, "rb");
    if (!f) { *outData = NULL; if (outLen) *outLen = 0; return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); *outData = NULL; if (outLen) *outLen = 0; return; }
    u8 *buf = (u8 *)malloc((usize)sz);
    size_t got = fread(buf, 1, (usize)sz, f);
    fclose(f);
    *outData = buf;
    if (outLen) *outLen = got;
}

static int win_write_file(const char *name, const void *data, usize len, void *user) {
    (void)user;
    char path[MAX_PATH * 2];
    path_for(path, sizeof(path), name);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t put = fwrite(data, 1, len, f);
    fclose(f);
    return put == len ? 0 : -1;
}

static int win_file_exists(const char *name, void *user) {
    (void)user;
    char path[MAX_PATH * 2];
    path_for(path, sizeof(path), name);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static double win_now(void *user) {
    (void)user;
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - g_qpc_start.QuadPart) / g_qpc_freq;
}

static void win_sleep(int ms, void *user) { (void)user; Sleep((DWORD)ms); }

/* Write a PNG next to the executable. */
static void write_png_file(const u32 *px, int w, int h, const char *name);

static void win_screenshot(const u32 *px, int w, int h, void *user) {
    (void)user;
    char path[MAX_PATH];
    char stamp[64];
    SYSTEMTIME st;
    GetLocalTime(&st);
    sm_sprintf(stamp, sizeof(stamp), "screenshot_%04d%02d%02d_%02d%02d%02d.png",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    sm_sprintf(path, sizeof(path), "%s%s", g_save_prefix, stamp);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fclose(f);
    write_png_file(px, w, h, path);
}

/* --------------------------------------------------------------- GL ------- */
static void gl_setup_projection(int vw, int vh) {
    glViewport(0, 0, vw, vh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static int gl_create_texture(void) {
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SM_SCREEN_W, SM_SCREEN_H, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
    return g_tex != 0;
}

static void gl_present(const u32 *px, int w, int h) {
    static u32 *scratch = NULL;
    if (!scratch) scratch = (u32 *)malloc((usize)SM_SCREEN_W * SM_SCREEN_H * 4);
    if (!scratch) return;
    /* convert 0xAARRGGBB -> BGRA little endian bytes (identical layout) */
    memcpy(scratch, px, (usize)w * (usize)h * 4);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, scratch);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(1, 0);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(0, 1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    SwapBuffers(g_hdc);
}

/* ------------------------------------------------------------ window ------ */
static int g_style_saved = 0;
static DWORD g_saved_style;
static RECT g_saved_rect;

static void toggle_fullscreen(void) {
    if (!g_fullscreen) {
        GetWindowRect(g_hwnd, &g_windowed_rect);
        if (!g_style_saved) {
            g_saved_style = (DWORD)GetWindowLongA(g_hwnd, GWL_STYLE);
            GetWindowRect(g_hwnd, &g_saved_rect);
            g_style_saved = 1;
        }
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLongA(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = 1;
    } else {
        if (g_style_saved) SetWindowLongA(g_hwnd, GWL_STYLE, (LONG)g_saved_style);
        SetWindowPos(g_hwnd, HWND_TOP, g_saved_rect.left, g_saved_rect.top,
                     g_saved_rect.right - g_saved_rect.left,
                     g_saved_rect.bottom - g_saved_rect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = 0;
    }
}

static void client_to_virtual(int mx, int my, f32 *vx, f32 *vy) {
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) { *vx = *vy = 0; return; }
    f32 scale = SM_MIN((f32)cw / SM_SCREEN_W, (f32)ch / SM_SCREEN_H);
    f32 vw = SM_SCREEN_W * scale, vh = SM_SCREEN_H * scale;
    f32 ox = (cw - vw) * 0.5f, oy = (ch - vh) * 0.5f;
    *vx = ((f32)mx - ox) / scale;
    *vy = ((f32)my - oy) / scale;
}

static int translate_vk(WPARAM vk) {
    switch (vk) {
        case VK_ESCAPE: return SM_KEY_ESCAPE;
        case VK_SPACE: return SM_KEY_SPACE;
        case VK_LEFT: return SM_KEY_LEFT;
        case VK_RIGHT: return SM_KEY_RIGHT;
        case VK_UP: return SM_KEY_UP;
        case VK_DOWN: return SM_KEY_DOWN;
        case VK_TAB: return SM_KEY_TAB;
        case VK_RETURN: return SM_KEY_ENTER;
        case VK_BACK: return SM_KEY_BACKSPACE;
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return SM_KEY_SHIFT;
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return SM_KEY_CTRL;
        case VK_F1: return SM_KEY_F1;
        case VK_F2: return SM_KEY_F2;
        case VK_F3: return SM_KEY_F3;
        case VK_F4: return SM_KEY_F4;
        case VK_F5: return SM_KEY_F5;
        case VK_F6: return SM_KEY_F6;
        case VK_F11: return SM_KEY_F11;
        case VK_F12: return SM_KEY_F12;
        default: break;
    }
    if (vk >= 'A' && vk <= 'Z') return SM_KEY_A + (int)(vk - 'A');
    if (vk >= '0' && vk <= '9') return SM_KEY_0 + (int)(vk - '0');
    if (vk == VK_OEM_MINUS) return '-';
    if (vk == VK_OEM_PLUS) return '=';
    if (vk == VK_OEM_COMMA) return ',';
    if (vk == VK_OEM_PERIOD) return '.';
    if (vk == VK_OEM_2) return '/';
    return 0;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            g_running = 0;
            return 0;
        case WM_SIZE:
            if (wp == SIZE_MINIMIZED) g_minimized = 1;
            else {
                g_minimized = 0;
                g_win_w = LOWORD(lp);
                g_win_h = HIWORD(lp);
            }
            return 0;
        case WM_ACTIVATEAPP:
            return 0;
        case WM_SYSKEYDOWN:
            if (wp == VK_F4) { g_running = 0; return 0; }
            break;
        case WM_KEYDOWN:
            if (wp == VK_F11) { toggle_fullscreen(); return 0; }
            if (!(lp & (1 << 30))) input_key_event(&g_input, translate_vk(wp), 1);
            return 0;
        case WM_KEYUP:
            input_key_event(&g_input, translate_vk(wp), 0);
            return 0;
        case WM_CHAR:
            if (wp >= 32 && wp < 127) {
                if (g_input.text_len < (int)sizeof(g_input.text))
                    g_input.text[g_input.text_len++] = (u8)wp;
            }
            return 0;
        case WM_MOUSEMOVE: {
            f32 vx, vy;
            client_to_virtual(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &vx, &vy);
            input_mouse_move(&g_input, vx, vy);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            f32 vx, vy;
            client_to_virtual(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &vx, &vy);
            input_mouse_move(&g_input, vx, vy);
            input_mouse_button(&g_input, 0, 1);
            SetCapture(hwnd);
            return 0;
        }
        case WM_LBUTTONUP:
            input_mouse_button(&g_input, 0, 0);
            ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            input_mouse_button(&g_input, 1, 1);
            SetCapture(hwnd);
            return 0;
        case WM_RBUTTONUP:
            input_mouse_button(&g_input, 1, 0);
            ReleaseCapture();
            return 0;
        case WM_MBUTTONDOWN:
            input_mouse_button(&g_input, 2, 1);
            return 0;
        case WM_MBUTTONUP:
            input_mouse_button(&g_input, 2, 0);
            return 0;
        case WM_MOUSEWHEEL:
            g_input.wheel += (f32)GET_WHEEL_DELTA_WPARAM(wp) / 120.f;
            return 0;
    }
    if (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_KEYMENU) return 0;
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------------------------------------------------------------- main ---- */
static int create_window(HINSTANCE hinst) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "SpaceMinerWnd";
    wc.hIcon = LoadIconA(hinst, "APPICON");
    if (!wc.hIcon) wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    if (!RegisterClassA(&wc)) return 0;

    RECT rc = {0, 0, SM_SCREEN_W, SM_SCREEN_H};
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    AdjustWindowRect(&rc, style, FALSE);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "SPACE MINER", style,
                             CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                             NULL, NULL, hinst, NULL);
    if (!g_hwnd) return 0;
    SetWindowTextA(g_hwnd, "SPACE MINER  -  Deep Field Extraction");
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    return 1;
}

static int init_gl(void) {
    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf) return 0;
    if (!SetPixelFormat(g_hdc, pf, &pfd)) return 0;
    g_glrc = wglCreateContext(g_hdc);
    if (!g_glrc) return 0;
    if (!wglMakeCurrent(g_hdc, g_glrc)) return 0;
    p_wglSwapIntervalEXT = (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(1);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return gl_create_texture();
}

/* PNG writer helper (uses core/png.c) */
#include "../../core/png.h"
static void write_png_file(const u32 *px, int w, int h, const char *name) {
    usize len = 0;
    u8 *data = png_encode(px, w, h, &len);
    if (!data) return;
    FILE *f = fopen(name, "wb");
    if (f) { fwrite(data, 1, len, f); fclose(f); }
    free(data);
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show) {
    (void)hprev; (void)show;
    SetProcessDPIAware();
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpc_freq = (double)f.QuadPart;
    QueryPerformanceCounter(&g_qpc_start);
    (void)cmdline;

    /* save dir: %APPDATA%/SpaceMiner/ */
    const char *appdata = getenv("APPDATA");
    if (appdata && appdata[0]) {
        sm_sprintf(g_save_prefix, MAX_PATH, "%s\\SpaceMiner\\", appdata);
        CreateDirectoryA(g_save_prefix, NULL);
    } else {
        g_save_prefix[0] = 0;
    }

    if (!create_window(hinst)) return 1;
    if (!init_gl()) {
        MessageBoxA(NULL, "SPACE MINER requires OpenGL 1.1 or newer.", "SPACE MINER", MB_ICONERROR);
        return 1;
    }

    g_assets = (u8 *)sm_assets_pak;
    g_assets_len = (usize)sm_assets_pak_len;

    SmPlatform plat;
    memset(&plat, 0, sizeof(plat));
    plat.read_file = win_read_file;
    plat.write_file = win_write_file;
    plat.file_exists = win_file_exists;
    plat.log = win_log;
    plat.now = win_now;
    plat.sleep_ms = win_sleep;
    plat.save_screenshot = win_screenshot;
    plat.save_dir = g_save_prefix;
    sm_log_set(win_log, NULL);

    Gfx gfx;
    gfx_init(&gfx, SM_SCREEN_W, SM_SCREEN_H);

    Pak pak;
    if (pak_open(&pak, g_assets, g_assets_len) != 0) {
        MessageBoxA(NULL, "SPACE MINER: asset pack is missing or corrupted.\nPlease reinstall the game.",
                    "SPACE MINER", MB_ICONERROR);
        return 1;
    }
    audio_init();
    audio_load_all(&pak);
    waudio_init();
    if (!waudio_ok()) sm_warnf("audio device unavailable; running silent");

    g_game = game_create(&plat, &pak, &gfx);
    if (!g_game) {
        MessageBoxA(NULL, "SPACE MINER failed to initialise.", "SPACE MINER", MB_ICONERROR);
        return 1;
    }

    LARGE_INTEGER prev;
    QueryPerformanceCounter(&prev);
    MSG msg;
    while (g_running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_running) break;
        if (g_minimized) { Sleep(30); continue; }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        f32 dt = (f32)((double)(now.QuadPart - prev.QuadPart) / g_qpc_freq);
        prev = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt <= 0.f) dt = 1.f / 60.f;

        /* audio gains from settings */
        waudio_set_settings(g_game->save.audio.master, g_game->save.audio.music,
                            g_game->save.audio.sfx, g_game->save.audio.muted);

        input_begin_frame(&g_input);
        game_update(g_game, &g_input, dt);
        game_render(g_game);
        gl_present(gfx.px, gfx.w, gfx.h);
        input_end_frame(&g_input);
        if (game_wants_quit(g_game)) g_running = 0;
    }

    game_destroy(g_game);
    waudio_shutdown();
    if (g_glrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_glrc); }
    if (g_hdc) ReleaseDC(g_hwnd, g_hdc);
    if (g_hwnd) DestroyWindow(g_hwnd);
    return 0;
}

const u8 *plat_assets(usize *outLen) { if (outLen) *outLen = g_assets_len; return g_assets; }
