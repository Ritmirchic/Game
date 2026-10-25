# Оффлайн-превью той же математики, что в index.html (для проверки композиции кадра)
import math, random, sys
from PIL import Image, ImageDraw

WORLD=58; W_DIST=3.2; FOCAL=(900/2)/math.tan(math.radians(78)/2); CX=720; CY=450
V4=[[(1 if i&1 else -1),(1 if i&2 else -1),(1 if i&4 else -1),(1 if i&8 else -1)] for i in range(16)]
EDGES=[(i,i^(1<<b),b) for i in range(16) for b in range(4) if (i^(1<<b))>i]

def mul(a,b):
    o=[0]*16
    for r in range(4):
        for c in range(4):
            o[r*4+c]=sum(a[r*4+k]*b[k*4+c] for k in range(4))
    return o
def rot(i,j,a):
    m=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]; c,s=math.cos(a),math.sin(a)
    m[i*4+i]=c; m[i*4+j]=-s; m[j*4+i]=s; m[j*4+j]=c; return m
def app(m,v): return [sum(m[r*4+k]*v[k] for k in range(4)) for r in range(4)]
def world(t,pulse=0):
    m=rot(1,3,t*0.118+pulse*0.6)
    m=mul(m,rot(0,2,t*0.072-pulse*0.4))
    m=mul(m,rot(2,3,t*0.096+pulse*0.5))
    m=mul(m,rot(0,3,t*0.052))
    m=mul(m,rot(0,1,t*0.041))
    return m
def proj4(v):
    k=1.9/(W_DIST-v[3]); return [v[0]*k,v[1]*k,v[2]*k]
def render(t, path, camz=WORLD*2.6, shards=None):
    img=Image.new('RGB',(1440,900),(5,6,15)); d=ImageDraw.Draw(img)
    m=world(t)
    P=[]
    for v4 in V4:
        v=app(m,v4); p=proj4(v); w=[p[0]*WORLD,p[1]*WORLD,p[2]*WORLD]
        # камера смотрит в -Z (yaw=PI), поэтому z_cam = camz - wz, x_cam = -wx
        z=camz-w[2]
        P.append((v[3],[-w[0],w[1],w[2]],z))
    for a,b,axis in EDGES:
        za,zb=P[a][2],P[b][2]
        wa,wb=P[a][1],P[b][1]
        if za<0.6 and zb<0.6: continue
        if za<0.6 or zb<0.6:
            tt=(0.6-za)/(zb-za)
            ix=wa[0]+(wb[0]-wa[0])*tt; iy=wa[1]+(wb[1]-wa[1])*tt
            if za<0.6: wa=[ix,iy,0.6]; za=0.6
            else: wb=[ix,iy,0.6]; zb=0.6
        dnear=min(za,zb)
        sala=min(1,(0.95 if axis==3 else 0.62)*(1-min(1,dnear/(WORLD*5.5)))+0.10)
        if dnear<9: sala*=max(0.12,dnear/9)
        col=(255,90,230) if axis==3 else (90,235,255)
        col=tuple(int(c*sala) for c in col)
        x1=CX+wa[0]/za*FOCAL; y1=CY-wa[1]/za*FOCAL
        x2=CX+wb[0]/zb*FOCAL; y2=CY-wb[1]/zb*FOCAL
        d.line([(x1,y1),(x2,y2)],fill=col,width=3 if axis==3 else 2)
    if shards:
        for (x,y,z) in shards:
            if z<0.6: continue
            px=CX+x/z*FOCAL; py=CY-y/z*FOCAL
            r=min(60,900/z)
            if 0<px<1440 and 0<py<900:
                d.ellipse([px-r/2,py-r/2,px+r/2,py+r/2],fill=(255,120,220))
    img.save(path)
    print('saved',path)

if __name__=='__main__':
    for t in [0,3,7,12,19]:
        render(t, f'/home/user/Game/tools/preview_{t}.png')
