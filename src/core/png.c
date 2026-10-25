/* ============================================================================
   SPACE MINER — core/png.c
   Minimal PNG reader (8-bit, non-interlaced, gray/RGB/RGBA/palette) and writer.
   ========================================================================== */
#include "common.h"
#include "png.h"
#include "inflate.h"

static u32 be32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static void put_be32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v;
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

void image_free(Image *img) {
    if (!img) return;
    if (img->owns_pixels) sm_free(img->px);
    img->px = NULL; img->w = img->h = 0;
}

int png_decode(const void *data, usize len, Image *out) {
    const u8 *p = (const u8 *)data;
    if (len < 8 || p[0] != 0x89 || p[1] != 'P' || p[2] != 'N' || p[3] != 'G') return -1;
    usize pos = 8;
    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    u8 pal[256 * 3];
    int palCount = 0;
    u8 *idat = NULL;
    usize idatLen = 0, idatCap = 0;
    int haveHeader = 0;

    while (pos + 8 <= len) {
        u32 clen = be32(p + pos);
        const u8 *ctype4 = p + pos + 4;
        usize dpos = pos + 8;
        if (dpos + clen > len) break;
        if (!memcmp(ctype4, "IHDR", 4)) {
            if (clen < 13) return -1;
            w = (int)be32(p + dpos);
            h = (int)be32(p + dpos + 4);
            depth = p[dpos + 8];
            ctype = p[dpos + 9];
            interlace = p[dpos + 12];
            if (depth != 8 || interlace != 0) return -1;
            if (ctype != 0 && ctype != 2 && ctype != 3 && ctype != 6) return -1;
            if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return -1;
            haveHeader = 1;
        } else if (!memcmp(ctype4, "PLTE", 4)) {
            palCount = (int)(clen / 3);
            if (palCount > 256) palCount = 256;
            memcpy(pal, p + dpos, (usize)palCount * 3);
        } else if (!memcmp(ctype4, "IDAT", 4)) {
            if (idatLen + clen > idatCap) {
                usize ncap = idatCap ? idatCap * 2 : 65536;
                while (ncap < idatLen + clen) ncap *= 2;
                u8 *nb = (u8 *)malloc(ncap);
                if (!nb) { free(idat); return -1; }
                if (idat) { memcpy(nb, idat, idatLen); free(idat); }
                idat = nb; idatCap = ncap;
            }
            memcpy(idat + idatLen, p + dpos, clen);
            idatLen += clen;
        } else if (!memcmp(ctype4, "IEND", 4)) {
            break;
        }
        pos = dpos + clen + 4;
    }

    if (!haveHeader || !idat) { free(idat); return -1; }

    int channels = (ctype == 0) ? 1 : (ctype == 2) ? 3 : (ctype == 3) ? 1 : 4;
    usize stride = (usize)w * (usize)channels;
    usize rawLen = (stride + 1) * (usize)h;
    u8 *raw = (u8 *)malloc(rawLen);
    if (!raw) { free(idat); return -1; }
    usize got = 0;
    if (sm_inflate(raw, rawLen, &got, idat, idatLen) != 0 || got < rawLen) {
        free(raw); free(idat); return -1;
    }
    free(idat);

    u32 *px = (u32 *)sm_alloc((usize)w * (usize)h * 4);
    if (!px) { free(raw); return -1; }
    u8 *prev = NULL;
    for (int y = 0; y < h; y++) {
        u8 *row = raw + (stride + 1) * (usize)y;
        int filter = row[0];
        u8 *cur = row + 1;
        int bpp = channels;
        for (usize x = 0; x < stride; x++) {
            int a = (x >= (usize)bpp) ? cur[x - bpp] : 0;
            int b = prev ? prev[x] : 0;
            int c = (prev && x >= (usize)bpp) ? prev[x - bpp] : 0;
            int v = cur[x];
            switch (filter) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) >> 1; break;
                case 4: v += paeth(a, b, c); break;
                default: free(raw); sm_free(px); return -1;
            }
            cur[x] = (u8)v;
        }
        for (int x = 0; x < w; x++) {
            u8 r, g, bl, al = 255;
            if (ctype == 6) { r = cur[x * 4]; g = cur[x * 4 + 1]; bl = cur[x * 4 + 2]; al = cur[x * 4 + 3]; }
            else if (ctype == 2) { r = cur[x * 3]; g = cur[x * 3 + 1]; bl = cur[x * 3 + 2]; }
            else if (ctype == 0) { r = g = bl = cur[x]; }
            else {
                int idx = cur[x];
                if (idx < palCount) { r = pal[idx * 3]; g = pal[idx * 3 + 1]; bl = pal[idx * 3 + 2]; }
                else { r = g = bl = (u8)idx; }
            }
            px[y * w + x] = ((u32)al << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)bl;
        }
        prev = cur;
    }
    free(raw);
    out->w = w; out->h = h; out->px = px; out->owns_pixels = 1;
    return 0;
}

/* ------------------------------------------------------------- encoder ---- */
static u8 *chunk_write(u8 *dst, const char *type, const u8 *data, u32 len) {
    put_be32(dst, len);
    memcpy(dst + 4, type, 4);
    if (len && data) memcpy(dst + 8, data, len);
    u32 crc = sm_crc32(dst + 4, len + 4, 0);
    put_be32(dst + 8 + len, crc);
    return dst + 12 + len;
}

u8 *png_encode(const u32 *px, int w, int h, usize *outLen) {
    usize stride = (usize)w * 4u;
    usize rawLen = (stride + 1) * (usize)h;
    u8 *raw = (u8 *)malloc(rawLen);
    if (!raw) return NULL;
    for (int y = 0; y < h; y++) {
        u8 *row = raw + (stride + 1) * (usize)y;
        row[0] = 1; /* Sub filter improves ratio even with stored blocks */
        const u32 *src = px + (usize)y * (usize)w;
        u8 *cur = row + 1;
        for (int x = 0; x < w; x++) {
            u32 c = src[x];
            u8 r = (u8)(c >> 16), g = (u8)(c >> 8), b = (u8)c, a = (u8)(c >> 24);
            u8 pr = x > 0 ? (u8)(src[x - 1] >> 16) : 0;
            u8 pg = x > 0 ? (u8)(src[x - 1] >> 8) : 0;
            u8 pb = x > 0 ? (u8)src[x - 1] : 0;
            u8 pa = x > 0 ? (u8)(src[x - 1] >> 24) : 0;
            cur[x * 4 + 0] = (u8)(r - pr);
            cur[x * 4 + 1] = (u8)(g - pg);
            cur[x * 4 + 2] = (u8)(b - pb);
            cur[x * 4 + 3] = (u8)(a - pa);
        }
    }
    usize zcap = rawLen + rawLen / 65535 * 5 + 64;
    u8 *z = (u8 *)malloc(zcap);
    usize zlen = 0;
    if (!z || sm_deflate_stored(z, zcap, &zlen, raw, rawLen) != 0) { free(z); free(raw); return NULL; }
    free(raw);

    usize total = 8 + 25 + 12 + zlen + 12;
    u8 *out = (u8 *)malloc(total);
    if (!out) { free(z); return NULL; }
    static const u8 sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    memcpy(out, sig, 8);
    u8 *o = out + 8;
    u8 ihdr[13];
    put_be32(ihdr, (u32)w);
    put_be32(ihdr + 4, (u32)h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    o = chunk_write(o, "IHDR", ihdr, 13);
    o = chunk_write(o, "IDAT", z, (u32)zlen);
    o = chunk_write(o, "IEND", NULL, 0);
    free(z);
    if (outLen) *outLen = (usize)(o - out);
    return out;
}
