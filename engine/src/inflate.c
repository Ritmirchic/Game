// Minimal zlib/deflate decoder (RFC1950/1951). Public-domain style implementation.
#include "core.h"
#include <string.h>

typedef struct {
    const uint8_t *p;
    size_t len, pos;
    uint32_t buf;
    int cnt;
} BitR;

static inline int br_need(BitR *b, int n) {
    while (b->cnt < n) {
        if (b->pos >= b->len) return 0;
        b->buf |= (uint32_t)b->p[b->pos++] << b->cnt;
        b->cnt += 8;
    }
    return 1;
}
static inline uint32_t br_get(BitR *b, int n) {
    if (!br_need(b, n)) return 0;
    uint32_t v = b->buf & ((1u << n) - 1);
    b->buf >>= n;
    b->cnt -= n;
    return v;
}
static inline void br_align(BitR *b) { b->buf = 0; b->cnt = 0; }

typedef struct { short count[16]; short symbol[288]; } Huff;

static void huff_build(Huff *h, const short *lens, int n) {
    for (int i = 0; i < 16; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[lens[i]]++;
    h->count[0] = 0;
    short offs[16];
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (int i = 0; i < n; i++)
        if (lens[i]) h->symbol[offs[lens[i]]++] = (short)i;
}

static int huff_decode(BitR *b, const Huff *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        int c = (int)br_get(b, 1);
        if (b->pos > b->len && b->cnt <= 0) return -1;
        code |= c;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static const short len_base[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const short len_ext[29]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const short dst_base[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const short dst_ext[30]  = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int inflate_block(BitR *b, uint8_t *out, size_t outlen, size_t *outpos, const Huff *lit, const Huff *dist) {
    for (;;) {
        int sym = huff_decode(b, lit);
        if (sym < 0) return 0;
        if (sym < 256) {
            if (*outpos >= outlen) return 0;
            out[(*outpos)++] = (uint8_t)sym;
        } else if (sym == 256) {
            return 1;
        } else {
            sym -= 257;
            if (sym >= 29) return 0;
            int len = len_base[sym] + (int)br_get(b, len_ext[sym]);
            int dsym = huff_decode(b, dist);
            if (dsym < 0 || dsym >= 30) return 0;
            int d = dst_base[dsym] + (int)br_get(b, dst_ext[dsym]);
            if ((size_t)d > *outpos) return 0;
            if (*outpos + len > outlen) len = (int)(outlen - *outpos);
            uint8_t *src = out + *outpos - d;
            for (int i = 0; i < len; i++) out[(*outpos)++] = src[i];
        }
    }
}

// returns 1 on success
int zlib_inflate(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen, size_t *outused) {
    if (inlen < 2) return 0;
    // zlib header
    int cmf = in[0], flg = in[1];
    if ((cmf & 0x0F) != 8) return 0;
    if ((cmf * 256 + flg) % 31 != 0) return 0;
    size_t pos = 2;
    if (flg & 0x20) pos += 4;  // preset dictionary (unsupported but skip)
    BitR b = {in + pos, inlen - pos, 0, 0, 0};
    size_t outp = 0;
    int final = 0;
    while (!final) {
        final = (int)br_get(&b, 1);
        int type = (int)br_get(&b, 2);
        if (type == 0) {
            br_align(&b);
            if (b.pos + 4 > b.len) return 0;
            uint32_t len = (uint32_t)b.p[b.pos] | ((uint32_t)b.p[b.pos + 1] << 8);
            b.pos += 4;
            if (b.pos + len > b.len) return 0;
            if (outp + len > outlen) len = (uint32_t)(outlen - outp);
            memcpy(out + outp, b.p + b.pos, len);
            outp += len;
            b.pos += len;
        } else if (type == 1) {
            static Huff lit, dist;
            static int init = 0;
            if (!init) {
                short lens[288];
                int i = 0;
                for (; i < 144; i++) lens[i] = 8;
                for (; i < 256; i++) lens[i] = 9;
                for (; i < 280; i++) lens[i] = 7;
                for (; i < 288; i++) lens[i] = 8;
                huff_build(&lit, lens, 288);
                for (i = 0; i < 30; i++) lens[i] = 5;
                huff_build(&dist, lens, 30);
                init = 1;
            }
            if (!inflate_block(&b, out, outlen, &outp, &lit, &dist)) return 0;
        } else if (type == 2) {
            int hlit = (int)br_get(&b, 5) + 257;
            int hdist = (int)br_get(&b, 5) + 1;
            int hclen = (int)br_get(&b, 4) + 4;
            static const short order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            short clens[19] = {0};
            for (int i = 0; i < hclen; i++) clens[order[i]] = (short)br_get(&b, 3);
            Huff chuff;
            huff_build(&chuff, clens, 19);
            short lens[288 + 32];
            int n = 0;
            while (n < hlit + hdist) {
                int sym = huff_decode(&b, &chuff);
                if (sym < 0) return 0;
                if (sym < 16) {
                    lens[n++] = (short)sym;
                } else if (sym == 16) {
                    if (n == 0) return 0;
                    short prev = lens[n - 1];
                    int rep = 3 + (int)br_get(&b, 2);
                    while (rep-- && n < hlit + hdist) lens[n++] = prev;
                } else if (sym == 17) {
                    int rep = 3 + (int)br_get(&b, 3);
                    while (rep-- && n < hlit + hdist) lens[n++] = 0;
                } else {
                    int rep = 11 + (int)br_get(&b, 7);
                    while (rep-- && n < hlit + hdist) lens[n++] = 0;
                }
            }
            Huff lit, dist;
            huff_build(&lit, lens, hlit);
            huff_build(&dist, lens + hlit, hdist);
            if (!inflate_block(&b, out, outlen, &outp, &lit, &dist)) return 0;
        } else {
            return 0;
        }
    }
    if (outused) *outused = outp;
    return 1;
}
