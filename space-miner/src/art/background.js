/* =========================================================================
   SPACE MINER — procedural sector backdrops (nebula / stars / planets)
   Rendered once per sector into an offscreen canvas, then parallax-scrolled.
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, makeCanvas = U.makeCanvas, hexA = U.hexA, lerp = U.lerp;
  var Path = SM.Path;

  var BW = 2048, BH = 1280;

  function blob(ctx, x, y, r, color, alpha, comp) {
    var g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, hexA(color, alpha));
    g.addColorStop(0.45, hexA(color, alpha * 0.42));
    g.addColorStop(1, hexA(color, 0));
    ctx.globalCompositeOperation = comp || 'lighter';
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fill();
  }

  function softEllipse(ctx, x, y, rx, ry, rot, color, alpha, comp) {
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(rot);
    ctx.scale(1, ry / rx);
    var g = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
    g.addColorStop(0, hexA(color, alpha));
    g.addColorStop(0.55, hexA(color, alpha * 0.3));
    g.addColorStop(1, hexA(color, 0));
    ctx.globalCompositeOperation = comp || 'lighter';
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(0, 0, rx, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  function star(ctx, x, y, size, color, alpha) {
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = hexA(color, alpha);
    ctx.beginPath();
    ctx.arc(x, y, size, 0, Math.PI * 2);
    ctx.fill();
  }

  function flare(ctx, x, y, len, color, alpha) {
    ctx.globalCompositeOperation = 'lighter';
    var g = ctx.createRadialGradient(x, y, 0, x, y, len);
    g.addColorStop(0, hexA(color, alpha));
    g.addColorStop(0.25, hexA(color, alpha * 0.35));
    g.addColorStop(1, hexA(color, 0));
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(x, y, len, 0, Math.PI * 2);
    ctx.fill();
    ctx.save();
    ctx.translate(x, y);
    ctx.globalAlpha = alpha * 0.85;
    var lg = ctx.createLinearGradient(-len, 0, len, 0);
    lg.addColorStop(0, hexA(color, 0));
    lg.addColorStop(0.5, hexA(color, 1));
    lg.addColorStop(1, hexA(color, 0));
    ctx.fillStyle = lg;
    ctx.fillRect(-len, -0.7, len * 2, 1.4);
    ctx.fillRect(-0.7, -len * 0.62, 1.4, len * 1.24);
    ctx.restore();
  }

  /**
   * Build the entire sector backdrop. Deterministic per sector id.
   */
  function build(sector, seed) {
    var rng = new U.RNG(seed || U.hashStr(sector.id) ^ 0x51ab);
    var noise = new SM.Noise(seed || U.hashStr(sector.id));
    var c = makeCanvas(BW, BH);
    var ctx = c.getContext('2d');
    var pal = sector.palette;

    // --- deep space gradient ------------------------------------------------
    var g = ctx.createLinearGradient(0, 0, BW * 0.35, BH);
    g.addColorStop(0, pal.deep);
    g.addColorStop(0.45, pal.mid);
    g.addColorStop(1, pal.deep);
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, BW, BH);

    // --- distant galactic band ---------------------------------------------
    ctx.save();
    ctx.translate(BW * 0.5, BH * 0.52);
    ctx.rotate(-0.42);
    for (var i = 0; i < 26; i++) {
      var t = i / 26;
      softEllipse(ctx, (t - 0.5) * BW * 1.1 + rng.range(-90, 90), rng.range(-120, 120),
        rng.range(160, 420), rng.range(50, 140), rng.range(-0.2, 0.2),
        pal.nebula[i % pal.nebula.length], 0.05 + rng.range(0, 0.06));
    }
    ctx.restore();

    // --- big soft glow behind everything -----------------------------------
    var sunX = BW * rng.range(0.2, 0.8), sunY = BH * rng.range(0.15, 0.55);
    var sunR = Math.max(BW, BH) * rng.range(0.5, 0.75);
    var sg = ctx.createRadialGradient(sunX, sunY, 0, sunX, sunY, sunR);
    sg.addColorStop(0, hexA(pal.sun, 0.16));
    sg.addColorStop(0.35, hexA(pal.high, 0.10));
    sg.addColorStop(1, hexA(pal.deep, 0));
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = sg;
    ctx.fillRect(0, 0, BW, BH);

    // --- sculpted nebula clouds (noise-advected blobs) ---------------------
    var colors = pal.nebula;
    var clouds = 62;
    for (var k = 0; k < clouds; k++) {
      var cx = rng.range(0, BW), cy = rng.range(0, BH);
      var nv = noise.fbm(cx * 0.0022, cy * 0.0022, 4);
      // push blobs along the noise field so they group into wisps
      cx += noise.perlin(cx * 0.0016 + 9.1, cy * 0.0016) * 170;
      cy += noise.perlin(cx * 0.0016, cy * 0.0016 + 3.7) * 170;
      var col = colors[Math.floor((nv * 0.5 + 0.5) * colors.length) % colors.length];
      var rad = rng.range(120, 520) * (0.6 + Math.abs(nv));
      blob(ctx, cx, cy, rad, col, (0.035 + Math.abs(nv) * 0.075) * pal.nebulaMix);
    }

    // bright core wisps
    for (var w2 = 0; w2 < 18; w2++) {
      var wx = rng.range(0, BW), wy = rng.range(0, BH);
      blob(ctx, wx, wy, rng.range(40, 150), colors[w2 % colors.length], 0.10 * pal.nebulaMix);
      blob(ctx, wx, wy, rng.range(14, 46), '#ffffff', 0.05 * pal.nebulaMix);
    }

    // --- dark dust lanes ---------------------------------------------------
    ctx.globalCompositeOperation = 'source-over';
    for (var d = 0; d < 18; d++) {
      softEllipse(ctx, rng.range(0, BW), rng.range(0, BH),
        rng.range(160, 520), rng.range(40, 130), rng.range(0, Math.PI),
        '#000000', rng.range(0.05, 0.16) * pal.nebulaMix, 'source-over');
    }

    // --- distant planet / landmark ----------------------------------------
    if (sector.index !== 2 && sector.index !== 3) drawPlanet(ctx, rng, pal, sector);

    // --- star field --------------------------------------------------------
    var stars = 2200;
    for (var s = 0; s < stars; s++) {
      var sx = rng.range(0, BW), sy = rng.range(0, BH);
      var depth = rng.next();
      var size = depth < 0.82 ? rng.range(0.35, 0.9) : rng.range(0.9, 1.9);
      var a = depth < 0.82 ? rng.range(0.10, 0.45) : rng.range(0.5, 0.95);
      var tint = rng.next();
      var sc = tint < 0.7 ? pal.star : tint < 0.85 ? pal.accent : '#ffd9b0';
      star(ctx, sx, sy, size, sc, a);
    }
    // bright flare stars
    for (var f = 0; f < 26; f++) {
      flare(ctx, rng.range(0, BW), rng.range(0, BH), rng.range(16, 54), pal.star, rng.range(0.12, 0.4));
    }

    // --- vignette baked in -------------------------------------------------
    ctx.globalCompositeOperation = 'source-over';
    var vg = ctx.createRadialGradient(BW / 2, BH / 2, Math.min(BW, BH) * 0.25, BW / 2, BH / 2, Math.max(BW, BH) * 0.72);
    vg.addColorStop(0, 'rgba(0,0,0,0)');
    vg.addColorStop(1, 'rgba(0,0,0,0.55)');
    ctx.fillStyle = vg;
    ctx.fillRect(0, 0, BW, BH);

    ctx.globalCompositeOperation = 'source-over';
    return c;
  }

  function drawPlanet(ctx, rng, pal, sector) {
    var x = rng.chance(0.5) ? rng.range(BW * 0.04, BW * 0.24) : rng.range(BW * 0.76, BW * 0.96);
    var y = rng.chance(0.5) ? rng.range(BH * 0.04, BH * 0.2) : rng.range(BH * 0.8, BH * 0.96);
    var r = rng.range(100, 170);
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    // atmosphere halo
    var halo = ctx.createRadialGradient(x, y, r * 0.9, x, y, r * 2.1);
    halo.addColorStop(0, hexA(pal.sun, 0.045));
    halo.addColorStop(1, hexA(pal.sun, 0));
    ctx.fillStyle = halo;
    ctx.beginPath(); ctx.arc(x, y, r * 2.1, 0, Math.PI * 2); ctx.fill();

    // lit crescent
    var lg = ctx.createRadialGradient(x - r * 0.45, y - r * 0.45, r * 0.1, x, y, r * 1.05);
    lg.addColorStop(0, hexA(pal.sun, 0.15));
    lg.addColorStop(0.55, hexA(pal.high, 0.08));
    lg.addColorStop(1, hexA(pal.deep, 0));
    ctx.fillStyle = lg;
    ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill();

    // dark limb
    ctx.globalCompositeOperation = 'source-over';
    var limb = ctx.createRadialGradient(x + r * 0.3, y + r * 0.25, r * 0.2, x, y, r);
    limb.addColorStop(0, 'rgba(0,0,0,0)');
    limb.addColorStop(0.86, 'rgba(0,0,0,0.10)');
    limb.addColorStop(1, 'rgba(0,0,0,0.42)');
    ctx.fillStyle = limb;
    ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill();

    // surface bands, faint
    ctx.globalCompositeOperation = 'lighter';
    ctx.save();
    ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.clip();
    for (var i = 0; i < 9; i++) {
      var by = y - r + (i + rng.range(-0.2, 0.2)) * (r * 2 / 9);
      softEllipse(ctx, x, by, r * 1.2, rng.range(8, 30), rng.range(-0.1, 0.1),
        pal.nebula[i % pal.nebula.length], rng.range(0.04, 0.1));
    }
    ctx.restore();

    // ring for some sectors
    if (sector.index === 1 || sector.index === 4) {
      ctx.globalCompositeOperation = 'lighter';
      ctx.save();
      ctx.translate(x, y);
      ctx.rotate(-0.5);
      ctx.scale(1, 0.24);
      ctx.strokeStyle = hexA(pal.sun, 0.18);
      ctx.lineWidth = r * 0.06;
      ctx.beginPath(); ctx.arc(0, 0, r * 1.6, 0, Math.PI * 2); ctx.stroke();
      ctx.strokeStyle = hexA(pal.star, 0.10);
      ctx.lineWidth = r * 0.03;
      ctx.beginPath(); ctx.arc(0, 0, r * 1.82, 0, Math.PI * 2); ctx.stroke();
      ctx.restore();
    }
    ctx.restore();
  }

  /* ------- runtime parallax star / dust layers (cheap, twinkling) -------- */
  function starLayer(seed, count, spread, palette) {
    var rng = new U.RNG(seed);
    var arr = [];
    for (var i = 0; i < count; i++) {
      arr.push({
        x: rng.range(0, spread), y: rng.range(0, spread),
        r: rng.range(0.4, 1.7), a: rng.range(0.2, 0.85),
        tw: rng.range(0.6, 2.4), ph: rng.range(0, 6.283),
        c: rng.chance(0.14) ? palette.accent : rng.chance(0.1) ? '#ffd9b0' : palette.star
      });
    }
    return arr;
  }

  function dustLayer(seed, count, spread, palette) {
    var rng = new U.RNG(seed);
    var arr = [];
    for (var i = 0; i < count; i++) {
      arr.push({
        x: rng.range(0, spread), y: rng.range(0, spread),
        r: rng.range(0.6, 2.2), a: rng.range(0.05, 0.22),
        vx: rng.range(-6, 6), vy: rng.range(-6, 6), c: palette.dust
      });
    }
    return arr;
  }

  SM.Art = SM.Art || {};
  SM.Art.background = build;
  SM.Art.starLayer = starLayer;
  SM.Art.dustLayer = dustLayer;
})(typeof window !== 'undefined' ? window : globalThis);
