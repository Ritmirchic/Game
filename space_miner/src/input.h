// ============================================================
// SPACE MINER  -  input.h
// Backend independent input state.
// ============================================================
#pragma once
#include "core.h"

namespace sm {

enum KeyCode {
    K_LEFT = 128, K_RIGHT, K_UP, K_DOWN, K_ESC, K_ENTER, K_SPACE, K_TAB, K_SHIFT, K_CTRL, K_ALT,
    K_BACKSPACE, K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_PLUS, K_MINUS, K_COMMA, K_PERIOD, K_SLASH, K_SEMI, K_APOS, K_LBRACKET, K_RBRACKET,
    K_TILDE, K_HOME, K_END, K_PAGEUP, K_PAGEDOWN, K_DELETE, K_COUNT = 256
};

inline int keyOf(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    return (int)(unsigned char)c;
}

struct Input {
    bool down[K_COUNT] = {};
    bool pressed[K_COUNT] = {};
    bool released[K_COUNT] = {};
    float mouseX = -1, mouseY = -1;     // in virtual canvas coords
    float mouseDX = 0, mouseDY = 0;
    bool mouseDown[3] = {};
    bool mousePressed[3] = {};
    bool mouseReleased[3] = {};
    float wheel = 0;
    float time = 0;

    void beginFrame() {
        std::memset(pressed, 0, sizeof(pressed));
        std::memset(released, 0, sizeof(released));
        std::memset(mousePressed, 0, sizeof(mousePressed));
        std::memset(mouseReleased, 0, sizeof(mouseReleased));
        mouseDX = mouseDY = 0;
        wheel = 0;
    }
    bool isDown(int k) const { return k >= 0 && k < K_COUNT && down[k]; }
    bool wasPressed(int k) const { return k >= 0 && k < K_COUNT && pressed[k]; }
    bool wasReleased(int k) const { return k >= 0 && k < K_COUNT && released[k]; }
    bool anyKeyPressed() const {
        for (int i = 0; i < K_COUNT; i++) if (pressed[i]) return true;
        return false;
    }
    // helpers
    bool left()   const { return isDown(keyOf('A')) || isDown(K_LEFT); }
    bool right()  const { return isDown(keyOf('D')) || isDown(K_RIGHT); }
    bool up()     const { return isDown(keyOf('W')) || isDown(K_UP); }
    bool downKey() const { return isDown(keyOf('S')) || isDown(K_DOWN); }
    bool mouseIn(int x, int y, int w, int h) const { return mouseX >= x && mouseX < x + w && mouseY >= y && mouseY < y + h; }
};

}  // namespace sm
