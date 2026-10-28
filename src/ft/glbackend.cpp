// FRACTURE — OpenGL 3.3 core renderer (mirrors swbackend shading; SW = source of truth)
#include "render.h"
#include "gl_min.h"
#include "font_data.h"
#include <cstring>
#include <type_traits>
#include <algorithm>

namespace ft {

// ------------------------------------------------------------ GL pointers ---
void (*glEnable)(GLenum) = nullptr;
void (*glDisable)(GLenum) = nullptr;
void (*glViewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*glClear)(GLbitfield) = nullptr;
void (*glDepthFunc)(GLenum) = nullptr;
void (*glDepthMask)(GLboolean) = nullptr;
void (*glBlendFunc)(GLenum, GLenum) = nullptr;
void (*glCullFace)(GLenum) = nullptr;
void (*glFrontFace)(GLenum) = nullptr;
void (*glPixelStorei)(GLenum, GLint) = nullptr;
GLenum (*glGetError)() = nullptr;
const GLubyte* (*glGetString)(GLenum) = nullptr;
void (*glGenBuffers)(GLsizei, GLuint*) = nullptr;
void (*glBindBuffer)(GLenum, GLuint) = nullptr;
void (*glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (*glBufferSubData)(GLenum, GLintptr_off, GLsizeiptr, const void*) = nullptr;
void (*glDeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (*glGenVertexArrays)(GLsizei, GLuint*) = nullptr;
void (*glBindVertexArray)(GLuint) = nullptr;
void (*glDeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
void (*glEnableVertexAttribArray)(GLuint) = nullptr;
void (*glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
void (*glVertexAttribDivisor)(GLuint, GLuint) = nullptr;
void (*glDrawArrays)(GLenum, GLint, GLsizei) = nullptr;
void (*glDrawElements)(GLenum, GLsizei, GLenum, const void*) = nullptr;
GLuint (*glCreateShader)(GLenum) = nullptr;
void (*glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
void (*glCompileShader)(GLuint) = nullptr;
void (*glGetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*glDeleteShader)(GLuint) = nullptr;
GLuint (*glCreateProgram)() = nullptr;
void (*glAttachShader)(GLuint, GLuint) = nullptr;
void (*glLinkProgram)(GLuint) = nullptr;
void (*glGetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*glUseProgram)(GLuint) = nullptr;
void (*glDeleteProgram)(GLuint) = nullptr;
GLint (*glGetUniformLocation)(GLuint, const GLchar*) = nullptr;
void (*glUniform1i)(GLint, GLint) = nullptr;
void (*glUniform1f)(GLint, GLfloat) = nullptr;
void (*glUniform2f)(GLint, GLfloat, GLfloat) = nullptr;
void (*glUniform3f)(GLint, GLfloat, GLfloat, GLfloat) = nullptr;
void (*glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*glUniform3fv)(GLint, GLsizei, const GLfloat*) = nullptr;
void (*glUniform4fv)(GLint, GLsizei, const GLfloat*) = nullptr;
void (*glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
void (*glGenTextures)(GLsizei, GLuint*) = nullptr;
void (*glBindTexture)(GLenum, GLuint) = nullptr;
void (*glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
void (*glTexParameteri)(GLenum, GLenum, GLint) = nullptr;
void (*glDeleteTextures)(GLsizei, const GLuint*) = nullptr;
void (*glActiveTexture)(GLenum) = nullptr;
void (*glGenerateMipmap)(GLenum) = nullptr;
void (*glGenFramebuffers)(GLsizei, GLuint*) = nullptr;
void (*glBindFramebuffer)(GLenum, GLuint) = nullptr;
void (*glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
GLenum (*glCheckFramebufferStatus)(GLenum) = nullptr;
void (*glDeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
void (*glDrawBuffer)(GLenum) = nullptr;
void (*glReadBuffer)(GLenum) = nullptr;
void (*glFinish)() = nullptr;
void (*glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;

// platform hook: resolve "glFunc" → address (opengl32 GetProcAddress then wgl)
void* (*ft_getproc)(const char*) = nullptr;

static bool loadGL() {
    if (!ft_getproc) return false;
    bool ok = true;
    auto L = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(ft_getproc(name));
        if (!fn) { errf("GL missing %s", name); ok = false; }
    };
#define GLLOAD(f) L(f, "gl" #f)
    GLLOAD(glEnable); GLLOAD(glDisable); GLLOAD(glViewport); GLLOAD(glClearColor); GLLOAD(glClear);
    GLLOAD(glDepthFunc); GLLOAD(glDepthMask); GLLOAD(glBlendFunc); GLLOAD(glCullFace); GLLOAD(glFrontFace);
    GLLOAD(glPixelStorei); GLLOAD(glGetError); GLLOAD(glGetString);
    GLLOAD(glGenBuffers); GLLOAD(glBindBuffer); GLLOAD(glBufferData); GLLOAD(glBufferSubData); GLLOAD(glDeleteBuffers);
    GLLOAD(glGenVertexArrays); GLLOAD(glBindVertexArray); GLLOAD(glDeleteVertexArrays);
    GLLOAD(glEnableVertexAttribArray); GLLOAD(glVertexAttribPointer); GLLOAD(glVertexAttribDivisor);
    GLLOAD(glDrawArrays); GLLOAD(glDrawElements);
    GLLOAD(glCreateShader); GLLOAD(glShaderSource); GLLOAD(glCompileShader); GLLOAD(glGetShaderiv);
    GLLOAD(glGetShaderInfoLog); GLLOAD(glDeleteShader); GLLOAD(glCreateProgram); GLLOAD(glAttachShader);
    GLLOAD(glLinkProgram); GLLOAD(glGetProgramiv); GLLOAD(glGetProgramInfoLog); GLLOAD(glUseProgram);
    GLLOAD(glDeleteProgram); GLLOAD(glGetUniformLocation); GLLOAD(glUniform1i); GLLOAD(glUniform1f);
    GLLOAD(glUniform2f); GLLOAD(glUniform3f); GLLOAD(glUniform4f); GLLOAD(glUniform3fv);
    GLLOAD(glUniform4fv); GLLOAD(glUniformMatrix4fv);
    GLLOAD(glGenTextures); GLLOAD(glBindTexture); GLLOAD(glTexImage2D); GLLOAD(glTexParameteri);
    GLLOAD(glDeleteTextures); GLLOAD(glActiveTexture); GLLOAD(glGenerateMipmap);
    GLLOAD(glGenFramebuffers); GLLOAD(glBindFramebuffer); GLLOAD(glFramebufferTexture2D);
    GLLOAD(glCheckFramebufferStatus); GLLOAD(glDeleteFramebuffers); GLLOAD(glDrawBuffer); GLLOAD(glReadBuffer);
    GLLOAD(glFinish); GLLOAD(glReadPixels);
#undef GLLOAD
    return ok;
}

// ------------------------------------------------------------------ shaders --
static const char* VS_VOXEL = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec3 aCol;
layout(location=3) in float aAo;
layout(location=4) in int aMat;
layout(location=5) in int aFlags;
uniform mat4 uVP, uModel, uLightVP;
uniform vec3 uTint;
out vec3 vW; out vec3 vN; out vec3 vC; out float vAo;
flat out int vMat; flat out int vFlags;
out vec4 vSc;
void main(){
  vec4 wp = uModel * vec4(aPos,1.0);
  vW = wp.xyz;
  vN = normalize(mat3(uModel) * aNrm);
  vC = aCol * uTint; vAo = aAo; vMat = aMat; vFlags = aFlags;
  vSc = uLightVP * wp;
  gl_Position = uVP * wp;
})";

// lighting ported from swbackend shadeVertex; triplanar mirror-wrap matches SW
static const char* FS_VOXEL = R"(#version 330 core
in vec3 vW; in vec3 vN; in vec3 vC; in float vAo;
flat in int vMat; flat in int vFlags;
in vec4 vSc;
out vec4 Frag;
uniform sampler2D uAtlas;
uniform sampler2D uShadow;
uniform vec3 uCam, uSunDir, uSunColor, uZenith, uHorizon, uFogColor;
uniform float uSunInt, uSkyAmb, uFogDensity, uExposure, uFlash, uShadowOn;
uniform float uBlendAlpha;      // 1.0 opaque pass; water alpha on blend pass
uniform vec4 uMat[64];          // rough, metal, uvScale, unused
uniform vec4 uSlot[64];         // atlas base tile per material
uniform vec4 uPLpos[8], uPLcol[8]; uniform int uPLn;
uniform vec4 uSpotPos, uSpotDir, uSpotCol; uniform float uSpotCos;

vec2 tileUV(vec3 wp, vec3 N, float scale){
  vec3 an = abs(N);
  float u, v;
  if (an.x >= an.y && an.x >= an.z) { u = wp.y; v = wp.z; }
  else if (an.y >= an.z)            { u = wp.x; v = wp.z; }
  else                              { u = wp.x; v = wp.y; }
  u *= scale; v *= scale;
  float fu = fract(u), fv = fract(v);
  if (mod(floor(u), 2.0) != 0.0) fu = 1.0 - fu;   // GLSL mod handles negatives like C++ &1
  if (mod(floor(v), 2.0) != 0.0) fv = 1.0 - fv;
  float slot = uSlot[vMat].x + ((vFlags & 1) != 0 ? 32.0 : 0.0);
  float tx = mod(slot, 8.0), ty = floor(slot / 8.0);
  return vec2((tx + fu) / 8.0, (ty + fv) / 8.0);
}

void main(){
  vec3 N = normalize(vN);
  vec3 albedo = vC * texture(uAtlas, tileUV(vW, N, uMat[vMat].z)).rgb;
  float aoL = clamp(vAo, 0.0, 4.0);
  float aoF = aoL < 0.5 ? 0.5 : (aoL < 1.5 ? 0.45 : aoL < 2.5 ? 0.66 : aoL < 3.5 ? 0.85 : 1.0);

  float rough = clamp(uMat[vMat].x, 0.05, 1.0);
  float metal = uMat[vMat].y;

  vec3 L = normalize(-uSunDir);
  float diff = max(0.0, dot(N, L));
  vec3 hemi = mix(uHorizon * 0.40, uZenith * 0.62, N.y * 0.5 + 0.5) * uSkyAmb;
  vec3 sunC = uSunColor * (uSunInt * diff);
  vec3 Vv = normalize(uCam - vW);
  vec3 H = normalize(L + Vv);
  float shin = 2.0 + (1.0 - rough) * 90.0;
  float specAmt = metal > 0.5 ? 0.55 : 0.10;
  vec3 spec = uSunColor * (specAmt * diff * pow(max(0.0, dot(N, H)), shin) * uSunInt);
  // shadow only on the sun's diffuse+spec energy
  float sunMul = 1.0;
  if (uShadowOn > 0.5 && diff > 0.0) {
    vec3 pc = vSc.xyz / vSc.w * 0.5 + 0.5;
    if (pc.x > 0.0 && pc.x < 1.0 && pc.y > 0.0 && pc.y < 1.0 && pc.z < 1.0) {
      float bias = max(0.0026 * (1.0 - diff), 0.0009);
      float sum = 0.0;
      for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++) {
        float d = texture(uShadow, pc.xy + vec2(float(i), float(j)) * (1.0 / 2048.0)).r;
        sum += (pc.z - bias) <= d ? 1.0 : 0.0;
      }
      sunMul = sum / 9.0;
    }
  }
  vec3 total = hemi + (sunC + spec) * sunMul;
  for (int i = 0; i < 8; i++) {
    if (i >= uPLn) break;
    vec3 dl = uPLpos[i].xyz - vW;
    float d = length(dl);
    float r = uPLpos[i].w;
    if (d < r) {
      float att = 1.0 - d / r; att *= att;
      float nd = max(0.0, dot(N, dl / max(d, 1e-4)));
      total += uPLcol[i].rgb * (nd * att * uPLcol[i].a);
    }
  }
  if (uSpotCol.a > 0.0) {
    vec3 toP = uSpotPos.xyz - vW;
    float d = length(toP);
    if (d < uSpotPos.w) {
      float cd = dot(normalize(-toP), normalize(uSpotDir.xyz));
      float cone = clamp((cd - uSpotCos) / max(1e-4, 1.0 - uSpotCos), 0.0, 1.0);
      float att = 1.0 - d / uSpotPos.w;
      total += uSpotCol.rgb * (cone * cone * att * uSpotCol.a * max(0.0, dot(N, normalize(toP))));
    }
  }
  if (uFlash > 0.0) total += vec3(1.0, 0.96, 0.88) * (uFlash * 2.4);

  vec3 col = albedo * (total * aoF);

  float vd = length(vW - uCam);
  float f = uFogDensity * vd;
  float fog = clamp(1.0 - exp(-f * f), 0.0, 1.0);
  col = mix(col, uFogColor, fog);
  col = pow(clamp(col * uExposure, 0.0, 1.0), vec3(1.0 / 2.2));
  float alpha = uBlendAlpha;
  if (vMat == 23) alpha = 0.659;   // water168/255
  Frag = vec4(col, alpha);
})";

static const char* VS_SKY = R"(#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vNdc;
void main(){ vNdc = aPos; gl_Position = vec4(aPos, 0.9999, 1.0); })";

static const char* FS_SKY = R"(#version 330 core
in vec2 vNdc;
out vec4 Frag;
uniform mat4 uInvVP;
uniform vec3 uCam, uZenith, uHorizon, uFogColor, uSunColor, uSunDir;
uniform float uSunInt, uFogDensity, uExposure, uFlash, uWeather;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453); }
float vnoise(vec2 p){
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i), b = hash(i + vec2(1,0)), c = hash(i + vec2(0,1)), d = hash(i + vec2(1,1));
  return mix(mix(a,b,f.x), mix(c,d,f.x), f.y);
}
float fbm(vec2 p){
  float s = 0.0, a = 0.5;
  for (int i = 0; i < 4; i++) { s += a * vnoise(p); p *= 2.03; a *= 0.5; }
  return s;
}
void main(){
  vec4 far = uInvVP * vec4(vNdc, 1.0, 1.0);
  vec3 rd = normalize(far.xyz / far.w - uCam);
  float t = clamp(rd.y * 1.15, 0.0, 1.0);
  float grad = pow(t, 0.62);
  vec3 col = mix(uHorizon, uZenith, grad);
  vec3 sunT = -uSunDir;
  float sd = dot(rd, sunT);
  float disc = smoothstep(0.9965, 0.9993, sd);
  float glow = pow(max(0.0, sd), 36.0) * 0.35;
  col += uSunColor * (disc * 1.6 + glow) * uSunInt;
  if (rd.y > 0.02 && uWeather < 3.5) {
    float cu = (rd.x / rd.y) * 0.06, cv = (rd.z / rd.y) * 0.06;
    float n = fbm(vec2(cu + uWeather * 3.0 + 42.0, cv));
    float cover = clamp((n - (uWeather > 0.5 ? 0.35 : 0.55)) * 3.0, 0.0, 1.0);
    cover *= clamp(rd.y * 4.0, 0.0, 1.0);
    float dim = uWeather > 2.5 ? 0.35 : uWeather > 1.5 ? 0.5 : uWeather > 0.5 ? 0.62 : 0.85;
    vec3 cloudC = mix(uHorizon * 1.06, vec3(1.05,1.04,1.02) * (0.4 + dim * 0.7), n);
    if (uWeather > 2.5) cloudC *= 0.55;
    col = mix(col, cloudC, cover * 0.9);
  }
  float haze = clamp((1.0 - rd.y * 2.4) * uFogDensity * 70.0, 0.0, 0.92);
  col = mix(col, uFogColor, haze);
  col += vec3(1.0, 0.97, 0.9) * (uFlash * 1.5);
  col = pow(clamp(col * uExposure, 0.0, 1.0), vec3(1.0 / 2.2));
  Frag = vec4(col, 1.0);
})";

static const char* VS_UI = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUv;
layout(location=2) in vec4 aCol;
uniform vec2 uScreen;
out vec2 vUv; out vec4 vCol;
void main(){
  vUv = aUv; vCol = aCol;
  vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
})";

static const char* FS_UI = R"(#version 330 core
in vec2 vUv; in vec4 vCol;
out vec4 Frag;
uniform sampler2D uTex;
uniform float uUseTex;
void main(){
  float a = vCol.a;
  if (uUseTex > 0.5) a *= texture(uTex, vUv).r;
  if (a <= 0.004) discard;
  Frag = vec4(vCol.rgb, a);
})";

static const char* VS_PART = R"(#version 330 core
layout(location=0) in vec3 aCenter;
layout(location=1) in vec2 aCorner;
layout(location=2) in vec4 aSizeRot;
layout(location=3) in vec4 aColor;
layout(location=4) in vec3 aFrame;
uniform mat4 uVP;
uniform vec3 uRight, uUp, uCam;
out vec2 vUv; out vec4 vCol; out float vDist;
void main(){
  float c = cos(aSizeRot.y), s = sin(aSizeRot.y);
  vec2 cr = vec2(aCorner.x * c - aCorner.y * s, aCorner.x * s + aCorner.y * c);
  vec3 wp = aCenter + uRight * (cr.x * aSizeRot.x) + uUp * (cr.y * aSizeRot.x);
  vDist = length(wp - uCam);
  gl_Position = uVP * vec4(wp, 1.0);
  vec2 uv = aCorner + 0.5;
  float col4 = mod(aFrame.x, 4.0), row4 = floor(aFrame.x / 4.0);
  vUv = vec2((col4 + uv.x) * 0.25, (row4 + (1.0 - uv.y)) * 0.25);
  vCol = aColor;
})";

// particles fade by visibility multiply (matches SW: color * vis), then exposure+gamma
static const char* FS_PART = R"(#version 330 core
in vec2 vUv; in vec4 vCol; in float vDist;
out vec4 Frag;
uniform sampler2D uTex;
uniform vec3 uCam, uFogColor;
uniform float uFogDensity, uExposure;
void main(){
  vec4 t = texture(uTex, vUv);
  float a = t.a * vCol.a;
  if (a <= 0.004) discard;
  float f = uFogDensity * vDist;
  float vis = 1.0 - clamp(1.0 - exp(-f * f), 0.0, 1.0);
  vec3 col = t.rgb * vCol.rgb * vis;
  col = pow(clamp(col * uExposure, 0.0, 1.0), vec3(1.0 / 2.2));
  Frag = vec4(col, a);
})";

static const char* VS_SHADOW = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uLightVP, uModel;
void main(){ gl_Position = uLightVP * uModel * vec4(aPos, 1.0); })";

static const char* FS_SHADOW = R"(#version 330 core
void main(){ })";

// ---------------------------------------------------------------- backend ----
class GLBackend : public IBackend {
public:
    const char* name() const override { return "gl33"; }
    bool init(void* window, int w, int h, int msaa, bool vsync) override {
        (void)window; (void)msaa; (void)vsync;
        wW = w; wH = h;
        if (!loadGL()) return false;
        logf("GL renderer: %s (%s)", (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
        if (!buildPrograms()) return false;
        if (!buildTextures()) return false;
        // shadow depth FBO
        glGenTextures(1, &shadowTex_);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 2048, 2048, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &shadowFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            errf("shadow FBO incomplete");
            shadowOn_ = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // dynamic buffers
        glGenVertexArrays(1, &uiVao_);
        glGenBuffers(1, &uiVbo_);
        glBindVertexArray(uiVao_);
        glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, 0, 32, (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, 0, 32, (void*)8);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, 0, 32, (void*)16);
        glGenVertexArrays(1, &partVao_);
        glGenBuffers(1, &partVbo_);
        glBindVertexArray(partVao_);
        glBindBuffer(GL_ARRAY_BUFFER, partVbo_);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, 0, 64, (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, 0, 64, (void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, 0, 64, (void*)20);
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, 0, 64, (void*)36);
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_FLOAT, 0, 64, (void*)52);
        glGenVertexArrays(1, &skyVao_);
        glGenBuffers(1, &skyVbo_);
        glBindVertexArray(skyVao_);
        glBindBuffer(GL_ARRAY_BUFFER, skyVbo_);
        f32 tri[6] = {-1, -1, 3, -1, -1, 3};
        glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, 0, 8, (void*)0);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);  // SW-independent safety: both windings visible
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        ok_ = true;
        return true;
    }

    void shutdown() override {
        if (!ok_) return;
        for (auto& kv : chunkRec_) { destroyRec(kv.second.solid); destroyRec(kv.second.water); }
        for (auto& kv : meshRec_) { destroyRec(kv.second); }
        if (uiVbo_) glDeleteBuffers(1, &uiVbo_);
        if (uiVao_) glDeleteVertexArrays(1, &uiVao_);
        if (partVbo_) glDeleteBuffers(1, &partVbo_);
        if (partVao_) glDeleteVertexArrays(1, &partVao_);
        if (skyVbo_) glDeleteBuffers(1, &skyVbo_);
        if (skyVao_) glDeleteVertexArrays(1, &skyVao_);
        if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
        if (shadowTex_) glDeleteTextures(1, &shadowTex_);
        GLuint progs[6] = {progVoxel_, progSky_, progUi_, progPart_, progShadow_};
        for (GLuint p : progs) if (p) glDeleteProgram(p);
        if (texAtlas_) glDeleteTextures(1, &texAtlas_);
        if (texFont_) glDeleteTextures(1, &texFont_);
        if (texSprite_) glDeleteTextures(1, &texSprite_);
        ok_ = false;
    }

    void beginScene(const Camera& c, f32 aspect, const SkyParams& s,
                    const std::vector<LightInfo>& pl, const SpotInfo& sp) override {
        cam_ = c;
        aspect_ = aspect;
        sky_ = s;
        lights_ = pl;
        spot_ = sp;
        vp_ = c.proj(aspect) * c.view();
        invVP_ = vp_.inverse();
        draws_.clear();
        waterDraws_.clear();
        // sun shadow matrix
        Vec3 sunL = norm(-s.sunDir);
        Vec3 center = cam_.pos + cam_.fwd * 26.f;
        center.y = tclamp(center.y, 0.f, 48.f);
        Vec3 lp = center - sunL * 95.f;
        Vec3 up = std::fabs(sunL.y) > 0.95f ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
        Mat4 lview = Mat4::lookDir(lp, norm(center - lp), up);
        f32 ext = 48.f;
        lightVP_ = Mat4::ortho(-ext, ext, -ext, ext, 1.f, 220.f) * lview;
        shadowValid_ = shadowOn_ && s.sunIntensity > 0.15f;
    }

    void drawChunk(const VoxWorld& w, u32 ci) override {
        worldRef_ = &w;
        const ChunkMesh& m = w.meshes[ci];
        if (m.idx.empty() && m.tidx.empty()) return;
        if (!m.idx.empty()) draws_.push_back({0, ci, 0, Mat4(), 0});
        if (!m.tidx.empty()) waterDraws_.push_back({0, ci, 0, Mat4(), 1});
    }
    void drawMesh(u32 h, const Mat4& xform, f32 tr, f32 tg, f32 tb, u8 flags) override {
        draws_.push_back({1, 0, h, xform, flags, tr, tg, tb});
    }
    u32 createMesh(const std::vector<u8>& verts, const std::vector<u32>& idx) override {
        u32 h = nextHandle_++;
        meshRec_[h] = GLRec();
        pending_[h] = {verts, idx};
        return h;
    }
    void updateMesh(u32 handle, const std::vector<u8>& verts, const std::vector<u32>& idx) override {
        if (!meshRec_.count(handle)) return;
        pending_[handle] = {verts, idx};
    }
    void destroyMesh(u32 handle) override {
        auto it = meshRec_.find(handle);
        if (it != meshRec_.end()) { destroyRec(it->second); meshRec_.erase(it); }
        pending_.erase(handle);
    }

    void renderScene() override {
        if (!ok_) return;
        uploadPending();
        // ---- shadow pass ----
        if (shadowValid_) {
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
            glViewport(0, 0, 2048, 2048);
            glClear(GL_DEPTH_BUFFER_BIT);
            glUseProgram(progShadow_);
            glUniformMatrix4fv(uShadow_lightVP_, 1, 0, lightVP_.m);
            for (auto& d : draws_) {
                if (d.kind == 0) {
                    uploadChunkIfNeeded(d.chunk);
                    auto it = chunkRec_.find(d.chunk);
                    if (it == chunkRec_.end() || !it->second.solid.vao) continue;
                    Mat4 model = chunkModel(d.chunk);
                    glUniformMatrix4fv(uShadow_model_, 1, 0, model.m);
                    glBindVertexArray(it->second.solid.vao);
                    glDrawElements(GL_TRIANGLES, it->second.solid.count, GL_UNSIGNED_INT, nullptr);
                } else {
                    auto it = meshRec_.find(d.handle);
                    if (it == meshRec_.end() || !it->second.vao) continue;
                    glUniformMatrix4fv(uShadow_model_, 1, 0, d.xform.m);
                    glBindVertexArray(it->second.vao);
                    glDrawElements(GL_TRIANGLES, it->second.count, GL_UNSIGNED_INT, nullptr);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, wW, wH);
        }
        // ---- main ----
        glClearColor(sky_.fogColor.x, sky_.fogColor.y, sky_.fogColor.z, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawSkyNow();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(1);
        glDisable(GL_BLEND);
        drawVoxels(draws_, 1.0f, false);
        if (!waterDraws_.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(0);
            drawVoxels(waterDraws_, 0.659f, true);
            glDepthMask(1);
            glDisable(GL_BLEND);
        }
        drawParticlesNow();
    }

    void beginUI() override { ui_.clear(); }
    void pushRect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a) override {
        UIQuad q{};
        q.x = x; q.y = y; q.w = w; q.h = h;
        q.u0 = q.v0 = 0; q.u1 = q.v1 = 1;
        q.r = r; q.g = g; q.b = b; q.a = a; q.tex = 0;
        ui_.push_back(q);
    }
    void pushQuad(const UIQuad& q) override { ui_.push_back(q); }
    void drawParticles(const PInst* parts, int n) override {
        parts_.assign(parts, parts + n);
    }
    void renderUI() override {
        if (!ok_ || ui_.empty()) return;
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        std::vector<f32> solid, text;
        solid.reserve(ui_.size() * 48);
        text.reserve(ui_.size() * 48);
        auto emit = [](std::vector<f32>& out, const UIQuad& q, f32 u0, f32 v0, f32 u1, f32 v1) {
            f32 x0 = q.x, y0 = q.y, x1 = q.x + q.w, y1 = q.y + q.h;
            f32 cc[4] = {q.r / 255.f, q.g / 255.f, q.b / 255.f, q.a / 255.f};
            auto pv = [&](f32 x, f32 y, f32 u, f32 v) {
                out.push_back(x); out.push_back(y); out.push_back(u); out.push_back(v);
                out.push_back(cc[0]); out.push_back(cc[1]); out.push_back(cc[2]); out.push_back(cc[3]);
            };
            pv(x0, y0, u0, v0); pv(x1, y0, u1, v0); pv(x1, y1, u1, v1);
            pv(x0, y0, u0, v0); pv(x1, y1, u1, v1); pv(x0, y1, u0, v1);
        };
        for (auto& q : ui_) {
            if (q.tex == 1) emit(text, q, q.u0, q.v0, q.u1, q.v1);
            else emit(solid, q, 0, 0, 1, 1);
        }
        glUseProgram(progUi_);
        glUniform2f(uUi_screen_, (f32)wW, (f32)wH);
        glBindVertexArray(uiVao_);
        glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
        auto batch = [&](std::vector<f32>& v, f32 useTex) {
            if (v.empty()) return;
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(f32)), v.data(), GL_STREAM_DRAW);
            glUniform1f(uUi_useTex_, useTex);
            if (useTex > 0.5f) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texFont_);
                glUniform1i(uUi_tex_, 0);
            }
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 8));
        };
        batch(solid, 0.f);
        batch(text, 1.f);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    void present() override { /* platform swaps buffers */ }
    void readPixels(std::vector<u8>& rgb) override {
        rgb.assign((size_t)wW * wH * 3, 0);
        if (!ok_) return;
        std::vector<u8> tmp(rgb.size());
        glReadPixels(0, 0, wW, wH, GL_RGB, GL_UNSIGNED_BYTE, tmp.data());
        for (int y = 0; y < wH; y++)
            std::memcpy(rgb.data() + (size_t)y * wW * 3,
                        tmp.data() + (size_t)(wH - 1 - y) * wW * 3, (size_t)wW * 3);
    }

private:
    struct GLRec {
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLsizei count = 0;
    };
    struct ChunkGL {
        GLRec solid;
        GLRec water;
        u32 version = 0xFFFFFFFFu;
        bool built = false;
    };
    struct Draw {
        int kind;        // 0 chunk, 1 mesh
        u32 chunk = 0;
        u32 handle = 0;
        Mat4 xform{};
        u8 blend = 0;
        f32 tr = 1, tg = 1, tb = 1;
    };

    GLuint compile(GLenum type, const char* src, const char* label) {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            errf("shader %s failed:\n%s", label, log);
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    }
    GLuint link(const char* vs, const char* fs, const char* label) {
        GLuint v = compile(GL_VERTEX_SHADER, vs, label);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs, label);
        if (!v || !f) return 0;
        GLuint p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        glDeleteShader(v);
        glDeleteShader(f);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetProgramInfoLog(p, sizeof(log), nullptr, log);
            errf("program %s link failed:\n%s", label, log);
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }

    bool buildPrograms() {
        progVoxel_ = link(VS_VOXEL, FS_VOXEL, "voxel");
        progSky_ = link(VS_SKY, FS_SKY, "sky");
        progUi_ = link(VS_UI, FS_UI, "ui");
        progPart_ = link(VS_PART, FS_PART, "particle");
        progShadow_ = link(VS_SHADOW, FS_SHADOW, "shadow");
        if (!progVoxel_ || !progSky_ || !progUi_ || !progPart_ || !progShadow_) return false;
        uUi_screen_ = glGetUniformLocation(progUi_, "uScreen");
        uUi_useTex_ = glGetUniformLocation(progUi_, "uUseTex");
        uUi_tex_ = glGetUniformLocation(progUi_, "uTex");
        uShadow_lightVP_ = glGetUniformLocation(progShadow_, "uLightVP");
        uShadow_model_ = glGetUniformLocation(progShadow_, "uModel");
        return true;
    }

    bool buildTextures() {
        // albedo atlas
        const TexAtlas& at = texAtlas();
        glGenTextures(1, &texAtlas_);
        glBindTexture(GL_TEXTURE_2D, texAtlas_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, at.size(), at.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, at.albedo.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // font atlas (alpha8 → R8)
        glGenTextures(1, &texFont_);
        glBindTexture(GL_TEXTURE_2D, texFont_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FT_FONT_W, FT_FONT_H, 0, GL_RED, GL_UNSIGNED_BYTE, FT_FONT_DATA);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // particle sprites
        const SpriteSheet& sh = spriteSheet();
        glGenTextures(1, &texSprite_);
        glBindTexture(GL_TEXTURE_2D, texSprite_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sh.w, sh.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sh.rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return texAtlas_ && texFont_ && texSprite_;
    }

    Mat4 chunkModel(u32 ci) const {
        int cx = (int)(ci % (u32)worldRef_->CW);
        int cy = (int)((ci / (u32)worldRef_->CW) % (u32)worldRef_->CHh);
        int cz = (int)(ci / ((u32)worldRef_->CW * (u32)worldRef_->CHh));
        return Mat4::translate(Vec3(cx * CH * VOX, cy * CH * VOX, cz * CH * VOX));
    }

    void destroyRec(GLRec& r) {
        if (r.vbo) glDeleteBuffers(1, &r.vbo);
        if (r.ebo) glDeleteBuffers(1, &r.ebo);
        if (r.vao) glDeleteVertexArrays(1, &r.vao);
        r = GLRec();
    }

    static void bindMeshAttrs() {
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(MVert), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, 0, sizeof(MVert), (void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, 1, sizeof(MVert), (void*)24);
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, 1, sizeof(MVert), (void*)27);
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_UNSIGNED_BYTE, 0, sizeof(MVert), (void*)28);
        glEnableVertexAttribArray(5); glVertexAttribPointer(5, 1, GL_UNSIGNED_BYTE, 0, sizeof(MVert), (void*)29);
    }

    void uploadRec(GLRec& r, const u8* verts, size_t vb, const u32* idx, size_t ic) {
        if (!r.vao) {
            glGenVertexArrays(1, &r.vao);
            glGenBuffers(1, &r.vbo);
            glGenBuffers(1, &r.ebo);
        }
        glBindVertexArray(r.vao);
        glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vb, verts, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(ic * sizeof(u32)), idx, GL_DYNAMIC_DRAW);
        bindMeshAttrs();
        glBindVertexArray(0);
        r.count = (GLsizei)ic;
    }

    void uploadChunkIfNeeded(u32 ci) {
        if (!worldRef_) return;
        const ChunkMesh& m = worldRef_->meshes[ci];
        ChunkGL& r = chunkRec_[ci];
        if (r.built && r.version == m.version) return;
        if (!m.idx.empty()) {
            uploadRec(r.solid, m.verts.data(), m.verts.size(), m.idx.data(), m.idx.size());
        } else if (r.solid.vao) {
            destroyRec(r.solid);
        }
        if (!m.tidx.empty()) {
            uploadRec(r.water, m.tverts.data(), m.tverts.size(), m.tidx.data(), m.tidx.size());
        } else if (r.water.vao) {
            destroyRec(r.water);
        }
        r.version = m.version;
        r.built = true;
    }

    void uploadPending() {
        if (pending_.empty()) return;
        for (auto& kv : pending_) {
            auto it = meshRec_.find(kv.first);
            if (it == meshRec_.end()) continue;
            if (kv.second.second.empty()) {
                if (it->second.vao) destroyRec(it->second);
                continue;
            }
            uploadRec(it->second, kv.second.first.data(), kv.second.first.size(),
                      kv.second.second.data(), kv.second.second.size());
        }
        pending_.clear();
    }

    void setVoxelUniforms(GLfloat blendAlpha) {
        GLuint p = progVoxel_;
        glUseProgram(p);
        auto U = [&](const char* n) { return glGetUniformLocation(p, n); };
        uModelLoc_ = U("uModel");
        uTintLoc_ = U("uTint");
        glUniform3f(uTintLoc_, 1.f, 1.f, 1.f);
        glUniformMatrix4fv(U("uVP"), 1, 0, vp_.m);
        glUniformMatrix4fv(U("uLightVP"), 1, 0, lightVP_.m);
        glUniform3f(U("uCam"), cam_.pos.x, cam_.pos.y, cam_.pos.z);
        glUniform3f(U("uSunDir"), sky_.sunDir.x, sky_.sunDir.y, sky_.sunDir.z);
        glUniform3f(U("uSunColor"), sky_.sunColor.x, sky_.sunColor.y, sky_.sunColor.z);
        glUniform1f(U("uSunInt"), sky_.sunIntensity);
        glUniform1f(U("uSkyAmb"), sky_.ambient);
        glUniform3f(U("uZenith"), sky_.zenith.x, sky_.zenith.y, sky_.zenith.z);
        glUniform3f(U("uHorizon"), sky_.horizon.x, sky_.horizon.y, sky_.horizon.z);
        glUniform3f(U("uFogColor"), sky_.fogColor.x, sky_.fogColor.y, sky_.fogColor.z);
        glUniform1f(U("uFogDensity"), sky_.fogDensity);
        glUniform1f(U("uExposure"), sky_.exposure);
        glUniform1f(U("uFlash"), sky_.flash / 255.f);
        glUniform1f(U("uShadowOn"), shadowValid_ ? 1.f : 0.f);
        glUniform1f(U("uBlendAlpha"), blendAlpha);
        static f32 matArr[64 * 4], slotArr[64 * 4];
        const MatGpu* mt = matGpuTable();
        const int* bs = matSlotBaseTable();
        for (int i = 0; i < 64; i++) {
            matArr[i * 4 + 0] = mt[i].rough;
            matArr[i * 4 + 1] = mt[i].metal;
            matArr[i * 4 + 2] = mt[i].uvScale;
            matArr[i * 4 + 3] = 0;
            slotArr[i * 4 + 0] = (i < MAT_COUNT) ? (f32)bs[i] : 0.f;
            slotArr[i * 4 + 1] = slotArr[i * 4 + 2] = slotArr[i * 4 + 3] = 0;
        }
        glUniform4fv(U("uMat"), 64, matArr);
        glUniform4fv(U("uSlot"), 64, slotArr);
        f32 pos[8 * 4] = {0}, col[8 * 4] = {0};
        int n = tmin(8, (int)lights_.size());
        for (int i = 0; i < n; i++) {
            pos[i * 4 + 0] = lights_[i].pos.x; pos[i * 4 + 1] = lights_[i].pos.y;
            pos[i * 4 + 2] = lights_[i].pos.z; pos[i * 4 + 3] = lights_[i].radius;
            col[i * 4 + 0] = lights_[i].color.x; col[i * 4 + 1] = lights_[i].color.y;
            col[i * 4 + 2] = lights_[i].color.z; col[i * 4 + 3] = lights_[i].intensity;
        }
        glUniform4fv(U("uPLpos"), 8, pos);
        glUniform4fv(U("uPLcol"), 8, col);
        glUniform1i(U("uPLn"), n);
        glUniform4f(U("uSpotPos"), spot_.pos.x, spot_.pos.y, spot_.pos.z, spot_.range);
        glUniform4f(U("uSpotDir"), spot_.dir.x, spot_.dir.y, spot_.dir.z, 0);
        glUniform4f(U("uSpotCol"), spot_.color.x, spot_.color.y, spot_.color.z, spot_.intensity);
        glUniform1f(U("uSpotCos"), spot_.angleCos);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texAtlas_);
        glUniform1i(U("uAtlas"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        glUniform1i(U("uShadow"), 1);
    }

    void drawVoxels(std::vector<Draw>& list, f32 blendAlpha, bool isWater) {
        if (list.empty()) return;
        setVoxelUniforms(blendAlpha);
        for (auto& d : list) {
            if (d.kind == 0) {
                uploadChunkIfNeeded(d.chunk);
                auto it = chunkRec_.find(d.chunk);
                if (it == chunkRec_.end()) continue;
                GLRec& rec = isWater ? it->second.water : it->second.solid;
                if (!rec.vao || rec.count <= 0) continue;
                Mat4 model = chunkModel(d.chunk);
                glUniformMatrix4fv(uModelLoc_, 1, 0, model.m);
                glUniform3f(uTintLoc_, 1.f, 1.f, 1.f);
                glBindVertexArray(rec.vao);
                glDrawElements(GL_TRIANGLES, rec.count, GL_UNSIGNED_INT, nullptr);
            } else {
                auto it = meshRec_.find(d.handle);
                if (it == meshRec_.end() || !it->second.vao) continue;
                glUniformMatrix4fv(uModelLoc_, 1, 0, d.xform.m);
                glUniform3f(uTintLoc_, d.tr, d.tg, d.tb);
                glBindVertexArray(it->second.vao);
                glDrawElements(GL_TRIANGLES, it->second.count, GL_UNSIGNED_INT, nullptr);
            }
        }
        glBindVertexArray(0);
    }

    void drawSkyNow() {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(0);
        glUseProgram(progSky_);
        glUniformMatrix4fv(glGetUniformLocation(progSky_, "uInvVP"), 1, 0, invVP_.m);
        auto U = [&](const char* n) { return glGetUniformLocation(progSky_, n); };
        glUniform3f(U("uCam"), cam_.pos.x, cam_.pos.y, cam_.pos.z);
        glUniform3f(U("uZenith"), sky_.zenith.x, sky_.zenith.y, sky_.zenith.z);
        glUniform3f(U("uHorizon"), sky_.horizon.x, sky_.horizon.y, sky_.horizon.z);
        glUniform3f(U("uFogColor"), sky_.fogColor.x, sky_.fogColor.y, sky_.fogColor.z);
        glUniform3f(U("uSunColor"), sky_.sunColor.x, sky_.sunColor.y, sky_.sunColor.z);
        glUniform3f(U("uSunDir"), sky_.sunDir.x, sky_.sunDir.y, sky_.sunDir.z);
        glUniform1f(U("uSunInt"), sky_.sunIntensity);
        glUniform1f(U("uFogDensity"), sky_.fogDensity);
        glUniform1f(U("uExposure"), sky_.exposure);
        glUniform1f(U("uFlash"), sky_.flash / 255.f);
        glUniform1f(U("uWeather"), (f32)sky_.weather);
        glBindVertexArray(skyVao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthMask(1);
        glEnable(GL_DEPTH_TEST);
    }

    void drawParticlesNow() {
        if (parts_.empty()) return;
        // back-to-front
        std::vector<std::pair<f32, int>> order;
        order.reserve(parts_.size());
        for (int i = 0; i < (int)parts_.size(); i++) {
            Vec3 dp(parts_[i].x - cam_.pos.x, parts_[i].y - cam_.pos.y, parts_[i].z - cam_.pos.z);
            order.push_back({-len(dp), i});
        }
        std::sort(order.begin(), order.end());
        std::vector<f32> verts;
        verts.reserve(order.size() * 6 * 16);
        Vec3 right = cam_.right();
        Vec3 up = cam_.up;
        for (auto& o : order) {
            const PInst& p = parts_[o.second];
            const f32 corners[4][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
            const int tri[6] = {0, 1, 2, 0, 2, 3};
            f32 fr = p.frame;
            f32 alpha = p.a / 255.f;
            f32 base[16];
            for (int k = 0; k < 6; k++) {
                int c = tri[k];
                // rotated size handled in shader (aSizeRot.y) — pass raw corners
                base[c * 16 + 0] = p.x; base[c * 16 + 1] = p.y; base[c * 16 + 2] = p.z;
                base[c * 16 + 3] = corners[c][0]; base[c * 16 + 4] = corners[c][1];
                base[c * 16 + 5] = p.size; base[c * 16 + 6] = p.rot; base[c * 16 + 7] = 0; base[c * 16 + 8] = 0;
                base[c * 16 + 9] = p.r; base[c * 16 + 10] = p.g; base[c * 16 + 11] = p.b; base[c * 16 + 12] = alpha;
                base[c * 16 + 13] = fr; base[c * 16 + 14] = (f32)p.kind; base[c * 16 + 15] = 0;
            }
            for (int k = 0; k < 6; k++) {
                int c = tri[k];
                for (int f = 0; f < 16; f++) verts.push_back(base[c * 16 + f]);
            }
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(0);
        glUseProgram(progPart_);
        glUniformMatrix4fv(glGetUniformLocation(progPart_, "uVP"), 1, 0, vp_.m);
        glUniform3f(glGetUniformLocation(progPart_, "uRight"), right.x, right.y, right.z);
        glUniform3f(glGetUniformLocation(progPart_, "uUp"), up.x, up.y, up.z);
        glUniform3f(glGetUniformLocation(progPart_, "uCam"), cam_.pos.x, cam_.pos.y, cam_.pos.z);
        glUniform3f(glGetUniformLocation(progPart_, "uFogColor"), sky_.fogColor.x, sky_.fogColor.y, sky_.fogColor.z);
        glUniform1f(glGetUniformLocation(progPart_, "uFogDensity"), sky_.fogDensity);
        glUniform1f(glGetUniformLocation(progPart_, "uExposure"), sky_.exposure);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texSprite_);
        glUniform1i(glGetUniformLocation(progPart_, "uTex"), 0);
        glBindVertexArray(partVao_);
        glBindBuffer(GL_ARRAY_BUFFER, partVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(f32)), verts.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 16));
        glBindVertexArray(0);
        glDepthMask(1);
        glDisable(GL_BLEND);
    }

    bool ok_ = false;
    bool shadowOn_ = true;
    bool shadowValid_ = false;
    int wW = 0, wH = 0;
    f32 aspect_ = 1.f;
    Camera cam_;
    SkyParams sky_;
    std::vector<LightInfo> lights_;
    SpotInfo spot_;
    Mat4 vp_, invVP_, lightVP_;
    const VoxWorld* worldRef_ = nullptr;
    std::vector<Draw> draws_, waterDraws_;
    std::vector<UIQuad> ui_;
    std::vector<PInst> parts_;

    std::unordered_map<u32, ChunkGL> chunkRec_;
    std::unordered_map<u32, GLRec> meshRec_;
    std::unordered_map<u32, std::pair<std::vector<u8>, std::vector<u32>>> pending_;
    u32 nextHandle_ = 1;

    GLuint progVoxel_ = 0, progSky_ = 0, progUi_ = 0, progPart_ = 0, progShadow_ = 0;
    GLuint texAtlas_ = 0, texFont_ = 0, texSprite_ = 0;
    GLuint shadowTex_ = 0, shadowFbo_ = 0;
    GLuint uiVao_ = 0, uiVbo_ = 0, partVao_ = 0, partVbo_ = 0, skyVao_ = 0, skyVbo_ = 0;
    GLint uUi_screen_ = -1, uUi_useTex_ = -1, uUi_tex_ = -1;
    GLint uModelLoc_ = -1, uTintLoc_ = -1;
    GLint uShadow_lightVP_ = -1, uShadow_model_ = -1;
};

IBackend* createGLBackend() { return new GLBackend(); }

} // namespace ft
