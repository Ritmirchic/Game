// FRACTURE — Win32 audio device (waveOut, callback refill, no external assets)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "../ft/audio.h"
#include <vector>

namespace ft {

namespace {
struct Dev {
    HWAVEOUT hwo = nullptr;
    HANDLE th = nullptr;
    HANDLE quitEv = nullptr;
    volatile LONG running = 0;
    static constexpr int NBUF = 4;
    static constexpr int FRAMES = 1024;
    WAVEHDR hdr[NBUF];
    std::vector<s16> buf[NBUF];
};
Dev g_dev;

void CALLBACK waveProc(HWAVEOUT, UINT msg, DWORD_PTR, DWORD_PTR param, DWORD_PTR) {
    if (msg != WOM_DONE) return;
    if (!g_dev.running) return;
    WAVEHDR* h = (WAVEHDR*)param;
    if (h->dwFlags & WHDR_PREPARED) {
        s16* pcm = (s16*)h->lpData;
        audio().mix(pcm, Dev::FRAMES);
        // unprepare not required for reuse; rewrite in place
        waveOutWrite(g_dev.hwo, h, sizeof(WAVEHDR));
    }
}
} // namespace

bool startAudioDevice() {
    audioInitBank();
    if (g_dev.hwo) return true;
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = 44100;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    MMRESULT r = waveOutOpen(&g_dev.hwo, WAVE_MAPPER, &fmt, (DWORD_PTR)waveProc, 0, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) { g_dev.hwo = nullptr; return false; }
    for (int i = 0; i < Dev::NBUF; i++) {
        g_dev.buf[i].assign((size_t)Dev::FRAMES * 2, 0);
        WAVEHDR& h = g_dev.hdr[i];
        ZeroMemory(&h, sizeof(h));
        h.lpData = (LPSTR)g_dev.buf[i].data();
        h.dwBufferLength = (DWORD)(g_dev.buf[i].size() * 2);
        waveOutPrepareHeader(g_dev.hwo, &h, sizeof(h));
        waveOutWrite(g_dev.hwo, &h, sizeof(h));
    }
    InterlockedExchange(&g_dev.running, 1);
    logf("audio device: waveOut44100 stereo x%d", Dev::NBUF);
    return true;
}

void stopAudioDevice() {
    if (!g_dev.hwo) return;
    InterlockedExchange(&g_dev.running, 0);
    waveOutReset(g_dev.hwo);
    for (int i = 0; i < Dev::NBUF; i++)
        if (g_dev.hdr[i].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(g_dev.hwo, &g_dev.hdr[i], sizeof(WAVEHDR));
    waveOutClose(g_dev.hwo);
    g_dev.hwo = nullptr;
    audio().stopAll();
}

} // namespace ft
#endif
