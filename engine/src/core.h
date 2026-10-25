// ============================================================================
// THE LAST NIGHT — core types, math and utility layer
// Portable C99. No dependencies beyond libm/libc.
// ============================================================================
#ifndef TLN_CORE_H
#define TLN_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TLN_VERSION "1.0.0"
#define TLN_TITLE "THE LAST NIGHT"
#define TLN_SUBTITLE "Survive ten nights"
#define TLN_TAGLINE "a supermarket siege"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LN2PI 6.283185307179586f
#define DEG2RAD(d) ((d) * 0.017453292519943295f)
#define RAD2DEG(r) ((r) * 57.29577951308232f)

// ---------------------------------------------------------------- scalar math
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline int   clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstepf(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0 + 1e-9f), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
static inline float signf(float x) { return x < 0.f ? -1.f : 1.f; }
static inline float fractf(float x) { return x - floorf(x); }
static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float saturatef(float x) { return clampf(x, 0.f, 1.f); }
static inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
static inline float approachf(float cur, float target, float rate, float dt) {
    float d = target - cur;
    float step = rate * dt;
    if (d > step) return cur + step;
    if (d < -step) return cur - step;
    return target;
}
static inline float angwrapf(float a) {
    while (a > (float)M_PI) a -= (float)LN2PI;
    while (a < -(float)M_PI) a += (float)LN2PI;
    return a;
}
static inline float anglestep(float cur, float target, float maxdelta) {
    float d = angwrapf(target - cur);
    if (d > maxdelta) d = maxdelta;
    if (d < -maxdelta) d = -maxdelta;
    return angwrapf(cur + d);
}

// ---------------------------------------------------------------- vectors
typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Vec4;

static inline Vec2 v2(float x, float y) { Vec2 r = {x, y}; return r; }
static inline Vec2 v2add(Vec2 a, Vec2 b) { return v2(a.x + b.x, a.y + b.y); }
static inline Vec2 v2sub(Vec2 a, Vec2 b) { return v2(a.x - b.x, a.y - b.y); }
static inline Vec2 v2scale(Vec2 a, float s) { return v2(a.x * s, a.y * s); }
static inline float v2len(Vec2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
static inline float v2dist(Vec2 a, Vec2 b) { return v2len(v2sub(a, b)); }
static inline Vec3 v3(float x, float y, float z) { Vec3 r = {x, y, z}; return r; }
static inline Vec3 v3s(float s) { Vec3 r = {s, s, s}; return r; }
static inline Vec3 vadd(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec3 vsub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec3 vmul(Vec3 a, Vec3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline Vec3 vscale(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline Vec3 vmad(Vec3 a, Vec3 b, float s) { return v3(a.x + b.x * s, a.y + b.y * s, a.z + b.z * s); }
static inline Vec3 vneg(Vec3 a) { return v3(-a.x, -a.y, -a.z); }
static inline float vdot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 vcross(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float vlen(Vec3 a) { return sqrtf(vdot(a, a)); }
static inline float vlen2(Vec3 a) { return vdot(a, a); }
static inline Vec3 vnorm(Vec3 a) {
    float l = vlen(a);
    if (l < 1e-8f) return v3(0, 1, 0);
    return vscale(a, 1.f / l);
}
static inline Vec3 vlerp(Vec3 a, Vec3 b, float t) { return vadd(a, vscale(vsub(b, a), t)); }
static inline float vdist(Vec3 a, Vec3 b) { return vlen(vsub(a, b)); }
static inline float vdist2(Vec3 a, Vec3 b) { return vlen2(vsub(a, b)); }
static inline Vec3 vmin3(Vec3 a, Vec3 b) { return v3(minf(a.x,b.x), minf(a.y,b.y), minf(a.z,b.z)); }
static inline Vec3 vmax3(Vec3 a, Vec3 b) { return v3(maxf(a.x,b.x), maxf(a.y,b.y), maxf(a.z,b.z)); }
static inline Vec3 vto(Vec3 a, Vec3 b) { return vnorm(vsub(b, a)); }

// yaw around Y (0 = +Z), pitch around X
static inline Vec3 dir_from_yaw_pitch(float yaw, float pitch) {
    float cp = cosf(pitch);
    return v3(sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp);
}

// angle helpers
static inline float yaw_from_dir(Vec3 d) { return atan2f(d.x, d.z); }

// ---------------------------------------------------------------- matrices
// Row-major storage. Column-vector convention: v' = M * v.
typedef struct { float m[16]; } Mat4;

static inline Mat4 mat4_id(void) {
    Mat4 r;
    memset(&r, 0, sizeof r);
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
    return r;
}
static inline Mat4 mat4_transpose(Mat4 a) {
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) r.m[i * 4 + j] = a.m[j * 4 + i];
    return r;
}
static inline Mat4 mat4_mul(Mat4 a, Mat4 b) {  // apply b first, then a
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0.f;
            for (int k = 0; k < 4; k++) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    return r;
}
static inline Vec4 mat4_mulv4(Mat4 a, Vec4 v) {
    Vec4 r;
    r.x = a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z + a.m[3] * v.w;
    r.y = a.m[4] * v.x + a.m[5] * v.y + a.m[6] * v.z + a.m[7] * v.w;
    r.z = a.m[8] * v.x + a.m[9] * v.y + a.m[10] * v.z + a.m[11] * v.w;
    r.w = a.m[12] * v.x + a.m[13] * v.y + a.m[14] * v.z + a.m[15] * v.w;
    return r;
}
static inline Vec3 mat4_mul_point(Mat4 a, Vec3 v) {
    Vec4 r = mat4_mulv4(a, (Vec4){v.x, v.y, v.z, 1.f});
    if (fabsf(r.w) > 1e-9f && r.w != 1.f) return v3(r.x / r.w, r.y / r.w, r.z / r.w);
    return v3(r.x, r.y, r.z);
}
static inline Vec3 mat4_mul_dir(Mat4 a, Vec3 v) {
    Vec4 r = mat4_mulv4(a, (Vec4){v.x, v.y, v.z, 0.f});
    return v3(r.x, r.y, r.z);
}
static inline Mat4 mat4_identity(void) { Mat4 r; memset(&r, 0, sizeof r); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r; }
static inline Mat4 mat4_translate(Vec3 t) {
    Mat4 r = mat4_id();
    r.m[3] = t.x; r.m[7] = t.y; r.m[11] = t.z;
    return r;
}
static inline Mat4 mat4_scale(Vec3 s) {
    Mat4 r = mat4_id();
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}
static inline Mat4 mat4_rot_x(float a) {
    Mat4 r = mat4_id();
    float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = -s; r.m[9] = s; r.m[10] = c;
    return r;
}
static inline Mat4 mat4_rot_y(float a) {
    Mat4 r = mat4_id();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[2] = s; r.m[8] = -s; r.m[10] = c;
    return r;
}
static inline Mat4 mat4_rot_z(float a) {
    Mat4 r = mat4_id();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[1] = -s; r.m[4] = s; r.m[5] = c;
    return r;
}
// TRS (scale, then rotate Y/X/Z, then translate)
static inline Mat4 mat4_trs(Vec3 t, float ry, Vec3 s) {
    return mat4_mul(mat4_translate(t), mat4_mul(mat4_rot_y(ry), mat4_scale(s)));
}
static inline Mat4 mat4_perspective(float fovy_rad, float aspect, float zn, float zf) {
    Mat4 r;
    memset(&r, 0, sizeof r);
    float f = 1.f / tanf(fovy_rad * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = (2.f * zf * zn) / (zn - zf);
    r.m[14] = -1.f;
    return r;
}
// Right-handed view matrix, camera looks down -Z, yaw around Y, pitch around X.
static inline Mat4 mat4_view(Vec3 eye, float yaw, float pitch, float roll) {
    Mat4 rot = mat4_mul(mat4_rot_x(-pitch), mat4_rot_y(-yaw));
    if (roll != 0.f) rot = mat4_mul(rot, mat4_rot_z(roll));
    return mat4_mul(rot, mat4_translate(vneg(eye)));
}

// ---------------------------------------------------------------- colour
static inline uint32_t rgba8(int r, int g, int b, int a) {
    return ((uint32_t)clampi(a,0,255) << 24) | ((uint32_t)clampi(r,0,255) << 16) |
           ((uint32_t)clampi(g,0,255) << 8) | (uint32_t)clampi(b,0,255);
}
static inline uint32_t rgbaf(float r, float g, float b, float a) {
    return rgba8((int)(saturatef(r) * 255.f + 0.5f), (int)(saturatef(g) * 255.f + 0.5f),
                 (int)(saturatef(b) * 255.f + 0.5f), (int)(saturatef(a) * 255.f + 0.5f));
}
static inline int c_r(uint32_t c) { return (c >> 16) & 255; }
static inline int c_g(uint32_t c) { return (c >> 8) & 255; }
static inline int c_b(uint32_t c) { return c & 255; }
static inline int c_a(uint32_t c) { return (c >> 24) & 255; }
static inline uint32_t c_lerp(uint32_t a, uint32_t b, float t) {
    return rgba8((int)lerpf((float)c_r(a), (float)c_r(b), t), (int)lerpf((float)c_g(a), (float)c_g(b), t),
                 (int)lerpf((float)c_b(a), (float)c_b(b), t), (int)lerpf((float)c_a(a), (float)c_a(b), t));
}
static inline uint32_t c_mul(uint32_t c, float s) {
    return rgba8((int)(c_r(c) * s), (int)(c_g(c) * s), (int)(c_b(c) * s), c_a(c));
}

// ---------------------------------------------------------------- random
typedef struct { uint64_t s; } Rng;
static inline uint32_t rng_u32(Rng *r) {
    r->s = r->s * 6364136223846793005ULL + 1442695040888963407ULL;
    uint32_t x = (uint32_t)(r->s >> 33);
    x ^= x >> 16;
    return x;
}
static inline void rng_seed(Rng *r, uint64_t seed) { r->s = seed * 2654435761ULL + 0x9E3779B97F4A7C15ULL; rng_u32(r); }
static inline float rng_f(Rng *r) { return (float)(rng_u32(r) & 0xFFFFFF) / 16777215.f; }
static inline float rng_range(Rng *r, float a, float b) { return a + (b - a) * rng_f(r); }
static inline int rng_int(Rng *r, int a, int b) {  // inclusive
    if (b <= a) return a;
    return a + (int)(rng_u32(r) % (uint32_t)(b - a + 1));
}
static inline float rng_sign(Rng *r) { return rng_f(r) < 0.5f ? -1.f : 1.f; }
// gaussian-ish
static inline float rng_gauss(Rng *r) { return (rng_f(r) + rng_f(r) + rng_f(r) - 1.5f) * 1.1547f; }

// ---------------------------------------------------------------- arena
typedef struct ArenaBlock { struct ArenaBlock *next; size_t cap, used; char data[]; } ArenaBlock;
typedef struct { ArenaBlock *head; size_t total; } Arena;
void *arena_alloc(Arena *a, size_t n);
void *arena_zalloc(Arena *a, size_t n);
char *arena_strdup(Arena *a, const char *s);
void  arena_reset(Arena *a);
void  arena_free(Arena *a);

// ---------------------------------------------------------------- logging
enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
void log_set_file(const char *path);
void log_msg(int level, const char *fmt, ...);
#define LOGI(...) log_msg(LOG_INFO, __VA_ARGS__)
#define LOGW(...) log_msg(LOG_WARN, __VA_ARGS__)
#define LOGE(...) log_msg(LOG_ERROR, __VA_ARGS__)
#define LOGD(...) log_msg(LOG_DEBUG, __VA_ARGS__)

// ---------------------------------------------------------------- time
double core_now(void);   // seconds since arbitrary epoch, high resolution

// ---------------------------------------------------------------- file io
void  file_set_asset_root(const char *root);
const char *file_asset_root(void);
uint8_t *file_read_all(const char *path, size_t *size);
uint8_t *file_read_optional(const char *path, size_t *size);
int   file_exists(const char *path);

// ---------------------------------------------------------------- misc
uint32_t hash32(uint32_t x);
float    value_noise2(float x, float y, int seed);
float    value_noise3(float x, float y, float z, int seed);
float    fbm2(float x, float y, int octaves, int seed);
float    fbm3(float x, float y, float z, int octaves, int seed);

// rotate vec around Y
static inline Vec3 vrot_y(Vec3 v, float a) {
    float c = cosf(a), s = sinf(a);
    return v3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

// simple 2D line-vs-segment / ray helpers used by AI
static inline int segment_intersect_2d(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float *t) {
    float rx = b.x - a.x, ry = b.y - a.y;
    float sx = d.x - c.x, sy = d.y - c.y;
    float denom = rx * sy - ry * sx;
    if (fabsf(denom) < 1e-9f) return 0;
    float t0 = ((c.x - a.x) * sy - (c.y - a.y) * sx) / denom;
    float u0 = ((c.x - a.x) * ry - (c.y - a.y) * rx) / denom;
    if (t0 >= 0.f && t0 <= 1.f && u0 >= 0.f && u0 <= 1.f) { if (t) *t = t0; return 1; }
    return 0;
}

#endif // TLN_CORE_H
