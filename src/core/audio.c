/* ============================================================================
   SPACE MINER — core/audio.c  software mixer
   ========================================================================== */
#include "audio.h"
#include "inflate.h"

typedef struct {
    Sound *snd;
    f64 pos;
    f32 pitch;
    f32 vol;
    f32 pan;
    int active;
    int loop;
    int is_music;
    f32 fade;         /* 0..1 target */
    f32 fade_cur;
    f32 fade_speed;
    int stopping;
} Voice;

static Sound g_sounds[AUDIO_MAX_SOUNDS];
static int g_sound_count;
static Voice g_voices[AUDIO_MAX_VOICES];
static f32 g_master = 0.85f, g_music = 0.55f, g_sfx = 0.85f;
static int g_music_voice = -1;

void audio_init(void) {
    memset(g_sounds, 0, sizeof(g_sounds));
    memset(g_voices, 0, sizeof(g_voices));
    g_sound_count = 0;
    g_music_voice = -1;
}

int audio_load_all(const Pak *pak) {
    int loaded = 0;
    for (int i = 0; i < pak->count; i++) {
        const PakEntry *e = &pak->entries[i];
        if (e->kind != PAK_PCM) continue;
        if (g_sound_count >= AUDIO_MAX_SOUNDS) break;
        Sound *s = &g_sounds[g_sound_count];
        memset(s, 0, sizeof(*s));
        strncpy(s->name, e->name, sizeof(s->name) - 1);
        s->channels = (int)e->w;
        s->is_loop_ok = (int)e->h;
        s->data = (s16 *)sm_alloc((usize)e->raw_size);
        if (!s->data) continue;
        u8 *tmp = (u8 *)sm_alloc((usize)e->raw_size);
        if (!tmp) { s->data = NULL; continue; }
        usize got = 0;
        if (sm_inflate(tmp, (usize)e->raw_size, &got, pak->data + pak->data_base + e->offset, e->data_size) != 0) {
            sm_warnf("audio: cannot inflate '%s'", e->name);
            s->data = NULL;
            continue;
        }
        memcpy(s->data, tmp, (usize)e->raw_size);
        s->frames = (int)(e->raw_size / (2u * (u32)s->channels));
        loaded++;
        g_sound_count++;
    }
    sm_logf("audio: %d sounds loaded", loaded);
    return loaded;
}

Sound *audio_find(const char *name) {
    for (int i = 0; i < g_sound_count; i++)
        if (!strcmp(g_sounds[i].name, name)) return &g_sounds[i];
    return NULL;
}

static Voice *alloc_voice(int music) {
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (!g_voices[i].active) {
            memset(&g_voices[i], 0, sizeof(Voice));
            g_voices[i].active = 1;
            g_voices[i].fade = 1.f;
            g_voices[i].fade_cur = 1.f;
            g_voices[i].is_music = music;
            return &g_voices[i];
        }
    }
    return NULL;
}

static int voice_index(Voice *v) { return (int)(v - g_voices); }

int audio_loop(const char *name, f32 vol, f32 pan, f32 pitch) {
    Sound *s = audio_find(name);
    if (!s || !s->data) return -1;
    Voice *v = alloc_voice(0);
    if (!v) return -1;
    v->snd = s; v->pos = 0; v->pitch = pitch; v->vol = vol; v->pan = pan;
    v->loop = 1;
    return voice_index(v);
}

void audio_play(const char *name, f32 vol, f32 pan, f32 pitch) {
    Sound *s = audio_find(name);
    if (!s || !s->data) return;
    Voice *v = alloc_voice(0);
    if (!v) {
        /* steal the quietest non-music voice */
        Voice *best = NULL; f32 bestv = 1e9f;
        for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
            if (g_voices[i].active && !g_voices[i].is_music && g_voices[i].vol < bestv) {
                bestv = g_voices[i].vol; best = &g_voices[i];
            }
        }
        if (!best) return;
        v = best;
        memset(v, 0, sizeof(Voice));
        v->active = 1; v->fade = 1.f; v->fade_cur = 1.f;
    }
    v->snd = s; v->pos = 0; v->pitch = pitch; v->vol = vol; v->pan = pan; v->loop = 0;
}

void audio_voice_set(int voice, f32 vol, f32 pan, f32 pitch) {
    if (voice < 0 || voice >= AUDIO_MAX_VOICES) return;
    Voice *v = &g_voices[voice];
    if (!v->active) return;
    v->vol = vol; v->pan = pan; v->pitch = pitch;
}

void audio_voice_stop(int voice) {
    if (voice < 0 || voice >= AUDIO_MAX_VOICES) return;
    g_voices[voice].active = 0;
}

void audio_music_play(const char *name, f32 fade_seconds) {
    Sound *s = audio_find(name);
    if (!s || !s->data) return;
    if (g_music_voice >= 0) {
        Voice *old = &g_voices[g_music_voice];
        if (old->active && old->snd == s) return;  /* already playing */
        old->stopping = 1;
        old->fade_speed = fade_seconds > 0.01f ? 1.f / fade_seconds : 100.f;
    }
    Voice *v = alloc_voice(1);
    if (!v) return;
    v->snd = s; v->pos = 0; v->pitch = 1.f; v->vol = 1.f; v->pan = 0.f;
    v->loop = 1; v->is_music = 1;
    v->fade = 0.f; v->fade_cur = 0.f;
    v->fade_speed = fade_seconds > 0.01f ? 1.f / fade_seconds : 100.f;
    g_music_voice = voice_index(v);
}

void audio_music_stop(f32 fade_seconds) {
    if (g_music_voice < 0) return;
    Voice *v = &g_voices[g_music_voice];
    if (!v->active) { g_music_voice = -1; return; }
    v->stopping = 1;
    v->fade_speed = fade_seconds > 0.01f ? 1.f / fade_seconds : 100.f;
    g_music_voice = -1;
}

void audio_set_gain(f32 master, f32 music, f32 sfx) {
    g_master = sm_saturate(master);
    g_music = sm_saturate(music);
    g_sfx = sm_saturate(sfx);
}

int audio_active_voices(void) {
    int n = 0;
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) if (g_voices[i].active) n++;
    return n;
}

void audio_update(f32 dt) {
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        Voice *v = &g_voices[i];
        if (!v->active) continue;
        if (v->stopping) v->fade_cur -= v->fade_speed * dt;
        else v->fade_cur += v->fade_speed * dt;
        if (v->fade_cur >= 1.f) { v->fade_cur = 1.f; v->fade_speed = 0.f; }
        if (v->fade_cur <= 0.f) {
            v->active = 0;
            if (i == g_music_voice) g_music_voice = -1;
        }
    }
}

void audio_mix(f32 *dst, int frames) {
    memset(dst, 0, (usize)frames * 2u * sizeof(f32));
    for (int vi = 0; vi < AUDIO_MAX_VOICES; vi++) {
        Voice *v = &g_voices[vi];
        if (!v->active || !v->snd || !v->snd->data) continue;
        Sound *s = v->snd;
        f32 gain = v->vol * v->fade_cur * g_master * (v->is_music ? g_music : g_sfx);
        if (gain <= 0.0001f) continue;
        f32 lpan = 0.5f - v->pan * 0.5f, rpan = 0.5f + v->pan * 0.5f;
        f32 gl = gain * lpan, gr = gain * rpan;
        f64 step = (f64)v->pitch;
        for (int i = 0; i < frames; i++) {
            if (v->pos >= (f64)s->frames) {
                if (v->loop) v->pos = fmod(v->pos, (f64)s->frames);
                else { v->active = 0; break; }
            }
            int i0 = (int)v->pos;
            int i1 = i0 + 1;
            if (i1 >= s->frames) i1 = v->loop ? 0 : s->frames - 1;
            f32 t = (f32)(v->pos - (f64)i0);
            f32 sl, sr;
            if (s->channels == 2) {
                f32 a0 = s->data[(usize)i0 * 2], a1 = s->data[(usize)i1 * 2];
                f32 b0 = s->data[(usize)i0 * 2 + 1], b1 = s->data[(usize)i1 * 2 + 1];
                sl = (a0 + (a1 - a0) * t) * (1.f / 32768.f);
                sr = (b0 + (b1 - b0) * t) * (1.f / 32768.f);
            } else {
                f32 a0 = s->data[i0], a1 = s->data[i1];
                sl = sr = (a0 + (a1 - a0) * t) * (1.f / 32768.f);
            }
            dst[i * 2] += sl * gl;
            dst[i * 2 + 1] += sr * gr;
            v->pos += step;
        }
    }
    /* soft clip */
    for (int i = 0; i < frames * 2; i++) {
        f32 x = dst[i];
        if (x > 1.f) x = 1.f;
        if (x < -1.f) x = -1.f;
        dst[i] = x;
    }
}
