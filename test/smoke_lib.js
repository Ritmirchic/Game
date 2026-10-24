/* ═══════════════════════════════════════════════════
   Headless-харнес: стабы DOM/Canvas/rAF + загрузка игры.
   Используется smoke.js и баланс-пробами.
   ═══════════════════════════════════════════════════ */
'use strict';

function makeCtx(){
  const grad = { addColorStop(){} };
  const noop = ()=>{};
  return new Proxy({
    createRadialGradient: ()=>grad,
    createLinearGradient: ()=>grad,
    measureText: ()=>({ width: 10 }),
    canvas: { width: 1280, height: 720 },
  }, {
    get(t, k){
      if(k in t) return t[k];
      return noop;
    },
    set(){ return true; },
  });
}

function makeEl(id){
  const listeners = {};
  const e = {
    id,
    width: 300, height: 150,
    style: new Proxy({}, { get: (t,k)=>t[k]||'', set:(t,k,v)=>{t[k]=v; return true;} }),
    classList: {
      _s: new Set(),
      add(...c){ c.forEach(x=>this._s.add(x)); },
      remove(...c){ c.forEach(x=>this._s.delete(x)); },
      toggle(c, on){ on ? this._s.add(c) : this._s.delete(c); },
      contains(c){ return this._s.has(c); },
    },
    textContent: '', innerHTML: '',
    offsetWidth: 100,
    children: [],
    appendChild(c){ this.children.push(c); return c; },
    prepend(c){ this.children.unshift(c); },
    removeChild(c){ this.children = this.children.filter(x=>x!==c); return c; },
    get lastChild(){ return this.children[this.children.length-1]; },
    get firstChild(){ return this.children[0]; },
    addEventListener(ev, fn){ listeners[ev] = fn; },
    removeEventListener(){},
    getContext: ()=>makeCtx(),
    _fire(ev, arg){ if(listeners[ev]) listeners[ev](arg || {}); },
  };
  return e;
}

const elements = {};
const IDS = ['game','hud','menu','pause','over','barYou','barRival','massYou','massRival',
  'rivalName','advantage','pulseFill','pulseWrap','score','waveLabel','bestLabel',
  'log','banner','bannerText','bannerSub','danger','title','menuBest','stats','newRecord',
  'overTitle','overSub','startBtn','retryBtn','resumeBtn','pulseBtn','soundHint'];
IDS.forEach(id=>elements[id] = makeEl(id));
elements.game.width = 1280; elements.game.height = 720;

global.window = global;
const winListeners = {};
global.addEventListener = (ev, fn)=>{ winListeners[ev] = fn; };
global.removeEventListener = ()=>{};
global.document = {
  getElementById: id => elements[id] || makeEl(id),
  createElement: tag => makeEl(tag),
  addEventListener(){},
  hidden: false,
};
global.innerWidth = 1280;
global.innerHeight = 720;
global.devicePixelRatio = 1;
global.performance = { now: ()=>Date.now() };
const store = {};
global.localStorage = {
  getItem: k => store[k] || null,
  setItem: (k,v) => { store[k] = String(v); },
};

let rafCb = null, frames = 0, simT = 0;
let MAX_FRAMES = 1000000;
global.requestAnimationFrame = cb => { rafCb = cb; };
function step(n){
  for(let i=0;i<n;i++){
    simT += 1000/60;
    frames++;
    const cb = rafCb; rafCb = null;
    if(!cb) throw new Error('rAF chain broke at frame ' + frames);
    cb(simT);
    if(frames > MAX_FRAMES) throw new Error('too many frames');
  }
}

const fs = require('fs'), path = require('path'), vm = require('vm');
const root = path.join(__dirname, '..');
vm.runInThisContext(fs.readFileSync(path.join(root, 'js/audio.js'), 'utf8'), { filename:'audio.js' });
vm.runInThisContext(fs.readFileSync(path.join(root, 'js/game.js'), 'utf8'), { filename:'game.js' });

if(!window.__G) throw new Error('game not exposed');

module.exports = {
  step,
  G: window.__G,
  frames: ()=>frames,
  simT: ()=>simT,
  assert: (cond, msg)=>{ if(!cond) throw new Error('ASSERT: ' + msg); },
  noNaN: (o, name)=>{
    for(const k of ['x','y','vx','vy','M','R']){
      if(!Number.isFinite(o[k])) throw new Error('ASSERT: ' + name + '.' + k + ' = ' + o[k]);
    }
  },
};
