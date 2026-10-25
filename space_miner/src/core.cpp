// ============================================================
// SPACE MINER  -  core.cpp
// ============================================================
#include "core.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#ifndef EXCLUDE_FROM_GAME
#include <shellapi.h>
#endif
#endif

namespace sm {

// ---------------------------------------------------------------- colour ops
Color shade(Color c, float f) {
    return rgba((int)(cr(c) * f), (int)(cg(c) * f), (int)(cb(c) * f), ca(c));
}
Color addColor(Color a, Color b) {
    return rgba(cr(a) + cr(b), cg(a) + cg(b), cb(a) + cb(b), 255);
}

// ---------------------------------------------------------------- hashing
static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static inline float hashToFloat(uint32_t h) { return (h >> 8) * (1.0f / 16777216.0f); }

// ---------------------------------------------------------------- value noise
float valueNoise2(float x, float y, uint32_t seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float xf = x - xi, yf = y - yi;
    float u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    auto H = [&](int a, int b) {
        // all arithmetic in unsigned space: signed overflow here would be UB
        uint32_t ua = (uint32_t)a * 374761393u;
        uint32_t ub = (uint32_t)b * 668265263u;
        return hashToFloat(hash32(ua + ub + seed * 971u));
    };
    float a = H(xi, yi), b = H(xi + 1, yi), c = H(xi, yi + 1), d = H(xi + 1, yi + 1);
    return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}

float fbm2(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed) {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += valueNoise2(x, y, seed + i * 131u) * amp;
        norm += amp;
        amp *= gain;
        x *= lacunarity; y *= lacunarity;
        x += 17.3f; y += 9.1f;
    }
    return norm > 0 ? sum / norm : 0.f;
}

float worley2(float x, float y, uint32_t seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float best = 1e9f;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            int cx = xi + i, cy = yi + j;
            uint32_t uc = (uint32_t)cx, vc = (uint32_t)cy;
            float px = cx + hashToFloat(hash32(uc * 555557u ^ vc * 123457u ^ seed)),
                  py = cy + hashToFloat(hash32(vc * 777767u ^ uc * 918733u ^ (seed + 7u)));
            float dx = px - x, dy = py - y;
            float d = dx * dx + dy * dy;
            if (d < best) best = d;
        }
    return sqrtf(best);
}

float ridged2(float x, float y, int octaves, uint32_t seed) {
    float sum = 0, amp = .5f, freq = 1.f;
    for (int i = 0; i < octaves; i++) {
        float n = valueNoise2(x * freq, y * freq, seed + i * 71u);
        n = 1.f - fabsf(n * 2 - 1);
        sum += n * n * amp;
        freq *= 2.03f; amp *= .5f;
    }
    return sum;
}

// ---------------------------------------------------------------- file io
bool writeFile(const std::string& path, const void* data, size_t size) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t w = fwrite(data, 1, size, f);
    fclose(f);
    return w == size;
}
bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return false; }
    out.resize((size_t)sz);
    size_t r = sz > 0 ? fread(out.data(), 1, (size_t)sz, f) : 0;
    fclose(f);
    return r == (size_t)sz;
}
bool fileExists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// ---------------------------------------------------------------- adler / crc
static uint32_t adler32(const uint8_t* d, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) { a = (a + d[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}
static uint32_t crcTable[256];
static bool crcInit = false;
static void initCRC() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crcTable[n] = c;
    }
    crcInit = true;
}
static uint32_t crc32buf(const uint8_t* d, size_t n) {
    if (!crcInit) initCRC();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crcTable[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));  v.push_back((uint8_t)x);
}
static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    put32(out, (uint32_t)data.size());
    std::vector<uint8_t> td;
    td.reserve(4 + data.size());
    for (int i = 0; i < 4; i++) td.push_back((uint8_t)type[i]);
    td.insert(td.end(), data.begin(), data.end());
    out.insert(out.end(), td.begin(), td.end());
    put32(out, crc32buf(td.data(), td.size()));
}

// PNG writer, zlib stream using stored (uncompressed) deflate blocks.
bool writePNG(const char* path, const uint32_t* px, int w, int h) {
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(w * 3 + 1) * h);
    for (int y = 0; y < h; y++) {
        raw.push_back(0);  // filter none
        for (int x = 0; x < w; x++) {
            uint32_t c = px[y * w + x];
            raw.push_back((uint8_t)((c >> 16) & 255));
            raw.push_back((uint8_t)((c >> 8) & 255));
            raw.push_back((uint8_t)(c & 255));
        }
    }
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool final = (pos + n >= raw.size());
        z.push_back(final ? 1 : 0);
        z.push_back((uint8_t)(n & 255)); z.push_back((uint8_t)(n >> 8));
        z.push_back((uint8_t)(~n & 255)); z.push_back((uint8_t)((~n >> 8) & 255));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    put32(z, adler32(raw.data(), raw.size()));

    std::vector<uint8_t> png;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    png.insert(png.end(), sig, sig + 8);
    std::vector<uint8_t> ihdr;
    put32(ihdr, (uint32_t)w); put32(ihdr, (uint32_t)h);
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    return writeFile(path, png.data(), png.size());
}

bool savePNG(const std::string& path, const uint32_t* argb, int w, int h) {
    return writePNG(path.c_str(), argb, w, h);
}

// ---------------------------------------------------------------- strings
std::string fmt(const char* f, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return std::string(buf);
}
std::string intToStr(long long v) { return fmt("%lld", v); }
std::string money(long long v) {
    bool neg = v < 0;
    if (neg) v = -v;
    std::string s = fmt("%lld", v);
    std::string out;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) {
        out.push_back(s[i]);
        if (++c % 3 == 0 && i > 0) out.push_back('\'');
    }
    if (neg) out.push_back('-');
    std::reverse(out.begin(), out.end());
    return out;
}
std::string padLeft(const std::string& s, int n, char c) {
    if ((int)s.size() >= n) return s;
    return std::string(n - s.size(), c) + s;
}
std::string upper(const std::string& s) {
    std::string o = s;
    for (auto& c : o) c = (char)toupper((unsigned char)c);
    return o;
}
std::string float1(float v) { return fmt("%.1f", v); }

}  // namespace sm
