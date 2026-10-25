/* ============================================================================
   SPACE MINER — platform/win32/win_audio.c
   WinMM waveOut streaming device fed by the portable software mixer.
   ========================================================================== */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "../../core/audio.h"

#define WA_BUFFERS 4
#define WA_FRAMES  512

static HWAVEOUT g_wo;
static WAVEHDR g_hdr[WA_BUFFERS];
static s16 *g_buf[WA_BUFFERS];
static HANDLE g_event;
static HANDLE g_thread;
static volatile LONG g_run;
static int g_ok;
static f32 *g_mixbuf;

void waudio_set_settings(f32 master, f32 music, f32 sfx, int muted) {
    if (muted) audio_set_gain(0.f, 0.f, 0.f);
    else audio_set_gain(master, music, sfx);
}

static void fill_and_write(int i) {
    audio_mix(g_mixbuf, WA_FRAMES);
    s16 *dst = g_buf[i];
    for (int f = 0; f < WA_FRAMES * 2; f++) {
        f32 x = g_mixbuf[f];
        int v = (int)(x * 32000.f);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        dst[f] = (s16)v;
    }
    g_hdr[i].dwBufferLength = (DWORD)(WA_FRAMES * 2 * sizeof(s16));
    waveOutWrite(g_wo, &g_hdr[i], sizeof(WAVEHDR));
}

static DWORD WINAPI audio_thread(LPVOID param) {
    (void)param;
    /* prime all buffers */
    for (int i = 0; i < WA_BUFFERS; i++) fill_and_write(i);
    while (g_run) {
        DWORD r = WaitForSingleObject(g_event, 120);
        if (r != WAIT_OBJECT_0 && r != WAIT_TIMEOUT) break;
        for (int i = 0; i < WA_BUFFERS; i++) {
            if (g_hdr[i].dwFlags & WHDR_DONE) {
                g_hdr[i].dwFlags &= ~WHDR_DONE;
                fill_and_write(i);
            }
        }
    }
    return 0;
}

void waudio_init(void) {
    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = AUDIO_RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    g_mixbuf = (f32 *)malloc(sizeof(f32) * 2 * WA_FRAMES);
    if (!g_mixbuf) return;
    g_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!g_event) return;
    MMRESULT mr = waveOutOpen(&g_wo, WAVE_MAPPER, &fmt, (DWORD_PTR)g_event, 0, CALLBACK_EVENT);
    if (mr != MMSYSERR_NOERROR) {
        sm_warnf("waveOutOpen failed (%u)", (unsigned)mr);
        return;
    }
    for (int i = 0; i < WA_BUFFERS; i++) {
        g_buf[i] = (s16 *)malloc(sizeof(s16) * 2 * WA_FRAMES);
        memset(g_buf[i], 0, sizeof(s16) * 2 * WA_FRAMES);
        memset(&g_hdr[i], 0, sizeof(WAVEHDR));
        g_hdr[i].lpData = (LPSTR)g_buf[i];
        g_hdr[i].dwBufferLength = (DWORD)(WA_FRAMES * 2 * sizeof(s16));
        waveOutPrepareHeader(g_wo, &g_hdr[i], sizeof(WAVEHDR));
    }
    g_run = 1;
    g_thread = CreateThread(NULL, 0, audio_thread, NULL, 0, NULL);
    g_ok = g_thread != NULL;
    if (g_ok) SetThreadPriority(g_thread, THREAD_PRIORITY_ABOVE_NORMAL);
}

int waudio_ok(void) { return g_ok; }

void waudio_shutdown(void) {
    if (!g_ok) return;
    InterlockedExchange(&g_run, 0);
    SetEvent(g_event);
    if (g_thread) { WaitForSingleObject(g_thread, 800); CloseHandle(g_thread); g_thread = NULL; }
    waveOutReset(g_wo);
    for (int i = 0; i < WA_BUFFERS; i++) {
        waveOutUnprepareHeader(g_wo, &g_hdr[i], sizeof(WAVEHDR));
        free(g_buf[i]);
        g_buf[i] = NULL;
    }
    waveOutClose(g_wo);
    CloseHandle(g_event);
    free(g_mixbuf);
    g_mixbuf = NULL;
    g_ok = 0;
}
