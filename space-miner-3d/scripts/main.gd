extends Node3D
const State = preload("res://scripts/state.gd")
const Catalog = preload("res://scripts/catalog.gd")
const Art = preload("res://scripts/art.gd")
var state = State.new()
var art = Art.new()
var world: Node3D
var ship: RigidBody3D
var camera: Camera3D
var module_nodes: Array = []
var rocks: Array = []
var enemies: Array = []
var chunks: Array = []
var shots: Array = []
var transient: Array = []
var sector: int = 0
var mode: String = "hangar"
var page: String = "BUILD"
var selected: String = "Hull"
var rotation_step: int = 0
var cursor_cell = Vector2i.ZERO
var aim: Vector3 = Vector3.ZERO
var camera_yaw: float = 0.5
var zoom: float = 25
var pan: Vector3 = Vector3.ZERO
var energy: float = 100
var shield: float = 0
var cargo: int = 0
var hull: float = 100
var fire_timer: float = 0
var enemy_timer: float = 0
var hazard_timer: float = 0
var traveled: float = 0
var tick: float = 0
var saved_tick: float = 0
var flash_time: float = 0
var toast_time: float = 0
var toast_text: String = ""
var paused: bool = false
var ghost: Node3D
var com_marker: Node3D
var hud: CanvasLayer
var ui: Control
var top_label: Label
var status_label: Label
var toast_label: Label
var info_label: Label
var hud_label: Label
var panel: PanelContainer
var panel_content: VBoxContainer
var reveal_layer: Control
var reveal_cards: Array = []
var reveal_rewards: Array = []
var reveal_time: float = 0
var reveal_stage: int = 0
var audio: AudioStreamPlayer
var rng = RandomNumberGenerator.new()
var smoke: bool = false

func _ready() -> void:
 rng.seed=2410
 state.load_game()
 smoke = "--smoke" in OS.get_cmdline_user_args()
 setup_world()
 setup_ui()
 enter_hangar()
 if smoke:
  call_deferred("runtime_smoke")
 elif "--capture" in OS.get_cmdline_user_args():
  call_deferred("capture_screens")

func setup_world() -> void:
 world=Node3D.new();add_child(world)
 var env = WorldEnvironment.new()
 var e = Environment.new()
 e.background_mode=Environment.BG_COLOR
 e.background_color=Color("030913")
 e.ambient_light_source=Environment.AMBIENT_SOURCE_COLOR
 e.ambient_light_color=Color("8bb9d2")
 e.ambient_light_energy=.65
 e.tonemap_mode=Environment.TONE_MAPPER_FILMIC
 env.environment=e
 add_child(env)
 var sun = DirectionalLight3D.new();sun.rotation_degrees=Vector3(-45,-30,0);sun.light_color=Color("a2d4ed");sun.light_energy=1.8;sun.shadow_enabled=true;add_child(sun)
 var fill = DirectionalLight3D.new();fill.rotation_degrees=Vector3(-20,140,20);fill.light_color=Color("f6ba82");fill.light_energy=.7;add_child(fill)
 camera=Camera3D.new();camera.far=1200;camera.fov=55;add_child(camera)
 audio=AudioStreamPlayer.new();audio.volume_db=-12;add_child(audio)

func clear_world() -> void:
 for n in world.get_children():
  world.remove_child(n);n.queue_free()
 rocks.clear();enemies.clear();chunks.clear();shots.clear();transient.clear();module_nodes.clear()
 ghost=null;com_marker=null

func enter_hangar() -> void:
 if is_instance_valid(ship) and mode=="space":
  state.credits += cargo * int(Catalog.SECTORS[sector].value)
  state.event("earn",cargo*int(Catalog.SECTORS[sector].value))
 cargo=0
 mode="hangar";paused=false;page="BUILD";pan=Vector3.ZERO;zoom=25
 clear_world()
 for m in state.modules: m.hp=100.0
 hull=100;energy=100
 build_hangar()
 build_ship(Vector3.ZERO,true)
 state.save_game()
 refresh_panel()
 message("HELIOS DOCK • Ship repaired. Cargo sold. Progress saved.")

func build_hangar() -> void:
 art.box(world,Vector3(70,1,65),Vector3(0,-2,0),art.dark)
 art.cylinder(world,13,.7,Vector3(0,-1.1,0),art.alloy,13)
 art.cylinder(world,13.2,.08,Vector3(0,-.72,0),art.trim,13.2)
 art.cylinder(world,12.9,.08,Vector3(0,-.66,0),art.dark,12.9)
 for i in range(-5,6):
  art.box(world,Vector3(.018,.015,25),Vector3(i*2.4,-.6,0),art.material(Color("34505c")))
  art.box(world,Vector3(25,.015,.018),Vector3(0,-.6,i*2.4),art.material(Color("34505c")))
 for x in [-25,25]:
  for z in [-23,-8,8,23]:
   art.box(world,Vector3(2,13,2),Vector3(x,4,z),art.alloy)
   art.box(world,Vector3(.12,9,2.1),Vector3(x-.9,5,z),art.glow)
  art.box(world,Vector3(2,2,58),Vector3(x,11,0),art.dark)
 for z in [-26,26]:
  art.box(world,Vector3(54,12,1.5),Vector3(0,4,z),art.dark)
  art.box(world,Vector3(48,.2,.2),Vector3(0,9,z-1),art.glow)
 for i in range(7):
  var x: float = -21+i*7
  var terminal = Node3D.new();world.add_child(terminal);terminal.position=Vector3(x,0,-20)
  art.cylinder(terminal,1.1,1.2,Vector3.ZERO,art.alloy)
  art.box(terminal,Vector3(2.4,2,.18),Vector3(0,1.7,0),art.glass)
  var label = Label3D.new();label.text=["SHIPYARD","EQUIPMENT","PERSONNEL","RELIC EXCHANGE","RESEARCH","CONTRACTS","FLIGHT OPS"][i];label.font_size=32;label.pixel_size=.006;label.position=Vector3(0,3.1,0);terminal.add_child(label)
 for side in [-1,1]:
  var arm = Node3D.new();world.add_child(arm);arm.position=Vector3(side*16,0,2);arm.name="ServiceArm"
  art.cylinder(arm,1.6,1.3,Vector3.ZERO,art.dark)
  art.box(arm,Vector3(.9,5,.9),Vector3(0,2.5,0),art.trim)
  var link = art.box(arm,Vector3(6,.7,.8),Vector3(-side*2.2,4.9,0),art.alloy);link.rotation.z=side*.25
  art.cylinder(arm,.5,1.4,Vector3(-side*5,4,0),art.dark)
 for i in range(14):
  var p = Vector3(rng.randf_range(-23,23),0,rng.randf_range(16,23))
  art.box(world,Vector3(2,2,2),p,art.alloy)
  art.box(world,Vector3(.18,2.1,2.1),p,art.trim)
 var light = OmniLight3D.new();light.position=Vector3(0,8,0);light.omni_range=35;light.light_color=Color("4de6d5");light.light_energy=3;world.add_child(light)
 com_marker=art.cylinder(world,.2,.8,Vector3.ZERO,art.glow)

func build_ship(pos: Vector3, frozen: bool) -> void:
 ship=RigidBody3D.new();ship.name="PlayerShip";ship.gravity_scale=0;ship.freeze=frozen;ship.contact_monitor=true;ship.max_contacts_reported=8;ship.linear_damp=.15;ship.angular_damp=2.5
 ship.axis_lock_angular_x=true;ship.axis_lock_angular_z=true
 ship.collision_layer=1;ship.collision_mask=6
 world.add_child(ship);ship.position=pos
 var perf: Dictionary = state.performance(cargo)
 ship.mass=perf.mass
 ship.center_of_mass_mode=RigidBody3D.CENTER_OF_MASS_MODE_CUSTOM
 ship.center_of_mass=perf.com
 for m in state.modules:
  var n: Node3D = art.module(m.kind);ship.add_child(n);n.position=Vector3(float(m.x)*2.4,0,float(m.z)*2.4);n.rotation.y=int(m.r)*PI/2
  module_nodes.append(n)
  var collision = CollisionShape3D.new();var shape = BoxShape3D.new();shape.size=Vector3(2.15,1.7,2.15);collision.shape=shape;collision.position=n.position+Vector3(0,.5,0);ship.add_child(collision)
 # Visible structural spars bridge the hexagonal armor shells.
 for a in state.modules:
  for b in state.modules:
   var delta=Vector2i(int(b.x)-int(a.x),int(b.z)-int(a.z))
   if delta==Vector2i.RIGHT or delta==Vector2i.DOWN:
    var midpoint=Vector3(float(a.x+b.x)*1.2,-.02,float(a.z+b.z)*1.2)
    art.box(ship,Vector3(2.4,.24,.55) if delta==Vector2i.RIGHT else Vector3(.55,.24,2.4),midpoint,art.dark)
 ship.body_entered.connect(func(body):
  if mode=="space" and ship.linear_velocity.length()>5:
   damage_ship(ship.linear_velocity.length()*.4,body.global_position)
 )

func launch(index: int) -> void:
 if not state.sectors.has(index):
  var cost: int = int(Catalog.SECTORS[index].price)
  if state.credits<cost: message("Insufficient credits to chart this sector.");return
  state.credits-=cost;state.sectors.append(index)
 sector=index;mode="space";paused=false;cargo=0;energy=100;shield=shield_capacity();hull=100;zoom=32;pan=Vector3.ZERO
 clear_world()
 build_ship(Vector3(0,0,12),false)
 build_space()
 state.event("launch",1);state.save_game()
 refresh_panel()
 message("EXPEDITION ACTIVE • Mine, collect ore, survive. E returns to dock within 22m of beacon.")

func shield_capacity() -> float:
 var cap: float = 0
 for m in state.modules:
  if m.kind=="Shield" and float(m.hp)>0:cap+=80*float(m.level)
 return cap*state.bonus("Commander")

func build_space() -> void:
 var col: Color = Catalog.SECTORS[sector].color
 # A true 3D starfield with distant planet, dust structures and navigable asteroid belt.
 var stars = MultiMeshInstance3D.new();var mm = MultiMesh.new();mm.transform_format=MultiMesh.TRANSFORM_3D
 var star_mesh = SphereMesh.new();star_mesh.radius=.35;star_mesh.height=.7;star_mesh.radial_segments=4;star_mesh.rings=2;mm.mesh=star_mesh;mm.instance_count=600
 stars.multimesh=mm;stars.material_override=art.material(Color("c7d6ef"),0,Color("7189a4"));world.add_child(stars)
 for i in range(600):
  var p = Vector3(rng.randf_range(-1,1),rng.randf_range(-1,1),rng.randf_range(-1,1)).normalized()*rng.randf_range(450,650)
  mm.set_instance_transform(i,Transform3D(Basis.IDENTITY,p))
 var planet = SphereMesh.new();planet.radius=75;planet.height=150
 art.mesh(world,planet,Vector3(-260,-100,-280),art.material(col*.24,.25))
 var torus = TorusMesh.new();torus.inner_radius=95;torus.outer_radius=135;torus.rings=48;torus.ring_segments=12
 var ring = art.mesh(world,torus,Vector3(-260,-100,-280),art.material(col*.5,.6));ring.rotation_degrees=Vector3(20,10,-22)
 for i in range(60):
  var p: Vector3
  if i<8:p=Vector3((i%4)*9-13,0,-8-(i/4)*14)
  else:p=Vector3(rng.randf_range(-180,180),rng.randf_range(-3,3),rng.randf_range(-200,180))
  var radius: float = rng.randf_range(1.8,4.2)
  var body = StaticBody3D.new();body.collision_layer=2;body.collision_mask=1;world.add_child(body);body.position=p
  var model = art.mesh(body,art.asteroid(i+sector*100,radius),Vector3.ZERO,art.rock)
  model.rotation=Vector3(rng.randf(),rng.randf(),rng.randf())
  var cs = CollisionShape3D.new();var sh=SphereShape3D.new();sh.radius=radius*.95;cs.shape=sh;body.add_child(cs)
  var hp: float = 28.0*float(Catalog.SECTORS[sector].hardness)
  body.set_meta("type","rock");body.set_meta("hp",hp);body.set_meta("max_hp",hp);body.set_meta("radius",radius)
  rocks.append(body)
  if i%4==0:
   for j in range(3):
    var crystal = PrismMesh.new();crystal.size=Vector3(.5,1.3,.5)
    art.mesh(body,crystal,Vector3(radius*.6,j*.3,0),art.material(col,.4,col*.3)).rotation.z=-.7+j*.5
 if sector==2:
  for i in range(9):
   var wreck = art.module("Cargo");world.add_child(wreck);wreck.position=Vector3(-70+i*9,0,-60);wreck.rotation=Vector3(rng.randf(),rng.randf(),rng.randf())
 var beacon = Node3D.new();beacon.name="DockBeacon";world.add_child(beacon)
 var t=TorusMesh.new();t.inner_radius=7;t.outer_radius=7.5
 art.mesh(beacon,t,Vector3(0,-2,0),art.glow)
 for i in range(4):
  var p = Vector3(cos(i*PI/2)*8,0,sin(i*PI/2)*8)
  art.cylinder(beacon,.4,4,p,art.alloy);art.cylinder(beacon,.5,.3,p+Vector3(0,2,0),art.glow)
 for i in range(2+sector):spawn_enemy(Vector3(rng.randf_range(-70,70),0,-65-i*20))

func spawn_enemy(pos: Vector3) -> void:
 var b = RigidBody3D.new();b.gravity_scale=0;b.mass=28+sector*10;b.linear_damp=.8;b.angular_damp=2;b.axis_lock_linear_y=true;b.axis_lock_angular_x=true;b.axis_lock_angular_z=true;b.collision_layer=4;b.collision_mask=3
 world.add_child(b);b.position=pos
 var n: Node3D = art.module("Pulse cannon" if sector<3 else "Rocket launcher");b.add_child(n)
 for side in [-1,1]:
  var wing = PrismMesh.new();wing.size=Vector3(2.2,.5,2.3)
  art.mesh(b,wing,Vector3(side*1.7,0,.5),art.trim).rotation.y=side*.3
 var cs = CollisionShape3D.new();var sh=SphereShape3D.new();sh.radius=2.2;cs.shape=sh;b.add_child(cs)
 b.set_meta("type","enemy");b.set_meta("hp",45.0+sector*30);b.set_meta("cooldown",rng.randf_range(.5,2.5));enemies.append(b)

func play_sound(which: String) -> void:
 if smoke:return
 audio.stream=load("res://assets/"+which+".wav");audio.play()

func get_aim() -> Vector3:
 var p: Vector2 = get_viewport().get_mouse_position()
 var origin: Vector3 = camera.project_ray_origin(p)
 var dir: Vector3 = camera.project_ray_normal(p)
 var hit = Plane(Vector3.UP,0).intersects_ray(origin,dir)
 return hit if hit!=null else ship.position-Vector3(0,0,20)

func ui_hover() -> bool:
 return get_viewport().gui_get_hovered_control()!=null

func _unhandled_input(event: InputEvent) -> void:
 if event is InputEventMouseButton:
  if event.button_index==MOUSE_BUTTON_WHEEL_UP:zoom=maxf(14,zoom-2)
  if event.button_index==MOUSE_BUTTON_WHEEL_DOWN:zoom=minf(70,zoom+2)
  if mode=="hangar" and page=="BUILD" and event.pressed:
   if event.button_index==MOUSE_BUTTON_LEFT:
    if state.place(selected,cursor_cell.x,cursor_cell.y,rotation_step): rebuild_hangar_ship();play_sound("click")
    else:message("Placement rejected: buy a cell, keep modules connected, and check credits / overlap.")
   elif event.button_index==MOUSE_BUTTON_RIGHT:
    if state.remove_at(cursor_cell.x,cursor_cell.y):rebuild_hangar_ship()
    else:message("Cannot remove cockpit or disconnect another module.")
 if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE):
  if Input.is_key_pressed(KEY_SHIFT):pan+=Vector3(-event.relative.x,0,-event.relative.y)*.035
  else:camera_yaw-=event.relative.x*.006*state.sensitivity
 if event is InputEventKey and event.pressed and not event.echo:
  match event.keycode:
   KEY_R:
    if mode=="hangar":rotation_step=(rotation_step+1)%4;update_ghost()
   KEY_U:
    if mode=="hangar" and state.upgrade_at(cursor_cell.x,cursor_cell.y):rebuild_hangar_ship()
   KEY_ESCAPE:
    if is_instance_valid(reveal_layer):finish_reveal()
    elif mode=="space":paused=not paused;refresh_panel()
    else:page="SETTINGS";refresh_panel()
   KEY_E:
    if mode=="space":
     if ship.position.length()<22:enter_hangar()
     else:message("Return to the cyan dock beacon (within 22m).")
   KEY_TAB:
    if mode=="space":message("CARGO %d / %d %s • potential sale %d credits" % [cargo,state.performance(cargo).cargo,Catalog.SECTORS[sector].ore,cargo*int(Catalog.SECTORS[sector].value)])
   KEY_F5:state.save_game();message("Progress saved. Expedition cargo is banked only on return.")

func rebuild_hangar_ship() -> void:
 world.remove_child(ship);ship.queue_free();module_nodes.clear();build_ship(Vector3.ZERO,true)
 state.save_game();refresh_panel()

func update_ghost() -> void:
 if is_instance_valid(ghost):world.remove_child(ghost);ghost.queue_free()
 ghost=art.module(selected);world.add_child(ghost);ghost.rotation.y=rotation_step*PI/2
 var mat = art.material(Color(.25,.95,.8,.4));mat.transparency=BaseMaterial3D.TRANSPARENCY_ALPHA
 for n in ghost.find_children("*","MeshInstance3D",true,false):n.material_override=mat

func _process(dt: float) -> void:
 if not is_instance_valid(ship):return
 tick+=dt;saved_tick+=dt
 if mode=="space":
  ship.freeze=paused
  for enemy in enemies:
   if is_instance_valid(enemy):enemy.freeze=paused
  for chunk in chunks:
   if is_instance_valid(chunk):chunk.freeze=paused
 aim=get_aim()
 if mode=="hangar":
  cursor_cell=Vector2i(roundi(aim.x/2.4),roundi(aim.z/2.4))
  if page=="BUILD":
   if not is_instance_valid(ghost):update_ghost()
   ghost.position=Vector3(cursor_cell.x*2.4,.05,cursor_cell.y*2.4)
   ghost.visible=abs(cursor_cell.x)<=5 and abs(cursor_cell.y)<=5
  elif is_instance_valid(ghost):ghost.visible=false
  if is_instance_valid(com_marker):com_marker.position=state.performance().com+Vector3(0,2,0)
 var target: Vector3 = (Vector3.ZERO if mode=="hangar" else ship.position)+pan
 var offset = Vector3(sin(camera_yaw)*zoom*.75,zoom,cos(camera_yaw)*zoom*.75)
 camera.position=camera.position.lerp(target+offset,1-exp(-dt*7))
 if camera.position.distance_to(target)>.1:camera.look_at(target)
 if toast_time>0:toast_time-=dt
 if is_instance_valid(toast_label):toast_label.text=toast_text if toast_time>0 else ""
 update_labels()
 if saved_tick>20:state.save_game();saved_tick=0
 if mode=="hangar" and page=="CONTRACTS" and int(tick*2)%2==0:update_contract_labels()
 if is_instance_valid(reveal_layer):animate_reveal(dt)

func _physics_process(dt: float) -> void:
 if mode!="space" or paused or not is_instance_valid(ship):return
 fire_timer=maxf(0,fire_timer-dt);hazard_timer+=dt
 var perf: Dictionary = state.performance(cargo)
 ship.mass=perf.mass;ship.center_of_mass=perf.com
 var regen: float = 10
 for m in state.modules:
  if m.kind=="Generator" and float(m.hp)>0:regen+=25*float(m.level)
 energy=minf(100,energy+regen*state.bonus("Engineer")*dt)
 var throttle: float = float(Input.is_key_pressed(KEY_W))-float(Input.is_key_pressed(KEY_S))*.45
 var turn: float = float(Input.is_key_pressed(KEY_A))-float(Input.is_key_pressed(KEY_D))
 var boost: float = 1.8 if Input.is_key_pressed(KEY_SHIFT) and energy>10 else 1
 for i in range(state.modules.size()):
  var m: Dictionary = state.modules[i];var n: Node3D = module_nodes[i]
  var alive: bool = float(m.hp)>0
  if m.kind=="Engine":
   var flame = n.get_node_or_null("Exhaust")
   if flame:flame.visible=abs(throttle)>.01 and alive and energy>0
   if alive and energy>0 and throttle!=0:
    var force: Vector3 = -n.global_basis.z*400.0*float(m.level)*throttle*boost*state.bonus("Pilot")*float(m.hp)/100
    ship.apply_force(force,n.global_position-ship.global_position)
    energy=maxf(0,energy-8*dt*boost)
  if m.kind=="Repair" and alive and energy>10:
   for part in state.modules:part.hp=minf(100,float(part.hp)+dt*1.5*float(m.level))
   energy-=10*dt
  var turret = n.get_node_or_null("Turret")
  if turret:
   var local_target: Vector3 = n.to_local(aim)
   turret.rotation.y=atan2(-local_target.x,-local_target.z)
 ship.apply_torque(Vector3(0,turn*perf.mass*18,0))
 # Flight assist keeps player within the shallow belt and prevents runaway tumbling.
 ship.apply_central_force(Vector3(0,-ship.position.y*perf.mass*4-ship.linear_velocity.y*perf.mass*3,0))
 if ship.linear_velocity.length()>45*state.bonus("Pilot"):ship.linear_velocity=ship.linear_velocity.normalized()*45*state.bonus("Pilot")
 if Input.is_key_pressed(KEY_Q):ship.apply_central_force(-ship.linear_velocity*perf.mass*2)
 if Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT) and not ui_hover():mine(dt)
 if (Input.is_key_pressed(KEY_SPACE) or Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)) and fire_timer<=0 and not ui_hover():fire_weapons()
 var cap: float = shield_capacity()
 if shield<cap and energy>25:shield=minf(cap,shield+6*dt);energy-=2*dt
 traveled+=ship.linear_velocity.length()*dt
 if traveled>=1:state.event("travel",int(traveled));traveled-=int(traveled)
 update_enemies(dt)
 update_shots(dt)
 update_chunks(dt)
 for i in range(transient.size()-1,-1,-1):
  var item: Dictionary = transient[i];item.time-=dt
  if item.time<=0:
   if is_instance_valid(item.node):item.node.queue_free()
   transient.remove_at(i)
 if sector==3 and hazard_timer>7:
  hazard_timer=0;damage_ship(10,ship.position+Vector3(8,0,0));effect(ship.position,Color("6ea4ff"),10);message("PLASMA DISCHARGE • Shield or hull damaged")
 if hull<=0:
  cargo=0;state.credits=maxi(0,state.credits-100);enter_hangar();message("SHIP RECOVERED • Cargo lost. 100 credit salvage fee.")

func ray_hit(start: Vector3,end: Vector3,mask: int,exclude: Array[RID] = []) -> Dictionary:
 var q = PhysicsRayQueryParameters3D.create(start,end,mask,exclude)
 return get_world_3d().direct_space_state.intersect_ray(q)

func mount_origin(n: Node3D) -> Vector3:
 var turret=n.get_node_or_null("Turret")
 return turret.to_global(Vector3(0,0,-1.6)) if turret else n.global_position+Vector3(0,.8,0)

func mine(dt: float) -> void:
 for i in range(state.modules.size()):
  var m: Dictionary=state.modules[i]
  if m.kind!="Mining laser" or float(m.hp)<=0 or energy<12*dt:continue
  var start: Vector3 = mount_origin(module_nodes[i]);var dir: Vector3 = (aim-start).normalized()
  var hit: Dictionary = ray_hit(start,start+dir*70,6)
  var end: Vector3 = hit.get("position",start+dir*70)
  var beam: MeshInstance3D = art.beam(world,start,end,Color("63f0db"),.045);transient.append({"node":beam,"time":.04})
  energy-=12*dt
  if hit.has("collider"):
   damage_target(hit.collider,18*dt*float(m.level)*state.bonus("Miner"),hit.position)
   if rng.randf()<.15:effect(hit.position,Color("aaf5e1"),2)

func fire_weapons() -> void:
 var fired: bool = false
 for i in range(state.modules.size()):
  var m: Dictionary = state.modules[i]
  if not m.kind in ["Pulse cannon","Rocket launcher"] or float(m.hp)<=0 or energy<5:continue
  var n: Node3D = module_nodes[i];var start: Vector3 = mount_origin(n)
  var rocket: bool = m.kind=="Rocket launcher"
  spawn_shot(start,(aim-start).normalized()*(45 if rocket else 85)+ship.linear_velocity,(45 if rocket else 16)*float(m.level)*state.bonus("Gunner"),false,rocket)
  energy-=5;fired=true
 if fired:fire_timer=.3;play_sound("pulse")
 else:fire_timer=.5;message("Install a pulse cannon or rocket launcher in the shipyard to fire.")

func spawn_shot(pos: Vector3,velocity: Vector3,damage: float,hostile: bool,rocket: bool = false) -> void:
 var n = Node3D.new();world.add_child(n);n.position=pos
 if rocket:
  var c = art.cylinder(n,.16,.9,Vector3.ZERO,art.trim,.06);c.rotation.x=PI/2
 else:
  var s=SphereMesh.new();s.radius=.15;s.height=.3
  art.mesh(n,s,Vector3.ZERO,art.material(Color("ff775e") if hostile else Color("7be4ff"),0,Color("ff775e") if hostile else Color("7be4ff")))
 shots.append({"node":n,"velocity":velocity,"damage":damage,"hostile":hostile,"time":4.0})

func update_shots(dt: float) -> void:
 for i in range(shots.size()-1,-1,-1):
  var s: Dictionary=shots[i];var start: Vector3 = s.node.position;var end: Vector3=start+s.velocity*dt
  var hit: Dictionary = ray_hit(start,end,3 if s.hostile else 6)
  s.node.position=end;s.time-=dt
  if not hit.is_empty():
   if hit.collider==ship:damage_ship(s.damage,hit.position)
   else:damage_target(hit.collider,s.damage,hit.position)
   effect(hit.position,Color("ffb770"),4)
  if not hit.is_empty() or s.time<=0:s.node.queue_free();shots.remove_at(i)

func update_enemies(dt: float) -> void:
 for enemy in enemies.duplicate():
  if not is_instance_valid(enemy):continue
  var delta: Vector3 = ship.position-enemy.position;var distance: float=delta.length()
  if distance>180:continue
  enemy.apply_central_force(delta.normalized()*(130+sector*35) if distance>25 else Vector3(-delta.z,0,delta.x).normalized()*130)
  enemy.rotation.y=atan2(-delta.x,-delta.z)
  var cooldown: float=float(enemy.get_meta("cooldown"))-dt
  if cooldown<=0 and distance<100:
   var direction: Vector3=(ship.position+ship.linear_velocity*.3-enemy.position).normalized()
   spawn_shot(enemy.position+direction*3+Vector3(0,.6,0),direction*(35+sector*4),5+sector*3,true)
   cooldown=2.8-sector*.25
  enemy.set_meta("cooldown",cooldown)
 enemy_timer+=dt
 if enemy_timer>35 and enemies.size()<8:enemy_timer=0;spawn_enemy(ship.position+Vector3(70,0,-70))

func damage_target(body: Node,damage: float,point: Vector3) -> void:
 if not is_instance_valid(body) or not body.has_meta("hp"):return
 var hp: float=float(body.get_meta("hp"))-damage;body.set_meta("hp",hp)
 if hp>0:return
 var pos: Vector3=body.global_position
 if body.get_meta("type")=="rock":
  rocks.erase(body);state.event("mine",1)
  var count: int = 5+sector
  if rng.randf()<.04*state.bonus("Scientist"):count+=5
  for i in range(count):spawn_chunk(pos+Vector3(rng.randf_range(-1,1),0,rng.randf_range(-1,1)))
 else:
  enemies.erase(body);state.event("kill",1);state.credits+=50+sector*25
  for i in range(3):spawn_chunk(pos+Vector3(i-1,0,0))
 effect(point,Color("ffae65"),16);play_sound("explosion")
 body.remove_meta("hp");body.collision_layer=0;body.queue_free()

func damage_ship(amount: float,point: Vector3) -> void:
 if shield>0:
  var absorbed: float=minf(shield,amount);shield-=absorbed;amount-=absorbed
  effect(point,Color("65bfff"),4)
 if amount<=0:return
 hull-=amount*.35
 var nearest: int = 0;var distance: float = INF
 for i in range(module_nodes.size()):
  var d: float=module_nodes[i].global_position.distance_to(point)
  if d<distance and float(state.modules[i].hp)>0:nearest=i;distance=d
 state.modules[nearest].hp=maxf(0,float(state.modules[nearest].hp)-amount)
 var n: Node3D=module_nodes[nearest]
 if float(state.modules[nearest].hp)<=0:n.scale=Vector3.ONE*.82;effect(n.global_position,Color("ff845a"),8)

func spawn_chunk(pos: Vector3) -> void:
 if chunks.size()>200:return
 var b=RigidBody3D.new();b.gravity_scale=0;b.mass=.5;b.linear_damp=.5;b.collision_layer=8;b.collision_mask=2;world.add_child(b);b.position=pos
 art.mesh(b,art.asteroid(rng.randi(),.3),Vector3.ZERO,art.trim)
 var cs=CollisionShape3D.new();var sh=SphereShape3D.new();sh.radius=.3;cs.shape=sh;b.add_child(cs)
 b.linear_velocity=Vector3(rng.randf_range(-3,3),0,rng.randf_range(-3,3));chunks.append(b)

func update_chunks(dt: float) -> void:
 var magnet: float=0
 for m in state.modules:
  if m.kind=="Magnet" and float(m.hp)>0 and energy>0:magnet=maxf(magnet,18*float(m.level)*state.bonus("Collector"))
 if magnet>0:energy=maxf(0,energy-6*dt)
 for chunk in chunks.duplicate():
  var delta: Vector3=ship.position-chunk.position;var dist: float=delta.length()
  if dist<magnet and dist>1:chunk.apply_central_force(delta.normalized()*minf(18,100/maxf(dist,1)))
  if dist<3 and cargo<int(state.performance(cargo).cargo):
   var amount: int=maxi(1,roundi(state.bonus("Miner")))
   amount=mini(amount,int(state.performance(cargo).cargo)-cargo)
   cargo+=amount;state.event("ore",amount);chunks.erase(chunk);chunk.queue_free();play_sound("collect")

func effect(pos: Vector3,color: Color,count: int) -> void:
 if state.reduced_motion:count=mini(count,2)
 var particles=GPUParticles3D.new();world.add_child(particles);particles.position=pos
 var mat=ParticleProcessMaterial.new();mat.direction=Vector3.UP;mat.spread=180;mat.initial_velocity_min=2;mat.initial_velocity_max=8;mat.gravity=Vector3.ZERO;mat.scale_min=.04;mat.scale_max=.2;mat.color=color
 particles.process_material=mat;particles.amount=maxi(count,1);particles.lifetime=.6;particles.one_shot=true;particles.explosiveness=1
 var mesh=SphereMesh.new();mesh.radius=.15;mesh.height=.3;mesh.material=art.material(color,0,color)
 particles.draw_pass_1=mesh;particles.emitting=true;transient.append({"node":particles,"time":.8})

func message(text: String) -> void:
 toast_text=text;toast_time=6

func style(bg: Color,border: Color = Color("294552"),radius: int = 8) -> StyleBoxFlat:
 var s=StyleBoxFlat.new();s.bg_color=bg;s.border_color=border;s.set_border_width_all(1);s.set_corner_radius_all(radius);s.content_margin_left=16;s.content_margin_right=16;s.content_margin_top=12;s.content_margin_bottom=12
 return s

func setup_ui() -> void:
 hud=CanvasLayer.new();add_child(hud)
 ui=Control.new();ui.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT);ui.mouse_filter=Control.MOUSE_FILTER_IGNORE;hud.add_child(ui)
 var theme=Theme.new();theme.default_font_size=15
 theme.set_color("font_color","Label",Color("dce9e9"))
 theme.set_color("font_color","Button",Color("e1eeee"))
 theme.set_color("font_hover_color","Button",Color("071b22"))
 theme.set_stylebox("normal","Button",style(Color("122936")))
 theme.set_stylebox("hover","Button",style(Color("6adbc9"),Color("a0fff1")))
 theme.set_stylebox("pressed","Button",style(Color("338b89")))
 theme.set_stylebox("disabled","Button",style(Color("101a25"),Color("192c38")))
 theme.set_color("font_disabled_color","Button",Color("536a77"))
 theme.set_stylebox("panel","PanelContainer",style(Color(.025,.055,.085,.96)))
 ui.theme=theme
 var header=PanelContainer.new();ui.add_child(header);header.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE);header.offset_left=20;header.offset_top=18;header.offset_right=-20;header.offset_bottom=90
 var hb=HBoxContainer.new();header.add_child(hb)
 var title=label("SPACE MINER",25,Color("e4f4ef"));title.size_flags_horizontal=Control.SIZE_EXPAND_FILL;hb.add_child(title)
 var subtitle=label("BUILD  /  CONQUER     •     HELIOS INDUSTRIES",12,Color("79a4af"));subtitle.size_flags_horizontal=Control.SIZE_EXPAND_FILL;hb.add_child(subtitle)
 top_label=label("",16,Color("e9c98f"));hb.add_child(top_label)
 panel=PanelContainer.new();ui.add_child(panel);panel.set_anchors_and_offsets_preset(Control.PRESET_LEFT_WIDE);panel.offset_left=20;panel.offset_top=106;panel.offset_right=390;panel.offset_bottom=-85
 var scroll=ScrollContainer.new();scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED;panel.add_child(scroll)
 panel_content=VBoxContainer.new();panel_content.size_flags_horizontal=Control.SIZE_EXPAND_FILL;panel_content.add_theme_constant_override("separation",10);scroll.add_child(panel_content)
 var bottom=PanelContainer.new();ui.add_child(bottom);bottom.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE);bottom.offset_left=20;bottom.offset_right=-20;bottom.offset_top=-70;bottom.offset_bottom=-18
 status_label=label("",13,Color("8cb0bb"));bottom.add_child(status_label)
 hud_label=label("",17,Color("d8f1ee"));ui.add_child(hud_label);hud_label.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT);hud_label.offset_left=-340;hud_label.offset_right=-30;hud_label.offset_top=116;hud_label.offset_bottom=300;hud_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_RIGHT;hud_label.mouse_filter=Control.MOUSE_FILTER_IGNORE
 toast_label=label("",15,Color("f6d9a5"));ui.add_child(toast_label);toast_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_BOTTOM);toast_label.offset_left=-430;toast_label.offset_right=430;toast_label.offset_top=-145;toast_label.offset_bottom=-82;toast_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER;toast_label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART;toast_label.mouse_filter=Control.MOUSE_FILTER_IGNORE

func label(text: String,size: int=15,color: Color=Color("dce9e9")) -> Label:
 var l=Label.new();l.text=text;l.add_theme_font_size_override("font_size",size);l.add_theme_color_override("font_color",color);l.mouse_filter=Control.MOUSE_FILTER_IGNORE;return l

func paragraph(text: String) -> void:
 var l=label(text,13,Color("91aeb9"));l.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART;panel_content.add_child(l)

func button(text: String,callback: Callable,parent: Node=null) -> Button:
 var b=Button.new();b.text=text;b.custom_minimum_size.y=40;b.pressed.connect(callback)
 if parent==null:panel_content.add_child(b)
 else:parent.add_child(b)
 b.mouse_entered.connect(func():
  if not state.reduced_motion:b.create_tween().tween_property(b,"modulate",Color(1.15,1.15,1.15),.12)
 )
 b.mouse_exited.connect(func():b.modulate=Color.WHITE)
 return b

func heading(text: String,small: String="") -> void:
 panel_content.add_child(label(text,23,Color("b8f2e6")))
 if small!="":paragraph(small)
 panel_content.add_child(HSeparator.new())

func navigate(to: String) -> void:
 page=to;refresh_panel();play_sound("click")

func refresh_panel() -> void:
 for n in panel_content.get_children():panel_content.remove_child(n);n.queue_free()
 if mode=="space":
  panel.visible=paused
  if paused:
   heading("FLIGHT PAUSED","Physics and combat are suspended.")
   button("RESUME FLIGHT",func():paused=false;refresh_panel())
   button("SAVE PROGRESS",func():state.save_game();message("Saved persistent progress. Cargo must be returned to dock."))
   button("ABANDON CARGO / RETURN",func():cargo=0;enter_hangar())
   button("QUIT TO DESKTOP",func():state.save_game();get_tree().quit())
  return
 panel.visible=true
 if page!="HOME":button("‹  HELIOS OPERATIONS",func():navigate("HOME"))
 match page:
  "HOME":
   heading("DOCK 07 / HELIOS","Independent extraction. Infinite possibility.")
   for dest in ["BUILD","CREW","PACKS","UPGRADES","CONTRACTS","LAUNCH","SETTINGS"]:
    button({"BUILD":"01   SHIPYARD & EQUIPMENT","CREW":"02   CREW MANIFEST","PACKS":"03   RELIC CARD EXCHANGE","UPGRADES":"04   ACTIVE RESEARCH","CONTRACTS":"05   CONTRACT ALBUM","LAUNCH":"06   EXPEDITION CHART","SETTINGS":"07   FLIGHT SETTINGS"}[dest],func():navigate(dest))
   paragraph("EARLY ACCESS 0.1.0\nNative 3D • Single-player • Offline saves\n\nStarting allowance: 1,200 credits and 9 crystals. Build your own ship before exploring further.")
  "BUILD":build_editor_ui()
  "CREW":build_crew_ui()
  "PACKS":
   heading("RELIC EXCHANGE","Original personnel cards / 40 to collect")
   paragraph("STANDARD PACK\nExactly 3 cards • 3 crystals\nDuplicates convert to 50 credits each.\n\nCard rarity odds per card:\nCommon 40% / Uncommon 30% / Rare 18%\nEpic 8% / Legendary 3% / Mythic 0.8%\nSecret 0.2%\n\nSeparate permanent fifth-slot reward: 1% per purchased pack until unlocked. No duplicate slot awards.")
   button("PURCHASE PACK • 3 CRYSTALS",confirm_pack).disabled=state.crystals<3
   paragraph("Earn crystals by claiming completed contracts. Press Escape or SKIP during a reveal to finish the animation; rewards are saved before it begins.")
  "UPGRADES":
   heading("ACTIVE RESEARCH","These bonuses are separate from physical modules.")
   paragraph("%d / %d active slots. Each active discipline grants +20%% to its matching crew bonus. Select up to your slot limit." % [state.active.size(),state.upgrade_slots()])
   for role in Catalog.ROLES:
    button(("[ACTIVE]  " if state.active.has(role) else "+  ")+role,func():
     if not state.toggle_active(role):message("All active upgrade slots are occupied.")
     state.save_game();refresh_panel()
    )
   button("FOURTH SLOT • 10,000 CREDITS" if not state.fourth else "FOURTH SLOT UNLOCKED",func():
    if state.buy_fourth():state.save_game();refresh_panel()
    else:message("Requires 10,000 credits. Permanent unlocks cannot be purchased twice.")
   ).disabled=state.fourth
   paragraph("Fifth slot: "+("UNLOCKED" if state.fifth else "1% permanent reward per card pack. Maximum five slots."))
  "CONTRACTS":build_contract_ui()
  "LAUNCH":
   heading("EXPEDITION CHART","Docked ore is sold automatically. Explore at your own risk.")
   for i in range(5):
    var s: Dictionary=Catalog.SECTORS[i]
    var b=button("%02d  %s\n%s" % [i+1,s.name,"LAUNCH EXPEDITION" if state.sectors.has(i) else "CHART & LAUNCH • %d CREDITS" % s.price],func():launch(i))
    b.add_theme_font_size_override("font_size",13)
    paragraph("%s • %s • %d credits / ore" % [s.sub,s.ore,s.value])
   paragraph("Preflight: add a magnet for ore collection and a cannon for combat. The basic mining laser also damages enemies. Move through ore to collect without a magnet.")
  "SETTINGS":
   heading("FLIGHT SETTINGS","Progress is stored locally in Godot's user data directory.")
   button("REDUCED MOTION: "+("ON" if state.reduced_motion else "OFF"),func():state.reduced_motion=not state.reduced_motion;state.save_game();refresh_panel())
   paragraph("Orbit sensitivity")
   var slider=HSlider.new();slider.min_value=.2;slider.max_value=3;slider.step=.1;slider.value=state.sensitivity;panel_content.add_child(slider);slider.value_changed.connect(func(v):state.sensitivity=v)
   button("SAVE PROGRESS",func():state.save_game();message("Progress saved."))
   button("QUIT TO DESKTOP",func():state.save_game();get_tree().quit())
   paragraph("W / S • thrust / reverse\nA / D • yaw control\nQ • inertial brake\nShift • afterburner\nLeft mouse • mining beam\nRight mouse / Space • weapons\nE • dock near cyan beacon\nTab • cargo readout\nMiddle drag • orbit camera\nShift + middle drag • pan\nWheel • zoom\nR • rotate editor placement\nU • upgrade module under cursor\nF5 • save / Escape • pause\n\nLICENSES\nOriginal project assets: MIT.\nGodot Engine: MIT (see LICENSES.md).")

func build_editor_ui() -> void:
 heading("SHIPYARD / ASSEMBLY","Connected modular construction • 11 × 11 grid")
 var p: Dictionary=state.performance()
 paragraph("MASS %.1f t    THRUST %.0f kN\nLOAD %.0f E/s    CARGO %d\nASSEMBLY %d / %d cells\nPurchased cells %d • CREW %d slots\nNext crew slot: %d / 10 cells" % [p.mass,p.thrust,p.power,p.cargo,state.modules.size(),4+state.cells,state.cells,state.crew_slots(),state.cells%10])
 button("BUY EXPANSION CELL • %d CR" % state.cell_price(),func():
  if state.buy_cell():state.save_game();refresh_panel()
  else:message("Insufficient credits.")
 )
 paragraph("Left click empty grid: install. Right click module: remove for 50% refund. R rotates. U upgrades a module (max level 5). Cyan marker shows center of mass. Starter cells never count toward crew capacity.")
 for kind in Catalog.MODULES:
  var d: Dictionary=Catalog.MODULES[kind]
  var b=button(("›  " if selected==kind else "")+kind.to_upper()+"  /  %d CR" % d.price,func():selected=kind;update_ghost();refresh_panel())
  b.add_theme_font_size_override("font_size",13)
  if selected==kind:paragraph(d.desc+"\n%.0f t • %d E/s • rotation %d°" % [d.mass,d.power,rotation_step*90])
 button("PREPARE EXPEDITION  ›",func():navigate("LAUNCH"))

var crew_search: String = ""
var crew_filter: int = 0
var crew_sort: int = 0
func build_crew_ui() -> void:
 heading("CREW MANIFEST","%d / 40 collected • %d / %d equipped" % [state.collection.size(),state.equipped.size(),state.crew_slots()])
 paragraph("Every 10 purchased expansion cells unlocks exactly one crew slot. Unique equipped cards apply their bonuses once. Click a portrait to equip / unequip.")
 var search=LineEdit.new();search.placeholder_text="Search name or role";search.text=crew_search;panel_content.add_child(search)
 var filter=OptionButton.new();filter.add_item("All rarities")
 for r in Catalog.RARITIES:filter.add_item(r)
 filter.selected=crew_filter;panel_content.add_child(filter)
 var sorting=OptionButton.new();sorting.add_item("Collection number");sorting.add_item("Rarity: highest first");sorting.selected=crew_sort;panel_content.add_child(sorting)
 var grid=GridContainer.new();grid.columns=2;grid.add_theme_constant_override("h_separation",8);grid.add_theme_constant_override("v_separation",8);panel_content.add_child(grid)
 var render=func():
  for n in grid.get_children():grid.remove_child(n);n.queue_free()
  var ids: Array=range(40)
  if crew_sort==1:ids.reverse()
  for id in ids:
   var c: Dictionary=Catalog.crew(id)
   if crew_filter>0 and c.rarity!=crew_filter-1:continue
   if crew_search!="" and not (c.name+" "+c.role).to_lower().contains(crew_search.to_lower()):continue
   var card=make_card(id,Vector2(151,256),state.collection.has(id))
   grid.add_child(card)
   card.gui_input.connect(func(event):
    if event is InputEventMouseButton and event.button_index==MOUSE_BUTTON_LEFT and event.pressed:
     if not state.equip(id):message("Card unavailable or crew capacity full. Buy 10 expansion cells for each slot.")
     state.save_game();refresh_panel()
   )
 render.call()
 search.text_changed.connect(func(text):crew_search=text;render.call())
 filter.item_selected.connect(func(index):crew_filter=index;render.call())
 sorting.item_selected.connect(func(index):crew_sort=index;render.call())

func make_card(id: int,size: Vector2,owned: bool=true) -> PanelContainer:
 var c: Dictionary=Catalog.crew(id)
 var card=PanelContainer.new();card.custom_minimum_size=size;card.pivot_offset=size/2
 var col: Color=Catalog.COLORS[c.rarity]
 card.add_theme_stylebox_override("panel",style(Color("0a1623"),col,10))
 var v=VBoxContainer.new();v.mouse_filter=Control.MOUSE_FILTER_IGNORE;card.add_child(v)
 var number=label(c.number+"   "+("ACTIVE" if state.equipped.has(id) else Catalog.RARITIES[c.rarity].to_upper()),10,col);v.add_child(number)
 var tex=TextureRect.new();tex.texture=load("res://assets/crew_%02d.png" % id);tex.custom_minimum_size=Vector2(size.x-34,size.y*.51);tex.expand_mode=TextureRect.EXPAND_IGNORE_SIZE;tex.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_COVERED;tex.mouse_filter=Control.MOUSE_FILTER_IGNORE
 if not owned:tex.modulate=Color(.18,.23,.27)
 v.add_child(tex)
 v.add_child(label(c.name if owned else "UNRECRUITED",12,col))
 v.add_child(label(c.role.to_upper(),11,Color("8daebb")))
 var bonus=label("+%d%% %s" % [roundi(c.bonus*100),c.role],11);bonus.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART;v.add_child(bonus)
 return card

func confirm_pack() -> void:
 var dialog=ConfirmationDialog.new();dialog.title="STANDARD PERSONNEL PACK";dialog.dialog_text="Spend 3 crystals for exactly 3 cards?\nDuplicates convert to 50 credits. Fifth-slot reward: 1% per pack.";dialog.ok_button_text="PURCHASE";ui.add_child(dialog)
 dialog.confirmed.connect(func():
  var rewards: Array=state.open_pack()
  if rewards.size()==3:
   state.save_game();begin_reveal(rewards)
  dialog.queue_free()
 )
 dialog.canceled.connect(func():dialog.queue_free());dialog.popup_centered()

func begin_reveal(rewards: Array) -> void:
 reveal_rewards=rewards;reveal_cards.clear();reveal_time=0;reveal_stage=0
 reveal_layer=Control.new();ui.add_child(reveal_layer);reveal_layer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT);reveal_layer.mouse_filter=Control.MOUSE_FILTER_STOP
 var bg=ColorRect.new();bg.color=Color(.012,.022,.05,.98);reveal_layer.add_child(bg);bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
 var title=label("HELIOS  /  PERSONNEL ARCHIVE",28,Color("b8efe3"));reveal_layer.add_child(title);title.position=Vector2(80,55)
 var sub=label("RELIC SIGNAL DECODED  •  THREE TRANSMISSIONS ACQUIRED",13,Color("9ab5c2"));reveal_layer.add_child(sub);sub.position=Vector2(82,100)
 for i in range(3):
  var c=make_card(rewards[i],Vector2(250,440));reveal_layer.add_child(c)
  c.position=Vector2(get_viewport().get_visible_rect().size.x/2-125,get_viewport().get_visible_rect().size.y/2-220)
  c.modulate.a=0;c.scale=Vector2(.05,1);reveal_cards.append(c)
 var skip=button("SKIP / CONTINUE",finish_reveal,reveal_layer);skip.position=Vector2(80,get_viewport().get_visible_rect().size.y-90);skip.custom_minimum_size.x=250
 if state.reduced_motion:reveal_time=8

func animate_reveal(dt: float) -> void:
 reveal_time+=dt
 var viewport_size: Vector2=get_viewport().get_visible_rect().size
 for i in range(3):
  var t: float=clampf((reveal_time-.5-i*.75)/1.2,0,1)
  var ease: float=1-pow(1-t,3)
  var card: Control=reveal_cards[i]
  card.size=Vector2(250,440)
  card.modulate.a=minf(1,t*3)
  card.scale=Vector2(maxf(.02,abs(cos((1-t)*PI/2))),1)*lerpf(.8,1,ease)
  card.position=Vector2(viewport_size.x/2-125+(i-1)*290*ease,viewport_size.y/2-210-sin(t*PI)*35)
  card.rotation=sin((1-t)*PI)*float(i-1)*.15
  if reveal_time>4 and not state.reduced_motion:card.position.y+=sin(reveal_time*1.7+i)*4
 if reveal_time>4 and reveal_stage==0:
  reveal_stage=1;play_sound("collect")
  if state.notice!="":message(state.notice);state.notice=""

func finish_reveal() -> void:
 if is_instance_valid(reveal_layer):reveal_layer.queue_free();reveal_layer=null
 reveal_cards.clear();state.save_game();refresh_panel()

func build_contract_ui() -> void:
 state.refresh_tasks(int(Time.get_unix_time_from_system()))
 heading("CONTRACT ALBUM","30 UTC-timed contracts • claim crystals to buy card packs")
 paragraph("5 minute / 3 hourly / 7 daily / 15 weekly. Timers use UTC epoch-aligned periods; offline elapsed periods reset on load. Minute goals avoid combat requirements.")
 var specs: Array=state.task_specs()
 for i in range(30):
  var s: Dictionary=specs[i];var t: Dictionary=state.tasks[i]
  var card=PanelContainer.new();panel_content.add_child(card)
  var vb=VBoxContainer.new();card.add_child(vb)
  vb.add_child(label(s.group+" / "+s.title,14,Catalog.COLORS[mini(i/6,6)]))
  var progress=label("",12,Color("95b5c2"));progress.name="Contract%d" % i;vb.add_child(progress)
  var bar=ProgressBar.new();bar.max_value=s.goal;bar.value=t.progress;bar.custom_minimum_size.y=8;bar.show_percentage=false;vb.add_child(bar)
  var b=button("CLAIMED" if t.claimed else "CLAIM %d CRYSTALS" % s.reward,func():
   if state.claim(i):state.save_game();refresh_panel();play_sound("collect")
   else:message("Objective not yet complete, expired, or reward already claimed.")
  ,vb);b.disabled=t.claimed or int(t.progress)<int(s.goal)
 update_contract_labels()

func update_contract_labels() -> void:
 var now: int=int(Time.get_unix_time_from_system())
 state.refresh_tasks(now)
 var specs: Array=state.task_specs()
 for i in range(30):
  var l=panel_content.find_child("Contract%d" % i,true,false)
  if l:
   var s: Dictionary=specs[i];var t: Dictionary=state.tasks[i]
   l.text="%s  %d / %d • resets in %ds" % [s.metric.to_upper(),t.progress,s.goal,int(s.period)-now%int(s.period)]
   var vb=l.get_parent();var bar=vb.get_child(2);bar.value=t.progress
   var b=vb.get_child(3);b.disabled=t.claimed or int(t.progress)<int(s.goal);b.text="CLAIMED" if t.claimed else "CLAIM %d CRYSTALS" % s.reward

func update_labels() -> void:
 top_label.text="%s CR    /    %d CRYSTALS" % [str(state.credits),state.crystals]
 if mode=="hangar":
  status_label.text="DOCK 07   /   LMB install   RMB remove   R rotate   U upgrade   MMB orbit   Shift+MMB pan   Wheel zoom"
  hud_label.text="HELIOS SHIPYARD\n\nASSEMBLY BAY 07\n%s\nROTATION %d°\nGRID %d, %d\n\nNATIVE 3D / EARLY ACCESS 0.1.0" % [selected.to_upper(),rotation_step*90,cursor_cell.x,cursor_cell.y]
 else:
  status_label.text="FLIGHT   /   W S thrust   A D yaw   Q brake   Shift boost   LMB mine   Space fire   E dock   Esc pause"
  hud_label.text="%s\n\nHULL %03d  /  SHIELD %03d\nENERGY %03d / 100\nCARGO %d / %d\nSPEED %.1f m/s\nDOCK %.0f m\nHOSTILES %d" % [Catalog.SECTORS[sector].name,maxi(0,int(hull)),int(shield),int(energy),cargo,state.performance(cargo).cargo,ship.linear_velocity.length(),ship.position.length(),enemies.size()]
  for m in state.modules:
   if m.kind=="Scanner" and float(m.hp)>0:
    var hit: Dictionary=ray_hit(camera.project_ray_origin(get_viewport().get_mouse_position()),camera.project_ray_origin(get_viewport().get_mouse_position())+camera.project_ray_normal(get_viewport().get_mouse_position())*600,6)
    if hit.has("collider") and hit.collider.has_meta("hp"):
     hud_label.text+="\n\nSCANNER\n%s / %.0f integrity" % [Catalog.SECTORS[sector].ore if hit.collider.get_meta("type")=="rock" else "HOSTILE",hit.collider.get_meta("hp")]
    break

func runtime_smoke() -> void:
 # Real scene-level integration, invoked only with -- --smoke by CI.
 await get_tree().process_frame
 assert(is_instance_valid(ship))
 assert(module_nodes.size()==state.modules.size())
 for dest in ["HOME","BUILD","CREW","PACKS","UPGRADES","CONTRACTS","LAUNCH","SETTINGS"]:
  navigate(dest)
  await get_tree().process_frame
 launch(0)
 await get_tree().physics_frame
 assert(rocks.size()==60 and enemies.size()==2)
 var rock: Node=rocks[0]
 set_process(false)
 aim=rock.global_position
 mine(2.0)
 assert(rocks.size()==59)
 assert(chunks.size()==5)
 set_process(true)
 assert(int(state.stats.mine)>0)
 var enemy: Node=enemies[0]
 await get_tree().physics_frame
 spawn_shot(enemy.global_position+Vector3(0,0,5),Vector3(0,0,-50),1000,false)
 update_shots(.2)
 assert(enemies.size()==1)
 spawn_chunk(ship.position+Vector3(1,0,0))
 update_chunks(.01)
 assert(cargo>0)
 var key=InputEventKey.new();key.keycode=KEY_W;key.pressed=true
 Input.parse_input_event(key)
 for frame in range(30):await get_tree().physics_frame
 key.pressed=false;Input.parse_input_event(key)
 assert(ship.linear_velocity.length()>0.1)
 cargo=3
 var before: int=state.credits
 enter_hangar()
 assert(state.credits==before+48)
 assert(state.save_game("user://smoke-save.json"))
 var other=State.new();assert(other.load_game("user://smoke-save.json"))
 begin_reveal([0,24,39])
 await get_tree().process_frame
 animate_reveal(6)
 finish_reveal()
 print("SCENE SMOKE PASS: hangar, 8 menus, thrust physics, mining ray, projectile combat, ore collection, cargo sale, save/load, card reveal")
 get_tree().quit(0)

func capture_screens() -> void:
 # Actual rendered viewports, not generated mockups.
 DirAccess.make_dir_recursive_absolute("res://docs/visual")
 state=State.new()
 enter_hangar()
 await get_tree().create_timer(3).timeout
 await RenderingServer.frame_post_draw
 get_viewport().get_texture().get_image().save_png("res://docs/visual/01-hangar.png")
 navigate("CREW")
 await get_tree().create_timer(1).timeout
 await RenderingServer.frame_post_draw
 get_viewport().get_texture().get_image().save_png("res://docs/visual/02-crew.png")
 launch(1)
 await get_tree().create_timer(2).timeout
 paused=true
 panel.visible=false
 await RenderingServer.frame_post_draw
 get_viewport().get_texture().get_image().save_png("res://docs/visual/03-expedition.png")
 begin_reveal([0,24,39])
 reveal_time=5
 await get_tree().create_timer(1).timeout
 await RenderingServer.frame_post_draw
 get_viewport().get_texture().get_image().save_png("res://docs/visual/04-cards.png")
 print("VISUAL CAPTURE COMPLETE")
 get_tree().quit()
