/* ============================================================
   DON'T LOOK BACK — Horror Ray-Voxel Renderer
   Voxel walls + sprites + flashlight + volumetric fog
   ============================================================ */
(function(root){
'use strict';
const WCOL_HORROR=[[58,38,28],[85,55,42],[120,90,70],[45,55,65]];
const SIN=new Float32Array(256); for(let i=0;i<256;i++) SIN[i]=Math.sin(i/256*Math.PI*2);
const COS=new Float32Array(256); for(let i=0;i<256;i++) COS[i]=Math.cos(i/256*Math.PI*2);

function render(world,cam,W,H,pix,depth,time){
  const tanX=cam.fov, tanY=cam.fov*H/W;
  const yaw=cam.yaw, pitch=cam.pitch;
  const sy=Math.sin(yaw), cy=Math.cos(yaw);
  const sp=Math.sin(pitch), cp=Math.cos(pitch);
  const fx=sy*cp, fy=sp, fz=cy*cp;
  const rx=cy, ry=0, rz=-sy;
  const ux=-sy*sp, uy=cp, uz=-cy*sp;
  const ox=cam.x, oy=cam.y, oz=cam.z;
  const occ=cam.occ, P=cam.P, S=P*P;
  const lamps=cam.lamps;
  const flashOn=cam.flashOn, flashIntensity=cam.flashOn?1:0.15;
  const emergency=cam.emergency;
  const corridorScale=cam.corridorScale||1;
  const wallpaperVariant=cam.wallpaperVariant||0;
  const lightShift=cam.lightShift||0;
  const lightFlicker=cam.lightFlicker||0;
  const flickerPhase=time*8;

  const MAXD=42, MAXSTEPS=70;
  const tm=time*40;
  let px=0;
  for(let y=0;y<H;y++){
    const v=(1-2*(y+0.5)/H)*tanY;
    const bx=fx+ux*v, by=fy+uy*v, bz=fz+uz*v;
    for(let x=0;x<W;x++,px++){
      const u=(2*(x+0.5)/W-1)*tanX;
      let dx=bx+rx*u, dy=by+ry*u, dz=bz+rz*u;
      const il=1/Math.sqrt(dx*dx+dy*dy+dz*dz); dx*=il; dy*=il; dz*=il;
      let mx=ox|0, my=oy|0, mz=oz|0;
      let base=(my+1)*S+(mz+1)*P+(mx+1);
      const ax=dx<0?-dx:dx, ay=dy<0?-dy:dy, az=dz<0?-dz:dz;
      const ddx=ax<1e-9?1e30:1/ax, ddy=ay<1e-9?1e30:1/ay, ddz=az<1e-9?1e30:1/az;
      let stx,sty,stz,sdx,sdy,sdz,bX,bY,bZ;
      if(dx<0){stx=-1;bX=-1;sdx=(ox-mx)*ddx;}else{stx=1;bX=1;sdx=(mx+1-ox)*ddx;}
      if(dy<0){sty=-1;bY=-S;sdy=(oy-my)*ddy;}else{sty=1;bY=S;sdy=(my+1-oy)*ddy;}
      if(dz<0){stz=-1;bZ=-P;sdz=(oz-mz)*ddz;}else{stz=1;bZ=P;sdz=(mz+1-oz)*ddz;}
      let cr=0,cg=0,cb=0,A=0,tEnter=0,axis=0,solidD=MAXD,any=0;
      for(let i=0;i<MAXSTEPS;i++){
        const ob=occ[base];
        if(ob===255) break;
        // lamp glow
        if(lamps&&lamps[base]&&tEnter>0.05){
          const hx=ox+dx*tEnter, hy=oy+dy*tEnter, hz=oz+dz*tEnter;
          const core=1-Math.min(1,(Math.abs(hx-mx-0.5)+Math.abs(hy-my-0.5)+Math.abs(hz-mz-0.5))*1.6);
          // flicker
          let fl=0.88+0.12*SIN[(tm*3)&255];
          if(lightFlicker>0) fl*=0.5+0.5*Math.sin(flickerPhase+mx*2.1);
          if(emergency) fl*=0.8+0.3*Math.sin(time*6+mx);
          const inten=(0.45+0.55*core*core)*fl*(1-Math.min(0.8,tEnter*0.03));
          let lr=250,lg=220,lb=160;
          if(emergency){ lr=255; lg=30; lb=20; }
          else if(lightShift){ lr=240; lg=190; lb=120; }
          cr+=(1-A)*inten*lr;
          cg+=(1-A)*inten*lg;
          cb+=(1-A)*inten*lb;
          A+=(1-A)*inten*0.7;
          if(A>0.97){ solidD=tEnter; break; }
        }
        if(ob&1){
          const hx=ox+dx*tEnter, hy=oy+dy*tEnter, hz=oz+dz*tEnter;
          let f1,f2,shade;
          if(axis===0){ f1=hy-(hy|0); f2=hz-(hz|0); shade=dx>0?0.68:0.82; }
          else if(axis===1){ f1=hx-(hx|0); f2=hz-(hz|0); shade=dy>0?0.52:1.0; }
          else{ f1=hx-(hx|0); f2=hy-(hy|0); shade=dz>0?0.62:0.76; }
          const e1=f1<0.5?f1:1-f1, e2=f2<0.5?f2:1-f2;
          let line=0; if(e1<0.03||e2<0.03) line=1; else if(e1<0.06||e2<0.06) line=0.35;
          // flashlight contribution
          const toHitX=hx-ox, toHitY=hy-oy, toHitZ=hz-oz;
          const dist=Math.sqrt(toHitX*toHitX+toHitY*toHitY+toHitZ*toHitZ);
          const dot=(toHitX*fx+toHitY*fy+toHitZ*fz)/ (dist||1); // alignment with view
          let flash=0;
          if(dot>0.7){
            const angleAtt=Math.pow(dot, 28); // narrow cone
            const distAtt=1/(1+dist*0.18);
            const spot= angleAtt*distAtt* (flashOn?1.8:0.35);
            flash=spot;
            // also add outer soft halo
            const dot2=(dot-0.7)/0.3;
            if(dot2>0) flash+=dot2*0.15*distAtt*(flashOn?0.6:0.15);
          }
          // base wall color by position: corridor vs apartment
          let baseR,baseG,baseB;
          // determine room type via position
          const isCorridor=(mx>=14&&mx<=17);
          const isApt=(mx>=5&&mx<=12)||(mx>=20&&mx<=27);
          if(my===3){ // ceiling
            baseR=28; baseG=28; baseB=32;
            shade*=0.9;
          } else if(emergency){
            baseR=58+flash*40; baseG=12; baseB=12;
          } else if(isCorridor){
            // wallpaper dirty beige with variation
            const pat=SIN[((hx*11+hz*7)|0)&255];
            baseR=92+pat*10+ (wallpaperVariant?12:0);
            baseG=84+pat*8 + (wallpaperVariant?-10:0);
            baseB=68+pat*6 + (wallpaperVariant?15:0);
            if(wallpaperVariant){
              // red pattern shift
              baseR+=14*Math.sin(hx*6+hz*4);
              baseG-=6;
            }
            if(lightShift){ baseR+=12; baseG+=6; baseB-=8; }
          } else if(isApt){
            baseR=88; baseG=82; baseB=76;
            // apartment paint color slight variation
            const hv=(mx*31+mz*17)&3;
            if(hv===0){ baseR+=6; baseG+=4; }
          } else {
            baseR=70; baseG=68; baseB=64;
          }
          // window special: if at window wall positions, render window frame + view
          let isWindow=false;
          if((mx===5&&mz===7)||(mx===27&&mz===7)||(mx===5&&mz===23)||(mx===27&&mz===23)){
            // mark window area (y 1..2)
            if(my>=1&&my<=2 && f1>0.18 && f1<0.82 && f2>0.1 && f2<0.9){
              isWindow=true;
              // window glass color depends on anomaly view
              let winView=cam.windowViews? cam.windowViews[mx+','+mz] : 0;
              if(winView===1){
                baseR=35; baseG=22; baseB=20; // brick
                // add brick lines?
                line=0.7;
              } else {
                baseR=18; baseG=28; baseB=52; // night sky
                // faint city lights?
                const city=Math.random()<0.02?1:0; // not deterministic but ok
              }
            }
          }
          const fog0=tEnter*0.045;
          const fog=fog0/(1+fog0), keep=1-fog;
          // apply shading + flashlight
          let r=baseR*shade, g=baseG*shade, b=baseB*shade;
          // flashlight adds
          r+=flash*120; g+=flash*110; b+=flash*95;
          if(emergency && !isWindow) { r+= (0.15+0.1*Math.sin(time*5+mx))*22; }
          // distance fog to dark
          r=r*keep + 6*fog + flash*10*fog;
          g=g*keep + 7*fog + flash*9*fog;
          b=b*keep + 10*fog;
          // edge lines for wallpaper seams
          r+=line*18*keep; g+=line*16*keep; b+=line*14*keep;
          // vignette-like darkening at distance
          cr+=(1-A)*r; cg+=(1-A)*g; cb+=(1-A)*b;
          A=1; solidD=tEnter; any=1;
          break;
        }
        if(sdx<sdy){ if(sdx<sdz){ tEnter=sdx; mx+=stx; base+=bX; sdx+=ddx; axis=0; } else{ tEnter=sdz; mz+=stz; base+=bZ; sdz+=ddz; axis=2; } }
        else { if(sdy<sdz){ tEnter=sdy; my+=sty; base+=bY; sdy+=ddy; axis=1; } else{ tEnter=sdz; mz+=stz; base+=bZ; sdz+=ddz; axis=2; } }
        if(tEnter>MAXD) break;
      }
      let R,G,B;
      if(A<0.999){
        // skybox inside building not visible, but we are outside? Actually outside windows sky, else dark interior fog
        const dr=6+10*up(dy), dg=7+11*up(dy), db=12+18*up(dy);
        // add flashlight bloom to empty
        let fr=0,fg=0,fb=0;
        if(tEnter>18){ // far distance add slight fog color
          fr=6; fg=7; fb=10;
        }
        R=cr+(1-A)*dr+fr; G=cg+(1-A)*dg+fg; B=cb+(1-A)*db+fb;
        // if emergency, tint sky red
        if(emergency){ R*=(1-A)*0.2 + A*1; R+= (1-A)*14; G*=(1-A)*0.5+ A*1; B*=(1-A)*0.5 + A*1; }
      } else { R=cr; G=cg; B=cb; }
      // flashlight center halo (screen-space) subtle
      // post process brightness based on distance and flashlight already handled
      // apply subtle vignette
      const vx=(x/W-0.5), vy=(y/H-0.5);
      const vign=1- (vx*vx+vy*vy)*0.45;
      R*=vign; G*=vign; B*=vign;
      pix[px]=0xff000000| ((B>255?255:B)<<16) | ((G>255?255:G)<<8) | (R>255?255:R);
      depth[px]=any?solidD:MAXD;
    }
  }
  return {W,H};
}
function up(dy){ const v=dy*1.05+0.08; return v<0?0:v>1?1:v; }

const api={render,WCOL:WCOL_HORROR};
if(typeof module!=='undefined'&&module.exports) module.exports=api;
else root.Render=api;
})(typeof globalThis!=='undefined'?globalThis:this);
