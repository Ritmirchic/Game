/* ==========================================================================
   Проверка однофайлового билда: собираем neevklid.html во временный файл,
   вытаскиваем инлайновые скрипты и прогоняем их в headless-DOM.
   Игра обязана завестись ровно так же, как из раздельных файлов.
   node tests/standalone.test.js
   ========================================================================== */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const { execFileSync } = require('child_process');
const { makeSandbox } = require('./harness/stub-dom.js');

const ROOT = path.join(__dirname, '..');
const OUT = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'ne-')), 'neevklid.html');
execFileSync(process.execPath, [path.join(ROOT, 'tools', 'build-standalone.js'), OUT], { stdio: 'pipe' });

const html = fs.readFileSync(OUT, 'utf8');
assert.ok(!/<script src=/.test(html), 'в билде не осталось внешних скриптов');
assert.ok(!/<link rel="stylesheet"/.test(html), 'в билде не осталось внешних стилей');
assert.ok(html.includes('<style>'), 'стили инлайном');

const bodies = [];
const re = /<script>([\s\S]*?)<\/script>/g;
let m;
while ((m = re.exec(html))) bodies.push(m[1]);
assert.strictEqual(bodies.length, 4, 'четыре инлайновых скрипта, найдено ' + bodies.length);

const h = makeSandbox();
const ctx = vm.createContext(h.sandbox);
for (const code of bodies) vm.runInContext(code, ctx, { filename: 'neevklid.html:inline' });

const NE = h.sandbox.NE;
assert.ok(NE && NE.debug, 'движок поднялся из однофайлового билда');
const D = NE.debug, st = D.st;

h.ids.btnStart.dispatch('click', {});
assert.strictEqual(st.mode, 'play', 'игра стартует');
h.pump(10);
assert.ok(isFinite(st.x) && isFinite(st.ang), 'кадры считаются без NaN');

const data = D.img.data;
let lit = 0;
for (let k = 0; k < data.length; k += 4) if (data[k] + data[k + 1] + data[k + 2] > 24) lit++;
assert.ok(lit / (data.length / 4) > 0.5, 'кадр отрисован, непустых пикселей ' + (100 * lit / (data.length / 4)).toFixed(0) + '%');

fs.rmSync(path.dirname(OUT), { recursive: true, force: true });
console.log('✓ однофайловый билд собирается и играет (' + bodies.length + ' скрипта, headless-прогон пройден)');
