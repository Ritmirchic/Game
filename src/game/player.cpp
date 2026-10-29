#include "game/player.h"
#include "game/fx.h"
#include "game/globals.h"
#include "core/engine.h"
#include <cmath>


void Player::addShake(float amount) {
    shake = std::min(1.6f, shake + amount);
}

void Player::damage(float amount, float gameTime) {
    if (amount <= 0) return;
    health -= amount;
    lastDamage = gameTime;
    addShake(std::min(0.9f, amount * 0.05f));
    if (health <= 0) {
        health = 0;
        dead = true;
    }
}
void Player::heal(float a) { health = std::min(maxHealth, health + a); }

void Player::moveWithInput(InputState& in, World& world, Physics& phys, float dt) {
    if (noclip) {
        return;
    }
    vec3 fwd(std::sin(yaw), 0, -std::cos(yaw));
    vec3 right(std::cos(yaw), 0, std::sin(yaw));
    vec3 wish(0, 0, 0);
    if (in.isDown(K_W)) wish += fwd;
    if (in.isDown(K_S)) wish -= fwd;
    if (in.isDown(K_D)) wish += right;
    if (in.isDown(K_A)) wish -= right;
    float wl = wish.len();
    if (wl > 0.01f) wish = wish / wl;

    crouching = in.isDown(K_CONTROL) || in.isDown(K_C);
    sprinting = in.isDown(K_SHIFT) && !crouching && wl > 0.1f;
    float speed = crouching ? 2.35f : (sprinting ? 6.7f : 4.35f);
    if (carried) speed *= 0.82f;
    // inertia: accelerate toward the wish velocity
    float accel = grounded ? 42.0f : 9.0f;
    vec3 targetVel = wish * speed;
    vec3 d = targetVel - vec3(vel.x, 0, vel.z);
    float dl = d.len();
    if (dl > accel * dt) d = d * (accel * dt / dl);
    vel.x += d.x;
    vel.z += d.z;
    if (wl < 0.01f && grounded) {
        float fr = 1.0f - 12.0f * dt;
        if (fr < 0) fr = 0;
        vel.x *= fr;
        vel.z *= fr;
    }
    // jump
    if (in.isPressed(K_SPACE) && grounded) {
        vel.y = 5.35f;
        grounded = false;
        audio::play(SFX_JUMP, pos, 0.4f, 0.9f + hash3f((int)(g_time * 10), 1, 1) * 0.2f);
    }
    vel.y -= 19.6f * dt;
    if (vel.y < -45) vel.y = -45;

    float prevY = pos.y;
    bool wasGrounded = grounded;
    bool hitCeiling = false;
    float hgt = crouching ? 1.18f : height;
    phys.moveCapsule(pos, vel, radius, hgt, dt, grounded, hitCeiling);
    if (!wasGrounded && grounded && vel.y <= 0.01f) {
        float fall = std::max(0.0f, (prevY - pos.y));
        if (fall > 0.35f) {
            landDip = std::min(0.22f, fall * 0.055f);
            addShake(std::min(0.35f, fall * 0.035f));
            audio::play(SFX_LAND, pos, std::min(0.75f, 0.25f + fall * 0.05f), 0.95f + hash3f((int)g_time, 3, 3) * 0.12f);
            if (fall > 9.0f) damage((fall - 9.0f) * 6.5f, g_time);
            if (g_fx) g_fx->dustBurst(pos, 4, 0.3f, 0.35f, vec3(0.55f, 0.52f, 0.46f));
        }
    }
    // vehicle push-out
    if (g_physics) {
        for (auto& b : g_physics->bodies) {
            if (!b.active) continue;
            vec3 dp = pos + vec3(0, 0.9f, 0) - b.pos;
            if (std::fabs(dp.y) > 1.6f) continue;
            if (dp.x * dp.x + dp.z * dp.z < 1.0f + b.half.x * b.half.x) {
                float push = 1.0f;
                vec3 dir(dp.x, 0, dp.z);
                if (dir.len2() < 0.01f) dir = vec3(1, 0, 0);
                pos += dir.norm() * push * 0.02f;
            }
        }
    }
    // footsteps
    if (grounded) {
        float horiz = std::sqrt(vel.x * vel.x + vel.z * vel.z);
        stepDistance += horiz * dt;
        float stride = sprinting ? 2.05f : (crouching ? 1.35f : 1.72f);
        if (stepDistance > stride) {
            stepDistance = 0;
            int vx = (int)std::floor(pos.x / VOX), vz = (int)std::floor(pos.z / VOX);
            int gy = (int)std::floor((pos.y - 0.05f) / VOX);
            uint8_t m = M_CONCRETE;
            for (int dy = 0; dy < 3; dy++) {
                uint8_t t = world.at(vx, gy - dy, vz);
                if (matSolid(t)) { m = t; break; }
            }
            playFootstepSound(m, pos, crouching ? 0.16f : 0.34f);
            bobPhase += 1.0f;
        }
    }
}

void Player::updateInput(InputState& in, World& world, Physics& phys, float dt, bool uiBlocking) {
    if (uiBlocking) {
        // keep physics ticking but ignore movement input
        vel.x *= 0.85f;
        vel.z *= 0.85f;
        if (!noclip) {
            vel.y -= 19.6f * dt;
            float hgt = crouching ? 1.18f : height;
            bool hitCeiling = false;
            phys.moveCapsule(pos, vel, radius, hgt, dt, grounded, hitCeiling);
        }
        updateCamera(dt);
        return;
    }
    float sens = 0.0022f;
    float mdx = in.mouseDX, mdy = in.mouseDY;
    yaw += mdx * sens;
    pitch -= mdy * sens;
    pitch = clampf(pitch, -1.52f, 1.52f);
    recoilPitch *= (1.0f - 9.0f * dt);
    recoilYaw *= (1.0f - 9.0f * dt);
    if (noclip) {
        vec3 fwd(std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw));
        vec3 right(std::cos(yaw), 0, std::sin(yaw));
        vec3 wish(0, 0, 0);
        if (in.isDown(K_W)) wish += fwd;
        if (in.isDown(K_S)) wish -= fwd;
        if (in.isDown(K_D)) wish += right;
        if (in.isDown(K_A)) wish -= right;
        if (in.isDown(K_SPACE)) wish.y += 1;
        if (in.isDown(K_CONTROL)) wish.y -= 1;
        float sp = in.isDown(K_SHIFT) ? 26.0f : 9.0f;
        pos += wish.norm() * sp * dt;
    } else {
        moveWithInput(in, world, phys, dt);
    }
    if (in.isPressed(K_F)) {
        flashlightOn = !flashlightOn;
        audio::play(SFX_UI_CLICK, pos, 0.3f, 1.4f);
    }
    updateCamera(dt);
}


void Player::updateCamera(float dt) {
    bobPhase += std::sqrt(vel.x * vel.x + vel.z * vel.z) * dt * 1.35f;
    float speed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
    float targetBob = grounded ? std::min(1.0f, speed / 5.0f) : 0.0f;
    bobAmount = lerpf(bobAmount, targetBob, dt * 8.0f);
    breathe += dt;
    landDip = lerpf(landDip, 0, dt * 9.0f);
    shake *= std::exp(-6.5f * dt);
    shakeSeed += dt * 37.0f;
    shakeOffset = vec3(std::sin(shakeSeed * 1.7f), std::sin(shakeSeed * 2.3f + 1.2f), std::sin(shakeSeed * 1.1f + 2.4f)) *
                  (shake * 0.11f);
    float targetHeight = (crouching ? 1.15f : 1.66f) + std::sin(bobPhase * PI) * 0.045f * bobAmount - landDip +
                         std::sin(breathe * 1.4f) * 0.008f;
    cameraHeight = lerpf(cameraHeight, targetHeight, clampf(dt * 14.0f, 0, 1));
    roll = lerpf(roll, -vel.x * 0.0025f * std::cos(yaw) + vel.z * 0.0025f * std::sin(yaw), dt * 6.0f);
    fovTarget = sprinting ? 84.0f : 78.0f;
    fov = lerpf(fov, fovTarget, dt * 6.0f);
    // tool sway follows the mouse
    toolSwayX = lerpf(toolSwayX, clampf(-g_lastMouseDX * 0.00055f, -0.05f, 0.05f), dt * 9.0f);
    toolSwayY = lerpf(toolSwayY, clampf(g_lastMouseDY * 0.00055f, -0.05f, 0.05f), dt * 9.0f);
    toolBobX = std::sin(bobPhase * PI) * 0.012f * bobAmount;
    toolBobY = std::fabs(std::cos(bobPhase * PI)) * 0.014f * bobAmount;
    equipAnim = std::min(1.0f, equipAnim + dt * 3.0f);
}

Camera Player::makeCamera(float aspect) const {
    Camera c;
    c.pos = pos + vec3(0, cameraHeight, 0) + shakeOffset;
    c.yaw = yaw + recoilYaw;
    c.pitch = pitch + recoilPitch;
    c.fovY = fov * DEG2RAD;
    c.aspect = aspect;
    c.zn = 0.04f;
    c.zf = 900.0f;
    c.roll = roll;
    return c;
}

bool Player::interact(World& world, Physics& phys) {
    if (carried) {
        // drop
        carried->active = true;
        carried->vel = vec3(0, -1, 0);
        audio::play(SFX_OBJECT_PICKUP, pos, 0.35f, 0.8f);
        carried = nullptr;
        return true;
    }
    vec3 eye = pos + vec3(0, cameraHeight, 0);
    vec3 dir(std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw));
    Body* b = phys.nearestProp(eye + dir * 1.4f, 1.6f);
    if (b && b->mass < 140.0f) {
        carried = b;
        b->vel = vec3(0);
        b->angVel = vec3(0);
        b->sleeping = true;
        audio::play(SFX_OBJECT_PICKUP, pos, 0.45f, 1.0f);
        return true;
    }
    return false;
}
