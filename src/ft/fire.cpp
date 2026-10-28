// FRACTURE — voxel fire propagation implementation
#include "fire.h"
#include "common.h"

namespace ft {

static bool isFlammable(const Voxel& v) {
    if (!matSolid(v.mat)) return false;
    const MatDef& md = matDef(v.mat);
    return md.flammability > 0 && (md.flags & MF_FLAMMABLE) != 0;
}

static f32 burnDuration(const Voxel& v) {
    const MatDef& md = matDef(v.mat);
    // higher flammability → shorter burn; base2..6s
    return 6.f - (md.flammability / 255.f) * 4.f;
}

bool FireSystem::ignite(int x, int y, int z, f32 boost) {
    if (!world_ || !world_->inBounds(x, y, z)) return false;
    Voxel v = world_->get(x, y, z);
    if (!isFlammable(v)) return false;
    u32 k = key(x, y, z);
    if (findIdx(k) >= 0) return true;
    f32 life = burnDuration(v) / tmax(0.25f, boost);
    // soot the surface immediately (visual charring)
    u16 col = v.col;
    int r, g, b;
    unpack565(col, r, g, b);
    r = (int)(r * 0.45f); g = (int)(g * 0.42f); b = (int)(b * 0.42f);
    world_->set(x, y, z, v.mat, rgb565(r, g, b), (u8)(v.flags | VF_BURNING | VF_WET), EDIT_FIRE, true);
    cells_.push_back({k, life, life, 0});
    return true;
}

bool FireSystem::igniteAt(Vec3 pos, f32 radius, f32 chance) {
    int n = 0;
    int x0 = (int)std::floor((pos.x - radius) / VOX), x1 = (int)std::ceil((pos.x + radius) / VOX);
    int y0 = (int)std::floor((pos.y - radius) / VOX), y1 = (int)std::ceil((pos.y + radius) / VOX);
    int z0 = (int)std::floor((pos.z - radius) / VOX), z1 = (int)std::ceil((pos.z + radius) / VOX);
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        if (chance < 1.f && rng.range(0.f, 1.f) > chance) continue;
        if (ignite(x, y, z)) n++;
    }
    return n > 0;
}

int FireSystem::extinguishAt(Vec3 pos, f32 radius) {
    int out = 0;
    f32 r2 = radius * radius;
    for (int i = (int)cells_.size() - 1; i >= 0; i--) {
        int x = (int)(cells_[i].key & 2047);
        int y = (int)((cells_[i].key >> 11) & 511);
        int z = (int)((cells_[i].key >> 20) & 4095);
        Vec3 c((x + 0.5f) * VOX, (y + 0.5f) * VOX, (z + 0.5f) * VOX);
        if (lenSq(c - pos) <= r2) {
            Voxel v = world_->get(x, y, z);
            // clear burning flag, mark wet, restore some colour
            world_->set(x, y, z, v.mat, v.col, (u8)((v.flags & ~(u8)VF_BURNING) | VF_WET), EDIT_DAMAGE, true);
            cells_.erase(cells_.begin() + i);
            out++;
        }
    }
    return out;
}

int FireSystem::igniteRing(Vec3 center, f32 radius, f32 chance) {
    int n = 0;
    f32 r0 = radius, r1 = radius + VOX * 2.5f;
    int x0 = (int)std::floor((center.x - r1) / VOX), x1 = (int)std::ceil((center.x + r1) / VOX);
    int y0 = (int)std::floor((center.y - r1) / VOX), y1 = (int)std::ceil((center.y + r1) / VOX);
    int z0 = (int)std::floor((center.z - r1) / VOX), z1 = (int)std::ceil((center.z + r1) / VOX);
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        Vec3 c((x + 0.5f) * VOX, (y + 0.5f) * VOX, (z + 0.5f) * VOX);
        f32 d = len(c - center);
        if (d < r0 * 0.75f || d > r1) continue;
        if (rng.range(0.f, 1.f) > chance) continue;
        if (ignite(x, y, z)) n++;
    }
    return n;
}

Vec3 FireSystem::centroid() const {
    if (cells_.empty()) return Vec3();
    Vec3 s;
    for (auto& c : cells_) {
        int x = (int)(c.key & 2047);
        int y = (int)((c.key >> 11) & 511);
        int z = (int)((c.key >> 20) & 4095);
        s += Vec3((x + 0.5f) * VOX, (y + 0.5f) * VOX, (z + 0.5f) * VOX);
    }
    return s * (1.f / (f32)cells_.size());
}

int FireSystem::update(f32 dt, Vec3& heatCenter) {
    if (!world_ || cells_.empty()) { heatCenter = Vec3(); return 0; }
    int consumed = 0;
    static std::vector<u32> expired;
    expired.clear();
    // spread bookkeeping
    int spreadBudget = 24;
    for (Cell& c : cells_) {
        c.life -= dt;
        int x = (int)(c.key & 2047);
        int y = (int)((c.key >> 11) & 511);
        int z = (int)((c.key >> 20) & 4095);
        // emit handled by game via centroid; spread to neighbors
        if (c.life > 0) {
            const static int NB[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            for (auto& nb : NB) {
                if (spreadBudget <= 0) break;
                int nx = x + nb[0], ny = y + nb[1], nz = z + nb[2];
                if (!world_->inBounds(nx, ny, nz)) continue;
                if (findIdx(key(nx, ny, nz)) >= 0) continue;
                Voxel nv = world_->get(nx, ny, nz);
                if (!isFlammable(nv) || (nv.flags & VF_WET)) continue;
                const MatDef& md = matDef(nv.mat);
                f32 p = (md.flammability / 255.f) * 0.85f * dt;  // per-second chance
                if (rng.range(0.f, 1.f) < p && ignite(nx, ny, nz)) spreadBudget--;
            }
        }
        if (c.life <= 0.f) expired.push_back(c.key);
    }
    // burn out
    for (u32 k : expired) {
        int x = (int)(k & 2047);
        int y = (int)((k >> 11) & 511);
        int z = (int)((k >> 20) & 4095);
        Voxel v = world_->get(x, y, z);
        if (v.flags & VF_BURNING) {
            if (rng.range(0.f, 1.f) < 0.72f) {
                // ash out: remove voxel (EDIT_FIRE, no carved stat)
                world_->set(x, y, z, MAT_AIR, 0, 0, EDIT_FIRE, true);
                consumed++;
            } else {
                // charred remnant stays (small chance → structures partially survive)
                world_->set(x, y, z, v.mat, rgb565(34, 30, 28), (u8)(v.flags & ~(u8)(VF_BURNING | VF_IGNITED)), EDIT_FIRE, true);
            }
        }
        int idx = findIdx(k);
        if (idx >= 0) { cells_[idx] = cells_.back(); cells_.pop_back(); }
    }
    heatCenter = cells_.empty() ? Vec3() : centroid();
    return consumed;
}

} // namespace ft
