/* Generates the SPACE MINER application icon (.ico, multi-size) procedurally. */
const fs = require('fs');
const path = require('path');
const { Canvas } = require('skia-canvas');

const SIZES = [16, 24, 32, 48, 64, 128, 256];
const OUT = path.join(__dirname, '..', 'win', 'icon.ico');

function drawIcon(size) {
  const c = new Canvas(size, size);
  const x = c.getContext('2d');
  const S = size;
  const k = S / 256;

  x.clearRect(0, 0, S, S);

  // badge background — rounded square with radial depth
  const r = 56 * k;
  x.beginPath();
  x.moveTo(r, 0);
  x.lineTo(S - r, 0);
  x.quadraticCurveTo(S, 0, S, r);
  x.lineTo(S, S - r);
  x.quadraticCurveTo(S, S, S - r, S);
  x.lineTo(r, S);
  x.quadraticCurveTo(0, S, 0, S - r);
  x.lineTo(0, r);
  x.quadraticCurveTo(0, 0, r, 0);
  x.closePath();
  const bg = x.createLinearGradient(0, 0, S, S);
  bg.addColorStop(0, '#0d2233');
  bg.addColorStop(0.55, '#071522');
  bg.addColorStop(1, '#03080f');
  x.fillStyle = bg;
  x.fill();

  // star dust
  const rng = (() => { let s = 12345 + S; return () => { s = (s * 1103515245 + 12345) & 0x7fffffff; return s / 0x7fffffff; }; })();
  x.save();
  x.beginPath();
  x.rect(0, 0, S, S);
  x.clip();
  for (let i = 0; i < 26; i++) {
    x.fillStyle = 'rgba(190,230,255,' + (0.12 + rng() * 0.5) + ')';
    const px = rng() * S, py = rng() * S, pr = (0.5 + rng() * 1.3) * k;
    x.beginPath(); x.arc(px, py, pr, 0, 6.2832); x.fill();
  }
  x.restore();

  // orbit ring
  x.save();
  x.translate(S * 0.5, S * 0.52);
  x.rotate(-0.42);
  x.strokeStyle = 'rgba(95,230,216,0.5)';
  x.lineWidth = Math.max(1, 4 * k);
  x.beginPath();
  x.ellipse(0, 0, S * 0.40, S * 0.17, 0, 0, 6.2832);
  x.stroke();
  x.restore();

  // asteroid (upper right)
  x.save();
  x.translate(S * 0.66, S * 0.36);
  x.rotate(0.3);
  const ar = S * 0.19;
  const pts = [];
  const n = 11;
  for (let i = 0; i < n; i++) {
    const a = (i / n) * 6.2832;
    const rr = ar * (0.86 + Math.sin(i * 2.3) * 0.1 + Math.sin(i * 4.1) * 0.06);
    pts.push([Math.cos(a) * rr, Math.sin(a) * rr]);
  }
  x.beginPath();
  x.moveTo(pts[0][0], pts[0][1]);
  for (let i = 1; i < pts.length; i++) x.lineTo(pts[i][0], pts[i][1]);
  x.closePath();
  const ag = x.createRadialGradient(-ar * 0.45, -ar * 0.45, ar * 0.1, 0, 0, ar * 1.15);
  ag.addColorStop(0, '#c9d3dd');
  ag.addColorStop(0.45, '#78838f');
  ag.addColorStop(0.8, '#3a434c');
  ag.addColorStop(1, '#1a2027');
  x.fillStyle = ag;
  x.fill();
  x.strokeStyle = 'rgba(255,255,255,0.45)';
  x.lineWidth = Math.max(1, 2.2 * k);
  x.stroke();
  // mineral vein
  x.strokeStyle = 'rgba(159,240,220,' + (S > 32 ? 0.85 : 0.6) + ')';
  x.lineWidth = Math.max(1, 3 * k);
  x.beginPath();
  x.moveTo(-ar * 0.5, -ar * 0.2);
  x.lineTo(-ar * 0.1, ar * 0.1);
  x.lineTo(ar * 0.35, -ar * 0.05);
  x.stroke();
  x.restore();

  // mining ship (lower left, simplified cutter silhouette)
  x.save();
  x.translate(S * 0.42, S * 0.66);
  x.rotate(-0.42);
  const sh = S * 0.30;
  x.beginPath();
  x.moveTo(sh * 0.62, 0);
  x.lineTo(sh * 0.30, -sh * 0.17);
  x.lineTo(-sh * 0.12, -sh * 0.21);
  x.lineTo(-sh * 0.46, -sh * 0.16);
  x.lineTo(-sh * 0.58, 0);
  x.lineTo(-sh * 0.46, sh * 0.16);
  x.lineTo(-sh * 0.12, sh * 0.21);
  x.lineTo(sh * 0.30, sh * 0.17);
  x.closePath();
  const sg = x.createLinearGradient(0, -sh * 0.2, 0, sh * 0.2);
  sg.addColorStop(0, '#cfe2ef');
  sg.addColorStop(0.5, '#7d95a6');
  sg.addColorStop(1, '#2b3742');
  x.fillStyle = sg;
  x.fill();
  x.strokeStyle = 'rgba(3,8,12,0.85)';
  x.lineWidth = Math.max(0.7, 1.6 * k);
  x.stroke();
  // cockpit
  x.fillStyle = '#63d6e8';
  x.beginPath();
  x.ellipse(sh * 0.2, 0, sh * 0.12, sh * 0.07, 0, 0, 6.2832);
  x.fill();
  // engine plume
  x.save();
  x.globalCompositeOperation = 'lighter';
  const pg = x.createLinearGradient(-sh * 0.5, 0, -sh * 1.15, 0);
  pg.addColorStop(0, 'rgba(220,255,255,0.95)');
  pg.addColorStop(0.4, 'rgba(95,230,216,0.7)');
  pg.addColorStop(1, 'rgba(60,150,255,0)');
  x.fillStyle = pg;
  x.beginPath();
  x.moveTo(-sh * 0.5, -sh * 0.1);
  x.lineTo(-sh * 1.15, 0);
  x.lineTo(-sh * 0.5, sh * 0.1);
  x.closePath();
  x.fill();
  x.restore();
  x.restore();

  // mining beam from ship to rock
  x.save();
  x.globalCompositeOperation = 'lighter';
  x.strokeStyle = 'rgba(140,255,240,0.85)';
  x.lineWidth = Math.max(1, 3.2 * k);
  x.beginPath();
  x.moveTo(S * 0.50, S * 0.60);
  x.lineTo(S * 0.58, S * 0.44);
  x.stroke();
  const ig = x.createRadialGradient(S * 0.585, S * 0.435, 0, S * 0.585, S * 0.435, S * 0.09);
  ig.addColorStop(0, 'rgba(255,255,255,0.95)');
  ig.addColorStop(1, 'rgba(95,230,216,0)');
  x.fillStyle = ig;
  x.beginPath(); x.arc(S * 0.585, S * 0.435, S * 0.09, 0, 6.2832); x.fill();
  x.restore();

  // outer frame
  x.beginPath();
  x.moveTo(r, 0.5);
  x.lineTo(S - r, 0.5);
  x.quadraticCurveTo(S - 0.5, 0.5, S - 0.5, r);
  x.lineTo(S - 0.5, S - r);
  x.quadraticCurveTo(S - 0.5, S - 0.5, S - r, S - 0.5);
  x.lineTo(r, S - 0.5);
  x.quadraticCurveTo(0.5, S - 0.5, 0.5, S - r);
  x.lineTo(0.5, r);
  x.quadraticCurveTo(0.5, 0.5, r, 0.5);
  x.closePath();
  x.strokeStyle = 'rgba(95,230,216,0.75)';
  x.lineWidth = Math.max(1, 5 * k);
  x.stroke();

  return c;
}

/* icon resource blob for CreateIconFromResourceEx (BITMAPINFOHEADER + BGRA + mask) */
function toIconResource(size) {
  const canvas = drawIcon(size);
  const ctx = canvas.getContext('2d');
  const img = ctx.getImageData(0, 0, size, size).data;
  const maskStride = Math.ceil(size / 32) * 4;
  const header = Buffer.alloc(40);
  header.writeUInt32LE(40, 0);
  header.writeInt32LE(size, 4);
  header.writeInt32LE(size * 2, 8);
  header.writeUInt16LE(1, 12);
  header.writeUInt16LE(32, 14);
  header.writeUInt32LE(0, 16);
  header.writeUInt32LE(size * size * 4, 20);
  const xor = Buffer.alloc(size * size * 4);
  for (let y = 0; y < size; y++) {
    const srcRow = (size - 1 - y) * size * 4;
    for (let x = 0; x < size; x++) {
      const s = srcRow + x * 4;
      const d = (y * size + x) * 4;
      xor[d] = img[s + 2];       // B
      xor[d + 1] = img[s + 1];   // G
      xor[d + 2] = img[s];       // R
      xor[d + 3] = img[s + 3];   // A
    }
  }
  const mask = Buffer.alloc(maskStride * size);
  return Buffer.concat([header, xor, mask]);
}

function main() {
  const images = SIZES.map((s) => ({ size: s, canvas: drawIcon(s) }));
  // ICO container with PNG payloads (Windows Vista+)
  const chunks = [];
  const count = images.length;
  const header = Buffer.alloc(6);
  header.writeUInt16LE(0, 0);
  header.writeUInt16LE(1, 2);
  header.writeUInt16LE(count, 4);
  chunks.push(header);

  const pngs = images.map((im) => im.canvas.toBufferSync('png'));
  let offset = 6 + 16 * count;
  const dir = Buffer.alloc(16 * count);
  images.forEach((im, i) => {
    const b = i * 16;
    dir.writeUInt8(im.size >= 256 ? 0 : im.size, b);
    dir.writeUInt8(im.size >= 256 ? 0 : im.size, b + 1);
    dir.writeUInt8(0, b + 2);
    dir.writeUInt8(0, b + 3);
    dir.writeUInt16LE(1, b + 4);
    dir.writeUInt16LE(32, b + 6);
    dir.writeUInt32LE(pngs[i].length, b + 8);
    dir.writeUInt32LE(offset, b + 12);
    offset += pngs[i].length;
  });
  chunks.push(dir, ...pngs);

  fs.mkdirSync(path.dirname(OUT), { recursive: true });
  fs.writeFileSync(OUT, Buffer.concat(chunks));
  console.log('icon.ico written:', (Buffer.concat(chunks).length / 1024).toFixed(1), 'KB,', count, 'sizes');

  // also export a PNG for docs / shortcuts
  const big = images[images.length - 1];
  fs.writeFileSync(path.join(__dirname, '..', 'win', 'icon.png'), big.canvas.toBufferSync('png'));

  // DIB blobs for the native shell (CreateIconFromResourceEx needs DIB, not PNG)
  fs.writeFileSync(path.join(__dirname, '..', 'win', 'icon32.dib'), toIconResource(32));
  fs.writeFileSync(path.join(__dirname, '..', 'win', 'icon16.dib'), toIconResource(16));

  /* resource icon: classic DIB payloads for the small shell sizes (guaranteed
     support in Explorer/taskbar) and PNG for the large ones. */
  const resImages = images.map((im) => ({
    size: im.size,
    payload: im.size <= 64 ? toIconResource(im.size) : im.canvas.toBufferSync('png')
  }));
  const rh = Buffer.alloc(6);
  rh.writeUInt16LE(0, 0);
  rh.writeUInt16LE(1, 2);
  rh.writeUInt16LE(resImages.length, 4);
  const rd = Buffer.alloc(16 * resImages.length);
  let rOff = 6 + 16 * resImages.length;
  resImages.forEach((im, i) => {
    const b = i * 16;
    rd.writeUInt8(im.size >= 256 ? 0 : im.size, b);
    rd.writeUInt8(im.size >= 256 ? 0 : im.size, b + 1);
    rd.writeUInt8(0, b + 2);
    rd.writeUInt8(0, b + 3);
    rd.writeUInt16LE(1, b + 4);
    rd.writeUInt16LE(32, b + 6);
    rd.writeUInt32LE(im.payload.length, b + 8);
    rd.writeUInt32LE(rOff, b + 12);
    rOff += im.payload.length;
  });
  const resIco = Buffer.concat([rh, rd, ...resImages.map((i) => i.payload)]);
  fs.writeFileSync(path.join(__dirname, '..', 'win', 'icon_res.ico'), resIco);
  console.log('icon_res.ico written:', (resIco.length / 1024).toFixed(1), 'KB (DIB + PNG mix)');
}

main();
