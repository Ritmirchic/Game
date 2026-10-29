// FRACTURE — shared procedural material shading.
// The same functions are mirrored in the GLSL terrain shader (see render_gl.cpp) so that the
// CPU reference renderer and the GPU renderer produce matching looks.
#pragma once
#include "core/math.h"

struct ShadeDetail {
    vec3 color;
    float rough;
    vec3 nrm;
    float alpha;
    float emissive;
};

static inline vec3 srgbToLin(vec3 c) {
    return {std::pow(c.x, 2.2f), std::pow(c.y, 2.2f), std::pow(c.z, 2.2f)};
}
static inline float patternNoise(vec3 p, uint32_t seed) { return valueNoise3(p, seed); }

// Fine per-pixel material detail. wp = world position (metres), n = axis-aligned normal,
// base = material albedo (linear), ao = 0..1 ambient occlusion.
static inline void shadeVoxelDetail(uint8_t mat, const vec3& wp, const vec3& n, const vec3& base, float ao,
                                    ShadeDetail& o) {
    o.color = base;
    o.rough = 0.85f;
    o.nrm = n;
    o.alpha = 1;
    o.emissive = 0;
    switch (mat) {
        case M_BRICK: {
            // 0.42 x 0.21 m bricks, 0.015 m mortar, staggered courses
            float bh = 0.21f, bw = 0.42f;
            float row = std::floor(wp.y / bh);
            float off = std::fmod(row, 2.0f) * 0.5f * bw;
            float u = (std::fabs(n.x) > 0.5f ? wp.z : wp.x) + off;
            float fx = std::fmod(u, bw), fy = std::fmod(wp.y, bh);
            float mortar = std::min(fx, bw - fx) < 0.012f || std::min(fy, bh - fy) < 0.012f ? 1.0f : 0.0f;
            float tex = patternNoise(wp * 18.0f, 11) * 0.5f + patternNoise(wp * 4.0f, 12) * 0.5f;
            vec3 col = base * (0.86f + tex * 0.30f);
            o.color = vlerp(col, vec3(0.55f, 0.53f, 0.50f), mortar * 0.75f);
            o.rough = 0.9f;
            o.nrm = (n + (mortar > 0.5f ? n * -0.15f : vec3(0))).norm();
            break;
        }
        case M_CONCRETE:
        case M_CONCRETE_DARK:
        case M_PAINT_BLUE:
        case M_PAINT_RED:
        case M_WHITE_PANEL: {
            float big = patternNoise(wp * 0.55f, 21);
            float fine = patternNoise(wp * 9.0f, 22);
            float grime = smoothstepf(0.35f, 0.75f, big * 0.7f + fine * 0.3f);
            vec3 col = base * (0.92f + fine * 0.10f);
            col = vlerp(col, base * 0.62f, grime * 0.5f);
            // smoke streaks below ledges
            float streak = patternNoise(vec3(wp.x * 2.4f, wp.y * 0.35f, wp.z * 2.4f), 23);
            col = col * (0.9f + 0.16f * streak);
            o.color = col;
            o.rough = 0.93f;
            break;
        }
        case M_PLASTER: {
            float fine = patternNoise(wp * 12.0f, 31);
            o.color = base * (0.95f + fine * 0.09f);
            o.rough = 0.96f;
            break;
        }
        case M_WOOD:
        case M_WOOD_DARK:
        case M_PLANK:
        case M_TRUNK: {
            // grain runs along the longer horizontal axis
            bool alongX = std::fabs(n.x) < 0.5f;
            float g1 = patternNoise(vec3(alongX ? wp.x * 1.6f : wp.z * 1.6f, wp.y * 22.0f, alongX ? wp.z * 22.0f : wp.x * 22.0f), 41);
            float g2 = patternNoise(wp * 6.0f, 42);
            float grain = 0.5f + 0.5f * std::sin((alongX ? wp.x : wp.z) * 26.0f + g1 * 5.0f);
            float boards = std::fmod(n.y > 0.5f ? (alongX ? wp.x : wp.z) : wp.y, 0.32f);
            float seam = boards < 0.012f ? 1.0f : 0.0f;
            vec3 col = base * (0.86f + grain * 0.16f + g2 * 0.08f);
            col = vlerp(col, base * 0.55f, seam * 0.8f);
            o.color = col;
            o.rough = 0.78f;
            break;
        }
        case M_METAL:
        case M_METAL_DARK:
        case M_RUST:
        case M_CONTAINER_BLUE:
        case M_CONTAINER_RED:
        case M_CONTAINER_GREEN:
        case M_CONTAINER_ORANGE: {
            float panel = 1.0f;
            if (mat >= M_CONTAINER_BLUE) {
                float rib = std::fmod(std::fabs(n.x) > 0.5f ? wp.z : wp.x, 0.30f);
                panel = rib < 0.075f ? 0.72f : 1.0f;
            } else {
                float px = std::fabs(n.x) > 0.5f ? wp.z : wp.x;
                float py = wp.y;
                float seamX = std::fmod(px, 1.2f) < 0.02f ? 1.0f : 0.0f;
                float seamY = std::fmod(py, 0.9f) < 0.02f ? 1.0f : 0.0f;
                panel = (seamX + seamY) > 0.5f ? 0.55f : 1.0f;
                // rivets
                float rx = std::fmod(px, 0.2f) - 0.1f;
                float ry = std::fmod(py, 0.2f) - 0.1f;
                if (rx * rx + ry * ry < 0.00012f) panel *= 1.35f;
            }
            float scratch = patternNoise(vec3(wp.x * 3.0f, wp.y * 26.0f, wp.z * 3.0f), 51);
            vec3 col = base * panel * (0.9f + scratch * 0.18f);
            if (mat == M_RUST) {
                float rust = smoothstepf(0.30f, 0.70f, patternNoise(wp * 2.2f, 52) * 0.7f + patternNoise(wp * 10.0f, 53) * 0.3f);
                col = vlerp(col, vec3(0.34f, 0.15f, 0.06f), rust * 0.85f);
            }
            o.color = col;
            o.rough = mat == M_RUST ? 0.95f : 0.38f + scratch * 0.2f;
            // bevel the panel seams a little in the normal
            if (panel < 0.99f) o.nrm = (n + n * -0.18f).norm();
            break;
        }
        case M_GRATE: {
            float gx = std::fmod(wp.x, 0.12f), gz = std::fmod(wp.z, 0.12f);
            bool hole = gx < 0.07f && gz < 0.07f;
            o.color = hole ? base * 0.35f : base * 1.1f;
            o.rough = 0.6f;
            break;
        }
        case M_GLASS:
        case M_SKYLIGHT: {
            o.color = base * (0.95f + patternNoise(wp * 3.0f, 61) * 0.1f);
            o.alpha = 0.42f;
            o.rough = 0.06f;
            break;
        }
        case M_ASPHALT: {
            float speck = patternNoise(wp * 34.0f, 71);
            float patch = patternNoise(wp * 0.8f, 72);
            vec3 col = base * (0.85f + speck * 0.35f);
            col = vlerp(col, base * 1.5f, smoothstepf(0.62f, 0.75f, patch) * 0.4f);
            // faded road markings
            if (std::fmod(std::fabs(wp.z), 3.0f) < 0.09f && wp.x > 0 && wp.y < 4.3f) col = vlerp(col, vec3(0.62f, 0.58f, 0.35f), 0.55f);
            o.color = col;
            o.rough = 0.94f;
            break;
        }
        case M_TILE:
        case M_COBBLE: {
            float ts = mat == M_TILE ? 0.32f : 0.22f;
            float fx = std::fmod(std::fabs(n.y) > 0.5f ? wp.x : wp.y, ts);
            float fz = std::fmod(std::fabs(n.y) > 0.5f ? wp.z : (std::fabs(n.x) > 0.5f ? wp.z : wp.x), ts);
            float grout = (fx < 0.012f || fz < 0.012f) ? 1.0f : 0.0f;
            float var = patternNoise(wp * 22.0f, 81);
            o.color = vlerp(base * (0.9f + var * 0.12f), base * 0.5f, grout * 0.8f);
            o.rough = mat == M_TILE ? 0.55f : 0.92f;
            break;
        }
        case M_ROOF: {
            float row = std::floor(wp.y / 0.16f);
            float off = std::fmod(row, 2.0f) * 0.11f;
            float u = (std::fabs(n.x) > 0.5f ? wp.z : wp.x) + off;
            float shingle = std::fmod(u, 0.22f) < 0.015f || std::fmod(wp.y, 0.16f) < 0.012f ? 0.55f : 1.0f;
            o.color = base * shingle * (0.88f + patternNoise(wp * 14.0f, 91) * 0.2f);
            o.rough = 0.85f;
            break;
        }
        case M_SOIL:
        case M_GRAVEL:
        case M_SAND:
        case M_MUD: {
            float clump = patternNoise(wp * 7.0f, 101) * 0.6f + patternNoise(wp * 27.0f, 102) * 0.4f;
            vec3 col = base * (0.80f + clump * 0.42f);
            o.color = col;
            o.rough = 0.98f;
            break;
        }
        case M_FOLIAGE: {
            float leaves = patternNoise(wp * 9.0f, 111) * 0.5f + patternNoise(wp * 26.0f, 112) * 0.5f;
            vec3 col = vlerp(base * 0.55f, base * 1.5f, leaves);
            o.color = col;
            o.rough = 0.85f;
            o.alpha = 0.92f;
            break;
        }
        case M_STONE: {
            float rocky = patternNoise(wp * 5.0f, 121) * 0.6f + patternNoise(wp * 17.0f, 122) * 0.4f;
            float strata = std::sin(wp.y * 5.0f + rocky * 4.0f) * 0.5f + 0.5f;
            o.color = base * (0.78f + rocky * 0.30f + strata * 0.10f);
            o.rough = 0.9f;
            break;
        }
        case M_LAMP: {
            o.color = base;
            o.emissive = 1.0f;
            o.rough = 0.25f;
            break;
        }
        case M_WATER: {
            o.color = base * 0.8f;
            o.alpha = 0.62f;
            o.rough = 0.04f;
            break;
        }
        case M_RUBBER: {
            o.color = base * (0.92f + patternNoise(wp * 20.0f, 131) * 0.14f);
            o.rough = 0.95f;
            break;
        }
        default: {
            o.color = base * (0.93f + patternNoise(wp * 11.0f, 141) * 0.14f);
            break;
        }
    }
    // dirt accumulation towards the bottom of surfaces + wet darkening handled by caller
    float wet = 0.0f;
    (void)wet;
}
