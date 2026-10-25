/* ============================================================================
   SPACE MINER — core/save.c
   Versioned, checksummed save files with validation and corruption handling.
   ========================================================================== */
#include "state.h"
#include "inflate.h"

#define SAVE_MAGIC 0x56534D53u  /* 'SMSV' */
#define SAVE_VERSION 3u
#define SAVE_FILE "spaceminer.sav"
#define SAVE_BACKUP "spaceminer.bak"

typedef struct {
    u8 *data;
    usize cap, pos;
    int overflow;
} Writer;

static void wr_u32(Writer *w, u32 v) {
    if (w->pos + 4 > w->cap) { w->overflow = 1; return; }
    w->data[w->pos++] = (u8)(v & 0xFF);
    w->data[w->pos++] = (u8)((v >> 8) & 0xFF);
    w->data[w->pos++] = (u8)((v >> 16) & 0xFF);
    w->data[w->pos++] = (u8)((v >> 24) & 0xFF);
}
static void wr_i32(Writer *w, s32 v) { wr_u32(w, (u32)v); }
static void wr_u64(Writer *w, u64 v) { wr_u32(w, (u32)(v & 0xFFFFFFFFu)); wr_u32(w, (u32)(v >> 32)); }
static void wr_f32(Writer *w, f32 v) {
    union { f32 f; u32 u; } cv;
    cv.f = v;
    wr_u32(w, cv.u);
}

typedef struct { const u8 *data; usize len, pos; int err; } Reader;
static u32 rd_u32(Reader *r) {
    if (r->pos + 4 > r->len) { r->err = 1; return 0; }
    u32 v = (u32)r->data[r->pos] | ((u32)r->data[r->pos + 1] << 8) |
            ((u32)r->data[r->pos + 2] << 16) | ((u32)r->data[r->pos + 3] << 24);
    r->pos += 4;
    return v;
}
static s32 rd_i32(Reader *r) { return (s32)rd_u32(r); }
static u64 rd_u64(Reader *r) {
    u64 lo = rd_u32(r), hi = rd_u32(r);
    return lo | (hi << 32);
}
static f32 rd_f32(Reader *r) {
    union { f32 f; u32 u; } cv;
    cv.u = rd_u32(r);
    return cv.f;
}

void save_apply_defaults(Game *g) {
    SaveData *d = &g->save;
    memset(d, 0, sizeof(*d));
    d->version = SAVE_VERSION;
    d->credits = 0;
    d->xp = 0;
    d->level = 1;
    d->ship = SHIP_PROSPECTOR;
    d->ship_owned[SHIP_PROSPECTOR] = 1;
    d->weapon = WEAPON_PULSE;
    d->weapon_owned[WEAPON_PULSE] = 1;
    d->sector_unlocked[0] = 1;
    d->sector_current = 0;
    d->drone_count = 0;
    d->achievements = 0;
    d->gameplay.difficulty = 1;
    d->gameplay.show_hints = 1;
    d->gameplay.autosave = 1;
    d->audio.master = 0.85f;
    d->audio.music = 0.55f;
    d->audio.sfx = 0.85f;
    d->audio.muted = 0;
    d->video.fullscreen = 0;
    d->video.bloom = 1;
    d->video.shake = 1;
    d->video.particles = 3;
    d->video.scanlines = 0;
    d->video.ui_scale = 1.f;
    d->valid = 1;
}

int save_validate(SaveData *d) {
    if (!d) return 0;
    if (d->credits < 0 || d->credits > 1000000000000ll) return 0;
    if (d->level < 1 || d->level > 40) return 0;
    if (d->xp < 0) return 0;
    if (d->ship < 0 || d->ship >= SHIP_COUNT) return 0;
    if (d->weapon < 0 || d->weapon >= WEAPON_COUNT) return 0;
    if (d->sector_current < 0 || d->sector_current >= SECTOR_COUNT) return 0;
    for (int i = 0; i < UPG_COUNT; i++) if (d->upgrades[i] < 0 || d->upgrades[i] > UPG_MAX_LEVEL) return 0;
    for (int i = 0; i < RES_COUNT; i++) if (d->cargo[i] < 0 || d->cargo[i] > 2000000) return 0;
    if (d->drone_count < 0 || d->drone_count > SM_MAX_DRONES) return 0;
    for (int i = 0; i < DRONE_TYPE_COUNT; i++) if (d->drone_owned[i] < 0 || d->drone_owned[i] > SM_MAX_DRONES) return 0;
    for (int i = 0; i < SECTOR_COUNT; i++) if (d->sector_unlocked[i] != 0 && d->sector_unlocked[i] != 1) return 0;
    if (!d->sector_unlocked[0]) return 0;
    if (d->audio.master < 0.f || d->audio.master > 1.f) d->audio.master = 0.85f;
    if (d->audio.music < 0.f || d->audio.music > 1.f) d->audio.music = 0.55f;
    if (d->audio.sfx < 0.f || d->audio.sfx > 1.f) d->audio.sfx = 0.85f;
    if (d->video.particles < 1 || d->video.particles > 3) d->video.particles = 3;
    if (d->video.ui_scale < 0.75f || d->video.ui_scale > 1.5f) d->video.ui_scale = 1.f;
    if (d->gameplay.difficulty < 0 || d->gameplay.difficulty > 2) d->gameplay.difficulty = 1;
    d->valid = 1;
    return 1;
}

static void write_payload(Writer *w, SaveData *d) {
    wr_u32(w, d->version);
    wr_u64(w, (u64)d->credits);
    wr_i32(w, d->xp);
    wr_i32(w, d->level);
    wr_i32(w, d->ship);
    for (int i = 0; i < SHIP_COUNT; i++) wr_i32(w, d->ship_owned[i]);
    wr_i32(w, d->weapon);
    for (int i = 0; i < WEAPON_COUNT; i++) wr_i32(w, d->weapon_owned[i]);
    for (int i = 0; i < UPG_COUNT; i++) wr_i32(w, d->upgrades[i]);
    for (int i = 0; i < SECTOR_COUNT; i++) wr_i32(w, d->sector_unlocked[i]);
    wr_i32(w, d->sector_current);
    for (int i = 0; i < DRONE_TYPE_COUNT; i++) wr_i32(w, d->drone_owned[i]);
    wr_i32(w, d->drone_count);
    wr_u32(w, d->achievements);
    for (int i = 0; i < RES_COUNT; i++) wr_i32(w, d->cargo[i]);
    wr_i32(w, d->cargo_used);
    wr_i32(w, d->stat_enemies_killed);
    wr_i32(w, d->stat_asteroids_mined);
    wr_u64(w, (u64)d->stat_credits_earned);
    wr_i32(w, d->stat_crystals_sold);
    wr_f32(w, d->stat_time_played);
    wr_f32(w, d->stat_distance);
    wr_i32(w, d->stat_sentinels_killed);
    wr_i32(w, d->gameplay.difficulty);
    wr_i32(w, d->gameplay.show_hints);
    wr_i32(w, d->gameplay.autosave);
    wr_f32(w, d->audio.master);
    wr_f32(w, d->audio.music);
    wr_f32(w, d->audio.sfx);
    wr_i32(w, d->audio.muted);
    wr_i32(w, d->video.fullscreen);
    wr_i32(w, d->video.bloom);
    wr_i32(w, d->video.shake);
    wr_i32(w, d->video.particles);
    wr_i32(w, d->video.scanlines);
    wr_f32(w, d->video.ui_scale);
}

#define SAVE_PAYLOAD_BYTES (4 + 8 + 4 + 4 + 4 + SHIP_COUNT*4 + 4 + WEAPON_COUNT*4 + UPG_COUNT*4 + \
                            SECTOR_COUNT*4 + 4 + DRONE_TYPE_COUNT*4 + 4 + 4 + RES_COUNT*4 + 4 + \
                            4*3 + 8 + 4 + 4 + 4 + 4 + 4*2 + 4*3 + 4*5 + 4)

int save_write(Game *g) {
    SaveData *d = &g->save;
    d->version = SAVE_VERSION;
    if (!save_validate(d)) {
        sm_errorf("save_write: refusing to write invalid save data");
        return -1;
    }
    u8 buf[2048];
    Writer w;
    w.data = buf; w.cap = sizeof(buf); w.pos = 0; w.overflow = 0;
    write_payload(&w, d);
    if (w.overflow) return -1;
    usize payload = w.pos;
    /* header: magic, version, payload size, crc */
    u8 out[2100];
    usize o = 0;
    u32 magic = SAVE_MAGIC, ver = SAVE_VERSION;
    u32 crc = sm_crc32(buf, payload, 0);
    memcpy(out + o, &magic, 4); o += 4;
    memcpy(out + o, &ver, 4); o += 4;
    u32 pl = (u32)payload;
    memcpy(out + o, &pl, 4); o += 4;
    memcpy(out + o, &crc, 4); o += 4;
    memcpy(out + o, buf, payload); o += payload;
    if (!g->plat->write_file) return -1;
    if (g->plat->write_file(SAVE_FILE, out, o, g->plat->user) != 0) {
        sm_errorf("save_write: file write failed");
        return -1;
    }
    /* keep a rolling backup */
    if (g->plat->write_file) g->plat->write_file(SAVE_BACKUP, out, o, g->plat->user);
    return 0;
}

static int read_payload(SaveData *d, const u8 *payload, usize len) {
    Reader r;
    r.data = payload; r.len = len; r.pos = 0; r.err = 0;
    d->version = rd_u32(&r);
    d->credits = (s64)rd_u64(&r);
    d->xp = rd_i32(&r);
    d->level = rd_i32(&r);
    d->ship = rd_i32(&r);
    for (int i = 0; i < SHIP_COUNT; i++) d->ship_owned[i] = rd_i32(&r);
    d->weapon = rd_i32(&r);
    for (int i = 0; i < WEAPON_COUNT; i++) d->weapon_owned[i] = rd_i32(&r);
    for (int i = 0; i < UPG_COUNT; i++) d->upgrades[i] = rd_i32(&r);
    for (int i = 0; i < SECTOR_COUNT; i++) d->sector_unlocked[i] = rd_i32(&r);
    d->sector_current = rd_i32(&r);
    for (int i = 0; i < DRONE_TYPE_COUNT; i++) d->drone_owned[i] = rd_i32(&r);
    d->drone_count = rd_i32(&r);
    d->achievements = rd_u32(&r);
    for (int i = 0; i < RES_COUNT; i++) d->cargo[i] = rd_i32(&r);
    d->cargo_used = rd_i32(&r);
    d->stat_enemies_killed = rd_i32(&r);
    d->stat_asteroids_mined = rd_i32(&r);
    d->stat_credits_earned = (s64)rd_u64(&r);
    d->stat_crystals_sold = rd_i32(&r);
    d->stat_time_played = rd_f32(&r);
    d->stat_distance = rd_f32(&r);
    d->stat_sentinels_killed = rd_i32(&r);
    d->gameplay.difficulty = rd_i32(&r);
    d->gameplay.show_hints = rd_i32(&r);
    d->gameplay.autosave = rd_i32(&r);
    d->audio.master = rd_f32(&r);
    d->audio.music = rd_f32(&r);
    d->audio.sfx = rd_f32(&r);
    d->audio.muted = rd_i32(&r);
    d->video.fullscreen = rd_i32(&r);
    d->video.bloom = rd_i32(&r);
    d->video.shake = rd_i32(&r);
    d->video.particles = rd_i32(&r);
    d->video.scanlines = rd_i32(&r);
    d->video.ui_scale = rd_f32(&r);
    return r.err ? -1 : 0;
}

static int try_load(Game *g, const char *name, SaveData *out) {
    u8 *data = NULL;
    usize len = 0;
    if (!g->plat->read_file) return -1;
    g->plat->read_file(name, &data, &len, g->plat->user);
    if (!data || len < 16) {
        if (data) free(data);
        return -1;
    }
    u32 magic, ver, plen, crc;
    memcpy(&magic, data, 4);
    memcpy(&ver, data + 4, 4);
    memcpy(&plen, data + 8, 4);
    memcpy(&crc, data + 12, 4);
    if (magic != SAVE_MAGIC) { sm_warnf("save: bad magic in %s", name); free(data); return -1; }
    if (ver != SAVE_VERSION) { sm_warnf("save: version %u != %u", ver, SAVE_VERSION); free(data); return -1; }
    if (16 + plen > len) { sm_warnf("save: truncated payload"); free(data); return -1; }
    u32 actual = sm_crc32(data + 16, plen, 0);
    if (actual != crc) { sm_warnf("save: checksum mismatch (%08x != %08x)", actual, crc); free(data); return -1; }
    SaveData tmp;
    memset(&tmp, 0, sizeof(tmp));
    if (read_payload(&tmp, data + 16, plen) != 0) { sm_warnf("save: parse error"); free(data); return -1; }
    free(data);
    if (!save_validate(&tmp)) { sm_warnf("save: validation failed"); return -1; }
    *out = tmp;
    return 0;
}

int save_read(Game *g) {
    SaveData tmp;
    if (try_load(g, SAVE_FILE, &tmp) == 0) {
        g->save = tmp;
        sm_logf("save loaded: level %d, %lld credits, sector %d", g->save.level, (long long)g->save.credits, g->save.sector_current);
        return 0;
    }
    if (try_load(g, SAVE_BACKUP, &tmp) == 0) {
        g->save = tmp;
        sm_warnf("save: recovered from backup");
        return 0;
    }
    return -1;
}
