"""Deterministic original procedural surface art, crew portraits, and synthesized SFX."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from pathlib import Path
import random, math, wave, struct
ROOT=Path(__file__).resolve().parents[1]/'assets'
ROOT.mkdir(exist_ok=True)
r=random.Random(9841)
for kind in ['alloy','rock']:
 im=Image.new('RGB',(256,256)); p=im.load()
 for y in range(256):
  for x in range(256):
   n=r.randrange(-18,19)
   if kind=='alloy':
    v=85+n+(8 if y%64<3 or x%64<3 else 0); p[x,y]=(v, int(v*1.1),int(v*1.2))
   else:
    v=int(80+18*math.sin(x*.12)*math.cos(y*.14)+n);p[x,y]=(v,int(v*.87),int(v*.75))
 d=ImageDraw.Draw(im)
 if kind=='alloy':
  for y in range(0,256,64):
   for x in range(0,256,64):
    d.line((x+4,y+4,x+60,y+4),fill=(135,157,170),width=1)
    for dx,dy in [(8,8),(56,56)]:d.ellipse((x+dx-2,y+dy-2,x+dx+2,y+dy+2),fill=(27,39,48))
 im.save(ROOT/f'{kind}.png')
# Forty individually seeded, illustrated astronaut ID portraits (not generated-image placeholders).
for i in range(40):
 rr=random.Random(2100+i); w,h=320,420
 im=Image.new('RGB',(w,h),(9,16,30));d=ImageDraw.Draw(im)
 hue=[(51,142,157),(91,119,177),(150,94,177),(169,106,64),(71,163,133)][i%5]
 for y in range(h):
  q=y/h;d.line((0,y,w,y),fill=tuple(int(c*(.22+.35*q)) for c in hue))
 for j in range(35):
  x,y=rr.randrange(w),rr.randrange(h);d.point((x,y),fill=(90,140,160))
 # Graphic orbital paths, serial marks, and suit architecture.
 d.arc((-120,20,440,540),200,350,fill=hue,width=3)
 d.ellipse((48,42,272,276),fill=(16,28,43),outline=hue,width=8)
 shoulder=rr.randrange(8,26)
 d.polygon([(12,420),(25,330),(92,285),(228,285),(295,330),(310,420)],fill=(32+shoulder,54+shoulder,69+shoulder))
 d.polygon([(95,283),(130,315),(190,315),(224,283),(216,350),(104,350)],fill=(15,28,39),outline=hue)
 skin=[(178,125,91),(119,77,59),(211,164,126),(88,63,55),(205,155,130)][i%5]
 facew=rr.randrange(59,75)
 d.rounded_rectangle((160-facew,91,160+facew,263),radius=60,fill=skin)
 d.polygon([(104,239),(128,279),(190,279),(218,239),(215,291),(106,291)],fill=(24,39,53))
 hair=[(35,28,35),(68,44,30),(170,172,175),(25,24,28),(131,67,40)][(i//3)%5]
 d.pieslice((160-facew-4,60,160+facew+4,170),180,360,fill=hair)
 if i%3==0:d.polygon([(89,136),(102,84),(155,58),(217,88),(231,145),(202,111),(147,103)],fill=hair)
 eyey=157+rr.randrange(-6,7)
 for x in [134,187]:
  d.line((x-12,eyey-11,x+9,eyey-13),fill=hair,width=4)
  d.rounded_rectangle((x-10,eyey-2,x+10,eyey+3),2,fill=(25,34,42))
  d.rectangle((x-1,eyey-2,x+2,eyey+1),fill=(146,216,220))
 d.line((161,174,154,199,169,201),fill=tuple(int(v*.72) for v in skin),width=3)
 d.line((143,224,176,224+rr.randrange(-3,4)),fill=(75,44,47),width=3)
 if i%4==0:d.polygon([(119,216),(138,249),(183,250),(204,216),(191,259),(134,259)],fill=hair)
 if i%6==0:
  d.rounded_rectangle((111,145,209,175),8,outline=(98,233,231),width=3)
 if i%7==0:d.line((199,160,187,185,203,197),fill=(233,190,162),width=2)
 # Helmet rim and personalized tech implant.
 d.arc((58,39,262,290),180,360,fill=(149,179,192),width=12)
 for x in [60,241]:
  d.rounded_rectangle((x,131,x+20,214),6,fill=(43,62,78),outline=hue,width=3)
 d.rectangle((68,327,101,388),fill=hue)
 d.rectangle((227,327,260,388),fill=hue)
 d.rounded_rectangle((119,354,202,397),6,fill=(12,23,34),outline=(101,144,157),width=2)
 for j in range(4):d.line((130,365+j*7,182-rr.randrange(25),365+j*7),fill=hue,width=2)
 d.text((18,18),f'HELIOS / {i+1:03d}',fill=(177,218,222))
 d.text((19,397),f'DEEP BELT PERSONNEL    {chr(65+i%26)}-{201+i}',fill=(152,187,195))
 im.save(ROOT/f'crew_{i:02d}.png')
for name,freq,dur in [('pulse',240,.13),('mine',100,.18),('collect',660,.18),('explosion',60,.45),('click',480,.06)]:
 sr=22050;rr=random.Random(23);samples=[]
 for j in range(int(sr*dur)):
  t=j/sr;env=(1-t/dur)**2
  v=math.sin(2*math.pi*freq*t*(1-.45*t/dur))
  if name in ['mine','explosion']:v=.7*rr.uniform(-1,1)+.3*v
  samples.append(struct.pack('<h',int(v*env*9000)))
 with wave.open(str(ROOT/f'{name}.wav'),'wb') as f:f.setparams((1,2,sr,0,'NONE',''));f.writeframes(b''.join(samples))
print('Generated 40 original portraits, 2 surface textures, 5 sound effects.')
icon=Image.new('RGBA',(256,256),(8,21,35,255));d=ImageDraw.Draw(icon)
d.regular_polygon((128,128,105),6,rotation=30,outline=(76,222,212),width=9)
d.polygon([(128,42),(181,186),(128,158),(75,186)],fill=(213,244,237))
d.polygon([(128,176),(144,194),(128,230),(112,194)],fill=(255,177,93))
icon.save(ROOT/'icon.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
