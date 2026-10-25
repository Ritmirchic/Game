/* =========================================================================
   SPACE MINER — keyboard / mouse / touch input state
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});

  function Input(canvas) {
    this.canvas = canvas;
    this.keys = {};
    this.pressed = {};
    this.mouse = { x: 0, y: 0, down: false, right: false, wheel: 0, moved: 0 };
    this.clicks = [];
    this.ax = 0; this.ay = 0;
    this.mine = false;
    this.fire = false;
    this.boost = false;
    this.aimWorld = null;
    this.touch = { active: false, id: null, x: 0, y: 0 };
    this.anyKey = false;
    this.touchMode = false;
    this.bind();
  }

  var BLOCK = { ArrowUp: 1, ArrowDown: 1, ArrowLeft: 1, ArrowRight: 1, Space: 1, Tab: 1, KeyW: 1, KeyA: 1, KeyS: 1, KeyD: 1 };

  Input.prototype.bind = function () {
    var self = this;
    if (!root.addEventListener) return;

    root.addEventListener('keydown', function (e) {
      if (BLOCK[e.code]) e.preventDefault();
      if (!self.keys[e.code]) self.pressed[e.code] = true;
      self.keys[e.code] = true;
      self.anyKey = true;
    });
    root.addEventListener('keyup', function (e) {
      self.keys[e.code] = false;
    });
    root.addEventListener('blur', function () {
      self.keys = {};
      self.mouse.down = false;
      self.mouse.right = false;
    });

    var c = this.canvas;
    c.addEventListener('mousemove', function (e) {
      var r = c.getBoundingClientRect();
      self.mouse.x = e.clientX - r.left;
      self.mouse.y = e.clientY - r.top;
      self.mouse.moved = 8;
    });
    c.addEventListener('mousedown', function (e) {
      var r = c.getBoundingClientRect();
      self.mouse.x = e.clientX - r.left;
      self.mouse.y = e.clientY - r.top;
      if (e.button === 0) { self.mouse.down = true; self.clicks.push({ x: self.mouse.x, y: self.mouse.y, button: 0 }); }
      if (e.button === 2) self.mouse.right = true;
      self.anyKey = true;
    });
    root.addEventListener('mouseup', function (e) {
      if (e.button === 0) self.mouse.down = false;
      if (e.button === 2) self.mouse.right = false;
    });
    c.addEventListener('contextmenu', function (e) { e.preventDefault(); });
    c.addEventListener('wheel', function (e) {
      e.preventDefault();
      self.mouse.wheel += e.deltaY;
    }, { passive: false });

    // ---- touch: drag anywhere to steer + aim + mine
    c.addEventListener('touchstart', function (e) {
      e.preventDefault();
      self.touchMode = true;
      var t = e.changedTouches[0];
      var r = c.getBoundingClientRect();
      self.touch.id = t.identifier;
      self.touch.active = true;
      self.touch.x = t.clientX - r.left;
      self.touch.y = t.clientY - r.top;
      self.mouse.x = self.touch.x;
      self.mouse.y = self.touch.y;
      self.clicks.push({ x: self.touch.x, y: self.touch.y, button: 0 });
      self.mouse.down = true;
      self.anyKey = true;
    }, { passive: false });
    c.addEventListener('touchmove', function (e) {
      e.preventDefault();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var t = e.changedTouches[i];
        if (t.identifier !== self.touch.id) continue;
        var r = c.getBoundingClientRect();
        self.touch.x = t.clientX - r.left;
        self.touch.y = t.clientY - r.top;
        self.mouse.x = self.touch.x;
        self.mouse.y = self.touch.y;
      }
    }, { passive: false });
    var endTouch = function (e) {
      for (var i = 0; i < e.changedTouches.length; i++) {
        if (e.changedTouches[i].identifier === self.touch.id) {
          self.touch.active = false;
          self.mouse.down = false;
        }
      }
    };
    c.addEventListener('touchend', endTouch);
    c.addEventListener('touchcancel', endTouch);
  };

  Input.prototype.consumeClick = function () {
    return this.clicks.shift() || null;
  };
  Input.prototype.peekClick = function () {
    return this.clicks[0] || null;
  };
  Input.prototype.consumeWheel = function () {
    var w = this.mouse.wheel;
    this.mouse.wheel = 0;
    return w;
  };
  Input.prototype.wasPressed = function (code) {
    if (this.pressed[code]) { this.pressed[code] = false; return true; }
    return false;
  };
  Input.prototype.endFrame = function () {
    this.pressed = {};
    if (this.mouse.moved > 0) this.mouse.moved--;
  };

  /** recompute per-frame flight axes from keys */
  Input.prototype.sample = function () {
    var k = this.keys;
    var ax = 0, ay = 0;
    if (k.KeyW || k.ArrowUp) ay -= 1;
    if (k.KeyS || k.ArrowDown) ay += 1;
    if (k.KeyA || k.ArrowLeft) ax -= 1;
    if (k.KeyD || k.ArrowRight) ax += 1;
    if (this.touchMode && this.touch.active) {
      // steer toward finger, mining at the same time
      var cx = this.screenW ? this.screenW / 2 : 0;
      var cy = this.screenH ? this.screenH / 2 : 0;
      var dx = this.touch.x - cx, dy = this.touch.y - cy;
      var d = Math.hypot(dx, dy);
      if (d > 26) { ax = dx / d; ay = dy / d; }
      this.mine = true;
    }
    this.ax = ax; this.ay = ay;
    this.boost = !!(k.ShiftLeft || k.ShiftRight);
    this.fire = !!(k.Space || k.KeyF || this.mouse.right);
    if (!this.touchMode) this.mine = !!this.mouse.down;
  };

  SM.Input = Input;
})(typeof window !== 'undefined' ? window : globalThis);
