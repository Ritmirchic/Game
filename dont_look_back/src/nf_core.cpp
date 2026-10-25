// ============================================================================
//  nf_core.cpp : implementation of core utilities (image, PNG, noise, IO)
// ============================================================================
#include "nf_core.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#endif
#include <chrono>
#include <thread>

namespace nf {

// ------------------------------------------------------------------ images
Image Image::mipHalf() const {
    Image o(maxi(1, w / 2), maxi(1, h / 2));
    for (int y = 0; y < o.h; ++y) {
        for (int x = 0; x < o.w; ++x) {
            int r = 0, g = 0, b = 0, a = 0;
            for (int j = 0; j < 2; ++j) for (int i = 0; i < 2; ++i) {
                Color c = get(x * 2 + i, y * 2 + j);
                int ca = colA(c);
                r += colR(c) * ca; g += colG(c) * ca; b += colB(c) * ca; a += ca;
            }
            if (a == 0) o.set(x, y, 0);
            else o.set(x, y, rgba(r / a, g / a, b / a, a / 4));
        }
    }
    return o;
}

void Image::genMips(std::vector<Image>& out, int levels) const {
    out.clear();
    out.push_back(*this);
    for (int i = 1; i < levels; ++i) {
        const Image& prev = out.back();
        if (prev.w <= 2 || prev.h <= 2) break;
        out.push_back(prev.mipHalf());
    }
}

// ------------------------------------------------------------------ CRC / adler
static uint32_t crcTable[256];
static bool crcInit = false;
static void initCrc() {
    if (crcInit) return;
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crcTable[n] = c;
    }
    crcInit = true;
}
static uint32_t crc32up(uint32_t crc, const uint8_t* d, size_t n) {
    initCrc();
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = crcTable[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
static uint32_t adler32(const uint8_t* d, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) {
        a = (a + d[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// ------------------------------------------------------------------ deflate (fixed Huffman + LZ77)
struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t bitbuf = 0;
    int bitcnt = 0;
    explicit BitWriter(std::vector<uint8_t>& o) : out(o) {}
    void put(uint32_t v, int n) {           // LSB-first
        bitbuf |= (v & ((1u << n) - 1)) << bitcnt;
        bitcnt += n;
        while (bitcnt >= 8) { out.push_back((uint8_t)(bitbuf & 0xFF)); bitbuf >>= 8; bitcnt -= 8; }
    }
    void putRev(uint32_t code, int n) {     // Huffman codes: MSB first
        for (int i = n - 1; i >= 0; --i) put((code >> i) & 1, 1);
    }
    void flush() {
        if (bitcnt > 0) { out.push_back((uint8_t)(bitbuf & 0xFF)); bitbuf = 0; bitcnt = 0; }
    }
};

static void writeLitLen(BitWriter& bw, int sym) {
    if (sym <= 143) bw.putRev(0x30 + sym, 8);
    else if (sym <= 255) bw.putRev(0x190 + (sym - 144), 9);
    else if (sym <= 279) bw.putRev(sym - 256, 7);
    else bw.putRev(0xC0 + (sym - 280), 8);
}

static const int LEN_BASE[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const int LEN_EXTRA[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const int DIST_BASE[30]  = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const int DIST_EXTRA[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static void writeMatch(BitWriter& bw, int len, int dist) {
    int li = 28;
    while (li > 0 && LEN_BASE[li] > len) --li;
    writeLitLen(bw, 257 + li);
    if (LEN_EXTRA[li]) bw.put(len - LEN_BASE[li], LEN_EXTRA[li]);
    int di = 29;
    while (di > 0 && DIST_BASE[di] > dist) --di;
    bw.putRev(di, 5);
    if (DIST_EXTRA[di]) bw.put(dist - DIST_BASE[di], DIST_EXTRA[di]);
}

static void deflateFixed(const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    BitWriter bw(out);
    bw.put(1, 1);        // final block
    bw.put(1, 2);        // fixed Huffman
    const int WSIZE = 32768, HBITS = 15, HSIZE = 1 << HBITS, MAXCHAIN = 24;
    std::vector<int> head((size_t)HSIZE, -1), prev((size_t)WSIZE, -1);
    auto hash3 = [&](size_t p) -> int {
        return (int)(((in[p] << 16) ^ (in[p + 1] << 8) ^ in[p + 2]) & (HSIZE - 1));
    };
    size_t pos = 0;
    while (pos < n) {
        int bestLen = 0, bestDist = 0;
        if (pos + 2 < n) {
            int h = hash3(pos);
            int cand = head[h];
            int chain = MAXCHAIN;
            while (cand >= 0 && chain-- > 0) {
                size_t dist = pos - (size_t)cand;
                if (dist == 0 || dist > 32768) break;
                int l = 0;
                while (l < 258 && pos + l < n && in[cand + l] == in[pos + l]) ++l;
                if (l > bestLen) { bestLen = l; bestDist = (int)dist; if (l == 258) break; }
                cand = prev[cand & (WSIZE - 1)];
            }
            prev[pos & (WSIZE - 1)] = head[h];
            head[h] = (int)pos;
        }
        if (bestLen >= 3) {
            writeMatch(bw, bestLen, bestDist);
            // insert hashes for the covered bytes
            for (int k = 1; k < bestLen; ++k) {
                size_t p = pos + k;
                if (p + 2 < n) {
                    int h = hash3(p);
                    prev[p & (WSIZE - 1)] = head[h];
                    head[h] = (int)p;
                }
            }
            pos += bestLen;
        } else {
            writeLitLen(bw, in[pos]);
            ++pos;
        }
    }
    writeLitLen(bw, 256);
    bw.flush();
}

// ------------------------------------------------------------------ inflate
struct BitReader {
    const uint8_t* d;
    size_t n, pos = 0;
    uint32_t bitbuf = 0;
    int bitcnt = 0;
    BitReader(const uint8_t* dd, size_t nn) : d(dd), n(nn) {}
    int bit() {
        if (bitcnt == 0) {
            bitbuf = (pos < n) ? d[pos++] : 0;
            bitcnt = 8;
        }
        int b = bitbuf & 1;
        bitbuf >>= 1;
        --bitcnt;
        return b;
    }
    uint32_t bits(int c) {
        uint32_t v = 0;
        for (int i = 0; i < c; ++i) v |= (uint32_t)bit() << i;
        return v;
    }
};

struct HuffTable {
    int counts[16];
    int symbols[288];
};

static void buildHuff(HuffTable& t, const uint8_t* lens, int n) {
    for (int i = 0; i < 16; ++i) t.counts[i] = 0;
    for (int i = 0; i < n; ++i) t.counts[lens[i]]++;
    t.counts[0] = 0;
    int offs[16];
    offs[0] = 0;
    for (int i = 1; i < 16; ++i) offs[i] = offs[i - 1] + t.counts[i - 1];
    for (int i = 0; i < n; ++i) if (lens[i]) t.symbols[offs[lens[i]]++] = i;
}

static int decodeSym(BitReader& br, const HuffTable& t) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
        code |= br.bit();
        int count = t.counts[len];
        if (code - first < count) return t.symbols[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

static bool inflateData(const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    BitReader br(in, n);
    while (true) {
        int last = br.bit();
        int type = (int)br.bits(2);
        if (type == 0) {
            br.bitcnt = 0; br.bitbuf = 0;
            if (br.pos + 4 > br.n) return false;
            uint32_t len = (uint32_t)in[br.pos] | ((uint32_t)in[br.pos + 1] << 8);
            br.pos += 4;
            for (uint32_t i = 0; i < len; ++i) out.push_back(br.pos < br.n ? in[br.pos++] : 0);
        } else {
            HuffTable lit, dist;
            if (type == 1) {
                uint8_t l[288];
                for (int i = 0; i < 144; ++i) l[i] = 8;
                for (int i = 144; i < 256; ++i) l[i] = 9;
                for (int i = 256; i < 280; ++i) l[i] = 7;
                for (int i = 280; i < 288; ++i) l[i] = 8;
                buildHuff(lit, l, 288);
                uint8_t dl[30];
                for (int i = 0; i < 30; ++i) dl[i] = 5;
                buildHuff(dist, dl, 30);
            } else if (type == 2) {
                int hlit = (int)br.bits(5) + 257, hdist = (int)br.bits(5) + 1, hclen = (int)br.bits(4) + 4;
                static const int order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                uint8_t cl[19];
                for (int i = 0; i < 19; ++i) cl[i] = 0;
                for (int i = 0; i < hclen; ++i) cl[order[i]] = (uint8_t)br.bits(3);
                HuffTable clt;
                buildHuff(clt, cl, 19);
                uint8_t lens[320];
                int i = 0;
                while (i < hlit + hdist) {
                    int sym = decodeSym(br, clt);
                    if (sym < 0) return false;
                    if (sym < 16) lens[i++] = (uint8_t)sym;
                    else if (sym == 16) {
                        if (i == 0) return false;
                        uint8_t prev = lens[i - 1];
                        int rep = 3 + (int)br.bits(2);
                        while (rep-- && i < hlit + hdist) lens[i++] = prev;
                    } else if (sym == 17) {
                        int rep = 3 + (int)br.bits(3);
                        while (rep-- && i < hlit + hdist) lens[i++] = 0;
                    } else {
                        int rep = 11 + (int)br.bits(7);
                        while (rep-- && i < hlit + hdist) lens[i++] = 0;
                    }
                }
                buildHuff(lit, lens, hlit);
                buildHuff(dist, lens + hlit, hdist);
            } else return false;
            while (true) {
                int sym = decodeSym(br, lit);
                if (sym < 0) return false;
                if (sym < 256) out.push_back((uint8_t)sym);
                else if (sym == 256) break;
                else {
                    int li = sym - 257;
                    if (li < 0 || li > 28) return false;
                    int len = LEN_BASE[li] + (int)br.bits(LEN_EXTRA[li]);
                    int ds = decodeSym(br, dist);
                    if (ds < 0 || ds > 29) return false;
                    int d = DIST_BASE[ds] + (int)br.bits(DIST_EXTRA[ds]);
                    if ((size_t)d > out.size()) return false;
                    size_t start = out.size() - d;
                    for (int k = 0; k < len; ++k) out.push_back(out[start + k]);
                }
            }
        }
        if (last) break;
    }
    return true;
}

// ------------------------------------------------------------------ PNG
static void putBE32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));  v.push_back((uint8_t)x);
}
static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    putBE32(out, (uint32_t)data.size());
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t c = crc32up(0, out.data() + start, out.size() - start);
    putBE32(out, c);
}

bool pngWrite(const char* path, const Image& img) {
    if (!img.valid()) return false;
    std::vector<uint8_t> raw;
    raw.reserve((size_t)img.h * (img.w * 4 + 1));
    for (int y = 0; y < img.h; ++y) {
        raw.push_back(1);  // filter: sub
        int pr = 0, pg = 0, pb = 0, pa = 0;
        for (int x = 0; x < img.w; ++x) {
            Color c = img.px[(size_t)y * img.w + x];
            int r = colR(c), g = colG(c), b = colB(c), a = colA(c);
            raw.push_back((uint8_t)((r - pr) & 255));
            raw.push_back((uint8_t)((g - pg) & 255));
            raw.push_back((uint8_t)((b - pb) & 255));
            raw.push_back((uint8_t)((a - pa) & 255));
            pr = r; pg = g; pb = b; pa = a;
        }
    }
    std::vector<uint8_t> comp;
    comp.push_back(0x78); comp.push_back(0x01);
    deflateFixed(raw.data(), raw.size(), comp);
    putBE32(comp, adler32(raw.data(), raw.size()));
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    putBE32(ihdr, (uint32_t)img.w); putBE32(ihdr, (uint32_t)img.h);
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", comp);
    chunk(out, "IEND", std::vector<uint8_t>());
    return writeFile(path, out.data(), out.size());
}

bool pngDecode(const uint8_t* d, size_t n, Image& out) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (n < 8 || memcmp(d, sig, 8) != 0) return false;
    size_t p = 8;
    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    std::vector<uint8_t> idat;
    while (p + 8 <= n) {
        uint32_t len = ((uint32_t)d[p] << 24) | ((uint32_t)d[p + 1] << 16) | ((uint32_t)d[p + 2] << 8) | d[p + 3];
        const char* type = (const char*)(d + p + 4);
        if (p + 12 + len > n) break;
        if (!memcmp(type, "IHDR", 4)) {
            const uint8_t* q = d + p + 8;
            w = (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
            h = (q[4] << 24) | (q[5] << 16) | (q[6] << 8) | q[7];
            depth = q[8]; ctype = q[9]; interlace = q[12];
        } else if (!memcmp(type, "IDAT", 4)) {
            idat.insert(idat.end(), d + p + 8, d + p + 8 + len);
        } else if (!memcmp(type, "IEND", 4)) break;
        p += 12 + len;
    }
    if (w <= 0 || h <= 0 || depth != 8 || interlace != 0) return false;
    if (ctype != 0 && ctype != 2 && ctype != 4 && ctype != 6) return false;
    size_t raw;
    if (idat.size() < 2) return false;
    std::vector<uint8_t> data;
    if (!inflateData(idat.data() + 2, idat.size() - 2, data)) return false;
    int ch = (ctype == 0) ? 1 : (ctype == 2 ? 3 : (ctype == 4 ? 2 : 4));
    size_t stride = (size_t)w * ch;
    raw = (stride + 1) * h;
    if (data.size() < raw) return false;
    std::vector<uint8_t> img(raw);
    size_t prevOff = 0;
    for (int y = 0; y < h; ++y) {
        uint8_t f = data[y * (stride + 1)];
        const uint8_t* src = data.data() + y * (stride + 1) + 1;
        uint8_t* dst = img.data() + y * stride;
        const uint8_t* prev = (y > 0) ? img.data() + (y - 1) * stride : nullptr;
        for (size_t x = 0; x < stride; ++x) {
            int a = (x >= (size_t)ch) ? dst[x - ch] : 0;
            int b = prev ? prev[x] : 0;
            int c = (prev && x >= (size_t)ch) ? prev[x - ch] : 0;
            int v = src[x];
            switch (f) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: {
                    int pp = a + b - c;
                    int pa = abs(pp - a), pb = abs(pp - b), pc = abs(pp - c);
                    v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    break;
                }
                default: break;
            }
            dst[x] = (uint8_t)(v & 255);
        }
    }
    out.resize(w, h, 0xFF000000);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const uint8_t* s = img.data() + (size_t)y * stride + (size_t)x * ch;
            Color c;
            if (ch == 1) c = rgba(s[0], s[0], s[0], 255);
            else if (ch == 2) c = rgba(s[0], s[0], s[0], s[1]);
            else if (ch == 3) c = rgba(s[0], s[1], s[2], 255);
            else c = rgba(s[0], s[1], s[2], s[3]);
            out.set(x, y, c);
        }
    }
    return true;
}

bool pngLoad(const char* path, Image& out) {
    std::vector<uint8_t> d;
    if (!readFile(path, d)) return false;
    return pngDecode(d.data(), d.size(), out);
}

// ------------------------------------------------------------------ hash / noise

Noise::Noise(uint32_t seed) {
    Rng r(seed * 2654435761u + 12345u);
    for (int i = 0; i < 256; ++i) perm[i] = (uint8_t)i;
    for (int i = 255; i > 0; --i) std::swap(perm[i], perm[r.u32() % (uint32_t)(i + 1)]);
    for (int i = 0; i < 256; ++i) perm[256 + i] = perm[i];
}

static float fadeC(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }

float Noise::value2(float x, float y) const {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float xf = x - xi, yf = y - yi;
    auto h = [&](int a, int b) {
        return hashf((uint32_t)(perm[(a & 255)] + perm[(b & 255)] * 256 + 1000)) * 2.0f - 1.0f;
    };
    float v00 = h(xi, yi), v10 = h(xi + 1, yi), v01 = h(xi, yi + 1), v11 = h(xi + 1, yi + 1);
    float u = fadeC(xf), v = fadeC(yf);
    return lerpf(lerpf(v00, v10, u), lerpf(v01, v11, u), v);
}

float Noise::value3(float x, float y, float z) const {
    int xi = (int)std::floor(x), yi = (int)std::floor(y), zi = (int)std::floor(z);
    float xf = x - xi, yf = y - yi, zf = z - zi;
    auto h = [&](int a, int b, int c) {
        return hashf((uint32_t)(perm[a & 255] + perm[b & 255] * 256 + perm[c & 255] * 65536 + 77)) * 2.0f - 1.0f;
    };
    float u = fadeC(xf), v = fadeC(yf), w = fadeC(zf);
    float c000 = h(xi, yi, zi),       c100 = h(xi + 1, yi, zi);
    float c010 = h(xi, yi + 1, zi),   c110 = h(xi + 1, yi + 1, zi);
    float c001 = h(xi, yi, zi + 1),   c101 = h(xi + 1, yi, zi + 1);
    float c011 = h(xi, yi + 1, zi + 1), c111 = h(xi + 1, yi + 1, zi + 1);
    float x00 = lerpf(c000, c100, u), x10 = lerpf(c010, c110, u);
    float x01 = lerpf(c001, c101, u), x11 = lerpf(c011, c111, u);
    return lerpf(lerpf(x00, x10, v), lerpf(x01, x11, v), w);
}

float Noise::fbm2(float x, float y, int oct, float lac, float gain) const {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < oct; ++i) {
        sum += value2(x, y) * amp;
        norm += amp;
        x *= lac; y *= lac; amp *= gain;
    }
    return sum / (norm + 1e-6f);
}
float Noise::fbm3(float x, float y, float z, int oct) const {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < oct; ++i) {
        sum += value3(x, y, z) * amp;
        norm += amp;
        x *= 2.0f; y *= 2.0f; z *= 2.0f; amp *= 0.5f;
    }
    return sum / (norm + 1e-6f);
}
float Noise::ridged2(float x, float y, int oct) const {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < oct; ++i) {
        float n = 1.0f - std::fabs(value2(x, y));
        n *= n;
        sum += n * amp;
        norm += amp;
        x *= 2.0f; y *= 2.0f; amp *= 0.5f;
    }
    return sum / (norm + 1e-6f);
}
float Noise::worley2(float x, float y) const {
    float id;
    return worley2b(x, y, id);
}
float Noise::worley2b(float x, float y, float& id) const {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float best = 8.0f, bestId = 0;
    for (int j = -1; j <= 1; ++j) for (int i = -1; i <= 1; ++i) {
        int cx = xi + i, cy = yi + j;
        uint32_t hsh = hash32((uint32_t)(perm[cx & 255] * 1973 + perm[cy & 255] * 9277));
        float px = cx + (hsh & 0xFFFF) / 65535.0f;
        float py = cy + ((hsh >> 16) & 0xFFFF) / 65535.0f;
        float dx = px - x, dy = py - y;
        float d = dx * dx + dy * dy;
        if (d < best) { best = d; bestId = (hsh % 1000) / 1000.0f; }
    }
    id = bestId;
    return std::sqrt(best);
}

// ------------------------------------------------------------------ text / io
std::string strfmt(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return std::string(buf);
}

bool writeFile(const char* path, const void* data, size_t n) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    if (n) fwrite(data, 1, n, f);
    fclose(f);
    return true;
}

bool readFile(const char* path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize((size_t)maxi(0, (int)n));
    if (n > 0 && fread(out.data(), 1, (size_t)n, f) != (size_t)n) { fclose(f); return false; }
    fclose(f);
    return true;
}

bool fileExists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

std::string exeDir() {
#ifdef _WIN32
    char buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    std::string s(buf, n);
    size_t p = s.find_last_of("\\/");
    return p == std::string::npos ? std::string(".") : s.substr(0, p);
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string(".");
    buf[n] = 0;
    std::string s(buf);
    size_t p = s.find_last_of('/');
    return p == std::string::npos ? std::string(".") : s.substr(0, p);
#endif
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char last = a.back();
    if (last == '/' || last == '\\') return a + b;
#ifdef _WIN32
    return a + "\\" + b;
#else
    return a + "/" + b;
#endif
}

void ensureDir(const char* path) {
#ifdef _WIN32
    CreateDirectoryA(path, nullptr);
#else
    mkdir(path, 0755);
#endif
}

std::string userDataDir(const char* appName) {
#ifdef _WIN32
    const char* base = getenv("APPDATA");
    if (!base) base = getenv("LOCALAPPDATA");
    if (!base) base = ".";
    std::string d = joinPath(base, appName);
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    std::string d = joinPath(joinPath(home, ".local/share"), appName);
    std::string parent = joinPath(joinPath(home, ".local"), "share");
    ensureDir(joinPath(home, ".local").c_str());
    ensureDir(parent.c_str());
#endif
    ensureDir(d.c_str());
    return d;
}

uint64_t nowMillis() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
double nowSeconds() { return nowMillis() / 1000.0; }
void sleepMillis(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void logMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
}

} // namespace nf

// ---------------------------------------------------------------- thread pool
namespace nf {

struct Parallel::Impl {
    std::vector<std::thread> workers;
    std::mutex m;
    std::condition_variable cvWork, cvDone;
    const std::function<void(int)>* fn = nullptr;
    std::atomic<int> next{0};
    int total = 0;
    int pending = 0;
    uint64_t generation = 0;
    bool quit = false;
};

Parallel& Parallel::get() {
    static Parallel p;
    return p;
}

void Parallel::start(int nWorkers) {
    impl_ = new Impl();
    impl_->workers.reserve((size_t)nWorkers);
    for (int i = 0; i < nWorkers; ++i) {
        impl_->workers.emplace_back([this, i] {
            Impl& I = *impl_;
            uint64_t seen = 0;
            for (;;) {
                std::unique_lock<std::mutex> lk(I.m);
                I.cvWork.wait(lk, [&] { return I.quit || I.generation != seen; });
                if (I.quit) return;
                seen = I.generation;
                lk.unlock();
                for (;;) {
                    int idx = I.next.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= I.total) break;
                    (*I.fn)(idx);
                }
                lk.lock();
                if (--I.pending <= 0) I.cvDone.notify_one();
            }
        });
    }
    nWorkers_ = nWorkers;
}

void Parallel::run(int nTasks, const std::function<void(int)>& fn) {
    if (nTasks <= 0) return;
    unsigned hc = std::thread::hardware_concurrency();
    hc = hc ? hc : 1;
    int want = (int)std::min<unsigned>(hc, 8u) - 1;   // workers besides this thread
    if (want < 0) want = 0;
    if (!impl_) start(want);
    if (nTasks == 1 || nWorkers_ == 0) {
        for (int i = 0; i < nTasks; ++i) fn(i);
        return;
    }
    Impl& I = *impl_;
    {
        std::lock_guard<std::mutex> lk(I.m);
        I.fn = &fn;
        I.total = nTasks;
        I.next.store(0, std::memory_order_relaxed);
        I.pending = nWorkers_;
        ++I.generation;
        I.cvWork.notify_all();
    }
    for (;;) {
        int idx = I.next.fetch_add(1, std::memory_order_relaxed);
        if (idx >= nTasks) break;
        fn(idx);
    }
    std::unique_lock<std::mutex> lk(I.m);
    I.cvDone.wait(lk, [&] { return I.pending <= 0; });
}

void Parallel::shutdown() {
    if (!impl_) return;
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->quit = true;
        impl_->cvWork.notify_all();
    }
    for (auto& t : impl_->workers) if (t.joinable()) t.join();
    delete impl_;
    impl_ = nullptr;
    nWorkers_ = 0;
}

} // namespace nf
