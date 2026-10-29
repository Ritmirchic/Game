// FRACTURE — first-person player controller.
#pragma once
#include "core/engine.h"
#include "world/physics.h"

struct Vehicle;

struct Player {
    vec3 pos{0, 3, 0};
    vec3 vel{0, 0, 0};
    float yaw = 0, pitch = 0;
    float height = 1.78f;
    float radius = 0.33f;
    bool grounded = false;
    bool crouching = false;
    bool sprinting = false;
    bool noclip = false;
    float eyeHeight() const { return crouching ? 1.15f : 1.66f; }
    float cameraHeight = 1.66f;
    float bobPhase = 0;
    float bobAmount = 0;
    float stepDistance = 0;
    float breathe = 0;
    float roll = 0;
    float landDip = 0;
    float shake = 0;
    float shakeSeed = 0;
    vec3 shakeOffset;
    float recoilPitch = 0, recoilYaw = 0;
    float swingTilt = 0;
    float fov = 78;
    float fovTarget = 78;
    float health = 100, maxHealth = 100;
    float lastDamage = -100;
    bool dead = false;
    bool flashlightOn = false;
    float flashlightBattery = 1.0f;
    // interaction
    RayHit aimHit;
    bool aimValid = false;
    Body* carried = nullptr;
    float carryDist = 1.9f;
    Vehicle* vehicle = nullptr;      // vehicle being driven
    float vehicleEnterCooldown = 0;
    vec3 lastGroundPos;
    // tool view model animation state
    float toolBobX = 0, toolBobY = 0, toolSwayX = 0, toolSwayY = 0;
    float equipAnim = 1.0f;
    int lastFootstepMat = 0;
    float footstepTimer = 0;

    void updateInput(InputState& in, World& world, Physics& phys, float dt, bool uiBlocking);
    void updateCamera(float dt);
    Camera makeCamera(float aspect) const;
    void addShake(float amount);
    void damage(float amount, float gameTime);
    void heal(float amount);
    bool interact(World& world, Physics& phys);
    void moveWithInput(InputState& in, World& world, Physics& phys, float dt);
};
