// PNG decoder (8-bit, non-interlaced, colour types 0/2/4/6) -> RGBA8
#include "core.h"
#include <string.h>

int zlib_inflate(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen, size_t *outused);
uint8_t *file_read_all(const char *path, size_t *size);

static uint32_t rd32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// Decode PNG from memory. Returns RGBA8 buffer (malloc) or NULL.
uint8_t *png_decode(const uint8_t *data, size_t len, int *ow, int *oh) {
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (len < 8 || memcmp(data, sig, 8) != 0) return NULL;
    size_t pos = 8;
    int w = 0, h = 0, bitdepth = 0, ctype = 0, interlace = 0;
    uint8_t *idat = NULL;
    size_t idat_len = 0, idat_cap = 0;
    while (pos + 8 <= len) {
        uint32_t clen = rd32be(data + pos);
        const uint8_t *type = data + pos + 4;
        const uint8_t *cd = data + pos + 8;
        if (pos + 12 + clen > len) break;
        if (!memcmp(type, "IHDR", 4)) {
            w = (int)rd32be(cd);
            h = (int)rd32be(cd + 4);
            bitdepth = cd[8];
            ctype = cd[9];
            interlace = cd[12];
        } else if (!memcmp(type, "IDAT", 4)) {
            if (idat_len + clen > idat_cap) {
                idat_cap = (idat_len + clen) * 2 + 1024;
                idat = (uint8_t *)realloc(idat, idat_cap);
            }
            memcpy(idat + idat_len, cd, clen);
            idat_len += clen;
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + clen;
    }
    if (!idat || w <= 0 || h <= 0 || bitdepth != 8 || interlace != 0) {
        free(idat);
        return NULL;
    }
    int channels = ctype == 0 ? 1 : ctype == 2 ? 3 : ctype == 4 ? 2 : ctype == 6 ? 4 : 0;
    if (!channels) { free(idat); return NULL; }
    size_t raw_len = (size_t)h * ((size_t)w * channels + 1);
    uint8_t *raw = (uint8_t *)malloc(raw_len + 64);
    size_t got = 0;
    if (!zlib_inflate(idat, idat_len, raw, raw_len, &got)) { free(idat); free(raw); return NULL; }
    free(idat);
    if (got < raw_len) { /* tolerate truncated tail */ }
    uint8_t *out = (uint8_t *)malloc((size_t)w * h * 4);
    size_t bpp = (size_t)channels;
    size_t stride = (size_t)w * bpp;
    uint8_t *prev = (uint8_t *)calloc(stride, 1);
    uint8_t *cur = (uint8_t *)malloc(stride);
    for (int y = 0; y < h; y++) {
        size_t rowoff = (size_t)y * (stride + 1);
        if (rowoff + 1 + stride > raw_len + 1) { /* fill rest black */
            memset(cur, 0, stride);
        } else {
            int filt = raw[rowoff];
            memcpy(cur, raw + rowoff + 1, stride);
            for (size_t x = 0; x < stride; x++) {
                int a = x >= bpp ? cur[x - bpp] : 0;
                int b = prev[x];
                int c = x >= bpp ? prev[x - bpp] : 0;
                int v = cur[x];
                switch (filt) {
                    case 1: v += a; break;
                    case 2: v += b; break;
                    case 3: v += (a + b) >> 1; break;
                    case 4: v += paeth(a, b, c); break;
                    default: break;
                }
                cur[x] = (uint8_t)v;
            }
        }
        uint8_t *orow = out + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            uint8_t *s = cur + (size_t)x * bpp;
            uint8_t *d = orow + (size_t)x * 4;
            if (channels == 1) { d[0] = d[1] = d[2] = s[0]; d[3] = 255; }
            else if (channels == 2) { d[0] = d[1] = d[2] = s[0]; d[3] = s[1]; }
            else if (channels == 3) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; }
            else { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; }
        }
        uint8_t *tmp = prev; prev = cur; cur = tmp;
    }
    free(prev);
    free(cur);
    free(raw);
    *ow = w;
    *oh = h;
    return out;
}

uint8_t *png_load_file(const char *path, int *w, int *h) {
    size_t sz = 0;
    uint8_t *buf = file_read_all(path, &sz);
    if (!buf) { LOGE("png: cannot open %s", path); return NULL; }
    uint8_t *res = png_decode(buf, sz, w, h);
    free(buf);
    if (!res) LOGE("png: decode failed %s", path);
    return res;
}
