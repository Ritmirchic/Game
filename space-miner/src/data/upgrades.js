/* =========================================================================
   SPACE MINER — upgrade tree.  Every level has a real, measured effect.
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  /*
    apply(stats, level) mutates the derived stat block.
    cost(level) -> credits for the next level.
  */
  function ul(cost, growth, max, apply, extra) {
    var o = {
      cost: function (lvl) { return Math.round(cost * Math.pow(growth, lvl)); },
      max: max, apply: apply
    };
    if (extra) for (var k in extra) o[k] = extra[k];
    return o;
  }

  var LIST = [
    {
      id: 'laser', name: 'Mining Laser', cat: 'MINING', icon: 'laser',
      desc: 'Focusing array output. Increases mining damage per second against all rock.',
      max: 12, baseCost: 150, growth: 1.52,
      apply: function (s, l) { s.laserPower *= 1 + 0.19 * l; }
    },
    {
      id: 'focus', name: 'Beam Focus', cat: 'MINING', icon: 'focus',
      desc: 'Narrow-band emitter. Penetrates hard minerals faster and extends beam reach.',
      max: 10, baseCost: 240, growth: 1.55,
      apply: function (s, l) { s.penetration += 0.14 * l; s.laserRange *= 1 + 0.045 * l; }
    },
    {
      id: 'cargo', name: 'Cargo Bay', cat: 'SHIP', icon: 'cargo',
      desc: 'Expanded pressurised hold. More ore per trip, fewer trips.',
      max: 14, baseCost: 180, growth: 1.46,
      apply: function (s, l) { s.cargoCap += 18 * l; }
    },
    {
      id: 'compression', name: 'Ore Compactor', cat: 'SHIP', icon: 'compress',
      desc: 'Hydraulic compaction reduces the cargo space each item occupies.',
      max: 6, baseCost: 900, growth: 1.7,
      apply: function (s, l) { s.massFactor *= Math.pow(0.86, l); }
    },
    {
      id: 'thrusters', name: 'Ion Thrusters', cat: 'ENGINE', icon: 'thruster',
      desc: 'Top speed and drift control. Escape is a valid mining strategy.',
      max: 12, baseCost: 160, growth: 1.5,
      apply: function (s, l) { s.maxSpeed *= 1 + 0.085 * l; s.turnRate *= 1 + 0.04 * l; }
    },
    {
      id: 'inertia', name: 'Inertial Dampers', cat: 'ENGINE', icon: 'inertia',
      desc: 'Faster acceleration and stronger braking — tighter asteroid threading.',
      max: 10, baseCost: 220, growth: 1.52,
      apply: function (s, l) { s.accel *= 1 + 0.12 * l; s.drag *= 1 + 0.05 * l; }
    },
    {
      id: 'hull', name: 'Hull Plating', cat: 'DEFENSE', icon: 'hull',
      desc: 'Layered composite armour. Increases maximum hull integrity.',
      max: 14, baseCost: 200, growth: 1.48,
      apply: function (s, l) { s.maxHull += 26 * l; }
    },
    {
      id: 'shield', name: 'Shield Capacitor', cat: 'DEFENSE', icon: 'shield',
      desc: 'Adds regenerating shield capacity. Shields absorb damage before the hull.',
      max: 14, baseCost: 320, growth: 1.55,
      apply: function (s, l) { s.maxShield += 24 * l; s.shieldRegen += 1.1 * l; }
    },
    {
      id: 'reactor', name: 'Fusion Reactor', cat: 'ENGINE', icon: 'reactor',
      desc: 'Sustained energy output for lasers and shields. Less downtime, more ore.',
      max: 12, baseCost: 280, growth: 1.5,
      apply: function (s, l) { s.energyRegen *= 1 + 0.15 * l; s.maxEnergy *= 1 + 0.05 * l; }
    },
    {
      id: 'efficiency', name: 'Beam Economiser', cat: 'MINING', icon: 'eco',
      desc: 'Pulse modulation cuts the energy cost of every mining tick.',
      max: 8, baseCost: 500, growth: 1.6,
      apply: function (s, l) { s.energyCost *= Math.pow(0.88, l); }
    },
    {
      id: 'scanner', name: 'Deep Scanner', cat: 'UTILITY', icon: 'scanner',
      desc: 'Long-range resonance sweep. Reveals asteroid content and finds rarer seams.',
      max: 10, baseCost: 260, growth: 1.5,
      apply: function (s, l) { s.scanRange += 190 * l; s.rareFind += 0.03 * l; }
    },
    {
      id: 'magnet', name: 'Tractor Field', cat: 'UTILITY', icon: 'magnet',
      desc: 'Pulls free-floating ore into the hold. Greatly speeds up collection.',
      max: 10, baseCost: 300, growth: 1.52,
      apply: function (s, l) { s.magnetRadius += 62 * l; s.collectSpeed *= 1 + 0.1 * l; }
    },
    {
      id: 'weapons', name: 'Pulse Cannons', cat: 'COMBAT', icon: 'weapon',
      desc: 'Damage output of your defensive cannons against drones and raiders.',
      max: 12, baseCost: 340, growth: 1.52,
      apply: function (s, l) { s.weaponDamage *= 1 + 0.18 * l; s.weaponCooldown *= Math.pow(0.97, l); }
    },
    {
      id: 'droneBay', name: 'Drone Bay', cat: 'DRONES', icon: 'drone',
      desc: 'Deploys an additional autonomous mining drone. Drones work while you do.',
      max: 5, baseCost: 2400, growth: 2.35,
      apply: function (s, l) { s.droneSlots += l; }
    },
    {
      id: 'dronePower', name: 'Drone Overclock', cat: 'DRONES', icon: 'dronePower',
      desc: 'Improves every drone\'s mining beam and cargo pod capacity.',
      max: 10, baseCost: 700, growth: 1.62,
      apply: function (s, l) { s.dronePower *= 1 + 0.22 * l; s.droneCargo += 6 * l; }
    },
    {
      id: 'nanorepair', name: 'Nano Repair Bay', cat: 'DEFENSE', icon: 'nano',
      desc: 'Field repairs hull continuously, even mid-combat. Slow but relentless.',
      max: 8, baseCost: 1200, growth: 1.66,
      apply: function (s, l) { s.hullRepair += 0.9 * l; }
    }
  ];

  var BY_ID = {};
  LIST.forEach(function (u) {
    BY_ID[u.id] = u;
    u.cost = function (lvl) { return Math.round(u.baseCost * Math.pow(u.growth, lvl)); };
  });

  SM.Upgrades = {
    list: LIST,
    byId: BY_ID,
    get: function (id) { return BY_ID[id]; },
    categories: ['MINING', 'SHIP', 'ENGINE', 'DEFENSE', 'COMBAT', 'UTILITY', 'DRONES']
  };
})(typeof window !== 'undefined' ? window : globalThis);
