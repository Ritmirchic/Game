/* =========================================================================
   SPACE MINER — achievements (all wired to real save-state counters)
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  /* check(state) receives the profile; `counters` holds lifetime totals. */
  var LIST = [
    { id: 'first_ore', name: 'First Contact', desc: 'Mine your first asteroid.', reward: 100,
      test: function (p) { return p.counters.asteroidsMined >= 1; } },
    { id: 'rock_50', name: 'Rock Collector', desc: 'Destroy 50 asteroids.', reward: 600,
      test: function (p) { return p.counters.asteroidsMined >= 50; } },
    { id: 'rock_500', name: 'Belt Devourer', desc: 'Destroy 500 asteroids.', reward: 4000,
      test: function (p) { return p.counters.asteroidsMined >= 500; } },
    { id: 'credits_10k', name: 'Solvent', desc: 'Earn 10,000 credits in total.', reward: 500,
      test: function (p) { return p.counters.creditsEarned >= 10000; } },
    { id: 'credits_250k', name: 'Belt Baron', desc: 'Earn 250,000 credits in total.', reward: 6000,
      test: function (p) { return p.counters.creditsEarned >= 250000; } },
    { id: 'drone_first', name: 'Autonomy', desc: 'Deploy your first mining drone.', reward: 800,
      test: function (p) { return p.counters.droneMined >= 1; } },
    { id: 'drone_200', name: 'Foreman', desc: 'Let drones mine 200 units of ore.', reward: 3000,
      test: function (p) { return p.counters.droneMined >= 200; } },
    { id: 'kill_10', name: 'Not Defenceless', desc: 'Destroy 10 hostile craft.', reward: 700,
      test: function (p) { return p.counters.kills >= 10; } },
    { id: 'kill_100', name: 'Graveyard Shift', desc: 'Destroy 100 hostile craft.', reward: 5000,
      test: function (p) { return p.counters.kills >= 100; } },
    { id: 'sector_3', name: 'Derelict Diver', desc: 'Unlock the Iron Graveyard.', reward: 2500,
      test: function (p) { return p.sectors.indexOf('graveyard') >= 0; } },
    { id: 'sector_5', name: 'Edge of Nothing', desc: 'Reach the Void Frontier.', reward: 12000,
      test: function (p) { return p.sectors.indexOf('void') >= 0; } },
    { id: 'rare', name: 'Deep Seam', desc: 'Recover a Void Crystal.', reward: 3000,
      test: function (p) { return (p.counters.minedByType.void || 0) >= 1; } },
    { id: 'neutron', name: 'Star Metal', desc: 'Recover Neutronium.', reward: 8000,
      test: function (p) { return (p.counters.minedByType.neutronium || 0) >= 1; } },
    { id: 'rich', name: 'Upgrade Addiction', desc: 'Purchase 30 upgrade levels.', reward: 2500,
      test: function (p) { return p.counters.upgradesBought >= 30; } },
    { id: 'hull_5', name: 'Different Ship', desc: 'Buy a second hull class.', reward: 4000,
      test: function (p) { return p.ships.length >= 2; } },
    { id: 'full_hold', name: 'Brimming', desc: 'Fill your cargo hold completely.', reward: 400,
      test: function (p) { return p.counters.fullHolds >= 1; } },
    { id: 'survivor', name: 'Still Flying', desc: 'Survive 10 minutes of flight time.', reward: 1500,
      test: function (p) { return p.counters.playTime >= 600; } },
    { id: 'autolord', name: 'Fully Automated', desc: 'Field five drones at once.', reward: 9000,
      test: function (p) { return p.counters.maxDrones >= 5; } }
  ];

  var BY_ID = {};
  LIST.forEach(function (a) { BY_ID[a.id] = a; });
  SM.Achievements = { list: LIST, byId: BY_ID, get: function (id) { return BY_ID[id]; } };
})(typeof window !== 'undefined' ? window : globalThis);
