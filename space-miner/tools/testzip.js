/* =========================================================================
   Verifies the installer's extraction engine (win/zip.h) and the payload
   footer protocol, on the host, by compiling the very same sources with a
   POSIX harness and feeding them a real release payload.
   ========================================================================= */
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { execFileSync } = require('child_process');

const ROOT = path.join(__dirname, '..');
const BUILD = path.join(ROOT, 'build');
const WIN = path.join(ROOT, 'win');

function findCC() {
  for (const cc of [process.env.ZIG && path.join(process.env.ZIG, ''), '/tmp/zigx/ziglang/zig', 'cc', 'gcc', 'clang']) {
    if (!cc) continue;
    try { execFileSync(cc, ['--version'], { stdio: 'pipe' }); return cc; } catch (e) {}
  }
  return null;
}

function crc32(buf) {
  let c = ~0;
  for (let i = 0; i < buf.length; i++) {
    c ^= buf[i];
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return (~c) >>> 0;
}

/* ---- build a real payload: stored zip, mirroring tools/win.js ---- */
function buildPayload(files) {
  const locals = [], central = [];
  let off = 0;
  for (const f of files) {
    const name = Buffer.from(f.name, 'utf8');
    const crc = crc32(f.data);
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0); lh.writeUInt16LE(20, 4); lh.writeUInt16LE(0x800, 6);
    lh.writeUInt16LE(0, 8); lh.writeUInt16LE(0, 10); lh.writeUInt16LE(0x21, 12);
    lh.writeUInt32LE(crc, 14); lh.writeUInt32LE(f.data.length, 18); lh.writeUInt32LE(f.data.length, 22);
    lh.writeUInt16LE(name.length, 26);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0); ch.writeUInt16LE(20, 4); ch.writeUInt16LE(20, 6);
    ch.writeUInt16LE(0x800, 8); ch.writeUInt16LE(0, 10); ch.writeUInt16LE(0, 12);
    ch.writeUInt16LE(0x21, 14); ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(f.data.length, 20); ch.writeUInt32LE(f.data.length, 24);
    ch.writeUInt16LE(name.length, 28); ch.writeUInt32LE(off, 42);
    locals.push(lh, name, f.data); central.push(ch, name);
    off += 30 + name.length + f.data.length;
  }
  const cd = Buffer.concat(central);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(files.length, 8); end.writeUInt16LE(files.length, 10);
  end.writeUInt32LE(cd.length, 12); end.writeUInt32LE(off, 16);
  return Buffer.concat([...locals, cd, end]);
}

const HARNESS = String.raw`
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#define ZIP_SEP '/'
#include "zip.h"

static void make_dirs(const char *p) {
    char tmp[1024]; size_t i, len;
    snprintf(tmp, sizeof(tmp), "%s", p);
    len = strlen(tmp);
    for (i = 1; i < len; i++) {
        if (tmp[i] == '/') { char c = tmp[i]; tmp[i] = 0; mkdir(tmp, 0755); tmp[i] = c; }
    }
    mkdir(tmp, 0755);
}
void zip_mkdir(const char *p) { make_dirs(p); }
int zip_write(const char *p, const unsigned char *d, unsigned int n) {
    FILE *f = fopen(p, "wb");
    if (!f) return 0;
    if (n && fwrite(d, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}
static void prog(int pct, void *ctx) { (void)ctx; if (pct % 25 == 0) fprintf(stderr, "  progress %d%%\n", pct); }

int main(int argc, char **argv) {
    FILE *f;
    long size, off, plen;
    unsigned int uoff, uplen;
    unsigned char *buf, *payload, foot[16];
    int n;
    if (argc < 3) { fprintf(stderr, "usage: %s payload.zip outdir\n", argv[0]); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc(size);
    if (fread(buf, 1, size, f) != (size_t)size) return 1;
    fclose(f);
    /* emulate the appended-footer protocol: footer is last 16 bytes */
    memcpy(foot, buf + size - 16, 16);
    if (memcmp(foot, "SPMNRSET", 8) != 0) { fprintf(stderr, "bad magic\n"); return 1; }
    memcpy(&uoff, foot + 8, 4);
    memcpy(&uplen, foot + 12, 4);
    (void)off; (void)plen;
    payload = buf + uoff;
    n = zip_extract(payload, uplen, argv[2], prog, NULL);
    printf("extracted %d files\n", n);
    return n > 0 ? 0 : 1;
}
`;

function main() {
  fs.mkdirSync(BUILD, { recursive: true });
  const cc = findCC();
  if (!cc) { console.error('no C compiler'); process.exit(2); }

  // realistic payload: the real release file + a small deflate zip for the inflater
  const realFiles = [
    { name: 'SPACE_MINER.exe', data: fs.readFileSync(path.join(BUILD, 'windows', 'SPACE_MINER.exe')) },
    { name: 'game/index.html', data: fs.readFileSync(path.join(ROOT, 'index.html')) },
    { name: 'game/icon.ico', data: fs.readFileSync(path.join(WIN, 'icon.ico')) },
    { name: 'game/BUILD.txt', data: Buffer.from('build info\n') }
  ];
  const stored = buildPayload(realFiles);
  const footer = Buffer.alloc(16);
  footer.write('SPMNRSET', 0, 'ascii');
  footer.writeUInt32LE(1234, 8);            // fake exe prefix length
  footer.writeUInt32LE(stored.length, 12);
  const prefix = Buffer.alloc(1234, 0x90);
  const exePath = path.join(BUILD, 'setup_fake.exe');
  fs.writeFileSync(exePath, Buffer.concat([prefix, stored, footer]));

  const harnessPath = path.join(BUILD, 'ziphost.c');
  fs.writeFileSync(harnessPath, HARNESS);

  const bin = path.join(BUILD, 'ziphost');
  execFileSync(cc, ['-O2', '-I', WIN, '-o', bin, harnessPath], { stdio: 'inherit' });

  const outDir = path.join(BUILD, "extract_out");
  fs.rmSync(outDir, { recursive: true, force: true });
  fs.mkdirSync(outDir, { recursive: true });
  const res = execFileSync(bin, [exePath, outDir], { encoding: 'utf8', stdio: 'pipe' });
  console.log('  ', res.trim());

  let pass = 0, fail = 0;
  const check = (name, cond, detail) => {
    if (cond) { pass++; console.log('  ok   ' + name); }
    else { fail++; console.log('  FAIL ' + name + (detail ? '  ' + detail : '')); }
  };
  for (const f of realFiles) {
    const p = path.join(outDir, f.name);
    const ok = fs.existsSync(p) && fs.readFileSync(p).equals(f.data);
    check('payload file ' + f.name, ok);
  }

  // now the inflater: a deflated archive built by Node's zlib
  const deflated = buildDeflated([
    { name: 'a/alpha.txt', data: Buffer.from('the quick brown fox jumps over the lazy dog\n'.repeat(400)) },
    { name: 'a/beta.bin', data: Buffer.from(Array.from({ length: 5000 }, (_, i) => i * 37 & 255)) },
    { name: 'empty.txt', data: Buffer.alloc(0) }
  ]);
  const dzip = path.join(BUILD, 'deflated.zip');
  fs.writeFileSync(dzip, deflated);
  const out2 = path.join(BUILD, 'extract_deflate');
  fs.rmSync(out2, { recursive: true, force: true });
  // no footer here: wrap as if it were a payload body
  const wrapped = Buffer.concat([Buffer.alloc(0), deflated]);
  const foot2 = Buffer.alloc(16);
  foot2.write('SPMNRSET', 0, 'ascii');
  foot2.writeUInt32LE(0, 8);
  foot2.writeUInt32LE(wrapped.length, 12);
  const exe2 = path.join(BUILD, 'setup_deflate.exe');
  fs.writeFileSync(exe2, Buffer.concat([wrapped, foot2]));
  execFileSync(bin, [exe2, out2], { stdio: 'pipe' });
  check('deflate: text file', fs.existsSync(path.join(out2, 'a/alpha.txt')) &&
    fs.readFileSync(path.join(out2, 'a/alpha.txt')).equals(Buffer.from('the quick brown fox jumps over the lazy dog\n'.repeat(400))));
  check('deflate: binary file', fs.existsSync(path.join(out2, 'a/beta.bin')) &&
    fs.readFileSync(path.join(out2, 'a/beta.bin')).equals(Buffer.from(Array.from({ length: 5000 }, (_, i) => i * 37 & 255))));
  check('deflate: empty file', fs.existsSync(path.join(out2, 'empty.txt')) && fs.statSync(path.join(out2, 'empty.txt')).size === 0);

  // corruption must be rejected, not crash
  const bad = Buffer.from(deflated);
  const badAt = 30 + 'a/alpha.txt'.length + 40;   /* inside the first entry's deflate stream */
  bad[badAt] ^= 0x5a;
  const exe3 = path.join(BUILD, 'setup_bad.exe');
  const foot3 = Buffer.alloc(16);
  foot3.write('SPMNRSET', 0, 'ascii'); foot3.writeUInt32LE(0, 8); foot3.writeUInt32LE(bad.length, 12);
  fs.writeFileSync(exe3, Buffer.concat([bad, foot3]));
  let rejected = false;
  try { execFileSync(bin, [exe3, path.join(BUILD, 'extract_bad')], { stdio: 'pipe' }); }
  catch (e) { rejected = true; }
  check('corrupt archive rejected safely', rejected);

  /* ---- the real shipped installer ---- */
  const realSetup = path.join(ROOT, 'dist', 'SPACE_MINER_SETUP.exe');
  if (fs.existsSync(realSetup)) {
    const file = fs.readFileSync(realSetup);
    const pOff = file.readUInt32LE(file.length - 8);
    const pLen = file.readUInt32LE(file.length - 4);
    const magic = file.subarray(file.length - 16, file.length - 8).toString('ascii');
    check('setup footer magic', magic === 'SPMNRSET', magic);
    check('setup payload inside file', pOff > 0 && pOff + pLen <= file.length,
      pOff + '+' + pLen + ' <= ' + file.length);
    const realOut = path.join(BUILD, 'extract_real');
    fs.rmSync(realOut, { recursive: true, force: true });
    let okReal = true;
    try { execFileSync(bin, [realSetup, realOut], { stdio: 'pipe' }); } catch (e) { okReal = false; }
    check('real setup extracts', okReal);
    const pairs = [
      ['SPACE_MINER.exe', path.join(ROOT, 'dist', 'SPACE_MINER.exe')],
      ['game/index.html', path.join(ROOT, 'index.html')],
      ['game/icon.ico', path.join(WIN, 'icon.ico')]
    ];
    for (const [rel, want] of pairs) {
      const got = path.join(realOut, rel);
      const same = fs.existsSync(got) && fs.readFileSync(got).equals(fs.readFileSync(want));
      check('real payload matches ' + rel, same);
    }
    const stagedExe = path.join(realOut, 'SPACE_MINER.exe');
    if (fs.existsSync(stagedExe)) {
      const head = fs.readFileSync(stagedExe).subarray(0, 2);
      check('extracted launcher is a PE/MZ image', head.toString('ascii') === 'MZ');
    }
  } else {
    check('real setup present', false, 'run tools/win.js first');
  }

  console.log(`\n  ${pass} passed, ${fail} failed\n`);
  process.exit(fail ? 1 : 0);
}

function buildDeflated(files) {
  const locals = [], central = [];
  let off = 0;
  for (const f of files) {
    const name = Buffer.from(f.name, 'utf8');
    const comp = zlib.deflateRawSync(f.data);
    const crc = crc32(f.data);
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0); lh.writeUInt16LE(20, 4); lh.writeUInt16LE(0x800, 6);
    lh.writeUInt16LE(8, 8); lh.writeUInt16LE(0, 10); lh.writeUInt16LE(0x21, 12);
    lh.writeUInt32LE(crc, 14); lh.writeUInt32LE(comp.length, 18); lh.writeUInt32LE(f.data.length, 22);
    lh.writeUInt16LE(name.length, 26);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0); ch.writeUInt16LE(20, 4); ch.writeUInt16LE(20, 6);
    ch.writeUInt16LE(0x800, 8); ch.writeUInt16LE(8, 10); ch.writeUInt16LE(0, 12);
    ch.writeUInt16LE(0x21, 14); ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(comp.length, 20); ch.writeUInt32LE(f.data.length, 24);
    ch.writeUInt16LE(name.length, 28); ch.writeUInt32LE(off, 42);
    locals.push(lh, name, comp); central.push(ch, name);
    off += 30 + name.length + comp.length;
  }
  const cd = Buffer.concat(central);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(files.length, 8); end.writeUInt16LE(files.length, 10);
  end.writeUInt32LE(cd.length, 12); end.writeUInt32LE(off, 16);
  return Buffer.concat([...locals, cd, end]);
}

main();
