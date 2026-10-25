import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { JSDOM, ResourceLoader } from 'jsdom';

const DOCS_DIR = path.resolve('docs');
const SUBPATH = '/Game';

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.json': 'application/json',
};

let httpRequests = 0;
const httpErrors = [];

const server = http.createServer((req, res) => {
  httpRequests++;
  const urlPath = (req.url || '/').split('?')[0];
  if (!urlPath.startsWith(SUBPATH)) {
    httpErrors.push(`Request escaped subpath: ${urlPath}`);
    res.writeHead(404);
    res.end('Not Found');
    return;
  }
  let rel = urlPath.slice(SUBPATH.length);
  if (rel === '' || rel === '/') rel = '/index.html';
  const filePath = path.join(DOCS_DIR, rel);

  if (!fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
    httpErrors.push(`404 Not Found: ${urlPath}`);
    res.writeHead(404);
    res.end('Not Found');
    return;
  }

  const ext = path.extname(filePath).toLowerCase();
  res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream' });
  fs.createReadStream(filePath).pipe(res);
});

function createMock2DContext(canvas) {
  const grad = { addColorStop() {} };
  return {
    canvas,
    fillStyle: '#000',
    strokeStyle: '#000',
    lineWidth: 1,
    globalAlpha: 1,
    globalCompositeOperation: 'source-over',
    imageSmoothingEnabled: true,
    font: '12px sans-serif',
    textAlign: 'left',
    textBaseline: 'alphabetic',
    lineCap: 'butt',
    lineJoin: 'miter',
    miterLimit: 10,
    shadowOffsetX: 0,
    shadowOffsetY: 0,
    shadowBlur: 0,
    shadowColor: 'rgba(0,0,0,0)',
    save() {},
    restore() {},
    scale() {},
    rotate() {},
    translate() {},
    transform() {},
    setTransform() {},
    resetTransform() {},
    createLinearGradient() { return grad; },
    createRadialGradient() { return grad; },
    createPattern() { return {}; },
    clearRect() {},
    fillRect() {},
    strokeRect() {},
    beginPath() {},
    fill() {},
    stroke() {},
    drawFocusIfNeeded() {},
    clip() {},
    isPointInPath() { return false; },
    isPointInStroke() { return false; },
    fillText() {},
    strokeText() {},
    measureText(str) {
      return {
        width: String(str).length * 8 + 4,
        actualBoundingBoxAscent: 12,
        actualBoundingBoxDescent: 4,
      };
    },
    drawImage() {},
    createImageData(w = 1, h = 1) {
      return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) };
    },
    getImageData(sx = 0, sy = 0, sw = 1, sh = 1) {
      const data = new Uint8ClampedArray(Math.max(4, sw * sh * 4));
      data[0] = 255;
      data[3] = 255;
      return { width: sw, height: sh, data };
    },
    putImageData() {},
    setLineDash() {},
    getLineDash() { return []; },
    closePath() {},
    moveTo() {},
    lineTo() {},
    quadraticCurveTo() {},
    bezierCurveTo() {},
    arcTo() {},
    rect() {},
    roundRect() {},
    arc() {},
    ellipse() {},
  };
}

await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const port = server.address().port;
const baseUrl = `http://127.0.0.1:${port}${SUBPATH}/`;

console.log(`[E2E] Serving production build under subpath: ${baseUrl}`);

const runtimeErrors = [];
const htmlRaw = fs.readFileSync(path.join(DOCS_DIR, 'index.html'), 'utf8');

// Extract module script paths from docs/index.html to verify exact bundle execution
const scriptMatches = [...htmlRaw.matchAll(/src="([^"]+\.js)"/g)].map((m) => m[1]);
if (scriptMatches.length === 0) {
  throw new Error('No bundled JS module found in docs/index.html');
}

const dom = new JSDOM('<!DOCTYPE html><html><body><div id="game-root"></div></body></html>', {
  url: baseUrl,
  pretendToBeVisual: true,
  resources: new ResourceLoader(),
  beforeParse(win) {
    win.__PRISMFORGE_HEADLESS__ = true;
    win.HTMLCanvasElement.prototype.getContext = function (type) {
      if (type === '2d') return createMock2DContext(this);
      return null;
    };
    win.HTMLCanvasElement.prototype.toDataURL = () => 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==';
    win.matchMedia = () => ({
      matches: false,
      addListener() {},
      removeListener() {},
      addEventListener() {},
      removeEventListener() {},
    });
    win.focus = () => {};
    win.scrollTo = () => {};

    win.addEventListener('error', (e) => {
      runtimeErrors.push(`window.error: ${e.message || e.error}`);
    });
    win.addEventListener('unhandledrejection', (e) => {
      runtimeErrors.push(`unhandledrejection: ${e.reason}`);
    });
  },
});

// Set globals & patch JSDOM's post-constructor HTMLCanvasElement stub
dom.window.CanvasRenderingContext2D = function CanvasRenderingContext2D() {};
dom.window.HTMLCanvasElement.prototype.getContext = function (type) {
  if (type === '2d') return createMock2DContext(this);
  return null;
};
dom.window.HTMLCanvasElement.prototype.toDataURL = () =>
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==';

globalThis.window = dom.window;
globalThis.document = dom.window.document;
Object.defineProperty(globalThis, 'navigator', {
  value: dom.window.navigator,
  configurable: true,
});
globalThis.HTMLElement = dom.window.HTMLElement;
globalThis.HTMLCanvasElement = dom.window.HTMLCanvasElement;
globalThis.HTMLVideoElement = dom.window.HTMLVideoElement;
dom.window.URL.createObjectURL = () => 'blob:mock-asset-url';
dom.window.URL.revokeObjectURL = () => {};

// Patch VerifyingImage after JSDOM init so both new Image() and document.createElement('img') fire load!
function attachImageLoader(img, val) {
  if (String(val).startsWith('blob:') || String(val).startsWith('data:')) {
    Object.defineProperty(img, 'width', { value: 360, configurable: true });
    Object.defineProperty(img, 'height', { value: 120, configurable: true });
    Object.defineProperty(img, 'naturalWidth', { value: 360, configurable: true });
    Object.defineProperty(img, 'naturalHeight', { value: 120, configurable: true });
    Object.defineProperty(img, 'complete', { value: true, configurable: true });
    setTimeout(() => {
      img.dispatchEvent(new dom.window.Event('load'));
    }, 2);
    return;
  }
  const resolved = new URL(val, baseUrl).href;
  fetch(resolved)
    .then((r) => {
      if (!r.ok) {
        httpErrors.push(`Asset HTTP ${r.status}: ${resolved}`);
        img.dispatchEvent(new dom.window.Event('error'));
        return;
      }
      return r.arrayBuffer();
    })
    .then((buf) => {
      if (!buf) return;
      Object.defineProperty(img, 'width', { value: 360, configurable: true });
      Object.defineProperty(img, 'height', { value: 120, configurable: true });
      Object.defineProperty(img, 'naturalWidth', { value: 360, configurable: true });
      Object.defineProperty(img, 'naturalHeight', { value: 120, configurable: true });
      Object.defineProperty(img, 'complete', { value: true, configurable: true });
      img.dispatchEvent(new dom.window.Event('load'));
    })
    .catch((err) => {
      runtimeErrors.push(`Image fetch error: ${err.message}`);
    });
}

Object.defineProperty(dom.window.HTMLImageElement.prototype, 'src', {
  set(val) {
    this._src = val;
    attachImageLoader(this, val);
  },
  get() {
    return this._src || '';
  },
  configurable: true,
});

globalThis.URL = dom.window.URL;
globalThis.HTMLImageElement = dom.window.HTMLImageElement;
globalThis.Image = dom.window.Image;
globalThis.XMLHttpRequest = dom.window.XMLHttpRequest;
globalThis.localStorage = dom.window.localStorage;
globalThis.requestAnimationFrame = dom.window.requestAnimationFrame.bind(dom.window);
globalThis.cancelAnimationFrame = dom.window.cancelAnimationFrame.bind(dom.window);
globalThis.Element = dom.window.Element;
globalThis.Event = dom.window.Event;
globalThis.CustomEvent = dom.window.CustomEvent;
globalThis.MouseEvent = dom.window.MouseEvent;
globalThis.KeyboardEvent = dom.window.KeyboardEvent;
globalThis.MutationObserver = dom.window.MutationObserver;
globalThis.DOMParser = dom.window.DOMParser;
globalThis.Audio = dom.window.Audio;
globalThis.FocusEvent = dom.window.FocusEvent;
globalThis.screen = dom.window.screen;

const entryRel = scriptMatches[0].replace(/^\.\//, '');
const entryPath = path.join(DOCS_DIR, entryRel);
try {
  await import(entryPath);
} catch (err) {
  console.error('[E2E] Import error:', err.message);
  console.error(String(err.stack).split('\n').slice(0, 6).join('\n'));
  process.exit(1);
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
await sleep(900);

const game = dom.window.__PRISMFORGE_GAME__;
if (!game) {
  throw new Error('Phaser Game instance failed to initialize');
}

// Wait up to 3s for BootScene asset preloading to transition to MainMenuScene
for (let attempt = 0; attempt < 30; attempt++) {
  if (game.scene.isActive('MainMenuScene')) break;
  await sleep(100);
}

const menuActive = game.scene.isActive('MainMenuScene');
console.log(`[E2E] BootScene -> MainMenuScene active: ${menuActive} (HTTP asset requests: ${httpRequests})`);
if (!menuActive) {
  throw new Error('Failed to reach MainMenuScene after asset preload');
}

// Transition through WardenSelectScene
game.scene.start('WardenSelectScene');
await sleep(180);
if (!game.scene.isActive('WardenSelectScene')) {
  throw new Error('WardenSelectScene failed to activate');
}
console.log('[E2E] WardenSelectScene verified OK');

// Transition through all 4 tabs of MetaScene
for (const tab of ['tree', 'codex', 'achievements', 'settings']) {
  game.scene.start('MetaScene', { tab });
  await sleep(120);
  if (!game.scene.isActive('MetaScene')) {
    throw new Error(`MetaScene (${tab}) failed to activate`);
  }
}
console.log('[E2E] MetaScene (tree, codex, achievements, settings) verified OK');

// Start GameScene & run live combat simulation + keyboard input!
game.scene.start('GameScene', { wardenId: 'lyra', modeId: 'campaign', resumeSaved: false });
await sleep(220);
const gameScene = game.scene.getScene('GameScene');
if (!gameScene || !game.scene.isActive('GameScene')) {
  throw new Error('GameScene failed to activate');
}

// Dispatch keyboard 'r' (rotate prism) and ' ' (launch Wave 1!)
dom.window.dispatchEvent(new dom.window.KeyboardEvent('keydown', { key: 'r', bubbles: true }));
dom.window.dispatchEvent(new dom.window.KeyboardEvent('keydown', { key: ' ', bubbles: true }));
await sleep(450);

// Dispatch tactical abilities '1', '2', '3' and pause/unpause 'Escape'
dom.window.dispatchEvent(new dom.window.KeyboardEvent('keydown', { key: '1', bubbles: true }));
dom.window.dispatchEvent(new dom.window.KeyboardEvent('keydown', { key: '2', bubbles: true }));
dom.window.dispatchEvent(new dom.window.KeyboardEvent('keydown', { key: '3', bubbles: true }));
await sleep(300);
dom.window.dispatchEvent(new dom.window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
await sleep(80);
dom.window.dispatchEvent(new dom.window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
await sleep(200);

const savedPayload = dom.window.localStorage.getItem('prismforge_save_v2');
if (!savedPayload) {
  throw new Error('Save data was not persisted to localStorage during E2E run');
}
const parsedSave = JSON.parse(savedPayload);
console.log(
  `[E2E] Save verified OK (version=${parsedSave.version}, runsStarted=${parsedSave.stats.runsStarted}, pulsesEmitted=${parsedSave.stats.pulsesEmitted})`
);

game.destroy(true);
server.close();

if (httpErrors.length > 0 || runtimeErrors.length > 0) {
  console.error('[E2E] ERRORS DETECTED:', { httpErrors, runtimeErrors });
  process.exit(1);
}

console.log(`[E2E] ALL PRODUCTION BUNDLE & SUBPATH CHECKS PASSED (${httpRequests} HTTP requests, 0 errors)!`);
process.exit(0);
