/* =========================================================================
   SPACE MINER — UI orchestrator (state dispatch, input plumbing)
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, W = SM.W, Theme = SM.Theme, Font = SM.Font, Text = SM.Text, hexA = U.hexA;
  var Path = SM.Path;

  var UI = {
    w: 1280, h: 720, s: 1, t: 0,
    mouse: { x: 0, y: 0, down: false },
    click: null,
    wheel: 0,
    scroll: {},
    station: { tab: 'trade', cat: 'ALL' },
    mapSel: null,
    showCargo: false,
    confirm: null,
    lastHover: null,
    hoverId: null,
    game: null,

    init: function (game) { this.game = game; },

    reset: function () {
      this.scroll = {};
      this.station = { tab: 'trade', cat: 'ALL' };
      this.showCargo = false;
      this.confirm = null;
      this.mapSel = null;
    },

    openStation: function () { this.station.tab = 'trade'; this.showCargo = false; },
    toggleCargo: function () { this.showCargo = !this.showCargo; },

    consume: function () {
      if (this.game && this.game.input) this.game.input.clicks.shift();
      this.click = null;
    },

    play: function (name) { if (this.game) this.game.sfxUI(name); },

    sync: function (game) {
      var inp = game.input;
      this.w = game.renderer.w;
      this.h = game.renderer.h;
      this.s = U.clamp(Math.min(this.w / 1280, this.h / 720), 0.68, 1.35);
      if (inp) {
        this.mouse.x = inp.mouse.x;
        this.mouse.y = inp.mouse.y;
        this.mouse.down = inp.mouse.down;
        this.click = inp.peekClick();
        this.wheel = inp.consumeWheel();
      }
      this.t += game.dt || 0.016;
      this.hoverId = null;
    },

    finish: function (game) {
      if (!game.input) return;
      // clicks that no widget claimed are discarded (world handles held mouse)
      game.input.clicks.length = 0;
      if (game.canvas && game.canvas.style) {
        var interactive = this.hoverId && (game.state !== 'flight');
        game.canvas.style.cursor = interactive ? 'pointer' : (game.state === 'flight' ? 'crosshair' : 'default');
      }
      this.lastHover = this.hoverId || this.lastHover;
    },

    draw: function (game, ctx, dt) {
      this.sync(game);
      var state = game.state;
      var s = this.s;

      if (state === 'boot') { drawBoot(game, ctx, this); this.finish(game); return; }
      if (state === 'error') { drawError(game, ctx, this); this.finish(game); return; }

      // world/HUD beneath overlays
      if (game.world && ['flight', 'station', 'map', 'paused', 'dead', 'settings', 'achievements', 'controls'].indexOf(state) >= 0) {
        if (state !== 'settings' && state !== 'achievements' && state !== 'controls') {
          SM.HUD.draw(game, ctx, this);
        }
      }

      if (state === 'menu') SM.Menu.menu(game, ctx, this);
      else if (state === 'flight') { /* HUD only */ }
      else if (state === 'station') SM.StationUI.draw(game, ctx, this);
      else if (state === 'map') SM.Menu.map(game, ctx, this);
      else if (state === 'paused') SM.Menu.pause(game, ctx, this);
      else if (state === 'settings') SM.Menu.settings(game, ctx, this);
      else if (state === 'achievements') SM.Menu.achievements(game, ctx, this);
      else if (state === 'controls') SM.Menu.controls(game, ctx, this);
      else if (state === 'dead') SM.Menu.death(game, ctx, this);

      // confirm modal can appear over any screen
      if (this.confirm && state !== 'menu' && state !== 'settings') {
        SM.Menu.confirm(this, ctx, game);
      }

      this.finish(game);
    }
  };

  /* ---------------- boot screen ---------------- */
  function drawBoot(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    var g = ctx.createLinearGradient(0, 0, w * 0.4, h);
    g.addColorStop(0, '#050a12');
    g.addColorStop(0.5, '#08121e');
    g.addColorStop(1, '#03060a');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);

    var cx = w / 2, cy = h / 2;
    var prog = U.clamp(game.bootT / 1.6, 0, 1);

    // rotating hex emblem
    ctx.save();
    ctx.translate(cx, cy - 60 * s);
    ctx.rotate(ui.t * 0.5);
    for (var r = 0; r < 3; r++) {
      ctx.beginPath();
      for (var i = 0; i < 6; i++) {
        var a = i / 6 * 6.2832;
        var rad = (60 + r * 18) * s;
        if (i === 0) ctx.moveTo(Math.cos(a) * rad, Math.sin(a) * rad);
        else ctx.lineTo(Math.cos(a) * rad, Math.sin(a) * rad);
      }
      ctx.closePath();
      ctx.strokeStyle = 'rgba(95,230,216,' + (0.5 - r * 0.13) + ')';
      ctx.lineWidth = r === 0 ? 2 : 1;
      ctx.stroke();
    }
    ctx.restore();

    Text.draw(ctx, 'SPACE MINER', cx, cy + 10 * s, {
      font: Font.title(44 * s, 700), color: '#ffffff', align: 'center', spacing: 10 * s,
      shadow: 'rgba(95,230,216,0.6)', shadowBlur: 30
    });
    Text.draw(ctx, 'DEEP BELT OPERATIONS', cx, cy + 34 * s, {
      font: Font.mono(10 * s, 600), color: 'rgba(95,230,216,0.8)', align: 'center', spacing: 6 * s
    });

    // progress bar
    var bw = 420 * s, bx = cx - bw / 2, by = cy + 84 * s;
    W.bar(ctx, bx, by, bw, 5 * s, prog, '#5fe6d8', { cut: 2 });
    for (var l = 0; l < game.bootLines.length; l++) {
      var lineProgress = (l + 1) / game.bootLines.length;
      if (prog < lineProgress - 0.12) continue;
      Text.draw(ctx, game.bootLines[l], bx, by + 28 * s + l * 14 * s, {
        font: Font.mono(8.5 * s), color: 'rgba(140,180,205,0.7)', spacing: 1
      });
    }
    Text.draw(ctx, Math.round(prog * 100) + '%', bx + bw, by - 8 * s, {
      font: Font.mono(9 * s, 600), color: '#5fe6d8', align: 'right', spacing: 1
    });
  }

  function drawError(game, ctx, ui) {
    var s = ui.s, w = ui.w, h = ui.h;
    ctx.fillStyle = '#0a0406';
    ctx.fillRect(0, 0, w, h);
    Text.draw(ctx, 'FLIGHT SYSTEM FAULT', w / 2, h / 2 - 40 * s, {
      font: Font.title(24 * s, 700), color: '#ff8f9b', align: 'center', spacing: 4 * s
    });
    var msg = game.lastError ? (game.lastError.stack || game.lastError.message || String(game.lastError)) : 'unknown';
    var lines = String(msg).split('\n').slice(0, 8);
    for (var i = 0; i < lines.length; i++) {
      Text.draw(ctx, lines[i].slice(0, 120), w / 2, h / 2 + i * 16 * s, {
        font: Font.mono(10 * s), color: '#b98a92', align: 'center'
      });
    }
    Text.draw(ctx, 'PRESS F5 TO RESTART', w / 2, h - 60 * s, {
      font: Font.mono(10 * s, 600), color: '#ffd166', align: 'center', spacing: 2
    });
  }

  SM.UI = UI;
})(typeof window !== 'undefined' ? window : globalThis);
