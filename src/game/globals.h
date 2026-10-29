// FRACTURE — shared singletons used across gameplay systems.
#pragma once
#include "core/math.h"
#include "core/engine.h"

class FX;
class Physics;
struct Player;
struct Vehicle;

extern float g_time;
extern float g_gameTime;         // campaign timer (0 = infinite)
extern FX* g_fx;
extern Physics* g_physics;
extern Player* g_playerRef;
extern float g_lastMouseDX, g_lastMouseDY;
extern void explodeAt(const vec3& center, float radius, int source);   // implemented in game.cpp
extern void playFootstepSound(uint8_t mat, const vec3& pos, float vol);
extern void carveWorld(const vec3& center, float radius, float power, bool structural, bool ignite);
