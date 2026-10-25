/* ============================================================================
   SPACE MINER — core/common.c
   ========================================================================== */
#include "common.h"
#include <stdarg.h>

/* ------------------------------------------------------------ logging ----- */
static SmLogFn g_log_fn = NULL;
static void *g_log_user = NULL;

void sm_log_set(SmLogFn fn, void *user) { g_log_fn = fn; g_log_user = user; }

static void sm_vlog(const char *prefix, const char *fmt, va_list ap) {
    char buf[2048];
    int n = 0;
    if (prefix) n = snprintf(buf, sizeof(buf), "%s", prefix);
    vsnprintf(buf + n, sizeof(buf) - (usize)n, fmt, ap);
    if (g_log_fn) g_log_fn(buf, g_log_user);
    else { fputs(buf, stdout); fputc('\n', stdout); fflush(stdout); }
}

void sm_logf(const char *fmt, ...)   { va_list ap; va_start(ap, fmt); sm_vlog("[sm] ", fmt, ap); va_end(ap); }
void sm_warnf(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); sm_vlog("[sm][warn] ", fmt, ap); va_end(ap); }
void sm_errorf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); sm_vlog("[sm][error] ", fmt, ap); va_end(ap); }

/* ------------------------------------------------------------- memory ----- */
/* Arena-backed allocator: a few large blocks, individual frees are no-ops for
   small allocations (the game is a single long-lived session). Large blocks
   (assets) are allocated with the OS allocator directly. */
#define SM_ARENA_BLOCK (4u * 1024u * 1024u)
typedef struct ArenaBlock { struct ArenaBlock *next; usize used, cap; } ArenaBlock;

static ArenaBlock *g_arena = NULL;
static usize g_mem_used = 0;

static ArenaBlock *arena_new(usize cap) {
    ArenaBlock *b = (ArenaBlock *)malloc(sizeof(ArenaBlock) + cap);
    if (!b) return NULL;
    b->next = NULL; b->used = 0; b->cap = cap;
    return b;
}

void *sm_alloc(usize bytes) {
    bytes = (bytes + 15u) & ~(usize)15u;
    if (bytes == 0) bytes = 16;
    if (bytes > SM_ARENA_BLOCK / 2) {
        void *p = malloc(bytes);
        if (p) g_mem_used += bytes;
        return p;
    }
    if (!g_arena) g_arena = arena_new(SM_ARENA_BLOCK);
    if (!g_arena) return NULL;
    if (g_arena->used + bytes > g_arena->cap) {
        ArenaBlock *nb = arena_new(SM_ARENA_BLOCK > bytes ? SM_ARENA_BLOCK : bytes * 2);
        if (!nb) return NULL;
        nb->next = g_arena; g_arena = nb;
    }
    void *p = (u8 *)(g_arena + 1) + g_arena->used;
    g_arena->used += bytes;
    g_mem_used += bytes;
    return p;
}

void *sm_calloc(usize count, usize bytes) {
    usize total = count * bytes;
    void *p = sm_alloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void sm_free(void *p) {
    (void)p; /* arena frees are bulk-only */
}

usize sm_mem_used(void) { return g_mem_used; }

/* -------------------------------------------------------------- string ---- */
void sm_strlcpy(char *dst, const char *src, usize cap) {
    if (!cap) return;
    usize i = 0;
    if (src) { for (; i + 1 < cap && src[i]; i++) dst[i] = src[i]; }
    dst[i] = 0;
}

void sm_strlcat(char *dst, const char *src, usize cap) {
    usize len = strlen(dst);
    if (len >= cap) return;
    sm_strlcpy(dst + len, src, cap - len);
}

int sm_streq(const char *a, const char *b) {
    if (!a || !b) return a == b;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void sm_sprintf(char *buf, usize cap, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
}

void sm_format_num(char *buf, usize cap, s64 value) {
    char tmp[32];
    int neg = value < 0;
    u64 v = neg ? (u64)(-(value + 1)) + 1ull : (u64)value;
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + (int)(v % 10ull)); v /= 10ull; }
    int out = 0;
    if (neg && out + 1 < (int)cap) buf[out++] = '-';
    for (int i = n - 1; i >= 0; i--) {
        if (out + 1 >= (int)cap) break;
        buf[out++] = tmp[i];
        if (i > 0 && i % 3 == 0 && out + 1 < (int)cap) buf[out++] = ',';
    }
    buf[out] = 0;
}

void sm_format_credits(char *buf, usize cap, s64 value) {
    char num[48];
    sm_format_num(num, sizeof(num), value);
    sm_sprintf(buf, cap, "%s CR", num);
}

void sm_format_time(char *buf, usize cap, f32 seconds) {
    int total = (int)seconds;
    int h = total / 3600, m = (total / 60) % 60, s = total % 60;
    if (h > 0) sm_sprintf(buf, cap, "%d:%02d:%02d", h, m, s);
    else sm_sprintf(buf, cap, "%d:%02d", m, s);
}
