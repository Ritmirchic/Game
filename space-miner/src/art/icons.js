/* =========================================================================
   SPACE MINER — original resource icons + upgrade / HUD glyph set
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, makeCanvas = U.makeCanvas, hexA = U.hexA;

  var cache = {};

  function icon(res) {
    var key = res.id;
    if (cache[key]) return cache[key];
    var S = 56, ss = 3;
    var c = makeCanvas(S * ss, S * ss);
    var ctx = c.getContext('2d');
    ctx.scale(ss, ss);
    ctx.translate(S / 2, S / 2);
    var rng = new U.RNG(U.hashStr(res.id) ^ 0x77aa);
    var col = res.color, col2 = res.color2, glow = res.glow;

    // outer glow
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var g = ctx.createRadialGradient(0, 0, 0, 0, 0, 26);
    g.addColorStop(0, hexA(glow, 0.22));
    g.addColorStop(0.55, hexA(col, 0.10));
    g.addColorStop(1, hexA(col, 0));
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(0, 0, 26, 0, 6.2832); ctx.fill();
    ctx.restore();

    var shape = res.shape || 'chunk';
    if (shape === 'ingot') drawIngot(ctx, col, col2, glow);
    else if (shape === 'crystal') drawCrystal(ctx, col, col2, glow, rng);
    else if (shape === 'orb') drawOrb(ctx, col, col2, glow);
    else if (shape === 'shard') drawShard(ctx, col, col2, glow);
    else drawChunk(ctx, col, col2, glow, rng);

    // tiny sparkles
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (var i = 0; i < 5; i++) {
      var a = rng.range(0, 6.2832), r = rng.range(13, 22);
      ctx.fillStyle = hexA('#ffffff', rng.range(0.25, 0.7));
      ctx.beginPath();
      ctx.arc(Math.cos(a) * r, Math.sin(a) * r, rng.range(0.5, 1.3), 0, 6.2832);
      ctx.fill();
    }
    ctx.restore();

    cache[key] = { canvas: c, size: S, ss: ss };
    return cache[key];
  }

  function facet(ctx, pts, c1, c2, glow) {
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
    ctx.closePath();
    var minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    pts.forEach(function (p) {
      minX = Math.min(minX, p[0]); maxX = Math.max(maxX, p[0]);
      minY = Math.min(minY, p[1]); maxY = Math.max(maxY, p[1]);
    });
    var lg = ctx.createLinearGradient(minX, minY, maxX, maxY);
    lg.addColorStop(0, glow);
    lg.addColorStop(0.35, c1);
    lg.addColorStop(1, c2);
    ctx.fillStyle = lg;
    ctx.fill();
    ctx.strokeStyle = 'rgba(0,0,0,0.45)';
    ctx.lineWidth = 0.8;
    ctx.stroke();
  }

  function drawChunk(ctx, col, col2, glow, rng) {
    var n = 7;
    var pts = [];
    for (var i = 0; i < n; i++) {
      var a = i / n * 6.2832 + rng.range(-0.15, 0.15);
      var r = rng.range(15, 21);
      pts.push([Math.cos(a) * r, Math.sin(a) * r * 0.92]);
    }
    facet(ctx, pts, col, col2, glow);
    // top facets
    facet(ctx, [pts[0], pts[1], [pts[1][0] * 0.5, pts[1][1] * 0.5 - 2], [pts[0][0] * 0.5, pts[0][1] * 0.5 - 4]], glow, hexA('#ffffff', 0.6), col);
    facet(ctx, [pts[5], pts[6], [pts[6][0] * 0.45, pts[6][1] * 0.45 + 1], [pts[5][0] * 0.5, pts[5][1] * 0.5 + 3]], col, col2, col);
    // specular
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = hexA('#ffffff', 0.35);
    ctx.beginPath();
    ctx.ellipse(-5, -7, 6, 3, -0.6, 0, 6.2832);
    ctx.fill();
    ctx.restore();
  }

  function drawIngot(ctx, col, col2, glow) {
    var pts = [[-19, -6], [15, -6], [21, 2], [-11, 2]];
    facet(ctx, pts, glow, col, col2);
    var side = [[-11, 2], [21, 2], [19, 9], [-13, 9]];
    facet(ctx, side, col, col2, col);
    var top = [[-16, -6], [12, -6], [15, -10], [-13, -10]];
    facet(ctx, top, '#ffffff', glow, col);
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = hexA('#ffffff', 0.5);
    ctx.fillRect(-14, -4.5, 20, 1.2);
    ctx.restore();
    // stamped mark
    ctx.strokeStyle = hexA('#000000', 0.35);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(3, -1); ctx.lineTo(9, -1); ctx.lineTo(9, -3.4);
    ctx.stroke();
  }

  function drawCrystal(ctx, col, col2, glow, rng) {
    var prisms = [
      { x: -6, y: 4, h: 20, w: 6, rot: 0.16 },
      { x: 6, y: 2, h: 26, w: 7, rot: -0.12 },
      { x: -13, y: 8, h: 13, w: 4.5, rot: 0.45 },
      { x: 14, y: 9, h: 12, w: 4, rot: -0.5 }
    ];
    prisms.forEach(function (p, i) {
      ctx.save();
      ctx.translate(p.x, p.y);
      ctx.rotate(p.rot);
      // body
      var g = ctx.createLinearGradient(-p.w, 0, p.w, 0);
      g.addColorStop(0, hexA('#ffffff', 0.85));
      g.addColorStop(0.4, col);
      g.addColorStop(1, col2);
      ctx.beginPath();
      ctx.moveTo(0, -p.h);
      ctx.lineTo(p.w, -p.h * 0.62);
      ctx.lineTo(p.w * 0.8, p.h * 0.5);
      ctx.lineTo(-p.w * 0.8, p.h * 0.5);
      ctx.lineTo(-p.w, -p.h * 0.62);
      ctx.closePath();
      ctx.fillStyle = g;
      ctx.fill();
      ctx.strokeStyle = hexA(glow, 0.6);
      ctx.lineWidth = 0.7;
      ctx.stroke();
      // inner facet
      ctx.beginPath();
      ctx.moveTo(0, -p.h);
      ctx.lineTo(p.w * 0.28, p.h * 0.5);
      ctx.lineTo(-p.w * 0.28, p.h * 0.5);
      ctx.closePath();
      ctx.fillStyle = hexA(glow, 0.35);
      ctx.fill();
      ctx.restore();
    });
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var g2 = ctx.createRadialGradient(0, -4, 0, 0, -4, 24);
    g2.addColorStop(0, hexA(glow, 0.35));
    g2.addColorStop(1, hexA(glow, 0));
    ctx.fillStyle = g2;
    ctx.beginPath(); ctx.arc(0, -4, 24, 0, 6.2832); ctx.fill();
    ctx.restore();
  }

  function drawOrb(ctx, col, col2, glow) {
    var g = ctx.createRadialGradient(-5, -6, 2, 0, 0, 19);
    g.addColorStop(0, '#ffffff');
    g.addColorStop(0.22, glow);
    g.addColorStop(0.6, col);
    g.addColorStop(1, col2);
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(0, 0, 18, 0, 6.2832); ctx.fill();
    ctx.strokeStyle = hexA('#000000', 0.3);
    ctx.lineWidth = 1;
    ctx.stroke();
    // swirling energy bands
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.strokeStyle = hexA('#ffffff', 0.55);
    ctx.lineWidth = 1.3;
    for (var i = 0; i < 2; i++) {
      ctx.beginPath();
      ctx.ellipse(0, 0, 14 - i * 4, 6 - i * 2, 0.6 + i * 0.7, 0, 6.2832);
      ctx.stroke();
    }
    var hg = ctx.createRadialGradient(-7, -8, 0, -7, -8, 9);
    hg.addColorStop(0, hexA('#ffffff', 0.75));
    hg.addColorStop(1, hexA('#ffffff', 0));
    ctx.fillStyle = hg;
    ctx.beginPath(); ctx.arc(-7, -8, 9, 0, 6.2832); ctx.fill();
    ctx.restore();
  }

  function drawShard(ctx, col, col2, glow) {
    var pts = [[2, -22], [11, -2], [4, 18], [-4, 22], [-9, 2], [-4, -14]];
    facet(ctx, pts, glow, col, col2);
    facet(ctx, [[2, -22], [11, -2], [4, 18], [1, 4]], '#ffffff', col, col2);
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.strokeStyle = hexA(glow, 0.8);
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(2, -22); ctx.lineTo(1, 10); ctx.stroke();
    ctx.restore();
  }

  /* ---------------- HUD glyphs ---------------- */
  function hud(ctx, kind, x, y, s, color, fill) {
    ctx.save();
    ctx.translate(x, y);
    var k = s / 24;
    ctx.scale(k, k);
    ctx.strokeStyle = color;
    ctx.fillStyle = fill || 'transparent';
    ctx.lineWidth = 1.8;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    switch (kind) {
      case 'hull':
        ctx.beginPath();
        ctx.moveTo(0, -10); ctx.lineTo(9, -6); ctx.lineTo(9, 2);
        ctx.quadraticCurveTo(9, 9, 0, 11);
        ctx.quadraticCurveTo(-9, 9, -9, 2); ctx.lineTo(-9, -6);
        ctx.closePath();
        ctx.fill(); ctx.stroke();
        break;
      case 'shield':
        ctx.beginPath();
        ctx.arc(0, 1, 9, Math.PI * 1.15, Math.PI * 1.85);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(0, -11); ctx.lineTo(0, 10);
        ctx.stroke();
        break;
      case 'energy':
        ctx.beginPath();
        ctx.moveTo(1, -11); ctx.lineTo(-7, 2); ctx.lineTo(-1, 2); ctx.lineTo(-2, 11);
        ctx.lineTo(7, -3); ctx.lineTo(1, -3);
        ctx.closePath();
        ctx.fill(); ctx.stroke();
        break;
      case 'cargo':
        ctx.beginPath();
        ctx.rect(-9, -8, 18, 16);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(-9, -2); ctx.lineTo(9, -2);
        ctx.stroke();
        break;
      case 'credits':
        ctx.beginPath();
        ctx.arc(0, 0, 9.5, 0, 6.2832);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(2.5, -4.5); ctx.lineTo(-2, -4.5);
        ctx.quadraticCurveTo(-5.5, -4.5, -5.5, 0);
        ctx.quadraticCurveTo(-5.5, 4.5, -2, 4.5);
        ctx.lineTo(3, 4.5);
        ctx.stroke();
        break;
      case 'drone':
        ctx.beginPath();
        ctx.moveTo(0, -8); ctx.lineTo(7, -3); ctx.lineTo(7, 5); ctx.lineTo(0, 9);
        ctx.lineTo(-7, 5); ctx.lineTo(-7, -3);
        ctx.closePath();
        ctx.stroke();
        ctx.beginPath(); ctx.arc(0, 1, 2.6, 0, 6.2832); ctx.stroke();
        break;
      case 'scan':
        ctx.beginPath();
        ctx.arc(0, 3, 9, Math.PI * 1.22, Math.PI * 1.78);
        ctx.stroke();
        ctx.beginPath(); ctx.moveTo(0, 3); ctx.lineTo(-6, -7); ctx.stroke();
        break;
      case 'warn':
        ctx.beginPath();
        ctx.moveTo(0, -10); ctx.lineTo(10, 8); ctx.lineTo(-10, 8);
        ctx.closePath();
        ctx.stroke();
        ctx.beginPath(); ctx.moveTo(0, -3); ctx.lineTo(0, 2.5); ctx.stroke();
        ctx.beginPath(); ctx.arc(0, 5.4, 1.1, 0, 6.2832); ctx.fillStyle = color; ctx.fill();
        break;
      case 'speed':
        ctx.beginPath(); ctx.arc(0, 2, 9, Math.PI * 1.1, Math.PI * 1.9); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(0, 2); ctx.lineTo(5.5, -4.5); ctx.stroke();
        break;
      case 'level':
        ctx.beginPath();
        ctx.moveTo(0, -10); ctx.lineTo(3, -3); ctx.lineTo(10, -2); ctx.lineTo(5, 3);
        ctx.lineTo(6, 10); ctx.lineTo(0, 6.5); ctx.lineTo(-6, 10); ctx.lineTo(-5, 3);
        ctx.lineTo(-10, -2); ctx.lineTo(-3, -3);
        ctx.closePath();
        ctx.stroke();
        break;
    }
    ctx.restore();
  }

  /* ---------------- upgrade glyphs (drawn inline by the UI) ----------- */
  var GLYPH = {
    laser: function (c) {
      c.beginPath(); c.moveTo(-9, 8); c.lineTo(3, -4); c.stroke();
      c.beginPath(); c.moveTo(-2, 8); c.lineTo(10, -4); c.stroke();
      c.beginPath(); c.arc(1, 3, 3.4, 0, 6.2832); c.stroke();
      c.beginPath(); c.moveTo(-11, -6); c.lineTo(-4, -6); c.moveTo(-11, -2); c.lineTo(-6, -2); c.stroke();
    },
    focus: function (c) {
      c.beginPath(); c.ellipse(0, 0, 9, 5.5, -0.6, 0, 6.2832); c.stroke();
      c.beginPath(); c.arc(0, 0, 2.4, 0, 6.2832); c.stroke();
      c.beginPath(); c.moveTo(-11, 9); c.lineTo(11, -9); c.stroke();
    },
    cargo: function (c) {
      c.beginPath(); c.rect(-9, -7, 18, 14); c.stroke();
      c.beginPath(); c.moveTo(-9, -2); c.lineTo(9, -2); c.moveTo(0, -7); c.lineTo(0, 7); c.stroke();
    },
    compress: function (c) {
      c.beginPath(); c.rect(-9, -9, 18, 5); c.stroke();
      c.beginPath(); c.rect(-9, 4, 18, 5); c.stroke();
      c.beginPath(); c.moveTo(0, -3); c.lineTo(0, 3); c.moveTo(-4, -1); c.lineTo(0, 3); c.lineTo(4, -1); c.stroke();
    },
    thruster: function (c) {
      c.beginPath(); c.moveTo(-8, -7); c.lineTo(4, -7); c.lineTo(4, 7); c.lineTo(-8, 7); c.closePath(); c.stroke();
      c.beginPath(); c.moveTo(4, -3); c.lineTo(10, 0); c.lineTo(4, 3); c.stroke();
      c.beginPath(); c.moveTo(-11, -3); c.lineTo(-8, 0); c.lineTo(-11, 3); c.stroke();
    },
    inertia: function (c) {
      c.beginPath(); c.arc(0, 0, 4, 0, 6.2832); c.stroke();
      c.beginPath(); c.ellipse(0, 0, 10, 5, 0.5, 0, 6.2832); c.stroke();
      c.beginPath(); c.arc(7, -6, 1.6, 0, 6.2832); c.stroke();
    },
    hull: function (c) {
      c.beginPath();
      c.moveTo(0, -10); c.lineTo(9, -6); c.lineTo(9, 2);
      c.quadraticCurveTo(9, 9, 0, 11);
      c.quadraticCurveTo(-9, 9, -9, 2); c.lineTo(-9, -6);
      c.closePath(); c.stroke();
      c.beginPath(); c.moveTo(-4, -5); c.lineTo(4, -5); c.moveTo(-4, 0); c.lineTo(4, 0); c.stroke();
    },
    shield: function (c) {
      c.beginPath(); c.arc(0, 1, 9, Math.PI * 1.12, Math.PI * 1.88); c.stroke();
      c.beginPath(); c.arc(0, 1, 5, Math.PI * 1.2, Math.PI * 1.8); c.stroke();
      c.beginPath(); c.moveTo(0, -10); c.lineTo(0, 9); c.stroke();
    },
    reactor: function (c) {
      c.beginPath(); c.arc(0, 0, 6, 0, 6.2832); c.stroke();
      c.beginPath(); c.ellipse(0, 0, 11, 4.5, 0.5, 0, 6.2832); c.stroke();
      c.beginPath(); c.ellipse(0, 0, 4.5, 11, 0.5, 0, 6.2832); c.stroke();
    },
    eco: function (c) {
      c.beginPath();
      c.moveTo(-11, 4); c.lineTo(-7, 4); c.lineTo(-4, -6); c.lineTo(0, 8); c.lineTo(3, -2); c.lineTo(6, 4); c.lineTo(11, 4);
      c.stroke();
    },
    scanner: function (c) {
      c.beginPath(); c.arc(0, 6, 10, Math.PI * 1.2, Math.PI * 1.8); c.stroke();
      c.beginPath(); c.arc(0, 6, 6, Math.PI * 1.2, Math.PI * 1.8); c.stroke();
      c.beginPath(); c.moveTo(0, 6); c.lineTo(-5, -6); c.stroke();
      c.beginPath(); c.arc(2, -4, 1.6, 0, 6.2832); c.stroke();
    },
    magnet: function (c) {
      c.beginPath(); c.arc(0, -1, 8, Math.PI, 0); c.stroke();
      c.beginPath(); c.moveTo(-8, -1); c.lineTo(-8, 8); c.moveTo(8, -1); c.lineTo(8, 8); c.stroke();
      c.beginPath(); c.moveTo(-10.5, 5.5); c.lineTo(-5.5, 5.5); c.moveTo(5.5, 5.5); c.lineTo(10.5, 5.5); c.stroke();
    },
    weapon: function (c) {
      c.beginPath(); c.rect(-10, -4, 15, 8); c.stroke();
      c.beginPath(); c.moveTo(5, -2); c.lineTo(11, -2); c.lineTo(11, 2); c.lineTo(5, 2); c.stroke();
      c.beginPath(); c.moveTo(-10, -6); c.lineTo(-10, 6); c.stroke();
    },
    drone: function (c) {
      c.beginPath();
      c.moveTo(0, -7); c.lineTo(7, -3); c.lineTo(7, 5); c.lineTo(0, 9); c.lineTo(-7, 5); c.lineTo(-7, -3);
      c.closePath(); c.stroke();
      c.beginPath(); c.arc(0, 1, 2.4, 0, 6.2832); c.stroke();
      c.beginPath(); c.moveTo(-9, -9); c.lineTo(-6, -6); c.moveTo(9, -9); c.lineTo(6, -6); c.stroke();
    },
    dronePower: function (c) {
      c.beginPath();
      c.moveTo(0, -7); c.lineTo(7, -3); c.lineTo(7, 5); c.lineTo(0, 9); c.lineTo(-7, 5); c.lineTo(-7, -3);
      c.closePath(); c.stroke();
      c.beginPath(); c.moveTo(1, -4); c.lineTo(-2, 1); c.lineTo(1, 1); c.lineTo(-1, 6); c.stroke();
    },
    nano: function (c) {
      c.beginPath(); c.arc(0, 0, 7, 0, 6.2832); c.stroke();
      c.beginPath(); c.moveTo(-3, -3); c.lineTo(3, 3); c.moveTo(3, -3); c.lineTo(-3, 3); c.stroke();
      for (var i = 0; i < 4; i++) {
        var a = i / 4 * 6.2832 + 0.7;
        c.beginPath();
        c.moveTo(Math.cos(a) * 7, Math.sin(a) * 7);
        c.lineTo(Math.cos(a) * 11, Math.sin(a) * 11);
        c.stroke();
      }
    }
  };

  SM.Icons = {
    resource: icon,
    hud: hud,
    glyph: function (ctx, id, x, y, size, color) {
      var f = GLYPH[id];
      if (!f) return;
      ctx.save();
      ctx.translate(x, y);
      var k = size / 24;
      ctx.scale(k, k);
      ctx.strokeStyle = color;
      ctx.fillStyle = color;
      ctx.lineWidth = 1.7;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      f(ctx);
      ctx.restore();
    }
  };
})(typeof window !== 'undefined' ? window : globalThis);
