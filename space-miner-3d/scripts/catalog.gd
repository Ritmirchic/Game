extends RefCounted

const MODULES = {
 "Hull": {"price":80,"mass":8.0,"hp":100,"power":0,"desc":"Structural plate • expands your connected hull"},
 "Engine": {"price":200,"mass":12.0,"hp":80,"power":8,"desc":"Directional ion drive • rotation changes thrust direction"},
 "Mining laser": {"price":240,"mass":10.0,"hp":65,"power":12,"desc":"18 damage/s • mines rock along the aiming reticle"},
 "Magnet": {"price":300,"mass":14.0,"hp":90,"power":6,"desc":"Attracts drifting ore within 18m"},
 "Pulse cannon": {"price":350,"mass":16.0,"hp":90,"power":10,"desc":"Physical pulse projectiles • 16 damage"},
 "Rocket launcher": {"price":650,"mass":22.0,"hp":80,"power":14,"desc":"High-impact projectiles • 45 damage"},
 "Shield": {"price":400,"mass":18.0,"hp":80,"power":8,"desc":"Adds 80 rechargeable shield points"},
 "Cargo": {"price":180,"mass":8.0,"hp":90,"power":0,"desc":"Adds 50 ore capacity; loaded ore adds mass"},
 "Generator": {"price":420,"mass":24.0,"hp":100,"power":-25,"desc":"Adds 25 energy/s regeneration"},
 "Repair": {"price":500,"mass":15.0,"hp":75,"power":10,"desc":"Repairs damaged ship modules during flight"},
 "Scanner": {"price":260,"mass":8.0,"hp":60,"power":4,"desc":"Reveals the mineral type and remaining integrity of the target"}
}
const SECTORS = [
 {"name":"STARTER BELT","sub":"Helios extraction zone","color":Color("42c9c2"),"price":0,"hardness":1.0,"ore":"Iron","value":16},
 {"name":"CRYSTAL NEBULA","sub":"The violet shoals","color":Color("b58aff"),"price":1200,"hardness":1.4,"ore":"Crystal","value":27},
 {"name":"IRON GRAVEYARD","sub":"Wrecks of the lost fleet","color":Color("e7a365"),"price":2400,"hardness":1.8,"ore":"Titanium","value":39},
 {"name":"PLASMA STORM","sub":"Electrical exclusion zone","color":Color("6ea4ff"),"price":4500,"hardness":2.2,"ore":"Iridium","value":55},
 {"name":"VOID FRONTIER","sub":"Beyond the last beacon","color":Color("f472a4"),"price":8000,"hardness":2.8,"ore":"Voidstone","value":80}
]
const FIRST = ["Ada","Soren","Nia","Voss","Iris","Kei","Mara","Sol","Orin","Lyra","Hale","Zara","Rook","Esme","Tao","Nova","Axel","Lena","Rune","Yara","Dax","Mira","Oren","Vera","Kira","Enzo","Thea","Cass","Juno","Arlo","Echo","Faye","Ivo","Lumi","Nox","Opal","Pax","Rhea","Sage","Zenith"]
const LAST = ["Vega","Thorne","Kade","Ash","Reyes","Vale","Sato","Dune"]
const RARITIES = ["Common","Uncommon","Rare","Epic","Legendary","Mythic","Secret"]
const COLORS = [Color("a3b4bf"),Color("79d6a3"),Color("66baff"),Color("bf8aff"),Color("ffc968"),Color("ff8bc3"),Color("f3f7ff")]
const ROLES = ["Miner","Gunner","Pilot","Engineer","Collector","Commander","Scientist"]

static func crew(id: int) -> Dictionary:
 var r: int = mini(id / 6, 6)
 var role: int = id % 7
 var bonus: float = [0.08,0.15,0.4,0.5,1.0,2.0,3.0][r]
 if r == 4 and role == 1: bonus = 2.0
 if r == 5 and role == 0: bonus = 4.0
 return {"id":id,"name":FIRST[id]+" "+LAST[id % 8],"rarity":r,"role":ROLES[role],"bonus":bonus,"number":"%03d / 040" % (id+1)}

static func roll_card(rng: RandomNumberGenerator) -> int:
 var roll: float = rng.randf()*100.0
 var rarity: int = 0
 if roll < 0.2: rarity = 6
 elif roll < 1.0: rarity = 5
 elif roll < 4.0: rarity = 4
 elif roll < 12.0: rarity = 3
 elif roll < 30.0: rarity = 2
 elif roll < 60.0: rarity = 1
 var ids: Array[int] = []
 for i in range(40):
  if mini(i / 6,6) == rarity: ids.append(i)
 return ids[rng.randi_range(0, ids.size()-1)]
