/* =========================================================================
   SPACE MINER — hull classes. Base stats feed the derived stat block.
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  var SHIPS = [
    {
      id: 'prospector', name: 'PROSPECTOR MK II', role: 'Starter Industrial Cutter',
      cost: 0, levelReq: 1,
      desc: 'A dependable single-seat cutter. Twin mining emitters, one drone cradle, ' +
            'and a hull that has clearly been welded more than once.',
      stats: {
        maxHull: 110, maxShield: 40, shieldRegen: 2.2,
        maxEnergy: 110, energyRegen: 15,
        laserPower: 13, laserRange: 400, penetration: 1, energyCost: 1,
        cargoCap: 48, massFactor: 1,
        maxSpeed: 305, accel: 520, drag: 1.9, turnRate: 7,
        weaponDamage: 9, weaponCooldown: 0.32, weaponSpeed: 900,
        scanRange: 700, rareFind: 0, magnetRadius: 85, collectSpeed: 330,
        droneSlots: 1, dronePower: 1, droneCargo: 8, hullRepair: 0, armor: 0
      },
      art: { scale: 1.0, engines: 2, accent: '#5fe6c8' }
    },
    {
      id: 'harvester', name: 'HARVESTER-class BULK', role: 'Heavy Ore Barge',
      cost: 14000, levelReq: 4,
      desc: 'A slow, wide industrial barge with a pressurised hold the size of a hangar. ' +
            'Three drone cradles and hull plating thick enough to shrug off debris.',
      stats: {
        maxHull: 190, maxShield: 70, shieldRegen: 2.6,
        maxEnergy: 150, energyRegen: 18,
        laserPower: 16, laserRange: 440, penetration: 1.15, energyCost: 0.95,
        cargoCap: 125, massFactor: 1,
        maxSpeed: 245, accel: 400, drag: 1.75, turnRate: 5,
        weaponDamage: 8, weaponCooldown: 0.38, weaponSpeed: 850,
        scanRange: 800, rareFind: 0.02, magnetRadius: 120, collectSpeed: 300,
        droneSlots: 3, dronePower: 1, droneCargo: 10, hullRepair: 0, armor: 0.12
      },
      art: { scale: 1.22, engines: 3, accent: '#ffb257' }
    },
    {
      id: 'interceptor', name: 'LANCET-class INTERCEPTOR', role: 'Fast Escort Cutter',
      cost: 26000, levelReq: 6,
      desc: 'Built for pilots who mine the edges of hostile space. Thin armour, ' +
            'brutal acceleration, and cannons that actually end arguments.',
      stats: {
        maxHull: 120, maxShield: 105, shieldRegen: 4.2,
        maxEnergy: 130, energyRegen: 20,
        laserPower: 12, laserRange: 380, penetration: 1, energyCost: 1.05,
        cargoCap: 40, massFactor: 1,
        maxSpeed: 400, accel: 780, drag: 2.3, turnRate: 8.4,
        weaponDamage: 21, weaponCooldown: 0.21, weaponSpeed: 1080,
        scanRange: 760, rareFind: 0.03, magnetRadius: 95, collectSpeed: 380,
        droneSlots: 1, dronePower: 0.9, droneCargo: 8, hullRepair: 0, armor: 0.05
      },
      art: { scale: 0.95, engines: 2, accent: '#54e0ff' }
    },
    {
      id: 'leviathan', name: 'LEVIATHAN-class FORGE', role: 'Mobile Mining Platform',
      cost: 88000, levelReq: 10,
      desc: 'Not so much a ship as a small foundry with engines. Four drone cradles, ' +
            'triple mining arrays, and a reactor that hums in your sleep.',
      stats: {
        maxHull: 300, maxShield: 190, shieldRegen: 5.5,
        maxEnergy: 240, energyRegen: 26,
        laserPower: 26, laserRange: 540, penetration: 1.4, energyCost: 0.9,
        cargoCap: 200, massFactor: 0.98,
        maxSpeed: 320, accel: 600, drag: 2.0, turnRate: 6,
        weaponDamage: 30, weaponCooldown: 0.26, weaponSpeed: 1000,
        scanRange: 1100, rareFind: 0.08, magnetRadius: 170, collectSpeed: 420,
        droneSlots: 4, dronePower: 1.35, droneCargo: 14, hullRepair: 0.5, armor: 0.2
      },
      art: { scale: 1.42, engines: 4, accent: '#9ff7c8' }
    }
  ];

  var BY_ID = {};
  SHIPS.forEach(function (s) { BY_ID[s.id] = s; });

  SM.Ships = { list: SHIPS, byId: BY_ID, get: function (id) { return BY_ID[id] || SHIPS[0]; } };
})(typeof window !== 'undefined' ? window : globalThis);
