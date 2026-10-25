(() => {
  const canvas = document.getElementById('gameCanvas');
  const ctx = canvas.getContext('2d');
  const stage = document.getElementById('gameStage');
  const TAU = Math.PI * 2;
  const H = 2.5, N = 5;
  const V3 = (x,y,z) => ({x,y,z});
  const add = (a,b) => V3(a.x+b.x,a.y+b.y,a.z+b.z);
  const mul = (a,s) => V3(a.x*s,a.y*s,a.z*s);
  const dot = (a,b) => a.x*b.x+a.y*b.y+a.z*b.z;
  const cross = (a,b) => V3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);
  const faces = [
    {id:'+Z',name:'ВЕРХНЯЯ',symbol:'↑',n:V3(0,0,1),u:V3(1,0,0),v:V3(0,-1,0),base:'#193a3c',light:'#72d4b3'},
    {id:'-Y',name:'ПЕРЕДНЯЯ',symbol:'↘',n:V3(0,-1,0),u:V3(1,0,0),v:V3(0,0,1),base:'#173039',light:'#61c8c9'},
    {id:'+X',name:'ПРАВАЯ',symbol:'→',n:V3(1,0,0),u:V3(0,-1,0),v:V3(0,0,1),base:'#293441',light:'#8fc3c4'},
    {id:'+Y',name:'ЗАДНЯЯ',symbol:'↖',n:V3(0,1,0),u:V3(-1,0,0),v:V3(0,0,1),base:'#263847',light:'#78b9d0'},
    {id:'-X',name:'ЛЕВАЯ',symbol:'←',n:V3(-1,0,0),u:V3(0,1,0),v:V3(0,0,1),base:'#303445',light:'#ba9fd7'},
    {id:'-Z',name:'НИЖНЯЯ',symbol:'↓',n:V3(0,0,-1),u:V3(1,0,0),v:V3(0,1,0),base:'#27383c',light:'#7cbdb1'}
  ];
  const byId = Object.fromEntries(faces.map(f => [f.id,f]));
  const shards = [
    {face:'-Y',i:0,j:1,got:false},
    {face:'+X',i:3,j:1,got:false},
    {face:'+Y',i:1,j:3,got:false},
    {face:'+Z',i:4,j:0,got:false}
  ];
  const exit = {face:'+Z',i:4,j:4};
  let player, yaw=0, moveAnim=null, particles=[], audioCtx=null, muted=false, turns=0, won=false;
  let width=0,height=0,dpr=1,lastTime=performance.now(),idle=0;
  const $ = id => document.getElementById(id);

  function reset() {
    player={face:'-Y',i:2,j:4}; yaw=0; turns=0; moveAnim=null; particles=[]; won=false;
    shards.forEach(s=>s.got=false);
    $('winMessage').hidden=true;
    syncUI(); render();
  }
  function resize(){
    const r=stage.getBoundingClientRect(); dpr=Math.min(2,window.devicePixelRatio||1); width=r.width; height=r.height;
    canvas.width=Math.round(width*dpr); canvas.height=Math.round(height*dpr); ctx.setTransform(dpr,0,0,dpr,0,0); render();
  }
  function facePoint(face,i,j){return add(add(mul(face.n,H),mul(face.u,i-2)),mul(face.v,2-j));}
  function rotatePoint(p){
    const a=yaw,c=Math.cos(a),s=Math.sin(a);
    return V3(p.x*c-p.y*s,p.x*s+p.y*c,p.z);
  }
  function project(p){
    const r=rotatePoint(p); const isoX=(r.x-r.y)*Math.SQRT1_2; const isoY=(r.x+r.y)*.5-r.z;
    const scale=Math.min(width/9.0,height/7.3);
    return {x:width*.5+isoX*scale,y:height*.51+isoY*scale,scale};
  }
  function activeFaces(){
    const cam=V3(6,-7,8);
    return faces.filter(f=>dot(rotatePoint(f.n),cam)>0.15).sort((a,b)=>dot(rotatePoint(a.n),cam)-dot(rotatePoint(b.n),cam));
  }
  function poly(points,fill,stroke,line=1){
    ctx.beginPath(); points.forEach((p,i)=>{const q=project(p); if(!i)ctx.moveTo(q.x,q.y);else ctx.lineTo(q.x,q.y)});ctx.closePath();
    if(fill){ctx.fillStyle=fill;ctx.fill()} if(stroke){ctx.strokeStyle=stroke;ctx.lineWidth=line;ctx.stroke()}
  }
  function faceCorners(face,extra=0){
    return [
      add(add(mul(face.n,H),mul(face.u,-H+extra)),mul(face.v,-H+extra)),
      add(add(mul(face.n,H),mul(face.u,H-extra)),mul(face.v,-H+extra)),
      add(add(mul(face.n,H),mul(face.u,H-extra)),mul(face.v,H-extra)),
      add(add(mul(face.n,H),mul(face.u,-H+extra)),mul(face.v,H-extra))
    ];
  }
  function cellCorners(face,i,j,margin=.055){
    const center=facePoint(face,i,j), du=mul(face.u,.5-margin), dv=mul(face.v,.5-margin);
    return [add(add(center,mul(du,-1)),mul(dv,-1)),add(add(center,du),mul(dv,-1)),add(add(center,du),dv),add(add(center,mul(du,-1)),dv)];
  }
  function hexToRgb(h){const n=parseInt(h.slice(1),16);return [n>>16,(n>>8)&255,n&255]}
  function tint(hex,amt){const c=hexToRgb(hex);return `rgb(${c.map(v=>Math.max(0,Math.min(255,v+amt))).join(',')})`}
  function drawFace(face,now){
    const center=project(mul(face.n,H));
    const grad=ctx.createLinearGradient(center.x-100,center.y-100,center.x+130,center.y+150);
    grad.addColorStop(0,tint(face.base,12));grad.addColorStop(1,tint(face.base,-15));
    poly(faceCorners(face),grad,'#c0f8e322',1.2);
    // A fine coordinate lattice gives the surface a softly luminous, instrument-like quality.
    for(let j=0;j<N;j++)for(let i=0;i<N;i++){
      const special=shards.some(s=>s.face===face.id&&s.i===i&&s.j===j&&!s.got);
      const isExit=exit.face===face.id&&exit.i===i&&exit.j===j;
      const isPlayer=player.face===face.id&&player.i===i&&player.j===j;
      let fill=tint(face.base,((i+j)%2?4:-4));
      if(special)fill='#1d4a49'; if(isExit)fill=shards.every(s=>s.got)?'#264841':'#1e3034';
      poly(cellCorners(face,i,j),fill, special?'#9ef5d25a':isExit?'#ffd59b45':'#b7ddd21b',.65);
      if(special) drawShard(face,i,j,now);
      if(isExit) drawExit(face,i,j,now);
      if(isPlayer && !moveAnim) drawPlayer(facePoint(face,i,j),now);
    }
    // corner registration marks
    const cs=faceCorners(face,.07);
    cs.forEach((p,k)=>{const q=project(p);ctx.fillStyle='#bcebd355';ctx.beginPath();ctx.arc(q.x,q.y,1.2,0,TAU);ctx.fill()});
  }
  function drawShard(face,i,j,now){
    const p=project(add(facePoint(face,i,j),mul(face.n,.14))), pulse=1+Math.sin(now*.004+i)*.12, s=5*pulse;
    ctx.save();ctx.shadowColor='#9ef5d2';ctx.shadowBlur=16;
    ctx.beginPath();ctx.moveTo(p.x,p.y-s*1.65);ctx.lineTo(p.x+s,p.y);ctx.lineTo(p.x,p.y+s*1.65);ctx.lineTo(p.x-s,p.y);ctx.closePath();
    ctx.fillStyle='#c4ffe5';ctx.fill();ctx.shadowBlur=0;ctx.strokeStyle='#effff4';ctx.lineWidth=.8;ctx.stroke();
    ctx.beginPath();ctx.arc(p.x,p.y+s*2.35,1.5,0,TAU);ctx.fillStyle='#9ef5d2aa';ctx.fill();ctx.restore();
  }
  function drawExit(face,i,j,now){
    const p=project(add(facePoint(face,i,j),mul(face.n,.04))),open=shards.every(s=>s.got),pulse=.5+.5*Math.sin(now*.003);
    ctx.save();ctx.translate(p.x,p.y);ctx.rotate(Math.PI/4);ctx.strokeStyle=open?`rgba(255,205,145,${.65+pulse*.3})`:'#607074';ctx.lineWidth=1.1;ctx.shadowColor='#ffc77a';ctx.shadowBlur=open?14:0;ctx.strokeRect(-6,-6,12,12);ctx.restore();
    if(open){ctx.beginPath();ctx.arc(p.x,p.y,9+pulse*2,0,TAU);ctx.strokeStyle=`rgba(255,201,133,${.13+pulse*.14})`;ctx.lineWidth=1;ctx.stroke()}
  }
  function drawPlayer(p,now){
    const q=project(add(p,V3(0,0,.17))), pulse=1+Math.sin(now*.006)*.08;
    ctx.save();ctx.globalAlpha=.35;ctx.fillStyle='#a5ffde';ctx.beginPath();ctx.ellipse(q.x,q.y+5,5,2,0,0,TAU);ctx.fill();ctx.restore();
    ctx.save();ctx.shadowColor='#9ef5d2';ctx.shadowBlur=19;
    const g=ctx.createRadialGradient(q.x-2,q.y-2,1,q.x,q.y,7*pulse);g.addColorStop(0,'#ffffff');g.addColorStop(.35,'#c1ffe5');g.addColorStop(1,'#51bca8');
    ctx.beginPath();ctx.arc(q.x,q.y,6*pulse,0,TAU);ctx.fillStyle=g;ctx.fill();ctx.restore();
    ctx.beginPath();ctx.arc(q.x,q.y,10+pulse*1.2,0,TAU);ctx.strokeStyle='#a1ffdd75';ctx.lineWidth=.7;ctx.stroke();
  }
  function drawFloorGrid(now){
    const f=byId[player.face]; if(!activeFaces().includes(f))return;
    const p=project(facePoint(f,player.i,player.j));
    ctx.save();ctx.setLineDash([2,5]);ctx.strokeStyle='#b7efde22';ctx.lineWidth=.7;
    for(let i=0;i<5;i++){const a=project(facePoint(f,i,player.j));ctx.beginPath();ctx.moveTo(p.x,p.y);ctx.lineTo(a.x,a.y);ctx.stroke()}
    ctx.restore();
  }
  function drawMove(now){
    if(!moveAnim)return;
    const t=Math.min(1,(now-moveAnim.start)/moveAnim.duration),e=1-Math.pow(1-t,3);
    const p=V3(moveAnim.from.x+(moveAnim.to.x-moveAnim.from.x)*e,moveAnim.from.y+(moveAnim.to.y-moveAnim.from.y)*e,moveAnim.from.z+(moveAnim.to.z-moveAnim.from.z)*e+Math.sin(Math.PI*t)*.18);
    drawPlayer(p,now); if(t>=1)moveAnim=null;
  }
  function render(now=performance.now()){
    if(!width||!height)return;
    ctx.clearRect(0,0,width,height);
    const gl=ctx.createRadialGradient(width*.5,height*.48,4,width*.5,height*.5,Math.max(width,height)*.48);gl.addColorStop(0,'#14232a22');gl.addColorStop(1,'#00000000');ctx.fillStyle=gl;ctx.fillRect(0,0,width,height);
    const visible=activeFaces();
    // A quiet shadow anchors the impossible object in a space with no floor.
    const base=project(V3(0,0,-2.8));ctx.save();ctx.globalAlpha=.25;ctx.filter='blur(18px)';ctx.fillStyle='#000';ctx.beginPath();ctx.ellipse(base.x,base.y+17,Math.min(width*.22,150),17,0,0,TAU);ctx.fill();ctx.restore();
    visible.forEach(f=>drawFace(f,now));
    drawFloorGrid(now);drawMove(now);
    particles=particles.filter(p=>now-p.t<p.life);
    for(const p of particles){const t=(now-p.t)/p.life;const q=project(p.pos);ctx.globalAlpha=1-t;ctx.fillStyle=p.color;ctx.beginPath();ctx.arc(q.x+p.dx*t,q.y+p.dy*t,p.size*(1-t),0,TAU);ctx.fill()}
    ctx.globalAlpha=1;
    // Small orbiting motes make the void feel inhabited.
    for(let k=0;k<13;k++){const a=now*.00022+k*TAU/13;const x=width*.5+Math.cos(a)*Math.min(width*.36,300),y=height*.5+Math.sin(a*1.7)*height*.32;ctx.fillStyle=`rgba(167,232,213,${.08+.08*Math.sin(a*2)})`;ctx.fillRect(x,y,1,1)}
  }
  function animationLoop(now){const dt=now-lastTime;lastTime=now;idle+=dt;render(now);requestAnimationFrame(animationLoop)}
  function getScreenNormal(dir,face){return dir==='right'?face.u:dir==='left'?mul(face.u,-1):dir==='up'?face.v:mul(face.v,-1)}
  function move(dir){
    if(won||moveAnim)return;
    const f=byId[player.face],di=dir==='right'?1:dir==='left'?-1:0,dj=dir==='down'?1:dir==='up'?-1:0;
    let nf=f,ni=player.i+di,nj=player.j+dj;
    if(ni<0||ni>=N||nj<0||nj>=N){
      const D=getScreenNormal(dir,f); nf=faces.find(x=>dot(x.n,D)>.99); if(!nf)return;
      const p=facePoint(f,player.i,player.j);
      const edgeAxis=(dir==='left'||dir==='right')?f.v:f.u;
      const t=dot(p,edgeAxis);
      // The player walks across the folded edge; preserving the tangent coordinate is what makes opposite sides line up.
      const target=add(add(mul(D,H),mul(f.n,H-.5)),mul(edgeAxis,t));
      ni=Math.round(dot(target,nf.u)+2); nj=Math.round(2-dot(target,nf.v));
      ni=Math.max(0,Math.min(4,ni));nj=Math.max(0,Math.min(4,nj));
    }
    const from=facePoint(f,player.i,player.j),to=facePoint(nf,ni,nj);
    player={face:nf.id,i:ni,j:nj};
    moveAnim={from,to,start:performance.now(),duration:nf===f?115:245};
    const shard=shards.find(s=>!s.got&&s.face===player.face&&s.i===ni&&s.j===nj);
    if(shard){shard.got=true;burst(to,'#b9ffe0',18);sound('collect');}
    else sound('step');
    if(player.face===exit.face&&player.i===exit.i&&player.j===exit.j&&shards.every(s=>s.got)){
      won=true;setTimeout(()=>{$('winMessage').hidden=false;sound('win')},380);burst(to,'#ffcb83',28);
    }
    syncUI();
  }
  function turn(amount){if(won)return;yaw+=amount*Math.PI/2;turns=(turns+1)%4;syncUI();sound('turn')}
  function burst(pos,color,count){for(let i=0;i<count;i++)particles.push({pos,t:performance.now(),life:450+Math.random()*500,dx:(Math.random()-.5)*54,dy:(Math.random()-.5)*60,size:1+Math.random()*2.8,color})}
  function remaining(){return shards.filter(s=>!s.got).length}
  function syncUI(){
    const got=4-remaining(), pct=Math.round(got/4*100);$('progressFill').style.width=pct+'%';$('progressText').textContent=`${got} ИЗ 4 ФРАГМЕНТОВ`;$('progressPct').textContent=String(pct).padStart(2,'0')+'%';
    const f=byId[player.face];$('phaseLabel').textContent=`ПОВЕРХНОСТЬ 0${faces.indexOf(f)+1} · ${f.id}`;$('gravityLabel').textContent=f.id;
    $('faceIndex').textContent=`0${faces.indexOf(f)+1} / 06`;$('faceName').textContent=f.name;$('faceSymbol').textContent=f.symbol;$('faceSubtitle').textContent=`${f.id} · 5 × 5 ШАГОВ`;
    $('coords').innerHTML=`X ${String(player.i+1).padStart(2,'0')}&nbsp; / &nbsp;Y ${String(player.j+1).padStart(2,'0')}&nbsp; / &nbsp;Z ${f.id}`;
    $('turnLabel').textContent=`ПОВОРОТ ${String(((turns%4)*90)).padStart(3,'0')}°`;
    $('faceTip').textContent=got===4?'Выход открыт. Он ждёт на верхней грани.':'На краю не падай. Переходи на следующую грань.';
    $('stageHint').innerHTML=got===4?'<span class="hint-key">ВЫХОД ОТКРЫТ</span> НАЙДИ ЗОЛОТОЙ ПРОЁМ <span class="hint-sep">·</span> <span class="hint-key">Q / E</span> ПОВЕРНИ МИР':'<span class="hint-key">WASD</span> ДВИГАЙСЯ ПО ГРАНИ <span class="hint-sep">·</span> <span class="hint-key">Q / E</span> ПОВЕРНИ МИР';
  }
  function sound(type){if(muted)return;try{audioCtx ||= new (window.AudioContext||window.webkitAudioContext)(); if(audioCtx.state==='suspended')audioCtx.resume();const o=audioCtx.createOscillator(),g=audioCtx.createGain();o.connect(g);g.connect(audioCtx.destination);const t=audioCtx.currentTime;const freq=type==='collect'?660:type==='win'?440:type==='turn'?180:260;o.type=type==='collect'?'sine':'triangle';o.frequency.setValueAtTime(freq,t);o.frequency.exponentialRampToValueAtTime(type==='collect'?1040:type==='win'?880:freq*.72,t+.16);g.gain.setValueAtTime(type==='collect'?.055:.025,t);g.gain.exponentialRampToValueAtTime(.0001,t+.22);o.start(t);o.stop(t+.23)}catch(e){}}
  function key(e){const k=e.key.toLowerCase();if(['arrowup','arrowdown','arrowleft','arrowright',' '].includes(k))e.preventDefault();if(k==='w'||k==='arrowup')move('up');else if(k==='s'||k==='arrowdown')move('down');else if(k==='a'||k==='arrowleft')move('left');else if(k==='d'||k==='arrowright')move('right');else if(k==='q')turn(-1);else if(k==='e')turn(1);else if(k==='r')reset();else if(k==='m')toggleSound()}
  function toggleSound(){muted=!muted;$('soundToggle').classList.toggle('muted',muted);$('soundToggle').setAttribute('aria-label',muted?'Звук выключен':'Звук включен');sound('turn')}
  window.addEventListener('keydown',key);window.addEventListener('resize',resize);
  document.querySelectorAll('.mobile-pad button').forEach(b=>{const fn=e=>{e.preventDefault();move(b.dataset.move)};b.addEventListener('pointerdown',fn)});
  $('soundToggle').addEventListener('click',toggleSound);$('playAgain').addEventListener('click',reset);
  stage.addEventListener('pointerdown',e=>{if(e.pointerType==='touch')stage.dataset.touch='1'});
  resize();reset();requestAnimationFrame(animationLoop);
})();
