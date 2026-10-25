import * as esbuild from 'esbuild';
import fs from 'node:fs';

const watch = process.argv.includes('--watch');

/** Общая сборка: один IIFE-файл — работает даже при открытии index.html двойным кликом (file://). */
const opts = {
  entryPoints: ['src/main.js'],
  bundle: true,
  format: 'iife',
  target: ['es2020'],
  outfile: 'dist/game.js',
  minify: !watch,
  sourcemap: false,
  legalComments: 'none',
  logLevel: 'info',
};

/** Один автономный HTML: удобно скачать и запустить без сервера. */
async function buildSingle() {
  const res = await esbuild.build({
    ...opts,
    outfile: undefined,
    write: false,
    minify: true,
  });
  const js = res.outputFiles[0].text;
  const html = fs.readFileSync('index.html', 'utf8')
    .replace(
      /<script defer src="\.\/dist\/game\.js"><\/script>/,
      () => `<script>${js}</script>`
    );
  fs.writeFileSync('mobius.html', html);
  const kb = (Buffer.byteLength(html) / 1024).toFixed(0);
  console.log(`mobius.html — ${kb} КБ (один файл, без интернета)`);
}

if (watch) {
  const ctx = await esbuild.context(opts);
  await ctx.watch();
  console.log('watching…');
} else {
  await esbuild.build(opts);
  await buildSingle();
}
