// Сборка + сервер с автопересборкой.
import * as esbuild from 'esbuild';
import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));

const ctx = await esbuild.context({
  entryPoints: [path.join(root, 'src/main.js')],
  bundle: true,
  format: 'esm',
  target: ['es2020'],
  outfile: path.join(root, 'dist/game.js'),
  logLevel: 'info',
});
await ctx.watch();

const srv = spawn(process.execPath, [path.join(root, 'server.mjs')], {
  stdio: 'inherit',
  cwd: root,
});
srv.on('exit', (c) => process.exit(c ?? 0));
