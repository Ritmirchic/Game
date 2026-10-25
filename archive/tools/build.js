/* Собирает один автономный index.html из src/*.js */
const fs = require('fs');
const path = require('path');
const root = path.join(__dirname, '..');
const rd = (p) => fs.readFileSync(path.join(root, p), 'utf8');
const tpl = rd('src/index.template.html');
const html = tpl
  .replace('/*__WORLD__*/', () => rd('src/world.js'))
  .replace('/*__RENDER__*/', () => rd('src/render.js'))
  .replace('/*__GAME__*/', () => rd('src/game.js'));
fs.writeFileSync(path.join(root, 'index.html'), html);
console.log('index.html:', (html.length / 1024).toFixed(1), 'KB');
