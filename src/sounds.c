// Sound design: all audio is synthesized procedurally at load time (no
// third-party samples are used anywhere in this project).
#include "world.h"
#include <string.h>

static Sound *mk(const char *name, float dur) {
    return snd_make(name, (int)(AUD_RATE * dur));
}

#define GEN(fn, snd, secs, name, ...) do {        \
        (snd) = mk((name), (secs));                   \
        if (snd) { fn((snd), ##__VA_ARGS__); }          \
    } while (0)

void sounds_build(Assets *a) {
    for (int i = 0; i < 3; i++) GEN(snd_gen_shot, a->snd_shot_pistol[i], 0.55f, "pistol", 780.f - i * 40.f, 0.75f, 0.055f, 0.5f + i * 0.05f);
    for (int i = 0; i < 2; i++) GEN(snd_gen_shot, a->snd_shot_shotgun[i], 0.95f, "shotgun", 190.f + i * 12.f, 0.95f, 0.115f, 1.0f);
    for (int i = 0; i < 3; i++) GEN(snd_gen_shot, a->snd_shot_rifle[i], 0.62f, "rifle", 520.f - i * 30.f, 0.85f, 0.045f, 0.75f);
    for (int i = 0; i < 2; i++) GEN(snd_gen_shot, a->snd_shot_sniper[i], 1.25f, "sniper", 130.f + i * 10.f, 0.95f, 0.09f, 1.2f);
    for (int i = 0; i < 2; i++) GEN(snd_gen_shot, a->snd_shot_crossbow[i], 0.42f, "crossbow", 1500.f + i * 90.f, 0.35f, 0.03f, 0.15f);
    for (int i = 0; i < 3; i++) GEN(snd_gen_melee, a->snd_melee_swing[i], 0.34f, "swing", 420.f + i * 90.f, 0.f);
    for (int i = 0; i < 3; i++) GEN(snd_gen_melee, a->snd_melee_hit[i], 0.40f, "meleehit", 210.f + i * 40.f, 0.9f);
    // zombie voices: pitch varies a lot between individuals
    float voices[6] = {88.f, 104.f, 76.f, 121.f, 68.f, 96.f};
    for (int i = 0; i < 6; i++) GEN(snd_gen_groan, a->snd_groan[i], 1.7f, "groan", voices[i], 0.5f + 0.09f * (float)i, 1.6f, 31 + i * 7);
    for (int i = 0; i < 3; i++) GEN(snd_gen_scream, a->snd_scream[i], 2.3f, "scream", 300.f + i * 55.f, 71 + i * 13);
    for (int i = 0; i < 4; i++) GEN(snd_gen_step, a->snd_step_player[i], 0.30f, "pstep", 120.f + i * 8.f, 11 + i * 5, 0.5f);
    for (int i = 0; i < 4; i++) GEN(snd_gen_step, a->snd_step_zombie[i], 0.34f, "zstep", 92.f + i * 7.f, 91 + i * 5, 0.35f);
    for (int i = 0; i < 3; i++) GEN(snd_gen_thunder, a->snd_thunder[i], 6.5f, "thunder", 5 + i * 9);
    GEN(snd_gen_rain, a->snd_rain, 8.0f, "rain", 0.85f, 3);
    GEN(snd_gen_click, a->snd_click, 0.09f, "click", 1800.f, 0.05f);
    GEN(snd_gen_impact, a->snd_hit, 0.16f, "hit", 700.f, 0.13f);
    for (int i = 0; i < 3; i++) GEN(snd_gen_break_wood, a->snd_break[i], 1.1f, "break", 41 + i * 17);
    for (int i = 0; i < 2; i++) GEN(snd_gen_metal, a->snd_metal[i], 0.9f, "metal", 320.f + i * 130.f, 0.85f);
    GEN(snd_gen_heartbeat, a->snd_heartbeat, 1.2f, "heartbeat");
    for (int i = 0; i < 2; i++) GEN(snd_gen_stinger, a->snd_stinger[i], 3.0f, "stinger", 17 + i * 5);
    GEN(snd_gen_hum, a->snd_hum, 4.0f, "hum", 58.f, 4.0f);
    for (int i = 0; i < 3; i++) GEN(snd_gen_reload, a->snd_reload[i], 0.85f, "reload", 21 + i * 6);
    GEN(snd_gen_pickup, a->snd_pickup, 0.35f, "pickup");
    for (int i = 0; i < 2; i++) GEN(snd_gen_door, a->snd_door[i], 1.1f, "door", 61 + i * 4);
    GEN(snd_gen_explosion, a->snd_explosion, 2.2f, "explosion", 13);
    for (int i = 0; i < 2; i++) GEN(snd_gen_roar, a->snd_roar[i], 2.6f, "roar", 101 + i * 11);
    GEN(snd_gen_click, a->snd_empty, 0.10f, "empty", 950.f, 0.06f);
    GEN(snd_gen_click, a->snd_ui_move, 0.06f, "uimove", 1300.f, 0.035f);
    GEN(snd_gen_pickup, a->snd_ui_select, 0.22f, "uiselect");
    GEN(snd_gen_click, a->snd_ui_back, 0.14f, "uiback", 420.f, 0.10f);
    GEN(snd_gen_pickup, a->snd_heal, 0.55f, "heal");
    GEN(snd_gen_metal, a->snd_build, 0.65f, "build", 240.f, 0.60f);
}

void sounds_free(Assets *a) {
    // every Sound* member of Assets is laid out contiguously from snd_shot_pistol
    Sound **base = &a->snd_shot_pistol[0];
    Sound **end = &a->snd_build + 1;
    for (Sound **p = base; p < end; p++) {
        if (*p) { snd_free(*p); *p = NULL; }
    }
}
