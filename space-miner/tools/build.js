/* Builds a single self-contained index.html from src/*  */
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');
const rd = (p) => fs.readFileSync(path.join(root, p), 'utf8');

function group(files) {
  return files.map((f) => '/* ---- ' + f + ' ---- */\n' + rd('src/' + f)).join('\n');
}

const CORE = ['core/util.js', 'core/noise.js'];
const ART = [
  'art/theme.js', 'art/background.js', 'art/asteroids.js', 'art/sprites.js', 'art/icons.js'
];
const DATA = ['data/resources.js', 'data/sectors.js', 'data/upgrades.js', 'data/ships.js', 'data/enemies.js', 'data/achievements.js'];
const GAME = [
  'game/profile.js', 'game/particles.js', 'game/entities.js', 'game/world.js',
  'game/render.js', 'game/audio.js', 'game/input.js', 'game/game.js'
];
const UI = ['ui/widgets.js', 'ui/hud.js', 'ui/station.js', 'ui/menu.js', 'ui/ui.js'];

let html = rd('src/index.template.html');
html = html
  .replace('/*__CORE__*/', () => group(CORE))
  .replace('/*__ART__*/', () => group(ART))
  .replace('/*__DATA__*/', () => group(DATA))
  .replace('/*__GAME__*/', () => group(GAME))
  .replace('/*__UI__*/', () => group(UI));

const out = path.join(root, 'index.html');
fs.writeFileSync(out, html);
const kb = (Buffer.byteLength(html) / 1024).toFixed(1);
console.log('built index.html —', kb, 'KB');

// also expose the concatenated sources for the headless harness
const bundle = 'globalThis.SM = globalThis.SM || {};\n' + [CORE, ART, DATA, GAME, UI]
  .map(group).join('\n');
fs.writeFileSync(path.join(root, 'tools', '.bundle.js'), bundle);
module.exports = { CORE, ART, DATA, GAME, UI };
