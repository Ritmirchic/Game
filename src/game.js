/* MEMORIA — renderer + gameplay. Pure WebGL, zero dependencies. */
(function () {
  'use strict';

  var CFG = MEM.CFG, City = MEM.City;

  /* =======================================================================
     GLSL
     ======================================================================= */

  var PREC = [
    '#ifdef GL_FRAGMENT_PRECISION_HIGH',
    'precision highp float;',
    '#else',
    'precision mediump float;',
    '#endif'
  ].join('\n');

  var BAYER = [
    'float bayer2(vec2 a){ a = floor(a); return fract(a.x * 0.5 + a.y * a.y * 0.75); }',
    'float bayer4(vec2 a){ return bayer2(a * 0.5) * 0.25 + bayer2(a); }',
    'float hash21(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }'
  ].join('\n');

  var SKY_VS = `
attribute vec2 aPos;
varying vec2 vNdc;
void main(){ vNdc = aPos; gl_Position = vec4(aPos, 0.9999, 1.0); }
`;

  var SKY_FS = PREC + `
varying vec2 vNdc;
uniform mat4 uInvVP;
uniform vec3 uCamPos;
uniform float uTime;
uniform vec3 uFog;

float h13(vec3 p){ p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419)); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }

void main(){
  vec4 far = uInvVP * vec4(vNdc, 1.0, 1.0);
  vec3 dir = normalize(far.xyz / far.w - uCamPos);
  float h = dir.y;

  vec3 zenith = vec3(0.004, 0.009, 0.028);
  vec3 mid = vec3(0.012, 0.030, 0.062);
  vec3 horizon = vec3(0.035, 0.085, 0.115);
  vec3 col = mix(horizon, mid, smoothstep(0.0, 0.28, h));
  col = mix(col, zenith, smoothstep(0.22, 0.85, h));

  // glow of the sleeping city
  col += vec3(0.03, 0.10, 0.13) * pow(clamp(1.0 - abs(h) * 3.2, 0.0, 1.0), 2.0);

  // aurora
  float az = atan(dir.z, dir.x);
  float band = smoothstep(0.02, 0.35, h) * smoothstep(0.95, 0.4, h);
  float aur = (sin(az * 3.0 + uTime * 0.045) * 0.5 + 0.5) * (sin(az * 7.0 - uTime * 0.08) * 0.5 + 0.5);
  col += vec3(0.055, 0.30, 0.36) * band * aur * 0.55;
  col += vec3(0.16, 0.10, 0.34) * band * pow(aur, 3.0) * 0.7;

  // stars
  vec3 sp = dir * 120.0;
  vec3 id = floor(sp);
  vec3 f = fract(sp) - 0.5;
  float rnd = h13(id);
  float star = smoothstep(0.22, 0.0, dot(f, f)) * step(0.955, rnd);
  star *= smoothstep(-0.02, 0.25, h);
  float tw = 0.6 + 0.4 * sin(uTime * 2.0 + rnd * 40.0);
  col += vec3(0.75, 0.88, 1.0) * star * (0.4 + 0.9 * rnd) * tw;

  // moon
  vec3 moonDir = normalize(vec3(-0.42, 0.55, -0.72));
  float md = max(dot(dir, moonDir), 0.0);
  col += vec3(0.55, 0.72, 1.0) * pow(md, 900.0) * 3.2;
  col += vec3(0.20, 0.32, 0.55) * pow(md, 14.0) * 0.34;

  // horizon haze matching the fog
  col = mix(col, uFog, smoothstep(0.16, -0.05, h) * 0.85);

  col += (h13(vec3(gl_FragCoord.xy, uTime)) - 0.5) * 0.012;
  gl_FragColor = vec4(col, 1.0);
}
`;

  var WORLD_VS = `
attribute vec3 aPos;
attribute vec3 aNor;
attribute vec3 aCol;
attribute float aGlow;
attribute float aPres;
attribute float aClip;
attribute float aFlash;
uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform float uMirror;
uniform float uTime;
varying vec3 vWorld;
varying vec3 vNor;
varying vec3 vCol;
varying float vGlow;
varying float vPres;
varying float vClip;
varying float vFlash;
varying float vDist;
varying float vSeed;
void main(){
  vec3 wp = aPos;
  vec3 n = aNor;
  if (uMirror > 0.5) { wp.y = -wp.y; n.y = -n.y; }
  vWorld = wp; vNor = n; vCol = aCol; vGlow = aGlow;
  vPres = aPres; vClip = aClip; vFlash = aFlash;
  vDist = length(wp - uCamPos);
  vSeed = aCol.r * 13.0 + aCol.b * 7.0;
  gl_Position = uViewProj * vec4(wp, 1.0);
}
`;

  var WORLD_FS = PREC + `
varying vec3 vWorld;
varying vec3 vNor;
varying vec3 vCol;
varying float vGlow;
varying float vPres;
varying float vClip;
varying float vFlash;
varying float vDist;
varying float vSeed;
uniform vec3 uCamPos;
uniform vec3 uCenter;
uniform vec3 uFog;
uniform float uRadius;
uniform float uTime;
uniform float uMirror;
uniform float uHarmony;
uniform float uPulse;
` + BAYER + `
void main(){
  float oy = mix(vWorld.y, -vWorld.y, uMirror);
  if (oy > vClip) discard;
  if (uMirror > 0.5 && vWorld.y > -0.005) discard;

  float dist = length(vWorld.xz - uCenter.xz);
  float mem = 1.0 - smoothstep(uRadius * 0.34, uRadius * 1.02, dist);
  float cov = 0.020 + 0.98 * mem * pow(vPres, 1.35) * uPulse;
  cov = clamp(cov, 0.0, 1.0);
  cov = mix(cov, 1.0, uHarmony);
  if (cov < 0.999) {
    if (bayer4(gl_FragCoord.xy) > cov) discard;
  }

  vec3 n = normalize(vNor);
  vec3 V = normalize(uCamPos - vWorld);
  float face = 1.0 - abs(n.y);
  vec3 tint = vCol;

  // window grid on vertical faces
  vec2 cell = vec2(abs(n.x) > abs(n.z) ? vWorld.z : vWorld.x, oy) * vec2(0.62, 0.55);
  vec2 f = fract(cell);
  float win = step(0.22, f.x) * step(f.x, 0.86) * step(0.18, f.y) * step(f.y, 0.74);
  float rnd = hash21(floor(cell));
  win *= step(0.40, rnd) * face;

  float lam = 0.42 + 0.58 * max(dot(n, normalize(vec3(0.36, 0.88, 0.30))), 0.0);
  vec3 glass = tint * 0.085 * lam + vec3(0.010, 0.018, 0.032);
  vec3 emis = tint * win * (0.55 + 0.65 * rnd);
  float rim = pow(1.0 - clamp(dot(n, V), 0.0, 1.0), 2.4);
  vec3 col = glass + emis + tint * rim * 0.62;

  // holographic scan
  float scan = 0.5 + 0.5 * sin(oy * 26.0 - uTime * 2.4 + vSeed * 6.0);
  col += tint * face * scan * 0.045;

  // accent glow bands
  float pulse = 0.55 + 0.45 * sin(uTime * 1.7 + oy * 0.55 + vSeed * 3.0);
  col += mix(tint, vec3(1.0), 0.30) * vGlow * (0.55 + pulse * 1.1) * 1.5;

  // birth flash
  col += vec3(0.68, 0.93, 1.0) * vFlash * (0.55 + rim * 1.6);

  // remembering pulse ring travelling outwards
  float pr = length(vWorld - uCenter) - uPulse * 26.0;
  col += vec3(0.25, 0.85, 1.0) * exp(-abs(pr) * 2.2) * step(0.02, uPulse) * 0.45;

  // golden harmony
  col = mix(col, col * vec3(1.30, 1.02, 0.74) + vec3(0.10, 0.07, 0.03), uHarmony * 0.85);

  float fog = 1.0 - exp(-pow(vDist * 0.0125, 1.75));
  col = mix(col, uFog, clamp(fog, 0.0, 1.0));

  gl_FragColor = vec4(col, 1.0);
}
`;

  var LINE_VS = `
attribute vec3 aPos;
attribute vec3 aCol;
attribute float aPres;
attribute float aClip;
attribute float aFlash;
uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform float uMirror;
varying vec3 vWorld;
varying vec3 vCol;
varying float vPres;
varying float vClip;
varying float vFlash;
varying float vDist;
void main(){
  vec3 wp = aPos;
  if (uMirror > 0.5) wp.y = -wp.y;
  vWorld = wp; vCol = aCol; vPres = aPres; vClip = aClip; vFlash = aFlash;
  vDist = length(wp - uCamPos);
  gl_Position = uViewProj * vec4(wp, 1.0);
}
`;

  var LINE_FS = PREC + `
varying vec3 vWorld;
varying vec3 vCol;
varying float vPres;
varying float vClip;
varying float vFlash;
varying float vDist;
uniform vec3 uCenter;
uniform vec3 uFog;
uniform float uRadius;
uniform float uMirror;
uniform float uHarmony;
uniform float uPulse;
uniform float uTime;
` + BAYER + `
void main(){
  float oy = mix(vWorld.y, -vWorld.y, uMirror);
  if (oy > vClip) discard;
  if (uMirror > 0.5 && vWorld.y > -0.005) discard;
  float dist = length(vWorld.xz - uCenter.xz);
  float mem = 1.0 - smoothstep(uRadius * 0.34, uRadius * 1.02, dist);
  float cov = 0.0 + 0.995 * mem * vPres * uPulse;
  cov = mix(cov, 1.0, uHarmony);
  if (bayer4(gl_FragCoord.xy) > cov) discard;

  vec3 col = mix(vCol, vec3(0.86, 0.97, 1.0), 0.42) * (0.85 + 0.35 * sin(uTime * 1.4 + vWorld.y * 0.3));
  col += vec3(0.7, 0.95, 1.0) * vFlash * 2.0;
  col = mix(col, vec3(1.0, 0.86, 0.55), uHarmony * 0.7);

  // remembering pulse ring
  float pr = length(vWorld - uCenter) - uPulse * 26.0;
  col += vec3(0.2, 0.8, 1.0) * exp(-abs(pr) * 2.0) * step(0.02, uPulse) * 1.4;

  float fade = exp(-vDist * 0.0115);
  float alpha = clamp(0.30 * fade * (1.0 - uHarmony * 0.25), 0.0, 1.0);
  gl_FragColor = vec4(col * fade, alpha);
}
`;

  var FLOOR_VS = `
attribute vec3 aPos;
uniform mat4 uViewProj;
varying vec3 vWorld;
void main(){
  vWorld = aPos;
  gl_Position = uViewProj * vec4(aPos, 1.0);
}
`;

  var FLOOR_FS = PREC + `
varying vec3 vWorld;
uniform sampler2D uMirrorTex;
uniform sampler2D uMemTex;
uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform vec3 uCenter;
uniform vec3 uFog;
uniform float uTime;
uniform float uRadius;
uniform float uHarmony;
uniform vec4 uRings[6];
uniform float uRingN;
` + BAYER + `
void main(){
  vec2 wp = vWorld.xz;
  float camDist = length(vWorld - uCamPos);

  // mirror projection of this very point (y = 0 is the mirror plane)
  vec4 mp = uViewProj * vec4(wp.x, 0.0, wp.y, 1.0);
  vec2 uv = (mp.xy / max(mp.w, 0.0001)) * 0.5 + 0.5;

  // ripples from footsteps / pulses
  float wave = 0.0;
  for (int i = 0; i < 6; i++) {
    vec4 r = uRings[i];
    float active = step(float(i), uRingN - 0.5);
    float age = clamp(uTime - r.z, 0.0, 2.7);
    float live = step(0.0, uTime - r.z) * step(uTime - r.z, 2.7) * active;
    float d = length(wp - r.xy);
    float front = age * 15.0;
    float band = exp(-abs(d - front) * 1.35);
    wave += sin((d - front) * 3.4) * band * exp(-age * 1.25) * r.w * live;
  }
  vec2 dir = normalize(wp - uCenter + vec2(0.001));
  uv += dir * wave * 0.0075 * (1.6 / (1.0 + camDist * 0.05));

  vec3 refl = texture2D(uMirrorTex, clamp(uv, vec2(0.001), vec2(0.999))).rgb;

  // memory tiles under the floor
  vec2 muv = (wp + 48.0) / 96.0;
  float rev = texture2D(uMemTex, muv).r;
  float pres = texture2D(uMemTex, muv).g;

  // grid: fine 2m + broad 12m
  vec2 gq = wp * 0.5;
  float aa = 0.012 + camDist * 0.00045;
  vec2 gf = abs(fract(gq) - 0.5);
  float fine = 1.0 - smoothstep(0.0, aa * 2.0, min(gf.x, gf.y));
  vec2 gb = abs(fract(wp / 12.0 + 0.5) - 0.5);
  float broad = 1.0 - smoothstep(0.0, 0.006 + camDist * 0.0006, min(gb.x, gb.y));

  float pd = length(wp - uCenter);
  float halo = exp(-pd * 0.16);

  vec3 col = vec3(0.004, 0.010, 0.020);
  col += refl * 0.92;
  col += vec3(0.02, 0.10, 0.16) * (fine * 0.30 + broad * 0.65) * (0.55 + 0.45 * halo);
  col += vec3(0.02, 0.16, 0.22) * pres * 0.30;
  col += vec3(0.10, 0.42, 0.55) * halo * 0.12;
  col += vec3(0.32, 0.85, 1.0) * abs(wave) * 0.42;
  col += vec3(0.10, 0.35, 0.55) * rev * 0.04;

  col += (bayer4(gl_FragCoord.xy) - 0.5) * 0.010;
  col = mix(col, col * vec3(1.25, 1.0, 0.78) + vec3(0.02, 0.012, 0.0), uHarmony * 0.8);

  float fog = 1.0 - exp(-pow(camDist * 0.010, 1.7));
  col = mix(col, uFog, clamp(fog * 0.92, 0.0, 1.0));
  if (camDist > 240.0) discard;

  gl_FragColor = vec4(col, 1.0);
}
`;

  var POST_VS = `
attribute vec2 aPos;
varying vec2 vUv;
void main(){ vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }
`;

  var POST_FS = PREC + `
varying vec2 vUv;
uniform sampler2D uScene;
uniform vec2 uRes;
uniform float uTime;
uniform float uFade;
uniform float uHarmony;
uniform float uShake;
float h21(vec2 p){ return fract(sin(dot(p, vec2(41.7, 289.3))) * 43758.5453); }
void main(){
  vec2 uv = vUv;
  vec2 c = uv - 0.5;
  float r2 = dot(c, c);

  // barrel-ish chromatic aberration
  float ca = (0.0009 + r2 * 0.004) * (1.0 + uShake * 4.0);
  vec3 col;
  col.r = texture2D(uScene, uv + c * ca).r;
  col.g = texture2D(uScene, uv).g;
  col.b = texture2D(uScene, uv - c * ca).b;

  // cheap glow: wide bright pass
  vec3 glow = vec3(0.0);
  float wsum = 0.0;
  for (int i = 0; i < 6; i++) {
    float a = float(i) * 1.0472;
    vec2 off = vec2(cos(a), sin(a)) * (2.2 / uRes) * (1.0 + float(i) * 0.6);
    vec3 s = texture2D(uScene, uv + off).rgb;
    float b = max(max(s.r, s.g), s.b);
    glow += s * smoothstep(0.55, 1.4, b);
    wsum += 1.0;
  }
  col += glow / wsum * (0.85 + uHarmony * 0.8);

  // scanlines + grain
  float scan = 0.965 + 0.035 * sin(uv.y * uRes.y * 1.6);
  col *= scan;
  float grain = h21(uv * uRes + fract(uTime) * 131.0) - 0.5;
  col += grain * 0.016;

  // vignette
  col *= 1.0 - smoothstep(0.18, 0.72, r2) * 0.62;

  // cool shadows / warm highlights
  col.rgb = mix(col.rgb, col.rgb * vec3(0.92, 1.0, 1.09), 0.35);

  // tone
  col = col / (col + vec3(0.72)) * 1.55;
  col = pow(max(col, 0.0), vec3(0.92));

  col *= uFade;
  col += (h21(uv * uRes) - 0.5) * 0.004;
  gl_FragColor = vec4(col, 1.0);
}
`;

  /* =======================================================================
     boot
     ======================================================================= */

  var canvas = document.getElementById('gl');
  var GLW, gl, city, audio = new MEM.Audio();
  var scene, mirrorT, progWorld, progLine, progSky, progFloor, progPost;
  var quadBuf, floorBuf, fillBuf, lineBuf;
  var fillData, lineData, fillCount = 0, lineCount = 0;
  var FSTRIDE = 13, LSTRIDE = 9;
  var memTex, memDirty = true;

  var state = 'menu';
  var t = 0, dt = 0, last = 0;
  var timeLeft = 300, elapsed = 0;
  var yaw = 0, pitch = 0;
  var pos = { x: 0, y: 0, z: 26 };
  var vel = { x: 0, z: 0, y: 0 };
  var flying = false, sprint = false;
  var lastPulse = -99, lastStep = 0, lastRipple = -99, lastBell = -99;
  var pulseT = 0, lastPulseFired = -99;
  var shake = 0, fade = 0, fadeTarget = 1, harmony = 0;
  var pulsesUsed = 0, distanceWalked = 0;
  var seed = 7;
  var newBuildings = 0, newTimer = 0;
  var subtitlesShown = {};
  var endTimer = 0, endShown = false, won = false;

  var keys = {};
  var hintOn = true;
  var ringArr = new Float32Array(24);

  var el = {
    timer: document.getElementById('timer'),
    bar: document.getElementById('bar'),
    pct: document.getElementById('pct'),
    count: document.getElementById('count'),
    cdArc: document.getElementById('cdArc'),
    cdText: document.getElementById('cdText'),
    posx: document.getElementById('posx'),
    posz: document.getElementById('posz'),
    modeLabel: document.getElementById('modeLabel'),
    hintArrow: document.getElementById('hintArrow'),
    toasts: document.getElementById('toasts'),
    subtitle: document.getElementById('subtitle'),
    hud: document.getElementById('hud'),
    menu: document.getElementById('menu'),
    end: document.getElementById('end'),
    seedLabel: document.getElementById('seedLabel'),
    crosshair: document.getElementById('crosshair')
  };

  function fail(err) {
    var e = document.getElementById('err');
    document.getElementById('errText').textContent = (err && err.message) || String(err);
    e.classList.add('on');
    document.getElementById('loader').classList.add('off');
    console.error(err);
  }

  function init() {
    try {
      GLW = new GL(canvas); gl = GLW.gl;
    } catch (e) { fail(e); return; }
    progWorld = GLW.program(WORLD_VS, WORLD_FS, 'world');
    progLine = GLW.program(LINE_VS, LINE_FS, 'line');
    progSky = GLW.program(SKY_VS, SKY_FS, 'sky');
    progFloor = GLW.program(FLOOR_VS, FLOOR_FS, 'floor');
    progPost = GLW.program(POST_VS, POST_FS, 'post');

    quadBuf = GLW.buffer(new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]));
    var F = 500;
    floorBuf = GLW.buffer(new Float32Array([-F, 0, -F, F, 0, -F, -F, 0, F, -F, 0, F, F, 0, -F, F, 0, F]));

    memTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, memTex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    city = new City(seed);
    city.reveal(0, 26, 13, 0, { ripple: false });
    buildCity();
    resize();

    window.addEventListener('resize', resize);
    bindInput();
    requestAnimationFrame(loop);
    setTimeout(function () { document.getElementById('loader').classList.add('off'); }, 380);
    setSub('Всё, что ты видишь, — то, что ты уже вспомнил.', 5);
  }

  /* =======================================================================
     city -> geometry
     ======================================================================= */

  var CORN = [
    [-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1],
    [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1]
  ];
  var QUADS = [
    [1, 2, 6, 5], [0, 4, 7, 3], [3, 7, 6, 2], [0, 1, 5, 4], [4, 5, 6, 7], [0, 3, 2, 1]
  ];
  var EDGES = [
    [0, 1], [1, 2], [2, 3], [3, 0], [4, 5], [5, 6], [6, 7], [7, 4],
    [0, 4], [1, 5], [2, 6], [3, 7]
  ];

  function emitBox(fOut, lOut, cx, cy, cz, hx, hy, hz, rot, col, glow, pres, clip, flash) {
    var c = Math.cos(rot), s = Math.sin(rot);
    var cxp = new Float32Array(24);
    for (var k = 0; k < 8; k++) {
      var lx = CORN[k][0] * hx, lz = CORN[k][2] * hz;
      cxp[k * 3] = cx + lx * c - lz * s;
      cxp[k * 3 + 1] = cy + CORN[k][1] * hy;
      cxp[k * 3 + 2] = cz + lx * s + lz * c;
    }
    for (var q = 0; q < 6; q++) {
      var Q = QUADS[q];
      var ax = cxp[Q[0] * 3], ay = cxp[Q[0] * 3 + 1], az = cxp[Q[0] * 3 + 2];
      var bx = cxp[Q[1] * 3], by = cxp[Q[1] * 3 + 1], bz = cxp[Q[1] * 3 + 2];
      var dx = cxp[Q[3] * 3], dy = cxp[Q[3] * 3 + 1], dz = cxp[Q[3] * 3 + 2];
      var e1x = bx - ax, e1y = by - ay, e1z = bz - az;
      var e2x = dx - ax, e2y = dy - ay, e2z = dz - az;
      var nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x;
      var nl = Math.hypot(nx, ny, nz) || 1; nx /= nl; ny /= nl; nz /= nl;
      var ox = ax - cx, oy2 = ay - cy, oz = az - cz;
      if (nx * ox + ny * oy2 + nz * oz < 0) { nx = -nx; ny = -ny; nz = -nz; }
      for (var vi = 0; vi < 6; vi++) {
        var idx = Q[vi === 0 ? 0 : vi === 1 ? 1 : vi === 2 ? 2 : vi === 3 ? 0 : vi === 4 ? 2 : 3];
        fOut.push(cxp[idx * 3], cxp[idx * 3 + 1], cxp[idx * 3 + 2], nx, ny, nz, col[0], col[1], col[2], glow, pres, clip, flash);
      }
    }
    for (var e = 0; e < 12; e++) {
      var E = EDGES[e];
      lOut.push(cxp[E[0] * 3], cxp[E[0] * 3 + 1], cxp[E[0] * 3 + 2], col[0], col[1], col[2], pres, clip, flash);
      lOut.push(cxp[E[1] * 3], cxp[E[1] * 3 + 1], cxp[E[1] * 3 + 2], col[0], col[1], col[2], pres, clip, flash);
    }
  }

  function emitStructure(s, ang, dy, fOut, lOut) {
    var pres = s.presence, clip = s.clipY, flash = s.flash;
    for (var i = 0; i < s.parts.length; i++) {
      var p = s.parts[i];
      var col = p.tint || s.tint;
      var glow = p.tint ? 1.0 : 0.0;
      var cx = p.x, cz = p.z, cy = p.y, rot = p.rot;
      if (ang !== 0 || dy !== 0) {
        var rx = p.x - s.cx, rz = p.z - s.cz;
        var ca = Math.cos(ang), sa = Math.sin(ang);
        cx = s.cx + rx * ca - rz * sa;
        cz = s.cz + rx * sa + rz * ca;
        cy = p.y + dy;
        rot = p.rot + ang;
      }
      emitBox(fOut, lOut, cx, cy, cz, p.hx, p.hy, p.hz, rot, col, glow, pres, clip, flash);
    }
  }

  function buildCity() {
    var S = city.structures;
    var fOut = [], lOut = [];
    for (var i = 0; i < S.length; i++) {
      var s = S[i];
      s._fStart = fOut.length;
      s._lStart = lOut.length;
      s._fPrev = -1; s._lPrev = -1;
      s._pPrev = [s.presence, s.clipY, s.flash];
      emitStructure(s, 0, 0, fOut, lOut);
      s._fEnd = fOut.length;
      s._lEnd = lOut.length;
    }
    fillData = new Float32Array(fOut);
    lineData = new Float32Array(lOut);
    fillCount = fillData.length / FSTRIDE;
    lineCount = lineData.length / LSTRIDE;

    if (fillBuf) { gl.deleteBuffer(fillBuf); gl.deleteBuffer(lineBuf); }
    fillBuf = GLW.buffer(fillData);
    lineBuf = GLW.buffer(lineData);
    // reset dirty trackers
    fillDirtyMin = 1e12; fillDirtyMax = -1;
    lineDirtyMin = 1e12; lineDirtyMax = -1;
    memDirty = true;
  }

  var fillDirtyMin = 1e12, fillDirtyMax = -1, lineDirtyMin = 1e12, lineDirtyMax = -1;

  function mergeInto(a, b) { return a < b ? a : b; }

  /* per-frame sync of presence/clip/flash + animated structures */
  function syncGeometry() {
    var S = city.structures;
    for (var i = 0; i < S.length; i++) {
      var s = S[i];
      var fmin = 1e12, fmax = -1, lmin = 1e12, lmax = -1;

      if (s.anim) {
        var ang = s.spin * t + s.phase;
        var dy = Math.sin(t * 1.3 + s.phase) * s.bob;
        // rebuild in place
        var tmpF = [], tmpL = [];
        emitStructure(s, ang, dy, tmpF, tmpL);
        var n = Math.min(tmpF.length, s._fEnd - s._fStart);
        for (var k = 0; k < n; k++) fillData[s._fStart + k] = tmpF[k];
        var m = Math.min(tmpL.length, s._lEnd - s._lStart);
        for (var k2 = 0; k2 < m; k2++) lineData[s._lStart + k2] = tmpL[k2];
        fmin = s._fStart; fmax = s._fStart + n - 1;
        lmin = s._lStart; lmax = s._lStart + m - 1;
        s._pPrev = [s.presence, s.clipY, s.flash];
      } else {
        var pp = s._pPrev;
        if (pp[0] !== s.presence || pp[1] !== s.clipY || pp[2] !== s.flash) {
          for (var v = s._fStart; v < s._fEnd; v += FSTRIDE) {
            fillData[v + 10] = s.presence;
            fillData[v + 11] = s.clipY;
            fillData[v + 12] = s.flash;
          }
          for (var w = s._lStart; w < s._lEnd; w += LSTRIDE) {
            lineData[w + 6] = s.presence;
            lineData[w + 7] = s.clipY;
            lineData[w + 8] = s.flash;
          }
          fmin = s._fStart; fmax = s._fEnd - 1;
          lmin = s._lStart; lmax = s._lEnd - 1;
          s._pPrev = [s.presence, s.clipY, s.flash];
        }
      }
      if (fmax > fmin) {
        if (fmin < fillDirtyMin) fillDirtyMin = fmin;
        if (fmax > fillDirtyMax) fillDirtyMax = fmax;
      }
      if (lmax > lmin) {
        if (lmin < lineDirtyMin) lineDirtyMin = lmin;
        if (lmax > lineDirtyMax) lineDirtyMax = lmax;
      }
    }

    if (fillDirtyMax > fillDirtyMin) {
      gl.bindBuffer(gl.ARRAY_BUFFER, fillBuf);
      gl.bufferSubData(gl.ARRAY_BUFFER, fillDirtyMin * 4, fillData.subarray(fillDirtyMin, Math.min(fillData.length, fillDirtyMax + 1)));
      fillDirtyMin = 1e12; fillDirtyMax = -1;
    }
    if (lineDirtyMax > lineDirtyMin) {
      gl.bindBuffer(gl.ARRAY_BUFFER, lineBuf);
      gl.bufferSubData(gl.ARRAY_BUFFER, lineDirtyMin * 4, lineData.subarray(lineDirtyMin, Math.min(lineData.length, lineDirtyMax + 1)));
      lineDirtyMin = 1e12; lineDirtyMax = -1;
    }
    if (memDirty) {
      gl.bindTexture(gl.TEXTURE_2D, memTex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, CFG.TEX, CFG.TEX, 0, gl.RGBA, gl.UNSIGNED_BYTE, city.mem);
      memDirty = false;
    }
  }

  function bindAttribs(prog, buf, defs) {
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    for (var i = 0; i < defs.length; i++) {
      var d = defs[i];
      var loc = prog.a[d[0]];
      if (loc === undefined || loc < 0) continue;
      gl.enableVertexAttribArray(loc);
      gl.vertexAttribPointer(loc, d[1], gl.FLOAT, false, d[2] * 4, d[3] * 4);
    }
  }

  var FILL_DEFS = [['aPos', 3, FSTRIDE, 0], ['aNor', 3, FSTRIDE, 3], ['aCol', 3, FSTRIDE, 6], ['aGlow', 1, FSTRIDE, 9], ['aPres', 1, FSTRIDE, 10], ['aClip', 1, FSTRIDE, 11], ['aFlash', 1, FSTRIDE, 12]];
  var LINE_DEFS = [['aPos', 3, LSTRIDE, 0], ['aCol', 3, LSTRIDE, 3], ['aPres', 1, LSTRIDE, 6], ['aClip', 1, LSTRIDE, 7], ['aFlash', 1, LSTRIDE, 8]];

  /* =======================================================================
     camera / matrices
     ======================================================================= */

  var proj = null, view = null, viewProj = null, invVP = null;
  var camPos = [0, 1.62, 26];
  var camDir = [0, 0, -1];
  var fov = 74 * Math.PI / 180;
  var width = 1, height = 1, dpr = 1;

  function resize() {
    dpr = Math.min(window.devicePixelRatio || 1, 1.75);
    width = Math.max(2, Math.floor(canvas.clientWidth * dpr));
    height = Math.max(2, Math.floor(canvas.clientHeight * dpr));
    canvas.width = width; canvas.height = height;
    gl.viewport(0, 0, width, height);
    if (!scene) { scene = GLW.target(width, height, true); mirrorT = GLW.target(Math.max(2, width >> 1), Math.max(2, height >> 1), true); }
    else {
      GLW.resizeTarget(scene, width, height);
      GLW.resizeTarget(mirrorT, Math.max(2, width >> 1), Math.max(2, height >> 1));
    }
  }

  function updateMatrices() {
    var aspect = width / height;
    proj = M4.perspective(fov, aspect, 0.08, 420);
    view = M4.lookAt(camPos, [camPos[0] + camDir[0], camPos[1] + camDir[1], camPos[2] + camDir[2]], [0, 1, 0]);
    viewProj = M4.mul(proj, view);
    invVP = M4.invert(viewProj);
  }

  /* =======================================================================
     input
     ======================================================================= */

  function bindInput() {
    document.addEventListener('keydown', function (e) {
      keys[e.code] = true;
      if (e.code === 'Space' || e.code === 'Tab') e.preventDefault();
      if (e.code === 'KeyE' || e.code === 'Enter') firePulse();
      if (e.code === 'KeyM') { var m = audio.toggleMute(); toast(m ? 'звук выключен' : 'звук включён'); }
      if (e.code === 'KeyR' && (state === 'playing' || state === 'paused')) newCity();
      if (e.code === 'KeyH') { hintOn = !hintOn; toast(hintOn ? 'компас включён' : 'компас выключен'); }
      if (e.code === 'Escape' && state === 'playing') document.exitPointerLock && document.exitPointerLock();
    });
    document.addEventListener('keyup', function (e) { keys[e.code] = false; });

    document.addEventListener('mousemove', function (e) {
      if (state !== 'playing' || !pointerLocked) return;
      var s = 0.0022;
      yaw -= e.movementX * s;
      pitch -= e.movementY * s;
      pitch = Math.max(-1.45, Math.min(1.45, pitch));
    });

    canvas.addEventListener('mousedown', function (e) {
      if (state === 'playing' && pointerLocked && (e.button === 0)) firePulse();
    });

    document.addEventListener('pointerlockchange', function () {
      pointerLocked = (document.pointerLockElement === canvas);
      if (!pointerLocked && state === 'playing') pause();
    });

    document.getElementById('startBtn').addEventListener('click', startGame);
    document.getElementById('againBtn').addEventListener('click', function (e) {
      e.stopPropagation();
      startGame();
    });
    document.getElementById('newCityBtn').addEventListener('click', function (e) {
      e.stopPropagation();
      seed = Math.floor(Math.random() * 90000) + 1000;
      startGame();
    });

    // resume by clicking anywhere while paused
    document.addEventListener('click', function () {
      if (state === 'paused') resume();
    });

    // touch fallback: drag to look, tap to pulse
    var touching = false, lastTX = 0, lastTY = 0;
    canvas.addEventListener('touchstart', function (e) {
      if (state !== 'playing') return;
      touching = true;
      lastTX = e.touches[0].clientX; lastTY = e.touches[0].clientY;
    }, { passive: true });
    canvas.addEventListener('touchmove', function (e) {
      if (!touching || state !== 'playing') return;
      var tx = e.touches[0].clientX, ty = e.touches[0].clientY;
      yaw -= (tx - lastTX) * 0.005;
      pitch = Math.max(-1.4, Math.min(1.4, pitch - (ty - lastTY) * 0.005));
      lastTX = tx; lastTY = ty;
    }, { passive: true });
    canvas.addEventListener('touchend', function () { touching = false; }, { passive: true });
  }

  var pointerLocked = false;

  function lock() { if (canvas.requestPointerLock) canvas.requestPointerLock(); }

  /* =======================================================================
     HUD helpers
     ======================================================================= */

  function toast(text, warm) {
    var d = document.createElement('div');
    d.className = 'toast' + (warm ? ' warm' : '');
    d.textContent = text;
    el.toasts.appendChild(d);
    setTimeout(function () { d.style.transition = 'opacity .7s, transform .7s'; d.style.opacity = '0'; d.style.transform = 'translateY(-8px)'; }, 2400);
    setTimeout(function () { if (d.parentNode) d.parentNode.removeChild(d); }, 3300);
    while (el.toasts.children.length > 3) el.toasts.removeChild(el.toasts.firstChild);
  }

  var subTimer = 0;
  function setSub(text, dur) {
    if (!text) { el.subtitle.classList.remove('on'); return; }
    el.subtitle.innerHTML = text;
    el.subtitle.classList.add('on');
    subTimer = dur || 4;
  }

  function markSub(key, text, dur) {
    if (subtitlesShown[key]) return;
    subtitlesShown[key] = true;
    setSub(text, dur || 5);
  }

  /* =======================================================================
     game flow
     ======================================================================= */

  function startGame() {
    audio.init();
    restart(seed);
    lastPulseFired = -99;
    el.menu.classList.add('hidden');
    el.end.classList.add('hidden');
    el.hud.classList.add('on');
    state = 'playing';
    fade = 0; fadeTarget = 1;
    el.seedLabel.textContent = 'город · № ' + seed;
    setSub('Иди. Всё, что вокруг, — <b>твоя память</b>. Шагни туда, где ещё темно.', 6);
    lock();
    setTimeout(function () { toast('вспоминай · ' + city.total + ' фрагментов'); }, 700);
    setTimeout(function () { setSub('Нажми <b>H</b>, если потеряешься. <b>Пробел</b> — взлёт.', 5); }, 7600);
  }

  function restart(newSeed) {
    seed = newSeed | 0 || 7;
    city = new City(seed);
    city.reveal(0, 26, 13, 0, { ripple: false });
    buildCity();
    pos = { x: 0, y: 0, z: 26 };
    vel = { x: 0, z: 0, y: 0 };
    yaw = 0; pitch = -0.02;
    flying = false;
    timeLeft = 300; elapsed = 0;
    pulsing = 0; harmony = 0;
    pulsesUsed = 0; distanceWalked = 0;
    lastPulse = -99; lastBell = -99; lastRipple = -99;
    subtitlesShown = {};
    newBuildings = 0; newTimer = 0;
    endShown = false; endTimer = 0; won = false;
    fade = 0; fadeTarget = 1;
    el.seedLabel.textContent = 'город · № ' + seed;
    el.end.classList.add('hidden');
    el.hintArrow.classList.remove('on');
    updateHud(true);
    try {
      var be2 = document.getElementById('best');
      var bv = Number(localStorage.getItem('memoria.best') || 0);
      if (be2 && bv > 0) be2.textContent = 'лучший результат · ' + bv + '%';
    } catch (e) {}
    if (state === 'playing') {
      toast('новый город · № ' + seed);
      el.hud.classList.add('on');
    }
  }

  function pause() {
    if (state !== 'playing') return;
    state = 'paused';
    var ov = document.getElementById('pauseOv');
    if (!ov) {
      ov = document.createElement('div');
      ov.id = 'pauseOv';
      ov.style.cssText = 'position:fixed;inset:0;z-index:25;display:grid;place-items:center;background:rgba(2,5,11,.55);backdrop-filter:blur(5px);cursor:pointer;text-align:center;padding:24px';
      ov.innerHTML = '<div><div style="font-size:11px;letter-spacing:.44em;color:#7fd8f5;text-transform:uppercase">пауза</div>' +
        '<div style="margin-top:14px;font-size:26px;font-weight:200;letter-spacing:.1em;color:#eafcff">Память ждёт</div>' +
        '<div style="margin-top:12px;font-size:11px;letter-spacing:.28em;color:#6f97b0;text-transform:uppercase">клик — продолжить</div></div>';
      ov.addEventListener('click', function (e) { e.stopPropagation(); resume(); });
      document.body.appendChild(ov);
    }
    ov.style.display = 'grid';
  }

  function resume() {
    if (state !== 'paused') return;
    var ov = document.getElementById('pauseOv');
    if (ov) ov.style.display = 'none';
    state = 'playing';
    lock();
  }

  var pulsing = 0;

  function firePulse() {
    if (state !== 'playing') return;
    if (t - lastPulseFired < 5.5) return;
    lastPulseFired = t;
    pulsesUsed++;
    pulsing = 1;
    shake = 0.8;
    city.reveal(pos.x, pos.z, 18.5, t, { power: 1.9 });
    audio.whoosh(1.0);
    audio.bell(196 * Math.pow(2, city.progress() * 1.5), 0.16);
    for (var i = 0; i < 3; i++) {
      setTimeout(function () { audio.bell(392 + Math.random() * 300, 0.07); }, 90 + i * 110);
    }
    memDirty = true;
  }

  function finishWin() {
    if (state !== 'playing') return;
    state = 'won';
    won = true;
    endTimer = 0; endShown = false;
    audio.vortex();
    audio.chord(196, true, 0.18);
    toast('город вспомнен', true);
    setSub('Ты <b>вспомнил всё</b>. Город помнит тебя.', 6);
  }

  function finishLose() {
    if (state !== 'playing') return;
    state = 'lost';
    won = false;
    endTimer = 0; endShown = false;
    audio.setTension(0);
    setSub('Время вышло. Остальное осталось <b>в темноте</b>.', 6);
  }

  function showEnd() {
    endShown = true;
    var pct = Math.round(city.progress() * 100);
    try {
      var best = Math.max(pct, Number(localStorage.getItem('memoria.best') || 0));
      localStorage.setItem('memoria.best', String(best));
      var be = document.getElementById('best');
      if (be) be.textContent = 'лучший результат · ' + best + '%';
    } catch (e) { /* file:// may block storage */ }
    document.getElementById('sPct').textContent = pct + '%';
    document.getElementById('sTime').textContent = fmtTime(Math.min(300, elapsed));
    document.getElementById('sPulses').textContent = pulsesUsed;
    document.getElementById('sSteps').textContent = Math.round(distanceWalked / 0.78);
    el.end.classList.add(won ? 'win' : 'lose');
    document.getElementById('verdict').textContent = won ? 'Город собран' : (pct >= 70 ? 'Почти вспомнил' : 'Город остался сном');
    document.getElementById('epitaph').textContent = won
      ? 'Ты прошёл его улицы и вернул каждую башню из небытия. Теперь он существует. И, кажется, дышит.'
      : (pct >= 70
        ? 'Ты уже видел почти всё. Город почти стал настоящим — но темнота оказалась быстрее. Ещё одна попытка?'
        : 'Не всё, что ты не вспомнил, потеряно. Оно просто ждёт следующего раза.');
    el.end.classList.remove('hidden');
    el.hud.classList.remove('on');
    if (document.exitPointerLock) document.exitPointerLock();
    document.getElementById('loader').classList.add('off');
  }

  function fmtTime(s) {
    s = Math.max(0, Math.ceil(s));
    var m = Math.floor(s / 60);
    var ss = s % 60;
    return m + ':' + (ss < 10 ? '0' : '') + ss;
  }

  function newCity() {
    var ns = Math.floor(Math.random() * 90000) + 1000;
    restart(ns);
    toast('другой город · № ' + ns);
  }

  /* =======================================================================
     update
     ======================================================================= */

  var STEP_SOUND_DIST = 0;

  function update(dt) {
    // ---- camera orientation
    camDir = [
      -Math.sin(yaw) * Math.cos(pitch),
      Math.sin(pitch),
      -Math.cos(yaw) * Math.cos(pitch)
    ];

    if (state === 'playing') {
      elapsed += dt;
      timeLeft = Math.max(0, 300 - elapsed);

      // ---- movement
      var wantFly = !!keys['Space'];
      var down = !!keys['ControlLeft'] || !!keys['ControlRight'] || !!keys['KeyC'] || !!keys['ShiftRight'];
      var fwd = (keys['KeyW'] || keys['ArrowUp'] ? 1 : 0) - (keys['KeyS'] || keys['ArrowDown'] ? 1 : 0);
      var str = (keys['KeyD'] || keys['ArrowRight'] ? 1 : 0) - (keys['KeyA'] || keys['ArrowLeft'] ? 1 : 0);
      sprint = !!keys['ShiftLeft'];
      if (wantFly || down) flying = true;
      if (flying && !wantFly && pos.y <= 0.66 && vel.y <= 0.4) flying = false;

      var speed = flying ? (sprint ? 15 : 10.5) : (sprint ? 8.4 : 4.8);
      var fx = -Math.sin(yaw), fz = -Math.cos(yaw);
      var rx = Math.cos(yaw), rz = -Math.sin(yaw);
      var mx = fx * fwd + rx * str, mz = fz * fwd + rz * str;
      var ml = Math.hypot(mx, mz);
      if (ml > 0.001) { mx /= ml; mz /= ml; }

      var targetVX = mx * speed, targetVZ = mz * speed;
      var accel = flying ? 3.4 : 11.0;
      vel.x += (targetVX - vel.x) * Math.min(1, accel * dt);
      vel.z += (targetVZ - vel.z) * Math.min(1, accel * dt);

      var nx = pos.x + vel.x * dt;
      var nz = pos.z + vel.z * dt;

      if (flying) {
        var vy = (wantFly ? 1 : 0) - (down ? 1 : 0);
        vel.y += (vy * 7.5 - vel.y) * Math.min(1, 5 * dt);
        pos.y += vel.y * dt;
        if (pos.y < 0.6) { pos.y = 0.6; vel.y = 0; }
        if (pos.y > 46) { pos.y = 46; vel.y = Math.min(0, vel.y); }
        var res = city.collide(nx, nz, 0.42, pos.y);
        nx = res[0]; nz = res[1];
        pos.x = nx; pos.z = nz;
        var gb = city.groundAt(nx, nz, pos.y + 0.42);
        if (pos.y < gb) { pos.y = gb; vel.y = Math.max(0, vel.y); }
      } else {
        // step up / blocked
        var gh = city.groundAt(nx, nz, pos.y + 0.45);
        if (gh > pos.y + 0.45) {
          var gx = city.groundAt(nx, pos.z, pos.y + 0.45);
          if (gx <= pos.y + 0.45) { nz = pos.z; }
          else {
            var gz = city.groundAt(pos.x, nz, pos.y + 0.45);
            if (gz <= pos.y + 0.45) nx = pos.x;
            else { nx = pos.x; nz = pos.z; }
          }
        }
        var res2 = city.collide(nx, nz, 0.42, pos.y);
        nx = res2[0]; nz = res2[1];
        var ground = city.groundAt(nx, nz, pos.y + 0.6);
        if (ground > pos.y) pos.y += Math.min(ground - pos.y, 8 * dt * Math.max(1, ground - pos.y));
        else pos.y += (ground - pos.y) * Math.min(1, 14 * dt);
        vel.y = 0;
      }
      distanceWalked += Math.hypot(nx - pos.x, nz - pos.z);
      pos.x = nx; pos.z = nz;

      // footsteps
      STEP_SOUND_DIST += Math.hypot(nx - pos.x, nz - pos.z);
      if (!flying && Math.hypot(vel.x, vel.z) > 1.4 && t - lastStep > (sprint ? 0.29 : 0.42)) {
        lastStep = t; audio.step();
      }

      // ---- reveal
      var rad = flying ? CFG.FLY_R : CFG.WALK_R;
      var before = city.revealedCount;
      var newly = city.reveal(pos.x, pos.z, rad, t, { ripple: false });
      if (newly > 0) {
        newBuildings += newly;
        memDirty = true;
        if (t - lastBell > 0.24) {
          lastBell = t;
          var f = 330 * Math.pow(2, Math.floor(city.progress() * 10) / 10);
          audio.bell(f + Math.random() * 60, 0.10 + Math.min(0.12, newly * 0.012), 'warm');
        }
      }
      if (t - lastRipple > 0.75) {
        var moved = Math.hypot(pos.x - lastRippleX, pos.z - lastRippleZ);
        if (moved > 2.2) {
          city.rings.push({ x: pos.x, z: pos.z, r0: 0, t0: t, power: 0.55 });
          lastRipple = t;
          lastRippleX = pos.x; lastRippleZ = pos.z;
        }
      }

      // pulse wave / shake decay
      pulsing = Math.max(0, pulsing - dt * 1.35);
      shake = Math.max(0, shake - dt * 2.4);

      // ---- milestones
      var p = city.progress();
      if (p > 0.0) markSub('s1', 'Дома появляются там, где ты <b>прошёл</b>. Не останавливайся.', 4.5);
      if (p >= 0.1) markSub('s10', 'Уже <b>десятая часть</b> города дышит.', 4);
      if (p >= 0.25) { markSub('s25', '<b>Четверть.</b> Слышишь, как он отзывается?', 4.5); if (!subtitlesShown.chord25) { subtitlesShown.chord25 = true; audio.chord(220, true, 0.12); toast('25% · город просыпается'); } }
      if (p >= 0.5) { markSub('s50', '<b>Половина.</b> Ты идёшь быстрее, чем темнота.', 4.5); if (!subtitlesShown.chord50) { subtitlesShown.chord50 = true; audio.chord(261, true, 0.14); toast('50% · эхо стало светом', true); } }
      if (p >= 0.75) { markSub('s75', '<b>Три четверти.</b> Не смотри на таймер.', 4); if (!subtitlesShown.chord75) { subtitlesShown.chord75 = true; audio.chord(329, true, 0.15); toast('75% · почти всё', true); } }
      if (p >= 0.95) markSub('s95', 'Ещё одна улица — и он <b>вспомнит тебя</b>.', 5);
      if (timeLeft < 60) markSub('t60', 'Осталась <b>минута</b>. Дыши ровно.', 4);
      if (timeLeft < 30 && !subtitlesShown.t30) { subtitlesShown.t30 = true; setSub('<b>Тридцать секунд.</b>', 3); audio.heart(); }
      if (timeLeft < 30) { audio.setTension(1 - timeLeft / 30); if (Math.floor(t * 1.1) !== Math.floor((t - dt) * 1.1)) audio.heart(); }
      else audio.setTension(Math.min(1, p * 0.7));

      if (city.revealedCount >= city.total && city.total > 0) finishWin();
      if (timeLeft <= 0) finishLose();

      cameraFollow(dt);
    } else if (state === 'menu' || state === 'won' || state === 'lost') {
      cinematic(dt);
      if (state === 'won' || state === 'lost') {
        endTimer += dt;
        if (!endShown && endTimer > 3.4) showEnd();
      }
      if (state === 'won') {
        harmony = Math.min(1, harmony + dt * 0.45);
        pulsing = Math.max(0, pulsing - dt);
      }
      if (state === 'lost') {
        fadeTarget = 0.35;
        fade += (fadeTarget - fade) * Math.min(1, dt * 1.2);
      }
    }

    if ((state === 'playing' || state === 'menu') && !won) {
      if (fade < 1) fade += (1 - fade) * Math.min(1, dt * 1.1);
    }

    // pulse visual decay for shader
    if (pulsing > 0) pulseT = t;
    city.update(t, dt);
    syncGeometry();
  }

  var lastRippleX = 0, lastRippleZ = 26;

  var bobPhase = 0;
  function cameraFollow(dt) {
    bobPhase += dt * (flying ? 2 : Math.hypot(vel.x, vel.z) * 1.9);
    var bob = flying ? Math.sin(bobPhase * 1.4) * 0.045 : Math.sin(bobPhase) * 0.028;
    var sway = Math.sin(bobPhase * 0.5) * 0.012;
    var sx = (Math.random() - 0.5) * shake * 0.16;
    var sy = (Math.random() - 0.5) * shake * 0.16;
    camPos = [pos.x + sx, pos.y + CFG.EYE + bob, pos.z + sway];
    var kick = pulsing * 0.05;
    camDir = [
      -Math.sin(yaw) * Math.cos(pitch),
      Math.sin(pitch) - kick - sy * 0.4,
      -Math.cos(yaw) * Math.cos(pitch)
    ];
  }

  function cinematic(dt) {
    if (state === 'won') {
      var k = Math.min(1, endTimer / 3.4);
      var ang = -0.7 + endTimer * 0.32;
      var rad = 26 - 12 * k;
      var hgt = 9 + 14 * k;
      var cx = Math.cos(ang) * rad, cz = Math.sin(ang) * rad - 6;
      camPos = [cx, hgt, cz];
      var lx = 0, ly = 16 + 8 * k, lz = 0;
      camDir = normalize3([lx - cx, ly - hgt, lz - cz]);
      city.reveal(0, 0, 60, t, { ripple: false, instant: true });
      return;
    }
    var a = t * 0.055 + 0.6;
    var r = 34 + Math.sin(t * 0.09) * 5;
    var h = 10 + Math.sin(t * 0.13) * 5.5;
    camPos = [Math.cos(a) * r, h, Math.sin(a) * r];
    var target = [0, 12 + Math.sin(t * 0.1) * 3, 0];
    camDir = normalize3([target[0] - camPos[0], target[1] - camPos[1], target[2] - camPos[2]]);
    // the city assembles itself behind the menu
    if (state === 'menu') city.reveal(camPos[0] * 0.9, camPos[2] * 0.9, 7.5, t, { ripple: false });
  }

  function normalize3(v) {
    var l = Math.hypot(v[0], v[1], v[2]) || 1;
    return [v[0] / l, v[1] / l, v[2] / l];
  }

  /* =======================================================================
     HUD update
     ======================================================================= */

  function updateHud(force) {
    var p = city.progress();
    el.timer.textContent = fmtTime(timeLeft);
    el.timer.classList.toggle('low', state === 'playing' && timeLeft < 30);
    el.bar.style.width = (p * 100).toFixed(1) + '%';
    el.pct.textContent = Math.round(p * 100) + '% ВСПОМНЕНО';
    el.count.textContent = city.revealedCount + ' / ' + city.total;
    el.posx.textContent = Math.round(pos.x);
    el.posz.textContent = Math.round(pos.z);
    el.modeLabel.textContent = flying ? 'В ПОЛЁТЕ' : (sprint ? 'БЕГ' : 'ПЕШКОМ');

    var cd = Math.min(1, (t - lastPulseFired) / 5.5);
    el.cdArc.style.setProperty('--cd', (cd * 360).toFixed(0) + 'deg');
    el.cdText.textContent = cd >= 1 ? 'ГОТОВ' : Math.ceil((1 - cd) * 5.5) + 'с';

    // hint arrow toward the nearest not-yet-remembered block
    var near = city.nearestUnknown(pos.x, pos.z);
    if (near && state === 'playing' && hintOn) {
      var ang = Math.atan2(near.s.cx - pos.x, near.s.cz - pos.z); // 0 = +z
      var rel = ang - Math.atan2(-Math.sin(yaw), -Math.cos(yaw));
      el.hintArrow.classList.add('on');
      el.hintArrow.style.transform = 'rotate(' + (-rel * 180 / Math.PI).toFixed(1) + 'deg)';
      el.hintArrow.style.opacity = near.d < 60 ? 0.85 : 0.4;
    } else {
      el.hintArrow.classList.remove('on');
    }

    if (subTimer > 0) {
      subTimer -= dt;
      if (subTimer <= 0) el.subtitle.classList.remove('on');
    }
  }

  /* =======================================================================
     draw
     ======================================================================= */

  function revealCenter() {
    var inGame = (state === 'playing' || state === 'paused');
    return inGame ? [pos.x, pos.z, (flying ? CFG.FLY_R : CFG.WALK_R)] : [camPos[0], camPos[2], 16];
  }

  function setWorldUniforms(prog, mirror) {
    gl.uniformMatrix4fv(prog.u.uViewProj, false, viewProj);
    gl.uniform3f(prog.u.uCamPos, camPos[0], camPos[1], camPos[2]);
    if (prog.u.uMirror) gl.uniform1f(prog.u.uMirror, mirror ? 1 : 0);
    var rc = revealCenter();
    if (prog.u.uCenter) gl.uniform3f(prog.u.uCenter, rc[0], 0, rc[1]);
    if (prog.u.uRadius) gl.uniform1f(prog.u.uRadius, rc[2]);
    if (prog.u.uTime) gl.uniform1f(prog.u.uTime, t);
    if (prog.u.uFog) gl.uniform3f(prog.u.uFog, 0.022, 0.045, 0.075);
    if (prog.u.uHarmony) gl.uniform1f(prog.u.uHarmony, harmony);
    if (prog.u.uPulse) gl.uniform1f(prog.u.uPulse, pulsing);
    if (prog.u.uWobble) gl.uniform1f(prog.u.uWobble, 0);
  }

  function drawSky() {
    gl.useProgram(progSky.p);
    gl.uniformMatrix4fv(progSky.u.uInvVP, false, invVP);
    gl.uniform3f(progSky.u.uCamPos, camPos[0], camPos[1], camPos[2]);
    gl.uniform1f(progSky.u.uTime, t);
    gl.uniform3f(progSky.u.uFog, 0.022, 0.045, 0.075);
    gl.disable(gl.DEPTH_TEST);
    gl.depthMask(false);
    gl.bindBuffer(gl.ARRAY_BUFFER, quadBuf);
    var loc = progSky.a.aPos;
    gl.enableVertexAttribArray(loc);
    gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    gl.depthMask(true);
    gl.enable(gl.DEPTH_TEST);
  }

  function drawWorld(mirror) {
    // fill
    gl.useProgram(progWorld.p);
    setWorldUniforms(progWorld, mirror);
    bindAttribs(progWorld, fillBuf, FILL_DEFS);
    gl.disable(gl.BLEND);
    gl.drawArrays(gl.TRIANGLES, 0, fillCount);

    // wireframe
    gl.useProgram(progLine.p);
    setWorldUniforms(progLine, mirror);
    bindAttribs(progLine, lineBuf, LINE_DEFS);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
    gl.depthMask(false);
    gl.drawArrays(gl.LINES, 0, lineCount);
    gl.depthMask(true);
    gl.disable(gl.BLEND);
  }

  function drawFloor() {
    gl.useProgram(progFloor.p);
    gl.uniformMatrix4fv(progFloor.u.uViewProj, false, viewProj);
    gl.uniform3f(progFloor.u.uCamPos, camPos[0], camPos[1], camPos[2]);
    var rcF = revealCenter();
    gl.uniform3f(progFloor.u.uCenter, rcF[0], 0, rcF[1]);
    gl.uniform3f(progFloor.u.uFog, 0.022, 0.045, 0.075);
    gl.uniform1f(progFloor.u.uTime, t);
    gl.uniform1f(progFloor.u.uRadius, rcF[2]);
    gl.uniform1f(progFloor.u.uHarmony, harmony);

    var n = 0;
    for (var i = 0; i < city.rings.length && n < 6; i++) {
      var r = city.rings[i];
      ringArr[n * 4] = r.x; ringArr[n * 4 + 1] = r.z; ringArr[n * 4 + 2] = r.t0; ringArr[n * 4 + 3] = r.power;
      n++;
    }
    gl.uniform4fv(progFloor.u.uRings, ringArr);
    gl.uniform1f(progFloor.u.uRingN, n);

    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, mirrorT.tex);
    gl.uniform1i(progFloor.u.uMirrorTex, 0);
    gl.activeTexture(gl.TEXTURE1);
    gl.bindTexture(gl.TEXTURE_2D, memTex);
    gl.uniform1i(progFloor.u.uMemTex, 1);

    gl.bindBuffer(gl.ARRAY_BUFFER, floorBuf);
    var loc = progFloor.a.aPos;
    gl.enableVertexAttribArray(loc);
    gl.vertexAttribPointer(loc, 3, gl.FLOAT, false, 0, 0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
  }

  function render() {
    if (!scene) return;
    updateMatrices();
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
    gl.disable(gl.CULL_FACE);

    // --- reflection pass
    gl.bindFramebuffer(gl.FRAMEBUFFER, mirrorT.fbo);
    gl.viewport(0, 0, mirrorT.w, mirrorT.h);
    gl.clearColor(0.012, 0.028, 0.05, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    drawWorld(true);

    // --- main pass
    gl.bindFramebuffer(gl.FRAMEBUFFER, scene.fbo);
    gl.viewport(0, 0, scene.w, scene.h);
    gl.clearColor(0.02, 0.04, 0.07, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    drawSky();
    gl.clear(gl.DEPTH_BUFFER_BIT);
    drawFloor();
    drawWorld(false);

    // --- post
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, width, height);
    gl.useProgram(progPost.p);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, scene.tex);
    gl.uniform1i(progPost.u.uScene, 0);
    gl.uniform2f(progPost.u.uRes, width, height);
    gl.uniform1f(progPost.u.uTime, t);
    gl.uniform1f(progPost.u.uFade, fade);
    gl.uniform1f(progPost.u.uHarmony, harmony);
    gl.uniform1f(progPost.u.uShake, shake);
    gl.disable(gl.DEPTH_TEST);
    gl.bindBuffer(gl.ARRAY_BUFFER, quadBuf);
    var loc = progPost.a.aPos;
    gl.enableVertexAttribArray(loc);
    gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    gl.enable(gl.DEPTH_TEST);
  }

  /* =======================================================================
     loop
     ======================================================================= */

  function loop(now) {
    requestAnimationFrame(loop);
    if (!last) last = now;
    dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    t += dt;
    try {
      update(dt);
      render();
      if (state === 'playing' || state === 'paused' || state === 'menu') updateHud();
    } catch (e) {
      console.error(e);
      fail(e);
    }
  }

  // tiny debug handle (also used by the headless smoke test)
  try {
    window.__MEMDBG = {
      get pos() { return pos; }, get vel() { return vel; }, get state() { return state; },
      get keys() { return keys; }, get yaw() { return yaw; }, get flying() { return flying; },
      get timeLeft() { return timeLeft; }, get city() { return city; }, get frame() { return t; }
    };
  } catch (e) { /* ignore */ }

  if (document.readyState === 'complete' || document.readyState === 'interactive') setTimeout(init, 0);
  else window.addEventListener('DOMContentLoaded', init);
})();
