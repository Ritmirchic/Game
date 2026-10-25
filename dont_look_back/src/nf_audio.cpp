// ============================================================================
//  nf_audio.cpp : mixer, spatialisation, reverb, waveOut backend
// ============================================================================
#include "nf_audio.h"
#include <thread>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <mmsystem.h>
#  pragma comment(lib, "winmm.lib")
#endif

namespace nf {

static constexpr int MAX_VOICES = 48;

// ------------------------------------------------------------------ wav
bool loadWav(const std::string& path, SoundData& out) {
    std::vector<uint8_t> raw;
    if (!readFile(path.c_str(), raw)) return false;
    if (raw.size() < 44) return false;
    if (memcmp(raw.data(), "RIFF", 4) != 0 || memcmp(raw.data() + 8, "WAVE", 4) != 0) return false;
    size_t p = 12;
    int ch = 1, bits = 16, rate = 44100;
    const uint8_t* samp = nullptr;
    size_t sampBytes = 0;
    while (p + 8 <= raw.size()) {
        uint32_t sz = raw[p + 4] | (raw[p + 5] << 8) | (raw[p + 6] << 16) | ((uint32_t)raw[p + 7] << 24);
        const char* id = (const char*)raw.data() + p;
        if (!memcmp(id, "fmt ", 4)) {
            if (p + 16 > raw.size()) return false;
            ch = raw[p + 10] | (raw[p + 11] << 8);
            rate = raw[p + 12] | (raw[p + 13] << 8) | (raw[p + 14] << 16) | (raw[p + 15] << 24);
            bits = raw[p + 22] | (raw[p + 23] << 8);
        } else if (!memcmp(id, "data", 4)) {
            size_t off = p + 8;
            size_t avail = raw.size() - off;
            sampBytes = sz < avail ? sz : avail;
            samp = raw.data() + off;
        }
        p += 8 + sz + (sz & 1);
    }
    if (!samp || !sampBytes) return false;
    int bytesPerSample = bits / 8;
    size_t n = sampBytes / (bytesPerSample * ch);
    out.samples.resize(n);
    out.rate = rate;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* s = samp + i * bytesPerSample * ch;
        float v = 0;
        if (bits == 16) v = (int16_t)(s[0] | (s[1] << 8)) / 32768.0f;
        else if (bits == 8) v = (s[0] - 128) / 128.0f;
        else if (bits == 24) v = ((int32_t)((s[0] << 8) | (s[1] << 16) | (s[2] << 24))) / 2147483648.0f;
        out.samples[i] = v;
    }
    // alias-free sample-rate conversion to output rate
    if (rate != 44100 && n > 2) {
        double ratio = 44100.0 / rate;
        size_t nn = (size_t)(n * ratio);
        std::vector<float> conv(nn);
        for (size_t i = 0; i < nn; ++i) {
            double sp = i / ratio;
            size_t i0 = (size_t)sp;
            size_t i1 = mini(i0 + 1, n - 1);
            float t = (float)(sp - i0);
            conv[i] = lerpf(out.samples[i0], out.samples[i1], t);
        }
        out.samples.swap(conv);
        out.rate = 44100;
    }
    out.loaded = n > 0;
    return out.loaded;
}

bool writeWav(const std::string& path, const std::vector<float>& mono, int rate) {
    std::vector<uint8_t> d;
    auto put32 = [&](uint32_t v) { d.push_back(v & 255); d.push_back((v >> 8) & 255); d.push_back((v >> 16) & 255); d.push_back((v >> 24) & 255); };
    auto put16 = [&](uint16_t v) { d.push_back(v & 255); d.push_back((v >> 8) & 255); };
    uint32_t dataBytes = (uint32_t)mono.size() * 2;
    d.insert(d.end(), {'R', 'I', 'F', 'F'});
    put32(36 + dataBytes);
    d.insert(d.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(16); put16(1); put16(1); put32((uint32_t)rate); put32((uint32_t)rate * 2);
    put16(2); put16(16);
    d.insert(d.end(), {'d', 'a', 't', 'a'});
    put32(dataBytes);
    for (float f : mono) {
        int16_t s = (int16_t)clampf(f * 32767.0f, -32768.0f, 32767.0f);
        put16((uint16_t)s);
    }
    return writeFile(path.c_str(), d.data(), d.size());
}

// ------------------------------------------------------------------ Audio
bool Audio::init(int rate) {
    rate_ = rate;
    voices_.assign(MAX_VOICES, Voice());
    const int combLens[4] = {1557, 1617, 1491, 1422};
    for (int i = 0; i < 4; ++i) comb_[i].assign(combLens[i] * 2, 0.0f);
    ap_[0].assign(556, 0.0f);
    ap_[1].assign(441, 0.0f);
    for (int i = 0; i < (int)Bus::COUNT; ++i) busVol_[i] = (i == 0) ? 1.0f : 0.85f;
    return true;
}

void Audio::shutdown() {
    stopDevice();
    stopAll(0.0f);
}

SoundId Audio::load(const std::string& path) {
    SoundData s;
    if (!loadWav(path, s)) {
        logMsg("[audio] failed to load %s", path.c_str());
        sounds_.push_back(s);
        return SoundId{(int)sounds_.size() - 1};
    }
    sounds_.push_back(std::move(s));
    return SoundId{(int)sounds_.size() - 1};
}

SoundId Audio::addSamples(const std::vector<float>& mono, int rate) {
    SoundData s;
    s.samples = mono;
    s.rate = rate;
    s.loaded = !mono.empty();
    if (rate != 44100 && mono.size() > 2) {
        double ratio = 44100.0 / rate;
        size_t nn = (size_t)(mono.size() * ratio);
        s.samples.resize(nn);
        for (size_t i = 0; i < nn; ++i) {
            double sp = i / ratio;
            size_t i0 = (size_t)sp, i1 = mini(i0 + 1, mono.size() - 1);
            s.samples[i] = lerpf(mono[i0], mono[i1], (float)(sp - i0));
        }
        s.rate = 44100;
    }
    sounds_.push_back(std::move(s));
    return SoundId{(int)sounds_.size() - 1};
}

const SoundData* Audio::data(SoundId id) const {
    if (id.v < 0 || id.v >= (int)sounds_.size()) return nullptr;
    return &sounds_[id.v];
}

int Audio::play(SoundId id, float volume, float pitch, Bus bus, float pan, bool loop) {
    if (!loaded(id)) return 0;
    std::lock_guard<std::mutex> lk(mx_);
    for (auto& v : voices_) {
        if (v.active) continue;
        v = Voice();
        v.active = true;
        v.snd = &sounds_[id.v];
        v.pos = 0;
        v.pitch = pitch;
        v.baseVol = volume;
        v.targetVol = volume;
        v.bus = bus;
        v.loop = loop;
        v.threeD = false;
        v.volL = volume * (1.0f - clampf(pan, -1, 1) * 0.5f);
        v.volR = volume * (1.0f + clampf(pan, -1, 1) * 0.5f);
        v.id = nextHandle_++;
        return v.id;
    }
    return 0;
}

int Audio::play3D(SoundId id, const V3& pos, float volume, float pitch, Bus bus, bool loop, float range) {
    if (!loaded(id)) return 0;
    std::lock_guard<std::mutex> lk(mx_);
    for (auto& v : voices_) {
        if (v.active) continue;
        v = Voice();
        v.active = true;
        v.snd = &sounds_[id.v];
        v.pos = 0;
        v.pitch = pitch;
        v.baseVol = volume;
        v.targetVol = volume;
        v.bus = bus;
        v.loop = loop;
        v.threeD = true;
        v.worldPos = pos;
        v.range = range;
        v.id = nextHandle_++;
        updateVoicePan(v);
        return v.id;
    }
    return 0;
}

void Audio::setListener(const V3& pos, const V3& fwd, const V3& up) {
    listenerPos_ = pos;
    listenerFwd_ = normalize(fwd);
    listenerUp_ = normalize(up);
    listenerRight_ = normalize(cross(listenerFwd_, listenerUp_));
}

void Audio::updateVoicePan(Voice& v) {
    if (!v.threeD) return;
    V3 d = v.worldPos - listenerPos_;
    float dist = length(d);
    float atten = 1.0f / (1.0f + dist * dist / maxf(0.5f, v.range * v.range * 0.16f));
    atten *= saturate(1.0f - dist / (v.range * 2.2f));
    V3 dir = dist > 0.001f ? d / dist : listenerFwd_;
    float pan = dot(dir, listenerRight_);
    float front = dot(dir, listenerFwd_);
    float spread = 0.72f;
    float l = atten * (1.0f - pan * spread) * (0.65f + 0.35f * saturate(front * 0.5f + 0.5f));
    float r = atten * (1.0f + pan * spread) * (0.65f + 0.35f * saturate(front * 0.5f + 0.5f));
    float occ = (front < -0.35f) ? 0.72f : 1.0f;
    v.volL = v.baseVol * l * occ;
    v.volR = v.baseVol * r * occ;
}

void Audio::stop(int handle, float fadeOut) {
    if (!handle) return;
    std::lock_guard<std::mutex> lk(mx_);
    for (auto& v : voices_) {
        if (v.active && v.id == handle) {
            if (fadeOut > 0.0f) v.fadeOut = fadeOut;
            else v.active = false;
            return;
        }
    }
}

void Audio::stopAll(float fade) {
    std::lock_guard<std::mutex> lk(mx_);
    for (auto& v : voices_) {
        if (!v.active) continue;
        if (fade > 0.0f) v.fadeOut = fade;
        else v.active = false;
    }
}

bool Audio::active(int handle) const {
    for (auto& v : voices_) if (v.active && v.id == handle) return true;
    return false;
}

float Audio::timeOf(int handle) const {
    for (auto& v : voices_) if (v.active && v.id == handle) return v.elapsed;
    return -1.0f;
}

void Audio::setHandleVolume(int handle, float vol) {
    for (auto& v : voices_) if (v.active && v.id == handle) { v.baseVol = vol; v.targetVol = vol; if (v.threeD) updateVoicePan(v); else { v.volL = vol; v.volR = vol; } }
}

void Audio::setHandlePitch(int handle, float p) {
    for (auto& v : voices_) if (v.active && v.id == handle) v.pitch = p;
}

void Audio::setHandlePos(int handle, const V3& p) {
    for (auto& v : voices_) if (v.active && v.id == handle) { v.worldPos = p; v.threeD = true; }
}

void Audio::setReverb(float wet, float room) {
    revWet_ = clampf(wet, 0.0f, 1.0f);
    revRoom_ = clampf(room, 0.0f, 1.0f);
}

void Audio::duckAmbient(float amount, float) { duckTarget_ = clampf(amount, 0.0f, 1.0f); }

int Audio::activeVoices() const {
    int n = 0;
    for (auto& v : voices_) if (v.active) ++n;
    return n;
}

void Audio::mix(float* out, int frames) {
    std::lock_guard<std::mutex> lk(mx_);
    const float dt = 1.0f / (float)rate_;
    // reverb buffers
    float* comb0 = comb_[0].data(); size_t c0n = comb_[0].size();
    float* comb1 = comb_[1].data(); size_t c1n = comb_[1].size();
    float* comb2 = comb_[2].data(); size_t c2n = comb_[2].size();
    float* comb3 = comb_[3].data(); size_t c3n = comb_[3].size();
    float* ap0 = ap_[0].data(); size_t a0n = ap_[0].size();
    float* ap1 = ap_[1].data(); size_t a1n = ap_[1].size();
    float fb = 0.62f + revRoom_ * 0.28f;
    float damp = 0.18f + revRoom_ * 0.22f;
    float lp0 = 0, lp1 = 0, lp2 = 0, lp3 = 0;
    for (int f = 0; f < frames; ++f) {
        float l = 0, r = 0;
        float dry = 0;
        for (auto& v : voices_) {
            if (!v.active) continue;
            const SoundData& s = *v.snd;
            if (s.samples.empty()) { v.active = false; continue; }
            size_t i0 = (size_t)v.pos;
            size_t i1 = i0 + 1;
            if (i1 >= s.samples.size()) {
                if (v.loop) { v.pos = 0; i0 = 0; i1 = 1; }
                else { v.active = false; continue; }
            }
            float frac = (float)(v.pos - i0);
            float smp = lerpf(s.samples[i0], s.samples[i1], frac);
            v.pos += v.pitch;
            v.elapsed += dt;
            float g = 1.0f, busG = busVol_[(int)v.bus];
            if (v.fadeOut > 0.0f) {
                v.fadeGain -= dt / v.fadeOut;
                if (v.fadeGain <= 0.0f) { v.active = false; continue; }
                g = v.fadeGain;
            }
            if (v.bus == Bus::AMBIENT) { duck_ = approach(duck_, duckTarget_, duckSpeed_ * dt); busG *= duck_; }
            if (v.threeD) updateVoicePan(v);
            float vl = v.volL * busG * g;
            float vr = v.volR * busG * g;
            l += smp * vl;
            r += smp * vr;
            dry += smp * (vl + vr) * 0.5f;
        }
        // reverb send
        float send = dry * revWet_ * 0.5f;
        float t0 = comb0[combIdx_[0]], t1 = comb1[combIdx_[1]];
        float t2 = comb2[combIdx_[2]], t3 = comb3[combIdx_[3]];
        lp0 = lerpf(lp0, t0, damp); lp1 = lerpf(lp1, t1, damp);
        lp2 = lerpf(lp2, t2, damp); lp3 = lerpf(lp3, t3, damp);
        comb0[combIdx_[0]] = send + lp0 * fb;
        comb1[combIdx_[1]] = send + lp1 * fb;
        comb2[combIdx_[2]] = send + lp2 * fb;
        comb3[combIdx_[3]] = send + lp3 * fb;
        combIdx_[0] = (combIdx_[0] + 1) % c0n;
        combIdx_[1] = (combIdx_[1] + 1) % c1n;
        combIdx_[2] = (combIdx_[2] + 1) % c2n;
        combIdx_[3] = (combIdx_[3] + 1) % c3n;
        float wet = (t0 + t1 + t2 + t3) * 0.25f;
        // allpass
        float a0 = ap0[apIdx_[0]];
        float o0 = -wet + a0;
        ap0[apIdx_[0]] = wet + a0 * 0.5f;
        apIdx_[0] = (apIdx_[0] + 1) % a0n;
        float a1 = ap1[apIdx_[1]];
        float o1 = -o0 + a1;
        ap1[apIdx_[1]] = o0 + a1 * 0.5f;
        apIdx_[1] = (apIdx_[1] + 1) % a1n;
        wet = o1;
        l = l * master_ + wet * master_ * 0.5f;
        r = r * master_ + wet * master_ * 0.5f;
        // soft clip
        l = std::tanh(l * 1.05f);
        r = std::tanh(r * 1.05f);
        out[f * 2] = l;
        out[f * 2 + 1] = r;
    }
}

// ------------------------------------------------------------------ device
#ifdef _WIN32
namespace {
struct WaveOutCtx {
    HWAVEOUT hwo = nullptr;
    static constexpr int BUFFERS = 4;
    static constexpr int FRAMES = 512;
    WAVEHDR hdr[BUFFERS];
    std::vector<int16_t> buf[BUFFERS];
    int next = 0;
    std::thread thread;
    std::atomic<bool> run{false};
    Audio* audio = nullptr;
    bool primed = false;
};
static DWORD WINAPI waveThread(LPVOID param) {
    WaveOutCtx* c = (WaveOutCtx*)param;
    std::vector<float> mixbuf(WaveOutCtx::FRAMES * 2);
    while (c->run.load()) {
        // find a free buffer
        int idx = -1;
        for (int i = 0; i < WaveOutCtx::BUFFERS; ++i) {
            int j = (c->next + i) % WaveOutCtx::BUFFERS;
            if (!c->primed || (c->hdr[j].dwFlags & WHDR_DONE)) { idx = j; break; }
        }
        if (idx < 0) { Sleep(1); continue; }
        c->audio->mix(mixbuf.data(), WaveOutCtx::FRAMES);
        int16_t* dst = c->buf[idx].data();
        for (int i = 0; i < WaveOutCtx::FRAMES * 2; ++i)
            dst[i] = (int16_t)clampf(mixbuf[i] * 32767.0f, -32768.0f, 32767.0f);
        if (c->hdr[idx].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(c->hwo, &c->hdr[idx], sizeof(WAVEHDR));
        c->hdr[idx].dwBufferLength = WaveOutCtx::FRAMES * 2 * sizeof(int16_t);
        c->hdr[idx].lpData = (LPSTR)dst;
        c->hdr[idx].dwFlags = 0;
        waveOutPrepareHeader(c->hwo, &c->hdr[idx], sizeof(WAVEHDR));
        if (waveOutWrite(c->hwo, &c->hdr[idx], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) { Sleep(2); continue; }
        c->primed = true;
        c->next = (idx + 1) % WaveOutCtx::BUFFERS;
        Sleep(1);
    }
    return 0;
}
}
#endif

bool Audio::startDevice() {
#ifdef _WIN32
    if (device_) return true;
    WaveOutCtx* c = new WaveOutCtx();
    c->audio = this;
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = (DWORD)rate_;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = (DWORD)rate_ * 4;
    if (waveOutOpen(&c->hwo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        delete c;
        logMsg("[audio] waveOutOpen failed (no device)");
        return false;
    }
    for (int i = 0; i < WaveOutCtx::BUFFERS; ++i) {
        c->buf[i].assign(WaveOutCtx::FRAMES * 2, 0);
        memset(&c->hdr[i], 0, sizeof(WAVEHDR));
    }
    c->run = true;
    c->thread = std::thread(waveThread, (LPVOID)c);
    device_ = c;
    return true;
#else
    return false;
#endif
}

void Audio::stopDevice() {
#ifdef _WIN32
    if (!device_) return;
    WaveOutCtx* c = (WaveOutCtx*)device_;
    c->run = false;
    if (c->thread.joinable()) c->thread.join();
    waveOutReset(c->hwo);
    for (int i = 0; i < WaveOutCtx::BUFFERS; ++i)
        if (c->hdr[i].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(c->hwo, &c->hdr[i], sizeof(WAVEHDR));
    waveOutClose(c->hwo);
    delete c;
    device_ = nullptr;
#else
    device_ = nullptr;
#endif
}

} // namespace nf
