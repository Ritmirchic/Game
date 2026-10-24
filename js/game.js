/* ═══════════════════════════════════════════════════════════════
   СИНГУЛЯРНОСТЬ
   Ты — чёрная дыра. Поглощай материю, расти, переживи соперниц.

   Фишки:
   • Настоящее (ну, почти) гравитационное линзирование: фоновые
     звёзды искажаются и складываются в кольцо Эйнштейна вокруг
     каждой дыры, а за горизонтом событий — тень.
   • Физика: F = G·M/r² со смягчением, спиральная аккреция (орбиты
     гаснут сопротивлением среды), фотоны летят с постоянной
     скоростью, но их траектории изгибаются гравитацией.
   • Соперницы-сингулярности растут вместе с тобой и едят ту же
     еду. Слияние неизбежно — побеждает более массивная.
   • Чем ты массивнее, тем медленнее — сила и скорость в размене.
   • Звук слияния — «чирп» сигнала гравитационных волн GW150914.
   ═══════════════════════════════════════════════════════════════ */
(function(){
'use strict';

/* ────────────────────────── КОНФИГ ────────────────────────── */
const CFG = {
  G: 15000,               // гравитационная постоянная (игровая)
  SOFT_MIN: 40,           // минимальное смягчение r²
  START_MASS: 50,
  DEBRIS_CAP: 320,
  SPARK_CAP: 760,
  PHOTON_CAP: 140,
  SHIP_CAP: 4,
  PHOTON_C: 1050,         // скорость света, px/s
  LENS_K: 52,             // сила линзирования звёзд (экранная)
  PULSE_COST: 0.12,       // доля массы за импульс
  PULSE_CD: 4.5,
  PULSE_RADIUS: 1000,
  STAR_COUNT: 340,
  MAX_DT: 0.032,
  BG_FADE: 'rgba(4,5,13,0.34)',
};

const RIVAL_COLORS = ['#b48cff', '#56d8c9', '#ff7ea8'];
const SHIP_NAMES = ['Аврора','Восток','Кедр','Буран','Мир','Союз','Прогресс','Чайка','Рубин','Зенит','Стриж','Аргон','Восход','Фобос','Гелиос'];
const DEBRIS_COLORS = ['#737c9e', '#a3957c', '#9ef0ff'];

/* ────────────────────────── УТИЛИТЫ ────────────────────────── */
const TAU = Math.PI * 2;
const rnd = (a,b)=> a + Math.random()*(b-a);
const rndi = (a,b)=> Math.floor(rnd(a,b+1));
const clamp = (v,a,b)=> v<a?a:(v>b?b:v);
const lerp = (a,b,t)=> a+(b-a)*t;
const pick = arr => arr[Math.floor(Math.random()*arr.length)];
function segDist2(px,py, ax,ay, bx,by){
  const dx=bx-ax, dy=by-ay, l2=dx*dx+dy*dy;
  let t = l2>0 ? ((px-ax)*dx+(py-ay)*dy)/l2 : 0;
  t = clamp(t,0,1);
  const cx=ax+dx*t-px, cy=ay+dy*t-py;
  return cx*cx+cy*cy;
}
const fmt = n => Math.round(n).toLocaleString('ru-RU');
const fmtMass = m => {
  const s = m/50;
  if(s < 10) return s.toFixed(1) + ' M☉';
  if(s < 1000) return Math.round(s) + ' M☉';
  return (s/1000).toFixed(1) + 'K M☉';
};
const fmtTime = t => {
  const m = Math.floor(t/60), s = Math.floor(t%60);
  return m + ':' + (s<10?'0':'') + s;
};

const $ = id => document.getElementById(id);
const AUDIO = window.AUDIO || { init(){}, startAmbient(){}, setMass(){}, blip(){}, laser(){},
  absorbLight(){}, eatShip(){}, pulse(){}, chirp(){}, thump(){}, waveBell(){}, ui(){}, lose(){},
  toggleMute(){ return true; } };

/* ────────────────────────── DOM ────────────────────────── */
const canvas = $('game');
const ctx = canvas.getContext('2d');
const el = {
  hud:$('hud'), menu:$('menu'), pause:$('pause'), over:$('over'),
  barYou:$('barYou'), barRival:$('barRival'), massYou:$('massYou'), massRival:$('massRival'),
  rivalName:$('rivalName'), advantage:$('advantage'), pulseFill:$('pulseFill'), pulseWrap:$('pulseWrap'),
  score:$('score'), waveLabel:$('waveLabel'), bestLabel:$('bestLabel'),
  log:$('log'), banner:$('banner'), bannerText:$('bannerText'), bannerSub:$('bannerSub'),
  danger:$('danger'), title:$('title'), menuBest:$('menuBest'),
  stats:$('stats'), newRecord:$('newRecord'), overTitle:$('overTitle'), overSub:$('overSub'),
  startBtn:$('startBtn'), retryBtn:$('retryBtn'), resumeBtn:$('resumeBtn'), pulseBtn:$('pulseBtn'),
};

let W = 0, H = 0, DPR = 1;
function resize(){
  DPR = Math.min(2, window.devicePixelRatio || 1);
  W = window.innerWidth; H = window.innerHeight;
  canvas.width = Math.floor(W*DPR);
  canvas.height = Math.floor(H*DPR);
  makeStars();
}
window.addEventListener('resize', resize);

/* ────────────────────────── ЗВЁЗДЫ / ТУМАННОСТИ ────────────────────────── */
let stars = [], nebulae = [], blobCanvases = [];
const STAR_COLORS = ['#ffffff','#cfe0ff','#ffe9c4','#aac4ff','#ffd9d9'];

function makeStars(){
  stars = [];
  const n = CFG.STAR_COUNT;
  for(let i=0;i<n;i++){
    stars.push({
      x: Math.random()*(W+80)-40,
      y: Math.random()*(H+80)-40,
      r: rnd(0.4, 1.5) * (Math.random()<0.08 ? 1.8 : 1),
      b: rnd(0.25, 0.9),
      ph: rnd(0, TAU),
      sp: rnd(0.5, 2.5),
      c: pick(STAR_COLORS),
    });
  }
  nebulae = [];
  const cols = ['rgba(90,60,170,', 'rgba(40,80,170,', 'rgba(30,130,120,', 'rgba(150,60,120,'];
  for(let i=0;i<6;i++){
    nebulae.push({
      x: Math.random()*W, y: Math.random()*H,
      r: rnd(240, 620), a: rnd(0.020, 0.045),
      c: cols[i % cols.length], px: rnd(0.015, 0.045),
    });
  }
}

function makeBlob(color){
  const c = document.createElement('canvas');
  c.width = c.height = 256;
  const g = c.getContext('2d');
  const grad = g.createRadialGradient(128,128,0, 128,128,128);
  grad.addColorStop(0, color + '0.55)');
  grad.addColorStop(0.5, color + '0.16)');
  grad.addColorStop(1, color + '0)');
  g.fillStyle = grad;
  g.fillRect(0,0,256,256);
  return c;
}
function initBlobs(){
  blobCanvases = [
    makeBlob('rgba(90,60,170,'),
    makeBlob('rgba(40,80,170,'),
    makeBlob('rgba(30,130,120,'),
    makeBlob('rgba(150,60,120,'),
  ];
}

/* ────────────────────────── СОСТОЯНИЕ ────────────────────────── */
const G = {
  state: 'menu',        // menu | play | dying | over
  paused: false,
  t: 0,
  wave: 0,
  wavePending: true,
  waveTimer: 3.5,
  waveTime: 0,
  score: 0,
  best: 0,
  tsTarget: 1, ts: 1,   // timescale (slow-mo)
  slowmoT: 0,
  deathT: 0,
  winner: null,
  shake: 0,
  flash: 0,
  stats: null,
  flags: null,
  logQueue: [],
  logCooldown: 0,
  menuResetTimer: 0,
  hudTimer: 0,
  heartTimer: 0,
};

let holes = [], debris = [], ships = [], photons = [], sparks = [], waves = [], texts = [];
let player = null;
const cam = { x:0, y:0, zoom:1, tzoom:1, sx:0, sy:0 };
const input = { mx:0, my:0, hasMouse:false, keys:{}, wx:0, wy:0 };

/* рекорды */
try{
  const sv = JSON.parse(localStorage.getItem('singularity_best') || '{}');
  G.best = sv.score || 0;
}catch(e){ G.best = 0; }
function saveBest(){
  try{ localStorage.setItem('singularity_best', JSON.stringify({ score: Math.round(G.best) })); }catch(e){}
}

/* ────────────────────────── СУЩНОСТИ ────────────────────────── */
function holeRadius(M){ return 10 * Math.pow(M/50, 0.34); }

function makeHole(x, y, M, controller, color, name){
  return {
    x, y, vx:0, vy:0, M,
    R: holeRadius(M),
    controller,               // 'player' | 'ai'
    color,
    name: name || 'СОПЕРНИЦА',
    age: 0,
    // AI
    tx:x, ty:y, think: rnd(0, 0.2), mode:'forage', rage:false,
    // player
    pulseCd: 0,
    eatenRecently: 0,
  };
}

function spawnDebris(x, y, vx, vy, type){
  if(debris.length >= CFG.DEBRIS_CAP) return null;
  const t = type !== undefined ? type : (Math.random()<0.045 ? 2 : (Math.random()<0.45 ? 1 : 0));
  const m = t===0 ? rnd(0.15,0.5) : t===1 ? rnd(0.6,2.2) : rnd(3.5,6);
  const d = debris.length;
  const obj = {
    x, y, px:x, py:y, vx, vy, m, t,
    r: t===0 ? rnd(0.9,1.7) : t===1 ? rnd(1.8,3.4) : rnd(3,4.2),
    rot: rnd(0,TAU), spin: rnd(-2,2),
    tw: rnd(0,TAU),
  };
  if(d < CFG.DEBRIS_CAP){ debris.push(obj); return obj; }
  return null;
}

function spawnSpark(x, y, vx, vy, color, life){
  if(sparks.length >= CFG.SPARK_CAP) sparks.shift();
  sparks.push({ x, y, px:x, py:y, vx, vy, color,
    life: life||rnd(4,9), max: life||9, r: rnd(0.8,2.0) });
}

function spawnShip(){
  const focus = player || holes[0];
  if(ships.length >= CFG.SHIP_CAP || !focus) return;
  const a = rnd(0,TAU), d = rnd(750, 1300);
  const s = {
    x: focus.x + Math.cos(a)*d, y: focus.y + Math.sin(a)*d,
    vx:0, vy:0, angle:a, m: 8, r: 5,
    name: pick(SHIP_NAMES),
    fireCd: rnd(1.5, 4),
    wpT: 0, wx:0, wy:0,
    panic: 0,
  };
  ships.push(s);
}

function spawnPhoton(x, y, vx, vy){
  if(photons.length >= CFG.PHOTON_CAP) return;
  photons.push({ x, y, px:x, py:y, vx, vy, life: 5, bent:false });
}

function spawnShockwave(x, y, v, max, color, width){
  waves.push({ x, y, r: 4, v, max, color, width, a: 1 });
}

function spawnText(x, y, text, color, size){
  texts.push({ x, y, text, color, size: size||13, life: 1.1, max: 1.1 });
}

/* ────────────────────────── МИР ────────────────────────── */
function orbitalSpawn(cx, cy, centerMass, count, spinSign){
  const vCirc = d => Math.sqrt(CFG.G * centerMass / Math.max(60, d));
  // внутреннее кольцо: падает быстро — мгновенная аккреция с первых секунд
  const inner = Math.floor(count*0.28);
  for(let i=0;i<inner;i++){
    const a = rnd(0, TAU);
    const d = 140 + Math.pow(Math.random(), 0.8) * 320;
    const x = cx + Math.cos(a)*d, y = cy + Math.sin(a)*d;
    const v = vCirc(d) * rnd(0.5, 0.85);
    const dir = spinSign || 1;
    spawnDebris(x, y, -Math.sin(a)*v*dir + rnd(-15,15), Math.cos(a)*v*dir + rnd(-15,15));
  }
  // внешний диск: околокруговые орбиты, медленно спиралют внутрь
  for(let i=inner;i<count;i++){
    const a = rnd(0, TAU);
    const d = 420 + Math.pow(Math.random(), 0.7) * 1250;
    const x = cx + Math.cos(a)*d, y = cy + Math.sin(a)*d;
    const v = vCirc(d) * rnd(0.85, 1.12);
    const dir = spinSign || 1;
    spawnDebris(x, y, -Math.sin(a)*v*dir + rnd(-25,25), Math.cos(a)*v*dir + rnd(-25,25));
  }
}

function resetWorld(forPlayer){
  debris = []; ships = []; photons = []; sparks = []; waves = []; texts = [];
  holes = [];
  G.t = 0; G.score = 0; G.ts = 1; G.tsTarget = 1; G.slowmoT = 0; G.shake = 0; G.flash = 0;
  G.wave = 0; G.wavePending = true; G.waveTimer = 3.5; G.waveTime = 0;
  G.stats = { mass:0, ships:0, photons:0, merges:0, pulses:0, time:0 };
  G.flags = {};
  G.logQueue = []; G.logCooldown = 0;
  el.log.innerHTML = '';

  if(forPlayer){
    player = makeHole(0, 0, CFG.START_MASS, 'player', '#ffc46b', 'ТЫ');
    holes.push(player);
    orbitalSpawn(0, 0, CFG.START_MASS, 260, 1);
    cam.x = 0; cam.y = 0; cam.zoom = 1; cam.tzoom = 1;
    input.wx = 120; input.wy = 0;
  }else{
    // attract-режим для меню: две AI-дыры танцуют и сливаются
    player = null;
    const h1 = makeHole(-500, 60, 140, 'ai', '#ffc46b', 'α');
    const h2 = makeHole(520, -80, 110, 'ai', RIVAL_COLORS[0], 'β');
    holes.push(h1, h2);
    orbitalSpawn(0, 0, 250, 240, 1);
    cam.x = 0; cam.y = 0; cam.zoom = 0.6; cam.tzoom = 0.6;
    G.menuResetTimer = 4.5;   // дать победителю слияния покрасоваться
  }
}

function startGame(){
  AUDIO.init();
  AUDIO.startAmbient();
  AUDIO.ui();
  resetWorld(true);
  G.state = 'play';
  G.winner = null;
  G.playerMassAtDeath = 0;
  el.menu.classList.add('hidden');
  setTimeout(()=>el.menu.classList.add('gone'), 800);
  el.over.classList.add('hidden');
  el.hud.classList.remove('hidden');
  showBanner('ТЫ — СИНГУЛЯРНОСТЬ', 'пыль уже падает');
  queueLog('Пустота искривляется вокруг тебя. Начни с пыли — она уже падает.');
}

/* ────────────────────────── ЛОГ / БАННЕРЫ ────────────────────────── */
function queueLog(text, alert){
  if(G.logQueue.length < 4) G.logQueue.push({ text, alert });
}
function flushLog(dt){
  G.logCooldown -= dt;
  if(G.logCooldown > 0 || G.logQueue.length === 0) return;
  const item = G.logQueue.shift();
  G.logCooldown = 2.2;
  const div = document.createElement('div');
  div.className = 'logline' + (item.alert ? ' alert' : '');
  div.textContent = item.text;
  el.log.prepend(div);
  while(el.log.children.length > 4) el.log.removeChild(el.log.lastChild);
  setTimeout(()=>div.classList.add('fade'), 5200);
  setTimeout(()=>{ if(div.parentNode) div.parentNode.removeChild(div); }, 6600);
}
function showBanner(text, sub){
  el.bannerText.textContent = text;
  el.bannerSub.textContent = sub || '';
  el.banner.classList.remove('show');
  void el.banner.offsetWidth;
  el.banner.classList.add('show');
}

/* ────────────────────────── ПОГЛОЩЕНИЕ ────────────────────────── */
function eat(h, m, x, y, opts){
  h.M += m;
  h.R = holeRadius(h.M);
  h.eatenRecently = 0.15;
  if(h.controller === 'player'){
    G.stats.mass += m;
    const mult = 1 + 0.25*(G.wave-1);
    G.score += m * 10 * mult * (opts && opts.mult ? opts.mult : 1);
    if(opts && opts.text) spawnText(x, y, opts.text, opts.color || '#ffe9c4', opts.size || 13);
  }
}

function burstSparks(x, y, vx, vy, color, n, speed){
  for(let i=0;i<n;i++){
    const a = rnd(0,TAU), s = rnd(0.3,1)*speed;
    spawnSpark(x, y, vx*0.6 + Math.cos(a)*s, vy*0.6 + Math.sin(a)*s, color, rnd(3,7));
  }
}

/* ────────────────────────── СЛИЯНИЕ ДЫР ────────────────────────── */
function mergeHoles(a, b){
  const win = a.M >= b.M ? a : b;
  const lose = win === a ? b : a;
  const mx = (a.x*a.M + b.x*b.M)/(a.M+b.M);
  const my = (a.y*a.M + b.y*b.M)/(a.M+b.M);

  win.M += lose.M * 0.9;
  win.R = holeRadius(win.M);
  holes.splice(holes.indexOf(lose), 1);

  // гравитационные волны!
  spawnShockwave(mx, my, 1500, 1600, '#ffffff', 7);
  spawnShockwave(mx, my, 1100, 1300, win.color, 5);
  spawnShockwave(mx, my, 800, 1000, lose.color, 4);
  burstSparks(mx, my, 0, 0, '#ffe9c4', 70, 420);
  G.flash = 0.85;

  const camDist = Math.hypot(mx - cam.x, my - cam.y);
  G.shake = Math.min(30, 1500 / (60 + camDist));

  const playerInvolved = (a.controller === 'player' || b.controller === 'player');

  if(playerInvolved){
    AUDIO.chirp(true);
  }else{
    AUDIO.chirp(false);
  }

  if(lose.controller === 'player'){
    /* ── игрок проиграл: кинематографичная смерть ── */
    G.state = 'dying';
    G.deathT = 2.0;
    G.winner = win;
    G.playerMassAtDeath = lose.M;
    G.tsTarget = 0.18;
    AUDIO.lose();
    spawnText(mx, my, 'ПОГЛОЩЁН', '#ff5d6c', 26);
    player = null;   // мир живёт дальше — уже без тебя
  }else if(win.controller === 'player'){
    /* ── игрок победил ── */
    G.stats.merges++;
    const mult = 1 + 0.25*(G.wave-1);
    G.score += 750 * mult + lose.M * 5 * mult;
    G.tsTarget = 0.3;
    G.slowmoT = 1.0;
    spawnText(mx, my, '+' + fmt(750*mult) + ' СЛИЯНИЕ', '#ffd88a', 20);
    if(!G.flags.firstMerge){
      G.flags.firstMerge = true;
      queueLog('Слияние! Пространство рябило — детекторы зафиксировали гравитационные волны.');
    }else{
      queueLog('Ещё одна сингулярность стала частью тебя. ' + fmtMass(win.M) + '.');
    }
    G.wavePending = true;
    G.waveTimer = 3.0;
  }else if(G.state === 'play'){
    queueLog('Соперницы слились без тебя. Победительница стала тяжелее.');
  }
}

/* ────────────────────────── ИМПУЛЬС ────────────────────────── */
function doPulse(){
  if(!player || G.state !== 'play' || player.pulseCd > 0 || player.M < 58) return;
  const cost = player.M * CFG.PULSE_COST;
  player.M -= cost;
  player.R = holeRadius(player.M);
  player.pulseCd = CFG.PULSE_CD;
  G.stats.pulses++;

  spawnShockwave(player.x, player.y, 1500, CFG.PULSE_RADIUS, '#6ee7ff', 6);
  burstSparks(player.x, player.y, 0, 0, '#6ee7ff', 36, 560);
  G.shake = Math.max(G.shake, 9);
  AUDIO.pulse();

  const R = CFG.PULSE_RADIUS;
  const push = (e, K)=>{
    const dx = e.x - player.x, dy = e.y - player.y;
    const d = Math.hypot(dx, dy) || 1;
    if(d > R) return;
    const f = (1 - d/R);
    e.vx += dx/d * K * f;
    e.vy += dy/d * K * f;
  };
  debris.forEach(e=>push(e, 620));
  sparks.forEach(e=>push(e, 620));
  ships.forEach(e=>push(e, 520));
  photons.forEach(e=>{
    const dx = e.x - player.x, dy = e.y - player.y;
    const d = Math.hypot(dx, dy) || 1;
    if(d > R){ return; }
    e.vx = dx/d * CFG.PHOTON_C; e.vy = dy/d * CFG.PHOTON_C;
  });
  holes.forEach(h=>{
    if(h === player) return;
    const dx = h.x - player.x, dy = h.y - player.y;
    const d = Math.hypot(dx, dy) || 1;
    if(d > R) return;
    const f = (1 - d/R);
    const imp = f * (200 + 420*Math.sqrt(cost / h.M));
    h.vx += dx/d * imp; h.vy += dy/d * imp;
  });

  if(!G.flags.firstPulse){
    G.flags.firstPulse = true;
    queueLog('Импульс оттолкнул всё вокруг. Даже твой обед. −12% массы.');
  }
}

/* ────────────────────────── ВОЛНЫ ────────────────────────── */
function spawnWave(){
  G.wave++;
  G.waveTime = 0;
  const count = Math.min(3, 1 + Math.floor((G.wave-1)/2));
  const massK = clamp(0.55 + 0.04*G.wave, 0.55, 0.95);
  for(let i=0;i<count;i++){
    const a = rnd(0,TAU) + i*TAU/count;
    const d = rnd(950, 1350);
    const M = player.M * massK * rnd(0.92, 1.08);
    const h = makeHole(player.x + Math.cos(a)*d, player.y + Math.sin(a)*d, M,
      'ai', RIVAL_COLORS[i % RIVAL_COLORS.length],
      count>1 ? 'СОПЕРНИЦА ' + 'αβγ'[i] : 'СОПЕРНИЦА');
    holes.push(h);
    spawnShockwave(h.x, h.y, 700, 420, h.color, 4);
  }
  AUDIO.waveBell();
  showBanner('ВОЛНА ' + G.wave, count>1 ? count + ' сингулярности на подходе' : 'она идёт за твоей материей');
  if(G.wave === 1) queueLog('⚠ Вторая сингулярность вошла в систему. Она ест ту же пыль, что и ты.', true);
  if(G.wave === 3 && !G.flags.multi){ G.flags.multi = true; queueLog('Их несколько. Хорошая новость: они сожрут и друг друга.'); }
  if(G.wave === 5 && !G.flags.w5){ G.flags.w5 = true; queueLog('Система умирает. Ты — причина. Продолжай.'); }
}

/* ────────────────────────── ИИ СОПЕРНИЦ ────────────────────────── */
function updateAI(h, dt){
  h.think -= dt;
  const me = h;

  // угрозы: кто тяжелее меня
  let threat = null, threatD = Infinity;
  for(const o of holes){
    if(o === me) continue;
    const d = Math.hypot(o.x-me.x, o.y-me.y);
    if(o.M > me.M*1.12 && d < 950 && d < threatD){ threat = o; threatD = d; }
  }
  // цель охоты: кто легче меня и близко
  let prey = null, preyD = Infinity;
  for(const o of holes){
    if(o === me) continue;
    const d = Math.hypot(o.x-me.x, o.y-me.y);
    if(me.M > o.M*1.12 && d < 1400 && d < preyD){ prey = o; preyD = d; }
  }

  const farFromAction = player && Math.hypot(player.x-me.x, player.y-me.y) > 3000;

  if(h.think <= 0 || farFromAction){
    h.think = 0.25 + Math.random()*0.15;

    if(farFromAction){
      h.mode = 'return';
      h.tx = player.x + rnd(-300,300); h.ty = player.y + rnd(-300,300);
    }else if(threat && !h.rage){
      h.mode = 'flee';
      const dx = me.x - threat.x, dy = me.y - threat.y;
      const d = Math.hypot(dx,dy)||1;
      // удирать по касательной — так труднее догнать
      const tx = -dy/d, ty = dx/d;
      h.tx = me.x + dx/d*500 + tx*300*(Math.random()<0.5?-1:1);
      h.ty = me.y + dy/d*500 + ty*300*(Math.random()<0.5?-1:1);
    }else if(prey){
      h.mode = 'hunt';
      h.tx = prey.x + prey.vx*0.5;
      h.ty = prey.y + prey.vy*0.5;
    }else{
      h.mode = 'forage';
      // лучшая еда: ценность = масса / дистанция, с поправкой «держи action рядом с игроком»
      let best = null, bestV = 0;
      const n = debris.length;
      const samples = Math.min(40, n);
      for(let i=0;i<samples;i++){
        const e = debris[Math.floor(Math.random()*n)];
        if(!e) continue;
        const dS = Math.hypot(e.x-me.x, e.y-me.y) + 60;
        const dP = player ? Math.hypot(e.x-player.x, e.y-player.y) : 0;
        const v = e.m / Math.pow(dS, 1.15) / (1 + dP/3000);
        if(v > bestV){ bestV = v; best = e; }
      }
      if(best){ h.tx = best.x; h.ty = best.y; }
      else{
        const a = rnd(0,TAU), d = rnd(350, 900);
        h.tx = (player?player.x:0) + Math.cos(a)*d;
        h.ty = (player?player.y:0) + Math.sin(a)*d;
      }
    }
  }

  // ярость: если игрок тянет время — соперницы идут на таран
  if(h.rage || G.waveTime > 45){
    if(!h.rage){
      h.rage = true;
      if(player) queueLog('Она устала ждать. Она идёт.', true);
    }
    h.mode = 'hunt';
    if(player){ h.tx = player.x + player.vx*0.6; h.ty = player.y + player.vy*0.6; }
    h.think = Math.min(h.think, 0.3);
  }

  steer(h, h.tx, h.ty, dt, 0.86, h.rage ? 1.14 : 1);
}

/* общее руление к точке: тяжело — значит медленно и инертно */
function steer(h, tx, ty, dt, speedK, extra){
  const dx = tx - h.x, dy = ty - h.y;
  const d = Math.hypot(dx, dy) || 1;
  const maxSpeed = clamp(430 * Math.pow(50/h.M, 0.18), 100, 560) * (speedK||1) * (extra||1);
  const want = Math.min(maxSpeed, d * 2.2);
  const resp = clamp(2600 / Math.pow(h.M, 0.55), 0.45, 7) * 0.9;
  const dvx = dx/d * want, dvy = dy/d * want;
  h.vx += (dvx - h.vx) * Math.min(1, resp * dt);
  h.vy += (dvy - h.vy) * Math.min(1, resp * dt);
}

/* ────────────────────────── ФИЗИКА ────────────────────────── */
function gravityAccel(x, y, srcM, srcR, sx, sy){
  const dx = sx - x, dy = sy - y;
  const d2 = dx*dx + dy*dy;
  const soft2 = Math.max(srcR, CFG.SOFT_MIN);
  const denom = d2 + soft2*soft2;
  const a = CFG.G * srcM / denom;
  const d = Math.sqrt(d2) || 1;
  return [a*dx/d, a*dy/d];
}

function updateWorld(dt){
  G.t += dt;
  if(G.state === 'play') G.stats.time += dt;

  /* ── игрок: цель ── */
  if(player && G.state === 'play'){
    let kx = 0, ky = 0;
    const k = input.keys;
    if(k['a'] || k['ф'] || k['arrowleft']) kx -= 1;
    if(k['d'] || k['в'] || k['arrowright']) kx += 1;
    if(k['w'] || k['ц'] || k['arrowup']) ky -= 1;
    if(k['s'] || k['ы'] || k['arrowdown']) ky += 1;
    if(kx || ky){
      const l = Math.hypot(kx,ky);
      input.wx = player.x + kx/l * 340;
      input.wy = player.y + ky/l * 340;
    }
    steer(player, input.wx, input.wy, dt, 1, 1);
    player.pulseCd = Math.max(0, player.pulseCd - dt);
  }

  /* ── AI дыр ── */
  for(const h of holes){
    h.age += dt;
    if(h.controller === 'ai') updateAI(h, dt);
  }

  /* ── взаимная гравитация дыр + интеграция ── */
  for(let i=0;i<holes.length;i++){
    const a = holes[i];
    for(let j=i+1;j<holes.length;j++){
      const b = holes[j];
      const dx = b.x-a.x, dy = b.y-a.y;
      const d2 = dx*dx+dy*dy;
      const d = Math.sqrt(d2) || 1;
      const soft = Math.max(a.R, b.R, CFG.SOFT_MIN);
      const f = CFG.G / (d2 + soft*soft);
      a.vx += dx/d * f * b.M * dt; a.vy += dy/d * f * b.M * dt;
      b.vx -= dx/d * f * a.M * dt; b.vy -= dy/d * f * a.M * dt;
    }
  }
  for(const h of holes){
    const sp = Math.hypot(h.vx, h.vy);
    if(sp > 900){ h.vx *= 900/sp; h.vy *= 900/sp; }
    h.x += h.vx*dt; h.y += h.vy*dt;
  }

  /* ── проверки слияния ── */
  outer:
  for(let i=0;i<holes.length;i++){
    for(let j=i+1;j<holes.length;j++){
      const a = holes[i], b = holes[j];
      const d = Math.hypot(b.x-a.x, b.y-a.y);
      if(d < (a.R + b.R) * 0.85 && a.age > 0.5 && b.age > 0.5){
        mergeHoles(a, b);
        break outer;
      }
    }
  }

  const dragD = Math.pow(0.98, dt*60);    // сопротивление среды: орбиты гаснут → спиральная аккреция
  const dragS = Math.pow(0.988, dt*60);

  /* ── пыль ── */
  for(let i=debris.length-1;i>=0;i--){
    const e = debris[i];
    e.px = e.x; e.py = e.y;
    for(const h of holes){
      const [ax, ay] = gravityAccel(e.x, e.y, h.M, h.R, h.x, h.y);
      e.vx += ax*dt; e.vy += ay*dt;
    }
    e.vx *= dragD; e.vy *= dragD;
    const sp = Math.hypot(e.vx, e.vy);
    if(sp > 1300){ e.vx *= 1300/sp; e.vy *= 1300/sp; }
    e.x += e.vx*dt; e.y += e.vy*dt;
    e.rot += e.spin*dt; e.tw += dt*3;

    // поглощение (проверка по отрезку — быстрые частицы не туннелируют)
    let eaten = null;
    for(const h of holes){
      const rr = h.R*1.3 + e.r;
      if(segDist2(h.x, h.y, e.px, e.py, e.x, e.y) < rr*rr){ eaten = h; break; }
    }
    if(eaten){
      eat(eaten, e.m, e.x, e.y, e.m >= 2 ? { text:'+' + e.m.toFixed(1), size:12 } : null);
      burstSparks(e.x, e.y, e.vx, e.vy, DEBRIS_COLORS[e.t], 2, 60);
      if(eaten.controller === 'player'){
        AUDIO.blip(e.m, clamp((e.x-cam.x)*cam.zoom/(W/2), -1, 1));
        if(!G.flags.firstEat && G.stats.mass > 6){
          G.flags.firstEat = true;
          queueLog('Первая материя за горизонтом. Она теперь — ты.');
        }
      }
      debris.splice(i,1);
      continue;
    }
    // улетела далеко — перерожаем ближе
    if(player){
      const d = Math.hypot(e.x - cam.x, e.y - cam.y);
      if(d > 3400){ debris.splice(i,1); }
    }
  }

  /* ── корабли ── */
  for(let i=ships.length-1;i>=0;i--){
    const s = ships[i];
    s.px = s.x; s.py = s.y;

    // ближайшая дыра
    let nh = null, nd = Infinity;
    for(const h of holes){
      const d = Math.hypot(h.x-s.x, h.y-s.y);
      if(d < nd){ nd = d; nh = h; }
    }

    s.panic = clamp(1 - (nd - 150)/500, 0, 1);

    if(nh && nd < 560){
      // бежим!
      const dx = s.x-nh.x, dy = s.y-nh.y, d = nd||1;
      const want = 300;
      s.vx += (dx/d*want - s.vx) * Math.min(1, 2.4*dt);
      s.vy += (dy/d*want - s.vy) * Math.min(1, 2.4*dt);
      // стрельба от страха
      s.fireCd -= dt;
      if(s.fireCd <= 0 && nd < 460){
        s.fireCd = rnd(2.2, 4.5);
        const lead = nd / CFG.PHOTON_C;
        const tx = nh.x + nh.vx*lead, ty = nh.y + nh.vy*lead;
        const a = Math.atan2(ty-s.y, tx-s.x) + rnd(-0.06, 0.06);
        spawnPhoton(s.x + Math.cos(a)*8, s.y + Math.sin(a)*8,
          Math.cos(a)*CFG.PHOTON_C, Math.sin(a)*CFG.PHOTON_C);
        AUDIO.laser(clamp((s.x-cam.x)*cam.zoom/(W/2), -1, 1));
      }
    }else{
      // патруль: блуждаем возле игрока
      s.wpT -= dt;
      if(s.wpT <= 0){
        s.wpT = rnd(3.5, 7);
        const c = player || holes[0] || {x:0,y:0};
        const a = rnd(0,TAU), d = rnd(500, 1100);
        s.wx = c.x + Math.cos(a)*d; s.wy = c.y + Math.sin(a)*d;
      }
      const dx = s.wx-s.x, dy = s.wy-s.y, d = Math.hypot(dx,dy)||1;
      s.vx += (dx/d*170 - s.vx) * Math.min(1, 1.2*dt);
      s.vy += (dy/d*170 - s.vy) * Math.min(1, 1.2*dt);
      s.fireCd = Math.min(s.fireCd, 1.5);
    }

    // гравитация на корабли тоже действует — они смертны
    for(const h of holes){
      const [ax, ay] = gravityAccel(s.x, s.y, h.M*0.9, h.R, h.x, h.y);
      s.vx += ax*dt; s.vy += ay*dt;
    }
    const sp = Math.hypot(s.vx, s.vy);
    if(sp > 700){ s.vx *= 700/sp; s.vy *= 700/sp; }
    s.x += s.vx*dt; s.y += s.vy*dt;
    if(sp > 8) s.angle = Math.atan2(s.vy, s.vx);

    // съеден?
    let eaten = null;
    for(const h of holes){
      const rr = h.R*1.15 + s.r;
      if(segDist2(h.x, h.y, s.px, s.py, s.x, s.y) < rr*rr){ eaten = h; break; }
    }
    if(eaten){
      eat(eaten, s.m, s.x, s.y, eaten.controller==='player' ? { text:'ФРЕГАТ +'+s.name, color:'#ffd88a', size:14 } : null);
      burstSparks(s.x, s.y, s.vx, s.vy, '#cfe0ff', 14, 180);
      spawnShockwave(s.x, s.y, 300, 120, '#cfe0ff', 2);
      if(eaten.controller === 'player'){
        AUDIO.eatShip(clamp((s.x-cam.x)*cam.zoom/(W/2), -1, 1));
        G.stats.ships++;
        G.score += 60;
        queueLog('Фрегат «' + s.name + '» не выходит на связь.');
        if(G.stats.ships === 3 && !G.flags.ships3){
          G.flags.ships3 = true;
          queueLog('«Оно ест корабли...» — последняя запись бортового журнала.');
        }
      }
      ships.splice(i,1);
      continue;
    }
    if(Math.hypot(s.x - cam.x, s.y - cam.y) > 3600){ ships.splice(i,1); }
  }

  /* ── фотоны: свет искривляется! ── */
  for(let i=photons.length-1;i>=0;i--){
    const p = photons[i];
    p.px = p.x; p.py = p.y;
    for(const h of holes){
      const [ax, ay] = gravityAccel(p.x, p.y, h.M * 1.25, h.R*0.7, h.x, h.y);
      p.vx += ax*dt; p.vy += ay*dt;
      const d = Math.hypot(p.x-h.x, p.y-h.y);
      if(!p.bent && d < h.R*3.2 && d > h.R*1.3){
        p.bent = true;
        if(h.controller === 'player' && !G.flags.bent){
          G.flags.bent = true;
          queueLog('«Командир! Луч огибает её! Свет летит по геодезической!»');
        }
      }
    }
    // скорость света постоянна — меняется только направление
    const sp = Math.hypot(p.vx, p.vy) || 1;
    p.vx *= CFG.PHOTON_C/sp; p.vy *= CFG.PHOTON_C/sp;
    p.x += p.vx*dt; p.y += p.vy*dt;
    p.life -= dt;

    let eaten = null;
    for(const h of holes){
      const rr = h.R*1.06;
      if(segDist2(h.x, h.y, p.px, p.py, p.x, p.y) < rr*rr){ eaten = h; break; }
    }
    if(eaten){
      if(eaten.controller === 'player'){
        eat(eaten, 0.05, p.x, p.y);
        G.stats.photons++;
        AUDIO.absorbLight(clamp((p.x-cam.x)*cam.zoom/(W/2), -1, 1));
        if(G.stats.photons === 5 && !G.flags.ph5){
          G.flags.ph5 = true;
          queueLog('Даже свет не уходит. Особенно свет.');
        }
      }
      burstSparks(p.x, p.y, 0, 0, '#a6ffcb', 3, 40);
      photons.splice(i,1);
      continue;
    }
    if(p.life <= 0 || Math.hypot(p.x-cam.x, p.y-cam.y) > 3800){ photons.splice(i,1); }
  }

  /* ── искры аккреционного диска ── */
  for(let i=sparks.length-1;i>=0;i--){
    const s = sparks[i];
    s.px = s.x; s.py = s.y;
    for(const h of holes){
      const [ax, ay] = gravityAccel(s.x, s.y, h.M, h.R, h.x, h.y);
      s.vx += ax*dt; s.vy += ay*dt;
    }
    s.vx *= dragS; s.vy *= dragS;
    const sp = Math.hypot(s.vx, s.vy);
    if(sp > 1400){ s.vx *= 1400/sp; s.vy *= 1400/sp; }
    s.x += s.vx*dt; s.y += s.vy*dt;
    s.life -= dt;

    let gone = s.life <= 0;
    if(!gone){
      for(const h of holes){
        if(Math.hypot(s.x-h.x, s.y-h.y) < h.R*0.95){ gone = true; break; }
      }
    }
    if(gone || Math.hypot(s.x-cam.x, s.y-cam.y) > 3800){ sparks.splice(i,1); }
  }

  /* ── ударные волны ── */
  for(let i=waves.length-1;i>=0;i--){
    const w = waves[i];
    w.r += w.v*dt;
    w.a = 1 - w.r/w.max;
    if(w.a <= 0) waves.splice(i,1);
  }

  /* ── всплывающий текст ── */
  for(let i=texts.length-1;i>=0;i--){
    const t = texts[i];
    t.life -= dt;
    t.y -= 28*dt;
    if(t.life <= 0) texts.splice(i,1);
  }

  /* ── спавнеры ── */
  const focus = player || holes[0];
  if(focus){
    // пыль постоянно пополняется: внешний поток + «кометы» ближе к действию
    if(debris.length < CFG.DEBRIS_CAP - 10 && Math.random() < dt*42){
      const a = rnd(0,TAU);
      const near = Math.random() < 0.35;
      const d = near ? rnd(650, 1300) : rnd(1500, 2300);
      const x = focus.x + Math.cos(a)*d, y = focus.y + Math.sin(a)*d;
      const Mc = player ? player.M : 200;
      const vOrb = Math.sqrt(CFG.G * Mc / d) * rnd(0.8, 1.15);
      spawnDebris(x, y,
        -Math.sin(a)*vOrb - Math.cos(a)*rnd(20,60),
         Math.cos(a)*vOrb - Math.sin(a)*rnd(20,60));
    }
    if(G.state !== 'menu'){
      if(ships.length < 3 && Math.random() < dt*0.5) spawnShip();
    }else{
      if(ships.length < 2 && Math.random() < dt*0.3) spawnShip();
    }
  }

  /* ── логика волн ── */
  if(G.state === 'play'){
    if(G.wavePending){
      G.waveTimer -= dt;
      if(G.waveTimer <= 0){ G.wavePending = false; spawnWave(); }
    }else{
      G.waveTime += dt;
      const rivals = holes.filter(h=>h.controller === 'ai');
      if(rivals.length === 0){
        G.wavePending = true;
        G.waveTimer = 2.8;
        G.score += 250 * G.wave;
        spawnText(player.x, player.y - player.R - 40, 'ВОЛНА ОЧИЩЕНА +' + (250*G.wave), '#7dffb2', 16);
      }else{
        // драматургия: комментарии о перевесе
        const r = rivals[0];
        const d = Math.hypot(r.x-player.x, r.y-player.y);
        if(r.M > player.M*1.15 && d < 900){
          if(!G.flags.overtaken){ G.flags.overtaken = true; queueLog('Она тяжелее тебя. Беги или расти быстрее.', true); }
        }else if(G.flags.overtaken && player.M > r.M*1.1){
          G.flags.overtaken = false;
          G.flags.regained = true;
          queueLog('Снова ты тяжелее. Она это чувствует.');
        }
        // сердцебиение при опасности
        if(r.M > player.M && d < 850){
          G.heartTimer -= dt;
          if(G.heartTimer <= 0){
            G.heartTimer = 0.95;
            AUDIO.thump(clamp(1 - d/850, 0.2, 1));
          }
        }
      }
    }
    // milestones
    if(!G.flags.m500 && player.M >= 500){ G.flags.m500 = true; queueLog('Твоя тень видна с соседних орбит. ' + fmtMass(player.M) + '.'); }
    if(!G.flags.m2000 && player.M >= 2000){ G.flags.m2000 = true; queueLog('Ты искривляешь время. Часы рядом с тобой идут медленнее.'); }

    AUDIO.setMass(player.M);
  }

  /* ── slow-mo ── */
  if(G.slowmoT > 0){
    G.slowmoT -= dt;
    if(G.slowmoT <= 0) G.tsTarget = 1;
  }
  G.ts = lerp(G.ts, G.tsTarget, Math.min(1, dt*4));

  /* ── смерть игрока ── */
  if(G.state === 'dying'){
    G.deathT -= dt;
    if(G.deathT <= 0) showOver();
  }

  /* ── меню: респаун танцующих дыр после их слияния ── */
  if(G.state === 'menu' && holes.length < 2){
    G.menuResetTimer -= dt;
    if(G.menuResetTimer <= 0){
      resetWorld(false);
    }
  }else if(G.state === 'menu'){
    G.menuResetTimer = Math.max(G.menuResetTimer, 4.5);
  }

  flushLog(dt);
  G.flash = Math.max(0, G.flash - dt*2.2);
  G.shake *= Math.pow(0.0025, dt);
}

/* ────────────────────────── КАМЕРА ────────────────────────── */
function updateCamera(dt){
  let tx, ty, tz;
  if(G.state === 'menu'){
    if(holes.length >= 2){
      const a = holes[0], b = holes[1];
      tx = (a.x+b.x)/2; ty = (a.y+b.y)/2;
      const d = Math.hypot(a.x-b.x, a.y-b.y);
      tz = clamp(Math.min(W,H) / (d + 750), 0.22, 0.95);
    }else if(holes.length === 1){
      tx = holes[0].x; ty = holes[0].y; tz = 0.75;
    }else{ tx = 0; ty = 0; tz = 0.6; }
  }else if(G.state === 'dying' && G.winner){
    tx = G.winner.x; ty = G.winner.y;
    tz = clamp(Math.pow(55/G.winner.M, 0.22), 0.3, 1.2);
  }else if(player){
    tx = player.x + player.vx*0.18;
    ty = player.y + player.vy*0.18;
    tz = clamp(Math.pow(55/player.M, 0.22), 0.3, 1.25);
  }else{ tx = cam.x; ty = cam.y; tz = cam.zoom; }

  const k = Math.min(1, dt * (G.state==='dying' ? 1.2 : 2.2));
  cam.x = lerp(cam.x, tx, k);
  cam.y = lerp(cam.y, ty, k);
  cam.zoom = lerp(cam.zoom, tz, Math.min(1, dt*1.4));

  if(G.shake > 0.3){
    cam.sx = rnd(-G.shake, G.shake);
    cam.sy = rnd(-G.shake, G.shake);
  }else{ cam.sx = 0; cam.sy = 0; }
}

/* ────────────────────────── РЕНДЕР ────────────────────────── */
function worldToScreenX(x){ return (x - cam.x)*cam.zoom + W/2 + cam.sx; }
function worldToScreenY(y){ return (y - cam.y)*cam.zoom + H/2 + cam.sy; }

function render(){
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);

  // motion blur: полупрозрачный фон вместо полной очистки → шлейфы
  ctx.globalCompositeOperation = 'source-over';
  ctx.fillStyle = CFG.BG_FADE;
  ctx.fillRect(0, 0, W, H);

  drawNebulae();
  drawStars();

  // мировая система координат
  ctx.translate(W/2 + cam.sx, H/2 + cam.sy);
  ctx.scale(cam.zoom, cam.zoom);
  ctx.translate(-cam.x, -cam.y);

  drawDebris();
  drawShips();
  drawSparks();
  drawPhotons();
  for(const h of holes) drawHole(h);
  drawShockwaves();

  // экранная система
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  drawTexts();
  drawIndicators();

  if(G.flash > 0.01){
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = 'rgba(255,244,224,' + (G.flash*0.55).toFixed(3) + ')';
    ctx.fillRect(0,0,W,H);
  }
  ctx.globalCompositeOperation = 'source-over';
}

function drawNebulae(){
  ctx.globalCompositeOperation = 'lighter';
  nebulae.forEach((n, i)=>{
    const blob = blobCanvases[i % blobCanvases.length];
    const px = ((n.x - cam.x*n.px) % (W+600) + W+600) % (W+600) - 300;
    const py = ((n.y - cam.y*n.px) % (H+600) + H+600) % (H+600) - 300;
    const r = n.r * (0.7 + 0.5*cam.zoom);
    ctx.globalAlpha = n.a;
    ctx.drawImage(blob, px-r, py-r, r*2, r*2);
  });
  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'source-over';
}

function drawStars(){
  ctx.globalCompositeOperation = 'lighter';
  // экранные позиции дыр для линзирования
  const hs = holes.map(h=>({
    sx: worldToScreenX(h.x), sy: worldToScreenY(h.y),
    Rz: h.R * cam.zoom, Mz: h.M * cam.zoom, color: h.color,
    a: clamp(h.age/0.8, 0, 1),
  }));

  for(const s of stars){
    let sx = ((s.x - cam.x*0.05*cam.zoom) % (W+80) + W+80) % (W+80) - 40;
    let sy = ((s.y - cam.y*0.05*cam.zoom) % (H+80) + H+80) % (H+80) - 40;
    let b = s.b * (0.72 + 0.28*Math.sin(G.t*s.sp + s.ph));
    let r = s.r;
    let hidden = false;

    for(const h of hs){
      const dx = sx - h.sx, dy = sy - h.sy;
      const d2 = dx*dx + dy*dy;
      const d = Math.sqrt(d2) || 0.001;
      const shadow = h.Rz * 1.10;
      if(d < shadow){ hidden = true; break; }          // тень фотонной сферы
      // отклонение света: d = k·M/r² — звезды «растекаются» в кольцо Эйнштейна
      let defl = CFG.LENS_K * h.Mz / (d2 + 30);
      defl = Math.min(defl, h.Rz*4);
      const nx = dx/d, ny = dy/d;
      sx += nx*defl; sy += ny*defl;
      const mag = 1 + Math.min(2.2, defl / (h.Rz*0.55 + 1));
      b *= mag; r *= (1 + (mag-1)*0.35);
      if(d < h.Rz*2.4) b *= 1.25;                      // свечение кольца
    }
    if(hidden) continue;

    ctx.globalAlpha = Math.min(1, b) * 0.9;
    ctx.fillStyle = s.c;
    ctx.beginPath();
    ctx.arc(sx, sy, Math.min(4.5, r), 0, TAU);
    ctx.fill();
  }

  // мягкое эйнштейновское гало вокруг каждой дыры
  for(const h of hs){
    if(h.Rz < 1) continue;
    ctx.globalAlpha = 0.07 * h.a;
    ctx.strokeStyle = '#fff6e0';
    ctx.lineWidth = Math.max(2, h.Rz*0.55);
    ctx.beginPath();
    ctx.arc(h.sx, h.sy, h.Rz*1.6, 0, TAU);
    ctx.stroke();
  }

  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'source-over';
}

function drawDebris(){
  ctx.globalCompositeOperation = 'lighter';
  for(const e of debris){
    const c = DEBRIS_COLORS[e.t];
    // штрих по скорости
    ctx.strokeStyle = c;
    ctx.globalAlpha = e.t===2 ? 0.75 : 0.4;
    ctx.lineWidth = e.r * 0.8;
    ctx.beginPath();
    ctx.moveTo(e.x, e.y);
    ctx.lineTo(e.x - e.vx*0.045, e.y - e.vy*0.045);
    ctx.stroke();
    ctx.globalAlpha = e.t===2 ? 1 : 0.8;
    ctx.fillStyle = c;
    ctx.beginPath();
    ctx.arc(e.x, e.y, e.r * (e.t===2 ? 1 + 0.2*Math.sin(e.tw*2) : 1), 0, TAU);
    ctx.fill();
    if(e.t === 2){
      ctx.globalAlpha = 0.25;
      ctx.beginPath();
      ctx.arc(e.x, e.y, e.r*2.6, 0, TAU);
      ctx.fill();
    }
  }
  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'source-over';
}

function drawShips(){
  for(const s of ships){
    ctx.save();
    ctx.translate(s.x, s.y);
    ctx.rotate(s.angle);
    // двигатель
    ctx.globalCompositeOperation = 'lighter';
    const eg = ctx.createRadialGradient(-7,0,0, -7,0,9);
    eg.addColorStop(0, 'rgba(126,200,255,0.8)');
    eg.addColorStop(1, 'rgba(126,200,255,0)');
    ctx.fillStyle = eg;
    ctx.beginPath(); ctx.arc(-7, 0, 9, 0, TAU); ctx.fill();
    ctx.globalCompositeOperation = 'source-over';
    // корпус
    ctx.fillStyle = s.panic > 0.5 ? '#ffd9c9' : '#c9d4ff';
    ctx.beginPath();
    ctx.moveTo(7, 0);
    ctx.lineTo(-5, 4);
    ctx.lineTo(-3, 0);
    ctx.lineTo(-5, -4);
    ctx.closePath();
    ctx.fill();
    ctx.restore();
  }
}

function drawSparks(){
  ctx.globalCompositeOperation = 'lighter';
  for(const s of sparks){
    const a = clamp(s.life/2, 0, 1);
    ctx.globalAlpha = a * 0.9;
    ctx.strokeStyle = s.color;
    ctx.lineWidth = s.r;
    ctx.beginPath();
    ctx.moveTo(s.x, s.y);
    ctx.lineTo(s.x - s.vx*0.03, s.y - s.vy*0.03);
    ctx.stroke();
  }
  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'source-over';
}

function drawPhotons(){
  ctx.globalCompositeOperation = 'lighter';
  for(const p of photons){
    ctx.strokeStyle = 'rgba(166,255,203,0.25)';
    ctx.lineWidth = 5;
    ctx.beginPath();
    ctx.moveTo(p.px, p.py);
    ctx.lineTo(p.x, p.y);
    ctx.stroke();
    ctx.strokeStyle = 'rgba(220,255,235,0.95)';
    ctx.lineWidth = 1.6;
    ctx.beginPath();
    ctx.moveTo(p.px, p.py);
    ctx.lineTo(p.x, p.y);
    ctx.stroke();
  }
  ctx.globalCompositeOperation = 'source-over';
}

function hexToRgb(hex){
  const v = parseInt(hex.slice(1), 16);
  return [(v>>16)&255, (v>>8)&255, v&255];
}

function drawHole(h){
  const R = h.R;
  const [cr, cg, cb] = hexToRgb(h.color);
  const born = clamp(h.age/0.9, 0, 1);

  // 1) внешнее свечение аккреции
  ctx.globalCompositeOperation = 'lighter';
  const g1 = ctx.createRadialGradient(h.x, h.y, R*0.85, h.x, h.y, R*6);
  g1.addColorStop(0, 'rgba(' + cr+','+cg+','+cb+',' + (0.30*born).toFixed(3) + ')');
  g1.addColorStop(0.28, 'rgba(' + cr+','+cg+','+cb+',' + (0.09*born).toFixed(3) + ')');
  g1.addColorStop(1, 'rgba(' + cr+','+cg+','+cb+',0)');
  ctx.fillStyle = g1;
  ctx.beginPath(); ctx.arc(h.x, h.y, R*6, 0, TAU); ctx.fill();

  // 2) горячий внутренний диск
  const g2 = ctx.createRadialGradient(h.x, h.y, R, h.x, h.y, R*2.3);
  g2.addColorStop(0, 'rgba(255,244,220,' + (0.5*born).toFixed(3) + ')');
  g2.addColorStop(0.4, 'rgba(' + cr+','+cg+','+cb+',' + (0.22*born).toFixed(3) + ')');
  g2.addColorStop(1, 'rgba(' + cr+','+cg+','+cb+',0)');
  ctx.fillStyle = g2;
  ctx.beginPath(); ctx.arc(h.x, h.y, R*2.3, 0, TAU); ctx.fill();

  // 3) вращающиеся дуги диска
  const rot = G.t * (60/(R+8));
  for(let k=0;k<3;k++){
    const rr = R * (1.35 + k*0.42);
    const a0 = rot*(1 - k*0.25) + k*2.1;
    const len = 1.1 + 0.9*Math.sin(G.t*1.3 + k*2);
    ctx.strokeStyle = 'rgba(' + cr+','+cg+','+cb+',' + ((0.4 - k*0.09)*born).toFixed(3) + ')';
    ctx.lineWidth = R * (0.26 - k*0.05);
    ctx.beginPath();
    ctx.arc(h.x, h.y, rr, a0, a0 + len);
    ctx.stroke();
  }

  // 4) горизонт событий — чистая чернота
  ctx.globalCompositeOperation = 'source-over';
  ctx.fillStyle = '#000000';
  ctx.beginPath(); ctx.arc(h.x, h.y, R, 0, TAU); ctx.fill();
  // мягкий край
  const ge = ctx.createRadialGradient(h.x, h.y, R*0.88, h.x, h.y, R*1.12);
  ge.addColorStop(0, 'rgba(0,0,0,1)');
  ge.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.fillStyle = ge;
  ctx.beginPath(); ctx.arc(h.x, h.y, R*1.12, 0, TAU); ctx.fill();

  // 5) фотонное кольцо — визитная карточка чёрной дыры
  ctx.globalCompositeOperation = 'lighter';
  ctx.strokeStyle = 'rgba(255,250,235,' + (0.9*born).toFixed(3) + ')';
  ctx.lineWidth = Math.max(1, R*0.09);
  ctx.beginPath(); ctx.arc(h.x, h.y, R*1.045, 0, TAU); ctx.stroke();
  ctx.strokeStyle = 'rgba(' + cr+','+cg+','+cb+',' + (0.3*born).toFixed(3) + ')';
  ctx.lineWidth = R*0.3;
  ctx.beginPath(); ctx.arc(h.x, h.y, R*1.12, 0, TAU); ctx.stroke();

  // вспышка при недавнем питании
  if(h.eatenRecently > 0){
    h.eatenRecently -= 1/60;
    ctx.strokeStyle = 'rgba(255,255,255,' + (h.eatenRecently*1.6).toFixed(3) + ')';
    ctx.lineWidth = 1.4;
    ctx.beginPath(); ctx.arc(h.x, h.y, R*1.25, 0, TAU); ctx.stroke();
  }

  ctx.globalCompositeOperation = 'source-over';
}

function drawShockwaves(){
  ctx.globalCompositeOperation = 'lighter';
  for(const w of waves){
    ctx.globalAlpha = clamp(w.a, 0, 1) * 0.85;
    ctx.strokeStyle = w.color;
    ctx.lineWidth = Math.max(0.5, w.width * w.a);
    ctx.beginPath();
    ctx.arc(w.x, w.y, w.r, 0, TAU);
    ctx.stroke();
  }
  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'source-over';
}

function drawTexts(){
  ctx.textAlign = 'center';
  for(const t of texts){
    const a = clamp(t.life/t.max, 0, 1);
    const sx = worldToScreenX(t.x), sy = worldToScreenY(t.y);
    if(sx < -200 || sx > W+200 || sy < -100 || sy > H+100) continue;
    ctx.globalAlpha = a;
    ctx.font = '700 ' + t.size + 'px "Exo 2", sans-serif';
    ctx.fillStyle = t.color;
    ctx.shadowColor = t.color;
    ctx.shadowBlur = 12;
    ctx.fillText(t.text, sx, sy);
    ctx.shadowBlur = 0;
  }
  ctx.globalAlpha = 1;
}

/* индикаторы соперниц за экраном */
function drawIndicators(){
  for(const h of holes){
    if(h.controller === 'player') continue;
    let sx = worldToScreenX(h.x), sy = worldToScreenY(h.y);
    const m = 60;
    if(sx > m && sx < W-m && sy > m && sy < H-m) continue;   // на экране — не нужно
    const cx = W/2, cy = H/2;
    let dx = sx - cx, dy = sy - cy;
    const d = Math.hypot(dx,dy) || 1;
    dx /= d; dy /= d;
    const rx = W/2 - 46, ry = H/2 - 46;
    const t = Math.min(rx/Math.abs(dx || 0.0001), ry/Math.abs(dy || 0.0001));
    sx = cx + dx*t; sy = cy + dy*t;

    const bigger = player && h.M > player.M;
    const ang = Math.atan2(dy, dx);
    ctx.save();
    ctx.translate(sx, sy);
    ctx.rotate(ang);
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = bigger ? '#ff5d6c' : h.color;
    ctx.globalAlpha = 0.75 + 0.25*Math.sin(G.t*6);
    ctx.beginPath();
    ctx.moveTo(12, 0); ctx.lineTo(-6, 7); ctx.lineTo(-6, -7);
    ctx.closePath(); ctx.fill();
    ctx.restore();

    ctx.globalCompositeOperation = 'source-over';
    ctx.font = '600 10px "Exo 2", sans-serif';
    ctx.fillStyle = bigger ? '#ff8b96' : h.color;
    ctx.textAlign = 'center';
    ctx.fillText(fmtMass(h.M), sx, sy + 22);
    ctx.globalAlpha = 1;
  }
}

/* ────────────────────────── HUD ────────────────────────── */
function updateHUD(){
  if(!player && G.state === 'menu') return;
  const p = player || G.winner;
  if(!p) return;

  // ближайшая/крупнейшая соперница
  let rival = null, worst = -Infinity;
  for(const h of holes){
    if(h.controller === 'player') continue;
    const score = h.M / (1 + Math.hypot(h.x-p.x, h.y-p.y)/2000);
    if(score > worst){ worst = score; rival = h; }
  }

  const maxM = Math.max(p.M, rival ? rival.M : 1) * 1.15;
  el.barYou.style.width = clamp(p.M/maxM*100, 0, 100).toFixed(1) + '%';
  el.massYou.textContent = fmtMass(p.M);
  if(rival){
    el.barRival.style.width = clamp(rival.M/maxM*100, 0, 100).toFixed(1) + '%';
    el.massRival.textContent = fmtMass(rival.M);
    el.rivalName.textContent = rival.name;
    el.rivalName.style.color = rival.color;
    const ratio = p.M / rival.M;
    if(ratio >= 1.05){
      el.advantage.textContent = 'ТЫ ТЯЖЕЛЕЕ ×' + ratio.toFixed(2);
      el.advantage.className = 'good';
    }else if(ratio <= 0.95){
      el.advantage.textContent = 'ОНА ТЯЖЕЛЕЕ ×' + (1/ratio).toFixed(2);
      el.advantage.className = 'bad';
    }else{
      el.advantage.textContent = 'НА РАВНЫХ — СЛИЯНИЕ РЕШИТ ВСЁ';
      el.advantage.className = '';
    }
    // красная vignette при опасности
    const d = Math.hypot(rival.x-p.x, rival.y-p.y);
    const threat = rival.M > p.M ? clamp(1 - d/1200, 0, 1) : 0;
    el.danger.style.opacity = (threat*0.85).toFixed(2);
    el.danger.classList.toggle('on', threat > 0.35);
  }else{
    el.barRival.style.width = '0%';
    el.massRival.textContent = '—';
    el.advantage.textContent = G.wavePending ? 'ТИШИНА… ЧТО-ТО ПРИБЛИЖАЕТСЯ' : '';
    el.advantage.className = '';
    el.danger.style.opacity = 0;
    el.danger.classList.remove('on');
  }

  // импульс
  const cd = p.pulseCd > 0 ? 1 - p.pulseCd/CFG.PULSE_CD : 1;
  el.pulseFill.style.transform = 'scaleX(' + clamp(cd,0,1).toFixed(2) + ')';
  el.pulseWrap.classList.toggle('ready', cd >= 1 && p.M >= 58);
  el.pulseBtn.classList.toggle('cooling', cd < 1);

  el.score.textContent = fmt(G.score);
  el.waveLabel.textContent = 'ВОЛНА ' + Math.max(1, G.wave);
  el.bestLabel.textContent = 'РЕКОРД · ' + fmt(Math.max(G.best, G.score));
}

/* ────────────────────────── GAME OVER ────────────────────────── */
function showOver(){
  G.state = 'over';
  el.hud.classList.add('hidden');
  el.danger.style.opacity = 0;

  const s = G.stats;
  const isRecord = G.score > G.best;
  if(isRecord){ G.best = G.score; saveBest(); }

  el.overTitle.textContent = 'ТЕБЯ ПОГЛОТИЛИ';
  el.overSub.textContent = 'Твоя материя теперь часть чего-то большего. Буквально.';
  el.stats.innerHTML = [
    ['Прожито', fmtTime(s.time)],
    ['Волна', G.wave],
    ['Финальная масса', fmtMass(G.playerMassAtDeath || CFG.START_MASS)],
    ['Материи съедено', s.mass.toFixed(0) + ' ед.'],
    ['Фрегатов', s.ships],
    ['Лучей света', s.photons],
    ['Слияний выиграно', s.merges],
    ['Импульсов', s.pulses],
    ['СЧЁТ', fmt(G.score)],
    ['Рекорд', fmt(G.best)],
  ].map(([k,v])=>'<div class="stat"><span>' + k + '</span><b>' + v + '</b></div>').join('');

  el.newRecord.classList.toggle('hidden', !isRecord);
  el.over.classList.remove('hidden');
}

/* ────────────────────────── ВВОД ────────────────────────── */
function screenToWorld(sx, sy){
  return [ (sx - W/2 - cam.sx)/cam.zoom + cam.x, (sy - H/2 - cam.sy)/cam.zoom + cam.y ];
}

window.addEventListener('mousemove', e=>{
  input.mx = e.clientX; input.my = e.clientY;
  input.hasMouse = true;
  if(player && G.state === 'play'){
    const [wx, wy] = screenToWorld(e.clientX, e.clientY);
    input.wx = wx; input.wy = wy;
  }
});
window.addEventListener('mousedown', e=>{
  AUDIO.init();
  if(e.button === 2) doPulse();
  if(G.state === 'menu') return;
});
window.addEventListener('contextmenu', e=>e.preventDefault());

window.addEventListener('keydown', e=>{
  const key = e.key.toLowerCase();
  input.keys[key] = true;
  AUDIO.init();

  if(key === ' '){
    e.preventDefault();
    if(G.state === 'menu'){ startGame(); }
    else doPulse();
  }
  if(key === 'enter'){
    if(G.state === 'menu' || G.state === 'over') startGame();
  }
  if(key === 'm'){
    const on = AUDIO.toggleMute();
    $('soundHint').textContent = on ? 'звук [M]' : 'без звука [M]';
  }
  if(key === 'p' || key === 'escape' || key === 'з'){
    togglePause();
  }
});
window.addEventListener('keyup', e=>{ input.keys[e.key.toLowerCase()] = false; });

/* тач */
function touchTarget(t){
  const [wx, wy] = screenToWorld(t.clientX, t.clientY);
  input.wx = wx;
  input.wy = wy - 90/cam.zoom;   // цель чуть выше пальца — дыру видно
}
window.addEventListener('touchstart', e=>{
  AUDIO.init();
  const t = e.touches[0];
  if(t && player) touchTarget(t);
  if(e.touches.length >= 2) doPulse();
}, {passive:true});
window.addEventListener('touchmove', e=>{
  const t = e.touches[0];
  if(t && player && G.state === 'play') touchTarget(t);
}, {passive:true});
el.pulseBtn.addEventListener('touchstart', e=>{ e.preventDefault(); e.stopPropagation(); doPulse(); });
el.pulseBtn.addEventListener('click', e=>{ e.stopPropagation(); doPulse(); });

function togglePause(){
  if(G.state === 'play' && !G.paused){
    G.paused = true;
    el.pause.classList.remove('hidden');
  }else if(G.paused){
    G.paused = false;
    el.pause.classList.add('hidden');
    lastTs = performance.now();
  }
}

el.startBtn.addEventListener('click', startGame);
el.retryBtn.addEventListener('click', startGame);
el.resumeBtn.addEventListener('click', togglePause);
document.addEventListener('visibilitychange', ()=>{
  if(document.hidden && G.state === 'play' && !G.paused) togglePause();
});

/* ────────────────────────── ЦИКЛ ────────────────────────── */
let lastTs = 0;
function loop(ts){
  requestAnimationFrame(loop);
  if(!lastTs) lastTs = ts;
  let dt = Math.min(CFG.MAX_DT, (ts - lastTs)/1000);
  lastTs = ts;
  if(G.paused){ render(); return; }
  if(dt <= 0) dt = 1/60;

  const scaled = dt * G.ts;
  updateWorld(scaled);
  updateCamera(dt);       // камера в реальном времени — плавнее slow-mo
  render();

  G.hudTimer -= dt;
  if(G.hudTimer <= 0){ G.hudTimer = 0.08; updateHUD(); }
}

/* ────────────────────────── СТАРТ ────────────────────────── */
function init(){
  resize();
  initBlobs();
  resetWorld(false);
  if(el.menuBest) el.menuBest.textContent = G.best > 0 ? 'РЕКОРД · ' + fmt(G.best) + ' очков' : 'рекорда ещё нет — вселенная ждёт';
  requestAnimationFrame(loop);
}

init();

/* отладка/тесты */
window.__G = { G, holes: ()=>holes, debris: ()=>debris, player: ()=>player,
  start: startGame, pulse: doPulse,
  setTarget: (x,y)=>{ input.wx = x; input.wy = y; },
  spawnRival: (M)=>{ if(player){ const h = makeHole(player.x+400, player.y, M||player.M*0.8, 'ai', RIVAL_COLORS[0], 'СОПЕРНИЦА'); holes.push(h); } },
};

})();
