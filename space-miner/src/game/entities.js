/* =========================================================================
   SPACE MINER — entity factories and per-entity simulation
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, TAU = U.TAU;

  /* ------------------------------------------------------------------ */
  /*  ASTEROIDS                                                          */
  /* ------------------------------------------------------------------ */
  function makeAsteroid(world, opts) {
    var rng = world.rng;
    var radius = opts.radius;
    var family = opts.family;
    var resourceId = opts.resource;
    var variant = opts.variant !== undefined ? opts.variant : rng.int(0, 7);
    var art = SM.AsteroidArt.get(family, radius, variant, resourceId);
    var res = SM.Resources.get(resourceId);
    var sizeFactor = 0.5 + radius / 150;
    var hpBase = (11 + radius * 0.78) * (0.55 + res.hardness * 0.12) * rng.range(0.85, 1.18);
    var yieldQty = Math.max(2, Math.round(radius / 7.2 * rng.range(0.75, 1.35) * sizeFactor));

    var a = {
      kind: 'asteroid',
      x: opts.x, y: opts.y,
      vx: opts.vx || 0, vy: opts.vy || 0,
      radius: radius,
      drawRadius: radius * (art.size / art.baseRadius) * 0.5,
      spriteScale: radius / art.baseRadius,
      art: art,
      family: family,
      resourceId: resourceId,
      rot: rng.range(0, TAU),
      vr: rng.range(-0.32, 0.32) * (radius < 60 ? 1.5 : 0.8),
      hp: hpBase, maxHp: hpBase,
      richness: yieldQty,
      hitFlash: 0,
      damagePct: 0,
      glowPhase: rng.range(0, TAU),
      cracked: false,
      id: ++world._aid,
      spawnT: 0
    };
    return a;
  }

  function asteroidPenetrationStat(res) {
    return 0.55 + res.hardness * 0.12;
  }

  /* ------------------------------------------------------------------ */
  /*  PICKUPS                                                           */
  /* ------------------------------------------------------------------ */
  function makePickup(world, x, y, resId, qty, vx, vy) {
    return {
      kind: 'pickup',
      x: x, y: y,
      vx: vx || 0, vy: vy || 0,
      resId: resId, qty: qty,
      life: 0, maxLife: 60,
      r: 7 + Math.min(6, qty * 0.5),
      phase: world.rng.range(0, TAU),
      dragged: false
    };
  }

  /* ------------------------------------------------------------------ */
  /*  PROJECTILES                                                       */
  /* ------------------------------------------------------------------ */
  var PROJ_STYLE = {
    bolt: { r: 3.4, len: 16, color: '#9ff0ff', core: '#ffffff' },
    slug: { r: 4.2, len: 11, color: '#ffab5e', core: '#fff0d0' },
    orb: { r: 8.5, len: 8.5, color: '#ffd166', core: '#fff6d8' },
    plasma: { r: 5, len: 20, color: '#ff5cc8', core: '#ffd6f4' },
    void: { r: 6.5, len: 14, color: '#b07dff', core: '#efe4ff' },
    player: { r: 3.6, len: 18, color: '#7dffe8', core: '#ffffff' }
  };

  function makeProjectile(world, o) {
    var st = PROJ_STYLE[o.type] || PROJ_STYLE.bolt;
    return {
      kind: 'projectile',
      x: o.x, y: o.y,
      vx: o.vx, vy: o.vy,
      dmg: o.dmg,
      team: o.team,          // 'player' | 'enemy'
      type: o.type,
      life: o.life || 2.4,
      maxLife: o.life || 2.4,
      r: o.r || st.r,
      style: st,
      angle: Math.atan2(o.vy, o.vx),
      wobble: world.rng.range(0, TAU)
    };
  }

  /* ------------------------------------------------------------------ */
  /*  ENEMIES                                                           */
  /* ------------------------------------------------------------------ */
  function makeEnemy(world, typeId, x, y) {
    var def = SM.Enemies.get(typeId);
    var tier = 1 + (world.sector.index * 0.16);
    return {
      kind: 'enemy',
      def: def, type: typeId,
      x: x, y: y,
      vx: 0, vy: 0,
      angle: 0,
      hp: def.hp * tier, maxHp: def.hp * tier,
      shield: def.shield * tier, maxShield: def.shield * tier,
      radius: def.radius,
      cooldown: world.rng.range(0.6, def.fireEvery + 0.4),
      state: 'idle',
      stateT: 0,
      orbitDir: world.rng.sign(),
      hitFlash: 0,
      shieldFlash: 0,
      dashCd: world.rng.range(1.5, 3.5),
      dashing: 0,
      spiral: world.rng.range(0, TAU),
      target: null,
      alive: true,
      spawnT: 0,
      deathT: 0,
      wobble: world.rng.range(0, TAU)
    };
  }

  function enemyFire(world, e) {
    var def = e.def;
    var px = world.player.x, py = world.player.y;
    var base = Math.atan2(py - e.y, px - e.x);
    var spread = 0;
    var count = 1;
    if (e.type === 'voidSentinel') { count = 3; spread = 0.24; }
    if (e.type === 'heavy') { count = 1; }
    for (var i = 0; i < count; i++) {
      var a = base + (count > 1 ? (i - (count - 1) / 2) * spread : 0) + world.rng.range(-0.045, 0.045);
      var sp = def.projSpeed;
      world.projectiles.push(makeProjectile(world, {
        x: e.x + Math.cos(a) * (def.radius + 6),
        y: e.y + Math.sin(a) * (def.radius + 6),
        vx: Math.cos(a) * sp + e.vx * 0.3,
        vy: Math.sin(a) * sp + e.vy * 0.3,
        dmg: def.damage * (1 + world.sector.index * 0.08),
        team: 'enemy',
        type: def.projectile,
        life: 3.2
      }));
    }
    world.sfx('enemyShot', e.x, e.y);
    world.particles.burst(e.x, e.y, 4, { color: def.art.color, speed: 60, size: 2, life: 0.3 });
  }

  function updateEnemy(world, e, dt) {
    var def = e.def;
    var p = world.player;
    e.stateT += dt;
    e.spawnT += dt;
    e.hitFlash = Math.max(0, e.hitFlash - dt * 3.6);
    e.shieldFlash = Math.max(0, e.shieldFlash - dt * 3.2);
    e.wobble += dt * 2;

    var dx = p.x - e.x, dy = p.y - e.y;
    var d = Math.hypot(dx, dy) || 1;
    var nx = dx / d, ny = dy / d;
    var desiredX = 0, desiredY = 0;
    var accel = def.accel;

    if (p.dead) { desiredX = -nx; desiredY = -ny; accel *= 0.4; }

    switch (def.ai) {
      case 'orbit': {
        var want = def.orbitDist;
        var radial = (d - want) / want;
        desiredX = nx * radial * 1.4 + (-ny) * e.orbitDir * 0.85;
        desiredY = ny * radial * 1.4 + (nx) * e.orbitDir * 0.85;
        break;
      }
      case 'strafe': {
        var want2 = def.orbitDist;
        var radial2 = U.clamp((d - want2) / 180, -1, 1);
        desiredX = nx * radial2 * 1.5 + (-ny) * e.orbitDir;
        desiredY = ny * radial2 * 1.5 + (nx) * e.orbitDir;
        if (e.stateT > 3.2) { e.orbitDir *= -1; e.stateT = 0; }
        break;
      }
      case 'siege': {
        var want3 = def.orbitDist;
        var radial3 = U.clamp((d - want3) / 240, -0.9, 1);
        desiredX = nx * radial3;
        desiredY = ny * radial3;
        break;
      }
      case 'dash': {
        e.dashCd -= dt;
        if (e.dashing > 0) {
          e.dashing -= dt;
          desiredX = nx * 1.9; desiredY = ny * 1.9;
          accel *= 3.4;
          if (world.rng.chance(0.35)) world.particles.add({
            x: e.x, y: e.y, vx: -e.vx * 0.1, vy: -e.vy * 0.1,
            life: 0.5, maxLife: 0.5, size: 2.4, color: def.art.color, type: 'glow'
          });
        } else {
          var want4 = def.orbitDist;
          var radial4 = U.clamp((d - want4) / 150, -1, 1);
          desiredX = nx * radial4 * 1.2 + (-ny) * e.orbitDir * 0.7;
          desiredY = ny * radial4 * 1.2 + (nx) * e.orbitDir * 0.7;
          if (e.dashCd <= 0 && d < 520 && d > 90) {
            e.dashing = 0.5;
            e.dashCd = world.rng.range(2.6, 4.4);
            e.angle = Math.atan2(dy, dx);
            world.sfx('dash', e.x, e.y);
          }
        }
        break;
      }
      case 'sentinel': {
        var orbitR = def.orbitDist + Math.sin(e.spawnT * 0.6) * 60;
        var radial5 = U.clamp((d - orbitR) / 200, -1, 1);
        desiredX = nx * radial5 * 0.9 + (-ny) * 0.75;
        desiredY = ny * radial5 * 0.9 + (nx) * 0.75;
        // teleport blink when hurt
        if (e.hp < e.maxHp * 0.4 && e.stateT > 4 && world.rng.chance(dt * 0.7)) {
          var ang = world.rng.range(0, TAU);
          var rad = world.rng.range(240, 420);
          world.particles.burst(e.x, e.y, 18, { color: '#b07dff', speed: 180, size: 3, life: 0.6 });
          e.x = U.clamp(p.x + Math.cos(ang) * rad, 80, SM.Sectors.WORLD_W - 80);
          e.y = U.clamp(p.y + Math.sin(ang) * rad, 80, SM.Sectors.WORLD_H - 80);
          world.particles.burst(e.x, e.y, 18, { color: '#d9baff', speed: 200, size: 3, life: 0.6 });
          e.stateT = 0;
          e.shield = Math.min(e.maxShield, e.shield + e.maxShield * 0.15);
          world.sfx('teleport', e.x, e.y);
        }
        break;
      }
    }

    // separation from other enemies
    for (var i = 0; i < world.enemies.length; i++) {
      var o = world.enemies[i];
      if (o === e) continue;
      var ox = e.x - o.x, oy = e.y - o.y;
      var od = Math.hypot(ox, oy);
      if (od > 0.01 && od < e.radius + o.radius + 26) {
        desiredX += (ox / od) * 1.2;
        desiredY += (oy / od) * 1.2;
      }
    }

    var dl = Math.hypot(desiredX, desiredY);
    if (dl > 0.001) { desiredX /= dl; desiredY /= dl; }

    e.vx += desiredX * accel * dt;
    e.vy += desiredY * accel * dt;
    var drag = Math.pow(0.16, dt);
    e.vx *= drag; e.vy *= drag;
    var speed = Math.hypot(e.vx, e.vy);
    var maxS = def.speed * (e.dashing > 0 ? 1.7 : 1);
    if (speed > maxS) { e.vx = e.vx / speed * maxS; e.vy = e.vy / speed * maxS; }
    e.x += e.vx * dt;
    e.y += e.vy * dt;

    var pad = 60;
    if (e.x < pad) { e.x = pad; e.vx = Math.abs(e.vx) * 0.4; }
    if (e.y < pad) { e.y = pad; e.vy = Math.abs(e.vy) * 0.4; }
    if (e.x > SM.Sectors.WORLD_W - pad) { e.x = SM.Sectors.WORLD_W - pad; e.vx = -Math.abs(e.vx) * 0.4; }
    if (e.y > SM.Sectors.WORLD_H - pad) { e.y = SM.Sectors.WORLD_H - pad; e.vy = -Math.abs(e.vy) * 0.4; }

    // facing
    var faceTarget = e.dashing > 0 ? Math.atan2(e.vy, e.vx) : Math.atan2(dy, dx);
    e.angle = U.angleLerp(e.angle, faceTarget, 1 - Math.pow(0.001, dt));

    // engine trail
    if (world.rng.chance(dt * 14)) {
      world.particles.add({
        x: e.x - Math.cos(e.angle) * e.radius * 0.8,
        y: e.y - Math.sin(e.angle) * e.radius * 0.8,
        vx: -Math.cos(e.angle) * 40 + world.rng.range(-16, 16),
        vy: -Math.sin(e.angle) * 40 + world.rng.range(-16, 16),
        life: 0.35, maxLife: 0.35, size: 2.2,
        color: def.art.color, type: 'glow'
      });
    }

    // shooting
    e.cooldown -= dt;
    if (e.cooldown <= 0 && !p.dead && d < def.range * (1 + world.sector.index * 0.04) && e.spawnT > 1.1) {
      enemyFire(world, e);
      e.cooldown = def.fireEvery * world.rng.range(0.85, 1.2);
    }
  }

  /* ------------------------------------------------------------------ */
  /*  DRONES                                                            */
  /* ------------------------------------------------------------------ */
  function makeDrone(world, index, serial) {
    var p = world.player;
    var ang = (index / Math.max(1, world.droneCount)) * TAU;
    return {
      kind: 'drone',
      x: p.x + Math.cos(ang) * 46,
      y: p.y + Math.sin(ang) * 46,
      vx: 0, vy: 0,
      angle: 0,
      serial: serial || (index + 1),
      state: 'follow',      // follow | seek | mine | return | unload
      target: null,
      cargo: {},
      cargoUsed: 0,
      cooldown: 0,
      beam: null,
      hitFlash: 0,
      slot: index,
      minedTotal: 0,
      orbitPhase: ang,
      thrust: 0
    };
  }

  function droneCapacity(world) {
    return Math.round(world.stats.droneCargo);
  }

  function updateDrone(world, dr, dt) {
    var p = world.player;
    var stats = world.stats;
    var cap = droneCapacity(world);
    var speed = 190 + stats.dronePower * 22;

    dr.beam = null;
    dr.hitFlash = Math.max(0, dr.hitFlash - dt * 3);
    if (dr.target && dr.target.dead) {
      dr.target = null;
      if (dr.state === 'seek' || dr.state === 'mine') dr.state = 'follow';
    }
    if (!world.droneAuto || p.dead) {
      if (dr.state !== 'return') { dr.state = 'follow'; dr.target = null; }
    }

    // acquire work
    if (world.droneAuto && !p.dead && (dr.state === 'follow' || dr.state === 'seek') && !dr.target) {
      dr.target = world.findDroneTarget(dr.x, dr.y, 1350);
      if (dr.target) dr.state = 'seek';
    }

    var tx = null, ty = null;

    if (dr.state === 'seek' && dr.target) {
      var a = dr.target;
      var d = Math.hypot(a.x - dr.x, a.y - dr.y);
      if (d < a.radius + 96) dr.state = 'mine';
      else { tx = a.x; ty = a.y; }
    }

    if (dr.state === 'mine') {
      var a2 = dr.target;
      if (!a2 || a2.dead) {
        dr.state = 'follow'; dr.target = null;
      } else {
        var d2 = Math.hypot(a2.x - dr.x, a2.y - dr.y);
        var ring = a2.radius + 62;
        var ang = Math.atan2(dr.y - a2.y, dr.x - a2.x);
        tx = a2.x + Math.cos(ang) * ring;
        ty = a2.y + Math.sin(ang) * ring;
        if (d2 < a2.radius + 140) {
          dr.vx *= Math.pow(0.02, dt);
          dr.vy *= Math.pow(0.02, dt);
          dr.angle = U.angleLerp(dr.angle, Math.atan2(a2.y - dr.y, a2.x - dr.x), 1 - Math.pow(0.001, dt));
          dr.beam = { x: a2.x, y: a2.y, target: a2 };
          dr.thrust = 0.25;
          world.mineAsteroid(a2, dt * 7.6 * stats.dronePower, 'drone', dr);
          if (dr.cargoUsed >= cap * 0.9) { dr.state = 'return'; dr.target = null; }
          dr.x += dr.vx * dt;
          dr.y += dr.vy * dt;
          return;
        }
      }
    }

    if (dr.state === 'return') {
      tx = p.x; ty = p.y;
      var dp = Math.hypot(p.x - dr.x, p.y - dr.y);
      if (dp < 52) {
        var total = 0;
        for (var rid in dr.cargo) {
          var q = dr.cargo[rid];
          var added = SM.Profile.addResource(world.profile, rid, q);
          total += added;
          if (added < q) dr.cargo[rid] = q - added; else delete dr.cargo[rid];
        }
        checkDroneReturn(world, dr, cap);
        if (total > 0) {
          world.profile.counters.droneMined += total;
          if (world.profile.drones[dr.slot]) world.profile.drones[dr.slot].mined += total;
          dr.minedTotal += total;
          world.particles.burst(p.x, p.y, 9, { color: '#9ff0ff', speed: 80, size: 1.8, life: 0.45 });
          world.notify('DRONE #' + dr.serial + ' TRANSFERRED ' + total + ' UNIT' + (total > 1 ? 'S' : ''), 'good');
          world.sfx('unload', p.x, p.y);
        }
        dr.cargo = {};
        dr.cargoUsed = 0;
        dr.state = 'follow';
        dr.target = null;
        tx = p.x + Math.cos(dr.slot * 2.1) * 64;
        ty = p.y + Math.sin(dr.slot * 2.1) * 64;
      }
    } else if (dr.state === 'follow' && tx === null) {
      dr.orbitPhase += dt * 0.8;
      tx = p.x + Math.cos(dr.orbitPhase + dr.slot * 2.1) * (64 + dr.slot * 8) + p.vx * 0.14;
      ty = p.y + Math.sin(dr.orbitPhase + dr.slot * 2.1) * (64 + dr.slot * 8) + p.vy * 0.14;
    }

    if (tx !== null) {
      var dx = tx - dr.x, dy = ty - dr.y;
      var dd = Math.hypot(dx, dy) || 1;
      var desiredSpeed = dd > 60 ? speed : dd * 2.6;
      dr.vx += (dx / dd) * Math.min(desiredSpeed, speed) * 3.4 * dt;
      dr.vy += (dy / dd) * Math.min(desiredSpeed, speed) * 3.4 * dt;
      var drag2 = Math.pow(0.03, dt);
      dr.vx *= drag2; dr.vy *= drag2;
      var sp = Math.hypot(dr.vx, dr.vy);
      if (sp > speed) { dr.vx = dr.vx / sp * speed; dr.vy = dr.vy / sp * speed; }
      dr.thrust = U.clamp(sp / speed, 0, 1);
      if (sp > 12) dr.angle = U.angleLerp(dr.angle, Math.atan2(dy, dx), 1 - Math.pow(0.002, dt));
    }

    dr.x += dr.vx * dt;
    dr.y += dr.vy * dt;
    if (world.rng.chance(dt * 7 * (0.25 + dr.thrust))) {
      world.particles.add({
        x: dr.x - Math.cos(dr.angle) * 12,
        y: dr.y - Math.sin(dr.angle) * 12,
        vx: -Math.cos(dr.angle) * 30 + world.rng.range(-10, 10),
        vy: -Math.sin(dr.angle) * 30 + world.rng.range(-10, 10),
        life: 0.3, maxLife: 0.3, size: 1.8, color: '#8ff5e6', type: 'glow'
      });
    }
    checkDroneReturn(world, dr, cap);
  }

  function checkDroneReturn(world, dr, cap) {
    var used = 0;
    for (var rid in dr.cargo) used += dr.cargo[rid] * (SM.Resources.get(rid) || { mass: 1 }).mass;
    dr.cargoUsed = used;
    if (used >= cap * 0.92 && dr.state !== 'return') {
      dr.state = 'return';
      dr.target = null;
    }
  }

  function addDroneCargo(world, dr, resId, qty) {
    dr.cargo[resId] = (dr.cargo[resId] | 0) + qty;
    checkDroneReturn(world, dr, droneCapacity(world));
  }

  SM.Ent = {
    makeAsteroid: makeAsteroid,
    asteroidPenetrationStat: asteroidPenetrationStat,
    makePickup: makePickup,
    makeProjectile: makeProjectile,
    makeEnemy: makeEnemy,
    updateEnemy: updateEnemy,
    enemyFire: enemyFire,
    makeDrone: makeDrone,
    updateDrone: updateDrone,
    addDroneCargo: addDroneCargo,
    droneCapacity: droneCapacity,
    PROJ_STYLE: PROJ_STYLE
  };
})(typeof window !== 'undefined' ? window : globalThis);
