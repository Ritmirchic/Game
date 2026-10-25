/* =========================================================================
   SPACE MINER — front end: title screen, chart, settings, records, pause
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, W = SM.W, Theme = SM.Theme, Font = SM.Font, Text = SM.Text, hexA = U.hexA;
  var Path = SM.Path;

  /* ==================== TITLE SCREEN ==================== */
  function drawMenu(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;

    // cinematic gradient wash
    ctx.save();
    var g = ctx.createLinearGradient(0, 0, w * 0.6, h);
    g.addColorStop(0, 'rgba(6,14,24,0.86)');
    g.addColorStop(0.55, 'rgba(4,8,14,0.55)');
    g.addColorStop(1, 'rgba(2,4,8,0.86)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
    ctx.restore();

    // slow drifting ship (hero shot)
    var art = SM.Art.ship('prospector');
    var k = Math.min(w, h) / 900 * 1.45;
    var sx = w * 0.735, sy = h * 0.46 + Math.sin(ui.t * 0.7) * 10;
    /* soft backlight behind the hero ship */
    var backX = sx - art.w * k * 0.30;
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    var eg = ctx.createRadialGradient(backX, sy, 0, backX, sy, 210 * k);
    eg.addColorStop(0, 'rgba(70,170,190,0.13)');
    eg.addColorStop(0.55, 'rgba(50,120,160,0.05)');
    eg.addColorStop(1, 'rgba(95,230,216,0)');
    ctx.fillStyle = eg;
    ctx.beginPath(); ctx.arc(backX, sy, 210 * k, 0, 6.2832); ctx.fill();
    ctx.restore();
    ctx.save();
    ctx.translate(sx, sy);
    ctx.rotate(-0.16 + Math.sin(ui.t * 0.4) * 0.02);
    ctx.drawImage(art.canvas, -art.w * k / 2, -art.h * k / 2, art.w * k, art.h * k);
    ctx.restore();
    // thruster plumes, anchored to the hull's tail
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.translate(sx, sy);
    ctx.rotate(-0.16 + Math.sin(ui.t * 0.4) * 0.02);
    var tailX = -art.w * k * 0.385;
    for (var e = 0; e < 2; e++) {
      var ey = (e ? 1 : -1) * art.h * k * 0.155;
      var flick = 0.86 + Math.sin(ui.t * 9 + e * 2.1) * 0.14;
      var lg = ctx.createRadialGradient(tailX, ey, 0, tailX, ey, 34 * k * flick);
      lg.addColorStop(0, 'rgba(225,252,255,0.55)');
      lg.addColorStop(0.35, 'rgba(110,215,255,0.28)');
      lg.addColorStop(1, 'rgba(70,160,255,0)');
      ctx.fillStyle = lg;
      ctx.beginPath(); ctx.arc(tailX, ey, 34 * k * flick, 0, 6.2832); ctx.fill();
      // exhaust streak
      var sg = ctx.createLinearGradient(tailX - 54 * k, 0, tailX, 0);
      sg.addColorStop(0, 'rgba(120,220,255,0)');
      sg.addColorStop(1, 'rgba(160,235,255,' + (0.20 * flick).toFixed(3) + ')');
      ctx.fillStyle = sg;
      ctx.fillRect(tailX - 54 * k, ey - 3.0 * k, 54 * k, 6.0 * k);
    }
    ctx.restore();

    // title block
    var tx = Math.max(56 * s, w * 0.08);
    var ty = h * 0.30;
    Text.draw(ctx, 'DEEP BELT OPERATIONS', tx, ty - 46 * s, {
      font: Font.mono(10.5 * s, 600), color: hexA('#5fe6d8', 0.9), spacing: 7 * s
    });
    ctx.save();
    ctx.shadowColor = 'rgba(95,230,216,0.55)';
    ctx.shadowBlur = 34;
    Text.draw(ctx, 'SPACE MINER', tx, ty + 20 * s, {
      font: Font.title(66 * s, 700), color: '#ffffff', spacing: 9 * s
    });
    ctx.restore();
    Text.draw(ctx, 'MINE · UPGRADE · AUTOMATE · SURVIVE', tx + 4 * s, ty + 52 * s, {
      font: Font.mono(11 * s, 600), color: '#8fb6cf', spacing: 4.4 * s
    });

    // divider
    var dg = ctx.createLinearGradient(tx, 0, tx + 420 * s, 0);
    dg.addColorStop(0, 'rgba(95,230,216,0.8)');
    dg.addColorStop(1, 'rgba(95,230,216,0)');
    ctx.fillStyle = dg;
    ctx.fillRect(tx + 4 * s, ty + 70 * s, 420 * s, 1);

    // menu buttons
    var loadRes = game.loadResult;
    var hasSave = loadRes && loadRes.ok;
    var bw = 268 * s, bh = 46 * s, gap = 12 * s;
    var by = ty + 92 * s;

    if (hasSave) {
      var pr = loadRes.profile;
      var lp = SM.Profile.levelProgress(pr);
      if (W.button(ui, ctx, 'continue', tx + 4 * s, by, bw, bh, 'CONTINUE FLIGHT', {
        primary: true, size: 13 * s, sub: 'LVL ' + pr.level + ' · ' + U.fmtNum(pr.credits) + ' CR · ' + SM.Sectors.get(pr.sector).name,
        accent: '#5fe6d8'
      })) {
        game.continueGame();
        ui.play('warp');
        return;
      }
      by += bh + gap;
    }
    if (W.button(ui, ctx, 'new', tx + 4 * s, by, bw, bh, hasSave ? 'NEW FLIGHT' : 'LAUNCH FLIGHT', {
      primary: !hasSave, size: 13 * s, accent: '#5fe6d8',
      sub: hasSave ? 'ERASES THE CURRENT PILOT RECORD' : 'BEGIN IN THE STARTER BELT'
    })) {
      if (hasSave) { ui.confirm = { action: 'new' }; }
      else { game.startNew(); ui.play('warp'); }
      by += 0;
    }
    by += bh + gap;

    var half = (bw - gap) / 2;
    if (W.button(ui, ctx, 'settings', tx + 4 * s, by, half, bh, 'SETTINGS', { size: 11 * s })) {
      game.settingsReturn = 'menu';
      game.state = 'settings';
    }
    if (W.button(ui, ctx, 'records', tx + 4 * s + half + gap, by, half, bh, 'RECORDS', { size: 11 * s })) {
      game.settingsReturn = 'menu';
      game.state = 'achievements';
    }
    by += bh + gap;
    if (W.button(ui, ctx, 'controls', tx + 4 * s, by, bw, bh, 'CONTROLS & FLIGHT MANUAL', { size: 11 * s })) {
      game.settingsReturn = 'menu';
      game.state = 'controls';
    }

    // footer
    Text.draw(ctx, 'SPACE MINER v1.0  ·  ARENA.AI  ·  PROCEDURAL CANVAS ENGINE', tx + 4 * s, h - 36 * s, {
      font: Font.mono(8.5 * s), color: 'rgba(120,160,185,0.55)', spacing: 2
    });
    Text.draw(ctx, 'NO ASSET FILES · ALL ART, AUDIO AND LEVELS GENERATED AT RUNTIME', tx + 4 * s, h - 22 * s, {
      font: Font.mono(8 * s), color: 'rgba(120,160,185,0.35)', spacing: 1.4
    });

    // press-any-key hint on fresh install
    if (!hasSave && Math.sin(ui.t * 2.2) > 0) {
      Text.draw(ctx, 'SELECT LAUNCH FLIGHT TO BEGIN', tx + 4 * s, by + bh + 30 * s, {
        font: Font.mono(9 * s, 600), color: 'rgba(95,230,216,0.7)', spacing: 2.4
      });
    }

    drawConfirm(ui, ctx, game);
  }

  function drawConfirm(ui, ctx, game) {
    if (!ui.confirm) return;
    var s = ui.s, w = ui.w, h = ui.h;
    ctx.save();
    ctx.fillStyle = 'rgba(0,0,0,0.6)';
    ctx.fillRect(0, 0, w, h);
    var pw = 420 * s, ph = 170 * s;
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(12,18,26,0.98)', accent: '#ff6b6b', cut: 12 * s });
    Text.draw(ctx, 'ERASE PILOT RECORD?', px + pw / 2, py + 44 * s, {
      font: Font.title(16 * s, 700), color: '#ffb0b8', align: 'center', spacing: 2.4
    });
    Text.draw(ctx, 'All credits, upgrades, ships and achievements will be lost.', px + pw / 2, py + 70 * s, {
      font: Font.body(11 * s), color: Theme.dim, align: 'center'
    });
    if (W.button(ui, ctx, 'confirm-yes', px + 24 * s, py + ph - 58 * s, (pw - 60 * s) / 2, 36 * s, 'ERASE', {
      accent: '#ff6b6b', primary: true, size: 11 * s
    })) {
      ui.confirm = null;
      game.startNew();
      ui.play('warp');
      ctx.restore();
      return;
    }
    if (W.button(ui, ctx, 'confirm-no', px + pw / 2 + 6 * s, py + ph - 58 * s, (pw - 60 * s) / 2, 36 * s, 'CANCEL', {
      size: 11 * s
    })) {
      ui.confirm = null;
    }
    ctx.restore();
  }

  /* ==================== SETTINGS ==================== */
  function drawSettings(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    ctx.save();
    ctx.fillStyle = 'rgba(2,6,11,0.82)';
    ctx.fillRect(0, 0, w, h);
    var pw = Math.min(560 * s, w - 60 * s), ph = 470 * s;
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(8,16,26,0.98)', accent: '#5fe6d8' });
    W.header(ctx, px + 24 * s, py + 22 * s, pw - 48 * s, 'SYSTEM SETTINGS', { sub: 'SPACE MINER v1.0' });

    var pr = game.profile || SM.Profile.create();
    var st = pr.settings;
    var x = px + 34 * s, y = py + 86 * s;

    if (W.checkbox(ui, ctx, 'sfx', x, y, 'SOUND EFFECTS', st.sfx)) {
      st.sfx = !st.sfx;
      game.applySettings();
      game.save();
    }
    if (W.checkbox(ui, ctx, 'music', x, y + 34 * s, 'GENERATIVE MUSIC', st.music)) {
      st.music = !st.music;
      game.applySettings();
      game.save();
    }
    if (W.checkbox(ui, ctx, 'shake', x, y + 68 * s, 'CAMERA SHAKE', st.shake)) {
      st.shake = !st.shake;
      game.save();
    }
    if (W.checkbox(ui, ctx, 'hints', x, y + 102 * s, 'CONTEXT HINTS', st.showHints)) {
      st.showHints = !st.showHints;
      game.save();
    }

    var q = W.slider(ui, ctx, 'quality', x, y + 156 * s, pw - 68 * s, 'RENDER QUALITY', st.quality === undefined ? 1 : st.quality, { steps: 2 });
    if (q.active) {
      st.quality = q.value;
      game.quality = q.value;
      game.save();
    }
    var qLabel = st.quality > 0.8 ? 'HIGH' : st.quality > 0.4 ? 'MEDIUM' : 'LOW';
    Text.draw(ctx, qLabel, px + pw - 34 * s, y + 146 * s, {
      font: Font.mono(10 * s, 600), color: Theme.cyan, align: 'right', spacing: 1
    });

    Text.draw(ctx, 'Render quality scales particle density and glow passes.', x, y + 196 * s, {
      font: Font.body(10 * s), color: Theme.dimmer
    });

    // save management
    W.divider(ctx, x, y + 220 * s, pw - 68 * s);
    Text.draw(ctx, 'SAVE DATA', x, y + 244 * s, {
      font: Font.mono(9 * s, 600), color: Theme.dim, spacing: 1.8
    });
    if (W.button(ui, ctx, 'save-now', x, y + 258 * s, 150 * s, 32 * s, 'SAVE NOW', { size: 10 * s })) {
      var r = game.save();
      game.toast(r && r.ok ? 'PROGRESS SAVED' : 'SAVE FAILED', r && r.ok ? 'good' : 'warn', 2.4);
    }
    if (W.button(ui, ctx, 'wipe', x + 162 * s, y + 258 * s, 170 * s, 32 * s, 'RESET PROGRESS', {
      size: 10 * s, accent: '#ff6b6b'
    })) {
      ui.confirm = { action: 'wipe' };
    }
    if (ui.confirm && ui.confirm.action === 'wipe') {
      ctx.restore();
      drawConfirmWipe(ui, ctx, game);
      return;
    }

    if (W.button(ui, ctx, 'back', px + pw - 152 * s, py + ph - 58 * s, 124 * s, 38 * s, 'BACK', {
      primary: true, size: 11 * s
    })) {
      game.state = game.settingsReturn || 'menu';
    }
    Text.draw(ctx, 'ESC TO RETURN', px + 34 * s, py + ph - 34 * s, {
      font: Font.mono(8.5 * s), color: Theme.dimmer, spacing: 2
    });
    ctx.restore();
  }

  function drawConfirmWipe(ui, ctx, game) {
    var s = ui.s, w = ui.w, h = ui.h;
    var pw = 420 * s, ph = 168 * s;
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(20,10,14,0.99)', accent: '#ff6b6b' });
    Text.draw(ctx, 'DELETE ALL SAVE DATA?', px + pw / 2, py + 44 * s, {
      font: Font.title(15 * s, 700), color: '#ffb0b8', align: 'center', spacing: 2
    });
    Text.draw(ctx, 'This cannot be undone.', px + pw / 2, py + 68 * s, {
      font: Font.body(11 * s), color: Theme.dim, align: 'center'
    });
    if (W.button(ui, ctx, 'wipe-yes', px + 24 * s, py + ph - 56 * s, (pw - 60 * s) / 2, 34 * s, 'DELETE', {
      accent: '#ff6b6b', primary: true, size: 11 * s
    })) {
      SM.Profile.wipe();
      game.loadResult = { ok: false, reason: 'empty' };
      ui.confirm = null;
      game.toast('SAVE DATA DELETED', 'warn', 2.4);
    }
    if (W.button(ui, ctx, 'wipe-no', px + pw / 2 + 6 * s, py + ph - 56 * s, (pw - 60 * s) / 2, 34 * s, 'CANCEL', {
      size: 11 * s
    })) {
      ui.confirm = null;
    }
  }

  /* ==================== ACHIEVEMENTS ==================== */
  function drawAchievements(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    ctx.save();
    ctx.fillStyle = 'rgba(2,6,11,0.85)';
    ctx.fillRect(0, 0, w, h);
    var pw = Math.min(880 * s, w - 60 * s), ph = Math.min(640 * s, h - 80 * s);
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(8,16,26,0.98)', accent: '#ffd166' });

    var pr = game.profile || (game.loadResult && game.loadResult.profile) || SM.Profile.create();
    var unlockedCount = pr.achievements.length;
    W.header(ctx, px + 24 * s, py + 22 * s, pw - 48 * s, 'PILOT RECORDS', {
      sub: unlockedCount + ' / ' + SM.Achievements.list.length + ' UNLOCKED', accent: '#ffd166'
    });

    var cols = 2, gap = 12 * s;
    var cardW = (pw - 60 * s - gap) / cols, cardH = 76 * s;
    var rows = Math.ceil(SM.Achievements.list.length / cols);

    W.scrollArea(ui, ctx, 'ach', px + 24 * s, py + 70 * s, pw - 48 * s, ph - 150 * s,
      rows * (cardH + gap) + 10 * s, function (ctx) {
        for (var i = 0; i < SM.Achievements.list.length; i++) {
          var a = SM.Achievements.list[i];
          var col = i % cols, row = Math.floor(i / cols);
          var bx = px + 24 * s + col * (cardW + gap);
          var by = py + 70 * s + row * (cardH + gap);
          var unlocked = pr.achievements.indexOf(a.id) >= 0;
          ctx.save();
          Path.cutRect(ctx, bx, by, cardW, cardH, 8 * s);
          ctx.fillStyle = unlocked ? 'rgba(255,209,102,0.08)' : 'rgba(255,255,255,0.02)';
          ctx.fill();
          ctx.strokeStyle = unlocked ? 'rgba(255,209,102,0.5)' : 'rgba(120,160,190,0.18)';
          ctx.lineWidth = 1;
          ctx.stroke();
          ctx.restore();

          // medallion
          ctx.save();
          ctx.translate(bx + 34 * s, by + cardH / 2);
          ctx.beginPath(); ctx.arc(0, 0, 19 * s, 0, 6.2832);
          ctx.fillStyle = unlocked ? 'rgba(255,209,102,0.16)' : 'rgba(255,255,255,0.03)';
          ctx.fill();
          ctx.strokeStyle = unlocked ? 'rgba(255,209,102,0.8)' : 'rgba(120,160,190,0.25)';
          ctx.lineWidth = 1.4;
          ctx.stroke();
          SM.Icons.hud(ctx, unlocked ? 'level' : 'warn', 0, 0, 18 * s, unlocked ? '#ffd166' : 'rgba(120,160,190,0.4)');
          ctx.restore();

          Text.draw(ctx, a.name.toUpperCase(), bx + 64 * s, by + 26 * s, {
            font: Font.title(11 * s, 700), color: unlocked ? '#ffe9a8' : Theme.inkSoft, spacing: 1.2
          });
          var lines = Text.wrap(ctx, a.desc, cardW - 84 * s, Font.body(9.5 * s));
          Text.draw(ctx, lines[0], bx + 64 * s, by + 42 * s, { font: Font.body(9.5 * s), color: Theme.dim });
          Text.draw(ctx, unlocked ? 'UNLOCKED  +' + a.reward + ' CR' : 'REWARD ' + a.reward + ' CR',
            bx + 64 * s, by + cardH - 12 * s, {
            font: Font.mono(8 * s, 600), color: unlocked ? '#ffd166' : Theme.dimmer, spacing: 1
          });
        }
      });

    if (W.button(ui, ctx, 'ach-back', px + pw - 152 * s, py + ph - 58 * s, 124 * s, 38 * s, 'BACK', {
      primary: true, size: 11 * s
    })) {
      game.state = game.settingsReturn || 'menu';
    }
    Text.draw(ctx, 'ACHIEVEMENTS AWARD CREDITS INSTANTLY WHEN EARNED', px + 24 * s, py + ph - 34 * s, {
      font: Font.mono(8 * s), color: Theme.dimmer, spacing: 1.4
    });
    ctx.restore();
  }

  /* ==================== CONTROLS ==================== */
  function drawControls(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    ctx.save();
    ctx.fillStyle = 'rgba(2,6,11,0.85)';
    ctx.fillRect(0, 0, w, h);
    var pw = Math.min(720 * s, w - 60 * s), ph = Math.min(560 * s, h - 80 * s);
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(8,16,26,0.98)', accent: '#5fe6d8' });
    W.header(ctx, px + 24 * s, py + 22 * s, pw - 48 * s, 'FLIGHT MANUAL', { sub: 'KEYBOARD · MOUSE' });

    var rows = [
      ['W A S D / ARROWS', 'Thrust and strafe'],
      ['MOUSE', 'Aim the ship and the mining beam'],
      ['LEFT MOUSE (HOLD)', 'Fire mining laser at the asteroid under the cursor'],
      ['SPACE / RIGHT MOUSE', 'Fire pulse cannons'],
      ['SHIFT', 'Afterburner — burns reactor energy for speed'],
      ['E', 'Dock at station (when in range)'],
      ['M', 'Sector chart'],
      ['TAB', 'Expand cargo hold readout'],
      ['ESC', 'Pause / close panel'],
      ['DRONE PANEL', 'Toggle drone autonomy from the HUD (bottom right)']
    ];
    var y = py + 78 * s;
    for (var i = 0; i < rows.length; i++) {
      var kw = W.keycap(ctx, px + 34 * s, y, rows[i][0]);
      Text.draw(ctx, rows[i][1], px + 44 * s + kw, y + 13 * s, {
        font: Font.body(11 * s), color: Theme.inkSoft
      });
      y += 30 * s;
    }
    Text.draw(ctx, 'DOCK TO SELL ORE, BUY UPGRADES, DEPLOY DRONES AND OPEN NEW SECTORS.', px + 34 * s, py + ph - 76 * s, {
      font: Font.mono(8.5 * s), color: Theme.dim, spacing: 1.4
    });
    Text.draw(ctx, 'DRONES WORK AUTOMATICALLY WHILE YOU FLY — THEY DO NOT NEED SUPERVISION.', px + 34 * s, py + ph - 58 * s, {
      font: Font.mono(8.5 * s), color: Theme.dim, spacing: 1.4
    });
    if (W.button(ui, ctx, 'ctrl-back', px + pw - 152 * s, py + ph - 58 * s, 124 * s, 38 * s, 'BACK', {
      primary: true, size: 11 * s
    })) {
      game.state = game.settingsReturn || 'menu';
    }
    ctx.restore();
  }

  /* ==================== PAUSE ==================== */
  function drawPause(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    ctx.save();
    ctx.fillStyle = 'rgba(2,6,11,0.62)';
    ctx.fillRect(0, 0, w, h);
    var pw = 380 * s, ph = 388 * s;
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(8,16,26,0.97)', accent: '#5fe6d8' });
    Text.draw(ctx, 'PAUSED', px + pw / 2, py + 48 * s, {
      font: Font.title(26 * s, 700), color: '#ffffff', align: 'center', spacing: 8 * s,
      shadow: 'rgba(95,230,216,0.6)', shadowBlur: 18
    });
    var pr = game.profile;
    Text.draw(ctx, SM.Sectors.get(pr.sector).name + ' · PILOT LVL ' + pr.level, px + pw / 2, py + 68 * s, {
      font: Font.mono(9 * s, 600), color: Theme.dim, align: 'center', spacing: 2
    });

    var bx = px + 32 * s, bw = pw - 64 * s, by = py + 92 * s, bh = 42 * s;
    if (W.button(ui, ctx, 'resume', bx, by, bw, bh, 'RESUME FLIGHT', { primary: true, size: 12 * s })) {
      game.state = 'flight';
    }
    by += bh + 10 * s;
    if (W.button(ui, ctx, 'p-settings', bx, by, bw, bh, 'SETTINGS', { size: 11 * s })) {
      game.settingsReturn = 'paused';
      game.state = 'settings';
    }
    by += bh + 10 * s;
    if (W.button(ui, ctx, 'p-map', bx, by, bw, bh, 'SECTOR CHART', { size: 11 * s })) {
      game.state = 'map';
    }
    by += bh + 10 * s;
    if (W.button(ui, ctx, 'p-records', bx, by, bw, bh, 'PILOT RECORDS', { size: 11 * s })) {
      game.settingsReturn = 'paused';
      game.state = 'achievements';
    }
    by += bh + 14 * s;
    if (W.button(ui, ctx, 'p-quit', bx, by, bw, bh, 'SAVE & QUIT TO TITLE', { size: 10.5 * s, accent: '#ffb257' })) {
      game.quitToMenu();
    }
    ctx.restore();
  }

  /* ==================== DEATH ==================== */
  function drawDeath(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    var a = U.clamp(game.deathT / 0.8, 0, 1);
    ctx.save();
    ctx.globalAlpha = a;
    ctx.fillStyle = 'rgba(14,2,4,0.72)';
    ctx.fillRect(0, 0, w, h);
    var pw = 460 * s, ph = 300 * s;
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(20,8,10,0.97)', accent: '#ff5f6d', border: 'rgba(255,95,109,0.5)' });
    Text.draw(ctx, 'HULL BREACH', px + pw / 2, py + 62 * s, {
      font: Font.title(30 * s, 700), color: '#ff8f9b', align: 'center', spacing: 8 * s,
      shadow: 'rgba(255,95,109,0.8)', shadowBlur: 26
    });
    Text.draw(ctx, 'EMERGENCY BEACON RECEIVED BY ' + (game.world ? game.world.station.name : 'STATION'), px + pw / 2, py + 86 * s, {
      font: Font.mono(9 * s, 600), color: Theme.dim, align: 'center', spacing: 1.8
    });
    var pr = game.profile;
    Text.draw(ctx, '35% OF CARGO LOST · 8% INSURANCE FEE', px + pw / 2, py + 118 * s, {
      font: Font.body(11 * s), color: Theme.inkSoft, align: 'center'
    });
    Text.draw(ctx, 'CREDITS ' + U.fmtNum(pr.credits) + '   ·   KILLS ' + pr.counters.kills + '   ·   ROCKS ' + pr.counters.asteroidsMined,
      px + pw / 2, py + 140 * s, {
      font: Font.mono(9 * s), color: Theme.dimmer, align: 'center', spacing: 1
    });
    if (a >= 1 && W.button(ui, ctx, 'respawn', px + 40 * s, py + ph - 74 * s, pw - 80 * s, 46 * s,
      'RESPAWN AT STATION', { primary: true, accent: '#5fe6d8', size: 13 * s })) {
      game.respawn();
      ui.play('dock');
    }
    ctx.restore();
  }

  /* ==================== SECTOR CHART OVERLAY ==================== */
  function drawMap(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    var pr = game.profile;
    ctx.save();
    ctx.fillStyle = 'rgba(2,6,11,0.86)';
    ctx.fillRect(0, 0, w, h);

    var pw = Math.min(1060 * s, w - 50 * s), ph = Math.min(620 * s, h - 60 * s);
    var px = w / 2 - pw / 2, py = h / 2 - ph / 2;
    W.panel(ctx, px, py, pw, ph, { fill: 'rgba(6,13,22,0.98)', accent: '#5fe6d8' });
    W.header(ctx, px + 26 * s, py + 22 * s, pw - 52 * s, 'SECTOR CHART', {
      sub: 'M · CLOSE    ·    JUMP VIA CHART', accent: '#5fe6d8'
    });

    // chart area
    var cw = pw * 0.56, chh = ph - 100 * s;
    var cx = px + 26 * s, cy = py + 72 * s;
    ctx.save();
    Path.cutRect(ctx, cx, cy, cw, chh, 8 * s);
    ctx.fillStyle = 'rgba(2,6,12,0.9)';
    ctx.fill();
    ctx.strokeStyle = 'rgba(90,140,170,0.25)';
    ctx.stroke();
    ctx.clip();

    // grid + star dust
    ctx.strokeStyle = 'rgba(90,150,190,0.08)';
    ctx.lineWidth = 1;
    for (var gx = 0; gx < cw; gx += 40 * s) {
      ctx.beginPath(); ctx.moveTo(cx + gx, cy); ctx.lineTo(cx + gx, cy + chh); ctx.stroke();
    }
    for (var gy = 0; gy < chh; gy += 40 * s) {
      ctx.beginPath(); ctx.moveTo(cx, cy + gy); ctx.lineTo(cx + cw, cy + gy); ctx.stroke();
    }
    var rng = new U.RNG(4242);
    for (var st = 0; st < 160; st++) {
      ctx.fillStyle = 'rgba(200,230,255,' + rng.range(0.05, 0.4) + ')';
      ctx.fillRect(cx + rng.range(0, cw), cy + rng.range(0, chh), 1.4, 1.4);
    }

    function nodePos(sec) {
      return {
        x: cx + sec.mapPos.x * cw * 0.92 + cw * 0.04,
        y: cy + sec.mapPos.y * chh * 0.92 + chh * 0.04
      };
    }
    // jump lanes
    for (var l = 0; l < SM.Sectors.list.length - 1; l++) {
      var a1 = nodePos(SM.Sectors.list[l]);
      var a2 = nodePos(SM.Sectors.list[l + 1]);
      var open = pr.sectors.indexOf(SM.Sectors.list[l + 1].id) >= 0;
      ctx.strokeStyle = open ? 'rgba(95,230,216,0.45)' : 'rgba(120,150,170,0.18)';
      ctx.lineWidth = open ? 1.6 : 1;
      ctx.setLineDash(open ? [] : [5, 6]);
      ctx.beginPath();
      ctx.moveTo(a1.x, a1.y);
      ctx.lineTo(a2.x, a2.y);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    var sel = ui.mapSel || pr.sector;
    for (var i = 0; i < SM.Sectors.list.length; i++) {
      var sec = SM.Sectors.list[i];
      var p = nodePos(sec);
      var unlocked = pr.sectors.indexOf(sec.id) >= 0;
      var here = pr.sector === sec.id;
      var hover = U.dist(ui.mouse.x, ui.mouse.y, p.x, p.y) < 34 * s;

      if (hover && ui.click) { ui.consume(); ui.mapSel = sec.id; sel = sec.id; ui.play('uiHover'); }

      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      var ng = ctx.createRadialGradient(p.x, p.y, 0, p.x, p.y, 54 * s);
      ng.addColorStop(0, hexA(sec.palette.accent, unlocked ? 0.28 : 0.08));
      ng.addColorStop(1, hexA(sec.palette.accent, 0));
      ctx.fillStyle = ng;
      ctx.beginPath(); ctx.arc(p.x, p.y, 54 * s, 0, 6.2832); ctx.fill();
      ctx.restore();

      ctx.save();
      ctx.translate(p.x, p.y);
      ctx.rotate(ui.t * 0.25 + i);
      ctx.strokeStyle = hexA(sec.palette.accent, unlocked ? 0.75 : 0.3);
      ctx.lineWidth = here ? 2.2 : 1.4;
      ctx.beginPath();
      for (var k = 0; k < 6; k++) {
        var ang = k / 6 * 6.2832;
        var rr = sel === sec.id ? 20 * s : 16 * s;
        if (k === 0) ctx.moveTo(Math.cos(ang) * rr, Math.sin(ang) * rr);
        else ctx.lineTo(Math.cos(ang) * rr, Math.sin(ang) * rr);
      }
      ctx.closePath();
      ctx.stroke();
      if (unlocked) {
        ctx.fillStyle = hexA(sec.palette.accent, here ? 0.3 : 0.14);
        ctx.fill();
      }
      ctx.restore();

      ctx.save();
      ctx.translate(p.x, p.y);
      if (!unlocked) {
        ctx.strokeStyle = 'rgba(255,255,255,0.4)';
        ctx.lineWidth = 1.4;
        ctx.beginPath();
        ctx.arc(0, -2 * s, 5 * s, Math.PI, 0);
        ctx.stroke();
        ctx.fillStyle = 'rgba(255,255,255,0.35)';
        ctx.fillRect(-5 * s, -1 * s, 10 * s, 8 * s);
      }
      ctx.restore();

      Text.draw(ctx, sec.code, p.x, p.y + 34 * s, {
        font: Font.mono(8.5 * s, 600), color: hexA(sec.palette.accent, unlocked ? 0.95 : 0.5), align: 'center', spacing: 1.4
      });
      if (here) {
        ctx.save();
        ctx.strokeStyle = 'rgba(255,255,255,0.6)';
        ctx.lineWidth = 1;
        ctx.setLineDash([3, 4]);
        ctx.beginPath(); ctx.arc(p.x, p.y, 26 * s + Math.sin(ui.t * 3) * 2, 0, 6.2832); ctx.stroke();
        ctx.restore();
      }
    }
    ctx.restore();

    // info column
    var ix = cx + cw + 20 * s, iw = pw - cw - 72 * s;
    var sec2 = SM.Sectors.get(sel);
    var unlocked2 = pr.sectors.indexOf(sec2.id) >= 0;

    Text.draw(ctx, sec2.code, ix, py + 84 * s, {
      font: Font.mono(9.5 * s, 600), color: hexA(sec2.palette.accent, 0.95), spacing: 2
    });
    Text.draw(ctx, sec2.name, ix, py + 112 * s, {
      font: Font.title(21 * s, 700), color: Theme.ink, spacing: 2.6
    });
    Text.draw(ctx, sec2.subtitle, ix, py + 130 * s, {
      font: Font.mono(9 * s), color: Theme.dim, spacing: 1
    });
    var dl = Text.wrap(ctx, sec2.desc, iw, Font.body(11 * s));
    for (var d = 0; d < dl.length; d++) {
      Text.draw(ctx, dl[d], ix, py + 156 * s + d * 16 * s, { font: Font.body(11 * s), color: Theme.inkSoft });
    }

    var ry = py + 156 * s + dl.length * 16 * s + 22 * s;
    Text.draw(ctx, 'DANGER', ix, ry, { font: Font.mono(8 * s, 600), color: Theme.dimmer, spacing: 2 });
    for (var dd = 0; dd < 5; dd++) {
      ctx.fillStyle = dd < sec2.danger ? hexA('#ff6b6b', 0.9) : 'rgba(255,255,255,0.1)';
      ctx.fillRect(ix + 62 * s + dd * 10 * s, ry - 8 * s, 7 * s, 10 * s);
    }
    ry += 18 * s;

    Text.draw(ctx, 'RESOURCES', ix, ry + 8 * s, { font: Font.mono(8 * s, 600), color: Theme.dimmer, spacing: 2 });
    ry += 20 * s;
    var keys = Object.keys(sec2.asteroid.mineralWeights);
    var chx = ix;
    for (var q = 0; q < keys.length; q++) {
      var res = SM.Resources.get(keys[q]);
      if (!res) continue;
      var label = res.code;
      ctx.font = Font.mono(7.5 * s, 600);
      var cw2 = Text.measure(ctx, label, Font.mono(7.5 * s, 600), 0.8) + 14 * s;
      if (chx + cw2 > ix + iw) { chx = ix; ry += 20 * s; }
      ctx.save();
      Path.cutRect(ctx, chx, ry, cw2, 15 * s, 2);
      ctx.fillStyle = hexA(res.color, 0.15);
      ctx.fill();
      ctx.strokeStyle = hexA(res.color, 0.5);
      ctx.stroke();
      Text.draw(ctx, label, chx + cw2 / 2, ry + 10.5 * s, {
        font: Font.mono(7.5 * s, 600), color: res.color, align: 'center', spacing: 0.8
      });
      ctx.restore();
      chx += cw2 + 5 * s;
    }
    ry += 34 * s;

    // hostiles
    Text.draw(ctx, 'HOSTILES: ' + sec2.enemies.types.map(function (t) {
      return SM.Enemies.get(t[0]).short;
    }).join(' · '), ix, ry, { font: Font.mono(8.5 * s), color: '#ff8f9b', spacing: 1 });

    ry += 26 * s;

    if (unlocked2) {
      if (pr.sector === sec2.id) {
        Text.draw(ctx, 'CURRENT POSITION', ix, ry, {
          font: Font.mono(9 * s, 700), color: hexA(sec2.palette.accent, 0.9), spacing: 2
        });
      } else if (W.button(ui, ctx, 'map-jump', ix, ry - 10 * s, iw, 44 * s, 'ENGAGE JUMP DRIVE', {
        primary: true, accent: sec2.palette.accent, size: 12 * s, sub: 'ARRIVE AT ' + sec2.station.name
      })) {
        game.travelTo(sec2.id);
        ui.play('warp');
      }
    } else {
      var ck = SM.Profile.canUnlock(pr, sec2.id);
      var reqLines2 = [];
      reqLines2.push(sec2.unlock.credits + ' CR   (' + U.fmtNum(pr.credits) + ' AVAILABLE)');
      for (var rid in sec2.unlock.deliver) {
        var need = sec2.unlock.deliver[rid];
        reqLines2.push(need + ' × ' + SM.Resources.get(rid).name + '   (' + (pr.cargo[rid] | 0) + ' IN HOLD)');
      }
      Text.draw(ctx, 'CORRIDOR LOCKED', ix, ry, { font: Font.mono(9 * s, 700), color: '#ff8f9b', spacing: 2 });
      for (var rl = 0; rl < reqLines2.length; rl++) {
        Text.draw(ctx, reqLines2[rl], ix, ry + 20 * s + rl * 16 * s, {
          font: Font.mono(9 * s), color: Theme.inkSoft, spacing: 0.6
        });
      }
      Text.draw(ctx, 'DOCK AT A STATION TO HAND OVER THE REQUIRED ORE.', ix, ry + 28 * s + reqLines2.length * 16 * s, {
        font: Font.body(9.5 * s), color: Theme.dimmer
      });
    }

    if (W.button(ui, ctx, 'map-close', px + pw - 152 * s, py + ph - 56 * s, 124 * s, 38 * s, 'CLOSE', {
      size: 11 * s
    })) {
      game.state = 'flight';
    }
    ctx.restore();
  }

  SM.Menu = {
    menu: drawMenu, settings: drawSettings, achievements: drawAchievements,
    controls: drawControls, pause: drawPause, death: drawDeath, map: drawMap,
    confirm: drawConfirm
  };
})(typeof window !== 'undefined' ? window : globalThis);
