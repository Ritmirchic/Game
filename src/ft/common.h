// FRACTURE: TOTAL DESTRUCTION — common definitions
#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace ft {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

// ---------------------------------------------------------------- config ----
constexpr f32 VOX        = 0.25f;   // metres per voxel edge
constexpr int CH         = 16;      // voxels per chunk axis
constexpr f32 FIXED_DT   = 1.0f / 60.0f;
constexpr int MAX_LIGHTS = 8;

// ---------------------------------------------------------------- logging ---
inline void logf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    std::fprintf(stdout, "[ft] ");
    std::vfprintf(stdout, fmt, ap);
    std::fprintf(stdout, "\n");
    va_end(ap);
    std::fflush(stdout);
}
inline void errf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    std::fprintf(stderr, "[ft:ERR] ");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
    va_end(ap);
    std::fflush(stderr);
}
#define FT_ASSERT(cond, ...) do { if(!(cond)) { errf("ASSERT %s:%d %s", __FILE__, __LINE__, #cond); errf(__VA_ARGS__); std::abort(); } } while(0)

// ---------------------------------------------------------------- util ------
template <class T> inline T tmin(T a, T b) { return a < b ? a : b; }
template <class T> inline T tmax(T a, T b) { return a > b ? a : b; }
template <class T> inline T tclamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline f32 lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
inline f32 smoothstepf(f32 e0, f32 e1, f32 x) { f32 t = tclamp((x - e0) / (e1 - e0), 0.f, 1.f); return t * t * (3.f - 2.f * t); }
inline u32 hashU32(u32 x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }

// colour helper: pack/unpack RGB565
inline u16 rgb565(int r, int g, int b) {
    return (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
inline void unpack565(u16 c, int& r, int& g, int& b) {
    r = ((c >> 11) & 31) * 255 / 31;
    g = ((c >> 5) & 63) * 255 / 63;
    b = (c & 31) * 255 / 31;
}
struct RGB { u8 r, g, b; };

// ---------------------------------------------------------------- materials -
enum : u8 {
    MAT_AIR = 0,
    MAT_CONCRETE = 1,
    MAT_BRICK = 2,
    MAT_WOOD = 3,
    MAT_GLASS = 4,
    MAT_METAL = 5,
    MAT_REBAR = 6,        // reinforced concrete
    MAT_PLASTER = 7,
    MAT_SOIL = 8,
    MAT_ASPHALT = 9,
    MAT_STONE = 10,
    MAT_TILE = 11,        // roof / floor tiles
    MAT_SAND = 12,
    MAT_LEAF = 13,        // vegetation
    MAT_PLASTIC = 14,
    MAT_CARDBOARD = 15,
    MAT_FABRIC = 16,
    MAT_PAINTED = 17,     // painted steel (industrial)
    MAT_RUBBER = 18,
    MAT_DIRTROAD = 19,
    MAT_GRAVEL = 20,
    MAT_SNOW = 21,
    MAT_ROCK = 22,
    MAT_WATER = 23,       // non-solid, transparent
    MAT_SANDST = 24,      // sandstone / canyon rock
    MAT_CERAMIC = 25,     // bathroom/roof ceramic
    MAT_COUNT = 26,
};

enum MatFlags : u8 {
    MF_SOLID      = 1 << 0,   // collides / structural
    MF_GLASS      = 1 << 1,
    MF_METAL      = 1 << 2,
    MF_FLAMMABLE  = 1 << 3,
    MF_TRANSPARENT= 1 << 4,   // drawn in transparent pass (water)
    MF_ANCHOR     = 1 << 5,   // counts as structural anchor
    MF_SOFT       = 1 << 6,   // diggable soil / sand
    MF_NOSHADOW   = 1 << 7,
};

struct MatDef {
    const char* name;
    u8  flags;
    f32 density;      // kg per voxel-ish unit
    f32 hardness;     // resistance to blunt impact (sledge, debris)
    f32 cutRes;       // resistance to cutting (torch, drill)
    f32 blastRes;     // resistance to explosions
    u8  flammability; // 0..255 ignition likelihood / burn speed
    u8  baseR, baseG, baseB;
    u8  intR, intG, intB;    // freshly exposed interior colour
};

inline const MatDef& matDef(u8 id) {
    static const MatDef T[MAT_COUNT] = {
        //name          flags                                    dens  hard  cut   blast flam  base-col      interior-col
        {"air",         0,                                        0,0,0,0,       0, 0,0,0,        0,0,0},
        {"concrete",    MF_SOLID,                                 2.4f,1.6f,2.0f,1.8f, 0,  150,150,146,  118,118,116},
        {"brick",       MF_SOLID,                                 1.9f,1.2f,1.5f,1.2f, 0,  156,86,70,   132,72,60},
        {"wood",        MF_SOLID|MF_FLAMMABLE,                    0.7f,0.55f,0.8f,0.7f,200,150,108,66,  124,88,54},
        {"glass",       MF_SOLID|MF_GLASS,                        2.5f,0.12f,0.35f,0.25f,0, 196,222,230, 176,206,216},
        {"metal",       MF_SOLID|MF_METAL,                        7.8f,2.6f,1.6f,2.4f, 0,  132,138,146, 108,112,120},
        {"rebar",       MF_SOLID|MF_METAL,                        3.0f,3.4f,3.0f,3.6f, 0,  134,134,132, 104,104,104},
        {"plaster",     MF_SOLID,                                 1.2f,0.4f,0.7f,0.5f, 40, 224,218,206, 206,200,188},
        {"soil",        MF_SOLID|MF_SOFT,                         1.5f,0.35f,0.4f,0.35f,0, 112,84,58,   96,72,50},
        {"asphalt",     MF_SOLID,                                 2.3f,1.5f,1.7f,1.3f, 0,  74,76,80,    60,60,64},
        {"stone",       MF_SOLID,                                 2.6f,2.2f,2.4f,2.2f, 0,  142,140,134, 116,114,110},
        {"tile",        MF_SOLID,                                 2.0f,0.9f,1.2f,0.8f, 0,  172,96,76,   196,196,196},
        {"sand",        MF_SOLID|MF_SOFT,                         1.6f,0.3f,0.35f,0.3f,0, 206,182,132, 188,164,116},
        {"leaf",        MF_FLAMMABLE,                             0.2f,0.15f,0.3f,0.2f,240, 84,124,58,  70,104,50},
        {"plastic",     MF_SOLID|MF_FLAMMABLE,                    0.9f,0.4f,0.5f,0.5f,170, 198,196,190, 176,174,168},
        {"cardboard",   MF_SOLID|MF_FLAMMABLE,                    0.4f,0.25f,0.5f,0.25f,230,176,144,104,156,126,90},
        {"fabric",      MF_FLAMMABLE,                             0.3f,0.2f,0.4f,0.2f, 220,120,110,140, 104,96,124},
        {"paintedsteel",MF_SOLID|MF_METAL,                        7.8f,2.5f,1.5f,2.3f, 0,  92,110,140,  130,136,144},
        {"rubber",      MF_SOLID|MF_FLAMMABLE,                    1.1f,0.6f,0.9f,0.5f, 120, 44,44,48,   36,36,40},
        {"dirtroad",    MF_SOLID|MF_SOFT,                         1.5f,0.35f,0.4f,0.35f,0, 122,98,72,   104,84,60},
        {"gravel",      MF_SOLID|MF_SOFT,                         1.7f,0.5f,0.5f,0.45f,0, 134,132,126, 118,116,110},
        {"snow",        MF_SOLID|MF_SOFT,                         0.5f,0.2f,0.25f,0.2f,0, 236,240,246,  222,228,238},
        {"rock",        MF_SOLID,                                 2.5f,2.4f,2.6f,2.4f, 0,  128,124,118, 106,102,96},
        {"water",       MF_TRANSPARENT,                           1.0f,0,0,0,          0,  52,104,150,  52,104,150},
        {"sandstone",   MF_SOLID,                                 2.2f,1.8f,2.0f,1.8f, 0,  186,142,98,  166,124,84},
        {"ceramic",     MF_SOLID,                                 2.1f,0.8f,1.0f,0.6f, 0,  226,226,222, 206,206,202},
    };
    return T[id < MAT_COUNT ? id : MAT_AIR];
}
inline bool matSolid(u8 id) { return (matDef(id).flags & MF_SOLID) != 0; }

// voxel flags (per-cell)
enum : u8 {
    VF_FRESH   = 1 << 0,  // recently exposed interior surface
    VF_BURNING = 1 << 1,
    VF_IGNITED = 1 << 2,  // burnt past ignition, will char out
    VF_FANCHOR = 1 << 3,  // hand-anchored (counts as support source)
    VF_WET     = 1 << 4,  // rain wetness
    VF_USER    = 1 << 5,  // placed by player (plank/repair)
};

struct Voxel { u8 mat; u8 flags; u16 col; };
static_assert(sizeof(Voxel) == 4, "voxel is4 bytes");

} // namespace ft
