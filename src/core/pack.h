/* SPACE MINER — core/pack.h  asset pack reader */
#ifndef SM_PACK_H
#define SM_PACK_H
#include "common.h"
#include "gfx.h"

#define SM_PAK_MAGIC 0x4B504D53u /* 'SMPK' */
#define SM_PAK_VERSION 2u

enum { PAK_IMAGE = 0, PAK_PCM = 1, PAK_BLOB = 2 };

typedef struct {
    char name[48];
    u32 w, h, frames;
    u32 kind;
    u32 raw_size;
    u32 data_size;
    u32 offset;      /* offset into pak data blob */
} PakEntry;

typedef struct {
    const u8 *data;      /* whole file image */
    usize size;
    PakEntry *entries;
    int count;
    usize data_base;   /* absolute offset of the payload section */
} Pak;

int pak_open(Pak *p, const void *data, usize size);
const PakEntry *pak_find(const Pak *p, const char *name);
int pak_load_sheet(const Pak *p, const char *name, Sheet *out);
int pak_load_tex(const Pak *p, const char *name, Tex *out);
int pak_load_blob(const Pak *p, const char *name, void *dst, usize cap);
const char *pak_blob_str(const Pak *p, const char *name);   /* NUL-terminated, cached */

#endif
