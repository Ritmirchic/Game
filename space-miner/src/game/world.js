/* =========================================================================
   SPACE MINER — sector simulation: flight, mining, combat, hazards, drones
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, Ent = SM.Ent, TAU = U.TAU;

  var WORLD_W = SM.Sectors.WORLD_W, WORLD_H = SM.Sectors.WORLD_H;

  function World(game, profile, sectorId, seed) {
    this.game = game;
    this.profile = profile;
    this.sector = SM.Sectors.get(sectorId);
    this.seed = seed || ((Date.now() ^ (Math.random() * 1e9)) >>> 0);
    this.rng = new U.RNG(this.seed);
    this.stats = SM.Profile.stats(profile);
    this.time = 0;
    this._aid = 0;
    this.droneAuto = true;
    this.paused = false;

    this.asteroids = [];
    this.enemies = [];
    this.pickups = [];
    this.projectiles = [];
    this.drones = [];
    this.debris = [];
    this.storms = [];
    this.wells = [];
    this.particles = new SM.Particles(1200);
    this.toasts = [];

    var st = this.sector.station;
    this.station = { x: st.x, y: st.y, r: 96, name: st.name, phase: 0 };

    this.player = {
      x: st.x + 40, y: st.y + 300,
      vx: 0, vy: 0,
      angle: -Math.PI / 2,
      hull: this.stats.maxHull,
      shield: this.stats.maxShield,
      energy: this.stats.maxEnergy,
      dead: false,
      deadT: 0,
      invuln: 1.2,
      hitFlash: 0,
      shieldFlash: 0,
      shieldHitT: 0,
      beam: null,
      beamHeat: 0,
      weaponCd: 0,
      miningTarget: null,
      boost: 0,
      speed: 0,
      docked: false,
      cargoFullWarnCd: 0,
      repairTick: 0,
      engineGlow: 0,
      trailT: 0
    };

    this.bg = SM.Art.background(this.sector, this.seed);
    this.stars1 = SM.Art.starLayer(this.seed ^ 0x1234, 320, 2400, this.sector.palette);
    this.stars2 = SM.Art.starLayer(this.seed ^ 0x9876, 180, 2400, this.sector.palette);
    this.dust = SM.Art.dustLayer(this.seed ^ 0xbeef, 130, 2600, this.sector.palette);

    this.populate();
  }

  /* =======================  POPULATION  ============================== */
  World.prototype.pickResource = function () {
    var rng = this.rng;
    var weights = this.sector.asteroid.mineralWeights;
    var stats = this.stats;
    var rare = [];
    Object.keys(weights).forEach(function (id) {
      var r = SM.Resources.get(id);
      var w = weights[id];
      if (!r) return;
      var bias = 1;
      if (stats.rareFind > 0 && r.tier >= 8) bias = 1 + stats.rareFind * 3.4;
      rare.push({ id: id, w: w * bias });
    });
    return rng.weighted(rare, function (e) { return e.w; }).id;
  };

  World.prototype.positionFree = function (x, y, radius) {
    if (x < radius + 40 || y < radius + 40 || x > WORLD_W - radius - 40 || y > WORLD_H - radius - 40) return false;
    if (U.dist(x, y, this.player.x, this.player.y) < radius + 190) return false;
    if (U.dist(x, y, this.station.x, this.station.y) < radius + 200) return false;
    for (var i = 0; i < this.asteroids.length; i++) {
      var a = this.asteroids[i];
      if (U.dist(x, y, a.x, a.y) < radius + a.radius + 26) return false;
    }
    return true;
  };

  World.prototype.addAsteroid = function (opts) {
    var a = Ent.makeAsteroid(this, opts);
    a.dead = false;
    this.asteroids.push(a);
    return a;
  };

  World.prototype.spawnAsteroid = function (opts) {
    opts = opts || {};
    var rng = this.rng;
    var radius = opts.radius || rng.range(this.sector.asteroid.sizeMin, this.sector.asteroid.sizeMax);
    var family = opts.family || rng.pick(this.sector.asteroid.families);
    var resource = opts.resource || this.pickResource();

    for (var attempt = 0; attempt < 60; attempt++) {
      var x, y;
      if (attempt < 40) {
        x = rng.range(100, WORLD_W - 100);
        y = rng.range(100, WORLD_H - 100);
      } else {
        // widen out: bias to areas away from the player
        var ang = rng.range(0, TAU);
        var d = rng.range(520, Math.max(700, Math.min(WORLD_W, WORLD_H) * 0.45));
        x = U.clamp(this.player.x + Math.cos(ang) * d, 120, WORLD_W - 120);
        y = U.clamp(this.player.y + Math.sin(ang) * d, 120, WORLD_H - 120);
      }
      if (opts.x !== undefined && opts.y !== undefined) { x = opts.x; y = opts.y; }
      if (!this.positionFree(x, y, radius)) {
        if (opts.x !== undefined) { radius *= 0.8; if (radius < 16) return null; continue; }
        continue;
      }
      return this.addAsteroid({
        x: x, y: y, radius: radius, family: family, resource: resource,
        vx: rng.range(-7, 7), vy: rng.range(-7, 7), variant: rng.int(0, 7)
      });
    }
    return null;
  };

  World.prototype.populate = function () {
    var i;
    var count = this.sector.asteroid.density;
    for (i = 0; i < count; i++) this.spawnAsteroid();

    // a couple of rich, hand-placed rocks near the station to get started
    for (i = 0; i < 7; i++) {
      var ang = this.rng.range(0, TAU);
      var d = this.rng.range(260, 560);
      var a = this.spawnAsteroid({
        radius: this.rng.range(46, 70),
        resource: i === 0 ? 'iron' : i === 1 ? 'silicate' : this.pickResource()
      });
      if (a) { a.rich = true; a.hp *= 1.15; }
    }

    // initial hostiles
    var n = Math.min(2, this.sector.enemies.max);
    for (i = 0; i < n; i++) {
      var e = this.spawnEnemy(this.randomSpawnPoint(900, 1400));
      if (e) e.spawnT = 0;
    }

    // drones
    this.rebuildDrones();

    this.setupHazards();
  };

  World.prototype.rebuildDrones = function () {
    this.drones.length = 0;
    this.droneCount = this.stats.droneSlots;
    for (var i = 0; i < this.droneCount; i++) {
      var dr = Ent.makeDrone(this, i, (this.profile.drones[i] && this.profile.drones[i].serial) || i + 1);
      dr.slot = i;
      this.drones.push(dr);
    }
  };

  World.prototype.randPoint = function () {
    return { x: this.rng.range(150, WORLD_W - 150), y: this.rng.range(150, WORLD_H - 150) };
  };

  World.prototype.randomSpawnPoint = function (minD, maxD) {
    var p = this.player;
    for (var i = 0; i < 40; i++) {
      var a = this.rng.range(0, TAU);
      var d = this.rng.range(minD || 700, maxD || 1300);
      var x = U.clamp(p.x + Math.cos(a) * d, 90, WORLD_W - 90);
      var y = U.clamp(p.y + Math.sin(a) * d, 90, WORLD_H - 90);
      var blocked = false;
      for (var k = 0; k < this.asteroids.length; k++) {
        var as = this.asteroids[k];
        if (U.dist(x, y, as.x, as.y) < as.radius + 60) { blocked = true; break; }
      }
      if (!blocked && U.dist(x, y, p.x, p.y) > 520) return { x: x, y: y };
    }
    return { x: U.clamp(p.x + 900, 90, WORLD_W - 90), y: p.y };
  };

  World.prototype.setupHazards = function () {
    var h = this.sector.hazard;
    this.storms.length = 0;
    this.wells.length = 0;
    this.debris.length = 0;
    this.hazardTimer = h.kind === 'storm' ? 3 : 0;
    if (h.kind === 'gravity') {
      var n = 2 + Math.round(h.intensity);
      for (var i = 0; i < n; i++) {
        var pt = this.randPoint();
        this.wells.push({ x: pt.x, y: pt.y, r: this.rng.range(380, 620), strength: this.rng.range(180, 320) * h.intensity, phase: this.rng.range(0, TAU) });
      }
    }
    if (h.kind === 'debris') {
      for (var d = 0; d < 26; d++) this.spawnDebris();
    }
  };

  World.prototype.spawnDebris = function () {
    var rng = this.rng;
    var side = rng.int(0, 3);
    var x, y;
    if (side === 0) { x = rng.range(0, WORLD_W); y = -30; }
    else if (side === 1) { x = WORLD_W + 30; y = rng.range(0, WORLD_H); }
    else if (side === 2) { x = rng.range(0, WORLD_W); y = WORLD_H + 30; }
    else { x = -30; y = rng.range(0, WORLD_H); }
    var ang = Math.atan2(WORLD_H / 2 - y, WORLD_W / 2 - x) + rng.range(-0.5, 0.5);
    var sp = rng.range(60, 150);
    this.debris.push({
      x: x, y: y, vx: Math.cos(ang) * sp, vy: Math.sin(ang) * sp,
      r: rng.range(9, 26), rot: rng.range(0, TAU), vr: rng.range(-2, 2),
      dmg: rng.range(4, 9), hitCd: 0, shape: rng.int(0, 2)
    });
  };

  /* =======================  EFFECTS / FEEDBACK  ====================== */
  World.prototype.notify = function (text, kind, ttl) {
    this.toasts.push({ text: text, kind: kind || 'info', life: ttl || 2.6, maxLife: ttl || 2.6 });
    if (this.toasts.length > 5) this.toasts.shift();
  };

  World.prototype.sfx = function (name, x, y, vol) {
    if (this.game && this.game.audio) this.game.audio.play(name, x, y, vol, this.player, this.game.camera);
  };

  World.prototype.shake = function (amt) {
    if (this.game && this.game.addShake) this.game.addShake(amt);
  };

  /* =======================  MINING  ================================= */
  World.prototype.asteroidAt = function (x, y, pad) {
    for (var i = 0; i < this.asteroids.length; i++) {
      var a = this.asteroids[i];
      if (a.dead) continue;
      var r = a.radius * 0.94 + (pad || 0);
      if (U.dist2(x, y, a.x, a.y) <= r * r) return a;
    }
    return null;
  };

  /* nearest asteroid hit by a ray; returns {asteroid, x, y, dist} */
  World.prototype.raycastAsteroids = function (x0, y0, dx, dy, maxDist) {
    var best = null;
    for (var i = 0; i < this.asteroids.length; i++) {
      var a = this.asteroids[i];
      if (a.dead) continue;
      var ox = a.x - x0, oy = a.y - y0;
      var proj = ox * dx + oy * dy;
      if (proj < -a.radius || proj > maxDist + a.radius) continue;
      var perp2 = ox * ox + oy * oy - proj * proj;
      var rr = a.radius * 0.92;
      if (perp2 > rr * rr) continue;
      var hitT = proj - Math.sqrt(Math.max(0, rr * rr - perp2));
      if (hitT < 0) hitT = 0;
      if (hitT > maxDist) continue;
      if (!best || hitT < best.dist) {
        best = {
          asteroid: a,
          dist: hitT,
          x: x0 + dx * hitT,
          y: y0 + dy * hitT
        };
      }
    }
    return best;
  };

  World.prototype.mineAsteroid = function (a, dmg, source, dr) {
    if (!a || a.dead) return;
    var res = SM.Resources.get(a.resourceId);
    var hardness = res.hardness;
    if (!a.hardnessApplied) a.hardnessApplied = true;
    var eff = dmg / (0.55 + hardness * 0.12);
    if (source === 'player') eff *= this.stats.penetration;
    a.hp -= eff;
    a.damagePct = U.clamp(1 - a.hp / a.maxHp, 0, 1);
    a.hitFlash = Math.min(1, a.hitFlash + eff * 0.06);

    // sparks at the impact point
    if (this.rng.chance(0.55)) {
      var ang = this.rng.range(0, TAU);
      this.particles.add({
        x: a.x + Math.cos(ang) * a.radius * 0.9,
        y: a.y + Math.sin(ang) * a.radius * 0.9,
        vx: Math.cos(ang) * this.rng.range(40, 170),
        vy: Math.sin(ang) * this.rng.range(40, 170),
        life: this.rng.range(0.2, 0.5), maxLife: 0.5,
        size: this.rng.range(1, 2.6),
        color: this.rng.chance(0.6) ? res.color : '#ffe6b0',
        type: 'spark', drag: 2.2
      });
    }
    if (a.hp <= 0) this.destroyAsteroid(a, source);
  };

  World.prototype.destroyAsteroid = function (a, source) {
    if (a.dead) return;
    a.dead = true;
    var idx = this.asteroids.indexOf(a);
    if (idx >= 0) this.asteroids.splice(idx, 1);

    var res = SM.Resources.get(a.resourceId);
    var p = this.profile;
    var stats = this.stats;

    // ---- ore
    var qty = a.richness;
    var bonus = 0;
    if (this.rng.chance(0.12 + stats.rareFind * 0.5)) bonus = this.rng.int(1, 3);
    var total = qty + bonus;
    var chunks = U.clamp(Math.round(total / 2.2), 2, 7);
    var remaining = total;
    for (var i = 0; i < chunks; i++) {
      var q = i === chunks - 1 ? remaining : Math.max(1, Math.round(total / chunks));
      remaining -= q;
      if (q <= 0) continue;
      var ang = this.rng.range(0, TAU);
      var sp = this.rng.range(45, 135);
      var px = a.x + Math.cos(ang) * a.radius * this.rng.range(0.1, 0.7);
      var py = a.y + Math.sin(ang) * a.radius * this.rng.range(0.1, 0.7);
      this.pickups.push(Ent.makePickup(this, px, py, a.resourceId, q,
        Math.cos(ang) * sp + a.vx, Math.sin(ang) * sp + a.vy));
    }

    // ---- effects
    var col = res.color;
    this.particles.burst(a.x, a.y, 26, { color: col, speed: 180, size: 2.6, life: 0.8, type: 'spark' });
    this.particles.burst(a.x, a.y, 14, { color: '#c9b7a5', speed: 120, size: 3.4, life: 1.1, type: 'smoke', drag: 0.8 });
    for (var f = 0; f < 14; f++) {
      var fa = this.rng.range(0, TAU), fs = this.rng.range(40, 220);
      this.particles.add({
        x: a.x + this.rng.range(-a.radius * 0.6, a.radius * 0.6),
        y: a.y + this.rng.range(-a.radius * 0.6, a.radius * 0.6),
        vx: Math.cos(fa) * fs + a.vx, vy: Math.sin(fa) * fs + a.vy,
        life: this.rng.range(0.6, 1.4), maxLife: 1.4,
        size: this.rng.range(2, 5.5), color: this.rng.chance(0.5) ? col : '#7d8b96',
        type: 'frag', rot: this.rng.range(0, TAU), vr: this.rng.range(-6, 6), drag: 0.7
      });
    }
    this.particles.ring(a.x, a.y, a.radius * 0.5, a.radius * 2.1, 0.55, U.hexA(col, 0.55), 3);
    this.shake(2 + a.radius * 0.03);
    this.sfx('rockBreak', a.x, a.y, a.radius / 110);

    // ---- progression
    p.counters.asteroidsMined++;
    var xp = Math.round(4 + a.radius * 0.18 + res.hardness * 2.2);
    this.game.awardXp(xp);
    if (this.rng.chance(0.28)) this.notify('+' + total + ' ' + res.name.toUpperCase(), 'ore', 1.6);
  };

  /* =======================  PLAYER  ================================= */
  World.prototype.updatePlayer = function (dt, input) {
    var p = this.player;
    var s = this.stats;
    var profile = this.profile;

    p.invuln = Math.max(0, p.invuln - dt);
    p.hitFlash = Math.max(0, p.hitFlash - dt * 2.4);
    p.shieldFlash = Math.max(0, p.shieldFlash - dt * 2.2);
    p.shieldHitT = Math.max(0, p.shieldHitT - dt);
    p.weaponCd = Math.max(0, p.weaponCd - dt);
    p.cargoFullWarnCd = Math.max(0, p.cargoFullWarnCd - dt);

    if (p.dead) {
      p.deadT += dt;
      p.vx *= Math.pow(0.3, dt);
      p.vy *= Math.pow(0.3, dt);
      p.x += p.vx * dt; p.y += p.vy * dt;
      return;
    }

    // ---- steering
    var ax = 0, ay = 0;
    if (input && !p.docked) {
      ax = input.ax || 0;
      ay = input.ay || 0;
    }
    var mag = Math.hypot(ax, ay);
    if (mag > 1) { ax /= mag; ay /= mag; mag = 1; }

    var boosting = input && input.boost && p.energy > 1 && mag > 0.1;
    var accel = s.accel * (boosting ? 1.85 : 1);
    p.vx += ax * accel * dt;
    p.vy += ay * accel * dt;

    if (boosting) {
      p.energy = Math.max(0, p.energy - 16 * dt);
      p.boost = U.clamp(p.boost + dt * 6, 0, 1);
      if (this.rng.chance(dt * 30)) {
        this.particles.add({
          x: p.x - Math.cos(p.angle) * 30, y: p.y - Math.sin(p.angle) * 30,
          vx: -Math.cos(p.angle) * 120 + this.rng.range(-30, 30),
          vy: -Math.sin(p.angle) * 120 + this.rng.range(-30, 30),
          life: 0.35, maxLife: 0.35, size: 2.4, color: '#9ff0ff', type: 'glow'
        });
      }
    } else {
      p.boost = Math.max(0, p.boost - dt * 3);
    }

    // drag (space has none, but gameplay needs it)
    var drag = Math.pow(1 / (1 + s.drag * 0.42), dt * 60 * 0.06 + dt);
    var damping = Math.pow(0.06, dt);
    p.vx *= damping; p.vy *= damping;

    var speed = Math.hypot(p.vx, p.vy);
    var maxSpeed = s.maxSpeed * (boosting ? 1.5 : 1);
    if (speed > maxSpeed) {
      p.vx = p.vx / speed * maxSpeed;
      p.vy = p.vy / speed * maxSpeed;
      speed = maxSpeed;
    }
    p.speed = speed;

    p.x += p.vx * dt;
    p.y += p.vy * dt;

    // world bounds with soft bounce
    var pad = 40;
    if (p.x < pad) { p.x = pad; p.vx = Math.abs(p.vx) * 0.35; }
    if (p.y < pad) { p.y = pad; p.vy = Math.abs(p.vy) * 0.35; }
    if (p.x > WORLD_W - pad) { p.x = WORLD_W - pad; p.vx = -Math.abs(p.vx) * 0.35; }
    if (p.y > WORLD_H - pad) { p.y = WORLD_H - pad; p.vy = -Math.abs(p.vy) * 0.35; }

    // ---- facing
    var aim = input && input.aimWorld;
    if (aim) {
      var want = Math.atan2(aim.y - p.y, aim.x - p.x);
      p.angle = U.angleLerp(p.angle, want, 1 - Math.pow(0.0000005, dt));
    } else if (speed > 30) {
      p.angle = U.angleLerp(p.angle, Math.atan2(p.vy, p.vx), 1 - Math.pow(0.001, dt));
    }

    // ---- energy & shields
    var regen = s.energyRegen;
    p.energy = Math.min(s.maxEnergy, p.energy + regen * dt);
    if (p.shieldHitT <= 0) {
      p.shield = Math.min(s.maxShield, p.shield + s.shieldRegen * dt);
    }
    if (s.hullRepair > 0) {
      p.hull = Math.min(s.maxHull, p.hull + s.hullRepair * dt);
    }

    // ---- mining beam
    p.beam = null;
    p.miningTarget = null;
    if (input && input.mine && !p.docked && p.energy > 1) {
      var aimPt = aim || { x: p.x + Math.cos(p.angle) * 300, y: p.y + Math.sin(p.angle) * 300 };
      var dx = aimPt.x - p.x, dy = aimPt.y - p.y;
      var dl = Math.hypot(dx, dy) || 1;
      dx /= dl; dy /= dl;
      var range = s.laserRange;
      var hit = this.raycastAsteroids(p.x + dx * 34, p.y + dy * 34, dx, dy, range);
      var fromX = p.x + dx * 30, fromY = p.y + dy * 30;
      var endX = p.x + dx * range, endY = p.y + dy * range;
      if (hit) {
        endX = hit.x; endY = hit.y;
        this.mineAsteroid(hit.asteroid, s.laserPower * dt, 'player');
        p.miningTarget = hit.asteroid;
        p.energy = Math.max(0, p.energy - 7.5 * s.energyCost * dt);
        p.beamHeat = Math.min(1, p.beamHeat + dt * 1.6);
        // face the rock while mining
        p.angle = U.angleLerp(p.angle, Math.atan2(dy, dx), 1 - Math.pow(0.00001, dt));
        // enemies take splash from the beam too
        for (var i = 0; i < this.enemies.length; i++) {
          var e = this.enemies[i];
          if (e.dead) continue;
          var perp = Math.abs((e.x - p.x) * dy - (e.y - p.y) * dx);
          var along = (e.x - p.x) * dx + (e.y - p.y) * dy;
          if (along > 0 && along < hit.dist && perp < e.radius) {
            this.damageEnemy(e, s.laserPower * 0.35 * dt, 'beam');
          }
        }
      } else {
        p.energy = Math.max(0, p.energy - 3.4 * s.energyCost * dt);
        p.beamHeat = Math.min(1, p.beamHeat + dt * 0.9);
        // beam can also burn enemies directly
        for (var k = 0; k < this.enemies.length; k++) {
          var en = this.enemies[k];
          if (en.dead) continue;
          var perp2 = Math.abs((en.x - p.x) * dy - (en.y - p.y) * dx);
          var along2 = (en.x - p.x) * dx + (en.y - p.y) * dy;
          if (along2 > 0 && along2 < range && perp2 < en.radius) {
            this.damageEnemy(en, s.laserPower * 0.4 * dt, 'beam');
          }
        }
      }
      p.beam = { x1: fromX, y1: fromY, x2: endX, y2: endY, heat: p.beamHeat, hit: !!hit };
      if (this.rng.chance(dt * 40) && hit) {
        this.particles.add({
          x: endX, y: endY, vx: this.rng.range(-90, 90), vy: this.rng.range(-90, 90),
          life: 0.3, maxLife: 0.3, size: 2, color: '#a8fff2', type: 'spark', drag: 2
        });
      }
      if (!this._beamLoop) { this.sfx('beamLoop', p.x, p.y); this._beamLoop = true; }
    } else {
      p.beamHeat = Math.max(0, p.beamHeat - dt * 1.4);
      this._beamLoop = false;
    }

    // ---- weapons
    if (input && (input.fire || input.click) && !p.docked && p.weaponCd <= 0 && p.energy > 4) {
      var aim2 = aim || { x: p.x + Math.cos(p.angle) * 300, y: p.y + Math.sin(p.angle) * 300 };
      var a2 = Math.atan2(aim2.y - p.y, aim2.x - p.x) + this.rng.range(-0.02, 0.02);
      var sp = s.weaponSpeed;
      this.projectiles.push(Ent.makeProjectile(this, {
        x: p.x + Math.cos(a2) * 34, y: p.y + Math.sin(a2) * 34,
        vx: Math.cos(a2) * sp + p.vx * 0.35,
        vy: Math.sin(a2) * sp + p.vy * 0.35,
        dmg: s.weaponDamage, team: 'player', type: 'player', life: 1.9
      }));
      p.weaponCd = s.weaponCooldown;
      p.energy = Math.max(0, p.energy - 4);
      this.sfx('shoot', p.x, p.y);
      this.particles.burst(p.x + Math.cos(a2) * 36, p.y + Math.sin(a2) * 36, 3,
        { color: '#9ff0ff', speed: 90, size: 1.6, life: 0.16, angle: a2, spread: 0.7 });
      this.shake(0.5);
    }

    // ---- engine trail
    p.engineGlow = U.clamp(p.engineGlow + (mag > 0.2 ? dt * 4 : -dt * 3), 0, 1);
    if (mag > 0.2 && this.rng.chance(dt * 55)) {
      var back = p.angle + Math.PI;
      this.particles.add({
        x: p.x + Math.cos(back) * 30 + this.rng.range(-6, 6),
        y: p.y + Math.sin(back) * 30 + this.rng.range(-6, 6),
        vx: Math.cos(back) * 70 + this.rng.range(-20, 20),
        vy: Math.sin(back) * 70 + this.rng.range(-20, 20),
        life: 0.32, maxLife: 0.32, size: 2.2, color: '#9ff0ff', type: 'glow'
      });
    }

    // ---- asteroid collision
    for (var ai = 0; ai < this.asteroids.length; ai++) {
      var as = this.asteroids[ai];
      var dd = U.dist(p.x, p.y, as.x, as.y);
      var minD = as.radius * 0.86 + 26;
      if (dd < minD && dd > 0.01) {
        var nx = (p.x - as.x) / dd, ny = (p.y - as.y) / dd;
        var overlap = minD - dd;
        p.x += nx * overlap; p.y += ny * overlap;
        var vn = p.vx * nx + p.vy * ny;
        if (vn < 0) {
          p.vx -= nx * vn * 1.5;
          p.vy -= ny * vn * 1.5;
          if (-vn > 180) {
            this.damagePlayer(-vn * 0.06, 'impact');
            this.particles.burst(p.x + nx * 24, p.y + ny * 24, 6, { color: '#cfe6f2', speed: 100, size: 2, life: 0.4 });
            this.shake(2);
          }
        }
      }
    }
  };

  World.prototype.damagePlayer = function (amount, source) {
    var p = this.player;
    var s = this.stats;
    if (p.dead || p.invuln > 0) return;
    var dmg = Math.max(0, amount) * (1 - (s.armor || 0));
    if (p.shield > 0) {
      var absorbed = Math.min(p.shield, dmg);
      p.shield -= absorbed;
      dmg -= absorbed;
      p.shieldFlash = 1;
      p.shieldHitT = 2.6;
      if (absorbed > 0) {
        this.particles.ring(p.x, p.y, 26, 52, 0.35, 'rgba(120,210,255,0.65)', 2.5);
        this.sfx('shieldHit', p.x, p.y);
      }
    }
    if (dmg > 0) {
      p.hull -= dmg;
      p.hitFlash = 1;
      p.invuln = 0.18;
      this.sfx('hullHit', p.x, p.y);
      this.particles.burst(p.x, p.y, 5, { color: '#ff6a6a', speed: 110, size: 2.2, life: 0.4 });
      if (source !== 'storm') this.shake(Math.min(9, 2 + dmg * 0.22));
    }
    if (p.hull <= 0) {
      p.hull = 0;
      this.killPlayer();
    }
  };

  World.prototype.killPlayer = function () {
    var p = this.player;
    if (p.dead) return;
    p.dead = true;
    p.deadT = 0;
    this.particles.burst(p.x, p.y, 42, { color: '#ffb257', speed: 300, size: 3.4, life: 1.2 });
    this.particles.burst(p.x, p.y, 22, { color: '#9ff0ff', speed: 220, size: 2.4, life: 0.9 });
    this.particles.ring(p.x, p.y, 20, 190, 1.1, 'rgba(255,178,87,0.7)', 5);
    this.shake(16);
    this.sfx('playerDeath', p.x, p.y);
    this.profile.counters.deaths++;
    this.game.onPlayerDeath();
  };

  World.prototype.respawn = function () {
    var p = this.player;
    var s = this.stats;
    p.dead = false;
    p.deadT = 0;
    p.x = this.station.x + 30;
    p.y = this.station.y + 210;
    p.vx = 0; p.vy = 0;
    p.hull = s.maxHull;
    p.shield = s.maxShield;
    p.energy = s.maxEnergy;
    p.invuln = 3;
    if (this.game && this.game.camera) {
      this.game.camera.x = p.x;
      this.game.camera.y = p.y;
    }
  };

  /* =======================  ENEMIES  ================================ */
  World.prototype.spawnEnemy = function (pos) {
    if (this.enemies.length >= this.sector.enemies.max) return null;
    var defs = this.sector.enemies.types;
    var pick = this.rng.weighted(defs, function (d) { return d[1]; });
    var type = pick ? pick[0] : 'scout';
    var pt = pos || this.randomSpawnPoint();
    var e = Ent.makeEnemy(this, type, pt.x, pt.y);
    this.enemies.push(e);
    // warp-in flash
    this.particles.ring(pt.x, pt.y, 10, 60, 0.5, U.hexA(e.def.art.color, 0.6), 2);
    return e;
  };

  World.prototype.damageEnemy = function (e, dmg, source, fromPlayer, hitX, hitY) {
    if (e.dead) return;
    var dealt = dmg;
    if (e.shield > 0) {
      var abs = Math.min(e.shield, dealt);
      e.shield -= abs;
      dealt -= abs;
      e.shieldFlash = 1;
    }
    e.hp -= dealt;
    e.hitFlash = 1;
    if (dealt > 0.6) {
      this.particles.text(hitX || e.x, (hitY || e.y) - e.radius * 0.6, Math.round(dealt),
        fromPlayer ? '#ffe9a8' : '#ffffff', fromPlayer ? 13 : 11);
    }
    if (dmg >= 4) this.sfx('enemyHit', e.x, e.y);
    if (e.hp <= 0) this.killEnemy(e);
  };

  World.prototype.killEnemy = function (e) {
    if (e.dead) return;
    e.dead = true;
    var idx = this.enemies.indexOf(e);
    if (idx >= 0) this.enemies.splice(idx, 1);
    var def = e.def;
    this.particles.burst(e.x, e.y, 30, { color: def.art.color, speed: 240, size: 3, life: 0.9 });
    this.particles.burst(e.x, e.y, 14, { color: '#ffd9a0', speed: 160, size: 2.4, life: 0.6 });
    for (var i = 0; i < 8; i++) {
      var a = this.rng.range(0, TAU), sp = this.rng.range(50, 220);
      this.particles.add({
        x: e.x, y: e.y, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp,
        life: this.rng.range(0.6, 1.2), maxLife: 1.2, size: this.rng.range(2, 4.5),
        color: this.rng.chance(0.6) ? def.art.color : '#5b6672',
        type: 'frag', rot: this.rng.range(0, TAU), vr: this.rng.range(-8, 8), drag: 0.8
      });
    }
    this.particles.ring(e.x, e.y, e.radius, e.radius * 4, 0.6, U.hexA(def.art.color, 0.5), 3);
    this.shake(4 + def.radius * 0.08);
    this.sfx('explode', e.x, e.y);

    var reward = Math.round(def.reward * (1 + this.sector.index * 0.08));
    this.profile.credits += reward;
    this.profile.counters.creditsEarned += reward;
    this.profile.counters.kills++;
    this.game.awardXp(def.xp);
    this.notify('+' + reward + ' CR  ·  ' + def.name + ' DESTROYED', 'good', 2);
    this.particles.text(e.x, e.y - 20, '+' + reward, '#ffd166', 14);

    // salvage drop
    if (this.rng.chance(0.55)) {
      var resId = this.pickResource();
      this.pickups.push(Ent.makePickup(this, e.x, e.y, resId, this.rng.int(1, 3),
        this.rng.range(-60, 60), this.rng.range(-60, 60)));
    }
    this.game.checkAchievementsNow();
  };

  /* =======================  PICKUPS  ================================= */
  World.prototype.updatePickups = function (dt) {
    var p = this.player;
    var s = this.stats;
    for (var i = this.pickups.length - 1; i >= 0; i--) {
      var pk = this.pickups[i];
      pk.life += dt;
      var target = null, collectR = 30;
      var dPlayer = U.dist(pk.x, pk.y, p.x, p.y);
      if (!p.dead && dPlayer < s.magnetRadius) target = p;
      if (!target) {
        var best = null, bd = 1e9;
        for (var d = 0; d < this.drones.length; d++) {
          var dr = this.drones[d];
          if (dr.cargoUsed >= SM.Ent.droneCapacity(this) * 0.95) continue;
          var dd = U.dist(pk.x, pk.y, dr.x, dr.y);
          if (dd < 260 && dd < bd) { bd = dd; best = dr; }
        }
        if (best) { target = best; collectR = 24; }
      }

      if (target) {
        var dx = target.x - pk.x, dy = target.y - pk.y;
        var dl = Math.hypot(dx, dy) || 1;
        var pull = 900 * s.collectSpeed / 330;
        pk.vx += (dx / dl) * pull * dt;
        pk.vy += (dy / dl) * pull * dt;
        var sp = Math.hypot(pk.vx, pk.vy);
        if (sp > 620) { pk.vx = pk.vx / sp * 620; pk.vy = pk.vy / sp * 620; }
        if (dl < collectR) {
          // collect
          if (target === p) {
            var added = SM.Profile.addResource(this.profile, pk.resId, pk.qty);
            if (added > 0) {
              this.sfx('pickup', pk.x, pk.y);
              this.particles.burst(pk.x, pk.y, 4, { color: SM.Resources.get(pk.resId).glow, speed: 70, size: 1.6, life: 0.3 });
              if (added < pk.qty) {
                pk.qty -= added;
                this.warnCargoFull();
                continue;
              }
              this.pickups.splice(i, 1);
              continue;
            } else {
              this.warnCargoFull();
            }
          } else {
            if (pk.qty <= 0) { this.pickups.splice(i, 1); continue; }
            // drone collection
            var res = SM.Resources.get(pk.resId);
            var capLeft = SM.Ent.droneCapacity(this) - target.cargoUsed;
            var can = Math.max(0, Math.floor(capLeft / res.mass));
            var take = Math.min(pk.qty, can);
            if (take > 0) {
              SM.Ent.addDroneCargo(this, target, pk.resId, take);
              pk.qty -= take;
              this.sfx('pickup', pk.x, pk.y, 0.5);
              this.particles.burst(pk.x, pk.y, 3, { color: res.glow, speed: 60, size: 1.4, life: 0.25 });
              if (pk.qty <= 0) { this.pickups.splice(i, 1); continue; }
            }
          }
        }
      }

      var dr2 = Math.pow(0.4, dt);
      pk.vx *= dr2; pk.vy *= dr2;
      pk.x += pk.vx * dt;
      pk.y += pk.vy * dt;
      pk.phase += dt * 3;
      if (pk.life > pk.maxLife) this.pickups.splice(i, 1);
    }
  };

  World.prototype.warnCargoFull = function () {
    var p = this.player;
    if (p.cargoFullWarnCd > 0) return;
    p.cargoFullWarnCd = 3;
    this.notify('CARGO HOLD FULL — DOCK TO SELL', 'warn', 3);
    this.sfx('alarm', p.x, p.y, 0.5);
  };

  /* =======================  COMBAT  ================================= */
  World.prototype.updateProjectiles = function (dt) {
    var p = this.player;
    for (var i = this.projectiles.length - 1; i >= 0; i--) {
      var pr = this.projectiles[i];
      pr.life -= dt;
      pr.x += pr.vx * dt;
      pr.y += pr.vy * dt;
      if (pr.life <= 0 || pr.x < -50 || pr.y < -50 || pr.x > WORLD_W + 50 || pr.y > WORLD_H + 50) {
        this.projectiles.splice(i, 1);
        continue;
      }
      // asteroids stop shots
      var hitA = null;
      for (var a = 0; a < this.asteroids.length; a++) {
        var as = this.asteroids[a];
        if (U.dist2(pr.x, pr.y, as.x, as.y) < as.radius * as.radius * 0.85) { hitA = as; break; }
      }
      if (hitA) {
        this.particles.burst(pr.x, pr.y, 4, { color: pr.style.color, speed: 90, size: 1.8, life: 0.3 });
        if (pr.team === 'player') this.mineAsteroid(hitA, pr.dmg * 0.5, 'weapon');
        this.projectiles.splice(i, 1);
        continue;
      }

      if (pr.team === 'player') {
        var hitE = null;
        for (var e = 0; e < this.enemies.length; e++) {
          var en = this.enemies[e];
          if (en.dead) continue;
          if (U.dist2(pr.x, pr.y, en.x, en.y) < (en.radius + pr.r) * (en.radius + pr.r)) { hitE = en; break; }
        }
        if (hitE) {
          this.damageEnemy(hitE, pr.dmg, 'weapon', true, pr.x, pr.y);
          this.particles.burst(pr.x, pr.y, 6, { color: '#ffe9a8', speed: 120, size: 2, life: 0.3 });
          this.projectiles.splice(i, 1);
          continue;
        }
      } else if (!p.dead) {
        var rr = 20;
        if (U.dist2(pr.x, pr.y, p.x, p.y) < (rr + pr.r) * (rr + pr.r)) {
          this.damagePlayer(pr.dmg, 'projectile');
          this.particles.burst(pr.x, pr.y, 7, { color: pr.style.color, speed: 130, size: 2.2, life: 0.35 });
          this.projectiles.splice(i, 1);
          continue;
        }
      }
    }
  };

  World.prototype.updateEnemies = function (dt) {
    var p = this.player;
    for (var i = this.enemies.length - 1; i >= 0; i--) {
      var e = this.enemies[i];
      Ent.updateEnemy(this, e, dt);

      // contact damage
      if (!p.dead && U.dist(e.x, e.y, p.x, p.y) < e.radius + 22) {
        e.ramCd = (e.ramCd || 0) - dt;
        if (e.ramCd <= 0) {
          this.damagePlayer(e.def.damage * 0.7, 'ram');
          e.ramCd = 0.8;
          e.vx *= -0.5; e.vy *= -0.5;
          this.particles.burst((e.x + p.x) / 2, (e.y + p.y) / 2, 8, { color: '#ffb257', speed: 120, size: 2, life: 0.35 });
        }
      }

      // despawn far strays
      if (U.dist(e.x, e.y, p.x, p.y) > 2600) {
        this.enemies.splice(i, 1);
        continue;
      }
    }

    // spawn management
    this.enemySpawnT = (this.enemySpawnT || 0) + dt;
    var want = this.sector.enemies.max;
    if (this.enemies.length < want && this.enemySpawnT > this.sector.enemies.spawnEvery) {
      this.enemySpawnT = 0;
      if (!p.dead) {
        var e2 = this.spawnEnemy(this.randomSpawnPoint(900, 1500));
        if (e2) this.notify('HOSTILE SIGNATURE · ' + e2.def.short, 'warn', 2);
      }
    }
  };

  /* =======================  DRONES  ================================= */
  World.prototype.updateDrones = function (dt) {
    for (var i = 0; i < this.drones.length; i++) {
      Ent.updateDrone(this, this.drones[i], dt);
    }
  };

  World.prototype.findDroneTarget = function (x, y, maxDist) {
    var best = null, bd = maxDist * maxDist;
    for (var i = 0; i < this.asteroids.length; i++) {
      var a = this.asteroids[i];
      if (a.dead || a.reserved) continue;
      var d = U.dist2(x, y, a.x, a.y);
      if (d < bd) { bd = d; best = a; }
    }
    if (best) best.reserved = false;
    return best;
  };

  /* =======================  HAZARDS  ================================ */
  World.prototype.updateHazards = function (dt) {
    var h = this.sector.hazard;
    var p = this.player;

    if (h.kind === 'debris') {
      for (var i = this.debris.length - 1; i >= 0; i--) {
        var d = this.debris[i];
        d.x += d.vx * dt; d.y += d.vy * dt;
        d.rot += d.vr * dt;
        d.hitCd = Math.max(0, d.hitCd - dt);
        if (d.x < -80 || d.y < -80 || d.x > WORLD_W + 80 || d.y > WORLD_H + 80) {
          this.debris.splice(i, 1);
          this.spawnDebris();
          continue;
        }
        if (!p.dead && d.hitCd <= 0 && U.dist(d.x, d.y, p.x, p.y) < d.r + 20) {
          this.damagePlayer(d.dmg, 'impact');
          d.hitCd = 1;
          this.particles.burst(d.x, d.y, 8, { color: '#b0a89a', speed: 120, size: 2, life: 0.5 });
          d.vx *= -0.4; d.vy *= -0.4;
        }
        // debris can be shot for a little ore
        for (var k = this.projectiles.length - 1; k >= 0; k--) {
          var pr = this.projectiles[k];
          if (pr.team === 'player' && U.dist2(pr.x, pr.y, d.x, d.y) < d.r * d.r) {
            this.projectiles.splice(k, 1);
            this.particles.burst(d.x, d.y, 10, { color: '#cbbfa8', speed: 150, size: 2.2, life: 0.6 });
            this.debris.splice(i, 1);
            this.spawnDebris();
            if (this.rng.chance(0.35)) {
              this.pickups.push(Ent.makePickup(this, d.x, d.y, this.pickResource(), 1, this.rng.range(-70, 70), this.rng.range(-70, 70)));
            }
            break;
          }
        }
      }
    }

    if (h.kind === 'storm') {
      this.hazardTimer -= dt;
      if (this.hazardTimer <= 0 && !p.dead) {
        this.hazardTimer = this.rng.range(3.4, 6.2) / (0.7 + h.intensity * 0.4);
        var ang = this.rng.range(0, TAU);
        var dist = this.rng.range(120, 460);
        this.storms.push({
          x: p.x + Math.cos(ang) * dist,
          y: p.y + Math.sin(ang) * dist,
          t: 0, tele: 1.15, done: false, seed: this.rng.next() * 1000
        });
      }
      for (var s = this.storms.length - 1; s >= 0; s--) {
        var st = this.storms[s];
        st.t += dt;
        if (!st.done && st.t > st.tele) {
          st.done = true;
          st.strikeT = 0;
          this.particles.burst(st.x, st.y, 20, { color: '#bfa8ff', speed: 260, size: 2.6, life: 0.5 });
          this.particles.ring(st.x, st.y, 10, 110, 0.4, 'rgba(190,160,255,0.7)', 3);
          this.shake(6);
          this.sfx('thunder', st.x, st.y);
          if (!p.dead && U.dist(st.x, st.y, p.x, p.y) < 120) {
            this.damagePlayer(17 * h.intensity, 'storm');
          }
          for (var en = 0; en < this.enemies.length; en++) {
            if (U.dist(st.x, st.y, this.enemies[en].x, this.enemies[en].y) < 120) {
              this.damageEnemy(this.enemies[en], 26, 'storm', false);
            }
          }
        }
        if (st.done) st.strikeT += dt;
        if (st.t > st.tele + 0.55) this.storms.splice(s, 1);
      }
    }

    if (h.kind === 'gravity') {
      for (var w = 0; w < this.wells.length; w++) {
        var well = this.wells[w];
        well.phase += dt;
        var dx = well.x - p.x, dy = well.y - p.y;
        var d2 = Math.hypot(dx, dy) || 1;
        if (d2 < well.r) {
          var f = (1 - d2 / well.r) * well.strength;
          if (!p.dead) {
            p.vx += (dx / d2) * f * dt;
            p.vy += (dy / d2) * f * dt;
          }
          for (var ai = 0; ai < this.asteroids.length; ai++) {
            var as = this.asteroids[ai];
            var ax = well.x - as.x, ay = well.y - as.y;
            var ad = Math.hypot(ax, ay) || 1;
            if (ad < well.r) {
              var af = (1 - ad / well.r) * well.strength * 0.12;
              as.vx += (ax / ad) * af * dt;
              as.vy += (ay / ad) * af * dt;
            }
          }
        }
      }
      // draw occasional dust falling into wells
      if (this.rng.chance(dt * 6)) {
        var ws = this.wells[this.rng.int(0, this.wells.length - 1)];
        if (ws) {
          var a3 = this.rng.range(0, TAU), rr = this.rng.range(ws.r * 0.5, ws.r);
          this.particles.add({
            x: ws.x + Math.cos(a3) * rr, y: ws.y + Math.sin(a3) * rr,
            vx: -Math.cos(a3) * 40, vy: -Math.sin(a3) * 40,
            life: 1.4, maxLife: 1.4, size: 1.4, color: '#b07dff', type: 'glow', drag: 0
          });
        }
      }
    }

    if (h.kind === 'nebula') {
      if (this.rng.chance(dt * 3)) {
        this.particles.add({
          x: p.x + this.rng.range(-700, 700), y: p.y + this.rng.range(-500, 500),
          vx: this.rng.range(-8, 8), vy: this.rng.range(-8, 8),
          life: 4, maxLife: 4, size: this.rng.range(3, 9),
          color: this.sector.palette.nebula[this.rng.int(0, 2)], type: 'smoke', drag: 0.2, alpha: 0.25
        });
      }
    }
  };

  /* =======================  MAIN UPDATE  ============================ */
  World.prototype.update = function (dt, input) {
    dt = Math.min(dt, 0.05);
    if (this.paused) return;
    this.time += dt;
    this.profile.counters.playTime += dt;

    this.updatePlayer(dt, input);
    this.updateDrones(dt);
    this.updateEnemies(dt);
    this.updateProjectiles(dt);
    this.updatePickups(dt);
    this.updateHazards(dt);

    // asteroid drift + spin
    for (var i = 0; i < this.asteroids.length; i++) {
      var a = this.asteroids[i];
      a.x += a.vx * dt;
      a.y += a.vy * dt;
      a.rot += a.vr * dt;
      a.hitFlash = Math.max(0, a.hitFlash - dt * 2.4);
      a.glowPhase += dt;
      if (a.x < a.radius) { a.x = a.radius; a.vx = Math.abs(a.vx); }
      if (a.y < a.radius) { a.y = a.radius; a.vy = Math.abs(a.vy); }
      if (a.x > WORLD_W - a.radius) { a.x = WORLD_W - a.radius; a.vx = -Math.abs(a.vx); }
      if (a.y > WORLD_H - a.radius) { a.y = WORLD_H - a.radius; a.vy = -Math.abs(a.vy); }
    }

    this.station.phase += dt * 0.25;
    this.particles.update(dt);

    for (var t = this.toasts.length - 1; t >= 0; t--) {
      this.toasts[t].life -= dt;
      if (this.toasts[t].life <= 0) this.toasts.splice(t, 1);
    }

    // recharge / heat
    if (this.player.beam) this.player.beamHeat = this.player.beam.heat;
  };

  World.prototype.distanceToStation = function () {
    return U.dist(this.player.x, this.player.y, this.station.x, this.station.y);
  };

  World.prototype.canDock = function () {
    return !this.player.dead && this.distanceToStation() < this.station.r + 90;
  };

  World.prototype.getPickableTarget = function (x, y) {
    return this.asteroidAt(x, y, 12);
  };

  SM.World = World;
})(typeof window !== 'undefined' ? window : globalThis);
