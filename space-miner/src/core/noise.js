/* =========================================================================
   SPACE MINER — procedural noise (value / fbm / ridged), seeded
   ========================================================================= */
(function (root) {
  'use strict';
  var SM = root.SM || (root.SM = {});
  var lerp = SM.U.lerp, smooth = SM.U.smooth;

  function Noise(seed) {
    var RNG = SM.U.RNG;
    var rng = new RNG(seed || 1337);
    this.p = new Uint8Array(512);
    var perm = new Uint8Array(256);
    for (var i = 0; i < 256; i++) perm[i] = i;
    rng.shuffle(perm);
    for (var j = 0; j < 512; j++) this.p[j] = perm[j & 255];
  }

  Noise.prototype.grad = function (h, x, y) {
    switch (h & 7) {
      case 0: return x + y;
      case 1: return -x + y;
      case 2: return x - y;
      case 3: return -x - y;
      case 4: return x;
      case 5: return -x;
      case 6: return y;
      default: return -y;
    }
  };

  // classic 2D perlin in [-1,1]
  Noise.prototype.perlin = function (x, y) {
    var X = Math.floor(x) & 255, Y = Math.floor(y) & 255;
    var xf = x - Math.floor(x), yf = y - Math.floor(y);
    var u = smooth(xf), v = smooth(yf);
    var p = this.p;
    var aa = p[p[X] + Y], ab = p[p[X] + Y + 1];
    var ba = p[p[X + 1] + Y], bb = p[p[X + 1] + Y + 1];
    var x1 = lerp(this.grad(aa, xf, yf), this.grad(ba, xf - 1, yf), u);
    var x2 = lerp(this.grad(ab, xf, yf - 1), this.grad(bb, xf - 1, yf - 1), u);
    return lerp(x1, x2, v);
  };

  Noise.prototype.fbm = function (x, y, oct, lac, gain) {
    oct = oct || 4; lac = lac || 2; gain = gain === undefined ? 0.5 : gain;
    var sum = 0, amp = 1, norm = 0;
    for (var i = 0; i < oct; i++) {
      sum += this.perlin(x, y) * amp;
      norm += amp;
      amp *= gain; x *= lac; y *= lac;
    }
    return sum / norm;
  };

  Noise.prototype.ridged = function (x, y, oct) {
    oct = oct || 4;
    var sum = 0, amp = 0.5, norm = 0;
    for (var i = 0; i < oct; i++) {
      var n = 1 - Math.abs(this.perlin(x, y));
      sum += n * n * amp;
      norm += amp;
      amp *= 0.5; x *= 2.03; y *= 2.03;
    }
    return sum / norm;
  };

  Noise.prototype.warp = function (x, y, amt) {
    var qx = this.perlin(x * 0.5 + 5.2, y * 0.5 + 1.3);
    var qy = this.perlin(x * 0.5 + 9.7, y * 0.5 + 4.8);
    return this.perlin(x + amt * qx, y + amt * qy);
  };

  SM.Noise = Noise;
})(typeof window !== 'undefined' ? window : globalThis);
