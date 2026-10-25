/* =========================================================================
   SPACE MINER — immediate-mode UI widgets (holographic console chrome)
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U, Theme = SM.Theme, Font = SM.Font, Path = SM.Path, Text = SM.Text;
  var hexA = U.hexA;

  function inside(m, x, y, w, h) {
    return m.x >= x && m.x <= x + w && m.y >= y && m.y <= y + h;
  }

  /* ---------------- panels ---------------- */
  function panel(ctx, x, y, w, h, opt) {
    opt = opt || {};
    var cut = opt.cut === undefined ? 16 : opt.cut;
    ctx.save();
    // drop shadow
    ctx.shadowColor = 'rgba(0,0,0,0.55)';
    ctx.shadowBlur = 28;
    ctx.shadowOffsetY = 8;
    Path.cutRect(ctx, x, y, w, h, cut);
    ctx.fillStyle = opt.fill || Theme.panel;
    ctx.fill();
    ctx.shadowColor = 'transparent';
    ctx.shadowBlur = 0;
    ctx.shadowOffsetY = 0;

    // inner gradient sheen
    var g = ctx.createLinearGradient(x, y, x, y + h);
    g.addColorStop(0, 'rgba(255,255,255,0.055)');
    g.addColorStop(0.35, 'rgba(255,255,255,0.012)');
    g.addColorStop(1, 'rgba(0,0,0,0.18)');
    Path.cutRect(ctx, x, y, w, h, cut);
    ctx.fillStyle = g;
    ctx.fill();

    // border
    Path.cutRect(ctx, x + 0.5, y + 0.5, w - 1, h - 1, cut);
    ctx.strokeStyle = opt.border || 'rgba(82,130,164,0.55)';
    ctx.lineWidth = 1;
    ctx.stroke();

    // corner brackets
    ctx.strokeStyle = opt.accent || hexA('#5fe6d8', 0.8);
    ctx.lineWidth = 2;
    var L = 16;
    ctx.beginPath();
    ctx.moveTo(x + cut, y + 2); ctx.lineTo(x + cut + L, y + 2);
    ctx.moveTo(x + 2, y + cut); ctx.lineTo(x + 2, y + cut + L);
    ctx.moveTo(x + w - 2, y + h - cut); ctx.lineTo(x + w - 2, y + h - cut - L);
    ctx.moveTo(x + w - cut, y + h - 2); ctx.lineTo(x + w - cut - L, y + h - 2);
    ctx.stroke();
    ctx.restore();
  }

  function header(ctx, x, y, w, title, opt) {
    opt = opt || {};
    ctx.save();
    ctx.fillStyle = opt.accent || Theme.cyan;
    ctx.fillRect(x, y, 3, opt.h || 18);
    Text.draw(ctx, title, x + 12, y + (opt.h || 18) - 4, {
      font: Font.title(opt.size || 15, 700), color: opt.color || Theme.ink, spacing: 2.2
    });
    if (opt.sub) {
      ctx.restore();
      Text.draw(ctx, opt.sub, x + w, y + (opt.h || 18) - 4, {
        font: Font.mono(10), color: Theme.dim, align: 'right', spacing: 1.2
      });
      ctx.save();
    }
    if (opt.rule !== false) {
      ctx.strokeStyle = 'rgba(90,140,170,0.28)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(x, y + (opt.h || 18) + 6);
      ctx.lineTo(x + w, y + (opt.h || 18) + 6);
      ctx.stroke();
    }
    ctx.restore();
    return (opt.h || 18) + 6;
  }

  /* ---------------- buttons ---------------- */
  function button(ui, ctx, id, x, y, w, h, label, opt) {
    opt = opt || {};
    var hover = inside(ui.mouse, x, y, w, h) && !opt.disabled;
    var clicked = false;
    if (hover && ui.click) {
      ui.consume();
      clicked = true;
    }
    if (hover && !ui.hoverId) {
      ui.hoverId = id;
      if (ui.lastHover !== id) { ui.lastHover = id; ui.play('uiHover'); }
    }
    var accent = opt.accent || Theme.cyan;
    var active = opt.active;

    ctx.save();
    ctx.globalAlpha = opt.disabled ? 0.4 : 1;
    var fill;
    if (opt.primary) {
      fill = ctx.createLinearGradient(x, y, x, y + h);
      fill.addColorStop(0, hover ? hexA(accent, 0.30) : hexA(accent, 0.16));
      fill.addColorStop(1, hover ? hexA(accent, 0.14) : hexA(accent, 0.05));
    } else {
      fill = ctx.createLinearGradient(x, y, x, y + h);
      fill.addColorStop(0, hover ? 'rgba(120,180,220,0.20)' : (active ? 'rgba(95,230,216,0.12)' : 'rgba(120,180,220,0.06)'));
      fill.addColorStop(1, hover ? 'rgba(120,180,220,0.06)' : 'rgba(0,0,0,0.10)');
    }
    Path.cutRect(ctx, x, y, w, h, Math.min(10, h / 2.6));
    ctx.fillStyle = fill;
    ctx.fill();
    ctx.strokeStyle = hover ? hexA(accent, 0.85) : (opt.active ? hexA(accent, 0.55) : 'rgba(90,130,160,0.4)');
    ctx.lineWidth = hover ? 1.4 : 1;
    ctx.stroke();

    if (hover) {
      ctx.shadowColor = hexA(accent, 0.7);
      ctx.shadowBlur = 14;
      Path.cutRect(ctx, x, y, w, h, Math.min(10, h / 2.6));
      ctx.strokeStyle = hexA(accent, 0.9);
      ctx.stroke();
      ctx.shadowBlur = 0;
    }
    if (opt.leftIcon) {
      ctx.save();
      ctx.strokeStyle = hover ? accent : Theme.inkSoft;
      ctx.fillStyle = 'transparent';
      ctx.lineWidth = 1.5;
      ctx.restore();
      SM.Icons.hud(ctx, opt.leftIcon, x + 20, y + h / 2, 16, hover ? accent : Theme.inkSoft);
    }
    Text.draw(ctx, label, x + w / 2 + (opt.leftIcon ? 8 : 0), y + h / 2 + (opt.sub ? 0 : 5), {
      font: Font.title(opt.size || 13, 700), color: opt.disabled ? Theme.dimmer : (hover ? '#ffffff' : (opt.color || Theme.ink)),
      align: 'center', spacing: opt.spacing === undefined ? 1.6 : opt.spacing
    });
    if (opt.sub) {
      Text.draw(ctx, opt.sub, x + w / 2, y + h - 7, {
        font: Font.mono(9), color: hover ? hexA(accent, 0.9) : Theme.dim, align: 'center', spacing: 0.8
      });
    }
    ctx.restore();
    if (clicked && !opt.disabled) ui.play(opt.sound || 'uiClick');
    return clicked;
  }

  /* ---------------- bars ---------------- */
  function bar(ctx, x, y, w, h, pct, color, opt) {
    opt = opt || {};
    pct = U.clamp(pct, 0, 1);
    ctx.save();
    ctx.fillStyle = opt.bg || 'rgba(255,255,255,0.07)';
    Path.cutRect(ctx, x, y, w, h, opt.cut === undefined ? 3 : opt.cut);
    ctx.fill();
    if (pct > 0.001) {
      var g = ctx.createLinearGradient(x, y, x + w, y);
      g.addColorStop(0, U.shade(color, -0.25));
      g.addColorStop(0.55, color);
      g.addColorStop(1, U.shade(color, 0.35));
      Path.cutRect(ctx, x, y, Math.max(2, w * pct), h, opt.cut === undefined ? 3 : opt.cut);
      ctx.fillStyle = g;
      ctx.fill();
      // gloss
      ctx.fillStyle = 'rgba(255,255,255,0.18)';
      ctx.fillRect(x + 1, y + 1, Math.max(0, w * pct - 2), Math.max(1, h * 0.3));
    }
    if (opt.marker !== undefined) {
      ctx.fillStyle = 'rgba(255,255,255,0.5)';
      ctx.fillRect(x + w * U.clamp(opt.marker, 0, 1), y, 1, h);
    }
    ctx.restore();
  }

  /* ---------------- misc ---------------- */
  function tag(ctx, x, y, text, color, opt) {
    opt = opt || {};
    ctx.save();
    ctx.font = Font.mono(opt.size || 9, 600);
    var w = Text.measure(ctx, text, Font.mono(opt.size || 9, 600), 1) + 12;
    var h = opt.h || 16;
    Path.cutRect(ctx, x, y, w, h, 3);
    ctx.fillStyle = hexA(color, 0.13);
    ctx.fill();
    ctx.strokeStyle = hexA(color, 0.5);
    ctx.lineWidth = 1;
    ctx.stroke();
    Text.draw(ctx, text, x + w / 2, y + h / 2 + 3.5, {
      font: Font.mono(opt.size || 9, 600), color: color, align: 'center', spacing: 0.8
    });
    ctx.restore();
    return w;
  }

  function keycap(ctx, x, y, label) {
    ctx.save();
    ctx.font = Font.mono(10, 700);
    var w = Math.max(18, Text.measure(ctx, label, Font.mono(10, 700)) + 10);
    Path.roundRect(ctx, x, y, w, 18, 4);
    ctx.fillStyle = 'rgba(255,255,255,0.06)';
    ctx.fill();
    ctx.strokeStyle = 'rgba(150,190,215,0.45)';
    ctx.lineWidth = 1;
    ctx.stroke();
    Text.draw(ctx, label, x + w / 2, y + 12.5, { font: Font.mono(10, 700), color: Theme.inkSoft, align: 'center' });
    ctx.restore();
    return w;
  }

  function divider(ctx, x, y, w, color) {
    ctx.save();
    var g = ctx.createLinearGradient(x, y, x + w, y);
    g.addColorStop(0, color || 'rgba(120,170,200,0.45)');
    g.addColorStop(1, 'rgba(120,170,200,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x, y, w, 1);
    ctx.restore();
  }

  function scrollArea(ui, ctx, key, x, y, w, h, contentH, drawFn) {
    var st = ui.scroll[key] || (ui.scroll[key] = { y: 0, target: 0 });
    var maxScroll = Math.max(0, contentH - h);
    var hover = inside(ui.mouse, x, y, w, h);
    if (hover && ui.wheel) {
      st.target = U.clamp(st.target + ui.wheel * 0.8, 0, maxScroll);
      ui.wheel = 0;
    }
    st.target = U.clamp(st.target, 0, maxScroll);
    st.y += (st.target - st.y) * 0.28;
    ctx.save();
    ctx.beginPath();
    ctx.rect(x, y, w, h);
    ctx.clip();
    ctx.translate(0, -st.y);
    drawFn(ctx, y + st.y);
    ctx.restore();

    if (maxScroll > 0) {
      var trackH = h;
      var thumbH = Math.max(26, trackH * (h / contentH));
      var thumbY = y + (trackH - thumbH) * (st.y / maxScroll);
      ctx.save();
      ctx.fillStyle = 'rgba(255,255,255,0.05)';
      ctx.fillRect(x + w + 6, y, 3, trackH);
      ctx.fillStyle = hexA(Theme.cyan, hover ? 0.7 : 0.4);
      ctx.fillRect(x + w + 6, thumbY, 3, thumbH);
      ctx.restore();
    }
    return st;
  }

  function slider(ui, ctx, id, x, y, w, label, value, opts) {
    opts = opts || {};
    var h = 22;
    var hover = inside(ui.mouse, x, y - 8, w, h + 8);
    var knobX = x + U.clamp(value, 0, 1) * w;
    ctx.save();
    Text.draw(ctx, label, x, y - 10, { font: Font.mono(10), color: Theme.dim, spacing: 1 });
    ctx.fillStyle = 'rgba(255,255,255,0.08)';
    ctx.fillRect(x, y + 4, w, 3);
    ctx.fillStyle = hexA(Theme.cyan, 0.7);
    ctx.fillRect(x, y + 4, U.clamp(value, 0, 1) * w, 3);
    ctx.beginPath();
    ctx.arc(knobX, y + 5.5, hover ? 7 : 5.5, 0, 6.2832);
    ctx.fillStyle = hover ? '#ffffff' : Theme.cyan;
    ctx.fill();
    ctx.restore();
    if (hover && ui.mouse.down) {
      var v = U.clamp((ui.mouse.x - x) / w, 0, 1);
      if (opts.steps) v = Math.round(v * opts.steps) / opts.steps;
      return { value: v, active: true };
    }
    return { value: value, active: false };
  }

  function checkbox(ui, ctx, id, x, y, label, value) {
    var hover = inside(ui.mouse, x, y - 14, 220, 26);
    var clicked = false;
    if (hover && ui.click) { ui.consume(); clicked = true; ui.play('uiClick'); }
    ctx.save();
    var s = 15;
    Path.cutRect(ctx, x, y - 11, s, s, 3);
    ctx.fillStyle = value ? hexA(Theme.cyan, 0.22) : 'rgba(255,255,255,0.05)';
    ctx.fill();
    ctx.strokeStyle = hover ? Theme.cyan : 'rgba(140,180,210,0.45)';
    ctx.lineWidth = 1.2;
    ctx.stroke();
    if (value) {
      ctx.strokeStyle = Theme.cyan;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(x + 4, y - 3.5);
      ctx.lineTo(x + 6.5, y - 0.5);
      ctx.lineTo(x + 11.5, y - 7.5);
      ctx.stroke();
    }
    Text.draw(ctx, label, x + s + 10, y, {
      font: Font.body(13), color: hover ? '#fff' : Theme.inkSoft, spacing: 0.6
    });
    ctx.restore();
    return clicked;
  }

  function cornerTicks(ctx, x, y, w, h, color, len) {
    len = len || 10;
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(x, y + len); ctx.lineTo(x, y); ctx.lineTo(x + len, y);
    ctx.moveTo(x + w - len, y); ctx.lineTo(x + w, y); ctx.lineTo(x + w, y + len);
    ctx.moveTo(x + w, y + h - len); ctx.lineTo(x + w, y + h); ctx.lineTo(x + w - len, y + h);
    ctx.moveTo(x + len, y + h); ctx.lineTo(x, y + h); ctx.lineTo(x, y + h - len);
    ctx.stroke();
    ctx.restore();
  }

  SM.W = {
    inside: inside, panel: panel, header: header, button: button, bar: bar,
    tag: tag, keycap: keycap, divider: divider, scrollArea: scrollArea,
    slider: slider, checkbox: checkbox, cornerTicks: cornerTicks
  };
})(typeof window !== 'undefined' ? window : globalThis);
