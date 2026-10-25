import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { HALF_W, LAP, surfaceFrame, wrapPi } from './mobius.js';
import { Sfx } from './audio.js';

/* =========================================================================
   М Ё Б И У С
   -------------------------------------------------------------------------
   Мир — лента Мёбиуса. Физика живёт в координатах (u, v, h):
     u — вдоль ленты, v — поперёк,
     h — высота над поверхностью в системе игрока (его «верх» = normal*sign).
   sign = +1 — игрок на лицевой стороне, -1 — на изнанке.
   Разлом переворачивает sign: поверхность остаётся под ногами,
   а верх и низ меняются местами.
   Врата стоят на стороне -1 → пройти их можно только «вверх ногами».
   ========================================================================= */

const MAX_LAP = 6;
const RIFT_A = 0.85;
const RIFT_B = 2.55;
const GATE_U = 3.45;
const GATE_SIGN = -1;
const RIFT_H = 0.55;        // полувысота завесы разлома = порог переворота

const BASE_SPEED = 3.15;
const SPEED_PER_LAP = 0.24;
const GRAV = 14;
const JUMP_V = 5.25;        // апекс ≈ 0.98
const V_MAX = 4.8;
const V_ACC = 34;
const V_DAMP = 13;

/* ------------------------------ рендерер -------------------------------- */

const canvas = document.getElementById('c');
let renderer;
try {
  renderer = new THREE.WebGLRenderer({
    canvas, antialias: true, powerPreference: 'high-performance',
  });
} catch (e) {
  const err = document.getElementById('err');
  err.style.display = 'grid';
  err.innerHTML = 'Для этой игры нужен WebGL.<br>Открой её в современном браузере.';
  throw e;
}
renderer.setPixelRatio(Math.min(devicePixelRatio || 1, 1.75));
renderer.setClearColor(0x04060e, 1);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;

const scene = new THREE.Scene();
scene.fog = new THREE.FogExp2(0x04070f, 0.016);

const camera = new THREE.PerspectiveCamera(62, 1, 0.1, 700);
camera.position.set(0, 12, 26);
camera.up.set(0, 1, 0);

/* ---------------------------- мелкие утилиты ---------------------------- */

const lapsOf = (u) => Math.floor(u / LAP);
const mirrorV = (v, lap) => (lap % 2 ? -v : v);

const _m4 = new THREE.Matrix4();
const _q = new THREE.Quaternion();
const tmpV2 = new THREE.Vector3();

function basisMatrix(u, v, upSign, out = _m4) {
  const f = surfaceFrame(u, v, {});
  const up = f.normal.clone().multiplyScalar(upSign);
  const fwd = upSign > 0 ? f.tangent.clone() : f.tangent.clone().negate();
  // X × Y = Z: side × (n·upSign) = tangent·upSign  →  правая тройка
  return out.makeBasis(f.side, up, fwd);
}

function transformFor(u, v, upSign, h = 0) {
  const f = surfaceFrame(u, v, {});
  const pos = f.pos.clone().addScaledVector(f.normal, upSign * h);
  const quat = new THREE.Quaternion().setFromRotationMatrix(
    basisMatrix(u, v, upSign)
  );
  return { pos, quat };
}

/* ------------------------------ текстуры -------------------------------- */

function dotTexture() {
  const s = 64;
  const c = document.createElement('canvas');
  c.width = c.height = s;
  const g = c.getContext('2d');
  const grd = g.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2);
  grd.addColorStop(0, 'rgba(255,255,255,1)');
  grd.addColorStop(0.35, 'rgba(255,255,255,.6)');
  grd.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grd;
  g.fillRect(0, 0, s, s);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

function glowTexture() {
  const s = 256;
  const c = document.createElement('canvas');
  c.width = c.height = s;
  const g = c.getContext('2d');
  const grd = g.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2);
  grd.addColorStop(0, 'rgba(255,255,255,.95)');
  grd.addColorStop(0.18, 'rgba(150,240,255,.55)');
  grd.addColorStop(0.5, 'rgba(60,120,220,.14)');
  grd.addColorStop(1, 'rgba(10,20,60,0)');
  g.fillStyle = grd;
  g.fillRect(0, 0, s, s);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

function streakTexture() {
  const W = 96;
  const H = 256;
  const c = document.createElement('canvas');
  c.width = W;
  c.height = H;
  const g = c.getContext('2d');
  g.clearRect(0, 0, W, H);
  for (let x = 0; x < W; x += 2) {
    const a = 0.08 + Math.random() * 0.85;
    const grd = g.createLinearGradient(0, 0, 0, H);
    grd.addColorStop(0, 'rgba(190,250,255,0)');
    grd.addColorStop(0.18, `rgba(220,255,255,${a})`);
    grd.addColorStop(0.5, `rgba(255,255,255,${Math.min(1, a + 0.15)})`);
    grd.addColorStop(0.82, `rgba(220,255,255,${a})`);
    grd.addColorStop(1, 'rgba(190,250,255,0)');
    g.fillStyle = grd;
    g.fillRect(x, 0, 1.5, H);
  }
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  return t;
}

const DOT = dotTexture();
const GLOW = glowTexture();
const STREAK = streakTexture();

/* -------------------------------- лента --------------------------------- */

function buildRibbon() {
  const NU = 440;
  const NV = 14;
  const idx = [];
  const posArr = new Float32Array((NU + 1) * (NV + 1) * 3);

  // считать лицевую и изнаночную палитры будем из одних и тех же точек
  const palettes = [
    { // лицевая сторона — холодная бирюза
      a: 0x061a2a, b: 0x0c2f44, edge: 0x1a6f92,
      emissive: 0x03080f,
    },
    { // изнанка — мир «наоборот»: магента
      a: 0x2a0d20, b: 0x481833, edge: 0xc2438a,
      emissive: 0x1a0612,
    },
  ];
  const c = new THREE.Color();

  for (let i = 0; i <= NU; i++) {
    const u = (i / NU) * LAP;
    const a = u * 0.5;
    for (let j = 0; j <= NV; j++) {
      const v = -HALF_W + (j / NV) * HALF_W * 2;
      const k = 9 + v * Math.cos(a);
      const o = (i * (NV + 1) + j) * 3;
      posArr[o] = k * Math.cos(u);
      posArr[o + 1] = v * Math.sin(a);
      posArr[o + 2] = k * Math.sin(u);
    }
  }
  for (let i = 0; i < NU; i++) {
    for (let j = 0; j < NV; j++) {
      const a = i * (NV + 1) + j;
      const b = a + (NV + 1);
      idx.push(a, b, a + 1, b, b + 1, a + 1);
    }
  }

  // базовая геометрия с нормалями (нужны для смещения сторон)
  const base = new THREE.BufferGeometry();
  base.setAttribute('position', new THREE.BufferAttribute(posArr, 3));
  base.setIndex(idx);
  base.computeVertexNormals();
  const nrm = base.attributes.normal.array;

  for (const [k, pal] of palettes.entries()) {
    const g = new THREE.BufferGeometry();
    const off = (k === 0 ? 1 : -1) * 0.022; // каждая сторона чуть «своя»
    const p2 = posArr.slice();
    for (let i = 0; i < p2.length; i += 3) {
      p2[i] += nrm[i] * off;
      p2[i + 1] += nrm[i + 1] * off;
      p2[i + 2] += nrm[i + 2] * off;
    }
    const col = new Float32Array((NU + 1) * (NV + 1) * 3);
    const cA = new THREE.Color(pal.a);
    const cB = new THREE.Color(pal.b);
    const cEdge = new THREE.Color(pal.edge);
    for (let i = 0; i <= NU; i++) {
      const u = (i / NU) * LAP;
      const band = Math.floor(u / 0.11) % 2 === 0;
      for (let j = 0; j <= NV; j++) {
        const v = -HALF_W + (j / NV) * HALF_W * 2;
        const o = (i * (NV + 1) + j) * 3;
        const center = 1 - Math.abs(v) / HALF_W;
        c.copy(band ? cA : cB).lerp(cEdge, center * 0.55);
        c.multiplyScalar(0.5 + 0.28 * Math.sin(u * 3.0 + v * 0.8));
        col[o] = c.r;
        col[o + 1] = c.g;
        col[o + 2] = c.b;
      }
    }
    g.setAttribute('position', new THREE.BufferAttribute(p2, 3));
    g.setAttribute('color', new THREE.BufferAttribute(col, 3));
    g.setIndex(idx);
    g.computeVertexNormals();
    scene.add(new THREE.Mesh(g, new THREE.MeshStandardMaterial({
      vertexColors: true,
      side: THREE.DoubleSide,
      roughness: 0.55,
      metalness: 0.3,
      emissive: pal.emissive,
      emissiveIntensity: 1,
    })));
  }
}

function buildEdges() {
  const NU = 440;
  [-1, 1].forEach((sgn) => {
    const g = new THREE.BufferGeometry();
    const pos = new Float32Array((NU + 1) * 2 * 3);
    const idx = [];
    for (let i = 0; i <= NU; i++) {
      const u = (i / NU) * LAP;
      const a = u * 0.5;
      for (let j = 0; j < 2; j++) {
        const v = j === 0 ? sgn * (HALF_W - 0.08) : sgn * HALF_W;
        const k = 9 + v * Math.cos(a);
        const o = (i * 2 + j) * 3;
        pos[o] = k * Math.cos(u);
        pos[o + 1] = v * Math.sin(a);
        pos[o + 2] = k * Math.sin(u);
      }
      if (i < NU) {
        const a0 = i * 2;
        idx.push(a0, a0 + 2, a0 + 1, a0 + 2, a0 + 3, a0 + 1);
      }
    }
    g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    g.setIndex(idx);
    // кромка тоже двуцветная: лицевая — циан, изнаночная — магента
    scene.add(new THREE.Mesh(g, new THREE.MeshBasicMaterial({
      color: sgn > 0 ? 0x5ff2ff : 0xff5cc8,
      side: THREE.FrontSide, transparent: true, opacity: 0.6, toneMapped: false,
    })));
    scene.add(new THREE.Mesh(g, new THREE.MeshBasicMaterial({
      color: sgn > 0 ? 0xff5cc8 : 0x8ef4ff,
      side: THREE.BackSide, transparent: true, opacity: 0.5, toneMapped: false,
    })));
  });
}

function buildRungs() {
  const N = 160;
  const parts = [];
  for (let i = 0; i < N; i++) {
    const u = (i / N) * LAP;
    const a = u * 0.5;
    for (let j = 0; j < 10; j++) {
      const v0 = -HALF_W + (j / 10) * HALF_W * 2;
      const v1 = -HALF_W + ((j + 1) / 10) * HALF_W * 2;
      const k0 = 9 + v0 * Math.cos(a);
      const k1 = 9 + v1 * Math.cos(a);
      parts.push(
        k0 * Math.cos(u), v0 * Math.sin(a), k0 * Math.sin(u),
        k1 * Math.cos(u), v1 * Math.sin(a), k1 * Math.sin(u)
      );
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(parts, 3));
  scene.add(new THREE.LineSegments(g, new THREE.LineBasicMaterial({
    color: 0x9df6ff, transparent: true, opacity: 0.1, toneMapped: false,
  })));
}

/* -------------------------------- космос -------------------------------- */

const rings = [];
const coreSpin = [];

function buildCore() {
  // ядро — тёмная сфера в светящейся сетке: читается и не «выжигает» кадр
  scene.add(new THREE.Mesh(
    new THREE.IcosahedronGeometry(1.7, 3),
    new THREE.MeshBasicMaterial({ color: 0x05161f })
  ));
  const wire = new THREE.Mesh(
    new THREE.IcosahedronGeometry(1.74, 1),
    new THREE.MeshBasicMaterial({
      color: 0x7ce9ff, wireframe: true, transparent: true, opacity: 0.55,
      blending: THREE.AdditiveBlending, toneMapped: false, depthWrite: false,
    })
  );
  scene.add(wire);
  const wire2 = new THREE.Mesh(
    new THREE.IcosahedronGeometry(2.35, 1),
    new THREE.MeshBasicMaterial({
      color: 0xff5cc8, wireframe: true, transparent: true, opacity: 0.22,
      blending: THREE.AdditiveBlending, toneMapped: false, depthWrite: false,
    })
  );
  scene.add(wire2);
  coreSpin.push(wire, wire2);

  const halo = new THREE.Sprite(new THREE.SpriteMaterial({
    map: GLOW, color: 0x74ecff, blending: THREE.AdditiveBlending,
    depthWrite: false, transparent: true, opacity: 0.95, toneMapped: false,
  }));
  halo.scale.setScalar(11);
  halo.material.opacity = 0.34;
  scene.add(halo);

  const halo2 = new THREE.Sprite(new THREE.SpriteMaterial({
    map: GLOW, color: 0xff5cc8, blending: THREE.AdditiveBlending,
    depthWrite: false, transparent: true, opacity: 0.36, toneMapped: false,
  }));
  halo2.scale.setScalar(26);
  scene.add(halo2);

  scene.add(new THREE.PointLight(0x7ce9ff, 160, 60, 2));

  for (let i = 0; i < 3; i++) {
    const ring = new THREE.Mesh(
      new THREE.TorusGeometry(3.6 + i * 1.15, 0.012, 6, 150),
      new THREE.MeshBasicMaterial({
        color: i === 1 ? 0xff5cc8 : 0x8ef4ff,
        transparent: true, opacity: 0.3, toneMapped: false,
        blending: THREE.AdditiveBlending,
      })
    );
    ring.rotation.set(Math.random() * 3, Math.random() * 3, Math.random() * 3);
    ring.userData.spin = 0.1 + Math.random() * 0.2;
    scene.add(ring);
    rings.push(ring);
  }
}

function buildStars() {
  const N = 2400;
  const pos = new Float32Array(N * 3);
  const col = new Float32Array(N * 3);
  const c = new THREE.Color();
  for (let i = 0; i < N; i++) {
    const r = 110 + Math.random() * 260;
    const t = Math.random() * Math.PI * 2;
    const p = Math.acos(2 * Math.random() - 1);
    pos[i * 3] = r * Math.sin(p) * Math.cos(t);
    pos[i * 3 + 1] = r * Math.cos(p);
    pos[i * 3 + 2] = r * Math.sin(p) * Math.sin(t);
    const h = Math.random();
    if (h > 0.9) c.setHex(0xff8ad8);
    else if (h > 0.72) c.setHex(0x8ec9ff);
    else c.setHex(0xe8f6ff);
    c.multiplyScalar(0.62 + Math.random() * 0.8);
    col[i * 3] = c.r;
    col[i * 3 + 1] = c.g;
    col[i * 3 + 2] = c.b;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  scene.add(new THREE.Points(g, new THREE.PointsMaterial({
    size: 2.1, sizeAttenuation: true, vertexColors: true, map: DOT,
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    toneMapped: false, opacity: 0.95,
  })));

  const colors = [0x2632c8, 0xa02074, 0x1086a8, 0x4a20b0];
  for (let i = 0; i < 5; i++) {
    const spr = new THREE.Sprite(new THREE.SpriteMaterial({
      map: GLOW, color: colors[i % colors.length], blending: THREE.AdditiveBlending,
      transparent: true, opacity: 0.075, depthWrite: false, toneMapped: false,
    }));
    const r = 150 + Math.random() * 90;
    spr.position.set(
      Math.cos(i * 1.9) * r, (Math.random() - 0.5) * 90, Math.sin(i * 1.9) * r
    );
    spr.scale.setScalar(55 + Math.random() * 55);
    scene.add(spr);
  }
}

let headLight;
function buildLights() {
  scene.add(new THREE.AmbientLight(0x54708f, 0.34));
  scene.add(new THREE.HemisphereLight(0x86c8f0, 0x1a0a2c, 0.62));

  const key = new THREE.DirectionalLight(0xbfe9ff, 1.15);
  key.position.set(18, 40, 22);
  scene.add(key);

  const rim = new THREE.DirectionalLight(0xff5cc8, 0.75);
  rim.position.set(-26, -30, -18);
  scene.add(rim);

  headLight = new THREE.DirectionalLight(0xd9f7ff, 0.5);
  headLight.position.set(0, 3, 0);
  scene.add(headLight);
  scene.add(headLight.target);
}

/* -------------------------------- текст --------------------------------- */

function makeTextMesh(text, { px = 190, color = '#dff6ff', track = 0, weight = 600 } = {}) {
  const W = 1024;
  const H = 256;
  const c = document.createElement('canvas');
  c.width = W;
  c.height = H;
  const g = c.getContext('2d');
  g.clearRect(0, 0, W, H);
  g.font = `${weight} ${px}px ui-monospace, Menlo, Consolas, monospace`;
  g.fillStyle = color;
  g.textAlign = 'center';
  g.textBaseline = 'middle';
  g.shadowColor = color;
  g.shadowBlur = 26;
  if ('letterSpacing' in g) g.letterSpacing = `${track}px`;
  g.fillText(text, W / 2, H / 2 + px * 0.04);
  g.fillText(text, W / 2, H / 2 + px * 0.04);
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 4;
  return new THREE.Mesh(
    new THREE.PlaneGeometry(4, 1),
    new THREE.MeshBasicMaterial({
      map: tex, transparent: true, depthWrite: false, side: THREE.DoubleSide,
      toneMapped: false, blending: THREE.AdditiveBlending,
    })
  );
}

/* ------------------------------- разломы -------------------------------- */

const rifts = [];

function buildRift(absU) {
  const group = new THREE.Group();
  const mat = new THREE.MeshBasicMaterial({
    map: STREAK, color: 0x4fc4ef, transparent: true, opacity: 0.42,
    blending: THREE.AdditiveBlending, toneMapped: false,
    side: THREE.DoubleSide, depthWrite: false,
  });
  const curtain = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), mat);
  const tf = transformFor(absU, 0, 1, 0);
  curtain.position.copy(tf.pos);
  curtain.quaternion.copy(tf.quat);
  // локально: X = поперёк, Y = нормаль → завеса поперёк ленты, по высоте ±RIFT_H
  curtain.scale.set(HALF_W * 2, RIFT_H * 2, 1);
  group.add(curtain);

  // нижняя кромка — яркая линия по поверхности
  const edgeMat = new THREE.MeshBasicMaterial({
    color: 0xbff4ff, transparent: true, opacity: 0.55,
    blending: THREE.AdditiveBlending, toneMapped: false,
  });
  const edge = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), edgeMat);
  edge.position.copy(tf.pos);
  edge.quaternion.copy(tf.quat);
  edge.scale.set(HALF_W * 2, 0.07, 1);
  group.add(edge);

  scene.add(group);
  rifts.push({ u: absU, mat, group });
}

/* --------------------------------- врата -------------------------------- */

const gates = [];
const DECAL_CYAN = new THREE.Color(0x8ef4ff);
const DECAL_PINK = new THREE.Color(0xff86c8);
const MEM_OPEN = new THREE.Color(0x2ed6ff);
const MEM_SHUT = new THREE.Color(0xff2f6a);

function buildGate(absU) {
  const group = new THREE.Group();
  const tf = transformFor(absU, 0, GATE_SIGN, 0);
  group.position.copy(tf.pos);
  group.quaternion.copy(tf.quat);
  // локально: X — поперёк, Y — «верх» стороны врат (-normal), Z — вдоль ленты

  const frameMat = new THREE.MeshStandardMaterial({
    color: 0x0b2230, metalness: 0.9, roughness: 0.25,
    emissive: 0x1a6d88, emissiveIntensity: 1.4,
  });
  [-1, 1].forEach((s) => {
    const p = new THREE.Mesh(new THREE.BoxGeometry(0.26, 4.1, 0.26), frameMat);
    p.position.set(s * (HALF_W - 0.2), 2.05, 0);
    group.add(p);
  });
  const top = new THREE.Mesh(new THREE.BoxGeometry(HALF_W * 2, 0.28, 0.28), frameMat);
  top.position.set(0, 4.1, 0);
  group.add(top);

  const membraneMat = new THREE.MeshBasicMaterial({
    map: STREAK, color: 0x1a4160, transparent: true, opacity: 0.3,
    side: THREE.DoubleSide, blending: THREE.AdditiveBlending,
    toneMapped: false, depthWrite: false,
  });
  const membrane = new THREE.Mesh(
    new THREE.PlaneGeometry(HALF_W * 2 - 0.45, 3.9), membraneMat
  );
  membrane.position.set(0, 2.05, 0);
  group.add(membrane);

  const label = makeTextMesh('ВРАТА', { px: 200, color: '#bff6ff', track: 30 });
  label.scale.set(1.85, 0.4625, 1);
  label.position.set(0, 4.62, 0);
  group.add(label);

  const light = new THREE.PointLight(0x59f0ff, 0, 18, 2);
  light.position.set(0, 2.2, 0);
  group.add(light);

  scene.add(group);
  gates.push({ u: absU, group, membrane, membraneMat, frameMat, light });
}

// Надписи на ленте: «НИЗ» читается сверху, «ВВЕРХ» — снизу.
const decals = [];
function buildDecal(text, absU, v, readFrom) {
  const tf = transformFor(absU, v, 1, 0);
  const f = surfaceFrame(absU, v, {});
  const mesh = makeTextMesh(text, {
    px: 200, color: readFrom > 0 ? '#ff86c8' : '#8ef4ff', track: 18,
  });
  mesh.scale.set(0.95, 0.2375, 1);
  mesh.position.copy(tf.pos).addScaledVector(f.normal, readFrom * 0.014);

  // Плоскость лежит на ленте: локальные X,Y — вдоль поверхности, Z — нормаль.
  // Читаемость: с +normal → X = -side, Z = +normal; с -normal → X = side, Z = -normal.
  const m = new THREE.Matrix4().makeBasis(
    readFrom > 0 ? f.side.clone().negate() : f.side.clone(),
    f.tangent.clone(),
    readFrom > 0 ? f.normal.clone() : f.normal.clone().negate()
  );
  mesh.quaternion.setFromRotationMatrix(m);
  scene.add(mesh);
  decals.push(mesh);
}

/* ------------------------------ сферы/шипы ------------------------------ */

const orbs = [];
function buildOrb(absU, v) {
  const tf = transformFor(absU, v, 1, 0);
  const mesh = new THREE.Mesh(
    new THREE.SphereGeometry(0.17, 14, 10),
    new THREE.MeshBasicMaterial({ color: 0x9ff4ff, toneMapped: false })
  );
  mesh.position.copy(tf.pos);
  const halo = new THREE.Sprite(new THREE.SpriteMaterial({
    map: GLOW, color: 0x63e8ff, blending: THREE.AdditiveBlending,
    transparent: true, opacity: 0.5, depthWrite: false, toneMapped: false,
  }));
  halo.scale.setScalar(0.85);
  mesh.add(halo);
  scene.add(mesh);
  orbs.push({ u: absU, v, mesh, taken: false, phase: Math.random() * 6.28 });
}

const spikes = [];
function buildSpike(absU, v) {
  const tf = transformFor(absU, v, 1, 0);
  const mesh = new THREE.Mesh(
    new THREE.OctahedronGeometry(0.5, 0),
    new THREE.MeshStandardMaterial({
      color: 0x2a0b18, emissive: 0xff2f6a, emissiveIntensity: 2.2,
      metalness: 0.6, roughness: 0.3, flatShading: true,
    })
  );
  mesh.scale.set(0.6, 1.3, 0.6);
  mesh.position.copy(tf.pos);
  mesh.quaternion.copy(tf.quat);
  scene.add(mesh);
  spikes.push({ u: absU, v, mesh, phase: Math.random() * 6.28 });
}

/* -------------------------------- расклад ------------------------------- */

const ORB_LAYOUT = [
  [0.30, -0.9], [0.42, 0.0], [0.55, 0.95], [0.70, 1.5],
  [1.15, -1.4], [1.28, -0.5], [1.40, 0.4], [1.52, 1.3],
  [1.90, 0.0], [2.10, -1.2], [2.30, 1.2],
  [2.85, -0.6], [3.02, 0.3], [3.22, 1.1],
  [3.85, 0.0], [4.25, -1.4], [4.65, 1.4], [5.05, 0.0],
  [5.45, -1.0], [5.80, 1.0], [6.10, 0.0],
];

const SPIKE_LAYOUT = [
  [1.98, 1.3], [2.12, -1.3], [2.32, 0.95],
  [4.40, -0.95], [4.55, 0.95], [5.20, 0.0], [5.35, 1.45],
];

/* ================================= мир ================================= */

buildRibbon();
buildEdges();
buildRungs();
buildCore();
buildStars();
buildLights();

for (let lap = 0; lap < MAX_LAP; lap++) {
  const base = lap * LAP;
  buildRift(base + RIFT_A);
  buildRift(base + RIFT_B);
  buildGate(base + GATE_U);
  buildDecal('НИЗ', base + GATE_U - 0.62, 0, 1);
  buildDecal('ВВЕРХ', base + GATE_U - 0.62, 0, -1);
  for (const [u, v] of ORB_LAYOUT) buildOrb(base + u, mirrorV(v, lap));
  for (const [u, v] of SPIKE_LAYOUT) buildSpike(base + u, mirrorV(v, lap));
}

/* ================================ игрок ================================ */

const player = {
  u: 0.12, v: 0, h: 0, vv: 0, vh: 0,
  sign: 1, roll: 0, grounded: true, speedMul: 1, hitCool: 0,
  runT: 0,
};

const playerGroup = new THREE.Group();
const bodyMat = new THREE.MeshStandardMaterial({
  color: 0x07161f, metalness: 0.75, roughness: 0.34,
  emissive: 0x0a5f7a, emissiveIntensity: 0.5,
});
const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.31, 0.46, 8, 16), bodyMat);
body.rotation.x = Math.PI / 2;
body.position.y = 0.62;
playerGroup.add(body);

// светящиеся «позвонки» на спине
for (let i = 0; i < 3; i++) {
  const seg = new THREE.Mesh(
    new THREE.BoxGeometry(0.1, 0.05, 0.16),
    new THREE.MeshBasicMaterial({ color: 0x8ef4ff, toneMapped: false })
  );
  seg.position.set(0, 0.62 + 0.16 - i * 0.16, -0.16 + i * 0.05);
  playerGroup.add(seg);
}

const visor = new THREE.Mesh(
  new THREE.SphereGeometry(0.135, 14, 10),
  new THREE.MeshBasicMaterial({ color: 0xd8fbff, toneMapped: false })
);
visor.scale.set(1, 0.8, 0.7);
visor.position.set(0, 0.74, 0.3);
playerGroup.add(visor);

const playerRing = new THREE.Mesh(
  new THREE.TorusGeometry(0.58, 0.016, 6, 44),
  new THREE.MeshBasicMaterial({
    color: 0x8ef4ff, transparent: true, opacity: 0.85,
    toneMapped: false, blending: THREE.AdditiveBlending,
  })
);
playerRing.position.y = 0.62;
playerRing.rotation.x = Math.PI / 2;
playerGroup.add(playerRing);

const playerLight = new THREE.PointLight(0x7ce9ff, 4, 8, 2);
playerLight.position.y = 1.0;
playerGroup.add(playerLight);

const aura = new THREE.Sprite(new THREE.SpriteMaterial({
  map: GLOW, color: 0x7ce9ff, blending: THREE.AdditiveBlending,
  transparent: true, opacity: 0.14, depthWrite: false, toneMapped: false,
}));
aura.scale.setScalar(2.1);
aura.position.y = 0.62;
playerGroup.add(aura);

scene.add(playerGroup);

// волна переворота
const flipRing = new THREE.Mesh(
  new THREE.TorusGeometry(1, 0.035, 8, 64),
  new THREE.MeshBasicMaterial({
    color: 0x9ff4ff, transparent: true, opacity: 0,
    blending: THREE.AdditiveBlending, toneMapped: false, depthWrite: false,
  })
);
flipRing.visible = false;
scene.add(flipRing);
let flipRingT = 0;
let flipRingSign = 1;

/* -------------------------------- шлейф --------------------------------- */

const TRAIL_N = 120;
const trailPos = new Float32Array(TRAIL_N * 3);
const trailCol = new Float32Array(TRAIL_N * 3);
const trailLife = new Float32Array(TRAIL_N);
const trailGeo = new THREE.BufferGeometry();
trailGeo.setAttribute('position', new THREE.BufferAttribute(trailPos, 3));
trailGeo.setAttribute('color', new THREE.BufferAttribute(trailCol, 3));
const trailPts = new THREE.Points(trailGeo, new THREE.PointsMaterial({
  size: 0.34, sizeAttenuation: true, vertexColors: true, map: DOT,
  transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
  toneMapped: false,
}));
trailPts.frustumCulled = false;
scene.add(trailPts);
let trailHead = 0;

/* -------------------------------- состояние ----------------------------- */

const safeStore = {
  get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
  set(k, v) { try { localStorage.setItem(k, String(v)); } catch (e) { /* ignore */ } },
};

const state = {
  phase: 'title',
  t: 0,
  runTime: 0,
  orbs: 0,
  lap: 0,
  shake: 0,
  slow: 0,
  intro: 0,
  introDone: false,
  flags: {},
  cam: { back: 5.0, up: 3.0, ahead: 5.0, lookUp: 0.5 },
  best: Number(safeStore.get('mobius-best') || 0) || 0,
};

const camPos = new THREE.Vector3(0, 16, 32);
const camUp = new THREE.Vector3(0, 1, 0);
const camLook = new THREE.Vector3(0, 0, 0);

/* -------------------------------- ввод ---------------------------------- */

const input = { left: false, right: false, jumpBuf: 0 };

addEventListener('keydown', (e) => {
  if (e.code === 'Space' || e.code === 'ArrowUp' || e.code === 'KeyW') {
    e.preventDefault();
    if (state.phase === 'title') startGame();
    else if (state.phase === 'win') restart();
    else input.jumpBuf = 0.15;
  }
  if (e.code === 'KeyA' || e.code === 'ArrowLeft') input.left = true;
  if (e.code === 'KeyD' || e.code === 'ArrowRight') input.right = true;
  if (e.code === 'KeyR') restart();
  if (e.code === 'KeyM') toggleMute();
  Sfx.unlock();
}, { passive: false });

const release = (e) => {
  if (e.code === 'KeyA' || e.code === 'ArrowLeft') input.left = false;
  if (e.code === 'KeyD' || e.code === 'ArrowRight') input.right = false;
};
addEventListener('keyup', release);
addEventListener('blur', () => { input.left = false; input.right = false; });

const touchEl = document.getElementById('touch');
if ('ontouchstart' in window || navigator.maxTouchPoints > 0) touchEl.classList.add('on');

touchEl.querySelectorAll('.tbtn').forEach((btn) => {
  const k = btn.dataset.k;
  const on = (e) => {
    e.preventDefault();
    if (k === 'left') input.left = true;
    else if (k === 'right') input.right = true;
    else if (state.phase === 'title') startGame();
    else if (state.phase === 'win') restart();
    else input.jumpBuf = 0.15;
  };
  const off = (e) => {
    e.preventDefault();
    if (k === 'left') input.left = false;
    if (k === 'right') input.right = false;
  };
  btn.addEventListener('pointerdown', on);
  btn.addEventListener('pointerup', off);
  btn.addEventListener('pointercancel', off);
  btn.addEventListener('pointerleave', off);
});

canvas.addEventListener('pointerdown', (e) => {
  if (e.button !== 0 && e.pointerType === 'mouse') return;
  if (state.phase === 'play') input.jumpBuf = 0.15;
});

/* ---------------------------------- UI ---------------------------------- */

const el = {
  sOrb: document.getElementById('sOrb'),
  sLap: document.getElementById('sLap'),
  sTime: document.getElementById('sTime'),
  chip: document.getElementById('chipWrap'),
  chipText: document.getElementById('chipText'),
  toast: document.getElementById('toast'),
  screen: document.getElementById('screen'),
  scTitle: document.getElementById('scTitle'),
  scLede: document.getElementById('scLede'),
  scRules: document.getElementById('scRules'),
  scStats: document.getElementById('scStats'),
  scBtn: document.getElementById('scBtn'),
  scFoot: document.getElementById('scFoot'),
  flash: document.getElementById('flash'),
  muteBtn: document.getElementById('muteBtn'),
  hint: document.getElementById('hint'),
  lapbar: document.getElementById('lapbar'),
  lapDot: document.getElementById('lapDot'),
  intro: document.getElementById('intro'),
};

const LAP_TICKS = [];
for (const [u, kind] of [[RIFT_A, ''], [RIFT_B, ''], [GATE_U, 'gate']]) {
  const t = document.createElement('div');
  t.className = 'tick ' + kind;
  t.style.left = `${((u / (Math.PI * 2)) * 100).toFixed(2)}%`;
  el.lapbar.appendChild(t);
  LAP_TICKS.push(t);
}

function updateLapBar() {
  const p = ((player.u % (Math.PI * 2)) + Math.PI * 2) % (Math.PI * 2);
  el.lapDot.style.left = `${((p / (Math.PI * 2)) * 100).toFixed(2)}%`;
  el.lapbar.classList.toggle('flip', player.sign < 0);
  el.lapbar.classList.toggle('on', state.phase !== 'title');
}

let toastTimer = 0;
function toast(html, ms = 2600, pink = false) {
  el.toast.innerHTML = html;
  el.toast.classList.toggle('pink', pink);
  el.toast.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.toast.classList.remove('show'), ms);
}

function setChip() {
  const flipped = player.sign < 0;
  el.chip.classList.toggle('flipped', flipped);
  el.chipText.textContent = flipped ? 'верх ногами' : 'нормально';
}

function toggleMute() {
  const m = Sfx.toggleMute();
  el.muteBtn.textContent = m ? '✕' : '♪';
  el.muteBtn.style.opacity = m ? 0.5 : 1;
}

el.muteBtn.addEventListener('click', (e) => {
  e.stopPropagation();
  toggleMute();
  el.muteBtn.blur();
});

el.scBtn.addEventListener('click', () => {
  if (state.phase === 'title') startGame();
  else restart();
});

/* -------------------------------- фазы ---------------------------------- */

const START_TEXT = 'пробел — <span class="k">прыжок</span> · a/d — влево-вправо';

function startGame() {
  Sfx.unlock();
  player.speedMul = 0.04;
  state.phase = 'play';
  state.runTime = 0;
  state.orbs = 0;
  state.flags = {};
  state.slow = 0;
  el.screen.classList.remove('show');
  el.chip.classList.remove('hidden');
  // кинематографичный въезд: камера приходит снаружи
  const f = surfaceFrame(player.u, player.v, {});
  camPos.copy(f.pos)
    .addScaledVector(f.tangent, -state.cam.back * 2.6)
    .addScaledVector(f.normal, state.cam.up * 2.3);
  camLook.copy(f.pos);
  if (!state.introDone) {
    state.introDone = true;
    state.intro = 2.7;
    el.intro.classList.add('on');
    el.lapbar.classList.add('on');
    setTimeout(() => el.intro.classList.remove('on'), 2200);
    setTimeout(() => toast(START_TEXT, 2600), 2300);
  } else {
    toast(START_TEXT, 2400);
  }
  Sfx.gateHint();
}

function restart() {
  resetRun();
  player.speedMul = 0.04;
  state.phase = 'play';
  state.runTime = 0;
  state.orbs = 0;
  state.flags = {};
  state.slow = 0;
  el.screen.classList.remove('show');
  el.chip.classList.remove('hidden');
  toast(START_TEXT, 2200);
}

function resetRun() {
  player.u = 0.12;
  player.v = 0;
  player.h = 0;
  player.vv = 0;
  player.vh = 0;
  player.sign = 1;
  player.roll = 0;
  player.grounded = true;
  player.speedMul = 1;
  player.hitCool = 0;
  player.runT = 0;
  input.left = input.right = false;
  input.jumpBuf = 0;
  orbs.forEach((o) => { o.taken = false; o.mesh.visible = true; });
  trailLife.fill(0);
  setChip();
  const f = surfaceFrame(player.u, player.v, {});
  camPos.copy(f.pos).addScaledVector(f.tangent, -state.cam.back).addScaledVector(f.normal, state.cam.up);
  camUp.copy(f.normal);
  camLook.copy(f.pos);
}

function win() {
  state.phase = 'win';
  Sfx.win();
  setChip();
  el.chip.classList.remove('flipped');
  if (!state.best || state.runTime < state.best) {
    state.best = state.runTime;
    safeStore.set('mobius-best', state.best.toFixed(2));
  }
  el.scTitle.textContent = 'ВРАТА ОТКРЫЛИСЬ';
  el.scTitle.classList.add('small');
  el.scLede.innerHTML =
    'Ты прошёл по «потолку» до самой арки, и мир пропустил тебя. ' +
    'Не потому что ты победил, а потому что <em>верх и низ здесь — одна и та же сторона</em>.';
  el.scRules.classList.add('hidden');
  el.scStats.classList.remove('hidden');
  el.scStats.innerHTML = `
    <div><b>${state.runTime.toFixed(1)}</b><span>секунд</span></div>
    <div><b>${state.orbs}</b><span>сфер</span></div>
    <div><b>${state.lap + 1}</b><span>виток</span></div>
    <div><b>${state.best.toFixed(1)}</b><span>рекорд</span></div>`;
  el.scBtn.textContent = 'ещё раз';
  el.scFoot.textContent = 'r — заново · m — звук';
  setTimeout(() => {
    if (state.phase === 'win') el.screen.classList.add('show');
  }, 1100);
}

function showTitle() {
  state.phase = 'title';
  el.scTitle.textContent = 'МЁБИУС';
  el.scTitle.classList.remove('small');
  el.scLede.innerHTML =
    'Беги по ленте Мёбиуса. У этого мира <em>нет низа</em>: один виток — и ты ' +
    'возвращаешься в ту же точку вверх ногами. Разломы переворачивают тебя. ' +
    'Врата открываются только для тех, кто идёт по «потолку».';
  el.scRules.classList.remove('hidden');
  el.scStats.classList.add('hidden');
  el.scBtn.textContent = 'начать';
  el.scFoot.textContent = state.best
    ? `рекорд: ${state.best.toFixed(1)} с · r — заново · m — звук`
    : 'r — заново · m — звук · наушники приветствуются';
  el.screen.classList.add('show');
  el.chip.classList.add('hidden');
  el.lapbar.classList.remove('on');
}

/* ------------------------------ физика ---------------------------------- */

const fNow = {};
const ppos = new THREE.Vector3();
const pNext = new THREE.Vector3();

function currentSpeed() {
  return BASE_SPEED + Math.min(state.lap, 6) * SPEED_PER_LAP;
}

function worldPos(p, out = new THREE.Vector3()) {
  const f = surfaceFrame(p.u, p.v, {});
  return out.copy(f.pos).addScaledVector(f.normal, p.sign * p.h);
}

function flip() {
  player.sign *= -1;
  player.h = -player.h;
  player.vh = -player.vh;
  if (player.h < 0) {
    player.h = 0;
    player.vh = 0;
    player.grounded = true;
  }
  state.shake = Math.max(state.shake, 0.3);
  state.slow = 0.55;
  flipRingT = 1;
  flipRingSign = player.sign;
  Sfx.flip();
  setChip();
  if (!state.flags.hintA) {
    state.flags.hintA = true;
    toast('ты <span class="k">перевёрнут</span>. верх и низ поменялись местами —<br>' +
      'а лента всё так же под ногами', 4200);
  }
}

function updatePlayer(dt) {
  const speed = currentSpeed() * player.speedMul;

  // ---- поперёк ленты ----
  let want = 0;
  if (input.left) want -= 1;
  if (input.right) want += 1;
  if (want !== 0) player.vv += want * V_ACC * dt;
  else player.vv *= Math.max(0, 1 - V_DAMP * dt);
  player.vv = THREE.MathUtils.clamp(player.vv, -V_MAX, V_MAX);
  player.v += player.vv * dt;
  const lim = HALF_W - 0.34;
  if (player.v > lim) { player.v = lim; player.vv *= -0.3; }
  if (player.v < -lim) { player.v = -lim; player.vv *= -0.3; }

  // ---- прыжок ----
  if (input.jumpBuf > 0) {
    input.jumpBuf -= dt;
    if (player.grounded) {
      player.vh = JUMP_V;
      player.grounded = false;
      input.jumpBuf = 0;
      Sfx.jump();
    }
  }

  // ---- вертикаль ----
  if (!player.grounded) {
    player.vh -= GRAV * dt;
    player.h += player.vh * dt;
    if (player.h <= 0) {
      player.h = 0;
      player.vh = 0;
      player.grounded = true;
    }
  }

  // ---- вперёд ----
  const probe = surfaceFrame(player.u, player.v, fNow);
  const uPrev = player.u;
  player.u += (speed * dt) / Math.max(2, probe.scale);

  // ---- разломы ----
  for (const r of rifts) {
    if (uPrev < r.u && player.u >= r.u) {
      if (player.h < RIFT_H) flip();
      else if (!state.flags.jumped) {
        state.flags.jumped = true;
        toast('разлом перепрыгнут — <span class="k">ты сохранил ориентацию</span>', 2200);
      }
    }
  }

  // ---- врата ----
  for (const g of gates) {
    if (uPrev < g.u && player.u >= g.u) {
      if (player.sign === GATE_SIGN) {
        updateGatesStatic();
        win();
        return;
      }
      Sfx.denied();
      state.shake = Math.max(state.shake, 0.6);
      flash(0.4);
      toast('врата закрыты: ты снова <span class="k">нормальный</span>.<br>' +
        'перепрыгни ближайший разлом — и развернись', 4200, true);
      if (!state.flags.denied) {
        state.flags.denied = true;
        setTimeout(() => {
          if (state.phase === 'play') {
            toast('врата стоят на «изнанке». в них входят <span class="k">вверх ногами</span>', 4200, true);
          }
        }, 4400);
      }
    }
  }

  // ---- сбор сфер ----
  worldPos(player, ppos);
  for (const o of orbs) {
    if (o.taken || Math.abs(o.u - player.u) > 0.4) continue;
    if (o.mesh.position.distanceToSquared(ppos) < 1.4) {
      o.taken = true;
      o.mesh.visible = false;
      state.orbs++;
      Sfx.pickup(state.orbs % 6);
    }
  }

  // ---- шипы ----
  if (player.hitCool > 0) player.hitCool -= dt;
  for (const s of spikes) {
    if (Math.abs(s.u - player.u) > 0.32) continue;
    if (s.mesh.position.distanceToSquared(ppos) < 1.1) {
      if (player.hitCool > 0) break;
      player.hitCool = 0.85;
      player.speedMul = 0.3;
      player.vv += player.v > 0 ? -3 : 3;
      state.shake = 0.85;
      flash(0.55);
      Sfx.hit();
      break;
    }
  }
  player.speedMul += (1 - player.speedMul) * (1 - Math.exp(-2.4 * dt));
  player.runT += dt;

  // ---- подсказка у второго разлома ----
  const lapBase = Math.floor(player.u / LAP) * LAP;
  const dB = player.u - (lapBase + RIFT_B);
  if (!state.flags.hintB && player.sign < 0 && dB > -1.5 && dB < -0.5) {
    state.flags.hintB = true;
    toast('впереди <span class="k">разлом</span>. перепрыгни его — иначе он перевернёт тебя обратно', 4200);
  }
  const dGate = (lapBase + GATE_U) - player.u;
  if (!state.flags.gateNear && dGate > 0 && dGate < 1.3 && player.sign === GATE_SIGN) {
    state.flags.gateNear = true;
    Sfx.gateHint();
    toast('врата видят тебя. входи', 2200);
  }

  state.lap = lapsOf(player.u);
}

/* ------------------------------ графика --------------------------------- */

const _roller = new THREE.Quaternion();
const _zAxis = new THREE.Vector3(0, 0, 1);
const upV = new THREE.Vector3();
const fwdV = new THREE.Vector3();
const camTarget = new THREE.Vector3();
const lookTarget = new THREE.Vector3();
const tmpV = new THREE.Vector3();
const tmpQ = new THREE.Quaternion();
const _cTmp = new THREE.Color();

function updateVisual(dt) {
  const f = surfaceFrame(player.u, player.v, fNow);
  const targetRoll = player.sign < 0 ? Math.PI : 0;
  player.roll += wrapPi(targetRoll - player.roll) * (1 - Math.exp(-9 * dt));
  if (Math.abs(wrapPi(targetRoll - player.roll)) < 0.002) player.roll = targetRoll;

  _q.setFromRotationMatrix(basisMatrix(player.u, player.v, 1));
  _roller.setFromAxisAngle(_zAxis, player.roll);
  _q.multiply(_roller);

  worldPos(player, tmpV);
  playerGroup.position.copy(tmpV);
  playerGroup.quaternion.copy(_q);

  const pulse = 0.85 + Math.sin(state.t * 3.2) * 0.15;
  aura.material.opacity = 0.13 * pulse;
  aura.scale.setScalar(2.1 + Math.sin(state.t * 2.1) * 0.25);
  playerRing.rotation.z += dt * 2.6;
  playerRing.scale.setScalar(1 + Math.sin(state.t * 4) * 0.05);
  playerLight.intensity = 3.6 + Math.sin(state.t * 5) * 0.9;

  upV.set(0, 1, 0).applyQuaternion(_q);
  fwdV.set(0, 0, 1).applyQuaternion(_q);

  if (state.phase !== 'win') {
    camTarget.copy(tmpV)
      .addScaledVector(fwdV, -state.cam.back)
      .addScaledVector(upV, state.cam.up);
    const followK = state.intro > 0 ? 1.6 : 7.5;
    camPos.lerp(camTarget, 1 - Math.exp(-followK * dt));
    camUp.lerp(upV, 1 - Math.exp(-7.5 * dt)).normalize();

    if (state.shake > 0.002) {
      state.shake *= Math.max(0, 1 - 7 * dt);
      camPos.x += (Math.random() - 0.5) * state.shake;
      camPos.y += (Math.random() - 0.5) * state.shake;
      camPos.z += (Math.random() - 0.5) * state.shake;
    }

    camera.position.copy(camPos);
    camera.up.copy(camUp);
    lookTarget.copy(tmpV)
      .addScaledVector(fwdV, state.cam.ahead)
      .addScaledVector(upV, state.cam.lookUp);
    camLook.lerp(lookTarget, 1 - Math.exp(-12 * dt));
    camera.lookAt(camLook);
  }

  headLight.position.copy(camera.position);
  headLight.target.position.copy(state.phase === 'win' ? tmpV : camLook);

  // шлейф
  for (let k = 0; k < 2; k++) {
    const i = trailHead;
    trailHead = (trailHead + 1) % TRAIL_N;
    trailPos[i * 3] = tmpV.x + (Math.random() - 0.5) * 0.45;
    trailPos[i * 3 + 1] = tmpV.y + (Math.random() - 0.5) * 0.45;
    trailPos[i * 3 + 2] = tmpV.z + (Math.random() - 0.5) * 0.45;
    trailLife[i] = 1;
  }
  updateTrail(dt);

  // волна от переворота
  if (flipRingT > 0) {
    flipRingT -= dt * 1.35;
    const k = Math.max(0, flipRingT);
    worldPos(player, tmpV);
    flipRing.position.copy(tmpV);
    const f = surfaceFrame(player.u, player.v, fNow);
    flipRing.quaternion.setFromRotationMatrix(
      new THREE.Matrix4().makeBasis(f.side, f.tangent, f.normal)
    );
    flipRing.scale.setScalar(0.6 + (1 - k) * 4.5);
    flipRing.material.opacity = k * 0.7;
    flipRing.material.color.set(flipRingSign < 0 ? 0xff8ad4 : 0x9ff4ff);
    flipRing.visible = true;
  } else {
    flipRing.visible = false;
  }

  // мир
  rings.forEach((r) => {
    r.rotation.x += r.userData.spin * dt;
    r.rotation.y += r.userData.spin * 0.7 * dt;
  });
  coreSpin[0].rotation.y += dt * 0.13;
  coreSpin[0].rotation.z += dt * 0.05;
  coreSpin[1].rotation.y -= dt * 0.07;
  coreSpin[1].rotation.x += dt * 0.04;
  orbs.forEach((o) => {
    if (!o.taken) o.mesh.rotation.y += dt * 1.7;
  });
  spikes.forEach((s) => {
    s.mesh.rotation.y += dt * 1.2;
  });

  STREAK.offset.x = state.t * 0.06;
  STREAK.offset.y = state.t * 0.02;
  rifts.forEach((r) => {
    r.mat.opacity = 0.38 + Math.sin(state.t * 4.6) * 0.1;
  });

  gates.forEach((g) => {
    const near = Math.abs(g.u - player.u) < 12;
    const aligned = player.sign === GATE_SIGN;
    const targetOpacity = aligned
      ? 0.46 + Math.sin(state.t * 4) * 0.1
      : 0.16;
    g.membraneMat.opacity += (targetOpacity - g.membraneMat.opacity) * Math.min(1, dt * 4);
    _cTmp.copy(aligned ? MEM_OPEN : MEM_SHUT);
    g.membraneMat.color.lerp(_cTmp, Math.min(1, dt * 3));
    g.frameMat.emissiveIntensity = 1.0 + (aligned ? 1.3 : 0);
    g.light.intensity += ((aligned ? 45 : 5) - g.light.intensity) * Math.min(1, dt * 4);
    g.membrane.visible = near;
  });
}

function updateGatesStatic() {
  gates.forEach((g) => {
    g.membraneMat.color.set(0x8ef4ff);
    g.membraneMat.opacity = 0.55;
    g.light.intensity = 55;
  });
}

const _tc = new THREE.Color();
function updateTrail(dt) {
  for (let i = 0; i < TRAIL_N; i++) {
    if (trailLife[i] <= 0) {
      trailCol[i * 3] = trailCol[i * 3 + 1] = trailCol[i * 3 + 2] = 0;
      continue;
    }
    trailLife[i] -= dt * 1.5;
    const l = Math.max(0, trailLife[i]);
    _tc.setRGB(0.3 * l, 0.85 * l, 1.0 * l);
    trailCol[i * 3] = _tc.r;
    trailCol[i * 3 + 1] = _tc.g;
    trailCol[i * 3 + 2] = _tc.b;
  }
  trailGeo.attributes.position.needsUpdate = true;
  trailGeo.attributes.color.needsUpdate = true;
}

function flash(a) {
  el.flash.style.transition = 'none';
  el.flash.style.opacity = String(a);
  requestAnimationFrame(() => {
    el.flash.style.transition = 'opacity .55s';
    el.flash.style.opacity = '0';
  });
}

/* ------------------------------- камера --------------------------------- */

function titleCamera(dt) {
  const a = state.t * 0.07;
  const r = 27 + Math.sin(state.t * 0.19) * 5;
  const y = 9 + Math.sin(state.t * 0.13) * 7;
  camTarget.set(Math.cos(a) * r, y, Math.sin(a) * r);
  camPos.lerp(camTarget, 1 - Math.exp(-1.1 * dt));
  camUp.lerp(tmpV.set(0, 1, 0), 1 - Math.exp(-2 * dt)).normalize();
  camera.position.copy(camPos);
  camera.up.copy(camUp);
  camera.lookAt(0, 0, 0);
}

/* ------------------------------- рендер --------------------------------- */

let composer = null;
try {
  composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));
  composer.addPass(new UnrealBloomPass(new THREE.Vector2(1, 1), 0.42, 0.55, 0.9));
  composer.addPass(new OutputPass());
} catch (e) {
  composer = null;
}

function resize() {
  const w = innerWidth;
  const h = innerHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  if (composer) composer.setSize(w, h);
}
addEventListener('resize', resize);
resize();

const clock = new THREE.Clock();

function frame() {
  const dt = Math.min(clock.getDelta(), 0.05);
  state.t += dt;

  if (state.phase === 'title') {
    worldPos(player, tmpV);
    playerGroup.position.copy(tmpV);
    _q.setFromRotationMatrix(basisMatrix(player.u, player.v, 1));
    playerGroup.quaternion.copy(_q);
    titleCamera(dt);
    for (let k = 0; k < 1; k++) {
      const i = trailHead;
      trailHead = (trailHead + 1) % TRAIL_N;
      trailPos[i * 3] = tmpV.x;
      trailPos[i * 3 + 1] = tmpV.y + 0.4;
      trailPos[i * 3 + 2] = tmpV.z;
      trailLife[i] = 1;
    }
    updateTrail(dt);
    rings.forEach((r) => { r.rotation.x += r.userData.spin * dt; });
    rifts.forEach((r) => { r.mat.opacity = 0.38 + Math.sin(state.t * 4.6) * 0.1; });
    gates.forEach((g) => {
      g.membraneMat.opacity = 0.3 + Math.sin(state.t * 3) * 0.08;
    });
    orbs.forEach((o) => { o.mesh.rotation.y += dt * 1.7; });
    STREAK.offset.x = state.t * 0.06;
  } else if (state.phase === 'play') {
    let sdt = dt;
    if (state.slow > 0) {
      state.slow -= dt;
      sdt = dt * 0.32;
    }
    state.runTime += dt;
    if (state.intro > 0) state.intro -= dt;
    updatePlayer(sdt);
    if (state.phase === 'play' || state.phase === 'win') updateVisual(dt);
  } else if (state.phase === 'win') {
    updateVisual(dt);
    const f = surfaceFrame(player.u, player.v, fNow);
    const c = worldPos(player, tmpV);
    const a = state.t * 0.55;
    camTarget.copy(c)
      .addScaledVector(f.side, Math.cos(a) * 6.8)
      .addScaledVector(f.normal, 2.6 + Math.sin(state.t * 0.7) * 1.3)
      .addScaledVector(f.tangent, Math.sin(a) * 6.8);
    camPos.lerp(camTarget, 1 - Math.exp(-2 * dt));
    camera.position.copy(camPos);
    camera.up.lerp(tmpV2.copy(f.normal).multiplyScalar(player.sign), 1 - Math.exp(-2 * dt)).normalize();
    camera.lookAt(c);
    updateTrail(dt);
    rings.forEach((r) => { r.rotation.x += r.userData.spin * dt * 2; });
  }

  updateLapBar();
  // подсказки по управлению уходят, когда игрок освоился
  el.hint.style.opacity = state.phase === 'play' && state.runTime > 22 ? '0.14' : '';
  el.sOrb.textContent = String(state.orbs);
  el.sLap.textContent = String(state.lap + 1);
  el.sTime.textContent = state.runTime.toFixed(1);

  if (composer) composer.render();
  else renderer.render(scene, camera);
  requestAnimationFrame(frame);
}

/* ------------------------------- запуск --------------------------------- */

resetRun();
showTitle();
setChip();
Sfx.startDrone();
requestAnimationFrame(frame);

document.addEventListener('visibilitychange', () => {
  if (!document.hidden) clock.getDelta();
});

addEventListener('pointerdown', () => Sfx.unlock(), { once: true });

// Небольшой отладочный люк (используется тестами и любопытными).
window.__MOBIUS = { player, state, worldPos, surfaceFrame, LAP, camera, scene };
// мгновенно перенести игрока (используется в тестах)
window.__MOBIUS.teleport = (u, sign = 1, v = 0) => {
  player.u = u; player.sign = sign; player.v = v; player.h = 0; player.vh = 0;
  player.roll = sign < 0 ? Math.PI : 0; setChip();
  const f = surfaceFrame(player.u, player.v, {});
  camPos.copy(f.pos).addScaledVector(f.tangent, -state.cam.back)
    .addScaledVector(f.normal, state.cam.up * sign);
  camUp.copy(f.normal).multiplyScalar(sign);
};
