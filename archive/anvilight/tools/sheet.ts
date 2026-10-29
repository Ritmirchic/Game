import { createCanvas, Image } from '@napi-rs/canvas';
import { writeFileSync } from 'node:fs';
const g = globalThis as unknown as Record<string, unknown>;
g.window = g; g.devicePixelRatio = 1;
function makeCanvas(w: number, h: number) {
  const c = createCanvas(w, h) as unknown as HTMLCanvasElement;
  (c as unknown as Record<string, unknown>).getBoundingClientRect = () => ({ left: 0, top: 0, width: c.width, height: c.height, right: c.width, bottom: c.height, x: 0, y: 0 });
  return c;
}
g.document = { createElement: (tag: string) => (tag === 'canvas' ? makeCanvas(16, 16) : {}) };
const { drawPortrait } = await import('../src/render');
const { ENEMIES, HEROES } = await import('../src/core/content');
const S = 210;
const list = [
  ...HEROES.map((h) => ({ kind: 'hero' as const, id: h.id })),
  ...ENEMIES.map((e) => ({ kind: 'enemy' as const, id: e.id })),
];
const cols = 5;
const rows = Math.ceil(list.length / cols);
const sheet = makeCanvas(cols * S, rows * S);
const ctx = sheet.getContext('2d') as unknown as CanvasRenderingContext2D;
ctx.fillStyle = '#0a0d15';
ctx.fillRect(0, 0, cols * S, rows * S);
list.forEach((it, i) => {
  const x = (i % cols) * S;
  const y = Math.floor(i / cols) * S;
  ctx.save();
  ctx.beginPath();
  ctx.rect(x, y, S, S);
  ctx.clip();
  drawPortrait(ctx, it.kind, it.id, x, y, S);
  ctx.restore();
  ctx.strokeStyle = 'rgba(255,255,255,0.08)';
  ctx.strokeRect(x, y, S, S);
});
writeFileSync('shots/sheet.png', (sheet as unknown as { toBuffer: (m: string) => Buffer }).toBuffer('image/png'));
void Image;
console.log('sheet written', list.length);
process.exit(0);
