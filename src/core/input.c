/* SPACE MINER — core/input.c : shared input bookkeeping */
#include "platform.h"

int sm_key_index(int key) {
    if (key >= 0 && key < SM_KEY_COUNT) return key;
    return -1;
}

void input_clear(Input *in) { memset(in, 0, sizeof(*in)); }

void input_begin_frame(Input *in) {
    memset(in->pressed, 0, sizeof(in->pressed));
    memset(in->released, 0, sizeof(in->released));
    memset(in->mouse_pressed, 0, sizeof(in->mouse_pressed));
    memset(in->mouse_released, 0, sizeof(in->mouse_released));
    in->mouse_delta = v2(0, 0);
    in->wheel = 0.f;
    in->text_len = 0;
}

void input_end_frame(Input *in) {
    /* keep `down` state; deltas are cleared at the next begin_frame */
    (void)in;
}

void input_key_event(Input *in, int key, int down) {
    int k = sm_key_index(key);
    if (k < 0) return;
    if (down) {
        if (!in->down[k]) in->pressed[k] = 1;
        in->down[k] = 1;
        if (key >= 32 && key < 127 && in->text_len < (int)sizeof(in->text)) in->text[in->text_len++] = (u8)key;
    } else {
        if (in->down[k]) in->released[k] = 1;
        in->down[k] = 0;
    }
}

void input_mouse_move(Input *in, f32 x, f32 y) {
    in->mouse_delta = v2add(in->mouse_delta, v2sub(v2(x, y), in->mouse));
    in->mouse = v2(x, y);
}

void input_mouse_button(Input *in, int button, int down) {
    if (button < 0 || button > 2) return;
    if (down) {
        if (!in->mouse_down[button]) in->mouse_pressed[button] = 1;
        in->mouse_down[button] = 1;
    } else {
        if (in->mouse_down[button]) in->mouse_released[button] = 1;
        in->mouse_down[button] = 0;
    }
}
