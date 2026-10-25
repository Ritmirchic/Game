/* Tiny static server for local play-testing:  node tools/serve.js [port]
   Serves the built index.html (and the folder) on 0.0.0.0 so it can be opened
   from a preview URL as well as from localhost. */
const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const PORT = Number(process.argv[2] || process.env.PORT || 8080);

const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.zip': 'application/zip',
  '.exe': 'application/vnd.microsoft.portable-executable',
  '.json': 'application/json'
};

const DOWNLOADS = [
  ['SPACE_MINER_SETUP.exe', 'Установщик для Windows 10/11 x64', 'application/octet-stream'],
  ['SPACE_MINER.exe', 'Лаунчер без установки (нужна папка game рядом)', 'application/octet-stream'],
  ['SPACE_MINER_PORTABLE.zip', 'Портативная сборка: распаковать и запустить', 'application/zip'],
  ['SPACE_MINER_SOURCE.zip', 'Полные исходники, C-код лаунчера, тесты', 'application/zip']
];

function downloadPage() {
  const rows = DOWNLOADS.map(([name, label]) => {
    const p = path.join(ROOT, 'dist', name);
    let size = '—';
    try { size = (fs.statSync(p).size / 1024).toFixed(0) + ' KB'; } catch (e) {}
    return '<a class="row" href="/dist/' + name + '?download=1" download>' +
      '<span class="name">' + name + '</span>' +
      '<span class="label">' + label + '</span>' +
      '<span class="size">' + size + '</span>' +
      '<span class="go">СКАЧАТЬ</span></a>';
  }).join('');
  return '<!doctype html><html lang="ru"><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>SPACE MINER — загрузки</title><style>' +
    'body{margin:0;background:#05090f;color:#d6e6f2;font:15px/1.5 -apple-system,Segoe UI,Roboto,sans-serif;' +
    'display:flex;justify-content:center;padding:56px 20px}' +
    '.wrap{width:100%;max-width:760px}' +
    'h1{font-size:34px;letter-spacing:6px;margin:0 0 6px;font-weight:700}' +
    'h1 span{color:#5fe6d8}' +
    '.sub{color:#7f9cb2;font-family:ui-monospace,Consolas,monospace;font-size:12px;letter-spacing:2px;margin-bottom:28px}' +
    '.row{display:grid;grid-template-columns:1fr auto;gap:2px 16px;align-items:center;' +
    'padding:16px 18px;margin-bottom:10px;text-decoration:none;color:inherit;' +
    'border:1px solid rgba(95,230,216,.25);background:linear-gradient(180deg,rgba(95,230,216,.07),rgba(0,0,0,.25));' +
    'border-left:3px solid #5fe6d8}' +
    '.row:hover{background:linear-gradient(180deg,rgba(95,230,216,.16),rgba(0,0,0,.3));border-color:#5fe6d8}' +
    '.name{font-family:ui-monospace,Consolas,monospace;font-size:14px;color:#fff}' +
    '.label{grid-row:2;grid-column:1;font-size:12.5px;color:#8fb0c6}' +
    '.size{grid-column:2;grid-row:1;font-family:ui-monospace,monospace;font-size:12px;color:#5fe6d8}' +
    '.go{grid-column:2;grid-row:2;font-family:ui-monospace,monospace;font-size:11px;letter-spacing:1.6px;color:#7f9cb2}' +
    '.note{margin-top:22px;font-size:12.5px;color:#7f9cb2}.note b{color:#ffcf5c}' +
    '</style></head><body><div class="wrap">' +
    '<h1>SPACE <span>MINER</span></h1>' +
    '<div class="sub">DEEP BELT OPERATIONS · V1.0.0 · WINDOWS 10/11 x64</div>' +
    rows +
    '<div class="note">Сборка без цифровой подписи: SmartScreen покажет предупреждение — ' +
    '<b>Подробнее → Выполнить всё равно</b>. Играть в браузере: <a href="/index.html" ' +
    'style="color:#5fe6d8">здесь</a>.</div>' +
    '</div></body></html>';
}

const server = http.createServer((req, res) => {
  let rel = decodeURIComponent((req.url || '/').split('?')[0]);
  if (rel === '/' || rel === '') rel = '/index.html';
  if (rel === '/downloads' || rel === '/download') {
    const body = Buffer.from(downloadPage(), 'utf8');
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Content-Length': body.length, 'Cache-Control': 'no-store' });
    return res.end(req.method === 'HEAD' ? undefined : body);
  }
  if (rel.includes('..')) {
    res.writeHead(400, { 'Content-Type': 'text/plain' });
    return res.end('bad path');
  }
  let file = path.join(ROOT, rel);
  if (!file.startsWith(ROOT)) {
    res.writeHead(403, { 'Content-Type': 'text/plain' });
    return res.end('no');
  }
  if (!fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    file = path.join(ROOT, 'index.html');
  }
  const body = fs.readFileSync(file);
  const headers = {
    'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream',
    'Content-Length': body.length,
    'Cache-Control': 'no-store'
  };
  if (/[?&]download=1/.test(req.url || '') || rel.startsWith('/dist/')) {
    headers['Content-Disposition'] = 'attachment; filename="' + path.basename(file) + '"';
  }
  if (/[?&]inline=1/.test(req.url || '')) delete headers['Content-Disposition'];
  res.writeHead(200, headers);
  res.end(req.method === 'HEAD' ? undefined : body);
});

server.listen(PORT, '0.0.0.0', () => {
  console.log('SPACE MINER play-test server: http://0.0.0.0:' + PORT + '/index.html');
});
