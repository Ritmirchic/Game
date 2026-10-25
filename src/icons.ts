// ── Hand-authored inline SVG glyphs (no emoji, no external assets) ───────────
type Glyph = { paths: string[]; circles?: [number, number, number][]; fill?: string[] };

const G: Record<string, Glyph> = {
  flame: {
    paths: [
      'M12 2.5c2.4 4.2 5.6 5.6 5.6 10.4A5.6 5.6 0 0 1 6.4 13c0-2.2 1-3.6 2.4-5 .3 1.3 1 2 2 2.4-.4-2.7 .4-5.6 1.2-7.9Z',
      'M12 20.5a3 3 0 0 1-3-3c0-1.7 1.4-2.6 3-4.6 1.6 2 3 2.9 3 4.6a3 3 0 0 1-3 3Z',
    ],
  },
  step: { paths: ['M5 18c3 0 4-2 4-4s1-4 4-4 3-2 3-4', 'M13 5h3v3', 'M4 21h5'] },
  lance: { paths: ['M3 12h13', 'M12 7l6 5-6 5', 'M19 4v3', 'M19 17v3'] },
  quake: {
    paths: ['M3 15c2.2 0 2.2-2 4.4-2s2.2 2 4.4 2 2.2-2 4.4-2 2.2 2 4.4 2', 'M3 20c2.2 0 2.2-2 4.4-2s2.2 2 4.4 2 2.2-2 4.4-2 2.2 2 4.4 2', 'M12 3v7', 'M9 6l3-3 3 3'],
  },
  bolt: { paths: ['M13 2 5 13h5l-1 9 8-12h-5l1-8Z'] },
  wall: { paths: ['M3 6h18v12H3z', 'M3 12h18', 'M9 6v6', 'M15 12v6'] },
  bellows: { paths: ['M4 9h6l8-4v14l-8-4H4z', 'M4 9v6', 'M18 8c1.6 1.3 1.6 6.7 0 8'] },
  shield: { paths: ['M12 3 5 6v5c0 4.4 3 8 7 10 4-2 7-5.6 7-10V6l-7-3Z', 'M9 12l2.2 2.2L15.5 10'] },
  heal: { paths: ['M12 20s-7-4.4-7-9.2A4 4 0 0 1 12 8a4 4 0 0 1 7 2.8C19 15.6 12 20 12 20Z', 'M12 11v5', 'M9.5 13.5h5'] },
  bomb: { paths: ['M11 21a6 6 0 1 0 0-12 6 6 0 0 0 0 12Z', 'M15 6c0-2 1-3 3-3', 'M18 3l2 2', 'M9 19h4'] },
  brand: { paths: ['M12 21a7 7 0 0 1-7-7c0-4 3-5 4-9 2 2 3 4 3 6 1-1 2-2 2-4 2 2 5 4 5 7a7 7 0 0 1-7 7Z'] },
  flare: { paths: ['M12 4v4', 'M12 16v4', 'M4 12h4', 'M16 12h4', 'M6.3 6.3l2.8 2.8', 'M14.9 14.9l2.8 2.8', 'M17.7 6.3l-2.8 2.8', 'M9.1 14.9l-2.8 2.8', 'M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z'] },
  hook: { paths: ['M5 4v6a5 5 0 0 0 10 0V8', 'M15 11a4 4 0 0 0 4-4V4', 'M3 4h4', 'M17 2h4v4'] },
  vent: { paths: ['M4 19c2 0 2-2 4-2s2 2 4 2 2-2 4-2 2 2 4 2', 'M8 4c0 3 2 3 2 6', 'M13 3c0 3 2 3 2 6', 'M18 5c0 2 1 2 1 4'] },
  bulwark: { paths: ['M12 3 4 6v6c0 4 3.5 7 8 9 4.5-2 8-5 8-9V6l-8-3Z', 'M8 12h8', 'M12 8v8'] },
  dash: { paths: ['M3 12h11', 'M10 7l5 5-5 5', 'M18 5v14', 'M21 8v8'] },
  tithe: { paths: ['M12 3v18', 'M6 7h12', 'M5 20h14', 'M9 12h6'] },
  hammer: { paths: ['M4 20l7-7', 'M9 4h6l3 3v5h-5L9 8V4Z', 'M15 12l-3 3'] },
  anvil: { paths: ['M3 9h13l3 3h2v3h-6l-2 4H7l2-4H3V9Z', 'M2 20h14'] },
  momentum: { paths: ['M3 16h9', 'M8 11l5 5-5 5', 'M15 5h6', 'M18 2l3 3-3 3'] },
  axe: { paths: ['M14 3 5 12l3 3 9-9', 'M14 3c3 0 6 2 7 5-3 1-5 0-7-2', 'M6 14l-3 3 4 4 3-3'] },
  echo: { paths: ['M12 4a8 8 0 1 1-7.4 5', 'M12 8a4 4 0 1 1-3.6 2.5', 'M4 4v5h5'] },
  heart: { paths: ['M12 20s-7-4.4-7-9.2A4 4 0 0 1 12 8a4 4 0 0 1 7 2.8C19 15.6 12 20 12 20Z'] },
  armor: { paths: ['M12 3 5 6v6c0 4 3 7 7 9 4-2 7-5 7-9V6l-7-3Z', 'M9 9h6v3l-3 2-3-2V9Z'] },
  spark: { paths: ['M12 2v5', 'M12 17v5', 'M2 12h5', 'M17 12h5', 'M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z'] },
  pyre: { paths: ['M12 21a6 6 0 0 1-6-6c0-4 4-5 4-9 3 2 3 5 3 5 2-1 2-3 2-3 3 3 3 4 3 7a6 6 0 0 1-6 6Z', 'M4 21h16'] },
  ricochet: { paths: ['M3 18l5-9 5 9', 'M13 9l4 7 4-7', 'M6 15h4'] },
  blood: { paths: ['M12 3c3 5 6 6.5 6 10a6 6 0 0 1-12 0c0-3.5 3-5 6-10Z', 'M9 14h6'] },
  coin: { paths: ['M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18Z', 'M12 7v10', 'M15 9.5c-.6-1-1.7-1.5-3-1.5-1.6 0-2.6.8-2.6 1.9 0 3 5.8 1.4 5.8 4.3 0 1.2-1.2 1.9-3 1.9-1.4 0-2.5-.5-3-1.5'] },
  chest: { paths: ['M3 10h18v10H3z', 'M3 10c0-3 2-5 5-5h8c3 0 5 2 5 5', 'M10 13h4v4h-4z'] },
  sword: { paths: ['M4 20l5-5', 'M16 3h5v5L9 20l-5-5L16 3Z', 'M11 7l6 6'] },
  skull: { paths: ['M12 3a8 8 0 0 0-4 15v3h8v-3a8 8 0 0 0-4-15Z', 'M9 11h.01', 'M15 11h.01', 'M10 17h4'] },
  camp: { paths: ['M12 4 3 20h18L12 4Z', 'M12 10l-4 10h8l-4-10Z'] },
  boss: { paths: ['M4 20V8l4 4 4-8 4 8 4-4v12H4Z', 'M8 20v-4h8v4'] },
  trophy: { paths: ['M7 4h10v5a5 5 0 0 1-10 0V4Z', 'M7 6H4v2a3 3 0 0 0 3 3', 'M17 6h3v2a3 3 0 0 1-3 3', 'M10 20h4', 'M12 14v6'] },
  crown: { paths: ['M3 18 5 7l4 5 3-7 3 7 4-5 2 11H3Z', 'M6 21h12'] },
  core: { paths: ['M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18Z', 'M12 16a4 4 0 1 0 0-8 4 4 0 0 0 0 8Z', 'M12 2v2', 'M12 20v2', 'M2 12h2', 'M20 12h2'] },
  whetstone: { paths: ['M3 15 15 3l6 6-12 12H3v-6Z', 'M11 7l6 6'] },
  lock: { paths: ['M6 11h12v10H6z', 'M9 11V8a3 3 0 0 1 6 0v3', 'M12 15v3'] },
  star: { paths: ['M12 3l2.7 5.8 6.3.8-4.6 4.4 1.2 6.2L12 17.2 6.4 20.2 7.6 14 3 9.6l6.3-.8L12 3Z'] },
  settings: { paths: ['M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6Z', 'M12 2v3', 'M12 19v3', 'M2 12h3', 'M19 12h3', 'M4.9 4.9 7 7', 'M17 17l2.1 2.1', 'M19.1 4.9 17 7', 'M7 17l-2.1 2.1'] },
  close: { paths: ['M6 6l12 12', 'M18 6 6 18'] },
  arrow: { paths: ['M5 12h14', 'M13 6l6 6-6 6'] },
  book: { paths: ['M4 4h7a3 3 0 0 1 3 3v13a3 3 0 0 0-3-3H4V4Z', 'M20 4h-3a3 3 0 0 0-3 3v13a3 3 0 0 1 3-3h3V4Z'] },
  sound: { paths: ['M4 9h4l5-4v14l-5-4H4V9Z', 'M17 9a4 4 0 0 1 0 6', 'M19.5 6.5a8 8 0 0 1 0 11'] },
  music: { paths: ['M9 18V5l10-2v13', 'M9 18a3 3 0 1 1-6 0 3 3 0 0 1 6 0Z', 'M19 16a3 3 0 1 1-6 0 3 3 0 0 1 6 0Z'] },
  pause: { paths: ['M8 5v14', 'M16 5v14'] },
  route: { paths: ['M6 20V8a4 4 0 0 1 4-4h4', 'M17 7l3-3-3-3', 'M10 16l3 3-3 3', 'M6 12h6'] },
};

export function icon(id: string, className = 'ic'): string {
  const g = G[id] ?? G.star;
  const circles = (g.circles ?? [])
    .map(([cx, cy, r]) => `<circle cx="${cx}" cy="${cy}" r="${r}" fill="currentColor" stroke="none"/>`)
    .join('');
  const paths = g.paths
    .map((d) => `<path d="${d}" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/>`)
    .join('');
  return `<svg class="${className}" viewBox="0 0 24 24" aria-hidden="true" focusable="false">${paths}${circles}</svg>`;
}

export function hasIcon(id: string): boolean {
  return id in G;
}
