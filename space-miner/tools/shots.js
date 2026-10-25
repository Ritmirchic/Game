/* Renders screenshots of every major screen with the real renderer. */
const path = require('path');
const fs = require('fs');
const { makeGame, shot } = require('./headless');

const OUT = path.join(__dirname, '..', 'shots');
fs.mkdirSync(OUT, { recursive: true });

function frames(game, n, dt = 1 / 60) {
  for (let i = 0; i < n; i++) {
    game.dt = dt;
    game.time += dt;
    game.update(dt);
    game.draw(dt);
  }
}

function main() {
  const { SM, game, input } = makeGame({ w: 1440, h: 810 });
  const results = [];
  const save = (name) => {
    const file = path.join(OUT, name + '.png');
    const bytes = shot(game, file);
    results.push({ name, bytes });
    console.log('  ✓', name + '.png', (bytes / 1024).toFixed(0) + 'KB');
  };

  // boot screen
  game.boot();
  game.bootT = 0.8;
  game.update(1 / 60);
  game.draw(1 / 60);
  save('01_boot');

  // menu
  game.loadResult = { ok: false, reason: 'empty' };
  game.state = 'menu';
  frames(game, 10);
  save('02_menu');

  // start a game with a decent profile so the HUD looks alive
  game.startNew();
  const pr = game.profile;
  pr.credits = 24800;
  pr.level = 5;
  pr.xp = SM.Profile.totalXpForLevel(5) + 220;
  pr.upgrades.laser = 4; pr.upgrades.cargo = 3; pr.upgrades.thrusters = 2;
  pr.upgrades.droneBay = 2; pr.upgrades.shield = 2; pr.upgrades.weapons = 3;
  pr.upgrades.magnet = 2; pr.upgrades.scanner = 2;
  pr.cargo = { iron: 26, silicate: 18, copper: 9, silver: 4, crystal: 3, gold: 2 };
  SM.Profile.invalidateStats();
  game.enterSector('belt');
  game.state = 'flight';
  game.world.droneAuto = true;
  game.world.player.hull = 78;
  game.world.player.shield = 42;
  game.world.player.energy = 64;
  frames(game, 40);
  save('03_flight_belt');

  // mining beam: aim at a nearby asteroid
  const w = game.world;
  let target = null, bd = 1e9;
  w.asteroids.forEach((a) => {
    const d = Math.hypot(a.x - w.player.x, a.y - w.player.y);
    if (d < bd) { bd = d; target = a; }
  });
  if (target) {
    input.aimWorld = { x: target.x, y: target.y };
    input.mouse.x = game.renderer.w / 2 + (target.x - w.player.x);
    input.mouse.y = game.renderer.h / 2 + (target.y - w.player.y);
    input.mineOn = true;
    for (let i = 0; i < 25; i++) {
      game.world.update(1 / 60, { ax: 0, ay: 0, mine: true, fire: false, aimWorld: { x: target.x, y: target.y } });
      game.updateCamera(1 / 60);
    }
    game.draw(1 / 60);
    save('04_mining');
  }
  input.mineOn = false;

  // explosion / debris cloud
  if (target && !target.dead) {
    game.world.mineAsteroid(target, target.hp + 50, 'player');
    for (let i = 0; i < 8; i++) { game.world.update(1 / 60, { mine: true, aimWorld: { x: target.x, y: target.y } }); game.updateCamera(1 / 60); }
    game.draw(1 / 60);
    save('05_asteroid_break');
  }

  // combat
  const e = game.world.spawnEnemy({ x: w.player.x + 300, y: w.player.y - 90 });
  if (e) { e.spawnT = 1; }
  game.world.spawnEnemy({ x: w.player.x - 260, y: w.player.y + 160 });
  for (let i = 0; i < 90; i++) {
    game.world.update(1 / 60, { ax: 0.2, ay: -0.1, mine: false, fire: true, aimWorld: { x: w.player.x + 300, y: w.player.y - 90 } });
    game.updateCamera(1 / 60);
  }
  game.draw(1 / 60);
  save('06_combat');

  // station
  game.dock();
  game.state = 'station';
  game.draw(1 / 60);
  save('07_station_trade');
  SM.UI.station.tab = 'upgrades';
  game.draw(1 / 60);
  save('08_station_upgrades');
  SM.UI.station.tab = 'drones';
  game.draw(1 / 60);
  save('09_station_drones');
  SM.UI.station.tab = 'shipyard';
  game.draw(1 / 60);
  save('10_station_shipyard');
  SM.UI.station.tab = 'sectors';
  game.draw(1 / 60);
  save('11_station_sectors');

  // map
  game.state = 'map';
  game.draw(1 / 60);
  save('12_sector_map');

  // settings + records
  game.settingsReturn = 'menu';
  game.state = 'settings';
  game.draw(1 / 60);
  save('13_settings');
  game.state = 'achievements';
  game.profile.achievements = ['first_ore', 'credits_10k', 'kill_10'];
  game.draw(1 / 60);
  save('14_achievements');

  // other sectors
  game.state = 'flight';
  game.profile.sectors = ['belt', 'nebula', 'graveyard', 'plasma', 'void'];
  ['nebula', 'graveyard', 'plasma', 'void'].forEach((id, i) => {
    game.enterSector(id);
    game.state = 'flight';
    frames(game, 45);
    save((15 + i) + '_sector_' + id);
  });

  // death screen
  game.world.player.hull = 0;
  game.world.killPlayer();
  game.deathT = 1;
  game.draw(1 / 60);
  save('19_death');

  console.log('\nScreenshots written to shots/');
}

main();
