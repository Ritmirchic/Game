// FRACTURE — structural support implementation
#include "support.h"

namespace ft {

static const int NB6[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

static inline bool softMat(u8 m) {
    switch (m) {
        case MAT_SOIL: case MAT_SAND: case MAT_GRAVEL: case MAT_DIRTROAD:
        case MAT_ASPHALT: case MAT_SNOW: case MAT_LEAF: return true;
        default: return false;
    }
}

void SupportSystem::bind(VoxWorld* w) {
    world = w;
    regionMark.assign((size_t)w->W * w->H * w->D, 0);
    edits.clear(); suspects.clear(); regionList.clear();
}

void SupportSystem::computeAll() {
    if (!world) return;
    VoxWorld& w = *world;
    std::vector<u32> q;
    for (auto& c : w.chunks) std::memset(c.sup, 0xFF, sizeof(c.sup));
    for (int z = 0; z < w.D; z++)
        for (int x = 0; x < w.W; x++)
            if (w.solid(x, 0, z)) { w.supDirect(x, 0, z) = 0; q.push_back(pack(x, 0, z)); }
    // flagged anchors (skip bedrock layer already seeded)
    for (int y = 1; y < w.H; y++)
        for (int z = 0; z < w.D; z++)
            for (int x = 0; x < w.W; x++) {
                Voxel v = w.get(x, y, z);
                if (matSolid(v.mat) && (v.flags & VF_FANCHOR) && w.supDirect(x, y, z) != 0) {
                    w.supDirect(x, y, z) = 0;
                    q.push_back(pack(x, y, z));
                }
            }
    size_t head = 0;
    while (head < q.size()) {
        u32 p = q[head++];
        int x = px(p), y = py(p), z = pz(p);
        u16 cur = w.supDirect(x, y, z);
        if (cur == SUP_UNREACHED || cur >= 65534) continue;
        for (auto& n : NB6) {
            int nx = x + n[0], ny = y + n[1], nz = z + n[2];
            if (!w.inBounds(nx, ny, nz) || !w.solid(nx, ny, nz)) continue;
            u16& s = w.supDirect(nx, ny, nz);
            if (s == SUP_UNREACHED) { s = (u16)(cur + 1); q.push_back(pack(nx, ny, nz)); }
        }
    }
    suspects.clear();
    for (int z = 0; z < w.D; z++)
        for (int y = 0; y < w.H; y++)
            for (int x = 0; x < w.W; x++)
                if (w.solid(x, y, z) && w.supDirect(x, y, z) == SUP_UNREACHED)
                    suspects.push_back(pack(x, y, z));
    w.pendingSup.clear();
    edits.clear();
    logf("support: computeAll done, %d unsupported initial cells", (int)suspects.size());
}

void SupportSystem::drainPending() {
    if (!world) return;
    for (const auto& e : world->pendingSup) edits.push_back(pack(e.x, e.y, e.z));
    world->pendingSup.clear();
}

void SupportSystem::buildRegionAndRecompute() {
    VoxWorld& w = *world;
    if (regionMark.size() != (size_t)w.W * w.H * w.D) bind(world);
    regionList.clear();

    // --- flood region collection ------------------------------------------
    std::vector<u32> stack;
    stack.reserve(edits.size() * 7);
    auto trySeed = [&](int x, int y, int z) {
        if (!w.inBounds(x, y, z) || !w.solid(x, y, z)) return;
        if (y == 0) return;                    // bedrock stays outside (anchor)
        u32 p = pack(x, y, z);
        size_t mi = (size_t)x + (size_t)y * w.W + (size_t)z * w.W * w.H;
        if (regionMark[mi]) return;
        regionMark[mi] = 1;                    // depth0+1
        regionList.push_back(p);
        stack.push_back(p);
    };
    for (u32 e : edits) {
        int x = px(e), y = py(e), z = pz(e);
        trySeed(x, y, z);
        for (auto& n : NB6) trySeed(x + n[0], y + n[1], z + n[2]);
    }
    bool cappedLog = false;
    while (!stack.empty()) {
        u32 p = stack.back();
        stack.pop_back();
        int x = px(p), y = py(p), z = pz(p);
        size_t mi = (size_t)x + (size_t)y * w.W + (size_t)z * w.W * w.H;
        u16 mk = regionMark[mi];
        int depth = mk ? (int)(mk - 1) : 0;
        for (auto& n : NB6) {
            int nx = x + n[0], ny = y + n[1], nz = z + n[2];
            if (!w.inBounds(nx, ny, nz) || !w.solid(nx, ny, nz)) continue;
            if (ny == 0) continue;             // bedrock: outside boundary
            size_t nmi = (size_t)nx + (size_t)ny * w.W + (size_t)nz * w.W * w.H;
            if (regionMark[nmi]) continue;
            bool unreach = w.supDirect(nx, ny, nz) == SUP_UNREACHED;
            if (!unreach && softMat(w.mat(nx, ny, nz)) && depth + 1 > softHops) continue;
            if (regionList.size() >= (size_t)regionCap) {
                if (!cappedLog) { logf("support: region cap %d reached (big structure)", regionCap); cappedLog = true; }
                continue;
            }
            int nd = depth + 1;
            regionMark[nmi] = (u16)(nd >= 0x7FFF ? 0x7FFF : nd + 1);
            regionList.push_back(pack(nx, ny, nz));
            stack.push_back(pack(nx, ny, nz));
        }
    }
    if (regionList.empty()) return;

    // --- reset region to UNREACHED ----------------------------------------
    for (u32 p : regionList) {
        int x = px(p), y = py(p), z = pz(p);
        w.supDirect(x, y, z) = SUP_UNREACHED;
    }

    // --- relaxation from boundary / anchors until stable -------------------
    std::vector<u32> work = regionList;
    size_t head = 0;
    while (head < work.size()) {
        u32 p = work[head++];
        int x = px(p), y = py(p), z = pz(p);
        if (!w.solid(x, y, z)) continue;
        u16 nv;
        if (isAnchorAt(w, x, y, z)) nv = 0;
        else {
            u16 best = SUP_UNREACHED;
            for (auto& n : NB6) {
                int nx = x + n[0], ny = y + n[1], nz = z + n[2];
                if (!w.inBounds(nx, ny, nz)) continue;
                if (ny == 0 && w.solid(nx, ny, nz)) { best = tmin<u16>(best, 1); continue; }
                if (!w.solid(nx, ny, nz)) continue;
                size_t nmi = (size_t)nx + (size_t)ny * w.W + (size_t)nz * w.W * w.H;
                u16 s;
                if (regionMark[nmi]) s = w.supDirect(nx, ny, nz); // in region (may be current)
                else s = w.supDirect(nx, ny, nz);                 // outside: assumed valid
                if (s == SUP_UNREACHED) continue;
                u16 cand = (u16)(s + 1);
                if (cand == SUP_UNREACHED) continue;
                if (cand < best) best = cand;
            }
            nv = best;
        }
        if (nv != w.supDirect(x, y, z)) {
            w.supDirect(x, y, z) = nv;
            for (auto& n : NB6) {
                int nx = x + n[0], ny = y + n[1], nz = z + n[2];
                if (!w.inBounds(nx, ny, nz) || !w.solid(nx, ny, nz)) continue;
                size_t nmi = (size_t)nx + (size_t)ny * w.W + (size_t)nz * w.W * w.H;
                if (!regionMark[nmi]) continue;   // only region cells are updated
                work.push_back(pack(nx, ny, nz));
            }
        }
    }

    // --- collect suspects + clear marks ------------------------------------
    suspects.clear();
    for (u32 p : regionList) {
        int x = px(p), y = py(p), z = pz(p);
        if (w.solid(x, y, z) && w.supDirect(x, y, z) == SUP_UNREACHED) suspects.push_back(p);
        regionMark[(size_t)x + (size_t)y * w.W + (size_t)z * w.W * w.H] = 0;
    }
    regionList.clear();
}

int SupportSystem::process(int budget, std::vector<Cluster>& outClusters) {
    (void)budget;
    if (!world) return 0;
    drainPending();
    int work = 0;
    if (!edits.empty()) {
        buildRegionAndRecompute();
        work = (int)edits.size();
        edits.clear();
    }
    if (!suspects.empty()) extractSuspects(outClusters);
    return work;
}

void SupportSystem::extractSuspects(std::vector<Cluster>& out) {
    VoxWorld& w = *world;
    const int MAXV = 60000;
    std::vector<u32> marked;
    marked.reserve(1024);
    bool stopped = false;
    size_t idx = 0;
    for (; idx < suspects.size(); idx++) {
        u32 sp = suspects[idx];
        int sx = px(sp), sy = py(sp), sz = pz(sp);
        if (!w.inBounds(sx, sy, sz) || !w.solid(sx, sy, sz)) continue;
        if (w.supDirect(sx, sy, sz) != SUP_UNREACHED) continue;
        int minx = sx, miny = sy, minz = sz, maxx = sx, maxy = sy, maxz = sz;
        std::vector<u32> comp;
        std::vector<u32> stack{sp};
        w.supDirect(sx, sy, sz) = 0xFFFE;
        marked.push_back(sp);
        bool capped = false;
        while (!stack.empty()) {
            if ((int)comp.size() >= MAXV) { capped = true; break; }
            u32 cp = stack.back(); stack.pop_back();
            comp.push_back(cp);
            int x = px(cp), y = py(cp), z = pz(cp);
            minx = tmin(minx, x); miny = tmin(miny, y); minz = tmin(minz, z);
            maxx = tmax(maxx, x); maxy = tmax(maxy, y); maxz = tmax(maxz, z);
            for (auto& n : NB6) {
                int nx = x + n[0], ny = y + n[1], nz = z + n[2];
                if (!w.inBounds(nx, ny, nz) || !w.solid(nx, ny, nz)) continue;
                if (w.supDirect(nx, ny, nz) != SUP_UNREACHED) continue;
                w.supDirect(nx, ny, nz) = 0xFFFE;
                marked.push_back(pack(nx, ny, nz));
                stack.push_back(pack(nx, ny, nz));
            }
        }
        if (comp.empty()) continue;
        Cluster cl;
        cl.ox = minx; cl.oy = miny; cl.oz = minz;
        cl.w = maxx - minx + 1; cl.h = maxy - miny + 1; cl.d = maxz - minz + 1;
        cl.vox.assign((size_t)cl.w * cl.h * cl.d, Voxel{MAT_AIR, 0, 0});
        for (u32 cp : comp) {
            int x = px(cp), y = py(cp), z = pz(cp);
            cl.vox[(size_t)(x - minx) + (size_t)(y - miny) * cl.w + (size_t)(z - minz) * cl.w * cl.h] = w.get(x, y, z);
        }
        cl.count = (int)comp.size();
        for (u32 cp : comp) w.set(px(cp), py(cp), pz(cp), MAT_AIR, 0, 0, EDIT_DAMAGE, false);
        out.push_back(std::move(cl));
        if (capped) { stopped = true; break; }
    }
    std::vector<u32> next;
    if (stopped) {
        next.reserve(marked.size() + (suspects.size() - idx));
        for (u32 m : marked) {
            int x = px(m), y = py(m), z = pz(m);
            if (w.inBounds(x, y, z) && w.solid(x, y, z)) next.push_back(m);
        }
        for (size_t i = idx + 1; i < suspects.size(); i++) {
            u32 m = suspects[i];
            int x = px(m), y = py(m), z = pz(m);
            if (w.inBounds(x, y, z) && w.solid(x, y, z)) next.push_back(m);
        }
    }
    for (u32 m : marked) w.supDirect(px(m), py(m), pz(m)) = SUP_UNREACHED;
    suspects = std::move(next);
}

bool findUnsupportedCluster(VoxWorld& w, int x, int y, int z, int maxVox, Cluster& out) {
    if (!w.inBounds(x, y, z) || !w.solid(x, y, z)) return false;
    if (w.support(x, y, z) != SUP_UNREACHED) return false;
    auto pk = [](int a, int b, int c) { return (u32)a | ((u32)b << 11) | ((u32)c << 20); };
    std::vector<u32> stack, comp, seen;
    stack.push_back(pk(x, y, z));
    seen.push_back(stack[0]);
    int minx = x, miny = y, minz = z, maxx = x, maxy = y, maxz = z;
    while (!stack.empty() && (int)comp.size() < maxVox) {
        u32 p = stack.back(); stack.pop_back();
        comp.push_back(p);
        int cx = (int)(p & 2047), cy = (int)((p >> 11) & 511), cz = (int)((p >> 20) & 4095);
        minx = tmin(minx, cx); miny = tmin(miny, cy); minz = tmin(minz, cz);
        maxx = tmax(maxx, cx); maxy = tmax(maxy, cy); maxz = tmax(maxz, cz);
        for (auto& n : NB6) {
            int nx = cx + n[0], ny = cy + n[1], nz = cz + n[2];
            if (!w.inBounds(nx, ny, nz) || !w.solid(nx, ny, nz)) continue;
            if (w.support(nx, ny, nz) != SUP_UNREACHED) continue;
            u32 np = pk(nx, ny, nz);
            bool dup = false;
            for (u32 s : seen) if (s == np) { dup = true; break; }
            if (dup) continue;
            seen.push_back(np);
            stack.push_back(np);
        }
    }
    if (comp.empty()) return false;
    out.ox = minx; out.oy = miny; out.oz = minz;
    out.w = maxx - minx + 1; out.h = maxy - miny + 1; out.d = maxz - minz + 1;
    out.vox.assign((size_t)out.w * out.h * out.d, Voxel{MAT_AIR, 0, 0});
    for (u32 p : comp) {
        int cx = (int)(p & 2047), cy = (int)((p >> 11) & 511), cz = (int)((p >> 20) & 4095);
        out.vox[(size_t)(cx - minx) + (size_t)(cy - miny) * out.w + (size_t)(cz - minz) * out.w * out.h] = w.get(cx, cy, cz);
    }
    out.count = (int)comp.size();
    return true;
}

} // namespace ft
