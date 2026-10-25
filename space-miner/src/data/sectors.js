/* =========================================================================
   SPACE MINER — five hand-authored sectors
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  var WORLD_W = 3900, WORLD_H = 2700;

  var SECTORS = [
    {
      id: 'belt', index: 0, name: 'STARTER BELT', code: 'SB-01',
      subtitle: 'Ceres Threshold · Low Risk',
      desc: 'A tired old mining belt circling a brown dwarf. Dense with common ore, ' +
            'patrolled only by survey drones. Every hauler starts here.',
      danger: 1,
      unlock: { credits: 0, deliver: {} },
      palette: {
        deep: '#05070d', mid: '#0b1521', high: '#173347',
        nebula: ['#1d3b57', '#2a5a6e', '#7a5a35'],
        nebulaMix: 0.55, star: '#d7ecff', dust: '#8fb6cf',
        accent: '#ffb257', accent2: '#5fe6c8', sun: '#ffd39a',
        fog: 0.05, tint: 'rgba(30,50,70,0.10)'
      },
      asteroid: {
        density: 190, sizeMin: 18, sizeMax: 78, families: ['rocky', 'rocky', 'metallic'],
        mineralWeights: { silicate: 30, iron: 30, nickel: 20, copper: 14, silver: 6 }
      },
      enemies: { types: [['scout', 1]], max: 4, spawnEvery: 15, threat: 0.35 },
      hazard: { kind: 'none', intensity: 0 },
      station: { name: 'CERES RELAY', x: 1950, y: 1350 },
      music: { root: 98, scale: [0, 3, 5, 7, 10], bpm: 58 },
      mapPos: { x: 0.16, y: 0.62 }
    },
    {
      id: 'nebula', index: 1, name: 'CRYSTAL NEBULA', code: 'CN-02',
      subtitle: 'Lumen Drift · Moderate Risk',
      desc: 'An ionised veil of glowing dust where crystals grow like reef coral. ' +
            'Visibility is poor, profits are excellent, and raiders know it.',
      danger: 2,
      unlock: { credits: 2600, deliver: { iron: 70 } },
      palette: {
        deep: '#07020f', mid: '#160a2b', high: '#3a1050',
        nebula: ['#8a2fd0', '#2a7fd6', '#ff4fa8'],
        nebulaMix: 0.95, star: '#f0e2ff', dust: '#c9a4ff',
        accent: '#c98bff', accent2: '#54e0ff', sun: '#ff9ae0',
        fog: 0.22, tint: 'rgba(70,30,110,0.14)'
      },
      asteroid: {
        density: 150, sizeMin: 22, sizeMax: 90, families: ['crystal', 'crystal', 'rocky'],
        mineralWeights: { crystal: 30, cobalt: 18, silver: 16, gold: 16, copper: 10, void: 4, silicate: 6 }
      },
      enemies: { types: [['scout', 3], ['raider', 1.4]], max: 6, spawnEvery: 13, threat: 0.6 },
      hazard: { kind: 'nebula', intensity: 1 },
      station: { name: 'PRISM ANCHORAGE', x: 2000, y: 1420 },
      music: { root: 110, scale: [0, 2, 3, 7, 9], bpm: 64 },
      mapPos: { x: 0.36, y: 0.36 }
    },
    {
      id: 'graveyard', index: 2, name: 'IRON GRAVEYARD', code: 'IG-03',
      subtitle: 'Derelict Field · High Risk',
      desc: 'The bones of a failed convoy war. Hulls drift in slow collision, ' +
            'reactor cores still bleeding iridium. Heavy drones guard the salvage.',
      danger: 3,
      unlock: { credits: 12000, deliver: { crystal: 30 } },
      palette: {
        deep: '#06080a', mid: '#121a1c', high: '#2b3a33',
        nebula: ['#2f4a3c', '#5a3a22', '#1f3d4a'],
        nebulaMix: 0.45, star: '#dfe9e1', dust: '#a8bfae',
        accent: '#ff9d4d', accent2: '#7fe0a8', sun: '#ffcf8f',
        fog: 0.10, tint: 'rgba(40,55,45,0.12)'
      },
      asteroid: {
        density: 180, sizeMin: 18, sizeMax: 94, families: ['wreck', 'metallic', 'rocky'],
        mineralWeights: { iron: 30, titanium: 24, iridium: 16, silver: 12, platinum: 8, gold: 6, nickel: 4 }
      },
      enemies: { types: [['raider', 3], ['heavy', 1.6], ['scout', 2]], max: 8, spawnEvery: 11, threat: 0.85 },
      hazard: { kind: 'debris', intensity: 1 },
      station: { name: 'SALVAGE SPINE', x: 1900, y: 1300 },
      music: { root: 87, scale: [0, 1, 5, 7, 8], bpm: 70 },
      mapPos: { x: 0.56, y: 0.68 }
    },
    {
      id: 'plasma', index: 3, name: 'PLASMA STORM', code: 'PS-04',
      subtitle: 'Discharge Reach · Extreme Risk',
      desc: 'A permanent electrical hurricane. Ore here is half-melted and worth a fortune, ' +
            'but a single arc can strip your shields to nothing.',
      danger: 4,
      unlock: { credits: 34000, deliver: { iridium: 24, gold: 140 } },
      palette: {
        deep: '#080312', mid: '#1a0a2e', high: '#42175a',
        nebula: ['#ff3f8f', '#8a2fff', '#2f6cff'],
        nebulaMix: 0.8, star: '#e8d8ff', dust: '#b38bff',
        accent: '#ff6fa8', accent2: '#7ad9ff', sun: '#ffb0d8',
        fog: 0.16, tint: 'rgba(90,20,110,0.16)'
      },
      asteroid: {
        density: 130, sizeMin: 20, sizeMax: 86, families: ['molten', 'molten', 'crystal'],
        mineralWeights: { plasma: 30, uranium: 22, platinum: 14, iridium: 14, titanium: 10, gold: 8, crystal: 2 }
      },
      enemies: { types: [['plasmaHunter', 3], ['heavy', 2], ['raider', 2]], max: 9, spawnEvery: 10, threat: 1.15 },
      hazard: { kind: 'storm', intensity: 1 },
      station: { name: 'ARC BASTION', x: 1950, y: 1400 },
      music: { root: 73, scale: [0, 4, 5, 7, 11], bpm: 78 },
      mapPos: { x: 0.76, y: 0.34 }
    },
    {
      id: 'void', index: 4, name: 'VOID FRONTIER', code: 'VF-05',
      subtitle: 'The Quiet Dark · Lethal',
      desc: 'Beyond the mapped stars, matter is rare and gravity lies. ' +
            'Sentinel constructs patrol the silence around what remains of a dead fleet.',
      danger: 5,
      unlock: { credits: 96000, deliver: { plasma: 40, uranium: 120 } },
      palette: {
        deep: '#02030a', mid: '#080a1c', high: '#1b1236',
        nebula: ['#2a1a6a', '#0d3b52', '#4a1a3a'],
        nebulaMix: 0.7, star: '#cfe0ff', dust: '#8f9ed0',
        accent: '#9ff7c8', accent2: '#b07dff', sun: '#8fa8ff',
        fog: 0.30, tint: 'rgba(20,25,60,0.18)'
      },
      asteroid: {
        density: 105, sizeMin: 24, sizeMax: 98, families: ['void', 'void', 'crystal'],
        mineralWeights: { neutronium: 16, void: 30, plasma: 18, iridium: 14, platinum: 12, uranium: 10 }
      },
      enemies: { types: [['voidSentinel', 3], ['plasmaHunter', 2], ['heavy', 2]], max: 9, spawnEvery: 9, threat: 1.5 },
      hazard: { kind: 'gravity', intensity: 1.2 },
      station: { name: 'LAST LIGHT', x: 2000, y: 1320 },
      music: { root: 65, scale: [0, 1, 3, 6, 8], bpm: 52 },
      mapPos: { x: 0.88, y: 0.56 }
    }
  ];

  var BY_ID = {};
  SECTORS.forEach(function (s) { BY_ID[s.id] = s; });

  SM.Sectors = {
    list: SECTORS,
    byId: BY_ID,
    get: function (id) { return BY_ID[id] || SECTORS[0]; },
    WORLD_W: WORLD_W,
    WORLD_H: WORLD_H,
    first: SECTORS[0]
  };
})(typeof window !== 'undefined' ? window : globalThis);
