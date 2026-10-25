/* =========================================================================
   SPACE MINER — in-flight HUD (instrument panels, radar, toasts, prompts)
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, W = SM.W, Theme = SM.Theme, Font = SM.Font, Text = SM.Text, hexA = U.hexA;
  var Path = SM.Path;

  function draw(game, ctx, ui) {
    var world = game.world;
    if (!world) return;
    var w = ui.w, h = ui.h;
    var s = ui.s;
    var p = world.player;
    var st = world.stats;
    var pal = world.sector.palette;

    drawVitals(ctx, game, ui, world, p, st, pal);
    drawTopRight(ctx, game, ui, world, pal);
    drawRadar(ctx, game, ui, world, pal);
    drawDroneRoster(ctx, game, ui, world, pal);
    drawObjective(ctx, game, ui, world, pal);
    drawToasts(ctx, game, ui);
    drawPrompt(ctx, game, ui, world);
    drawWarnings(ctx, game, ui, world, p, st);
    drawCargoPanel(ctx, game, ui, world);
    drawSectorBanner(ctx, game, ui, world);
  }

  /* ---------------- vitals (top left) ---------------- */
  function drawVitals(ctx, game, ui, world, p, st, pal) {
    var s = ui.s;
    var x = 26 * s, y = 24 * s, w = 322 * s, h = 118 * s;
    W.panel(ctx, x, y, w, h, { fill: 'rgba(6,14,22,0.78)', accent: hexA(pal.accent2, 0.85), cut: 14 * s });

    var pad = 16 * s;
    var bx = x + pad + 22 * s, bw = w - pad * 2 - 84 * s;
    var hullPct = p.hull / Math.max(1, st.maxHull);
    var rows = [
      { icon: 'hull', label: 'HULL', val: p.hull, max: st.maxHull, pct: hullPct, color: hullPct > 0.55 ? '#7fe0a8' : hullPct > 0.28 ? '#ffd166' : '#ff5f6d' },
      { icon: 'shield', label: 'SHLD', val: p.shield, max: st.maxShield, pct: p.shield / Math.max(1, st.maxShield), color: '#6cc7ff' },
      { icon: 'energy', label: 'ENRG', val: p.energy, max: st.maxEnergy, pct: p.energy / Math.max(1, st.maxEnergy), color: '#ffcf5c' }
    ];
    for (var i = 0; i < rows.length; i++) {
      var r = rows[i];
      var ry = y + 22 * ui.s + i * 26 * ui.s;
      SM.Icons.hud(ctx, r.icon, x + pad + 8 * ui.s, ry + 4 * ui.s, 15 * ui.s, hexA(r.color, 0.9));
      Text.draw(ctx, r.label, x + pad + 22 * ui.s, ry + 3.5 * ui.s, {
        font: Font.mono(9 * ui.s, 600), color: Theme.dim, spacing: 1.4
      });
      W.bar(ctx, bx, ry - 3.5 * ui.s, bw, 8 * ui.s, r.pct, r.color, { cut: 2 });
      Text.draw(ctx, Math.round(r.val) + '/' + Math.round(r.max), x + w - pad, ry + 3.5 * ui.s, {
        font: Font.mono(9.5 * ui.s, 600), color: Theme.inkSoft, align: 'right'
      });
    }

    // cargo
    var used = SM.Profile.cargoUsed(game.profile);
    var cap = SM.Profile.cargoCap(game.profile);
    var cpct = used / Math.max(1, cap);
    var cy = y + h - 26 * ui.s;
    Text.draw(ctx, 'CARGO', x + pad + 22 * ui.s, cy + 3.5 * ui.s, {
      font: Font.mono(9 * ui.s, 600), color: Theme.dim, spacing: 1.4
    });
    SM.Icons.hud(ctx, 'cargo', x + pad + 8 * ui.s, cy + 1 * ui.s, 14 * ui.s, hexA(cpct > 0.9 ? '#ff5f6d' : pal.accent2, 0.9));
    W.bar(ctx, bx, cy - 3.5 * ui.s, bw, 8 * ui.s, cpct, cpct > 0.9 ? '#ff5f6d' : cpct > 0.7 ? '#ffb257' : pal.accent2, { cut: 2 });
    Text.draw(ctx, used.toFixed(0) + '/' + cap.toFixed(0) + ' t', x + w - pad, cy + 3.5 * ui.s, {
      font: Font.mono(9.5 * ui.s, 600), color: Theme.inkSoft, align: 'right'
    });
  }

  /* ---------------- top right ---------------- */
  function drawTopRight(ctx, game, ui, world, pal) {
    var s = ui.s;
    var w = ui.w;
    var x = w - 26 * s, y = 24 * s;
    var panelW = 268 * s, panelH = 96 * s;
    var px = x - panelW;
    W.panel(ctx, px, y, panelW, panelH, { fill: 'rgba(6,14,22,0.78)', accent: hexA(pal.accent, 0.85), cut: 14 * s });

    var pad = 16 * s;
    // credits
    SM.Icons.hud(ctx, 'credits', px + pad + 7 * s, y + 28 * s, 16 * s, '#ffd166');
    Text.draw(ctx, U.fmtNum(game.profile.credits), px + pad + 22 * s, y + 34 * s, {
      font: Font.title(20 * s, 700), color: '#ffe9a8', spacing: 0.8
    });
    Text.draw(ctx, 'CR', px + panelW - pad, y + 34 * s, {
      font: Font.mono(10 * s), color: Theme.dim, align: 'right', spacing: 1.2
    });

    // level + xp
    var lp = SM.Profile.levelProgress(game.profile);
    var ly = y + 56 * s;
    Text.draw(ctx, 'LVL ' + game.profile.level, px + pad, ly + 3 * s, {
      font: Font.mono(10 * s, 700), color: pal.accent2, spacing: 1.2
    });
    W.bar(ctx, px + pad + 52 * s, ly - 4 * s, panelW - pad * 2 - 52 * s, 6 * s, lp.pct, pal.accent2, { cut: 2 });
    Text.draw(ctx, U.fmtNum(lp.cur) + ' / ' + U.fmtNum(lp.need), px + panelW - pad, ly + 3 * s, {
      font: Font.mono(8.5 * s), color: Theme.dimmer, align: 'right'
    });

    // sector + danger
    var sy = y + panelH + 12 * s;
    W.panel(ctx, px, sy, panelW, 44 * s, { fill: 'rgba(6,14,22,0.72)', accent: hexA(pal.accent, 0.6), cut: 10 * s });
    Text.draw(ctx, world.sector.name, px + pad, sy + 20 * s, {
      font: Font.title(12.5 * s, 700), color: Theme.ink, spacing: 1.8
    });
    Text.draw(ctx, world.sector.code, px + pad, sy + 34 * s, {
      font: Font.mono(8.5 * s), color: Theme.dim, spacing: 1.4
    });
    // danger pips
    for (var i = 0; i < 5; i++) {
      var dx = px + panelW - pad - (4 - i) * 11 * s;
      ctx.save();
      Path.cutRect(ctx, dx, sy + 14 * s, 7 * s, 14 * s, 1.5);
      ctx.fillStyle = i < world.sector.danger ? hexA('#ff6b6b', 0.85) : 'rgba(255,255,255,0.10)';
      ctx.fill();
      ctx.restore();
    }
    Text.draw(ctx, 'DANGER', px + panelW - pad, sy + 36 * s, {
      font: Font.mono(7.5 * s), color: Theme.dimmer, align: 'right', spacing: 1
    });
  }

  /* ---------------- radar (bottom left) ---------------- */
  function drawRadar(ctx, game, ui, world, pal) {
    var s = ui.s;
    var size = 178 * s;
    var x = 26 * s, y = ui.h - size - 26 * s;
    var cx = x + size / 2, cy = y + size / 2;
    var R = size / 2 - 6 * s;

    ctx.save();
    ctx.beginPath(); ctx.arc(cx, cy, R + 4 * s, 0, 6.2832);
    ctx.fillStyle = 'rgba(4,10,16,0.82)';
    ctx.fill();
    ctx.strokeStyle = hexA(pal.accent2, 0.5);
    ctx.lineWidth = 1.2;
    ctx.stroke();
    ctx.beginPath(); ctx.arc(cx, cy, R, 0, 6.2832); ctx.clip();

    // grid
    ctx.strokeStyle = hexA(pal.accent2, 0.10);
    ctx.lineWidth = 1;
    for (var g = 1; g <= 3; g++) {
      ctx.beginPath(); ctx.arc(cx, cy, R * g / 3, 0, 6.2832); ctx.stroke();
    }
    ctx.beginPath();
    ctx.moveTo(cx - R, cy); ctx.lineTo(cx + R, cy);
    ctx.moveTo(cx, cy - R); ctx.lineTo(cx, cy + R);
    ctx.stroke();

    var range = 1800;
    var scaleR = R / range;
    function blip(wx, wy, col, r, glow) {
      var dx = (wx - world.player.x) * scaleR;
      var dy = (wy - world.player.y) * scaleR;
      if (dx * dx + dy * dy > R * R) return;
      if (glow) {
        ctx.globalCompositeOperation = 'lighter';
        var gg = ctx.createRadialGradient(cx + dx, cy + dy, 0, cx + dx, cy + dy, r * 3);
        gg.addColorStop(0, hexA(col, 0.55));
        gg.addColorStop(1, hexA(col, 0));
        ctx.fillStyle = gg;
        ctx.beginPath(); ctx.arc(cx + dx, cy + dy, r * 3, 0, 6.2832); ctx.fill();
        ctx.globalCompositeOperation = 'source-over';
      }
      ctx.fillStyle = col;
      ctx.beginPath(); ctx.arc(cx + dx, cy + dy, r, 0, 6.2832); ctx.fill();
    }

    for (var i = 0; i < world.asteroids.length; i++) {
      var a = world.asteroids[i];
      var res = SM.Resources.get(a.resourceId);
      if (res.tier >= 7) {
        blip(a.x, a.y, hexA(res.glow, 0.95), 2.6 * s, true);
      } else if (i % 2 === 0) {
        blip(a.x, a.y, hexA(res.color, 0.30), 1.2 * s, false);
      }
    }
    for (var d = 0; d < world.drones.length; d++) {
      blip(world.drones[d].x, world.drones[d].y, '#5fe6d8', 2.2 * s, true);
    }
    for (var e = 0; e < world.enemies.length; e++) {
      blip(world.enemies[e].x, world.enemies[e].y, '#ff5f6d', 2.6 * s, true);
    }
    blip(world.station.x, world.station.y, '#ffb257', 3.6 * s, true);

    // sweep
    var sweep = (world.time * 1.2) % 6.2832;
    var sg = ctx.createLinearGradient(cx, cy, cx + Math.cos(sweep) * R, cy + Math.sin(sweep) * R);
    sg.addColorStop(0, hexA(pal.accent2, 0.22));
    sg.addColorStop(1, hexA(pal.accent2, 0));
    ctx.strokeStyle = sg;
    ctx.lineWidth = 2.5;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.cos(sweep) * R, cy + Math.sin(sweep) * R);
    ctx.stroke();

    // player
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(world.player.angle);
    ctx.fillStyle = '#ffffff';
    ctx.beginPath();
    ctx.moveTo(7 * s, 0); ctx.lineTo(-4 * s, -4 * s); ctx.lineTo(-2 * s, 0); ctx.lineTo(-4 * s, 4 * s);
    ctx.closePath(); ctx.fill();
    ctx.restore();

    ctx.restore();

    // label
    Text.draw(ctx, 'SECTOR SCAN', x + 4 * s, y - 8 * s, {
      font: Font.mono(8.5 * s, 600), color: Theme.dim, spacing: 2
    });
    Text.draw(ctx, 'M · CHART', x + size - 4 * s, y - 8 * s, {
      font: Font.mono(8.5 * s, 600), color: hexA(pal.accent, 0.8), align: 'right', spacing: 1.4
    });
  }

  /* ---------------- drones roster (bottom right) ---------------- */
  function drawDroneRoster(ctx, game, ui, world, pal) {
    var s = ui.s;
    var w = 232 * s;
    var rows = world.drones.length;
    var h = (34 + rows * 22) * s;
    var x = ui.w - w - 26 * s, y = ui.h - h - 26 * s;
    W.panel(ctx, x, y, w, h, { fill: 'rgba(6,14,22,0.76)', accent: hexA(pal.accent2, 0.6), cut: 12 * s });
    Text.draw(ctx, 'MINING DRONES', x + 14 * s, y + 20 * s, {
      font: Font.mono(9 * s, 700), color: Theme.inkSoft, spacing: 1.8
    });

    // autonomy toggle
    var tW = 68 * s, tX = x + w - tW - 12 * s, tY = y + 9 * s;
    var hover = W.inside(ui.mouse, tX, tY, tW, 16 * s);
    if (hover && ui.click) {
      ui.consume();
      world.droneAuto = !world.droneAuto;
      ui.play('uiClick');
      game.toast(world.droneAuto ? 'DRONES: AUTONOMOUS' : 'DRONES: RECALLED', 'info', 2);
    }
    ctx.save();
    Path.cutRect(ctx, tX, tY, tW, 16 * s, 3);
    ctx.fillStyle = world.droneAuto ? hexA(pal.accent2, 0.2) : 'rgba(255,95,109,0.16)';
    ctx.fill();
    ctx.strokeStyle = world.droneAuto ? hexA(pal.accent2, 0.7) : 'rgba(255,95,109,0.6)';
    ctx.lineWidth = 1;
    ctx.stroke();
    Text.draw(ctx, world.droneAuto ? 'AUTO' : 'RECALL', tX + tW / 2, tY + 11.5 * s, {
      font: Font.mono(8.5 * s, 700), color: world.droneAuto ? pal.accent2 : '#ff8f9b', align: 'center', spacing: 1
    });
    ctx.restore();

    for (var i = 0; i < world.drones.length; i++) {
      var dr = world.drones[i];
      var ry = y + (30 + i * 22) * s;
      var cap = SM.Ent.droneCapacity(world);
      var pct = U.clamp(dr.cargoUsed / Math.max(1, cap), 0, 1);
      var stateCol = dr.state === 'mine' ? '#7fe0a8' : dr.state === 'return' ? '#ffd166' : dr.beam ? pal.accent2 : Theme.dim;
      Text.draw(ctx, 'D-' + U.pad(dr.serial, 2), x + 14 * s, ry + 13 * s, {
        font: Font.mono(9.5 * s, 700), color: Theme.ink
      });
      Text.draw(ctx, dr.state.toUpperCase(), x + 56 * s, ry + 13 * s, {
        font: Font.mono(8 * s), color: stateCol, spacing: 0.8
      });
      W.bar(ctx, x + 128 * s, ry + 4 * s, 86 * s, 6 * s, pct, pct > 0.85 ? '#ffb257' : pal.accent2, { cut: 2 });
    }
    if (world.drones.length === 0) {
      Text.draw(ctx, 'NO DRONES — BUY A DRONE BAY', x + 14 * s, y + 42 * s, {
        font: Font.mono(8.5 * s), color: Theme.dimmer
      });
    }
  }

  /* ---------------- objective strip ---------------- */
  function nextObjective(game) {
    var pr = game.profile;
    var sectors = SM.Sectors.list;
    for (var i = 0; i < sectors.length; i++) {
      if (pr.sectors.indexOf(sectors[i].id) < 0) {
        var sec = sectors[i];
        var parts = [];
        if (sec.unlock.credits > 0) parts.push(U.fmtNum(sec.unlock.credits) + ' CR');
        for (var rid in sec.unlock.deliver) {
          parts.push(sec.unlock.deliver[rid] + ' ' + (SM.Resources.get(rid) || { code: rid }).code);
        }
        return {
          title: 'NEXT: UNLOCK ' + sec.name,
          detail: parts.join('  +  '),
          pct: U.clamp(pr.credits / Math.max(1, sec.unlock.credits), 0, 1)
        };
      }
    }
    return { title: 'FRONTIER MASTERED', detail: 'MAXIMISE YOUR FLEET AND UPGRADES', pct: 1 };
  }

  function drawObjective(ctx, game, ui, world, pal) {
    var s = ui.s;
    var obj = nextObjective(game);
    var w = 360 * s, h = 46 * s;
    var x = ui.w / 2 - w / 2, y = 24 * s;
    ctx.save();
    ctx.globalAlpha = 0.92;
    W.panel(ctx, x, y, w, h, { fill: 'rgba(5,12,20,0.7)', accent: hexA(pal.accent, 0.5), cut: 10 * s });
    Text.draw(ctx, obj.title, x + w / 2, y + 19 * s, {
      font: Font.title(10.5 * s, 700), color: pal.accent, align: 'center', spacing: 2
    });
    Text.draw(ctx, obj.detail, x + w / 2, y + 34 * s, {
      font: Font.mono(9 * s), color: Theme.inkSoft, align: 'center', spacing: 1
    });
    ctx.restore();
  }

  /* ---------------- toasts ---------------- */
  function drawToasts(ctx, game, ui) {
    var s = ui.s;
    var list = [];
    for (var i = 0; i < game.toasts.length; i++) list.push({ t: game.toasts[i] });
    if (game.world && game.world.toasts.length) {
      for (var j = 0; j < game.world.toasts.length; j++) list.push({ t: game.world.toasts[j] });
    }
    list = list.slice(-5);

    var y = ui.h - 150 * s;
    for (var k = list.length - 1; k >= 0; k--) {
      var entry = list[k];
      var t = entry.t;
      var a = U.clamp(t.life / Math.min(1, t.maxLife) * 1.4, 0, 1);
      if (t.life > t.maxLife - 0.25) a = (t.maxLife - t.life) / 0.25;
      var kindCol = t.kind === 'warn' ? '#ff6b6b' : t.kind === 'good' ? '#7fe0a8' :
        t.kind === 'level' ? '#ffd166' : t.kind === 'achievement' ? '#ffd166' :
          t.kind === 'ore' ? '#9ff0ff' : t.kind === 'tip' ? '#8ec6ff' : '#a8c4d6';
      ctx.save();
      ctx.globalAlpha = U.clamp(a, 0, 1);
      ctx.font = Font.mono(10.5 * s, 600);
      var tw = Math.min(ui.w * 0.6, Text.measure(ctx, t.text, Font.mono(10.5 * s, 600), 0.6) + 30 * s);
      var tx = ui.w / 2 - tw / 2;
      Path.cutRect(ctx, tx, y - 13 * s, tw, 24 * s, 5 * s);
      ctx.fillStyle = 'rgba(4,10,16,0.72)';
      ctx.fill();
      ctx.strokeStyle = hexA(kindCol, 0.35);
      ctx.lineWidth = 1;
      ctx.stroke();
      ctx.fillStyle = hexA(kindCol, 0.9);
      ctx.fillRect(tx + 4 * s, y - 8 * s, 2 * s, 14 * s);
      Text.draw(ctx, t.text, ui.w / 2, y + 3.5 * s, {
        font: Font.mono(10.5 * s, 600), color: kindCol, align: 'center', spacing: 0.6
      });
      ctx.restore();
      y -= 28 * s;
    }
  }

  /* ---------------- docking prompt ---------------- */
  function drawPrompt(ctx, game, ui, world) {
    var s = ui.s;
    if (game.state !== 'flight') return;
    if (!world.canDock()) return;
    var x = ui.w / 2, y = ui.h - 196 * s;
    var pulse = 0.65 + 0.35 * Math.sin(world.time * 4);
    ctx.save();
    ctx.globalAlpha = pulse;
    var label = 'PRESS  E  TO DOCK AT ' + world.station.name;
    ctx.font = Font.title(12 * s, 700);
    var tw = Text.measure(ctx, label, Font.title(12 * s, 700), 2) + 40 * s;
    W.panel(ctx, x - tw / 2, y - 18 * s, tw, 34 * s, { fill: 'rgba(30,20,6,0.75)', accent: '#ffb257', cut: 8 * s });
    Text.draw(ctx, label, x, y + 5 * s, {
      font: Font.title(12 * s, 700), color: '#ffd8a0', align: 'center', spacing: 2
    });
    ctx.restore();
  }

  /* ---------------- warnings ---------------- */
  function drawWarnings(ctx, game, ui, world, p, st) {
    var s = ui.s;
    var msgs = [];
    if (p.hull / Math.max(1, st.maxHull) < 0.3) msgs.push(['HULL CRITICAL', '#ff5f6d']);
    if (p.energy < st.maxEnergy * 0.12) msgs.push(['ENERGY DEPLETED', '#ffcf5c']);
    if (SM.Profile.cargoFree(game.profile) < 1) msgs.push(['CARGO HOLD FULL', '#ffb257']);
    if (world.enemies.length > 3) msgs.push(['MULTIPLE HOSTILES', '#ff8f9b']);
    var y = ui.h * 0.5 - 40 * s;
    for (var i = 0; i < msgs.length; i++) {
      var a = 0.65 + 0.35 * Math.sin(world.time * 6 + i);
      ctx.save();
      ctx.globalAlpha = a;
      Text.draw(ctx, msgs[i][0], ui.w - 30 * s, 206 * s + i * 18 * s, {
        font: Font.mono(10 * s, 700), color: msgs[i][1], align: 'right', spacing: 2.2, shadow: hexA(msgs[i][1], 0.8), shadowBlur: 10
      });
      ctx.restore();
    }
  }

  /* ---------------- expandable cargo overlay ---------------- */
  function drawCargoPanel(ctx, game, ui, world) {
    if (!ui.showCargo) return;
    var s = ui.s;
    var w = 380 * s, h = Math.min(ui.h - 200 * s, 420 * s);
    var x = 26 * s, y = ui.h - h - 224 * s;
    W.panel(ctx, x, y, w, h, { fill: 'rgba(6,14,22,0.94)', accent: '#5fe6d8' });
    var hdr = W.header(ctx, x + 16 * s, y + 14 * s, w - 32 * s, 'CARGO HOLD', { sub: 'TAB TO CLOSE' });
    var used = SM.Profile.cargoUsed(game.profile);
    var cap = SM.Profile.cargoCap(game.profile);
    W.bar(ctx, x + 16 * s, y + 44 * s, w - 32 * s, 8 * s, used / Math.max(1, cap), used / cap > 0.9 ? '#ff5f6d' : '#5fe6d8');
    Text.draw(ctx, used.toFixed(1) + ' / ' + cap + ' t', x + w - 16 * s, y + 42 * s, {
      font: Font.mono(9 * s), color: Theme.dim, align: 'right'
    });
    var listY = y + 62 * s;
    var rowH = 28 * s;
    var ids = Object.keys(game.profile.cargo).filter(function (id) { return game.profile.cargo[id] > 0; });
    ids.sort(function (a, b) { return (SM.Resources.get(b).value) - (SM.Resources.get(a).value); });
    if (!ids.length) {
      Text.draw(ctx, 'HOLD EMPTY — FIRE THE MINING BEAM AT AN ASTEROID', x + 16 * s, listY + 20 * s, {
        font: Font.mono(9 * s), color: Theme.dimmer
      });
      return;
    }
    var total = 0;
    for (var i = 0; i < ids.length && i < 12; i++) {
      var rid = ids[i];
      var res = SM.Resources.get(rid);
      var qty = game.profile.cargo[rid];
      var ry = listY + i * rowH;
      var icon = SM.Icons.resource(res);
      ctx.drawImage(icon.canvas, x + 16 * s, ry + 2 * s, 22 * s, 22 * s);
      Text.draw(ctx, res.name, x + 46 * s, ry + 12 * s, { font: Font.body(11.5 * s, 600), color: Theme.ink });
      Text.draw(ctx, 'x' + qty, x + 220 * s, ry + 12 * s, { font: Font.mono(10 * s, 600), color: res.color });
      var val = qty * res.value;
      total += val;
      Text.draw(ctx, U.fmtNum(val) + ' CR', x + w - 16 * s, ry + 12 * s, {
        font: Font.mono(10 * s, 600), color: '#ffd166', align: 'right'
      });
    }
    Text.draw(ctx, 'TOTAL VALUE ' + U.fmtNum(total) + ' CR', x + w - 16 * s, y + h - 14 * s, {
      font: Font.mono(10 * s, 700), color: '#ffe0a0', align: 'right', spacing: 1
    });
  }

  /* ---------------- sector name banner ---------------- */
  function drawSectorBanner(ctx, game, ui, world) {
    if (!world.bannerT) world.bannerT = 0;
    world.bannerT = Math.min(4.2, world.bannerT + game.dt);
    var t = world.bannerT;
    if (t > 3.6) return;
    var s = ui.s;
    var a = t < 0.5 ? t / 0.5 : t > 3 ? (3.6 - t) / 0.6 : 1;
    var y = ui.h * 0.275;
    ctx.save();
    ctx.globalAlpha = U.clamp(a, 0, 1);
    ctx.textAlign = 'center';
    var w = Math.min(ui.w * 0.8, 620 * s);
    var g = ctx.createLinearGradient(ui.w / 2 - w / 2, 0, ui.w / 2 + w / 2, 0);
    g.addColorStop(0, 'rgba(255,255,255,0)');
    g.addColorStop(0.5, hexA(world.sector.palette.accent, 0.5));
    g.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = g;
    ctx.fillRect(ui.w / 2 - w / 2, y - 8 * s, w, 1);
    ctx.fillRect(ui.w / 2 - w / 2, y + 48 * s, w, 1);
    Text.draw(ctx, world.sector.name, ui.w / 2, y + 30 * s, {
      font: Font.title(30 * s, 700), color: '#ffffff', align: 'center', spacing: 7 * s,
      shadow: hexA(world.sector.palette.accent, 0.9), shadowBlur: 24
    });
    Text.draw(ctx, world.sector.subtitle.toUpperCase(), ui.w / 2, y + 64 * s, {
      font: Font.mono(10.5 * s, 600), color: world.sector.palette.accent, align: 'center', spacing: 4 * s
    });
    ctx.restore();
  }

  SM.HUD = { draw: draw, nextObjective: nextObjective };
})(typeof window !== 'undefined' ? window : globalThis);
