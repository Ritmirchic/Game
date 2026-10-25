extends RefCounted
const PATH = "user://space_miner_v1.json"
var credits = 140
var xp = 0
var cargo: Array = []
var upgrades: Array = []
var sectors: Array = [0]
var ships: Array = [0]
var ship = 0
var drone_count = 0
var drone_level = 0
var achievements: Array = []
var settings = {"volume":0.45,"particles":true,"fullscreen":false}
var mined = 0
var kills = 0
var sold = 0
var warning = ""

func _init():
 cargo.resize(15)
 cargo.fill(0)
 upgrades.resize(12)
 upgrades.fill(0)

func total() -> int:
 var n = 0
 for v in cargo: n += int(v)
 return n

func save_data(path: String = PATH) -> bool:
 var data = {"version":1,"credits":credits,"xp":xp,"cargo":cargo,"upgrades":upgrades,"sectors":sectors,"ships":ships,"ship":ship,"drone_count":drone_count,"drone_level":drone_level,"achievements":achievements,"settings":settings,"mined":mined,"kills":kills,"sold":sold}
 var f = FileAccess.open(path + ".tmp",FileAccess.WRITE)
 if f == null: return false
 f.store_string(JSON.stringify(data,"  "))
 f.close()
 if FileAccess.file_exists(path):
  DirAccess.copy_absolute(path,path+".bak")
 return DirAccess.rename_absolute(path+".tmp",path)==OK

func read_data(path: String = PATH) -> bool:
 if not FileAccess.file_exists(path): return false
 var data = JSON.parse_string(FileAccess.get_file_as_string(path))
 if not valid(data):
  warning = "Save damaged. Recovered backup where possible."
  if FileAccess.file_exists(path+".bak"):
   data=JSON.parse_string(FileAccess.get_file_as_string(path+".bak"))
  if not valid(data): return false
 for key in ["credits","xp","cargo","upgrades","sectors","ships","ship","drone_count","drone_level","achievements","settings","mined","kills","sold"]:
  if data.has(key): set(key,data[key])
 return true

func valid(d) -> bool:
 if not d is Dictionary or d.get("version",0)!=1: return false
 for k in ["credits","xp","ship","drone_count","drone_level","mined","kills","sold"]:
  var v = d.get(k,-1)
  if not (v is float or v is int) or not is_finite(float(v)) or v<0 or v>100000000: return false
 if d.ship>2 or d.drone_count>6 or d.drone_level>5: return false
 for pair in [["cargo",15,100000],["upgrades",12,5]]:
  var a=d.get(pair[0],null)
  if not a is Array or a.size()!=pair[1]: return false
  for v in a:
   if not (v is float or v is int) or not is_finite(float(v)) or v<0 or v>pair[2] or v!=int(v): return false
 for pair in [["sectors",4],["ships",2],["achievements",7]]:
  var a=d.get(pair[0],null)
  if not a is Array or a.size()>pair[1]+1: return false
  for v in a:
   if not (v is float or v is int) or v<0 or v>pair[1] or v!=int(v): return false
 if not 0 in d.sectors or not 0 in d.ships or not int(d.ship) in d.ships: return false
 var s=d.get("settings",null)
 if not s is Dictionary: return false
 if not s.get("volume",null) is float and not s.get("volume",null) is int: return false
 if s.volume<0 or s.volume>1: return false
 if not s.get("particles",null) is bool or not s.get("fullscreen",null) is bool: return false
 return true
