// FRACTURE — greedy voxel mesher with per-corner ambient occlusion.
// Template core over a padded accessor; instantiations for world chunks and clusters.
#pragma once
#include "voxworld.h"

namespace ft {

inline f32 aoFactor(int level) { // stored level1..4 → brightness
    static const f32 T[4] = {0.45f, 0.66f, 0.85f, 1.0f};
    return T[tclamp(level - 1, 0, 3)];
}

// Src concept: int w,h,d;  Voxel get(int x,int y,int z) const  (padded: out-of-range = air)
template <class Src>
inline void greedyMeshPadded(const Src& s, int w, int h, int d,
                             std::vector<u8>& outV, std::vector<u32>& outI,
                             std::vector<u8>& outTV, std::vector<u32>& outTI) {
    auto aoOcc = [&](int x, int y, int z) -> int {
        return matSolid(s.get(x, y, z).mat) ? 1 : 0;
    };
    // does neighbour q occlude a face of p?
    auto occludes = [&](const Voxel& p, const Voxel& q) -> bool {
        if (q.mat == MAT_AIR) return false;
        if (matSolid(q.mat)) return true;
        if (q.mat == MAT_WATER && p.mat == MAT_WATER) return true;
        if (q.mat != MAT_WATER && p.mat != MAT_WATER && !matSolid(p.mat)) return true; // foliage/fuzzy pairs
        return false;
    };
    auto packKey = [&](u8 matId, u16 col, u8 flags, int ao0, int ao1, int ao2, int ao3) -> u64 {
        return (u64)matId | ((u64)col << 8) | ((u64)flags << 24) |
               ((u64)(ao0 & 3) << 32) | ((u64)(ao1 & 3) << 34) |
               ((u64)(ao2 & 3) << 36) | ((u64)(ao3 & 3) << 38);
    };
    const int dims[3] = {w, h, d};
    std::vector<u64> mask((size_t)w * h * d, 0);
    std::vector<u64> tmask((size_t)w * h * d, 0);

    auto emit = [&](std::vector<u8>& ov, std::vector<u32>& oi,
                    int a, int dsign, int sl, int u0, int v0, int uw, int vh,
                    u8 matId, u8 flags, u16 col, const int ao[4]) {
        u32 vb = (u32)(ov.size() / sizeof(MVert));
        f32 aCoord = (f32)((dsign > 0 ? sl + 1 : sl)) * VOX;
        f32 uA = (f32)u0 * VOX, uB = (f32)(u0 + uw) * VOX;
        f32 vA = (f32)v0 * VOX, vB = (f32)(v0 + vh) * VOX;
        f32 pos[4][3];
        auto setPos = [&](int k, f32 uu, f32 vv) {
            pos[k][0] = pos[k][1] = pos[k][2] = 0;
            pos[k][a] = aCoord;
            pos[k][(a + 1) % 3] = uu;
            pos[k][(a + 2) % 3] = vv;
        };
        setPos(0, uA, vA);
        setPos(1, uB, vA);
        setPos(2, uB, vB);
        setPos(3, uA, vB);
        f32 nrm[3] = {0, 0, 0};
        nrm[a] = (f32)dsign;
        int rgb[3]; unpack565(col, rgb[0], rgb[1], rgb[2]);

        MVert mv;
        std::memset(&mv, 0, sizeof(mv));
        mv.n[0] = nrm[0]; mv.n[1] = nrm[1]; mv.n[2] = nrm[2];
        mv.col[0] = (u8)rgb[0]; mv.col[1] = (u8)rgb[1]; mv.col[2] = (u8)rgb[2];
        mv.mat = matId; mv.flags = flags;

        auto push = [&](int k, int aoL) {
            mv.p[0] = pos[k][0]; mv.p[1] = pos[k][1]; mv.p[2] = pos[k][2];
            mv.ao = (u8)(aoL + 1);
            size_t off = ov.size();
            ov.resize(off + sizeof(MVert));
            std::memcpy(ov.data() + off, &mv, sizeof(MVert));
        };
        if (dsign > 0) {
            push(0, ao[0]); push(1, ao[1]); push(2, ao[2]);
            push(0, ao[0]); push(2, ao[2]); push(3, ao[3]);
            oi.push_back(vb + 0); oi.push_back(vb + 1); oi.push_back(vb + 2);
            oi.push_back(vb + 3); oi.push_back(vb + 4); oi.push_back(vb + 5);
        } else {
            push(0, ao[0]); push(3, ao[3]); push(2, ao[2]);
            push(0, ao[0]); push(2, ao[2]); push(1, ao[1]);
            oi.push_back(vb + 0); oi.push_back(vb + 1); oi.push_back(vb + 2);
            oi.push_back(vb + 3); oi.push_back(vb + 4); oi.push_back(vb + 5);
        }
    };

    for (int a = 0; a < 3; a++) {
        int u = (a + 1) % 3, v = (a + 2) % 3;
        int sa = dims[a], su = dims[u], sv = dims[v];
        int dsigns[2] = {1, -1};
        for (int ds = 0; ds < 2; ds++) {
            int dsign = dsigns[ds];
            int slStart = dsign > 0 ? 0 : sa - 1;
            int slEnd = dsign > 0 ? sa : -1;
            int slStep = dsign > 0 ? 1 : -1;
            for (int sl = slStart; sl != slEnd; sl += slStep) {
                int maskW = su, maskH = sv;
                std::memset(mask.data(), 0, sizeof(u64) * (size_t)maskW * maskH);
                std::memset(tmask.data(), 0, sizeof(u64) * (size_t)maskW * maskH);
                for (int iv = 0; iv < maskH; iv++) {
                    for (int iu = 0; iu < maskW; iu++) {
                        int c[3];
                        c[a] = sl; c[u] = iu; c[v] = iv;
                        const Voxel p = s.get(c[0], c[1], c[2]);
                        if (p.mat == MAT_AIR) continue;
                        int q[3] = {c[0], c[1], c[2]};
                        q[a] += dsign;
                        const Voxel qp = s.get(q[0], q[1], q[2]);
                        if (occludes(p, qp)) continue;
                        // AO from cells in the plane one step in front of the face
                        int n[3] = {q[0], q[1], q[2]};
                        int ao[4];
                        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
                            int du = i ? 1 : -1, dv = j ? 1 : -1;
                            int o1[3] = {n[0], n[1], n[2]}, o2[3] = {n[0], n[1], n[2]}, o3[3] = {n[0], n[1], n[2]};
                            o1[u] += du;
                            o2[v] += dv;
                            o3[u] += du; o3[v] += dv;
                            int s1 = aoOcc(o1[0], o1[1], o1[2]);
                            int s2 = aoOcc(o2[0], o2[1], o2[2]);
                            int sc = aoOcc(o3[0], o3[1], o3[2]);
                            int lvl = (s1 && s2) ? 0 : 3 - (s1 + s2 + sc);
                            ao[j * 2 + i] = tclamp(lvl, 0, 3);
                        }
                        u64 key = packKey(p.mat, p.col, p.flags, ao[0], ao[1], ao[3], ao[2]);
                        size_t mi = (size_t)iu + (size_t)iv * maskW;
                        if (p.mat == MAT_WATER) tmask[mi] = key;
                        else mask[mi] = key;
                    }
                }
                for (int pass = 0; pass < 2; pass++) {
                    u64* mmask = pass ? tmask.data() : mask.data();
                    std::vector<u8>& ov = pass ? outTV : outV;
                    std::vector<u32>& oi = pass ? outTI : outI;
                    for (int iv = 0; iv < maskH; iv++) {
                        for (int iu = 0; iu < maskW; iu++) {
                            u64 key = mmask[(size_t)iu + (size_t)iv * maskW];
                            if (!key) continue;
                            int wRun = 1;
                            while (iu + wRun < maskW &&
                                   mmask[(size_t)(iu + wRun) + (size_t)iv * maskW] == key) wRun++;
                            int hRun = 1, jj = iv + 1;
                            for (; jj < maskH; jj++) {
                                bool same = true;
                                for (int k = 0; k < wRun; k++)
                                    if (mmask[(size_t)(iu + k) + (size_t)jj * maskW] != key) { same = false; break; }
                                if (!same) break;
                                hRun++;
                            }
                            auto cornerAt = [&](int gi, int gj) {
                                int cu = gi ? iu + wRun - 1 : iu;
                                int cv = gj ? iv + hRun - 1 : iv;
                                u64 k = mmask[(size_t)cu + (size_t)cv * maskW];
                                int c00 = (int)((k >> 32) & 3), c10 = (int)((k >> 34) & 3);
                                int c11 = (int)((k >> 36) & 3), c01 = (int)((k >> 38) & 3);
                                if (gj == 0) return gi ? c10 : c00;
                                return gi ? c11 : c01;
                            };
                            int aoQuad[4] = {cornerAt(0, 0), cornerAt(1, 0), cornerAt(1, 1), cornerAt(0, 1)};
                            emit(ov, oi, a, dsign, sl, iu, iv, wRun, hRun,
                                 (u8)(key & 0xFF), (u8)((key >> 24) & 0xFF), (u16)((key >> 8) & 0xFFFF), aoQuad);
                            for (int j = 0; j < hRun; j++)
                                for (int k = 0; k < wRun; k++)
                                    mmask[(size_t)(iu + k) + (size_t)(iv + j) * maskW] = 0;
                        }
                    }
                }
            }
        }
    }
}

// --- VoxWorld chunk instantiation (accesses world with clamped neighbours) ---
struct ChunkSrc {
    const VoxWorld& w;
    int cx, cy, cz;
    Voxel get(int x, int y, int z) const {
        return w.get(cx * CH + x, cy * CH + y, cz * CH + z);
    }
};

void meshChunk(const VoxWorld& w, int cx, int cy, int cz, ChunkMesh& out);

// --- cluster (fragment) grid with1-voxel air padding ------------------------
struct ClusterGrid {
    int w = 0, h = 0, d = 0;
    std::vector<Voxel> cells;
    Voxel get(int x, int y, int z) const {
        x++; y++; z++;
        if (x < 0 || y < 0 || z < 0 || x >= w + 2 || y >= h + 2 || z >= d + 2) return Voxel{MAT_AIR, 0, 0};
        return cells[(size_t)x + (size_t)y * (w + 2) + (size_t)z * (w + 2) * (h + 2)];
    }
    void set(int x, int y, int z, Voxel v) {
        x++; y++; z++;
        if (x < 0 || y < 0 || z < 0 || x >= w + 2 || y >= h + 2 || z >= d + 2) return;
        cells[(size_t)x + (size_t)y * (w + 2) + (size_t)z * (w + 2) * (h + 2)] = v;
    }
    void resize(int W, int H, int D) {
        w = W; h = H; d = D;
        cells.assign((size_t)(w + 2) * (h + 2) * (d + 2), Voxel{MAT_AIR, 0, 0});
    }
};

void meshCluster(const ClusterGrid& g, std::vector<u8>& outV, std::vector<u32>& outI,
                 std::vector<u8>& outTV, std::vector<u32>& outTI);

} // namespace ft
