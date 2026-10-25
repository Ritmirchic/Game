import math
from preview import world, V4, app, proj4, WORLD, FOCAL, CX, CY
def stats(camr, center):
    rows=[]
    for T in [i*0.5 for i in range(80)]:
        m=world(T); pts=[]
        for v4 in V4:
            v=app(m,v4); p=proj4(v); w=[p[0]*WORLD,p[1]*WORLD,p[2]*WORLD]
            z=camr*WORLD-w[2]; x=-w[0]; y=w[1]
            if z<0.6: continue
            pts.append((CX+x/z*FOCAL, CY-y/z*FOCAL, z))
        if len(pts)<8: rows.append(None); continue
        mx=sum(p[0] for p in pts)/len(pts); my=sum(p[1] for p in pts)/len(pts)
        if center:
            ox=(mx-CX)*center; oy=(my-CY)*center
            pts=[(p[0]-ox,p[1]-oy,p[2]) for p in pts]
        xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
        span=max(max(xs)-min(xs), max(ys)-min(ys))
        off=math.hypot((max(xs)+min(xs))/2-CX,(max(ys)+min(ys))/2-CY)
        rows.append((span,off,len(pts)))
    rows=[r for r in rows if r]
    spans=sorted(r[0] for r in rows); offs=sorted(r[1] for r in rows)
    return f"span min/med/max {int(spans[0])}/{int(spans[len(spans)//2])}/{int(spans[-1])}  offcenter med/max {int(offs[len(offs)//2])}/{int(offs[-1])}  behind~{sum(1 for r in rows if r[2]<16)}"
for camr in [1.78, 2.2, 2.6, 3.0]:
    for c in [0, 0.5, 0.75, 1.0]:
        print(f"cam={camr} center={c}: {stats(camr,c)}")
