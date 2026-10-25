/* Генерирует превью-гифку: идём по залу и переключаем W каждые 6 кадров */
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const { makeWorld } = require('../src/world.js');
const { render } = require('../src/render.js');

const world = makeWorld(20261025);
const W = 340, H = 191;
const pix = new Uint32Array(W * H), depth = new Float32Array(W * H);
const dir = path.join(__dirname, '..', 'shots');
fs.mkdirSync(dir, { recursive: true });

const FRAMES = 24;
const files = [];
for (let f = 0; f < FRAMES; f++) {
  const t = f / FRAMES;
  // камера: идёт по залу и медленно поворачивает
  const z = 19.5 - Math.sin(t * Math.PI * 2) * 0.9;
  const yaw = Math.sin(t * Math.PI * 2) * 0.5;
  const w = Math.floor(f / 8) % 4 + 0; // 4 глубины по 8 кадров? нет: переключение каждые 4 кадра
  const slice = Math.floor(f / 3) % 4;
  render(world, {
    fov: 0.95, x: 15.5, y: 1.62, z, yaw, pitch: 0.03,
    w: slice, occ: world.occ, P: world.P, lamps: world.lamps,
  }, W, H, pix, depth, f * 0.16);
  const head = Buffer.from(`P6\n${W} ${H}\n255\n`, 'ascii');
  const body = Buffer.alloc(W * H * 3);
  for (let i = 0; i < W * H; i++) {
    const v = pix[i];
    body[i * 3] = v & 255; body[i * 3 + 1] = (v >> 8) & 255; body[i * 3 + 2] = (v >> 16) & 255;
  }
  const p = path.join(dir, `pv_${String(f).padStart(2, '0')}.ppm`);
  fs.writeFileSync(p, Buffer.concat([head, body]));
  files.push(p);
}
console.log('кадры готовы:', files.length);
const gif = path.join(dir, 'preview.gif');
execSync(`convert -delay 9 -loop 0 ${files.join(' ')} -colors 96 -layers Optimize ${gif}`);
console.log('preview.gif:', (fs.statSync(gif).size / 1024).toFixed(0), 'KB');
// первый кадр как превью-картинка
execSync(`convert ${files[0]} ${path.join(dir, 'preview.png')}`);
for (const f of files) fs.unlinkSync(f);
