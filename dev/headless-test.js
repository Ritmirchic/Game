// Headless-прогон игры: мок DOM + Canvas 2D, чтобы поймать рантайм-ошибки
// и проверить, что механика (стены/сдвиг по W/сбор/финиш) действительно работает.
const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync('/home/user/Game/hypercut.html', 'utf8');
const code = html.match(/<script>([\s\S]*)<\/script>/)[1];

const calls = { fill: 0, stroke: 0, arc: 0, moveTo: 0 };
const grad = () => ({ addColorStop(){} });
const ctxMock = new Proxy({}, {
  get(t, k){
    if (k === 'createLinearGradient' || k === 'createRadialGradient') return grad;
    if (k === 'measureText') return () => ({ width: 120 });
    if (k === 'canvas') return { width: 1280, height: 720 };
    if (k === 'createPattern') return () => null;
    if (k === 'getImageData') return () => ({ data: new Uint8ClampedArray(4) });
    if (typeof k === 'string' && /^(fill|stroke|arc|moveTo|lineTo|closePath|beginPath|save|restore|translate|rotate|scale|clip|setTransform|fillRect|strokeRect|clearRect|quadraticCurveTo|arcTo|bezierCurveTo|fillText|strokeText|drawImage|rect|ellipse|setLineDash)$/.test(k)){
      return (...a) => { if (calls[k] !== undefined) calls[k]++; return undefined; };
    }
    return undefined;
  },
  set(){ return true; }
});

class Path2Dmock {
  constructor(){ this.ops = 0; }
  moveTo(){ this.ops++; } lineTo(){ this.ops++; } closePath(){} rect(){} arc(){} ellipse(){}
  bezierCurveTo(){} quadraticCurveTo(){} addPath(){}
}

const listeners = new Map();
function addEL(type, fn){
  if (!listeners.has(type)) listeners.set(type, []);
  listeners.get(type).push(fn);
}
function fire(type, ev){
  const arr = listeners.get(type) || [];
  for (const fn of arr) fn(Object.assign({
    preventDefault(){}, stopPropagation(){}, movementX: 0, movementY: 0, deltaY: 0, key: '', clientX: 0, clientY: 0
  }, ev));
}

const elCache = new Map();
function mkEl(id){
  const el = {
    id, textContent: '', style: {},
    classList: { _s: new Set(['hidden']), add(c){ this._s.add(c); }, remove(c){ this._s.delete(c); }, contains(c){ return this._s.has(c); } },
    _l: new Map(),
    addEventListener(t, fn){ if (!this._l.has(t)) this._l.set(t, []); this._l.get(t).push(fn); },
    requestPointerLock(){ return Promise.resolve(); },
    getContext(){ return ctxMock; },
    width: 1280, height: 720
  };
  el.click = () => { const a = el._l.get('click') || []; a.forEach(f => f({ preventDefault(){} })); };
  return el;
}

const rafQueue = [];
const sandbox = {
  console,
  setTimeout, clearTimeout, setInterval, clearInterval,
  performance: { now: () => Date.now() },
  Path2D: Path2Dmock,
  Math, Date, JSON, Object, Array, Number, String, Boolean, isFinite, isNaN, Uint8Array, Int32Array, Float32Array, Uint8ClampedArray,
  requestAnimationFrame: (cb) => { rafQueue.push(cb); return rafQueue.length; }
};
sandbox.window = sandbox;
sandbox.self = sandbox;
sandbox.globalThis = sandbox;
sandbox.devicePixelRatio = 1;
sandbox.innerWidth = 1280; sandbox.innerHeight = 720;
sandbox.addEventListener = addEL;
sandbox.AudioContext = undefined;
sandbox.webkitAudioContext = undefined;
sandbox.document = {
  getElementById: (id) => { if (!elCache.has(id)) elCache.set(id, mkEl(id)); return elCache.get(id); },
  addEventListener: addEL,
  pointerLockElement: null,
  exitPointerLock(){},
  body: mkEl('body'),
  querySelector: () => mkEl('q')
};

vm.createContext(sandbox);
let ok = true;
function step(label){ process.stdout.write('  · ' + label + '\n'); }

try {
  vm.runInContext(code, sandbox, { filename: 'hypercut.js' });
  step('скрипт инициализирован без исключений');

  const G = sandbox.window.HYPERCUT;
  if (!G) throw new Error('нет отладочного люка HYPERCUT');
  const { S, LV } = G;

  // один кадр
  function frame(dt){
    const arr = rafQueue.splice(0, rafQueue.length);
    for (const cb of arr) cb(performanceNow += dt);
  }
  let performanceNow = 1000;

  // 40 кадров титульного экрана (4D-проекция уровня)
  for (let i=0;i<40;i++) frame(16.7);
  step('титульная сцена: 40 кадров, 4D-проекция жива');
  if (!(calls.stroke > 0)) throw new Error('титульный экран ничего не нарисовал');

  // старт
  elCache.get('startBtn').click();
  if (S.mode !== 'play') throw new Error('игра не стартовала, mode=' + S.mode);
  for (let i=0;i<5;i++) frame(16.7);
  step('старт: mode=play, W=' + S.w.toFixed(2) + ', позиция=' + S.px.toFixed(2) + ',' + S.pz.toFixed(2));

  // идём вперёд 2 секунды — должна быть стена/движение
  const x0 = S.px, z0 = S.pz;
  fire('keydown', { key: 'w' });
  for (let i=0;i<120;i++) frame(16.7);
  fire('keyup', { key: 'w' });
  const moved = Math.hypot(S.px - x0, S.pz - z0);
  step('движение вперёд: смещение ' + moved.toFixed(2) + ' м');
  if (!(moved > 0.4)) throw new Error('игрок не двигается');

  // стена на старте должна быть «нижней» и исчезать при сдвиге по W
  const demoWall = LV.boxes.find(b => b.kind === 'low' && Math.abs(b.cz - LV.spawn.z) < 6);
  if (!demoWall) throw new Error('нет обучающей стены у старта');
  const blocksAtStart = (S.w >= demoWall.w0 && S.w <= demoWall.w1);
  S.w = 1.2;
  const blocksAfter = (S.w >= demoWall.w0 && S.w <= demoWall.w1);
  step('обучающая стена: непроходима при W=-1.2 → ' + blocksAtStart + ', исчезла при W=1.2 → ' + !blocksAfter);
  if (!blocksAtStart || blocksAfter) throw new Error('механика 4-й оси сломана');

  // стены в 4D: считаем, сколько стен видимо при разных W
  const countAt = w => LV.boxes.filter(b => w >= b.w0 && w <= b.w1).length;
  step('стен в срезе: W=-2 → ' + countAt(-2) + ', W=0 → ' + countAt(0) + ', W=2 → ' + countAt(2));
  if (countAt(-2) === countAt(2)) throw new Error('срез не меняется — четвёртая ось не влияет');

  // телепорт к каждому осколку и подбор
  for (const sh of LV.shards){
    S.px = sh.x; S.pz = sh.z; S.w = sh.w; S.vy = 0; S.py = 0;
    for (let i=0;i<3;i++) frame(16.7);
  }
  step('собрано осколков: ' + S.collected + ' / 6');
  if (S.collected !== 6) throw new Error('осколки не собираются');

  // доходим до ядра
  S.px = LV.core.x; S.pz = LV.core.z; S.w = LV.core.w; S.py = 0;
  for (let i=0;i<3;i++) frame(16.7);
  step('финиш: mode=' + S.mode);
  if (S.mode !== 'win') throw new Error('финиш не срабатывает');

  // стресс: случайные клавиши + 4D-зрение + прыжки, 600 кадров
  elCache.get('againBtn').click();
  const pressKeys = ['w','a','s','d','q','e','shift',' ','arrowleft','arrowright'];
  let seed = 12345;
  const rnd = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff; };
  fire('keydown', { key: 'tab' });
  for (let i=0;i<600;i++){
    if (rnd() < 0.08){
      const k = pressKeys[(rnd()*pressKeys.length)|0];
      if (rnd() < 0.5) fire('keydown', { key: k }); else fire('keyup', { key: k });
    }
    fire('mousemove', { movementX: (rnd()-0.5)*40, movementY: (rnd()-0.5)*20 });
    frame(16.7);
  }
  fire('keyup', { key: 'tab' });
  const finite = [S.px, S.py, S.pz, S.w, S.yaw, S.pitch].every(v => Number.isFinite(v));
  step('стресс 600 кадров: координаты конечны = ' + finite + ', W=' + S.w.toFixed(2) + ', xray=' + S.xray.toFixed(2));
  if (!finite) throw new Error('появились NaN');
  if (!(calls.fill > 1000 && calls.stroke > 100)) throw new Error('рендер почти не рисует: ' + JSON.stringify(calls));

  step('всего вызовов отрисовки: fill=' + calls.fill + ', stroke=' + calls.stroke);
  console.log('\nРЕЗУЛЬТАТ: ВСЁ ПРОШЛО');
} catch (err){
  ok = false;
  console.log('\nОШИБКА: ' + err.message);
  console.log(err.stack.split('\n').slice(0, 6).join('\n'));
}
process.exit(ok ? 0 : 1);
