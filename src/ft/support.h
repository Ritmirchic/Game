// FRACTURE — structural support: distance-to-anchor field.
//
// Design: support(u16) = graph distance to the nearest anchor (map bedrock,
// VF_FANCHOR cells). UNREACHED (0xFFFF) = no path = unsupported.
// On edits we collect a bounded REGION around the edit (flood through solids,
// stopping at bedrock, limited by hop-distance for soft ground materials),
// reset it to UNREACHED and re-run Dijkstra-style relaxation from the region
// boundary. Unsupported components then fall as fragments.
#pragma once
#include "voxworld.h"

namespace ft {

// A connected set of unsupported voxels, ready to become a dynamic fragment.
struct Cluster {
    int ox = 0, oy = 0, oz = 0;      // world voxel origin (min corner of bbox)
    int w = 0, h = 0, d = 0;
    std::vector<Voxel> vox;          // local w*h*d (unpadded)
    int count = 0;
};

class SupportSystem {
public:
    VoxWorld* world = nullptr;
    int regionCap = 262144;          // max cells collected per recompute
    int softHops = 24;               // expansion hops allowed through soft ground

    void bind(VoxWorld* w);          // allocate scratch
    void computeAll();               // full BFS — after map generation / load
    bool ok() const { return idle(); }
    void drainPending();             // absorb world.pendingSup into edit list
    // synchronous: region recompute for pending edits + cluster extraction
    int process(int budget, std::vector<Cluster>& outClusters);
    bool idle() const { return edits.empty() && suspects.empty() && (world ? world->pendingSup.empty() : true); }

    bool supportedAt(int x, int y, int z) const { return world && world->support(x, y, z) != SUP_UNREACHED; }

private:
    std::vector<u32> edits;          // packed edit positions awaiting recompute
    std::vector<u32> suspects;       // UNREACHED cells waiting for extraction
    std::vector<u16> regionMark;     // per-voxel:0=none,1=region+depth, 2=boundary anchor
    std::vector<u32> regionList;

    static inline u32 pack(int x, int y, int z) { return (u32)x | ((u32)y << 11) | ((u32)z << 20); }
    static inline int px(u32 p) { return (int)(p & 2047); }
    static inline int py(u32 p) { return (int)((p >> 11) & 511); }
    static inline int pz(u32 p) { return (int)((p >> 20) & 4095); }

    void buildRegionAndRecompute();
    void extractSuspects(std::vector<Cluster>& out);
};

// helper for tests: find the unsupported component containing a voxel
bool findUnsupportedCluster(VoxWorld& w, int x, int y, int z, int maxVox, Cluster& out);

inline bool isAnchorAt(const VoxWorld& w, int x, int y, int z) {
    if (y == 0) return w.solid(x, y, z);
    Voxel v = w.get(x, y, z);
    return matSolid(v.mat) && (v.flags & VF_FANCHOR) != 0;
}

} // namespace ft
