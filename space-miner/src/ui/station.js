/* =========================================================================
   SPACE MINER — station interface: trade, upgrades, drones, shipyard, chart
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, W = SM.W, Theme = SM.Theme, Font = SM.Font, Text = SM.Text, hexA = U.hexA;
  var Path = SM.Path;

  var TABS = [
    { id: 'trade', label: 'TRADE & REPAIR', icon: 'credits' },
    { id: 'upgrades', label: 'UPGRADES', icon: 'level' },
    { id: 'drones', label: 'DRONE BAY', icon: 'drone' },
    { id: 'shipyard', label: 'SHIPYARD', icon: 'hull' },
    { id: 'sectors', label: 'SECTOR CHART', icon: 'scan' }
  ];

  function open() {
    return { tab: 'trade', cat: 'ALL', shipSel: null };
  }

  function draw(game, ctx, ui) {
    var s = ui.s;
    var w = ui.w, h = ui.h;
    var pr = game.profile;

    // dim world
    ctx.save();
    ctx.fillStyle = 'rgba(2,6,11,0.78)';
    ctx.fillRect(0, 0, w, h);

    var pw = Math.min(w - 60 * s, 1180 * s);
    var ph = Math.min(h - 70 * s, 700 * s);
    var px = (w - pw) / 2, py = (h - ph) / 2;

    W.panel(ctx, px, py, pw, ph, {
      fill: 'rgba(7,16,26,0.97)', border: 'rgba(90,150,190,0.55)', accent: '#ffb257', cut: 20 * s
    });

    // ---------- header ----------
    var st = game.world ? game.world.station : { name: 'STATION' };
    var sector = game.world ? game.world.sector : SM.Sectors.first;
    var hh = 62 * s;
    ctx.save();
    ctx.fillStyle = 'rgba(255,178,87,0.06)';
    ctx.fillRect(px + 1, py + 1, pw - 2, hh);
    var g = ctx.createLinearGradient(px, py, px, py + hh);
    g.addColorStop(0, 'rgba(255,255,255,0.05)');
    g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g;
    ctx.fillRect(px + 1, py + 1, pw - 2, hh);
    ctx.restore();

    Text.draw(ctx, st.name, px + 26 * s, py + 30 * s, {
      font: Font.title(21 * s, 700), color: '#ffffff', spacing: 3.2 * s,
      shadow: 'rgba(255,178,87,0.6)', shadowBlur: 16
    });
    Text.draw(ctx, 'DOCKED · ' + sector.name + ' · ' + sector.code, px + 26 * s, py + 48 * s, {
      font: Font.mono(9.5 * s, 600), color: Theme.dim, spacing: 2
    });

    // credits + level
    SM.Icons.hud(ctx, 'credits', px + pw - 236 * s, py + 26 * s, 17 * s, '#ffd166');
    Text.draw(ctx, U.fmtNum(pr.credits), px + pw - 218 * s, py + 32 * s, {
      font: Font.title(19 * s, 700), color: '#ffe9a8'
    });
    Text.draw(ctx, 'PILOT LVL ' + pr.level, px + pw - 26 * s, py + 32 * s, {
      font: Font.mono(9.5 * s, 600), color: Theme.inkSoft, align: 'right', spacing: 1.4
    });

    // depart
    if (W.button(ui, ctx, 'depart', px + pw - 156 * s, py + 40 * s, 130 * s, 18 * s, 'UNDOCK  [ESC]', {
      size: 10 * s, accent: '#ffb257', primary: true
    })) {
      game.undock();
      return;
    }

    ctx.save();
    ctx.strokeStyle = 'rgba(120,170,200,0.28)';
    ctx.beginPath();
    ctx.moveTo(px + 20 * s, py + hh); ctx.lineTo(px + pw - 20 * s, py + hh);
    ctx.stroke();
    ctx.restore();

    // ---------- sidebar ----------
    var sx = px + 20 * s, sy = py + hh + 20 * s;
    var sw = 196 * s;
    var sideH = ph - hh - 40 * s;
    W.panel(ctx, sx, sy, sw, sideH, { fill: 'rgba(5,12,20,0.7)', cut: 12 * s, accent: 'rgba(95,230,216,0.5)' });

    var tabY = sy + 16 * s;
    for (var i = 0; i < TABS.length; i++) {
      var t = TABS[i];
      var active = ui.station.tab === t.id;
      if (W.button(ui, ctx, 'tab-' + t.id, sx + 12 * s, tabY, sw - 24 * s, 40 * s, t.label, {
        size: 10.5 * s, leftIcon: t.icon, active: active, accent: '#5fe6d8'
      })) {
        ui.station.tab = t.id;
      }
      tabY += 46 * s;
    }

    // ship status mini panel
    var mst = game.world ? game.world.stats : SM.Profile.stats(pr);
    var my = sy + sideH - 164 * s;
    Text.draw(ctx, 'VESSEL', sx + 14 * s, my, { font: Font.mono(8.5 * s, 600), color: Theme.dimmer, spacing: 2 });
    Text.draw(ctx, SM.Ships.get(pr.shipId).name, sx + 14 * s, my + 18 * s, {
      font: Font.title(10 * s, 700), color: Theme.ink
    });
    var stats = [
      ['HULL', mst.maxHull, '#7fe0a8'],
      ['SHIELD', mst.maxShield, '#6cc7ff'],
      ['CARGO', mst.cargoCap, '#5fe6d8'],
      ['MINING DPS', Math.round(mst.laserPower * mst.penetration * 10) / 10, '#ffd166'],
      ['DRONES', mst.droneSlots, '#9fe8dd']
    ];
    for (var k = 0; k < stats.length; k++) {
      var sy2 = my + 38 * s + k * 15 * s;
      Text.draw(ctx, stats[k][0], sx + 14 * s, sy2, {
        font: Font.mono(8.5 * s), color: Theme.dim, spacing: 1
      });
      Text.draw(ctx, String(stats[k][1]), sx + sw - 14 * s, sy2, {
        font: Font.mono(9.5 * s, 600), color: stats[k][2], align: 'right'
      });
    }

    // ---------- content ----------
    var cx = sx + sw + 18 * s, cy = py + hh + 20 * s;
    var cw = pw - sw - 58 * s, ch = ph - hh - 40 * s;

    if (ui.station.tab === 'trade') drawTrade(game, ctx, ui, cx, cy, cw, ch);
    else if (ui.station.tab === 'upgrades') drawUpgrades(game, ctx, ui, cx, cy, cw, ch);
    else if (ui.station.tab === 'drones') drawDrones(game, ctx, ui, cx, cy, cw, ch);
    else if (ui.station.tab === 'shipyard') drawShipyard(game, ctx, ui, cx, cy, cw, ch);
    else if (ui.station.tab === 'sectors') drawSectors(game, ctx, ui, cx, cy, cw, ch);

    ctx.restore();
  }

  /* ================= TRADE ================= */
  function drawTrade(game, ctx, ui, x, y, w, h) {
    var s = ui.s, pr = game.profile;
    var leftW = w * 0.58;
    W.header(ctx, x, y, leftW, 'CARGO MANIFEST', { sub: 'SELL TO STATION', accent: '#ffd166' });

    var ids = Object.keys(pr.cargo).filter(function (id) { return pr.cargo[id] > 0; });
    ids.sort(function (a, b) { return SM.Resources.get(b).value - SM.Resources.get(a).value; });
    var rowH = 42 * s;
    var listH = h - 130 * s;
    var total = 0;
    ids.forEach(function (id) { total += pr.cargo[id] * SM.Resources.get(id).value; });

    if (!ids.length) {
      ctx.save();
      ctx.globalAlpha = 0.7;
      Text.draw(ctx, 'THE HOLD IS EMPTY', x + 8 * s, y + 56 * s, {
        font: Font.title(13 * s, 700), color: Theme.dimmer, spacing: 3
      });
      Text.draw(ctx, 'Undock, fire the mining beam at an asteroid,', x + 8 * s, y + 78 * s, { font: Font.body(11.5 * s), color: Theme.dim });
      Text.draw(ctx, 'then return here to sell the ore.', x + 8 * s, y + 96 * s, { font: Font.body(11.5 * s), color: Theme.dim });
      ctx.restore();
    } else {
      W.scrollArea(ui, ctx, 'trade', x, y + 34 * s, leftW - 10 * s, listH, ids.length * rowH + 8 * s, function (ctx) {
        for (var i = 0; i < ids.length; i++) {
          var rid = ids[i];
          var res = SM.Resources.get(rid);
          var qty = pr.cargo[rid];
          var ry = y + 34 * s + i * rowH;
          var hover = W.inside(ui.mouse, x, ry, leftW - 10 * s, rowH - 4 * s);
          ctx.save();
          Path.cutRect(ctx, x, ry, leftW - 10 * s, rowH - 4 * s, 6 * s);
          ctx.fillStyle = hover ? 'rgba(120,180,220,0.10)' : 'rgba(255,255,255,0.025)';
          ctx.fill();
          ctx.restore();
          var icon = SM.Icons.resource(res);
          ctx.drawImage(icon.canvas, x + 8 * s, ry + 5 * s, 28 * s, 28 * s);
          Text.draw(ctx, res.name, x + 44 * s, ry + 18 * s, { font: Font.body(12.5 * s, 600), color: Theme.ink });
          Text.draw(ctx, res.code + ' · ' + res.value + ' CR/UNIT', x + 44 * s, ry + 32 * s, {
            font: Font.mono(8.5 * s), color: Theme.dim, spacing: 0.8
          });
          Text.draw(ctx, 'x' + qty, x + leftW - 196 * s, ry + 22 * s, {
            font: Font.mono(12 * s, 700), color: res.color, align: 'right'
          });
          Text.draw(ctx, U.fmtNum(qty * res.value) + ' CR', x + leftW - 108 * s, ry + 22 * s, {
            font: Font.mono(11 * s, 600), color: '#ffd166', align: 'right'
          });
          if (W.button(ui, ctx, 'sell-' + rid, x + leftW - 96 * s, ry + 8 * s, 76 * s, 22 * s, 'SELL', {
            size: 9.5 * s, primary: true, accent: '#ffd166'
          })) {
            var earned = SM.Profile.sellResource(pr, rid);
            if (earned) {
              game.toast('SOLD ' + res.name + ' FOR ' + U.fmtNum(earned) + ' CR', 'good', 2.4);
              ui.play('sell');
              game.save();
              game.checkAchievementsNow();
            }
          }
        }
      });
    }

    // right column: actions
    var rx = x + leftW + 14 * s, rw = w - leftW - 14 * s;
    W.header(ctx, rx, y, rw, 'STATION SERVICES', { accent: '#5fe6d8' });

    var bw = rw - 6 * s;
    var by = y + 44 * s;

    W.panel(ctx, rx, by, bw, 74 * s, { fill: 'rgba(255,209,102,0.06)', accent: 'rgba(255,209,102,0.55)', cut: 8 * s });
    Text.draw(ctx, 'TOTAL HOLD VALUE', rx + 14 * s, by + 20 * s, {
      font: Font.mono(8.5 * s, 600), color: Theme.dim, spacing: 1.6
    });
    Text.draw(ctx, U.fmtNum(total) + ' CR', rx + 14 * s, by + 46 * s, {
      font: Font.title(24 * s, 700), color: '#ffe9a8'
    });
    by += 84 * s;

    if (W.button(ui, ctx, 'sellall2', rx, by, bw, 42 * s, 'SELL ENTIRE HOLD', {
      primary: true, accent: '#ffd166', size: 12 * s, sub: U.fmtNum(total) + ' CR'
    })) {
      var earnedAll = SM.Profile.sellAll(pr);
      if (earnedAll > 0) {
        game.toast('HOLD SOLD — +' + U.fmtNum(earnedAll) + ' CR', 'good', 3);
        ui.play('sell');
        game.save();
        game.checkAchievementsNow();
      } else {
        ui.play('deny');
      }
    }
    by += 52 * s;

    // repair
    var missing = game.world ? (game.world.stats.maxHull - game.world.player.hull) : 0;
    var repairCost = Math.ceil(missing * 2.4);
    var canRepair = missing > 0.5 && pr.credits >= repairCost;
    if (W.button(ui, ctx, 'repair', rx, by, bw, 38 * s, missing > 0.5 ? 'REPAIR HULL' : 'HULL INTACT', {
      accent: '#7fe0a8', size: 11 * s, disabled: !canRepair,
      sub: missing > 0.5 ? repairCost + ' CR · ' + Math.ceil(missing) + ' HP' : 'NO DAMAGE'
    })) {
      pr.credits -= repairCost;
      game.world.player.hull = game.world.stats.maxHull;
      game.world.player.shield = game.world.stats.maxShield;
      game.toast('HULL REPAIRED', 'good', 2);
      ui.play('buy');
      game.save();
    }
    by += 46 * s;

    if (W.button(ui, ctx, 'refuel', rx, by, bw, 34 * s, 'RECHARGE REACTOR', {
      accent: '#ffcf5c', size: 10.5 * s, sub: 'FREE · DOCKING SERVICE'
    })) {
      if (game.world) game.world.player.energy = game.world.stats.maxEnergy;
      ui.play('buy');
    }
    by += 44 * s;

    // station flavour — the panel is sized to its content, no dead space
    var lines = Text.wrap(ctx, 'Traders here pay market rate for raw ore. Rare minerals unlock ' +
      'jump corridors: the chart on the next tab lists what each frontier sector demands.',
      bw - 32 * s, Font.body(10.5 * s));
    var bulletinH = 54 * s + lines.length * 16 * s;
    W.panel(ctx, rx, by, bw, bulletinH, { fill: 'rgba(255,255,255,0.02)', cut: 8 * s, border: 'rgba(90,130,160,0.25)' });
    Text.draw(ctx, 'STATION BULLETIN', rx + 16 * s, by + 22 * s, {
      font: Font.mono(8.5 * s, 600), color: Theme.dimmer, spacing: 1.8
    });
    for (var l = 0; l < lines.length; l++) {
      Text.draw(ctx, lines[l], rx + 16 * s, by + 44 * s + l * 16 * s, { font: Font.body(10.5 * s), color: Theme.inkSoft });
    }
  }

  /* human readable current effect for an upgrade at a level */
  function effectLine(u, lvl) {
    var base = { laserPower: 1, penetration: 1, laserRange: 1, cargoCap: 0, massFactor: 1, maxSpeed: 1,
      turnRate: 1, accel: 1, drag: 1, maxHull: 0, maxShield: 0, shieldRegen: 0, energyRegen: 1,
      maxEnergy: 1, energyCost: 1, scanRange: 1, rareFind: 0, magnetRadius: 1, collectSpeed: 1,
      weaponDamage: 1, weaponCooldown: 1, droneSlots: 0, dronePower: 1, droneCargo: 0, hullRepair: 0 };
    var before = {}, after = {};
    for (var k in base) { before[k] = base[k]; after[k] = base[k]; }
    if (lvl > 0) u.apply(before, lvl - 1);
    u.apply(after, lvl);
    var parts = [];
    for (var key in after) {
      var a = before[key], b = after[key];
      if (Math.abs(b - a) < 1e-6) continue;
      var label = key.replace(/([A-Z])/g, ' $1').toUpperCase();
      var fmt = function (v) {
        if (Math.abs(v) < 10 && v !== Math.round(v)) return v.toFixed(2);
        return Math.round(v * 10) / 10;
      };
      var arrow = b < a ? ' ▼' : ' ▲';
      parts.push(label + ' ' + fmt(a) + arrow + fmt(b));
    }
    if (!parts.length) return 'NO EFFECT APPLIED YET';
    return parts.slice(0, 2).join('   ');
  }

  /* ================= UPGRADES ================= */
  function drawUpgrades(game, ctx, ui, x, y, w, h) {
    var s = ui.s, pr = game.profile;
    W.header(ctx, x, y, w, 'UPGRADE GRID', {
      sub: U.fmtNum(pr.credits) + ' CR AVAILABLE', accent: '#5fe6d8'
    });

    var cats = ['ALL'].concat(SM.Upgrades.categories);
    var tx = x;
    for (var c = 0; c < cats.length; c++) {
      var label = cats[c];
      ctx.save();
      ctx.font = Font.mono(8.5 * s, 600);
      var tw = Text.measure(ctx, label, Font.mono(8.5 * s, 600), 1) + 20 * s;
      ctx.restore();
      if (W.button(ui, ctx, 'cat-' + label, tx, y + 34 * s, tw, 20 * s, label, {
        size: 8.5 * s, active: ui.station.cat === label, accent: '#5fe6d8', spacing: 1
      })) {
        ui.station.cat = label;
      }
      tx += tw + 6 * s;
    }

    var list = SM.Upgrades.list.filter(function (u) {
      return ui.station.cat === 'ALL' || u.cat === ui.station.cat;
    });
    var cols = w > 760 * s ? 3 : 2;
    var gap = 12 * s;
    var cardW = (w - gap * (cols - 1)) / cols;
    var cardH = 158 * s;
    var rows = Math.ceil(list.length / cols);

    W.scrollArea(ui, ctx, 'upg', x, y + 62 * s, w, h - 62 * s, rows * (cardH + gap) + 10 * s, function (ctx) {
      for (var i = 0; i < list.length; i++) {
        var u = list[i];
        var col = i % cols, row = Math.floor(i / cols);
        var bx = x + col * (cardW + gap);
        var by = y + 62 * s + row * (cardH + gap);
        var lvl = SM.Profile.upgradeLevel(pr, u.id);
        var maxed = lvl >= u.max;
        var cost = SM.Profile.upgradeCost(pr, u.id);
        var afford = pr.credits >= cost;
        var hover = W.inside(ui.mouse, bx, by, cardW, cardH);

        ctx.save();
        Path.cutRect(ctx, bx, by, cardW, cardH, 8 * s);
        var bg = ctx.createLinearGradient(bx, by, bx, by + cardH);
        bg.addColorStop(0, hover ? 'rgba(95,230,216,0.10)' : 'rgba(255,255,255,0.035)');
        bg.addColorStop(1, 'rgba(0,0,0,0.16)');
        ctx.fillStyle = bg;
        ctx.fill();
        ctx.strokeStyle = maxed ? 'rgba(255,209,102,0.5)' : hover ? 'rgba(95,230,216,0.7)' : 'rgba(100,140,170,0.3)';
        ctx.lineWidth = 1;
        ctx.stroke();
        ctx.restore();

        SM.Icons.glyph(ctx, u.icon, bx + 26 * s, by + 26 * s, 26 * s, maxed ? '#ffd166' : '#9fe8dd');
        Text.draw(ctx, u.name.toUpperCase(), bx + 48 * s, by + 22 * s, {
          font: Font.title(11 * s, 700), color: Theme.ink, spacing: 1.2
        });
        Text.draw(ctx, u.cat, bx + 48 * s, by + 35 * s, {
          font: Font.mono(7.5 * s), color: Theme.dimmer, spacing: 1.4
        });

        // level pips
        var pipW = (cardW - 34 * s - (u.max - 1) * 2 * s) / u.max;
        for (var lv = 0; lv < u.max; lv++) {
          ctx.fillStyle = lv < lvl ? '#5fe6d8' : 'rgba(255,255,255,0.12)';
          ctx.fillRect(bx + 17 * s + lv * (pipW + 2 * s), by + 44 * s, Math.max(1, pipW), 3 * s);
        }

        var wrapLines = Text.wrap(ctx, u.desc, cardW - 34 * s, Font.body(9.5 * s));
        for (var wl = 0; wl < Math.min(3, wrapLines.length); wl++) {
          Text.draw(ctx, wrapLines[wl], bx + 17 * s, by + 64 * s + wl * 12.5 * s, {
            font: Font.body(9.5 * s), color: Theme.dim
          });
        }
        // current effect readout
        Text.draw(ctx, effectLine(u, lvl), bx + 17 * s, by + 112 * s, {
          font: Font.mono(8.5 * s, 600), color: lvl > 0 ? '#9fe8dd' : Theme.dimmer, spacing: 0.6
        });

        Text.draw(ctx, 'LVL ' + lvl + ' / ' + u.max, bx + 17 * s, by + cardH - 14 * s, {
          font: Font.mono(8.5 * s, 600), color: maxed ? '#ffd166' : Theme.inkSoft, spacing: 1
        });

        if (!maxed) {
          if (W.button(ui, ctx, 'buy-' + u.id, bx + cardW - 96 * s, by + cardH - 34 * s, 78 * s, 24 * s,
            U.fmtNum(cost), { size: 9.5 * s, primary: afford, accent: '#ffd166', disabled: !afford })) {
            var r = SM.Profile.buyUpgrade(pr, u.id);
            if (r.ok) {
              game.refreshDrones();
              game.toast(u.name.toUpperCase() + ' → LVL ' + r.level, 'good', 2.4);
              ui.play('buy');
              game.save();
              game.checkAchievementsNow();
            } else {
              ui.play('deny');
            }
          }
        } else {
          Text.draw(ctx, 'MAX', bx + cardW - 17 * s, by + cardH - 14 * s, {
            font: Font.mono(10 * s, 700), color: '#ffd166', align: 'right', spacing: 2
          });
        }
      }
    });
  }

  /* ================= DRONES ================= */
  function drawDrones(game, ctx, ui, x, y, w, h) {
    var s = ui.s, pr = game.profile;
    var stats = SM.Profile.stats(pr);
    W.header(ctx, x, y, w, 'DRONE BAY', {
      sub: stats.droneSlots + ' CRADLES · ' + pr.drones.length + ' DEPLOYED', accent: '#5fe6d8'
    });

    var leftW = w * 0.56;
    var rowH = 76 * s;

    // drone illustration strip
    var art = SM.Art.drone();
    W.scrollArea(ui, ctx, 'drones', x, y + 38 * s, leftW, h - 130 * s,
      Math.max(pr.drones.length, 1) * rowH + 10 * s, function (ctx) {
        if (pr.drones.length === 0) {
          Text.draw(ctx, 'NO DRONES INSTALLED', x + 10 * s, y + 80 * s, {
            font: Font.title(12 * s, 700), color: Theme.dimmer, spacing: 2
          });
          Text.draw(ctx, 'Buy the DRONE BAY upgrade to field automated miners.', x + 10 * s, y + 102 * s, {
            font: Font.body(11 * s), color: Theme.dim
          });
        }
        for (var i = 0; i < pr.drones.length; i++) {
          var dr = pr.drones[i];
          var ry = y + 38 * s + i * rowH;
          ctx.save();
          Path.cutRect(ctx, x, ry, leftW - 10 * s, rowH - 8 * s, 7 * s);
          ctx.fillStyle = 'rgba(255,255,255,0.03)';
          ctx.fill();
          ctx.strokeStyle = 'rgba(95,230,216,0.2)';
          ctx.stroke();
          ctx.restore();
          // drone render
          ctx.save();
          ctx.translate(x + 46 * s, ry + (rowH - 8 * s) / 2);
          ctx.drawImage(art.canvas, -30 * s, -24 * s, 60 * s, 48 * s);
          ctx.restore();

          Text.draw(ctx, 'DRONE D-' + U.pad(dr.serial, 2), x + 86 * s, ry + 22 * s, {
            font: Font.title(12 * s, 700), color: Theme.ink, spacing: 1.5
          });
          Text.draw(ctx, 'AUTONOMOUS MINER · CLASS II', x + 86 * s, ry + 36 * s, {
            font: Font.mono(8 * s), color: Theme.dim, spacing: 1
          });
          var mined = dr.mined || 0;
          Text.draw(ctx, 'ORE DELIVERED', x + 86 * s, ry + 54 * s, {
            font: Font.mono(8 * s), color: Theme.dimmer, spacing: 1
          });
          Text.draw(ctx, U.fmtNum(mined) + ' u', x + 176 * s, ry + 54 * s, {
            font: Font.mono(10 * s, 600), color: '#ffd166'
          });
          Text.draw(ctx, 'BEAM ' + Math.round(7.6 * stats.dronePower * 10) / 10 + '/s', x + leftW - 24 * s, ry + 22 * s, {
            font: Font.mono(8.5 * s), color: Theme.inkSoft, align: 'right'
          });
          Text.draw(ctx, 'POD ' + Math.round(stats.droneCargo) + ' t', x + leftW - 24 * s, ry + 36 * s, {
            font: Font.mono(8.5 * s), color: Theme.inkSoft, align: 'right'
          });
          Text.draw(ctx, game.world && game.world.droneAuto ? 'AUTO-MINING' : 'RECALLED', x + leftW - 24 * s, ry + 54 * s, {
            font: Font.mono(8.5 * s, 700), color: game.world && game.world.droneAuto ? '#7fe0a8' : '#ff8f9b', align: 'right', spacing: 1
          });
        }
      });

    // right side: drone upgrades
    var rx = x + leftW + 16 * s, rw = w - leftW - 16 * s;
    W.header(ctx, rx, y + 38 * s, rw, 'FLEET DOCTRINE', { accent: '#ffd166' });
    var by = y + 82 * s;

    var autonomy = game.world ? game.world.droneAuto : true;
    if (W.button(ui, ctx, 'auto', rx, by, rw, 36 * s, autonomy ? 'AUTONOMY: ENGAGED' : 'AUTONOMY: RECALLED', {
      active: autonomy, accent: '#5fe6d8', size: 10.5 * s,
      sub: autonomy ? 'DRONES MINE AND RETURN AUTOMATICALLY' : 'DRONES HOLD FORMATION WITH THE SHIP'
    })) {
      if (game.world) game.world.droneAuto = !game.world.droneAuto;
      ui.play('uiClick');
    }
    by += 46 * s;

    Text.draw(ctx, 'Drones seek the nearest asteroid, cut it with their own beam,', rx, by, {
      font: Font.body(10.5 * s), color: Theme.dim
    });
    Text.draw(ctx, 'scoop the ore into their pod and fly it back to your hold.', rx, by + 16 * s, {
      font: Font.body(10.5 * s), color: Theme.dim
    });
    by += 40 * s;

    ['droneBay', 'dronePower'].forEach(function (uid) {
      var u = SM.Upgrades.get(uid);
      var lvl = SM.Profile.upgradeLevel(pr, uid);
      var cost = SM.Profile.upgradeCost(pr, uid);
      var maxed = lvl >= u.max;
      W.panel(ctx, rx, by, rw, 76 * s, { cut: 8 * s, fill: 'rgba(255,255,255,0.03)' });
      SM.Icons.glyph(ctx, u.icon, rx + 26 * s, by + 26 * s, 26 * s, '#9fe8dd');
      Text.draw(ctx, u.name.toUpperCase(), rx + 48 * s, by + 22 * s, {
        font: Font.title(10.5 * s, 700), color: Theme.ink, spacing: 1.2
      });
      Text.draw(ctx, 'LVL ' + lvl + '/' + u.max, rx + 48 * s, by + 36 * s, {
        font: Font.mono(8 * s), color: Theme.dim
      });
      var lw = Text.wrap(ctx, u.desc, rw - 60 * s, Font.body(9 * s));
      Text.draw(ctx, lw[0], rx + 14 * s, by + 56 * s, { font: Font.body(9 * s), color: Theme.dim });
      if (!maxed) {
        if (W.button(ui, ctx, 'dbuy-' + uid, rx + rw - 92 * s, by + 12 * s, 78 * s, 22 * s,
          U.fmtNum(cost), { size: 9.5 * s, primary: true, accent: '#ffd166', disabled: pr.credits < cost })) {
          var r = SM.Profile.buyUpgrade(pr, uid);
          if (r.ok) {
            game.refreshDrones();
            ui.play('buy');
            game.toast(u.name.toUpperCase() + ' UPGRADED', 'good', 2);
            game.save();
          } else ui.play('deny');
        }
      } else {
        Text.draw(ctx, 'MAX', rx + rw - 14 * s, by + 26 * s, {
          font: Font.mono(9 * s, 700), color: '#ffd166', align: 'right'
        });
      }
      by += 84 * s;
    });
  }

  /* ================= SHIPYARD ================= */
  function drawShipyard(game, ctx, ui, x, y, w, h) {
    var s = ui.s, pr = game.profile;
    W.header(ctx, x, y, w, 'SHIPYARD', {
      sub: 'PILOT LVL ' + pr.level + ' · ' + U.fmtNum(pr.credits) + ' CR', accent: '#ffb257'
    });

    var ships = SM.Ships.list;
    var cardH = Math.min(190 * s, (h - 80 * s) / 2 - 12 * s);
    var gap = 14 * s;
    var cardW = (w - gap) / 2;
    var rows = Math.ceil(ships.length / 2);

    W.scrollArea(ui, ctx, 'yard', x, y + 36 * s, w, h - 36 * s, rows * (cardH + gap), function (ctx) {
      for (var i = 0; i < ships.length; i++) {
        var sh = ships[i];
        var col = i % 2, row = Math.floor(i / 2);
        var bx = x + col * (cardW + gap), by = y + 36 * s + row * (cardH + gap);
        var owned = SM.Profile.shipOwned(pr, sh.id);
        var selected = pr.shipId === sh.id;
        var hover = W.inside(ui.mouse, bx, by, cardW, cardH);

        ctx.save();
        Path.cutRect(ctx, bx, by, cardW, cardH, 9 * s);
        var bg = ctx.createLinearGradient(bx, by, bx, by + cardH);
        bg.addColorStop(0, hover ? 'rgba(255,178,87,0.10)' : 'rgba(255,255,255,0.03)');
        bg.addColorStop(1, 'rgba(0,0,0,0.2)');
        ctx.fillStyle = bg;
        ctx.fill();
        ctx.strokeStyle = selected ? 'rgba(95,230,216,0.8)' : hover ? 'rgba(255,178,87,0.6)' : 'rgba(100,140,170,0.3)';
        ctx.lineWidth = selected ? 1.6 : 1;
        ctx.stroke();
        ctx.restore();

        // ship preview
        var art = SM.Art.ship(sh.id);
        var scale = Math.min((cardW * 0.52) / art.w, (cardH * 0.5) / art.h);
        ctx.save();
        ctx.globalAlpha = pr.ships.indexOf(sh.id) >= 0 ? 1 : 0.45;
        var bob = Math.sin(ui.t * 1.4 + i) * 3 * s;
        ctx.drawImage(art.canvas, bx + cardW * 0.28 - art.w * scale / 2, by + cardH * 0.42 - art.h * scale / 2 + bob,
          art.w * scale, art.h * scale);
        ctx.restore();

        var tx = bx + cardW * 0.54;
        Text.draw(ctx, sh.name, tx, by + 26 * s, {
          font: Font.title(12 * s, 700), color: selected ? '#5fe6d8' : Theme.ink, spacing: 1.4
        });
        Text.draw(ctx, sh.role.toUpperCase(), tx, by + 40 * s, {
          font: Font.mono(7.5 * s), color: Theme.dimmer, spacing: 1.4
        });

        var stats = [
          ['CARGO', sh.stats.cargoCap, 220, '#5fe6d8'],
          ['SPEED', sh.stats.maxSpeed, 420, '#8ec6ff'],
          ['HULL', sh.stats.maxHull, 320, '#7fe0a8'],
          ['WEAPON', sh.stats.weaponDamage, 32, '#ff8f9b']
        ];
        for (var k = 0; k < stats.length; k++) {
          var sy = by + 54 * s + k * 16 * s;
          Text.draw(ctx, stats[k][0], tx, sy + 6 * s, {
            font: Font.mono(7.5 * s), color: Theme.dim, spacing: 1
          });
          W.bar(ctx, tx + 46 * s, sy, cardW * 0.42 - 52 * s, 5 * s,
            Math.min(1, stats[k][1] / stats[k][2]), stats[k][3], { cut: 2 });
        }

        var descLines = Text.wrap(ctx, sh.desc, cardW * 0.44 - 10 * s, Font.body(8.5 * s));
        for (var d = 0; d < Math.min(3, descLines.length); d++) {
          Text.draw(ctx, descLines[d], tx, by + cardH - 42 * s + d * 11 * s, {
            font: Font.body(8.5 * s), color: Theme.dim
          });
        }

        if (selected) {
          Text.draw(ctx, 'ACTIVE VESSEL', bx + cardW - 14 * s, by + 22 * s, {
            font: Font.mono(9 * s, 700), color: '#5fe6d8', align: 'right', spacing: 1.4
          });
        } else if (owned) {
          if (W.button(ui, ctx, 'select-' + sh.id, bx + cardW - 104 * s, by + cardH - 30 * s, 92 * s, 22 * s, 'SELECT', {
            size: 9 * s, accent: '#5fe6d8'
          })) {
            SM.Profile.selectShip(pr, sh.id);
            game.refreshDrones();
            game.toast(sh.name + ' IS NOW YOUR VESSEL', 'good', 3);
            ui.play('buy');
            game.save();
          }
        } else {
          var price = sh.cost;
          var levelOk = pr.level >= sh.levelReq;
          var canBuy = pr.credits >= price && levelOk;
          if (W.button(ui, ctx, 'buyship-' + sh.id, bx + cardW - 118 * s, by + cardH - 30 * s, 106 * s, 22 * s,
            levelOk ? U.fmtNum(price) + ' CR' : 'LVL ' + sh.levelReq, {
            size: 9 * s, accent: '#ffd166', disabled: !canBuy, primary: canBuy
          })) {
            var rr = SM.Profile.buyShip(pr, sh.id);
            if (rr.ok) {
              game.toast(sh.name + ' PURCHASED — SELECT IT TO FLY', 'good', 4);
              ui.play('buy');
              game.save();
              game.checkAchievementsNow();
            } else ui.play('deny');
          }
        }
      }
    });
  }

  /* ================= SECTORS ================= */
  function drawSectors(game, ctx, ui, x, y, w, h) {
    var s = ui.s, pr = game.profile;
    W.header(ctx, x, y, w, 'SECTOR CHART', {
      sub: pr.sectors.length + ' / 5 CORRIDORS OPEN', accent: '#5fe6d8'
    });

    var list = SM.Sectors.list;
    var rowH = 110 * s;

    W.scrollArea(ui, ctx, 'sectors', x, y + 36 * s, w, h - 36 * s, list.length * (rowH + 10 * s) + 10 * s, function (ctx) {
      for (var i = 0; i < list.length; i++) {
        var sec = list[i];
        var ry = y + 36 * s + i * (rowH + 10 * s);
        var unlocked = SM.Profile.sectorUnlocked(pr, sec.id);
        var here = pr.sector === sec.id;
        var hover = W.inside(ui.mouse, x, ry, w, rowH);

        ctx.save();
        Path.cutRect(ctx, x, ry, w, rowH, 9 * s);
        var bg = ctx.createLinearGradient(x, ry, x + w, ry);
        bg.addColorStop(0, here ? hexA(sec.palette.accent, 0.16) : hover ? 'rgba(255,255,255,0.06)' : 'rgba(255,255,255,0.025)');
        bg.addColorStop(1, 'rgba(0,0,0,0.18)');
        ctx.fillStyle = bg;
        ctx.fill();
        ctx.strokeStyle = here ? hexA(sec.palette.accent, 0.8) : unlocked ? 'rgba(120,180,210,0.35)' : 'rgba(255,255,255,0.12)';
        ctx.lineWidth = 1;
        ctx.stroke();
        // sector colour chip
        ctx.fillStyle = hexA(sec.palette.accent, 0.9);
        ctx.fillRect(x + 1, ry + 12 * s, 3, rowH - 24 * s);
        ctx.restore();

        // code + name
        Text.draw(ctx, sec.code, x + 18 * s, ry + 24 * s, {
          font: Font.mono(9 * s, 600), color: hexA(sec.palette.accent, 0.9), spacing: 2
        });
        Text.draw(ctx, sec.name, x + 18 * s, ry + 46 * s, {
          font: Font.title(16 * s, 700), color: unlocked ? Theme.ink : Theme.dimmer, spacing: 2.4
        });
        Text.draw(ctx, sec.subtitle, x + 18 * s, ry + 62 * s, {
          font: Font.mono(8.5 * s), color: Theme.dim, spacing: 1
        });

        // danger
        for (var d = 0; d < 5; d++) {
          ctx.fillStyle = d < sec.danger ? hexA('#ff6b6b', 0.85) : 'rgba(255,255,255,0.10)';
          ctx.fillRect(x + 18 * s + d * 9 * s, ry + 74 * s, 6 * s, 10 * s);
        }

        // resource chips
        var rids = Object.keys(sec.asteroid.mineralWeights).slice(0, 5);
        var chX = x + 18 * s;
        for (var r = 0; r < rids.length; r++) {
          var res = SM.Resources.get(rids[r]);
          if (!res) continue;
          var label = res.code;
          ctx.font = Font.mono(7.5 * s, 600);
          var chipW = Text.measure(ctx, label, Font.mono(7.5 * s, 600), 0.8) + 12 * s;
          ctx.save();
          Path.cutRect(ctx, chX, ry + 90 * s, chipW, 13 * s, 2);
          ctx.fillStyle = hexA(res.color, 0.14);
          ctx.fill();
          ctx.strokeStyle = hexA(res.color, 0.45);
          ctx.stroke();
          Text.draw(ctx, label, chX + chipW / 2, ry + 99.5 * s, {
            font: Font.mono(7.5 * s, 600), color: res.color, align: 'center', spacing: 0.8
          });
          ctx.restore();
          chX += chipW + 5 * s;
        }

        // right side actions
        var ax = x + w - 210 * s;
        if (here) {
          Text.draw(ctx, 'CURRENT SECTOR', x + w - 18 * s, ry + 28 * s, {
            font: Font.mono(9 * s, 700), color: hexA(sec.palette.accent, 0.95), align: 'right', spacing: 1.6
          });
        } else if (unlocked) {
          if (W.button(ui, ctx, 'travel-' + sec.id, ax, ry + 30 * s, 192 * s, 34 * s, 'JUMP TO SECTOR', {
            primary: true, accent: sec.palette.accent, size: 10.5 * s, sub: 'DRIVE CHARGE READY'
          })) {
            game.undock();
            game.travelTo(sec.id);
            ui.play('warp');
            return;
          }
        } else {
          var check = SM.Profile.canUnlock(pr, sec.id);
          var reqY = ry + 16 * s;
          Text.draw(ctx, 'LOCKED', x + w - 18 * s, reqY + 6 * s, {
            font: Font.mono(8.5 * s, 700), color: '#ff8f9b', align: 'right', spacing: 2
          });
          var reqLines = [];
          reqLines.push(sec.unlock.credits + ' CR  ' + (pr.credits >= sec.unlock.credits ? '✓' : '✗'));
          for (var rid in sec.unlock.deliver) {
            var need = sec.unlock.deliver[rid];
            var have = pr.cargo[rid] | 0;
            reqLines.push(need + ' ' + SM.Resources.get(rid).code + '  ' + have + '/' + need);
          }
          for (var q = 0; q < reqLines.length; q++) {
            Text.draw(ctx, reqLines[q], x + w - 130 * s, ry + 26 * s + q * 14 * s, {
              font: Font.mono(8.5 * s), color: q === 0 ? (pr.credits >= sec.unlock.credits ? '#7fe0a8' : '#ffb0b8') : Theme.inkSoft,
              align: 'left'
            });
          }
          if (W.button(ui, ctx, 'unlock-' + sec.id, x + w - 126 * s, ry + 74 * s, 108 * s, 24 * s,
            check.ok ? 'UNLOCK' : 'REQUIRES', {
            size: 9 * s, accent: '#ffd166', primary: check.ok && !check.ok ? false : true, disabled: !check.ok
          })) {
            var ur = SM.Profile.unlockSector(pr, sec.id);
            if (ur.ok) {
              game.toast('CORRIDOR OPENED — ' + sec.name, 'level', 4);
              ui.play('levelUp');
              game.save();
              game.checkAchievementsNow();
            } else ui.play('deny');
          }
        }
      }
    });
  }

  SM.StationUI = { draw: draw, open: open, TABS: TABS };
})(typeof window !== 'undefined' ? window : globalThis);
