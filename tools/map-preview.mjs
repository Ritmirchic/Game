// Dev tool: renders a top-down map of the generated city to PNG.
// usage: node tools/map-preview.mjs [seed] [out.png]
import fs from 'node:fs';
import zlib from 'node:zlib';

globalThis.window = globalThis;
await import('../src/world.js');
const { City, CFG } = globalThis.MEM;

const seed = Number(process.argv[2] || 7);
const out = process.argv[3] || 'tools/map.png';
const city = new City(seed);
city.reveal(0, 26, 11.5, 0);

const W = 900, H = 900;
const M = 2 * CFG.HALF;
const px = (v) => Math.round((v + CFG.HALF) / M * W);

const img = new Uint8Array(W * H * 3);
for (let i = 0; i < W * H; i++) { img[i * 3] = 6; img[i * 3 + 1] = 8; img[i * 3 + 2] = 16; }

function plot(x, y, r, g, b) {
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  const i = (y * W + x) * 3; img[i] = r; img[i + 1] = g; img[i + 2] = b;
}
function rect(x0, z0, x1, z1, r, g, b) {
  for (let y = Math.max(0, px(z0)); y < Math.min(H, px(z1)); y++)
    for (let x = Math.max(0, px(x0)); x < Math.min(W, px(x1)); x++) plot(x, y, r, g, b);
}
function line(x0, z0, x1, z1, r, g, b) {
  const n = Math.ceil(Math.hypot(px(x1) - px(x0), px(z1) - px(z0))) || 1;
  for (let i = 0; i <= n; i++) {
    const t = i / n;
    plot(Math.round(px(x0) + (px(x1) - px(x0)) * t), Math.round(px(z0) + (px(z1) - pz(z0)) * t), r, g, b);
  }
}
function pz(v) { return (v + CFG.HALF) / M * H; }

// memory tiles
for (let tz = 0; tz < CFG.TILES; tz++) for (let tx = 0; tx < CFG.TILES; tx++) {
  if (city.mem[(tz * CFG.TILES + tx) * 4]) {
    rect(tx * CFG.TILE - CFG.HALF, tz * CFG.TILE - CFG.HALF, (tx + 1) * CFG.TILE - CFG.HALF, (tz + 1) * CFG.TILE - CFG.HALF, 18, 30, 46);
  }
}

for (const s of city.structures) {
  for (const p of s.parts) {
    const x0 = p.x - p.hx, x1 = p.x + p.hx, z0 = p.z - p.hz, z1 = p.z + p.hz;
    const k = Math.min(1, s.top / 34);
    if (s.count) {
      rect(x0, z0, x1, z1, 30 + 60 * k, 90 + 70 * k, 140 + 80 * k);
      line(x0, z0, x1, z0, 140, 240, 255); line(x1, z0, x1, z1, 140, 240, 255);
      line(x1, z1, x0, z1, 140, 240, 255); line(x0, z1, x0, z0, 140, 240, 255);
    } else {
      rect(x0, z0, x1, z1, 70, 50, 90);
    }
  }
}
// start marker + obelisk
rect(-0.4, 25.6, 0.4, 26.4, 255, 80, 120);
rect(-3, -3, 3, 3, 255, 220, 120);
rect(-13.5, -13.5, 13.5, 13.5, 24, 34, 44);

// axis cross
for (let i = 0; i < 40; i++) { plot(px(0), px(-44 + i * 2.2), 40, 60, 80); plot(px(-44 + i * 2.2), px(0), 40, 60, 80); }

// --- PNG encode
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const t = Buffer.from(type, 'ascii');
  const crcTable = chunk.t || (chunk.t = (() => {
    const tab = new Int32Array(256);
    for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; tab[n] = c; }
    return tab;
  })());
  let crc = -1;
  const buf = Buffer.concat([t, data]);
  for (let i = 0; i < buf.length; i++) crc = crcTable[(crc ^ buf[i]) & 0xff] ^ (crc >>> 8);
  const c = Buffer.alloc(4); c.writeInt32BE(crc ^ -1);
  return Buffer.concat([len, t, data, c]);
}
const raw = Buffer.alloc((W * 3 + 1) * H);
for (let y = 0; y < H; y++) {
  raw[y * (W * 3 + 1)] = 0;
  Buffer.from(img.buffer, y * W * 3, W * 3).copy(raw, y * (W * 3 + 1) + 1);
}
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4);
ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
const png = Buffer.concat([
  Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
  chunk('IHDR', ihdr),
  chunk('IDAT', zlib.deflateSync(raw, { level: 9 })),
  chunk('IEND', Buffer.alloc(0))
]);
fs.writeFileSync(out, png);
console.log('wrote', out, png.length, 'bytes;', city.total, 'countable structures');
