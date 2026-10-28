// FRACTURE — voxel fire propagation
#pragma once
#include "voxworld.h"
#include "rng.h"

namespace ft {

class FireSystem {
public:
    void bind(VoxWorld* w) { world_ = w; cells_.clear(); }
    void clear() { cells_.clear(); }

    // ignite the flammable voxel at position; returns true if it caught
    bool ignite(int x, int y, int z, f32 boost = 1.f);
    bool igniteAt(Vec3 pos, f32 radius = 0.6f, f32 chance = 1.f);

    // extinguish all burning cells inside radius; returns cells put out
    int extinguishAt(Vec3 pos, f32 radius = 1.6f);

    // advance burn timers, spread, burn-out. Returns voxels consumed.
    int update(f32 dt, Vec3& heatCenter);

    int burning() const { return (int)cells_.size(); }
    bool any() const { return !cells_.empty(); }
    Vec3 centroid() const;

    // called by explosions: ignite flammables in the rim around the crater
    int igniteRing(Vec3 center, f32 radius, f32 chance = 0.55f);

    // fire-loop audio voice tracking (game reads)
    f32 audioT = 0.f;

    bool sample(int i, int& x, int& y, int& z, f32& life, f32& total) const {
        if (i < 0 || i >= (int)cells_.size()) return false;
        const Cell& c = cells_[(size_t)i];
        x = (int)(c.key & 2047);
        y = (int)((c.key >> 11) & 511);
        z = (int)((c.key >> 20) & 4095);
        life = c.life;
        total = c.total;
        return true;
    }

private:
    struct Cell {
        u32 key;
        f32 life;      // remaining burn seconds
        f32 total;
        u8 phase;      // 0 burning, 1 sputter-out
    };
    VoxWorld* world_ = nullptr;
    std::vector<Cell> cells_;
    Rng rng{0xF17E5EEDu};
    int findIdx(u32 key) const {
        for (int i = 0; i < (int)cells_.size(); i++) if (cells_[i].key == key) return i;
        return -1;
    }
    static u32 key(int x, int y, int z) { return (u32)x | ((u32)y << 11) | ((u32)z << 20); }
};

} // namespace ft
