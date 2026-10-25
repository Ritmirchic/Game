/* MEMORIA — world: generation of the city plan, memory grid, collision.
   Pure JS, no DOM. Loaded as a classic script so the game runs even from file:// */
(function (root) {
  'use strict';

  var CFG = {
    HALF: 46,        // playable half-size (metres)
    TILE: 2,         // memory tile size (metres)
    TILES: 48,       // 96m / 2m
    TEX: 64,         // memory texture resolution (power of two)
    EYE: 1.62,
    WALK_R: 12.0,    // reveal radius while walking
    FLY_R: 9.0,      // reveal radius while flying (fast, but you skim the surface)
    PULSE_R: 17.0    // "remember" burst radius
  };

  /* ---------- deterministic random ---------- */
  function rng(seed) {
    var a = seed >>> 0;
    return function () {
      a |= 0; a = (a + 0x6D2B79F5) | 0;
      var t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  var PALETTE = [
    [0.32, 0.90, 1.00],  // cyan
    [0.56, 0.46, 1.00],  // violet
    [1.00, 0.38, 0.84],  // magenta
    [1.00, 0.74, 0.38],  // amber
    [0.66, 0.88, 1.00]   // ice
  ];

  function pickTint(r) {
    if (r < 0.40) return PALETTE[0];
    if (r < 0.68) return PALETTE[1];
    if (r < 0.80) return PALETTE[2];
    if (r < 0.87) return PALETTE[3];
    return PALETTE[4];
  }

  function accentFor(t) {
    // contrasting accent
    if (t === PALETTE[0]) return PALETTE[2];
    if (t === PALETTE[1]) return PALETTE[0];
    if (t === PALETTE[2]) return PALETTE[1];
    if (t === PALETTE[3]) return PALETTE[0];
    return PALETTE[2];
  }

  /* ---------- primitives ---------- */
  function box(x, y, z, hx, hy, hz, rot, tint) {
    return { x: x, y: y, z: z, hx: hx, hy: hy, hz: hz, rot: rot || 0, tint: tint || null };
  }

  function solidOfPart(p) {
    return { k: 0, x0: p.x - p.hx, x1: p.x + p.hx, z0: p.z - p.hz, z1: p.z + p.hz, y0: Math.max(0, p.y - p.hy), y1: p.y + p.hy };
  }

  function newStruct(parts, tint, opts) {
    var s = {
      parts: parts,
      solids: [],
      tint: tint,
      count: true,
      anim: 0,        // 1 = rebuilt on CPU every frame (rotation / bob)
      spin: 0,
      bob: 0,
      phase: 0,
      top: 0,
      cx: 0, cz: 0,
      x0: 1e9, x1: -1e9, z0: 1e9, z1: -1e9,
      revealed: false,
      revealAt: 0,
      presence: 0.055,
      clipY: 1e5,
      flash: 0,
      tiles: null
    };
    if (opts) for (var k in opts) s[k] = opts[k];
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i];
      var rad = Math.hypot(p.hx, p.hz);
      s.top = Math.max(s.top, p.y + p.hy);
      s.x0 = Math.min(s.x0, p.x - rad); s.x1 = Math.max(s.x1, p.x + rad);
      s.z0 = Math.min(s.z0, p.z - rad); s.z1 = Math.max(s.z1, p.z + rad);
    }
    s.cx = (s.x0 + s.x1) * 0.5;
    s.cz = (s.z0 + s.z1) * 0.5;
    return s;
  }

  /* ---------- tower silhouettes ---------- */
  function towerParts(R, cx, cz, w, d, h, tint, acc) {
    var parts = [];
    var hx = w * 0.5, hz = d * 0.5;
    var kind = R();
    if (h > 15 && kind < 0.72) {
      var h1 = h * (0.5 + 0.22 * R());
      parts.push(box(cx, h1 * 0.5, cz, hx, h1 * 0.5, hz));
      var s = 0.7 + R() * 0.12;
      parts.push(box(cx, h1 + (h - h1) * 0.5, cz, hx * s, (h - h1) * 0.5, hz * s));
      if (R() < 0.55) parts.push(box(cx, h - 0.16, cz, hx * s * 0.55, 0.16, hz * s * 0.55, 0, acc));
      else parts.push(box(cx, h1 * 0.86, cz, hx * 1.05, 0.14, hz * 1.05, 0, acc));
    } else if (kind < 0.9) {
      parts.push(box(cx, h * 0.5, cz, hx, h * 0.5, hz));
      if (R() < 0.6) parts.push(box(cx, h * (0.55 + 0.3 * R()), cz, hx * 1.06, 0.12, hz * 1.06, 0, acc));
    } else {
      // open frame: four legs + slab top (a "pergola" tower)
      var t = 0.34;
      var yy = h * 0.5;
      parts.push(box(cx - hx + t, yy, cz - hz + t, t, yy, t));
      parts.push(box(cx + hx - t, yy, cz - hz + t, t, yy, t));
      parts.push(box(cx - hx + t, yy, cz + hz - t, t, yy, t));
      parts.push(box(cx + hx - t, yy, cz + hz - t, t, yy, t));
      parts.push(box(cx, h - 0.3, cz, hx, 0.3, hz, 0, acc));
      parts.push(box(cx, h * 0.5, cz, hx * 0.9, 0.1, hz * 0.9));
    }
    return parts;
  }

  /* ---------- city generation ---------- */
  function generate(seed) {
    seed = (seed | 0) || 7;
    var R = rng(seed * 2654435761 + 12345);
    var S = [];
    var CUTS = [-46, -23, 0, 23, 46];
    var MAIN_HW = 3.6, SEC_HW = 2.4, EDGE_IN = 1.0;
    function insetAt(k) {
      if (k === 0 || k === 4) return EDGE_IN;
      return CUTS[k] === 0 ? MAIN_HW : SEC_HW;
    }

    function push(parts, tint, opts) {
      var s = newStruct(parts, tint, opts);
      s.phase = R() * 6.283;
      S.push(s);
      return s;
    }

    /* --- 1. the obelisk at the origin --- */
    var obeliskTint = PALETTE[1];
    var obAcc = PALETTE[0];
    var ob = push([
      box(0, 0.55, 0, 3.0, 0.55, 3.0),
      box(0, 0.55 + 9.0, 0, 1.55, 9.0, 1.55),
      box(0, 12 + 8.0, 0, 1.15, 8.0, 1.15),
      box(0, 22 + 5.5, 0, 0.75, 5.5, 0.75)
    ], obeliskTint, { count: false });
    ob.solids.push({ k: 0, x0: -3, x1: 3, z0: -3, z1: 3, y0: 0, y1: 1.1 });
    ob.solids.push({ k: 0, x0: -1.6, x1: 1.6, z0: -1.6, z1: 1.6, y0: 0, y1: 33 });
    var ob2 = push([
      box(0, 20.0, 0, 2.6, 0.18, 2.6, 0, obAcc),
      box(0, 15.0, 0, 2.4, 0.12, 2.4, 0, obAcc),
      box(0, 27.5, 0, 2.0, 0.12, 2.0, 0, obAcc),
      box(0, 10.0, 0, 2.7, 0.10, 2.7, 0, obAcc)
    ], obAcc, { count: false });
    ob2.revealAt = -1;

    /* crystal above the obelisk */
    push([
      box(0, 34.5, 0, 0.85, 1.5, 0.85, 0, PALETTE[0]),
      box(0, 34.5, 0, 0.42, 2.1, 0.42, 0.6, PALETTE[2])
    ], PALETTE[0], { count: false, anim: 1, spin: 0.35, bob: 0.55, phase: 1.2 });

    /* --- 2. plaza plinth + pavilions --- */
    var plinth = push([
      box(0, 0.11, 0, 13.5, 0.11, 13.5),
      box(0, 0.26, 0, 12.6, 0.15, 12.6)
    ], PALETTE[4], { count: false });
    plinth.solids.push({ k: 0, x0: -13.5, x1: 13.5, z0: -13.5, z1: 13.5, y0: 0, y1: 0.41 });

    for (var pi = 0; pi < 4; pi++) {
      var ang = Math.PI * 0.25 + pi * Math.PI * 0.5;
      var px = Math.cos(ang) * 10.5, pz = Math.sin(ang) * 10.5;
      var pav = [
        box(px - 1.6, 1.9, pz - 1.6, 0.13, 1.9, 0.13),
        box(px + 1.6, 1.9, pz - 1.6, 0.13, 1.9, 0.13),
        box(px - 1.6, 1.9, pz + 1.6, 0.13, 1.9, 0.13),
        box(px + 1.6, 1.9, pz + 1.6, 0.13, 1.9, 0.13),
        box(px, 3.9, pz, 1.95, 0.11, 1.95),
        box(px, 4.1, pz, 1.35, 0.09, 1.35, 0, PALETTE[2])
      ];
      push(pav, PALETTE[4], { count: false });
    }

    /* --- 3. blocks of towers --- */
    for (var bi = 0; bi < 4; bi++) {
      for (var bj = 0; bj < 4; bj++) {
        var x0 = CUTS[bi] + insetAt(bi);
        var x1 = CUTS[bi + 1] - insetAt(bi + 1);
        var z0 = CUTS[bj] + insetAt(bj);
        var z1 = CUTS[bj + 1] - insetAt(bj + 1);
        var nx = Math.max(1, Math.round((x1 - x0) / 6.7));
        var nz = Math.max(1, Math.round((z1 - z0) / 6.7));
        var lw = (x1 - x0) / nx, ld = (z1 - z0) / nz;
        for (var ax = 0; ax < nx; ax++) {
          for (var az = 0; az < nz; az++) {
            var lx0 = x0 + ax * lw, lz0 = z0 + az * ld;
            var lx1 = lx0 + lw, lz1 = lz0 + ld;
            var cx = (lx0 + lx1) * 0.5, cz = (lz0 + lz1) * 0.5;
            var dist = Math.hypot(cx, cz);
            if (dist < 15.6) continue;
            var roll = R();
            var w = lw - 0.85, d = ld - 0.85;
            if (roll < 0.30) {
              // courtyard / breathing space
              if (R() < 0.45) {
                var kw = 1.1 + R() * 1.1, kd = 1.0 + R() * 1.0, kh = 1.6 + R() * 1.4;
                var kp = [box(cx, kh * 0.5, cz, kw, kh * 0.5, kd),
                          box(cx, kh + 0.06, cz, kw * 1.12, 0.07, kd * 1.12, 0, PALETTE[3])];
                var ks = push(kp, PALETTE[3], { count: true });
                ks.solids.push(solidOfPart(kp[0]));
              }
              continue;
            }
            w -= R() * 1.5; d -= R() * 1.5;
            var hh = (5 + Math.pow(R(), 1.55) * 13) * (1 + 1.05 * Math.exp(-dist / 19));
            if (R() < 0.09) hh *= 1.55;
            hh = Math.min(hh, 35);
            var tint = pickTint(R());
            var acc = accentFor(tint);
            var parts = towerParts(R, cx, cz, w, d, hh, tint, acc);
            var st = push(parts, tint, { count: true });
            st.solids.push({ k: 0, x0: cx - w * 0.5, x1: cx + w * 0.5, z0: cz - d * 0.5, z1: cz + d * 0.5, y0: 0, y1: hh });
          }
        }
      }
    }

    /* --- 4. outer wall of towers --- */
    for (var ri = 0; ri < 34; ri++) {
      var a = (ri / 34) * Math.PI * 2;
      if (Math.abs(Math.cos(a)) < 0.13 || Math.abs(Math.sin(a)) < 0.13) continue; // keep the avenues open
      var rr = 43.2 + R() * 1.6;
      var tx = Math.cos(a) * rr, tz = Math.sin(a) * rr;
      var tw = 5.0 + R() * 4.0, td = 5.0 + R() * 4.0;
      var th = 13 + R() * 20;
      var t2 = pickTint(R());
      var tp = towerParts(R, tx, tz, tw, td, th, t2, accentFor(t2));
      var ts = push(tp, t2, { count: true });
      ts.solids.push({ k: 0, x0: tx - tw * 0.5, x1: tx + tw * 0.5, z0: tz - td * 0.5, z1: tz + td * 0.5, y0: 0, y1: th });
    }

    /* --- 5. gates on the avenues --- */
    for (var gi = 0; gi < 4; gi++) {
      var ga = gi * Math.PI * 0.5;
      var gx = Math.cos(ga) * 21, gz = Math.sin(ga) * 21;
      var archX = (gi % 2 === 1);           // gate on the Z avenue -> the arch spans X
      var legT = 0.7, span = 4.0, gy = 5.8, gtop = 7.8;
      var gparts, gs, gsides;
      if (archX) {
        gparts = [
          box(gx - span, gy * 0.5, gz, legT, gy * 0.5, legT),
          box(gx + span, gy * 0.5, gz, legT, gy * 0.5, legT),
          box(gx, (gy + gtop) * 0.5, gz, span + legT, (gtop - gy) * 0.5, legT * 1.25),
          box(gx, gtop + 0.18, gz, span + legT * 1.25, 0.18, legT * 1.6, 0, PALETTE[2])
        ];
        gs = push(gparts, PALETTE[0], { count: true });
        gs.solids.push({ k: 0, x0: gx - span - legT, x1: gx - span + legT, z0: gz - legT, z1: gz + legT, y0: 0, y1: gy });
        gs.solids.push({ k: 0, x0: gx + span - legT, x1: gx + span + legT, z0: gz - legT, z1: gz + legT, y0: 0, y1: gy });
      } else {
        gparts = [
          box(gx, gy * 0.5, gz - span, legT, gy * 0.5, legT),
          box(gx, gy * 0.5, gz + span, legT, gy * 0.5, legT),
          box(gx, (gy + gtop) * 0.5, gz, legT * 1.25, (gtop - gy) * 0.5, span + legT),
          box(gx, gtop + 0.18, gz, legT * 1.6, 0.18, span + legT * 1.25, 0, PALETTE[2])
        ];
        gs = push(gparts, PALETTE[1], { count: true });
        gs.solids.push({ k: 0, x0: gx - legT, x1: gx + legT, z0: gz - span - legT, z1: gz - span + legT, y0: 0, y1: gy });
        gs.solids.push({ k: 0, x0: gx - legT, x1: gx + legT, z0: gz + span - legT, z1: gz + span + legT, y0: 0, y1: gy });
      }
    }

    /* --- 6. sky bridges between close towers --- */
    var towers = [];
    for (var ti = 0; ti < S.length; ti++) {
      var s0 = S[ti];
      if (s0.count && s0.top > 12 && !s0.anim) towers.push(s0);
    }
    var bridges = 0;
    for (var i1 = 0; i1 < towers.length && bridges < 26; i1++) {
      if (R() > 0.20) continue;
      var A = towers[i1];
      for (var j1 = i1 + 1; j1 < towers.length; j1++) {
        var B = towers[j1];
        var dx = B.cx - A.cx, dz = B.cz - A.cz;
        var dd = Math.hypot(dx, dz);
        if (dd < 7 || dd > 13.5) continue;
        if (Math.abs(dx) < Math.abs(dz) * 1.8 && Math.abs(dz) < Math.abs(dx) * 1.8) continue;
        var by = Math.min(A.top, B.top) * (0.5 + R() * 0.18);
        if (by < 6) continue;
        var mx = (A.cx + B.cx) * 0.5, mz = (A.cz + B.cz) * 0.5;
        var half = dd * 0.5;
        var bt = PALETTE[0];
        var bparts;
        if (Math.abs(dx) > Math.abs(dz)) {
          bparts = [
            box(mx, by, mz, half + 0.4, 0.22, 0.95),
            box(mx, by + 0.85, mz - 0.9, half + 0.4, 0.07, 0.06, 0, PALETTE[2]),
            box(mx, by + 0.85, mz + 0.9, half + 0.4, 0.07, 0.06, 0, PALETTE[2]),
            box(mx, by + 0.1, mz, half + 0.42, 0.04, 1.02, 0, PALETTE[2])
          ];
        } else {
          bparts = [
            box(mx, by, mz, 0.95, 0.22, half + 0.4),
            box(mx - 0.9, by + 0.85, mz, 0.06, 0.07, half + 0.4, 0, PALETTE[2]),
            box(mx + 0.9, by + 0.85, mz, 0.06, 0.07, half + 0.4, 0, PALETTE[2]),
            box(mx, by + 0.1, mz, 1.02, 0.04, half + 0.42, 0, PALETTE[2])
          ];
        }
        push(bparts, bt, { count: true });
        bridges++;
        break;
      }
    }

    /* --- 7. floating lanterns along the avenues --- */
    for (var li = 0; li < 30; li++) {
      var lp = -41 + li * 2.9;
      if (Math.abs(lp) < 4) continue;
      var lt = (li % 3 === 0) ? PALETTE[2] : PALETTE[3];
      push([box(0, 4.3 + (li % 2) * 0.5, lp, 0.22, 0.22, 0.22, 0, lt)], lt,
        { count: false, anim: 1, spin: 0.4, bob: 0.28, phase: li * 0.7 });
      push([box(lp, 4.3 + (li % 2) * 0.5, 0, 0.22, 0.22, 0.22, 0, lt)], lt,
        { count: false, anim: 1, spin: 0.4, bob: 0.28, phase: li * 0.7 + 2.0 });
    }

    /* --- 8. floating shards above the city --- */
    for (var si = 0; si < 22; si++) {
      var sa = R() * Math.PI * 2;
      var sr = 12 + R() * 28;
      var sx = Math.cos(sa) * sr, sz = Math.sin(sa) * sr;
      var sy = 13 + R() * 20;
      var ssz = 1.2 + R() * 3.4;
      var st = (si % 5 === 0) ? PALETTE[3] : (si % 3 === 0 ? PALETTE[2] : PALETTE[0]);
      push([
        box(sx, sy, sz, ssz, ssz * (0.4 + R() * 0.9), ssz * (0.5 + R() * 0.8)),
        box(sx, sy, sz, ssz * 1.18, 0.06, ssz * 1.18, 0, PALETTE[1])
      ], st, { count: false, anim: 1, spin: 0.12 + R() * 0.2, bob: 0.4 + R() * 0.5, phase: R() * 6.283 });
    }

    /* --- 9. memory tiles for every structure --- */
    var tiles = CFG.TILES;
    for (var k1 = 0; k1 < S.length; k1++) {
      var s = S[k1];
      var arr = [];
      var tx0 = Math.floor((s.x0 + CFG.HALF) / CFG.TILE);
      var tx1 = Math.floor((s.x1 + CFG.HALF) / CFG.TILE);
      var tz0 = Math.floor((s.z0 + CFG.HALF) / CFG.TILE);
      var tz1 = Math.floor((s.z1 + CFG.HALF) / CFG.TILE);
      for (var tz = tz0; tz <= tz1; tz++) {
        for (var tx = tx0; tx <= tx1; tx++) {
          if (tx < 0 || tz < 0 || tx >= tiles || tz >= tiles) continue;
          arr.push(tz * tiles + tx);
        }
      }
      s.tiles = arr;
    }

    var total = 0;
    for (var c1 = 0; c1 < S.length; c1++) if (S[c1].count) total++;

    return {
      seed: seed,
      structures: S,
      total: total,
      obelisk: ob
    };
  }

  /* ---------- city runtime: memory, reveal, collision ---------- */
  function City(seed) {
    this.cfg = CFG;
    this.mem = new Uint8Array(CFG.TEX * CFG.TEX * 4); // R=revealed, G=presence
    this.rings = [];   // floor ripples {x,z,r0,t0,power}
    this.reset(seed);
  }

  City.prototype.reset = function (seed) {
    var w = generate(seed);
    this.seed = w.seed;
    this.structures = w.structures;
    this.total = w.total;
    this.revealedCount = 0;
    this.mem.fill(0);
    this.rings.length = 0;
    this.pending = [];
    this.countablePending = [];
    for (var i = 0; i < this.structures.length; i++) {
      var s = this.structures[i];
      s.revealed = false;
      s.presence = 0.055;
      s.clipY = 1e5;
      s.revealAt = 1e9;
      s.flash = 0;
      this.pending.push(s);
      if (s.count) this.countablePending.push(s);
    }
    // the obelisk rings + crystal are already awake
    for (var j = 1; j <= 2; j++) {
      var t = this.structures[j];
      if (!t) continue;
      t.revealed = true; t.presence = 1; t.clipY = 1e5; t.revealAt = 0;
      var at = this.pending.indexOf(t);
      if (at >= 0) this.pending.splice(at, 1);
    }
    this.stats = { steps: 0, pulses: 0, started: 0 };
  };

  City.prototype.progress = function () {
    return this.total > 0 ? this.revealedCount / this.total : 0;
  };

  City.prototype.tileAt = function (x, z) {
    var tx = Math.floor((x + CFG.HALF) / CFG.TILE);
    var tz = Math.floor((z + CFG.HALF) / CFG.TILE);
    if (tx < 0 || tz < 0 || tx >= CFG.TILES || tz >= CFG.TILES) return -1;
    return tz * CFG.TILES + tx;
  };

  /* Reveal everything within radius. Returns number of newly remembered buildings. */
  City.prototype.reveal = function (px, pz, radius, now, opts) {
    opts = opts || {};
    var t0 = Math.floor((px - radius + CFG.HALF) / CFG.TILE);
    var t1 = Math.floor((px + radius + CFG.HALF) / CFG.TILE);
    var u0 = Math.floor((pz - radius + CFG.HALF) / CFG.TILE);
    var u1 = Math.floor((pz + radius + CFG.HALF) / CFG.TILE);
    var r2 = radius * radius;
    var touched = 0;
    for (var tz = Math.max(0, u0); tz <= Math.min(CFG.TILES - 1, u1); tz++) {
      for (var tx = Math.max(0, t0); tx <= Math.min(CFG.TILES - 1, t1); tx++) {
        var cx = (tx + 0.5) * CFG.TILE - CFG.HALF;
        var cz = (tz + 0.5) * CFG.TILE - CFG.HALF;
        var dx = cx - px, dz = cz - pz;
        var d2 = dx * dx + dz * dz;
        if (d2 > r2) continue;
        var idx = (tz * CFG.TILES + tx) * 4;
        if (!this.mem[idx]) { this.mem[idx] = 255; touched++; }
        var pres = Math.round(255 * Math.max(0.18, 1 - Math.sqrt(d2) / radius));
        if (pres > this.mem[idx + 1]) this.mem[idx + 1] = pres;
      }
    }

    var newly = 0;
    if (touched) {
      var still = [];
      for (var i = 0; i < this.pending.length; i++) {
        var s = this.pending[i];
        var hit = false;
        var tl = s.tiles;
        for (var k = 0; k < tl.length; k++) {
          if (this.mem[tl[k] * 4]) { hit = true; break; }
        }
        if (hit) {
          s.revealed = true;
          var d = Math.hypot(s.cx - px, s.cz - pz);
          s.revealAt = now + Math.min(0.85, d * 0.035) + (opts.instant ? 0 : 0);
          s.flash = 1;
          newly++;
        } else still.push(s);
      }
      this.pending = still;
      // recount
      var c = 0;
      for (var q = 0; q < this.structures.length; q++) if (this.structures[q].count && this.structures[q].revealed) c++;
      this.revealedCount = c;
    }
    if (touched && opts.ripple !== false) {
      this.rings.push({ x: px, z: pz, r0: 0.5, t0: now, power: opts.power || 1 });
      if (this.rings.length > 6) this.rings.shift();
    }
    return newly;
  };

  /* advance growth animation */
  City.prototype.update = function (now, dt) {
    for (var i = 0; i < this.structures.length; i++) {
      var s = this.structures[i];
      if (!s.revealed) { s.presence = 0.055; s.clipY = 1e5; continue; }
      var t = now - s.revealAt;
      if (t <= 0) { s.presence = 0.055; s.clipY = 1e5; continue; }
      var dur = s.anim ? 1.5 : 1.25;
      var k = t / dur;
      if (k >= 1) {
        s.presence = 1; s.clipY = 1e5; s.flash = 0;
        continue;
      }
      var e = 1 - Math.pow(1 - k, 3);
      s.presence = 0.055 + 0.945 * e;
      s.clipY = s.anim ? 1e5 : s.top * (1 - Math.pow(1 - k, 2)) + 0.02;
      s.flash = Math.max(0, 1 - k * 1.6);
    }
    // floor ripples
    for (var r = this.rings.length - 1; r >= 0; r--) {
      if (now - this.rings[r].t0 > 2.6) this.rings.splice(r, 1);
    }
  };

  City.prototype.groundAt = function (x, z, feetY) {
    var best = 0;
    for (var i = 0; i < this.structures.length; i++) {
      var s = this.structures[i];
      if (!s.revealed || !s.solids.length) continue;
      var ss = s.solids;
      for (var k = 0; k < ss.length; k++) {
        var b = ss[k];
        if (x < b.x0 || x > b.x1 || z < b.z0 || z > b.z1) continue;
        if (b.y1 > feetY + 0.42) continue;
        if (b.y1 > best) best = b.y1;
      }
    }
    return best;
  };

  /* circle vs AABB, axis separated */
  City.prototype.collide = function (px, pz, r, feetY) {
    var nx = px, nz = pz;
    for (var pass = 0; pass < 2; pass++) {
      for (var i = 0; i < this.structures.length; i++) {
        var s = this.structures[i];
        if (!s.revealed || !s.solids.length) continue;
        var ss = s.solids;
        for (var k = 0; k < ss.length; k++) {
          var b = ss[k];
          if (b.y1 <= feetY + 0.42) continue;
          if (b.y0 > feetY + 1.75) continue;
          var qx = Math.max(b.x0, Math.min(nx, b.x1));
          var qz = Math.max(b.z0, Math.min(nz, b.z1));
          var dx = nx - qx, dz = nz - qz;
          var d2 = dx * dx + dz * dz;
          if (d2 >= r * r) continue;
          if (d2 > 1e-8) {
            var d = Math.sqrt(d2);
            nx = qx + (dx / d) * r;
            nz = qz + (dz / d) * r;
          } else {
            // centre inside the box: push out along the shallowest axis
            var l = nx - b.x0, rr = b.x1 - nx, u = nz - b.z0, dn = b.z1 - nz;
            var m = Math.min(l, rr, u, dn);
            if (m === l) nx = b.x0 - r;
            else if (m === rr) nx = b.x1 + r;
            else if (m === u) nz = b.z0 - r;
            else nz = b.z1 + r;
          }
        }
      }
    }
    // outer arena boundary
    var rad = Math.hypot(nx, nz);
    var lim = CFG.HALF - 0.8;
    if (rad > lim) {
      nx *= lim / rad; nz *= lim / rad;
    }
    return [nx, nz];
  };

  City.prototype.nearestUnknown = function (px, pz) {
    var best = null, bd = 1e9;
    for (var i = 0; i < this.structures.length; i++) {
      var s = this.structures[i];
      if (!s.count || s.revealed) continue;
      var d = Math.hypot(s.cx - px, s.cz - pz);
      if (d < bd) { bd = d; best = s; }
    }
    return best ? { s: best, d: bd } : null;
  };

  City.prototype.treasures = function () { return this.revealedCount; };

  root.MEM = root.MEM || {};
  root.MEM.CFG = CFG;
  root.MEM.City = City;
  root.MEM.generate = generate;
  root.MEM.rng = rng;

})(typeof window !== 'undefined' ? window : globalThis);
