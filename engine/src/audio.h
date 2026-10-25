// ============================================================================
// THE LAST NIGHT — software audio mixer (3D positional, procedural synths)
// ============================================================================
#ifndef TLN_AUDIO_H
#define TLN_AUDIO_H

#include "core.h"

#define AUD_MAX_VOICES 48
#define AUD_RATE 44100

typedef struct Sound {
    int16_t *pcm;      // interleaved stereo
    int frames;
    int rate;
    char name[40];
} Sound;

typedef struct Voice {
    const Sound *snd;
    float pos_t;        // playhead in frames
    float rate;         // pitch ratio
    float vol_l, vol_r;
    float pitch_l, pitch_r;  // playback rate per channel (cheap doppler detune)
    int active;
    int loop;
    int bus;            // 0 = sfx, 1 = ambience, 2 = music, 3 = ui
    float pitch;
} Voice;

typedef struct Listener {
    Vec3 pos, forward, right;
} Listener;

typedef struct Audio {
    Voice voices[AUD_MAX_VOICES];
    Listener listener;
    float master, sfx, ambience, music, ui;
    int sample_counter;
    float time;
    Rng rng;
    float reverb_mix;
    // simple 1-pole filters for ambience beds
    float noise_state[2];
    int amb_bed_id;
    float amb_target, amb_current;
} Audio;

int   aud_init(Audio *a);
void  aud_shutdown(Audio *a);
void  aud_set_listener(Audio *a, Vec3 pos, Vec3 forward, Vec3 up);
// play a sound at a world position (NULL pos = non-positional)
int   aud_play(Audio *a, const Sound *s, Vec3 *pos, float volume, float pitch, int loop, int bus);
void  aud_stop(Audio *a, int voice);
void  aud_stop_all(Audio *a, int bus);
void  aud_mix(Audio *a, int16_t *out, int frames);   // fills stereo buffer

// ---------------------------------------------------------------- synthesis
Sound *snd_from_wav(const char *path, const char *name);
Sound *snd_make(const char *name, int frames);
void  snd_free(Sound *s);
Sound *snd_resample(Sound *s, int rate);

// procedural sound generators (deterministic)
void snd_gen_shot(Sound *s, float base_hz, float noise, float decay, float thump);
void snd_gen_melee(Sound *s, float whoosh_hz, float impact);
void snd_gen_groan(Sound *s, float voice_hz, float growl, float duration, int seed);
void snd_gen_scream(Sound *s, float pitch, int seed);
void snd_gen_step(Sound *s, float tone, int seed, float foot);
void snd_gen_thunder(Sound *s, int seed);
void snd_gen_rain(Sound *s, float intensity, int seed);
void snd_gen_click(Sound *s, float hz, float dur);
void snd_gen_impact(Sound *s, float hz, float dur);
void snd_gen_break_wood(Sound *s, int seed);
void snd_gen_metal(Sound *s, float hz, float dur);
void snd_gen_heartbeat(Sound *s);
void snd_gen_stinger(Sound *s, int seed);
void snd_gen_hum(Sound *s, float hz, float dur);
void snd_gen_reload(Sound *s, int seed);
void snd_gen_pickup(Sound *s);
void snd_gen_door(Sound *s, int seed);
void snd_gen_explosion(Sound *s, int seed);
void snd_gen_roar(Sound *s, int seed);

#endif
