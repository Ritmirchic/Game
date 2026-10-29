// FRACTURE — OpenGL 3.3 renderer (primary). Voxel chunk meshes with baked AO, shadow mapping,
// procedural materials driven by the shared material table, instanced particles/boxes, water,
// FBO-based post processing and the bitmap-font UI layer.
#include "core/engine.h"
#include "core/png.h"

#ifdef _WIN32
#include "render/gl_loader.h"
#include <deque>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>

// ------------------------------------------------------------------ gl loader
static GLFuncs g_gl;
GLFuncs& gl() { return g_gl; }

static void* glGet(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        static HMODULE mod = LoadLibraryA("opengl32.dll");
        if (mod) p = (void*)GetProcAddress(mod, name);
    }
    return p;
}

bool glLoad() {
    bool ok = true;
#define L(f) g_gl.f = (PFN_gl##f)glGet("gl" #f); if (!g_gl.f) ok = false;
    L(GenBuffers) L(BindBuffer) L(BufferData) L(BufferSubData) L(DeleteBuffers)
    L(GenVertexArrays) L(BindVertexArray) L(DeleteVertexArrays) L(EnableVertexAttribArray)
    L(VertexAttribPointer) L(VertexAttribIPointer) L(VertexAttribDivisor)
    L(CreateShader) L(ShaderSource) L(CompileShader) L(GetShaderiv) L(GetShaderInfoLog) L(DeleteShader)
    L(CreateProgram) L(AttachShader) L(LinkProgram) L(GetProgramiv) L(GetProgramInfoLog) L(UseProgram)
    L(DeleteProgram) L(GetUniformLocation) L(Uniform1i) L(Uniform1f) L(Uniform2f) L(Uniform3f)
    L(Uniform3fv) L(Uniform4f) L(Uniform1fv) L(Uniform4fv) L(Uniform2fv) L(UniformMatrix4fv) L(ActiveTexture) L(GenerateMipmap)
    L(GenFramebuffers) L(BindFramebuffer) L(FramebufferTexture2D) L(CheckFramebufferStatus)
    L(DeleteFramebuffers) L(GenRenderbuffers) L(BindRenderbuffer) L(RenderbufferStorage)
    L(FramebufferRenderbuffer) L(DeleteRenderbuffers) L(DrawArraysInstanced) L(DrawElementsInstanced)
    L(BlitFramebuffer)
#undef L
    g_gl.ok = ok;
    return ok;
}

// ------------------------------------------------------------------ shaders
static const char* VS_WORLD = R"GLSL(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in uvec4 aData;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat4 uLightVP;
uniform float uTime;
uniform int uWaterPass;
out vec3 vWorld;
out vec3 vNormal;
out vec4 vLightPos;
out float vAO;
flat out uint vMat;
flat out uint vFlags;
out float vWave;
const vec3 NORMALS[6] = vec3[6](vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));
void main() {
    vec3 wp = (uModel * vec4(aPos, 1.0)).xyz;
    uint mat = aData.y;
    uint flags = aData.w;
    float wave = 0.0;
    if ((flags & 2u) != 0u) {
        // water surface: gentle swell
        wave = sin(wp.x * 3.1 + uTime * 1.7) * 0.035 + sin(wp.z * 4.3 + uTime * 1.13) * 0.028
             + sin((wp.x + wp.z) * 1.6 + uTime * 0.7) * 0.04;
        wp.y += wave;
    }
    vWorld = wp;
    vWave = wave;
    vNormal = NORMALS[aData.x];
    vAO = float(aData.z) / 255.0;
    vMat = mat;
    vFlags = flags;
    vLightPos = uLightVP * vec4(wp, 1.0);
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)GLSL";

static const char* FS_WORLD = R"GLSL(#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec4 vLightPos;
in float vAO;
flat in uint vMat;
flat in uint vFlags;
in float vWave;

uniform vec3 uCamPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform vec3 uAmbientSky;
uniform vec3 uAmbientGround;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uExposure;
uniform float uWetness;
uniform float uTime;
uniform float uCloudCover;
uniform int uWaterPass;
uniform sampler2D uShadowMap;
uniform float uShadowTexel;
uniform float uShadowStrength;
uniform vec3 uMatColor[40];
uniform vec3 uMatInterior[40];
uniform vec4 uMatProps[40];      // x=rough y=metal z=pattern w=transparent
uniform vec3 uFlashlightDir;
uniform vec3 uFlashlightPos;
uniform float uFlashlightOn;
uniform vec3 uExplosionLightPos[4];
uniform vec3 uExplosionLightColor[4];
uniform float uExplosionLightRadius[4];

out vec4 outColor;

uint PATTERN() { return uint(uMatProps[vMat].z); }

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float vnoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash21(i.xy + i.z * 31.7);
    float n100 = hash21(i.xy + vec2(1, 0) + i.z * 31.7);
    float n010 = hash21(i.xy + vec2(0, 1) + i.z * 31.7);
    float n110 = hash21(i.xy + vec2(1, 1) + i.z * 31.7);
    float n001 = hash21(i.xy + vec2(0, 0) + (i.z + 1.0) * 31.7);
    float n101 = hash21(i.xy + vec2(1, 0) + (i.z + 1.0) * 31.7);
    float n011 = hash21(i.xy + vec2(0, 1) + (i.z + 1.0) * 31.7);
    float n111 = hash21(i.xy + vec2(1, 1) + (i.z + 1.0) * 31.7);
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

float shadowFactor(vec3 wp, vec3 n) {
    if (uShadowStrength < 0.01) return 1.0;
    vec3 p = vLightPos.xyz / vLightPos.w;
    p = p * 0.5 + 0.5;
    if (p.x < 0.002 || p.x > 0.998 || p.y < 0.002 || p.y > 0.998 || p.z > 1.0) return 1.0;
    float bias = max(0.0009, 0.0035 * (1.0 - max(0.0, dot(n, uSunDir))));
    float lit = 0.0;
    vec2 ts = vec2(uShadowTexel);
    float spread = 1.4;
    lit += step(p.z - bias, texture(uShadowMap, p.xy + vec2(-0.5, -0.5) * ts * spread).r);
    lit += step(p.z - bias, texture(uShadowMap, p.xy + vec2( 0.5, -0.5) * ts * spread).r);
    lit += step(p.z - bias, texture(uShadowMap, p.xy + vec2(-0.5,  0.5) * ts * spread).r);
    lit += step(p.z - bias, texture(uShadowMap, p.xy + vec2( 0.5,  0.5) * ts * spread).r);
    float s = lit * 0.25;
    return mix(1.0, s, uShadowStrength);
}

void main() {
    uint pattern = PATTERN();
    vec3 base = uMatColor[vMat];
    if ((vFlags & 1u) != 0u) base = uMatInterior[vMat];
    vec3 n = normalize(vNormal);
    float rough = uMatProps[vMat].x;
    float metal = uMatProps[vMat].y;
    float alpha = 1.0;
    float emissive = 0.0;
    vec3 wp = vWorld;
    float ao = vAO;

    // ---- procedural material detail (mirrors render/shading.h on the CPU) ----
    if (pattern == 1u) {           // brick
        float bh = 0.21, bw = 0.42;
        float row = floor(wp.y / bh);
        float off = mod(row, 2.0) * 0.5 * bw;
        float u = (abs(n.x) > 0.5 ? wp.z : wp.x) + off;
        float fx = mod(u, bw), fy = mod(wp.y, bh);
        float mortar = (min(fx, bw - fx) < 0.013 || min(fy, bh - fy) < 0.013) ? 1.0 : 0.0;
        float tex = vnoise(wp * 18.0) * 0.5 + vnoise(wp * 4.0) * 0.5;
        base *= 0.86 + tex * 0.30;
        base = mix(base, vec3(0.55, 0.53, 0.50), mortar * 0.72);
        rough = 0.92;
    } else if (pattern == 2u) {    // concrete / plaster / paint
        float big = vnoise(wp * 0.55);
        float fine = vnoise(wp * 9.0);
        float grime = smoothstep(0.35, 0.78, big * 0.7 + fine * 0.3);
        base *= 0.92 + fine * 0.10;
        base = mix(base, base * 0.60, grime * 0.5);
        float streak = vnoise(vec3(wp.x * 2.4, wp.y * 0.30, wp.z * 2.4));
        base *= 0.90 + 0.17 * streak;
        rough = 0.93;
    } else if (pattern == 3u) {    // wood grain
        bool alongX = abs(n.x) < 0.5;
        float g1 = vnoise(vec3(alongX ? wp.x * 1.6 : wp.z * 1.6, wp.y * 22.0, alongX ? wp.z * 22.0 : wp.x * 22.0));
        float g2 = vnoise(wp * 6.0);
        float grain = 0.5 + 0.5 * sin((alongX ? wp.x : wp.z) * 26.0 + g1 * 5.0);
        base *= 0.86 + grain * 0.16 + g2 * 0.08;
        rough = 0.78;
    } else if (pattern == 4u) {    // plank
        bool alongX = abs(n.x) < 0.5;
        float seam = mod(n.y > 0.5 ? (alongX ? wp.x : wp.z) : wp.y, 0.32);
        base *= 0.90 + vnoise(wp * 12.0) * 0.14;
        if (seam < 0.014) base *= 0.62;
        rough = 0.80;
    } else if (pattern == 5u) {    // metal / container
        float panel = 1.0;
        if (vMat >= 30u && vMat <= 33u) {
            float rib = mod(abs(n.x) > 0.5 ? wp.z : wp.x, 0.30);
            panel = rib < 0.075 ? 0.72 : 1.0;
        } else {
            float px = abs(n.x) > 0.5 ? wp.z : wp.x;
            float seamX = mod(px, 1.2) < 0.02 ? 1.0 : 0.0;
            float seamY = mod(wp.y, 0.9) < 0.02 ? 1.0 : 0.0;
            panel = (seamX + seamY) > 0.5 ? 0.55 : 1.0;
            float rx = mod(px, 0.2) - 0.1;
            float ry = mod(wp.y, 0.2) - 0.1;
            if (rx * rx + ry * ry < 0.00012) panel *= 1.35;
        }
        float scratch = vnoise(vec3(wp.x * 3.0, wp.y * 26.0, wp.z * 3.0));
        base *= panel * (0.9 + scratch * 0.18);
        if (vMat == 12u) {   // rust
            float rust = smoothstep(0.30, 0.72, vnoise(wp * 2.2) * 0.7 + vnoise(wp * 10.0) * 0.3);
            base = mix(base, vec3(0.34, 0.15, 0.06), rust * 0.85);
        }
        rough = (vMat == 12u) ? 0.95 : 0.38 + scratch * 0.2;
    } else if (pattern == 6u) {    // grating
        float gx = mod(wp.x, 0.12), gz = mod(wp.z, 0.12);
        bool hole = gx < 0.07 && gz < 0.07;
        base = hole ? base * 0.35 : base * 1.1;
    } else if (pattern == 7u) {    // asphalt
        float speck = vnoise(wp * 34.0);
        float patch = vnoise(wp * 0.8);
        base *= 0.85 + speck * 0.35;
        base = mix(base, base * 1.5, smoothstep(0.62, 0.78, patch) * 0.4);
        if (mod(abs(wp.z), 3.0) < 0.09 && wp.y < 4.3) base = mix(base, vec3(0.62, 0.58, 0.35), 0.5);
        rough = 0.94;
    } else if (pattern == 8u) {    // tiles / cobble
        float ts = (vMat == 19u) ? 0.32 : 0.22;
        float fx = mod(abs(n.y) > 0.5 ? wp.x : wp.y, ts);
        float fz = mod(abs(n.y) > 0.5 ? wp.z : (abs(n.x) > 0.5 ? wp.z : wp.x), ts);
        float grout = (fx < 0.013 || fz < 0.013) ? 1.0 : 0.0;
        float var = vnoise(wp * 22.0);
        base = mix(base * (0.9 + var * 0.12), base * 0.5, grout * 0.8);
    } else if (pattern == 9u) {    // soil / gravel / sand
        float clump = vnoise(wp * 7.0) * 0.6 + vnoise(wp * 27.0) * 0.4;
        base *= 0.80 + clump * 0.42;
        rough = 0.98;
    } else if (pattern == 10u) {   // roof shingles
        float row = floor(wp.y / 0.16);
        float off = mod(row, 2.0) * 0.11;
        float u = (abs(n.x) > 0.5 ? wp.z : wp.x) + off;
        float sh = (mod(u, 0.22) < 0.015 || mod(wp.y, 0.16) < 0.012) ? 0.55 : 1.0;
        base *= sh * (0.88 + vnoise(wp * 14.0) * 0.2);
    } else if (pattern == 13u) {   // rock / stone
        float rocky = vnoise(wp * 5.0) * 0.6 + vnoise(wp * 17.0) * 0.4;
        float strata = sin(wp.y * 5.0 + rocky * 4.0) * 0.5 + 0.5;
        base *= 0.78 + rocky * 0.30 + strata * 0.10;
    } else if (pattern == 14u) {   // glass
        base *= 0.95 + vnoise(wp * 3.0) * 0.1;
        alpha = 0.42;
        rough = 0.06;
        float fres = pow(1.0 - max(0.0, dot(n, normalize(uCamPos - wp))), 3.0);
        base += vec3(0.10, 0.14, 0.18) * fres;
    } else {
        base *= 0.93 + vnoise(wp * 11.0) * 0.14;
    }
    if (vMat == 22u) {             // foliage: broken up alpha
        float leaves = vnoise(wp * 9.0) * 0.5 + vnoise(wp * 26.0) * 0.5;
        base = mix(base * 0.55, base * 1.5, leaves);
        alpha = 0.94;
    }
    if (vMat == 28u) { emissive = 2.4; }   // lamp
    if ((vFlags & 2u) != 0u) {             // water
        base = uMatColor[vMat] * 0.85;
        alpha = 0.66;
        rough = 0.05;
        // fresnel + wave normal
        vec3 V = normalize(uCamPos - wp);
        float fres = pow(1.0 - max(0.0, dot(vec3(0, 1, 0), V)), 2.5);
        base = mix(base, vec3(0.42, 0.55, 0.62), fres * 0.75);
        n = normalize(vec3(cos(wp.x * 3.1 + uTime * 1.7) * 0.18, 1.0, cos(wp.z * 4.3 + uTime * 1.13) * 0.18));
    }

    // ---- lighting ----
    float shadow = shadowFactor(wp, n);
    float ndl = max(0.0, dot(n, uSunDir));
    float sunVis = uSunIntensity * (1.0 - uCloudCover * 0.80);
    float sky = 0.5 + 0.5 * n.y;
    float aof = 0.34 + 0.66 * ao;
    vec3 amb = uAmbientSky * sky * aof + uAmbientGround * (1.0 - sky) * aof * 0.5;
    vec3 sunLight = uSunColor * sunVis * 1.55;
    vec3 lighting = amb + sunLight * ndl * shadow * (0.35 + 0.65 * aof);
    // fill light from the opposite direction so interiors stay readable
    float fill = max(0.0, -dot(n, uSunDir)) * 0.14;
    lighting += uAmbientSky * fill * aof;
    // flashlight
    if (uFlashlightOn > 0.5) {
        vec3 toP = wp - uFlashlightPos;
        float d = length(toP);
        vec3 dir = toP / max(0.001, d);
        float cone = smoothstep(0.86, 0.985, dot(dir, uFlashlightDir));
        float atten = 1.0 / (1.0 + d * d * 0.05);
        lighting += vec3(1.0, 0.94, 0.82) * cone * atten * 2.6 * max(0.25, dot(n, -dir));
    }
    // explosion flash lights
    for (int i = 0; i < 4; i++) {
        if (uExplosionLightRadius[i] <= 0.0) continue;
        vec3 toP = wp - uExplosionLightPos[i];
        float d = length(toP);
        float atten = clamp(1.0 - d / uExplosionLightRadius[i], 0.0, 1.0);
        lighting += uExplosionLightColor[i] * atten * atten * max(0.15, dot(n, normalize(-toP)));
    }
    // specular
    vec3 V = normalize(uCamPos - wp);
    vec3 H = normalize(uSunDir + V);
    float shin = clamp(2.0 / max(0.04, rough) * 16.0, 4.0, 3000.0);
    float spec = pow(max(0.0, dot(n, H)), shin);
    lighting += sunLight * spec * (0.15 + 0.85 * metal) * (0.3 + 0.7 * (1.0 - rough)) * shadow;
    if (uWetness > 0.01) {
        float w = uWetness * (n.y > 0.5 ? 1.0 : 0.5);
        lighting += sunLight * pow(max(0.0, dot(n, H)), 120.0) * w * 1.2;
        lighting *= (1.0 - w * 0.20);
    }
    vec3 col = base * lighting + base * emissive;
    // fog
    float dist = length(wp - uCamPos);
    float fog = 1.0 - exp(-dist * uFogDensity);
    col = mix(col, uFogColor, clamp(fog, 0.0, 0.97));
    // exposure + ACES-ish tonemap
    col *= uExposure;
    col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
    outColor = vec4(col, alpha);
}
)GLSL";

static const char* VS_SKY = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    vUV = p;
    gl_Position = vec4(p, 0.9999, 1.0);
}
)GLSL";

static const char* FS_SKY = R"GLSL(#version 330 core
in vec2 vUV;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform vec3 uSkyTop;
uniform vec3 uSkyHorizon;
uniform vec3 uFogColor;
uniform float uCloudCover;
uniform float uTime;
uniform float uExposure;
uniform vec3 uAmbientSky;
out vec4 outColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float vnoise2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x),
               mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}
void main() {
    vec4 ndc = vec4(vUV, 1.0, 1.0);
    vec4 wp = uInvViewProj * ndc;
    vec3 d = normalize(wp.xyz / wp.w - uCamPos);
    float h = clamp(d.y * 1.7 + 0.16, 0.0, 1.0);
    vec3 sky = mix(uSkyHorizon, uSkyTop, smoothstep(0.0, 1.0, h));
    sky = mix(sky, uFogColor, (1.0 - clamp(abs(d.y) * 4.0, 0.0, 1.0)) * 0.55);
    float sd = max(0.0, dot(d, uSunDir));
    float si = uSunIntensity * (1.0 - uCloudCover * 0.85);
    sky += uSunColor * (pow(sd, 24.0) * 0.28 + pow(sd, 1400.0) * 14.0) * si;
    if (uCloudCover > 0.03 && d.y > 0.015) {
        vec2 cp = d.xz / max(0.06, d.y);
        float n = vnoise2(cp * 0.42 + vec2(uTime * 0.02, uTime * 0.01)) * 0.62 +
                  0.38 * vnoise2(cp * 1.3 + vec2(0.0, uTime * 0.03));
        float cover = smoothstep(0.62 - uCloudCover * 0.4, 0.86, n);
        vec3 cLow = vec3(0.30, 0.32, 0.36), cHigh = vec3(1.05, 1.02, 0.97);
        vec3 cloudCol = mix(cLow, cHigh, clamp(uSunColor.y * 0.85, 0.0, 1.0));
        sky = mix(sky, cloudCol, cover * uCloudCover * smoothstep(0.0, 0.18, d.y));
    }
    sky += uAmbientSky * 0.02;
    sky *= uExposure;
    sky = (sky * (2.51 * sky + 0.03)) / (sky * (2.43 * sky + 0.59) + 0.14);
    outColor = vec4(sky, 1.0);
}
)GLSL";

static const char* VS_PART = R"GLSL(#version 330 core
layout(location=0) in vec3 iPos;
layout(location=1) in float iSize;
layout(location=2) in uvec4 iColor;
layout(location=3) in uvec4 iInfo;    // x=kind y=lifeRatio z=seed
uniform mat4 uViewProj;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
out vec2 vUV;
out vec4 vColor;
out float vKind;
out float vLife;
out float vSeed;
void main() {
    const vec2 CORNERS[6] = vec2[6](vec2(-1,-1), vec2(1,-1), vec2(1,1), vec2(-1,-1), vec2(1,1), vec2(-1,1));
    vec2 c = CORNERS[gl_VertexID % 6];
    float size = iSize;
    float kind = float(iInfo.x);
    if (kind > 0.5 && kind < 1.5) size *= 1.0 + (1.0 - float(iInfo.y)) * 1.6;   // smoke grows
    vec3 wp = iPos + (uCamRight * c.x + uCamUp * c.y) * size;
    vUV = c;
    vColor = vec4(float(iColor.x) / 255.0, float(iColor.y) / 255.0, float(iColor.z) / 255.0, float(iColor.w) / 255.0);
    vKind = kind;
    vLife = float(iInfo.y) / 255.0;
    vSeed = float(iInfo.z) / 255.0;
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)GLSL";

static const char* FS_PART = R"GLSL(#version 330 core
in vec2 vUV;
in vec4 vColor;
in float vKind;
in float vLife;
in float vSeed;
uniform vec3 uAmbientSky;
uniform vec3 uSunColor;
uniform float uExposure;
uniform float uCloudCover;
out vec4 outColor;
void main() {
    float r = length(vUV);
    if (r > 1.0) discard;
    float alpha = 1.0;
    vec3 col = vColor.rgb;
    if (vKind < 0.5 || (vKind > 0.5 && vKind < 1.5) || vKind > 6.5) {
        alpha = smoothstep(1.0, 0.15, r) * vColor.a * vLife;
        col *= uAmbientSky * 1.05 + uSunColor * (1.0 - uCloudCover * 0.8) * 0.7;
    } else if (vKind < 2.5) {          // spark
        alpha = smoothstep(1.0, 0.0, r) * vColor.a * vLife;
        col *= 2.6;
        col += vec3(1.0, 0.6, 0.25) * smoothstep(0.7, 0.0, r) * 1.5;
    } else if (vKind < 3.5) {          // fire
        alpha = smoothstep(1.0, 0.1, r) * vColor.a * min(1.0, vLife * 1.6);
        col *= 2.2;
    } else {
        alpha = smoothstep(1.0, 0.35, r) * vColor.a * vLife;
    }
    if (alpha < 0.004) discard;
    col *= uExposure;
    col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
    outColor = vec4(col, alpha);
}
)GLSL";

static const char* VS_BOX = R"GLSL(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 iM0;
layout(location=3) in vec4 iM1;
layout(location=4) in vec4 iM2;
layout(location=5) in vec4 iM3;
layout(location=6) in vec4 iColor;     // rgb + rough
layout(location=7) in vec4 iEmis;      // rgb + metal
uniform mat4 uViewProj;
uniform mat4 uLightVP;
out vec3 vWorld;
out vec3 vNormal;
out vec4 vLightPos;
out vec3 vColor;
out vec3 vEmis;
out float vRough;
out float vMetal;
void main() {
    mat4 M = mat4(iM0, iM1, iM2, iM3);
    vec3 wp = (M * vec4(aPos, 1.0)).xyz;
    vWorld = wp;
    vNormal = normalize(mat3(M) * aNormal);
    vColor = iColor.rgb;
    vRough = iColor.w;
    vEmis = iEmis.rgb;
    vMetal = iEmis.w;
    vLightPos = uLightVP * vec4(wp, 1.0);
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)GLSL";

static const char* FS_BOX = R"GLSL(#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec4 vLightPos;
in vec3 vColor;
in vec3 vEmis;
in float vRough;
in float vMetal;
uniform vec3 uCamPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform vec3 uAmbientSky;
uniform vec3 uAmbientGround;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uExposure;
uniform float uCloudCover;
uniform sampler2D uShadowMap;
uniform float uShadowTexel;
uniform float uShadowStrength;
uniform vec3 uFlashlightDir;
uniform vec3 uFlashlightPos;
uniform float uFlashlightOn;
out vec4 outColor;
void main() {
    vec3 n = normalize(vNormal);
    float sunVis = uSunIntensity * (1.0 - uCloudCover * 0.8);
    float sky = 0.5 + 0.5 * n.y;
    vec3 lighting = uAmbientSky * sky * 0.9 + uAmbientGround * (1.0 - sky) * 0.45;
    float shadow = 1.0;
    if (uShadowStrength > 0.01) {
        vec3 p = vLightPos.xyz / vLightPos.w * 0.5 + 0.5;
        if (p.x > 0.003 && p.x < 0.997 && p.y > 0.003 && p.y < 0.997 && p.z < 1.0) {
            float bias = max(0.0012, 0.004 * (1.0 - max(0.0, dot(n, uSunDir))));
            float s = 0.0;
            s += step(p.z - bias, texture(uShadowMap, p.xy + vec2(-0.7, -0.7) * uShadowTexel).r);
            s += step(p.z - bias, texture(uShadowMap, p.xy + vec2( 0.7, -0.7) * uShadowTexel).r);
            s += step(p.z - bias, texture(uShadowMap, p.xy + vec2(-0.7,  0.7) * uShadowTexel).r);
            s += step(p.z - bias, texture(uShadowMap, p.xy + vec2( 0.7,  0.7) * uShadowTexel).r);
            shadow = mix(1.0, s * 0.25, uShadowStrength);
        }
    }
    vec3 sunLight = uSunColor * sunVis * 1.5;
    vec3 col = vColor * (lighting + sunLight * max(0.0, dot(n, uSunDir)) * shadow);
    vec3 V = normalize(uCamPos - vWorld);
    vec3 H = normalize(uSunDir + V);
    col += sunLight * pow(max(0.0, dot(n, H)), 60.0) * (0.2 + 0.8 * vMetal) * (1.0 - vRough) * shadow;
    if (uFlashlightOn > 0.5) {
        vec3 toP = vWorld - uFlashlightPos;
        float d = length(toP);
        vec3 dir = toP / max(0.001, d);
        float cone = smoothstep(0.86, 0.985, dot(dir, uFlashlightDir));
        col += vec3(1.0, 0.94, 0.82) * cone * (1.0 / (1.0 + d * d * 0.05)) * 2.4 * max(0.25, dot(n, -dir));
    }
    col += vEmis;
    float dist = length(vWorld - uCamPos);
    col = mix(col, uFogColor, clamp(1.0 - exp(-dist * uFogDensity), 0.0, 0.97));
    col *= uExposure;
    col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
    outColor = vec4(col, 1.0);
}
)GLSL";

static const char* VS_UI = R"GLSL(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV = aUV;
    // vertex colour arrives as BGRA bytes (UI packs 0xAARRGGBB)
    vColor = vec4(aColor.bgr, aColor.a);
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

static const char* FS_UI = R"GLSL(#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uFont;
out vec4 outColor;
void main() {
    float a = texture(uFont, vUV).r;
    outColor = vec4(vColor.rgb, vColor.a * a);
}
)GLSL";

static const char* VS_POST = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    vUV = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

static const char* FS_POST = R"GLSL(#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform vec2 uTexel;
uniform int uAA;
uniform float uVignette;
uniform float uTime;
uniform float uDamage;
out vec4 outColor;
vec3 fetch(vec2 uv) { return texture(uScene, uv).rgb; }
void main() {
    vec3 c = fetch(vUV);
    if (uAA == 1) {
        // cheap FXAA-ish edge blend
        vec3 n = fetch(vUV + vec2(0.0, uTexel.y));
        vec3 s = fetch(vUV - vec2(0.0, uTexel.y));
        vec3 e = fetch(vUV + vec2(uTexel.x, 0.0));
        vec3 w = fetch(vUV - vec2(uTexel.x, 0.0));
        vec3 avg = (n + s + e + w) * 0.25;
        float edge = length(c - avg);
        c = mix(c, avg, clamp(edge * 1.6, 0.0, 0.55));
    }
    // vignette + subtle grain
    vec2 q = vUV - 0.5;
    float vig = 1.0 - dot(q, q) * uVignette;
    c *= vig;
    float g = fract(sin(dot(vUV * 1024.0 + uTime, vec2(12.9898, 78.233))) * 43758.5453);
    c += (g - 0.5) * 0.012;
    // damage tint
    c = mix(c, vec3(0.55, 0.06, 0.04), uDamage * smoothstep(0.15, 0.75, length(q)));
    outColor = vec4(c, 1.0);
}
)GLSL";

// ------------------------------------------------------------------ helpers
static GLuint compileShader(GLenum type, const char* src, const char* name) {
    GLuint s = gl().CreateShader(type);
    gl().ShaderSource(s, 1, &src, nullptr);
    gl().CompileShader(s);
    GLint okFlag = 0;
    gl().GetShaderiv(s, GL_COMPILE_STATUS, &okFlag);
    if (!okFlag) {
        char log[8192];
        GLsizei len = 0;
        gl().GetShaderInfoLog(s, sizeof(log) - 1, &len, log);
        log[len < (int)sizeof(log) ? len : (int)sizeof(log) - 1] = 0;
        logMsg("[GL] shader compile failed (%s):\n%s", name, log);
        gl().DeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint linkProgram(const char* vs, const char* fs, const char* name) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs, name);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs, name);
    if (!v || !f) return 0;
    GLuint p = gl().CreateProgram();
    gl().AttachShader(p, v);
    gl().AttachShader(p, f);
    gl().LinkProgram(p);
    GLint okFlag = 0;
    gl().GetProgramiv(p, GL_LINK_STATUS, &okFlag);
    if (!okFlag) {
        char log[8192];
        GLsizei len = 0;
        gl().GetProgramInfoLog(p, sizeof(log) - 1, &len, log);
        log[len < (int)sizeof(log) ? len : (int)sizeof(log) - 1] = 0;
        logMsg("[GL] program link failed (%s):\n%s", name, log);
        return 0;
    }
    gl().DeleteShader(v);
    gl().DeleteShader(f);
    return p;
}

struct ProgUniforms {
    GLint uViewProj = -1, uModel = -1, uLightVP = -1, uTime = -1, uWaterPass = -1;
    GLint uCamPos = -1, uSunDir = -1, uSunColor = -1, uSunIntensity = -1;
    GLint uAmbientSky = -1, uAmbientGround = -1, uFogColor = -1, uFogDensity = -1;
    GLint uExposure = -1, uWetness = -1, uCloudCover = -1, uShadowMap = -1, uShadowTexel = -1, uShadowStrength = -1;
    GLint uMatColor = -1, uMatInterior = -1, uMatProps = -1;
    GLint uFlashDir = -1, uFlashPos = -1, uFlashOn = -1;
    GLint uExpPos = -1, uExpColor = -1, uExpRadius = -1;
};

static void cacheUniforms(GLuint p, ProgUniforms& u) {
    u.uViewProj = gl().GetUniformLocation(p, "uViewProj");
    u.uModel = gl().GetUniformLocation(p, "uModel");
    u.uLightVP = gl().GetUniformLocation(p, "uLightVP");
    u.uTime = gl().GetUniformLocation(p, "uTime");
    u.uWaterPass = gl().GetUniformLocation(p, "uWaterPass");
    u.uCamPos = gl().GetUniformLocation(p, "uCamPos");
    u.uSunDir = gl().GetUniformLocation(p, "uSunDir");
    u.uSunColor = gl().GetUniformLocation(p, "uSunColor");
    u.uSunIntensity = gl().GetUniformLocation(p, "uSunIntensity");
    u.uAmbientSky = gl().GetUniformLocation(p, "uAmbientSky");
    u.uAmbientGround = gl().GetUniformLocation(p, "uAmbientGround");
    u.uFogColor = gl().GetUniformLocation(p, "uFogColor");
    u.uFogDensity = gl().GetUniformLocation(p, "uFogDensity");
    u.uExposure = gl().GetUniformLocation(p, "uExposure");
    u.uWetness = gl().GetUniformLocation(p, "uWetness");
    u.uCloudCover = gl().GetUniformLocation(p, "uCloudCover");
    u.uShadowMap = gl().GetUniformLocation(p, "uShadowMap");
    u.uShadowTexel = gl().GetUniformLocation(p, "uShadowTexel");
    u.uShadowStrength = gl().GetUniformLocation(p, "uShadowStrength");
    u.uMatColor = gl().GetUniformLocation(p, "uMatColor");
    u.uMatInterior = gl().GetUniformLocation(p, "uMatInterior");
    u.uMatProps = gl().GetUniformLocation(p, "uMatProps");
    u.uFlashDir = gl().GetUniformLocation(p, "uFlashlightDir");
    u.uFlashPos = gl().GetUniformLocation(p, "uFlashlightPos");
    u.uFlashOn = gl().GetUniformLocation(p, "uFlashlightOn");
    u.uExpPos = gl().GetUniformLocation(p, "uExplosionLightPos");
    u.uExpColor = gl().GetUniformLocation(p, "uExplosionLightColor");
    u.uExpRadius = gl().GetUniformLocation(p, "uExplosionLightRadius");
}

// ------------------------------------------------------------------ renderer
struct GLRenderer : IRenderer {
    Settings settings;
    Camera cam;
    EnvState env;
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    GLuint progWorld = 0, progSky = 0, progPart = 0, progBox = 0, progUI = 0, progPost = 0;
    ProgUniforms uw, ub;
    GLuint fbo = 0, texColor = 0, rbDepth = 0;
    GLuint shadowFBO = 0, shadowTex = 0;
    int shadowSize = 2048;
    GLuint emptyVAO = 0;
    GLuint fontTex = 0;
    GLuint uiVAO = 0, uiVBO = 0;
    GLuint partVAO = 0, partVBO = 0, partInstVBO = 0;
    GLuint boxVAO = 0, boxVBO = 0, boxInstVBO = 0;
    GLuint skyVAO = 0;
    mat4 lightVP;
    int drawCalls = 0, trisDrawn = 0;
    std::vector<BoxInstance> boxScratch;
    std::vector<Particle> partScratch;
    bool ready = false;
    float flashlightOn = 0;
    vec3 flashlightPos, flashlightDir;
    typedef FXLight ExpLight;
    ExpLight expLights[4];
    float damageFx = 0;
    std::deque<MeshGPU> meshHandles;
    std::vector<int> meshSlot;   // index into meshHandles -> vao etc.

    bool init(void* hwnd, int w, int h, const Settings& st) override {
        (void)hwnd;
        if (!gl().ok && !glLoad()) {
            logMsg("[GL] required functions missing");
            return false;
        }
        winW = w; winH = h;
        settings = st;
        const char* ver = (const char*)glGetString(GL_VERSION);
        const char* rend = (const char*)glGetString(GL_RENDERER);
        logMsg("[GL] %s | %s", ver ? ver : "?", rend ? rend : "?");
        progWorld = linkProgram(VS_WORLD, FS_WORLD, "world");
        progSky = linkProgram(VS_SKY, FS_SKY, "sky");
        progPart = linkProgram(VS_PART, FS_PART, "particles");
        progBox = linkProgram(VS_BOX, FS_BOX, "boxes");
        progUI = linkProgram(VS_UI, FS_UI, "ui");
        progPost = linkProgram(VS_POST, FS_POST, "post");
        if (!progWorld || !progSky || !progPart || !progBox || !progUI || !progPost) return false;
        cacheUniforms(progWorld, uw);
        cacheUniforms(progBox, ub);
        gl().GenVertexArrays(1, &emptyVAO);
        gl().GenVertexArrays(1, &skyVAO);
        createShadowMap();
        createFontTexture();
        createFullscreen();
        createParticleBuffers();
        createBoxBuffers();
        gl().GenVertexArrays(1, &uiVAO);
        gl().GenBuffers(1, &uiVBO);
        gl().BindVertexArray(uiVAO);
        gl().BindBuffer(GL_ARRAY_BUFFER, uiVBO);
        gl().EnableVertexAttribArray(0);
        gl().VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVert), (void*)0);
        gl().EnableVertexAttribArray(1);
        gl().VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVert), (void*)8);
        gl().EnableVertexAttribArray(2);
        gl().VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UIVert), (void*)16);
        gl().BindVertexArray(0);
        ready = true;
        logMsg("[GL] renderer ready (%dx%d)", w, h);
        return true;
    }

    void createShadowMap() {
        shadowSize = settings.shadowQuality >= 3 ? 4096 : (settings.shadowQuality == 2 ? 2048 : 1024);
        glGenTextures(1, &shadowTex);
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl().GenFramebuffers(1, &shadowFBO);
        gl().BindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        gl().FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (gl().CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) logMsg("[GL] shadow FBO incomplete");
        gl().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void createFontTexture() {
        const FontAtlas& fa = fontAtlas();
        glGenTextures(1, &fontTex);
        glBindTexture(GL_TEXTURE_2D, fontTex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, fa.width, fa.height, 0, GL_RED, GL_UNSIGNED_BYTE, fa.data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void createFullscreen() {
        glGenTextures(1, &texColor);
        glBindTexture(GL_TEXTURE_2D, texColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, winW, winH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl().GenRenderbuffers(1, &rbDepth);
        gl().BindRenderbuffer(GL_RENDERBUFFER, rbDepth);
        gl().RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, winW, winH);
        gl().GenFramebuffers(1, &fbo);
        gl().BindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl().FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColor, 0);
        gl().FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbDepth);
        if (gl().CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) logMsg("[GL] main FBO incomplete");
        gl().BindFramebuffer(GL_FRAMEBUFFER, 0);
        fbW = winW; fbH = winH;
    }

    void createParticleBuffers() {
        gl().GenVertexArrays(1, &partVAO);
        gl().GenBuffers(1, &partInstVBO);
        gl().BindVertexArray(partVAO);
        gl().BindBuffer(GL_ARRAY_BUFFER, partInstVBO);
        // instance layout: pos(3f) size(f) color(4ub) info(4ub) = 24 bytes
        const int stride = 24;
        gl().EnableVertexAttribArray(0);
        gl().VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        gl().VertexAttribDivisor(0, 1);
        gl().EnableVertexAttribArray(1);
        gl().VertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (void*)12);
        gl().VertexAttribDivisor(1, 1);
        gl().EnableVertexAttribArray(2);
        gl().VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)16);
        gl().VertexAttribDivisor(2, 1);
        gl().EnableVertexAttribArray(3);
        gl().VertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, (void*)20);
        gl().VertexAttribDivisor(3, 1);
        gl().BindVertexArray(0);
    }

    void createBoxBuffers() {
        // unit cube: 36 verts, pos + normal
        float v[36 * 6];
        int n = 0;
        auto quad = [&](vec3 a, vec3 b, vec3 c, vec3 d, vec3 nrm) {
            vec3 tri[6] = {a, b, c, a, c, d};
            for (int i = 0; i < 6; i++) {
                v[n++] = tri[i].x; v[n++] = tri[i].y; v[n++] = tri[i].z;
                v[n++] = nrm.x; v[n++] = nrm.y; v[n++] = nrm.z;
            }
        };
        vec3 p000(-0.5f, -0.5f, -0.5f), p100(0.5f, -0.5f, -0.5f), p110(0.5f, 0.5f, -0.5f), p010(-0.5f, 0.5f, -0.5f);
        vec3 p001(-0.5f, -0.5f, 0.5f), p101(0.5f, -0.5f, 0.5f), p111(0.5f, 0.5f, 0.5f), p011(-0.5f, 0.5f, 0.5f);
        quad(p001, p101, p111, p011, vec3(0, 0, 1));
        quad(p100, p000, p010, p110, vec3(0, 0, -1));
        quad(p010, p011, p111, p110, vec3(0, 1, 0));
        quad(p000, p100, p101, p001, vec3(0, -1, 0));
        quad(p101, p100, p110, p111, vec3(1, 0, 0));
        quad(p000, p001, p011, p010, vec3(-1, 0, 0));
        gl().GenVertexArrays(1, &boxVAO);
        gl().GenBuffers(1, &boxVBO);
        gl().BindVertexArray(boxVAO);
        gl().BindBuffer(GL_ARRAY_BUFFER, boxVBO);
        gl().BufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        gl().EnableVertexAttribArray(0);
        gl().VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);
        gl().EnableVertexAttribArray(1);
        gl().VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12);
        gl().GenBuffers(1, &boxInstVBO);
        gl().BindBuffer(GL_ARRAY_BUFFER, boxInstVBO);
        const int stride = 16 * 4 + 16 + 16;
        for (int i = 0; i < 4; i++) {
            gl().EnableVertexAttribArray(2 + i);
            gl().VertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)(i * 16));
            gl().VertexAttribDivisor(2 + i, 1);
        }
        gl().EnableVertexAttribArray(6);
        gl().VertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)(64));
        gl().VertexAttribDivisor(6, 1);
        gl().EnableVertexAttribArray(7);
        gl().VertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride, (void*)(size_t)(80));
        gl().VertexAttribDivisor(7, 1);
        gl().BindVertexArray(0);
    }

    void shutdown() override {}
    void resize(int w, int h) override {
        winW = w; winH = h;
        if (!ready) return;
        glBindTexture(GL_TEXTURE_2D, texColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        gl().BindRenderbuffer(GL_RENDERBUFFER, rbDepth);
        gl().RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        fbW = w; fbH = h;
    }
    void setSettings(const Settings& st) override {
        settings = st;
        int want = st.shadowQuality >= 3 ? 4096 : (st.shadowQuality == 2 ? 2048 : 1024);
        if (st.shadowQuality == 0) want = 1024;
        if (want != shadowSize) {
            shadowSize = want;
            gl().DeleteFramebuffers(1, &shadowFBO);
            glDeleteTextures(1, &shadowTex);
            createShadowMap();
        }
    }
    const char* name() const override { return "OpenGL 3.3"; }
    int frameWidth() const override { return winW; }
    int frameHeight() const override { return winH; }

    void setFlashlight(bool on, const vec3& pos, const vec3& dir) override {
        flashlightOn = on ? 1.0f : 0.0f;
        flashlightPos = pos;
        flashlightDir = dir;
    }
    void setExplosionLights(const FXLight* l, int n) override {
        for (int i = 0; i < 4; i++) expLights[i] = i < n ? l[i] : ExpLight{vec3(0), vec3(0), 0};
    }
    void setDamageFx(float d) override { damageFx = d; }

    void uploadWorldUniforms(GLuint prog, const ProgUniforms& u) {
        gl().UniformMatrix4fv(u.uViewProj, 1, GL_FALSE, cam.viewProj().m);
        gl().UniformMatrix4fv(u.uLightVP, 1, GL_FALSE, lightVP.m);
        gl().Uniform3f(u.uCamPos, cam.pos.x, cam.pos.y, cam.pos.z);
        gl().Uniform3f(u.uSunDir, env.sunDir.x, env.sunDir.y, env.sunDir.z);
        gl().Uniform3f(u.uSunColor, env.sunColor.x, env.sunColor.y, env.sunColor.z);
        gl().Uniform1f(u.uSunIntensity, env.sunIntensity);
        gl().Uniform3f(u.uAmbientSky, env.ambientSky.x, env.ambientSky.y, env.ambientSky.z);
        gl().Uniform3f(u.uAmbientGround, env.ambientGround.x, env.ambientGround.y, env.ambientGround.z);
        gl().Uniform3f(u.uFogColor, env.fogColor.x, env.fogColor.y, env.fogColor.z);
        gl().Uniform1f(u.uFogDensity, env.fogDensity);
        gl().Uniform1f(u.uExposure, env.exposure);
        gl().Uniform1f(u.uWetness, env.wetness);
        gl().Uniform1f(u.uTime, env.time);
        gl().Uniform1f(u.uCloudCover, env.cloudCover);
        gl().Uniform1f(u.uShadowTexel, 1.0f / shadowSize);
        gl().Uniform1f(u.uShadowStrength, settings.shadowQuality == 0 ? 0.0f : 1.0f);
        gl().ActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        gl().Uniform1i(u.uShadowMap, 1);
        // material palette
        static std::vector<float> colors, interiors, props;
        if (colors.empty()) {
            colors.resize(M_COUNT * 3);
            interiors.resize(M_COUNT * 3);
            props.resize(M_COUNT * 4);
            for (int m = 0; m < M_COUNT; m++) {
                const Material& mm = material((uint8_t)m);
                colors[m * 3 + 0] = mm.color.x; colors[m * 3 + 1] = mm.color.y; colors[m * 3 + 2] = mm.color.z;
                interiors[m * 3 + 0] = mm.interior.x; interiors[m * 3 + 1] = mm.interior.y; interiors[m * 3 + 2] = mm.interior.z;
                props[m * 4 + 0] = mm.roughness;
                props[m * 4 + 1] = mm.metallic;
                props[m * 4 + 2] = (float)mm.pattern;
                props[m * 4 + 3] = mm.transparent ? 1.0f : 0.0f;
            }
        }
        gl().Uniform3fv(u.uMatColor, M_COUNT, colors.data());
        gl().Uniform3fv(u.uMatInterior, M_COUNT, interiors.data());
        // props are vec4 -> upload as 4fv array (layout matches)
        static GLint locPropsTmp = -1;
        locPropsTmp = u.uMatProps;
        gl().Uniform4fv(locPropsTmp, M_COUNT, props.data());
        gl().Uniform3f(u.uFlashDir, flashlightDir.x, flashlightDir.y, flashlightDir.z);
        gl().Uniform3f(u.uFlashPos, flashlightPos.x, flashlightPos.y, flashlightPos.z);
        gl().Uniform1f(u.uFlashOn, flashlightOn);
        if (u.uExpPos >= 0) {
            float p[12], c[12], r[4];
            for (int i = 0; i < 4; i++) {
                p[i * 3 + 0] = expLights[i].pos.x; p[i * 3 + 1] = expLights[i].pos.y; p[i * 3 + 2] = expLights[i].pos.z;
                c[i * 3 + 0] = expLights[i].color.x; c[i * 3 + 1] = expLights[i].color.y; c[i * 3 + 2] = expLights[i].color.z;
                r[i] = expLights[i].radius;
            }
            gl().Uniform3fv(u.uExpPos, 4, p);
            gl().Uniform3fv(u.uExpColor, 4, c);
            float* rr = r;
            gl().Uniform1fv(u.uExpRadius, 4, rr);
        }
    }

    void computeLightMatrix() {
        // a single shadow cascade that follows the camera
        float extent = 55.0f;
        vec3 focus = cam.pos + cam.forward() * (extent * 0.42f);
        focus.y = std::max(focus.y, 4.0f);
        vec3 ldir = env.sunDir.norm();
        vec3 up = std::fabs(ldir.y) > 0.95f ? vec3(0, 0, 1) : vec3(0, 1, 0);
        vec3 eye = focus + ldir * 160.0f;
        mat4 view = matLookAt(eye, focus, up);
        mat4 proj = matOrtho(-extent, extent, -extent, extent, 1.0f, 320.0f);
        lightVP = proj * view;
    }

    void beginFrame(const Camera& c, const EnvState& e, const Settings& st) override {
        cam = c; env = e;
        settings = st;
        if (!ready) return;
        computeLightMatrix();
        glViewport(0, 0, winW, winH);
        gl().BindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(0.02f, 0.03f, 0.05f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        // sky
        gl().UseProgram(progSky);
        gl().UniformMatrix4fv(gl().GetUniformLocation(progSky, "uInvViewProj"), 1, GL_FALSE, matInverse(cam.viewProj()).m);
        gl().Uniform3f(gl().GetUniformLocation(progSky, "uCamPos"), cam.pos.x, cam.pos.y, cam.pos.z);
        gl().Uniform3f(gl().GetUniformLocation(progSky, "uSunDir"), env.sunDir.x, env.sunDir.y, env.sunDir.z);
        gl().Uniform3f(gl().GetUniformLocation(progSky, "uSunColor"), env.sunColor.x, env.sunColor.y, env.sunColor.z);
        gl().Uniform1f(gl().GetUniformLocation(progSky, "uSunIntensity"), env.sunIntensity);
        gl().Uniform3f(gl().GetUniformLocation(progSky, "uSkyTop"), env.skyTop.x, env.skyTop.y, env.skyTop.z);
        gl().Uniform3f(gl().GetUniformLocation(progSky, "uSkyHorizon"), env.skyHorizon.x, env.skyHorizon.y, env.skyHorizon.z);
        gl().Uniform3f(gl().GetUniformLocation(progSky, "uFogColor"), env.fogColor.x, env.fogColor.y, env.fogColor.z);
        gl().Uniform1f(gl().GetUniformLocation(progSky, "uCloudCover"), env.cloudCover);
        gl().Uniform1f(gl().GetUniformLocation(progSky, "uTime"), env.time);
        gl().Uniform1f(gl().GetUniformLocation(progSky, "uExposure"), env.exposure);
        gl().Uniform3f(gl().GetUniformLocation(progSky, "uAmbientSky"), env.ambientSky.x, env.ambientSky.y, env.ambientSky.z);
        glDepthMask(GL_FALSE);
        gl().BindVertexArray(skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDepthMask(GL_TRUE);
        drawCalls = 0;
        trisDrawn = 0;
    }

    void shadowPass(World& world) {
        if (settings.shadowQuality == 0) return;
        gl().BindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glViewport(0, 0, shadowSize, shadowSize);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        gl().UseProgram(progWorld);
        uploadWorldUniforms(progWorld, uw);
        gl().UniformMatrix4fv(uw.uModel, 1, GL_FALSE, mat4().m);
        gl().Uniform1i(uw.uWaterPass, 0);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.2f, 2.0f);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        vec3 ldir = env.sunDir.norm();
        for (auto& kv : world.meshes) {
            ChunkMesh& m = kv.second;
            if (m.indexCount == 0 || !m.gpuReady) continue;
            vec3 center = m.bounds.center();
            float dist = (center - cam.pos).len();
            if (dist > 150.0f) continue;
            // shadow-caster test: the chunk must be in front of the sun relative to the cast volume
            if (dot(center - cam.pos, ldir) < -70.0f) continue;
            gl().BindVertexArray(m.vao);
            gl().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr);
            trisDrawn += m.indexCount / 3;
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_POLYGON_OFFSET_FILL);
        gl().BindVertexArray(0);
        gl().BindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, winW, winH);
    }

    void drawWorld(World& world, const Settings& st) override {
        if (!ready) return;
        settings = st;
        // upload dirty chunk meshes
        int uploads = 0;
        for (auto& kv : world.meshes) {
            ChunkMesh& m = kv.second;
            if (!m.gpuDirty) continue;
            if (uploads > 6) break;
            if (!m.gpuReady) {
                gl().GenVertexArrays(1, &m.vao);
                gl().GenBuffers(1, &m.vbo);
                gl().GenBuffers(1, &m.ibo);
                gl().GenBuffers(1, &m.iboTrans);
                m.gpuReady = true;
            }
            gl().BindVertexArray(m.vao);
            gl().BindBuffer(GL_ARRAY_BUFFER, m.vbo);
            gl().BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m.verts.size() * sizeof(VoxVert)), m.verts.data(), GL_STATIC_DRAW);
            gl().EnableVertexAttribArray(0);
            gl().VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxVert), (void*)0);
            gl().EnableVertexAttribArray(1);
            gl().VertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, sizeof(VoxVert), (void*)12);
            gl().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
            gl().BufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(m.idx.size() * 4), m.idx.data(), GL_STATIC_DRAW);
            m.indexCount = (int)m.idx.size();
            gl().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.iboTrans);
            gl().BufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(m.idxTrans.size() * 4), m.idxTrans.data(), GL_STATIC_DRAW);
            m.indexCountTrans = (int)m.idxTrans.size();
            gl().BindVertexArray(0);
            m.gpuDirty = false;
            uploads++;
        }
        shadowPass(world);

        gl().UseProgram(progWorld);
        uploadWorldUniforms(progWorld, uw);
        gl().UniformMatrix4fv(uw.uModel, 1, GL_FALSE, mat4().m);
        gl().Uniform1i(uw.uWaterPass, 0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        gl().BindVertexArray(emptyVAO);
        // opaque chunks
        int drawn = 0;
        for (auto& kv : world.meshes) {
            ChunkMesh& m = kv.second;
            if (m.indexCount == 0 || !m.gpuReady) continue;
            vec3 center = m.bounds.center();
            float dist = (center - cam.pos).len();
            if (dist > settings.viewDistance) continue;
            // sphere/AABB frustum test
            if (!chunkInFrustum(m.bounds)) continue;
            gl().BindVertexArray(m.vao);
            gl().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr);
            trisDrawn += m.indexCount / 3;
            drawCalls++;
            if (++drawn > 5000) break;
        }
        // transparent chunks (water, glass, foliage) — back to front
        std::vector<std::pair<float, ChunkMesh*>> trans;
        for (auto& kv : world.meshes) {
            ChunkMesh& m = kv.second;
            if (m.indexCountTrans == 0 || !m.gpuReady) continue;
            float dist = (m.bounds.center() - cam.pos).len();
            if (dist > settings.viewDistance) continue;
            if (!chunkInFrustum(m.bounds)) continue;
            trans.push_back({dist, &m});
        }
        std::sort(trans.begin(), trans.end(), [](auto& a, auto& b) { return a.first > b.first; });
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (auto& t : trans) {
            gl().BindVertexArray(t.second->vao);
            gl().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, t.second->iboTrans);
            glDrawElements(GL_TRIANGLES, t.second->indexCountTrans, GL_UNSIGNED_INT, nullptr);
            trisDrawn += t.second->indexCountTrans / 3;
            drawCalls++;
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        gl().BindVertexArray(0);
    }

    bool chunkInFrustum(const AABB& b) {
        // extract 6 planes from viewProj (column-major)
        mat4 vp = cam.viewProj();
        vec4 planes[6];
        auto row = [&](int r) { return vec4(vp.at(0, r), vp.at(1, r), vp.at(2, r), vp.at(3, r)); };
        vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        planes[0] = r3 + r0;
        planes[1] = r3 + vec4(-r0.x, -r0.y, -r0.z, -r0.w);
        planes[2] = r3 + r1;
        planes[3] = r3 + vec4(-r1.x, -r1.y, -r1.z, -r1.w);
        planes[4] = r3 + r2;
        planes[5] = r3 + vec4(-r2.x, -r2.y, -r2.z, -r2.w);
        for (int i = 0; i < 6; i++) {
            vec3 n(planes[i].x, planes[i].y, planes[i].z);
            float d = planes[i].w;
            vec3 p((n.x > 0) ? b.mx.x : b.mn.x, (n.y > 0) ? b.mx.y : b.mn.y, (n.z > 0) ? b.mx.z : b.mn.z);
            if (dot(n, p) + d < 0) return false;
        }
        return true;
    }

    MeshGPU* createMesh() override {
        meshHandles.push_back(MeshGPU());
        return &meshHandles.back();
    }
    void updateMesh(MeshGPU* m, const std::vector<VoxVert>& verts, const std::vector<uint32_t>& idx) override {
        if (!m->ready) {
            gl().GenVertexArrays(1, &m->vao);
            gl().GenBuffers(1, &m->vbo);
            gl().GenBuffers(1, &m->ibo);
            m->ready = true;
        }
        gl().BindVertexArray(m->vao);
        gl().BindBuffer(GL_ARRAY_BUFFER, m->vbo);
        gl().BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(VoxVert)), verts.data(), GL_STATIC_DRAW);
        gl().EnableVertexAttribArray(0);
        gl().VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxVert), (void*)0);
        gl().EnableVertexAttribArray(1);
        gl().VertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, sizeof(VoxVert), (void*)12);
        gl().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
        gl().BufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * 4), idx.data(), GL_STATIC_DRAW);
        m->indexCount = (int)idx.size();
        gl().BindVertexArray(0);
    }
    void drawMesh(MeshGPU* m, const mat4& model) override {
        if (!m->ready || m->indexCount == 0) return;
        gl().UseProgram(progWorld);
        uploadWorldUniforms(progWorld, uw);
        gl().UniformMatrix4fv(uw.uModel, 1, GL_FALSE, model.m);
        gl().Uniform1i(uw.uWaterPass, 0);
        gl().BindVertexArray(m->vao);
        gl().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
        glDrawElements(GL_TRIANGLES, m->indexCount, GL_UNSIGNED_INT, nullptr);
        trisDrawn += m->indexCount / 3;
        drawCalls++;
    }
    void destroyMesh(MeshGPU* m) override { (void)m; }

    void drawBoxes(const BoxInstance* boxes, int count) override {
        if (count <= 0) return;
        std::vector<float> data((size_t)count * 24);
        for (int i = 0; i < count; i++) {
            const BoxInstance& b = boxes[i];
            float* d = &data[i * 24];
            memcpy(d, b.model.m, 64);
            d[16] = b.color.x; d[17] = b.color.y; d[18] = b.color.z; d[19] = b.rough;
            d[20] = b.emis.x; d[21] = b.emis.y; d[22] = b.emis.z; d[23] = b.metal;
        }
        gl().UseProgram(progBox);
        uploadWorldUniforms(progBox, ub);
        gl().BindVertexArray(boxVAO);
        gl().BindBuffer(GL_ARRAY_BUFFER, boxInstVBO);
        gl().BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * 4), data.data(), GL_DYNAMIC_DRAW);
        gl().DrawArraysInstanced(GL_TRIANGLES, 0, 36, count);
        drawCalls++;
        trisDrawn += count * 12;
    }

    void drawParticles(const Particle* p, int count) override {
        if (count <= 0) return;
        vec3 right = cam.right(), up = cross(right, cam.forward());
        static std::vector<unsigned char> alphaData, addData;
        alphaData.clear();
        addData.clear();
        auto pushOne = [](std::vector<unsigned char>& buf, const Particle& pt) {
            size_t off = buf.size();
            buf.resize(off + 24);
            float* f = (float*)&buf[off];
            f[0] = pt.pos.x; f[1] = pt.pos.y; f[2] = pt.pos.z; f[3] = pt.size;
            uint32_t c = pt.color;
            buf[off + 16] = (unsigned char)((c >> 24) & 0xFF);
            buf[off + 17] = (unsigned char)((c >> 16) & 0xFF);
            buf[off + 18] = (unsigned char)((c >> 8) & 0xFF);
            buf[off + 19] = (unsigned char)(c & 0xFF);
            buf[off + 20] = pt.kind;
            buf[off + 21] = (unsigned char)(clampf(pt.life / std::max(0.01f, pt.maxLife), 0, 1) * 255.0f);
            buf[off + 22] = pt.rot;
            buf[off + 23] = 0;
        };
        for (int i = 0; i < count; i++) {
            const Particle& pt = p[i];
            if (pt.life <= 0 || pt.size <= 0) continue;
            if (pt.kind == 2 || pt.kind == 3) pushOne(addData, pt);
            else pushOne(alphaData, pt);
        }
        int nA = (int)(alphaData.size() / 24), nB = (int)(addData.size() / 24);
        if (nA + nB == 0) return;
        gl().UseProgram(progPart);
        GLuint pProg = progPart;
        gl().UniformMatrix4fv(gl().GetUniformLocation(pProg, "uViewProj"), 1, GL_FALSE, cam.viewProj().m);
        gl().Uniform3f(gl().GetUniformLocation(pProg, "uCamRight"), right.x, right.y, right.z);
        gl().Uniform3f(gl().GetUniformLocation(pProg, "uCamUp"), up.x, up.y, up.z);
        gl().Uniform3f(gl().GetUniformLocation(pProg, "uAmbientSky"), env.ambientSky.x, env.ambientSky.y, env.ambientSky.z);
        gl().Uniform3f(gl().GetUniformLocation(pProg, "uSunColor"), env.sunColor.x, env.sunColor.y, env.sunColor.z);
        gl().Uniform1f(gl().GetUniformLocation(pProg, "uExposure"), env.exposure);
        gl().Uniform1f(gl().GetUniformLocation(pProg, "uCloudCover"), env.cloudCover);
        gl().BindVertexArray(partVAO);
        gl().BindBuffer(GL_ARRAY_BUFFER, partInstVBO);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        if (nA > 0) {
            gl().BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)alphaData.size(), alphaData.data(), GL_DYNAMIC_DRAW);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gl().DrawArraysInstanced(GL_TRIANGLES, 0, 6, nA);
            drawCalls++;
        }
        if (nB > 0) {
            gl().BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)addData.size(), addData.data(), GL_DYNAMIC_DRAW);
            glBlendFunc(GL_ONE, GL_ONE);
            gl().DrawArraysInstanced(GL_TRIANGLES, 0, 6, nB);
            drawCalls++;
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        gl().BindVertexArray(0);
    }

    void drawWater(World&, const EnvState&) override {}

    const UIDrawList* pendingOverlay = nullptr;
    void drawOverlay(const UIDrawList& ui) override { pendingOverlay = &ui; }
    void drawOverlayNow(const UIDrawList& ui) {
        if (ui.verts.empty()) return;
        gl().BindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, winW, winH);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gl().UseProgram(progUI);
        gl().Uniform2f(gl().GetUniformLocation(progUI, "uScreen"), (float)winW, (float)winH);
        gl().ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTex);
        gl().Uniform1i(gl().GetUniformLocation(progUI, "uFont"), 0);
        gl().BindVertexArray(uiVAO);
        gl().BindBuffer(GL_ARRAY_BUFFER, uiVBO);
        gl().BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ui.verts.size() * sizeof(UIVert)), ui.verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)ui.verts.size());
        gl().BindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        drawCalls++;
    }

    void endFrame() override {
        if (!ready) return;
        gl().BindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, winW, winH);
        glDisable(GL_DEPTH_TEST);
        gl().UseProgram(progPost);
        gl().ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texColor);
        gl().Uniform1i(gl().GetUniformLocation(progPost, "uScene"), 0);
        gl().Uniform2f(gl().GetUniformLocation(progPost, "uTexel"), 1.0f / winW, 1.0f / winH);
        gl().Uniform1i(gl().GetUniformLocation(progPost, "uAA"), settings.aaMode == 1 ? 1 : 0);
        gl().Uniform1f(gl().GetUniformLocation(progPost, "uVignette"), 0.45f);
        gl().Uniform1f(gl().GetUniformLocation(progPost, "uTime"), env.time);
        gl().Uniform1f(gl().GetUniformLocation(progPost, "uDamage"), damageFx);
        gl().BindVertexArray(skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        gl().BindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        if (pendingOverlay) {
            drawOverlayNow(*pendingOverlay);
            pendingOverlay = nullptr;
        }
    }

    void setViewportOverride(int x, int y, int w, int h, bool depthClear) override {
        gl().BindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(x, winH - y - h, w, h);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, winH - y - h, w, h);
        if (depthClear) glClear(GL_DEPTH_BUFFER_BIT);
    }
    void clearViewportOverride() override {
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, winW, winH);
    }
    bool screenshot(const char* path) override {
        std::vector<uint32_t> px((size_t)winW * winH);
        // read the back buffer AFTER the post pass and overlay so the capture matches the screen
        gl().BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, winW, winH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glReadBuffer(GL_BACK);
        // flip rows and BGRA->RGBA
        std::vector<uint32_t> out((size_t)winW * winH);
        for (int y = 0; y < winH; y++)
            for (int x = 0; x < winW; x++) {
                uint32_t p = px[(size_t)(winH - 1 - y) * winW + x];
                uint32_t b = p & 0xFF, g = (p >> 8) & 0xFF, r = (p >> 16) & 0xFF, a = (p >> 24) & 0xFF;
                out[(size_t)y * winW + x] = (r << 24) | (g << 16) | (b << 8) | (a ? a : 255);
            }
        return writePNG(path, out.data(), winW, winH);
    }
};

IRenderer* createGLRenderer() { return new GLRenderer; }

#else
IRenderer* createGLRenderer() { return nullptr; }
bool glLoad() { return false; }
#endif
