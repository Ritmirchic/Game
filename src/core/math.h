// FRACTURE: TOTAL DESTRUCTION — core math
// Column-major matrices (OpenGL convention), metres for gameplay, voxels for storage.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define DEG2RAD (PI / 180.0f)
#define RAD2DEG (180.0f / PI)

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float signf(float v) { return v < 0 ? -1.0f : 1.0f; }
static inline float smoothstepf(float a, float b, float x) {
    float t = clampf((x - a) / (b - a + 1e-9f), 0, 1);
    return t * t * (3 - 2 * t);
}
static inline int   iclamp(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static inline int   clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }

struct vec2 {
    float x = 0, y = 0;
    vec2() {}
    vec2(float x_, float y_) : x(x_), y(y_) {}
    vec2 operator+(const vec2& o) const { return {x + o.x, y + o.y}; }
    vec2 operator-(const vec2& o) const { return {x - o.x, y - o.y}; }
    vec2 operator*(float s) const { return {x * s, y * s}; }
    vec2 operator/(float s) const { return {x / s, y / s}; }
    vec2& operator+=(const vec2& o) { x += o.x; y += o.y; return *this; }
    vec2& operator-=(const vec2& o) { x -= o.x; y -= o.y; return *this; }
    vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};

struct vec3 {
    float x = 0, y = 0, z = 0;
    vec3() {}
    vec3(float v) : x(v), y(v), z(v) {}
    vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit vec3(const vec2& v, float z_ = 0) : x(v.x), y(v.y), z(z_) {}
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
    vec3 operator+(const vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    vec3 operator-(const vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    vec3 operator*(const vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    vec3 operator-() const { return {-x, -y, -z}; }
    vec3& operator+=(const vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    vec3& operator-=(const vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    bool operator==(const vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    float len() const { return std::sqrt(x * x + y * y + z * z); }
    float len2() const { return x * x + y * y + z * z; }
    vec3 norm() const { float l = len(); return l > 1e-8f ? (*this) / l : vec3(0, 1, 0); }
};

static inline vec3 operator*(float s, const vec3& v) { return v * s; }
static inline float dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline vec3 cross(const vec3& a, const vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static inline vec3 vmin(const vec3& a, const vec3& b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
static inline vec3 vmax(const vec3& a, const vec3& b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
static inline vec3 vabs(const vec3& a) { return {std::fabs(a.x), std::fabs(a.y), std::fabs(a.z)}; }
static inline vec3 vlerp(const vec3& a, const vec3& b, float t) { return a + (b - a) * t; }
static inline float vdist(const vec3& a, const vec3& b) { return (a - b).len(); }
static inline vec3 vexp(const vec3& a) { return {std::exp(a.x), std::exp(a.y), std::exp(a.z)}; }

struct ivec3 {
    int x = 0, y = 0, z = 0;
    ivec3() {}
    ivec3(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}
    int& operator[](int i) { return (&x)[i]; }
    int operator[](int i) const { return (&x)[i]; }
    bool operator==(const ivec3& o) const { return x == o.x && y == o.y && z == o.z; }
    ivec3 operator+(const ivec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    ivec3 operator-(const ivec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
};

struct vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    vec4() {}
    vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    vec4(const vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    vec3 xyz() const { return {x, y, z}; }
    vec4 operator+(const vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
};

struct quat {
    float x = 0, y = 0, z = 0, w = 1;
    quat() {}
    quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    quat operator*(const quat& q) const {
        return {w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w,
                w * q.w - x * q.x - y * q.y - z * q.z};
    }
    vec3 rotate(const vec3& v) const {
        vec3 u(x, y, z);
        return u * (2.0f * dot(u, v)) + v * (w * w - dot(u, u)) + cross(u, v) * (2.0f * w);
    }
};
static inline quat quatAxisAngle(const vec3& axis, float angle) {
    vec3 a = axis.norm();
    float s = std::sin(angle * 0.5f), c = std::cos(angle * 0.5f);
    return {a.x * s, a.y * s, a.z * s, c};
}
static inline quat quatFromEuler(float yaw, float pitch, float roll) {
    quat qy = quatAxisAngle({0, 1, 0}, yaw);
    quat qp = quatAxisAngle({1, 0, 0}, pitch);
    quat qr = quatAxisAngle({0, 0, 1}, roll);
    return qy * qp * qr;
}

// Column-major 4x4. m[col*4 + row]. Transforms: M * v.
struct mat4 {
    float m[16];
    mat4() { identity(); }
    void identity() { std::memset(m, 0, sizeof(m)); m[0] = m[5] = m[10] = m[15] = 1; }
    static mat4 zero() { mat4 r; std::memset(r.m, 0, sizeof(r.m)); return r; }
    float& at(int c, int r) { return m[c * 4 + r]; }
    float at(int c, int r) const { return m[c * 4 + r]; }
    mat4 operator*(const mat4& o) const {
        mat4 r = zero();
        for (int c = 0; c < 4; c++)
            for (int rr = 0; rr < 4; rr++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += at(k, rr) * o.at(c, k);
                r.at(c, rr) = s;
            }
        return r;
    }
    vec4 operator*(const vec4& v) const {
        return {at(0,0)*v.x + at(1,0)*v.y + at(2,0)*v.z + at(3,0)*v.w,
                at(0,1)*v.x + at(1,1)*v.y + at(2,1)*v.z + at(3,1)*v.w,
                at(0,2)*v.x + at(1,2)*v.y + at(2,2)*v.z + at(3,2)*v.w,
                at(0,3)*v.x + at(1,3)*v.y + at(2,3)*v.z + at(3,3)*v.w};
    }
    vec3 point(const vec3& p) const {
        vec4 r = (*this) * vec4(p, 1);
        return {r.x, r.y, r.z};
    }
    vec3 dir(const vec3& p) const {
        vec4 r = (*this) * vec4(p, 0);
        return {r.x, r.y, r.z};
    }
};

static inline mat4 matTranslate(const vec3& t) {
    mat4 r; r.at(3,0) = t.x; r.at(3,1) = t.y; r.at(3,2) = t.z; return r;
}
static inline mat4 matScale(const vec3& s) {
    mat4 r; r.at(0,0) = s.x; r.at(1,1) = s.y; r.at(2,2) = s.z; return r;
}
static inline mat4 matScale(float s) { return matScale({s, s, s}); }
static inline mat4 matRotX(float a) {
    mat4 r; float c = std::cos(a), s = std::sin(a);
    r.at(1,1) = c; r.at(1,2) = s; r.at(2,1) = -s; r.at(2,2) = c; return r;
}
static inline mat4 matRotY(float a) {
    mat4 r; float c = std::cos(a), s = std::sin(a);
    r.at(0,0) = c; r.at(0,2) = -s; r.at(2,0) = s; r.at(2,2) = c; return r;
}
static inline mat4 matRotZ(float a) {
    mat4 r; float c = std::cos(a), s = std::sin(a);
    r.at(0,0) = c; r.at(0,1) = s; r.at(1,0) = -s; r.at(1,1) = c; return r;
}
static inline mat4 matFromQuat(const quat& q) {
    mat4 r;
    float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
    float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
    float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
    r.at(0,0) = 1-2*(yy+zz); r.at(0,1) = 2*(xy+wz);   r.at(0,2) = 2*(xz-wy);
    r.at(1,0) = 2*(xy-wz);   r.at(1,1) = 1-2*(xx+zz); r.at(1,2) = 2*(yz+wx);
    r.at(2,0) = 2*(xz+wy);   r.at(2,1) = 2*(yz-wx);   r.at(2,2) = 1-2*(xx+yy);
    return r;
}
static inline mat4 matTRS(const vec3& t, const quat& q, const vec3& s) {
    mat4 r = matFromQuat(q);
    r.at(0,0) *= s.x; r.at(0,1) *= s.x; r.at(0,2) *= s.x;
    r.at(1,0) *= s.y; r.at(1,1) *= s.y; r.at(1,2) *= s.y;
    r.at(2,0) *= s.z; r.at(2,1) *= s.z; r.at(2,2) *= s.z;
    r.at(3,0) = t.x; r.at(3,1) = t.y; r.at(3,2) = t.z;
    return r;
}
static inline mat4 matPerspective(float fovyRad, float aspect, float zn, float zf) {
    mat4 r = mat4::zero();
    float f = 1.0f / std::tan(fovyRad * 0.5f);
    r.at(0,0) = f / aspect;
    r.at(1,1) = f;
    r.at(2,2) = (zf + zn) / (zn - zf);
    r.at(2,3) = -1;
    r.at(3,2) = (2 * zf * zn) / (zn - zf);
    return r;
}
static inline mat4 matOrtho(float l, float r_, float b, float t, float zn, float zf) {
    mat4 r = mat4::zero();
    r.at(0,0) = 2 / (r_ - l);
    r.at(1,1) = 2 / (t - b);
    r.at(2,2) = -2 / (zf - zn);
    r.at(3,0) = -(r_ + l) / (r_ - l);
    r.at(3,1) = -(t + b) / (t - b);
    r.at(3,2) = -(zf + zn) / (zf - zn);
    r.at(3,3) = 1;
    return r;
}
static inline mat4 matLookAt(const vec3& eye, const vec3& center, const vec3& up) {
    vec3 f = (center - eye).norm();
    vec3 s = cross(f, up).norm();
    vec3 u = cross(s, f);
    mat4 r;
    r.at(0,0) = s.x; r.at(1,0) = s.y; r.at(2,0) = s.z;
    r.at(0,1) = u.x; r.at(1,1) = u.y; r.at(2,1) = u.z;
    r.at(0,2) = -f.x; r.at(1,2) = -f.y; r.at(2,2) = -f.z;
    r.at(3,0) = -dot(s, eye); r.at(3,1) = -dot(u, eye); r.at(3,2) = dot(f, eye);
    return r;
}
static inline mat4 matInverse(const mat4& a) {
    const float* m = a.m;
    float inv[16];
    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    det = 1.0f / det;
    mat4 r;
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] * det;
    return r;
}

struct AABB {
    vec3 mn{0, 0, 0}, mx{0, 0, 0};
    AABB() {}
    AABB(const vec3& a, const vec3& b) : mn(a), mx(b) {}
    vec3 center() const { return (mn + mx) * 0.5f; }
    vec3 size() const { return mx - mn; }
    bool intersects(const AABB& o) const {
        return mn.x < o.mx.x && mx.x > o.mn.x && mn.y < o.mx.y && mx.y > o.mn.y && mn.z < o.mx.z && mx.z > o.mn.z;
    }
    bool contains(const vec3& p) const {
        return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y && p.z >= mn.z && p.z <= mx.z;
    }
    AABB expanded(float e) const { return {mn - vec3(e), mx + vec3(e)}; }
};

struct ray {
    vec3 o{0, 0, 0}, d{0, 0, 1};
    vec3 at(float t) const { return o + d * t; }
};

static inline bool rayAABB(const ray& r, const AABB& b, float maxT, float* outT) {
    float tmin = 0, tmax = maxT;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(r.d[i]) < 1e-8f) {
            if (r.o[i] < b.mn[i] || r.o[i] > b.mx[i]) return false;
        } else {
            float inv = 1.0f / r.d[i];
            float t1 = (b.mn[i] - r.o[i]) * inv;
            float t2 = (b.mx[i] - r.o[i]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    if (outT) *outT = tmin;
    return true;
}

// ---------- deterministic noise / hashing ----------
static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static inline uint32_t hash3i(int x, int y, int z, uint32_t seed = 0) {
    uint32_t h = (uint32_t)x * 374761393u ^ (uint32_t)y * 668265263u ^ (uint32_t)z * 2147483647u ^ seed;
    return hash32(h);
}
static inline float hash3f(int x, int y, int z, uint32_t seed = 0) {
    return (hash3i(x, y, z, seed) & 0xFFFFFF) / (float)0xFFFFFF;
}
static inline float valueNoise3(const vec3& p, uint32_t seed = 0) {
    int xi = (int)std::floor(p.x), yi = (int)std::floor(p.y), zi = (int)std::floor(p.z);
    float fx = p.x - xi, fy = p.y - yi, fz = p.z - zi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    float c000 = hash3f(xi, yi, zi, seed),     c100 = hash3f(xi + 1, yi, zi, seed);
    float c010 = hash3f(xi, yi + 1, zi, seed), c110 = hash3f(xi + 1, yi + 1, zi, seed);
    float c001 = hash3f(xi, yi, zi + 1, seed), c101 = hash3f(xi + 1, yi, zi + 1, seed);
    float c011 = hash3f(xi, yi + 1, zi + 1, seed), c111 = hash3f(xi + 1, yi + 1, zi + 1, seed);
    float x00 = lerpf(c000, c100, fx), x10 = lerpf(c010, c110, fx);
    float x01 = lerpf(c001, c101, fx), x11 = lerpf(c011, c111, fx);
    return lerpf(lerpf(x00, x10, fy), lerpf(x01, x11, fy), fz);
}
static inline float fbm3(const vec3& p, int oct, uint32_t seed = 0) {
    float a = 0.5f, s = 0, f = 1;
    for (int i = 0; i < oct; i++) {
        s += valueNoise3(p * f, seed + i * 977) * a;
        f *= 2.0f; a *= 0.5f;
    }
    return s;
}

// Deterministic RNG
struct RNG {
    uint64_t s;
    explicit RNG(uint64_t seed = 0x9E3779B97F4A7C15ULL) : s(seed ? seed : 1) {}
    uint64_t next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return s;
    }
    uint32_t u32() { return (uint32_t)(next() >> 32); }
    float f() { return (next() >> 40) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) { return a + (int)(u32() % (uint32_t)std::max(1, b - a + 1)); }
    bool chance(float p) { return f() < p; }
    vec3 dir() {
        float z = range(-1, 1), a = range(0, 2 * PI), r = std::sqrt(std::max(0.0f, 1 - z * z));
        return {r * std::cos(a), z, r * std::sin(a)};
    }
};

static inline uint32_t packColor(float r, float g, float b, float a = 1) {
    return ((uint32_t)(clampf(r, 0, 1) * 255) << 24) | ((uint32_t)(clampf(g, 0, 1) * 255) << 16) |
           ((uint32_t)(clampf(b, 0, 1) * 255) << 8) | (uint32_t)(clampf(a, 0, 1) * 255);
}
