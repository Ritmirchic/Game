// Minimal PNG writer (stored deflate) + BMP writer — no external deps
#include "core/png.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdint>

static uint32_t crcTable[256];
static bool crcInit = false;
static void initCRC() {
    if (crcInit) return;
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crcTable[n] = c;
    }
    crcInit = true;
}
static uint32_t crc32buf(const uint8_t* buf, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    initCRC();
    for (size_t i = 0; i < len; i++) crc = crcTable[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}
static void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF);
}
static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    put32(out, (uint32_t)data.size());
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t c = crc32buf(&out[start], 4 + data.size());
    put32(out, c ^ 0xFFFFFFFFu);
}

bool writePNG(const char* path, const uint32_t* pixels, int w, int h) {
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * (w * 4 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0); // filter none
        for (int x = 0; x < w; x++) {
            uint32_t p = pixels[(size_t)y * w + x];
            raw.push_back((p >> 24) & 0xFF);   // R
            raw.push_back((p >> 16) & 0xFF);   // G
            raw.push_back((p >> 8) & 0xFF);    // B
            raw.push_back(p & 0xFF);           // A
        }
    }
    // zlib stream with stored deflate blocks
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool final = (pos + n >= raw.size());
        z.push_back(final ? 1 : 0);
        z.push_back(n & 0xFF); z.push_back((n >> 8) & 0xFF);
        z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    put32(z, (b << 16) | a);

    std::vector<uint8_t> out;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);
    std::vector<uint8_t> ihdr;
    put32(ihdr, (uint32_t)w); put32(ihdr, (uint32_t)h);
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return true;
}

bool writeBMP(const char* path, const uint32_t* pixels, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int rowSize = ((w * 3) + 3) & ~3;
    uint32_t dataSize = rowSize * h;
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    uint32_t fileSize = 54 + dataSize;
    std::memcpy(hdr + 2, &fileSize, 4);
    uint32_t offset = 54;
    std::memcpy(hdr + 10, &offset, 4);
    uint32_t dibSize = 40;
    std::memcpy(hdr + 14, &dibSize, 4);
    int32_t ww = w, hh = h;
    std::memcpy(hdr + 18, &ww, 4);
    std::memcpy(hdr + 22, &hh, 4);
    uint16_t planes = 1, bpp = 24;
    std::memcpy(hdr + 26, &planes, 2);
    std::memcpy(hdr + 28, &bpp, 2);
    std::memcpy(hdr + 34, &dataSize, 4);
    fwrite(hdr, 1, 54, f);
    std::vector<uint8_t> row(rowSize, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint32_t p = pixels[(size_t)y * w + x];
            row[x * 3 + 0] = p & 0xFF;
            row[x * 3 + 1] = (p >> 8) & 0xFF;
            row[x * 3 + 2] = (p >> 16) & 0xFF;
        }
        fwrite(row.data(), 1, rowSize, f);
    }
    fclose(f);
    return true;
}
