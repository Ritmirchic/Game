/* =========================================================================
   SPACE MINER — hostile drone archetypes
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  var LIST = [
    {
      id: 'scout', name: 'Survey Drone', short: 'SCOUT',
      hp: 30, shield: 0, speed: 215, accel: 260, radius: 15,
      damage: 5, fireEvery: 1.7, range: 330, projectile: 'bolt', projSpeed: 380,
      ai: 'orbit', orbitDist: 260, reward: 55, xp: 34, threat: 1,
      art: { color: '#7fd8ff', core: '#e6faff', style: 'scout' },
      desc: 'Cheap automated surveyor with a mining cutter bolted on backwards.'
    },
    {
      id: 'raider', name: 'Raider Skiff', short: 'RAIDER',
      hp: 78, shield: 15, speed: 190, accel: 240, radius: 20,
      damage: 9, fireEvery: 1.15, range: 380, projectile: 'slug', projSpeed: 470,
      ai: 'strafe', orbitDist: 300, reward: 140, xp: 78, threat: 1.6,
      art: { color: '#ff8a4d', core: '#ffd9b0', style: 'raider' },
      desc: 'Scavenger skiff with a scrap cannon and no regard for your hull.'
    },
    {
      id: 'heavy', name: 'Heavy Breaker', short: 'HEAVY',
      hp: 230, shield: 70, speed: 108, accel: 150, radius: 30,
      damage: 17, fireEvery: 1.9, range: 420, projectile: 'orb', projSpeed: 290,
      ai: 'siege', orbitDist: 340, reward: 380, xp: 190, threat: 2.4,
      art: { color: '#ffc857', core: '#fff3c4', style: 'heavy' },
      desc: 'Slow ore-breaker rig converted into a siege platform. Hits like a rockfall.'
    },
    {
      id: 'plasmaHunter', name: 'Plasma Hunter', short: 'HUNTER',
      hp: 135, shield: 40, speed: 310, accel: 700, radius: 18,
      damage: 13, fireEvery: 1.0, range: 360, projectile: 'plasma', projSpeed: 560,
      ai: 'dash', orbitDist: 220, reward: 460, xp: 240, threat: 2.8,
      art: { color: '#ff5cc8', core: '#ffd0f0', style: 'hunter' },
      desc: 'Ionised predator. Dashes in, burns a hole, dashes out before you can aim.'
    },
    {
      id: 'voidSentinel', name: 'Void Sentinel', short: 'SENTINEL',
      hp: 460, shield: 170, speed: 155, accel: 300, radius: 34,
      damage: 26, fireEvery: 1.5, range: 480, projectile: 'void', projSpeed: 420,
      ai: 'sentinel', orbitDist: 380, reward: 1350, xp: 560, threat: 4.2,
      art: { color: '#b07dff', core: '#e8d8ff', style: 'sentinel' },
      desc: 'Guardian construct of the dead fleet. Fires in arcs and refuses to die politely.'
    }
  ];

  var BY_ID = {};
  LIST.forEach(function (e) { BY_ID[e.id] = e; });

  SM.Enemies = { list: LIST, byId: BY_ID, get: function (id) { return BY_ID[id]; } };
})(typeof window !== 'undefined' ? window : globalThis);
