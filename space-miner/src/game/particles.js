/* =========================================================================
   SPACE MINER — pooled particle system (sparks, smoke, debris, rings, text)
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var U = SM.U;

  function Particles(max) {
    this.list = [];
    this.max = max || 900;
    this.beams = [];
    this.rings = [];
  }

  Particles.prototype.add = function (p) {
    if (this.list.length >= this.max) return null;
    p.life = p.life || 0.5;
    p.maxLife = p.maxLife || p.life;
    p.size = p.size || 2;
    p.color = p.color || '#ffffff';
    p.type = p.type || 'spark';
    p.vx = p.vx || 0; p.vy = p.vy || 0;
    p.drag = p.drag === undefined ? 1.6 : p.drag;
    p.grav = p.grav || 0;
    p.rot = p.rot || 0;
    p.vr = p.vr || 0;
    p.alpha = p.alpha === undefined ? 1 : p.alpha;
    this.list.push(p);
    return p;
  };

  Particles.prototype.burst = function (x, y, n, opt) {
    opt = opt || {};
    var spd = opt.speed || 120;
    var spread = opt.spread === undefined ? Math.PI * 2 : opt.spread;
    var dir = opt.angle === undefined ? 0 : opt.angle;
    for (var i = 0; i < n; i++) {
      var a = dir + (Math.random() - 0.5) * spread;
      var s = spd * (0.35 + Math.random() * 0.9);
      this.add({
        x: x + (Math.random() - 0.5) * (opt.jitter || 0),
        y: y + (Math.random() - 0.5) * (opt.jitter || 0),
        vx: Math.cos(a) * s, vy: Math.sin(a) * s,
        life: (opt.life || 0.6) * (0.6 + Math.random() * 0.8),
        maxLife: opt.life || 0.6,
        size: (opt.size || 2) * (0.6 + Math.random() * 0.9),
        color: opt.color || '#ffffff',
        type: opt.type || 'spark',
        drag: opt.drag === undefined ? 1.4 : opt.drag,
        grav: opt.grav || 0,
        rot: Math.random() * 6.28,
        vr: (Math.random() - 0.5) * 12
      });
    }
  };

  Particles.prototype.ring = function (x, y, r0, r1, life, color, width) {
    if (this.rings.length > 40) return;
    this.rings.push({ x: x, y: y, r0: r0, r1: r1, life: life, maxLife: life, color: color, width: width || 2 });
  };

  Particles.prototype.text = function (x, y, str, color, size) {
    this.add({
      x: x, y: y, vx: (Math.random() - 0.5) * 20, vy: -34,
      life: 1.1, maxLife: 1.1, size: size || 12, color: color || '#ffffff',
      type: 'text', text: str, drag: 0.4, alpha: 1
    });
  };

  Particles.prototype.update = function (dt) {
    var arr = this.list;
    for (var i = arr.length - 1; i >= 0; i--) {
      var p = arr[i];
      p.life -= dt;
      if (p.life <= 0) { arr.splice(i, 1); continue; }
      var d = Math.pow(0.5, dt * p.drag);
      p.vx *= d; p.vy *= d;
      p.vy += p.grav * dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.rot += p.vr * dt;
    }
    for (var r = this.rings.length - 1; r >= 0; r--) {
      var g = this.rings[r];
      g.life -= dt;
      if (g.life <= 0) this.rings.splice(r, 1);
    }
  };

  Particles.prototype.clear = function () {
    this.list.length = 0;
    this.rings.length = 0;
  };

  SM.Particles = Particles;
})(typeof window !== 'undefined' ? window : globalThis);
