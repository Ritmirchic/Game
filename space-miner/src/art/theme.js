/* =========================================================================
   SPACE MINER — visual theme + typography helpers
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  var THEME = {
    bg: '#04060b',
    panel: 'rgba(9,18,28,0.92)',
    panelHi: 'rgba(16,32,46,0.94)',
    panelDeep: 'rgba(4,9,15,0.95)',
    line: '#20384b',
    lineHi: '#3d6a8c',
    ink: '#d9edf7',
    inkSoft: '#a8c4d6',
    dim: '#6f8ea4',
    dimmer: '#496274',
    cyan: '#5fe6d8',
    amber: '#ffb257',
    red: '#ff5f6d',
    violet: '#b07dff',
    gold: '#ffd166',
    green: '#8fe27a',
    white: '#ffffff',
    fontBody: '"Segoe UI", "Helvetica Neue", Helvetica, Arial, sans-serif',
    fontMono: '"JetBrains Mono", "Consolas", "DejaVu Sans Mono", "SF Mono", monospace',
    fontTitle: '"Bahnschrift", "DIN Alternate", "Segoe UI Semibold", "Helvetica Neue", Arial, sans-serif'
  };

  var FONT = {
    body: function (size, weight) {
      return (weight || 400) + ' ' + size + 'px ' + THEME.fontBody;
    },
    mono: function (size, weight) {
      return (weight || 400) + ' ' + size + 'px ' + THEME.fontMono;
    },
    title: function (size, weight) {
      return (weight || 700) + ' ' + size + 'px ' + THEME.fontTitle;
    }
  };

  var measureCache = {};

  /* letter-spaced text. Canvas 2D has no reliable letterSpacing everywhere,
     so glyphs are placed manually. Returns the drawn width. */
  function text(ctx, str, x, y, opt) {
    opt = opt || {};
    var font = opt.font || FONT.body(14);
    var spacing = opt.spacing || 0;
    var align = opt.align || 'left';
    ctx.font = font;
    ctx.textBaseline = opt.baseline || 'alphabetic';
    var key = font + '|' + str;
    var w = measureCache[key];
    if (w === undefined) {
      w = ctx.measureText(str).width;
      measureCache[key] = w;
    }
    var total = w + spacing * Math.max(0, str.length - 1);
    var sx = x;
    if (align === 'center') sx = x - total / 2;
    else if (align === 'right') sx = x - total;
    if (opt.shadow) {
      ctx.save();
      ctx.shadowColor = opt.shadow;
      ctx.shadowBlur = opt.shadowBlur || 12;
      ctx.fillStyle = opt.color || THEME.ink;
      drawSpaced(ctx, str, sx, y, spacing);
      ctx.restore();
    } else {
      ctx.fillStyle = opt.color || THEME.ink;
      drawSpaced(ctx, str, sx, y, spacing);
    }
    return total;
  }

  function drawSpaced(ctx, str, x, y, spacing) {
    if (!spacing) { ctx.fillText(str, x, y); return; }
    var cx = x;
    for (var i = 0; i < str.length; i++) {
      var ch = str[i];
      ctx.fillText(ch, cx, y);
      cx += ctx.measureText(ch).width + spacing;
    }
  }

  function measure(ctx, str, font, spacing) {
    ctx.font = font;
    return ctx.measureText(str).width + (spacing || 0) * Math.max(0, str.length - 1);
  }

  /* Wrap text into lines that fit `maxW`. */
  function wrap(ctx, str, maxW, font) {
    ctx.font = font;
    var words = str.split(' ');
    var lines = [], cur = '';
    for (var i = 0; i < words.length; i++) {
      var test = cur ? cur + ' ' + words[i] : words[i];
      if (ctx.measureText(test).width > maxW && cur) { lines.push(cur); cur = words[i]; }
      else cur = test;
    }
    if (cur) lines.push(cur);
    return lines;
  }

  SM.Theme = THEME;
  SM.Font = FONT;
  SM.Text = { draw: text, measure: measure, wrap: wrap, drawSpaced: drawSpaced };

  /** Path helpers used across art + UI **/
  var Path = {
    roundRect: function (ctx, x, y, w, h, r) {
      r = Math.min(r, Math.abs(w) / 2, Math.abs(h) / 2);
      ctx.beginPath();
      ctx.moveTo(x + r, y);
      ctx.lineTo(x + w - r, y);
      ctx.quadraticCurveTo(x + w, y, x + w, y + r);
      ctx.lineTo(x + w, y + h - r);
      ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
      ctx.lineTo(x + r, y + h);
      ctx.quadraticCurveTo(x, y + h, x, y + h - r);
      ctx.lineTo(x, y + r);
      ctx.quadraticCurveTo(x, y, x + r, y);
      ctx.closePath();
    },
    /* angular panel with clipped corner — the game's signature shape */
    cutRect: function (ctx, x, y, w, h, cut) {
      cut = cut || 12;
      ctx.beginPath();
      ctx.moveTo(x + cut, y);
      ctx.lineTo(x + w, y);
      ctx.lineTo(x + w, y + h - cut);
      ctx.lineTo(x + w - cut, y + h);
      ctx.lineTo(x, y + h);
      ctx.lineTo(x, y + cut);
      ctx.closePath();
    },
    cutRectR: function (ctx, x, y, w, h, cut) {
      cut = cut || 12;
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.lineTo(x + w - cut, y);
      ctx.lineTo(x + w, y + cut);
      ctx.lineTo(x + w, y + h);
      ctx.lineTo(x + cut, y + h);
      ctx.lineTo(x, y + h - cut);
      ctx.closePath();
    },
    polygon: function (ctx, pts) {
      ctx.beginPath();
      ctx.moveTo(pts[0].x, pts[0].y);
      for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
      ctx.closePath();
    },
    star4: function (ctx, x, y, r1, r2) {
      ctx.beginPath();
      for (var i = 0; i < 8; i++) {
        var a = (i / 8) * Math.PI * 2 - Math.PI / 2;
        var r = i % 2 ? r2 : r1;
        var px = x + Math.cos(a) * r, py = y + Math.sin(a) * r;
        if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
      }
      ctx.closePath();
    }
  };
  SM.Path = Path;
})(typeof window !== 'undefined' ? window : globalThis);
