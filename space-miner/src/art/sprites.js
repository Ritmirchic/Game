/* =========================================================================
   SPACE MINER — hand-authored vector sprite artwork (ships, enemies, station)
   Everything is drawn with paths + gradients onto cached, supersampled canvases.
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, makeCanvas = U.makeCanvas, hexA = U.hexA;

  /* ------------------------------------------------------------------ */
  function sprite(w, h, ss, draw) {
    var c = makeCanvas(w * ss, h * ss);
    var ctx = c.getContext('2d');
    ctx.save();
    ctx.scale(ss, ss);
    ctx.translate(w / 2, h / 2);
    draw(ctx);
    ctx.restore();
    return { canvas: c, w: w, h: h, ss: ss };
  }

  function poly(ctx, pts, fill, stroke, lw) {
    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var i = 1; i < pts.length; i++) {
      var p = pts[i];
      if (p.len) ctx.arcTo(p[0], p[1], pts[(i + 1) % pts.length][0], pts[(i + 1) % pts.length][1], p.len);
      else ctx.lineTo(p[0], p[1]);
    }
    ctx.closePath();
    if (fill) { ctx.fillStyle = fill; ctx.fill(); }
    if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = lw || 1; ctx.stroke(); }
  }

  function lin(ctx, x0, y0, x1, y1, stops) {
    var g = ctx.createLinearGradient(x0, y0, x1, y1);
    for (var i = 0; i < stops.length; i++) g.addColorStop(stops[i][0], stops[i][1]);
    return g;
  }

  function bolts(ctx, pts, color, step) {
    ctx.fillStyle = color;
    for (var i = 0; i < pts.length; i++) {
      var a = pts[i], b = pts[(i + 1) % pts.length];
      var len = Math.hypot(b[0] - a[0], b[1] - a[1]);
      var n = Math.max(1, Math.floor(len / (step || 9)));
      for (var k = 1; k < n; k++) {
        var t = k / n;
        ctx.beginPath();
        ctx.arc(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, 0.7, 0, 6.2832);
        ctx.fill();
      }
    }
  }

  function nozzle(ctx, x, y, r, hot) {
    var g = ctx.createRadialGradient(x, y, r * 0.15, x, y, r);
    g.addColorStop(0, hot || '#dff6ff');
    g.addColorStop(0.45, 'rgba(120,200,255,0.75)');
    g.addColorStop(1, 'rgba(20,40,60,0.9)');
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(x, y, r, 0, 6.2832); ctx.fill();
    ctx.strokeStyle = 'rgba(160,215,255,0.55)';
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.strokeStyle = 'rgba(10,18,26,0.9)';
    ctx.lineWidth = 1.6;
    ctx.beginPath(); ctx.arc(x, y, r * 0.55, 0, 6.2832); ctx.stroke();
  }

  function glass(ctx, x, y, w, h, rot, tint) {
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(rot || 0);
    ctx.beginPath();
    ctx.ellipse(0, 0, w, h, 0, 0, 6.2832);
    ctx.fillStyle = lin(ctx, -w, -h, w, h, [
      [0, tint || '#0d2b3a'],
      [0.42, '#1d6b84'],
      [0.6, '#63d6e8'],
      [0.78, '#0f3547'],
      [1, '#061019']
    ]);
    ctx.fill();
    ctx.strokeStyle = 'rgba(190,240,255,0.55)';
    ctx.lineWidth = 1;
    ctx.stroke();
    // specular streaks
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = 'rgba(220,250,255,0.35)';
    ctx.beginPath();
    ctx.ellipse(-w * 0.25, -h * 0.3, w * 0.36, h * 0.16, -0.5, 0, 6.2832);
    ctx.fill();
    ctx.restore();
  }

  function panelLines(ctx, lines, color) {
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 0.8;
    ctx.beginPath();
    for (var i = 0; i < lines.length; i++) {
      var l = lines[i];
      ctx.moveTo(l[0], l[1]);
      for (var k = 2; k < l.length; k += 2) ctx.lineTo(l[k], l[k + 1]);
    }
    ctx.stroke();
    ctx.restore();
  }

  function stripes(ctx, x, y, w, h, color, n, gap) {
    ctx.save();
    ctx.fillStyle = color;
    var step = gap || 7;
    for (var i = 0; i < n; i++) ctx.fillRect(x + i * step, y, 3.2, h);
    ctx.restore();
  }

  function runningLights(ctx, pts, color) {
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (var i = 0; i < pts.length; i++) {
      var g = ctx.createRadialGradient(pts[i][0], pts[i][1], 0, pts[i][0], pts[i][1], 4);
      g.addColorStop(0, color);
      g.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(pts[i][0], pts[i][1], 4, 0, 6.2832); ctx.fill();
      ctx.fillStyle = '#ffffff';
      ctx.beginPath(); ctx.arc(pts[i][0], pts[i][1], 1.1, 0, 6.2832); ctx.fill();
    }
    ctx.restore();
  }

  /* ============================ SHIPS ================================ */

  function drawProspector(ctx) {
    var hull = [
      [46, 0], [34, -8], [16, -13], [-8, -15], [-28, -12], [-38, -6], [-41, 0],
      [-38, 6], [-28, 12], [-8, 15], [16, 13], [34, 8]
    ];
    // drop shadow
    ctx.save();
    ctx.globalAlpha = 0.35;
    ctx.fillStyle = '#000';
    ctx.beginPath();
    ctx.ellipse(-2, 4, 42, 18, 0, 0, 6.2832);
    ctx.fill();
    ctx.restore();

    // rear engine block
    poly(ctx, [[-42, -12], [-30, -12], [-26, 12], [-42, 12]], '#1b2530', 'rgba(0,0,0,0.6)');
    // side cargo pods
    [[-6, -20], [-6, 20]].forEach(function (p) {
      var s = p[1] < 0 ? -1 : 1;
      var pts = [[p[0] - 16, p[1] - 7], [p[0] + 14, p[1] - 7], [p[0] + 19, p[1]], [p[0] + 14, p[1] + 7], [p[0] - 16, p[1] + 7]];
      poly(ctx, pts, lin(ctx, p[0] - 16, p[1] - 7, p[0] + 16, p[1] + 7, [
        [0, '#3d4d5c'], [0.5, '#5b7183'], [1, '#28323c']
      ]), 'rgba(0,0,0,0.65)', 1);
      // straps
      ctx.fillStyle = 'rgba(10,16,22,0.75)';
      ctx.fillRect(p[0] - 8, p[1] - 7, 3, 14);
      ctx.fillRect(p[0] + 6, p[1] - 7, 3, 14);
      ctx.fillStyle = 'rgba(95,230,216,0.55)';
      ctx.fillRect(p[0] - 4, p[1] - 5, 6, 1.4);
      bolts(ctx, pts, 'rgba(0,0,0,0.5)', 8);
      if (s) { /* keep symmetric detail */ }
    });

    // main hull
    poly(ctx, hull, lin(ctx, 0, -16, 0, 16, [
      [0, '#7d94a6'], [0.28, '#5a7080'], [0.6, '#3b4b58'], [1, '#222c35']
    ]), 'rgba(6,10,14,0.9)', 1.4);

    // upper deck highlight
    poly(ctx, [[36, 0], [24, -6], [2, -9], [-20, -8], [-30, -4], [-32, 0], [-30, 4], [-20, 8], [2, 9], [24, 6]],
      lin(ctx, 0, -10, 0, 10, [[0, 'rgba(200,225,240,0.30)'], [0.5, 'rgba(140,170,190,0.10)'], [1, 'rgba(0,0,0,0.18)']]));

    // armour plating seams
    panelLines(ctx, [
      [16, -13, 12, -4, 14, 4, 16, 13],
      [-8, -15, -12, -4, -12, 4, -8, 15],
      [-28, -12, -30, 0, -28, 12],
      [34, -8, 30, 0, 34, 8],
      [-2, -15, -2, 15]
    ], 'rgba(8,14,20,0.55)');
    bolts(ctx, hull, 'rgba(210,235,250,0.35)', 11);

    // hazard stripes rear
    stripes(ctx, -34, -5, 10, 10, 'rgba(255,178,87,0.75)', 3, 5);

    // mining emitter prongs
    [[0, -1], [0, 1]].forEach(function (_, i) {
      var s = i === 0 ? -1 : 1;
      var y0 = s * 9;
      var pts = [[20, y0], [40, s * 12], [47, s * 12], [46, s * 14], [38, s * 14], [18, s * 13]];
      poly(ctx, pts, lin(ctx, 20, y0, 46, s * 13, [[0, '#4a5a68'], [1, '#8fa7b8']]), 'rgba(0,0,0,0.6)', 1);
      // emitter tip
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      var g = ctx.createRadialGradient(46, s * 12.5, 0, 46, s * 12.5, 7);
      g.addColorStop(0, 'rgba(160,255,245,0.95)');
      g.addColorStop(0.4, 'rgba(95,230,216,0.55)');
      g.addColorStop(1, 'rgba(95,230,216,0)');
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(46, s * 12.5, 7, 0, 6.2832); ctx.fill();
      ctx.restore();
    });

    // cockpit
    glass(ctx, 22, 0, 11, 7, 0, '#0c2d3a');

    // dorsal sensor dome
    ctx.beginPath();
    ctx.arc(-4, 0, 5.2, 0, 6.2832);
    ctx.fillStyle = lin(ctx, -9, -5, 1, 5, [[0, '#9fb6c6'], [0.5, '#455966'], [1, '#1b232b']]);
    ctx.fill();
    ctx.strokeStyle = 'rgba(0,0,0,0.6)'; ctx.lineWidth = 1; ctx.stroke();
    ctx.fillStyle = 'rgba(95,230,216,0.8)';
    ctx.beginPath(); ctx.arc(-4, -0.5, 1.6, 0, 6.2832); ctx.fill();

    // engine nacelles + nozzles
    [-13, 13].forEach(function (ny) {
      poly(ctx, [[-30, ny - 5], [-20, ny - 6], [-20, ny + 6], [-30, ny + 5]],
        lin(ctx, -30, ny, -20, ny, [[0, '#243039'], [1, '#63798a']]), 'rgba(0,0,0,0.6)');
      nozzle(ctx, -32, ny, 5.4, '#cdefff');
    });

    runningLights(ctx, [[44, 0], [-24, -11], [-24, 11], [10, -12.5], [10, 12.5]], 'rgba(120,240,255,0.8)');
  }

  function drawHarvester(ctx) {
    ctx.save();
    ctx.globalAlpha = 0.35;
    ctx.fillStyle = '#000';
    ctx.beginPath(); ctx.ellipse(-2, 5, 58, 30, 0, 0, 6.2832); ctx.fill();
    ctx.restore();

    // side cargo containers
    [-1, 1].forEach(function (s) {
      var pts = [[-26, s * 20], [26, s * 20], [32, s * 33], [24, s * 38], [-24, s * 38], [-31, s * 32]];
      poly(ctx, pts, lin(ctx, 0, s * 18, 0, s * 40, [
        [0, s > 0 ? '#3e515f' : '#6a7f8f'], [0.5, '#54697a'], [1, s > 0 ? '#1e262e' : '#33404b']
      ]), 'rgba(0,0,0,0.7)', 1.2);
      // container ribs
      for (var i = 0; i < 5; i++) {
        ctx.fillStyle = 'rgba(8,13,18,0.55)';
        ctx.fillRect(-22 + i * 11, s * 21, 2.6, 15);
      }
      ctx.fillStyle = 'rgba(255,178,87,0.65)';
      ctx.fillRect(-18, s * 33, 14, 2);
      ctx.fillRect(4, s * 33, 8, 2);
      bolts(ctx, pts, 'rgba(190,215,230,0.3)', 10);
    });

    // main hull
    var hull = [[52, 0], [42, -10], [22, -16], [-6, -20], [-32, -17], [-46, -9], [-49, 0], [-46, 9], [-32, 17], [-6, 20], [22, 16], [42, 10]];
    poly(ctx, hull, lin(ctx, 0, -20, 0, 20, [
      [0, '#8a9aa5'], [0.25, '#647684'], [0.6, '#3f4f5c'], [1, '#232d36']
    ]), 'rgba(5,9,12,0.9)', 1.4);
    // deck plates
    poly(ctx, [[44, 0], [30, -9], [4, -13], [-24, -11], [-38, -5], [-40, 0], [-38, 5], [-24, 11], [4, 13], [30, 9]],
      'rgba(180,205,220,0.10)');
    panelLines(ctx, [
      [22, -16, 16, -6, 18, 6, 22, 16],
      [-6, -20, -10, -6, -10, 6, -6, 20],
      [-32, -17, -34, 0, -32, 17],
      [42, -10, 36, 0, 42, 10]
    ], 'rgba(7,12,17,0.5)');
    bolts(ctx, hull, 'rgba(210,230,245,0.28)', 12);

    // bridge
    poly(ctx, [[30, -8], [40, -5], [40, 5], [30, 8], [26, 0]], 'rgba(20,32,42,0.95)');
    glass(ctx, 33, 0, 6.5, 7.5, 0, '#0d2a35');

    // excavation arm + drill
    ctx.save();
    ctx.translate(30, 0);
    poly(ctx, [[0, -5], [18, -3], [18, 3], [0, 5]], lin(ctx, 0, -5, 18, 5, [[0, '#7c8f9e'], [1, '#3a4a56']]), 'rgba(0,0,0,0.6)');
    ctx.save();
    ctx.translate(20, 0);
    ctx.rotate(0.0);
    // drill cone
    poly(ctx, [[0, -6], [14, -2], [14, 2], [0, 6]], lin(ctx, 0, -6, 14, 6, [[0, '#c9d8e2'], [0.5, '#7d8f9d'], [1, '#39454f']]), 'rgba(0,0,0,0.7)');
    ctx.strokeStyle = 'rgba(240,250,255,0.45)';
    ctx.lineWidth = 0.9;
    for (var i = 0; i < 4; i++) {
      ctx.beginPath(); ctx.moveTo(2 + i * 3, -5 + i * 0.8); ctx.lineTo(2 + i * 3, 5 - i * 0.8); ctx.stroke();
    }
    ctx.restore();
    ctx.restore();

    // engines
    [-26, 0, 26].forEach(function (ny) {
      poly(ctx, [[-46, ny - 7], [-34, ny - 8], [-34, ny + 8], [-46, ny + 7]],
        lin(ctx, -46, ny, -34, ny, [[0, '#1e2a33'], [1, '#6a8091']]), 'rgba(0,0,0,0.6)');
      nozzle(ctx, -48, ny, 6.4, '#ffe0b8');
    });

    stripes(ctx, -40, -8, 8, 16, 'rgba(255,178,87,0.6)', 2, 6);
    runningLights(ctx, [[50, 0], [-30, -15], [-30, 15], [12, -18], [12, 18], [-20, -30], [-20, 30]], 'rgba(255,200,140,0.9)');
  }

  function drawInterceptor(ctx) {
    ctx.save();
    ctx.globalAlpha = 0.3;
    ctx.fillStyle = '#000';
    ctx.beginPath(); ctx.ellipse(0, 3, 40, 14, 0, 0, 6.2832); ctx.fill();
    ctx.restore();

    // swept wings
    [-1, 1].forEach(function (s) {
      var pts = [[16, s * 6], [-2, s * 30], [-16, s * 30], [-22, s * 12], [-6, s * 5]];
      poly(ctx, pts, lin(ctx, 0, s * 5, 0, s * 30, [
        [0, s > 0 ? '#3d4f60' : '#6f8698'], [0.45, '#4a5f70'], [1, s > 0 ? '#1a242c' : '#2b3742']
      ]), 'rgba(0,0,0,0.7)', 1.2);
      // wing light strip
      ctx.strokeStyle = 'rgba(84,224,255,0.8)';
      ctx.lineWidth = 1.6;
      ctx.beginPath(); ctx.moveTo(6, s * 9); ctx.lineTo(-13, s * 27); ctx.stroke();
      // missile rail
      poly(ctx, [[-8, s * 16], [-18, s * 16], [-18, s * 18], [-8, s * 18]], 'rgba(30,42,52,0.9)', 'rgba(0,0,0,0.6)');
    });

    // fuselage
    var hull = [[56, 0], [40, -6], [12, -11], [-12, -10], [-28, -6], [-34, 0], [-28, 6], [-12, 10], [12, 11], [40, 6]];
    poly(ctx, hull, lin(ctx, 0, -11, 0, 11, [
      [0, '#a4bac9'], [0.3, '#73899a'], [0.6, '#43535f'], [1, '#1c252c']
    ]), 'rgba(4,8,11,0.9)', 1.3);
    poly(ctx, [[46, 0], [30, -5], [6, -7], [-16, -6], [-26, -3], [-28, 0], [-26, 3], [-16, 6], [6, 7], [30, 5]],
      'rgba(190,225,245,0.10)');
    panelLines(ctx, [[36, -6, 32, 0, 36, 6], [14, -11, 10, 0, 14, 11], [-14, -10, -18, 0, -14, 10]],
      'rgba(6,11,16,0.55)');
    glass(ctx, 27, 0, 10, 5.2, 0, '#08222e');
    // cyan fuselage stripe
    stripes(ctx, -22, -1.1, 30, 2.2, 'rgba(84,224,255,0.55)', 1, 0);
    // twin engines
    [-8, 8].forEach(function (ny) {
      poly(ctx, [[-26, ny - 4], [-38, ny - 5], [-38, ny + 5], [-26, ny + 4]],
        lin(ctx, -38, ny, -26, ny, [[0, '#233039'], [1, '#5f7687']]), 'rgba(0,0,0,0.6)');
      nozzle(ctx, -39, ny, 4.8, '#d6f6ff');
    });
    runningLights(ctx, [[54, 0], [-20, -30], [-20, 30], [8, -10], [8, 10]], 'rgba(120,240,255,0.9)');
  }

  function drawLeviathan(ctx) {
    ctx.save();
    ctx.globalAlpha = 0.35;
    ctx.fillStyle = '#000';
    ctx.beginPath(); ctx.ellipse(-2, 6, 64, 40, 0, 0, 6.2832); ctx.fill();
    ctx.restore();

    // hangar pods
    [-1, 1].forEach(function (s) {
      var pts = [[-34, s * 26], [30, s * 24], [44, s * 38], [34, s * 48], [-30, s * 48], [-42, s * 38]];
      poly(ctx, pts, lin(ctx, 0, s * 24, 0, s * 48, [
        [0, s > 0 ? '#3a4d5b' : '#7b93a4'], [0.5, '#52687a'], [1, s > 0 ? '#19222a' : '#2e3b46']
      ]), 'rgba(0,0,0,0.75)', 1.3);
      // hangar door glow
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      var g = ctx.createLinearGradient(-20, s * 34, 18, s * 34);
      g.addColorStop(0, 'rgba(159,247,200,0)');
      g.addColorStop(0.5, 'rgba(159,247,200,0.35)');
      g.addColorStop(1, 'rgba(159,247,200,0)');
      ctx.fillStyle = g;
      ctx.fillRect(-22, s * 33, 44, s > 0 ? 4 : -4);
      ctx.restore();
      for (var i = 0; i < 6; i++) {
        ctx.fillStyle = 'rgba(6,11,16,0.5)';
        ctx.fillRect(-28 + i * 10, s * 28, 2.5, 16);
      }
      bolts(ctx, pts, 'rgba(200,225,240,0.3)', 11);
    });

    // central spine
    var hull = [[64, 0], [52, -12], [24, -18], [-14, -22], [-40, -18], [-52, -8], [-55, 0],
      [-52, 8], [-40, 18], [-14, 22], [24, 18], [52, 12]];
    poly(ctx, hull, lin(ctx, 0, -22, 0, 22, [
      [0, '#93a6b3'], [0.25, '#6d808e'], [0.6, '#465864'], [1, '#232e36']
    ]), 'rgba(4,8,11,0.9)', 1.5);
    poly(ctx, [[54, 0], [38, -10], [8, -14], [-22, -12], [-38, -5], [-40, 0], [-38, 5], [-22, 12], [8, 14], [38, 10]],
      'rgba(185,215,235,0.12)');

    // forward mining array (two towers)
    [-1, 1].forEach(function (s) {
      poly(ctx, [[30, s * 9], [54, s * 9], [58, s * 13], [30, s * 14]],
        lin(ctx, 30, s * 9, 58, s * 14, [[0, '#55697a'], [1, '#9cb4c4']]), 'rgba(0,0,0,0.6)');
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      var g = ctx.createRadialGradient(58, s * 13, 0, 58, s * 13, 9);
      g.addColorStop(0, 'rgba(200,255,235,0.95)');
      g.addColorStop(0.4, 'rgba(159,247,200,0.5)');
      g.addColorStop(1, 'rgba(159,247,200,0)');
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(58, s * 13, 9, 0, 6.2832); ctx.fill();
      ctx.restore();
    });

    // bridge
    poly(ctx, [[26, -11], [40, -8], [44, 0], [40, 8], [26, 11], [20, 0]], 'rgba(18,30,38,0.95)', 'rgba(0,0,0,0.6)');
    glass(ctx, 32, 0, 9, 6, 0, '#0a2831');

    // turrets
    [[-8, -16], [-8, 16]].forEach(function (t) {
      ctx.save();
      ctx.translate(t[0], t[1]);
      ctx.beginPath(); ctx.arc(0, 0, 5, 0, 6.2832);
      ctx.fillStyle = lin(ctx, -5, -5, 5, 5, [[0, '#9fb3c1'], [1, '#38454f']]);
      ctx.fill();
      ctx.strokeStyle = 'rgba(0,0,0,0.7)'; ctx.stroke();
      ctx.fillStyle = '#202a33';
      ctx.fillRect(-1.4, t[1] > 0 ? -2 : -9, 2.8, 11);
      ctx.restore();
    });

    // 4 engine pods
    [-32, -12, 12, 32].forEach(function (ny) {
      poly(ctx, [[-52, ny - 7], [-38, ny - 8], [-38, ny + 8], [-52, ny + 7]],
        lin(ctx, -52, ny, -38, ny, [[0, '#18222a'], [1, '#6c8494']]), 'rgba(0,0,0,0.6)');
      nozzle(ctx, -54, ny, 7, '#d8fff0');
    });

    panelLines(ctx, [
      [26, -18, 20, -6, 20, 6, 26, 18],
      [-14, -22, -18, -6, -18, 6, -14, 22],
      [-40, -18, -42, 0, -40, 18],
      [52, -12, 46, 0, 52, 12],
      [8, -18, 8, 18]
    ], 'rgba(6,11,16,0.5)');
    bolts(ctx, hull, 'rgba(210,235,250,0.3)', 13);
    stripes(ctx, -34, -6, 12, 12, 'rgba(159,247,200,0.5)', 3, 5);
    runningLights(ctx, [[62, 0], [-30, -22], [-30, 22], [0, -22], [0, 22], [18, -34], [18, 34]],
      'rgba(170,255,220,0.9)');
  }

  var SHIP_DRAW = {
    prospector: { draw: drawProspector, w: 210, h: 130, ss: 2.4 },
    harvester: { draw: drawHarvester, w: 250, h: 180, ss: 2.2 },
    interceptor: { draw: drawInterceptor, w: 190, h: 130, ss: 2.6 },
    leviathan: { draw: drawLeviathan, w: 270, h: 220, ss: 2.1 }
  };

  var shipCache = {};
  function ship(id) {
    if (shipCache[id]) return shipCache[id];
    var d = SHIP_DRAW[id] || SHIP_DRAW.prospector;
    var s = sprite(d.w, d.h, d.ss, d.draw);
    s.id = id;
    shipCache[id] = s;
    return s;
  }

  /* ============================ ENEMIES ============================== */

  function drawScout(ctx) {
    poly(ctx, [[0, -13], [14, -6], [18, 0], [14, 6], [0, 13], [-12, 7], [-16, 0], [-12, -7]],
      lin(ctx, 0, -14, 0, 14, [[0, '#5b7d91'], [0.5, '#33495a'], [1, '#141d26']]), 'rgba(0,0,0,0.8)', 1.2);
    poly(ctx, [[-2, -8], [10, -3], [10, 3], [-2, 8], [-6, 0]], 'rgba(140,190,215,0.18)');
    // eye
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var g = ctx.createRadialGradient(6, 0, 0, 6, 0, 9);
    g.addColorStop(0, '#ffffff');
    g.addColorStop(0.3, '#7fd8ff');
    g.addColorStop(1, 'rgba(127,216,255,0)');
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(6, 0, 9, 0, 6.2832); ctx.fill();
    ctx.restore();
    // ring
    ctx.strokeStyle = 'rgba(127,216,255,0.75)';
    ctx.lineWidth = 1.6;
    ctx.beginPath(); ctx.ellipse(-2, 0, 16, 9, 0, 0, 6.2832); ctx.stroke();
    ctx.strokeStyle = 'rgba(150,220,255,0.35)';
    ctx.beginPath(); ctx.ellipse(-2, 0, 20, 4, 0, 0, 6.2832); ctx.stroke();
    [-1, 1].forEach(function (s) {
      ctx.fillStyle = 'rgba(20,30,38,0.95)';
      ctx.fillRect(-14, s * 8 - 1.2, 10, 2.4);
      ctx.fillStyle = 'rgba(127,216,255,0.9)';
      ctx.beginPath(); ctx.arc(-15, s * 8, 1.5, 0, 6.2832); ctx.fill();
    });
  }

  function drawRaider(ctx) {
    // asymmetric scavenger skiff
    poly(ctx, [[22, 0], [12, -9], [-4, -14], [-16, -10], [-22, -3], [-22, 3], [-16, 10], [-4, 14], [12, 9]],
      lin(ctx, 0, -15, 0, 15, [[0, '#8a6a52'], [0.4, '#5c4436'], [1, '#241a14']]), 'rgba(0,0,0,0.85)', 1.3);
    // welded plates
    poly(ctx, [[6, -9], [14, -4], [12, 3], [2, 6], [-6, 2], [-4, -6]], 'rgba(200,160,120,0.10)');
    poly(ctx, [[-16, -9], [-6, -11], [-2, -4], [-10, 2], [-20, 0]], 'rgba(0,0,0,0.25)');
    // jagged blades at back
    [-1, 1].forEach(function (s) {
      poly(ctx, [[-18, s * 8], [-32, s * 17], [-24, s * 20], [-12, s * 11]], 'rgba(70,52,40,0.95)', 'rgba(0,0,0,0.7)');
    });
    // cannon barrel
    ctx.fillStyle = '#2a2018';
    ctx.fillRect(16, -3.4, 16, 6.8);
    ctx.fillStyle = '#6d5442';
    ctx.fillRect(28, -4.6, 5, 9.2);
    // hot core
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var g = ctx.createRadialGradient(-4, 0, 0, -4, 0, 11);
    g.addColorStop(0, '#fff0d0');
    g.addColorStop(0.35, '#ff8a4d');
    g.addColorStop(1, 'rgba(255,138,77,0)');
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(-4, 0, 11, 0, 6.2832); ctx.fill();
    ctx.restore();
    // cockpit slit
    ctx.fillStyle = 'rgba(255,180,120,0.75)';
    ctx.fillRect(2, -1.6, 9, 3.2);
    ctx.strokeStyle = 'rgba(255,180,120,0.35)';
    ctx.lineWidth = 1;
    ctx.strokeRect(2, -1.6, 9, 3.2);
    // thrusters
    ctx.fillStyle = '#1a1410';
    ctx.fillRect(-24, -8, 5, 4); ctx.fillRect(-24, 4, 5, 4);
  }

  function drawHeavy(ctx) {
    // heavy hexagonal armour
    var pts = [[0, -30], [24, -18], [30, 0], [24, 18], [0, 30], [-24, 18], [-30, 0], [-24, -18]];
    poly(ctx, pts, lin(ctx, 0, -32, 0, 32, [[0, '#8a7a58'], [0.4, '#574c38'], [1, '#221d15']]), 'rgba(0,0,0,0.9)', 1.6);
    poly(ctx, [[0, -22], [17, -13], [22, 0], [17, 13], [0, 22], [-17, 13], [-22, 0], [-17, -13]], 'rgba(255,220,150,0.08)');
    // armour plates
    [[0, -22, 12], [0, 22, -12]].forEach(function (p) {
      poly(ctx, [[-14, p[1]], [14, p[1]], [10, p[1] + p[2] * 0.6], [-10, p[1] + p[2] * 0.6]],
        'rgba(30,24,16,0.65)', 'rgba(255,200,120,0.25)', 1);
    });
    // cannon
    ctx.fillStyle = '#241d14';
    ctx.fillRect(20, -6, 20, 12);
    ctx.fillStyle = '#6a5636';
    ctx.fillRect(36, -7.5, 6, 15);
    ctx.fillStyle = 'rgba(255,200,87,0.9)';
    ctx.fillRect(41, -3, 2, 6);
    // shield emitter nodes
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    [[-14, -18], [-14, 18]].forEach(function (n) {
      var g = ctx.createRadialGradient(n[0], n[1], 0, n[0], n[1], 8);
      g.addColorStop(0, 'rgba(255,220,120,0.95)');
      g.addColorStop(1, 'rgba(255,200,87,0)');
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(n[0], n[1], 8, 0, 6.2832); ctx.fill();
    });
    ctx.restore();
    // vent grill
    ctx.strokeStyle = 'rgba(0,0,0,0.6)';
    ctx.lineWidth = 1.4;
    for (var i = 0; i < 4; i++) {
      ctx.beginPath(); ctx.moveTo(-26, -12 + i * 4); ctx.lineTo(-14, -12 + i * 4); ctx.stroke();
    }
    // hazard stripes
    ctx.save();
    ctx.translate(-4, -30);
    stripes(ctx, 0, 0, 20, 5, 'rgba(255,200,87,0.55)', 3, 6);
    ctx.restore();
  }

  function drawHunter(ctx) {
    // jagged claw silhouette
    poly(ctx, [[26, 0], [12, -10], [2, -6], [-8, -20], [-16, -10], [-30, -14], [-22, -2],
      [-34, 4], [-20, 10], [-24, 20], [-6, 12], [4, 18], [10, 6]],
      lin(ctx, 0, -22, 0, 20, [[0, '#7c3a72'], [0.45, '#4a1d46'], [1, '#180617']]), 'rgba(0,0,0,0.85)', 1.3);
    // bright energy seams
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.strokeStyle = 'rgba(255,92,200,0.85)';
    ctx.lineWidth = 1.8;
    ctx.beginPath();
    ctx.moveTo(-24, -6); ctx.lineTo(-6, -2); ctx.lineTo(10, -4);
    ctx.stroke();
    ctx.strokeStyle = 'rgba(255,180,235,0.5)';
    ctx.beginPath(); ctx.moveTo(-18, 8); ctx.lineTo(2, 4); ctx.lineTo(16, 6); ctx.stroke();
    var g = ctx.createRadialGradient(8, 0, 0, 8, 0, 12);
    g.addColorStop(0, '#ffffff');
    g.addColorStop(0.3, '#ff5cc8');
    g.addColorStop(1, 'rgba(255,92,200,0)');
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(8, 0, 12, 0, 6.2832); ctx.fill();
    ctx.restore();
    // blades
    ctx.strokeStyle = 'rgba(200,130,220,0.6)';
    ctx.lineWidth = 1.4;
    ctx.beginPath(); ctx.moveTo(-22, -12); ctx.lineTo(-10, -22); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(-22, 12); ctx.lineTo(-8, 20); ctx.stroke();
  }

  function drawSentinel(ctx) {
    // outer rotating ring plate
    ctx.save();
    ctx.strokeStyle = 'rgba(176,125,255,0.55)';
    ctx.lineWidth = 3;
    ctx.beginPath(); ctx.arc(0, 0, 34, 0, 6.2832); ctx.stroke();
    ctx.strokeStyle = 'rgba(224,200,255,0.25)';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.arc(0, 0, 27, 0, 6.2832); ctx.stroke();
    // rune ticks
    for (var i = 0; i < 12; i++) {
      var a = i / 12 * 6.2832;
      ctx.strokeStyle = 'rgba(210,180,255,' + (i % 3 === 0 ? 0.9 : 0.3) + ')';
      ctx.lineWidth = i % 3 === 0 ? 2.4 : 1.2;
      ctx.beginPath();
      ctx.moveTo(Math.cos(a) * 33, Math.sin(a) * 33);
      ctx.lineTo(Math.cos(a) * (i % 3 === 0 ? 39 : 36), Math.sin(a) * (i % 3 === 0 ? 39 : 36));
      ctx.stroke();
    }
    ctx.restore();
    // core body
    poly(ctx, [[0, -20], [14, -12], [20, 0], [14, 12], [0, 20], [-14, 12], [-20, 0], [-14, -12]],
      lin(ctx, 0, -22, 0, 22, [[0, '#6b4fa0'], [0.5, '#33204f'], [1, '#0f0819']]), 'rgba(0,0,0,0.85)', 1.4);
    poly(ctx, [[0, -13], [9, -8], [12, 0], [9, 8], [0, 13], [-9, 8], [-12, 0], [-9, -8]],
      'rgba(200,170,255,0.10)');
    // eye
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var g = ctx.createRadialGradient(0, 0, 0, 0, 0, 16);
    g.addColorStop(0, '#ffffff');
    g.addColorStop(0.25, '#d9baff');
    g.addColorStop(0.6, 'rgba(176,125,255,0.55)');
    g.addColorStop(1, 'rgba(176,125,255,0)');
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(0, 0, 16, 0, 6.2832); ctx.fill();
    ctx.restore();
    ctx.strokeStyle = 'rgba(230,210,255,0.8)';
    ctx.lineWidth = 1.2;
    ctx.beginPath(); ctx.arc(0, 0, 7, 0, 6.2832); ctx.stroke();
  }

  var ENEMY_DRAW = {
    scout: { draw: drawScout, w: 74, h: 74, ss: 2.6 },
    raider: { draw: drawRaider, w: 96, h: 74, ss: 2.6 },
    heavy: { draw: drawHeavy, w: 120, h: 110, ss: 2.4 },
    plasmaHunter: { draw: drawHunter, w: 100, h: 90, ss: 2.6 },
    voidSentinel: { draw: drawSentinel, w: 130, h: 130, ss: 2.4 }
  };

  var enemyCache = {};
  function enemy(id) {
    if (enemyCache[id]) return enemyCache[id];
    var d = ENEMY_DRAW[id] || ENEMY_DRAW.scout;
    var s = sprite(d.w, d.h, d.ss, d.draw);
    s.id = id;
    enemyCache[id] = s;
    return s;
  }

  /* ============================ STATION ============================== */

  function drawStation(ctx) {
    // outer ring
    ctx.save();
    ctx.strokeStyle = 'rgba(120,170,200,0.20)';
    ctx.lineWidth = 22;
    ctx.beginPath(); ctx.arc(0, 0, 150, 0, 6.2832); ctx.stroke();
    ctx.strokeStyle = 'rgba(180,220,245,0.45)';
    ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(0, 0, 162, 0, 6.2832); ctx.stroke();
    ctx.beginPath(); ctx.arc(0, 0, 138, 0, 6.2832); ctx.stroke();
    // ring segments
    for (var i = 0; i < 24; i++) {
      var a = i / 24 * 6.2832;
      ctx.strokeStyle = i % 2 ? 'rgba(90,140,170,0.25)' : 'rgba(150,200,230,0.30)';
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      ctx.moveTo(Math.cos(a) * 138, Math.sin(a) * 138);
      ctx.lineTo(Math.cos(a) * 162, Math.sin(a) * 162);
      ctx.stroke();
    }
    ctx.restore();

    // spokes
    for (var s = 0; s < 4; s++) {
      ctx.save();
      ctx.rotate(s * Math.PI / 2 + Math.PI / 4);
      ctx.fillStyle = lin(ctx, 0, -8, 150, 8, [[0, '#2c3b47'], [0.5, '#7a91a2'], [1, '#33434f']]);
      ctx.fillRect(40, -8, 100, 16);
      ctx.strokeStyle = 'rgba(0,0,0,0.6)';
      ctx.strokeRect(40, -8, 100, 16);
      ctx.fillStyle = 'rgba(255,178,87,0.7)';
      for (var l = 0; l < 6; l++) ctx.fillRect(50 + l * 15, -8, 3, 3);
      ctx.restore();
    }

    // core hull
    var hull = [];
    for (var h = 0; h < 12; h++) {
      var ang = h / 12 * 6.2832;
      var r = h % 2 ? 46 : 54;
      hull.push([Math.cos(ang) * r, Math.sin(ang) * r]);
    }
    poly(ctx, hull, lin(ctx, 0, -56, 0, 56, [[0, '#8ea3b3'], [0.4, '#576a79'], [1, '#232e37']]),
      'rgba(0,0,0,0.8)', 2);
    // docking arms
    [-1, 1].forEach(function (sg) {
      poly(ctx, [[-10, sg * 46], [10, sg * 46], [14, sg * 78], [0, sg * 92], [-14, sg * 78]],
        lin(ctx, 0, sg * 46, 0, sg * 92, [[0, '#4a5c6a'], [1, '#8fa6b6']]), 'rgba(0,0,0,0.75)', 1.4);
      ctx.fillStyle = 'rgba(255,178,87,0.8)';
      for (var d = 0; d < 4; d++) ctx.fillRect(-10, sg * (58 + d * 8), 20, 2);
    });
    // central dome
    var dg = ctx.createRadialGradient(-12, -14, 4, 0, 0, 40);
    dg.addColorStop(0, '#dff2ff');
    dg.addColorStop(0.4, '#7c98ab');
    dg.addColorStop(1, '#26333d');
    ctx.fillStyle = dg;
    ctx.beginPath(); ctx.arc(0, 0, 36, 0, 6.2832); ctx.fill();
    ctx.strokeStyle = 'rgba(0,0,0,0.7)'; ctx.lineWidth = 1.6; ctx.stroke();
    // windows
    for (var w = 0; w < 14; w++) {
      var wa = w / 14 * 6.2832;
      ctx.fillStyle = 'rgba(255,215,150,0.85)';
      ctx.beginPath();
      ctx.arc(Math.cos(wa) * 24, Math.sin(wa) * 24, 2.2, 0, 6.2832);
      ctx.fill();
    }
    // beacon
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var bg = ctx.createRadialGradient(0, 0, 0, 0, 0, 20);
    bg.addColorStop(0, 'rgba(255,235,200,0.9)');
    bg.addColorStop(0.4, 'rgba(255,178,87,0.35)');
    bg.addColorStop(1, 'rgba(255,178,87,0)');
    ctx.fillStyle = bg;
    ctx.beginPath(); ctx.arc(0, 0, 20, 0, 6.2832); ctx.fill();
    ctx.restore();

    // solar panels
    [[-1, -1], [1, 1], [1, -1], [-1, 1]].forEach(function (q, i) {
      if (i > 2) return;
      ctx.save();
      ctx.translate(q[0] * 96, q[1] * 96);
      ctx.rotate(Math.atan2(q[1], q[0]));
      ctx.fillStyle = 'rgba(30,60,90,0.9)';
      ctx.fillRect(-26, -14, 52, 28);
      ctx.strokeStyle = 'rgba(120,180,220,0.5)';
      ctx.lineWidth = 1;
      for (var g2 = 0; g2 < 4; g2++) {
        ctx.beginPath(); ctx.moveTo(-26, -14 + g2 * 7); ctx.lineTo(26, -14 + g2 * 7); ctx.stroke();
      }
      ctx.strokeRect(-26, -14, 52, 28);
      ctx.restore();
    });
  }

  var stationCache = null;
  function station() {
    if (!stationCache) {
      stationCache = sprite(400, 400, 1.8, drawStation);
    }
    return stationCache;
  }

  /* ============================ DRONE ================================ */

  function drawDrone(ctx) {
    // small hex utility drone with mining arm
    poly(ctx, [[0, -11], [9, -6], [9, 6], [0, 11], [-9, 6], [-9, -6]],
      lin(ctx, 0, -12, 0, 12, [[0, '#9fb6c4'], [0.5, '#5d7382'], [1, '#2a3742']]), 'rgba(0,0,0,0.8)', 1.2);
    poly(ctx, [[0, -6], [5, -3], [5, 3], [0, 6], [-5, 3], [-5, -3]], 'rgba(95,230,216,0.25)');
    ctx.fillStyle = '#1b2530';
    ctx.fillRect(6, -2, 9, 4);
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var g = ctx.createRadialGradient(15, 0, 0, 15, 0, 7);
    g.addColorStop(0, 'rgba(180,255,245,0.95)');
    g.addColorStop(1, 'rgba(95,230,216,0)');
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(15, 0, 7, 0, 6.2832); ctx.fill();
    ctx.restore();
    ctx.fillStyle = 'rgba(95,230,216,0.9)';
    [-11, 11].forEach(function (yy) {
      ctx.beginPath(); ctx.arc(-4, yy * 0.5, 1.5, 0, 6.2832); ctx.fill();
    });
  }

  var droneCache = null;
  function drone() {
    if (!droneCache) droneCache = sprite(56, 44, 3, drawDrone);
    return droneCache;
  }

  SM.Art = SM.Art || {};
  SM.Art.ship = ship;
  SM.Art.enemy = enemy;
  SM.Art.station = station;
  SM.Art.drone = drone;
  SM.Art.sprite = sprite;
})(typeof window !== 'undefined' ? window : globalThis);
