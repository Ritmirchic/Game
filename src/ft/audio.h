// FRACTURE — procedural audio: synthesized sample bank + positional mixer
#pragma once
#include "common.h"
#include "math.h"

namespace ft {

using SfxId = int;
enum : SfxId {
    SFX_UI_CLICK = 0,
    SFX_UI_HOVER,
    SFX_FOOTSTEP_1,
    SFX_FOOTSTEP_2,
    SFX_FOOTSTEP_3,
    SFX_FOOTSTEP_4,
    SFX_IMPACT_WOOD,
    SFX_IMPACT_METAL,
    SFX_IMPACT_CONCRETE,
    SFX_IMPACT_GLASS,
    SFX_IMPACT_SOIL,
    SFX_EXPLOSION_S,
    SFX_EXPLOSION_M,
    SFX_EXPLOSION_L,
    SFX_DEBRIS_RAIN,
    SFX_GUN_SHOTGUN,
    SFX_GUN_RIFLE,
    SFX_GUN_SMG,
    SFX_GUN_PISTOL,
    SFX_GUN_PLASMA,
    SFX_ROCKET_LAUNCH,
    SFX_GRENADE_LAUNCH,
    SFX_CHARGE_BEEP,
    SFX_CHARGE_STICK,
    SFX_DRILL,
    SFX_SAW,
    SFX_TORCH,
    SFX_EXTINGUISHER,
    SFX_SPRAY_PAINT,
    SFX_HARPOON,
    SFX_SLEDGE_SWING,
    SFX_DOOR_CREAK,
    SFX_IGNITE,
    SFX_FIRE_LOOP,
    SFX_VEHICLE_CRASH,
    SFX_PICKUP,
    SFX_JUMP_LAND,
    SFX_BODY_FALL,
    SFX_EMPTY,
    SFX_COUNT
};

struct VoiceDesc {
    SfxId sfx = SFX_UI_CLICK;
    Vec3 pos{};             // world position (ignored when direct2D)
    f32 gain = 1.f;
    f32 pitch = 1.f;        // playback rate
    bool direct2D = false;  // UI / no attenuation
    bool loop = false;
    f32 minDist = 2.5f;     // attenuation reference
    f32 maxDist = 90.f;
    int  priority = 0;      // higher survives voice stealing
};

// ---- bank: synthesized once at startup ----
struct SfxSample {
    std::vector<s16> pcm;   // stereo interleaved
    int frames = 0;
    bool loop = false;
};

void audioInitBank();                       // idempotent, synthesizes all SFX_COUNT
const SfxSample& sfxSample(SfxId id);
bool sfxWriteWav(SfxId id, const char* path);   // debug/verification
bool sfxWriteWavRaw(const s16* pcm, int frames, const char* path);

// ---- mixer: pull model, platform feeds device ----
class AudioMixer {
public:
    void init(int sampleRate = 44100);
    void setListener(Vec3 pos, Vec3 fwd) { lpos_ = pos; lfwd_ = fwd; }
    void setMaster(f32 m) { master_ = tclamp(m, 0.f, 1.f); }
    void setVolume(SfxId s, f32 v);        // per-category not needed; master only (MVP)
    int  play(const VoiceDesc& d);         // returns voice id (-1 if rejected)
    void stop(int voice);
    void stopAll();
    void updateLoops(f32 dt);              // called per frame
    // mix exactly `frames` stereo frames into out (s16 interleaved)
    void mix(s16* out, int frames);
    int activeVoices() const;
    int sampleRate() const { return sr_; }

private:
    struct Voice {
        SfxId sfx;
        Vec3 pos;
        f32 gain, pitch;
        bool direct2D, loop, active;
        f32 minDist, maxDist;
        int priority;
        f64 cursor;         // fractional sample cursor
        f32 age;
    };
    std::vector<Voice> voices_;
    Vec3 lpos_, lfwd_;
    f32 master_ = 1.f;
    int sr_ = 44100;
};

// global (platform binds device in win_main; headless tests mix manually)
AudioMixer& audio();
// convenience one-shots used across systems
void sfxPlay(SfxId id, Vec3 pos, f32 gain = 1.f, f32 pitch = 1.f);
void sfxPlay2D(SfxId id, f32 gain = 1.f);
// map impact event material → sample
SfxId sfxForImpact(u8 mat, bool hardHit);

} // namespace ft
