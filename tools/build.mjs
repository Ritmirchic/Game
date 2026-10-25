// Builds single-file and zip distributions of MEMORIA.
// usage: node tools/build.mjs
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const root = path.resolve(import.meta.dirname, '..');
const dist = path.join(root, 'dist');
fs.mkdirSync(dist, { recursive: true });

let html = fs.readFileSync(path.join(root, 'index.html'), 'utf8');

// inline every local <script src="...">
html = html.replace(/<script src="([^"]+)"><\/script>/g, (m, src) => {
  const code = fs.readFileSync(path.join(root, src), 'utf8');
  return '<script>\n/* ' + src + ' */\n' + code + '\n</script>';
});

const banner = `<!--
  MEMORIA — город, которого не было.
  Single-file build. Just open this file in any modern browser (Chrome, Edge, Firefox, Safari).
  No server, no internet, no dependencies. Everything below is hand-written JavaScript + WebGL.

  Controls: WASD move · mouse look · SHIFT run · SPACE fly up · CTRL down · E / click = memory pulse
            H compass · R another city · M mute
-->`;

const single = path.join(dist, 'MEMORIA.html');
fs.writeFileSync(single, banner + '\n' + html);
console.log('wrote', path.relative(root, single), (fs.statSync(single).size / 1024).toFixed(1) + ' KB');

/* ---------- minimal zip writer (store + deflate) ---------- */
const crcTable = (() => {
  const t = new Int32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c; }
  return t;
})();
function crc32(buf) {
  let c = -1;
  for (let i = 0; i < buf.length; i++) c = crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ -1) >>> 0;
}
function zip(files) {
  const chunks = [], central = [];
  let offset = 0;
  const now = new Date();
  const dosTime = ((now.getHours() << 11) | (now.getMinutes() << 5) | (now.getSeconds() >> 1)) & 0xffff;
  const dosDate = (((now.getFullYear() - 1980) << 9) | ((now.getMonth() + 1) << 5) | now.getDate()) & 0xffff;
  for (const f of files) {
    const data = fs.readFileSync(f.abs);
    const comp = zlib.deflateRawSync(data, { level: 9 });
    const useDeflate = comp.length < data.length;
    const body = useDeflate ? comp : data;
    const name = Buffer.from(f.name, 'utf8');
    const crc = crc32(data);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(0x0800, 6); // UTF-8 names
    local.writeUInt16LE(useDeflate ? 8 : 0, 8);
    local.writeUInt16LE(dosTime, 10);
    local.writeUInt16LE(dosDate, 12);
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(body.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(name.length, 26);
    local.writeUInt16LE(0, 28);
    chunks.push(local, name, body);
    const cd = Buffer.alloc(46);
    cd.writeUInt32LE(0x02014b50, 0);
    cd.writeUInt16LE(20, 4);
    cd.writeUInt16LE(20, 6);
    cd.writeUInt16LE(0x0800, 8);
    cd.writeUInt16LE(useDeflate ? 8 : 0, 10);
    cd.writeUInt16LE(dosTime, 12);
    cd.writeUInt16LE(dosDate, 14);
    cd.writeUInt32LE(crc, 16);
    cd.writeUInt32LE(body.length, 20);
    cd.writeUInt32LE(data.length, 24);
    cd.writeUInt16LE(name.length, 28);
    cd.writeUInt32LE(offset, 42);
    central.push(Buffer.concat([cd, name]));
    offset += local.length + name.length + body.length;
  }
  const centralBuf = Buffer.concat(central);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(files.length, 8);
  end.writeUInt16LE(files.length, 10);
  end.writeUInt32LE(centralBuf.length, 12);
  end.writeUInt32LE(offset, 16);
  return Buffer.concat([...chunks, centralBuf, end]);
}

const zipPath = path.join(dist, 'MEMORIA.zip');
const files = [
  { name: 'MEMORIA/index.html', abs: path.join(root, 'index.html') },
  { name: 'MEMORIA/src/world.js', abs: path.join(root, 'src/world.js') },
  { name: 'MEMORIA/src/gl.js', abs: path.join(root, 'src/gl.js') },
  { name: 'MEMORIA/src/audio.js', abs: path.join(root, 'src/audio.js') },
  { name: 'MEMORIA/src/game.js', abs: path.join(root, 'src/game.js') },
  { name: 'MEMORIA/README.md', abs: path.join(root, 'README.md') },
  { name: 'MEMORIA/MEMORIA.html', abs: single }
];
fs.writeFileSync(zipPath, zip(files));
console.log('wrote', path.relative(root, zipPath), (fs.statSync(zipPath).size / 1024).toFixed(1) + ' KB');
