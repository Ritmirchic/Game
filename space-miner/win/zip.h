/* =========================================================================
   SPACE MINER — portable ZIP extraction (stored + deflate entries)
   Host must provide:
       void zip_mkdir(const char *path);
       int  zip_write(const char *path, const unsigned char *data, unsigned int len);
   ========================================================================= */
#ifndef SPACE_MINER_ZIP_H
#define SPACE_MINER_ZIP_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* path separator used when joining the destination folder with entry names */
#ifndef ZIP_SEP
#define ZIP_SEP '\\'
#endif

/* provided by the host translation unit */
void zip_mkdir(const char *path);
int  zip_write(const char *path, const unsigned char *data, unsigned int len);

static unsigned int zrd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}
static unsigned int zrd16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int zip_crc32(const unsigned char *data, unsigned int len)
{
    unsigned int c = 0xffffffffu;
    int k, i;
    for (i = 0; i < (int)len; i++) {
        c ^= data[i];
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (~(c & 1) + 1));
    }
    return c ^ 0xffffffffu;
}

/* ---------------------------------------------------------------------
   Minimal raw DEFLATE decoder (RFC 1951). The installer payload is stored
   uncompressed, but accepting deflate lets the same code read ordinary
   archives written by external tools.
   --------------------------------------------------------------------- */
#define ZW_MAXBITS 15

typedef struct {
    const unsigned char *src;
    unsigned int srclen, srcpos;
    unsigned int bitbuf, bitcnt;
    unsigned char *out;
    unsigned int outlen, outcap;
    int err;
} zinflatestate;

static int zi_bits(zinflatestate *s, int need)
{
    unsigned int val = s->bitbuf;
    while (s->bitcnt < (unsigned int)need) {
        if (s->srcpos >= s->srclen) { s->err = 1; return 0; }
        val |= ((unsigned int)s->src[s->srcpos++]) << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = val >> need;
    s->bitcnt -= (unsigned int)need;
    return (int)(val & ((1u << need) - 1));
}

typedef struct {
    short counts[ZW_MAXBITS + 1];
    short symbols[288];
} zhuff;

static int zi_decode(zinflatestate *s, const zhuff *h)
{
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len <= ZW_MAXBITS; len++) {
        int count;
        code |= zi_bits(s, 1);
        if (s->err) return -1;
        count = h->counts[len];
        if (code - count < first) return h->symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static int zi_construct(zhuff *h, const short *length, int n)
{
    short offs[ZW_MAXBITS + 1];
    int symbol, len, left;
    for (len = 0; len <= ZW_MAXBITS; len++) h->counts[len] = 0;
    for (symbol = 0; symbol < n; symbol++) h->counts[length[symbol]]++;
    if (h->counts[0] == n) return 0;
    left = 1;
    for (len = 1; len <= ZW_MAXBITS; len++) {
        left <<= 1;
        left -= h->counts[len];
        if (left < 0) return left;          /* over-subscribed */
    }
    offs[1] = 0;
    for (len = 1; len < ZW_MAXBITS; len++) offs[len + 1] = (short)(offs[len] + h->counts[len]);
    for (symbol = 0; symbol < n; symbol++) {
        if (length[symbol] != 0) h->symbols[offs[length[symbol]]++] = (short)symbol;
    }
    return left;
}

static int zi_put(zinflatestate *s, int byte)
{
    if (s->outlen >= s->outcap) {
        unsigned int ncap = s->outcap ? s->outcap * 2 : 65536;
        unsigned char *n;
        if (ncap > 64u * 1024u * 1024u) { s->err = 1; return 0; }
        n = (unsigned char *)realloc(s->out, ncap);
        if (!n) { s->err = 1; return 0; }
        s->out = n;
        s->outcap = ncap;
    }
    s->out[s->outlen++] = (unsigned char)byte;
    return 1;
}

static int zi_codes(zinflatestate *s, const zhuff *lencode, const zhuff *distcode)
{
    static const short lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    static const short dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
    static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
    for (;;) {
        int symbol = zi_decode(s, lencode);
        if (symbol < 0) return 0;
        if (symbol < 256) {
            if (!zi_put(s, symbol)) return 0;
        } else if (symbol == 256) {
            return 1;
        } else {
            int len, dist, i;
            symbol -= 257;
            if (symbol >= 29) return 0;
            len = lbase[symbol] + zi_bits(s, lext[symbol]);
            if (s->err) return 0;
            symbol = zi_decode(s, distcode);
            if (symbol < 0 || symbol >= 30) return 0;
            dist = dbase[symbol] + zi_bits(s, dext[symbol]);
            if (s->err) return 0;
            if ((unsigned int)dist > s->outlen) return 0;
            for (i = 0; i < len; i++) {
                if (!zi_put(s, s->out[s->outlen - (unsigned int)dist])) return 0;
            }
        }
    }
}

static int zi_uncompressed(zinflatestate *s)
{
    unsigned int len, nlen;
    if (s->bitcnt >= 8) { s->srcpos -= s->bitcnt / 8; s->bitbuf = 0; s->bitcnt = 0; }
    if (s->srcpos + 4 > s->srclen) return 0;
    len = zrd16(s->src + s->srcpos);
    nlen = zrd16(s->src + s->srcpos + 2);
    s->srcpos += 4;
    if (len != (~nlen & 0xffffu)) return 0;
    if (s->srcpos + len > s->srclen) return 0;
    while (len--) {
        if (!zi_put(s, s->src[s->srcpos++])) return 0;
    }
    return 1;
}

static int zi_fixed(zinflatestate *s)
{
    static zhuff lencode, distcode;
    static int built = 0;
    if (!built) {
        short lengths[288];
        int i;
        for (i = 0; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < 288; i++) lengths[i] = 8;
        zi_construct(&lencode, lengths, 288);
        for (i = 0; i < 30; i++) lengths[i] = 5;
        zi_construct(&distcode, lengths, 30);
        built = 1;
    }
    return zi_codes(s, &lencode, &distcode);
}

static int zi_dynamic(zinflatestate *s)
{
    static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    int nlen, ndist, ncode, index, err = 0;
    short lengths[320];
    zhuff lencode, distcode;

    nlen = zi_bits(s, 5) + 257;
    ndist = zi_bits(s, 5) + 1;
    ncode = zi_bits(s, 4) + 4;
    if (s->err) return 0;
    if (nlen > 286 || ndist > 30) return 0;
    for (index = 0; index < ncode; index++) lengths[order[index]] = (short)zi_bits(s, 3);
    for (; index < 19; index++) lengths[order[index]] = 0;
    if (s->err) return 0;
    zi_construct(&lencode, lengths, 19);
    index = 0;
    while (index < nlen + ndist) {
        int symbol = zi_decode(s, &lencode);
        if (symbol < 0) return 0;
        if (symbol < 16) {
            lengths[index++] = (short)symbol;
        } else {
            int len = 0, rep;
            if (symbol == 16) {
                if (index == 0) return 0;
                len = lengths[index - 1];
                rep = 3 + zi_bits(s, 2);
            } else if (symbol == 17) {
                rep = 3 + zi_bits(s, 3);
            } else {
                rep = 11 + zi_bits(s, 7);
            }
            if (s->err) return 0;
            if (index + rep > nlen + ndist) return 0;
            while (rep--) lengths[index++] = (short)len;
        }
    }
    if (lengths[256] == 0) return 0;
    err = zi_construct(&lencode, lengths, nlen);
    if (!err && (lencode.counts[0] != nlen)) err = zi_construct(&distcode, lengths + nlen, ndist);
    if (err) return 0;
    return zi_codes(s, &lencode, &distcode);
}

/* returns 1 on success; *out receives a malloc'd buffer the caller frees */
static int zlib_inflate_raw(const unsigned char *src, unsigned int srclen,
                            unsigned char **out, unsigned int *outlen)
{
    zinflatestate s;
    int last, err = 1;
    memset(&s, 0, sizeof(s));
    s.src = src;
    s.srclen = srclen;
    do {
        int type;
        last = zi_bits(&s, 1);
        type = zi_bits(&s, 2);
        if (s.err) { err = 0; break; }
        if (type == 0) err = zi_uncompressed(&s);
        else if (type == 1) err = zi_fixed(&s);
        else if (type == 2) err = zi_dynamic(&s);
        else err = 0;
        if (!err) break;
    } while (!last);
    if (!err) { free(s.out); return 0; }
    *out = s.out;
    *outlen = s.outlen;
    return 1;
}

/* ---------------------------------------------------------------------
   Extract every file of a ZIP archive into `dest` (creating folders).
   --------------------------------------------------------------------- */
static int zip_extract(const unsigned char *z, unsigned int zsize, const char *dest,
                       void (*progress)(int percent, void *ctx), void *ctx)
{
    long eocd = -1, low, i;
    unsigned int count, cdOff, e;
    int written = 0;

    if (zsize < 24) return 0;
    low = (long)zsize - 22 - 65535;
    if (low < 0) low = 0;
    for (i = (long)zsize - 22; i >= low; i--) {
        if (zrd32(z + i) == 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) return 0;
    count = zrd16(z + eocd + 10);
    cdOff = zrd32(z + eocd + 16);
    if ((unsigned int)eocd < cdOff) return 0;

    e = cdOff;
    for (i = 0; i < (long)count; i++) {
        unsigned int method, compSize, uncompSize, nameLen, extraLen, commentLen, localOff;
        unsigned int wantCrc;
        char name[512];
        char full[1024];
        char *p;
        if (e + 46 > zsize) return written;
        if (zrd32(z + e) != 0x02014b50) return written;
        method = zrd16(z + e + 10);
        wantCrc = zrd32(z + e + 16);
        compSize = zrd32(z + e + 20);
        uncompSize = zrd32(z + e + 24);
        nameLen = zrd16(z + e + 28);
        extraLen = zrd16(z + e + 30);
        commentLen = zrd16(z + e + 32);
        localOff = zrd32(z + e + 42);
        if (nameLen >= sizeof(name) || e + 46 + nameLen > zsize) return written;
        memcpy(name, z + e + 46, nameLen);
        name[nameLen] = 0;
        for (p = name; *p; p++) if (*p == '/') *p = ZIP_SEP;

        if (strstr(name, "..")) return written;      /* no traversal */

        if (nameLen && name[nameLen - 1] != ZIP_SEP) {
            unsigned int dataOff, nameLenL, extraLenL;
            unsigned char *data = NULL;
            unsigned int dataLen = 0;
            if (localOff + 30 > zsize) return written;
            if (zrd32(z + localOff) != 0x04034b50) return written;
            nameLenL = zrd16(z + localOff + 26);
            extraLenL = zrd16(z + localOff + 28);
            dataOff = localOff + 30 + nameLenL + extraLenL;
            if (dataOff + compSize > zsize) return written;

            if (method == 0) {
                data = (unsigned char *)(z + dataOff);
                dataLen = compSize;
            } else if (method == 8) {
                if (!zlib_inflate_raw(z + dataOff, compSize, &data, &dataLen)) return written;
                if (dataLen != uncompSize) { free(data); return written; }
            } else {
                return written;                          /* unsupported method */
            }

            char *slash;
            if (zip_crc32(data, dataLen) != wantCrc) {
                if (method == 8) free(data);
                return written;                 /* corrupted entry */
            }
            snprintf(full, sizeof(full), "%s%c%s", dest, ZIP_SEP, name);
            full[sizeof(full) - 1] = 0;
            slash = strrchr(full, ZIP_SEP);
            if (slash) { *slash = 0; zip_mkdir(full); *slash = ZIP_SEP; }
            if (!zip_write(full, data, dataLen)) {
                if (method == 8) free(data);
                return written;
            }
            if (method == 8) free(data);
            written++;
        } else {
            snprintf(full, sizeof(full), "%s%c%s", dest, ZIP_SEP, name);
            full[sizeof(full) - 1] = 0;
            zip_mkdir(full);
        }
        e += 46 + nameLen + extraLen + commentLen;
        if (progress && count && (i % 2 == 0)) progress((int)((i + 1) * 96 / (long)count), ctx);
    }
    if (progress) progress(100, ctx);
    return written;
}

#endif /* SPACE_MINER_ZIP_H */
