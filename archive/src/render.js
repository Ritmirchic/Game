/* ============================================================
   СРЕЗ / SLICE — 4D ray-voxel renderer
   Для каждого пикселя пускаем 3D-луч сквозь 3D-срез четырёхмерной решётки.
   Клетка бывает: пустой, породой и «призраком» (материей одного измерения).
   Если в моём срезе пусто, а в соседнем стоит призрак — рисуем светящуюся
   грань из другого измерения и продолжаем луч. Отсюда невозможная геометрия.
   occ: байт на клетку, биты 0..3 — порода по срезам, биты 4..7 — призрак.
   ============================================================ */
(function (root) {
  'use strict';

  const WCOL = [[56, 232, 214], [255, 190, 84], [255, 92, 158], [172, 138, 255]];
  const SIN = new Float32Array(256);
  for (let i = 0; i < 256; i++) SIN[i] = Math.sin(i / 256 * Math.PI * 2);

  function render(world, cam, W, H, pix, depth, time) {
    const tanX = cam.fov, tanY = cam.fov * H / W;
    const yaw = cam.yaw, pitch = cam.pitch;
    const sy = Math.sin(yaw), cy = Math.cos(yaw);
    const sp = Math.sin(pitch), cp = Math.cos(pitch);

    const fx = sy * cp, fy = sp, fz = cy * cp;
    const rx = cy, ry = 0, rz = -sy;
    const ux = -sy * sp, uy = cp, uz = -cy * sp;

    const ox = cam.x, oy = cam.y, oz = cam.z;
    const w = cam.w;
    const occ = cam.occ, P = cam.P, S = P * P;
    const lamps = cam.lamps;

    const wbit = 1 << w, gbit = 1 << (w + 4);
    const other = (0xF ^ wbit) << 4;          // «призраки» чужих срезов
    const t0 = WCOL[w];
    const tr = t0[0], tg = t0[1], tb = t0[2];

    const MAXD = 40, MAXSTEPS = 60;
    const tm = (time || 0) * 40;

    let px = 0;
    for (let y = 0; y < H; y++) {
      const v = (1 - 2 * (y + 0.5) / H) * tanY;
      const bx = fx + ux * v, by = fy + uy * v, bz = fz + uz * v;   // строка: f + up*v
      for (let x = 0; x < W; x++, px++) {
        const u = (2 * (x + 0.5) / W - 1) * tanX;
        let dx = bx + rx * u, dy = by + ry * u, dz = bz + rz * u;   // + right*u
        const il = 1 / Math.sqrt(dx * dx + dy * dy + dz * dz);
        dx *= il; dy *= il; dz *= il;

        let mx = ox | 0, my = oy | 0, mz = oz | 0;
        let base = (my + 1) * S + (mz + 1) * P + (mx + 1);
        const ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, az = dz < 0 ? -dz : dz;
        const ddx = ax < 1e-9 ? 1e30 : 1 / ax;
        const ddy = ay < 1e-9 ? 1e30 : 1 / ay;
        const ddz = az < 1e-9 ? 1e30 : 1 / az;
        let stx, sty, stz, sdx, sdy, sdz, bX, bY, bZ;
        if (dx < 0) { stx = -1; bX = -1; sdx = (ox - mx) * ddx; } else { stx = 1; bX = 1; sdx = (mx + 1 - ox) * ddx; }
        if (dy < 0) { sty = -1; bY = -S; sdy = (oy - my) * ddy; } else { sty = 1; bY = S; sdy = (my + 1 - oy) * ddy; }
        if (dz < 0) { stz = -1; bZ = -P; sdz = (oz - mz) * ddz; } else { stz = 1; bZ = P; sdz = (mz + 1 - oz) * ddz; }

        let cr = 0, cg = 0, cb = 0, A = 0, tEnter = 0, axis = 0, solidD = MAXD, any = 0;

        for (let i = 0; i < MAXSTEPS; i++) {
          const ob = occ[base];
          if (ob === 255) break;                         // за пределами мира

          // ---------- лампа: источник света, горит во всех измерениях
          if (lamps && lamps[base] && tEnter > 0.05) {
            const hx = ox + dx * tEnter, hy = oy + dy * tEnter, hz = oz + dz * tEnter;
            const core = 1 - Math.min(1, (Math.abs(hx - mx - 0.5) + Math.abs(hy - my - 0.5) + Math.abs(hz - mz - 0.5)) * 1.6);
            const fl = 0.88 + 0.12 * SIN[(tm * 3) & 255];
            const inten = (0.45 + 0.55 * core * core) * fl * (1 - Math.min(0.8, tEnter * 0.03));
            cr += (1 - A) * inten * 250;
            cg += (1 - A) * inten * 226;
            cb += (1 - A) * inten * 190;
            A += (1 - A) * inten * 0.75;
            if (A > 0.97) { solidD = tEnter; break; }
          }

          if (ob & wbit) {
            // ---------- материя моего среза
            const hx = ox + dx * tEnter, hy = oy + dy * tEnter, hz = oz + dz * tEnter;
            let f1, f2, shade;
            if (axis === 0) { f1 = hy - (hy | 0); f2 = hz - (hz | 0); shade = dx > 0 ? 0.70 : 0.84; }
            else if (axis === 1) { f1 = hx - (hx | 0); f2 = hz - (hz | 0); shade = dy > 0 ? 0.50 : 1.0; }
            else { f1 = hx - (hx | 0); f2 = hy - (hy | 0); shade = dz > 0 ? 0.60 : 0.78; }

            const e1 = f1 < 0.5 ? f1 : 1 - f1;
            const e2 = f2 < 0.5 ? f2 : 1 - f2;
            let line = 0;
            if (e1 < 0.03 || e2 < 0.03) line = 1;
            else if (e1 < 0.08 || e2 < 0.08) line = 0.4;

            const nse = 0.88 + 0.24 * (((mx * 73856093) ^ (my * 19349663) ^ (mz * 83492791)) & 7) / 7;
            const lamp = 1 + 0.55 / (1 + tEnter * tEnter * 0.09);   // «фонарь» наблюдателя
            const fg0 = tEnter * 0.052;
            const fog = fg0 / (1 + fg0), keep = 1 - fog;
            shade *= nse;

            let r, gg, b;
            if (ob & gbit) {
              // призрачная материя в своём измерении — гладкая, цветная, глянцевая
              const pat = 0.72 + 0.28 * SIN[((hy * 6 + hx * 3 + hz * 2) & 255)];
              r = (tr * 0.62 + 26) * shade * pat;
              gg = (tg * 0.62 + 26) * shade * pat;
              b = (tb * 0.62 + 26) * shade * pat;
            } else {
              // порода: у каждого измерения свой камень
              const pat2 = SIN[((hx * 11 + hy * 7 + hz * 5) | 0) & 255];
              if (w === 0) {          // ядро: холодный шлифованный камень
                r = (74 + tr * 0.32) * shade; gg = (86 + tg * 0.28) * shade; b = (94 + tb * 0.32) * shade;
              } else if (w === 1) {   // разлом: тёплый грубый камень с косой штриховкой
                const k = 0.86 + 0.22 * pat2;
                r = (96 + tr * 0.26) * shade * k; gg = (84 + tg * 0.24) * shade * k; b = (80 + tb * 0.26) * shade * k;
              } else if (w === 2) {   // обратная сторона: тёмный металл
                const k = 0.82 + 0.30 * SIN[((hy * 40) | 0) & 255];
                r = (52 + tr * 0.22) * shade * k; gg = (54 + tg * 0.20) * shade * k; b = (66 + tb * 0.26) * shade * k;
              } else {                // тишина: светлый, почти белый мрамор
                const k = 0.94 + 0.10 * pat2;
                r = (150 + tr * 0.22) * shade * k; gg = (152 + tg * 0.20) * shade * k; b = (160 + tb * 0.24) * shade * k;
              }
            }
            r = r * keep * (1 + (lamp - 1) * 0.55) + (6 + tr * 0.06) * fog;
            gg = gg * keep * (1 + (lamp - 1) * 0.55) + (10 + tg * 0.06) * fog;
            b = b * keep * (1 + (lamp - 1) * 0.55) + (14 + tb * 0.06) * fog;
            r += line * 34 * keep; gg += line * 37 * keep; b += line * 40 * keep;

            cr += (1 - A) * r; cg += (1 - A) * gg; cb += (1 - A) * b;
            A = 1; solidD = tEnter; any = 1;
            break;
          }

          // ---------- призрачные грани из других измерений
          if ((ob & other) && A < 0.92 && tEnter > 0.045) {
            const hx = ox + dx * tEnter, hy = oy + dy * tEnter, hz = oz + dz * tEnter;
            let f1, f2;
            if (axis === 0) { f1 = hy - (hy | 0); f2 = hz - (hz | 0); }
            else if (axis === 1) { f1 = hx - (hx | 0); f2 = hz - (hz | 0); }
            else { f1 = hx - (hx | 0); f2 = hy - (hy | 0); }
            const e1 = f1 < 0.5 ? f1 : 1 - f1;
            const e2 = f2 < 0.5 ? f2 : 1 - f2;
            let edge = 0;
            if (e1 < 0.04 || e2 < 0.04) edge = 1;
            else if (e1 < 0.12 || e2 < 0.12) edge = 0.45;

            const scan = 0.5 + 0.5 * SIN[(((hy * 150 + hx * 22 + tm) | 0) & 255)];
            let rr = 0, rg = 0, rb = 0, n = 0;
            const gh = ob & other;
            if (gh & 16) { rr += WCOL[0][0]; rg += WCOL[0][1]; rb += WCOL[0][2]; n++; }
            if (gh & 32) { rr += WCOL[1][0]; rg += WCOL[1][1]; rb += WCOL[1][2]; n++; }
            if (gh & 64) { rr += WCOL[2][0]; rg += WCOL[2][1]; rb += WCOL[2][2]; n++; }
            if (gh & 128) { rr += WCOL[3][0]; rg += WCOL[3][1]; rb += WCOL[3][2]; n++; }
            rr /= n; rg /= n; rb /= n;

            const fog = tEnter * 0.018;
            let al = (0.10 + 0.20 * scan) * (0.40 + 0.60 * edge);
            if (fog > 0.55) al *= 1 - (fog - 0.55);
            const lift = 14 * edge + 6 * scan;
            cr += (1 - A) * al * (rr * (0.30 + 0.95 * edge) + lift);
            cg += (1 - A) * al * (rg * (0.30 + 0.95 * edge) + lift);
            cb += (1 - A) * al * (rb * (0.30 + 0.95 * edge) + lift);
            A += (1 - A) * al;
            any = 1;
            if (A > 0.92) { solidD = tEnter; break; }
          }

          // ---------- шаг DDA
          if (sdx < sdy) {
            if (sdx < sdz) { tEnter = sdx; mx += stx; base += bX; sdx += ddx; axis = 0; }
            else { tEnter = sdz; mz += stz; base += bZ; sdz += ddz; axis = 2; }
          } else {
            if (sdy < sdz) { tEnter = sdy; my += sty; base += bY; sdy += ddy; axis = 1; }
            else { tEnter = sdz; mz += stz; base += bZ; sdz += ddz; axis = 2; }
          }
          if (tEnter > MAXD) break;
        }

        let R, G, B;
        if (A < 0.999) {
          const sr = skyR(dy, tr, dx, dz), sg = skyG(dy, tg, dx, dz), sb = skyB(dy, tb, dx, dz);
          R = cr + (1 - A) * sr; G = cg + (1 - A) * sg; B = cb + (1 - A) * sb;
        } else { R = cr; G = cg; B = cb; }

        pix[px] = 0xff000000 | ((B > 255 ? 255 : B) << 16) | ((G > 255 ? 255 : G) << 8) | (R > 255 ? 255 : R);
        depth[px] = any ? solidD : MAXD;
      }
    }
    return { W, H };
  }

  /* 4D-небо: градиент, свечение горизонта и звёздная решётка */
  function skyR(dy, tr, dx, dz) { return 6 + 15 * up(dy) + tr * 0.05 * horiz(dy) + grid4(dx, dy, dz) * 0.75; }
  function skyG(dy, tg, dx, dz) { return 11 + 23 * up(dy) + tg * 0.05 * horiz(dy) + grid4(dx, dy, dz) * 0.85; }
  function skyB(dy, tb, dx, dz) { return 19 + 31 * up(dy) + tb * 0.05 * horiz(dy) + grid4(dx, dy, dz) * 1.0; }
  function up(dy) { const v = dy * 1.7 + 0.12; return v < 0 ? 0 : v > 1 ? 1 : v; }
  function horiz(dy) { const h = 1 - (dy < 0 ? -dy : dy) * 4.5; return h < 0 ? 0 : h * h; }
  function grid4(dx, dy, dz) {
    if (dy < 0.02) return 0;
    const a = dx / dy, b = dz / dy;
    const fa = ((a * 3.2) % 1 + 1) % 1, fb = ((b * 3.2) % 1 + 1) % 1;
    if (fa < 0.016 || fb < 0.016) return 24;
    return 0;
  }

  const api = { render, WCOL };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Render = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
