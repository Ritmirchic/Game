/* =========================================================================
   SPACE MINER — player profile: progression, derived stats, save/load.
   Pure logic, no rendering — used by the game and by the headless tests.
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U;

  var SAVE_VERSION = 4;
  var SAVE_KEY = 'spaceminer.save.v4';
  var BACKUP_KEY = 'spaceminer.save.backup';
  var SETTINGS_KEY = 'spaceminer.settings';

  function emptyCounters() {
    return {
      asteroidsMined: 0, creditsEarned: 0, kills: 0, droneMined: 0,
      minedByType: {}, upgradesBought: 0, fullHolds: 0, playTime: 0,
      maxDrones: 0, deaths: 0, distance: 0, sectorsVisited: 0, runs: 0
    };
  }

  function create() {
    var p = {
      v: SAVE_VERSION,
      credits: 500,
      xp: 0,
      level: 1,
      shipId: 'prospector',
      ships: ['prospector'],
      sector: 'belt',
      sectors: ['belt'],
      upgrades: {},
      cargo: {},
      drones: [{ serial: 1, mined: 0 }],
      achievements: [],
      counters: emptyCounters(),
      settings: { sfx: true, music: true, shake: true, quality: 1, showHints: true },
      seen: {},
      playTime: 0,
      lastSaved: 0
    };
    SM.Upgrades.list.forEach(function (u) { p.upgrades[u.id] = 0; });
    return p;
  }

  function xpForLevel(level) {
    return Math.round(85 * Math.pow(level, 1.42));
  }

  function totalXpForLevel(level) {
    var t = 0;
    for (var i = 1; i < level; i++) t += xpForLevel(i);
    return t;
  }

  function resolveLevel(p) {
    var lvl = 1;
    while (lvl < 40 && p.xp >= totalXpForLevel(lvl + 1)) lvl++;
    p.level = lvl;
    return lvl;
  }

  /* ---------------- derived statistics ---------------- */
  var statsCache = { key: '', value: null };

  function stats(p) {
    var key = p.shipId + '|' + p.level;
    for (var id in p.upgrades) key += '|' + id + p.upgrades[id];
    if (statsCache.key === key && statsCache.value) return statsCache.value;

    var base = SM.Ships.get(p.shipId).stats;
    var s = {};
    for (var k in base) s[k] = base[k];

    SM.Upgrades.list.forEach(function (u) {
      var lvl = p.upgrades[u.id] | 0;
      if (lvl > 0) u.apply(s, lvl);
    });

    // every 5 pilot levels grant a small all-round bonus (real, documented)
    var lv = Math.max(0, (p.level | 0) - 1);
    s.maxHull *= 1 + lv * 0.012;
    s.maxShield *= 1 + lv * 0.012;
    s.laserPower *= 1 + lv * 0.015;
    s.weaponDamage *= 1 + lv * 0.015;

    // drone count follows ship cradles + drone-bay upgrades, capped
    s.droneSlots = Math.min(6, Math.round(base.droneSlots + (p.upgrades.droneBay | 0)));
    s.maxHull = Math.round(s.maxHull);
    s.maxShield = Math.round(s.maxShield);
    s.maxEnergy = Math.round(s.maxEnergy);
    s.cargoCap = Math.round(s.cargoCap);

    statsCache.key = key;
    statsCache.value = s;
    return s;
  }

  function invalidateStats() { statsCache.key = ''; statsCache.value = null; }

  /* ---------------- cargo ---------------- */
  function cargoUsed(p) {
    var s = stats(p), total = 0;
    for (var id in p.cargo) {
      var q = p.cargo[id] | 0;
      if (q <= 0) continue;
      var r = SM.Resources.get(id);
      if (!r) continue;
      total += q * r.mass * s.massFactor;
    }
    return total;
  }
  function cargoCap(p) { return stats(p).cargoCap; }
  function cargoFree(p) { return Math.max(0, cargoCap(p) - cargoUsed(p)); }

  function addResource(p, id, qty) {
    var r = SM.Resources.get(id);
    if (!r) return 0;
    var s = stats(p);
    var per = r.mass * s.massFactor;
    var free = cargoFree(p);
    var canFit = per > 0 ? Math.floor(free / per + 1e-6) : qty;
    var added = Math.min(qty, Math.max(0, canFit));
    if (added <= 0) return 0;
    p.cargo[id] = (p.cargo[id] | 0) + added;
    p.counters.minedByType[id] = (p.counters.minedByType[id] | 0) + added;
    if (cargoFree(p) <= 0.001) p.counters.fullHolds++;
    return added;
  }

  function cargoValue(p) {
    var total = 0;
    for (var id in p.cargo) {
      var r = SM.Resources.get(id);
      if (!r) continue;
      total += (p.cargo[id] | 0) * r.value;
    }
    return total;
  }

  function sellAll(p) {
    var total = cargoValue(p);
    p.credits += total;
    p.counters.creditsEarned += total;
    p.cargo = {};
    return total;
  }

  function sellResource(p, id) {
    var r = SM.Resources.get(id);
    if (!r || !p.cargo[id]) return 0;
    var total = p.cargo[id] * r.value;
    p.credits += total;
    p.counters.creditsEarned += total;
    delete p.cargo[id];
    return total;
  }

  /* ---------------- purchases ---------------- */
  function upgradeLevel(p, id) { return p.upgrades[id] | 0; }
  function upgradeCost(p, id) {
    var u = SM.Upgrades.get(id);
    if (!u) return 0;
    var lvl = upgradeLevel(p, id);
    if (lvl >= u.max) return 0;
    return u.cost(lvl);
  }

  function buyUpgrade(p, id, freeMode) {
    var u = SM.Upgrades.get(id);
    if (!u) return { ok: false, reason: 'unknown' };
    var lvl = upgradeLevel(p, id);
    if (lvl >= u.max) return { ok: false, reason: 'max' };
    var cost = u.cost(lvl);
    if (!freeMode && p.credits < cost) return { ok: false, reason: 'credits', cost: cost };
    if (!freeMode) p.credits -= cost;
    p.upgrades[id] = lvl + 1;
    p.counters.upgradesBought++;
    invalidateStats();
    syncDrones(p);
    return { ok: true, cost: cost, level: lvl + 1 };
  }

  function shipOwned(p, id) { return p.ships.indexOf(id) >= 0; }
  function buyShip(p, id, freeMode) {
    var ship = SM.Ships.get(id);
    if (!ship) return { ok: false, reason: 'unknown' };
    if (shipOwned(p, id)) return { ok: false, reason: 'owned' };
    if (p.level < ship.levelReq) return { ok: false, reason: 'level', need: ship.levelReq };
    if (p.credits < ship.cost && !freeMode) return { ok: false, reason: 'credits', cost: ship.cost };
    if (!freeMode) p.credits -= ship.cost;
    p.ships.push(id);
    return { ok: true };
  }

  function selectShip(p, id) {
    if (!shipOwned(p, id)) return false;
    p.shipId = id;
    invalidateStats();
    syncDrones(p);
    return true;
  }

  function syncDrones(p) {
    var want = stats(p).droneSlots;
    while (p.drones.length < want) {
      p.drones.push({ serial: p.drones.length + 1, mined: 0 });
    }
    if (p.drones.length > want) p.drones.length = want;
    p.counters.maxDrones = Math.max(p.counters.maxDrones, p.drones.length);
  }

  /* ---------------- sectors ---------------- */
  function sectorUnlocked(p, id) { return p.sectors.indexOf(id) >= 0; }

  function canUnlock(p, id) {
    var sec = SM.Sectors.get(id);
    if (!sec) return { ok: false, reason: 'unknown' };
    if (sectorUnlocked(p, id)) return { ok: false, reason: 'owned' };
    if (p.credits < sec.unlock.credits) return { ok: false, reason: 'credits', need: sec.unlock.credits };
    var missing = {};
    var missingAny = false;
    for (var rid in sec.unlock.deliver) {
      var need = sec.unlock.deliver[rid];
      var have = p.cargo[rid] | 0;
      if (have < need) { missing[rid] = need - have; missingAny = true; }
    }
    if (missingAny) return { ok: false, reason: 'deliver', missing: missing };
    return { ok: true };
  }

  function unlockSector(p, id, freeMode) {
    var check = canUnlock(p, id);
    if (!check.ok) return check;
    var sec = SM.Sectors.get(id);
    if (!freeMode) p.credits -= sec.unlock.credits;
    for (var rid in sec.unlock.deliver) {
      p.cargo[rid] = (p.cargo[rid] | 0) - sec.unlock.deliver[rid];
      if (p.cargo[rid] <= 0) delete p.cargo[rid];
    }
    p.sectors.push(id);
    p.counters.sectorsVisited = p.sectors.length;
    return { ok: true };
  }

  /* ---------------- xp / achievements ---------------- */
  function addXp(p, amt) {
    p.xp += Math.max(0, amt); // xp stored as lifetime total
    var before = p.level;
    resolveLevel(p);
    return p.level > before ? p.level : 0;
  }

  function railLevelProgress(p) {
    var cur = totalXpForLevel(p.level);
    var next = totalXpForLevel(p.level + 1);
    return { cur: p.xp - cur, need: next - cur, pct: U.clamp((p.xp - cur) / Math.max(1, next - cur), 0, 1) };
  }

  function checkAchievements(p) {
    var unlocked = [];
    SM.Achievements.list.forEach(function (a) {
      if (p.achievements.indexOf(a.id) >= 0) return;
      var ok = false;
      try { ok = !!a.test(p); } catch (e) { ok = false; }
      if (ok) {
        p.achievements.push(a.id);
        p.credits += a.reward;
        p.counters.creditsEarned += a.reward;
        unlocked.push(a);
      }
    });
    return unlocked;
  }

  /* ---------------- serialisation ---------------- */
  function checksum(payload) {
    var s = JSON.stringify(payload);
    var h = 5381;
    for (var i = 0; i < s.length; i++) h = ((h * 33) ^ s.charCodeAt(i)) >>> 0;
    return h.toString(36);
  }

  function serialize(p) {
    var payload = {
      credits: Math.round(p.credits),
      xp: p.xp, level: p.level,
      shipId: p.shipId, ships: p.ships.slice(),
      sector: p.sector, sectors: p.sectors.slice(),
      upgrades: p.upgrades, cargo: p.cargo,
      drones: p.drones,
      achievements: p.achievements.slice(),
      counters: p.counters,
      settings: p.settings,
      seen: p.seen,
      playTime: Math.round(p.playTime),
      lastSaved: Date.now()
    };
    return JSON.stringify({ v: SAVE_VERSION, sum: checksum(payload), data: payload });
  }

  function num(v, def, lo, hi) {
    v = Number(v);
    if (!isFinite(v)) return def;
    return U.clamp(v, lo, hi);
  }

  function deserialize(json) {
    var parsed;
    try { parsed = JSON.parse(json); } catch (e) { return { ok: false, reason: 'json' }; }
    if (!parsed || typeof parsed !== 'object') return { ok: false, reason: 'shape' };
    if (parsed.v > SAVE_VERSION) return { ok: false, reason: 'future-version' };
    var d = parsed.data;
    if (!d || typeof d !== 'object') return { ok: false, reason: 'data' };
    if (parsed.sum && checksum(d) !== parsed.sum) return { ok: false, reason: 'checksum' };

    var p = create();
    p.credits = num(d.credits, 500, 0, 1e12);
    p.xp = num(d.xp, 0, 0, 1e12);
    p.level = 1;
    resolveLevel(p);

    if (SM.Ships.get(d.shipId) && d.shipId) p.shipId = d.shipId;
    p.ships = ['prospector'];
    (Array.isArray(d.ships) ? d.ships : []).forEach(function (id) {
      if (SM.Ships.get(id) && p.ships.indexOf(id) < 0) p.ships.push(id);
    });
    if (p.ships.indexOf(p.shipId) < 0) p.shipId = 'prospector';

    p.sectors = ['belt'];
    (Array.isArray(d.sectors) ? d.sectors : []).forEach(function (id) {
      if (SM.Sectors.byId[id] && p.sectors.indexOf(id) < 0) p.sectors.push(id);
    });
    p.sector = SM.Sectors.byId[d.sector] ? d.sector : 'belt';
    if (p.sectors.indexOf(p.sector) < 0) p.sector = 'belt';

    if (d.upgrades && typeof d.upgrades === 'object') {
      Object.keys(d.upgrades).forEach(function (id) {
        var u = SM.Upgrades.get(id);
        if (!u) return;
        p.upgrades[id] = Math.floor(num(d.upgrades[id], 0, 0, u.max));
      });
    }
    if (d.cargo && typeof d.cargo === 'object') {
      Object.keys(d.cargo).forEach(function (id) {
        if (!SM.Resources.get(id)) return;
        var q = Math.floor(num(d.cargo[id], 0, 0, 1e7));
        if (q > 0) p.cargo[id] = q;
      });
    }
    if (Array.isArray(d.drones)) {
      p.drones = d.drones.slice(0, 6).map(function (dr, i) {
        return { serial: num(dr && dr.serial, i + 1, 1, 999), mined: Math.floor(num(dr && dr.mined, 0, 0, 1e9)) };
      });
    }
    if (Array.isArray(d.achievements)) {
      d.achievements.forEach(function (id) {
        if (SM.Achievements.get(id) && p.achievements.indexOf(id) < 0) p.achievements.push(id);
      });
    }
    var c = d.counters || {};
    var base = emptyCounters();
    Object.keys(base).forEach(function (k) {
      if (k === 'minedByType') {
        if (c.minedByType && typeof c.minedByType === 'object') {
          Object.keys(c.minedByType).forEach(function (rid) {
            if (SM.Resources.get(rid)) p.counters.minedByType[rid] = Math.floor(num(c.minedByType[rid], 0, 0, 1e9));
          });
        }
      } else {
        p.counters[k] = Math.floor(num(c[k], 0, 0, 1e12));
      }
    });
    if (d.settings && typeof d.settings === 'object') {
      ['sfx', 'music', 'shake', 'showHints'].forEach(function (k) {
        if (typeof d.settings[k] === 'boolean') p.settings[k] = d.settings[k];
      });
      p.settings.quality = U.clamp(num(d.settings.quality, 1, 0, 1), 0, 1);
    }
    if (d.seen && typeof d.seen === 'object') p.seen = d.seen;
    p.playTime = num(d.playTime, 0, 0, 1e9);
    p.lastSaved = num(d.lastSaved, 0, 0, 1e15);

    resolveLevel(p);
    invalidateStats();
    syncDrones(p);
    return { ok: true, profile: p };
  }

  /* ---------------- storage helpers ---------------- */
  var mem = {};
  function storage() {
    if (root.localStorage) return root.localStorage;
    if (!mem.__store) mem.__store = {};
    return {
      getItem: function (k) { return Object.prototype.hasOwnProperty.call(mem.__store, k) ? mem.__store[k] : null; },
      setItem: function (k, v) { mem.__store[k] = String(v); },
      removeItem: function (k) { delete mem.__store[k]; }
    };
  }

  function save(p) {
    var json = serialize(p);
    try {
      var st = storage();
      var prev = st.getItem(SAVE_KEY);
      if (prev) st.setItem(BACKUP_KEY, prev);
      st.setItem(SAVE_KEY, json);
      p.lastSaved = Date.now();
      return { ok: true, bytes: json.length };
    } catch (e) {
      return { ok: false, error: String(e && e.message || e) };
    }
  }

  function load() {
    var st = storage();
    var raw = null, where = SAVE_KEY;
    try { raw = st.getItem(SAVE_KEY); } catch (e) { raw = null; }
    if (!raw) {
      try { raw = st.getItem(BACKUP_KEY); where = BACKUP_KEY; } catch (e) { raw = null; }
      if (!raw) return { ok: false, reason: 'empty' };
    }
    var res = deserialize(raw);
    if (!res.ok) {
      // keep the broken file for diagnostics, then try the previous one
      try { st.setItem('spaceminer.save.corrupt', raw); } catch (e) {}
      if (where === SAVE_KEY) {
        var backup = null;
        try { backup = st.getItem(BACKUP_KEY); } catch (e) {}
        if (backup) {
          var res2 = deserialize(backup);
          if (res2.ok) { res2.recovered = true; return res2; }
        }
      }
      return res;
    }
    res.source = where;
    return res;
  }

  function wipe() {
    var st = storage();
    try { st.removeItem(SAVE_KEY); st.removeItem(BACKUP_KEY); } catch (e) {}
  }

  function saveSettings(settings) {
    try { storage().setItem(SETTINGS_KEY, JSON.stringify(settings)); return true; } catch (e) { return false; }
  }
  function loadSettings() {
    try {
      var raw = storage().getItem(SETTINGS_KEY);
      return raw ? JSON.parse(raw) : null;
    } catch (e) { return null; }
  }

  SM.Profile = {
    SAVE_VERSION: SAVE_VERSION, SAVE_KEY: SAVE_KEY, BACKUP_KEY: BACKUP_KEY,
    create: create, stats: stats, invalidateStats: invalidateStats,
    xpForLevel: xpForLevel, totalXpForLevel: totalXpForLevel, resolveLevel: resolveLevel, addXp: addXp,
    levelProgress: railLevelProgress,
    cargoUsed: cargoUsed, cargoCap: cargoCap, cargoFree: cargoFree,
    addResource: addResource, cargoValue: cargoValue, sellAll: sellAll, sellResource: sellResource,
    upgradeLevel: upgradeLevel, upgradeCost: upgradeCost, buyUpgrade: buyUpgrade,
    shipOwned: shipOwned, buyShip: buyShip, selectShip: selectShip, syncDrones: syncDrones,
    sectorUnlocked: sectorUnlocked, canUnlock: canUnlock, unlockSector: unlockSector,
    checkAchievements: checkAchievements,
    serialize: serialize, deserialize: deserialize, save: save, load: load, wipe: wipe,
    saveSettings: saveSettings, loadSettings: loadSettings, storage: storage
  };
})(typeof window !== 'undefined' ? window : globalThis);
