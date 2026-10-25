'use strict';
/* ============================================================
   ТЕССЕРАКТ — первая 4D-игра.
   Мир настоящий четырёхмерный: у каждого объекта есть (x,y,z,w).
   Игрок вращает вселенную в плоскости X-W (Q/E), а рендер — это
   перспективная проекция 4D -> 3D (как тень на стену).
   ============================================================ */

// ---------- Утилиты ----------
const $ = id => document.getElementById(id);
const clamp = (v,a,b)=>Math.max(a,Math.min(b,v));
const lerp = (a,b,t)=>a+(b-a)*t;
const rand = (a,b)=>a+Math.random()*(b-a);
const TAU = Math.PI*2;
function normAngle(a){ while(a>Math.PI)a-=TAU; while(a<-Math.PI)a+=TAU; return a; }
function rot2(a,b,ang){ const c=Math.cos(ang), s=Math.sin(ang); return [a*c-b*s, a*s+b*c]; }

// ---------- Константы мира ----------
const D4 = 26;              // дистанция 4D-"камеры" для проекции
const D_FAR = 175;          // для звёзд и фоновых тессерактов
const SOLID_W = 2.6;        // объект "в нашем 3D", если |w'| <= SOLID_W
const ARENA_R = 18.5, Y_MIN = -7.6, Y_MAX = 9.5;
const GAME_TIME = 300;      // ровно 5 минут на всю вселенную

// ---------- Звук (весь синтезируется, без файлов) ----------
const Snd = {
  ctx:null, master:null, muted:false, rotOsc:null, rotGain:null, rotFilter:null,
  init(){
    if(this.ctx){ if(this.ctx.state==='suspended') this.ctx.resume(); return; }
    try{
      const AC = window.AudioContext||window.webkitAudioContext; if(!AC) return;
      this.ctx = new AC();
      this.master = this.ctx.createGain(); this.master.gain.value = 0.5;
      this.master.connect(this.ctx.destination);
      // космический фон: два расстроенных осциллятора
      const g = this.ctx.createGain(); g.gain.value = 0.035;
      const f = this.ctx.createBiquadFilter(); f.type='lowpass'; f.frequency.value=320;
      [54,54.6,108.3].forEach(fr=>{ const o=this.ctx.createOscillator(); o.type='sine'; o.frequency.value=fr; o.connect(f); o.start(); });
      f.connect(g); g.connect(this.master);
      // гул вращения в W
      this.rotOsc = this.ctx.createOscillator(); this.rotOsc.type='sawtooth'; this.rotOsc.frequency.value=55;
      this.rotFilter = this.ctx.createBiquadFilter(); this.rotFilter.type='bandpass'; this.rotFilter.frequency.value=300; this.rotFilter.Q.value=2.5;
      this.rotGain = this.ctx.createGain(); this.rotGain.gain.value=0;
      this.rotOsc.connect(this.rotFilter); this.rotFilter.connect(this.rotGain); this.rotGain.connect(this.master);
      this.rotOsc.start();
    }catch(e){}
  },
  setRot(intensity, theta){
    if(!this.ctx||!this.rotGain) return;
    const t=this.ctx.currentTime;
    this.rotGain.gain.setTargetAtTime(this.muted?0:clamp(intensity,0,1)*0.14, t, 0.06);
    const fr = 180 + ((theta*130)%520+520)%520;
    this.rotFilter.frequency.setTargetAtTime(fr, t, 0.05);
    this.rotOsc.frequency.setTargetAtTime(40+intensity*40, t, 0.05);
  },
  tone(freq,dur,type,g,slideTo,delay){
    if(!this.ctx||this.muted) return;
    type=type||'sine'; g=g||0.2; delay=delay||0;
    const t0=this.ctx.currentTime+delay;
    const o=this.ctx.createOscillator(), gn=this.ctx.createGain();
    o.type=type; o.frequency.setValueAtTime(freq,t0);
    if(slideTo) o.frequency.exponentialRampToValueAtTime(Math.max(20,slideTo), t0+dur);
    gn.gain.setValueAtTime(0,t0); gn.gain.linearRampToValueAtTime(g,t0+0.012);
    gn.gain.exponentialRampToValueAtTime(0.0008,t0+dur);
    o.connect(gn); gn.connect(this.master); o.start(t0); o.stop(t0+dur+0.05);
  },
  noise(dur,g){
    if(!this.ctx||this.muted) return;
    const n=Math.floor(this.ctx.sampleRate*dur), buf=this.ctx.createBuffer(1,n,this.ctx.sampleRate), d=buf.getChannelData(0);
    for(let i=0;i<n;i++) d[i]=(Math.random()*2-1)*(1-i/n);
    const src=this.ctx.createBufferSource(); src.buffer=buf;
    const f=this.ctx.createBiquadFilter(); f.type='lowpass';
    f.frequency.setValueAtTime(2600,this.ctx.currentTime);
    f.frequency.exponentialRampToValueAtTime(90,this.ctx.currentTime+dur);
    const gn=this.ctx.createGain(); gn.gain.value=g;
    src.connect(f); f.connect(gn); gn.connect(this.master); src.start();
  },
  pickup(n){
    const scale=[523.25,587.33,659.25,783.99,880,1046.5,1174.66,1318.5,1567.98];
    const f=scale[clamp(n,0,scale.length-1)];
    this.tone(f,0.4,'sine',0.26); this.tone(f*2,0.3,'sine',0.1); this.tone(f*1.5,0.35,'triangle',0.12,null,0.03);
  },
  solidify(){ this.tone(1180,0.16,'sine',0.11,1960); },
  damage(){ this.noise(0.32,0.4); this.tone(110,0.45,'sawtooth',0.3,38); },
  portalOpen(){ [392,523.25,659.25,783.99,1046.5].forEach((f,i)=>this.tone(f,0.7,'triangle',0.2,null,i*0.09)); },
  levelClear(){ [659.25,783.99,987.77,1318.5].forEach((f,i)=>this.tone(f,0.5,'sine',0.22,null,i*0.08)); this.noise(0.5,0.08); },
  win(){ [523.25,659.25,783.99,1046.5,1318.5,1567.98,2093].forEach((f,i)=>this.tone(f,0.8,'triangle',0.22,null,i*0.11)); },
  lose(){ [392,329.63,261.63,196,130.81].forEach((f,i)=>this.tone(f,0.7,'sawtooth',0.14,null,i*0.16)); this.noise(1.1,0.15); },
  tick(){ this.tone(980,0.05,'square',0.06); },
  click(){ this.tone(700,0.08,'sine',0.12,990); }
};

// ---------- Состояние игры ----------
let state = 'title'; // title | playing | paused | ended
let levelIdx = 0, score = 0, hp = 3, timeLeft = GAME_TIME;
let theta = 0, thetaRateSmooth = 0, wheelAcc = 0;
let totalCollected = 0, combo = 0, invuln = 0, shake = 0;
let elapsed = 0, lastTickSec = -1, solidifyBlipCd = 0;
let hasMoved=false, hasRotated=false, sawSolid=false;
let portalUnlocked = false, levelTime = 0;

const LEVELS = [
  {name:'Глубина I · Пробуждение',     crystals:4, mines:2, driftAmp:0,    driftSpd:0.35, orbit:0,    thetaDrift:0},
  {name:'Глубина II · Дрейф в W',      crystals:6, mines:5, driftAmp:0.55, driftSpd:0.4,  orbit:0,    thetaDrift:0},
  {name:'Глубина III · Коллапс',       crystals:8, mines:8, driftAmp:0.35, driftSpd:0.5,  orbit:0.11, thetaDrift:0.05}
];

// ---------- Three.js сцена ----------
const canvas = $('scene');
const renderer = new THREE.WebGLRenderer({canvas, antialias:true, powerPreference:'high-performance'});
renderer.setPixelRatio(Math.min(window.devicePixelRatio,2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.outputEncoding = THREE.sRGBEncoding;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.15;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x02040c);
scene.fog = new THREE.FogExp2(0x02040c, 0.011);

const camera = new THREE.PerspectiveCamera(68, window.innerWidth/window.innerHeight, 0.1, 600);
camera.position.set(0,8,26);

scene.add(new THREE.HemisphereLight(0x8fa8ff, 0x0a0a22, 0.85));
const sun = new THREE.DirectionalLight(0xffffff, 0.65); sun.position.set(12,24,10); scene.add(sun);
scene.add(new THREE.AmbientLight(0x223355, 0.7));

// свечение-спрайт (рисуем сами)
function makeGlowTexture(){
  const c=document.createElement('canvas'); c.width=c.height=128;
  const g=c.getContext('2d'), gr=g.createRadialGradient(64,64,2,64,64,64);
  gr.addColorStop(0,'rgba(255,255,255,1)'); gr.addColorStop(0.25,'rgba(255,255,255,0.55)');
  gr.addColorStop(0.6,'rgba(255,255,255,0.16)'); gr.addColorStop(1,'rgba(255,255,255,0)');
  g.fillStyle=gr; g.fillRect(0,0,128,128);
  return new THREE.CanvasTexture(c);
}
const glowTex = makeGlowTexture();
function makeGlow(color, scale, opacity){
  const m=new THREE.SpriteMaterial({map:glowTex,color,transparent:true,opacity,blending:THREE.AdditiveBlending,depthWrite:false,fog:false});
  const s=new THREE.Sprite(m); s.scale.set(scale,scale,1); return s;
}

// ---------- 4D математика ----------
function project4(x,y,z,w,D){
  let s = D/(D-w); s = clamp(s,0.04,4);
  return {x:x*s, y:y*s, z:z*s, s};
}
function deltaToSlice(p4){ // насколько довернуть theta, чтобы объект лёг в W=0
  const phi = Math.atan2(p4.w, p4.x);
  const d1 = normAngle(-phi - theta), d2 = normAngle(Math.PI - phi - theta);
  return Math.abs(d1) < Math.abs(d2) ? d1 : d2;
}

// тессеракт (гиперкуб): 16 вершин, 32 ребра
function makeTesseract(size,color,opacity){
  const verts=[];
  for(let i=0;i<16;i++) verts.push([(i&1?size:-size),(i&2?size:-size),(i&4?size:-size),(i&8?size:-size)]);
  const edges=[];
  for(let i=0;i<16;i++) for(let b=0;b<4;b++){ const j=i^(1<<b); if(j>i) edges.push([i,j]); }
  const geo=new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(edges.length*6),3));
  const mat=new THREE.LineBasicMaterial({color,transparent:true,opacity,fog:false});
  const line=new THREE.LineSegments(geo,mat);
  line.frustumCulled=false;
  line.userData={verts,edges};
  return line;
}
function updateTesseract(line, rots, D, center4){
  // rots: список [осьA, осьB, угол]; оси 0=x 1=y 2=z 3=w. center4 добавляется ПОСЛЕ локальных вращений, затем глобальный XW(theta)
  const {verts,edges}=line.userData;
  const pos=line.geometry.attributes.position, arr=pos.array;
  const proj=new Array(16);
  for(let i=0;i<16;i++){
    let v=verts[i].slice();
    for(const r of rots){ const [a,b]=rot2(v[r[0]],v[r[1]],r[2]); v[r[0]]=a; v[r[1]]=b; }
    if(center4){ v[0]+=center4.x; v[1]+=center4.y; v[2]+=center4.z; v[3]+=center4.w;
      const [x1,w1]=rot2(v[0],v[3],theta); v[0]=x1; v[3]=w1; }
    proj[i]=project4(v[0],v[1],v[2],v[3],D);
  }
  let k=0;
  for(const e of edges){ const a=proj[e[0]],b=proj[e[1]];
    arr[k++]=a.x;arr[k++]=a.y;arr[k++]=a.z; arr[k++]=b.x;arr[k++]=b.y;arr[k++]=b.z; }
  pos.needsUpdate=true;
}

// ---------- Фон: гигантские тессеракты, 4D-звёзды, 4D-пыль ----------
const decorTess = [
  {line:makeTesseract(27,0x1e6f9e,0.5),  sp:[0.05,0.037,0.021]},
  {line:makeTesseract(40,0x4a2f8f,0.38), sp:[-0.033,0.05,0.028]},
  {line:makeTesseract(56,0x14557a,0.3),  sp:[0.022,-0.04,0.05]}
];
decorTess.forEach(d=>scene.add(d.line));

function makePointCloud(list4,size,color,opacity){
  const geo=new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(list4.length*3),3));
  const mat=new THREE.PointsMaterial({size,color,map:glowTex,transparent:true,opacity,
    blending:THREE.AdditiveBlending,depthWrite:false,sizeAttenuation:true,fog:false});
  const pts=new THREE.Points(geo,mat); pts.frustumCulled=false; scene.add(pts);
  return {pts,list4};
}
const starData=[];
for(let i=0;i<550;i++){
  let v=[rand(-1,1),rand(-1,1),rand(-1,1),rand(-1,1)];
  const l=Math.hypot(v[0],v[1],v[2],v[3])||1, R=rand(75,120);
  starData.push(v.map(x=>x/l*R));
}
const stars = makePointCloud(starData,1.9,0xaedcff,0.9);
const dustData=[];
for(let i=0;i<330;i++){
  let x,w,tries=0;
  do{ x=rand(-17,17); w=rand(-11,11); tries++; }while(Math.hypot(x,w)>20.5 && tries<20);
  dustData.push([x,rand(-7,9),rand(-17,17),w]);
}
const dust = makePointCloud(dustData,0.34,0x39e6ff,0.5);

function updateCloud(cloud, D, wzAngle, xwAngle){
  const arr=cloud.pts.geometry.attributes.position.array;
  const list=cloud.list4;
  for(let i=0;i<list.length;i++){
    let [x,y,z,w]=list[i];
    if(wzAngle){ const r=rot2(z,w,wzAngle); z=r[0]; w=r[1]; }
    const r=rot2(x,w,xwAngle); x=r[0]; w=r[1];
    const p=project4(x,y,z,w,D);
    arr[i*3]=p.x; arr[i*3+1]=p.y; arr[i*3+2]=p.z;
  }
  cloud.pts.geometry.attributes.position.needsUpdate=true;
}

// пол-нашего-среза и граница арены
const grid = new THREE.PolarGridHelper(19,16,8,48,0x1d5f7a,0x123a52);
grid.position.y=Y_MIN-0.4; grid.material.transparent=true; grid.material.opacity=0.55; scene.add(grid);
const boundsSphere = new THREE.Mesh(new THREE.SphereGeometry(20,24,16),
  new THREE.MeshBasicMaterial({color:0x2a7fff,wireframe:true,transparent:true,opacity:0.045,fog:false}));
scene.add(boundsSphere);
// "якоря 3D-среза" — статичные обелиски-ориентиры
[[15,0],[-15,0],[0,15],[0,-15]].forEach(p=>{
  const m=new THREE.Mesh(new THREE.BoxGeometry(1.1,17,1.1),
    new THREE.MeshStandardMaterial({color:0x0b1a30,roughness:0.4,metalness:0.4,emissive:0x06283d}));
  m.position.set(p[0],0.4,p[1]); scene.add(m);
  const e=new THREE.LineSegments(new THREE.EdgesGeometry(m.geometry),new THREE.LineBasicMaterial({color:0x2fb9d8,transparent:true,opacity:0.7}));
  e.position.copy(m.position); scene.add(e);
  const tip=makeGlow(0x39e6ff,3.4,0.5); tip.position.set(p[0],9.4,p[1]); scene.add(tip);
});

// ---------- Игрок: стеклянная сфера с живым гиперкубом внутри ----------
const player = {
  pos:new THREE.Vector3(0,0,11), vel:new THREE.Vector3(),
  group:new THREE.Group(), yaw:Math.PI, pitch:0.2
};
{
  const shell=new THREE.Mesh(new THREE.SphereGeometry(0.55,28,22),
    new THREE.MeshStandardMaterial({color:0x66d9ff,transparent:true,opacity:0.34,roughness:0.08,metalness:0.1,emissive:0x0a3d55}));
  player.shell=shell; player.group.add(shell);
  player.inner=makeTesseract(0.34,0x9ff2ff,0.95); player.group.add(player.inner);
  const core=new THREE.Mesh(new THREE.SphereGeometry(0.13,14,12),
    new THREE.MeshBasicMaterial({color:0xdffbff})); player.group.add(core);
  player.group.add(makeGlow(0x39c6ff,3.6,0.55));
  player.light=new THREE.PointLight(0x39c6ff,1.5,20,1.6); player.group.add(player.light);
  scene.add(player.group);
}
// след игрока
const TRAIL_N=80, trailHist=[];
const trailGeo=new THREE.BufferGeometry();
trailGeo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(TRAIL_N*3),3));
trailGeo.setAttribute('color', new THREE.BufferAttribute(new Float32Array(TRAIL_N*3),3));
const trail=new THREE.Line(trailGeo,new THREE.LineBasicMaterial({vertexColors:true,transparent:true,opacity:0.85,blending:THREE.AdditiveBlending,depthWrite:false}));
trail.frustumCulled=false; scene.add(trail);

// ---------- Сущности: кристаллы, мины, портал ----------
let crystals=[], mines=[], portal=null, effects=[];

function spawnP4(placed, minDist){
  for(let tries=0;tries<80;tries++){
    const rho=rand(7,17.5), phi=rand(0,TAU);
    const p={x:rho*Math.cos(phi), y:rand(-5,6.5), z:rand(-13,13), w:rho*Math.sin(phi), rho, phi};
    let ok=true;
    for(const q of placed){
      const d=Math.hypot(p.x-q.x,p.y-q.y,p.z-q.z,p.w-q.w);
      if(d<minDist){ ok=false; break; }
    }
    if(ok && Math.hypot(p.x,p.y,p.z-11,p.w)>6) return p;
  }
  const rho=rand(8,16), phi=rand(0,TAU);
  return {x:rho*Math.cos(phi), y:rand(-4,6), z:rand(-12,12), w:rho*Math.sin(phi), rho, phi};
}

function makeCrystal(p4, idx){
  const group=new THREE.Group();
  const coreMat=new THREE.MeshStandardMaterial({color:0x2fd9ff,emissive:0x1188bb,emissiveIntensity:1.4,
    roughness:0.15,metalness:0.2,transparent:true,opacity:0.2});
  const core=new THREE.Mesh(new THREE.OctahedronGeometry(0.85,0),coreMat); group.add(core);
  const innerMat=new THREE.MeshBasicMaterial({color:0xdcfbff,transparent:true,opacity:0.2,fog:false});
  const inner=new THREE.Mesh(new THREE.OctahedronGeometry(0.38,0),innerMat); group.add(inner);
  const glow=makeGlow(0x35c8ff,4.6,0.15); group.add(glow);
  const beamMat=new THREE.MeshBasicMaterial({color:0x35c8ff,transparent:true,opacity:0,blending:THREE.AdditiveBlending,depthWrite:false,side:THREE.DoubleSide,fog:false});
  const beam=new THREE.Mesh(new THREE.CylinderGeometry(0.09,0.09,34,8,1,true),beamMat); group.add(beam);
  scene.add(group);
  return {kind:'crystal',p4,group,core,inner,glow,coreMat,innerMat,beamMat,
    seed:Math.random()*100, idx, wasSolid:false, pos3:new THREE.Vector3(), w1:99, solid:false, dead:false};
}
function makeMine(p4, cfg){
  const group=new THREE.Group();
  const coreMat=new THREE.MeshStandardMaterial({color:0x54101f,emissive:0xff2244,emissiveIntensity:0.7,
    roughness:0.35,metalness:0.5,transparent:true,opacity:0.15});
  const core=new THREE.Mesh(new THREE.IcosahedronGeometry(0.8,0),coreMat); group.add(core);
  const spikeMat=new THREE.MeshBasicMaterial({color:0xff5577,wireframe:true,transparent:true,opacity:0.15,fog:false});
  const spikes=new THREE.Mesh(new THREE.IcosahedronGeometry(1.08,0),spikeMat); group.add(spikes);
  const glow=makeGlow(0xff3355,3.8,0.1); group.add(glow);
  scene.add(group);
  return {kind:'mine',p4,group,core,spikes,glow,coreMat,spikeMat,
    phi0:p4.phi, rho:p4.rho, driftAmp:cfg.driftAmp, driftSpd:cfg.driftSpd*rand(0.7,1.3),
    orbit:cfg.orbit*rand(0.7,1.4)*(Math.random()<0.5?-1:1), phase:rand(0,TAU),
    seed:Math.random()*100, pos3:new THREE.Vector3(), w1:99, solid:false, dead:false};
}
function makePortal(p4){
  const group=new THREE.Group();
  const coreMat=new THREE.MeshStandardMaterial({color:0x555f73,emissive:0x222a3a,emissiveIntensity:1,
    roughness:0.2,metalness:0.3,transparent:true,opacity:0.25});
  const core=new THREE.Mesh(new THREE.SphereGeometry(1.15,26,20),coreMat); group.add(core);
  const ringMat=new THREE.MeshBasicMaterial({color:0x8a94a8,transparent:true,opacity:0.3,fog:false});
  const ring1=new THREE.Mesh(new THREE.TorusGeometry(2.5,0.07,10,60),ringMat); group.add(ring1);
  const ring2=new THREE.Mesh(new THREE.TorusGeometry(2.9,0.05,10,60),ringMat); ring2.rotation.x=Math.PI/2; group.add(ring2);
  const glow=makeGlow(0x8899bb,9,0.15); group.add(glow);
  const light=new THREE.PointLight(0xffd54a,0,26,1.6); group.add(light);
  scene.add(group);
  const tess=makeTesseract(2.15,0x8a94a8,0.55); scene.add(tess);
  return {kind:'portal',p4,group,core,coreMat,ring1,ring2,ringMat,glow,light,tess,
    pos3:new THREE.Vector3(), w1:99, solid:false};
}

// ---------- Эффекты: взрывы частиц и кольца ----------
function burst(pos,color,count,speed){
  count=count||42; speed=speed||7;
  const arr=new Float32Array(count*3), vel=[];
  for(let i=0;i<count;i++){
    arr[i*3]=pos.x; arr[i*3+1]=pos.y; arr[i*3+2]=pos.z;
    const th=rand(0,TAU), ph=Math.acos(rand(-1,1)), sp=speed*rand(0.3,1);
    vel.push([sp*Math.sin(ph)*Math.cos(th),sp*Math.cos(ph),sp*Math.sin(ph)*Math.sin(th)]);
  }
  const geo=new THREE.BufferGeometry(); geo.setAttribute('position',new THREE.BufferAttribute(arr,3));
  const mat=new THREE.PointsMaterial({size:0.42,color,map:glowTex,transparent:true,opacity:1,blending:THREE.AdditiveBlending,depthWrite:false,fog:false});
  const pts=new THREE.Points(geo,mat); pts.frustumCulled=false; scene.add(pts);
  effects.push({type:'burst',obj:pts,vel,life:0,ttl:0.85});
  const ring=new THREE.Mesh(new THREE.RingGeometry(0.75,1,44),
    new THREE.MeshBasicMaterial({color,transparent:true,opacity:0.85,blending:THREE.AdditiveBlending,depthWrite:false,side:THREE.DoubleSide,fog:false}));
  ring.position.copy(pos); scene.add(ring);
  effects.push({type:'ring',obj:ring,life:0,ttl:0.55});
}
function updateEffects(dt){
  for(let i=effects.length-1;i>=0;i--){
    const e=effects[i]; e.life+=dt; const k=e.life/e.ttl;
    if(k>=1){ scene.remove(e.obj); if(e.obj.geometry)e.obj.geometry.dispose(); if(e.obj.material)e.obj.material.dispose(); effects.splice(i,1); continue; }
    if(e.type==='burst'){
      const arr=e.obj.geometry.attributes.position.array;
      for(let j=0;j<e.vel.length;j++){
        arr[j*3]+=e.vel[j][0]*dt; arr[j*3+1]+=e.vel[j][1]*dt; arr[j*3+2]+=e.vel[j][2]*dt;
        e.vel[j][0]*=(1-1.7*dt); e.vel[j][1]*=(1-1.7*dt); e.vel[j][2]*=(1-1.7*dt);
      }
      e.obj.geometry.attributes.position.needsUpdate=true;
      e.obj.material.opacity=1-k;
    }else{
      e.obj.scale.setScalar(0.5+k*7); e.obj.material.opacity=0.85*(1-k);
      e.obj.lookAt(camera.position);
    }
  }
}

// ---------- Генерация уровня ----------
function clearLevel(){
  for(const c of crystals) scene.remove(c.group);
  for(const m of mines) scene.remove(m.group);
  if(portal){ scene.remove(portal.group); scene.remove(portal.tess); }
  crystals=[]; mines=[]; portal=null;
}
function genLevel(idx){
  clearLevel();
  const cfg=LEVELS[idx], placed=[];
  for(let i=0;i<cfg.crystals;i++){
    let p;
    if(idx===0 && i===0){ // учебный кристалл: чуть в стороне в W, прямо по курсу
      const rho=11.5, phi=0.38;
      p={x:rho*Math.cos(phi), y:0.3, z:-2, w:rho*Math.sin(phi), rho, phi};
    } else p=spawnP4(placed,5.5);
    placed.push(p); crystals.push(makeCrystal(p,i));
  }
  for(let i=0;i<cfg.mines;i++){ const p=spawnP4(placed,5); placed.push(p); mines.push(makeMine(p,cfg)); }
  const pp=spawnP4(placed,7); placed.push(pp); portal=makePortal(pp);
  portalUnlocked=false; combo=0; levelTime=0;
  player.pos.set(0,0,11); player.vel.set(0,0,0); player.yaw=Math.PI; player.pitch=0.2;
  trailHist.length=0;
  updateHUDCounts();
  $('levelName').textContent=cfg.name;
}

// ---------- Ввод ----------
const keys={};
let pointerLocked=false, dragging=false, lastMX=0, lastMY=0;
const isTouch = ('ontouchstart' in window) || navigator.maxTouchPoints>0;
const touchIn={x:0,y:0,q:false,e:false,up:false,down:false};

window.addEventListener('keydown',e=>{
  keys[e.code]=true;
  if(state==='playing' && ['Space','KeyW','KeyA','KeyS','KeyD','KeyQ','KeyE','KeyC'].includes(e.code)) e.preventDefault();
  if(e.code==='KeyM') toggleMute();
  if(e.code==='KeyP'){ if(state==='playing') pauseGame(); else if(state==='paused') resumeGame(); }
  if(e.code==='Enter'){ if(state==='title') startGame(); else if(state==='ended') startGame(); else if(state==='paused') resumeGame(); }
});
window.addEventListener('keyup',e=>{ keys[e.code]=false; });
window.addEventListener('blur',()=>{ for(const k in keys) keys[k]=false; });
document.addEventListener('contextmenu',e=>e.preventDefault());

canvas.addEventListener('click',()=>{
  if(state==='playing' && !pointerLocked && !isTouch) lockPointer();
});
function lockPointer(){ try{ const p=canvas.requestPointerLock(); if(p&&p.catch)p.catch(()=>{}); }catch(e){} }
document.addEventListener('pointerlockchange',()=>{
  const was=pointerLocked; pointerLocked = document.pointerLockElement===canvas;
  if(was && !pointerLocked && state==='playing') pauseGame();
});
document.addEventListener('mousemove',e=>{
  if(state!=='playing') return;
  if(pointerLocked) rotateView(e.movementX||0, e.movementY||0);
  else if(dragging) rotateView(e.clientX-lastMX, e.clientY-lastMY);
  lastMX=e.clientX; lastMY=e.clientY;
});
canvas.addEventListener('mousedown',e=>{ dragging=true; lastMX=e.clientX; lastMY=e.clientY; });
window.addEventListener('mouseup',()=>{ dragging=false; });
function rotateView(dx,dy){
  player.yaw   -= dx*0.0026;
  player.pitch  = clamp(player.pitch + dy*0.0026, -1.15, 1.3);
}
window.addEventListener('wheel',e=>{
  if(state!=='playing') return;
  wheelAcc += clamp(e.deltaY,-120,120)*0.0019;
  hasRotated=true;
},{passive:true});

// сенсоры
if(isTouch){
  const joy=$('joy'), knob=$('joyKnob'); let joyId=null;
  const joySet=e=>{
    const r=joy.getBoundingClientRect(), cx=r.left+r.width/2, cy=r.top+r.height/2;
    let dx=(e.clientX-cx)/(r.width/2), dy=(e.clientY-cy)/(r.height/2);
    const l=Math.hypot(dx,dy); if(l>1){dx/=l;dy/=l;}
    touchIn.x=dx; touchIn.y=dy;
    knob.style.transform=`translate(calc(-50% + ${dx*34}px), calc(-50% + ${dy*34}px))`;
  };
  joy.addEventListener('pointerdown',e=>{ joyId=e.pointerId; joy.setPointerCapture(e.pointerId); joySet(e); });
  joy.addEventListener('pointermove',e=>{ if(e.pointerId===joyId) joySet(e); });
  const joyEnd=()=>{ joyId=null; touchIn.x=0; touchIn.y=0; knob.style.transform='translate(-50%,-50%)'; };
  joy.addEventListener('pointerup',joyEnd); joy.addEventListener('pointercancel',joyEnd);
  const bind=(id,key)=>{
    const el=$(id);
    el.addEventListener('pointerdown',e=>{ e.preventDefault(); touchIn[key]=true; if(key==='q'||key==='e') hasRotated=true; });
    const off=()=>{ touchIn[key]=false; };
    el.addEventListener('pointerup',off); el.addEventListener('pointerleave',off); el.addEventListener('pointercancel',off);
  };
  bind('tQ','q'); bind('tE','e'); bind('tUp','up'); bind('tDown','down');
  let camId=null, lx=0, ly=0;
  canvas.addEventListener('pointerdown',e=>{ if(e.pointerType==='touch'){ camId=e.pointerId; lx=e.clientX; ly=e.clientY; } });
  canvas.addEventListener('pointermove',e=>{
    if(e.pointerType==='touch' && e.pointerId===camId && state==='playing'){
      rotateView((e.clientX-lx)*2.4,(e.clientY-ly)*2.4); lx=e.clientX; ly=e.clientY;
    }
  });
  canvas.addEventListener('pointerup',e=>{ if(e.pointerId===camId) camId=null; });
  canvas.addEventListener('pointercancel',()=>{ camId=null; });
}

// ---------- HUD ----------
const hud={ c:$('crystalCount'), s:$('score'), h:$('hearts'), t:$('timer'), tw:$('timerWrap'),
  th:$('thetaRead'), hint:$('hint'), fill:$('wbarFill'), wd:$('wDepth'), marker:$('marker'), banner:$('banner') };
function updateHUDCounts(){
  const left=crystals.filter(c=>!c.dead).length, total=LEVELS[levelIdx].crystals;
  hud.c.textContent=(total-left)+' / '+total;
  hud.s.textContent=score;
  hud.h.textContent='❤'.repeat(Math.max(0,hp))+'🖤'.repeat(Math.max(0,3-hp));
}
let bannerTimer=null;
function showBanner(html,sub,dur){
  hud.banner.innerHTML=html+(sub?`<small>${sub}</small>`:'');
  hud.banner.classList.add('show');
  clearTimeout(bannerTimer);
  bannerTimer=setTimeout(()=>hud.banner.classList.remove('show'), dur||2600);
}
function flashWhite(){
  const f=$('flash'); f.style.transition='none'; f.style.opacity='0.9';
  requestAnimationFrame(()=>{ f.style.transition='opacity .7s'; f.style.opacity='0'; });
}
function dmgFlash(){
  const d=$('dmg'); d.style.transition='none'; d.style.opacity='1';
  requestAnimationFrame(()=>{ d.style.transition='opacity .6s'; d.style.opacity='0'; });
}
function fmtTime(sec){ sec=Math.max(0,Math.ceil(sec)); return Math.floor(sec/60)+':'+String(sec%60).padStart(2,'0'); }

// 4D-компас: плоскость X-W
const comp=$('compass').getContext('2d');
function drawCompass(){
  const S=156, C=S/2, K=(S/2-6)/20;
  comp.clearRect(0,0,S,S);
  // фон
  comp.beginPath(); comp.arc(C,C,S/2-2,0,TAU);
  comp.fillStyle='rgba(6,16,36,0.65)'; comp.fill();
  comp.strokeStyle='rgba(57,230,255,0.35)'; comp.lineWidth=1; comp.stroke();
  // полоса нашего 3D-среза
  comp.fillStyle='rgba(57,230,255,0.13)';
  comp.fillRect(4,C-SOLID_W*K,S-8,SOLID_W*K*2);
  comp.beginPath(); comp.moveTo(4,C); comp.lineTo(S-4,C);
  comp.strokeStyle='rgba(140,240,255,0.75)'; comp.stroke();
  comp.fillStyle='rgba(160,220,255,0.8)'; comp.font='9px sans-serif';
  comp.fillText('НАШ 3D →', S-58, C-4);
  comp.fillText('W ↑', 6, 13);
  // орбита ближайшей цели
  const tgt = portalUnlocked ? portal : nearestCrystal();
  if(tgt){
    const rho=Math.hypot(tgt.p4.x,tgt.p4.w);
    comp.beginPath(); comp.arc(C,C,rho*K,0,TAU);
    comp.setLineDash([3,4]); comp.strokeStyle='rgba(255,213,74,0.35)'; comp.stroke(); comp.setLineDash([]);
  }
  const plot=(x1,w1)=>[clamp(C+x1*K,5,S-5), clamp(C-w1*K,5,S-5)];
  for(const m of mines){ if(m.dead)continue; const [px,py]=plot(m.x1||0,m.w1||0);
    comp.fillStyle=m.solid?'#ff4d6d':'rgba(255,77,109,0.4)';
    comp.beginPath(); comp.moveTo(px,py-4); comp.lineTo(px+3.6,py+3); comp.lineTo(px-3.6,py+3); comp.closePath(); comp.fill(); }
  for(const c of crystals){ if(c.dead)continue; const [px,py]=plot(c.x1||0,c.w1||0);
    if(c.solid){ comp.beginPath(); comp.arc(px,py,8,0,TAU); comp.strokeStyle='rgba(80,255,190,0.8)'; comp.stroke(); }
    comp.beginPath(); comp.arc(px,py,c.solid?5:3.6,0,TAU);
    comp.fillStyle=c.solid?'#5cffc0':'rgba(57,230,255,0.55)'; comp.fill(); }
  if(portal){ const [px,py]=plot(portal.x1||0,portal.w1||0);
    comp.save(); comp.translate(px,py); comp.rotate(Math.PI/4);
    comp.fillStyle=portalUnlocked?(portal.solid?'#ffd54a':'rgba(255,213,74,0.6)'):'rgba(150,160,185,0.5)';
    comp.fillRect(-4,-4,8,8); comp.restore(); }
  // игрок (живёт на срезе W=0)
  const [px,py]=plot(player.pos.x,0);
  comp.beginPath(); comp.arc(px,py,4,0,TAU); comp.fillStyle='#ffffff'; comp.fill();
  comp.beginPath(); comp.arc(px,py,7,0,TAU); comp.strokeStyle='rgba(255,255,255,0.6)'; comp.stroke();
  // метка угла поворота
  const ax=C+Math.cos(-theta)*(S/2-9), ay=C+Math.sin(-theta)*(S/2-9);
  comp.beginPath(); comp.arc(ax,ay,3,0,TAU); comp.fillStyle='#b26bff'; comp.fill();
}
function nearestCrystal(){
  let best=null, bd=1e9;
  for(const c of crystals){ if(c.dead)continue; const d=Math.abs(c.w1); if(d<bd){bd=d;best=c;} }
  return best;
}
function nearestSolidCrystal(){
  let best=null, bd=1e9;
  for(const c of crystals){ if(c.dead||!c.solid)continue; const d=player.pos.distanceTo(c.pos3); if(d<bd){bd=d;best=c;} }
  return best;
}

// экранный маркер цели
const _v3=new THREE.Vector3(), _camDir=new THREE.Vector3();
function updateMarker(){
  let target=null, isPortal=false;
  if(portalUnlocked && portal){ target=portal; isPortal=true; }
  else target=nearestSolidCrystal();
  if(!target||state!=='playing'){ hud.marker.classList.add('hidden'); return; }
  camera.getWorldDirection(_camDir);
  const dir=target.pos3.clone().sub(camera.position);
  const behind=dir.dot(_camDir)<0;
  _v3.copy(target.pos3).project(camera);
  let nx=_v3.x, ny=_v3.y;
  if(behind){ nx=-nx; ny=-ny; }
  const off = behind || Math.abs(nx)>0.9 || Math.abs(ny)>0.86;
  if(!off){ hud.marker.classList.add('hidden'); return; }
  hud.marker.classList.remove('hidden');
  hud.marker.classList.toggle('portal',isPortal);
  hud.marker.textContent=isPortal?'⬢':'◆';
  hud.marker.style.left=((clamp(nx,-0.94,0.94)*0.5+0.5)*window.innerWidth)+'px';
  hud.marker.style.top=((-clamp(ny,-0.9,0.9)*0.5+0.5)*window.innerHeight)+'px';
}

// ---------- Поток игры ----------
function startGame(){
  Snd.init(); Snd.click();
  score=0; hp=3; levelIdx=0; timeLeft=GAME_TIME; totalCollected=0;
  invuln=0; shake=0; lastTickSec=-1; theta=0; wheelAcc=0;
  hasMoved=false; hasRotated=false; sawSolid=false;
  genLevel(0);
  $('titleScreen').classList.add('hidden');
  $('endScreen').classList.add('hidden');
  $('pauseScreen').classList.add('hidden');
  $('hud').classList.remove('hidden');
  if(isTouch) $('touch').classList.remove('hidden');
  flashWhite();
  state='playing';
  showBanner('ГЛУБИНА I','найди кристаллы в четвёртом измерении',3000);
  if(!isTouch) lockPointer();
}
function pauseGame(){
  if(state!=='playing') return;
  state='paused'; $('pauseScreen').classList.remove('hidden');
  Snd.setRot(0,theta);
  if(pointerLocked) document.exitPointerLock();
}
function resumeGame(){
  if(state!=='paused') return;
  state='playing'; $('pauseScreen').classList.add('hidden');
  Snd.click();
  if(!isTouch) lockPointer();
}
function endGame(win, reason){
  state='ended';
  Snd.setRot(0,theta);
  if(pointerLocked) document.exitPointerLock();
  if(win){ score += Math.ceil(timeLeft)*10; Snd.win(); } else Snd.lose();
  $('endTitle').textContent = win ? '🌀 ТЫ СБЕЖАЛ ИЗ 3D!' : '💥 ВСЕЛЕННАЯ СХЛОПНУЛАСЬ';
  $('endTitle').className = 'endTitle '+(win?'winTxt':'loseTxt');
  $('endText').innerHTML = win
    ? 'Ты прошёл все три глубины, собрал гипер-кристаллы из измерения, которого не существует на экране, и вырвался в настоящий 4D-мир. Скучное 3D осталось позади — как плоская тень на стене.'
    : (reason==='time'
      ? 'Пять минут истекли — вселенная сжалась до трёх измерений. Кристаллы, оставшиеся в W, исчезли навсегда... но ведь почти получилось?'
      : 'Твоя гипер-оболочка разрушена минами из W. Четырёхмерное пространство не прощает тех, кто не смотрит на компас.');
  $('finalStats').innerHTML =
    `⭐ Очки: <b>${score}</b> &nbsp;·&nbsp; 💠 Кристаллов собрано: <b>${totalCollected}</b><br>` +
    `⏱ ${win?'Осталось времени':'Прожито'}: <b>${fmtTime(win?timeLeft:GAME_TIME-timeLeft)}</b> &nbsp;·&nbsp; 🌀 Достигнута: <b>${LEVELS[levelIdx].name}</b>`;
  $('hud').classList.add('hidden');
  $('touch').classList.add('hidden');
  $('endScreen').classList.remove('hidden');
  updateHUDCounts();
}
function nextLevel(){
  score+=250; Snd.levelClear(); flashWhite();
  if(hp<3) hp++;
  if(levelIdx>=LEVELS.length-1){ endGame(true); return; }
  levelIdx++; genLevel(levelIdx); updateHUDCounts();
  const subs=['','мины теперь сами дрейфуют в W — следи за компасом!','сама вселенная вращается в 4D... держись!'];
  showBanner(LEVELS[levelIdx].name.toUpperCase(), subs[levelIdx]||'',3200);
}
function toggleMute(){
  Snd.muted=!Snd.muted;
  $('btnMute').textContent=Snd.muted?'🔇':'🔊';
  if(Snd.muted) Snd.setRot(0,theta);
}
$('btnStart').addEventListener('click',startGame);
$('btnRestart').addEventListener('click',startGame);
$('btnRestart2').addEventListener('click',()=>{ $('pauseScreen').classList.add('hidden'); startGame(); });
$('btnResume').addEventListener('click',resumeGame);
$('btnPause').addEventListener('click',()=>{ if(state==='playing')pauseGame(); else if(state==='paused')resumeGame(); });
$('btnHelp').addEventListener('click',()=>{ if(state==='playing')pauseGame(); });
$('btnMute').addEventListener('click',toggleMute);

// ---------- Обновление сущностей (4D -> проекция -> 3D) ----------
function updateEntity4D(e, t){
  const [x1,w1]=rot2(e.p4.x,e.p4.w,theta);
  e.x1=x1; e.w1=w1;
  const p=project4(x1,e.p4.y,e.p4.z,w1,D4);
  e.pos3.set(p.x,p.y,p.z);
  e.solid = Math.abs(w1) <= SOLID_W;
  const solidF = clamp(1-(Math.abs(w1)-SOLID_W)/3.8, 0, 1);
  e.solidF=solidF;
  return p;
}
function updateCrystals(dt,t){
  for(const c of crystals){
    if(c.dead) continue;
    const bobY=c.p4.y+Math.sin(t*1.4+c.seed)*0.25;
    const keepY=c.p4.y; c.p4.y=bobY;
    const p=updateEntity4D(c,t); c.p4.y=keepY;
    c.group.position.copy(c.pos3);
    const pulse=c.solid?1+Math.sin(t*4+c.seed)*0.08:1;
    c.group.scale.setScalar(p.s*pulse);
    c.core.rotation.y+=dt*1.6; c.core.rotation.x+=dt*0.7;
    c.inner.rotation.y-=dt*2.4;
    c.coreMat.opacity=0.16+0.84*c.solidF;
    c.coreMat.depthWrite=c.solidF>0.6;
    c.innerMat.opacity=0.15+0.85*c.solidF;
    c.glow.material.opacity=0.07+0.6*c.solidF+(c.solid?Math.sin(t*5+c.seed)*0.08:0);
    c.beamMat.opacity=0.4*c.solidF*c.solidF;
    if(c.solid&&!c.wasSolid){
      sawSolid=true;
      if(solidifyBlipCd<=0){ Snd.solidify(); solidifyBlipCd=0.2; }
    }
    c.wasSolid=c.solid;
  }
}
function updateMines(dt,t){
  for(const m of mines){
    if(m.dead) continue;
    const phi=m.phi0 + Math.sin(t*m.driftSpd+m.phase)*m.driftAmp + m.orbit*t;
    m.p4.x=m.rho*Math.cos(phi); m.p4.w=m.rho*Math.sin(phi); m.p4.phi=phi;
    const p=updateEntity4D(m,t);
    m.group.position.copy(m.pos3);
    m.group.scale.setScalar(p.s);
    m.core.rotation.y+=dt*0.9; m.core.rotation.z+=dt*0.5;
    m.spikes.rotation.y-=dt*1.5; m.spikes.rotation.x+=dt*0.8;
    m.coreMat.opacity=0.07+0.93*m.solidF;
    m.coreMat.depthWrite=m.solidF>0.6;
    m.spikeMat.opacity=0.08+0.85*m.solidF;
    m.glow.material.opacity=(0.05+0.5*m.solidF)*(0.8+0.3*Math.sin(t*6+m.seed));
  }
}
function updatePortal(dt,t){
  if(!portal) return;
  const p=updateEntity4D(portal,t);
  portal.group.position.copy(portal.pos3);
  portal.group.scale.setScalar(p.s);
  portal.core.rotation.y+=dt*0.8;
  portal.ring1.rotation.z+=dt*0.9; portal.ring1.rotation.x=Math.sin(t*0.6)*0.5;
  portal.ring2.rotation.z-=dt*0.7;
  const f=portal.solidF;
  if(portalUnlocked){
    portal.coreMat.opacity=0.25+0.75*f;
    portal.ringMat.opacity=0.3+0.7*f;
    portal.glow.material.opacity=0.12+0.65*f+Math.sin(t*3)*0.06;
    portal.light.intensity=2.2*f;
    portal.tess.material.opacity=0.25+0.65*f;
  }else{
    portal.coreMat.opacity=0.1+0.25*f; portal.ringMat.opacity=0.12+0.2*f;
    portal.glow.material.opacity=0.04+0.1*f; portal.light.intensity=0;
    portal.tess.material.opacity=0.15+0.25*f;
  }
  updateTesseract(portal.tess,[[2,3,t*0.7],[1,3,t*0.53],[0,1,t*0.31]], D4, portal.p4);
}
function unlockPortal(){
  portalUnlocked=true; Snd.portalOpen();
  portal.coreMat.color.setHex(0xffd54a); portal.coreMat.emissive.setHex(0xcc8800); portal.coreMat.emissiveIntensity=1.6;
  portal.ringMat.color.setHex(0xffd54a);
  portal.glow.material.color.setHex(0xffc933);
  portal.tess.material.color.setHex(0xffd54a);
  showBanner('⬢ ПОРТАЛ ОТКРЫТ!','но он в W — вращай вселенную и прояви его!',3400);
  burst(portal.pos3,0xffd54a,60,9);
}

// ---------- Подсказки ----------
function dirKeyFor(e){ const d=deltaToSlice(e.p4); return d>0?'E':'Q'; }
let _lastHint='', _lastWd='', _lastFill=-1, _lastTimerTxt='', _lastDeg=-1;
function setHint(msg){ if(msg!==_lastHint){ _lastHint=msg; hud.hint.innerHTML=msg; } }
function updateHint(){
  if(state!=='playing') return;
  const near=nearestCrystal();
  const tgt = portalUnlocked?portal:near;
  if(tgt){
    const pct=Math.round((tgt.solidF||0)*100);
    if(pct!==_lastFill){ _lastFill=pct; hud.fill.style.width=pct+'%'; }
    const wd = tgt.solid?'В 3D! ✓':'W-глубина: '+Math.abs(tgt.w1).toFixed(1);
    if(wd!==_lastWd){ _lastWd=wd; hud.wd.textContent=wd; }
  }
  if(!hasMoved){ setHint('Лети: <b>WASD</b> + мышь — обзор. <b>SPACE/C</b> — вверх и вниз.'); return; }
  if(!hasRotated){ setHint('А теперь ГЛАВНОЕ: удерживай <b>Q</b> или <b>E</b> (или крути колесо мыши) — ты вращаешь мир в <b>4-м измерении</b>!'); return; }
  if(portalUnlocked && portal){
    setHint(portal.solid
      ? '⬢ Портал в нашем 3D! <b>Влетай в золотую сферу!</b>'
      : `⬢ Портал прячется в W (глубина ${Math.abs(portal.w1).toFixed(1)}). Удерживай <b>${dirKeyFor(portal)}</b> — прояви его!`);
    return;
  }
  const sol=nearestSolidCrystal();
  if(sol){
    const d=player.pos.distanceTo(sol.pos3).toFixed(0);
    setHint(`💠 Кристалл материализовался в 3D (${d} м)! <b>Влетай в него!</b> Ищи светящийся столб.`);
    return;
  }
  if(near){
    setHint(`💠 Ближайший кристалл — призрак на W-глубине <b>${Math.abs(near.w1).toFixed(1)}</b>. Удерживай <b>${dirKeyFor(near)}</b>, пока он не станет плотным. Смотри на <b>4D-компас</b> ↙`);
  }
}

// ---------- Главный цикл ----------
const clock=new THREE.Clock();
const _fwd=new THREE.Vector3(), _right=new THREE.Vector3(), _move=new THREE.Vector3(), _camT=new THREE.Vector3();
let trailTimer=0;

function animate(){
  requestAnimationFrame(animate);
  const dt=Math.min(clock.getDelta(),0.05);
  const t=clock.elapsedTime;
  elapsed=t;
  if(solidifyBlipCd>0) solidifyBlipCd-=dt;

  // ----- вращение в 4D -----
  let rotIn=0;
  if(state==='playing'){
    rotIn=((keys.KeyE||touchIn.e)?1:0)-((keys.KeyQ||touchIn.q)?1:0);
    if(rotIn!==0) hasRotated=true;
    let rate=rotIn*0.85;
    const nearC=nearestCrystal();
    const assistTgt=portalUnlocked?portal:nearC;
    if(assistTgt && Math.abs(assistTgt.w1)<4.6) rate*=0.5; // "магнитное" замедление у среза
    const step=wheelAcc*Math.min(1,dt*11); wheelAcc-=step;
    let dth=rate*dt+step;
    // мягкий захват среза: кристалл сам "дотягивается" в 3D
    if(rotIn===0 && Math.abs(wheelAcc)<0.002 && assistTgt){
      const dd=deltaToSlice(assistTgt.p4);
      if(Math.abs(dd)<0.2 && Math.abs(dd)>0.0015) dth+=clamp(dd,-1.1*dt,1.1*dt);
    }
    dth+=LEVELS[levelIdx].thetaDrift*dt;
    theta+=dth;
    thetaRateSmooth=lerp(thetaRateSmooth, dt>0?dth/dt:0, 0.12);
  } else if(state==='title'){
    theta+=dt*0.12; thetaRateSmooth=lerp(thetaRateSmooth,0.12,0.05);
  } else {
    thetaRateSmooth=lerp(thetaRateSmooth,0,0.1);
  }
  Snd.setRot(state==='playing'?clamp(Math.abs(thetaRateSmooth)/0.9,0,1):0, theta);
  const targetFov=68+clamp(Math.abs(thetaRateSmooth),0,1.4)*5;
  if(Math.abs(camera.fov-targetFov)>0.05){ camera.fov=lerp(camera.fov,targetFov,0.1); camera.updateProjectionMatrix(); }

  // ----- движение игрока -----
  if(state==='playing'){
    levelTime+=dt;
    _fwd.set(Math.sin(player.yaw),0,Math.cos(player.yaw));
    _right.set(-Math.cos(player.yaw),0,Math.sin(player.yaw));
    _move.set(0,0,0);
    const mf=((keys.KeyW)?1:0)-((keys.KeyS)?1:0)-touchIn.y;
    const mr=((keys.KeyD)?1:0)-((keys.KeyA)?1:0)+touchIn.x;
    const mu=((keys.Space||touchIn.up)?1:0)-((keys.KeyC||touchIn.down)?1:0);
    if(mf||mr||mu) hasMoved=true;
    _move.addScaledVector(_fwd,mf).addScaledVector(_right,mr); _move.y+=mu;
    if(_move.lengthSq()>1) _move.normalize();
    player.vel.addScaledVector(_move,40*dt);
    player.vel.multiplyScalar(Math.max(0,1-2.1*dt));
    const maxSp=(keys.ShiftLeft||keys.ShiftRight)?18.5:11.5;
    if(player.vel.length()>maxSp) player.vel.setLength(maxSp);
    player.pos.addScaledVector(player.vel,dt);
    // границы арены
    const rr=Math.hypot(player.pos.x,player.pos.z);
    if(rr>ARENA_R){ const k=ARENA_R/rr; player.pos.x*=k; player.pos.z*=k; }
    player.pos.y=clamp(player.pos.y,Y_MIN,Y_MAX);
    if(invuln>0) invuln-=dt;
    // таймер вселенной
    timeLeft-=dt;
    const sec=Math.ceil(timeLeft);
    if(sec!==lastTickSec){ lastTickSec=sec; if(sec<=10&&sec>0) Snd.tick(); }
    if(timeLeft<=0){ timeLeft=0; hud.t.textContent=fmtTime(0); endGame(false,'time'); }
    const tt=fmtTime(timeLeft);
    if(tt!==_lastTimerTxt){ _lastTimerTxt=tt; hud.t.textContent=tt; }
    hud.tw.classList.toggle('danger',timeLeft<=60);
    const deg=Math.round(((theta*180/Math.PI)%360+360)%360);
    if(deg!==_lastDeg){ _lastDeg=deg; hud.th.textContent='поворот в W: '+deg+'°'; }
  }

  // ----- сущности и столкновения -----
  updateCrystals(dt,t); updateMines(dt,t); updatePortal(dt,t);
  if(state==='playing'){
    for(const c of crystals){
      if(c.dead||!c.solid) continue;
      if(player.pos.distanceTo(c.pos3)<2.15*c.group.scale.x){
        c.dead=true; scene.remove(c.group);
        totalCollected++; score+=100; combo++;
        Snd.pickup(combo); burst(c.pos3,0x35e0ff,46,8);
        updateHUDCounts();
        const left=crystals.filter(x=>!x.dead).length;
        if(left===0) unlockPortal();
        else if(left===1) showBanner('ОСТАЛСЯ 1 КРИСТАЛЛ!','',1600);
      }
    }
    for(const m of mines){
      if(m.dead||!m.solid||invuln>0) continue;
      if(player.pos.distanceTo(m.pos3)<1.65*m.group.scale.x){
        m.dead=true; scene.remove(m.group);
        hp--; invuln=2.2; shake=1; combo=0;
        Snd.damage(); dmgFlash(); burst(m.pos3,0xff3355,54,10);
        const kb=player.pos.clone().sub(m.pos3).normalize().multiplyScalar(15); kb.y+=4;
        player.vel.add(kb);
        updateHUDCounts();
        if(hp<=0){ burst(player.pos,0x66d9ff,90,11); endGame(false,'mines'); }
        else showBanner('💥 МИНА ИЗ W!','оболочка повреждена · осталось ❤×'+hp,1800);
      }
    }
    if(portalUnlocked&&portal&&portal.solid&&state==='playing'){
      if(player.pos.distanceTo(portal.pos3)<3.1*portal.group.scale.x){
        burst(portal.pos3,0xffd54a,80,11);
        nextLevel();
      }
    }
  }

  // ----- игрок: вид -----
  if(state==='title'){ player.pos.set(Math.sin(t*0.4)*3, Math.sin(t*0.8)*0.8, 6+Math.cos(t*0.4)*2); }
  player.group.position.copy(player.pos);
  updateTesseract(player.inner,[[0,3,t*1.25],[2,3,t*0.9],[0,1,t*0.4]],2.4,null);
  player.shell.material.opacity = (invuln>0 && state==='playing') ? (Math.sin(t*26)>0?0.1:0.45) : 0.34;
  player.light.intensity=1.4+Math.sin(t*3.2)*0.25;

  // след
  trailTimer-=dt;
  if((state==='playing'||state==='title')&&trailTimer<=0){
    trailTimer=0.018;
    trailHist.unshift(player.pos.clone());
    if(trailHist.length>TRAIL_N) trailHist.pop();
  }
  {
    const pa=trailGeo.attributes.position.array, ca=trailGeo.attributes.color.array;
    for(let i=0;i<TRAIL_N;i++){
      const p=trailHist[Math.min(i,trailHist.length-1)]||player.pos;
      pa[i*3]=p.x; pa[i*3+1]=p.y; pa[i*3+2]=p.z;
      const f=Math.pow(1-i/TRAIL_N,1.6);
      ca[i*3]=0.15*f; ca[i*3+1]=0.75*f; ca[i*3+2]=1.0*f;
    }
    trailGeo.attributes.position.needsUpdate=true;
    trailGeo.attributes.color.needsUpdate=true;
  }

  // ----- фон 4D -----
  decorTess.forEach((d,i)=>{
    updateTesseract(d.line,[[0,3,theta*0.55+t*d.sp[0]],[2,3,t*d.sp[1]],[1,3,t*d.sp[2]],[0,1,t*0.02*(i+1)]],D_FAR,null);
    d.line.material.opacity=(i===0?0.5:i===1?0.38:0.3)*(0.8+0.2*Math.sin(t*0.5+i*2));
  });
  updateCloud(stars,D_FAR,t*0.011,theta*0.6+t*0.008);
  updateCloud(dust,D4,0,theta);
  boundsSphere.rotation.y+=dt*0.02; boundsSphere.rotation.x+=dt*0.008;

  // ----- камера -----
  if(state==='title'){
    const a=t*0.07;
    camera.position.lerp(_camT.set(Math.sin(a)*25,9+Math.sin(t*0.23)*3,Math.cos(a)*25),0.04);
    camera.lookAt(0,0,0);
  } else {
    const dist=9.5, cp=Math.cos(player.pitch), sp=Math.sin(player.pitch);
    _camT.set(
      player.pos.x-Math.sin(player.yaw)*cp*dist,
      player.pos.y+sp*dist+1.1,
      player.pos.z-Math.cos(player.yaw)*cp*dist);
    camera.position.lerp(_camT,1-Math.exp(-7*dt));
    if(shake>0.004){
      camera.position.x+=rand(-1,1)*shake*0.35;
      camera.position.y+=rand(-1,1)*shake*0.35;
      camera.position.z+=rand(-1,1)*shake*0.35;
      shake*=Math.exp(-5*dt);
    }
    camera.lookAt(player.pos.x,player.pos.y+0.7,player.pos.z);
  }

  updateEffects(dt);
  if(state==='playing'||state==='paused'){ drawCompass(); updateHint(); updateMarker(); }
  renderer.render(scene,camera);
}

// ---------- Старт ----------
genLevel(0);
updateHUDCounts();
animate();

window.addEventListener('resize',()=>{
  camera.aspect=window.innerWidth/window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth,window.innerHeight);
});
