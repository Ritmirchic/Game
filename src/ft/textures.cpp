// FRACTURE — procedural PBR-ish texture atlas + sprite sheets + material table
#include "render.h"
#include "rng.h"

namespace ft {

// ------------------------------------------------------------ material GPU --
static MatGpu g_matGpu[256];
static bool g_matBuilt = false;

const MatGpu* matGpuTable() {
    if (!g_matBuilt) buildMatGpuTable();
    return g_matGpu;
}

void buildMatGpuTable() {
    for (int i = 0; i < 256; i++) g_matGpu[i] = {0.85f, 0.f, 0.5f, 0.f};
    auto set = [](int id, f32 rough, f32 metal, f32 uv, f32 emis) {
        g_matGpu[id] = MatGpu{rough, metal, uv, emis};
    };
    set(MAT_AIR, 1, 0, 0.5f, 0);
    set(MAT_CONCRETE, 0.92f, 0.f, 0.45f, 0);
    set(MAT_BRICK, 0.88f, 0.f, 0.5f, 0);
    set(MAT_WOOD, 0.8f, 0.f, 0.55f, 0);
    set(MAT_GLASS, 0.06f, 0.f, 0.5f, 0);
    set(MAT_METAL, 0.32f, 1.f, 0.5f, 0);
    set(MAT_REBAR, 0.75f, 0.15f, 0.45f, 0);
    set(MAT_PLASTER, 0.95f, 0.f, 0.4f, 0);
    set(MAT_SOIL, 0.98f, 0.f, 0.4f, 0);
    set(MAT_ASPHALT, 0.95f, 0.f, 0.5f, 0);
    set(MAT_STONE, 0.85f, 0.f, 0.45f, 0);
    set(MAT_TILE, 0.35f, 0.f, 0.5f, 0);
    set(MAT_SAND, 0.98f, 0.f, 0.4f, 0);
    set(MAT_LEAF, 0.9f, 0.f, 0.6f, 0);
    set(MAT_PLASTIC, 0.45f, 0.f, 0.5f, 0);
    set(MAT_CARDBOARD, 0.95f, 0.f, 0.5f, 0);
    set(MAT_FABRIC, 0.98f, 0.f, 0.6f, 0);
    set(MAT_PAINTED, 0.4f, 0.7f, 0.5f, 0);
    set(MAT_RUBBER, 0.95f, 0.f, 0.6f, 0);
    set(MAT_DIRTROAD, 0.98f, 0.f, 0.4f, 0);
    set(MAT_GRAVEL, 0.95f, 0.f, 0.45f, 0);
    set(MAT_SNOW, 0.85f, 0.f, 0.35f, 0);
    set(MAT_ROCK, 0.9f, 0.f, 0.4f, 0);
    set(MAT_WATER, 0.05f, 0.f, 0.12f, 0);
    set(MAT_SANDST, 0.88f, 0.f, 0.4f, 0);
    set(MAT_CERAMIC, 0.2f, 0.f, 0.5f, 0);
    g_matBuilt = true;
}

// ------------------------------------------------------------------ helpers --
namespace {

struct TileCtx {
    int T;
    std::vector<f32> r, g, b, hgt;  // T*T each, hgt in0..1
    explicit TileCtx(int t) : T(t), r(t * t), g(t * t), b(t * t), hgt(t * t) {}
    inline int idx(int x, int y) const { return x + y * T; }
    void fill(f32 R, f32 G, f32 B, f32 h = 0.5f) {
        for (int i = 0; i < T * T; i++) { r[i] = R; g[i] = G; b[i] = B; hgt[i] = h; }
    }
    void shade(int x, int y, f32 f) { // multiplicative light
        int i = idx(x, y); r[i] *= f; g[i] *= f; b[i] *= f;
    }
};

f32 hash01(int x, int y, u32 s) { return vnoise2(x, y, s); }

// smooth value noise at texel scale
f32 n2(f32 x, f32 y, u32 s) { return fbm2(x, y, s); }

} // namespace

// ------------------------------------------------------------------- tiles --
static void genConcrete(TileCtx& c, bool interior, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 fine = n2(x * 0.11f / s, y * 0.11f / s, seed) * 0.5f + n2(x * 0.5f / s, y * 0.5f / s, seed + 7) * 0.5f;
        f32 blotch = n2(x * 0.014f / s, y * 0.014f / s, seed + 13);
        f32 base = 0.86f + fine * 0.18f - (blotch > 0.62f ? (blotch - 0.62f) * 0.7f : 0.f);
        if (interior) {
            // exposed aggregate: pebbles
            f32 peb = n2(x * 0.16f / s, y * 0.16f / s, seed + 31);
            if (peb > 0.68f) { base += 0.16f; c.hgt[i] = 0.62f; }
            else c.hgt[i] = 0.45f + fine * 0.1f;
            base *= 0.82f;
        } else {
            c.hgt[i] = 0.5f + fine * 0.06f;
            // form-tie holes
            int hx = ((x / (int)(44 * s)) * (int)(44 * s) + (int)(22 * s));
            int hy = ((y / (int)(64 * s)) * (int)(64 * s) + (int)(32 * s));
            int dx = x - hx, dy = y - hy;
            f32 rr = (f32)(dx * dx + dy * dy) / ((f32)(5 * s) * (5 * s) + 1);
            if (rr < 1) { base *= 0.62f + 0.3f * rr; c.hgt[i] = 0.35f; }
        }
        c.r[i] = 0.59f * base; c.g[i] = 0.59f * base; c.b[i] = 0.575f * base;
    }
}

static void genBrick(TileCtx& c, bool interior, u32 seed) {
    f32 s = c.T / 128.f;
    int bw = (int)(32 * s), bh = (int)(16 * s), mort = (int)(3 * s);
    if (bw < 6) bw = 6; if (bh < 4) bh = 4; if (mort < 1) mort = 1;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        int row = y / bh;
        int xx = x + (row % 2 ? bw / 2 : 0);
        int bx = xx % bw, by = y % bh;
        int bid = (xx / bw) * 31 + row * 17;
        f32 jit = hash01(bid, row, seed) * 0.22f - 0.11f;
        f32 grain = n2(x * 0.35f / s, y * 0.35f / s, seed + 5) * 0.12f;
        bool mortar = (bx < mort) || (by < mort) || (bx >= bw - mort) || (by >= bh - mort);
        if (mortar) {
            f32 m = 0.72f + n2(x * 0.5f / s, y * 0.5f / s, seed + 9) * 0.2f;
            c.r[i] = 0.68f * m; c.g[i] = 0.66f * m; c.b[i] = 0.61f * m;
            c.hgt[i] = 0.36f;
        } else {
            f32 base = 0.94f + jit + grain;
            if (interior) base *= 0.86f;
            c.r[i] = 0.61f * base; c.g[i] = 0.34f * base; c.b[i] = 0.27f * base;
            c.hgt[i] = 0.62f;
            // bevel near edges
            f32 edge = (f32)tmin(tmin(bx - mort, by - mort), tmin(bw - mort - bx, bh - mort - by)) / (2.5f * s + 1);
            if (edge < 1) c.hgt[i] = 0.5f + edge * 0.12f;
        }
    }
}

static void genWood(TileCtx& c, bool interior, u32 seed) {
    f32 s = c.T / 128.f;
    int plank = (int)(32 * s); if (plank < 8) plank = 8;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        int pid = y / plank;
        f32 py = (f32)(y % plank) / plank;
        f32 grain = n2(x * 0.045f / s, y * 0.5f / s, seed + pid * 13) ;
        f32 rings = std::sin(y * 0.19f / s + grain * 6.5f + pid * 2.f) * 0.5f + 0.5f;
        f32 fine = n2(x * 0.7f / s, y * 0.3f / s, seed + 77);
        f32 v = 0.78f + rings * 0.2f + fine * 0.1f;
        if (py < 0.045f || py > 0.955f) { v *= 0.55f; }   // plank gap
        if (interior) v *= 0.88f;
        c.r[i] = 0.62f * v; c.g[i] = 0.44f * v; c.b[i] = 0.27f * v;
        c.hgt[i] = (py < 0.045f || py > 0.955f) ? 0.3f : 0.55f + rings * 0.08f;
    }
}

static void genGlass(TileCtx& c, u32 seed) {
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 smudge = n2(x * 0.05f, y * 0.05f, seed) * 0.08f;
        c.r[i] = 0.72f + smudge; c.g[i] = 0.83f + smudge; c.b[i] = 0.87f + smudge;
        c.hgt[i] = 0.5f;
    }
}

static void genMetal(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    int panel = (int)(42 * s); if (panel < 10) panel = 10;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 brush = n2(x * 0.9f / s, y * 0.03f / s, seed) * 0.12f;
        f32 v = 0.86f + brush;
        f32 seam = (f32)(x % panel) / panel;
        if (seam < 0.035f) v *= 0.55f;
        // rivets along seams
        f32 ry = (f32)(y % (panel / 2)) - panel / 2.f;
        f32 dxr = (f32)(x % panel) - panel * 0.5f;
        f32 rr = (dxr * dxr + ry * ry) / ((f32)(3 * s) * (3 * s) + 1);
        if (seam < 0.12f && rr < 1) { v *= 1.18f; c.hgt[i] = 0.68f; }
        else c.hgt[i] = seam < 0.035f ? 0.36f : 0.5f;
        c.r[i] = 0.52f * v; c.g[i] = 0.545f * v; c.b[i] = 0.575f * v;
    }
}

static void genPlaster(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 n = n2(x * 0.09f / s, y * 0.09f / s, seed) * 0.14f;
        f32 crack = n2(x * 0.02f / s, y * 0.02f / s, seed + 4);
        f32 v = 0.9f + n;
        if (crack > 0.74f) v *= 0.8f;
        c.r[i] = 0.88f * v; c.g[i] = 0.855f * v; c.b[i] = 0.81f * v;
        c.hgt[i] = 0.5f + n * 0.4f;
    }
}

static void genSoil(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 clump = n2(x * 0.07f / s, y * 0.07f / s, seed);
        f32 fine = n2(x * 0.4f / s, y * 0.4f / s, seed + 3);
        f32 v = 0.7f + clump * 0.4f + fine * 0.16f;
        c.r[i] = 0.42f * v; c.g[i] = 0.32f * v; c.b[i] = 0.21f * v;
        c.hgt[i] = 0.35f + clump * 0.4f;
    }
}

static void genAsphalt(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 grit = n2(x * 0.55f / s, y * 0.55f / s, seed);
        f32 patch = n2(x * 0.03f / s, y * 0.03f / s, seed + 8);
        f32 v = 0.66f + grit * 0.5f + (patch > 0.65f ? 0.1f : 0.f);
        c.r[i] = 0.28f * v; c.g[i] = 0.285f * v; c.b[i] = 0.30f * v;
        c.hgt[i] = 0.4f + grit * 0.35f;
    }
}

static void genStone(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 v1 = n2(x * 0.05f / s, y * 0.05f / s, seed);
        f32 ridge = std::fabs(n2(x * 0.11f / s, y * 0.11f / s, seed + 2) - 0.5f) * 2.f;
        f32 crack = ridge < 0.09f ? 0.62f : 1.f;
        f32 v = (0.66f + v1 * 0.4f) * crack;
        c.r[i] = 0.55f * v; c.g[i] = 0.54f * v; c.b[i] = 0.515f * v;
        c.hgt[i] = 0.4f + v1 * 0.4f * (crack < 1 ? 0.3f : 1.f);
    }
}

static void genRoofTile(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    int tw = (int)(24 * s), th = (int)(20 * s); if (tw < 8) tw = 8; if (th < 6) th = 6;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        int row = y / th;
        int xx = x + (row % 2 ? tw / 2 : 0);
        f32 u = (f32)(xx % tw) / tw;
        f32 v = (f32)(y % th) / th;
        f32 curve = std::sin(u * 3.14159f);
        f32 jit = hash01(xx / tw, row, seed) * 0.16f - 0.08f;
        f32 shadeV = 0.72f + curve * 0.3f + jit;
        if (v > 0.86f) shadeV *= 0.55f;  // overlap shadow
        c.r[i] = 0.52f * shadeV; c.g[i] = 0.27f * shadeV; c.b[i] = 0.2f * shadeV;
        c.hgt[i] = 0.35f + curve * 0.35f - (v > 0.86f ? 0.2f : 0.f);
    }
}

static void genSand(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 ripple = std::sin((x + n2(x * 0.03f / s, y * 0.03f / s, seed) * 40.f / s) * 0.16f / s) * 0.5f + 0.5f;
        f32 fine = n2(x * 0.7f / s, y * 0.7f / s, seed + 4);
        f32 v = 0.82f + ripple * 0.14f + fine * 0.1f;
        c.r[i] = 0.82f * v; c.g[i] = 0.72f * v; c.b[i] = 0.52f * v;
        c.hgt[i] = 0.4f + ripple * 0.25f;
    }
}

static void genLeaf(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 blob = n2(x * 0.13f / s, y * 0.13f / s, seed);
        f32 edge = n2(x * 0.33f / s, y * 0.33f / s, seed + 6);
        f32 v = 0.55f + blob * 0.7f;
        if (edge > 0.72f) v *= 1.25f;   // leaf highlights
        c.r[i] = 0.28f * v; c.g[i] = 0.47f * v; c.b[i] = 0.19f * v;
        c.hgt[i] = 0.4f + blob * 0.4f;
    }
}

static void genPlastic(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 n = n2(x * 0.08f / s, y * 0.08f / s, seed) * 0.1f;
        f32 scratch = (n2(x * 0.5f / s, y * 0.02f / s, seed + 11) > 0.7f) ? 0.08f : 0.f;
        f32 v = 0.9f + n + scratch;
        c.r[i] = 0.78f * v; c.g[i] = 0.77f * v; c.b[i] = 0.75f * v;
        c.hgt[i] = 0.5f + n * 0.3f;
    }
}

static void genCardboard(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 fiber = n2(x * 0.6f / s, y * 0.06f / s, seed) * 0.14f;
        f32 v = 0.86f + fiber;
        // tape strip
        if (std::fabs((f32)x - c.T * 0.5f) < 10 * s) v *= 1.1f;
        c.r[i] = 0.66f * v; c.g[i] = 0.53f * v; c.b[i] = 0.37f * v;
        c.hgt[i] = 0.5f + fiber * 0.5f;
    }
}

static void genFabric(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 weave = ((x / (int)(3 * s + 1)) % 2 ^ (y / (int)(3 * s + 1)) % 2) ? 1.08f : 0.9f;
        f32 n = n2(x * 0.2f / s, y * 0.2f / s, seed) * 0.1f;
        f32 v = weave + n;
        c.r[i] = 0.47f * v; c.g[i] = 0.42f * v; c.b[i] = 0.53f * v;
        c.hgt[i] = 0.5f + (weave - 1.f) * 0.6f;
    }
}

static void genPainted(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 brush = n2(x * 0.5f / s, y * 0.05f / s, seed) * 0.1f;
        f32 chip = n2(x * 0.08f / s, y * 0.08f / s, seed + 15);
        f32 v = 0.88f + brush;
        f32 R = 0.3f, G = 0.4f, B = 0.55f;
        if (chip > 0.78f) { R = 0.5f; G = 0.52f; B = 0.55f; }  // scratched to metal
        c.r[i] = R * v; c.g[i] = G * v; c.b[i] = B * v;
        c.hgt[i] = chip > 0.78f ? 0.44f : 0.53f;
    }
}

static void genRubber(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 n = n2(x * 0.5f / s, y * 0.5f / s, seed) * 0.16f;
        f32 v = 0.62f + n;
        c.r[i] = 0.16f * v; c.g[i] = 0.16f * v; c.b[i] = 0.17f * v;
        c.hgt[i] = 0.5f + n;
    }
}

static void genGravel(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 p = n2(x * 0.24f / s, y * 0.24f / s, seed);
        f32 e = n2(x * 0.6f / s, y * 0.6f / s, seed + 7);
        f32 v = 0.6f + p * 0.55f + e * 0.2f;
        c.r[i] = 0.52f * v; c.g[i] = 0.51f * v; c.b[i] = 0.485f * v;
        c.hgt[i] = 0.3f + p * 0.6f;
    }
}

static void genSnow(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 drift = n2(x * 0.06f / s, y * 0.06f / s, seed);
        f32 spark = n2(x * 0.9f / s, y * 0.9f / s, seed + 3) > 0.86f ? 1.2f : 1.f;
        f32 v = (0.9f + drift * 0.14f) * spark;
        c.r[i] = 0.93f * v; c.g[i] = 0.95f * v; c.b[i] = 0.98f * v;
        c.hgt[i] = 0.42f + drift * 0.3f;
    }
}

static void genRock(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 strat = std::sin(y * 0.09f / s + n2(x * 0.04f / s, y * 0.04f / s, seed) * 5.f) * 0.5f + 0.5f;
        f32 n = n2(x * 0.2f / s, y * 0.2f / s, seed + 5);
        f32 v = 0.7f + strat * 0.3f + n * 0.16f;
        c.r[i] = 0.48f * v; c.g[i] = 0.465f * v; c.b[i] = 0.44f * v;
        c.hgt[i] = 0.35f + strat * 0.4f;
    }
}

static void genWater(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 w = n2(x * 0.09f / s, y * 0.09f / s, seed) * 0.5f + n2(x * 0.03f / s, y * 0.03f / s, seed + 2) * 0.5f;
        f32 v = 0.85f + w * 0.3f;
        c.r[i] = 0.16f * v; c.g[i] = 0.38f * v; c.b[i] = 0.55f * v;
        c.hgt[i] = 0.4f + w * 0.5f;
    }
}

static void genSandstone(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 band = std::sin(y * 0.13f / s + n2(x * 0.03f / s, y * 0.02f / s, seed) * 6.f) * 0.5f + 0.5f;
        f32 grain = n2(x * 0.5f / s, y * 0.5f / s, seed + 4) * 0.14f;
        f32 v = 0.72f + band * 0.3f + grain;
        c.r[i] = 0.72f * v; c.g[i] = 0.55f * v; c.b[i] = 0.36f * v;
        c.hgt[i] = 0.4f + band * 0.35f;
    }
}

static void genCeramic(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    int tw = (int)(32 * s); if (tw < 8) tw = 8;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 u = (f32)(x % tw) / tw, v = (f32)(y % tw) / tw;
        f32 grout = (u < 0.05f || v < 0.05f) ? 1.f : 0.f;
        f32 n = n2(x * 0.2f / s, y * 0.2f / s, seed) * 0.08f;
        if (grout) {
            c.r[i] = 0.55f; c.g[i] = 0.54f; c.b[i] = 0.52f;
            c.hgt[i] = 0.34f;
        } else {
            f32 v2 = 0.92f + n;
            c.r[i] = 0.88f * v2; c.g[i] = 0.88f * v2; c.b[i] = 0.86f * v2;
            c.hgt[i] = 0.55f;
        }
    }
}

static void genDirtRoad(TileCtx& c, u32 seed) {
    f32 s = c.T / 128.f;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 rut = std::sin(x * 0.07f / s) * 0.5f + 0.5f;
        f32 n = n2(x * 0.28f / s, y * 0.28f / s, seed);
        f32 v = 0.72f + n * 0.4f - rut * 0.08f;
        c.r[i] = 0.47f * v; c.g[i] = 0.38f * v; c.b[i] = 0.28f * v;
        c.hgt[i] = 0.38f + n * 0.4f - rut * 0.1f;
    }
}

static void genRebarConcrete(TileCtx& c, u32 seed) {
    genConcrete(c, true, seed);
    f32 s = c.T / 128.f;
    // grid of exposed rebar rods
    int step = (int)(40 * s); if (step < 10) step = 10;
    for (int y = 0; y < c.T; y++) for (int x = 0; x < c.T; x++) {
        int i = c.idx(x, y);
        f32 dx = (f32)(x % step) - step * 0.5f;
        f32 dy = (f32)(y % step) - step * 0.5f;
        f32 rr = (dx * dx) / ((f32)(3.2f * s) * (3.2f * s) + 1);   // vertical rod
        if (rr < 1) { f32 sh = 0.45f + (1 - rr) * 0.5f; c.r[i] = 0.42f * sh; c.g[i] = 0.3f * sh; c.b[i] = 0.22f * sh; c.hgt[i] = 0.7f; }
        f32 rr2 = (dy * dy) / ((f32)(3.2f * s) * (3.2f * s) + 1);
        if (rr2 < 1) { f32 sh = 0.45f + (1 - rr2) * 0.5f; c.r[i] = 0.44f * sh; c.g[i] = 0.32f * sh; c.b[i] = 0.24f * sh; c.hgt[i] = 0.7f; }
    }
}

// ------------------------------------------------------------------ atlas ---
static int g_baseSlot[MAT_COUNT] = {0};
static bool g_baseSlotInit = false;

bool buildTexAtlas(TexAtlas& out, int tilePx) {
    out.tiles = 8;
    out.tilePx = tilePx;
    int S = out.tiles * tilePx;
    out.albedo.assign((size_t)S * S * 4, 255);
    out.normal.assign((size_t)S * S * 4, 128);
    Rng rng(0xC0FFEE);

    // tile slot per material (+32 = interior variant)
    struct Slot { int mat; bool interior; void (*fn)(TileCtx&, bool, u32); u32 seed; };
    auto wrap = [](void (*f)(TileCtx&, u32)) { (void)f; };
    (void)wrap;
    // generate function table using lambdas converted through static funcs
    struct Gen { int mat; bool interior; void (*fn)(TileCtx&, bool, u32); };
    static const Gen gens[] = {
        {MAT_CONCRETE, false, [](TileCtx& c, bool, u32 sd) { genConcrete(c, false, sd); }},
        {MAT_BRICK, false, [](TileCtx& c, bool, u32 sd) { genBrick(c, false, sd); }},
        {MAT_WOOD, false, [](TileCtx& c, bool, u32 sd) { genWood(c, false, sd); }},
        {MAT_GLASS, false, [](TileCtx& c, bool, u32 sd) { genGlass(c, sd); }},
        {MAT_METAL, false, [](TileCtx& c, bool, u32 sd) { genMetal(c, sd); }},
        {MAT_REBAR, false, [](TileCtx& c, bool, u32 sd) { genRebarConcrete(c, sd); }},
        {MAT_PLASTER, false, [](TileCtx& c, bool, u32 sd) { genPlaster(c, sd); }},
        {MAT_SOIL, false, [](TileCtx& c, bool, u32 sd) { genSoil(c, sd); }},
        {MAT_ASPHALT, false, [](TileCtx& c, bool, u32 sd) { genAsphalt(c, sd); }},
        {MAT_STONE, false, [](TileCtx& c, bool, u32 sd) { genStone(c, sd); }},
        {MAT_TILE, false, [](TileCtx& c, bool, u32 sd) { genRoofTile(c, sd); }},
        {MAT_SAND, false, [](TileCtx& c, bool, u32 sd) { genSand(c, sd); }},
        {MAT_LEAF, false, [](TileCtx& c, bool, u32 sd) { genLeaf(c, sd); }},
        {MAT_PLASTIC, false, [](TileCtx& c, bool, u32 sd) { genPlastic(c, sd); }},
        {MAT_CARDBOARD, false, [](TileCtx& c, bool, u32 sd) { genCardboard(c, sd); }},
        {MAT_FABRIC, false, [](TileCtx& c, bool, u32 sd) { genFabric(c, sd); }},
        {MAT_PAINTED, false, [](TileCtx& c, bool, u32 sd) { genPainted(c, sd); }},
        {MAT_RUBBER, false, [](TileCtx& c, bool, u32 sd) { genRubber(c, sd); }},
        {MAT_DIRTROAD, false, [](TileCtx& c, bool, u32 sd) { genDirtRoad(c, sd); }},
        {MAT_GRAVEL, false, [](TileCtx& c, bool, u32 sd) { genGravel(c, sd); }},
        {MAT_SNOW, false, [](TileCtx& c, bool, u32 sd) { genSnow(c, sd); }},
        {MAT_ROCK, false, [](TileCtx& c, bool, u32 sd) { genRock(c, sd); }},
        {MAT_WATER, false, [](TileCtx& c, bool, u32 sd) { genWater(c, sd); }},
        {MAT_SANDST, false, [](TileCtx& c, bool, u32 sd) { genSandstone(c, sd); }},
        {MAT_CERAMIC, false, [](TileCtx& c, bool, u32 sd) { genCeramic(c, sd); }},
        // interiors (fresh-cut look) for the key materials
        {MAT_CONCRETE, true, [](TileCtx& c, bool in, u32 sd) { genConcrete(c, in, sd); }},
        {MAT_BRICK, true, [](TileCtx& c, bool in, u32 sd) { genBrick(c, in, sd); }},
        {MAT_WOOD, true, [](TileCtx& c, bool in, u32 sd) { genWood(c, in, sd); }},
        {MAT_PLASTER, true, [](TileCtx& c, bool, u32 sd) { genPlaster(c, sd); }},
        {MAT_METAL, true, [](TileCtx& c, bool, u32 sd) { genMetal(c, sd); }},
        {MAT_REBAR, true, [](TileCtx& c, bool, u32 sd) { genRebarConcrete(c, sd); }},
        {MAT_PAINTED, true, [](TileCtx& c, bool, u32 sd) { genPainted(c, sd); }},
        {MAT_SANDST, true, [](TileCtx& c, bool, u32 sd) { genSandstone(c, sd); }},
    };
    // material → slot mapping (base tile index0..31), published globally
    static int baseSlot[MAT_COUNT];
    static bool slotsReady = false;
    int nextSlot = 0;
    for (int i = 0; i < MAT_COUNT; i++) baseSlot[i] = 0;
    for (const Gen& g : gens) {
        if (g.interior) continue;
        baseSlot[g.mat] = nextSlot++;
    }
    slotsReady = true; (void)slotsReady;
    for (int i = 0; i < MAT_COUNT; i++) g_baseSlot[i] = baseSlot[i];
    for (const Gen& g : gens) if (g.interior) baseSlot[g.mat] = baseSlot[g.mat]; // interior = base+32

    for (const Gen& g : gens) {
        int slot = baseSlot[g.mat] + (g.interior ? 32 : 0);
        TileCtx ctx(tilePx);
        g.fn(ctx, g.interior, 0x1234 + g.mat * 77 + (g.interior ? 999 : 0));
        int tx = (slot % out.tiles) * tilePx;
        int ty = (slot / out.tiles) * tilePx;
        // blit albedo (+ tile-able: patterns are constructed tile-able by design for bricks? not all perfectly — acceptable)
        for (int y = 0; y < tilePx; y++) for (int x = 0; x < tilePx; x++) {
            int si = ctx.idx(x, y);
            size_t di = (size_t)(ty + y) * S + (tx + x);
            out.albedo[di * 4 + 0] = (u8)tclamp(ctx.r[si] * 255.f, 0.f, 255.f);
            out.albedo[di * 4 + 1] = (u8)tclamp(ctx.g[si] * 255.f, 0.f, 255.f);
            out.albedo[di * 4 + 2] = (u8)tclamp(ctx.b[si] * 255.f, 0.f, 255.f);
            out.albedo[di * 4 + 3] = 255;
        }
        // height → normal (sobel), tile borders handled by wrapping within tile
        auto hAt = [&](int x, int y) {
            x = tclamp(x, 0, tilePx - 1); y = tclamp(y, 0, tilePx - 1);
            return ctx.hgt[ctx.idx(x, y)];
        };
        f32 strength = 2.6f;
        for (int y = 0; y < tilePx; y++) for (int x = 0; x < tilePx; x++) {
            f32 dx = (hAt(x + 1, y) - hAt(x - 1, y)) * strength;
            f32 dy = (hAt(x, y + 1) - hAt(x, y - 1)) * strength;
            Vec3 n = norm(Vec3(-dx, -dy, 1.f));
            size_t di = (size_t)(ty + y) * S + (tx + x);
            out.normal[di * 4 + 0] = (u8)tclamp((n.x * 0.5f + 0.5f) * 255.f, 0.f, 255.f);
            out.normal[di * 4 + 1] = (u8)tclamp((n.y * 0.5f + 0.5f) * 255.f, 0.f, 255.f);
            out.normal[di * 4 + 2] = (u8)tclamp((n.z * 0.5f + 0.5f) * 255.f, 0.f, 255.f);
            out.normal[di * 4 + 3] = 255;
        }
    }
    (void)rng;
    return true;
}

int matTileSlot(u8 mat, bool interior) {
    if (!g_baseSlotInit) {
        texAtlas();   // ensure built (fills g_baseSlot below via first build call)
        g_baseSlotInit = true;
    }
    int s = (mat < MAT_COUNT) ? g_baseSlot[mat] : 0;
    return interior ? s + 32 : s;
}

static TexAtlas* g_atlas = nullptr;
const TexAtlas& texAtlas() {
    if (!g_atlas) {
        g_atlas = new TexAtlas();
        buildTexAtlas(*g_atlas, 128);
    }
    return *g_atlas;
}
void freeTexAtlas() { delete g_atlas; g_atlas = nullptr; }

// ------------------------------------------------------------- sprites ------
static SpriteSheet* g_sheet = nullptr;
const SpriteSheet& spriteSheet() {
    if (!g_sheet) {
        g_sheet = new SpriteSheet();
        SpriteSheet& sh = *g_sheet;
        sh.rgba.assign((size_t)sh.w * sh.h * 4, 0);
        Rng rng(777);
        // frames:0..3 dust puffs,4..7 smoke,8..11 fire,12 sparks,13 shock,14..15 misc
        for (int f = 0; f < 16; f++) {
            int fx = (f % 4) * sh.frame, fy = (f / 4) * sh.frame;
            for (int y = 0; y < sh.frame; y++) for (int x = 0; x < sh.frame; x++) {
                f32 u = (x + 0.5f) / sh.frame * 2.f - 1.f;
                f32 v = (y + 0.5f) / sh.frame * 2.f - 1.f;
                f32 d = std::sqrt(u * u + v * v);
                u8 r = 255, g = 255, b = 255, a = 0;
                if (f < 4) { // dust: soft round, noisy edge
                    f32 n = fbm2(x * 0.09f, y * 0.09f, (u32)f * 13 + 1);
                    f32 alpha = tclamp((1.f - d) * 2.2f - n * 0.7f, 0.f, 1.f);
                    alpha *= alpha;
                    a = (u8)(alpha * 200);
                    f32 sh2 = 0.75f + n * 0.4f;
                    r = (u8)(210 * sh2); g = (u8)(205 * sh2); b = (u8)(195 * sh2);
                } else if (f < 8) { // smoke: darker, billowy
                    f32 n = fbm2(x * 0.07f + f, y * 0.07f, (u32)f * 31 + 5);
                    f32 alpha = tclamp((1.f - d) * 1.8f - n * 0.85f, 0.f, 1.f);
                    a = (u8)(alpha * alpha * 165);
                    f32 sh2 = 0.5f + n * 0.55f;
                    r = (u8)(70 * sh2 + 30); g = (u8)(68 * sh2 + 30); b = (u8)(66 * sh2 + 32);
                } else if (f < 12) { // fire: hot core
                    f32 n = fbm2(x * 0.11f, y * 0.11f - f, (u32)f * 7 + 3);
                    f32 heat = tclamp((1.f - d) * 1.7f * (0.7f + n * 0.6f), 0.f, 1.f);
                    a = (u8)(tmin(1.f, heat * 1.6f) * 235);
                    r = (u8)(tmin(255.f, heat * 480.f));
                    g = (u8)(tmin(255.f, heat * heat * 400.f));
                    b = (u8)(tmin(255.f, heat * heat * heat * 340.f));
                } else if (f == 12) { // spark point
                    f32 alpha = tclamp(1.f - d * 1.6f, 0.f, 1.f);
                    a = (u8)(alpha * alpha * 255);
                    r = 255; g = 245; b = 190;
                } else if (f == 13) { // shock ring
                    f32 ring = tclamp(1.f - std::fabs(d - 0.72f) * 7.f, 0.f, 1.f);
                    a = (u8)(ring * 140);
                    r = 255; g = 250; b = 235;
                } else if (f == 14) { // water splash
                    f32 n = fbm2(x * 0.13f, y * 0.13f, 9);
                    f32 alpha = tclamp((1.f - d) * 2.f - n * 0.6f, 0.f, 1.f);
                    a = (u8)(alpha * 190);
                    r = 200; g = 220; b = 235;
                } else { // debris chip
                    f32 sq = (std::fabs(u) < 0.55f && std::fabs(v) < 0.55f) ? 1.f : 0.f;
                    a = (u8)(sq * 255);
                    r = 255; g = 255; b = 255;
                }
                size_t di = ((size_t)(fy + y) * sh.w + (fx + x)) * 4;
                sh.rgba[di + 0] = r; sh.rgba[di + 1] = g; sh.rgba[di + 2] = b; sh.rgba[di + 3] = a;
            }
        }
    }
    return *g_sheet;
}
void freeSpriteSheet() { delete g_sheet; g_sheet = nullptr; }

} // namespace ft
