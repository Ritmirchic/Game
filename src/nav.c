// Navigation grid: build from level colliders, flow-field pathfinding.
#include "world.h"
#include <string.h>

#define NAV_CELL 0.5f

void nav_build(NavGrid *nav, const Level *lv) {
    memset(nav, 0, sizeof *nav);
    nav->cell = NAV_CELL;
    nav->origin = v2(-48.f, -36.f);
    nav->w = (int)(96.f / NAV_CELL);
    nav->h = (int)(92.f / NAV_CELL);
    size_t n = (size_t)nav->w * nav->h;
    nav->walk = (uint8_t *)calloc(n, 1);
    nav->cost = (float *)calloc(n, sizeof(float));
    nav->room = (uint8_t *)calloc(n, 1);
    nav->flow = (Vec2 *)calloc(n, sizeof(Vec2));
    nav->dist = (float *)calloc(n, sizeof(float));
    nav->visited = (uint32_t *)calloc(n, sizeof(uint32_t));
    for (int y = 0; y < nav->h; y++)
        for (int x = 0; x < nav->w; x++) {
            float wx = nav->origin.x + ((float)x + 0.5f) * NAV_CELL;
            float wz = nav->origin.y + ((float)y + 0.5f) * NAV_CELL;
            int blocked = 0;
            if (wx < -46.f || wx > 46.f || wz < -29.f || wz > 57.f) blocked = 1;
            for (int i = 0; i < lv->aabb_count && !blocked; i++) {
                const AABB *a = &lv->aabbs[i];
                if (!a->blocks_move) continue;
                if (a->ymax <= 0.30f) continue;
                if (wx > a->mn.x - 0.28f && wx < a->mx.x + 0.28f && wz > a->mn.y - 0.28f && wz < a->mx.y + 0.28f)
                    blocked = 1;
            }
            nav->walk[(size_t)y * nav->w + x] = blocked ? 0 : 1;
        }
    // doors / entry points must stay navigable
    for (int i = 0; i < lv->slot_count; i++) {
        const BarricadeSlot *s = &lv->slots[i];
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                int cx = (int)((s->pos.x + (float)dx * 0.45f - nav->origin.x) / NAV_CELL);
                int cy = (int)((s->pos.z + (float)dy * 0.45f - nav->origin.y) / NAV_CELL);
                if (cx < 0 || cy < 0 || cx >= nav->w || cy >= nav->h) continue;
                nav->walk[(size_t)cy * nav->w + cx] = 1;
            }
    }
    LOGI("nav: %dx%d cells (%d walkable)", nav->w, nav->h, 0);
}
void nav_free(NavGrid *nav) {
    free(nav->walk); free(nav->cost); free(nav->room); free(nav->flow); free(nav->dist); free(nav->visited);
    memset(nav, 0, sizeof *nav);
}

void nav_update_flow(NavGrid *nav, Vec3 target) {
    if (!nav->walk) return;
    size_t n = (size_t)nav->w * nav->h;
    for (size_t i = 0; i < n; i++) {
        nav->dist[i] = 1e30f;
        nav->flow[i] = v2(0, 0);
    }
    int tx = (int)((target.x - nav->origin.x) / NAV_CELL);
    int ty = (int)((target.z - nav->origin.y) / NAV_CELL);
    tx = clampi(tx, 0, nav->w - 1);
    ty = clampi(ty, 0, nav->h - 1);
    // if the target cell is blocked, spiral outwards to find a walkable one
    if (!nav->walk[(size_t)ty * nav->w + tx]) {
        int best = -1, bestd = 1 << 30;
        for (int r = 1; r < 14 && best < 0; r++) {
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    int cx = tx + dx, cy = ty + dy;
                    if (cx < 0 || cy < 0 || cx >= nav->w || cy >= nav->h) continue;
                    if (!nav->walk[(size_t)cy * nav->w + cx]) continue;
                    int d = dx * dx + dy * dy;
                    if (d < bestd) { bestd = d; best = cy * nav->w + cx; }
                }
        }
        if (best < 0) return;
        tx = best % nav->w;
        ty = best / nav->w;
    }
    // Dijkstra with a simple binary heap
    typedef struct { float d; int idx; } Node;
    static Node heap[70000];
    int hn = 0;
    size_t start = (size_t)ty * nav->w + tx;
    nav->dist[start] = 0.f;
    heap[hn].d = 0.f; heap[hn].idx = (int)start; hn++;
    nav->visited[start] = ++nav->target_cx;   // stamp
    static const int ox[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int oz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (hn > 0) {
        // pop min
        Node top = heap[0];
        heap[0] = heap[--hn];
        int i = 0;
        while (1) {
            int l = i * 2 + 1, r = l + 1, small = i;
            if (l < hn && heap[l].d < heap[small].d) small = l;
            if (r < hn && heap[r].d < heap[small].d) small = r;
            if (small == i) break;
            Node tmp = heap[i]; heap[i] = heap[small]; heap[small] = tmp;
            i = small;
        }
        if (top.d > nav->dist[top.idx]) continue;
        int cx = top.idx % nav->w, cy = top.idx / nav->w;
        for (int k = 0; k < 8; k++) {
            int nx = cx + ox[k], nz = cy + oz[k];
            if (nx < 0 || nz < 0 || nx >= nav->w || nz >= nav->h) continue;
            size_t ni = (size_t)nz * nav->w + nx;
            if (!nav->walk[ni]) continue;
            float step = (k >= 4) ? 1.41421f : 1.f;
            step += nav->cost[ni] * 6.f;
            float nd = top.d + step;
            if (nd < nav->dist[ni]) {
                nav->dist[ni] = nd;
                // flow points away from the target (towards the neighbour with lower dist)
                nav->flow[ni] = v2((float)(cx - nx), (float)(cy - nz));
                if (hn < 69000) {
                    heap[hn].d = nd; heap[hn].idx = (int)ni;
                    hn++;
                    int j = hn - 1;
                    while (j > 0) {
                        int p = (j - 1) / 2;
                        if (heap[p].d <= heap[j].d) break;
                        Node tmp = heap[p]; heap[p] = heap[j]; heap[j] = tmp;
                        j = p;
                    }
                }
            }
        }
    }
    // normalise flow vectors (bilinear-ish sampling done by nav_flow_dir)
    for (size_t i = 0; i < n; i++) {
        float l = v2len(nav->flow[i]);
        if (l > 1e-5f) nav->flow[i] = v2(nav->flow[i].x / l, nav->flow[i].y / l);
    }
}

int nav_walkable(const NavGrid *nav, Vec3 p) {
    if (!nav->walk) return 0;
    int cx = (int)((p.x - nav->origin.x) / nav->cell);
    int cy = (int)((p.z - nav->origin.y) / nav->cell);
    if (cx < 0 || cy < 0 || cx >= nav->w || cy >= nav->h) return 0;
    return nav->walk[(size_t)cy * nav->w + cx];
}

Vec2 nav_flow_dir(const NavGrid *nav, Vec3 p) {
    if (!nav->walk) return v2(0, 0);
    int cx = (int)((p.x - nav->origin.x) / nav->cell);
    int cy = (int)((p.z - nav->origin.y) / nav->cell);
    if (cx < 0 || cy < 0 || cx >= nav->w || cy >= nav->h) return v2(0, 0);
    if (!nav->walk[(size_t)cy * nav->w + cx]) {
        // find a nearby walkable cell and use its flow
        for (int r = 1; r <= 3; r++) {
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= nav->w || ny >= nav->h) continue;
                    if (!nav->walk[(size_t)ny * nav->w + nx]) continue;
                    return nav->flow[(size_t)ny * nav->w + nx];
                }
        }
        return v2(0, 0);
    }
    Vec2 f = nav->flow[(size_t)cy * nav->w + cx];
    if (v2len(f) < 0.01f) {
        // look at neighbours for the best direction (handles ridge cells)
        static const int ox[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static const int oz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        float bestd = nav->dist[(size_t)cy * nav->w + cx];
        int bx = 0, by = 0;
        for (int k = 0; k < 8; k++) {
            int nx = cx + ox[k], nz = cy + oz[k];
            if (nx < 0 || nz < 0 || nx >= nav->w || nz >= nav->h) continue;
            size_t ni = (size_t)nz * nav->w + nx;
            if (!nav->walk[ni]) continue;
            if (nav->dist[ni] < bestd) { bestd = nav->dist[ni]; bx = ox[k]; by = oz[k]; }
        }
        if (bx || by) {
            float l = sqrtf((float)(bx * bx + by * by));
            return v2((float)bx / l, (float)by / l);
        }
    }
    return f;
}

// bullet raycast with surface normal
int level_raycast_bullet(const Level *lv, Vec3 from, Vec3 to, float *hit_t, Vec3 *hit_nrm) {
    int idx = -1;
    Vec3 d = vsub(to, from);
    float dist = vlen(d);
    if (dist < 1e-5f) return 0;
    Vec3 dir = vscale(d, 1.f / dist);
    float best = dist;
    for (int i = 0; i < lv->aabb_count; i++) {
        const AABB *a = &lv->aabbs[i];
        if (!a->blocks_bullet) continue;
        float tmin = 0.f, tmax = best;
        const float mins[3] = {a->mn.x, a->ymin, a->mn.y};
        const float maxs[3] = {a->mx.x, a->ymax, a->mx.y};
        const float o[3] = {from.x, from.y, from.z};
        const float dd[3] = {dir.x, dir.y, dir.z};
        int miss = 0;
        for (int ax = 0; ax < 3; ax++) {
            if (fabsf(dd[ax]) < 1e-6f) {
                if (o[ax] < mins[ax] || o[ax] > maxs[ax]) { miss = 1; break; }
            } else {
                float inv = 1.f / dd[ax];
                float t0 = (mins[ax] - o[ax]) * inv;
                float t1 = (maxs[ax] - o[ax]) * inv;
                if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }
                if (t0 > tmin) tmin = t0;
                if (t1 < tmax) tmax = t1;
                if (tmin > tmax) { miss = 1; break; }
            }
        }
        if (miss) continue;
        if (tmin < best && tmin > 0.001f) { best = tmin; idx = i; }
    }
    if (idx < 0) return 0;
    if (hit_t) *hit_t = best;
    if (hit_nrm) {
        Vec3 p = vmad(from, dir, best);
        const AABB *a = &lv->aabbs[idx];
        Vec3 n = v3(0, 1, 0);
        float dx0 = fabsf(p.x - a->mn.x), dx1 = fabsf(p.x - a->mx.x);
        float dz0 = fabsf(p.z - a->mn.y), dz1 = fabsf(p.z - a->mx.y);
        float dy0 = fabsf(p.y - a->ymin), dy1 = fabsf(p.y - a->ymax);
        float m = minf(minf(dx0, dx1), minf(minf(dz0, dz1), minf(dy0, dy1)));
        if (m == dx0) n = v3(-1, 0, 0);
        else if (m == dx1) n = v3(1, 0, 0);
        else if (m == dz0) n = v3(0, 0, -1);
        else if (m == dz1) n = v3(0, 0, 1);
        else if (m == dy0) n = v3(0, -1, 0);
        else n = v3(0, 1, 0);
        *hit_nrm = n;
    }
    return 1;
}

// Simple ray-vs-AABB test that reports the nearest blocking hit distance.
int level_raycast(const Level *lv, Vec3 from, Vec3 to, float *hit_t, Vec3 *hit_pos) {
    float t = 0.f;
    Vec3 nrm;
    Vec3 d = vsub(to, from);
    float dist = vlen(d);
    if (dist < 1e-5f) return 0;
    // clip the query to the requested segment length
    if (!level_raycast_bullet(lv, from, to, &t, &nrm)) return 0;
    if (t > dist) return 0;
    if (hit_t) *hit_t = t;
    if (hit_pos) *hit_pos = vmad(from, vscale(d, 1.f / dist), t);
    return 1;
}
