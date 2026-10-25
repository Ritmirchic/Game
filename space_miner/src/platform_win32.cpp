// ============================================================
// SPACE MINER  -  platform_win32.cpp
// Native Win32 host: window, input, GDI presentation, waveOut audio.
// ============================================================
#ifdef _WIN32
#include "platform.h"
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <cstdio>
#include <string>
#include <atomic>

namespace sm {

static HWND gHwnd = nullptr;
static HDC gDC = nullptr;
static HDC gMemDC = nullptr;
static HBITMAP gBmp = nullptr;
static std::vector<uint32_t> gPresentBuf;
static int gWinW = 1280, gWinH = 720;
static float gScale = 1.f;
static int gOffX = 0, gOffY = 0;
static bool gRunning = true;
static LARGE_INTEGER gFreq, gPerfNow, gPerfBase;
static bool gKeys[256];
static bool gMouseBtn[3];
static double gPrevTime = 0;
static int gWheelAccum = 0;
static const double kTargetFrame = 1.0 / 60.0;
static std::string gSaveDir;
static bool gBorderless = false;
static RECT gWindowedRect;
static DWORD gWindowedStyle = 0;

// ---------------------------------------------------------------- audio
#define AUDIO_CHANNELS 2
#define AUDIO_FRAMES 1024
#define AUDIO_BUFFERS 4
static HWAVEOUT gWave = nullptr;
static WAVEHDR gHdr[AUDIO_BUFFERS];
static int16_t gAudioBuf[AUDIO_BUFFERS][AUDIO_FRAMES * AUDIO_CHANNELS];
static std::atomic<bool> gAudioOn{false};
static CRITICAL_SECTION gAudioLock;

static void CALLBACK waveCallback(HWAVEOUT, UINT msg, DWORD_PTR user, DWORD_PTR, DWORD_PTR) {
    if (msg != WOM_DONE) return;
    int idx = (int)user;
    EnterCriticalSection(&gAudioLock);
    WAVEHDR* h = &gHdr[idx];
    if (h->dwUser == 0) { LeaveCriticalSection(&gAudioLock); return; }
    float tmp[AUDIO_FRAMES * AUDIO_CHANNELS];
    audio::render(tmp, AUDIO_FRAMES, AUDIO_CHANNELS);  // internally locked
    for (int i = 0; i < AUDIO_FRAMES * AUDIO_CHANNELS; i++) {
        float v = tmp[i];
        if (v > 1.f) v = 1.f;
        if (v < -1.f) v = -1.f;
        gAudioBuf[idx][i] = (int16_t)(v * 32000.f);
    }
    h->lpData = (LPSTR)gAudioBuf[idx];
    h->dwBufferLength = AUDIO_FRAMES * AUDIO_CHANNELS * sizeof(int16_t);
    LeaveCriticalSection(&gAudioLock);
    waveOutWrite(gWave, h, sizeof(WAVEHDR));
}

static void initAudio() {
    InitializeCriticalSection(&gAudioLock);
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = AUDIO_CHANNELS;
    wf.nSamplesPerSec = 44100;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = AUDIO_CHANNELS * 2;
    wf.nAvgBytesPerSec = 44100 * wf.nBlockAlign;
    MMRESULT r = waveOutOpen(&gWave, WAVE_MAPPER, &wf, (DWORD_PTR)waveCallback, 0, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) { gAudioOn = false; return; }
    for (int i = 0; i < AUDIO_BUFFERS; i++) {
        ZeroMemory(&gHdr[i], sizeof(WAVEHDR));
        gHdr[i].lpData = (LPSTR)gAudioBuf[i];
        gHdr[i].dwBufferLength = AUDIO_FRAMES * AUDIO_CHANNELS * sizeof(int16_t);
        gHdr[i].dwUser = 1;
        waveOutPrepareHeader(gWave, &gHdr[i], sizeof(WAVEHDR));
        waveOutWrite(gWave, &gHdr[i], sizeof(WAVEHDR));
    }
    gAudioOn = true;
}

static void shutdownAudio() {
    if (!gWave) return;
    gAudioOn = false;
    waveOutReset(gWave);
    for (int i = 0; i < AUDIO_BUFFERS; i++) {
        gHdr[i].dwUser = 0;
        waveOutUnprepareHeader(gWave, &gHdr[i], sizeof(WAVEHDR));
    }
    waveOutClose(gWave);
    gWave = nullptr;
}

// ---------------------------------------------------------------- window
static void computeScale() {
    RECT rc;
    GetClientRect(gHwnd, &rc);
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    float sx = cw / (float)VW, sy = ch / (float)VH;
    gScale = std::min(sx, sy);
    gOffX = (int)((cw - VW * gScale) * .5f);
    gOffY = (int)((ch - VH * gScale) * .5f);
}

void platformSetBorderless(bool on) {
    if (!gHwnd || on == gBorderless) return;
    gBorderless = on;
    if (on) {
        GetWindowRect(gHwnd, &gWindowedRect);
        gWindowedStyle = (DWORD)GetWindowLongPtr(gHwnd, GWL_STYLE);
        SetWindowLongPtr(gHwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        ShowWindow(gHwnd, SW_MAXIMIZE);
    } else {
        SetWindowLongPtr(gHwnd, GWL_STYLE, gWindowedStyle);
        SetWindowPos(gHwnd, nullptr, gWindowedRect.left, gWindowedRect.top,
                     gWindowedRect.right - gWindowedRect.left, gWindowedRect.bottom - gWindowedRect.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowWindow(gHwnd, SW_SHOW);
    }
    computeScale();
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
            gRunning = false;
            return 0;
        case WM_DESTROY:
            gRunning = false;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            computeScale();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            // let Alt+F4 reach the default handler
            if (msg == WM_SYSKEYDOWN && wp == VK_F4 && (GetKeyState(VK_MENU) & 0x8000)) break;
            int code = -1;
            switch (wp) {
                case VK_LEFT: code = K_LEFT; break;
                case VK_RIGHT: code = K_RIGHT; break;
                case VK_UP: code = K_UP; break;
                case VK_DOWN: code = K_DOWN; break;
                case VK_ESCAPE: code = K_ESC; break;
                case VK_RETURN: code = K_ENTER; break;
                case VK_SPACE: code = K_SPACE; break;
                case VK_TAB: code = K_TAB; break;
                case VK_SHIFT: code = K_SHIFT; break;
                case VK_CONTROL: code = K_CTRL; break;
                case VK_MENU: code = K_ALT; break;
                case VK_BACK: code = K_BACKSPACE; break;
                case VK_DELETE: code = K_DELETE; break;
                case VK_F1: code = K_F1; break;
                case VK_F2: code = K_F2; break;
                case VK_F11: code = K_F11; break;
                case VK_OEM_PLUS: code = K_PLUS; break;
                case VK_OEM_MINUS: code = K_MINUS; break;
                case VK_OEM_3: code = K_TILDE; break;
                case VK_OEM_4: code = K_LBRACKET; break;
                case VK_OEM_6: code = K_RBRACKET; break;
                default:
                    if ((wp >= 'A' && wp <= 'Z') || (wp >= '0' && wp <= '9')) code = (int)wp;
                    break;
            }
            if (code >= 0 && code < K_COUNT) gKeys[code] = true;
            if (wp == VK_F11) platformSetBorderless(!gBorderless);
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            int code = -1;
            switch (wp) {
                case VK_LEFT: code = K_LEFT; break;
                case VK_RIGHT: code = K_RIGHT; break;
                case VK_UP: code = K_UP; break;
                case VK_DOWN: code = K_DOWN; break;
                case VK_ESCAPE: code = K_ESC; break;
                case VK_RETURN: code = K_ENTER; break;
                case VK_SPACE: code = K_SPACE; break;
                case VK_TAB: code = K_TAB; break;
                case VK_SHIFT: code = K_SHIFT; break;
                case VK_CONTROL: code = K_CTRL; break;
                case VK_MENU: code = K_ALT; break;
                case VK_BACK: code = K_BACKSPACE; break;
                case VK_F1: code = K_F1; break;
                case VK_F2: code = K_F2; break;
                case VK_F11: code = K_F11; break;
                case VK_OEM_PLUS: code = K_PLUS; break;
                case VK_OEM_MINUS: code = K_MINUS; break;
                case VK_OEM_3: code = K_TILDE; break;
                case VK_OEM_4: code = K_LBRACKET; break;
                case VK_OEM_6: code = K_RBRACKET; break;
                default:
                    if ((wp >= 'A' && wp <= 'Z') || (wp >= '0' && wp <= '9')) code = (int)wp;
                    break;
            }
            if (code >= 0 && code < K_COUNT) gKeys[code] = false;
            return 0;
        }
        case WM_KILLFOCUS:
            ZeroMemory(gKeys, sizeof(gKeys));
            gMouseBtn[0] = gMouseBtn[1] = gMouseBtn[2] = false;
            return 0;
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            return 0;
        }
        case WM_LBUTTONDOWN: gMouseBtn[0] = true; SetCapture(hwnd); return 0;
        case WM_LBUTTONUP: gMouseBtn[0] = false; ReleaseCapture(); return 0;
        case WM_RBUTTONDOWN: gMouseBtn[1] = true; return 0;
        case WM_RBUTTONUP: gMouseBtn[1] = false; return 0;
        case WM_MBUTTONDOWN: gMouseBtn[2] = true; return 0;
        case WM_MBUTTONUP: gMouseBtn[2] = false; return 0;
        case WM_MOUSEWHEEL:
            gWheelAccum += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) { SetCursor(nullptr); return TRUE; }
            break;
        default: break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

bool platformInit(const char* title, int w, int h) {
    QueryPerformanceFrequency(&gFreq);
    QueryPerformanceCounter(&gPerfBase);
    SetProcessDPIAware();
    HINSTANCE inst = GetModuleHandle(nullptr);
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = nullptr;
    wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wc.lpszClassName = "SpaceMinerWnd";
    if (!RegisterClassExA(&wc)) return false;
    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;
    gHwnd = CreateWindowExA(0, "SpaceMinerWnd", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                            std::max(0, sx), std::max(0, sy), ww, wh, nullptr, nullptr, inst, nullptr);
    if (!gHwnd) return false;
    gDC = GetDC(gHwnd);
    gMemDC = CreateCompatibleDC(gDC);
    computeScale();
    // save directory lives in the roaming profile folder
    char appdataDir[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("APPDATA", appdataDir, MAX_PATH) > 0)
        gSaveDir = std::string(appdataDir) + "\\SpaceMiner\\";
    else
        gSaveDir = ".\\";
    CreateDirectoryA(gSaveDir.c_str(), nullptr);
    initAudio();
    ShowWindow(gHwnd, SW_SHOW);
    UpdateWindow(gHwnd);
    return true;
}

void platformShutdown() {
    shutdownAudio();
    gAudioOn = false;
    DeleteCriticalSection(&gAudioLock);
    if (gBmp) DeleteObject(gBmp);
    if (gMemDC) DeleteDC(gMemDC);
    if (gDC) ReleaseDC(gHwnd, gDC);
    if (gHwnd) DestroyWindow(gHwnd);
}

bool platformPump(Input& in, float& dt) {
    MSG msg;
    in.beginFrame();
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) gRunning = false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (!gRunning) return false;
    // keys
    for (int k = 0; k < K_COUNT; k++) {
        bool now = gKeys[k];
        if (now && !in.down[k]) in.pressed[k] = true;
        if (!now && in.down[k]) in.released[k] = true;
        in.down[k] = now;
    }
    // mouse position -> virtual canvas
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(gHwnd, &p);
    in.mouseX = (p.x - gOffX) / gScale;
    in.mouseY = (p.y - gOffY) / gScale;
    for (int b = 0; b < 3; b++) {
        bool now = gMouseBtn[b];
        if (now && !in.mouseDown[b]) in.mousePressed[b] = true;
        if (!now && in.mouseDown[b]) in.mouseReleased[b] = true;
        in.mouseDown[b] = now;
    }
    in.wheel = (float)gWheelAccum;
    gWheelAccum = 0;
    // frame pacing: never spin faster than 60 Hz (the canvas is software rendered)
    QueryPerformanceCounter(&gPerfNow);
    double now = (double)gPerfNow.QuadPart / (double)gFreq.QuadPart;
    if (gPrevTime > 0) {
        double elapsed = now - gPrevTime;
        if (elapsed < kTargetFrame) {
            double remain = kTargetFrame - elapsed;
            if (remain > .0025) Sleep((DWORD)((remain - .001) * 1000.0));
            do {
                QueryPerformanceCounter(&gPerfNow);
                now = (double)gPerfNow.QuadPart / (double)gFreq.QuadPart;
            } while (now - gPrevTime < kTargetFrame);
        }
    }
    dt = (float)(now - gPrevTime);
    if (gPrevTime <= 0) dt = (float)kTargetFrame;
    gPrevTime = now;
    if (dt > .1f) dt = .1f;
    if (dt < 0) dt = 0;
    return true;
}

void platformPresent(Canvas& fb) {
    if (!gHwnd) return;
    if (fb.w != VW || fb.h != VH) return;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = VW;
    bi.bmiHeader.biHeight = -VH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(gDC, COLORONCOLOR);
    StretchDIBits(gDC, gOffX, gOffY, (int)(VW * gScale), (int)(VH * gScale), 0, 0, VW, VH,
                  fb.px.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    // letterbox
    RECT rc;
    GetClientRect(gHwnd, &rc);
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (gOffY > 0) {
        RECT top = {0, 0, rc.right, gOffY};
        RECT bot = {0, rc.bottom - gOffY, rc.right, rc.bottom};
        FillRect(gDC, &top, black);
        FillRect(gDC, &bot, black);
    }
    if (gOffX > 0) {
        RECT left = {0, 0, gOffX, rc.bottom};
        RECT right = {rc.right - gOffX, 0, rc.right, rc.bottom};
        FillRect(gDC, &left, black);
        FillRect(gDC, &right, black);
    }
}

void platformSetTitle(const char* title) {
    if (gHwnd) SetWindowTextA(gHwnd, title);
}

void platformSetIcon(int size, const uint32_t* argb) {
    if (!gHwnd || size <= 0) return;
    // build a 32bpp DIB section from ARGB, then HICON
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!color) { ReleaseDC(nullptr, dc); return; }
    if (bits) memcpy(bits, argb, (size_t)size * size * 4);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    HICON icon = CreateIconIndirect(&ii);
    if (icon) {
        SendMessage(gHwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessage(gHwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }
    // also the "alt-tab" small variant is handled by the system scaling
    DeleteObject(color);
    DeleteObject(mask);
    ReleaseDC(nullptr, dc);
}

const char* platformSaveDir() { return gSaveDir.c_str(); }

void platformSleepMs(int ms) { Sleep(ms); }

double platformSeconds() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - gPerfBase.QuadPart) / (double)gFreq.QuadPart;
}

}  // namespace sm
#endif  // _WIN32
