/* =========================================================================
   SPACE MINER — core utilities, math, deterministic randomness
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  var TAU = Math.PI * 2;

  function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
  function lerp(a, b, t) { return a + (b - a) * t; }
  function invLerp(a, b, v) { return b === a ? 0 : clamp((v - a) / (b - a), 0, 1); }
  function smooth(t) { return t * t * (3 - 2 * t); }
  function smoothstep(a, b, v) { return smooth(invLerp(a, b, v)); }
  function dist2(ax, ay, bx, by) { var dx = bx - ax, dy = by - ay; return dx * dx + dy * dy; }
  function dist(ax, ay, bx, by) { return Math.sqrt(dist2(ax, ay, bx, by)); }
  function sign(v) { return v < 0 ? -1 : v > 0 ? 1 : 0; }
  function round2(v) { return Math.round(v * 100) / 100; }

  function angleDiff(a, b) {
    var d = (b - a) % TAU;
    if (d > Math.PI) d -= TAU;
    if (d < -Math.PI) d += TAU;
    return d;
  }
  function angleLerp(a, b, t) { return a + angleDiff(a, b) * t; }
  function approach(cur, target, rate) {
    if (cur < target) return Math.min(target, cur + rate);
    if (cur > target) return Math.max(target, cur - rate);
    return target;
  }

  /* ---------- deterministic RNG (mulberry32) ---------- */
  function RNG(seed) {
    this.s = (seed >>> 0) || 0x9e3779b9;
  }
  RNG.prototype.next = function () {
    this.s = (this.s + 0x6D2B79F5) >>> 0;
    var t = this.s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  RNG.prototype.range = function (a, b) { return a + (b - a) * this.next(); };
  RNG.prototype.int = function (a, b) { return Math.floor(this.range(a, b + 1)); };
  RNG.prototype.pick = function (arr) { return arr[Math.floor(this.next() * arr.length) % arr.length]; };
  RNG.prototype.chance = function (p) { return this.next() < p; };
  RNG.prototype.sign = function () { return this.next() < 0.5 ? -1 : 1; };
  RNG.prototype.shuffle = function (arr) {
    for (var i = arr.length - 1; i > 0; i--) {
      var j = Math.floor(this.next() * (i + 1));
      var t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
    return arr;
  };
  RNG.prototype.weighted = function (items, weightFn) {
    var total = 0, i;
    for (i = 0; i < items.length; i++) total += Math.max(0, weightFn(items[i], i));
    if (total <= 0) return items[0];
    var r = this.next() * total;
    for (i = 0; i < items.length; i++) {
      r -= Math.max(0, weightFn(items[i], i));
      if (r <= 0) return items[i];
    }
    return items[items.length - 1];
  };
  RNG.prototype.gauss = function () {
    var u = 1 - this.next(), v = this.next();
    return Math.sqrt(-2 * Math.log(u)) * Math.cos(TAU * v);
  };

  function hashStr(str) {
    var h = 2166136261 >>> 0;
    for (var i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = Math.imul(h, 16777619);
    }
    return h >>> 0;
  }

  /* ---------- colour helpers ---------- */
  function hexToRgb(hex) {
    hex = hex.replace('#', '');
    if (hex.length === 3) hex = hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
    var n = parseInt(hex, 16);
    return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255 };
  }
  function rgba(r, g, b, a) {
    return 'rgba(' + (r | 0) + ',' + (g | 0) + ',' + (b | 0) + ',' + (a === undefined ? 1 : a) + ')';
  }
  function hexA(hex, a) {
    var c = hexToRgb(hex);
    return rgba(c.r, c.g, c.b, a);
  }
  function mixHex(h1, h2, t) {
    var a = hexToRgb(h1), b = hexToRgb(h2);
    return rgba(lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), 1);
  }
  function shade(hex, amt) {
    var c = hexToRgb(hex);
    if (amt >= 0) return rgba(lerp(c.r, 255, amt), lerp(c.g, 255, amt), lerp(c.b, 255, amt), 1);
    return rgba(c.r * (1 + amt), c.g * (1 + amt), c.b * (1 + amt), 1);
  }

  /* ---------- formatting ---------- */
  function fmtNum(n) {
    n = Math.round(n);
    if (Math.abs(n) >= 1e9) return (n / 1e9).toFixed(2) + 'B';
    if (Math.abs(n) >= 1e6) return (n / 1e6).toFixed(2) + 'M';
    if (Math.abs(n) >= 1e4) return (n / 1e3).toFixed(1) + 'K';
    return String(n);
  }
  function fmtCredits(n) { return fmtNum(n) + ' CR'; }
  function pad(n, len) {
    var s = String(n);
    while (s.length < len) s = '0' + s;
    return s;
  }

  /* ---------- geometry ---------- */
  function pointInPoly(px, py, pts) {
    var inside = false;
    for (var i = 0, j = pts.length - 1; i < pts.length; j = i++) {
      var xi = pts[i].x, yi = pts[i].y, xj = pts[j].x, yj = pts[j].y;
      if (((yi > py) !== (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) inside = !inside;
    }
    return inside;
  }
  function polyRadiusAt(pts, angle) {
    // radius of a star-shaped polygon along a ray at `angle`
    var dx = Math.cos(angle), dy = Math.sin(angle);
    var best = 0;
    for (var i = 0; i < pts.length; i++) {
      var a = pts[i], b = pts[(i + 1) % pts.length];
      var ex = b.x - a.x, ey = b.y - a.y;
      var den = dx * ey - dy * ex;
      if (Math.abs(den) < 1e-9) continue;
      var t = (a.x * ey - a.y * ex) / den;             // ray param
      var u = (a.x * dy - a.y * dx) / (dx * ey - dy * ex); // segment param
      if (t > 0 && u >= -0.0001 && u <= 1.0001 && t > best) best = t;
    }
    return best;
  }

  /* ---------- canvas ---------- */
  function makeCanvas(w, h) {
    var c;
    if (typeof document !== 'undefined' && document.createElement) {
      c = document.createElement('canvas');
      c.width = Math.max(1, Math.ceil(w));
      c.height = Math.max(1, Math.ceil(h));
    } else {
      c = new root.SM.SkiaCanvas(Math.max(1, Math.ceil(w)), Math.max(1, Math.ceil(h)));
    }
    return c;
  }

  /* ---------- misc ---------- */
  function pickWeightedRanges(rng, table) { return rng.weighted(table, function (e) { return e.w; }); }

  function easeOutCubic(t) { return 1 - Math.pow(1 - t, 3); }
  function easeInCubic(t) { return t * t * t; }
  function easeOutBack(t) { var c = 1.70158; return 1 + (c + 1) * Math.pow(t - 1, 3) + c * Math.pow(t - 1, 2); }
  function easeInOut(t) { return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2; }

  SM.U = {
    TAU: TAU, clamp: clamp, lerp: lerp, invLerp: invLerp, smooth: smooth, smoothstep: smoothstep,
    dist: dist, dist2: dist2, sign: sign, round2: round2, angleDiff: angleDiff, angleLerp: angleLerp,
    approach: approach, RNG: RNG, hashStr: hashStr, hexToRgb: hexToRgb, rgba: rgba, hexA: hexA,
    mixHex: mixHex, shade: shade, fmtNum: fmtNum, fmtCredits: fmtCredits, pad: pad,
    pointInPoly: pointInPoly, polyRadiusAt: polyRadiusAt, makeCanvas: makeCanvas,
    pickWeightedRanges: pickWeightedRanges, easeOutCubic: easeOutCubic, easeInCubic: easeInCubic,
    easeOutBack: easeOutBack, easeInOut: easeInOut
  };
})(typeof window !== 'undefined' ? window : globalThis);
