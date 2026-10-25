/* =========================================================================
   SPACE MINER — procedural asteroid bodies.
   Produces an irregular collision silhouette + a pre-rendered, detailed
   texture (strata, craters, mineral veins, rim light) cached per variant.
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, makeCanvas = U.makeCanvas, hexA = U.hexA, Path = SM.Path;

  var BUCKET = { s: 82, m: 132, l: 196 };

  var FAMILY = {
    rocky: {
      base: '#5f6773', base2: '#2b3038', light: '#b3bdc9', dark: '#1a1f25',
      crater: 12, grain: 1500, chips: 10, strata: 0.55, emissive: 0.0
    },
    metallic: {
      base: '#54616e', base2: '#26303a', light: '#c6d8e5', dark: '#151d24',
      crater: 6, grain: 900, chips: 6, strata: 0.8, emissive: 0.0
    },
    crystal: {
      base: '#38647a', base2: '#152836', light: '#b0f4ff', dark: '#0a1a22',
      crater: 3, grain: 500, chips: 4, strata: 0.3, emissive: 0.55
    },
    wreck: {
      base: '#545c55', base2: '#232725', light: '#b3c0b5', dark: '#101312',
      crater: 4, grain: 800, chips: 12, strata: 0.9, emissive: 0.0
    },
    molten: {
      base: '#452f38', base2: '#1c1116', light: '#ffab6d', dark: '#0b0606',
      crater: 8, grain: 1100, chips: 8, strata: 0.4, emissive: 1.0
    },
    void: {
      base: '#2a2450', base2: '#0e0b1c', light: '#b49ae0', dark: '#050310',
      crater: 5, grain: 700, chips: 6, strata: 0.35, emissive: 0.7
    }
  };

  /* ---------- silhouette ---------- */
  function makeSilhouette(rng, R) {
    var n = rng.int(19, 27);
    var pts = [];
    var h = [rng.range(0.06, 0.17), rng.range(0.05, 0.13), rng.range(0.03, 0.09)];
    var p = [rng.range(0, 6.28), rng.range(0, 6.28), rng.range(0, 6.28)];
    var notchA = rng.range(0, 6.28);
    var notchW = rng.range(0.35, 0.75);
    var notchD = rng.range(0.16, 0.34);
    var squash = rng.range(0.82, 1.06);
    var rot = rng.range(0, 6.28);
    var cosR = Math.cos(rot), sinR = Math.sin(rot);
    for (var i = 0; i < n; i++) {
      var a = i / n * Math.PI * 2;
      var r = 1
        + Math.sin(a * 2 + p[0]) * h[0]
        + Math.sin(a * 3 + p[1]) * h[1] * 0.8
        + Math.sin(a * 5 + p[2]) * h[2] * 0.6;
      var da = Math.abs(U.angleDiff(notchA, a));
      if (da < notchW) r -= notchD * (1 - da / notchW) * (1 - da / notchW);
      var x = Math.cos(a) * r * R * squash;
      var y = Math.sin(a) * r * R;
      pts.push({ x: x * cosR - y * sinR, y: x * sinR + y * cosR });
    }
    return pts;
  }

  function pathFrom(ctx, pts, sx, sy, scale) {
    ctx.beginPath();
    for (var i = 0; i < pts.length; i++) {
      var x = sx + pts[i].x * scale, y = sy + pts[i].y * scale;
      if (i === 0) ctx.moveTo(x, y);
      else {
        var px = sx + pts[(i - 1) % pts.length].x * scale;
        var py = sy + pts[(i - 1) % pts.length].y * scale;
        ctx.quadraticCurveTo((px + x) / 2 + (py - y) * 0.06, (py + y) / 2 + (x - px) * 0.06, x, y);
      }
    }
    ctx.closePath();
  }

  /* ---------- main texture builder ---------- */
  function buildBody(family, R, variant, mineralColor, mineralId, seed) {
    var cfg = FAMILY[family] || FAMILY.rocky;
    var rng = new U.RNG(seed);
    var pad = R * 0.42;
    var size = (R + pad) * 2;
    var c = makeCanvas(size, size);
    var ctx = c.getContext('2d');
    var cx = size / 2, cy = size / 2;

    var pts = makeSilhouette(rng, R);
    var crackCanvas = makeCanvas(size, size);
    var cctx = crackCanvas.getContext('2d');

    // ---- silhouette mask + base shading
    ctx.save();
    pathFrom(ctx, pts, cx, cy, 1);
    ctx.clip();

    var g = ctx.createRadialGradient(cx - R * 0.52, cy - R * 0.55, R * 0.08, cx, cy, R * 1.32);
    g.addColorStop(0, cfg.light);
    g.addColorStop(0.22, U.mixHex(cfg.light, cfg.base, 0.55));
    g.addColorStop(0.52, cfg.base);
    g.addColorStop(0.82, cfg.base2);
    g.addColorStop(1, cfg.dark);
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, size, size);

    // broad directional light wash from the upper-left key light
    var kw = ctx.createLinearGradient(cx - R, cy - R, cx + R * 0.6, cy + R * 0.8);
    kw.addColorStop(0, hexA('#ffffff', 0.16));
    kw.addColorStop(0.42, hexA('#ffffff', 0.03));
    kw.addColorStop(1, hexA('#000000', 0.30));
    ctx.fillStyle = kw;
    ctx.fillRect(0, 0, size, size);

    // ---- large tonal blotches
    for (var b = 0; b < 26; b++) {
      var bx = cx + rng.range(-R, R), by = cy + rng.range(-R, R);
      var br = rng.range(R * 0.18, R * 0.62);
      var bg = ctx.createRadialGradient(bx, by, 0, bx, by, br);
      var tint = rng.chance(0.5) ? cfg.light : cfg.dark;
      bg.addColorStop(0, hexA(tint, rng.range(0.05, 0.16)));
      bg.addColorStop(1, hexA(tint, 0));
      ctx.fillStyle = bg;
      ctx.beginPath(); ctx.arc(bx, by, br, 0, 6.2832); ctx.fill();
    }

    // ---- strata / facet bands
    if (cfg.strata > 0) {
      ctx.save();
      ctx.lineWidth = Math.max(1, R * 0.022);
      for (var s = 0; s < 9; s++) {
        var ang = rng.range(-0.5, 0.5) + (family === 'metallic' ? 0.35 : 0);
        var off = rng.range(-R, R);
        ctx.strokeStyle = hexA(rng.chance(0.5) ? cfg.dark : cfg.light, rng.range(0.05, 0.15) * cfg.strata);
        ctx.beginPath();
        ctx.save();
        ctx.translate(cx, cy);
        ctx.rotate(ang);
        ctx.moveTo(-R * 1.3, off);
        ctx.bezierCurveTo(-R * 0.3, off + rng.range(-R * 0.2, R * 0.2), R * 0.3, off + rng.range(-R * 0.25, R * 0.25), R * 1.3, off);
        ctx.stroke();
        ctx.restore();
      }
      ctx.restore();
    }

    // ---- rock facet plates (adds readable surface structure)
    if (family === 'rocky' || family === 'metallic' || family === 'void' || family === 'molten') {
      var facets = rng.int(10, 18);
      for (var fp = 0; fp < facets; fp++) {
        var fax = cx + rng.range(-R, R), fay = cy + rng.range(-R, R);
        var far = rng.range(R * 0.12, R * 0.34);
        var fang = rng.range(0, 6.2832);
        var fverts = rng.int(3, 5);
        ctx.beginPath();
        for (var fv = 0; fv < fverts; fv++) {
          var fa2 = fang + (fv / fverts) * 6.2832;
          var fr2 = far * rng.range(0.6, 1.15);
          var px2 = fax + Math.cos(fa2) * fr2, py2 = fay + Math.sin(fa2) * fr2;
          if (fv === 0) ctx.moveTo(px2, py2); else ctx.lineTo(px2, py2);
        }
        ctx.closePath();
        var litUp = fay < cy;
        ctx.fillStyle = hexA(litUp ? cfg.light : cfg.dark, rng.range(0.03, 0.10));
        ctx.fill();
        ctx.strokeStyle = hexA(litUp ? '#ffffff' : '#000000', rng.range(0.05, 0.14));
        ctx.lineWidth = 1;
        ctx.stroke();
      }
    }

    // ---- wreck plating
    if (family === 'wreck') {
      ctx.save();
      var plates = rng.int(7, 12);
      for (var pl = 0; pl < plates; pl++) {
        var px = cx + rng.range(-R, R), py = cy + rng.range(-R, R);
        var pw = rng.range(R * 0.3, R * 0.85), ph = rng.range(R * 0.22, R * 0.6);
        var prot = rng.range(0, 3.14);
        ctx.save();
        ctx.translate(px, py); ctx.rotate(prot);
        ctx.fillStyle = hexA(rng.chance(0.5) ? cfg.light : cfg.base2, rng.range(0.10, 0.28));
        ctx.fillRect(-pw / 2, -ph / 2, pw, ph);
        ctx.strokeStyle = hexA('#000000', 0.45);
        ctx.lineWidth = 1.4;
        ctx.strokeRect(-pw / 2, -ph / 2, pw, ph);
        // rivets
        ctx.fillStyle = hexA(cfg.light, 0.25);
        for (var rv = 0; rv < 5; rv++) {
          ctx.beginPath();
          ctx.arc(-pw / 2 + 6 + rv * (pw - 12) / 4, -ph / 2 + 5, Math.max(0.8, R * 0.014), 0, 6.2832);
          ctx.fill();
        }
        // hazard stripe
        if (rng.chance(0.35)) {
          ctx.fillStyle = hexA('#d8a13a', 0.30);
          for (var hs = 0; hs < 4; hs++) {
            ctx.fillRect(-pw / 2 + hs * pw / 4, ph / 2 - 8, pw / 8, 6);
          }
        }
        ctx.restore();
      }
      ctx.restore();
    }

    // ---- craters
    var craters = rng.int(Math.max(2, cfg.crater - 4), cfg.crater);
    for (var cr = 0; cr < craters; cr++) {
      var ang2 = rng.range(0, 6.2832), rad2 = Math.sqrt(rng.next()) * R * 0.86;
      var cxx = cx + Math.cos(ang2) * rad2, cyy = cy + Math.sin(ang2) * rad2;
      var cR = rng.range(R * 0.06, R * 0.26);
      ctx.save();
      ctx.globalCompositeOperation = 'source-over';
      var cg = ctx.createRadialGradient(cxx - cR * 0.3, cyy - cR * 0.35, cR * 0.1, cxx, cyy, cR);
      cg.addColorStop(0, hexA(cfg.dark, 0.62));
      cg.addColorStop(0.7, hexA(cfg.dark, 0.28));
      cg.addColorStop(1, hexA(cfg.dark, 0));
      ctx.fillStyle = cg;
      ctx.beginPath(); ctx.arc(cxx, cyy, cR, 0, 6.2832); ctx.fill();
      // lit rim on upper-left
      ctx.strokeStyle = hexA(cfg.light, 0.30);
      ctx.lineWidth = Math.max(1, cR * 0.16);
      ctx.beginPath();
      ctx.arc(cxx, cyy, cR * 0.92, Math.PI * 1.05, Math.PI * 1.85);
      ctx.stroke();
      // shadow rim lower-right
      ctx.strokeStyle = hexA('#000000', 0.35);
      ctx.beginPath();
      ctx.arc(cxx, cyy, cR * 0.92, Math.PI * 0.05, Math.PI * 0.85);
      ctx.stroke();
      ctx.restore();
    }

    // ---- molten emissive cracks
    if (family === 'molten' || family === 'void') {
      var glowC = family === 'molten' ? '#ff7a3c' : '#9b6cff';
      for (var m = 0; m < 7; m++) {
        ctx.save();
        ctx.globalCompositeOperation = 'lighter';
        ctx.strokeStyle = hexA(glowC, 0.55);
        ctx.lineWidth = Math.max(1, R * rng.range(0.012, 0.035));
        ctx.shadowColor = glowC;
        ctx.shadowBlur = R * 0.18;
        var a0 = rng.range(0, 6.2832);
        var x0 = cx + Math.cos(a0) * R * 0.15, y0 = cy + Math.sin(a0) * R * 0.15;
        ctx.beginPath();
        ctx.moveTo(x0, y0);
        var a1 = a0 + rng.range(-0.8, 0.8);
        for (var seg = 0; seg < 4; seg++) {
          a1 += rng.range(-0.7, 0.7);
          var len = R * rng.range(0.18, 0.36);
          x0 += Math.cos(a1) * len; y0 += Math.sin(a1) * len;
          ctx.lineTo(x0, y0);
        }
        ctx.stroke();
        ctx.restore();
      }
    }

    // ---- crystal facet planes
    if (family === 'crystal') {
      for (var f2 = 0; f2 < 16; f2++) {
        var fx = cx + rng.range(-R, R), fy = cy + rng.range(-R, R);
        var fr = rng.range(R * 0.1, R * 0.34);
        var fa = rng.range(0, 6.2832);
        ctx.beginPath();
        ctx.moveTo(fx + Math.cos(fa) * fr, fy + Math.sin(fa) * fr);
        ctx.lineTo(fx + Math.cos(fa + 2.2) * fr * 0.9, fy + Math.sin(fa + 2.2) * fr * 0.9);
        ctx.lineTo(fx + Math.cos(fa + 3.9) * fr * 0.75, fy + Math.sin(fa + 3.9) * fr * 0.75);
        ctx.closePath();
        ctx.fillStyle = hexA(rng.chance(0.55) ? cfg.light : '#bff0ff', rng.range(0.05, 0.16));
        ctx.fill();
        ctx.strokeStyle = hexA('#dffaff', rng.range(0.08, 0.22));
        ctx.lineWidth = 1;
        ctx.stroke();
      }
    }

    // ---- grain
    var grain = cfg.grain;
    for (var gr = 0; gr < grain; gr++) {
      var gx = cx + rng.range(-R * 1.1, R * 1.1), gy = cy + rng.range(-R * 1.1, R * 1.1);
      var gsz = rng.range(0.5, 2.1);
      ctx.fillStyle = hexA(rng.chance(0.5) ? '#ffffff' : '#000000', rng.range(0.03, 0.14));
      ctx.fillRect(gx, gy, gsz, gsz);
    }

    // ---- mineral veins (the payoff colour)
    var veinCount = rng.int(2, 5);
    var veins = [];
    for (var v = 0; v < veinCount; v++) {
      var va = rng.range(0, 6.2832);
      var vx = cx + Math.cos(va) * rng.range(0, R * 0.35);
      var vy = cy + Math.sin(va) * rng.range(0, R * 0.35);
      var vlen = rng.range(R * 0.45, R * 0.95);
      var vend = va + rng.range(-0.6, 0.6);
      var ex = vx + Math.cos(vend) * vlen, ey = vy + Math.sin(vend) * vlen;
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      ctx.strokeStyle = hexA(mineralColor, 0.75);
      ctx.lineWidth = Math.max(1.2, R * rng.range(0.015, 0.04));
      ctx.shadowColor = mineralColor;
      ctx.shadowBlur = R * 0.16;
      ctx.beginPath();
      ctx.moveTo(vx, vy);
      var px2 = vx, py2 = vy;
      for (var st = 1; st <= 4; st++) {
        var t = st / 4;
        var mx = U.lerp(vx, ex, t) + rng.range(-R * 0.12, R * 0.12);
        var my = U.lerp(vy, ey, t) + rng.range(-R * 0.12, R * 0.12);
        ctx.lineTo(mx, my);
        px2 = mx; py2 = my;
      }
      ctx.stroke();
      ctx.restore();
      veins.push({ x: px2, y: py2, a: vend, s: R * rng.range(0.1, 0.18) });

      // mineral cluster at the end
      if (rng.chance(0.75)) {
        drawCluster(ctx, px2, py2, R * rng.range(0.10, 0.20), mineralColor, rng);
      }
    }

    // ---- inner edge shading (ambient occlusion)
    ctx.save();
    pathFrom(ctx, pts, cx, cy, 1);
    ctx.clip();
    var ag = ctx.createRadialGradient(cx, cy, R * 0.55, cx, cy, R * 1.15);
    ag.addColorStop(0, 'rgba(0,0,0,0)');
    ag.addColorStop(1, 'rgba(0,0,0,0.55)');
    ctx.fillStyle = ag;
    ctx.fillRect(0, 0, size, size);
    ctx.restore();

    ctx.restore(); // unclip

    // ---- rim light (drawn outside clip so the edge glows)
    ctx.save();
    pathFrom(ctx, pts, cx, cy, 1);
    var rim = ctx.createLinearGradient(cx - R, cy - R, cx + R, cy + R);
    rim.addColorStop(0, hexA('#ffffff', 0.62));
    rim.addColorStop(0.3, hexA(cfg.light, 0.34));
    rim.addColorStop(0.6, hexA('#000000', 0.05));
    rim.addColorStop(1, hexA('#000000', 0.42));
    ctx.strokeStyle = rim;
    ctx.lineWidth = Math.max(1.6, R * 0.026);
    ctx.stroke();
    ctx.restore();

    // ---- loose chips around silhouette
    for (var ch = 0; ch < Math.round(cfg.chips * 0.5); ch++) {
      var ca = rng.range(0, 6.2832);
      var cr2 = R * rng.range(1.02, 1.14);
      var chx = cx + Math.cos(ca) * cr2, chy = cy + Math.sin(ca) * cr2;
      var chs = rng.range(R * 0.008, R * 0.028);
      ctx.fillStyle = hexA(cfg.base2, rng.range(0.25, 0.55));
      ctx.beginPath();
      ctx.moveTo(chx, chy - chs);
      ctx.lineTo(chx + chs, chy + chs * 0.7);
      ctx.lineTo(chx - chs * 0.8, chy + chs * 0.5);
      ctx.closePath();
      ctx.fill();
    }

    // ---- crack overlay (revealed as the rock is damaged)
    cctx.save();
    pathFrom(cctx, pts, cx, cy, 1);
    cctx.clip();
    for (var ck = 0; ck < 16; ck++) {
      var ka = rng.range(0, 6.2832);
      var kx = cx + Math.cos(ka) * R * 0.1, ky = cy + Math.sin(ka) * R * 0.1;
      cctx.strokeStyle = hexA('#000000', rng.range(0.35, 0.6));
      cctx.lineWidth = Math.max(0.8, R * rng.range(0.008, 0.022));
      cctx.beginPath();
      cctx.moveTo(kx, ky);
      var ka2 = ka;
      for (var ks = 0; ks < 5; ks++) {
        ka2 += rng.range(-0.9, 0.9);
        kx += Math.cos(ka2) * R * rng.range(0.12, 0.3);
        ky += Math.sin(ka2) * R * rng.range(0.12, 0.3);
        cctx.lineTo(kx, ky);
      }
      cctx.stroke();
      // hot inner glow along some cracks
      if (ck % 3 === 0) {
        cctx.save();
        cctx.globalCompositeOperation = 'lighter';
        cctx.strokeStyle = hexA(mineralColor, 0.5);
        cctx.lineWidth = 1.2;
        cctx.stroke();
        cctx.restore();
      }
    }
    cctx.restore();

    return { sprite: c, cracks: crackCanvas, pts: pts };
  }

  function drawCluster(ctx, x, y, s, color, rng) {
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(rng.range(0, 6.2832));
    var n = rng.int(3, 5);
    for (var i = 0; i < n; i++) {
      var a = (i / n) * 6.2832 + rng.range(-0.5, 0.5);
      var len = s * rng.range(0.55, 1.15);
      var w = s * rng.range(0.14, 0.26);
      var cx2 = Math.cos(a) * len * 0.45, cy2 = Math.sin(a) * len * 0.45;
      ctx.beginPath();
      ctx.moveTo(cx2 + Math.cos(a) * len * 0.7, cy2 + Math.sin(a) * len * 0.7);
      ctx.lineTo(cx2 + Math.cos(a + 1.7) * w, cy2 + Math.sin(a + 1.7) * w);
      ctx.lineTo(cx2 - Math.cos(a) * len * 0.5, cy2 - Math.sin(a) * len * 0.5);
      ctx.lineTo(cx2 + Math.cos(a - 1.7) * w, cy2 + Math.sin(a - 1.7) * w);
      ctx.closePath();
      var g = ctx.createLinearGradient(cx2 - len * 0.3, cy2 - len * 0.3, cx2 + len * 0.7, cy2 + len * 0.7);
      g.addColorStop(0, hexA(U.shade(color, 0.35), 0.55));
      g.addColorStop(0.55, hexA(color, 0.92));
      g.addColorStop(1, hexA(U.shade(color, -0.45), 0.85));
      ctx.fillStyle = g;
      ctx.fill();
      ctx.strokeStyle = hexA('#ffffff', 0.35);
      ctx.lineWidth = 0.8;
      ctx.stroke();
    }
    // tiny hot core
    ctx.globalCompositeOperation = 'lighter';
    var gg = ctx.createRadialGradient(0, 0, 0, 0, 0, s * 1.1);
    gg.addColorStop(0, hexA(color, 0.5));
    gg.addColorStop(1, hexA(color, 0));
    ctx.fillStyle = gg;
    ctx.beginPath(); ctx.arc(0, 0, s * 1.1, 0, 6.2832); ctx.fill();
    ctx.restore();
  }

  /* ---------- cache ---------- */
  var cache = {};
  var cacheKeys = [];

  function get(family, radius, variant, mineral) {
    var bucket = radius < 50 ? 's' : radius < 92 ? 'm' : 'l';
    var key = family + '|' + bucket + '|' + variant + '|' + mineral;
    if (cache[key]) return cache[key];
    var base = BUCKET[bucket];
    var res = SM.Resources.get(mineral) || SM.Resources.list[0];
    var body = buildBody(family, base, variant, res.color, mineral,
      U.hashStr(key) ^ 0x2f9a3c1);
    body.bucket = bucket;
    body.baseRadius = base;
    body.size = base * 1.42;
    cache[key] = body;
    cacheKeys.push(key);
    if (cacheKeys.length > 160) {
      var old = cacheKeys.shift();
      delete cache[old];
    }
    return body;
  }

  SM.AsteroidArt = { get: get, FAMILY: FAMILY, BUCKET: BUCKET, makeSilhouette: makeSilhouette };
})(typeof window !== 'undefined' ? window : globalThis);
