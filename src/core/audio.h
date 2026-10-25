/* ============================================================================
   SPACE MINER — core/audio.h
   Portable software mixer: PCM playback with pitch/pan/loops, music crossfade.
   The platform layer only has to pull mixed frames.
   ========================================================================== */
#ifndef SM_AUDIO_H
#define SM_AUDIO_H
#include "common.h"
#include "pack.h"

#define AUDIO_RATE 32000
#define AUDIO_MAX_SOUNDS 160
#define AUDIO_MAX_VOICES 56

typedef struct {
    char name[40];
    s16 *data;          /* interleaved s16 */
    int frames;
    int channels;       /* 1 or 2 */
    int is_loop_ok;
} Sound;

void audio_init(void);
int  audio_load_all(const Pak *pak);
Sound *audio_find(const char *name);

/* one-shot */
void audio_play(const char *name, f32 vol, f32 pan, f32 pitch);
/* looping voice: returns voice index, or -1 */
int  audio_loop(const char *name, f32 vol, f32 pan, f32 pitch);
void audio_voice_set(int voice, f32 vol, f32 pan, f32 pitch);
void audio_voice_stop(int voice);
void audio_music_play(const char *name, f32 fade_seconds);
void audio_music_stop(f32 fade_seconds);
void audio_set_gain(f32 master, f32 music, f32 sfx);
void audio_update(f32 dt);
/* fill interleaved stereo float buffer in [-1,1] */
void audio_mix(f32 *dst, int frames);
int  audio_active_voices(void);

#endif
