#include "world/voxel.h"
#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------- materials
static Material g_mats[M_COUNT];
static bool g_matsInit = false;

static void initMats() {
    if (g_matsInit) return;
    g_matsInit = true;
    auto S = [](Material& m, const char* name, vec3 col, vec3 interior, float rough, float metal,
                float res, bool flam, bool solid, bool trans, float dens, DebrisKind db, PatternId pat,
                int foot, float spark) {
        m.name = name; m.color = col; m.interior = interior; m.roughness = rough; m.metallic = metal;
        m.resistance = res; m.flammable = flam; m.solid = solid; m.transparent = trans; m.density = dens;
        m.debris = db; m.pattern = pat; m.footstep = foot; m.spark = spark;
    };
    S(g_mats[M_AIR], "air", {0,0,0}, {0,0,0}, 1, 0, 0, false, false, true, 0, DB_CONCRETE, PAT_NONE, 0, 0);
    S(g_mats[M_CONCRETE], "concrete", {0.63f,0.62f,0.59f}, {0.46f,0.45f,0.43f}, 0.92f, 0.0f, 15, false, true, false, 2300, DB_CONCRETE, PAT_CONCRETE, 0, 0);
    S(g_mats[M_CONCRETE_DARK], "dark concrete", {0.40f,0.41f,0.42f}, {0.30f,0.30f,0.31f}, 0.90f, 0.0f, 18, false, true, false, 2350, DB_CONCRETE, PAT_CONCRETE, 0, 0);
    S(g_mats[M_REINFORCED], "reinforced concrete", {0.56f,0.57f,0.57f}, {0.38f,0.38f,0.39f}, 0.85f, 0.25f, 1600, false, true, false, 2500, DB_CONCRETE, PAT_CONCRETE, 0, 0.1f);
    S(g_mats[M_BRICK], "brick", {0.60f,0.28f,0.21f}, {0.42f,0.23f,0.18f}, 0.88f, 0.0f, 9, false, true, false, 1900, DB_BRICK, PAT_BRICK, 0, 0);
    S(g_mats[M_PLASTER], "plaster", {0.84f,0.82f,0.77f}, {0.70f,0.68f,0.63f}, 0.95f, 0.0f, 4, false, true, false, 1200, DB_CONCRETE, PAT_CONCRETE, 0, 0);
    S(g_mats[M_STONE], "stone", {0.52f,0.52f,0.50f}, {0.38f,0.38f,0.37f}, 0.88f, 0.0f, 120, false, true, false, 2600, DB_STONE, PAT_ROCK, 0, 0);
    S(g_mats[M_WOOD], "wood", {0.52f,0.35f,0.19f}, {0.62f,0.45f,0.26f}, 0.82f, 0.0f, 6, true, true, false, 650, DB_WOOD, PAT_WOOD, 1, 0);
    S(g_mats[M_WOOD_DARK], "dark wood", {0.32f,0.21f,0.13f}, {0.45f,0.31f,0.19f}, 0.80f, 0.0f, 7, true, true, false, 700, DB_WOOD, PAT_WOOD, 1, 0);
    S(g_mats[M_PLANK], "plank", {0.66f,0.51f,0.33f}, {0.72f,0.57f,0.38f}, 0.85f, 0.0f, 5, true, true, false, 550, DB_WOOD, PAT_PLANK, 1, 0);
    S(g_mats[M_METAL], "metal", {0.63f,0.65f,0.68f}, {0.48f,0.50f,0.53f}, 0.35f, 0.85f, 900, false, true, false, 7800, DB_METAL, PAT_METAL, 2, 0.8f);
    S(g_mats[M_METAL_DARK], "dark metal", {0.33f,0.35f,0.38f}, {0.26f,0.28f,0.31f}, 0.45f, 0.80f, 1000, false, true, false, 7800, DB_METAL, PAT_METAL, 2, 0.8f);
    S(g_mats[M_RUST], "rusted steel", {0.44f,0.26f,0.15f}, {0.36f,0.20f,0.11f}, 0.90f, 0.35f, 24, false, true, false, 6900, DB_METAL, PAT_METAL, 2, 0.5f);
    S(g_mats[M_GLASS], "glass", {0.68f,0.80f,0.86f}, {0.82f,0.90f,0.94f}, 0.08f, 0.15f, 1, false, true, true, 2500, DB_GLASS, PAT_GLASS, 5, 0);
    S(g_mats[M_ASPHALT], "asphalt", {0.21f,0.21f,0.22f}, {0.16f,0.16f,0.17f}, 0.94f, 0.0f, 11, false, true, false, 2400, DB_CONCRETE, PAT_ASPHALT, 0, 0);
    S(g_mats[M_GRAVEL], "gravel", {0.44f,0.43f,0.40f}, {0.36f,0.35f,0.33f}, 0.96f, 0.0f, 5, false, true, false, 1800, DB_DIRT, PAT_SOIL, 3, 0);
    S(g_mats[M_SOIL], "soil", {0.34f,0.25f,0.16f}, {0.29f,0.21f,0.13f}, 0.98f, 0.0f, 3, false, true, false, 1500, DB_DIRT, PAT_SOIL, 3, 0);
    S(g_mats[M_SAND], "sand", {0.78f,0.70f,0.50f}, {0.70f,0.62f,0.44f}, 0.97f, 0.0f, 2, false, true, false, 1600, DB_DIRT, PAT_SOIL, 3, 0);
    S(g_mats[M_SANDBAG], "sandbag", {0.58f,0.54f,0.38f}, {0.50f,0.46f,0.32f}, 0.95f, 0.0f, 4, false, true, false, 1400, DB_DIRT, PAT_SOIL, 3, 0);
    S(g_mats[M_TILE], "tile", {0.79f,0.79f,0.77f}, {0.62f,0.62f,0.60f}, 0.55f, 0.0f, 7, false, true, false, 2100, DB_STONE, PAT_TILES, 0, 0);
    S(g_mats[M_ROOF], "roof tile", {0.38f,0.26f,0.23f}, {0.30f,0.21f,0.18f}, 0.80f, 0.0f, 6, false, true, false, 1800, DB_BRICK, PAT_ROOF, 0, 0);
    S(g_mats[M_FOLIAGE], "foliage", {0.20f,0.41f,0.15f}, {0.16f,0.33f,0.12f}, 0.85f, 0.0f, 1, true, true, true, 200, DB_LEAF, PAT_NONE, 4, 0);
    S(g_mats[M_TRUNK], "trunk", {0.30f,0.23f,0.15f}, {0.40f,0.31f,0.20f}, 0.92f, 0.0f, 8, true, true, false, 700, DB_WOOD, PAT_WOOD, 1, 0);
    S(g_mats[M_WATER], "water", {0.13f,0.30f,0.40f}, {0.10f,0.24f,0.33f}, 0.05f, 0.2f, 0, false, false, true, 1000, DB_CONCRETE, PAT_NONE, 0, 0);
    S(g_mats[M_RUBBER], "rubber", {0.10f,0.10f,0.11f}, {0.08f,0.08f,0.09f}, 0.92f, 0.0f, 5, true, true, false, 1100, DB_PLASTIC, PAT_NONE, 1, 0);
    S(g_mats[M_PAINT_BLUE], "blue paint", {0.18f,0.34f,0.58f}, {0.50f,0.50f,0.50f}, 0.55f, 0.0f, 11, false, true, false, 2000, DB_CONCRETE, PAT_CONCRETE, 0, 0);
    S(g_mats[M_PAINT_RED], "red paint", {0.62f,0.15f,0.14f}, {0.50f,0.50f,0.50f}, 0.55f, 0.0f, 11, false, true, false, 2000, DB_CONCRETE, PAT_CONCRETE, 0, 0);
    S(g_mats[M_PAINT_YELLOW], "yellow paint", {0.82f,0.65f,0.15f}, {0.45f,0.45f,0.45f}, 0.55f, 0.0f, 10, false, true, false, 2000, DB_CONCRETE, PAT_METAL, 2, 0.2f);
    S(g_mats[M_LAMP], "lamp", {1.00f,0.92f,0.72f}, {0.90f,0.85f,0.70f}, 0.20f, 0.0f, 2, false, true, false, 2500, DB_GLASS, PAT_NONE, 5, 0);
    S(g_mats[M_SKYLIGHT], "skylight", {0.72f,0.85f,0.92f}, {0.80f,0.90f,0.95f}, 0.10f, 0.1f, 3, false, true, true, 2500, DB_GLASS, PAT_GLASS, 5, 0);
    S(g_mats[M_CONTAINER_BLUE], "container blue", {0.16f,0.31f,0.45f}, {0.55f,0.42f,0.25f}, 0.70f, 0.45f, 22, false, true, false, 3000, DB_METAL, PAT_CONTAINER, 2, 0.4f);
    S(g_mats[M_CONTAINER_RED], "container red", {0.48f,0.17f,0.13f}, {0.55f,0.42f,0.25f}, 0.70f, 0.45f, 22, false, true, false, 3000, DB_METAL, PAT_CONTAINER, 2, 0.4f);
    S(g_mats[M_CONTAINER_GREEN], "container green", {0.18f,0.34f,0.24f}, {0.55f,0.42f,0.25f}, 0.70f, 0.45f, 22, false, true, false, 3000, DB_METAL, PAT_CONTAINER, 2, 0.4f);
    S(g_mats[M_CONTAINER_ORANGE], "container orange", {0.60f,0.32f,0.12f}, {0.55f,0.42f,0.25f}, 0.70f, 0.45f, 22, false, true, false, 3000, DB_METAL, PAT_CONTAINER, 2, 0.4f);
    S(g_mats[M_GOLD], "brass", {0.80f,0.63f,0.24f}, {0.65f,0.50f,0.20f}, 0.28f, 1.0f, 1400, false, true, false, 8500, DB_METAL, PAT_METAL, 2, 0.6f);
    S(g_mats[M_WHITE_PANEL], "panel", {0.82f,0.83f,0.82f}, {0.60f,0.61f,0.60f}, 0.42f, 0.15f, 14, false, true, false, 2000, DB_METAL, PAT_METAL, 2, 0.2f);
    S(g_mats[M_GRATE], "steel grating", {0.42f,0.44f,0.47f}, {0.30f,0.31f,0.33f}, 0.55f, 0.85f, 700, false, true, false, 5000, DB_METAL, PAT_GRATE, 2, 0.7f);
    S(g_mats[M_COBBLE], "cobble", {0.46f,0.46f,0.44f}, {0.34f,0.34f,0.33f}, 0.92f, 0.0f, 16, false, true, false, 2400, DB_STONE, PAT_TILES, 0, 0);
    S(g_mats[M_MUD], "mud", {0.26f,0.21f,0.15f}, {0.22f,0.18f,0.13f}, 0.98f, 0.0f, 2, false, true, false, 1700, DB_DIRT, PAT_SOIL, 3, 0);
    S(g_mats[M_GRASS], "grass", {0.20f,0.36f,0.14f}, {0.16f,0.26f,0.11f}, 0.96f, 0.0f, 2, true, true, false, 1200, DB_LEAF, PAT_SOIL, 4, 0);
    S(g_mats[M_GRAVEL_PATH], "gravel path", {0.52f,0.49f,0.44f}, {0.42f,0.40f,0.36f}, 0.94f, 0.0f, 3, false, true, false, 1800, DB_STONE, PAT_ASPHALT, 3, 0);
    S(g_mats[M_SANDSTONE], "sandstone", {0.62f,0.40f,0.24f}, {0.48f,0.30f,0.18f}, 0.90f, 0.0f, 24, false, true, false, 2200, DB_STONE, PAT_ROCK, 0, 0);
    S(g_mats[M_ROCK_RED], "red rock", {0.44f,0.24f,0.15f}, {0.32f,0.18f,0.11f}, 0.92f, 0.0f, 38, false, true, false, 2500, DB_STONE, PAT_ROCK, 0, 0);
}

const Material& material(uint8_t m) { initMats(); return g_mats[m < M_COUNT ? m : 0]; }
const char* matName(uint8_t m) { initMats(); return g_mats[m < M_COUNT ? m : 0].name; }

// ---------------------------------------------------------------- world
void World::init(int sx_, int sy_, int sz_) {
    clear();
    sx = sx_; sy = sy_; sz = sz_;
}

void World::clear() {
    for (auto& kv : chunks) delete kv.second;
    chunks.clear();
    meshes.clear();
    fresh.clear();
    damage.clear();
    dirtyChunks.clear();
    modifications = 0;
    allocChunks = 0;
    modifiedChunks.clear();
    // keep dims
}

Chunk* World::getChunk(int cx, int cy, int cz) {
    auto it = chunks.find(chunkKey(cx, cy, cz));
    return it == chunks.end() ? nullptr : it->second;
}

Chunk* World::getOrCreateChunk(int cx, int cy, int cz) {
    uint64_t k = chunkKey(cx, cy, cz);
    auto it = chunks.find(k);
    if (it != chunks.end()) return it->second;
    if (cx < 0 || cy < 0 || cz < 0 || (cx + 1) * CH > sx || (cy + 1) * CH > sy || (cz + 1) * CH > sz) return nullptr;
    Chunk* c = new Chunk();
    chunks[k] = c;
    allocChunks++;
    meshes[k] = ChunkMesh();
    c->dirty = true;
    dirtyChunks.push_back(k);
    return c;
}

void World::setRaw(int x, int y, int z, uint8_t m) {
    if (!inBounds(x, y, z)) return;
    // single-entry chunk cache: map generation writes are highly coherent
    static Chunk* s_cache = nullptr;
    static int scx = -1, scy = -1, scz = -1;
    int cx = x >> 5, cy = y >> 5, cz = z >> 5;
    Chunk* c;
    if (s_cache && cx == scx && cy == scy && cz == scz) {
        c = s_cache;
    } else {
        c = getOrCreateChunk(cx, cy, cz);
        s_cache = c;
        scx = cx; scy = cy; scz = cz;
    }
    if (!c) return;
    int idx = (x & 31) + CH * ((z & 31) + CH * (y & 31));
    c->v[idx] = m;
}

void World::markChunkDirty(int x, int y, int z) {
    int cx = x >> 5, cy = y >> 5, cz = z >> 5;
    // mark this chunk and neighbours if the voxel sits on a chunk border
    int lx = x & 31, ly = y & 31, lz = z & 31;
    int x0 = cx, x1 = cx, y0 = cy, y1 = cy, z0 = cz, z1 = cz;
    if (lx == 0) x0--; else if (lx == 31) x1++;
    if (ly == 0) y0--; else if (ly == 31) y1++;
    if (lz == 0) z0--; else if (lz == 31) z1++;
    for (int i = x0; i <= x1; i++)
        for (int j = y0; j <= y1; j++)
            for (int k = z0; k <= z1; k++) {
                uint64_t key = chunkKey(i, j, k);
                auto it = chunks.find(key);
                if (it == chunks.end()) continue;
                if (!it->second->dirty) {
                    it->second->dirty = true;
                    dirtyChunks.push_back(key);
                }
            }
    modifications++;
    if (chunks.find(chunkKey(cx, cy, cz)) != chunks.end()) modifiedChunks.insert(chunkKey(cx, cy, cz));
    if (x0 != cx || x1 != cx) { /* neighbouring chunk modified indirectly */ }
}

void World::setFast(int x, int y, int z, uint8_t m) {
    if (!inBounds(x, y, z)) return;
    Chunk* c = getOrCreateChunk(x >> 5, y >> 5, z >> 5);
    if (!c) return;
    int idx = (x & 31) + CH * ((z & 31) + CH * (y & 31));
    if (c->v[idx] == m) return;
    c->v[idx] = m;
    markChunkDirty(x, y, z);
    if (m == M_AIR) markFreshAround(x, y, z);
}

bool World::eraseFast(int x, int y, int z) {
    if (!inBounds(x, y, z)) return false;
    uint8_t m = at(x, y, z);
    if (m == M_AIR) return false;
    setFast(x, y, z, M_AIR);
    return true;
}

void World::markFreshAround(int x, int y, int z) {
    static const int off[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (auto& o : off) {
        int nx = x + o[0], ny = y + o[1], nz = z + o[2];
        if (inBounds(nx, ny, nz) && matSolid(at(nx, ny, nz))) {
            if (fresh.size() < 400000) fresh.insert(keyOf(nx, ny, nz));
        }
    }
}

void World::addDamage(int x, int y, int z, uint8_t d) {
    if (!inBounds(x, y, z)) return;
    uint64_t k = keyOf(x, y, z);
    int cur = damageAt(x, y, z) + d;
    damage[k] = (uint8_t)std::min(255, cur);
}

void World::box(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t m) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    if (z0 > z1) std::swap(z0, z1);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) setRaw(x, y, z, m);
}

void World::hollowBox(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t wall, uint8_t floorMat, uint8_t air) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    if (z0 > z1) std::swap(z0, z1);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                bool edge = (x == x0 || x == x1 || z == z0 || z == z1 || y == y0 || y == y1);
                if (edge) setRaw(x, y, z, (y == y0 && floorMat != M_AIR) ? floorMat : wall);
                else setRaw(x, y, z, air);
            }
}

void World::sphere(const vec3& c, float r, uint8_t m, bool hollow) {
    int x0 = (int)std::floor(c.x - r), x1 = (int)std::ceil(c.x + r);
    int y0 = (int)std::floor(c.y - r), y1 = (int)std::ceil(c.y + r);
    int z0 = (int)std::floor(c.z - r), z1 = (int)std::ceil(c.z + r);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                float d = std::sqrt((x + 0.5f - c.x) * (x + 0.5f - c.x) + (y + 0.5f - c.y) * (y + 0.5f - c.y) + (z + 0.5f - c.z) * (z + 0.5f - c.z));
                if (d <= r && (!hollow || d > r - 1.2f)) setRaw(x, y, z, m);
            }
}

void World::cylY(int cx, int cz, int y0, int y1, float r, uint8_t m, bool hollow) {
    for (int y = y0; y <= y1; y++) {
        int rr = (int)std::ceil(r);
        for (int z = cz - rr; z <= cz + rr; z++)
            for (int x = cx - rr; x <= cx + rr; x++) {
                float d = std::sqrt((float)((x - cx) * (x - cx) + (z - cz) * (z - cz)));
                if (d <= r && (!hollow || d > r - 1.2f)) setRaw(x, y, z, m);
            }
    }
}

void World::line3(const vec3& a, const vec3& b, float radius, uint8_t m) {
    vec3 d = b - a;
    float len = d.len();
    if (len < 1e-4f) { sphere(a, radius, m); return; }
    int steps = (int)(len * 2.0f) + 1;
    for (int i = 0; i <= steps; i++) {
        vec3 p = a + d * ((float)i / steps);
        sphere(p, radius, m);
    }
}

void World::disk(int cx, int cy, int cz, float r, uint8_t m, int axis) {
    int rr = (int)std::ceil(r);
    for (int a = -rr; a <= rr; a++)
        for (int b = -rr; b <= rr; b++) {
            if (a * a + b * b > r * r) continue;
            if (axis == 1) setRaw(cx + a, cy, cz + b, m);
            else if (axis == 0) setRaw(cx, cy + a, cz + b, m);
            else setRaw(cx + a, cy + b, cz, m);
        }
}

// ---------------------------------------------------------------- meshing
static inline int pidx(int x, int y, int z) { return (x + 1) + 34 * ((z + 1) + 34 * (y + 1)); }

void World::buildChunkMesh(int cx, int cy, int cz) {
    uint64_t ck = chunkKey(cx, cy, cz);
    auto cit = chunks.find(ck);
    if (cit == chunks.end()) {
        meshes[ck].clear();
        return;
    }
    Chunk* c = cit->second;
    ChunkMesh& out = meshes[ck];

    // padded 34^3 copy with 1-voxel border
    static thread_local std::vector<uint8_t> padded;
    padded.assign(34 * 34 * 34, 0);
    for (int y = 0; y < CH; y++)
        for (int z = 0; z < CH; z++)
            for (int x = 0; x < CH; x++)
                padded[pidx(x, y, z)] = c->v[x + CH * (z + CH * y)];
    // borders from neighbour chunks
    int bx = cx * CH, by = cy * CH, bz = cz * CH;
    for (int y = -1; y <= CH; y++)
        for (int z = -1; z <= CH; z++)
            for (int x = -1; x <= CH; x++) {
                int lx = x, ly = y, lz = z;
                if (x >= 0 && x < CH && y >= 0 && y < CH && z >= 0 && z < CH) continue;
                uint8_t m = at(bx + lx, by + ly, bz + lz);
                padded[pidx(x, y, z)] = m;
            }

    out.verts.clear();
    out.idx.clear();
    out.idxTrans.clear();

    // greedy meshing per axis & direction
    for (int axis = 0; axis < 3; axis++) {
        int u = (axis + 1) % 3, v = (axis + 2) % 3;
        for (int dir = 0; dir < 2; dir++) {
            int sgn = dir == 0 ? 1 : -1;
            uint8_t nrm = (uint8_t)(axis * 2 + (sgn > 0 ? 0 : 1));
            static thread_local std::vector<uint32_t> mask;
            for (int s = 0; s < CH; s++) {
                mask.assign(CH * CH, 0);
                bool anyMask = false;
                int pos[3];
                for (int j = 0; j < CH; j++) {
                    for (int i = 0; i < CH; i++) {
                        int co[3];
                        co[axis] = s; co[u] = i; co[v] = j;
                        uint8_t self = padded[pidx(co[0], co[1], co[2])];
                        if (!matSolid(self)) continue;
                        int n[3] = {co[0], co[1], co[2]};
                        n[axis] += sgn;
                        uint8_t nb = padded[pidx(n[0], n[1], n[2])];
                        bool emit = !matSolid(nb);
                        if (self == M_WATER && nb == M_WATER) emit = false;
                        if (!emit) continue;
                        // ambient occlusion at 4 corners
                        uint32_t aos = 0;
                        for (int cc = 0; cc < 4; cc++) {
                            int cu = (cc == 1 || cc == 2) ? 1 : -1;   // +u side
                            int cv = (cc >= 2) ? 1 : -1;             // +v side
                            int base[3] = {co[0], co[1], co[2]};
                            base[axis] += sgn;
                            int p1[3] = {base[0], base[1], base[2]}; p1[u] += cu;
                            int p2[3] = {base[0], base[1], base[2]}; p2[v] += cv;
                            int p3[3] = {base[0], base[1], base[2]}; p3[u] += cu; p3[v] += cv;
                            int s1 = matSolid(padded[pidx(p1[0], p1[1], p1[2])]) ? 1 : 0;
                            int s2 = matSolid(padded[pidx(p2[0], p2[1], p2[2])]) ? 1 : 0;
                            int s3 = matSolid(padded[pidx(p3[0], p3[1], p3[2])]) ? 1 : 0;
                            int ao = (s1 && s2) ? 0 : 3 - (s1 + s2 + s3);
                            aos |= (uint32_t)ao << (cc * 2);
                        }
                        int wx = bx + co[0], wy = by + co[1], wz = bz + co[2];
                        uint32_t flags = 0;
                        if (isFresh(wx, wy, wz)) flags |= 1;
                        uint32_t entry = 0x80000000u | ((uint32_t)self << 20) | (aos << 4) | flags;
                        mask[i + CH * j] = entry;
                        anyMask = true;
                    }
                }
                if (!anyMask) continue;
                // merge rectangles
                for (int j = 0; j < CH; j++) {
                    for (int i = 0; i < CH;) {
                        uint32_t e = mask[i + CH * j];
                        if (!(e & 0x80000000u)) { i++; continue; }
                        int w = 1;
                        while (i + w < CH && mask[(i + w) + CH * j] == e) w++;
                        int h = 1;
                        bool grow = true;
                        while (grow && j + h < CH) {
                            for (int k = 0; k < w; k++)
                                if (mask[(i + k) + CH * (j + h)] != e) { grow = false; break; }
                            if (grow) h++;
                        }
                        for (int jj = 0; jj < h; jj++)
                            for (int ii = 0; ii < w; ii++) mask[(i + ii) + CH * (j + jj)] = 0;

                        uint8_t mat = (uint8_t)((e >> 20) & 0xFF);
                        uint32_t aos = (e >> 4) & 0xFF;
                        uint32_t flags = e & 0xF;
                        int lo[3], hi[3];
                        lo[axis] = hi[axis] = s + (sgn > 0 ? 1 : 0);
                        lo[u] = i; hi[u] = i + w;
                        lo[v] = j; hi[v] = j + h;
                        // corners in (u,v) order: (lo,lo) (hi,lo) (hi,hi) (lo,hi)
                        vec3 P[4];
                        int ao[4];
                        for (int cc = 0; cc < 4; cc++) {
                            int cu = (cc == 1 || cc == 2) ? hi[u] : lo[u];
                            int cv = (cc >= 2) ? hi[v] : lo[v];
                            int q[3] = {0, 0, 0};
                            q[axis] = lo[axis]; q[u] = cu; q[v] = cv;
                            P[cc] = vec3((float)q[0] * VOX, (float)q[1] * VOX, (float)q[2] * VOX);
                            // map corner index -> ao bits: 0:(-u,-v) 1:(+u,-v) 2:(+u,+v) 3:(-u,+v)
                            ao[cc] = (aos >> (cc * 2)) & 3;
                        }
                        uint32_t base = (uint32_t)out.verts.size();
                        std::vector<uint32_t>& tgt = matTransparent(mat) ? out.idxTrans : out.idx;
                        uint8_t vflags = (uint8_t)(flags & 1);
                        if (mat == M_WATER) vflags |= 2;
                        if (mat == M_LAMP) vflags |= 4;
                        for (int cc = 0; cc < 4; cc++) {
                            VoxVert vv;
                            vv.x = P[cc].x; vv.y = P[cc].y; vv.z = P[cc].z;
                            vv.n = nrm; vv.mat = mat; vv.ao = (uint8_t)ao[cc] * 85; vv.flags = vflags;
                            out.verts.push_back(vv);
                        }
                        bool flip = (ao[0] + ao[2]) < (ao[1] + ao[3]);
                        if (sgn > 0) {
                            if (!flip) {
                                tgt.push_back(base + 0); tgt.push_back(base + 1); tgt.push_back(base + 2);
                                tgt.push_back(base + 0); tgt.push_back(base + 2); tgt.push_back(base + 3);
                            } else {
                                tgt.push_back(base + 1); tgt.push_back(base + 2); tgt.push_back(base + 3);
                                tgt.push_back(base + 1); tgt.push_back(base + 3); tgt.push_back(base + 0);
                            }
                        } else {
                            if (!flip) {
                                tgt.push_back(base + 0); tgt.push_back(base + 2); tgt.push_back(base + 1);
                                tgt.push_back(base + 0); tgt.push_back(base + 3); tgt.push_back(base + 2);
                            } else {
                                tgt.push_back(base + 1); tgt.push_back(base + 3); tgt.push_back(base + 2);
                                tgt.push_back(base + 1); tgt.push_back(base + 0); tgt.push_back(base + 3);
                            }
                        }
                        i += w;
                    }
                }
            }
        }
    }
    // offset in chunk space so vertex positions are world-relative and chunk model matrix = translation
    vec3 off((float)(cx * CH) * VOX, (float)(cy * CH) * VOX, (float)(cz * CH) * VOX);
    for (auto& v : out.verts) { v.x += off.x; v.y += off.y; v.z += off.z; }
    out.bounds = AABB(off, off + vec3((float)CH * VOX));
    out.gpuDirty = true;
    c->dirty = false;
}

void World::rebuildDirtyMeshes(int budgetChunks) {
    if (dirtyChunks.empty()) return;
    int n = 0;
    // build the queued chunks, prioritising the ones closest to the front of the queue
    while (!dirtyChunks.empty() && n < budgetChunks) {
        uint64_t k = dirtyChunks.front();
        dirtyChunks.erase(dirtyChunks.begin());
        auto it = chunks.find(k);
        if (it == chunks.end()) continue;
        if (!it->second->dirty) continue;
        ivec3 cc = unchunkKey(k);
        buildChunkMesh(cc.x, cc.y, cc.z);
        n++;
    }
}

void World::markAllDirty() {
    for (auto& kv : chunks) {
        kv.second->dirty = true;
        dirtyChunks.push_back(kv.first);
    }
}

// ---------------------------------------------------------------- raycast
RayHit World::raycast(const vec3& originM, const vec3& dir, float maxDistM, bool hitWater) const {
    RayHit hit;
    vec3 o = originM * (1.0f / VOX);
    vec3 d = dir.norm();
    float maxT = maxDistM / VOX;

    int x = (int)std::floor(o.x), y = (int)std::floor(o.y), z = (int)std::floor(o.z);
    int stepX = d.x > 0 ? 1 : -1, stepY = d.y > 0 ? 1 : -1, stepZ = d.z > 0 ? 1 : -1;
    float tDeltaX = d.x != 0 ? std::fabs(1.0f / d.x) : 1e30f;
    float tDeltaY = d.y != 0 ? std::fabs(1.0f / d.y) : 1e30f;
    float tDeltaZ = d.z != 0 ? std::fabs(1.0f / d.z) : 1e30f;
    float tMaxX = d.x != 0 ? ((d.x > 0 ? (x + 1 - o.x) : (o.x - x)) * tDeltaX) : 1e30f;
    float tMaxY = d.y != 0 ? ((d.y > 0 ? (y + 1 - o.y) : (o.y - y)) * tDeltaY) : 1e30f;
    float tMaxZ = d.z != 0 ? ((d.z > 0 ? (z + 1 - o.z) : (o.z - z)) * tDeltaZ) : 1e30f;

    if (!inBounds(x, y, z)) {
        // fast-forward into the world bounds
        float t = 0;
        for (int i = 0; i < 3; i++) {
            if (d[i] > 0 && o[i] < 0) t = std::max(t, -o[i] / d[i]);
            if (d[i] < 0 && o[i] > (float)(&sx)[i]) t = std::max(t, (o[i] - (float)(&sx)[i]) / -d[i]);
        }
        if (t > maxT) return hit;
        o += d * (t + 0.001f);
        x = (int)std::floor(o.x); y = (int)std::floor(o.y); z = (int)std::floor(o.z);
        tMaxX = d.x != 0 ? ((d.x > 0 ? (x + 1 - o.x) : (o.x - x)) * tDeltaX) : 1e30f;
        tMaxY = d.y != 0 ? ((d.y > 0 ? (y + 1 - o.y) : (o.y - y)) * tDeltaY) : 1e30f;
        tMaxZ = d.z != 0 ? ((d.z > 0 ? (z + 1 - o.z) : (o.z - z)) * tDeltaZ) : 1e30f;
        if (!inBounds(x, y, z)) return hit;
    }

    float t = 0;
    int guard = 0;
    while (t <= maxT && guard++ < 4000) {
        uint8_t m = at(x, y, z);
        bool solid = matSolid(m);
        if (solid || (hitWater && m == M_WATER)) {
            hit.hit = true;
            hit.t = t * VOX;
            hit.pos = originM + dir * hit.t;
            hit.cell = {x, y, z};
            hit.mat = m;
            if (t == 0) {
                // starting inside geometry: normal opposite dominant direction
                vec3 ad = vabs(dir);
                if (ad.x >= ad.y && ad.x >= ad.z) hit.normal = {-signf(dir.x), 0, 0};
                else if (ad.y >= ad.z) hit.normal = {0, -signf(dir.y), 0};
                else hit.normal = {0, 0, -signf(dir.z)};
            }
            return hit;
        }
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX; t = tMaxX; tMaxX += tDeltaX;
            hit.normal = {-(float)stepX, 0, 0};
        } else if (tMaxY < tMaxZ) {
            y += stepY; t = tMaxY; tMaxY += tDeltaY;
            hit.normal = {0, -(float)stepY, 0};
        } else {
            z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ;
            hit.normal = {0, 0, -(float)stepZ};
        }
    }
    return hit;
}

bool World::aabbSolid(const AABB& box) const {
    int x0 = (int)std::floor(box.mn.x / VOX), x1 = (int)std::floor(box.mx.x / VOX - 1e-4f);
    int y0 = (int)std::floor(box.mn.y / VOX), y1 = (int)std::floor(box.mx.y / VOX - 1e-4f);
    int z0 = (int)std::floor(box.mn.z / VOX), z1 = (int)std::floor(box.mx.z / VOX - 1e-4f);
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                if (solidAt(x, y, z)) return true;
    return false;
}

float World::groundHeightM(float x, float z) const {
    int vx = (int)std::floor(x / VOX), vz = (int)std::floor(z / VOX);
    for (int y = sy - 1; y >= 0; y--)
        if (solidAt(vx, y, vz)) return (y + 1) * VOX;
    return 0;
}

// ---------------------------------------------------------------- destruction
World::CarveResult World::carve(const CarveParams& p) {
    CarveResult res;
    res.box = AABB(p.centerM - vec3(p.radiusM), p.centerM + vec3(p.radiusM));
    res.any = false;
    carveVoxelSphere(p.centerM * (1.0f / VOX), p.radiusM / VOX, p.power, p.damageMul, &res, &res.box);
    return res;
}

void World::carveVoxelSphere(const vec3& c, float r, float power, float damageMul, CarveResult* res, AABB* touched) {
    // Distance is measured to the *box* of each voxel, not its centre: a hit on a face or
    // an edge must always carve the geometry it actually touched (small tool radii would
    // otherwise miss every voxel centre and appear to do nothing).
    int x0 = (int)std::floor(c.x - r), x1 = (int)std::ceil(c.x + r);
    int y0 = (int)std::floor(c.y - r), y1 = (int)std::ceil(c.y + r);
    int z0 = (int)std::floor(c.z - r), z1 = (int)std::ceil(c.z + r);
    int removedTotal = 0;
    const int kMaxRemove = 60000;
    for (int y = std::max(0, y0); y <= std::min(sy - 1, y1); y++)
        for (int z = std::max(0, z0); z <= std::min(sz - 1, z1); z++)
            for (int x = std::max(0, x0); x <= std::min(sx - 1, x1); x++) {
                float bx = std::max(std::max((float)x - c.x, c.x - (float)(x + 1)), 0.0f);
                float by = std::max(std::max((float)y - c.y, c.y - (float)(y + 1)), 0.0f);
                float bz = std::max(std::max((float)z - c.z, c.z - (float)(z + 1)), 0.0f);
                float d = std::sqrt(bx * bx + by * by + bz * bz);
                if (d > r) continue;
                uint8_t m = at(x, y, z);
                if (!matSolid(m)) continue;
                float falloff = std::pow(std::max(0.0f, 1.0f - d / r), 0.85f);
                float eff = power * falloff * damageMul;
                const Material& mt = material(m);
                if (eff >= mt.resistance) {
                    if (removedTotal >= kMaxRemove) continue;
                    setFast(x, y, z, M_AIR);
                    removedTotal++;
                    if (res) { res->removed++; res->any = true; res->matCounts[m]++; }
                    if (touched) {
                        touched->mn = vmin(touched->mn, vec3((float)x * VOX, (float)y * VOX, (float)z * VOX));
                        touched->mx = vmax(touched->mx, vec3((float)(x + 1) * VOX, (float)(y + 1) * VOX, (float)(z + 1) * VOX));
                    }
                } else {
                    uint8_t add = (uint8_t)clampf(eff * 0.25f, 1.0f, 255.0f);
                    uint64_t k = keyOf(x, y, z);
                    int cur = (int)damageAt(x, y, z) + add;
                    if (cur >= (int)mt.resistance) {
                        if (removedTotal >= kMaxRemove) continue;
                        setFast(x, y, z, M_AIR);
                        removedTotal++;
                        if (res) { res->removed++; res->any = true; res->matCounts[m]++; }
                    } else {
                        damage[k] = (uint8_t)std::min(255, cur);
                        if (res) { res->damaged++; res->any = true; }
                        markChunkDirty(x, y, z);
                    }
                }
            }
}

// ---------------------------------------------------------------- support analysis
void analyzeSupport(const World& w, const ivec3& rmin, const ivec3& rmax, std::vector<FloatingCluster>& out) {
    const int CS = 4;
    ivec3 cmin(rmin.x / CS, rmin.y / CS, rmin.z / CS);
    ivec3 cmax(rmax.x / CS, rmax.y / CS, rmax.z / CS);
    cmin = {std::max(0, cmin.x), std::max(0, cmin.y), std::max(0, cmin.z)};
    cmax = {std::min(w.sx / CS - 1, cmax.x), std::min(w.sy / CS - 1, cmax.y), std::min(w.sz / CS - 1, cmax.z)};
    if (cmin.x > cmax.x || cmin.y > cmax.y || cmin.z > cmax.z) return;
    int nx = cmax.x - cmin.x + 1, ny = cmax.y - cmin.y + 1, nz = cmax.z - cmin.z + 1;
    if ((long long)nx * ny * nz > 400000) return;

    std::vector<uint8_t> cell((size_t)nx * ny * nz, 0);
    auto CI = [&](int x, int y, int z) { return (x - cmin.x) + nx * ((z - cmin.z) + nz * (y - cmin.y)); };
    for (int gy = 0; gy < ny; gy++)
        for (int gz = 0; gz < nz; gz++)
            for (int gx = 0; gx < nx; gx++) {
                int bx = (cmin.x + gx) * CS, by = (cmin.y + gy) * CS, bz = (cmin.z + gz) * CS;
                int cnt = 0;
                for (int y = 0; y < CS; y++)
                    for (int z = 0; z < CS; z++)
                        for (int x = 0; x < CS; x++)
                            if (w.solidAt(bx + x, by + y, bz + z)) cnt++;
                cell[CI(cmin.x + gx, cmin.y + gy, cmin.z + gz)] = cnt >= 9 ? 1 : (cnt >= 3 ? 2 : 0);
            }

    // BFS from anchors
    std::vector<uint8_t> anchored(cell.size(), 0);
    std::vector<int> stack;
    auto pushIf = [&](int gx, int gy, int gz) {
        if (gx < 0 || gy < 0 || gz < 0 || gx >= nx || gy >= ny || gz >= nz) return;
        int i = gx + nx * (gz + nz * gy);
        if (!cell[i] || anchored[i]) return;
        anchored[i] = 1;
        stack.push_back(i);
    };
    for (int gy = 0; gy < ny; gy++)
        for (int gz = 0; gz < nz; gz++)
            for (int gx = 0; gx < nx; gx++) {
                int i = gx + nx * (gz + nz * gy);
                if (!cell[i]) continue;
                bool border = (gx == 0 || gy == 0 || gz == 0 || gx == nx - 1 || gy == ny - 1 || gz == nz - 1);
                bool ground = (cmin.y + gy) * CS < w.groundVox;
                if (border || ground) { anchored[i] = 1; stack.push_back(i); }
            }
    static const int off[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    while (!stack.empty()) {
        int i = stack.back(); stack.pop_back();
        int gy = i / (nx * nz), rem = i % (nx * nz);
        int gz = rem / nx, gx = rem % nx;
        for (auto& o : off) pushIf(gx + o[0], gy + o[1], gz + o[2]);
    }
    // collect floating clusters
    std::vector<uint8_t> seen(cell.size(), 0);
    for (int gy = 0; gy < ny; gy++)
        for (int gz = 0; gz < nz; gz++)
            for (int gx = 0; gx < nx; gx++) {
                int i = gx + nx * (gz + nz * gy);
                if (!cell[i] || anchored[i] || seen[i]) continue;
                FloatingCluster cl;
                stack.clear();
                stack.push_back(i);
                seen[i] = 1;
                vec3 mn(1e9f), mx(-1e9f);
                while (!stack.empty()) {
                    int j = stack.back(); stack.pop_back();
                    int jy = j / (nx * nz), jrem = j % (nx * nz);
                    int jz = jrem / nx, jx = jrem % nx;
                    SupportCell sc{cmin.x + jx, cmin.y + jy, cmin.z + jz};
                    cl.cells.push_back(sc);
                    vec3 a((float)sc.x * CS * VOX, (float)sc.y * CS * VOX, (float)sc.z * CS * VOX);
                    vec3 b = a + vec3((float)CS * VOX);
                    mn = vmin(mn, a); mx = vmax(mx, b);
                    for (auto& o : off) {
                        int qx = jx + o[0], qy = jy + o[1], qz = jz + o[2];
                        if (qx < 0 || qy < 0 || qz < 0 || qx >= nx || qy >= ny || qz >= nz) continue;
                        int qi = qx + nx * (qz + nz * qy);
                        if (cell[qi] && !anchored[qi] && !seen[qi]) { seen[qi] = 1; stack.push_back(qi); }
                    }
                }
                cl.boundsM = AABB(mn, mx);
                out.push_back(std::move(cl));
            }
}

// ---------------------------------------------------------------- serialization
namespace {
struct Wr {
    std::vector<uint8_t>& b;
    void u8(uint8_t v) { b.push_back(v); }
    void u16(uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (i * 8)) & 0xFF); }
    void u64(uint64_t v) { for (int i = 0; i < 8; i++) b.push_back((v >> (i * 8)) & 0xFF); }
};
struct Rd {
    const uint8_t* d; size_t n, p = 0;
    bool ok = true;
    uint8_t u8() { if (p + 1 > n) { ok = false; return 0; } return d[p++]; }
    uint16_t u16() { uint16_t v = u8(); v |= (uint16_t)u8() << 8; return v; }
    uint32_t u32() { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)u8() << (i * 8); return v; }
    uint64_t u64() { uint64_t v = 0; for (int i = 0; i < 8; i++) v |= (uint64_t)u8() << (i * 8); return v; }
};
}

void serializeWorld(const World& w, std::vector<uint8_t>& out) {
    Wr wr{out};
    wr.u32(0x46524357); // FRCW
    wr.u32(1);
    wr.u32((uint32_t)w.sx); wr.u32((uint32_t)w.sy); wr.u32((uint32_t)w.sz);
    wr.u32((uint32_t)w.groundVox);
    // only chunks that were modified during gameplay
    std::vector<uint64_t> keys;
    for (auto& kv : w.chunks)
        if (w.modifiedChunks.count(kv.first)) keys.push_back(kv.first);
    wr.u32((uint32_t)keys.size());
    for (uint64_t k : keys) {
        Chunk* c = w.chunks.at(k);
        std::vector<std::pair<uint16_t, uint8_t>> runs;
        runs.reserve(512);
        uint16_t runLen = 0;
        uint8_t runMat = c->v[0];
        for (int i = 0; i < CHV; i++) {
            if (c->v[i] == runMat && runLen < 65535) { runLen++; continue; }
            runs.push_back({runLen, runMat});
            runMat = c->v[i];
            runLen = 1;
        }
        if (runLen) runs.push_back({runLen, runMat});
        wr.u64(k);
        wr.u32((uint32_t)runs.size());
        for (auto& r : runs) { wr.u16(r.first); wr.u8(r.second); }
    }
}

bool deserializeWorld(World& w, const uint8_t* data, size_t size) {
    Rd rd{data, size};
    if (rd.u32() != 0x46524357) return false;
    uint32_t ver = rd.u32();
    if (ver != 1) return false;
    int sx = (int)rd.u32(), sy = (int)rd.u32(), sz = (int)rd.u32();
    int gv = (int)rd.u32();
    uint32_t count = rd.u32();
    if (!rd.ok) return false;
    if (sx != w.sx || sy != w.sy || sz != w.sz) return false;
    w.groundVox = gv;
    for (uint32_t ci = 0; ci < count && ci < 100000; ci++) {
        uint64_t k = rd.u64();
        uint32_t runs = rd.u32();
        ivec3 cc = World::unchunkKey(k);
        Chunk* c = w.getOrCreateChunk(cc.x, cc.y, cc.z);
        if (!c) return false;
        int idx = 0;
        for (uint32_t r = 0; r < runs; r++) {
            uint16_t len = rd.u16();
            uint8_t m = rd.u8();
            if (!rd.ok) return false;
            for (int i = 0; i < len && idx < CHV; i++, idx++) c->v[idx] = m;
        }
        w.modifiedChunks.insert(k);
        c->dirty = true;
    }
    return rd.ok;
}

size_t World::memoryEstimate() const {
    size_t s = chunks.size() * (sizeof(Chunk) + 64);
    for (auto& kv : meshes) s += kv.second.verts.capacity() * sizeof(VoxVert) + kv.second.idx.capacity() * 4;
    s += damage.size() * 24 + fresh.size() * 24;
    return s;
}
