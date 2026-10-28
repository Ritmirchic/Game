// FRACTURE — physics implementation
#include "physics.h"

namespace ft {

// ------------------------------------------------------------------ bodies --
static void massFromBox(Body& b);
u32 Physics::addBody(const Body& b) {
    if ((int)bodies.size() >= maxBodies) {
        // evict oldest sleeping debris
        int victim = -1;
        for (int i = 0; i < (int)bodies.size(); i++)
            if ((bodies[i].flags & BF_SLEEP) && bodies[i].kind == BK_DEBRIS) { victim = i; break; }
        if (victim < 0) victim = 0;
        bodies[victim] = bodies.back();
        bodies.pop_back();
    }
    Body nb = b;
    nb.id = nextId++;
    if (nb.mass <= 0) massFromBox(nb);
    if (nb.invMass == 0 && (nb.flags & BF_STATIC)) { /* static */ }
    else if (nb.flags & BF_STATIC) { nb.invMass = 0; }
    else nb.invMass = 1.f / nb.mass;
    bodies.push_back(nb);
    return nb.id;
}

void Physics::removeBody(u32 id) {
    for (size_t i = 0; i < bodies.size(); i++)
        if (bodies[i].id == id) {
            bodies[i] = bodies.back();
            bodies.pop_back();
            return;
        }
}

Body* Physics::findBody(u32 id) {
    for (auto& b : bodies) if (b.id == id) return &b;
    return nullptr;
}

static void massFromBox(Body& b) {
    MatDef d = matDef(b.mat);
    f32 v = b.half.x * b.half.y * b.half.z * 8.f;
    b.mass = tmax(0.05f, d.density * v * 2.4f); // density is kg/L-ish scaled
    b.invMass = b.mass > 0 ? 1.f / b.mass : 0.f;
}

u32 Physics::spawnFragment(const Cluster& cl, Vec3 vel, Vec3 angVel) {
    if ((int)frags.size() >= maxFrags) {
        // remove oldest sleeping
        int victim = -1;
        for (int i = 0; i < (int)frags.size(); i++) if (frags[i].state == 2) { victim = i; break; }
        if (victim < 0) victim = 0;
        frags[victim] = std::move(frags.back());
        frags.pop_back();
    }
    Fragment f;
    f.grid.resize(cl.w, cl.h, cl.d);
    f.voxCount = 0;
    f32 densSum = 0;
    for (int z = 0; z < cl.d; z++) for (int y = 0; y < cl.h; y++) for (int x = 0; x < cl.w; x++) {
        Voxel v = cl.vox[(size_t)x + (size_t)y * cl.w + (size_t)z * cl.w * cl.h];
        f.grid.set(x, y, z, v);
        if (matSolid(v.mat)) { f.voxCount++; densSum += matDef(v.mat).density; }
    }
    f.localCenter = Vec3(cl.w * VOX * 0.5f, cl.h * VOX * 0.5f, cl.d * VOX * 0.5f);
    f.pos = Vec3((cl.ox + cl.w * 0.5f) * VOX, (cl.oy + cl.h * 0.5f) * VOX, (cl.oz + cl.d * 0.5f) * VOX);
    f.vel = vel; f.angVel = angVel;
    f.rot = Quat();
    f.mass = tmax(0.5f, (f.voxCount > 0 ? densSum / f.voxCount : 1.f) * (VOX * VOX * VOX) * 1000.f * 0.4f);
    f.invMass = 1.f / f.mass;
    meshCluster(f.grid, f.mverts, f.midx, f.tv, f.ti);
    f.id = nextId++;
    if (f.midx.empty()) return 0; // nothing visible
    frags.push_back(std::move(f));
    return frags.back().id;
}

u32 Physics::spawnFragmentBox(Vec3 pos, Vec3 half, u8 mat, u16 col, Vec3 vel) {
    Cluster cl;
    cl.ox = (int)std::floor(pos.x / VOX); cl.oy = (int)std::floor(pos.y / VOX); cl.oz = (int)std::floor(pos.z / VOX);
    cl.w = tmax(1, (int)std::ceil(half.x * 2 / VOX));
    cl.h = tmax(1, (int)std::ceil(half.y * 2 / VOX));
    cl.d = tmax(1, (int)std::ceil(half.z * 2 / VOX));
    cl.w = tmin(cl.w, 64); cl.h = tmin(cl.h, 64); cl.d = tmin(cl.d, 64);
    cl.vox.assign((size_t)cl.w * cl.h * cl.d, Voxel{mat, 0, col});
    cl.count = cl.w * cl.h * cl.d;
    cl.ox -= cl.w / 2; cl.oy -= cl.h / 2; cl.oz -= cl.d / 2;
    return spawnFragment(cl, vel, Vec3(0, 0, 0));
}

void Physics::removeFragment(u32 id) {
    for (size_t i = 0; i < frags.size(); i++)
        if (frags[i].id == id) { frags[i] = std::move(frags.back()); frags.pop_back(); return; }
}

// --------------------------------------------------------------- utilities --
static inline Vec3 boxInvInertiaMul(const Body& b, Vec3 v) {
    // I = m/12 * (dims²..): invI applied in local frame
    if (b.invMass == 0) return Vec3();
    f32 m = b.mass;
    f32 hx = b.half.x * 2, hy = b.half.y * 2, hz = b.half.z * 2;
    f32 ix = m / 12.f * (hy * hy + hz * hz);
    f32 iy = m / 12.f * (hx * hx + hz * hz);
    f32 iz = m / 12.f * (hx * hx + hy * hy);
    Vec3 lv = b.rot.conj().rotate(v);
    Vec3 lo(lv.x / tmax(ix, 1e-6f), lv.y / tmax(iy, 1e-6f), lv.z / tmax(iz, 1e-6f));
    return b.rot.rotate(lo);
}
static inline Vec3 fragInvInertiaMul(const Fragment& f, Vec3 v) {
    if (f.invMass == 0) return Vec3();
    Vec3 he(f.grid.w * VOX * 0.5f, f.grid.h * VOX * 0.5f, f.grid.d * VOX * 0.5f);
    f32 m = f.mass;
    f32 ix = m / 12.f * (he.y * he.y * 4 + he.z * he.z * 4);
    f32 iy = m / 12.f * (he.x * he.x * 4 + he.z * he.z * 4);
    f32 iz = m / 12.f * (he.x * he.x * 4 + he.y * he.y * 4);
    Vec3 lv = f.rot.conj().rotate(v);
    Vec3 lo(lv.x / tmax(ix, 1e-6f), lv.y / tmax(iy, 1e-6f), lv.z / tmax(iz, 1e-6f));
    return f.rot.rotate(lo);
}

static inline Quat quatIntegrate(const Quat& q, Vec3 w, f32 dt) {
    Quat dq(0.5f * (w.x * q.w + w.y * q.z - w.z * q.y),
            0.5f * (w.y * q.w + w.z * q.x - w.x * q.z),
            0.5f * (w.z * q.w + w.x * q.y - w.y * q.x),
            -0.5f * (w.x * q.x + w.y * q.y + w.z * q.z));
    Quat r(q.x + dq.x * dt, q.y + dq.y * dt, q.z + dq.z * dt, q.w + dq.w * dt);
    f32 l = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    if (l > 1e-8f) { r.x /= l; r.y /= l; r.z /= l; r.w /= l; }
    return r;
}

// ------------------------------------------------------------ world collide --
void Physics::collideBodyWorld(Body& b, bool allowDamage) {
    if (!world) return;
    for (int pass = 0; pass < 2; pass++) {
        AABB wb = AABB::center(b.pos, b.half);
        int x0 = (int)std::floor(wb.lo.x / VOX), x1 = (int)std::floor(wb.hi.x / VOX);
        int y0 = (int)std::floor(wb.lo.y / VOX), y1 = (int)std::floor(wb.hi.y / VOX);
        int z0 = (int)std::floor(wb.lo.z / VOX), z1 = (int)std::floor(wb.hi.z / VOX);
        f32 maxPen[6] = {0, 0, 0, 0, 0, 0};  // per axis: -side, +side
        Vec3 deepPos;
        f32 deepVol = 0;
        u8 deepMat = MAT_AIR;
        bool any = false;
        for (int z = z0; z <= z1; z++)
        for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!world->inBounds(x, y, z) || !world->solid(x, y, z)) continue;
            f32 vx0 = x * VOX, vy0 = y * VOX, vz0 = z * VOX;
            f32 ox = tmin(wb.hi.x, vx0 + VOX) - tmax(wb.lo.x, vx0);
            f32 oy = tmin(wb.hi.y, vy0 + VOX) - tmax(wb.lo.y, vy0);
            f32 oz = tmin(wb.hi.z, vz0 + VOX) - tmax(wb.lo.z, vz0);
            if (ox <= 0 || oy <= 0 || oz <= 0) continue;
            any = true;
            f32 c[3] = {vx0 + VOX * 0.5f, vy0 + VOX * 0.5f, vz0 + VOX * 0.5f};
            f32 pens[3] = {ox, oy, oz};
            for (int a = 0; a < 3; a++) {
                int slot = a * 2 + (((&b.pos.x)[a] >= c[a]) ? 1 : 0);
                if (pens[a] > maxPen[slot]) maxPen[slot] = pens[a];
            }
            if (ox * oy * oz > deepVol) {
                deepVol = ox * oy * oz;
                deepPos = Vec3(vx0 + VOX * 0.5f, vy0 + VOX * 0.5f, vz0 + VOX * 0.5f);
                deepMat = world->mat(x, y, z);
            }
        }
        if (!any) { b.groundT = tmax(0.f, b.groundT - 0.016f); return; }
        // correction axis: movement-preferred, else deepest penetration
        f32 sp[3] = {std::fabs(b.vel.x), std::fabs(b.vel.y) + 0.8f, std::fabs(b.vel.z)};
        int mAxis = (sp[0] >= sp[1] && sp[0] >= sp[2]) ? 0 : (sp[1] >= sp[2] ? 1 : 2);
        int slot;
        if (maxPen[mAxis * 2] > 0.0001f || maxPen[mAxis * 2 + 1] > 0.0001f)
            slot = mAxis * 2 + (maxPen[mAxis * 2 + 1] >= maxPen[mAxis * 2] ? 1 : 0);
        else {
            slot = 0;
            for (int i = 1; i < 6; i++) if (maxPen[i] > maxPen[slot]) slot = i;
        }
        f32 pen = maxPen[slot];
        if (pen <= 0.0001f) return;
        int axis = slot / 2;
        f32 dir = (slot % 2) ? 1.f : -1.f;
        Vec3 n; (&n.x)[axis] = dir;
        (&b.pos.x)[axis] += dir * pen;
        f32 vn = dot(b.vel, n);
        if (vn < 0) {
            f32 e = (std::fabs(vn) < 1.4f) ? 0.f : b.restitution;  // kill micro-bounce
            b.vel -= n * ((1.f + e) * vn);
            Vec3 vt = b.vel - n * dot(b.vel, n);
            b.vel -= vt * tmin(0.5f, b.friction * 0.5f);
            if (std::fabs(vn) > 2.f) {
                Vec3 r = deepPos - b.pos;
                Vec3 J = n * (-(1.f + e) * vn * b.mass);
                b.angVel += boxInvInertiaMul(b, cross(r, J)) * 0.5f;
            }
            if (dir > 0 && axis == 1) b.groundT = 0.1f;
            f32 energy = b.mass * vn * vn;
            if (allowDamage && energy > 45.f) {
                ImpactEvent ev;
                ev.pos = deepPos; ev.nrm = n; ev.energy = energy;
                ev.mat = deepMat; ev.type = IMP_VOX; ev.bodyId = b.id;
                impacts.push_back(ev);
            }
        } else if (dir > 0 && axis == 1) b.groundT = 0.1f;
    }
}

void Physics::collideFragWorld(Fragment& f, std::vector<ImpactEvent>& localImpacts) {
    if (!world) return;
    int stride = 1;
    int sw = (f.grid.w + stride - 1) / stride, sh = (f.grid.h + stride - 1) / stride, sd = (f.grid.d + stride - 1) / stride;
    while (sw * sh * sd > 340) { stride++; sw = (f.grid.w + stride - 1) / stride; sh = (f.grid.h + stride - 1) / stride; sd = (f.grid.d + stride - 1) / stride; }
    bool ground = false;
    int contacts = 0;
    Vec3 lc = f.localCenter;
    for (int z = 0; z < f.grid.d && contacts < 64; z += stride)
    for (int y = 0; y < f.grid.h && contacts < 64; y += stride)
    for (int x = 0; x < f.grid.w && contacts < 64; x += stride) {
        Voxel v = f.grid.get(x, y, z);
        if (!matSolid(v.mat)) continue;
        Vec3 local((x + 0.5f) * VOX - lc.x, (y + 0.5f) * VOX - lc.y, (z + 0.5f) * VOX - lc.z);
        Vec3 wc = f.pos + f.rot.rotate(local);
        int cx = (int)std::floor(wc.x / VOX), cy = (int)std::floor(wc.y / VOX), cz = (int)std::floor(wc.z / VOX);
        if (!world->inBounds(cx, cy, cz) || !world->solid(cx, cy, cz)) continue;
        // find exit face toward least penetration with free neighbour
        f32 px0 = wc.x - cx * VOX, py0 = wc.y - cy * VOX, pz0 = wc.z - cz * VOX;
        f32 cands[6] = {px0, VOX - px0, py0, VOX - py0, pz0, VOX - pz0};
        int order[6] = {0, 1, 2, 3, 4, 5};
        // pick smallest penetration whose outward neighbour is free
        int bestAxis = -1; f32 bestPen = 1e9f; f32 bestDir = 1;
        static const int dx[6] = {-1, 1, 0, 0, 0, 0};
        static const int dy[6] = {0, 0, -1, 1, 0, 0};
        static const int dz[6] = {0, 0, 0, 0, -1, 1};
        (void)order;
        for (int a = 0; a < 6; a++) {
            if (cands[a] <= 1e-5f) continue;
            int nx = cx + dx[a], ny = cy + dy[a], nz = cz + dz[a];
            if (world->inBounds(nx, ny, nz) && world->solid(nx, ny, nz)) continue;
            if (cands[a] < bestPen) {
                bestPen = cands[a];
                bestAxis = a;
                bestDir = 0;
            }
        }
        if (bestAxis < 0) {
            // fully enclosed: push along velocity
            if (lenSq(f.vel) < 1e-4f) continue;
            bestAxis = (std::fabs(f.vel.x) > std::fabs(f.vel.y) ? (std::fabs(f.vel.x) > std::fabs(f.vel.z) ? (f.vel.x > 0 ? 1 : 0) : (f.vel.z > 0 ? 5 : 4))
                                                                : (std::fabs(f.vel.y) > std::fabs(f.vel.z) ? (f.vel.y > 0 ? 3 : 2) : (f.vel.z > 0 ? 5 : 4)));
            bestPen = VOX * 0.05f;
        }
        Vec3 n(dx[bestAxis], dy[bestAxis], dz[bestAxis]);
        (void)bestDir;
        f.pos += n * bestPen;
        f32 vn = dot(f.vel, n);
        if (vn < 0) {
            Vec3 preVel = f.vel;
            f.vel -= n * ((1.f + f.restitution) * vn);
            Vec3 vt = f.vel - n * dot(f.vel, n);
            f.vel -= vt * tmin(0.6f, f.friction * 0.55f);
            if (std::fabs(vn) > 1.5f) {
                Vec3 r = wc - f.pos;
                Vec3 J = n * (-(1.f + f.restitution) * vn * f.mass);
                f.angVel += fragInvInertiaMul(f, cross(r, J)) * 0.7f;
            }
            f32 energy = f.mass * vn * vn;
            if (energy > 60.f) {
                ImpactEvent ev;
                ev.pos = wc; ev.nrm = n; ev.energy = energy;
                ev.mat = v.mat; ev.type = IMP_FRAG; ev.bodyId = f.id;
                localImpacts.push_back(ev);
                if (energy > 500.f && f.voxCount > 90 && f.splitCool <= 0) {
                    Vec3 dir0 = norm(preVel);
                    if (lenSq(dir0) < 1e-6f) dir0 = n;
                    if (trySplitFragment((int)(&f - frags.data()), wc, dir0)) return; // f invalidated
                }
            }
        }
        if (n.y > 0.4f) ground = true;
        contacts++;
    }
    f.contactT = ground ? f.contactT + 0.016f : tmax(0.f, f.contactT - 0.032f);
}

// ---------------------------------------------------------------- stepping --
void Physics::stepBodies(f32 dt) {
    // hinge-driven doors first
    for (auto& b : bodies) {
        if (b.flags & BF_SLEEP) continue;
        if ((b.flags & BF_DRIVEN) && (b.flags & BF_HINGED)) {
            f32 speed = 3.4f * dt;
            f32 diff = b.hingeTarget - b.hingeAngle;
            f32 step = tclamp(diff, -speed, speed);
            b.hingeAngle += step;
            b.rot = Quat::axisAngle(b.hingeAxis, b.hingeAngle) * b.hingeRot0;
            b.pos = b.hingePivot + b.rot.rotate(b.hingeLocalOffset);
            b.vel = Vec3();
            b.angVel = b.hingeAxis * (step / dt);
            if (std::fabs(diff) < 0.01f && (b.flags & BF_SLEEP) == 0) {
                // keep awake while driven (player may bump it)
            }
            continue;
        }
        if (b.flags & (BF_STATIC | BF_SLEEP | BF_HELD)) continue;
        // substep to prevent tunneling
        f32 disp = len(b.vel) * dt;
        int sub = (int)tclamp((int)std::ceil(disp / (VOX * 0.7f)), 1, 8);
        f32 sdt = dt / (f32)sub;
        for (int s = 0; s < sub; s++) {
            if (!(b.flags & BF_NOGRAV)) b.vel.y += gravity * sdt;
            b.vel *= (1.f - 0.06f * sdt);
            b.angVel *= (1.f - 3.2f * sdt);
            b.pos += b.vel * sdt;
            b.rot = quatIntegrate(b.rot, b.angVel, sdt);
            collideBodyWorld(b, true);
        }
        // safety: never allowed to end up buried inside solid voxels
        {
            int cx = (int)std::floor(b.pos.x / VOX), cy = (int)std::floor(b.pos.y / VOX), cz = (int)std::floor(b.pos.z / VOX);
            int guard = 0;
            while (world && world->inBounds(cx, cy, cz) && world->solid(cx, cy, cz) && guard++ < 400) {
                b.pos.y += VOX * 0.5f;
                cy = (int)std::floor(b.pos.y / VOX);
            }
            if (guard >= 400) { b.pos.y += 2.f; b.vel = Vec3(); }
        }
        // sleep bookkeeping
        if (lenSq(b.vel) < 0.004f && lenSq(b.angVel) < 0.02f && b.groundT > 0) {
            b.sleepT += dt;
            if (b.sleepT > 0.45f && !(b.flags & BF_DRIVEN)) { b.flags |= BF_SLEEP; b.vel = Vec3(); b.angVel = Vec3(); }
        } else b.sleepT = 0;
        b.groundT = tmax(0.f, b.groundT - dt);
    }
}

void Physics::stepFrags(f32 dt) {
    for (size_t i = 0; i < frags.size(); i++) {
        Fragment& f = frags[i];
        if (f.splitCool > 0) f.splitCool -= dt;
        if (f.state == 2) continue; // settled sleeper
        f32 disp = len(f.vel) * dt;
        int sub = (int)tclamp((int)std::ceil(disp / (VOX * 0.7f)), 1, 10);
        f32 sdt = dt / (f32)sub;
        size_t before = frags.size();
        bool splitAway = false;
        for (int s = 0; s < sub && !splitAway; s++) {
            f.vel.y += gravity * sdt;
            f.vel *= (1.f - 0.04f * sdt);
            f.angVel *= (1.f - 0.7f * sdt);
            f.pos += f.vel * sdt;
            f.rot = quatIntegrate(f.rot, f.angVel, sdt);
            f.angVel *= (1.f - 0.5f * sdt);
            size_t szBefore = frags.size();
            collideFragWorld(f, impacts);
            if (frags.size() != szBefore) splitAway = true;
        }
        if (splitAway) { i = (size_t)-1; continue; }
        size_t before2 = frags.size(); (void)before2;
        if (frags.size() != before) { i = (size_t)-1; continue; }
        if (i >= frags.size()) { i = (size_t)-1; continue; }
        // settle & voxelize
        if (f.contactT > 0.35f && lenSq(f.vel) < 0.09f && lenSq(f.angVel) < 0.4f) {
            f.state = 1;
            f.sleepT += dt;
            if (f.sleepT > 0.5f) {
                if (voxelizeFragment((int)i)) {
                    i = (size_t)-1;
                    continue;
                } else {
                    f.state = 2;
                    f.vel = Vec3(); f.angVel = Vec3();
                }
            }
        } else {
            if (f.state == 1 && (lenSq(f.vel) > 0.2f || f.contactT <= 0)) { f.state = 0; f.sleepT = 0; }
        }
        // out-of-world kill
        if (f.pos.y < -8 || world == nullptr) { removeFragment(f.id); i = (size_t)-1; continue; }
    }
}

void Physics::stepConstraints(f32 dt) {
    (void)dt;
    for (auto& c : cons) {
        Body* a = c.bodyA ? findBody(c.bodyA) : nullptr;
        Body* b = c.bodyB ? findBody(c.bodyB) : nullptr;
        if (c.bodyA && !a) continue;
        if (c.bodyB && !b) continue;
        Vec3 pa = a ? a->pos + a->rot.rotate(c.localA) : c.pointA;
        Vec3 pb = b ? b->pos + b->rot.rotate(c.localB) : c.pointB;
        Vec3 d = pb - pa;
        f32 dist = len(d);
        if (dist < 1e-5f) continue;
        f32 over = dist - c.restLen;
        if (over <= 0) continue;
        Vec3 dir = d / dist;
        f32 wa = (a && a->invMass > 0 && !(a->flags & BF_SLEEP)) ? a->invMass : (a && a->invMass > 0 ? a->invMass : 0);
        f32 wb = (b && b->invMass > 0 && !(b->flags & BF_SLEEP)) ? b->invMass : (b && b->invMass > 0 ? b->invMass : 0);
        if (a && a->flags & BF_SLEEP) wa = 0;
        if (b && b->flags & BF_SLEEP) wb = 0;
        f32 wsum = wa + wb;
        if (wsum <= 0) continue;
        if (dist > c.maxLen) over = dist - c.maxLen;
        f32 corr = over / wsum;
        if (a && wa > 0) { a->pos -= dir * (corr * wa); a->flags &= (u16)~BF_SLEEP; }
        if (b && wb > 0) { b->pos += dir * (corr * wb); b->flags &= (u16)~BF_SLEEP; }
        // velocity constraint (rope: kill separating velocity)
        f32 va = a && wa > 0 ? dot(a->vel, dir) : 0;
        f32 vb = b && wb > 0 ? dot(b->vel, dir) : 0;
        f32 rel = vb - va;
        if (rel > 0) {
            f32 j = rel / wsum;
            if (a && wa > 0) a->vel -= dir * (j * wa * 0.9f);
            if (b && wb > 0) b->vel += dir * (j * wb * 0.9f);
        }
    }
    cons.erase(std::remove_if(cons.begin(), cons.end(), [&](const Constraint& c) {
        if (c.bodyA && !findBody(c.bodyA)) return true;
        if (c.bodyB && !findBody(c.bodyB)) return true;
        return false;
    }), cons.end());
}

void Physics::step(f32 dt) {
    impacts.clear();
    explodes.clear();
    breaks.clear();
    stepBodies(dt);
    stepFrags(dt);
    stepConstraints(dt);
    stepBodyPairs(dt);
}

// ------------------------------------------------------- body-body contacts --
void Physics::stepBodyPairs(f32 dt) {
    (void)dt;
    int n = (int)bodies.size();
    if (n < 2) return;
    // spatial hash
    std::unordered_map<u32, std::vector<int>> grid;
    grid.reserve((size_t)n * 2);
    auto cellOf = [&](const Body& b) {
        int cx = (int)std::floor(b.pos.x / 3.f), cy = (int)std::floor(b.pos.y / 3.f), cz = (int)std::floor(b.pos.z / 3.f);
        return hashU32((u32)(cx + 4096) ^ ((u32)(cy + 4096) << 10) ^ ((u32)(cz + 4096) << 20));
    };
    for (int i = 0; i < n; i++) grid[cellOf(bodies[i])].push_back(i);
    for (int i = 0; i < n; i++) {
        Body& a = bodies[i];
        if ((a.flags & BF_STATIC) && !(a.flags & BF_DRIVEN)) continue;
        int cx = (int)std::floor(a.pos.x / 3.f), cy = (int)std::floor(a.pos.y / 3.f), cz = (int)std::floor(a.pos.z / 3.f);
        for (int dz = -1; dz <= 1; dz++) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            u32 h = hashU32((u32)(cx + dx + 4096) ^ ((u32)(cy + dy + 4096) << 10) ^ ((u32)(cz + dz + 4096) << 20));
            auto it = grid.find(h);
            if (it == grid.end()) continue;
            for (int j : it->second) {
                if (j <= i) continue;
                Body& b = bodies[j];
                if (a.id == b.id) continue;
                bool aStatic = (a.flags & (BF_STATIC | BF_DRIVEN)) != 0;
                bool bStatic = (b.flags & (BF_STATIC | BF_DRIVEN)) != 0;
                if (aStatic && bStatic) continue;
                Vec3 d = b.pos - a.pos;
                f32 ra = a.boundingR(), rb = b.boundingR();
                f32 dist = len(d);
                f32 pen = ra + rb - dist;
                if (pen <= 0) continue;
                Vec3 nrm = dist > 1e-4f ? d / dist : Vec3(0, 1, 0);
                f32 wa = (aStatic || (a.flags & BF_SLEEP)) ? 0.f : ((a.flags & BF_HELD) ? 0.f : a.invMass);
                f32 wb = (bStatic || (b.flags & BF_SLEEP)) ? 0.f : ((b.flags & BF_HELD) ? 0.f : b.invMass);
                // sleeping bodies wake on firm contact
                f32 rel = dot(b.vel - a.vel, nrm);
                if ((a.flags & BF_SLEEP) && rel < -1.2f) { a.flags &= (u16)~BF_SLEEP; wa = a.invMass; a.sleepT = 0; }
                if ((b.flags & BF_SLEEP) && rel > 1.2f) { b.flags &= (u16)~BF_SLEEP; wb = b.invMass; b.sleepT = 0; }
                if ((a.flags & BF_HELD)) wa = 0;
                if ((b.flags & BF_HELD)) wb = 0;
                f32 wsum = wa + wb;
                if (wsum <= 0) continue;
                f32 corr = pen * 0.6f;
                if (wa > 0) a.pos -= nrm * (corr * (wa / wsum));
                if (wb > 0) b.pos += nrm * (corr * (wb / wsum));
                if (rel < 0) {
                    f32 e = tmin(a.restitution, b.restitution);
                    f32 j = -(1.f + e) * rel / wsum;
                    if (wa > 0) a.vel -= nrm * (j * wa);
                    if (wb > 0) b.vel += nrm * (j * wb);
                    f32 energy = -rel * -rel * tmin(a.mass, b.mass);
                    if (energy > 55.f) {
                        ImpactEvent ev;
                        ev.pos = a.pos + nrm * ra;
                        ev.nrm = -nrm;
                        ev.energy = energy;
                        ev.mat = b.mat; ev.type = IMP_BODY; ev.bodyId = b.id;
                        impacts.push_back(ev);
                        // structural damage to props on hard hits
                        f32 dmg = energy * 0.05f;
                        if (dmg > 6) {
                            if (b.hp > 0 && !(b.flags & BF_STATIC)) { b.hp = (u16)(b.hp > dmg ? b.hp - (int)dmg : 0); }
                            if (a.hp > 0 && !(a.flags & BF_STATIC)) { a.hp = (u16)(a.hp > dmg ? a.hp - (int)dmg : 0); }
                        }
                    }
                }
            }
        }
    }
    // handle destroyed bodies (hp<=0)
    for (int i = (int)bodies.size() - 1; i >= 0; i--) {
        Body& b = bodies[i];
        if (b.hp > 0) continue;
        if (b.flags & BF_EXPLOSIVE) {
            ExplodeEvent ev;
            ev.pos = b.pos; ev.force = b.explosiveForce >= 3 ? 340.f : b.explosiveForce == 2 ? 220.f : b.explosiveForce == 1 ? 130.f : 90.f;
            ev.level = b.explosiveForce;
            explodes.push_back(ev);
        } else {
            BreakEvent ev;
            ev.pos = b.pos; ev.mat = b.mat; ev.kind = b.kind; ev.bodyId = b.id;
            ev.pad = 0;
            breaks.push_back(ev);
        }
        removeBody(b.id);
    }
}

// ------------------------------------------------------------ fragmentation --
Quat Physics::nearestAxisQuat(const Quat& q) const {
    // find signed permutation closest to q's rotation matrix
    Mat4 m = q.toMat();
    Vec3 c0(m.m[0], m.m[1], m.m[2]);
    Vec3 c1(m.m[4], m.m[5], m.m[6]);
    Vec3 c2(m.m[8], m.m[9], m.m[10]);
    Vec3 axes[3] = {Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)};
    int map[3] = {-1, -1, -1};
    bool used[3] = {false, false, false};
    Vec3 cols[3] = {c0, c1, c2};
    for (int i = 0; i < 3; i++) {
        f32 best = -2.f; int bi = 0;
        for (int j = 0; j < 3; j++) {
            if (used[j]) continue;
            f32 d = dot(cols[i], axes[j]);
            if (std::fabs(d) > std::fabs(best)) { best = d; bi = j; }
        }
        used[bi] = true;
        map[i] = bi;
        cols[i] = axes[bi] * (best >= 0 ? 1.f : -1.f);
    }
    // re-orthogonalize c2
    Vec3 r2 = cross(cols[0], cols[1]);
    if (dot(r2, cols[2]) < 0) r2 = -r2;
    // build quaternion from basis (columns = images of X,Y,Z)
    f32 m00 = cols[0].x, m10 = cols[0].y, m20 = cols[0].z;
    f32 m01 = cols[1].x, m11 = cols[1].y, m21 = cols[1].z;
    f32 m02 = r2.x, m12 = r2.y, m22 = r2.z;
    f32 tr = m00 + m11 + m22;
    Quat r;
    if (tr > 0) {
        f32 s = std::sqrt(tr + 1.f) * 2;
        r.w = 0.25f * s; r.x = (m21 - m12) / s; r.y = (m02 - m20) / s; r.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        f32 s = std::sqrt(1.f + m00 - m11 - m22) * 2;
        r.w = (m21 - m12) / s; r.x = 0.25f * s; r.y = (m01 + m10) / s; r.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        f32 s = std::sqrt(1.f + m11 - m00 - m22) * 2;
        r.w = (m02 - m20) / s; r.x = (m01 + m10) / s; r.y = 0.25f * s; r.z = (m12 + m21) / s;
    } else {
        f32 s = std::sqrt(1.f + m22 - m00 - m11) * 2;
        r.w = (m10 - m01) / s; r.x = (m02 + m20) / s; r.y = (m12 + m21) / s; r.z = 0.25f * s;
    }
    f32 l = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    if (l > 1e-6f) { r.x /= l; r.y /= l; r.z /= l; r.w /= l; }
    return r;
}

bool Physics::voxelizeFragment(int fi) {
    if (fi < 0 || fi >= (int)frags.size()) return false;
    Fragment& f = frags[fi];
    if (!world) return false;
    Quat rot = nearestAxisQuat(f.rot);
    struct Cell { int x, y, z; Voxel v; };
    std::vector<Cell> cand;
    cand.reserve(f.voxCount);
    Vec3 lc = f.localCenter;
    int placed = 0, total = 0;
    for (int z = 0; z < f.grid.d; z++)
    for (int y = 0; y < f.grid.h; y++)
    for (int x = 0; x < f.grid.w; x++) {
        Voxel v = f.grid.get(x, y, z);
        if (!matSolid(v.mat)) continue;
        total++;
        Vec3 local((x + 0.5f) * VOX - lc.x, (y + 0.5f) * VOX - lc.y, (z + 0.5f) * VOX - lc.z);
        Vec3 wc = f.pos + rot.rotate(local);
        int cx = (int)std::floor(wc.x / VOX), cy = (int)std::floor(wc.y / VOX), cz = (int)std::floor(wc.z / VOX);
        if (!world->inBounds(cx, cy, cz)) continue;
        if (world->solid(cx, cy, cz)) continue;
        cand.push_back({cx, cy, cz, v});
        placed++;
    }
    if (total == 0) { removeFragment(f.id); return true; }
    if (placed * 10 < total * 7) return false; // <70% fits → stay as physics body
    for (auto& c : cand) world->set(c.x, c.y, c.z, c.v.mat, c.v.col, c.v.flags, EDIT_SETTLE, true);
    removeFragment(f.id);
    return true;
}

bool Physics::trySplitFragment(int fi, Vec3 planePoint, Vec3 planeNrm) {
    if (fi < 0 || fi >= (int)frags.size()) return false;
    Fragment& f = frags[fi];
    if (f.voxCount < 90 || (int)frags.size() > maxFrags - 2) return false;
    if (f.splitCool > 0) return false;
    Vec3 lc = f.localCenter;
    // partition in local space
    int cnt[2] = {0, 0};
    int minx[2] = {1<<30, 1<<30}, miny[2] = {1<<30, 1<<30}, minz[2] = {1<<30, 1<<30};
    int maxx[2] = {-1, -1}, maxy[2] = {-1, -1}, maxz[2] = {-1, -1};
    std::vector<u8> side((size_t)f.grid.w * f.grid.h * f.grid.d, 0);
    for (int z = 0; z < f.grid.d; z++)
    for (int y = 0; y < f.grid.h; y++)
    for (int x = 0; x < f.grid.w; x++) {
        Voxel v = f.grid.get(x, y, z);
        if (!matSolid(v.mat)) continue;
        Vec3 local((x + 0.5f) * VOX - lc.x, (y + 0.5f) * VOX - lc.y, (z + 0.5f) * VOX - lc.z);
        Vec3 wc = f.pos + f.rot.rotate(local);
        f32 s = dot(wc - planePoint, planeNrm);
        int k = s >= 0 ? 1 : 0;
        side[(size_t)x + (size_t)y * f.grid.w + (size_t)z * f.grid.w * f.grid.h] = (u8)k;
        cnt[k]++;
        minx[k] = tmin(minx[k], x); miny[k] = tmin(miny[k], y); minz[k] = tmin(minz[k], z);
        maxx[k] = tmax(maxx[k], x); maxy[k] = tmax(maxy[k], y); maxz[k] = tmax(maxz[k], z);
    }
    if (cnt[0] < 30 || cnt[1] < 30) return false;
    Cluster children[2];
    for (int k = 0; k < 2; k++) {
        children[k].ox = minx[k]; children[k].oy = miny[k]; children[k].oz = minz[k];
        children[k].w = maxx[k] - minx[k] + 1;
        children[k].h = maxy[k] - miny[k] + 1;
        children[k].d = maxz[k] - minz[k] + 1;
        if (children[k].w * children[k].h * children[k].d > 400000) return false;
        children[k].vox.assign((size_t)children[k].w * children[k].h * children[k].d, Voxel{MAT_AIR, 0, 0});
        for (int z = minz[k]; z <= maxz[k]; z++)
        for (int y = miny[k]; y <= maxy[k]; y++)
        for (int x = minx[k]; x <= maxx[k]; x++) {
            u8 s = side[(size_t)x + (size_t)y * f.grid.w + (size_t)z * f.grid.w * f.grid.h];
            if (s != k) continue;
            Voxel v = f.grid.get(x, y, z);
            children[k].vox[(size_t)(x - minx[k]) + (size_t)(y - miny[k]) * children[k].w + (size_t)(z - minz[k]) * children[k].w * children[k].h] = v;
        }
        children[k].count = cnt[k];
    }
    // capture state before removal
    Vec3 pos = f.pos, vel = f.vel, ang = f.angVel;
    Quat rot = f.rot;
    f32 massRatio0 = (f32)cnt[0] / (f32)(cnt[0] + cnt[1]);
    Vec3 shift = planeNrm;
    removeFragment(f.id);
    for (int k = 0; k < 2; k++) {
        // spawn with inherited transform: temporarily build then adjust
        u32 id = spawnFragment(children[k], Vec3(), Vec3());
        // find it (last pushed)
        if (!frags.empty() && frags.back().id == id) {
            Fragment& nf = frags.back();
            // reposition relative to parent: children origin is parent-local; map to world
            // centre of child in parent local coords:
            Vec3 childCenterLocal((children[k].ox + children[k].w * 0.5f) * VOX - lc.x,
                                  (children[k].oy + children[k].h * 0.5f) * VOX - lc.y,
                                  (children[k].oz + children[k].d * 0.5f) * VOX - lc.z);
            // spawnFragment set pos from world voxel origin — override with parent transform
            Vec3 intended = pos + rot.rotate(childCenterLocal);
            Vec3 delta = intended - nf.pos;
            (void)delta;
            nf.pos = intended;
            nf.rot = rot;
            nf.vel = vel + shift * ((k ? 1.f : -1.f) * 1.4f * (0.5f - massRatio0) + 0.6f);
            nf.angVel = ang * 0.6f + Vec3(rng.range(-1.f, 1.f), rng.range(-1.f, 1.f), rng.range(-1.f, 1.f)) * 1.2f;
            nf.splitCool = 0.7f;
            nf.state = 0;
        }
    }
    return true;
}

// ------------------------------------------------------------------- queries --
RayHit Physics::raycast(Vec3 o, Vec3 d, f32 maxT, Body** hitBody, Fragment** hitFrag, u32 skipBodyId) const {
    if (hitBody) *hitBody = nullptr;
    if (hitFrag) *hitFrag = nullptr;
    RayHit best;
    best.t = maxT;
    if (world) {
        RayHit h = world->raycast(o, d, maxT);
        if (h.hit && h.t < best.t) best = h;
    }
    for (const auto& b : bodies) {
        if (b.id == skipBodyId) continue;
        f32 t;
        if (rayOBB(o, d, b.pos, b.half, b.rot, t) && t >= 0 && t < best.t) {
            best.hit = true; best.t = t;
            best.pos = o + d * t;
            Vec3 dd = norm(best.pos - b.pos);
            best.nrm = dd;
            best.mat = b.mat;
            best.vx = best.vy = best.vz = 0;
            if (hitBody) *hitBody = const_cast<Body*>(&b);
            if (hitFrag) *hitFrag = nullptr;
        }
    }
    for (const auto& f : frags) {
        Vec3 half(f.grid.w * VOX * 0.5f, f.grid.h * VOX * 0.5f, f.grid.d * VOX * 0.5f);
        f32 t;
        if (!rayOBB(o, d, f.pos, half, f.rot, t) || t < 0 || t >= best.t) continue;
        // refine: march inside bbox to find solid voxel
        f32 tEnd = t + (half.x + half.y + half.z) * 2.f;
        int steps = 24;
        f32 hitT = -1;
        Voxel hitVox;
        for (int s = 0; s <= steps; s++) {
            f32 tt = t + (tEnd - t) * ((f32)s / steps);
            if (tt > maxT) break;
            Vec3 p = o + d * tt;
            Vec3 lp = f.rot.conj().rotate(p - f.pos);
            int vx = (int)std::floor((lp.x + f.localCenter.x) / VOX);
            int vy = (int)std::floor((lp.y + f.localCenter.y) / VOX);
            int vz = (int)std::floor((lp.z + f.localCenter.z) / VOX);
            Voxel v = f.grid.get(vx, vy, vz);
            if (matSolid(v.mat)) { hitT = tt; hitVox = v; break; }
        }
        if (hitT >= 0 && hitT < best.t) {
            best.hit = true; best.t = hitT;
            best.pos = o + d * hitT;
            best.nrm = -norm(d);
            best.mat = hitVox.mat;
            if (hitFrag) *hitFrag = const_cast<Fragment*>(&f);
            if (hitBody) *hitBody = nullptr;
        }
    }
    return best;
}

int Physics::overlapBodies(const AABB& box, u32* out, int maxOut, u32 skipId) const {
    int n = 0;
    for (const auto& b : bodies) {
        if (b.id == skipId || n >= maxOut) continue;
        AABB bb = AABB::center(b.pos, b.half);
        // rotated approximation: expand by rotation extent
        Vec3 e = b.half;
        f32 maxE = tmax(e.x, tmax(e.y, e.z));
        bb = AABB::center(b.pos, Vec3(maxE, maxE, maxE));
        if (box.overlaps(bb)) out[n++] = b.id;
    }
    return n;
}

// -------------------------------------------------------------- gameplay -----
void Physics::explosionAt(Vec3 pos, f32 radius, f32 strength, u8 srcLevel, bool damageProps) {
    (void)srcLevel;
    // bodies
    for (auto& b : bodies) {
        f32 d = len(b.pos - pos);
        if (d > radius + b.boundingR()) continue;
        f32 f = tclamp(1.f - d / radius, 0.f, 1.f);
        Vec3 dir = norm(b.pos - pos + Vec3(0, 0.25f, 0));
        b.flags &= (u16)~BF_SLEEP;
        b.sleepT = 0;
        b.vel += dir * (f * strength * 0.02f * tmin(60.f, b.mass));
        b.angVel += Vec3(rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)) * (f * strength * 0.05f);
        if (damageProps) {
            f32 dmg = f * strength * 0.9f / tmax(0.5f, b.mass * 0.06f);
            if (dmg > 1) b.hp = (u16)(b.hp > (int)dmg ? b.hp - (int)dmg : 0);
        }
    }
    // fragments (reverse: removals swap-pop)
    for (int i = (int)frags.size() - 1; i >= 0; i--) {
        Fragment& fr = frags[i];
        AABB fa = fr.worldAabb();
        Vec3 cp = vmax(vmin(pos, fa.hi), fa.lo);
        f32 d = len(cp - pos);
        if (d > radius) continue;
        f32 f = tclamp(1.f - d / radius, 0.f, 1.f);
        if (fr.state == 2) { fr.state = 0; fr.sleepT = 0; } // wake settled
        Vec3 dir = norm(fr.pos - pos + Vec3(0, 0.3f, 0));
        fr.vel += dir * (f * strength * 0.02f * tmin(60.f, fr.mass));
        fr.angVel += Vec3(rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)) * (f * strength * 0.03f);
        if (damageProps) {
            // carve local voxels
            Vec3 lc = fr.localCenter;
            std::vector<int> removedIdx;
            for (int z = 0; z < fr.grid.d; z++)
            for (int y = 0; y < fr.grid.h; y++)
            for (int x = 0; x < fr.grid.w; x++) {
                Voxel v = fr.grid.get(x, y, z);
                if (!matSolid(v.mat)) continue;
                Vec3 local((x + 0.5f) * VOX - lc.x, (y + 0.5f) * VOX - lc.y, (z + 0.5f) * VOX - lc.z);
                Vec3 wc = fr.pos + fr.rot.rotate(local);
                f32 dd = len(wc - pos);
                if (dd > radius) continue;
                f32 score = strength * (1.f - (dd / radius) * (dd / radius));
                if (score > matDef(v.mat).blastRes * rng.range(0.75f, 1.15f)) {
                    fr.grid.set(x, y, z, Voxel{MAT_AIR, 0, 0});
                    fr.voxCount--;
                }
            }
            if (fr.voxCount < 12) {
                BreakEvent ev;
                ev.pos = fr.pos; ev.mat = MAT_CONCRETE; ev.kind = BK_DEBRIS; ev.bodyId = fr.id; ev.pad = 0;
                breaks.push_back(ev);
                removeFragment(fr.id);
                continue;
            }
            meshCluster(fr.grid, fr.mverts, fr.midx, fr.tv, fr.ti);
            fr.meshRev++;
        }
    }
    // destroyed bodies (hp<=0) are processed at the end of stepBodyPairs
}

void Physics::pushAt(Vec3 pos, f32 radius, Vec3 impulse) {
    for (auto& b : bodies) {
        f32 d = len(b.pos - pos);
        if (d > radius + b.boundingR()) continue;
        f32 f = tclamp(1.f - d / radius, 0.f, 1.f);
        b.flags &= (u16)~BF_SLEEP;
        b.vel += impulse * (f * tmin(1.f, b.invMass * 8.f + 0.15f));
    }
    for (auto& fr : frags) {
        f32 d = len(fr.pos - pos);
        if (d > radius) continue;
        f32 f = tclamp(1.f - d / radius, 0.f, 1.f);
        if (fr.state == 2) { fr.state = 0; fr.sleepT = 0; }
        fr.vel += impulse * (f * tmin(1.f, fr.invMass * 8.f + 0.15f));
    }
}

void Physics::damageBody(u32 id, int amount, Vec3 srcPos) {
    (void)srcPos;
    Body* b = findBody(id);
    if (!b) return;
    b->flags &= (u16)~BF_SLEEP;
    b->hp = (u16)(b->hp > amount ? b->hp - amount : 0);
    // destroyed bodies are processed at the end of stepBodyPairs
    if (b->hp == 0) {
        if (b->flags & BF_EXPLOSIVE) {
            ExplodeEvent ev;
            ev.pos = b->pos;
            ev.force = b->explosiveForce >= 3 ? 340.f : b->explosiveForce == 2 ? 220.f : b->explosiveForce == 1 ? 130.f : 90.f;
            ev.level = b->explosiveForce;
            explodes.push_back(ev);
        } else {
            BreakEvent ev;
            ev.pos = b->pos; ev.mat = b->mat; ev.kind = b->kind; ev.bodyId = b->id; ev.pad = 0;
            breaks.push_back(ev);
        }
        removeBody(id);
    }
}

void Physics::wakeNear(Vec3 pos, f32 radius) {
    f32 r2 = radius * radius;
    for (auto& b : bodies) {
        if (lenSq(b.pos - pos) < r2) { b.flags &= (u16)~BF_SLEEP; b.sleepT = 0; }
    }
    for (auto& fr : frags) {
        if (lenSq(fr.pos - pos) < r2 && fr.state == 2) { fr.state = 0; fr.sleepT = 0; }
    }
}

size_t Physics::byteSize() const {
    size_t s = 0;
    for (const auto& b : bodies) s += sizeof(Body);
    for (const auto& f : frags) s += sizeof(Fragment) + f.mverts.capacity() + f.midx.capacity() * 4 + f.grid.cells.size() * sizeof(Voxel);
    return s;
}

// -------------------------------------------------------------- carve helper --
int carveWorld(VoxWorld& w, Vec3 center, f32 radius, f32 power, f32 matBias,
               EditCause cause, std::vector<Voxel>* removedOut, Rng* rng) {
    int x0 = (int)std::floor((center.x - radius) / VOX), x1 = (int)std::floor((center.x + radius) / VOX);
    int y0 = (int)std::floor((center.y - radius) / VOX), y1 = (int)std::floor((center.y + radius) / VOX);
    int z0 = (int)std::floor((center.z - radius) / VOX), z1 = (int)std::floor((center.z + radius) / VOX);
    struct Rem { int x, y, z; };
    std::vector<Rem> removed;
    removed.reserve(64);
    int total = 0;
    f32 r2 = radius * radius;
    for (int z = z0; z <= z1; z++)
    for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
        if (!w.inBounds(x, y, z)) continue;
        Voxel v = w.get(x, y, z);
        if (!matSolid(v.mat)) continue;
        f32 cx = (x + 0.5f) * VOX - center.x;
        f32 cy = (y + 0.5f) * VOX - center.y;
        f32 cz = (z + 0.5f) * VOX - center.z;
        f32 d2 = cx * cx + cy * cy + cz * cz;
        if (d2 > r2) continue;
        f32 d = std::sqrt(d2) / radius;
        f32 score = power * (1.f - d * d) * (0.85f + (rng ? rng->range(0.f, 0.3f) : 0.15f));
        const MatDef& md = matDef(v.mat);
        f32 res = (cause == EDIT_BLAST) ? md.blastRes : md.hardness;
        res *= matBias;
        if (score > res) {
            removed.push_back({x, y, z});
            if (removedOut) removedOut->push_back(v);
        }
    }
    for (auto& r : removed) {
        w.set(r.x, r.y, r.z, MAT_AIR, 0, 0, cause, true);
        total++;
        // mark exposed neighbours as fresh interior
        static const int NB[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (auto& n : NB) {
            int nx = r.x + n[0], ny = r.y + n[1], nz = r.z + n[2];
            if (!w.inBounds(nx, ny, nz)) continue;
            Voxel nv = w.get(nx, ny, nz);
            if (matSolid(nv.mat) && !(nv.flags & VF_FRESH))
                w.set(nx, ny, nz, nv.mat, nv.col, (u8)(nv.flags | VF_FRESH), cause, false);
        }
    }
    return total;
}

} // namespace ft
