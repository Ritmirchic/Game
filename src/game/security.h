// FRACTURE — security systems: sweeping cameras, sentry turrets, alarms.
#pragma once
#include "core/engine.h"
#include "world/voxel.h"
#include "world/physics.h"

struct Player;
#include <vector>

struct SecurityCamera {
    vec3 pos{0, 4, 0};
    vec3 mountDir{0, 0, 1};
    float baseYaw = 0;
    float yaw = 0;
    float sweep = 0.9f;      // radians either side
    float sweepSpeed = 0.55f;
    float phase = 0;
    float viewRange = 20.0f;
    float fov = 0.55f;       // half-angle
    float suspicion = 0;     // 0..1
    float health = 40;
    bool destroyed = false;
    bool alert = false;
    float sparkTimer = 0;
};

struct SecurityTurret {
    vec3 pos{0, 3, 0};
    float yaw = 0;
    float targetYaw = 0;
    float pitch = 0;
    float health = 120;
    bool destroyed = false;
    bool hostile = false;
    float fireCooldown = 0;
    float muzzleFlash = 0;
    float retract = 0;       // 0 deployed, 1 stowed
    float sparkTimer = 0;
};

class SecuritySystem {
public:
    std::vector<SecurityCamera> cameras;
    std::vector<SecurityTurret> turrets;
    World* world = nullptr;
    Physics* physics = nullptr;
    bool alarm = false;
    float alarmTime = 0;
    float alarmDuration = 90.0f;   // seconds until the lockdown fails the job
    float alarmBlink = 0;
    int detections = 0;
    float lastShotTime = 0;
    int shotsFired = 0;

    void init(World* w, Physics* p);
    void clear();
    void addCamera(const vec3& pos, float yaw);
    void addTurret(const vec3& pos, float yaw);
    void placeForTargets(const std::vector<std::pair<vec3, vec3>>& boxes);  // min/max corners
    void update(float dt, const vec3& playerPos, bool playerVisible, Player* player);
    void resetAlarm();
    // damage from explosions / heavy impacts; returns true if something broke
    bool damageArea(const vec3& center, float radius, float power);
    // a tool or bullet hit test: returns true if the ray hit a device
    bool damageRay(const RayHit& hit, float power);
    void appendBoxes(std::vector<BoxInstance>& out, const vec3& camPos) const;
    int aliveCameras() const;
    int aliveTurrets() const;
    bool anyAlert() const;
    void triggerAlarm(const vec3& pos);
    vec3 lastAlarmPos;
};
