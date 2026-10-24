export const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export const lerp = (a, b, t) => a + (b - a) * t;
export const distance = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);

export function circleIntersectsRect(x, y, radius, rect) {
  const nearestX = clamp(x, rect.x, rect.x + rect.w);
  const nearestY = clamp(y, rect.y, rect.y + rect.h);
  const dx = x - nearestX;
  const dy = y - nearestY;
  return dx * dx + dy * dy < radius * radius;
}

export function moveCircle(position, dx, dy, radius, obstacles) {
  const result = { x: position.x, y: position.y };
  const nextX = result.x + dx;
  if (!obstacles.some((wall) => circleIntersectsRect(nextX, result.y, radius, wall))) {
    result.x = nextX;
  }
  const nextY = result.y + dy;
  if (!obstacles.some((wall) => circleIntersectsRect(result.x, nextY, radius, wall))) {
    result.y = nextY;
  }
  return result;
}

export function sampleRecording(points, time) {
  if (!points?.length) return null;
  if (time <= points[0].t) return { x: points[0].x, y: points[0].y, ended: false };
  const last = points[points.length - 1];
  if (time >= last.t) return { x: last.x, y: last.y, ended: true };

  let low = 0;
  let high = points.length - 1;
  while (low + 1 < high) {
    const middle = (low + high) >> 1;
    if (points[middle].t <= time) low = middle;
    else high = middle;
  }
  const a = points[low];
  const b = points[high];
  const amount = (time - a.t) / Math.max(0.00001, b.t - a.t);
  return {
    x: lerp(a.x, b.x, amount),
    y: lerp(a.y, b.y, amount),
    ended: false,
  };
}

export function isPlateActive(plate, entities, extraRadius = 8) {
  return entities.some((entity) => entity && distance(plate, entity) <= plate.r + extraRadius);
}

export function isGateOpen(gate, plates, entities, playerRadius = 12) {
  const allPlatesActive = gate.plates.every((id) => {
    const plate = plates.find((candidate) => candidate.id === id);
    return plate && isPlateActive(plate, entities);
  });

  // A closing gate never traps or crushes someone already in its doorway.
  const doorwayOccupied = entities.some((entity) => entity &&
    circleIntersectsRect(entity.x, entity.y, playerRadius + 3, gate));
  return allPlatesActive || doorwayOccupied;
}

export function hashArchive(archive) {
  let hash = 2166136261;
  for (const chapter of archive) {
    for (const recording of chapter) {
      // Include every point (and especially the endpoint): even a short, unusual
      // route must produce a different sky rather than being sampled away.
      for (let index = 0; index < recording.length; index += 1) {
        const point = recording[index];
        hash ^= (Math.round(point.x * 2) * 31 + Math.round(point.y * 2) * 17 + index);
        hash = Math.imul(hash, 16777619);
      }
      hash ^= recording.length;
      hash = Math.imul(hash, 16777619);
    }
    hash ^= 0x9e3779b9;
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

const ADJECTIVES = ['Тихий', 'Невозможный', 'Светлый', 'Последний', 'Скрытый', 'Живой', 'Обратный', 'Нежный'];
const NOUNS = ['маяк', 'след', 'сад', 'сигнал', 'путь', 'отклик', 'пульс', 'горизонт'];

export function constellationIdentity(archive) {
  const hash = hashArchive(archive);
  const adjective = ADJECTIVES[hash % ADJECTIVES.length];
  const noun = NOUNS[(hash >>> 5) % NOUNS.length];
  return {
    hash,
    name: `${adjective} ${noun}`,
    code: hash.toString(16).toUpperCase().padStart(8, '0').slice(0, 6),
  };
}

export function recordingLength(recording) {
  let length = 0;
  for (let index = 1; index < recording.length; index += 1) {
    length += distance(recording[index - 1], recording[index]);
  }
  return length;
}
