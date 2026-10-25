/* =========================================================================
   SPACE MINER — Windows build driver
   Produces:
     dist/SPACE_MINER.exe            native launcher (hosts + opens the game)
     dist/SPACE_MINER_SETUP.exe      self-contained installer (payload appended)
     dist/SPACE_MINER_PORTABLE.zip   launcher + game, unzip and run
     dist/SPACE_MINER_SOURCE.zip     full source project
   Requires a zig toolchain: set ZIG=/path/to/zig or have `zig` on PATH.
   ========================================================================= */
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { execFileSync } = require('child_process');

const ROOT = path.join(__dirname, '..');
const WIN = path.join(ROOT, 'win');
const DIST = path.join(ROOT, 'dist');
const BUILD = path.join(ROOT, 'build');

function findZig() {
  const cands = [
    process.env.ZIG,
    path.join(ROOT, '.zig', 'zig'),
    '/tmp/zigx/ziglang/zig',
    'zig'
  ].filter(Boolean);
  for (const c of cands) {
    try {
      execFileSync(c, ['version'], { stdio: 'pipe' });
      return c;
    } catch (e) { /* next */ }
  }
  return null;
}

function log(...a) { console.log('  ', ...a); }

/* ---------------- crc32 ---------------- */
const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32(buf) {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

/* ---------------- minimal zip writer (deflate) ---------------- */
function zipBuild(entries) {
  // entries: [{name, data}] or [{name, dir:true}]
  const locals = [];
  const central = [];
  let offset = 0;
  for (const e of entries) {
    const nameBuf = Buffer.from(e.name.replace(/\\/g, '/'), 'utf8');
    const data = e.dir ? Buffer.alloc(0) : e.data;
    const isDir = !!e.dir || e.name.endsWith('/');
    let payload = data;
    let method = 0;
    if (!isDir && data.length > 64) {
      const deflated = zlib.deflateRawSync(data, { level: 9 });
      if (deflated.length < data.length) { payload = deflated; method = 8; }
    }
    const crc = isDir ? 0 : crc32(data);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(0x0800, 6);           // utf-8 names
    local.writeUInt16LE(method, 8);
    local.writeUInt16LE(0, 10);               // time
    local.writeUInt16LE(0x21, 12);            // date (1996-01-01)
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(payload.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(nameBuf.length, 26);
    local.writeUInt16LE(0, 28);
    locals.push(local, nameBuf, payload);

    const cen = Buffer.alloc(46);
    cen.writeUInt32LE(0x02014b50, 0);
    cen.writeUInt16LE(20, 4);
    cen.writeUInt16LE(20, 6);
    cen.writeUInt16LE(0x0800, 8);
    cen.writeUInt16LE(method, 10);
    cen.writeUInt16LE(0, 12);
    cen.writeUInt16LE(0x21, 14);
    cen.writeUInt32LE(crc, 16);
    cen.writeUInt32LE(payload.length, 20);
    cen.writeUInt32LE(data.length, 24);
    cen.writeUInt16LE(nameBuf.length, 28);
    cen.writeUInt16LE(0, 30);
    cen.writeUInt16LE(0, 32);
    cen.writeUInt16LE(0, 34);
    cen.writeUInt16LE(0, 36);
    cen.writeUInt32LE(isDir ? 0x10 : 0, 38);
    cen.writeUInt32LE(offset, 42);
    central.push(cen, nameBuf);

    offset += 30 + nameBuf.length + payload.length;
  }
  const cd = Buffer.concat(central);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(0, 4);
  end.writeUInt16LE(0, 6);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(cd.length, 12);
  end.writeUInt32LE(offset, 16);
  end.writeUInt16LE(0, 20);
  return Buffer.concat([Buffer.concat(locals), cd, end]);
}

/* ---------------- zip writer for STORED entries (installer payload) --- */
function zipBuildStored(entries) {
  const locals = [];
  const central = [];
  let offset = 0;
  for (const e of entries) {
    const nameBuf = Buffer.from(e.name.replace(/\\/g, '/'), 'utf8');
    const data = e.dir ? Buffer.alloc(0) : e.data;
    const isDir = !!e.dir || e.name.endsWith('/');
    const crc = isDir ? 0 : crc32(data);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(0x0800, 6);
    local.writeUInt16LE(0, 8);                // stored
    local.writeUInt16LE(0, 10);
    local.writeUInt16LE(0x21, 12);
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(data.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(nameBuf.length, 26);
    local.writeUInt16LE(0, 28);
    locals.push(local, nameBuf, data);

    const cen = Buffer.alloc(46);
    cen.writeUInt32LE(0x02014b50, 0);
    cen.writeUInt16LE(20, 4);
    cen.writeUInt16LE(20, 6);
    cen.writeUInt16LE(0x0800, 8);
    cen.writeUInt16LE(0, 10);
    cen.writeUInt16LE(0, 12);
    cen.writeUInt16LE(0x21, 14);
    cen.writeUInt32LE(crc, 16);
    cen.writeUInt32LE(data.length, 20);
    cen.writeUInt32LE(data.length, 24);
    cen.writeUInt16LE(nameBuf.length, 28);
    cen.writeUInt16LE(0, 30);
    cen.writeUInt16LE(0, 32);
    cen.writeUInt16LE(0, 34);
    cen.writeUInt16LE(0, 36);
    cen.writeUInt32LE(isDir ? 0x10 : 0, 38);
    cen.writeUInt32LE(offset, 42);
    central.push(cen, nameBuf);
    offset += 30 + nameBuf.length + data.length;
  }
  const cd = Buffer.concat(central);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(cd.length, 12);
  end.writeUInt32LE(offset, 16);
  return Buffer.concat([Buffer.concat(locals), cd, end]);
}

/* ---------------- icon header export ---------------- */
function writeIconHeader(dib32Path, dib16Path) {
  const d32 = fs.readFileSync(dib32Path);
  const d16 = fs.readFileSync(dib16Path);
  const blob = Buffer.concat([d32, d16]);
  const offs = [0, d32.length];
  const sizes = [d32.length, d16.length];
  let out = '/* generated by tools/win.js — do not edit */\n';
  out += '#ifndef SPACE_MINER_ICON_H\n#define SPACE_MINER_ICON_H\n';
  out += 'static const unsigned char kIconData[] = {\n';
  for (let i = 0; i < blob.length; i++) {
    out += blob[i] + (i === blob.length - 1 ? '\n' : ',');
    if ((i + 1) % 24 === 0) out += '\n';
  }
  out += '};\n';
  out += 'static const int kIconOffset[] = { ' + offs.join(', ') + ' };\n';
  out += 'static const int kIconSize[] = { ' + sizes.join(', ') + ' };\n';
  out += 'static const int kIconCount = 2;\n';
  out += '#endif\n';
  fs.writeFileSync(path.join(WIN, '.gen_icon.h'), out);
  log('generated .gen_icon.h', (out.length / 1024).toFixed(1) + 'KB');
}

/* ---------------- collect game files ---------------- */
function walk(dir, base = '', out = []) {
  for (const name of fs.readdirSync(dir)) {
    const full = path.join(dir, name);
    const rel = base ? base + '/' + name : name;
    const st = fs.statSync(full);
    if (st.isDirectory()) walk(full, rel, out);
    else out.push({ name: rel, data: fs.readFileSync(full), size: st.size });
  }
  return out;
}

function main() {
  fs.mkdirSync(DIST, { recursive: true });
  fs.mkdirSync(BUILD, { recursive: true });

  console.log('\nSPACE MINER — windows build\n');

  // 1. game payload
  log('building single-file game…');
  execFileSync(process.execPath, [path.join(ROOT, 'tools', 'build.js')], { stdio: 'inherit' });
  log('generating icons…');
  execFileSync(process.execPath, [path.join(ROOT, 'tools', 'ico.js')], { stdio: 'inherit' });

  const stage = path.join(BUILD, 'windows');
  fs.rmSync(stage, { recursive: true, force: true });
  fs.mkdirSync(path.join(stage, 'game'), { recursive: true });
  fs.copyFileSync(path.join(ROOT, 'index.html'), path.join(stage, 'game', 'index.html'));
  fs.copyFileSync(path.join(WIN, 'icon.ico'), path.join(stage, 'game', 'icon.ico'));
  fs.writeFileSync(path.join(stage, 'game', 'BUILD.txt'),
    'SPACE MINER ' + new Date().toISOString() + '\nSingle-file build. Open index.html in any modern browser.\n');

  const iconHTML = fs.readFileSync(path.join(stage, 'game', 'index.html'), 'utf8');
  log('game payload', (Buffer.byteLength(iconHTML) / 1024).toFixed(1) + 'KB');

  writeIconHeader(path.join(WIN, 'icon32.dib'), path.join(WIN, 'icon16.dib'));

  const zig = findZig();
  if (!zig) {
    console.error('\n  !! no zig toolchain found — cannot produce native executables.');
    console.error('     set ZIG=/path/to/zig (0.13+ recommended) and re-run: node tools/win.js');
    writePortableOnly(stage);
    process.exit(3);
  }
  log('zig:', execFileSync(zig, ['version'], { encoding: 'utf8' }).trim());

  const common = ['-target', 'x86_64-windows-gnu', '-O2', '-Wl,--subsystem,windows',
    '-Wall', '-Wno-unused-variable', '-Wno-unused-but-set-variable'];
  const libs = ['-luser32', '-lgdi32', '-lshell32', '-lshlwapi', '-lole32', '-luuid',
    '-ladvapi32', '-lcomctl32', '-lws2_32'];

  // 2. launcher
  log('compiling SPACE_MINER.exe…');
  const launcherExe = path.join(stage, 'SPACE_MINER.exe');
  run(zig, ['cc', ...common, '-o', launcherExe, path.join(WIN, 'launcher.c'), ...libs]);
  dropPdb(stage);
  injectIcon(launcherExe, {
    version: '1.0.0.1', company: 'Arena.ai', product: 'SPACE MINER',
    description: 'SPACE MINER — Deep Belt Operations', internal: 'SPACE_MINER',
    filename: 'SPACE_MINER.exe', copyright: 'Copyright © 2026 Arena.ai',
    comments: 'Native launcher and local session host', variable: true
  });
  log('launcher', (fs.statSync(launcherExe).size / 1024).toFixed(0) + 'KB');

  // 3. installer with appended payload
  log('compiling SPACE_MINER_SETUP.exe…');
  const installerRaw = path.join(BUILD, 'setup_raw.exe');
  run(zig, ['cc', ...common, '-o', installerRaw, path.join(WIN, 'installer.c'), ...libs]);

  const payloadEntries = [];
  const files = walk(stage);
  files.sort((a, b) => a.name.localeCompare(b.name));
  for (const f of files) payloadEntries.push({ name: f.name, data: f.data });
  const payload = zipBuildStored(payloadEntries);
  log('installer payload', (payload.length / 1024).toFixed(0) + 'KB,', files.length, 'files');

  dropPdb(BUILD);
  injectIcon(installerRaw, {
    version: '1.0.0.1', company: 'Arena.ai', product: 'SPACE MINER',
    description: 'SPACE MINER Setup', internal: 'SPACE_MINER_SETUP',
    filename: 'SPACE_MINER_SETUP.exe', copyright: 'Copyright © 2026 Arena.ai',
    comments: 'Installs SPACE MINER for the current user', variable: true
  });
  const installer = fs.readFileSync(installerRaw);
  const footer = Buffer.alloc(16);
  footer.write('SPMNRSET', 0, 'ascii');
  footer.writeUInt32LE(installer.length, 8);
  footer.writeUInt32LE(payload.length, 12);
  const setupExe = path.join(DIST, 'SPACE_MINER_SETUP.exe');
  fs.writeFileSync(setupExe, Buffer.concat([installer, payload, footer]));
  log('setup', (fs.statSync(setupExe).size / 1024).toFixed(0) + 'KB');

  // 4. portable + source archives
  fs.copyFileSync(launcherExe, path.join(DIST, 'SPACE_MINER.exe'));
  const portableEntries = [{ name: 'SPACE_MINER/', dir: true }].concat(
    files.map((f) => ({ name: 'SPACE_MINER/' + f.name, data: f.data }))
  );
  portableEntries.push({
    name: 'SPACE_MINER/READ_ME_FIRST.txt',
    data: Buffer.from(
      'SPACE MINER — portable build\r\n' +
      '============================\r\n\r\n' +
      '1. Unzip this whole folder anywhere you like.\r\n' +
      '2. Run SPACE_MINER.exe\r\n\r\n' +
      'SPACE_MINER.exe starts a private local web server on 127.0.0.1 and opens\r\n' +
      'the game in a chromeless Microsoft Edge / Google Chrome app window.\r\n' +
      'No internet connection is used; nothing leaves your machine.\r\n\r\n' +
      'If no Chromium-based browser is present, the game opens in your default\r\n' +
      'browser instead, or you can open game\\index.html directly.\r\n\r\n' +
      'Your pilot profile is saved automatically by the game.\r\n')
  });
  const portableZip = path.join(DIST, 'SPACE_MINER_PORTABLE.zip');
  fs.writeFileSync(portableZip, zipBuild(portableEntries));
  log('portable zip', (fs.statSync(portableZip).size / 1024).toFixed(0) + 'KB');

  // 5. source archive
  const srcEntries = [];
  const srcFiles = [];
  walkDir(path.join(ROOT, 'src'), 'src', srcFiles);
  walkDir(path.join(ROOT, 'tools'), 'tools', srcFiles);
  walkDir(path.join(ROOT, 'win'), 'win', srcFiles);
  for (const f of srcFiles) {
    if (f.name.includes('node_modules') || f.name.endsWith('.dib') || f.name.endsWith('.ico') ||
        f.name.endsWith('.png') || f.name.endsWith('.gen_icon.h') || f.name.endsWith('.pdb') ||
        f.name.endsWith('.bundle.js') || f.name.endsWith('.exe')) continue;
    srcEntries.push({ name: 'SPACE_MINER_SOURCE/' + f.name, data: f.data });
  }
  for (const extra of ['package.json', 'index.html', 'README.md', 'LICENSE',
                       'docs/TEST_RESULTS.md', 'docs/KNOWN_LIMITATIONS.md',
                       'docs/WINDOWS_DEPLOYMENT.md']) {
    const p = path.join(ROOT, extra);
    if (fs.existsSync(p)) srcEntries.push({ name: 'SPACE_MINER_SOURCE/' + extra, data: fs.readFileSync(p) });
  }
  srcEntries.push({ name: 'SPACE_MINER_SOURCE/', dir: true });
  const sourceZip = path.join(DIST, 'SPACE_MINER_SOURCE.zip');
  fs.writeFileSync(sourceZip, zipBuild(srcEntries));
  log('source zip', (fs.statSync(sourceZip).size / 1024).toFixed(0) + 'KB,', srcEntries.length, 'files');

  console.log('\n  done. artifacts in dist/:');
  for (const f of fs.readdirSync(DIST)) {
    const st = fs.statSync(path.join(DIST, f));
    console.log('   ', f.padEnd(28), (st.size / 1024).toFixed(0) + 'KB');
  }
  console.log('');
}

function walkDir(dir, prefix, out) {
  if (!fs.existsSync(dir)) return;
  for (const name of fs.readdirSync(dir)) {
    if (name === 'node_modules' || name === '.git') continue;
    const full = path.join(dir, name);
    const rel = prefix ? prefix + '/' + name : name;
    const st = fs.statSync(full);
    if (st.isDirectory()) walkDir(full, rel, out);
    else out.push({ name: rel, data: fs.readFileSync(full), size: st.size });
  }
}

function injectIcon(exePath, meta) {
  const pe = require('./pe.js');
  const before = fs.readFileSync(exePath);
  const after = pe.injectResources(before, path.join(WIN, 'icon_res.ico'), meta);
  fs.writeFileSync(exePath, after);
  const dump = pe.dumpResources(after);
  const groups = dump.types.find((t) => t.id === 14);
  const icons = dump.types.find((t) => t.id === 3);
  const version = dump.types.find((t) => t.id === 16);
  if (!groups || !icons || !version || groups.children.length !== 1) {
    throw new Error('icon resource injection failed for ' + exePath);
  }
  log('icon resource:', icons.children.length, 'images,',
    (dump.resourceSize / 1024).toFixed(0) + 'KB');
}

function dropPdb(dir) {
  for (const name of fs.readdirSync(dir)) {
    if (name.toLowerCase().endsWith('.pdb')) fs.rmSync(path.join(dir, name), { force: true });
  }
}

function run(cmd, args) {
  try {
    execFileSync(cmd, args, { stdio: 'pipe' });
  } catch (e) {
    const out = (e.stdout || '') + '' + (e.stderr || '');
    console.error('\n  compile failed:', cmd, args.join(' '));
    console.error(out.split('\n').slice(0, 40).join('\n'));
    process.exit(4);
  }
}

function writePortableOnly(stage) {
  const files = walk(stage);
  const entries = files.map((f) => ({ name: 'SPACE_MINER/' + f.name, data: f.data }));
  const zipPath = path.join(DIST, 'SPACE_MINER_PORTABLE.zip');
  fs.writeFileSync(zipPath, zipBuild(entries));
  console.log('  wrote portable archive without launcher:', zipPath);
}

main();
