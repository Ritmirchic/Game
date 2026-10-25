/* ============================================================================
   SPACE MINER — core/pack.c
   ========================================================================== */
#include "pack.h"
#include "inflate.h"

typedef struct {
    u32 magic, version, count;
} PakHeader;

int pak_open(Pak *p, const void *data, usize size) {
    memset(p, 0, sizeof(*p));
    if (!data || size < sizeof(PakHeader)) return -1;
    const u8 *b = (const u8 *)data;
    PakHeader h;
    memcpy(&h, b, sizeof(h));
    if (h.magic != SM_PAK_MAGIC || h.version != SM_PAK_VERSION) {
        sm_errorf("asset pack: bad magic/version (%08x v%u)", h.magic, h.version);
        return -1;
    }
    if (h.count > 20000) return -1;
    usize entryBytes = (usize)h.count * sizeof(PakEntry);
    if (sizeof(PakHeader) + entryBytes > size) return -1;
    p->entries = (PakEntry *)sm_alloc(entryBytes);
    if (!p->entries) return -1;
    memcpy(p->entries, b + sizeof(PakHeader), entryBytes);
    p->count = (int)h.count;
    p->data_base = sizeof(PakHeader) + entryBytes;
    p->data = b;
    p->size = size;
    return 0;
}

const PakEntry *pak_find(const Pak *p, const char *name) {
    for (int i = 0; i < p->count; i++) {
        if (!strncmp(p->entries[i].name, name, sizeof(p->entries[i].name))) return &p->entries[i];
    }
    return NULL;
}

int pak_load_blob(const Pak *p, const char *name, void *dst, usize cap) {
    const PakEntry *e = pak_find(p, name);
    if (!e) { sm_errorf("asset pack: missing '%s'", name); return -1; }
    if (e->raw_size > cap) { sm_errorf("asset pack: '%s' too large (%u > %u)", name, e->raw_size, (u32)cap); return -1; }
    if (p->data_base + e->offset + e->data_size > p->size) return -1;
    usize outLen = 0;
    if (sm_inflate(dst, cap, &outLen, p->data + p->data_base + e->offset, e->data_size) != 0) {
        sm_errorf("asset pack: inflate failed for '%s'", name);
        return -1;
    }
    if (outLen < e->raw_size) return -1;
    return (int)e->raw_size;
}

static u8 *load_blob_alloc(const Pak *p, const char *name, usize *outLenPtr) {
    const PakEntry *e = pak_find(p, name);
    if (!e) { sm_errorf("asset pack: missing '%s'", name); return NULL; }
    u8 *buf = (u8 *)sm_alloc((usize)e->raw_size + 1);
    if (!buf) return NULL;
    usize got = 0;
    if (sm_inflate(buf, (usize)e->raw_size + 1, &got, p->data + p->data_base + e->offset, e->data_size) != 0) {
        sm_errorf("asset pack: inflate failed for '%s'", name);
        return NULL;
    }
    buf[e->raw_size] = 0;
    if (got < e->raw_size) return NULL;
    if (outLenPtr) *outLenPtr = got;
    return buf;
}

const char *pak_blob_str(const Pak *p, const char *name) {
    usize len = 0;
    u8 *b = load_blob_alloc(p, name, &len);
    return (const char *)b;
}

int pak_load_sheet(const Pak *p, const char *name, Sheet *out) {
    const PakEntry *e = pak_find(p, name);
    if (!e) { sm_errorf("asset pack: missing sheet '%s'", name); return -1; }
    if (e->kind != PAK_IMAGE) return -1;
    if (e->w == 0 || e->h == 0 || e->frames == 0) return -1;
    usize expect = (usize)e->w * (usize)e->h * (usize)e->frames * 4u;
    if (expect != (usize)e->raw_size) { sm_errorf("asset pack: size mismatch for '%s'", name); return -1; }
    u32 *px = (u32 *)sm_alloc(expect);
    if (!px) return -1;
    if (pak_load_blob(p, name, px, expect) != (int)e->raw_size) return -1;
    out->w = (int)e->w; out->h = (int)e->h; out->frames = (int)e->frames; out->px = px;
    return 0;
}

int pak_load_tex(const Pak *p, const char *name, Tex *out) {
    Sheet s;
    if (pak_load_sheet(p, name, &s) != 0) return -1;
    out->w = s.w; out->h = s.h; out->px = s.px;
    return 0;
}
