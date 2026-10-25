extends RefCounted
const C = preload("res://scripts/catalog.gd")
const SAVE = "user://progress-v1.json"
var credits: int = 1200
var crystals: int = 9
var cells: int = 0
var modules: Array = []
var collection: Array = []
var equipped: Array = []
var active: Array = []
var fourth: bool = false
var fifth: bool = false
var sectors: Array = [0]
var stats: Dictionary = {"mine":0,"ore":0,"kill":0,"travel":0,"launch":0,"cells":0,"cards":0,"upgrade":0,"earn":0}
var tasks: Array = []
var reduced_motion: bool = false
var sensitivity: float = 1.0
var rng = RandomNumberGenerator.new()
var notice: String = ""

func _init() -> void:
 rng.randomize()
 modules = [module_at("Cockpit",0,0),module_at("Hull",0,1),module_at("Engine",0,2),module_at("Mining laser",1,0)]
 refresh_tasks(int(Time.get_unix_time_from_system()))

func module_at(kind: String, x: int, z: int, r: int = 0) -> Dictionary:
 return {"kind":kind,"x":x,"z":z,"r":r,"hp":100.0,"level":1}

func crew_slots() -> int: return cells / 10
func upgrade_slots() -> int: return 3 + int(fourth) + int(fifth)
func cell_price() -> int: return 50 + cells * 12
func buy_cell() -> bool:
 if credits < cell_price(): return false
 credits -= cell_price()
 cells += 1
 event("cells",1)
 return true

func grid_key(m: Dictionary) -> Vector2i: return Vector2i(int(m.x),int(m.z))
func valid_layout(layout: Array) -> bool:
 if layout.is_empty() or layout.size() > 4 + cells: return false
 var positions: Dictionary = {}
 var cockpit: Vector2i = Vector2i(99,99)
 for m in layout:
  if not m is Dictionary or not m.has_all(["kind","x","z","r","hp","level"]): return false
  if m.kind != "Cockpit" and not C.MODULES.has(m.kind): return false
  if not (m.x is int or m.x is float) or not (m.z is int or m.z is float): return false
  if float(m.x) != int(m.x) or float(m.z) != int(m.z): return false
  var p: Vector2i = grid_key(m)
  if abs(p.x)>5 or abs(p.y)>5 or positions.has(p) or int(m.r)<0 or int(m.r)>3: return false
  if not is_finite(float(m.hp)) or float(m.hp)<0 or float(m.hp)>100: return false
  if int(m.level)<1 or int(m.level)>5: return false
  if m.kind == "Cockpit":
   if cockpit != Vector2i(99,99): return false
   cockpit = p
  positions[p] = true
 if cockpit != Vector2i.ZERO: return false
 var seen: Dictionary = {cockpit:true}
 var queue: Array = [cockpit]
 while not queue.is_empty():
  var p: Vector2i = queue.pop_front()
  for step in [Vector2i.LEFT,Vector2i.RIGHT,Vector2i.UP,Vector2i.DOWN]:
   var q: Vector2i = p + step
   if positions.has(q) and not seen.has(q):
    seen[q] = true
    queue.append(q)
 return seen.size() == layout.size()

func place(kind: String, x: int, z: int, rot: int) -> bool:
 if not C.MODULES.has(kind) or credits < int(C.MODULES[kind].price): return false
 var candidate: Array = modules.duplicate(true)
 candidate.append(module_at(kind,x,z,rot))
 if not valid_layout(candidate): return false
 credits -= int(C.MODULES[kind].price)
 modules = candidate
 return true

func remove_at(x: int,z: int) -> bool:
 for i in range(modules.size()):
  var m: Dictionary = modules[i]
  if int(m.x)==x and int(m.z)==z:
   if m.kind=="Cockpit": return false
   var candidate: Array = modules.duplicate(true)
   candidate.remove_at(i)
   if not valid_layout(candidate): return false
   credits += int(C.MODULES[m.kind].price) / 2
   modules = candidate
   return true
 return false

func upgrade_at(x: int,z: int) -> bool:
 for m in modules:
  if int(m.x)==x and int(m.z)==z and m.kind!="Cockpit" and int(m.level)<5:
   var cost: int = int(C.MODULES[m.kind].price) * int(m.level)
   if credits < cost: return false
   credits -= cost
   m.level = int(m.level)+1
   event("upgrade",1)
   return true
 return false

func performance(cargo: float = 0) -> Dictionary:
 var mass: float = 20.0 + cargo * 0.15
 var moment: Vector3 = Vector3.ZERO
 var power: float = 0
 var thrust: float = 0
 var capacity: int = 40
 for m in modules:
  var d: Dictionary = C.MODULES.get(m.kind,{"mass":20.0,"power":0})
  mass += float(d.mass)
  moment += Vector3(float(m.x)*2.4,0,float(m.z)*2.4)*float(d.mass)
  if float(m.hp)<=0: continue
  power += float(d.power)
  if m.kind=="Engine": thrust += 400.0 * float(m.level)
  if m.kind=="Cargo": capacity += 50*int(m.level)
 return {"mass":mass,"com":moment/mass,"power":power,"thrust":thrust,"cargo":capacity}

func bonus(role: String) -> float:
 var value: float = 1.0
 for id in equipped:
  var c: Dictionary = C.crew(int(id))
  if c.role==role: value += float(c.bonus)
 if active.has(role): value += 0.2
 return value

func equip(id: int) -> bool:
 if equipped.has(id): equipped.erase(id); return true
 if not collection.has(id) or equipped.size()>=crew_slots(): return false
 equipped.append(id)
 return true

func toggle_active(role: String) -> bool:
 if not C.ROLES.has(role): return false
 if active.has(role): active.erase(role); return true
 if active.size()>=upgrade_slots(): return false
 active.append(role)
 return true

func buy_fourth() -> bool:
 if fourth or credits<10000: return false
 credits -= 10000
 fourth = true
 return true

func open_pack() -> Array:
 if crystals<3: return []
 crystals -= 3
 var rewards: Array = []
 for i in range(3):
  var id: int = C.roll_card(rng)
  rewards.append(id)
  if not collection.has(id): collection.append(id)
  else: credits += 50
  event("cards",1)
 # One eligible permanent-slot roll per paid pack, independent of card rarity.
 if not fifth and rng.randf()<0.01:
  fifth = true
  notice = "RARE REWARD: fifth upgrade slot permanently unlocked"
 return rewards

func task_specs() -> Array:
 var specs: Array = []
 var groups: Array = [["MINUTE",60,5,1],["HOURLY",3600,3,8],["DAILY",86400,7,20],["WEEKLY",604800,15,75]]
 for group in groups:
  for i in range(int(group[2])):
   var metric: String = ["mine","ore","travel"][i%3]
   var base: int = [5,20,500][i%3]
   var scale: int = [1,10,20,100][groups.find(group)]
   specs.append({"group":group[0],"period":group[1],"metric":metric,"goal":base*scale*(1+i/3),"reward":group[3],"title":["Rock breaker","Ore runner","Pathfinder"][i%3]})
 return specs

func refresh_tasks(now: int) -> void:
 var specs: Array = task_specs()
 if tasks.size()!=30: tasks = []; tasks.resize(30)
 for i in range(30):
  var bucket: int = now / int(specs[i].period)
  if not tasks[i] is Dictionary or int(tasks[i].get("bucket",-1))<bucket:
   tasks[i] = {"bucket":bucket,"progress":0,"claimed":false}

func event(metric: String, amount: int) -> void:
 refresh_tasks(int(Time.get_unix_time_from_system()))
 stats[metric] = int(stats.get(metric,0))+amount
 var specs: Array = task_specs()
 for i in range(30):
  if specs[i].metric == metric: tasks[i].progress = mini(int(tasks[i].progress)+amount,int(specs[i].goal))

func claim(index: int) -> bool:
 refresh_tasks(int(Time.get_unix_time_from_system()))
 if index<0 or index>=30: return false
 var spec: Dictionary = task_specs()[index]
 if tasks[index].claimed or int(tasks[index].progress)<int(spec.goal): return false
 tasks[index].claimed = true
 crystals += int(spec.reward)
 return true

func serialize() -> Dictionary:
 return {"version":1,"credits":credits,"crystals":crystals,"cells":cells,"modules":modules,"collection":collection,"equipped":equipped,"active":active,"fourth":fourth,"fifth":fifth,"sectors":sectors,"stats":stats,"tasks":tasks,"reduced_motion":reduced_motion,"sensitivity":sensitivity}

func save_game(path: String = SAVE) -> bool:
 var f = FileAccess.open(path+".tmp",FileAccess.WRITE)
 if f==null: return false
 f.store_string(JSON.stringify(serialize()))
 f.close()
 if FileAccess.file_exists(path):
  DirAccess.copy_absolute(path,path+".bak")
 return DirAccess.rename_absolute(path+".tmp",path)==OK

func load_game(path: String = SAVE, recover: bool = true) -> bool:
 if not FileAccess.file_exists(path): return false
 var parser = JSON.new()
 var parse_status: int = parser.parse(FileAccess.get_file_as_string(path))
 var data = parser.data if parse_status==OK else null
 if not validate_save(data):
  if recover: return load_game(path+".bak",false)
  return false
 credits = int(data.credits)
 crystals = int(data.crystals)
 cells = int(data.cells)
 modules = data.modules
 collection = data.collection.map(func(x): return int(x))
 equipped = data.equipped.map(func(x): return int(x))
 active = data.active
 fourth = bool(data.fourth)
 fifth = bool(data.fifth)
 sectors = data.sectors.map(func(x): return int(x))
 stats = data.stats
 tasks = data.tasks
 reduced_motion = bool(data.get("reduced_motion",false))
 sensitivity = clampf(float(data.get("sensitivity",1)),0.2,3)
 refresh_tasks(int(Time.get_unix_time_from_system()))
 return true

func validate_save(d) -> bool:
 if not d is Dictionary or int(d.get("version",0))!=1: return false
 for k in ["credits","crystals","cells"]:
  if not d.get(k) is float and not d.get(k) is int: return false
  if float(d[k])<0 or float(d[k])>100000000 or float(d[k])!=int(d[k]): return false
 for k in ["modules","collection","equipped","active","sectors","tasks"]:
  if not d.get(k) is Array: return false
 if not d.get("stats") is Dictionary or not d.get("fourth") is bool or not d.get("fifth") is bool: return false
 var old: int = cells
 cells = int(d.cells)
 var valid: bool = valid_layout(d.modules)
 cells = old
 if not valid or d.equipped.size()>int(d.cells)/10 or d.active.size()>3+int(d.fourth)+int(d.fifth): return false
 for key in ["collection","equipped","active","sectors"]:
  var seen: Dictionary = {}
  for v in d[key]:
   if seen.has(v): return false
   seen[v] = true
   if key=="active":
    if not C.ROLES.has(v): return false
   elif not (v is float or v is int) or float(v)!=int(v): return false
   elif key=="sectors":
    if int(v)<0 or int(v)>4: return false
   elif int(v)<0 or int(v)>=40: return false
 for id in d.equipped:
  if not d.collection.has(id): return false
 var normalized_sectors: Array = d.sectors.map(func(v): return int(v))
 if not normalized_sectors.has(0) or d.tasks.size()!=30: return false
 for task in d.tasks:
  if not task is Dictionary or not task.has_all(["bucket","progress","claimed"]): return false
  if not (task.bucket is float or task.bucket is int) or not (task.progress is float or task.progress is int) or not task.claimed is bool: return false
  if float(task.progress)<0 or float(task.bucket)<0: return false
 return true
