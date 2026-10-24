/* ==========================================================================
   НЕЕВКЛИД — чистая логика мира (генерация, поиск пути, валидация).
   Не трогает DOM, поэтому тестируется в Node: require('./src/logic.js')
   ========================================================================== */
(function (root, factory) {
  'use strict';
  const api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.NE = root.NE || {};
  for (const k in api) root.NE[k] = api[k];
})(typeof window !== 'undefined' ? window : globalThis, function () {
  'use strict';

  /* ---------------- размеры и коды тайлов ---------------- */
  const MAP_W = 39;
  const MAP_H = 29;

  const T = {
    EMPTY: 0,
    WALL_A: 1,   // зона 0
    WALL_B: 2,   // зона 1
    WALL_C: 3,   // зона 2
    WALL_D: 4,   // зона 3
    QDOOR: 8,    // квантовая дверь: существует, только когда на неё смотрят
    EXIT: 9      // выход: открывается при 5/5 осколков
  };

  const isWall = (c) => c >= 1 && c <= 4;
  const isSolidCode = (c) => isWall(c) || c === T.EXIT;

  const ZONES = [
    { name: 'ПРЕДДВЕРИЕ',  sub: 'Здесь тебя никто не ждал.',            fog: [26, 34, 44],  light: [178, 214, 232] },
    { name: 'АРХИВ',       sub: 'Стены помнят всех, кто не вышел.',      fog: [40, 28, 20],  light: [240, 190, 130] },
    { name: 'ГАЛЕРЕЯ',     sub: 'Не смотри слишком долго.',              fog: [34, 20, 44],  light: [228, 160, 255] },
    { name: 'НИЖНИЙ ЯРУС', sub: 'Свет сюда не доезжает.',                fog: [18, 34, 28],  light: [150, 255, 200] }
  ];

  /* ---------------- ГСЧ ---------------- */
  function mulberry32(a) {
    a = a >>> 0;
    return function () {
      a = (a + 0x6D2B79F5) >>> 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  const I = (x, y) => y * MAP_W + x;
  const inB = (x, y) => x >= 0 && y >= 0 && x < MAP_W && y < MAP_H;

  function shuffled(arr, rnd) {
    const a = arr.slice();
    for (let i = a.length - 1; i > 0; i--) {
      const j = (rnd() * (i + 1)) | 0;
      const t = a[i]; a[i] = a[j]; a[j] = t;
    }
    return a;
  }

  /* ---------------- BFS по проходимым тайлам ---------------- */
  // passable(code) — что считаем проходимым. usePortals — телепорты как рёбра.
  function bfs(world, sx, sy, passable, usePortals) {
    const n = MAP_W * MAP_H;
    const dist = new Int32Array(n).fill(-1);
    const prev = new Int32Array(n).fill(-1);
    if (!inB(sx, sy)) return { dist, prev };
    const can = passable || ((c) => c === T.EMPTY);
    const q = new Int32Array(n);
    let head = 0, tail = 0;
    const s = I(sx, sy);
    if (!can(world.grid[s])) return { dist, prev };
    dist[s] = 0; q[tail++] = s;
    const portalByTile = world.portalByTile || null;
    while (head < tail) {
      const cur = q[head++];
      const cx = cur % MAP_W, cy = (cur / MAP_W) | 0;
      const d = dist[cur] + 1;
      for (let k = 0; k < 4; k++) {
        const nx = cx + (k === 0 ? 1 : k === 1 ? -1 : 0);
        const ny = cy + (k === 2 ? 1 : k === 3 ? -1 : 0);
        if (!inB(nx, ny)) continue;
        const ni = I(nx, ny);
        if (dist[ni] !== -1) continue;
        if (!can(world.grid[ni])) continue;
        dist[ni] = d; prev[ni] = cur; q[tail++] = ni;
      }
      if (usePortals && portalByTile) {
        const p = portalByTile[cur];
        if (p !== undefined && p >= 0) {
          const ni = p;
          if (dist[ni] === -1 && can(world.grid[ni])) { dist[ni] = d; prev[ni] = cur; q[tail++] = ni; }
        }
      }
    }
    return { dist, prev };
  }

  function pathFrom(prev, target) {
    const out = [];
    let c = target;
    while (c !== -1 && out.length < 4096) { out.push(c); c = prev[c]; }
    out.reverse();
    return out;
  }

  /* ---------------- A* для Наблюдателей ----------------
     opts.qDoors — пускать ли сквозь квантовые двери (Наблюдатели их не понимают,
     поэтому за дверью игрок в безопасности). Число в opts трактуется как maxExpand. */
  function astar(world, sx, sy, tx, ty, opts) {
    if (typeof opts === 'number') opts = { maxExpand: opts };
    opts = opts || {};
    const throughQ = !!opts.qDoors;
    if (!inB(sx, sy) || !inB(tx, ty)) return [];
    const n = MAP_W * MAP_H;
    const s = I(sx, sy), t = I(tx, ty);
    if (s === t) return [t];
    const g = new Float32Array(n).fill(Infinity);
    const prev = new Int32Array(n).fill(-1);
    const closed = new Uint8Array(n);
    const open = [];                                  // простой бинарный хип
    const h = (i) => {
      const x = i % MAP_W, y = (i / MAP_W) | 0;
      return Math.abs(x - tx) + Math.abs(y - ty);
    };
    g[s] = 0; open.push({ i: s, f: h(s) });
    let expanded = 0;
    const limit = opts.maxExpand || 5000;
    while (open.length) {
      let bi = 0;
      for (let k = 1; k < open.length; k++) if (open[k].f < open[bi].f) bi = k;
      const cur = open.splice(bi, 1)[0].i;
      if (closed[cur]) continue;
      closed[cur] = 1;
      if (++expanded > limit) break;
      if (cur === t) return pathFrom(prev, t);
      const cx = cur % MAP_W, cy = (cur / MAP_W) | 0;
      for (let k = 0; k < 4; k++) {
        const nx = cx + (k === 0 ? 1 : k === 1 ? -1 : 0);
        const ny = cy + (k === 2 ? 1 : k === 3 ? -1 : 0);
        if (!inB(nx, ny)) continue;
        const ni = I(nx, ny);
        if (closed[ni]) continue;
        const code = world.grid[ni];
        if (isSolidCode(code)) continue;
        if (code === T.QDOOR && !throughQ) continue;
        const ng = g[cur] + 1;
        if (ng < g[ni]) { g[ni] = ng; prev[ni] = cur; open.push({ i: ni, f: ng + h(ni) }); }
      }
    }
    return [];
  }

  /* ---------------- подсчёт открытых соседей ---------------- */
  const walkable = (c) => c === T.EMPTY || c === T.QDOOR;
  function openNeighbours(grid, x, y) {
    let n = 0;
    if (inB(x + 1, y) && walkable(grid[I(x + 1, y)])) n++;
    if (inB(x - 1, y) && walkable(grid[I(x - 1, y)])) n++;
    if (inB(x, y + 1) && walkable(grid[I(x, y + 1)])) n++;
    if (inB(x, y - 1) && walkable(grid[I(x, y - 1)])) n++;
    return n;
  }

  /* ---------------- генерация одного мира ---------------- */
  function tryGenerate(seed, relax) {
    const rnd = mulberry32(seed);
    const grid = new Uint8Array(MAP_W * MAP_H);
    grid.fill(0);                                   // 0 = временно «камень», позже станет EMPTY/WALL

    const WALLMARK = 200;
    grid.fill(WALLMARK);

    /* 1. Лабиринт (recursive backtracker) по клеткам */
    const CW = (MAP_W - 1) >> 1, CH = (MAP_H - 1) >> 1;
    const seen = new Uint8Array(CW * CH);
    const stack = [[0, 0]];
    seen[0] = 1;
    grid[I(1, 1)] = 0;
    while (stack.length) {
      const c = stack[stack.length - 1];
      const cx = c[0], cy = c[1];
      const dirs = shuffled([[1, 0], [-1, 0], [0, 1], [0, -1]], rnd);
      let moved = false;
      for (let d = 0; d < 4; d++) {
        const nx = cx + dirs[d][0], ny = cy + dirs[d][1];
        if (nx < 0 || ny < 0 || nx >= CW || ny >= CH) continue;
        if (seen[ny * CW + nx]) continue;
        seen[ny * CW + nx] = 1;
        grid[I(2 * cx + 1 + dirs[d][0], 2 * cy + 1 + dirs[d][1])] = 0;
        grid[I(2 * nx + 1, 2 * ny + 1)] = 0;
        stack.push([nx, ny]);
        moved = true;
        break;
      }
      if (!moved) stack.pop();
    }

    /* 2. Комнаты — чтобы лабиринт дышал */
    const nRooms = 5 + ((rnd() * 3) | 0);
    const rooms = [];
    for (let r = 0; r < nRooms; r++) {
      const rw = 3 + 2 * ((rnd() * 3) | 0);
      const rh = 3 + 2 * ((rnd() * 2) | 0);
      const rx = 1 + 2 * ((rnd() * (CW - 2)) | 0);
      const ry = 1 + 2 * ((rnd() * (CH - 2)) | 0);
      if (rx + rw > MAP_W - 2 || ry + rh > MAP_H - 2) continue;
      for (let y = ry; y < ry + rh; y++) for (let x = rx; x < rx + rw; x++) grid[I(x, y)] = 0;
      rooms.push({ x: rx + (rw >> 1), y: ry + (rh >> 1), w: rw, h: rh });
    }

    /* 3. «Плетение»: убираем часть тупиков → появляются петли */
    const braids = [];
    for (let y = 1; y < MAP_H - 1; y++) {
      for (let x = 1; x < MAP_W - 1; x++) {
        if (grid[I(x, y)] !== 0) continue;
        let open = 0;
        if (grid[I(x + 1, y)] === 0) open++;
        if (grid[I(x - 1, y)] === 0) open++;
        if (grid[I(x, y + 1)] === 0) open++;
        if (grid[I(x, y - 1)] === 0) open++;
        if (open !== 1) continue;
        if (rnd() > (relax ? 0.55 : 0.34)) continue;
        const cand = [];
        for (let k = 0; k < 4; k++) {
          const dx = k === 0 ? 1 : k === 1 ? -1 : 0;
          const dy = k === 2 ? 1 : k === 3 ? -1 : 0;
          const wx = x + dx, wy = y + dy, bx = x + 2 * dx, by = y + 2 * dy;
          if (!inB(bx, by)) continue;
          if (grid[I(wx, wy)] !== WALLMARK) continue;
          if (grid[I(bx, by)] !== 0) continue;
          cand.push([wx, wy]);
        }
        if (!cand.length) continue;
        const pick = cand[(rnd() * cand.length) | 0];
        grid[I(pick[0], pick[1])] = 0;
        braids.push(pick);
      }
    }

    /* 4. Пол / стена */
    const floorMask = new Uint8Array(MAP_W * MAP_H);
    const floors = [];
    for (let y = 0; y < MAP_H; y++) {
      for (let x = 0; x < MAP_W; x++) {
        if (grid[I(x, y)] === 0) { floorMask[I(x, y)] = 1; floors.push([x, y]); }
      }
    }
    if (floors.length < 260) return null;

    /* 5. Зоны: 4 удалённых друг от друга якоря + многоисточниковый BFS */
    const anchors = [];
    const midX = MAP_W / 2, midY = MAP_H / 2;
    let best = null, bestScore = -1;
    for (const f of floors) {
      const s = Math.hypot(f[0] - midX, f[1] - midY);
      if (s > bestScore) { bestScore = s; best = f; }
    }
    anchors.push(best);
    while (anchors.length < 4) {
      let bp = null, bs = -1;
      for (const f of floors) {
        let m = Infinity;
        for (const a of anchors) m = Math.min(m, Math.hypot(f[0] - a[0], f[1] - a[1]));
        if (m > bs) { bs = m; bp = f; }
      }
      if (!bp) break;
      anchors.push(bp);
    }
    const zone = new Uint8Array(MAP_W * MAP_H);
    {
      const q = [];
      let head = 0;
      const vis = new Uint8Array(MAP_W * MAP_H);
      anchors.forEach((a, zi) => { const i = I(a[0], a[1]); zone[i] = zi; vis[i] = 1; q.push(i); });
      while (head < q.length) {
        const cur = q[head++];
        const cx = cur % MAP_W, cy = (cur / MAP_W) | 0;
        for (let k = 0; k < 4; k++) {
          const nx = cx + (k === 0 ? 1 : k === 1 ? -1 : 0);
          const ny = cy + (k === 2 ? 1 : k === 3 ? -1 : 0);
          if (!inB(nx, ny)) continue;
          const ni = I(nx, ny);
          if (vis[ni]) continue;
          vis[ni] = 1; zone[ni] = zone[cur]; q.push(ni);
        }
      }
    }
    for (let i = 0; i < grid.length; i++) {
      grid[i] = floorMask[i] ? T.EMPTY : (T.WALL_A + zone[i]);
    }

    const world = {
      seed, W: MAP_W, H: MAP_H, grid, zone, floorMask, rooms, anchors,
      start: null, fragments: [], exit: null, portals: [], qdoors: [], angels: [],
      decals: new Uint8Array(MAP_W * MAP_H), decalHue: new Uint8Array(MAP_W * MAP_H),
      portalByTile: new Map(), maxDist: 0, zoneNames: ZONES
    };

    const startTile = [1, 1];
    world.start = { x: startTile[0] + 0.5, y: startTile[1] + 0.5, tx: 1, ty: 1 };

    const passFloor = (c) => c === T.EMPTY;
    const r0 = bfs(world, 1, 1, passFloor, false);
    let maxD = 0;
    for (const f of floors) maxD = Math.max(maxD, r0.dist[I(f[0], f[1])]);
    world.maxDist = maxD;
    if (maxD < (relax ? 12 : 26)) return null;

    const reserved = new Set();
    reserved.add(I(1, 1));
    const key = (x, y) => I(x, y);

    /* 6. Осколки — максимально разнесены */
    const fragCandidates = floors.filter((f) => {
      if (r0.dist[I(f[0], f[1])] < 0) return false;
      if (f[0] === 1 && f[1] === 1) return false;
      return openNeighbours(grid, f[0], f[1]) >= 2;
    });
    if (fragCandidates.length < 40) return null;
    const chosen = [];
    let far = null, farD = -1;
    for (const f of fragCandidates) {
      const d = r0.dist[I(f[0], f[1])];
      if (d > farD) { farD = d; far = f; }
    }
    chosen.push(far);
    while (chosen.length < 5) {
      let bp = null, bs = -1;
      for (const f of fragCandidates) {
        let m = Math.hypot(f[0] - 1, f[1] - 1) * 0.55;
        for (const c of chosen) m = Math.min(m, Math.hypot(f[0] - c[0], f[1] - c[1]));
        if (m > bs) { bs = m; bp = f; }
      }
      if (!bp || bs < (relax ? 4 : 6)) break;
      chosen.push(bp);
    }
    if (chosen.length < 5) return null;
    for (const f of chosen) {
      world.fragments.push({ x: f[0] + 0.5, y: f[1] + 0.5, tx: f[0], ty: f[1], taken: false, phase: rnd() * 6.28 });
      reserved.add(key(f[0], f[1]));
    }

    /* 7. Разломы (порталы) — далеко друг от друга, с поворотом пространства */
    const portalCand = floors.filter((f) =>
      !reserved.has(key(f[0], f[1])) &&
      openNeighbours(grid, f[0], f[1]) >= 2 &&
      r0.dist[I(f[0], f[1])] >= 0
    );
    const usedPortal = new Set();
    const placedPortalTiles = [];
    const farFromPortals = (x, y, min) => {
      for (const t of placedPortalTiles) if (Math.hypot(t[0] - x, t[1] - y) < min) return false;
      return true;
    };
    let tries = 0;
    while (world.portals.length < 5 && tries++ < 90) {
      const a = portalCand[(rnd() * portalCand.length) | 0];
      if (!a || usedPortal.has(key(a[0], a[1]))) continue;
      if (!farFromPortals(a[0], a[1], 5)) continue;              // разломы не кучкуются
      const rb = bfs(world, a[0], a[1], passFloor, false);
      let bp = null, bs = -1;
      for (const f of portalCand) {
        if (usedPortal.has(key(f[0], f[1]))) continue;
        if (Math.hypot(f[0] - a[0], f[1] - a[1]) < 6) continue;   // не склеиваем соседние клетки
        if (!farFromPortals(f[0], f[1], 5)) continue;
        const d = rb.dist[I(f[0], f[1])];
        if (d > bs) { bs = d; bp = f; }
      }
      if (!bp || bs < (relax ? 10 : 16)) continue;
      usedPortal.add(key(a[0], a[1])); usedPortal.add(key(bp[0], bp[1]));
      placedPortalTiles.push(a, bp);
      reserved.add(key(a[0], a[1])); reserved.add(key(bp[0], bp[1]));
      const hue = world.portals.length;
      const rot = 1 + ((rnd() * 3) | 0);                 // 1..3 → 90/180/270°
      world.portals.push({
        ax: a[0], ay: a[1], bx: bp[0], by: bp[1],
        rot, hue,
        label: ['АЛЬФА', 'БЕТА', 'ГАММА', 'ДЕЛЬТА', 'ЭПСИЛОН'][hue]
      });
      world.decals[I(a[0], a[1])] = 2; world.decalHue[I(a[0], a[1])] = hue;
      world.decals[I(bp[0], bp[1])] = 2; world.decalHue[I(bp[0], bp[1])] = hue;
    }
    if (world.portals.length < (relax ? 2 : 4)) return null;

    /* 8. Дверь выхода — в дальней стене, между двумя полами */
    {
      let bestSpot = null, bestD = -1;
      for (let y = 2; y < MAP_H - 2; y++) {
        for (let x = 2; x < MAP_W - 2; x++) {
          if (!isWall(grid[I(x, y)])) continue;
          const horiz = grid[I(x - 1, y)] === T.EMPTY && grid[I(x + 1, y)] === T.EMPTY;
          const vert = grid[I(x, y - 1)] === T.EMPTY && grid[I(x, y + 1)] === T.EMPTY;
          if (!horiz && !vert) continue;
          const nb = horiz ? [x - 1, y] : [x, y - 1];
          if (reserved.has(key(nb[0], nb[1]))) continue;
          const d = r0.dist[I(nb[0], nb[1])];
          if (d < 0) continue;
          let tooClose = false;
          for (const f of world.fragments) if (Math.hypot(f.tx - x, f.ty - y) < 4) tooClose = true;
          if (tooClose) continue;
          if (d > bestD) { bestD = d; bestSpot = { x, y, horiz, nb }; }
        }
      }
      if (!bestSpot || bestD < (relax ? 8 : 18)) return null;
      grid[I(bestSpot.x, bestSpot.y)] = T.EXIT;
      world.exit = { x: bestSpot.x, y: bestSpot.y, horiz: bestSpot.horiz, open: false };
      // обе стороны дверного проёма выходa защищены: там не встанет ни дверь, ни наблюдатель
      for (const n of (bestSpot.horiz
        ? [[bestSpot.x - 1, bestSpot.y], [bestSpot.x + 1, bestSpot.y]]
        : [[bestSpot.x, bestSpot.y - 1], [bestSpot.x, bestSpot.y + 1]])) {
        reserved.add(key(n[0], n[1]));
        world.exitSide = world.exitSide || [];
        world.exitSide.push({ x: n[0], y: n[1] });
      }
    }

    /* 9. Квантовые двери — в узких проходах на границах зон */
    {
      const cand = [];
      for (let y = 1; y < MAP_H - 1; y++) {
        for (let x = 1; x < MAP_W - 1; x++) {
          if (grid[I(x, y)] !== T.EMPTY) continue;
          if (reserved.has(key(x, y))) continue;
          const e = grid[I(x + 1, y)] === T.EMPTY, w = grid[I(x - 1, y)] === T.EMPTY;
          const s = grid[I(x, y + 1)] === T.EMPTY, n = grid[I(x, y - 1)] === T.EMPTY;
          const corridor = (e && w && !s && !n) || (n && s && !e && !w);
          if (!corridor) continue;
          if (r0.dist[I(x, y)] < 7) continue;
          const zc = zone[I(x, y)];
          let boundary = false;
          for (let k = 0; k < 4; k++) {
            const nx = x + (k === 0 ? 1 : k === 1 ? -1 : 0);
            const ny = y + (k === 2 ? 1 : k === 3 ? -1 : 0);
            if (!inB(nx, ny)) continue;
            if (floorMask[I(nx, ny)] && zone[I(nx, ny)] !== zc) boundary = true;
          }
          cand.push({ x, y, boundary, d: r0.dist[I(x, y)] });
        }
      }
      cand.sort((p, q) => (q.boundary - p.boundary) || (q.d - p.d));
      const picked = [];
      for (const c of cand) {
        if (picked.length >= 4) break;
        let ok = true;
        for (const p of picked) if (Math.hypot(p.x - c.x, p.y - c.y) < 7) ok = false;
        if (!ok) continue;
        picked.push(c);
        grid[I(c.x, c.y)] = T.QDOOR;
        world.qdoors.push({ x: c.x, y: c.y, observed: false, hum: 0 });
        world.decals[I(c.x, c.y)] = 1;
        reserved.add(key(c.x, c.y));
      }
      if (world.qdoors.length < (relax ? 1 : 2)) return null;
    }

    /* 10. Наблюдатели */
    {
      const cand = floors.filter((f) =>
        !reserved.has(key(f[0], f[1])) &&
        r0.dist[I(f[0], f[1])] > maxD * 0.45 &&
        openNeighbours(grid, f[0], f[1]) >= 2
      );
      const picks = [];
      for (let i = 0; i < 2 && cand.length; i++) {
        let bp = null, bs = -1;
        for (const f of cand) {
          let m = r0.dist[I(f[0], f[1])];
          for (const p of picks) m = Math.min(m, Math.hypot(f[0] - p[0], f[1] - p[1]) * 2);
          if (m > bs) { bs = m; bp = f; }
        }
        if (!bp) break;
        picks.push(bp);
        world.angels.push({
          x: bp[0] + 0.5, y: bp[1] + 0.5, tx: bp[0], ty: bp[1],
          path: [], repath: 0, observed: false, stillness: 0, jitter: rnd() * 10, id: i
        });
        reserved.add(key(bp[0], bp[1]));
      }
      if (!world.angels.length) return null;
    }

    /* 11. Индекс порталов для BFS */
    for (let i = 0; i < world.portals.length; i++) {
      const p = world.portals[i];
      world.portalByTile.set(I(p.ax, p.ay), I(p.bx, p.by));
      world.portalByTile.set(I(p.bx, p.by), I(p.ax, p.ay));
    }

    world.roomsFor = rooms;
    return validate(world) ? world : null;
  }

  /* ---------------- валидация: мир обязан быть проходим ---------------- */
  function validate(world) {
    const passable = (c) => c === T.EMPTY || c === T.QDOOR;   // сквозь дверь можно пройти спиной
    const r = bfs(world, world.start.tx, world.start.ty, passable, true);
    for (const f of world.fragments) if (r.dist[I(f.tx, f.ty)] < 0) return false;
    for (const p of world.portals) {
      if (r.dist[I(p.ax, p.ay)] < 0) return false;
      if (r.dist[I(p.bx, p.by)] < 0) return false;
    }
    // выход должен стоять вплотную к достижимому полу
    const ex = world.exit;
    let ok = false;
    const nbs = ex.horiz ? [[ex.x - 1, ex.y], [ex.x + 1, ex.y]] : [[ex.x, ex.y - 1], [ex.x, ex.y + 1]];
    for (const n of nbs) if (inB(n[0], n[1]) && r.dist[I(n[0], n[1])] >= 0) ok = true;
    if (!ok) return false;
    // осколки не должны кучковаться
    for (let i = 0; i < world.fragments.length; i++) {
      for (let j = i + 1; j < world.fragments.length; j++) {
        const a = world.fragments[i], b = world.fragments[j];
        if (Math.hypot(a.tx - b.tx, a.ty - b.ty) < 5) return false;
      }
    }
    world.reach = r.dist;
    return true;
  }

  function generateWorld(seed) {
    for (let attempt = 0; attempt < 90; attempt++) {
      const w = tryGenerate((seed + attempt * 2654435761) >>> 0, false);
      if (w) return w;
    }
    for (let attempt = 0; attempt < 40; attempt++) {
      const w = tryGenerate((seed + attempt * 40503) >>> 0, true);
      if (w) return w;
    }
    throw new Error('Не удалось породить лабиринт для сида ' + seed);
  }

  /* ---------------- ASCII-дамп для отладки ---------------- */
  function dump(world) {
    const ch = (c) => c === T.EMPTY ? '·' : c === T.QDOOR ? '◊' : c === T.EXIT ? 'Ж' : isWall(c) ? '█' : '?';
    let s = '';
    for (let y = 0; y < MAP_H; y++) {
      let row = '';
      for (let x = 0; x < MAP_W; x++) {
        let c = ch(world.grid[I(x, y)]);
        if (x === world.start.tx && y === world.start.ty) c = '@';
        for (const f of world.fragments) if (f.tx === x && f.ty === y) c = '*';
        for (const p of world.portals) if ((p.ax === x && p.ay === y) || (p.bx === x && p.by === y)) c = '+';
        for (const a of world.angels) if (a.tx === x && a.ty === y) c = 'A';
        row += c;
      }
      s += row + '\n';
    }
    return s;
  }

  /* ---------------- мелкая математика ---------------- */
  const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
  const lerp = (a, b, t) => a + (b - a) * t;
  function angDiff(a, b) {
    let d = (a - b) % (Math.PI * 2);
    if (d > Math.PI) d -= Math.PI * 2;
    if (d < -Math.PI) d += Math.PI * 2;
    return d;
  }

  return {
    MAP_W, MAP_H, T, ZONES, isWall, isSolidCode, mulberry32, bfs, astar,
    walkable, generateWorld, tryGenerate, validate, dump, clamp, lerp, angDiff, I, inB, openNeighbours
  };
});
