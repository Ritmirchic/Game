// FRACTURE — input state (shared by platform layers)
#pragma once
#include "common.h"

namespace ft {

enum Key : int {
    KEY_NONE = 0,
    KEY_W = 1, KEY_A, KEY_S, KEY_D, KEY_Q, KEY_E, KEY_R, KEY_F, KEY_G, KEY_H, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B,
    KEY_SPACE, KEY_SHIFT, KEY_CTRL, KEY_TAB, KEY_ESC, KEY_ENTER, KEY_BACKSPACE,
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F11,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_LMB, KEY_RMB, KEY_MMB,
    KEY_COUNT
};

struct InputState {
    bool down[KEY_COUNT] = {};
    bool pressed[KEY_COUNT] = {};     // edge: went down this frame
    bool released[KEY_COUNT] = {};
    f32  mx = 0, my = 0;              // relative mouse delta (pixels)
    f32  wheel = 0;                   // wheel ticks this frame (+/-)
    f32  cursorX = 0, cursorY = 0;    // absolute cursor (menus)
    bool quitRequested = false;
    bool mouseCaptured = false;

    void beginFrame() {
        std::memset(pressed, 0, sizeof(pressed));
        std::memset(released, 0, sizeof(released));
        mx = my = 0;
        wheel = 0;
    }
    void key(int k, bool isDown) {
        if (k <= 0 || k >= KEY_COUNT) return;
        if (isDown && !down[k]) pressed[k] = true;
        if (!isDown && down[k]) released[k] = true;
        down[k] = isDown;
    }
};

} // namespace ft
