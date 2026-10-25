/* =========================================================================
   SPACE MINER — game controller: boot, state machine, camera, main loop
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U;

  function Game(canvas, opts) {
    opts = opts || {};
    this.canvas = canvas;
    this.headless = !!opts.headless;
    this.renderer = new SM.Renderer(canvas);
    this.input = opts.input || (canvas ? new SM.Input(canvas) : null);
    this.audio = opts.audio || new SM.Audio();
    this.state = 'boot';
    this.prevState = 'menu';
    this.profile = null;
    this.world = null;
    this.time = 0;
    this.dt = 0;
    this.fps = 60;
    this.frameTimes = [];
    this.bootT = 0;
    this.bootLines = [];
    this.camera = { x: 0, y: 0, zoom: 1, shake: 0, shakeX: 0, shakeY: 0, parallaxX: 0, parallaxY: 0 };
    this.toasts = [];
    this.autosaveT = 0;
    this.lastError = null;
    this.uiState = {};
    this.menuT = 0;
    this.sessionStart = Date.now();
    this.lowFpsStreak = 0;
    this.showDebug = false;
    this.deathT = 0;
    this.quality = 1;
    this.settingsReturn = 'menu';
    this.loadNotice = null;
  }

  /* ---------------- boot ---------------- */
  Game.prototype.boot = function () {
    this.state = 'boot';
    this.bootT = 0;
    this.bootLines = [
      'SPACE MINER // FLIGHT SYSTEM v1.0',
      'reactor core ................ ONLINE',
      'mining arrays ............... ONLINE',
      'navigation lattice .......... ONLINE',
      'sector chart ................ SYNCED'
    ];
    var self = this;
    this.loadResult = null;
    setTimeout(function () {
      if (self.headless) return;
      try { self.loadResult = SM.Profile.load(); } catch (e) { self.loadResult = { ok: false, reason: String(e) }; }
    }, 60);
  };

  Game.prototype.resize = function () {
    if (!this.canvas || !this.canvas.clientWidth) return;
    var dpr = Math.min(root.devicePixelRatio || 1, 2);
    this.renderer.resize(this.canvas.clientWidth, this.canvas.clientHeight, dpr);
    if (this.input) {
      this.input.screenW = this.renderer.w;
      this.input.screenH = this.renderer.h;
    }
  };

  /* ---------------- profile / sessions ---------------- */
  Game.prototype.startNew = function () {
    SM.Profile.wipe();
    this.profile = SM.Profile.create();
    this.applySettings();
    this.enterSector('belt');
    this.state = 'flight';
    this.toast('WELCOME TO THE BELT — MINE, SELL, UPGRADE', 'info', 4);
  };

  Game.prototype.continueGame = function () {
    var res = this.loadResult && this.loadResult.ok ? this.loadResult : SM.Profile.load();
    if (!res || !res.ok) {
      this.startNew();
      if (res && res.reason && res.reason !== 'empty') {
        this.toast('SAVE COULD NOT BE READ — STARTING FRESH', 'warn', 5);
      }
      return;
    }
    this.profile = res.profile;
    if (res.recovered) this.toast('RECOVERED FROM BACKUP SAVE', 'warn', 4);
    this.applySettings();
    var sector = this.profile.sectors.indexOf(this.profile.sector) >= 0 ? this.profile.sector : 'belt';
    this.enterSector(sector);
    this.state = 'flight';
  };

  Game.prototype.applySettings = function () {
    if (!this.profile) return;
    this.quality = this.profile.settings.quality === undefined ? 1 : this.profile.settings.quality;
    this.audio.setSettings(this.profile.settings);
  };

  Game.prototype.enterSector = function (id) {
    var profile = this.profile;
    var sector = SM.Sectors.get(id);
    profile.sector = sector.id;
    this.world = new SM.World(this, profile, sector.id, (Date.now() ^ 0x9e3779b9) >>> 0);
    this.camera.x = this.world.player.x;
    this.camera.y = this.world.player.y;
    this.camera.shake = 0;
    if (this.audio) {
      this.audio.setSector(sector);
      this.audio.play('warp');
    }
    if (SM.UI && SM.UI.reset) SM.UI.reset();
  };

  /* ---------------- camera ---------------- */
  Game.prototype.addShake = function (amount) {
    if (this.profile && !this.profile.settings.shake) return;
    this.camera.shake = Math.min(26, this.camera.shake + amount);
  };

  Game.prototype.updateCamera = function (dt) {
    var w = this.world;
    if (!w) return;
    var p = w.player;
    var cam = this.camera;
    var leadX = U.clamp(p.vx * 0.16, -140, 140);
    var leadY = U.clamp(p.vy * 0.16, -140, 140);
    var aimOffsetX = 0, aimOffsetY = 0;
    if (this.input && this.input.aimWorld) {
      var dx = this.input.aimWorld.x - p.x, dy = this.input.aimWorld.y - p.y;
      var d = Math.hypot(dx, dy) || 1;
      var pull = Math.min(d * 0.14, 90);
      aimOffsetX = dx / d * pull;
      aimOffsetY = dy / d * pull;
    }
    var targetX = p.x + leadX + aimOffsetX;
    var targetY = p.y + leadY + aimOffsetY;
    var k = 1 - Math.pow(0.0009, dt);
    cam.x += (targetX - cam.x) * k;
    cam.y += (targetY - cam.y) * k;

    // keep the camera inside the world so the backdrop never shows a seam
    var halfW = this.renderer.w / 2 / cam.zoom;
    var halfH = this.renderer.h / 2 / cam.zoom;
    if (SM.Sectors.WORLD_W > halfW * 2) cam.x = U.clamp(cam.x, halfW, SM.Sectors.WORLD_W - halfW);
    if (SM.Sectors.WORLD_H > halfH * 2) cam.y = U.clamp(cam.y, halfH, SM.Sectors.WORLD_H - halfH);

    // shake
    cam.shake *= Math.pow(0.02, dt);
    if (cam.shake < 0.05) cam.shake = 0;
    cam.shakeX = (Math.random() - 0.5) * cam.shake;
    cam.shakeY = (Math.random() - 0.5) * cam.shake;

    if (this.input) {
      var m = this.input.mouse;
      cam.parallaxX = ((m.x / Math.max(1, this.renderer.w)) - 0.5) * 2;
      cam.parallaxY = ((m.y / Math.max(1, this.renderer.h)) - 0.5) * 2;
    }
    cam.zoom = 1.5;
  };

  /* ---------------- per-frame ---------------- */
  Game.prototype.frame = function (timestamp) {
    var now = timestamp || (root.performance ? performance.now() : Date.now());
    if (!this._lastT) this._lastT = now;
    var dt = Math.min(0.05, Math.max(0.0001, (now - this._lastT) / 1000));
    this._lastT = now;
    this.dt = dt;
    this.time += dt;

    this.frameTimes.push(dt);
    if (this.frameTimes.length > 60) this.frameTimes.shift();
    var avg = this.frameTimes.reduce(function (a, b) { return a + b; }, 0) / this.frameTimes.length;
    this.fps = 1 / Math.max(0.00001, avg);

    try {
      this.update(dt);
      this.draw(dt);
    } catch (err) {
      this.lastError = err;
      if (root.console) console.error('[SPACE MINER] frame error', err);
      this.state = 'error';
    }
    if (this.input) this.input.endFrame();
  };

  Game.prototype.update = function (dt) {
    if (!this.input) return;
    var input = this.input;
    input.sample();

    if (this.state === 'boot') {
      this.bootT += dt;
      if (this.bootT > 1.6) {
        if (!this.loadResult) this.loadResult = SM.Profile.load();
        this.state = 'menu';
        this.menuT = 0;
        if (SM.UI && SM.UI.reset) SM.UI.reset();
      }
      return;
    }

    this.menuT += dt;

    // global hotkeys
    if (input.wasPressed('Escape')) {
      this.onEscape();
    }
    if (input.wasPressed('KeyM') && (this.state === 'flight' || this.state === 'map')) {
      this.state = this.state === 'map' ? 'flight' : 'map';
      this.sfxUI('uiClick');
    }
    if (input.wasPressed('Tab') && (this.state === 'flight' || this.state === 'station')) {
      SM.UI.toggleCargo();
      this.sfxUI('uiClick');
    }

    if (this.state === 'flight' || this.state === 'map' || this.state === 'station' || this.state === 'dead') {
      var world = this.world;
      var aim = null;
      if (this.renderer) {
        var cam = this.camera;
        aim = this.renderer.screenToWorld(input.mouse.x, input.mouse.y, cam);
      }
      input.aimWorld = aim;

      if (this.state === 'flight' && !input.touchMode) {
        // mining / weapons only while flying
        if (input.wasPressed('KeyE') && world && world.canDock()) this.dock();
      }

      if (world) {
        var flying = this.state === 'flight';
        var fl = flying ? input : { ax: 0, ay: 0, mine: false, fire: false, boost: false, aimWorld: aim };
        if (this.state === 'dead') fl = { ax: 0, ay: 0, mine: false, fire: false, boost: false, aimWorld: aim };
        world.update(dt, fl);
        this.updateCamera(dt);
        if (world.player.dead && this.state === 'flight') {
          this.state = 'dead';
          this.deathT = 0;
        }
        if (this.state === 'dead') this.deathT += dt;

        // contextual hints
        this.updateHints(dt);

        // audio
        if (this.audio) {
          var throttle = flying ? Math.min(1, Math.hypot(world.player.vx, world.player.vy) / Math.max(1, world.stats.maxSpeed)) : 0;
          this.audio.update(dt, dt, throttle, !!(world.player.beam), world.player.beamHeat);
        }

        // autosave
        this.autosaveT += dt;
        if (this.profile && this.autosaveT > 30 && this.state !== 'dead') {
          this.autosaveT = 0;
          this.save();
        }
      }
    }

    for (var i = this.toasts.length - 1; i >= 0; i--) {
      this.toasts[i].life -= dt;
      if (this.toasts[i].life <= 0) this.toasts.splice(i, 1);
    }
  };

  Game.prototype.updateHints = function () {
    var w = this.world, p = w.player, pr = this.profile;
    if (!pr.settings.showHints) return;
    function once(key) { if (pr.seen[key]) return false; pr.seen[key] = 1; return true; }

    if (this.state === 'flight') {
      if (!p.dead && w.time > 1.5 && once('fly')) {
        this.toast('WASD / ARROWS — FLY     ·     MOUSE — AIM', 'tip', 5);
      }
      var nearRock = false;
      for (var i = 0; i < w.asteroids.length; i++) {
        if (U.dist(p.x, p.y, w.asteroids[i].x, w.asteroids[i].y) < 420) { nearRock = true; break; }
      }
      if (nearRock && w.time > 3 && once('mine')) {
        this.toast('HOLD LEFT MOUSE TO FIRE THE MINING BEAM', 'tip', 5);
      }
      if (pr.counters.asteroidsMined >= 2 && once('collect')) {
        this.toast('ORE IS PULLED IN AUTOMATICALLY — WATCH YOUR CARGO', 'tip', 5);
      }
      if (w.canDock() && once('dock')) {
        this.toast('PRESS E TO DOCK AT ' + w.station.name, 'tip', 6);
      }
      if (SM.Profile.cargoUsed(pr) / Math.max(1, SM.Profile.cargoCap(pr)) > 0.6 && once('cargofull')) {
        this.toast('CARGO FILLING UP — DOCK AND SELL ORE', 'warn', 5);
      }
      if (p.beam && once('beam')) {
        this.toast('MINING DRAINS ENERGY — LET IT RECHARGE OR UPGRADE THE REACTOR', 'tip', 5);
      }
      if (w.time > 25 && once('map')) {
        this.toast('PRESS M FOR THE SECTOR CHART', 'tip', 5);
      }
      if (pr.counters.asteroidsMined >= 6 && once('upgrade')) {
        this.toast('DOCK, SELL ORE AND BUY UPGRADES TO REACH NEW SECTORS', 'tip', 6);
      }
      if (pr.counters.kills > 0 && once('combat')) {
        this.toast('SPACE / RIGHT MOUSE — FIRE CANNONS', 'tip', 4);
      }
    }
  };

  Game.prototype.onEscape = function () {
    if (this.state === 'flight') { this.prevState = 'flight'; this.state = 'paused'; this.sfxUI('uiClick'); }
    else if (this.state === 'paused') { this.state = 'flight'; this.sfxUI('uiClick'); }
    else if (this.state === 'map' || this.state === 'station') { this.state = 'flight'; this.sfxUI('uiClick'); if (this.world) this.world.paused = false; }
    else if (this.state === 'settings') { this.state = this.settingsReturn; }
    else if (this.state === 'achievements') { this.state = this.settingsReturn; }
    else if (this.state === 'menu') { /* no-op */ }
    else if (this.state === 'dead') { this.state = 'flight'; }
  };

  /* ---------------- progression ---------------- */
  Game.prototype.awardXp = function (amt) {
    if (!this.profile) return;
    var lvl = SM.Profile.addXp(this.profile, amt);
    if (lvl) {
      SM.Profile.invalidateStats();
      if (this.world) this.world.stats = SM.Profile.stats(this.profile);
      this.toast('PILOT LEVEL ' + lvl + ' — ALL SYSTEMS IMPROVED', 'level', 4);
      this.sfxUI('levelUp');
      this.refreshDrones();
    }
    this.checkAchievementsNow();
  };

  Game.prototype.checkAchievementsNow = function () {
    if (!this.profile) return;
    var unlocked = SM.Profile.checkAchievements(this.profile);
    for (var i = 0; i < unlocked.length; i++) {
      this.toast('ACHIEVEMENT · ' + unlocked[i].name + '  (+' + unlocked[i].reward + ' CR)', 'achievement', 5);
      this.sfxUI('achievement');
    }
    return unlocked;
  };

  Game.prototype.refreshDrones = function () {
    if (!this.world) return;
    this.world.stats = SM.Profile.stats(this.profile);
    SM.Profile.syncDrones(this.profile);
    if (this.world.drones.length !== this.world.stats.droneSlots) {
      this.world.rebuildDrones();
    }
  };

  Game.prototype.sfxUI = function (name) {
    if (this.audio) this.audio.play(name);
  };

  Game.prototype.toast = function (text, kind, ttl) {
    this.toasts.push({ text: text, kind: kind || 'info', life: ttl || 3.4, maxLife: ttl || 3.4, born: this.time });
    if (this.toasts.length > 4) this.toasts.shift();
  };

  Game.prototype.onPlayerDeath = function () {
    this.state = 'dead';
    this.deathT = 0;
    this.checkAchievementsNow();
  };

  Game.prototype.respawn = function () {
    var pr = this.profile;
    // insurance: lose 35% of cargo and 8% of credits (minimum 0)
    var lostUnits = 0;
    for (var id in pr.cargo) {
      var q = pr.cargo[id] | 0;
      var keep = Math.floor(q * 0.65);
      lostUnits += q - keep;
      if (keep > 0) pr.cargo[id] = keep; else delete pr.cargo[id];
    }
    var fee = Math.floor(pr.credits * 0.08);
    pr.credits -= fee;
    if (pr.credits < 0) pr.credits = 0;
    if (this.world) this.world.respawn();
    this.state = 'flight';
    this.toast('RECOVERED AT STATION · -' + fee + ' CR INSURANCE' + (lostUnits ? ' · ' + lostUnits + ' UNITS LOST' : ''), 'warn', 5);
    this.save();
  };

  Game.prototype.dock = function () {
    if (!this.world) return;
    if (!this.world.canDock()) { this.toast('TOO FAR FROM THE STATION', 'warn'); return; }
    this.world.paused = true;
    this.world.player.vx = 0;
    this.world.player.vy = 0;
    this.world.player.docked = true;
    this.state = 'station';
    this.sfxUI('dock');
    if (SM.UI) SM.UI.openStation();
  };

  Game.prototype.undock = function () {
    if (!this.world) return;
    this.world.paused = false;
    this.world.player.docked = false;
    var w = this.world;
    w.player.x = w.station.x + 30;
    w.player.y = w.station.y + 190;
    w.player.vx = 0; w.player.vy = 0;
    w.player.invuln = 1.5;
    this.state = 'flight';
    this.save();
    this.sfxUI('uiClick');
  };

  Game.prototype.travelTo = function (sectorId) {
    if (!this.profile) return;
    if (this.profile.sectors.indexOf(sectorId) < 0) return;
    this.profile.sector = sectorId;
    this.enterSector(sectorId);
    this.state = 'flight';
    this.world.player.docked = false;
    this.toast('JUMP COMPLETE — ' + SM.Sectors.get(sectorId).name, 'info', 4);
    this.save();
  };

  Game.prototype.save = function () {
    if (!this.profile) return;
    var res = SM.Profile.save(this.profile);
    if (!res.ok) this.toast('SAVE FAILED: ' + (res.error || 'unknown'), 'warn', 4);
    return res;
  };

  Game.prototype.loadSettingsFromProfile = function () {
    this.applySettings();
  };

  Game.prototype.quitToMenu = function () {
    this.save();
    this.state = 'menu';
    this.world = null;
    this.menuT = 0;
    if (SM.UI && SM.UI.reset) SM.UI.reset();
    this.audio.setBeam(false, 0);
    this.audio.setEngine(0, 0);
  };

  /* ---------------- draw ---------------- */
  Game.prototype.draw = function (dt) {
    var ctx = this.renderer.ctx;
    if (this.world && (this.state === 'flight' || this.state === 'station' || this.state === 'map' ||
      this.state === 'paused' || this.state === 'dead' || this.state === 'settings' || this.state === 'achievements')) {
      this.renderer.render(this, dt);
    } else {
      this.renderer.render(this, dt);
    }
    if (SM.UI) SM.UI.draw(this, ctx, dt);
  };

  Game.prototype.startLoop = function () {
    var self = this;
    var loop = function (ts) { self.frame(ts); if (!self.headless) root.requestAnimationFrame(loop); };
    this._loop = loop;
    if (!this.headless) root.requestAnimationFrame(loop);
  };

  /* step the simulation without a renderer (tests / benchmarks) */
  Game.prototype.stepHeadless = function (dt) {
    this.dt = dt;
    this.time += dt;
    this.update(dt);
  };

  SM.Game = Game;
})(typeof window !== 'undefined' ? window : globalThis);
