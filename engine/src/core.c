#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "core.h"
#include <stdarg.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

// ---------------------------------------------------------------- arena
void *arena_alloc(Arena *a, size_t n) {
    n = (n + 15) & ~(size_t)15;
    if (!a->head || a->head->used + n > a->head->cap) {
        size_t cap = n > (1u << 20) ? n : (1u << 20);
        if (a->head && a->head->cap > cap) cap = a->head->cap;
        ArenaBlock *b = (ArenaBlock *)malloc(sizeof(ArenaBlock) + cap);
        if (!b) { LOGE("arena OOM (%zu bytes)", n); exit(1); }
        b->next = a->head; b->cap = cap; b->used = 0;
        a->head = b;
        a->total += cap;
    }
    void *p = a->head->data + a->head->used;
    a->head->used += n;
    return p;
}
void *arena_zalloc(Arena *a, size_t n) { void *p = arena_alloc(a, n); memset(p, 0, n); return p; }
char *arena_strdup(Arena *a, const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)arena_alloc(a, n);
    memcpy(p, s, n);
    return p;
}
void arena_reset(Arena *a) {
    for (ArenaBlock *b = a->head; b; b = b->next) b->used = 0;
}
void arena_free(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) { ArenaBlock *n = b->next; free(b); b = n; }
    a->head = NULL; a->total = 0;
}

// ---------------------------------------------------------------- logging
static FILE *g_logfile = NULL;
static int g_log_level = LOG_INFO;
void log_set_file(const char *path) {
    if (g_logfile) fclose(g_logfile);
    g_logfile = path ? fopen(path, "wb") : NULL;
}
void log_msg(int level, const char *fmt, ...) {
    static const char *tags[] = {"DBG", "INF", "WRN", "ERR"};
    if (level < g_log_level) return;
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(stdout, "[%s] %s\n", tags[level & 3], buf);
    fflush(stdout);
    if (g_logfile) {
        fprintf(g_logfile, "[%s] %s\n", tags[level & 3], buf);
        fflush(g_logfile);
    }
}

// ---------------------------------------------------------------- hash/noise
uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static inline float h2f(uint32_t h) { return (float)(h & 0xFFFFFF) / 16777216.f; }
static inline float h1d(int i, int seed) { return h2f(hash32((uint32_t)i * 374761393U + (uint32_t)seed * 668265263U)); }

float value_noise2(float x, float y, int seed) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = h1d(ix + iy * 7919, seed), b = h1d(ix + 1 + iy * 7919, seed);
    float c = h1d(ix + (iy + 1) * 7919, seed), d = h1d(ix + 1 + (iy + 1) * 7919, seed);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
static inline float h3d(int x, int y, int z, int seed) {
    return h2f(hash32((uint32_t)x * 374761393U + (uint32_t)y * 1103515245U + (uint32_t)z * 69069U + (uint32_t)seed * 2246822519U));
}
float value_noise3(float x, float y, float z, int seed) {
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    float c000 = h3d(ix, iy, iz, seed), c100 = h3d(ix + 1, iy, iz, seed);
    float c010 = h3d(ix, iy + 1, iz, seed), c110 = h3d(ix + 1, iy + 1, iz, seed);
    float c001 = h3d(ix, iy, iz + 1, seed), c101 = h3d(ix + 1, iy, iz + 1, seed);
    float c011 = h3d(ix, iy + 1, iz + 1, seed), c111 = h3d(ix + 1, iy + 1, iz + 1, seed);
    float x00 = lerpf(c000, c100, fx), x10 = lerpf(c010, c110, fx);
    float x01 = lerpf(c001, c101, fx), x11 = lerpf(c011, c111, fx);
    return lerpf(lerpf(x00, x10, fy), lerpf(x01, x11, fy), fz);
}
float fbm2(float x, float y, int octaves, int seed) {
    float s = 0.f, a = 0.5f, f = 1.f, norm = 0.f;
    for (int i = 0; i < octaves; i++) {
        s += value_noise2(x * f, y * f, seed + i * 31) * a;
        norm += a; a *= 0.5f; f *= 2.03f;
    }
    return s / (norm + 1e-9f);
}
float fbm3(float x, float y, float z, int octaves, int seed) {
    float s = 0.f, a = 0.5f, f = 1.f, norm = 0.f;
    for (int i = 0; i < octaves; i++) {
        s += value_noise3(x * f, y * f, z * f, seed + i * 37) * a;
        norm += a; a *= 0.5f; f *= 2.01f;
    }
    return s / (norm + 1e-9f);
}

double core_now(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    static int init = 0;
    if (!init) { QueryPerformanceFrequency(&freq); init = 1; }
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}
