// ============================================================
// SPACE MINER  -  core.h
// Math, colour, RNG, noise, utility helpers.
// (c) 2026 - original engine, no third party dependencies.
// ============================================================
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace sm {

// ---------------------------------------------------------------- constants
constexpr float PI   = 3.14159265358979f;
constexpr float TAU  = 6.28318530717959f;
constexpr int   VW   = 1280;   // internal render width
constexpr int   VH   = 720;    // internal render height

// ---------------------------------------------------------------- vec2
struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float X, float Y) : x(X), y(Y) {}
    explicit Vec2(float a) : x(a), y(a) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator*(const Vec2& o) const { return {x * o.x, y * o.y}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2 operator-() const { return {-x, -y}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline float dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
inline float cross(const Vec2& a, const Vec2& b) { return a.x * b.y - a.y * b.x; }
inline float length(const Vec2& v) { return sqrtf(v.x * v.x + v.y * v.y); }
inline float lengthSq(const Vec2& v) { return v.x * v.x + v.y * v.y; }
inline Vec2 normalize(const Vec2& v) { float l = length(v); return l > 1e-6f ? v / l : Vec2(0, 0); }
inline Vec2 rot(const Vec2& v, float a) {
    float c = cosf(a), s = sinf(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}
inline Vec2 fromAngle(float a) { return {cosf(a), sinf(a)}; }
inline float angleOf(const Vec2& v) { return atan2f(v.y, v.x); }
inline Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return a + (b - a) * t; }
inline Vec2 perp(const Vec2& v) { return {-v.y, v.x}; }
inline Vec2 clampLen(const Vec2& v, float maxLen) {
    float l = length(v);
    return (l > maxLen && l > 1e-6f) ? v * (maxLen / l) : v;
}

// ---------------------------------------------------------------- scalars
template <class T> inline T clampv(T v, T a, T b) { return v < a ? a : (v > b ? b : v); }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float saturate(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float signf(float v) { return v < 0 ? -1.f : 1.f; }
inline float sqr(float v) { return v * v; }
inline float wrapAngle(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}
inline float lerpAngle(float a, float b, float t) { return a + wrapAngle(b - a) * t; }
inline float approach(float cur, float target, float rate) {
    if (cur < target) return cur + rate > target ? target : cur + rate;
    return cur - rate < target ? target : cur - rate;
}
// frame-rate independent smooth factor
inline float smoothing(float ratePerSec, float dt) { return 1.f - powf(1.f - clampf(ratePerSec, 0.f, 0.9999f), dt * 60.f); }
inline float easeOutCubic(float t) { t = saturate(t); return 1.f - powf(1.f - t, 3.f); }
inline float easeInCubic(float t) { t = saturate(t); return t * t * t; }
inline float easeInOut(float t) { t = saturate(t); return t < .5f ? 2 * t * t : 1 - powf(-2 * t + 2, 2) / 2; }
inline float easeOutBack(float t) {
    t = saturate(t);
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
}

// ---------------------------------------------------------------- colour
// 0xAARRGGBB
typedef uint32_t Color;
inline Color rgba(int r, int g, int b, int a = 255) {
    return ((Color)clampv(a, 0, 255) << 24) | ((Color)clampv(r, 0, 255) << 16) |
           ((Color)clampv(g, 0, 255) << 8) | (Color)clampv(b, 0, 255);
}
inline Color rgbaf(float r, float g, float b, float a = 1.f) {
    return rgba((int)(saturate(r) * 255 + .5f), (int)(saturate(g) * 255 + .5f),
                (int)(saturate(b) * 255 + .5f), (int)(saturate(a) * 255 + .5f));
}
inline Color gray(int v, int a = 255) { return rgba(v, v, v, a); }
inline int cr(Color c) { return (c >> 16) & 255; }
inline int cg(Color c) { return (c >> 8) & 255; }
inline int cb(Color c) { return c & 255; }
inline int ca(Color c) { return (c >> 24) & 255; }
inline Color withAlpha(Color c, int a) { return (c & 0x00FFFFFF) | ((Color)clampv(a, 0, 255) << 24); }
inline Color scaleColor(Color c, float f) {
    return rgba((int)(cr(c) * f), (int)(cg(c) * f), (int)(cb(c) * f), ca(c));
}
inline Color mixColor(Color a, Color b, float t) {
    t = saturate(t);
    return rgba((int)(cr(a) + (cr(b) - cr(a)) * t), (int)(cg(a) + (cg(b) - cg(a)) * t),
                (int)(cb(a) + (cb(b) - cb(a)) * t), (int)(ca(a) + (ca(b) - ca(a)) * t));
}
inline Color fromHSV(float h, float s, float v, float a = 1.f) {
    h = h - floorf(h);
    float r = 0, g = 0, b = 0;
    if (s <= 0) { r = g = b = v; }
    else {
        float i = floorf(h * 6.f), f = h * 6.f - i;
        float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
        switch (((int)i) % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }
    return rgbaf(r, g, b, a);
}
Color shade(Color c, float f);          // multiply rgb
Color addColor(Color a, Color b);       // additive saturating

// ---------------------------------------------------------------- rng (pcg32)
struct Rng {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc = 0xda3e39cb94b95bdbULL;
    void seed(uint64_t s) {
        state = 0; inc = (s << 1u) | 1u;
        next(); state += 0x853c49e6748fea9bULL; next();
    }
    uint32_t next() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }              // [0,1)
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) { return b <= a ? a : a + (int)(next() % (uint32_t)(b - a)); }
    bool chance(float p) { return f() < p; }
    Vec2 dir() { float a = f() * TAU; return {cosf(a), sinf(a)}; }
    Vec2 insideCircle(float radius) {
        float a = f() * TAU, r = sqrtf(f()) * radius;
        return {cosf(a) * r, sinf(a) * r};
    }
};

// ---------------------------------------------------------------- noise
float valueNoise2(float x, float y, uint32_t seed);
float fbm2(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed);
float worley2(float x, float y, uint32_t seed);
float ridged2(float x, float y, int octaves, uint32_t seed);

// ---------------------------------------------------------------- file utils
bool writeFile(const std::string& path, const void* data, size_t size);
bool readFile(const std::string& path, std::vector<uint8_t>& out);
bool fileExists(const std::string& path);
bool savePNG(const std::string& path, const uint32_t* argb, int w, int h);
bool writePNG(const char* path, const uint32_t* px, int w, int h);

// string helpers
std::string fmt(const char* f, ...);
std::string intToStr(long long v);
std::string money(long long v);          // 1 234 567 style grouping with thin spaces
std::string padLeft(const std::string& s, int n, char c = ' ');
std::string upper(const std::string& s);
std::string float1(float v);

}  // namespace sm
