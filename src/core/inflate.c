/* ============================================================================
   SPACE MINER — core/inflate.c  (+ checksums, raw deflate encoder)
   Self-contained DECODE implementation of RFC 1950/1951 (zlib / deflate) and
   a minimal stored-block encoder used for in-game screenshot PNG output.
   No external dependencies, no libcrypto, no zlib linkage.
   ========================================================================== */
#include "common.h"
#include "inflate.h"

/* ------------------------------------------------------------ checksums --- */
static u32 g_crc_table[256];
static int g_crc_ready = 0;

u32 sm_crc32(const void *data, usize len, u32 crc) {
    if (!g_crc_ready) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            g_crc_table[i] = c;
        }
        g_crc_ready = 1;
    }
    const u8 *p = (const u8 *)data;
    crc = crc ^ 0xFFFFFFFFu;
    for (usize i = 0; i < len; i++) crc = g_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

u32 sm_adler32(const void *data, usize len) {
    const u8 *p = (const u8 *)data;
    u32 a = 1, b = 0;
    while (len) {
        usize n = len > 5552 ? 5552 : len;
        len -= n;
        while (n--) { a += *p++; b += a; }
        a %= 65521u; b %= 65521u;
    }
    return (b << 16) | a;
}

/* ------------------------------------------------------------ bit reader -- */
typedef struct {
    const u8 *src;
    usize len;
    usize pos;
    u32 bitbuf;
    int bitcnt;
    int err;
} BitReader;

static inline void br_init(BitReader *br, const u8 *src, usize len) {
    br->src = src; br->len = len; br->pos = 0; br->bitbuf = 0; br->bitcnt = 0; br->err = 0;
}

static inline void br_need(BitReader *br, int n) {
    while (br->bitcnt < n) {
        if (br->pos >= br->len) { br->err = 1; return; }
        br->bitbuf |= (u32)br->src[br->pos++] << br->bitcnt;
        br->bitcnt += 8;
    }
}

static inline u32 br_bits(BitReader *br, int n) {
    if (n == 0) return 0;
    br_need(br, n);
    u32 v = br->bitbuf & ((1u << n) - 1u);
    br->bitbuf >>= n;
    br->bitcnt -= n;
    return v;
}

static inline void br_align(BitReader *br) {
    int drop = br->bitcnt & 7;
    br->bitbuf >>= drop;
    br->bitcnt -= drop;
}

/* ---------------------------------------------------------- huffman dec --- */
#define HUFF_FAST_BITS 10
#define HUFF_MAX_BITS  15

typedef struct {
    u16 fast[1 << HUFF_FAST_BITS];   /* symbol | (len<<9), 0 = slow path */
    u16 count[HUFF_MAX_BITS + 1];
    u16 symbol[288];
} Huff;

static int huff_build(Huff *h, const u8 *lengths, int n) {
    int i;
    memset(h->count, 0, sizeof(h->count));
    memset(h->fast, 0, sizeof(h->fast));
    for (i = 0; i < n; i++) h->count[lengths[i]]++;
    if (h->count[0] == n) return 0; /* empty table is legal (no codes) */
    int left = 1;
    for (i = 1; i <= HUFF_MAX_BITS; i++) {
        left <<= 1;
        left -= h->count[i];
        if (left < 0) return -1; /* over-subscribed */
    }
    u16 offs[HUFF_MAX_BITS + 2];
    offs[1] = 0;
    for (i = 1; i <= HUFF_MAX_BITS; i++) offs[i + 1] = offs[i] + h->count[i];
    for (i = 0; i < n; i++) if (lengths[i]) h->symbol[offs[lengths[i]]++] = (u16)i;

    /* Build fast lookup: canonical codes are transmitted MSB-first, the bit
       reader consumes LSB-first, so index bits are the reversed code. */
    int code = 0, idx = 0;
    for (int len = 1; len <= HUFF_MAX_BITS; len++) {
        int cnt = h->count[len];
        for (int k = 0; k < cnt; k++) {
            u16 sym = h->symbol[idx + k];
            if (len <= HUFF_FAST_BITS) {
                int rev = 0, c = code + k;
                for (int b = 0; b < len; b++) { rev = (rev << 1) | ((c >> b) & 1); }
                int stride = 1 << len;
                for (int fill = rev; fill < (1 << HUFF_FAST_BITS); fill += stride)
                    h->fast[fill] = (u16)(sym | (len << 9));
            }
        }
        idx += cnt;
        code = (code + cnt) << 1;
    }
    return 0;
}

static int huff_decode(BitReader *br, const Huff *h) {
    br_need(br, HUFF_FAST_BITS);
    if (!br->err) {
        u32 peek = br->bitbuf & ((1u << HUFF_FAST_BITS) - 1u);
        u16 e = h->fast[peek];
        if (e) {
            int len = e >> 9;
            br->bitbuf >>= len; br->bitcnt -= len;
            return e & 511;
        }
    }
    /* slow path */
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= HUFF_MAX_BITS; len++) {
        code |= (int)br_bits(br, 1);
        if (br->err) return -1;
        int count = h->count[len];
        if (code - first < count) return h->symbol[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

/* ----------------------------------------------------------- length tables */
static const u16 k_len_base[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const u8  k_len_extra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const u16 k_dist_base[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const u8  k_dist_extra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static const u8 k_len_order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

static int inflate_block(BitReader *br, u8 *dst, usize dstCap, usize *outPos) {
    Huff lit, dist;
    int final = (int)br_bits(br, 1);
    int type = (int)br_bits(br, 2);
    if (br->err) return -1;

    if (type == 0) {
        br_align(br);
        /* Drop to a clean byte-stream state: buffered whole bytes + position. */
        usize bytePos = br->pos - (usize)(br->bitcnt / 8);
        br->bitbuf = 0;
        br->bitcnt = 0;
        if (bytePos + 4 > br->len) return -1;
        u32 len  = (u32)br->src[bytePos] | ((u32)br->src[bytePos + 1] << 8);
        u32 nlen = (u32)br->src[bytePos + 2] | ((u32)br->src[bytePos + 3] << 8);
        bytePos += 4;
        if ((len ^ 0xFFFFu) != nlen) return -1;
        if (*outPos + len > dstCap) return -1;
        if (bytePos + len > br->len) return -1;
        memcpy(dst + *outPos, br->src + bytePos, len);
        br->pos = bytePos + len;
        *outPos += len;
        return final ? 1 : 0;
    }

    if (type == 1) {
        u8 lens[288];
        int i;
        for (i = 0; i < 144; i++) lens[i] = 8;
        for (; i < 256; i++) lens[i] = 9;
        for (; i < 280; i++) lens[i] = 7;
        for (; i < 288; i++) lens[i] = 8;
        if (huff_build(&lit, lens, 288) < 0) return -1;
        for (i = 0; i < 30; i++) lens[i] = 5;
        if (huff_build(&dist, lens, 30) < 0) return -1;
    } else if (type == 2) {
        int hlit = (int)br_bits(br, 5) + 257;
        int hdist = (int)br_bits(br, 5) + 1;
        int hclen = (int)br_bits(br, 4) + 4;
        if (br->err) return -1;
        u8 cl[19];
        memset(cl, 0, sizeof(cl));
        for (int i = 0; i < hclen; i++) cl[k_len_order[i]] = (u8)br_bits(br, 3);
        Huff clh;
        if (huff_build(&clh, cl, 19) < 0) return -1;
        u8 lens[320];
        int n = 0;
        while (n < hlit + hdist) {
            int sym = huff_decode(br, &clh);
            if (sym < 0 || br->err) return -1;
            if (sym < 16) lens[n++] = (u8)sym;
            else if (sym == 16) {
                if (n == 0) return -1;
                u8 prev = lens[n - 1];
                int rep = 3 + (int)br_bits(br, 2);
                if (n + rep > hlit + hdist) return -1;
                while (rep--) lens[n++] = prev;
            } else if (sym == 17) {
                int rep = 3 + (int)br_bits(br, 3);
                if (n + rep > hlit + hdist) return -1;
                while (rep--) lens[n++] = 0;
            } else {
                int rep = 11 + (int)br_bits(br, 7);
                if (n + rep > hlit + hdist) return -1;
                while (rep--) lens[n++] = 0;
            }
        }
        if (huff_build(&lit, lens, hlit) < 0) return -1;
        if (huff_build(&dist, lens + hlit, hdist) < 0) return -1;
    } else {
        return -1;
    }

    for (;;) {
        int sym = huff_decode(br, &lit);
        if (sym < 0 || br->err) return -1;
        if (sym < 256) {
            if (*outPos >= dstCap) return -1;
            dst[(*outPos)++] = (u8)sym;
        } else if (sym == 256) {
            return final ? 1 : 0;
        } else {
            sym -= 257;
            if (sym >= 29) return -1;
            int len = k_len_base[sym] + (int)br_bits(br, k_len_extra[sym]);
            int dsym = huff_decode(br, &dist);
            if (dsym < 0 || dsym >= 30 || br->err) return -1;
            int d = k_dist_base[dsym] + (int)br_bits(br, k_dist_extra[dsym]);
            if ((usize)d > *outPos) return -1;
            if (*outPos + (usize)len > dstCap) return -1;
            u8 *o = dst + *outPos;
            const u8 *from = o - d;
            for (int i = 0; i < len; i++) o[i] = from[i];
            *outPos += (usize)len;
        }
    }
}

int sm_inflate(void *dst, usize dstCap, usize *dstLen, const void *src, usize srcLen) {
    const u8 *s = (const u8 *)src;
    if (srcLen < 2) return -1;
    int cmf = s[0], flg = s[1];
    if ((cmf & 0x0F) != 8) return -1;                 /* deflate only */
    if (((cmf << 8) | flg) % 31 != 0) return -1;      /* header checksum */
    BitReader br;
    br_init(&br, s + 2, srcLen - 2);
    usize outPos = 0;
    int r;
    do { r = inflate_block(&br, (u8 *)dst, dstCap, &outPos); } while (r == 0);
    if (dstLen) *dstLen = outPos;
    return (r == 1) ? 0 : -1;
}

/* ------------------------------------------------- zlib encode (stored) --- */
int sm_deflate_stored(void *dst, usize dstCap, usize *dstLen, const void *src, usize srcLen) {
    const u8 *in = (const u8 *)src;
    u8 *out = (u8 *)dst;
    usize need = 2 + srcLen + (srcLen / 65535 + 1) * 5 + 4;
    if (dstCap < need) return -1;
    usize o = 0;
    out[o++] = 0x78; out[o++] = 0x01;
    usize pos = 0;
    while (pos < srcLen) {
        usize chunk = srcLen - pos;
        if (chunk > 65535) chunk = 65535;
        int final = (pos + chunk >= srcLen);
        out[o++] = (u8)(final ? 1 : 0);
        out[o++] = (u8)(chunk & 0xFF); out[o++] = (u8)(chunk >> 8);
        out[o++] = (u8)(~chunk & 0xFF); out[o++] = (u8)((~chunk >> 8) & 0xFF);
        memcpy(out + o, in + pos, chunk);
        o += chunk;
        pos += chunk;
    }
    if (srcLen == 0) { out[o++] = 1; out[o++] = 0; out[o++] = 0; out[o++] = 0xFF; out[o++] = 0xFF; }
    u32 ad = sm_adler32(src, srcLen);
    out[o++] = (u8)(ad >> 24); out[o++] = (u8)(ad >> 16); out[o++] = (u8)(ad >> 8); out[o++] = (u8)ad;
    if (dstLen) *dstLen = o;
    return 0;
}
