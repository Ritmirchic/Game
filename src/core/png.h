/* SPACE MINER — core/png.h  (PNG decode + encode, self contained) */
#ifndef SM_PNG_H
#define SM_PNG_H
#include "common.h"

typedef struct {
    int w, h;
    u32 *px;          /* RGBA8888, 0xAABBGGRR order in memory as bytes R,G,B,A */
    int owns_pixels;
} Image;

/* Decode PNG from memory into an RGBA8888 buffer (allocated with sm_alloc).
   Supports 8-bit grayscale / RGB / RGBA / palette, non-interlaced. Returns 0 on ok. */
int  png_decode(const void *data, usize len, Image *out);
/* Encode RGBA8888 buffer to PNG. Returns malloc'd buffer + size. */
u8  *png_encode(const u32 *px, int w, int h, usize *outLen);

void image_free(Image *img);

#endif
