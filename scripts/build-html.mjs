import { readFile, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { build } from 'vite';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const outputDirectory = path.join(root, '.standalone-build');
const outputFile = path.join(root, 'last-beacon.html');

await build({
  configFile: false,
  root,
  base: './',
  logLevel: 'silent',
  build: {
    outDir: outputDirectory,
    emptyOutDir: true,
    rollupOptions: {
      output: { inlineDynamicImports: true },
    },
  },
});

let html = await readFile(path.join(outputDirectory, 'index.html'), 'utf8');
const scriptUrl = html.match(/<script\b[^>]*\bsrc="([^"]+)"[^>]*>/)?.[1];
const styleUrl = html.match(/<link\b[^>]*\brel="stylesheet"[^>]*\bhref="([^"]+)"[^>]*>/)?.[1];
if (!scriptUrl || !styleUrl) throw new Error('Could not locate the generated JS/CSS entry points.');

const resolveAsset = (url) => path.resolve(outputDirectory, url.replace(/^\.\//, '').replace(/^\//, ''));
const asDataUri = async (url, mime, baseDirectory = outputDirectory) => {
  const assetPath = url.startsWith('/')
    ? resolveAsset(url)
    : path.resolve(baseDirectory, url);
  return `data:${mime};base64,${await readFile(assetPath, 'base64')}`;
};

let css = await readFile(resolveAsset(styleUrl), 'utf8');
const assetUrls = new Set();
css = css.replace(/url\(\s*(['"]?)(?!data:)([^)'"]+)\1\s*\)/g, (match, _quote, url) => {
  if (/^(?:https?:|#)/.test(url)) return match;
  assetUrls.add(url);
  return `__INLINE_ASSET_${assetUrls.size - 1}__`;
});
let assetIndex = 0;
for (const url of assetUrls) {
  const dataUri = await asDataUri(url, 'font/woff2', path.dirname(resolveAsset(styleUrl)));
  css = css.replaceAll(`__INLINE_ASSET_${assetIndex}__`, `url("${dataUri}")`);
  assetIndex += 1;
}

let javascript = await readFile(resolveAsset(scriptUrl), 'utf8');
javascript = javascript.replace(/<\/script/gi, '<\\/script');
const favicon = await asDataUri('/favicon.svg', 'image/svg+xml');

html = html
  .replace(/<link\b[^>]*\brel="modulepreload"[^>]*>\s*/g, '')
  .replace(/<script\b[^>]*\bsrc="[^"]+"[^>]*>\s*<\/script>/, '')
  .replace(/<link\b[^>]*\brel="stylesheet"[^>]*>/, () => `<style>${css}</style>`)
  .replace(/(<link\b[^>]*\brel="icon"[^>]*\bhref=")[^"]+(")/, `$1${favicon}$2`)
  .replace('</body>', () => `  <script type="module">${javascript}</script>\n  </body>`)
  .replace('<html lang="ru">', '<html lang="ru">\n<!-- Автономная сборка «Последнего маяка». Работает без сервера и внешних файлов. -->');

await writeFile(outputFile, html, 'utf8');
await rm(outputDirectory, { recursive: true, force: true });
console.log(`Standalone HTML: ${outputFile} (${(Buffer.byteLength(html) / 1024).toFixed(0)} KB)`);
