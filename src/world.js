/* ============================================================
   DON'T LOOK BACK — World Generator
   Apartment complex · 32³ voxel grid · anomaly system
   ============================================================ */
(function(root){
'use strict';
const N=32;
const IH0=1, IH1=3, CEIL=4;
const idx=(x,y,z)=>(y*N+z)*N+x;

function makeWorld(seedIn){
  let seed=(seedIn>>>0)||1;
  const rnd=()=>{ seed=(seed*1664525+1013904223)>>>0; return seed/4294967296; };
  const ri=(a,b)=>a+Math.floor(rnd()*(b-a+1));
  const rfl=()=>rnd();

  const grid=new Uint8Array(N*N*N); grid.fill(1);
  // helpers
  const inb=(x,y,z)=>x>=0&&y>=0&&z>=0&&x<N&&y<N&&z<N;
  const SET=(x,y,z,v)=>{ if(inb(x,y,z)) grid[idx(x,y,z)]=v; };
  const GET=(x,y,z)=>inb(x,y,z)?grid[idx(x,y,z)]:1;
  const FILL=(x0,y0,z0,x1,y1,z1,v)=>{ for(let y=y0;y<=y1;y++)for(let z=z0;z<=z1;z++)for(let x=x0;x<=x1;x++)SET(x,y,z,v); };
  const CARVE=(x0,y0,z0,x1,y1,z1)=>FILL(x0,y0,z0,x1,y1,z1,0);

  // ---------- BASE APARTMENT COMPLEX LAYOUT ----------
  // Carve interior void base (floor)
  // Outer shell walls remain (grid filled with 1 = wall)
  // We'll carve rooms + corridors inside 3..28

  // Main north-south spine corridor: x 14..17
  CARVE(14,IH0,2,17,IH1,29);
  // East-west hubs
  CARVE(5,IH0,14,27,IH1,18); // central hub (cross)
  // Hub extensions
  CARVE(5,IH0,5,12,IH1,10); // Apt 101 area (NW)
  CARVE(20,IH0,5,27,IH1,10); // Apt 102 (NE)
  CARVE(5,IH0,20,12,IH1,27); // Apt 105/107 south-west
  CARVE(20,IH0,20,27,IH1,27); // Apt 106/108 south-east

  // Separate apartments by interior walls (add back walls then doors)
  // We'll add wall partitions as voxels (set to 1) inside carved area
  // Apt walls: vertical partitions
  // Apartment 101 internal
  FILL(8,IH0,5,8,IH1,7,1); // partition inside apt 101
  FILL(5,IH0,7,12,IH1,7,1);
  // door openings in these partitions
  CARVE(9,IH0,7,10,IH1,7); CARVE(7,IH0,6,7,IH1,7);
  // Apt 102
  FILL(23,IH0,5,23,IH1,7,1); FILL(20,IH0,7,27,IH1,7,1);
  CARVE(24,IH0,7,25,IH1,7);
  // Central rooms
  // Stairwell room west of corridor
  FILL(11,IH0,14,12,IH1,18,1); CARVE(11,IH0,16,12,IH1,16); // door to stairwell
  // Maintenance / storage
  FILL(5,IH0,14,5,IH1,14,1); // pillar
  // Elevator shaft (closed, door only)
  FILL(19,IH0,14,20,IH1,18,1);
  // Laundry room southeast
  FILL(20,IH0,20,20,IH1,24,1); CARVE(20,IH0,22,20,IH1,22);
  // Storage south
  FILL(14,IH0,25,17,IH1,25,1); CARVE(15,IH0,25,16,IH1,25);
  // Emergency exit corridor south end
  CARVE(14,IH0,28,17,IH1,29); // extends to south wall

  // ---------- DOORS (carved openings in walls that remain) ----------
  // Corridor to apartments doors (holes in the walls we kept)
  // We already have open; now define door positions as passable cells that can be locked
  // We'll store door meta separately
  const baseDoors=[
    {id:'d101', x:12,y:IH0,z:7, open:true, locked:false, label:'101'},
    {id:'d102', x:20,y:IH0,z:7, open:true, locked:false, label:'102'},
    {id:'d103', x:12,y:IH0,z:16, open:true, locked:true, label:'103'},
    {id:'d104', x:20,y:IH0,z:16, open:false, locked:true, label:'104'},
    {id:'d105', x:12,y:IH0,z:22, open:true, locked:false, label:'105'},
    {id:'d106', x:20,y:IH0,z:22, open:true, locked:false, label:'106'},
    {id:'d107', x:8,y:IH0,z:18, open:false, locked:true, label:'107'},
    {id:'d108', x:24,y:IH0,z:18, open:false, locked:true, label:'108'},
    {id:'dStair', x:11,y:IH0,z:16, open:true, locked:false, label:'STAIR'},
    {id:'dElev', x:19,y:IH0,z:16, open:false, locked:true, label:'ELEV'},
    {id:'dMaint', x:5,y:IH0,z:15, open:false, locked:true, label:'MAINT'},
    {id:'dLaundry', x:20,y:IH0,z:22, open:true, locked:false, label:'LAUNDRY'},
    {id:'dEmerg', x:15,y:IH0,z:29, open:false, locked:true, label:'EXIT'},
    {id:'dBase', x:6,y:IH0,z:25, open:false, locked:true, label:'BASEMENT'},
  ];
  // Ensure doors are carved (open state will be handled logically, but geometry stays open for now)
  // Locked closed doors we will treat as wall in collision if needed
  for(const d of baseDoors){
    // carve door cell if not already (ensure passage)
    if(GET(d.x,d.y,d.z)===1) SET(d.x,d.y,d.z,0);
    if(GET(d.x,d.y+1,d.z)===1) SET(d.x,d.y+1,d.z,0);
  }
  // Actually close some doors geometrically for initial state: d104 etc should be walls to appear locked
  function setDoorGeom(d, isOpen){
    if(isOpen){ SET(d.x,IH0,d.z,0); SET(d.x,IH0+1,d.z,0); }
    else { SET(d.x,IH0,d.z,1); SET(d.x,IH0+1,d.z,1); }
  }
  for(const d of baseDoors){
    if(d.id==='d104' || d.id==='d107' || d.id==='d108' || d.id==='dElev' || d.id==='dMaint' || d.id==='dEmerg' || d.id==='dBase'){
      setDoorGeom(d,false);
    }
  }

  // ---------- SPAWN & KEY POSITIONS ----------
  const SPAWN={x:15.5, z:4.5, yaw:0};
  const CORE={x:15.5, z:28.5}; // emergency exit at south
  const ELEV_POS={x:19.5,z:16.5};
  const STAIR_POS={x:10.5,z:16};
  const DECISION_POS={x:15.5,z:18.5}; // intersection where anomaly decision made, trigger zone z 18-19

  // ---------- DECORATIONS / PROPS ----------
  // Each prop: {id, x, z, y, kind, rot, room, baseVariant}
  // kinds: painting, chair, sofa, table, vase, lamp_stand, fridge, tv, plant, mirror, ceiling_light, wallpaper_panel, window, radio, etc.
  const props=[
    // central corridor decorations
    {id:'paintA', x:14.2,z:8, y:1.6, kind:'painting', room:'corrN', variant:0, base:0},
    {id:'paintB', x:17.8,z:12, y:1.6, kind:'painting', room:'corrN', variant:0, base:0},
    {id:'paintC', x:14.2,z:24, y:1.6, kind:'painting', room:'corrS', variant:0, base:0},
    {id:'chair1', x:15.2,z:9, y:0.9, kind:'chair', room:'corrN', variant:0},
    {id:'table1', x:15.8,z:10.2,y:0.7, kind:'table', room:'corrN', variant:0},
    {id:'vase1', x:16.2,z:11, y:1.1, kind:'vase', room:'corrN', variant:0},
    {id:'lamp1', x:7,z:7.5,y:1.2, kind:'lamp_stand', room:'101'},
    {id:'sofa1', x:9,z:9, y:0.8, kind:'sofa', room:'101'},
    {id:'fridge1', x:6.5,z:6.2,y:1.4, kind:'fridge', room:'101'},
    {id:'plant1', x:10.5,z:6.8,y:1.0, kind:'plant', room:'101'},
    {id:'tv1', x:22,z:7, y:1.3, kind:'tv', room:'102'},
    {id:'chair2', x:24,z:9.2,y:0.9, kind:'chair', room:'102'},
    {id:'mirror1', x:11,z:15.8,y:1.5, kind:'mirror', room:'hub'},
    {id:'window1', x:5.2,z:7, y:1.6, kind:'window', room:'101', view:0},
    {id:'window2', x:27.8,z:7, y:1.6, kind:'window', room:'102', view:0},
    {id:'window3', x:5.2,z:23,y:1.6, kind:'window', room:'105', view:0},
    {id:'window4', x:27.8,z:23,y:1.6, kind:'window', room:'106', view:0},
    {id:'radio1', x:21.5,z:6.5,y:1.1, kind:'radio', room:'102'},
    {id:'ceiling1', x:15.5,z:16, y:3.2, kind:'ceiling_light', room:'hub'},
    {id:'ceiling2', x:15.5,z:8, y:3.2, kind:'ceiling_light', room:'corrN'},
    {id:'ceiling3', x:15.5,z:24, y:3.2, kind:'ceiling_light', room:'corrS'},
    {id:'wallpaperA', x:14.5,z:7, y:1.6, kind:'wallpaper', room:'corrN', variant:0},
    {id:'wallpaperB', x:16.5,z:25,y:1.6, kind:'wallpaper', room:'corrS', variant:0},
    {id:'bathtub1', x:6,z:25.5,y:0.8, kind:'bathtub', room:'107'},
    {id:'bed1', x:9,z:24,y:0.6, kind:'bed', room:'105'},
    {id:'cabinet1', x:23,z:24.5,y:1.2, kind:'cabinet', room:'106'},
    {id:'clock1', x:16.8,z:15.2,y:1.9, kind:'clock', room:'hub'},
    {id:'doll1', x:8.5,z:9.5,y:0.9, kind:'doll', room:'101'},
    {id:'chair3', x:15.5,z:20.5,y:0.9, kind:'chair', room:'corrS'},
  ];

  // ---------- LAMPS (ceiling) ----------
  const baseLamps=[
    {x:15,z:CEIL, zz:4}, {x:16,z:CEIL, zz:8}, {x:15,z:CEIL, zz:12}, {x:16,z:CEIL, zz:16},
    {x:15,z:CEIL, zz:20}, {x:16,z:CEIL, zz:24}, {x:15,z:CEIL, zz:28},
    {x:7,z:CEIL, zz:7}, {x:24,z:CEIL, zz:7}, {x:7,z:CEIL, zz:23}, {x:24,z:CEIL, zz:23},
    {x:10,z:CEIL, zz:16}, {x:22,z:CEIL, zz:16},
  ];

  // ---------- WINDOWS POSITIONS ----------
  const windows=[
    {x:5,z:7, view:0}, {x:27,z:7, view:0}, {x:5,z:23, view:0}, {x:27,z:23, view:0},
  ];

  // ---------- DOCUMENTS / CLUES ----------
  const documents=[
    {id:'doc1', x:7.5,z:9.2, room:'101', title:'Дневник жильца 101', text:'Она наблюдала из коридора. Каждый раз, когда я оглядывался — её не было. Но когда я не смотрел — слышал шаги.', found:false},
    {id:'doc2', x:23.5,z:8.5, room:'102', title:'Записка на холодильнике', text:'ЛИФТ ВРЁТ. Нажимаешь 3 — попадаешь в подвал. Не заходи туда, если слышишь дыхание.', found:false},
    {id:'doc3', x:6.2,z:24.8, room:'107', title:'Отчёт смотрителя', text:'1978-03-12: аварийное освещение сработало без причины. В коридоре A стало длиннее на 6 метров. Измеряли рулеткой.', found:false},
    {id:'doc4', x:22.5,z:25.2, room:'106', title:'Письмо', text:'Моя жена клянётся, что картина в прихожей изменилась. Там теперь я. Я стою спиной.', found:false},
    {id:'doc5', x:10.8,z:15.5, room:'stair', title:'Журнал лифта', text:'Техник: кабина прибыла на 9 этаж. В здании 5 этажей. Внутри — никого. Кнопки стёрты.', found:false},
    {id:'doc6', x:6.5,z:26.5, room:'base', title:'Подвал — предупреждение', text:'НЕ СМОТРИ НАЗАД, когда идёшь к выходу. Оно идёт только когда ты смотришь вперёд.', found:false},
  ];

  // ---------- OCC MASK ----------
  // occ byte: low 4 bits = solid, but we only have 1 world now. For compatibility with render, we use bit0.
  // We'll still produce occ with bit0 solid, bits 4-7 for ghost? Not needed but keep.
  const P2=N+2, S2=P2*P2;
  const occ=new Uint8Array(P2*P2*P2); occ.fill(0xFF);
  for(let y=0;y<N;y++)for(let z=0;z<N;z++)for(let x=0;x<N;x++){
    const v=grid[idx(x,y,z)];
    let m=0;
    if(v===1||v===2) m|=1; // solid in w0
    // ghost not used now
    occ[(y+1)*S2+(z+1)*P2+(x+1)]=m;
  }
  // lamps byte map for render
  const lamps=new Uint8Array(N*N*N);
  for(const l of baseLamps){
    const xi=Math.floor(l.x), zi=Math.floor(l.zz);
    if(inb(xi,CEIL,zi)){ SET(xi,CEIL,zi,0); lamps[idx(xi,CEIL,zi)]=1; }
    // occ also: lamp cells are empty, so ensure not solid
  }
  // Recompute occ after lamp carve
  for(let y=0;y<N;y++)for(let z=0;z<N;z++)for(let x=0;x<N;x++){
    const v=grid[idx(x,y,z)];
    let m=0; if(v===1||v===2) m|=1;
    occ[(y+1)*S2+(z+1)*P2+(x+1)]=m;
  }

  // ---------- ANOMALY DEFINITIONS (30) ----------
  // Each anomaly has id 1..30, kind, applies(worldState, grid, doors, props, lamps, etc) -> revert function or data
  // For runtime we will have activeAnomalies list and functions to apply/revert per loop
  const ANOMALIES=[
    {id:1, kind:'visual', name:'Картина изменилась', desc:'Портрет в коридоре сменился', danger:0,
     apply:(state)=>{ const p=props.find(q=>q.id==='paintA'); if(p) { state._prevPaintA=p.variant; p.variant=1; p.creepy=true; } }},
    {id:2, kind:'visual', name:'Мебель сдвинута', desc:'Стул в коридоре стоит посреди прохода', danger:0,
     apply:(state)=>{ const c=props.find(q=>q.id==='chair1'); if(c){ state._px=c.x; state._pz=c.z; c.x=15.6; c.z=16; } }},
    {id:3, kind:'visual', name:'Предмет исчез', desc:'Ваза из коридора пропала', danger:0,
     apply:(state)=>{ const v=props.find(q=>q.id==='vase1'); if(v){ state._vVis=v.hidden; v.hidden=true; } }},
    {id:4, kind:'visual', name:'Новая дверь', desc:'В стене появилась дверь, которой не было', danger:1,
     apply:(state)=>{ // open a hidden door in corridor wall
        state._newDoor=false; const x=14,z=11; // west wall of corridor
        if(GET(x,IH0,z)===1){ SET(x,IH0,z,0); SET(x,IH0+1,z,0); state._newDoor={x,z,added:true}; // recompute occ
           for(let y=IH0;y<=IH0+1;y++) occ[(y+1)*S2+(z+1)*P2+(x+1)]=0;
        }
     }},
    {id:5, kind:'visual', name:'Обои изменились', desc:'Рисунок обоев в южном коридоре стал иным', danger:0,
     apply:(state)=>{ const w=props.find(q=>q.id==='wallpaperB'); if(w){ state._wp=w.variant; w.variant=1; } }},
    {id:6, kind:'visual', name:'Окно в никуда', desc:'За окном вместо города — кирпичная стена', danger:0,
     apply:(state)=>{ const win=props.find(q=>q.id==='window1'); if(win){ state._wv=win.view; win.view=1; } }},
    {id:7, kind:'visual', name:'Свет изменился', desc:'Освещение коридора стало тускло-жёлтым', danger:0,
     apply:(state)=>{ state.lightShift=1; state.lightFlicker=0.6; }},
    {id:8, kind:'visual', name:'Предмет не на месте', desc:'Ванна стоит в коридоре', danger:0,
     apply:(state)=>{ const b=props.find(q=>q.id==='bathtub1'); if(b){ state._bx=b.x; state._bz=b.z; state._bRoom=b.room; b.x=15.8; b.z=19; b.room='corrS'; } }},
    {id:9, kind:'visual', name:'Светильник сместился', desc:'Потолочный светильник висит криво', danger:0,
     apply:(state)=>{ const c=props.find(q=>q.id==='ceiling2'); if(c){ state._cx=c.x; c.x=17.2; } }},
    {id:10, kind:'visual', name:'Объект деформирован', desc:'Кукла в квартире 101 вытянулась', danger:0,
     apply:(state)=>{ const d=props.find(q=>q.id==='doll1'); if(d){ state._dScale=d.scale; d.scale=2.2; d.creepy=true; } }},

    {id:11, kind:'audio', name:'Шаги сзади', desc:'Слышны шаги за спиной', danger:0,
     apply:(state)=>{ state.audioAnomalies.push({id:11, kind:'footsteps_behind', loop:true, pos:{x:15.5,z:6}}); }},
    {id:12, kind:'audio', name:'Шёпот', desc:'Шёпот из запертой квартиры', danger:0,
     apply:(state)=>{ state.audioAnomalies.push({id:12, kind:'whisper', loop:true, pos:{x:11,z:15.5}}); }},
    {id:13, kind:'audio', name:'Стук в дверь', desc:'Стук из-за запертой двери 104', danger:0,
     apply:(state)=>{ state.audioAnomalies.push({id:13, kind:'knock', loop:true, pos:{x:20,z:16}, interval:2.2}); }},
    {id:14, kind:'audio', name:'Радиопомехи', desc:'Радио в 102 шипит искажённым голосом', danger:0,
     apply:(state)=>{ state.audioAnomalies.push({id:14, kind:'radio', loop:true, pos:{x:21.5,z:6.5}}); }},
    {id:15, kind:'audio', name:'Крик', desc:'Далёкий женский крик', danger:1,
     apply:(state)=>{ state.audioAnomalies.push({id:15, kind:'scream', loop:false, playOnce:true}); }},
    {id:16, kind:'audio', name:'Лифт без движения', desc:'Звуки лифта, хотя он стоит', danger:0,
     apply:(state)=>{ state.audioAnomalies.push({id:16, kind:'elevator', loop:true, pos:{x:19.5,z:16.5}}); }},
    {id:17, kind:'audio', name:'Шаги сверху', desc:'Топот на этаже выше', danger:0,
     apply:(state)=>{ state.audioAnomalies.push({id:17, kind:'footsteps_above', loop:true}); }},
    {id:18, kind:'audio', name:'Дыхание', desc:'Тяжёлое дыхание за стеной', danger:1,
     apply:(state)=>{ state.audioAnomalies.push({id:18, kind:'breathing', loop:true, pos:{x:15.5,z:12}}); }},

    {id:19, kind:'env', name:'Дверь заперта', desc:'Дверь в квартиру 105 внезапно заперта', danger:1,
     apply:(state)=>{ const d=baseDoors.find(q=>q.id==='d105'); if(d){ state._d105=d.open; d.open=false; setDoorGeom(d,false); } }},
    {id:20, kind:'env', name:'Коридор удлинился', desc:'Южный коридор стал вдвое длиннее', danger:0,
     apply:(state)=>{ state.corridorScale=1.8; }},
    {id:21, kind:'env', name:'Комнаты переставлены', desc:'Мебель из 101 и 102 поменялась местами', danger:0,
     apply:(state)=>{ const a=props.find(q=>q.id==='sofa1'), b=props.find(q=>q.id==='tv1'); if(a&&b){ state._sw=[a.room,b.room,a.x,b.x,a.z,b.z]; const tr=a.room; a.room=b.room; b.room=tr; const tx=a.x, tz=a.z; a.x=b.x; a.z=b.z; b.x=tx; b.z=tz; } }},
    {id:22, kind:'env', name:'Лифт не туда', desc:'Двери лифта открываются в темноту', danger:1,
     apply:(state)=>{ state.elevatorWrong=true; const d=baseDoors.find(q=>q.id==='dElev'); if(d){ state._dElev=d.open; d.open=true; setDoorGeom(d,true); } }},
    {id:23, kind:'env', name:'Аварийное освещение', desc:'Включились красные лампы', danger:0,
     apply:(state)=>{ state.emergency=true; }},
    {id:24, kind:'env', name:'Проход заблокирован', desc:'Шкаф перекрыл коридор', danger:1,
     apply:(state)=>{ state.blockCorridor=true; const x=15,z=20; SET(x,IH0,z,1); SET(x,IH0+1,z,1); occ[(IH0+1)*S2+(z+1)*P2+(x+1)]=1; occ[(IH0+2)*S2+(z+1)*P2+(x+1)]=1; state._block={x,z}; }},
    {id:25, kind:'env', name:'Комната открылась', desc:'Запертая 107 теперь открыта', danger:0,
     apply:(state)=>{ const d=baseDoors.find(q=>q.id==='d107'); if(d){ state._d107=d.open; d.open=true; setDoorGeom(d,true); } }},

    {id:26, kind:'entity', name:'Фигура в конце коридора', desc:'Тёмный силуэт стоит вдали', danger:2,
     apply:(state)=>{ state.entities.push({id:26, kind:'figure', x:15.5,z:27,y:1.2, visible:true, hostile:false}); }},
    {id:27, kind:'entity', name:'Тень', desc:'Быстрая тень пересекла комнату', danger:1,
     apply:(state)=>{ state.entities.push({id:27, kind:'shadow', x:12,z:16, moving:true, speed:6}); }},
    {id:28, kind:'entity', name:'Наблюдатель в окне', desc:'Лицо смотрит через окно', danger:1,
     apply:(state)=>{ state.entities.push({id:28, kind:'window_watcher', x:5,z:7, attachedTo:'window1'}); const w=props.find(q=>q.id==='window1'); if(w) w.entity=true; }},
    {id:29, kind:'entity', name:'Сущность за дверью', desc:'Кто-то выглядывает из приоткрытой двери', danger:2,
     apply:(state)=>{ state.entities.push({id:29, kind:'door_peek', x:20,z:16.3, y:1.3, door:'d104'}); const d=baseDoors.find(q=>q.id==='d104'); if(d){ state._d104=d.open; d.open=true; setDoorGeom(d,true); // half open handled in render
     }} } ,
    {id:30, kind:'entity', name:'Преследователь', desc:'Существо тихо идёт за игроком', danger:2,
     apply:(state)=>{ state.entities.push({id:30, kind:'follower', x:15.5,z:5, y:1.4, speed:1.35, follower:true, observer:true}); }},
  ];

  // ---------- NAV MESH (simple walkable grid) ----------
  // For collision we use grid solid check. For entities we use same.

  // ---------- RETURN ----------
  return {
    N, grid, occ, lamps, P:P2, S:S2, idx,
    baseDoors, props, documents, windows, baseLamps,
    SPAWN, CORE, ELEV_POS, STAIR_POS, DECISION_POS,
    ANOMALIES,
    IH0,IH1, CEIL,
    // factory to clone base state for each loop (we mutate props/doors per loop)
    _rawGrid: grid.slice(), // backup
  };
}
const api={makeWorld,N};
if(typeof module!=='undefined'&&module.exports) module.exports=api;
else root.World=api;
})(typeof globalThis!=='undefined'?globalThis:this);
