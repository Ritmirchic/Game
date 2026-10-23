import { readFile, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { build } from 'esbuild';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const outputDirectory = path.join(root, '.standalone-build');
const outputFile = path.join(root, 'mirage.html');
const scriptFile = path.join(outputDirectory, 'main.js');
const styleFile = path.join(outputDirectory, 'main.css');

await rm(outputDirectory, { recursive: true, force: true });
await build({
  absWorkingDir: root,
  entryPoints: ['src/main.ts'],
  outfile: scriptFile,
  bundle: true,
  format: 'iife',
  platform: 'browser',
  target: 'es2022',
  minify: true,
  legalComments: 'inline',
  loader: { '.woff2': 'dataurl' },
  logLevel: 'warning',
});

let html = await readFile(path.join(root, 'index.html'), 'utf8');
let javascript = await readFile(scriptFile, 'utf8');
const css = await readFile(styleFile, 'utf8');
javascript = javascript.replace(/<\/script/gi, '<\\/script');

const asDataUri = async (filePath, mime) =>
  `data:${mime};base64,${await readFile(filePath, 'base64')}`;
const favicon = await asDataUri(path.join(root, 'public', 'mirage.svg'), 'image/svg+xml');
const embeddedAssets = [
  'soldier.glb', 'fox.glb', 'dead-tree.glb', 'outdoor.env',
  'grass-photo.jpg', 'grass-normal.png', 'sand.jpg', 'water-bump.png',
  'rock.png', 'rock-normal.png', 'bark.png', 'tree-billboard.png',
  'wood.jpg', 'wood-bump.jpg', 'wood-roughness.jpg',
];
const mimeByExtension = {
  '.glb': 'model/gltf-binary', '.env': 'application/octet-stream',
  '.jpg': 'image/jpeg', '.png': 'image/png',
};
const assetManifest = {};
for (const name of embeddedAssets) {
  const data = await readFile(path.join(root, 'public', 'assets', name), 'base64');
  assetManifest[name] = `data:${mimeByExtension[path.extname(name)]};base64,${data}`;
}
const assetBootstrap = `<script>window.__MIRAGE_ASSETS__=${JSON.stringify(assetManifest)};</script>`;

html = html
  .replace(/<script\b[^>]*\bsrc="\/src\/main\.ts"[^>]*>\s*<\/script>/, '')
  .replace('</head>', () => `  <style>${css}</style>\n  </head>`)
  .replace(/(<link\b[^>]*\brel="icon"[^>]*\bhref=")[^"]+(")/, `$1${favicon}$2`)
  .replace('</body>', () => `  ${assetBootstrap}\n  <script>${javascript}</script>\n  </body>`)
  .replace('<html lang="ru">', '<html lang="ru">\n<!-- Автономная сборка «МИРАЖА». Работает без сервера и внешних файлов. -->');

if (/<script\b[^>]*\bsrc=|<link\b[^>]*\brel="stylesheet"/i.test(html)) {
  throw new Error('Standalone HTML still contains an external script or stylesheet.');
}

// Shader template literals from dependencies may contain trailing spaces.
// Trim line ends only; embedded base64 and executable tokens remain unchanged.
html = html.replace(/[ \t]+$/gm, '');
await writeFile(outputFile, html, 'utf8');
await rm(outputDirectory, { recursive: true, force: true });
console.log(`Standalone HTML: ${outputFile} (${(Buffer.byteLength(html) / 1024).toFixed(0)} KB)`);
