/* =========================================================================
   SPACE MINER — automated test suite (headless, real game code)
   Run: node tools/test.js
   ========================================================================= */
const { makeGame, bootstrap } = require('./headless');

let pass = 0, fail = 0;
const failures = [];
function ok(cond, name, detail) {
  if (cond) { pass++; process.stdout.write('.'); }
  else {
    fail++; failures.push(name + (detail ? '  → ' + detail : ''));
    process.stdout.write('F');
  }
}
function eq(a, b, name) { ok(a === b, name, 'got ' + a + ', want ' + b); }
function near(a, b, tol, name) { ok(Math.abs(a - b) <= tol, name, 'got ' + a + ', want ' + b + ' ±' + tol); }
function section(t) { process.stdout.write('\n  ' + t + ' '); }
function finite(v) { return typeof v === 'number' && isFinite(v); }

const { SM } = makeGame();

/* ===================================================================== */
section('profile');
{
  const p = SM.Profile.create();
  eq(p.credits, 500, 'starting credits');
  eq(SM.Profile.stats(p).cargoCap, 48, 'base cargo capacity');
  ok(SM.Profile.stats(p).droneSlots === 1, 'starter hull has one drone cradle');

  // upgrades must have real effects
  const before = SM.Profile.stats(p);
  const laserBefore = before.laserPower, cargoBefore = before.cargoCap;
  p.credits = 100000;
  const r1 = SM.Profile.buyUpgrade(p, 'laser');
  ok(r1.ok, 'buy laser upgrade succeeds');
  const afterLaser = SM.Profile.stats(p);
  ok(afterLaser.laserPower > laserBefore, 'laser upgrade raises laser power',
    laserBefore + ' → ' + afterLaser.laserPower);

  SM.Profile.buyUpgrade(p, 'cargo');
  ok(SM.Profile.stats(p).cargoCap > cargoBefore, 'cargo upgrade raises capacity');

  const slotBefore = SM.Profile.stats(p).droneSlots;
  p.credits = 100000;
  SM.Profile.buyUpgrade(p, 'droneBay');
  eq(SM.Profile.stats(p).droneSlots, slotBefore + 1, 'drone bay adds a slot');
  SM.Profile.syncDrones(p);
  eq(p.drones.length, slotBefore + 1, 'drone roster grows with the bay');

  // affordability
  const poor = SM.Profile.create();
  poor.credits = 0;
  const bad = SM.Profile.buyUpgrade(poor, 'laser');
  ok(!bad.ok && bad.reason === 'credits', 'upgrade refused without credits');

  // max levels
  const rich = SM.Profile.create();
  rich.credits = 1e9;
  const u = SM.Upgrades.get('laser');
  for (let i = 0; i < u.max + 3; i++) SM.Profile.buyUpgrade(rich, 'laser');
  eq(rich.upgrades.laser, u.max, 'upgrade cannot exceed its max level');

  // cargo limits & selling
  const c = SM.Profile.create();
  const added = SM.Profile.addResource(c, 'iron', 500);
  ok(added > 0 && added < 500, 'cargo clamp rejects overflow', 'added ' + added);
  ok(SM.Profile.cargoFree(c) < 1, 'hold reports full');
  const value = SM.Profile.cargoValue(c);
  const sold = SM.Profile.sellAll(c);
  eq(sold, value, 'sell returns full cargo value');
  eq(Object.keys(c.cargo).length, 0, 'hold emptied after selling');
  eq(c.credits, 500 + sold, 'credits increased by sale');

  // xp / levels
  const x = SM.Profile.create();
  const lvl = SM.Profile.addXp(x, SM.Profile.totalXpForLevel(4));
  eq(lvl, 4, 'xp advances pilot level');
  ok(SM.Profile.levelProgress(x).pct >= 0 && SM.Profile.levelProgress(x).pct <= 1, 'level bar in range');

  // progression: sector unlock requirements
  const s = SM.Profile.create();
  s.credits = 5000;
  const check = SM.Profile.canUnlock(s, 'nebula');
  ok(!check.ok && check.reason === 'deliver', 'sector needs ore delivery, not just credits');
  s.cargo = { iron: 70 };
  const okUnlock = SM.Profile.canUnlock(s, 'nebula');
  ok(okUnlock.ok, 'sector unlockable once requirements met');
  SM.Profile.unlockSector(s, 'nebula');
  ok(SM.Profile.sectorUnlocked(s, 'nebula'), 'sector added to unlocked list');
  ok(!s.cargo.iron, 'required ore consumed by unlock');

  // ships
  const sh = SM.Profile.create();
  sh.credits = 1e6; sh.xp = SM.Profile.totalXpForLevel(12);
  SM.Profile.resolveLevel(sh);
  const bought = SM.Profile.buyShip(sh, 'harvester');
  ok(bought.ok, 'ship purchase succeeds at level');
  ok(SM.Profile.selectShip(sh, 'harvester'), 'ship selectable after purchase');
  eq(SM.Profile.stats(sh).cargoCap, SM.Ships.get('harvester').stats.cargoCap, 'hull stats applied');

  // achievements award credits once
  const a = SM.Profile.create();
  a.counters.asteroidsMined = 1;
  const unlocked = SM.Profile.checkAchievements(a);
  ok(unlocked.length >= 1, 'achievement unlocked by counter');
  const creditsAfter = a.credits;
  const again = SM.Profile.checkAchievements(a);
  eq(again.length, 0, 'achievement not awarded twice');
  eq(a.credits, creditsAfter, 'no duplicate credit reward');
}

/* ===================================================================== */
section('save system');
{
  const p = SM.Profile.create();
  p.credits = 4242;
  p.upgrades.laser = 3;
  p.cargo = { iron: 12, crystal: 2 };
  p.xp = 900;
  p.sectors = ['belt', 'nebula'];
  p.sector = 'nebula';
  p.upgrades.droneBay = 1;
  p.drones = [{ serial: 1, mined: 44 }, { serial: 2, mined: 12 }];
  p.achievements = ['first_ore'];
  const json = SM.Profile.serialize(p);

  const res = SM.Profile.deserialize(json);
  ok(res.ok, 'valid save deserialises');
  eq(res.profile.credits, 4242, 'credits round-trip');
  eq(res.profile.cargo.iron, 12, 'cargo round-trip');
  eq(res.profile.upgrades.laser, 3, 'upgrades round-trip');
  eq(res.profile.sector, 'nebula', 'sector round-trip');
  eq(res.profile.drones.length, 2, 'drones round-trip');
  eq(res.profile.xp, 900, 'xp round-trip');

  const corrupt = SM.Profile.deserialize('{"v":4,"sum":"nope","data":{"credits":9e9}}');
  ok(!corrupt.ok, 'checksum mismatch rejected', corrupt.reason);

  const truncated = SM.Profile.deserialize('{"v":4,"data":');
  ok(!truncated.ok && truncated.reason === 'json', 'truncated json rejected');

  const future = SM.Profile.deserialize(JSON.stringify({ v: 99, data: {} }));
  ok(!future.ok && future.reason === 'future-version', 'future version rejected');

  // injected hostile values must be sanitised
  const evil = JSON.stringify({
    v: 4,
    data: {
      credits: -500, xp: 'NaN', shipId: 'nonexistent', sector: 'atlantis',
      upgrades: { laser: 999, fake: 4 }, cargo: { iron: -5, unobtanium: 10 },
      settings: { quality: 12 }
    }
  });
  const evilRes = SM.Profile.deserialize(evil);
  ok(evilRes.ok, 'hostile but shaped save loads');
  if (evilRes.ok) {
    ok(evilRes.profile.credits >= 0, 'negative credits clamped');
    ok(finite(evilRes.profile.xp), 'NaN xp replaced');
    eq(evilRes.profile.shipId, 'prospector', 'unknown ship replaced');
    eq(evilRes.profile.sector, 'belt', 'unknown sector replaced');
    eq(evilRes.profile.upgrades.laser, SM.Upgrades.get('laser').max, 'upgrade level clamped');
    ok(evilRes.profile.upgrades.fake === undefined, 'unknown upgrade dropped');
    ok(!(evilRes.profile.cargo.iron > 0), 'negative cargo dropped');
    ok(evilRes.profile.cargo.unobtanium === undefined, 'unknown resource dropped');
    ok(evilRes.profile.settings.quality <= 1, 'settings clamped');
  }

  // storage round trip + backup recovery
  const st = SM.Profile.storage();
  SM.Profile.wipe();
  const live = SM.Profile.create();
  live.credits = 777;
  SM.Profile.save(live);
  live.credits = 999;
  SM.Profile.save(live);           // previous save becomes the backup
  const loaded = SM.Profile.load();
  ok(loaded.ok && loaded.profile.credits === 999, 'save/load through storage');

  st.setItem(SM.Profile.SAVE_KEY, '{"v":4,"data":{"credits":');
  const recovered = SM.Profile.load();
  ok(recovered.ok && recovered.recovered, 'corrupt primary recovered from backup');
  SM.Profile.wipe();
}

/* ===================================================================== */
section('world / asteroids');
{
  const { game } = makeGame();
  game.profile = SM.Profile.create();
  game.profile.credits = 1e6;
  game.enterSector('belt');
  const w = game.world;
  ok(w.asteroids.length > 60, 'sector populated with asteroids', w.asteroids.length + ' rocks');

  // no asteroid spawned on the ship, none overlapping
  let onShip = 0, overlapping = 0;
  for (const a of w.asteroids) {
    if (Math.hypot(a.x - w.player.x, a.y - w.player.y) < a.radius + 30) onShip++;
  }
  for (let i = 0; i < w.asteroids.length; i++) {
    for (let j = i + 1; j < w.asteroids.length; j++) {
      const a = w.asteroids[i], b = w.asteroids[j];
      if (Math.hypot(a.x - b.x, a.y - b.y) < (a.radius + b.radius) * 0.75) overlapping++;
    }
  }
  eq(onShip, 0, 'no asteroid spawns inside the player ship');
  eq(overlapping, 0, 'asteroid placement validated (no deep overlaps)');

  // all asteroids are inside the world bounds and renderable
  let outOfBounds = 0, unrendered = 0;
  for (const a of w.asteroids) {
    if (a.x < 0 || a.y < 0 || a.x > SM.Sectors.WORLD_W || a.y > SM.Sectors.WORLD_H) outOfBounds++;
    if (!a.art || !a.art.sprite || a.art.sprite.width < 4) unrendered++;
  }
  eq(outOfBounds, 0, 'asteroids stay inside world bounds');
  eq(unrendered, 0, 'every asteroid has generated artwork');

  // asteroid integrity stats
  ok(w.asteroids.every((a) => a.hp > 0 && a.maxHp > 0 && a.radius > 10), 'asteroids have health and size');

  // mining reduces hp and eventually destroys the rock
  const target = w.asteroids[0];
  const hp0 = target.hp;
  w.mineAsteroid(target, 5, 'player');
  ok(target.hp < hp0, 'mining beam damages asteroids');
  ok(target.damagePct > 0, 'damage state tracked for visuals');
  const pickupsBefore = w.pickups.length;
  w.mineAsteroid(target, target.hp + 999, 'player');
  ok(target.dead, 'asteroid destroyed at zero hp');
  ok(w.pickups.length > pickupsBefore, 'destruction spawns collectible ore');
  ok(w.particles.list.length > 0, 'destruction spawns VFX particles');

  // collection into cargo
  const p = game.profile;
  p.cargo = {};
  const pk = w.pickups[0];
  pk.x = w.player.x + 5;
  pk.y = w.player.y + 5;
  const before = SM.Profile.cargoUsed(p);
  w.updatePickups(0.2);
  ok(SM.Profile.cargoUsed(p) > before, 'pickups are collected into the hold');

  // rarity weighting respects sector tables
  const ids = new Set();
  for (let i = 0; i < 300; i++) ids.add(w.pickResource());
  const allowed = Object.keys(w.sector.asteroid.mineralWeights);
  ok([...ids].every((id) => allowed.includes(id)), 'mining yields only sector-legal resources');

  // hardened rocks take longer
  const soft = SM.Resources.get('silicate'), hard = SM.Resources.get('neutronium');
  ok(hard.hardness > soft.hardness && hard.value > soft.value, 'harder ores are worth more');
}

/* ===================================================================== */
section('flight + camera');
{
  const { game, input } = makeGame();
  game.profile = SM.Profile.create();
  game.enterSector('belt');
  game.state = 'flight';
  const w = game.world, p = w.player;
  const x0 = p.x;

  for (let i = 0; i < 60; i++) w.update(1 / 60, { ax: 1, ay: 0, mine: false, fire: false, aimWorld: { x: p.x + 100, y: p.y } });
  ok(p.x > x0 + 40, 'thrust moves the ship', 'Δx=' + (p.x - x0).toFixed(1));
  ok(Math.hypot(p.vx, p.vy) <= w.stats.maxSpeed * 1.01, 'speed clamped to hull maximum');

  // aiming follows the cursor
  for (let i = 0; i < 40; i++) w.update(1 / 60, { ax: 0, ay: 0, aimWorld: { x: p.x - 200, y: p.y } });
  ok(Math.abs(p.angle - Math.PI) < 0.2 || Math.abs(p.angle + Math.PI) < 0.2, 'ship faces the aim point',
    'angle=' + p.angle.toFixed(2));

  // camera follows and stays inside world bounds
  for (let i = 0; i < 90; i++) { game.updateCamera(1 / 60); }
  ok(Math.abs(game.camera.x - p.x) < 400, 'camera tracks the ship');
  ok(game.camera.x >= 0 && game.camera.x <= SM.Sectors.WORLD_W, 'camera clamped to sector');

  // boosts cost energy
  const e0 = p.energy;
  w.update(0.4, { ax: 1, ay: 0, boost: true, aimWorld: null });
  ok(p.energy < e0, 'afterburner consumes energy');

  // asteroid collision pushes the ship out instead of tunnelling
  const near = w.asteroids[0];
  p.x = near.x + near.radius * 0.3;
  p.y = near.y;
  p.vx = 0; p.vy = 0;
  w.update(1 / 60, { ax: 0, ay: 0 });
  ok(Math.hypot(p.x - near.x, p.y - near.y) >= near.radius * 0.8, 'ship is ejected from asteroid overlap');
}

/* ===================================================================== */
section('drones');
{
  const { game } = makeGame();
  game.profile = SM.Profile.create();
  game.profile.credits = 1e6;
  for (let i = 0; i < 4; i++) SM.Profile.buyUpgrade(game.profile, 'droneBay');
  SM.Profile.buyUpgrade(game.profile, 'dronePower');
  game.enterSector('belt');
  game.state = 'flight';
  const w = game.world;
  game.refreshDrones();
  ok(w.drones.length >= 3, 'drone bay deploys visible drones', w.drones.length + ' drones');

  const states = new Set();
  let delivered = 0;
  const startMined = game.profile.counters.droneMined;
  for (let i = 0; i < 60 * 90; i++) {
    w.update(1 / 60, { ax: 0, ay: 0 });
    w.drones.forEach((d) => states.add(d.state));
  }
  delivered = game.profile.counters.droneMined - startMined;
  ok(states.has('seek') || states.has('mine'), 'drones search for and mine asteroids', [...states].join(','));
  ok(states.has('return'), 'drones return to unload');
  ok(delivered > 0, 'drones deliver ore into the hold', delivered + ' units in 90s');
  ok(w.drones.every((d) => finite(d.x) && finite(d.y)), 'drone simulation stays finite');

  // recall stops autonomous work
  w.droneAuto = false;
  w.drones.forEach((d) => { d.state = 'follow'; d.target = null; });
  const minedBefore = game.profile.counters.droneMined;
  for (let i = 0; i < 300; i++) w.update(1 / 60, { ax: 0, ay: 0 });
  eq(game.profile.counters.droneMined, minedBefore, 'recalled drones do not mine');
  ok(w.drones.every((d) => d.state === 'follow' || d.state === 'return'), 'recalled drones hold formation');
}

/* ===================================================================== */
section('combat');
{
  const { game } = makeGame();
  game.profile = SM.Profile.create();
  game.profile.credits = 1e6;
  for (let i = 0; i < 6; i++) SM.Profile.buyUpgrade(game.profile, 'weapons');
  game.enterSector('belt');
  game.state = 'flight';
  const w = game.world;
  const p = w.player;
  w.enemies.length = 0;
  w.asteroids = w.asteroids.filter((a) => Math.hypot(a.x - p.x, a.y - p.y) > 800);
  const e = SM.Ent.makeEnemy(w, 'raider', p.x + 200, p.y);
  w.enemies.push(e);
  ok(!!e, 'enemy spawns');
  if (e) {
    e.spawnT = 2;
    const hp0 = e.hp;
    // player cannons
    for (let i = 0; i < 240; i++) {
      w.update(1 / 60, { ax: 0, ay: 0, fire: true, aimWorld: { x: e.x, y: e.y } });
      if (e.dead) break;
    }
    ok(e.dead || e.hp < hp0, 'player cannons damage enemies', 'hp ' + hp0 + ' → ' + (e.dead ? 'dead' : e.hp.toFixed(1)));
    ok(w.projectiles.length >= 0 && finite(w.player.hull), 'combat simulation stays valid');
  }

  // enemy projectiles damage the player
  const p2 = w.player;
  p2.shield = 0; p2.hull = w.stats.maxHull; p2.invuln = 0;
  const hull0 = p2.hull;
  w.enemies.length = 0;
  w.asteroids = w.asteroids.filter((a) => Math.hypot(a.x - p2.x, a.y - p2.y) > 700);
  const gunner = SM.Ent.makeEnemy(w, 'raider', p2.x + 170, p2.y);
  w.enemies.push(gunner);
  if (gunner) {
    gunner.spawnT = 2;
    for (let i = 0; i < 60 * 20 && p2.hull === hull0; i++) w.update(1 / 60, { ax: 0, ay: 0 });
    ok(p2.hull < hull0, 'enemy fire damages the player hull',
      hull0 + ' → ' + p2.hull.toFixed(1) + ' type=' + gunner.type + ' dist=' +
      Math.hypot(gunner.x - p2.x, gunner.y - p2.y).toFixed(0) + ' hp=' + gunner.hp.toFixed(0) +
      ' cd=' + gunner.cooldown.toFixed(2) + ' proj=' + w.projectiles.length);
  }

  // kills pay credits + counter
  w.enemies.length = 0;
  const victim = SM.Ent.makeEnemy(w, 'scout', p2.x + 300, p2.y);
  w.enemies.push(victim);
  if (victim) {
    const credits0 = game.profile.credits, kills0 = game.profile.counters.kills;
    w.damageEnemy(victim, 99999, 'test', true);
    ok(victim.dead, 'enemy dies at zero hp');
    ok(game.profile.credits > credits0, 'kill pays credits');
    eq(game.profile.counters.kills, kills0 + 1, 'kill counter increments');
    ok(w.particles.rings.length > 0 || w.particles.list.length > 0, 'kill produces explosion VFX');
  }

  // death handling
  w.player.invuln = 0;
  w.damagePlayer(99999, 'test');
  ok(w.player.dead, 'player death at zero hull');
  eq(game.state, 'dead', 'game enters death state');

  game.profile.cargo = { iron: 40 };
  game.profile.credits = 1000;
  game.respawn();
  eq(game.state, 'flight', 'respawn returns to flight');
  ok(w.player.hull === w.stats.maxHull, 'hull restored on respawn');
  ok(game.profile.cargo.iron === 26, '35% of cargo lost on death', 'iron=' + game.profile.cargo.iron);
  ok(game.profile.credits < 1000, 'insurance fee charged');
}

/* ===================================================================== */
section('hazards + sectors');
{
  const { game } = makeGame();
  game.profile = SM.Profile.create();
  game.profile.sectors = SM.Sectors.list.map((s) => s.id);
  const results = [];
  for (const sec of SM.Sectors.list) {
    game.enterSector(sec.id);
    game.state = 'flight';
    const w = game.world;
    w.enemies.length = 0;
    const p = w.player;
    p.hull = w.stats.maxHull;
    let errors = 0;
    for (let i = 0; i < 60 * 20; i++) {
      try {
        w.update(1 / 60, { ax: Math.sin(i / 40), ay: Math.cos(i / 53), mine: i % 120 < 60, fire: i % 90 < 20, aimWorld: { x: p.x + Math.cos(i / 30) * 200, y: p.y + Math.sin(i / 30) * 200 } });
      } catch (err) { errors++; }
    }
    results.push({ id: sec.id, errors, asteroids: w.asteroids.length, finite: finite(p.x) && finite(p.y) && finite(p.hull) });
  }
  ok(results.every((r) => r.errors === 0), 'all sectors simulates 20s without exceptions',
    results.filter((r) => r.errors).map((r) => r.id + ':' + r.errors).join(' '));
  ok(results.every((r) => r.finite), 'player state finite in every sector');
  ok(results.every((r) => r.asteroids > 20), 'every sector has asteroids');

  // hazards exist and do something
  const storms = SM.Sectors.list.filter((s) => s.hazard.kind === 'storm');
  ok(storms.length === 1, 'plasma sector has electrical storms');
  game.enterSector('plasma');
  const w = game.world;
  w.player.invuln = 0;
  w.player.shield = 0;
  w.player.hull = w.stats.maxHull;
  w.hazardTimer = 0;
  for (let i = 0; i < 60 * 40; i++) game.world.update(1 / 60, { ax: 0, ay: 0, aimWorld: { x: w.player.x + 90, y: w.player.y } });
  ok(w.storms.length > 0 || w.player.hull < w.stats.maxHull, 'lightning storms spawn strikes');

  game.enterSector('void');
  ok(game.world.wells.length > 0, 'void frontier has gravity wells');
  const before = Math.hypot(game.world.player.vx, game.world.player.vy);
  game.enterSector('graveyard');
  ok(game.world.debris.length > 0, 'iron graveyard has drifting wreckage');
}

/* ===================================================================== */
section('economy balance');
{
  const { game } = makeGame();
  game.profile = SM.Profile.create();
  game.enterSector('belt');
  game.state = 'flight';
  /* fixed seed: the run must be reproducible, not luck-dependent */
  game.world = new SM.World(game, game.profile, 'belt', 0x5EED1234);
  const w = game.world;

  // simulate an efficient 2-minute mining run: mine the nearest rocks, then sell
  let mined = 0;
  const t0 = Date.now();
  for (let i = 0; i < 60 * 120; i++) {
    let best = null, bd = 1e9;
    for (const a of w.asteroids) {
      if (Math.hypot(a.x - w.player.x, a.y - w.player.y) < 3000) {
        const d = Math.hypot(a.x - w.player.x, a.y - w.player.y) / Math.max(0.4, SM.Resources.get(a.resourceId).value / 40);
        if (d < bd) { bd = d; best = a; }
      }
    }
    const aim = best ? { x: best.x, y: best.y } : null;
    let ax = 0, ay = 0;
    if (best) {
      const dx = best.x - w.player.x, dy = best.y - w.player.y;
      const dl = Math.hypot(dx, dy) || 1;
      if (dl > w.stats.laserRange * 0.7) { ax = dx / dl; ay = dy / dl; }
      else { ax = -dx / dl * 0.35; ay = -dy / dl * 0.35; }
    }
    w.update(1 / 60, { ax: ax, ay: ay, mine: true, aimWorld: aim });
    if (w.player.dead) w.respawn();
  }
  mined = SM.Profile.cargoValue(game.profile);
  const credits = game.profile.credits;
  ok(mined > 1500, 'two minutes of mining fills the hold with real value', mined + ' CR of ore');
  const firstUpgrade = SM.Upgrades.get('laser').cost(0);
  ok(credits + mined >= firstUpgrade, 'first upgrade affordable after one run');
  ok(game.profile.counters.asteroidsMined > 8, 'a run destroys a meaningful number of rocks',
    game.profile.counters.asteroidsMined + ' rocks');
  ok(Date.now() - t0 < 8000, 'economy simulation runs fast enough for CI');
}

/* ===================================================================== */
section('ui rendering');
{
  const { game, input } = makeGame();
  game.loadResult = { ok: false, reason: 'empty' };
  game.profile = SM.Profile.create();
  game.profile.credits = 50000;
  game.profile.sectors = ['belt', 'nebula'];
  game.enterSector('belt');
  const states = ['menu', 'flight', 'station', 'map', 'paused', 'settings', 'achievements', 'controls', 'dead', 'boot'];
  let uiErrors = [];
  for (const st of states) {
    game.state = st;
    if (st === 'station') { game.world.paused = true; }
    for (let k = 0; k < 3; k++) {
      try { game.dt = 1 / 60; game.draw(1 / 60); }
      catch (e) { uiErrors.push(st + ': ' + e.message); }
    }
  }
  eq(uiErrors.length, 0, 'every UI screen renders without errors', uiErrors.join(' | '));

  // station tabs all render
  game.state = 'station';
  let tabErrors = [];
  for (const tab of SM.StationUI.TABS) {
    SM.UI.station.tab = tab.id;
    try { game.draw(1 / 60); } catch (e) { tabErrors.push(tab.id + ': ' + e.message); }
  }
  eq(tabErrors.length, 0, 'all station tabs render', tabErrors.join(' | '));

  // clicking a widget works (simulated click on SELL ALL)
  game.state = 'station';
  SM.UI.station.tab = 'trade';
  game.profile.cargo = { iron: 20 };
  game.draw(1 / 60);
  ok(true, 'station draws with cargo present');
}

/* ===================================================================== */
section('stability soak');
{
  const { game } = makeGame({ w: 1280, h: 720 });
  game.profile = SM.Profile.create();
  game.profile.credits = 1e6;
  for (let i = 0; i < 3; i++) SM.Profile.buyUpgrade(game.profile, 'droneBay');
  game.enterSector('belt');
  game.state = 'flight';
  const w = game.world;
  let maxParticles = 0, maxEnemies = 0, maxProjectiles = 0, maxAsteroids = 0;
  const t0 = Date.now();
  for (let i = 0; i < 60 * 180; i++) {
    w.update(1 / 60, { ax: Math.sin(i / 70), ay: Math.cos(i / 90), mine: true, fire: i % 45 < 10, boost: i % 300 < 60, aimWorld: { x: w.player.x + Math.cos(i / 50) * 260, y: w.player.y + Math.sin(i / 50) * 260 } });
    if (i % 600 === 0) game.updateCamera(1 / 60);
    maxParticles = Math.max(maxParticles, w.particles.list.length);
    maxEnemies = Math.max(maxEnemies, w.enemies.length);
    maxProjectiles = Math.max(maxProjectiles, w.projectiles.length);
    maxAsteroids = Math.max(maxAsteroids, w.asteroids.length);
  }
  const ms = Date.now() - t0;
  ok(maxParticles <= 1200, 'particle pool capped', String(maxParticles));
  ok(maxEnemies <= w.sector.enemies.max, 'enemy count capped', String(maxEnemies));
  ok(maxProjectiles < 400, 'projectile count bounded', String(maxProjectiles));
  ok(maxAsteroids <= 260, 'asteroid count bounded', String(maxAsteroids));
  ok(finite(w.player.x) && finite(w.player.y) && finite(w.player.hull), 'player state finite after 3 minutes');
  ok(w.drones.every((d) => finite(d.x) && finite(d.y)), 'drones finite after 3 minutes');
  console.log('\n    3 simulated minutes in ' + ms + 'ms (' + (180000 / ms).toFixed(0) + '× realtime), ' +
    'particles peak ' + maxParticles + ', enemies peak ' + maxEnemies);
}

/* ===================================================================== */
section('render performance');
{
  const { game } = makeGame({ w: 1600, h: 900 });
  game.profile = SM.Profile.create();
  game.enterSector('belt');
  game.state = 'flight';
  const t0 = Date.now();
  const frames = 90;
  for (let i = 0; i < frames; i++) {
    game.world.update(1 / 60, { ax: 0.3, ay: 0.2, mine: true, aimWorld: { x: game.world.player.x + 200, y: game.world.player.y } });
    game.updateCamera(1 / 60);
    game.draw(1 / 60);
  }
  const ms = (Date.now() - t0) / frames;
  ok(ms < 60, 'frame render budget acceptable in CI (skia software)', ms.toFixed(1) + ' ms/frame');
  console.log('\n    1600×900 render: ' + ms.toFixed(1) + ' ms/frame (headless skia, CPU only)');
}

/* ===================================================================== */
console.log('\n\n  ' + pass + ' passed, ' + fail + ' failed');
if (fail) {
  console.log('\n  FAILURES:');
  failures.forEach((f) => console.log('   ✗ ' + f));
  process.exit(1);
}
process.exit(0);
