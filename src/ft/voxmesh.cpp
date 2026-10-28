// FRACTURE — mesher instantiations
#include "voxmesh.h"

namespace ft {

void meshChunk(const VoxWorld& w, int cx, int cy, int cz, ChunkMesh& out) {
    ChunkSrc src{w, cx, cy, cz};
    out.verts.clear(); out.idx.clear(); out.tverts.clear(); out.tidx.clear();
    greedyMeshPadded(src, CH, CH, CH, out.verts, out.idx, out.tverts, out.tidx);
    const Chunk& c = w.chunks[w.chunkIndex(cx, cy, cz)];
    out.version = c.version;
}

void meshCluster(const ClusterGrid& g, std::vector<u8>& outV, std::vector<u32>& outI,
                 std::vector<u8>& outTV, std::vector<u32>& outTI) {
    greedyMeshPadded(g, g.w, g.h, g.d, outV, outI, outTV, outTI);
}

} // namespace ft
