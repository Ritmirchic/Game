/* SPACE MINER — core/inflate.h */
#ifndef SM_INFLATE_H
#define SM_INFLATE_H
#include "common.h"

u32  sm_crc32(const void *data, usize len, u32 crc);
u32  sm_adler32(const void *data, usize len);

/* Decompress a zlib stream into dst.  Returns 0 on success. */
int  sm_inflate(void *dst, usize dstCap, usize *dstLen, const void *src, usize srcLen);
/* Compress with stored (uncompressed) deflate blocks; always succeeds if cap fits. */
int  sm_deflate_stored(void *dst, usize dstCap, usize *dstLen, const void *src, usize srcLen);

#endif
