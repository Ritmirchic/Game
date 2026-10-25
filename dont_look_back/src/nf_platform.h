// ============================================================================
//  nf_platform.h : window, input, timing (Win32 + headless)
// ============================================================================
#pragma once
#include "nf_core.h"

namespace nf {

enum Key {
    KC_NONE = 0,
    KC_A = 65, KC_B, KC_C, KC_D, KC_E, KC_F, KC_G, KC_H, KC_I, KC_J, KC_K, KC_L, KC_M,
    KC_N, KC_O, KC_P, KC_Q, KC_R, KC_S, KC_T, KC_U, KC_V, KC_W, KC_X, KC_Y, KC_Z,
    KC_0 = 48, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9,
    KC_ESC = 256, KC_SHIFT, KC_CTRL, KC_ALT, KC_SPACE, KC_TAB, KC_ENTER, KC_BACK,
    KC_UP, KC_DOWN, KC_LEFT, KC_RIGHT, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6,
    KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, KC_DELETE, KC_COMMA, KC_PERIOD,
    KC_MINUS, KC_EQUALS, KC_COUNT = 320
};

struct Input {
    bool down[KC_COUNT] = {false};
    bool pressed[KC_COUNT] = {false};
    bool released[KC_COUNT] = {false};
    int  mouseX = 0, mouseY = 0;
    float mouseDX = 0, mouseDY = 0;
    float wheel = 0;
    bool  mouse[3] = {false, false, false};
    bool  mousePressed[3] = {false, false, false};
    bool  quit = false;
    bool  focus = true;
    bool  captured = false;

    bool isDown(Key k) const { return down[k]; }
    bool wasPressed(Key k) const { return pressed[k]; }
    bool wasReleased(Key k) const { return released[k]; }
    void newFrame() {
        memset(pressed, 0, sizeof(pressed));
        memset(released, 0, sizeof(released));
        memset(mousePressed, 0, sizeof(mousePressed));
        mouseDX = mouseDY = 0;
        wheel = 0;
    }
    // headless injection
    void injectPress(Key k) { down[k] = true; pressed[k] = true; }
    void injectRelease(Key k) { down[k] = false; released[k] = true; }
};

struct WindowOptions {
    int width = 1600;
    int height = 900;
    std::string title = "DON'T LOOK BACK";
    bool fullscreen = false;
    bool resizable = true;
    int iconResourceId = 0;
};

class Platform {
public:
    bool createWindow(const WindowOptions& opt);
    void destroyWindow();
    bool pumpEvents(Input& in);
    void setTitle(const std::string& t);
    void setCursorCaptured(bool captured);
    void setFullscreen(bool full);
    bool fullscreen() const { return fullscreen_; }
    void toggleFullscreen();
    void getSize(int& w, int& h) const { w = w_; h = h_; }
    bool resized() { bool r = resized_; resized_ = false; return r; }
    void* nativeHandle() const { return hwnd_; }
    void showMessage(const std::string& title, const std::string& msg);
    void sleepMs(int ms);
    // present a BGRA framebuffer (top-down rows)
    void present(const Color* pixels, int w, int h);
    void setWindowIconFromImage(const Image& img);
    bool headless = false;
    void setHeadlessSize(int w, int h) { w_ = w; h_ = h; }

private:
    void* hwnd_ = nullptr;
    int w_ = 640, h_ = 640;
    bool resized_ = true;
    bool fullscreen_ = false;
    bool captured_ = false;
    bool cursorVisible_ = true;
    int winX_ = 0, winY_ = 0, winW_ = 1600, winH_ = 900;
    std::vector<Color> presentBuf_;
    void* bmp_ = nullptr;
    void* dcMem_ = nullptr;
};

// total RAM available in MB (best effort)
int systemMemoryMB();

} // namespace nf
