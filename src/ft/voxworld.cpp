// FRACTURE — voxel world implementation
#include "voxworld.h"

namespace ft {

RayHit VoxWorld::raycast(Vec3 o, Vec3 d, f32 maxT, bool includeGlass) const {
    RayHit h;
    d = norm(d);
    if (W <= 0) return h;

    int x = (int)std::floor(o.x / VOX);
    int y = (int)std::floor(o.y / VOX);
    int z = (int)std::floor(o.z / VOX);

    auto blocks = [&](int cx, int cy, int cz) -> bool {
        if (!inBounds(cx, cy, cz)) return false;
        u8 m = mat(cx, cy, cz);
        if (m == MAT_AIR) return false;
        const MatDef& md = matDef(m);
        if (md.flags & MF_TRANSPARENT) return false;
        if (!includeGlass && (md.flags & MF_GLASS)) return false;
        return (md.flags & MF_SOLID) != 0;
    };

    if (blocks(x, y, z)) {
        h.hit = true; h.t = 0; h.pos = o; h.vx = x; h.vy = y; h.vz = z;
        h.mat = mat(x, y, z);
        h.nrm = Vec3(-std::signbit(d.x) * 1.f, -std::signbit(d.y) * 1.f, -std::signbit(d.z) * 1.f);
        if (h.nrm.x == 0 && h.nrm.y == 0 && h.nrm.z == 0) h.nrm = Vec3(0, 1, 0);
        return h;
    }

    int stepX = d.x > 0 ? 1 : -1;
    int stepY = d.y > 0 ? 1 : -1;
    int stepZ = d.z > 0 ? 1 : -1;

    f32 tDeltaX = d.x != 0 ? std::fabs(VOX / d.x) : 1e30f;
    f32 tDeltaY = d.y != 0 ? std::fabs(VOX / d.y) : 1e30f;
    f32 tDeltaZ = d.z != 0 ? std::fabs(VOX / d.z) : 1e30f;

    auto edge = [](f32 p, f32 dv, int s) -> f32 {
        f32 e = std::floor(p / VOX) * VOX;
        if (s > 0) e += VOX;
        return e;
    };
    f32 tMaxX = d.x != 0 ? (edge(o.x, d.x, stepX) - o.x) / d.x : 1e30f;
    f32 tMaxY = d.y != 0 ? (edge(o.y, d.y, stepY) - o.y) / d.y : 1e30f;
    f32 tMaxZ = d.z != 0 ? (edge(o.z, d.z, stepZ) - o.z) / d.z : 1e30f;

    Vec3 nrm(0, 0, 0);
    f32 t = 0;
    int guard = 0;
    while (t <= maxT && guard++ < 4096) {
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX; t = tMaxX; tMaxX += tDeltaX; nrm = Vec3((f32)-stepX, 0, 0);
        } else if (tMaxY < tMaxZ) {
            y += stepY; t = tMaxY; tMaxY += tDeltaY; nrm = Vec3(0, (f32)-stepY, 0);
        } else {
            z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; nrm = Vec3(0, 0, (f32)-stepZ);
        }
        if (t > maxT) break;
        if (x < -1 || y < -1 || z < -1 || x > W || y > H || z > D) break; // left world bounds with margin
        if (blocks(x, y, z)) {
            h.hit = true; h.t = t;
            h.pos = o + d * t;
            h.vx = x; h.vy = y; h.vz = z;
            h.mat = mat(x, y, z);
            h.nrm = nrm;
            return h;
        }
    }
    return h;
}

std::vector<u8> VoxWorld::serialize() const {
    std::vector<u8> out;
    auto pushU32 = [&](u32 v) { out.push_back((u8)v); out.push_back((u8)(v >> 8)); out.push_back((u8)(v >> 16)); out.push_back((u8)(v >> 24)); };
    out.push_back('F'); out.push_back('T'); out.push_back('W'); out.push_back('1');
    pushU32((u32)W); pushU32((u32)H); pushU32((u32)D);
    // per chunk: RLE over (mat | flags<<8 | col<<16); runs that equal the *template-independent* value
    for (const Chunk& c : chunks) {
        u32 runVal = 0;
        u32 runLen = 0;
        for (int i = 0; i < CH * CH * CH; i++) {
            const Voxel& v = c.vox[i];
            u32 val = (u32)v.mat | ((u32)v.flags << 8) | ((u32)v.col << 16);
            if (runLen > 0 && val == runVal) { runLen++; continue; }
            if (runLen) { pushU32(runVal); pushU32(runLen); }
            runVal = val; runLen = 1;
        }
        if (runLen) { pushU32(runVal); pushU32(runLen); }
    }
    return out;
}

bool VoxWorld::deserialize(const std::vector<u8>& data) {
    size_t p = 0;
    auto readU32 = [&](u32& v) -> bool {
        if (p + 4 > data.size()) return false;
        v = (u32)data[p] | ((u32)data[p + 1] << 8) | ((u32)data[p + 2] << 16) | ((u32)data[p + 3] << 24);
        p += 4; return true;
    };
    if (data.size() < 16) return false;
    if (data[0] != 'F' || data[1] != 'T' || data[2] != 'W' || data[3] != '1') return false;
    p = 4;
    u32 w, h, d;
    if (!readU32(w) || !readU32(h) || !readU32(d)) return false;
    if ((int)w != W || (int)h != H || (int)d != D) return false;
    for (auto& c : chunks) {
        int i = 0;
        while (i < CH * CH * CH) {
            u32 val, len;
            if (!readU32(val) || !readU32(len)) return false;
            if (len == 0 || i + (int)len > CH * CH * CH) return false;
            Voxel v;
            v.mat = (u8)(val & 0xFF);
            v.flags = (u8)((val >> 8) & 0xFF);
            v.col = (u16)((val >> 16) & 0xFFFF);
            for (u32 k = 0; k < len; k++) c.vox[i + k] = v;
            i += (int)len;
        }
        if (!c.dirty) { c.dirty = 1; }
        c.recount();
        c.version++;
    }
    dirtyChunks.clear();
    for (size_t i = 0; i < chunks.size(); i++) if (chunks[i].dirty) dirtyChunks.push_back((u32)i);
    pendingSup.clear();
    return true;
}

} // namespace ft
