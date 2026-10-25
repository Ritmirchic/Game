/* ============================================================================
   SPACE MINER — core/common.h
   Portable base layer: fixed-width types, math, RNG, memory, logging.
   No OS dependencies. Compiles as C99 on MSVC-clang / gcc / zig cc.
   ========================================================================== */
#ifndef SM_COMMON_H
#define SM_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;
typedef size_t   usize;

#define SM_VERSION_MAJOR 1
#define SM_VERSION_MINOR 0
#define SM_VERSION_PATCH 0
#define SM_VERSION_STR "1.0.0"

#ifndef SM_PI
#define SM_PI 3.14159265358979323846f
#endif
#define SM_TAU   (2.0f * SM_PI)
#define SM_DEG2RAD (SM_PI / 180.0f)
#define SM_RAD2DEG (180.0f / SM_PI)

#define SM_MIN(a,b) ((a) < (b) ? (a) : (b))
#define SM_MAX(a,b) ((a) > (b) ? (a) : (b))
#define SM_CLAMP(x,lo,hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define SM_CLAMP01(x) SM_CLAMP(x, 0.0f, 1.0f)
#define SM_LERP(a,b,t) ((a) + ((b) - (a)) * (t))
#define SM_SIGN(x) ((x) < 0 ? -1 : 1)
#define SM_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static inline f32 sm_lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
static inline f32 sm_clampf(f32 x, f32 lo, f32 hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline f32 sm_saturate(f32 x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
static inline f32 sm_signf(f32 x) { return x < 0.f ? -1.f : 1.f; }
static inline f32 sm_wrapf(f32 x, f32 m) { f32 r = fmodf(x, m); return r < 0.f ? r + m : r; }
static inline f32 sm_angle_diff(f32 a, f32 b) {
    f32 d = fmodf(a - b, SM_TAU);
    if (d > SM_PI) d -= SM_TAU;
    if (d < -SM_PI) d += SM_TAU;
    return d;
}
static inline f32 sm_approach(f32 cur, f32 target, f32 rate) {
    if (cur < target) return SM_MIN(cur + rate, target);
    if (cur > target) return SM_MAX(cur - rate, target);
    return target;
}
static inline f32 sm_smoothstep(f32 t) { t = sm_saturate(t); return t * t * (3.f - 2.f * t); }
static inline f32 sm_easeout(f32 t) { t = sm_saturate(t); return 1.f - (1.f - t) * (1.f - t); }
static inline f32 sm_easein(f32 t) { t = sm_saturate(t); return t * t; }
/* frame-rate independent exponential decay factor */
static inline f32 sm_decay(f32 rate, f32 dt) { return expf(-rate * dt); }

typedef struct { f32 x, y; } Vec2;
static inline Vec2 v2(f32 x, f32 y) { Vec2 r; r.x = x; r.y = y; return r; }
static inline Vec2 v2add(Vec2 a, Vec2 b) { return v2(a.x + b.x, a.y + b.y); }
static inline Vec2 v2sub(Vec2 a, Vec2 b) { return v2(a.x - b.x, a.y - b.y); }
static inline Vec2 v2mul(Vec2 a, f32 s) { return v2(a.x * s, a.y * s); }
static inline f32  v2dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
static inline f32  v2len(Vec2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
static inline f32  v2len2(Vec2 a) { return a.x * a.x + a.y * a.y; }
static inline f32  v2dist(Vec2 a, Vec2 b) { return v2len(v2sub(a, b)); }
static inline f32  v2dist2(Vec2 a, Vec2 b) { return v2len2(v2sub(a, b)); }
static inline Vec2 v2norm(Vec2 a) { f32 l = v2len(a); return l > 1e-6f ? v2(a.x / l, a.y / l) : v2(0, 0); }
static inline Vec2 v2from_angle(f32 a) { return v2(cosf(a), sinf(a)); }
static inline Vec2 v2rot(Vec2 a, f32 ang) {
    f32 c = cosf(ang), s = sinf(ang);
    return v2(a.x * c - a.y * s, a.x * s + a.y * c);
}
static inline Vec2 v2lerp(Vec2 a, Vec2 b, f32 t) { return v2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); }
static inline f32  v2angle(Vec2 a) { return atan2f(a.y, a.x); }

typedef struct { f32 x, y, w, h; } Rect;
static inline Rect rect(f32 x, f32 y, f32 w, f32 h) { Rect r; r.x = x; r.y = y; r.w = w; r.h = h; return r; }
static inline int rect_contains(Rect r, f32 px, f32 py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

/* ---------------------------------------------------------------- RNG ----- */
typedef struct { u64 s; } Rng;
static inline void rng_seed(Rng *r, u64 seed) { r->s = seed ? seed : 0x9E3779B97F4A7C15ull; }
static inline u32 rng_u32(Rng *r) {
    r->s ^= r->s << 13; r->s ^= r->s >> 7; r->s ^= r->s << 17;
    return (u32)(r->s >> 32);
}
static inline u32 rng_range_u32(Rng *r, u32 n) { return n ? rng_u32(r) % n : 0; }
static inline f32 rng_f(Rng *r) { return (f32)(rng_u32(r) >> 8) * (1.0f / 16777216.0f); }
static inline f32 rng_range(Rng *r, f32 lo, f32 hi) { return lo + (hi - lo) * rng_f(r); }
static inline int rng_int(Rng *r, int lo, int hi) { return lo + (int)rng_range_u32(r, (u32)(hi - lo + 1)); }
static inline f32 rng_sign(Rng *r) { return (rng_u32(r) & 1) ? 1.f : -1.f; }

/* Deterministic value noise (cheap, used for shake / wobble) */
static inline f32 hash1(u32 x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (f32)(x & 0xFFFFFF) / (f32)0xFFFFFF;
}

/* ------------------------------------------------------------ logging ----- */
typedef void (*SmLogFn)(const char *msg, void *user);
void sm_log_set(SmLogFn fn, void *user);
void sm_logf(const char *fmt, ...);
void sm_warnf(const char *fmt, ...);
void sm_errorf(const char *fmt, ...);

/* ------------------------------------------------------------- memory ----- */
/* Simple bump arena + free (file-scope pools). Keeps allocation sites explicit. */
void *sm_alloc(usize bytes);
void *sm_calloc(usize count, usize bytes);
void  sm_free(void *p);
usize sm_mem_used(void);

/* Portable float->int fast floor */
static inline int sm_ifloor(f32 x) { int i = (int)x; return (x < (f32)i) ? i - 1 : i; }
static inline int sm_iround(f32 x) { return (int)(x + (x >= 0.f ? 0.5f : -0.5f)); }

/* String helpers */
void sm_strlcpy(char *dst, const char *src, usize cap);
void sm_strlcat(char *dst, const char *src, usize cap);
int  sm_streq(const char *a, const char *b);
void sm_sprintf(char *buf, usize cap, const char *fmt, ...);
/* Format a large number with thousands separators: 1234567 -> "1,234,567" */
void sm_format_num(char *buf, usize cap, s64 value);
void sm_format_credits(char *buf, usize cap, s64 value);
void sm_format_time(char *buf, usize cap, f32 seconds);

#endif /* SM_COMMON_H */
