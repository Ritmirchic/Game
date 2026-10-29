#include "game/maps.h"
#include "game/globals.h"
#include "game/vehicles.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------- helpers
static int V(float m) { return (int)std::round(m / VOX); }

int MapTarget::current(const World& w) const {
    int x0 = (int)std::floor(box.mn.x / VOX), x1 = (int)std::ceil(box.mx.x / VOX);
    int y0 = (int)std::floor(box.mn.y / VOX), y1 = (int)std::ceil(box.mx.y / VOX);
    int z0 = (int)std::floor(box.mn.z / VOX), z1 = (int)std::ceil(box.mx.z / VOX);
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                if (w.solidAt(x, y, z)) n++;
    return n;
}
float MapTarget::destroyedFraction(const World& w) const {
    if (baseline <= 0) return 0;
    int cur = current(w);
    return clampf(1.0f - (float)cur / (float)baseline, 0, 1);
}

MapTarget* MapBuildResult::addTarget(const std::string& name, const std::string& label, const AABB& box) {
    MapTarget t;
    t.name = name;
    t.label = label;
    t.box = box;
    targets.push_back(t);
    return &targets.back();
}

struct Builder {
    World& w;
    MapBuildResult& out;
    RNG rng{1337};
    float groundY = 2.0f;

    Builder(World& w_, MapBuildResult& o) : w(w_), out(o) {}

    void fill(float mx0, float my0, float mz0, float mx1, float my1, float mz1, uint8_t m) {
        w.box(V(mx0), V(my0), V(mz0), V(mx1) - 1, V(my1) - 1, V(mz1) - 1, m);
    }
    void solid(float mx0, float my0, float mz0, float mx1, float my1, float mz1, uint8_t m) { fill(mx0, my0, mz0, mx1, my1, mz1, m); }
    void clear(float mx0, float my0, float mz0, float mx1, float my1, float mz1) { fill(mx0, my0, mz0, mx1, my1, mz1, M_AIR); }
    bool solidAt(float x, float y, float z) { return w.solidAtV(vec3(x, y, z) * (1.0f / VOX)); }

    void terrain(uint8_t top, uint8_t sub, float topY, float roughness = 0.0f) {
        groundY = topY;
        int ty = V(topY);
        for (int z = 0; z < w.sz; z++)
            for (int x = 0; x < w.sx; x++) {
                float h = topY;
                if (roughness > 0.0f) {
                    float n = fbm3(vec3(x * 0.012f, z * 0.012f, 0.5f), 3, 77) - 0.5f;
                    h += n * roughness * 2.0f;
                }
                int ht = V(h);
                for (int y = 0; y <= ht; y++) w.setRaw(x, y, z, y == ht ? top : sub);
            }
        w.groundVox = ty;
    }

    void road(float x0, float z0, float x1, float z1, uint8_t surface = M_ASPHALT, bool markings = true) {
        int y = V(groundY);
        float cx0 = std::min(x0, x1), cx1 = std::max(x0, x1);
        float cz0 = std::min(z0, z1), cz1 = std::max(z0, z1);
        for (int z = V(cz0); z < V(cz1); z++)
            for (int x = V(cx0); x < V(cx1); x++) {
                w.setRaw(x, y, z, surface);
                if (markings) {
                    bool alongX = (cx1 - cx0) > (cz1 - cz0);
                    float coord = alongX ? (z * VOX) : (x * VOX);
                    float mid = alongX ? (cz0 + cz1) * 0.5f : (cx0 + cx1) * 0.5f;
                    if (std::fabs(coord - mid) < 0.12f && ((alongX ? x : z) / 6) % 3 != 0)
                        w.setRaw(x, y, z, M_PAINT_YELLOW);
                }
            }
    }
    void sidewalk(float x0, float z0, float x1, float z1, uint8_t m = M_CONCRETE) {
        int y = V(groundY) + 1;
        for (int z = V(std::min(z0, z1)); z < V(std::max(z0, z1)); z++)
            for (int x = V(std::min(x0, x1)); x < V(std::max(x0, x1)); x++) {
                w.setRaw(x, y, z, m);
                w.setRaw(x, y - 1, z, M_CONCRETE_DARK);
            }
    }

    // perimeter building shell with floors; returns the inner cavity bounds
    void shell(float x0, float z0, float x1, float z1, float y0, float y1, uint8_t wall, int floorCount, uint8_t floorMat) {
        solid(x0, y0, z0, x1, y1, z1, wall);
        // carve the inside
        float t = 0.5f;
        clear(x0 + t, y0 + t, z0 + t, x1 - t, y1 - t, z1 - t);
        // floors
        float fh = (y1 - y0) / std::max(1, floorCount);
        for (int f = 0; f < floorCount; f++) {
            float fy = y0 + f * fh;
            solid(x0 + t * 0.5f, fy + 0.25f, z0 + t * 0.5f, x1 - t * 0.5f, fy + 0.5f, z1 - t * 0.5f, floorMat);
        }
        // ceiling slab
        solid(x0 + t * 0.5f, y1 - 0.5f, z0 + t * 0.5f, x1 - t * 0.5f, y1 - 0.25f, z1 - t * 0.5f, floorMat);
    }

    void door(float x, float z, float y, bool alongX, uint8_t frameMat = M_WOOD_DARK) {
        // 1.1m wide, 2.2m tall opening through a wall
        float hw = 0.55f, h = 2.2f;
        if (alongX) {
            clear(x - hw, y + 0.3f, z - 0.6f, x + hw, y + h, z + 0.6f);
            solid(x - hw - 0.12f, y + 0.3f, z - 0.12f, x - hw, y + h, z + 0.12f, frameMat);
            solid(x + hw, y + 0.3f, z - 0.12f, x + hw + 0.12f, y + h, z + 0.12f, frameMat);
            solid(x - hw, y + h, z - 0.12f, x + hw, y + h + 0.12f, z + 0.12f, frameMat);
        } else {
            clear(x - 0.6f, y + 0.3f, z - hw, x + 0.6f, y + h, z + hw);
            solid(x - 0.12f, y + 0.3f, z - hw - 0.12f, x + 0.12f, y + h, z - hw, frameMat);
            solid(x - 0.12f, y + 0.3f, z + hw, x + 0.12f, y + h, z + hw + 0.12f, frameMat);
            solid(x - 0.12f, y + h, z - hw, x + 0.12f, y + h + 0.12f, z + hw, frameMat);
        }
    }

    void window(float x, float z, float y, float w_, float h_, bool alongX, uint8_t glassMat = M_GLASS) {
        if (alongX) {
            clear(x - w_ * 0.5f, y, z - 0.6f, x + w_ * 0.5f, y + h_, z + 0.6f);
            solid(x - w_ * 0.5f, y, z - 0.1f, x + w_ * 0.5f, y + h_, z + 0.1f, glassMat);
            solid(x - w_ * 0.5f - 0.12f, y - 0.12f, z - 0.14f, x + w_ * 0.5f + 0.12f, y, z + 0.14f, M_WHITE_PANEL);
        } else {
            clear(x - 0.6f, y, z - w_ * 0.5f, x + 0.6f, y + h_, z + w_ * 0.5f);
            solid(x - 0.1f, y, z - w_ * 0.5f, x + 0.1f, y + h_, z + w_ * 0.5f, glassMat);
            solid(x - 0.14f, y - 0.12f, z - w_ * 0.5f - 0.12f, x + 0.14f, y, z + w_ * 0.5f + 0.12f, M_WHITE_PANEL);
        }
    }

    void stairs(float x, float z, float y0, float y1, bool alongX, float width, uint8_t m = M_CONCRETE) {
        float rise = 0.25f, run = 0.34f;
        int steps = (int)std::round((y1 - y0) / rise);
        for (int i = 0; i < steps; i++) {
            float sy = y0 + i * rise;
            float sPos = i * run;
            if (alongX) solid(x + sPos, sy, z, x + sPos + run, sy + rise, z + width, m);
            else solid(x, sy, z + sPos, x + width, sy + rise, z + sPos + run, m);
        }
    }

    void railing(float x0, float z0, float x1, float z1, float y, uint8_t m = M_METAL_DARK) {
        float len = std::sqrt((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0));
        int n = (int)(len / 1.2f);
        for (int i = 0; i <= n; i++) {
            float t = (float)i / std::max(1, n);
            float px = lerpf(x0, x1, t), pz = lerpf(z0, z1, t);
            solid(px - 0.06f, y, pz - 0.06f, px + 0.06f, y + 1.05f, pz + 0.06f, m);
        }
        // top rail
        float hx = (x1 - x0), hz = (z1 - z0);
        float l = std::sqrt(hx * hx + hz * hz);
        for (float t = 0; t <= l; t += 0.22f) {
            float px = x0 + hx * (t / std::max(0.01f, l)), pz = z0 + hz * (t / std::max(0.01f, l));
            solid(px - 0.05f, y + 1.0f, pz - 0.05f, px + 0.05f, y + 1.1f, pz + 0.05f, m);
        }
    }

    void tree(float x, float z, float height, float radius) {
        float gy = groundHeight(x, z);
        int trunkR = 1;
        int tx = V(x), tz = V(z);
        int ty0 = V(gy), ty1 = V(gy + height);
        for (int y = ty0; y <= ty1; y++)
            for (int dz = -trunkR; dz <= trunkR; dz++)
                for (int dx = -trunkR; dx <= trunkR; dx++) w.setRaw(tx + dx, y, tz + dz, M_TRUNK);
        // canopy: a few overlapping spheres
        vec3 c(x, gy + height, z);
        w.sphere(c * (1.0f / VOX), radius / VOX, M_FOLIAGE);
        w.sphere((c + vec3(radius * 0.6f, -radius * 0.4f, 0)) * (1.0f / VOX), radius * 0.72f / VOX, M_FOLIAGE);
        w.sphere((c + vec3(-radius * 0.5f, -radius * 0.25f, radius * 0.4f)) * (1.0f / VOX), radius * 0.66f / VOX, M_FOLIAGE);
        w.sphere((c + vec3(0.1f, radius * 0.55f, -radius * 0.3f)) * (1.0f / VOX), radius * 0.6f / VOX, M_FOLIAGE);
    }

    void bush(float x, float z, float r) {
        float gy = groundHeight(x, z);
        w.sphere(vec3(x, gy + r * 0.7f, z) * (1.0f / VOX), r / VOX, M_FOLIAGE);
    }

    void streetlight(float x, float z, bool alongX) {
        float gy = groundHeight(x, z);
        solid(x - 0.09f, gy, z - 0.09f, x + 0.09f, gy + 7.0f, z + 0.09f, M_METAL_DARK);
        if (alongX) {
            solid(x, gy + 6.8f, z - 0.08f, x + 1.8f, gy + 7.0f, z + 0.08f, M_METAL_DARK);
            solid(x + 1.5f, gy + 6.5f, z - 0.22f, x + 2.1f, gy + 6.85f, z + 0.22f, M_LAMP);
        } else {
            solid(x - 0.08f, gy + 6.8f, z, x + 0.08f, gy + 7.0f, z + 1.8f, M_METAL_DARK);
            solid(x - 0.22f, gy + 6.5f, z + 1.5f, x + 0.22f, gy + 6.85f, z + 2.1f, M_LAMP);
        }
    }

    void fence(float x0, float z0, float x1, float z1, float h, uint8_t m = M_WOOD) {
        float len = std::sqrt((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0));
        int n = std::max(1, (int)(len / 0.9f));
        for (int i = 0; i <= n; i++) {
            float t = (float)i / n;
            float px = lerpf(x0, x1, t), pz = lerpf(z0, z1, t);
            float gy = groundHeight(px, pz);
            solid(px - 0.07f, gy, pz - 0.07f, px + 0.07f, gy + h, pz + 0.07f, m);
        }
        // rails
        for (float f = 0; f < len; f += 0.25f) {
            float t = f / std::max(0.01f, len);
            float px = lerpf(x0, x1, t), pz = lerpf(z0, z1, t);
            float gy = groundHeight(px, pz);
            for (int r = 0; r < 2; r++) {
                float ry = gy + 0.35f + r * 0.65f;
                solid(px - 0.05f, ry, pz - 0.05f, px + 0.05f, ry + 0.14f, pz + 0.05f, m);
            }
        }
    }

    void concreteWall(float x0, float z0, float x1, float z1, float y, float h, float t, uint8_t m) {
        float len = std::sqrt((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0));
        for (float f = 0; f <= len; f += 0.2f) {
            float tt = f / std::max(0.01f, len);
            float px = lerpf(x0, x1, tt), pz = lerpf(z0, z1, tt);
            solid(px - t * 0.5f, y, pz - t * 0.5f, px + t * 0.5f, y + h, pz + t * 0.5f, m);
        }
    }

    float groundHeight(float x, float z) {
        int vx = V(x), vz = V(z);
        for (int y = w.sy - 1; y >= 0; y--)
            if (w.solidAt(vx, y, vz)) return (y + 1) * VOX;
        return 0;
    }

    // ---- interior furniture made of voxels (fully destructible) ----
    void table(float x, float z, float gy, float w_, float d_, uint8_t m = M_WOOD) {
        solid(x, gy, z, x + w_, gy + 0.06f, z + d_, m);                        // top
        solid(x, gy - 0.75f, z, x + 0.08f, gy, z + 0.08f, M_WOOD_DARK);        // legs
        solid(x + w_ - 0.08f, gy - 0.75f, z, x + w_, gy, z + 0.08f, M_WOOD_DARK);
        solid(x, gy - 0.75f, z + d_ - 0.08f, x + 0.08f, gy, z + d_, M_WOOD_DARK);
        solid(x + w_ - 0.08f, gy - 0.75f, z + d_ - 0.08f, x + w_, gy, z + d_, M_WOOD_DARK);
    }
    void chair(float x, float z, float gy) {
        solid(x, gy - 0.45f, z, x + 0.45f, gy - 0.38f, z + 0.45f, M_WOOD_DARK);
        solid(x, gy - 0.45f, z, x + 0.45f, gy - 0.05f, z + 0.07f, M_WOOD);
        for (int i = 0; i < 4; i++) {
            float lx = x + (i & 1) * 0.37f, lz = z + ((i >> 1) & 1) * 0.37f;
            solid(lx, gy - 0.45f, lz, lx + 0.07f, gy - 0.02f, lz + 0.07f, M_WOOD_DARK);
        }
    }
    void bed(float x, float z, float gy) {
        solid(x, gy - 0.55f, z, x + 1.4f, gy - 0.30f, z + 2.0f, M_PLANK);
        solid(x + 0.05f, gy - 0.30f, z + 0.05f, x + 1.35f, gy - 0.12f, z + 1.9f, M_WHITE_PANEL);
        solid(x, gy - 0.55f, z, x + 0.1f, gy + 0.35f, z + 2.0f, M_WOOD_DARK);
    }
    void shelf(float x, float z, float gy, float w_, uint8_t m = M_WOOD_DARK) {
        solid(x, gy, z, x + w_, gy + 0.05f, z + 0.35f, m);
        for (int i = 0; i < 4; i++) solid(x, gy - 1.9f + i * 0.6f, z, x + w_, gy - 1.85f + i * 0.6f, z + 0.35f, m);
        solid(x, gy - 1.9f, z, x + 0.05f, gy, z + 0.35f, m);
        solid(x + w_ - 0.05f, gy - 1.9f, z, x + w_, gy, z + 0.35f, m);
    }
    void counter(float x, float z, float gy, float w_) {
        solid(x, gy - 0.95f, z, x + w_, gy, z + 0.7f, M_WOOD_DARK);
        solid(x, gy, z, x + w_, gy + 0.05f, z + 0.75f, M_TILE);
    }
    void sofa(float x, float z, float gy, uint8_t m = M_CONTAINER_GREEN) {
        solid(x, gy - 0.45f, z, x + 2.0f, gy, z + 0.85f, m);
        solid(x, gy - 0.45f, z, x + 2.0f, gy + 0.5f, z + 0.15f, m);
    }
    void toilet(float x, float z, float gy) {
        solid(x, gy - 0.45f, z, x + 0.45f, gy + 0.4f, z + 0.6f, M_WHITE_PANEL);
        solid(x - 0.05f, gy - 0.45f, z - 0.1f, x + 0.5f, gy + 0.05f, z + 0.7f, M_WHITE_PANEL);
    }
    void machinery(float x, float y, float z, float s, uint8_t m) {
        solid(x, y, z, x + s, y + s * 0.8f, z + s * 0.7f, m);
        solid(x + s * 0.15f, y + s * 0.8f, z + s * 0.15f, x + s * 0.6f, y + s * 1.1f, z + s * 0.5f, M_METAL_DARK);
    }

    void crateStack(float x, float z, int n) {
        for (int i = 0; i < n; i++) {
            float gy = groundHeight(x, z) + i * 0.62f;
            solid(x, gy, z, x + 0.6f, gy + 0.6f, z + 0.6f, M_PLANK);
        }
    }
    void pipe(float x0, float y0, float z0, float x1, float y1, float z1, float r, uint8_t m) {
        w.line3(vec3(x0, y0, z0) * (1.0f / VOX), vec3(x1, y1, z1) * (1.0f / VOX), r / VOX, m);
    }
};

// ---------------------------------------------------------------- house generator
static void residentialHouse(Builder& b, float x0, float z0, float w, float d, int floors, uint8_t extWall,
                             uint8_t palette, float floorH, bool gable, bool garage) {
    float y0 = b.groundHeight(x0 + w * 0.5f, z0 + d * 0.5f);
    float y1 = y0 + floors * floorH;
    b.shell(x0, z0, x0 + w, z0 + d, y0, y1, extWall, floors, M_CONCRETE);
    // brick trim around windows / base
    b.solid(x0 - 0.06f, y0, z0 - 0.06f, x0 + w + 0.06f, y0 + 0.8f, z0 + d + 0.06f, M_BRICK);
    // door on the street-facing side (+z or -z)
    bool frontIsPlusZ = true;
    float doorX = x0 + w * 0.5f;
    float doorZ = frontIsPlusZ ? z0 + d : z0;
    b.door(doorX, doorZ, y0, true, M_WOOD_DARK);
    // porch
    b.solid(doorX - 1.1f, y0 - 0.35f, frontIsPlusZ ? doorZ : doorZ - 1.6f, doorX + 1.1f, y0 + 0.05f,
            frontIsPlusZ ? doorZ + 1.6f : doorZ, M_CONCRETE);
    // windows
    for (int f = 0; f < floors; f++) {
        float wy = y0 + f * floorH + 1.1f;
        int nx = std::max(1, (int)(w / 3.4f));
        for (int i = 0; i < nx; i++) {
            float wx = x0 + 1.4f + i * (w - 2.8f) / std::max(1, nx - 1);
            if (std::fabs(wx - doorX) > 1.4f || f > 0) {
                b.window(wx, z0, wy, 1.3f, 1.15f, true);
                b.window(wx, z0 + d, wy, 1.3f, 1.15f, true);
            }
        }
        int nz = std::max(1, (int)(d / 3.4f));
        for (int i = 0; i < nz; i++) {
            float wz = z0 + 1.4f + i * (d - 2.8f) / std::max(1, nz - 1);
            b.window(x0, wz, wy, 1.3f, 1.15f, false);
            b.window(x0 + w, wz, wy, 1.3f, 1.15f, false);
        }
    }
    // interior partitions with doorways
    for (int f = 0; f < floors; f++) {
        float fy = y0 + f * floorH;
        float wallY0 = fy + 0.5f, wallY1 = fy + floorH - 0.3f;
        if (f == 0) {
            // living room / kitchen split
            b.concreteWall(x0 + w * 0.55f, z0 + 0.5f, x0 + w * 0.55f, z0 + d * 0.55f, wallY0, wallY1 - wallY0, 0.28f, palette);
            b.door(x0 + w * 0.55f, z0 + d * 0.62f, fy, false);
            b.counter(x0 + 0.8f, z0 + d * 0.68f, fy + 0.5f, 2.8f);
            b.shelf(x0 + w * 0.62f, z0 + 1.0f, fy + 0.5f, 1.6f);
            b.table(x0 + 2.2f, z0 + 2.2f, fy + 0.95f, 1.5f, 0.9f);
            b.chair(x0 + 1.6f, z0 + 2.3f, fy + 0.85f);
            b.chair(x0 + 3.8f, z0 + 2.3f, fy + 0.85f);
            b.sofa(x0 + 1.2f, z0 + d - 2.2f, fy + 0.5f);
            b.table(x0 + 1.4f, z0 + d - 2.9f, fy + 0.65f, 1.5f, 0.6f);
        } else {
            b.concreteWall(x0 + 0.5f, z0 + d * 0.5f, x0 + w - 0.5f, z0 + d * 0.5f, wallY0, wallY1 - wallY0, 0.28f, palette);
            b.door(x0 + w * 0.30f, z0 + d * 0.5f, fy, true);
            b.bed(x0 + 1.0f, z0 + 1.0f, fy + 0.5f);
            b.bed(x0 + w - 2.6f, z0 + d * 0.15f, fy + 0.5f);
            b.shelf(x0 + w * 0.35f, z0 + 1.2f, fy + 0.5f, 1.4f);
            b.table(x0 + 2.0f, z0 + d * 0.68f, fy + 0.95f, 1.2f, 0.7f);
        }
    }
    // staircase along the back wall
    if (floors > 1) {
        float sx = x0 + w - 3.2f;
        float sz = z0 + d - 1.4f;
        b.stairs(sx, sz, y0 + 0.5f, y0 + floorH + 0.5f, false, 1.3f);
        // stairwell opening in the floor above
        b.clear(sx, y0 + floorH + 0.2f, sz, sx + 1.4f, y0 + floorH + 0.65f, sz + 3.2f);
        b.railing(sx - 0.2f, sz, sx - 0.2f, sz + 3.0f, y0 + floorH + 0.5f);
    }
    // roof
    if (gable) {
        int steps = V(w * 0.5f);
        for (int i = 0; i < steps; i++) {
            float ry = y1 + (i * (w * 0.5f) / std::max(1, steps)) * 0.55f;
            int vx0 = V(x0) + i, vx1 = V(x0 + w) - i;
            if (vx1 <= vx0) break;
            int z0v = V(z0) - 1, z1v = V(z0 + d) + 1;
            int ryv = V(ry);
            for (int z = z0v; z <= z1v; z++) {
                b.w.setRaw(vx0, ryv, z, M_ROOF);
                b.w.setRaw(vx1, ryv, z, M_ROOF);
                b.w.setRaw(vx0, ryv + 1, z, M_ROOF);
                b.w.setRaw(vx1, ryv + 1, z, M_ROOF);
            }
        }
        // gable end walls
        for (int i = 0; i < steps; i++) {
            float ry = y1 + (i * (w * 0.5f) / std::max(1, steps)) * 0.55f;
            int vx0 = V(x0) + i, vx1 = V(x0 + w) - i;
            if (vx1 <= vx0) break;
            b.w.box(vx0, V(ry), V(z0), vx1, V(ry), V(z0), extWall);
            b.w.box(vx0, V(ry), V(z0 + d) - 1, vx1, V(ry), V(z0 + d) - 1, extWall);
        }
        b.solid(x0 - 0.15f, y1 - 0.35f, z0 - 0.15f, x0 + w + 0.15f, y1, z0 + d + 0.15f, M_ROOF);
    } else {
        b.solid(x0 - 0.2f, y1 - 0.3f, z0 - 0.2f, x0 + w + 0.2f, y1 + 0.15f, z0 + d + 0.2f, M_ROOF);
    }
    // garage annex
    if (garage) {
        float gx = x0 + w + 0.2f, gz = z0 + 1.0f, gw = 4.4f, gd = 5.4f;
        float gy = b.groundHeight(gx + gw * 0.5f, gz + gd * 0.5f);
        b.shell(gx, gz, gx + gw, gz + gd, gy, gy + 2.9f, palette, 1, M_CONCRETE);
        b.clear(gx + 0.4f, gy, gz + gd * 0.5f - 1.4f, gx + gw - 0.4f, gy + 2.3f, gz + gd + 1.0f);
        b.solid(gx - 0.2f, gy + 2.9f, gz - 0.2f, gx + gw + 0.2f, gy + 3.1f, gz + gd + 0.2f, M_ROOF);
        b.shelf(gx + 0.5f, gz + 0.5f, gy + 0.1f, 1.8f, M_METAL_DARK);
    }
}

// ---------------------------------------------------------------- map 1: Westhaven Suburbs
static void genWesthaven(World& w, MapBuildResult& out) {
    Builder b(w, out);
    b.terrain(M_GRASS, M_SOIL, 2.0f, 0.10f);
    // grass strip
    for (int z = 0; z < w.sz; z++)
        for (int x = 0; x < w.sx; x++) {
            float n = fbm3(vec3(x * 0.03f, z * 0.03f, 1), 2, 13);
            if (n > 0.48f) w.setRaw(x, V(2.0f), z, M_FOLIAGE);
        }
    // main street (east-west) and cross street
    b.road(-4, 34, 84, 46);
    b.road(38, -4, 50, 84);
    b.sidewalk(-4, 32.6f, 84, 34, M_CONCRETE);
    b.sidewalk(-4, 46, 84, 47.4f, M_CONCRETE);
    b.sidewalk(36.6f, -4, 38, 84, M_CONCRETE);
    b.sidewalk(50, -4, 51.4f, 84, M_CONCRETE);
    // streetlights along the main street
    for (int i = 0; i < 6; i++) {
        b.streetlight(6 + i * 13.0f, 32.0f, false);
        b.streetlight(12 + i * 13.0f, 47.8f, true);
    }
    // --- houses ---
    residentialHouse(b, 6, 6, 11, 9, 2, M_PLASTER, M_PLASTER, 3.0f, true, false);
    residentialHouse(b, 24, 6, 12, 10, 2, M_BRICK, M_PLASTER, 3.0f, true, true);
    residentialHouse(b, 56, 6, 12, 10, 2, M_PLASTER, M_PLASTER, 3.0f, true, false);
    residentialHouse(b, 4, 52, 13, 10, 2, M_BRICK, M_PLASTER, 3.0f, true, true);
    residentialHouse(b, 24, 54, 11, 9, 2, M_PLASTER, M_PLASTER, 3.0f, true, false);
    residentialHouse(b, 62, 52, 12, 10, 2, M_CONCRETE, M_PLASTER, 3.0f, false, false);
    // destructible building targets (baseline captured once generation is done)
    out.addTarget("house0", "HOUSE 6", AABB(vec3(6, 1.6f, 6), vec3(17, 9.5f, 15)));
    out.addTarget("house1", "HOUSE 14", AABB(vec3(24, 1.6f, 6), vec3(36, 9.5f, 16)));
    out.addTarget("house2", "HOUSE 20", AABB(vec3(56, 1.6f, 6), vec3(68, 9.5f, 16)));
    out.addTarget("house3", "HOUSE 31", AABB(vec3(4, 1.6f, 52), vec3(17, 9.5f, 62)));
    out.addTarget("house4", "HOUSE 37", AABB(vec3(24, 1.6f, 54), vec3(35, 9.5f, 63)));
    out.addTarget("house5", "HOUSE 42", AABB(vec3(62, 1.6f, 52), vec3(74, 9.5f, 62)));
    // fences and yards
    b.fence(4, 16.5f, 18, 16.5f, 1.1f);
    b.fence(20, 16.5f, 36, 16.5f, 1.1f);
    b.fence(54, 16.5f, 70, 16.5f, 1.1f);
    b.tree(20, 28, 6.0f, 2.6f);
    b.tree(52, 24, 7.0f, 3.0f);
    b.tree(74, 30, 6.5f, 2.8f);
    b.tree(3, 30, 5.0f, 2.4f);
    b.tree(78, 60, 7.0f, 3.0f);
    b.bush(16, 20, 1.0f);
    b.bush(48, 26, 0.9f);
    // swimming pool at house 5
    b.clear(26, 1.2f, 62, 33, 2.1f, 69);
    b.solid(25.6f, 1.9f, 61.6f, 33.4f, 2.05f, 69.4f, M_WATER);
    b.solid(25.4f, 1.85f, 61.4f, 26.0f, 2.2f, 69.6f, M_TILE);
    b.solid(33.0f, 1.85f, 61.4f, 33.6f, 2.2f, 69.6f, M_TILE);
    // --- convenience store at (58..74, 22..30) ---
    {
        float x0 = 58, z0 = 18, x1 = 76, z1 = 30;
        b.shell(x0, z0, x1, z1, 2.0f, 6.5f, M_CONCRETE, 1, M_TILE);
        b.clear(x0 + 2.0f, 2.1f, z0 - 0.6f, x0 + 5.0f, 4.6f, z0 + 0.6f);      // entrance
        b.window(x0 + 8.0f, z0, 2.6f, 5.0f, 2.2f, true);
        b.window(x0 + 15.0f, z0, 2.6f, 4.0f, 2.2f, true);
        b.window(x0, z0 + 6.0f, 2.6f, 5.0f, 2.2f, false);
        // shelves
        for (int i = 0; i < 3; i++) b.shelf(x0 + 3.0f + i * 4.2f, z0 + 3.0f, 4.4f, 3.0f, M_METAL);
        b.counter(x0 + 13.0f, z0 + 8.0f, 3.5f, 4.0f);
        b.solid(x0 - 0.2f, 6.5f, z0 - 0.2f, x1 + 0.2f, 6.8f, z1 + 0.2f, M_CONCRETE_DARK);
        // sign
        b.solid(x0 + 3, 6.9f, z0 - 0.3f, x0 + 12, 8.2f, z0 - 0.05f, M_PAINT_RED);
        out.addTarget("store", "Convenience store", AABB(vec3(x0, 2, z0), vec3(x1, 8.2f, z1)));
    }
    // --- gas station (20..34, 72..84) ---
    {
        float cx = 26, cz = 76;
        b.solid(cx - 6, 5.0f, cz - 4, cx + 6, 5.5f, cz + 4, M_WHITE_PANEL);      // canopy
        for (int i = 0; i < 4; i++) {
            float px = cx - 5 + (i & 1) * 10;
            float pz = cz - 3 + ((i >> 1) & 1) * 6;
            b.solid(px - 0.2f, 2.0f, pz - 0.2f, px + 0.2f, 5.0f, pz + 0.2f, M_WHITE_PANEL);
        }
        // pumps
        for (int i = 0; i < 2; i++) {
            float px = cx - 2.5f + i * 5.0f;
            b.solid(px - 0.4f, 2.0f, cz - 0.4f, px + 0.4f, 3.4f, cz + 0.4f, M_PAINT_RED);
            b.solid(px - 0.3f, 3.4f, cz - 0.3f, px + 0.3f, 3.7f, cz + 0.3f, M_METAL_DARK);
        }
        // kiosk
        b.shell(cx + 7, cz - 2, cx + 12, cz + 3, 2.0f, 5.0f, M_CONCRETE, 1, M_TILE);
        b.clear(cx + 8.5f, 2.1f, cz - 2.6f, cx + 10.5f, 4.2f, cz - 1.8f);
        out.props.push_back({3, vec3(cx - 7.5f, 2.5f, cz + 2)});   // fuel tank prop
        out.props.push_back({3, vec3(cx - 7.5f, 2.5f, cz + 4)});
        out.addTarget("gasstation", "Gas station", AABB(vec3(cx - 7, 2, cz - 5), vec3(cx + 13, 6, cz + 5)));
    }
    // --- construction site (44..58, 62..82) ---
    {
        float x0 = 46, z0 = 64, x1 = 60, z1 = 80;
        // unfinished concrete frame, 4 stories
        for (int f = 0; f < 4; f++) {
            float fy = 2.0f + f * 3.0f;
            for (int i = 0; i < 4; i++) {
                float px = x0 + (i & 1) * (x1 - x0 - 0.8f);
                float pz = z0 + ((i >> 1) & 1) * (z1 - z0 - 0.8f);
                b.solid(px, fy, pz, px + 0.8f, fy + 3.0f, pz + 0.8f, M_CONCRETE);
            }
            b.solid(x0, fy + 2.7f, z0, x1, fy + 3.0f, z1, M_CONCRETE);
        }
        // core
        b.solid(x0 + 5, 2.0f, z0 + 6, x0 + 8, 14.0f, z0 + 9, M_REINFORCED);
        // scaffolding on the west face
        for (int f = 0; f < 4; f++) {
            float fy = 2.0f + f * 3.0f;
            for (int i = 0; i < 4; i++) {
                float pz = z0 + 1 + i * 4.0f;
                b.solid(x0 - 1.0f, fy, pz, x0 - 0.85f, fy + 3.0f, pz + 0.15f, M_METAL);
                b.solid(x0 - 1.0f, fy + 2.6f, pz, x0 - 0.3f, fy + 2.75f, pz + 0.15f, M_PLANK);
            }
            b.solid(x0 - 1.05f, fy + 1.4f, z0 + 1, x0 - 0.95f, fy + 1.5f, z1 - 1, M_PLANK);
        }
        out.props.push_back({1, vec3(x0 - 3, 2.5f, z0 + 4)});
        out.props.push_back({1, vec3(x0 - 3.5f, 3.2f, z0 + 7)});
        out.props.push_back({0, vec3(x0 - 4, 2.5f, z0 + 10)});
        out.addTarget("construction", "Construction site", AABB(vec3(x0 - 2, 2, z0), vec3(x1, 14, z1)));
    }
    out.vehicles.push_back({VK_SEDAN, vec3(12, 2.2f, 40), PI * 0.5f});
    out.vehicles.push_back({VK_PICKUP, vec3(30, 2.2f, 41), PI * 0.5f});
    out.vehicles.push_back({VK_BULLDOZER, vec3(44, 2.2f, 74), 0.0f});
    out.vehicles.push_back({VK_SEDAN, vec3(44, 2.2f, 20), -PI * 0.5f});
    out.hints.push_back("Brick and plaster walls are weak - the sledgehammer opens them in a few hits.");
    out.hints.push_back("Supports matter: knock out a house's load-bearing corners and the roof comes down.");
    out.hints.push_back("The bulldozer at the construction site clears rubble and walls alike.");
}

// ---------------------------------------------------------------- map 2: Blackstone Industrial
static void genBlackstone(World& w, MapBuildResult& out) {
    Builder b(w, out);
    b.terrain(M_CONCRETE_DARK, M_GRAVEL, 2.0f, 0.05f);
    // perimeter roads
    b.road(-4, 40, 88, 52);
    b.road(40, -4, 52, 88);
    // --- Big warehouse ---
    {
        float x0 = 58, z0 = 6, x1 = 86, z1 = 36;
        b.solid(x0, 2.0f, z0, x1, 9.0f, z0 + 0.5f, M_METAL);            // walls
        b.solid(x0, 2.0f, z1 - 0.5f, x1, 9.0f, z1, M_METAL);
        b.solid(x0, 2.0f, z0, x0 + 0.5f, 9.0f, z1, M_METAL);
        b.solid(x1 - 0.5f, 2.0f, z0, x1, 9.0f, z1, M_METAL);
        b.solid(x0, 8.5f, z0, x1, 9.0f, z1, M_METAL_DARK);              // roof
        // roof trusses
        for (float tx = x0 + 3; tx < x1; tx += 5.0f) b.solid(tx, 8.0f, z0, tx + 0.3f, 8.5f, z1, M_METAL_DARK);
        // main loading bay opening
        b.clear(x0 + 4, 2.0f, z0, x0 + 10, 6.2f, z0 + 0.6f);
        b.clear(x0 + 14, 2.0f, z0, x0 + 20, 6.2f, z0 + 0.6f);
        b.clear(x0, 2.0f, z0 + 10, x0 + 0.6f, 5.0f, z0 + 16);
        // interior racking
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 3; c++) {
                float px = x0 + 4 + c * 8.0f, pz = z0 + 6 + r * 7.0f;
                b.shelf(px, pz, 6.5f, 3.0f, M_METAL_DARK);
                b.shelf(px, pz + 1.0f, 6.5f, 3.0f, M_METAL_DARK);
            }
        // mezzanine office
        b.solid(x1 - 16, 5.5f, z0 + 0.5f, x1 - 0.5f, 5.9f, z0 + 8, M_GRATE);
        b.railing(x1 - 16, z0 + 8, x1 - 0.5f, z0 + 8, 5.9f);
        b.solid(x1 - 16, 5.9f, z0 + 0.5f, x1 - 15.6f, 8.5f, z0 + 8, M_WHITE_PANEL);
        b.stairs(x1 - 6, z0 + 8.5f, 2.0f, 5.9f, false, 1.4f, M_GRATE);
        out.props.push_back({0, vec3(x0 + 22, 2.5f, z0 + 4)});
        out.props.push_back({0, vec3(x0 + 22.5f, 2.5f, z0 + 5)});
        out.props.push_back({4, vec3(x0 + 12, 2.5f, z0 + 26)});
        out.props.push_back({4, vec3(x0 + 12.5f, 2.5f, z0 + 27)});
        out.addTarget("warehouse", "Warehouse A", AABB(vec3(x0, 2, z0), vec3(x1, 9, z1)));
    }
    // --- Factory hall with machinery ---
    {
        float x0 = 8, z0 = 8, x1 = 36, z1 = 34;
        b.solid(x0, 2.0f, z0, x1, 11.0f, z0 + 0.6f, M_CONCRETE);
        b.solid(x0, 2.0f, z1 - 0.6f, x1, 11.0f, z1, M_CONCRETE);
        b.solid(x0, 2.0f, z0, x0 + 0.6f, 11.0f, z1, M_CONCRETE);
        b.solid(x1 - 0.6f, 2.0f, z0, x1, 11.0f, z1, M_CONCRETE);
        b.solid(x0, 10.6f, z0, x1, 11.0f, z1, M_METAL_DARK);
        b.clear(x0 + 10, 2.0f, z1 - 0.7f, x0 + 17, 6.5f, z1 + 0.1f);    // big door
        // skylights
        for (int i = 0; i < 4; i++) b.solid(x0 + 4 + i * 6.0f, 10.4f, z0 + 6, x0 + 8 + i * 6.0f, 10.62f, z0 + 22, M_SKYLIGHT);
        // structural columns
        for (int cx = 0; cx < 3; cx++)
            for (int cz = 0; cz < 3; cz++)
                b.solid(x0 + 5 + cx * 9.0f, 2.0f, z0 + 6 + cz * 9.0f, x0 + 5.0f + cx * 9.0f + 0.7f, 10.6f, z0 + 6.0f + cz * 9.0f + 0.7f, M_REINFORCED);
        // machinery
        b.machinery(x0 + 4, 3.0f, z0 + 10, 2.6f, M_METAL);
        b.machinery(x0 + 9, 3.0f, z0 + 16, 3.2f, M_METAL_DARK);
        b.machinery(x0 + 16, 3.0f, z0 + 12, 2.2f, M_RUST);
        b.pipe(x0 + 6, 9.0f, z0 + 12, x0 + 26, 9.0f, z0 + 12, 0.28f, M_RUST);
        b.pipe(x0 + 6, 9.0f, z0 + 12, x0 + 6, 9.0f, z1 - 4, 0.28f, M_RUST);
        // control room
        b.solid(x0 + 22, 2.0f, z0 + 22, x1 - 0.6f, 5.6f, z1 - 0.6f, M_WHITE_PANEL);
        b.clear(x0 + 23, 2.1f, z0 + 23, x1 - 1.5f, 5.5f, z1 - 1.5f);
        b.door(x0 + 22, z0 + 26, 2.0f, false);
        out.props.push_back({4, vec3(x0 + 20, 2.5f, z0 + 6)});
        out.props.push_back({4, vec3(x0 + 20.6f, 2.5f, z0 + 6.6f)});
        out.addTarget("factory", "Factory hall", AABB(vec3(x0, 2, z0), vec3(x1, 11, z1)));
    }
    // --- storage tanks ---
    {
        float cx = 22, cz = 52;
        for (int i = 0; i < 3; i++) {
            float px = cx + i * 9.0f;
            b.solid(px - 3.2f, 2.0f, cz - 3.2f, px + 3.2f, 12.0f, cz + 3.2f, M_METAL);
            b.clear(px - 2.8f, 2.2f, cz - 2.8f, px + 2.8f, 11.6f, cz + 2.8f);
            b.solid(px - 3.4f, 11.6f, cz - 3.4f, px + 3.4f, 12.2f, cz + 3.4f, M_METAL_DARK);
            b.pipe(px, 12.2f, cz, px, 9.0f, cz + 8, 0.3f, M_RUST);
            out.props.push_back({3, vec3(px + 5.0f, 3.0f, cz + 4.0f)});
            out.props.push_back({4, vec3(px - 4.6f, 2.6f, cz + 4.4f)});
        }
        out.addTarget("tanks", "Fuel tank farm", AABB(vec3(cx - 5, 2, cz - 5), vec3(cx + 24, 13, cz + 8)));
    }
    // --- gantry crane ---
    {
        float cx = 28, cz = 70;
        b.solid(cx - 0.6f, 2.0f, cz - 8, cx + 0.6f, 22.0f, cz + 8, M_METAL_DARK);      // legs
        b.solid(cx + 16, 2.0f, cz - 8, cx + 17.2f, 22.0f, cz + 8, M_METAL_DARK);
        b.solid(cx - 1, 21.0f, cz - 9, cx + 18, 23.0f, cz + 9, M_METAL);               // bridge
        b.solid(cx + 6, 15.0f, cz - 1, cx + 10, 21.0f, cz + 1, M_PAINT_YELLOW);        // trolley
        out.addTarget("crane", "Gantry crane", AABB(vec3(cx - 1, 2, cz - 9), vec3(cx + 18, 23, cz + 9)));
    }
    // --- offices ---
    {
        float x0 = 58, z0 = 54, x1 = 80, z1 = 70;
        b.shell(x0, z0, x1, z1, 2.0f, 11.0f, M_CONCRETE, 3, M_TILE);
        b.door(x0 + 6, z0, 2.0f, true);
        for (int f = 0; f < 3; f++) {
            float fy = 2.0f + f * 3.0f;
            b.window(x0 + 5, z0, fy + 1.2f, 4.0f, 1.4f, true);
            b.window(x0 + 14, z0, fy + 1.2f, 4.0f, 1.4f, true);
        }
        b.table(x0 + 4, z0 + 4, 5.0f, 2.0f, 1.0f);
        b.shelf(x0 + 12, z0 + 3, 5.0f, 2.0f, M_METAL_DARK);
        out.addTarget("offices", "Site offices", AABB(vec3(x0, 2, z0), vec3(x1, 11, z1)));
    }
    out.vehicles.push_back({VK_TRUCK, vec3(60, 2.2f, 44), 0.0f});
    out.vehicles.push_back({VK_FORKLIFT, vec3(70, 2.2f, 40), PI});
    out.vehicles.push_back({VK_ARMORED, vec3(20, 2.2f, 44), 0.0f});
    out.vehicles.push_back({VK_EXCAVATOR, vec3(30, 2.2f, 42), PI * 0.5f});
    out.hints.push_back("Fuel tanks chain-react: one charge can take the whole farm.");
    out.hints.push_back("Heavy machinery swallows concrete - but reinforced columns hold longer.");
    out.hints.push_back("The gantry crane can be toppled by cutting its legs.");
}

// ---------------------------------------------------------------- map 3: Downtown Collapse
static void cityBuilding(Builder& b, float x0, float z0, float x1, float z1, int floors, uint8_t facade, bool shops) {
    float y0 = 2.0f;
    float fh = 3.2f;
    float y1 = y0 + floors * fh;
    b.shell(x0, z0, x1, z1, y0, y1, facade, floors, M_CONCRETE);
    // structural columns
    for (int cx = 0; cx < 3; cx++)
        for (int cz = 0; cz < 3; cz++) {
            float px = x0 + 0.8f + cx * (x1 - x0 - 2.0f) * 0.5f;
            float pz = z0 + 0.8f + cz * (z1 - z0 - 2.0f) * 0.5f;
            b.solid(px, y0, pz, px + 0.7f, y1, pz + 0.7f, M_REINFORCED);
        }
    // curtain wall glazing on the two street faces
    for (int f = 1; f < floors; f++) {
        float fy = y0 + f * fh;
        for (float gx = x0 + 1.5f; gx < x1 - 1.5f; gx += 2.2f)
            b.window(gx, z0, fy + 0.9f, 1.7f, 1.8f, true, M_GLASS);
        for (float gz = z0 + 1.5f; gz < z1 - 1.5f; gz += 2.2f)
            b.window(x0, gz, fy + 0.9f, 1.7f, 1.8f, false, M_GLASS);
    }
    if (shops) {
        b.clear(x0 + 2.0f, y0 + 0.6f, z0 - 0.4f, x0 + 7.0f, y0 + 3.0f, z0 + 0.8f);
        b.solid(x0 + 2.0f, y0 + 0.6f, z0 - 0.12f, x0 + 7.0f, y0 + 3.0f, z0 + 0.12f, M_GLASS);
        b.counter(x0 + 3.0f, z0 + 4.0f, y0 + 0.5f + 0.95f, 3.5f);
        for (int i = 0; i < 2; i++) b.shelf(x0 + 10.0f + i * 3.0f, z0 + 3.0f, y0 + 2.4f, 2.4f, M_METAL);
        b.clear(x0 + 13.0f, y0 + 0.6f, z0 - 0.4f, x0 + 18.0f, y0 + 3.0f, z0 + 0.8f);
        b.solid(x0 + 13.0f, y0 + 0.6f, z0 - 0.12f, x0 + 18.0f, y0 + 3.0f, z0 + 0.12f, M_GLASS);
    }
    // interior office partitions + stairs
    for (int f = 0; f < floors; f++) {
        float fy = y0 + f * fh;
        b.concreteWall(x0 + 2.0f, z0 + (z1 - z0) * 0.5f, x1 - 2.0f, z0 + (z1 - z0) * 0.5f, fy + 0.5f, fh - 0.7f, 0.22f, M_PLASTER);
        b.door(x0 + (x1 - x0) * 0.35f, z0 + (z1 - z0) * 0.5f, fy, true);
        b.table(x0 + 3.0f, z0 + 2.5f, fy + 0.95f, 1.6f, 0.8f);
        b.chair(x0 + 3.0f, z0 + 2.0f, fy + 0.85f);
        b.shelf(x1 - 4.0f, z0 + 2.0f, fy + 2.4f, 1.8f, M_METAL);
    }
    b.stairs(x0 + (x1 - x0) * 0.62f, z0 + 1.2f, y0 + 0.5f, y0 + fh + 0.5f, false, 1.5f);
    for (int f = 1; f < floors; f++) {
        float fy = y0 + f * fh;
        b.clear(x0 + (x1 - x0) * 0.62f, fy + 0.2f, z0 + 1.2f, x0 + (x1 - x0) * 0.62f + 1.7f, fy + 0.65f, z0 + 4.6f);
        b.stairs(x0 + (x1 - x0) * 0.62f, z0 + 1.2f, fy + 0.5f, fy + fh + 0.5f, false, 1.5f);
        // shaft continues up
    }
    // rooftop equipment
    b.solid(x0 + 3, y1, z0 + 3, x0 + 7, y1 + 1.4f, z0 + 6, M_METAL_DARK);
    b.solid(x0 + (x1 - x0) * 0.6f, y1, z0 + 4, x0 + (x1 - x0) * 0.6f + 3, y1 + 2.0f, z0 + 7, M_WHITE_PANEL);
    b.railing(x0, z0, x1, z0, y1, M_METAL_DARK);
    b.railing(x0, z1, x1, z1, y1, M_METAL_DARK);
}

static void genDowntown(World& w, MapBuildResult& out) {
    Builder b(w, out);
    b.terrain(M_CONCRETE_DARK, M_SOIL, 2.0f, 0.02f);
    // intersection grid
    b.road(-4, 30, 88, 44);
    b.road(38, -4, 52, 88);
    b.sidewalk(-4, 28.4f, 88, 30, M_CONCRETE);
    b.sidewalk(-4, 44, 88, 45.6f, M_CONCRETE);
    b.sidewalk(36.4f, -4, 38, 88, M_CONCRETE);
    b.sidewalk(52, -4, 53.6f, 88, M_CONCRETE);
    for (int i = 0; i < 5; i++) {
        b.streetlight(8 + i * 16.0f, 28.0f, false);
        b.streetlight(16 + i * 16.0f, 46.0f, true);
    }
    // buildings
    cityBuilding(b, 4, 4, 30, 26, 6, M_CONCRETE, true);       // NW office block
    cityBuilding(b, 58, 4, 84, 26, 8, M_WHITE_PANEL, true);   // NE tower
    cityBuilding(b, 4, 50, 30, 78, 5, M_BRICK, true);         // SW apartments
    cityBuilding(b, 58, 50, 84, 78, 4, M_CONCRETE, true);     // SE low block
    out.addTarget("nw_tower", "North-west office block", AABB(vec3(4, 2, 4), vec3(30, 22, 26)));
    out.addTarget("ne_tower", "North-east tower", AABB(vec3(58, 2, 4), vec3(84, 28, 26)));
    out.addTarget("sw_block", "South-west apartments", AABB(vec3(4, 2, 50), vec3(30, 20, 78)));
    out.addTarget("se_block", "South-east low block", AABB(vec3(58, 2, 50), vec3(84, 15, 78)));
    // parking garage SE corner (open deck structure)
    {
        float x0 = 54, z0 = 56, x1 = 70, z1 = 74;
        for (int f = 0; f < 3; f++) {
            float fy = 2.0f + f * 3.2f;
            b.solid(x0, fy, z0, x1, fy + 0.4f, z1, M_CONCRETE);
            for (int i = 0; i < 4; i++) {
                float px = x0 + (i & 1) * (x1 - x0 - 0.6f);
                float pz = z0 + ((i >> 1) & 1) * (z1 - z0 - 0.6f);
                b.solid(px, fy, pz, px + 0.6f, fy + 3.2f, pz + 0.6f, M_CONCRETE);
            }
            b.railing(x0, z0, x1, z0, fy, M_CONCRETE);
        }
    }
    // construction scaffolding on the NE tower
    for (int f = 0; f < 7; f++) {
        float fy = 2.0f + 1.6f * f;
        for (int i = 0; i < 5; i++)
            b.solid(56.0f, fy, 6.0f + i * 4.0f, 56.4f, fy + 1.7f, 6.4f + i * 4.0f, M_METAL);
        b.solid(55.6f, fy + 0.8f, 6.0f, 58.0f, fy + 1.0f, 24.0f, M_PLANK);
    }
    // overpass bridge over the north road
    {
        float zc = 20.0f;
        b.solid(30, 8.0f, zc - 2.5f, 58, 8.8f, zc + 2.5f, M_CONCRETE);
        b.solid(33, 2.0f, zc - 1.5f, 35, 8.0f, zc + 1.5f, M_REINFORCED);
        b.solid(52, 2.0f, zc - 1.5f, 54, 8.0f, zc + 1.5f, M_REINFORCED);
        b.railing(30, zc - 2.5f, 58, zc - 2.5f, 8.8f, M_METAL);
        b.railing(30, zc + 2.5f, 58, zc + 2.5f, 8.8f, M_METAL);
        out.addTarget("overpass", "Overpass bridge", AABB(vec3(30, 2, zc - 2.5f), vec3(58, 9, zc + 2.5f)));
    }
    // underground service tunnel
    {
        b.clear(36, -1.0f, 4, 40, 1.6f, 28);
        b.solid(35, -1.4f, 3, 41, -1.0f, 29, M_CONCRETE_DARK);
        b.solid(35, -1.0f, 3, 36, 1.6f, 29, M_CONCRETE);
        b.solid(40, -1.0f, 3, 41, 1.6f, 29, M_CONCRETE);
        // access ramp from the street
        for (int i = 0; i < 10; i++) b.solid(38, 1.2f - i * 0.24f, 28 + i * 0.4f, 40, 1.45f - i * 0.24f, 28.4f + i * 0.4f, M_CONCRETE_DARK);
        for (int i = 0; i < 14; i++) b.solid(38, 1.35f - i * 0.24f, 32 + i * 0.4f, 40, 1.6f - i * 0.24f, 32.4f + i * 0.4f, M_CONCRETE_DARK);
    }
    out.vehicles.push_back({VK_SEDAN, vec3(20, 2.2f, 37), PI * 0.5f});
    out.vehicles.push_back({VK_TRUCK, vec3(60, 2.2f, 37), -PI * 0.5f});
    out.vehicles.push_back({VK_ARMORED, vec3(44, 2.2f, 12), 0.0f});
    out.props.push_back({0, vec3(34, 2.5f, 33)});
    out.props.push_back({0, vec3(35, 2.5f, 34)});
    out.hints.push_back("Reinforced columns carry these towers - blasting the corners drops the floors.");
    out.hints.push_back("Glass curtain walls open rooms in one shotgun blast.");
    out.hints.push_back("The service tunnel under the north road is a route past the overpass.");
}

// ---------------------------------------------------------------- map 4: Harbor Breakdown
static void genHarbor(World& w, MapBuildResult& out) {
    Builder b(w, out);
    b.terrain(M_SAND, M_SOIL, 1.2f, 0.04f);
    // basin: carve the water area
    float waterY = 0.75f;
    for (int z = V(44); z < w.sz; z++)
        for (int x = 0; x < w.sx; x++) {
            for (int y = 0; y < V(1.2f); y++) w.setRaw(x, y, z, y == 0 ? M_SOIL : M_SAND);
            for (int y = 1; y < V(waterY) + 1; y++) w.setRaw(x, y, z, M_WATER);
        }
    // quay wall
    b.solid(-2, 0.0f, 42, 86, 2.4f, 44, M_CONCRETE_DARK);
    b.solid(-2, 1.2f, 43.2f, 86, 2.0f, 44, M_RUBBER);
    // bollards
    for (int i = 0; i < 10; i++) b.solid(4 + i * 8.0f, 1.2f, 43.0f, 4.6f + i * 8.0f, 1.9f, 43.6f, M_METAL_DARK);
    // roads
    b.road(-4, 16, 88, 28);
    b.road(40, 28, 52, 44);
    // --- container yard ---
    {
        uint8_t colors[4] = {M_CONTAINER_BLUE, M_CONTAINER_RED, M_CONTAINER_GREEN, M_CONTAINER_ORANGE};
        for (int i = 0; i < 14; i++) {
            float px = 6 + (i % 7) * 7.2f;
            float pz = 4 + (i / 7) * 9.0f;
            int stack = 1 + (i % 3);
            for (int s = 0; s < stack; s++) {
                float y = 1.2f + s * 2.6f;
                b.solid(px, y, pz, px + 6.0f, y + 2.5f, pz + 3.0f, colors[(i + s) % 4]);
                b.clear(px + 0.35f, y + 0.35f, pz + 0.3f, px + 5.65f, y + 2.15f, pz + 2.7f);
            }
        }
        out.addTarget("containers", "Container stacks", AABB(vec3(6, 1, 4), vec3(56, 9, 22)));
    }
    // --- warehouse on the quay ---
    {
        float x0 = 60, z0 = 32, x1 = 86, z1 = 52;
        b.solid(x0, 2.4f, z0, x1, 9.0f, z0 + 0.6f, M_METAL);
        b.solid(x0, 2.4f, z1 - 0.6f, x1, 9.0f, z1, M_METAL);
        b.solid(x0, 2.4f, z0, x0 + 0.6f, 9.0f, z1, M_METAL);
        b.solid(x1 - 0.6f, 2.4f, z0, x1, 9.0f, z1, M_METAL);
        b.solid(x0, 8.6f, z0, x1, 9.0f, z1, M_METAL_DARK);
        b.clear(x0 + 4, 2.4f, z0 - 0.2f, x0 + 12, 6.5f, z0 + 0.8f);
        for (int i = 0; i < 3; i++) b.shelf(x0 + 4 + i * 7.0f, z0 + 6.0f, 6.6f, 3.0f, M_METAL_DARK);
        out.props.push_back({4, vec3(x0 + 22, 3.0f, z0 + 16)});
        out.props.push_back({4, vec3(x0 + 22.5f, 3.0f, z0 + 16.5f)});
        out.addTarget("harbor_warehouse", "Harbor warehouse", AABB(vec3(x0, 2.4f, z0), vec3(x1, 9, z1)));
    }
    // --- cargo ship at the quay ---
    {
        float x0 = 16, z0 = 46, x1 = 60, z1 = 62;
        // hull
        b.solid(x0, -0.5f, z0, x1, 4.0f, z1, M_METAL_DARK);
        b.clear(x0 + 2, 2.4f, z0 + 2, x1 - 2, 4.2f, z1 - 2);
        b.solid(x0 + 2, 2.2f, z0 + 2, x1 - 2, 2.4f, z1 - 2, M_METAL);
        // superstructure aft
        b.solid(x1 - 10, 4.0f, z0 + 2, x1 - 2, 11.0f, z1 - 2, M_WHITE_PANEL);
        b.clear(x1 - 9, 4.2f, z0 + 3, x1 - 3, 10.6f, z1 - 3);
        for (int i = 0; i < 3; i++)
            b.window(x1 - 10, z0 + 4 + i * 3.4f, 6.0f, 1.6f, 1.2f, false, M_GLASS);
        // containers on deck
        uint8_t cols[3] = {M_CONTAINER_RED, M_CONTAINER_ORANGE, M_CONTAINER_BLUE};
        for (int i = 0; i < 8; i++) {
            float px = x0 + 3 + (i % 4) * 6.4f;
            float pz = z0 + 3 + (i / 4) * 6.0f;
            b.solid(px, 4.2f, pz, px + 6.0f, 6.6f, pz + 2.8f, cols[i % 3]);
        }
        // bridge crane on deck
        b.solid(x0 + 6, 6.6f, z0 + 2, x0 + 7.2f, 15.0f, z0 + 3.2f, M_PAINT_YELLOW);
        b.solid(x0 + 24, 6.6f, z0 + 2, x0 + 25.2f, 15.0f, z0 + 3.2f, M_PAINT_YELLOW);
        b.solid(x0 + 5, 14.0f, z0 + 1.5f, x0 + 26, 15.4f, z0 + 3.5f, M_PAINT_YELLOW);
        out.addTarget("cargo_ship", "Cargo ship", AABB(vec3(x0, -0.5f, z0), vec3(x1, 15.4f, z1)));
    }
    // --- gantry cranes on the quay ---
    for (int c = 0; c < 2; c++) {
        float cx = 22 + c * 26.0f;
        b.solid(cx - 0.8f, 2.0f, 38.0f, cx + 0.8f, 26.0f, 39.6f, M_PAINT_BLUE);
        b.solid(cx + 10.0f, 2.0f, 38.0f, cx + 11.6f, 26.0f, 39.6f, M_PAINT_BLUE);
        b.solid(cx - 1, 25.0f, 36.0f, cx + 12.6f, 27.0f, 41.0f, M_PAINT_BLUE);
        b.solid(cx + 3, 18.0f, 37.0f, cx + 6, 25.0f, 40.0f, M_METAL_DARK);
        b.pipe(cx + 4.5f, 25.0f, 39.0f, cx + 4.5f, 14.0f, 46.0f, 0.1f, M_METAL_DARK);
    }
    out.addTarget("harbor_cranes", "Gantry cranes", AABB(vec3(20, 2, 36), vec3(60, 27, 41)));
    // --- fuel depot ---
    {
        float cx = 74, cz = 8;
        for (int i = 0; i < 2; i++) {
            float px = cx + i * 9.0f;
            b.solid(px - 3.0f, 1.2f, cz - 3.0f, px + 3.0f, 9.0f, cz + 3.0f, M_WHITE_PANEL);
            b.clear(px - 2.6f, 1.4f, cz - 2.6f, px + 2.6f, 8.6f, cz + 2.6f);
            b.solid(px - 3.2f, 8.6f, cz - 3.2f, px + 3.2f, 9.2f, cz + 3.2f, M_METAL_DARK);
            out.props.push_back({3, vec3(px + 4.6f, 2.2f, cz + 4.0f)});
            out.props.push_back({4, vec3(px - 4.2f, 2.0f, cz + 4.2f)});
        }
        out.addTarget("fuel_depot", "Fuel depot", AABB(vec3(cx - 4, 1.2f, cz - 4), vec3(cx + 14, 10, cz + 5)));
    }
    // boats
    b.solid(14, 0.55f, 68, 22, 1.4f, 72, M_WHITE_PANEL);
    b.solid(15, 1.4f, 69, 21, 2.0f, 71, M_CONTAINER_BLUE);
    out.vehicles.push_back({VK_BOAT, vec3(30, 1.0f, 54), PI * 0.5f});
    out.vehicles.push_back({VK_BOAT, vec3(70, 1.0f, 56), -PI * 0.5f});
    out.vehicles.push_back({VK_TRUCK, vec3(20, 2.2f, 22), 0.0f});
    out.vehicles.push_back({VK_FORKLIFT, vec3(58, 2.2f, 34), PI * 0.5f});
    out.vehicles.push_back({VK_BULLDOZER, vec3(46, 2.2f, 22), 0.0f});
    out.hints.push_back("Containers are hollow steel boxes - cut a corner and the stack tumbles.");
    out.hints.push_back("Cut the gantry crane legs to drop it across the quay.");
    out.hints.push_back("The fuel depot will take the whole east quay with it.");
}

// ---------------------------------------------------------------- map 5: Red Canyon Test Range
static void genCanyon(World& w, MapBuildResult& out) {
    Builder b(w, out);
    // canyon: high rock walls, sandy floor, winding
    for (int z = 0; z < w.sz; z++)
        for (int x = 0; x < w.sx; x++) {
            // long-wavelength mesas so the range reads as rock shelves, not noise
            float n = fbm3(vec3(x * 0.005f, z * 0.005f, 3.3f), 3, 21);
            float ridge = 1.0f - std::fabs(fbm3(vec3(x * 0.0025f, z * 0.0025f, 9.1f), 2, 41) - 0.5f) * 2.0f;
            float h = 2.0f + n * 16.0f * (0.30f + ridge * 0.70f);
            // carve the central canyon path
            float cx = 40 + std::sin(z * 0.045f) * 13.0f;
            float d = std::fabs(x - cx);
            if (d < 16.0f) h = std::min(h, 2.0f + d * d * 0.035f);
            int ht = V(h);
            for (int y = 0; y <= ht; y++) {
                // horizontal strata: alternating sandstone and red rock bands
                int band = (y / 5) % 3;
                uint8_t m = band == 0 ? M_SANDSTONE : (band == 1 ? M_ROCK_RED : M_STONE);
                if (y == ht) m = (h < 3.6f) ? M_SAND : m;
                w.setRaw(x, y, z, m);
            }
        }
    w.groundVox = V(2.0f);
    // flattened test pad
    b.clear(20, 2.0f, 40, 66, 4.0f, 74);
    b.solid(20, 1.75f, 40, 66, 2.0f, 74, M_CONCRETE);
    // bunker
    {
        float x0 = 24, z0 = 44, x1 = 40, z1 = 58;
        b.solid(x0, 2.0f, z0, x1, 6.5f, z1, M_REINFORCED);
        b.clear(x0 + 1.0f, 2.1f, z0 + 1.0f, x1 - 1.0f, 6.0f, z1 - 1.0f);
        b.clear(x0 + 3, 2.1f, z1 - 1.2f, x0 + 5, 4.2f, z1 + 0.2f);          // entrance
        // firing slit
        b.clear(x0 + 8, 4.2f, z0 - 0.2f, x0 + 14, 5.2f, z0 + 0.6f);
        // interior
        b.table(x0 + 3, z0 + 4, 3.0f, 2.2f, 1.0f, M_METAL_DARK);
        b.shelf(x0 + 10, z0 + 2, 4.6f, 2.0f, M_METAL_DARK);
        b.solid(x0 - 0.3f, 6.5f, z0 - 0.3f, x1 + 0.3f, 6.9f, z1 + 0.3f, M_CONCRETE);
        out.addTarget("bunker", "Reinforced bunker", AABB(vec3(x0, 2, z0), vec3(x1, 7, z1)));
    }
    // watchtowers
    for (int t = 0; t < 2; t++) {
        float cx = 26 + t * 34.0f, cz = 66.0f;
        for (int i = 0; i < 4; i++) {
            float px = cx + ((i & 1) ? 2.2f : -2.2f);
            float pz = cz + ((i >> 1) ? 2.2f : -2.2f);
            b.solid(px - 0.2f, 2.0f, pz - 0.2f, px + 0.2f, 12.0f, pz + 0.2f, M_WOOD_DARK);
        }
        b.solid(cx - 3.0f, 11.6f, cz - 3.0f, cx + 3.0f, 12.0f, cz + 3.0f, M_PLANK);
        b.solid(cx - 3.0f, 12.0f, cz - 3.0f, cx + 3.0f, 14.6f, cz - 2.6f, M_PLANK);
        b.solid(cx - 3.0f, 12.0f, cz + 2.6f, cx + 3.0f, 14.6f, cz + 3.0f, M_PLANK);
        b.solid(cx - 3.0f, 14.4f, cz - 3.0f, cx + 3.0f, 14.8f, cz + 3.0f, M_METAL_DARK);
        b.stairs(cx + 3.2f, cz - 1.0f, 2.0f, 11.6f, false, 1.2f, M_PLANK);
        out.addTarget("tower" + std::to_string(t), "Watchtower", AABB(vec3(cx - 3, 2, cz - 3), vec3(cx + 3, 15, cz + 3)));
    }
    // target wall structures
    for (int i = 0; i < 3; i++) {
        float px = 46 + i * 8.0f;
        b.solid(px, 2.0f, 30, px + 6.0f, 5.0f, 30.8f, i == 0 ? M_CONCRETE : (i == 1 ? M_BRICK : M_REINFORCED));
        out.addTarget("targetwall" + std::to_string(i), "Target wall", AABB(vec3(px, 2, 30), vec3(px + 6, 5, 30.8f)));
    }
    // bridge over a ravine
    {
        b.clear(60, -2.0f, 56, 78, 2.2f, 70);
        b.solid(58, 2.0f, 56, 80, 2.6f, 58.5f, M_CONCRETE);
        b.solid(58, 2.0f, 67.5f, 80, 2.6f, 70, M_CONCRETE);
        b.solid(60, 2.0f, 58.5f, 78, 2.6f, 67.5f, M_CONCRETE);
        b.solid(63, 0.0f, 60.0f, 65, 2.0f, 62, M_REINFORCED);
        b.solid(73, 0.0f, 60.0f, 75, 2.0f, 62, M_REINFORCED);
        out.addTarget("canyon_bridge", "Canyon bridge", AABB(vec3(60, 0, 58.5f), vec3(78, 2.6f, 67.5f)));
    }
    // underground tunnel from the bunker north
    {
        b.clear(30, -1.6f, 20, 33, 1.4f, 44);
        b.solid(29, -2.0f, 19, 34, -1.6f, 45, M_CONCRETE_DARK);
        b.solid(28.5f, -1.6f, 19, 29.5f, 1.4f, 45, M_STONE);
        b.solid(33.5f, -1.6f, 19, 34.5f, 1.4f, 45, M_STONE);
        // ramp up into the bunker's back
        for (int i = 0; i < 12; i++) b.solid(30, -1.4f + i * 0.3f, 38 + i * 0.5f, 33, -1.15f + i * 0.3f, 38.5f + i * 0.5f, M_CONCRETE_DARK);
    }
    // explosive crates
    for (int i = 0; i < 4; i++) out.props.push_back({4, vec3(48.0f + i * 1.2f, 2.6f, 52.0f)});
    out.props.push_back({3, vec3(44, 2.6f, 46)});
    out.vehicles.push_back({VK_ARMORED, vec3(34, 2.2f, 66), 0.0f});
    out.vehicles.push_back({VK_TRUCK, vec3(44, 2.2f, 44), PI * 0.5f});
    out.vehicles.push_back({VK_EXCAVATOR, vec3(56, 2.2f, 62), 0.0f});
    out.hints.push_back("Sandstone is soft - the drill goes through fast. Reinforced bunker walls do not.");
    out.hints.push_back("The bridge is held by two reinforced piers. Cut both and it drops.");
    out.hints.push_back("Airstrikes are authorised anywhere in the range.");
}

// ---------------------------------------------------------------- map 6: Grand Mall
static void genMall(World& w, MapBuildResult& out) {
    Builder b(w, out);
    b.terrain(M_ASPHALT, M_SOIL, 2.0f, 0.01f);
    // parking lot markings
    for (int z = V(6); z < V(26); z++)
        for (int x = V(6); x < V(78); x++)
            if ((x % 12) < 1) w.setRaw(x, V(2.0f), z, M_WHITE_PANEL);
    // --- mall shell: 3 floors, large atrium ---
    float x0 = 14, z0 = 30, x1 = 76, z1 = 72;
    float y0 = 2.0f, fh = 4.2f;
    int floors = 3;
    float y1 = y0 + floors * fh;
    // outer walls
    b.solid(x0, y0, z0, x1, y1, z0 + 0.6f, M_WHITE_PANEL);
    b.solid(x0, y0, z1 - 0.6f, x1, y1, z1, M_WHITE_PANEL);
    b.solid(x0, y0, z0, x0 + 0.6f, y1, z1, M_WHITE_PANEL);
    b.solid(x1 - 0.6f, y0, z0, x1, y1, z1, M_WHITE_PANEL);
    // roof
    b.solid(x0 - 0.3f, y1 - 0.4f, z0 - 0.3f, x1 + 0.3f, y1, z1 + 0.3f, M_METAL_DARK);
    // atrium skylight
    b.solid(x0 + 18, y1 - 0.5f, z0 + 12, x0 + 40, y1 - 0.2f, z0 + 30, M_SKYLIGHT);
    // main entrance
    b.clear(x0 + 20, y0 + 0.2f, z0 - 0.4f, x0 + 32, y0 + 3.6f, z0 + 0.8f);
    b.solid(x0 + 20, y0 + 0.2f, z0 - 0.15f, x0 + 32, y0 + 3.6f, z0 + 0.15f, M_GLASS);
    // floor slabs with atrium hole
    for (int f = 1; f < floors; f++) {
        float fy = y0 + f * fh;
        // ring around the atrium (atrium = centre)
        for (int z = V(z0); z < V(z1); z++)
            for (int x = V(x0); x < V(x1); x++) {
                bool atrium = (x > V(x0 + 18) && x < V(x0 + 42) && z > V(z0 + 12) && z < V(z0 + 34));
                if (atrium) continue;
                for (int y = V(fy); y < V(fy + 0.35f); y++) w.setRaw(x, y, z, M_TILE);
            }
        // railings around atrium
        b.railing(x0 + 18, z0 + 12, x0 + 42, z0 + 12, fy + 0.35f, M_METAL);
        b.railing(x0 + 18, z0 + 34, x0 + 42, z0 + 34, fy + 0.35f, M_METAL);
        b.railing(x0 + 18, z0 + 12, x0 + 18, z0 + 34, fy + 0.35f, M_METAL);
        b.railing(x0 + 42, z0 + 12, x0 + 42, z0 + 34, fy + 0.35f, M_METAL);
    }
    // support columns in the atrium
    for (int cx = 0; cx < 3; cx++)
        for (int cz = 0; cz < 2; cz++)
            b.solid(x0 + 20 + cx * 10.0f, y0, z0 + 14 + cz * 18.0f, x0 + 21.2f + cx * 10.0f, y1, z0 + 15.2f + cz * 18.0f, M_REINFORCED);
    // shops around the perimeter, each floor
    for (int f = 0; f < floors; f++) {
        float fy = y0 + f * fh;
        for (int s = 0; s < 6; s++) {
            float sx0 = x0 + 1.0f + (s % 3) * 20.0f;
            float sz0 = (s < 3) ? z0 + 1.0f : z1 - 9.0f;
            // shop front glass
            if (s < 3) b.window(sx0 + 8.0f, sz0 + 7.5f, fy + 0.3f, 6.0f, 3.2f, true, M_GLASS);
            else b.window(sx0 + 8.0f, sz0 - 0.5f, fy + 0.3f, 6.0f, 3.2f, true, M_GLASS);
            b.counter(sx0 + 1.0f, sz0 + 1.0f, fy + 0.95f, 3.0f);
            b.shelf(sx0 + 4.5f, sz0 + 1.0f, fy + 2.4f, 2.5f, M_METAL);
            b.shelf(sx0 + 12.0f, sz0 + 1.0f, fy + 2.4f, 2.5f, M_METAL);
            b.table(sx0 + 8.0f, sz0 + 3.5f, fy + 0.95f, 1.6f, 1.0f);
        }
    }
    // escalators: 4 stepped ramps crossing the atrium
    for (int f = 0; f < 2; f++) {
        float fy = y0 + f * fh;
        for (int i = 0; i < 28; i++) {
            float yy = fy + 0.35f + i * (fh / 28.0f);
            b.solid(x0 + 22.0f + i * 0.7f, yy, z0 + 20.0f, x0 + 22.9f + i * 0.7f, yy + 0.25f, z0 + 22.4f, M_METAL);
        }
        b.railing(x0 + 22.0f, z0 + 20.0f, x0 + 22.0f + 20.0f, z0 + 20.0f, fy + 1.0f, M_METAL);
    }
    // cinema block on the top floor
    {
        float cx0 = x0 + 46, cz0 = z0 + 2, cx1 = x1 - 1, cz1 = z0 + 14;
        float fy = y0 + 2 * fh;
        b.solid(cx0, fy, cz0, cx1, fy + 0.4f, cz1, M_CONCRETE);
        b.solid(cx0, fy + 0.4f, cz0, cx1, fy + 3.6f, cz0 + 0.4f, M_CONCRETE);
        b.door(cx0 + 6, cz0, fy + 0.4f, true);
        b.solid(cx0, fy + 3.6f, cz0, cx1, fy + 3.9f, cz1, M_CONCRETE);
    }
    // service corridor behind the mall
    {
        b.clear(x0 + 2, y0 + 0.2f, z0 + 40, x0 + 3.0f, y0 + 2.6f, z1 - 2);
        for (int i = 0; i < 5; i++) b.door(x0 + 2.5f, z0 + 44 + i * 5.0f, y0, false);
    }
    // rooftop HVAC
    for (int i = 0; i < 6; i++) {
        float px = x0 + 6 + (i % 3) * 20.0f;
        float pz = z0 + 6 + (i / 3) * 20.0f;
        b.solid(px, y1, pz, px + 2.4f, y1 + 1.6f, pz + 2.4f, M_METAL);
        b.solid(px + 0.4f, y1 + 1.6f, pz + 0.4f, px + 1.0f, y1 + 2.2f, pz + 1.0f, M_METAL_DARK);
    }
    out.addTarget("mall_atrium", "Mall atrium", AABB(vec3(x0 + 18, y0, z0 + 12), vec3(x0 + 42, y1, z0 + 34)));
    out.addTarget("mall_front", "Mall frontage", AABB(vec3(x0, y0, z0), vec3(x1, y1, z0 + 2)));
    out.addTarget("cinema", "Cinema block", AABB(vec3(x0 + 46, y0 + 2 * fh, z0), vec3(x1, y0 + 2 * fh + 4, z0 + 14)));
    out.props.push_back({0, vec3(24, 2.5f, 28)});
    out.props.push_back({0, vec3(25, 2.5f, 28.5f)});
    out.props.push_back({2, vec3(20, 2.5f, 26)});
    out.vehicles.push_back({VK_SEDAN, vec3(30, 2.2f, 12), PI * 0.5f});
    out.vehicles.push_back({VK_PICKUP, vec3(46, 2.2f, 12), PI * 0.5f});
    out.vehicles.push_back({VK_FORKLIFT, vec3(16, 2.2f, 26), 0.0f});
    out.hints.push_back("The atrium columns are the only thing holding the upper floors.");
    out.hints.push_back("Escalators make good ramps once the railings are gone.");
    out.hints.push_back("Roof HVAC units are heavy - dropping one punches through the floor below.");
}

// ---------------------------------------------------------------- registry
static GameMap g_maps[6] = {
    {0, {}, "WESTHAVEN SUBURBS", "Residential demolition district",
     genWesthaven},
    {1, {}, "BLACKSTONE INDUSTRIAL", "Chain-reaction territory", genBlackstone},
    {2, {}, "DOWNTOWN COLLAPSE", "Vertical destruction", genDowntown},
    {3, {}, "HARBOR BREAKDOWN", "Docks, ships, fuel", genHarbor},
    {4, {}, "RED CANYON TEST RANGE", "Military demolition ground", genCanyon},
    {5, {}, "THE GRAND MALL", "Interior demolition", genMall},
};

static bool g_mapsInit = false;

static void initMapInfos() {
    if (g_mapsInit) return;
    g_mapsInit = true;
    struct Def {
        int sx, sy, sz;
        vec3 spawn;
        float yaw;
        vec3 sunDir, sunCol;
        vec3 skyTop, skyHor;
        vec3 ambSky, ambGround, fogCol;
        float fogDensity;
        int hour;
        Weather w;
    };
    Def defs[6];
    defs[0] = {320, 72, 320, vec3(40, 3.2f, 40), 0.0f, vec3(-0.45f, 0.72f, 0.52f), vec3(1.0f, 0.95f, 0.86f),
               vec3(0.20f, 0.40f, 0.68f), vec3(0.70f, 0.79f, 0.86f), vec3(0.44f, 0.53f, 0.66f), vec3(0.28f, 0.25f, 0.20f),
               vec3(0.68f, 0.75f, 0.82f), 0.0055f, 15, {0, 0.22f, 0, 0, 2.0f, 0.0055f}};
    defs[1] = {336, 96, 336, vec3(44, 3.2f, 46), 0.6f, vec3(-0.36f, 0.68f, 0.64f), vec3(1.0f, 0.92f, 0.80f),
               vec3(0.24f, 0.36f, 0.52f), vec3(0.62f, 0.66f, 0.70f), vec3(0.40f, 0.44f, 0.50f), vec3(0.24f, 0.21f, 0.18f),
               vec3(0.58f, 0.61f, 0.64f), 0.009f, 10, {1, 0.55f, 0, 0.1f, 3.5f, 0.009f}};
    defs[2] = {336, 160, 336, vec3(45, 3.2f, 36), -0.7f, vec3(0.42f, 0.66f, -0.62f), vec3(1.0f, 0.90f, 0.78f),
               vec3(0.19f, 0.32f, 0.56f), vec3(0.68f, 0.67f, 0.70f), vec3(0.42f, 0.46f, 0.55f), vec3(0.26f, 0.23f, 0.20f),
               vec3(0.66f, 0.66f, 0.70f), 0.0075f, 8, {1, 0.7f, 0, 0.15f, 4.0f, 0.0075f}};
    defs[3] = {320, 80, 320, vec3(30, 3.2f, 30), 0.3f, vec3(0.35f, 0.74f, 0.58f), vec3(1.0f, 0.96f, 0.90f),
               vec3(0.17f, 0.36f, 0.62f), vec3(0.72f, 0.80f, 0.86f), vec3(0.44f, 0.54f, 0.66f), vec3(0.25f, 0.27f, 0.28f),
               vec3(0.70f, 0.77f, 0.82f), 0.005f, 13, {0, 0.3f, 0.0f, 0.05f, 3.0f, 0.005f}};
    defs[4] = {352, 112, 352, vec3(44, 3.2f, 60), 0.0f, vec3(0.55f, 0.70f, 0.45f), vec3(1.05f, 0.86f, 0.66f),
               vec3(0.28f, 0.42f, 0.62f), vec3(0.86f, 0.74f, 0.58f), vec3(0.52f, 0.50f, 0.46f), vec3(0.34f, 0.27f, 0.20f),
               vec3(0.80f, 0.72f, 0.60f), 0.0045f, 16, {0, 0.2f, 0.0f, 0.0f, 5.0f, 0.0045f}};
    defs[5] = {288, 96, 288, vec3(45, 3.2f, 24), 0.0f, vec3(0.30f, 0.78f, 0.55f), vec3(1.0f, 0.98f, 0.94f),
               vec3(0.22f, 0.44f, 0.70f), vec3(0.74f, 0.82f, 0.88f), vec3(0.46f, 0.55f, 0.68f), vec3(0.30f, 0.28f, 0.24f),
               vec3(0.70f, 0.78f, 0.84f), 0.0045f, 12, {0, 0.28f, 0.0f, 0.0f, 2.0f, 0.0045f}};
    for (int i = 0; i < 6; i++) {
        MapInfo& mi = g_maps[i].info;
        mi.id = i;
        mi.name = g_maps[i].shortName;
        mi.subtitle = g_maps[i].tagline;
        mi.sx = defs[i].sx;
        mi.sy = defs[i].sy;
        mi.sz = defs[i].sz;
        mi.spawn = defs[i].spawn;
        mi.spawnYaw = defs[i].yaw;
        mi.sunDir = defs[i].sunDir.norm();
        mi.sunColor = defs[i].sunCol;
        mi.skyTop = defs[i].skyTop;
        mi.skyHorizon = defs[i].skyHor;
        mi.ambientSky = defs[i].ambSky;
        mi.ambientGround = defs[i].ambGround;
        mi.fogColor = defs[i].fogCol;
        mi.fogDensity = defs[i].fogDensity;
        mi.weather = defs[i].w;
        mi.defaultHour = defs[i].hour;
        mi.waterLevel = (i == 3) ? 0.75f : -1000.0f;
    }
    g_maps[0].info.description = "A quiet residential district: houses with furnished interiors, yards, a convenience store, a gas station and a construction site. The perfect place to learn that nothing is load-bearing forever.";
    g_maps[1].info.description = "A sprawling industrial complex of warehouses, factories, fuel tanks and cranes. Everything here is connected to something explosive.";
    g_maps[2].info.description = "A dense downtown block of offices and apartments over a service tunnel. Destroy the wrong column and six floors arrive at once.";
    g_maps[3].info.description = "A working harbor: container stacks, gantry cranes, a cargo ship at the quay and a fuel depot waiting for a spark.";
    g_maps[4].info.description = "A military demolition range in a red rock canyon. Reinforced bunkers, watchtowers, a bridge over a ravine, and unlimited authorisation.";
    g_maps[5].info.description = "A three-storey shopping mall with an open atrium, escalators, glass storefronts and a cinema. Interior demolition at its finest.";
}

int mapCount() { return 6; }
const GameMap& getMap(int id) {
    initMapInfos();
    return g_maps[iclamp(id, 0, 5)];
}
int defaultTimeOfDay(int mapId) {
    initMapInfos();
    return g_maps[iclamp(mapId, 0, 5)].info.defaultHour;
}

void buildMap(int id, World& outWorld, MapBuildResult& out) {
    initMapInfos();
    const GameMap& m = getMap(id);
    outWorld.init(m.info.sx, m.info.sy, m.info.sz);
    out.targets.clear();
    out.props.clear();
    out.vehicles.clear();
    out.hints.clear();
    m.generate(outWorld, out);
    // ---- world rim: raise the border terrain into hills so the play area reads as
    // real ground instead of a floating slab, and the player cannot walk off the edge
    {
        const int rimW = 22;   // voxels (5.5 m) of rising ground at each border
        for (int z = 0; z < outWorld.sz; z++)
            for (int x = 0; x < outWorld.sx; x++) {
                int edge = std::min(std::min(x, outWorld.sx - 1 - x), std::min(z, outWorld.sz - 1 - z));
                if (edge >= rimW) continue;
                float t = 1.0f - (float)edge / (float)rimW;          // 1 at the border
                // existing surface material + height at this column
                int topY = -1;
                uint8_t topMat = M_SOIL;
                for (int y = outWorld.sy - 1; y >= 0; y--) {
                    uint8_t v = outWorld.at(x, y, z);
                    if (matSolid(v)) { topY = y; topMat = v; break; }
                }
                if (topY < 0) continue;
                float n = fbm3(vec3(x * 0.06f, z * 0.06f, 7.0f), 2, 33);
                int rise = (int)(std::pow(t, 1.35f) * (3.5f + n * 6.0f) / VOX);
                int h = std::min(outWorld.sy - 1, topY + rise);
                for (int y = topY; y <= h; y++)
                    outWorld.setRaw(x, y, z, y == h ? topMat : (uint8_t)M_STONE);
            }
    }
    // compute baseline voxel counts for the destruction targets
    for (auto& t : out.targets) t.baseline = t.current(outWorld);
    outWorld.modifiedChunks.clear();
    outWorld.damage.clear();
    outWorld.fresh.clear();
    outWorld.markAllDirty();
}
