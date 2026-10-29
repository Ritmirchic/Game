#include "world/physics.h"
#include "core/engine.h"
#include <cmath>
#include <algorithm>


static inline float clampv(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

void Physics::init(World* w) {
    world = w;
    bodies.clear();
    bodies.reserve(512);
    time = 0;
}

void Physics::clear() {
    bodies.clear();
    impactEvents.clear();
}

int Physics::spawn(vec3 pos, vec3 half, uint8_t mat, float mass, vec3 vel, vec3 angVel, float life, bool heavy) {
    // reuse an inactive slot
    int idx = -1;
    for (int i = 0; i < (int)bodies.size(); i++)
        if (!bodies[i].active) { idx = i; break; }
    if (idx < 0) {
        if ((int)bodies.size() >= maxBodies) {
            // replace the oldest far debris
            idx = 0;
            for (int i = 0; i < (int)bodies.size(); i++)
                if (bodies[i].life > 0 && bodies[i].life < bodies[idx].life) idx = i;
        } else {
            bodies.push_back(Body());
            idx = (int)bodies.size() - 1;
        }
    }
    Body& b = bodies[idx];
    b = Body();
    b.active = true;
    b.pos = pos;
    b.vel = vel;
    b.angVel = angVel;
    b.rot = quatFromEuler(((int)(hash32(idx * 7919 + (int)(time * 1000)) % 628)) / 100.0f,
                          ((int)(hash32(idx * 104729 + (int)(time * 977)) % 628)) / 100.0f,
                          ((int)(hash32(idx * 1299709 + (int)(time * 31)) % 628)) / 100.0f);
    b.half = half;
    b.mat = mat;
    b.mass = std::max(0.4f, mass);
    b.invMass = 1.0f / b.mass;
    // box inertia (diagonal)
    vec3 s = half * 2.0f;
    vec3 I(b.mass / 12.0f * (s.y * s.y + s.z * s.z), b.mass / 12.0f * (s.x * s.x + s.z * s.z),
           b.mass / 12.0f * (s.x * s.x + s.y * s.y));
    b.invI = vec3(1.0f / std::max(0.0001f, I.x), 1.0f / std::max(0.0001f, I.y), 1.0f / std::max(0.0001f, I.z));
    const Material& mt = material(mat);
    b.restitution = (mat == M_GLASS || mat == M_RUBBER) ? 0.28f : 0.14f;
    b.friction = 0.72f + (mt.roughness * 0.2f);
    b.life = life;
    b.born = time;
    b.heavy = heavy;
    b.color = mt.color;
    updateModel(b);
    stats.spawnedTotal++;
    return idx;
}

void Physics::updateModel(Body& b) {
    b.model = matTRS(b.pos, b.rot, b.half * 2.0f);
    vec3 ex(std::fabs(b.model.at(0,0)) * 0.5f + std::fabs(b.model.at(1,0)) * 0.5f + std::fabs(b.model.at(2,0)) * 0.5f,
            std::fabs(b.model.at(0,1)) * 0.5f + std::fabs(b.model.at(1,1)) * 0.5f + std::fabs(b.model.at(2,1)) * 0.5f,
            std::fabs(b.model.at(0,2)) * 0.5f + std::fabs(b.model.at(1,2)) * 0.5f + std::fabs(b.model.at(2,2)) * 0.5f);
    // model already includes scale, so compose with the unit-cube extents
    b.box = AABB(b.pos - ex, b.pos + ex);
}

void Physics::spawnDebrisBurst(const vec3& center, const AABB& area, const int* matCounts, int quality, float power) {
    // decide how many chunks to emit based on destroyed voxel material mix
    int totalVox = 0;
    int dominant = M_CONCRETE;
    int bestCount = 0;
    for (int m = 1; m < M_COUNT; m++) {
        totalVox += matCounts[m];
        if (matCounts[m] > bestCount) { bestCount = matCounts[m]; dominant = m; }
    }
    if (totalVox <= 0) return;
    static const int extraByQuality[4] = {0, 3, 7, 12};
    int n = std::min(20, 2 + totalVox / 26 + extraByQuality[clampi(quality, 0, 3)]);
    vec3 size = area.size();
    const Material& mt = material((uint8_t)dominant);
    for (int i = 0; i < n; i++) {
        vec3 p(hash3f(i, 1, 7, 3) * size.x + area.mn.x, hash3f(i, 2, 9, 4) * size.y + area.mn.y,
               hash3f(i, 3, 11, 5) * size.z + area.mn.z);
        if (!area.contains(p)) p = center;
        float scale = (dominant == M_GLASS) ? 0.035f : (0.05f + hash3f(i, 5, 5, 6) * 0.10f);
        vec3 half(scale * (0.7f + hash3f(i, 7, 1, 7) * 0.8f), scale * (0.6f + hash3f(i, 8, 2, 8) * 0.7f),
                  scale * (0.7f + hash3f(i, 9, 3, 9) * 0.8f));
        vec3 dir = (p - center);
        float dl = std::max(0.05f, dir.len());
        vec3 v = dir / dl * (1.2f + hash3f(i, 11, 11, 11) * 3.4f) * clampf(power * 0.25f, 0.4f, 3.0f);
        v.y += 1.0f + hash3f(i, 13, 13, 13) * 3.0f;
        float density = mt.density;
        float vol = 8 * half.x * half.y * half.z;
        float mass = std::max(0.35f, vol * density);
        int b = spawn(p, half, (uint8_t)dominant, mass, v,
                      vec3((hash3f(i,17,2,1)-0.5f)*8, (hash3f(i,19,3,1)-0.5f)*8, (hash3f(i,23,5,1)-0.5f)*8),
                      14.0f + hash3f(i, 29, 7, 2) * 10.0f, false);
        bodies[b].life = 16.0f + hash3f(i, 31, 11, 3) * 10.0f;
    }
    // spray a light dust of micro-debris for glass
    if (dominant == M_GLASS && quality >= 1) {
        for (int i = 0; i < 5; i++) {
            vec3 p = center + vec3((hash3f(i,41,1,1)-0.5f)*0.6f, (hash3f(i,43,1,1)-0.5f)*0.6f, (hash3f(i,47,1,1)-0.5f)*0.6f);
            int b = spawn(p, vec3(0.02f), M_GLASS, 0.05f,
                          vec3((hash3f(i,53,1,1)-0.5f)*7, hash3f(i,59,1,1)*6, (hash3f(i,61,1,1)-0.5f)*7), vec3(2, 2, 2), 6.0f, false);
            bodies[b].restitution = 0.35f;
        }
    }
}

void Physics::spawnCollapsePiece(const vec3& pos, const vec3& half, uint8_t mat, const vec3& push) {
    const Material& mt = material(mat);
    float vol = 8 * half.x * half.y * half.z;
    float mass = std::max(20.0f, vol * mt.density * 0.55f);
    int b = spawn(pos, half, mat, mass, push, vec3((hash3f((int)pos.x, 1, 1) - 0.5f) * 2.5f, (hash3f((int)pos.y, 2, 2) - 0.5f) * 2.5f,
                                                   (hash3f((int)pos.z, 3, 3) - 0.5f) * 2.5f), -1, true);
    bodies[b].friction = 0.85f;
    bodies[b].restitution = 0.05f;
}

void Physics::skipVoxels(int, int, int, int, int, int) {}

void Physics::applyRadialImpulse(const vec3& center, float radius, float strength) {
    for (auto& b : bodies) {
        if (!b.active) continue;
        vec3 d = b.pos - center;
        float dist = d.len();
        if (dist > radius) continue;
        float f = 1.0f - dist / radius;
        f = f * f * strength * b.invMass * 40.0f;
        vec3 dir = dist > 0.01f ? d / dist : vec3(0, 1, 0);
        b.vel += dir * f + vec3(0, f * 0.35f, 0);
        b.angVel += vec3((hash3f((int)b.pos.x, 1, 5) - 0.5f) * f * 12, (hash3f((int)b.pos.y, 2, 6) - 0.5f) * f * 12,
                         (hash3f((int)b.pos.z, 3, 7) - 0.5f) * f * 12);
        b.sleeping = false;
        b.sleepTimer = 0;
    }
}

void Physics::wakeAll() {
    for (auto& b : bodies) { b.sleeping = false; b.sleepTimer = 0; }
}

void Physics::update(float dt) {
    time += dt;
    dt = std::min(dt, 0.05f);
    int substeps = 2;
    float h = dt / substeps;
    for (int s = 0; s < substeps; s++) {
        integrate(h);
        for (auto& b : bodies) if (b.active && !b.sleeping) collideVoxels(b, h);
        collideBodies(h);
    }
    // sleep + lifetime management
    int active = 0, sleeping = 0;
    for (auto& b : bodies) {
        if (!b.active) continue;
        b.hitCooldown -= dt;
        float speed2 = b.vel.len2() + b.angVel.len2() * 0.05f;
        if (b.sleeping) {
            sleeping++;
        } else {
            active++;
            if (speed2 < 0.045f) {
                b.sleepTimer += dt;
                if (b.sleepTimer > 0.7f) { b.sleeping = true; b.vel = vec3(0); b.angVel = vec3(0); }
            } else {
                b.sleepTimer = 0;
            }
        }
        if (b.life > 0) {
            b.life -= dt;
            if (b.life <= 0) {
                b.active = false;
                continue;
            }
            // fade out by shrinking (avoids a pop)
            if (b.life < 0.9f) {
                updateModel(b);
            }
        }
        if (b.pos.y < -20.0f) b.active = false;
    }
    stats.activeBodies = active;
    stats.sleepingBodies = sleeping;
    // hard cap: retire the farthest sleeping tiny bodies
    int live = 0;
    for (auto& b : bodies) if (b.active) live++;
    if (live > maxBodies) {
        int toKill = live - maxBodies;
        for (auto& b : bodies) {
            if (toKill <= 0) break;
            if (b.active && b.sleeping && b.life > 0 && !b.heavy) { b.active = false; toKill--; }
        }
    }
}

void Physics::integrate(float h) {
    for (auto& b : bodies) {
        if (!b.active || b.sleeping) continue;
        if (gravityEnabled) b.vel.y -= 9.81f * gravityScale * h;
        // air drag
        b.vel *= (1.0f - 0.045f * h);
        b.angVel *= (1.0f - 0.6f * h);
        // clamp extreme velocities for stability
        float vl = b.vel.len();
        if (vl > 60.0f) b.vel *= 60.0f / vl;
        float al = b.angVel.len();
        if (al > 40.0f) b.angVel *= 40.0f / al;
        b.pos += b.vel * h;
        float angle = b.angVel.len() * h;
        if (angle > 1e-5f) {
            quat dq = quatAxisAngle(b.angVel.norm(), angle);
            b.rot = dq * b.rot;
            float len = std::sqrt(b.rot.x*b.rot.x + b.rot.y*b.rot.y + b.rot.z*b.rot.z + b.rot.w*b.rot.w);
            if (len > 1e-6f) { b.rot.x /= len; b.rot.y /= len; b.rot.z /= len; b.rot.w /= len; }
        }
        updateModel(b);
    }
}

// contact impulse at a point with normal n
static void applyContact(Body& b, const vec3& point, const vec3& n, float pen, float restitution, float friction,
                         bool& grounded) {
    if (b.invMass <= 0) return;
    vec3 r = point - b.pos;
    vec3 rn = cross(r, n);
    // local-space inverse inertia applied to world vector
    mat4 rot = matFromQuat(b.rot);
    vec3 local = vec3(dot(rot.at(0,0), rn.x) + rot.at(0,1) * rn.y + rot.at(0,2) * rn.z, 0, 0);
    (void)local;
    // transform rn into local space, scale by inverse inertia, back to world
    vec3 rnLocal(dot(rn, vec3(rot.at(0,0), rot.at(0,1), rot.at(0,2))),
                 dot(rn, vec3(rot.at(1,0), rot.at(1,1), rot.at(1,2))),
                 dot(rn, vec3(rot.at(2,0), rot.at(2,1), rot.at(2,2))));
    rnLocal = rnLocal * b.invI;
    vec3 angTerm(dot(rnLocal, vec3(rot.at(0,0), rot.at(0,1), rot.at(0,2))),
                 dot(rnLocal, vec3(rot.at(1,0), rot.at(1,1), rot.at(1,2))),
                 dot(rnLocal, vec3(rot.at(2,0), rot.at(2,1), rot.at(2,2))));
    float denom = b.invMass + dot(angTerm, rn);
    if (denom < 1e-8f) return;
    vec3 velAtPoint = b.vel + cross(b.angVel, r);
    float vn = dot(velAtPoint, n);
    if (vn > 0) return;   // separating
    float e = restitution;
    if (std::fabs(vn) < 1.2f) e = 0;   // no bounce for resting contacts
    float jn = -(1.0f + e) * vn / denom;
    vec3 impulse = n * jn;
    b.vel += impulse * b.invMass;
    b.angVel += vec3(angTerm.x * jn * 0, 0, 0);   // rotational term handled below
    // angular: dOmega = I^-1 (r x J)
    vec3 torque = cross(r, impulse);
    vec3 tLocal(dot(torque, vec3(rot.at(0,0), rot.at(0,1), rot.at(0,2))),
                dot(torque, vec3(rot.at(1,0), rot.at(1,1), rot.at(1,2))),
                dot(torque, vec3(rot.at(2,0), rot.at(2,1), rot.at(2,2))));
    tLocal = tLocal * b.invI;
    b.angVel += vec3(dot(tLocal, vec3(rot.at(0,0), rot.at(0,1), rot.at(0,2))),
                     dot(tLocal, vec3(rot.at(1,0), rot.at(1,1), rot.at(1,2))),
                     dot(tLocal, vec3(rot.at(2,0), rot.at(2,1), rot.at(2,2))));
    // friction
    vec3 vt = velAtPoint - n * vn;
    float vtLen = vt.len();
    if (vtLen > 1e-4f) {
        vec3 tdir = vt / vtLen;
        float jt = -vtLen / denom;
        float maxFric = friction * std::fabs(jn);
        jt = clampv(jt, -maxFric, maxFric);
        vec3 fImp = tdir * jt;
        b.vel += fImp * b.invMass;
        vec3 fTorque = cross(r, fImp);
        vec3 fLocal(dot(fTorque, vec3(rot.at(0,0), rot.at(0,1), rot.at(0,2))),
                    dot(fTorque, vec3(rot.at(1,0), rot.at(1,1), rot.at(1,2))),
                    dot(fTorque, vec3(rot.at(2,0), rot.at(2,1), rot.at(2,2))));
        fLocal = fLocal * b.invI;
        b.angVel += vec3(dot(fLocal, vec3(rot.at(0,0), rot.at(0,1), rot.at(0,2))),
                         dot(fLocal, vec3(rot.at(1,0), rot.at(1,1), rot.at(1,2))),
                         dot(fLocal, vec3(rot.at(2,0), rot.at(2,1), rot.at(2,2))));
    }
    // positional correction
    b.pos += n * (pen * 0.85f);
    if (n.y > 0.55f) grounded = true;
}

void Physics::collideVoxels(Body& b, float h) {
    (void)h;
    mat4 M = matFromQuat(b.rot);
    // 8 corners plus face centres for better resting behaviour
    static const vec3 localPts[14] = {
        {-1, -1, -1}, {1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {-1, 1, 1}, {1, 1, 1},
        {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}};
    bool grounded = false;
    float deepest = 0;
    vec3 deepestN(0, 1, 0);
    vec3 deepestP;
    for (int i = 0; i < 14; i++) {
        vec3 lp = vec3(localPts[i].x * b.half.x, localPts[i].y * b.half.y, localPts[i].z * b.half.z);
        vec3 wp = b.pos + M.dir(lp);
        float invVox = 1.0f / VOX;
        int vx = (int)std::floor(wp.x * invVox), vy = (int)std::floor(wp.y * invVox), vz = (int)std::floor(wp.z * invVox);
        uint8_t m = world->at(vx, vy, vz);
        if (!matSolid(m)) continue;
        // find the nearest free face of this voxel
        vec3 local = wp - vec3((float)vx * VOX, (float)vy * VOX, (float)vz * VOX);
        float dx0 = local.x, dx1 = VOX - local.x;
        float dy0 = local.y, dy1 = VOX - local.y;
        float dz0 = local.z, dz1 = VOX - local.z;
        float pen = dx0;
        vec3 n(-1, 0, 0);
        if (dx1 < pen) { pen = dx1; n = vec3(1, 0, 0); }
        if (dy0 < pen) { pen = dy0; n = vec3(0, -1, 0); }
        if (dy1 < pen) { pen = dy1; n = vec3(0, 1, 0); }
        if (dz0 < pen) { pen = dz0; n = vec3(0, 0, -1); }
        if (dz1 < pen) { pen = dz1; n = vec3(0, 0, 1); }
        // prefer the direction the body is moving from (prevents tunnelling through thin walls)
        vec3 vDir = b.vel.len2() > 0.01f ? b.vel.norm() : vec3(0, -1, 0);
        if (dot(n, vDir) > 0.5f) {
            // we're pushing into this face: flip to the opposite if that is also a valid exit
            vec3 alt = -n;
            float altPen = VOX - pen;
            if (altPen < 0.30f) { n = alt; pen = altPen; }
        }
        if (pen > deepest) { deepest = pen; deepestN = n; deepestP = wp; }
        applyContact(b, wp, n, pen, b.restitution, b.friction, grounded);
        // impact effects
        if (b.hitCooldown <= 0 && b.vel.len() > 7.5f && b.mass > 3.0f) {
            b.hitCooldown = 0.2f;
            impactEvents.push_back((int)(&b - &bodies[0]));
            if (b.heavy) {
                // heavy structural pieces smash the voxels they hit
                World::CarveParams cp;
                cp.centerM = wp;
                cp.radiusM = 0.35f + std::min(1.4f, b.mass / 900.0f);
                cp.power = 6.0f + std::min(60.0f, b.mass / 90.0f);
                cp.damageMul = 1.0f;
                world->carve(cp);
            }
        }
    }
    if (grounded) {
        b.sleepTimer += 0.02f;
    }
    if (deepest > 0.0f && std::fabs(deepestN.y) < 0.4f) {
        // reduce horizontal spin when scraping a wall
        b.angVel *= 0.92f;
    }
}

void Physics::collideBodies(float h) {
    (void)h;
    int n = (int)bodies.size();
    if (n > 400) n = 400;
    // broadphase: uniform grid buckets of 1m
    static const float CELL = 1.0f;
    static std::vector<std::vector<int>> grid;
    static const int GDIM = 48;
    if ((int)grid.size() != GDIM * GDIM * GDIM) grid.assign(GDIM * GDIM * GDIM, {});
    for (auto& g : grid) g.clear();
    auto cellOf = [&](const vec3& p, int& x, int& y, int& z) {
        x = iclamp((int)std::floor(p.x / CELL) + GDIM / 2, 0, GDIM - 1);
        y = iclamp((int)std::floor(p.y / CELL), 0, GDIM - 1);
        z = iclamp((int)std::floor(p.z / CELL) + GDIM / 2, 0, GDIM - 1);
    };
    for (int i = 0; i < n; i++) {
        Body& b = bodies[i];
        if (!b.active || b.sleeping) continue;
        int cx, cy, cz;
        cellOf(b.pos, cx, cy, cz);
        grid[(cz * GDIM + cy) * GDIM + cx].push_back(i);
    }
    for (int i = 0; i < n; i++) {
        Body& a = bodies[i];
        if (!a.active || a.sleeping) continue;
        int cx, cy, cz;
        cellOf(a.pos, cx, cy, cz);
        for (int dz = -1; dz <= 1; dz++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int gx = cx + dx, gy = cy + dy, gz = cz + dz;
                    if (gx < 0 || gy < 0 || gz < 0 || gx >= GDIM || gy >= GDIM || gz >= GDIM) continue;
                    for (int j : grid[(gz * GDIM + gy) * GDIM + gx]) {
                        if (j <= i) continue;
                        Body& b = bodies[j];
                        if (!b.active) continue;
                        vec3 d = b.pos - a.pos;
                        float rad = a.half.len() * 0.9f + b.half.len() * 0.9f;
                        float dist2 = d.len2();
                        if (dist2 > rad * rad || dist2 < 1e-8f) continue;
                        float dist = std::sqrt(dist2);
                        vec3 n = d / dist;
                        float pen = rad - dist;
                        float totalInv = a.invMass + b.invMass;
                        if (totalInv <= 0) continue;
                        vec3 rv = b.vel - a.vel;
                        float vn = dot(rv, n);
                        if (vn < 0) {
                            float e = 0.12f;
                            float jn = -(1 + e) * vn / totalInv;
                            vec3 imp = n * jn;
                            a.vel -= imp * a.invMass;
                            b.vel += imp * b.invMass;
                            // friction
                            vec3 vt = rv - n * vn;
                            float vtl = vt.len();
                            if (vtl > 1e-4f) {
                                float jt = clampv(-vtl / totalInv, -0.55f * jn, 0.55f * jn);
                                vec3 fi = (vt / vtl) * jt;
                                a.vel -= fi * a.invMass;
                                b.vel += fi * b.invMass;
                            }
                            a.angVel += vec3((hash3f(i, 1, 1) - 0.5f) * 2, 0, (hash3f(i, 2, 2) - 0.5f) * 2) * (jn * 0.02f);
                            b.angVel += vec3((hash3f(j, 1, 1) - 0.5f) * 2, 0, (hash3f(j, 2, 2) - 0.5f) * 2) * (jn * 0.02f);
                        }
                        // separate
                        vec3 corr = n * (pen * 0.5f / totalInv);
                        a.pos -= corr * a.invMass * 0.7f;
                        b.pos += corr * b.invMass * 0.7f;
                        // wake sleepers
                        if (a.sleeping && !b.sleeping) a.sleeping = false;
                        if (b.sleeping && !a.sleeping) b.sleeping = false;
                    }
                }
    }
}

// ---------------------------------------------------------------- player capsule
static bool voxelSolidAt(const World& w, float x, float y, float z) {
    return w.solidAt((int)std::floor(x / VOX), (int)std::floor(y / VOX), (int)std::floor(z / VOX));
}

bool Physics::capsuleBlocked(const vec3& pos, float radius, float height) {
    // sample a grid of points around the capsule
    for (int i = 0; i <= 3; i++) {
        float y = pos.y + 0.05f + (height - 0.1f) * (i / 3.0f);
        for (int k = 0; k < 8; k++) {
            float a = k * PI / 4;
            float x = pos.x + std::cos(a) * radius;
            float z = pos.z + std::sin(a) * radius;
            if (voxelSolidAt(*world, x, y, z)) return true;
        }
        if (voxelSolidAt(*world, pos.x, y, pos.z)) return true;
    }
    return false;
}

float Physics::capsuleGround(const vec3& pos, float radius) {
    float best = -1000;
    for (int k = 0; k < 5; k++) {
        float a = k * PI * 2 / 5;
        float x = pos.x + std::cos(a) * radius * 0.7f;
        float z = pos.z + std::sin(a) * radius * 0.7f;
        float g = world->groundHeightM(x, z);
        best = std::max(best, g);
    }
    return best;
}

bool Physics::moveCapsule(vec3& pos, vec3& vel, float radius, float height, float dt, bool& grounded, bool& hitCeiling) {
    grounded = false;
    hitCeiling = false;
    vec3 delta = vel * dt;
    // axis-separated movement with step-up
    vec3 step = delta;
    // vertical
    vec3 test = pos;
    test.y += step.y;
    if (capsuleBlocked(test, radius, height)) {
        if (step.y < 0) {
            // land: snap to the voxel surface
            float gy = capsuleGround(pos, radius);
            if (pos.y - gy < 1.2f) pos.y = gy;
            grounded = true;
            vel.y = 0;
        } else {
            hitCeiling = true;
            vel.y = 0;
        }
    } else {
        pos.y = test.y;
        float gy = capsuleGround(pos, radius);
        if (pos.y - gy < 0.06f) { pos.y = gy; grounded = true; if (vel.y < 0) vel.y = 0; }
    }
    // horizontal with step-up
    auto tryMove = [&](float dx, float dz) {
        if (std::fabs(dx) < 1e-6f && std::fabs(dz) < 1e-6f) return;
        vec3 t = pos;
        t.x += dx;
        t.z += dz;
        if (!capsuleBlocked(t, radius, height)) {
            pos.x = t.x;
            pos.z = t.z;
            return;
        }
        // try stepping up (curbs, rubble)
        for (float lift = 0.12f; lift <= 0.62f; lift += 0.12f) {
            vec3 s = pos;
            s.x += dx;
            s.z += dz;
            s.y += lift;
            if (!capsuleBlocked(s, radius, height)) {
                vec3 s2 = s;
                s2.y -= lift - 0.02f;
                if (!capsuleBlocked(s2, radius, height)) {
                    pos = s2;
                    return;
                }
                pos = s;
                return;
            }
        }
        // slide along the blocked axis
        vec3 sx = pos;
        sx.x += dx;
        if (!capsuleBlocked(sx, radius, height)) { pos.x = sx.x; }
        vec3 sz = pos;
        sz.z += dz;
        if (!capsuleBlocked(sz, radius, height)) { pos.z = sz.z; }
    };
    tryMove(step.x, step.z);
    if (grounded) {
        float gy = capsuleGround(pos, radius);
        if (pos.y - gy > 0.08f) grounded = false;
    }
    return grounded;
}

bool Physics::pointBlocked(const vec3& p) const {
    return world->solidAtM(p);
}

Body* Physics::nearestProp(const vec3& p, float maxDist) {
    Body* best = nullptr;
    float bestD = maxDist;
    for (auto& b : bodies) {
        if (!b.active || !b.isProp) continue;
        float d = (b.pos - p).len();
        if (d < bestD) { bestD = d; best = &b; }
    }
    return best;
}

void Physics::damageBody(int index, float dmg, const vec3& dir, float impulse) {
    if (index < 0 || index >= (int)bodies.size()) return;
    Body& b = bodies[index];
    if (!b.active) return;
    b.health -= dmg;
    b.vel += dir * (impulse * b.invMass);
    b.sleeping = false;
    b.sleepTimer = 0;
    if (b.health <= 0) {
        // break apart into debris
        int counts[M_COUNT] = {0};
        counts[b.mat] = 40;
        AABB area(b.pos - b.half * 2.0f, b.pos + b.half * 2.0f);
        spawnDebrisBurst(b.pos, area, counts, 2, 1.4f);
        if (b.propKind == 4) {
            extern void explodeAt(const vec3&, float, int);
            explodeAt(b.pos, 5.5f, 1);
        }
        b.active = false;
    }
}

bool Physics::spawnProp(vec3 pos, int kind) {
    uint8_t mat = M_METAL_DARK;
    vec3 half(0.28f, 0.42f, 0.28f);
    float mass = 30;
    int hp = 100;
    if (kind == 0) { mat = M_RUST; half = vec3(0.30f, 0.44f, 0.30f); mass = 32; }
    else if (kind == 1) { mat = M_PLANK; half = vec3(0.34f); mass = 22; mat = M_WOOD; }
    else if (kind == 2) { mat = M_RUBBER; half = vec3(0.46f, 0.36f, 0.46f); mass = 26; }
    else if (kind == 3) { mat = M_METAL; half = vec3(0.55f, 0.85f, 0.55f); mass = 120; }
    else if (kind == 4) { mat = M_PAINT_RED; half = vec3(0.30f, 0.44f, 0.30f); mass = 28; hp = 40; }
    int i = spawn(pos, half, mat, mass, vec3(0), vec3(0), -1, false);
    Body& b = bodies[i];
    b.isProp = true;
    b.propKind = kind;
    b.health = (float)hp;
    b.restitution = 0.12f;
    return true;
}

void Physics::cullFarBodies(const vec3& viewPos, float maxDist) {
    for (auto& b : bodies) {
        if (!b.active || b.heavy) continue;
        if ((b.pos - viewPos).len() > maxDist && b.life > 0) b.active = false;
    }
}
