// FRACTURE — deterministic RNG utilities
#pragma once
#include "common.h"

namespace ft {

struct Rng {
    u64 s;
    explicit Rng(u64 seed = 0x9e3779b97f4a7c15ULL) : s(seed ? seed : 1) {}
    u64 next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return s;
    }
    f32 uni() { return (f32)((next() >> 40) * (1.0 / 16777216.0)); } // [0,1)
    f32 range(f32 a, f32 b) { return a + (b - a) * uni(); }
    int irange(int a, int b) { return a + (int)(next() % (u32)(b - a)); } // [a,b)
    f32 gauss() { // approx sum of3 uniforms
        return (uni() + uni() + uni() - 1.5f) * 1.633f;
    }
    bool chance(f32 p) { return uni() < p; }
    Vec3 dir() {
        f32 z = range(-1.f, 1.f), a = range(0.f, 6.2831853f);
        f32 r = std::sqrt(tmax(0.f, 1.f - z * z));
        return {r * std::cos(a), r * std::sin(a), z};
    }
    Vec3 inSphere() {
        Vec3 d = dir();
        f32 r = std::cbrt(uni());
        return d * r;
    }
};

// cheap deterministic value noise (for textures / colours)
inline f32 vnoise2(int x, int y, u32 seed) {
    u32 h = hashU32((u32)x * 374761393u ^ (u32)y * 668265263u ^ seed);
    return (f32)((h & 0xffff) / 65535.0);
}
inline f32 vnoise3(int x, int y, int z, u32 seed) {
    u32 h = hashU32((u32)x * 374761393u ^ (u32)y * 668265263u ^ (u32)z * 2246822519u ^ seed);
    return (f32)((h & 0xffff) / 65535.0);
}
inline f32 fbm2(f32 x, f32 y, u32 seed) {
    // value-noise fbm,2 octaves (fast, used for colour jitter)
    auto smooth = [](f32 t) { return t * t * (3 - 2 * t); };
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    f32 fx = x - xi, fy = y - yi;
    f32 a = vnoise2(xi, yi, seed), b = vnoise2(xi + 1, yi, seed);
    f32 c = vnoise2(xi, yi + 1, seed), d = vnoise2(xi + 1, yi + 1, seed);
    f32 u = smooth(fx), v = smooth(fy);
    f32 o1 = lerpf(a, b, u), o2 = lerpf(c, d, u);
    return lerpf(o1, o2, v);
}

} // namespace ft
