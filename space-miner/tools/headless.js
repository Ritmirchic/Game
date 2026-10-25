/* Headless harness: runs the real game sources against skia-canvas. */
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const { Canvas } = require('skia-canvas');

function makeCanvas(w, h) {
  const c = new Canvas(w, h);
  c.style = {};
  return c;
}

function bootstrap() {
  const g = globalThis;
  g.window = g;
  g.SM = g.SM || {};
  g.SM.SkiaCanvas = Canvas;
  g.document = {
    createElement(tag) { return makeCanvas(2, 2); },
    getElementById() { return null; },
    addEventListener() {}
  };
  g.navigator = { userAgent: 'space-miner-headless' };
  g.requestAnimationFrame = (fn) => setTimeout(() => fn(Date.now()), 16);
  g.devicePixelRatio = 1;

  const files = [
    'core/util.js', 'core/noise.js',
    'art/theme.js', 'art/background.js', 'art/asteroids.js', 'art/sprites.js', 'art/icons.js',
    'data/resources.js', 'data/sectors.js', 'data/upgrades.js', 'data/ships.js', 'data/enemies.js', 'data/achievements.js',
    'game/profile.js', 'game/particles.js', 'game/entities.js', 'game/world.js',
    'game/render.js', 'game/audio.js', 'game/input.js', 'game/game.js',
    'ui/widgets.js', 'ui/hud.js', 'ui/station.js', 'ui/menu.js', 'ui/ui.js'
  ];
  const root = path.join(__dirname, '..');
  for (const f of files) {
    const code = fs.readFileSync(path.join(root, 'src', f), 'utf8');
    vm.runInThisContext(code, { filename: f });
  }
  return g.SM;
}

class FakeInput {
  constructor(w, h) {
    this.mouse = { x: w / 2, y: h / 2, down: false, right: false, wheel: 0, moved: 0 };
    this.keys = {};
    this.pressed = {};
    this.clicks = [];
    this.ax = 0; this.ay = 0;
    this.mine = false; this.fire = false; this.boost = false;
    this.aimWorld = null;
    this.touchMode = false;
    this.touch = { active: false };
    this.screenW = w; this.screenH = h;
  }
  sample() {
    if (this.autopilot) { this.ax = this.autopilot.ax; this.ay = this.autopilot.ay; }
    this.mine = !!this.mineOn;
    this.fire = !!this.fireOn;
    this.boost = !!this.boostOn;
  }
  peekClick() { return this.clicks[0] || null; }
  consumeClick() { return this.clicks.shift() || null; }
  consumeWheel() { const w = this.mouse.wheel; this.mouse.wheel = 0; return w; }
  wasPressed(code) { if (this.pressed[code]) { this.pressed[code] = false; return true; } return false; }
  press(code) { this.pressed[code] = true; this.keys[code] = true; }
  endFrame() { this.pressed = {}; }
}

function makeGame(opts = {}) {
  const SM = bootstrap();
  const w = opts.w || 1280, h = opts.h || 720;
  const canvas = makeCanvas(w, h);
  canvas.style = {};
  canvas.clientWidth = w;
  canvas.clientHeight = h;
  canvas.getBoundingClientRect = () => ({ left: 0, top: 0, width: w, height: h });
  const input = new FakeInput(w, h);
  const game = new SM.Game(canvas, { headless: true, input });
  game.renderer.resize(w, h, 1);
  SM.UI.init(game);
  return { SM, game, canvas, input, w, h };
}

function shot(game, file) {
  const png = game.renderer.canvas.toBufferSync('png');
  fs.writeFileSync(file, png);
  return png.length;
}

module.exports = { bootstrap, makeGame, FakeInput, shot, makeCanvas };
