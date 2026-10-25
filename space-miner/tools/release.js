/* =========================================================================
   SPACE MINER — one-command release gate.
   Runs every suite in dependency order and prints a single verdict.
     node tools/release.js            (uses zig on PATH or ZIG=...)
     node tools/release.js --skip-win (game-only checks)
   ========================================================================= */
const path = require('path');
const fs = require('fs');
const { execFileSync } = require('child_process');

const ROOT = path.join(__dirname, '..');
const steps = [
  { name: 'build single-file game', cmd: 'tools/build.js', parse: /(\d+\.\d+) KB/ },
  { name: 'gameplay + engine suite', cmd: 'tools/test.js', parse: /(\d+) passed, (\d+) failed/ },
  { name: 'shipped-file smoke test', cmd: 'tools/smoke.js', parse: /(\d+) passed, (\d+) failed/ },
  { name: 'launcher server (HTTP)', cmd: 'tools/testhttp.js', parse: /(\d+) passed, (\d+) failed/ },
  { name: 'windows build', cmd: 'tools/win.js', parse: /setup (\d+)KB/ },
  { name: 'installer payload round-trip', cmd: 'tools/testzip.js', parse: /(\d+) passed, (\d+) failed/ }
];

const skipWin = process.argv.includes('--skip-win');
const results = [];
let failed = 0;

for (const step of steps) {
  if (skipWin && (step.cmd === 'tools/win.js' || step.cmd === 'tools/testzip.js')) continue;
  const t0 = Date.now();
  process.stdout.write(`\n▶ ${step.name}\n`);
  let out = '';
  let ok = true;
  try {
    out = execFileSync(process.execPath, [path.join(ROOT, step.cmd)], {
      encoding: 'utf8', stdio: 'pipe', timeout: 600000
    });
  } catch (e) {
    ok = false;
    out = (e.stdout || '') + (e.stderr || '');
  }
  const ms = Date.now() - t0;
  const tail = out.trim().split('\n').slice(-3).join('\n');
  console.log(tail.split('\n').map((l) => '   ' + l).join('\n'));
  const m = out.match(step.parse);
  if (m && m[2] !== undefined && Number(m[2]) > 0) ok = false;
  if (!ok) failed++;
  results.push({ name: step.name, ok, ms, detail: m ? m.slice(1).join(' / ') : '' });
}

console.log('\n' + '─'.repeat(64));
for (const r of results) {
  console.log(` ${r.ok ? 'PASS' : 'FAIL'}  ${r.name.padEnd(32)} ${(r.ms / 1000).toFixed(1)}s  ${r.detail}`);
}
const dist = path.join(ROOT, 'dist');
if (fs.existsSync(dist)) {
  console.log('\n artifacts in dist/:');
  for (const f of fs.readdirSync(dist).sort()) {
    const st = fs.statSync(path.join(dist, f));
    console.log('   ' + f.padEnd(30) + (st.size / 1024).toFixed(0).padStart(6) + ' KB');
  }
}
console.log('\n ' + (failed ? `${failed} step(s) FAILED` : 'release gate PASSED') + '\n');
process.exit(failed ? 1 : 0);
