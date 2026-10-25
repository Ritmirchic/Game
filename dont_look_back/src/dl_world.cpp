// ============================================================================
//  dl_world.cpp : apartment complex construction, materials, navigation
// ============================================================================
#include "dl_world.h"
#include <map>

namespace dl {
using nf::V3;
using nf::Box3;
using nf::M4;

const char* kMatNames[MAT_COUNT] = {
    "wallpaper_stripe", "wallpaper_damask", "wallpaper_floral", "paint_cream", "paint_green",
    "plaster_bare", "brick", "concrete_wall", "tile_bath", "tile_kitchen", "panel_wood",
    "parquet", "lino_green", "lino_beige", "carpet_red", "concrete_floor", "tile_lobby",
    "stone_floor", "wood_planks", "dirt_floor", "ceiling_stucco", "ceiling_water",
    "wood_dark", "wood_light", "metal_rust", "metal_painted", "metal_steel", "ceramic_white",
    "fabric_sofa", "mattress", "curtain_lace", "fabric_velvet",
    "painting_landscape", "painting_portrait", "painting_abstract", "photo_family",
    "clock_face", "radio_face", "document", "books_row", "door_panel", "door_metal",
    "glass_dirty", "light_fluorescent", "lamp_shade", "cobweb", "exit_sign", "tv_static",
    "paper_old", "skin_pale", "coat_black", "coat_white", "hair_black", "face_dark",
    "shadow", "blood", "wall_sign", "rules", "note_wall",
    "unlit_white", "unlit_black", "graybox"
};

// ------------------------------------------------------------------ materials
struct MatDef {
    const char* tex;
    float spec, gloss, emis, alpha;
    bool unlit, alphaTest, cull;
    uint32_t tint;
};

static const MatDef kMatDefs[MAT_COUNT] = {
    /* WALLPAPER_STRIPE */ {"wallpaper_stripe", 0.05f, 12, 0, 1, false, false, true, 0xFFFFFFFF},
    /* WALLPAPER_DAMASK */ {"wallpaper_damask", 0.05f, 12, 0, 1, false, false, true, 0xFFFFFFFF},
    /* WALLPAPER_FLORAL */ {"wallpaper_floral", 0.05f, 12, 0, 1, false, false, true, 0xFFFFFFFF},
    /* PAINT_CREAM */      {"paint_cream", 0.04f, 10, 0, 1, false, false, true, 0xFFFFFFFF},
    /* PAINT_GREEN */      {"paint_green", 0.06f, 14, 0, 1, false, false, true, 0xFFFFFFFF},
    /* PLASTER_BARE */     {"plaster_bare", 0.02f, 6, 0, 1, false, false, true, 0xFFFFFFFF},
    /* BRICK */            {"brick", 0.03f, 8, 0, 1, false, false, true, 0xFFFFFFFF},
    /* CONCRETE_WALL */    {"concrete_wall", 0.03f, 8, 0, 1, false, false, true, 0xFFFFFFFF},
    /* TILE_BATH */        {"tile_bath", 0.35f, 48, 0, 1, false, false, true, 0xFFFFFFFF},
    /* TILE_KITCHEN */     {"tile_kitchen", 0.32f, 42, 0, 1, false, false, true, 0xFFFFFFFF},
    /* PANEL_WOOD */       {"panel_wood", 0.12f, 22, 0, 1, false, false, true, 0xFFFFFFFF},
    /* PARQUET */          {"parquet", 0.22f, 30, 0, 1, false, false, true, 0xFFFFFFFF},
    /* LINO_GREEN */       {"lino_green", 0.16f, 26, 0, 1, false, false, true, 0xFFFFFFFF},
    /* LINO_BEIGE */       {"lino_beige", 0.14f, 24, 0, 1, false, false, true, 0xFFFFFFFF},
    /* CARPET_RED */       {"carpet_red", 0.02f, 6, 0, 1, false, false, true, 0xFFFFFFFF},
    /* CONCRETE_FLOOR */   {"concrete_floor", 0.05f, 10, 0, 1, false, false, true, 0xFFFFFFFF},
    /* TILE_LOBBY */       {"tile_lobby", 0.42f, 60, 0, 1, false, false, true, 0xFFFFFFFF},
    /* STONE_FLOOR */      {"stone_floor", 0.10f, 18, 0, 1, false, false, true, 0xFFFFFFFF},
    /* WOOD_PLANKS */      {"wood_planks", 0.10f, 20, 0, 1, false, false, true, 0xFFFFFFFF},
    /* DIRT_FLOOR */       {"dirt_floor", 0.01f, 4, 0, 1, false, false, true, 0xFFFFFFFF},
    /* CEILING */          {"ceiling_stucco", 0.02f, 6, 0, 1, false, false, true, 0xFFFFFFFF},
    /* CEILING_WATER */    {"ceiling_water", 0.02f, 6, 0, 1, false, false, true, 0xFFFFFFFF},
    /* WOOD_DARK */        {"wood_dark", 0.14f, 24, 0, 1, false, false, true, 0xFFFFFFFF},
    /* WOOD_LIGHT */       {"wood_light", 0.16f, 26, 0, 1, false, false, true, 0xFFFFFFFF},
    /* METAL_RUST */       {"metal_rust", 0.30f, 40, 0, 1, false, false, true, 0xFFFFFFFF},
    /* METAL_PAINTED */    {"metal_painted", 0.26f, 36, 0, 1, false, false, true, 0xFFFFFFFF},
    /* METAL_STEEL */      {"metal_steel", 0.55f, 90, 0, 1, false, false, true, 0xFFFFFFFF},
    /* CERAMIC */          {"ceramic_white", 0.45f, 70, 0, 1, false, false, true, 0xFFFFFFFF},
    /* FABRIC_SOFA */      {"fabric_sofa", 0.02f, 5, 0, 1, false, false, true, 0xFFFFFFFF},
    /* MATTRESS */         {"mattress", 0.02f, 5, 0, 1, false, false, true, 0xFFFFFFFF},
    /* CURTAIN */          {"curtain_lace", 0.02f, 5, 0, 0.85f, false, true, false, 0xFFFFFFFF},
    /* VELVET */           {"fabric_velvet", 0.06f, 10, 0, 1, false, false, true, 0xFFFFFFFF},
    /* PAINTING_LANDSCAPE */ {"painting_landscape", 0.10f, 30, 0.02f, 1, false, false, true, 0xFFFFFFFF},
    /* PAINTING_PORTRAIT */  {"painting_portrait", 0.10f, 30, 0.02f, 1, false, false, true, 0xFFFFFFFF},
    /* PAINTING_ABSTRACT */  {"painting_abstract", 0.10f, 30, 0.02f, 1, false, false, true, 0xFFFFFFFF},
    /* PHOTO */            {"photo_family", 0.05f, 12, 0.02f, 1, false, false, true, 0xFFFFFFFF},
    /* CLOCK */            {"clock_face", 0.25f, 50, 0.0f, 1, false, false, true, 0xFFFFFFFF},
    /* RADIO */            {"radio_face", 0.20f, 30, 0.03f, 1, false, false, true, 0xFFFFFFFF},
    /* DOCUMENT */         {"document", 0.02f, 6, 0.03f, 1, false, false, true, 0xFFFFFFFF},
    /* BOOKS */            {"books_row", 0.05f, 10, 0, 1, false, false, true, 0xFFFFFFFF},
    /* DOOR_PANEL */       {"door_panel", 0.16f, 26, 0, 1, false, false, true, 0xFFFFFFFF},
    /* DOOR_METAL */       {"door_metal", 0.30f, 40, 0, 1, false, false, true, 0xFFFFFFFF},
    /* GLASS */            {"glass_dirty", 0.80f, 120, 0, 0.35f, false, false, false, 0xFFFFFFFF},
    /* LIGHTBASE */        {"light_fluorescent", 0.20f, 30, 0.42f, 1, false, false, true, 0xFFE8DFC8},
    /* LAMPSHADE */        {"lamp_shade", 0.05f, 10, 0.34f, 1, false, false, false, 0xFFE0CFA8},
    /* COBWEB */           {"cobweb", 0.05f, 10, 0.0f, 0.5f, false, true, false, 0xFFFFFFFF},
    /* EXIT_SIGN */        {"exit_sign", 0.20f, 30, 1.4f, 1, false, false, true, 0xFFFFFFFF},
    /* TV_STATIC */        {"tv_static", 0.30f, 60, 0.9f, 1, false, false, true, 0xFFFFFFFF},
    /* PAPER */            {"paper_old", 0.02f, 6, 0.03f, 1, false, false, true, 0xFFFFFFFF},
    /* SKIN */             {"skin_pale", 0.06f, 12, 0, 1, false, false, true, 0xFFFFFFFF},
    /* COAT_BLACK */       {"coat_black", 0.04f, 8, 0.055f, 1, false, false, true, 0xFFFFFFFF},
    /* COAT_WHITE */       {"coat_white", 0.04f, 8, 0.045f, 1, false, false, true, 0xFFFFFFFF},
    /* HAIR */             {"hair_black", 0.10f, 20, 0, 1, false, false, true, 0xFFFFFFFF},
    /* FACE */             {"face_dark", 0.02f, 4, 0, 1, false, false, true, 0xFFFFFFFF},
    /* SHADOW */           {"shadow", 0.0f, 1, 0, 0.55f, true, true, false, 0xFFFFFFFF},
    /* BLOOD */            {"blood", 0.25f, 40, 0, 1, false, true, false, 0xFFFFFFFF},
    /* WALL_SIGN */        {"wall_sign", 0.06f, 14, 0.05f, 1, false, false, true, 0xFFFFFFFF},
    /* RULES */            {"rules", 0.05f, 10, 0.03f, 1, false, false, true, 0xFFFFFFFF},
    /* NOTE */             {"note_wall", 0.03f, 8, 0.03f, 1, false, false, true, 0xFFFFFFFF},
    /* UNLIT_WHITE */      {"", 0, 1, 0.85f, 1, true, false, false, 0xFFF6EEDC},
    /* UNLIT_BLACK */      {"", 0, 1, 0.0f, 1, true, false, false, 0xFF000000},
    /* GRAYBOX */          {"", 0, 1, 0.0f, 1, false, false, false, 0xFF1A1A1C},
};

void MatLib::build(nf::Renderer& r, const std::string& texDir) {
    mats.assign(MAT_COUNT, nf::Material());
    for (int i = 0; i < MAT_COUNT; ++i) {
        const MatDef& d = kMatDefs[i];
        nf::Material m;
        if (d.tex[0]) {
            nf::Image img;
            std::string p = nf::joinPath(texDir, std::string(d.tex) + ".png");
            if (nf::pngLoad(p.c_str(), img)) {
                m.tex = r.textures.add(img);
            } else {
                nf::logMsg("[materials] missing texture %s", p.c_str());
            }
        }
        m.tint = d.tint;
        m.spec = d.spec;
        m.gloss = d.gloss;
        m.emis = d.emis;
        m.alpha = d.alpha;
        m.unlit = d.unlit;
        m.alphaTest = d.alphaTest;
        m.cull = d.cull;
        m.castShadow = (d.alpha >= 0.9f);
        m.receiveShadow = true;
        mats[i] = m;
    }
}

// ------------------------------------------------------------------ nav grid
void NavGrid::init(const V3& lo, const V3& hi, float cellSize) {
    cell = cellSize;
    origin = lo;
    nx = nf::maxi(1, (int)std::ceil((hi.x - lo.x) / cell));
    ny = nf::maxi(1, (int)std::ceil((hi.y - lo.y) / cell));
    nz = nf::maxi(1, (int)std::ceil((hi.z - lo.z) / cell));
    blocked.assign((size_t)nx * ny * nz, 0);
}

void NavGrid::setBox(const Box3& b, bool block, int expand) {
    float e = expand * cell;
    int x0 = (int)std::floor((b.lo.x - e - origin.x) / cell);
    int x1 = (int)std::ceil((b.hi.x + e - origin.x) / cell);
    int y0 = (int)std::floor((b.lo.y - e - origin.y) / cell);
    int y1 = (int)std::ceil((b.hi.y + e - origin.y) / cell);
    int z0 = (int)std::floor((b.lo.z - e - origin.z) / cell);
    int z1 = (int)std::ceil((b.hi.z + e - origin.z) / cell);
    for (int z = nf::maxi(0, z0); z < nf::mini(nz, z1); ++z)
        for (int y = nf::maxi(0, y0); y < nf::mini(ny, y1); ++y)
            for (int x = nf::maxi(0, x0); x < nf::mini(nx, x1); ++x)
                blocked[(size_t)index(x, y, z)] = block ? 1 : 0;
}

bool NavGrid::cellOf(const V3& p, int& x, int& y, int& z) const {
    x = (int)std::floor((p.x - origin.x) / cell);
    y = (int)std::floor((p.y - origin.y) / cell);
    z = (int)std::floor((p.z - origin.z) / cell);
    return valid(x, y, z);
}

V3 NavGrid::cellCenter(int x, int y, int z) const {
    return V3(origin.x + (x + 0.5f) * cell, origin.y + (y + 0.5f) * cell, origin.z + (z + 0.5f) * cell);
}

bool NavGrid::pointWalkable(const V3& p) const {
    int x, y, z;
    if (!cellOf(p, x, y, z)) return false;
    return walkable(x, y, z);
}

bool NavGrid::rayWalk(const V3& a, const V3& b, int samples) const {
    float dist = nf::length(b - a);
    if (samples <= 0) samples = nf::maxi(2, (int)(dist / (cell * 0.5f)));
    for (int i = 0; i <= samples; ++i) {
        V3 p = nf::lerp(a, b, (float)i / samples);
        if (!pointWalkable(p)) return false;
    }
    return true;
}

bool NavGrid::nearestWalkable(const V3& p, V3& out) const {
    int cx, cy, cz;
    if (!cellOf(p, cx, cy, cz)) {
        cx = nf::clampi((int)((p.x - origin.x) / cell), 0, nx - 1);
        cy = nf::clampi((int)((p.y - origin.y) / cell), 0, ny - 1);
        cz = nf::clampi((int)((p.z - origin.z) / cell), 0, nz - 1);
    }
    for (int r = 0; r < nf::maxi(nx, ny); ++r) {
        for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            if (nf::maxi(std::abs(dx), std::abs(dy)) != r) continue;
            int x = cx + dx, y = cy + dy, z = cz + dz;
            if (!walkable(x, y, z)) continue;
            out = cellCenter(x, y, z);
            return true;
        }
    }
    return false;
}

// cells inside the stair shaft: the only place where the walk layer may change
static bool navStairColumn(const NavGrid& n, int x, int y) {
    V3 c = n.cellCenter(x, y, 0);
    return c.x > -8.4f && c.x < -4.0f && c.y > 8.3f && c.y < 11.0f;
}

bool NavGrid::findPath(const V3& from, const V3& to, std::vector<V3>& out, int maxNodes) const {
    out.clear();
    int sx, sy, sz, ex, ey, ez;
    if (!cellOf(from, sx, sy, sz) || !cellOf(to, ex, ey, ez)) return false;
    if (!walkable(sx, sy, sz)) {
        V3 n;
        if (!nearestWalkable(from, n)) return false;
        cellOf(n, sx, sy, sz);
    }
    // the walk layer is taken from the start; the goal is projected onto it so that
    // rooms on the same floor always connect, even if their probes differ slightly in Z
    ez = sz;
    if (!walkable(ex, ey, ez)) {
        V3 n;
        if (!nearestWalkable(V3(to.x, to.y, cellCenter(ex, ey, ez).z), n)) return false;
        cellOf(n, ex, ey, ez);
        ez = sz;
        if (!walkable(ex, ey, ez)) return false;
    }
    if (sx == ex && sy == ey && sz == ez) { out.push_back(to); return true; }
    struct Node { float f; int idx; };
    const size_t total = (size_t)nx * ny * nz;
    static thread_local std::vector<float> gScore;
    static thread_local std::vector<int> came;
    static thread_local std::vector<uint8_t> closed;
    gScore.assign(total, 1e30f);
    came.assign(total, -1);
    closed.assign(total, 0);
    auto heur = [&](int x, int y) {
        int dx = std::abs(x - ex), dy = std::abs(y - ey);
        return (float)(dx + dy);
    };
    std::vector<Node> heap;
    auto push = [&](Node n) {
        heap.push_back(n);
        size_t i = heap.size() - 1;
        while (i > 0) {
            size_t p = (i - 1) / 2;
            if (heap[p].f <= heap[i].f) break;
            std::swap(heap[p], heap[i]);
            i = p;
        }
    };
    auto pop = [&]() {
        Node top = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        size_t i = 0;
        while (true) {
            size_t l = i * 2 + 1, r = l + 1, best = i;
            if (l < heap.size() && heap[l].f < heap[best].f) best = l;
            if (r < heap.size() && heap[r].f < heap[best].f) best = r;
            if (best == i) break;
            std::swap(heap[best], heap[i]);
            i = best;
        }
        return top;
    };
    int startIdx = index(sx, sy, sz), goalIdx = index(ex, ey, ez);
    gScore[startIdx] = 0;
    push({heur(sx, sy), startIdx});
    int expanded = 0;
    bool found = false;
    const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    while (!heap.empty() && expanded < maxNodes) {
        Node cur = pop();
        if (closed[cur.idx]) continue;
        closed[cur.idx] = 1;
        ++expanded;
        if (cur.idx == goalIdx) { found = true; break; }
        int cz = cur.idx / (nx * ny);
        int rem = cur.idx % (nx * ny);
        int cy = rem / nx, cx = rem % nx;
        for (auto& d : dirs) {
            int x = cx + d[0], y = cy + d[1], z = cz;
            if (!walkable(x, y, z)) continue;
            if (d[0] && d[1] && (!walkable(cx + d[0], cy, cz) || !walkable(cx, cy + d[1], cz))) continue;
            int ni = index(x, y, z);
            if (closed[ni]) continue;
            float step = (d[0] && d[1]) ? 1.4142f : 1.0f;
            float ng = gScore[cur.idx] + step;
            if (ng < gScore[ni]) {
                gScore[ni] = ng;
                came[ni] = cur.idx;
                push({ng + heur(x, y), ni});
            }
        }
        // vertical links exist only inside the stair shaft
        if (navStairColumn(*this, cx, cy)) {
            for (int dz = -1; dz <= 1; dz += 2) {
                int z = cz + dz;
                if (!walkable(cx, cy, z)) continue;
                int ni = index(cx, cy, z);
                if (closed[ni]) continue;
                float ng = gScore[cur.idx] + 2.5f;
                if (ng < gScore[ni]) {
                    gScore[ni] = ng;
                    came[ni] = cur.idx;
                    push({ng + heur(cx, cy), ni});
                }
            }
        }
    }
    if (!found) return false;
    std::vector<int> chain;
    int c = goalIdx;
    while (c >= 0) {
        chain.push_back(c);
        if (c == startIdx) break;
        c = came[c];
    }
    std::reverse(chain.begin(), chain.end());
    for (int idx : chain) {
        int z = idx / (nx * ny);
        int rem = idx % (nx * ny);
        int y = rem / nx, x = rem % nx;
        out.push_back(cellCenter(x, y, z));
    }
    // path smoothing (same layer only)
    if (out.size() > 2) {
        std::vector<V3> sm;
        sm.push_back(out.front());
        size_t i = 0;
        while (i + 1 < out.size()) {
            size_t j = out.size() - 1;
            for (; j > i + 1; --j) {
                if (std::fabs(out[i].z - out[j].z) > 0.5f) continue;
                if (rayWalk(out[i], out[j])) break;
            }
            sm.push_back(out[j]);
            i = j;
        }
        out.swap(sm);
    }
    return true;
}

// ------------------------------------------------------------------ World basics
int World::beginGroup(int mat, const std::string&) {
    Group g;
    g.mat = mat;
    g.id = (int)groups.size();
    groups.push_back(std::move(g));
    curGroup_ = (int)groups.size() - 1;
    return curGroup_;
}

void World::endGroup(int) { curGroup_ = -1; }

void World::setGroupVisible(int groupIdx, bool vis) {
    if (groupIdx >= 0 && groupIdx < (int)groups.size()) groups[groupIdx].visible = vis;
}

void World::addCollider(const Box3& b, int owner, bool sight) {
    Collider c;
    c.box = b;
    c.ownerId = owner;
    c.blocksSight = sight;
    colliders.push_back(c);
}

int World::addDoor(const std::string& id, const V3& hinge, const V3& axis, float width,
                   float height, int swing, const V3& normal, const std::string& lockedHint) {
    Door d;
    d.id = id;
    d.index = (int)doors.size();
    d.hinge = hinge;
    d.axis = nf::normalize(V3(axis.x, axis.y, 0));
    d.width = width;
    d.height = height;
    d.swing = swing;
    d.normal = nf::normalize(V3(normal.x, normal.y, 0));
    d.lockedHint = lockedHint;
    doors.push_back(d);
    rebuildDoorBox((int)doors.size() - 1);
    return (int)doors.size() - 1;
}

void World::rebuildDoorBox(int doorIdx) {
    if (doorIdx < 0 || doorIdx >= (int)doors.size()) return;
    Door& d = doors[doorIdx];
    V3 dir = d.axis * std::cos(d.openness) + d.normal * (std::sin(d.openness) * d.swing);
    V3 a = d.hinge;
    V3 b = d.hinge + dir * d.width;
    Box3 bx;
    bx.lo = V3(nf::minf(a.x, b.x) - 0.04f, nf::minf(a.y, b.y) - 0.04f, d.hinge.z);
    bx.hi = V3(nf::maxf(a.x, b.x) + 0.04f, nf::maxf(a.y, b.y) + 0.04f, d.hinge.z + d.height);
    d.closedBox = bx;
    // update the dynamic collider
    for (auto& c : colliders) {
        if (c.doorId == doorIdx) {
            c.box = bx;
            c.blocksWalk = d.openness < 0.62f && d.state != DoorState::Open;
            return;
        }
    }
    Collider c;
    c.box = bx;
    c.doorId = doorIdx;
    c.blocksWalk = d.openness < 0.62f && d.state != DoorState::Open;
    c.blocksSight = false;
    colliders.push_back(c);
}

void World::addInteract(const Interactable& it) { interactables.push_back(it); }

const nf::Mesh& World::doorLeaf(float w, float h) {
    int key = (int)(w * 100.0f) * 10000 + (int)(h * 100.0f);
    auto it = leafCache.find(key);
    if (it != leafCache.end()) return it->second;
    GeoBuilder g;
    g.mat = MAT_DOOR_PANEL;
    g.uvScale = 1.0f;
    float th = 0.045f;
    // leaf slab (local +x along width, +z up, thickness about y)
    g.box(Box3(V3(0.008f, -th * 0.5f, 0.008f), V3(w - 0.008f, th * 0.5f, h - 0.008f)), true, true);
    // raised panels
    g.mat = MAT_WOOD_DARK;
    g.uvScale = 0.8f;
    g.box(Box3(V3(0.10f, th * 0.5f, 0.35f), V3(w - 0.10f, th * 0.5f + 0.012f, h * 0.52f)), true, false);
    g.box(Box3(V3(0.10f, -th * 0.5f - 0.012f, 0.35f), V3(w - 0.10f, -th * 0.5f, h * 0.52f)), true, false);
    // handle
    g.mat = MAT_METAL_STEEL;
    g.uvScale = 0.3f;
    g.cylinder(V3(w * 0.86f, th * 0.5f, 1.02f), 0.018f, 0.09f, 8);
    g.cylinder(V3(w * 0.86f, -th * 0.5f - 0.09f, 1.02f), 0.018f, 0.09f, 8);
    g.box(Box3(V3(w * 0.86f - 0.012f, th * 0.5f, 0.98f), V3(w * 0.86f + 0.012f, th * 0.5f + 0.10f, 1.02f)), true, true);
    g.finish();
    auto res = leafCache.emplace(key, std::move(g.mesh));
    return res.first->second;
}

nf::M4 World::doorTransform(const Door& d) const {
    float baseAng = std::atan2(d.axis.y, d.axis.x);
    float ang = baseAng + (float)d.swing * d.openness * (nf::PI * 0.46f);
    return nf::M4::translate(d.hinge) * nf::M4::rotZ(ang);
}

void World::buildDoorFrame(int doorIdx, int wallMat) {
    if (doorIdx < 0 || doorIdx >= (int)doors.size()) return;
    Door& d = doors[doorIdx];
    (void)wallMat;
    int gi = beginGroup(MAT_WOOD_DARK, "doorframe");
    GeoBuilder g;
    g.mat = MAT_WOOD_DARK;
    g.uvScale = 0.8f;
    props::doorFrame(g, d.hinge, d.axis, d.normal, d.width, d.height);
    // threshold strip
    g.mat = MAT_METAL_STEEL;
    g.uvScale = 1.0f;
    V3 n = d.normal;
    V3 th = n * 0.10f;
    g.box(Box3(vmin(d.hinge - th, d.hinge + d.axis * d.width - th) + V3(0, 0, 0.0f),
               vmax(d.hinge + th, d.hinge + d.axis * d.width + th) + V3(0, 0, 0.012f)), true, true);
    g.finish();
    groups[gi].mesh = std::move(g.mesh);
}

RoomInfo* World::roomAt(const V3& p) {
    for (auto& r : rooms) {
        Box3 b = r.bounds;
        b.grow(0.05f);
        if (b.contains(p)) return &r;
    }
    return nullptr;
}

RoomInfo* World::roomById(const std::string& id) {
    for (auto& r : rooms) if (r.id == id) return &r;
    return nullptr;
}

Door* World::doorById(const std::string& id) {
    for (auto& d : doors) if (d.id == id) return &d;
    return nullptr;
}

int World::doorIndex(const std::string& id) {
    for (size_t i = 0; i < doors.size(); ++i) if (doors[i].id == id) return (int)i;
    return -1;
}

World::PropRef* World::prop(const std::string& id) {
    for (auto& p : props) if (p.id == id) return &p;
    return nullptr;
}

int World::addDocument(const std::string& id, const std::string& title, const std::string& body,
                       const V3& pos, const std::string& room) {
    Document d;
    d.id = id;
    d.title = title;
    d.body = body;
    d.pos = pos;
    d.room = room;
    documents.push_back(d);
    return (int)documents.size() - 1;
}

int World::addProp(const std::string& id, const V3& pos, const Box3& collider, const std::string& room) {
    PropRef p;
    p.id = id;
    p.pos = pos;
    p.collider = collider;
    p.room = room;
    props.push_back(p);
    return (int)props.size() - 1;
}

void World::addLight(const V3& pos, const V3& color, float intensity, float range,
                     const std::string& room, bool emergency) {
    LightFixture f;
    f.pos = pos;
    f.color = color;
    f.baseIntensity = intensity;
    f.range = range;
    f.roomId = room;
    f.emergency = emergency;
    f.phase = (float)(nf::hashf((uint32_t)(pos.x * 91 + pos.y * 37 + pos.z * 13)) * 6.28f);
    fixtures.push_back(f);
}

void World::finalizeDoors() {
    for (auto& d : doors) buildDoorFrame(d.index, MAT_WOOD_DARK);
}

// ------------------------------------------------------------------ wall helpers
static void buildAABB(GeoBuilder& g, const Box3& b, bool top, bool bottom) { g.box(b, top, bottom); }

void World::wallSegment(const V3& a, const V3& b, float z0, float z1, int mat,
                        float thickness, bool bothSides) {
    (void)bothSides;
    int gi = beginGroup(mat, "wall");
    GeoBuilder g;
    g.mat = mat;
    g.uvScale = 1.35f;
    V3 lo(nf::minf(a.x, b.x) - thickness * 0.5f, nf::minf(a.y, b.y) - thickness * 0.5f, z0);
    V3 hi(nf::maxf(a.x, b.x) + thickness * 0.5f, nf::maxf(a.y, b.y) + thickness * 0.5f, z1);
    if (std::fabs(a.x - b.x) < 0.001f) { lo.x = a.x - thickness * 0.5f; hi.x = a.x + thickness * 0.5f; }
    if (std::fabs(a.y - b.y) < 0.001f) { lo.y = a.y - thickness * 0.5f; hi.y = a.y + thickness * 0.5f; }
    g.box(Box3(lo, hi), true, true);
    g.finish();
    groups[gi].mesh = std::move(g.mesh);
    addCollider(Box3(lo, hi), gi);
}

void World::floorSlab(const Box3& b, int floorMat, int ceilMat) {
    {
        int gi = beginGroup(floorMat, "floor");
        GeoBuilder g;
        g.mat = floorMat;
        g.uvScale = 1.7f;
        Box3 fl(V3(b.lo.x, b.lo.y, b.lo.z - 0.16f), V3(b.hi.x, b.hi.y, b.lo.z));
        g.box(fl, true, false);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(b.lo.x, b.lo.y, b.lo.z - 0.6f), V3(b.hi.x, b.hi.y, b.lo.z)), gi);
    }
    if (b.hi.z > b.lo.z) {
        int gi = beginGroup(ceilMat, "ceil");
        GeoBuilder g;
        g.mat = ceilMat;
        g.uvScale = 1.7f;
        Box3 ce(V3(b.lo.x, b.lo.y, b.hi.z), V3(b.hi.x, b.hi.y, b.hi.z + 0.16f));
        g.box(ce, false, true);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(b.lo.x, b.lo.y, b.hi.z), V3(b.hi.x, b.hi.y, b.hi.z + 0.6f)), gi);
    }
}

// ------------------------------------------------------------------ local frame
struct Frame {
    V3 o;
    float yaw;
    V3 p(float x, float y, float z) const {
        float c = std::cos(yaw), s = std::sin(yaw);
        return V3(o.x + x * c - y * s, o.y + x * s + y * c, o.z + z);
    }
    V3 dir(float x, float y) const {
        float c = std::cos(yaw), s = std::sin(yaw);
        return V3(x * c - y * s, x * s + y * c, 0);
    }
};

// ============================================================================
//  MAIN CORRIDOR + WINGS
// ============================================================================
void World::buildMainCorridor() {
    const float z0 = 0.0f, zh = 2.85f;
    const int WALL = MAT_WALLPAPER_STRIPE, FLOOR = MAT_LINO_GREEN, CEIL = MAT_CEILING;

    // floor & ceiling
    floorSlab(Box3(V3(-8.8f, -0.2f, z0), V3(8.8f, 2.4f, zh)), FLOOR, CEIL);

    auto wallX = [&](float y, float xa, float xb, int mat) {
        if (xb - xa < 0.02f) return;
        wallSegment(V3(xa, y, z0), V3(xb, y, z0), z0, zh, mat, 0.2f);
    };
    auto wallY = [&](float x, float ya, float yb, int mat) {
        if (yb - ya < 0.02f) return;
        wallSegment(V3(x, ya, z0), V3(x, yb, z0), z0, zh, mat, 0.2f);
    };

    // ---- south wall (y = 0): doors to apt303 (x -3.25..-2.15) and apt304 (x 6.95..8.05)
    wallX(0.0f, -8.8f, -3.25f, WALL);
    wallX(0.0f, -2.15f, -1.1f, WALL);
    wallX(0.0f, 1.1f, 6.95f, WALL);
    wallX(0.0f, 8.05f, 8.8f, WALL);
    for (auto sp : {std::pair<float, float>{-3.25f, -2.15f}, {6.95f, 8.05f}}) {
        int gi = beginGroup(WALL, "header");
        GeoBuilder g;
        g.mat = WALL;
        g.box(Box3(V3(sp.first, -0.10f, 2.06f), V3(sp.second, 0.10f, zh)), true, true);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
    }
    // ---- north wall (y = 2.2): doors to apt305 (x -8.05..-6.95) and apt306 (x 1.85..2.95)
    wallX(2.2f, -8.8f, -8.05f, WALL);
    wallX(2.2f, -6.95f, -1.1f, WALL);
    wallX(2.2f, 1.1f, 1.85f, WALL);
    wallX(2.2f, 2.95f, 8.8f, WALL);
    for (auto sp : {std::pair<float, float>{-8.05f, -6.95f}, {1.85f, 2.95f}}) {
        int gi = beginGroup(WALL, "header");
        GeoBuilder g;
        g.mat = WALL;
        g.box(Box3(V3(sp.first, 2.10f, 2.06f), V3(sp.second, 2.30f, zh)), true, true);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
    }
    // ---- end walls with apartment doors (apt47 west, apt308 east)
    wallY(-8.8f, 0.0f, 0.55f, WALL);
    wallY(-8.8f, 1.65f, 2.2f, WALL);
    wallY(8.8f, 0.0f, 0.55f, WALL);
    wallY(8.8f, 1.65f, 2.2f, WALL);
    for (float x : {-8.8f, 8.8f}) {
        int gi = beginGroup(WALL, "header");
        GeoBuilder g;
        g.mat = WALL;
        g.box(Box3(V3(x - 0.10f, 0.55f, 2.06f), V3(x + 0.10f, 1.65f, zh)), true, true);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
    }

    // ---- doors (hinges aligned with each apartment's entrance opening)
    addDoor("apt47", V3(-8.8f, 0.55f, 0.0f), V3(0, 1, 0), 1.10f, 2.06f, -1, V3(1, 0, 0), "");
    addDoor("apt308", V3(8.8f, 0.55f, 0.0f), V3(0, 1, 0), 1.10f, 2.06f, 1, V3(-1, 0, 0), "");
    addDoor("apt305", V3(-8.05f, 2.2f, 0.0f), V3(1, 0, 0), 1.10f, 2.06f, 1, V3(0, -1, 0), "");
    addDoor("apt306", V3(1.85f, 2.2f, 0.0f), V3(1, 0, 0), 1.10f, 2.06f, 1, V3(0, -1, 0), "");
    addDoor("apt303", V3(-3.25f, 0.0f, 0.0f), V3(1, 0, 0), 1.10f, 2.06f, -1, V3(0, 1, 0), "");
    addDoor("apt304", V3(6.95f, 0.0f, 0.0f), V3(1, 0, 0), 1.10f, 2.06f, -1, V3(0, 1, 0), "");
    // the player's apartment door starts open behind them
    if (Door* d = doorById("apt47")) { d->openness = 0.65f; d->state = DoorState::Ajar; }

    // ---- baseboards + picture rail accents along the corridor
    {
        int gi = beginGroup(MAT_WOOD_DARK, "baseboard");
        GeoBuilder g;
        g.mat = MAT_WOOD_DARK;
        g.uvScale = 0.5f;
        g.box(Box3(V3(-8.6f, 0.0f, 0.0f), V3(8.6f, 0.05f, 0.11f)), true, false);
        g.box(Box3(V3(-8.6f, 2.15f, 0.0f), V3(8.6f, 2.2f, 0.11f)), true, false);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
    }

    // corridor lighting: two working fixtures, others dead or weak
    addLight(V3(-6.2f, 1.1f, zh - 0.12f), V3(1.0f, 0.84f, 0.62f), 1.15f, 6.2f, "corridor_main");
    addLight(V3(-2.0f, 1.1f, zh - 0.12f), V3(1.0f, 0.84f, 0.62f), 0.0f, 6.2f, "corridor_main");
    addLight(V3(2.0f, 1.1f, zh - 0.12f), V3(1.0f, 0.86f, 0.68f), 0.85f, 6.2f, "corridor_main");
    addLight(V3(6.2f, 1.1f, zh - 0.12f), V3(1.0f, 0.84f, 0.62f), 1.05f, 6.2f, "corridor_main");
    addLight(V3(0.0f, 4.4f, zh - 0.12f), V3(0.94f, 0.88f, 0.72f), 0.95f, 5.4f, "corridor_north");
    {
        int lgi = beginGroup(MAT_LIGHTBASE, "corridor_lights");
        GeoBuilder lg;
        lg.mat = MAT_LIGHTBASE;
        for (float x : {-6.2f, -2.0f, 2.0f, 6.2f}) props::ceilingLight(lg, V3(x, 1.1f, zh), false);
        props::ceilingLight(lg, V3(0.0f, 4.4f, zh), false);
        props::ceilingLight(lg, V3(0.0f, -2.4f, zh), true);
        lg.finish();
        groups[lgi].mesh = std::move(lg.mesh);
    }
    rooms.push_back({"corridor_main", "Main corridor", Box3(V3(-8.8f, 0.0f, 0.0f), V3(8.8f, 2.2f, 2.85f)),
                     V3(0, 1.1f, 1.7f), 3, true});
    rooms.push_back({"corridor_north", "North corridor", Box3(V3(-1.1f, 2.2f, 0.0f), V3(1.1f, 7.6f, 2.85f)),
                     V3(0, 4.8f, 1.7f), 3, true});
    rooms.push_back({"corridor_south", "South corridor", Box3(V3(-1.1f, -7.6f, 0.0f), V3(1.1f, 0.0f, 2.85f)),
                     V3(0, -3.5f, 1.7f), 3, true});
}

// ------------------------------------------------------------------ north/south wings
void World::buildFixturesAndDetails() {
    const float z0 = 0.0f, zh = 2.85f;
    const int WALL = MAT_WALLPAPER_STRIPE;

    // ---- north corridor (x -1.1..1.1, y 2.2..7.6)
    floorSlab(Box3(V3(-1.1f, 2.2f, z0), V3(1.1f, 7.6f, zh)), MAT_LINO_GREEN, MAT_CEILING);
    wallSegment(V3(-1.1f, 2.2f, 0), V3(-1.1f, 7.6f, 0), z0, zh, WALL, 0.2f);
    wallSegment(V3(1.1f, 2.2f, 0), V3(1.1f, 7.6f, 0), z0, zh, WALL, 0.2f);

    // ---- south corridor (x -1.1..1.1, y -7.6..0)
    floorSlab(Box3(V3(-1.1f, -7.6f, z0), V3(1.1f, 0.0f, zh)), MAT_LINO_GREEN, MAT_CEILING);
    wallSegment(V3(-1.1f, -7.6f, 0), V3(-1.1f, 0.0f, 0), z0, zh, WALL, 0.2f);
    wallSegment(V3(1.1f, -7.6f, 0), V3(1.1f, 0.0f, 0), z0, zh, WALL, 0.2f);
    // end wall with the storage door
    wallSegment(V3(-1.1f, -7.6f, 0), V3(-0.4f, -7.6f, 0), z0, zh, WALL, 0.2f);
    wallSegment(V3(0.5f, -7.6f, 0), V3(1.1f, -7.6f, 0), z0, zh, WALL, 0.2f);
    {
        int gi = beginGroup(WALL, "header");
        GeoBuilder g;
        g.mat = WALL;
        g.box(Box3(V3(-0.5f, -7.7f, 2.06f), V3(0.6f, -7.5f, zh)), true, true);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
    }
    addDoor("storage2", V3(-0.45f, -7.6f, 0.0f), V3(1, 0, 0), 0.95f, 2.04f, 1, V3(0, -1, 0), "");

    // storage 2 room (south)
    {
        float y0 = -10.6f, y1 = -7.8f, x0 = -1.3f, x1 = 3.1f;
        floorSlab(Box3(V3(x0, y0, z0), V3(x1, y1, 2.6f)), MAT_CONCRETE_FLOOR, MAT_CEILING);
        wallSegment(V3(x0, y0, 0), V3(x1, y0, 0), z0, 2.6f, MAT_PLASTER_BARE, 0.2f);
        wallSegment(V3(x0, y0, 0), V3(x0, y1, 0), z0, 2.6f, MAT_PLASTER_BARE, 0.2f);
        wallSegment(V3(x1, y0, 0), V3(x1, y1, 0), z0, 2.6f, MAT_PLASTER_BARE, 0.2f);
        wallSegment(V3(x0, y1, 0), V3(-0.45f, y1, 0), z0, 2.6f, MAT_PLASTER_BARE, 0.2f);
        wallSegment(V3(0.6f, y1, 0), V3(x1, y1, 0), z0, 2.6f, MAT_PLASTER_BARE, 0.2f);
        rooms.push_back({"storage2", "Store room 2", Box3(V3(x0, y0, 0), V3(x1, y1, 2.6f)),
                         V3(1, -9, 1.6f), 3, true});
        // shelving and crates
        int gi = beginGroup(MAT_WOOD_PLANKS, "storage_shelf");
        GeoBuilder g;
        g.mat = MAT_WOOD_PLANKS;
        props::crate(g, V3(0.4f, -9.6f, 0), 0.3f, 0.7f);
        props::crate(g, V3(1.3f, -9.9f, 0), 1.1f, 0.55f);
        props::crate(g, V3(1.1f, -9.8f, 0.55f), 0.7f, 0.45f);
        props::crate(g, V3(2.4f, -8.6f, 0), 0.2f, 0.8f);
        props::barrel(g, V3(2.5f, -10.0f, 0), 0, 0.28f, 0.85f);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(0.05f, -10.0f, 0), V3(0.75f, -9.25f, 0.7f)), gi);
        addCollider(Box3(V3(1.0f, -10.2f, 0), V3(1.65f, -9.6f, 0.55f)), gi);
        addCollider(Box3(V3(2.0f, -8.95f, 0), V3(2.8f, -8.25f, 0.8f)), gi);
        addCollider(Box3(V3(2.2f, -10.3f, 0), V3(2.8f, -9.7f, 0.85f)), gi);
        addLight(V3(1.0f, -9.2f, 2.2f), V3(1.0f, 0.82f, 0.6f), 0.8f, 5.0f, "storage2");
    }

    // ---- stair lobby
    {
        float x0 = -4.6f, x1 = 4.6f, y0 = 7.6f, y1 = 11.0f;
        floorSlab(Box3(V3(x0, y0, z0), V3(x1, y1, zh)), MAT_TILE_LOBBY, MAT_CEILING);
        // west wall (opening to stairwell)
        wallSegment(V3(x0, y0, 0), V3(x0, 8.6f, 0), z0, zh, WALL, 0.2f);
        wallSegment(V3(x0, 10.4f, 0), V3(x0, y1, 0), z0, zh, WALL, 0.2f);
        {
            int gi = beginGroup(WALL, "header");
            GeoBuilder g;
            g.mat = WALL;
            g.box(Box3(V3(x0 - 0.1f, 8.6f, 2.20f), V3(x0 + 0.1f, 10.4f, zh)), true, true);
            g.finish();
            groups[gi].mesh = std::move(g.mesh);
        }
        // east wall with elevator + maintenance door
        wallSegment(V3(x1, y0, 0), V3(x1, 8.5f, 0), z0, zh, WALL, 0.2f);
        wallSegment(V3(x1, 10.2f, 0), V3(x1, y1, 0), z0, zh, WALL, 0.2f);
        {
            int gi = beginGroup(WALL, "header");
            GeoBuilder g;
            g.mat = WALL;
            g.box(Box3(V3(x1 - 0.1f, 8.5f, 2.20f), V3(x1 + 0.1f, 10.2f, zh)), true, true);
            g.finish();
            groups[gi].mesh = std::move(g.mesh);
        }
        // north wall with exit landing door
        wallSegment(V3(x0, y1, 0), V3(-2.4f, y1, 0), z0, zh, MAT_PAINT_GREEN, 0.2f);
        wallSegment(V3(-1.0f, y1, 0), V3(x1, y1, 0), z0, zh, MAT_PAINT_GREEN, 0.2f);
        {
            int gi = beginGroup(MAT_PAINT_GREEN, "header");
            GeoBuilder g;
            g.mat = MAT_PAINT_GREEN;
            g.box(Box3(V3(-2.5f, y1 - 0.1f, 2.10f), V3(-0.9f, y1 + 0.1f, zh)), true, true);
            g.finish();
            groups[gi].mesh = std::move(g.mesh);
        }
        addDoor("exit_landing", V3(-2.4f, y1, 0.0f), V3(1, 0, 0), 1.4f, 2.08f, 1, V3(0, -1, 0), "The fire door is chained.");
        rooms.push_back({"lobby", "Stair lobby", Box3(V3(x0, y0, 0), V3(x1, y1, zh)),
                         V3(0, 9.2f, 1.9f), 3, true});

        // lobby details: notice board, exit sign, radiator, potted plant, floor numbers
        int gi = beginGroup(MAT_RULES, "lobby_details");
        GeoBuilder g;
        g.mat = MAT_RULES;
        props::sign(g, V3(1.9f, y1 - 0.14f, 1.55f), V3(0, -1, 0), 0.62f, 0.8f, MAT_RULES);
        g.mat = MAT_EXIT_SIGN;
        props::sign(g, V3(-1.7f, y1 - 0.14f, 2.35f), V3(0, -1, 0), 0.34f, 0.56f, MAT_EXIT_SIGN);
        g.mat = MAT_WALL_SIGN;
        props::sign(g, V3(x0 + 0.16f, 9.0f, 1.9f), V3(1, 0, 0), 0.5f, 0.7f, MAT_WALL_SIGN);
        g.mat = MAT_METAL_PAINTED;
        props::radiator(g, V3(-3.6f, y1 - 0.26f, 0.05f), 0, 1.4f);
        props::plantPot(g, V3(-4.1f, 10.4f, 0), 1.0f);
        props::fireExtinguisher(g, V3(3.9f, 10.6f, 0));
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(-3.75f, 10.6f, 0), V3(-4.45f, 10.0f, 0.6f)), gi);

        addLight(V3(0, 9.3f, zh - 0.12f), V3(0.95f, 0.86f, 0.68f), 1.9f, 8.0f, "lobby");
        addLight(V3(-3.2f, 10.4f, 2.3f), V3(0.9f, 0.8f, 0.62f), 0.8f, 5.0f, "lobby");
    }

    // ---- emergency exit landing (north of lobby) - ending route
    {
        float x0 = -2.9f, x1 = 0.8f, y0 = 11.0f, y1 = 13.4f;
        floorSlab(Box3(V3(x0, y0, z0), V3(x1, y1, 2.5f)), MAT_CONCRETE_FLOOR, MAT_CEILING);
        wallSegment(V3(x0, y0, 0), V3(x0, y1, 0), z0, 2.5f, MAT_PAINT_GREEN, 0.2f);
        wallSegment(V3(x1, y0, 0), V3(x1, y1, 0), z0, 2.5f, MAT_PAINT_GREEN, 0.2f);
        wallSegment(V3(x0, y1, 0), V3(-0.6f, y1, 0), z0, 2.5f, MAT_PAINT_GREEN, 0.2f);
        wallSegment(V3(0.2f, y1, 0), V3(x1, y1, 0), z0, 2.5f, MAT_PAINT_GREEN, 0.2f);
        {
            int gi = beginGroup(MAT_PAINT_GREEN, "header");
            GeoBuilder g;
            g.mat = MAT_PAINT_GREEN;
            g.box(Box3(V3(-0.7f, y1 - 0.1f, 2.12f), V3(0.3f, y1 + 0.1f, 2.5f)), true, true);
            g.finish();
            groups[gi].mesh = std::move(g.mesh);
        }
        // exit door (opens with the maintenance key)
        addDoor("fire_exit", V3(-0.6f, y1, 0.0f), V3(1, 0, 0), 0.85f, 2.10f, -1, V3(0, -1, 0),
                "The fire door is locked. A keyhole, recently oiled.");
        Door* d = doorById("fire_exit");
        if (d) { d->opensWith = "maintenance_key"; d->state = DoorState::Locked; }
        int gi = beginGroup(MAT_EXIT_SIGN, "exit_sign");
        GeoBuilder g;
        g.mat = MAT_EXIT_SIGN;
        props::sign(g, V3(-0.15f, y1 - 0.16f, 2.42f), V3(0, -1, 0), 0.4f, 0.62f, MAT_EXIT_SIGN);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        rooms.push_back({"exit_landing", "Fire stair landing", Box3(V3(x0, y0, 0), V3(x1, y1, 2.5f)),
                         V3(-1, 12.2f, 1.6f), 3, false});
        addLight(V3(-1.0f, 12.4f, 2.35f), V3(0.55f, 1.0f, 0.62f), 1.1f, 5.0f, "exit_landing", true);
        addInteract({"fire_exit_sign", "FIRE DOOR", V3(-0.2f, 12.6f, 1.6f), 2.4f, InteractType::Report, -1});
        interactables.back().id = "ending_exit";
        interactables.back().type = InteractType::Custom;
    }

    // ---- maintenance room (east of lobby)
    {
        float x0 = 4.6f, x1 = 8.4f, y0 = 9.9f, y1 = 13.2f;
        floorSlab(Box3(V3(x0, y0, z0), V3(x1, y1, 2.6f)), MAT_CONCRETE_FLOOR, MAT_CEILING_WATER);
        wallSegment(V3(x0, y1, 0), V3(x1, y1, 0), z0, 2.6f, MAT_CONCRETE_WALL, 0.2f);
        wallSegment(V3(x1, y0, 0), V3(x1, y1, 0), z0, 2.6f, MAT_CONCRETE_WALL, 0.2f);
        wallSegment(V3(x0, y0, 0), V3(x0, 10.4f, 0), z0, 2.6f, MAT_CONCRETE_WALL, 0.2f);
        wallSegment(V3(x0, 11.6f, 0), V3(x0, y1, 0), z0, 2.6f, MAT_CONCRETE_WALL, 0.2f);
        wallSegment(V3(x0, y0, 0), V3(x1, y0, 0), z0, 2.6f, MAT_CONCRETE_WALL, 0.2f);
        {
            int gi = beginGroup(MAT_CONCRETE_WALL, "header");
            GeoBuilder g;
            g.mat = MAT_CONCRETE_WALL;
            g.box(Box3(V3(x0 - 0.1f, 10.4f, 2.08f), V3(x0 + 0.1f, 11.6f, 2.6f)), true, true);
            g.finish();
            groups[gi].mesh = std::move(g.mesh);
        }
        addDoor("maintenance", V3(x0, 10.4f, 0.0f), V3(0, 1, 0), 1.2f, 2.06f, 1, V3(-1, 0, 0), "");
        rooms.push_back({"maintenance", "Maintenance room", Box3(V3(x0, y0, 0), V3(x1, y1, 2.6f)),
                         V3(6.4f, 11.4f, 1.7f), 3, true});
        int gi = beginGroup(MAT_METAL_RUST, "maintenance_props");
        GeoBuilder g;
        g.mat = MAT_METAL_RUST;
        props::pipeRun(g, V3(4.8f, 12.9f, 2.3f), V3(8.2f, 12.9f, 2.3f), 0.07f);
        props::pipeRun(g, V3(4.9f, 12.9f, 2.3f), V3(4.9f, 12.9f, 0.4f), 0.07f);
        props::pipeRun(g, V3(8.1f, 12.9f, 2.3f), V3(8.1f, 11.4f, 2.3f), 0.05f);
        g.mat = MAT_METAL_STEEL;
        props::boilerPanel(g, V3(8.15f, 11.4f, 1.15f), V3(-1, 0, 0));
        g.mat = MAT_METAL_PAINTED;
        props::lockers(g, V3(5.1f, 12.0f, 0), 3.14159f, 3);
        g.mat = MAT_METAL_RUST;
        props::mopBucket(g, V3(7.6f, 10.6f, 0));
        props::barrel(g, V3(7.4f, 12.5f, 0), 0, 0.3f, 0.9f);
        g.mat = MAT_CERAMIC;
        props::sink(g, V3(5.2f, 12.9f, 0.0f), 3.14159f);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(5.0f, 11.75f, 0), V3(6.4f, 12.25f, 1.85f)), gi);
        addCollider(Box3(V3(7.1f, 12.2f, 0), V3(7.7f, 12.8f, 0.9f)), gi);
        addCollider(Box3(V3(5.0f, 12.7f, 0), V3(5.4f, 13.1f, 0.9f)), gi);
        addLight(V3(6.5f, 11.5f, 2.45f), V3(0.85f, 0.9f, 1.0f), 1.4f, 5.5f, "maintenance");
        // document: the maintenance log
        addDocument("doc_log", "Maintenance log, page 31",
                    "The knocking in 305 started the week the Lisovs moved out.\n"
                    "I checked the walls twice. There is nothing behind them.\n"
                    "Meter readings for floor 3 do not match the building plans:\n"
                    "there is a load on a circuit that was never installed.\n"
                    "I am not going back into the basement alone.",
                    V3(5.2f, 12.85f, 0.95f), "maintenance");
        // the maintenance key pickup
        addInteract({"item_torch_battery", "SPARE BATTERIES", V3(7.6f, 10.5f, 0.9f), 1.6f, InteractType::Item, 100});
        // the maintenance key, on a nail by the door
        addInteract({"item_maintenance_key", "MAINTENANCE KEY", V3(4.5f, 13.0f, 1.45f), 1.7f, InteractType::Item, 102});
        // a hidden document inside the locker
        addDocument("doc_names", "List of residents",
                    "3rd floor register, last updated 1998.\n"
                    "301 - vacant since 1991\n302 - vacant\n303 - vacant\n"
                    "305 - LISOV, A. (2)\n308 - vacant\n47 - occupied\n\n"
                    "Someone has written under the last line, in pencil:\n"
                    "\"it counts the doors. do not let it count you.\"",
                    V3(5.7f, 11.8f, 1.3f), "maintenance");
    }

    // ---- south corridor details: buckets, a stalled pram, notice
    {
        int gi = beginGroup(MAT_METAL_RUST, "corridor_details");
        GeoBuilder g;
        g.mat = MAT_METAL_RUST;
        props::pipeRun(g, V3(-1.0f, -3.0f, 2.7f), V3(1.0f, -3.0f, 2.7f), 0.05f);
        props::mopBucket(g, V3(0.7f, -6.4f, 0));
        g.mat = MAT_METAL_PAINTED;
        props::radiator(g, V3(-1.02f, -2.0f, 0.1f), 1.5708f, 1.2f);
        g.mat = MAT_NOTE;
        props::sign(g, V3(-1.0f, -5.2f, 1.5f), V3(1, 0, 0), 0.35f, 0.45f, MAT_NOTE);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(0.5f, -6.6f, 0), V3(0.95f, -6.1f, 0.4f)), gi);
        addLight(V3(0, -2.6f, 2.7f), V3(0.9f, 0.82f, 0.66f), 1.0f, 6.0f, "corridor_south");
        addLight(V3(0, -6.0f, 2.7f), V3(0.9f, 0.82f, 0.66f), 0.0f, 6.0f, "corridor_south");  // dead bulb
    }
}

// ============================================================================
//  APARTMENTS
// ============================================================================
void World::buildApartment(const V3& origin, float yaw, int variant, const std::string& label,
                           bool playerApt) {
    // local plan (metres): shell 7.0 x 5.6, entrance on the y=0 wall at x 0.55..1.65
    //   hall    x 0.0-2.0 , y 0.0-5.6
    //   living  x 2.0-4.9 , y 0.0-3.2
    //   kitchen x 4.9-7.0 , y 0.0-3.2  (arch from the living room)
    //   bath    x 2.0-3.4 , y 3.2-5.6  (door from the hall)
    //   bedroom x 3.4-7.0 , y 3.2-5.6  (door from the living room)
    const float z0 = 0.0f, zh = 2.72f;
    const float D = 5.6f, W = 7.0f;
    const int WALLM = (variant % 3 == 0) ? MAT_WALLPAPER_STRIPE
                    : (variant % 3 == 1) ? MAT_WALLPAPER_DAMASK : MAT_WALLPAPER_FLORAL;
    const int FLOORM = (variant % 2 == 0) ? MAT_PARQUET : MAT_LINO_BEIGE;
    const int CEILM = (variant == 2) ? MAT_CEILING_WATER : MAT_CEILING;
    Frame F{origin, yaw};
    auto P = [&](float x, float y, float z) { return F.p(x, y, z); };
    auto B = [&](float x0, float y0, float x1, float y1, float zz0, float zz1) {
        return Box3(vmin(P(x0, y0, zz0), P(x1, y1, zz1)), vmax(P(x0, y0, zz0), P(x1, y1, zz1)));
    };
    auto wall = [&](float x0, float y0, float x1, float y1, int m, float t = 0.12f) {
        wallSegment(P(x0, y0, 0), P(x1, y1, 0), z0, zh, m, t);
    };
    auto header = [&](float x0, float y0, float x1, float y1, int m, float t = 0.12f, float hz = 2.04f) {
        int gi = beginGroup(m, "header");
        GeoBuilder g;
        g.mat = m;
        g.box(B(x0, y0, x1, y1, hz, zh), true, true);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        (void)t;
    };

    // ---- shell
    floorSlab(B(0, 0, W, D, 0, zh), FLOORM, CEILM);
    wall(0, 0, 0, D, WALLM, 0.2f);
    wall(W, 0, W, D, WALLM, 0.2f);
    wall(0, D, W, D, WALLM, 0.2f);
    wall(0, 0, 0.5f, 0, WALLM, 0.2f);
    wall(1.7f, 0, W, 0, WALLM, 0.2f);
    header(0.5f, -0.1f, 1.7f, 0.1f, WALLM, 0.2f, 2.06f);

    // ---- hall walls (x = 2.0) with living and bath doors
    wall(2.0f, 0.0f, 2.0f, 1.0f, WALLM);
    wall(2.0f, 2.0f, 2.0f, 3.9f, WALLM);
    wall(2.0f, 4.8f, 2.0f, D, WALLM);
    header(1.94f, 1.0f, 2.06f, 2.0f, WALLM);
    header(1.94f, 3.9f, 2.06f, 4.8f, WALLM);
    // living/kitchen divider with an arch
    wall(4.9f, 0.0f, 4.9f, 1.5f, WALLM);
    wall(4.9f, 2.7f, 4.9f, 3.2f, WALLM);
    header(4.84f, 1.5f, 4.96f, 2.7f, WALLM, 0.12f, 2.10f);
    // rear wall (y = 3.2) with the bedroom door
    wall(2.0f, 3.2f, 3.6f, 3.2f, WALLM);
    wall(4.6f, 3.2f, W, 3.2f, WALLM);
    header(3.6f, 3.14f, 4.6f, 3.26f, WALLM);
    // bath / bedroom divider
    wall(3.4f, 3.2f, 3.4f, D, MAT_TILE_BATH, 0.1f);

    // ---- interior doors
    auto addLocalDoor = [&](const std::string& id, float x, float y, float dirX, float dirY,
                            float w, int swing, float nx, float ny, const std::string& hint) {
        addDoor(id, P(x, y, 0), F.dir(dirX, dirY), w, 2.0f, swing, F.dir(nx, ny), hint);
    };
    addLocalDoor(label + "_living", 2.0f, 1.0f, 0, 1, 1.0f, 1, -1, 0, "");
    addLocalDoor(label + "_bath", 2.0f, 3.9f, 0, 1, 0.9f, 1, -1, 0, "");
    addLocalDoor(label + "_bed", 3.6f, 3.2f, 1, 0, 1.0f, 1, 0, -1, "");
    if (!playerApt && (variant % 2)) {
        if (Door* d = doorById(label + "_bed")) { d->openness = 0.62f; d->state = DoorState::Ajar; }
    }
    if (!playerApt && (variant == 3)) {
        if (Door* d = doorById(label + "_bath")) { d->openness = 0.35f; d->state = DoorState::Ajar; }
    }

    // ---- furniture
    // Every material gets its own geometry group so props are textured correctly and the
    // anomaly system can target individual pieces (painting, sofa, clock, ...).
    std::map<int, GeoBuilder> batches;
    auto G = [&](int m) -> GeoBuilder& {
        auto it = batches.find(m);
        if (it == batches.end()) {
            GeoBuilder b;
            b.mat = m;
            b.uvScale = 1.0f;
            it = batches.emplace(m, std::move(b)).first;
        }
        return it->second;
    };
    int gi = -1;
    float Y = yaw;
    // hall
    props::shoeRack(G(MAT_WOOD_DARK), P(0.34f, 0.65f, 0), Y + 3.14159f);
    props::coatHooks(G(MAT_METAL_STEEL), P(0.14f, 1.35f, 1.72f), F.dir(1, 0));
    props::mirror(G(MAT_GLASS), P(0.14f, 2.75f, 1.55f), yaw, 0.45f, 0.7f);
    // living
    props::sofa(G(MAT_FABRIC_SOFA), P(3.45f, 2.78f, 0), Y + 3.14159f, MAT_FABRIC_SOFA);
    props::rug(G(MAT_CARPET_RED), P(3.45f, 1.55f, 0), 2.3f, 1.7f, MAT_CARPET_RED, Y);
    props::table(G(MAT_WOOD_LIGHT), P(3.45f, 1.55f, 0), 1.15f, 0.72f, 0.74f, MAT_WOOD_LIGHT);
    props::chair(G(MAT_WOOD_LIGHT), P(3.05f, 1.15f, 0), Y + 0.6f, MAT_WOOD_LIGHT);
    props::chair(G(MAT_WOOD_LIGHT), P(3.85f, 1.95f, 0), Y - 2.5f, MAT_WOOD_LIGHT);
    props::tv(G(MAT_UNLIT_BLACK), P(3.45f, 0.36f, 0), Y);
    props::bookshelf(G(MAT_WOOD_DARK), P(2.16f, 2.62f, 0), Y - 1.5708f, 4, 1.1f);
    props::clock(G(MAT_CLOCK), P(2.08f, 2.4f, 1.72f), F.dir(1, 0));
    props::painting(G(variant == 3 ? MAT_PAINTING_ABSTRACT : MAT_PAINTING_LANDSCAPE),
                    P(4.2f, 3.14f, 1.62f), F.dir(0, -1), 0.95f, 0.68f,
                    variant == 3 ? MAT_PAINTING_ABSTRACT : MAT_PAINTING_LANDSCAPE);
    // kitchen
    props::kitchenRun(G(MAT_TILE_KITCHEN), P(5.0f, 2.4f, 0), 0.9f, 0);
    props::stove(G(MAT_METAL_STEEL), P(6.15f, 2.62f, 0), Y + 3.14159f);
    props::fridge(G(MAT_UNLIT_WHITE), P(6.7f, 2.58f, 0), Y + 3.14159f);
    props::sink(G(MAT_CERAMIC), P(5.42f, 2.62f, 0.74f), Y + 3.14159f);
    // bath
    props::bathtub(G(MAT_CERAMIC), P(2.44f, 3.35f, 0), Y);
    props::toilet(G(MAT_CERAMIC), P(3.14f, 4.72f, 0), Y + 3.14159f);
    props::washbasin(G(MAT_CERAMIC), P(3.27f, 3.65f, 0), Y - 1.5708f);
    props::mirror(G(MAT_GLASS), P(3.34f, 3.65f, 1.38f), yaw + 3.14159f, 0.5f, 0.55f);
    // bedroom
    props::bed(G(MAT_MATTRESS), P(5.75f, 4.35f, 0), Y + 3.14159f, (variant % 2) == 1);
    props::desk(G(MAT_WOOD_LIGHT), P(4.62f, 5.14f, 0), Y + 3.14159f);
    props::bookshelf(G(MAT_WOOD_DARK), P(6.86f, 4.3f, 0), Y - 1.5708f, 3, 1.0f);
    props::wardrobe(G(MAT_WOOD_DARK), P(3.62f, 4.18f, 0), Y - 1.5708f, MAT_WOOD_DARK);
    props::lamp(G(MAT_LAMPSHADE), P(4.2f, 3.95f, 0.77f), false, true);
    props::painting(G(MAT_PHOTO), P(5.7f, 5.44f, 1.75f), F.dir(0, -1), 0.55f, 0.42f, MAT_PHOTO);
    if (variant == 1 || variant == 4) props::toyBlocks(G(MAT_WOOD_LIGHT), P(4.3f, 4.9f, 0));
    props::suitcase(G(MAT_WOOD_DARK), P(6.35f, 3.55f, 0), Y + 0.4f);
    // flush the material batches into groups; the deepest (wood) group owns the colliders
    for (auto& kv : batches) {
        if (kv.second.mesh.idx.empty()) continue;
        int gid = beginGroup(kv.first, label + "_prop");
        kv.second.finish();
        groups[gid].mesh = std::move(kv.second.mesh);
        if (gi < 0) gi = gid;
        if (kv.first == MAT_WOOD_DARK) gi = gid;
    }

    // ---- colliders
    addCollider(B(2.85f, 1.15f, 4.05f, 1.95f, 0, 0.78f), gi);       // table
    addCollider(B(2.45f, 2.36f, 4.45f, 3.14f, 0, 0.95f), gi);       // sofa
    addCollider(B(3.05f, 0.06f, 3.85f, 0.66f, 0, 0.58f), gi);       // tv
    addCollider(B(2.02f, 2.14f, 2.32f, 3.1f, 0, 1.55f), gi);         // bookshelf (clear of the door)
    addCollider(B(5.0f, 2.3f, 7.0f, 3.2f, 0, 2.1f), gi);            // kitchen run
    addCollider(B(4.7f, 3.25f, 6.8f, 5.45f, 0, 0.62f), gi);         // bed
    addCollider(B(4.06f, 4.8f, 5.18f, 5.5f, 0, 0.78f), gi);          // desk
    addCollider(B(3.48f, 3.6f, 3.86f, 4.76f, 0, 2.1f), gi);          // wardrobe
    addCollider(B(2.05f, 3.3f, 2.85f, 5.0f, 0, 0.58f), gi);         // tub
    addCollider(B(2.9f, 4.5f, 3.38f, 4.95f, 0, 0.85f), gi);         // toilet
    // hall furniture is shallow enough to walk past

    // ---- lighting
    addLight(P(3.45f, 1.6f, zh - 0.12f), V3(1.0f, 0.86f, 0.62f), playerApt ? 1.35f : 0.0f, 7.0f, label);
    addLight(P(6.1f, 1.6f, zh - 0.12f), V3(1.0f, 0.92f, 0.74f), 0.0f, 5.2f, label + "_kitchen");
    addLight(P(5.3f, 4.4f, 1.78f), V3(1.0f, 0.76f, 0.48f), playerApt ? 0.6f : 0.0f, 4.2f, label + "_bed");
    addLight(P(1.0f, 1.6f, zh - 0.12f), V3(1.0f, 0.86f, 0.62f), 0.0f, 4.0f, label + "_hall");
    if (variant == 0 && playerApt) addLight(P(1.05f, 3.4f, 2.1f), V3(0.9f, 0.95f, 1.0f), 0.0f, 3.0f, label);
    {
        int lgi = beginGroup(MAT_LIGHTBASE, label + "_lights");
        GeoBuilder lg;
        lg.mat = MAT_LIGHTBASE;
        props::ceilingLight(lg, P(3.45f, 1.6f, zh), false);
        props::ceilingLight(lg, P(6.1f, 1.6f, zh), true);
        props::ceilingLight(lg, P(1.0f, 1.4f, zh), false);
        props::wallLamp(lg, P(2.08f, 2.5f, 1.78f), F.dir(1, 0));
        lg.finish();
        groups[lgi].mesh = std::move(lg.mesh);
    }

    // ---- rooms
    auto addRoom = [&](const std::string& id, const char* name, float x0, float y0, float x1, float y1,
                       float px, float py) {
        rooms.push_back({id, name, B(x0, y0, x1, y1, 0, zh), P(px, py, 1.6f), 3, true});
    };
    addRoom(label + "_hall", "Entry hall", 0, 0, 2.0f, D, 1.0f, 1.6f);
    addRoom(label + "_living", "Living room", 2.0f, 0, 4.9f, 3.2f, 3.45f, 1.6f);
    addRoom(label + "_kitchen", "Kitchen", 4.9f, 0, W, 3.2f, 6.0f, 1.6f);
    addRoom(label + "_bath", "Bathroom", 2.0f, 3.2f, 3.4f, D, 2.7f, 4.4f);
    addRoom(label + "_bed", "Bedroom", 3.4f, 3.2f, W, D, 5.3f, 4.4f);

    // ---- papers
    if (playerApt) {
        addDocument("doc_letter", "A letter, folded twice",
                    "If you are reading this you have already woken up here.\n\n"
                    "The building does not want you dead. It wants you to stop noticing.\n"
                    "Every time you walk the corridor and tell yourself that everything is\n"
                    "as it should be, it takes something away from you.\n\n"
                    "Report what is wrong. Count the doors.\n"
                    "And whatever is standing at the end of the corridor, do not look back\n"
                    "at it. Look at the floor. Count.\n\n- the one who came before you",
                    P(3.45f, 1.55f, 0.76f), "apt47_living");
        addDocument("doc_drawing", "A child's drawing",
                    "Crayon on wallpaper: a block of flats with too many windows.\n"
                    "Six small figures stand in front of it. A seventh, taller figure stands\n"
                    "behind them, drawn in black, filling the whole page.\n"
                    "Underneath, in uncertain letters: SHE LIVES IN THE WALLS",
                    P(4.2f, 3.98f, 0.79f), "apt47_bed");
        if (Interactable* it = nullptr) { (void)it; }
        addInteract({"player_start_note", "SOMETHING WRITTEN ON THE WALL", P(3.45f, 1.55f, 0.8f), 1.8f, InteractType::Custom, -1});
        interactables.back().id = "player_start_note";
    }
}

// ============================================================================
//  STAIRWELL
// ============================================================================
void World::buildStairwell() {
    const float z0 = 0.0f, zh = 4.4f;   // full height for the flight to floor 4
    // shaft x -8.6..-4.6, y 7.6..11.0
    float x0 = -8.6f, x1 = -4.6f, y0 = 7.6f, y1 = 11.0f;
    floorSlab(Box3(V3(x0, y0, z0), V3(x1, y1, zh)), MAT_CONCRETE_FLOOR, MAT_CONCRETE_WALL);
    wallSegment(V3(x0, y0, 0), V3(x1, y0, 0), z0, zh, MAT_PAINT_GREEN, 0.2f);
    wallSegment(V3(x0, y0, 0), V3(x0, y1, 0), z0, zh, MAT_PAINT_GREEN, 0.2f);
    wallSegment(V3(x0, y1, 0), V3(x1, y1, 0), z0, zh, MAT_PAINT_GREEN, 0.2f);
    wallSegment(V3(x1, y0, 0), V3(x1, 7.6f + 0.0f, 0), z0, zh, MAT_PAINT_GREEN, 0.2f);
    // (east side is the lobby opening at y 8.6..10.4, built in the lobby step)

    // ---- down flight: from lobby level (x -5.9..-7.3 lane) descending toward y=10.9
    int gi = beginGroup(MAT_CONCRETE_FLOOR, "stairs");
    GeoBuilder g;
    g.mat = MAT_CONCRETE_FLOOR;
    g.uvScale = 1.2f;
    float rise = 3.4f / 13.0f, run = 0.30f;
    // flight A: descends northward in lane x -6.1..-7.5
    for (int i = 0; i < 13; ++i) {
        float zTop = -rise * (i + 1);
        float yA = y0 + 0.55f + i * run;
        g.box(Box3(V3(-7.45f, yA, zTop - 0.12f), V3(-6.15f, yA + run, zTop)), true, true);
    }
    // lower landing at z = -3.4, y 10.5..11.0
    g.box(Box3(V3(-7.45f, y0 + 0.55f + 13 * run, -3.52f), V3(-5.4f, y1, -3.40f)), true, true);
    // up flight: from lobby level rising in lane x -5.9..-4.75 toward y 10.9
    for (int i = 0; i < 13; ++i) {
        float zTop = rise * (i + 1);
        float yA = y0 + 0.55f + i * run;
        g.box(Box3(V3(-4.95f, yA, zTop - 0.12f), V3(-3.9f, yA + run, zTop)), true, true);
    }
    g.finish();
    groups[gi].mesh = std::move(g.mesh);
    // stair colliders approximated as blocks (walkable handled by a ramp collider exception)
    for (int i = 0; i < 13; ++i) {
        float zTop = -rise * (i + 1);
        float yA = y0 + 0.55f + i * run;
        addCollider(Box3(V3(-7.45f, yA, zTop - 0.30f), V3(-6.15f, yA + run, zTop - 0.02f)), gi);
        float zTop2 = rise * (i + 1);
        addCollider(Box3(V3(-4.95f, yA, zTop2 - 0.30f), V3(-3.9f, yA + run, zTop2 - 0.02f)), gi);
    }

    // railings
    gi = beginGroup(MAT_METAL_STEEL, "railings");
    GeoBuilder rg;
    rg.mat = MAT_METAL_STEEL;
    props::railing(rg, V3(-6.15f, y0 + 0.55f, -0.05f), V3(-6.15f, y0 + 0.55f + 13 * run, -3.45f), 0.95f);
    props::railing(rg, V3(-4.95f, y0 + 0.55f, 0.05f), V3(-4.95f, y0 + 0.55f + 13 * run, 3.45f), 0.95f);
    rg.finish();
    groups[gi].mesh = std::move(rg.mesh);

    // window on the west wall landing (impossible view anomaly target)
    addInteract({"stair_window", "LOOK OUT OF THE WINDOW", V3(-8.4f, 9.2f, 2.0f), 2.0f, InteractType::Custom, -1});

    rooms.push_back({"stairwell", "Stairwell", Box3(V3(x0, y0, -3.5f), V3(x1, y1, zh)),
                     V3(-6.0f, 9.2f, 1.6f), 3, true});
    addLight(V3(-6.0f, 9.4f, zh - 0.2f), V3(0.92f, 0.9f, 0.8f), 1.5f, 7.0f, "stairwell");
    addLight(V3(-6.6f, 10.6f, -1.6f), V3(0.9f, 0.86f, 0.7f), 0.9f, 4.5f, "stairwell");

    // utility door at the bottom of the stairs (leads to the basement)
    addDoor("basement_door", V3(-5.4f, 10.9f, -3.4f), V3(1, 0, 0), 0.95f, 2.0f, 1, V3(0, 1, 0), "");
    // window frame on the landing wall
    int wgi = beginGroup(MAT_GLASS, "stair_window");
    GeoBuilder wg;
    wg.mat = MAT_GLASS;
    props::windowOpening(wg, V3(-8.55f, 9.2f, 1.9f), V3(1, 0, 0), 1.1f, 1.3f);
    wg.finish();
    groups[wgi].mesh = std::move(wg.mesh);

    // the 4th floor landing blocked with debris (anomaly target: it can open)
    int dgi = beginGroup(MAT_WOOD_PLANKS, "debris");
    GeoBuilder dg;
    dg.mat = MAT_WOOD_PLANKS;
    dg.box(Box3(V3(-4.95f, 10.4f, 3.3f), V3(-3.9f, 11.0f, 4.3f)), true, true);
    dg.finish();
    groups[dgi].mesh = std::move(dg.mesh);
    addCollider(Box3(V3(-4.95f, 10.4f, 3.3f), V3(-3.9f, 11.0f, 4.3f)), dgi);
    rooms.push_back({"stair_landing4", "Fourth floor landing",
                     Box3(V3(-4.95f, 10.2f, 3.4f), V3(-3.9f, 11.0f, 5.6f)), V3(-4.4f, 10.6f, 4.2f), 4, false});
}

// ============================================================================
//  BASEMENT
// ============================================================================
void World::buildBasement() {
    const float zf = -3.4f, zh = -0.8f;
    const int WALL = MAT_CONCRETE_WALL, FLOOR = MAT_CONCRETE_FLOOR, CEIL = MAT_CEILING_WATER;

    // landing below the stairs
    floorSlab(Box3(V3(-6.0f, 10.4f, zf), V3(-4.2f, 11.2f, zh)), FLOOR, CEIL);
    // east-west spine corridor
    floorSlab(Box3(V3(-4.2f, 9.6f, zf), V3(2.8f, 11.4f, zh)), FLOOR, CEIL);
    // north-south spine
    floorSlab(Box3(V3(0.6f, -1.4f, zf), V3(2.8f, 9.6f, zh)), FLOOR, CEIL);

    auto wall = [&](float x0, float y0, float x1, float y1, int m, float zTop) {
        wallSegment(V3(x0, y0, zf), V3(x1, y1, zf), zf, zTop, m, 0.2f);
    };
    // corridor walls (with door gaps)
    wall(-4.2f, 9.6f, 2.8f, 9.6f, WALL, zh);           // south wall of EW spine
    wall(-4.2f, 9.6f, -1.6f, 9.6f, WALL, zh);
    wall(0.0f, 9.6f, 2.8f, 9.6f, WALL, zh);
    wall(-4.2f, 9.6f, -4.2f, 11.2f, WALL, zh);         // west end
    wall(2.8f, 9.6f, 2.8f, 11.4f, WALL, zh);
    wall(-4.2f, 11.4f, 0.6f, 11.4f, WALL, zh);         // north wall
    wall(2.8f, 11.4f, 2.8f, 9.6f, WALL, zh);

    // doorways in the south wall of the EW corridor: laundry (x -3.2..-2.2), storage A (x 1.2..2.2)
    // headers
    {
        int gi = beginGroup(WALL, "headers");
        GeoBuilder g;
        g.mat = WALL;
        g.box(Box3(V3(-3.2f, 9.5f, zf + 2.0f), V3(-2.2f, 9.7f, zh)), true, true);
        g.box(Box3(V3(1.2f, 9.5f, zf + 2.0f), V3(2.2f, 9.7f, zh)), true, true);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
    }
    addDoor("laundry", V3(-3.2f, 9.6f, zf), V3(1, 0, 0), 1.0f, 2.0f, -1, V3(0, 1, 0), "");
    addDoor("storageA", V3(1.2f, 9.6f, zf), V3(1, 0, 0), 1.0f, 2.0f, -1, V3(0, 1, 0), "");

    // ---- laundry room
    {
        float x0 = -8.6f, x1 = -3.4f, y0 = 4.6f, y1 = 9.4f;
        floorSlab(Box3(V3(x0, y0, zf), V3(x1, y1, zh)), MAT_TILE_KITCHEN, CEIL);
        wall(x0, y0, x1, y0, WALL, zh);
        wall(x0, y0, x0, y1, WALL, zh);
        wall(x0, y1, -3.4f, y1, WALL, zh);
        wall(-2.2f, y1, x1, y1, WALL, zh);
        wall(x1, y1, x1, 9.6f, WALL, zh);
        // (the door is in the corridor wall; add a small return wall)
        wall(-2.2f, 9.6f, -2.2f, y1, WALL, zh);
        wall(-3.4f, 9.6f, -3.4f, y1, WALL, zh);
        rooms.push_back({"laundry", "Laundry room", Box3(V3(x0, y0, zf), V3(x1, y1, zh)),
                         V3(-6.0f, 7.0f, zf + 1.6f), 1, true});
        int gi = beginGroup(MAT_CERAMIC, "laundry_props");
        GeoBuilder g;
        g.mat = MAT_CERAMIC;
        for (int i = 0; i < 3; ++i)
            props::washingMachine(g, V3(-7.9f + i * 0.75f, 8.9f, zf), 3.14159f);
        props::sink(g, V3(-7.9f, 5.2f, zf + 0.0f), 0.0f);
        g.mat = MAT_METAL_STEEL;
        props::pipeRun(g, V3(-8.4f, 9.2f, zh - 0.25f), V3(-3.5f, 9.2f, zh - 0.25f), 0.06f);
        g.mat = MAT_WOOD_PLANKS;
        props::table(g, V3(-5.4f, 6.2f, zf), 1.6f, 0.8f, 0.8f, MAT_WOOD_PLANKS);
        props::chair(g, V3(-5.9f, 6.2f, zf), 1.5708f, MAT_WOOD_PLANKS);
        g.mat = MAT_METAL_RUST;
        props::mopBucket(g, V3(-4.0f, 5.0f, zf));
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(-8.1f, 8.55f, zf), V3(-5.6f, 9.25f, zf + 0.9f)), gi);
        addCollider(Box3(V3(-6.2f, 5.8f, zf), V3(-4.6f, 6.6f, zf + 0.8f)), gi);
        addLight(V3(-6.0f, 7.0f, zh - 0.1f), V3(0.85f, 0.92f, 1.0f), 1.3f, 6.0f, "laundry");
        addDocument("doc_laundry", "Laundry room notice",
                    "DO NOT USE MACHINE 3.\n\n"
                    "The engineer says the drum turns by itself.\n"
                    "He has stopped coming down. Nobody knows where he is.\n"
                    "Somebody keeps folding the same towel.",
                    V3(-5.4f, 6.25f, zf + 0.82f), "laundry");
    }

    // ---- storage A (east of the N-S spine)
    {
        float x0 = 3.0f, x1 = 6.6f, y0 = 5.0f, y1 = 9.4f;
        floorSlab(Box3(V3(x0, y0, zf), V3(x1, y1, zh)), FLOOR, CEIL);
        wall(x1, y0, x1, y1, WALL, zh);
        wall(x0, y0, x1, y0, WALL, zh);
        wall(x0, y1, x1, y1, WALL, zh);
        wall(x0, y0, x0, y1, WALL, zh);       // west wall of room (door is on corridor)
        rooms.push_back({"storageA", "Store room A", Box3(V3(x0, y0, zf), V3(x1, y1, zh)),
                         V3(4.8f, 7.0f, zf + 1.6f), 1, true});
        int gi = beginGroup(MAT_WOOD_PLANKS, "storageA_props");
        GeoBuilder g;
        g.mat = MAT_WOOD_PLANKS;
        for (int i = 0; i < 4; ++i) props::crate(g, V3(3.5f + (i % 2) * 0.9f, 5.6f + (i / 2) * 1.1f, zf), i * 0.4f, 0.75f);
        for (int i = 0; i < 3; ++i) props::crate(g, V3(5.6f, 6.0f + i * 1.0f, zf), 0.2f * i, 0.65f);
        g.mat = MAT_METAL_RUST;
        props::barrel(g, V3(6.1f, 8.9f, zf), 0, 0.3f, 0.9f);
        g.mat = MAT_METAL_PAINTED;
        props::lockers(g, V3(3.4f, 9.0f, zf), 3.14159f, 3);
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(3.1f, 5.2f, zf), V3(4.6f, 7.0f, zf + 0.8f)), gi);
        addCollider(Box3(V3(5.3f, 5.7f, zf), V3(6.0f, 8.4f, zf + 0.7f)), gi);
        addCollider(Box3(V3(3.3f, 8.6f, zf), V3(4.7f, 9.2f, zh)), gi);
        addLight(V3(4.8f, 7.0f, zh - 0.1f), V3(0.9f, 0.86f, 0.72f), 1.0f, 5.5f, "storageA");
        addDocument("doc_plans", "Basement floor plans",
                    "Original plans, block 4, dated 1961.\n"
                    "The laundry and the boiler room are marked.\n"
                    "There is a further room drawn beneath the boiler room,\n"
                    "crossed out in red pencil and re-drawn as a solid wall.\n"
                    "An arrow points to it. Handwritten: \"she was kept here\".",
                    V3(5.6f, 6.9f, zf + 0.72f), "storageA");
        // the second pickup: a fuse that restores the basement lights
        addInteract({"item_fuse", "CERAMIC FUSE", V3(4.2f, 5.5f, zf + 0.7f), 1.6f, InteractType::Item, 101});
        interactables.back().id = "item_fuse";
    }

    // ---- boiler room (west of N-S spine)
    {
        float x0 = -4.4f, x1 = 0.6f, y0 = 0.4f, y1 = 8.4f;
        floorSlab(Box3(V3(x0, y0, zf), V3(x1, y1, zh)), MAT_STONE_FLOOR, CEIL);
        wall(x0, y0, x1, y0, WALL, zh);
        wall(x0, y0, x0, y1, WALL, zh);
        wall(x0, y1, x1, y1, WALL, zh);
        wall(x1, y0, x1, 1.6f, WALL, zh);      // doorway into the N-S spine (y 1.6..2.6)
        wall(x1, 2.6f, x1, y1, WALL, zh);
        {
            int gi = beginGroup(WALL, "header");
            GeoBuilder g;
            g.mat = WALL;
            g.box(Box3(V3(0.5f, 1.6f, zf + 2.0f), V3(0.7f, 2.6f, zh)), true, true);
            g.finish();
            groups[gi].mesh = std::move(g.mesh);
        }
        addDoor("boiler", V3(0.6f, 1.6f, zf), V3(0, 1, 0), 1.0f, 2.0f, 1, V3(-1, 0, 0), "");
        rooms.push_back({"boiler", "Boiler room", Box3(V3(x0, y0, zf), V3(x1, y1, zh)),
                         V3(-2.0f, 4.0f, zf + 1.6f), 1, true});
        int gi = beginGroup(MAT_METAL_PAINTED, "boiler_props");
        GeoBuilder g;
        g.mat = MAT_METAL_PAINTED;
        props::boilerTank(g, V3(-3.2f, 4.0f, zf), 0.85f, 2.2f);
        props::boilerTank(g, V3(-3.2f, 6.4f, zf), 0.6f, 1.7f);
        g.mat = MAT_METAL_RUST;
        for (int i = 0; i < 4; ++i)
            props::pipeRun(g, V3(-4.2f, 1.0f + i * 1.8f, zh - 0.2f), V3(0.4f, 1.0f + i * 1.8f, zh - 0.2f), 0.075f);
        props::pipeRun(g, V3(-4.2f, 0.6f, zh - 0.2f), V3(-4.2f, 8.2f, zh - 0.2f), 0.1f);
        props::pipeRun(g, V3(-3.2f, 4.0f, zf + 2.2f), V3(-3.2f, 4.0f, zh - 0.2f), 0.12f);
        g.mat = MAT_METAL_STEEL;
        props::boilerPanel(g, V3(0.45f, 5.2f, zf + 1.2f), V3(-1, 0, 0));
        props::boilerPanel(g, V3(-4.25f, 3.0f, zf + 1.2f), V3(1, 0, 0));
        g.mat = MAT_DIRT_FLOOR;
        props::barrel(g, V3(-1.0f, 7.6f, zf), 0, 0.3f, 0.85f);
        g.mat = MAT_METAL_RUST;
        props::drainPipe(g, V3(-1.2f, 1.0f, zf), 2.4f);
        // valve wheel: the lever that opens the hidden passage
        g.mat = MAT_METAL_STEEL;
        g.cylinder(V3(-3.2f, 2.0f, zf + 0.9f), 0.03f, 1.0f, 8);
        g.cylinder(V3(-3.2f, 2.0f, zf + 1.9f), 0.22f, 0.03f, 16);
        for (int i = 0; i < 4; ++i) {
            float a = i * nf::HALF_PI;
            g.cylinderAxis(V3(-3.2f + std::cos(a) * 0.03f, 2.0f + std::sin(a) * 0.03f, zf + 1.9f),
                           V3(-3.2f + std::cos(a) * 0.22f, 2.0f + std::sin(a) * 0.22f, zf + 1.9f), 0.015f, 6);
        }
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(-4.1f, 3.1f, zf), V3(-2.3f, 4.9f, zf + 2.2f)), gi);
        addCollider(Box3(V3(-3.9f, 5.8f, zf), V3(-2.5f, 7.0f, zf + 1.7f)), gi);
        addLight(V3(-1.6f, 3.0f, zh - 0.12f), V3(0.75f, 0.88f, 1.0f), 0.0f, 6.0f, "boiler");   // dead until fuse
        addLight(V3(-1.6f, 7.0f, zh - 0.12f), V3(1.0f, 0.7f, 0.35f), 0.9f, 5.0f, "boiler", true);
        addInteract({"lever_hidden", "BRASS VALVE", V3(-3.2f, 2.0f, zf + 1.9f), 2.2f, InteractType::Lever, 0});
        interactables.back().id = "lever_hidden";
        addDocument("doc_boiler", "Boiler room log",
                    "02:14 - pressure drop, west riser.\n"
                    "02:14 - the knocking begins. It is not the pipes. It is the same\n"
                    "number of knocks as the number of doors on the third floor.\n"
                    "02:41 - the coal store wall is warm to the touch.\n"
                    "03:00 - it stops. Every night, it stops at three.\n"
                    "03:00 is when she is allowed to move.",
                    V3(0.3f, 5.3f, zf + 1.3f), "boiler");
    }

    // ---- hidden room (south of the boiler, behind a false wall)
    {
        float x0 = -4.4f, x1 = 0.6f, y0 = -3.2f, y1 = 0.2f;
        floorSlab(Box3(V3(x0, y0, zf), V3(x1, y1, zh)), MAT_DIRT_FLOOR, CEIL);
        wall(x0, y0, x1, y0, MAT_BRICK, zh);
        wall(x0, y0, x0, y1, MAT_BRICK, zh);
        wall(x1, y0, x1, y1, MAT_BRICK, zh);
        wall(x0, y1, -1.4f, y1, MAT_BRICK, zh);        // false wall opening x -1.4..-0.2
        wall(-0.2f, y1, x1, y1, MAT_BRICK, zh);
        rooms.push_back({"hidden", "Coal store", Box3(V3(x0, y0, zf), V3(x1, y1, zh)),
                         V3(-2.0f, -1.5f, zf + 1.6f), 1, false});
        int gi = beginGroup(MAT_BRICK, "hidden_props");
        GeoBuilder g;
        g.mat = MAT_BRICK;
        g.box(Box3(V3(-1.4f, -0.02f, zf), V3(-0.2f, 0.22f, zf + 2.0f)), true, true);   // false wall panel
        g.finish();
        groups[gi].mesh = std::move(g.mesh);
        addCollider(Box3(V3(-1.4f, -0.02f, zf), V3(-0.2f, 0.22f, zf + 2.0f)), gi);
        // sacral markings: chalk circle, children's drawings, candles
        int mgi = beginGroup(MAT_SHADOW, "hidden_marks");
        GeoBuilder mg;
        mg.mat = MAT_SHADOW;
        mg.uvScale = 1.0f;
        mg.plane(V3(-2.0f, -1.4f, zf + 0.01f), V3(1, 0, 0), V3(0, 1, 0), 2.6f, 2.6f, 1.0f);
        mg.finish();
        groups[mgi].mesh = std::move(mg.mesh);
        addLight(V3(-2.0f, -1.2f, zf + 1.9f), V3(1.0f, 0.62f, 0.32f), 1.2f, 4.5f, "hidden", true);
        addDocument("doc_truth", "The last entry",
                    "We called her the tenet. The thing that lives in the space\n"
                    "between the walls, in the millimetres the architects did not use.\n"
                    "She cannot enter a room she has been seen in. So she moves\n"
                    "when nobody looks, and she follows the sound of breathing.\n\n"
                    "She learned our voices. She learned the number of our doors.\n"
                    "When the count is wrong, she is inside.\n\n"
                    "We sealed the store at three in the morning.\n"
                    "I am writing this on the other side of the brick.\n"
                    "If you can read it, she has already counted you.\n\n"
                    "Do not look back. Look at the doors. Count them. Leave.",
                    V3(-2.0f, -1.6f, zf + 1.1f), "hidden");
        // ending B trigger
        addInteract({"ending_descent", "THE WARM BRICK", V3(-2.0f, -2.2f, zf + 1.0f), 2.0f, InteractType::Custom, -1});
        interactables.back().id = "ending_descent";
    }
    addLight(V3(1.7f, 6.0f, zh - 0.12f), V3(0.86f, 0.9f, 1.0f), 1.0f, 6.0f, "basement_corridor");
    addLight(V3(1.7f, 0.5f, zh - 0.12f), V3(0.86f, 0.9f, 1.0f), 0.7f, 6.0f, "basement_corridor");
    addLight(V3(-1.0f, 10.5f, zh - 0.12f), V3(0.86f, 0.9f, 1.0f), 0.9f, 5.0f, "basement_corridor");
    rooms.push_back({"basement_corridor", "Basement corridor",
                     Box3(V3(-4.2f, -1.4f, zf), V3(2.8f, 11.4f, zh)), V3(1.7f, 4.0f, zf + 1.6f), 1, true});
}

// ============================================================================
//  ELEVATOR
// ============================================================================
static void buildElevator(dl::World& w) {
    // shaft east of the lobby, x 4.6..7.0, y 8.5..10.2
    float x0 = 4.6f, x1 = 7.0f, y0 = 8.5f, y1 = 10.2f, zh = 2.7f;
    int gi = w.beginGroup(MAT_METAL_STEEL, "elevator");
    GeoBuilder g;
    g.mat = MAT_METAL_STEEL;
    g.uvScale = 1.2f;
    g.box(nf::Box3(nf::V3(x0, y0, 0), nf::V3(x1, y0 + 0.12f, zh)), true, true);          // back
    g.box(nf::Box3(nf::V3(x0, y1 - 0.12f, 0), nf::V3(x1, y1, zh)), true, true);          // front wall
    g.box(nf::Box3(nf::V3(x1 - 0.12f, y0, 0), nf::V3(x1, y1, zh)), true, true);          // side
    // floor & ceiling of the car
    g.mat = MAT_TILE_LOBBY;
    g.box(nf::Box3(nf::V3(x0, y0, 0), nf::V3(x1 - 0.12f, y1 - 0.12f, 0.04f)), true, false);
    g.mat = MAT_METAL_PAINTED;
    g.box(nf::Box3(nf::V3(x0, y0, zh - 0.12f), nf::V3(x1 - 0.12f, y1 - 0.12f, zh)), false, true);
    // handrail
    g.mat = MAT_METAL_STEEL;
    g.cylinderAxis(nf::V3(x0 + 0.06f, y0 + 0.3f, 1.0f), nf::V3(x0 + 0.06f, y1 - 0.3f, 1.0f), 0.02f, 6);
    g.finish();
    w.groups[gi].mesh = std::move(g.mesh);
    w.addCollider(nf::Box3(nf::V3(x0, y0, 0), nf::V3(x1, y0 + 0.12f, zh)), gi);
    w.addCollider(nf::Box3(nf::V3(x0, y1 - 0.12f, 0), nf::V3(x1, y1, zh)), gi);
    w.addCollider(nf::Box3(nf::V3(x1 - 0.12f, y0, 0), nf::V3(x1, y1, zh)), gi);
    // elevator entrance frame + doors on the west face
    int fgi = w.beginGroup(MAT_METAL_PAINTED, "elevator_frame");
    GeoBuilder fg;
    fg.mat = MAT_METAL_PAINTED;
    props::elevatorFrame(fg, nf::V3(x0, 9.35f, 0.0f), nf::V3(1, 0, 0), 1.1f, 2.1f);
    props::elevatorDoors(fg, nf::V3(x0 - 0.02f, 9.35f, 0.0f), nf::V3(1, 0, 0), 1.1f, 2.1f);
    fg.mat = MAT_EXIT_SIGN;
    fg.finish();
    w.groups[fgi].mesh = std::move(fg.mesh);
    // the car's sliding doors are driven by the elevator system (see gameplay)
    w.addDoor("elevator_car", nf::V3(x0 - 0.02f, 8.8f, 0.0f), nf::V3(0, 1, 0), 1.1f, 2.1f, 1,
              nf::V3(1, 0, 0), "");
    if (dl::Door* d = w.doorById("elevator_car")) d->openness = 0.0f;
    w.rooms.push_back({"elevator", "Lift", nf::Box3(nf::V3(x0, y0, 0), nf::V3(x1, y1, zh)),
                       nf::V3(5.8f, 9.35f, 1.6f), 3, false});
    w.addLight(nf::V3(5.8f, 9.35f, zh - 0.2f), nf::V3(0.9f, 0.94f, 1.0f), 1.2f, 3.0f, "elevator");
    w.addInteract({"elevator_panel", "LIFT CALL BUTTON", nf::V3(x0 + 0.22f, 8.75f, 1.3f), 1.8f, InteractType::Elevator, 0});
    w.interactables.back().id = "elevator_panel";
}

// ============================================================================
//  BUILD
// ============================================================================
void World::build(nf::Renderer& r, const std::string& texDir) {
    textures = &r.textures;
    mats.build(r, texDir);
    groups.clear();
    colliders.clear();
    doors.clear();
    interactables.clear();
    rooms.clear();
    fixtures.clear();
    documents.clear();
    props.clear();
    structuralColliders = -1;

    buildMainCorridor();
    buildFixturesAndDetails();
    buildStairwell();
    buildBasement();
    buildElevator(*this);

    // ---- the player's apartment (west block, door faces east onto the corridor)
    buildApartment(V3(-8.9f, 0.0f, 0.0f), nf::HALF_PI, 0, "apt47", true);
    // ---- apartments around the main corridor
    buildApartment(V3(-8.6f, 2.3f, 0.0f), 0.0f, 1, "apt305", false);   // north-west
    buildApartment(V3(1.3f, 2.3f, 0.0f), 0.0f, 2, "apt306", false);    // north-east
    buildApartment(V3(-1.6f, -0.2f, 0.0f), nf::PI, 3, "apt303", false); // south-west
    buildApartment(V3(8.6f, -0.2f, 0.0f), nf::PI, 4, "apt304", false);  // south-east
    buildApartment(V3(8.9f, 2.2f, 0.0f), -nf::HALF_PI, 2, "apt308", false); // east
    finalizeDoors();
    structuralColliders = (int)colliders.size();

    playerSpawn = V3(-11.2f, 1.1f, 1.62f);

    buildNav();
}

// ------------------------------------------------------------------ nav build
void World::buildNav() {
    nav.init(V3(-14.6f, -11.5f, -3.6f), V3(15.0f, 13.6f, 5.7f), 0.25f);
    if (structuralColliders < 0) structuralColliders = (int)colliders.size();
    const int structural = nf::mini(structuralColliders, (int)colliders.size());
    for (int ci = 0; ci < structural; ++ci) {
        Collider& c = colliders[ci];
        if (!c.blocksWalk) continue;
        // door leaves are dynamic: a closed door must not seal a room for the AI,
        // otherwise every apartment becomes unreachable.
        if (c.doorId >= 0) continue;
        // ignore rugs / sills / anything a person simply steps over
        if (c.box.hi.z - c.box.lo.z < 0.06f) continue;
        if (c.box.hi.z < 0.24f) continue;
        // only structural obstacles (walls, shafts, shells) shape the AI graph;
        // furniture is handled by the physics pass and must never seal a doorway
        if (c.box.hi.z - c.box.lo.z < 1.85f) continue;
        nav.setBox(c.box, true, 0);
    }
    // explicit walkability for the two floors: block everything not on a floor surface
    // (the geometry already provides floor colliders; we only need to allow air above floors)
}

} // namespace dl
