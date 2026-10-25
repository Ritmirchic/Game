// ============================================================================
//  DON'T LOOK BACK  --  NIGHTFALL engine
//  nf_core.h : math, colour, images, PNG codec, RNG, noise, IO
//  Copyright (c) 2026.  Original work, no third-party code.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace nf {

// ---------------------------------------------------------------- constants
constexpr float PI      = 3.14159265358979f;
constexpr float TAU     = 6.28318530717958f;
constexpr float HALF_PI = 1.57079632679489f;
constexpr float DEG     = PI / 180.0f;

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float saturate(float v) { return clampf(v, 0.0f, 1.0f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) {
    float t = saturate((x - e0) / (e1 - e0 + 1e-9f));
    return t * t * (3.0f - 2.0f * t);
}
inline float signf(float v) { return v < 0.0f ? -1.0f : 1.0f; }
inline int   clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline int   mini(int a, int b) { return a < b ? a : b; }
inline int   maxi(int a, int b) { return a > b ? a : b; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
inline float approach(float v, float target, float amount) {
    if (v < target) return minf(v + amount, target);
    return maxf(v - amount, target);
}
// frame-rate independent smoothing
inline float dampf(float v, float target, float rate, float dt) {
    return lerpf(v, target, 1.0f - std::exp(-rate * dt));
}

// ---------------------------------------------------------------- vectors
struct V2 {
    float x = 0, y = 0;
    V2() {}
    V2(float _x, float _y) : x(_x), y(_y) {}
    V2 operator+(const V2& o) const { return V2(x + o.x, y + o.y); }
    V2 operator-(const V2& o) const { return V2(x - o.x, y - o.y); }
    V2 operator*(float s) const { return V2(x * s, y * s); }
    V2 operator/(float s) const { return V2(x / s, y / s); }
    V2& operator+=(const V2& o) { x += o.x; y += o.y; return *this; }
    V2& operator-=(const V2& o) { x -= o.x; y -= o.y; return *this; }
    V2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline V2 operator*(float s, const V2& v) { return V2(v.x * s, v.y * s); }
inline float dot(const V2& a, const V2& b) { return a.x * b.x + a.y * b.y; }
inline float length(const V2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline float lengthSq(const V2& v) { return v.x * v.x + v.y * v.y; }
inline V2 normalize(const V2& v) { float l = length(v); return l > 1e-6f ? v * (1.0f / l) : V2(1, 0); }
inline V2 perp(const V2& v) { return V2(-v.y, v.x); }
inline V2 lerp(const V2& a, const V2& b, float t) { return a + (b - a) * t; }

struct V3 {
    float x = 0, y = 0, z = 0;
    V3() {}
    V3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    explicit V3(float s) : x(s), y(s), z(s) {}
    V3 operator+(const V3& o) const { return V3(x + o.x, y + o.y, z + o.z); }
    V3 operator-(const V3& o) const { return V3(x - o.x, y - o.y, z - o.z); }
    V3 operator-() const { return V3(-x, -y, -z); }
    V3 operator*(float s) const { return V3(x * s, y * s, z * s); }
    V3 operator*(const V3& o) const { return V3(x * o.x, y * o.y, z * o.z); }
    V3 operator/(float s) const { return V3(x / s, y / s, z / s); }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    V3& operator*=(const V3& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
};
inline V3 operator*(float s, const V3& v) { return V3(v.x * s, v.y * s, v.z * s); }
inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3& a, const V3& b) {
    return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float lengthSq(const V3& v) { return dot(v, v); }
inline float length(const V3& v) { return std::sqrt(dot(v, v)); }
inline V3 normalize(const V3& v) {
    float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : V3(0, 1, 0);
}
inline V3 lerp(const V3& a, const V3& b, float t) { return a + (b - a) * t; }
inline V3 vmin(const V3& a, const V3& b) { return V3(minf(a.x, b.x), minf(a.y, b.y), minf(a.z, b.z)); }
inline V3 vmax(const V3& a, const V3& b) { return V3(maxf(a.x, b.x), maxf(a.y, b.y), maxf(a.z, b.z)); }
inline V3 vabs(const V3& a) { return V3(std::fabs(a.x), std::fabs(a.y), std::fabs(a.z)); }
inline float maxComp(const V3& v) { return maxf(v.x, maxf(v.y, v.z)); }

struct V4 {
    float x = 0, y = 0, z = 0, w = 0;
    V4() {}
    V4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

// ---------------------------------------------------------------- matrix (column-major)
struct M4 {
    float m[16];
    M4() { identity(); }
    void identity() {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
    static M4 translate(const V3& t) {
        M4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r;
    }
    static M4 scale(const V3& s) {
        M4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r;
    }
    static M4 scale(float s) { return scale(V3(s, s, s)); }
    static M4 rotX(float a) {
        M4 r; float c = std::cos(a), s = std::sin(a);
        r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r;
    }
    static M4 rotY(float a) {
        M4 r; float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r;
    }
    static M4 rotZ(float a) {
        M4 r; float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r;
    }
    static M4 perspective(float fovY, float aspect, float zn, float zf) {
        M4 r;
        float f = 1.0f / std::tan(fovY * 0.5f);
        for (int i = 0; i < 16; ++i) r.m[i] = 0.0f;
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zf + zn) / (zn - zf);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zf * zn) / (zn - zf);
        return r;
    }
    static M4 ortho(float l, float r_, float b, float t, float zn, float zf) {
        M4 r;
        r.m[0] = 2.0f / (r_ - l);
        r.m[5] = 2.0f / (t - b);
        r.m[10] = -2.0f / (zf - zn);
        r.m[12] = -(r_ + l) / (r_ - l);
        r.m[13] = -(t + b) / (t - b);
        r.m[14] = -(zf + zn) / (zf - zn);
        return r;
    }
    static M4 lookAt(const V3& eye, const V3& center, const V3& up) {
        V3 f = normalize(center - eye);
        V3 s = normalize(cross(f, up));
        V3 u = cross(s, f);
        M4 r;
        r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] = dot(f, eye);
        return r;
    }
    static M4 fromTRS(const V3& t, const V3& rot, const V3& s) {
        M4 r = rotY(rot.y) * rotX(rot.x) * rotZ(rot.z);
        r.m[0] *= s.x; r.m[1] *= s.x; r.m[2] *= s.x; r.m[3] *= s.x;
        r.m[4] *= s.y; r.m[5] *= s.y; r.m[6] *= s.y; r.m[7] *= s.y;
        r.m[8] *= s.z; r.m[9] *= s.z; r.m[10] *= s.z; r.m[11] *= s.z;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }
    M4 operator*(const M4& o) const {
        M4 r;
        for (int c = 0; c < 4; ++c) {
            for (int rw = 0; rw < 4; ++rw) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + rw] * o.m[c * 4 + k];
                r.m[c * 4 + rw] = s;
            }
        }
        return r;
    }
    V4 operator*(const V4& v) const {
        return V4(m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
                  m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
                  m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
                  m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w);
    }
    V3 point(const V3& p) const {
        return V3(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                  m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                  m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
    }
    V3 dir(const V3& p) const {
        return V3(m[0] * p.x + m[4] * p.y + m[8] * p.z,
                  m[1] * p.x + m[5] * p.y + m[9] * p.z,
                  m[2] * p.x + m[6] * p.y + m[10] * p.z);
    }
    M4 transpose() const {
        M4 r;
        for (int c = 0; c < 4; ++c) for (int rw = 0; rw < 4; ++rw) r.m[c * 4 + rw] = m[rw * 4 + c];
        return r;
    }
};

// ---------------------------------------------------------------- axis-aligned box
struct Box3 {
    V3 lo, hi;
    Box3() {}
    Box3(const V3& a, const V3& b) : lo(vmin(a, b)), hi(vmax(a, b)) {}
    static Box3 fromCenterSize(const V3& c, const V3& s) {
        return Box3(c - s * 0.5f, c + s * 0.5f);
    }
    bool overlaps(const Box3& o) const {
        return lo.x < o.hi.x && hi.x > o.lo.x && lo.y < o.hi.y && hi.y > o.lo.y &&
               lo.z < o.hi.z && hi.z > o.lo.z;
    }
    bool contains(const V3& p) const {
        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y && p.z >= lo.z && p.z <= hi.z;
    }
    void expand(const V3& p) { lo = vmin(lo, p); hi = vmax(hi, p); }
    void grow(float e) { lo -= V3(e, e, e); hi += V3(e, e, e); }
    V3 center() const { return (lo + hi) * 0.5f; }
    V3 size() const { return hi - lo; }
};

// ---------------------------------------------------------------- colour (0xAARRGGBB)
typedef uint32_t Color;
inline Color rgba(int r, int g, int b, int a = 255) {
    return ((Color)(a & 255) << 24) | ((Color)(r & 255) << 16) | ((Color)(g & 255) << 8) | (Color)(b & 255);
}
inline Color colF(float r, float g, float b, float a = 1.0f) {
    return rgba((int)(saturate(r) * 255.0f + 0.5f), (int)(saturate(g) * 255.0f + 0.5f),
                (int)(saturate(b) * 255.0f + 0.5f), (int)(saturate(a) * 255.0f + 0.5f));
}
inline int colR(Color c) { return (c >> 16) & 255; }
inline int colG(Color c) { return (c >> 8) & 255; }
inline int colB(Color c) { return c & 255; }
inline int colA(Color c) { return (c >> 24) & 255; }
inline Color colMul(Color a, Color b) {
    return rgba(colR(a) * colR(b) / 255, colG(a) * colG(b) / 255, colB(a) * colB(b) / 255,
                colA(a) * colA(b) / 255);
}
inline Color colScale(Color c, float s) {
    return rgba((int)(colR(c) * s), (int)(colG(c) * s), (int)(colB(c) * s), colA(c));
}
inline Color colLerp(Color a, Color b, float t) {
    return rgba((int)lerpf((float)colR(a), (float)colR(b), t),
                (int)lerpf((float)colG(a), (float)colG(b), t),
                (int)lerpf((float)colB(a), (float)colB(b), t),
                (int)lerpf((float)colA(a), (float)colA(b), t));
}
inline V3 colToV3(Color c) {
    return V3(colR(c) / 255.0f, colG(c) / 255.0f, colB(c) / 255.0f);
}
inline Color v3ToCol(const V3& v, float a = 1.0f) { return colF(v.x, v.y, v.z, a); }

// ---------------------------------------------------------------- image
struct Image {
    int w = 0, h = 0;
    std::vector<Color> px;   // 0xAARRGGBB
    Image() {}
    Image(int _w, int _h, Color fill = 0) : w(_w), h(_h), px((size_t)_w * _h, fill) {}
    void resize(int _w, int _h, Color fill = 0) {
        w = _w; h = _h; px.assign((size_t)w * h, fill);
    }
    bool valid() const { return w > 0 && h > 0 && px.size() == (size_t)w * h; }
    Color get(int x, int y) const {
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x >= w) x = w - 1; if (y >= h) y = h - 1;
        return px[(size_t)y * w + x];
    }
    void set(int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        px[(size_t)y * w + x] = c;
    }
    void blend(int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        int a = colA(c);
        if (a >= 255) { px[(size_t)y * w + x] = c; return; }
        if (a <= 0) return;
        Color d = px[(size_t)y * w + x];
        px[(size_t)y * w + x] = colLerp(d, c | 0xFF000000u, a / 255.0f);
    }
    Color sampleBilinear(float u, float v) const {
        if (!valid()) return 0xFF000000;
        float fx = u * w - 0.5f, fy = v * h - 0.5f;
        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
        float tx = fx - x0, ty = fy - y0;
        Color c00 = get(x0, y0), c10 = get(x0 + 1, y0), c01 = get(x0, y0 + 1), c11 = get(x0 + 1, y0 + 1);
        Color a = colLerp(c00, c10, tx), b = colLerp(c01, c11, tx);
        return colLerp(a, b, ty);
    }
    void fill(Color c) { std::fill(px.begin(), px.end(), c); }
    void box(int x0, int y0, int x1, int y1, Color c) {
        for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) blend(x, y, c);
    }
    Image mipHalf() const;
    void genMips(std::vector<Image>& out, int levels) const;
};

// PNG -----------------------------------------------------------------------
bool pngWrite(const char* path, const Image& img);
bool pngLoad(const char* path, Image& out);      // gray / rgb / rgba, 8-bit
bool pngDecode(const uint8_t* data, size_t n, Image& out);

// ---------------------------------------------------------------- RNG
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 1) {}
    uint32_t u32() {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return (uint32_t)((s * 2685821657736338717ull) >> 32);
    }
    float f() { return (u32() & 0x00FFFFFFu) / 16777216.0f; }
    float range(float a, float b) { return a + (b - a) * f(); }
    int   irange(int a, int b) { return a + (int)(u32() % (uint32_t)maxi(1, b - a + 1)); }
    bool  chance(float p) { return f() < p; }
    V3 unitSphere() {
        float z = range(-1, 1), a = range(0, TAU), r = std::sqrt(maxf(0.0f, 1 - z * z));
        return V3(r * std::cos(a), z, r * std::sin(a));
    }
    V3 inBox(const V3& lo, const V3& hi) {
        return V3(range(lo.x, hi.x), range(lo.y, hi.y), range(lo.z, hi.z));
    }
    template <class T> const T& pick(const std::vector<T>& v) {
        return v[(size_t)(u32() % (uint32_t)v.size())];
    }
};

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline float hashf(uint32_t x) { return (hash32(x) & 0xFFFFFF) / 16777216.0f; }

// ---------------------------------------------------------------- noise
struct Noise {
    uint8_t perm[512];
    explicit Noise(uint32_t seed = 1337);
    float value2(float x, float y) const;
    float value3(float x, float y, float z) const;
    float fbm2(float x, float y, int oct = 4, float lac = 2.0f, float gain = 0.5f) const;
    float fbm3(float x, float y, float z, int oct = 4) const;
    float ridged2(float x, float y, int oct = 4) const;
    float worley2(float x, float y) const;         // F1 distance, cells ~1 unit
    float worley2b(float x, float y, float& id) const;
};

// ---------------------------------------------------------------- io / text
std::string strfmt(const char* fmt, ...);
bool writeFile(const char* path, const void* data, size_t n);
bool readFile(const char* path, std::vector<uint8_t>& out);
bool fileExists(const char* path);
std::string exeDir();
std::string joinPath(const std::string& a, const std::string& b);
void ensureDir(const char* path);
std::string userDataDir(const char* appName);   // %APPDATA%\appName or ~/.local/share/appName
uint64_t nowMillis();
double nowSeconds();
void sleepMillis(int ms);
void logMsg(const char* fmt, ...);


// ---------------------------------------------------------------- thread pool
// A tiny persistent worker pool used by the renderer (banded raster, shadow
// map, post).  Threads are created once, then parked; a dispatch costs one
// mutex wake-up instead of a thread spawn per band.
class Parallel {
public:
    static Parallel& get();
    void run(int nTasks, const std::function<void(int)>& fn);
    int threads() const { return nWorkers_ + 1; }
    void shutdown();
private:
    Parallel() = default;
    void start(int nWorkers);
    struct Impl;
    Impl* impl_ = nullptr;
    int nWorkers_ = 0;
};

} // namespace nf
