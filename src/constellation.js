import { WORLD } from './levels.js';

export function seededRandom(seed = 1234567) {
  let value = seed >>> 0;
  return () => {
    value += 0x6d2b79f5;
    let result = value;
    result = Math.imul(result ^ (result >>> 15), result | 1);
    result ^= result + Math.imul(result ^ (result >>> 7), result | 61);
    return ((result ^ (result >>> 14)) >>> 0) / 4294967296;
  };
}

export function makeStars(count = 180, seed = 9713) {
  const random = seededRandom(seed);
  return Array.from({ length: count }, () => ({
    x: random(),
    y: random(),
    r: 0.35 + random() * 1.35,
    phase: random() * Math.PI * 2,
    depth: 0.25 + random() * 0.75,
  }));
}

export function drawSky(ctx, width, height, stars, time = 0, intensity = 1) {
  const background = ctx.createRadialGradient(
    width * 0.55, height * 0.35, 0,
    width * 0.5, height * 0.5, Math.max(width, height) * 0.85,
  );
  background.addColorStop(0, '#11152a');
  background.addColorStop(0.48, '#090d19');
  background.addColorStop(1, '#05070d');
  ctx.fillStyle = background;
  ctx.fillRect(0, 0, width, height);

  for (const star of stars) {
    const twinkle = 0.55 + Math.sin(time * (0.35 + star.depth * 0.4) + star.phase) * 0.25;
    ctx.globalAlpha = twinkle * star.depth * intensity;
    ctx.fillStyle = star.depth > 0.78 ? '#d9f9f5' : '#b8c4df';
    ctx.beginPath();
    ctx.arc(star.x * width, star.y * height, star.r * Math.min(2, width / 900 + 0.5), 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.globalAlpha = 1;
}

function simplify(recording, maximum = 68) {
  if (recording.length <= maximum) return recording;
  const stride = Math.ceil((recording.length - 1) / (maximum - 1));
  const points = recording.filter((_, index) => index % stride === 0);
  if (points[points.length - 1] !== recording[recording.length - 1]) {
    points.push(recording[recording.length - 1]);
  }
  return points;
}

function sparkle(ctx, x, y, radius, color, alpha = 1) {
  ctx.save();
  ctx.globalAlpha = alpha;
  ctx.strokeStyle = color;
  ctx.lineWidth = Math.max(0.7, radius * 0.22);
  ctx.shadowColor = color;
  ctx.shadowBlur = radius * 3;
  ctx.beginPath();
  ctx.moveTo(x - radius * 2.2, y);
  ctx.lineTo(x + radius * 2.2, y);
  ctx.moveTo(x, y - radius * 2.2);
  ctx.lineTo(x, y + radius * 2.2);
  ctx.stroke();
  ctx.restore();
}

const CHAPTER_COLORS = ['#7ce7df', '#aaa2ff', '#75b7ff', '#d59aff', '#ff9e91', '#ffb38a'];
const CHAPTER_LABELS = [
  'I · ОСТАТЬСЯ',
  'II · ДОВЕРИТЬСЯ',
  'III · СОЕДИНИТЬ',
  'IV · НАВСТРЕЧУ',
  'V · ТИШИНА',
  'VI · ОТПУСТИТЬ',
];

export function drawConstellations(ctx, archive, width, height, options = {}) {
  const {
    time = 0,
    reveal = 1,
    region = { x: 0, y: 0, w: width, h: height },
    labels = true,
  } = options;

  const chapters = archive.slice(0, 6);
  const rows = Math.max(1, Math.ceil(chapters.length / 3));
  chapters.forEach((recordings, chapterIndex) => {
    const column = chapterIndex % 3;
    const row = Math.floor(chapterIndex / 3);
    const centerX = region.x + region.w * (0.18 + column * 0.32);
    const centerY = region.y + region.h * (rows === 1 ? 0.44 : row === 0 ? 0.25 : 0.7);
    const clusterWidth = Math.min(region.w * 0.255, (region.h / rows) * 0.95);
    const color = CHAPTER_COLORS[chapterIndex];
    const usable = recordings.filter((recording) => recording.length > 1);

    usable.forEach((recording, pathIndex) => {
      const source = simplify(recording);
      const points = source.map((point) => ({
        x: centerX + ((point.x - WORLD.width / 2) / WORLD.width) * clusterWidth,
        y: centerY + ((point.y - WORLD.height / 2) / WORLD.height) * clusterWidth * 0.72,
      }));
      const localReveal = Math.max(0, Math.min(1, reveal * 1.35 - pathIndex * 0.035));
      const visibleCount = Math.max(2, Math.ceil(points.length * localReveal));
      const visible = points.slice(0, visibleCount);

      ctx.save();
      ctx.globalAlpha = (0.18 + (pathIndex % 3) * 0.055) * localReveal;
      ctx.strokeStyle = color;
      ctx.lineWidth = Math.max(0.65, clusterWidth * 0.0035);
      ctx.shadowColor = color;
      ctx.shadowBlur = clusterWidth * 0.018;
      ctx.beginPath();
      visible.forEach((point, index) => {
        if (index === 0) ctx.moveTo(point.x, point.y);
        else ctx.lineTo(point.x, point.y);
      });
      ctx.stroke();
      ctx.restore();

      visible.forEach((point, index) => {
        if (index % 11 !== 0 && index !== visible.length - 1) return;
        const pulse = 0.72 + Math.sin(time * 0.8 + index + pathIndex) * 0.18;
        ctx.save();
        ctx.globalAlpha = (index === visible.length - 1 ? 0.9 : 0.48) * localReveal;
        ctx.fillStyle = '#e9fffb';
        ctx.shadowColor = color;
        ctx.shadowBlur = clusterWidth * 0.025;
        ctx.beginPath();
        ctx.arc(point.x, point.y, Math.max(1, clusterWidth * 0.006) * pulse, 0, Math.PI * 2);
        ctx.fill();
        ctx.restore();
      });

      if (visible.length) {
        const endpoint = visible[visible.length - 1];
        sparkle(ctx, endpoint.x, endpoint.y, Math.max(2, clusterWidth * 0.009), color, localReveal * 0.72);
      }
    });

    if (!usable.length) {
      sparkle(ctx, centerX, centerY, Math.max(2, clusterWidth * 0.01), color, reveal * 0.5);
    }

    if (labels) {
      ctx.save();
      ctx.globalAlpha = 0.48 * reveal;
      ctx.fillStyle = '#dbe4f2';
      ctx.textAlign = 'center';
      ctx.font = `500 ${Math.max(9, clusterWidth * 0.037)}px Manrope, sans-serif`;
      ctx.letterSpacing = '2px';
      ctx.fillText(CHAPTER_LABELS[chapterIndex], centerX, centerY + clusterWidth * 0.31 + 28);
      ctx.restore();
    }
  });
}
