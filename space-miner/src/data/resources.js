/* =========================================================================
   SPACE MINER — the fifteen mineral resources
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  /*
    value      : credits per unit when sold at a station
    hardness   : 1 (soft) .. 10 (dense) — resists mining lasers
    mass       : cargo units consumed per item
    shape      : procedural icon family (see art/icons.js)
    drop       : base units an asteroid of this type yields
  */
  var LIST = [
    {
      id: 'silicate', name: 'Silicate', code: 'SIL', value: 6, hardness: 1.1, mass: 0.5,
      color: '#9fb6c4', color2: '#5d7486', glow: '#cfe6f2', shape: 'chunk',
      blurb: 'Compressed quartz rubble. Abundant, cheap, and always in demand for station plating.'
    },
    {
      id: 'iron', name: 'Ferrite', code: 'FER', value: 11, hardness: 1.8, mass: 0.8,
      color: '#b8a795', color2: '#6b5a4c', glow: '#e8d7c3', shape: 'ingot',
      blurb: 'The backbone of every hull in the belt. Smelts clean, sells anywhere.'
    },
    {
      id: 'nickel', name: 'Nickel', code: 'NIC', value: 19, hardness: 2.4, mass: 0.9,
      color: '#c3c8b6', color2: '#6f7768', glow: '#e9ecd8', shape: 'chunk',
      blurb: 'Corrosion-resistant alloy stock. Station fabricators pay a steady premium.'
    },
    {
      id: 'copper', name: 'Cuprite', code: 'CUP', value: 34, hardness: 2.7, mass: 1.0,
      color: '#e08b4a', color2: '#8a3f1f', glow: '#ffb56b', shape: 'ingot',
      blurb: 'Superconductive ore with a warm signature. Your scanner loves it.'
    },
    {
      id: 'titanium', name: 'Titanium', code: 'TIT', value: 62, hardness: 3.6, mass: 1.3,
      color: '#a8bfd4', color2: '#4b6178', glow: '#d9ecff', shape: 'ingot',
      blurb: 'Light, stubborn, and the reason your drill bit keeps needing replacement.'
    },
    {
      id: 'silver', name: 'Silver', code: 'AG', value: 104, hardness: 3.1, mass: 1.1,
      color: '#dfe9f2', color2: '#7e909f', glow: '#ffffff', shape: 'chunk',
      blurb: 'Bright metallic veins, sought after by optics manufacturers.'
    },
    {
      id: 'cobalt', name: 'Cobalt', code: 'CO', value: 148, hardness: 4.0, mass: 1.2,
      color: '#5d8dff', color2: '#1d3d8a', glow: '#8fb4ff', shape: 'crystal',
      blurb: 'Deep blue lattice used in shield capacitors. Glows faintly under strain.'
    },
    {
      id: 'gold', name: 'Gold', code: 'AU', value: 210, hardness: 3.3, mass: 1.4,
      color: '#ffcf5c', color2: '#8a5c12', glow: '#ffe9a8', shape: 'ingot',
      blurb: 'Wealth in rock form. Worth the detour and the risk.'
    },
    {
      id: 'crystal', name: 'Lumen Crystal', code: 'LUM', value: 285, hardness: 4.4, mass: 1.0,
      color: '#59f0e2', color2: '#0d6b74', glow: '#a8fff6', shape: 'crystal',
      blurb: 'A living lattice that stores light. Nebula bloom turns it into a beacon.'
    },
    {
      id: 'iridium', name: 'Iridium', code: 'IR', value: 470, hardness: 5.6, mass: 1.6,
      color: '#d9d0ff', color2: '#5a4b8f', glow: '#c8bbff', shape: 'chunk',
      blurb: 'Scavenged from derelict reactor cores. Dense enough to bend scanners.'
    },
    {
      id: 'platinum', name: 'Platinum', code: 'PT', value: 640, hardness: 4.8, mass: 1.5,
      color: '#f0f4f8', color2: '#8d98a6', glow: '#ffffff', shape: 'ingot',
      blurb: 'Catalyst metal. Every fuel cell in the sector needs a pinch of it.'
    },
    {
      id: 'uranium', name: 'Uranium', code: 'U', value: 810, hardness: 5.2, mass: 1.8,
      color: '#a8e05a', color2: '#2f5c12', glow: '#d8ff8a', shape: 'orb',
      blurb: 'Hot to the touch. Shielded cargo pods are strongly advised.'
    },
    {
      id: 'plasma', name: 'Plasma Ore', code: 'PLS', value: 1240, hardness: 6.6, mass: 1.6,
      color: '#ff7ac8', color2: '#7a1258', glow: '#ffb0e2', shape: 'orb',
      blurb: 'Ionised rock trapped mid-discharge. It hums a note you can feel in your teeth.'
    },
    {
      id: 'void', name: 'Void Crystal', code: 'VOI', value: 2400, hardness: 7.8, mass: 1.4,
      color: '#b07dff', color2: '#2a0e52', glow: '#d9baff', shape: 'crystal',
      blurb: 'Light falls into it at an angle that does not exist. Frontier traders pay in fear.'
    },
    {
      id: 'neutronium', name: 'Neutronium', code: 'NTR', value: 4800, hardness: 9.4, mass: 2.6,
      color: '#9ff7c8', color2: '#0d4a34', glow: '#d6ffe8', shape: 'shard',
      blurb: 'A spoonful of collapsed star. Your cargo clamps complain loudly.'
    }
  ];

  var BY_ID = {};
  LIST.forEach(function (r, i) { r.tier = i; BY_ID[r.id] = r; });

  SM.Resources = {
    list: LIST,
    byId: BY_ID,
    get: function (id) { return BY_ID[id]; },
    /* presentation order for inventories */
    order: LIST.map(function (r) { return r.id; })
  };
})(typeof window !== 'undefined' ? window : globalThis);
