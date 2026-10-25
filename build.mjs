import * as esbuild from 'esbuild';
import fs from 'node:fs';
import zlib from 'node:zlib';

/** Минимальный ZIP-писатель: GitHub отдаёт .html как текст, а .zip — как файл. */
function writeZip(outPath, name, data) {
  const crcTable = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crcTable[i] = c >>> 0;
  }
  let crc = 0xffffffff;
  for (const b of data) crc = crcTable[(crc ^ b) & 0xff] ^ (crc >>> 8);
  crc = (crc ^ 0xffffffff) >>> 0;

  const deflated = zlib.deflateRawSync(data, { level: 9 });
  const nameBuf = Buffer.from(name, 'utf8');

  const local = Buffer.alloc(30);
  local.writeUInt32LE(0x04034b50, 0);
  local.writeUInt16LE(20, 4);          // version needed
  local.writeUInt16LE(0x0800, 6);      // UTF-8
  local.writeUInt16LE(8, 8);           // deflate
  local.writeUInt16LE(0, 10);          // time
  local.writeUInt16LE(0x21, 12);       // date (1996-01-01)
  local.writeUInt32LE(crc, 14);
  local.writeUInt32LE(deflated.length, 18);
  local.writeUInt32LE(data.length, 22);
  local.writeUInt16LE(nameBuf.length, 26);
  local.writeUInt16LE(0, 28);

  const central = Buffer.alloc(46);
  central.writeUInt32LE(0x02014b50, 0);
  central.writeUInt16LE(20, 4);
  central.writeUInt16LE(20, 6);
  central.writeUInt16LE(0x0800, 8);
  central.writeUInt16LE(8, 10);
  central.writeUInt16LE(0, 12);
  central.writeUInt16LE(0x21, 14);
  central.writeUInt32LE(crc, 16);
  central.writeUInt32LE(deflated.length, 20);
  central.writeUInt32LE(data.length, 24);
  central.writeUInt16LE(nameBuf.length, 28);
  central.writeUInt32LE(0, 38);        // external attrs
  central.writeUInt32LE(0, 42);        // local header offset

  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(1, 8);
  end.writeUInt16LE(1, 10);
  end.writeUInt32LE(central.length + nameBuf.length, 12);
  end.writeUInt32LE(local.length + nameBuf.length + deflated.length, 16);

  fs.writeFileSync(outPath, Buffer.concat([local, nameBuf, deflated, central, nameBuf, end]));
  return deflated.length;
}

const watch = process.argv.includes('--watch');

/** Общая сборка: один IIFE-файл — работает даже при открытии index.html двойным кликом (file://). */
const opts = {
  entryPoints: ['src/main.js'],
  bundle: true,
  format: 'iife',
  target: ['es2020'],
  outfile: 'dist/game.js',
  minify: !watch,
  sourcemap: false,
  legalComments: 'none',
  logLevel: 'info',
};

/** Один автономный HTML: удобно скачать и запустить без сервера. */
async function buildSingle() {
  const res = await esbuild.build({
    ...opts,
    outfile: undefined,
    write: false,
    minify: true,
  });
  const js = res.outputFiles[0].text;
  const htmlRaw = fs.readFileSync('index.html', 'utf8');
  const finalHtml = htmlRaw.replace(
    /<script defer src="\.\/dist\/game\.js"><\/script>/,
    () => `<script>${js}</script>`
  );
  fs.writeFileSync('mobius.html', finalHtml);
  writeZip('mobius.zip', 'mobius.html', Buffer.from(finalHtml, 'utf8'));
  const kb = (Buffer.byteLength(finalHtml) / 1024).toFixed(0);
  const zipKb = (fs.statSync('mobius.zip').size / 1024).toFixed(0);
  console.log(`mobius.html — ${kb} ` + 'КБ (один файл, без интернета)');
  console.log(`mobius.zip  — ${zipKb} ` + 'КБ (архив для скачивания)');
}

if (watch) {
  const ctx = await esbuild.context(opts);
  await ctx.watch();
  console.log('watching…');
} else {
  await esbuild.build(opts);
  await buildSingle();
}
