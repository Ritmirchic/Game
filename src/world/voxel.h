// FRACTURE — voxel world: materials, chunk storage, greedy meshing, destruction
#pragma once
#include "core/math.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

static const int CH = 32;          // chunk edge (voxels)
static const int CHV = CH * CH * CH;
static float const VOX = 0.25f;    // metres per voxel

// ---------------- materials ----------------
enum Mat : uint8_t {
    M_AIR = 0,
    M_CONCRETE, M_CONCRETE_DARK, M_REINFORCED, M_BRICK, M_PLASTER, M_STONE,
    M_WOOD, M_WOOD_DARK, M_PLANK, M_METAL, M_METAL_DARK, M_RUST, M_GLASS,
    M_ASPHALT, M_GRAVEL, M_SOIL, M_SAND, M_SANDBAG, M_TILE, M_ROOF,
    M_FOLIAGE, M_TRUNK, M_WATER, M_RUBBER, M_PAINT_BLUE, M_PAINT_RED,
    M_PAINT_YELLOW, M_LAMP, M_SKYLIGHT, M_CONTAINER_BLUE, M_CONTAINER_RED,
    M_CONTAINER_GREEN, M_CONTAINER_ORANGE, M_GOLD, M_WHITE_PANEL, M_GRATE,
    M_COBBLE, M_MUD, M_GRASS, M_GRAVEL_PATH, M_SANDSTONE, M_ROCK_RED, M_COUNT
};

enum DebrisKind : uint8_t { DB_CONCRETE = 0, DB_BRICK, DB_WOOD, DB_GLASS, DB_METAL, DB_DIRT, DB_STONE, DB_PLASTIC, DB_LEAF };

enum PatternId : uint8_t {
    PAT_NONE = 0, PAT_BRICK, PAT_CONCRETE, PAT_WOOD, PAT_PLANK, PAT_METAL, PAT_GRATE,
    PAT_ASPHALT, PAT_TILES, PAT_SOIL, PAT_ROOF, PAT_CARPET, PAT_CONTAINER, PAT_ROCK, PAT_GLASS
};

struct Material {
    const char* name;
    vec3 color;            // base albedo (sRGB-ish, shader treats as linear-ish)
    vec3 interior;         // raw interior colour when freshly exposed
    float roughness;
    float metallic;
    float resistance;      // damage needed to break one voxel
    bool flammable;
    bool solid;
    bool transparent;
    float density;         // kg/m^3 (approx)
    DebrisKind debris;
    PatternId pattern;
    int footstep;          // 0 concrete,1 wood,2 metal,3 dirt,4 grass,5 glass
    float spark;           // 0..1 metal spark tendency
};

const Material& material(uint8_t m);
const char* matName(uint8_t m);
static inline bool matSolid(uint8_t m) { return m != M_AIR && m != M_WATER; }
static inline bool matOpaque(uint8_t m) { return m != M_AIR && m != M_WATER; }
static inline bool matTransparent(uint8_t m) { return m == M_WATER || m == M_GLASS || m == M_FOLIAGE; }

// ---------------- vertex / mesh ----------------
#pragma pack(push, 1)
struct VoxVert {
    float x, y, z;         // position in metres (world or local)
    uint8_t n;             // 0..5 normal index (+X,-X,+Y,-Y,+Z,-Z)
    uint8_t mat;
    uint8_t ao;            // 0 (dark) .. 3 (open)
    uint8_t flags;         // bit0 fresh interior, bit1 water, bit2 emissive
};
#pragma pack(pop)
static_assert(sizeof(VoxVert) == 16, "vertex must be 16 bytes");

static inline vec3 normalOf(uint8_t n) {
    switch (n) {
        case 0: return {1, 0, 0};
        case 1: return {-1, 0, 0};
        case 2: return {0, 1, 0};
        case 3: return {0, -1, 0};
        case 4: return {0, 0, 1};
        default: return {0, 0, -1};
    }
}

struct MeshCPU {
    std::vector<VoxVert> verts;
    std::vector<uint32_t> idx;
    AABB bounds;
    bool empty() const { return idx.empty(); }
};

struct ChunkMesh : MeshCPU {
    std::vector<uint32_t> idxTrans;    // water / glass / foliage (blended pass)
    uint32_t vbo = 0, ibo = 0, iboTrans = 0, vao = 0;
    int indexCount = 0, indexCountTrans = 0;
    bool gpuDirty = true;
    bool gpuReady = false;
    void clear() { verts.clear(); idx.clear(); idxTrans.clear(); gpuDirty = true; indexCount = indexCountTrans = 0; }
};

struct Chunk {
    uint8_t v[CHV];
    bool dirty = true;
    bool allocated = true;
    Chunk() { std::memset(v, 0, sizeof(v)); }
    int index(int x, int y, int z) const { return x + CH * (z + CH * y); }
};

struct RayHit {
    bool hit = false;
    float t = 0;
    vec3 pos;
    vec3 normal{0, 1, 0};
    ivec3 cell;
    uint8_t mat = M_AIR;
};

struct VoxelKeyHash {
    size_t operator()(uint64_t k) const { return (size_t)hash32((uint32_t)k ^ (uint32_t)(k >> 32) * 2654435761u); }
};

class World {
public:
    int sx = 0, sy = 0, sz = 0;       // voxel dimensions
    std::unordered_map<uint64_t, Chunk*> chunks;
    std::unordered_set<uint64_t, VoxelKeyHash> fresh;    // freshly exposed voxels
    std::unordered_map<uint64_t, uint8_t, VoxelKeyHash> damage;
    std::vector<uint64_t> dirtyChunks;
    std::unordered_map<uint64_t, ChunkMesh> meshes;      // chunk meshes (CPU+GPU)
    std::unordered_set<uint64_t> modifiedChunks;         // chunks altered during gameplay (saved)
    int groundVox = 12;               // nominal terrain top
    uint64_t modifications = 0;       // counter of edits (for save dirty flag)
    int allocChunks = 0;

    void init(int sx_, int sy_, int sz_);
    void clear();

    static uint64_t keyOf(int x, int y, int z) {
        return ((uint64_t)(uint32_t)y << 42) | ((uint64_t)(uint32_t)z << 21) | (uint64_t)(uint32_t)x;
    }
    static ivec3 unkey(uint64_t k) {
        return {(int)(k & 0x1FFFFF), (int)((k >> 21) & 0x1FFFFF), (int)((k >> 42) & 0x3FFFFF)};
    }
    static uint64_t chunkKey(int cx, int cy, int cz) {
        return ((uint64_t)(uint32_t)cz << 42) | ((uint64_t)(uint32_t)cy << 21) | (uint64_t)(uint32_t)cx;
    }
    static ivec3 unchunkKey(uint64_t k) {
        return {(int)(k & 0x1FFFFF), (int)((k >> 21) & 0x1FFFFF), (int)((k >> 42) & 0x3FFFFF)};
    }

    inline bool inBounds(int x, int y, int z) const {
        return (unsigned)x < (unsigned)sx && (unsigned)y < (unsigned)sy && (unsigned)z < (unsigned)sz;
    }
    inline uint8_t at(int x, int y, int z) const {
        if (!inBounds(x, y, z)) return M_AIR;
        uint64_t ck = chunkKey(x >> 5, y >> 5, z >> 5);
        auto it = chunks.find(ck);
        if (it == chunks.end()) return M_AIR;
        return it->second->v[(x & 31) + CH * ((z & 31) + CH * (y & 31))];
    }
    inline uint8_t at(const ivec3& c) const { return at(c.x, c.y, c.z); }
    // fast variant that assumes chunk exists (caller provides)
    static inline uint8_t chAt(const Chunk* c, int x, int y, int z) { return c->v[(x & 31) + CH * ((z & 31) + CH * (y & 31))]; }

    void setFast(int x, int y, int z, uint8_t m);   // sets + marks dirty
    void setRaw(int x, int y, int z, uint8_t m);    // sets without dirty (bulk gen)
    bool eraseFast(int x, int y, int z);
    Chunk* getChunk(int cx, int cy, int cz);
    Chunk* getOrCreateChunk(int cx, int cy, int cz);

    bool solidAt(int x, int y, int z) const { return matSolid(at(x, y, z)); }
    bool solidAtM(const vec3& metres) const { return solidAtV(metres * (1.0f / VOX)); }
    bool solidAtV(const vec3& vox) const {
        return solidAt((int)std::floor(vox.x), (int)std::floor(vox.y), (int)std::floor(vox.z));
    }
    uint8_t matAtM(const vec3& metres) const {
        vec3 v = metres * (1.0f / VOX);
        return at((int)std::floor(v.x), (int)std::floor(v.y), (int)std::floor(v.z));
    }

    // structural / freshness bookkeeping
    void markFreshAround(int x, int y, int z);
    bool isFresh(int x, int y, int z) const { return fresh.find(keyOf(x, y, z)) != fresh.end(); }
    uint8_t damageAt(int x, int y, int z) const {
        auto it = damage.find(keyOf(x, y, z));
        return it == damage.end() ? 0 : it->second;
    }
    void addDamage(int x, int y, int z, uint8_t d);
    void markChunkDirty(int x, int y, int z);

    // meshing
    void buildChunkMesh(int cx, int cy, int cz);
    void rebuildDirtyMeshes(int budgetChunks = 4);
    int pendingDirty() const { return (int)dirtyChunks.size(); }

    // queries
    RayHit raycast(const vec3& originM, const vec3& dir, float maxDistM, bool hitWater = false) const;
    bool aabbSolid(const AABB& boxM) const;
    float groundHeightM(float x, float z) const;      // top of highest solid

    // destruction
    struct CarveParams {
        vec3 centerM;
        float radiusM = 0.6f;
        float power = 10.0f;
        float damageMul = 1.0f;
        bool structural = true;
        bool ignite = false;
    };
    struct CarveResult {
        int removed = 0;
        int damaged = 0;
        AABB box;
        bool any = false;
        int matCounts[M_COUNT] = {0};
    };
    CarveResult carve(const CarveParams& p);
    void carveVoxelSphere(const vec3& centerVox, float radiusVox, float power, float damageMul, CarveResult* res, AABB* touched);

    // fill helpers used by map generation
    void box(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t m);
    void hollowBox(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t wall, uint8_t floorMat, uint8_t air = M_AIR);
    void sphere(const vec3& c, float r, uint8_t m, bool hollow = false);
    void cylY(int cx, int cz, int y0, int y1, float r, uint8_t m, bool hollow = false);
    void line3(const vec3& a, const vec3& b, float radius, uint8_t m);
    void disk(int cx, int cy, int cz, float r, uint8_t m, int axis = 1);

    size_t memoryEstimate() const;
    void markAllDirty();

private:
    void buildMeshFor(Chunk* c, ChunkMesh& out, const uint8_t* padded);
};

// Serialization of modified chunk data (RLE)
void serializeWorld(const World& w, std::vector<uint8_t>& out);
bool deserializeWorld(World& w, const uint8_t* data, size_t size);

// Structural support analysis (coarse cells)
struct SupportCell { int x, y, z; };
struct FloatingCluster {
    std::vector<SupportCell> cells;
    AABB boundsM;
};
// region in voxels; returns clusters that are not connected to ground / region border
void analyzeSupport(const World& w, const ivec3& rmin, const ivec3& rmax, std::vector<FloatingCluster>& out);
