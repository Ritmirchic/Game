// PNG writer (stored deflate blocks) for screenshots / diagnostics
#include "core.h"
#include <string.h>

static uint32_t crc_table[256];
static int crc_init = 0;
static void crc_build(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
    crc_init = 1;
}
static uint32_t crc32_buf(const uint8_t *buf, size_t len, uint32_t crc) {
    if (!crc_init) crc_build();
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
static uint32_t adler32_buf(const uint8_t *buf, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) { a = (a + buf[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}
static void wr32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t hdr[8];
    wr32(hdr, len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t crc = crc32_buf((const uint8_t *)type, 4, 0);
    crc = crc32_buf(data, len, crc);
    uint8_t cb[4];
    wr32(cb, crc);
    fwrite(cb, 1, 4, f);
}

int png_write_rgba(const char *path, const uint8_t *px, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    wr32(ihdr, (uint32_t)w);
    wr32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    // raw scanlines with filter byte 0
    size_t rawlen = (size_t)h * ((size_t)w * 4 + 1);
    uint8_t *raw = (uint8_t *)malloc(rawlen);
    for (int y = 0; y < h; y++) {
        uint8_t *row = raw + (size_t)y * (w * 4 + 1);
        row[0] = 0;
        memcpy(row + 1, px + (size_t)y * w * 4, (size_t)w * 4);
    }
    // zlib stream of stored blocks
    size_t maxz = rawlen + rawlen / 65535 * 5 + 64;
    uint8_t *z = (uint8_t *)malloc(maxz);
    size_t zp = 0;
    z[zp++] = 0x78; z[zp++] = 0x01;
    size_t off = 0;
    while (off < rawlen) {
        size_t n = rawlen - off;
        if (n > 65535) n = 65535;
        int final = (off + n >= rawlen);
        z[zp++] = final ? 1 : 0;
        z[zp++] = (uint8_t)(n & 0xFF);
        z[zp++] = (uint8_t)(n >> 8);
        z[zp++] = (uint8_t)(~n & 0xFF);
        z[zp++] = (uint8_t)((~n >> 8) & 0xFF);
        memcpy(z + zp, raw + off, n);
        zp += n;
        off += n;
    }
    uint32_t ad = adler32_buf(raw, rawlen);
    z[zp++] = (uint8_t)(ad >> 24); z[zp++] = (uint8_t)(ad >> 16);
    z[zp++] = (uint8_t)(ad >> 8); z[zp++] = (uint8_t)ad;
    chunk(f, "IDAT", z, (uint32_t)zp);
    chunk(f, "IEND", NULL, 0);
    free(raw);
    free(z);
    fclose(f);
    return 1;
}
