"""Original deterministic layered sprite/texture generation. Pillow only."""
from PIL import Image, ImageDraw, ImageFilter
import random, math, os
random.seed(74291)
ROOT=os.path.join(os.path.dirname(__file__),'../assets');os.makedirs(ROOT,exist_ok=True)
pal=[(95,178,215),(177,108,238),(224,132,69),(245,77,128),(80,222,191)]
for sector,c in enumerate(pal):
    w,h=1280,800
    im=Image.new('RGB',(w,h),(3,7,17)); cloud=Image.new('RGB',(320,200));p=cloud.load()
    for y in range(200):
        for x in range(320):
            v=(math.sin(x*.024+y*.035+sector)*.5+.5)*(.5+.5*math.sin(x*.048-y*.026))
            v*=math.exp(-((y-95-28*math.sin(x*.018)) / 65)**2)*.32
            p[x,y]=(int(3+c[0]*v),int(7+c[1]*v),int(17+c[2]*v))
    im=cloud.resize((w,h),Image.Resampling.BICUBIC);d=ImageDraw.Draw(im)
    for i in range(650):
        x,y=random.randrange(w),random.randrange(h);v=random.randrange(55,190);r=random.choice([0,0,0,1])
        d.ellipse((x-r,y-r,x+r,y+r),fill=(v,v,min(255,v+30)))
        if i%120==0:d.line((x-4,y,x+4,y),fill=(50,100,120));d.line((x,y-4,x,y+4),fill=(50,100,120))
    im.save(f'{ROOT}/nebula_{sector}.jpg',quality=91)
    for variant in range(5):
        S=160;im=Image.new('RGBA',(S,S));mask=Image.new('L',(S,S));d=ImageDraw.Draw(mask)
        n=13;rs=[random.uniform(48,70) for i in range(n)];pts=[(80+math.cos(i*math.tau/n)*rs[i],80+math.sin(i*math.tau/n)*rs[i]) for i in range(n)]
        d.polygon(pts,fill=255);tex=Image.new('RGBA',(S,S));p=tex.load()
        for y in range(S):
            for x in range(S):
                light=.52+.35*(1-(x+y)/320)+random.uniform(-.13,.13)+.08*math.sin(x*.4+y*.3)
                base=(90+sector*6,100-sector*4,115+sector*6)
                p[x,y]=tuple(int(v*light) for v in base)+(255,)
        d=ImageDraw.Draw(tex)
        for i in range(20):
            x,y=random.randrange(20,140),random.randrange(20,140);r=random.randrange(3,14)
            d.ellipse((x-r,y-r,x+r,y+r),fill=(30,35,44),outline=(110,118,126),width=2)
        for i in range(13):
            x,y=random.randrange(25,135),random.randrange(25,135)
            d.polygon([(x,y-6),(x+4,y),(x,y+7),(x-3,y+1)],fill=c+(255,));d.line((x,y-5,x,y+2),fill=(220,245,255),width=1)
        im.paste(tex,(0,0),mask);im.save(f'{ROOT}/rock_{sector}_{variant}.png')

def ship(name,kind,c):
    im=Image.new('RGBA',(160,160));d=ImageDraw.Draw(im)
    # The prow faces right; separate engine pods, wing plates, radiator ribs and mining arms.
    d.polygon([(22,39),(66,26),(107,42),(126,66),(143,80),(126,94),(107,118),(66,134),(22,121),(39,99),(39,61)],fill=(13,23,33),outline=c,width=3)
    for y in (38,103):
        d.rounded_rectangle((16,y,83,y+20),5,fill=(55,68,78),outline=(132,153,162),width=2)
        d.rectangle((13,y+4,28,y+16),fill=(5,15,23),outline=c,width=2)
        for x in range(38,70,7):d.line((x,y+3,x-6,y+17),fill=(20,32,44),width=3)
    d.polygon([(41,61),(103,48),(137,80),(103,112),(41,99)],fill=(85,106,119),outline=(170,195,205),width=2)
    d.polygon([(57,67),(103,59),(125,80),(103,101),(57,93)],fill=(30,48,62),outline=c,width=2)
    d.polygon([(86,66),(108,74),(116,80),(108,86),(86,94)],fill=c,outline=(205,245,255),width=2)
    for y in (56,100):
        d.rectangle((85,y,136,y+5),fill=(28,34,40),outline=(153,165,173));d.rectangle((131,y,143,y+5),fill=c)
    for y in (49,108):d.rectangle((61,y,70,y+3),fill=(255,182,78))
    if kind>0:
        d.polygon([(35,23),(70,14),(112,32),(72,39)],fill=(54,75,87),outline=c,width=2)
        d.polygon([(35,137),(70,146),(112,128),(72,121)],fill=(54,75,87),outline=c,width=2)
    if kind>1:
        for y in (7,137):d.rounded_rectangle((29,y,82,y+16),4,fill=(68,80,90),outline=c,width=2)
    im.save(f'{ROOT}/{name}.png')
for i,c in enumerate([(100,226,221),(240,178,91),(164,144,246)]):ship('ship_'+str(i),i,c)
for i,c in enumerate(pal):ship('enemy_'+str(i),i%3,(255,75+i*14,70+i*22))
ship('drone',0,(248,201,105))
# Orbital refinery silhouette, docking bay and segmented solar arrays.
im=Image.new('RGBA',(512,512));d=ImageDraw.Draw(im)
for x in (34,344):
    d.rectangle((x,92,x+134,420),fill=(13,31,49),outline=(73,115,133),width=4)
    for y in range(100,410,29):
        for xx in range(x+6,x+128,24):d.rectangle((xx,y,xx+18,y+20),fill=(27,65,91),outline=(49,95,118))
d.rectangle((145,220,367,290),fill=(67,83,93),outline=(151,174,181),width=3)
d.regular_polygon((256,256,125),8,rotation=22.5,fill=(44,62,73),outline=(146,180,188),width=5)
d.regular_polygon((256,256,94),8,rotation=22.5,fill=(15,31,44),outline=(75,216,210),width=3)
d.rectangle((206,223,306,289),fill=(3,11,23),outline=(255,187,92),width=3)
for y in (203,303):
    for x in range(201,310,14):d.rectangle((x,y,x+7,y+4),fill=(100,243,227))
im.save(f'{ROOT}/station.png')
colors=[(155,170,180),(219,145,92),(155,185,163),(122,170,208),(210,220,236),(246,199,92),(197,207,219),(152,226,88),(164,128,246),(246,113,172),(87,238,216),(241,170,99),(100,166,250),(241,235,171),(184,96,241)]
for i,c in enumerate(colors):
    im=Image.new('RGBA',(64,64));d=ImageDraw.Draw(im)
    d.polygon([(13,43),(9,28),(23,10),(41,8),(55,25),(49,46),(31,56)],fill=tuple(int(v*.45) for v in c),outline=c,width=2)
    d.polygon([(23,10),(41,8),(34,31),(9,28)],fill=c);d.polygon([(34,31),(55,25),(49,46),(31,56)],fill=tuple(int(v*.7) for v in c))
    d.line((23,10,34,31,31,56),fill=(232,248,255),width=2)
    im.save(f'{ROOT}/mineral_{i}.png')
im=Image.open(f'{ROOT}/ship_0.png').resize((256,256));im.save(f'{ROOT}/icon.png');im.save(f'{ROOT}/icon.ico',sizes=[(16,16),(32,32),(48,48),(128,128),(256,256)])
print('Generated 5 nebulae, 25 textured asteroid variants, 9 ships, 15 minerals, refinery and application icon.')
