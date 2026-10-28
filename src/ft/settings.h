// FRACTURE — settings (graphics / audio / controls), persisted to INI-style file
#pragma once
#include "common.h"

namespace ft {

enum Preset { PRESET_LOW = 0, PRESET_MEDIUM, PRESET_HIGH, PRESET_ULTRA };

struct Settings {
    // video
    int   width = 1280, height = 720;
    int   windowMode = 1;        //0 windowed,1 borderless,2 fullscreen
    int   vsync = 1;
    int   fpsLimit = 0;          //0 = uncapped
    f32   renderScale = 1.0f;    //0.5..1
    int   shadowQuality = 2;     //0 off,1 low(1024),2 med(2048),3 high(4096)
    int   textureQuality = 1;    //0 low(64 tiles),1 high(128)
    int   lightingQuality = 2;   //0 low (4 lights),1 med (6),2 high (8)
    int   effectsQuality = 2;    // particle budgets
    int   viewDistance = 2;      //0 low,1 med,2 high,3 ultra (fog + cull)
    int   aa = 1;                //0 off,1 msaa2,2 msaa4
    int   motionBlur = 0;        //0/1 (cheap radial on damage only)
    f32   fov = 70.f;
    int   destructionQuality = 2;//0 low,1 med,2 high,3 ultra
    int   debrisDensity = 2;     //0 low..3 ultra
    int   physicsQuality = 2;
    // audio
    f32   master = 0.8f, sfx = 1.f, music = 0.6f;
    int   maxVoices = 24;
    // controls
    f32   mouseSens = 1.0f;      // multiplier
    f32   invertY = 0;
    int   showFps = 0;
    // sandbox defaults carried around
    int   preset = PRESET_HIGH;

    void applyPreset(int p) {
        preset = p;
        switch (p) {
            case PRESET_LOW:
                renderScale = 0.75f; shadowQuality = 1; textureQuality = 0; lightingQuality = 0;
                effectsQuality = 0; viewDistance = 1; aa = 0; destructionQuality = 0; debrisDensity = 0; physicsQuality = 1; break;
            case PRESET_MEDIUM:
                renderScale = 0.85f; shadowQuality = 1; textureQuality = 1; lightingQuality = 1;
                effectsQuality = 1; viewDistance = 2; aa = 1; destructionQuality = 1; debrisDensity = 1; physicsQuality = 2; break;
            case PRESET_ULTRA:
                renderScale = 1.0f; shadowQuality = 3; textureQuality = 1; lightingQuality = 2;
                effectsQuality = 3; viewDistance = 3; aa = 2; destructionQuality = 3; debrisDensity = 3; physicsQuality = 3; break;
            default: // high
                renderScale = 1.0f; shadowQuality = 2; textureQuality = 1; lightingQuality = 2;
                effectsQuality = 2; viewDistance = 2; aa = 2; destructionQuality = 2; debrisDensity = 2; physicsQuality = 3; break;
        }
    }

    void save(const char* path) const;
    bool load(const char* path);
};

// derived helpers
inline int maxLightsFor(int lightingQ) { return lightingQ <= 0 ? 4 : lightingQ == 1 ? 6 : MAX_LIGHTS; }
inline f32 viewDistFor(int vd) { static const float T[4] = {90.f, 150.f, 230.f, 340.f}; return T[tclamp(vd, 0, 3)]; }
inline int debrisBudget(int q) { static const int T[4] = {120, 260, 500, 900}; return T[tclamp(q, 0, 3)]; }
inline int particleBudget(int q) { static const int T[4] = {600, 1500, 3000, 5000}; return T[tclamp(q, 0, 3)]; }

} // namespace ft
