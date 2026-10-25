/* ==========================================================================
   Сборка однофайловой версии: css и все скрипты инлайнятся в neevklid.html.
   Такой файл можно скачать и открыть двойным кликом — игра работает офлайн.
   node tools/build-standalone.js [выходной_файл]
   ========================================================================== */
'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const OUT = process.argv[2] ? path.resolve(process.argv[2]) : path.join(ROOT, 'neevklid.html');

let html = fs.readFileSync(path.join(ROOT, 'index.html'), 'utf8');
const css = fs.readFileSync(path.join(ROOT, 'css', 'style.css'), 'utf8');

if (/<\/style/i.test(css)) throw new Error('в CSS есть "</style>" — инлайн сломается');

html = html.replace(
  '<link rel="stylesheet" href="css/style.css">',
  '<style>\n' + css.trim() + '\n</style>'
);

const scripts = ['src/logic.js', 'src/textures.js', 'src/audio.js', 'src/game.js'];
for (const rel of scripts) {
  const code = fs.readFileSync(path.join(ROOT, rel), 'utf8');
  if (/<\/script/i.test(code)) throw new Error(rel + ' содержит "</script>" — инлайн сломается');
  const tag = '<script src="' + rel + '"></script>';
  if (!html.includes(tag)) throw new Error('не найден тег ' + tag);
  html = html.replace(tag, '<script>\n/* ==== ' + rel + ' ==== */\n' + code.trim() + '\n</script>');
}

if (/src="(css|src)\//.test(html)) throw new Error('в html остались внешние ссылки на css/src');

fs.writeFileSync(OUT, html);
const kb = (fs.statSync(OUT).size / 1024).toFixed(1);
console.log('собрано: ' + OUT + ' (' + kb + ' КБ, один файл, работает офлайн)');
