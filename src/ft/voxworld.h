// FRACTURE — voxel world: chunk storage, edits, raycast, serialisation
#pragma once
#include "math.h"
#include "rng.h"

namespace ft {

struct Chunk {
    Voxel vox[CH * CH * CH];
    u16 sup[CH * CH * CH];          // structural distance-to-anchor field (0xFFFF = unsupported)
    u32 version = 0;                // bumped when mesh must re-upload
    u16 solidCount = 0;
    u16 liquidCount = 0;
    u8  dirty = 1;                  // needs remesh
    u8  pad = 0;

    inline bool hasSolid() const { return solidCount > 0; }
    inline bool hasLiquid() const { return liquidCount > 0; }
    inline int idx(int x, int y, int z) const { return x + y * CH + z * CH * CH; }
    void recount() {
        u32 s = 0, l = 0;
        for (int i = 0; i < CH * CH * CH; i++) {
            u8 m = vox[i].mat;
            if (matSolid(m)) s++;
            else if (m == MAT_WATER) l++;
        }
        solidCount = (u16)tmin(s, (u32)65535);
        liquidCount = (u16)tmin(l, (u32)65535);
    }
};

constexpr u16 SUP_UNREACHED = 0xFFFF;

struct ChunkMesh {
    std::vector<u8> verts;   // MVert[32] each
    std::vector<u32> idx;
    std::vector<u8> tverts;  // transparent (water) sub-mesh
    std::vector<u32> tidx;
    u32 version = 0xFFFFFFFFu; // matches Chunk::version once built
    int vertCount() const { return (int)(verts.size() / 32); }
};

//32-byte voxel-mesh vertex (layout must match renderer attribs)
struct MVert {
    f32 p[3];
    f32 n[3];
    u8  col[3];
    u8  ao;
    u8  mat;
    u8  flags;
    u16 pad;
};
static_assert(sizeof(MVert) == 32, "MVert32B");

enum EditCause : u8 {
    EDIT_DAMAGE = 0,   // weapon / tool
    EDIT_BLAST,
    EDIT_FIRE,
    EDIT_COLLISION,    // vehicle / debris impact
    EDIT_BUILD,        // player placement
    EDIT_SCRIPT,       // mission / map script
    EDIT_SETTLE,       // fragment voxelsation
};

struct EditEvent {
    u16 x, y, z;
    u8  oldMat, newMat;
    EditCause cause;
};

class VoxWorld {
public:
    int W = 0, H = 0, D = 0;        // voxel dimensions
    int CW = 0, CHh = 0, CD = 0;    // chunk dimensions
    std::vector<Chunk> chunks;
    std::vector<ChunkMesh> meshes;  // parallel to chunks
    std::vector<EditEvent> events;  // per-frame edit events (cleared by game)
    std::vector<u32> dirtyChunks;
    int carved = 0;                 // voxels removed by blasts/cuts (stats)
    int carvedCount() const { return carved; }   // chunk indices needing remesh

    bool init(int w, int h, int d) {
        if (w <= 0 || h <= 0 || d <= 0 || w > 2048 || h > 512 || d > 4096) return false;
        W = w; H = h; D = d;
        CW = (w + CH - 1) / CH; CHh = (h + CH - 1) / CH; CD = (d + CH - 1) / CH;
        chunks.assign((size_t)CW * CHh * CD, Chunk{});
        meshes.assign(chunks.size(), ChunkMesh{});
        for (auto& c : chunks) {
            for (int i = 0; i < CH * CH * CH; i++) c.vox[i] = {MAT_AIR, 0, 0};
            std::memset(c.sup, 0xFF, sizeof(c.sup));
            c.dirty = 1; c.version = 0; c.solidCount = 0; c.liquidCount = 0;
        }
        pendingSup.clear();
        events.clear();
        dirtyChunks.clear();
        for (size_t i = 0; i < chunks.size(); i++) dirtyChunks.push_back((u32)i);
        return true;
    }
    void clear() { chunks.clear(); meshes.clear(); W = H = D = 0; pendingSup.clear(); events.clear(); dirtyChunks.clear(); }

    inline bool inBounds(int x, int y, int z) const { return x >= 0 && y >= 0 && z >= 0 && x < W && y < H && z < D; }
    inline size_t chunkIndex(int cx, int cy, int cz) const { return (size_t)cx + (size_t)cy * CW + (size_t)cz * CW * CHh; }
    inline size_t chunkIndexAtVoxel(int x, int y, int z) const { return chunkIndex(x / CH, y / CH, z / CH); }

    inline Voxel get(int x, int y, int z) const {
        if (!inBounds(x, y, z)) return Voxel{MAT_AIR, 0, 0};
        const Chunk& c = chunks[chunkIndex(x / CH, y / CH, z / CH)];
        return c.vox[c.idx(x & (CH - 1), y & (CH - 1), z & (CH - 1))];
    }
    inline u8 mat(int x, int y, int z) const { return get(x, y, z).mat; }
    inline bool solid(int x, int y, int z) const { return matSolid(get(x, y, z).mat); }
    inline u16 support(int x, int y, int z) const {
        if (!inBounds(x, y, z)) return SUP_UNREACHED;
        const Chunk& c = chunks[chunkIndex(x / CH, y / CH, z / CH)];
        return c.sup[c.idx(x & (CH - 1), y & (CH - 1), z & (CH - 1))];
    }

    // raw access (no bounds check)
    inline Voxel& atDirect(int x, int y, int z) {
        Chunk& c = chunks[chunkIndex(x / CH, y / CH, z / CH)];
        return c.vox[c.idx(x & (CH - 1), y & (CH - 1), z & (CH - 1))];
    }
    inline u16& supDirect(int x, int y, int z) {
        Chunk& c = chunks[chunkIndex(x / CH, y / CH, z / CH)];
        return c.sup[c.idx(x & (CH - 1), y & (CH - 1), z & (CH - 1))];
    }

    void markDirty(int x, int y, int z) {
        if (!inBounds(x, y, z)) return;
        size_t ci = chunkIndexAtVoxel(x, y, z);
        Chunk& c = chunks[ci];
        if (!c.dirty) { c.dirty = 1; dirtyChunks.push_back((u32)ci); }
    }

    // set a voxel; records support edits when solidity changed; bumps chunk versions
    void set(int x, int y, int z, u8 matId, u16 col, u8 flags, EditCause cause = EDIT_DAMAGE, bool updateSupport = true) {
        if (!inBounds(x, y, z)) return;
        size_t ci = chunkIndexAtVoxel(x, y, z);
        Chunk& c = chunks[ci];
        int li = c.idx(x & (CH - 1), y & (CH - 1), z & (CH - 1));
        Voxel old = c.vox[li];
        if (old.mat == matId && old.col == col && old.flags == flags) return;
        bool wasSolid = matSolid(old.mat), nowSolid = matSolid(matId);
        bool wasLiq = old.mat == MAT_WATER, nowLiq = matId == MAT_WATER;
        if (wasSolid && !nowSolid && (cause == EDIT_BLAST || cause == EDIT_DAMAGE)) carved++;
        c.vox[li] = Voxel{matId, flags, col};
        if (wasSolid != nowSolid) {
            if (nowSolid) c.solidCount = (u16)tmin((int)c.solidCount + 1, (int)CH * CH * CH);
            else if (c.solidCount > 0) c.solidCount--;
        }
        if (wasLiq != nowLiq) {
            if (nowLiq) c.liquidCount = (u16)tmin((int)c.liquidCount + 1, (int)CH * CH * CH);
            else if (c.liquidCount > 0) c.liquidCount--;
        }
        if (!c.dirty) { c.dirty = 1; dirtyChunks.push_back((u32)ci); }
        if (updateSupport && (wasSolid != nowSolid || ((old.flags ^ flags) & VF_FANCHOR))) {
            pendingSup.push_back({(u16)x, (u16)y, (u16)z});
        }
        if (old.mat != matId || old.col != col) {
            events.push_back(EditEvent{(u16)x, (u16)y, (u16)z, old.mat, matId, cause});
        }
    }
    // ---- bulk helpers -----------------------------------------------------
    void fillBox(int x0, int y0, int z0, int x1, int y1, int z1, u8 matId, u16 col, u8 extraFlags = 0, EditCause cause = EDIT_SCRIPT) {
        if (x0 > x1) std::swap(x0, x1); if (y0 > y1) std::swap(y0, y1); if (z0 > z1) std::swap(z0, z1);
        const MatDef& m = matDef(matId);
        u8 fl = (u8)((m.flags & MF_ANCHOR) | extraFlags);
        for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            if (inBounds(x, y, z)) {
                size_t ci = chunkIndexAtVoxel(x, y, z);
                Chunk& c = chunks[ci];
                int li = c.idx(x & (CH - 1), y & (CH - 1), z & (CH - 1));
                c.vox[li] = Voxel{matId, fl, col};
                pendingSup.push_back({(u16)x, (u16)y, (u16)z});
            }
        }
        // recount affected chunks (dirty marks too)
        int cx0 = x0 / CH, cy0 = y0 / CH, cz0 = z0 / CH;
        int cx1 = x1 / CH, cy1 = y1 / CH, cz1 = z1 / CH;
        for (int cz = cz0; cz <= cz1; cz++) for (int cy = cy0; cy <= cy1; cy++) for (int cx = cx0; cx <= cx1; cx++) {
            if (cx < 0 || cy < 0 || cz < 0 || cx >= CW || cy >= CHh || cz >= CD) continue;
            size_t ci = chunkIndex(cx, cy, cz);
            Chunk& c = chunks[ci];
            c.recount();
            if (!c.dirty) { c.dirty = 1; dirtyChunks.push_back((u32)ci); }
        }
    }
    // ---- raycast ----------------------------------------------------------
    // DDA through the grid; only MF_SOLID voxels block (unless includeLiquid)
    RayHit raycast(Vec3 origin, Vec3 dir, f32 maxT, bool includeGlass = true) const;

    // ---- support field ----------------------------------------------------
    struct SupEdit { u16 x, y, z; };
    std::vector<SupEdit> pendingSup;  // voxel coords queued for support wave

    // ---- serialisation (RLE of full state) --------------------------------
    std::vector<u8> serialize() const;
    bool deserialize(const std::vector<u8>& data);

    size_t byteSize() const { return chunks.size() * (sizeof(Chunk) + sizeof(ChunkMesh)); }
};

// helper: clamp line step count for carving
inline int iroundp(f32 v) { return (int)std::lround(v); }

} // namespace ft
