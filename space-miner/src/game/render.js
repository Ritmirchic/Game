/* =========================================================================
   SPACE MINER — world renderer (canvas 2D, layered, additive glow)
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, TAU = U.TAU, hexA = U.hexA, Path = SM.Path;

  var SHIP_RENDER = {
    prospector: { k: 0.50, engines: [[-31, -13], [-31, 13]], nose: 46, width: 21 },
    harvester: { k: 0.50, engines: [[-47, -26], [-47, 0], [-47, 26]], nose: 52, width: 30 },
    interceptor: { k: 0.55, engines: [[-38, -8], [-38, 8]], nose: 56, width: 15 },
    leviathan: { k: 0.52, engines: [[-53, -32], [-53, -12], [-53, 12], [-53, 32]], nose: 64, width: 38 }
  };

  function Renderer(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.w = 1280; this.h = 720;
    this.dpr = 1;
    this.overlay = null;
    this.fog = null;
  }

  Renderer.prototype.resize = function (w, h, dpr) {
    this.w = w; this.h = h;
    this.dpr = dpr || 1;
    this.canvas.width = Math.round(w * this.dpr);
    this.canvas.height = Math.round(h * this.dpr);
    this.canvas.style.width = w + 'px';
    this.canvas.style.height = h + 'px';
    this.buildOverlay();
  };

  Renderer.prototype.buildOverlay = function () {
    var c = U.makeCanvas(this.w, this.h);
    var ctx = c.getContext('2d');
    // scanlines
    ctx.fillStyle = 'rgba(0,0,0,0.16)';
    for (var y = 0; y < this.h; y += 3) ctx.fillRect(0, y, this.w, 1);
    // vignette
    var g = ctx.createRadialGradient(this.w / 2, this.h / 2, Math.min(this.w, this.h) * 0.32,
      this.w / 2, this.h / 2, Math.max(this.w, this.h) * 0.78);
    g.addColorStop(0, 'rgba(0,0,0,0)');
    g.addColorStop(0.72, 'rgba(0,0,0,0.16)');
    g.addColorStop(1, 'rgba(0,0,0,0.62)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, this.w, this.h);
    this.overlay = c;
  };

  /* ------------------------------------------------------------------ */
  Renderer.prototype.worldToScreen = function (x, y, cam) {
    return { x: (x - cam.x) * cam.zoom + this.w / 2, y: (y - cam.y) * cam.zoom + this.h / 2 };
  };
  Renderer.prototype.screenToWorld = function (x, y, cam) {
    return { x: (x - this.w / 2) / cam.zoom + cam.x, y: (y - this.h / 2) / cam.zoom + cam.y };
  };

  /* ------------------------------------------------------------------ */
  Renderer.prototype.render = function (game, dt) {
    var ctx = this.ctx;
    var w = this.w, h = this.h;
    ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#04060b';
    ctx.fillRect(0, 0, w, h);

    var world = game.world;
    if (!world) {
      this.renderIdleBackdrop(game, dt);
      return;
    }
    var cam = game.camera;

    this.drawBackdrop(ctx, world, cam, game);
    this.drawHazardBack(ctx, world, game);

    ctx.save();
    ctx.translate(w / 2 + cam.shakeX, h / 2 + cam.shakeY);
    ctx.scale(cam.zoom, cam.zoom);
    ctx.translate(-cam.x, -cam.y);

    this.drawStation(ctx, world, game);
    this.drawPickups(ctx, world, game);
    this.drawAsteroids(ctx, world, game);
    this.drawDebris(ctx, world, game);
    this.drawDrones(ctx, world, game);
    this.drawEnemies(ctx, world, game);
    this.drawPlayer(ctx, world, game);
    this.drawProjectiles(ctx, world, game);
    this.drawParticles(ctx, world, game);
    this.drawHazardFront(ctx, world, game);

    ctx.restore();

    this.drawScreenFX(ctx, world, game, dt);
  };

  Renderer.prototype.renderIdleBackdrop = function (game, dt) {
    var ctx = this.ctx, w = this.w, h = this.h;
    if (!this._idle) {
      this._idle = { stars: SM.Art.starLayer(9981, 260, 2000, { star: '#cfe8ff', accent: '#5fe6d8' }), t: 0 };
    }
    this._idle.t += dt;
    var g = ctx.createLinearGradient(0, 0, w * 0.3, h);
    g.addColorStop(0, '#050a14');
    g.addColorStop(0.5, '#0a1424');
    g.addColorStop(1, '#04060b');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (var i = 0; i < this._idle.stars.length; i++) {
      var s = this._idle.stars[i];
      var tw = 0.55 + 0.45 * Math.sin(this._idle.t * s.tw + s.ph);
      ctx.fillStyle = hexA(s.c, s.a * tw);
      ctx.beginPath();
      ctx.arc((s.x + this._idle.t * 6) % 2000 * (w / 2000), (s.y % h), s.r, 0, TAU);
      ctx.fill();
    }
    ctx.restore();
  };

  /* ---------------- backdrop ---------------- */
  Renderer.prototype.drawBackdrop = function (ctx, world, cam, game) {
    var w = this.w, h = this.h;
    var bg = world.bg;
    if (bg) {
      var zoom = Math.max(w / bg.width, h / bg.height) * 1.22;
      var bw = bg.width * zoom, bh = bg.height * zoom;
      var px = U.clamp((cam.x / SM.Sectors.WORLD_W), 0, 1);
      var py = U.clamp((cam.y / SM.Sectors.WORLD_H), 0, 1);
      var ox = -(bw - w) * px + cam.parallaxX * 16;
      var oy = -(bh - h) * py + cam.parallaxY * 16;
      ctx.drawImage(bg, ox, oy, bw, bh);
    }

    // far stars (twinkle, slow parallax)
    var t = world.time;
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var layers = [[world.stars1, 0.10, 2400], [world.stars2, 0.22, 2400]];
    for (var L = 0; L < layers.length; L++) {
      var arr = layers[L][0], par = layers[L][1], span = layers[L][2];
      for (var i = 0; i < arr.length; i++) {
        var s = arr[i];
        var sx = ((s.x - cam.x * par) % span + span) % span;
        var sy = ((s.y - cam.y * par) % span + span) % span;
        if (sx > w + 4 || sy > h + 4) continue;
        var tw = 0.45 + 0.55 * (0.5 + 0.5 * Math.sin(t * s.tw + s.ph));
        ctx.fillStyle = hexA(s.c, s.a * tw * (L ? 0.9 : 0.65));
        ctx.beginPath();
        ctx.arc(sx, sy, s.r * (L ? 1.1 : 0.85), 0, TAU);
        ctx.fill();
      }
    }
    // dust motes drifting close to the camera
    var dust = world.dust;
    for (var d = 0; d < dust.length; d++) {
      var m = dust[d];
      var dx = ((m.x + m.vx * t - cam.x * 0.62) % 2400 + 2400) % 2400;
      var dy = ((m.y + m.vy * t - cam.y * 0.62) % 2400 + 2400) % 2400;
      if (dx > w || dy > h) continue;
      ctx.fillStyle = hexA(m.c, m.a * 0.7);
      ctx.beginPath();
      ctx.arc(dx, dy, m.r, 0, TAU);
      ctx.fill();
    }
    ctx.restore();
  };

  /* ---------------- hazards behind entities ---------------- */
  Renderer.prototype.drawHazardBack = function (ctx, world, game) {
    if (world.sector.hazard.kind !== 'gravity') return;
    var cam = game.camera;
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (var i = 0; i < world.wells.length; i++) {
      var wl = world.wells[i];
      var p = this.worldToScreen(wl.x, wl.y, cam);
      if (p.x < -800 || p.y < -800 || p.x > this.w + 800 || p.y > this.h + 800) continue;
      var r = wl.r * cam.zoom;
      var g = ctx.createRadialGradient(p.x, p.y, r * 0.12, p.x, p.y, r);
      g.addColorStop(0, 'rgba(10,4,26,0.55)');
      g.addColorStop(0.45, 'rgba(70,30,130,0.14)');
      g.addColorStop(0.85, 'rgba(140,80,255,0.06)');
      g.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(p.x, p.y, r, 0, TAU); ctx.fill();
      ctx.strokeStyle = 'rgba(176,125,255,0.14)';
      ctx.lineWidth = 1.5;
      for (var k = 1; k <= 3; k++) {
        ctx.beginPath();
        ctx.arc(p.x, p.y, r * (0.28 + k * 0.2) + Math.sin(world.time * 0.8 + k) * 6, 0, TAU);
        ctx.stroke();
      }
    }
    ctx.restore();
  };

  Renderer.prototype.drawHazardFront = function (ctx, world, game) {
    var hz = world.sector.hazard;
    if (hz.kind === 'storm') {
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      for (var i = 0; i < world.storms.length; i++) {
        var s = world.storms[i];
        if (s.done) {
          var a = 1 - s.strikeT / 0.55;
          if (a <= 0) continue;
          ctx.strokeStyle = 'rgba(200,180,255,' + (a * 0.9) + ')';
          ctx.lineWidth = 3 * a + 1;
          ctx.beginPath();
          var rng = new U.RNG(s.seed | 0);
          var x = s.x, y = s.y - 320;
          ctx.moveTo(x, y);
          for (var seg = 0; seg < 8; seg++) {
            x += rng.range(-24, 24);
            y += 40;
            ctx.lineTo(x, y);
          }
          ctx.stroke();
        } else {
          var t = s.t / s.tele;
          ctx.strokeStyle = 'rgba(190,160,255,' + (0.14 + t * 0.4) + ')';
          ctx.lineWidth = 1.4;
          ctx.beginPath();
          ctx.arc(s.x, s.y, U.lerp(120, 26, t), 0, TAU);
          ctx.stroke();
          ctx.strokeStyle = 'rgba(230,215,255,' + (0.1 + t * 0.24) + ')';
          ctx.beginPath();
          ctx.arc(s.x, s.y, U.lerp(140, 30, t) + Math.sin(world.time * 18) * 4, 0, TAU);
          ctx.stroke();
        }
      }
      ctx.restore();
    }
  };

  /* ---------------- station ---------------- */
  Renderer.prototype.drawStation = function (ctx, world, game) {
    var st = world.station;
    var art = SM.Art.station();
    var size = 400 * 0.66;
    var cam = game.camera;
    if (Math.abs(st.x - cam.x) > this.w / cam.zoom + 500 || Math.abs(st.y - cam.y) > this.h / cam.zoom + 500) return;

    // beacon glow
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var pulse = 0.5 + 0.5 * Math.sin(world.time * 1.6);
    var g = ctx.createRadialGradient(st.x, st.y, 30, st.x, st.y, 260);
    g.addColorStop(0, 'rgba(255,190,120,' + (0.16 + pulse * 0.06) + ')');
    g.addColorStop(0.4, 'rgba(255,150,80,0.05)');
    g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc(st.x, st.y, 260, 0, TAU); ctx.fill();
    ctx.restore();

    ctx.save();
    ctx.translate(st.x, st.y);
    ctx.rotate(st.phase * 0.35);
    ctx.globalAlpha = 0.98;
    ctx.drawImage(art.canvas, -size / 2, -size / 2, size, size);
    ctx.restore();

    // docking halo
    var near = world.distanceToStation() < world.station.r + 220;
    if (near) {
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      ctx.strokeStyle = 'rgba(95,230,216,' + (0.25 + 0.2 * Math.sin(world.time * 4)) + ')';
      ctx.lineWidth = 2;
      ctx.setLineDash([10, 12]);
      ctx.beginPath();
      ctx.arc(st.x, st.y, st.r + 62, world.time * 0.6, world.time * 0.6 + TAU);
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.restore();
    }
  };

  /* ---------------- asteroids ---------------- */
  Renderer.prototype.drawAsteroids = function (ctx, world, game) {
    var cam = game.camera;
    var vw = this.w / cam.zoom, vh = this.h / cam.zoom;
    for (var i = 0; i < world.asteroids.length; i++) {
      var a = world.asteroids[i];
      if (a.x < cam.x - vw / 2 - 200 || a.x > cam.x + vw / 2 + 200) continue;
      if (a.y < cam.y - vh / 2 - 200 || a.y > cam.y + vh / 2 + 200) continue;
      var art = a.art;
      var s = a.radius / art.baseRadius;
      var dw = art.sprite.width * s, dh = art.sprite.height * s;
      var res = SM.Resources.get(a.resourceId);

      // subtle mineral glow behind the rock
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      var glowA = 0.08 + 0.05 * Math.sin(a.glowPhase * 1.4) + a.damagePct * 0.16;
      var g = ctx.createRadialGradient(a.x, a.y, a.radius * 0.2, a.x, a.y, a.radius * 1.5);
      g.addColorStop(0, hexA(res.glow, glowA));
      g.addColorStop(1, hexA(res.glow, 0));
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(a.x, a.y, a.radius * 1.5, 0, TAU); ctx.fill();
      ctx.restore();

      ctx.save();
      ctx.translate(a.x, a.y);
      ctx.rotate(a.rot);
      ctx.drawImage(art.sprite, -dw / 2, -dh / 2, dw, dh);

      // crack overlay as it takes damage
      if (a.damagePct > 0.02) {
        ctx.globalAlpha = Math.min(0.95, a.damagePct * 1.15);
        ctx.drawImage(art.cracks, -dw / 2, -dh / 2, dw, dh);
        ctx.globalAlpha = 1;
      }
      // hit flash
      if (a.hitFlash > 0.01) {
        ctx.globalCompositeOperation = 'lighter';
        ctx.globalAlpha = a.hitFlash * 0.5;
        ctx.drawImage(art.sprite, -dw / 2, -dh / 2, dw, dh);
        ctx.globalAlpha = 1;
      }
      ctx.restore();

      // richness sparkle
      if (a.rich || a.richness > 12) {
        ctx.save();
        ctx.globalCompositeOperation = 'lighter';
        var tw = 0.5 + 0.5 * Math.sin(world.time * 3 + a.id);
        ctx.fillStyle = hexA(res.glow, 0.35 * tw);
        ctx.beginPath();
        ctx.arc(a.x + Math.cos(a.id) * a.radius * 0.4, a.y + Math.sin(a.id * 1.7) * a.radius * 0.4, 2.2, 0, TAU);
        ctx.fill();
        ctx.restore();
      }

      // health ring when actively mined or damaged
      if (a.damagePct > 0.01 && (a === world.player.miningTarget || a.damagePct > 0.05)) {
        var pct = 1 - a.damagePct;
        ctx.save();
        ctx.strokeStyle = 'rgba(0,0,0,0.4)';
        ctx.lineWidth = 3.5;
        ctx.beginPath();
        ctx.arc(a.x, a.y, a.radius + 12, 0, TAU);
        ctx.stroke();
        ctx.strokeStyle = pct > 0.55 ? hexA(res.glow, 0.75) : pct > 0.25 ? '#ffd166' : '#ff6b6b';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(a.x, a.y, a.radius + 12, -Math.PI / 2, -Math.PI / 2 + TAU * pct);
        ctx.stroke();
        ctx.restore();
      }

      // resource tag when the ship is close / scanning
      var dToShip = U.dist(a.x, a.y, world.player.x, world.player.y);
      if (dToShip < 260 && !world.player.dead) {
        ctx.save();
        ctx.globalAlpha = U.clamp(1 - (dToShip - 120) / 140, 0, 0.9);
        ctx.font = SM.Font.mono(9);
        ctx.fillStyle = hexA(res.color, 0.95);
        ctx.textAlign = 'center';
        ctx.fillText(res.code + ' · ' + a.richness, a.x, a.y + a.radius + 26);
        ctx.restore();
      }
    }
  };

  Renderer.prototype.drawDebris = function (ctx, world, game) {
    var d = world.debris;
    for (var i = 0; i < d.length; i++) {
      var o = d[i];
      ctx.save();
      ctx.translate(o.x, o.y);
      ctx.rotate(o.rot);
      ctx.fillStyle = '#4c4a44';
      ctx.strokeStyle = 'rgba(0,0,0,0.6)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      if (o.shape === 0) {
        ctx.moveTo(-o.r, -o.r * 0.5); ctx.lineTo(o.r * 0.3, -o.r); ctx.lineTo(o.r, o.r * 0.2);
        ctx.lineTo(o.r * 0.1, o.r); ctx.lineTo(-o.r * 0.8, o.r * 0.4);
      } else if (o.shape === 1) {
        ctx.rect(-o.r * 0.8, -o.r * 0.5, o.r * 1.6, o.r);
      } else {
        ctx.moveTo(-o.r, 0); ctx.lineTo(0, -o.r * 0.85); ctx.lineTo(o.r, 0); ctx.lineTo(0, o.r * 0.85);
      }
      ctx.closePath();
      ctx.fill();
      ctx.stroke();
      ctx.fillStyle = 'rgba(255,178,87,0.35)';
      ctx.fillRect(-o.r * 0.3, -o.r * 0.12, o.r * 0.6, o.r * 0.24);
      ctx.restore();
    }
  };

  /* ---------------- pickups ---------------- */
  Renderer.prototype.drawPickups = function (ctx, world, game) {
    for (var i = 0; i < world.pickups.length; i++) {
      var pk = world.pickups[i];
      var res = SM.Resources.get(pk.resId);
      var icon = SM.Icons.resource(res);
      var bob = Math.sin(pk.phase) * 2.2;
      var fade = pk.life > pk.maxLife - 6 ? U.clamp((pk.maxLife - pk.life) / 6, 0, 1) : 1;
      var size = 22 + pk.qty * 1.6;
      ctx.save();
      ctx.globalAlpha = fade;
      ctx.globalCompositeOperation = 'lighter';
      var g = ctx.createRadialGradient(pk.x, pk.y + bob, 0, pk.x, pk.y + bob, size);
      g.addColorStop(0, hexA(res.glow, 0.35));
      g.addColorStop(1, hexA(res.glow, 0));
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(pk.x, pk.y + bob, size, 0, TAU); ctx.fill();
      ctx.globalCompositeOperation = 'source-over';
      ctx.drawImage(icon.canvas, pk.x - size / 2, pk.y + bob - size / 2, size, size);
      ctx.restore();
    }
  };

  /* ---------------- player ---------------- */
  Renderer.prototype.drawPlayer = function (ctx, world, game) {
    var p = world.player;
    var ship = SM.Ships.get(world.profile.shipId);
    var art = SM.Art.ship(ship.id);
    var cfg = SHIP_RENDER[ship.id] || SHIP_RENDER.prospector;
    var k = cfg.k;

    if (p.dead) {
      return;
    }

    // engine plumes (behind hull)
    var thrust = p.speed / Math.max(1, world.stats.maxSpeed);
    ctx.save();
    ctx.translate(p.x, p.y);
    ctx.rotate(p.angle);
    ctx.globalCompositeOperation = 'lighter';
    for (var i = 0; i < cfg.engines.length; i++) {
      var ex = cfg.engines[i][0] * k, ey = cfg.engines[i][1] * k;
      var len = 8 + thrust * 34 + Math.sin(world.time * 24 + i * 2) * 3;
      var wdt = 3.4 + thrust * 2.6;
      var g = ctx.createLinearGradient(ex, ey, ex - len, ey);
      g.addColorStop(0, 'rgba(220,255,255,0.95)');
      g.addColorStop(0.28, 'rgba(120,235,255,0.75)');
      g.addColorStop(0.7, 'rgba(60,150,255,0.28)');
      g.addColorStop(1, 'rgba(40,80,255,0)');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(ex, ey - wdt);
      ctx.quadraticCurveTo(ex - len * 0.5, ey - wdt * 0.6, ex - len, ey);
      ctx.quadraticCurveTo(ex - len * 0.5, ey + wdt * 0.6, ex, ey + wdt);
      ctx.closePath();
      ctx.fill();
      var gl = ctx.createRadialGradient(ex, ey, 0, ex, ey, 12 + thrust * 8);
      gl.addColorStop(0, 'rgba(200,245,255,0.6)');
      gl.addColorStop(1, 'rgba(90,200,255,0)');
      ctx.fillStyle = gl;
      ctx.beginPath(); ctx.arc(ex, ey, 12 + thrust * 8, 0, TAU); ctx.fill();
    }
    ctx.restore();

    // hull
    ctx.save();
    ctx.translate(p.x, p.y);
    ctx.rotate(p.angle);
    var dw = art.w * k, dh = art.h * k;
    ctx.drawImage(art.canvas, -dw / 2, -dh / 2, dw, dh);
    if (p.hitFlash > 0.01) {
      ctx.globalCompositeOperation = 'lighter';
      ctx.globalAlpha = p.hitFlash * 0.55;
      ctx.drawImage(art.canvas, -dw / 2, -dh / 2, dw, dh);
      ctx.globalAlpha = 1;
      ctx.globalCompositeOperation = 'source-over';
    }
    ctx.restore();

    // mining beam
    if (p.beam) {
      var b = p.beam;
      var flicker = 0.7 + 0.3 * Math.sin(world.time * 55);
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      ctx.lineCap = 'round';
      // outer bloom
      ctx.strokeStyle = 'rgba(95,230,216,0.16)';
      ctx.lineWidth = 13;
      ctx.beginPath(); ctx.moveTo(b.x1, b.y1); ctx.lineTo(b.x2, b.y2); ctx.stroke();
      // mid
      var lg = ctx.createLinearGradient(b.x1, b.y1, b.x2, b.y2);
      lg.addColorStop(0, 'rgba(150,255,245,' + (0.55 * flicker) + ')');
      lg.addColorStop(0.5, 'rgba(95,230,216,' + (0.8 * flicker) + ')');
      lg.addColorStop(1, 'rgba(230,255,255,' + (0.95 * flicker) + ')');
      ctx.strokeStyle = lg;
      ctx.lineWidth = 3.2 + Math.sin(world.time * 40) * 0.8;
      ctx.beginPath(); ctx.moveTo(b.x1, b.y1); ctx.lineTo(b.x2, b.y2); ctx.stroke();
      // core
      ctx.strokeStyle = 'rgba(255,255,255,' + (0.85 * flicker) + ')';
      ctx.lineWidth = 1.2;
      ctx.beginPath(); ctx.moveTo(b.x1, b.y1); ctx.lineTo(b.x2, b.y2); ctx.stroke();

      // impact flare
      if (b.hit) {
        var ig = ctx.createRadialGradient(b.x2, b.y2, 0, b.x2, b.y2, 26);
        ig.addColorStop(0, 'rgba(255,255,255,0.95)');
        ig.addColorStop(0.25, 'rgba(150,255,245,0.6)');
        ig.addColorStop(1, 'rgba(95,230,216,0)');
        ctx.fillStyle = ig;
        ctx.beginPath(); ctx.arc(b.x2, b.y2, 26, 0, TAU); ctx.fill();
        for (var s = 0; s < 4; s++) {
          var ang = world.time * 9 + s * 1.57;
          ctx.fillStyle = 'rgba(220,255,255,0.9)';
          ctx.beginPath();
          ctx.arc(b.x2 + Math.cos(ang) * (8 + s * 3), b.y2 + Math.sin(ang) * (8 + s * 3), 1.4, 0, TAU);
          ctx.fill();
        }
      }
      ctx.restore();
    }

    // shield bubble
    var spct = world.stats.maxShield > 0 ? p.shield / world.stats.maxShield : 0;
    if (spct > 0.02 || p.shieldFlash > 0.01) {
      ctx.save();
      ctx.translate(p.x, p.y);
      var rr = 42 + Math.sin(world.time * 2) * 1.5;
      ctx.globalCompositeOperation = 'lighter';
      var sg = ctx.createRadialGradient(0, 0, rr * 0.55, 0, 0, rr);
      sg.addColorStop(0, 'rgba(90,190,255,0)');
      sg.addColorStop(0.75, 'rgba(90,190,255,' + (0.05 + spct * 0.06 + p.shieldFlash * 0.25) + ')');
      sg.addColorStop(1, 'rgba(160,230,255,' + (0.10 + spct * 0.12 + p.shieldFlash * 0.4) + ')');
      ctx.fillStyle = sg;
      ctx.beginPath(); ctx.arc(0, 0, rr, 0, TAU); ctx.fill();
      // hex facets
      ctx.strokeStyle = 'rgba(150,220,255,' + (0.10 + p.shieldFlash * 0.5) + ')';
      ctx.lineWidth = 1;
      for (var hx = 0; hx < 6; hx++) {
        var a1 = hx / 6 * TAU + p.angle;
        var a2 = (hx + 1) / 6 * TAU + p.angle;
        ctx.beginPath();
        ctx.moveTo(Math.cos(a1) * rr, Math.sin(a1) * rr);
        ctx.lineTo(Math.cos(a2) * rr * 0.86, Math.sin(a2) * rr * 0.86);
        ctx.stroke();
      }
      ctx.restore();
    }

    // invulnerability shimmer
    if (p.invuln > 0.4) {
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      ctx.strokeStyle = 'rgba(160,240,255,' + (0.25 + 0.25 * Math.sin(world.time * 16)) + ')';
      ctx.lineWidth = 1.4;
      ctx.beginPath(); ctx.arc(p.x, p.y, 34, 0, TAU); ctx.stroke();
      ctx.restore();
    }
  };

  /* ---------------- drones ---------------- */
  Renderer.prototype.drawDrones = function (ctx, world, game) {
    var art = SM.Art.drone();
    for (var i = 0; i < world.drones.length; i++) {
      var dr = world.drones[i];
      // beam first
      if (dr.beam) {
        ctx.save();
        ctx.globalCompositeOperation = 'lighter';
        ctx.strokeStyle = 'rgba(120,255,220,0.22)';
        ctx.lineWidth = 7;
        ctx.beginPath(); ctx.moveTo(dr.x, dr.y); ctx.lineTo(dr.beam.x, dr.beam.y); ctx.stroke();
        ctx.strokeStyle = 'rgba(220,255,250,0.85)';
        ctx.lineWidth = 1.6;
        ctx.beginPath(); ctx.moveTo(dr.x, dr.y); ctx.lineTo(dr.beam.x, dr.beam.y); ctx.stroke();
        ctx.restore();
      }
      ctx.save();
      ctx.translate(dr.x, dr.y);
      ctx.rotate(dr.angle);
      var k = 0.62;
      ctx.drawImage(art.canvas, -art.w * k / 2, -art.h * k / 2, art.w * k, art.h * k);
      ctx.restore();

      // status pip + cargo bar
      if (dr.cargoUsed > 0.01) {
        var cap = SM.Ent.droneCapacity(world);
        var pct = U.clamp(dr.cargoUsed / Math.max(1, cap), 0, 1);
        ctx.save();
        ctx.fillStyle = 'rgba(0,0,0,0.5)';
        ctx.fillRect(dr.x - 11, dr.y + 15, 22, 3);
        ctx.fillStyle = pct > 0.85 ? '#ffb257' : '#5fe6d8';
        ctx.fillRect(dr.x - 11, dr.y + 15, 22 * pct, 3);
        ctx.restore();
      }
      // tether hint when returning
      if (dr.state === 'return') {
        ctx.save();
        ctx.globalAlpha = 0.3;
        ctx.strokeStyle = 'rgba(95,230,216,0.6)';
        ctx.setLineDash([4, 8]);
        ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(dr.x, dr.y); ctx.lineTo(world.player.x, world.player.y); ctx.stroke();
        ctx.restore();
      }
    }
  };

  /* ---------------- enemies ---------------- */
  Renderer.prototype.drawEnemies = function (ctx, world, game) {
    for (var i = 0; i < world.enemies.length; i++) {
      var e = world.enemies[i];
      var art = SM.Art.enemy(e.type);
      var def = e.def;
      var spawnIn = U.clamp(e.spawnT / 0.6, 0, 1);

      ctx.save();
      ctx.globalAlpha = spawnIn;
      // engine glow
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      var bg = ctx.createRadialGradient(
        e.x - Math.cos(e.angle) * e.radius, e.y - Math.sin(e.angle) * e.radius, 0,
        e.x - Math.cos(e.angle) * e.radius, e.y - Math.sin(e.angle) * e.radius, e.radius * 1.4);
      bg.addColorStop(0, hexA(def.art.color, 0.45));
      bg.addColorStop(1, hexA(def.art.color, 0));
      ctx.fillStyle = bg;
      ctx.beginPath();
      ctx.arc(e.x - Math.cos(e.angle) * e.radius, e.y - Math.sin(e.angle) * e.radius, e.radius * 1.4, 0, TAU);
      ctx.fill();
      ctx.restore();

      ctx.translate(e.x, e.y);
      ctx.rotate(e.angle);
      var k = 0.76;
      var dw = art.w * k, dh = art.h * k;
      // sentinel orbiting plates
      if (e.type === 'voidSentinel') {
        ctx.save();
        ctx.rotate(-e.angle + world.time * 0.8);
        ctx.strokeStyle = 'rgba(200,170,255,0.5)';
        ctx.lineWidth = 2;
        for (var pl = 0; pl < 3; pl++) {
          var a = world.time * 1.6 + pl * TAU / 3;
          ctx.beginPath();
          ctx.arc(0, 0, 40, a, a + 0.7);
          ctx.stroke();
        }
        ctx.restore();
      }
      ctx.drawImage(art.canvas, -dw / 2, -dh / 2, dw, dh);
      if (e.hitFlash > 0.01) {
        ctx.globalCompositeOperation = 'lighter';
        ctx.globalAlpha = spawnIn * e.hitFlash * 0.7;
        ctx.drawImage(art.canvas, -dw / 2, -dh / 2, dw, dh);
      }
      ctx.restore();

      // shield bubble
      if (e.shield > 0.5) {
        ctx.save();
        ctx.globalCompositeOperation = 'lighter';
        var spct2 = e.shield / Math.max(1, e.maxShield);
        ctx.strokeStyle = hexA(def.art.color, 0.16 + spct2 * 0.22 + e.shieldFlash * 0.5);
        ctx.lineWidth = 1.6;
        ctx.beginPath(); ctx.arc(e.x, e.y, e.radius + 8, 0, TAU); ctx.stroke();
        ctx.restore();
      }

      // health bar
      var hpPct = U.clamp(e.hp / e.maxHp, 0, 1);
      if (hpPct < 0.999 || e.maxShield > 0) {
        var bw = e.radius * 1.9;
        ctx.save();
        ctx.globalAlpha = spawnIn;
        ctx.fillStyle = 'rgba(0,0,0,0.55)';
        ctx.fillRect(e.x - bw / 2, e.y - e.radius - 16, bw, 4);
        ctx.fillStyle = hpPct > 0.55 ? '#ff7d6b' : hpPct > 0.25 ? '#ffb257' : '#ff4d4d';
        ctx.fillRect(e.x - bw / 2, e.y - e.radius - 16, bw * hpPct, 4);
        if (e.maxShield > 0) {
          ctx.fillStyle = 'rgba(120,200,255,0.8)';
          ctx.fillRect(e.x - bw / 2, e.y - e.radius - 20, bw * (e.shield / Math.max(1, e.maxShield)), 2.4);
        }
        ctx.restore();
      }

      // threat marker
      if (e.type === 'voidSentinel' || e.type === 'heavy') {
        ctx.save();
        ctx.globalAlpha = 0.55 * spawnIn;
        ctx.strokeStyle = hexA(def.art.color, 0.7);
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(e.x - 5, e.y - e.radius - 26);
        ctx.lineTo(e.x + 5, e.y - e.radius - 26);
        ctx.moveTo(e.x, e.y - e.radius - 31);
        ctx.lineTo(e.x, e.y - e.radius - 21);
        ctx.stroke();
        ctx.restore();
      }
    }
  };

  /* ---------------- projectiles ---------------- */
  Renderer.prototype.drawProjectiles = function (ctx, world, game) {
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (var i = 0; i < world.projectiles.length; i++) {
      var pr = world.projectiles[i];
      var st = pr.style;
      var speed = Math.hypot(pr.vx, pr.vy) || 1;
      var tx = pr.x - (pr.vx / speed) * st.len;
      var ty = pr.y - (pr.vy / speed) * st.len;
      var g = ctx.createLinearGradient(pr.x, pr.y, tx, ty);
      g.addColorStop(0, st.core);
      g.addColorStop(0.4, st.color);
      g.addColorStop(1, hexA(st.color, 0));
      ctx.strokeStyle = g;
      ctx.lineWidth = pr.r * 1.4;
      ctx.lineCap = 'round';
      ctx.beginPath(); ctx.moveTo(pr.x, pr.y); ctx.lineTo(tx, ty); ctx.stroke();
      ctx.fillStyle = hexA(st.color, 0.35);
      ctx.beginPath(); ctx.arc(pr.x, pr.y, pr.r * 2.6, 0, TAU); ctx.fill();
      ctx.fillStyle = st.core;
      ctx.beginPath(); ctx.arc(pr.x, pr.y, pr.r * 0.7, 0, TAU); ctx.fill();
      if (pr.type === 'orb' || pr.type === 'void') {
        ctx.strokeStyle = hexA(st.color, 0.5);
        ctx.lineWidth = 1;
        ctx.beginPath(); ctx.arc(pr.x, pr.y, pr.r * 1.8 + Math.sin(world.time * 12 + pr.wobble) * 2, 0, TAU); ctx.stroke();
      }
    }
    ctx.restore();
  };

  /* ---------------- particles ---------------- */
  Renderer.prototype.drawParticles = function (ctx, world, game) {
    var ps = world.particles;
    ctx.save();
    for (var i = 0; i < ps.list.length; i++) {
      var p = ps.list[i];
      var a = U.clamp(p.life / p.maxLife, 0, 1);
      switch (p.type) {
        case 'spark': {
          ctx.globalCompositeOperation = 'lighter';
          var len = 0.035;
          ctx.strokeStyle = hexA(p.color, a * 0.9);
          ctx.lineWidth = p.size * 0.8;
          ctx.beginPath();
          ctx.moveTo(p.x, p.y);
          ctx.lineTo(p.x - p.vx * len, p.y - p.vy * len);
          ctx.stroke();
          break;
        }
        case 'glow': {
          ctx.globalCompositeOperation = 'lighter';
          var g = ctx.createRadialGradient(p.x, p.y, 0, p.x, p.y, p.size * 3);
          g.addColorStop(0, hexA(p.color, a * 0.8));
          g.addColorStop(1, hexA(p.color, 0));
          ctx.fillStyle = g;
          ctx.beginPath(); ctx.arc(p.x, p.y, p.size * 3, 0, TAU); ctx.fill();
          break;
        }
        case 'smoke': {
          ctx.globalCompositeOperation = 'source-over';
          var sg = ctx.createRadialGradient(p.x, p.y, 0, p.x, p.y, p.size * 4);
          sg.addColorStop(0, hexA(p.color, a * 0.28 * (p.alpha || 1)));
          sg.addColorStop(1, hexA(p.color, 0));
          ctx.fillStyle = sg;
          ctx.beginPath(); ctx.arc(p.x, p.y, p.size * 4, 0, TAU); ctx.fill();
          break;
        }
        case 'frag': {
          ctx.globalCompositeOperation = 'source-over';
          ctx.save();
          ctx.translate(p.x, p.y);
          ctx.rotate(p.rot);
          ctx.fillStyle = hexA(p.color, a);
          ctx.beginPath();
          ctx.moveTo(-p.size, -p.size * 0.5);
          ctx.lineTo(p.size * 0.8, -p.size);
          ctx.lineTo(p.size, p.size * 0.6);
          ctx.lineTo(-p.size * 0.6, p.size);
          ctx.closePath();
          ctx.fill();
          ctx.restore();
          break;
        }
        case 'text': {
          ctx.globalCompositeOperation = 'source-over';
          ctx.font = SM.Font.mono(p.size, 600);
          ctx.fillStyle = hexA(p.color, a);
          ctx.textAlign = 'center';
          ctx.fillText(p.text, p.x, p.y);
          break;
        }
        default: {
          ctx.globalCompositeOperation = 'lighter';
          ctx.fillStyle = hexA(p.color, a);
          ctx.beginPath(); ctx.arc(p.x, p.y, p.size, 0, TAU); ctx.fill();
        }
      }
    }
    // rings
    ctx.globalCompositeOperation = 'lighter';
    for (var r = 0; r < ps.rings.length; r++) {
      var ring = ps.rings[r];
      var t = 1 - ring.life / ring.maxLife;
      var rad = U.lerp(ring.r0, ring.r1, U.easeOutCubic(t));
      ctx.strokeStyle = ring.color;
      ctx.globalAlpha = (1 - t) * 0.9;
      ctx.lineWidth = ring.width * (1 - t * 0.6);
      ctx.beginPath(); ctx.arc(ring.x, ring.y, rad, 0, TAU); ctx.stroke();
    }
    ctx.globalAlpha = 1;
    ctx.restore();
  };

  /* ---------------- screen space fx ---------------- */
  Renderer.prototype.drawScreenFX = function (ctx, world, game, dt) {
    var w = this.w, h = this.h;
    var pal = world.sector.palette;

    // sector tint
    ctx.save();
    ctx.fillStyle = pal.tint;
    ctx.fillRect(0, 0, w, h);

    // nebula fog hazard
    if (world.sector.hazard.kind === 'nebula') {
      ctx.fillStyle = 'rgba(35,10,55,0.16)';
      ctx.fillRect(0, 0, w, h);
    }

    // damage vignette
    var p = world.player;
    if (p.hitFlash > 0.01) {
      var dg = ctx.createRadialGradient(w / 2, h / 2, Math.min(w, h) * 0.28, w / 2, h / 2, Math.max(w, h) * 0.62);
      dg.addColorStop(0, 'rgba(255,40,40,0)');
      dg.addColorStop(1, 'rgba(255,30,30,' + (p.hitFlash * 0.42) + ')');
      ctx.fillStyle = dg;
      ctx.fillRect(0, 0, w, h);
    }
    if (p.dead) {
      ctx.fillStyle = 'rgba(10,0,0,0.45)';
      ctx.fillRect(0, 0, w, h);
    }

    // low hull warning pulse
    var hullPct = p.hull / Math.max(1, world.stats.maxHull);
    if (hullPct < 0.3 && !p.dead) {
      var pulse = (Math.sin(world.time * 5) * 0.5 + 0.5) * (1 - hullPct / 0.3);
      var hg = ctx.createRadialGradient(w / 2, h / 2, Math.min(w, h) * 0.3, w / 2, h / 2, Math.max(w, h) * 0.7);
      hg.addColorStop(0, 'rgba(255,50,50,0)');
      hg.addColorStop(1, 'rgba(255,40,40,' + (pulse * 0.28) + ')');
      ctx.fillStyle = hg;
      ctx.fillRect(0, 0, w, h);
    }
    ctx.restore();

    // scanlines + vignette overlay
    if (this.overlay) ctx.drawImage(this.overlay, 0, 0, w, h);

    // aiming reticle (in-world position)
    if (!p.dead && game.input && game.state === 'flight') {
      var aim = game.input.aimWorld;
      if (aim) {
        var target = world.getPickableTarget(aim.x, aim.y);
        ctx.save();
        ctx.globalCompositeOperation = 'lighter';
        var col = target ? (target === world.player.miningTarget ? '#ffffff' : '#9ff0ff') : 'rgba(150,200,220,0.55)';
        ctx.strokeStyle = col;
        ctx.lineWidth = 1.4;
        var mr = target ? 12 : 9;
        ctx.beginPath();
        ctx.arc(aim.x, aim.y, mr, 0, TAU);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(aim.x - mr - 6, aim.y); ctx.lineTo(aim.x - mr - 2, aim.y);
        ctx.moveTo(aim.x + mr + 2, aim.y); ctx.lineTo(aim.x + mr + 6, aim.y);
        ctx.moveTo(aim.x, aim.y - mr - 6); ctx.lineTo(aim.x, aim.y - mr - 2);
        ctx.moveTo(aim.x, aim.y + mr + 2); ctx.lineTo(aim.x, aim.y + mr + 6);
        ctx.stroke();
        if (target) {
          ctx.strokeStyle = hexA(SM.Resources.get(target.resourceId).color, 0.55);
          ctx.beginPath(); ctx.arc(aim.x, aim.y, mr + 7, 0, TAU); ctx.stroke();
        }
        ctx.restore();
      }
    }
  };

  SM.Renderer = Renderer;
  SM.SHIP_RENDER = SHIP_RENDER;
})(typeof window !== 'undefined' ? window : globalThis);
