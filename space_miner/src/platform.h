// ============================================================
// SPACE MINER  -  platform.h
// Host abstraction: window, input pump, audio.
// ============================================================
#pragma once
#include "core.h"
#include "input.h"
#include "gfx.h"

namespace sm {

bool  platformInit(const char* title, int w, int h);   // returns false on fatal error
void  platformShutdown();
// Pumps events. Returns false when the user closed the window.
bool  platformPump(Input& in, float& dt);
void  platformPresent(Canvas& fb);                     // blits the virtual canvas to the window
void  platformSetTitle(const char* title);
void  platformSetIcon(int size, const uint32_t* argb);
void  platformSetBorderless(bool on);                   // windowed <-> borderless fullscreen
const char* platformSaveDir();                         // directory for save files (with trailing sep)
void  platformSleepMs(int ms);
double platformSeconds();

// ------------------------------------------------------------ audio (synth)
namespace audio {
void init();
void shutdown();
void setMasterVolume(float v);
float masterVolume();
// one shot sound
void playLaser();
void playMining();
void playExplosion(float size);
void playPickup();
void playHit();
void playUiClick();
void playUiHover();
void playSell();
void playUpgrade();
void playAlarm();
void playWarp();
void playEnemyShot();
void playDroneBeep();
void update(float dt);          // called each frame from the game loop
void setMusicIntensity(float v);
void setAmbient(float sectorMood);
// Pulls synthesised audio for the platform's output callback.
void render(float* interleaved, int frames, int channels);
}  // namespace audio

}  // namespace sm
