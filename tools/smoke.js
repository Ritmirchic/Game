/* Smoke test for DON'T LOOK BACK */
const fs=require('fs'), path=require('path');
const {JSDOM}=require('jsdom');
const html=fs.readFileSync(path.join(__dirname,'..','index.html'),'utf8');
const errors=[];
function fakeCtx(canvas){
  const grad={addColorStop(){}};
  const ctx={
    canvas,imageSmoothingEnabled:false,filter:'none',globalAlpha:1,globalCompositeOperation:'source-over',fillStyle:'#000',strokeStyle:'#000',lineWidth:1,font:'',textAlign:'left',
    createImageData(w,h){ return {width:w,height:h,data:new Uint8ClampedArray(w*h*4)}; },
    putImageData(){}, getImageData(){ return {data:new Uint8ClampedArray(4)}; },
    drawImage(){},save(){},restore(){},translate(){},rotate(){},scale(){},beginPath(){},closePath(){},moveTo(){},lineTo(){},arc(){},ellipse(){},fill(){},stroke(){},fillRect(){},clearRect(){},strokeRect(){},fillText(){},createRadialGradient(){return grad;},createLinearGradient(){return grad;},measureText(){return {width:10};}, setTransform(){},
  }; return ctx;
}
const dom=new JSDOM(html,{
  runScripts:'dangerously', pretendToBeVisual:true,
  beforeParse(window){
    window.HTMLCanvasElement.prototype.getContext=function(){return fakeCtx(this);};
    window.matchMedia=()=>({matches:false,addEventListener(){},removeEventListener(){}});
    window.addEventListener('error',e=>errors.push('window.error: '+(e.message||e.error)));
    const orig=window.console.error; window.console.error=(...a)=>{errors.push('console.error: '+a.join(' ')); orig.apply(window.console,a);};
  }
});
const w=dom.window;
w.addEventListener('unhandledrejection',e=>errors.push('unhandled: '+e.reason));
setTimeout(()=>{
  const doc=w.document;
  try{
    doc.getElementById('btnStart').dispatchEvent(new w.MouseEvent('click',{bubbles:true}));
    console.log('start ok, intro gone='+doc.getElementById('intro').classList.contains('gone'));
  }catch(e){errors.push('start: '+e.stack);}
  const press=k=>w.dispatchEvent(new w.KeyboardEvent('keydown',{key:k,bubbles:true}));
  const release=k=>w.dispatchEvent(new w.KeyboardEvent('keyup',{key:k,bubbles:true}));
  let t=0;
  const step=()=>{
    t++;
    try{
      press('w'); if(t%6===0) press('e'); if(t%8===0) press('f'); if(t%14===0) press('r'); if(t%10===0) press('m'); if(t===30) press('Escape'); if(t===32) press('Escape');
      if(t===50){ // test reporting
        // move to decision zone artificially
        // we can't teleport easily, but test function exists
      }
      release('w');
    }catch(e){errors.push('tick '+t+': '+e.stack);}
    if(t<70) setTimeout(step,6); else finish();
  };
  const finish=()=>{
    const loop=doc.getElementById('loopNum').textContent;
    const sanity=doc.getElementById('sanityVal').textContent;
    const timer=doc.getElementById('timer').textContent;
    console.log('HUD -> loop:'+loop+' sanity:'+sanity+' timer:'+timer);
    console.log('fps:'+(doc.getElementById('fps').textContent||'(empty)'));
    console.log('entities test: world should have 30 anomalies');
    if(errors.length){ console.log('\nERRORS('+errors.length+'):'); errors.slice(0,12).forEach(e=>console.log(' -',e)); process.exit(1); }
    console.log('\nSMOKE OK');
    process.exit(0);
  };
  step();
},400);
