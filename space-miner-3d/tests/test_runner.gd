extends SceneTree
const State = preload("res://scripts/state.gd")
const C = preload("res://scripts/catalog.gd")
var passed: int=0
var failed: int=0
func check(ok: bool,description: String) -> void:
 if ok:passed+=1;print("PASS ",description)
 else:failed+=1;push_error("FAIL "+description)

func _init() -> void:
 var s=State.new()
 check(s.modules.size()==4,"four starter components")
 check(s.valid_layout(s.modules),"valid starter layout")
 check(s.crew_slots()==0,"free components do not unlock crew")
 for pair in [[9,0],[10,1],[19,1],[20,2],[29,2],[30,3]]:
  s.cells=pair[0];check(s.crew_slots()==pair[1],"crew floor formula %d cells" % pair[0])
 s.cells=0
 var cr: int=s.credits;var price: int=s.cell_price()
 check(s.buy_cell(),"purchase expansion")
 check(s.credits==cr-price and s.cells==1,"purchase charged exactly once")
 check(s.cell_price()>price,"progressive cell pricing")
 check(not s.place("Hull",0,0,0),"overlap rejected")
 check(not s.place("Hull",5,5,0),"disconnected rejected")
 check(not s.place("Hull",6,0,0),"out of bounds rejected")
 check(not s.place("Hull",-1,0,4),"invalid orientation rejected")
 check(s.place("Hull",-1,0,1),"connected rotated placement")
 check(not s.place("Hull",-2,0,0),"capacity enforced")
 check(not s.remove_at(0,0),"cockpit cannot be removed")
 check(not s.remove_at(0,1),"bridge removal rejected")
 check(s.remove_at(-1,0),"leaf removal accepted")
 var perf: Dictionary=s.performance()
 check(absf(perf.mass-70)<.001,"aggregate starter mass including chassis")
 check(perf.com.x>0 and perf.com.z>0,"asymmetric center of mass")
 check(s.performance(20).mass>perf.mass,"cargo adds mass")
 check(s.performance(20).com.length()<perf.com.length(),"center cargo shifts center of mass")
 check(s.performance().thrust==400,"engine thrust")
 s.modules[2].hp=0.0
 check(s.performance().thrust==0,"destroyed engine produces no thrust")
 s.modules[2].hp=100.0
 s.credits=0;check(not s.buy_cell(),"cannot buy without credits")
 s.credits=10000
 check(s.upgrade_at(1,0),"equipment upgrade succeeds")
 check(s.modules[3].level==2,"equipment level increased")
 s.collection=[0,1,24,30,39];s.cells=10
 check(s.equip(0),"equip owned crew")
 check(not s.equip(1),"crew capacity enforced")
 var b: float=s.bonus("Miner")
 check(absf(b-1.08)<.001,"crew bonus applied once")
 check(s.equip(0) and s.bonus("Miner")==1,"unequip removes bonus")
 check(not s.equip(9),"unowned crew rejected")
 check(s.upgrade_slots()==3,"three default upgrade slots")
 for role in ["Miner","Gunner","Pilot"]:check(s.toggle_active(role),"activate "+role)
 check(not s.toggle_active("Engineer"),"active capacity enforced")
 s.credits=10000
 check(s.buy_fourth(),"purchase fourth slot")
 check(s.upgrade_slots()==4 and s.credits==0,"fourth slot permanent charge")
 check(not s.buy_fourth(),"duplicate fourth slot rejected")
 s.fifth=true;check(s.upgrade_slots()==5,"maximum fifth slot")
 check(s.toggle_active("Engineer"),"fourth active slot usable")
 check(s.toggle_active("Collector"),"fifth active slot usable")
 check(not s.toggle_active("Commander"),"sixth slot forbidden")
 check(not s.toggle_active("Invalid"),"invalid active rejected")
 var unique: Dictionary={}
 for id in range(40):
  var card: Dictionary=C.crew(id);unique[card.name]=true
  check(FileAccess.file_exists("res://assets/crew_%02d.png" % id),"portrait exists %d" % id)
 check(unique.size()==40,"forty unique named cards")
 s=State.new();s.crystals=2
 check(s.open_pack().is_empty(),"pack requires three crystals")
 s.crystals=3
 check(s.open_pack().size()==3,"exactly three rewards per pack")
 check(s.crystals==0,"one pack debit")
 check(s.open_pack().is_empty(),"second transaction denied without currency")
 s.collection=range(40);s.crystals=3;cr=s.credits
 check(s.open_pack().size()==3 and s.credits==cr+150,"three duplicates convert to credits")
 var rng=RandomNumberGenerator.new();rng.seed=19333
 var counts: Array=[0,0,0,0,0,0,0]
 for i in range(100000):counts[C.crew(C.roll_card(rng)).rarity]+=1
 var rates: Array=[.4,.3,.18,.08,.03,.008,.002]
 for i in range(7):check(absf(float(counts[i])/100000-rates[i])<.008,"rarity frequency "+C.RARITIES[i])
 var unlocks: int=0
 s.rng.seed=83
 for i in range(2000):
  s.fifth=false;s.crystals=3;s.open_pack()
  if s.fifth:unlocks+=1
 check(unlocks>=5 and unlocks<=40,"configured 1% slot roll plausible seeded frequency")
 s=State.new()
 var specs: Array=s.task_specs();var group_counts: Dictionary={}
 for spec in specs:group_counts[spec.group]=int(group_counts.get(spec.group,0))+1
 check(specs.size()==30,"thirty achievements")
 check(group_counts=={"MINUTE":5,"HOURLY":3,"DAILY":7,"WEEKLY":15},"exact category counts")
 var now: int=int(Time.get_unix_time_from_system())
 s.refresh_tasks(now);s.event("mine",5)
 check(s.tasks[0].progress==5,"mining progress tracked")
 var crystals: int=s.crystals
 check(s.claim(0),"completed task claim")
 check(s.crystals==crystals+1,"minute reward exact")
 check(not s.claim(0),"duplicate claim blocked")
 check(not s.claim(-1),"negative claim blocked")
 check(not s.claim(30),"out of range claim blocked")
 for period in [60,3600,86400,604800]:
  s=State.new();s.tasks=[];s.refresh_tasks(period*100-1)
  var ix: int=0
  for i in range(30):
   if int(specs[i].period)==period:ix=i;break
  s.tasks[ix].progress=3;s.tasks[ix].claimed=true
  s.refresh_tasks(period*100-1)
  check(s.tasks[ix].progress==3,"preserve before %ds boundary" % period)
  s.refresh_tasks(period*100)
  check(s.tasks[ix].progress==0 and not s.tasks[ix].claimed,"reset on %ds boundary" % period)
  s.tasks[ix].progress=2;s.refresh_tasks(period*99)
  check(s.tasks[ix].progress==2,"clock rollback cannot reroll %ds" % period)
 s=State.new()
 s.credits=8317
 check(s.save_game("user://tests.json"),"save atomic write")
 var loaded=State.new()
 check(loaded.load_game("user://tests.json"),"versioned load")
 check(loaded.serialize()==s.serialize(),"save data round trip")
 var data: Dictionary=s.serialize().duplicate(true)
 data.modules[0].x=2
 check(not s.validate_save(data),"invalid saved cockpit rejected")
 data=s.serialize().duplicate(true);data.collection=[0,0]
 check(not s.validate_save(data),"duplicate collection rejected")
 data=s.serialize().duplicate(true);data.modules[1].r=8
 check(not s.validate_save(data),"invalid saved rotation rejected")
 data=s.serialize().duplicate(true);data.version=99
 check(not s.validate_save(data),"unknown save version rejected")
 data=s.serialize().duplicate(true);data.credits=-1
 check(not s.validate_save(data),"negative currency rejected")
 data=s.serialize().duplicate(true);data.equipped=[0]
 check(not s.validate_save(data),"saved crew over capacity rejected")
 data=s.serialize().duplicate(true);data.active=["Miner","Miner"]
 check(not s.validate_save(data),"duplicate active upgrade rejected")
 data=s.serialize().duplicate(true);data.tasks[0].progress="broken"
 check(not s.validate_save(data),"corrupt progress type rejected")
 s.save_game("user://tests.json")
 var f=FileAccess.open("user://tests.json",FileAccess.WRITE);f.store_string("{corrupt");f.close()
 check(loaded.load_game("user://tests.json"),"backup recovery from corrupt main save")
 print("TEST SUMMARY: %d passed / %d failed" % [passed,failed])
 quit(1 if failed else 0)
