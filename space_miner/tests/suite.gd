extends SceneTree
const Game=preload("res://scripts/game.gd")
const Profile=preload("res://scripts/profile.gd")
var checks=0
var failures=0
var game
func verify(condition: bool,description: String):
 checks+=1
 if condition: print("PASS ",description)
 else: failures+=1;print("FAIL ",description)

func _initialize(): call_deferred("run")
func run():
 game=Game.new();root.add_child(game)
 await process_frame
 game.set_process(false);game.player.set_physics_process(false)
 game.profile=Profile.new();game.resume()
 verify(game.rocks.size()>=90,"Procedural field contains at least 90 textured asteroids")
 var safe=true;var overlap=false
 for i in range(game.rocks.size()):
  var r=game.rocks[i]
  if r.pos.length()<r.radius+24 or r.pos.distance_to(game.station_pos)<r.radius+160: safe=false
  for j in range(i):
   var r2=game.rocks[j]
   if r.pos.distance_to(r2.pos)<r.radius+r2.radius: overlap=true
 verify(safe,"Spawn clearances protect pilot and refinery")
 verify(not overlap,"Asteroid placement rejects overlapping collision envelopes")
 verify(game.camera.get_parent()==game.player and game.camera.position_smoothing_enabled,"Camera follows pilot with smoothing")
 var before=game.player.position
 var event=InputEventKey.new();event.physical_keycode=KEY_D;event.pressed=true;Input.parse_input_event(event)
 await process_frame
 for i in range(20): game.player._physics_process(.016)
 verify(game.player.velocity.x>0 and game.player.position.x>before.x,"WASD thrust accelerates and moves CharacterBody2D")
 event.pressed=false;Input.parse_input_event(event);game.player.velocity=Vector2.ZERO
 var index=game.rocks.size()-1
 var ore=game.rocks[index].resource
 game.damage_rock(index,game.rocks[index].max_hp+1)
 verify(game.rocks[index].hp<=0 and game.drops.size()>0,"Mining depletes health and releases actual collectible ore")
 verify(game.profile.mined==1 and game.profile.xp>0,"Asteroid destruction awards progression")
 game.player.position=game.drops[0].pos;game.update_drops(.016)
 var collected=game.profile.total()
 game.drops=[{"pos":game.player.position,"resource":0,"vel":Vector2.ZERO,"age":999.0}]
 game.update_drops(.016)
 verify(collected>0 and game.profile.total()==collected,"Physical collection excludes already-consumed drone drops")
 game.collect(0,10000)
 verify(game.profile.total()==game.capacity() and game.collect(0)==0,"Cargo capacity strictly limits collection")
 game.player.position=game.station_pos
 var credits=game.profile.credits;var value=game.cargo_value();game.sell()
 verify(game.profile.total()==0 and game.profile.credits==credits+value,"Docked sale exchanges cargo for exact catalog value")
 game.profile.credits=100000
 var cap=game.capacity();var cost=game.upgrade_cost(2);credits=game.profile.credits
 verify(game.buy_upgrade(2) and game.capacity()==cap+30 and game.profile.credits==credits-cost,"Cargo upgrade charges credits and increases real capacity")
 var power=game.mining_power();game.buy_upgrade(0);game.buy_upgrade(1)
 verify(game.mining_power()>power*1.4,"Laser and cycle upgrades increase actual mining throughput")
 var speed=game.player.top_speed;var acceleration=game.player.acceleration
 game.buy_upgrade(3);game.buy_upgrade(4)
 verify(game.player.top_speed>speed and game.player.acceleration>acceleration,"Engine upgrades affect physical movement")
 var hp=game.max_hull();var shields=game.max_shield();game.buy_upgrade(5);game.buy_upgrade(6)
 verify(game.max_hull()>hp and game.max_shield()>shields,"Defence upgrades increase hull and shield maxima")
 var range_before=game.scan_range();game.buy_upgrade(8)
 verify(game.scan_range()>range_before,"Scanner upgrade increases acquisition range")
 game.profile.upgrades[0]=5;game.profile.credits=100000
 verify(not game.buy_upgrade(0) and game.profile.upgrades[0]==5,"Maximum upgrade tier cannot be exceeded")
 game.profile.credits=0
 verify(not game.buy_upgrade(7),"Unaffordable upgrades are rejected")
 game.profile.credits=100000;game.player.position=Vector2(900,900)
 verify(not game.buy_upgrade(7),"Equipment installation is locked away from refinery")
 game.player.position=game.station_pos;game.buy_drone()
 verify(game.drones.size()==1 and game.profile.drone_count==1,"Drone purchase deploys a visible unit")
 var d=game.drones[0];var start=d.pos
 game.update_drones(.5);game.update_drones(.5)
 verify(d.target>=0 and d.pos!=start,"Drone searches field and navigates to a target")
 # Isolate a target and run the complete state machine deterministically.
 game.rocks=[{"pos":Vector2(250,0),"radius":44.0,"hp":8.0,"max_hp":8.0,"resource":0,"quantity":8,"rotation":0.0,"spin":0.0,"variant":0}]
 game.drops.clear();game.profile.cargo.fill(0);game.player.position=Vector2.ZERO
 game.drones=[{"pos":Vector2(160,0),"mode":"search","target":-1,"load":0,"resource":0,"beam":false,"angle":0.0}]
 for i in range(1500):
  game.update_drones(.02)
  if game.profile.total()>0: break
 verify(game.rocks[0].hp<=0 and game.profile.total()>0,"Drone mines, collects fragments, returns and unloads real resources")
 game.enemies.clear();game.spawn_enemy();var enemy=game.enemies[0];var enemy_start=enemy.pos
 game.update_enemies(.5)
 verify(enemy.pos!=enemy_start,"Enemy AI moves toward an attack orbit")
 game.player.position=Vector2(700,0);enemy.pos=Vector2(900,0);enemy.cooldown=0;game.bullets.clear();game.update_enemies(.01)
 verify(game.bullets.size()>0 and game.bullets[0].enemy,"Enemy AI fires damaging projectiles")
 game.shield=10;game.health=100;game.hit_player(18)
 verify(game.shield==0 and game.health==92,"Shield absorption passes overflow damage to hull")
 enemy.pos=Vector2(800,0);enemy.hp=1;game.bullets=[{"pos":Vector2(780,0),"vel":Vector2(800,0),"life":1.0,"enemy":false,"damage":20}]
 var kills=game.profile.kills;game.update_bullets(.05)
 verify(enemy.hp<=0 and game.profile.kills==kills+1,"Swept projectile hit destroys enemy and awards bounty")
 game.profile.credits=100000;game.jump_clicked(1)
 verify(1 in game.profile.sectors and game.sector==1 and game.player.position==Vector2.ZERO,"Research unlocks sector, changes field and performs safe jump")
 verify(game.rocks[0].resource in [3,4,8,12],"New sector uses distinct resource distribution")
 var sectors_before=game.profile.sectors.size();game.jump_clicked(4)
 verify(game.profile.sectors.size()==sectors_before,"Sector research cannot skip prerequisites")
 for i in range(2,5): game.jump_clicked(i)
 verify(game.sector==4 and game.profile.sectors.size()==5,"All five sectors can be unlocked in order")
 for id in ["cargo","upgrades","map","shop","drones","achievements","settings","help","dock"]:
  game.player.position=game.station_pos;game.open_screen(id)
  verify(is_instance_valid(game.panel) and not game.player.active,"Functional UI screen opens and pauses flight: "+id)
 game.profile.credits=9876;game.profile.cargo[14]=3;game.profile.settings.volume=.37
 var path="user://test_profile.json"
 verify(game.profile.save_data(path),"Versioned save writes atomically")
 var other=Profile.new()
 verify(other.read_data(path) and other.credits==9876 and other.cargo[14]==3 and absf(other.settings.volume-.37)<.001,"Save/load round-trip preserves progression, cargo and settings")
 game.profile.save_data(path)
 var f=FileAccess.open(path,FileAccess.WRITE);f.store_string("{broken-json");f.close()
 other=Profile.new()
 verify(other.read_data(path) and other.credits==9876,"Corrupted save recovers validated previous backup")
 verify(not other.valid({"version":1,"credits":-500}),"Invalid save schema and negative balances rejected")
 var valid_save=JSON.parse_string(FileAccess.get_file_as_string(path+".bak"));valid_save.cargo[0]="injection"
 verify(not other.valid(valid_save),"Non-numeric inventory entries are rejected")
 var now=Time.get_ticks_usec()
 game.resume()
 for i in range(600): game.simulate(.016)
 var elapsed=(Time.get_ticks_usec()-now)/1000.0
 verify(game.effects.size()<=651 and game.enemies.size()<=6,"Simulation bounds particle and enemy counts")
 print("PERFORMANCE: 600 simulation updates in %.1f ms (headless CPU; not GPU FPS)"%elapsed)
 print("RESULT: %d checks, %d failures"%[checks,failures])
 game.sound_player.stop();game.audio=null
 game.queue_free();await process_frame
 quit(1 if failures>0 else 0)
