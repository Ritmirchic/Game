/* Headless-смоук-тест игровой оболочки: jsdom + фейковый canvas */
const fs = require('fs');
const path = require('path');
const { JSDOM } = require(process.env.JSDOM_PATH || '/tmp/jt/node_modules/jsdom');

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');
const errors = [];

function fakeCtx(canvas) {
  const grad = { addColorStop() {} };
  const ctx = {
    canvas, imageSmoothingEnabled: false, filter: 'none', globalAlpha: 1,
    globalCompositeOperation: 'source-over', fillStyle: '#000', strokeStyle: '#000',
    lineWidth: 1, font: '', textAlign: 'left',
    createImageData(w, h) { return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }; },
    putImageData() {}, getImageData(w, h) { return { data: new Uint8ClampedArray(4) }; },
    drawImage() {}, save() {}, restore() {}, translate() {}, rotate() {}, scale() {},
    beginPath() {}, closePath() {}, moveTo() {}, lineTo() {}, arc() {}, ellipse() {},
    fill() {}, stroke() {}, fillRect() {}, clearRect() {}, strokeRect() {}, fillText() {},
    createRadialGradient() { return grad; }, createLinearGradient() { return grad; },
    measureText() { return { width: 10 }; }, setTransform() {},
  };
  return ctx;
}

const dom = new JSDOM(html, {
  runScripts: 'dangerously',
  pretendToBeVisual: true,
  beforeParse(window) {
    window.HTMLCanvasElement.prototype.getContext = function () { return fakeCtx(this); };
    window.matchMedia = () => ({ matches: false, addEventListener() {}, removeEventListener() {} });
    window.addEventListener('error', (e) => errors.push('window.error: ' + (e.message || e.error)));
    const origErr = window.console.error;
    window.console.error = (...a) => { errors.push('console.error: ' + a.join(' ')); origErr.apply(window.console, a); };
  },
});
const w = dom.window;
w.addEventListener('unhandledrejection', (e) => errors.push('unhandled: ' + e.reason));

function fire(type, target, props) {
  const ev = new w.Event(type, { bubbles: true, cancelable: true });
  Object.assign(ev, props || {});
  (target || w.document).dispatchEvent(ev);
}

setTimeout(() => {
  const doc = w.document;
  const log = [];
  try {
    // старт игры
    doc.getElementById('btnStart').dispatchEvent(new w.MouseEvent('click', { bubbles: true }));
    log.push('старт: mode ok, intro скрыт = ' + doc.getElementById('intro').classList.contains('gone'));
  } catch (e) { errors.push('start: ' + e.stack); }

  // нажатия
  const press = (k) => {
    w.dispatchEvent(new w.KeyboardEvent('keydown', { key: k, bubbles: true }));
  };
  const release = (k) => w.dispatchEvent(new w.KeyboardEvent('keyup', { key: k, bubbles: true }));

  let t = 0;
  const step = () => {
    t++;
    try {
      press('w');
      if (t % 5 === 0) { press('e'); }
      if (t % 7 === 0) { press('q'); }
      if (t % 11 === 0) { press('m'); }
      if (t % 13 === 0) { press('a'); }
      else release('a');
      if (t === 40) press('Escape');
      if (t === 42) press('Escape');
      release('w');
    } catch (e) { errors.push('tick ' + t + ': ' + e.stack); }
    if (t < 70) setTimeout(step, 6);
    else finish();
  };
  const finish = () => {
    const depth = doc.getElementById('dlabel').textContent;
    const frag = doc.getElementById('frag').textContent;
    const timer = doc.getElementById('timer').textContent;
    console.log('HUD →', JSON.stringify({ depth, frag, timer }));
    console.log('fps-строка:', doc.getElementById('fps').textContent || '(пусто)');
    if (errors.length) {
      console.log('\nОШИБКИ (' + errors.length + '):');
      errors.slice(0, 12).forEach((e) => console.log(' -', e));
      process.exit(1);
    }
    console.log('\nСМОУК OK — ошибок нет');
    process.exit(0);
  };
  step();
}, 300);
