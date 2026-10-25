/* =========================================================================
   End-to-end test of the launcher's file server (win/serve.h + win/http_util.h).

   The very same headers the Windows launcher compiles are built here into a
   POSIX host that listens on 127.0.0.1, and is then driven over real TCP
   sockets: MIME types, HEAD, fallbacks, traversal defences, heartbeat.
   ========================================================================= */
const fs = require('fs');
const path = require('path');
const net = require('net');
const { execFileSync, spawn } = require('child_process');

const ROOT = path.join(__dirname, '..');
const BUILD = path.join(ROOT, 'build');
const WIN = path.join(ROOT, 'win');

function findCC() {
  for (const cc of ['/tmp/zigx/ziglang/zig', 'cc', 'gcc', 'clang']) {
    try { execFileSync(cc, ['--version'], { stdio: 'pipe' }); return cc; } catch (e) {}
  }
  return null;
}

const HOST = String.raw`
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "serve.h"

/* ---- host hooks ---- */
const char *srv_root = ".";
static int g_requests = 0;

int srv_file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}
int srv_read_file(const char *path, unsigned char **out, unsigned int *len) {
    FILE *f = fopen(path, "rb");
    long n; unsigned char *b;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 32 * 1024 * 1024) { fclose(f); return 0; }
    b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return 0; }
    if (n && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return 0; }
    fclose(f);
    *out = b; *len = (unsigned int)n;
    return 1;
}
void srv_free_file(unsigned char *b) { free(b); }
void srv_note_request(void) { g_requests++; }

/* ---- write into an fd ---- */
static int fd_write(void *ctx, const char *data, int len) {
    int fd = *(int *)ctx, sent = 0;
    while (sent < len) {
        int r = (int)write(fd, data + sent, len - sent);
        if (r <= 0) return 0;
        sent += r;
    }
    return 1;
}

int main(int argc, char **argv) {
    int port, srv, one = 1;
    struct sockaddr_in a;
    if (argc < 3) { fprintf(stderr, "usage: %s <port> <root>\n", argv[0]); return 2; }
    port = atoi(argv[1]);
    srv_root = argv[2];
    srv = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((unsigned short)port);
    if (bind(srv, (struct sockaddr *)&a, sizeof(a)) != 0) { perror("bind"); return 1; }
    listen(srv, 16);
    printf("ready %d\n", g_requests); fflush(stdout);
    for (;;) {
        int c = accept(srv, NULL, NULL), total = 0, headOnly = 0;
        char req[4096], method[8] = {0}, target[1024] = {0};
        srv_out out;
        if (c < 0) continue;
        for (;;) {
            int r = (int)read(c, req + total, sizeof(req) - 1 - total);
            if (r <= 0) break;
            total += r;
            req[total] = 0;
            if (strstr(req, "\r\n\r\n") || strstr(req, "\n\n")) break;
            if (total > (int)sizeof(req) - 2) break;
        }
        if (total >= 4 && sscanf(req, "%7s %1023s", method, target) == 2) {
            out.write = fd_write; out.ctx = &c;
            headOnly = !strcmp(method, "HEAD");
            serve_request(method, target, headOnly, &out);
        } else if (total >= 4) {
            out.write = fd_write; out.ctx = &c;
            srv_text(&out, 400, "bad request", 0);
        }
        close(c);
    }
    return 0;
}
`;

let pass = 0, fail = 0;
const check = (name, cond, detail) => {
  if (cond) { pass++; console.log('  ok   ' + name); }
  else { fail++; console.log('  FAIL ' + name + (detail ? '  → ' + detail : '')); }
};

function request(port, raw) {
  return new Promise((resolve) => {
    const s = net.connect(port, '127.0.0.1', () => s.write(raw));
    const chunks = [];
    s.on('data', (d) => chunks.push(d));
    s.on('close', () => resolve(Buffer.concat(chunks)));
    s.on('error', () => resolve(Buffer.concat(chunks)));
    setTimeout(() => { s.destroy(); resolve(Buffer.concat(chunks)); }, 4000);
  });
}

function parse(res) {
  const text = res.toString('latin1');
  const idx = text.indexOf('\r\n\r\n');
  const head = idx >= 0 ? text.slice(0, idx) : text;
  const body = idx >= 0 ? res.subarray(idx + 4) : Buffer.alloc(0);
  const lines = head.split('\r\n');
  const status = parseInt(lines[0].split(' ')[1], 10);
  const headers = {};
  for (const l of lines.slice(1)) {
    const i = l.indexOf(':');
    if (i > 0) headers[l.slice(0, i).toLowerCase()] = l.slice(i + 1).trim();
  }
  return { status, headers, body, raw: lines[0] };
}

async function main() {
  fs.mkdirSync(BUILD, { recursive: true });
  const cc = findCC();
  if (!cc) { console.error('no C compiler available'); process.exit(2); }

  // a small but realistic site
  const site = path.join(BUILD, 'site');
  fs.rmSync(site, { recursive: true, force: true });
  fs.mkdirSync(path.join(site, 'sub'), { recursive: true });
  fs.writeFileSync(path.join(site, 'index.html'), '<!doctype html><title>SPACE MINER</title><canvas></canvas>');
  fs.writeFileSync(path.join(site, 'sub', 'app.js'), 'console.log("hi");'.repeat(100));
  fs.writeFileSync(path.join(site, 'sub', 'style.css'), 'body{margin:0}');
  fs.writeFileSync(path.join(site, 'icon.ico'), Buffer.from([0, 0, 1, 0, 1, 0]));
  fs.writeFileSync(path.join(BUILD, 'secret.txt'), 'TOP SECRET');

  const src = path.join(BUILD, 'httpHost.c');
  fs.writeFileSync(src, HOST);
  const bin = path.join(BUILD, 'httpHost');
  execFileSync(cc, ['-O1', '-I', WIN, '-o', bin, src], { stdio: 'inherit' });

  const port = 20000 + Math.floor(Math.random() * 20000);
  const child = spawn(bin, [String(port), site], { stdio: ['ignore', 'pipe', 'pipe'] });
  const ready = await new Promise((resolve) => {
    let out = '';
    child.stdout.on('data', (d) => {
      out += d.toString();
      if (out.includes('ready')) resolve(true);
    });
    child.on('exit', () => resolve(false));
    setTimeout(() => resolve(false), 4000);
  });
  if (!ready) { console.error('server failed to start'); child.kill(); process.exit(1); }

  try {
    let r = parse(await request(port, 'GET / HTTP/1.1\r\nHost: x\r\n\r\n'));
    check('GET / serves index.html', r.status === 200 && r.body.toString().includes('SPACE MINER'));
    check('html content-type', /text\/html/.test(r.headers['content-type'] || ''), r.headers['content-type']);
    check('html not cached aggressively', r.headers['cache-control'] === 'no-store');

    r = parse(await request(port, 'GET /index.html HTTP/1.1\r\n\r\n'));
    check('GET /index.html', r.status === 200 && r.body.toString().includes('<canvas>'));

    r = parse(await request(port, 'GET /sub/app.js HTTP/1.1\r\n\r\n'));
    check('nested js served', r.status === 200 && r.body.toString().startsWith('console.log'));
    check('js content-type', /javascript/.test(r.headers['content-type'] || ''), r.headers['content-type']);
    check('static asset cached', r.headers['cache-control'] === 'max-age=3600');

    r = parse(await request(port, 'GET /sub/style.css HTTP/1.1\r\n\r\n'));
    check('css content-type', /text\/css/.test(r.headers['content-type'] || ''));

    r = parse(await request(port, 'GET /icon.ico HTTP/1.1\r\n\r\n'));
    check('ico mime', r.headers['content-type'] === 'image/x-icon', r.headers['content-type']);

    r = parse(await request(port, 'HEAD /sub/app.js HTTP/1.1\r\n\r\n'));
    check('HEAD returns headers only', r.status === 200 && r.body.length === 0);
    check('HEAD keeps content-length', Number(r.headers['content-length']) > 0);

    r = parse(await request(port, 'GET /deep/route/whatever HTTP/1.1\r\n\r\n'));
    check('unknown route falls back to the shell', r.status === 200 && r.body.toString().includes('SPACE MINER'));

    r = parse(await request(port, 'GET /%2e%2e/secret.txt HTTP/1.1\r\n\r\n'));
    check('encoded traversal rejected', r.status === 400, r.status);
    r = parse(await request(port, 'GET /../secret.txt HTTP/1.1\r\n\r\n'));
    check('plain traversal rejected', r.status === 400, r.status);
    r = parse(await request(port, 'GET /sub/%2e%2e%2f%2e%2e%2fsecret.txt HTTP/1.1\r\n\r\n'));
    check('double encoded traversal rejected', r.status === 400, r.status);
    r = parse(await request(port, 'GET /C:/Windows/win.ini HTTP/1.1\r\n\r\n'));
    check('absolute drive path never leaks', r.status === 400 || !r.body.toString().includes('TOP SECRET'), r.status);
    r = parse(await request(port, 'GET /%00index.html HTTP/1.1\r\n\r\n'));
    check('nul byte rejected', r.status === 400, r.status);

    r = parse(await request(port, 'POST / HTTP/1.1\r\n\r\n'));
    check('POST refused', r.status === 405, r.status);

    r = parse(await request(port, 'GET /__ping?t=123 HTTP/1.1\r\n\r\n'));
    check('heartbeat answers', r.status === 200 && r.body.toString() === 'pong');

    r = parse(await request(port, 'GET /__ping HTTP/1.1\r\n\r\n'));
    check('heartbeat without query', r.status === 200 && r.body.toString() === 'pong');

    // two requests in a row (browser-style keep-alive is not offered: Connection: close)
    r = parse(await request(port, 'GET /sub/app.js HTTP/1.1\r\n\r\n'));
    check('connection closed per request', /close/i.test(r.headers.connection || ''));

    // long/garbage request must not hang the server
    r = parse(await request(port, 'GET /' + 'a'.repeat(3000) + ' HTTP/1.1\r\n\r\n'));
    check('oversized path rejected cleanly', r.status === 400, r.status);
    r = parse(await request(port, 'garbage\r\n\r\n'));
    check('malformed request rejected', r.status === 400, r.status);
  } finally {
    child.kill();
  }

  // pure helpers, checked directly
  r = null;
  const helperSrc = path.join(BUILD, 'helpers.c');
  fs.writeFileSync(helperSrc, String.raw`
#include <stdio.h>
#include <string.h>
#include "http_util.h"
int main(void) {
    char out[512];
    struct { const char *in; int ok; const char *want; } cases[] = {
        { "/", 1, "index.html" },
        { "/index.html", 1, "index.html" },
        { "/sub/", 1, "sub/index.html" },
        { "/a/b.js?x=1", 1, "a/b.js" },
        { "/a/b.js#frag", 1, "a/b.js" },
        { "/a\\b.js", 1, "a\\b.js" },
        { "/%41.txt", 1, "A.txt" },
        { "/../x", 0, "" },
        { "/a/%2e%2e/x", 0, "" },
        { "/a%5c..%5cx", 0, "" },
        { "/x%00y", 0, "" },
        { "/x%zz", 0, "" },
        { "/C:/x", 0, "" },
        { "/sub/index.html", 1, "sub/index.html" }
    };
    int i, bad = 0;
    for (i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        int ok = url_to_rel(cases[i].in, out, sizeof(out));
        int good = (ok == cases[i].ok) && (!ok || !strcmp(out, cases[i].want));
        printf("%-16s %-4s %-20s %s\n", cases[i].in, ok ? "ok" : "deny", ok ? out : "-", good ? "PASS" : "FAIL");
        if (!good) bad++;
    }
    struct { const char *p; const char *m; } mimes[] = {
        { "a.HTML", "text/html" }, { "b.js", "text/javascript" }, { "c.css", "text/css" },
        { "d.json", "application/json" }, { "e.png", "image/png" }, { "f.ico", "image/x-icon" },
        { "g.woff2", "font/woff2" }, { "h.bin", "application/octet-stream" }
    };
    for (i = 0; i < (int)(sizeof(mimes)/sizeof(mimes[0])); i++) {
        const char *got = mime_for(mimes[i].p);
        int good = strncmp(got, mimes[i].m, strlen(mimes[i].m)) == 0;
        printf("mime %-8s %-32s %s\n", mimes[i].p, got, good ? "PASS" : "FAIL");
        if (!good) bad++;
    }
    printf("%d helper failures\n", bad);
    return bad ? 1 : 0;
}
`);
  const helperBin = path.join(BUILD, 'helpers');
  execFileSync(cc, ['-O1', '-I', WIN, '-o', helperBin, helperSrc], { stdio: 'inherit' });
  let helperOut = '';
  try { helperOut = execFileSync(helperBin, { encoding: 'utf8' }); }
  catch (e) { helperOut = (e.stdout || '') + (e.stderr || ''); }
  for (const line of helperOut.trim().split('\n')) {
    if (line.includes('PASS')) { pass++; console.log('  ok   ' + line.replace(' PASS', '')); }
    else if (line.includes('FAIL')) { fail++; console.log('  FAIL ' + line); }
  }

  console.log(`\n  ${pass} passed, ${fail} failed\n`);
  process.exit(fail ? 1 : 0);
}

main().catch((e) => { console.error(e); process.exit(1); });
