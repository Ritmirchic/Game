// Dev tool: offline wireframe preview of a camera view (no browser needed).
// usage: node tools/view-preview.mjs [out.png] [view]
import fs from 'node:fs';
import zlib from 'node:zlib';

globalThis.window = globalThis;
await import('../src/world.js');
const { City, CFG } = globalThis.MEM;

/* ---- tiny mat4 (same conventions as src/gl.js) ---- */
const M4 = {
  perspective(fovy, aspect, near, far) {
    const f = 1 / Math.tan(fovy / 2), nf = 1 / (near - far);
    return [f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) * nf, -1, 0, 0, 2 * far * near * nf, 0];
  },
  lookAt(eye, center, up) {
    let zx = eye[0] - center[0], zy = eye[1] - center[1], zz = eye[2] - center[2];
    const zl = Math.hypot(zx, zy, zz) || 1; zx /= zl; zy /= zl; zz /= zl;
    let xx = up[1] * zz - up[2] * zy, xy = up[2] * zx - up[0] * zz, xz = up[0] * zy - up[1] * zx;
    const xl = Math.hypot(xx, xy, xz) || 1; xx /= xl; xy /= xl; xz /= xl;
    const yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
    return [xx, yx, zx, 0, xy, yy, zy, 0, xz, yz, zz, 0,
      -(xx * eye[0] + xy * eye[1] + xz * eye[2]),
      -(yx * eye[0] + yy * eye[1] + yz * eye[2]),
      -(zx * eye[0] + zy * eye[1] + zz * eye[2]), 1];
  },
  mul(a, b) {
    const o = new Array(16);
    for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++)
      o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    return o;
  }
};

const W = 1100, H = 620;
const img = new Float32Array(W * H * 3);
function add(x, y, r, g, b) {
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  const i = (y * W + x) * 3;
  img[i] += r; img[i + 1] += g; img[i + 2] += b;
}

const city = new City(7);
city.reveal(0, 26, 13, 0);

const out = process.argv[2] || 'tools/view.png';
const view = process.argv[3] || 'start';
let eye, center;
if (view === 'menu') { eye = [Math.cos(0.6) * 34, 10, Math.sin(0.6) * 34]; center = [0, 12, 0]; }
else if (view === 'fly') { eye = [0, 26, 34]; center = [0, 6, -6]; }
else { eye = [0, CFG.EYE, 26]; center = [0, CFG.EYE + 12, 0]; }

const near = 0.08, far = 420, fov = 74 * Math.PI / 180;
const proj = M4.perspective(fov, W / H, near, far);
const view4 = M4.lookAt(eye, center, [0, 1, 0]);
const vp = M4.mul(proj, view4);

function project(p) {
  const x = p[0], y = p[1], z = p[2];
  const cx = vp[0] * x + vp[4] * y + vp[8] * z + vp[12];
  const cy = vp[1] * x + vp[5] * y + vp[9] * z + vp[13];
  const cz = vp[2] * x + vp[6] * y + vp[10] * z + vp[14];
  const cw = vp[3] * x + vp[7] * y + vp[11] * z + vp[15];
  return [cx, cy, cz, cw];
}

function seg(a, b, r, g, bl, clip) {
  let A = project(a), B = project(b);
  if (clip) {
    // cut the segment at y = clip
    if ((a[1] - clip) * (b[1] - clip) < 0) {
      const t = (clip - a[1]) / (b[1] - a[1]);
      const m = [a[0] + (b[0] - a[0]) * t, clip, a[2] + (b[2] - a[2]) * t];
      if (a[1] > clip) { a = m; A = project(m); } else { b = m; B = project(m); }
    } else if (a[1] > clip || b[1] > clip) return;
  }
  let ax = A[0], ay = A[1], aw = A[3], bx = B[0], by = B[1], bw = B[3];
  if (aw < near && bw < near) return;
  if (aw < near) { const t = (near - aw) / (bw - aw); ax += (bx - ax) * t; ay += (by - ay) * t; aw = near; }
  if (bw < near) { const t = (near - bw) / (aw - bw); bx += (ax - bx) * t; by += (ay - by) * t; bw = near; }
  const sx0 = (ax / aw * 0.5 + 0.5) * W, sy0 = (1 - (ay / aw * 0.5 + 0.5)) * H;
  const sx1 = (bx / bw * 0.5 + 0.5) * W, sy1 = (1 - (by / bw * 0.5 + 0.5)) * H;
  const n = Math.ceil(Math.hypot(sx1 - sx0, sy1 - sy0));
  if (n > 4000) return;
  for (let i = 0; i <= n; i++) {
    const t = n ? i / n : 0;
    add(Math.round(sx0 + (sx1 - sx0) * t), Math.round(sy0 + (sy1 - sy0) * t), r, g, bl);
  }
}

const CORN = [[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]];
const EDGES = [[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]];

for (const s of city.structures) {
  const tint = s.tint;
  for (const p of s.parts) {
    const c = Math.cos(p.rot), si = Math.sin(p.rot);
    const pts = CORN.map(([sx, sy, sz]) => {
      const lx = sx * p.hx, lz = sz * p.hz;
      return [p.x + lx * c - lz * si, p.y + sy * p.hy, p.z + lx * si + lz * c];
    });
    const glow = p.tint ? 1 : 0;
    for (const e of EDGES) {
      const a = pts[e[0]], b = pts[e[1]];
      const d = Math.hypot((a[0] + b[0]) / 2 - eye[0], (a[1] + b[1]) / 2 - eye[1], (a[2] + b[2]) / 2 - eye[2]);
      const fade = Math.exp(-d * 0.0115);
      const k = 0.20 + 0.35 * glow;
      seg(a, b, (tint[0] * 0.5 + 0.5) * fade * k * 1.0, (tint[1] * 0.5 + 0.6) * fade * k * 1.2, (tint[2] * 0.5 + 0.7) * fade * k * 1.4);
    }
  }
}
// floor grid
for (let i = -46; i <= 46; i += 2) {
  const major = i % 12 === 0;
  const k = major ? 0.20 : 0.06;
  seg([i, 0, -46], [i, 0, 46], 0.1 * k, 0.6 * k, 0.9 * k);
  seg([-46, 0, i], [46, 0, i], 0.1 * k, 0.6 * k, 0.9 * k);
}

/* ---- tone map + png ---- */
const bytes = new Uint8Array(W * H * 3);
for (let i = 0; i < W * H; i++) {
  for (let c = 0; c < 3; c++) {
    let v = img[i * 3 + c];
    v = v / (v + 0.7) * 1.4;
    v = Math.pow(Math.max(0, v), 0.9);
    const vig = 1 - Math.max(0, Math.hypot((i % W) / W - 0.5, Math.floor(i / W) / H - 0.5) - 0.18) * 0.9;
    bytes[i * 3 + c] = Math.max(0, Math.min(255, Math.round(v * vig * 230)));
  }
}
const crcTable = (() => { const t = new Int32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c; } return t; })();
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const t = Buffer.from(type, 'ascii'), buf = Buffer.concat([t, data]);
  let crc = -1;
  for (let i = 0; i < buf.length; i++) crc = crcTable[(crc ^ buf[i]) & 0xff] ^ (crc >>> 8);
  const c = Buffer.alloc(4); c.writeInt32BE(crc ^ -1);
  return Buffer.concat([len, t, data, c]);
}
const raw = Buffer.alloc((W * 3 + 1) * H);
for (let y = 0; y < H; y++) Buffer.from(bytes.buffer, y * W * 3, W * 3).copy(raw, y * (W * 3 + 1) + 1);
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
const png = Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
fs.writeFileSync(out, png);
console.log('wrote', out, png.length, 'bytes');
