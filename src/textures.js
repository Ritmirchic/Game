/* ==========================================================================
   НЕЕВКЛИД — процедурные текстуры. Ни одного изображения извне:
   всё рисуется кодом на canvas и разбирается на R/G/B/A-плоскости.
   ========================================================================== */
(function (root) {
  'use strict';
  root.NE = root.NE || {};

  const S = 64;

  function mkCanvas(w, h) {
    const c = document.createElement('canvas');
    c.width = w; c.height = h || w;
    return c;
  }

  function toTex(c) {
    const w = c.width, h = c.height;
    const d = c.getContext('2d').getImageData(0, 0, w, h).data;
    const n = w * h;
    const r = new Uint8ClampedArray(n), g = new Uint8ClampedArray(n),
          b = new Uint8ClampedArray(n), a = new Uint8ClampedArray(n);
    for (let i = 0, p = 0; i < n; i++, p += 4) { r[i] = d[p]; g[i] = d[p + 1]; b[i] = d[p + 2]; a[i] = d[p + 3]; }
    return { w, h, r, g, b, a };
  }

  function author(w, h, fn) {
    const c = mkCanvas(w, h);
    const ctx = c.getContext('2d');
    ctx.clearRect(0, 0, w, h);
    fn(ctx, w, h);
    return toTex(c);
  }

  function pixelPass(ctx, w, h, fn) {
    const img = ctx.getImageData(0, 0, w, h);
    const d = img.data;
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) fn(d, (y * w + x) * 4, x, y);
    ctx.putImageData(img, 0, 0);
  }

  /* бесшовный value-noise */
  function tileNoise(rnd, w, h, cell) {
    const gw = Math.max(1, (w / cell) | 0), gh = Math.max(1, (h / cell) | 0);
    const g = new Float32Array(gw * gh);
    for (let i = 0; i < g.length; i++) g[i] = rnd();
    const out = new Float32Array(w * h);
    const sm = (t) => t * t * (3 - 2 * t);
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const fx = x / cell, fy = y / cell;
        const x0 = Math.floor(fx), y0 = Math.floor(fy);
        const tx = sm(fx - x0), ty = sm(fy - y0);
        const x1 = (x0 + 1) % gw, y1 = (y0 + 1) % gh;
        const xa = x0 % gw, ya = y0 % gh;
        const a = g[ya * gw + xa], b = g[ya * gw + x1];
        const c = g[y1 * gw + xa], d = g[y1 * gw + x1];
        out[y * w + x] = (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
      }
    }
    return out;
  }

  function fbm(rnd, w, h, cells) {
    const acc = new Float32Array(w * h);
    let total = 0;
    for (const c of cells) {
      const n = tileNoise(rnd, w, h, c);
      const weight = c / cells[0];              // крупные детали весят больше
      for (let i = 0; i < acc.length; i++) acc[i] += n[i] * weight;
      total += weight;
    }
    for (let i = 0; i < acc.length; i++) acc[i] /= total;
    return acc;
  }

  const mix = (a, b, t) => a + (b - a) * t;

  /* альбедо-усилитель: процедурный бетон слишком честный тёмный,
     а фонарик должен его высвечивать, а не угадывать */
  function gain(tex, k) {
    for (let i = 0; i < tex.r.length; i++) {
      tex.r[i] = Math.min(255, tex.r[i] * k);
      tex.g[i] = Math.min(255, tex.g[i] * k);
      tex.b[i] = Math.min(255, tex.b[i] * k);
    }
    return tex;
  }

  /* ============================ СТЕНЫ ЗОН ============================ */

  function wallVestibule(rnd) {        // холодный бетон с опалубкой
    return author(S, S, (ctx, w, h) => {
      const n = fbm(rnd, w, h, [16, 8, 4]);
      ctx.fillStyle = '#2a323c'; ctx.fillRect(0, 0, w, h);
      pixelPass(ctx, w, h, (d, i, x, y) => {
        const v = n[y * w + x];
        const c = 34 + v * 40;
        d[i] = c * 0.86; d[i + 1] = c * 0.97; d[i + 2] = c * 1.12; d[i + 3] = 255;
      });
      // швы опалубки
      ctx.strokeStyle = 'rgba(10,14,18,0.85)'; ctx.lineWidth = 2;
      for (let y = 0; y <= h; y += 32) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }
      ctx.beginPath(); ctx.moveTo(32, 0); ctx.lineTo(32, h); ctx.stroke();
      ctx.strokeStyle = 'rgba(190,220,240,0.10)'; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(0, 33); ctx.lineTo(w, 33); ctx.stroke();
      // потёки
      for (let k = 0; k < 7; k++) {
        const x = rnd() * w, y0 = rnd() * h * 0.5, len = 8 + rnd() * 30;
        const g = ctx.createLinearGradient(x, y0, x, y0 + len);
        g.addColorStop(0, 'rgba(8,12,16,0.35)'); g.addColorStop(1, 'rgba(8,12,16,0)');
        ctx.fillStyle = g; ctx.fillRect(x, y0, 1 + rnd() * 2, len);
      }
      // трещина
      ctx.strokeStyle = 'rgba(6,9,12,0.7)'; ctx.lineWidth = 1;
      ctx.beginPath();
      let cx = rnd() * w, cy = 0;
      ctx.moveTo(cx, cy);
      while (cy < h) { cy += 3 + rnd() * 5; cx += (rnd() - 0.5) * 7; ctx.lineTo(cx, cy); }
      ctx.stroke();
    });
  }

  function wallArchive(rnd) {          // ржавые панели с заклёпками
    return author(S, S, (ctx, w, h) => {
      const n = fbm(rnd, w, h, [32, 8, 4]);
      ctx.fillStyle = '#3a2a1c'; ctx.fillRect(0, 0, w, h);
      pixelPass(ctx, w, h, (d, i, x, y) => {
        const v = n[y * w + x];
        const band = 0.82 + 0.18 * Math.sin((y / h) * Math.PI * 2);
        d[i] = (58 + v * 74) * band;
        d[i + 1] = (38 + v * 46) * band;
        d[i + 2] = (24 + v * 26) * band;
        d[i + 3] = 255;
      });
      // ржавые пятна
      for (let k = 0; k < 12; k++) {
        const x = rnd() * w, y = rnd() * h, r = 3 + rnd() * 12;
        const g = ctx.createRadialGradient(x, y, 0, x, y, r);
        g.addColorStop(0, 'rgba(120,58,20,0.35)'); g.addColorStop(1, 'rgba(120,58,20,0)');
        ctx.fillStyle = g; ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.fill();
      }
      // панели и заклёпки
      ctx.strokeStyle = 'rgba(12,8,5,0.8)'; ctx.lineWidth = 2;
      ctx.strokeRect(1, 1, w - 2, h / 2 - 2); ctx.strokeRect(1, h / 2 + 1, w - 2, h / 2 - 2);
      ctx.fillStyle = 'rgba(190,150,110,0.35)';
      for (let y = 6; y < h; y += 16) for (let x = 6; x < w; x += 16) {
        ctx.beginPath(); ctx.arc(x, y, 1.6, 0, 7); ctx.fill();
      }
    });
  }

  function wallGallery(rnd) {          // чёрная плитка с неоновыми швами
    return author(S, S, (ctx, w, h) => {
      const n = fbm(rnd, w, h, [8, 4]);
      ctx.fillStyle = '#120a1a'; ctx.fillRect(0, 0, w, h);
      pixelPass(ctx, w, h, (d, i, x, y) => {
        const v = n[y * w + x];
        d[i] = 16 + v * 26; d[i + 1] = 8 + v * 14; d[i + 2] = 24 + v * 38; d[i + 3] = 255;
      });
      for (let k = 0; k < 2; k++) {
        const off = k * 32;
        ctx.strokeStyle = 'rgba(255,90,220,0.55)'; ctx.lineWidth = 1.4;
        ctx.beginPath(); ctx.moveTo(0, off + 0.5); ctx.lineTo(w, off + 0.5); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(off + 0.5, 0); ctx.lineTo(off + 0.5, h); ctx.stroke();
        ctx.strokeStyle = 'rgba(120,255,250,0.22)'; ctx.lineWidth = 3;
        ctx.beginPath(); ctx.moveTo(0, off + 0.5); ctx.lineTo(w, off + 0.5); ctx.stroke();
      }
      // глифы
      ctx.strokeStyle = 'rgba(200,140,255,0.16)'; ctx.lineWidth = 1;
      for (let k = 0; k < 5; k++) {
        const x = 8 + rnd() * 48, y = 8 + rnd() * 48, r = 3 + rnd() * 5;
        ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(x - r, y); ctx.lineTo(x + r, y); ctx.stroke();
      }
    });
  }

  function wallLower(rnd) {            // рифлёный металл, зелёная гниль
    return author(S, S, (ctx, w, h) => {
      ctx.fillStyle = '#101a16'; ctx.fillRect(0, 0, w, h);
      const n = fbm(rnd, w, h, [16, 4]);
      pixelPass(ctx, w, h, (d, i, x, y) => {
        const rib = 0.55 + 0.45 * Math.abs(Math.sin((x / w) * Math.PI * 8));
        const v = n[y * w + x];
        d[i] = (14 + v * 22) * rib;
        d[i + 1] = (34 + v * 48) * rib;
        d[i + 2] = (28 + v * 34) * rib;
        d[i + 3] = 255;
      });
      for (let k = 0; k < 9; k++) {
        const x = rnd() * w, y = rnd() * h, r = 4 + rnd() * 14;
        const g = ctx.createRadialGradient(x, y, 0, x, y, r);
        g.addColorStop(0, 'rgba(120,255,150,0.14)'); g.addColorStop(1, 'rgba(120,255,150,0)');
        ctx.fillStyle = g; ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.fill();
      }
      ctx.strokeStyle = 'rgba(0,0,0,0.55)'; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.moveTo(0, 32); ctx.lineTo(w, 32); ctx.stroke();
      ctx.strokeStyle = 'rgba(150,220,190,0.12)'; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(0, 33.5); ctx.lineTo(w, 33.5); ctx.stroke();
    });
  }

  /* ============================ ПОЛ И ПОТОЛОК ============================ */

  function floorTex(rnd, base, accent, style) {
    return author(S, S, (ctx, w, h) => {
      const n = fbm(rnd, w, h, [16, 8, 4]);
      ctx.fillStyle = base; ctx.fillRect(0, 0, w, h);
      pixelPass(ctx, w, h, (d, i, x, y) => {
        const v = n[y * w + x];
        d[i] = d[i] * (0.7 + v * 0.6);
        d[i + 1] = d[i + 1] * (0.7 + v * 0.6);
        d[i + 2] = d[i + 2] * (0.7 + v * 0.6);
        d[i + 3] = 255;
      });
      if (style === 0) {                     // бетон с сеткой
        ctx.strokeStyle = 'rgba(0,0,0,0.35)'; ctx.lineWidth = 1;
        for (let p = 0; p <= w; p += 32) {
          ctx.beginPath(); ctx.moveTo(p + 0.5, 0); ctx.lineTo(p + 0.5, h); ctx.stroke();
          ctx.beginPath(); ctx.moveTo(0, p + 0.5); ctx.lineTo(w, p + 0.5); ctx.stroke();
        }
      } else if (style === 1) {              // тёплая плитка
        ctx.strokeStyle = 'rgba(0,0,0,0.5)'; ctx.lineWidth = 2;
        for (let p = 0; p <= w; p += 16) {
          ctx.beginPath(); ctx.moveTo(p, 0); ctx.lineTo(p, h); ctx.stroke();
          ctx.beginPath(); ctx.moveTo(0, p); ctx.lineTo(w, p); ctx.stroke();
        }
        ctx.fillStyle = accent;
        for (let y = 0; y < h; y += 16) for (let x = 0; x < w; x += 16)
          if (((x + y) / 16) % 2 === 0) ctx.fillRect(x + 2, y + 2, 12, 12);
      } else if (style === 2) {              // тёмный камень с прожилками
        ctx.strokeStyle = accent; ctx.lineWidth = 1;
        for (let k = 0; k < 6; k++) {
          ctx.beginPath();
          let x = rnd() * w, y = 0;
          ctx.moveTo(x, y);
          while (y < h) { y += 4 + rnd() * 6; x += (rnd() - 0.5) * 10; ctx.lineTo(x, y); }
          ctx.stroke();
        }
      } else {                               // решётка
        ctx.fillStyle = 'rgba(0,0,0,0.75)';
        for (let y = 2; y < h; y += 8) ctx.fillRect(0, y, w, 3);
        for (let x = 2; x < w; x += 16) ctx.fillRect(x, 0, 3, h);
        ctx.fillStyle = accent;
        for (let x = 0; x < w; x += 16) ctx.fillRect(x, 0, 1, h);
      }
      // пыль/грязь
      for (let k = 0; k < 26; k++) {
        const x = rnd() * w, y = rnd() * h, r = 1 + rnd() * 6;
        const g = ctx.createRadialGradient(x, y, 0, x, y, r);
        g.addColorStop(0, 'rgba(0,0,0,0.28)'); g.addColorStop(1, 'rgba(0,0,0,0)');
        ctx.fillStyle = g; ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.fill();
      }
    });
  }

  function ceilTex(rnd, dark) {
    return author(S, S, (ctx, w, h) => {
      const n = fbm(rnd, w, h, [16, 8]);
      ctx.fillStyle = dark; ctx.fillRect(0, 0, w, h);
      pixelPass(ctx, w, h, (d, i, x, y) => {
        const v = n[y * w + x];
        d[i] *= 0.7 + v * 0.6; d[i + 1] *= 0.7 + v * 0.6; d[i + 2] *= 0.7 + v * 0.6; d[i + 3] = 255;
      });
      ctx.strokeStyle = 'rgba(0,0,0,0.5)'; ctx.lineWidth = 2;
      ctx.strokeRect(0, 0, w, h);
      ctx.strokeStyle = 'rgba(255,255,255,0.05)';
      ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();
    });
  }

  /* ============================ СПЕЦИАЛЬНЫЕ СТЕНЫ ============================ */

  function qdoorTex(rnd) {
    return author(S, S, (ctx, w, h) => {
      const g = ctx.createLinearGradient(0, 0, 0, h);
      g.addColorStop(0, 'rgba(10,4,18,0.95)');
      g.addColorStop(0.5, 'rgba(60,12,90,0.9)');
      g.addColorStop(1, 'rgba(10,4,18,0.95)');
      ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
      for (let k = 0; k < 26; k++) {
        const y = rnd() * h;
        ctx.strokeStyle = `rgba(${180 + rnd() * 75 | 0},${80 + rnd() * 80 | 0},255,${0.06 + rnd() * 0.3})`;
        ctx.lineWidth = 0.6 + rnd() * 1.6;
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y + (rnd() - 0.5) * 6); ctx.stroke();
      }
      ctx.strokeStyle = 'rgba(255,190,255,0.55)'; ctx.lineWidth = 1.4;
      ctx.strokeRect(2, 2, w - 4, h - 4);
      // руна в центре
      ctx.strokeStyle = 'rgba(255,235,255,0.75)'; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(w / 2, h / 2, 12, 0, 7); ctx.stroke();
      ctx.beginPath();
      for (let k = 0; k < 3; k++) {
        const a1 = (k / 3) * Math.PI * 2, a2 = ((k + 1) / 3) * Math.PI * 2;
        ctx.moveTo(w / 2 + Math.cos(a1) * 12, h / 2 + Math.sin(a1) * 12);
        ctx.lineTo(w / 2 + Math.cos(a2) * 12, h / 2 + Math.sin(a2) * 12);
      }
      ctx.stroke();
      ctx.fillStyle = 'rgba(255,255,255,0.9)';
      ctx.beginPath(); ctx.arc(w / 2, h / 2, 2.4, 0, 7); ctx.fill();
    });
  }

  function exitTex(rnd, lit, open) {
    return author(S, S, (ctx, w, h) => {
      if (open) {
        const g = ctx.createRadialGradient(w / 2, h / 2, 2, w / 2, h / 2, w * 0.7);
        g.addColorStop(0, 'rgba(255,255,255,1)');
        g.addColorStop(0.35, 'rgba(190,240,255,0.9)');
        g.addColorStop(1, 'rgba(20,40,60,0.2)');
        ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
        for (let k = 0; k < 40; k++) {
          ctx.strokeStyle = `rgba(255,255,255,${0.05 + rnd() * 0.2})`;
          ctx.lineWidth = 0.5 + rnd();
          const a = rnd() * Math.PI * 2;
          ctx.beginPath();
          ctx.moveTo(w / 2 + Math.cos(a) * 4, h / 2 + Math.sin(a) * 4);
          ctx.lineTo(w / 2 + Math.cos(a) * (20 + rnd() * 30), h / 2 + Math.sin(a) * (20 + rnd() * 30));
          ctx.stroke();
        }
        return;
      }
      const n = fbm(rnd, w, h, [16, 8]);
      ctx.fillStyle = '#1a1a20'; ctx.fillRect(0, 0, w, h);
      pixelPass(ctx, w, h, (d, i, x, y) => {
        const v = n[y * w + x];
        d[i] = 24 + v * 34; d[i + 1] = 24 + v * 34; d[i + 2] = 30 + v * 42; d[i + 3] = 255;
      });
      ctx.strokeStyle = 'rgba(0,0,0,0.7)'; ctx.lineWidth = 3;
      ctx.strokeRect(1.5, 1.5, w - 3, h - 3);
      ctx.strokeStyle = 'rgba(150,170,190,0.25)'; ctx.lineWidth = 1;
      ctx.strokeRect(5.5, 5.5, w - 11, h - 11);
      // печать с пятью гнёздами под осколки
      ctx.strokeStyle = 'rgba(190,215,235,0.5)'; ctx.lineWidth = 1.6;
      ctx.beginPath(); ctx.arc(w / 2, h / 2, 17, 0, 7); ctx.stroke();
      ctx.beginPath(); ctx.arc(w / 2, h / 2, 21, 0, 7); ctx.stroke();
      for (let k = 0; k < 5; k++) {
        const a = -Math.PI / 2 + (k / 5) * Math.PI * 2;
        const x = w / 2 + Math.cos(a) * 19, y = h / 2 + Math.sin(a) * 19;
        if (k < lit) {
          const g = ctx.createRadialGradient(x, y, 0, x, y, 7);
          g.addColorStop(0, 'rgba(255,255,255,1)');
          g.addColorStop(0.4, 'rgba(140,230,255,0.85)');
          g.addColorStop(1, 'rgba(60,160,255,0)');
          ctx.fillStyle = g; ctx.beginPath(); ctx.arc(x, y, 7, 0, 7); ctx.fill();
        } else {
          ctx.fillStyle = 'rgba(10,12,16,0.9)';
          ctx.beginPath(); ctx.arc(x, y, 3.2, 0, 7); ctx.fill();
          ctx.strokeStyle = 'rgba(120,140,160,0.4)'; ctx.lineWidth = 1;
          ctx.beginPath(); ctx.arc(x, y, 3.2, 0, 7); ctx.stroke();
        }
      }
    });
  }

  /* ============================ ДЕКАЛИ НА ПОЛУ ============================ */

  function decalRune(rnd) {
    return author(S, S, (ctx, w, h) => {
      ctx.strokeStyle = 'rgba(230,150,255,0.75)'; ctx.lineWidth = 1.6;
      ctx.beginPath(); ctx.arc(w / 2, h / 2, 22, 0, 7); ctx.stroke();
      ctx.strokeStyle = 'rgba(230,150,255,0.4)';
      ctx.beginPath(); ctx.arc(w / 2, h / 2, 27, 0, 7); ctx.stroke();
      ctx.beginPath();
      for (let k = 0; k < 6; k++) {
        const a = (k / 6) * Math.PI * 2;
        ctx.moveTo(w / 2, h / 2);
        ctx.lineTo(w / 2 + Math.cos(a) * 22, h / 2 + Math.sin(a) * 22);
      }
      ctx.stroke();
    });
  }

  function decalPortal(rnd, hue) {
    return author(S, S, (ctx, w, h) => {
      const col = `hsla(${hue},95%,65%,`;
      for (let k = 0; k < 4; k++) {
        ctx.strokeStyle = col + (0.55 - k * 0.11) + ')';
        ctx.lineWidth = 2.2 - k * 0.4;
        ctx.beginPath(); ctx.arc(w / 2, h / 2, 10 + k * 6.5, 0, 7); ctx.stroke();
      }
      const g = ctx.createRadialGradient(w / 2, h / 2, 0, w / 2, h / 2, 30);
      g.addColorStop(0, col + '0.5)'); g.addColorStop(1, col + '0)');
      ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
      ctx.strokeStyle = col + '0.6)'; ctx.lineWidth = 1;
      for (let k = 0; k < 8; k++) {
        const a = (k / 8) * Math.PI * 2;
        ctx.beginPath();
        ctx.moveTo(w / 2 + Math.cos(a) * 12, h / 2 + Math.sin(a) * 12);
        ctx.lineTo(w / 2 + Math.cos(a) * 30, h / 2 + Math.sin(a) * 30);
        ctx.stroke();
      }
    });
  }

  /* ============================ СПРАЙТЫ ============================ */

  function fragSprite(rnd) {
    return author(S, S, (ctx, w, h) => {
      const cx = w / 2, cy = h / 2;
      const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, 30);
      g.addColorStop(0, 'rgba(255,255,255,0.95)');
      g.addColorStop(0.25, 'rgba(150,235,255,0.55)');
      g.addColorStop(1, 'rgba(60,150,255,0)');
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(cx, cy, 30, 0, 7); ctx.fill();
      // гранёный кристалл
      const pts = [[0, -20], [9, -6], [6, 16], [0, 21], [-6, 16], [-9, -6]];
      ctx.beginPath();
      pts.forEach((p, i) => i ? ctx.lineTo(cx + p[0], cy + p[1]) : ctx.moveTo(cx + p[0], cy + p[1]));
      ctx.closePath();
      const cg = ctx.createLinearGradient(cx - 10, cy - 20, cx + 10, cy + 20);
      cg.addColorStop(0, 'rgba(225,250,255,0.98)');
      cg.addColorStop(0.5, 'rgba(110,205,255,0.9)');
      cg.addColorStop(1, 'rgba(50,120,220,0.85)');
      ctx.fillStyle = cg; ctx.fill();
      ctx.strokeStyle = 'rgba(255,255,255,0.9)'; ctx.lineWidth = 1; ctx.stroke();
      ctx.strokeStyle = 'rgba(255,255,255,0.5)';
      ctx.beginPath();
      ctx.moveTo(cx, cy - 20); ctx.lineTo(cx, cy + 21);
      ctx.moveTo(cx - 9, cy - 6); ctx.lineTo(cx + 6, cy + 16);
      ctx.moveTo(cx + 9, cy - 6); ctx.lineTo(cx - 6, cy + 16);
      ctx.stroke();
    });
  }

  function angelSprite(rnd) {
    return author(64, 128, (ctx, w, h) => {
      const cx = w / 2;
      // аура
      const g = ctx.createRadialGradient(cx, h * 0.45, 4, cx, h * 0.45, 40);
      g.addColorStop(0, 'rgba(40,20,60,0.5)'); g.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
      // силуэт: вытянутая фигура в балахоне
      ctx.beginPath();
      ctx.moveTo(cx, 8);
      ctx.bezierCurveTo(cx + 9, 10, cx + 11, 22, cx + 9, 34);
      ctx.bezierCurveTo(cx + 20, 52, cx + 22, 92, cx + 17, h - 6);
      ctx.lineTo(cx - 17, h - 6);
      ctx.bezierCurveTo(cx - 22, 92, cx - 20, 52, cx - 9, 34);
      ctx.bezierCurveTo(cx - 11, 22, cx - 9, 10, cx, 8);
      ctx.closePath();
      const bg = ctx.createLinearGradient(0, 0, 0, h);
      bg.addColorStop(0, 'rgba(6,6,10,0.98)');
      bg.addColorStop(0.55, 'rgba(14,10,20,0.95)');
      bg.addColorStop(1, 'rgba(2,2,4,0.55)');
      ctx.fillStyle = bg; ctx.fill();
      // рваные края
      for (let k = 0; k < 220; k++) {
        const y = 10 + rnd() * (h - 16);
        const spread = 8 + (y / h) * 16;
        const x = cx + (rnd() - 0.5) * 2 * spread;
        ctx.fillStyle = `rgba(0,0,0,${0.25 + rnd() * 0.6})`;
        ctx.fillRect(x, y, 1 + rnd() * 2, 1 + rnd() * 3);
      }
      // глаза
      for (const ex of [-4.5, 4.5]) {
        const gg = ctx.createRadialGradient(cx + ex, 22, 0, cx + ex, 22, 7);
        gg.addColorStop(0, 'rgba(255,255,255,1)');
        gg.addColorStop(0.25, 'rgba(255,120,90,0.9)');
        gg.addColorStop(1, 'rgba(255,40,20,0)');
        ctx.fillStyle = gg; ctx.beginPath(); ctx.arc(cx + ex, 22, 7, 0, 7); ctx.fill();
      }
    });
  }

  function portalSprite(rnd, hue) {
    return author(64, 96, (ctx, w, h) => {
      const cx = w / 2, cy = h / 2 + 8;
      const col = (a) => `hsla(${hue},95%,${(60 + a * 20) | 0}%,`;
      // арка
      ctx.lineWidth = 3;
      for (let k = 0; k < 5; k++) {
        ctx.strokeStyle = col(0.5 - k * 0.07) + '0.9)';
        ctx.beginPath();
        ctx.ellipse(cx, cy, 20 - k * 3.2, 34 - k * 5.5, 0, 0, Math.PI * 2);
        ctx.stroke();
      }
      const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, 34);
      g.addColorStop(0, `hsla(${hue},100%,92%,0.95)`);
      g.addColorStop(0.35, col(0.5) + '0.55)');
      g.addColorStop(1, col(0) + '0)');
      ctx.fillStyle = g; ctx.beginPath(); ctx.ellipse(cx, cy, 26, 40, 0, 0, 7); ctx.fill();
      // завихрения
      for (let k = 0; k < 26; k++) {
        const a = rnd() * Math.PI * 2, r = 6 + rnd() * 24;
        ctx.strokeStyle = `hsla(${hue + (rnd() - 0.5) * 40},100%,${(70 + rnd() * 25) | 0}%,${(0.15 + rnd() * 0.5).toFixed(3)})`;
        ctx.lineWidth = 0.6 + rnd() * 1.4;
        ctx.beginPath();
        ctx.arc(cx, cy, r, a, a + 0.5 + rnd() * 1.6);
        ctx.stroke();
      }
      // искры вверх
      for (let k = 0; k < 24; k++) {
        const x = cx + (rnd() - 0.5) * 34, y = rnd() * h;
        ctx.fillStyle = `hsla(${hue},100%,85%,${rnd() * 0.7})`;
        ctx.fillRect(x, y, 1.4, 1.4 + rnd() * 4);
      }
    });
  }

  function exitRune(rnd) {
    return author(S, S, (ctx, w, h) => {
      const cx = w / 2, cy = h / 2;
      const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, 26);
      g.addColorStop(0, 'rgba(255,255,255,0.9)');
      g.addColorStop(0.4, 'rgba(150,220,255,0.45)');
      g.addColorStop(1, 'rgba(60,140,255,0)');
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(cx, cy, 26, 0, 7); ctx.fill();
      ctx.strokeStyle = 'rgba(220,245,255,0.9)'; ctx.lineWidth = 1.6;
      ctx.beginPath(); ctx.arc(cx, cy, 15, 0, 7); ctx.stroke();
      ctx.beginPath();
      for (let k = 0; k < 5; k++) {
        const a = -Math.PI / 2 + (k * 4 * Math.PI * 2) / 5;
        const x = cx + Math.cos(a) * 15, y = cy + Math.sin(a) * 15;
        k ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.closePath(); ctx.stroke();
    });
  }

  /* ============================ СБОРКА ============================ */

  const PORTAL_HUES = [190, 45, 305, 130, 15];

  function build(seed) {
    const rnd = (s) => {
      let a = s >>> 0;
      return function () {
        a = (a + 0x6D2B79F5) >>> 0;
        let t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
      };
    };

    const t = {
      wall: [
        gain(wallVestibule(rnd(seed + 11)), 1.95),
        gain(wallArchive(rnd(seed + 22)), 1.9),
        gain(wallGallery(rnd(seed + 33)), 2.45),
        gain(wallLower(rnd(seed + 44)), 2.2)
      ],
      floor: [
        gain(floorTex(rnd(seed + 55), '#232a33', 'rgba(120,150,180,0.05)', 0), 2.15),
        gain(floorTex(rnd(seed + 66), '#33261a', 'rgba(190,140,80,0.10)', 1), 2.0),
        gain(floorTex(rnd(seed + 77), '#170f20', 'rgba(200,120,255,0.16)', 2), 2.5),
        gain(floorTex(rnd(seed + 88), '#121c18', 'rgba(120,255,180,0.10)', 3), 2.7)
      ],
      ceil: [gain(ceilTex(rnd(seed + 99), '#0d1116'), 1.7), gain(ceilTex(rnd(seed + 111), '#141019'), 1.7)],
      qdoor: qdoorTex(rnd(seed + 123)),
      exitOpen: exitTex(rnd(seed + 135), 5, true),
      exitClosed: [],
      decalRune: decalRune(rnd(seed + 147)),
      decalPortal: PORTAL_HUES.map((h, i) => decalPortal(rnd(seed + 159 + i), h)),
      frag: fragSprite(rnd(seed + 171)),
      angel: angelSprite(rnd(seed + 183)),
      portal: PORTAL_HUES.map((h, i) => portalSprite(rnd(seed + 195 + i), h)),
      rune: exitRune(rnd(seed + 207)),
      hues: PORTAL_HUES
    };
    for (let k = 0; k <= 5; k++) t.exitClosed.push(gain(exitTex(rnd(seed + 219 + k * 7), k, false), 1.7));
    return t;
  }

  root.NE.Textures = { build, S, PORTAL_HUES, author, toTex };
})(typeof window !== 'undefined' ? window : globalThis);
