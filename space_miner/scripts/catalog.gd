extends RefCounted

const RESOURCES = [
 ["Iron",12,1.0,"COMMON"], ["Copper",18,1.1,"COMMON"], ["Nickel",25,1.2,"COMMON"],
 ["Titanium",42,1.5,"UNCOMMON"], ["Silver",60,1.6,"UNCOMMON"], ["Gold",95,1.9,"RARE"],
 ["Platinum",140,2.2,"RARE"], ["Uranium",180,2.5,"RARE"], ["Amethyst",110,1.8,"RARE"],
 ["Plasma ore",245,3.0,"EXOTIC"], ["Void crystal",520,4.0,"LEGENDARY"], ["Cobalt",65,1.6,"UNCOMMON"],
 ["Sapphire",155,2.2,"RARE"], ["Iridium",310,3.2,"EXOTIC"], ["Dark matter",850,4.5,"LEGENDARY"]]
const SECTORS = [
 ["STARTER BELT","Helix system · Survey zone 01",0,[0,1,2],"Drifting debris",Color("68dfd4")],
 ["CRYSTAL NEBULA","Veil system · Survey zone 02",1200,[3,4,8,12],"Crystal resonance",Color("b294f5")],
 ["IRON GRAVEYARD","Kestrel system · Survey zone 03",3600,[5,6,7,11],"Raider patrols",Color("f1b271")],
 ["PLASMA STORM","Arc system · Survey zone 04",8000,[9,12,13],"Electrical discharges",Color("f579ac")],
 ["VOID FRONTIER","Unknown system · Survey zone 05",16000,[10,13,14],"Void instability",Color("5ce1c5")]]
const UPGRADES = [
 ["Laser power","Focused emitter · +35% mining damage",240],
 ["Mining cycle","Pulse oscillator · +20% mining rate",200],
 ["Cargo hold","Compression racks · +30 units",180],
 ["Engine speed","Ion drive · +45 maximum velocity",230],
 ["Acceleration","Vector nozzles · +90 acceleration",160],
 ["Hull plating","Titanium weave · +35 integrity",260],
 ["Shield array","Deflector bank · +30 shields",280],
 ["Energy reactor","Flux recovery · +5 energy per second",220],
 ["Resource scanner","Spectral analysis · +160 targeting range",170],
 ["Drone command","Parallel uplink · +1 drone capacity",450],
 ["Weapon damage","Rail capacitor · +10 projectile damage",290],
 ["Auto collection","Tractor field · +90 collection radius",190]]
const SHIPS = [["MANTIS / PROSPECTOR",0,"Balanced exploration chassis"],["KESTREL / INDUSTRIAL",2800,"+60 cargo · +30% mining"],["ORION / VANGUARD",7200,"+80 hull · +20% speed · +8 weapon damage"]]
const ENEMIES = ["Scout drone","Raider ship","Heavy drone","Plasma hunter","Void sentinel"]
const ACHIEVEMENTS = ["First extraction","Loaded for transit","Independent contractor","Drone operator","Beyond the belt","Combat certified","Deep frontier","Industrialist"]
