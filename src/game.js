/* ============================================================
   DON'T LOOK BACK — Game logic
   Loops, anomalies, entities, sanity, story, endings
   ============================================================ */
(function(){
'use strict';
const WHEX='#c9a86a'; // horror accent
const FOV=0.92, EYE=1.62, RAD=0.29, WALK=3.0, RUN=4.8;
const MAX_SANITY=100;
const TOTAL_LOOPS=8; // loops to reach escape

const world=World.makeWorld(20261025);
const occ=world.occ, P=world.P, N=world.N;
let grid=world.grid; // will be mutated per anomaly; but we keep reference

// canvases
const view=document.getElementById('view'), vctx=view.getContext('2d',{alpha:false});
let VW=1,VH=1,DPR=1;
const low=document.createElement('canvas'), lctx=low.getContext('2d',{alpha:false});
let RW=2,RH=2,img=null,pix=null,dep=null;
let scale=matchMedia('(pointer:coarse)').matches?0.44:0.52;
function resize(){
  DPR=Math.min(window.devicePixelRatio||1,1.5);
  VW=view.width=Math.floor(innerWidth*DPR);
  VH=view.height=Math.floor(innerHeight*DPR);
  view.style.width=innerWidth+'px'; view.style.height=innerHeight+'px';
  allocScale(scale);
  vctx.imageSmoothingEnabled=false;
}
function allocScale(s){
  scale=Math.max(0.24,Math.min(0.9,s));
  const maxPx=380000;
  if(VW*VH*scale*scale>maxPx) scale=Math.sqrt(maxPx/(VW*VH));
  RW=Math.max(120,Math.round(VW*scale));
  RH=Math.max(80,Math.round(VH*scale));
  low.width=RW; low.height=RH;
  img=lctx.createImageData(RW,RH); pix=new Uint32Array(img.data.buffer); dep=new Float32Array(RW*RH);
  lctx.imageSmoothingEnabled=false;
}
addEventListener('resize',resize);

// state
const G={
  mode:'intro', // intro | play | pause | dead | win1 | win2 | win3
  x:world.SPAWN.x, z:world.SPAWN.z, y:EYE, yaw:0,pitch:0,
  sanity:MAX_SANITY, loop:0, time:0, t0:0,
  flashOn:true, flashFlicker:0,
  stamina:1,
  glitch:0,punch:0,shake:0,bob:0,
  moveDist:0, demoT:0,
  loopAnomalies:[], // current active anomaly ids
  loopHasAnomaly:false,
  decided:false, correctStreak:0, wrongStreak:0,
  docsFound:0, totalDocs:world.documents.length,
  emergency:false, corridorScale:1,
  entities:[], // runtime entities
  audioAnomalies:[],
  windowViews:{}, // map window pos->view type
  wallpaperVariant:0,
  lightShift:0, lightFlicker:0,
  elevatorWrong:false, blockCorridor:false,
  // for loop reset snapshot
  _snap:{},
  sanityDrain:0,
  ending:null,
  footstepAcc:0,
  noise:0, // accumulated sound level for Listener
  lastNoisePos:null,
  imitatorTimer:0,
  observer:null,
  listener:null,
  imitator:null,
  flashlightBattery:100, // drains slightly, flicker when low
  interactionTarget:null,
  journal:[],
  loopHistory:[],
  // UI
  showJournal:false,
};
let mouseDX=0,mouseDY=0,locked=false;

// helpers
function clamp(v,a,b){return v<a?a:v>b?b:v;}
function solidAt(x,z,y0,y1){
  const xx=Math.floor(x), zz=Math.floor(z);
  if(xx<0||zz<0||xx>=N||zz>=N) return true;
  for(let y=y0;y<=y1;y++){ const vv=grid[world.idx(xx,y,zz)]; if(vv===1||vv===2) return true; }
  return false;
}
function bodyBlocked(x,z){
  const r=RAD;
  return solidAt(x-r,z-r,1,2)||solidAt(x+r,z-r,1,2)||solidAt(x-r,z+r,1,2)||solidAt(x+r,z+r,1,2)||solidAt(x,z,1,2);
}
function rayOccluded(ax,az,bx,bz){
  // DDA trace from a to b at y=1.6, check walls
  const steps=12;
  for(let i=1;i<=steps;i++){
    const t=i/steps;
    const x=ax+(bx-ax)*t, z=az+(bz-az)*t;
    const xx=Math.floor(x), zz=Math.floor(z);
    if(xx<0||zz<0||xx>=N||zz>=N) return true;
    if(grid[world.idx(xx,1,zz)]===1 || grid[world.idx(xx,2,zz)]===1) return true;
  }
  return false;
}
function inFOV(entity){
  const dx=entity.x-G.x, dz=entity.z-G.z;
  const dist=Math.hypot(dx,dz);
  if(dist<0.3) return true;
  const ang=Math.atan2(dx,dz); // note our yaw 0 = +z
  let diff=ang - G.yaw;
  diff=((diff+Math.PI)%(Math.PI*2))-Math.PI;
  const half=FOV*0.95; // approximate
  return Math.abs(diff) < 0.85 && dist<14; // 60 deg cone
}

// snapshot helpers for loop reset
function snapshotWorld(){
  // save mutable props doors grid occ lamps windowViews etc.
  const snap={};
  snap.props=JSON.parse(JSON.stringify(world.props));
  snap.doors=JSON.parse(JSON.stringify(world.baseDoors));
  snap.grid=grid.slice();
  snap.occ=occ.slice();
  snap.lamps=world.lamps.slice();
  return snap;
}
let baseSnapshot=snapshotWorld();
function restoreBase(){
  // restore from baseSnapshot
  // careful: world.props is reference used elsewhere, so assign back
  world.props=JSON.parse(JSON.stringify(baseSnapshot.props));
  world.baseDoors=JSON.parse(JSON.stringify(baseSnapshot.doors));
  grid.set(baseSnapshot.grid);
  occ.set(baseSnapshot.occ);
  world.lamps.set(baseSnapshot.lamps);
  // reset runtime flags
  G.emergency=false; G.corridorScale=1; G.wallpaperVariant=0; G.lightShift=0; G.lightFlicker=0; G.elevatorWrong=false; G.blockCorridor=false; G.windowViews={};
  G.entities=[]; G.audioAnomalies=[];
  G.corridorScale=1;
  // rebuild occ already
}

// anomaly generation per loop
function generateLoopAnomalies(loopIdx){
  restoreBase();
  G.loopAnomalies=[]; G.audioAnomalies=[]; G.entities=[];
  G.wallpaperVariant=0; G.lightShift=0; G.lightFlicker=0; G.emergency=false; G.corridorScale=1; G.elevatorWrong=false; G.blockCorridor=false; G.windowViews={};
  // difficulty curve: loop 0 = 0 anomalies (tutorial), 1-2 = 0-1, 3-5 =1-2, 6-7=2-3
  let count=0;
  let pool=[];
  if(loopIdx===0){ count=0; }
  else if(loopIdx<=2){ count=Math.random()<0.5?0:1; }
  else if(loopIdx<=5){ count=1+ (Math.random()<0.5?0:1); }
  else { count=2+ (Math.random()<0.6?0:1); }
  // ensure anomaly exists sometimes forced for story
  // For loop 7 final, ensure at least 1
  if(loopIdx===7 && count===0) count=1;

  // weighted pool: early loops favor visual (harmless), later favor entity dangerous
  const weights=[];
  for(const a of world.ANOMALIES){
    let w=1;
    if(a.danger===0) w= loopIdx<3? 2 : 0.8;
    if(a.danger===1) w= loopIdx<4? 0.6 : 1.2;
    if(a.danger===2) w= loopIdx<5? 0.3 : 1.5;
    // don't repeat too recently
    if(G.loopHistory.slice(-3).flat().includes(a.id)) w*=0.3;
    weights.push({a,w});
  }
  // pick distinct
  for(let i=0;i<count;i++){
    let tot=weights.reduce((s,q)=>s+q.w,0);
    let r=Math.random()*tot, acc=0, pick=null;
    for(const q of weights){ acc+=q.w; if(r<acc){ pick=q.a; break; } }
    if(!pick) pick=weights[0].a;
    G.loopAnomalies.push(pick.id);
    // remove weight to avoid duplicate
    const idxW=weights.findIndex(q=>q.a.id===pick.id); if(idxW>=0) weights.splice(idxW,1);
  }
  // apply each anomaly mutation
  const state={
    _prevPaintA:null, _px:null,_pz:null, _vVis:null, _newDoor:null, _wp:null,_wv:null,_cx:null,_dScale:null,
    _bx:null,_bz:null,_bRoom:null,_sw:null,_d105:null,_block:null,_d107:null,_d104:null,_dElev:null,
    windowViews:G.windowViews, corridorScale:G.corridorScale,
    entities:G.entities, audioAnomalies:G.audioAnomalies,
    emergency:G.emergency, lightShift:G.lightShift, lightFlicker:G.lightFlicker, elevatorWrong:G.elevatorWrong, blockCorridor:G.blockCorridor,
    get corridorScaleV(){return G.corridorScale;}, set corridorScaleV(v){G.corridorScale=v;},
  };
  // we need to let anomalies mutate global G directly via closure capturing G. So we patch anomalies apply to use G.
  // Our ANOMALIES definitions above already captured state.entities etc but they referenced state param. We'll call them with a wrapper that sets G.
  // Actually definitions use state.* to push, so we need to redirect to G.
  // Let's create a proxy state that syncs to G on set
  const proxy=new Proxy(state,{
    get(t,p){ if(p in t) return t[p]; if(p in G) return G[p]; return undefined; },
    set(t,p,v){ if(p==='emergency') G.emergency=v; else if(p==='lightShift') G.lightShift=v; else if(p==='lightFlicker') G.lightFlicker=v; else if(p==='corridorScale') G.corridorScale=v; else if(p==='elevatorWrong') G.elevatorWrong=v; else if(p==='blockCorridor') G.blockCorridor=v; else t[p]=v; G[p]=v; return true; }
  });
  // Apply
  for(const aid of G.loopAnomalies){
    const def=world.ANOMALIES.find(q=>q.id===aid);
    if(def && def.apply){
      // need to ensure def.apply uses G via proxy: we created anomalies with functions that expect state param referencing entities etc.
      // Our anomaly funcs were written to use state.* - but some also directly touch grid/occ etc. That's fine.
      // We'll call with proxy and also bind world context
      try{ def.apply(proxy); }catch(e){ console.error('anomaly apply fail',aid,e); }
      // sync specific props that were mutated on world.props
      // For windowViews, proxy.windowViews is G.windowViews, but anomalies also did G.windowViews assignment
    }
  }
  // sync back
  G.wallpaperVariant = world.props.find(p=>p.id==='wallpaperB')?.variant||0;
  // window views collected
  for(const p of world.props){
    if(p.kind==='window' && p.view!==undefined) G.windowViews[ (p.x|0)+','+(p.z|0) ] = p.view;
    if(p.id==='window1' && p.view) G.windowViews['5,7']=p.view;
  }
  // Also handle entities special: if follower (30) add persistent observer
  // Create runtime entity objects with behavior
  const runtime=[];
  for(const e of G.entities){
    runtime.push({
      ...e,
      lastSeen:0, state:'idle', targetX:e.x, targetZ:e.z,
      visible:false, dist:0, inFov:false, occluded:false,
      baseX:e.x, baseZ:e.z,
      time:0,
    });
  }
  G.entities=runtime;
  // Set global flags
  G.loopHasAnomaly = G.loopAnomalies.length>0;
  G.decided=false;

  // Special: ensure at least one document spawns in this loop if not all found
  // (documents already positioned, we just reveal)
  updateHUD();
  toastLoopStart();
}
function toastLoopStart(){
  if(G.loop===0){
    toast('НОВЫЙ ЦИКЛ · КОРИДОР ВЫГЛЯДИТ ОБЫЧНО · ИЩИ ОТКЛОНЕНИЯ',3000);
  } else {
    if(G.loopHasAnomaly) toast('АНОМАЛИИ ВОЗМОЖНЫ · БУДЬ ВНИМАТЕЛЕН · НЕ ОБОРАЧИВАЙСЯ',2800);
    else toast('ЦИКЛ '+(G.loop+1)+' · ПОКА ТИХО · ПРОВЕРЬ ДЕТАЛИ',2200);
  }
}

// interaction
function getInteraction(){
  // doors within 1.6m front
  const fx=Math.sin(G.yaw), fz=Math.cos(G.yaw);
  const cx=G.x+fx*0.9, cz=G.z+fz*0.9;
  for(const d of world.baseDoors){
    const dist=Math.hypot(d.x - G.x, d.z - G.z);
    if(dist<1.8){
      const dx=d.x-G.x, dz=d.z-G.z;
      const dot=(dx*fx+dz*fz);
      if(dot>0.3){
        return {kind:'door', door:d, dist};
      }
    }
  }
  // documents
  for(const doc of world.documents){
    if(doc.found) continue;
    const dist=Math.hypot(doc.x-G.x, doc.z-G.z);
    if(dist<1.7) return {kind:'doc', doc, dist};
  }
  // decision point: if near z 18-19 and x 14-17, show report prompts
  if(G.mode==='play' && G.z>17.5 && G.z<19.5 && G.x>13 && G.x<18 && !G.decided){
    return {kind:'decision', dist:0};
  }
  // props that can be inspected? paintings
  for(const p of world.props){
    if(p.kind==='painting' || p.kind==='doll'){
      const dist=Math.hypot(p.x-G.x, p.z-G.z);
      if(dist<1.7 && Math.abs(p.y-G.y)<1.2){
        const dx=p.x-G.x, dz=p.z-G.z, dot=(dx*fx+dz*fz)/ (dist||1);
        if(dot>0.6) return {kind:'inspect', prop:p, dist};
      }
    }
  }
  return null;
}

// audio engine
let AC=null, master=null, muted=false, padGain=null, padFilter=null, padOsc=[], noiseBuf=null;
let footSrc=null;
function initAudio(){
  if(AC) return;
  try{ AC=new (window.AudioContext||window.webkitAudioContext)(); }catch(e){return;}
  master=AC.createGain(); master.gain.value=0.85; master.connect(AC.destination);
  padFilter=AC.createBiquadFilter(); padFilter.type='lowpass'; padFilter.frequency.value=340; padFilter.Q.value=2.2;
  padGain=AC.createGain(); padGain.gain.value=0.0001; padFilter.connect(padGain); padGain.connect(master);
  const bases=[52, 53.2, 78];
  for(let i=0;i<3;i++){
    const o=AC.createOscillator(); o.type=i===2?'triangle':'sawtooth';
    o.frequency.value=bases[i]; o.detune.value=[0,7,-5][i];
    o.connect(padFilter); o.start(); padOsc.push(o);
  }
  padGain.gain.linearRampToValueAtTime(0.06, AC.currentTime+4);
  const lfo=AC.createOscillator(), lg=AC.createGain(); lfo.frequency.value=0.04; lg.gain.value=120; lfo.connect(lg); lg.connect(padFilter.frequency); lfo.start();
  noiseBuf=AC.createBuffer(1, AC.sampleRate*0.4, AC.sampleRate);
  const d=noiseBuf.getChannelData(0); for(let i=0;i<d.length;i++) d[i]=(Math.random()*2-1)*(1-i/d.length*0.5);
}
function setPadLoop(loop){ if(!AC||!padOsc.length) return; const base=[52,54.5,57,61,58,66,70,72][loop%8]; const now=AC.currentTime; padOsc[0].frequency.linearRampToValueAtTime(base,now+1); padOsc[1].frequency.linearRampToValueAtTime(base*1.007,now+1); padOsc[2].frequency.linearRampToValueAtTime(base*1.498,now+1); padFilter.frequency.linearRampToValueAtTime(340+loop*18+ G.emergency?80:0, now+1); }
function tone(freq,dur,type,vol,slideTo,when){
  if(!AC||muted) return; const t=AC.currentTime+(when||0);
  const o=AC.createOscillator(), g=AC.createGain();
  o.type=type||'sine'; o.frequency.setValueAtTime(freq,t); if(slideTo) o.frequency.exponentialRampToValueAtTime(Math.max(18,slideTo),t+dur);
  g.gain.setValueAtTime(0.0001,t); g.gain.exponentialRampToValueAtTime(vol,t+Math.min(0.02,dur*0.3)); g.gain.exponentialRampToValueAtTime(0.0001,t+dur);
  o.connect(g); g.connect(master); o.start(t); o.stop(t+dur+0.04);
}
function sfx(s){
  if(!AC||muted) return;
  if(s==='door'){ tone(180,0.22,'square',0.07,90); tone(80,0.28,'sine',0.04); }
  else if(s==='locked'){ tone(120,0.18,'square',0.05,70); tone(90,0.22,'sawtooth',0.03); }
  else if(s==='pick'){ [880,1174,1568].forEach((f,i)=>tone(f,0.36,'triangle',0.04,null,i*0.06)); }
  else if(s==='correct'){ tone(440,0.5,'sine',0.06,880); tone(880,0.7,'sine',0.04);}
  else if(s==='wrong'){ tone(180,0.6,'sawtooth',0.07,60); tone(90,0.8,'triangle',0.05); }
  else if(s==='breath'){ tone(70,1.2,'sawtooth',0.03,65); }
  else if(s==='knock'){ tone(180,0.07,'square',0.09); setTimeout(()=>tone(180,0.07,'square',0.09),180); }
  else if(s==='whisper'){ tone(300,0.9,'sine',0.02,280); tone(520,0.7,'triangle',0.015); }
  else if(s==='scream'){ tone(900,0.4,'sawtooth',0.08,1200); tone(450,0.6,'square',0.06); }
  else if(s==='foot'){ if(!noiseBuf) return; const src=AC.createBufferSource(); src.buffer=noiseBuf; const f=AC.createBiquadFilter(); f.type='lowpass'; f.frequency.value=540; const g=AC.createGain(); g.gain.value=0.035; src.connect(f); f.connect(g); g.connect(master); src.start(); }
  else if(s==='flash'){ tone(1000,0.05,'square',0.04); }
  else if(s==='radio'){ tone(400+Math.random()*600,0.15,'square',0.03); }
  else if(s==='elevator_ding'){ tone(1200,0.6,'sine',0.07); tone(1800,0.5,'sine',0.05,900); }
}
function sfxLoopAnomalies(dt){
  if(!AC||muted) return;
  for(const a of G.audioAnomalies){
    if(a.kind==='footsteps_behind'){
      a.t=(a.t||0)+dt; if(a.t> (1.8+Math.random()*1.2)){ a.t=0; // panned behind
        const p=AC.createPanner(); p.panningModel='HRTF'; p.setPosition(G.x- Math.sin(G.yaw)*2, 0, G.z- Math.cos(G.yaw)*2);
        const src=AC.createBufferSource(); src.buffer=noiseBuf; const f=AC.createBiquadFilter(); f.type='lowpass'; f.frequency.value=420; const g=AC.createGain(); g.gain.value=0.04; src.connect(f); f.connect(p); p.connect(master); src.start();
      }
    }
    if(a.kind==='whisper'){ a.t=(a.t||0)+dt; if(a.t>4+Math.random()*3){ a.t=0; tone(220+Math.random()*80,0.8,'sine',0.025); } }
    if(a.kind==='knock'){ a.t=(a.t||0)+dt; const intv=a.interval||2.5; if(a.t>intv){ a.t=0; sfx('knock'); } }
    if(a.kind==='radio'){ if(Math.random()<0.08) sfx('radio'); }
    if(a.kind==='elevator'){ a.t=(a.t||0)+dt; if(a.t>5){ a.t=0; sfx('elevator_ding'); tone(60,1.2,'sawtooth',0.03,55); } }
    if(a.kind==='footsteps_above'){ if(Math.random()<0.03) { const f=AC.createBiquadFilter(); f.type='bandpass'; f.frequency.value=180; const g=AC.createGain(); g.gain.value=0.02; const src=AC.createBufferSource(); src.buffer=noiseBuf; src.connect(f); f.connect(g); g.connect(master); src.start(); } }
    if(a.kind==='breathing'){ a.t=(a.t||0)+dt; if(a.t>3.2){ a.t=0; sfx('breath'); } }
  }
}

// input
const keys=Object.create(null);
addEventListener('keydown',e=>{
  const k=e.key.length===1?e.key.toLowerCase():e.key.toLowerCase();
  keys[k]=true;
  if([' ','arrowup','arrowdown','arrowleft','arrowright','tab'].includes(k)) e.preventDefault();
  if(G.mode==='play'){
    if(k==='e' || k==='enter' || k===' ') interact();
    if(k==='f') toggleFlash();
    if(k==='r') reportAnomaly(true);
    if(k==='t') reportAnomaly(false);
    if(k==='q') reportAnomaly(false); // legacy
    if(k==='j') toggleJournal();
    if(k==='escape') setPause(true);
    if(k==='m') toggleMute();
  } else if(G.mode==='pause' && k==='escape') setPause(false);
  if(k==='escape' && (G.mode.startsWith('win')||G.mode==='dead')){ /* allow menu */ }
});
addEventListener('keyup',e=>{ keys[e.key.toLowerCase()]=false; });
addEventListener('blur',()=>{ for(const k in keys) keys[k]=false; });
view.addEventListener('mousedown',e=>{
  if(G.mode==='intro'||G.mode.startsWith('win')||G.mode==='dead') return;
  if(G.mode==='pause'){ setPause(false); return; }
  if(!locked){ lockPointer(); return; }
  if(e.button===0) interact(); // left click interact / report if decision
  if(e.button===2) toggleFlash();
});
view.addEventListener('contextmenu',e=>e.preventDefault());
addEventListener('mousemove',e=>{
  if(!locked) return;
  mouseDX+=e.movementX; mouseDY+=e.movementY;
});
document.addEventListener('pointerlockchange',()=>{
  locked=document.pointerLockElement===view;
  if(!locked && G.mode==='play') setPause(true);
});
function lockPointer(){ if(view.requestPointerLock) try{view.requestPointerLock();}catch(e){} }
document.addEventListener('visibilitychange',()=>{ if(document.hidden && G.mode==='play') setPause(true); });

// touch
const touchWrap=document.getElementById('touch'), stick=document.getElementById('stick'), knob=document.getElementById('knob');
const tmove={x:0,y:0}; let stickId=null, lookId=null, lastLook={x:0,y:0};
function isTouch(){return matchMedia('(pointer:coarse)').matches;}
if(isTouch()) touchWrap.classList.add('on');
stick.addEventListener('touchstart',e=>{ const t=e.changedTouches[0]; stickId=t.identifier; e.preventDefault(); },{passive:false});
addEventListener('touchmove',e=>{
  for(const t of e.changedTouches){
    if(t.identifier===stickId){
      const r=stick.getBoundingClientRect();
      const dx=t.clientX-(r.left+r.width/2), dy=t.clientY-(r.top+r.height/2);
      const m=Math.min(1,Math.hypot(dx,dy)/(r.width*0.42)), a=Math.atan2(dy,dx);
      tmove.x=Math.cos(a)*m; tmove.y=-Math.sin(a)*m;
      knob.style.transform=`translate(${Math.cos(a)*m*30}px,${Math.sin(a)*m*30}px)`;
    } else if(lookId===null && t.clientX> innerWidth*0.4){ lookId=t.identifier; lastLook={x:t.clientX,y:t.clientY};}
    else if(t.identifier===lookId){ G.yaw-=(t.clientX-lastLook.x)*0.005; G.pitch=clamp(G.pitch-(t.clientY-lastLook.y)*0.005,-0.85,0.85); lastLook={x:t.clientX,y:t.clientY};}
  }
},{passive:true});
addEventListener('touchend',e=>{
  for(const t of e.changedTouches){ if(t.identifier===stickId){ stickId=null; tmove.x=tmove.y=0; knob.style.transform=''; } if(t.identifier===lookId) lookId=null; }
});
document.getElementById('tReport')?.addEventListener('touchstart',e=>{e.preventDefault(); reportAnomaly(true);},{passive:false});
document.getElementById('tSafe')?.addEventListener('touchstart',e=>{e.preventDefault(); reportAnomaly(false);},{passive:false});
document.getElementById('tFlash')?.addEventListener('touchstart',e=>{e.preventDefault(); toggleFlash();},{passive:false});
document.getElementById('tInteract')?.addEventListener('touchstart',e=>{e.preventDefault(); interact();},{passive:false});

// UI helpers
const toastEl=document.getElementById('toast'), probeEl=document.getElementById('probe');
let toastT=null;
function toast(msg,ms){
  toastEl.textContent=msg; toastEl.classList.toggle('on',!!msg);
  clearTimeout(toastT); if(msg) toastT=setTimeout(()=>toastEl.classList.remove('on'),ms||2200);
}
function updateHUD(){
  document.getElementById('loopNum').textContent=(G.loop+1)+' / '+TOTAL_LOOPS;
  document.getElementById('sanityBar').style.width=G.sanity+'%';
  document.getElementById('sanityVal').textContent=Math.round(G.sanity)+'%';
  document.getElementById('sanityBar').style.background=G.sanity>60?'#c9a86a':G.sanity>30?'#d87a1a':'#a32222';
  document.getElementById('docsFound').textContent=G.docsFound+'/'+G.totalDocs;
  document.getElementById('anomalyCount').textContent=G.loopAnomalies.length;
  // journal chips
  const jc=document.getElementById('journalChips'); if(jc){ jc.innerHTML=''; for(const id of G.loopHistory.flat().slice(-8)){ const el=document.createElement('span'); el.textContent=id; el.style.background='#c9a86a22'; el.style.border='1px solid #c9a86a55'; el.style.padding='2px 6px'; el.style.borderRadius='4px'; el.style.fontSize='10px'; jc.appendChild(el); } }
}
function toggleMute(){ muted=!muted; if(master) master.gain.linearRampToValueAtTime(muted?0:0.85, AC.currentTime+0.15); toast(muted?'ЗВУК ВЫКЛЮЧЕН':'ЗВУК ВКЛ',1100); }
function toggleFlash(){ G.flashOn=!G.flashOn; sfx('flash'); G.noise+=1.2; G.lastNoisePos={x:G.x,z:G.z}; toast(G.flashOn?'ФОНАРЬ ВКЛ':'ФОНАРЬ ВЫКЛ',900); }
function toggleJournal(){ G.showJournal=!G.showJournal; document.getElementById('journal').classList.toggle('on',G.showJournal); if(G.showJournal) renderJournal(); }

// interactions
function interact(){
  const tgt=getInteraction();
  G.noise+=0.8; G.lastNoisePos={x:G.x,z:G.z};
  if(!tgt){ // if decision zone, treat as report? No
    // footsteps noise already
    return;
  }
  if(tgt.kind==='door'){
    const d=tgt.door;
    if(!d.open){
      if(d.locked){
        // check if has key? For now only basement needs docs
        if(d.id==='dBase' && G.docsFound>=3){ d.locked=false; d.open=true; setDoorGeom(d,true); toast('ПОДВАЛ ОТКРЫТ',1500); sfx('door'); }
        else if(d.id==='dEmerg' && G.loop>=TOTAL_LOOPS-1 && G.correctStreak>=2){ d.locked=false; d.open=true; setDoorGeom(d,true); toast('АВАРИЙНЫЙ ВЫХОД ОТКРЫТ',2000); sfx('door'); }
        else { toast('ЗАПЕРТО',900); sfx('locked'); G.sanity-=2; updateHUD(); }
      } else {
        d.open=true; setDoorGeom(d,true); toast('ДВЕРЬ ОТКРЫТА: '+d.label,900); sfx('door');
      }
    } else {
      // close? Keep open for horror, but allow toggling for maintenance etc.
      // For entity door peek, closing triggers scare
      if(d.id==='d104' && G.entities.some(e=>e.kind==='door_peek')){ d.open=false; setDoorGeom(d,false); toast('ТЫ ЗАКРЫЛ ДВЕРЬ',900); sfx('door'); G.sanity-=5; updateHUD(); }
      else toast('ДВЕРЬ ОТКРЫТА',700);
    }
  } else if(tgt.kind==='doc'){
    tgt.doc.found=true; G.docsFound++; sfx('pick'); toast('НАЙДЕН ДОКУМЕНТ: '+tgt.doc.title,3000);
    G.journal.push({type:'doc', title:tgt.doc.title, text:tgt.doc.text, loop:G.loop});
    updateHUD(); renderJournal();
    if(G.docsFound===G.totalDocs) toast('ВСЕ ДОКУМЕНТЫ СОБРАНЫ · ТЫ ПОНИМАЕШЬ ПРАВДУ',3500);
  } else if(tgt.kind==='decision'){
    // decide depends on last key pressed? But we require R/T. So just hint
    toast('НАЖМИ [R] — АНОМАЛИЯ / [T] — ЧИСТО',1500);
  } else if(tgt.kind==='inspect'){
    const p=tgt.prop;
    if(p.kind==='painting'){
      if(p.variant===1) { toast('КРАСКА СВЕЖАЯ · КТО-ТО НАБЛЮДАЕТ С ПОРТРЕТА',1800); G.sanity-=3; }
      else toast('ПЕЙЗАЖ · ТИХО',900);
    } else if(p.kind==='doll'){ toast(p.scale>1.5?'КУКЛА СМОТРИТ ПРЯМО НА ТЕБЯ':'Старая кукла',1000); }
  }
}
function setDoorGeom(d,isOpen){
  const x=d.x, z=d.z;
  if(isOpen){ grid[world.idx(x,1,z)]=0; grid[world.idx(x,2,z)]=0; occ[(2)*P*P+(z+1)*P+(x+1)]=0; occ[(3)*P*P+(z+1)*P+(x+1)]=0; }
  else { grid[world.idx(x,1,z)]=1; grid[world.idx(x,2,z)]=1; occ[(2)*P*P+(z+1)*P+(x+1)]=1; occ[(3)*P*P+(z+1)*P+(x+1)]=1; }
}

function reportAnomaly(isAnomalyReport){
  if(G.mode!=='play' || G.decided) return;
  // Must be in decision zone? Allow anywhere but penalize if not in zone
  const inZone=G.z>17 && G.z<19.5 && G.x>13 && G.x<18;
  if(!inZone){ toast('НУЖНО ДОЙТИ ДО ПЕРЕКРЁСТКА',1000); return; }
  G.decided=true;
  const correct = (isAnomalyReport===G.loopHasAnomaly);
  G.noise+=1.5;
  if(correct){
    G.correctStreak++; G.wrongStreak=0; G.sanity=Math.min(100,G.sanity+ (G.loopHasAnomaly?6:3));
    sfx('correct');
    toast(isAnomalyReport?'ВЕРНО · АНОМАЛИЯ ЗАФИКСИРОВАНА':'ВЕРНО · ЦИКЛ ЧИСТ',2000);
    G.journal.push({type:'report', loop:G.loop, result:'correct', anomaly:G.loopAnomalies.slice(), time:G.time});
  } else {
    G.correctStreak=0; G.wrongStreak++; G.sanity-=18 + (G.loopHasAnomaly?7:0);
    sfx('wrong');
    G.shake=1.2; G.glitch=0.9;
    if(isAnomalyReport) toast('ОШИБКА · АНОМАЛИИ НЕ БЫЛО · ОНО ЗАМЕТИЛО ТВОЮ ОШИБКУ',2800);
    else toast('ОШИБКА · ТЫ ПРОПУСТИЛ АНОМАЛИЮ · ОНО БЛИЖЕ',2800);
    G.journal.push({type:'report', loop:G.loop, result:'wrong', anomaly:G.loopAnomalies.slice(), time:G.time});
    // spawn imitator trick or accelerate follower
    if(Math.random()<0.7 && !G.entities.some(e=>e.follower)){
      G.entities.push({id:30,kind:'follower',x:G.x-1,z:G.z-4,y:1.4,speed:1.4,follower:true,observer:true,baseX:G.x-1,baseZ:G.z-4,time:0,dist:0,inFov:false,occluded:false});
      toast('ТЫ СЛЫШИШЬ ШАГИ ПОЗАДИ',1200);
    }
  }
  updateHUD();
  // advance loop after delay
  setTimeout(()=>nextLoop(), 1400);
}

function nextLoop(){
  G.loopHistory.push(G.loopAnomalies.slice());
  G.loop++;
  if(G.sanity<=0){ doEnding('bad'); return; }
  if(G.loop>=TOTAL_LOOPS){
    // check endings
    if(G.docsFound>=5 && G.correctStreak>=3) doEnding('truth');
    else if(G.sanity>=65 && G.correctStreak>=2) doEnding('escape');
    else if(G.sanity<=35) doEnding('bad');
    else doEnding('escape');
    return;
  }
  // teleport to spawn, keep sanity
  G.x=world.SPAWN.x; G.z=world.SPAWN.z; G.yaw=0;
  G.glitch=0.7; G.punch=0.6; G.shake=0.6;
  generateLoopAnomalies(G.loop);
  setPadLoop(G.loop);
  updateHUD();
}

// endings
function doEnding(which){
  G.ending=which;
  if(which==='escape'){
    G.mode='win1';
    document.getElementById('ending1').classList.remove('gone');
    document.getElementById('eStatTime').textContent=fmt(G.time);
    document.getElementById('eStatLoops').textContent=G.loop+'/'+TOTAL_LOOPS;
    document.getElementById('eStatSanity').textContent=Math.round(G.sanity)+'%';
    document.getElementById('eStatDocs').textContent=G.docsFound+'/'+G.totalDocs;
    sfx('correct');
    if(document.pointerLockElement) document.exitPointerLock();
  } else if(which==='truth'){
    G.mode='win2';
    document.getElementById('ending2').classList.remove('gone');
    document.getElementById('e2Time').textContent=fmt(G.time);
    sfx('pick');
    if(document.pointerLockElement) document.exitPointerLock();
  } else {
    G.mode='dead';
    document.getElementById('endingBad').classList.remove('gone');
    document.getElementById('badReason').textContent=G.sanity<=0?'Твоя рассудок распался · Ты стал частью коридора':'Оно догнало тебя, когда ты отвернулся';
    if(document.pointerLockElement) document.exitPointerLock();
    sfx('scream');
  }
}

// mode helpers
function startGame(){
  initAudio(); if(AC&&AC.state==='suspended') AC.resume();
  resetGame();
  G.mode='play';
  document.getElementById('intro').classList.add('gone');
  document.getElementById('pause').classList.add('gone');
  document.getElementById('ending1').classList.add('gone');
  document.getElementById('ending2').classList.add('gone');
  document.getElementById('endingBad').classList.add('gone');
  document.getElementById('hud').style.opacity=1;
  lockPointer();
  setPadLoop(0);
  toast('ДОБРО ПОЖАЛОВАТЬ В ДОМ · НЕ СМОТРИ НАЗАД · ИЩИ АНОМАЛИИ',3600);
}
function resetGame(){
  G.sanity=MAX_SANITY; G.loop=0; G.time=0; G.t0=performance.now(); G.glitch=0; G.punch=0; G.shake=0; G.bob=0; G.moveDist=0;
  G.x=world.SPAWN.x; G.z=world.SPAWN.z; G.yaw=0; G.pitch=0; G.flashOn=true; G.correctStreak=0; G.wrongStreak=0;
  G.docsFound=0; world.documents.forEach(d=>d.found=false); G.journal=[]; G.loopHistory=[]; G.entities=[]; G.audioAnomalies=[];
  G.imitatorTimer=0; G.noise=0; G.sanityDrain=0; G.stamina=1;
  restoreBase(); generateLoopAnomalies(0);
}
function setPause(p){
  if(G.mode.startsWith('win')||G.mode==='dead') return;
  if(p){ G.mode='pause'; document.getElementById('pause').classList.remove('gone'); if(document.pointerLockElement) document.exitPointerLock(); }
  else { G.mode='play'; document.getElementById('pause').classList.add('gone'); lockPointer(); }
}
document.getElementById('btnStart').addEventListener('click',startGame);
document.getElementById('btnResume').addEventListener('click',()=>setPause(false));
document.getElementById('btnRestart').addEventListener('click',startGame);
document.getElementById('btnAgain1').addEventListener('click',startGame);
document.getElementById('btnAgain2').addEventListener('click',startGame);
document.getElementById('btnAgainBad').addEventListener('click',startGame);

// HUD render helpers
function fmt(t){ const m=Math.floor(t/60), s=Math.floor(t%60); return String(m).padStart(2,'0')+':'+String(s).padStart(2,'0'); }

function renderJournal(){
  const el=document.getElementById('journalContent');
  if(!el) return;
  el.innerHTML='';
  for(let i=G.journal.length-1;i>=0;i--){
    const j=G.journal[i];
    const d=document.createElement('div'); d.style.padding='8px 10px'; d.style.background='#ffffff08'; d.style.border='1px solid #ffffff12'; d.style.borderRadius='8px'; d.style.marginBottom='8px'; d.style.fontSize='11px'; d.style.lineHeight='1.6';
    if(j.type==='doc'){ d.innerHTML='<b style="color:#c9a86a">'+j.title+'</b><br>'+j.text; }
    else if(j.type==='report'){ const ok=j.result==='correct'?'✓ ВЕРНО':'✗ ОШИБКА'; d.innerHTML='<b>Цикл '+(j.loop+1)+' · '+ok+'</b><br>Аномалии: '+(j.anomaly.length?j.anomaly.join(', '):'нет')+' · '+fmt(j.time); d.style.color=j.result==='correct'?'#a3d9b1':'#e89a9a'; }
    el.appendChild(d);
  }
  if(G.journal.length===0) el.innerHTML='<div style="opacity:.6">Пусто · Исследуй дом и собирай улики. Нажми E у светящихся объектов.</div>';
}

// update
let stepAcc=0, footTimer=0;
function update(dt){
  if(G.mode==='intro'){
    G.demoT+=dt; G.yaw+=dt*0.12; G.pitch=Math.sin(G.demoT*0.28)*0.08+0.02;
    const a=G.demoT*0.22; G.x=15.5+Math.cos(a)*2.2; G.z=16.5+Math.sin(a)*1.8;
    return;
  }
  if(G.mode!=='play') return;
  G.time=(performance.now()-G.t0)/1000; document.getElementById('timer').textContent=fmt(G.time);
  const sens=0.0024;
  G.yaw-=mouseDX*sens; G.pitch=clamp(G.pitch-mouseDY*sens,-0.85,0.85); mouseDX=0; mouseDY=0;
  let f=0,s=0;
  if(keys['w']||keys['ц']) f+=1;
  if(keys['s']||keys['ы']) f-=1;
  if(keys['a']||keys['ф']) s-=1;
  if(keys['d']||keys['в']) s+=1;
  f+=tmove.y; s+=tmove.x;
  const run=keys['shift']||keys['shiftleft']||keys['shiftright'];
  const spd=(run?RUN:WALK)*dt * (G.sanity<20?0.85:1) * (G.stamina>0.2?1:0.7);
  const len=Math.hypot(f,s);
  let moved=false;
  if(len>1e-4){
    f/=Math.max(1,len); s/=Math.max(1,len);
    const sy=Math.sin(G.yaw), cy=Math.cos(G.yaw);
    const mx=(sy*f+cy*s)*spd, mz=(cy*f-sy*s)*spd;
    if(!bodyBlocked(G.x+mx, G.z)) G.x+=mx; else G.x+=mx*0.1;
    if(!bodyBlocked(G.x, G.z+mz)) G.z+=mz; else G.z+=mz*0.1;
    // corridor scale illusion: when anomaly 20 active, stretch perceived distance
    if(G.corridorScale>1){
      // south corridor feels longer: slow down slightly in that region to simulate longer walk
      if(G.z>19 && G.z<27) { G.x-=mx*0.25; G.z-=mz*0.25; } // extra friction
    }
    G.moveDist+=Math.hypot(mx,mz);
    G.bob+=dt*(run?13:9);
    stepAcc+=Math.hypot(mx,mz);
    if(stepAcc>(run?1.55:1.25)){ stepAcc=0; sfx('foot'); G.noise+=run?1.2:0.6; G.lastNoisePos={x:G.x,z:G.z}; }
    G.stamina=clamp(G.stamina - (run?dt*0.15: -dt*0.25),0,1);
    moved=true;
  } else {
    G.bob+=dt*1.2; G.stamina=clamp(G.stamina+dt*0.35,0,1);
  }
  G.y=EYE;
  // sanity drain over time, faster when anomalies nearby & emergency
  let drain=0.015;
  if(G.emergency) drain+=0.02;
  if(G.loopHasAnomaly) drain+=0.012;
  if(!G.flashOn) drain+=0.018;
  if(G.entities.some(e=>e.follower)) drain+=0.02;
  G.sanity=clamp(G.sanity - drain*dt,0,100);
  if(G.sanity<=0) doEnding('bad');
  // flashlight battery flicker
  if(G.flashOn){ G.flashlightBattery=clamp(G.flashlightBattery - dt*0.03,0,100);
    if(G.flashlightBattery<25 && Math.random()<0.02) { G.flashFlicker=0.5; sfx('radio'); }
  }
  G.flashFlicker=Math.max(0,G.flashFlicker - dt*2);
  // entity AI
  updateEntities(dt);
  // imitator false cues
  G.imitatorTimer+=dt;
  if(G.imitatorTimer>12+Math.random()*18){
    G.imitatorTimer=0;
    if(Math.random()<0.5 && G.loop>2){
      // fake audio cue
      const fake=['footsteps_behind','knock','whisper'][Math.floor(Math.random()*3)];
      toast('ТЫ ЧТО-ТО СЛЫШАЛ... ПОКАЗАЛОСЬ?',1200);
      sfx(fake==='knock'?'knock': fake==='whisper'?'whisper':'foot');
      G.noise+=0.4;
    }
  }
  // interaction probe
  updateProbe();
  sfxLoopAnomalies(dt);
  // decay effects
  G.glitch=Math.max(0,G.glitch - dt*2.8);
  G.punch=Math.max(0,G.punch - dt*2.2);
  G.shake=Math.max(0,G.shake - dt*3.0);
  // check win by reaching core after loops? Already via nextLoop ending
  // Also allow basement secret: if player reaches basement door with docs
  if(G.x>5 && G.x<7 && G.z>25 && G.z<26.5){
    if(!world.baseDoors.find(d=>d.id==='dBase').open && G.docsFound>=3){
      world.baseDoors.find(d=>d.id==='dBase').open=true; setDoorGeom(world.baseDoors.find(d=>d.id==='dBase'),true);
      toast('ТЫ НАШЁЛ ПОДВАЛ · ЗДЕСЬ ТЕМНО И ПАХНЕТ ПЛЕСЕНЬЮ',3000);
      sfx('door');
    }
  }
  // check documents pickup via proximity already in interaction, but also auto pickup if near
  for(const doc of world.documents){
    if(!doc.found && Math.hypot(doc.x-G.x, doc.z-G.z)<1.2){
      // hint
    }
  }
  updateHUD();
}

function updateEntities(dt){
  for(const e of G.entities){
    e.time+=dt;
    e.dist=Math.hypot(e.x-G.x, e.z-G.z);
    e.inFov=inFOV(e);
    e.occluded=rayOccluded(G.x,G.z,e.x,e.z);
    e.visible=e.inFov && !e.occluded && e.dist<13;
    // Observer logic (follower)
    if(e.follower){
      if(e.visible){
        e.state='frozen';
        // if too close and visible, still drain sanity but not kill. If player looks away and it's close, attack.
      } else {
        e.state='hunt';
        // move towards player
        const dx=G.x-e.x, dz=G.z-e.z;
        const d=Math.hypot(dx,dz)||1;
        const nx=dx/d, nz=dz/d;
        let mv=e.speed*dt;
        // avoid walls: check next pos
        const nxPos=e.x+nx*mv, nzPos=e.z+nz*mv;
        if(!bodyBlocked(nxPos, nzPos)){ e.x=nxPos; e.z=nzPos; }
        else {
          // try sidestep
          if(!bodyBlocked(e.x+nx*mv, e.z)) e.x+=nx*mv;
          if(!bodyBlocked(e.x, e.z+nz*mv)) e.z+=nz*mv;
        }
        if(d<0.65){
          // catch
          G.sanity-=35*dt;
          G.shake=1.0; G.glitch=0.8;
          if(G.sanity<=0) doEnding('bad');
          if(d<0.5 && !e.visible){ // insta kill if directly behind and close
            G.sanity=0; doEnding('bad');
          }
        }
      }
    }
    // Figure at end: stands still, vanishes on approach or when looked long
    if(e.kind==='figure'){
      e.visible=e.dist<18 && !rayOccluded(G.x,G.z,e.x,e.z);
      if(e.dist<2.5){ // jumpscare then vanish
        G.sanity-=12; G.glitch=0.9; G.shake=0.8; sfx('scream');
        e.x=-100; // hide
      }
      if(e.inFov && e.dist<10 && e.time>3){ // fades if stared
        e.alpha=Math.max(0,1-(e.time-3)*0.3);
      }
    }
    // Shadow crossing: moves fast linear
    if(e.kind==='shadow'){
      e.x+=Math.sin(e.time*2)*dt*4; e.z+=Math.cos(e.time*1.3)*dt*3;
      if(e.time>2.5) e.x=-100;
    }
    // Door peek: peeks when not looked, hides when seen
    if(e.kind==='door_peek'){
      if(e.visible){ e.hideTimer=(e.hideTimer||0)+dt; if(e.hideTimer>0.8) e.x=-100; }
      else e.hideTimer=0;
    }
    // window watcher
    if(e.kind==='window_watcher'){
      if(e.visible && e.dist<6) G.sanity-=0.08;
    }
    // LISTENER: stalks to last noise position, then to player
    if(e.listener){
      const target = G.lastNoisePos || {x:G.x,z:G.z};
      const dx=target.x-e.x, dz=target.z-e.z, d=Math.hypot(dx,dz)||1;
      if(d>0.6){
        const nx=dx/d, nz=dz/d, mv=e.speed*dt*0.95;
        const npX=e.x+nx*mv, npZ=e.z+nz*mv;
        if(!bodyBlocked(npX,npZ)){ e.x=npX; e.z=npZ; }
        else { if(!bodyBlocked(e.x+nx*mv,e.z)) e.x+=nx*mv; if(!bodyBlocked(e.x,e.z+nz*mv)) e.z+=nz*mv; }
      } else {
        // reached noise source, now hunt player if close
        const pdx=G.x-e.x, pdz=G.z-e.z, pd=Math.hypot(pdx,pdz);
        if(pd<12 && !e.visible){
          const nx=pdx/pd, nz=pdz/pd, mv=e.speed*dt;
          if(!bodyBlocked(e.x+nx*mv,e.z+nz*mv)){ e.x+=nx*mv; e.z+=nz*mv; }
        }
      }
      if(e.dist<0.75){ G.sanity-=28*dt; G.shake=0.9; if(e.dist<0.55) { G.sanity=0; doEnding('bad'); } }
      // listener hates light
      if(G.flashOn && e.visible && e.dist<5) e.speed=0.7; else e.speed=2.1;
    }
  }
  // global noise decay & spawn check before loop
  if(G.noise>4 && !G.entities.some(q=>q.listener) && G.loop>1 && Math.random()<0.025){
    const p=G.lastNoisePos||{x:G.x+(Math.random()-0.5)*3, z:G.z+(Math.random()-0.5)*3};
    G.entities.push({id:99,kind:'listener',x:p.x,z:p.z,y:1.35,speed:2.1,listener:true,time:0,dist:0,inFov:false,occluded:false,visible:false});
    toast('ШУМ ПРИВЛЁК ВНИМАНИЕ · ШАГИ В ТЕМНОТЕ',1500); G.noise=1;
  }
  G.noise=Math.max(0,G.noise - dt*0.85);
  // filter dead
  G.entities=G.entities.filter(e=>e.x>-50);
}

let probeT=0;
function updateProbe(){
  probeT++;
  const tgt=getInteraction();
  G.interactionTarget=tgt;
  const prompt=document.getElementById('prompt');
  if(!prompt) return;
  if(tgt){
    if(tgt.kind==='door'){
      const d=tgt.door;
      prompt.textContent=d.open?'[E] ЗАКРЫТЬ · '+d.label : (d.locked?'ЗАПЕРТО · '+d.label+' [E]':'[E] ОТКРЫТЬ · '+d.label);
      prompt.classList.add('on');
    } else if(tgt.kind==='doc'){
      prompt.textContent='[E] ПРОЧИТАТЬ · '+tgt.doc.title;
      prompt.classList.add('on');
    } else if(tgt.kind==='decision'){
      prompt.innerHTML='АНОМАЛИЯ? <span style="background:#c9a86a;color:#000;padding:2px 8px;border-radius:4px;margin:0 6px">[R] ДА</span> <span style="background:#ffffff22;padding:2px 8px;border-radius:4px">[T] НЕТ</span>';
      prompt.classList.add('on');
    } else if(tgt.kind==='inspect'){
      prompt.textContent='[E] ОСМОТРЕТЬ';
      prompt.classList.add('on');
    }
  } else {
    prompt.classList.remove('on');
  }
}

// sprites
function project(x,y,z){
  const sy=Math.sin(G.yaw), cy=Math.cos(G.yaw), sp=Math.sin(G.pitch), cp=Math.cos(G.pitch);
  const fx=sy*cp, fy=sp, fz=cy*cp, rx=cy, rz=-sy, ux=-sy*sp, uy=cp, uz=-cy*sp;
  const dx=x-G.x, dy=y-G.y, dz=z-G.z;
  const cz=dx*fx+dy*fy+dz*fz, cx=dx*rx+dz*rz, cyy=dx*ux+dy*uy+dz*uz;
  const tanX=FOV, tanY=FOV*VH/VW;
  const ndcX=(cx/cz)/tanX, ndcY=(cyy/cz)/tanY;
  return {sx:(ndcX+1)/2*VW, sy:(1-ndcY)/2*VH, cz, cx, cyy};
}
function depthAt(nx,ny){
  const x=clamp(nx*RW|0,0,RW-1), y=clamp(ny*RH|0,0,RH-1);
  return dep[y*RW+x];
}
function drawSprites(time){
  // props sprites
  for(const p of world.props){
    if(p.hidden) continue;
    // only draw if room matches and not occluded? We'll draw all within 13m
    const dx=p.x-G.x, dz=p.z-G.z, d=Math.hypot(dx,dz);
    if(d>14) continue;
    if(rayOccluded(G.x,G.z,p.x,p.z) && d>3.5) continue;
    const pr=project(p.x, p.y||1.0, p.z);
    if(pr.cz<0.25) continue;
    const sz=Math.min(VH*1.2, VH*0.55/pr.cz);
    let behind=pr.cz> depthAt(pr.sx/VW, pr.sy/VH)+0.4;
    vctx.save();
    vctx.globalAlpha= behind?0.25:0.95;
    const x=pr.sx, y=pr.sy;
    // draw by kind
    if(p.kind==='painting'){
      const isAnom=p.variant===1;
      vctx.fillStyle=isAnom?'#4a1212':'#2a2418';
      vctx.fillRect(x-sz*0.32, y-sz*0.22, sz*0.64, sz*0.44);
      vctx.strokeStyle=isAnom?'#a32222':'#c9a86a';
      vctx.lineWidth=2*DPR; vctx.strokeRect(x-sz*0.32, y-sz*0.22, sz*0.64, sz*0.44);
      if(isAnom){
        // eyes
        vctx.fillStyle='#0a0a0a';
        vctx.beginPath(); vctx.arc(x-sz*0.1, y, sz*0.08,0,7); vctx.arc(x+sz*0.1, y, sz*0.08,0,7); vctx.fill();
        vctx.fillStyle='#c9a86a'; vctx.beginPath(); vctx.arc(x-sz*0.1, y, sz*0.03,0,7); vctx.arc(x+sz*0.1, y, sz*0.03,0,7); vctx.fill();
        if(Math.sin(time*3)>0.5){ vctx.fillStyle='#ff0000'; vctx.fillRect(x-sz*0.12, y-sz*0.01, sz*0.24, sz*0.02); } // slit
      } else {
        // landscape: simple hills
        vctx.fillStyle='#3e5a3a'; vctx.beginPath(); vctx.moveTo(x-sz*0.32, y+sz*0.12); vctx.lineTo(x, y-sz*0.05); vctx.lineTo(x+sz*0.32, y+sz*0.12); vctx.lineTo(x+sz*0.32, y+sz*0.22); vctx.lineTo(x-sz*0.32, y+sz*0.22); vctx.closePath(); vctx.fill();
        vctx.fillStyle='#6a9ab0'; vctx.fillRect(x-sz*0.32, y-sz*0.22, sz*0.64, sz*0.16);
      }
    } else if(p.kind==='chair'){
      vctx.fillStyle='#5a3e28'; vctx.fillRect(x-sz*0.12, y-sz*0.05, sz*0.24, sz*0.22);
      vctx.fillStyle='#6e4a30'; vctx.fillRect(x-sz*0.10, y-sz*0.14, sz*0.20, sz*0.12);
    } else if(p.kind==='table'){
      vctx.fillStyle='#4a3522'; vctx.fillRect(x-sz*0.20, y, sz*0.40, sz*0.06);
      vctx.fillRect(x-sz*0.03, y, sz*0.06, sz*0.20);
    } else if(p.kind==='vase'){
      vctx.fillStyle='#8a6a3e'; vctx.beginPath(); vctx.ellipse(x,y,sz*0.07,sz*0.11,0,0,7); vctx.fill();
      vctx.fillStyle='#c9a86a'; vctx.beginPath(); vctx.ellipse(x,y-sz*0.06,sz*0.03,sz*0.04,0,0,7); vctx.fill();
    } else if(p.kind==='lamp_stand'){ vctx.fillStyle='#1a1a1a'; vctx.fillRect(x-2, y-sz*0.1, 4, sz*0.3); vctx.fillStyle='#c9a86a55'; vctx.beginPath(); vctx.arc(x,y-sz*0.12, sz*0.12,0,7); vctx.fill(); }
    else if(p.kind==='sofa'){ vctx.fillStyle='#3a2820'; vctx.fillRect(x-sz*0.22, y-sz*0.02, sz*0.44, sz*0.14); vctx.fillStyle='#4a3528'; vctx.fillRect(x-sz*0.18, y-sz*0.10, sz*0.36, sz*0.10); }
    else if(p.kind==='window'){
      // window frame already handled in walls, but draw glow
      if(G.windowViews['5,7']===1 && p.id==='window1'){ /* brick handled */ }
      else { vctx.fillStyle='#0b1a2a44'; vctx.fillRect(x-sz*0.14, y-sz*0.12, sz*0.28, sz*0.22); }
      if(p.entity){
        // face in window
        vctx.fillStyle='#e8dcc2'; vctx.beginPath(); vctx.arc(x,y,sz*0.09,0,7); vctx.fill();
        vctx.fillStyle='#0a0a0a'; vctx.beginPath(); vctx.arc(x-sz*0.03,y-sz*0.01,sz*0.02,0,7); vctx.arc(x+sz*0.03,y-sz*0.01,sz*0.02,0,7); vctx.fill();
      }
    } else if(p.kind==='ceiling_light'){ /* ceiling is voxel */ }
    else if(p.kind==='wallpaper'){ /* wall voxel */ }
    else if(p.kind==='bathtub'){ vctx.fillStyle='#d8ddd6'; vctx.fillRect(x-sz*0.18, y, sz*0.36, sz*0.12); vctx.strokeStyle='#aab'; vctx.strokeRect(x-sz*0.18,y,sz*0.36,sz*0.12); }
    else if(p.kind==='bed'){ vctx.fillStyle='#4a3a3a'; vctx.fillRect(x-sz*0.20, y, sz*0.40, sz*0.16); vctx.fillStyle='#8a6a5a'; vctx.fillRect(x-sz*0.16, y-sz*0.04, sz*0.32, sz*0.10); }
    else if(p.kind==='doll'){ const sc=p.scale||1; vctx.fillStyle=p.creepy?'#b03030':'#c9a86a'; vctx.beginPath(); vctx.arc(x,y,sz*0.07*sc,0,7); vctx.fill(); vctx.fillRect(x-sz*0.04*sc, y, sz*0.08*sc, sz*0.14*sc); if(p.creepy){ vctx.fillStyle='#000'; vctx.beginPath(); vctx.arc(x-sz*0.02,y-sz*0.01,sz*0.015,0,7); vctx.arc(x+sz*0.02,y-sz*0.01,sz*0.015,0,7); vctx.fill(); } }
    else { vctx.fillStyle='#c9a86a'; vctx.fillRect(x-3, y-3, 6,6); }
    vctx.restore();
  }
  // documents sprites
  for(const d of world.documents){
    if(d.found) continue;
    const dx=d.x-G.x, dz=d.z-G.z; if(Math.hypot(dx,dz)>12) continue;
    if(rayOccluded(G.x,G.z,d.x,d.z)) continue;
    const pr=project(d.x,0.9,d.z); if(pr.cz<0.3) continue;
    if(pr.cz> depthAt(pr.sx/VW, pr.sy/VH)+0.5) continue;
    const sz=Math.min(VH*0.9,VH*0.45/pr.cz);
    vctx.save(); vctx.globalAlpha=0.9;
    vctx.fillStyle='#e8e0c8'; vctx.fillRect(pr.sx-sz*0.12, pr.sy-sz*0.10, sz*0.24, sz*0.18);
    vctx.strokeStyle='#c9a86a'; vctx.lineWidth=1; vctx.strokeRect(pr.sx-sz*0.12, pr.sy-sz*0.10, sz*0.24, sz*0.18);
    vctx.fillStyle='#1a1a1a'; vctx.font=`${Math.round(sz*0.08)}px monospace`; vctx.textAlign='center'; vctx.fillText('!',pr.sx, pr.sy);
    // glow
    vctx.shadowColor='#c9a86a'; vctx.shadowBlur=12*DPR; vctx.strokeRect(pr.sx-sz*0.12, pr.sy-sz*0.10, sz*0.24, sz*0.18);
    vctx.restore();
  }
  // entities sprites
  for(const e of G.entities){
    const dx=e.x-G.x, dz=e.z-G.z; const d=Math.hypot(dx,dz);
    if(d>16) continue;
    if(e.kind==='shadow' && e.time>2) continue;
    // occlusion for entities: if wall between, don't draw (unless they can be seen through door)
    if(rayOccluded(G.x,G.z,e.x,e.z) && d>2.5) continue;
    const pr=project(e.x, e.y||1.3, e.z); if(pr.cz<0.3) continue;
    const sz=Math.min(VH*1.6, VH*0.65/pr.cz);
    vctx.save();
    const x=pr.sx, y=pr.sy;
    if(e.kind==='figure'){
      // tall dark figure at end
      const alpha= e.alpha!==undefined?e.alpha: (0.85*Math.max(0,1-d/14));
      vctx.globalAlpha=alpha;
      vctx.fillStyle='#05070a';
      vctx.fillRect(x-sz*0.09, y-sz*0.42, sz*0.18, sz*0.65);
      // head
      vctx.beginPath(); vctx.ellipse(x, y-sz*0.38, sz*0.07, sz*0.09,0,0,7); vctx.fill();
      // eyes faint
      if(d<10){ vctx.fillStyle='#c9a86a'; vctx.globalAlpha=0.6; vctx.beginPath(); vctx.arc(x-sz*0.02,y-sz*0.38,sz*0.015,0,7); vctx.arc(x+sz*0.02,y-sz*0.38,sz*0.015,0,7); vctx.fill(); }
    } else if(e.kind==='follower'){
      // observer that follows: hunched silhouette
      const alpha=e.visible?0.9:0.45;
      vctx.globalAlpha=alpha;
      // flicker when not seen?
      if(!e.visible && Math.sin(time*10)>0.5) vctx.globalAlpha=0.15; // blink
      vctx.fillStyle='#0a0c0f';
      vctx.fillRect(x-sz*0.10, y-sz*0.30, sz*0.20, sz*0.48);
      vctx.beginPath(); vctx.ellipse(x, y-sz*0.28, sz*0.09, sz*0.11,0,0,7); vctx.fill();
      // elongated limbs
      vctx.fillRect(x-sz*0.14, y-sz*0.10, sz*0.06, sz*0.32);
      vctx.fillRect(x+sz*0.08, y-sz*0.10, sz*0.06, sz*0.32);
      if(e.state==='hunt' && d<8){
        // more tense
        vctx.strokeStyle='#c9a86a44'; vctx.lineWidth=1; vctx.strokeRect(x-sz*0.12, y-sz*0.32, sz*0.24, sz*0.50);
      }
    } else if(e.kind==='door_peek'){
      vctx.globalAlpha=0.92;
      vctx.fillStyle='#0a0a0a';
      // slit of door
      vctx.fillRect(x-sz*0.08, y-sz*0.20, sz*0.16, sz*0.40);
      // face half visible
      vctx.fillStyle='#d8c9a8';
      vctx.beginPath(); vctx.arc(x-sz*0.01, y-sz*0.05, sz*0.07, -0.6, 0.6); vctx.fill();
      vctx.fillStyle='#000'; vctx.beginPath(); vctx.arc(x+0.01,y-sz*0.05,sz*0.018,0,7); vctx.fill();
    } else if(e.kind==='shadow'){
      vctx.globalAlpha=0.45;
      vctx.fillStyle='#000000';
      vctx.fillRect(x-sz*0.30, y-sz*0.25, sz*0.60, sz*0.12);
      vctx.globalAlpha=0.25; vctx.fillRect(x-sz*0.50, y-sz*0.25, sz*1.0, sz*0.12);
    } else if(e.kind==='window_watcher'){
      vctx.globalAlpha=0.85;
      vctx.fillStyle='#e8dcc2'; vctx.beginPath(); vctx.arc(x,y,sz*0.08,0,7); vctx.fill();
    }
    vctx.restore();
  }
  // decision zone marker
  if(G.z>17 && G.z<19.5 && G.x>13 && G.x<18 && !G.decided){
    const pr=project(15.5,1.5,18.8); if(pr.cz>0.3 && pr.cz<10){
      vctx.save(); vctx.globalAlpha=0.85; vctx.fillStyle='#c9a86a'; vctx.font=`${Math.round(11*DPR)}px monospace`; vctx.textAlign='center'; vctx.fillText('РЕШИ · R / T',pr.sx, pr.sy); vctx.restore();
    }
  }
}

// frame
let lastT=performance.now(), frameMs=14, frames=0, scaleTimer=0;
let bloomOn=true, bloomOk=!matchMedia('(pointer:coarse)').matches;
function frame(now){
  let dt=(now-lastT)/1000; lastT=now; if(dt>0.05) dt=0.05; const time=now/1000;
  update(dt);
  const shakeAmp=G.shake*0.011, shx=(Math.sin(time*71)*0.6+(Math.random()-0.5))*shakeAmp*VW, shy=(Math.cos(time*83)*0.6+(Math.random()-0.5))*shakeAmp*VH;
  const camY=G.y+Math.sin(G.bob)*0.03;
  const fov=FOV*(1+G.punch*0.14);
  const t0=performance.now();
  // flicker flashlight
  let flashOn=G.flashOn;
  if(G.flashFlicker>0) flashOn=Math.sin(time*32)>0;
  Render.render(world,{fov,x:G.x,y:camY,z:G.z,yaw:G.yaw,pitch:G.pitch,w:0,occ,P,lamps:world.lamps,flashOn,emergency:G.emergency,corridorScale:G.corridorScale,windowViews:G.windowViews,wallpaperVariant:G.wallpaperVariant,lightShift:G.lightShift,lightFlicker:G.lightFlicker},RW,RH,pix,dep,time);
  lctx.putImageData(img,0,0);
  const tRender=performance.now()-t0;
  vctx.save(); vctx.imageSmoothingEnabled=false; vctx.translate(shx,shy);
  const gl=G.glitch;
  if(gl>0.02){
    const bands=12, bh=VH/bands;
    for(let i=0;i<bands;i++){
      const off=(Math.random()-0.5)*gl*52*DPR*(0.4+Math.random());
      const h=bh+1; const sy=i*bh*RH/VH, sh=Math.min(RH-sy, h*RH/VH);
      vctx.drawImage(low,0,sy,RW,sh,off,i*bh,VW,h);
    }
    vctx.globalCompositeOperation='lighter'; vctx.globalAlpha=gl*0.32; vctx.drawImage(low,0,0,RW,RH,-6*DPR*gl,0,VW,VH); vctx.globalAlpha=gl*0.24; vctx.drawImage(low,0,0,RW,RH,6*DPR*gl,0,VW,VH); vctx.globalAlpha=1; vctx.globalCompositeOperation='source-over';
  } else vctx.drawImage(low,0,0,RW,RH,0,0,VW,VH);
  vctx.restore();
  if(bloomOn&&bloomOk){
    vctx.save(); vctx.globalCompositeOperation='lighter'; vctx.globalAlpha=0.18;
    try{vctx.filter='blur('+Math.round(6*DPR)+'px)';}catch(e){}
    vctx.drawImage(low,0,0,RW,RH,0,0,VW,VH); vctx.filter='none'; vctx.restore();
  }
  drawSprites(time);
  // flashlight cone overlay (screen)
  if(G.flashOn){
    vctx.save(); vctx.globalCompositeOperation='lighter'; vctx.globalAlpha=0.05;
    const grad=vctx.createRadialGradient(VW/2,VH/2,0,VW/2,VH/2,VW*0.65);
    grad.addColorStop(0,'#fff8cc00'); grad.addColorStop(0.45,'#fff2a522'); grad.addColorStop(1,'#00000000');
    vctx.fillStyle=grad; vctx.fillRect(0,0,VW,VH); vctx.restore();
  } else {
    vctx.save(); vctx.fillStyle='#00000088'; vctx.fillRect(0,0,VW,VH); vctx.restore();
  }
  // sanity vignette when low
  if(G.sanity<50){
    const v=Math.min(0.55,(50-G.sanity)/50 *0.55);
    vctx.save(); vctx.fillStyle=`rgba(18,4,4,${v})`; vctx.fillRect(0,0,VW,VH);
    // breathing pulse
    const pul=Math.sin(time*1.9)*0.08+0.92;
    vctx.globalAlpha=v*0.5*pul; vctx.strokeStyle='#a32222'; vctx.lineWidth=2*DPR; vctx.strokeRect(12,12,VW-24,VH-24);
    vctx.restore();
  }
  // minimap (optional)
  drawMap();
  frameMs+=(tRender-frameMs)*0.08; frames++; scaleTimer++;
  if(scaleTimer>45){ scaleTimer=0; if(frameMs>22 && scale>0.28) allocScale(scale*0.9); else if(frameMs<11 && scale<0.62) allocScale(scale*1.06); }
  if(frames===120){ const fps=1000/(performance.now()-fpsRef); if(fps<36) bloomOk=false; document.getElementById('fps').textContent='Σ '+scale.toFixed(2)+'x · '+frameMs.toFixed(1)+'ms'; }
  requestAnimationFrame(frame);
}
let fpsRef=performance.now();

// minimap
const mapC=document.getElementById('map'), mctx=mapC.getContext('2d');
let mapTick=0;
function drawMap(){
  if(mapTick++%5!==0) return;
  const S0=mapC.width, cs=S0/N;
  mctx.clearRect(0,0,S0,S0); mctx.fillStyle='#0a1418'; mctx.fillRect(0,0,S0,S0);
  for(let z=0;z<N;z++)for(let x=0;x<N;x++){
    const v=grid[world.idx(x,1,z)]; const solid=(v===1||v===2);
    if(solid){ mctx.fillStyle='#d0d8dc'; mctx.fillRect(x*cs,z*cs,cs+0.6,cs+0.6); }
    else { // door highlight?
      for(const d of world.baseDoors){ if(Math.floor(d.x)===x && Math.floor(d.z)===z){ mctx.fillStyle=d.open?'#c9a86a':'#a32222'; mctx.globalAlpha=0.6; mctx.fillRect(x*cs,z*cs,cs+0.6,cs+0.6); mctx.globalAlpha=1; } }
    }
  }
  // docs
  for(const d of world.documents) if(!d.found){ mctx.fillStyle='#c9a86a'; mctx.globalAlpha=0.9; mctx.beginPath(); mctx.arc(d.x*cs,d.z*cs,cs*0.7,0,7); mctx.fill(); mctx.globalAlpha=1; }
  // entities
  for(const e of G.entities){ mctx.fillStyle='#ff2a2a'; mctx.beginPath(); mctx.arc(e.x*cs,e.z*cs,cs*0.9,0,7); mctx.fill(); }
  mctx.fillStyle='#fff'; mctx.save(); mctx.translate(G.x*cs,G.z*cs); mctx.rotate(-G.yaw+Math.PI); mctx.beginPath(); mctx.moveTo(0,-cs*1.1); mctx.lineTo(cs*0.8,cs*0.9); mctx.lineTo(-cs*0.8,cs*0.9); mctx.closePath(); mctx.fill(); mctx.restore();
}

// boot
resize(); resetGame(); updateHUD(); requestAnimationFrame(frame);
})();
