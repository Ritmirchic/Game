/* Headless check for DON'T LOOK BACK */
const fs=require('fs'), path=require('path');
const {makeWorld}=require('../src/world.js');
const {render}=require('../src/render.js');
const world=makeWorld(20261025);
const N=world.N, grid=world.grid, idx=world.idx;
console.log('=== DON\'T LOOK BACK — WORLD CHECK ===');
console.log('World N='+N+' props='+world.props.length+' doors='+world.baseDoors.length+' docs='+world.documents.length+' anomalies='+world.ANOMALIES.length);
// check anomalies count
if(world.ANOMALIES.length!==30) console.log('FAIL: anomalies !=30 got '+world.ANOMALIES.length);
else console.log('Anomalies: 30 OK');
const kinds={visual:0,audio:0,env:0,entity:0};
for(const a of world.ANOMALIES) kinds[a.kind]=(kinds[a.kind]||0)+1;
console.log('  visual='+kinds.visual+' audio='+kinds.audio+' env='+kinds.env+' entity='+kinds.entity);
// check each anomaly can apply/revert without crash
let animOk=true;
for(const a of world.ANOMALIES){
  try{
    const snap=world.grid.slice();
    const state={entities:[],audioAnomalies:[],emergency:false,corridorScale:1,lightShift:0,lightFlicker:0,elevatorWrong:false,blockCorridor:false,windowViews:{}};
    // create mock proxy G
    a.apply(state);
  }catch(e){ console.log('  anomaly '+a.id+' apply FAIL: '+e.message); animOk=false; }
}
console.log(animOk?'All anomalies apply: OK':'Anomalies apply: FAIL');
// walkable check from spawn to emergency exit
function walkable(x,z){
  if(x<0||z<0||x>=N||z>=N) return false;
  return grid[idx(x,1,z)]===0 && grid[idx(x,2,z)]===0;
}
const sx=Math.floor(world.SPAWN.x), sz=Math.floor(world.SPAWN.z);
const seen=new Set(), q=[[sx,sz]]; seen.add(sx+','+sz);
while(q.length){
  const [x,z]=q.pop();
  for(const [nx,nz] of [[x+1,z],[x-1,z],[x,z+1],[x,z-1]]){
    if(!walkable(nx,nz)) continue;
    const k=nx+','+nz; if(seen.has(k)) continue; seen.add(k); q.push([nx,nz]);
  }
}
const coreReach=seen.has(Math.floor(world.CORE.x)+','+Math.floor(world.CORE.z));
console.log('Spawn walkable: '+(walkable(sx,sz)?'YES':'NO'));
console.log('Emergency exit reachable: '+(coreReach?'YES':'NO')+' (cells '+seen.size+')');
console.log('Spawn '+world.SPAWN.x.toFixed(1)+','+world.SPAWN.z.toFixed(1)+' Core '+world.CORE.x.toFixed(1)+','+world.CORE.z.toFixed(1));

// render shots
const W=480,H=270, pix=new Uint32Array(W*H), depth=new Float32Array(W*H);
function shot(name,cam){
  const t0=Date.now();
  render(world,{fov:0.92, occ:world.occ, P:world.P, lamps:world.lamps, flashOn:true, emergency:false, windowViews:{}, ...cam},W,H,pix,depth,1.2);
  const ms=Date.now()-t0;
  const head=Buffer.from(`P6\n${W} ${H}\n255\n`,'ascii');
  const body=Buffer.alloc(W*H*3);
  for(let i=0;i<W*H;i++){ const v=pix[i]; body[i*3]=v&255; body[i*3+1]=(v>>8)&255; body[i*3+2]=(v>>16)&255; }
  const out=path.join(__dirname,'..','shots',name+'.ppm');
  fs.mkdirSync(path.dirname(out),{recursive:true});
  fs.writeFileSync(out,Buffer.concat([head,body]));
  console.log('  '+name+': '+ms+'ms');
}
const EY=1.62;
shot('01_spawn', {x:15.5,y:EY,z:4.5,yaw:0,pitch:0});
shot('02_corridor_n', {x:15.5,y:EY,z:10,yaw:0,pitch:0});
shot('03_hub', {x:15.5,y:EY,z:16,yaw:0,pitch:0.08});
shot('04_corridor_s', {x:15.5,y:EY,z:24,yaw:0,pitch:0});
shot('05_apt101', {x:8,y:EY,z:7.5,yaw:0,pitch:0});
shot('06_elevator', {x:19.5,y:EY,z:16.5,yaw:-1.2,pitch:0});
shot('07_emergency', {x:15.5,y:EY,z:28,yaw:0,pitch:0});
shot('08_window_view', {x:6.5,y:EY,z:7,yaw:-1.57,pitch:0});
console.log('WORLD CHECK DONE');
