/* Headless-проверка: достижимость фрагментов + рендер кадров в PPM/PNG */
const fs = require('fs');
const path = require('path');
const { makeWorld } = require('../src/world.js');
const { render } = require('../src/render.js');

const world = makeWorld(20261025);
const N = world.N;
const occ = world.occ, P = world.P;
const { grid, idx } = world;

const SOLID = (v) => v === 1 || v === 2;
function walk(w, x, z) {
  if (x < 0 || z < 0 || x >= N || z >= N) return false;
  return !SOLID(grid[w][idx(x, 1, z)]) && !SOLID(grid[w][idx(x, 2, z)]);
}
let ok = true;
for (let ws = 0; ws < 4; ws++) {
  const seen = new Set();
  const sx = Math.floor(world.SPAWN.x), sz = Math.floor(world.SPAWN.z);
  const q = [[sx, sz]];
  seen.add(sx + ',' + sz);
  while (q.length) {
    const [x, z] = q.pop();
    for (const [nx, nz] of [[x + 1, z], [x - 1, z], [x, z + 1], [x, z - 1]]) {
      if (!walk(ws, nx, nz)) continue;
      const k = nx + ',' + nz;
      if (seen.has(k)) continue;
      seen.add(k); q.push([nx, nz]);
    }
  }
  const f = world.FRAGS[ws];
  const reach = seen.has(Math.floor(f.x) + ',' + Math.floor(f.z));
  const coreReach = seen.has(Math.floor(world.CORE.x) + ',' + Math.floor(world.CORE.z));
  const spawnFree = walk(ws, sx, sz);
  const bad = world.FRAGS.some((g, i) => i !== ws && seen.has(Math.floor(g.x) + ',' + Math.floor(g.z)));
  console.log(`срез ${ws}: старт ${spawnFree ? 'свободен' : 'ЗАБЛОКИРОВАН'}, фрагмент ${reach ? 'достижим' : 'НЕДОСТИЖИМ'}, ядро ${coreReach ? 'ок' : 'НЕТ'}, чужих фрагментов в срезе: ${bad ? 'ЕСТЬ (плохо)' : 'нет'}, клеток ${seen.size}`);
  if (!reach || !coreReach || !spawnFree || bad) ok = false;
}
console.log(ok ? 'МИР: OK' : 'МИР: ПРОБЛЕМА');

// --- рендер кадров
const W = 480, H = 270;
const pix = new Uint32Array(W * H), depth = new Float32Array(W * H);

function shot(name, cam) {
  const t0 = Date.now();
  render(world, { fov: 0.92, occ, P, lamps: world.lamps, ...cam }, W, H, pix, depth, 1.0);
  const ms = Date.now() - t0;
  // PPM
  const head = Buffer.from(`P6\n${W} ${H}\n255\n`, 'ascii');
  const body = Buffer.alloc(W * H * 3);
  for (let i = 0; i < W * H; i++) {
    const v = pix[i];
    body[i * 3] = v & 255; body[i * 3 + 1] = (v >> 8) & 255; body[i * 3 + 2] = (v >> 16) & 255;
  }
  const out = path.join(__dirname, '..', 'shots', name + '.ppm');
  fs.mkdirSync(path.dirname(out), { recursive: true });
  fs.writeFileSync(out, Buffer.concat([head, body]));
  console.log(`  ${name}: ${ms} ms  (${(W * H / 1000).toFixed(0)}k пикселей)`);
}

const EY = 1.62;
shot('01_spawn_w0', { x: 15.5, y: EY, z: 5.5, yaw: 0, pitch: 0, w: 0 });
shot('02_hall_w0', { x: 15.5, y: EY, z: 19.5, yaw: 0, pitch: 0.02, w: 0 });
shot('03_hall_w1', { x: 15.5, y: EY, z: 19.5, yaw: 0, pitch: 0.02, w: 1 });
shot('04_hall_w2', { x: 15.5, y: EY, z: 19.5, yaw: 0, pitch: 0.02, w: 2 });
shot('05_hall_w3', { x: 15.5, y: EY, z: 19.5, yaw: 0, pitch: 0.02, w: 3 });
shot('06_ghost_door', { x: 8.5, y: EY, z: 5.1, yaw: 0, pitch: 0, w: 2 });
shot('07_chamber0', { x: 8.5, y: EY, z: 9.6, yaw: 0, pitch: -0.05, w: 0 });
shot('08_corridor_n_w1', { x: 15.5, y: EY, z: 10.0, yaw: 0, pitch: 0, w: 1 });
shot('09_west_corr_w3', { x: 5.5, y: EY, z: 17.5, yaw: -Math.PI/2, pitch: 0, w: 3 });
shot('10_hub_look_up', { x: 15.5, y: EY, z: 18.0, yaw: Math.PI/2, pitch: 0.55, w: 0 });
shot('11_door_wrong_w3', { x: 24.5, y: EY, z: 16.5, yaw: 0, pitch: -0.1, w: 0 });
shot('12_inside_chamber3', { x: 22.5, y: EY, z: 24.0, yaw: 2.2, pitch: 0, w: 3 });
