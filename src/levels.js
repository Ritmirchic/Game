export const WORLD = Object.freeze({ width: 1000, height: 600 });
export const LOOP_DURATION = 45;
export const PLAYER_RADIUS = 12;

const outerWalls = () => [
  { x: 0, y: 0, w: 1000, h: 44 },
  { x: 0, y: 556, w: 1000, h: 44 },
  { x: 0, y: 0, w: 44, h: 600 },
  { x: 956, y: 0, w: 44, h: 600 },
];

export const LEVELS = [
  {
    id: 'remain',
    numeral: 'I',
    title: 'ОСТАТЬСЯ',
    subtitle: 'Иногда, чтобы пройти, нужно остаться.',
    objective: 'Коснись ядра за дверью',
    spawn: { x: 138, y: 300 },
    core: { x: 842, y: 300, r: 25 },
    walls: [
      ...outerWalls(),
      { x: 486, y: 44, w: 28, h: 184 },
      { x: 486, y: 372, w: 28, h: 184 },
    ],
    gates: [
      { id: 'only', x: 486, y: 228, w: 28, h: 144, plates: ['a'] },
    ],
    plates: [
      { id: 'a', x: 292, y: 300, r: 30, label: '01' },
    ],
    firstEcho: 'Теперь у тебя есть прошлое. Оно повторит путь — и останется там, где ты закончил цикл.',
    completion: 'Чтобы пойти дальше, ты оставил часть себя.',
    accent: '#7ce7df',
  },
  {
    id: 'trust',
    numeral: 'II',
    title: 'ДОВЕРИТЬСЯ',
    subtitle: 'Одна дверь. Две пластины. Одного тебя недостаточно.',
    objective: 'Активируй обе пластины одновременно',
    spawn: { x: 138, y: 300 },
    core: { x: 850, y: 300, r: 25 },
    walls: [
      ...outerWalls(),
      { x: 500, y: 44, w: 28, h: 180 },
      { x: 500, y: 376, w: 28, h: 180 },
      { x: 224, y: 270, w: 74, h: 60 },
    ],
    gates: [
      { id: 'double', x: 500, y: 224, w: 28, h: 152, plates: ['north', 'south'] },
    ],
    plates: [
      { id: 'north', x: 342, y: 142, r: 30, label: '02' },
      { id: 'south', x: 342, y: 458, r: 30, label: '03' },
    ],
    firstEcho: 'Одно эхо может держать только одну пластину. Оставь ещё одно — у другой.',
    completion: 'Ты не стал сильнее. Тебя стало больше.',
    accent: '#a9a1ff',
  },
  {
    id: 'release',
    numeral: 'III',
    title: 'ОТПУСТИТЬ',
    subtitle: 'Последняя дверь открывается не наружу.',
    objective: 'Коснись последнего ядра',
    spawn: { x: 154, y: 300 },
    core: { x: 716, y: 300, r: 31 },
    walls: [
      ...outerWalls(),
      { x: 408, y: 44, w: 24, h: 122 },
      { x: 408, y: 434, w: 24, h: 122 },
      { x: 836, y: 180, w: 120, h: 24 },
      { x: 836, y: 396, w: 120, h: 24 },
    ],
    gates: [],
    plates: [],
    echoOnly: true,
    firstEcho: 'Не возвращайся за собой. Позволь записи дойти до конца.',
    completion: 'Цикл завершён.',
    accent: '#ffb38a',
  },
];
